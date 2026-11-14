// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 MediaTek Inc.
 */

/**
 * @file    gpufreq_mt6858.c
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
#include <gpufreq_mt6858.h>
#include <gpufreq_reg_mt6858.h>
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
/* bringup function */
static unsigned int __gpufreq_bringup(void);
static void __gpufreq_dump_bringup_status(struct platform_device *pdev);
/* whitebox function */
static void __gpufreq_wb_mfg1_slave_stress(void);
/* get function */
static unsigned int __gpufreq_get_fmeter_freq(enum gpufreq_target target);
static unsigned int __gpufreq_get_pll_fgpu(void);
static unsigned int __gpufreq_get_pll_fstack(void);
/* init function */
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

static void __iomem *g_mfg_rpc_base;
static void __iomem *g_mfg_top_base;
static void __iomem *g_nth_emicfg_base;
static void __iomem *g_nemi_mi32_smi_sub_base;
static void __iomem *g_nemi_mi33_smi_sub_base;
static void __iomem *g_emi_infra_pdn_bcrm_base;
static void __iomem *g_sleep;
static void __iomem *g_mali_base;
static void __iomem *g_mfg_pll4h_top_base;
static unsigned int g_gpueb_support;
static unsigned int g_gpufreq_ready;
static unsigned int g_wb_mfg1_slave_stress;
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
	if (!g_gpufreq_ready)
		return;
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"== [GPUFREQ INFRA STATUS] ==");
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x",
		"[MFG_GALS]",
		"NEMI_M0_RX", DRV_Reg32(MFG_EMI0_GALS_SLV_DBG),
		"NEMI_M1_RX", DRV_Reg32(MFG_EMI1_GALS_SLV_DBG));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%x, %s=0x%x",
		"[EMI]",
		"NEMI_M0_AXI_SLPPORT_IDLE", (unsigned int)(DRV_Reg32(EMI_IFR_NEMI_M0_AXI_SLPPORT_IDLE) & BIT(0)),
		"NEMI_M1_AXI_SLPPORT_IDLE", (unsigned int)(DRV_Reg32(EMI_IFR_NEMI_M1_AXI_SLPPORT_IDLE) & BIT(0)));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_SMI]",
		"NEMI_MI32_S0", DRV_Reg32(NEMI_MI32_SMI_DEBUG_S0),
		"NEMI_MI32_S1", DRV_Reg32(NEMI_MI32_SMI_DEBUG_S1),
		"NEMI_MI32_S2", DRV_Reg32(NEMI_MI32_SMI_DEBUG_S2),
		"NEMI_MI32_M0", DRV_Reg32(NEMI_MI32_SMI_DEBUG_M0),
		"NEMI_MI32_MISC", DRV_Reg32(NEMI_MI32_SMI_DEBUG_MISC));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_SMI]",
		"NEMI_MI33_S0", DRV_Reg32(NEMI_MI33_SMI_DEBUG_S0),
		"NEMI_MI33_S1", DRV_Reg32(NEMI_MI33_SMI_DEBUG_S1),
		"NEMI_MI33_M0", DRV_Reg32(NEMI_MI33_SMI_DEBUG_M0),
		"NEMI_MI33_MISC", DRV_Reg32(NEMI_MI33_SMI_DEBUG_MISC));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%x",
		"[EMI]",
		"NEMI_M0_M31_BUSY", (unsigned int)(DRV_Reg32(EMI_IFR_NEMI_M0_M31_BUSY_SIGNAL) & GENMASK(1, 0)));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08lx",
		"[MISC]",
		"SPM_SRC_REQ", DRV_Reg32(SPM_SRC_REQ),
		"SPM_MFG0_PWR_CON", DRV_Reg32(SPM_MFG0_PWR_CON),
		"PWR_STATUS", MFG_PWR_STATUS);
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[MTCMOS]",
		"MFG1", DRV_Reg32(MFG_RPC_MFG1_PWR_CON),
		"MFG2", DRV_Reg32(MFG_RPC_MFG2_PWR_CON),
		"MFG3", DRV_Reg32(MFG_RPC_MFG3_PWR_CON),
		"MFG5", DRV_Reg32(MFG_RPC_MFG5_PWR_CON),
		"MFG9", DRV_Reg32(MFG_RPC_MFG9_PWR_CON),
		"MFG10", DRV_Reg32(MFG_RPC_MFG10_PWR_CON));
}

