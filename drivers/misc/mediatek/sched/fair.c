// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/cpuidle.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/cgroup.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/cgroup.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <linux/sched/clock.h>
#include <linux/arch_topology.h>
#include "eas/eas_plus.h"
#include "eas/shortcut/compress.h"
#include "util/cpu_util.h"
#include "eas/shortcut/gather.h"
#include "sugov/cpufreq.h"
#include "sugov/dsu_interface.h"
#include <sched/pelt.h>
#include <linux/stop_machine.h>
#include <linux/kthread.h>

#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
#include <thermal_interface.h>
#endif // CONFIG_MTK_THERMAL_INTERFACE
#include <mt-plat/mtk_irq_mon.h>
#include "common.h"
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
#include "mtk_energy_model/v3/energy_model.h"
#else
#include "mtk_energy_model/v1/energy_model.h"
#endif // CONFIG_MTK_GEARLESS_SUPPORT
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
#include "eas/vip.h"
#endif // CONFIG_MTK_SCHED_VIP_TASK
#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
#include "eas/group.h"
#endif // CONFIG_MTK_SCHED_FAST_LOAD_TRACKING
#define CREATE_TRACE_POINTS
#include "sched_trace.h"
#include "sugov/sched_version_ctrl.h"

MODULE_LICENSE("GPL");

/*
 * Unsigned subtract and clamp on underflow.
 *
 * Explicitly do a load-store to ensure the intermediate value never hits
 * memory. This allows lockless observations without ever seeing the negative
 * values.
 */
#define sub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	typeof(*ptr) val = (_val);				\
	typeof(*ptr) res, var = READ_ONCE(*ptr);		\
	res = var - val;					\
	if (res > var)						\
		res = 0;					\
	WRITE_ONCE(*ptr, res);					\
} while (0)

#define UTIL_EST_MARGIN (SCHED_CAPACITY_SCALE / 100)

/*
 * Remove and clamp on negative, from a local variable.
 *
 * A variant of sub_positive(), which does not use explicit load-store
 * and is thus optimized for local variable updates.
 */
#define lsub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	*ptr -= min_t(typeof(*ptr), *ptr, _val);		\
} while (0)

/* Runqueue only has SCHED_IDLE tasks enqueued */
static int sched_idle_rq(struct rq *rq)
{
	return unlikely(rq->nr_running == rq->cfs.idle_h_nr_running &&
			rq->nr_running);
}

#ifdef CONFIG_SMP
bool mtk_cpus_share_cache(unsigned int this_cpu, unsigned int that_cpu)
{
	if (this_cpu == that_cpu)
		return true;

	return topology_cluster_id(this_cpu) == topology_cluster_id(that_cpu);
}

static int sched_idle_cpu(int cpu)
{
	return sched_idle_rq(cpu_rq(cpu));
}

int task_fits_capacity(struct task_struct *p, long capacity, int cpu, unsigned int margin)
{
	unsigned long task_util;

	if (is_dpt_v2_support()) {
		int using_uclamp_freq = 0;

		task_util = uclamp_task_util_dpt_v2(p, cpu, &using_uclamp_freq);
		margin = using_uclamp_freq ? NO_MARGIN : margin;
		capacity = dpt_v2_local_capacity_all_util_of(cpu);
	} else
		task_util = uclamp_task_util(p);

	return fits_capacity(task_util, capacity, margin);
}

unsigned long capacity_of(int cpu)
{
	return cpu_rq(cpu)->cpu_capacity;

}

#if IS_ENABLED(CONFIG_MTK_EAS)
bool s_type = 1; /* 0: from tcm, 1: from util */
/*
 * Compute the task busy time for compute_energy(). This time cannot be
 * injected directly into effective_cpu_util() because of the IRQ scaling.
 * The latter only makes sense with the most recent CPUs where the task has
 * run.
 */
static inline void eenv_task_busy_time(struct energy_env *eenv,
				       struct task_struct *p, int prev_cpu)
{
	unsigned long busy_time, max_cap = arch_scale_cpu_capacity(prev_cpu);
	unsigned long irq = cpu_util_irq(cpu_rq(prev_cpu));
	/* unsigned long local_util_cpu_avg, local_util_coef1_avg, local_util_coef2_avg;
	 * unsigned int local_util_cpu_est, local_util_coef1_est, local_util_coef2_est;
	 */

	if (unlikely(irq >= max_cap))
		busy_time = max_cap;
	else {
		/*
		if (eenv->dpt_v2_support) {
			task_global_to_local_dpt_v2(prev_cpu, p, &local_util_cpu_avg, &local_util_coef1_avg, &local_util_coef2_avg, &local_util_cpu_est, &local_util_coef1_est, &local_util_coef2_est);
			busy_time = scale_irq_capacity(max(local_util_cpu_avg + local_util_coef1_avg + local_util_coef2_avg, local_util_cpu_est + local_util_coef1_est + local_util_coef2_est), irq, max_cap);
		}
		else
		*/
			busy_time = scale_irq_capacity(task_util_est(p), irq, max_cap);
	}

	eenv->task_busy_time = busy_time;
}

/*
 * Compute the perf_domain (PD) busy time for compute_energy(). Based on the
 * utilization for each @pd_cpus, it however doesn't take into account
 * clamping since the ratio (utilization / cpu_capacity) is already enough to
 * scale the EM reported power consumption at the (eventually clamped)
 * cpu_capacity.
 *
 * The contribution of the task @p for which we want to estimate the
 * energy cost is removed (by cpu_util_next()) and must be calculated
 * separately (see eenv_task_busy_time). This ensures:
 *
 *   - A stable PD utilization, no matter which CPU of that PD we want to place
 *     the task on.
 *
 *   - A fair comparison between CPUs as the task contribution (task_util())
 *     will always be the same no matter which CPU utilization we rely on
 *     (util_avg or util_est).
 *
 * Set @eenv busy time for the PD that spans @pd_cpus. This busy time can't
 * exceed @eenv->pd_cap.
 */
static inline void eenv_pd_busy_time(struct energy_env *eenv,
				struct cpumask *pd_cpus,
				struct task_struct *p)
{
	unsigned long busy_time = 0;
	int pd_idx = cpumask_first(pd_cpus);
	int cpu;
	/* unused = 0; */

	if (eenv->pds_busy_time[pd_idx] != -1)
		return;

	/*if (eenv->dpt_v2_support) {*/
	/*        for_each_cpu(cpu, pd_cpus) {*/
	/*                unsigned long dpt_v2_cpu_util = 0;*/
	/*                unsigned long dpt_v2_coef1_util = 0, dpt_v2_coef2_util = 0;*/

	/*                mtk_cpu_util_next_dpt_v2(cpu, p, -1, 0,*/
	/*                                &dpt_v2_cpu_util, &dpt_v2_coef1_util, &dpt_v2_coef2_util);*/

	/*                busy_time += mtk_effective_cpu_util_dpt_v2(cpu,*/
	/*                                &dpt_v2_cpu_util, &dpt_v2_coef1_util, &dpt_v2_coef2_util,*/
	/*                                NULL, NULL, NULL);*/
	/*        }*/
	/*} else {*/
	for_each_cpu(cpu, pd_cpus)
		busy_time += mtk_effective_cpu_util_total(cpu, p, -1, 0, NULL, NULL, NULL, NULL, NULL, 0, false);
	/*}*/

	eenv->pds_busy_time[pd_idx] = min(eenv->pds_cap[pd_idx], busy_time);
}

inline unsigned long
eenv_pd_max_util_dpt_v2(struct energy_env *eenv, struct cpumask *pd_cpus,
		 struct task_struct *p, int dst_cpu, unsigned long *min, unsigned long *max)
{
	unsigned long max_freq = 0, gear_max_freq = 0;
	int pd_idx = cpumask_first(pd_cpus);
	int cpu, dst_idx, pd_cpu = -1, gear_cpu = -1;

	for_each_cpu_and(cpu, get_gear_cpumask(eenv->gear_idx), cpu_active_mask) {
		struct task_struct *tsk = (cpu == dst_cpu) ? p : NULL;
		unsigned long dpt_v2_cpu_util_local = -1, dpt_v2_coef1_util_local = -1, dpt_v2_coef2_util_local = -1, freq = -1, unused = 0;

		dst_idx = (cpu == dst_cpu) ? 1 : 0;
		if (eenv->dpt_v2_freq[cpu][dst_idx] == -1) {
			mtk_cpu_util_next_dpt_v2(cpu, p, dst_cpu, 1, &dpt_v2_cpu_util_local, &dpt_v2_coef1_util_local, &dpt_v2_coef2_util_local);

			/*
			 * Performance domain frequency: utilization clamping
			 * must be considered since it affects the selection
			 * of the performance domain frequency.
			 * NOTE: in case RT tasks are running, by default the
			 * FREQUENCY_UTIL's utilization can be max OPP.
			 */
#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
				mtk_effective_cpu_util_dpt_v2(cpu, &dpt_v2_cpu_util_local, &dpt_v2_coef1_util_local, &dpt_v2_coef2_util_local,
					tsk, min, max);
#else
			// effective_cpu_util(cpu, util, FREQUENCY_UTIL, tsk);
#endif // CONFIG_MTK_CPUFREQ_SUGOV_EXT

			if (tsk && uclamp_is_used()) {
				*min = max(*min, uclamp_eff_value(p, UCLAMP_MIN));
				/*
				* If there is no active max uclamp constraint,
				* directly use task's one, otherwise keep max.
				*/
				if (uclamp_rq_is_idle(cpu_rq(cpu)))
					*max = uclamp_eff_value(p, UCLAMP_MAX);
				else
					*max = max(*max, uclamp_eff_value(p, UCLAMP_MAX));
			}

			mtk_map_util_freq_dpt_v2(NULL, cpu, &freq, &unused, pd_cpus, dpt_v2_cpu_util_local, dpt_v2_coef1_util_local, dpt_v2_coef2_util_local, *min, *max);
			eenv->dpt_v2_freq[cpu][dst_idx] = freq;
			eenv->dpt_v2_cpu_util[cpu][dst_idx] = dpt_v2_cpu_util_local;
			eenv->dpt_v2_coef1_util[cpu][dst_idx] = dpt_v2_coef1_util_local;
			eenv->dpt_v2_coef2_util[cpu][dst_idx] = dpt_v2_coef2_util_local;
			eenv->dpt_v2_cap_params[cpu][dst_idx].cpu_util_local = dpt_v2_cpu_util_local;
			eenv->dpt_v2_cap_params[cpu][dst_idx].total_util_local = dpt_v2_cpu_util_local +
					dpt_v2_coef1_util_local + dpt_v2_coef2_util_local;
			eenv->dpt_v2_sratio[cpu][dst_idx] = (dpt_v2_coef1_util_local + dpt_v2_coef2_util_local) *
					100 / eenv->dpt_v2_cap_params[cpu][dst_idx].total_util_local;
		} else {
			freq = eenv->dpt_v2_freq[cpu][dst_idx];
			dpt_v2_cpu_util_local = eenv->dpt_v2_cpu_util[cpu][dst_idx];
			dpt_v2_coef1_util_local = eenv->dpt_v2_coef1_util[cpu][dst_idx];
			dpt_v2_coef2_util_local = eenv->dpt_v2_coef2_util[cpu][dst_idx];
		}

		if (cpumask_test_cpu(cpu, pd_cpus)) {
			if (freq > max_freq) {
				pd_cpu = cpu;
				max_freq = freq;
			}
		}

		if (freq > gear_max_freq) {
			gear_cpu = cpu;
			gear_max_freq = freq;
		}

		/*get dst_cpu base utilization*/
		if (cpu == dst_cpu) {
			unsigned long dpt_v2_cpu_util_base_local = 0, dpt_v2_coef1_util_base_local = 0;
			unsigned long dpt_v2_coef2_util_base_local = 0, freq_base = 0;

			mtk_cpu_util_next_dpt_v2(cpu, p, -1, -1, &dpt_v2_cpu_util_base_local,
				&dpt_v2_coef1_util_base_local, &dpt_v2_coef2_util_base_local);

#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
			mtk_effective_cpu_util_dpt_v2(cpu, &dpt_v2_cpu_util_base_local, &dpt_v2_coef1_util_base_local,
				&dpt_v2_coef2_util_base_local, NULL, min, max);
#else
			// effective_cpu_util(cpu, util, FREQUENCY_UTIL, tsk);
#endif // CONFIG_MTK_CPUFREQ_SUGOV_EXT

			if (tsk && uclamp_is_used()) {
				*min = max(*min, uclamp_eff_value(p, UCLAMP_MIN));
				/*
				* If there is no active max uclamp constraint,
				* directly use task's one, otherwise keep max.
				*/
				if (uclamp_rq_is_idle(cpu_rq(cpu)))
					*max = uclamp_eff_value(p, UCLAMP_MAX);
				else
					*max = max(*max, uclamp_eff_value(p, UCLAMP_MAX));
			}

			mtk_map_util_freq_dpt_v2(NULL, cpu, &freq_base, &unused, pd_cpus, dpt_v2_cpu_util_base_local, dpt_v2_coef1_util_base_local, dpt_v2_coef2_util_base_local, *min, *max);
			eenv->dpt_v2_freq[cpu][0] = freq_base;
			eenv->dpt_v2_cpu_util[cpu][0] = dpt_v2_cpu_util_base_local;
			eenv->dpt_v2_coef1_util[cpu][0] = dpt_v2_coef1_util_base_local;
			eenv->dpt_v2_coef2_util[cpu][0] = dpt_v2_coef2_util_base_local;
			eenv->dpt_v2_cap_params[cpu][0].cpu_util_local = dpt_v2_cpu_util_base_local;
			eenv->dpt_v2_cap_params[cpu][0].total_util_local = dpt_v2_cpu_util_base_local +
					dpt_v2_coef1_util_base_local + dpt_v2_coef2_util_base_local;
			eenv->dpt_v2_sratio[cpu][0] = (dpt_v2_coef1_util_base_local + dpt_v2_coef2_util_base_local) *
					100 / eenv->dpt_v2_cap_params[cpu][0].total_util_local;

			if (trace_sched_max_util_dpt_v2_enabled())
				trace_sched_max_util_dpt_v2("cpu", cpu, dst_cpu, 0,
					eenv->dpt_v2_freq[cpu][0], cpu, dpt_v2_cpu_util_base_local, dpt_v2_coef1_util_base_local, dpt_v2_coef2_util_base_local);

			if (trace_sched_max_util_dpt_v2_sratio_enabled())
				trace_sched_max_util_dpt_v2_sratio(cpu, dst_idx, pd_idx, pd_cpu,
					eenv->dpt_v2_sratio[cpu][dst_idx], eenv->dpt_v2_sratio[cpu][0],
					eenv->dpt_v2_cpu_util[cpu][dst_idx], eenv->dpt_v2_coef1_util[cpu][dst_idx],
					eenv->dpt_v2_coef2_util[cpu][dst_idx]);
		}

		if (trace_sched_max_util_dpt_v2_enabled())
			trace_sched_max_util_dpt_v2("cpu", cpu, dst_cpu, dst_idx,
				eenv->dpt_v2_freq[cpu][dst_idx], cpu, dpt_v2_cpu_util_local, dpt_v2_coef1_util_local, dpt_v2_coef2_util_local);
	}

	dst_idx = (dst_cpu != -1) ? 1 : 0;
	if (trace_sched_max_util_dpt_v2_enabled())
		trace_sched_max_util_dpt_v2("pd", pd_idx, dst_cpu, dst_idx, max_freq, pd_cpu, -1, -1, -1);

	eenv->dpt_v2_gear_max_freq[eenv->gear_idx][dst_idx] = gear_max_freq;

	if (trace_sched_max_util_dpt_v2_enabled()) {
		trace_sched_max_util_dpt_v2("gear", eenv->gear_idx, dst_cpu, dst_idx,
			eenv->dpt_v2_gear_max_freq[eenv->gear_idx][dst_idx], gear_cpu, -1, -1, -1);
	}

	return max_freq;
}

/*
 * Compute the maximum utilization for compute_energy() when the task @p
 * is placed on the cpu @dst_cpu.
 *
 * Returns the maximum utilization among @eenv->cpus. This utilization can't
 * exceed @eenv->cpu_cap.
 */
static inline unsigned long
eenv_pd_max_util(struct energy_env *eenv, struct cpumask *pd_cpus,
		 struct task_struct *p, int dst_cpu, unsigned long *min, unsigned long *max)
{
	unsigned long max_util = 0, gear_max_util = 0;
	int pd_idx = cpumask_first(pd_cpus);
	int cpu, dst_idx, pd_cpu = -1, gear_cpu = -1;

	for_each_cpu_and(cpu, get_gear_cpumask(eenv->gear_idx), cpu_active_mask) {
		unsigned long cpu_util = -1;

		dst_idx = (cpu == dst_cpu) ? 1 : 0;
		if (eenv->cpu_max_util[cpu][dst_idx] == -1)
			cpu_util = eenv->cpu_max_util[cpu][dst_idx]
				= mtk_effective_cpu_util_total(cpu, p, dst_cpu, 1, min, max,
						&eenv->min_cap, &eenv->max_cap, NULL, 0, false);
		else
			cpu_util = eenv->cpu_max_util[cpu][dst_idx];

		if (cpumask_test_cpu(cpu, pd_cpus)) {
			pd_cpu = (max_util < cpu_util) ? cpu : pd_cpu;
			max_util = max(max_util, cpu_util);
		}

		gear_cpu = (gear_max_util < cpu_util) ? cpu : gear_cpu;
		gear_max_util = max(gear_max_util, cpu_util);

		/*get dst_cpu base utilization*/
		if (cpu == dst_cpu)
			eenv->cpu_max_util[cpu][0] =
				mtk_effective_cpu_util_total(cpu, p, -1, 1, min, max,
						&eenv->min_cap, &eenv->max_cap, NULL, 0, false);
	}

	dst_idx = (dst_cpu != -1) ? 1 : 0;
	if (trace_sched_max_util_enabled())
		trace_sched_max_util("pd", pd_idx, dst_cpu, dst_idx, max_util, pd_cpu, -1, -1);

	eenv->gear_max_util[eenv->gear_idx][dst_idx] = min(gear_max_util,
					eenv->pds_cpu_cap[pd_idx]);

	if (trace_sched_max_util_enabled()) {
		trace_sched_max_util("gear", eenv->gear_idx, dst_cpu, dst_idx,
			eenv->gear_max_util[eenv->gear_idx][dst_idx], gear_cpu, -1, -1);
	}

	return min(max_util, eenv->pds_cpu_cap[pd_idx]);
}


inline int reasonable_temp(int temp)
{
	if ((temp > 125) || (temp < -40))
		return 0;

	return 1;
}

