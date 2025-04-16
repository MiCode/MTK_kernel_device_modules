// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

/**
 * @file    mtk_gpufreq_core.c
 * @brief   GPU-DVFS Driver Platform Implementation
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>

#include <gpufreq_v2.h>
#include <gpufreq_debug.h>
#include <gpuppm.h>
#include <gpufreq_common.h>
#include <gpufreq_mt6855.h>
#include <gpueb_debug.h>
#include <mtk_gpu_utility.h>

#if 0
#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
#include <mtk_battery_oc_throttling.h>
#endif
#if IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING)
#include <mtk_bp_thl.h>
#endif
#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
#include <mtk_low_battery_throttling.h>
#endif
#if IS_ENABLED(CONFIG_MTK_STATIC_POWER)
#include <leakage_table_v2/mtk_static_power.h>
#endif
#if IS_ENABLED(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING)
#include <clk-mtk.h>
#endif
#endif

/**
 * ===============================================
 * Local Function Declaration
 * ===============================================
 */
/* misc function */
static void __iomem *__gpufreq_of_ioremap(const char *node_name, int idx);
/* bringup function */
static unsigned int __gpufreq_bringup(void);
static void __gpufreq_dump_bringup_status(struct platform_device *pdev);
/* get function */
static unsigned int __gpufreq_get_fmeter_fgpu(void);
static unsigned int __gpufreq_execute_fmeter(enum gpufreq_clk_src clksrc);
static unsigned int __gpufreq_get_real_fgpu(void);
static unsigned int __gpufreq_get_real_vgpu(void);
static unsigned int __gpufreq_get_real_vsram(void);
/* init function */
static int __gpufreq_init_pmic(struct platform_device *pdev);
static int __gpufreq_init_platform_info(struct platform_device *pdev);
static int __gpufreq_pdrv_probe(struct platform_device *pdev);
static void __gpufreq_pdrv_remove(struct platform_device *pdev);

/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */
static const struct of_device_id g_gpufreq_of_match[] = {
	{.compatible = "mediatek,gpufreq"},
	{ /* sentinel */ }
};

static struct platform_driver g_gpufreq_pdrv = {
	.probe = __gpufreq_pdrv_probe,
	.remove = __gpufreq_pdrv_remove,
	.driver = {
		   .name = "gpufreq",
		   .owner = THIS_MODULE,
		   .of_match_table = g_gpufreq_of_match,
		   },
};

static void __iomem *g_mfg_pll_base;
static void __iomem *g_mfg_rpc_base;
static void __iomem *g_sleep;
static void __iomem *g_nth_emicfg_base;
static void __iomem *g_nth_emicfg_ao_mem_base;
static void __iomem *g_nth_emi_mpu_base;
static void __iomem *g_fmem_ao_debug_ctrl;
static void __iomem *g_infracfg_ao_base;
static void __iomem *g_infra_ao_debug_ctrl;
static void __iomem *g_infra_ao1_debug_ctrl;
static void __iomem *g_rgx_base;
static struct gpufreq_pmic_info *g_pmic;
static struct gpufreq_status g_gpu;
static unsigned int g_gpueb_support;
static unsigned int g_aging_load;
static unsigned int g_mcl50_load;
static DEFINE_MUTEX(gpufreq_lock);

static struct gpufreq_platform_fp platform_ap_fp = {

};

static struct gpufreq_platform_fp platform_eb_fp = {
	.dump_external_status = __gpufreq_dump_external_status,
	.get_dyn_pgpu = __gpufreq_get_dyn_pgpu,
};

/**
 * ===============================================
 * External Function Definition
 * ===============================================
 */
/* API: get current working OPP index of GPU */
int __gpufreq_get_cur_idx_gpu(void)
{
	return -1;
}

/* API: get current working OPP index of STACK */
int __gpufreq_get_cur_idx_stack(void)
{
	return -1;
}

/* API: get number of working OPP of GPU */
int __gpufreq_get_opp_num_gpu(void)
{
	return 0;
}

/* API: get number of working OPP of STACK */
int __gpufreq_get_opp_num_stack(void)
{
	return 0;
}

/* API: get working OPP index of GPU via Freq */
int __gpufreq_get_idx_by_fgpu(unsigned int freq)
{
	GPUFREQ_UNREFERENCED(freq);
	return -1;
}

