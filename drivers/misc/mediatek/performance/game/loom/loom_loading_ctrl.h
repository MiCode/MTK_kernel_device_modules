/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LOOM_LOADING_CTRL_H__
#define __LOOM_LOADING_CTRL_H__


#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/sched/task.h>
#include <linux/sched.h>
#include <linux/kobject.h>

#define LOOM_RESCUE_TIMER_NUM 5

struct loom_jerk {
	int id;
	int jerking;
	int postpone;
	int last_check;
	unsigned long long frame_qu_ts;
	struct hrtimer timer;
	struct work_struct work;
};

struct loom_proc {
	int active_jerk_id;
	struct loom_jerk jerks[LOOM_RESCUE_TIMER_NUM];
};

struct loom_loading_info {
	int tid;
	unsigned long long duration;
	unsigned long long runtime;
	int cap;
	int freq;
	struct list_head hlist;
};

struct loom_loading_ctrl {
	int tid;
	int tgid;
	int loading_window_count;
	int loading_thr_up_bound;
	int loading_thr_low_bound;
	int cap;
	int freq;
	int cluster;
	int cpu;
	int opp_up_step;
	int opp_down_step;
	int bhr;
	int limit_min_freq;
	int limit_max_freq;
	int set_rescue;
	int rescue_f_opp;
	int rescue_c_freq;
	int rescue_time;
	unsigned long long prev_ts;
	unsigned long long prev_runtime;
	struct loom_proc loom_proc_obj;
	struct list_head loading_list;
	struct list_head hlist;
};

struct loom_loading_ctrl *loom_search_and_add_loading_ctrl_info(struct list_head *lc_active_list,
	int tid, int tgid, int add);
void loom_delete_loading_ctrl_info(struct loom_loading_ctrl *lc_info);
int init_loom_loading_ctrl(void);
int exit_loom_loading_ctrl(void);
int loom_loading_ctrl_operation(struct loom_loading_ctrl *lc_info, unsigned long long ts, int cluster, int cpu);
int loom_cal_window_loading(struct loom_loading_ctrl *lc_info, int *avail_window_count);
void loom_add_new_frame(struct loom_loading_ctrl *lc_info, unsigned long long ts, unsigned long long runtime);
int _update_userlimit_cpufreq_min(int cid, int value);
int _update_userlimit_cpufreq_max(int cid, int value);

extern int fbt_cluster_X2Y(int cluster, unsigned long input, int in_type,
	int out_type, int is_to_scale_cap, const char *caller);
// TODO: reset


#endif  // __LOOM_LOADING_CTRL_H__
