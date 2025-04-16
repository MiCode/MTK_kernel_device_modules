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
#if IS_ENABLED(CONFIG_MTK_DEVINFO)
#include <linux/nvmem-consumer.h>
#endif

#include <gpufreq_v2.h>
#include <gpufreq_debug.h>
#include <gpuppm.h>
#include <gpufreq_common.h>
#include <gpufreq_mt6895.h>
#include <gpudfd_mt6895.h>
#include <mtk_gpu_utility.h>

#if IS_ENABLED(CONFIG_MTK_STATIC_POWER)
#include <leakage_table_v2/mtk_static_power.h>
#endif

/**
 * ===============================================
 * Local Function Declaration
 * ===============================================
 */
/* misc function */
static void __gpufreq_dump_bringup_status(struct platform_device *pdev);
static void __iomem *__gpufreq_of_ioremap(const char *node_name, int idx);
/* get function */
static unsigned int __gpufreq_get_fmeter_fgpu(void);
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
	{ .compatible = "mediatek,gpufreq" },
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
static void __iomem *g_mfgsc_pll_base;
static void __iomem *g_mfg_rpc_base;
static void __iomem *g_mfg_top_base;
static void __iomem *g_sleep;
static void __iomem *g_nth_emicfg_base;
static void __iomem *g_sth_emicfg_base;
static void __iomem *g_nth_emicfg_ao_mem_base;
static void __iomem *g_sth_emicfg_ao_mem_base;
static void __iomem *g_infracfg_ao_base;
static void __iomem *g_infra_ao_debug_ctrl;
static void __iomem *g_infra_ao1_debug_ctrl;
static void __iomem *g_nth_emi_ao_debug_ctrl;
static void __iomem *g_efuse_base;
static void __iomem *g_mfg_cpe_control_base;
static void __iomem *g_mfg_cpe_sensor_base;
static void __iomem *g_topckgen_base;
static void __iomem *g_mali_base;
static struct gpufreq_pmic_info *g_pmic;
static struct gpufreq_clk_info *g_clk;
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
#if 0
	unsigned int p_dynamic = GPU_ACT_REF_POWER;
	unsigned int ref_freq = GPU_ACT_REF_FREQ;
	unsigned int ref_volt = GPU_ACT_REF_VOLT;

	p_dynamic = p_dynamic *
	    ((freq * 100) / ref_freq) *
	    ((volt * 100) / ref_volt) * ((volt * 100) / ref_volt) / (100 * 100 * 100);

	return p_dynamic;
#endif
	GPUFREQ_UNREFERENCED(freq);
	GPUFREQ_UNREFERENCED(volt);

	return 0;
}

/* API: get dynamic Power of STACK */
unsigned int __gpufreq_get_dyn_pstack(unsigned int freq, unsigned int volt)
{
	GPUFREQ_UNREFERENCED(freq);
	GPUFREQ_UNREFERENCED(volt);

	return 0;
}

