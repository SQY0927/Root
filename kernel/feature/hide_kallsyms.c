#include <linux/slab.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/uidgid.h>
#include <linux/sched.h>
#include <linux/kallsyms.h>
#include <linux/module.h>

#include "feature/hide_kallsyms.h"
#include "infra/symbol_resolver.h"
#include "policy/feature.h"
#include "uapi/feature.h"
#include "hook/patch_memory.h"
#include "klog.h" // IWYU pragma: keep

static bool ksu_hide_kallsyms_enabled __read_mostly;

// 命中即丢弃整行；大小写不敏感，覆盖 ksu / KernelSU / SUPERCALL 等变体
static const char *forbidden_patterns[] = { "ksu", "kernelsu", "supercall", "ksud", NULL };

static int (*orig_kallsyms_show)(struct seq_file *m, void *v);
static int (**kallsyms_show_slot)(struct seq_file *m, void *v);

static bool line_has_forbidden(const char *buf, size_t len)
{
    for (int i = 0; forbidden_patterns[i]; i++) {
        size_t plen = strlen(forbidden_patterns[i]);
        for (size_t j = 0; j + plen <= len; j++) {
            if (strncasecmp(buf + j, forbidden_patterns[i], plen) == 0)
                return true;
        }
    }
    return false;
}

/*
 * 替换 seq_operations->show：让原始 show 先把这一行写进 buffer，
 * 若含敏感关键字则回退 m->count 丢弃整行。
 * read()/read_iter 完全不被 hook，syscall 时延侧信道看不到任何开销。
 * __nocfi：被 patch 进回调表的函数必须加，避免 CFI trap（对照 my_setprocattr）。
 */
static int __nocfi hooked_kallsyms_show(struct seq_file *m, void *v)
{
    int (*orig)(struct seq_file *, void *) = READ_ONCE(orig_kallsyms_show);
    int ret;

    /*
     * 兜底：恢复失败或尚未完成初始化时原始指针可能为空，此时绝不能解引用。
     * 返回 0 让 seq_file 认为这一行无输出（读到 EOF），而不是 Oops。
     */
    if (unlikely(!orig))
        return 0;

    // 快路径：未启用 或 非应用进程，直接透传，零额外开销
    if (likely(!READ_ONCE(ksu_hide_kallsyms_enabled) || current_uid().val < 10000))
        return orig(m, v);

    size_t before = m->count;
    ret = orig(m, v);

    if (ret == 0 && m->count > before &&
        line_has_forbidden(m->buf + before, m->count - before)) {
        m->count = before; // 丢弃这一行
        return 0;
    }
    return ret;
}

static int hide_kallsyms_feature_get(u64 *value)
{
    *value = READ_ONCE(ksu_hide_kallsyms_enabled) ? 1 : 0;
    return 0;
}

static int hide_kallsyms_feature_set(u64 value)
{
    WRITE_ONCE(ksu_hide_kallsyms_enabled, value != 0);
    return 0;
}

static const struct ksu_feature_handler hide_kallsyms_handler = {
    .feature_id = KSU_FEATURE_HIDE_KALLSYMS,
    .name = "hide_kallsyms",
    .get_handler = hide_kallsyms_feature_get,
    .set_handler = hide_kallsyms_feature_set,
};

void __init ksu_hide_kallsyms_init(void)
{
    struct seq_operations *ops;
    int (*new_show)(struct seq_file *m, void *v);
    int ret;

    // kallsyms 的 seq_operations 符号名跨内核版本有差异，逐个尝试
    ops = find_kernel_symbol_exact("kallsyms_op");
    if (!ops)
        ops = find_kernel_symbol_exact("kallsyms_seq_ops");
    if (!ops)
        ops = find_kernel_symbol_exact("kallsyms_sops");

    if (!ops || !ops->show) {
        pr_warn("hide_kallsyms: cannot locate kallsyms seq_operations, feature inert\n");
        goto register_feature;
    }

    orig_kallsyms_show = ops->show;
    kallsyms_show_slot = &ops->show;

    new_show = hooked_kallsyms_show;
    ret = ksu_patch_text(kallsyms_show_slot, &new_show, sizeof(new_show),
                         KSU_PATCH_TEXT_FLUSH_DCACHE);
    if (ret) {
        pr_err("hide_kallsyms: patch_text show err: %d\n", ret);
        orig_kallsyms_show = NULL;
        kallsyms_show_slot = NULL;
    } else {
        /*
         * 钩子把 kallsyms seq_operations->show 指向了模块代码。模块卸载后
         * 这段文本会被释放，而槽位仍然指向它；且 __exit 里的恢复失败无法
         * 中止卸载——delete_module 在调用 exit 之前就已按引用计数放行。
         * 因此在 patch 成功后对自身取一个永不释放的引用，让 rmmod 直接
         * 返回 -EBUSY：宁可模块不可卸载，也不能卸载后让 /proc/kallsyms
         * 跳进已释放内存。
         *
         * 内建形态 THIS_MODULE 为 NULL，调用是 no-op；CONFIG_MODULE_UNLOAD
         * 关闭时本就无法卸载（delete_module 不存在），该调用不改变任何
         * 可观察行为。两种情况下都安全。
         */
        if (!try_module_get(THIS_MODULE))
            pr_err("hide_kallsyms: failed to pin module, rmmod may be unsafe\n");
        pr_info("hide_kallsyms: patched show -> %pS\n", hooked_kallsyms_show);
    }

register_feature:
    if (ksu_register_feature_handler(&hide_kallsyms_handler))
        pr_err("hide_kallsyms: failed to register feature handler\n");
}

void __exit ksu_hide_kallsyms_exit(void)
{
    int ret;

    ksu_unregister_feature_handler(KSU_FEATURE_HIDE_KALLSYMS);

    // 先复位使能标志：即便后续恢复失败，钩子也只剩原始 show 的透传语义
    WRITE_ONCE(ksu_hide_kallsyms_enabled, false);

    if (kallsyms_show_slot && orig_kallsyms_show) {
        ret = ksu_patch_text(kallsyms_show_slot, &orig_kallsyms_show,
                             sizeof(orig_kallsyms_show), KSU_PATCH_TEXT_FLUSH_DCACHE);
        if (ret) {
            /*
             * 正常卸载在 patch 成功时已被 init 中的自引用挡在 delete_module
             * 的引用计数检查之前，走不到这里；只有强制卸载（rmmod -f）等
             * 绕过引用计数的路径才可能到达。此时绝不能把 orig_kallsyms_show
             * 置空，否则就留下「钩子还挂着、原始指针为空」的必崩状态，
             * 保留原始指针让钩子继续正确工作（钩子内也有空指针兜底）。
             */
            pr_err("hide_kallsyms: restore show err: %d, keep hook with valid orig\n", ret);
        } else {
            orig_kallsyms_show = NULL;
            kallsyms_show_slot = NULL;
        }
    }
}