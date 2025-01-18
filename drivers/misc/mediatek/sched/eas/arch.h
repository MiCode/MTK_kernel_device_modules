/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include "eas_plus.h"

int pd_get_util_opp_wFloor_Freq(struct energy_env *eenv,
	struct cpumask *pd_cpus, unsigned long max_util)
{
	int cpu, opp = -1;
	unsigned long freq;

	cpu = cpumask_first(pd_cpus);
	mtk_map_util_freq(NULL, max_util, pd_get_opp_freq(cpu, 0), pd_cpus,
		&freq, eenv->wl_type);
	freq = max(freq, per_cpu(min_freq, cpu));
	opp = pd_freq2opp(cpu, freq, false, eenv->wl_type);

	return opp;
}

unsigned long pd_get_util_volt_wFloor_Freq(struct energy_env *eenv,
	struct cpumask *pd_cpus, unsigned long max_util)
{
	int cpu, opp = -1;

	cpu = cpumask_first(pd_cpus);
	opp = pd_get_util_opp_wFloor_Freq(eenv, pd_cpus, max_util);

	return (unsigned long) pd_opp2volt(cpu, opp, false, eenv->wl_type);
}

unsigned long pd_get_util_dsu_freq_wFloor_Freq(struct energy_env *eenv,
	struct cpumask *pd_cpus, unsigned long max_util)
{
	int cpu, opp = -1;

	cpu = cpumask_first(pd_cpus);
	opp = pd_get_util_opp_wFloor_Freq(eenv, pd_cpus, max_util);

	return (unsigned long) pd_cpu_opp2dsu_freq(cpu, opp, false, eenv->wl_type);
}

unsigned long pd_get_util_cpufreq_wFloor_Freq(struct energy_env *eenv,
	struct cpumask *pd_cpus, unsigned long max_util)
{
	int cpu, opp = -1;

	cpu = cpumask_first(pd_cpus);
	opp = pd_get_util_opp_wFloor_Freq(eenv, pd_cpus, max_util);

	return (unsigned long) pd_opp2freq(cpu, opp, false, eenv->wl_type);
}

int pd_get_volt_opp(int wl_type, int cpu, int opp, unsigned long extern_volt)
{
	int i;
	unsigned long cpu_volt;

	for (i = opp-1; i >= 0; i--) {
		cpu_volt = (unsigned long) pd_opp2volt(cpu, opp, false, wl_type);

		if (cpu_volt > extern_volt)
			break;
	}

	if (i != 0)
		i = i + 1;

	return i;
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

	i = pd_get_volt_opp(wl_type, cpu, opp, extern_volt);
	static_pwr = pd_get_opp_leakage(cpu, i, temperature);

	return static_pwr;
}

unsigned long shared_buck_dyn_pwr(unsigned long dyn_pwr, unsigned long cpu_volt,
	unsigned long extern_volt)
{
	if (!extern_volt || extern_volt <= cpu_volt)
		return dyn_pwr;

	return (unsigned long long)dyn_pwr * (unsigned long long)extern_volt *
			(unsigned long long)extern_volt / cpu_volt / cpu_volt;
}

unsigned long update_dsu_status(struct energy_env *eenv,
	struct cpumask *pd_cpus, unsigned long max_util, int dst_cpu)
{
	unsigned long dsu_freq, dsu_volt;
	unsigned int dsu_opp;
	struct dsu_state *dsu_ps;

	dsu_freq = pd_get_util_dsu_freq_wFloor_Freq(eenv, pd_cpus, max_util);
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
	} else
		dsu_volt = (unsigned long) eenv->dsu_volt_base;

	if (trace_sched_dsu_freq_enabled()) {
		trace_sched_dsu_freq(eenv->gear_idx, eenv->dsu_freq_new, eenv->dsu_volt_new,
			pd_get_util_cpufreq_wFloor_Freq(eenv, pd_cpus, max_util), dsu_freq,
			dsu_volt);
	}

	return dsu_volt;
}
