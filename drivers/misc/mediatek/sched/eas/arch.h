/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include "eas_plus.h"
#include "sugov/cpufreq.h"
#include "sugov/dsu_interface.h"
#include "eas/dsu_pwr.h"
#include "eas_trace.h"
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
#include <thermal_interface.h>
#endif

int val_m = 1;

inline
int pd_get_opp_wFloor_Freq(int cpu, unsigned long freq,
	int quant, int wl_type, unsigned long floor_freq)
{
	int opp = -1;

	freq = max(freq, floor_freq);
	opp = pd_freq2opp(cpu, freq, quant, wl_type);

	return opp;
}

inline
unsigned long pd_get_volt_wFloor_Freq_(int cpu, unsigned long freq,
	int quant, int wl_type, unsigned long floor_freq)
{
	int opp = -1;

	opp = pd_get_opp_wFloor_Freq(cpu, freq, quant, wl_type, floor_freq);

	return (unsigned long) pd_opp2volt(cpu, opp, quant, wl_type);
}

inline
unsigned long pd_get_dsu_freq_wFloor_Freq(int cpu, unsigned long freq,
	int quant, int wl_type, unsigned long floor_freq)
{
	int opp = -1;

	opp = pd_get_opp_wFloor_Freq(cpu, freq, quant, wl_type, floor_freq);

	return (unsigned long) pd_cpu_opp2dsu_freq(cpu, opp, quant, wl_type);
}

unsigned long pd_get_cpufreq_wFloor_Freq(int cpu, unsigned long freq,
	int wl_type, unsigned long floor_freq)
{
	return max(freq, floor_freq);
}

unsigned long shared_buck_lkg_pwr(int wl_type, int cpu, int opp, int temperature,
	unsigned long extern_volt)
{
	int i;
	unsigned long cpu_volt_orig = 0, static_pwr = 0, static_pwr_orig = 0;

	cpu_volt_orig = pd_opp2volt(cpu, opp, false, wl_type);
	static_pwr_orig = (unsigned long) pd_get_opp_leakage(cpu, opp, temperature);
	if (!extern_volt || extern_volt <= cpu_volt_orig)
		return static_pwr_orig;

	i = pd_cpu_volt2opp(cpu, extern_volt, false, wl_type);
	static_pwr = pd_get_opp_leakage(cpu, i, temperature);

	return static_pwr;
}

unsigned long shared_buck_dyn_pwr(unsigned long dyn_pwr, unsigned long cpu_volt,
	unsigned long extern_volt)
{
	if (!extern_volt || !cpu_volt || extern_volt <= cpu_volt)
		return dyn_pwr;

	return (unsigned long long)dyn_pwr * (unsigned long long)extern_volt *
			(unsigned long long)extern_volt / cpu_volt / cpu_volt;
}

unsigned long mtk_update_dsu_status(void *private, int quant, unsigned int wl,
	unsigned int gear_idx, unsigned long freq, unsigned long floor_freq,
	int this_cpu, int dst_cpu, unsigned int *output)
{
	unsigned long dsu_freq, dsu_volt;
	unsigned int dsu_opp;
	struct dsu_state *dsu_ps;
	struct eenv_dsu *eenv = (struct eenv_dsu *) private;

	dsu_freq = pd_get_dsu_freq_wFloor_Freq(this_cpu, freq, quant, wl, floor_freq);
	if (dst_cpu >= 0) {
		if (eenv->dsu_freq_base < dsu_freq) {
			eenv->dsu_freq_new = dsu_freq;
			dsu_opp = dsu_get_freq_opp(eenv->dsu_freq_new);
			dsu_ps = dsu_get_opp_ps(wl, dsu_opp);
			eenv->dsu_volt_new = dsu_ps->volt;
		} else {
			eenv->dsu_freq_new = eenv->dsu_freq_base;
			eenv->dsu_volt_new = eenv->dsu_volt_base;
		}
		dsu_volt = (unsigned long) eenv->dsu_volt_new;
	} else if (dst_cpu == -2) {
		dsu_volt = (unsigned long) eenv->dsu_volt_new;
	} else {
		dsu_volt = (unsigned long) eenv->dsu_volt_base;
	}

	return dsu_volt;
}

