/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _EAS_PLUS_H
#define _EAS_PLUS_H
#include <linux/ioctl.h>
#include <linux/android_vendor.h>
#include "vip.h"

#define MIGR_IDLE_BALANCE               1
#define MIGR_IDLE_PULL_MISFIT_RUNNING   2
#define MIGR_TICK_PULL_MISFIT_RUNNING   3
#define MIGR_IDLE_PULL_VIP_RUNNABLE     4
#define MIGR_SWITCH_PUSH_VIP            5

DECLARE_PER_CPU(unsigned long, max_freq_scale);
DECLARE_PER_CPU(unsigned long, min_freq);

#define LB_FAIL         (0x01)
#define LB_SYNC         (0x02)
#define LB_FAIL_IN_REGULAR (0x03)
#define LB_ZERO_UTIL    (0x04)
#define LB_ZERO_EENV_UTIL    (0x08)
#define LB_PREV         (0x10)
#define LB_LATENCY_SENSITIVE_BEST_IDLE_CPU      (0x20)
#define LB_LATENCY_SENSITIVE_IDLE_MAX_SPARE_CPU (0x40)
#define LB_LATENCY_SENSITIVE_MAX_SPARE_CPU      (0x80)
#define LB_BEST_ENERGY_CPU      (0x100)
#define LB_MAX_SPARE_CPU        (0x200)
#define LB_IN_INTERRUPT		(0x400)
#define LB_IRQ_BEST_IDLE    (0x410)
#define LB_IRQ_SYS_MAX_SPARE   (0x420)
#define LB_IRQ_MAX_SPARE   (0x440)
#define LB_BACKUP_CURR         (0x480)
#define LB_BACKUP_PREV         (0x481)
#define LB_BACKUP_IDLE_CAP      (0x482)
#define LB_BACKUP_AFFINE_WITHOUT_IDLE_CAP (0x483)
#define LB_BACKUP_RECENT_USED_CPU (0x484)
#define LB_BACKUP_AFFINE_IDLE_FIT (0x488)
#define LB_BACKUP_VVIP (0x490)
#define LB_BACKUP_VIP_IN_MASK (0x491)
#define LB_RT_FAIL         (0x1000)
#define LB_RT_FAIL_PD      (0x1001)
#define LB_RT_FAIL_CPU     (0x1002)
#define LB_RT_SYNC      (0x2000)
#define LB_RT_IDLE      (0x4000)
#define LB_RT_NON_IDLE  (0x4005)
#define LB_RT_LOWEST_PRIO         (0x8000)
#define LB_RT_LOWEST_PRIO_NORMAL  (0x8001)
#define LB_RT_LOWEST_PRIO_RT      (0x8002)
#define LB_RT_SOURCE_CPU       (0x10000)
#define LB_RT_FAIL_SYNC        (0x20000)
#define LB_RT_FAIL_RANDOM      (0x40000)
#define LB_RT_NO_LOWEST_RQ     (0x80000)
#define LB_RT_SAME_SYNC      (0x80001)
#define LB_RT_SAME_FIRST     (0x80002)
#define LB_RT_FAIL_FIRST     (0x80004)
#define LB_SHORTCUT_COMPRESS (0x100000)
#define LB_LOOM_OP (0x1000000)
#define LB_LOOM_ALGO (0x1000001)

/*
 * energy_env - Utilization landscape for energy estimation.
 * @task_busy_time: Utilization contribution by the task for which we test the
 *                  placement. Given by eenv_task_busy_time().
 * @pd_busy_time:   Utilization of the whole perf domain without the task
 *                  contribution. Given by eenv_pd_busy_time().
 * @cpu_cap:        Maximum CPU capacity for the perf domain.
 * @pd_cap:         Entire perf domain capacity. (pd->nr_cpus * cpu_cap).
 */
#define MAX_NR_CPUS CONFIG_MAX_NR_CPUS

typedef struct {
	unsigned int cpu_util_local;
	unsigned int total_util_local;
	int IPC_scaling_factor;
} dpt_v2_cap_params_struct;
struct energy_env {

