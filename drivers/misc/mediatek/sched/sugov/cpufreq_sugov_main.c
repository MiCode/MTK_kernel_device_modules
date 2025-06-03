// SPDX-License-Identifier: GPL-2.0
/*
 * CPUFreq governor based on scheduler-provided CPU utilization data.
 *
 * Copyright (C) 2016, Intel Corporation
 * Author: Rafael J. Wysocki <rafael.j.wysocki@intel.com>
 */
/*
 *
 * Copyright (c) 2019 MediaTek Inc.
 *
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <linux/tick.h>
#include <linux/sched/cpufreq.h>
#include <linux/sched/clock.h>
#include <trace/events/power.h>
#include <trace/hooks/sched.h>
#include <linux/sched/topology.h>
#include <trace/hooks/topology.h>
#include <trace/hooks/cpufreq.h>
#include <linux/sched/cpufreq.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>
#include <thermal_interface.h>
#include <mt-plat/mtk_irq_mon.h>
#include "common.h"
#include "cpufreq.h"
#include "util/cpu_util.h"
#include "sched_version_ctrl.h"
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
#include "mtk_energy_model/v3/energy_model.h"
#else
#include "mtk_energy_model/v1/energy_model.h"
#endif

#define CREATE_TRACE_POINTS
#include "sugov_trace.h"

#define IOWAIT_BOOST_MIN	(SCHED_CAPACITY_SCALE / 8)
#define DEFAULT_DSU_IDLE		true
#define DEFAULT_CURR_TASK_UCLAMP	false
#define DEFAULT_HOLD_FREQ	false
#define DEFAULT_FLT_COEF_MARGIN	false

struct sugov_cpu {
	struct update_util_data	update_util;
	struct sugov_policy	__rcu *sg_policy;
	unsigned int		cpu;

	bool			iowait_boost_pending;
	unsigned int		iowait_boost;
	u64			last_update;
	unsigned long		util;
	unsigned long		bw_min;
	unsigned long		max;

	/* The field below is for single-CPU policies only: */
#if IS_ENABLED(CONFIG_NO_HZ_COMMON)
	unsigned long		saved_idle_calls;
#endif

	/* DPT v2 data */
	unsigned int dpt_v2_cpu_util_local;
	unsigned int dpt_v2_coef1_util_local;
	unsigned int dpt_v2_coef2_util_local;
};

static DEFINE_PER_CPU(struct sugov_cpu, sugov_cpu);
DEFINE_PER_CPU(struct mtk_rq *, rq_data);
EXPORT_SYMBOL(rq_data);

/*
 * dynamic control util_est
 * 0:disable 1:enable
 */
#if IS_ENABLED(CONFIG_MTK_SCHEDULER)
bool sysctl_util_est = true;
EXPORT_SYMBOL(sysctl_util_est);
#endif

void (*fpsgo_notify_fbt_is_boost_fp)(int fpsgo_is_boost);
EXPORT_SYMBOL(fpsgo_notify_fbt_is_boost_fp);

/************************* scheduler common ************************/

static bool sched_debug_lock;

bool _get_sched_debug_lock(void)
{
	return sched_debug_lock;
}
EXPORT_SYMBOL(_get_sched_debug_lock);

void _set_sched_debug_lock(bool lock)
{
	sched_debug_lock = lock;
}
EXPORT_SYMBOL(_set_sched_debug_lock);

/* curr_task_uclamp_max_ctrl */
static int curr_task_uclamp_max_ctrl = DEFAULT_CURR_TASK_UCLAMP;

void set_curr_task_uclamp_ctrl(int set)
{
	curr_task_uclamp_max_ctrl = set;
}
EXPORT_SYMBOL(set_curr_task_uclamp_ctrl);

int get_curr_task_uclamp_ctrl(void)
{
	return curr_task_uclamp_max_ctrl;
}
EXPORT_SYMBOL(get_curr_task_uclamp_ctrl);

void unset_curr_task_uclamp_ctrl(void)
{
	curr_task_uclamp_max_ctrl = DEFAULT_CURR_TASK_UCLAMP;
}
EXPORT_SYMBOL(unset_curr_task_uclamp_ctrl);

/* dsu_idle ctrl */
static bool dsu_idle_enable = DEFAULT_DSU_IDLE;

void set_dsu_idle_enable(bool dsu_idle_ctrl)
{
	dsu_idle_enable = dsu_idle_ctrl;
}
EXPORT_SYMBOL(set_dsu_idle_enable);

void unset_dsu_idle_enable(void)
{
	dsu_idle_enable = DEFAULT_DSU_IDLE;
}
EXPORT_SYMBOL(unset_dsu_idle_enable);

bool is_dsu_idle_enable(void)
{
	return dsu_idle_enable;
}
EXPORT_SYMBOL(is_dsu_idle_enable);

/* dptv2*/
inline struct cpufreq_policy *get_cpufreq_policy(int cpu)
{
	struct sugov_policy *sg_policy;
	struct cpufreq_policy *policy;

	rcu_read_lock();
	sg_policy = rcu_dereference(per_cpu(sugov_cpu, cpu).sg_policy);
	policy = (sg_policy) ? sg_policy->policy : NULL;
	rcu_read_unlock();

	return policy;
}
EXPORT_SYMBOL(get_cpufreq_policy);

/* HOLD FREQ ctrl */
static bool hold_freq_enable = DEFAULT_HOLD_FREQ;

void set_hold_freq_enable(bool set)
{
	hold_freq_enable = set;
}
EXPORT_SYMBOL(set_hold_freq_enable);

void unset_hold_freq_enable(void)
{
	dsu_idle_enable = DEFAULT_DSU_IDLE;
}
EXPORT_SYMBOL(unset_hold_freq_enable);

bool is_hold_freq_enable(void)
{
	return hold_freq_enable;
}
EXPORT_SYMBOL(is_hold_freq_enable);

/* flt_coef_ctrl */
static int flt_coef_margin_ctrl = DEFAULT_FLT_COEF_MARGIN;

void set_flt_coef_margin_ctrl(int set)
{
	flt_coef_margin_ctrl = set;
}
EXPORT_SYMBOL(set_flt_coef_margin_ctrl);

int get_flt_coef_margin_ctrl(void)
{
	return flt_coef_margin_ctrl;
}
EXPORT_SYMBOL(get_flt_coef_margin_ctrl);

/************************ Governor internals ***********************/

static bool sugov_should_update_freq(struct sugov_policy *sg_policy, u64 time)
{
	s64 delta_ns;

	/*
	 * Since cpufreq_update_util() is called with rq->lock held for
	 * the @target_cpu, our per-CPU data is fully serialized.
	 *
	 * However, drivers cannot in general deal with cross-CPU
	 * requests, so while get_next_freq() will work, our
	 * sugov_update_commit() call may not for the fast switching platforms.
	 *
	 * Hence stop here for remote requests if they aren't supported
	 * by the hardware, as calculating the frequency is pointless if
	 * we cannot in fact act on it.
	 *
	 * This is needed on the slow switching platforms too to prevent CPUs
	 * going offline from leaving stale IRQ work items behind.
	 */
	if (!cpufreq_this_cpu_can_update(sg_policy->policy))
		return false;

	if (unlikely(sg_policy->limits_changed)) {
		sg_policy->limits_changed = false;
		sg_policy->need_freq_update = true;
		return true;
	}

	/* No need to recalculate next freq for min_rate_limit_us
	 * at least. However we might still decide to further rate
	 * limit once frequency change direction is decided, according
	 * to the separate rate limits.
	 */

	delta_ns = time - sg_policy->last_freq_update_time;
	return delta_ns >= READ_ONCE(sg_policy->min_rate_limit_ns);
}

static bool sugov_up_down_rate_limit(struct sugov_policy *sg_policy, u64 time,
				     unsigned int next_freq)
{
	s64 delta_ns;

	delta_ns = time - sg_policy->last_freq_update_time;

	if (next_freq > sg_policy->next_freq &&
	    delta_ns < sg_policy->up_rate_delay_ns)
		return true;

	if (next_freq < sg_policy->next_freq &&
	    delta_ns < sg_policy->down_rate_delay_ns)
		return true;

	return false;
}

static int wl_dsu_cnt_cached;
static int wl_cpu_cnt_cached;
static bool sugov_update_next_freq(struct sugov_policy *sg_policy, u64 time,
				   unsigned int next_freq)
{
	if (sugov_up_down_rate_limit(sg_policy, time, next_freq))
		return false;

	if (sg_policy->need_freq_update || wl_dsu_cnt_cached != wl_dsu_delay_ch_cnt ||
		wl_cpu_cnt_cached != wl_cpu_delay_ch_cnt || enq_force_update_freq(sg_policy)) {
		sg_policy->need_freq_update = false;
	} else if (sg_policy->next_freq == next_freq)
		return false;

	wl_dsu_cnt_cached = wl_dsu_delay_ch_cnt;
	wl_cpu_cnt_cached = wl_cpu_delay_ch_cnt;
	sg_policy->next_freq = next_freq;
	sg_policy->last_freq_update_time = time;

	return true;
}