int __gpufreq_power_control(enum gpufreq_power_state power)
{
	GPUFREQ_UNREFERENCED(power);

	return 1;
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


//* API: commit DVFS with only STACK OPP index */
int __gpufreq_generic_commit_stack(int target_oppidx, enum gpufreq_dvfs_state key)
{
	return __gpufreq_generic_commit_dual(target_oppidx, target_oppidx, key);
}

/* API: fix OPP of GPU via given OPP index */
int __gpufreq_fix_target_oppidx_gpu(int oppidx)
{
	GPUFREQ_UNREFERENCED(oppidx);

	return GPUFREQ_EINVAL;
}

void __gpufreq_dump_external_status(char *log_buf, int *log_len, int log_size)
{
	u32 val = 0;

	GPUFREQ_LOGI("== [GPUFREQ INFRA STATUS] ==");
	if (g_gpueb_support) {
		GPUFREQ_LOGI("[Regulator] Vcore: %d, Vsram: %d",
			__gpufreq_get_real_vgpu(), __gpufreq_get_real_vsram());
		GPUFREQ_LOGI("[Clk] MFG_PLL: %d, MFGSC_PLL: %d, MFG_SEL_1: 0x%x",
			__gpufreq_get_real_fgpu(),
			readl(g_topckgen_base + 0x1F0) & MFG_SEL_0_MASK,
			readl(g_topckgen_base + 0x1F0) & MFG_SEL_1_MASK);
	} else {
		GPUFREQ_LOGI("GPU[%d] Freq: %d, Vgpu: %d, Vsram: %d",
			g_gpu.cur_oppidx, g_gpu.cur_freq,
			g_gpu.cur_volt, g_gpu.cur_vsram);
	}

	/* 0x13FBF000, 0x13F90000 */
	if (g_mfg_top_base && g_mfg_rpc_base) {
		/* MFG_QCHANNEL_CON 0x13FBF0B4 [0] MFG_ACTIVE_SEL = 1'b1 */
		val = readl(g_mfg_top_base + 0xB4);
		val |= (1UL << 0);
		writel(val, g_mfg_top_base + 0xB4);
		/* MFG_DEBUG_SEL 0x13FBF170 [1:0] MFG_DEBUG_TOP_SEL = 2'b11 */
		val = readl(g_mfg_top_base + 0x170);
		val |= (1UL << 0);
		val |= (1UL << 1);
		writel(val, g_mfg_top_base + 0x170);

		/* MFG_DEBUG_SEL */
		/* MFG_DEBUG_TOP */
		/* MFG_GPU_EB_SPM_RPC_SLP_PROT_EN_STA */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[MFG]",
			(0x13FBF000 + 0x170), readl(g_mfg_top_base + 0x170),
			(0x13FBF000 + 0x178), readl(g_mfg_top_base + 0x178),
			(0x13F90000 + 0x1048), readl(g_mfg_rpc_base + 0x1048));
	}

	/* 0x1021C000, 0x1021E000 */
	if (g_nth_emicfg_base && g_sth_emicfg_base) {
		/* NTH_EMICFG_REG_MFG_EMI1_GALS_SLV_DBG */
		/* NTH_EMICFG_REG_MFG_EMI0_GALS_SLV_DBG */
		/* STH_EMICFG_REG_MFG_EMI1_GALS_SLV_DBG */
		/* STH_EMICFG_REG_MFG_EMI0_GALS_SLV_DBG */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[EMI]",
			(0x1021C000 + 0x82C), readl(g_nth_emicfg_base + 0x82C),
			(0x1021C000 + 0x830), readl(g_nth_emicfg_base + 0x830),
			(0x1021E000 + 0x82C), readl(g_sth_emicfg_base + 0x82C),
			(0x1021E000 + 0x830), readl(g_sth_emicfg_base + 0x830));
	}

	/* 0x10270000, 0x1030E000 */
	if (g_nth_emicfg_ao_mem_base && g_sth_emicfg_ao_mem_base) {
		/* NTH_EMICFG_AO_MEM_REG_M6M7_IDLE_BIT_EN_1 */
		/* NTH_EMICFG_AO_MEM_REG_M6M7_IDLE_BIT_EN_0 */
		/* STH_EMICFG_AO_MEM_REG_M6M7_IDLE_BIT_EN_1 */
		/* STH_EMICFG_AO_MEM_REG_M6M7_IDLE_BIT_EN_0 */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[EMI]",
			(0x10270000 + 0x228), readl(g_nth_emicfg_ao_mem_base + 0x228),
			(0x10270000 + 0x22C), readl(g_nth_emicfg_ao_mem_base + 0x22C),
			(0x1030E000 + 0x228), readl(g_sth_emicfg_ao_mem_base + 0x228),
			(0x1030E000 + 0x22C), readl(g_sth_emicfg_ao_mem_base + 0x22C));
	}

	/* 0x10001000, 0x10023000 */
	if (g_infracfg_ao_base && g_infra_ao_debug_ctrl) {
		/* MD_MFGSYS_PROTECT_EN_STA_0 */
		/* MD_MFGSYS_PROTECT_RDY_STA_0 */
		/* INFRA_AO_BUS_U_DEBUG_CTRL_AO_INFRA_AO_CTRL0 */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[INFRA]",
			(0x10001000 + 0xCA0), readl(g_infracfg_ao_base + 0xCA0),
			(0x10001000 + 0xCAC), readl(g_infracfg_ao_base + 0xCAC),
			(0x10023000 + 0x000), readl(g_infra_ao_debug_ctrl + 0x000));
	}

	/* 0x1002B000, 0x10042000 */
	if (g_infra_ao1_debug_ctrl && g_nth_emi_ao_debug_ctrl) {
		/* INFRA_QAXI_AO_BUS_SUB1_U_DEBUG_CTRL_AO_INFRA_AO1_CTRL0 */
		/* NTH_EMI_AO_DEBUG_CTRL_EMI_AO_BUS_U_DEBUG_CTRL_AO_EMI_AO_CTRL0 */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[INFRA]",
			(0x1002B000 + 0x000), readl(g_infra_ao1_debug_ctrl + 0x000),
			(0x10042000 + 0x000), readl(g_nth_emi_ao_debug_ctrl + 0x000));
	}

	/* 0x1C001000 */
	if (g_sleep) {
		/* MFG0_PWR_CON - MFG3_PWR_CON */
		GPUFREQ_LOGI("%-7s (0x%x-%x): 0x%08x 0x%08x 0x%08x 0x%08x",
			"[SPM]", (0x1C001000 + 0xEB8), 0xEC4,
			readl(g_sleep + 0xEB8), readl(g_sleep + 0xEBC),
			readl(g_sleep + 0xEC0), readl(g_sleep + 0xEC4));
		/* MFG4_PWR_CON - MFG7_PWR_CON */
		GPUFREQ_LOGI("%-7s (0x%x-%x): 0x%08x 0x%08x 0x%08x 0x%08x",
			"[SPM]", (0x1C001000 + 0xEC8), 0xED4,
			readl(g_sleep + 0xEC8), readl(g_sleep + 0xECC),
			readl(g_sleep + 0xED0), readl(g_sleep + 0xED4));
		/* MFG8_PWR_CON - MFG12_PWR_CON */
		GPUFREQ_LOGI("%-7s (0x%x-%x): 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x",
			"[SPM]", (0x1C001000 + 0xED8), 0xEE8,
			readl(g_sleep + 0xED8), readl(g_sleep + 0xEDC),
			readl(g_sleep + 0xEE0), readl(g_sleep + 0xEE4),
			readl(g_sleep + 0xEE8));
		/* XPU_PWR_STATUS */
		/* XPU_PWR_STATUS_2ND */
		GPUFREQ_LOGI("%-7s (0x%x): 0x%08x, (0x%x): 0x%08x",
			"[SPM]",
			(0x1C001000 + PWR_STATUS_OFS), readl(g_sleep + PWR_STATUS_OFS),
			(0x1C001000 + PWR_STATUS_2ND_OFS), readl(g_sleep + PWR_STATUS_2ND_OFS));
	}
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
 * API: dump power/clk status when bring-up
 */
