#include <linux/audit.h>
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
/* current_sid() 在 security/selinux/include/objsec.h（5.10 与 6.1 一致） */
#include "objsec.h"
#include "feature/avc_silent.h"
#include "infra/symbol_resolver.h"
#include "klog.h" // IWYU pragma: keep
#include "policy/feature.h"
#include "selinux/selinux.h"
#include "uapi/feature.h"

/*
 * 只在审计出口丢掉这些拒绝记录。
 *
 * 钩子挂在 audit_log_start(ctx, gfp_mask, type)：审计子系统里每条记录唯一
 * 的入口，第三个参数就是记录类型（AVC 记录是 AUDIT_AVC）。记录缓冲区在这一
 * 步才分配，调用方拿到 NULL 之后 audit_log_format()/audit_log_end() 都是空
 * 操作，因此把返回值改成 NULL 等价于这条记录从未产生。权限计算在更早的
 * 地方就已经完整执行完，这里没有改 SELinux 策略、avtab、审计开关，也没有
 * hook avc_has_perm，权限查表路径与未加载时完全一致。
 *
 * 来源判定不解析记录文本：写记录的这一刻 current 就是触发者，它的 sid
 * 就是这条记录的 scontext，直接和来源域列表比对。
 *
 * 过滤条件：
 *   - scontext ∈ {vendor_hal_perf_default, system_server, app_zygote}，
 *     只按来源域判定，不看目标；
 *   - 或 scontext 就是 ksu 域自身（ksu_cred 下内核特权操作产生的拒绝记录）。
 *     凭证没有任何改动。
 *
 * 三档，默认 0（关闭，连探针都不装，零开销）：
 *   0 = 关闭
 *   1 = 只判定并打日志说明是否命中，不改返回值（显式开启后的默认档）
 *   2 = 真正丢弃记录
 */

#define AVC_SILENT_SOURCE_COUNT 3

#define AVC_SILENT_MODE_OFF 0
#define AVC_SILENT_MODE_OBSERVE 1
#define AVC_SILENT_MODE_ENFORCE 2

static const char *const avc_silent_sources[AVC_SILENT_SOURCE_COUNT] = {
    "u:r:vendor_hal_perf_default:s0",
    "u:r:system_server:s0",
    "u:r:app_zygote:s0",
};

static u32 avc_silent_ksu_sid;
static u32 avc_silent_source_sids[AVC_SILENT_SOURCE_COUNT];

static u8 ksu_avc_silent_mode __read_mostly;
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
    int i;

    avc_silent_ksu_sid = 0;
    for (i = 0; i < AVC_SILENT_SOURCE_COUNT; i++)
        avc_silent_source_sids[i] = 0;

    if (avc_silent_resolve(KERNEL_SU_CONTEXT, &avc_silent_ksu_sid)) {
        pr_warn("avc_silent: cannot resolve %s\n", KERNEL_SU_CONTEXT);
        return -EOPNOTSUPP;
    }

    for (i = 0; i < AVC_SILENT_SOURCE_COUNT; i++) {
        u32 sid = 0;

        if (!avc_silent_resolve(avc_silent_sources[i], &sid))
            avc_silent_source_sids[i] = sid;
        else
            pr_warn("avc_silent: source %s unavailable\n", avc_silent_sources[i]);
    }

    pr_info("avc_silent: ksu sid %u, sources %u/%u/%u\n", avc_silent_ksu_sid, avc_silent_source_sids[0],
            avc_silent_source_sids[1], avc_silent_source_sids[2]);
    return 0;
}

#ifdef CONFIG_KRETPROBES
static int avc_silent_log_entry(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    u8 mode = READ_ONCE(ksu_avc_silent_mode);
    u32 sid;

    (void)ri;

    if (likely(mode == AVC_SILENT_MODE_OFF))
        return 1;

    if ((int)PT_REGS_PARM3(regs) != AUDIT_AVC)
        return 1;

    /* 记录这一刻 current 的 sid 就是 scontext，只按来源域判定 */
    sid = current_sid();
    if (!sid)
        return 1;

    if (sid != READ_ONCE(avc_silent_ksu_sid) && !avc_silent_is_source(sid))
        return 1;

    if (mode == AVC_SILENT_MODE_OBSERVE) {
        /* 一档：只说明命中，返回值原样保留 */
        pr_info_ratelimited("avc_silent: observe: AVC record from sid %u matches filter\n", sid);
        return 1;
    }

    /* 二档：只有要丢弃的记录才挂 return 探针 */
    return 0;
}

static int avc_silent_log_ret(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    (void)ri;

    /*
     * 只把返回值清成 NULL，让调用方的 format/end 空转。
     *
     * 缓冲区不在这里释放：struct audit_buffer 由审计子系统私有的 kmem_cache
     * 分配，模块里拿不到那条释放路径（audit_buffer_free/audit_buffer_cache
     * 都是 kernel/audit.c 内部静态符号），擅自 kfree/kfree_skb 属于对
     * 未知分配器的手工释放，一旦布局或分配器变化就是重复释放或释放后使用。
     * 按"证明不了就不做"的原则，这里宁可让这条缓冲区泄漏，每丢弃一条记录
     * 泄漏一个 audit_buffer（该档必须由用户显式开启，且开启时会打印告警）。
     */
    if (PT_REGS_RC(regs))
        PT_REGS_RC(regs) = 0;

    return 0;
}