/* API: get working OPP index of STACK via Freq */
int __gpufreq_get_idx_by_fstack(unsigned int freq)
{
	GPUFREQ_UNREFERENCED(freq);
	return -1;
}

/* API: get working OPP index of GPU via Volt */
int __gpufreq_get_idx_by_vgpu(unsigned int volt)
{
	GPUFREQ_UNREFERENCED(volt);
	return -1;
}

/* API: get working OPP index of STACK via Volt */
int __gpufreq_get_idx_by_vstack(unsigned int volt)
{
	GPUFREQ_UNREFERENCED(volt);
	return -1;
}

/* API: get working OPP index of GPU via Power */
int __gpufreq_get_idx_by_pgpu(unsigned int power)
{
	GPUFREQ_UNREFERENCED(power);
	return -1;
}

/* API: get working OPP index of STACK via Power */
int __gpufreq_get_idx_by_pstack(unsigned int power)
{
	GPUFREQ_UNREFERENCED(power);
	return -1;
}

/* API: get dynamic Power of GPU */
unsigned int __gpufreq_get_dyn_pgpu(unsigned int freq, unsigned int volt)
{
	unsigned int p_dynamic = GPU_ACT_REF_POWER;
	unsigned int ref_freq = GPU_ACT_REF_FREQ;
	unsigned int ref_volt = GPU_ACT_REF_VOLT;

	p_dynamic = p_dynamic *
	    ((freq * 100) / ref_freq) *
	    ((volt * 100) / ref_volt) * ((volt * 100) / ref_volt) / (100 * 100 * 100);

	return p_dynamic;
}

/* API: get dynamic Power of STACK */
unsigned int __gpufreq_get_dyn_pstack(unsigned int freq, unsigned int volt)
{
	GPUFREQ_UNREFERENCED(freq);
	GPUFREQ_UNREFERENCED(volt);

	return 0;
}

/*
 * API: commit DVFS to GPU by given OPP index
 * this is the main entrance of generic DVFS
 */
int __gpufreq_generic_commit_gpu(int target_oppidx, enum gpufreq_dvfs_state key)
{
	GPUFREQ_UNREFERENCED(target_oppidx);
	GPUFREQ_UNREFERENCED(key);

	return GPUFREQ_EINVAL;
}

/* API: commit DVFS with only STACK OPP index */
int __gpufreq_generic_commit_stack(int target_oppidx, enum gpufreq_dvfs_state key)
{
	return __gpufreq_generic_commit_dual(target_oppidx, target_oppidx, key);
}

/* API: commit DVFS by given both GPU and STACK OPP index */
int __gpufreq_generic_commit_dual(int target_oppidx_gpu, int target_oppidx_stack,
	enum gpufreq_dvfs_state key)
{
	GPUFREQ_UNREFERENCED(target_oppidx_gpu);
	GPUFREQ_UNREFERENCED(target_oppidx_stack);
	GPUFREQ_UNREFERENCED(key);

	return GPUFREQ_EINVAL;
}

/*
 * API: control power state of whole MFG system
 * return power_count if success
 * return GPUFREQ_EINVAL if failure
 */
int __gpufreq_power_control(enum gpufreq_power_state power)
{
	GPUFREQ_UNREFERENCED(power);

	return 1;
}

