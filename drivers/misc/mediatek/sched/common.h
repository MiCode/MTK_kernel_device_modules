/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef _SCHED_COMMON_H
#define _SCHED_COMMON_H

#define MTK_VENDOR_DATA_SIZE_TEST(mstruct, kstruct)		\
	BUILD_BUG_ON(sizeof(mstruct) > (sizeof(u64) *		\
		ARRAY_SIZE(((kstruct *)0)->android_vendor_data1)))

#define UTIL_EST_QUEUE_SHIFT		15
#define UTIL_EST_MASK			((1 << UTIL_EST_QUEUE_SHIFT) - 1)
#define GEAR_HINT_UNSET -1
#define MTK_TASK_GROUP_FLAG 1
#define MTK_TASK_FLAG 9
#define RAVG_HIST_SIZE_MAX (5)
#define FLT_NR_CPUS CONFIG_MAX_NR_CPUS

#define scale_ratio(a, b) ((a) << SCHED_CAPACITY_SHIFT) / (b)
#define scale_ratio_u64(a, b) div64_u64((a) << SCHED_CAPACITY_SHIFT, (b))

#define arch_set_non_s_dpt_v2				topology_set_non_s_scale_dpt_v2
#define arch_scale_non_s_capacity_dpt_v2	topology_get_non_s_scale_dpt_v2
#define arch_scale_cpu_capacity_dpt_v2			topology_get_cpu_scale_dpt_v2

#define arch_set_coef1_s_scale_dpt_v2			topology_set_coef1_s_scale_dpt_v2
#define arch_scale_coef1_s_capacity_dpt_v2		topology_get_coef1_s_scale_dpt_v2
#define arch_set_coef1_ltime_scale_dpt_v2		topology_set_coef1_ltime_scale_dpt_v2
#define arch_scale_coef1_ltime_capacity_dpt_v2	topology_get_coef1_ltime_scale_dpt_v2
#define arch_scale_coef1_capacity_dpt_v2			topology_get_coef1_scale_dpt_v2

#define arch_set_coef2_s_scale_dpt_v2		topology_set_coef2_s_scale_dpt_v2
#define arch_scale_coef2_s_capacity_dpt_v2	topology_get_coef2_s_scale_dpt_v2
#define arch_set_coef2_ltime_scale_dpt_v2		topology_set_coef2_ltime_scale_dpt_v2
#define arch_scale_coef2_ltime_capacity_dpt_v2	topology_get_coef2_ltime_scale_dpt_v2
#define arch_scale_coef2_capacity_dpt_v2			topology_get_coef2_scale_dpt_v2

enum s_type {
	NON_S,
	S_COEF1,
	S_COEF2,
	S_TOTAL,
	MAX_S_TYPE_NUM,
};

enum DPT_V2_RESCALE_TYPE {
	RESCALE_UTIL,
	RESCALE_RATIO,
};

enum DPT_V2_SCALING_TYPE {
	CPU_COMPUTING_CYCLE_SCALING,
	COEF1_S_SCALING,
	COEF2_S_SCALING,
	CPU_S_SCALING,
	NUM_SCALING_TYPE,
};

enum DPT_V2_CONVERT_TYPE {
	TO_BCORE,
	BCORE2MCORE,
	BCORE2LCORE,
	NUM_CONVERT_TYPE,
};

enum pelt_type {
	TASK,
	CFS,
	RT
};

typedef int perf_scaling_factor_arr[NUM_CONVERT_TYPE][NUM_SCALING_TYPE];
struct dpt_task_struct {
	u32 util_cpu_sum;
	u32 util_coef1_sum;
	u32 util_coef2_sum;
	unsigned long util_cpu_avg;
	unsigned long util_coef1_avg;
	unsigned long util_coef2_avg;
	unsigned int util_cpu_est;
	unsigned int util_coef1_est;
	unsigned int util_coef2_est;

	/* scaling factor of last used CPU, update when schedule out*/
	perf_scaling_factor_arr perf_scaling_factor;
	perf_scaling_factor_arr inv_perf_scaling_factor;
	int power_scaling_factor;
};