bool dsu_pwr_enable;

void init_dsu_pwr_enable(void)
{
	dsu_pwr_enable = sched_dsu_pwr_enable_get();
}

inline bool is_dsu_pwr_concerned(int wl)
{
	if (wl == 4)
		return false;

	if (is_dpt_support_driver_hook) {
		if (is_dpt_support_driver_hook())
			return false;
	}

	return true;
}

inline bool is_dsu_pwr_triggered(int wl)
{
	return dsu_pwr_enable && is_dsu_pwr_concerned(wl);
}

DEFINE_PER_CPU(cpumask_var_t, mtk_select_rq_mask);
static inline void eenv_init(struct energy_env *eenv, struct task_struct *p,
			int prev_cpu, struct perf_domain *pd, bool in_irq)
{
	unsigned int cpu;

	eenv->dpt_v2_support = is_dpt_v2_support();
	eenv->dpt_v2_swpm_support = eenv->dpt_v2_support ? sched_dpt_v2_swpm_mode_get() : 0;

	eenv->min_cap = uclamp_eff_value(p, UCLAMP_MIN);
	eenv->max_cap = uclamp_eff_value(p, UCLAMP_MAX);

	eenv_task_busy_time(eenv, p, prev_cpu);

	for_each_cpu(cpu, cpu_possible_mask) {
		eenv->pds_busy_time[cpu] =  -1;
		eenv->cpu_max_util[cpu][0] =  -1;
		eenv->cpu_max_util[cpu][1] =  -1;
		eenv->gear_max_util[cpu][0] =  -1;
		eenv->gear_max_util[cpu][1] =  -1;
		eenv->pds_cpu_cap[cpu] = -1;
		eenv->pds_cap[cpu] = -1;
		eenv->pd_base_max_util[cpu] = 0;
		eenv->pd_base_freq[cpu] = 0;

		if (eenv->dpt_v2_support) {
			eenv->dpt_v2_freq[cpu][0] = -1;
			eenv->dpt_v2_freq[cpu][1] = -1;
			eenv->dpt_v2_gear_max_freq[cpu][0] = -1;
			eenv->dpt_v2_gear_max_freq[cpu][1] = -1;
			eenv->dpt_v2_sratio[cpu][0] = -1;
			eenv->dpt_v2_sratio[cpu][1] = -1;
		}
	}

	if (eenv->dpt_v2_support) {
		struct cpumask *cpus = this_cpu_cpumask_var_ptr(mtk_select_rq_mask);

		for (struct perf_domain *pd_ptr = pd; pd_ptr; pd_ptr = pd_ptr->next) {
			cpumask_and(cpus, perf_domain_span(pd_ptr), cpu_active_mask);
			if (cpumask_empty(cpus))
				continue;

			for_each_cpu(cpu, cpus) {
				if (cpu == 7)
					eenv->dpt_v2_cap_params[cpu][0].IPC_scaling_factor
						= eenv->dpt_v2_cap_params[cpu][1].IPC_scaling_factor = 1024;
				else
					eenv->dpt_v2_cap_params[cpu][0].IPC_scaling_factor
						= eenv->dpt_v2_cap_params[cpu][1].IPC_scaling_factor
						= get_task_ipc_scaling_factor(p, topology_cluster_id(cpu));

				if (!s_type) {
					dpt_rq_t *dpt_rq = &per_cpu(__dpt_rq, cpu);

					eenv->dpt_v2_sratio[cpu][0] = dpt_rq->sratio[S_TOTAL];
				}

				if (trace_sched_per_core_base_sratio_enabled())
					trace_sched_per_core_base_sratio(
							eenv->dpt_v2_support,
							cpu,
							eenv->dpt_v2_sratio[cpu][0]);
			}
		}
	}
}

void eenv_init_for_wl(struct energy_env *eenv)
{
	eenv->wl_support = get_eas_dsu_ctrl();

	/* get wl snapshot*/
	if (eenv->wl_support) {
		eenv->wl_cpu = get_em_wl();
		eenv->wl_dsu = get_wl_dsu();
	} else {
		eenv->wl_cpu = 0;
		eenv->wl_dsu = 0;
	}
}

void eenv_init_for_cap(struct energy_env *eenv, struct perf_domain *pd)
{
	for (struct perf_domain *pd_ptr = pd; pd_ptr; pd_ptr = pd_ptr->next) {
		unsigned long cpu_thermal_cap;
		struct cpumask *cpus = this_cpu_cpumask_var_ptr(mtk_select_rq_mask);
		int pd_idx, cpu;

		cpumask_and(cpus, perf_domain_span(pd_ptr), cpu_active_mask);
		if (cpumask_empty(cpus))
			continue;

		pd_idx = cpu = cpumask_first(cpus);
		eenv->pds_cpu_cap[pd_idx] = cpu_thermal_cap = mtk_get_actual_cpu_capacity(cpu);

		eenv->pds_cap[pd_idx] = 0;
		for_each_cpu(cpu, cpus) {
			if (eenv->dpt_v2_support)
				eenv->pds_cap[pd_idx] += (DPT_V2_MAX_RUNNING_TIME_LOCAL * get_thermal_freq_ceiling_ratio(cpu)) >> FREQ_CEILING_RATIO_BIT;
			else
				eenv->pds_cap[pd_idx] += cpu_thermal_cap;
		}
	}
}

void eenv_init_for_cpu(struct energy_env *eenv, struct task_struct *p, struct perf_domain *pd)
{
	unsigned long min, max;

	eenv->total_util = 0;

	for (struct perf_domain *pd_ptr = pd; pd_ptr; pd_ptr = pd_ptr->next) {
		struct cpumask *cpus = this_cpu_cpumask_var_ptr(mtk_select_rq_mask);
		unsigned long max_util, pd_freq;
		int pd_idx;

		cpumask_and(cpus, perf_domain_span(pd_ptr), cpu_active_mask);
		if (cpumask_empty(cpus))
			continue;

		pd_idx = cpumask_first(cpus);

		eenv_pd_busy_time(eenv, cpus, p);
		eenv->total_util += eenv->pds_busy_time[pd_idx];

		/* get dsu_freq_base by max_util voting */

		eenv->gear_idx = topology_cluster_id(pd_idx);

		max_util = eenv_pd_max_util(eenv, cpus, p, -1, &min, &max);
		eenv->pd_base_max_util[pd_idx] = max_util;

		if (eenv->dpt_v2_support) {
			pd_freq = eenv_pd_max_util_dpt_v2(eenv, cpus, p, -1, &min, &max);
			eenv->pd_base_freq[pd_idx] = max(pd_freq, per_cpu(min_freq, pd_idx));
		} else {
			pd_freq = pd_get_util_cpufreq(eenv, cpus, max_util,
					eenv->pds_cpu_cap[pd_idx], arch_scale_cpu_capacity(pd_idx), min, max);
			eenv->pd_base_freq[pd_idx] = max(pd_freq, per_cpu(min_freq, pd_idx));
		}
	}
}

void eenv_init_for_temp(struct energy_env *eenv)
{
	int cpu;

	for_each_cpu(cpu, cpu_possible_mask) {
#if IS_ENABLED(CONFIG_MTK_LEAKAGE_AWARE_TEMP)
		eenv->cpu_temp[cpu] = get_cpu_temp(cpu);
		eenv->cpu_temp[cpu] /= 1000;
#else
		eenv->cpu_temp[cpu] = -1;
#endif // CONFIG_MTK_LEAKAGE_AWARE_TEMP

		if (!reasonable_temp(eenv->cpu_temp[cpu])) {
			if (trace_sched_check_temp_enabled())
				trace_sched_check_temp("cpu", cpu, eenv->cpu_temp[cpu]);
		}
	}
}

void eenv_init_for_dsu(struct energy_env *eenv)
{
	int cpu;
	unsigned int output[11] = {[0 ... 10] = -1};
	unsigned int val[MAX_NR_CPUS] = {[0 ... MAX_NR_CPUS-1] = -1};

	if (is_dsu_pwr_triggered(eenv->wl_dsu)) {
		eenv_dsu_init(eenv->android_vendor_data1, false, eenv->wl_dsu,
				PERCORE_L3_BW, cpu_active_mask->bits[0], eenv->pd_base_freq,
				val, output);
	}

	if (PERCORE_L3_BW) {
		unsigned int sum_val = 0;

		for_each_cpu(cpu, cpu_active_mask) {
			sum_val += val[cpu];
			if (trace_sched_per_core_BW_enabled())
				trace_sched_per_core_BW(cpu, val[cpu], sum_val);
		}
	}

	if (!reasonable_temp(output[0])) {
		if (trace_sched_check_temp_enabled())
			trace_sched_check_temp("dsu", -1, (int) output[0]);
	}

	if (trace_sched_eenv_init_enabled())
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
		trace_sched_eenv_init(output[1], output[2], output[6], output[7],
				output[3], output[4], output[5], output[8], output[9], output[10],
				share_buck.gear_idx);
#else
		trace_sched_eenv_init(output[1], output[2], output[6], output[7],
				0, output[4], output[5],  output[8], output[9], output[10],
				share_buck.gear_idx);
#endif // CONFIG_MTK_THERMAL_INTERFACE
}

static inline unsigned long
mtk_compute_energy_cpu(struct energy_env *eenv, struct perf_domain *pd,
		       struct cpumask *pd_cpus, struct task_struct *p, int dst_cpu, int candidate_cpu)
{
	unsigned long min, max;
	unsigned long pd_max_util;
	unsigned long gear_max_util = -1;
	int pd_idx = cpumask_first(pd_cpus);
	unsigned long busy_time = eenv->pds_busy_time[pd_idx];
	unsigned long energy, extern_volt = -1;
	unsigned long dsu_volt = -1, pd_volt = -1, gear_volt = -1;
	int dst_idx, shared_buck_mode;
	unsigned long pd_freq = 0, gear_freq, scale_cpu;
	unsigned int dpt_v2_sratio = 0;
	int pid = task_pid_nr(p);

	scale_cpu = arch_scale_cpu_capacity(pd_idx);

	if (dst_cpu >= 0)
		busy_time = min(eenv->pds_cap[pd_idx], busy_time + eenv->task_busy_time);

	if (eenv->dpt_v2_support)
		pd_freq = eenv_pd_max_util_dpt_v2(eenv, pd_cpus, p, dst_cpu, &min, &max);
	else {
		pd_max_util = eenv_pd_max_util(eenv, pd_cpus, p, dst_cpu, &min, &max);
		if (pd_max_util == eenv->pd_base_max_util[pd_idx]) {
			pd_freq = eenv->pd_base_freq[pd_idx];
		} else {
			pd_freq = pd_get_util_cpufreq(eenv, pd_cpus, pd_max_util,
					eenv->pds_cpu_cap[pd_idx], scale_cpu, min, max);
		}
	}

	if (eenv->wl_support && is_dsu_pwr_triggered(eenv->wl_dsu)) {
		dsu_volt = update_dsu_status(eenv, false, pd_freq, pd_idx, dst_cpu);

		if (share_buck.gear_idx != eenv->gear_idx)
			dsu_volt = 0;
	} else {
		dsu_volt = 0;
	}

	dst_idx = (dst_cpu >= 0) ? 1 : 0;
	if (eenv->dpt_v2_support)
		dpt_v2_sratio = eenv->dpt_v2_sratio[candidate_cpu][dst_idx];

	/* dvfs power overhead */
	if (!cpumask_equal(pd_cpus, get_gear_cpumask(eenv->gear_idx))) {
		/* dvfs Vin/Vout */
		pd_volt = pd_get_freq_volt(pd_idx, pd_freq, false, eenv->wl_cpu);

		if (eenv->dpt_v2_support) {
			gear_freq = eenv->dpt_v2_gear_max_freq[eenv->gear_idx][dst_idx];
		} else {
			gear_max_util = eenv->gear_max_util[eenv->gear_idx][dst_idx];
			gear_freq = pd_get_util_cpufreq(eenv, pd_cpus, gear_max_util,
					eenv->pds_cpu_cap[pd_idx], scale_cpu, min, max);
		}
		gear_volt = pd_get_freq_volt(pd_idx, gear_freq, false, eenv->wl_cpu);

		if (gear_volt-pd_volt < volt_diff) {
			extern_volt = max(gear_volt, dsu_volt);
			energy =  mtk_em_cpu_energy(pid, pd->em_pd, pd_freq, busy_time,
					scale_cpu, eenv, extern_volt, pd_max_util, candidate_cpu,
					dpt_v2_sratio, eenv->dpt_v2_cap_params[candidate_cpu][dst_idx]);
			shared_buck_mode = 1;
		} else {
			extern_volt = 0;
			energy =  mtk_em_cpu_energy(pid, pd->em_pd, pd_freq, busy_time,
					scale_cpu, eenv, extern_volt, pd_max_util, candidate_cpu,
					dpt_v2_sratio, eenv->dpt_v2_cap_params[candidate_cpu][dst_idx]);
			energy = ((pd_volt) ? energy * max(gear_volt, dsu_volt) / pd_volt : energy);
			shared_buck_mode = 2;
		}
	} else {
		extern_volt = dsu_volt;
		energy =  mtk_em_cpu_energy(pid, pd->em_pd, pd_freq, busy_time,
				scale_cpu, eenv, extern_volt, pd_max_util, candidate_cpu,
				dpt_v2_sratio, eenv->dpt_v2_cap_params[candidate_cpu][dst_idx]);
		shared_buck_mode = 0;
	}

	if (trace_sched_compute_energy_enabled())
		trace_sched_compute_energy(dst_cpu, pd_idx, pd_cpus, energy, shared_buck_mode,
			gear_max_util, pd_max_util, busy_time,
			gear_volt, pd_volt, dsu_volt, extern_volt);

	return energy;
}

struct share_buck_info share_buck;
int get_share_buck(void)
{
	return share_buck.gear_idx;
}
EXPORT_SYMBOL_GPL(get_share_buck);

int init_share_buck(void)
{
	int ret;
	struct device_node *eas_node;

	/* Default share buck gear_idx=0 */
	share_buck.gear_idx = 0;
	eas_node = of_find_node_by_name(NULL, "eas-info");
	if (eas_node == NULL)
		pr_info("failed to find node @ %s\n", __func__);
	else {
		ret = of_property_read_u32(eas_node, "share-buck", &share_buck.gear_idx);
		if (ret < 0)
			pr_info("no share_buck err_code=%d %s\n", ret,  __func__);
	}

	share_buck.cpus = get_gear_cpumask(share_buck.gear_idx);


	return 0;
}

static inline int shared_gear(int gear_idx)
{
	return gear_idx == share_buck.gear_idx;
}

/*
 * compute_energy(): Use the Energy Model to estimate the energy that @pd would
 * consume for a given utilization landscape @eenv. When @dst_cpu < 0, the task
 * contribution is ignored.
 */
static inline unsigned long
mtk_compute_energy(struct energy_env *eenv, struct perf_domain *pd,
	       struct cpumask *pd_cpus, struct task_struct *p, int dst_cpu, int candidate_cpu)
{
	unsigned long cpu_pwr = 0, dsu_pwr = 0;
	unsigned long shared_pwr = 0, shared_pwr_dvfs = 0;
	unsigned int gear_idx;
	int dst_idx, use_base_freq = 0;
	int pd_idx = cpumask_first(pd_cpus);
	unsigned long total_util;
	unsigned long share_buck_freq;
	unsigned long dsu_extern_volt = 0;

	cpu_pwr = mtk_compute_energy_cpu(eenv, pd, pd_cpus, p, dst_cpu, candidate_cpu);

	/* indirect dvfs power overhead (when gear_max_util changes)*/
	if (!cpumask_equal(pd_cpus, get_gear_cpumask(eenv->gear_idx)) && 
		((eenv->dpt_v2_support && (eenv->dpt_v2_gear_max_freq[1]) > eenv->dpt_v2_gear_max_freq[0]) || /* not sure */
		(eenv->gear_max_util[eenv->gear_idx][1] > eenv->gear_max_util[eenv->gear_idx][0]))) {

		struct root_domain *rd = this_rq()->rd;
		struct perf_domain *pd_ptr;

		rcu_read_lock();
		pd_ptr = rcu_dereference(rd->pd);
		for (; pd_ptr; pd_ptr = pd_ptr->next) {
			struct cpumask *pd_mask = perf_domain_span(pd_ptr);
			unsigned int cpu = cpumask_first(pd_mask);

			if (cpu != dst_cpu && cpumask_test_cpu(cpu,
						get_gear_cpumask(eenv->gear_idx))) {
				gear_idx = eenv->gear_idx;
				eenv->gear_idx = topology_cluster_id(cpu);
				if (dst_cpu >= 0)
					shared_pwr_dvfs += mtk_compute_energy_cpu(eenv, pd_ptr,
									pd_mask, p, -2, cpu);
				else
					shared_pwr_dvfs += mtk_compute_energy_cpu(eenv, pd_ptr,
									pd_mask, p, -1, cpu);
				eenv->gear_idx = gear_idx;
			}
		}
		rcu_read_unlock();
	}

	if (!eenv->wl_support)
		goto done;

	/* calc indirect DSU share_buck */

	if (is_dsu_pwr_triggered(eenv->wl_dsu)) {
		if ((share_buck.gear_idx != -1) && !(shared_gear(eenv->gear_idx))
				&& dsu_freq_changed(eenv->android_vendor_data1)) {
			struct root_domain *rd = this_rq()->rd;
			struct perf_domain *pd_ptr, *share_buck_pd = 0;

			rcu_read_lock();
			pd_ptr = rcu_dereference(rd->pd);
			if (!pd_ptr)
				goto calc_sharebuck_done;

			for (; pd_ptr; pd_ptr = pd_ptr->next) {
				struct cpumask *pd_mask = perf_domain_span(pd_ptr);
				unsigned int cpu = cpumask_first(pd_mask);

				if (share_buck.gear_idx == topology_cluster_id(cpu)) {
					share_buck_pd = pd_ptr;
					break;
				}
			}

			if (!share_buck_pd)
				goto calc_sharebuck_done;

			/* calculate share_buck gear pwr with new DSU freq */
			gear_idx = eenv->gear_idx;
			eenv->gear_idx = share_buck.gear_idx;
			if (dst_cpu >= 0)
				shared_pwr = mtk_compute_energy_cpu(eenv, share_buck_pd,
								share_buck.cpus, p, -2, cpumask_first(share_buck.cpus));
			else
				shared_pwr = mtk_compute_energy_cpu(eenv, share_buck_pd,
								share_buck.cpus, p, -1, cpumask_first(share_buck.cpus));

			eenv->gear_idx = gear_idx;

calc_sharebuck_done:
			rcu_read_unlock();
		}
	}

