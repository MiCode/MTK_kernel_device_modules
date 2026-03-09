/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#ifndef _LOOM_BASE_H_
#define _LOOM_BASE_H_


#include <linux/list.h>
#include <linux/mutex.h>

#define LOOM_DEFAULT_VALUE -1
#define LOOM_MAX_NAME_LENGTH 16
#define LOOM_MAX_RENDER_NUM 20
#define LOOM_MAX_LOOM_CFG_NUM 100


enum LOOM_MATCH_MODE {
	MATCH_NAME_APPROX = 0,
	MATCH_NAME_EXACT = 1,
	MATCH_PID = 2,
};

// loom control put here?
struct loom_attr_info {
	char proc_name[LOOM_MAX_NAME_LENGTH];
	char thread_name[LOOM_MAX_NAME_LENGTH];
	int pid;
	int tgid;
	int mode;
	int matching_num;
	int prio;
	int cpu_mask;
	int set_exclusive;
	int loading_ub;
	int loading_lb;
	int limit_min_freq;
	int limit_max_freq;
	int bhr;
	int set_rescue;
	int rescue_f_opp;
	int rescue_c_freq;
	int rescue_time;
	struct hlist_node hlist;

	int vip_set;		// flag indicates vip is already set
	int cmask_set;		// flag indicates the cpu mask which is already set
	int is_exclusive;	// flag indicates whether affinity is cpu dedicated
};

struct loom_render_info {
	struct hlist_node render_hlist;
	int tgid;
	int pid;
	unsigned long long buffer_id;
	unsigned long long last_update_ts;
	unsigned long long queue_end_ts;
	struct hlist_head active_list;
	struct list_head lc_active_list;

	int q_cnt; // workaround for minchao app hang

	/* for thermal */
	int thermal_bypass;
	unsigned long long last_thermal_check_ts; // use for cooldown calculation
};

void *loom_alloc(int size);
void *loom_calloc(int num, int size);
void loom_free(void *pvBuf);
void loom_render_lock(void);
void loom_render_unlock(void);
void loom_cfg_lock(void);
void loom_cfg_unlock(void);
void loom_mode_lock(void);
void loom_mode_unlock(void);
int loom_arch_nr_clusters(void);
int loom_get_render_num(void);
int loom_get_cfg_length(void);
unsigned long long loom_get_time(void);
long loom_sched_setaffinity(int pid, int cpumask);
struct loom_attr_info *loom_add_task_cfg_pid_sorted(struct hlist_head *head, char *proc_name,
	char *thread_name, int pid);
void loom_clear_loom_attr(struct hlist_head *head);
void loom_clear_loading_ctrl_list(struct list_head *head);
void loom_delete_task_cfg(struct loom_attr_info *iter, struct hlist_head *head);
struct loom_attr_info *loom_search_add_task_cfg(struct hlist_head *head, int mode,
	char *proc_name, char *thread_name, int pid, int add);
void loom_assign_task_cfg(struct loom_attr_info *info, int mode,
	int match_num, int prio, int cpu_mask, int set_exclusive,
	int loading_ub, int loading_lb, int bhr,
	int limit_min_cap, int limit_max_cap,
	int set_rescue, int rescue_f_opp, int rescue_c_freq, int rescue_time);
struct loom_render_info *loom_search_add_render_info(int tgid, int add);
void loom_delete_render_info(struct loom_render_info *iter);
struct hlist_head *loom_get_cfg_list(void);
struct hlist_head *loom_get_render_list(void);
int get_loom_is_enable(int rpid);

int loom_check_loom_jerk_work_addr_invalid(struct work_struct *target_work);

extern int set_cpus_allowed_ptr_by_kernel(struct task_struct *p, const struct cpumask *new_mask);
#endif // _LOOM_BASE_H_