typedef struct dpt_rq_struct {
	/* clock */
	u64 local_clock[MAX_S_TYPE_NUM];
	u64 global_clock[MAX_S_TYPE_NUM];
	unsigned int local_clock_ratio[MAX_S_TYPE_NUM];
	unsigned int global_clock_ratio[MAX_S_TYPE_NUM];

	/* arch scale */
	unsigned int arch_s_scale[MAX_S_TYPE_NUM];
	unsigned int arch_ltime_scale[MAX_S_TYPE_NUM];

	/* sratio info */
	unsigned int sratio[MAX_S_TYPE_NUM];

	/* ltime info */
	unsigned int cur_ltime[MAX_S_TYPE_NUM];
	unsigned int min_ltime[MAX_S_TYPE_NUM];

	/* rq Util info */
	struct dpt_task_struct util_cfs;
	int removed_nr;
	unsigned long removed_util_cpu_avg;
	unsigned long removed_util_coef1_avg;
	unsigned long removed_util_coef2_avg;
	u32 util_cpu_sum_tmp;
	u32 util_coef1_sum_tmp;
	u32 util_coef2_sum_tmp;
	unsigned long util_cpu_avg_tmp;
	unsigned long util_coef1_avg_tmp;
	unsigned long util_coef2_avg_tmp;
	struct dpt_task_struct util_rt;

	/* cpu info */
	unsigned int cpu_cap_ratio;
	unsigned int cpu_freq_ratio;
	/* TODO */

	/* Manual setting */
	int coef2_ltime_manual;
	int coef1_ltime_manual;
	int cpu_sratio_manual;
	int coef1_sratio_manual;
	int coef2_sratio_manual;
	/* TODO */

	/* util info */
	unsigned long dpt_v2_total_util;
	unsigned long dpt_v2_cpu_util;
	unsigned long dpt_v2_coef1_util;
	unsigned long dpt_v2_coef2_util;
	unsigned int cfs_cpu_util;
	unsigned int cfs_coef1_util;
	unsigned int cfs_coef2_util;
	u64 local_clock_pelt;		/* for all rq's own local utilization */
	u64 last_clock_pelt;		/* for all rq's own local utilization */
	u64 local_cfs_last_update_time;	/* for cfs rq's own local utilization */
	u32 local_cfs_period_contrib;	/* for cfs rq's own local utilization */
	u64 local_rt_last_update_time;	/* for rt rq's own local utilization */
	u32 local_rt_period_contrib;	/* for rt rq's own local utilization */
	unsigned int rt_cpu_util;
	unsigned int rt_coef1_util;
	unsigned int rt_coef2_util;
} dpt_rq_t;

struct task_gear_hints {
	int gear_start;
	int num_gear;
	int reverse;
};

struct vip_task_struct {
	struct list_head		vip_list;
	u64				sum_exec_snapshot;
	u64				total_exec;
	int				vip_prio;
	bool			basic_vip;
	bool			vvip;
	bool			faster_compute_eng;
	int				priority_based_prio;
	unsigned int	throttle_time;
};

struct soft_affinity_task {
	bool latency_sensitive;
	struct cpumask soft_cpumask;
};

struct gp_task_struct {
	struct grp __rcu	*grp;
	bool customized;
};

struct sbb_task_struct {
	int set_task;
	int set_group;
};

struct curr_uclamp_hint {
	int hint;
};

struct rot_task_struct {
	u64 ktime_ns;
};

struct cc_task_struct {
	u64 over_type;
};

struct task_turbo_t {
	unsigned char turbo:1;
	unsigned char render:1;
	unsigned short inherit_cnt:14;
	short nice_backup;
	atomic_t inherit_types;
	int vip_prio_backup;
	unsigned int throttle_time_backup;
	int is_uclamp_binder;
	int uclamp_binder_cnt;
	unsigned short uclamp_value_min:11;
	unsigned short uclamp_value_max:11;
	int is_binder_vip_server;
	int is_binder_vip_server_done;
};

struct flt_task_struct {
	u64	last_update_time;
	u64	mark_start;
	u32	sum;
	u32	util_sum;
	u32	demand;
	u32	util_demand;
	u32	sum_history[RAVG_HIST_SIZE_MAX];
	u32	util_sum_history[RAVG_HIST_SIZE_MAX];
	u32	util_avg_history[RAVG_HIST_SIZE_MAX];
	u64	active_time;
	u32	init_load_pct;
	u32	curr_window_cpu[FLT_NR_CPUS];
	u32	prev_window_cpu[FLT_NR_CPUS];
	u32	curr_window;
	u32	prev_window;
	int	prev_on_rq;
	int	prev_on_rq_cpu;
};