	unsigned long task_busy_time;     /* task util*/
	unsigned long min_cap;            /* min cap of task */
	unsigned long max_cap;            /* max cap of task */

	int dst_cpu;
	unsigned int gear_idx;
	unsigned int pds_busy_time[MAX_NR_CPUS];
	unsigned int cpu_max_util[MAX_NR_CPUS][2]; /* 0: dst_cpu=-1 1: with dst_cpu*/
	unsigned int gear_max_util[MAX_NR_CPUS][2]; /* 0: dst_cpu=-1 1: with dst_cpu*/
	unsigned int pds_cpu_cap[MAX_NR_CPUS];
	unsigned int pds_cap[MAX_NR_CPUS];
	unsigned int pd_base_max_util[MAX_NR_CPUS];
	unsigned long pd_base_freq[MAX_NR_CPUS];
	unsigned int total_util;

	/* temperature for each cpu*/
	int cpu_temp[MAX_NR_CPUS];

	/* WL-based CPU+DSU ctrl */
	unsigned int wl_support;
	unsigned int wl_cpu; /* wl for CPU */
	unsigned int wl_dsu; /* wl for DSU */

	int val_s[10];

	/* dpt v2 */
	int dpt_v2_support;
	unsigned int dpt_v2_freq[MAX_NR_CPUS][2]; /* 0: dst_cpu=-1 1: with dst_cpu*/
	unsigned int dpt_v2_gear_max_freq[MAX_NR_CPUS][2]; /* 0: dst_cpu=-1 1: with dst_cpu*/
	int dpt_v2_swpm_support;
	unsigned int dpt_v2_sratio[MAX_NR_CPUS][2];  /* 0: dst_cpu=-1 1: with dst_cpu*/
	u16 dpt_v2_cpu_util[MAX_NR_CPUS][2];  /* 0: dst_cpu=-1 1: with dst_cpu*/
	u16 dpt_v2_coef1_util[MAX_NR_CPUS][2];  /* 0: dst_cpu=-1 1: with dst_cpu*/
	u16 dpt_v2_coef2_util[MAX_NR_CPUS][2];  /* 0: dst_cpu=-1 1: with dst_cpu*/
	dpt_v2_cap_params_struct dpt_v2_cap_params[MAX_NR_CPUS][2]; /* cpu * NUM_DST_CPU_TYPE * {cpu_util_local, total_util_local, IPC_scaling_factor}*/

	ANDROID_VENDOR_DATA_ARRAY(1, 32);
};

struct rt_energy_aware_output {
	unsigned int rt_cpus;
	unsigned int cfs_cpus;
	unsigned int idle_cpus;
	int cfs_lowest_cpu;
	int cfs_lowest_prio;
	int cfs_lowest_pid;
	int rt_lowest_cpu;
	int rt_lowest_prio;
	int rt_lowest_pid;
	int shortcut;
	int select_reason;
	int rt_aggre_preempt_enable;
};

extern struct cpumask bcpus;

#ifdef CONFIG_SMP
/*
 * The margin used when comparing utilization with CPU capacity.
 *
 * (default: ~20%)
 */
#define fits_capacity(cap, max, margin) ((cap) * margin < (max) * 1024)
unsigned long capacity_of(int cpu);
#endif //CONFIG_SMP

extern int task_fits_capacity(struct task_struct *p, long capacity, int cpu, unsigned int margin);
extern struct perf_domain *find_pd(struct perf_domain *pd, int cpu);

#if IS_ENABLED(CONFIG_MTK_EAS)
extern void hook_sched_balance_find_src_group(void *data, struct sched_group *busiest,
		struct rq *dst_rq, int *out_balance);
extern void mtk_find_energy_efficient_cpu(void *data, struct task_struct *p,
		int prev_cpu, int sync, int *new_cpu, int loom_select_reason);
extern void mtk_cpu_overutilized(void *data, int cpu, int *overutilized);
extern void mtk_overutilized_temp(void *ignore, struct task_struct *p,
							int prev_cpu, int sd_flag,
							int wake_flags, int *target_cpu);

