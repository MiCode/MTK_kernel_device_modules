/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __FPSGO_FRAME_INFO_H__
#define __FPSGO_FRAME_INFO_H__

#define FPSGO_MAX_CALLBACK_NUM 10
#define FPSGO_MAX_RENDER_INFO_SIZE 30
#define FPSGO_MAX_TASK_NUM 100

enum FPSGO_SET_TIMESTAMP_FLAG {
	FPSGO_DEQUEUE_START = 0,
	FPSGO_DEQUEUE_END = 1,
	FPSGO_ENQUEUE_START = 2,
	FPSGO_ENQUEUE_END = 3,
	FPSGO_BUFFER_QUOTA = 4,
};

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
	GET_FPSGO_Q2Q_TIME = 13,
	GET_FPSGO_QDQ_TS = 14,
	GET_FPSGO_JERK_BOOST = 15,
	GET_FPSGO_QUEUE_START = 16,
	GET_FPSGO_QUEUE_END = 17,
	GET_FPSGO_DEQUEUE_START = 18,
	GET_FPSGO_DEQUEUE_END = 19,
	GET_FPSGO_BUFFER_TIME = 20,
	FPSGO_FRAME_INFO_MAX_NUM
};

enum XGF_ACTION {
	XGF_DEL_DEP = -1,
	XGF_ADD_DEP = 0,
	XGF_ADD_DEP_NO_LLF = 1,
	XGF_ADD_DEP_FORCE_LLF = 2,
	XGF_ADD_DEP_FORCE_CPU_TIME = 3,
	XGF_ADD_DEP_FORCE_GROUPING = 4,
	XGF_FORCE_BOOST = 5,
	XGF_FORCE_L3_CT = 6,
};

struct task_info {
	int pid;
	int loading;
	int action;
	int vip_prio;
	int vip_timeout;
};

struct render_fw_info {
	int consumer_tgid;
	int consumer_pid;
	int producer_tgid;
	int producer_pid;
	int api;
	int hwui;
	int queue_SF;
	int buffer_count;
	int frame_type;
	int boosting;
	unsigned long long buffer_id;
};

struct render_fps_info {
	int queue_fps;
	int raw_target_fps;
	int target_fps_diff;
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
	int jerk_boost_flag;
	long frame_aa;
	long dep_aa;
	long long t_gpu;
	unsigned long long buffer_id;
	unsigned long long raw_t_cpu;
	unsigned long long ema_t_cpu;
	unsigned long long q2q_time;
	unsigned long long t_enqueue_start;
	unsigned long long t_enqueue_end;
	unsigned long long t_dequeue_start;
	unsigned long long t_dequeue_end;
	struct task_info dep_arr[FPSGO_MAX_TASK_NUM];
	struct task_info non_dep_arr[FPSGO_MAX_TASK_NUM];
};

struct fstb_policy_cmd {
	int mode;
	int tgid;
	int pid;
	int target_fps_policy_enable;
	int target_fps_detect_enable;
	unsigned long long bufid;
	unsigned long long ts;
	struct hlist_node hlist;
};

struct xgf_policy_cmd {
	int mode;
	int tgid;
	int pid;
	int ema2_enable;
	int filter_dep_task_enable;
	int calculate_dep_enable;
	int xgf_extra_sub;
	unsigned long long bufid;
	unsigned long long ts;
	struct hlist_node hlist;
};

struct fpsgo_boost_attr {
	/* AA */
	int aa_enable_by_pid;

	/*    rescue    */
	int rescue_enable_by_pid;
	int rescue_second_enable_by_pid;
	int rescue_second_time_by_pid;
	int rescue_second_group_by_pid;

	/*   grouping   */
	int group_by_lr_by_pid;
	int heavy_group_num_by_pid;
	int second_group_num_by_pid;

	/* filter frame */
	int filter_frame_enable_by_pid;
	int filter_frame_window_size_by_pid;
	int filter_frame_kmin_by_pid;

	/* boost affinity */
	int boost_affinity_by_pid;
	int set_soft_affinity_by_pid;
	int cpumask_heavy_by_pid;
	int cpumask_second_by_pid;
	int cpumask_others_by_pid;

