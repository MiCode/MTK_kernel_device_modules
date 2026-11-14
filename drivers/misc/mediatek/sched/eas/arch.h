/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include "sugov/cpufreq.h"
#include "sugov/dsu_interface.h"
#include "sugov/sched_version_ctrl.h"
#include "eas_trace.h"
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
#include <thermal_interface.h>
#endif

void (*init_swpm_pwr_coef_by_gear_hook) (int mode, unsigned int gear_id);
EXPORT_SYMBOL(init_swpm_pwr_coef_by_gear_hook);
void (*set_swpm_pwr_coef_hook) (unsigned int gear_id, unsigned int *vals, unsigned int vals_size);
EXPORT_SYMBOL(set_swpm_pwr_coef_hook);
void (*get_swpm_pwr_coef_hook) (unsigned int gear_id, unsigned int *vals, unsigned int vals_size);
EXPORT_SYMBOL(get_swpm_pwr_coef_hook);

unsigned long (*get_freq_volt_hook)(int cpu, unsigned long freq, int quant, int wl);
EXPORT_SYMBOL(get_freq_volt_hook);

unsigned long pd_get_freq_volt(int cpu, unsigned long freq, int quant, int wl)
{
	if (get_freq_volt_hook)
		return get_freq_volt_hook(cpu, freq, quant, wl);

	return 0;
}

unsigned long (*get_cpu_power_hook)(unsigned int mtk_em, unsigned int get_lkg,
	int quant, int wl, int *dpt_pwr_eff_val, int *val_s, int val_m, int r_o,
	int this_cpu, int *cpu_temp, int opp, unsigned int cpumask_val,
	unsigned long *data, unsigned long *output, int *swpm_vars,
	unsigned long cpu_util_local, unsigned long total_util_local, int IPC_scaling_factor);
EXPORT_SYMBOL(get_cpu_power_hook);

unsigned long get_cpu_power(int pid, unsigned int mtk_em, unsigned int get_lkg,
	int quant, int wl, int *val_s, int r_o, int caller,
	int this_cpu, int *cpu_temp, int opp, unsigned int cpumask_val,
	unsigned long *data, unsigned long *output, struct em_perf_domain *pd,
	unsigned long freq, unsigned long max_util,
	int dpt_v2_swpm_support, unsigned int dpt_v2_sratio, dpt_v2_cap_params_struct dpt_v2_cap_params)
{
	unsigned long sum_util;
	struct em_perf_state *ps;
	int i;

	if (get_cpu_power_hook) {
		unsigned long result;
		int dpt_pwr_eff_val[6];
		int swpm_vars[11] = {0};

		swpm_vars[7] = dpt_v2_swpm_support;
		swpm_vars[8] = dpt_v2_sratio;
		swpm_vars[10] = data[4];

		if (dpt_v2_swpm_support == 2 && get_cpu_power_scaling_factor_hook) {
			get_cpu_power_scaling_factor_hook(this_cpu,
				&per_cpu(__dpt_rq, this_cpu).util_cfs.power_scaling_factor);
			swpm_vars[2] = per_cpu(__dpt_rq, this_cpu).util_cfs.power_scaling_factor;
		}

		result = get_cpu_power_hook(mtk_em, get_lkg, quant, wl,
				dpt_pwr_eff_val, val_s, val_m, r_o,
				this_cpu, cpu_temp, opp, cpumask_val, data, output, swpm_vars,
				dpt_v2_cap_params.cpu_util_local, dpt_v2_cap_params.total_util_local, dpt_v2_cap_params.IPC_scaling_factor);

		if (trace_sched_dptv2_swpm_enabled())
			trace_sched_dptv2_swpm(this_cpu, pid, swpm_vars);

		record_sched_pd_opp2pwr_eff(this_cpu, opp, quant, wl,
			dpt_pwr_eff_val[0], dpt_pwr_eff_val[1], dpt_pwr_eff_val[2],
			dpt_pwr_eff_val[3], dpt_pwr_eff_val[4], dpt_pwr_eff_val[5], val_s, r_o, caller);

		return result;
	}

	sum_util = data[0];
	i = em_pd_get_efficient_state(pd->em_table->state, pd, max_util);
	ps = &pd->em_table->state[i];
	return ps->cost * sum_util;
}

unsigned long (*get_cpu_pwr_eff_hook)(int cpu, unsigned long pd_freq, int quant, int wl,
	unsigned long  *dpt_opp_val, int *dpt_pwr_eff_val, int *val_s, int val_m, int r_o,
	int temperature, unsigned long extern_volt, unsigned long *output,
	int dpt_v2_support, dpt_v2_cap_params_struct dpt_v2_cap_params);
EXPORT_SYMBOL(get_cpu_pwr_eff_hook);