void __gpufreq_dump_external_status(char *log_buf, int *log_len, int log_size)
{
	u32 val = 0;

	GPUFREQ_LOGD("== [GPUFREQ INFRA STATUS] ==");
	if (g_gpueb_support) {
		GPUFREQ_LOGD("[Regulator] Vgpu: %d, Vsram: %d",
			     __gpufreq_get_real_vgpu(), __gpufreq_get_real_vsram());
		GPUFREQ_LOGD("[Clk] MFG_PLL: %d", __gpufreq_get_real_fgpu());
	} else {
		GPUFREQ_LOGD("GPU[%d] Freq: %d, Vgpu: %d, Vsram: %d",
			     g_gpu.cur_oppidx, g_gpu.cur_freq, g_gpu.cur_volt, g_gpu.cur_vsram);
	}

	/* 0x13F90000, 0x13000000 */
	if (g_mfg_rpc_base && g_rgx_base) {
		/* MFG_GPU_EB_SPM_RPC_SLP_PROT_EN_STA */
		/* RGX_CR_SYS_BUS_SECURE */
		GPUFREQ_LOGD("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x",
			     "[MFG]",
			     (0x13F90000 + 0x1048), readl(g_mfg_rpc_base + 0x1048),
			     (0x13000000 + 0xA100), readl(g_rgx_base + 0xA100));
	}

	/* 0x1021C000, 0x10270000 */
	if (g_nth_emicfg_base && g_nth_emicfg_ao_mem_base) {
		/* NTH_EMICFG_REG_MFG_EMI1_GALS_SLV_DBG */
		/* NTH_EMICFG_REG_MFG_EMI0_GALS_SLV_DBG */
		/* NTH_EMICFG_AO_MEM_REG_M6M7_IDLE_BIT_EN_1 */
		/* NTH_EMICFG_AO_MEM_REG_M6M7_IDLE_BIT_EN_0 */
		GPUFREQ_LOGD("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
			     "[EMI]",
			     (0x1021C000 + 0x82C), readl(g_nth_emicfg_base + 0x82C),
			     (0x1021C000 + 0x830), readl(g_nth_emicfg_base + 0x830),
			     (0x10270000 + 0x228), readl(g_nth_emicfg_ao_mem_base + 0x228),
			     (0x10270000 + 0x22C), readl(g_nth_emicfg_ao_mem_base + 0x22C));
	}

	/* 0x10351000, 0x10042000 */
	if (g_nth_emi_mpu_base && g_fmem_ao_debug_ctrl) {
		/* SECURERANGE0 */
		/* SECURERANGE0_1 */
		/* SECURERANGE0_2 */
		/* FMEM_AO_BUS_U_DEBUG_CTRL_AO_FMEM_AO_CTRL0 */
#if 0
		GPUFREQ_LOGD("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
			     "[EMI]",
			     (0x10351000 + 0x1D8), readl(g_nth_emi_mpu_base + 0x1D8),
			     (0x10351000 + 0x3D8), readl(g_nth_emi_mpu_base + 0x3D8),
			     (0x10351000 + 0x5D8), readl(g_nth_emi_mpu_base + 0x5D8),
			     (0x10042000 + 0x000), readl(g_fmem_ao_debug_ctrl + 0x000));
#endif
	}

	/* 0x10001000, 0x10023000, 0x1002B000 */
	if (g_infracfg_ao_base && g_infra_ao_debug_ctrl && g_infra_ao1_debug_ctrl) {
		/* MD_MFGSYS_PROTECT_EN_STA_0 */
		/* MD_MFGSYS_PROTECT_RDY_STA_0 */
		/* INFRA_AO_BUS_U_DEBUG_CTRL_AO_INFRA_AO_CTRL0 */
		/* INFRA_QAXI_AO_BUS_SUB1_U_DEBUG_CTRL_AO_INFRA_AO1_CTRL0 */
		GPUFREQ_LOGD("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
			     "[INFRA]",
			     (0x10001000 + 0xCA0), readl(g_infracfg_ao_base + 0xCA0),
			     (0x10001000 + 0xCAC), readl(g_infracfg_ao_base + 0xCAC),
			     (0x10023000 + 0x000), readl(g_infra_ao_debug_ctrl + 0x000),
			     (0x1002B000 + 0x000), readl(g_infra_ao1_debug_ctrl + 0x000));
	}

	/* 0x1C001000 */
	if (g_sleep) {
		/* MFG0_PWR_CON */
		/* MFG1_PWR_CON */
		/* MFG2_PWR_CON */
		GPUFREQ_LOGD("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
			     "[SPM]",
			     (0x1C001000 + 0xEB8), readl(g_sleep + 0xEB8),
			     (0x1C001000 + 0xEBC), readl(g_sleep + 0xEBC),
			     (0x1C001000 + 0xEC0), readl(g_sleep + 0xEC0));
		/* XPU_PWR_STATUS */
		/* XPU_PWR_STATUS_2ND */
		GPUFREQ_LOGD("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x",
			     "[SPM]",
			     (0x1C001000 + PWR_STATUS_OFS), readl(g_sleep + PWR_STATUS_OFS),
			     (0x1C001000 + PWR_STATUS_2ND_OFS),
			     readl(g_sleep + PWR_STATUS_2ND_OFS));
	}
}

/* API: get working OPP index of GPU limited by BATTERY_OC via given level */
int __gpufreq_get_batt_oc_idx(int batt_oc_level)
{
	GPUFREQ_UNREFERENCED(batt_oc_level);
	return -1;
}

