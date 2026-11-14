/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef _CPU_UTIL_H
#define _CPU_UTIL_H

bool is_runnable_boost_enable(void);
void set_runnable_boost_enable(int ctrl);
void unset_runnable_boost_enable(void);

void hook_cpu_util_cfs_boost(void *data, int cpu, unsigned long *util);

unsigned long mtk_cpu_util_cfs(int cpu);
unsigned long mtk_cpu_util_cfs_boost(int cpu);

unsigned long mtk_cpu_util_next(int cpu, struct task_struct *p, int dst_cpu, int boost);

int mtk_effective_cpu_util_with_margin(int util, int cpu,
		struct cpumask *sg_cpumask, int source);

int mtk_effective_cpu_util_with_uclamp(int util, int cpu,
		unsigned long min, unsigned long max, int curr_task_uclamp);

int mtk_effective_cpu_util_total(int cpu, struct task_struct *p, int dst_cpu, int runnable_boost,
		unsigned long *min, unsigned long *max,
		unsigned long *tsk_min_clp, unsigned long *tsk_max_clp,
		struct cpumask *sg_cpumask, unsigned long cpu_util_iowait, int curr_task_uclamp);

#endif /* _CPU_UTIL_H */
