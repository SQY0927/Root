#ifndef __KSU_AVC_SILENT_H
#define __KSU_AVC_SILENT_H

#include <linux/types.h>

void __init ksu_avc_silent_init(void);
void ksu_avc_silent_late_init(void);
void __exit ksu_avc_silent_exit(void);

#endif
