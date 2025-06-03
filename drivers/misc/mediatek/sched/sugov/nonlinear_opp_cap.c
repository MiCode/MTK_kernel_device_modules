// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/percpu.h>
#include <linux/sort.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <sched/autogroup.h>
#include <sched/pelt.h>
#include <linux/sched/clock.h>
#include <linux/energy_model.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/cpuset.h>
#include "common.h"
#include "cpufreq.h"
#include "mtk_unified_power.h"
#include "sugov_trace.h"
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
#include "mtk_energy_model/v3/energy_model.h"
#else
#include "mtk_energy_model/v1/energy_model.h"
#endif // CONFIG_MTK_GEARLESS_SUPPORT
#include "mediatek-cpufreq-hw_fdvfs.h"
#include "util/cpu_util.h"
#include "dsu_interface.h"
#include <mt-plat/mtk_irq_mon.h>
#include "eas/group.h"
#include "sched_version_ctrl.h"
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
#include <thermal_interface.h>
#endif // CONFIG_MTK_THERMAL_INTERFACE

DEFINE_PER_CPU(struct sbb_cpu_data *, sbb);
EXPORT_SYMBOL(sbb);
static void __iomem *l3ctl_sram_base_addr;
#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
static struct resource *csram_res;
static struct cpumask *pd_cpumask;
static int *cpu2cluster_id;
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
static void __iomem *sram_base_addr_freq_scaling;
static bool freq_scaling_disabled = true;
#endif // CONFIG_MTK_GEARLESS_SUPPORT
static int pd_count;
static int busy_tick_boost_all;
static int sbb_active_ratio[MAX_NR_CPUS] = {
	[0 ... MAX_NR_CPUS - 1] = 100 };
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
static unsigned int wl_delay_update_tick = 2;
#endif // CONFIG_MTK_GEARLESS_SUPPORT

static int fpsgo_boosting; //0 : disable, 1 : enable
#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
void (*flt_get_fpsgo_boosting)(int fpsgo_flag);
EXPORT_SYMBOL(flt_get_fpsgo_boosting);
#endif // CONFIG_MTK_SCHED_FAST_LOAD_TRACKING

#define CHECK_ADDR(addr, msg) \
    if (!(addr)) { pr_info(msg); return -EIO; }
#define wl_valid(wl) (wl >= 0 && wl < nr_wl)
int get_wl_dsu(void);

/* DPT */
static void __iomem *dpt_sram_base;
static void __iomem *coef1_sratio_addr, *coef2_sratio_addr, *cpu_sratio_addr;
static void __iomem *coef1_ltime_addr, *coef2_ltime_addr;

static unsigned long last_jiffies_dpt;
static DEFINE_SPINLOCK(update_dpt_lock);
struct curr_collab_state_struct *curr_collab_state;
EXPORT_SYMBOL_GPL(curr_collab_state);
int nr_collab_type = -1;
int *curr_collab_state_manual;
int *wl_collab_type_mapping_mask;
void (*change_dpt_support_driver_hook) (int turn_on);
EXPORT_SYMBOL(change_dpt_support_driver_hook);
int val_m = 1;
EXPORT_SYMBOL_GPL(val_m);
int dpt_default_status;
bool __dpt_init_done;

/* DPT v2 */
int coef1_min_ltime, coef1_max_ltime, coef2_min_ltime, coef2_max_ltime;
int init_dpt_v2_driver_done;
int scaling_factor_shift_bit;

void update_dpt_v2_info(void);
inline int is_dpt_v2_support (void);

void (*dpt_v2_uclamp2local_cap_hook)(int cpu, int quant, unsigned long *umin, unsigned long *umax);
EXPORT_SYMBOL(dpt_v2_uclamp2local_cap_hook);
void (*get_coef1_min_max_ltime_hook)(int *min_ltime, int *max_ltime);
EXPORT_SYMBOL(get_coef1_min_max_ltime_hook);
void (*get_coef2_min_max_ltime_hook)(int *min_ltime, int *max_ltime);
EXPORT_SYMBOL(get_coef2_min_max_ltime_hook);
unsigned long (*dpt_v2_opp2pwr_eff_hook)(int cpu, int opp, int quant, int wl_type, int *swpm_vars,
	unsigned long cpu_util_local, unsigned long total_util_local, int IPC_scaling_factor);
EXPORT_SYMBOL(dpt_v2_opp2pwr_eff_hook);
unsigned long (*dpt_v2_linear_local_cap2opp_hook)(int cpu, int quant, unsigned long util);
EXPORT_SYMBOL(dpt_v2_linear_local_cap2opp_hook);
unsigned long (*dpt_v2_opp2global_cap_hook)(int quant, int gear_idx, int opp, unsigned long cpu_util_local, unsigned long total_util_local, int IPC_scaling_factor);
EXPORT_SYMBOL(dpt_v2_opp2global_cap_hook);
int (*get_scaling_factor_shift_bit_hook)(void);
EXPORT_SYMBOL(get_scaling_factor_shift_bit_hook);
void __iomem *(*get_dpt_v2_cs_counter_addr_hook)(int cpu);
EXPORT_SYMBOL(get_dpt_v2_cs_counter_addr_hook);
unsigned long (*dpt_v2_linear_local_cap2freq_hook)(int cpu, int quant, unsigned int util, unsigned long umin, unsigned long umax, int rescale_linearly);
EXPORT_SYMBOL(dpt_v2_linear_local_cap2freq_hook);
void (*get_cpu_perf_scaling_factor_hook)(int cpu, perf_scaling_factor_arr *task_perf_scaling_factor);
EXPORT_SYMBOL(get_cpu_perf_scaling_factor_hook);
void (*get_cpu_power_scaling_factor_hook)(int cpu, int *task_power_scaling_factor);
EXPORT_SYMBOL(get_cpu_power_scaling_factor_hook);
void (*check_cpu_perf_scaling_factor_hook)(int cpu, perf_scaling_factor_arr *task_perf_scaling_factor);
EXPORT_SYMBOL(check_cpu_perf_scaling_factor_hook);
void (*check_cpu_power_scaling_factor_hook)(int cpu, int *task_power_scaling_factor);
EXPORT_SYMBOL(check_cpu_power_scaling_factor_hook);
unsigned long (*rescale_cpu_util_upscale_hook)(int cpu, int quant, unsigned long cpu_util, unsigned long freq);
EXPORT_SYMBOL(rescale_cpu_util_upscale_hook);
unsigned long (*dpt_v2_util2cap_needed_local_hook)(unsigned long cpu_util_local, unsigned long coef1_util_local,
	unsigned long coef2_util_local, unsigned long *coefs);
EXPORT_SYMBOL(dpt_v2_util2cap_needed_local_hook);
void (*change_dpt_v2_support_hook) (int status);
EXPORT_SYMBOL(change_dpt_v2_support_hook);
void (*set_dpt_v2_default_status_hook) (int status);
EXPORT_SYMBOL(set_dpt_v2_default_status_hook);
int (*get_dpt_v2_default_status_hook) (void);
EXPORT_SYMBOL(get_dpt_v2_default_status_hook);
int (*is_dpt_v2_support_hook) (void);
EXPORT_SYMBOL(is_dpt_v2_support_hook);
unsigned long (*rescale_coef1_util_or_ratio_hook) (unsigned long coef1_val, int type);
EXPORT_SYMBOL(rescale_coef1_util_or_ratio_hook);
unsigned long (*rescale_coef2_util_or_ratio_hook) (unsigned long coef2_val, int type);
EXPORT_SYMBOL(rescale_coef2_util_or_ratio_hook);
unsigned long (*dpt_v2_freq2linear_cap_local_hook) (int quant, int cpu, unsigned long freq);
EXPORT_SYMBOL(dpt_v2_freq2linear_cap_local_hook);
void (*update_coef1_ltime_perf_model_hook) (unsigned long curr_coef1_ltime);
EXPORT_SYMBOL(update_coef1_ltime_perf_model_hook);
void (*update_coef2_ltime_perf_model_hook) (unsigned long curr_coef2_ltime);
EXPORT_SYMBOL(update_coef2_ltime_perf_model_hook);
unsigned long (*get_coef1_ltime_perf_model_result_hook) (void);
EXPORT_SYMBOL(get_coef1_ltime_perf_model_result_hook);
unsigned long (*get_coef2_ltime_perf_model_result_hook) (void);
EXPORT_SYMBOL(get_coef2_ltime_perf_model_result_hook);
void (*dpt_v2_init_done_hook) (void);
EXPORT_SYMBOL(dpt_v2_init_done_hook);

DEFINE_PER_CPU(dpt_rq_t, __dpt_rq);
EXPORT_PER_CPU_SYMBOL(__dpt_rq);
DEFINE_PER_CPU(unsigned long, max_freq) = 0;
EXPORT_PER_CPU_SYMBOL(max_freq);
DEFINE_PER_CPU(unsigned long, thermal_freq); /* to sync with thermal_pressure */
EXPORT_PER_CPU_SYMBOL(thermal_freq);
DEFINE_PER_CPU(unsigned long, freq_ceiling);
EXPORT_PER_CPU_SYMBOL(freq_ceiling);
DEFINE_PER_CPU(unsigned long, freq_floor);
EXPORT_PER_CPU_SYMBOL(freq_floor);
DEFINE_PER_CPU(unsigned long, thermal_freq_ceiling_ratio) = 1 << FREQ_CEILING_RATIO_BIT;
EXPORT_PER_CPU_SYMBOL(thermal_freq_ceiling_ratio);
DEFINE_PER_CPU(unsigned long, freq_ceiling_ratio) = 1 << FREQ_CEILING_RATIO_BIT;
EXPORT_PER_CPU_SYMBOL(freq_ceiling_ratio);
DEFINE_PER_CPU(unsigned long, dpt_v2_cpu_capacity_all_util_local) = DPT_V2_MAX_RUNNING_TIME;
EXPORT_PER_CPU_SYMBOL(dpt_v2_cpu_capacity_all_util_local);
/* End of DPT */

#define MAX_PD_COUNT				3
#define COEF1_COEF2_S_PER_CPU_SIZE	0x1
#define CPU_S_PER_CPU_SIZE		0x4

struct pd_capacity_info {
	int nr_caps;
	unsigned int *util_opp;
	unsigned int *util_freq;
	unsigned long *caps;
	struct cpumask cpus;
};

/* weight mode */
enum _flt_cpu_type {
	FLT_CPU_TAR = 0,
	FLT_CPU_S = 1,
	FLT_CPU_NS = 2,
	FLT_CPU_NUM,
};

static int entry_count;
static int legacy_pd_count;
static struct pd_capacity_info *pd_capacity_tbl;
static int legacy_pd_enable;

/* group aware dvfs */
int grp_dvfs_support_mode;
int grp_dvfs_ctrl_mode;
void init_grp_dvfs(void)
{
	if (grp_dvfs_support_mode)
		pr_info("grp_dvfs enable\n");
}

int get_grp_dvfs_ctrl(void)
{
	return grp_dvfs_ctrl_mode;
}
EXPORT_SYMBOL_GPL(get_grp_dvfs_ctrl);

void set_grp_dvfs_ctrl(int set)
{
	grp_dvfs_ctrl_mode = (set && grp_dvfs_support_mode) ? set : 0;
}
EXPORT_SYMBOL_GPL(set_grp_dvfs_ctrl);

/* adaptive margin */
int am_support;
int am_ctrl; /* 0: disable, 1: calculate adaptive ratio, 2: adaptive margin */
void init_adaptive_margin(void)
{
	if (am_support) {
		am_ctrl = 1;
		pr_info("adaptive-margin enable\n");
	} else {
		am_ctrl = 0;
	}
}
int get_am_ctrl(void)
{
	return am_ctrl;
}
EXPORT_SYMBOL_GPL(get_am_ctrl);

void set_am_ctrl(int set)
{
	am_ctrl = (set && am_support) ? set : 0;
}
EXPORT_SYMBOL_GPL(set_am_ctrl);

/* eas dsu ctrl */
enum {
	REG_FREQ_LUT_TABLE,
	REG_FREQ_ENABLE,
	REG_FREQ_PERF_STATE,
	REG_FREQ_HW_STATE,
	REG_EM_POWER_TBL,
	REG_FREQ_LATENCY,
	REG_ARRAY_SIZE,
};

struct cpufreq_mtk {
	struct cpufreq_frequency_table *table;
	void __iomem *reg_bases[REG_ARRAY_SIZE];
	int nr_opp;
	unsigned int last_index;
	cpumask_t related_cpus;
	long sb_ch;
};
static struct cpu_dsu_freq_state freq_state;
void init_eas_dsu_ctrl(void)
{
	unsigned int i;

	freq_state.is_eas_dsu_support = true;
	freq_state.is_eas_dsu_ctrl = true;
	freq_state.pd_count = pd_count;
	freq_state.cpu_freq = kcalloc(MAX_NR_CPUS, sizeof(unsigned int), GFP_KERNEL);
	for (i = 0; i < MAX_NR_CPUS; i++)
		freq_state.cpu_freq[i] = 0;
	freq_state.dsu_freq_vote = kcalloc(pd_count, sizeof(unsigned int), GFP_KERNEL);
	pr_info("eas_dsu_sup.=%d\n", freq_state.is_eas_dsu_support);
}

bool enq_force_update_freq(struct sugov_policy *sg_policy)
{
	struct cpufreq_policy *policy = sg_policy->policy;
	struct sugov_rq_data *sugov_data_ptr;

	if (!freq_state.is_eas_dsu_support || !freq_state.is_eas_dsu_ctrl)
		return false;
	sugov_data_ptr = &per_cpu(rq_data, policy->cpu)->sugov_data;
	if (!READ_ONCE(sugov_data_ptr->enq_update_dsu_freq))
		return false;
	WRITE_ONCE(sugov_data_ptr->enq_update_dsu_freq, false);
	return true;
}

bool get_eas_dsu_ctrl(void)
{
	return freq_state.is_eas_dsu_ctrl;
}
EXPORT_SYMBOL_GPL(get_eas_dsu_ctrl);

static int wl_cpu_manual = -1;
static int wl_dsu_manual = -1;
// eas dsu ctrl on / off
void set_eas_dsu_ctrl(bool set)
{
	int i;

	if (freq_state.is_eas_dsu_support) {
		freq_state.is_eas_dsu_ctrl = set;
		if (freq_state.is_eas_dsu_ctrl == false) {
			freq_state.dsu_target_freq = 0;
			for (i = 0; i < freq_state.pd_count; i++)
				freq_state.dsu_freq_vote[i] = 0;
		}
	}
}
EXPORT_SYMBOL_GPL(set_eas_dsu_ctrl);

// eas or legacy dsu ctrl
void set_dsu_ctrl(bool set)
{
	if (freq_state.is_eas_dsu_support) {
		iowrite32(set ? 1 : 0, l3ctl_sram_base_addr + 0x1C);
		set_eas_dsu_ctrl(set);
	}
}
EXPORT_SYMBOL_GPL(set_dsu_ctrl);

struct cpu_dsu_freq_state *get_dsu_freq_state(void)
{
	return &freq_state;
}
EXPORT_SYMBOL_GPL(get_dsu_freq_state);

int (*mtk_dsu_freq_agg_hook)(int cpu, int max_freq_in_gear, int quant, int wl,
		int dsu_fine_ctrl_enabled, int dsu_fine_ctrl_tcm, int dsu_fine_pct, int *dsu_target_freq);
EXPORT_SYMBOL(mtk_dsu_freq_agg_hook);
int dsu_freq_agg(int cpu, int max_freq_in_gear, int quant, int wl,
		int dsu_fine_ctrl_enabled, int dsu_fine_ctrl_tcm, int dsu_fine_pct, int *dsu_target_freq)
{
	int dsu_freq;

	if (mtk_dsu_freq_agg_hook){
		return mtk_dsu_freq_agg_hook(cpu, max_freq_in_gear, quant, wl, dsu_fine_ctrl_enabled,
				dsu_fine_ctrl_tcm, dsu_fine_pct, dsu_target_freq);
	}
	// if mtk_dsu_freq_agg_hook not ready yet
	dsu_freq = max_freq_in_gear > 1;
	if (*dsu_target_freq < dsu_freq)
		*dsu_target_freq = dsu_freq;
	return dsu_freq;
}
EXPORT_SYMBOL_GPL(dsu_freq_agg);

void set_dsu_target_freq(struct cpufreq_policy *policy)
{
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
	int i, cpu, dsu_target_freq = 0, max_freq_in_gear, cpu_idx;
	unsigned int wl = get_wl_dsu();
	int dsu_fine_ctrl_enabled = get_dsu_fine_ctrl_enable();
	bool dsu_fine_ctrl_tcm = get_dsu_fine_ctrl();
	unsigned int dsu_fine_pct = 0;
	struct cpufreq_mtk *c = policy->driver_data;
	unsigned int freq_thermal = 0;
	struct sugov_rq_data *sugov_data_ptr;
	bool dsu_idle_ctrl = is_dsu_idle_enable();
	unsigned int cpu_freq_with_thermal = 0;

	for_each_cpu(cpu_idx, policy->related_cpus)
		freq_state.cpu_freq[cpu_idx] = policy->cached_target_freq;

	for (i = 0; i < pd_count; i++) {
		if(!cpumask_intersects(&pd_cpumask[i], cpu_active_mask)) {
			freq_state.dsu_freq_vote[i] = 0;
			continue;
		}

		cpu = cpumask_first(&pd_cpumask[i]);
		max_freq_in_gear = 0;
		if (dsu_fine_ctrl_enabled!=0 && dsu_fine_ctrl_tcm!=0)
			dsu_fine_pct = get_fine_value_pct_gear(i);
		for_each_cpu(cpu_idx, &pd_cpumask[i]) {
			if(dsu_idle_ctrl) {
				if (freq_state.cpu_freq[cpu_idx] > max_freq_in_gear &&
					cpu_active(cpu_idx) &&
					!mtk_available_idle_cpu(cpu_idx))
					max_freq_in_gear = freq_state.cpu_freq[cpu_idx];
			} else {
				if (cpumask_weight(&pd_cpumask[i]) == 1 && mtk_available_idle_cpu(cpu)) {
					sugov_data_ptr = &per_cpu(rq_data, cpu)->sugov_data;
					if (READ_ONCE(sugov_data_ptr->enq_ing) == 0) {
						freq_state.dsu_freq_vote[i] = 0;
						WRITE_ONCE(sugov_data_ptr->enq_update_dsu_freq, true);
						goto skip_single_idle_cpu;
					}
				}
				if (freq_state.cpu_freq[cpu_idx] > max_freq_in_gear &&
					cpu_active(cpu_idx))
					max_freq_in_gear = freq_state.cpu_freq[cpu_idx];
			}
		}
		cpu_freq_with_thermal = max_freq_in_gear;

#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
		freq_thermal = get_cpu_ceiling_freq(i);
		if(max_freq_in_gear > freq_thermal)
			cpu_freq_with_thermal = freq_thermal;
#endif
		freq_state.dsu_freq_vote[i] = dsu_freq_agg(cpu, cpu_freq_with_thermal, false, wl,
			dsu_fine_ctrl_enabled, dsu_fine_ctrl_tcm, dsu_fine_pct, &dsu_target_freq);

skip_single_idle_cpu:
		if (trace_sugov_ext_dsu_freq_vote_enabled())
			trace_sugov_ext_dsu_freq_vote(wl, i, dsu_idle_ctrl,
				max_freq_in_gear, freq_state.dsu_freq_vote[i] ,freq_thermal,
				dsu_fine_ctrl_enabled, dsu_fine_ctrl_tcm, dsu_fine_pct);
	}

	freq_state.dsu_target_freq = dsu_target_freq;
	c->sb_ch = dsu_target_freq;
	return;
#endif
}