static void sugov_deferred_update(struct sugov_policy *sg_policy)
{
	if (!sg_policy->work_in_progress) {
		sg_policy->work_in_progress = true;
		irq_work_queue(&sg_policy->irq_work);
	}
}

static unsigned int get_next_freq_dpt_v2(struct sugov_policy *sg_policy, int cpu, unsigned int cpu_util_local, unsigned int coef1_util_local, unsigned int coef2_util_local,
	unsigned long *capacity_result, unsigned long min, unsigned long max)
{
	struct cpufreq_policy *policy = sg_policy->policy;
	unsigned int freq = policy->cpuinfo.max_freq;
	unsigned long next_freq = 0;

	mtk_map_util_freq_dpt_v2((void *)sg_policy, cpu, &next_freq, capacity_result, policy->related_cpus, cpu_util_local, coef1_util_local, coef2_util_local, min, max);

	if (next_freq)
		freq = next_freq;
	/* else {
	 * 	freq = map_util_freq((cpu_util_local+coef1_util_local+coef2_util_local), freq, cap);
	 * 	if (freq == sg_policy->cached_raw_freq && !sg_policy->need_freq_update)
	 * 		return sg_policy->next_freq;
	 * 	sg_policy->cached_raw_freq = freq;
	 * 	freq = cpufreq_driver_resolve_freq(policy, freq);
	 * }
	 */

	return freq;
}

/**
 * get_next_freq - Compute a new frequency for a given cpufreq policy.
 * @sg_policy: schedutil policy object to compute the new frequency for.
 * @util: Current CPU utilization.
 * @max: CPU capacity.
 *
 * If the utilization is frequency-invariant, choose the new frequency to be
 * proportional to it, that is
 *
 * next_freq = C * max_freq * util / max
 *
 * Otherwise, approximate the would-be frequency-invariant utilization by
 * util_raw * (curr_freq / max_freq) which leads to
 *
 * next_freq = C * curr_freq * util_raw / max
 *
 * Take C = 1.25 for the frequency tipping point at (util / max) = 0.8.
 *
 * The lowest driver-supported frequency which is equal or greater than the raw
 * next_freq (as calculated above) is returned, subject to policy min/max and
 * cpufreq driver limitations.
 */
static unsigned int get_next_freq(struct sugov_policy *sg_policy,
				  unsigned long util, unsigned long cap,
				  unsigned long min, unsigned long max)
{
	struct cpufreq_policy *policy = sg_policy->policy;
	unsigned int freq = policy->cpuinfo.max_freq;
	unsigned long next_freq = 0;

	mtk_map_util_freq((void *)sg_policy, util, policy->related_cpus, &next_freq, min, max);
	if (next_freq) {
		freq = next_freq;
	} else {
		freq = map_util_freq(util, freq, cap);
		if (freq == sg_policy->cached_raw_freq && !sg_policy->need_freq_update)
			return sg_policy->next_freq;
		sg_policy->cached_raw_freq = freq;
		freq = cpufreq_driver_resolve_freq(policy, freq);
	}

	return freq;
}

//Clone from kernel mainline -  sugov_effective_cpu_perf
unsigned long sugov_effective_cpu_perf_clamp(unsigned long actual,
				unsigned long min, unsigned long max)
{
	/* Add dvfs headroom to actual utilization */
	/* Actually we don't need to target the max performance */
	if (actual < max)
		max = actual;
	/*
	 * Ensure at least minimum performance while providing more compute
	 * capacity when possible.
	 */
	return max(min, max);
}
EXPORT_SYMBOL(sugov_effective_cpu_perf_clamp);

inline int curr_clamp(struct rq *rq, unsigned long *util)
{
	struct task_struct *curr_task;
	int u_min = 0, u_max = 1024;
	int cpu = rq->cpu;
	unsigned long util_debug, util_ori = *util;
	struct curr_uclamp_hint *cu_ht;

	rcu_read_lock();
	curr_task = rcu_dereference(rq->curr);
	if (!curr_task) {
		rcu_read_unlock();
		return -1;
	}

	if (curr_task->exit_state) {
		rcu_read_unlock();
		return -1;
	}

	cu_ht = &((struct mtk_task *)android_task_vendor_data(curr_task))->cu_hint;
	if (!cu_ht->hint) {
		rcu_read_unlock();
		return -1;
	}
	u_min = curr_task->uclamp_req[UCLAMP_MIN].value;
	u_max = curr_task->uclamp_req[UCLAMP_MAX].value;
	rcu_read_unlock();

	*util = clamp_val(*util, u_min, u_max);
	util_debug = *util;

	if (trace_sugov_ext_curr_uclamp_enabled())
		trace_sugov_ext_curr_uclamp(cpu, curr_task->pid,
		util_ori, util_debug, u_min, u_max);
	return 0;
}

/*
 * This function computes an effective utilization for the given CPU, to be
 * used for frequency selection given the linear relation: f = u * f_max.
 *
 * The scheduler tracks the following metrics:
 *
 *   cpu_util_{cfs,rt,dl,irq}()
 *   cpu_bw_dl()
 *
 * Where the cfs,rt and dl util numbers are tracked with the same metric and
 * synchronized windows and are thus directly comparable.
 *
 * The cfs,rt,dl utilization are the running times measured with rq->clock_task
 * which excludes things like IRQ and steal-time. These latter are then accrued
 * in the irq utilization.
 *
 * The DL bandwidth number otoh is not a measured metric but a value computed
 * based on the task model parameters and gives the minimal utilization
 * required to meet deadlines.
 */
unsigned long mtk_effective_cpu_util(unsigned int cpu, unsigned long util_cfs, struct task_struct *p,
				unsigned long *min, unsigned long *max)
{
	unsigned long util, irq, scale;
	unsigned long util_ori;
	struct rq *rq = cpu_rq(cpu);

	scale = arch_scale_cpu_capacity(cpu);


	/*
	 * Early check to see if IRQ/steal time saturates the CPU, can be
	 * because of inaccuracies in how we track these -- see
	 * update_irq_load_avg().
	 */
	irq = cpu_util_irq(rq);
	if (unlikely(irq >= scale)) {
		if (min)
			*min = scale;
		if (max)
			*max = scale;
		return scale;
	}

	if (min) {
		/*
		 * The minimum utilization returns the highest level between:
		 * - the computed DL bandwidth needed with the IRQ pressure which
		 *   steals time to the deadline task.
		 * - The minimum performance requirement for CFS and/or RT.
		 */
		*min = max(irq + cpu_bw_dl(rq), uclamp_rq_get(rq, UCLAMP_MIN));
		/*
		 * When an RT task is runnable and uclamp is not used, we must
		 * ensure that the task will run at maximum compute capacity.
		 */
		if (!uclamp_is_used() && rt_rq_is_runnable(&rq->rt))
			*min = max(*min, scale);
	}


	/*
	 * Because the time spend on RT/DL tasks is visible as 'lost' time to
	 * CFS tasks and we use the same metric to track the effective
	 * utilization (PELT windows are synchronized) we can directly add them
	 * to obtain the CPU's actual utilization.
	 *
	 */
	util = util_cfs + cpu_util_rt(rq);
	util_ori = util;

	if (min && max) {
		bool sbb_trigger = false;
		struct sbb_cpu_data *sbb_data = per_cpu(sbb, cpu);

		if (sbb_data->active &&
				p == (struct task_struct *)UINTPTR_MAX) {

			sbb_trigger = is_sbb_trigger(rq);

			if (sbb_trigger)
				util = util * sbb_data->boost_factor;
		}

		if (p == (struct task_struct *)UINTPTR_MAX) {
			unsigned long umin, umax;
			struct sugov_rq_data *sugov_data_ptr;

			umin = READ_ONCE(rq->uclamp[UCLAMP_MIN].value);
			umax = READ_ONCE(rq->uclamp[UCLAMP_MAX].value);
			sugov_data_ptr = &per_cpu(rq_data, cpu)->sugov_data;
			WRITE_ONCE(sugov_data_ptr->uclamp[UCLAMP_MIN], umin);
			WRITE_ONCE(sugov_data_ptr->uclamp[UCLAMP_MAX], umax);
			p = NULL;
			if (cu_ctrl && curr_clamp(rq, &util) == 0)
				goto skip_rq_uclamp;
			else if (gu_ctrl) {
				unsigned long umax_with_gear;
				umax_with_gear = min_t(unsigned long,
					umax, get_cpu_gear_uclamp_max(cpu));
				*max = umax_with_gear;

				if (trace_sugov_ext_gear_uclamp_enabled())
					trace_sugov_ext_gear_uclamp(cpu, util_ori,
						umin, umax, util, get_cpu_gear_uclamp_max(cpu));
				goto skip_rq_uclamp;
			}
		}
skip_rq_uclamp:
		if (sbb_trigger && trace_sugov_ext_sbb_enabled()) {
			int pid = -1;
			struct task_struct *curr;

			rcu_read_lock();
			curr = rcu_dereference(rq->curr);
			if (curr)
				pid = curr->pid;
			rcu_read_unlock();

			trace_sugov_ext_sbb(cpu, pid,
				sbb_data->boost_factor, util_ori, util,
				sbb_data->cpu_utilize,
				get_sbb_active_ratio_gear(topology_cluster_id(cpu)));
		}
	}


	util += cpu_util_dl(rq);

	/*
	 * The maximum hint is a soft bandwidth requirement, which can be lower
	 * than the actual utilization because of uclamp_max requirements.
	 */
	if (max)
		*max = min(scale, uclamp_rq_get(rq, UCLAMP_MAX));

	if (util >= scale)
		return scale;

	/*
	 * There is still idle time; further improve the number by using the
	 * irq metric. Because IRQ/steal time is hidden from the task clock we
	 * need to scale the task numbers:
	 *
	 *              max - irq
	 *   U' = irq + --------- * U
	 *                 max
	 */
#if 0
	if (trace_sugov_ext_util_debug_enabled())
		trace_sugov_ext_util_debug(cpu, util_cfs, cpu_util_rt(rq), dl_util,
				irq, util, scale_irq_capacity(util, irq, max),
				cpu_bw_dl(rq));
#endif
	util = scale_irq_capacity(util, irq, scale);
	util += irq;

	return min(scale, util);
}
EXPORT_SYMBOL(mtk_effective_cpu_util);

