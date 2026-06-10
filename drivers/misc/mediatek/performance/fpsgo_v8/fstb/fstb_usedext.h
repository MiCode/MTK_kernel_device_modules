/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef FSTB_USEDEXT_H
#define FSTB_USEDEXT_H

#include <linux/list.h>
#include <linux/sched.h>

#define DEFAULT_DFPS 120
#define DEFAULT_DTIME 8333333
#define CFG_MAX_FPS_LIMIT	240
#define CFG_MIN_FPS_LIMIT	1
#define FRAME_TIME_BUFFER_SIZE 200
#define MAX_NR_RENDER_FPS_LEVELS	10
#define DEFAULT_RESET_TOLERENCE 3
#define FSTB_IDLE_DBNC 3
#define MAX_FSTB_POLICY_CMD_NUM 10
#define MAX_USER_TARGET_PRIO 5 /*small number low priority*/

extern void (*ged_kpi_output_gfx_info2_fp)(long long t_gpu,
	unsigned int cur_freq, unsigned int cur_max_freq, u64 ulID);

struct fstb_frame_info {
	int pid;
	int proc_id;
	char proc_name[16];
	int hwui_flag;
	int target_fps_policy;
	int target_fps_detect;
	int target_fps_margin;
	int queue_fps;
	int raw_target_fpks;
	int final_target_fpks;
	int target_fps_diff;
	int target_fps_notifying;
	int queue_time_begin;
	int queue_time_end;
	int weighted_cpu_time_begin;
	int weighted_cpu_time_end;
	int weighted_gpu_time_begin;
	int weighted_gpu_time_end;
	int quantile_cpu_time;
	int quantile_gpu_time;
	unsigned long master_type;
	unsigned long long bufid;
	unsigned long long latest_use_ts;
	unsigned long long raw_target_time;
	unsigned long long final_target_time;
	long long cpu_time;
	long long gpu_time;
	unsigned long long queue_time_ts[FRAME_TIME_BUFFER_SIZE]; /*timestamp*/
	unsigned long long weighted_cpu_time[FRAME_TIME_BUFFER_SIZE];
	unsigned long long weighted_cpu_time_ts[FRAME_TIME_BUFFER_SIZE];
	unsigned long long weighted_gpu_time[FRAME_TIME_BUFFER_SIZE];
	unsigned long long weighted_gpu_time_ts[FRAME_TIME_BUFFER_SIZE];
	unsigned long long sorted_weighted_cpu_time[FRAME_TIME_BUFFER_SIZE];
	unsigned long long sorted_weighted_gpu_time[FRAME_TIME_BUFFER_SIZE];

	struct hlist_node hlist;
};

struct fstb_powerfps_list {
	int pid;
	int fps;
};

struct fstb_notifier_push_tag {
	int tgid;
	int pid;
	int target_fps_policy;
	int target_fps_margin;
	int eara_is_active;
	int only_detect;
	unsigned long long bufid;
	unsigned long long cur_queue_end_ts;

	struct work_struct sWork;
};

struct fstb_app_time_info {
	int pid;
	int app_self_ctrl_time_num;
	int app_self_ctrl_time_update;
	unsigned long long bufid;
	unsigned long long ts;
	unsigned long long app_self_ctrl_time[FRAME_TIME_BUFFER_SIZE];
	struct rb_node rb_node;
};

struct fstb_user_target_hint {
	int mode;
	int tgid;
	int pid;
	unsigned long long bufid;
	int target_fps_hint[MAX_USER_TARGET_PRIO];
	unsigned long long target_time_hint[MAX_USER_TARGET_PRIO];
	struct hlist_node hlist;
};

#endif
