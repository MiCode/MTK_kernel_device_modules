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

unsigned long (*get_volt_wFloor_Freq_hook)(int cpu, unsigned long freq,
	int quant, int wl, unsigned long floor_freq);
EXPORT_SYMBOL(get_volt_wFloor_Freq_hook);

unsigned long pd_get_volt_wFloor_Freq(int cpu, unsigned long freq,
	int quant, int wl, unsigned long floor_freq)
{
	if (get_volt_wFloor_Freq_hook)
		return get_volt_wFloor_Freq_hook(cpu, freq, quant, wl, floor_freq);

	return 0;
}

unsigned long (*get_cpu_power_hook)(unsigned int mtk_em, unsigned int get_lkg,
	int quant, int wl, int *dpt_pwr_eff_val, int *val_s, int val_m, int r_o,
	int this_cpu, int *cpu_temp, int opp, unsigned int cpumask_val,
	unsigned long *data, unsigned long *output);
EXPORT_SYMBOL(get_cpu_power_hook);

unsigned long get_cpu_power(unsigned int mtk_em, unsigned int get_lkg,
	int quant, int wl, int *val_s, int r_o, int caller,
	int this_cpu, int *cpu_temp, int opp, unsigned int cpumask_val,
	unsigned long *data, unsigned long *output)
{
	if (get_cpu_power_hook) {
		unsigned long result;
		int dpt_pwr_eff_val[5];

		result = get_cpu_power_hook(mtk_em, get_lkg, quant, wl,
				dpt_pwr_eff_val, val_s, val_m, r_o,
				this_cpu, cpu_temp, opp, cpumask_val, data, output);

		record_sched_pd_opp2pwr_eff(this_cpu, opp, quant, wl,
			dpt_pwr_eff_val[0], dpt_pwr_eff_val[1], dpt_pwr_eff_val[2],
			dpt_pwr_eff_val[3], dpt_pwr_eff_val[4], val_s, r_o, caller);

		return result;
	}

	return 0;
}

unsigned long (*get_cpu_pwr_eff_hook)(int cpu, unsigned long pd_freq, int quant, int wl,
	int *dpt_opp_val, int *dpt_pwr_eff_val, int *val_s, int val_m, int r_o,
	unsigned long floor_freq, int temperature, unsigned long extern_volt,
	unsigned long *output);
EXPORT_SYMBOL(get_cpu_pwr_eff_hook);

unsigned long get_cpu_pwr_eff(int cpu, unsigned long pd_freq,
	int quant, int wl, int *val_s, int r_o, int caller,
	unsigned long floor_freq, int temperature, unsigned long extern_volt,
	unsigned long *output)
{
	if (get_cpu_pwr_eff_hook) {
		unsigned long result;
		int dpt_opp_val[3];
		int dpt_pwr_eff_val[5];

		result = get_cpu_pwr_eff_hook(cpu, pd_freq, quant, wl,
				dpt_opp_val, dpt_pwr_eff_val, val_s, val_m, r_o,
				floor_freq, temperature, extern_volt, output);

		record_sched_pd_opp2cap(cpu, output[0], quant, wl,
			dpt_opp_val[0], dpt_opp_val[1], dpt_opp_val[2], val_s, r_o, caller);

		record_sched_pd_opp2pwr_eff(cpu, output[0], quant, wl,
			dpt_pwr_eff_val[0], dpt_pwr_eff_val[1], dpt_pwr_eff_val[2],
			dpt_pwr_eff_val[3], dpt_pwr_eff_val[4], val_s, r_o, caller);

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

void (*eenv_dsu_init_hook)(void *private, unsigned int wl,
		int PERCORE_L3_BW, unsigned int cpumask_val,
		void __iomem *base, unsigned int dsu_target_freq,
		unsigned int dsu_ceiling_freq, int dsu_temp,
		unsigned int *val, unsigned int *output);
EXPORT_SYMBOL(eenv_dsu_init_hook);

void eenv_dsu_init(void *private, unsigned int wl,
		int PERCORE_L3_BW, unsigned int cpumask_val,
		unsigned int *val, unsigned int *output)
{
	if (eenv_dsu_init_hook) {
		static struct cpu_dsu_freq_state *freq_state;
		freq_state = get_dsu_freq_state();

		(*eenv_dsu_init_hook)(private, wl,
			PERCORE_L3_BW, cpumask_val,
			get_l3ctl_sram_base_addr(), freq_state->dsu_target_freq,
			get_dsu_ceiling_freq(), get_dsu_temp(),
			val, output);
	}
}

unsigned long (*update_dsu_status_hook)(void *private, int quant, unsigned int wl,
		unsigned int gear_idx, unsigned long freq, unsigned long floor_freq,
		int this_cpu, int dst_cpu, unsigned int *output);
EXPORT_SYMBOL(update_dsu_status_hook);

unsigned long update_dsu_status(struct energy_env *eenv, int quant,
		unsigned long freq, unsigned long floor_freq, int this_cpu, int dst_cpu)
{
	if (update_dsu_status_hook) {
		unsigned long dsu_volt;
		unsigned int output[4];

		dsu_volt = update_dsu_status_hook(eenv->android_vendor_data1, quant,
			eenv->wl, eenv->gear_idx, freq, floor_freq, this_cpu, dst_cpu, output);

		if (trace_sched_dsu_freq_enabled())
			trace_sched_dsu_freq(eenv->gear_idx, dst_cpu, output[0], output[1],
					(unsigned long)output[2], (unsigned long)output[3], dsu_volt);

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
		int data[8];

		ret = mtk_get_dsu_pwr_hook(wl, dst_cpu, task_util,
			total_util, private, extern_volt, dsu_pwr_enable,
			(int) PERCORE_L3_BW, get_clkg_sram_base_addr(), &data[0]);

		if (trace_dsu_pwr_cal_enabled()) {
			trace_dsu_pwr_cal(dst_cpu, task_util, total_util, data[0],
					data[1], data[2], data[3], data[4], extern_volt,
					data[5], data[6], data[7]);
		}

	return ret;
	}
	return 0;
}
