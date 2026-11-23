#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include "hook/syscall_hook.h"
#include "policy/feature.h"
#include "uapi/feature.h"
#include "klog.h"

static bool ksu_hide_cmdline_enabled __read_mostly;

static const char *cmdline_blacklist[] = {
    "kernelsu", "ksu_config", "androidboot.verifiedbootstate", 
    "androidboot.flash.locked", NULL
};

static long hook_read_cmdline(int orig_nr, const struct pt_regs *regs) {
    long ret = ksu_syscall_table[orig_nr](regs);
    
    if (ret > 0 && ksu_hide_cmdline_enabled && current_uid().val >= 10000) {
        int fd = (int)syscall_arg0(regs);
        char __user *buf = (char __user *)syscall_arg1(regs);
        
        struct file *f = fget(fd);
        if (f && f->f_path.dentry && f->f_path.dentry->d_name.name) {
            if (strcmp(f->f_path.dentry->d_name.name, "cmdline") == 0) {
                char *kbuf = kmalloc(ret, GFP_KERNEL);
                if (kbuf && copy_from_user(kbuf, buf, ret) == 0) {
                    // 简单处理：如果包含黑名单关键字，直接返回空
                    for (int i = 0; cmdline_blacklist[i]; i++) {
                        if (strstr(kbuf, cmdline_blacklist[i])) {
                            kfree(kbuf);
                            fput(f);
                            return 0; // 返回空
                        }
                    }
                }
                kfree(kbuf);
            }
        }
        if (f) fput(f);
    }
    
    return ret;
}

static int hide_cmdline_feature_get(u64 *value) {
    *value = ksu_hide_cmdline_enabled;
    return 0;
}

static int hide_cmdline_feature_set(u64 value) {
    ksu_hide_cmdline_enabled = value != 0;
    return 0;
}

static const struct ksu_feature_handler hide_cmdline_handler = {
    .feature_id = KSU_FEATURE_HIDE_CMDLINE,
    .name = "hide_cmdline",
    .get_handler = hide_cmdline_feature_get,
    .set_handler = hide_cmdline_feature_set,
};

void __init ksu_hide_cmdline_init(void) {
    ksu_register_syscall_hook(__NR_read, hook_read_cmdline);
    ksu_register_feature_handler(&hide_cmdline_handler);
    pr_info("kernelsu: hide_cmdline initialized\n");
}
