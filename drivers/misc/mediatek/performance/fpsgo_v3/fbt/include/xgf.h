/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _XGF_H_
#define _XGF_H_

#include <linux/rbtree.h>
#include <linux/tracepoint.h>
#include <linux/slab.h>

#define XGF_DEFAULT_EMA_DIVIDEND 5
#define SP_ALLOW_NAME "UnityMain"
#define SP_ALLOW_NAME2 "Thread-"
#define SP_PASS_PROC_NAME_CHECK "*"
#define XGF_DEFAULT_DEP_FRAMES 10
#define XGF_DEP_FRAMES_MIN 2
#define XGF_DEP_FRAMES_MAX 20
#define XGF_MAX_SPID_LIST_LENGTH 50
#define XGF_MAX_POLICY_CMD_NUM 10
#define DEFAULT_MAX_DEP_PATH_NUM 50
#define DEFAULT_MAX_DEP_TASK_NUM 100
#define MAX_XGF_POLICY_CMD_NUM 10

enum XGF_ACTION {
	XGF_DEL_DEP = -1,
	XGF_ADD_DEP = 0,
	XGF_ADD_DEP_NO_LLF = 1,
	XGF_ADD_DEP_FORCE_LLF = 2,
	XGF_ADD_DEP_FORCE_CPU_TIME = 3,
	XGF_ADD_DEP_GROUPING_VIP = 4,
	XGF_FORCE_BOOST = 5,
};

enum XGF_EVENT {
	IRQ_ENTRY,
	IRQ_EXIT,
	SCHED_WAKING,
	SCHED_SWITCH,
	HRTIMER_ENTRY,
	HRTIMER_EXIT
};  // need align fpsgo.ko

enum XGF_BUFFER_TYPE {
	XGF_BUFFER,
	FSTB_BUFFER
}; // need to align ktf

struct fpsgo_trace_event {
	int event;
	int cpu;
	int note;
	int state;
	int pid;
	unsigned long long ts;
	unsigned long long addr;
};  // need align fpsgo.ko

struct xgf_render_if {
	struct hlist_node hlist;
	int tgid;
	int pid;
	int spid;
	unsigned long long bufid;
	unsigned long long prev_queue_end_ts;
	unsigned long long cur_queue_end_ts;
	unsigned long long raw_t_cpu;
	unsigned long long ema_t_cpu;

	struct rb_root dep_list;
	int dep_list_size;
	struct rb_root magt_dep_list;
	int magt_dep_list_size;

	int ema2_enable;
	int filter_dep_task_enable;

	unsigned long master_type;
};

struct xgf_dep {
	struct rb_node rb_node;

	pid_t tid;
	int action;
};

struct xgf_spid {
	struct hlist_node hlist;
	char process_name[16];
	char thread_name[16];
	int pid;
	int rpid;
	int tid;
	unsigned long long bufID;
	int action;
};

struct xgf_policy_cmd {
	struct rb_node rb_node;

	int tgid;
	int ema2_enable;
	int filter_dep_task_enable;
	unsigned long long ts;
};

struct xgf_thread_loading {
	int pid;
	unsigned long long buffer_id;
	unsigned long long loading;
	unsigned long long last_cb_ts;
};

struct xgff_runtime {
	int pid;
	unsigned long long loading;
};

struct xgff_frame {
	struct hlist_node hlist;
	pid_t parent;
	pid_t tid;
	unsigned long long bufid;
	unsigned long frameid;
	unsigned long long ts;
	struct xgf_thread_loading ploading;
	struct xgf_render_if xgfrender;
	struct xgff_runtime dep_runtime[XGF_DEP_FRAMES_MAX];
	int count_dep_runtime;
	int is_start_dep;
};

int __init init_xgf(void);
int __exit exit_xgf(void);
void fpsgo_ctrl2xgf_switch_xgf(int val);
void xgf_trace(const char *fmt, ...);
int xgf_split_dep_name(int tgid, char *dep_name, int dep_num, int *out_tid_arr);

void fpsgo_comp2xgf_qudeq_notify(int pid, unsigned long long bufID,
	unsigned long long *raw_runtime, unsigned long long *run_time,
	unsigned long long *enq_running_time,
	unsigned long long def_start_ts, unsigned long long def_end_ts,
	unsigned long long t_dequeue_start, unsigned long long t_dequeue_end,
	unsigned long long t_enqueue_start, unsigned long long t_enqueue_end,
	int skip);
int fpsgo_comp2xgf_do_recycle(void);
int fpsgo_comp2xgf_get_dep_list_num(int pid, unsigned long long bufID);
int fpsgo_comp2xgf_get_dep_list(int pid, int count,
	int *arr, unsigned long long bufID);
int fpsgo_other2xgf_set_dep_list(int tgid, int *rtid_arr,
	unsigned long long *bufID_arr, int rtid_num,
	char *specific_name, int specific_num, int action);
int fpsgo_comp2xgf_adpf_set_dep_list(int tgid, int rtid, unsigned long long bufID,
	int *dep_arr, int dep_num, int op);
int has_xgf_dep(pid_t tid);
void fpsgo_ctrl2xgf_magt_set_dep_list(int tgid, int *dep_arr, int dep_num, int action);
int fpsgo_ktf2xgf_add_delete_render_info(int mode, int pid, unsigned long long bufID);
struct xgf_thread_loading fbt_xgff_list_loading_add(int pid,
	unsigned long long buffer_id, unsigned long long ts);
long fbt_xgff_get_loading_by_cluster(struct xgf_thread_loading *ploading,
	unsigned long long ts, unsigned int prefer_cluster, int skip, long *area);

#endif