extern unsigned long pd_get_util_cpufreq(struct energy_env *eenv,
		struct cpumask *pd_cpus,unsigned long max_util, unsigned long allowed_cpu_cap,
		unsigned long scale_cpu, unsigned long min,unsigned long max);

/* arch-related API */
#define volt_diff  5000
extern unsigned long pd_get_freq_volt(int cpu, unsigned long freq, int quant, int wl);

extern unsigned long update_dsu_status(struct energy_env *eenv, int quant,
		unsigned long freq, int this_cpu, int dst_cpu);
extern int dsu_freq_changed(void *private);
extern void eenv_dsu_init(void *private, int quant, unsigned int wl,
		int PERCORE_L3_BW, unsigned int cpumask_val, unsigned long *pd_base_freq,
		unsigned int *val, unsigned int *output);
void init_percore_l3_bw(void);

unsigned long get_dsu_pwr(int wl, int dst_cpu, unsigned long task_util,
		unsigned long total_util, void *private, unsigned int extern_volt,
		int dsu_pwr_enable);

extern unsigned long mtk_em_cpu_energy(int pid, struct em_perf_domain *pd,
		unsigned long pd_freq, unsigned long sum_util,
		unsigned long scale_cpu, struct energy_env *eenv,
		unsigned long extern_volt, unsigned long max_util, int candidate_cpu,
		unsigned int dpt_v2_sratio, dpt_v2_cap_params_struct dpt_v2_cap_params);
extern unsigned int new_idle_balance_interval_ns;
#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
extern int sort_thermal_headroom(struct cpumask *cpus, int *cpu_order, bool in_irq);
extern unsigned int thermal_headroom_interval_tick;
#endif //CONFIG_MTK_THERMAL_AWARE_SCHEDULING

extern void mtk_freq_limit_notifier_register(void);
extern int init_sram_info(void);
extern int init_share_buck(void);
extern void mtk_tick_entry(void *data, struct rq *rq);
extern void mtk_set_wake_flags(void *data, int *wake_flags, unsigned int *mode);
extern unsigned long cpu_freq_ceiling(int cpu);
extern unsigned long cpu_cap_ceiling(int cpu);
extern void mtk_pelt_rt_tp(void *data, struct rq *rq);
extern void mtk_sched_switch(void *data, struct task_struct *prev,
		struct task_struct *next, struct rq *rq);
extern void mtk_update_misfit_status(void *data, struct task_struct *p, struct rq *rq, bool *need_update);
extern inline int util_fits_capacity(unsigned long util, unsigned long uclamp_min,
	unsigned long uclamp_max, unsigned long capacity, int cpu);
extern unsigned long task_h_load(struct task_struct *p);

extern void set_wake_sync(unsigned int sync);
extern unsigned int get_wake_sync(void);
extern void set_uclamp_min_ls(unsigned int val);
extern unsigned int get_uclamp_min_ls(void);
extern void set_newly_idle_balance_interval_us(unsigned int interval_us);
extern unsigned int get_newly_idle_balance_interval_us(void);
extern void set_get_thermal_headroom_interval_tick(unsigned int tick);
extern unsigned int get_thermal_headroom_interval_tick(void);

enum VIP_LOOM_SELECTOR {
	NONE,
	ORIGINAL_PATH,
};

/* add struct for user to control soft affinity */
enum {
	TOPAPP_ID,
	FOREGROUND_ID,
	BACKGROUND_ID,
	TG_NUM
};
struct SA_task {
	int pid;
	unsigned int mask;
};
extern void soft_affinity_init(void);
extern void set_top_app_cpumask(unsigned int cpumask_val);
extern void set_foreground_cpumask(unsigned int cpumask_val);
extern void set_background_cpumask(unsigned int cpumask_val);
extern struct cpumask *get_top_app_cpumask(void);
extern struct cpumask *get_foreground_cpumask(void);
extern struct cpumask *get_background_cpumask(void);
extern void set_task_ls(int pid);
extern void unset_task_ls(int pid);
extern struct task_struct *next_vvip_runable_in_cpu(struct rq *rq);
extern struct task_group *search_tg_by_cpuctl_id(unsigned int cpuctl_id);
extern struct task_group *search_tg_by_name(char *group_name);
extern inline void compute_effective_softmask(struct task_struct *p,
		bool *latency_sensitive, struct cpumask *dst_mask);