static void __gpufreq_dump_bringup_status(struct platform_device *pdev)
{
	struct device *gpufreq_dev = &pdev->dev;
	struct resource *res = NULL;
	u32 mfg_sel_0 = 0, mfg_ref_sel = 0;
	u32 mfg_sel_1 = 0, mfgsc_ref_sel = 0;

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

	/* 0x13FA0C00 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfgsc_pll");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFGSC_PLL");
		goto done;
	}
	g_mfgsc_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfgsc_pll_base)) {
		GPUFREQ_LOGE("fail to ioremap MFGSC_PLL: 0x%llx", res->start);
		goto done;
	}

	/* 0x13FBF000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_top_config");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_TOP_CONFIG");
		goto done;
	}
	g_mfg_top_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_top_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_TOP_CONFIG: 0x%llx", res->start);
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

	/* 0x10000000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "topckgen");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource TOPCKGEN");
		goto done;
	}
	g_topckgen_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_topckgen_base)) {
		GPUFREQ_LOGE("fail to ioremap TOPCKGEN: 0x%llx", res->start);
		goto done;
	}

	/* 0x13000000 */
	g_mali_base = __gpufreq_of_ioremap("mediatek,mali", 0);
	if (unlikely(!g_mali_base)) {
		GPUFREQ_LOGE("fail to ioremap MALI");
		goto done;
	}

	/* CLK_CFG_30 0x100001F0 [16] MFG_SEL_0 */
	mfg_sel_0 = readl(g_topckgen_base + 0x1F0) & MFG_SEL_0_MASK;
	/* CLK_CFG_30 0x100001F0 [17] MFG_SEL_1 */
	mfg_sel_1 = readl(g_topckgen_base + 0x1F0) & MFG_SEL_1_MASK;
	/* CLK_CFG_4 0x10000050 [25:24] MFG_REF_SEL */
	mfg_ref_sel = readl(g_topckgen_base + 0x50) & MFG_REF_SEL_MASK;
	/* CLK_CFG_5 0x10000060 [1:0] MFGSC_REF_SEL */
	mfgsc_ref_sel = readl(g_topckgen_base + 0x60) & MFGSC_REF_SEL_MASK;
	/*
	 * [SPM] pwr_status    : 0x10006F3C
	 * [SPM] pwr_status_2nd: 0x10006F40
	 * Power ON: 0011 1111 1111 1110 (0x3FFE)
	 * [13:1]: MFG0-12
	 */
	GPUFREQ_LOGI("[GPU]     MALI_ID:    0x%08x, MFG_TOP_CONFIG: 0x%08x",
		readl(g_mali_base), readl(g_mfg_top_base));
	GPUFREQ_LOGI("[MFG0-12] PWR_STATUS: 0x%08x, PWR_STATUS_2ND: 0x%08x",
		readl(g_sleep + PWR_STATUS_OFS) & MFG_0_12_PWR_MASK,
		readl(g_sleep + PWR_STATUS_2ND_OFS) & MFG_0_12_PWR_MASK);
	GPUFREQ_LOGI("[TOP]   CON1: %d, MFG_SEL_0: 0x%08x, MFG_REF_SEL:   0x%08x",
		__gpufreq_get_real_fgpu(), mfg_sel_0, mfg_ref_sel);
	GPUFREQ_LOGI("[STACK] MFG_SEL_1: 0x%08x, MFGSC_REF_SEL: 0x%08x",
		mfg_sel_1, mfgsc_ref_sel);

done:
	return;
}

