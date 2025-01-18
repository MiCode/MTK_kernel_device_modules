// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

/**
 * @file    gpufreq_mt6991.c
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
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/printk.h>

#include <gpufreq_v2.h>
#include <gpuppm.h>
#include <gpufreq_common.h>
#include <gpufreq_mt6991.h>
#include <gpufreq_reg_mt6991.h>

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
static unsigned int __gpufreq_get_fmeter_fstack0(void);
static unsigned int __gpufreq_get_fmeter_fstack1(void);
static unsigned int __gpufreq_get_pll_fgpu(void);
static unsigned int __gpufreq_get_pll_fstack0(void);
static unsigned int __gpufreq_get_pll_fstack1(void);
/* init function */
static int __gpufreq_init_platform_info(struct platform_device *pdev);
static int __gpufreq_pdrv_probe(struct platform_device *pdev);
static int __gpufreq_pdrv_remove(struct platform_device *pdev);

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

static void __iomem *g_cksys_base;
static void __iomem *g_emi_base;
static void __iomem *g_emi_infra_ao_bcrm_base;
static void __iomem *g_emi_infra_ao_mem_base;
static void __iomem *g_emi_infra_cfg_base;
static void __iomem *g_infra_ao_debug_ctrl;
static void __iomem *g_mali_base;
static void __iomem *g_mcusys_par_wrap_base;
static void __iomem *g_mfg_hbvc_base;
static void __iomem *g_mfg_top_base;
static void __iomem *g_mfg_pll_base;
static void __iomem *g_mfg_pll_sc0_base;
static void __iomem *g_mfg_pll_sc1_base;
static void __iomem *g_mfg_rpc_base;
static void __iomem *g_mfg_smmu_base;
static void __iomem *g_mfg_vcore_ao_cfg_base;
static void __iomem *g_mfg_vcore_devapc_base;
static void __iomem *g_mfg_vcore_dbg_trk_base;
static void __iomem *g_mfg_vgpu_dbg_trk_base;
static void __iomem *g_nemi_mi32_smi;
static void __iomem *g_nemi_mi33_smi;
static void __iomem *g_nth_emi_ao_debug_ctrl;
static void __iomem *g_nth_emicfg_base;
static void __iomem *g_nth_emicfg_ao_mem_base;
static void __iomem *g_semi_mi32_smi;
static void __iomem *g_semi_mi33_smi;
static void __iomem *g_sth_emi_ao_debug_ctrl;
static void __iomem *g_sth_emicfg_base;
static void __iomem *g_sth_emicfg_ao_mem_base;
static void __iomem *g_sub_emi_base;
static void __iomem *g_sleep;
static unsigned int g_gpueb_support;
static unsigned int g_shader_present;
static unsigned int g_mcl50_load;
static unsigned int g_aging_load;
static unsigned int g_gpufreq_ready;
static DEFINE_MUTEX(gpufreq_lock);

