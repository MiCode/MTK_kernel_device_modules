/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _LOOM_H_
#define _LOOM_H_


int loom_init(void);
void loom_exit(void);


int loom_set_task_cfg(char *proc_name, char *thread_name,
	int pid, int mode, int matching_num, int prio, int cpu_mask,
	int set_exclusive, int loading_ub, int loading_lb, int bhr,
	int limit_min_freq, int limit_max_freq,
	int set_rescue, int rescue_f_opp, int rescue_c_freq, int rescue_time);
int loom_reset_task_cfg(char *proc_name, char *thread_name, int pid);
int cpumask_to_cpu_id(int cpu_mask);

extern void set_task_priority_based_vip_and_throttle(int pid, int prio, unsigned int throttle_time);
extern void unset_task_priority_based_vip(int pid);
extern int vip_loom_select_cfg_apply(int val, int caller_id);
extern int vip_loom_flt_cfg_apply(int val, int caller_id);
extern int loom_cpu_dedicated(unsigned int enable);
extern int loom_ctask_cpu_dedicated(int pid, int aff_cpu);
extern int fpsgo_other2fstb_get_fps(int pid, unsigned long long bufID,
	int *qfps_arr, int *qfps_num, int max_qfps_num,
	int *tfps_arr, int *tfps_num, int max_tfps_num,
	int *diff_arr, int *diff_num, int max_diff_num);

#endif  // _LOOM_H_
