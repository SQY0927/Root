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

// 只在 patch 成功且回读确认后置位：feature 的对外状态以此为准，避免「钩子没装上却报 enabled」
static bool ksu_hide_kallsyms_hooked __read_mostly;

// 模块符号行末尾带 "\t[模块名]"，模块名在模块生命周期内恒定
static const char *kernelsu_module_name = "kernelsu";

static int (*orig_kallsyms_show)(struct seq_file *m, void *v);
static int (**kallsyms_show_slot)(struct seq_file *m, void *v);

/*
 * 只判定符号名开头，不再在全行任意位置找子串：
 * skb_checksum、f2fs_inode_chksum_* 这类名字里恰好含 "ksu" 的无关符号不再被误删。
 * 允许跳过前导下划线，__ksu_is_allow_uid 这类符号仍然命中。
 */
static bool symbol_name_is_forbidden(const char *name, size_t len)
{
    while (len > 0 && *name == '_') {
        name++;
        len--;
    }

    if (len >= 4 && !memcmp(name, "ksu_", 4))
        return true;
    if (len >= 8 && !memcmp(name, "kernelsu", 8))
        return true;
    if (len >= 9 && !memcmp(name, "supercall", 9))
        return true;

    return false;
}

// /proc/kallsyms 每行格式为 "<地址> <类型> <符号名>"，模块符号末尾再加 "\t[模块名]"
static const char *parse_symbol_name(const char *line, size_t len, size_t *name_len)
{
    size_t i = 0;
    size_t start;

    // 第一段：地址
    while (i < len && line[i] != ' ')
        i++;
    if (i >= len)
        return NULL;
    i++;

    // 第二段：类型字符
    if (i >= len || line[i] == ' ' || line[i] == '\t' || line[i] == '\n')
        return NULL;
    i++;
    if (i >= len || line[i] != ' ')
        return NULL;
    i++;

    start = i;
    while (i < len && line[i] != ' ' && line[i] != '\t' && line[i] != '\n' && line[i] != '\r')
        i++;

    *name_len = i - start;
    return line + start;
}

// 模块标签紧跟在符号名之后，形如 "\t[kernelsu]"；匹配到模块名即可整行丢弃
static bool line_has_kernelsu_module_tag(const char *line, size_t len)
{
    size_t end = len;
    size_t open;
    size_t name_len;

    while (end > 0 && (line[end - 1] == '\n' || line[end - 1] == '\r'))
        end--;
    if (end == 0 || line[end - 1] != ']')
        return false;

    open = end - 1;
    while (open > 0 && line[open - 1] != '\t')
        open--;
    if (open == 0 || line[open] != '[')
        return false;

    name_len = end - open - 2;
    if (name_len != strlen(kernelsu_module_name))
        return false;

    return !memcmp(line + open + 1, kernelsu_module_name, name_len);
}

static bool line_is_forbidden(const char *line, size_t len)
{
    const char *name;
    size_t name_len = 0;

    if (line_has_kernelsu_module_tag(line, len))
        return true;

    name = parse_symbol_name(line, len, &name_len);
    return name && symbol_name_is_forbidden(name, name_len);
}

/*
 * kallsyms 的 seq_operations 四个回调必须齐全，且结构本身按指针自然对齐；
 * 只认这种布局，避免把同名但形状不同的对象当成 seq_operations 去改写。
 */
static bool kallsyms_ops_is_sane(const void *addr)
{
    const struct seq_operations *ops = addr;

    if (!ops)
        return false;
    if ((unsigned long)ops & (__alignof__(*ops) - 1))
        return false;
    if (!ops->start || !ops->next || !ops->stop || !ops->show)
        return false;

    return true;
}

/*
 * 原始 show 指针是运行时从 seq_operations 读出来的，调用点无法做静态类型校验。
 * __nocfi 让这次间接调用跳过 CFI 检查；noinline 保证它不会被内联进调用方，
 * 否则属性丢失后间接调用又会被插桩。
 */