static struct gpufreq_platform_fp platform_eb_fp = {
	.dump_infra_status = __gpufreq_dump_infra_status,
	.dump_power_tracker_status = __gpufreq_dump_power_tracker_status,
	.dump_dbg_tracker_status = __gpufreq_dump_dbg_tracker_status,
	.get_dyn_pgpu = __gpufreq_get_dyn_pgpu,
	.get_dyn_pstack = __gpufreq_get_dyn_pstack,
	.get_core_mask_table = __gpufreq_get_core_mask_table,
	.get_core_num = __gpufreq_get_core_num,
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
	unsigned long long p_dynamic = STACK_DYN_REF_POWER;
	unsigned int ref_freq = STACK_DYN_REF_POWER_FREQ;
	unsigned int ref_volt = STACK_DYN_REF_POWER_VOLT;

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

void __gpufreq_dump_infra_status(char *log_buf, int *log_len, int log_size)
{
	if (!g_gpufreq_ready)
		return;

	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"== [GPUFREQ INFRA STATUS] ==");

	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[BUS_PROT]",
		"EMI_SLPPROT_EN_SET", DRV_Reg32(EMI_SLPPROT_EN_SET),
		"MFG_RPC_SLV_SLP_PROT_RDY", DRV_Reg32(MFG_RPC_SLV_SLP_PROT_RDY_STA),
		"MFG_RPC_SLV_WAY_EN_SET", DRV_Reg32(MFG_RPC_SLV_WAY_EN_SET),
		"MFG_RPC_SLV_WAY_EN_CLR", DRV_Reg32(MFG_RPC_SLV_WAY_EN_CLR),
		"MFG_RPC_SLV_CTRL_UPDATE", DRV_Reg32(MFG_RPC_SLV_CTRL_UPDATE));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[BUS_PROT]",
		"SPM_BUS_PROTECT_MSB_CON_SET", DRV_Reg32(SPM_BUS_PROTECT_MSB_CON_SET),
		"SPM_BUS_PROTECT_MSB_CON_CLR", DRV_Reg32(SPM_BUS_PROTECT_MSB_CON_CLR),
		"SPM_BUS_PROTECT_RDY_MSB", DRV_Reg32(SPM_BUS_PROTECT_RDY_MSB),
		"EMISYS_PROT_RDY", DRV_Reg32(VDNR_PWR_PROT_EMI_INFRA_BUS_SPM_CTRL_5));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[NEMI_THRO]",
		"URGENT_CNT", DRV_Reg32(EMI_URGENT_CNT),
		"MD_LAT", DRV_Reg32(EMI_MD_LAT_HRT_URGENT_CNT),
		"MD", DRV_Reg32(EMI_MD_HRT_URGENT_CNT),
		"DISP", DRV_Reg32(EMI_DISP_HRT_URGENT_CNT),
		"CAM", DRV_Reg32(EMI_CAM_HRT_URGENT_CNT),
		"MDMCU", DRV_Reg32(EMI_MDMCU_HRT_URGENT_CNT));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[NEMI_THRO]",
		"MD_WR_LAT", DRV_Reg32(EMI_MD_WR_LAT_HRT_URGENT_CNT),
		"MDMCU_LOW", DRV_Reg32(EMI_MDMCU_LOW_LAT_URGENT_CNT),
		"MDMCU_HIGH", DRV_Reg32(EMI_MDMCU_HIGH_LAT_URGENT_CNT),
		"MDMCU_LOW_WR", DRV_Reg32(EMI_MDMCU_LOW_WR_LAT_URGENT_CNT),
		"MDMCU_HIGH_WR", DRV_Reg32(EMI_MDMCU_HIGH_WR_LAT_URGENT_CNT));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[SEMI_THRO]",
		"URGENT_CNT", DRV_Reg32(SUB_EMI_URGENT_CNT),
		"MD_LAT", DRV_Reg32(SUB_EMI_MD_LAT_HRT_URGENT_CNT),
		"MD", DRV_Reg32(SUB_EMI_MD_HRT_URGENT_CNT),
		"DISP", DRV_Reg32(SUB_EMI_DISP_HRT_URGENT_CNT),
		"CAM", DRV_Reg32(SUB_EMI_CAM_HRT_URGENT_CNT),
		"MDMCU", DRV_Reg32(SUB_EMI_MDMCU_HRT_URGENT_CNT));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[SEMI_THRO]",
		"MD_WR_LAT", DRV_Reg32(SUB_EMI_MD_WR_LAT_HRT_URGENT_CNT),
		"MDMCU_LOW", DRV_Reg32(SUB_EMI_MDMCU_LOW_LAT_URGENT_CNT),
		"MDMCU_HIGH", DRV_Reg32(SUB_EMI_MDMCU_HIGH_LAT_URGENT_CNT),
		"MDMCU_LOW_WR", DRV_Reg32(SUB_EMI_MDMCU_LOW_WR_LAT_URGENT_CNT),
		"MDMCU_HIGH_WR", DRV_Reg32(SUB_EMI_MDMCU_HIGH_WR_LAT_URGENT_CNT));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_GALS]",
		"NTH_MFG_EMI1_GALS_SLV", DRV_Reg32(NTH_MFG_EMI1_GALS_SLV_DBG),
		"NTH_MFG_EMI0_GALS_SLV", DRV_Reg32(NTH_MFG_EMI0_GALS_SLV_DBG),
		"STH_MFG_EMI1_GALS_SLV", DRV_Reg32(STH_MFG_EMI1_GALS_SLV_DBG),
		"STH_MFG_EMI0_GALS_SLV", DRV_Reg32(STH_MFG_EMI0_GALS_SLV_DBG));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_GALS]",
		"NTH_APU_EMI1_GALS_SLV", DRV_Reg32(NTH_APU_EMI1_GALS_SLV_DBG),
		"NTH_APU_EMI0_GALS_SLV", DRV_Reg32(NTH_APU_EMI0_GALS_SLV_DBG),
		"STH_APU_EMI1_GALS_SLV", DRV_Reg32(STH_APU_EMI1_GALS_SLV_DBG),
		"STH_APU_EMI0_GALS_SLV", DRV_Reg32(STH_APU_EMI0_GALS_SLV_DBG));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_M6M7]",
		"NTH_M6M7_IDLE_1", DRV_Reg32(NTH_AO_M6M7_IDLE_BIT_EN_1),
		"NTH_M6M7_IDLE_0", DRV_Reg32(NTH_AO_M6M7_IDLE_BIT_EN_0),
		"STH_M6M7_IDLE_1", DRV_Reg32(STH_AO_M6M7_IDLE_BIT_EN_1),
		"STH_M6M7_IDLE_0", DRV_Reg32(STH_AO_M6M7_IDLE_BIT_EN_0));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_PROT]",
		"NTH_SLEEP_PROT", DRV_Reg32(NTH_AO_SLEEP_PROT_START),
		"NTH_GLITCH_PROT", DRV_Reg32(NTH_AO_GLITCH_PROT_START),
		"STH_SLEEP_PROT", DRV_Reg32(STH_AO_SLEEP_PROT_START),
		"STH_GLITCH_PROT", DRV_Reg32(STH_AO_GLITCH_PROT_START));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_M6M7_PROT]",
		"M6M7_IDLE_1", DRV_Reg32(M6M7_IDLE_BIT_EN_1),
		"M6M7_IDLE_0", DRV_Reg32(M6M7_IDLE_BIT_EN_0),
		"SLEEP_PROT", DRV_Reg32(EMI_SLEEP_PROT_START),
		"GLITCH_PROT", DRV_Reg32(EMI_GLITCH_PROT_START));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_SMI]",
		"NEMI_MI32_SMI_S0", DRV_Reg32(NEMI_MI32_SMI_DEBUG_S0),
		"NEMI_MI32_SMI_S1", DRV_Reg32(NEMI_MI32_SMI_DEBUG_S1),
		"NEMI_MI32_SMI_S2", DRV_Reg32(NEMI_MI32_SMI_DEBUG_S2),
		"NEMI_MI32_SMI_M0", DRV_Reg32(NEMI_MI32_SMI_DEBUG_M0),
		"NEMI_MI32_SMI_MISC", DRV_Reg32(NEMI_MI32_SMI_DEBUG_MISC));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_SMI]",
		"NEMI_MI33_SMI_S0", DRV_Reg32(NEMI_MI33_SMI_DEBUG_S0),
		"NEMI_MI33_SMI_S1", DRV_Reg32(NEMI_MI33_SMI_DEBUG_S1),
		"NEMI_MI33_SMI_S2", DRV_Reg32(NEMI_MI33_SMI_DEBUG_S2),
		"NEMI_MI33_SMI_M0", DRV_Reg32(NEMI_MI33_SMI_DEBUG_M0),
		"NEMI_MI33_SMI_MISC", DRV_Reg32(NEMI_MI33_SMI_DEBUG_MISC));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_SMI]",
		"SEMI_MI32_SMI_S0", DRV_Reg32(SEMI_MI32_SMI_DEBUG_S0),
		"SEMI_MI32_SMI_S1", DRV_Reg32(SEMI_MI32_SMI_DEBUG_S1),
		"SEMI_MI32_SMI_S2", DRV_Reg32(SEMI_MI32_SMI_DEBUG_S2),
		"SEMI_MI32_SMI_M0", DRV_Reg32(SEMI_MI32_SMI_DEBUG_M0),
		"SEMI_MI32_SMI_MISC", DRV_Reg32(SEMI_MI32_SMI_DEBUG_MISC));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_SMI]",
		"SEMI_MI33_SMI_S0", DRV_Reg32(SEMI_MI33_SMI_DEBUG_S0),
		"SEMI_MI33_SMI_S1", DRV_Reg32(SEMI_MI33_SMI_DEBUG_S1),
		"SEMI_MI33_SMI_S2", DRV_Reg32(SEMI_MI33_SMI_DEBUG_S2),
		"SEMI_MI33_SMI_M0", DRV_Reg32(SEMI_MI33_SMI_DEBUG_M0),
		"SEMI_MI33_SMI_MISC", DRV_Reg32(SEMI_MI33_SMI_DEBUG_MISC));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_DBG]",
		"INFRA_BUS0_DBG_CTRL", DRV_Reg32(INFRA_AO_BUS0_U_DEBUG_CTRL0),
		"NTH_EMI_BUS_DBG_CTRL", DRV_Reg32(NTH_EMI_AO_BUS_U_DEBUG_CTRL0),
		"STH_EMI_BUS_DBG_CTRL", DRV_Reg32(STH_EMI_AO_BUS_U_DEBUG_CTRL0));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[MFG_ACP]",
		"MCUSYS_ACP_GALS", DRV_Reg32(MCUSYS_PAR_WRAP_ACP_GALS_DBG),
		"NTH_DVM_GALS_MST", DRV_Reg32(NTH_MFG_ACP_DVM_GALS_MST_DBG),
		"NTH_GALS_SLV", DRV_Reg32(NTH_MFG_ACP_GALS_SLV_DBG),
		"STH_DVM_GALS_MST", DRV_Reg32(STH_MFG_ACP_DVM_GALS_MST_DBG),
		"STH_GALS_SLV", DRV_Reg32(STH_MFG_ACP_GALS_SLV_DBG));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08lx",
		"[MISC]",
		"SPM_SRC_REQ", DRV_Reg32(SPM_SRC_REQ),
		"SPM_SOC_BUCK_ISO", DRV_Reg32(SPM_SOC_BUCK_ISO_CON),
		"PWR_STATUS", MFG_0_23_37_PWR_STATUS);
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[HBVC_TOP]",
		"GRP0_BACKEND0", DRV_Reg32(MFG_HBVC_GRP0_DBG_BACKEND0),
		"GRP0_BACKEND1", DRV_Reg32(MFG_HBVC_GRP0_DBG_BACKEND1),
		"FLL0_FRONTEND0", DRV_Reg32(MFG_HBVC_FLL0_DBG_FRONTEND0),
		"FLL0_FRONTEND1", DRV_Reg32(MFG_HBVC_FLL0_DBG_FRONTEND1));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[HBVC_STACK]",
		"GRP1_BACKEND0", DRV_Reg32(MFG_HBVC_GRP1_DBG_BACKEND0),
		"GRP1_BACKEND1", DRV_Reg32(MFG_HBVC_GRP1_DBG_BACKEND1),
		"FLL1_FRONTEND0", DRV_Reg32(MFG_HBVC_FLL1_DBG_FRONTEND0),
		"FLL1_FRONTEND1", DRV_Reg32(MFG_HBVC_FLL1_DBG_FRONTEND1));
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
		"MFG7", DRV_Reg32(MFG_RPC_MFG7_PWR_CON),
		"MFG8", DRV_Reg32(MFG_RPC_MFG8_PWR_CON),
		"MFG23", DRV_Reg32(MFG_RPC_MFG23_PWR_CON),
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
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[NEMI_THRO]",
		"URGENT_CNT", DRV_Reg32(EMI_URGENT_CNT),
		"MD_LAT", DRV_Reg32(EMI_MD_LAT_HRT_URGENT_CNT),
		"MD", DRV_Reg32(EMI_MD_HRT_URGENT_CNT),
		"DISP", DRV_Reg32(EMI_DISP_HRT_URGENT_CNT),
		"CAM", DRV_Reg32(EMI_CAM_HRT_URGENT_CNT),
		"MDMCU", DRV_Reg32(EMI_MDMCU_HRT_URGENT_CNT));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[NEMI_THRO]",
		"MD_WR_LAT", DRV_Reg32(EMI_MD_WR_LAT_HRT_URGENT_CNT),
		"MDMCU_LOW", DRV_Reg32(EMI_MDMCU_LOW_LAT_URGENT_CNT),
		"MDMCU_HIGH", DRV_Reg32(EMI_MDMCU_HIGH_LAT_URGENT_CNT),
		"MDMCU_LOW_WR", DRV_Reg32(EMI_MDMCU_LOW_WR_LAT_URGENT_CNT),
		"MDMCU_HIGH_WR", DRV_Reg32(EMI_MDMCU_HIGH_WR_LAT_URGENT_CNT));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[SEMI_THRO]",
		"URGENT_CNT", DRV_Reg32(SUB_EMI_URGENT_CNT),
		"MD_LAT", DRV_Reg32(SUB_EMI_MD_LAT_HRT_URGENT_CNT),
		"MD", DRV_Reg32(SUB_EMI_MD_HRT_URGENT_CNT),
		"DISP", DRV_Reg32(SUB_EMI_DISP_HRT_URGENT_CNT),
		"CAM", DRV_Reg32(SUB_EMI_CAM_HRT_URGENT_CNT),
		"MDMCU", DRV_Reg32(SUB_EMI_MDMCU_HRT_URGENT_CNT));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[SEMI_THRO]",
		"MD_WR_LAT", DRV_Reg32(SUB_EMI_MD_WR_LAT_HRT_URGENT_CNT),
		"MDMCU_LOW", DRV_Reg32(SUB_EMI_MDMCU_LOW_LAT_URGENT_CNT),
		"MDMCU_HIGH", DRV_Reg32(SUB_EMI_MDMCU_HIGH_LAT_URGENT_CNT),
		"MDMCU_LOW_WR", DRV_Reg32(SUB_EMI_MDMCU_LOW_WR_LAT_URGENT_CNT),
		"MDMCU_HIGH_WR", DRV_Reg32(SUB_EMI_MDMCU_HIGH_WR_LAT_URGENT_CNT));
}

