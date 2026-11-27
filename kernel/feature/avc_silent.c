#include <linux/audit.h>
#include <linux/cache.h>
#include <linux/compiler.h>
#include <linux/cred.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/percpu.h>
#include <linux/preempt.h>
#include <linux/printk.h>
#include <linux/ptrace.h>
#include <linux/security.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/workqueue.h>

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
/* late_init 已经给出结论（支持 / 不支持）后不再重复判定 */
static bool avc_silent_checked __read_mostly;
/* SID 解析过之后 set 不必再解一次 */
static bool avc_silent_sids_ok __read_mostly;
/* handler 是否仍注册着：不支持时会注销，管理器据此显示“不支持” */
static bool avc_silent_registered __read_mostly;
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
 * 返回 0 表示可用；-ENOENT 表示本机没有 vendor_hal_perf_default（特性不适用）；
 * 其它错误表示策略还没准备好，可以稍后再试。
 *
 * 必须在 apply_kernelsu_rules() 之后调用：ksu 类型由那一步创建，之前解析
 * u:r:ksu:s0 会失败。来源域是否存在直接复用这次解析，不额外探测。
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

    /* 没有 perf HAL 的机型：这个特性不适用，按不支持处理 */
    if (avc_silent_resolve(avc_silent_sources[0], &avc_silent_source_sids[0])) {
        pr_info("avc_silent: %s not present, unsupported on this device\n", avc_silent_sources[0]);
        return -ENOENT;
    }

    for (i = 1; i < AVC_SILENT_SOURCE_COUNT; i++) {
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

struct audit_buffer; /* kernel/audit.c 内部结构，这里只保存和传递指针 */

static void avc_silent_enqueue(struct audit_buffer *ab);

static int avc_silent_log_ret(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    struct audit_buffer *ab;

    (void)ri;

    if (!PT_REGS_RC(regs))
        return 0;

    ab = (struct audit_buffer *)PT_REGS_RC(regs);
    /*
     * 先把返回值掐成 NULL，调用方的 format/end 就都空转了；
     * 指针随后交给延迟工作，在进程上下文里释放。
     */
    PT_REGS_RC(regs) = 0;
    avc_silent_enqueue(ab);

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

/*
 * 丢弃缓冲区后的回收。
 *
 * 探针里只把指针压进每 CPU 的环，纯原子/内存序操作：不睡眠、不抢会阻塞的锁，
 * 环满就丢指针（宁可多漏一条，也不在探针里等待）。真正的释放在延迟工作里、
 * 进程上下文中完成。
 *
 * 释放顺序对齐 kernel/audit.c 的 audit_buffer_free()：先 kfree_skb(ab->skb)
 * （导出符号），再把 ab 还回它来自的缓存；缓存指针解不到就只还 skb。
 */
#define AVC_SILENT_QUEUE_LEN 128
#define AVC_SILENT_RECLAIM_MS 1000

/* struct audit_buffer 的首成员就是 skb，这里只用这一个字段 */
struct avc_silent_ab_head {
    struct sk_buff *skb;
};

struct avc_silent_pcpu {
    struct audit_buffer *bufs[AVC_SILENT_QUEUE_LEN];
    unsigned int head; /* 消费者位置 */
    unsigned int tail; /* 生产者位置 */
    unsigned int lost; /* 环满被丢掉的条数，只做观测 */
};

static DEFINE_PER_CPU(struct avc_silent_pcpu, avc_silent_pcpu);
static struct kmem_cache *avc_silent_buffer_cache;

static void avc_silent_reclaim_workfn(struct work_struct *work);
static DECLARE_DELAYED_WORK(avc_silent_reclaim_work, avc_silent_reclaim_workfn);

static void avc_silent_enqueue(struct audit_buffer *ab)
{
    struct avc_silent_pcpu *q;
    unsigned int tail, next;

    /* 指针来源不可信就跳过 */
    if (!ab || ((unsigned long)ab & (sizeof(void *) - 1)))
        return;

    preempt_disable();
    q = this_cpu_ptr(&avc_silent_pcpu);
    tail = q->tail;
    next = (tail + 1) % AVC_SILENT_QUEUE_LEN;
    if (next == READ_ONCE(q->head)) {
        q->lost++;
        preempt_enable();
        return;
    }

    q->bufs[tail] = ab;
    smp_store_release(&q->tail, next);
    preempt_enable();
}

static void avc_silent_free_buffer(struct audit_buffer *ab)
{
    struct avc_silent_ab_head *head;
    struct sk_buff *skb;

    if (!ab || ((unsigned long)ab & (sizeof(void *) - 1)))
        return;

    head = (struct avc_silent_ab_head *)ab;
    skb = head->skb;
    if (skb) {
        head->skb = NULL;
        kfree_skb(skb);
    }

    if (avc_silent_buffer_cache)
        kmem_cache_free(avc_silent_buffer_cache, ab);
}

static void avc_silent_drain_cpu(struct avc_silent_pcpu *q)
{
    unsigned int head = READ_ONCE(q->head);
    unsigned int tail = smp_load_acquire(&q->tail);

    while (head != tail) {
        struct audit_buffer *ab = q->bufs[head];

        q->bufs[head] = NULL;
        head = (head + 1) % AVC_SILENT_QUEUE_LEN;
        smp_store_release(&q->head, head);
        avc_silent_free_buffer(ab);
    }
}

static void avc_silent_reclaim_workfn(struct work_struct *work)
{
    int cpu;

    (void)work;

    for_each_possible_cpu(cpu)
        avc_silent_drain_cpu(per_cpu_ptr(&avc_silent_pcpu, cpu));

    if (READ_ONCE(ksu_avc_silent_hooked))
        schedule_delayed_work(&avc_silent_reclaim_work, msecs_to_jiffies(AVC_SILENT_RECLAIM_MS));
}

/* 关特性 / 模块退出前把每个 CPU 的队列清空，不留悬空指针 */
static void avc_silent_flush_queues(void)
{
    int cpu;

    for_each_possible_cpu(cpu)
        avc_silent_drain_cpu(per_cpu_ptr(&avc_silent_pcpu, cpu));
}

/*
 * audit_buffer_cache 是 kernel/audit.c 的静态变量，按名字取到它的地址后读出
 * 缓存指针；取不到就只还 skb，绝不猜。
 */
static void avc_silent_resolve_buffer_cache(void)
{
    unsigned long addr;
    struct kmem_cache *cache;

    if (READ_ONCE(avc_silent_buffer_cache))
        return;

    addr = avc_silent_resolve_addr("audit_buffer_cache");
    if (!addr) {
        pr_warn("avc_silent: audit_buffer_cache not found, only skb will be freed\n");
        return;
    }

    cache = READ_ONCE(*(struct kmem_cache **)addr);
    if (!cache) {
        pr_warn("avc_silent: audit_buffer_cache is empty, only skb will be freed\n");
        return;
    }

    WRITE_ONCE(avc_silent_buffer_cache, cache);
    pr_info("avc_silent: audit buffer cache resolved, buffers are reclaimed\n");
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

    if (!READ_ONCE(avc_silent_sids_ok)) {
        ret = avc_silent_resolve_sids();
        if (ret) {
            ret = -EOPNOTSUPP;
            goto out;
        }
        WRITE_ONCE(avc_silent_sids_ok, true);
    }

#ifdef CONFIG_KRETPROBES
    {
        unsigned long addr = avc_silent_resolve_addr("audit_log_start");

        if (!addr) {
            pr_err("avc_silent: audit_log_start not found, feature disabled\n");
            ret = -ENOENT;
            goto out;
        }
        avc_silent_kretprobe.kp.addr = (kprobe_opcode_t *)addr;
        avc_silent_resolve_buffer_cache();
    }

    ret = register_kretprobe(&avc_silent_kretprobe);
    if (ret) {
        pr_err("avc_silent: register kretprobe on audit_log_start failed: %d, feature disabled\n", ret);
        goto out;
    }

    WRITE_ONCE(ksu_avc_silent_hooked, true);
    schedule_delayed_work(&avc_silent_reclaim_work, msecs_to_jiffies(AVC_SILENT_RECLAIM_MS));
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
#ifdef CONFIG_KRETPROBES
        /* 先停掉回收工作（它在 hooked 为 false 后不再续排），再把残留的指针清空 */
        cancel_delayed_work_sync(&avc_silent_reclaim_work);
        avc_silent_flush_queues();
#endif
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
        return;
    }

    WRITE_ONCE(avc_silent_registered, true);
}

/*
 * 在 apply_kernelsu_rules() + cache_sid() 之后调用。
 *
 * 默认档位就是拦截：来源域解析成功、本机有 perf HAL 时直接装钩子；
 * 没有 perf HAL 的机型注销 handler，管理器会显示“不支持”并置灰开关。
 */
void ksu_avc_silent_late_init(void)
{
    int ret;

    if (READ_ONCE(avc_silent_checked) || READ_ONCE(ksu_avc_silent_hooked))
        return;

    ret = avc_silent_resolve_sids();
    if (ret == -ENOENT) {
        WRITE_ONCE(avc_silent_checked, true);
        if (READ_ONCE(avc_silent_registered)) {
            ksu_unregister_feature_handler(KSU_FEATURE_AVC_SILENT);
            WRITE_ONCE(avc_silent_registered, false);
        }
        return;
    }
    if (ret) {
        /* 策略可能还没就绪，留给下一次调用 */
        pr_warn("avc_silent: sid resolution failed, feature stays off\n");
        return;
    }

    WRITE_ONCE(avc_silent_checked, true);
    WRITE_ONCE(avc_silent_sids_ok, true);

    if (avc_silent_install()) {
        WRITE_ONCE(ksu_avc_silent_mode, AVC_SILENT_MODE_OFF);
        return;
    }

    WRITE_ONCE(ksu_avc_silent_mode, AVC_SILENT_MODE_ENFORCE);
    pr_warn("avc_silent: enforce mode on, every dropped record leaks one audit buffer\n");
}

void __exit ksu_avc_silent_exit(void)
{
    if (READ_ONCE(avc_silent_registered)) {
        ksu_unregister_feature_handler(KSU_FEATURE_AVC_SILENT);
        WRITE_ONCE(avc_silent_registered, false);
    }
    WRITE_ONCE(ksu_avc_silent_mode, AVC_SILENT_MODE_OFF);
    avc_silent_uninstall();
}
