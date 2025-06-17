// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/hardirq.h>
#include <linux/init.h>
#include <linux/kallsyms.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/sort.h>
#include <linux/string.h>
#include <asm/div64.h>
#include <linux/topology.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/sched/task.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <linux/cpufreq.h>
#include <linux/irq_work.h>
#include <linux/power_supply.h>
#include "sugov/cpufreq.h"

#include "fpsgo_frame_info.h"
#include "game.h"
#include "loom_loading_ctrl.h"
#include "loom_base.h"
#include "loom_rescue.h"


static int loading_thr_up_bound;
static int loading_thr_low_bound;
static int g_loading_window_size;
static int policy_num;
static int g_opp_up_step;
static int g_opp_down_step;
static int g_limit_min_freq;
static int g_limit_max_freq;
static int g_expected_fps;

struct freq_qos_request *freq_max_request;
struct freq_qos_request *freq_min_request;

module_param(g_loading_window_size, int, 0644);
module_param(g_opp_up_step, int, 0644);
module_param(g_opp_down_step, int, 0644);
module_param(g_limit_min_freq, int, 0644);
module_param(g_limit_max_freq, int, 0644);
module_param(g_expected_fps, int, 0644);

static int _reset_userlimit_cpufreq_min(int cid)
{
	return freq_qos_remove_request(&(freq_min_request[cid]));
}

static int _reset_userlimit_cpufreq_max(int cid)
{
	return freq_qos_remove_request(&(freq_max_request[cid]));
}

int _update_userlimit_cpufreq_min(int cid, int value)
{
	return freq_qos_update_request(&(freq_min_request[cid]), value);
}

int _update_userlimit_cpufreq_max(int cid, int value)
{
	return freq_qos_update_request(&(freq_max_request[cid]), value);
}

static inline void prefetch_curr_exec_start(struct task_struct *p)
{
#if IS_ENABLED(CONFIG_FAIR_GROUP_SCHED)
	struct sched_entity *curr = p->se.cfs_rq->curr;
#else
	struct sched_entity *curr = task_rq(p)->cfs.curr;
#endif
	prefetch(curr);
	prefetch(&curr->exec_start);
}

unsigned long long loom_task_sched_runtime_precise(struct task_struct *p)
{
	struct rq_flags rf;
	struct rq *rq;
	unsigned long long ns;

#if IS_ENABLED(CONFIG_64BIT) && IS_ENABLED(CONFIG_SMP)
	/*
	 * 64-bit doesn't need locks to atomically read a 64-bit value.
	 * So we have a optimization chance when the task's delta_exec is 0.
	 * Reading ->on_cpu is racy, but this is OK.
	 *
	 * If we race with it leaving CPU, we'll take a lock. So we're correct.
	 * If we race with it entering CPU, unaccounted time is 0. This is
	 * indistinguishable from the read occurring a few cycles earlier.
	 * If we see ->on_cpu without ->on_rq, the task is leaving, and has
	 * been accounted, so we're correct here as well.
	 */
	if (!p->on_cpu || !task_on_rq_queued(p))
		return p->se.sum_exec_runtime;
#endif

	rq = task_rq_lock(p, &rf);
	/*
	 * Must be ->curr _and_ ->on_rq.  If dequeued, we would
	 * project cycles that may never be accounted to this
	 * thread, breaking clock_gettime().
	 */
	if (task_current_donor(rq, p) && task_on_rq_queued(p)) {
		prefetch_curr_exec_start(p);
		update_rq_clock(rq);
		p->sched_class->update_curr(rq);
	}
	ns = p->se.sum_exec_runtime;
	task_rq_unlock(rq, p, &rf);

	return ns;
}

static long long loom_task_sched_runtime(struct task_struct *p)
{
	return loom_task_sched_runtime_precise(p);
	// return p ? p->se.sum_exec_runtime : 0;
}