unsigned long mtk_effective_cpu_util_dpt_v2(unsigned int cpu, unsigned long *cpu_util_local,
	unsigned long *coef1_util_local, unsigned long *coef2_util_local, struct task_struct *p,
    unsigned long *min, unsigned long *max)
{
	unsigned long util, irq, scale, orig_umin, orig_umax;
	struct rq *rq = cpu_rq(cpu);
	unsigned long orig_cpu_util = *cpu_util_local, orig_coef1_util = *coef1_util_local, orig_coef2_util = *coef2_util_local;
	int curr_ipc_scaling_factor = get_task_ipc_scaling_factor(rcu_dereference(rq->curr), topology_cluster_id(cpu));

	*coef1_util_local += cpu_util_rt_dpt_v2(rq, COEF1_UTIL);
	*coef2_util_local += cpu_util_rt_dpt_v2(rq, COEF2_UTIL);
	scale = DPT_V2_MAX_RUNNING_TIME_LOCAL;

	/*
	 * Early check to see if IRQ/steal time saturates the CPU, can be
	 * because of inaccuracies in how we track these -- see
	 * update_irq_load_avg().
	 */
	irq = cpu_util_irq(rq);
	if (unlikely(irq >= scale)) {
		if (min)
			*min = scale;
		if (max)
			*max = scale;

		util = scale;
		if (trace_sugov_ext_util_debug_dpt_v2_enabled()) {
			unsigned long utils_debug[10] = {*cpu_util_local, *coef1_util_local, *coef2_util_local, orig_cpu_util,
				orig_coef1_util, orig_coef2_util, scale, scale, uclamp_rq_get(rq, UCLAMP_MIN), uclamp_rq_get(rq, UCLAMP_MAX)};

			trace_sugov_ext_util_debug_dpt_v2(cpu, cpu_rq(cpu), util, utils_debug, 0,
					irq, 0, 1, curr_ipc_scaling_factor);
		}
		return util;
	}

	if (min) {
		/*
		 * The minimum utilization returns the highest level between:
		 * - the computed DL bandwidth needed with the IRQ pressure which
		 *   steals time to the deadline task.
		 * - The minimum performance requirement for CFS and/or RT.
		 */
		*min = max(irq + cpu_bw_dl(rq), uclamp_rq_get(rq, UCLAMP_MIN));
		/*
		 * When an RT task is runnable and uclamp is not used, we must
		 * ensure that the task will run at maximum compute capacity.
		 */
		if (!uclamp_is_used() && rt_rq_is_runnable(&rq->rt))
			*min = max(*min, scale);
	}

	/*
	 * Because the time spend on RT/DL tasks is visible as 'lost' time to
	 * CFS tasks and we use the same metric to track the effective
	 * utilization (PELT windows are synchronized) we can directly add them
	 * to obtain the CPU's actual utilization.
	 *
	 */

	*cpu_util_local += cpu_util_rt_dpt_v2(rq, CPU_UTIL);

	if (min && max) {
		bool sbb_trigger = false;
		struct sbb_cpu_data *sbb_data = per_cpu(sbb, cpu);

		if (sbb_data->active &&
				p == (struct task_struct *)UINTPTR_MAX) {

			sbb_trigger = is_sbb_trigger(rq);

			if (sbb_trigger)
				*cpu_util_local = *cpu_util_local * sbb_data->boost_factor;
		}

		if (p == (struct task_struct *)UINTPTR_MAX) {
			unsigned long umin, umax;
			struct sugov_rq_data *sugov_data_ptr;

			umin = READ_ONCE(rq->uclamp[UCLAMP_MIN].value);
			umax = READ_ONCE(rq->uclamp[UCLAMP_MAX].value);
			sugov_data_ptr = &per_cpu(rq_data, cpu)->sugov_data;
			WRITE_ONCE(sugov_data_ptr->uclamp[UCLAMP_MIN], umin);
			WRITE_ONCE(sugov_data_ptr->uclamp[UCLAMP_MAX], umax);
			p = NULL;
			if (cu_ctrl && curr_clamp(rq, cpu_util_local) == 0) /* not fully support curr_uclamp for now */
				goto skip_rq_uclamp;
			else if (gu_ctrl) {
				unsigned long umax_with_gear;

				umax_with_gear = min_t(unsigned long,
					umax, get_cpu_gear_uclamp_max(cpu));
				*max = umax_with_gear;

				if (trace_sugov_ext_gear_uclamp_dpt_v2_enabled())
					trace_sugov_ext_gear_uclamp_dpt_v2(cpu, *cpu_util_local,
						umin, umax, (orig_cpu_util+cpu_util_rt_dpt_v2(rq, CPU_UTIL)), get_cpu_gear_uclamp_max(cpu), *cpu_util_local, *coef1_util_local, *coef2_util_local);
				goto skip_rq_uclamp;
			}
		}
skip_rq_uclamp:
		if (sbb_trigger && trace_sugov_ext_sbb_enabled()) {
			int pid = -1;
			struct task_struct *curr;

			rcu_read_lock();
			curr = rcu_dereference(rq->curr);
			if (curr)
				pid = curr->pid;
			rcu_read_unlock();

			trace_sugov_ext_sbb(cpu, pid,
				sbb_data->boost_factor, (orig_cpu_util+cpu_util_rt_dpt_v2(rq, CPU_UTIL)), *cpu_util_local,
				sbb_data->cpu_utilize,
				get_sbb_active_ratio_gear(topology_cluster_id(cpu)));
		}
	}

	*cpu_util_local += (cpu_util_dl(rq)  << __get_scaling_factor_shift_bit()) / curr_ipc_scaling_factor;

	if (max)
		*max = min(scale, uclamp_rq_get(rq, UCLAMP_MAX));

	/*
	 * The maximum hint is a soft bandwidth requirement, which can be lower
	 * than the actual utilization because of uclamp_max requirements.
	 */
	orig_umin = min ? *min : 9999;
	orig_umax = max ? *max : 9999;
	dpt_v2_uclamp2local_cap_hook(cpu, false, min, max);

	/*
	 * There is still idle time; further improve the number by using the
	 * irq metric. Because IRQ/steal time is hidden from the task clock we
	 * need to scale the task numbers:
	 *
	 *              max - irq
	 *   U' = irq + --------- * U
	 *                 max
	 */


	*cpu_util_local = scale_irq_capacity(*cpu_util_local, irq, scale);
	*cpu_util_local += irq;

	util = *cpu_util_local + *coef1_util_local + *coef2_util_local;

	if (trace_sugov_ext_util_debug_dpt_v2_enabled()) {
		unsigned long utils_debug[10] = {*cpu_util_local, *coef1_util_local, *coef2_util_local, orig_cpu_util,
			orig_coef1_util, orig_coef2_util, orig_umin, orig_umax, uclamp_rq_get(rq, UCLAMP_MIN), uclamp_rq_get(rq, UCLAMP_MAX)};

		trace_sugov_ext_util_debug_dpt_v2(cpu, cpu_rq(cpu), util, utils_debug, cpu_util_dl(rq),
				irq, cpu_bw_dl(rq), 2, curr_ipc_scaling_factor);
	}

	return util;
}
EXPORT_SYMBOL(mtk_effective_cpu_util_dpt_v2);

