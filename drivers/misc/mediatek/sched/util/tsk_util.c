// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#include <sched/sched.h>

#include "common.h"
#include "tsk_util.h"
#include "sugov/cpufreq.h"
#include "eas/eas_trace.h"

#define DEFAULT_RUNNABLE_BOOST_UTIL_EST_ENABLE true
#define UTIL_EST_MARGIN (SCHED_CAPACITY_SCALE / 100) /* clone from kmainline UTIL_EST_MARGIN */

static bool runnable_boost_util_est_enable = DEFAULT_RUNNABLE_BOOST_UTIL_EST_ENABLE;

/* modified from kmainline util_est_update() */
void hook_mtk_util_est_update(void *data, struct cfs_rq *cfs_rq, struct task_struct *p,
		bool task_sleep, int *ret)
{
	unsigned int ewma, dequeued, last_ewma_diff;
	unsigned long runnable;
	bool runnable_aware;

	*ret = 1;

	if (!sched_feat(UTIL_EST))
		return;

	if (!task_sleep)
		return;

	ewma = READ_ONCE(p->se.avg.util_est);

	if (ewma & UTIL_AVG_UNCHANGED)
		return;

	dequeued = task_util(p);

	if (ewma <= dequeued) {
		ewma = dequeued;
		goto done;
	}

	last_ewma_diff = ewma - dequeued;
	if (last_ewma_diff < UTIL_EST_MARGIN)
		goto done;

	if (dequeued > arch_scale_cpu_capacity(cpu_of(rq_of(cfs_rq))))
		return;

	runnable = task_runnable(p);
	runnable_aware = is_runnable_boost_util_est_enable();

	if (trace_sched_task_uest_enabled())
		trace_sched_task_uest(p->pid, dequeued, ewma, ewma, runnable, runnable_aware);

	if (runnable_aware) {
		if ((dequeued + UTIL_EST_MARGIN) < runnable)
			goto done;
	}

	ewma <<= UTIL_EST_WEIGHT_SHIFT;
	ewma  -= last_ewma_diff;
	ewma >>= UTIL_EST_WEIGHT_SHIFT;
done:
	ewma |= UTIL_AVG_UNCHANGED;
	WRITE_ONCE(p->se.avg.util_est, ewma);
}

void set_runnable_boost_util_est_enable(int ctrl)
{
	if (_get_sched_debug_lock() == true)
		return;

	if (ctrl == -1) {
		runnable_boost_util_est_enable = DEFAULT_RUNNABLE_BOOST_UTIL_EST_ENABLE;
		return;
	}

	runnable_boost_util_est_enable = ctrl;
}
EXPORT_SYMBOL(set_runnable_boost_util_est_enable);

void reset_runnable_boost_util_est_enable(void)
{
	set_runnable_boost_util_est_enable(-1);
}
EXPORT_SYMBOL(reset_runnable_boost_util_est_enable);

bool is_runnable_boost_util_est_enable(void)
{
	return runnable_boost_util_est_enable;
}
EXPORT_SYMBOL(is_runnable_boost_util_est_enable);