/* API: get working OPP index of GPU limited by BATTERY_PERCENT via given level */
int __gpufreq_get_batt_percent_idx(int batt_percent_level)
{
	GPUFREQ_UNREFERENCED(batt_percent_level);
	return -1;
}

/* API: get working OPP index of GPU limited by LOW_BATTERY via given level */
int __gpufreq_get_low_batt_idx(int low_batt_level)
{
	GPUFREQ_UNREFERENCED(low_batt_level);
	return -1;
}

/**
 * ===============================================
 * Internal Function Definition
 * ===============================================
 */
/*
 * API: dump power/clk status when bring-up
 */
static void __gpufreq_dump_bringup_status(struct platform_device *pdev)
{
	struct device *gpufreq_dev = &pdev->dev;
	struct resource *res = NULL;

	if (unlikely(!gpufreq_dev)) {
		GPUFREQ_LOGE("fail to find gpufreq device (ENOENT)");
		goto done;
	}

	/* 0x13FA0000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL");
		goto done;
	}
	g_mfg_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_pll_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL: 0x%llx", res->start);
		goto done;
	}

	/* 0x1C001000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sleep");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource SLEEP");
		goto done;
	}
	g_sleep = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sleep)) {
		GPUFREQ_LOGE("fail to ioremap SLEEP: 0x%llx", res->start);
		goto done;
	}

	/* 0x13F90000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_rpc");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_RPC");
		goto done;
	}
	g_mfg_rpc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_rpc_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_RPC: 0x%llx", res->start);
		goto done;
	}

	/* 0x13000000 */
	g_rgx_base = __gpufreq_of_ioremap("mediatek,rgx", 0);
	if (unlikely(!g_rgx_base)) {
		GPUFREQ_LOGE("fail to ioremap RGX");
		goto done;
	}

	/*
	 * [SPM] pwr_status    : 0x1C001F3C
	 * [SPM] pwr_status_2nd: 0x1C001F40
	 * Power ON: 0000 1110 (0xE) [3:1]: MFG0-2
	 */
	GPUFREQ_LOGD("[GPU] RGX_CR_CORE_ID: (0x%08x, 0x%08x), RGX_CR_SYS_BUS_SECURE: 0x%08x",
		     readl(g_rgx_base + 0x024), readl(g_rgx_base + 0x020),
		     readl(g_rgx_base + 0xA100));
	GPUFREQ_LOGD("[MFG0-2] PWR_STATUS: 0x%08x, PWR_STATUS_2ND: 0x%08x",
		     readl(g_sleep + PWR_STATUS_OFS) & MFG_0_2_PWR_MASK,
		     readl(g_sleep + PWR_STATUS_2ND_OFS) & MFG_0_2_PWR_MASK);
	GPUFREQ_LOGD("[TOP] FMETER: %d, CON1: %d, (FMETER_MAIN: %d, FMETER_SUB: %d)",
		     __gpufreq_get_fmeter_fgpu(), __gpufreq_get_real_fgpu(),
		     __gpufreq_execute_fmeter(CLOCK_MAIN), __gpufreq_execute_fmeter(CLOCK_SUB));
	GPUFREQ_LOGD("[MUX] CKMUX_SEL_REF_CORE: 0x%08x, CKMUX_SEL_REF_PARK: 0x%08x",
		     readl(g_mfg_rpc_base + CLK_MUX_OFS) & CKMUX_SEL_REF_CORE_MASK,
		     readl(g_mfg_rpc_base + CLK_MUX_OFS) & CKMUX_SEL_REF_PARK_MASK);

done:
	return;
}

static unsigned int __gpufreq_get_fmeter_fgpu(void)
{
	unsigned int main_src = 0, park_src = 0;

	/* MFG_RPC_AO_CLK_CFG 0x13F91034 [4] CKMUX_SEL_REF_CORE */
	main_src = readl(g_mfg_rpc_base + CLK_MUX_OFS) & CKMUX_SEL_REF_CORE_MASK;
	/* MFG_RPC_AO_CLK_CFG 0x13F91034 [5] CKMUX_SEL_REF_PARK */
	park_src = readl(g_mfg_rpc_base + CLK_MUX_OFS) & CKMUX_SEL_REF_PARK_MASK;

	if (main_src == CKMUX_SEL_REF_CORE_MASK)
		return __gpufreq_execute_fmeter(CLOCK_MAIN);
	else if (park_src == CKMUX_SEL_REF_PARK_MASK)
		return __gpufreq_execute_fmeter(CLOCK_SUB);
	else
		return 26000;	/* 26MHz */
}