void __gpufreq_dump_internal_status(char *log_buf, int *log_len, int log_size)
{
	unsigned int val1 = 0, val2 = 0;

	if (!g_gpufreq_ready)
		return;

	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"== [GPUFREQ INTERNAL STATUS] ==");
	/* MFG_TOP_DEBUG_SEL 0x13FBF170 [23:16] MFG_DEBUG_ASYNC_SEL = 2'h08 */
	DRV_WriteReg32(MFG_TOP_DEBUG_SEL, (DRV_Reg32(MFG_TOP_DEBUG_SEL) & ~GENMASK(23, 16)) | BIT(19));
	val1 = DRV_Reg32(MFG_TOP_DEBUG_ASYNC);
	/* MFG_TOP_DEBUG_SEL 0x13FBF170 [23:16] MFG_DEBUG_ASYNC_SEL = 2'h09 */
	DRV_WriteReg32(MFG_TOP_DEBUG_SEL, (DRV_Reg32(MFG_TOP_DEBUG_SEL) & ~GENMASK(23, 16)) | BIT(19) | BIT(16));
	val2 = DRV_Reg32(MFG_TOP_DEBUG_ASYNC);
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x",
		"[MFG_GALS]",
		"NEMI_M0_TX", val1,
		"NEMI_M1_TX", val2);

	__gpufreq_dump_power_tracker_status();
}

void __gpufreq_dump_shared_status(char *log_buf, int *log_len, int log_size)
{
	int cur_oppidx_gpu = 0, vgpu_diff = 0;
	unsigned int cur_vgpu = 0, opp_vgpu = 0;
	unsigned long long power_time = 0;

	if (!g_gpufreq_ready)
		return;

	if (g_shared_status) {
		GPUFREQ_LOGB(log_buf, log_len, log_size,
			"== [GPUFREQ SHARED STATUS] ==");

		power_time = g_shared_status->power_time_h;
		power_time = (power_time << 32) | g_shared_status->power_time_l;
		cur_oppidx_gpu = g_shared_status->cur_oppidx_gpu;
		cur_vgpu = g_shared_status->cur_vgpu;
		if (cur_oppidx_gpu >= 0 && cur_oppidx_gpu < g_shared_status->opp_num_gpu) {
			opp_vgpu = g_shared_status->working_table_gpu[cur_oppidx_gpu].volt;
			vgpu_diff = (int)cur_vgpu - (int)opp_vgpu;
		} else
			GPUFREQ_LOGE("abnormal cur_oppidx_gpu: %d", cur_oppidx_gpu);
		GPUFREQ_LOGB(log_buf, log_len, log_size,
			"DBGVer: 0x%08x, KDBGVer: 0x%08x, PTPVer: 0x%04x",
			g_shared_status->dbg_version, g_shared_status->kdbg_version,
			g_shared_status->ptp_version);
		GPUFREQ_LOGB(log_buf, log_len, log_size,
			"GPU[%d] Freq: %d/%d, Volt: %d (%d), Vsram: %d",
			g_shared_status->cur_oppidx_gpu, g_shared_status->cur_fgpu,
			g_shared_status->cur_out_fgpu, g_shared_status->cur_vgpu,
			vgpu_diff, g_shared_status->cur_vsram_gpu);
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

	/* 0x13000000 */
	g_mali_base = __gpufreq_of_ioremap("mediatek,mali", 0);
	if (!g_mali_base) {
		GPUFREQ_LOGE("fail to ioremap MALI");
		return;
	}
	/* 0x13FBF000 */
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
	/* 0x13F90000 */
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
	/* 0x13FA0000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll4h_top");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL4H_TOP");
		return;
	}
	g_mfg_pll4h_top_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_pll4h_top_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL4H_TOP: 0x%llx", res->start);
		return;
	}
	/* 0x1C001000 */
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
		"SPM_SPM2GPUPM_CON", DRV_Reg32(SPM_SPM2GPUPM_CON));
	GPUFREQ_LOGI("[MFG] %s=0x%08lx, %s=0x%08x, %s=0x%08x",
		"MFG_PWR_STATUS", MFG_PWR_STATUS,
		"SPM_MFG0_PWR_CON", DRV_Reg32(SPM_MFG0_PWR_CON),
		"MFG_RPC_MFG1_PWR_CON", DRV_Reg32(MFG_RPC_MFG1_PWR_CON));
	GPUFREQ_LOGI("[TOP] %s=0x%08x, %s=%d, %s=%d, %s=0x%08lx",
		"MFG_PLL4H_PLL1_CON0", DRV_Reg32(MFG_PLL4H_PLL1_CON0),
		"CON1", __gpufreq_get_pll_fgpu(),
		"FMETER", __gpufreq_get_fmeter_freq(TARGET_GPU),
		"SEL", DRV_Reg32(MFG_TOP_CKMUX_CON) & MFG_TOP_SEL_BIT);
	GPUFREQ_LOGI("[SC0] %s=0x%08x, %s=%d, %s=%d, %s=0x%08lx",
		"MFG_PLL4H_PLL4_CON0", DRV_Reg32(MFG_PLL4H_PLL4_CON0),
		"CON1", __gpufreq_get_pll_fstack(),
		"FMETER", __gpufreq_get_fmeter_freq(TARGET_STACK),
		"SEL", DRV_Reg32(MFG_TOP_CKMUX_CON) & MFG_SC0_SEL_BIT);
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
		w_ptr = (DRV_Reg32(MFG_TOP_POWER_TRACKER_SETTING) & GENMASK(13, 9)) >> 9;
		GPUFREQ_LOGI("== [PDC POWER TRACKER STATUS: %02u] ==", w_ptr);
		for (i = 1; i <= 16; i++) {
			/* only dump last 16 record */
			r_ptr = (w_ptr + ~i + 1) & GENMASK(4, 0);
			DRV_FieldReg32(MFG_TOP_POWER_TRACKER_SETTING, r_ptr, GENMASK(8, 4));
			udelay(1);

			GPUFREQ_LOGI("[%02u][%u] STA 1=0x%08x",
				r_ptr, DRV_Reg32(MFG_TOP_POWER_TRACKER_PDC_STATUS0),
				DRV_Reg32(MFG_TOP_POWER_TRACKER_PDC_STATUS1));
		}
	}
}

