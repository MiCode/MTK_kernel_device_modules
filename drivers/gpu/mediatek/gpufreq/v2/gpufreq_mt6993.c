// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

/**
 * @file    gpufreq_mt6993.c
 * @brief   GPU-DVFS Driver Platform Implementation
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/err.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/printk.h>

#include <gpufreq_v2.h>
#include <gpuppm.h>
#include <gpufreq_common.h>
#include <gpufreq_mt6993.h>
#include <gpufreq_reg_mt6993.h>
/* GPUEB */
#include <ghpm_wrapper.h>

/**
 * ===============================================
 * Local Function Declaration
 * ===============================================
 */
/* misc function */
static void __iomem *__gpufreq_of_ioremap(const char *node_name, int idx);
static void __gpufreq_dump_power_tracker_status(void);
static void __gpufreq_dump_fv_tracker_status(char *log_buf, int *log_len, int log_size);
static void __gpufreq_dump_bus_tracker_status(char *log_buf, int *log_len, int log_size);
static irqreturn_t __gpufreq_bus_tracker_irq_handler(int irq, void *data);
/* bringup function */
static unsigned int __gpufreq_bringup(void);
static void __gpufreq_dump_bringup_status(struct platform_device *pdev);
/* whitebox function */
static void __gpufreq_wb_mfg1_slave_stress(void);
/* get function */
static unsigned int __gpufreq_get_fmeter_fgpu(void);
static unsigned int __gpufreq_get_fmeter_fstack(void);
static unsigned int __gpufreq_get_pll_fgpu(void);
static unsigned int __gpufreq_get_pll_fstack(void);
/* init function */
static int __gpufreq_init_platform_info(struct platform_device *pdev);
static int __gpufreq_init_bus_tracker_irq(struct platform_device *pdev);
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

static void __iomem *g_mfg_acp_gals_top_base;
static void __iomem *g_emi_infra_noncoh_gals;
static void __iomem *emi_infra_cfg_base;
static void __iomem *g_emi_infra_pdn_bcrm_base;
static void __iomem *g_emi_infra_acp_rsi_base;
static void __iomem *g_mali_base;
static void __iomem *g_mfg_top_base;
static void __iomem *g_mfg_pll_base;
static void __iomem *g_mfg_pll_sc0_base;
static void __iomem *g_mfg_rpc_base;
static void __iomem *g_mfg_smmu_base;
static void __iomem *g_mfg_vcore_ao_cfg_base;
static void __iomem *g_mfg_vcore_bus_trk_base;
static void __iomem *g_mfg_eb_bus_trk_base;
static void __iomem *g_mfg_vgpu_bus_trk_base;
static void __iomem *g_sleep;
static unsigned int g_gpueb_support;
static unsigned int g_shader_present;
static unsigned int g_mcl50_load;
static unsigned int g_aging_load;
static unsigned int g_gpufreq_ready;
static unsigned int g_wb_mfg1_slave_stress;
static int g_slv_error_count;
static int g_slv_timeout_count;
static struct gpufreq_bus_tracker_info g_bus_slv_error[GPUFREQ_MAX_BUSTRK_NUM];
static struct gpufreq_bus_tracker_info g_bus_slv_timeout[GPUFREQ_MAX_BUSTRK_NUM];
static struct gpufreq_shared_status *g_shared_status;
static DEFINE_MUTEX(gpufreq_lock);

static struct gpufreq_platform_fp platform_eb_fp = {
	.dump_external_status = __gpufreq_dump_external_status,
	.dump_internal_status = __gpufreq_dump_internal_status,
	.dump_shared_status = __gpufreq_dump_shared_status,
	.get_dyn_pgpu = __gpufreq_get_dyn_pgpu,
	.get_dyn_pstack = __gpufreq_get_dyn_pstack,
	.get_core_mask_table = __gpufreq_get_core_mask_table,
	.get_core_num = __gpufreq_get_core_num,
	.set_shared_status = __gpufreq_set_shared_status,
	.set_mfgsys_config = __gpufreq_set_mfgsys_config,
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
	unsigned long long p_dynamic = GPU_DYN_REF_POWER;
	unsigned int ref_freq = GPU_DYN_REF_POWER_FREQ;
	unsigned int ref_volt = GPU_DYN_REF_POWER_VOLT;

	p_dynamic = p_dynamic *
		((freq * 100000ULL) / ref_freq) *
		((volt * 100000ULL) / ref_volt) *
		((volt * 100000ULL) / ref_volt) /
		(100000ULL * 100000 * 100000);

	return (unsigned int)p_dynamic;
}

/* API: get dynamic Power of STACK */
unsigned int __gpufreq_get_dyn_pstack(unsigned int freq, unsigned int volt)
{
	unsigned long long p_dynamic = 0;
	unsigned int ref_freq = STACK_DYN_REF_POWER_FREQ;
	unsigned int ref_volt = STACK_DYN_REF_POWER_VOLT;

	p_dynamic = STACK_DYN_REF_POWER;

	p_dynamic = p_dynamic *
		((freq * 100000ULL) / ref_freq) *
		((volt * 100000ULL) / ref_volt) *
		((volt * 100000ULL) / ref_volt) /
		(100000ULL * 100000 * 100000);

	return (unsigned int)p_dynamic;
}

/* API: get core_mask table */
struct gpufreq_core_mask_info *__gpufreq_get_core_mask_table(void)
{
	return g_core_mask_table;
}

/* API: get max number of shader cores */
unsigned int __gpufreq_get_core_num(void)
{
	return SHADER_CORE_NUM;
}