	/* calc DSU power */
	if (dst_cpu >= 0 && (shared_gear(eenv->gear_idx))) {
		dst_idx = 1;
	} else {
		dst_idx = 0;
	}

	if ((share_buck.gear_idx != -1) &&
		(eenv->gear_max_util[share_buck.gear_idx][dst_idx] != -1)) {
		gear_idx = eenv->gear_idx;
		eenv->gear_idx = share_buck.gear_idx;
		pd_idx = cpumask_first(share_buck.cpus);

        if (eenv->dpt_v2_support) {
            if (eenv->dpt_v2_gear_max_freq[share_buck.gear_idx][dst_idx] == eenv->pd_base_freq[pd_idx]) 
                use_base_freq = 1;
        } else if (eenv->gear_max_util[share_buck.gear_idx][dst_idx] == eenv->pd_base_max_util[pd_idx])
            use_base_freq = 1;
		if (use_base_freq) {
			share_buck_freq = eenv->pd_base_freq[pd_idx];
		} else {
			if (eenv->dpt_v2_support)
				share_buck_freq = eenv->dpt_v2_gear_max_freq[eenv->gear_idx][dst_idx];
			else
				share_buck_freq = pd_get_util_cpufreq(eenv, pd_cpus,
						eenv->gear_max_util[share_buck.gear_idx][dst_idx],
						eenv->pds_cpu_cap[pd_idx], arch_scale_cpu_capacity(pd_idx),
						0, SCHED_CAPACITY_SCALE);
		}
		dsu_extern_volt = pd_get_freq_volt(cpumask_first(share_buck.cpus),
				share_buck_freq, false, eenv->wl_dsu);
		eenv->gear_idx = gear_idx;
	}

	if (PERCORE_L3_BW)
		total_util = eenv->cpu_max_util[eenv->dst_cpu][0]; /* the reason of computing cpu_max_util in eenv_init (via eenv_pd_max_util)*/
	else
		total_util = eenv->total_util;

	dsu_pwr = get_dsu_pwr(eenv->wl_dsu, dst_cpu, eenv->task_busy_time, total_util,
			eenv->android_vendor_data1, dsu_extern_volt, is_dsu_pwr_triggered(eenv->wl_dsu));

done:
	if (trace_sched_compute_energy_cpu_dsu_enabled())
		trace_sched_compute_energy_cpu_dsu(dst_cpu, eenv->wl_cpu, cpu_pwr, shared_pwr_dvfs,
					shared_pwr, dsu_pwr, cpu_pwr + shared_pwr + dsu_pwr);

	return cpu_pwr + shared_pwr_dvfs + shared_pwr + dsu_pwr;
}
#endif //CONFIG_MTK_EAS

static unsigned int uclamp_min_ls;
void set_uclamp_min_ls(unsigned int val)
{
	uclamp_min_ls = val;
}
EXPORT_SYMBOL_GPL(set_uclamp_min_ls);

unsigned int get_uclamp_min_ls(void)
{
	return uclamp_min_ls;
}
EXPORT_SYMBOL_GPL(get_uclamp_min_ls);

#if IS_ENABLED(CONFIG_MTK_EAS)
static void __sched_fork_init(struct task_struct *p)
{
	struct soft_affinity_task *sa_task = &((struct mtk_task *)
		android_task_vendor_data(p))->sa_task;

	sa_task->latency_sensitive = false;
	cpumask_copy(&sa_task->soft_cpumask, cpu_possible_mask);
}

static void android_rvh_sched_fork_init(void *unused, struct task_struct *p)
{
	__sched_fork_init(p);
}

void init_task_soft_affinity(void)
{
	struct task_struct *g, *p;
	int ret;

	/* init soft affinity related value to exist tasks */
	read_lock(&tasklist_lock);
	for_each_process_thread(g, p) {
		__sched_fork_init(p);
	}
	read_unlock(&tasklist_lock);

	/* init soft affinity related value to newly forked tasks */
	ret = register_trace_android_rvh_sched_fork_init(android_rvh_sched_fork_init, NULL);
	if (ret)
		pr_info("register sched_fork_init hooks failed, returned %d\n", ret);
}

void _init_tg_mask(struct cgroup_subsys_state *css)
{
	struct task_group *tg = css_tg(css);
	struct soft_affinity_tg *sa_tg = &((struct mtk_tg *) tg->android_vendor_data1)->sa_tg;

	cpumask_copy(&sa_tg->soft_cpumask, cpu_possible_mask);
}

void init_tg_soft_affinity(void)
{
	struct cgroup_subsys_state *css = &root_task_group.css;
	struct cgroup_subsys_state *top_css = css;

	/* init soft affinity related value to exist cgroups */
	rcu_read_lock();
	_init_tg_mask(&root_task_group.css);
	css_for_each_child(css, top_css)
		_init_tg_mask(css);
	rcu_read_unlock();

	/* init soft affinity related value to newly created cgroups in vip.c*/
}

void soft_affinity_init(void)
{
	init_task_soft_affinity();
	init_tg_soft_affinity();
}

void __set_group_prefer_cpus(struct task_group *tg, unsigned int cpumask_val)
{
	struct cpumask *tg_mask;
	unsigned long cpumask_ulval = cpumask_val;

	tg_mask = &(((struct mtk_tg *) tg->android_vendor_data1)->sa_tg.soft_cpumask);
	cpumask_copy(tg_mask, to_cpumask(&cpumask_ulval));
}

struct task_group *search_tg_by_name(char *group_name)
{
	struct cgroup_subsys_state *css = &root_task_group.css;
	struct cgroup_subsys_state *top_css = css;
	int ret = 0;

	rcu_read_lock();
	css_for_each_child(css, top_css)
		if (!strcmp(css->cgroup->kn->name, group_name)) {
			ret = 1;
			break;
		}
	rcu_read_unlock();

	if (ret)
		return css_tg(css);

	return &root_task_group;
}

void set_group_prefer_cpus_by_name(unsigned int cpumask_val, char *group_name)
{
	struct task_group *tg = search_tg_by_name(group_name);

	if (tg == &root_task_group)
		return;

	__set_group_prefer_cpus(tg, cpumask_val);
}

void get_task_group_cpumask_by_name(char *group_name, struct cpumask *__tg_mask)
{
	struct task_group *tg = search_tg_by_name(group_name);
	struct cpumask *tg_mask;

	tg_mask = &((struct mtk_tg *) tg->android_vendor_data1)->sa_tg.soft_cpumask;
	cpumask_copy(__tg_mask, tg_mask);
}

struct task_group *search_tg_by_cpuctl_id(unsigned int cpuctl_id)
{
	struct cgroup_subsys_state *css = &root_task_group.css;
	struct cgroup_subsys_state *top_css = css;
	int ret = 0;

	rcu_read_lock();
	css_for_each_child(css, top_css)
		if (css->id == cpuctl_id) {
			ret = 1;
			break;
		}
	rcu_read_unlock();

	if (ret)
		return css_tg(css);

	return &root_task_group;
}

/* start of soft affinity interface */
int set_group_prefer_cpus(unsigned int cpuctl_id, unsigned int cpumask_val)
{
	struct task_group *tg = search_tg_by_cpuctl_id(cpuctl_id);

	if (tg == &root_task_group)
		return 0;

	__set_group_prefer_cpus(tg, cpumask_val);
	return 1;
}
EXPORT_SYMBOL_GPL(set_group_prefer_cpus);

void set_top_app_cpumask(unsigned int cpumask_val)
{
	set_group_prefer_cpus_by_name(cpumask_val, "top-app");
}
EXPORT_SYMBOL_GPL(set_top_app_cpumask);

void set_foreground_cpumask(unsigned int cpumask_val)
{
	set_group_prefer_cpus_by_name(cpumask_val, "foreground");
}
EXPORT_SYMBOL_GPL(set_foreground_cpumask);

void set_background_cpumask(unsigned int cpumask_val)
{
	set_group_prefer_cpus_by_name(cpumask_val, "background");
}
EXPORT_SYMBOL_GPL(set_background_cpumask);

void get_group_prefer_cpus(unsigned int cpuctl_id, struct cpumask *__tg_mask)
{
	struct task_group *tg = search_tg_by_cpuctl_id(cpuctl_id);
	struct cpumask *tg_mask;

	tg_mask = &(((struct mtk_tg *) tg->android_vendor_data1)->sa_tg.soft_cpumask);
	cpumask_copy(__tg_mask, tg_mask);
}
EXPORT_SYMBOL_GPL(get_group_prefer_cpus);

struct cpumask top_app_cpumask;
struct cpumask foreground_cpumask;
struct cpumask background_cpumask;
struct cpumask *get_top_app_cpumask(void)
{
	get_task_group_cpumask_by_name("top-app", &top_app_cpumask);
	return &top_app_cpumask;
}
EXPORT_SYMBOL_GPL(get_top_app_cpumask);

struct cpumask *get_foreground_cpumask(void)
{
	get_task_group_cpumask_by_name("foreground", &foreground_cpumask);
	return &foreground_cpumask;
}
EXPORT_SYMBOL_GPL(get_foreground_cpumask);

struct cpumask *get_background_cpumask(void)
{
	get_task_group_cpumask_by_name("background", &background_cpumask);
	return &background_cpumask;
}
EXPORT_SYMBOL_GPL(get_background_cpumask);

void set_task_ls(int pid)
{
	struct task_struct *p;
	struct soft_affinity_task *sa_task;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		sa_task = &((struct mtk_task *)android_task_vendor_data(p))->sa_task;
		sa_task->latency_sensitive = true;
		put_task_struct(p);
	}
	rcu_read_unlock();
}
EXPORT_SYMBOL_GPL(set_task_ls);

void unset_task_ls(int pid)
{
	struct task_struct *p;
	struct soft_affinity_task *sa_task;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		sa_task = &((struct mtk_task *)android_task_vendor_data(p))->sa_task;
		sa_task->latency_sensitive = false;
		put_task_struct(p);
	}
	rcu_read_unlock();
}
EXPORT_SYMBOL_GPL(unset_task_ls);

void set_task_ls_prefer_cpus(int pid, unsigned int cpumask_val)
{
	struct task_struct *p;
	struct soft_affinity_task *sa_task;
	unsigned long cpumask_ulval = cpumask_val;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		sa_task = &((struct mtk_task *)android_task_vendor_data(p))->sa_task;
		sa_task->latency_sensitive = true;
		cpumask_copy(&sa_task->soft_cpumask, to_cpumask(&cpumask_ulval));
		put_task_struct(p);
	}
	rcu_read_unlock();
}
EXPORT_SYMBOL_GPL(set_task_ls_prefer_cpus);

void unset_task_ls_prefer_cpus(int pid)
{
	struct task_struct *p;
	struct soft_affinity_task *sa_task;
	unsigned long cpumask_ulval = 0xff;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		sa_task = &((struct mtk_task *)android_task_vendor_data(p))->sa_task;
		sa_task->latency_sensitive = false;
		cpumask_copy(&sa_task->soft_cpumask, to_cpumask(&cpumask_ulval));
		put_task_struct(p);
	}
	rcu_read_unlock();
}
EXPORT_SYMBOL_GPL(unset_task_ls_prefer_cpus);
/* end of soft affinity interface */

#if IS_ENABLED(CONFIG_UCLAMP_TASK_GROUP)
static inline bool cloned_uclamp_latency_sensitive(struct task_struct *p)
{
	struct cgroup_subsys_state *css = task_css(p, cpu_cgrp_id);
	struct task_group *tg;

	if (!css)
		return false;
	tg = container_of(css, struct task_group, css);

	return tg->latency_sensitive;
}
#else
static inline bool cloned_uclamp_latency_sensitive(struct task_struct *p)
{
	return false;
}
#endif /* CONFIG_UCLAMP_TASK_GROUP */

bool is_task_ls_uclamp(struct task_struct *p)
{
	bool latency_sensitive = false;

	if (!uclamp_min_ls)
		latency_sensitive = cloned_uclamp_latency_sensitive(p);
	else {
		latency_sensitive = (p->uclamp_req[UCLAMP_MIN].value > 0 ? 1 : 0) ||
					cloned_uclamp_latency_sensitive(p);
	}
	return latency_sensitive;
}

inline bool is_task_latency_sensitive(struct task_struct *p)
{
	struct soft_affinity_task *sa_task;
	bool latency_sensitive = false;

	sa_task = &((struct mtk_task *)android_task_vendor_data(p))->sa_task;
	rcu_read_lock();
	latency_sensitive = sa_task->latency_sensitive || is_task_ls_uclamp(p);
	rcu_read_unlock();

	return latency_sensitive;
}
EXPORT_SYMBOL_GPL(is_task_latency_sensitive);

inline void compute_effective_softmask(struct task_struct *p,
		bool *latency_sensitive, struct cpumask *dst_mask)
{
	struct cgroup_subsys_state *css;
	struct task_group *tg;
	struct cpumask *task_mask;
	struct cpumask *tg_mask;

	*latency_sensitive = is_task_latency_sensitive(p);
	css = task_css(p, cpu_cgrp_id);
	if (!*latency_sensitive || !css) {
		cpumask_copy(dst_mask, cpu_possible_mask);
		return;
	}

	tg = container_of(css, struct task_group, css);
	tg_mask = &((struct mtk_tg *) tg->android_vendor_data1)->sa_tg.soft_cpumask;

	task_mask = &((struct mtk_task *)android_task_vendor_data(p))->sa_task.soft_cpumask;

	if (!cpumask_and(dst_mask, task_mask, tg_mask)) {
		cpumask_copy(dst_mask, tg_mask);
		return;
	}
}

__read_mostly int num_sched_clusters;
cpumask_t __read_mostly ***cpu_array;

void init_cpu_array(void)
{
	int i, j;
	num_sched_clusters = get_nr_gears();

	cpu_array = kcalloc(num_sched_clusters, sizeof(cpumask_t **),
			GFP_ATOMIC | __GFP_NOFAIL);
	if (!cpu_array)
		free_cpu_array();

	for (i = 0; i < num_sched_clusters; i++) {
		cpu_array[i] = kcalloc(num_sched_clusters, sizeof(cpumask_t *),
				GFP_ATOMIC | __GFP_NOFAIL);
		if (!cpu_array[i])
			free_cpu_array();

		for (j = 0; j < num_sched_clusters; j++) {
			cpu_array[i][j] = kcalloc(2, sizeof(cpumask_t),
					GFP_ATOMIC | __GFP_NOFAIL);
			if (!cpu_array[i][j])
				free_cpu_array();
		}
	}
}

void build_cpu_array(void)
{
	int i;

	if (!cpu_array)
		free_cpu_array();

	/* Construct cpu_array row by row */
	for (i = 0; i < num_sched_clusters; i++) {
		int j, k = 1;

		/* Fill out first column with appropriate cpu arrays */
		cpumask_copy(&cpu_array[i][0][0], get_gear_cpumask(i));
		cpumask_copy(&cpu_array[i][0][1], get_gear_cpumask(i));
		/*
		 * k starts from column 1 because 0 is filled
		 * Fill clusters for the rest of the row,
		 * above i in ascending order
		 */
		for (j = i + 1; j < num_sched_clusters; j++) {
			cpumask_copy(&cpu_array[i][k][0], get_gear_cpumask(j));
			cpumask_copy(&cpu_array[i][j][1], get_gear_cpumask(j));
			k++;
		}

		/*
		 * k starts from where we left off above.
		 * Fill cluster below i in descending order.
		 */
		for (j = i - 1; j >= 0; j--) {
			cpumask_copy(&cpu_array[i][k][0], get_gear_cpumask(j));
			cpumask_copy(&cpu_array[i][i-j][1], get_gear_cpumask(j));
			k++;
		}
	}
}

void free_cpu_array(void)
{
	int i, j;

	if (!cpu_array)
		return;

	for (i = 0; i < num_sched_clusters; i++) {
		for (j = 0; j < num_sched_clusters; j++) {
			kfree(cpu_array[i][j]);
			cpu_array[i][j] = NULL;
		}
		kfree(cpu_array[i]);
		cpu_array[i] = NULL;
	}
	kfree(cpu_array);
	cpu_array = NULL;
}

static int mtk_wake_affine_idle(int this_cpu, int prev_cpu, int sync)
{
	if (mtk_available_idle_cpu(this_cpu) && mtk_cpus_share_cache(this_cpu, prev_cpu))
		return mtk_available_idle_cpu(prev_cpu) ? prev_cpu : this_cpu;

	if (sync && cpu_rq(this_cpu)->nr_running == 1)
		return this_cpu;

	if (mtk_available_idle_cpu(prev_cpu))
		return prev_cpu;

	return nr_cpumask_bits;
}

static inline unsigned long cfs_rq_load_avg(struct cfs_rq *cfs_rq)
{
	return cfs_rq->avg.load_avg;
}

static unsigned long cpu_load(struct rq *rq)
{
	return cfs_rq_load_avg(&rq->cfs);
}

#if IS_ENABLED(CONFIG_FAIR_GROUP_SCHED)
static void update_cfs_rq_h_load(struct cfs_rq *cfs_rq)
{
	struct rq *rq = rq_of(cfs_rq);
	struct sched_entity *se = cfs_rq->tg->se[cpu_of(rq)];
	unsigned long now = jiffies;
	unsigned long load;

	if (cfs_rq->last_h_load_update == now)
		return;

	WRITE_ONCE(cfs_rq->h_load_next, NULL);
	for (; se; se = se->parent) {
		cfs_rq = cfs_rq_of(se);
		WRITE_ONCE(cfs_rq->h_load_next, se);
		if (cfs_rq->last_h_load_update == now)
			break;
	}

	if (!se) {
		cfs_rq->h_load = cfs_rq_load_avg(cfs_rq);
		cfs_rq->last_h_load_update = now;
	}

	while ((se = READ_ONCE(cfs_rq->h_load_next)) != NULL) {
		load = cfs_rq->h_load;
		load = div64_ul(load * se->avg.load_avg,
				cfs_rq_load_avg(cfs_rq) + 1);
		cfs_rq = group_cfs_rq(se);
		cfs_rq->h_load = load;
		cfs_rq->last_h_load_update = now;
	}
}

unsigned long task_h_load(struct task_struct *p)
{
	struct cfs_rq *cfs_rq = task_cfs_rq(p);

	update_cfs_rq_h_load(cfs_rq);
	return div64_ul(p->se.avg.load_avg * cfs_rq->h_load,
			cfs_rq_load_avg(cfs_rq) + 1);
}
#else
static unsigned long task_h_load(struct task_struct *p)
{
	return p->se.avg.load_avg;
}
#endif // CONFIG_FAIR_GROUP_SCHED