static void sugov_get_util_dpt_v2(struct sugov_cpu *sg_cpu, unsigned long boost, unsigned long *min, unsigned long *max)
{
	unsigned long cpu_util_local = 0, coef1_util_local = 0, coef2_util_local = 0;

	mtk_cpu_util_cfs_boost_dpt_v2(sg_cpu->cpu, &cpu_util_local, &coef1_util_local, &coef2_util_local);
	mtk_effective_cpu_util_dpt_v2(sg_cpu->cpu, &cpu_util_local, &coef1_util_local, &coef2_util_local,
							(struct task_struct *)UINTPTR_MAX,
							min, max);

	sg_cpu->bw_min = *min;
	// sg_cpu->max = arch_scale_cpu_capacity(sg_cpu->cpu);
	// sg_cpu->util = max(sg_cpu->util, boost); /* TODO: not sure how to apply io boost*/

	sg_cpu->dpt_v2_cpu_util_local = cpu_util_local;
	sg_cpu->dpt_v2_coef1_util_local = coef1_util_local;
	sg_cpu->dpt_v2_coef2_util_local = coef2_util_local;
}

#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
void (*sugov_grp_awr_update_cpu_tar_util_hook)(int cpu);
EXPORT_SYMBOL(sugov_grp_awr_update_cpu_tar_util_hook);
#endif

static void sugov_get_util(struct sugov_cpu *sg_cpu, struct cpumask *sg_cpumask, unsigned long boost,
		unsigned long *min,unsigned long *max, int curr_task_uclamp)
{
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
	if (sugov_grp_awr_update_cpu_tar_util_hook && grp_dvfs_ctrl_mode)
		sugov_grp_awr_update_cpu_tar_util_hook(sg_cpu->cpu);
#endif

	sg_cpu->util = mtk_effective_cpu_util_total(sg_cpu->cpu, NULL, -1, 1, min, max,
			NULL, NULL, sg_cpumask, boost, curr_task_uclamp);

	sg_cpu->bw_min = *min;
}

/**
 * sugov_iowait_reset() - Reset the IO boost status of a CPU.
 * @sg_cpu: the sugov data for the CPU to boost
 * @time: the update time from the caller
 * @set_iowait_boost: true if an IO boost has been requested
 *
 * The IO wait boost of a task is disabled after a tick since the last update
 * of a CPU. If a new IO wait boost is requested after more then a tick, then
 * we enable the boost starting from IOWAIT_BOOST_MIN, which improves energy
 * efficiency by ignoring sporadic wakeups from IO.
 */
static bool sugov_iowait_reset(struct sugov_cpu *sg_cpu, u64 time,
			       bool set_iowait_boost)
{
	s64 delta_ns = time - sg_cpu->last_update;

	/* Reset boost only if a tick has elapsed since last request */
	if (delta_ns <= TICK_NSEC)
		return false;

	sg_cpu->iowait_boost = set_iowait_boost ? IOWAIT_BOOST_MIN : 0;
	sg_cpu->iowait_boost_pending = set_iowait_boost;

	return true;
}

/**
 * sugov_iowait_boost() - Updates the IO boost status of a CPU.
 * @sg_cpu: the sugov data for the CPU to boost
 * @time: the update time from the caller
 * @flags: SCHED_CPUFREQ_IOWAIT if the task is waking up after an IO wait
 *
 * Each time a task wakes up after an IO operation, the CPU utilization can be
 * boosted to a certain utilization which doubles at each "frequent and
 * successive" wakeup from IO, ranging from IOWAIT_BOOST_MIN to the utilization
 * of the maximum OPP.
 *
 * To keep doubling, an IO boost has to be requested at least once per tick,
 * otherwise we restart from the utilization of the minimum OPP.
 */
static void sugov_iowait_boost(struct sugov_cpu *sg_cpu, u64 time,
			       unsigned int flags)
{
	bool set_iowait_boost = flags & SCHED_CPUFREQ_IOWAIT;

	/* Reset boost if the CPU appears to have been idle enough */
	if (sg_cpu->iowait_boost &&
	    sugov_iowait_reset(sg_cpu, time, set_iowait_boost))
		return;

	/* Boost only tasks waking up after IO */
	if (!set_iowait_boost)
		return;

	/* Ensure boost doubles only one time at each request */
	if (sg_cpu->iowait_boost_pending)
		return;
	sg_cpu->iowait_boost_pending = true;

	/* Double the boost at each request */
	if (sg_cpu->iowait_boost) {
		sg_cpu->iowait_boost =
			min_t(unsigned int, sg_cpu->iowait_boost << 1, SCHED_CAPACITY_SCALE);
		return;
	}

	/* First wakeup after IO: start with minimum boost */
	sg_cpu->iowait_boost = IOWAIT_BOOST_MIN;
}

/**
 * sugov_iowait_apply() - Apply the IO boost to a CPU.
 * @sg_cpu: the sugov data for the cpu to boost
 * @time: the update time from the caller
 *
 * A CPU running a task which woken up after an IO operation can have its
 * utilization boosted to speed up the completion of those IO operations.
 * The IO boost value is increased each time a task wakes up from IO, in
 * sugov_iowait_apply(), and it's instead decreased by this function,
 * each time an increase has not been requested (!iowait_boost_pending).
 *
 * A CPU which also appears to have been idle for at least one tick has also
 * its IO boost utilization reset.
 *
 * This mechanism is designed to boost high frequently IO waiting tasks, while
 * being more conservative on tasks which does sporadic IO operations.
 */
static unsigned long sugov_iowait_apply(struct sugov_cpu *sg_cpu, u64 time,
								unsigned long max_cap)
{

	/* No boost currently required */
	if (!sg_cpu->iowait_boost)
		return 0;

	/* Reset boost if the CPU appears to have been idle enough */
	if (sugov_iowait_reset(sg_cpu, time, false))
		return 0;

	if (!sg_cpu->iowait_boost_pending) {
		/*
		 * No boost pending; reduce the boost value.
		 */
		sg_cpu->iowait_boost >>= 1;
		if (sg_cpu->iowait_boost < IOWAIT_BOOST_MIN) {
			sg_cpu->iowait_boost = 0;
			return 0;
		}
	}

	sg_cpu->iowait_boost_pending = false;

	/*
	 * sg_cpu->util is already in capacity scale; convert iowait_boost
	 * into the same scale so we can compare.
	 */
	return (sg_cpu->iowait_boost * max_cap) >> SCHED_CAPACITY_SHIFT;
}

/*
 * Make sugov_should_update_freq() ignore the rate limit when DL
 * has increased the utilization.
 */
static inline void ignore_dl_rate_limit(struct sugov_cpu *sg_cpu)
{
	if (cpu_bw_dl(cpu_rq(sg_cpu->cpu)) > sg_cpu->bw_min)
		sg_cpu->sg_policy->limits_changed = true;
}

#if IS_ENABLED(CONFIG_MTK_OPP_MIN)
void mtk_set_cpu_min_opp(unsigned int cpu, unsigned long min_util)
{
	int gear_id, min_opp;

	gear_id = topology_cluster_id(cpu);
	min_util = get_cpu_util_with_margin(cpu, min_util);
	min_opp = pd_X2Y(cpu, min_util, CAP, OPP, true, DPT_CALL_MTK_SET_CPU_MIN_OPP);
	set_cpu_min_opp(gear_id, min_opp);
}

void mtk_set_cpu_min_opp_single(struct sugov_cpu *sg_cpu)
{
	int cpu = sg_cpu->cpu;
	struct rq *rq = cpu_rq(cpu);
	unsigned long min_util;

	min_util = READ_ONCE(rq->uclamp[UCLAMP_MIN].value);
	mtk_set_cpu_min_opp(cpu, min_util);
}

void mtk_set_cpu_min_opp_shared(struct sugov_cpu *sg_cpu)
{
	int cpu, i;
	unsigned long min_util = 0, util;
	struct sugov_policy *sg_policy = sg_cpu->sg_policy;
	struct cpufreq_policy *policy = sg_policy->policy;

	for_each_cpu(i, policy->cpus) {
		struct rq *rq = cpu_rq(i);

		util = READ_ONCE(rq->uclamp[UCLAMP_MIN].value);
		min_util = max(min_util, util);
	}


	cpu = cpumask_first(policy->cpus);
	mtk_set_cpu_min_opp(cpu, min_util);
}
#else

void mtk_set_cpu_min_opp_single(struct sugov_cpu *sg_cpu)
{
}