void __gpufreq_dump_external_status(char *log_buf, int *log_len, int log_size)
{
	unsigned int val = 0;

	if (!g_gpufreq_ready)
		return;

	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"== [GPUFREQ EXTERNAL STATUS] ==");

	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[MFG_GALS]",
		"NEMI_M0_RX", DRV_Reg32(EMI_IFR_CFG_MFG_EMI0_NTH_GALS),
		"NEMI_M1_RX", DRV_Reg32(EMI_IFR_CFG_MFG_EMI1_NTH_GALS),
		"SEMI_M0_RX", DRV_Reg32(EMI_IFR_CFG_MFG_EMI0_STH_GALS),
		"SEMI_M1_RX", DRV_Reg32(EMI_IFR_CFG_MFG_EMI1_STH_GALS));

	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%x, %s=0x%x, %s=0x%x, %s=0x%x",
		"[EMI]",
		"NEMI_M0_PROT_IN_IDLE", (unsigned int)(DRV_Reg32(EMI_IFR_MFG_NTH_M0_PROT_CTRL) & BIT(0)),
		"NEMI_M1_PROT_IN_IDLE", (unsigned int)(DRV_Reg32(EMI_IFR_MFG_NTH_M1_PROT_CTRL) & BIT(0)),
		"SEMI_M0_PROT_IN_IDLE", (unsigned int)(DRV_Reg32(EMI_IFR_MFG_STH_M0_PROT_CTRL) & BIT(0)),
		"SEMI_M1_PROT_IN_IDLE", (unsigned int)(DRV_Reg32(EMI_IFR_MFG_STH_M1_PROT_CTRL) & BIT(0)));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%x, %s=0x%x, %s=0x%x, %s=0x%x",
		"[EMI]",
		"NEMI_M0_PROT_OUT_IDLE", (unsigned int)(DRV_Reg32(EMI_IFR_MFG_NTH_M0_CTRL) & BIT(0)),
		"NEMI_M1_PROT_OUT_IDLE", (unsigned int)(DRV_Reg32(EMI_IFR_MFG_NTH_M1_CTRL) & BIT(0)),
		"SEMI_M0_PROT_OUT_IDLE", (unsigned int)(DRV_Reg32(EMI_IFR_MFG_STH_M0_CTRL) & BIT(0)),
		"SEMI_M1_PROT_OUT_IDLE", (unsigned int)(DRV_Reg32(EMI_IFR_MFG_STH_M1_CTRL) & BIT(0)));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%x, %s=0x%x, %s=0x%x, %s=0x%x",
		"[EMI]",
		"NEMI_M0_MI_BUSY", (unsigned int)(DRV_Reg32(EMI_IFR_MFG_FAKE0_MI_CTRL) & GENMASK(1, 0)),
		"NEMI_M1_MI_BUSY", (unsigned int)(DRV_Reg32(EMI_IFR_MFG_FAKE1_MI_CTRL) & GENMASK(1, 0)),
		"SEMI_M0_MI_BUSY", (unsigned int)(DRV_Reg32(EMI_IFR_MFG_FAKE2_MI_CTRL) & GENMASK(1, 0)),
		"SEMI_M1_MI_BUSY", (unsigned int)(DRV_Reg32(EMI_IFR_MFG_FAKE3_MI_CTRL) & GENMASK(1, 0)));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%x, %s=0x%x, %s=0x%x, %s=0x%x",
		"[EMI]",
		"NEMI_M6_MI_BUSY", (unsigned int)(DRV_Reg32(EMI_IFR_NTH_M6_RW_MI_CTRL) & GENMASK(1, 0)),
		"NEMI_M7_MI_BUSY", (unsigned int)(DRV_Reg32(EMI_IFR_NTH_M7_RW_MI_CTRL) & GENMASK(1, 0)),
		"SEMI_M6_MI_BUSY", (unsigned int)(DRV_Reg32(EMI_IFR_STH_M6_RW_MI_CTRL) & GENMASK(1, 0)),
		"SEMI_M7_MI_BUSY", (unsigned int)(DRV_Reg32(EMI_IFR_STH_M7_RW_MI_CTRL) & GENMASK(1, 0)));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%x, %s=0x%x, %s=0x%x, %s=0x%x",
		"[EMI]",
		"CHI0_MI_BUSY", (unsigned int)(DRV_Reg32(EMI_IFR_ACP_CHI0_RW_MI_CTRL) & GENMASK(1, 0)),
		"CHI1_MI_BUSY", (unsigned int)(DRV_Reg32(EMI_IFR_ACP_CHI1_RW_MI_CTRL) & GENMASK(1, 0)),
		"CHI2_MI_BUSY", (unsigned int)(DRV_Reg32(EMI_IFR_ACP_CHI2_RW_MI_CTRL) & GENMASK(1, 0)),
		"CHI3_MI_BUSY", (unsigned int)(DRV_Reg32(EMI_IFR_ACP_CHI3_RW_MI_CTRL) & GENMASK(1, 0)));

	val = DRV_Reg32(EMI_IFR_ACP_DVM_SI_CTRL);
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%x, %s=0x%x, %s=0x%x, %s=0x%x",
		"[EMI_ACP1]",
		"TCU2EMI_IDLE", (unsigned int)(DRV_Reg32(EMI_IFR_ACP_MFG_DVM_PROT_CTRL) & BIT(0)),
		"DVM_PROT_IDLE", (unsigned int)(DRV_Reg32(EMI_IFR_ACP_TCU_EFP_M_CTRL) & BIT(0)),
		"DVM_SI_R_BUSY", (unsigned int)((val & BIT(2)) >> 2),
		"DVM_SI_W_BUSY", (unsigned int)((val & BIT(1)) >> 1));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%x, %s=0x%x, %s=0x%x",
		"[EMI_ACP1]",
		"RSI_M0_AW_OSTD", (unsigned int)(DRV_Reg32(EMI_IFR_ACP_RSI_AWOSTD_M0) & GENMASK(4, 0)),
		"RSI_M0_W_OSTD", (unsigned int)(DRV_Reg32(EMI_IFR_ACP_RSI_WOSTD_M0) & GENMASK(4, 0)),
		"RSI_M0_AR_OSTD", (unsigned int)(DRV_Reg32(EMI_IFR_ACP_RSI_AROSTD_M0) & GENMASK(5, 0)));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%x, %s=0x%x, %s=0x%x",
		"[EMI_ACP1]",
		"RSI_M1_AW_OSTD", (unsigned int)(DRV_Reg32(EMI_IFR_ACP_RSI_AWOSTD_M1) & GENMASK(4, 0)),
		"RSI_M1_W_OSTD", (unsigned int)(DRV_Reg32(EMI_IFR_ACP_RSI_WOSTD_M1) & GENMASK(4, 0)),
		"RSI_M1_AR_OSTD", (unsigned int)(DRV_Reg32(EMI_IFR_ACP_RSI_AROSTD_M1) & GENMASK(5, 0)));

	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%x, %s=0x%x, %s=0x%x, %s=0x%x",
		"[EMI_APU]",
		"NTH_M0_TX_EMPTY", (unsigned int)(DRV_Reg32(EMI_IFR_NONCOH_GALS_NTH_APU_M0_TX_STA0) & GENMASK(3, 0)),
		"NTH_M1_TX_EMPTY", (unsigned int)(DRV_Reg32(EMI_IFR_NONCOH_GALS_NTH_APU_M1_TX_STA0) & GENMASK(3, 0)),
		"STH_M0_TX_EMPTY", (unsigned int)(DRV_Reg32(EMI_IFR_NONCOH_GALS_STH_APU_M0_TX_STA0) & GENMASK(3, 0)),
		"STH_M1_TX_EMPTY", (unsigned int)(DRV_Reg32(EMI_IFR_NONCOH_GALS_STH_APU_M1_TX_STA0) & GENMASK(3, 0)));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%x, %s=0x%x, %s=0x%x, %s=0x%x",
		"[EMI_APU]",
		"NTH_M0_RX_EMPTY", (unsigned int)(DRV_Reg32(EMI_IFR_NONCOH_GALS_NTH_APU_M0_RX_STA0) & GENMASK(1, 0)),
		"NTH_M1_RX_EMPTY", (unsigned int)(DRV_Reg32(EMI_IFR_NONCOH_GALS_NTH_APU_M1_RX_STA0) & GENMASK(1, 0)),
		"STH_M0_RX_EMPTY", (unsigned int)(DRV_Reg32(EMI_IFR_NONCOH_GALS_STH_APU_M0_RX_STA0) & GENMASK(1, 0)),
		"STH_M1_RX_EMPTY", (unsigned int)(DRV_Reg32(EMI_IFR_NONCOH_GALS_STH_APU_M1_RX_STA0) & GENMASK(1, 0)));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%x, %s=0x%x, %s=0x%x, %s=0x%x",
		"[EMI_SOC]",
		"NTH_M0_TX_EMPTY", (unsigned int)(DRV_Reg32(EMI_IFR_NONCOH_GALS_NTH_IFR_M0_TX_STA0) & GENMASK(5, 0)),
		"NTH_M1_TX_EMPTY", (unsigned int)(DRV_Reg32(EMI_IFR_NONCOH_GALS_NTH_IFR_M1_TX_STA0) & GENMASK(5, 0)),
		"STH_M0_TX_EMPTY", (unsigned int)(DRV_Reg32(EMI_IFR_NONCOH_GALS_STH_IFR_M0_TX_STA0) & GENMASK(5, 0)),
		"STH_M1_TX_EMPTY", (unsigned int)(DRV_Reg32(EMI_IFR_NONCOH_GALS_STH_IFR_M1_TX_STA0) & GENMASK(5, 0)));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%x, %s=0x%x, %s=0x%x, %s=0x%x",
		"[EMI_SOC]",
		"NTH_M0_RX_EMPTY", (unsigned int)(DRV_Reg32(EMI_IFR_NONCOH_GALS_NTH_IFR_M0_RX_STA0) & GENMASK(1, 0)),
		"NTH_M1_RX_EMPTY", (unsigned int)(DRV_Reg32(EMI_IFR_NONCOH_GALS_NTH_IFR_M1_RX_STA0) & GENMASK(1, 0)),
		"STH_M0_RX_EMPTY", (unsigned int)(DRV_Reg32(EMI_IFR_NONCOH_GALS_STH_IFR_M0_RX_STA0) & GENMASK(1, 0)),
		"STH_M1_RX_EMPTY", (unsigned int)(DRV_Reg32(EMI_IFR_NONCOH_GALS_STH_IFR_M1_RX_STA0) & GENMASK(1, 0)));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%x, %s=0x%x, %s=0x%x, %s=0x%x",
		"[EMI_MM]",
		"M0_TX_EMPTY", (unsigned int)(DRV_Reg32(EMI_IFR_NONCOH_GALS_MM_M0_TX_STA0) & GENMASK(7, 0)),
		"M1_TX_EMPTY", (unsigned int)(DRV_Reg32(EMI_IFR_NONCOH_GALS_MM_M1_TX_STA0) & GENMASK(7, 0)),
		"M0_RX_EMPTY", (unsigned int)(DRV_Reg32(EMI_IFR_NONCOH_GALS_MM_M0_RX_STA0) & GENMASK(4, 0)),
		"M1_RX_EMPTY", (unsigned int)(DRV_Reg32(EMI_IFR_NONCOH_GALS_MM_M1_RX_STA0) & GENMASK(4, 0)));

	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08lx",
		"[MISC]",
		"SPM_SRC_REQ", DRV_Reg32(SPM_SRC_REQ),
		"SPM_SOC_BUCK_ISO", DRV_Reg32(SPM_SOC_BUCK_ISO_CON),
		"PWR_STATUS", MFG_0_22_37_PWR_STATUS);
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[MTCMOS]",
		"MFG0", DRV_Reg32(MFG_RPC_MFG0_PWR_CON),
		"MFG1", DRV_Reg32(MFG_RPC_MFG1_PWR_CON),
		"MFG37", DRV_Reg32(MFG_RPC_MFG37_PWR_CON),
		"MFG2", DRV_Reg32(MFG_RPC_MFG2_PWR_CON),
		"MFG3", DRV_Reg32(MFG_RPC_MFG3_PWR_CON),
		"MFG4", DRV_Reg32(MFG_RPC_MFG4_PWR_CON));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[MTCMOS]",
		"MFG5", DRV_Reg32(MFG_RPC_MFG5_PWR_CON),
		"MFG22", DRV_Reg32(MFG_RPC_MFG22_PWR_CON),
		"MFG6", DRV_Reg32(MFG_RPC_MFG6_PWR_CON),
		"MFG7", DRV_Reg32(MFG_RPC_MFG7_PWR_CON),
		"MFG9", DRV_Reg32(MFG_RPC_MFG9_PWR_CON),
		"MFG10", DRV_Reg32(MFG_RPC_MFG10_PWR_CON));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[MTCMOS]",
		"MFG11", DRV_Reg32(MFG_RPC_MFG11_PWR_CON),
		"MFG12", DRV_Reg32(MFG_RPC_MFG12_PWR_CON),
		"MFG13", DRV_Reg32(MFG_RPC_MFG13_PWR_CON),
		"MFG14", DRV_Reg32(MFG_RPC_MFG14_PWR_CON),
		"MFG15", DRV_Reg32(MFG_RPC_MFG15_PWR_CON),
		"MFG16", DRV_Reg32(MFG_RPC_MFG16_PWR_CON));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[MTCMOS]",
		"MFG17", DRV_Reg32(MFG_RPC_MFG17_PWR_CON),
		"MFG18", DRV_Reg32(MFG_RPC_MFG18_PWR_CON),
		"MFG19", DRV_Reg32(MFG_RPC_MFG19_PWR_CON),
		"MFG20", DRV_Reg32(MFG_RPC_MFG20_PWR_CON));
}

