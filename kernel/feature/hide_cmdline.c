#include <linux/moduleparam.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>

#include "feature/hide_cmdline.h"
#include "hook/patch_memory.h"
#include "infra/symbol_resolver.h"
#include "policy/feature.h"
#include "uapi/feature.h"
#include "klog.h" // IWYU pragma: keep

/*
 * 拦截面判定结论（对齐上游 fs/proc/cmdline.c）：
 *
 *   pde = proc_create_single("cmdline", 0, NULL, cmdline_proc_show);
 *   pde->size = saved_command_line_len + 1;
 *
 *   static int cmdline_proc_show(struct seq_file *m, void *v)
 *   {
 *           seq_puts(m, saved_command_line);
 *           seq_putc(m, '\n');
 *           return 0;
 *   }
 *
 * 所以 hide_cmdline 不能照抄 hide_kallsyms 的 seq_operations->show 方案：
 * proc_create_single() 内部走 single_open()，而 single_open() 在**每次 open**
 * 时 kmalloc 一个新的 struct seq_operations（single_release() 再 kfree）。
 * 内核镜像里不存在可长期 patch 的静态 cmdline_seq_ops 符号；cmdline_proc_show
 * 又是 static 函数，LTO 下可能被改名/内联，改它的函数入口要做指令级跳转，
 * 风险和收益完全不成比例。
 *
 * 也不走 read() 的 syscall hook：逐次 read 挂钩本身带时延特征（与本模块目标
 * 冲突），会和 runtime/ksud_integration.c 里已直接 hook 的 __NR_read
 * （ksu_sys_read）叠加，而且 dispatcher 只覆盖被标记进程 —— 未被授权的检测
 * App 线程没有 TIF_SYSCALL_TRACEPOINT，sys_enter 根本不触发，hook 对真正的
 * 对手不会执行。
 *
 * 落点选字符串本体 saved_command_line：init/main.c 的 setup_command_line()
 * 用 memblock_alloc(strlen(boot_command_line) + 1, SMP_CACHE_BYTES) 分配，
 * 是普通可写的线性映射内存（非 const、非 __ro_after_init），可直接 memcpy，
 * 不需要 ksu_patch_text 的 fixmap + stop_machine 通道。原地整行替换之后：
 *   - 零 syscall、零 hook：read()/read_iter() 完全不动，时延侧信道看不到开销。
 *   - 全 UID 一致：不存在 root 与 app 读到不同内容这种可交叉验证的破绽。
 *   - 长度不变：替换串强制 pad 到原长度，saved_command_line_len、pde->size
 *     和 inode 大小与实际读出字节数天然自洽，不留 “stat 长度 != read 长度” 特征。
 *   - /proc/bootconfig、/proc/sys/kernel/* 不受影响，但 /proc/cmdline 的
 *     androidboot.* 与 KSU 特征参数一并变干净。
 *
 * 时机安全：本模块在 KernelSU init 阶段执行，早于任何应用读 cmdline；而内核
 * 自身的参数解析、bootconfig 解析、initcall 早已在启动极早期完成，此时改写
 * saved_command_line 不会再改变内核行为。
 */

/*
 * 伪装缓冲上限。COMMAND_LINE_SIZE 在不同版本是 1024/2048 且需要额外头文件，
 * 这里不依赖它；实际写入长度以原厂串长度为准，不会越界。
 */
#define SPOOF_CMDLINE_MAX 4096

/* 原厂态整行 cmdline，加载时传入；留空则本模块不生效 */
static char spoof_cmdline[SPOOF_CMDLINE_MAX] = "";
module_param_string(spoof_cmdline, spoof_cmdline, sizeof(spoof_cmdline), 0);

static bool ksu_hide_cmdline_enabled __read_mostly;

static char *real_cmdline;   /* saved_command_line 指向的字符串 */
static char *orig_copy;      /* 原厂内容独立备份，只作还原读取源，绝不复用为暂存 */
static size_t cmdline_len;   /* 原始长度，替换后保持不变 */
static bool armed;           /* 已完成原地替换，开关才有意义 */

/*
 * saved_command_line 在 arm64/generic 内核里是 `char *`（指针），但部分厂商
 * 或老内核（arch/<arch>/kernel/setup.c）把它定义成 `char saved_command_line[]`
 * 静态数组。若不加判断就按指针解引用，数组形态下会把 cmdline 的前 8 个 ASCII
 * 字节当成地址，随后 strlen() 读这个野指针直接 oops。
 *
 * 这里用 phys_from_virt() 判定：它沿 init_mm 的页表把地址翻译成物理地址，
 * 只读内核页表、不解引用目标地址，任意垃圾值只会返回 -ENOENT。早期 memblock
 * 分配的 cmdline 缓冲区就落在线性映射区，因此该判定既能放行真指针，也能
 * 否掉数组形态误读出来的假地址。
 */