/* Check wheather uclamp will influence OPP to determind uclamp set.
 * uclamp_min/max: uclamp_min/max after multiply by margin
 */
unsigned long sys_min_cap = UINT_MAX, sys_second_max_cap = 0;
int mtk_uclamp_involve(unsigned long uclamp_min, unsigned long uclamp_max)
{
	if ((uclamp_min > sys_min_cap) || (uclamp_max <= sys_second_max_cap))
		return true;

	return false;
}
EXPORT_SYMBOL_GPL(mtk_uclamp_involve);

int init_uclamp_involve_done;
void init_uclamp_involve(void)
{
	unsigned int gear_idx = 0;

	for (; gear_idx < pd_count; gear_idx++) {
		unsigned int cpu = cpumask_first(&pd_cpumask[gear_idx]);
		unsigned int min_opp = pd_util2opp(cpu, 0, 0, 0, NULL, true, DPT_CALL_INIT_UCLAMP_INVOLVE);
		unsigned long min_cap = pd_opp2cap(cpu, min_opp, false, 0, NULL, true,
			DPT_CALL_INIT_UCLAMP_INVOLVE);
		unsigned long second_max_cap = pd_opp2cap(cpu, 1, false, 0, NULL, true, DPT_CALL_INIT_UCLAMP_INVOLVE);

		if (min_cap < sys_min_cap)
			sys_min_cap = min_cap;

		if (second_max_cap > sys_second_max_cap)
			sys_second_max_cap = second_max_cap;
	}

	/* pr_info("%s, sys_min_cap=%lu sys_second_max_cap=%lu\n",
		__func__, sys_min_cap, sys_second_max_cap); */

	init_uclamp_involve_done = 1;
}

int wl_cpu_delay_ch_cnt = 1; // change counter
int wl_dsu_delay_ch_cnt = 1; // change counter
static int nr_wl = 1;
static int wl_cpu_curr;
static int wl_dsu_curr;
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
static int wl_cpu_delay;
static int wl_cpu_delay_cnt;
static int last_wl_cpu;

static int wl_dsu_delay;
static int wl_dsu_delay_cnt;
static int last_wl_dsu;
static unsigned long last_jiffies;
static DEFINE_SPINLOCK(update_wl_tbl_lock);
#endif // CONFIG_MTK_GEARLESS_SUPPORT

int get_nr_wl(void)
{
	return nr_wl;
}
EXPORT_SYMBOL_GPL(get_nr_wl);

#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
int get_nr_cpu_type(void)
{
	return mtk_mapping.nr_cpu_type;
}
EXPORT_SYMBOL_GPL(get_nr_cpu_type);

int get_cpu_type(int type)
{
	if (type < mtk_mapping.total_type)
		return mtk_mapping.cpu_to_dsu[type].cpu_type;
	else
		return -1;
}
EXPORT_SYMBOL_GPL(get_cpu_type);
#endif // CONFIG_MTK_GEARLESS_SUPPORT

#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
void set_wl_manual(int val)
{
	if (wl_valid(val) && is_wl_support()) {
		wl_cpu_manual = val;
		wl_dsu_manual = val;
	} else {
		wl_cpu_manual = -1;
		wl_dsu_manual = -1;
	}
}
EXPORT_SYMBOL_GPL(set_wl_manual);

void set_wl_cpu_manual(int val)
{
	if (wl_valid(val) && is_wl_support())
		wl_cpu_manual = val;
	else
		wl_cpu_manual = -1;
}
EXPORT_SYMBOL_GPL(set_wl_cpu_manual);

void set_wl_dsu_manual(int val)
{
	if (wl_valid(val) && is_wl_support())
		wl_dsu_manual = val;
	else
		wl_dsu_manual = -1;
}
EXPORT_SYMBOL_GPL(set_wl_dsu_manual);
#endif // CONFIG_MTK_GEARLESS_SUPPORT

int get_wl_manual(void)
{
	return wl_cpu_manual;
}
EXPORT_SYMBOL_GPL(get_wl_manual);

int get_wl_dsu_manual(void)
{
	return wl_dsu_manual;
}
EXPORT_SYMBOL_GPL(get_wl_dsu_manual);

#define CAP_UPDATED_BY_WL 0
#define CAP_UPDATED_BY_DPT 1

/************************* scheduler common ************************/

DEFINE_PER_CPU(unsigned long, max_freq_scale) = SCHED_CAPACITY_SCALE;
EXPORT_PER_CPU_SYMBOL(max_freq_scale);

DEFINE_PER_CPU(unsigned long, min_freq_scale) = 0;
EXPORT_PER_CPU_SYMBOL(min_freq_scale);

DEFINE_PER_CPU(unsigned long, min_freq) = 0;
EXPORT_PER_CPU_SYMBOL(min_freq);

/************************* scheduler common ************************/

/* cloned from k69 core.c */
int mtk_idle_cpu(int cpu)
{
	struct rq *rq = cpu_rq(cpu);

	if (rq->curr != rq->idle)
		return 0;

	if (rq->nr_running)
		return 0;

#ifdef CONFIG_SMP
	if (rq->ttwu_pending)
		return 0;
#endif

	return 1;
}

/* cloned from k69 core.c */
int mtk_available_idle_cpu(int cpu)
{
	if (!mtk_idle_cpu(cpu))
		return 0;

	if (vcpu_is_preempted(cpu))
		return 0;

	return 1;
}
EXPORT_SYMBOL_GPL(mtk_available_idle_cpu);

/* cloned from k66 scale_rt_capacity() */
static unsigned long mtk_scale_rt_capacity(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long max = mtk_get_actual_cpu_capacity(cpu);
	unsigned long used, free;
	unsigned long irq;

	irq = cpu_util_irq(rq);

	if (unlikely(irq >= max))
		return 1;

	used = cpu_util_rt(rq);
	used += cpu_util_dl(rq);

	if (unlikely(used >= max))
		return 1;

	free = max - used;

	return scale_irq_capacity(free, irq, max);
}

/* modified from k66 update_cpu_capacity() */
void mtk_update_cpu_capacity(int cpu, unsigned long cap_orig, int wl, int caller)
{
	unsigned long capacity = mtk_scale_rt_capacity(cpu);

	WRITE_ONCE(per_cpu(cpu_scale, cpu), cap_orig);

	if (!capacity)
		capacity = 1;
	
	cpu_rq(cpu)->cpu_capacity = capacity;
}

inline unsigned long get_thermal_freq_ceiling_ratio(int cpu)
{
	return per_cpu(thermal_freq_ceiling_ratio, cpu);
}
EXPORT_SYMBOL(get_thermal_freq_ceiling_ratio);

inline unsigned long get_freq_ceiling_ratio(int cpu)
{
	return per_cpu(freq_ceiling_ratio, cpu);
}
EXPORT_SYMBOL(get_freq_ceiling_ratio);

inline unsigned long dpt_v2_local_capacity_all_util_of(int cpu)
{
	return per_cpu(dpt_v2_cpu_capacity_all_util_local, cpu);
}
EXPORT_SYMBOL(dpt_v2_local_capacity_all_util_of);

unsigned long dpt_v2_scaling_irq(int cpu, unsigned long irq_util)
{
	return irq_util;
}

unsigned long dpt_v2_scailing_dl(int cpu, unsigned long dl_util)
{
	return dl_util;
}

static unsigned long mtk_scale_rt_capacity_dpt_v2(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long max = DPT_V2_MAX_RUNNING_TIME;
	unsigned long used, free;
	unsigned long irq;

	irq = dpt_v2_scaling_irq(cpu, cpu_util_irq(rq));

	if (unlikely(irq >= max))
		return 1;

	used = cpu_util_rt_dpt_v2(rq, TOTAL_UTIL);
	used += dpt_v2_scailing_dl(cpu, READ_ONCE(rq->avg_dl.util_avg));
	// used += thermal_load_avg(rq);

	if (unlikely(used >= max))
		return 1;

	free = max - used;

	return scale_irq_capacity(free, irq, max);
}

int pd_util2freq_r_o(unsigned int cpu, int util, bool quant, int wl);
void update_dpt_v2_cpu_capacity(int cpu, unsigned long *freq_for_debug)
{
	unsigned long gear_umax_freq = get_cpu_gear_uclamp_max_capacity(cpu, GU_RET_FREQ), __thermal_freq = per_cpu(thermal_freq, cpu);
	unsigned long min_f = per_cpu(min_freq, cpu), max_f = per_cpu(max_freq, cpu);
	unsigned long dpt_v2_capacity_local = mtk_scale_rt_capacity_dpt_v2(cpu);
	unsigned long __freq_ceiling, restricted_perf_ratio;

	__freq_ceiling = min(gear_umax_freq, max_f);
	__freq_ceiling = min(__freq_ceiling, __thermal_freq);
	per_cpu(freq_ceiling, cpu) = __freq_ceiling;
	per_cpu(freq_floor, cpu) = min_f;

	per_cpu(thermal_freq_ceiling_ratio, cpu) = (__thermal_freq << FREQ_CEILING_RATIO_BIT) / get_cpu_max_freq(cpu);

	restricted_perf_ratio = (__freq_ceiling << FREQ_CEILING_RATIO_BIT) / get_cpu_max_freq(cpu);
	per_cpu(freq_ceiling_ratio, cpu) = restricted_perf_ratio;

	per_cpu(dpt_v2_cpu_capacity_all_util_local, cpu) = min(dpt_v2_capacity_local, DPT_V2_MAX_RUNNING_TIME * restricted_perf_ratio >> FREQ_CEILING_RATIO_BIT);

	freq_for_debug[0] = __freq_ceiling;
	freq_for_debug[1] = gear_umax_freq;
	freq_for_debug[2] = max_f;
	freq_for_debug[3] = __thermal_freq;
	freq_for_debug[4] = min_f;
	freq_for_debug[5] = per_cpu(dpt_v2_cpu_capacity_all_util_local, cpu);
}

/* hooked from k66 update_cpu_capacity() */
void hook_update_cpu_capacity(void *data, int cpu, unsigned long *capacity)
{
	unsigned long cap_ceiling, capacity_orig = arch_scale_cpu_capacity(cpu), orig_cap_of = *capacity;
	unsigned long gear_umax = get_cpu_gear_uclamp_max_capacity(cpu, GU_RET_UTIL);
	unsigned long min_f_scale = READ_ONCE(per_cpu(min_freq_scale, cpu)), max_f_scale = READ_ONCE(per_cpu(max_freq_scale, cpu));
	unsigned long __thermal_pressure = READ_ONCE(per_cpu(hw_pressure, cpu));
	unsigned long freq_for_debug[6] = {[0 ... 5] = 0};

	cap_ceiling = min_t(unsigned long, *capacity, gear_umax);
	*capacity = clamp_t(unsigned long, cap_ceiling,
		min_f_scale, max_f_scale);
	*capacity = min_t(unsigned long, *capacity, capacity_orig - __thermal_pressure);

	if (is_dpt_v2_support())
		update_dpt_v2_cpu_capacity(cpu, freq_for_debug);

	if (trace_sched_update_cpu_capacity_enabled()) {
		trace_sched_update_cpu_capacity(cpu, cpu_rq(cpu), orig_cap_of, gear_umax, min_f_scale, max_f_scale, __thermal_pressure,
			freq_for_debug);
	}
}

/*kernel mainline get_acutual_cpu_capacity()*/
unsigned long mtk_get_actual_cpu_capacity(int cpu)
{
	unsigned long capacity= arch_scale_cpu_capacity(cpu);
	hook_update_cpu_capacity(NULL, cpu, &capacity);

	return capacity;
}
EXPORT_SYMBOL_GPL(mtk_get_actual_cpu_capacity);

#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
#define WL_CPU -1
#define WL_DSU -2
void update_wl_cpu_dsu_separately(int wl_tcm, int type, int is_manual, int *wl_curr, int wl_manual,
	int *last_wl, int *wl_delay, int *wl_delay_cnt, int *wl_delay_ch_cnt)
{
	int need_update_cpu_capacity;

	if (is_manual)
		*wl_curr = wl_manual;
	else if (wl_valid(wl_tcm))
		*wl_curr = wl_tcm;

	need_update_cpu_capacity = (type == WL_CPU && *last_wl != *wl_curr && wl_valid(*wl_curr));
	if (need_update_cpu_capacity) {
		int gear_idx, cpu;

		for (gear_idx = 0; gear_idx < pd_count; gear_idx++) {
			for_each_cpu(cpu, &pd_cpumask[gear_idx])
				mtk_update_cpu_capacity(cpu, pd_opp2cap(cpu, 0, true, *wl_curr, NULL, true,
						DPT_CALL_UPDATE_WL_TBL), *wl_curr, CAP_UPDATED_BY_WL);
		}
	}

	*last_wl = *wl_curr;
	if (*wl_delay != *wl_curr) {
		(*wl_delay_cnt)++;
		if (*wl_delay_cnt > wl_delay_update_tick) {
			*wl_delay_cnt = 0;
			*wl_delay = *wl_curr;
			(*wl_delay_ch_cnt)++;
		}
	} else
		*wl_delay_cnt = 0;
}

#define wl_cpu_is_manual() (wl_cpu_manual != -1)
#define wl_dsu_is_manual() (wl_dsu_manual != -1)
void update_wl_tbl(unsigned int cpu, bool *is_cpu_to_update_thermal)
{
	int wl_tcm = 0;

	if (spin_trylock(&update_wl_tbl_lock)) {
		unsigned long tmp_jiffies = jiffies;

		if (last_jiffies !=  tmp_jiffies) {
			last_jiffies =  tmp_jiffies;
			spin_unlock(&update_wl_tbl_lock);
			wl_tcm = get_wl(0);

			update_wl_cpu_dsu_separately(wl_tcm, WL_CPU, wl_cpu_is_manual(), &wl_cpu_curr, wl_cpu_manual,
				&last_wl_cpu, &wl_cpu_delay, &wl_cpu_delay_cnt, &wl_cpu_delay_ch_cnt);

			update_wl_cpu_dsu_separately(wl_tcm, WL_DSU, wl_dsu_is_manual(), &wl_dsu_curr, wl_dsu_manual,
				&last_wl_dsu, &wl_dsu_delay, &wl_dsu_delay_cnt, &wl_dsu_delay_ch_cnt);

			if (trace_sugov_ext_wl_enabled()) {
				trace_sugov_ext_wl(topology_cluster_id(cpu), cpu, wl_tcm, wl_cpu_curr, wl_cpu_delay,
					wl_cpu_manual, wl_dsu_curr, wl_dsu_delay, wl_dsu_manual);
			}

			*is_cpu_to_update_thermal = true;

			if (is_dpt_v2_support())
				update_dpt_v2_info();
			else
				update_curr_collab_state(is_cpu_to_update_thermal);
		} else
			spin_unlock(&update_wl_tbl_lock);
	}
}
EXPORT_SYMBOL_GPL(update_wl_tbl);
#endif

int get_curr_wl(void)
{
	return clamp_val(wl_cpu_curr, 0, nr_wl - 1);
}
EXPORT_SYMBOL_GPL(get_curr_wl);

int get_curr_wl_dsu(void)
{
	return clamp_val(wl_dsu_curr, 0, nr_wl - 1);
}
EXPORT_SYMBOL_GPL(get_curr_wl_dsu);

int get_classify_wl(void)
{
	return get_wl(0);
}
EXPORT_SYMBOL_GPL(get_classify_wl);

int get_em_wl(void)
{
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
	return clamp_val(wl_cpu_delay, 0, nr_wl - 1);
#endif
	return -1;
}
EXPORT_SYMBOL_GPL(get_em_wl);

int get_wl_dsu(void)
{
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
	return clamp_val(wl_dsu_delay, 0, nr_wl - 1);
#endif
	return -1;
}
EXPORT_SYMBOL_GPL(get_wl_dsu);

/* DPT */
void (*init_collab_driver_hook) (int *nr_collab_type);
EXPORT_SYMBOL(init_collab_driver_hook);
void (*init_wl_collab_mask_driver_hook) (int **wl_collab_type_mapping_mask);
EXPORT_SYMBOL(init_wl_collab_mask_driver_hook);
void (*set_curr_collab_state_driver_hook) (struct curr_collab_state_struct *__curr_collab_state);
EXPORT_SYMBOL(set_curr_collab_state_driver_hook);
void (*set_val_m_driver_hook) (int *val_m);
EXPORT_SYMBOL(set_val_m_driver_hook);
void (*set_collab_state_manual_driver_hook) (int type, int state);
EXPORT_SYMBOL(set_collab_state_manual_driver_hook);
int (*is_dpt_support_driver_hook) (void);
EXPORT_SYMBOL(is_dpt_support_driver_hook);
void (*set_v_driver_hook)(int v);
EXPORT_SYMBOL(set_v_driver_hook);

bool __dpt_init_done;
void init_curr_collab_struct(void)
{
	int collab_type = 0;

	init_collab_driver_hook(&nr_collab_type);
	if (nr_collab_type == -1) {
		pr_info("DPT hook fail, nr_collab_state is -1\n");
		return;
	}

	curr_collab_state = kcalloc(nr_collab_type,
		sizeof(struct curr_collab_state_struct), GFP_ATOMIC | __GFP_NOFAIL);
	curr_collab_state_manual = kcalloc(nr_collab_type,
		sizeof(struct curr_collab_state_struct), GFP_ATOMIC | __GFP_NOFAIL);

	for_each_collab_type(collab_type) {
		curr_collab_state[collab_type].state = 0;
		curr_collab_state_manual[collab_type] = -1;
	}
	curr_collab_state[0].ret_function = &collab_type_0_ret_function;

	set_curr_collab_state_driver_hook(curr_collab_state);

	set_val_m_driver_hook(&val_m);

	wl_collab_type_mapping_mask = kcalloc(nr_collab_type, sizeof(int), GFP_ATOMIC | __GFP_NOFAIL);
	init_wl_collab_mask_driver_hook(&wl_collab_type_mapping_mask);
	__dpt_init_done = true;
	change_dpt_support_driver_hook(dpt_default_status);
}

int sys_max_cap_cluster;
int get_sys_max_cap_cluster(void)
{
	return sys_max_cap_cluster;
}
EXPORT_SYMBOL_GPL(get_sys_max_cap_cluster);

void init_sys_max_cap_cpu(void)
{
	unsigned int cpu, sys_max_cap = 0;

	for_each_possible_cpu(cpu)
		if (arch_scale_cpu_capacity(cpu) > sys_max_cap) {
			sys_max_cap = arch_scale_cpu_capacity(cpu);
			sys_max_cap_cluster = topology_cluster_id(cpu);
	}
}

unsigned long *cpu_max_freq;
unsigned long *cpu_min_freq;
inline unsigned long get_cpu_max_freq(int cpu)
{
	if (cpu_max_freq)
		return cpu_max_freq[cpu];

	return 1;
}
EXPORT_SYMBOL(get_cpu_max_freq);

inline unsigned long get_cpu_min_freq(int cpu)
{
	if (cpu_min_freq)
		return cpu_min_freq[cpu];

	return 1;
}
EXPORT_SYMBOL(get_cpu_min_freq);

void init_freq_ceiling_floor(void)
{
	unsigned long __max_freq, min_freq;
	int cpu;

	cpu_max_freq = kcalloc(MAX_NR_CPUS, sizeof(unsigned long), GFP_KERNEL);
	cpu_min_freq = kcalloc(MAX_NR_CPUS, sizeof(unsigned long), GFP_KERNEL);
	for_each_possible_cpu(cpu) {
		min_freq = pd_util2freq(cpu, 0, false, 0);
		__max_freq = pd_util2freq(cpu, SCHED_CAPACITY_SCALE, false, 0);
		WRITE_ONCE(per_cpu(max_freq, cpu), __max_freq);
		WRITE_ONCE(per_cpu(freq_ceiling, cpu), __max_freq);
		WRITE_ONCE(per_cpu(freq_floor, cpu), min_freq);

		cpu_max_freq[cpu] = __max_freq;
		cpu_min_freq[cpu] = min_freq;
	}
}