static int loom_get_runtime(int tid, unsigned long long *runtime)
{
	struct task_struct *p;

	if (unlikely(!tid))
		return -EINVAL;

	rcu_read_lock();
	p = find_task_by_vpid(tid);
	if (!p) {
		rcu_read_unlock();
		return -ESRCH;
	}
	get_task_struct(p);
	rcu_read_unlock();

	*runtime = (u64)loom_task_sched_runtime(p);
	put_task_struct(p);

	return 0;
}

struct loom_loading_ctrl *loom_search_and_add_loading_ctrl_info(struct list_head *lc_active_list,
	int tid, int tgid, int add)
{
	struct loom_loading_ctrl *iter = NULL;
	int i = 0;

	list_for_each_entry(iter, lc_active_list, hlist) {
		if (iter->tid == tid)
			return iter;
	}

	if (!add)
		return NULL;

	iter = kzalloc(sizeof(struct loom_loading_ctrl), GFP_KERNEL);
	if (!iter)
		return NULL;

	INIT_LIST_HEAD(&iter->loading_list);
	iter->tid = tid;
	iter->tgid = tgid;
	iter->loading_window_count = 0;
	iter->loading_thr_up_bound = loading_thr_up_bound;
	iter->loading_thr_low_bound = loading_thr_low_bound;
	iter->cluster = -1;
	iter->cap = -1;
	iter->opp_up_step = g_opp_up_step;
	iter->opp_down_step = g_opp_down_step;
	iter->prev_ts = 0;
	iter->prev_runtime = 0;
	iter->limit_min_freq = 0;
	iter->limit_max_freq = 0;
	for (i = 0; i < LOOM_RESCUE_TIMER_NUM; i++)
		loom_init_jerk(&(iter->loom_proc_obj.jerks[i]), i);

	list_add_tail(&iter->hlist, lc_active_list);

	return iter;
}

void loom_delete_loading_ctrl_info(struct loom_loading_ctrl *lc_info)
{
	struct loom_loading_info *frame = NULL;
	struct loom_loading_info *tmp = NULL;

	if (!lc_info)
		return;

	list_for_each_entry_safe(frame, tmp, &lc_info->loading_list, hlist) {
		list_del(&frame->hlist);
		kvfree(frame);
	}

	_update_userlimit_cpufreq_max(lc_info->cluster,  fbt_cluster_X2Y(lc_info->cluster, 0, OPP, FREQ, 1, __func__));
	_update_userlimit_cpufreq_min(lc_info->cluster, 0);

	list_del(&lc_info->hlist);
	kvfree(lc_info);
}

static void loom_set_loom_ctrl_info_setting(struct loom_loading_ctrl *lc_info, int cluster, int cpu,
	int cap, int freq, unsigned long long prev_ts, unsigned long long prev_runtime)
{
	if (!lc_info)
		return;

	if (cluster >= 0 && cluster <= 2)
		lc_info->cluster = cluster;

	if (cap >= 0 && cap <= 100)
		lc_info->cap = cap;

	if (cpu > -1)
		lc_info->cpu = cpu;

	if (freq)
		lc_info->freq = freq;

	if (prev_ts)
		lc_info->prev_ts = prev_ts;

	if (prev_runtime)
		lc_info->prev_runtime = prev_runtime;
}

void loom_add_new_frame(struct loom_loading_ctrl *lc_info, unsigned long long ts, unsigned long long runtime)
{
	struct loom_loading_info *first_loading = NULL, *new_frame = NULL;
	unsigned long long duration = 0;

	if (!ts || !lc_info)
		return;

	new_frame = kzalloc(sizeof(struct loom_loading_info), GFP_KERNEL);
	if (lc_info->loading_window_count >= g_loading_window_size) {
		first_loading = list_first_entry_or_null(&lc_info->loading_list, struct loom_loading_info, hlist);
		if (first_loading) {
			list_del(&first_loading->hlist);
			kvfree(first_loading);
			lc_info->loading_window_count--;
		}
	}

	duration = ts > lc_info->prev_ts ? ts - lc_info->prev_ts : 0;

	runtime = (runtime > lc_info->prev_runtime && lc_info->prev_runtime) ? runtime - lc_info->prev_runtime : 0;
	lc_info->loading_window_count++;

	new_frame->tid = lc_info->tid;
	new_frame->duration = duration;
	new_frame->runtime = runtime;
	new_frame->cap = lc_info->cap;
	new_frame->freq = lc_info->freq;

	list_add_tail(&new_frame->hlist, &lc_info->loading_list);
}

