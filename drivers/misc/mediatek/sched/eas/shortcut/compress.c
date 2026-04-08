// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/sched.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>

#include "common.h"
#include "compress.h"
#include "eas/eas_plus.h"
#include "eas/eas_trace.h"
#include "sugov/cpufreq.h"

#define DEFAULT_SHORTCUT_COMPRESS_RATE 0

#define DEFAULT_RELAX_ENOUGH_TSK_UTIL 30

static int shortcut_compress_rate = DEFAULT_SHORTCUT_COMPRESS_RATE;

static struct relax_enough_util *relax_enough_cpu_util;
static struct relax_enough_util *relax_enough_tsk_util;

static int *tsk_util_clp_last;

#if IS_ENABLED(CONFIG_SMP)
static inline unsigned long task_util(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.util_avg);
}

static inline unsigned long _task_util_est(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.util_est) & ~UTIL_AVG_UNCHANGED;
}

static inline unsigned long task_util_est(struct task_struct *p)
{
	return max(task_util(p), _task_util_est(p));
}

#if IS_ENABLED(CONFIG_UCLAMP_TASK)
static inline unsigned long uclamp_task_util(struct task_struct *p)
{
	return clamp(rt_task(p) ? 0 : task_util_est(p),
		     uclamp_eff_value(p, UCLAMP_MIN),
		     uclamp_eff_value(p, UCLAMP_MAX));
}
#else
static inline unsigned long uclamp_task_util(struct task_struct *p)
{
	return rt_task(p) ? 0 : task_util_est(p);
}
#endif//CONFIG_UCLAMP_TASK
#endif//CONFIG_SMP

void compress_init(void)
{
	int num_sched_clusters = get_nr_gears();
	int cpu_idx;

	shortcut_compress_rate = DEFAULT_SHORTCUT_COMPRESS_RATE;

	relax_enough_cpu_util = kcalloc(num_sched_clusters, sizeof(struct relax_enough_util), GFP_KERNEL);
	relax_enough_tsk_util = kcalloc(num_sched_clusters, sizeof(struct relax_enough_util), GFP_KERNEL);

	for (int cluster_idx = 0; cluster_idx < num_sched_clusters; cluster_idx++) {
		relax_enough_cpu_util[cluster_idx].user = false;
		relax_enough_tsk_util[cluster_idx].user = false;

		update_shortcut_compress_relax_enough_cpu_util(cluster_idx);
		update_shortcut_compress_relax_enough_tsk_util(cluster_idx);
	}

	tsk_util_clp_last = kcalloc(MAX_NR_CPUS, sizeof(int), GFP_KERNEL);

	for_each_possible_cpu(cpu_idx)
		tsk_util_clp_last[cpu_idx] = -1;
}

void update_shortcut_compress_relax_enough_cpu_util(int cluster_idx)
{
	if (relax_enough_cpu_util[cluster_idx].user == true)
		return;

	relax_enough_cpu_util[cluster_idx].util = pd_opp2cap(cpumask_first(get_gear_cpumask(cluster_idx)),
			INT_MAX, true, 0, NULL, true, DPT_CALL_COMPRESS_INIT);
}

void update_shortcut_compress_relax_enough_tsk_util(int cluster_idx)
{
	if (relax_enough_tsk_util[cluster_idx].user == true)
		return;

	relax_enough_tsk_util[cluster_idx].util = DEFAULT_RELAX_ENOUGH_TSK_UTIL;
}

int compress_to_cpu(struct task_struct *p, int order_index)
{
	return compress_to_cpu_pro(p, order_index);
}

