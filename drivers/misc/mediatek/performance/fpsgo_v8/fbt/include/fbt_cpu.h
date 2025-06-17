/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __FBT_CPU_H__
#define __FBT_CPU_H__

void fpsgo_ctrl2fbt_cpufreq_cb_cap(int cid, int cap);
void fpsgo_ctrl2fbt_vsync(unsigned long long ts);
void fpsgo_ctrl2fbt_vsync_period(unsigned long long period_ts);
void fpsgo_comp2fbt_frame_start(struct render_info *thr,
		unsigned long long ts);
void fpsgo_comp2fbt_deq_end(struct render_info *thr,
		unsigned long long ts);

int fpsgo_base2fbt_node_init(struct render_info *obj);
void fpsgo_base2fbt_item_del(struct fbt_thread_blc *pblc,
		struct fpsgo_loading *pdep,
		struct render_info *thr);
int fpsgo_base2fbt_get_max_blc_pid(int *pid, unsigned long long *buffer_id);
void fpsgo_base2fbt_check_max_blc(void);
int fpsgo_base2fbt_get_cluster_num(void);
void fpsgo_base2fbt_no_one_render(void);
void fpsgo_base2fbt_set_min_cap(struct render_info *thr, int min_cap,
					int min_cap_b, int min_cap_m);
int fpsgo_base2fbt_is_finished(struct render_info *thr);
void fpsgo_base2fbt_stop_boost(struct render_info *thr);
void eara2fbt_set_2nd_t2wnt(int pid, unsigned long long buffer_id,
		unsigned long long t_duration);
int fpsgo_ctrl2fbt_buffer_quota(unsigned long long ts, int pid, int quota,
	unsigned long long buffer_id);
int fbt_get_rl_ko_is_ready(void);

int __init fbt_cpu_init(void);
void __exit fbt_cpu_exit(void);
void fbt_trace(const char *fmt, ...);

int fpsgo_ctrl2fbt_switch_fbt(int enable);
int fbt_switch_ceiling(int value);

#if !IS_ENABLED(CONFIG_ARM64)
void fbt_update_pwr_tbl(void);
#endif

void fbt_set_render_boost_attr(struct render_info *thr);
void fbt_set_render_last_cb(struct render_info *thr, unsigned long long ts);

void fbt_update_freq_qos_min(int policy_id, unsigned int freq);

void fpsgo_comp2fbt_jank_thread_boost(int boost, int pid);
void fpsgo_base2fbt_jank_thread_deboost(int pid);

void fpsgo_set_rl_l2q_enable(int enable);
void fpsgo_set_expected_l2q_us(int vsync_multiple, unsigned long long user_expected_l2q_us);
int fpsgo_get_rl_l2q_enable(void);
void fbt_task_reset_pmu(struct rb_root *pmu_info_tree, unsigned long long ts);
int fbt_cluster_X2Y(int cluster, unsigned long input, int in_type,
	int out_type, int is_to_scale_cap, const char *caller);

#endif
