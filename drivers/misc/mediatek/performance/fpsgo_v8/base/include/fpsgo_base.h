/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __FPSGO_BASE_H__
#define __FPSGO_BASE_H__

#include <linux/compiler.h>
#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/sched/task.h>
#include <linux/sched.h>
#include <linux/kobject.h>

#define FPSGO_VERSION_CODE 8
#define FPSGO_VERSION_MODULE "8.0"
#define MAX_DEP_NUM 100
#define WINDOW 20
#define RESCUE_TIMER_NUM 5
#define QUOTA_MAX_SIZE 300
#define GCC_MAX_SIZE 300
#define LOADING_CNT 256
#define FBT_FILTER_MAX_WINDOW 100
#define FPSGO_MW 1
#define FPSGO_DYNAMIC_WL 0
#define BY_PID_DEFAULT_VAL -1
#define BY_PID_DELETE_VAL -2
#define FPSGO_MAX_RECYCLE_IDLE_CNT 10
#define FPSGO_MAX_TREE_SIZE 10
#define FPSGO_MAX_WAIT_ENABLE_INFO_SIZE 1000
#define FPSGO_MAX_NO_BOOST_INFO_SIZE 100
#define FPSGO_MAX_JANK_DETECTION_INFO_SIZE 5
#define FPSGO_MAX_JANK_DETECTION_BOOST_CNT 2
#define MAX_SF_BUFFER_SIZE 10

enum {
	FPSGO_SET_UNKNOWN = -1,
	FPSGO_SET_BOOST_TA = 0,
};

enum FPSGO_FRAME_TYPE {
	NON_VSYNC_ALIGNED_TYPE = 0,
	BY_PASS_TYPE = 1,
	USER_FRAME_TYPE = 2,
};

enum FPSGO_CONNECT_API {
	WINDOW_DISCONNECT = 0,
	NATIVE_WINDOW_API_EGL = 1,
	NATIVE_WINDOW_API_CPU = 2,
	NATIVE_WINDOW_API_MEDIA = 3,
	NATIVE_WINDOW_API_CAMERA = 4,
};

enum FPSGO_FORCE {
	FPSGO_FORCE_OFF = 0,
	FPSGO_FORCE_ON = 1,
	FPSGO_FREE = 2,
};

enum FPSGO_RENDER_INFO_HWUI {
	RENDER_INFO_HWUI_UNKNOWN = 0,
	RENDER_INFO_HWUI_TYPE = 1,
	RENDER_INFO_HWUI_NONE = 2,
};

enum FPSGO_CAMERA_CMD {
	CAMERA_CLOSE = 0,
	CAMERA_APK = 1,
	CAMERA_SERVER = 2,
	CAMERA_DO_FRAME = 4,
	CAMERA_APP_MIN_FPS = 5,
	CAMERA_APP_SELF_CTRL = 7,
};

enum FPSGO_MASTER_TYPE {
	FPSGO_TYPE,
	USER_TYPE,
	KTF_TYPE
};

enum FPSGO_BASE_KERNEL_NODE {
	FPSGO_GENERAL_ENABLE = 0,
	FPSGO_FORCE_ONOFF = 1,
	RENDER_INFO_SHOW = 2,
	ACQUIRE_INFO_SHOW = 5,
	RENDER_TYPE_SHOW = 6,
	RENDER_LOADING_SHOW = 7,
	RENDER_INFO_PARAMS_SHOW = 8,
	RENDER_ATTR_PARAMS_SHOW = 9,
	RENDER_ATTR_PARAMS_TID_SHOW = 10,
	FPSGO_GET_ACQUIRE_HINT_ENABLE = 11,
	KFPS_CPU_MASK = 12,
	PERFSERV_TA = 13,
};

enum FPSGO_STRUCTURE_TYPE {
	FBT_RENDER_INFO = 0,
	FSTB_RENDER = 1,
	XGF_RENDER = 2,
	ACQUIRE_INFO = 5,
	HWUI_INFO = 6,
	FPSGO_CONTROL_INFO = 8,
	FPSGO_ATTR_BY_PID = 9,
	FPSGO_ATTR_BY_TID = 10,
	MAX_FPSGO_STRUCTURE_TYPE_NUM
};

/* composite key for render_info rbtree */
struct fbt_render_key {
	int key1;
	unsigned long long key2;
};

struct fbt_jerk {
	int id;
	int jerking;
	int postpone;
	int last_check;
	unsigned long long frame_qu_ts;
	struct hrtimer timer;
	struct work_struct work;
};

