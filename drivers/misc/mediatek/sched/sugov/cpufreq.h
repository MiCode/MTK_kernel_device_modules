/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __CPUFREQ_H__
#define __CPUFREQ_H__
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/irq_work.h>

#define MAX_CAP_ENTRYIES 168

#define SRAM_REDZONE 0x55AA55AAAA55AA55
#define CAPACITY_TBL_OFFSET 0xFA0
#define CAPACITY_TBL_SIZE 0x100
#define CAPACITY_ENTRY_SIZE 0x2
#define REG_FREQ_SCALING 0x4cc
#define LUT_ROW_SIZE 0x4
#define SBB_ALL 0
#define SBB_GROUP 1
#define SBB_TASK 2
#define MAX_NR_CPUS CONFIG_MAX_NR_CPUS

struct sbb_cpu_data {
	unsigned int active;
	unsigned int idle_time;
	unsigned int wall_time;
	unsigned int boost_factor;
	unsigned int tick_start;
	unsigned int cpu_utilize;
};

#if !IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
struct mtk_em_perf_state {
	/* Performance state setting */
	unsigned int freq;
	unsigned int volt;
	unsigned int capacity;
	unsigned int dyn_pwr;
	unsigned int pwr_eff;
	unsigned int dsu_freq;
};
#endif

enum sugov_type {
	OPP,
	CAP,
	FREQ,
	PWR_EFF,
};

struct cpu_weighting {
	unsigned int dsu_weighting;
	unsigned int emi_weighting;
};

struct pd_capacity_info {
	unsigned int nr_cpus;
	unsigned int dsu_weighting;
	unsigned int emi_weighting;
	int nr_caps;
	int nr_caps_legacy;
	unsigned int freq_max;
	unsigned int freq_min;
	/* table[0].freq => the max freq.
	 * table[0].capacity => the max capacity.
	 */
	struct mtk_em_perf_state *table;
	struct mtk_em_perf_state *table_legacy;

	// for util mapping in O(1)
	int nr_util_opp_map;
	int *util_opp_map;
	int *util_opp_map_legacy;

	// for freq mapping in O(1)
	unsigned int DFreq;
	u32 inv_DFreq;
	int nr_freq_opp_map;
	int *freq_opp_map;
	int *freq_opp_map_legacy;
};

struct sugov_tunables {
	struct gov_attr_set	attr_set;
	unsigned int		up_rate_limit_us;
	unsigned int		down_rate_limit_us;
};

struct sugov_policy {
	struct cpufreq_policy	*policy;

	struct sugov_tunables	*tunables;
	struct list_head	tunables_hook;

	raw_spinlock_t		update_lock;	/* For shared policies */
	u64			last_freq_update_time;
	s64			min_rate_limit_ns;
	s64			up_rate_delay_ns;
	s64			down_rate_delay_ns;
	unsigned int		next_freq;
	unsigned int		cached_raw_freq;

	/* The next fields are only needed if fast switch cannot be used: */
	struct			irq_work irq_work;
	struct			kthread_work work;
	struct			mutex work_lock;
	struct			kthread_worker worker;
	struct task_struct	*thread;
	bool			work_in_progress;

	bool			limits_changed;
	bool			need_freq_update;
};

struct dsu_state {
	unsigned int freq;
	unsigned int volt;
	unsigned int dyn_pwr;
	unsigned int BW;
	unsigned int EMI_BW;
};

struct dsu_table {
	unsigned int nr_opp;
	unsigned int freq_max;
	unsigned int freq_min;
	struct dsu_state **tbl;

	/* for O1 Mapping */
	unsigned int min_gap_log2;
	unsigned int nr_opp_map;
	unsigned int *opp_map;
};

struct cpu_dsu_freq_state {
	bool is_eas_dsu_support;
	bool is_eas_dsu_ctrl;
	unsigned int pd_count;
	unsigned int dsu_target_freq;
	unsigned int *cpu_freq;
	unsigned int *dsu_freq_vote;
};