void __gpufreq_dump_internal_status(char *log_buf, int *log_len, int log_size)
{
	unsigned int val1 = 0, val2 = 0, val3 = 0, val4 = 0;

	if (!g_gpufreq_ready)
		return;

	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"== [GPUFREQ INTERNAL STATUS] ==");

	val1 = DRV_Reg32(MFG_TOP_AXI_SLPPROT_FREQ_BRIDGE);
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%x, %s=0x%x, %s=0x%x, %s=0x%x",
		"[MFG]",
		"MFG1_NEMI_M0_IDLE", (unsigned int)((val1 & BIT(8)) >> 8),
		"MFG1_NEMI_M1_IDLE", (unsigned int)((val1 & BIT(10)) >> 10),
		"MFG1_SEMI_M0_IDLE", (unsigned int)((val1 & BIT(12)) >> 12),
		"MFG1_SEMI_M1_IDLE", (unsigned int)((val1 & BIT(14)) >> 14));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%x, %s=0x%x, %s=0x%x, %s=0x%x",
		"[MFG]",
		"MFG0_NEMI_M0_IDLE", (unsigned int)((val1 & BIT(16)) >> 16),
		"MFG0_NEMI_M1_IDLE", (unsigned int)((val1 & BIT(18)) >> 18),
		"MFG0_SEMI_M0_IDLE", (unsigned int)((val1 & BIT(20)) >> 20),
		"MFG0_SEMI_M1_IDLE", (unsigned int)((val1 & BIT(22)) >> 22));

	/* MFG_TOP_DEBUG_SEL 0x48500170 [23:16] MFG_DEBUG_ASYNC_SEL = 2'h08 */
	DRV_WriteReg32(MFG_TOP_DEBUG_SEL, (DRV_Reg32(MFG_TOP_DEBUG_SEL) & ~GENMASK(23, 16)) | BIT(19));
	val1 = DRV_Reg32(MFG_TOP_DEBUG_ASYNC);
	/* MFG_TOP_DEBUG_SEL 0x48500170 [23:16] MFG_DEBUG_ASYNC_SEL = 2'h0E */
	DRV_WriteReg32(MFG_TOP_DEBUG_SEL, (DRV_Reg32(MFG_TOP_DEBUG_SEL) & ~GENMASK(23, 16)) | GENMASK(19, 17));
	val2 = DRV_Reg32(MFG_TOP_DEBUG_ASYNC);
	/* MFG_TOP_DEBUG_SEL 0x48500170 [23:16] MFG_DEBUG_ASYNC_SEL = 2'h09 */
	DRV_WriteReg32(MFG_TOP_DEBUG_SEL, (DRV_Reg32(MFG_TOP_DEBUG_SEL) & ~GENMASK(23, 16)) | BIT(19) | BIT(16));
	val3 = DRV_Reg32(MFG_TOP_DEBUG_ASYNC);
	/* MFG_TOP_DEBUG_SEL 0x48500170 [23:16] MFG_DEBUG_ASYNC_SEL = 2'h0F */
	DRV_WriteReg32(MFG_TOP_DEBUG_SEL, (DRV_Reg32(MFG_TOP_DEBUG_SEL) & ~GENMASK(23, 16)) | GENMASK(19, 16));
	val4 = DRV_Reg32(MFG_TOP_DEBUG_ASYNC);
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[MFG_GALS]",
		"NEMI_M0_TX", val1,
		"NEMI_M1_TX", val2,
		"SEMI_M0_TX", val3,
		"SEMI_M1_TX", val4);

	val1 = DRV_Reg32(MFG_TOP_ACP_SLPPROT_FREQ_BRIDGE);
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%x, %s=0x%x, %s=0x%x, %s=0x%x",
		"[MFG_ACP1]",
		"MFG1_ACP1_IDLE", (unsigned int)((val1 & BIT(2)) >> 2),
		"MFG0_ACP1_IDLE", (unsigned int)((val1 & BIT(4)) >> 4),
		"AFIFO_TX_EMPTY", (unsigned int)(DRV_Reg32(MFG_ACP_GALS0_SLV_TX_STA0) & GENMASK(3, 0)),
		"AFIFO_RX_EMPTY", (unsigned int)(DRV_Reg32(MFG_ACP_GALS0_SLV_RX_STA0) & GENMASK(3, 0)));

	__gpufreq_dump_power_tracker_status();
	__gpufreq_dump_fv_tracker_status(log_buf, log_len, log_size);
	__gpufreq_dump_bus_tracker_status(log_buf, log_len, log_size);
}

void __gpufreq_dump_shared_status(char *log_buf, int *log_len, int log_size)
{
	int cur_oppidx_gpu = 0, cur_oppidx_stack = 0, vgpu_diff = 0, vstack_diff = 0;
	unsigned int cur_vgpu = 0, cur_vstack = 0, opp_vgpu = 0, opp_vstack = 0;
	unsigned long long power_time = 0;
	struct gpufreq_ptp3_shared_status ptp3_status = {};

	if (!g_gpufreq_ready)
		return;

	if (g_shared_status) {
		GPUFREQ_LOGB(log_buf, log_len, log_size,
			"== [GPUFREQ SHARED STATUS] ==");

		ptp3_status = g_shared_status->ptp3_status;
		power_time = g_shared_status->power_time_h;
		power_time = (power_time << 32) | g_shared_status->power_time_l;
		cur_oppidx_gpu = g_shared_status->cur_oppidx_gpu;
		cur_vgpu = g_shared_status->cur_vgpu;
		if (cur_oppidx_gpu >= 0 && cur_oppidx_gpu < g_shared_status->opp_num_gpu) {
			opp_vgpu = g_shared_status->working_table_gpu[cur_oppidx_gpu].volt;
			vgpu_diff = (int)cur_vgpu - (int)opp_vgpu;
		} else
			GPUFREQ_LOGE("abnormal cur_oppidx_gpu: %d", cur_oppidx_gpu);
		cur_oppidx_stack = g_shared_status->cur_oppidx_stack;
		cur_vstack = g_shared_status->cur_vstack;
		if (cur_oppidx_stack >= 0 && cur_oppidx_stack < g_shared_status->opp_num_stack) {
			opp_vstack = g_shared_status->working_table_stack[cur_oppidx_stack].volt;
			vstack_diff = (int)cur_vstack - (int)opp_vstack;
		} else
			GPUFREQ_LOGE("abnormal cur_oppidx_stack: %d", cur_oppidx_stack);

		GPUFREQ_LOGB(log_buf, log_len, log_size,
			"DBGVer: 0x%08x, KDBGVer: 0x%08x, PTPVer: 0x%04x, Flavor: %s, DVFSMode: %s",
			g_shared_status->dbg_version, g_shared_status->kdbg_version,
			g_shared_status->ptp_version, g_shared_status->flavor,
			(ptp3_status.dvfs_mode == HW_DUAL_LOOP_DVFS ? "HW_LOOP" :
			(ptp3_status.dvfs_mode == SW_DUAL_LOOP_DVFS ? "SW_LOOP" : "LEGACY")));
		GPUFREQ_LOGB(log_buf, log_len, log_size,
			"GPU[%d] Freq: %d/%d, Volt: %d (%d), Vsram: %d",
			g_shared_status->cur_oppidx_gpu, g_shared_status->cur_fgpu,
			g_shared_status->cur_out_fgpu, g_shared_status->cur_vgpu,
			vgpu_diff, g_shared_status->cur_vsram_gpu);
		GPUFREQ_LOGB(log_buf, log_len, log_size,
			"STACK[%d] Freq: %d/%d, Volt: %d (%d), Vsram: %d",
			g_shared_status->cur_oppidx_stack, g_shared_status->cur_fstack,
			g_shared_status->cur_out_fstack, g_shared_status->cur_vstack,
			vstack_diff, g_shared_status->cur_vsram_stack);
		GPUFREQ_LOGB(log_buf, log_len, log_size,
			"Temperature: %d'C, StressTest: %d, Ceiling/Floor: %d/%d, Limiter: %d/%d",
			g_shared_status->temperature, g_shared_status->stress_test,
			g_shared_status->cur_ceiling, g_shared_status->cur_floor,
			g_shared_status->cur_c_limiter, g_shared_status->cur_f_limiter);
		GPUFREQ_LOGB(log_buf, log_len, log_size,
			"PowerCount: %d, ActiveCount: %d, Buck: %d, MTCMOS: %d, CG: %d",
			g_shared_status->power_count, g_shared_status->active_count,
			g_shared_status->buck_count, g_shared_status->mtcmos_count,
			g_shared_status->cg_count);
		GPUFREQ_LOGB(log_buf, log_len, log_size,
			"PowerStatus: 0x%08x, Timestamp: %lld, ShaderPresent: 0x%08x",
			g_shared_status->mfg_pwr_status, power_time,
			g_shared_status->shader_present);
		GPUFREQ_LOGB(log_buf, log_len, log_size,
			"VCORE: %d, DDR: %d, EMI: %d",
			(g_shared_status->vcore_level & VCORE_LEVEL_MASK) >> VCORE_LEVEL_SHIFT,
			(g_shared_status->vcore_level & DDR_LEVEL_MASK) >> DDR_LEVEL_SHIFT,
			(g_shared_status->vcore_level & EMI_LEVEL_MASK) >> EMI_LEVEL_SHIFT);
		GPUFREQ_LOGB(log_buf, log_len, log_size,
			"InFreq: %d/%d, OutFreq: %d/%d, CC:%d/%d, FC:%d/%d",
			g_shared_status->ptp3_info.infreq0, g_shared_status->ptp3_info.infreq1,
			g_shared_status->ptp3_info.outfreq0, g_shared_status->ptp3_info.outfreq1,
			g_shared_status->ptp3_info.hw_cc, g_shared_status->ptp3_info.sw_cc,
			g_shared_status->ptp3_info.hw_fc, g_shared_status->ptp3_info.sw_fc);
	}
}