#define is_bit_set(value, bit) (((value) & (1 << (bit))) != 0)
#define USING_LAST_STATE 	-1
#define SCALING_FACTOR_MAX	(64LL << SCHED_CAPACITY_SHIFT)	/* TEMP */
#define SCALING_FACTOR_MIN	0								/* TEMP */
void update_curr_collab_state(bool *is_cpu_to_update_thermal)
{
	int collab_type = 0, curr_state = 0;
	int cpu = 0;
	unsigned long cap, sys_max_cap = 0, __sys_max_cap_cluster;
	bool need_update_capacity_orig = false;
	int wl = get_curr_wl(); /* should we use wl_curr? */

	if (spin_trylock(&update_dpt_lock)) {
		unsigned long tmp_jiffies_dpt = jiffies;

		if (last_jiffies_dpt != tmp_jiffies_dpt) {
			last_jiffies_dpt = tmp_jiffies_dpt;

			if (!__dpt_init_done) {
				if (init_collab_driver_hook == NULL) {
					spin_unlock(&update_dpt_lock);
					return;
				}

				if (nr_collab_type == -1)
					init_curr_collab_struct();
			}

			spin_unlock(&update_dpt_lock);


			for_each_collab_type(collab_type) {
				curr_state = curr_collab_state[collab_type].ret_function(DPT_V1);
				curr_collab_state[collab_type].state = curr_state;

				if (!need_update_capacity_orig)
					need_update_capacity_orig = is_bit_set(
						wl_collab_type_mapping_mask[collab_type],wl);
			}

			/* update capacity_orig_of */
			for_each_possible_cpu(cpu) {
				if (need_update_capacity_orig)
					cap = pd_opp2cap(cpu, 0, false, wl,
						NULL, false, DPT_CALL_UPDATE_CURR_COLLAB_STATE);
				else
					cap = pd_opp2cap(cpu, 0, false, wl,
						NULL, true, DPT_CALL_UPDATE_CURR_COLLAB_STATE);

				mtk_update_cpu_capacity(cpu, cap, wl, CAP_UPDATED_BY_DPT);

				if (arch_scale_cpu_capacity(cpu) > sys_max_cap) {
					sys_max_cap = arch_scale_cpu_capacity(cpu);
					__sys_max_cap_cluster = topology_cluster_id(cpu);
				}
			}
			*is_cpu_to_update_thermal = true;
			sys_max_cap_cluster = __sys_max_cap_cluster;
		} else
			spin_unlock(&update_dpt_lock);
	}
}
EXPORT_SYMBOL_GPL(update_curr_collab_state);
/* End of DPT */

void init_sbb_cpu_data(void)
{
	int cpu;
	struct sbb_cpu_data *data;

	for_each_possible_cpu(cpu) {
		data = kcalloc(1, sizeof(struct sbb_cpu_data), GFP_KERNEL);
		per_cpu(sbb, cpu) = data;
	}
}
EXPORT_SYMBOL_GPL(init_sbb_cpu_data);

void set_system_sbb(bool set)
{
	busy_tick_boost_all = set;
}
EXPORT_SYMBOL_GPL(set_system_sbb);

bool get_system_sbb(void)
{
	return busy_tick_boost_all;
}
EXPORT_SYMBOL_GPL(get_system_sbb);

void set_sbb(int flag, int pid, bool set)
{
	struct task_struct *p, *group_leader;
	int success = 0;

	switch (flag) {
	case SBB_ALL:
		set_system_sbb(set);
		success = 1;
		break;
	case SBB_GROUP:
		rcu_read_lock();
		p = find_task_by_vpid(pid);
		if (p && p->exit_state == 0) {
			get_task_struct(p);
			group_leader = p->group_leader;
			if (group_leader && group_leader->exit_state == 0) {
				struct sbb_task_struct *sts;

				get_task_struct(group_leader);
				sts = &((struct mtk_task *)
					android_task_vendor_data(group_leader))->sbb_task;
				sts->set_group = set;
				put_task_struct(group_leader);
				success = 1;
			}
			put_task_struct(p);
		}
		rcu_read_unlock();
		break;
	case SBB_TASK:
		rcu_read_lock();
		p = find_task_by_vpid(pid);
		if (p) {
			struct sbb_task_struct *sts;

			get_task_struct(p);
			sts = &((struct mtk_task *)android_task_vendor_data(p))->sbb_task;
			sts->set_task = set;
			put_task_struct(p);
			success = 1;
		}
		rcu_read_unlock();
	}
	if (trace_sugov_ext_act_sbb_enabled())
		trace_sugov_ext_act_sbb(flag, pid, set, success, -1, -1);
}
EXPORT_SYMBOL_GPL(set_sbb);

bool is_sbb_trigger(struct rq *rq)
{
	bool sbb_trigger = false;
	struct task_struct *curr, *group_leader;

	rcu_read_lock();
	curr = rcu_dereference(rq->curr);
	if (curr && curr->exit_state == 0) {
		struct sbb_task_struct *sts;

		sts = &((struct mtk_task *)android_task_vendor_data(curr))->sbb_task;
		sbb_trigger |= sts->set_task;
		group_leader = curr->group_leader;
		if (group_leader && group_leader->exit_state == 0) {
			get_task_struct(group_leader);
			sts = &((struct mtk_task *)android_task_vendor_data(group_leader))->sbb_task;
			sbb_trigger |= sts->set_group;
			put_task_struct(group_leader);
		}
	}
	rcu_read_unlock();
	sbb_trigger |= busy_tick_boost_all;

	return sbb_trigger;
}
EXPORT_SYMBOL_GPL(is_sbb_trigger);

void set_sbb_active_ratio(int val)
{
	int i;

	for (i = 0; i < pd_count; i++)
		set_sbb_active_ratio_gear(i, val);
}
EXPORT_SYMBOL_GPL(set_sbb_active_ratio);

void set_sbb_active_ratio_gear(int gear_id, int val)
{
	sbb_active_ratio[gear_id] = val;
	if (trace_sugov_ext_act_sbb_enabled())
		trace_sugov_ext_act_sbb(-1, -1, -1, -1, gear_id, val);
}
EXPORT_SYMBOL_GPL(set_sbb_active_ratio_gear);

int get_sbb_active_ratio_gear(int gear_id)
{
	return sbb_active_ratio[gear_id];
}
EXPORT_SYMBOL_GPL(get_sbb_active_ratio_gear);

bool is_gearless_support(void)
{
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
	return !freq_scaling_disabled;
#else
	return false;
#endif
}
EXPORT_SYMBOL_GPL(is_gearless_support);

unsigned int get_nr_gears(void)
{
	return pd_count;
}
EXPORT_SYMBOL_GPL(get_nr_gears);

inline struct cpumask *get_gear_cpumask(unsigned int gear)
{
	return &pd_cpumask[gear];
}
EXPORT_SYMBOL_GPL(get_gear_cpumask);

inline int get_eas_wl(int wl)
{
	if (wl < 0 || wl >= nr_wl)
		wl = wl_cpu_curr;
	return wl;
}

inline int get_eas_wl_dsu(int wl)
{
	if (wl < 0 || wl >= nr_wl)
		wl = wl_dsu_curr;
	return wl;
}

static void free_capacity_table(void)
{
	int i;

	if (!pd_capacity_tbl)
		return;

	for (i = 0; i < legacy_pd_count; i++) {
		kfree(pd_capacity_tbl[i].caps);
		kfree(pd_capacity_tbl[i].util_opp);
		kfree(pd_capacity_tbl[i].util_freq);
	}
	kfree(pd_capacity_tbl);
	pd_capacity_tbl = NULL;
	legacy_pd_enable = 0;
}

static int alloc_capacity_table(void)
{
	int i;
	int ret = 0;
	int cur_tbl = 0;

	pd_capacity_tbl = kcalloc(MAX_PD_COUNT, sizeof(struct pd_capacity_info),
			GFP_KERNEL);
	if (!pd_capacity_tbl)
		return -ENOMEM;

	for (i = 0; i < nr_cpu_ids; i++) {
		int nr_caps;
		struct em_perf_domain *pd;

		pd = em_cpu_get(i);
		if (!pd) {
			pr_info("em_cpu_get return NULL for cpu#%d", i);
			continue;
		}
		if (i != cpumask_first(to_cpumask(pd->cpus)))
			continue;

		WARN_ON(cur_tbl >= MAX_PD_COUNT);

		nr_caps = pd->nr_perf_states;
		pd_capacity_tbl[cur_tbl].nr_caps = nr_caps;
		cpumask_copy(&pd_capacity_tbl[cur_tbl].cpus, to_cpumask(pd->cpus));
		pd_capacity_tbl[cur_tbl].caps = kcalloc(nr_caps, sizeof(unsigned long),
							GFP_KERNEL);
		if (!pd_capacity_tbl[cur_tbl].caps)
			goto nomem;

		pd_capacity_tbl[cur_tbl].util_opp = NULL;
		pd_capacity_tbl[cur_tbl].util_freq = NULL;

		entry_count += nr_caps;

		cur_tbl++;
	}

	legacy_pd_count = cur_tbl;

	return 0;

nomem:
	ret = -ENOMEM;
	free_capacity_table();

	return ret;
}

static unsigned long get_freq_based_cost(int j, unsigned long cost, struct em_perf_domain *pd,
					unsigned long power_res, struct pd_capacity_info *pd_info)
{
	unsigned long new_cost = cost;
	struct device_node *dn = NULL;
	int use_freq_cost = 0;
	u64 fmax = (u64) pd->em_table->state[pd_info->nr_caps - 1].frequency;
	int ret=0;

	dn = of_find_node_by_name(NULL, "eas-info");
	if (dn) {
		ret = of_property_read_u32(dn, "freq-based-cost", &use_freq_cost);
		if (ret)
			pr_info("%s freq-based-cost node not found", __func__);
	} else
		pr_info("%s eas-info node not found", __func__);

	if (use_freq_cost == 1) {
		new_cost = div64_u64(fmax * power_res,
				pd->em_table->state[pd_info->nr_caps - j - 1].frequency);
	} else if (use_freq_cost == 2) {
		new_cost = power_res / 10;
	}

	if (use_freq_cost != 0) {
		pr_info("%s freq-based-cost = %d cost=%ld old_cost=%ld fmax=%d freq=%d power_res=%ld",
			__func__, use_freq_cost, (long) new_cost, (long) cost, (int) fmax,
			(int) pd->em_table->state[pd_info->nr_caps - j - 1].frequency, (long)power_res);
	}

	return new_cost;
}


int init_legacy_capacity_table(void)
{
	int ret;
	int i, j, cpu;
	int count = 0;
	unsigned long cap;
	long next_cap, k;
	struct pd_capacity_info *pd_info;
	struct em_perf_domain *pd;
	struct upower_tbl *tbl = NULL;
	struct eas_info eas_node;
	void __iomem *sram_base_addr = NULL;
	void __iomem *base = NULL;
	unsigned long offset = 0;
	unsigned long end_cap;
	unsigned long power_res, cost;

	parse_eas_data(&eas_node);
	if (eas_node.available) {
		sram_base_addr = ioremap(eas_node.csram_base + eas_node.offs_cap, CAPACITY_TBL_SIZE);
		if (!sram_base_addr) {
			pr_info("Remap thermal info failed in eas-info node\n");
			return -EIO;
		}
		base = sram_base_addr;
	}

	ret = alloc_capacity_table();
	if (ret) {
		pr_info("%s alloc failed\n", __func__);
		return ret;
	}
	for (i = 0; i < legacy_pd_count; i++) {
		pd_info = &pd_capacity_tbl[i];
		cpu = cpumask_first(&pd_info->cpus);
		pd = em_cpu_get(cpu);
		if (!pd) {
			pr_info("%s em_cpu_get failed\n", __func__);
			goto err;
		}
#if IS_ENABLED(CONFIG_MTK_UNIFIED_POWER)
		tbl = upower_get_core_tbl(cpu);
#endif
		if (!eas_node.available) {
			if (!tbl) {
				pr_info("%s EAS_NODE not found tbl=NULL\n", __func__);
				goto err;
			}
		}
		pr_info("%s i=%d pd_info->nr_caps=%d\n",__func__ , i, (int) pd_info->nr_caps);
		for (j = 0; j < pd_info->nr_caps; j++) {
			if (eas_node.available) {
				cap = ioread16(base + offset);
				next_cap = ioread16(base + offset + CAPACITY_ENTRY_SIZE);
			} else {
				cap = tbl->row[pd_info->nr_caps - j - 1].cap;
				if (j == pd_info->nr_caps - 1)
					next_cap = -1;
				else
					next_cap = tbl->row[pd_info->nr_caps - j - 2].cap;
			}
			if (cap == 0 || next_cap == 0) {
				pr_info("%s capacity = %d next_cap = %d fail\n", __func__, (int) cap, (int) next_cap );
				goto err;
			}
			pr_info("%s j=%d cap = %d next_cap=%d\n",__func__, j, (int) cap, (int) next_cap);
			pd_info->caps[j] = cap;
			pd->em_table->state[pd_info->nr_caps - j - 1].performance = pd_info->caps[j];

			power_res = pd->em_table->state[pd_info->nr_caps - j - 1].power * 10;
			cost = power_res / pd->em_table->state[pd_info->nr_caps - j - 1].performance;

			cost = get_freq_based_cost(j, cost, pd, power_res, pd_info);

			pd->em_table->state[pd_info->nr_caps - j - 1].cost = cost;
			if (!pd_info->util_opp) {
				pd_info->util_opp = kcalloc(cap + 1, sizeof(unsigned int),
										GFP_KERNEL);
				if (!pd_info->util_opp)
					goto nomem;
			}

			if (!pd_info->util_freq) {
				pd_info->util_freq = kcalloc(cap + 1, sizeof(unsigned int),
										GFP_KERNEL);
				if (!pd_info->util_freq)
					goto nomem;
			}

			if (j == pd_info->nr_caps - 1)
				next_cap = -1;

			for (k = cap; k > next_cap; k--) {
				pd_info->util_opp[k] = j;
				pd_info->util_freq[k] =
					pd->em_table->state[pd->nr_perf_states - j - 1].frequency;
			}

			count += 1;
			if (eas_node.available)
				offset += CAPACITY_ENTRY_SIZE;
		}

		/* repeated last cap 0 between each cluster */
		if (eas_node.available) {
			end_cap = ioread16(base + offset);
			if (end_cap != cap) {
				pr_info("%s end_cap=%d cap = %d  FAIL\n",__func__, (int) end_cap, (int) cap);
				goto err;
			}
			offset += CAPACITY_ENTRY_SIZE;
		}

		for_each_cpu(j, &pd_info->cpus) {
			if (per_cpu(cpu_scale, j) != pd_info->caps[0]) {
				pr_info("capacity err: cpu=%d, cpu_scale=%d, pd_info_cap=%d\n",
					j, (int) per_cpu(cpu_scale, j), (int) pd_info->caps[0]);
				per_cpu(cpu_scale, j) = pd_info->caps[0];
			} else {
				pr_info("capacity match: cpu=%d, cpu_scale=%d, pd_info_cap=%d\n",
					j, (int) per_cpu(cpu_scale, j), (int) pd_info->caps[0]);
			}
		}
	}
	if (entry_count != count) {
		pr_info("%s entry_count=%d count = %d  FAIL\n",__func__, (int) entry_count, (int) count);
		goto err;
	}
	legacy_pd_enable = 1;

	return 0;

nomem:
	pr_info("allocate util mapping table failed\n");
err:
	pr_info("count %d does not match entry_count %d\n", count, entry_count);

	free_capacity_table();

	return -ENOENT;
}

static int pd_capacity_tbl_show(struct seq_file *m, void *v)
{
	int i, j;
	struct pd_capacity_info *pd_info;

	if (!pd_capacity_tbl) {
		pr_info ("PD_CAPACITY_TBL not initialized");
		return 1;
	}

	for (i = 0; i < MAX_PD_COUNT; i++) {
		pd_info = &pd_capacity_tbl[i];

		if (!pd_info || !pd_info->nr_caps)
			break;

		seq_printf(m, "Pd table: %d\n", i);
		seq_printf(m, "cpus: %*pbl\n", cpumask_pr_args(&pd_info->cpus));
		seq_printf(m, "nr_caps: %d\n", pd_info->nr_caps);
		for (j = 0; j < pd_info->nr_caps; j++)
			seq_printf(m, "%d: %d, %d\n", j, (int) pd_info->caps[j],
					(int) pd_info->util_freq[pd_info->caps[j]]);
	}

	return 0;
}

static int pd_capacity_tbl_open(struct inode *in, struct file *file)
{
	return single_open(file, pd_capacity_tbl_show, NULL);
}

static const struct proc_ops pd_capacity_tbl_ops = {
	.proc_open = pd_capacity_tbl_open,
	.proc_read = seq_read
};


/* em */
int em_opp2freq(int cpu, int opp)
{
	struct em_perf_domain *pd;

	pd = em_cpu_get(cpu);
	if (!pd) {
		pr_info("error, pd = NULL, %s\n", __func__);
		return 1;
	}
	opp = clamp_val(opp, 0, pd->nr_perf_states - 1);
	if (pd->em_table->state[0].frequency > pd->em_table->state[1].frequency)
		return pd->em_table->state[opp].frequency;
	else
		return pd->em_table->state[pd->nr_perf_states - 1 - opp].frequency;
}

int em_opp2cap(int cpu, int opp)
{
	struct em_perf_domain *pd;
	struct pd_capacity_info *pd_info;
	int i;
	int cap = 0;
	int opp_freq;
	int max_freq;

	if (legacy_pd_enable) {
		for (i = 0; i < legacy_pd_count; i++) {
			pd_info = &pd_capacity_tbl[i];

			if (!cpumask_test_cpu(cpu, &pd_info->cpus))
				continue;

			/* Return max capacity if opp is not valid */
			if (opp < 0 || opp >= pd_info->nr_caps)
				return pd_info->caps[0];

			opp = clamp_val(opp, 0, pd_info->nr_caps - 1);
			return pd_info->caps[opp];
		}
	} else {
		pd = em_cpu_get(cpu);
		if (!pd) {
			pr_info("error, pd = NULL, %s\n", __func__);
			return 1;
		}
		opp = clamp_val(opp, 0, pd->nr_perf_states - 1);
		if (pd->em_table->state[0].frequency > pd->em_table->state[1].frequency) {
			max_freq = pd->em_table->state[0].frequency;
			opp_freq = pd->em_table->state[opp].frequency;
		} else {
			max_freq = pd->em_table->state[pd->nr_perf_states - 1].frequency;
			opp_freq = pd->em_table->state[pd->nr_perf_states - 1 - opp].frequency;
		}
		cap = (opp_freq * per_cpu(cpu_scale, cpu)) / max_freq;
	}
	return cap;
}

int em_opp2pwr_eff(int cpu, int opp)
{
	struct em_perf_domain *pd;

	pd = em_cpu_get(cpu);
	if (!pd) {
		pr_info("error, pd = NULL, %s\n", __func__);
		return 1;
	}
	opp = clamp_val(opp, 0, pd->nr_perf_states - 1);
	if (pd->em_table->state[0].frequency > pd->em_table->state[1].frequency)
		return pd->em_table->state[opp].cost;
	else
		return pd->em_table->state[pd->nr_perf_states - 1 - opp].cost;
}