extern struct dsu_state *dsu_get_opp_ps(int wl_type, int opp);
extern unsigned int dsu_get_freq_opp(unsigned int freq);
extern int init_dsu(void);
extern void update_wl_tbl(unsigned int cpu);
extern int get_curr_wl(void);
extern int get_classify_wl(void);
extern int get_em_wl(void);
extern void set_wl_type_manual(int val);
extern int get_wl_type_manual(void);
extern int get_nr_wl_type(void);
extern int get_nr_cpu_type(void);
extern int get_cpu_type(int type);
#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
int init_opp_cap_info(struct proc_dir_entry *dir);
void clear_opp_cap_info(void);

extern int get_eas_hook(void);
extern int pd_opp2freq(int cpu, int opp, int quant, int wl);
extern int pd_opp2cap(int cpu, int opp, int quant, int wl, int *val_s, int r_o, char *caller);
extern int pd_opp2pwr_eff(int cpu, int opp, int quant, int wl, int *val_s, int r_o, char *caller);
extern int pd_opp2dyn_pwr(int cpu, int opp, int quant, int wl, int *val_s, int r_o, char *caller);
extern int pd_opp2volt(int cpu, int opp, int quant, int wl);
extern int pd_util2opp(int cpu, int util, int quant, int wl, int *val_s, int r_o, char *caller);
extern int pd_freq2opp(int cpu, int freq, int quant, int wl);
extern int pd_cpu_volt2opp(int cpu, int volt, int quant, int wl);
extern int pd_freq2util(unsigned int cpu, int freq, bool quant, int wl, int *val_s, int r_o);
extern int pd_util2freq(unsigned int cpu, int util, bool quant, int wl);
extern int pd_cpu_opp2dsu_freq(int cpu, int opp, int quant, int wl);
extern int pd_dsu_volt2opp(int volt);
extern int pd_get_dsu_freq(void);
unsigned long pd_cpu_freq2dsu_freq(unsigned int cpu, int freq, bool quant, int wl);
extern unsigned long pd_X2Y(int cpu, unsigned long input, enum sugov_type in_type,
		enum sugov_type out_type, bool quant, char *caller);

extern unsigned long pd_get_opp_capacity(unsigned int cpu, int opp);
extern unsigned long pd_get_opp_capacity_legacy(unsigned int cpu, int opp);
extern unsigned long pd_get_opp_freq(unsigned int cpu, int opp);
extern unsigned long pd_get_opp_freq_legacy(unsigned int cpu, int opp);

extern unsigned long pd_get_freq_util(unsigned int cpu, unsigned long freq, int *val_s, int r_o, char *caller);
extern unsigned long pd_get_freq_opp(unsigned int cpu, unsigned long freq);
extern unsigned long pd_get_freq_pwr_eff(unsigned int cpu, unsigned long freq);
extern unsigned long pd_get_freq_opp_legacy(unsigned int cpu, unsigned long freq);
extern unsigned long pd_get_freq_opp_legacy_type(int wl_type, unsigned int cpu, unsigned long freq);

extern unsigned long pd_get_util_freq(unsigned int cpu, unsigned long util);
extern unsigned long pd_get_util_pwr_eff(unsigned int cpu, unsigned long util, char *caller);
extern unsigned long pd_get_util_opp(unsigned int cpu, unsigned long util);
extern unsigned long pd_get_util_opp_legacy(unsigned int cpu, unsigned long util);

extern unsigned int pd_get_cpu_opp(unsigned int cpu);
extern unsigned int pd_get_opp_leakage(unsigned int cpu, unsigned int opp,
	unsigned int temperature);
extern unsigned int pd_get_dsu_weighting(int wl_type, unsigned int cpu);
extern unsigned int pd_get_emi_weighting(int wl_type, unsigned int cpu);
extern unsigned int get_curr_cap(unsigned int cpu);
extern int get_fpsgo_bypass_flag(void);
extern void (*fpsgo_notify_fbt_is_boost_fp)(int fpsgo_is_boost);
#if IS_ENABLED(CONFIG_NONLINEAR_FREQ_CTL)
void mtk_cpufreq_fast_switch(void *data, struct cpufreq_policy *policy,
				unsigned int *target_freq, unsigned int old_target_freq);
void mtk_arch_set_freq_scale(void *data, const struct cpumask *cpus,
				unsigned long freq, unsigned long max, unsigned long *scale);
