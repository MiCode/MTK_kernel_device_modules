// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#include <sched/sched.h>

#include "common.h"
#include "cpu_util.h"
#include "sugov/cpufreq.h"
#include "sugov/sugov_trace.h"

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

void set_runnable_boost_enable(int ctrl)
{
	if (_get_sched_debug_lock() == true)
		return;

	if (ctrl == -1) {
		runnable_boost_enable = DEFAULT_RUNNABLE_BOOST;
		return;
	}

	if (ctrl < 0 || ctrl > 1)
		return;

	runnable_boost_enable = ctrl;
}
EXPORT_SYMBOL(set_runnable_boost_enable);

void unset_runnable_boost_enable(void)
{
	set_runnable_boost_enable(-1);
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

int mtk_effective_cpu_util_total(int cpu, struct task_struct *p, int dst_cpu, int runnable_boost,
		unsigned long *min, unsigned long *max,
		unsigned long *tsk_min_clp, unsigned long *tsk_max_clp,
		struct cpumask *sg_cpumask, unsigned long cpu_util_iowait, int curr_task_uclamp)
{
	int cpu_util_cfs, cpu_util_eff, cpu_util_mgn, cpu_util_clp, cpu_util_tal;
	struct task_struct *tsk = NULL;
	int source = -1;

	if (trace_sched_cpu_util_enabled()) {
		if (sg_cpumask) /* sugov */
			source = 0;
		else if (p && !rt_task(p)) { /* fair */
			if (min && max)
				source = 11; /* max_util */
			else
				source = 10; /* sum_util */
		} else { /* rt */
			if (min && max)
				source = 21; /* max_util */
			else
				source = 20; /* sum_util */
		}
	}

	if (sg_cpumask) { /* sugov */
		cpu_util_cfs = scx_cpuperf_target(cpu);

		if (!scx_switched_all())
			cpu_util_cfs += mtk_cpu_util_next(cpu, NULL, -1, 1);
	} else if (p && !rt_task(p)) /* normal */
		cpu_util_cfs = mtk_cpu_util_next(cpu, p, dst_cpu, runnable_boost);
	else /* rt */
		cpu_util_cfs = mtk_cpu_util_next(cpu, NULL, -1, runnable_boost);

#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
	if (sg_cpumask)
		cpu_util_eff = mtk_effective_cpu_util(cpu, cpu_util_cfs, (struct task_struct *)UINTPTR_MAX, min, max);
	else
		cpu_util_eff = mtk_effective_cpu_util(cpu, cpu_util_cfs, p, min, max);
#else
	cpu_util_eff = effective_cpu_util(cpu, cpu_util_cfs, min, max);
#endif

	if (sg_cpumask) /* sugov */
		cpu_util_eff = max(cpu_util_eff, (int)cpu_util_iowait);

	if (p) {
		if (!rt_task(p)) /* normal */
			tsk = (cpu == dst_cpu) ? p : NULL;
		else /* rt */
			tsk = p;
	}

	if (tsk && uclamp_is_used()) { /* normal rt */
		*min = max(*min, *tsk_min_clp);

		if (uclamp_rq_is_idle(cpu_rq(cpu)))
			*max = *tsk_max_clp;
		else
			*max = max(*max, *tsk_max_clp);
	}

	/* modified from kmainline sugov_effective_cpu_perf() */
	if (min && max) { /* sugov, normal max_util, rt max_util */
		cpu_util_mgn = mtk_effective_cpu_util_with_margin(cpu_util_eff, cpu, sg_cpumask, source);
		cpu_util_clp = mtk_effective_cpu_util_with_uclamp(cpu_util_mgn, cpu, *min, *max, curr_task_uclamp);
		cpu_util_tal = cpu_util_clp;
	} else { /* normal sum_util, rt sum_util */
		cpu_util_mgn = cpu_util_eff;
		cpu_util_clp = cpu_util_eff;
		cpu_util_tal = cpu_util_eff;
	}

	if (trace_sched_cpu_util_enabled())
		trace_sched_cpu_util(cpu, cpu_util_tal, cpu_util_clp, cpu_util_mgn, cpu_util_eff, cpu_util_cfs,
				min, max, dst_cpu, p, source, cpu_util_iowait);

	return cpu_util_tal;
}
EXPORT_SYMBOL_GPL(mtk_effective_cpu_util_total);