extern void set_task_ls_prefer_cpus(int pid, unsigned int cpumask_val);
extern void set_task_basic_vip(int pid);
extern void unset_task_basic_vip(int pid);
extern void set_top_app_vip(unsigned int prio);
extern void unset_top_app_vip(void);
extern void set_foreground_vip(unsigned int prio);
extern void unset_foreground_vip(void);
extern void set_background_vip(unsigned int prio);
extern void unset_background_vip(void);
extern void set_ls_task_vip(unsigned int prio);
extern void unset_ls_task_vip(void);
extern void (*change_dpt_support_driver_hook) (int turn_on);

extern void get_most_powerful_pd_and_util_Th(void);

#define EAS_SYNC_SET                            _IOW('g', 1,  unsigned int)
#define EAS_SYNC_GET                            _IOW('g', 2,  unsigned int)
#define EAS_PERTASK_LS_SET                      _IOW('g', 3,  unsigned int)
#define EAS_PERTASK_LS_GET                      _IOR('g', 4,  unsigned int)
#define EAS_ACTIVE_MASK_GET                     _IOR('g', 5,  unsigned int)
#define EAS_NEWLY_IDLE_BALANCE_INTERVAL_SET     _IOW('g', 6,  unsigned int)
#define EAS_NEWLY_IDLE_BALANCE_INTERVAL_GET     _IOR('g', 7,  unsigned int)
#define EAS_GET_THERMAL_HEADROOM_INTERVAL_SET	_IOW('g', 8,  unsigned int)
#define EAS_GET_THERMAL_HEADROOM_INTERVAL_GET	_IOR('g', 9,  unsigned int)
#define EAS_SBB_ALL_SET				_IOW('g', 12,  unsigned int)
#define EAS_SBB_ALL_UNSET			_IOW('g', 13,  unsigned int)
#define EAS_SBB_GROUP_SET			_IOW('g', 14,  unsigned int)
#define EAS_SBB_GROUP_UNSET			_IOW('g', 15,  unsigned int)
#define EAS_SBB_TASK_SET			_IOW('g', 16,  unsigned int)
#define EAS_SBB_TASK_UNSET			_IOW('g', 17,  unsigned int)
#define EAS_SBB_ACTIVE_RATIO		_IOW('g', 18,  unsigned int)
#define EAS_UTIL_EST_CONTROL		_IOW('g', 20,  unsigned int)
#define EAS_TURN_POINT_UTIL_C0		_IOW('g', 21,  unsigned int)
#define EAS_TARGET_MARGIN_C0		_IOW('g', 22,  unsigned int)
#define EAS_TURN_POINT_UTIL_C1		_IOW('g', 23,  unsigned int)
#define EAS_TARGET_MARGIN_C1		_IOW('g', 24,  unsigned int)
#define EAS_TURN_POINT_UTIL_C2		_IOW('g', 25,  unsigned int)
#define EAS_TARGET_MARGIN_C2		_IOW('g', 26,  unsigned int)
#define EAS_SET_CPUMASK_TA		_IOW('g', 27,  unsigned int)
#define EAS_SET_CPUMASK_BACKGROUND	_IOW('g', 28,  unsigned int)
#define EAS_SET_CPUMASK_FOREGROUND	_IOW('g', 29,  unsigned int)
#define EAS_SET_TASK_LS			_IOW('g', 30,  unsigned int)
#define EAS_UNSET_TASK_LS		_IOW('g', 31,  unsigned int)
#define EAS_SET_TASK_LS_PREFER_CPUS		_IOW('g', 32,  struct SA_task)
#define EAS_IGNORE_IDLE_UTIL_CTRL	_IOW('g', 33,  unsigned int)
#define EAS_SET_TASK_VIP			_IOW('g', 34,  unsigned int)
#define EAS_UNSET_TASK_VIP			_IOW('g', 35,  unsigned int)
#define EAS_SET_TA_VIP				_IOW('g', 36,  unsigned int)
#define EAS_UNSET_TA_VIP			_IOW('g', 37,  unsigned int)
#define EAS_SET_FG_VIP				_IOW('g', 38,  unsigned int)
#define EAS_UNSET_FG_VIP			_IOW('g', 39,  unsigned int)
#define EAS_SET_BG_VIP				_IOW('g', 40,  unsigned int)
#define EAS_UNSET_BG_VIP			_IOW('g', 41,  unsigned int)
#define EAS_SET_LS_VIP				_IOW('g', 42,  unsigned int)
#define EAS_UNSET_LS_VIP			_IOW('g', 43,  unsigned int)
#define EAS_GEAR_MIGR_DN_PCT		_IOW('g', 44,  unsigned int)
#define EAS_GEAR_MIGR_UP_PCT		_IOW('g', 45,  unsigned int)
#define EAS_GEAR_MIGR_SET			_IOW('g', 46,  unsigned int)
#define EAS_GEAR_MIGR_UNSET			_IOW('g', 47,  unsigned int)
#define EAS_TASK_GEAR_HINTS_START	_IOW('g', 48,  unsigned int)
#define EAS_TASK_GEAR_HINTS_NUM		_IOW('g', 49,  unsigned int)
#define EAS_TASK_GEAR_HINTS_REVERSE	_IOW('g', 50,  unsigned int)
#define EAS_TASK_GEAR_HINTS_SET		_IOW('g', 51,  unsigned int)
#define EAS_TASK_GEAR_HINTS_UNSET	_IOW('g', 52,  unsigned int)
#define EAS_SET_GAS_CTRL			_IOW('g', 53,  struct gas_ctrl)
#define EAS_SET_GAS_THR				_IOW('g', 54,  struct gas_thr)
#define EAS_RESET_GAS_THR			_IOW('g', 55,  int)
#define EAS_SET_GAS_MARG_THR		_IOW('g', 56,  struct gas_margin_thr)
#define EAS_RESET_GAS_MARG_THR		_IOW('g', 57,  int)
#define EAS_RT_AGGRE_PREEMPT_SET    _IOW('g', 58,  unsigned int)
#define EAS_RT_AGGRE_PREEMPT_RESET  _IOW('g', 59,  unsigned int)
#define EAS_DPT_CTRL				_IOW('g', 60,  int)
#define EAS_RUNNABLE_BOOST_SET                            _IOW('g', 61,  unsigned int)
#define EAS_RUNNABLE_BOOST_UNSET                          _IOW('g', 62,  unsigned int)
#define EAS_SET_DSU_IDLE			_IOW('g', 63,  unsigned int)
#define EAS_UNSET_DSU_IDLE			_IOW('g', 64,  unsigned int)
#define EAS_SET_CURR_TASK_UCLAMP	_IOW('g', 65,  unsigned int)
#define EAS_UNSET_CURR_TASK_UCLAMP	_IOW('g', 66,  unsigned int)
#define EAS_TARGET_MARGIN_LOW_C0		_IOW('g', 67,  unsigned int)
#define EAS_TARGET_MARGIN_LOW_C1		_IOW('g', 68,  unsigned int)
#define EAS_TARGET_MARGIN_LOW_C2		_IOW('g', 69,  unsigned int)
#define EAS_UNSET_TARGET_MARGIN_C0		_IOW('g', 70,  unsigned int)
#define EAS_UNSET_TARGET_MARGIN_C1		_IOW('g', 71,  unsigned int)
#define EAS_UNSET_TARGET_MARGIN_C2		_IOW('g', 72,  unsigned int)
#define EAS_UNSET_TARGET_MARGIN_LOW_C0		_IOW('g', 73,  unsigned int)
#define EAS_UNSET_TARGET_MARGIN_LOW_C1		_IOW('g', 74,  unsigned int)
#define EAS_UNSET_TARGET_MARGIN_LOW_C2		_IOW('g', 75,  unsigned int)
#define EAS_SET_SHORTCUT_COMPRESS_RATE                    _IOW('g', 76, int)
#define EAS_RESET_SHORTCUT_COMPRESS_RATE                  _IOW('g', 77, int)
#define EAS_SET_SHORTCUT_COMPRESS_RELAX_ENOUGH_CPU_UTIL   _IOW('g', 78, struct shortcut_compress_relax_enough_args)
#define EAS_RESET_SHORTCUT_COMPRESS_RELAX_ENOUGH_CPU_UTIL _IOW('g', 79, int)
#define EAS_SET_SHORTCUT_COMPRESS_RELAX_ENOUGH_TSK_UTIL   _IOW('g', 80, struct shortcut_compress_relax_enough_args)
#define EAS_RESET_SHORTCUT_COMPRESS_RELAX_ENOUGH_TSK_UTIL _IOW('g', 81, int)
#define EAS_SET_GH_GATHERING_TH                           _IOW('g', 82, int)
#define EAS_RESET_GH_GATHERING_TH                         _IOW('g', 83, int)
#define EAS_RUNNABLE_BOOST_UTIL_EST_SET                   _IOW('g', 84, int)
#define EAS_RUNNABLE_BOOST_UTIL_EST_UNSET                 _IOW('g', 85, int)

