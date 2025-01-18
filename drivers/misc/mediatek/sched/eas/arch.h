/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include "eas_plus.h"

inline
int pd_get_opp_wFloor_Freq(int cpu, unsigned long freq,
	int wl_type, unsigned long floor_freq)
{
	int opp = -1;

	freq = max(freq, floor_freq);
	opp = pd_freq2opp(cpu, freq, false, wl_type);

	return opp;
}

inline
unsigned long pd_get_volt_wFloor_Freq(int cpu, unsigned long freq,
	int wl_type, unsigned long floor_freq)
{
	int opp = -1;

	opp = pd_get_opp_wFloor_Freq(cpu, freq, wl_type, floor_freq);

	return (unsigned long) pd_opp2volt(cpu, opp, false, wl_type);
}

inline
unsigned long pd_get_dsu_freq_wFloor_Freq(int cpu, unsigned long freq,
	int wl_type, unsigned long floor_freq)
{
	int opp = -1;

	opp = pd_get_opp_wFloor_Freq(cpu, freq, wl_type, floor_freq);

	return (unsigned long) pd_cpu_opp2dsu_freq(cpu, opp, false, wl_type);
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

inline
unsigned long update_dsu_status(struct energy_env *eenv,
	unsigned long freq, unsigned long floor_freq, int this_cpu, int dst_cpu)
{
	unsigned long dsu_freq, dsu_volt;
	unsigned int dsu_opp;
	struct dsu_state *dsu_ps;

	dsu_freq = pd_get_dsu_freq_wFloor_Freq(this_cpu, freq, eenv->wl_type, floor_freq);
	if (dst_cpu >= 0) {
		if (eenv->dsu_freq_base < dsu_freq) {
			eenv->dsu_freq_new = dsu_freq;
			dsu_opp = dsu_get_freq_opp(eenv->dsu_freq_new);
			dsu_ps = dsu_get_opp_ps(eenv->wl_type, dsu_opp);
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

	if (trace_sched_dsu_freq_enabled())
		trace_sched_dsu_freq(eenv->gear_idx, dst_cpu,
			eenv->dsu_freq_new, eenv->dsu_volt_new,
			pd_get_cpufreq_wFloor_Freq(this_cpu, freq, eenv->wl_type, floor_freq),
			dsu_freq, dsu_volt);

	return dsu_volt;
}

inline
unsigned long get_cpu_power(bool dyn_pwr_ctrl, bool lkg_pwr_ctrl, int wl_type,
	int *val_s, int this_cpu, int *cpu_temp, int opp, unsigned long sum_util,
	unsigned long extern_volt, unsigned long cap, unsigned int cpumask_val,
	unsigned long *dyn_pwr, unsigned long *static_pwr, unsigned long *pwr_eff,
	unsigned long *sum_cap, unsigned int *cpu_static_pwr_array,
	unsigned long scale_cpu, unsigned long freq)
{
	unsigned int pd_volt = 0;
	int cpu = 0;

	if (lkg_pwr_ctrl) {
		while (cpumask_val) {
			if (cpumask_val & 1) {
				unsigned int cpu_static_pwr;

				cpu_static_pwr = shared_buck_lkg_pwr(wl_type, cpu, opp,
						cpu_temp[cpu], extern_volt);
				*static_pwr += cpu_static_pwr;
				*sum_cap += cap;
				cpu_static_pwr_array[cpu] = cpu_static_pwr;
			}
			cpumask_val >>= 1;
			cpu ++;
		}
		*static_pwr = (likely(*sum_cap) ? (*static_pwr * sum_util) / *sum_cap : 0);
	}

	if (dyn_pwr_ctrl) {
		*pwr_eff = pd_opp2pwr_eff(this_cpu, opp, false, wl_type, val_s,
				false, DPT_CALL_MTK_EM_CPU_ENERGY);
		*dyn_pwr = *pwr_eff * sum_util;

		/* shared-buck dynamic power*/
		pd_volt = pd_opp2volt(this_cpu, opp, false, wl_type);
		*dyn_pwr = shared_buck_dyn_pwr(*dyn_pwr, pd_volt, extern_volt);
	} else
		*dyn_pwr = (*pwr_eff * sum_util / scale_cpu);

	return *dyn_pwr + *static_pwr;
}