/* API: get working OPP index of STACK limited by BATTERY_OC via given level */
int __gpufreq_get_batt_oc_idx(int batt_oc_level)
{
	GPUFREQ_UNREFERENCED(batt_oc_level);
	return -1;
}

/* API: get working OPP index of STACK limited by BATTERY_PERCENT via given level */
int __gpufreq_get_batt_percent_idx(int batt_percent_level)
{
	GPUFREQ_UNREFERENCED(batt_percent_level);
	return -1;
}

/* API: get working OPP index of STACK limited by LOW_BATTERY via given level */
int __gpufreq_get_low_batt_idx(int low_batt_level)
{
	GPUFREQ_UNREFERENCED(low_batt_level);
	return -1;
}

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

/* API: init first time shared status */
void __gpufreq_set_shared_status(struct gpufreq_shared_status *shared_status)
{
	if (shared_status && !g_shared_status)
		g_shared_status = shared_status;

	/* update current status to shared memory */
	if (g_shared_status)
		g_shared_status->kdbg_version = GPUFREQ_KDEBUG_VERSION;

	GPUFREQ_LOGI("shared memory addr: 0x%llx", (unsigned long long)shared_status);
}

void __gpufreq_set_mfgsys_config(enum gpufreq_config_target target, enum gpufreq_config_value val)
{
	switch (target) {
	case CONFIG_WB_TEST_ONCE:
		/* execute every test case */
		__gpufreq_wb_mfg1_slave_stress();
		break;
	case CONFIG_WB_MFG1_SLAVE_STRESS:
		g_wb_mfg1_slave_stress = val;
		GPUFREQ_LOGI("set WB_MFG1_SLAVE_STRESS: %d", g_wb_mfg1_slave_stress);
		break;
	default:
		GPUFREQ_LOGE("invalid config target: %d", target);
		break;
	}
}

/**
 * ===============================================
 * Internal Function Definition
 * ===============================================
 */
static void __gpufreq_wb_mfg1_slave_stress(void)
{
	int i = 0;
	unsigned int val = 0;

	if (g_wb_mfg1_slave_stress) {
		for (i = 0; i < 100; i++) {
			val = DRV_Reg32(MALI_GPU_ID);
			val = DRV_Reg32(MFG_TOP_CG_CON);
		}
	}
}

static void __gpufreq_dump_bringup_status(struct platform_device *pdev)
{
	struct device *gpufreq_dev = &pdev->dev;
	struct resource *res = NULL;

	if (!gpufreq_dev) {
		GPUFREQ_LOGE("fail to find gpufreq device (ENOENT)");
		return;
	}

	/* 0x48000000 */
	g_mali_base = __gpufreq_of_ioremap("mediatek,mali", 0);
	if (!g_mali_base) {
		GPUFREQ_LOGE("fail to ioremap MALI");
		return;
	}
	/* 0x48500000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_top_config");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_TOP_CONFIG");
		return;
	}
	g_mfg_top_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_top_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_TOP_CONFIG: 0x%llx", res->start);
		return;
	}
	/* 0x48600000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_smmu");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_SMMU");
		return;
	}
	g_mfg_smmu_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_smmu_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_SMMU: 0x%llx", res->start);
		return;
	}
	/* 0x4B800000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_rpc");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_RPC");
		return;
	}
	g_mfg_rpc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_rpc_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_RPC: 0x%llx", res->start);
		return;
	}
	/* 0x4B810000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL");
		return;
	}
	g_mfg_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_pll_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL: 0x%llx", res->start);
		return;
	}
	/* 0x4B810400 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll_sc0");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL_SC0");
		return;
	}
	g_mfg_pll_sc0_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_pll_sc0_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL_SC0: 0x%llx", res->start);
		return;
	}
	/* 0x4B860000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_vcore_ao_cfg");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_VCORE_AO_CFG");
		return;
	}
	g_mfg_vcore_ao_cfg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_vcore_ao_cfg_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_VCORE_AO_CFG: 0x%llx", res->start);
		return;
	}
	/* 0x1C004000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sleep");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource SLEEP");
		return;
	}
	g_sleep = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_sleep) {
		GPUFREQ_LOGE("fail to ioremap SLEEP: 0x%llx", res->start);
		return;
	}

	GPUFREQ_LOGI("[SPM] %s=0x%08x",
		"SPM2GPUPM_CON", DRV_Reg32(SPM_SPM2GPUPM_CON));
	GPUFREQ_LOGI("[MFG] %s=0x%08lx, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"MFG_0_22_37_PWR_STATUS", MFG_0_22_37_PWR_STATUS,
		"MFG0_PWR_CON", DRV_Reg32(MFG_RPC_MFG0_PWR_CON),
		"MFG1_PWR_CON", DRV_Reg32(MFG_RPC_MFG1_PWR_CON),
		"SMMU_CR0", DRV_Reg32(MFG_SMMU_CR0),
		"SMMU_GBPA", DRV_Reg32(MFG_SMMU_GBPA));
	GPUFREQ_LOGI("[MFG] %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"MFG_DREQ", DRV_Reg32(MFG_VCORE_AO_DREQ_CONFIG),
		"MFG_DEFAULT_DELSEL", DRV_Reg32(MFG_TOP_DEFAULT_DELSEL_00),
		"MFG_TOP_DELSEL", DRV_Reg32(MFG_TOP_SRAM_FUL_SEL_ULV_TOP),
		"MFG_STACK_DELSEL", DRV_Reg32(MFG_TOP_SRAM_FUL_SEL_ULV));
	GPUFREQ_LOGI("[MFG] %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"BRISKET_TOP", DRV_Reg32(MFG_RPC_BRISKET_TOP_AO_CFG_0),
		"BRISKET_ST0", DRV_Reg32(MFG_TOP_BRISKET_ST0_AO_CFG_0),
		"BRISKET_ST1", DRV_Reg32(MFG_TOP_BRISKET_ST1_AO_CFG_0),
		"BRISKET_ST2", DRV_Reg32(MFG_TOP_BRISKET_ST2_AO_CFG_0));
	GPUFREQ_LOGI("[MFG] %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"BRISKET_ST3", DRV_Reg32(MFG_TOP_BRISKET_ST3_AO_CFG_0),
		"BRISKET_ST4", DRV_Reg32(MFG_TOP_BRISKET_ST4_AO_CFG_0),
		"BRISKET_ST5", DRV_Reg32(MFG_TOP_BRISKET_ST5_AO_CFG_0));
	GPUFREQ_LOGI("[TOP] %s=0x%08x, %s=%d, %s=%d, %s=0x%08lx",
		"PLL_CON0", DRV_Reg32(MFG_PLL_CON0),
		"CON1", __gpufreq_get_pll_fgpu(),
		"FMETER", __gpufreq_get_fmeter_fgpu(),
		"SEL", DRV_Reg32(MFG_VCORE_AO_CK_FAST_REF_SEL) & MFG_TOP_SEL_BIT);
	GPUFREQ_LOGI("[SC0] %s=0x%08x, %s=%d, %s=%d, %s=0x%08lx",
		"PLL_CON0", DRV_Reg32(MFG_PLL_SC0_CON0),
		"CON1", __gpufreq_get_pll_fstack(),
		"FMETER", __gpufreq_get_fmeter_fstack(),
		"SEL", DRV_Reg32(MFG_VCORE_AO_CK_FAST_REF_SEL) & MFG_SC0_SEL_BIT);
	GPUFREQ_LOGI("[GPU] %s=0x%08x", "MALI_GPU_ID", DRV_Reg32(MALI_GPU_ID));
}

static void __gpufreq_dump_power_tracker_status(void)
{
	int i = 0;
	unsigned int w_ptr = 0, r_ptr = 0;

	if (!g_gpufreq_ready)
		return;

	if (g_shared_status && g_shared_status->power_count &&
		g_shared_status->power_tracker_mode) {
		w_ptr = (DRV_Reg32(MFG_TOP_POWER_TRACKER_SETTING) & GENMASK(15, 10)) >> 10;
		GPUFREQ_LOGI("== [PDC POWER TRACKER STATUS: %02u] ==", w_ptr);
		for (i = 1; i <= 16; i++) {
			/* only dump last 16 record */
			r_ptr = (w_ptr + ~i + 1) & GENMASK(5, 0);
			DRV_FieldReg32(MFG_TOP_POWER_TRACKER_SETTING, r_ptr, GENMASK(9, 4));
			udelay(1);

			GPUFREQ_LOGI("[%02u][%u] 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x",
				r_ptr, DRV_Reg32(MFG_TOP_POWER_TRACKER_PDC_STATUS0),
				DRV_Reg32(MFG_TOP_POWER_TRACKER_PDC_STATUS1),
				DRV_Reg32(MFG_TOP_POWER_TRACKER_PDC_STATUS2),
				DRV_Reg32(MFG_TOP_POWER_TRACKER_PDC_STATUS3),
				DRV_Reg32(MFG_TOP_POWER_TRACKER_PDC_STATUS4),
				DRV_Reg32(MFG_TOP_POWER_TRACKER_PDC_STATUS5),
				DRV_Reg32(MFG_TOP_POWER_TRACKER_PDC_STATUS6),
				DRV_Reg32(MFG_TOP_POWER_TRACKER_PDC_STATUS7));
		}
	}
}

