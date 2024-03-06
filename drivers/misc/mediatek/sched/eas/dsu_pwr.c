// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/cpufreq.h>
#include <linux/kthread.h>
#include <linux/irq_work.h>
#include "sugov/cpufreq.h"
#include "sugov/dsu_interface.h"
#include "sugov/sched_version_ctrl.h"
#include "dsu_pwr.h"
#include "sched_trace.h"

bool PERCORE_L3_BW;

void init_percore_l3_bw(void)
{
	PERCORE_L3_BW = sched_percore_l3_bw_get();
}

/* bml weighting and the predict way for dsu and emi may different */
unsigned int predict_dsu_bw(int wl, int dst_cpu, unsigned long task_util,
		unsigned long total_util, struct dsu_info *p)
{
	unsigned int bml_weighting;
	unsigned int ret = 0;
	unsigned int dsu_bw;
	unsigned long total_util_orig;

	total_util_orig = total_util;
	total_util = max_t(unsigned long, total_util_orig, 1);
	if (PERCORE_L3_BW) {
		dsu_bw = max_t(unsigned int, p->per_core_dsu_bw[dst_cpu], 1);
		ret = dsu_bw * task_util / total_util;
	} else {
		/* pd_get_dsu_weighting return percentage */
		bml_weighting = pd_get_dsu_weighting(wl, dst_cpu);
		dsu_bw = max_t(unsigned int, p->dsu_bw, 1);
		ret = p->dsu_bw * bml_weighting * task_util / total_util / 100;
	}
	ret += p->dsu_bw;

	return ret;
}

unsigned int predict_emi_bw(int wl, int dst_cpu, unsigned long task_util,
		unsigned long total_util, unsigned int emi_bw)
{
	unsigned int bml_weighting;
	unsigned int ret = 0;

	bml_weighting = pd_get_emi_weighting(wl, dst_cpu);
	ret = emi_bw * bml_weighting * task_util / total_util / 100;
	ret += emi_bw;

	return ret;
}

unsigned int dsu_dyn_pwr(int wl, struct dsu_info *p, unsigned int p_dsu_bw,
		unsigned int extern_volt)
{
	int dsu_opp = 0;
	int real_co_point;
	unsigned int golden_bw_pwr, real_bw_pwr, real_bw, old_bw;
	int pwr_delta;
	struct dsu_state *dsu_tbl;
	int perbw_pwr_a, perbw_pwr_b;
	unsigned int ret = 0;
	unsigned int volt_temp, shared_volt;

	dsu_opp = dsu_get_freq_opp(p->dsu_freq);
	real_bw = p_dsu_bw;
	dsu_tbl = dsu_get_opp_ps(wl, dsu_opp);
	old_bw = dsu_tbl->BW;
	real_co_point = CO_POINT * p->dsu_freq / 100000;/* to 100mb/s */

#ifdef SEPA_DSU_EMI
	perbw_pwr_a = L3_PERBW_PWR_A;
	perbw_pwr_b = L3_PERBW_PWR_B;
#else
	perbw_pwr_a = L3_PERBW_PWR_A + EMI_PERBW_PWR * p->emi_bw / p->dsu_bw;
	perbw_pwr_b = L3_PERBW_PWR_B + EMI_PERBW_PWR * p->emi_bw / p->dsu_bw;
#endif

	if (old_bw > real_co_point) {
		golden_bw_pwr = real_co_point * perbw_pwr_a + (old_bw -
				real_co_point) * perbw_pwr_b;
	} else {
		golden_bw_pwr = old_bw * perbw_pwr_a;
	}

	if (real_bw > real_co_point) {
		real_bw_pwr = real_co_point * perbw_pwr_a + (real_bw - real_co_point)
			* perbw_pwr_b;
	} else {
		real_bw_pwr = real_bw * perbw_pwr_a;
	}

	/* mw to uw */
	shared_volt = max(p->dsu_volt, extern_volt);
	volt_temp = 1000 * shared_volt/100000 * shared_volt/100000;
	pwr_delta = volt_temp * (real_bw_pwr - golden_bw_pwr);

	ret = dsu_tbl->dyn_pwr + pwr_delta/10;

	return ret;

}

#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
unsigned int dsu_lkg_pwr(int wl, struct dsu_info *p, unsigned int extern_volt)
{
	int temperature;
	unsigned int coef_ab = 0, coef_a = 0, coef_b = 0, coef_c = 0, type, opp = 0;
	void __iomem *clkg_sram_base_addr;
	unsigned int shared_volt;

	clkg_sram_base_addr = get_clkg_sram_base_addr();
	temperature = p->temp;
	type = DSU_LKG;
	/* volt to opp for calculating offset */
	shared_volt = max(p->dsu_volt, extern_volt);
	opp = pd_dsu_volt2opp(shared_volt);

	/* read coef from sysram, real value = value/10000 */
	coef_ab = ioread32(clkg_sram_base_addr + LKG_BASE_OFFSET +
			type * LKG_TYPES_OFFSET + opp * 8);
	coef_c = ioread32(clkg_sram_base_addr + LKG_BASE_OFFSET +
			type * LKG_TYPES_OFFSET + opp * 8 + 4);
	coef_a = coef_ab & LKG_COEF_A_MASK;
	coef_b = (coef_ab >> LKG_COEF_A_BIT_NUM) & LKG_COEF_B_MASK;

	return (temperature * (temperature * coef_a - coef_b) + coef_c)/10000 *
	   1000;/* uw */
}
#endif