static noinline int __nocfi kallsyms_call_show(int (*show)(struct seq_file *, void *), struct seq_file *m, void *v)
{
    return show(m, v);
}

/*
 * 替换 seq_operations->show：让原始 show 先把这一行写进 buffer，
 * 若含敏感关键字则回退 m->count 丢弃整行。
 * read()/read_iter 完全不被 hook，syscall 时延侧信道看不到任何开销。
 *
 * CFI：本函数会被 seq_operations 间接调用，必须严格保持 show 的原型，
 * 且不能加 __nocfi——被间接调用的目标一旦缺少 CFI 类型信息，调用点会 trap。
 * 它自己发出的间接调用已收进 kallsyms_call_show。
 */
static int hooked_kallsyms_show(struct seq_file *m, void *v)
{
    int (*orig)(struct seq_file *, void *) = READ_ONCE(orig_kallsyms_show);
    int ret;
    size_t before;

    /*
     * 兜底：恢复失败或尚未完成初始化时原始指针可能为空，此时绝不能解引用。
     * 返回 0 让 seq_file 认为这一行无输出（读到 EOF），而不是 Oops。
     */
    if (unlikely(!orig))
        return 0;

    // 快路径：未启用 或 非应用进程，直接透传，零额外开销
    if (likely(!READ_ONCE(ksu_hide_kallsyms_enabled) || current_uid().val < 10000))
        return kallsyms_call_show(orig, m, v);

    before = m->count;
    ret = kallsyms_call_show(orig, m, v);

    if (ret == 0 && m->count > before &&
        line_is_forbidden(m->buf + before, m->count - before)) {
        m->count = before; // 丢弃这一行
        return 0;
    }
    return ret;
}

/*
 * 打补丁后回读槽位，确认它确实指向 hook。patch 被拒绝或回读不一致都算失败，
 * 后者还会把原始 show 写回，避免「返回失败、槽位却已改」的中间态。
 */
static int kallsyms_install_hook(int (**slot)(struct seq_file *, void *), int (*new_show)(struct seq_file *, void *))
{
    int (*orig)(struct seq_file *, void *) = READ_ONCE(*slot);
    int ret;

    ret = ksu_patch_text(slot, &new_show, sizeof(new_show), KSU_PATCH_TEXT_FLUSH_DCACHE);
    if (ret) {
        pr_err("hide_kallsyms: patch_text show rejected: %d\n", ret);
        return ret;
    }

    if (READ_ONCE(*slot) == new_show)
        return 0;

    pr_err("hide_kallsyms: show readback mismatch, rolling back\n");
    ret = ksu_patch_text(slot, &orig, sizeof(orig), KSU_PATCH_TEXT_FLUSH_DCACHE);
    if (ret)
        pr_err("hide_kallsyms: rollback show err: %d\n", ret);

    return -EIO;
}

/*
 * 把 show 槽位写回原始指针。成功才清空本地状态；失败则保留原始指针，
 * 让仍挂着的钩子继续以透传语义正确工作（钩子内也有空指针兜底）。
 */
static int kallsyms_restore_hook(void)
{
    int ret;

    ret = ksu_patch_text(kallsyms_show_slot, &orig_kallsyms_show,
                         sizeof(orig_kallsyms_show), KSU_PATCH_TEXT_FLUSH_DCACHE);
    if (ret)
        return ret;

    orig_kallsyms_show = NULL;
    kallsyms_show_slot = NULL;
    return 0;
}

static int hide_kallsyms_feature_get(u64 *value)
{
    // 钩子没装上时如实返回 0，不把静默失败伪装成已启用
    if (!READ_ONCE(ksu_hide_kallsyms_hooked)) {
        *value = 0;
        return 0;
    }

    *value = READ_ONCE(ksu_hide_kallsyms_enabled) ? 1 : 0;
    return 0;
}

