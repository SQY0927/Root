#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/string.h>

#include "feature/hide_proc_modules.h"
#include "infra/symbol_resolver.h"
#include "policy/feature.h"
#include "uapi/feature.h"
#include "hook/patch_memory.h"
#include "klog.h" // IWYU pragma: keep

/*
 * /sys/module/kernelsu 在 LKM 非调试构建里已被 kobject_del() 移除；
 * 若 /proc/modules 仍列出 kernelsu，就会留下「sysfs 没有、模块列表有」的矛盾。
 * 默认值与 sysfs 的隐藏条件保持一致，同时注册成可开关的 feature。
 */
#if defined(MODULE) && !defined(CONFIG_KSU_DEBUG)
static bool ksu_hide_proc_modules_enabled __read_mostly = true;
#else
static bool ksu_hide_proc_modules_enabled __read_mostly;
#endif

static int (*orig_modules_show)(struct seq_file *m, void *v);
static int (**modules_show_slot)(struct seq_file *m, void *v);

// 只在 patch 成功且回读确认后置位：对外状态以此为准，避免「钩子没装上却报 enabled」
static bool ksu_hide_proc_modules_hooked __read_mostly;

// 模块名在模块生命周期内恒定，直接引用即可，无需拷贝
static const char *hidden_module_name = "kernelsu";

static bool line_is_hidden_module(const char *buf, size_t len)
{
	size_t name_len = strlen(hidden_module_name);

	if (!name_len || len <= name_len)
		return false;
	if (memcmp(buf, hidden_module_name, name_len) != 0)
		return false;

	// /proc/modules 每行格式为 "<name> <size> ..."，名字之后必须是空格
	return buf[name_len] == ' ';
}

/*
 * /proc/modules 的 seq_operations 四个回调必须齐全，且结构本身按指针自然对齐；
 * 只认这种布局，避免把同名但形状不同的对象当成 seq_operations 去改写。
 */
static bool modules_ops_is_sane(const void *addr)
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
static noinline int __nocfi modules_call_show(int (*show)(struct seq_file *, void *), struct seq_file *m, void *v)
{
	return show(m, v);
}

/*
 * 替换 modules_op->show：原始 show 先写出完整行，命中目标模块则回退
 * m->count 丢弃该行。不 hook read()/read_iter()，syscall 侧无额外开销。
 *
 * CFI：本函数会被 seq_operations 间接调用，必须严格保持 show 的原型，
 * 且不能加 __nocfi——被间接调用的目标一旦缺少 CFI 类型信息，调用点会 trap。
 * 它自己发出的间接调用已收进 modules_call_show。
 */
static int hooked_modules_show(struct seq_file *m, void *v)
{
	int (*orig)(struct seq_file *, void *) = READ_ONCE(orig_modules_show);
	size_t before;
	int ret;

	// 兜底：恢复失败或尚未完成初始化时原始指针可能为空
	if (unlikely(!orig))
		return 0;

	if (likely(!READ_ONCE(ksu_hide_proc_modules_enabled)))
		return modules_call_show(orig, m, v);

	before = m->count;
	ret = modules_call_show(orig, m, v);

	if (ret == 0 && m->count > before &&
	    line_is_hidden_module(m->buf + before, m->count - before)) {
		m->count = before; // 丢弃这一行
		return 0;
	}
	return ret;
}

/*
 * 打补丁后回读槽位，确认它确实指向 hook。patch 被拒绝或回读不一致都算失败，
 * 后者还会把原始 show 写回，避免「返回失败、槽位却已改」的中间态。
 */
static int modules_install_hook(int (**slot)(struct seq_file *, void *), int (*new_show)(struct seq_file *, void *))
{
	int (*orig)(struct seq_file *, void *) = READ_ONCE(*slot);
	int ret;

	ret = ksu_patch_text(slot, &new_show, sizeof(new_show), KSU_PATCH_TEXT_FLUSH_DCACHE);
	if (ret) {
		pr_err("hide_proc_modules: patch_text show rejected: %d\n", ret);
		return ret;
	}

	if (READ_ONCE(*slot) == new_show)
		return 0;

	pr_err("hide_proc_modules: show readback mismatch, rolling back\n");
	ret = ksu_patch_text(slot, &orig, sizeof(orig), KSU_PATCH_TEXT_FLUSH_DCACHE);
	if (ret)
		pr_err("hide_proc_modules: rollback show err: %d\n", ret);

	return -EIO;
}

/*
 * 把 show 槽位写回原始指针。成功才清空本地状态；失败则保留原始指针，
 * 让仍挂着的钩子继续以透传语义正确工作（钩子内也有空指针兜底）。
 */
static int modules_restore_hook(void)
{
	int ret;

	ret = ksu_patch_text(modules_show_slot, &orig_modules_show,
			     sizeof(orig_modules_show), KSU_PATCH_TEXT_FLUSH_DCACHE);
	if (ret)
		return ret;

	orig_modules_show = NULL;
	modules_show_slot = NULL;
	return 0;
}