static int mtk_wake_affine_weight(struct task_struct *p, int this_cpu, int prev_cpu, int sync)
{
	s64 this_eff_load, prev_eff_load;
	unsigned long task_load;
	//struct sched_domain *sd = NULL;

	/* since "sched_domain_mutex undefined" build error, comment search sched_domain */
	/* find least shared sched_domain for this_cpu and prev_cpu */
	//for_each_domain(this_cpu, sd) {
	//	if ((sd->flags & SD_WAKE_AFFINE) &&
	//		cpumask_test_cpu(prev_cpu, sched_domain_span(sd)))
	//		break;
	//}
	//if (unlikely(!sd))
	//	return nr_cpumask_bits;

	this_eff_load = cpu_load(cpu_rq(this_cpu));

	if (sync) {
		unsigned long current_load = task_h_load(current);

		if (current_load > this_eff_load)
			return this_cpu;

		this_eff_load -= current_load;
	}

	task_load = task_h_load(p);

	this_eff_load += task_load;
	if (sched_feat(WA_BIAS))
		this_eff_load *= 100;
	this_eff_load *= capacity_of(prev_cpu);

	prev_eff_load = cpu_load(cpu_rq(prev_cpu));
	prev_eff_load -= task_load;

	/* since "sched_domain_mutex undefined" build error, replace imbalance_pct with its val 117
	 * prev_eff_load *= 100 + (sd->imbalance_pct - 100) / 2;
	 */
	if (sched_feat(WA_BIAS))
		prev_eff_load *= 100 + (117 - 100) / 2;
	prev_eff_load *= capacity_of(this_cpu);

	/*
	 * If sync, adjust the weight of prev_eff_load such that if
	 * prev_eff == this_eff that select_idle_sibling() will consider
	 * stacking the wakee on top of the waker if no other CPU is
	 * idle.
	 */
	if (sync)
		prev_eff_load += 1;

	return this_eff_load < prev_eff_load ? this_cpu : nr_cpumask_bits;
}

static int mtk_wake_affine(struct task_struct *p, int this_cpu, int prev_cpu, int sync)
{
	int target = nr_cpumask_bits;

	if (sched_feat(WA_IDLE))
		target = mtk_wake_affine_idle(this_cpu, prev_cpu, sync);

	if (sched_feat(WA_WEIGHT) && target == nr_cpumask_bits)
		target = mtk_wake_affine_weight(p, this_cpu, prev_cpu, sync);

	if (target == nr_cpumask_bits)
		return prev_cpu;

	return target;
}

/*
 * Scan the asym_capacity domain for idle CPUs; pick the first idle one on whi
 * the task fits. If no CPU is big enough, but there are idle ones, try to
 * maximize capacity.
 */
static int
mtk_select_idle_capacity(struct task_struct *p, struct cpumask *allowed_cpumask, int target,
	bool is_vip, struct energy_env *eenv)
{
	unsigned long task_util, util_min, util_max, best_cap = 0;
	int cpu, best_cpu = -1;
	int fits, best_fits = 0;

	task_util = task_util_est(p);
	util_min = uclamp_eff_value(p, UCLAMP_MIN);
	util_max = uclamp_eff_value(p, UCLAMP_MAX);

	for_each_cpu_wrap(cpu, allowed_cpumask, target) {
		unsigned long cpu_cap = capacity_of(cpu);

		if (!is_vip && (!mtk_available_idle_cpu(cpu) && !sched_idle_cpu(cpu)))
			continue;

		fits = util_fits_capacity(task_util, util_min, util_max, cpu_cap, cpu);

		/* This CPU fits with all requirements */
		if (fits > 0)
			return cpu;

		/*
		 * Only the min performance hint (i.e. uclamp_min) doesn't fit.
		 * Look for the CPU with best capacity.
		 */

		else if (fits < 0)
			cpu_cap = mtk_get_actual_cpu_capacity(cpu);

		/*
		 * First, select CPU which fits better (-1 being better than 0).
		 * Then, select the one with best capacity at same level.
		 */
		if ((fits < best_fits) ||
			((fits == best_fits) && (cpu_cap > best_cap))) {
				best_cap = cpu_cap;
				best_cpu = cpu;
				best_fits = fits;
		}
	}

	return best_cpu;
}

struct cpumask bcpus;
static unsigned long util_Th;

void get_most_powerful_pd_and_util_Th(void)
{
	unsigned int nr_gear = get_nr_gears();

	/* no mutliple pd */
	if (nr_gear <= 1) {
		util_Th = 0;
		cpumask_clear(&bcpus);
		return;
	}

	/* pd_capacity_tbl is sorted by ascending order,
	 * so nr_gear-1 is most powerful gear and
	 * nr_gear is the second powerful gear.
	 */
	cpumask_copy(&bcpus, get_gear_cpumask(nr_gear-1));
	/* threshold is set to large capacity in mcpus */
	util_Th = (pd_get_opp_capacity(
		cpumask_first(get_gear_cpumask(nr_gear-2)), 0) >> 2);

}

bool gear_hints_enable;
static inline bool task_can_skip_this_cpu(struct task_struct *p, unsigned long p_uclamp_min,
		bool latency_sensitive, int cpu, struct cpumask *bcpus, int vip_prio, struct energy_env *eenv)
{
	bool cpu_in_bcpus;
	unsigned long task_util;
	struct task_gear_hints *ghts = &((struct mtk_task *)android_task_vendor_data(p))->gear_hints;

	if (prio_is_vip(vip_prio, NOT_VIP))
		return 0;

	if (latency_sensitive)
		return 0;

	if (gear_hints_enable &&
		ghts->gear_start >= 0 &&
		ghts->gear_start <= num_sched_clusters-1)
		return 0;

	if (p_uclamp_min > 0)
		return 0;

	if (cpumask_empty(bcpus))
		return 0;

	if (cpumask_weight(p->cpus_ptr) < 8)
		return 0;

	cpu_in_bcpus = cpumask_test_cpu(cpu, bcpus);
	/* comment out for the possibility of future necessity
	 *if (eenv->dpt_v2_support) {
	 *	task_global_to_local_dpt_v2(cpu, p, &local_util_cpu_avg, &local_util_coef1_avg, &local_util_coef2_avg, &local_util_cpu_est, &local_util_coef1_est, &local_util_coef2_est);
	 *	task_util = max(local_util_cpu_avg + local_util_coef1_avg + local_util_coef2_avg, local_util_cpu_est + local_util_coef1_est + local_util_coef2_est);
	 *}
	 *else
	 */
	task_util = task_util_est(p);
	if (!cpu_in_bcpus || !fits_capacity(task_util, util_Th, get_adaptive_margin(cpu)))
		return 0;

	return 1;
}

static inline bool is_target_max_spare_cpu(long spare_cap, long target_max_spare_cap,
			int best_cpu, int new_cpu, const char *type, unsigned long cpu_cap, unsigned long cpu_util)
{
	bool replace = true;

	if (spare_cap <= target_max_spare_cap)
		replace = false;

	if (trace_sched_target_max_spare_cpu_enabled())
		trace_sched_target_max_spare_cpu(type, best_cpu, new_cpu, replace,
			spare_cap, target_max_spare_cap, cpu_cap, cpu_util);

	return replace;
}

void init_gear_hints(void)
{
	gear_hints_enable = sched_gear_hints_enable_get();
}

void __set_gear_indices(struct task_struct *p, int gear_start, int num_gear, int reverse)
{
	struct task_gear_hints *ghts = &((struct mtk_task *)android_task_vendor_data(p))->gear_hints;

	ghts->gear_start = gear_start;
	ghts->num_gear   = num_gear;
	ghts->reverse    = reverse;
}

int set_gear_indices(int pid, int gear_start, int num_gear, int reverse)
{
	struct task_struct *p;
	int ret = 0;

	/* check feature is enabled */
	if (!gear_hints_enable)
		goto done;

	/* check gear_start validity */
	if (gear_start < -1 || gear_start > num_sched_clusters-1)
		goto done;

	/* check num_gear validity */
	if ((num_gear != -1 && num_gear < 1) || num_gear > num_sched_clusters)
		goto done;

	/* check num_gear validity */
	if (reverse < 0 || reverse > 1)
		goto done;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		__set_gear_indices(p, gear_start, num_gear, reverse);
		put_task_struct(p);
		ret = 1;
	}

	rcu_read_unlock();

done:
	return ret;
}
EXPORT_SYMBOL_GPL(set_gear_indices);

int unset_gear_indices(int pid)
{
	struct task_struct *p;
	int ret = 0;

	/* check feature is enabled */
	if (!gear_hints_enable)
		goto done;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		__set_gear_indices(p, GEAR_HINT_UNSET, GEAR_HINT_UNSET, 0);
		put_task_struct(p);
		ret = 1;
	}
	rcu_read_unlock();

done:
	return ret;
}
EXPORT_SYMBOL_GPL(unset_gear_indices);

void __get_gear_indices(struct task_struct *p, int *gear_start, int *num_gear, int *reverse)
{
	struct task_gear_hints *ghts = &((struct mtk_task *)android_task_vendor_data(p))->gear_hints;

	*gear_start = ghts->gear_start;
	*num_gear = ghts->num_gear;
	*reverse = ghts->reverse;
}

int get_gear_indices(int pid, int *gear_start, int *num_gear, int *reverse)
{
	struct task_struct *p;
	int ret = 0;

	/* check feature is enabled */
	if (!gear_hints_enable)
		goto done;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (p) {
		get_task_struct(p);
		__get_gear_indices(p, gear_start, num_gear, reverse);
		put_task_struct(p);
		ret = 1;
	}
	rcu_read_unlock();

done:
	return ret;
}
EXPORT_SYMBOL_GPL(get_gear_indices);

#if IS_ENABLED(CONFIG_MTK_SCHED_UPDOWN_MIGRATE)

bool updown_migration_enable;
void init_updown_migration(void)
{
	updown_migration_enable = sched_updown_migration_enable_get();
}

/* Default Migration margin */
bool adaptive_margin_enabled[MAX_NR_CPUS] = {
			[0 ... MAX_NR_CPUS-1] = true /* default on */
};
unsigned int sched_capacity_up_margin[MAX_NR_CPUS] = {
			[0 ... MAX_NR_CPUS-1] = 1024 /* ~0% margin */
};
unsigned int sched_capacity_down_margin[MAX_NR_CPUS] = {
			[0 ... MAX_NR_CPUS-1] = 1024 /* ~0% margin */
};

int set_updown_migration_pct(int gear_idx, int dn_pct, int up_pct)
{
	int ret = 0, cpu;
	struct cpumask *cpus;

	/* check feature is enabled */
	if (!updown_migration_enable)
		goto done;

	/* check gear_idx validity */
	if (gear_idx < 0 || gear_idx > num_sched_clusters-1)
		goto done;

	/* check pct validity */
	if (dn_pct < 1 || dn_pct > 100)
		goto done;

	if (up_pct < 1 || up_pct > 100)
		goto done;

	if (dn_pct > up_pct)
		goto done;

	cpus = get_gear_cpumask(gear_idx);
	for_each_cpu(cpu, cpus) {
		sched_capacity_up_margin[cpu] =
			SCHED_CAPACITY_SCALE * 100 / up_pct;
		sched_capacity_down_margin[cpu] =
			SCHED_CAPACITY_SCALE * 100 / dn_pct;
		adaptive_margin_enabled[cpu] = false;
	}
	ret = 1;

done:
	return ret;
}
EXPORT_SYMBOL_GPL(set_updown_migration_pct);

int unset_updown_migration_pct(int gear_idx)
{
	int ret = 0, cpu;
	struct cpumask *cpus;

	/* check feature is enabled */
	if (!updown_migration_enable)
		goto done;

	/* check gear_idx validity */
	if (gear_idx < 0 || gear_idx > num_sched_clusters-1)
		goto done;

	cpus = get_gear_cpumask(gear_idx);
	for_each_cpu(cpu, cpus) {
		sched_capacity_up_margin[cpu] = 1024;
		sched_capacity_down_margin[cpu] = 1024;
		adaptive_margin_enabled[cpu] = true;
	}
	ret = 1;

done:
	return ret;
}
EXPORT_SYMBOL_GPL(unset_updown_migration_pct);

int get_updown_migration_pct(int gear_idx, int *dn_pct, int *up_pct)
{
	int ret = 0, cpu;

	*dn_pct = *up_pct = -1;
	/* check feature is enabled */
	if (!updown_migration_enable)
		goto done;

	/* check gear_idx validity */
	if (gear_idx < 0 || gear_idx > num_sched_clusters-1)
		goto done;

	cpu = cpumask_first(get_gear_cpumask(gear_idx));

	*dn_pct = SCHED_CAPACITY_SCALE * 100 / sched_capacity_down_margin[cpu];
	*up_pct = SCHED_CAPACITY_SCALE * 100 / sched_capacity_up_margin[cpu];

	ret = 1;

done:
	return ret;
}
EXPORT_SYMBOL_GPL(get_updown_migration_pct);

static inline bool task_demand_fits(struct task_struct *p, int dst_cpu)
{
	int src_cpu = task_cpu(p);
	unsigned int margin;
	bool AM_enabled;
	unsigned int sugov_margin;
	unsigned long dst_capacity = arch_scale_cpu_capacity(dst_cpu);
	unsigned long src_capacity = arch_scale_cpu_capacity(src_cpu);
	unsigned long task_util;

	if (dst_capacity == SCHED_CAPACITY_SCALE)
		return true;

	/* if updown_migration is not enabled */
	if (!updown_migration_enable)
		return task_fits_capacity(p, dst_capacity, dst_cpu, get_adaptive_margin(dst_cpu));

	/*
	 * Derive upmigration/downmigration margin wrt the src/dst CPU.
	 */
	if (src_capacity > dst_capacity) {
		margin = sched_capacity_down_margin[dst_cpu];
		AM_enabled = adaptive_margin_enabled[dst_cpu];
	} else {
		margin = sched_capacity_up_margin[src_cpu];
		AM_enabled = adaptive_margin_enabled[src_cpu];
	}

	/* bypass adaptive margin if capacity_updown_margin is enabled */
	sugov_margin = AM_enabled ? get_adaptive_margin(dst_cpu) : SCHED_CAPACITY_SCALE;
	if (is_dpt_v2_support()) {
		int using_uclamp_freq = 0;

		task_util = uclamp_task_util_dpt_v2(p, dst_cpu, &using_uclamp_freq);
		sugov_margin = using_uclamp_freq ? NO_MARGIN : sugov_margin;
		dst_capacity = DPT_V2_MAX_RUNNING_TIME_LOCAL;
	}
	else
		task_util = uclamp_task_util(p);

	return (dst_capacity * SCHED_CAPACITY_SCALE * SCHED_CAPACITY_SCALE
				> task_util * margin * sugov_margin);
}

/* cpu_util could be before/after uclamped. cpu_util & coef1_util & coef2_util should be local scaled. */
inline int util_fits_capacity_dpt_v2(unsigned long cpu_util_without_uclamp_local, unsigned long coef1_util_local,
	unsigned long coef2_util_local, unsigned long uclamp_min, unsigned long uclamp_max, unsigned long local_capacity_of, int cpu, unsigned long *cpu_util_uclamped)
{
	bool AM_enabled = adaptive_margin_enabled[cpu];
	unsigned int sugov_margin = AM_enabled ? get_adaptive_margin(cpu) : SCHED_CAPACITY_SCALE;
	unsigned long __capacity, updown_ceiling = DPT_V2_MAX_RUNNING_TIME_LOCAL, cpu_util_prime;
	int fit, using_uclamp_freq = 0;

	uclamp_min = min(uclamp_min, uclamp_max);

	/* capacity: Up-down migration */
	if (updown_migration_enable && sched_capacity_up_margin[cpu] != 1024)
		updown_ceiling = updown_ceiling * pd_util2freq_r_o(cpu, sched_capacity_up_margin[cpu], false, 0) / get_cpu_max_freq(cpu);

	/* capacity: aware thermal & gear_umax & user freq ceiling. */
	__capacity = min(local_capacity_of, updown_ceiling);

	/* util: aware uclamp */
	*cpu_util_uclamped = dpt_v2_get_uclamped_cpu_util(cpu, uclamp_max, uclamp_min, cpu_util_without_uclamp_local, coef1_util_local, coef2_util_local,
		0, &using_uclamp_freq);
	sugov_margin = using_uclamp_freq ? NO_MARGIN : sugov_margin;

	/* util: add margin only to cpu_util */
	cpu_util_prime = *cpu_util_uclamped * sugov_margin >> SCHED_CAPACITY_SHIFT;

	fit = fits_capacity(cpu_util_prime + rescale_coef1_util_or_ratio_hook(coef1_util_local, RESCALE_UTIL) + rescale_coef2_util_or_ratio_hook(coef2_util_local, RESCALE_UTIL), __capacity, NO_MARGIN);

	if (trace_sched_util_fits_capacity_dpt_v2_enabled()) {
		unsigned long utils_for_debug[4];

		utils_for_debug[0] = cpu_util_without_uclamp_local;
		utils_for_debug[1] = *cpu_util_uclamped;
		utils_for_debug[2] = coef1_util_local;
		utils_for_debug[3] = coef2_util_local;
		trace_sched_util_fits_capacity_dpt_v2(cpu, fit, __capacity, sugov_margin, cpu_util_prime,
		uclamp_min, uclamp_max, utils_for_debug, updown_ceiling, local_capacity_of);
	}

	/* #TODO? */
	return fit;
}

/* util_fits_cpu: return fit status to check if util fits capacity.
 * fit = 1 : fit when considering PELT & uclamp & thermal
 * fit = -1: fit when considering PELT, not fit if ulcamp min > cap influenced by thermal(capacity_orig_thermal)
 * fit = 0 : not fit when considering PELT
 *
 * util : util before considering uclamp.
 * capacity: expect this capacity is already thermal-awared.
 */
inline int util_fits_capacity(unsigned long util, unsigned long uclamp_min,
	unsigned long uclamp_max, unsigned long capacity, int cpu)
{
	unsigned long ceiling, cap_after_ceiling;
	bool AM_enabled = adaptive_margin_enabled[cpu];
	unsigned int sugov_margin = AM_enabled ? get_adaptive_margin(cpu) : SCHED_CAPACITY_SCALE;
	unsigned long capacity_orig = arch_scale_cpu_capacity(cpu);
	int fit, uclamp_max_fits;

	/* ceiling shouldn't affect capacity since updown_migration is not enabled,  */
	if (!updown_migration_enable)
		ceiling = SCHED_CAPACITY_SCALE;
	else
		ceiling = SCHED_CAPACITY_SCALE * arch_scale_cpu_capacity(cpu) / sched_capacity_up_margin[cpu];

	/* Whether PELT fit after considering up-down migration ? */
	cap_after_ceiling = min(ceiling, capacity);
	fit = fits_capacity(util, cap_after_ceiling, sugov_margin);

	/* Change fit status from 0 to 1 only if uclamp max restrict util. */
	uclamp_max_fits = (capacity_orig == SCHED_CAPACITY_SCALE) && (uclamp_max == SCHED_CAPACITY_SCALE);
	uclamp_max_fits = !uclamp_max_fits && (uclamp_max <= capacity_orig);
	fit = fit || uclamp_max_fits;

	/* Change fit status from 1 to -1 only if uclamp min raise util. */
	uclamp_min = min(uclamp_min, uclamp_max);
	if (fit && (util < uclamp_min) && (uclamp_min > mtk_get_actual_cpu_capacity(cpu)))
		fit = -1;

	if (trace_sched_fits_cap_ceiling_enabled())
		trace_sched_fits_cap_ceiling(fit, cpu, util, uclamp_min, uclamp_max, capacity, ceiling, sugov_margin,
			sched_capacity_down_margin[cpu], sched_capacity_up_margin[cpu], AM_enabled);

	return fit;
}