//TODO FQMTR
/* MFGPLL fmeter control flow is from GPU DE */
static unsigned int __gpufreq_get_fmeter_main_fgpu(void)
{
	u32 val = 0, ckgen_load_cnt = 0, ckgen_k1 = 0;
	int i = 0;
	unsigned int freq = 0;

	writel(0x00FF0000, MFGPLL_FQMTR_CON1);
	val = readl(MFGPLL_FQMTR_CON0);
	writel((val & 0x00FFFFFF), MFGPLL_FQMTR_CON0);
	writel(0x00009000, MFGPLL_FQMTR_CON0);

	ckgen_load_cnt = readl(MFGPLL_FQMTR_CON1) >> 16;
	ckgen_k1 = readl(MFGPLL_FQMTR_CON0) >> 24;

	val = readl(MFGPLL_FQMTR_CON0);
	writel((val | 0x1010), MFGPLL_FQMTR_CON0);

	/* wait fmeter finish */
	while (readl(MFGPLL_FQMTR_CON0) & 0x10) {
		udelay(10);
		i++;
		if (i > 1000) {
			GPUFREQ_LOGE("wait MFGPLL Fmeter timeout");
			break;
		}
	}

	val = readl(MFGPLL_FQMTR_CON1) & 0xFFFF;
	/* KHz */
	freq = (val * 26000 * (ckgen_k1 + 1)) / (ckgen_load_cnt + 1);

	return freq;
}

/* GPU parking source use CCF API directly */
static unsigned int __gpufreq_get_fmeter_sub_fgpu(void)
{
	unsigned int freq = 0;

	/* Hz */
//todo:	freq = clk_get_rate(g_clk->clk_sub_parent) / 1000;

	return freq;
}

