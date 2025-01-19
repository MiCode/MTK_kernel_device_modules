/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef FSTB_H
#define FSTB_H

enum FPSGO_FSTB_KERNEL_NODE {
	FPSGO_STATUS,
	FSTB_DEBUG,
	FSTB_FPS_LIST,
	FSTB_POLICY_CMD,
	FSTB_TFPS_INFO,
	FSTB_FRS_INFO,
	FSTB_TARGET_FPS_POLICY_ENABLE_GLOBAL,
	FSTB_TARGET_FPS_POLICY_ENABLE_BY_PID,
	NOTIFY_FSTB_TARGET_FPS_BY_PID,
	FSTB_NO_R_TIMER_ENABLE,
	SET_RENDER_MAX_FPS,
	FSTB_MARGIN_MODE,
	FSTB_MARGIN_MODE_DBNC_A,
	FSTB_MARGIN_MODE_DBNC_B,
	FSTB_RESET_TOLERENCE,
	FSTB_TUNE_QUANTILE,
	GPU_SLOWDOWN_CHECK,
	FSTB_SOFT_LEVEL,
};

int mtk_fstb_exit(void);
int mtk_fstb_init(void);
void fpsgo_comp2fstb_queue_time_update(int pid, unsigned long long bufID,
	unsigned long long ts, int hwui_flag);
void fpsgo_comp2fstb_prepare_calculate_target_fps(int pid,
	unsigned long long bufID,
	unsigned long long cur_queue_end_ts);
void fpsgo_comp2fstb_notify_info(int pid, unsigned long long bufID,
	unsigned long long q2q_time, unsigned long long enq_length,
	unsigned long long deq_length);
void fpsgo_ctrl2fstb_get_fps(int *pid, int *fps);
void fpsgo_ctrl2fstb_dfrc_fps(int fps);
void fpsgo_comp2fstb_delete_render_info(int pid, unsigned long long bufID);
int fpsgo_comp2fstb_do_recycle(void);
int fpsgo_comp2fstb_get_logic_head(int pid, unsigned long long bufID, int tgid,
	unsigned long long cur_queue_end, unsigned long long prev_queue_end_ts,
	unsigned long long pprev_queue_end_ts, unsigned long long dequeue_start_ts,
	unsigned long long *logic_head_ts, int *has_logic_head);

int fpsgo_ktf2fstb_add_delete_render_info(int mode, int pid, unsigned long long bufID,
	int target_fps, int queue_fps);

int is_fstb_active(long long time_diff);
int fpsgo_ctrl2fstb_switch_fstb(int value);
int fpsgo_fbt2fstb_update_cpu_frame_info(int pid, unsigned long long bufID,
	long long Runnging_time, unsigned int Curr_cap, unsigned int Max_cap);
void fpsgo_fbt2fstb_query_fps(int pid, unsigned long long bufID,
		int *target_fps, int *target_fps_ori, int *target_cpu_time, int *fps_margin,
		int *quantile_cpu_time, int *quantile_gpu_time,
		int *target_fpks, int *cooler_on);
long long fpsgo_base2fstb_get_gpu_time(int pid, unsigned long long bufID);

/* EARA */
void eara2fstb_get_tfps(int max_cnt, int *is_camera, int *pid, unsigned long long *buf_id,
				int *tfps, int *rftp, int *hwui, char name[][16], int *proc_id);
void eara2fstb_tfps_mdiff(int pid, unsigned long long buf_id, int diff,
				int tfps);

#endif

