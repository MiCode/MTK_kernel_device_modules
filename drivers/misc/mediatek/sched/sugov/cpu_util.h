/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef _CPU_UTIL_H
#define _CPU_UTIL_H

bool is_runnable_boost_enable(void);
void set_runnable_boost_enable(bool boost_ctrl);
void unset_runnable_boost_enable(void);

void hook_cpu_util_cfs_boost(void *data, int cpu, unsigned long *util);

unsigned long mtk_cpu_util_cfs(int cpu);
unsigned long mtk_cpu_util_cfs_boost(int cpu);

unsigned long mtk_cpu_util_next(int cpu, struct task_struct *p, int dst_cpu, int boost);

#endif /* _CPU_UTIL_H */