void __gpufreq_dump_power_tracker_status(void)
{
	int i = 0;
	unsigned int val = 0, cur_point = 0, read_point = 0;

	if (!g_gpufreq_ready)
		return;

	cur_point = (DRV_Reg32(MFG_POWER_TRACKER_SETTING) >> 10) & GENMASK(5, 0);
	GPUFREQ_LOGI("== [PDC POWER TRAKER STATUS: %d] ==", cur_point);
	for (i = 1; i <= 16; i++) {
		/* only dump last 16 record */
		read_point = (cur_point + ~i + 1) & GENMASK(5, 0);
		val = (DRV_Reg32(MFG_POWER_TRACKER_SETTING) & ~GENMASK(9, 4)) | (read_point << 4);
		DRV_WriteReg32(MFG_POWER_TRACKER_SETTING, val);
		udelay(1);

		GPUFREQ_LOGI("[%d][%d] STA 1=0x%08x, 2=0x%08x, 3=0x%08x, 4=0x%08x, 5=0x%08x",
			read_point, DRV_Reg32(MFG_POWER_TRACKER_PDC_STATUS0),
			DRV_Reg32(MFG_POWER_TRACKER_PDC_STATUS1),
			DRV_Reg32(MFG_POWER_TRACKER_PDC_STATUS2),
			DRV_Reg32(MFG_POWER_TRACKER_PDC_STATUS3),
			DRV_Reg32(MFG_POWER_TRACKER_PDC_STATUS4),
			DRV_Reg32(MFG_POWER_TRACKER_PDC_STATUS5));
	}
}

