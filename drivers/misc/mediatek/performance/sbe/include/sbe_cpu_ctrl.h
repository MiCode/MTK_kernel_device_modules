/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/rbtree.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>

#define SBE_AFFNITY_TASK 0
/*define cpu mask, maybe change setting to DTS not here*/
#define SBE_PREFER_NONE 255
#define SBE_PREFER_BIG 128
#define SBE_PREFER_LM 127
#define SBE_PREFER_M 112
#define SBE_PREFER_L 15

//high 16bit for frame status
#define FRAME_MASK 0xFFFF0000
#define FRAME_STATUS_OBTAINVIEW    (1 << 0)
#define FRAME_STATUS_INFLATE       (1 << 1)
#define FRAME_STATUS_INPUT         (1 << 2)
#define FRAME_STATUS_FLING         (1 << 3)
#define FRAME_STATUS_OTHERS        (1 << 4)
//low 16bit for rescue reason
#define RESCUE_MASK 0x0000FFFF
#define RESCUE_WAITING_ANIMATION_END      (1 << 0)
#define RESCUE_TRAVERSAL_OVER_VSYNC       (1 << 1)
#define RESCUE_RUNNING                    (1 << 2)
#define RESCUE_COUNTINUE_RESCUE           (1 << 3)
#define RESCUE_WAITING_VSYNC              (1 << 4)
#define RESCUE_TYPE_TRAVERSAL_DYNAMIC     (1 << 5)
#define RESCUE_TYPE_OI                    (1 << 6)
#define RESCUE_TYPE_MAX_ENHANCE           (1 << 7)
#define RESCUE_TYPE_SECOND_RESCUE         (1 << 8)
#define RESCUE_TYPE_BUFFER_FOUNT_FITLER   (1 << 9)
#define RESCUE_TYPE_PRE_ANIMATION         (1 << 10)
#define RESCUE_TYPE_ENABLE_MARGIN         (1 << 11)
#define RESCUE_TYPE_AI_RESCUE             (1 << 12)

/*define render loading */
#define RENDER_LOADING_LOW         (1 << 0)
#define RENDER_LOADING_MEDUIM      (1 << 1)
#define RENDER_LOADING_HIGH        (1 << 2)
#define RENDER_LOADING_PEAK        (1 << 3)

extern void set_task_basic_vip(int pid);
extern void unset_task_basic_vip(int pid);
extern void set_task_ls_prefer_cpus(int pid, unsigned int cpumask_val);
extern void unset_task_ls_prefer_cpus(int pid);
extern int set_tgid_vip(int tgid);
extern int unset_tgid_vip(int tgid);
extern void turn_on_tgid_vip(void);
extern void turn_off_tgid_vip(void);

void sbe_do_frame_start(struct sbe_render_info *thr, unsigned long long frameid, unsigned long long ts);
void sbe_do_frame_end(struct sbe_render_info *thr, unsigned long long frameid,
		unsigned long long start_ts, unsigned long long end_ts);
void sbe_do_frame_err(struct sbe_render_info *thr, int frame_count,
		unsigned long long frameID,unsigned long long ts);
void sbe_ux_reset(struct sbe_render_info *thr);
void sbe_set_per_task_cap(struct sbe_render_info *thr);
int sbe_set_sbb(int pid, int set, int active_ratio);
void sbe_set_dptv2_policy(struct sbe_render_info *thr, int start);
int sbe_get_fpsgo_info(int tgid, int pid, int blc, unsigned long mask, int jerk_boost_flag, struct task_info *dep_arr);
void sbe_reset_frame_cap(struct sbe_render_info *thr);
void sbe_notify_fpsgo_do_virtual_boost(int enable, int tgid, int render_tid);
void  __sbe_set_per_task_cap(struct sbe_render_info *thr, int min_cap, int max_cap);
int get_sbe_critical_basic_cap(void);
void sbe_core_ctl_ignore_vip_task(struct sbe_render_info *thr, int ignore_enable);

struct ux_frame_info {
	unsigned long long frameID;
	unsigned long long start_ts;
	struct rb_node entry;
};

struct ux_rescue_check {
	unsigned long long pid;
	unsigned long long frameID;
	int rescue_type;
	unsigned long long rsc_hint_ts;
	struct hrtimer timer;
	struct work_struct work;
};

struct ux_scroll_info {
	struct list_head queue_list;
	struct list_head frame_list;
	unsigned long long start_ts;
	unsigned long long end_ts;
	unsigned long long dur_ts;
	int frame_count;
	int frame_cap_count;
	int frame_ctime_count;
	int jank_count;
	int rescue_count;
	int enhance;
	int type;

