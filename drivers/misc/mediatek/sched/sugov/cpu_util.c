/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#include <sched/sched.h>

#include "common.h"
#include "cpu_util.h"
#include "sugov_trace.h"

#define DEFAULT_RUNNABLE_BOOST	true

#define lsub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	*ptr -= min_t(typeof(*ptr), *ptr, _val);		\
} while (0)

static bool runnable_boost_enable = DEFAULT_RUNNABLE_BOOST;

bool is_runnable_boost_enable(void)
{
	return runnable_boost_enable;
}
EXPORT_SYMBOL(is_runnable_boost_enable);

void set_runnable_boost_enable(bool boost_ctrl)
{
	runnable_boost_enable = boost_ctrl;
}
EXPORT_SYMBOL(set_runnable_boost_enable);

void unset_runnable_boost_enable(void)
{
	runnable_boost_enable = DEFAULT_RUNNABLE_BOOST;
}
EXPORT_SYMBOL(unset_runnable_boost_enable);

/* hooked from cpu_util_cfs_boost() */
void hook_cpu_util_cfs_boost(void *data, int cpu, unsigned long *util)
{
	*util = mtk_cpu_util_cfs_boost(cpu);
}
EXPORT_SYMBOL_GPL(hook_cpu_util_cfs_boost);

/* cloned from kmainline cpu_util_cfs() */
unsigned long mtk_cpu_util_cfs(int cpu)
{
	return mtk_cpu_util_next(cpu, NULL, -1, 0);
}
EXPORT_SYMBOL_GPL(mtk_cpu_util_cfs);

/* cloned from kmainline cpu_util_cfs_boost() */
unsigned long mtk_cpu_util_cfs_boost(int cpu)
{
	return mtk_cpu_util_next(cpu, NULL, -1, 1);
}
EXPORT_SYMBOL_GPL(mtk_cpu_util_cfs_boost);

/* modified from kmainline cpu_util() */
unsigned long mtk_cpu_util_next(int cpu, struct task_struct *p, int dst_cpu, int boost)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;
	unsigned long util = READ_ONCE(cfs_rq->avg.util_avg);
	unsigned long runnable;

	if (is_runnable_boost_enable() && boost) {
		runnable = READ_ONCE(cfs_rq->avg.runnable_avg);
		util = max(util, runnable);
	}

	if (p && task_cpu(p) == cpu && dst_cpu != cpu)
		lsub_positive(&util, task_util(p));
	else if (p && task_cpu(p) != cpu && dst_cpu == cpu)
		util += task_util(p);

	if (sched_feat(UTIL_EST) && is_util_est_enable()) {
		unsigned long util_est;

		util_est = READ_ONCE(cfs_rq->avg.util_est);

		if (dst_cpu == cpu)
			util_est += _task_util_est(p);
		else if (p && unlikely(task_on_rq_queued(p) || current == p))
			lsub_positive(&util_est, _task_util_est(p));

		util = max(util, util_est);
	}

	if (trace_sched_runnable_boost_enabled())
		trace_sched_runnable_boost(is_runnable_boost_enable(), boost, cfs_rq->avg.util_avg,
				cfs_rq->avg.util_est, runnable, util);

	return min(util, arch_scale_cpu_capacity(cpu) + 1);
}
EXPORT_SYMBOL_GPL(mtk_cpu_util_next);