bool __gpufreq_dump_dbg_tracker_status(void)
{
	bool ret = false;
	int i = 0;
	unsigned int vio_sta = 0, vcore_bus_dbg_con = 0, vgpu_bus_dbg_con = 0;

	if (!g_gpufreq_ready)
		return ret;

	vio_sta = DRV_Reg32(MFG_VCORE_DEVAPC_D0_VIO_STA_0);
	/* check bus tracker status */
	GPUFREQ_LOGI("[VCORE DEVAPC] %s=0x%08x", "D0_VIO_STA_0", vio_sta);

	if (vio_sta & BIT(26)) {
		/* check bus tracker violation status */
		vcore_bus_dbg_con = DRV_Reg32(MFG_VCORE_DBG_CON_0);
		vgpu_bus_dbg_con= DRV_Reg32(MFG_VGPU_DBG_CON_0);

		GPUFREQ_LOGI("[DBG_TRK] %s=0x%08x, %s=0x%08x",
				"VCORE BUS_DBG_CON_0", vcore_bus_dbg_con, "VGPU BUS_DBG_CON_0", vgpu_bus_dbg_con);

		/* bus transaction timeout */
		if (vcore_bus_dbg_con & GENMASK(9, 8) || vgpu_bus_dbg_con & GENMASK(9, 8)) {
			GPUFREQ_LOGE("[VCORE_DBG_TRK] %s=0x%08x, %s=0x%08x, %s=0x%08x",
				"TIMER_L", DRV_Reg32(MFG_VCORE_DBG_SYSTIMER_LATCH_L),
				"TIMER_H", DRV_Reg32(MFG_VCORE_DBG_SYSTIMER_LATCH_H),
				"TIMEOUT_INFO", DRV_Reg32(MFG_VCORE_DBG_TIMEOUT_INFO));
			GPUFREQ_LOGE("[VGPU_DBG_TRK] %s=0x%08x, %s=0x%08x, %s=0x%08x",
				"TIMER_L", DRV_Reg32(MFG_VGPU_DBG_SYSTIMER_LATCH_L),
				"TIMER_H", DRV_Reg32(MFG_VGPU_DBG_SYSTIMER_LATCH_H),
				"TIMEOUT_INFO", DRV_Reg32(MFG_VGPU_DBG_TIMEOUT_INFO));
			/* vgpu dbg tracker read timeout info */
			if (vgpu_bus_dbg_con & BIT(8)) {
				for (i = 0; i < 32; i++) {
					GPUFREQ_LOGE("[VGPU_AR_%d] %s=0x%08x, %s=0x%08x, %s=0x%08x", i,
						"LOG", DRV_Reg32(MFG_VGPU_DBG_AR_TRACKER_LOG + (i * 4)),
						"ID", DRV_Reg32(MFG_VGPU_DBG_AR_TRACKER_ID + (i * 4)),
						"ADDR", DRV_Reg32(MFG_VGPU_DBG_AR_TRACKER_L + (i * 4)));
				}
			}
			/* vgpu dbg tracker write timeout info */
			if (vgpu_bus_dbg_con & BIT(9)) {
				for (i = 0; i < 32; i++) {
					GPUFREQ_LOGE("[VGPU_AW_%d] %s=0x%08x, %s=0x%08x, %s=0x%08x", i,
						"LOG", DRV_Reg32(MFG_VGPU_DBG_AW_TRACKER_LOG + (i * 4)),
						"ID", DRV_Reg32(MFG_VGPU_DBG_AW_TRACKER_ID + (i * 4)),
						"ADDR", DRV_Reg32(MFG_VGPU_DBG_AW_TRACKER_L + (i * 4)));
				}
			}
			/* vcore dbg tracker read timeout info */
			if (vcore_bus_dbg_con & BIT(8)) {
				for (i = 0; i < 16; i++) {
					GPUFREQ_LOGE("[VCORE_AR_%d] %s=0x%08x, %s=0x%08x, %s=0x%08x", i,
						"LOG", DRV_Reg32(MFG_VCORE_DBG_AR_TRACKER_LOG + (i * 4)),
						"ID", DRV_Reg32(MFG_VCORE_DBG_AR_TRACKER_ID + (i * 4)),
						"ADDR", DRV_Reg32(MFG_VCORE_DBG_AR_TRACKER_L + (i * 4)));
				}
			}
			/* vcore dbg tracker write timeout info */
			if (vcore_bus_dbg_con & BIT(9)) {
				for (i = 0; i < 16; i++) {
					GPUFREQ_LOGE("[VCORE_AW_%d] %s=0x%08x, %s=0x%08x, %s=0x%08x", i,
						"LOG", DRV_Reg32(MFG_VCORE_DBG_AW_TRACKER_LOG + (i * 4)),
						"ID", DRV_Reg32(MFG_VCORE_DBG_AW_TRACKER_ID + (i * 4)),
						"ADDR", DRV_Reg32(MFG_VCORE_DBG_AW_TRACKER_L + (i * 4)));
				}
			}
			__gpufreq_abort("debug tracker violation");
		}
		/* vcore bus read slave error */
		if (vcore_bus_dbg_con & BIT(12)) {
			GPUFREQ_LOGI("[VCORE_SLV_TIMER] %s=0x%08x, %s=0x%08x",
				"SYSTIMER_LATCH_L", DRV_Reg32(MFG_VCORE_DBG_SYSTIMER_LATCH_SLVERR_L),
				"SYSTIMER_LATCH_H", DRV_Reg32(MFG_VCORE_DBG_SYSTIMER_LATCH_SLVERR_H));
			GPUFREQ_LOGI("[VCORE_AR_SLV] %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
				"SLVERR_ADDR_L", DRV_Reg32(MFG_VCORE_DBG_AR_SLVERR_ADDR_L),
				"SLVERR_ADDR_H", DRV_Reg32(MFG_VCORE_DBG_AR_SLVERR_ADDR_H),
				"SLVERR_ID", DRV_Reg32(MFG_VCORE_DBG_AR_SLVERR_ID),
				"SLVERR_LOG", DRV_Reg32(MFG_VCORE_DBG_AR_SLVERR_LOG));
		}
		/* vcore bus write slave error */
		if (vcore_bus_dbg_con & BIT(13)) {
			GPUFREQ_LOGI("[VCORE_SLV_TIMER] %s=0x%08x, %s=0x%08x",
				"SYSTIMER_LATCH_L", DRV_Reg32(MFG_VCORE_DBG_SYSTIMER_LATCH_SLVERR_L),
				"SYSTIMER_LATCH_H", DRV_Reg32(MFG_VCORE_DBG_SYSTIMER_LATCH_SLVERR_H));
			GPUFREQ_LOGI("[VCORE_AW_SLV] %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
				"SLVERR_ADDR_L", DRV_Reg32(MFG_VCORE_DBG_AW_SLVERR_ADDR_L),
				"SLVERR_ADDR_H", DRV_Reg32(MFG_VCORE_DBG_AW_SLVERR_ADDR_H),
				"SLVERR_ID", DRV_Reg32(MFG_VCORE_DBG_AW_SLVERR_ID),
				"SLVERR_LOG", DRV_Reg32(MFG_VCORE_DBG_AW_SLVERR_LOG));
		}
		/* vgpu bus read slave error */
		if (vgpu_bus_dbg_con & BIT(12)) {
			GPUFREQ_LOGI("[VGPU_SLV_TIMER] %s=0x%08x, %s=0x%08x",
				"SYSTIMER_LATCH_L", DRV_Reg32(MFG_VGPU_DBG_SYSTIMER_LATCH_SLVERR_L),
				"SYSTIMER_LATCH_H", DRV_Reg32(MFG_VGPU_DBG_SYSTIMER_LATCH_SLVERR_H));
			GPUFREQ_LOGI("[VGPU_AR_SLV] %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
				"SLVERR_ADDR_L", DRV_Reg32(MFG_VGPU_DBG_AR_SLVERR_ADDR_L),
				"SLVERR_ADDR_H", DRV_Reg32(MFG_VGPU_DBG_AR_SLVERR_ADDR_H),
				"SLVERR_ID", DRV_Reg32(MFG_VGPU_DBG_AR_SLVERR_ID),
				"SLVERR_LOG", DRV_Reg32(MFG_VGPU_DBG_AR_SLVERR_LOG));
		}
		/* vgpu bus write slave error */
		if (vgpu_bus_dbg_con & BIT(13)) {
			GPUFREQ_LOGI("[VGPU_SLV_TIMER] %s=0x%08x, %s=0x%08x",
				"SYSTIMER_LATCH_L", DRV_Reg32(MFG_VGPU_DBG_SYSTIMER_LATCH_SLVERR_L),
				"SYSTIMER_LATCH_H", DRV_Reg32(MFG_VGPU_DBG_SYSTIMER_LATCH_SLVERR_H));
			GPUFREQ_LOGI("[VGPU_AW_SLV] %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
				"SLVERR_ADDR_L", DRV_Reg32(MFG_VGPU_DBG_AW_SLVERR_ADDR_L),
				"SLVERR_ADDR_H", DRV_Reg32(MFG_VGPU_DBG_AW_SLVERR_ADDR_H),
				"SLVERR_ID", DRV_Reg32(MFG_VGPU_DBG_AW_SLVERR_ID),
				"SLVERR_LOG", DRV_Reg32(MFG_VGPU_DBG_AW_SLVERR_LOG));
		}
		/* clear dbg tracker irq */
		if (vcore_bus_dbg_con & GENMASK(13, 12)) {
			DRV_WriteReg32(MFG_VCORE_DBG_CON_0, BIT(7));
			DRV_WriteReg32(MFG_VCORE_DBG_CON_0, (BIT(7) | BIT(16)));
			DRV_WriteReg32(MFG_VCORE_DBG_CON_0, 0x0);
		}
		if (vgpu_bus_dbg_con & GENMASK(13, 12)) {
			DRV_WriteReg32(MFG_VGPU_DBG_CON_0, BIT(7));
			DRV_WriteReg32(MFG_VGPU_DBG_CON_0, (BIT(7) | BIT(16)));
			DRV_WriteReg32(MFG_VGPU_DBG_CON_0, 0x0);
		}
		/* clear devapc irq when only bus slave error happened */
		if (!(vio_sta & ~BIT(26))) {
			DRV_WriteReg32(MFG_VCORE_DEVAPC_D0_VIO_STA_0, BIT(26));
			DRV_WriteReg32(MFG_VCORE_DEVAPC_D0_VIO_MASK_0, 0x0);
			ret = true;
		}
	}
	return ret;
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

/**
 * ===============================================
 * Internal Function Definition
 * ===============================================
 */
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
	/* 0x4B810800 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll_sc1");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL_SC1");
		return;
	}
	g_mfg_pll_sc1_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_pll_sc1_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL_SC1: 0x%llx", res->start);
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
	/* 0x10000000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cksys");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource CKSYS");
		return;
	}
	g_cksys_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_cksys_base) {
		GPUFREQ_LOGE("fail to ioremap CKSYS: 0x%llx", res->start);
		return;
	}
	/* 0x10645000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "emi_infra_ao_bcrm");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource EMI_INFRA_AO_BCRM");
		return;
	}
	g_emi_infra_ao_bcrm_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_emi_infra_ao_bcrm_base) {
		GPUFREQ_LOGE("fail to ioremap EMI_INFRA_AO_BCRM: 0x%llx", res->start);
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
		"MFG_0_23_37_PWR_STATUS", MFG_0_23_37_PWR_STATUS,
		"MFG0_PWR_CON", DRV_Reg32(MFG_RPC_MFG0_PWR_CON),
		"MFG1_PWR_CON", DRV_Reg32(MFG_RPC_MFG1_PWR_CON),
		"SMMU_CR0", DRV_Reg32(MFG_SMMU_CR0),
		"SMMU_GBPA", DRV_Reg32(MFG_SMMU_GBPA));
	GPUFREQ_LOGI("[MFG] %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"MFG_DREQ", DRV_Reg32(MFG_VCORE_AO_RPC_DREQ_CONFIG),
		"MFG_DEFAULT_DELSEL", DRV_Reg32(MFG_DEFAULT_DELSEL_00),
		"MFG_TOP_DELSEL", DRV_Reg32(MFG_SRAM_FUL_SEL_ULV_TOP),
		"MFG_STACK_DELSEL", DRV_Reg32(MFG_SRAM_FUL_SEL_ULV));
	GPUFREQ_LOGI("[MFG] %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"BRISKET_TOP", DRV_Reg32(MFG_RPC_BRISKET_TOP_AO_CFG_0),
		"BRISKET_ST0", DRV_Reg32(MFG_BRISKET_ST0_AO_CFG_0),
		"BRISKET_ST1", DRV_Reg32(MFG_BRISKET_ST1_AO_CFG_0),
		"BRISKET_ST2", DRV_Reg32(MFG_BRISKET_ST2_AO_CFG_0));
	GPUFREQ_LOGI("[MFG] %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"BRISKET_ST5", DRV_Reg32(MFG_BRISKET_ST5_AO_CFG_0),
		"BRISKET_ST6", DRV_Reg32(MFG_BRISKET_ST6_AO_CFG_0),
		"BRISKET_ST7", DRV_Reg32(MFG_BRISKET_ST7_AO_CFG_0));
	GPUFREQ_LOGI("[TOP] %s=0x%08x, %s=%d, %s=%d, %s=0x%08lx, %s=0x%08lx",
		"PLL_CON0", DRV_Reg32(MFG_PLL_CON0),
		"CON1", __gpufreq_get_pll_fgpu(),
		"FMETER", __gpufreq_get_fmeter_fgpu(),
		"SEL", DRV_Reg32(MFG_VCORE_AO_CK_FAST_REF_SEL) & MFG_TOP_SEL_BIT,
		"REF_SEL", DRV_Reg32(CKSYS_CLK_CFG_6) & MFG_REF_SEL_BIT);
	GPUFREQ_LOGI("[SC0] %s=0x%08x, %s=%d, %s=%d, %s=0x%08lx",
		"PLL_CON0", DRV_Reg32(MFG_PLL_SC0_CON0),
		"CON1", __gpufreq_get_pll_fstack0(),
		"FMETER", __gpufreq_get_fmeter_fstack0(),
		"SEL", DRV_Reg32(MFG_VCORE_AO_CK_FAST_REF_SEL) & MFG_SC0_SEL_BIT);
	GPUFREQ_LOGI("[SC1] %s=0x%08x, %s=%d, %s=%d, %s=0x%08lx",
		"PLL_CON0", DRV_Reg32(MFG_PLL_SC1_CON0),
		"CON1", __gpufreq_get_pll_fstack1(),
		"FMETER", __gpufreq_get_fmeter_fstack1(),
		"SEL", DRV_Reg32(MFG_VCORE_AO_CK_FAST_REF_SEL) & MFG_SC1_SEL_BIT);
	GPUFREQ_LOGI("[GPU] %s=0x%08x",
		"MALI_GPU_ID", DRV_Reg32(MALI_GPU_ID));
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

static unsigned int __gpufreq_get_fmeter_fstack0(void)
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

static unsigned int __gpufreq_get_fmeter_fstack1(void)
{
	unsigned int val = 0, ckgen_load_cnt = 0, ckgen_k1 = 0;
	int i = 0;
	unsigned int freq = 0;

	/* Enable clock PLL_TST_CK */
	val = DRV_Reg32(MFG_PLL_SC1_CON0);
	DRV_WriteReg32(MFG_PLL_SC1_CON0, (val | BIT(12)));

	/* Enable RG_TST_CK_SEL */
	val = DRV_Reg32(MFG_PLL_SC1_CON5);
	DRV_WriteReg32(MFG_PLL_SC1_CON5, (val | BIT(4)));

	DRV_WriteReg32(MFG_PLL_SC1_FQMTR_CON1, GENMASK(23, 16));
	val = DRV_Reg32(MFG_PLL_SC1_FQMTR_CON0);
	DRV_WriteReg32(MFG_PLL_SC1_FQMTR_CON0, (val & GENMASK(23, 0)));
	/* Enable fmeter & select measure clock PLL_TST_CK */
	DRV_WriteReg32(MFG_PLL_SC1_FQMTR_CON0, (BIT(12) | BIT(15)));

	ckgen_load_cnt = DRV_Reg32(MFG_PLL_SC1_FQMTR_CON1) >> 16;
	ckgen_k1 = DRV_Reg32(MFG_PLL_SC1_FQMTR_CON0) >> 24;

	val = DRV_Reg32(MFG_PLL_SC1_FQMTR_CON0);
	DRV_WriteReg32(MFG_PLL_SC1_FQMTR_CON0, (val | BIT(4) | BIT(12)));

	/* wait fmeter finish */
	while (DRV_Reg32(MFG_PLL_SC1_FQMTR_CON0) & BIT(4)) {
		udelay(10);
		i++;
		if (i > 1000) {
			GPUFREQ_LOGE("wait MFG_SC0_PLL Fmeter timeout");
			break;
		}
	}

	val = DRV_Reg32(MFG_PLL_SC1_FQMTR_CON1) & GENMASK(15, 0);
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
static unsigned int __gpufreq_get_pll_fstack0(void)
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

/*
 * API: get current frequency from PLL CON1 (khz)
 * Freq = ((PLL_CON1[21:0] * 26M) / 2^14) / 2^PLL_CON1[26:24]
 */
static unsigned int __gpufreq_get_pll_fstack1(void)
{
	unsigned int con1 = 0;
	unsigned int posdiv = 0;
	unsigned long long freq = 0, pcw = 0;

	con1 = DRV_Reg32(MFG_PLL_SC1_CON1);
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
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_vgpu_dbg_trk");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_VGPU_DBG_TRACKER_BASE");
		goto done;
	}
	g_mfg_vgpu_dbg_trk_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_vgpu_dbg_trk_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_VGPU_DBG_TRACKER_BASE: 0x%llx", res->start);
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

	/* 0x4B810800 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll_sc1");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL_SC1");
		goto done;
	}
	g_mfg_pll_sc1_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_pll_sc1_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL_SC1: 0x%llx", res->start);
		goto done;
	}

	/* 0x4B840000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_hbvc");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_HBVC");
		goto done;
	}
	g_mfg_hbvc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_hbvc_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_HBVC: 0x%llx", res->start);
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

	/* 0x4B8B0000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_vcore_devapc");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_VCORE_DEVAPC");
		goto done;
	}
	g_mfg_vcore_devapc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_vcore_devapc_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_VCORE_DEVAPC: 0x%llx", res->start);
		goto done;
	}

	/* 0x4B900000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_vcore_dbg_trk");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_VCORE_DBG_TRACKER_BASE");
		goto done;
	}
	g_mfg_vcore_dbg_trk_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_vcore_dbg_trk_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_VCORE_DBG_TRACKER_BASE: 0x%llx", res->start);
		goto done;
	}

	/* 0x0C000000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mcusys_par_wrap");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MCUSYS_PAR_WRAP");
		goto done;
	}
	g_mcusys_par_wrap_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mcusys_par_wrap_base)) {
		GPUFREQ_LOGE("fail to ioremap MCUSYS_PAR_WRAP: 0x%llx", res->start);
		goto done;
	}

	/* 0x10000000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cksys");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource CKSYS");
		goto done;
	}
	g_cksys_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_cksys_base) {
		GPUFREQ_LOGE("fail to ioremap CKSYS: 0x%llx", res->start);
		goto done;
	}

	/* 0x10404000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emicfg_ao_mem");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource NTH_EMICFG_AO_MEM");
		goto done;
	}
	g_nth_emicfg_ao_mem_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_nth_emicfg_ao_mem_base) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMICFG_AO_MEM: 0x%llx", res->start);
		goto done;
	}

	/* 0x10416000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emi_ao_debug_ctrl");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource NTH_EMI_AO_DEBUG_CTRL");
		goto done;
	}
	g_nth_emi_ao_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_nth_emi_ao_debug_ctrl) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMI_AO_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

	/* 0x10425000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emicfg");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource NTH_EMICFG");
		goto done;
	}
	g_nth_emicfg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_nth_emicfg_base) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMICFG: 0x%llx", res->start);
		goto done;
	}

	/* 0x10469000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "emi");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource EMI");
		goto done;
	}
	g_emi_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_emi_base) {
		GPUFREQ_LOGE("fail to ioremap EMI: 0x%llx", res->start);
		goto done;
	}

	/* 0x10504000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sth_emicfg_ao_mem");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource STH_EMICFG_AO_MEM");
		goto done;
	}
	g_sth_emicfg_ao_mem_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_sth_emicfg_ao_mem_base) {
		GPUFREQ_LOGE("fail to ioremap STH_EMICFG_AO_MEM: 0x%llx", res->start);
		goto done;
	}

	/* 0x10516000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sth_emi_ao_debug_ctrl");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource STH_EMI_AO_DEBUG_CTRL");
		goto done;
	}
	g_sth_emi_ao_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_sth_emi_ao_debug_ctrl) {
		GPUFREQ_LOGE("fail to ioremap STH_EMI_AO_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

	/* 0x10525000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sth_emicfg");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource STH_EMICFG");
		goto done;
	}
	g_sth_emicfg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_sth_emicfg_base) {
		GPUFREQ_LOGE("fail to ioremap STH_EMICFG: 0x%llx", res->start);
		goto done;
	}

	/* 0x10569000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sub_emi");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource SUB_EMI");
		goto done;
	}
	g_sub_emi_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_sub_emi_base) {
		GPUFREQ_LOGE("fail to ioremap SUB_EMI: 0x%llx", res->start);
		goto done;
	}

	/* 0x10621000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "semi_mi32_smi");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource SEMI_MI32_SMI");
		goto done;
	}
	g_semi_mi32_smi = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_semi_mi32_smi) {
		GPUFREQ_LOGE("fail to ioremap SEMI_MI32_SMI: 0x%llx", res->start);
		goto done;
	}

	/* 0x10622000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nemi_mi32_smi");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource NEMI_MI32_SMI");
		goto done;
	}
	g_nemi_mi32_smi = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_nemi_mi32_smi) {
		GPUFREQ_LOGE("fail to ioremap NEMI_MI32_SMI: 0x%llx", res->start);
		goto done;
	}

	/* 0x10623000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "semi_mi33_smi");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource SEMI_MI33_SMI");
		goto done;
	}
	g_semi_mi33_smi = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_semi_mi33_smi) {
		GPUFREQ_LOGE("fail to ioremap SEMI_MI33_SMI: 0x%llx", res->start);
		goto done;
	}

	/* 0x10624000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nemi_mi33_smi");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource NEMI_MI33_SMI");
		goto done;
	}
	g_nemi_mi33_smi = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_nemi_mi33_smi) {
		GPUFREQ_LOGE("fail to ioremap NEMI_MI33_SMI: 0x%llx", res->start);
		goto done;
	}

	/* 0x10644000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "infra_ao_debug_ctrl");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource INFRA_AO_DEBUG_CTRL");
		goto done;
	}
	g_infra_ao_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_infra_ao_debug_ctrl) {
		GPUFREQ_LOGE("fail to ioremap INFRA_AO_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

	/* 0x10645000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "emi_infra_ao_bcrm");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource EMI_INFRA_AO_BCRM");
		goto done;
	}
	g_emi_infra_ao_bcrm_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_emi_infra_ao_bcrm_base) {
		GPUFREQ_LOGE("fail to ioremap EMI_INFRA_AO_BCRM: 0x%llx", res->start);
		goto done;
	}

	/* 0x10646000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "emi_infra_ao_mem");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource EMI_INFRA_AO_MEM");
		goto done;
	}
	g_emi_infra_ao_mem_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_emi_infra_ao_mem_base) {
		GPUFREQ_LOGE("fail to ioremap EMI_INFRA_AO_MEM: 0x%llx", res->start);
		goto done;
	}

	/* 0x10648000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "emi_infra_cfg");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource EMI_INFRA_CFG");
		goto done;
	}
	g_emi_infra_cfg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_emi_infra_cfg_base) {
		GPUFREQ_LOGE("fail to ioremap EMI_INFRA_CFG: 0x%llx", res->start);
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
	if (ret) {
		GPUFREQ_LOGE("fail to init platform info (%d)", ret);
		goto done;
	}

	if (!g_gpueb_support) {
		GPUFREQ_LOGE("gpufreq not support AP mode");
		ret = GPUFREQ_EINVAL;
		goto done;
	} else
		GPUFREQ_LOGI("gpufreq platform probe only init reg/fp in EB mode");

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

	g_gpufreq_ready = true;
	GPUFREQ_LOGI("gpufreq platform driver probe done");

done:
	return ret;
}

/* API: gpufreq driver remove */
static int __gpufreq_pdrv_remove(struct platform_device *pdev)
{
	return GPUFREQ_SUCCESS;
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
