/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _FRAME_INTERPOLATE_H_
#define _FRAME_INTERPOLATE_H_

extern int fstb_fi_detect_enable;
int init_frame_interpolate(void);
void fpsgo_fi_receive_q2q_cb(unsigned long cmd, struct render_frame_info *iter);
void frame_interpolate_exit(void);
int frame_interpolate_init(void);
void game_clear_render_info(int mode);
int game_switch_frame_inteprolate_onoff(int pid, int fi_info);
extern int (*fstb_get_is_interpolation_is_on_fp)(int pid, unsigned long long bufID, int tgid,
	unsigned long long cur_queue_end_ts, int *target_fps);

extern int register_fpsgo_frame_info_callback(unsigned long mask, fpsgo_frame_info_callback cb);
extern int unregister_fpsgo_frame_info_callback(fpsgo_frame_info_callback cb);
extern int fpsgo_other2comp_user_create(int tgid, int render_tid, unsigned long long buffer_id,
	int *dep_arr, int dep_num, unsigned long long target_time);
extern int fpsgo_other2comp_report_workload(int tgid, int render_tid, unsigned long long buffer_id,
	unsigned long long tcpu, unsigned long long ts);
extern int switch_fpsgo_control(int mode, int pid, int set_ctrl, unsigned long long buffer_id);
extern int fpsgo_other2fstb_get_fps_info(int pid, unsigned long long bufID,
	struct render_fps_info *info);
extern int fpsgo_other2fstb_set_target(int mode, int pid, int use, int priority,
	int target_fps, unsigned long long target_time, unsigned long long bufID);
extern int fpsgo_other2xgf_get_critical_tasks(int pid, int max_num,
	struct task_info *arr, int filter_non_cfs, unsigned long long bufID);
extern int fpsgo_other2xgf_set_critical_tasks(int rpid, unsigned long long bufID,
	struct task_info *arr, int num, int use);
extern void fpsgo_other2xgf_calculate_dep(int pid, unsigned long long bufID,
	unsigned long long *raw_running_time, unsigned long long *ema_running_time,
	unsigned long long *enq_running_time,
	unsigned long long def_start_ts, unsigned long long def_end_ts,
	unsigned long long t_dequeue_start, unsigned long long t_dequeue_end,
	unsigned long long t_enqueue_start, unsigned long long t_enqueue_end,
	int skip);
extern int fpsgo_other2fstb_calculate_target_fps(int policy, int pid,
	unsigned long long bufID, unsigned long long cur_ts);
extern int fpsgo_other2comp_set_timestamp(int tgid, int render_tid, unsigned long long buffer_id,
	int flag, unsigned long long ts);

#endif  // _FRAME_INTERPOLATION_H_