int loom_cal_window_loading(struct loom_loading_ctrl *lc_info, int *avail_window_count)
{
	int window_loading = 0;
	unsigned long long sum_duration = 0, sum_runtime = 0;
	struct loom_loading_info *frame;

	if (list_empty(&lc_info->loading_list) ||
		lc_info->loading_window_count < g_loading_window_size)
		return 0;

	*avail_window_count = 0;
	list_for_each_entry(frame, &lc_info->loading_list, hlist) {
		if (frame->duration && frame->runtime) {
			sum_duration += frame->duration;
			sum_runtime += frame->runtime;
			(*avail_window_count)++;
		}
	}

	if (sum_duration)
		window_loading = clamp((int)(sum_runtime * 100 / sum_duration), 1, 100);

	return window_loading;
}

int loom_cal_window_freq(struct loom_loading_ctrl *lc_info)
{
	int sum_freq = 0, frame_count = 0, avg_freq = 0;
	struct loom_loading_info *frame;

	if (list_empty(&lc_info->loading_list) ||
		lc_info->loading_window_count < g_loading_window_size)
		return lc_info->freq;

	frame_count = 0;
	list_for_each_entry(frame, &lc_info->loading_list, hlist) {
		if (frame->freq) {
			sum_freq += frame->freq;
			frame_count++;
		}
	}

	if (frame_count)
		avg_freq = sum_freq / frame_count;

	return avg_freq;
}

int (*lc_cal_freq_fp)(int *now_freq, int *now_freq_max, int tid,
		int cluster, int window_loading, int lc_ub, int lc_lb, int prev_freq,
		int limit_min_freq, int limit_max_freq, int bhr_opp_local);
EXPORT_SYMBOL(lc_cal_freq_fp);

int loom_loading_ctrl_operation(struct loom_loading_ctrl *lc_info, unsigned long long ts, int cluster, int cpu)
{
	int window_loading = 0, avail_window_count = 0, cap = 0, bhr = 0, avg_freq = 0, freq = 0;
	int ret = 0;
	unsigned long long runtime = 0;
	int limit_min_freq_final = 1, limit_max_freq_final = 100;
	int now_freq = 0, now_freq_max = 0;

	if (!lc_info) {
		game_main_trace("[%s] loom_loading_ctrl: lc_info is NULL\n", __func__);
		return -1;
	}

	ret = loom_get_runtime(lc_info->tid, &runtime);
	loom_add_new_frame(lc_info, ts, runtime);
	window_loading = loom_cal_window_loading(lc_info, &avail_window_count);
	avg_freq = loom_cal_window_freq(lc_info);

	if (lc_info->bhr >= 0)
		bhr = lc_info->bhr;

	if (lc_info->limit_min_freq >= 1 && lc_info->limit_min_freq <= 3200000)
		limit_min_freq_final = lc_info->limit_min_freq;
	else
		limit_min_freq_final = g_limit_min_freq;

	if (lc_info->limit_max_freq >= 1 && lc_info->limit_max_freq <= 3200000)
		limit_max_freq_final = lc_info->limit_max_freq;
	else
		limit_max_freq_final = g_limit_max_freq;

	lc_info->opp_up_step = g_opp_up_step;
	lc_info->opp_down_step = g_opp_down_step;

	if (avail_window_count < g_loading_window_size) {
#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
		cap = get_curr_cap(cpu);
		freq = fbt_cluster_X2Y(cluster, cap, CAP, FREQ, 1, __func__);
		cap = (cap * 100) >> 10;
#endif  // IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
		goto out;
	} else {
		if (lc_cal_freq_fp) {
			freq = lc_cal_freq_fp(&now_freq, &now_freq_max, lc_info->tid, lc_info->cluster,
				window_loading, lc_info->loading_thr_up_bound, lc_info->loading_thr_low_bound,
				avg_freq, limit_min_freq_final, limit_max_freq_final, bhr);
			_update_userlimit_cpufreq_max(lc_info->cluster, now_freq_max);
			game_systrace_c(GAME_DEBUG_MANDATORY, lc_info->tid, 0, now_freq_max,
				"loading_ctrl_C%d_freq_max", lc_info->cluster);

			_update_userlimit_cpufreq_min(cluster, now_freq);
			game_systrace_c(GAME_DEBUG_MANDATORY, lc_info->tid, 0, now_freq,
				"loading_ctrl_C%d_freq_min", lc_info->cluster);
		}
	}
out:
	loom_set_loom_ctrl_info_setting(lc_info, cluster, cpu, cap, freq, ts, runtime);
	game_systrace_c(GAME_DEBUG_MANDATORY, lc_info->tid, 0, window_loading, "window_loading");
	game_systrace_c(GAME_DEBUG_MANDATORY, lc_info->tid, 0, avg_freq, "avg_freq");
	game_main_trace("[%s] pid=%d, cluster=%d, cpu=%d, w_loading=%llu, freq=%d, ts=%llu, runtime=%llu, count=%d",
		__func__, lc_info->tid, lc_info->cluster, lc_info->cpu, window_loading,
		freq, ts, runtime, avail_window_count);
	loom_lc_set_jerk(lc_info, ts, g_expected_fps);
	return ret;
}

