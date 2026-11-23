#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/utsname.h>
#include <linux/sched.h>
#include "hook/syscall_hook.h"
#include "policy/feature.h"
#include "uapi/feature.h"
#include "klog.h"

static bool ksu_spoof_uname_enabled __read_mostly;

// 伪造值，可通过 module param 或 supercall 下发
static char spoofed_release[64] = "5.10.0-gki";
static char spoofed_version[256] = "#1 SMP PREEMPT Thu Jan  1 00:00:00 UTC 1970";

static long hook_newuname(int orig_nr, const struct pt_regs *regs) {
    // 先执行原始 syscall
    long ret = ksu_syscall_table[orig_nr](regs);
    
    if (ret == 0 && ksu_spoof_uname_enabled && current_uid().val >= 10000) {
        struct new_utsname __user *name = (struct new_utsname __user *)syscall_arg0(regs);
        
        // 覆写 release
        if (spoofed_release[0]) {
            if (copy_to_user(name->release, spoofed_release, strlen(spoofed_release) + 1))
                return -EFAULT;
        }
        
        // 覆写 version
        if (spoofed_version[0]) {
            if (copy_to_user(name->version, spoofed_version, strlen(spoofed_version) + 1))
                return -EFAULT;
        }
    }
    
    return ret;
}

static int spoof_uname_feature_get(u64 *value) {
    *value = ksu_spoof_uname_enabled;
    return 0;
}

static int spoof_uname_feature_set(u64 value) {
    ksu_spoof_uname_enabled = value != 0;
    return 0;
}

static const struct ksu_feature_handler spoof_uname_handler = {
    .feature_id = KSU_FEATURE_SPOOF_UNAME,
    .name = "spoof_uname",
    .get_handler = spoof_uname_feature_get,
    .set_handler = spoof_uname_feature_set,
};

void __init ksu_spoof_uname_init(void) {
    ksu_register_syscall_hook(__NR_newuname, hook_newuname);
    ksu_register_feature_handler(&spoof_uname_handler);
    pr_info("kernelsu: spoof_uname initialized\n");
}