static unsigned int __gpufreq_get_fmeter_fgpu(void)
{
	u32 mux_src = 0;

	/* CLK_CFG_30 0x100001F0 [16] MFG_SEL_0 */
	mux_src = readl(g_topckgen_base + 0x1F0) & MFG_SEL_0_MASK;

	if (mux_src == MFG_SEL_0_MASK)
		return __gpufreq_get_fmeter_main_fgpu();
	else if (mux_src == 0x0)
		return __gpufreq_get_fmeter_sub_fgpu();
	else
		return 0;
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

	if (regulator_is_enabled(g_pmic->reg_vcore))
		/* regulator_get_voltage return volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vcore) / 10;

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

	/* VGPU is co-buck with VCORE, so use VCORE_BUCK to get Volt */
	g_pmic->reg_vcore = regulator_get_optional(&pdev->dev, "_vcore");
	if (IS_ERR(g_pmic->reg_vcore)) {
		ret = PTR_ERR(g_pmic->reg_vcore);
		__gpufreq_abort("fail to get VCORE (%ld)", ret);
		goto done;
	}

#if GPUFREQ_VCORE_DVFS_ENABLE
	/* VGPU is co-buck with VCORE, so use DVFSRC to set Volt */
	g_pmic->reg_dvfsrc = regulator_get_optional(&pdev->dev, "_dvfsrc");
	if (IS_ERR(g_pmic->reg_dvfsrc)) {
		ret = PTR_ERR(g_pmic->reg_dvfsrc);
		__gpufreq_abort("fail to get DVFSRC (%ld)", ret);
		goto done;
	}
#endif /* GPUFREQ_VCORE_DVFS_ENABLE */

	g_pmic->reg_vstack = regulator_get_optional(&pdev->dev, "_vstack");
	if (IS_ERR(g_pmic->reg_vstack)) {
		ret = PTR_ERR(g_pmic->reg_vstack);
		__gpufreq_abort("fail to get VSATCK (%ld)", ret);
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

	/* 0x13FA0C00 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfgsc_pll");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFGSC_PLL");
		goto done;
	}
	g_mfgsc_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfgsc_pll_base)) {
		GPUFREQ_LOGE("fail to ioremap MFGSC_PLL: 0x%llx", res->start);
		goto done;
	}

	/* 0x13FBF000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_top_config");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_TOP_CONFIG");
		goto done;
	}
	g_mfg_top_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_top_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_TOP_CONFIG: 0x%llx", res->start);
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

	/* 0x10000000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "topckgen");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource TOPCKGEN");
		goto done;
	}
	g_topckgen_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_topckgen_base)) {
		GPUFREQ_LOGE("fail to ioremap TOPCKGEN: 0x%llx", res->start);
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

	/* 0x1021E000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sth_emicfg_reg");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource STH_EMICFG_REG");
		goto done;
	}
	g_sth_emicfg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sth_emicfg_base)) {
		GPUFREQ_LOGE("fail to ioremap STH_EMICFG_REG: 0x%llx", res->start);
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

	/* 0x1030E000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sth_emicfg_ao_mem_reg");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource STH_EMICFG_AO_MEM_REG");
		goto done;
	}
	g_sth_emicfg_ao_mem_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sth_emicfg_ao_mem_base)) {
		GPUFREQ_LOGE("fail to ioremap STH_EMICFG_AO_MEM_REG: 0x%llx", res->start);
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

	/* 0x10042000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emi_ao_debug_ctrl");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource NTH_EMI_AO_DEBUG_CTRL");
		goto done;
	}
	g_nth_emi_ao_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_nth_emi_ao_debug_ctrl)) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMI_AO_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

#if GPUFREQ_AVS_ENABLE || GPUFREQ_ASENSOR_ENABLE
	/* 0x11EE0000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "efuse");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource EFUSE");
		goto done;
	}
	g_efuse_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_efuse_base)) {
		GPUFREQ_LOGE("fail to ioremap EFUSE: 0x%llx", res->start);
		goto done;
	}
#endif /* GPUFREQ_AVS_ENABLE || GPUFREQ_ASENSOR_ENABLE */

#if GPUFREQ_ASENSOR_ENABLE
	/* 0x13FB9C00 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_cpe_control");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_CPE_CONTROL");
		goto done;
	}
	g_mfg_cpe_control_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_cpe_control_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_CPE_CONTROL: 0x%llx", res->start);
		goto done;
	}

	/* 0x13FB6000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_cpe_sensor");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_CPE_SENSOR");
		goto done;
	}
	g_mfg_cpe_sensor_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_cpe_sensor_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_CPE_SENSOR: 0x%llx", res->start);
		goto done;
	}
#endif /* GPUFREQ_ASENSOR_ENABLE */

	ret = GPUFREQ_SUCCESS;

done:
	GPUFREQ_TRACE_END();

	return ret;
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

	/* init gpu ppm */
	ret = gpuppm_init(TARGET_STACK, g_gpueb_support);
	if (unlikely(ret)) {
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

	GPUFREQ_LOGI("start to init gpufreq platform driver");

	/* register gpufreq platform driver */
	ret = platform_driver_register(&g_gpufreq_pdrv);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to register gpufreq platform driver (%d)", ret);
		goto done;
	}

	GPUFREQ_LOGI("gpufreq platform driver init done");

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