struct fbt_proc {
	int active_jerk_id;
	struct fbt_jerk jerks[RESCUE_TIMER_NUM];
};

struct fbt_frame_info {
	unsigned long long running_time;
};

struct fbt_loading_info {
	int target_fps;
	long loading;
	int index;
};

struct fpsgo_loading {
	int pid;
	int loading;
	int prefer_type;
	int ori_ls;		/* original ls flag */
	int policy;
	int ori_nice;	/* original nice */
	int is_vip;    /* is priority vip set */
	int is_vvip;   /* is vvip set */
	int is_prefer; /* is preferred by gear-hint */
	int action;
	int rmidx;
	int heavyidx;
	int reset_taskmask;
	int prio;	/* magt hint prio */
	int timeout; /* magt hint time out */
	unsigned long long latest_runtime;
};

struct fbt_thread_blc {
	int pid;
	unsigned long long buffer_id;
	unsigned int blc;
	unsigned int blc_b;
	unsigned int blc_m;
	int tp_set;
	int b_vip_set;
	int dep_num;
	struct fpsgo_loading dep[MAX_DEP_NUM];
	struct list_head entry;
};

struct fbt_ff_info {
	struct fbt_loading_info filter_loading[FBT_FILTER_MAX_WINDOW];
	struct fbt_loading_info filter_loading_b[FBT_FILTER_MAX_WINDOW];
	struct fbt_loading_info filter_loading_m[FBT_FILTER_MAX_WINDOW];

	int filter_index;
	int filter_index_b;
	int filter_index_m;
	unsigned int filter_frames_count;
	unsigned int filter_frames_count_b;
	unsigned int filter_frames_count_m;
};

struct frame_loading_info {
	struct list_head list;
	int pid;
	unsigned long long runtime;
	unsigned long long duration;
};

struct fbt_separate_ctrl {
	int count_frame;
	int count_loading;
	int th_cali;
	int lc_pid;
	unsigned long long prev_window;
	unsigned long long prev_ts;
	unsigned long long prev_runtime;
	struct list_head loading_list;
};

struct fbt_boost_info {
	int target_fps;
	unsigned long long target_time;
	unsigned long long t2wnt;
	unsigned int last_blc;
	unsigned int last_blc_b;
	unsigned int last_blc_m;
	unsigned int last_normal_blc;
	unsigned int last_normal_blc_b;
	unsigned int last_normal_blc_m;

	/* adjust loading */
	int loading_weight;
	int weight_cnt;
	int hit_cnt;
	int deb_cnt;
	int hit_cluster;
	struct fbt_frame_info frame_info[WINDOW];
	int f_iter;

	/* SeparateCap */
	long *cl_loading;

	/* Separate Ctrl Closed Loop for Second Group */
	unsigned long long target_time_m;
	struct fbt_separate_ctrl sep_ctrl_info;

	/* rescue*/
	struct fbt_proc proc;
	int cur_stage;

	/* filter heavy frames */
	struct fbt_ff_info ff_obj;

	/* quota */
	long long quota_raw[QUOTA_MAX_SIZE];
	int quota_cnt;
	int quota_cur_idx;
	int quota_fps;
	int quota;
	int quota_adj; /* remove outlier */
	int quota_mod; /* mod target time */
	int enq_raw[QUOTA_MAX_SIZE];
	int enq_sum;
	int enq_avg;
	int deq_raw[QUOTA_MAX_SIZE];
	int deq_sum;
	int deq_avg;

	/* GCC */
	int gcc_quota;
	int gcc_count;
	int gcc_target_fps;
	int correction;
	int quantile_cpu_time;
	int quantile_gpu_time;

	/* FRS */
	unsigned long long t_duration;

	/* Closed loop */
	unsigned long long last_target_time_ns;
};

struct fbt_powerRL_limit {
	int uclamp;
	int ruclamp;
	int uclamp_m;
	int ruclamp_m;
};

struct FSTB_FRAME_L2Q_INFO {
	int pid;
	unsigned long long buf_id;
	unsigned long long sf_buf_id;
	unsigned long long queue_end_ns;
	unsigned long long logic_head_ts;
	unsigned long long logic_head_fixed_ts;
	unsigned long long l2l_ts;
	unsigned long long l2q_ts;
	unsigned int is_logic_head_alive;

	unsigned int frame_id;
	unsigned long long logic_head_sys_ts;
	unsigned long long render_head_sys_ts;
	unsigned long long queue_end_sys_ns;
	unsigned int is_magt_l2q_enabled;
};