int init_loom_loading_ctrl(void)
{
	int ret = 0;
	int cpu;
	int num = 0, cpu_num = 0;
	struct cpufreq_policy *policy;

	loading_thr_up_bound = 100;
	loading_thr_low_bound = 0;
	g_loading_window_size = 1;
	policy_num = 0;
	g_opp_up_step = 1;
	g_opp_down_step = 1;
	g_limit_min_freq = 0;
	g_limit_max_freq = 0;
	g_expected_fps = 0;

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (policy) {
			pr_info("%s, policy[%d]: first:%d, min:%d, max:%d",
				__func__, cpu_num, cpu, policy->min, policy->max);
			cpu_num++;
			cpu = cpumask_last(policy->related_cpus);
			cpufreq_cpu_put(policy);
		}
	}
	policy_num = cpu_num;

	freq_min_request = kcalloc(policy_num, sizeof(struct freq_qos_request), GFP_KERNEL);
	freq_max_request = kcalloc(policy_num, sizeof(struct freq_qos_request), GFP_KERNEL);
	if (freq_min_request == NULL || freq_max_request == NULL)
		return 0;

	for_each_possible_cpu(cpu) {
		if (num >= policy_num)
			break;

		policy = cpufreq_cpu_get(cpu);

		if (!policy)
			continue;

		ret = freq_qos_add_request(&policy->constraints,
			&(freq_max_request[num]), FREQ_QOS_MAX, fbt_cluster_X2Y(num, 0, OPP, FREQ, 1, __func__));
		if (ret < 0)
			pr_info("%s freq_qos_add_request return %d\n", __func__, ret);

		ret = freq_qos_add_request(&policy->constraints,
			&(freq_min_request[num]), FREQ_QOS_MIN,	0);
		if (ret < 0)
			pr_info("%s freq_qos_add_request return %d\n", __func__, ret);

		num++;
		cpu = cpumask_last(policy->related_cpus);
		cpufreq_cpu_put(policy);
	}

	init_loom_rescue();

	return 0;
}

int exit_loom_loading_ctrl(void)
{
	int i = 0;

	for (i = 0; i < policy_num; i++) {
		_reset_userlimit_cpufreq_min(i);
		_reset_userlimit_cpufreq_max(i);
	}
	kvfree(freq_max_request);
	kvfree(freq_min_request);
	return 0;
}
