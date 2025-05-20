/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __FPSGO_USEDEXT_H__
#define __FPSGO_USEDEXT_H__

extern int (*fpsgo_notify_qudeq_fp)(int qudeq, unsigned int startend, int pid,
		unsigned long long identifier, unsigned long long sf_buf_id);
extern void (*fpsgo_notify_vsync_fp)(void);
extern void (*fpsgo_notify_vsync_period_fp)(unsigned long long period);
extern void (*fpsgo_notify_swap_buffer_fp)(int pid);
extern void (*power2fpsgo_get_fps_fp)(int *pid, int *fps);
extern void (*fpsgo_get_cmd_fp)(int *cmd, int *value1, int *value2);
extern void (*fpsgo_notify_acquire_fp)(int c_pid, int p_pid,
	int connectedAPI, unsigned long long buffer_id);
extern void (*fpsgo_notify_buffer_quota_fp)(int pid, int quota,
		unsigned long long identifier);

extern int (*fpsgo_get_enable_signal_fp)(int tgid, int wait, int *ret);
extern void (*fpsgo_notify_producer_info_fp)(int ipc_tgid, int pid, int connectedAPI,
	int queue_SF, unsigned long long buffer_id);

extern int (*magt2fpsgo_notify_target_fps_fp)(int *pid_arr, int *tid_arr,
	int *tfps_arr, int num);
extern int (*magt2fpsgo_notify_dep_list_fp)(int pid, void *dep_task_arr,
	int dep_task_num);

int fpsgo_is_force_enable(void);
void fpsgo_force_switch_enable(int enable);

int fpsgo_get_kfpsgo_tid(void);

extern int (*xgff_frame_startend_fp)(unsigned int startend,
		unsigned int tid,
		unsigned long long queueid,
		unsigned long long frameid,
		unsigned long long *cputime,
		unsigned int *area,
		unsigned int *pdeplistsize,
		unsigned int *pdeplist);
extern void (*xgff_frame_getdeplist_maxsize_fp)
		(unsigned int *pdeplistsize);
extern void (*xgff_frame_min_cap_fp)(unsigned int min_cap);

extern int (*fpsgo_get_lr_pair_fp)(unsigned long long sf_buffer_id,
	unsigned long long *cur_queue_ts,
	unsigned long long *l2q_ns, unsigned long long *logic_head_ts,
	unsigned int *is_logic_head_alive, unsigned long long *now_ts);
extern void (*fpsgo_set_rl_expected_l2q_us_fp)(int vsync_multiple,
	unsigned long long user_expected_l2q_us);
extern void (*fpsgo_set_rl_l2q_enable_fp)(int enable);
extern int (*fpsgo_get_now_logic_head_fp)(unsigned long long sf_buffer_id,
	int *pid, unsigned long long *logic_head_ts, unsigned int *is_logic_head_alive,
	unsigned long long *now_ts);


extern int (*fpsgo_other2comp_get_render_fw_info_fp)(int mode, int max_num,
		int *num, struct render_fw_info *arr);
extern void (*fpsgo_other2comp_flush_acquire_table_fp)(void);
extern unsigned long long (*fpsgo_other2fstb_get_app_self_ctrl_time_fp)(int pid,
	unsigned long long bufID);
extern int (*fpsgo_other2fstb_get_fps_info_fp)(int pid, unsigned long long bufID,
	struct render_fps_info *info);
extern int (*fpsgo_other2xgf_get_critical_tasks_fp)(int pid, int max_num,
	struct task_info *arr, int filter_non_cfs, unsigned long long bufID);

#endif