#ifdef SEPA_DSU_EMI
unsigned int mcusys_dyn_pwr(int wl, struct dsu_info *p,
		unsigned int p_emi_bw)
{
	int dsu_opp = 0;
	unsigned int old_bw_pwr, old_bw, new_bw, new_bw_pwr;
	int pwr_delta;
	struct dsu_state *dsu_tbl;
	unsigned int volt_temp;

	dsu_tbl = dsu_get_opp_ps(wl, dsu_opp);
	old_bw = dsu_tbl->EMI_BW;/* 100mb/s */
	new_bw = p_emi_bw;/* 100mb/s */

	old_bw_pwr = EMI_PERBW_PWR * old_bw/10;
	new_bw_pwr = EMI_PERBW_PWR * new_bw/10;
	/* mw to uw */
	volt_temp = 1000 * p->dsu_volt/100000 * p->dsu_volt/100000;
	pwr_delta = volt_temp * (new_bw_pwr - old_bw_pwr);

	return dsu_tbl->mcusys_dyn_pwr + pwr_delta;/* uw */
}
#endif

/* bw : 100 mb/s, temp : degree, freq : khz, volt : 10uv */
unsigned long get_dsu_pwr_(int wl, int dst_cpu, unsigned long task_util,
		unsigned long total_util, void *private, unsigned int extern_volt,
		bool dsu_pwr_enable)
{
	unsigned int dsu_pwr[RES_PWR];
	unsigned int p_dsu_bw, p_emi_bw; /* predict dsu and emi bw */
	int i;
	struct eenv_dsu *eenv = (struct eenv_dsu *) private;
	struct dsu_info *dsu = &eenv->dsu;

	if (!dsu_pwr_enable)
		return 0;

	/* predict task bw */
	if (dst_cpu >= 0) {
		dsu->dsu_freq = eenv->dsu_freq_new;
		dsu->dsu_volt = eenv->dsu_volt_new;

		/* predict dsu bw */
		p_dsu_bw = predict_dsu_bw(wl, dst_cpu, task_util, total_util,
				dsu);
		/* predict emi bw */
		p_emi_bw = predict_emi_bw(wl, dst_cpu, task_util, total_util,
				dsu->emi_bw);
	} else {
		dsu->dsu_freq = eenv->dsu_freq_base;
		dsu->dsu_volt = eenv->dsu_volt_base;

		p_dsu_bw = dsu->dsu_bw;
		p_emi_bw = dsu->emi_bw;
	}

	/*SWPM in uw */
	dsu_pwr[DSU_PWR_TAL] = 0;
	dsu_pwr[DSU_DYN_PWR] = dsu_dyn_pwr(wl, dsu, p_dsu_bw, extern_volt);
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
	dsu_pwr[DSU_LKG_PWR] = dsu_lkg_pwr(wl, dsu, extern_volt);
#else
	dsu_pwr[DSU_LKG_PWR] = 0;
#endif
#ifdef SEPA_DSU_EMI
	dsu_pwr[MCU_DYN_PWR] = mcusys_dyn_pwr(wl, dsu, p_emi_bw);
#else
	dsu_pwr[MCU_DYN_PWR] = 0;
#endif
	for (i = DSU_DYN_PWR; i < DSU_PWR_TAL ; i++)
		dsu_pwr[DSU_PWR_TAL] += dsu_pwr[i];

	if (trace_dsu_pwr_cal_enabled()) {
		trace_dsu_pwr_cal(dst_cpu, task_util, total_util, p_dsu_bw,
				p_emi_bw, dsu->temp, dsu->dsu_freq,
				dsu->dsu_volt, extern_volt,
				dsu_pwr[DSU_DYN_PWR], dsu_pwr[DSU_LKG_PWR],
				dsu_pwr[MCU_DYN_PWR]);
	}

	return dsu_pwr[DSU_PWR_TAL];
}

unsigned long (*mtk_get_dsu_pwr_hook)(int wl, int dst_cpu, unsigned long task_util,
		unsigned long total_util, void *private, unsigned int extern_volt,
		int dsu_pwr_enable, int PERCORE_L3_BW, void __iomem *base, int *data);
EXPORT_SYMBOL(mtk_get_dsu_pwr_hook);
unsigned long get_dsu_pwr(int wl, int dst_cpu, unsigned long task_util,
		unsigned long total_util, void *private, unsigned int extern_volt,
		bool dsu_pwr_enable)
{
	if (mtk_get_dsu_pwr_hook) {
		unsigned long ret;
		int data[8];

		ret = mtk_get_dsu_pwr_hook(wl, dst_cpu, task_util,
			total_util, private, extern_volt, dsu_pwr_enable,
			PERCORE_L3_BW, get_clkg_sram_base_addr(), &data[0]);

		if (trace_dsu_pwr_cal_enabled()) {
			trace_dsu_pwr_cal(dst_cpu, task_util, total_util, data[0],
					data[1], data[2], data[3], data[4], extern_volt,
					data[5], data[6], data[7]);
		}

		return ret;
	}
	return 0;
}
