#include <linux/cache.h>
#include <linux/compiler.h>
#include <linux/cred.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/ptrace.h>
#include <linux/security.h>
#include <linux/string.h>
#include <linux/types.h>

#include "arch.h"
#include <security.h>
#include "avc.h"
#include "feature/avc_silent.h"
#include "klog.h" // IWYU pragma: keep
#include "policy/feature.h"
#include "selinux/selinux.h"
#include "uapi/feature.h"

/*
 * 只在审计出口丢掉这些拒绝记录。
 *
 * avc_has_perm() 已经完整算完权限并得出结果，avc_audit() 只是拿本次决策的
 * 副本 avd 去格式化审计记录。清掉 avd->auditdeny 后 avc_audit_required()
 * 认为这次拒绝无需审计，记录不会写出去；权限结果本身不变，也没有改
 * SELinux 策略、avtab、审计开关，更没有 hook avc_has_perm。
 *
 * 过滤条件：
 *   - scontext ∈ {vendor_hal_perf_default, system_server, app_zygote}
 *     且 tcontext 的 SID 等于 ksu 域或 ksu 对象上下文（前者对应进程检查，
 *     后者对应 /data/adb/ksu 这类 ksu:dir/file 检查）；
 *   - 或 scontext 就是 ksu 域自身（ksu_cred 下内核特权操作产生的拒绝记录），
 *     此时不再看 tcontext。凭证没有任何改动。
 */

#define AVC_SILENT_SOURCE_COUNT 3

static const char *const avc_silent_sources[AVC_SILENT_SOURCE_COUNT] = {
    "u:r:vendor_hal_perf_default:s0",
    "u:r:system_server:s0",
    "u:r:app_zygote:s0",
};

/* [0] = u:r:ksu:s0，[1] = u:object_r:ksu:s0 */
static u32 avc_silent_ksu_sids[2];
static u32 avc_silent_source_sids[AVC_SILENT_SOURCE_COUNT];

#if defined(MODULE) && !defined(CONFIG_KSU_DEBUG)
static bool ksu_avc_silent_enabled __read_mostly = true;
#else
static bool ksu_avc_silent_enabled __read_mostly;
#endif
static bool ksu_avc_silent_hooked __read_mostly;
static DEFINE_MUTEX(avc_silent_mutex);

static bool avc_silent_is_source(u32 sid)
{
    int i;

    if (!sid)
        return false;

    for (i = 0; i < AVC_SILENT_SOURCE_COUNT; i++) {
        if (sid == READ_ONCE(avc_silent_source_sids[i]))
            return true;
    }

    return false;
}

static bool avc_silent_is_target(u32 sid)
{
    if (!sid)
        return false;

    return sid == READ_ONCE(avc_silent_ksu_sids[0]) || sid == READ_ONCE(avc_silent_ksu_sids[1]);
}

/*
 * avc_audit(state, ssid, tsid, tclass, requested, avd, result, ad)
 * 第 2/3/6 个参数在所有支持的架构上都在通用参数寄存器里，pre_handler
 * 拿到的是被中断时的 pt_regs，因此可以直接取值。
 */
static int avc_silent_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
    struct av_decision *avd;
    bool ksu_source;
    u32 ssid, tsid;

    (void)p;

    if (likely(!READ_ONCE(ksu_avc_silent_enabled)))
        return 0;

    ssid = (u32)PT_REGS_PARM2(regs);
    tsid = (u32)PT_REGS_PARM3(regs);

    /* ksu 域自己作为 scontext 的拒绝记录一律丢弃，不再限制 tcontext */
    ksu_source = ssid && ssid == READ_ONCE(avc_silent_ksu_sids[0]);
    if (!ksu_source && (!avc_silent_is_source(ssid) || !avc_silent_is_target(tsid)))
        return 0;

    avd = (struct av_decision *)PT_REGS_PARM6(regs);
    if (!avd)
        return 0;

    /* 只影响拒绝记录；allowed/auditallow/seqno 等权限结果保持原样 */
    avd->auditdeny = 0;

    return 0;
}

static struct kprobe avc_silent_kprobe = {
    .symbol_name = "avc_audit",
    .pre_handler = avc_silent_pre_handler,
};

static int avc_silent_resolve(const char *ctx, u32 *sid)
{
    return security_secctx_to_secid(ctx, strlen(ctx), sid);
}

/*
 * 必须在 apply_kernelsu_rules() 之后调用：ksu 类型由那一步创建，之前解析
 * u:r:ksu:s0 会失败。单个来源域不存在时只跳过该域，不影响其它过滤项。
 */