#else
static inline bool task_demand_fits(struct task_struct *p, int cpu)
{
	unsigned long capacity = arch_scale_cpu_capacity(cpu);

	if (capacity == SCHED_CAPACITY_SCALE)
		return true;

	return task_fits_capacity(p, capacity, cpu, get_adaptive_margin(cpu));
}

inline int util_fits_capacity(unsigned long util, unsigned long uclamp_min,
	unsigned long uclamp_max, unsigned long capacity, int cpu)
{
	bool AM_enabled = adaptive_margin_enabled[cpu];
	unsigned int sugov_margin = AM_enabled ? get_adaptive_margin(cpu) : SCHED_CAPACITY_SCALE;
	unsigned long capacity_orig = arch_scale_cpu_capacity(cpu);
	int fit, uclamp_max_fits;

	/* Whether PELT fit after considering up-down migration ? */
	fit = fits_capacity(util, capacity, sugov_margin);

	/* Change fit status from 0 to 1 only if uclamp max restrict util. */
	uclamp_max_fits = (capacity_orig == SCHED_CAPACITY_SCALE) && (uclamp_max == SCHED_CAPACITY_SCALE);
	uclamp_max_fits = !uclamp_max_fits && (uclamp_max <= capacity_orig);
	fit = fit || uclamp_max_fits;

	/* Change fit status from 1 to -1 only if uclamp min raise util. */
	uclamp_min = min(uclamp_min, uclamp_max);
	if (fit && (util < uclamp_min) && (uclamp_min > mtk_get_actual_cpu_capacity(cpu)))
		fit = -1;

	return fit;
}
#endif /* CONFIG_MTK_SCHED_UPDOWN_MIGRATE */

/*default value: gear_start = -1, num_gear = -1, reverse = 0 */
static inline bool gear_hints_unset(struct task_gear_hints *ghts)
{
	if (ghts->gear_start >= 0)
		return false;

	if (ghts->num_gear > 0 && ghts->num_gear <= num_sched_clusters)
		return false;

	if (ghts->reverse)
		return false;

	return true;
}

void mtk_get_gear_indicies(struct task_struct *p, int *order_index, int *end_index,
		int *reverse, bool latency_sensitive)
{
	int i = 0;
	struct task_gear_hints *ghts = &((struct mtk_task *)android_task_vendor_data(p))->gear_hints;
	int max_num_gear = -1;
	bool gathering = false;

	*order_index = 0;
	*end_index = 0;
	*reverse = 0;
	if (num_sched_clusters <= 1)
		goto out;

	/* gear_start's range -1~num_sched_clusters */
	if (ghts->gear_start > num_sched_clusters || ghts->gear_start < -1)
		goto out;
#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
	/* group based prefer MCPU */
	if (gear_hints_enable && gear_hints_unset(ghts) && group_get_gear_hint(p)) {
		*order_index = 1;
		*end_index = 0;
		*reverse = 1;
		goto out;
	}
#endif // CONFIG_MTK_SCHED_FAST_LOAD_TRACKING
	/* task has customized gear prefer */
	if (gear_hints_enable && ghts->gear_start >= 0) {
		*order_index = ghts->gear_start;
	} else {
		for (i = *order_index; i < num_sched_clusters - 1; i++) {
			if (task_demand_fits(p, cpumask_first(&cpu_array[i][0][0])))
				break;
		}

		*order_index = i;
	}

	if (gear_hints_enable && ghts->reverse)
		max_num_gear = *order_index + 1;
	else
		max_num_gear = num_sched_clusters - *order_index;

	if (gear_hints_enable && ghts->num_gear > 0 && ghts->num_gear <= max_num_gear)
		*end_index = ghts->num_gear - 1;
	else {
		*end_index = max_num_gear - 1;

		if (gear_hints_enable && !latency_sensitive)
			gathering = gather_to_gear(p, end_index);
	}

	if (gear_hints_enable)
		*reverse     = ghts->reverse;

out:
	if (trace_sched_get_gear_indices_enabled()) {
		trace_sched_get_gear_indices(p, uclamp_task_util(p), gear_hints_enable,
				ghts->gear_start, ghts->num_gear, ghts->reverse,
				num_sched_clusters, max_num_gear, *order_index, *end_index, *reverse, gathering);
	}
}

inline void init_val_s(struct energy_env *eenv)
{
	int collab_type = 0;

	if (is_dpt_support_driver_hook == NULL)
		return;

	else if (!is_dpt_support_driver_hook())
		return;

	for (collab_type = 0; collab_type < get_nr_collab_type(); collab_type++)
		eenv->val_s[collab_type] = curr_collab_state[collab_type].state;
}
#define dpt_v2_util_for_freq(total_util_global, cpu_util_local, total_util_local) ((total_util_global) * (cpu_util_local) / (total_util_local))

inline long dpt_v2_spare_cap(int cpu, struct task_struct *p, int dst_cpu,
	unsigned long *cpu_util_local, unsigned long *coef1_util_local, unsigned long *coef2_util_local,
	unsigned long *cpu_cap, unsigned long *cpu_util, unsigned long *total_util_global, struct energy_env *eenv)
{
	unsigned long total_util_local, cpu_util_ratio_shifted;
	long spare_cap;
	int shift_bit = 10;

	mtk_cpu_util_next_dpt_v2(cpu, p, dst_cpu, 0, cpu_util_local, coef1_util_local, coef2_util_local);

	total_util_local = (*cpu_util_local + *coef1_util_local + *coef2_util_local);
	*total_util_global = mtk_cpu_util_next(cpu, p, dst_cpu, 0);
	cpu_util_ratio_shifted = ((*cpu_util_local) << shift_bit) / total_util_local; /* For numbers of division reduction. */

	*cpu_cap = (((DPT_V2_MAX_RUNNING_TIME_LOCAL * eenv->dpt_v2_cap_params[cpu][0].IPC_scaling_factor) >>__get_scaling_factor_shift_bit()) * cpu_util_ratio_shifted) >> shift_bit;
	/* aware freq ceiling */
	*cpu_cap = (*cpu_cap) * get_freq_ceiling_ratio(cpu) >> FREQ_CEILING_RATIO_BIT;
	*cpu_util = *total_util_global * cpu_util_ratio_shifted >> shift_bit;
	spare_cap = *cpu_cap;
	lsub_positive(&spare_cap, *cpu_util);

	return spare_cap;
}
struct find_best_candidates_parameters {
	bool in_irq;
	bool latency_sensitive;
	int prev_cpu;
	int order_index;
	int end_index;
	int reverse;
	int shortcut;
	int fbc_reason;
	bool is_vip;
	int vip_prio;
	struct cpumask vip_candidate;
};

DEFINE_PER_CPU(cpumask_var_t, mtk_fbc_mask);