unsigned long get_cpu_pwr_eff(int cpu, unsigned long pd_freq,
	int quant, int wl, int *val_s, int r_o, int caller,
	int temperature, unsigned long extern_volt, unsigned long *output,
	int dpt_v2_support, dpt_v2_cap_params_struct dpt_v2_cap_params)
{
	if (get_cpu_pwr_eff_hook) {
		unsigned long result;
		unsigned long dpt_opp_val[3];
		int dpt_pwr_eff_val[6];

		result = get_cpu_pwr_eff_hook(cpu, pd_freq, quant, wl,
				dpt_opp_val, dpt_pwr_eff_val, val_s, val_m, r_o,
				temperature, extern_volt, output, dpt_v2_support, dpt_v2_cap_params);

		record_sched_pd_opp2cap(cpu, output[0], quant, wl,
			dpt_opp_val[0], dpt_opp_val[1], dpt_opp_val[2], val_s, r_o, caller);

		record_sched_pd_opp2pwr_eff(cpu, output[0], quant, wl,
			dpt_pwr_eff_val[0], dpt_pwr_eff_val[1], dpt_pwr_eff_val[2],
			dpt_pwr_eff_val[3], dpt_pwr_eff_val[4], dpt_pwr_eff_val[5], val_s, r_o, caller);

		return result;
	}

	return 0;
}

int (*dsu_freq_changed_hook)(void *private);
EXPORT_SYMBOL(dsu_freq_changed_hook);

int dsu_freq_changed(void *private)
{
	if (dsu_freq_changed_hook)
		return dsu_freq_changed_hook(private);

	return 0;
}

void (*eenv_dsu_init_hook)(void *private, int quant, unsigned int wl,
		int PERCORE_L3_BW, unsigned int cpumask_val,
		void __iomem *base, void __iomem *dsu_ctrl_base, unsigned long *pd_base_freq,
		unsigned int dsu_ceiling_freq, int dsu_temp, unsigned int *dsu_fg_data,
		unsigned int *val, unsigned int *output);
EXPORT_SYMBOL(eenv_dsu_init_hook);

void eenv_dsu_init(void *private, int quant, unsigned int wl,
		int PERCORE_L3_BW, unsigned int cpumask_val, unsigned long *pd_base_freq,
		unsigned int *val, unsigned int *output)
{
	if (eenv_dsu_init_hook) {
		int dsu_temp = 0;
		unsigned int dsu_freq_thermal = 0;
		unsigned int dsu_fg_data[MAX_NR_CPUS+2] = {0};
		unsigned int cpu;

#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
		dsu_temp = get_dsu_temp()/1000;
		dsu_freq_thermal = get_dsu_ceiling_freq();
#endif

		dsu_fg_data[0] = get_dsu_fine_ctrl_enable();
		dsu_fg_data[1] = get_dsu_fine_ctrl();
		if (dsu_fg_data[0]!=0 && dsu_fg_data[1] !=0) {
			for_each_cpu(cpu, cpu_active_mask)
				dsu_fg_data[cpu + 2] = get_fine_value_pct_cpu(cpu);
		}

		(*eenv_dsu_init_hook)(private, quant, wl,
			PERCORE_L3_BW, cpumask_val,
			get_l3ctl_sram_base_addr(), get_cdsu_sram_base_addr(), pd_base_freq,
			dsu_freq_thermal, dsu_temp, dsu_fg_data,
			val, output);
	}
}

unsigned long (*update_dsu_status_hook)(void *private, int quant, unsigned int wl,
		unsigned int gear_idx, unsigned long freq,
		int this_cpu, int dst_cpu, unsigned int *output);
EXPORT_SYMBOL(update_dsu_status_hook);

unsigned long update_dsu_status(struct energy_env *eenv, int quant,
		unsigned long freq, int this_cpu, int dst_cpu)
{
	if (update_dsu_status_hook) {
		unsigned long dsu_volt;
		unsigned int output[9];

		dsu_volt = update_dsu_status_hook(eenv->android_vendor_data1, quant,
			eenv->wl_dsu, eenv->gear_idx, freq, this_cpu, dst_cpu, output);

		if (trace_sched_dsu_freq_enabled())
			trace_sched_dsu_freq(eenv->gear_idx, dst_cpu, output[0], output[1],
					output[2], output[3], (unsigned long)output[4],
					(unsigned long)output[5], dsu_volt, output[6], output[7],
					output[8]);

		return dsu_volt;
	}

	return 0;
}

bool PERCORE_L3_BW;

void init_percore_l3_bw(void)
{
	PERCORE_L3_BW = sched_percore_l3_bw_get();
}

unsigned long (*mtk_get_dsu_pwr_hook)(int wl, int dst_cpu, unsigned long task_util,
		unsigned long total_util, void *private, unsigned int extern_volt,
		int dsu_pwr_enable, int PERCORE_L3_BW, void __iomem *base, int *data);
EXPORT_SYMBOL(mtk_get_dsu_pwr_hook);
unsigned long get_dsu_pwr(int wl, int dst_cpu, unsigned long task_util,
		unsigned long total_util, void *private, unsigned int extern_volt,
		int dsu_pwr_enable)
{
	if (mtk_get_dsu_pwr_hook) {
		unsigned long ret;
		int data[13] = {[0 ... 12] = 6666};

		ret = mtk_get_dsu_pwr_hook(wl, dst_cpu, task_util,
			total_util, private, extern_volt, dsu_pwr_enable,
			(int) PERCORE_L3_BW, get_clkg_sram_base_addr(), &data[0]);

		if (trace_dsu_pwr_cal_enabled()) {
			trace_dsu_pwr_cal(dst_cpu, task_util, total_util, data, extern_volt);
		}

	return ret;
	}
	return 0;
}