struct cpuqos_task_struct {
	int pd;
	int rank;
};

/* for dynamic task vendor data*/
struct mtk_task {
	struct soft_affinity_task sa_task;
	struct gp_task_struct	gp_task;
	struct task_gear_hints  gear_hints;
	struct sbb_task_struct sbb_task;
	struct curr_uclamp_hint cu_hint;
	struct rot_task_struct rot_task;
	struct cc_task_struct cc_task;
	struct task_turbo_t turbo_data;
	struct flt_task_struct flt_task;
	struct cpuqos_task_struct cpuqos_task;
	struct cpumask kernel_allowed_mask;
	struct dpt_task_struct dpt_task;
};

/* for static task vendor data*/
struct mtk_static_vendor_task {
	struct vip_task_struct	vip_task;
};

struct soft_affinity_tg {
	struct cpumask soft_cpumask;
};

struct cgrp_tg {
	bool colocate;
	int groupid;
};

struct vip_task_group {
	unsigned int threshold;
};

struct mtk_tg {
	struct soft_affinity_tg	sa_tg;
	struct cgrp_tg		cgrp_tg;
	struct vip_task_group vtg;
};

struct sugov_rq_data {
	short int uclamp[UCLAMP_CNT];
	bool enq_dvfs;
	bool enq_ing;
	bool enq_update_dsu_freq;
};

struct mtk_rq {
	struct sugov_rq_data sugov_data;
};

extern int num_sched_clusters;
extern cpumask_t __read_mostly ***cpu_array;
extern void init_cpu_array(void);
extern void build_cpu_array(void);
extern void free_cpu_array(void);
extern void mtk_get_gear_indicies(struct task_struct *p, int *order_index, int *end_index,
			int *reverse, bool latency_sensitive);
extern bool sched_gear_hints_enable_get(void);
extern void init_gear_hints(void);
extern bool sched_dsu_pwr_enable_get(void);
extern void init_dsu_pwr_enable(void);


extern bool sched_updown_migration_enable_get(void);
extern void init_updown_migration(void);

extern bool sched_post_init_util_enable_get(void);

extern int set_gear_indices(int pid, int gear_start, int num_gear, int reverse);
extern int unset_gear_indices(int pid);
extern int get_gear_indices(int pid, int *gear_start, int *num_gear, int *reverse);
extern int set_updown_migration_pct(int gear_idx, int dn_pct, int up_pct);
extern int unset_updown_migration_pct(int gear_idx);
extern int get_updown_migration_pct(int gear_idx, int *dn_pct, int *up_pct);

struct util_rq {
	unsigned long util_cfs;
	unsigned long dl_util;
	unsigned long irq_util;
	unsigned long rt_util;
	unsigned long bw_dl_util;
	bool base;
};

#if IS_ENABLED(CONFIG_NONLINEAR_FREQ_CTL)
extern void mtk_map_util_freq(void *data, unsigned long util,
			struct cpumask *cpumask, unsigned long *next_freq,
			unsigned long min, unsigned long max);
#else
#define mtk_map_util_freq(data, util, cap, next_freq)
#endif /* CONFIG_NONLINEAR_FREQ_CTL */

#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
DECLARE_PER_CPU(int, cpufreq_idle_cpu);
DECLARE_PER_CPU(spinlock_t, cpufreq_idle_cpu_lock);

unsigned long mtk_get_actual_cpu_capacity(int cpu);
unsigned long mtk_effective_cpu_util(unsigned int cpu, unsigned long util_cfs,
				struct task_struct *p, unsigned long *min, unsigned long *max);
unsigned long sugov_effective_cpu_perf_clamp(unsigned long actual, unsigned long min,
				unsigned long max);
DECLARE_PER_CPU(dpt_rq_t, __dpt_rq);
unsigned long mtk_effective_cpu_util_dpt_v2(unsigned int cpu, unsigned long *cpu_util_local,
	unsigned long *coef1_util_local, unsigned long *coef2_util_local, struct task_struct *p,
    unsigned long *min, unsigned long *max);