static void mtk_find_best_candidates(struct cpumask *candidates, struct task_struct *p,
		struct cpumask *effective_softmask, struct cpumask *allowed_cpu_mask,
		struct energy_env *eenv, struct find_best_candidates_parameters *fbc_params,
		int *sys_max_spare_cap_cpu, int *idle_max_spare_cap_cpu, unsigned long *cpu_utils)
{
	int cluster, cpu;
	struct cpumask *cpus = this_cpu_cpumask_var_ptr(mtk_fbc_mask);
	unsigned long target_cap = 0;
	unsigned long cpu_cap, cpu_util, cpu_cap_without_p, cpu_util_without_p;
	unsigned long cpu_util_without_uclamp, rq_uclamp_max, rq_uclamp_min;
	bool not_in_softmask;
	struct cpuidle_state *idle;
	long sys_max_spare_cap = LONG_MIN, idle_max_spare_cap = LONG_MIN;
	unsigned long min_cap = eenv->min_cap;
	unsigned long max_cap = eenv->max_cap;
	bool is_vvip = false;
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	int target_balance_cluster;
#endif // CONFIG_MTK_SCHED_VIP_TASK
	bool latency_sensitive = fbc_params->latency_sensitive;
	bool in_irq = fbc_params->in_irq;
	int prev_cpu = fbc_params->prev_cpu;
	int order_index = fbc_params->order_index;
	int end_index = fbc_params->end_index;
	int reverse = fbc_params->reverse;
	bool is_vip = fbc_params->is_vip;
	int vip_prio = fbc_params->vip_prio;
	struct cpumask vip_candidate = fbc_params->vip_candidate;
	int dpt_v2_support = eenv->dpt_v2_support;

	if (!latency_sensitive && !is_vip) {
		int compress_cpu = compress_to_cpu(p, &eenv->min_cap, &eenv->max_cap, order_index);

		if (compress_cpu >= 0) {
			fbc_params->fbc_reason = fbc_params->shortcut = LB_SHORTCUT_COMPRESS;
			cpumask_set_cpu(compress_cpu, candidates);

			goto done;
		}
	}

#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	is_vvip = prio_is_vip(vip_prio, VVIP);

	if (is_vvip && !cpumask_empty(&vip_candidate)) {
		target_balance_cluster = topology_cluster_id(cpumask_last(&vip_candidate));
		order_index = target_balance_cluster;
		end_index = 0;
	}
#endif // CONFIG_MTK_SCHED_VIP_TASK

	/* find best candidate */
	for (cluster = 0; cluster < num_sched_clusters; cluster++) {
		int fit;
		unsigned int uint_cpu;
		long spare_cap, spare_cap_without_p, pd_max_spare_cap = LONG_MIN;
		long pd_max_spare_cap_ls_idle = LONG_MIN;
		unsigned int pd_min_exit_lat = UINT_MAX;
		int pd_max_spare_cap_cpu = -1;
		int pd_max_spare_cap_cpu_ls_idle = -1;
		unsigned long dpt_v2_cpu_util_local, dpt_v2_coef1_util_local, dpt_v2_coef2_util_local, total_util_global;
		unsigned long dpt_v2_cpu_util_local_without_p, dpt_v2_coef1_util_local_without_p, dpt_v2_coef2_util_local_without_p, total_util_global_without_p;
#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
		int cpu_order[MAX_NR_CPUS]  ____cacheline_aligned, cnt, i;

#endif // CONFIG_MTK_THERMAL_AWARE_SCHEDULING

		cpumask_and(cpus, &cpu_array[order_index][cluster][reverse], cpu_active_mask);

		if (cpumask_empty(cpus))
			continue;

#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
		cnt = sort_thermal_headroom(cpus, cpu_order, in_irq);

		for (i = 0; i < cnt; i++) {
			cpu = cpu_order[i];
#else
		for_each_cpu(cpu, cpus) {
#endif // CONFIG_MTK_THERMAL_AWARE_SCHEDULING
			track_sched_cpu_util(p, cpu, min_cap, max_cap);

			if (!is_vip) {
				if (!cpumask_test_cpu(cpu, p->cpus_ptr))
					continue;

				if (cpu_paused(cpu))
					continue;

				if (cpu_high_irqload(cpu))
					continue;

				cpumask_set_cpu(cpu, allowed_cpu_mask);

				if (in_irq &&
					task_can_skip_this_cpu(p, min_cap, latency_sensitive, cpu, &bcpus, vip_prio, eenv))
					continue;

				if (cpu_rq(cpu)->rt.rt_nr_running >= 1 &&
							!rt_rq_throttled(&(cpu_rq(cpu)->rt)))
					continue;
			} else  {
				if (!cpumask_test_cpu(cpu, &vip_candidate))
					continue;
			}

			if (dpt_v2_support) {
				/* cpu_util 更精確來說是需要的capacity. */
				spare_cap = dpt_v2_spare_cap(cpu, p, cpu, &dpt_v2_cpu_util_local, &dpt_v2_coef1_util_local, &dpt_v2_coef2_util_local,
					&cpu_cap, &cpu_util, &total_util_global, eenv);

				spare_cap_without_p = dpt_v2_spare_cap(cpu, p, -1, &dpt_v2_cpu_util_local_without_p, &dpt_v2_coef1_util_local_without_p, &dpt_v2_coef2_util_local_without_p,
					&cpu_cap_without_p, &cpu_util_without_p, &total_util_global_without_p, eenv);

				if (trace_sched_dpt_v2_spare_capacity_enabled()) {
						int values_for_debug[4] = {eenv->dpt_v2_cap_params[cpu][0].IPC_scaling_factor, total_util_global, total_util_global_without_p, get_freq_ceiling_ratio(cpu)};

						trace_sched_dpt_v2_spare_capacity(cpu, spare_cap, cpu_cap, cpu_util, dpt_v2_cpu_util_local,
							(dpt_v2_cpu_util_local+dpt_v2_coef1_util_local+dpt_v2_coef2_util_local),
							spare_cap_without_p, cpu_cap_without_p, cpu_util_without_p, dpt_v2_cpu_util_local_without_p,
							(dpt_v2_cpu_util_local_without_p+dpt_v2_coef1_util_local_without_p+dpt_v2_coef2_util_local_without_p), values_for_debug);
				}
			} else {
				cpu_util = mtk_cpu_util_next(cpu, p, cpu, 0);
				cpu_util_without_uclamp = cpu_util;
				cpu_util_without_p = mtk_cpu_util_next(cpu, p, -1, 0);
				cpu_cap = capacity_of(cpu);
				spare_cap = cpu_cap;
				spare_cap_without_p = cpu_cap;
				lsub_positive(&spare_cap, cpu_util);
				lsub_positive(&spare_cap_without_p, cpu_util_without_p);
			}


			not_in_softmask = (latency_sensitive &&
						!cpumask_test_cpu(cpu, effective_softmask));
			if (not_in_softmask)
				continue;

			if (cpu == prev_cpu) /* spare cap may exceed 1024 when dptv2 turn on, yet resonable. */
					spare_cap += spare_cap >> 6;

			if (is_target_max_spare_cpu(spare_cap_without_p, sys_max_spare_cap,
				*sys_max_spare_cap_cpu, cpu, "sys_max_spare", cpu_cap, cpu_util_without_p)) {
					sys_max_spare_cap = spare_cap_without_p;
					*sys_max_spare_cap_cpu = cpu;
				}

			/*
			 * if there is no best idle cpu, then select max spare cap
			 * and idle cpu for latency_sensitive task to avoid runnable.
			 * Because this is just a backup option, we do not take care
			 * of exit latency.
			 */
			if (latency_sensitive && mtk_available_idle_cpu(cpu)) {
				if (is_target_max_spare_cpu(spare_cap_without_p, idle_max_spare_cap,
					*idle_max_spare_cap_cpu, cpu, "idle_max_spare", cpu_cap, cpu_util)) {
					idle_max_spare_cap = spare_cap_without_p;
					*idle_max_spare_cap_cpu = cpu;
				}
			}

			/*
			 * Skip CPUs that cannot satisfy the capacity request.
			 * IOW, placing the task there would make the CPU
			 * overutilized. Take uclamp into account to see how
			 * much capacity we can get out of the CPU; this is
			 * aligned with effective_cpu_util().
			 */

			cpu_util = mtk_uclamp_rq_util_with(cpu_rq(cpu), cpu_util, p,
							min_cap, max_cap, &rq_uclamp_min, &rq_uclamp_max, true);

			if (dpt_v2_support) {
				unsigned long cpu_util_uclamped; /* not use for now */

				fit = util_fits_capacity_dpt_v2(dpt_v2_cpu_util_local, dpt_v2_coef1_util_local, dpt_v2_coef2_util_local,
					rq_uclamp_min, rq_uclamp_max, dpt_v2_local_capacity_all_util_of(cpu), cpu, &cpu_util_uclamped);
			} else {
				uint_cpu = cpu;
				/* record pre-clamping cpu_util */
				cpu_utils[uint_cpu] = cpu_util;

				if (trace_sched_util_fits_cpu_enabled())
					trace_sched_util_fits_cpu(cpu, cpu_utils[uint_cpu], cpu_util, cpu_cap,
							min_cap, max_cap, cpu_rq(cpu));

				/* replace with post-clamping cpu_util */
				cpu_utils[uint_cpu] = cpu_util;

				fit = util_fits_capacity(cpu_util_without_uclamp, rq_uclamp_min, rq_uclamp_max, cpu_cap, cpu);
			}

			if (fit <= 0)
				continue;

			/*
			 * Find the CPU with the maximum spare capacity in
			 * the performance domain
			 */

			if (!latency_sensitive && is_target_max_spare_cpu(spare_cap, pd_max_spare_cap,
					pd_max_spare_cap_cpu, cpu, "pd_max_spare", cpu_cap, cpu_util)) {
				pd_max_spare_cap = spare_cap;
				pd_max_spare_cap_cpu = cpu;
			}

			if (!latency_sensitive)
				continue;

			if (mtk_available_idle_cpu(cpu)) {
				cpu_cap = arch_scale_cpu_capacity(cpu);
				idle = idle_get_state(cpu_rq(cpu));
				if (idle && idle->exit_latency > pd_min_exit_lat &&
						cpu_cap == target_cap)
					continue;

#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
				if (!in_irq && idle && idle->exit_latency == pd_min_exit_lat
						&& cpu_cap == target_cap)
					continue;
#endif // CONFIG_MTK_THERMAL_AWARE_SCHEDULING
				if (!is_target_max_spare_cpu(spare_cap, pd_max_spare_cap_ls_idle,
					pd_max_spare_cap_cpu_ls_idle, cpu, "pd_max_spare_is_idle", cpu_cap, cpu_util))
					continue;

				pd_min_exit_lat = idle ? idle->exit_latency : 0;

				pd_max_spare_cap_ls_idle = spare_cap;
				target_cap = cpu_cap;
				pd_max_spare_cap_cpu_ls_idle = cpu;
			}
		}

		if (pd_max_spare_cap_cpu_ls_idle != -1)
			cpumask_set_cpu(pd_max_spare_cap_cpu_ls_idle, candidates);
		else if (pd_max_spare_cap_cpu != -1)
			cpumask_set_cpu(pd_max_spare_cap_cpu, candidates);

		if ((cluster >= end_index) && (!cpumask_empty(candidates)))
			break;
		else if (is_vvip &&
			(*idle_max_spare_cap_cpu>=0 || *sys_max_spare_cap_cpu>=0)) {
			/*
			 * Don't calc energy if we found max spare cpu
			 * since we have search a target balance gear for VVIP
			 */
			break;
		}
	}

	if ((cluster > end_index) && !cpumask_empty(cpus))
		fbc_params->fbc_reason = LB_FAIL;

done:
	if (trace_sched_find_best_candidates_enabled())
		trace_sched_find_best_candidates(p, candidates, fbc_params->fbc_reason,
				order_index, end_index, is_vip, allowed_cpu_mask);
}

DEFINE_PER_CPU(cpumask_var_t, mtk_select_rq_mask);
static DEFINE_PER_CPU(cpumask_t, energy_cpus);

void mtk_find_energy_efficient_cpu(void *data, struct task_struct *p, int prev_cpu, int sync,
					int *new_cpu, int loom_select_reason)
{
	struct cpumask *cpus = this_cpu_cpumask_var_ptr(mtk_select_rq_mask);
	unsigned long best_delta = ULONG_MAX;
	int this_cpu = smp_processor_id();
	struct root_domain *rd = cpu_rq(this_cpu)->rd;
	int sys_max_spare_cap_cpu = -1;
	int idle_max_spare_cap_cpu = -1;
	bool latency_sensitive = false;
	int best_energy_cpu = -1;
	unsigned int cpu;
	struct perf_domain *pd;
	int select_reason = -1, backup_reason = 0;
	struct energy_env eenv;
	struct cpumask effective_softmask;
	bool in_irq = in_interrupt();
	struct cpumask allowed_cpu_mask;
	int order_index = 0, end_index = 0, weight = 0, reverse = 0;
	cpumask_t *candidates;
	struct find_best_candidates_parameters fbc_params;
	unsigned long cpu_utils[MAX_NR_CPUS] = {[0 ... MAX_NR_CPUS-1] = ULONG_MAX};
	int recent_used_cpu, target;
	bool is_vip = false;
	int vip_prio = NOT_VIP;
	struct cpumask vip_candidate;
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	struct vip_task_struct *vts = &((struct mtk_static_vendor_task *)p->android_vendor_data1)->vip_task;

	vip_prio = get_vip_task_prio(p);
	is_vip = prio_is_vip(vip_prio, NOT_VIP);
	if (vts->faster_compute_eng) {
		in_irq = true;
		vts->faster_compute_eng = false;
	}
#endif // CONFIG_MTK_SCHED_VIP_TASK

	if (!get_eas_hook())
		return;

	if (loom_select_reason != -1) {
		select_reason = loom_select_reason;
		goto done;
	}

	cpumask_clear(&allowed_cpu_mask);

	irq_log_store();

	rcu_read_lock();
	compute_effective_softmask(p, &latency_sensitive, &effective_softmask);

	pd = rcu_dereference(rd->pd);
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	if (is_vip) {
		if (vip_in_gh)
			mtk_get_gear_indicies(p, &order_index, &end_index, &reverse, latency_sensitive);
		vip_candidate = find_min_num_vip_cpus(pd, p, vip_prio, &allowed_cpu_mask,
			order_index, end_index, reverse);
	}
#endif // CONFIG_MTK_SCHED_VIP_TASK
	if (!pd || READ_ONCE(rd->overutilized)) {
		select_reason = LB_FAIL;
		rcu_read_unlock();
		goto fail;
	}

	irq_log_store();

	if (sync && cpu_rq(this_cpu)->nr_running == 1 &&
	    cpumask_test_cpu(this_cpu, p->cpus_ptr) &&
	    task_fits_capacity(p, capacity_of(this_cpu), this_cpu, get_adaptive_margin(this_cpu)) &&
	    !(latency_sensitive && !cpumask_test_cpu(this_cpu, &effective_softmask))) {
		rcu_read_unlock();
		*new_cpu = this_cpu;
		select_reason = LB_SYNC;
		goto done;
	}

	irq_log_store();

	/* TODO? */
	if (!task_util_est(p)) {
		select_reason = LB_ZERO_UTIL;
		rcu_read_unlock();
		goto fail;
	}

	irq_log_store();

	mtk_get_gear_indicies(p, &order_index, &end_index, &reverse, latency_sensitive);

	eenv_init(&eenv, p, prev_cpu, pd, in_irq);
	if (!eenv.task_busy_time) {
		select_reason = LB_ZERO_EENV_UTIL;
		rcu_read_unlock();
		goto fail;
	}

	fbc_params.in_irq = in_irq;
	fbc_params.latency_sensitive = latency_sensitive;
	fbc_params.prev_cpu = prev_cpu;
	fbc_params.order_index = order_index;
	fbc_params.end_index = end_index;
	fbc_params.reverse = reverse;
	fbc_params.shortcut = 0;
	fbc_params.fbc_reason = 0;
	fbc_params.is_vip = is_vip;
	fbc_params.vip_prio = vip_prio;
	fbc_params.vip_candidate = vip_candidate;

	/* Pre-select a set of candidate CPUs. */
	candidates = this_cpu_ptr(&energy_cpus);
	cpumask_clear(candidates);

	mtk_find_best_candidates(candidates, p, &effective_softmask, &allowed_cpu_mask,
		&eenv, &fbc_params, &sys_max_spare_cap_cpu, &idle_max_spare_cap_cpu, cpu_utils);

	irq_log_store();

	/* select best energy cpu in candidates */
	weight = cpumask_weight(candidates);
	if (!weight)
		goto unlock;
	if (weight == 1) {
		best_energy_cpu = cpumask_first(candidates);
		goto unlock;
	}

	eenv_init_for_wl(&eenv);
	eenv_init_for_cap(&eenv, pd);
	eenv_init_for_cpu(&eenv, p, pd);
	if (!unlikely(in_irq)) {
		eenv_init_for_temp(&eenv);

		if (eenv.wl_support)
			eenv_init_for_dsu(&eenv);
	}
	init_val_s(&eenv);

	if (trace_sched_dbg_eenv_init_enabled())
		trace_sched_dbg_eenv_init(&eenv);

	for_each_cpu(cpu, candidates) {
		unsigned long cur_delta, base_energy;
		struct perf_domain *target_pd = rcu_dereference(pd);

		target_pd = find_pd(target_pd, cpu);
		if (!target_pd)
			continue;

		eenv.gear_idx = topology_cluster_id(cpu);
		cpus = to_cpumask(target_pd->em_pd->cpus);
		eenv.dst_cpu = cpu;

		/* Evaluate the energy impact of using this CPU. */
		if (unlikely(in_irq)) {
			unsigned long min = 0, max = 1024, max_util = 0;

			if (!eenv.dpt_v2_support)
				max_util = eenv_pd_max_util(&eenv, cpus, p, cpu, &min, &max);

			cur_delta = shared_buck_calc_pwr_eff(&eenv, cpu, p, max_util, cpus,
				is_dsu_pwr_triggered(eenv.wl_dsu), min, max, eenv.dpt_v2_support, eenv.dpt_v2_cap_params[cpu][1]);
			base_energy = 0;
		} else {
			eenv_pd_busy_time(&eenv, cpus, p);
			cur_delta = mtk_compute_energy(&eenv, target_pd, cpus, p,
								cpu, cpu);
			base_energy = mtk_compute_energy(&eenv, target_pd, cpus, p, -1, cpu);
		}
		cur_delta = max(cur_delta, base_energy) - base_energy;
		if (cur_delta < best_delta) {
			best_delta = cur_delta;
			best_energy_cpu = cpu;
		}
	}

unlock:
	rcu_read_unlock();

	irq_log_store();

	if (latency_sensitive) {
		if (best_energy_cpu >= 0) {
			*new_cpu = best_energy_cpu;
			select_reason = LB_LATENCY_SENSITIVE_BEST_IDLE_CPU | fbc_params.fbc_reason;
			goto done;
		}
		if (idle_max_spare_cap_cpu >= 0) {
			*new_cpu = idle_max_spare_cap_cpu;
			select_reason = LB_LATENCY_SENSITIVE_IDLE_MAX_SPARE_CPU;
			goto done;
		}
		if (sys_max_spare_cap_cpu >= 0) {
			*new_cpu = sys_max_spare_cap_cpu;
			select_reason = LB_LATENCY_SENSITIVE_MAX_SPARE_CPU;
			goto done;
		}
	}

	irq_log_store();

	/* All cpu failed on !fit_capacity, use sys_max_spare_cap_cpu */
	if (best_energy_cpu >= 0) {
		if (fbc_params.shortcut > 0 && fbc_params.fbc_reason == fbc_params.shortcut)
			select_reason = fbc_params.fbc_reason;
		else
			select_reason = LB_BEST_ENERGY_CPU | fbc_params.fbc_reason;

		*new_cpu = best_energy_cpu;

		goto done;
	}

	irq_log_store();

	if (sys_max_spare_cap_cpu >= 0) {
		*new_cpu = sys_max_spare_cap_cpu;
		select_reason = LB_MAX_SPARE_CPU;
		goto done;
	}

	irq_log_store();

/* The following fail path will be copied to the hook function of select_task_rq_fair
 * because overutilization won't enter fail path since kernel moved the condition about
 * overutilized from find_energy_efficient to select_task_rq_fair.
 * We have to copied the algo. to the hook function of select_task_rq_fair to enter
 * the fail path when overutilized triggerd.
 */

fail:
	/* All cpu failed, even sys_max_spare_cap_cpu is not captured*/

	/* from overutilized & zero_util */
	if (cpumask_empty(&allowed_cpu_mask)) {
		cpumask_andnot(&allowed_cpu_mask, p->cpus_ptr, cpu_pause_mask);
		cpumask_and(&allowed_cpu_mask, &allowed_cpu_mask, cpu_active_mask);
	} else {
		if (select_reason != LB_FAIL)
			select_reason = LB_FAIL_IN_REGULAR;
	}

	rcu_read_lock();

#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	if (is_vip) {
		struct cpumask temp_mask;

		/* for VVIP, select biggest CPU */
		if (prio_is_vip(vip_prio , VVIP) && !cpumask_empty(&vip_candidate)) {
			*new_cpu = cpumask_last(&vip_candidate);
			backup_reason = LB_BACKUP_VVIP;
			goto backup_unlock;
		}

		/* for other VIPs */
		cpumask_copy(&temp_mask, &allowed_cpu_mask);
		if (!cpumask_and(&allowed_cpu_mask, &allowed_cpu_mask, &vip_candidate))
			cpumask_copy(&allowed_cpu_mask, &temp_mask);
	}
#endif // CONFIG_MTK_SCHED_VIP_TASK

	if (cpumask_test_cpu(this_cpu, p->cpus_ptr) && (this_cpu != prev_cpu))
		target = mtk_wake_affine(p, this_cpu, prev_cpu, sync);
	else
		target = prev_cpu;

	if (cpumask_test_cpu(target, &allowed_cpu_mask) &&
		/* Don't check CPU idle state if task is VIP, since idle is not critical for VIP*/
	    (is_vip || (mtk_available_idle_cpu(target) || sched_idle_cpu(target))) &&
	    task_fits_capacity(p, capacity_of(target), target, get_adaptive_margin(target))) {
		*new_cpu = target;
		backup_reason = LB_BACKUP_AFFINE_IDLE_FIT;
		goto backup_unlock;
	}

	/*
	 * If the previous CPU fit_capacity and idle, don't be stupid:
	 */
	if (prev_cpu != target && mtk_cpus_share_cache(prev_cpu, target) &&
		cpumask_test_cpu(prev_cpu, &allowed_cpu_mask) &&
	    (is_vip || (mtk_available_idle_cpu(prev_cpu) || sched_idle_cpu(prev_cpu))) &&
	    task_fits_capacity(p, capacity_of(prev_cpu), prev_cpu, get_adaptive_margin(prev_cpu))) {
		*new_cpu = prev_cpu;
		backup_reason = LB_BACKUP_PREV;
		goto backup_unlock;
	}

	irq_log_store();

	/*
	 * Allow a per-cpu kthread to stack with the wakee if the
	 * kworker thread and the tasks previous CPUs are the same.
	 * The assumption is that the wakee queued work for the
	 * per-cpu kthread that is now complete and the wakeup is
	 * essentially a sync wakeup. An obvious example of this
	 * pattern is IO completions.
	 */
	if (cpumask_test_cpu(this_cpu, &allowed_cpu_mask) &&
		is_per_cpu_kthread(current) && in_task() &&
	    prev_cpu == this_cpu &&
	    this_rq()->nr_running <= 1 &&
	    task_fits_capacity(p, capacity_of(this_cpu), this_cpu, get_adaptive_margin(this_cpu))) {
		*new_cpu = this_cpu;
		backup_reason = LB_BACKUP_CURR;
		goto backup_unlock;
	}

	irq_log_store();

	/* Check a recently used CPU as a potential idle candidate: */
	recent_used_cpu = p->recent_used_cpu;
	p->recent_used_cpu = prev_cpu;
	if (recent_used_cpu != prev_cpu && recent_used_cpu != target &&
		mtk_cpus_share_cache(recent_used_cpu, target) &&
		cpumask_test_cpu(recent_used_cpu, &allowed_cpu_mask) &&
	    (is_vip || (mtk_available_idle_cpu(recent_used_cpu) || sched_idle_cpu(recent_used_cpu))) &&
	    task_fits_capacity(p, capacity_of(recent_used_cpu), recent_used_cpu, get_adaptive_margin(recent_used_cpu))) {
		*new_cpu = recent_used_cpu;
		/*
		 * Replace recent_used_cpu
		 * candidate for the next
		 */
		p->recent_used_cpu = prev_cpu;
		backup_reason = LB_BACKUP_RECENT_USED_CPU;
		goto backup_unlock;
	}

	irq_log_store();

	*new_cpu = mtk_select_idle_capacity(p, &allowed_cpu_mask, target, is_vip, &eenv);
	if (*new_cpu < nr_cpumask_bits) {
		backup_reason = LB_BACKUP_IDLE_CAP;
		goto backup_unlock;
	}

#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	if (is_vip) {
		*new_cpu = find_vip_backup_cpu(p, &allowed_cpu_mask, prev_cpu, target);
		if (*new_cpu < nr_cpumask_bits) {
			backup_reason = LB_BACKUP_VIP_IN_MASK;
			goto backup_unlock;
		}
	}
#endif // CONFIG_MTK_SCHED_VIP_TASK

	if (*new_cpu == -1 && cpumask_test_cpu(target, p->cpus_ptr)) {
		*new_cpu = target;
		backup_reason = LB_BACKUP_AFFINE_WITHOUT_IDLE_CAP;
		goto backup_unlock;
	}

	*new_cpu = -1;
backup_unlock:
	rcu_read_unlock();

done:
	irq_log_store();

	if (trace_sched_find_energy_efficient_cpu_enabled())
		trace_sched_find_energy_efficient_cpu(in_irq, best_delta, best_energy_cpu,
				best_energy_cpu, idle_max_spare_cap_cpu, sys_max_spare_cap_cpu);
	if (trace_sched_select_task_rq_enabled()) {
		struct trace_info_sched_select_task_rq info = {
			.pid                   = p->pid,
			.compat_thread         = is_compat_thread(task_thread_info(p)),
			.in_irq                = in_irq,
			.policy                = select_reason,
			.backup_reason         = backup_reason,
			.prev_cpu              = prev_cpu,
			.target_cpu            = *new_cpu,
			.task_util             = task_util(p),
			.task_util_est         = task_util_est(p),
			.boost                 = uclamp_task_util(p),
			.task_cpu_util         = 0,
			.task_cpu_util_est     = 0,
			.task_coef1_util       = 0,
			.task_coef1_util_est   = 0,
			.task_coef2_util       = 0,
			.task_coef2_util_est   = 0,
			.vip_prio              = vip_prio,
			.task_mask             = p->cpus_ptr->bits[0],
			.effective_softmask    = effective_softmask.bits[0],
			.latency_sensitive     = latency_sensitive,
			.sync_flag             = sync,
			.cpuctl_grp_id         = sched_cgroup_state(p, cpu_cgrp_id),
			.cpuset_grp_id         = sched_cgroup_state(p, cpuset_cgrp_id),
			.nr_candidates         = weight,
		};

		if (eenv.dpt_v2_support) {
			info.task_cpu_util       = task_util_dpt_v2(p, CPU_UTIL);
			info.task_cpu_util_est   = task_util_est_dpt_v2(p, CPU_UTIL);
			info.task_coef1_util     = task_util_dpt_v2(p, COEF1_UTIL);
			info.task_coef1_util_est = task_util_est_dpt_v2(p, COEF1_UTIL);
			info.task_coef2_util     = task_util_dpt_v2(p, COEF2_UTIL);
			info.task_coef2_util_est = task_util_est_dpt_v2(p, COEF2_UTIL);
		}

		trace_sched_select_task_rq(&info);
	}
	if (trace_sched_effective_mask_enabled()) {
		struct soft_affinity_task *sa_task = &((struct mtk_task *)
			android_task_vendor_data(p))->sa_task;
		struct cgroup_subsys_state *css = task_css(p, cpu_cgrp_id);
		struct cpumask softmask;

		if (css) {
			struct task_group *tg = container_of(css, struct task_group, css);
			struct soft_affinity_tg *sa_tg = &((struct mtk_tg *)
				tg->android_vendor_data1)->sa_tg;
			softmask = sa_tg->soft_cpumask;
		} else {
			cpumask_clear(&softmask);
			cpumask_copy(&softmask, cpu_possible_mask);
		}
		trace_sched_effective_mask(p, *new_cpu, latency_sensitive,
			&effective_softmask, &sa_task->soft_cpumask, &softmask);
	}

	irq_log_store();
}
#endif // CONFIG_MTK_EAS
#endif // CONFIG_SMP

static inline u32 get_pelt_divider_cfs_dpt_v2(int cpu)
{
	return PELT_MIN_DIVIDER + per_cpu(__dpt_rq, cpu).local_cfs_period_contrib;
}

static inline u32 get_pelt_divider_rt_dpt_v2(int cpu)
{
	return PELT_MIN_DIVIDER + per_cpu(__dpt_rq, cpu).local_rt_period_contrib;
}

/* Cloned from kernel/sched/pelt.c: decay_load() */
static u64 mtk_decay_load(u64 val, u64 n)
{
	unsigned int local_n;

	if (unlikely(n > LOAD_AVG_PERIOD * 63))
		return 0;

	local_n = n;

	if (unlikely(local_n >= LOAD_AVG_PERIOD)) {
		val >>= local_n / LOAD_AVG_PERIOD;
		local_n %= LOAD_AVG_PERIOD;
	}

	val = mul_u64_u32_shr(val, runnable_avg_yN_inv[local_n], 32);
	return val;
}

