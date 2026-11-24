#ifndef __KSU_UAPI_FEATURE_H
#define __KSU_UAPI_FEATURE_H

enum ksu_feature_id {
    KSU_FEATURE_SU_COMPAT = 0,
    KSU_FEATURE_KERNEL_UMOUNT = 1,
    KSU_FEATURE_SULOG = 2,
    KSU_FEATURE_ADB_ROOT = 3,
    KSU_FEATURE_SELINUX_HIDE = 4,
    // 5 was webview_zygote_umount (v3.3.0), keep it unused so stale configs don't apply
    KSU_FEATURE_SHELL_SU_ADB_ONLY = 6,
    KSU_FEATURE_HIDE_KALLSYMS = 7,
    KSU_FEATURE_SU_COMPAT_PATH = 8,
    // 9 was hide_cmdline (this branch); removed in favour of SUSFS. Keep the
    // number unused so it is not recycled.
    KSU_FEATURE_MAX = 10
};

#endif