unsigned long update_dsu_status_(struct energy_env *eenv, int quant,
	unsigned long freq, unsigned long floor_freq, int this_cpu, int dst_cpu)
{
	unsigned long dsu_volt;
	unsigned int output[4];

	dsu_volt = mtk_update_dsu_status(eenv->android_vendor_data1, quant,
			eenv->wl_type, eenv->gear_idx, freq, floor_freq, this_cpu, dst_cpu, output);

	if (trace_sched_dsu_freq_enabled())
		trace_sched_dsu_freq(eenv->gear_idx, dst_cpu, output[0], output[1],
				(unsigned long)output[2], (unsigned long)output[3], dsu_volt);

	return dsu_volt;
}

int dsu_freq_changed_(void *private)
{
	struct eenv_dsu *eenv = (struct eenv_dsu *) private;

	return (eenv->dsu_freq_new  > eenv->dsu_freq_base);
}

void eenv_dsu_init_(void *private, unsigned int wl,
	int PERCORE_L3_BW, unsigned int cpumask_val,
	unsigned int *val, unsigned int *output)
{
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
	static struct cpu_dsu_freq_state *freq_state;
#endif
	struct dsu_info *dsu;
	unsigned int dsu_opp;
	struct dsu_state *dsu_ps;
	unsigned int dsu_bw, sum_dsu_bw;
	struct eenv_dsu *eenv = (struct eenv_dsu *) private;
	unsigned int cpu = 0;

	dsu = &(eenv->dsu);
	if (PERCORE_L3_BW) {
		sum_dsu_bw = 0;
		while (cpumask_val) {
			if (cpumask_val & 1) {
				dsu_bw = get_pelt_per_core_dsu_bw(cpu);
				val[cpu] = dsu->per_core_dsu_bw[cpu] = dsu_bw;
				sum_dsu_bw += dsu_bw;
			}
			cpumask_val >>= 1;
			cpu ++;
		}
		dsu->dsu_bw = sum_dsu_bw;
	} else
		dsu->dsu_bw = get_pelt_dsu_bw();

	dsu->emi_bw = get_pelt_emi_bw();
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
	dsu->temp = get_dsu_temp()/1000;
#else
	dsu->temp = 0;
#endif

#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
	eenv->dsu_freq_thermal = get_dsu_ceiling_freq();
	freq_state = get_dsu_freq_state();
	eenv->dsu_freq_base = freq_state->dsu_target_freq;
	dsu_opp = dsu_get_freq_opp(eenv->dsu_freq_base);
	dsu_ps = dsu_get_opp_ps(wl, dsu_opp);
	eenv->dsu_volt_base = dsu_ps->volt;
#endif

	output[0] = dsu->temp;
	output[1] = eenv->dsu_freq_base;
	output[2] = eenv->dsu_volt_base;
	output[3] = eenv->dsu_freq_thermal;
	output[4] = dsu->dsu_bw;
	output[5] = dsu->emi_bw;

}

inline
unsigned long get_cpu_power_(unsigned int mtk_em, unsigned int get_lkg,
	int quant, int wl_type, int *val_s, int r_o, int caller,
	int this_cpu, int *cpu_temp, int opp, unsigned int cpumask_val,
	unsigned long *data, unsigned long *output)
{
	unsigned int pd_volt = 0;
	int cpu = 0;
	unsigned long dyn_pwr, static_pwr = 0, pwr_eff, sum_cap = 0;
	unsigned long sum_util = data[0];
	unsigned long extern_volt = data[1];
	unsigned long cap = data[2];
	unsigned long scale_cpu = data[3];

	if (get_lkg) {
		while (cpumask_val) {
			if (cpumask_val & 1) {
				unsigned int cpu_static_pwr;

				cpu_static_pwr = shared_buck_lkg_pwr(wl_type, cpu, opp,
						cpu_temp[cpu], extern_volt);
				static_pwr += cpu_static_pwr;
				sum_cap += cap;
				output[cpu + 4] = cpu_static_pwr;
			}
			cpumask_val >>= 1;
			cpu ++;
		}
		static_pwr = (likely(sum_cap) ? (static_pwr * sum_util) / sum_cap : 0);
	}

	if (mtk_em) {
		pwr_eff = pd_opp2pwr_eff(this_cpu, opp, false, wl_type, val_s,
				false, DPT_CALL_MTK_EM_CPU_ENERGY);
		dyn_pwr = pwr_eff * sum_util;

		/* shared-buck dynamic power*/
		pd_volt = pd_opp2volt(this_cpu, opp, quant, wl_type);
		dyn_pwr = shared_buck_dyn_pwr(dyn_pwr, pd_volt, extern_volt);
	} else {
		pwr_eff = output[2];
		dyn_pwr = (pwr_eff * sum_util / scale_cpu);
	}

	output[0] = dyn_pwr;
	output[1] = static_pwr;
	output[2] = pwr_eff;
	output[3] = sum_cap;

	return dyn_pwr + static_pwr;
}

