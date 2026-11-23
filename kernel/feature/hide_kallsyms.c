#include <linux/slab.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include "hook/syscall_hook.h"
#include "infra/symbol_resolver.h"
#include "policy/feature.h"
#include "uapi/feature.h"
#include "klog.h"

static bool ksu_hide_kallsyms_enabled __read_mostly;

static const char *forbidden_patterns[] = {
    "ksu", "kernelsu", "supercall", "ksud", NULL
};

static bool should_hide_line(const char *line, size_t len) {
    for (int i = 0; forbidden_patterns[i]; i++) {
        if (memmem(line, len, forbidden_patterns[i], strlen(forbidden_patterns[i])))
            return true;
    }
    return false;
}

// Hook read 系统调用，过滤 kallsyms 内容
static long hook_read(int orig_nr, const struct pt_regs *regs) {
    long ret = ksu_syscall_table[orig_nr](regs);
    
    if (ret > 0 && ksu_hide_kallsyms_enabled && current_uid().val >= 10000) {
        int fd = (int)syscall_arg0(regs);
        char __user *buf = (char __user *)syscall_arg1(regs);
        
        // 检查 fd 是否指向 kallsyms
        struct file *f = fget(fd);
        if (f && f->f_path.dentry && f->f_path.dentry->d_name.name) {
            const char *name = f->f_path.dentry->d_name.name;
            if (strcmp(name, "kallsyms") == 0) {
                // 读取用户空间缓冲区内容，过滤敏感行
                char *kbuf = kmalloc(ret, GFP_KERNEL);
                if (kbuf && copy_from_user(kbuf, buf, ret) == 0) {
                    size_t out_len = 0;
                    char *line_start = kbuf;
                    char *line_end;
                    
                    while (line_start < kbuf + ret) {
                        line_end = memchr(line_start, '\n', kbuf + ret - line_start);
                        if (!line_end) line_end = kbuf + ret;
                        else line_end++; // 包含换行符
                        
                        size_t line_len = line_end - line_start;
                        if (!should_hide_line(line_start, line_len)) {
                            if (copy_to_user(buf + out_len, line_start, line_len)) {
                                kfree(kbuf);
                                fput(f);
                                return -EFAULT;
                            }
                            out_len += line_len;
                        }
                        line_start = line_end;
                    }
                    kfree(kbuf);
                    fput(f);
                    return out_len; // 返回过滤后的长度
                }
                kfree(kbuf);
            }
        }
        if (f) fput(f);
    }
    
    return ret;
}

static int hide_kallsyms_feature_get(u64 *value) {
    *value = ksu_hide_kallsyms_enabled;
    return 0;
}

static int hide_kallsyms_feature_set(u64 value) {
    ksu_hide_kallsyms_enabled = value != 0;
    return 0;
}

static const struct ksu_feature_handler hide_kallsyms_handler = {
    .feature_id = KSU_FEATURE_HIDE_KALLSYMS,
    .name = "hide_kallsyms",
    .get_handler = hide_kallsyms_feature_get,
    .set_handler = hide_kallsyms_feature_set,
};

void __init ksu_hide_kallsyms_init(void) {
    ksu_register_syscall_hook(__NR_read, hook_read);
    ksu_register_feature_handler(&hide_kallsyms_handler);
    pr_info("kernelsu: hide_kallsyms initialized\n");
}
