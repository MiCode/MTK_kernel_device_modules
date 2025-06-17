/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 MediaTek Inc.
 */
#ifndef _VIP_ENGINE_H_
#define _VIP_ENGINE_H_

#include <linux/list.h>
#include <uapi/linux/sched/types.h>
#include "common.h"

#define get_task_turbo_t(p)	\
	(&((struct mtk_task *)android_task_vendor_data(p))->turbo_data)
#define get_vip_t(p)	\
	(&((struct mtk_static_vendor_task *)(p)->android_vendor_data1)->vip_task)
#define TOUCH_DOWN 1
#define TOUCH_SUSTAIN_MS 2000
#define INVALID_TGID -1
#define INVALID_VAL -1
#define INVALID_LOADING -1
#define MAX_NR_THREAD 200
#define MAX_RT_PRIO 100
#define MAX_NORMAL_PRIO 140
#define MIN_CPUS 4

DECLARE_PER_CPU(unsigned long, max_freq_scale);
DECLARE_PER_CPU(unsigned long, min_freq_scale);

struct list_head;

enum {
	DEBUG_NODE,
	FPSGO,
	UX,
	VIDEO,
	MAX_TYPE
};

enum {
	PRINT_UCLAMP_LIST		= 1,
	CLEAR_UCLAMP_LIST		= 2,
};

/* FLT mode */
enum _flt_mode {
	FLT_MODE_0 = 0,
	FLT_MODE_1 = 1,
	FLT_MODE_2 = 2,
	FLT_MODE_3 = 3,
	FLT_MODE_4 = 4,
	FLT_MODE_NUM,
};

struct cpu_time {
	u64 time;
};

struct cpu_info {
	int *cpu_loading;
};

struct uclamp_data_node {
	pid_t pid;
	struct list_head list;
};

struct vipServer_data_node {
	pid_t pid;
	pid_t proc_pid;
	struct list_head list;
};

struct sched_attr_work {
	struct work_struct work;
	struct task_struct *task;
	struct sched_attr attr;
	int toSet;
};

void set_vip_ctrl_node(int pid, int vip_prio, unsigned int throttle_time);
void unset_vip_ctrl_node(int pid, int vip_prio);
extern int (*task_turbo_enforce_ct_to_vip_fp)(int val, int caller_id);
extern void (*task_turbo_do_set_binder_uclamp_param)(pid_t pid, int binder_uclamp_max, int binder_uclamp_min);
extern void (*task_turbo_do_unset_binder_uclamp_param)(pid_t pid);
extern void (*task_turbo_do_binder_uclamp_stuff)(int cmd);
extern void (*task_turbo_do_enable_binder_uclamp_inheritance)(int enable);
extern void (*vip_engine_set_vip_ctrl_node_cs)(int pid, int vip_prio, unsigned int throttle_time);
extern void (*vip_engine_unset_vip_ctrl_node_cs)(int pid, int vip_prio);
extern void (*vip_engine_set_vip_ctrl_node_sbe)(int pid, int vip_prio, unsigned int throttle_time);
extern void (*vip_engine_unset_vip_ctrl_node_sbe)(int pid, int vip_prio);

extern inline bool launch_turbo_enable(void);
extern void unset_task_priority_based_vip(int pid);
extern int get_cpu_gear_uclamp_max_capacity(unsigned int cpu, int ret_type);
#if IS_ENABLED(CONFIG_MTK_TASK_TURBO)
extern int *tt_vip_enable_p;
#endif
int do_set_server_vip(pid_t pid, int enable);
void do_enable_binder_vipServer(int enable);
#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
extern void flt_set_mode(u32 mode);
extern u32 flt_get_mode(void);
#endif
extern void (*vip_loom_select_task_rq_fair_hook)(struct task_struct *p, int *target_cpu, int *flag);
#endif /* _VIP_ENGINE_H_ */