void mtk_set_cpu_min_opp_shared(struct sugov_cpu *sg_cpu)
{
}

#endif

/* Is the rq being capped/throttled by uclamp_max? */
static inline bool mtk_uclamp_rq_is_capped(struct rq *rq)
{
	unsigned long rq_util;
	unsigned long max_util;

	if (!static_branch_likely(&sched_uclamp_used))
		return false;

	rq_util = mtk_cpu_util_cfs(cpu_of(rq)) + cpu_util_rt(rq);
	max_util = READ_ONCE(rq->uclamp[UCLAMP_MAX].value);

	return max_util != SCHED_CAPACITY_SCALE && rq_util >= max_util;
}

static bool sugov_hold_freq(struct sugov_cpu *sg_cpu)
{
	unsigned long idle_calls;
	bool ret;

	if(!hold_freq_enable)
		return false;

	/** The heuristics in this function is for the fair class. For SCX, the
	* performance target comes directly from the BPF scheduler. Let's just
	* follow it.
	*/
	if (scx_switched_all())
		return false;

	/* if capped by uclamp_max, always update to be in compliance */
	if (mtk_uclamp_rq_is_capped(cpu_rq(sg_cpu->cpu)))
		return false;

	/*
	* Maintain the frequency if the CPU has not been idle recently, as
	* reduction is likely to be premature.
	*/
	idle_calls = tick_nohz_get_idle_calls_cpu(sg_cpu->cpu);
	ret = idle_calls == sg_cpu->saved_idle_calls;
	sg_cpu->saved_idle_calls = idle_calls;

	return ret;
}

static inline bool sugov_update_single_common(struct sugov_cpu *sg_cpu, u64 time, unsigned long max_cap,
				unsigned int flags, unsigned long *min, unsigned long *max)
{
	sugov_iowait_boost(sg_cpu, time, flags);
	sg_cpu->last_update = time;
	ignore_dl_rate_limit(sg_cpu);

	if (!sugov_should_update_freq(sg_cpu->sg_policy, time))
		return false;

	return true;
}

static void sugov_update_single_freq(struct update_util_data *hook, u64 time,
				unsigned int flags)
{
	struct sugov_cpu *sg_cpu = container_of(hook, struct sugov_cpu, update_util);
	struct sugov_policy *sg_policy = sg_cpu->sg_policy;
	unsigned long min, max;
	unsigned long umin, umax, max_cap;
	unsigned int next_f;
	unsigned long boost;
	int dpt_v2_support = is_dpt_v2_support();
	unsigned long capacity_result = 0;
	unsigned int cached_freq = sg_policy->cached_raw_freq;
	int curr_task_uclamp = get_curr_task_uclamp_ctrl();

	max_cap = dpt_v2_support ? DPT_V2_MAX_RUNNING_TIME_LOCAL : arch_scale_cpu_capacity(sg_cpu->cpu);

	raw_spin_lock(&sg_policy->update_lock);

	if (!sugov_update_single_common(sg_cpu, time, max_cap, flags, &umin, &umax)) {
		raw_spin_unlock(&sg_policy->update_lock);
		return;
	}

	/* Critical Task aware thermal throttling, notify thermal */
	mtk_set_cpu_min_opp_single(sg_cpu);
	boost = sugov_iowait_apply(sg_cpu, time, max_cap);
	if (dpt_v2_support) {
		sugov_get_util_dpt_v2(sg_cpu, boost, &min, &max);
		umin = min;
		umax = max;
		if (gu_ctrl)
			umax = min_t(unsigned long,
				umax, get_cpu_gear_uclamp_max(sg_cpu->cpu));

		next_f = get_next_freq_dpt_v2(sg_policy, sg_cpu->cpu, sg_cpu->dpt_v2_cpu_util_local,
			sg_cpu->dpt_v2_coef1_util_local, sg_cpu->dpt_v2_coef2_util_local, &capacity_result, umin, umax);

		if (trace_sugov_ext_util_dpt_v2_enabled() && dpt_v2_support)
			trace_sugov_ext_util_dpt_v2(sg_cpu->cpu, next_f, capacity_result, sg_cpu->dpt_v2_cpu_util_local,
				sg_cpu->dpt_v2_coef1_util_local, sg_cpu->dpt_v2_coef2_util_local);

		if (trace_sugov_ext_util_enabled()) /* only for chrome trace debug */
			trace_sugov_ext_util(sg_cpu->cpu, capacity_result, 0, 1024, 0);
	} else {
		sugov_get_util(sg_cpu, sg_policy->policy->related_cpus, boost, &min, &max, curr_task_uclamp);
		umin = min;
		umax = max;
		if (gu_ctrl)
			umax = min_t(unsigned long,
				umax, get_cpu_gear_uclamp_max(sg_cpu->cpu));

		if (trace_sugov_ext_util_enabled()) {
			trace_sugov_ext_util(sg_cpu->cpu, sg_cpu->util, umin, umax, 0);
		}

		next_f = get_next_freq(sg_policy, sg_cpu->util, sg_cpu->max, umin, umax);

		if (sugov_hold_freq(sg_cpu) && next_f < sg_policy->next_freq &&	!sg_policy->need_freq_update) {
			next_f = sg_policy->next_freq;
			/* Restore cached freq as next_freq has changed */
			sg_policy->cached_raw_freq = cached_freq;
		}
	}

	if (!sugov_update_next_freq(sg_policy, time, next_f)) {
		raw_spin_unlock(&sg_policy->update_lock);
		return;
	}

	/*
	 * This code runs under rq->lock for the target CPU, so it won't run
	 * concurrently on two different CPUs for the same target and it is not
	 * necessary to acquire the lock in the fast switch case.
	 */
	if (sg_policy->policy->fast_switch_enabled) {
		irq_log_store();
		cpufreq_driver_fast_switch(sg_policy->policy, next_f);
		irq_log_store();
	} else
		sugov_deferred_update(sg_policy);

	raw_spin_unlock(&sg_policy->update_lock);
}

static void sugov_update_single_perf(struct update_util_data *hook, u64 time, unsigned int flags)
{
	int this_cpu = smp_processor_id();
	struct sugov_rq_data *sugov_data_ptr;

	sugov_data_ptr = &per_cpu(rq_data, this_cpu)->sugov_data;
	WRITE_ONCE(sugov_data_ptr->enq_dvfs, false);

	/** Fall back to the "frequency" path if frequency invariance is not
	* supported, because the direct mapping between the utilization and
	* the performance levels depends on the frequency invariance.
	*/
	sugov_update_single_freq(hook, time, flags);

	/**
	* MTK environment does not use condition:(!arch_scale_freq_invariant()),
	* so the following situation does not exist.
	*	max_cap = arch_scale_cpu_capacity(sg_cpu->cpu);
	if (!sugov_update_single_common(sg_cpu, time, max_cap, flags))
	return;

	if (sugov_hold_freq(sg_cpu) && sg_cpu->util < prev_util)
		sg_cpu->util = prev_util;
		cpufreq_driver_adjust_perf(sg_cpu->cpu, sg_cpu->bw_min,	sg_cpu->util, max_cap);
		sg_cpu->sg_policy->last_freq_update_time = time;
	*/
}

static unsigned int sugov_next_freq_shared_dpt_v2(struct sugov_cpu *sg_cpu, u64 time)
{
	struct sugov_policy *sg_policy = sg_cpu->sg_policy;
	struct cpufreq_policy *policy = sg_policy->policy;
	struct rq *rq;
	struct sugov_rq_data *sugov_data_ptr;
	unsigned long umin, umax;
	unsigned long max_cap, max_freq = 0;
	unsigned int j, freq;
	int idle = 0;
	bool _ignore_idle_ctrl = ignore_idle_ctrl;
	unsigned long capacity_result = 0;

	max_cap = DPT_V2_MAX_RUNNING_TIME_LOCAL;

	for_each_cpu(j, policy->cpus) {
		struct sugov_cpu *j_sg_cpu = &per_cpu(sugov_cpu, j);
		unsigned long boost, min, max;

		boost = sugov_iowait_apply(j_sg_cpu, time, max_cap);
		sugov_get_util_dpt_v2(j_sg_cpu, boost, &min, &max);

		if (_ignore_idle_ctrl) {
			sugov_data_ptr = &per_cpu(rq_data, j)->sugov_data;
			idle = (mtk_available_idle_cpu(j)
				&& ((READ_ONCE(sugov_data_ptr->enq_ing) == 0) ? 1 : 0));
		}

		freq = get_next_freq_dpt_v2(sg_policy, j_sg_cpu->cpu, j_sg_cpu->dpt_v2_cpu_util_local,
			j_sg_cpu->dpt_v2_coef1_util_local, j_sg_cpu->dpt_v2_coef2_util_local, &capacity_result, min, max);

		if (trace_sugov_ext_util_dpt_v2_enabled() && is_dpt_v2_support())
			trace_sugov_ext_util_dpt_v2(j, freq, capacity_result, j_sg_cpu->dpt_v2_cpu_util_local, j_sg_cpu->dpt_v2_coef1_util_local, j_sg_cpu->dpt_v2_coef2_util_local);

		if (trace_sugov_ext_util_enabled()) {
			rq = cpu_rq(j);

			umin = is_dpt_v2_support() ? 0 : rq->uclamp[UCLAMP_MIN].value;
			umax = is_dpt_v2_support() ? 1024 : rq->uclamp[UCLAMP_MAX].value;
			if (gu_ctrl)
				umax = min_t(unsigned long,
					umax, get_cpu_gear_uclamp_max(j));
			trace_sugov_ext_util(j, idle ? 0 : capacity_result, umin, umax, idle);
		}

		if (idle)
			continue;

		max_freq = max(freq, max_freq);
	}

	return max_freq;
}