static unsigned int __gpufreq_execute_fmeter(enum gpufreq_clk_src clksrc)
{
	u32 val;
	unsigned int freq = 0;
	int i = 0;

	/* de-asset FMETER reset */
	/* PLL4H_FQMTR_CON0 0x13FA0200 [15] CLK26CALI_0: 1 -> 0 -> 1 */
	writel((readl(PLL4H_FQMTR_CON0) & 0xFFFF7FFF), PLL4H_FQMTR_CON0);
	writel((readl(PLL4H_FQMTR_CON0) | 0x00008000), PLL4H_FQMTR_CON0);

	/* choose target PLL */
	/* PLL4H_FQMTR_CON0 0x13FA0200 [2:0] FQMTR_CKSEL */
	val = readl(PLL4H_FQMTR_CON0) & 0xFFFFFFF8;
	if (clksrc == CLOCK_MAIN)
		val |= FQMTR_PLL1_ID;
	else if (clksrc == CLOCK_SUB)
		val |= FQMTR_PLL4_ID;
	writel(val, PLL4H_FQMTR_CON0);

	/* PLL4H_FQMTR_CON1 0x13FA0204 [25:16] CKGEN_LOAD_CNT = 0x3FF */
	val = (readl(PLL4H_FQMTR_CON1) & 0xFC00FFFF) | (0x3FF << 16);
	writel(val, PLL4H_FQMTR_CON1);

	/* PLL4H_FQMTR_CON0 0x13FA0200 [31:24] CKGEN_K1 = 0x00 */
	writel((readl(PLL4H_FQMTR_CON0) & 0x00FFFFFF), PLL4H_FQMTR_CON0);

	/* enable FMETER */
	/* PLL4H_FQMTR_CON0 0x13FA0200 [12] FMETER_EN = 1'b1 */
	val = (readl(PLL4H_FQMTR_CON0) & 0xFFFFEFFF) | (1UL << 12);
	writel(val, PLL4H_FQMTR_CON0);

	/* trigger FMETER, auto-clear when calibration is done */
	/* PLL4H_FQMTR_CON0 0x13FA0200 [4] CKGEN_TRI_CAL = 1'b1 */
	val = (readl(PLL4H_FQMTR_CON0) & 0xFFFFFFEF) | (1UL << 4);
	writel(val, PLL4H_FQMTR_CON0);

	/* wait FMETER calibration finish */
	while (readl(PLL4H_FQMTR_CON0) & 0x10) {
		udelay(10);
		i++;
		if (i > 100) {
			GPUFREQ_LOGE("wait MFGPLL Fmeter timeout");
			return 0;
		}
	}

	/* read CAL_CNT and CKGEN_LOAD_CNT */
	val = readl(PLL4H_FQMTR_CON1) & 0xFFFF;
	/* Khz */
	freq = ((val * 26000)) / 1024;

	/* reset FMETER */
	writel(0x8000, PLL4H_FQMTR_CON0);

	return freq;
}

/*
 * API: get real current frequency from CON1 (khz)
 * Freq = ((PLL_CON1[21:0] * 26M) / 2^14) / 2^PLL_CON1[26:24]
 */
static unsigned int __gpufreq_get_real_fgpu(void)
{
	unsigned int mfgpll = 0;
	unsigned int posdiv_power = 0;
	unsigned int freq = 0;
	unsigned int pcw = 0;

	mfgpll = readl(MFG_PLL_CON1);

	pcw = mfgpll & (0x3FFFFF);

	posdiv_power = (mfgpll & (0x7 << POSDIV_SHIFT)) >> POSDIV_SHIFT;

	freq = (((pcw * TO_MHZ_TAIL + ROUNDING_VALUE) * MFGPLL_FIN) >> DDS_SHIFT) /
	    (1 << posdiv_power) * TO_MHZ_HEAD;

	return freq;
}