int dequeue_idle_cpu(int cpu);
#endif // CONFIG_MTK_CPUFREQ_SUGOV_EXT
__always_inline
unsigned long mtk_uclamp_rq_util_with(struct rq *rq, unsigned long util,
				  struct task_struct *p,
				  unsigned long min_cap, unsigned long max_cap,
				  unsigned long *__min_util, unsigned long *__max_util,
				  bool record_uclamp);
/* dpt v2 */
enum gear_uclamp_ret_type {
	GU_RET_FREQ,
	GU_RET_UTIL
};

#define get_scaling_factor_convert_type(gear_idx) (BCORE2LCORE - (gear_idx))
#define get_task_ipc_scaling_factor(p, gear) (( \
&((struct mtk_task *) android_task_vendor_data(p))->dpt_task \
)->perf_scaling_factor[get_scaling_factor_convert_type(gear)][CPU_COMPUTING_CYCLE_SCALING])
#define FREQ_CEILING_RATIO_BIT 10

extern unsigned long dpt_v2_get_uclamped_cpu_util(int cpu, unsigned long max_util, unsigned long min_util,
	unsigned long cpu_util_local, unsigned long coef1_util_local, unsigned long coef2_util_local, int quant, int *using_uclamp_freq);

extern int pd_util2freq(unsigned int cpu, int util, bool quant, int wl);
extern unsigned long (*dpt_v2_freq2cap_hook) (int quant, int cpu, unsigned long freq);
extern unsigned long (*rescale_coef2_util_or_ratio_hook) (unsigned long coef2_val, int type);
extern unsigned long (*rescale_coef1_util_or_ratio_hook) (unsigned long coef1_val, int type);
extern inline int is_dpt_v2_support (void);
extern inline unsigned long dpt_v2_global_capacity_of(int cpu);
extern inline unsigned long dpt_v2_local_capacity_all_util_of(int cpu);
extern inline unsigned long dpt_v2_global_capacity_orig(int cpu);
extern void topology_set_non_s_scale_dpt_v2(int cpu, unsigned int non_sratio);
extern void topology_set_coef1_s_scale_dpt_v2(int cpu, unsigned int sratio);
extern void topology_set_coef1_ltime_scale_dpt_v2(int cpu, unsigned int min_ltime, unsigned int cur_ltime);
extern void topology_set_coef2_s_scale_dpt_v2(int cpu, unsigned int sratio);
extern void topology_set_coef2_ltime_scale_dpt_v2(int cpu, unsigned int min_ltime, unsigned int cur_ltime);
extern void task_global_to_local_dpt_v2(int cpu, struct task_struct *p,
	unsigned long *local_util_cpu_avg, unsigned long *local_util_coef1_avg, unsigned long *local_util_coef2_avg,
	unsigned int *local_util_cpu_est, unsigned int *local_util_coef1_est, unsigned int *local_util_coef2_est);
extern unsigned long (*dpt_v2_opp2global_cap_hook)(int quant, int gear_idx, int opp,
	unsigned long cpu_util_local, unsigned long total_util_local, int IPC_scaling_factor);
extern unsigned long (*dpt_v2_linear_local_cap2opp_hook)(int cpu, int quant, unsigned long util);

extern void (*get_cpu_perf_scaling_factor_hook)(int cpu, perf_scaling_factor_arr *task_perf_scaling_factor);
extern void (*get_cpu_power_scaling_factor_hook)(int cpu, int *task_power_scaling_factor);
extern void (*check_cpu_perf_scaling_factor_hook)(int cpu, perf_scaling_factor_arr *task_perf_scaling_factor);
extern void (*check_cpu_power_scaling_factor_hook)(int cpu, int *task_power_scaling_factor);
extern unsigned long (*dpt_v2_opp2pwr_eff_hook)(int cpu, int opp, int quant, int wl_type, int *swpm_vars,
	unsigned long cpu_util_local, unsigned long total_util_local, int IPC_scaling_factor);
/* End of dpt v2 */

/* DPTv2 SWPM sysctl helper functions*/
extern void init_swpm_pwr_coef_by_gear(int mode, unsigned int gear_id);
extern void set_swpm_pwr_coef(unsigned int gear_id, unsigned int *vals, unsigned int vals_size);
extern void get_swpm_pwr_coef(unsigned int gear_id, unsigned int *vals, unsigned int vals_size);