static int hide_proc_modules_feature_get(u64 *value)
{
	// 钩子没装上时如实返回 0，不把静默失败伪装成已启用
	if (!READ_ONCE(ksu_hide_proc_modules_hooked)) {
		*value = 0;
		return 0;
	}

	*value = READ_ONCE(ksu_hide_proc_modules_enabled) ? 1 : 0;
	return 0;
}

static int hide_proc_modules_feature_set(u64 value)
{
	if (!READ_ONCE(ksu_hide_proc_modules_hooked)) {
		pr_warn("hide_proc_modules: hook not installed, refusing to enable\n");
		return -EOPNOTSUPP;
	}

	WRITE_ONCE(ksu_hide_proc_modules_enabled, value != 0);
	return 0;
}

static const struct ksu_feature_handler hide_proc_modules_handler = {
    .feature_id = KSU_FEATURE_HIDE_PROC_MODULES,
    .name = "hide_proc_modules",
    .get_handler = hide_proc_modules_feature_get,
    .set_handler = hide_proc_modules_feature_set,
};

void __init ksu_hide_proc_modules_init(void)
{
	struct seq_operations *ops;
	int ret;

#ifdef MODULE
	if (THIS_MODULE && THIS_MODULE->name[0])
		hidden_module_name = THIS_MODULE->name;
#endif

	// /proc/modules 的 seq_operations 符号名跨内核版本有差异，逐个尝试
	ops = find_kernel_symbol_exact("modules_op");
	if (!ops)
		ops = find_kernel_symbol_exact("modules_seq_ops");
	if (!ops)
		ops = find_kernel_symbol_exact("proc_modules_ops");

	if (!modules_ops_is_sane(ops)) {
		pr_err("hide_proc_modules: hook NOT installed: /proc/modules seq_operations missing or malformed, feature reports unsupported\n");
		WRITE_ONCE(ksu_hide_proc_modules_enabled, false);
		return;
	}

	orig_modules_show = READ_ONCE(ops->show);
	modules_show_slot = &ops->show;

	ret = modules_install_hook(modules_show_slot, hooked_modules_show);
	if (ret) {
		pr_err("hide_proc_modules: hook NOT installed: show patch failed: %d, feature reports unsupported\n", ret);
		/*
		 * 只有确认槽位已回到原始 show 才清掉本地状态；若回滚也没成功，槽位
		 * 可能仍指向 hook，此时保留原始指针并钉住模块，让钩子以透传语义安全
		 * 存活、退出时再尝试恢复，绝不留下指向模块代码的悬空槽位。
		 */
		if (READ_ONCE(*modules_show_slot) == orig_modules_show) {
			orig_modules_show = NULL;
			modules_show_slot = NULL;
		} else {
			WRITE_ONCE(ksu_hide_proc_modules_hooked, true);
			if (!try_module_get(THIS_MODULE))
				pr_err("hide_proc_modules: failed to pin module, rmmod may be unsafe\n");
		}
		WRITE_ONCE(ksu_hide_proc_modules_enabled, false);
		return;
	}

	WRITE_ONCE(ksu_hide_proc_modules_hooked, true);
	/*
	 * 与 hide_kallsyms 相同：钩子指向模块代码，卸载后该文本会被释放。
	 * patch 成功后对自身取一个永不释放的引用，让 rmmod 返回 -EBUSY，
	 * 避免卸载后 /proc/modules 跳进已释放内存。
	 * 内建形态 THIS_MODULE 为 NULL，调用是 no-op。
	 */
	if (!try_module_get(THIS_MODULE))
		pr_err("hide_proc_modules: failed to pin module, rmmod may be unsafe\n");
	pr_info("hide_proc_modules: patched show -> %pS\n", (void *)hooked_modules_show);

	// 只有钩子真正装上才注册；注册失败就撤回钩子，不留无人管理的改写点
	if (ksu_register_feature_handler(&hide_proc_modules_handler)) {
		pr_err("hide_proc_modules: failed to register feature handler, uninstalling hook\n");
		WRITE_ONCE(ksu_hide_proc_modules_hooked, false);
		ret = modules_restore_hook();
		if (ret)
			pr_err("hide_proc_modules: restore show err: %d\n", ret);
		WRITE_ONCE(ksu_hide_proc_modules_enabled, false);
	}
}

void __exit ksu_hide_proc_modules_exit(void)
{
	int ret;

	if (READ_ONCE(ksu_hide_proc_modules_hooked))
		ksu_unregister_feature_handler(KSU_FEATURE_HIDE_PROC_MODULES);

	// 先复位使能标志：即便后续恢复失败，钩子也只剩原始 show 的透传语义
	WRITE_ONCE(ksu_hide_proc_modules_enabled, false);
	WRITE_ONCE(ksu_hide_proc_modules_hooked, false);

	if (modules_show_slot && orig_modules_show) {
		ret = modules_restore_hook();
		// 与 hide_kallsyms 一致：恢复失败时保留原始指针，避免钩子解引用 NULL
		if (ret)
			pr_err("hide_proc_modules: restore show err: %d, keep hook with valid orig\n", ret);
	}
}