static bool ksu_is_kernel_va(const void *addr)
{
    int err = 0;

    if (!addr)
        return false;
    phys_from_virt((unsigned long)addr, &err);
    return !err;
}

static void hide_cmdline_write(const char *src)
{
    size_t n;

    if (!real_cmdline)
        return;

    /*
     * 定长替换：先把原区间全部填空格，再从头拷入目标串。memset 只覆盖
     * [0, cmdline_len)，不含末尾 NUL；memblock 申请的就是 cmdline_len + 1
     * 字节，因此不会越界。
     */
    memset(real_cmdline, ' ', cmdline_len);
    n = strlen(src);
    if (n > cmdline_len)
        n = cmdline_len;
    memcpy(real_cmdline, src, n);
}

static int hide_cmdline_feature_get(u64 *value)
{
    *value = (armed && READ_ONCE(ksu_hide_cmdline_enabled)) ? 1 : 0;
    return 0;
}

static int hide_cmdline_feature_set(u64 value)
{
    if (!armed) {
        pr_warn("hide_cmdline: not armed (spoof_cmdline empty or symbol missing), cannot toggle\n");
        return -EINVAL;
    }

    hide_cmdline_write(value ? spoof_cmdline : orig_copy);
    WRITE_ONCE(ksu_hide_cmdline_enabled, value != 0);
    return 0;
}

static const struct ksu_feature_handler hide_cmdline_handler = {
    .feature_id = KSU_FEATURE_HIDE_CMDLINE,
    .name = "hide_cmdline",
    .get_handler = hide_cmdline_feature_get,
    .set_handler = hide_cmdline_feature_set,
};

void __init ksu_hide_cmdline_init(void)
{
    unsigned long sym;

    if (!spoof_cmdline[0]) {
        pr_warn("hide_cmdline: spoof_cmdline is empty, feature inert\n");
        goto register_feature;
    }

    sym = find_kernel_symbol_exact("saved_command_line");
    if (!sym) {
        pr_warn("hide_cmdline: saved_command_line not found, feature inert\n");
        goto register_feature;
    }

    /* 指针形态：解引用；野指针或数组形态则退化为把符号地址当缓冲区 */
    real_cmdline = *(char **)sym;
    if (!ksu_is_kernel_va(real_cmdline)) {
        pr_info("hide_cmdline: saved_command_line looks like a static array, using it directly\n");
        real_cmdline = (char *)sym;
    }

    if (!ksu_is_kernel_va(real_cmdline)) {
        pr_warn("hide_cmdline: cmdline buffer is not a valid kernel address, feature inert\n");
        real_cmdline = NULL;
        goto register_feature;
    }

    cmdline_len = strnlen(real_cmdline, SPOOF_CMDLINE_MAX);
    if (!cmdline_len || cmdline_len >= SPOOF_CMDLINE_MAX) {
        pr_warn("hide_cmdline: implausible cmdline length %zu, feature inert\n", cmdline_len);
        real_cmdline = NULL;
        cmdline_len = 0;
        goto register_feature;
    }

    orig_copy = kvmalloc(cmdline_len + 1, GFP_KERNEL);
    if (!orig_copy) {
        pr_err("hide_cmdline: out of memory\n");
        goto register_feature;
    }

    memcpy(orig_copy, real_cmdline, cmdline_len);
    orig_copy[cmdline_len] = '\0';

    if (strlen(spoof_cmdline) > cmdline_len) {
        pr_warn("hide_cmdline: spoof_cmdline (%zu) longer than real (%zu), will be truncated\n",
                strlen(spoof_cmdline), cmdline_len);
    }

    hide_cmdline_write(spoof_cmdline);
    armed = true;
    WRITE_ONCE(ksu_hide_cmdline_enabled, true);

    pr_info("hide_cmdline: cmdline spoofed in place (%zu bytes)\n", cmdline_len);

register_feature:
    if (ksu_register_feature_handler(&hide_cmdline_handler))
        pr_err("hide_cmdline: failed to register feature handler\n");
}

void __exit ksu_hide_cmdline_exit(void)
{
    ksu_unregister_feature_handler(KSU_FEATURE_HIDE_CMDLINE);

    if (armed) {
        hide_cmdline_write(orig_copy);
        armed = false;
        WRITE_ONCE(ksu_hide_cmdline_enabled, false);
        real_cmdline = NULL;
        cmdline_len = 0;
        pr_info("hide_cmdline: real cmdline restored\n");
    }

    if (orig_copy) {
        kvfree(orig_copy);
        orig_copy = NULL;
    }
}