extern int set_sched_capacity_margin_dvfs(int capacity_margin);
extern int get_sched_capacity_margin_dvfs(void);
#if IS_ENABLED(CONFIG_UCLAMP_TASK_GROUP)
extern void mtk_uclamp_eff_get(void *data, struct task_struct *p, enum uclamp_id clamp_id,
		struct uclamp_se *uc_max, struct uclamp_se *uc_eff, int *ret);
#endif
extern bool cu_ctrl;
extern bool get_curr_uclamp_ctrl(void);
extern void set_curr_uclamp_ctrl(int val);
extern int set_curr_uclamp_hint(int pid, int set);
extern int get_curr_uclamp_hint(int pid);
extern bool gu_ctrl;
extern bool get_gear_uclamp_ctrl(void);
extern void set_gear_uclamp_ctrl(int val);
extern int get_gear_uclamp_max(int gearid);
extern int get_cpu_gear_uclamp_max(unsigned int cpu);
extern int get_cpu_gear_uclamp_max_capacity(unsigned int cpu);
extern void set_gear_uclamp_max(int gearid, int val);
#endif
#endif
/* ignore idle util */
extern bool ignore_idle_ctrl;
extern void set_ignore_idle_ctrl(bool val);
extern bool get_ignore_idle_ctrl(void);
/* sbb */
extern void set_target_active_ratio_pct(int gear_id, int val);
extern void set_sbb(int flag, int pid, bool set);
extern void set_sbb_active_ratio_gear(int gear_id, int val);
extern void set_sbb_active_ratio(int val);
extern int get_sbb_active_ratio_gear(int gear_id);
extern bool is_sbb_trigger(struct rq *rq);
extern unsigned int get_nr_gears(void);
extern struct cpumask *get_gear_cpumask(unsigned int gear);
extern bool is_gearless_support(void);
/* dsu ctrl */
extern int wl_type_delay_ch_cnt;
extern bool get_eas_dsu_ctrl(void);
extern void set_eas_dsu_ctrl(bool set);
extern void set_dsu_ctrl(bool set);
extern struct cpu_dsu_freq_state *get_dsu_freq_state(void);
bool enq_force_update_freq(struct sugov_policy *sg_policy);

/* adaptive margin */
extern int am_support;
extern int get_am_ctrl(void);
extern void set_am_ctrl(int set);
extern unsigned int get_adaptive_margin(unsigned int cpu);
extern int get_gear_max_active_ratio_cap(int gear);
extern int get_cpu_active_ratio_cap(int cpu);
extern void update_active_ratio_all(void);
extern void set_am_ceiling(int val);
extern int get_am_ceiling(void);

/* group aware dvfs */
extern int grp_dvfs_support_mode;
extern int grp_dvfs_ctrl_mode;
extern int get_grp_dvfs_ctrl(void);
extern void set_grp_dvfs_ctrl(int set);
extern bool get_grp_high_freq(int cluster_id);
extern void set_grp_high_freq(int cluster_id, bool set);


extern unsigned long get_turn_point_freq(int gearid);
DECLARE_PER_CPU(unsigned int, gear_id);
DECLARE_PER_CPU(struct sbb_cpu_data *, sbb);
DECLARE_PER_CPU(struct mtk_rq *, rq_data);

__weak extern unsigned int mtk_get_leakage(unsigned int cpu, unsigned int idx,
	unsigned int degree)
{
	return 0;
}
__weak extern unsigned int mtk_get_dsu_freq(void) { return 0; }
__weak int em_ver(void) { return 2; }

/* DPT */
struct curr_collab_state_struct {
	int state;
	int (*ret_function)(void);
};

extern void *get_dpt_base_info(void);
extern struct curr_collab_state_struct *get_curr_collab_state(void);
extern void update_curr_collab_state(void);
extern struct curr_collab_state_struct *curr_collab_state;
extern int nr_collab_type;
extern int get_nr_collab_type(void);
extern void set_collab_state_manual(int type, int state);
extern void (*change_dpt_support_driver_hook) (int turn_on);
extern int (*is_dpt_support_driver_hook) (void);
extern void (*set_v_driver_hook)(int v);
#define for_each_collab_type(collab_type) for (collab_type = 0; collab_type < nr_collab_type; collab_type++)
int collab_type_0_ret_function(void);
/* End of DPT */

#endif /* __CPUFREQ_H__ */