static unsigned int __gpufreq_get_fmeter_freq(enum gpufreq_target target)
{
	unsigned int val = 0, freq = 0;
	int i = 0;

	DRV_WriteReg32(MFG_PLL4H_FQMTR_CON0, 0x0);

	/* CLK26CALI_0[15]: rst 0 -> 1 */
	DRV_SetReg32(MFG_PLL4H_FQMTR_CON0, BIT(15));
	/*
	 * MFG_PLL4H_FQMTR_CON0 0x13FA0200
	 * [2:0] fqmtr_cksel = 0x0 or 0x3 (0: AD_PLL1_CK, 3: AD_PLL4_CK)
	 */
	val = DRV_Reg32(MFG_PLL4H_FQMTR_CON0);
	if(target == TARGET_GPU)
		val = ((val & ~GENMASK(2, 0)) | MFG_TOP_PLL_ID);
	else if(target == TARGET_STACK)
		val = ((val & ~GENMASK(2, 0)) | MFG_SC_PLL_ID);
	else {
		GPUFREQ_LOGI("[MFG] invalid target: %d", target);
		return 0;
	}
	DRV_WriteReg32(MFG_PLL4H_FQMTR_CON0, val);
	/* MFG_PLL4H_FQMTR_CON1 0x13FA0204 [25:16] ckgen_load_cnt = 0x1FF */
	DRV_SetReg32(MFG_PLL4H_FQMTR_CON1, GENMASK(24, 16));
	/* MFG_PLL4H_FQMTR_CON1 0x13FA0200 [31:24] ckgen_k1 = 0xFF */
	DRV_ClrReg32(MFG_PLL4H_FQMTR_CON0, GENMASK(31, 24));
	/*
	 * MFG_PLL4H_FQMTR_CON0 0x13FA0200
	 * [4] ckgen_tri_cal = 1'b1 enable [12] fmter_en = 1'b1 enable
	 */
	DRV_SetReg32(MFG_PLL4H_FQMTR_CON0, BIT(4) | BIT(12));

	/* wait fmeter finish */
	while (DRV_Reg32(MFG_PLL4H_FQMTR_CON0) & BIT(4)) {
		udelay(10);
		i++;
		if (i > 100) {
			GPUFREQ_LOGE("[MFG] wait MFGPLL Fmeter timeout");
			break;
		}
	}

	val = DRV_Reg32(MFG_PLL4H_FQMTR_CON1) & GENMASK(15,0);
	/* Khz */
	freq = ((val * 26000)) / 512;

	DRV_WriteReg32(MFG_PLL4H_FQMTR_CON0, BIT(15));

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

	con1 = DRV_Reg32(MFG_PLL4H_PLL1_CON1);
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

	con1 = DRV_Reg32(MFG_PLL4H_PLL4_CON1);
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
	of_property_read_u32(of_wrapper, "gpueb-support", &g_gpueb_support);

	/* 0x13000000 */
	g_mali_base = __gpufreq_of_ioremap("mediatek,mali", 0);
	if (!g_mali_base) {
		GPUFREQ_LOGE("fail to ioremap MALI");
		goto done;
	}

	/* 0x13F90000 */
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

	/* 0x13FBF000 */
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

	/* 0x1021C000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emicfg");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource NTH_EMICFG");
		goto done;
	}
	g_nth_emicfg_base  = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_nth_emicfg_base) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMICFG: 0x%llx", res->start);
		goto done;
	}

	/* 0x1025E000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nemi_mi32_smi_sub");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource NEMI_MI32_SMI_SUB");
		goto done;
	}
	g_nemi_mi32_smi_sub_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_nemi_mi32_smi_sub_base) {
		GPUFREQ_LOGE("fail to ioremap NEMI_MI32_SMI_SUB: 0x%llx", res->start);
		goto done;
	}

	/* 0x1025F000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nemi_mi33_smi_sub");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource NEMI_MI33_SMI_SUB");
		goto done;
	}
	g_nemi_mi33_smi_sub_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_nemi_mi33_smi_sub_base) {
		GPUFREQ_LOGE("fail to ioremap NEMI_MI33_SMI_SUB: 0x%llx", res->start);
		goto done;
	}

	/* 0x10276000 */
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

	/* 0x1C001000 */
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