int em_freq2opp(int cpu, int freq)
{
	struct em_perf_domain *pd;
	int i;

	pd = em_cpu_get(cpu);
	if (!pd) {
		pr_info("error, pd = NULL, %s\n", __func__);
		return 1;
	}
	for (i = pd->nr_perf_states - 1; i >= 0; i--)
		if (freq <= em_opp2freq(cpu, i))
			return i;
	return 0;
}

int em_util2opp(int cpu, int util)
{
	struct em_perf_domain *pd;
	struct pd_capacity_info *pd_info;
	int i;
	if (legacy_pd_enable) {
		for (i = 0; i < legacy_pd_count; i++) {
			pd_info = &pd_capacity_tbl[i];

			if (!cpumask_test_cpu(cpu, &pd_info->cpus))
				continue;

			/* Return max opp if util is not valid */
			if (util < 0 || util >= pd_info->caps[0])
				return 0;

			return pd_info->util_opp[util];
		}
	} else {
		pd = em_cpu_get(cpu);
		if (!pd) {
			pr_info("error, pd = NULL, %s\n", __func__);
			return 1;
		}
		for (i = pd->nr_perf_states - 1; i >= 0; i--) {
			if (util <= em_opp2cap(cpu, i))
				return i;
		}
	}
	return 0;
}
/* em */

/* hook API */
int (*mtk_eas_hook)(void);
EXPORT_SYMBOL(mtk_eas_hook);
int get_eas_hook(void)
{
	if (mtk_eas_hook)
		return mtk_eas_hook();
	return legacy_api_support_get();
}
EXPORT_SYMBOL_GPL(get_eas_hook);

int (*mtk_data_hook)(int *data);
EXPORT_SYMBOL(mtk_data_hook);
int mtk_data_get(int *data)
{
	if (mtk_data_hook)
		return mtk_data_hook(data);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_data_get);

void (*mtk_sched_test_hook)(int *data);
EXPORT_SYMBOL(mtk_sched_test_hook);

int (*mtk_opp2freq_hook)(int cpu, int opp, int quant, int wl);
EXPORT_SYMBOL(mtk_opp2freq_hook);
int pd_opp2freq(int cpu, int opp, int quant, int wl)
{
	if (mtk_opp2freq_hook)
		return mtk_opp2freq_hook(cpu, opp, quant, get_eas_wl(wl));
	else
		return em_opp2freq(cpu, opp);
}
EXPORT_SYMBOL_GPL(pd_opp2freq);

void record_sched_pd_opp2cap(int cpu, int opp, int quant, int wl,
	int val_1, int val_2, int val_r, int *val_s, int r_o, int caller)
{
	if (trace_sched_pd_opp2cap_enabled())  {
		int val_s_0 = val_s == NULL ? (curr_collab_state == NULL ? -1 :
						curr_collab_state[0].state) : val_s[0];
		trace_sched_pd_opp2cap(cpu, opp, quant, get_eas_wl(wl),
				val_1, val_2, val_r, val_s_0, val_m, r_o, caller);
	}
}
EXPORT_SYMBOL_GPL(record_sched_pd_opp2cap);

int (*mtk_opp2cap_hook)(int cpu, int opp, int quant, int wl,
	unsigned long *val_1, unsigned long *val_2, unsigned long *val_r, int *val_s, int val_m, int r_o);
EXPORT_SYMBOL(mtk_opp2cap_hook);
int pd_opp2cap(int cpu, int opp, int quant, int wl, int *val_s, int r_o, int caller)
{
	if (mtk_opp2cap_hook) {
		int result;
		unsigned long val_1, val_2, val_r;

		result = mtk_opp2cap_hook(cpu, opp, quant, get_eas_wl(wl),
			&val_1, &val_2, &val_r, val_s, val_m, r_o);

		record_sched_pd_opp2cap(cpu, opp, quant, wl, val_1, val_2, val_r, val_s, r_o, caller);

		return result;
	} else
		return em_opp2cap(cpu, opp);
}
EXPORT_SYMBOL_GPL(pd_opp2cap);

void record_sched_pd_opp2pwr_eff(int cpu, int opp, int quant, int wl,
	int val_1, int val_2, int val_3, int val_r1, int val_r2, int val_r3, int *val_s,
	int r_o, int caller)
{
	if (trace_sched_pd_opp2pwr_eff_enabled()) {
		int val_s_0 = val_s == NULL ? (curr_collab_state == NULL ? -1 :
						curr_collab_state[0].state) : val_s[0];
		int vals[3] = {val_r1, val_r2, val_r3};

		trace_sched_pd_opp2pwr_eff(cpu, opp, quant, get_eas_wl(wl),
				val_1, val_2, val_3, vals, val_s_0, r_o, caller);
	}
}
EXPORT_SYMBOL_GPL(record_sched_pd_opp2pwr_eff);

int (*mtk_opp2pwr_eff_hook)(int cpu, int opp, int quant, int wl,
	int *val_1, int *val_2, int *val_3, int *val_r1, int *val_r2, int *val_s, int val_m, int r_o);
EXPORT_SYMBOL(mtk_opp2pwr_eff_hook);
int pd_opp2pwr_eff(int cpu, int opp, int quant, int wl, int *val_s, int r_o, int caller)
{
	if (mtk_opp2pwr_eff_hook) {
		int result, val_1, val_2, val_3, val_r1, val_r2, val_r3 = 0;

		result = mtk_opp2pwr_eff_hook(cpu, opp, quant, get_eas_wl(wl),
			&val_1, &val_2, &val_3, &val_r1, &val_r2, val_s, val_m, r_o);

		record_sched_pd_opp2pwr_eff(cpu, opp, quant, wl, val_1, val_2, val_3,
				val_r1, val_r2, val_r3, val_s, r_o, caller);

		return result;
	} else
		return em_opp2pwr_eff(cpu, opp);
}
EXPORT_SYMBOL_GPL(pd_opp2pwr_eff);

int (*mtk_opp2dyn_pwr_hook)(int cpu, int opp, int quant, int wl,
	int *val_1, int *val_r, int *val_s, int val_m, int r_o);
EXPORT_SYMBOL(mtk_opp2dyn_pwr_hook);
int pd_opp2dyn_pwr(int cpu, int opp, int quant, int wl, int *val_s, int r_o, int caller)
{
	if (mtk_opp2dyn_pwr_hook) {
		int result, val_1, val_r;

		result = mtk_opp2dyn_pwr_hook(cpu, opp, quant, get_eas_wl(wl),
			&val_1, &val_r, val_s, val_m, r_o);
		if (trace_sched_pd_opp2dyn_pwr_enabled()) {
			int val_s_0 = val_s == NULL ? (curr_collab_state == NULL ? -1 :
							curr_collab_state[0].state) : val_s[0];
			trace_sched_pd_opp2dyn_pwr(cpu, opp, quant, wl,
				val_1, val_r, val_s_0, val_m, r_o, caller);
		}
		return result;
	} else
		return 1;
}
EXPORT_SYMBOL_GPL(pd_opp2dyn_pwr);

int (*mtk_opp2volt_hook)(int cpu, int opp, int quant, int wl);
EXPORT_SYMBOL(mtk_opp2volt_hook);
int pd_opp2volt(int cpu, int opp, int quant, int wl)
{
	if (mtk_opp2volt_hook)
		return mtk_opp2volt_hook(cpu, opp, quant, get_eas_wl(wl));
	else
		return 1;
}
EXPORT_SYMBOL_GPL(pd_opp2volt);

int (*mtk_cpu_opp2dsu_freq_hook)(int cpu, int opp, int quant, int wl);
EXPORT_SYMBOL(mtk_cpu_opp2dsu_freq_hook);
int pd_cpu_opp2dsu_freq(int cpu, int opp, int quant, int wl)
{
	if (mtk_cpu_opp2dsu_freq_hook)
		return mtk_cpu_opp2dsu_freq_hook(cpu, opp, quant, get_eas_wl_dsu(wl));
	else
		return 1;
}
EXPORT_SYMBOL_GPL(pd_cpu_opp2dsu_freq);

int (*mtk_util2opp_hook)(int cpu, int util, int quant, int wl,
	int *val_1, int *val_2, int *val_3, int *val_4, int *val_r, int *val_s, int val_m, int r_o);
EXPORT_SYMBOL(mtk_util2opp_hook);
int pd_util2opp(int cpu, int util, int quant, int wl, int *val_s, int r_o, int caller)
{
	if (mtk_util2opp_hook) {
		int result, val_1, val_2, val_3, val_4, val_r;

		result = mtk_util2opp_hook(cpu, util, quant, get_eas_wl(wl),
			&val_1, &val_2, &val_3, &val_4, &val_r, val_s, val_m, r_o);
		if(trace_sched_pd_util2opp_enabled() && !r_o) {
			int val_s_0 = val_s == NULL ? (curr_collab_state == NULL ? -1 :
							curr_collab_state[0].state) : val_s[0];
			trace_sched_pd_util2opp(cpu, quant, wl,
				val_1, val_2, val_3, val_4, val_r, val_s_0, val_m, r_o, caller);
		}
		return result;
	} else
		return em_util2opp(cpu, util);
}
EXPORT_SYMBOL_GPL(pd_util2opp);

int (*mtk_freq2opp_hook)(int cpu, int freq, int quant, int wl);
EXPORT_SYMBOL(mtk_freq2opp_hook);
int pd_freq2opp(int cpu, int freq, int quant, int wl)
{
	if (mtk_freq2opp_hook)
		return mtk_freq2opp_hook(cpu, freq, quant, get_eas_wl(wl));
	else
		return em_freq2opp(cpu, freq);
}
EXPORT_SYMBOL_GPL(pd_freq2opp);

int (*mtk_cpu_volt2opp_hook)(int cpu, int volt, int quant, int wl);
EXPORT_SYMBOL(mtk_cpu_volt2opp_hook);
int pd_cpu_volt2opp(int cpu, int volt, int quant, int wl)
{
	if (mtk_cpu_volt2opp_hook)
		return mtk_cpu_volt2opp_hook(cpu, volt, quant, get_eas_wl(wl));
	else
		return 1;
}
EXPORT_SYMBOL_GPL(pd_cpu_volt2opp);

int (*mtk_dsu_weighting_hook)(int wl, int cpu);
EXPORT_SYMBOL(mtk_dsu_weighting_hook);
unsigned int pd_get_dsu_weighting(int wl, unsigned int cpu)
{
	if (mtk_dsu_weighting_hook)
		return mtk_dsu_weighting_hook(get_eas_wl_dsu(wl), cpu);
	else
		return 1;
}
EXPORT_SYMBOL_GPL(pd_get_dsu_weighting);

int (*mtk_emi_weighting_hook)(int wl, int cpu);
EXPORT_SYMBOL(mtk_emi_weighting_hook);
unsigned int pd_get_emi_weighting(int wl, unsigned int cpu)
{
	if (mtk_emi_weighting_hook)
		return mtk_emi_weighting_hook(get_eas_wl(wl), cpu);
	else
		return 1;
}
EXPORT_SYMBOL_GPL(pd_get_emi_weighting);

int (*mtk_dsu_freq2opp_hook)(int freq);
EXPORT_SYMBOL(mtk_dsu_freq2opp_hook);
unsigned int dsu_get_freq_opp(unsigned int freq)
{
	if (mtk_dsu_freq2opp_hook)
		return mtk_dsu_freq2opp_hook(freq);
	return 0;
}
EXPORT_SYMBOL_GPL(dsu_get_freq_opp);

int (*mtk_dsu_volt2opp_hook)(int volt);
EXPORT_SYMBOL(mtk_dsu_volt2opp_hook);
int pd_dsu_volt2opp(int volt)
{
	if (mtk_dsu_volt2opp_hook)
		return mtk_dsu_volt2opp_hook(volt);
	else
		return 1;
}
EXPORT_SYMBOL_GPL(pd_dsu_volt2opp);

static struct dsu_state dsu_state_tmp;
struct dsu_state *(*mtk_dsu_opp_ps_hook)(int wl, int opp);
EXPORT_SYMBOL(mtk_dsu_opp_ps_hook);
struct dsu_state *dsu_get_opp_ps(int wl, int opp)
{
	if (mtk_dsu_opp_ps_hook)
		return mtk_dsu_opp_ps_hook(wl, opp);
	return &dsu_state_tmp;
}
EXPORT_SYMBOL_GPL(dsu_get_opp_ps);

int (*mtk_leakage_hook)(int cpu, int opp, int temperature);
EXPORT_SYMBOL(mtk_leakage_hook);
unsigned int pd_get_opp_leakage(unsigned int cpu, unsigned int opp, unsigned int temperature)
{
	if (mtk_leakage_hook)
		return mtk_leakage_hook(cpu, opp, temperature);
	return 1;
}
EXPORT_SYMBOL_GPL(pd_get_opp_leakage);

int (*mtk_dsu_freq_hook)(void);
EXPORT_SYMBOL(mtk_dsu_freq_hook);
int pd_get_dsu_freq(void)
{
	if (mtk_dsu_freq_hook)
		return mtk_dsu_freq_hook();
	return 1;
}
EXPORT_SYMBOL_GPL(pd_get_dsu_freq);
/* hook API */

int pd_freq2util(unsigned int cpu, int freq, bool quant, int wl, int *val_s, int r_o)
{
	int opp;

	wl = get_eas_wl(wl);
	opp = pd_freq2opp(cpu, freq, quant, wl);
	return pd_opp2cap(cpu, opp, quant, wl, val_s, r_o, DPT_CALL_PD_FREQ2UTIL);
}
EXPORT_SYMBOL_GPL(pd_freq2util);

int pd_util2freq_r_o(unsigned int cpu, int util, bool quant, int wl)
{
	int opp;

	wl = get_eas_wl(wl);
	opp = pd_util2opp(cpu, util, quant, wl, NULL, true, DPT_CALL_PD_UTIL2FREQ);
	return pd_opp2freq(cpu, opp, quant, wl);
}
EXPORT_SYMBOL_GPL(pd_util2freq_r_o);

int pd_util2freq(unsigned int cpu, int util, bool quant, int wl)
{
	int opp;

	wl = get_eas_wl(wl);
	opp = pd_util2opp(cpu, util, quant, wl, NULL, false, DPT_CALL_PD_UTIL2FREQ);
	return pd_opp2freq(cpu, opp, quant, wl);
}
EXPORT_SYMBOL_GPL(pd_util2freq);

unsigned long pd_get_util_opp(unsigned int cpu, unsigned long util)
{
	return pd_util2opp(cpu, util, false, -1, NULL, false, DPT_CALL_PD_GET_UTIL_OPP);
}
EXPORT_SYMBOL_GPL(pd_get_util_opp);

unsigned long pd_get_util_opp_legacy(unsigned int cpu, unsigned long util)
{
	return pd_util2opp(cpu, util, true, get_em_wl(), NULL, false, DPT_CALL_PD_GET_UTIL_OPP_LEGACY);
}
EXPORT_SYMBOL_GPL(pd_get_util_opp_legacy);

unsigned long pd_get_util_freq(unsigned int cpu, unsigned long util)
{
	int wl = get_em_wl();
	int opp = pd_util2opp(cpu, util, false, wl, NULL, false, DPT_CALL_PD_GET_UTIL_FREQ);

	return pd_opp2freq(cpu, opp, false, wl);
}
EXPORT_SYMBOL_GPL(pd_get_util_freq);

unsigned long pd_get_util_pwr_eff(unsigned int cpu, unsigned long util, int caller)
{
	int wl = get_eas_wl(-1);
	int opp = pd_util2opp(cpu, util, false, wl, NULL, false, caller);

	return pd_opp2pwr_eff(cpu, opp, false, wl, NULL, false, caller);
}
EXPORT_SYMBOL_GPL(pd_get_util_pwr_eff);

unsigned long pd_get_freq_opp(unsigned int cpu, unsigned long freq)
{
	return pd_freq2opp(cpu, freq, false, -1);
}
EXPORT_SYMBOL_GPL(pd_get_freq_opp);

unsigned long pd_get_freq_opp_legacy(unsigned int cpu, unsigned long freq)
{
	return pd_freq2opp(cpu, freq, true, -1);
}
EXPORT_SYMBOL_GPL(pd_get_freq_opp_legacy);

unsigned long pd_get_freq_opp_legacy_type(int wl, unsigned int cpu, unsigned long freq)
{
	return pd_freq2opp(cpu, freq, true, wl);
}
EXPORT_SYMBOL_GPL(pd_get_freq_opp_legacy_type);

unsigned long pd_get_freq_util_legacy(unsigned int cpu, unsigned long freq)
{
	int wl = get_eas_wl(-1);
	int opp = pd_freq2opp(cpu, freq, true, wl);

	return pd_opp2cap(cpu, opp, true, wl, NULL, false, DPT_CALL_PD_GET_FRE_UTIL);
}
EXPORT_SYMBOL_GPL(pd_get_freq_util_legacy);

unsigned long pd_get_freq_util(unsigned int cpu, unsigned long freq)
{
	int wl = get_eas_wl(-1);
	int opp = pd_freq2opp(cpu, freq, false, wl);

	return pd_opp2cap(cpu, opp, false, wl, NULL, false, DPT_CALL_PD_GET_FRE_UTIL);
}
EXPORT_SYMBOL_GPL(pd_get_freq_util);

unsigned long pd_get_freq_pwr_eff(unsigned int cpu, unsigned long freq)
{
	int wl = get_eas_wl(-1);
	int opp = pd_freq2opp(cpu, freq, false, wl);

	return pd_opp2pwr_eff(cpu, opp, false, wl, NULL, false, DPT_CALL_PD_GET_FREQ_PWR_EFF);
}
EXPORT_SYMBOL_GPL(pd_get_freq_pwr_eff);

unsigned long pd_get_opp_freq(unsigned int cpu, int opp)
{
	return pd_opp2freq(cpu, opp, false, -1);
}
EXPORT_SYMBOL_GPL(pd_get_opp_freq);

unsigned long pd_get_opp_capacity(unsigned int cpu, int opp)
{
	return pd_opp2cap(cpu, opp, false, -1, NULL, false, DPT_CALL_PD_GET_OPP_CAPACITY);
}
EXPORT_SYMBOL_GPL(pd_get_opp_capacity);

unsigned long pd_get_opp_capacity_legacy(unsigned int cpu, int opp)
{
	return pd_opp2cap(cpu, opp, true, get_em_wl(), NULL, false, DPT_CALL_PD_GET_OPP_CAPACITY_LEGACY);
}
EXPORT_SYMBOL_GPL(pd_get_opp_capacity_legacy);

unsigned long pd_get_opp_freq_legacy(unsigned int cpu, int opp)
{
	return pd_opp2freq(cpu, opp, true, -1);
}
EXPORT_SYMBOL_GPL(pd_get_opp_freq_legacy);

unsigned long pd_get_opp_pwr_eff(unsigned int cpu, int opp)
{
	return pd_opp2pwr_eff(cpu, opp, false, -1, NULL, false, DPT_CALL_PD_GET_OPP_PWR_EFF);
}
EXPORT_SYMBOL_GPL(pd_get_opp_pwr_eff);

unsigned long pd_get_opp_to(int cpu, unsigned long input, enum sugov_type out_type, bool quant)
{
	switch (out_type) {
	case CAP:
		return quant ? pd_get_opp_capacity_legacy(cpu, input) :
			pd_get_opp_capacity(cpu, input);
	case FREQ:
		return quant ? pd_get_opp_freq_legacy(cpu, input) : pd_get_opp_freq(cpu, input);
	case PWR_EFF:
		return pd_get_opp_pwr_eff(cpu, input);
	default:
		return -EINVAL;
	}
}