struct render_info {
	struct rb_node render_key_node;
	struct rb_node linger_node;
	struct hlist_node render_list_node;

	/*render basic info pid bufferId..etc*/
	int pid;
	struct fbt_render_key render_key; /*pid,identifier*/
	unsigned long long buffer_id;
	int queue_SF;
	int tgid;	/*render's process pid*/
	int api;	/*connected API*/
	int buffer_count;
	int frame_type;
	int target_render_flag;
	int producer_info_ready;
	int hwui;
	unsigned long long render_last_cb_ts;
	unsigned long master_type;

	/*render queue/dequeue/frame time info*/
	unsigned long long t_enqueue_start;
	unsigned long long t_enqueue_end;
	unsigned long long t_dequeue_start;
	unsigned long long t_dequeue_end;
	unsigned long long enqueue_length;
	unsigned long long enqueue_length_real;
	unsigned long long dequeue_length;
	unsigned long long prev_t_enqueue_end;
	unsigned long long Q2Q_time;
	unsigned long long running_time;
	unsigned long long raw_runtime;
	unsigned long long idle_time_b_us;
	unsigned long long wall_b_runtime_us;
	long frame_aa;
	long dep_aa;

	/*fbt*/
	int linger;
	struct fbt_boost_info boost_info;
	struct fbt_thread_blc *p_blc;
	struct fpsgo_loading *dep_arr;
	int dep_valid_size;
	unsigned long long dep_loading_ts;
	unsigned long long linger_ts;
	int avg_freq;
	unsigned long long buffer_quota_ts;
	int buffer_quota;

	/*powerRL*/
	struct rb_root pmu_info_tree;
	struct fbt_powerRL_limit powerRL;

	struct mutex thr_mlock;

	int bypass_closed_loop;
	unsigned long long sum_cpu_time_us;
	unsigned long long sum_q2q_time_us;
	unsigned long long sum_reset_ts;

	/* boost policy */
	struct fpsgo_boost_attr attr;

	/* touch latency */
	struct FSTB_FRAME_L2Q_INFO l2q_info[MAX_SF_BUFFER_SIZE];
	int l2q_index;
	unsigned long long logic_head_ts;
	int has_logic_head;
	int is_logic_valid;

	int target_fps_origin;
};

struct acquire_info {
	int p_pid;
	int c_pid;
	int c_tid;
	int api;
	unsigned long long buffer_id;
	unsigned long long ts;
	struct rb_node entry;
	struct hlist_node list_node;
	struct fbt_render_key key;
};

struct hwui_info {
	int pid;
	struct rb_node entry;
};

struct no_boost_info {
	int mode;  // 0 by specific process, 1 by specific task
	int specific_id;
	struct rb_node entry;
};

struct jank_detection_info {
	int pid;
	int rm_count;
	struct rb_node rb_node;
};

struct jank_detection_hint {
	int jank;
	int pid;
	struct work_struct sWork;
};

struct fps_control_info {
	int pid;
	unsigned long long buffer_id;
	unsigned long long ts;
	struct fbt_render_key key;
	struct rb_node entry;
};

struct wait_enable_info {
	int pid;
	int wait_cond;
	struct wait_queue_head wait_q;
	struct rb_node entry;
};

#ifdef FPSGO_DEBUG
#define FPSGO_LOGI(...)	pr_debug("FPSGO:" __VA_ARGS__)
#else
#define FPSGO_LOGI(...)
#endif
#define FPSGO_LOGE(...)	pr_debug("FPSGO:" __VA_ARGS__)

long long fpsgo_task_sched_runtime(struct task_struct *p);
long fpsgo_sched_setaffinity(pid_t pid, const struct cpumask *in_mask);
void *fpsgo_alloc_atomic(int i32Size);
void fpsgo_free(void *pvBuf, int i32Size);
unsigned long long fpsgo_get_time(void);
int fpsgo_arch_nr_clusters(void);
int fpsgo_arch_nr_get_opp_cpu(int cpu);
int fpsgo_arch_nr_max_opp_cpu(void);

int fpsgo_get_tgid(int pid);
void fpsgo_render_tree_lock(const char *tag);
void fpsgo_render_tree_unlock(const char *tag);
void fpsgo_thread_lock(struct mutex *mlock);
void fpsgo_thread_unlock(struct mutex *mlock);
void fpsgo_lockprove(const char *tag);
void fpsgo_thread_lockprove(const char *tag, struct mutex *mlock);
int fpsgo_get_lr_pair(unsigned long long sf_buffer_id,
	unsigned long long *cur_queue_ts,
	unsigned long long *l2q_ns, unsigned long long *logic_head_ts,
	unsigned int *is_logic_head_alive,
	unsigned long long *now_ts);