	int checked;
	int rescue_cap_count;
	int rescue_frame_oi_count;
	unsigned long long rescue_frame_time_count;
	int rescue_frame_count;
	int rescue_with_perf_mode;
	int *score;

	unsigned long long last_frame_ID;
	unsigned long long rescue_filter_buffer_time;
};

struct hwui_frame_info{
	struct list_head queue_list;
	unsigned long long frameID;
	unsigned long long start_ts;
	unsigned long long end_ts;
	unsigned long long dur_ts;
	unsigned long long rsc_start_ts;
	unsigned long long rsc_dur_ts;
	int display_rate;

	int perf_idx;
	int rescue_reason;
	int frame_status;
	bool overtime;
	int drop;
	bool rescue;
	bool rescue_suc;
	bool promotion;
	int using;
};

void sbe_del_ux(struct sbe_render_info *info);
void reset_hwui_frame_info(struct hwui_frame_info *frame);
struct hwui_frame_info *get_valid_hwui_frame_info_from_pool(struct sbe_render_info *info);
struct hwui_frame_info *get_hwui_frame_info_by_frameid(struct sbe_render_info *info, unsigned long long frameid);
struct hwui_frame_info *insert_hwui_frame_info_from_tmp(struct sbe_render_info *thr,
			struct hwui_frame_info *frame);
void count_scroll_rescue_info(struct sbe_render_info *thr, struct hwui_frame_info *hwui_info);

int get_ux_list_length(struct list_head *head);
void sbe_exec_doframe_end(struct sbe_render_info *thr, unsigned long long frame_id,
			long long frame_flags);
void clear_ux_info(struct sbe_render_info *thr);
void enqueue_ux_scroll_info(int type, unsigned long long start_ts, struct sbe_render_info *thr);
struct ux_scroll_info *search_ux_scroll_info(unsigned long long ts, struct sbe_render_info *thr);
int sbe_calculate_dy_enhance(struct sbe_render_info *thr);
void sbe_ux_scrolling_end(struct sbe_render_info *thr);
void sbe_ux_scrolling_start(int type, unsigned long long start_ts, struct sbe_render_info *thr);
void update_fpsgo_hint_param(int scrolling, int tgid);

void sbe_delete_frame_info(struct sbe_render_info *thr, struct ux_frame_info *info);
struct ux_frame_info *sbe_search_and_add_frame_info(struct sbe_render_info *thr,
		unsigned long long frameID, unsigned long long start_ts, int action);
int sbe_count_frame_info(struct sbe_render_info *thr, int target);
void sbe_boost_non_hwui_policy(struct sbe_render_info *thr, int set_vip);
void sbe_set_ux_general_policy(int scrolling, unsigned long ux_mask);
int get_ux_general_policy(void);
void set_sbe_thread_vip(int set_vip, int tgid, char *dep_name, int dep_num);
void sbe_enable_vip_sitch(int start, int tgid);
void sbe_reset_deplist_task_priority(struct sbe_render_info *thr);
void sbe_set_group_dvfs(int start);
void sbe_set_gas_policy(int start);
void update_ux_general_policy(void);
void sbe_set_deplist_policy(struct sbe_render_info *thr, int policy);
int get_sbe_force_bypass_dptv2(void);

void sbe_do_rescue(struct sbe_render_info *thr, int start, int enhance,
		int rescue_type, unsigned long long rescue_target, unsigned long long frame_id);
void sbe_do_rescue_legacy(struct sbe_render_info *thr, int start, int enhance,
		unsigned long long frame_id);

int sbe_get_perf(void);
int get_sbe_disable_runnable_util_est_status(void);
int get_sbe_extra_sub_en_deque_enable(void);
unsigned long long get_sbe_extra_sub_deque_margin_time(void);
void fbt_ux_set_perf(int cur_pid, int cur_blc);
void sbe_set_global_sbe_dy_enhance(int cur_pid, int cur_dy_enhance);
void sbe_register_jank_cb(unsigned long mask);
void sbe_notify_ux_jank_detection(bool enable, int tgid, int pid, unsigned long mask,
					struct sbe_render_info *sbe_thr, unsigned long long buf_id);

extern int group_set_threshold(int grp_id, int val);
extern int group_reset_threshold(int grp_id);
extern int get_dpt_default_status(void);
extern void set_ignore_idle_ctrl(bool val);
extern void set_grp_dvfs_ctrl(int set);
#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
extern void flt_ctrl_force_set(int set);
#endif
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
extern void group_set_mode(u32 mode);
#endif

void __exit sbe_cpu_ctrl_exit(void);
int __init sbe_cpu_ctrl_init(void);
