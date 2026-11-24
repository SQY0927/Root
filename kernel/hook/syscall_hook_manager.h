#ifndef __KSU_H_HOOK_MANAGER
#define __KSU_H_HOOK_MANAGER

#include <asm/ptrace.h>
#include <linux/types.h>

// Hook manager initialization and cleanup
void ksu_syscall_hook_manager_init(void);
void ksu_syscall_hook_manager_exit(void);

// Register/unregister the stat/access syscall hooks backing su_compat_path.
void ksu_syscall_hook_sucompat_path_set(bool enable);

#endif