static unsigned int sugov_next_freq_shared(struct sugov_cpu *sg_cpu, u64 time)
{
	struct sugov_policy *sg_policy = sg_cpu->sg_policy;
	struct cpufreq_policy *policy = sg_policy->policy;
	struct sugov_rq_data *sugov_data_ptr;
	unsigned long umin, umax;
	unsigned long util = 0, max_cap;
	unsigned int j;
	int idle = 0;
	bool _ignore_idle_ctrl = ignore_idle_ctrl;
	int curr_task_uclamp = get_curr_task_uclamp_ctrl();

	max_cap = arch_scale_cpu_capacity(sg_cpu->cpu);

	for_each_cpu(j, policy->cpus) {
		unsigned long boost;
		struct sugov_cpu *j_sg_cpu = &per_cpu(sugov_cpu, j);
		unsigned long min, max;

		boost = sugov_iowait_apply(j_sg_cpu, time, max_cap);
		sugov_get_util(j_sg_cpu, policy->related_cpus, boost, &min, &max, curr_task_uclamp);

		if (_ignore_idle_ctrl) {
			sugov_data_ptr = &per_cpu(rq_data, j)->sugov_data;
			idle = (mtk_available_idle_cpu(j)
				&& ((READ_ONCE(sugov_data_ptr->enq_ing) == 0) ? 1 : 0));
		}
		umin = min;
		umax = max;
		if (gu_ctrl)
			umax = min_t(unsigned long,
				umax, get_cpu_gear_uclamp_max(j));
		if (trace_sugov_ext_util_enabled()) {
			trace_sugov_ext_util(j, idle ? 0 : j_sg_cpu->util, umin, umax, idle);
		}
		if (idle)
			continue;
		util = max(j_sg_cpu->util, util);
	}
	return get_next_freq(sg_policy, util, max_cap, umin, umax);
}

static void
sugov_update_shared(struct update_util_data *hook, u64 time, unsigned int flags)
{
	struct sugov_cpu *sg_cpu = container_of(hook, struct sugov_cpu, update_util);
	struct sugov_policy *sg_policy = sg_cpu->sg_policy;
	unsigned int next_f;
	int this_cpu = smp_processor_id();
	struct sugov_rq_data *sugov_data_ptr;

	raw_spin_lock(&sg_policy->update_lock);

	sugov_data_ptr = &per_cpu(rq_data, this_cpu)->sugov_data;
	WRITE_ONCE(sugov_data_ptr->enq_dvfs, false);

	sugov_iowait_boost(sg_cpu, time, flags);
	sg_cpu->last_update = time;

	ignore_dl_rate_limit(sg_cpu);

	if (sugov_should_update_freq(sg_policy, time)) {
		if (is_dpt_v2_support())
			next_f = sugov_next_freq_shared_dpt_v2(sg_cpu, time);
		else
			next_f = sugov_next_freq_shared(sg_cpu, time);


		if (!sugov_update_next_freq(sg_policy, time, next_f))
			goto unlock;

		if (sg_policy->policy->fast_switch_enabled) {
			irq_log_store();
			cpufreq_driver_fast_switch(sg_policy->policy, next_f);
			irq_log_store();
		} else
			sugov_deferred_update(sg_policy);
	}
unlock:
	irq_log_store();
	/* Critical Task aware thermal throttling, notify thermal */
	mtk_set_cpu_min_opp_shared(sg_cpu);
	irq_log_store();

	raw_spin_unlock(&sg_policy->update_lock);
}

static void sugov_work(struct kthread_work *work)
{
	struct sugov_policy *sg_policy = container_of(work, struct sugov_policy, work);
	unsigned int freq;
	unsigned long flags;

	/*
	 * Hold sg_policy->update_lock shortly to handle the case where:
	 * in case sg_policy->next_freq is read here, and then updated by
	 * sugov_deferred_update() just before work_in_progress is set to false
	 * here, we may miss queueing the new update.
	 *
	 * Note: If a work was queued after the update_lock is released,
	 * sugov_work() will just be called again by kthread_work code; and the
	 * request will be proceed before the sugov thread sleeps.
	 */
	raw_spin_lock_irqsave(&sg_policy->update_lock, flags);
	freq = sg_policy->next_freq;
	sg_policy->work_in_progress = false;
	raw_spin_unlock_irqrestore(&sg_policy->update_lock, flags);

	mutex_lock(&sg_policy->work_lock);
	__cpufreq_driver_target(sg_policy->policy, freq, CPUFREQ_RELATION_L);
	mutex_unlock(&sg_policy->work_lock);
}

static void sugov_irq_work(struct irq_work *irq_work)
{
	struct sugov_policy *sg_policy;

	sg_policy = container_of(irq_work, struct sugov_policy, irq_work);

	kthread_queue_work(&sg_policy->worker, &sg_policy->work);
}

/************************** sysfs interface ************************/

static struct sugov_tunables *global_tunables;
static DEFINE_MUTEX(global_tunables_lock);

static inline struct sugov_tunables *to_sugov_tunables(struct gov_attr_set *attr_set)
{
	return container_of(attr_set, struct sugov_tunables, attr_set);
}

static DEFINE_MUTEX(min_rate_lock);

static void update_min_rate_limit_ns(struct sugov_policy *sg_policy)
{
	mutex_lock(&min_rate_lock);
	WRITE_ONCE(sg_policy->min_rate_limit_ns, min(sg_policy->up_rate_delay_ns,
					   sg_policy->down_rate_delay_ns));
	mutex_unlock(&min_rate_lock);
}

static ssize_t up_rate_limit_us_show(struct gov_attr_set *attr_set, char *buf)
{
	struct sugov_tunables *tunables = to_sugov_tunables(attr_set);

	return sprintf(buf, "%u\n", tunables->up_rate_limit_us);
}

static ssize_t down_rate_limit_us_show(struct gov_attr_set *attr_set, char *buf)
{
	struct sugov_tunables *tunables = to_sugov_tunables(attr_set);

	return sprintf(buf, "%u\n", tunables->down_rate_limit_us);
}

static ssize_t
up_rate_limit_us_store(struct gov_attr_set *attr_set, const char *buf, size_t count)
{
	struct sugov_tunables *tunables = to_sugov_tunables(attr_set);
	struct sugov_policy *sg_policy;
	unsigned int rate_limit_us;

	if (kstrtouint(buf, 10, &rate_limit_us))
		return -EINVAL;

	tunables->up_rate_limit_us = rate_limit_us;

	list_for_each_entry(sg_policy, &attr_set->policy_list, tunables_hook) {
		sg_policy->up_rate_delay_ns = rate_limit_us * NSEC_PER_USEC;
		update_min_rate_limit_ns(sg_policy);
	}

	return count;
}

static ssize_t
down_rate_limit_us_store(struct gov_attr_set *attr_set, const char *buf, size_t count)
{
	struct sugov_tunables *tunables = to_sugov_tunables(attr_set);
	struct sugov_policy *sg_policy;
	unsigned int rate_limit_us;

	if (kstrtouint(buf, 10, &rate_limit_us))
		return -EINVAL;

	tunables->down_rate_limit_us = rate_limit_us;

	list_for_each_entry(sg_policy, &attr_set->policy_list, tunables_hook) {
		sg_policy->down_rate_delay_ns = rate_limit_us * NSEC_PER_USEC;
		update_min_rate_limit_ns(sg_policy);
	}

	return count;
}

static struct governor_attr up_rate_limit_us = __ATTR_RW(up_rate_limit_us);
static struct governor_attr down_rate_limit_us = __ATTR_RW(down_rate_limit_us);

static struct attribute *sugov_attrs[] = {
	&up_rate_limit_us.attr,
	&down_rate_limit_us.attr,
	NULL
};
ATTRIBUTE_GROUPS(sugov);