static int hide_kallsyms_feature_set(u64 value)
{
    if (!READ_ONCE(ksu_hide_kallsyms_hooked)) {
        pr_warn("hide_kallsyms: hook not installed, refusing to enable\n");
        return -EOPNOTSUPP;
    }

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
    int ret;

#ifdef MODULE
    if (THIS_MODULE && THIS_MODULE->name[0])
        kernelsu_module_name = THIS_MODULE->name;
#endif

    // kallsyms 的 seq_operations 符号名跨内核版本有差异，逐个尝试
    ops = find_kernel_symbol_exact("kallsyms_op");
    if (!ops)
        ops = find_kernel_symbol_exact("kallsyms_seq_ops");
    if (!ops)
        ops = find_kernel_symbol_exact("kallsyms_sops");

    if (!kallsyms_ops_is_sane(ops)) {
        pr_err("hide_kallsyms: hook NOT installed: kallsyms seq_operations missing or malformed, feature reports unsupported\n");
        return;
    }

    orig_kallsyms_show = READ_ONCE(ops->show);
    kallsyms_show_slot = &ops->show;

    ret = kallsyms_install_hook(kallsyms_show_slot, hooked_kallsyms_show);
    if (ret) {
        pr_err("hide_kallsyms: hook NOT installed: show patch failed: %d, feature reports unsupported\n", ret);
        /*
         * 只有确认槽位已回到原始 show 才清掉本地状态；若回滚也没成功，槽位
         * 可能仍指向 hook，此时保留原始指针并钉住模块，让钩子以透传语义安全
         * 存活、退出时再尝试恢复，绝不留下指向模块代码的悬空槽位。
         */
        if (READ_ONCE(*kallsyms_show_slot) == orig_kallsyms_show) {
            orig_kallsyms_show = NULL;
            kallsyms_show_slot = NULL;
        } else {
            WRITE_ONCE(ksu_hide_kallsyms_hooked, true);
            if (!try_module_get(THIS_MODULE))
                pr_err("hide_kallsyms: failed to pin module, rmmod may be unsafe\n");
        }
        return;
    }

    WRITE_ONCE(ksu_hide_kallsyms_hooked, true);
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
    pr_info("hide_kallsyms: patched show -> %pS\n", (void *)hooked_kallsyms_show);

    // 只有钩子真正装上才注册；注册失败就撤回钩子，不留无人管理的改写点
    if (ksu_register_feature_handler(&hide_kallsyms_handler)) {
        pr_err("hide_kallsyms: failed to register feature handler, uninstalling hook\n");
        WRITE_ONCE(ksu_hide_kallsyms_hooked, false);
        ret = kallsyms_restore_hook();
        if (ret)
            pr_err("hide_kallsyms: restore show err: %d\n", ret);
    }
}

void __exit ksu_hide_kallsyms_exit(void)
{
    int ret;

    if (READ_ONCE(ksu_hide_kallsyms_hooked))
        ksu_unregister_feature_handler(KSU_FEATURE_HIDE_KALLSYMS);

    // 先复位使能标志：即便后续恢复失败，钩子也只剩原始 show 的透传语义
    WRITE_ONCE(ksu_hide_kallsyms_enabled, false);
    WRITE_ONCE(ksu_hide_kallsyms_hooked, false);

    if (kallsyms_show_slot && orig_kallsyms_show) {
        ret = kallsyms_restore_hook();
        if (ret) {
            /*
             * 正常卸载在 patch 成功时已被 init 中的自引用挡在 delete_module
             * 的引用计数检查之前，走不到这里；只有强制卸载（rmmod -f）等
             * 绕过引用计数的路径才可能到达。此时绝不能把 orig_kallsyms_show
             * 置空，否则就留下「钩子还挂着、原始指针为空」的必崩状态，
             * 保留原始指针让钩子继续正确工作（钩子内也有空指针兜底）。
             */
            pr_err("hide_kallsyms: restore show err: %d, keep hook with valid orig\n", ret);
        }
    }
}