/* Cloned from kernel/sched/pelt.c: __accumulate_pelt_segments() */
static u32 mtk_accumulate_pelt_segments(u64 periods, u32 d1, u32 d3)
{
	u32 c1, c2, c3 = d3;

	c1 = mtk_decay_load((u64)d1, periods);
	c2 = LOAD_AVG_MAX - mtk_decay_load(LOAD_AVG_MAX, periods) - 1024;

	return c1 + c2 + c3;
}

/* Cloned from kernel/sched/pelt.c: accumulate_sum() */
static __always_inline u32
mtk_accumulate_sum(int cpu, u64 delta, u64 dpt_delta, u32 *local_period_contrib,
		struct sched_avg *sa, unsigned long load, unsigned long runnable, int running,
		unsigned int global_clock_cpu_ratio, unsigned int global_clock_sratio, unsigned int global_clock_coef2_ratio,
		unsigned int local_clock_cpu_ratio, unsigned int local_clock_sratio, unsigned int local_clock_coef2_ratio,
		struct dpt_task_struct *util, int pelt_type)
{
	u64 periods = 0, dpt_periods = 0;

	/*
	* If delta has a value, it indicates that need to be processed the periods,
	* including the remaining amount that doesn't complete a full period.
	*/
	if (util && dpt_delta)
	{
		u32 dpt_contrib = (u32)dpt_delta;
		u32 dpt_period_contrib = local_period_contrib ? *local_period_contrib : sa->period_contrib;
		unsigned int clock_cpu_ratio = local_period_contrib ? local_clock_cpu_ratio : global_clock_cpu_ratio;
		unsigned int clock_sratio = local_period_contrib ? local_clock_sratio : global_clock_sratio;
		unsigned int clock_coef2_ratio = local_period_contrib ? local_clock_coef2_ratio : global_clock_coef2_ratio;

		dpt_delta += dpt_period_contrib;
		dpt_periods = dpt_delta / 1024; /* A period is 1024us (~1ms) */

		/* Step 1: decay old *_sum if we crossed period boundaries. */
		if (dpt_periods)
		{
			util->util_cpu_sum = mtk_decay_load((u64)(util->util_cpu_sum), dpt_periods);
			util->util_coef1_sum = mtk_decay_load((u64)(util->util_coef1_sum), dpt_periods);
			util->util_coef2_sum = mtk_decay_load((u64)(util->util_coef2_sum), dpt_periods);

			/* Step 2 */
			dpt_delta %= 1024;
			if (load)
				dpt_contrib = mtk_accumulate_pelt_segments(dpt_periods, 1024 - dpt_period_contrib, dpt_delta);
		}
		if (local_period_contrib)
			*local_period_contrib = dpt_delta;

		if (running)
		{
			util->util_cpu_sum += (dpt_contrib * clock_cpu_ratio);
			util->util_coef1_sum += (dpt_contrib * clock_sratio);
			util->util_coef2_sum += (dpt_contrib * clock_coef2_ratio);
		}
	}

	/*
	* If delta has a value, it indicates that need to be processed the periods,
	* including the remaining amount that doesn't complete a full period.
	*/
	if(delta)
	{
		u32 contrib = (u32)delta;
		delta += sa->period_contrib;
		periods  = delta / 1024; /* A period is 1024us (~1ms) */

		/* Step 1: decay old *_sum if we crossed period boundaries. */
		if (periods) {
			sa->load_sum = mtk_decay_load(sa->load_sum, periods);
			sa->runnable_sum = mtk_decay_load(sa->runnable_sum, periods);
			sa->util_sum = mtk_decay_load((u64)(sa->util_sum), periods);

			/* Step 2 */
			delta %= 1024;
			if (load)
				contrib = mtk_accumulate_pelt_segments(periods, 1024 - sa->period_contrib, delta);
		}
		sa->period_contrib = delta;

		if (load)
			sa->load_sum += load * contrib;
		if (runnable)
			sa->runnable_sum += runnable * contrib << SCHED_CAPACITY_SHIFT;
		if (running)
			sa->util_sum += contrib << SCHED_CAPACITY_SHIFT;
	}

	return periods ? periods : dpt_periods;
}

/* Cloned from kernel/sched/pelt.c: __update_load_sum() */
static int mtk_update_load_sum(int cpu, u64 now, u64 dpt_now, u64 *local_last_update_time, u32 *local_period_contrib,
		struct sched_avg *sa,
		unsigned long load, unsigned long runnable, int running,
		unsigned int global_clock_cpu_ratio, unsigned int global_clock_sratio, unsigned int global_clock_coef2_ratio,
		unsigned int local_clock_cpu_ratio, unsigned int local_clock_sratio, unsigned int local_clock_coef2_ratio,
		struct dpt_task_struct *util, int pelt_type)
{
	u64 delta, dpt_delta;
	delta = now - sa->last_update_time;
	dpt_delta = dpt_now - (local_last_update_time ? *local_last_update_time : sa->last_update_time);

	if (unlikely((s64)delta < 0))
	{
		sa->last_update_time = now;
		delta = 0;
	}
	else
	{
		delta >>= 10;
		if (delta)
			sa->last_update_time += delta << 10;
	}

	/*
	* Since we only need to update util of tasks and root rq,
	* we use whether util is NULL to filter out task groups and
	* non-root cfs rq that do not require load tracking.
	*/
	if(util)
	{
		/*
		* Determine the util of tasks or rq based on whether local_last_update_time is NULL.
		* Because the dpt structure of task represents global util,
		* whereas the dpt structure of rq represents local util.
		*/

		if (unlikely((s64)dpt_delta < 0))
		{
			if (local_last_update_time)
				*local_last_update_time = dpt_now;
			dpt_delta = 0;
		}
		else
		{
			dpt_delta >>= 10;
			if (dpt_delta && local_last_update_time)
				*local_last_update_time += dpt_delta << 10;
		}
	}

	if (!delta && !dpt_delta)
		return 0;

	if (!load)
		runnable = running = 0;

	/*
	* Calculate util, delta value of 0 indicates no need to update.
	*/
	if (!mtk_accumulate_sum(cpu, delta, dpt_delta, local_period_contrib,
	        sa, load, runnable, running,
			global_clock_cpu_ratio, global_clock_sratio, global_clock_coef2_ratio,
			local_clock_cpu_ratio, local_clock_sratio, local_clock_coef2_ratio ,
			util, pelt_type))
		return 0;

	return 1;
}

/* Cloned from kernel/sched/pelt.c: __update_load_avg() */
static void mtk_update_load_avg(int cpu, struct sched_avg *sa, unsigned long load, struct dpt_task_struct *util, int pelt_type)
{
	u32 divider = get_pelt_divider(sa);

	sa->load_avg = div_u64(load * sa->load_sum, divider);
	sa->runnable_avg = div_u64(sa->runnable_sum, divider);
	WRITE_ONCE(sa->util_avg, sa->util_sum / divider);

	switch(pelt_type)
	{
		case CFS:
			divider = get_pelt_divider_cfs_dpt_v2(cpu);
			break;
		case RT:
			divider = get_pelt_divider_rt_dpt_v2(cpu);
			break;
	}

	if (util)
	{
		if( !(util->util_cpu_avg) && sa->util_avg)
			WRITE_ONCE(util->util_cpu_sum, sa->util_sum);
		WRITE_ONCE(util->util_cpu_avg, util->util_cpu_sum / divider);
		WRITE_ONCE(util->util_coef1_avg, util->util_coef1_sum / divider);
		WRITE_ONCE(util->util_coef2_avg, util->util_coef2_sum / divider);
	}
}

/* Cloned from kernel/sched/pelt.h: cfs_se_util_change() */
static inline void mtk_cfs_se_util_change(struct sched_avg *avg, struct dpt_task_struct *util_task)
{
	unsigned int enqueued;

	if (!sched_feat(UTIL_EST))
		return;

	/* Avoid store if the flag has been already reset */
	enqueued = avg->util_est;
	if (!(enqueued & UTIL_AVG_UNCHANGED))
		return;

	/* Reset flag to report util_avg has been updated */
	enqueued &= ~UTIL_AVG_UNCHANGED;
	WRITE_ONCE(avg->util_est, enqueued);
}

/* Cloned from kernel/sched/pelt.h: _update_idle_rq_clock_pelt() */
void mtk_update_idle_rq_clock_pelt(struct rq *rq)
{
	rq->clock_pelt = rq_clock_task_mult(rq);

	u64_u32_store(rq->clock_idle, rq_clock(rq));
	/* Paired with smp_rmb in migrate_se_pelt_lag() */
	smp_wmb();
	u64_u32_store(rq->clock_pelt_idle, rq_clock_pelt(rq));
}

/* Hook from `trace_android_rvh_update_rq_clock_pelt` */
void mtk_update_rq_clock_pelt(void *unused, struct rq *rq, s64 delta, int *ret)
{
	int cpu;
	dpt_rq_t *dpt_rq = NULL;
	s64 global_cpu_delta, local_cpu_delta;
	s64 global_coef1_delta, local_coef1_delta;
	s64 global_coef2_delta, local_coef2_delta;
	u64 clock_diff = 0;

	if(!is_dpt_v2_support())
		return;

	if (unlikely(is_idle_task(rq->curr))) {
		mtk_update_idle_rq_clock_pelt(rq);
		return;
	}

	cpu = cpu_of(rq);
	dpt_rq = &per_cpu(__dpt_rq, cpu);
	local_cpu_delta = local_coef1_delta = local_coef2_delta = delta;

	/* Update CPU clocks */
	local_cpu_delta = cap_scale(local_cpu_delta, arch_scale_non_s_capacity_dpt_v2(cpu));
	local_cpu_delta = cap_scale(local_cpu_delta, arch_scale_freq_capacity(cpu));
	global_cpu_delta = cap_scale(local_cpu_delta, arch_scale_cpu_capacity_dpt_v2(cpu));

	/* Update coef1 s clock */
	local_coef1_delta = cap_scale(local_coef1_delta, arch_scale_coef1_s_capacity_dpt_v2(cpu));
	local_coef1_delta = cap_scale(local_coef1_delta, arch_scale_coef1_ltime_capacity_dpt_v2(cpu));
	global_coef1_delta = cap_scale(local_coef1_delta, arch_scale_coef1_capacity_dpt_v2(cpu));

	/* Update coef2 s clock */
	local_coef2_delta = cap_scale(local_coef2_delta, arch_scale_coef2_s_capacity_dpt_v2(cpu));
	local_coef2_delta = cap_scale(local_coef2_delta, arch_scale_coef2_ltime_capacity_dpt_v2(cpu));
	global_coef2_delta = cap_scale(local_coef2_delta, arch_scale_coef2_capacity_dpt_v2(cpu));

	/* Record individual components before aggregating into rq_clock;
	* The ratios will be used for update util when update_load_avg()*/
	dpt_rq->local_clock[NON_S]			+= local_cpu_delta;
	dpt_rq->local_clock[S_COEF1]			+= local_coef1_delta;
	dpt_rq->local_clock[S_COEF2]			+= local_coef2_delta;
	dpt_rq->global_clock[NON_S]			+= global_cpu_delta;
	dpt_rq->global_clock[S_COEF1]			+= global_coef1_delta;
	dpt_rq->global_clock[S_COEF2]			+= global_coef2_delta;

	/*
	* `clock_pelt` can also be synced when rq is idle,
	* increment here by the amount of time spent idling since last sync.
	*/
	if (rq->clock_pelt > dpt_rq->last_clock_pelt)
		clock_diff = rq->clock_pelt - dpt_rq->last_clock_pelt;

	/* Record current global and local rq_clock accumulations */
	dpt_rq->local_clock_pelt 	+= local_cpu_delta + local_coef1_delta + local_coef2_delta + clock_diff;
	rq->clock_pelt				+= global_cpu_delta + global_coef1_delta + global_coef2_delta;
	dpt_rq->last_clock_pelt 	= rq->clock_pelt;

	*ret = 1;

	if (trace_sched_update_rq_clock_pelt_enabled())
		trace_sched_update_rq_clock_pelt(cpu, rq->clock_pelt,
			local_cpu_delta, local_coef1_delta, local_coef2_delta,
			global_cpu_delta, global_coef1_delta, global_coef2_delta,
			dpt_rq, *ret);
}

/* Hook from `trace_android_rvh_update_load_avg_blocked_se`
 * Used when a task entity is about to migrate or sleep,
 * ensuring its utilization is updated to the latest state.
 */
void mtk_update_load_avg_blocked_se(void *unused, u64 now, struct sched_entity *se, int *ret)
{
	int cpu;
	pid_t pid = 0;
	dpt_rq_t *dpt_rq = NULL;
	struct task_struct *p = NULL;
	struct dpt_task_struct *util_task = NULL;

	if(!is_dpt_v2_support())
		return;

	cpu = cpu_of(rq_of(cfs_rq_of(se)));
	dpt_rq = &per_cpu(__dpt_rq, cpu);

	if (likely(entity_is_task(se)))
	{
		p = task_of(se);
		pid = task_pid_nr(p);
		util_task = &((struct mtk_task *) android_task_vendor_data(p))->dpt_task;
	}

	if (mtk_update_load_sum(cpu, now, now, 0, 0,
				&se->avg,
				0,
				0,
				0,
				dpt_rq->global_clock_ratio[NON_S],
				dpt_rq->global_clock_ratio[S_COEF1],
				dpt_rq->global_clock_ratio[S_COEF2],
				0,
				0,
				0,
				util_task, TASK))
	{
		mtk_update_load_avg(cpu, &se->avg, se_weight(se), util_task, TASK);
		trace_pelt_se_tp(se);
		*ret = 1;
	}
	*ret = 0;

	if (trace_sched_update_load_avg_blocked_se_enabled())
		trace_sched_update_load_avg_blocked_se(cpu, pid, &se->avg, util_task, dpt_rq);
}

/* Hook from `trace_android_rvh_update_load_avg_se` */
void mtk_update_load_avg_se(void *unused, u64 now, struct cfs_rq *cfs_rq, struct sched_entity *se, int *ret)
{
	int cpu;
	pid_t pid = 0;
	dpt_rq_t *dpt_rq = NULL;
	struct task_struct *p = NULL;
	struct dpt_task_struct *util_task = NULL;

	if(!is_dpt_v2_support())
		return;

	irq_log_store();

	cpu = cpu_of(rq_of(cfs_rq_of(se)));
	dpt_rq = &per_cpu(__dpt_rq, cpu);

	if (likely(entity_is_task(se)))
	{
		p = task_of(se);
		pid = task_pid_nr(p);
		util_task = &((struct mtk_task *) android_task_vendor_data(p))->dpt_task;
	}

	if (mtk_update_load_sum(cpu, now, now, 0, 0,
				&se->avg,
				!!se->on_rq,
				se_runnable(se),
				cfs_rq->curr == se,
				dpt_rq->global_clock_ratio[NON_S],
				dpt_rq->global_clock_ratio[S_COEF1],
				dpt_rq->global_clock_ratio[S_COEF2],
				0,
				0,
				0,
				util_task, TASK))
	{
		mtk_update_load_avg(cpu, &se->avg, se_weight(se), util_task, TASK);
		mtk_cfs_se_util_change(&se->avg, util_task);
		trace_pelt_se_tp(se);
		*ret = 1;
	}
	*ret = 0;

	irq_log_store();

	if (trace_sched_update_load_avg_se_enabled())
		trace_sched_update_load_avg_se(cpu, pid, &se->avg, util_task, dpt_rq);
}

/* Hook from `trace_android_rvh_update_load_avg_cfs_rq` */
void mtk_update_load_avg_cfs_rq(void *unused, u64 now, struct cfs_rq *cfs_rq, int *ret)
{
	int cpu;
	dpt_rq_t *dpt_rq = NULL;
	struct dpt_task_struct *util_cfs = NULL;
	struct rq *rq = NULL;

	if(!is_dpt_v2_support())
		return;

	irq_log_store();

	rq = rq_of(cfs_rq);
	cpu = cpu_of(rq);
	dpt_rq = &per_cpu(__dpt_rq, cpu);
	util_cfs = &dpt_rq->util_cfs;

	if (dpt_rq->removed_nr)
	{
		unsigned long removed_cpu_util = 0, removed_coef1_util = 0, removed_coef2_util = 0;
		u32 divider = get_pelt_divider_cfs_dpt_v2(cpu);

		raw_spin_lock(&cfs_rq->removed.lock);
		swap(dpt_rq->removed_util_cpu_avg, removed_cpu_util);
		swap(dpt_rq->removed_util_coef1_avg, removed_coef1_util);
		swap(dpt_rq->removed_util_coef2_avg, removed_coef2_util);
		dpt_rq->removed_nr = 0;
		raw_spin_unlock(&cfs_rq->removed.lock);

		sub_positive(&util_cfs->util_cpu_avg, removed_cpu_util);
		sub_positive(&util_cfs->util_coef1_avg, removed_coef1_util);
		sub_positive(&util_cfs->util_coef2_avg, removed_coef2_util);
		sub_positive(&util_cfs->util_cpu_sum, removed_cpu_util * divider);
		sub_positive(&util_cfs->util_coef1_sum, removed_coef1_util * divider);
		sub_positive(&util_cfs->util_coef2_sum, removed_coef2_util * divider);

		util_cfs->util_cpu_sum = max_t(u32, util_cfs->util_cpu_sum, util_cfs->util_cpu_avg * PELT_MIN_DIVIDER);
		util_cfs->util_coef1_sum = max_t(u32, util_cfs->util_coef1_sum, util_cfs->util_coef1_avg * PELT_MIN_DIVIDER);
		util_cfs->util_coef2_sum = max_t(u32, util_cfs->util_coef2_sum, util_cfs->util_coef2_avg * PELT_MIN_DIVIDER);
	}

	/* Update root cfs_rq's utilization */
	util_cfs = (&rq->cfs != cfs_rq) ? NULL : util_cfs;

	/*
	* `clock_pelt` can also be synced when rq is idle,
	* increment here by the amount of time spent idling since last sync.
	*/
	if (rq->clock_pelt > dpt_rq->last_clock_pelt)
	{
		dpt_rq->local_clock_pelt += (rq->clock_pelt - dpt_rq->last_clock_pelt);
		dpt_rq->last_clock_pelt = rq->clock_pelt;
	}

	if (mtk_update_load_sum(cpu, now, dpt_rq->local_clock_pelt, &dpt_rq->local_cfs_last_update_time, &dpt_rq->local_cfs_period_contrib,
				&cfs_rq->avg,
				scale_load_down(cfs_rq->load.weight),
				cfs_rq->h_nr_running,
				cfs_rq->curr != NULL,
				dpt_rq->global_clock_ratio[NON_S],
				dpt_rq->global_clock_ratio[S_COEF1],
				dpt_rq->global_clock_ratio[S_COEF2],
				dpt_rq->local_clock_ratio[NON_S],
				dpt_rq->local_clock_ratio[S_COEF1],
				dpt_rq->local_clock_ratio[S_COEF2],
				util_cfs, CFS))
	{
		mtk_update_load_avg(cpu, &cfs_rq->avg, 1, util_cfs, CFS);
		trace_pelt_cfs_tp(cfs_rq);
		*ret = 1;
	}
	*ret = 0;

	if (util_cfs) {
		util_cfs->util_cpu_avg += dpt_rq->util_cpu_avg_tmp;
		util_cfs->util_coef1_avg += dpt_rq->util_coef1_avg_tmp;
		util_cfs->util_coef2_avg += dpt_rq->util_coef2_avg_tmp;
		util_cfs->util_cpu_sum += dpt_rq->util_cpu_sum_tmp;
		util_cfs->util_coef1_sum += dpt_rq->util_coef1_sum_tmp;
		util_cfs->util_coef2_sum += dpt_rq->util_coef2_sum_tmp;
		dpt_rq->util_cpu_avg_tmp = 0;
		dpt_rq->util_coef1_avg_tmp = 0;
		dpt_rq->util_coef2_avg_tmp = 0;
		dpt_rq->util_cpu_sum_tmp = 0;
		dpt_rq->util_coef1_sum_tmp = 0;
		dpt_rq->util_coef2_sum_tmp = 0;
	}

	irq_log_store();

	if (trace_sched_update_load_avg_cfs_rq_enabled() && util_cfs)
		trace_sched_update_load_avg_cfs_rq(cpu, &cfs_rq->avg, util_cfs, dpt_rq);
}