static void sugov_tunables_free(struct kobject *kobj)
{
	struct gov_attr_set *attr_set = to_gov_attr_set(kobj);

	kfree(to_sugov_tunables(attr_set));
}

static struct kobj_type sugov_tunables_ktype = {
	.default_groups = sugov_groups,
	.sysfs_ops = &governor_sysfs_ops,
	.release = &sugov_tunables_free,
};

/********************** cpufreq governor interface *********************/

#if IS_ENABLED(CONFIG_ENERGY_MODEL)
static DEFINE_MUTEX(sched_energy_mutex);
static bool sched_energy_update;
extern void rebuild_sched_domains(void);

void rebuild_sched_domains_energy(void)
{
	mutex_lock(&sched_energy_mutex);
	sched_energy_update = true;
	rebuild_sched_domains();
	sched_energy_update = false;
	mutex_unlock(&sched_energy_mutex);
}

static void rebuild_sd_workfn(struct work_struct *work)
{
	rebuild_sched_domains_energy();
}
static DECLARE_WORK(rebuild_sd_work, rebuild_sd_workfn);

/*
 * EAS shouldn't be attempted without sugov, so rebuild the sched_domains
 * on governor changes to make sure the scheduler knows about it.
 */
static void sugov_eas_rebuild_sd(void)
{
	/*
	 * When called from the cpufreq_register_driver() path, the
	 * cpu_hotplug_lock is already held, so use a work item to
	 * avoid nested locking in rebuild_sched_domains().
	 */
	schedule_work(&rebuild_sd_work);
}
#else
static inline void sugov_eas_rebuild_sd(void) { };
#endif

struct cpufreq_governor mtk_gov;

static struct sugov_policy *sugov_policy_alloc(struct cpufreq_policy *policy)
{
	struct sugov_policy *sg_policy;

	sg_policy = kzalloc(sizeof(*sg_policy), GFP_KERNEL);
	if (!sg_policy)
		return NULL;

	sg_policy->policy = policy;
	raw_spin_lock_init(&sg_policy->update_lock);
	return sg_policy;
}

static void sugov_policy_free(struct rcu_head *head)
{
	struct sugov_policy *sg_policy = container_of(head, struct sugov_policy, rcu);
	kfree(sg_policy);
}

static int sugov_kthread_create(struct sugov_policy *sg_policy)
{
	struct task_struct *thread;
	struct sched_attr attr = {
		.size		= sizeof(struct sched_attr),
		.sched_policy	= SCHED_DEADLINE,
		.sched_flags	= SCHED_FLAG_SUGOV,
		.sched_nice	= 0,
		.sched_priority	= 0,
		/*
		 * Fake (unused) bandwidth; workaround to "fix"
		 * priority inheritance.
		 */
		.sched_runtime	=  NSEC_PER_MSEC,
		.sched_deadline = 10 * NSEC_PER_MSEC,
		.sched_period	= 10 * NSEC_PER_MSEC,
	};
	struct cpufreq_policy *policy = sg_policy->policy;
	int ret;

	/* kthread only required for slow path */
	if (policy->fast_switch_enabled)
		return 0;

	kthread_init_work(&sg_policy->work, sugov_work);
	kthread_init_worker(&sg_policy->worker);
	thread = kthread_create(kthread_worker_fn, &sg_policy->worker,
				"sugov:%d",
				cpumask_first(policy->related_cpus));
	if (IS_ERR(thread)) {
		pr_info("failed to create sugov thread: %ld\n", PTR_ERR(thread));
		return PTR_ERR(thread);
	}

	ret = sched_setattr_nocheck(thread, &attr);
	if (ret) {
		kthread_stop(thread);
		pr_info("%s: failed to set SCHED_DEADLINE\n", __func__);
		return ret;
	}

	sg_policy->thread = thread;
	kthread_bind_mask(thread, policy->related_cpus);
	init_irq_work(&sg_policy->irq_work, sugov_irq_work);
	mutex_init(&sg_policy->work_lock);

	wake_up_process(thread);

	return 0;
}

static void sugov_kthread_stop(struct sugov_policy *sg_policy)
{
	/* kthread only required for slow path */
	if (sg_policy->policy->fast_switch_enabled)
		return;

	kthread_flush_worker(&sg_policy->worker);
	kthread_stop(sg_policy->thread);
	mutex_destroy(&sg_policy->work_lock);
}

static struct sugov_tunables *sugov_tunables_alloc(struct sugov_policy *sg_policy)
{
	struct sugov_tunables *tunables;

	tunables = kzalloc(sizeof(*tunables), GFP_KERNEL);
	if (tunables) {
		gov_attr_set_init(&tunables->attr_set, &sg_policy->tunables_hook);
		if (!have_governor_per_policy())
			global_tunables = tunables;
	}
	return tunables;
}

static void sugov_clear_global_tunables(void)
{
	if (!have_governor_per_policy())
		global_tunables = NULL;
}

static int sugov_init(struct cpufreq_policy *policy)
{
	struct sugov_policy *sg_policy;
	struct sugov_tunables *tunables;
	int ret = 0;

	/* State should be equivalent to EXIT */
	if (policy->governor_data)
		return -EBUSY;

	cpufreq_enable_fast_switch(policy);

	sg_policy = sugov_policy_alloc(policy);
	if (!sg_policy) {
		ret = -ENOMEM;
		goto disable_fast_switch;
	}

	ret = sugov_kthread_create(sg_policy);
	if (ret)
		goto free_sg_policy;

	mutex_lock(&global_tunables_lock);

	if (global_tunables) {
		if (WARN_ON(have_governor_per_policy())) {
			ret = -EINVAL;
			goto stop_kthread;
		}
		policy->governor_data = sg_policy;
		sg_policy->tunables = global_tunables;

		gov_attr_set_get(&global_tunables->attr_set, &sg_policy->tunables_hook);
		goto out;
	}

	tunables = sugov_tunables_alloc(sg_policy);
	if (!tunables) {
		ret = -ENOMEM;
		goto stop_kthread;
	}

	tunables->up_rate_limit_us = cpufreq_policy_transition_delay_us(policy);
	tunables->down_rate_limit_us = cpufreq_policy_transition_delay_us(policy);

	policy->governor_data = sg_policy;
	sg_policy->tunables = tunables;

	ret = kobject_init_and_add(&tunables->attr_set.kobj, &sugov_tunables_ktype,
				   get_governor_parent_kobj(policy), "%s",
				   mtk_gov.name);
	if (ret)
		goto fail;

	policy->dvfs_possible_from_any_cpu = 1;

out:
	sugov_eas_rebuild_sd();
	mutex_unlock(&global_tunables_lock);
	return 0;

fail:
	kobject_put(&tunables->attr_set.kobj);
	policy->governor_data = NULL;
	sugov_clear_global_tunables();

stop_kthread:
	sugov_kthread_stop(sg_policy);
	mutex_unlock(&global_tunables_lock);

free_sg_policy:
	call_rcu(&sg_policy->rcu, sugov_policy_free);

disable_fast_switch:
	cpufreq_disable_fast_switch(policy);

	pr_info("initialization failed (error %d)\n", ret);
	return ret;
}

static void sugov_exit(struct cpufreq_policy *policy)
{
	struct sugov_policy *sg_policy = policy->governor_data;
	struct sugov_tunables *tunables = sg_policy->tunables;
	unsigned int count;

	mutex_lock(&global_tunables_lock);

	count = gov_attr_set_put(&tunables->attr_set, &sg_policy->tunables_hook);
	policy->governor_data = NULL;
	per_cpu(sugov_cpu, policy->cpu).sg_policy = NULL;
	if (!count)
		sugov_clear_global_tunables();

	mutex_unlock(&global_tunables_lock);

	sugov_kthread_stop(sg_policy);
	call_rcu(&sg_policy->rcu, sugov_policy_free);
	cpufreq_disable_fast_switch(policy);

	sugov_eas_rebuild_sd();
}

static int sugov_start(struct cpufreq_policy *policy)
{
	struct sugov_policy *sg_policy = policy->governor_data;
	unsigned int cpu;

	sg_policy->up_rate_delay_ns =
		sg_policy->tunables->up_rate_limit_us * NSEC_PER_USEC;
	sg_policy->down_rate_delay_ns =
		sg_policy->tunables->down_rate_limit_us * NSEC_PER_USEC;
	update_min_rate_limit_ns(sg_policy);
	sg_policy->last_freq_update_time	= 0;
	sg_policy->next_freq			= 0;
	sg_policy->work_in_progress		= false;
	sg_policy->limits_changed		= false;
	sg_policy->need_freq_update		= false;
	sg_policy->cached_raw_freq		= 0;

	for_each_cpu(cpu, policy->cpus) {
		struct sugov_cpu *sg_cpu = &per_cpu(sugov_cpu, cpu);

		memset(sg_cpu, 0, sizeof(*sg_cpu));
		sg_cpu->cpu			= cpu;
		sg_cpu->sg_policy		= sg_policy;

		cpufreq_add_update_util_hook(cpu, &sg_cpu->update_util,
					     policy_is_shared(policy) ?
							sugov_update_shared :
							sugov_update_single_perf);
	}

	return 0;
}