static void __gpufreq_dump_fv_tracker_status(char *log_buf, int *log_len, int log_size)
{
	int i = 0;
	unsigned int w_ptr_gpu = 0, r_ptr_gpu = 0, w_ptr_stk = 0, r_ptr_stk = 0;

	if (!g_gpufreq_ready)
		return;

	if (g_shared_status && g_shared_status->power_count &&
		g_shared_status->ptp3_status.freq_tracker_mode &&
		g_shared_status->ptp3_status.volt_tracker_mode) {
		/* stop tracker */
		DRV_ClrReg32(MFG_TOP_TOP_FREQ_TRACKER_CON_0, BIT(8));
		DRV_ClrReg32(MFG_TOP_STACK_FREQ_TRACKER_CON_0, BIT(8));

		w_ptr_gpu = (DRV_Reg32(MFG_TOP_TOP_FREQ_TRACKER_CON_3) & GENMASK(16, 11)) >> 11;
		w_ptr_stk = (DRV_Reg32(MFG_TOP_STACK_FREQ_TRACKER_CON_3) & GENMASK(16, 11)) >> 11;
		GPUFREQ_LOGB(log_buf, log_len, log_size,
			"== [FREQ VOLT TRACKER STATUS: GPU=%02u, STK=%02u] ==", w_ptr_gpu, w_ptr_stk);
		for (i = 1; i <= 16; i++) {
			/* only dump last 16 record */
			r_ptr_gpu = (w_ptr_gpu + ~i + 1) & GENMASK(5, 0);
			DRV_FieldReg32(MFG_TOP_TOP_FREQ_TRACKER_CON_1, r_ptr_gpu, GENMASK(19, 14));
			DRV_FieldReg32(MFG_TOP_VOLT_TRACKER_CON_5, r_ptr_gpu, GENMASK(19, 14));
			r_ptr_stk = (w_ptr_stk + ~i + 1) & GENMASK(5, 0);
			DRV_FieldReg32(MFG_TOP_STACK_FREQ_TRACKER_CON_1, r_ptr_stk, GENMASK(19, 14));
			DRV_FieldReg32(MFG_TOP_VOLT_TRACKER_CON_1, r_ptr_stk, GENMASK(19, 14));
			udelay(1);

			GPUFREQ_LOGB(log_buf, log_len, log_size,
				"[GPU][%02u][%u] Freq=%lu [%u] Volt=%lu [STK][%02u][%u] Freq=%lu [%u] Volt=%lu",
				r_ptr_gpu, FTRACKER_TGPU, FTRACKER_FGPU, VTRACKER_TGPU, VTRACKER_VGPU,
				r_ptr_stk, FTRACKER_TSTACK, FTRACKER_FSTACK, VTRACKER_TSTACK, VTRACKER_VSTACK);
		}
	}
}

static void __gpufreq_dump_bus_tracker_status(char *log_buf, int *log_len, int log_size)
{
	int i = 0, idx = 0;

	if (!g_gpufreq_ready)
		return;

	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"== [BUS TRACKER SLAVE ERROR: %d] ==", g_slv_error_count);
	for (i = 1; i <= GPUFREQ_MAX_BUSTRK_NUM; i++) {
		if (g_slv_error_count >= i) {
			idx = (g_slv_error_count - i) % GPUFREQ_MAX_BUSTRK_NUM;
			GPUFREQ_LOGB(log_buf, log_len, log_size,
				"[%02d][%s][%llu] LOG=0x%08x, ID=0x%08x, ADDR=0x%08x",
				g_slv_error_count - i,
				GPUFREQ_BUS_TRACKER_TYPE_STRING(g_bus_slv_error[idx].type),
				g_bus_slv_error[idx].timestamp, g_bus_slv_error[idx].log,
				g_bus_slv_error[idx].id, g_bus_slv_error[idx].addr);
		}
	}

	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"== [BUS TRACKER SLAVE TIMEOUT: %d] ==", g_slv_timeout_count);
	for (i = 1; i <= GPUFREQ_MAX_BUSTRK_NUM; i++) {
		if (g_slv_timeout_count >= i) {
			idx = (g_slv_timeout_count - i) % GPUFREQ_MAX_BUSTRK_NUM;
			GPUFREQ_LOGB(log_buf, log_len, log_size,
				"[%02d][%s][%llu] LOG=0x%08x, ID=0x%08x, ADDR=0x%08x",
				g_slv_timeout_count - i,
				GPUFREQ_BUS_TRACKER_TYPE_STRING(g_bus_slv_timeout[idx].type),
				g_bus_slv_timeout[idx].timestamp, g_bus_slv_timeout[idx].log,
				g_bus_slv_timeout[idx].id, g_bus_slv_timeout[idx].addr);
		}
	}
}