int compress_to_cpu_pro(struct task_struct *p, int order_index)
{
	cpumask_t unpaused_cpus, unpaused_cluster_cpus;
	int candidate_cpu = -1, compress_cpu = -1;
	struct rq *candidate_rq = NULL;
	int cpu_util_tal = -1, tsk_util_clp = -1;
	unsigned long min = 0, max = SCHED_CAPACITY_SCALE;
	int cpu_num = 0;
	int cpu_idx = -1;

	if (shortcut_compress_rate == 0)
		return compress_cpu;

	cpumask_andnot(&unpaused_cpus, cpu_active_mask, cpu_pause_mask);
	cpumask_and(&unpaused_cluster_cpus, &unpaused_cpus, get_gear_cpumask(order_index));

	for_each_cpu(cpu_idx, &unpaused_cluster_cpus) {
		if (cpu_num >= shortcut_compress_rate)
			continue;
		else
			cpu_num++;

		compress_cpu = candidate_cpu = cpu_idx;
		candidate_rq = cpu_rq(cpu_idx);

		if (rt_task(p))
			cpu_util_tal = mtk_effective_cpu_util_total(cpu_idx, p, -1, 1, &min, &max);
		else
			cpu_util_tal = mtk_effective_cpu_util_total(cpu_idx, p, cpu_idx, 1, &min, &max);

		tsk_util_clp = uclamp_task_util(p);

		if (cpu_paused(cpu_idx)) {
			compress_cpu = -2;
			continue;
		}

		if (!cpumask_test_cpu(cpu_idx, p->cpus_ptr)) {
			compress_cpu = -3;
			continue;
		}

		if (rt_task(p) && rt_rq_is_runnable(&(cpu_rq(cpu_idx)->rt))) {
			compress_cpu = -4;
			continue;
		}

		if (cpu_util_tal >= relax_enough_cpu_util[order_index].util) {
			compress_cpu = -5;
			continue;
		}

		if (tsk_util_clp >= relax_enough_tsk_util[order_index].util) {
			compress_cpu = -6;
			continue;
		}

		if (tsk_util_clp_last[cpu_idx] != -1) {
			if (cpu_util_tal + tsk_util_clp_last[cpu_idx] >= relax_enough_cpu_util[order_index].util) {
				compress_cpu = -7;
				tsk_util_clp_last[cpu_idx] = -1;
				continue;
			}
		}

		tsk_util_clp_last[cpu_idx] = tsk_util_clp;

		break;
	}

	if (trace_sched_compress_to_cpu_enabled()) {
		if (candidate_rq)
			trace_sched_compress_to_cpu(p, compress_cpu, candidate_cpu, order_index, tsk_util_clp,
					cpu_util_tal, candidate_rq, shortcut_compress_rate,
					relax_enough_cpu_util[order_index].util,
					relax_enough_tsk_util[order_index].util);
	}

	return compress_cpu;
}

void set_shortcut_compress_rate(int rate)
{
	if (rate == -1) {
		shortcut_compress_rate = DEFAULT_SHORTCUT_COMPRESS_RATE;
		return;
	}

	if (rate < 0)
		return;

	shortcut_compress_rate = rate;
}
EXPORT_SYMBOL(set_shortcut_compress_rate);

void reset_shortcut_compress_rate(void)
{
	set_shortcut_compress_rate(-1);
}
EXPORT_SYMBOL(reset_shortcut_compress_rate);

int get_shortcut_compress_rate(void)
{
	return shortcut_compress_rate;
}
EXPORT_SYMBOL(get_shortcut_compress_rate);

void set_shortcut_compress_relax_enough_cpu_util(int cluster_idx, int cpu_util)
{
	if (cluster_idx < 0 || cluster_idx >= get_nr_gears())
		return;

	if (cpu_util == -1) {
		relax_enough_cpu_util[cluster_idx].user = false;
		update_shortcut_compress_relax_enough_cpu_util(cluster_idx);
		return;
	}

	if (cpu_util < 0 || cpu_util > SCHED_CAPACITY_SCALE)
		return;

	relax_enough_cpu_util[cluster_idx].user = true;
	relax_enough_cpu_util[cluster_idx].util = cpu_util;
}
EXPORT_SYMBOL(set_shortcut_compress_relax_enough_cpu_util);

void set_shortcut_compress_relax_enough_tsk_util(int cluster_idx, int tsk_util)
{
	if (cluster_idx < 0 || cluster_idx >= get_nr_gears())
		return;

	if (tsk_util == -1) {
		relax_enough_tsk_util[cluster_idx].user = false;
		update_shortcut_compress_relax_enough_tsk_util(cluster_idx);
		return;
	}

	if (tsk_util < 0 || tsk_util > SCHED_CAPACITY_SCALE)
		return;

	relax_enough_tsk_util[cluster_idx].user = true;
	relax_enough_tsk_util[cluster_idx].util = tsk_util;
}
EXPORT_SYMBOL(set_shortcut_compress_relax_enough_tsk_util);

void reset_shortcut_compress_relax_enough_cpu_util(int cluster_idx)
{
	set_shortcut_compress_relax_enough_cpu_util(cluster_idx, -1);
}
EXPORT_SYMBOL(reset_shortcut_compress_relax_enough_cpu_util);

void reset_shortcut_compress_relax_enough_tsk_util(int cluster_idx)
{
	set_shortcut_compress_relax_enough_tsk_util(cluster_idx, -1);
}
EXPORT_SYMBOL(reset_shortcut_compress_relax_enough_tsk_util);

int get_shortcut_compress_relax_enough_cpu_util(int cluster_idx)
{
	if (cluster_idx < 0 || cluster_idx >= get_nr_gears())
		return -1;

	return relax_enough_cpu_util[cluster_idx].util;
}
EXPORT_SYMBOL(get_shortcut_compress_relax_enough_cpu_util);

int get_shortcut_compress_relax_enough_tsk_util(int cluster_idx)
{
	if (cluster_idx < 0 || cluster_idx >= get_nr_gears())
		return -1;

	return relax_enough_tsk_util[cluster_idx].util;
}
EXPORT_SYMBOL(get_shortcut_compress_relax_enough_tsk_util);
