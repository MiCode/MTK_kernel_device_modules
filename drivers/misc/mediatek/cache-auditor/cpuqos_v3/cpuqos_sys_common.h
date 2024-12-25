/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef CPUQOS_SYS_COMMON_H
#define CPUQOS_SYS_COMMON_H
#include <linux/module.h>

extern int init_cpuqos_common_sysfs(void);
extern void cleanup_cpuqos_common_sysfs(void);

extern struct kobj_attribute trace_enable_attr;
extern struct kobj_attribute boot_complete_attr;
extern struct kobj_attribute show_L3m_status_attr;
extern struct kobj_attribute resource_pct_attr;

#endif