#if IS_ENABLED(CONFIG_RT_GROUP_SCHED)
static inline int rt_rq_throttled(struct rt_rq *rt_rq)
{
	return rt_rq->rt_throttled && !rt_rq->rt_nr_boosted;
}
#else /* !CONFIG_RT_GROUP_SCHED */
static inline int rt_rq_throttled(struct rt_rq *rt_rq)
{
	return false;
}
#endif // CONFIG_RT_GROUP_SCHED

extern int set_target_margin(int gearid, int margin);
extern int set_target_margin_low(int gearid, int margin);
extern int unset_target_margin(int gearid);
extern int unset_target_margin_low(int gearid);
extern int set_turn_point_freq(int gearid, unsigned long freq);

extern void set_curr_task_uclamp_ctrl(int set);
extern void unset_curr_task_uclamp_ctrl(void);
extern int get_curr_task_uclamp_ctrl(void);

#if IS_ENABLED(CONFIG_MTK_SCHEDULER)
extern bool sysctl_util_est;
#endif // CONFIG_MTK_SCHEDULER

static inline bool is_util_est_enable(void)
{
#if IS_ENABLED(CONFIG_MTK_SCHEDULER)
	return sysctl_util_est;
#else
	return true;
#endif // CONFIG_MTK_SCHEDULER
}

enum {
	CPU_UTIL,
	COEF1_UTIL,
	COEF2_UTIL,
	TOTAL_UTIL
};

#if IS_ENABLED(CONFIG_SMP)
static inline unsigned long cpu_util_rt_dpt_v2(struct rq *rq, int type)
{
	dpt_rq_t *dpt_rq = &per_cpu(__dpt_rq, cpu_of(rq));

	switch(type) {
		case CPU_UTIL:
			return READ_ONCE(dpt_rq->util_rt.util_cpu_avg);
		case COEF1_UTIL:
			return READ_ONCE(dpt_rq->util_rt.util_coef1_avg);
		case COEF2_UTIL:
			return READ_ONCE(dpt_rq->util_rt.util_coef2_avg);
		default:
			return READ_ONCE(dpt_rq->util_rt.util_cpu_avg) + READ_ONCE(dpt_rq->util_rt.util_coef1_avg) + READ_ONCE(dpt_rq->util_rt.util_coef2_avg);
	}
}

static inline unsigned long cpu_util_cfs_dpt_v2(struct rq *rq, int type)
{
	dpt_rq_t *dpt_rq = &per_cpu(__dpt_rq, cpu_of(rq));

	switch(type) {
		case CPU_UTIL:
			return READ_ONCE(dpt_rq->util_cfs.util_cpu_avg);
		case COEF1_UTIL:
			return READ_ONCE(dpt_rq->util_cfs.util_coef1_avg);
		case COEF2_UTIL:
			return READ_ONCE(dpt_rq->util_cfs.util_coef2_avg);
		default:
			return READ_ONCE(dpt_rq->util_cfs.util_cpu_avg) + READ_ONCE(dpt_rq->util_cfs.util_coef1_avg) + READ_ONCE(dpt_rq->util_cfs.util_coef2_avg);
	}
}

static inline int task_inv_scaling_dpt_v2(struct task_struct *p, int covert_type, int scaling_type)
{
	struct dpt_task_struct *dts = &((struct mtk_task *) android_task_vendor_data(p))->dpt_task;
	return READ_ONCE(dts->inv_perf_scaling_factor[covert_type][scaling_type]);
}

static inline int task_scaling_dpt_v2(struct task_struct *p, int covert_type, int scaling_type)
{
	struct dpt_task_struct *dts = &((struct mtk_task *) android_task_vendor_data(p))->dpt_task;

	return READ_ONCE(dts->perf_scaling_factor[covert_type][scaling_type]);
}