#if FPSGO_MW
struct fpsgo_attr_by_pid *fpsgo_find_attr_by_pid(int pid, int add_new);
void delete_attr_by_pid(int tgid);
void fpsgo_reset_render_attr(int pid, int set_by_tid);
int is_to_delete_fpsgo_attr(struct fpsgo_attr_by_pid *fpsgo_attr);
struct fpsgo_attr_by_pid *fpsgo_find_attr_by_tid(int pid, int add_new);
void delete_attr_by_tid(int tid);
int is_to_delete_fpsgo_tid_attr(struct fpsgo_attr_by_pid *fpsgo_attr);
#endif  // FPSGO_MW
struct render_info *eara2fpsgo_search_render_info(int pid,
		unsigned long long buffer_id);
void fpsgo_delete_render_info(struct render_info *iter);
struct render_info *fpsgo_search_and_add_render_info(int pid,
		unsigned long long identifier, int force);
struct hwui_info *fpsgo_search_and_add_hwui_info(int pid, int force);
struct fps_control_info *fpsgo_search_and_add_fps_control_info(int mode, int pid,
	unsigned long long buffer_id, int force);
void fpsgo_delete_fps_control_info(int mode, int pid, unsigned long long buffer_id);
int fpsgo_get_all_fps_control_info(int mode, int max_count, struct fps_control_info *arr);
int fpsgo_check_render_info_status(void);
void fpsgo_clear(void);
struct acquire_info *fpsgo_add_acquire_info(int p_pid, int c_pid, int c_tid,
	int api, unsigned long long buffer_id, unsigned long long ts);
struct acquire_info *fpsgo_search_acquire_info(int tid, unsigned long long buffer_id);
int fpsgo_delete_acquire_info(int mode, int tid, unsigned long long buffer_id);
struct wait_enable_info *fpsgo_search_and_add_wait_enable_info(int pid, int create);
int fpsgo_check_wait_enable_info_status(void);
void fpsgo_wake_up_all_wait_enable_info(void);
int fpsgo_get_all_render_info(struct hlist_head *arr);
int fpsgo_get_all_acquire_info(struct hlist_head *arr);
void fpsgo_main_trace(const char *fmt, ...);
void fpsgo_clear_uclamp_boost(void);
void fpsgo_del_linger(struct render_info *thr);
int fpsgo_base_is_finished(struct render_info *thr);
int fpsgo_update_swap_buffer(int pid);
void fpsgo_sentcmd(int cmd, int value1, int value2);
void fpsgo_ctrl2base_get_pwr_cmd(int *cmd, int *value1, int *value2);
void fpsgo_stop_boost_by_render(struct render_info *r);
int fpsgo_check_fbt_jerk_work_addr_invalid(struct work_struct *target_work);
struct no_boost_info *fpsgo_get_no_boost_info(int mode, int id, int create);
void fpsgo_delete_no_boost_info(int mode, int id);
int fpsgo_get_all_no_boost_info(int max_count, struct no_boost_info *arr);

/*
 * TODO(CHI): fpsgo_get_jank_detection_info(), fpsgo_delete_jank_detection_info(), fpsgo_check_jank_detection_info_status()
 *            need to remove, jank detection need to notify FPSGO specific task no boost
 *            via fpsgo_get_no_boost_info() and fpsgo_delete_no_boost_info(),
 *            and then boost specific task by itself
 */
struct jank_detection_info *fpsgo_get_jank_detection_info(int pid, int create);
void fpsgo_delete_jank_detection_info(struct jank_detection_info *iter);
void fpsgo_check_jank_detection_info_status(void);

int fpsgo_get_render_info_by_tgid(int tgid, int *pid_arr,
	unsigned long long *bufID_arr, int max_num);
int fpsgo_fbt_delete_power_rl(int pid, unsigned long long buf_id);

void fpsgo_ktf_test_read_node(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf,
	ssize_t (*target_func)(struct kobject *, struct kobj_attribute *, char *));
void fpsgo_ktf_test_write_node(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf,
	ssize_t (*target_func)(struct kobject *, struct kobj_attribute *,
			const char *, size_t));

int init_fpsgo_common(void);

#endif