inline
unsigned long get_cpu_pwr_eff_(int cpu, unsigned long pd_freq, int quant, int wl_type,
	int *val_s, int r_o, int caller, unsigned long floor_freq, int temperature,
	unsigned long extern_volt, unsigned long *output)
{
	int opp;
	unsigned long static_pwr_eff, pwr_eff;
	int cap;
	int pd_pwr_eff;
	unsigned long pd_volt;

	output[0] = opp = pd_get_opp_wFloor_Freq(cpu, pd_freq, quant, wl_type, floor_freq);

	pd_pwr_eff = pd_opp2pwr_eff(cpu, opp, quant, wl_type, val_s, false, caller);
	pd_volt = pd_opp2volt(cpu, opp, quant, wl_type);
	output[1] = pd_pwr_eff = shared_buck_dyn_pwr(pd_pwr_eff, pd_volt, extern_volt);

	output[2] = cap = pd_opp2cap(cpu, opp, quant, wl_type, val_s, false, caller);
	output[3] = static_pwr_eff = shared_buck_lkg_pwr(wl_type, cpu, opp,
				temperature, extern_volt) / cap;
	pwr_eff = pd_pwr_eff + static_pwr_eff;

	return pwr_eff;
}

unsigned long (*get_volt_wFloor_Freq_hook)(int cpu, unsigned long freq,
	int quant, int wl_type, unsigned long floor_freq);
EXPORT_SYMBOL(get_volt_wFloor_Freq_hook);

unsigned long pd_get_volt_wFloor_Freq(int cpu, unsigned long freq,
	int quant, int wl_type, unsigned long floor_freq)
{
	if (get_volt_wFloor_Freq_hook)
		return get_volt_wFloor_Freq_hook(cpu, freq, quant, wl_type, floor_freq);

	return 0;
}

unsigned long (*get_cpu_power_hook)(unsigned int mtk_em, unsigned int get_lkg,
	int quant, int wl_type, int *dpt_pwr_eff_val, int *val_s, int val_m, int r_o,
	int this_cpu, int *cpu_temp, int opp, unsigned int cpumask_val,
	unsigned long *data, unsigned long *output);
EXPORT_SYMBOL(get_cpu_power_hook);

unsigned long get_cpu_power(unsigned int mtk_em, unsigned int get_lkg,
	int quant, int wl_type, int *val_s, int r_o, int caller,
	int this_cpu, int *cpu_temp, int opp, unsigned int cpumask_val,
	unsigned long *data, unsigned long *output)
{
	if (get_cpu_power_hook) {
		unsigned long result;
		int dpt_pwr_eff_val[5];

		result = get_cpu_power_hook(mtk_em, get_lkg, quant, wl_type,
				dpt_pwr_eff_val, val_s, val_m, r_o,
				this_cpu, cpu_temp, opp, cpumask_val, data, output);

		record_sched_pd_opp2pwr_eff(this_cpu, opp, quant, wl_type,
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
	int quant, int wl_type, int *val_s, int r_o, int caller,
	unsigned long floor_freq, int temperature, unsigned long extern_volt,
	unsigned long *output)
{
	if (get_cpu_pwr_eff_hook) {
		unsigned long result;
		int dpt_opp_val[3];
		int dpt_pwr_eff_val[5];

		result = get_cpu_pwr_eff_hook(cpu, pd_freq, quant, wl_type,
				dpt_opp_val, dpt_pwr_eff_val, val_s, val_m, r_o,
				floor_freq, temperature, extern_volt, output);

		record_sched_pd_opp2cap(cpu, output[0], quant, wl_type,
			dpt_opp_val[0], dpt_opp_val[1], dpt_opp_val[2], val_s, r_o, caller);

		record_sched_pd_opp2pwr_eff(cpu, output[0], quant, wl_type,
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
			eenv->wl_type, eenv->gear_idx, freq, floor_freq, this_cpu, dst_cpu, output);

		if (trace_sched_dsu_freq_enabled())
			trace_sched_dsu_freq(eenv->gear_idx, dst_cpu, output[0], output[1],
					(unsigned long)output[2], (unsigned long)output[3], dsu_volt);

		return dsu_volt;
	}

	return 0;
}
