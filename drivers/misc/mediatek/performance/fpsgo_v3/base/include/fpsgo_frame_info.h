/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __FPSGO_FRAME_INFO_H__
#define __FPSGO_FRAME_INFO_H__

#define FPSGO_MAX_TASK_NUM 100

enum GET_FPSGO_FRAME_INFO {
	GET_FPSGO_QUEUE_FPS = 0,
	GET_FPSGO_TARGET_FPS = 1,
	GET_FRS_TARGET_FPS_DIFF = 2,
	GET_FPSGO_RAW_CPU_TIME = 3,
	GET_FPSGO_EMA_CPU_TIME = 4,
	GET_FPSGO_DEP_LIST = 5,
	GET_FPSGO_FRAME_AA = 6,
	GET_FPSGO_DEP_AA = 7,
	GET_FPSGO_PERF_IDX = 8,
	GET_FPSGO_AVG_FRAME_CAP = 9,
	GET_FPSGO_MINITOP_LIST = 10,
	GET_FPSGO_DELETE_INFO = 11,
	GET_GED_GPU_TIME = 12,
	FPSGO_FRAME_INFO_MAX_NUM
};

struct task_info {
	int pid;
	int loading;
};

struct render_frame_info {
	int tgid;
	int pid;
	int queue_fps;
	int target_fps;
	int target_fps_diff;
	int avg_frame_cap;
	int dep_num;
	int non_dep_num;
	int blc;
	long frame_aa;
	long dep_aa;
	long long t_gpu;
	unsigned long long buffer_id;
	unsigned long long raw_t_cpu;
	unsigned long long ema_t_cpu;
	struct task_info dep_arr[FPSGO_MAX_TASK_NUM];
	struct task_info non_dep_arr[FPSGO_MAX_TASK_NUM];
};

int fpsgo_ctrl2base_get_render_frame_info(int max_num, unsigned long mask,
	struct render_frame_info *frame_info_arr);

extern int (*magt2fpsgo_get_fpsgo_frame_info)(int max_num, unsigned long mask,
	struct render_frame_info *frame_info_arr);

#endif
