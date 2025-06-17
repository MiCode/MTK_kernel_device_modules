/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SBE_BASE_H__
#define __SBE_BASE_H__

#include <linux/rbtree.h>

#define TARGET_UNLIMITED_FPS 240
#define MAX_TASK_NUM 100
#define HWUI_MAX_FRAME_SAME_TIME 5
#define MAX_SBE_SPID_LOADING_SIZE 10
#define MAX_SBE_RECYCLE_IDLE_CNT 5
#define MAX_PROCESS_NAME_LEN 16
#define SBE_DEFAULT_TARGET_FPS 60
#define SBE_HWUI_BUFFER_ID 5566
#define SBE_HWUI_VIRTUAL_BUFFER_ID 11223344
#define SBE_DEFAULT_DEUQUE_MARGIN_TIME_NS 2500000
#define SBE_DEFAULT_DEUQUE_MARGIN_MAX_TIME_NS 8333333
#define SBE_DEFAULT_AFFINITY_TASK_MIN_CAP 7
#define SBE_DEFAULT_AFFINITY_TASK_LOW_THRESHOLD_CAP 30

#define IS_BIT_SET(mask, bit) (test_bit((bit), &(mask)))

enum SBE_ERROR_STATUS {
	SBE_SUCCESS = 0,
	SBE_INPUT_ERROR,
	SBE_COPY_STR_ERROR,
	SBE_PID_NOT_FIND,
};

enum SBE_TASK_POLICY {
	SBE_TASK_DISABLE = 0,
	SBE_TASK_ENABLE = 1,
};

struct sbe_info {
	int pid;
	int ux_crtl_type;
	int ux_scrolling;
	struct rb_node entry;
};

struct sbe_render_info {
	int pid;
	int tgid;
	int buffer_count_filter;
	int rescue_more_count;
	int sbe_dy_enhance_f;
	int cur_buffer_count;
	int max_buffer_count;
	int sbe_rescuing_frame_id;
	int hwui_arr_idx;
	int sbe_enhance;
	int type;
	int scroll_status;
	int ux_blc_next;
	int ux_blc_cur;
	int dpt_policy_enable;
	int dpt_policy_force_disable;
	int target_fps;
	int dep_self_ctrl;
	int dep_num;
	int dep_arr[MAX_TASK_NUM];
	int aff_dep_arr[MAX_TASK_NUM];
	int ai_boost;
	int ai_boost_ctl;
	int frame_count;
	int frame_cap_count;
	int dy_compute_rescue;
	int affinity_task_mask;
	int affinity_task_mask_cnt;
	int calculate_dy_enhance_idx;
	int ux_affinity_task_basic_cap;
	int critical_basic_cap;
	int loading_type;
	int peak_frame_count;
	int jank_count;
	int is_webfunctor;
	int core_ctl_ignore_vip_task;
	int fpsgo_critical_flag;
	unsigned long long frame_ctime_count;
	unsigned int sbe_rescue;
	unsigned long long buffer_id;
	unsigned long long frame_time;
	unsigned long long target_time;
	unsigned long long ema_running_time;
	unsigned long long raw_running_time;
	unsigned long long sbe_rescue_target_time;
	unsigned long long rescue_start_time;
	unsigned long long t_last_start;
	unsigned long long latest_use_ts;
	struct list_head scroll_list;
	struct rb_root ux_frame_info_tree;
	struct rb_node entry;
	struct mutex ux_mlock;
	struct hwui_frame_info *tmp_hwui_frame_info_arr[HWUI_MAX_FRAME_SAME_TIME];
	struct ux_rescue_check *ux_rchk;
};

struct sbe_spid_loading {
	int tgid;
	int spid_arr[MAX_TASK_NUM];
	unsigned long long spid_latest_runtime[MAX_TASK_NUM];
	int spid_num;
	unsigned long long ts;
	struct rb_node rb_node;
};

void sbe_trace(const char *fmt, ...);
void sbe_systrace_c(pid_t pid, unsigned long long bufID,
	int val, const char *fmt, ...);
unsigned long long sbe_get_time(void);
void sbe_set_display_rate(int fps);
int sbe_get_display_rate(void);
int sbe_get_tgid(int pid);
int sbe_arch_nr_clusters(void);
int sbe_arch_nr_get_opp_cpu(int cpu);
int sbe_arch_nr_max_opp_cpu(void);
int sbe_get_kthread_tid(void);
void sbe_get_tree_lock(const char *tag);
void sbe_put_tree_lock(const char *tag);
struct sbe_info *sbe_get_info(int pid, int force);
void sbe_delete_info(int pid);
int sbe_check_info_status(void);
struct sbe_render_info *sbe_get_render_info(int pid,
	unsigned long long buffer_id, int force);
struct sbe_render_info *sbe_get_render_info_by_thread_name(int tgid, char *thread_name);
int sbe_check_render_info_status(void);
int sbe_check_spid_loading_status(void);
int sbe_delete_spid_loading(int tgid);
int sbe_update_spid_loading(int *cur_pid_arr, int cur_pid_num, int tgid);
int sbe_query_spid_loading(void);
int sbe_split_task_name(int tgid, char *dep_name, int dep_num, int *out_tid_arr, const char *caller);
int sbe_split_task_tid(char *dep_name, int dep_num, int *out_tid_arr, const char *caller);
int sbe_base_init(void);
void sbe_base_exit(void);
void sbe_get_proc_name(int tgid, char *name);
int sbe_forece_reset_fpsgo_critical_tasks(void);
int sbe_get_render_tid_by_render_pid(int tgid, int pid,
	int *out_tid_arr, unsigned long long *out_bufID_arr,
	int *out_tid_num, int out_tid_max_num);

#endif