	/* L3 cache policy */
	int set_l3_cache_ct_by_pid;

	/* set idle prefer */
	int set_ls_by_pid;
	int ls_groupmask_by_pid;

	/* separate cap */
	int separate_aa_by_pid;
	int limit_uclamp_by_pid;
	int limit_ruclamp_by_pid;
	int limit_uclamp_m_by_pid;
	int limit_ruclamp_m_by_pid;
	int separate_pct_b_by_pid;
	int separate_pct_m_by_pid;
	int separate_pct_other_by_pid;
	int separate_release_sec_by_pid;

	/* limit freq2cap */
	/* limit freq 2 cap */
	int limit_cfreq2cap_by_pid;
	int limit_rfreq2cap_by_pid;
	int limit_cfreq2cap_m_by_pid;
	int limit_rfreq2cap_m_by_pid;

	/* blc boost*/
	int blc_boost_by_pid;

	/* boost VIP */
	int boost_vip_by_pid;
	int vip_mask_by_pid;
	int set_vvip_by_pid;
	int vip_throttle_by_pid;

	/* Gear-Hint Prefer */
	int gh_prefer_by_pid;

	/* Tuning Point Control */
	int bm_th_by_pid;
	int ml_th_by_pid;
	int tp_policy_by_pid;
	int tp_strict_middle_by_pid;
	int tp_strict_little_by_pid;

	/* QUOTA */
	int qr_enable_by_pid;
	int qr_t2wnt_x_by_pid;
	int qr_t2wnt_y_p_by_pid;
	int qr_t2wnt_y_n_by_pid;

	/*  GCC   */
	int gcc_enable_by_pid;
	int gcc_fps_margin_by_pid;
	int gcc_up_sec_pct_by_pid;
	int gcc_down_sec_pct_by_pid;
	int gcc_up_step_by_pid;
	int gcc_down_step_by_pid;
	int gcc_reserved_up_quota_pct_by_pid;
	int gcc_reserved_down_quota_pct_by_pid;
	int gcc_enq_bound_thrs_by_pid;
	int gcc_deq_bound_thrs_by_pid;
	int gcc_enq_bound_quota_by_pid;
	int gcc_deq_bound_quota_by_pid;

	/* Reset taskmask */
	int reset_taskmask;

	/* Closed Loop */
	int check_buffer_quota_by_pid;
	int expected_fps_margin_by_pid;
	int quota_v2_diff_clamp_min_by_pid;
	int quota_v2_diff_clamp_max_by_pid;
	int limit_min_cap_target_t_by_pid;
	int target_time_up_bound_by_pid;
	int l2q_enable_by_pid;
	int l2q_exp_us_by_pid;

	/* sep loading ctrl */
	int sep_loading_ctrl_by_pid;
	int lc_th_by_pid;
	int lc_th_upbound_by_pid;
	int frame_lowbd_by_pid;
	int frame_upbd_by_pid;

	/* Minus idle time*/
	int aa_b_minus_idle_t_by_pid;

	/* power RL */
	int powerRL_enable_by_pid;
	int powerRL_perf_min_by_pid;
	int powerRL_current_cap_by_pid;
};

struct fpsgo_attr_by_pid {
	struct rb_node entry;
	int tgid;	/*for by tgid attribute*/
	int tid;	/*for by tid attribute*/
	unsigned long long ts;
	struct fpsgo_boost_attr attr;
};

typedef void (*fpsgo_frame_info_callback)(unsigned long cmd, struct render_frame_info *iter);

struct render_frame_info_cb {
	unsigned long mask;
	struct render_frame_info info_iter;
	fpsgo_frame_info_callback func_cb;
};

/* filter_bypass-> 0: get all render frame info, 1: filter bypass
 * tgid-> -1: no filter
 */
int fpsgo_ctrl2base_get_render_frame_info(int max_num, unsigned long mask,
	int filter_bypass, int tgid,
	struct render_frame_info *frame_info_arr);