static void sugov_stop(struct cpufreq_policy *policy)
{
	struct sugov_policy *sg_policy = policy->governor_data;
	unsigned int cpu;

	for_each_cpu(cpu, policy->cpus)
		cpufreq_remove_update_util_hook(cpu);

	synchronize_rcu();

	if (!policy->fast_switch_enabled) {
		irq_work_sync(&sg_policy->irq_work);
		kthread_cancel_work_sync(&sg_policy->work);
	}
}

static void sugov_limits(struct cpufreq_policy *policy)
{
	struct sugov_policy *sg_policy = policy->governor_data;

	if (!policy->fast_switch_enabled) {
		mutex_lock(&sg_policy->work_lock);
		cpufreq_policy_apply_limits(policy);
		mutex_unlock(&sg_policy->work_lock);
	}

	WRITE_ONCE(sg_policy->limits_changed, true);
#if 0
	if (trace_sugov_ext_limits_changed_enabled())
		trace_sugov_ext_limits_changed(policy->cpu, policy->cur, policy->min, policy->max);
#endif
}

struct cpufreq_governor mtk_gov = {
	.name			= "sugov_ext",
	.owner			= THIS_MODULE,
	.init			= sugov_init,
	.exit			= sugov_exit,
	.start			= sugov_start,
	.stop			= sugov_stop,
	.limits			= sugov_limits,
};

int init_mtk_rq_data(void)
{
	int cpu;
	struct mtk_rq *data;

	for_each_possible_cpu(cpu) {
		data = kcalloc(1, sizeof(struct mtk_rq), GFP_KERNEL);
		if (data)
			per_cpu(rq_data, cpu) = data;
		else
			return -ENOMEM;
	}
	return 0;
}

#if !IS_ENABLED(CONFIG_ARM64)
static int __init get_cpu_for_node(struct device_node *node)
{
	struct device_node *cpu_node;
	int cpu;

	cpu_node = of_parse_phandle(node, "cpu", 0);
	if (!cpu_node)
		return -1;

	for_each_possible_cpu(cpu) {
		if (of_get_cpu_node(cpu, NULL) == cpu_node) {
			topology_parse_cpu_capacity(cpu_node, cpu);
			of_node_put(cpu_node);
			return cpu;
		}
	}

	pr_info("Unable to find CPU node for %pOF\n", cpu_node);

	of_node_put(cpu_node);
	return -1;
}

static int __init parse_core(struct device_node *core, int cluster_id,
				int core_id)
{
	char name[10];
	bool leaf = true;
	int i = 0;
	int cpu;
	struct device_node *t;

	do {
		snprintf(name, sizeof(name), "thread%d", i);
		t = of_get_child_by_name(core, name);
		if (t) {
			leaf = false;
			cpu = get_cpu_for_node(t);
			if (cpu >= 0) {
				cpu_topology[cpu].package_id = cluster_id;
				cpu_topology[cpu].cluster_id = cluster_id;
				cpu_topology[cpu].core_id = core_id;
				cpu_topology[cpu].thread_id = i;
			} else {
				pr_info("%pOF: Can't get CPU for thread\n",
					t);
				of_node_put(t);
				return -EINVAL;
			}
			of_node_put(t);
		}
		i++;
	} while (t);

	cpu = get_cpu_for_node(core);
	if (cpu >= 0) {
		if (!leaf) {
			pr_info("%pOF: Core has both threads and CPU\n",
			core);
			return -EINVAL;
		}

		cpu_topology[cpu].package_id = cluster_id;
		cpu_topology[cpu].cluster_id = cluster_id;
		cpu_topology[cpu].core_id = core_id;
	} else if (leaf) {
		pr_info("%pOF: Can't get CPU for leaf core\n", core);
		return -EINVAL;
	}

	return 0;
}

static int __init parse_cluster(struct device_node *cluster, int depth)
{
	char name[10];
	bool leaf = true;
	bool has_cores = false;
	struct device_node *c;

	static int cluster_id;

	int core_id = 0;
	int i, ret;

	i = 0;
	do {
		snprintf(name, sizeof(name), "cluster%d", i);
		c = of_get_child_by_name(cluster, name);
		if (c) {
			leaf = false;
			ret = parse_cluster(c, depth + 1);
			of_node_put(c);
			if (ret != 0)
				return ret;
		}
		i++;
	} while (c);

	i = 0;
	do {
		snprintf(name, sizeof(name), "core%d", i);
		c = of_get_child_by_name(cluster, name);
		if (c) {
			has_cores = true;

			if (depth == 0) {
				pr_info("%pOF: cpu-map children should be clusters\n",
					c);
				of_node_put(c);
				return -EINVAL;
			}

			if (leaf) {
				ret = parse_core(c, cluster_id, core_id++);
			} else {
				pr_info("%pOF: Non-leaf cluster with core %s\n",
					cluster, name);
				ret = -EINVAL;
			}

			of_node_put(c);
			if (ret != 0)
				return ret;
		}
		i++;
	} while (c);

	if (leaf && !has_cores)
		pr_info("%pOF: empty cluster\n", cluster);

	if (leaf)
		cluster_id++;

	return 0;
}

static int __init parse_dt_topology_arm(void)
{
	struct device_node *cn_cpus = NULL;
	struct device_node *map;
	int ret;

	pr_info("parse_dt_topology\n");
	cn_cpus = of_find_node_by_path("/cpus");
	if (!cn_cpus) {
		pr_info("No CPU information found in DT\n");
		return -EINVAL;
	}

	map = of_get_child_by_name(cn_cpus, "cpu-map");
	if (!map) {
		pr_info("No cpu-map information found in DT\n");
		return -EINVAL;
	}

	ret = parse_cluster(map, 0);
	of_node_put(map);

	return ret;
}

#endif

static void sched_build_perf_domains(void *unused, bool *eas_check)
{
	*eas_check = true;
}

static int __init cpufreq_mtk_init(void)
{
	int ret = 0;
	struct proc_dir_entry *dir;

	ret = init_mtk_rq_data();
	if (ret) {
		pr_info("%s: failed to allocate init_mtk_rq_data, ret: %d\n",
			__func__, ret);
		return ret;
	}

	ret = mtk_static_power_init();
	if (ret) {
		pr_info("%s: failed to init MTK EM, ret: %d\n",
			__func__, ret);
		return ret;
	}

	dir = proc_mkdir("mtk_scheduler", NULL);
	if (!dir)
		return -ENOMEM;

	ret = init_sched_ctrl();
	if(ret)
		pr_info("register init_sched_ctrl failed\n");

#if !IS_ENABLED(CONFIG_ARM64)
	ret = parse_dt_topology_arm();
	if (ret)
		pr_info("parse_dt_topology fail on arm32, returned %d\n", ret);
#endif

	ret = init_opp_cap_info(dir);
	if (ret)
		pr_info("init_opp_cap_info failed\n");

	ret = register_trace_android_rvh_update_cpu_capacity(
			hook_update_cpu_capacity, NULL);
	if (ret)
		pr_info("register android_rvh_update_cpu_capacity failed\n");

#if IS_ENABLED(CONFIG_NONLINEAR_FREQ_CTL)
	ret = register_trace_android_vh_cpufreq_fast_switch(mtk_cpufreq_fast_switch, NULL);
	if (ret)
		pr_info("register android_vh_cpufreq_fast_switch failed\n");

	ret = register_trace_android_vh_cpufreq_target(mtk_cpufreq_target, NULL);
	if (ret)
		pr_info("register android_vh_cpufreq_target failed\n");

	ret = register_trace_android_vh_arch_set_freq_scale(
			mtk_arch_set_freq_scale, NULL);
	if (ret)
		pr_info("register android_vh_arch_set_freq_scale failed\n");
	else
		topology_clear_scale_freq_source(SCALE_FREQ_SOURCE_ARCH, cpu_possible_mask);
#endif
	ret = register_trace_android_rvh_build_perf_domains(sched_build_perf_domains, NULL);
	if (ret)
		pr_info("register sched_build_perf_domains hooks failed, returned %d\n", ret);

	return cpufreq_register_governor(&mtk_gov);
}

static void __exit cpufreq_mtk_exit(void)
{
	cpufreq_unregister_governor(&mtk_gov);
}

#if IS_BUILTIN(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
late_initcall(cpufreq_mtk_init);
#else
module_init(cpufreq_mtk_init);
#endif
module_exit(cpufreq_mtk_exit);

MODULE_LICENSE("GPL");