static irqreturn_t __gpufreq_bus_tracker_irq_handler(int irq, void *data)
{
	unsigned int ret = false, check_mask = 0;
	unsigned int vcore_bus_dbg_con = 0, vgpu_bus_dbg_con = 0, gpueb_bus_dbg_con = 0;
	unsigned int tracker_log = 0, tracker_id = 0, tracker_addr = 0;
	unsigned long long timestamp = 0;
	unsigned int fatal_slave_timeout = true;
	int i = 0;

	/* power on gpueb */
	ret = gpueb_ctrl(GHPM_ON, MFG1_OFF, SUSPEND_POWER_ON);
	if (ret) {
		__gpufreq_abort("gpueb power on fail, ret=%d", ret);
		return IRQ_NONE;
	}

	/* check bus tracker violation status */
	vcore_bus_dbg_con = DRV_Reg32(MFG_VCORE_BUS_DBG_CON_0);
	vgpu_bus_dbg_con = DRV_Reg32(MFG_VGPU_BUS_DBG_CON_0);
	gpueb_bus_dbg_con = DRV_Reg32(MFG_GPUEB_BUS_DBG_CON_0);

	/* GPUEB bus tracker timeout */
	if (gpueb_bus_dbg_con & GENMASK(9, 8)) {
		timestamp = (unsigned long long)DRV_Reg32(MFG_GPUEB_BUS_SYSTIMER_LATCH_H) << 32 |
			DRV_Reg32(MFG_GPUEB_BUS_SYSTIMER_LATCH_L);
		GPUFREQ_LOGE("[GPUEB_BUS_TRK][%llu] %s=0x%08x, %s=0x%08x", timestamp,
			"GPUEB BUS_DBG_CON_0", gpueb_bus_dbg_con,
			"TIMEOUT_INFO", DRV_Reg32(MFG_GPUEB_BUS_TIMEOUT_INFO));

		/* GPUEB read timeout */
		if (gpueb_bus_dbg_con & BIT(8)) {
			for (i = 0; i < 32; i++) {
				if (!DRV_Reg32(MFG_GPUEB_BUS_AR_TRACKER_LOG + (i * 4)))
					continue;
				BUS_TRACKER_OP(
					g_bus_slv_timeout[g_slv_timeout_count % GPUFREQ_MAX_BUSTRK_NUM],
					g_slv_timeout_count, BUS_GPUEB_AR, timestamp,
					DRV_Reg32(MFG_GPUEB_BUS_AR_TRACKER_LOG + (i * 4)),
					DRV_Reg32(MFG_GPUEB_BUS_AR_TRACKER_ID + (i * 4)),
					DRV_Reg32(MFG_GPUEB_BUS_AR_TRACKER_L + (i * 4)));
			}
		}
		/* GPUEB write timeout */
		if (gpueb_bus_dbg_con & BIT(9)) {
			for (i = 0; i < 32; i++) {
				if (!DRV_Reg32(MFG_GPUEB_BUS_AW_TRACKER_LOG + (i * 4)))
					continue;
				BUS_TRACKER_OP(
					g_bus_slv_timeout[g_slv_timeout_count % GPUFREQ_MAX_BUSTRK_NUM],
					g_slv_timeout_count, BUS_GPUEB_AW, timestamp,
					DRV_Reg32(MFG_GPUEB_BUS_AW_TRACKER_LOG + (i * 4)),
					DRV_Reg32(MFG_GPUEB_BUS_AW_TRACKER_ID + (i * 4)),
					DRV_Reg32(MFG_GPUEB_BUS_AW_TRACKER_L + (i * 4)));
			}
		}

		/* update current status to shared memory */
		if (g_shared_status)
			ARRAY_ASSIGN(g_shared_status->bus_slv_timeout,
				g_bus_slv_timeout, GPUFREQ_MAX_BUSTRK_NUM);
	}

	/* VCORE/VGPU bus tracker timeout */
	if (vcore_bus_dbg_con & GENMASK(9, 8) || vgpu_bus_dbg_con & GENMASK(9, 8)) {
		timestamp = (unsigned long long)DRV_Reg32(MFG_VCORE_BUS_SYSTIMER_LATCH_H) << 32 |
			DRV_Reg32(MFG_VCORE_BUS_SYSTIMER_LATCH_L);
		GPUFREQ_LOGE("[VCORE_BUS_TRK][%llu] %s=0x%08x, %s=0x%08x", timestamp,
			"VCORE BUS_DBG_CON_0", vcore_bus_dbg_con,
			"TIMEOUT_INFO", DRV_Reg32(MFG_VCORE_BUS_TIMEOUT_INFO));
		timestamp = (unsigned long long)DRV_Reg32(MFG_VGPU_BUS_SYSTIMER_LATCH_H) << 32 |
			DRV_Reg32(MFG_VGPU_BUS_SYSTIMER_LATCH_L);
		GPUFREQ_LOGE("[VGPU_BUS_TRK][%llu] %s=0x%08x, %s=0x%08x", timestamp,
			"VGPU BUS_DBG_CON_0", vgpu_bus_dbg_con,
			"TIMEOUT_INFO", DRV_Reg32(MFG_VGPU_BUS_TIMEOUT_INFO));

		/* VCORE read timeout */
		if (vcore_bus_dbg_con & BIT(8)) {
			for (i = 0; i < 16; i++) {
				tracker_log = DRV_Reg32(MFG_VCORE_BUS_AR_TRACKER_LOG + (i * 4));
				if (!tracker_log)
					continue;
				tracker_id = DRV_Reg32(MFG_VCORE_BUS_AR_TRACKER_ID + (i * 4));
				tracker_addr = DRV_Reg32(MFG_VCORE_BUS_AR_TRACKER_L + (i * 4));
				GPUFREQ_LOGE("[VCORE_AR_%02d] %s=0x%08x, %s=0x%08x, %s=0x%08x", i,
					"LOG", tracker_log, "ID", tracker_id, "ADDR", tracker_addr);
				BUS_TRACKER_OP(
					g_bus_slv_timeout[g_slv_timeout_count % GPUFREQ_MAX_BUSTRK_NUM],
					g_slv_timeout_count, BUS_VCORE_AR, timestamp,
					tracker_log, tracker_id, tracker_addr);
			}
		}
		/* VCORE write timeout */
		if (vcore_bus_dbg_con & BIT(9)) {
			for (i = 0; i < 16; i++) {
				tracker_log = DRV_Reg32(MFG_VCORE_BUS_AW_TRACKER_LOG + (i * 4));
				if (!tracker_log)
					continue;
				tracker_id = DRV_Reg32(MFG_VCORE_BUS_AW_TRACKER_ID + (i * 4));
				tracker_addr = DRV_Reg32(MFG_VCORE_BUS_AW_TRACKER_L + (i * 4));
				GPUFREQ_LOGE("[VCORE_AW_%02d] %s=0x%08x, %s=0x%08x, %s=0x%08x", i,
					"LOG", tracker_log, "ID", tracker_id, "ADDR", tracker_addr);
				BUS_TRACKER_OP(
					g_bus_slv_timeout[g_slv_timeout_count % GPUFREQ_MAX_BUSTRK_NUM],
					g_slv_timeout_count, BUS_VCORE_AW, timestamp,
					tracker_log, tracker_id, tracker_addr);
			}
		}
		/* VGPU read timeout */
		if (vgpu_bus_dbg_con & BIT(8)) {
			for (i = 0; i < 32; i++) {
				tracker_log = DRV_Reg32(MFG_VGPU_BUS_AR_TRACKER_LOG + (i * 4));
				if (!tracker_log)
					continue;
				tracker_id = DRV_Reg32(MFG_VGPU_BUS_AR_TRACKER_ID + (i * 4));
				tracker_addr = DRV_Reg32(MFG_VGPU_BUS_AR_TRACKER_L + (i * 4));
				GPUFREQ_LOGE("[VGPU_AR_%02d] %s=0x%08x, %s=0x%08x, %s=0x%08x", i,
					"LOG", tracker_log, "ID", tracker_id, "ADDR", tracker_addr);
				BUS_TRACKER_OP(
					g_bus_slv_timeout[g_slv_timeout_count % GPUFREQ_MAX_BUSTRK_NUM],
					g_slv_timeout_count, BUS_VGPU_AR, timestamp,
					tracker_log, tracker_id, tracker_addr);
#if GPUFREQ_SLAVE_BUS_RECOVERY_ENABLE
				/* ignore broadcaster timeout error */
				if (tracker_log & BIT(31) && (tracker_id & GENMASK(1, 0)) == 0x1)
					fatal_slave_timeout = false;
#endif /* GPUFREQ_SLAVE_BUS_RECOVERY_ENABLE */
			}
		}
		/* VGPU write timeout */
		if (vgpu_bus_dbg_con & BIT(9)) {
			for (i = 0; i < 32; i++) {
				tracker_log = DRV_Reg32(MFG_VGPU_BUS_AW_TRACKER_LOG + (i * 4));
				if (!tracker_log)
					continue;
				tracker_id = DRV_Reg32(MFG_VGPU_BUS_AW_TRACKER_ID + (i * 4));
				tracker_addr = DRV_Reg32(MFG_VGPU_BUS_AW_TRACKER_L + (i * 4));
				GPUFREQ_LOGE("[VGPU_AW_%02d] %s=0x%08x, %s=0x%08x, %s=0x%08x", i,
					"LOG", tracker_log, "ID", tracker_id, "ADDR", tracker_addr);
				BUS_TRACKER_OP(
					g_bus_slv_timeout[g_slv_timeout_count % GPUFREQ_MAX_BUSTRK_NUM],
					g_slv_timeout_count, BUS_VGPU_AW, timestamp,
					tracker_log, tracker_id, tracker_addr);
#if GPUFREQ_SLAVE_BUS_RECOVERY_ENABLE
				/* ignore broadcaster timeout error */
				if (tracker_log & BIT(31) && (tracker_id & GENMASK(1, 0)) == 0x1)
					fatal_slave_timeout = false;
#endif /* GPUFREQ_SLAVE_BUS_RECOVERY_ENABLE */
			}
		}

		if (fatal_slave_timeout)
			__gpufreq_abort("VCORE/VGPU bus tracker violation");
	}

	/* VCORE/VGPU/GPUEB bus tracker error */
	/* VCORE read error */
	if (vcore_bus_dbg_con & BIT(12)) {
		timestamp = (unsigned long long)DRV_Reg32(MFG_VCORE_BUS_SYSTIMER_LATCH_SLVERR_H) << 32 |
			DRV_Reg32(MFG_VCORE_BUS_SYSTIMER_LATCH_SLVERR_L);
		BUS_TRACKER_OP(g_bus_slv_error[g_slv_error_count % GPUFREQ_MAX_BUSTRK_NUM],
			g_slv_error_count, BUS_VCORE_AR, timestamp,
			DRV_Reg32(MFG_VCORE_BUS_AR_SLVERR_LOG),
			DRV_Reg32(MFG_VCORE_BUS_AR_SLVERR_ID),
			DRV_Reg32(MFG_VCORE_BUS_AR_SLVERR_ADDR_L));
	}
	/* VCORE write error */
	if (vcore_bus_dbg_con & BIT(13)) {
		timestamp = (unsigned long long)DRV_Reg32(MFG_VCORE_BUS_SYSTIMER_LATCH_SLVERR_H) << 32 |
			DRV_Reg32(MFG_VCORE_BUS_SYSTIMER_LATCH_SLVERR_L);
		BUS_TRACKER_OP(g_bus_slv_error[g_slv_error_count % GPUFREQ_MAX_BUSTRK_NUM],
			g_slv_error_count, BUS_VCORE_AW, timestamp,
			DRV_Reg32(MFG_VCORE_BUS_AW_SLVERR_LOG),
			DRV_Reg32(MFG_VCORE_BUS_AW_SLVERR_ID),
			DRV_Reg32(MFG_VCORE_BUS_AW_SLVERR_ADDR_L));
	}
	/* VGPU read error */
	if (vgpu_bus_dbg_con & BIT(12)) {
		timestamp = (unsigned long long)DRV_Reg32(MFG_VGPU_BUS_SYSTIMER_LATCH_SLVERR_H) << 32 |
			DRV_Reg32(MFG_VGPU_BUS_SYSTIMER_LATCH_SLVERR_L);
		BUS_TRACKER_OP(g_bus_slv_error[g_slv_error_count % GPUFREQ_MAX_BUSTRK_NUM],
			g_slv_error_count, BUS_VGPU_AR, timestamp,
			DRV_Reg32(MFG_VGPU_BUS_AR_SLVERR_LOG),
			DRV_Reg32(MFG_VGPU_BUS_AR_SLVERR_ID),
			DRV_Reg32(MFG_VGPU_BUS_AR_SLVERR_ADDR_L));
	}
	/* VGPU write error */
	if (vgpu_bus_dbg_con & BIT(13)) {
		timestamp = (unsigned long long)DRV_Reg32(MFG_VGPU_BUS_SYSTIMER_LATCH_SLVERR_H) << 32 |
			DRV_Reg32(MFG_VGPU_BUS_SYSTIMER_LATCH_SLVERR_L);
		BUS_TRACKER_OP(g_bus_slv_error[g_slv_error_count % GPUFREQ_MAX_BUSTRK_NUM],
			g_slv_error_count, BUS_VGPU_AW, timestamp,
			DRV_Reg32(MFG_VGPU_BUS_AW_SLVERR_LOG),
			DRV_Reg32(MFG_VGPU_BUS_AW_SLVERR_ID),
			DRV_Reg32(MFG_VGPU_BUS_AW_SLVERR_ADDR_L));
	}
	/* GPUEB read error */
	if (gpueb_bus_dbg_con & BIT(12)) {
		timestamp = (unsigned long long)DRV_Reg32(MFG_GPUEB_BUS_SYSTIMER_LATCH_SLVERR_H) << 32 |
			DRV_Reg32(MFG_GPUEB_BUS_SYSTIMER_LATCH_SLVERR_L);
		BUS_TRACKER_OP(g_bus_slv_error[g_slv_error_count % GPUFREQ_MAX_BUSTRK_NUM],
			g_slv_error_count, BUS_GPUEB_AR, timestamp,
			DRV_Reg32(MFG_GPUEB_BUS_AR_SLVERR_LOG),
			DRV_Reg32(MFG_GPUEB_BUS_AR_SLVERR_ID),
			DRV_Reg32(MFG_GPUEB_BUS_AR_SLVERR_ADDR_L));
	}
	/* GPUEB write error */
	if (gpueb_bus_dbg_con & BIT(13)) {
		timestamp = (unsigned long long)DRV_Reg32(MFG_GPUEB_BUS_SYSTIMER_LATCH_SLVERR_H) << 32 |
			DRV_Reg32(MFG_GPUEB_BUS_SYSTIMER_LATCH_SLVERR_L);
		BUS_TRACKER_OP(g_bus_slv_error[g_slv_error_count % GPUFREQ_MAX_BUSTRK_NUM],
			g_slv_error_count, BUS_GPUEB_AW, timestamp,
			DRV_Reg32(MFG_GPUEB_BUS_AW_SLVERR_LOG),
			DRV_Reg32(MFG_GPUEB_BUS_AW_SLVERR_ID),
			DRV_Reg32(MFG_GPUEB_BUS_AW_SLVERR_ADDR_L));
	}

	/* update current status to shared memory */
	if (g_shared_status)
		ARRAY_ASSIGN(g_shared_status->bus_slv_error, g_bus_slv_error, GPUFREQ_MAX_BUSTRK_NUM);

	/* clear bus tracker IRQ */
	if (vcore_bus_dbg_con & GENMASK(13, 12)) {
		DRV_WriteReg32(MFG_VCORE_BUS_DBG_CON_0, BIT(7));
		DRV_WriteReg32(MFG_VCORE_BUS_DBG_CON_0, (BIT(7) | BIT(16)));
		DRV_WriteReg32(MFG_VCORE_BUS_DBG_CON_0, 0x0);
	}
	if (vgpu_bus_dbg_con & (GENMASK(13, 12) | GENMASK(9, 8))) {
		DRV_WriteReg32(MFG_VGPU_BUS_DBG_CON_0, BIT(7));
		DRV_WriteReg32(MFG_VGPU_BUS_DBG_CON_0, (BIT(7) | BIT(16)));
		DRV_WriteReg32(MFG_VGPU_BUS_DBG_CON_0, 0x0);
	}
	if (gpueb_bus_dbg_con & (GENMASK(13, 12) | GENMASK(9, 8))) {
		DRV_WriteReg32(MFG_GPUEB_BUS_DBG_CON_0, BIT(7));
		DRV_WriteReg32(MFG_GPUEB_BUS_DBG_CON_0, (BIT(7) | BIT(16)));
		DRV_WriteReg32(MFG_GPUEB_BUS_DBG_CON_0, 0x0);
	}

	/* power off GPUEB */
	ret = gpueb_ctrl(GHPM_OFF, MFG1_OFF, SUSPEND_POWER_OFF);
	if (ret) {
		__gpufreq_abort("gpueb power off fail, ret=%d", ret);
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

static unsigned int __gpufreq_get_fmeter_fgpu(void)
{
	int i = 0;
	unsigned int val = 0, ckgen_load_cnt = 0, ckgen_k1 = 0, freq = 0;

	/* Enable clock PLL_TST_CK */
	val = DRV_Reg32(MFG_PLL_CON0);
	DRV_WriteReg32(MFG_PLL_CON0, (val | BIT(12)));

	/* Enable RG_TST_CK_SEL */
	val = DRV_Reg32(MFG_PLL_CON5);
	DRV_WriteReg32(MFG_PLL_CON5, (val | BIT(4)));

	DRV_WriteReg32(MFG_PLL_FQMTR_CON1, GENMASK(23, 16));
	val = DRV_Reg32(MFG_PLL_FQMTR_CON0);
	DRV_WriteReg32(MFG_PLL_FQMTR_CON0, (val & GENMASK(23, 0)));
	/* Enable fmeter & select measure clock PLL_TST_CK */
	/* MFG_PLL_FQMTR_CON0 0x13FA0040 [1:0] = 2'b10, select brisket_out_ck */
	DRV_WriteReg32(MFG_PLL_FQMTR_CON0, (BIT(1) & ~BIT(0) | BIT(12) | BIT(15)));

	ckgen_load_cnt = DRV_Reg32(MFG_PLL_FQMTR_CON1) >> 16;
	ckgen_k1 = DRV_Reg32(MFG_PLL_FQMTR_CON0) >> 24;

	val = DRV_Reg32(MFG_PLL_FQMTR_CON0);
	DRV_WriteReg32(MFG_PLL_FQMTR_CON0, (val | BIT(4) | BIT(12)));

	/* wait fmeter finish */
	while (DRV_Reg32(MFG_PLL_FQMTR_CON0) & BIT(4)) {
		udelay(10);
		i++;
		if (i > 1000) {
			GPUFREQ_LOGE("wait MFG_TOP_PLL Fmeter timeout");
			break;
		}
	}

	val = DRV_Reg32(MFG_PLL_FQMTR_CON1) & GENMASK(15, 0);
	/* KHz */
	freq = (val * 26000 * (ckgen_k1 + 1)) / (ckgen_load_cnt + 1);

	return freq;
}

static unsigned int __gpufreq_get_fmeter_fstack(void)
{
	int i = 0;
	unsigned int val = 0, ckgen_load_cnt = 0, ckgen_k1 = 0, freq = 0;

	/* Enable clock PLL_TST_CK */
	val = DRV_Reg32(MFG_PLL_SC0_CON0);
	DRV_WriteReg32(MFG_PLL_SC0_CON0, (val | BIT(12)));

	/* Enable RG_TST_CK_SEL */
	val = DRV_Reg32(MFG_PLL_SC0_CON5);
	DRV_WriteReg32(MFG_PLL_SC0_CON5, (val | BIT(4)));

	DRV_WriteReg32(MFG_PLL_SC0_FQMTR_CON1, GENMASK(23, 16));
	val = DRV_Reg32(MFG_PLL_SC0_FQMTR_CON0);
	DRV_WriteReg32(MFG_PLL_SC0_FQMTR_CON0, (val & GENMASK(23, 0)));
	/* Enable fmeter & select measure clock PLL_TST_CK */
	DRV_WriteReg32(MFG_PLL_SC0_FQMTR_CON0, (BIT(12) | BIT(15)));

	ckgen_load_cnt = DRV_Reg32(MFG_PLL_SC0_FQMTR_CON1) >> 16;
	ckgen_k1 = DRV_Reg32(MFG_PLL_SC0_FQMTR_CON0) >> 24;

	val = DRV_Reg32(MFG_PLL_SC0_FQMTR_CON0);
	DRV_WriteReg32(MFG_PLL_SC0_FQMTR_CON0, (val | BIT(4) | BIT(12)));

	/* wait fmeter finish */
	while (DRV_Reg32(MFG_PLL_SC0_FQMTR_CON0) & BIT(4)) {
		udelay(10);
		i++;
		if (i > 1000) {
			GPUFREQ_LOGE("wait MFG_SC0_PLL Fmeter timeout");
			break;
		}
	}

	val = DRV_Reg32(MFG_PLL_SC0_FQMTR_CON1) & GENMASK(15, 0);
	/* KHz */
	freq = (val * 26000 * (ckgen_k1 + 1)) / (ckgen_load_cnt + 1);

	return freq;
}

/*
 * API: get current frequency from PLL CON1 (khz)
 * Freq = ((PLL_CON1[21:0] * 26M) / 2^14) / 2^PLL_CON1[26:24]
 */
static unsigned int __gpufreq_get_pll_fgpu(void)
{
	unsigned int con1 = 0;
	unsigned int posdiv = 0;
	unsigned long long freq = 0, pcw = 0;

	con1 = DRV_Reg32(MFG_PLL_CON1);
	pcw = con1 & GENMASK(21, 0);
	posdiv = (con1 & GENMASK(26, 24)) >> POSDIV_SHIFT;
	freq = (((pcw * 1000) * MFGPLL_FIN) >> DDS_SHIFT) / (1 << posdiv);

	return FREQ_ROUNDUP_TO_10((unsigned int)freq);
}

/*
 * API: get current frequency from PLL CON1 (khz)
 * Freq = ((PLL_CON1[21:0] * 26M) / 2^14) / 2^PLL_CON1[26:24]
 */
static unsigned int __gpufreq_get_pll_fstack(void)
{
	unsigned int con1 = 0;
	unsigned int posdiv = 0;
	unsigned long long freq = 0, pcw = 0;

	con1 = DRV_Reg32(MFG_PLL_SC0_CON1);
	pcw = con1 & GENMASK(21, 0);
	posdiv = (con1 & GENMASK(26, 24)) >> POSDIV_SHIFT;
	freq = (((pcw * 1000) * MFGPLL_FIN) >> DDS_SHIFT) / (1 << posdiv);

	return FREQ_ROUNDUP_TO_10((unsigned int)freq);
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

/* API: init reg base address and flavor config of the platform */
static int __gpufreq_init_platform_info(struct platform_device *pdev)
{
	struct device *gpufreq_dev = &pdev->dev;
	struct device_node *of_wrapper = NULL;
	struct resource *res = NULL;
	int ret = GPUFREQ_ENOENT;

	if (!gpufreq_dev) {
		GPUFREQ_LOGE("fail to find gpufreq device (ENOENT)");
		goto done;
	}

	of_wrapper = of_find_compatible_node(NULL, NULL, "mediatek,gpufreq_wrapper");
	if (!of_wrapper) {
		GPUFREQ_LOGE("fail to find gpufreq_wrapper of_node");
		goto done;
	}

	/* ignore return error and use default value if property doesn't exist */
	of_property_read_u32(gpufreq_dev->of_node, "aging-load", &g_aging_load);
	of_property_read_u32(gpufreq_dev->of_node, "mcl50-load", &g_mcl50_load);
	of_property_read_u32(of_wrapper, "gpueb-support", &g_gpueb_support);

	/* 0x48000000 */
	g_mali_base = __gpufreq_of_ioremap("mediatek,mali", 0);
	if (!g_mali_base) {
		GPUFREQ_LOGE("fail to ioremap MALI");
		goto done;
	}

	/* 0x48500000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_top_config");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_TOP_CONFIG");
		goto done;
	}
	g_mfg_top_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_top_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_TOP_CONFIG: 0x%llx", res->start);
		goto done;
	}

	/* 0x48600000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_smmu");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_SMMU");
		goto done;
	}
	g_mfg_smmu_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_smmu_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_SMMU: 0x%llx", res->start);
		goto done;
	}

	/* 0x48800000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_vgpu_bus_trk");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_VGPU_BUS_TRACKER");
		goto done;
	}
	g_mfg_vgpu_bus_trk_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_vgpu_bus_trk_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_VGPU_BUS_TRACKER: 0x%llx", res->start);
		goto done;
	}

	/* 0x4B190000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_gpueb_bus_trk");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_GPUEB_BUS_TRACKER");
		goto done;
	}
	g_mfg_eb_bus_trk_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_eb_bus_trk_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_GPUEB_BUS_TRACKER: 0x%llx", res->start);
		goto done;
	}

	/* 0x4B420300 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_acp_gals_top");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_ACP_GALS_TOP");
		goto done;
	}
	g_mfg_acp_gals_top_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_acp_gals_top_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_ACP_GALS_TOP: 0x%llx", res->start);
		goto done;
	}

	/* 0x4B800000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_rpc");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_RPC");
		goto done;
	}
	g_mfg_rpc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_rpc_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_RPC: 0x%llx", res->start);
		goto done;
	}

	/* 0x4B810000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL");
		goto done;
	}
	g_mfg_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_pll_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL: 0x%llx", res->start);
		goto done;
	}

	/* 0x4B810400 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll_sc0");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL_SC0");
		goto done;
	}
	g_mfg_pll_sc0_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_pll_sc0_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL_SC0: 0x%llx", res->start);
		goto done;
	}

	/* 0x4B860000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_vcore_ao_cfg");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_VCORE_AO_CFG");
		goto done;
	}
	g_mfg_vcore_ao_cfg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_vcore_ao_cfg_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_VCORE_AO_CFG: 0x%llx", res->start);
		goto done;
	}

	/* 0x4B910000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_vcore_bus_trk");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_VCORE_BUS_TRACKER");
		goto done;
	}
	g_mfg_vcore_bus_trk_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_vcore_bus_trk_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_VCORE_BUS_TRACKER: 0x%llx", res->start);
		goto done;
	}

	/* 0x11014000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "emi_infra_noncoh_gals");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource EMI_IFR_NONCOH_GALS");
		goto done;
	}
	g_emi_infra_noncoh_gals = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_emi_infra_noncoh_gals) {
		GPUFREQ_LOGE("fail to ioremap EMI_IFR_NONCOH_GALS: 0x%llx", res->start);
		goto done;
	}

	/* 0x11025000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "emi_infra_cfg");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource EMI_IFR_CFG");
		goto done;
	}
	emi_infra_cfg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!emi_infra_cfg_base) {
		GPUFREQ_LOGE("fail to ioremap EMI_IFR_CFG: 0x%llx", res->start);
		goto done;
	}

	/* 0x1102B000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "emi_infra_pdn_bcrm");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource EMI_IFR_PDN_BRCM");
		goto done;
	}
	g_emi_infra_pdn_bcrm_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_emi_infra_pdn_bcrm_base) {
		GPUFREQ_LOGE("fail to ioremap EMI_IFR_PDN_BRCM: 0x%llx", res->start);
		goto done;
	}

	/* 0x11037000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "emi_infra_acp_rsi");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource EMI_IFR_ACP_RSI");
		goto done;
	}
	g_emi_infra_acp_rsi_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_emi_infra_acp_rsi_base) {
		GPUFREQ_LOGE("fail to ioremap EMI_IFR_ACP_RSI: 0x%llx", res->start);
		goto done;
	}

	/* 0x1C004000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sleep");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource SLEEP");
		goto done;
	}
	g_sleep = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_sleep) {
		GPUFREQ_LOGE("fail to ioremap SLEEP: 0x%llx", res->start);
		goto done;
	}

	ret = GPUFREQ_SUCCESS;

done:
	return ret;
}

static int __gpufreq_init_bus_tracker_irq(struct platform_device *pdev)
{
	int bus_tracker_irq = 0;
	int ret = GPUFREQ_SUCCESS;

	bus_tracker_irq = platform_get_irq_byname(pdev, "BUS_TRACKER");
	if (bus_tracker_irq <= 0) {
		GPUFREQ_LOGE("fail to get BUS_TRACKER interrupt (%d)", bus_tracker_irq);
		return GPUFREQ_EINVAL;
	}

	ret = request_irq(bus_tracker_irq, __gpufreq_bus_tracker_irq_handler,
		irqd_get_trigger_type(irq_get_irq_data(bus_tracker_irq)) | IRQF_SHARED,
		dev_name(&pdev->dev), pdev);
	if (ret) {
		GPUFREQ_LOGE("fail to request BUS_TRACKER interrupt (%d)", ret);
		return ret;
	}

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

	/* defer probe when gpufreq wrapper isn't ready */
	if (!gpufreq_wrapper_ready()) {
		GPUFREQ_LOGE("gpufreq wrapper has not been probed, defer gpufreq platform probe");
		ret = -EPROBE_DEFER;
		goto done;
	}

	/* init footprint */
	__gpufreq_reset_footprint();

	/* init reg base address and flavor config of the platform in both AP and EB mode */
	ret = __gpufreq_init_platform_info(pdev);
	if (ret) {
		GPUFREQ_LOGE("fail to init platform info (%d)", ret);
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

	/* init bus tracker irq */
	ret = __gpufreq_init_bus_tracker_irq(pdev);
	if (ret) {
		GPUFREQ_LOGE("fail to init bus tracker irq (%d)", ret);
		goto done;
	}

	/* init gpuppm */
	ret = gpuppm_init(TARGET_STACK, g_gpueb_support);
	if (ret) {
		GPUFREQ_LOGE("fail to init gpuppm (%d)", ret);
		goto done;
	}

	g_gpufreq_ready = true;
	GPUFREQ_LOGI("gpufreq platform driver probe done");

done:
	return ret;
}

/* API: gpufreq driver remove */
static void __gpufreq_pdrv_remove(struct platform_device *pdev)
{

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