static inline unsigned long task_util_dpt_v2(struct task_struct *p, int type)
{
	struct dpt_task_struct *util_task = &((struct mtk_task *) android_task_vendor_data(p))->dpt_task;

	switch(type) {
		case CPU_UTIL:
			return READ_ONCE(util_task->util_cpu_avg);
		case COEF1_UTIL:
			return READ_ONCE(util_task->util_coef1_avg);
		case COEF2_UTIL:
			return READ_ONCE(util_task->util_coef2_avg);
		default:
			return READ_ONCE(util_task->util_cpu_avg) + READ_ONCE(util_task->util_coef1_avg) + READ_ONCE(util_task->util_coef2_avg);
	}
}

static inline unsigned int _task_util_est_queue_dpt_v2(struct task_struct *p, int type)
{
	struct dpt_task_struct *util_task = &((struct mtk_task *)android_task_vendor_data(p))->dpt_task;

	switch(type) {
		case CPU_UTIL:
			return READ_ONCE(util_task->util_cpu_est) >> UTIL_EST_QUEUE_SHIFT;
		case COEF1_UTIL:
			return READ_ONCE(util_task->util_coef1_est) >> UTIL_EST_QUEUE_SHIFT;
		case COEF2_UTIL:
			return READ_ONCE(util_task->util_coef2_est) >> UTIL_EST_QUEUE_SHIFT;
		default:
			return (READ_ONCE(util_task->util_cpu_est) + READ_ONCE(util_task->util_coef1_est) + READ_ONCE(util_task->util_coef2_est)) >> UTIL_EST_QUEUE_SHIFT;
	}
}

static inline unsigned int _task_util_est_dpt_v2(struct task_struct *p, int type)
{
	struct dpt_task_struct *util_task = &((struct mtk_task *) android_task_vendor_data(p))->dpt_task;

	switch(type) {
		case CPU_UTIL:
			return READ_ONCE(util_task->util_cpu_est) & UTIL_EST_MASK;
		case COEF1_UTIL:
			return READ_ONCE(util_task->util_coef1_est) & UTIL_EST_MASK;
		case COEF2_UTIL:
			return READ_ONCE(util_task->util_coef2_est) & UTIL_EST_MASK;
		default:
			return (READ_ONCE(util_task->util_cpu_est) + READ_ONCE(util_task->util_coef1_est) + READ_ONCE(util_task->util_coef2_est)) & UTIL_EST_MASK;
	}
}

static inline unsigned long task_util_est_dpt_v2(struct task_struct *p, int type)
{
	if (sched_feat(UTIL_EST) && is_util_est_enable())
		return max(task_util_dpt_v2(p, type), _task_util_est_dpt_v2(p, type));
	return task_util_dpt_v2(p, type);
}

#if IS_ENABLED(CONFIG_UCLAMP_TASK)
static inline unsigned long uclamp_task_util_dpt_v2(struct task_struct *p, int cpu, int *using_uclamp_freq)
{
	unsigned long cpu_util_local, coef1_util_local, coef2_util_local, cpu_util_local_avg, coef1_util_local_avg, coef2_util_local_avg;
	unsigned int cpu_util_local_est, coef1_util_local_est, coef2_util_local_est;

	if (rt_task(p))
		return clamp(0, uclamp_eff_value(p, UCLAMP_MIN), uclamp_eff_value(p, UCLAMP_MAX));

	task_global_to_local_dpt_v2(cpu, p, &cpu_util_local_avg, &coef1_util_local_avg, &coef2_util_local_avg, &cpu_util_local_est, &coef1_util_local_est, &coef2_util_local_est);
	cpu_util_local = max(cpu_util_local_avg, cpu_util_local_est);
	coef1_util_local = max(coef1_util_local_avg, coef1_util_local_est);
	coef2_util_local = max(coef2_util_local_avg, coef2_util_local_est);

	cpu_util_local = dpt_v2_get_uclamped_cpu_util(cpu, uclamp_eff_value(p, UCLAMP_MAX), uclamp_eff_value(p, UCLAMP_MIN),
		cpu_util_local, coef1_util_local, coef2_util_local, 0, using_uclamp_freq);

	return cpu_util_local + rescale_coef1_util_or_ratio_hook(coef1_util_local, RESCALE_UTIL) + rescale_coef2_util_or_ratio_hook(coef2_util_local, RESCALE_UTIL);
}
#else
static inline unsigned long uclamp_task_util_dpt_v2(struct task_struct *p)
{
	return rt_task(p) ? 0 : task_util_est_dpt_v2(p);
}
#endif // CONFIG_UCLAMP_TASK
#endif // CONFIG_SMP

