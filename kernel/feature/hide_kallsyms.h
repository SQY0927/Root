#ifndef __KSU_HIDE_KALLSYMS_H
#define __KSU_HIDE_KALLSYMS_H

#include <linux/types.h>

void __init ksu_hide_kallsyms_init(void);
void __exit ksu_hide_kallsyms_exit(void);

#endif