unsigned long pd_get_util_to(int cpu, unsigned long input, enum sugov_type out_type, bool quant, int caller)
{
	switch (out_type) {
	case OPP:
		return quant ? pd_get_util_opp_legacy(cpu, input) : pd_get_util_opp(cpu, input);
	case FREQ:
		return pd_get_util_freq(cpu, input);
	case PWR_EFF:
		return pd_get_util_pwr_eff(cpu, input, caller);
	default:
		return -EINVAL;
	}
}

unsigned long pd_get_freq_to(int cpu, unsigned long input, enum sugov_type out_type, bool quant)
{
	switch (out_type) {
	case OPP:
		return quant ? pd_get_freq_opp_legacy(cpu, input) : pd_get_freq_opp(cpu, input);
	case CAP:
		return pd_get_freq_util(cpu, input);
	case PWR_EFF:
		return pd_get_freq_pwr_eff(cpu, input);
	default:
		return -EINVAL;
	}
}

unsigned long pd_X2Y(int cpu, unsigned long input, enum sugov_type in_type,
		enum sugov_type out_type, bool quant, int caller)
{
	switch (in_type) {
	case OPP:
		return pd_get_opp_to(cpu, input, out_type, quant);
	case CAP:
		return pd_get_util_to(cpu, input, out_type, quant, caller);
	case FREQ:
		return pd_get_freq_to(cpu, input, out_type, quant);
	default:
		return -EINVAL;
	}
}
EXPORT_SYMBOL_GPL(pd_X2Y);

unsigned int pd_get_cpu_opp(unsigned int cpu)
{
	return pd_util2opp(cpu, 0, false, -1, NULL, true, DPT_CALL_PD_GET_CPU_OPP) + 1;
}
EXPORT_SYMBOL_GPL(pd_get_cpu_opp);

void Adaptive_module_bypass(int fpsgo_flag)
{
	fpsgo_boosting = fpsgo_flag;
#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
	if (flt_get_fpsgo_boosting)
		flt_get_fpsgo_boosting(!fpsgo_boosting);
#endif // CONFIG_MTK_SCHED_FAST_LOAD_TRACKING
}
EXPORT_SYMBOL_GPL(Adaptive_module_bypass);

int get_fpsgo_bypass_flag(void)
{
	return fpsgo_boosting;
}
EXPORT_SYMBOL_GPL(get_fpsgo_bypass_flag);

static void register_fpsgo_sugov_hooks(void)
{
	fpsgo_notify_fbt_is_boost_fp = Adaptive_module_bypass;
}

#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
#else
#if IS_ENABLED(CONFIG_64BIT)
#define em_scale_power(p) ((p) * 1000)
#else
#define em_scale_power(p) (p)
#endif // CONFIG_64BIT
#endif // CONFIG_MTK_GEARLESS_SUPPORT

bool cu_ctrl;
bool get_curr_uclamp_ctrl(void)
{
	return cu_ctrl;
}
EXPORT_SYMBOL_GPL(get_curr_uclamp_ctrl);
void set_curr_uclamp_ctrl(int val)
{
	cu_ctrl = val ? true : false;
}
EXPORT_SYMBOL_GPL(set_curr_uclamp_ctrl);
int set_curr_uclamp_hint(int pid, int set)
{
	struct task_struct *p;
	struct curr_uclamp_hint *cu_ht;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (!p) {
		rcu_read_unlock();
		return -1;
	}

	if (p->exit_state) {
		rcu_read_unlock();
		return -1;
	}

	get_task_struct(p);
	cu_ht = &((struct mtk_task *)android_task_vendor_data(p))->cu_hint;
	cu_ht->hint = set;
	put_task_struct(p);
	rcu_read_unlock();
	return 0;
}
EXPORT_SYMBOL_GPL(set_curr_uclamp_hint);
int get_curr_uclamp_hint(int pid)
{
	struct task_struct *p;
	struct curr_uclamp_hint *cu_ht;
	int hint;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (!p) {
		rcu_read_unlock();
		return -1;
	}

	if (p->exit_state) {
		rcu_read_unlock();
		return -1;
	}

	get_task_struct(p);
	cu_ht = &((struct mtk_task *)android_task_vendor_data(p))->cu_hint;
	hint = cu_ht->hint;
	put_task_struct(p);
	rcu_read_unlock();
	return hint;
}
EXPORT_SYMBOL_GPL(get_curr_uclamp_hint);

#if IS_ENABLED(CONFIG_UCLAMP_TASK_GROUP)
static int gear_uclamp_max[MAX_NR_CPUS] = {
			[0 ... MAX_NR_CPUS - 1] = SCHED_CAPACITY_SCALE
};
bool gu_ctrl;
bool get_gear_uclamp_ctrl(void)
{
	return gu_ctrl;
}
EXPORT_SYMBOL_GPL(get_gear_uclamp_ctrl);
void set_gear_uclamp_ctrl(int val)
{
	int i;

	gu_ctrl = val ? true : false;
	if (gu_ctrl == false) {
		for (i = 0; i < pd_count; i++)
			gear_uclamp_max[i] = SCHED_CAPACITY_SCALE;
	}
}
EXPORT_SYMBOL_GPL(set_gear_uclamp_ctrl);

int get_gear_uclamp_max(int gearid)
{
	return gear_uclamp_max[gearid];
}
EXPORT_SYMBOL_GPL(get_gear_uclamp_max);

int get_cpu_gear_uclamp_max(unsigned int cpu)
{
	if (gu_ctrl == false)
		return SCHED_CAPACITY_SCALE;
	return gear_uclamp_max[topology_cluster_id(cpu)];
}
EXPORT_SYMBOL_GPL(get_cpu_gear_uclamp_max);

int get_cpu_gear_uclamp_max_capacity(unsigned int cpu, int ret_type)
{
	unsigned long capacity, freq;

	if (gu_ctrl == false) {
		if (ret_type == GU_RET_FREQ)
			return get_cpu_max_freq(cpu);
		else
			return SCHED_CAPACITY_SCALE;
	}

	capacity = get_cpu_util_with_margin(cpu, (gear_uclamp_max[topology_cluster_id(cpu)]));
	freq = pd_get_util_freq(cpu, capacity);

	if (ret_type == GU_RET_FREQ)
		return freq;
	else
		return pd_get_freq_util(cpu, freq);
}
EXPORT_SYMBOL_GPL(get_cpu_gear_uclamp_max_capacity);

void set_gear_uclamp_max(int gearid, int val)
{
	gear_uclamp_max[gearid] = val;
}
EXPORT_SYMBOL_GPL(set_gear_uclamp_max);
#endif

int init_pd_topology(void)
{
	int i, cpu, nr_cpu = 0;

	pd_count = 0;
	for_each_possible_cpu(cpu) {
		pd_count = max(pd_count, topology_cluster_id(cpu));
		nr_cpu++;
	}
	pd_count++;

	cpu2cluster_id = kcalloc(nr_cpu, sizeof(int), GFP_KERNEL);

	pd_cpumask = kcalloc(pd_count, sizeof(struct cpumask), GFP_KERNEL);
	if (!pd_cpumask)
		return -ENOMEM;

	for (i = 0; i < pd_count; i++) {
		cpumask_clear(&pd_cpumask[i]);
		for_each_possible_cpu(cpu)
			if (topology_cluster_id(cpu) == i) {
				cpumask_set_cpu(cpu, &pd_cpumask[i]);
				cpu2cluster_id[cpu] = i;
			}
	}
	return 0;
}

static long *mtk_em_api_data;
long *get_mtk_em_api_data(void)
{
	mtk_em_api_data = kcalloc(4, sizeof(long), GFP_KERNEL);
	mtk_em_api_data[0] = MAX_NR_CPUS;
	mtk_em_api_data[1] = pd_count;
	mtk_em_api_data[2] = nr_wl;
	mtk_em_api_data[3] = (long)cpu2cluster_id;
	return mtk_em_api_data;
}
EXPORT_SYMBOL(get_mtk_em_api_data);

#define MALLOC_OFFSET 2
void mtk_em_malloc(long *data)
{
	int i = 0;

	while(data[i] != 0) {
		data[i] = (long)kcalloc(data[i], data[i + 1], GFP_KERNEL);
		i += MALLOC_OFFSET;
	}
}
EXPORT_SYMBOL(mtk_em_malloc);

void *get_dpt_sram_base(void)
{
	pr_info("get_sram_base at driver\n");
	return dpt_sram_base;
}
EXPORT_SYMBOL(get_dpt_sram_base);