static int avc_silent_resolve_sids(void)
{
    static const char ksu_object[] = "u:object_r:" KERNEL_SU_DOMAIN ":s0";
    int i;

    avc_silent_ksu_sids[0] = 0;
    avc_silent_ksu_sids[1] = 0;
    for (i = 0; i < AVC_SILENT_SOURCE_COUNT; i++)
        avc_silent_source_sids[i] = 0;

    if (avc_silent_resolve(KERNEL_SU_CONTEXT, &avc_silent_ksu_sids[0])) {
        pr_warn("avc_silent: cannot resolve %s\n", KERNEL_SU_CONTEXT);
        return -EOPNOTSUPP;
    }
    if (avc_silent_resolve(ksu_object, &avc_silent_ksu_sids[1]))
        avc_silent_ksu_sids[1] = 0;

    for (i = 0; i < AVC_SILENT_SOURCE_COUNT; i++) {
        u32 sid = 0;

        if (!avc_silent_resolve(avc_silent_sources[i], &sid))
            avc_silent_source_sids[i] = sid;
        else
            pr_warn("avc_silent: source %s unavailable\n", avc_silent_sources[i]);
    }

    pr_info("avc_silent: ksu sid %u/%u, sources %u/%u/%u\n", avc_silent_ksu_sids[0], avc_silent_ksu_sids[1],
            avc_silent_source_sids[0], avc_silent_source_sids[1], avc_silent_source_sids[2]);
    return 0;
}

static int avc_silent_install(void)
{
    int ret;

    mutex_lock(&avc_silent_mutex);
    if (READ_ONCE(ksu_avc_silent_hooked)) {
        mutex_unlock(&avc_silent_mutex);
        return 0;
    }

    ret = avc_silent_resolve_sids();
    if (ret) {
        mutex_unlock(&avc_silent_mutex);
        return ret;
    }

    ret = register_kprobe(&avc_silent_kprobe);
    if (ret) {
        pr_err("avc_silent: register kprobe on avc_audit failed: %d\n", ret);
        mutex_unlock(&avc_silent_mutex);
        return ret;
    }

    WRITE_ONCE(ksu_avc_silent_hooked, true);
    mutex_unlock(&avc_silent_mutex);

    pr_info("avc_silent: kprobe installed on avc_audit\n");
    return 0;
}

static void avc_silent_uninstall(void)
{
    mutex_lock(&avc_silent_mutex);
    if (READ_ONCE(ksu_avc_silent_hooked)) {
        unregister_kprobe(&avc_silent_kprobe);
        WRITE_ONCE(ksu_avc_silent_hooked, false);
    }
    mutex_unlock(&avc_silent_mutex);
}

static int avc_silent_feature_get(u64 *value)
{
    // 钩子没装上时如实返回 0，不把静默失败伪装成已启用
    if (!READ_ONCE(ksu_avc_silent_hooked)) {
        *value = 0;
        return 0;
    }

    *value = READ_ONCE(ksu_avc_silent_enabled) ? 1 : 0;
    return 0;
}

static int avc_silent_feature_set(u64 value)
{
    if (!value) {
        WRITE_ONCE(ksu_avc_silent_enabled, false);
        return 0;
    }

    if (avc_silent_install())
        return -EOPNOTSUPP;

    WRITE_ONCE(ksu_avc_silent_enabled, true);
    return 0;
}

static const struct ksu_feature_handler avc_silent_handler = {
    .feature_id = KSU_FEATURE_AVC_SILENT,
    .name = "avc_silent",
    .get_handler = avc_silent_feature_get,
    .set_handler = avc_silent_feature_set,
};

void __init ksu_avc_silent_init(void)
{
    if (ksu_register_feature_handler(&avc_silent_handler)) {
        pr_err("avc_silent: failed to register feature handler\n");
    }
}

/* 在 apply_kernelsu_rules() + cache_sid() 之后调用 */
void ksu_avc_silent_late_init(void)
{
    if (!READ_ONCE(ksu_avc_silent_enabled) || READ_ONCE(ksu_avc_silent_hooked))
        return;

    if (avc_silent_install())
        WRITE_ONCE(ksu_avc_silent_enabled, false);
}

void __exit ksu_avc_silent_exit(void)
{
    ksu_unregister_feature_handler(KSU_FEATURE_AVC_SILENT);
    WRITE_ONCE(ksu_avc_silent_enabled, false);
    avc_silent_uninstall();
}