extern void update_curr_collab_state(bool *is_cpu_to_update_thermal);
#if IS_ENABLED(CONFIG_MTK_NEWIDLE_BALANCE)
extern void hook_sched_balance_newidle(void *data, struct rq *this_rq,
		struct rq_flags *rf, int *pulled_task, int *done);
#endif //CONFIG_MTK_NEWIDLE_BALANCE

extern unsigned long calc_pwr(int cpu, unsigned long task_util);
extern unsigned long calc_pwr_eff(int wl, int cpu, unsigned long cpu_util, int *val_s, int dpt_v2_support, dpt_v2_cap_params_struct dpt_v2_cap_params);
extern unsigned long shared_buck_calc_pwr_eff(struct energy_env *eenv,
		int cpu, struct task_struct *p, unsigned long max_util, struct cpumask *cpus,
		bool is_dsu_pwr_triggered, unsigned long min, unsigned long max,
		int dpt_v2_support, dpt_v2_cap_params_struct dpt_v2_cap_params);
#endif // CONFIG_MTK_EAS

extern void hook_sched_tick(void *data, struct rq *rq);
#if IS_ENABLED(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)
extern bool system_has_many_heavy_task(void);
extern void task_check_for_rotation(struct rq *src_rq);
extern void rotat_after_enqueue_task(void *data, struct rq *rq, struct task_struct *p);
extern void rotat_task_stats(void *data, struct task_struct *p);
extern void rotat_task_newtask(void __always_unused *data, struct task_struct *p,
				unsigned long clone_flags);