#if IS_ENABLED(CONFIG_SMP)
static inline unsigned long task_util(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.util_avg);
}

static inline unsigned long task_runnable(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.runnable_avg);
}

static inline unsigned long _task_util_est(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.util_est) & ~UTIL_AVG_UNCHANGED;
}

static inline unsigned long task_util_est(struct task_struct *p)
{
	return max(task_util(p), _task_util_est(p));
}

static inline unsigned int topology_get_non_s_scale_dpt_v2(int cpu)
{
	return READ_ONCE(per_cpu(__dpt_rq, cpu).arch_s_scale[NON_S]);
}

static inline int topology_get_cpu_scale_dpt_v2(int cpu)
{
	return READ_ONCE(per_cpu(__dpt_rq, cpu).util_cfs.inv_perf_scaling_factor[TO_BCORE][CPU_COMPUTING_CYCLE_SCALING]);
}

static inline unsigned int topology_get_coef1_s_scale_dpt_v2(int cpu)
{
	return READ_ONCE(per_cpu(__dpt_rq, cpu).arch_s_scale[S_COEF1]);
}

static inline unsigned int topology_get_coef1_ltime_scale_dpt_v2(int cpu)
{
	return READ_ONCE(per_cpu(__dpt_rq, cpu).arch_ltime_scale[S_COEF1]);
}

static inline int topology_get_coef1_scale_dpt_v2(int cpu)
{
	return READ_ONCE(per_cpu(__dpt_rq, cpu).util_cfs.perf_scaling_factor[TO_BCORE][COEF1_S_SCALING]);
}

static inline unsigned int topology_get_coef2_s_scale_dpt_v2(int cpu)
{
	return READ_ONCE(per_cpu(__dpt_rq, cpu).arch_s_scale[S_COEF2]);
}

static inline unsigned int topology_get_coef2_ltime_scale_dpt_v2(int cpu)
{
	return READ_ONCE(per_cpu(__dpt_rq, cpu).arch_ltime_scale[S_COEF2]);
}

static inline int topology_get_coef2_scale_dpt_v2(int cpu)
{
	return READ_ONCE(per_cpu(__dpt_rq, cpu).util_cfs.perf_scaling_factor[TO_BCORE][COEF2_S_SCALING]);
}

#if IS_ENABLED(CONFIG_UCLAMP_TASK)
static inline unsigned long uclamp_task_util(struct task_struct *p)
{
	return clamp(rt_task(p) ? 0 : task_util_est(p),
		     uclamp_eff_value(p, UCLAMP_MIN),
		     uclamp_eff_value(p, UCLAMP_MAX));
}
#else
static inline unsigned long uclamp_task_util(struct task_struct *p)
{
	return rt_task(p) ? 0 : task_util_est(p);
}
#endif // CONFIG_UCLAMP_TASK
#endif // CONFIG_SMP

void set_dsu_idle_enable(bool boost_ctrl);
void unset_dsu_idle_enable(void);
bool is_dsu_idle_enable(void);

int mtk_available_idle_cpu(int cpu);

unsigned long mtk_cpu_util_next_dpt_v2(int cpu, struct task_struct *p, int dst_cpu, int boost, unsigned long *cpu_util, unsigned long *coef1_util, unsigned long *coef2_util);
unsigned long mtk_cpu_util_cfs_dpt_v2(int cpu, unsigned long *cpu_util, unsigned long *coef1_util, unsigned long *coef2_util);
unsigned long mtk_cpu_util_cfs_boost_dpt_v2(int cpu, unsigned long *cpu_util, unsigned long *coef1_util, unsigned long *coef2_util);

#define EAS_NODE_NAME "eas-info"
#define EAS_PROP_CSRAM "csram-base"
#define EAS_PROP_OFFS_CAP "offs-cap"
#define EAS_PROP_OFFS_THERMAL_S "offs-thermal-limit"

struct eas_info {
	unsigned int csram_base;
	unsigned int offs_cap;
	unsigned int offs_thermal_limit_s;
	bool available;
};

void parse_eas_data(struct eas_info *info);

#endif /* _SCHED_COMMON_H */
