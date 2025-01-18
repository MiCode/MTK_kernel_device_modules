/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef _VIP_H
#define _VIP_H

extern bool vip_enable;

#define VIP_TIME_SLICE     3000000U
#define VIP_TIME_LIMIT     (4 * VIP_TIME_SLICE)

// #define WORKER_VIP         0
// #define VVIP               1
// #define NUM_VIP_PRIO       2
enum {
	WORKER_VIP,
	VVIP,
	NUM_VIP_PRIO
};

#define NOT_VIP           -1

#define DEFAULT_VIP_PRIO_THRESHOLD  99

#define mts_to_ts(mts) ({ \
		void *__mptr = (void *)(mts); \
		((struct task_struct *)(__mptr - \
			offsetof(struct task_struct, android_vendor_data1))); })

struct vip_rq {
	struct list_head vip_tasks;
	int num_vip_tasks[NUM_VIP_PRIO];
	int sum_num_vip_tasks;
};

enum vip_group {
	VIP_GROUP_TOPAPP,
	VIP_GROUP_FOREGROUND,
	VIP_GROUP_BACKGROUND,
	VIP_GROUP_NUM
};

extern void set_task_basic_vip(int pid);
extern void unset_task_basic_vip(int pid);
extern void set_task_vvip(int pid);
extern void unset_task_vvip(int pid);
extern void set_ls_task_vip(unsigned int prio);
extern int set_group_vip_prio(unsigned int cpuctl_id, unsigned int prio);
extern void set_top_app_vip(unsigned int prio);
extern void set_foreground_vip(unsigned int prio);
extern void set_background_vip(unsigned int prio);
extern bool sched_vip_enable_get(void);
extern inline int get_vip_task_prio(struct task_struct *p);
extern bool task_is_vip(struct task_struct *p, int type);
extern bool prio_is_vip(int vip_prio, int type);
extern inline unsigned int num_vip_in_cpu(int cpu, int vip_prio);
extern inline bool is_task_latency_sensitive(struct task_struct *p);
extern int find_imbalanced_vvip_gear(void);
extern struct task_struct *next_vip_runnable_in_cpu(struct rq *rq, int type);
extern struct cpumask find_min_num_vip_cpus(struct perf_domain *pd, struct task_struct *p, int vip_prio);
extern int find_vip_backup_cpu(struct task_struct *p, struct cpumask *allowed_cpu_mask, int prev_cpu, int target);
extern unsigned int get_adaptive_margin(unsigned int cpu);

extern void vip_enqueue_task(struct rq *rq, struct task_struct *p);

extern void vip_init(void);

extern inline bool vip_fair_task(struct task_struct *p);
extern void _init_tg_mask(struct cgroup_subsys_state *css);
extern bool balance_vvip_overutilied;
extern bool balance_vip_overutilized;

#endif /* _VIP_H */