#endif //CONFIG_MTK_SCHED_BIG_TASK_ROTATE
extern void mtk_hook_after_enqueue_task(void *data, struct rq *rq,
				struct task_struct *p, int flags);
extern void mtk_hook_after_dequeue_task(void *data, struct rq *rq,
				struct task_struct *p, int flags, bool *dequeue_task_result);
extern void hook_enqueue_task_fair(void *data, struct rq *rq,
				struct task_struct *p, int flags);
extern void hook_dequeue_task_fair(void *data, struct rq *rq,
				struct task_struct *p, int flags);
extern void mtk_select_task_rq_rt(void *data, struct task_struct *p, int cpu, int sd_flag,
				int flags, int *target_cpu);
extern int mtk_sched_asym_cpucapacity;

extern void mtk_find_lowest_rq(void *data, struct task_struct *p, struct task_struct *exec_ctx,
				struct cpumask *lowest_mask, int ret, int *lowest_cpu);

extern void throttled_rt_tasks_debug(void *unused, int cpu, u64 clock,
				ktime_t rt_period, u64 rt_runtime, s64 rt_period_timer_expires);

extern void mtk_update_rq_clock_pelt(void *unused, struct rq *rq, s64 delta, int *ret);
extern void mtk_update_load_avg_blocked_se(void *unused, u64 now, struct sched_entity *se, int *ret);
extern void mtk_update_load_avg_se(void *unused, u64 now, struct cfs_rq *cfs_rq, struct sched_entity *se, int *ret);
extern void mtk_update_load_avg_cfs_rq(void *unused, u64 now, struct cfs_rq *cfs_rq, int *ret);
extern void mtk_update_rt_rq_load_avg_internal(void *unused, u64 now, struct rq *rq, int running, int *ret);
extern void mtk_attach_entity_load_avg(void *unused, struct cfs_rq *cfs_rq, struct sched_entity *se);
extern void mtk_detach_entity_load_avg(void *unused, struct cfs_rq *cfs_rq, struct sched_entity *se);
extern void mtk_enqueue_task_fair(void *unused, struct rq *rq, struct task_struct *p, int flags);
extern void mtk_dequeue_task_fair(void *unused, struct rq *rq, struct task_struct *p, int flags);
extern void mtk_remove_entity_load_avg(void *unused, struct cfs_rq *cfs_rq, struct sched_entity *se);
extern void sched_task_util_est_hook(void *data, struct sched_entity *se);

