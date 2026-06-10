// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#include <sched/sched.h>
#include <uapi/asm-generic/errno-base.h>

#include "common.h"
#include "tsk_util.h"
#include "sugov/cpufreq.h"
#include "eas/eas_trace.h"

#define DEFAULT_RUNNABLE_BOOST_UTIL_EST_ENABLE true
#define UTIL_EST_MARGIN (SCHED_CAPACITY_SCALE / 100) /* clone from kmainline UTIL_EST_MARGIN */
#define MAX_UTIL_EST_WEIGHT_SHIFT 20
#define MIN_UTIL_EST_WEIGHT_SHIFT 1

static bool runnable_boost_util_est_enable = DEFAULT_RUNNABLE_BOOST_UTIL_EST_ENABLE;

unsigned int get_task_uest_weight_shift(struct task_struct *p)
{
	struct uest_task_struct *uts = &((struct mtk_task *)android_task_vendor_data(p))->uest_task;
	unsigned int ret = uts->set ? uts->weight_shift : UTIL_EST_WEIGHT_SHIFT;

	return ret;
}
EXPORT_SYMBOL(get_task_uest_weight_shift);

/* modified from kmainline util_est_update() */
void hook_mtk_util_est_update(void *data, struct cfs_rq *cfs_rq, struct task_struct *p,
		bool task_sleep, int *ret)
{
	unsigned int ewma, dequeued, last_ewma_diff;
	unsigned int final_weight_shift;
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
	final_weight_shift = get_task_uest_weight_shift(p);

	if (trace_sched_task_uest_enabled())
		trace_sched_task_uest(p->pid, dequeued, ewma, ewma, runnable, runnable_aware, final_weight_shift);

	if (runnable_aware) {
		if ((dequeued + UTIL_EST_MARGIN) < runnable)
			goto done;
	}

	ewma <<= final_weight_shift;
	ewma  -= last_ewma_diff;
	ewma >>= final_weight_shift;
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

int __set_task_uest_weight_shift(struct task_struct *p, bool set, int val)
{
	struct uest_task_struct *uts;

	if (val < MIN_UTIL_EST_WEIGHT_SHIFT || val > MAX_UTIL_EST_WEIGHT_SHIFT)
		return -EINVAL;

	uts = &((struct mtk_task *)android_task_vendor_data(p))->uest_task;
	uts->set = set;
	uts->weight_shift = val;

	return 0;
}
EXPORT_SYMBOL(__set_task_uest_weight_shift);

int set_task_uest_weight_shift(int pid, bool set, int val)
{
	int ret = 0;
	struct task_struct *p;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		ret = __set_task_uest_weight_shift(p, set, val);
		put_task_struct(p);
	} else
		ret = -ESRCH;
	rcu_read_unlock();

	return ret;
}
EXPORT_SYMBOL(set_task_uest_weight_shift);

void init_uest_task_struct(struct task_struct *p)
{
	struct uest_task_struct *uts = &((struct mtk_task *)android_task_vendor_data(p))->uest_task;

	uts->set = 0;
	uts->weight_shift = UTIL_EST_WEIGHT_SHIFT;
}