static struct kretprobe avc_silent_kretprobe = {
    .entry_handler = avc_silent_log_entry,
    .handler = avc_silent_log_ret,
};

/*
 * avc_audit 在 6.1 GKI 上会被 LTO 改名/内联，按裸符号注册拿不到地址；
 * audit_log_start 在审计子系统里一定存在，这里再用 kallsyms 解析一次，
 * 兼容 "name.llvm.<hash>" / "name$variant" 这类改写。
 */
static unsigned long avc_silent_resolve_addr(const char *name)
{
    const char *names[1] = { name };
    unsigned long addr = 0;

    if (ksu_lookup_kernel_symbols(names, &addr, 1) <= 0)
        return 0;

    return addr;
}
#endif /* CONFIG_KRETPROBES */

/* 装上钩子；失败返回 errno，由调用方把档位退回 0 */
static int avc_silent_install(void)
{
    int ret;

    mutex_lock(&avc_silent_mutex);
    if (READ_ONCE(ksu_avc_silent_hooked)) {
        mutex_unlock(&avc_silent_mutex);
        return 0;
    }

    ret = avc_silent_resolve_sids();
    if (ret)
        goto out;

#ifdef CONFIG_KRETPROBES
    {
        unsigned long addr = avc_silent_resolve_addr("audit_log_start");

        if (!addr) {
            pr_err("avc_silent: audit_log_start not found, feature disabled\n");
            ret = -ENOENT;
            goto out;
        }
        avc_silent_kretprobe.kp.addr = (kprobe_opcode_t *)addr;
    }

    ret = register_kretprobe(&avc_silent_kretprobe);
    if (ret) {
        pr_err("avc_silent: register kretprobe on audit_log_start failed: %d, feature disabled\n", ret);
        goto out;
    }

    WRITE_ONCE(ksu_avc_silent_hooked, true);
    pr_info("avc_silent: kretprobe installed on audit_log_start\n");
#else
    pr_err("avc_silent: kretprobes are not available, feature disabled\n");
    ret = -EOPNOTSUPP;
#endif
out:
    mutex_unlock(&avc_silent_mutex);
    return ret;
}

static void avc_silent_uninstall(void)
{
    mutex_lock(&avc_silent_mutex);
    if (READ_ONCE(ksu_avc_silent_hooked)) {
#ifdef CONFIG_KRETPROBES
        unregister_kretprobe(&avc_silent_kretprobe);
#endif
        WRITE_ONCE(ksu_avc_silent_hooked, false);
    }
    mutex_unlock(&avc_silent_mutex);
}

static int avc_silent_feature_get(u64 *value)
{
    // 钩子没装上时如实返回 0，不把静默失败伪装成已启用
    if (!READ_ONCE(ksu_avc_silent_hooked)) {
        *value = AVC_SILENT_MODE_OFF;
        return 0;
    }

    *value = READ_ONCE(ksu_avc_silent_mode);
    return 0;
}

static int avc_silent_feature_set(u64 value)
{
    switch (value) {
    case AVC_SILENT_MODE_OFF:
        WRITE_ONCE(ksu_avc_silent_mode, AVC_SILENT_MODE_OFF);
        avc_silent_uninstall();
        return 0;
    case AVC_SILENT_MODE_OBSERVE:
    case AVC_SILENT_MODE_ENFORCE:
        if (avc_silent_install()) {
            WRITE_ONCE(ksu_avc_silent_mode, AVC_SILENT_MODE_OFF);
            return -EOPNOTSUPP;
        }
        WRITE_ONCE(ksu_avc_silent_mode, (u8)value);
        if (value == AVC_SILENT_MODE_ENFORCE)
            pr_warn("avc_silent: enforce mode on, every dropped record leaks one audit buffer\n");
        else
            pr_info("avc_silent: observe mode on, records are not modified\n");
        return 0;
    default:
        return -EINVAL;
    }
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

/*
 * 默认关闭：启动阶段只解析并打印 SID，不安装任何钩子；要生效必须由用户
 * 显式 set。解析放在这里是为了早一点暴露 ksu 域没建好的问题。
 */
void ksu_avc_silent_late_init(void)
{
    if (avc_silent_resolve_sids())
        pr_warn("avc_silent: sid resolution failed, feature stays off\n");
}

void __exit ksu_avc_silent_exit(void)
{
    ksu_unregister_feature_handler(KSU_FEATURE_AVC_SILENT);
    WRITE_ONCE(ksu_avc_silent_mode, AVC_SILENT_MODE_OFF);
    avc_silent_uninstall();
}