/* FPSGO General API of Composer */
extern int fpsgo_other2comp_get_render_fw_info(int mode, int max_num, int *num, struct render_fw_info *arr);
extern void fpsgo_other2comp_flush_acquire_table(void);
extern int switch_fpsgo_control(int mode, int pid, int set_ctrl, unsigned long long buffer_id);
extern int fbt_set_magt_workaround_passive_mode(int value);
extern int set_fpsgo_attr(int mode, int pid, int set, struct fpsgo_boost_attr *request_boost_attr);
extern int fpsgo_other2comp_set_no_boost_info(int mode, int id, int set);
extern int register_fpsgo_frame_info_callback(unsigned long mask, fpsgo_frame_info_callback cb);
extern int unregister_fpsgo_frame_info_callback(fpsgo_frame_info_callback cb);

extern int fpsgo_other2comp_user_create(int tgid, int render_tid, unsigned long long buffer_id,
	int *dep_arr, int dep_num, unsigned long long target_time);
extern int fpsgo_other2comp_report_workload(int tgid, int render_tid, unsigned long long buffer_id,
	unsigned long long tcpu, unsigned long long ts);
extern int fpsgo_other2comp_set_timestamp(int tgid, int render_tid, unsigned long long buffer_id,
	int flag, unsigned long long ts);
extern void fpsgo_other2comp_control_resume(int render_tid, unsigned long long buffer_id);
extern void fpsgo_other2comp_control_pause(int render_tid, unsigned long long buffer_id);
extern void fpsgo_other2comp_user_close(int tgid, int render_tid, unsigned long long buffer_id);

/* FPSGO General API of FSTB */
extern int fpsgo_other2fstb_get_fps_info(int pid, unsigned long long bufID,
	struct render_fps_info *info);
extern unsigned long long fpsgo_other2fstb_get_app_self_ctrl_time(int pid, unsigned long long bufID);
extern int fpsgo_other2fstb_set_target(int mode, int pid, int use, int priority,
	int target_fps, unsigned long long target_time, unsigned long long bufID);
extern int fpsgo_other2fstb_calculate_target_fps(int policy, int pid,
	unsigned long long bufID, unsigned long long cur_ts);

/* FPSGO General API of XGF */
extern void fpsgo_other2xgf_calculate_dep(int pid, unsigned long long bufID,
	unsigned long long *raw_running_time, unsigned long long *ema_running_time,
	unsigned long long *enq_running_time,
	unsigned long long def_start_ts, unsigned long long def_end_ts,
	unsigned long long t_dequeue_start, unsigned long long t_dequeue_end,
	unsigned long long t_enqueue_start, unsigned long long t_enqueue_end,
	int skip);
extern int fpsgo_other2xgf_set_critical_tasks(int rpid, unsigned long long bufID,
	struct task_info *arr, int num, int use);
extern int fpsgo_other2xgf_get_critical_tasks(int pid, int max_num,
	struct task_info *arr, int filter_non_cfs, unsigned long long bufID);
extern int fpsgo_other2xgf_set_attr(int set, struct xgf_policy_cmd *request_attr);

/* FPSGO General API of FBT */
extern int fpsgo_other2fbt_get_cpu_capacity_area(int cluster, int pid,
	unsigned long long buffer_id, unsigned long long *loading,
	unsigned long long start_ts_ns, unsigned long long end_ts_ns);
extern int fpsgo_other2fbt_calculate_frame_loading(long loading,
		unsigned long long t_cpu, unsigned long long t_q2q, long *aa);
extern unsigned int fpsgo_other2fbt_calculate_blc(long aa, unsigned long long target_time,
	unsigned int blc_when_err, unsigned long long t_q2q, int is_retarget,
	unsigned int *blc_wt);
extern int fpsgo_other2fbt_deq_end(int tgid, int render_tid, unsigned long long buffer_id);

extern int (*powerhal2fpsgo_get_fpsgo_frame_info_fp)(int max_num, unsigned long mask,
	int filter_bypass, int tgid, struct render_frame_info *frame_info_arr);
extern int (*magt2fpsgo_get_fpsgo_frame_info)(int max_num, unsigned long mask,
	int filter_bypass, int tgid, struct render_frame_info *frame_info_arr);

#endif
