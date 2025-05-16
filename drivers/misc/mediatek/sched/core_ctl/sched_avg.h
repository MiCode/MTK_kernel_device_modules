/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _SCHED_AVG_H
#define _SCHED_AVG_H

enum {
	DISABLE_POLICY = 0,
	AGGRESSIVE_POLICY,
	CONSERVATIVE_POLICY,
	POLICY_CNT
};

extern void sched_max_util_task(int *util);
extern void arch_get_cluster_cpus(struct cpumask *cpus, int package_id);
extern int sched_get_nr_over_thres_avg(int cluster_id,
				       int *dn_avg,
				       int *up_avg,
				       int *sum_nr_over_dn_thres,
				       int *sum_nr_over_up_thres);
extern int arch_get_nr_clusters(void);
extern int arch_get_cluster_id(unsigned int cpu);
extern int init_sched_avg(void);
extern void exit_sched_avg(void);
extern unsigned int get_cpu_util_pct(unsigned int cpu, bool orig);
extern int set_over_threshold(unsigned int index, unsigned int val);
extern unsigned int get_over_threshold(int index);
extern unsigned int get_max_capacity(unsigned int cid);
extern unsigned int pd_get_opp_leakage(unsigned int cpu,
				    unsigned int opp,
				    unsigned int temperature);
extern unsigned long pd_get_opp_capacity_legacy(int cpu, int opp);
extern int get_max_nr_running(int cpu);
extern int get_max_rt_nr_running(int cpu);
extern int get_max_vip_nr_running(int cpu);
extern void policy_chg_notify(void);
extern unsigned int core_ctl_get_policy(void);
extern unsigned long _capacity_of(int cpu);
extern unsigned long get_freq_qos_max_of_min(unsigned int cid);
extern unsigned long mtk_cpu_util_cfs_boost(int cpu);
extern int mtk_effective_cpu_util_total(int cpu, struct task_struct *p, int dst_cpu, int runnable_boost,
					unsigned long *min, unsigned long *max, unsigned long *tsk_min_clp,
					unsigned long *tsk_max_clp, struct cpumask *sg_cpumask,
					unsigned long cpu_util_iowait, int curr_task_uclamp);

#endif /* _SCHED_AVG_H */