/* API: get real current Vgpu from regulator (mV * 100) */
static unsigned int __gpufreq_get_real_vgpu(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vgpu))
		/* regulator_get_voltage return volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vgpu) / 10;

	return volt;
}

/* API: get real current Vsram from regulator (mV * 100) */
static unsigned int __gpufreq_get_real_vsram(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vsram))
		/* regulator_get_voltage return volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vsram) / 10;

	return volt;
}

static void __iomem *__gpufreq_of_ioremap(const char *node_name, int idx)
{
	struct device_node *node;
	void __iomem *base;

	node = of_find_compatible_node(NULL, NULL, node_name);

	if (node)
		base = of_iomap(node, idx);
	else
		base = NULL;

	return base;
}

static int __gpufreq_init_pmic(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("pdev=0x%x", pdev);

	g_pmic = kzalloc(sizeof(struct gpufreq_pmic_info), GFP_KERNEL);
	if (!g_pmic) {
		GPUFREQ_LOGE("fail to alloc gpufreq_pmic_info (ENOMEM)");
		ret = GPUFREQ_ENOMEM;
		goto done;
	}

	g_pmic->reg_vgpu = regulator_get_optional(&pdev->dev, "_vgpu");
	if (IS_ERR(g_pmic->reg_vgpu)) {
		ret = PTR_ERR(g_pmic->reg_vgpu);
		__gpufreq_abort("fail to get VGPU (%ld)", ret);
		goto done;
	}

	/* VSRAM is co-buck and controlled by SRAMRC, but use regulator to get Volt */
	g_pmic->reg_vsram = regulator_get_optional(&pdev->dev, "_vsram");
	if (IS_ERR(g_pmic->reg_vsram)) {
		ret = PTR_ERR(g_pmic->reg_vsram);
		__gpufreq_abort("fail to get VSRAM (%ld)", ret);
		goto done;
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: init reg base address and flavor config of the platform */
static int __gpufreq_init_platform_info(struct platform_device *pdev)
{
	struct device *gpufreq_dev = &pdev->dev;
	struct device_node *of_wrapper = NULL;
	struct resource *res = NULL;
	int ret = GPUFREQ_ENOENT;

	GPUFREQ_TRACE_START("pdev=0x%x", pdev);

	if (unlikely(!gpufreq_dev)) {
		GPUFREQ_LOGE("fail to find gpufreq device (ENOENT)");
		goto done;
	}

	of_wrapper = of_find_compatible_node(NULL, NULL, "mediatek,gpufreq_wrapper");
	if (unlikely(!of_wrapper)) {
		GPUFREQ_LOGE("fail to find gpufreq_wrapper of_node");
		goto done;
	}

	/* ignore return error and use default value if property doesn't exist */
	of_property_read_u32(gpufreq_dev->of_node, "aging-load", &g_aging_load);
	of_property_read_u32(gpufreq_dev->of_node, "mcl50-load", &g_mcl50_load);
	of_property_read_u32(of_wrapper, "gpueb-support", &g_gpueb_support);

	/* 0x13000000 */
	g_rgx_base = __gpufreq_of_ioremap("mediatek,rgx", 0);
	if (unlikely(!g_rgx_base)) {
		GPUFREQ_LOGE("fail to ioremap RGX");
		goto done;
	}

	/* 0x13FA0000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL");
		goto done;
	}
	g_mfg_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_pll_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL: 0x%llx", res->start);
		goto done;
	}

	/* 0x13F90000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_rpc");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_RPC");
		goto done;
	}
	g_mfg_rpc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_rpc_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_RPC: 0x%llx", res->start);
		goto done;
	}

	/* 0x1C001000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sleep");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource SLEEP");
		goto done;
	}
	g_sleep = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sleep)) {
		GPUFREQ_LOGE("fail to ioremap SLEEP: 0x%llx", res->start);
		goto done;
	}

	/* 0x1021C000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emicfg_reg");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource NTH_EMICFG_REG");
		goto done;
	}
	g_nth_emicfg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_nth_emicfg_base)) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMICFG_REG: 0x%llx", res->start);
		goto done;
	}

	/* 0x10270000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emicfg_ao_mem_reg");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource NTH_EMICFG_AO_MEM_REG");
		goto done;
	}
	g_nth_emicfg_ao_mem_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_nth_emicfg_ao_mem_base)) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMICFG_AO_MEM_REG: 0x%llx", res->start);
		goto done;
	}

	/* 0x10351000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emi_mpu_reg");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource NTH_EMI_MPU_REG");
		goto done;
	}
	g_nth_emi_mpu_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_nth_emi_mpu_base)) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMI_MPU_REG: 0x%llx", res->start);
		goto done;
	}

	/* 0x10042000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "fmem_ao_debug_ctrl");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource FMEM_AO_DEBUG_CTRL");
		goto done;
	}
	g_fmem_ao_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_fmem_ao_debug_ctrl)) {
		GPUFREQ_LOGE("fail to ioremap FMEM_AO_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

	/* 0x10001000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "infracfg_ao");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource INFRACFG_AO");
		goto done;
	}
	g_infracfg_ao_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_infracfg_ao_base)) {
		GPUFREQ_LOGE("fail to ioremap INFRACFG_AO: 0x%llx", res->start);
		goto done;
	}

	/* 0x10023000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "infra_ao_debug_ctrl");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource INFRA_AO_DEBUG_CTRL");
		goto done;
	}
	g_infra_ao_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_infra_ao_debug_ctrl)) {
		GPUFREQ_LOGE("fail to ioremap INFRA_AO_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

	/* 0x1002B000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "infra_ao1_debug_ctrl");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource INFRA_AO1_DEBUG_CTRL");
		goto done;
	}
	g_infra_ao1_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_infra_ao1_debug_ctrl)) {
		GPUFREQ_LOGE("fail to ioremap INFRA_AO1_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

	ret = GPUFREQ_SUCCESS;

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: skip gpufreq driver probe if in bringup state */
static unsigned int __gpufreq_bringup(void)
{
	struct device_node *of_wrapper = NULL;
	unsigned int bringup_state = false;

	of_wrapper = of_find_compatible_node(NULL, NULL, "mediatek,gpufreq_wrapper");
	if (!of_wrapper) {
		GPUFREQ_LOGE("fail to find gpufreq_wrapper of_node, treat as bringup");
		return true;
	}

	/* check bringup state by dts */
	of_property_read_u32(of_wrapper, "gpufreq-bringup", &bringup_state);

	return bringup_state;
}

/* API: gpufreq driver probe */
static int __gpufreq_pdrv_probe(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_LOGI("start to probe gpufreq platform driver");

	/* keep probe successful but do nothing when bringup */
	if (__gpufreq_bringup()) {
		GPUFREQ_LOGI("skip gpufreq platform driver probe when bringup");
		__gpufreq_dump_bringup_status(pdev);
		goto done;
	}

	/* init footprint */
	__gpufreq_reset_footprint();

	/* init reg base address and flavor config of the platform in both AP and EB mode */
	ret = __gpufreq_init_platform_info(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init platform info (%d)", ret);
		goto done;
	}

	/* init pmic regulator */
	ret = __gpufreq_init_pmic(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init pmic (%d)", ret);
		goto done;
	}

	if (!g_gpueb_support) {
		GPUFREQ_LOGE("gpufreq not support AP mode");
		ret = GPUFREQ_EINVAL;
		goto done;
	}

	/*
	 * GPUFREQ PLATFORM INIT DONE
	 * register differnet platform fp to wrapper depending on AP or EB mode
	 */
	gpufreq_register_gpufreq_fp(&platform_eb_fp);

	/* init gpuppm */
	ret = gpuppm_init(TARGET_STACK, g_gpueb_support);
	if (ret) {
		GPUFREQ_LOGE("fail to init gpuppm (%d)", ret);
		goto done;
	}

	GPUFREQ_LOGI("gpufreq platform driver probe done");

done:
	return ret;
}

/* API: gpufreq driver remove */
static void __gpufreq_pdrv_remove(struct platform_device *pdev)
{
	kfree(g_pmic);
}

/* API: register gpufreq platform driver */
static int __init __gpufreq_init(void)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_LOGD("start to init gpufreq platform driver");

	/* register gpufreq platform driver */
	ret = platform_driver_register(&g_gpufreq_pdrv);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to register gpufreq platform driver (%d)", ret);
		goto done;
	}

	GPUFREQ_LOGD("gpufreq platform driver init done");

done:
	return ret;
}

/* API: unregister gpufreq driver */
static void __exit __gpufreq_exit(void)
{
	platform_driver_unregister(&g_gpufreq_pdrv);
}

module_init(__gpufreq_init);
module_exit(__gpufreq_exit);

MODULE_DEVICE_TABLE(of, g_gpufreq_of_match);
MODULE_DESCRIPTION("MediaTek GPU-DVFS platform driver");
MODULE_LICENSE("GPL");
