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

#define LOOM_SEC_DIVIDER 1000000000

static struct workqueue_struct *wq_jerk;

static void loom_do_jerk_locked(struct loom_loading_ctrl *iter, struct loom_jerk *jerk, int jerk_id)
{
	int rescue_f_opp = 0, rescue_c_freq = 0;
	int base_freq = 0, base_opp = 0, cluster = -1;
	int rescue_min_opp, rescue_min_freq, rescue_max_freq = 0;

	if (!iter)
		return;

	if (iter->set_rescue <= 0)
		return;

	rescue_f_opp = iter->rescue_f_opp;
	rescue_c_freq = iter->rescue_c_freq;
	base_freq = iter->freq;
	cluster = iter->cluster;

	base_opp = fbt_cluster_X2Y(cluster, base_freq, FREQ, OPP, 1, __func__);
	rescue_min_opp = base_opp;
	rescue_min_freq = base_freq;

	if (rescue_f_opp > 0) {
		rescue_min_opp = max(base_opp - rescue_f_opp, 0);
		rescue_min_freq = fbt_cluster_X2Y(cluster, rescue_min_opp, OPP, FREQ, 1, __func__);
	}

	if (rescue_c_freq > 0)
		rescue_max_freq = rescue_c_freq;
	else
		rescue_max_freq = fbt_cluster_X2Y(cluster, 0, OPP, FREQ, 1, __func__);

	if (rescue_min_freq > rescue_max_freq)
		rescue_min_freq = rescue_max_freq;

	_update_userlimit_cpufreq_max(cluster, rescue_max_freq);
	game_systrace_c(GAME_DEBUG_MANDATORY, iter->tid, 0, rescue_max_freq, "loading_ctrl_C%d_freq_max", cluster);

	_update_userlimit_cpufreq_min(cluster, rescue_min_freq);
	game_systrace_c(GAME_DEBUG_MANDATORY, iter->tid, 0, rescue_min_freq, "loading_ctrl_C%d_freq_min", cluster);
}

static void loom_do_jerk(struct work_struct *work)
{
	int ret;
	struct loom_jerk *jerk = NULL;
	struct loom_proc *proc = NULL;
	struct loom_loading_ctrl *iter = NULL;

	loom_render_lock();

	ret = loom_check_loom_jerk_work_addr_invalid(work);
	if (ret) {
		loom_render_unlock();
		return;
	}

	jerk = container_of(work, struct loom_jerk, work);
	if (jerk->id < 0 || jerk->id > LOOM_RESCUE_TIMER_NUM - 1) {
		loom_render_unlock();
		game_main_trace("ERROR %d\n", __LINE__);
		return;
	}

	proc = container_of(jerk, struct loom_proc, jerks[jerk->id]);
	if (proc->active_jerk_id < 0 ||
		proc->active_jerk_id > LOOM_RESCUE_TIMER_NUM - 1) {
		loom_render_unlock();
		game_main_trace("ERROR %d\n", __LINE__);
		return;
	}

	iter = container_of(proc, struct loom_loading_ctrl, loom_proc_obj);

	if (jerk->id != proc->active_jerk_id ||
		jerk->frame_qu_ts != iter->prev_ts)
		goto EXIT;

	loom_do_jerk_locked(iter, jerk, jerk->id);

EXIT:
	jerk->jerking = 0;

	loom_render_unlock();
}


static enum hrtimer_restart loom_jerk_tfn(struct hrtimer *timer)
{
	struct loom_jerk *jerk;

	jerk = container_of(timer, struct loom_jerk, timer);
	if (wq_jerk)
		queue_work(wq_jerk, &jerk->work);
	else
		schedule_work(&jerk->work);
	return HRTIMER_NORESTART;
}

void loom_init_jerk(struct loom_jerk *jerk, int id)
{
	jerk->id = id;

	hrtimer_init(&jerk->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	jerk->timer.function = &loom_jerk_tfn;
	INIT_WORK(&jerk->work, loom_do_jerk);
}

int loom_lc_set_jerk(struct loom_loading_ctrl *iter, unsigned long long ts, unsigned long long expected_fps)
{
	int set_rescue = 0, active_jerk_id = 0;
	struct hrtimer *timer = NULL;
	unsigned long long exp_time_ns = 1ULL, t2wnt = 1ULL;

	if (!iter)
		return -1;

	set_rescue = iter->set_rescue;

	if (set_rescue <= 0  || expected_fps <= 0)
		return -1;

	active_jerk_id = (iter->loom_proc_obj.active_jerk_id + 1) % LOOM_RESCUE_TIMER_NUM;
	iter->loom_proc_obj.active_jerk_id = active_jerk_id;
	iter->loom_proc_obj.jerks[active_jerk_id].last_check = 0;

	exp_time_ns = div64_u64(LOOM_SEC_DIVIDER, expected_fps);
	t2wnt = exp_time_ns;

	if (iter->rescue_time > 0)
		t2wnt = div64_u64(exp_time_ns * (unsigned long long)iter->rescue_time, 100ULL);

	timer = &(iter->loom_proc_obj.jerks[active_jerk_id].timer);
	if (timer) {
		if (iter->loom_proc_obj.jerks[active_jerk_id].jerking == 0) {
			iter->loom_proc_obj.jerks[active_jerk_id].jerking = 1;
			iter->loom_proc_obj.jerks[active_jerk_id].frame_qu_ts = ts;
			hrtimer_start(timer, ns_to_ktime(t2wnt), HRTIMER_MODE_REL);
		}
	} else
		game_main_trace("ERROR timer\n");

	game_main_trace("[%s] tid=%d, jerking=%d", __func__, iter->tid,
		iter->loom_proc_obj.jerks[active_jerk_id].jerking);
	return 0;
}

void init_loom_rescue(void)
{
	wq_jerk = alloc_workqueue("loom_rescue_workque",
		WQ_HIGHPRI | WQ_MEM_RECLAIM | WQ_UNBOUND, 0);
}