extern bool sched_skip_hiIRQ_enable_get(void);
extern bool sched_rt_aggre_preempt_enable_get(void);
extern void init_skip_hiIRQ(void);
extern void init_rt_aggre_preempt(void);
extern void set_rt_aggre_preempt(int val);
extern bool get_rt_aggre_preempt(void);
extern int cpu_high_irqload(int cpu);
extern unsigned int mtk_get_idle_exit_latency(int cpu, struct rt_energy_aware_output *rt_ea_output);
extern unsigned long mtk_sched_cpu_util(int cpu);
extern unsigned long mtk_sched_max_util(struct task_struct *p, int cpu,
					unsigned long min_cap, unsigned long max_cap);
extern void track_sched_cpu_util(struct task_struct *p, int cpu,
					unsigned long min_cap, unsigned long max_cap);
extern int get_cpu_irqUtil_threshold(int cpu);
extern int get_cpu_irqRatio_threshold(int cpu);

extern struct cpumask __cpu_pause_mask;
#define cpu_pause_mask ((struct cpumask *)&__cpu_pause_mask)

#if IS_ENABLED(CONFIG_MTK_CORE_PAUSE)
#define cpu_paused(cpu) cpumask_test_cpu((cpu), cpu_pause_mask)

extern void sched_pause_init(void);
#else
#define cpu_paused(cpu) 0
#endif // CONFIG_MTK_CORE_PAUSE
#define DPT_TURN_ON (is_dpt_support_driver_hook != NULL && is_dpt_support_driver_hook())
extern int set_target_margin(int gearid, int margin);
extern int set_turn_point_freq(int gearid, unsigned long turn_freq);
extern int set_target_margin_low(int gearid, int margin);
extern int unset_target_margin(int cpu);
extern int unset_target_margin_low(int cpu);
extern int set_flt_margin(int gearid, int margin);

extern int set_util_est_ctrl(bool enable);
struct share_buck_info {
	int gear_idx;
	struct cpumask *cpus;
};

extern struct share_buck_info share_buck;
extern int get_share_buck(void);
extern int sched_cgroup_state(struct task_struct *p, int subsys_id);

extern inline int util_fits_capacity_dpt_v2(unsigned long cpu_util_without_uclamp_local, unsigned long coef1_util_local,
	unsigned long coef2_util_local, unsigned long uclamp_min, unsigned long uclamp_max, unsigned long local_capacity_of, int cpu, unsigned long *cpu_util_uclamped);
extern unsigned long mtk_em_cpu_energy_v2(struct em_perf_domain *pd,
		struct energy_env *eenv, unsigned long pd_freq, int cpu, unsigned long ipc_p, unsigned long cycle_p);
extern inline unsigned long eenv_pd_max_util_dpt_v2(struct energy_env *eenv, struct cpumask *pd_cpus,
		 struct task_struct *p, int dst_cpu, unsigned long *min, unsigned long *max);
extern void __iomem *(*get_dpt_v2_cs_counter_addr_hook)(int cpu);

#endif //_EAS_PLUS_H
