/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __FPS_COMPOSER_H__
#define __FPS_COMPOSER_H__

#include <linux/rbtree.h>

enum FPSGO_COM_KERNEL_NODE {
	BYPASS_NON_SF_GLOBAL,
	BYPASS_NON_SF_BY_PID,
	CONTROL_API_MASK_GLOBAL,
	CONTROL_API_MASK_BY_PID,
	CONTROL_HWUI_GLOBAL,
	CONTROL_HWUI_BY_PID,
	FPSGO_CONTROL_GLOBAL,
	FPSGO_CONTROL_BY_PID,
	FPS_ALIGN_MARGIN,
	FPSGO_COM_POLICY_CMD,
	IS_FPSGO_BOOSTING,
};

struct fpsgo_com_policy_cmd {
	int tgid;
	int bypass_non_SF_by_pid;
	int control_api_mask_by_pid;
	int dep_loading_thr_by_pid;
	int cam_bypass_window_ms_by_pid;
	unsigned long long ts;
	struct rb_node rb_node;
};

int fpsgo_composer_init(void);
void fpsgo_composer_exit(void);

void fpsgo_ctrl2comp_dequeue_end(int pid,
			unsigned long long dequeue_end_time,
			unsigned long long identifier,
			unsigned long long sf_buf_id);
void fpsgo_ctrl2comp_dequeue_start(int pid,
			unsigned long long dequeue_start_time,
			unsigned long long identifier);
void fpsgo_ctrl2comp_enqueue_end(int pid,
			unsigned long long enqueue_end_time,
			unsigned long long identifier,
			unsigned long long sf_buf_id);
void fpsgo_ctrl2comp_enqueue_start(int pid,
			unsigned long long enqueue_start_time,
			unsigned long long identifier);
void fpsgo_ctrl2comp_acquire(int p_pid, int c_pid, int c_tid,
	int api, unsigned long long buffer_id, unsigned long long ts);
void fpsgo_ctrl2comp_buffer_count(int pid, int buffer_count,
	unsigned long long buffer_id);
void fpsgo_com_notify_fpsgo_is_boost(int enable);
void fpsgo_notify_frame_info_callback(int pid, unsigned long cmd,
	unsigned long long buffer_id, struct render_frame_info *r_iter);

int notify_fpsgo_touch_latency_ko_ready(void);
int fpsgo_get_now_logic_head(unsigned long long sf_buffer_id,
	int *pid, unsigned long long *logic_head_ts, unsigned int *is_logic_head_alive,
	unsigned long long *now_ts);

int fpsgo_ctrl2comp_get_receive_fw_info_enable(void);
int fpsgo_ctrl2comp_wait_receive_fw_info_enable(int tgid, int *ret);
void fpsgo_ctrl2comp_producer_info(int ipc_tgid, int pid, int api, int queue_SF,
	unsigned long long buffer_id);

#endif