int init_dpt_io(void)
{
	int i;
	struct device_node *dev_node;
	struct platform_device *pdev_temp;
	struct resource *sram_res;
	void **addrs[] = {&coef2_ltime_addr,
					&coef1_ltime_addr,
					&coef2_sratio_addr,
					&coef1_sratio_addr,
					&cpu_sratio_addr};
	int ret = 0, support;

	/* init dpt io*/
	dev_node = of_find_node_by_name(NULL, "dpt-info");
	if (!dev_node) {
		pr_info("failed to find node dpt-info @ %s\n", __func__);
		return -ENODEV;
	}

	pdev_temp = of_find_device_by_node(dev_node);
	if (!pdev_temp) {
		pr_info("failed to find dpt-info pdev @ %s\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(dev_node, "dpt_v2_support", &support);
	if (ret)
		support = 0;

	sched_dpt_v2_enable_set(support);

	sram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 0);
	if (sram_res) {
		dpt_sram_base = ioremap(sram_res->start,
				resource_size(sram_res));
		pr_info("dpt_sram_base init over\n");
	} else {
		pr_info("%s can't get dpt-info resource\n", __func__);
		return -EINVAL;
	}

	if (!dpt_sram_base) {
		pr_info("dpt info failed\n");
		return -EIO;
	}

	pr_info("dpt io init done\n");

	/* init collab type 0 io */
	dev_node = of_find_node_by_name(NULL, "collab-type-0-info");
	if (!dev_node) {
		pr_info("failed to find node collab_type_0-info @ %s\n", __func__);
		return -ENODEV;
	}

	pdev_temp = of_find_device_by_node(dev_node);
	if (!pdev_temp) {
		pr_info("failed to find collab_type_0-info pdev @ %s\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < sizeof(addrs)/sizeof(addrs[0]); i++)
	{
		sram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, i);
		if (sram_res)
		{
			*(addrs[i]) = ioremap(sram_res->start, resource_size(sram_res));
			pr_info("init %p over\n", *(addrs[i]));
		}
		else
		{
			pr_info("init_dpt_io can't get %d\n", i);
			return -EINVAL;
		}
	}

	CHECK_ADDR(coef2_ltime_addr,     "coef2 ltime addr init fail\n")
	CHECK_ADDR(coef1_ltime_addr,      "coef1 ltime addr init fail\n")
	CHECK_ADDR(coef2_sratio_addr,  "coef2 s addr init fail\n")
	CHECK_ADDR(coef1_sratio_addr,   "coef1 s addr init fail\n")
	CHECK_ADDR(cpu_sratio_addr,  "cpu s addr init fail\n")

	pr_info("collab_type_0-info init done\n");
	return 0;
}

void init_dpt_rq(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		dpt_rq_t *dpt_rq = &per_cpu(__dpt_rq, cpu);

		/* clock */
		dpt_rq->local_clock[NON_S] = 0;
		dpt_rq->local_clock[S_COEF1] = 0;
		dpt_rq->local_clock[S_COEF2] = 0;
		dpt_rq->global_clock[NON_S] = 0;
		dpt_rq->global_clock[S_COEF1] = 0;
		dpt_rq->global_clock[S_COEF2] = 0;
		dpt_rq->local_clock_ratio[NON_S] = 1024;
		dpt_rq->local_clock_ratio[S_COEF1] = 0;
		dpt_rq->local_clock_ratio[S_COEF2] = 0;
		dpt_rq->global_clock_ratio[NON_S] = 1024;
		dpt_rq->global_clock_ratio[S_COEF1] = 0;
		dpt_rq->global_clock_ratio[S_COEF2] = 0;

		/* arch scale */
		dpt_rq->arch_s_scale[NON_S] = 1024;
		dpt_rq->arch_s_scale[S_COEF1] = 0;
		dpt_rq->arch_s_scale[S_COEF2] = 0;
		dpt_rq->arch_ltime_scale[S_COEF1] = 0;
		dpt_rq->arch_ltime_scale[S_COEF2] = 0;

		/* sratio info */
		dpt_rq->sratio[NON_S] = 100;
		dpt_rq->sratio[S_COEF1] = 0;
		dpt_rq->sratio[S_COEF2] = 0;

		/* cpu info */
		dpt_rq->cpu_cap_ratio = 0;
		dpt_rq->cpu_freq_ratio = 0;

		/* Manual setting */
		dpt_rq->coef2_ltime_manual = -1;
		dpt_rq->coef1_ltime_manual = -1;
		dpt_rq->cpu_sratio_manual = -1;
		dpt_rq->coef1_sratio_manual = -1;
		dpt_rq->coef2_sratio_manual = -1;

		dpt_rq->dpt_v2_total_util = 0;
		dpt_rq->dpt_v2_cpu_util = 0;
		dpt_rq->dpt_v2_coef1_util = 0;
		dpt_rq->dpt_v2_coef2_util = 0;
		dpt_rq->cfs_cpu_util = 0;
		dpt_rq->cfs_coef1_util = 0;
		dpt_rq->cfs_coef2_util = 0;
		dpt_rq->rt_cpu_util = 0;
		dpt_rq->rt_coef1_util = 0;
		dpt_rq->rt_coef2_util = 0;

		/* util info */
		dpt_rq->local_clock_pelt = cpu_rq(cpu)->clock_pelt;
	}
}

inline struct dpt_rq_struct *get_dpt_rq(int cpu)
{
	return &per_cpu(__dpt_rq, cpu);
}
EXPORT_SYMBOL_GPL(get_dpt_rq);

void set_dpt_v2_info_manual(int cpu, int type, int val)
{
    struct dpt_rq_struct *dpt_rq = &per_cpu(__dpt_rq, cpu);
	int tmp_cpu;

	if (type == DPT_V2_TYPE_COEF2_LTIME) {
        for_each_possible_cpu(tmp_cpu) {
            struct dpt_rq_struct *tmp_dpt_rq = &per_cpu(__dpt_rq, tmp_cpu);
			tmp_dpt_rq->coef2_ltime_manual = max(val, coef2_min_ltime);
        }
    }
	else if (type == DPT_V2_TYPE_COEF1_LTIME) {
        for_each_possible_cpu(tmp_cpu) {
            struct dpt_rq_struct *tmp_dpt_rq = &per_cpu(__dpt_rq, tmp_cpu);
			tmp_dpt_rq->coef1_ltime_manual = max(val, coef1_min_ltime);
        }
    }
	else if (type == DPT_V2_TYPE_COEF1_RATIO)
		dpt_rq->coef1_sratio_manual = val;
	else if (type == DPT_V2_TYPE_COEF2_RATIO)
		dpt_rq->coef2_sratio_manual = val;
	else if (type == DPT_V2_TYPE_CPU_RATIO)
		dpt_rq->cpu_sratio_manual = val;
	else if (type == DPT_V2_TYPE_TOTAL_UTIL)
		dpt_rq->dpt_v2_total_util = val;
	else if (type == DPT_V2_TYPE_CPU_UTIL)
		dpt_rq->dpt_v2_cpu_util = val;
	else if (type == DPT_V2_TYPE_COEF1_UTIL)
		dpt_rq->dpt_v2_coef1_util = val;
	else if (type == DPT_V2_TYPE_COEF2_UTIL)
		dpt_rq->dpt_v2_coef2_util = val;
}
EXPORT_SYMBOL_GPL(set_dpt_v2_info_manual);

#define check_sratio_sanity(ratio) (ratio == 0xdeadbeef) ? USING_LAST_STATE : ratio
#define SET_VALUE(target, min, max, value) \
	if (min <= value && value <= max) \
		(target) = (value);
#define SET_VALUE_MAX(target, max, value) \
	do { \
		if (value <= max)	\
			(target) = (value);	\
	} while (0)

int get_dpt_v2_driver_init_status(void)
{
	return init_dpt_v2_driver_done;
}
EXPORT_SYMBOL(get_dpt_v2_driver_init_status);

void init_dpt_v2_driver(void)
{
	int dpt_v2_default_status, cpu;
	dpt_rq_t *dpt_rq;

	dpt_v2_default_status = sched_dpt_v2_enable_get();

	change_dpt_v2_support_hook(dpt_v2_default_status);
	set_dpt_v2_default_status_hook(dpt_v2_default_status);

	scaling_factor_shift_bit = get_scaling_factor_shift_bit_hook();

	get_coef1_min_max_ltime_hook(&coef1_min_ltime, &coef1_max_ltime);
	get_coef2_min_max_ltime_hook(&coef2_min_ltime, &coef2_max_ltime);
	/* Latency info */
	for_each_possible_cpu(cpu) {
		dpt_rq = &per_cpu(__dpt_rq, cpu);

		dpt_rq->min_ltime[S_COEF1] = coef1_min_ltime;
		dpt_rq->min_ltime[S_COEF2] = coef2_min_ltime;
	}

	pr_info("init dpt_v2_data_done\n");

	init_dpt_v2_driver_done = 1;

}

static DEFINE_SPINLOCK(init_driver_affter_vendor_init_lock);
int init_driver_after_vendor_init_done;
inline int get_init_driver_after_vendor_init_done(void)
{
	return init_driver_after_vendor_init_done;
}
EXPORT_SYMBOL(get_init_driver_after_vendor_init_done);

static unsigned long last_jiffies_init_driver;
void init_driver_after_vendor_init(void)
{
	if (spin_trylock(&init_driver_affter_vendor_init_lock)) {
		unsigned long tmp_jiffies = jiffies;

		if (last_jiffies_init_driver !=  tmp_jiffies) {
			last_jiffies_init_driver =  tmp_jiffies;
			spin_unlock(&init_driver_affter_vendor_init_lock);

			if (!init_dpt_v2_driver_done && dpt_v2_init_done_hook)
				init_dpt_v2_driver();

			if (!init_uclamp_involve_done)
				init_uclamp_involve();

			if (init_dpt_v2_driver_done && init_uclamp_involve_done)
				init_driver_after_vendor_init_done = 1;
		} else
			spin_unlock(&init_driver_affter_vendor_init_lock);
	}
}
EXPORT_SYMBOL(init_driver_after_vendor_init);

inline int __get_scaling_factor_shift_bit(void)
{
	return scaling_factor_shift_bit;
}
EXPORT_SYMBOL(__get_scaling_factor_shift_bit);

void record_scaling_factor_dpt_v2(int cpu, struct dpt_task_struct *dts)
{
	int c, s;

	if (get_cpu_perf_scaling_factor_hook) {
		get_cpu_perf_scaling_factor_hook(cpu, &dts->perf_scaling_factor);

		if (check_cpu_perf_scaling_factor_hook)
			check_cpu_perf_scaling_factor_hook(cpu, &dts->perf_scaling_factor);
	}

	if (get_cpu_power_scaling_factor_hook) {
		get_cpu_power_scaling_factor_hook(cpu, &dts->power_scaling_factor);

		if (check_cpu_power_scaling_factor_hook)
			check_cpu_power_scaling_factor_hook(cpu, &dts->power_scaling_factor);
	}
	
	for (c = 0; c < NUM_CONVERT_TYPE; c++)
		for (s = 0; s < NUM_SCALING_TYPE; s++)
			dts->inv_perf_scaling_factor[c][s] = scale_ratio(1 << __get_scaling_factor_shift_bit(), dts->perf_scaling_factor[c][s]);
}

void update_dpt_v2_info(void)
{
	dpt_rq_t *dpt_rq;
	int cpu, status = 0;
	u64 global_clock_sum = 0, local_clock_sum = 0;
	unsigned int cpu_coef1_sratio[MAX_NR_CPUS], cpu_coef2_sratio[MAX_NR_CPUS], cpu_sratio[MAX_NR_CPUS];
	int coef2_ltime = (coef2_ltime_addr) ? ioread32(coef2_ltime_addr) : 1;
	int coef1_ltime = (coef1_ltime_addr) ? ioread32(coef1_ltime_addr)  : 1;
	unsigned int coef1_s, coef2_s, total_s;
	unsigned long cpu_util[MAX_NR_CPUS], coef1_util[MAX_NR_CPUS], coef2_util[MAX_NR_CPUS];

	if (!init_dpt_v2_driver_done)
		return;

	for_each_possible_cpu(cpu) {
		dpt_rq = &per_cpu(__dpt_rq, cpu);

		coef1_s = (coef1_sratio_addr) ? ioread8(coef1_sratio_addr + (COEF1_COEF2_S_PER_CPU_SIZE * cpu)) : 1;
		coef2_s = (coef2_sratio_addr) ? ioread8(coef2_sratio_addr + (COEF1_COEF2_S_PER_CPU_SIZE * cpu)) : 1;
		total_s = (cpu_sratio_addr) ? ioread32(cpu_sratio_addr + (CPU_S_PER_CPU_SIZE * cpu)) : 1;

		SET_VALUE(dpt_rq->cur_ltime[S_COEF1], coef1_min_ltime, coef1_max_ltime, coef1_ltime);
		SET_VALUE(dpt_rq->cur_ltime[S_COEF1], coef1_min_ltime, coef1_max_ltime, dpt_rq->coef1_ltime_manual);
		SET_VALUE(dpt_rq->cur_ltime[S_COEF2], coef2_min_ltime, coef2_max_ltime, coef2_ltime);
		SET_VALUE(dpt_rq->cur_ltime[S_COEF2], coef2_min_ltime, coef2_max_ltime, dpt_rq->coef2_ltime_manual);

		/* sratio info */
		SET_VALUE_MAX(dpt_rq->sratio[S_COEF1], 100, coef1_s);
		SET_VALUE_MAX(dpt_rq->sratio[S_COEF1], 100, dpt_rq->coef1_sratio_manual);
		SET_VALUE_MAX(dpt_rq->sratio[S_COEF2], 100, coef2_s);
		SET_VALUE_MAX(dpt_rq->sratio[S_COEF2], 100, dpt_rq->coef2_sratio_manual);
		SET_VALUE_MAX(dpt_rq->sratio[S_TOTAL], 100, total_s);
		SET_VALUE_MAX(dpt_rq->sratio[S_TOTAL], 100, dpt_rq->cpu_sratio_manual);
		SET_VALUE_MAX(dpt_rq->sratio[NON_S], 100, (100 - dpt_rq->sratio[S_COEF1] - dpt_rq->sratio[S_COEF2]));

		record_scaling_factor_dpt_v2(cpu, &dpt_rq->util_cfs);

		/**
		* Updates `arch_s_scale` by considering ltime, sratio and CPU
		* scaling_factor to accurately calculate for DPT v2.
		* This value will be used to compute `rq_clock_pelt`
		*/
		arch_set_non_s_dpt_v2(cpu, dpt_rq->sratio[NON_S]);
		arch_set_coef1_s_scale_dpt_v2(cpu, dpt_rq->sratio[S_COEF1]);
		arch_set_coef1_ltime_scale_dpt_v2(cpu, dpt_rq->min_ltime[S_COEF1], dpt_rq->cur_ltime[S_COEF1]);
		arch_set_coef2_s_scale_dpt_v2(cpu, dpt_rq->sratio[S_COEF2]);
		arch_set_coef2_ltime_scale_dpt_v2(cpu, dpt_rq->min_ltime[S_COEF2], dpt_rq->cur_ltime[S_COEF2]);

		/* To calculate the sum of local/global CPU non-s and sratios, we must compute the sum first */
		local_clock_sum = dpt_rq->local_clock[NON_S] + dpt_rq->local_clock[S_COEF1] + dpt_rq->local_clock[S_COEF2];
		global_clock_sum = dpt_rq->global_clock[NON_S] + dpt_rq->global_clock[S_COEF1] + dpt_rq->global_clock[S_COEF2];

		/* Update `local` CPU non-s/sratios for the current window, used for task/rq util updates */
		dpt_rq->local_clock_ratio[S_COEF1] = (local_clock_sum) ? scale_ratio_u64(dpt_rq->local_clock[S_COEF1], local_clock_sum): 0;
		dpt_rq->local_clock_ratio[S_COEF2] = (local_clock_sum) ? scale_ratio_u64(dpt_rq->local_clock[S_COEF2], local_clock_sum): 0;
		dpt_rq->local_clock_ratio[NON_S] = SCHED_CAPACITY_SCALE - dpt_rq->local_clock_ratio[S_COEF1] - dpt_rq->local_clock_ratio[S_COEF2];

		/* Update `global` CPU non-s/sratios for the current window, used for task/rq util updates */
		dpt_rq->global_clock_ratio[S_COEF1] = (global_clock_sum) ? scale_ratio_u64(dpt_rq->global_clock[S_COEF1], global_clock_sum): 0;
		dpt_rq->global_clock_ratio[S_COEF2] = (global_clock_sum) ? scale_ratio_u64(dpt_rq->global_clock[S_COEF2], global_clock_sum): 0;
		dpt_rq->global_clock_ratio[NON_S] = SCHED_CAPACITY_SCALE - dpt_rq->global_clock_ratio[S_COEF1] - dpt_rq->global_clock_ratio[S_COEF2];

		/* Reset the clock sum for next update */
		dpt_rq->local_clock[NON_S] = dpt_rq->local_clock[S_COEF1] = dpt_rq->local_clock[S_COEF2] = 0;
		dpt_rq->global_clock[NON_S] = dpt_rq->global_clock[S_COEF1] = dpt_rq->global_clock[S_COEF2] = 0;

		mtk_cpu_util_next_dpt_v2(cpu, NULL, -1, 0, &cpu_util[cpu], &coef1_util[cpu], &coef2_util[cpu]);

		cpu_coef1_sratio[cpu] = dpt_rq->sratio[S_COEF1];
		cpu_coef2_sratio[cpu] = dpt_rq->sratio[S_COEF2];
		cpu_sratio[cpu] = dpt_rq->sratio[S_TOTAL];
	}

	update_coef1_ltime_perf_model_hook(dpt_rq->cur_ltime[S_COEF1]);
	update_coef2_ltime_perf_model_hook(dpt_rq->cur_ltime[S_COEF2]);

	/* update capacity_orig_of */
	for_each_possible_cpu(cpu) {
		mtk_update_cpu_capacity(cpu, pd_opp2cap(cpu, 0, true, 0, NULL, true,
						DPT_CALL_UPDATE_WL_TBL), 0, CAP_UPDATED_BY_DPT);
	}


	if (trace_collab_type_1_ret_function_enabled()) {
		if (is_dpt_support_driver_hook)
			status = is_dpt_support_driver_hook();
		trace_collab_type_1_ret_function(dpt_rq->cur_ltime[S_COEF2], dpt_rq->cur_ltime[S_COEF1], cpu_coef1_sratio, cpu_coef2_sratio, cpu_sratio, status);
	}
}

int prev_val = 0;
int collab_type_0_ret_function(int version)
{
	unsigned int val, status = 0;
	int result;

	if (coef2_ltime_addr == NULL)
		return 0;

	if (version == DPT_V1 && curr_collab_state_manual[0] != -1)
		return curr_collab_state_manual[0];
	else if (version == DPT_V2) {
		struct dpt_rq_struct *dpt_rq = &per_cpu(__dpt_rq, 0); /* writting CPU0 is not a good design */

		if (dpt_rq->coef2_ltime_manual != -1)
			return dpt_rq->coef2_ltime_manual;
	}

	val = ioread32(coef2_ltime_addr);

	if (trace_collab_type_0_ret_function_enabled()) {
		if (is_dpt_support_driver_hook)
			status = is_dpt_support_driver_hook();
		trace_collab_type_0_ret_function(val, status);
	}

	if (val == 0 || val == 0xdeadbeef)
		result = prev_val;
	else
		result = val;

	prev_val = result;

	return result;
}

int get_nr_collab_type(void)
{
	return nr_collab_type;
}
EXPORT_SYMBOL(get_nr_collab_type);

void set_collab_state_manual(int type, int state)
{
	if (is_dpt_support_driver_hook == NULL)
		return;

	if (type < 0 || type >= nr_collab_type || !is_dpt_support_driver_hook()) {
		pr_info("type=%d exceed nr_collab_type=%d\n", type, nr_collab_type);
		return;
	}

	curr_collab_state_manual[type] = state;
	pr_info("set collab_type=%d, state=%d\n", type, state);
}
EXPORT_SYMBOL_GPL(set_collab_state_manual);

static int init_sram_mapping(void)
{
	struct device_node *dvfs_node;
	struct platform_device *pdev_temp;

	dvfs_node = of_find_node_by_name(NULL, "cpuhvfs");
	if (dvfs_node == NULL) {
		pr_info("failed to find node @ %s\n", __func__);
		return -ENODEV;
	}

	pdev_temp = of_find_device_by_node(dvfs_node);
	if (pdev_temp == NULL) {
		pr_info("failed to find pdev @ %s\n", __func__);
		return -EINVAL;
	}

	csram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 1);

	if (!csram_res) {
		pr_info("%s can't get resource\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static int init_feature_status(void)
{
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
	if (csram_res)
		sram_base_addr_freq_scaling =
			ioremap(csram_res->start + REG_FREQ_SCALING, LUT_ROW_SIZE);
	if (!sram_base_addr_freq_scaling) {
		pr_info("Remap sram_base_addr_freq_scaling failed!\n");
		return -EIO;
	}
	if (readl_relaxed(sram_base_addr_freq_scaling))
		freq_scaling_disabled = false;
#endif // CONFIG_MTK_GEARLESS_SUPPORT
	return 0;
}

int get_dpt_default_status(void)
{
	return dpt_default_status;
}
EXPORT_SYMBOL_GPL(get_dpt_default_status);

int init_opp_cap_info(struct proc_dir_entry *dir)
{
	int ret;
	struct proc_dir_entry *entry;
	ret = init_sram_mapping();
	if (ret) {
		pr_info("init_sram_mapping fail, return=%d\n", ret);
		// should not return, because legacy chip dont have cpuhvfs node
		// return ret;
	}

	ret = init_feature_status();
	if (ret) {
		pr_info("init_feature_status fail, return=%d\n", ret);
		return ret;
	}

	ret = init_pd_topology();
	if (ret)
		return ret;

	if (legacy_api_support_get()) {
		rebuild_sched_domains();
		init_legacy_capacity_table();
	}

#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
	nr_wl = mtk_mapping.total_type;
#endif // CONFIG_MTK_GEARLESS_SUPPORT
	init_sys_max_cap_cpu();

	init_freq_ceiling_floor();


	ret = init_dpt_io();
	if (ret)
		pr_info("init_dpt_io fail, return=%d\n", ret);

	init_dpt_rq();


	if (legacy_api_support_get()) {
		for (int i = 0; i < MAX_NR_CPUS; i++) {
			set_target_margin(i, 20);
			set_target_margin_low(i, 20);
			set_turn_point_freq(i, 1);
		}
	}

#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
	for (int i = 0; i < MAX_NR_CPUS; i++) {
		unset_target_margin(i);
		unset_target_margin_low(i);
		set_turn_point_freq(i, 0);
		set_flt_margin(i, 20);
	}

	if (is_wl_support()) {
		ret = dsu_pwr_swpm_init();
		if (ret) {
			pr_info("dsu_pwr_swpm_init failed\n");
			return ret;
		}
		l3ctl_sram_base_addr = get_l3ctl_sram_base_addr();

		init_eas_dsu_ctrl();

		dsu_freq_init();
	}
#endif // CONFIG_MTK_GEARLESS_SUPPORT

	init_grp_dvfs();

	init_sbb_cpu_data();

	init_adaptive_margin();

	register_fpsgo_sugov_hooks();
	if (legacy_api_support_get()) {
		entry = proc_create("pd_capacity_tbl_legacy", 0644, dir, &pd_capacity_tbl_ops);
		if (!entry)
			pr_info("mtk_scheduler/pd_capacity_tbl_legacy entry create failed\n");
	}
	return ret;
}

#if IS_ENABLED(CONFIG_NONLINEAR_FREQ_CTL)
static inline void mtk_arch_set_freq_scale_gearless(struct cpufreq_policy *policy,
		unsigned int *target_freq)
{
	int i;
	unsigned long cap, max_cap;
	struct cpufreq_mtk *c = policy->driver_data;

	if (c->last_index == policy->cached_resolved_idx) {
		cap = pd_X2Y(policy->cpu, *target_freq, FREQ, CAP,
			false, DPT_CALL_MTK_ARCH_SET_FREQ_SCALE_GEARLESS);
		max_cap = pd_X2Y(policy->cpu, policy->cpuinfo.max_freq, FREQ, CAP,
			false, DPT_CALL_MTK_ARCH_SET_FREQ_SCALE_GEARLESS);
		for_each_cpu(i, policy->related_cpus)
			per_cpu(arch_freq_scale, i) = ((cap << SCHED_CAPACITY_SHIFT) / max_cap);
	}
}

static unsigned int curr_cap[MAX_NR_CPUS];
unsigned int get_curr_cap(unsigned int cpu)
{
	return curr_cap[topology_cluster_id(cpu)];
}
EXPORT_SYMBOL_GPL(get_curr_cap);

static void cpufreq_update_target_freq(struct cpufreq_policy *policy, unsigned int target_freq)
{
	unsigned int cpu = policy->cpu;
	bool dsu_idle_ctrl = is_dsu_idle_enable();

	irq_log_store();

	if (trace_sugov_ext_gear_state_enabled())
		trace_sugov_ext_gear_state(topology_cluster_id(cpu),
			pd_get_freq_opp(cpu, target_freq));

	if (policy->cached_target_freq != target_freq) {
		policy->cached_target_freq = target_freq;
		policy->cached_resolved_idx = pd_X2Y(cpu, target_freq, FREQ, OPP,
			true, DPT_CALL_CPUFREQ_UPDATE_TARGET_FREQ);
	}

	curr_cap[topology_cluster_id(cpu)] = pd_get_opp_capacity_legacy(policy->cpu,
		policy->cached_resolved_idx);

	if (is_gearless_support())
		mtk_arch_set_freq_scale_gearless(policy, &target_freq);

	if (freq_state.is_eas_dsu_support) {
		if (freq_state.is_eas_dsu_ctrl)
			set_dsu_target_freq(policy);
		else {
			struct cpufreq_mtk *c = policy->driver_data;

			c->sb_ch = -1;
			if (trace_sugov_ext_dsu_freq_vote_enabled())
				trace_sugov_ext_dsu_freq_vote(UINT_MAX, topology_cluster_id(cpu),
					dsu_idle_ctrl, target_freq, UINT_MAX, 0,
					-1,-1,0);
		}
	}

	irq_log_store();
}

void mtk_cpufreq_fast_switch(void *data, struct cpufreq_policy *policy,
				unsigned int *target_freq, unsigned int old_target_freq)
{
	cpufreq_update_target_freq(policy, *target_freq);
}

void mtk_cpufreq_target(void *data, struct cpufreq_policy *policy,
				unsigned int *target_freq, unsigned int old_target_freq)
{
	cpufreq_update_target_freq(policy, *target_freq);
}

void mtk_arch_set_freq_scale(void *data, const struct cpumask *cpus,
		unsigned long freq, unsigned long max, unsigned long *scale)
{
	int cpu = cpumask_first(cpus);
	unsigned long cap, max_cap;
	struct cpufreq_policy *policy;

	if(is_dpt_v2_support())
		return;

	irq_log_store();

	policy = cpufreq_cpu_get(cpu);
	if (policy) {
		freq = READ_ONCE(policy->cached_target_freq);
		cpufreq_cpu_put(policy);
	}
	cap = pd_X2Y(cpu, freq, FREQ, CAP, false, DPT_CALL_MTK_ARCH_SET_FREQ_SCALE);
	max_cap = pd_X2Y(cpu, max, FREQ, CAP, false, DPT_CALL_MTK_ARCH_SET_FREQ_SCALE);
	if (max_cap == 0)
		return;
	*scale = ((cap << SCHED_CAPACITY_SHIFT) / max_cap);
	irq_log_store();
}

unsigned int util_scale = 1280;
int sysctl_sched_capacity_margin_dvfs = 20;
unsigned int turn_point_util[MAX_NR_CPUS];
unsigned int target_margin[MAX_NR_CPUS];
unsigned int target_margin_low[MAX_NR_CPUS];
unsigned int target_margin_enable[MAX_NR_CPUS];
unsigned int target_margin_low_enable[MAX_NR_CPUS];

/*
 * set sched capacity margin for DVFS, Default = 20
 */
int set_sched_capacity_margin_dvfs(int capacity_margin)
{
	if (capacity_margin < -2000 || capacity_margin > 95)
		return -1;

	sysctl_sched_capacity_margin_dvfs = capacity_margin;
	util_scale = ((SCHED_CAPACITY_SCALE * 100) / (100 - sysctl_sched_capacity_margin_dvfs));

	return 0;
}
EXPORT_SYMBOL_GPL(set_sched_capacity_margin_dvfs);

int get_sched_capacity_margin_dvfs(void)
{

	return sysctl_sched_capacity_margin_dvfs;
}
EXPORT_SYMBOL_GPL(get_sched_capacity_margin_dvfs);

int set_target_margin(int cpu, int margin)
{
	struct cpufreq_policy *policy;
	int i = 0;

	if (cpu < 0 || cpu > MAX_NR_CPUS)
		return -1;

	if (margin < -2000 || margin > 99)
		return -1;

	policy = cpufreq_cpu_get(cpu);
	if (policy) {
		for_each_cpu(i, policy->related_cpus) {
			target_margin[i] = (SCHED_CAPACITY_SCALE * 100 / (100 - margin));
			target_margin_enable[i] = 1;
		}
		cpufreq_cpu_put(policy);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(set_target_margin);

int set_target_margin_low(int cpu, int margin)
{
	struct cpufreq_policy *policy;
	int i = 0;

	if (cpu < 0 || cpu > MAX_NR_CPUS)
		return -1;

	if (margin < -2000 || margin > 99)
		return -1;

	policy = cpufreq_cpu_get(cpu);
	if (policy) {
		for_each_cpu(i, policy->related_cpus) {
			target_margin_low[i] = (SCHED_CAPACITY_SCALE * 100 / (100 - margin));
			target_margin_low_enable[i] = 1;
		}
		cpufreq_cpu_put(policy);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(set_target_margin_low);

int get_target_margin(int cpu)
{
	return (100 - (SCHED_CAPACITY_SCALE * 100) / target_margin[cpu]);
}
EXPORT_SYMBOL_GPL(get_target_margin);

int get_target_margin_low(int cpu)
{
	return (100 - (SCHED_CAPACITY_SCALE * 100) / target_margin_low[cpu]);
}
EXPORT_SYMBOL_GPL(get_target_margin_low);

/* target_margin not used */ 
int unset_target_margin(int cpu)
{
	struct cpufreq_policy *policy;
	int i = 0;

	policy = cpufreq_cpu_get(cpu);
	if (policy) {
		for_each_cpu(i, policy->related_cpus) {
			target_margin[i] = (SCHED_CAPACITY_SCALE * 100 / (80));
			target_margin_enable[i] = 0;
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(unset_target_margin);

/* target_margin_low not used */ 
int unset_target_margin_low(int cpu)
{
	struct cpufreq_policy *policy;
	int i = 0;

	policy = cpufreq_cpu_get(cpu);
	if (policy) {
		for_each_cpu(i, policy->related_cpus) {
			target_margin_low[i] = (SCHED_CAPACITY_SCALE * 100 / (80));
			target_margin_low_enable[i] = 0;
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(unset_target_margin_low);


/*
 *for vonvenient, pass freq, but converty to util
 */
int set_turn_point_freq(int cpu, unsigned long freq)
{
	int i = 0;
	struct cpufreq_policy *policy;

	if (cpu < 0 || cpu > MAX_NR_CPUS)
		return -1;

	if (freq == 0) {
		turn_point_util[cpu] = 0;
		return 0;
	}

	policy = cpufreq_cpu_get(cpu);
	if (policy) {
		for_each_cpu(i, policy->related_cpus) {
			turn_point_util[i] = pd_freq2util(cpu, freq, false, -1, NULL, true);
		}
		cpufreq_cpu_put(policy);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(set_turn_point_freq);

inline unsigned long get_turn_point_freq(int cpu)
{
	if (turn_point_util[cpu] == 0)
		return 0;

	return pd_util2freq(cpu, turn_point_util[cpu], false, -1);
}
EXPORT_SYMBOL_GPL(get_turn_point_freq);

/* adaptive margin */
static int am_wind_dura = 4000; /* microsecond */
static int am_window = 2;
static int am_floor = 1024; /* 1024: 0% margin */
static int am_ceiling = 1280; /* 1280: 20% margin */
static int am_target_active_ratio_cap[MAX_NR_CPUS] = {
	[0 ... MAX_NR_CPUS - 1] = 819};
static unsigned int adaptive_margin[MAX_NR_CPUS] = {
	[0 ... MAX_NR_CPUS - 1] = 1280};
static unsigned int his_ptr[MAX_NR_CPUS];
static unsigned int margin_his[MAX_NR_CPUS][MAX_NR_CPUS];
static u64 last_wall_time_stamp[MAX_NR_CPUS];
static u64 last_idle_time_stamp[MAX_NR_CPUS];
static u64 last_idle_duratio[MAX_NR_CPUS];
static unsigned int cpu_active_ratio_cap[MAX_NR_CPUS];
static unsigned int policy_max_active_ratio_cap[MAX_NR_CPUS];
static unsigned int policy_update_active_ratio_cnt[MAX_NR_CPUS];
static unsigned int policy_update_active_ratio_cnt_last[MAX_NR_CPUS];
static unsigned int duration_wind[MAX_NR_CPUS];
static unsigned int duration_act[MAX_NR_CPUS];
static unsigned int ramp_up[MAX_NR_CPUS] = {
	[0 ... MAX_NR_CPUS - 1] = 0};
unsigned int get_adaptive_margin(unsigned int cpu)
{
	if (!turn_point_util[cpu] && am_ctrl)
		return READ_ONCE(adaptive_margin[cpu]);
	else
		return util_scale;
}
EXPORT_SYMBOL_GPL(get_adaptive_margin);

int get_cpu_util_with_margin(int cpu, int cpu_util)
{
	return (cpu_util * get_adaptive_margin(cpu)) >> SCHED_CAPACITY_SHIFT;
}
EXPORT_SYMBOL_GPL(get_cpu_util_with_margin);

int get_cpu_active_ratio_cap(int cpu)
{
	return cpu_active_ratio_cap[cpu];
}
EXPORT_SYMBOL_GPL(get_cpu_active_ratio_cap);

int get_gear_max_active_ratio_cap(int gear_id)
{
	int cpu, max_val = 0;

	for_each_cpu(cpu, &pd_cpumask[gear_id])
		if (max_val < cpu_active_ratio_cap[cpu])
			max_val = cpu_active_ratio_cap[cpu];
	return max_val;
}
EXPORT_SYMBOL_GPL(get_gear_max_active_ratio_cap);

void set_target_active_ratio_pct(int gear_id, int val)
{
	int cpu;

	for_each_cpu(cpu, &pd_cpumask[gear_id])
		am_target_active_ratio_cap[cpu] =
			(clamp_val(val, 1, 100) << SCHED_CAPACITY_SHIFT) / 100;
}
EXPORT_SYMBOL_GPL(set_target_active_ratio_pct);

void set_target_active_ratio_cap(int gear_id, int val)
{
	int cpu;

	for_each_cpu(cpu, &pd_cpumask[gear_id])
		am_target_active_ratio_cap[cpu] =
			clamp_val(val, 1, SCHED_CAPACITY_SCALE);
}
EXPORT_SYMBOL_GPL(set_target_active_ratio_cap);

void set_am_ceiling(int val)
{
	am_ceiling = val;
}
EXPORT_SYMBOL_GPL(set_am_ceiling);

int get_am_ceiling(void)
{
	return am_ceiling;
}
EXPORT_SYMBOL_GPL(get_am_ceiling);

bool ignore_idle_ctrl;
void set_ignore_idle_ctrl(bool val)
{
	ignore_idle_ctrl = val;
}
EXPORT_SYMBOL_GPL(set_ignore_idle_ctrl);
bool get_ignore_idle_ctrl(void)
{
	return ignore_idle_ctrl;
}
EXPORT_SYMBOL_GPL(get_ignore_idle_ctrl);

#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
unsigned long (*flt_sched_get_cpu_group_util_eas_hook)(int cpu, int group_id);
EXPORT_SYMBOL(flt_sched_get_cpu_group_util_eas_hook);
int (*flt_sched_get_flt_coef_util_eas_hook)(int cpu, enum _flt_cpu_type type);
EXPORT_SYMBOL(flt_sched_get_flt_coef_util_eas_hook);
unsigned long (*flt_get_cpu_util_hook)(int cpu);
EXPORT_SYMBOL(flt_get_cpu_util_hook);
int (*get_group_hint_hook)(int group);
EXPORT_SYMBOL(get_group_hint_hook);
#endif // CONFIG_MTK_SCHED_FAST_LOAD_TRACKING

#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
int group_aware_dvfs_util(struct cpumask *cpumask)
{
#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
	unsigned long cpu_util = 0;
	unsigned long ret_util = 0;
	unsigned long max_ret_util = 0;
	unsigned long umax = 0;
	int cpu;
	struct rq *rq;
	int am = 0;
	struct sugov_rq_data *sugov_data_ptr;

	for_each_cpu(cpu, cpumask) {
		rq = cpu_rq(cpu);
		sugov_data_ptr = &per_cpu(rq_data, cpu)->sugov_data;
		if ((READ_ONCE(sugov_data_ptr->enq_ing) == 0) && mtk_available_idle_cpu(cpu))
			goto skip_idle;

		am = get_adaptive_margin(cpu);

		cpu_util = flt_get_cpu_util_hook(cpu);

		umax = rq->uclamp[UCLAMP_MAX].value;
		if (gu_ctrl)
			umax = min_t(unsigned long, umax, get_cpu_gear_uclamp_max(cpu));
		ret_util = min_t(unsigned long,
			cpu_util, umax * am >> SCHED_CAPACITY_SHIFT);

		max_ret_util = max(max_ret_util, ret_util);
skip_idle:
		if (trace_sugov_ext_tar_enabled())
			trace_sugov_ext_tar(cpu, ret_util, cpu_util, umax, am);
	}
	return max_ret_util;
#endif
	return -1;
}
#endif // CONFIG_MTK_SCHED_GROUP_AWARE

inline int update_cpu_active_ratio(int cpu_idx)
{
	u64 idle_time_stamp, wall_time_stamp, duration;
	u64 idle_duration, active_duration;

	idle_time_stamp = get_cpu_idle_time(cpu_idx, &wall_time_stamp, 1);
	duration = wall_time_stamp - last_wall_time_stamp[cpu_idx];
	idle_duration = idle_time_stamp - last_idle_time_stamp[cpu_idx];
	idle_duration = min_t(u64, idle_duration, duration);
	last_idle_duratio[cpu_idx] = idle_duration;
	active_duration = duration - idle_duration;
	last_idle_time_stamp[cpu_idx] = idle_time_stamp;
	last_wall_time_stamp[cpu_idx] = wall_time_stamp;
	duration_wind[cpu_idx] = duration;
	duration_act[cpu_idx] = active_duration;
	return div_u64((active_duration << SCHED_CAPACITY_SHIFT), duration);
}

void update_active_ratio_policy(struct cpumask *cpumask)
{
	unsigned int cpu_idx, cpu_id, first_cpu = cpumask_first(cpumask);
	unsigned int policy_max_active_ratio_tmp = 0;

	for_each_cpu(cpu_idx, cpumask) {
		cpu_active_ratio_cap[cpu_idx] = update_cpu_active_ratio(cpu_idx);
		if (cpu_active_ratio_cap[cpu_idx] > policy_max_active_ratio_tmp) {
			policy_max_active_ratio_tmp = cpu_active_ratio_cap[cpu_idx];
			cpu_id = cpu_idx;
		}
	}
	if (last_idle_duratio[cpu_id] > am_wind_dura)
		WRITE_ONCE(ramp_up[first_cpu], 2);
	WRITE_ONCE(policy_max_active_ratio_cap[first_cpu], policy_max_active_ratio_tmp);
	WRITE_ONCE(policy_update_active_ratio_cnt[first_cpu],
		policy_update_active_ratio_cnt[first_cpu] + 1);
}
static bool grp_trigger;
void update_active_ratio_all(void)
{
	int cpu;
	struct cpufreq_policy *policy;

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (policy) {
			update_active_ratio_policy(policy->related_cpus);
			cpu = cpumask_last(policy->related_cpus);
			cpufreq_cpu_put(policy);
		}
	}
	grp_trigger = true;
}
EXPORT_SYMBOL(update_active_ratio_all);

inline void update_adaptive_margin(struct cpumask *sg_cpumask)
{
	unsigned int i;
	unsigned int first_cpu = cpumask_first(sg_cpumask);
	unsigned int adaptive_margin_tmp;

	if (policy_update_active_ratio_cnt_last[first_cpu]
			!= READ_ONCE(policy_update_active_ratio_cnt[first_cpu])) {
		policy_update_active_ratio_cnt_last[first_cpu] =
			READ_ONCE(policy_update_active_ratio_cnt[first_cpu]);

		if (READ_ONCE(ramp_up[first_cpu]) == 0) {
			unsigned int adaptive_ratio =
					((READ_ONCE(policy_max_active_ratio_cap[first_cpu])
					<< SCHED_CAPACITY_SHIFT)
					/ am_target_active_ratio_cap[first_cpu]);

			adaptive_margin_tmp = READ_ONCE(adaptive_margin[first_cpu]);
			adaptive_margin_tmp =
				(adaptive_margin_tmp * adaptive_ratio)
				>> SCHED_CAPACITY_SHIFT;
			adaptive_margin_tmp =
				clamp_val(adaptive_margin_tmp, am_floor, am_ceiling);
			his_ptr[first_cpu]++;
			his_ptr[first_cpu] %= am_window;
			margin_his[first_cpu][his_ptr[first_cpu]] = adaptive_margin_tmp;
			for (i = 0; i < am_window; i++)
				if (margin_his[first_cpu][i] > adaptive_margin_tmp)
					adaptive_margin_tmp = margin_his[first_cpu][i];
		} else {
			adaptive_margin_tmp = util_scale;
			WRITE_ONCE(ramp_up[first_cpu], ramp_up[first_cpu] - 1);
		}
		for_each_cpu(i, sg_cpumask)
			WRITE_ONCE(adaptive_margin[i], adaptive_margin_tmp);

		if (trace_sugov_ext_adaptive_margin_enabled())
			trace_sugov_ext_adaptive_margin(first_cpu,
				READ_ONCE(adaptive_margin[first_cpu]),
				READ_ONCE(policy_max_active_ratio_cap[first_cpu]));
	}
}

static bool grp_high_freq[MAX_NR_CPUS];
bool get_grp_high_freq(int cluster_id)
{
	return grp_high_freq[cluster_id];
}
EXPORT_SYMBOL(get_grp_high_freq);
void set_grp_high_freq(int cluster_id, bool set)
{
	grp_high_freq[cluster_id] = set;
}
EXPORT_SYMBOL(set_grp_high_freq);

void check_update_adaptive_margin_disabled(struct cpumask *cpumask)
{
	int i;

	if (am_ctrl == 0 || am_ctrl == 9)
		for_each_cpu(i, cpumask)
			WRITE_ONCE(adaptive_margin[i], util_scale);
}

int mtk_effective_cpu_util_with_margin_from_adap_grp(int util, int cpu, struct cpumask *sg_cpumask, int source)
{
	u64 wall_time_stamp;
	int pelt_util = util;
	int gearid __maybe_unused = topology_cluster_id(cpu), i;
	unsigned long flt_util = 0, pelt_util_with_margin = 0;
	struct cpumask *cpumask;
	struct cpumask mask;
	int first_cpu;

	if (sg_cpumask) {
		cpumask = sg_cpumask;
		first_cpu = cpumask_first(cpumask);

		if (grp_dvfs_ctrl_mode == 0 || grp_trigger == false) {
			get_cpu_idle_time(first_cpu, &wall_time_stamp, 1);

			if (wall_time_stamp - last_wall_time_stamp[first_cpu] > am_wind_dura)
				update_active_ratio_policy(cpumask);
		}

		update_adaptive_margin(cpumask);
	} else {
		cpumask_clear(&mask);
		cpumask_set_cpu(cpu, &mask);

		cpumask = &mask;
		first_cpu = cpu;
	}

	if (am_ctrl == 0 || am_ctrl == 9)
		for_each_cpu(i, cpumask)
			WRITE_ONCE(adaptive_margin[i], util_scale);

	pelt_util_with_margin =
		(util * READ_ONCE(adaptive_margin[first_cpu])) >> SCHED_CAPACITY_SHIFT;

#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
	if (flt_get_cpu_util_hook && grp_dvfs_ctrl_mode &&
			(wl_cpu_curr != 4 || grp_high_freq[gearid]))
		flt_util = group_aware_dvfs_util(get_gear_cpumask(gearid));
	if (grp_dvfs_ctrl_mode == 9)
		flt_util = 0;
#endif

	util = max_t(int, pelt_util_with_margin, flt_util);

	if (trace_sugov_ext_group_dvfs_enabled())
		trace_sugov_ext_group_dvfs(first_cpu, util, flt_util,
				pelt_util_with_margin, pelt_util, READ_ONCE(adaptive_margin[first_cpu]), source);

	return util;
}

int mtk_effective_cpu_util_with_margin_from_turn_point(int util, int cpu, struct cpumask *sg_cpumask, int source)
{
	int pelt_util = util;

	if (target_margin_enable[cpu] && util >= turn_point_util[cpu])
		util = max(turn_point_util[cpu], util * target_margin[cpu] >> SCHED_CAPACITY_SHIFT);
	else if (target_margin_low_enable[cpu] && util < turn_point_util[cpu])
		util = min(turn_point_util[cpu], util * target_margin_low[cpu] >> SCHED_CAPACITY_SHIFT);
	else if (am_ctrl || grp_dvfs_ctrl_mode)
		util = mtk_effective_cpu_util_with_margin_from_adap_grp(util, cpu, sg_cpumask, source);
	else
		util = map_util_perf(util);

	if (trace_sugov_ext_turn_point_margin_enabled()) {
		int pelt_util_with_orig_margin = (pelt_util * util_scale) >> SCHED_CAPACITY_SHIFT;

		trace_sugov_ext_turn_point_margin(cpu, util, pelt_util,
				turn_point_util[cpu], target_margin[cpu], target_margin_low[cpu],
				pelt_util_with_orig_margin,
				am_ctrl, grp_dvfs_ctrl_mode);
	}

	return util;
}

unsigned int flt_margin[MAX_NR_CPUS];

int set_flt_margin(int cpu, int margin)
{
	struct cpufreq_policy *policy;
	int i = 0;

	if (cpu < 0 || cpu > MAX_NR_CPUS)
		return -1;

	if (margin < -2000 || margin > 99)
		return -1;

	policy = cpufreq_cpu_get(cpu);
	if (policy) {
		for_each_cpu(i, policy->related_cpus) {
			flt_margin[i] = (SCHED_CAPACITY_SCALE * 100 / (100 - margin));
		}
		cpufreq_cpu_put(policy);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(set_flt_margin);

int get_flt_margin(int cpu)
{
	return (100 - (SCHED_CAPACITY_SCALE * 100) / flt_margin[cpu]);
}
EXPORT_SYMBOL_GPL(get_flt_margin);

int mtk_effective_flt_util_with_margin(int util, int cpu, struct cpumask *sg_cpumask, int source)
{
	int flt_util = -1, flt_util_margin = -1;

#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
	if (flt_sched_get_flt_coef_util_eas_hook) {
		flt_util = flt_sched_get_flt_coef_util_eas_hook(cpu, 2);
		if(flt_util >= 0)
			flt_util_margin = flt_util * flt_margin[cpu] >> SCHED_CAPACITY_SHIFT;
	}
#endif

	if (trace_sugov_ext_flt_util_with_coef_margin_enabled())
		trace_sugov_ext_flt_util_with_coef_margin(cpu, util, flt_util, flt_margin[cpu]);

	return flt_util_margin;
}

/* modified from kmainline map_uitl_perf() */
int mtk_effective_cpu_util_with_margin(int util, int cpu, struct cpumask *sg_cpumask, int source)
{
	int tmp_util = -1;

	if(unlikely(get_flt_coef_margin_ctrl()))
		tmp_util = mtk_effective_flt_util_with_margin(util, cpu, sg_cpumask, source);

	if(tmp_util < 0) {
		if (turn_point_util[cpu])
			util = mtk_effective_cpu_util_with_margin_from_turn_point(util, cpu, sg_cpumask, source);
		else if (am_ctrl || grp_dvfs_ctrl_mode)
			util = mtk_effective_cpu_util_with_margin_from_adap_grp(util, cpu, sg_cpumask, source);
		else
			util = map_util_perf(util);
	} else
		util = tmp_util;

	return util;
}

int mtk_effective_cpu_util_with_uclamp(int util, int cpu,
		unsigned long min, unsigned long max, int curr_task_uclamp)
{
	if (curr_task_uclamp) {
		struct rq *rq;
		struct task_struct *curr_task;
		unsigned long max_util_curr = ULONG_MAX;
		unsigned long max_util_curr_eff = ULONG_MAX;
		int flg_curr_task = -1, flg_exit_state = -1, flg_pid = -1;
		unsigned long umax = 0;
		int cpu_util_mgn = util;

		rq = cpu_rq(cpu);

		rcu_read_lock();

		curr_task = rcu_dereference(rq->curr);

		if (curr_task) {
			flg_curr_task = 1;
			flg_pid = curr_task->pid;

			if (!curr_task->exit_state) {
				flg_exit_state = 0;
				max_util_curr = curr_task->uclamp_req[UCLAMP_MAX].value;
				max_util_curr_eff = curr_task->uclamp[UCLAMP_MAX].value;
			} else {
				flg_exit_state = 1;
			}
		} else {
			flg_curr_task = 0;
		}

		rcu_read_unlock();

		umax = min_t(unsigned long, max_util_curr, rq->uclamp[UCLAMP_MAX].value);
		util = min_t(unsigned long, util, umax);

		if (trace_sugov_ext_curr_task_uclamp_enabled())
			trace_sugov_ext_curr_task_uclamp(cpu, flg_pid, flg_curr_task, flg_exit_state,
				util, cpu_util_mgn,
				max_util_curr, max_util_curr_eff, rq->uclamp[UCLAMP_MAX].value);
	} else {
		util = sugov_effective_cpu_perf_clamp(util, min, max);
	}

	return util;
}

inline int is_dpt_v2_support (void) {
	return (is_dpt_v2_support_hook && is_dpt_v2_support_hook() && dpt_v2_init_done_hook);
}
EXPORT_SYMBOL(is_dpt_v2_support);

void dpt_v2_change_config(int turn_on)
{
	pr_info("turn_on=%d\n", turn_on);
	change_dpt_v2_support_hook(turn_on);
}
EXPORT_SYMBOL(dpt_v2_change_config);

int dpt_v2_get_config(void)
{
	return is_dpt_v2_support();
}
EXPORT_SYMBOL(dpt_v2_get_config);

// TODO
void check_update_adaptive_margin(int cpu, struct cpufreq_policy *policy, struct cpumask *cpumask)
{
	u64 wall_time_stamp;

	if (grp_dvfs_ctrl_mode == 0 || grp_trigger == false) {
		get_cpu_idle_time(cpu, &wall_time_stamp, 1);
		if (wall_time_stamp - last_wall_time_stamp[cpu] > am_wind_dura)
			update_active_ratio_policy(cpumask);
	}
	update_adaptive_margin(cpumask);
}
unsigned long get_freq_margin(int cpu, unsigned long util)
{
	struct cpufreq_policy *policy = get_cpufreq_policy(cpu);
	struct cpumask *cpumask;
	unsigned int first_cpu;
	struct rq *rq;
	unsigned long rq_uclamp_min, rq_uclamp_max, margin = util_scale;
	int margin_id = -1;

	if(!policy)
		return margin;

	cpumask = policy->related_cpus;
	first_cpu = cpumask_first(cpumask);

	if (!turn_point_util[cpu] && (am_ctrl || grp_dvfs_ctrl_mode)) {

		/* using default marign when uclamp involved */
		rq = cpu_rq(cpu);
		rq_uclamp_min = READ_ONCE(rq->uclamp[UCLAMP_MIN].value);
		rq_uclamp_max = READ_ONCE(rq->uclamp[UCLAMP_MAX].value);
		if (mtk_uclamp_involve(rq_uclamp_min, rq_uclamp_max)) {
			margin = util_scale;
			margin_id = 0;
			goto done;
		}

		/* using adaptive margin */
		check_update_adaptive_margin(cpu, policy, cpumask); // TODO
		check_update_adaptive_margin_disabled(cpumask); {
			margin = adaptive_margin[first_cpu];
			margin_id = 1;
			goto done;
		}
	}

	/* using turning point margin */
	if (turn_point_util[cpu] &&
		util >= turn_point_util[cpu]) {
		margin = target_margin[cpu];
		margin_id = 2;
	} else if (turn_point_util[cpu] &&
		util < turn_point_util[cpu]) {
		margin = target_margin_low[cpu];
		margin_id = 3;
	}

done:
	if (trace_sugov_get_freq_margin_enabled())
		trace_sugov_get_freq_margin(cpu, margin, margin_id);

	return margin;
}

/* dpt_v2_get_uclamped_cpu_util - get uclamped dpt v2 cpu_util.
 *
 * DPT v2 depends on cpu_util & coef1_util & coef2_util to influence the frequency.
 * uclamp on total_util will not change the above util, and coef1_util & coef2_util is fixed.
 * Therefore the target of uclamp should effect cpu_util.
 *
 * First we have to check if original the frequency calculated by the original util is between umin_freq & umax_freq.
 * If not, calculate the target_cap via target_freq, then fix taget_cap & coef1_util & coef2_util to get cpu_util_prime,
 * replace the original cpu_util by cpu_util_prime.
 */
__always_inline
unsigned long dpt_v2_get_uclamped_cpu_util(int cpu, unsigned long max_util, unsigned long min_util,
	unsigned long cpu_util_local, unsigned long coef1_util_local, unsigned long coef2_util_local, int quant, int *using_uclamp_freq)
{
	unsigned long target_freq, target_cap = 0, cpu_util_prime = 0, result;
	unsigned long umin_freq = 0, umax_freq = 0, __umin = min_util, __umax = max_util;
	unsigned long util = 0, util_freq = 0, __margin, coefs[2];
	int wl_for_uclamp = get_eas_wl(0);

	/* k6.12 uclamp不需要*1.25倍, user調整前先暫時保留 */
	if (mtk_uclamp_involve(min_util, max_util)) {
		min_util = clamp((min_util * DEFAULT_MARGIN) >> SCHED_CAPACITY_SHIFT,
			0UL, (unsigned long) SCHED_CAPACITY_SCALE);
		max_util = clamp((max_util * DEFAULT_MARGIN) >> SCHED_CAPACITY_SHIFT,
			0UL, (unsigned long) SCHED_CAPACITY_SCALE);
	} else {
		result = cpu_util_local;
		goto done;
	}

	umin_freq = pd_opp2freq(cpu, pd_util2opp(cpu, min_util, quant, wl_for_uclamp, NULL, true, DPT_CALL_PD_UTIL2FREQ), quant, wl_for_uclamp);
	umax_freq = pd_opp2freq(cpu, pd_util2opp(cpu, max_util, quant, wl_for_uclamp, NULL, true, DPT_CALL_PD_UTIL2FREQ), quant, wl_for_uclamp);

	util = dpt_v2_util2cap_needed_local_hook(cpu_util_local, coef1_util_local, coef2_util_local, coefs);

	cpu_util_local = cpu_util_local == 0 ? 1 : cpu_util_local;

	__margin = get_freq_margin(cpu, util);
	util_freq = dpt_v2_linear_local_cap2freq_hook(cpu, false, util * __margin >> SCHED_CAPACITY_SHIFT, __umin, __umax, true);

	/* (Mainline comment)
	 * Since CPU's {min,max}_util clamps are MAX aggregated considering
	 * RUNNABLE tasks with _different_ clamps, we can end up with an
	 * inversion. Fix it now when the clamps are applied.
	 */
	if (unlikely(umin_freq >= umax_freq))
		target_freq = umin_freq;
	else if (util_freq < umin_freq)
		target_freq = umin_freq;
	else if (util_freq > umax_freq)
		target_freq = umax_freq;
	else {
		result = cpu_util_local;
		goto done;
	}

	target_cap = dpt_v2_freq2linear_cap_local_hook(quant, cpu, target_freq);
	cpu_util_prime = clamp(target_cap * (DPT_V2_MAX_RUNNING_TIME - rescale_coef1_util_or_ratio_hook(coef1_util_local, RESCALE_UTIL) - \
		rescale_coef2_util_or_ratio_hook(coef2_util_local, RESCALE_UTIL)) / DPT_V2_MAX_RUNNING_TIME, 0, DPT_V2_MAX_RUNNING_TIME);
	result = cpu_util_prime;

	if (using_uclamp_freq)
		*using_uclamp_freq = 1;

done:
	if (trace_sugov_ext_dpt_v2_get_uclamped_cpu_util_enabled()) {
		unsigned long arr_debug[4];

		arr_debug[0] = umin_freq;
		arr_debug[1] = umax_freq;
		arr_debug[2] = util_freq;
		trace_sugov_ext_dpt_v2_get_uclamped_cpu_util(cpu, min_util, max_util, cpu_util_prime, cpu_util_local, coef1_util_local, coef2_util_local,
			target_cap, util, arr_debug, result);
	}

	return result;
}
EXPORT_SYMBOL(dpt_v2_get_uclamped_cpu_util);

void mtk_map_util_freq_dpt_v2(void *data, int cpu, unsigned long *next_freq, unsigned long *capacity_result, struct cpumask *cpumask,
	unsigned int cpu_util_local, unsigned int coef1_util_local, unsigned int coef2_util_local, unsigned long min, unsigned long max)
{
	unsigned long orig_util, util, coefs[2];

	if (!is_dpt_v2_support())
		return;

	if (!dpt_v2_util2cap_needed_local_hook || !dpt_v2_linear_local_cap2freq_hook)
		return;

	orig_util = dpt_v2_util2cap_needed_local_hook(cpu_util_local, coef1_util_local, coef2_util_local, coefs);
	util = clamp_val(orig_util, min, max);

	cpu_util_local = cpu_util_local == 0 ? 1 : cpu_util_local;

	*capacity_result = util;

	if (data)
		util = mtk_effective_cpu_util_with_margin(util, cpu, cpumask, 3);
	else
		util = mtk_effective_cpu_util_with_margin(util, cpu, NULL, 3);

	*next_freq = dpt_v2_linear_local_cap2freq_hook(cpu, false, util, 0, 1024, false);
	if (trace_sugov_ext_mtk_map_util_freq_dpt_v2_enabled())
		trace_sugov_ext_mtk_map_util_freq_dpt_v2(cpu, *next_freq, util, orig_util, cpu_util_local,
			coef1_util_local, coef2_util_local, min, max, coefs);

	if (data != NULL) {
		struct sugov_policy *sg_policy = (struct sugov_policy *)data;
		struct cpufreq_policy *policy = sg_policy->policy;

		WRITE_ONCE(policy->cached_target_freq, *next_freq);
		policy->cached_resolved_idx = pd_X2Y(cpu, *next_freq, FREQ, OPP, true, DPT_CALL_MTK_MAP_UTIL_FREQ);
		sg_policy->cached_raw_freq = *next_freq;
	}
}
EXPORT_SYMBOL_GPL(mtk_map_util_freq_dpt_v2);

/* modified from kmainline map_util_freq() */
void mtk_map_util_freq(void *data, unsigned long util, struct cpumask *cpumask,
		unsigned long *next_freq, unsigned long min, unsigned long max) {
	unsigned int first_cpu;
	struct sugov_policy *sg_policy;
	struct cpufreq_policy *policy;

	if (!cpumask)
		return;

	first_cpu = cpumask_first(cpumask);

	*next_freq = pd_X2Y(first_cpu, util, CAP, FREQ, false, DPT_CALL_MTK_MAP_UTIL_FREQ);

	if (trace_sugov_ext_util_freq_enabled())
		trace_sugov_ext_util_freq(first_cpu, util, *next_freq);

	if (data != NULL) {
		sg_policy = (struct sugov_policy *)data;
		policy = sg_policy->policy;

		policy->cached_target_freq = *next_freq;
		policy->cached_resolved_idx = pd_X2Y(first_cpu, *next_freq, FREQ, OPP, true,
				DPT_CALL_MTK_MAP_UTIL_FREQ);
		sg_policy->cached_raw_freq = *next_freq;
	}
}
EXPORT_SYMBOL_GPL(mtk_map_util_freq);
#endif
#else

static int init_opp_cap_info(struct proc_dir_entry *dir) { return 0; }

#endif

#define lsub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	*ptr -= min_t(typeof(*ptr), *ptr, _val);		\
} while (0)

unsigned long mtk_cpu_util_next_dpt_v2(int cpu, struct task_struct *p, int dst_cpu, int boost, unsigned long *cpu_util, unsigned long *coef1_util, unsigned long *coef2_util)
{
	dpt_rq_t *dpt_rq = &per_cpu(__dpt_rq, cpu);
	struct dpt_task_struct *util_cfs = &dpt_rq->util_cfs;
	unsigned long __cpu_util = cpu_util_cfs_dpt_v2(cpu_rq(cpu), CPU_UTIL);
	unsigned long __coef1_util = cpu_util_cfs_dpt_v2(cpu_rq(cpu), COEF1_UTIL);
	unsigned long __coef2_util = cpu_util_cfs_dpt_v2(cpu_rq(cpu), COEF2_UTIL);
	unsigned long local_util_cpu_avg, local_util_coef1_avg, local_util_coef2_avg;
	unsigned int local_util_cpu_est = 0, local_util_coef1_est = 0, local_util_coef2_est = 0;
	/* unsigned long runnable; */

	/* if (is_runnable_boost_enable() && boost) {
	 * 	runnable = READ_ONCE(cfs_rq->avg.runnable_avg);
	 * 	util = max(util, runnable);
	 * }
	 */
	if (p)
		task_global_to_local_dpt_v2(cpu, p, &local_util_cpu_avg, &local_util_coef1_avg, &local_util_coef2_avg, &local_util_cpu_est, &local_util_coef1_est, &local_util_coef2_est);

	if (p && task_cpu(p) == cpu && dst_cpu != cpu) {
		lsub_positive(&__cpu_util, local_util_cpu_avg);
		lsub_positive(&__coef1_util, local_util_coef1_avg);
		lsub_positive(&__coef2_util, local_util_coef2_avg);
	}
	else if (p && task_cpu(p) != cpu && dst_cpu == cpu) {
		__cpu_util += local_util_cpu_avg;
		__coef1_util += local_util_coef1_avg;
		__coef2_util += local_util_coef2_avg;
	}

	*cpu_util = __cpu_util;
	*coef1_util = __coef1_util;
	*coef2_util = __coef2_util;

	if (sched_feat(UTIL_EST) && is_util_est_enable()) {
		unsigned long cpu_util_est, coef1_util_est, coef2_util_est;

		cpu_util_est = READ_ONCE(util_cfs->util_cpu_est) & UTIL_EST_MASK;
		coef1_util_est = READ_ONCE(util_cfs->util_coef1_est) & UTIL_EST_MASK;
		coef2_util_est = READ_ONCE(util_cfs->util_coef2_est) & UTIL_EST_MASK;

		if (dst_cpu == cpu) {
			cpu_util_est += local_util_cpu_est;
			coef1_util_est += local_util_coef1_est;
			coef2_util_est += local_util_coef2_est;
		}
		else if (p && unlikely(task_on_rq_queued(p) || current == p)) {
			lsub_positive(&cpu_util_est, local_util_cpu_est);
			lsub_positive(&coef1_util_est, local_util_coef1_est);
			lsub_positive(&coef2_util_est, local_util_coef2_est);
		}

		*cpu_util = max(__cpu_util, cpu_util_est);
		*coef1_util = max(__coef1_util, coef1_util_est);
		*coef2_util = max(__coef2_util, coef2_util_est);
	}

	/* if (trace_sched_runnable_boost_enabled())
	 * 	trace_sched_runnable_boost(is_runnable_boost_enable(), boost, cfs_rq->avg.util_avg,
	 * 			cfs_rq->avg.util_est, runnable, util);
     */
	return min(*cpu_util + *coef1_util + *coef2_util, DPT_V2_MAX_RUNNING_TIME);
}
EXPORT_SYMBOL_GPL(mtk_cpu_util_next_dpt_v2);

unsigned long mtk_cpu_util_cfs_dpt_v2(int cpu, unsigned long *cpu_util, unsigned long *coef1_util, unsigned long *coef2_util)
{
	return mtk_cpu_util_next_dpt_v2(cpu, NULL, -1, 0, cpu_util, coef1_util, coef2_util);
}
EXPORT_SYMBOL_GPL(mtk_cpu_util_cfs_dpt_v2);

/* cloned from k66 cpu_util_cfs_boost() */
unsigned long mtk_cpu_util_cfs_boost_dpt_v2(int cpu, unsigned long *cpu_util, unsigned long *coef1_util, unsigned long *coef2_util)
{
	return mtk_cpu_util_next_dpt_v2(cpu, NULL, -1, 1, cpu_util, coef1_util, coef2_util);
}
EXPORT_SYMBOL_GPL(mtk_cpu_util_cfs_boost_dpt_v2);

void topology_set_non_s_scale_dpt_v2(int cpu, unsigned int non_sratio)
{
	per_cpu(__dpt_rq, cpu).arch_s_scale[NON_S] = scale_ratio(non_sratio, 100);
}
EXPORT_SYMBOL_GPL(topology_set_non_s_scale_dpt_v2);

void topology_set_coef1_s_scale_dpt_v2(int cpu, unsigned int sratio)
{
	per_cpu(__dpt_rq, cpu).arch_s_scale[S_COEF1] = scale_ratio(sratio, 100);
}
EXPORT_SYMBOL_GPL(topology_set_coef1_s_scale_dpt_v2);

void topology_set_coef1_ltime_scale_dpt_v2(int cpu, unsigned int min_ltime, unsigned int cur_ltime)
{
	per_cpu(__dpt_rq, cpu).arch_ltime_scale[S_COEF1] = scale_ratio(min_ltime, cur_ltime);
}
EXPORT_SYMBOL_GPL(topology_set_coef1_ltime_scale_dpt_v2);


void topology_set_coef2_s_scale_dpt_v2(int cpu, unsigned int sratio)
{
	per_cpu(__dpt_rq, cpu).arch_s_scale[S_COEF2] = scale_ratio(sratio, 100);
}
EXPORT_SYMBOL_GPL(topology_set_coef2_s_scale_dpt_v2);

void topology_set_coef2_ltime_scale_dpt_v2(int cpu, unsigned int min_ltime, unsigned int cur_ltime)
{
	per_cpu(__dpt_rq, cpu).arch_ltime_scale[S_COEF2] = scale_ratio(min_ltime, cur_ltime);
}
EXPORT_SYMBOL_GPL(topology_set_coef2_ltime_scale_dpt_v2);

void task_global_to_local_dpt_v2(int cpu, struct task_struct *p,
	unsigned long *local_util_cpu_avg, unsigned long *local_util_coef1_avg, unsigned long *local_util_coef2_avg,
	unsigned int *local_util_cpu_est, unsigned int *local_util_coef1_est, unsigned int *local_util_coef2_est)
{
	int cpu_scaling_factor, coef1_scaling_factor, coef2_scaling_factor;
	int gear;

	if (!p)
		return;

	cpu_scaling_factor = coef1_scaling_factor = coef2_scaling_factor = 1024;

	if (!p) {
		pr_info("cpu=%d task is null when switch global to local\n", cpu);
		return;
	}

	for (gear = 0; gear < get_nr_gears() - 1; gear++)
	{
		if (cpumask_test_cpu(cpu, get_gear_cpumask(gear)))
		{
			const int covert_type = get_scaling_factor_convert_type(gear);

			SET_VALUE(cpu_scaling_factor, SCALING_FACTOR_MIN, SCALING_FACTOR_MAX,
				task_inv_scaling_dpt_v2(p, covert_type, CPU_COMPUTING_CYCLE_SCALING));
			SET_VALUE(coef1_scaling_factor, SCALING_FACTOR_MIN, SCALING_FACTOR_MAX,
				task_scaling_dpt_v2(p, covert_type, COEF1_S_SCALING));
			SET_VALUE(coef2_scaling_factor, SCALING_FACTOR_MIN, SCALING_FACTOR_MAX,
				task_scaling_dpt_v2(p, covert_type, COEF2_S_SCALING));
			break;
		}
	}

	if (local_util_cpu_avg)
		*local_util_cpu_avg = (task_util_dpt_v2(p, CPU_UTIL) * cpu_scaling_factor) >> scaling_factor_shift_bit;
	if (local_util_coef1_avg)
		*local_util_coef1_avg = (task_util_dpt_v2(p, COEF1_UTIL) * coef1_scaling_factor) >> scaling_factor_shift_bit;
	if (local_util_coef2_avg)
		*local_util_coef2_avg = (task_util_dpt_v2(p, COEF2_UTIL) * coef2_scaling_factor) >> scaling_factor_shift_bit;
	if (local_util_cpu_est)
		*local_util_cpu_est = (_task_util_est_dpt_v2(p, CPU_UTIL) * cpu_scaling_factor) >> scaling_factor_shift_bit;
	if (local_util_coef1_est)
		*local_util_coef1_est = (_task_util_est_dpt_v2(p, COEF1_UTIL) * coef1_scaling_factor) >> scaling_factor_shift_bit;
	if (local_util_coef2_est)
		*local_util_coef2_est = (_task_util_est_dpt_v2(p, COEF2_UTIL) * coef2_scaling_factor) >> scaling_factor_shift_bit;
}
EXPORT_SYMBOL_GPL(task_global_to_local_dpt_v2);