/* Hook from `trace_android_rvh_update_rt_rq_load_avg_internal` */
void mtk_update_rt_rq_load_avg_internal(void *unused, u64 now, struct rq *rq, int running, int *ret)
{
	int cpu;
	dpt_rq_t *dpt_rq = NULL;
	struct dpt_task_struct *util_rt = NULL;

	if(!is_dpt_v2_support())
		return;

	cpu = cpu_of(rq);
	dpt_rq = &per_cpu(__dpt_rq, cpu);
	util_rt = &dpt_rq->util_rt;

	/* Record current global and local rq_clock accumulations */
	if (rq->clock_pelt > dpt_rq->last_clock_pelt)
	{
		dpt_rq->local_clock_pelt += (rq->clock_pelt - dpt_rq->last_clock_pelt);
		dpt_rq->last_clock_pelt = rq->clock_pelt;
	}

	if (mtk_update_load_sum(cpu, now, dpt_rq->local_clock_pelt, &dpt_rq->local_rt_last_update_time, &dpt_rq->local_rt_period_contrib,
				&rq->avg_rt,
				running,
				running,
				running,
				dpt_rq->global_clock_ratio[NON_S],
				dpt_rq->global_clock_ratio[S_COEF1],
				dpt_rq->global_clock_ratio[S_COEF2],
				dpt_rq->local_clock_ratio[NON_S],
				dpt_rq->local_clock_ratio[S_COEF1],
				dpt_rq->local_clock_ratio[S_COEF2],
				util_rt, RT))
	{
		mtk_update_load_avg(cpu, &rq->avg_rt, 1, util_rt, RT);
		trace_pelt_rt_tp(rq);
		*ret = 1;
	}
	*ret = 0;

	if (trace_sched_update_rt_rq_load_avg_internal_enabled())
		trace_sched_update_rt_rq_load_avg_internal(cpu, &rq->avg_rt, util_rt, dpt_rq);
}

/* Hook from `trace_android_rvh_attach_entity_load_avg` */
void mtk_attach_entity_load_avg(void *unused, struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	int cpu;
	u32 divider;
	unsigned long local_util_cpu_avg, local_util_coef1_avg, local_util_coef2_avg;
	struct dpt_rq_struct *dpt_rq = NULL;
	struct dpt_task_struct *util_task = NULL;
	struct task_struct *p = NULL;
	struct rq *rq = NULL;

	if(!is_dpt_v2_support())
		return;

	if (unlikely(!entity_is_task(se)))
		return;

	irq_log_store();

	rq = rq_of(cfs_rq);
	cpu = cpu_of(rq);
	p = task_of(se);
	util_task = &((struct mtk_task *) android_task_vendor_data(p))->dpt_task;
	dpt_rq = &per_cpu(__dpt_rq, cpu);
	divider = get_pelt_divider(&rq->cfs.avg);

	util_task->util_cpu_sum = task_util_dpt_v2(p, CPU_UTIL) * divider;
	util_task->util_coef1_sum = task_util_dpt_v2(p, COEF1_UTIL) * divider;
	util_task->util_coef2_sum = task_util_dpt_v2(p, COEF2_UTIL) * divider;

	task_global_to_local_dpt_v2(cpu, p, &local_util_cpu_avg, &local_util_coef1_avg, &local_util_coef2_avg, NULL, NULL, NULL);
	divider = get_pelt_divider_cfs_dpt_v2(cpu);

	if (trace_sched_attach_entity_load_avg_enabled())
		trace_sched_attach_entity_load_avg(cpu, task_pid_nr(p), dpt_rq, util_task);

	dpt_rq->util_cpu_avg_tmp += local_util_cpu_avg;
	dpt_rq->util_coef1_avg_tmp += local_util_coef1_avg;
	dpt_rq->util_coef2_avg_tmp += local_util_coef2_avg;
	dpt_rq->util_cpu_sum_tmp += local_util_cpu_avg * divider;
	dpt_rq->util_coef1_sum_tmp += local_util_coef1_avg * divider;
	dpt_rq->util_coef2_sum_tmp += local_util_coef2_avg * divider;

	irq_log_store();

	if (trace_sched_attach_entity_load_avg_enabled())
		trace_sched_attach_entity_load_avg(cpu, task_pid_nr(p), dpt_rq, util_task);
}

/* Hook from `trace_android_rvh_detach_entity_load_avg` */
void mtk_detach_entity_load_avg(void *unused, struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	int cpu;
	u32 divider;
	unsigned long local_util_cpu_avg, local_util_coef1_avg, local_util_coef2_avg;
	struct dpt_rq_struct *dpt_rq = NULL;
	struct dpt_task_struct *util_task = NULL;
	struct task_struct *p = NULL;
	struct rq *rq = NULL;

	if(!is_dpt_v2_support())
		return;

	if (unlikely(!entity_is_task(se)))
		return;

	rq = rq_of(cfs_rq);
	cpu = cpu_of(rq);
	p = task_of(se);
	util_task = &((struct mtk_task *) android_task_vendor_data(p))->dpt_task;
	dpt_rq = &per_cpu(__dpt_rq, cpu);

	task_global_to_local_dpt_v2(cpu, p, &local_util_cpu_avg, &local_util_coef1_avg, &local_util_coef2_avg, NULL, NULL, NULL);
	divider = get_pelt_divider_cfs_dpt_v2(cpu);

	if (trace_sched_detach_entity_load_avg_enabled())
		trace_sched_detach_entity_load_avg(cpu, task_pid_nr(p), dpt_rq, util_task);

	sub_positive(&dpt_rq->util_cpu_avg_tmp, local_util_cpu_avg);
	sub_positive(&dpt_rq->util_coef1_avg_tmp, local_util_coef1_avg);
	sub_positive(&dpt_rq->util_coef2_avg_tmp, local_util_coef2_avg);
	sub_positive(&dpt_rq->util_cpu_sum_tmp, local_util_cpu_avg * divider);
	sub_positive(&dpt_rq->util_coef1_sum_tmp, local_util_coef1_avg * divider);
	sub_positive(&dpt_rq->util_coef2_sum_tmp, local_util_coef2_avg * divider);
	dpt_rq->util_cpu_sum_tmp = max_t(u32, dpt_rq->util_cpu_sum_tmp, dpt_rq->util_cpu_avg_tmp * PELT_MIN_DIVIDER);
	dpt_rq->util_coef1_sum_tmp = max_t(u32, dpt_rq->util_coef1_sum_tmp,
		dpt_rq->util_coef1_avg_tmp * PELT_MIN_DIVIDER);
	dpt_rq->util_coef2_sum_tmp = max_t(u32, dpt_rq->util_coef2_sum_tmp,
		dpt_rq->util_coef2_avg_tmp * PELT_MIN_DIVIDER);

	if (trace_sched_detach_entity_load_avg_enabled())
		trace_sched_detach_entity_load_avg(cpu, task_pid_nr(p), dpt_rq, util_task);
}

/* Hook from `trace_android_rvh_enqueue_task_fair` */
void mtk_enqueue_task_fair(void *unused, struct rq *rq, struct task_struct *p, int flags)
{
	int cpu;
	unsigned int enqueued_cpu, enqueued_coef1, enqueued_coef2;
	unsigned int local_util_cpu_est, local_util_coef1_est, local_util_coef2_est;
	struct dpt_task_struct *util_cfs = NULL, *util_task = NULL;

	if(!is_dpt_v2_support())
		return;

	if (!sched_feat(UTIL_EST))
		return;

	if (p->se.sched_delayed && (task_on_rq_migrating(p) || (flags & ENQUEUE_RESTORE)))
		return;

	cpu = cpu_of(rq);
	util_task = &((struct mtk_task *) android_task_vendor_data(p))->dpt_task;
	util_cfs = &per_cpu(__dpt_rq, cpu).util_cfs;

	if (trace_sched_enqueue_task_fair_enabled())
		trace_sched_enqueue_task_fair(cpu, task_pid_nr(p), util_cfs, util_task);

	task_global_to_local_dpt_v2(cpu, p, NULL, NULL, NULL, &local_util_cpu_est, &local_util_coef1_est, &local_util_coef2_est);

	enqueued_cpu = util_cfs->util_cpu_est & UTIL_EST_MASK;
	enqueued_coef1 = util_cfs->util_coef1_est & UTIL_EST_MASK;
	enqueued_coef2 = util_cfs->util_coef2_est & UTIL_EST_MASK;
	enqueued_cpu += local_util_cpu_est;
	enqueued_coef1 += local_util_coef1_est;
	enqueued_coef2 += local_util_coef2_est;

	/* Prevent the scaling factor being updated between enq/deq,
	* which may cause the wrong util_est of root cfs_rq*/
	WRITE_ONCE(util_task->util_cpu_est, (_task_util_est_dpt_v2(p, CPU_UTIL) | (local_util_cpu_est << UTIL_EST_QUEUE_SHIFT) ));
	WRITE_ONCE(util_task->util_coef1_est, (_task_util_est_dpt_v2(p, COEF1_UTIL) | (local_util_coef1_est << UTIL_EST_QUEUE_SHIFT) ));
	WRITE_ONCE(util_task->util_coef2_est, (_task_util_est_dpt_v2(p, COEF2_UTIL) | (local_util_coef2_est << UTIL_EST_QUEUE_SHIFT) ));
	WRITE_ONCE(util_cfs->util_cpu_est, enqueued_cpu);
	WRITE_ONCE(util_cfs->util_coef1_est, enqueued_coef1);
	WRITE_ONCE(util_cfs->util_coef2_est, enqueued_coef2);

	if (trace_sched_enqueue_task_fair_enabled())
		trace_sched_enqueue_task_fair(cpu, task_pid_nr(p), util_cfs, util_task);
}

/* Hook from `trace_android_rvh_dequeue_task_fair` */
void mtk_dequeue_task_fair(void *unused, struct rq *rq, struct task_struct *p, int flags)
{
	int cpu;
	unsigned int enqueued_cpu, enqueued_coef1, enqueued_coef2;
	struct dpt_task_struct *util_cfs = NULL, *util_task = NULL;

	if(!is_dpt_v2_support())
		return;

	if (!sched_feat(UTIL_EST))
		return;

	if (!p) /* catch null task after migrate to kmainline */
		return;

	if (p->se.sched_delayed && (task_on_rq_migrating(p) || (flags & DEQUEUE_SAVE)))
		return;

	cpu = cpu_of(rq);
	util_task = &((struct mtk_task *) android_task_vendor_data(p))->dpt_task;
	util_cfs = &per_cpu(__dpt_rq, cpu).util_cfs;

	if (trace_sched_dequeue_task_fair_enabled())
		trace_sched_dequeue_task_fair(cpu, task_pid_nr(p), util_cfs, util_task);

	enqueued_cpu = util_cfs->util_cpu_est & UTIL_EST_MASK;
	enqueued_coef1 = util_cfs->util_coef1_est & UTIL_EST_MASK;
	enqueued_coef2 = util_cfs->util_coef2_est & UTIL_EST_MASK;

	enqueued_cpu -= min_t(unsigned int, enqueued_cpu, _task_util_est_queue_dpt_v2(p, CPU_UTIL));
	enqueued_coef1 -= min_t(unsigned int, enqueued_coef1, _task_util_est_queue_dpt_v2(p, COEF1_UTIL));
	enqueued_coef2 -= min_t(unsigned int, enqueued_coef2, _task_util_est_queue_dpt_v2(p, COEF2_UTIL));
	WRITE_ONCE(util_cfs->util_cpu_est, enqueued_cpu);
	WRITE_ONCE(util_cfs->util_coef1_est, enqueued_coef1);
	WRITE_ONCE(util_cfs->util_coef2_est, enqueued_coef2);

	if (trace_sched_dequeue_task_fair_enabled())
		trace_sched_dequeue_task_fair(cpu, task_pid_nr(p), util_cfs, util_task);
}

/* Hook from `trace_android_rvh_remove_entity_load_avg` */
void mtk_remove_entity_load_avg(void *unused, struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	int cpu;
	unsigned long flags, local_util_cpu_avg, local_util_coef1_avg, local_util_coef2_avg;
	struct dpt_task_struct *util_task = NULL;
	dpt_rq_t *dpt_rq = NULL;
	struct task_struct *p = NULL;

	if(!is_dpt_v2_support())
		return;

	if (unlikely(!entity_is_task(se)))
		return;

	cpu = cpu_of(rq_of(cfs_rq));
	dpt_rq = &per_cpu(__dpt_rq, cpu);
	p = task_of(se);
	util_task = &((struct mtk_task *) android_task_vendor_data(p))->dpt_task;

	if (trace_sched_remove_entity_load_avg_enabled())
		trace_sched_remove_entity_load_avg(cpu, task_pid_nr(p), dpt_rq, util_task);

	task_global_to_local_dpt_v2(cpu, p, &local_util_cpu_avg, &local_util_coef1_avg, &local_util_coef2_avg, NULL, NULL, NULL);

	raw_spin_lock_irqsave(&cfs_rq->removed.lock, flags);
	++dpt_rq->removed_nr;
	dpt_rq->removed_util_cpu_avg	+= local_util_cpu_avg;
	dpt_rq->removed_util_coef1_avg	+= local_util_coef1_avg;
	dpt_rq->removed_util_coef2_avg	+= local_util_coef2_avg;
	raw_spin_unlock_irqrestore(&cfs_rq->removed.lock, flags);

	if (trace_sched_remove_entity_load_avg_enabled())
		trace_sched_remove_entity_load_avg(cpu, task_pid_nr(p), dpt_rq, util_task);
}

/* Hook from `trace_sched_util_est_se_tp` */
void sched_task_util_est_hook(void *data, struct sched_entity *se)
{
	unsigned int ewma_cpu, dequeued_cpu, last_ewma_diff_cpu;
	unsigned int ewma_coef1, dequeued_coef1, last_ewma_diff_coef1;
	unsigned int ewma_coef2, dequeued_coef2, last_ewma_diff_coef2;
	struct dpt_task_struct *util_task = NULL;
	struct task_struct *p;

	if(!is_dpt_v2_support())
		return;

	if (unlikely(!entity_is_task(se)))
		return;

	p = container_of(se, struct task_struct, se);
	util_task = &((struct mtk_task *) android_task_vendor_data(p))->dpt_task;

	ewma_cpu = _task_util_est_dpt_v2(p, CPU_UTIL);
	ewma_coef1 = _task_util_est_dpt_v2(p, COEF1_UTIL);
	ewma_coef2 = _task_util_est_dpt_v2(p, COEF2_UTIL);

	dequeued_cpu = task_util_dpt_v2(p, CPU_UTIL);
	dequeued_coef1 = task_util_dpt_v2(p, COEF1_UTIL);
	dequeued_coef2 = task_util_dpt_v2(p, COEF2_UTIL);

	if ((task_util(p) + UTIL_EST_MARGIN) < task_runnable(p))
		goto done;

	if (ewma_cpu <= dequeued_cpu)
		ewma_cpu = dequeued_cpu;
	else
	{
		last_ewma_diff_cpu = ewma_cpu - dequeued_cpu;
		if (last_ewma_diff_cpu >= UTIL_EST_MARGIN)
		{
			ewma_cpu <<= UTIL_EST_WEIGHT_SHIFT;
			ewma_cpu  -= last_ewma_diff_cpu;
			ewma_cpu >>= UTIL_EST_WEIGHT_SHIFT;
		}
	}

	if (ewma_coef1 <= dequeued_coef1)
		ewma_coef1 = dequeued_coef1;
	else
	{
		last_ewma_diff_coef1 = ewma_coef1 - dequeued_coef1;
		ewma_coef1 <<= UTIL_EST_WEIGHT_SHIFT;
		ewma_coef1  -= last_ewma_diff_coef1;
		ewma_coef1 >>= UTIL_EST_WEIGHT_SHIFT;
	}

	if (ewma_coef2 <= dequeued_coef2)
		ewma_coef2 = dequeued_coef2;
	else
	{
		last_ewma_diff_coef2 = ewma_coef2 - dequeued_coef2;
		ewma_coef2 <<= UTIL_EST_WEIGHT_SHIFT;
		ewma_coef2  -= last_ewma_diff_coef2;
		ewma_coef2 >>= UTIL_EST_WEIGHT_SHIFT;
	}

done:
	if(!ewma_cpu && (se->avg.util_est & ~UTIL_AVG_UNCHANGED))
		ewma_cpu = (se->avg.util_est & ~UTIL_AVG_UNCHANGED);
	WRITE_ONCE(util_task->util_cpu_est, ewma_cpu);
	WRITE_ONCE(util_task->util_coef1_est, ewma_coef1);
	WRITE_ONCE(util_task->util_coef2_est, ewma_coef2);

	if (trace_sched_util_est_update_enabled())
		trace_sched_util_est_update(task_pid_nr(p), se->avg.util_est ,util_task);
}
