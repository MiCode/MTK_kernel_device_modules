// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/proc_fs.h>
#include <linux/kallsyms.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>
#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/of_gpio.h>
#include "mtk_battery_oc_throttling.h"
#include "mtk_low_battery_throttling.h"
#include "mtk_bp_thl.h"
#include "mtk_peak_power_budget_mbrain.h"
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include <mcupm_ipi_id.h>
#if !IS_ENABLED(CONFIG_MTK_GPU_LEGACY)
#include <include/gpueb_ipi.h>
#endif

#define CREATE_TRACE_POINTS
#include "mtk_peak_power_budget_trace.h"
#include "mtk_cg_peak_power_throttling_table.h"
#include "mtk_cg_peak_power_throttling_core.h"
#include "mtk_peak_power_budget_cgppt.h"

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
#include <linux/scmi_protocol.h>
#include <tinysys-scmi.h>
#endif
#include "mtk_peak_power_budget.h"
#include <gpufreq_v2.h>

#define STR_SIZE 1024
#define MAX_VALUE 0x7FFF
#define MAX_POWER_DRAM 4000
#define MAX_POWER_DISPLAY 2000
#define SOC_ERROR 3000
#define BAT_CIRCUIT_DEFAULT_RDC 55
#define BAT_PATH_DEFAULT_RDC 100
#define BAT_PATH_DEFAULT_RAC 50
#define PPB_IPI_TIMEOUT_MS    3000U
#define PPB_IPI_DATA_LEN (sizeof(struct ppb_ipi_data) / sizeof(int))
#define MBRAIN_NOTIFY_BUDGET_THD    50000
#define PPB_LOG_DURATION msecs_to_jiffies(20000)
#define PPB3_LOG_DURATION msecs_to_jiffies(10000)
#define HPT_INIT_SETTING    7
#define DEFAULT_COMBO0_UISOC MAX_VALUE


static bool mt_ppb_debug;
static spinlock_t ppb_lock;
void __iomem *ppb_sram_base;
void __iomem *ppb3_dbg_base;
void __iomem *hpt_ctrl_base;
void __iomem *hpt_status_base;
void __iomem *gpu_dbg_base;
void __iomem *cpu_dbg_base;
void __iomem *spbm_csram_base;
void __iomem *spbm_cputcm_base;

struct fg_cus_data fg_data;
struct power_budget_t pb;
static struct notifier_block ppb_nb;
#if !IS_ENABLED(CONFIG_MTK_GPU_LEGACY)
static int channel_id;
static unsigned int ack_data;
#endif
static unsigned int bcpu_hpt_freq, mcpu_hpt_freq, lcpu_hpt_freq, dcpu_hpt_freq, gpu_hpt_freq, apu_hpt_freq;
struct xpu_dbg_t last_mbrain_xpu_dbg, last_klog_xpu_dbg;
static ppb_mbrain_func cb_func;
static ppb_mbrain_func cb_func_hpt;
static struct timer_list ppb_dbg_timer;
struct ppb_cgppt_dbg_operation *cgppt_dbg_ops;

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

struct ppb_ctrl ppb_ctrl = {
	.ppb_stop = 0,
	.ppb_drv_done = 0,
	.manual_mode = 0,
	.ppb_mode = 1,
};

struct ppb ppb = {
	.loading_flash = 0,
	.loading_audio = 0,
	.loading_camera = 0,
	.loading_display = MAX_POWER_DISPLAY,
	.loading_apu = 0,
	.loading_dram = MAX_POWER_DRAM,
	.vsys_budget = 0,
	.vsys_budget_noerr = 0,
	.cg_budget_thd = 0,
	.cg_budget_cnt = 0,
};

struct ppb ppb_manual = {
	.loading_flash = 0,
	.loading_audio = 0,
	.loading_camera = 0,
	.loading_display = 0,
	.loading_apu = 0,
	.loading_dram = 0,
	.vsys_budget = 0,
	.vsys_budget_noerr = 0,
	.cg_budget_thd = 0,
	.cg_budget_cnt = 0,
};

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
static int scmi_spbm_id;
static struct scmi_tinysys_info_st *_tinfo;
static DEFINE_MUTEX(spbm_scmi_mutex);
static int spbm_sspm_ready;
int spbm_ipi_ackdata;
struct spbm_scmi_state_t spbm_scmi_state;

#endif

static int spbm_scmi_to_sspm_command(unsigned int cmd, unsigned int p1, unsigned int p2,
			unsigned int p3, unsigned int p4)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	int ret = 0, ackdata = 0;
	struct scmi_tinysys_status rvalue = {0};

	mutex_lock(&spbm_scmi_mutex);

	if (cmd >= NR_SPBM_SCMI) {
		pr_info("spbm ipi cmd get error %d\n",
			cmd);
		goto error;
	}

	if (spbm_sspm_ready != 1) {
		pr_info("spbm ipi not ready, skip cmd=%d\n", cmd);
		goto error;
	}

	switch (p4) {
	case SPBM_SCMI_SET:
		ret = scmi_tinysys_common_set(_tinfo->ph, scmi_spbm_id,
			cmd, p1, p2, p3, p4);
		if (ret) {
			pr_info("spbm ipi cmd %d send fail ret %d\n",
			cmd, ret);
			goto error;
		}
		pr_info("spbm send ipi to sspm cmd %d success\n", cmd);
		ackdata = rvalue.r1;
		break;
	case SPBM_SCMI_GET:
		break;
	}
	mutex_unlock(&spbm_scmi_mutex);
	return ackdata;
error:
	mutex_unlock(&spbm_scmi_mutex);
#endif
	return -1;
}
static void spbm_scmi_init(void)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	unsigned int ret;

	_tinfo = get_scmi_tinysys_info();
	if (!_tinfo) {
		pr_info("spbm get scmi info fail\n");
		return;
	}

	ret = of_property_read_u32(_tinfo->sdev->dev.of_node, "scmi-spbm",
			&scmi_spbm_id);
	if (ret) {
		pr_info("get scmi-spbm fail, ret %d\n", ret);
		spbm_sspm_ready = -2;
		return;
	}

	spbm_sspm_ready = 1;
	pr_info("%s ready!\n", __func__);
#endif
}

static int spbm_intf_read_csram_s32(int offset)
{
	void __iomem *addr = spbm_csram_base + offset;

	if (!spbm_csram_base) {
		pr_info("spbm_csram_base error %p\n", spbm_csram_base);
		return 0;
	}

	return sign_extend32(readl(addr), 31);
}

static int spbm_intf_read_cputcm_s32(int offset)
{
	void __iomem *addr_cputcm = spbm_cputcm_base + offset;
	if (!spbm_cputcm_base) {
		pr_info("spbm_cputcm_base error %p\n", spbm_cputcm_base);
		return 0;
	}
	return sign_extend32(readl(addr_cputcm), 31);
}

static ssize_t spbm_cpu_info_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
		"%u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u\n",
		spbm_intf_read_cputcm_s32(SPBM_CPU_BCORE_TGT_PWR_TCM_OFFSET),
		spbm_intf_read_cputcm_s32(SPBM_CPU_MCORE_TGT_PWR_TCM_OFFSET),
		spbm_intf_read_cputcm_s32(SPBM_CPU_LCORE_TGT_PWR_TCM_OFFSET),

		spbm_intf_read_cputcm_s32(SPBM_CPU_BCORE_AVG_PWR_TCM_OFFSET),
		spbm_intf_read_cputcm_s32(SPBM_CPU_MCORE_AVG_PWR_TCM_OFFSET),
		spbm_intf_read_cputcm_s32(SPBM_CPU_LCORE_AVG_PWR_TCM_OFFSET),

		spbm_intf_read_cputcm_s32(SPBM_CPU_BCORE_DB_I_TCM_OFFSET),
		spbm_intf_read_cputcm_s32(SPBM_CPU_MCORE_DB_I_TCM_OFFSET),
		spbm_intf_read_cputcm_s32(SPBM_CPU_LCORE_DB_I_TCM_OFFSET),

		spbm_intf_read_cputcm_s32(SPBM_CPU_BCORE_DB_C_TCM_OFFSET),
		spbm_intf_read_cputcm_s32(SPBM_CPU_MCORE_DB_C_TCM_OFFSET),
		spbm_intf_read_cputcm_s32(SPBM_CPU_LCORE_DB_C_TCM_OFFSET),

		spbm_intf_read_cputcm_s32(SPBM_CPU_BCORE_DB_TSO_TCM_OFFSET),
		spbm_intf_read_cputcm_s32(SPBM_CPU_MCORE_DB_TSO_TCM_OFFSET),
		spbm_intf_read_cputcm_s32(SPBM_CPU_LCORE_DB_TSO_TCM_OFFSET));

	if (len < 0) {
		pr_info("[spbm]%s Error writing to buffer\n", __func__);
		return len; // Return error code
	} else if (len >= PAGE_SIZE) {
		pr_info("[spbm]%s Buffer size is too small, truncated output\n", __func__);
		len = PAGE_SIZE - 1; // Ensure len does not exceed PAGE_SIZE
	}

	return len;
}



static ssize_t spbm_gpu_info_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%u, %u, %u, %u\n",
		spbm_intf_read_csram_s32(SPBM_GPU_FREQ_OFFSET),
		spbm_intf_read_csram_s32(SPBM_GPU_AVG_CURRENT_OFFSET),
		spbm_intf_read_csram_s32(SPBM_GPU_TARGET_PWR_OFFSET),
		spbm_intf_read_csram_s32(SPBM_GPU_THROTTLED_OFFSET));

	if (len < 0) {
		pr_info("[spbm]%s Error writing to buffer\n", __func__);
		return len; // Return error code
	} else if (len >= PAGE_SIZE) {
		pr_info("[spbm]%s Buffer size is too small, truncated output\n", __func__);
		len = PAGE_SIZE - 1; // Ensure len does not exceed PAGE_SIZE
	}

	return len;
}


static ssize_t spbm_npu_info_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%u, %u, %u, %u\n",
		spbm_intf_read_csram_s32(SPBM_NPU_FREQ_OFFSET),
		spbm_intf_read_csram_s32(SPBM_NPU_AVG_CURRENT_OFFSET),
		spbm_intf_read_csram_s32(SPBM_NPU_TARGET_PWR_OFFSET),
		spbm_intf_read_csram_s32(SPBM_NPU_THROTTLED_OFFSET));

	if (len < 0) {
		pr_info("[spbm]%s Error writing to buffer\n", __func__);
		return len; // Return error code
	} else if (len >= PAGE_SIZE) {
		pr_info("[spbm]%s Buffer size is too small, truncated output\n", __func__);
		len = PAGE_SIZE - 1; // Ensure len does not exceed PAGE_SIZE
	}

	return len;
}

static ssize_t spbm_current_reporting_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
		"%u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u\n",
		spbm_intf_read_csram_s32(SPBM_BCPU_V_OFFSET),
		spbm_intf_read_csram_s32(SPBM_BCPU_I_OFFSET),
		spbm_intf_read_csram_s32(SPBM_BCPU_W_OFFSET),
		spbm_intf_read_csram_s32(SPBM_MCPU_V_OFFSET),
		spbm_intf_read_csram_s32(SPBM_MCPU_I_OFFSET),
		spbm_intf_read_csram_s32(SPBM_MCPU_W_OFFSET),
		spbm_intf_read_csram_s32(SPBM_LCPU_V_OFFSET),
		spbm_intf_read_csram_s32(SPBM_LCPU_I_OFFSET),
		spbm_intf_read_csram_s32(SPBM_LCPU_W_OFFSET),
		spbm_intf_read_csram_s32(SPBM_GPU_V_OFFSET),
		spbm_intf_read_csram_s32(SPBM_GPU_I_OFFSET),
		spbm_intf_read_csram_s32(SPBM_GPU_W_OFFSET),
		spbm_intf_read_csram_s32(SPBM_NPU_V_OFFSET),
		spbm_intf_read_csram_s32(SPBM_NPU_I_OFFSET),
		spbm_intf_read_csram_s32(SPBM_NPU_W_OFFSET)
		);

	if (len < 0) {
		pr_info("[spbm]%s Error writing to buffer\n", __func__);
		return len; // Return error code
	} else if (len >= PAGE_SIZE) {
		pr_info("[spbm]%s Buffer size is too small, truncated output\n", __func__);
		len = PAGE_SIZE - 1; // Ensure len does not exceed PAGE_SIZE
	}

	return len;
}

static ssize_t spbm_xpu_power_limit_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
		"%u, %u, %u, %u, %u, %u, %u\n",
		spbm_intf_read_csram_s32(SPBM_BAT_PWR_OFFSET),
		spbm_intf_read_csram_s32(SPBM_UVLO_TRIGGER_TIMES_OFFSET),
		spbm_intf_read_csram_s32(SPBM_BCPU_PWR_LIMIT_OFFSET),
		spbm_intf_read_csram_s32(SPBM_MCPU_PWR_LIMIT_OFFSET),
		spbm_intf_read_csram_s32(SPBM_LCPU_PWR_LIMIT_OFFSET),
		spbm_intf_read_csram_s32(SPBM_GPU_PWR_LIMIT_OFFSET),
		spbm_intf_read_csram_s32(SPBM_NPU_PWR_LIMIT_OFFSET));

	if (len < 0) {
		pr_info("[spbm]%s Error writing to buffer\n", __func__);
		return len; // Return error code
	} else if (len >= PAGE_SIZE) {
		pr_info("[spbm]%s Buffer size is too small, truncated output\n", __func__);
		len = PAGE_SIZE - 1; // Ensure len does not exceed PAGE_SIZE
	}

	return len;
}

static ssize_t spbm_xpu_request_power_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%u, %u, %u, %u, %u\n",
		spbm_intf_read_csram_s32(SPBM_BCPU_REQUEST_PWR_OFFSET),
		spbm_intf_read_csram_s32(SPBM_MCPU_REQUEST_PWR_OFFSET),
		spbm_intf_read_csram_s32(SPBM_LCPU_REQUEST_PWR_OFFSET),
		spbm_intf_read_csram_s32(SPBM_GPU_REQUEST_PWR_OFFSET),
		spbm_intf_read_csram_s32(SPBM_NPU_REQUEST_PWR_OFFSET));

	if (len < 0) {
		pr_info("[spbm]%s Error writing to buffer\n", __func__);
		return len; // Return error code
	} else if (len >= PAGE_SIZE) {
		pr_info("[spbm]%s Buffer size is too small, truncated output\n", __func__);
		len = PAGE_SIZE - 1; // Ensure len does not exceed PAGE_SIZE
	}

	return len;
}

static ssize_t spbm_debug_state_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%u\n",
		spbm_scmi_state.debug);

	if (len < 0) {
		pr_info("[spbm]%s Error writing to buffer\n", __func__);
		return len; // Return error code
	} else if (len >= PAGE_SIZE) {
		pr_info("[spbm]%s Buffer size is too small, truncated output\n", __func__);
		len = PAGE_SIZE - 1; // Ensure len does not exceed PAGE_SIZE
	}

	return len;
}

static ssize_t spbm_debug_state_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char cmd[20];
	unsigned int debug_state;

	if (sscanf(buf, "%11s %u", cmd, &debug_state) == 2) {
		if (strncmp(cmd, "spbm_debug", 10) == 0) {
			spbm_scmi_state.debug = debug_state;

			spbm_scmi_to_sspm_command(SPBM_SCMI_DEBUG,
				spbm_scmi_state.debug, 0 , 0,
				SPBM_SCMI_SET);

			return count;
		}
	}

	pr_info("[spbm]%s invalid input\n", __func__);

	return -EINVAL;
}

static ssize_t spbm_send_pwr_to_xpu_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;


	len = snprintf(buf, PAGE_SIZE, "send_pwr_to_xpu: %d\n",
		spbm_scmi_state.send_pwr_to_xpu);

	if (len < 0) {
		pr_info("[spbm]%s Error writing to buffer\n", __func__);
		return len; // Return error code
	} else if (len >= PAGE_SIZE) {
		pr_info("[spbm]%s Buffer size is too small, truncated output\n", __func__);
		len = PAGE_SIZE - 1; // Ensure len does not exceed PAGE_SIZE
	}

	return len;
}

static ssize_t spbm_send_pwr_to_xpu_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{

	if (strncmp(buf, "send_pwr_to_xpu", 15) == 0) {
		spbm_scmi_state.send_pwr_to_xpu = 1; // Enable the function
		pr_info("send_pwr_to_xpu %d\n", spbm_scmi_state.send_pwr_to_xpu);

		// Send command to the subsystem to set the state
		spbm_scmi_to_sspm_command(SPBM_SCMI_ENABLE_SEND_PWR,
		spbm_scmi_state.send_pwr_to_xpu, 0, 0, SPBM_SCMI_SET);
	} else if (strncmp(buf, "stop_send_pwr_to_xpu", 20) == 0) {
		spbm_scmi_state.send_pwr_to_xpu = 0; // Disable the function
		pr_info("send_pwr_to_xpu %d\n", spbm_scmi_state.send_pwr_to_xpu);

		// Send command to the subsystem to set the state
		spbm_scmi_to_sspm_command(SPBM_SCMI_ENABLE_SEND_PWR,
		spbm_scmi_state.send_pwr_to_xpu, 0, 0, SPBM_SCMI_SET);
	} else {// If the input does not match expected commands, return an error
		pr_info("[spbm]%s invalid input\n", __func__);
		return -EINVAL; // Invalid input
	}


	return count;
}


static ssize_t spbm_spbm_enable_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;


	len = snprintf(buf, PAGE_SIZE, "spbm_on: %d\n",
		spbm_scmi_state.enabled);

	if (len < 0) {
		pr_info("[spbm]%s Error writing to buffer\n", __func__);
		return len; // Return error code
	} else if (len >= PAGE_SIZE) {
		pr_info("[spbm]%s Buffer size is too small, truncated output\n", __func__);
		len = PAGE_SIZE - 1; // Ensure len does not exceed PAGE_SIZE
	}

	return len;
}

static ssize_t spbm_spbm_enable_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{

	if (strncmp(buf, "spbm_enable", 11) == 0) {
		spbm_scmi_state.enabled = 1; // Enable the function
		pr_info("spbm_enabled %d\n", spbm_scmi_state.enabled);
	} else if (strncmp(buf, "spbm_disable", 12) == 0) {
		spbm_scmi_state.enabled = 0; // Disable the function
		pr_info("spbm_enabled %d\n", spbm_scmi_state.enabled);
	} else if (strncmp(buf, "spbm_freerun", 12) == 0) {
		spbm_scmi_state.enabled = 2; // free run SPBM
		pr_info("spbm_freerun %d\n", spbm_scmi_state.enabled);
	} else {// If the input does not match expected commands, return an error
		pr_info("[spbm]%s invalid input\n", __func__);
		return -EINVAL; // Invalid input
	}

	// Send command to the subsystem to set the state
	spbm_scmi_to_sspm_command(SPBM_SCMI_SPBM_ENABLE,
		spbm_scmi_state.enabled, 0, 0, SPBM_SCMI_SET);

	return count;
}

static ssize_t spbm_fake_cpu_pwr_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;


	len = snprintf(buf, PAGE_SIZE, "fake bcpu: %d, mcpu: %d, lcpu: %d\n",
		spbm_scmi_state.fake_bcpu_tgt_pwr,
		spbm_scmi_state.fake_mcpu_tgt_pwr,
		spbm_scmi_state.fake_lcpu_tgt_pwr);

	if (len < 0) {
		pr_info("[spbm]%s Error writing to buffer\n", __func__);
		return len; // Return error code
	} else if (len >= PAGE_SIZE) {
		pr_info("[spbm]%s Buffer size is too small, truncated output\n", __func__);
		len = PAGE_SIZE - 1; // Ensure len does not exceed PAGE_SIZE
	}

	return len;
}

static ssize_t spbm_fake_cpu_pwr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char cmd[20];
	unsigned int fake_bcpu, fake_mcpu, fake_lcpu;

	if (sscanf(buf, "%14s %u %u %u", cmd, &fake_bcpu, &fake_mcpu, &fake_lcpu) == 4) {
		if (strncmp(cmd, "spbm_fake_cpu", 13) == 0) {
			spbm_scmi_state.fake_bcpu_tgt_pwr = fake_bcpu;
			spbm_scmi_state.fake_mcpu_tgt_pwr = fake_mcpu;
			spbm_scmi_state.fake_lcpu_tgt_pwr = fake_lcpu;

			spbm_scmi_to_sspm_command(SPBM_SCMI_FAKE_CPU_PWR_LIMIT,
				fake_bcpu, fake_mcpu , fake_lcpu,
				SPBM_SCMI_SET);

			return count;
		}
	}

	if (sscanf(buf, "%18s", cmd) == 1) {
		if (strncmp(cmd, "spbm_not_fake_cpu", 17) == 0) {
			spbm_scmi_state.fake_bcpu_tgt_pwr = 0xffffffff;
			spbm_scmi_state.fake_mcpu_tgt_pwr = 0xffffffff;
			spbm_scmi_state.fake_lcpu_tgt_pwr = 0xffffffff;

			spbm_scmi_to_sspm_command(SPBM_SCMI_FAKE_CPU_PWR_LIMIT,
				spbm_scmi_state.fake_bcpu_tgt_pwr,
				spbm_scmi_state.fake_mcpu_tgt_pwr,
				spbm_scmi_state.fake_lcpu_tgt_pwr,
				SPBM_SCMI_SET);

			return count;
		}
	}

	pr_info("[spbm]%s invalid input\n", __func__);

	return -EINVAL;
}

static ssize_t spbm_fake_gpu_pwr_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;


	len = snprintf(buf, PAGE_SIZE, "fake gpu(%d): target pwr: %d, average pwr: %d\n",
		spbm_scmi_state.fake_gpu,
		spbm_scmi_state.fake_gpu_tgt_pwr,
		spbm_scmi_state.fake_gpu_avg_pwr);

	if (len < 0) {
		pr_info("[spbm]%s Error writing to buffer\n", __func__);
		return len; // Return error code
	} else if (len >= PAGE_SIZE) {
		pr_info("[spbm]%s Buffer size is too small, truncated output\n", __func__);
		len = PAGE_SIZE - 1; // Ensure len does not exceed PAGE_SIZE
	}

	return len;
}

static ssize_t spbm_fake_gpu_pwr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char cmd[20];
	unsigned int fake_gpu, fake_gpu_tgt, fake_gpu_avg;

	if (sscanf(buf, "%14s %u %u %u", cmd, &fake_gpu, &fake_gpu_tgt, &fake_gpu_avg) == 4) {
		if (strncmp(cmd, "spbm_fake_gpu", 13) == 0) {
			spbm_scmi_state.fake_gpu = fake_gpu;
			spbm_scmi_state.fake_gpu_tgt_pwr = fake_gpu_tgt;
			spbm_scmi_state.fake_gpu_avg_pwr = fake_gpu_avg;

			spbm_scmi_to_sspm_command(SPBM_SCMI_FAKE_GPU_PWR_LIMIT,
				spbm_scmi_state.fake_gpu,
				spbm_scmi_state.fake_gpu_tgt_pwr,
				spbm_scmi_state.fake_gpu_avg_pwr,
				SPBM_SCMI_SET);

			return count;
		}
	}

	if (sscanf(buf, "%18s %u", cmd, &fake_gpu) == 2) {
		if (strncmp(cmd, "spbm_not_fake_gpu", 17) == 0) {
			spbm_scmi_state.fake_gpu = fake_gpu;
			spbm_scmi_state.fake_gpu_tgt_pwr = 0xffffffff;
			spbm_scmi_state.fake_gpu_avg_pwr = 0xffffffff;

			spbm_scmi_to_sspm_command(SPBM_SCMI_FAKE_GPU_PWR_LIMIT,
				fake_gpu, fake_gpu_tgt, fake_gpu_avg,
				SPBM_SCMI_SET);

			return count;

		}
	}

	pr_info("[spbm]%s invalid input\n", __func__);

	return -EINVAL;
}


static ssize_t spbm_fake_npu_pwr_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;


	len = snprintf(buf, PAGE_SIZE, "fake npu(%d): target pwr: %d, average pwr: %d\n",
		spbm_scmi_state.fake_npu,
		spbm_scmi_state.fake_npu_tgt_pwr,
		spbm_scmi_state.fake_npu_avg_pwr);

	if (len < 0) {
		pr_info("[spbm]%s Error writing to buffer\n", __func__);
		return len; // Return error code
	} else if (len >= PAGE_SIZE) {
		pr_info("[spbm]%s Buffer size is too small, truncated output\n", __func__);
		len = PAGE_SIZE - 1; // Ensure len does not exceed PAGE_SIZE
	}

	return len;
}

static ssize_t spbm_fake_npu_pwr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char cmd[20];
	unsigned int fake_npu, fake_npu_tgt, fake_npu_avg;


	if (sscanf(buf, "%14s %u %u %u", cmd, &fake_npu, &fake_npu_tgt,
		&fake_npu_avg) == 4) {
		if (strncmp(cmd, "spbm_fake_npu", 13) == 0) {
			spbm_scmi_state.fake_npu = fake_npu;
			spbm_scmi_state.fake_npu_tgt_pwr = fake_npu_tgt;
			spbm_scmi_state.fake_npu_avg_pwr = fake_npu_avg;

			spbm_scmi_to_sspm_command(SPBM_SCMI_FAKE_NPU_PWR_LIMIT,
				spbm_scmi_state.fake_npu,
				spbm_scmi_state.fake_npu_tgt_pwr,
				spbm_scmi_state.fake_npu_avg_pwr,
				SPBM_SCMI_SET);

			return count;
		}
	}

	if (sscanf(buf, "%18s %u", cmd, &fake_npu) == 2) {
		if (strncmp(cmd, "spbm_not_fake_npu", 17) == 0) {
			spbm_scmi_state.fake_npu = fake_npu;
			spbm_scmi_state.fake_npu_tgt_pwr = 0xffffffff;
			spbm_scmi_state.fake_npu_avg_pwr = 0xffffffff;

			spbm_scmi_to_sspm_command(SPBM_SCMI_FAKE_NPU_PWR_LIMIT,
				fake_npu, fake_npu_tgt, fake_npu_avg,
				SPBM_SCMI_SET);

			return count;
		}
	}

	pr_info("[spbm]%s invalid input\n", __func__);

	return -EINVAL;
}

static ssize_t spbm_sf_gpu_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;


	len = snprintf(buf, PAGE_SIZE, "sf scaling factor gpu(%d)\n",
		spbm_scmi_state.sf_gpu);

	if (len < 0) {
		pr_info("Error writing to buffer\n");
		return len; // Return error code
	} else if (len >= PAGE_SIZE) {
		pr_info("Buffer size is too small, truncated output\n");
		len = PAGE_SIZE - 1; // Ensure len does not exceed PAGE_SIZE
	}

	return len;
}

static ssize_t spbm_sf_gpu_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char cmd[20];
	unsigned int sf_gpu;

	if (sscanf(buf, "%12s %u", cmd, &sf_gpu) == 2) {
		if (strncmp(cmd, "spbm_sf_gpu", 11) == 0) {
			spbm_scmi_state.sf_gpu = sf_gpu;

			spbm_scmi_to_sspm_command(SPBM_SCMI_SCALING_FACTOR_GPU,
				spbm_scmi_state.sf_gpu, 0, 0,
				SPBM_SCMI_SET);

			return count;
		}
	}

	pr_info("[spbm]%s invalid input\n", __func__);

	return -EINVAL;
}

static ssize_t spbm_sf_npu_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;


	len = snprintf(buf, PAGE_SIZE, "sf scaling factor npu(%d)\n",
		spbm_scmi_state.sf_npu);

	if (len < 0) {
		pr_info("[spbm]%s Error writing to buffer\n", __func__);
		return len; // Return error code
	} else if (len >= PAGE_SIZE) {
		pr_info("[spbm]%s Buffer size is too small, truncated output\n", __func__);
		len = PAGE_SIZE - 1; // Ensure len does not exceed PAGE_SIZE
	}

	return len;
}

static ssize_t spbm_sf_npu_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char cmd[20];
	unsigned int sf_npu;

	if (sscanf(buf, "%12s %u", cmd, &sf_npu) == 2) {
		if (strncmp(cmd, "spbm_sf_npu", 11) == 0) {
			spbm_scmi_state.sf_npu = sf_npu;

			spbm_scmi_to_sspm_command(SPBM_SCMI_SCALING_FACTOR_NPU,
				spbm_scmi_state.sf_npu, 0, 0,
				SPBM_SCMI_SET);

			return count;
		}
	}

	pr_info("[spbm]%s invalid input\n", __func__);

	return -EINVAL;
}


static ssize_t spbm_sf_cpu_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len = snprintf(buf, PAGE_SIZE,
		"sf scaling factor bcpu(%d) mcpu(%d) lcpu(%d)\n",
		spbm_scmi_state.sf_bcpu,
		spbm_scmi_state.sf_mcpu,
		spbm_scmi_state.sf_lcpu);

	if (len < 0) {
		pr_info("[spbm]%s Error writing to buffer\n", __func__);
		return len; // Return error code
	} else if (len >= PAGE_SIZE) {
		pr_info("[spbm]%s Buffer size is too small, truncated output\n", __func__);
		len = PAGE_SIZE - 1; // Ensure len does not exceed PAGE_SIZE
	}

	return len;
}

static ssize_t spbm_sf_cpu_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char cmd[20];
	unsigned int sf_bcpu, sf_mcpu, sf_lcpu;

	if (sscanf(buf, "%12s %u %u %u", cmd, &sf_bcpu, &sf_mcpu, &sf_lcpu) == 4) {
		if (strncmp(cmd, "spbm_sf_cpu", 11) == 0) {
			spbm_scmi_state.sf_bcpu = sf_bcpu;
			spbm_scmi_state.sf_mcpu = sf_mcpu;
			spbm_scmi_state.sf_lcpu = sf_lcpu;

			spbm_scmi_to_sspm_command(SPBM_SCMI_SCALING_FACTOR_CPU,
			spbm_scmi_state.sf_bcpu,
			spbm_scmi_state.sf_mcpu,
			spbm_scmi_state.sf_lcpu,
				SPBM_SCMI_SET);

			return count;
		}
	}

	pr_info("[spbm]%s invalid input\n", __func__);

	return -EINVAL;
}


static struct kobj_attribute spbm_cpu_info_attr = __ATTR_RO(spbm_cpu_info);
static struct kobj_attribute spbm_gpu_info_attr = __ATTR_RO(spbm_gpu_info);
static struct kobj_attribute spbm_npu_info_attr = __ATTR_RO(spbm_npu_info);

static struct kobj_attribute spbm_current_reporting_attr = __ATTR_RO(spbm_current_reporting);
static struct kobj_attribute spbm_xpu_power_limit_attr = __ATTR_RO(spbm_xpu_power_limit);
static struct kobj_attribute spbm_xpu_request_power_attr = __ATTR_RO(spbm_xpu_request_power);

static struct kobj_attribute spbm_spbm_enable_attr = __ATTR_RW(spbm_spbm_enable);
static struct kobj_attribute spbm_send_pwr_to_xpu_attr = __ATTR_RW(spbm_send_pwr_to_xpu);
static struct kobj_attribute spbm_debug_state_attr = __ATTR_RW(spbm_debug_state);
static struct kobj_attribute spbm_fake_cpu_pwr_attr = __ATTR_RW(spbm_fake_cpu_pwr);
static struct kobj_attribute spbm_fake_gpu_pwr_attr = __ATTR_RW(spbm_fake_gpu_pwr);
static struct kobj_attribute spbm_fake_npu_pwr_attr = __ATTR_RW(spbm_fake_npu_pwr);
static struct kobj_attribute spbm_sf_cpu_attr = __ATTR_RW(spbm_sf_cpu);
static struct kobj_attribute spbm_sf_gpu_attr = __ATTR_RW(spbm_sf_gpu);
static struct kobj_attribute spbm_sf_npu_attr = __ATTR_RW(spbm_sf_npu);

static struct attribute *spbm_attrs[] = {
	&spbm_cpu_info_attr.attr,
	&spbm_gpu_info_attr.attr,
	&spbm_npu_info_attr.attr,
	&spbm_current_reporting_attr.attr,
	&spbm_xpu_power_limit_attr.attr,
	&spbm_xpu_request_power_attr.attr,
	&spbm_spbm_enable_attr.attr,
	&spbm_send_pwr_to_xpu_attr.attr,
	&spbm_debug_state_attr.attr,
	&spbm_fake_cpu_pwr_attr.attr,
	&spbm_fake_gpu_pwr_attr.attr,
	&spbm_fake_npu_pwr_attr.attr,
	&spbm_sf_cpu_attr.attr,
	&spbm_sf_gpu_attr.attr,
	&spbm_sf_npu_attr.attr,
	NULL
};

static struct attribute_group spbm_attr_group = {
	.name	= "spbm",
	.attrs	= spbm_attrs,
};


int ppb_set_wifi_pwr_addr(unsigned int val)
{
	if (!ppb_sram_base) {
		pr_info("%s: ppb_sram_base error %p\n", __func__, ppb_sram_base);
		return -1;
	}

	pr_info("ppb_wifi_pwr_addr: 0x%x\n", val);
	writel(val, (void __iomem *)(ppb_sram_base + PPB_WIFI_SMEM_ADDR * 4));
	return 0;
}
EXPORT_SYMBOL(ppb_set_wifi_pwr_addr);

int ppb_set_wifi_pwr_addr_by_dts(void)
{
	struct device_node *wifi_node = NULL;
	unsigned long long wlan_emi_pa = 0;
	unsigned int wlan_ppb_offset = 0;
	int ret;

	if (!ppb_sram_base) {
		pr_info("%s: ppb_sram_base error %p\n", __func__, ppb_sram_base);
		return -1;
	}

	wifi_node = of_find_compatible_node(NULL, NULL, "mediatek,wifi");
	if (!wifi_node) {
		pr_info("%s: get from dts, emi for ppb error\n", __func__);
		return -1;
	}

	ret = of_property_read_u64(wifi_node, "conninfra-emi-addr", &wlan_emi_pa);
	if (ret || wlan_emi_pa == 0) {
		pr_info("%s: ret %d, wlan_emi_pa 0x%llx\n", __func__, ret, wlan_emi_pa);
		of_node_put(wifi_node);
		return -1;
	}

	ret = of_property_read_u32(wifi_node, "ppb-emi-offset", &wlan_ppb_offset);
	of_node_put(wifi_node);
	if (ret || wlan_ppb_offset == 0) {
		pr_info("%s: ret %d, wlan_ppb_offset 0x%x\n", __func__, ret, wlan_ppb_offset);
		return -1;
	}

	ppb_set_wifi_pwr_addr((unsigned int)(0xFFFFFFFF & wlan_emi_pa) + wlan_ppb_offset);

	return 0;
}

static int __used ppb_read_sram(int offset)
{
	void __iomem *addr = ppb_sram_base + offset * 4;

	if (!ppb_sram_base) {
		pr_info("ppb_sram_base error %p\n", ppb_sram_base);
		return 0;
	}

	return readl(addr);
}

static void __used ppb_write_sram(unsigned int val, int offset)
{
	if (!ppb_sram_base) {
		pr_info("ppb_sram_base error %p\n", ppb_sram_base);
		return;
	}

	writel(val, (void __iomem *)(ppb_sram_base + offset * 4));
}

static int dbg_read(void __iomem *reg_base, int offset)
{
	if (!reg_base) {
		pr_info("reg_base error %p, offset %d\n", reg_base, offset);
		return 0;
	}

	return readl(reg_base + offset * 4);
}

static void __used dbg_write(void __iomem *reg_base, int offset, unsigned int val)
{
	if (!reg_base) {
		pr_info("reg_base error %p\n", reg_base);
		return;
	}

	writel(val, (void __iomem *)(reg_base + offset * 4));
}

static int __used hpt_ctrl_read(int offset)
{
	void __iomem *addr = hpt_ctrl_base + offset * 4;

	if (!hpt_ctrl_base) {
		pr_info("hpt_ctrl_base error %p\n", hpt_ctrl_base);
		return 0;
	}

	return readl(addr);
}

static void __used hpt_ctrl_write(unsigned int val, int offset)
{
	if (!hpt_ctrl_base) {
		pr_info("hpt_ctrl_base error %p\n", hpt_ctrl_base);
		return;
	}

	writel(val, (void __iomem *)(hpt_ctrl_base + offset * 4));
}

static int __used hpt_status_read(int offset)
{
	void __iomem *addr = hpt_status_base + offset * 4;

	if (!hpt_status_base) {
		pr_info("hpt_status_base error %p\n", hpt_status_base);
		return 0;
	}

	return readl(addr);
}

static void ppb_allocate_budget_manager(void)
{
	int vsys_budget_noerr = 0, remain_budget = 0, vsys_budget = 0;
	int flash, audio, camera, display, apu, dram;

	if (ppb_ctrl.manual_mode == 1) {
		flash = ppb_manual.loading_flash;
		audio = ppb_manual.loading_audio;
		camera = ppb_manual.loading_camera;
		display = ppb_manual.loading_display;
		apu = ppb_manual.loading_apu;
		dram = ppb_manual.loading_dram;
		vsys_budget = ppb_manual.vsys_budget;
		remain_budget = vsys_budget - (flash + audio + camera + display + dram);
		remain_budget = (remain_budget > 0) ? remain_budget : 0;
		ppb_manual.remain_budget = remain_budget;
		vsys_budget_noerr = ppb_manual.vsys_budget_noerr;
	} else {
		flash = ppb.loading_flash;
		audio = ppb.loading_audio;
		camera = ppb.loading_camera;
		display = ppb.loading_display;
		apu = ppb.loading_apu;
		dram = ppb.loading_dram;
		vsys_budget = pb.sys_power;
		remain_budget = vsys_budget - (flash + audio + camera + display + dram);
		remain_budget = (remain_budget > 0) ? remain_budget : 0;
		ppb.remain_budget = remain_budget;
		vsys_budget_noerr = pb.sys_power_noerr;
	}

	ppb_write_sram(remain_budget, PPB_VSYS_PWR);
	ppb_write_sram(flash, PPB_FLASH_PWR);
	ppb_write_sram(audio, PPB_AUDIO_PWR);
	ppb_write_sram(camera, PPB_CAMERA_PWR);
	ppb_write_sram(apu, PPB_APU_PWR);
	ppb_write_sram(display, PPB_DISPLAY_PWR);
	ppb_write_sram(dram, PPB_DRAM_PWR);
	ppb_write_sram(0, PPB_APU_PWR_ACK);
	ppb_write_sram(vsys_budget_noerr, PPB_VSYS_PWR_NOERR);

	if (mt_ppb_debug)
		pr_info("(P_BGT/H_BGT/R_BGT)=%u,%u,%u (FLASH/AUD/CAM/DISP/APU/DRAM)=%u,%u,%u,%u,%u,%u\n",
			vsys_budget_noerr, vsys_budget, remain_budget, flash, audio, camera, display, apu, dram);
	trace_peak_power_budget(&ppb);
}

static int __used get_xpu_debug_info(struct xpu_dbg_t *data)
{
	unsigned int val;

	if (!data)
		return -EINVAL;

	val = dbg_read(cpu_dbg_base, 1);
	data->cpub_len = (val >> 16) & 0x3FF;
	data->cpub_cnt = val & 0xFFFF;
	val = dbg_read(cpu_dbg_base, 2);
	data->cpub_th_t = val;
	val = dbg_read(cpu_dbg_base, 5);
	data->cpum_len = (val >> 16) & 0x3FF;
	data->cpum_cnt = val & 0xFFFF;
	val = dbg_read(cpu_dbg_base, 6);
	data->cpum_th_t = val;
	val = dbg_read(gpu_dbg_base, 0);
	data->gpu_len = (val >> 16) & 0x3FF;
	data->gpu_cnt = val & 0xFFFF;
	val = dbg_read(gpu_dbg_base, 1);
	data->gpu_th_t = val;

	return 0;
}

static int __used get_ppb3_debug_info(struct ppb3_dbg_t *data)
{
	unsigned int i;
	unsigned long long duration_unit = 3846; /* 38.46 ns */
	unsigned int  duration_us_factor = 100000;
	if (!data)
		return -EINVAL;

	for (i = 0; i < TOTAL_PWR_INTERVALS; i++) {
		data->lcpu_pwr[i]  = dbg_read((ppb3_dbg_base + SPBM_LCPU_PWR_LIMIT_TIMES_OFFSET), i);
		data->mcpu_pwr[i]  = dbg_read((ppb3_dbg_base + SPBM_MCPU_PWR_LIMIT_TIMES_OFFSET), i);
		data->bcpu_pwr[i]  = dbg_read((ppb3_dbg_base + SPBM_BCPU_PWR_LIMIT_TIMES_OFFSET), i);
		data->gpu_pwr[i]   = dbg_read((ppb3_dbg_base + SPBM_GPU_PWR_LIMIT_TIMES_OFFSET) , i);
		data->npu_pwr[i]   = dbg_read((ppb3_dbg_base + SPBM_NPU_PWR_LIMIT_TIMES_OFFSET) , i);
	}
	data->cgn_sf[0] = dbg_read(spbm_csram_base + SPBM_KERNEL_B_SF_OFFSET, 0);
	data->cgn_sf[1] = dbg_read(spbm_csram_base + SPBM_KERNEL_M_SF_OFFSET, 0);
	data->cgn_sf[2] = dbg_read(spbm_csram_base + SPBM_KERNEL_L_SF_OFFSET, 0);
	data->cgn_sf[3] = dbg_read(spbm_csram_base + SPBM_KERNEL_G_SF_OFFSET, 0);
	data->cgn_sf[4] = dbg_read(spbm_csram_base + SPBM_KERNEL_N_SF_OFFSET, 0);

	data->bat_pwr = dbg_read(spbm_csram_base + SPBM_BAT_PWR_OFFSET, 0);
	data->pre_uv = dbg_read(spbm_csram_base + SPBM_UVLO_TRIGGER_TIMES_OFFSET, 0);
	data->oc_count = dbg_read(spbm_csram_base + SPBM_OC_COUNT_OFFSET, 0);
	data->oc_duration = dbg_read(spbm_csram_base + SPBM_OC_DURATION_OFFSET, 0);
	data->oc_duration_us = ((unsigned long long)(data->oc_duration) * (duration_unit)) / (duration_us_factor);
	return 0;
}


static void __used ppb3_print_dbg_log(struct timer_list *timer)
{
	struct ppb3_dbg_t ppb3_dbg_data;
	int offset, ret;
	char str[STR_SIZE];
	int i;
	static int prev_oc_count;

	if (ppb_ctrl.ppb_drv_done) {

		memset(&ppb3_dbg_data, 0, sizeof(ppb3_dbg_data));

		get_ppb3_debug_info(&ppb3_dbg_data);

		offset = 0;
		ret = snprintf(str + offset, STR_SIZE - offset, "PPB3 L:(");
		if (ret < 0)
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
		else
			offset = offset + ret;

		for (i = 0; i < TOTAL_PWR_INTERVALS; i++) {
			ret = snprintf(str + offset, STR_SIZE - offset, "%d ", ppb3_dbg_data.lcpu_pwr[i]);
			if (ret < 0)
				pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			else
				offset = offset + ret;
		}

		ret = snprintf(str + offset, STR_SIZE - offset, ") M:(");
		if (ret < 0)
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
		else
			offset = offset + ret;

		for (i = 0; i < TOTAL_PWR_INTERVALS; i++) {
			ret = snprintf(str + offset, STR_SIZE - offset, "%d ", ppb3_dbg_data.mcpu_pwr[i]);
			if (ret < 0)
				pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			else
				offset = offset + ret;
		}

		ret = snprintf(str + offset, STR_SIZE - offset, ") B:(");
		if (ret < 0)
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
		else
			offset = offset + ret;

		for (i = 0; i < TOTAL_PWR_INTERVALS; i++) {
			ret = snprintf(str + offset, STR_SIZE - offset, "%d ", ppb3_dbg_data.bcpu_pwr[i]);
			if (ret < 0)
				pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			else
				offset = offset + ret;
		}

		ret = snprintf(str + offset, STR_SIZE - offset, ") G:(");
		if (ret < 0)
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
		else
			offset = offset + ret;

		for (i = 0; i < TOTAL_PWR_INTERVALS; i++) {
			ret = snprintf(str + offset, STR_SIZE - offset, "%d ", ppb3_dbg_data.gpu_pwr[i]);
			if (ret < 0)
				pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			else
				offset = offset + ret;
		}

		ret = snprintf(str + offset, STR_SIZE - offset, ") N:(");
		if (ret < 0)
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
		else
			offset = offset + ret;

		for (i = 0; i < TOTAL_PWR_INTERVALS; i++) {
			ret = snprintf(str + offset, STR_SIZE - offset, "%d ",
				ppb3_dbg_data.npu_pwr[i]);
			if (ret < 0)
				pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			else
				offset = offset + ret;
		}

		ret = snprintf(str + offset, STR_SIZE - offset, ") SF:(");
		if (ret < 0)
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
		else
			offset = offset + ret;

		for (i = 0; i < SF_NUM; i++) {
			ret = snprintf(str + offset, STR_SIZE - offset, "%d ", ppb3_dbg_data.cgn_sf[i]);
			if (ret < 0)
				pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			else
				offset = offset + ret;
		}

		ret = snprintf(str + offset, STR_SIZE - offset,
		") bat[soc:%d t:%d] bdt[pwr:%d noer:%d][spbm pwr:%d pre_uv:%d oc:%d dur:%d(%d)]",
			pb.soc, pb.temp, pb.sys_power,pb.sys_power_noerr,
			ppb3_dbg_data.bat_pwr, ppb3_dbg_data.pre_uv, ppb3_dbg_data.oc_count,
			ppb3_dbg_data.oc_duration, ppb3_dbg_data.oc_duration_us);
		if (ret < 0)
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
		else
			offset = offset + ret;


		pr_info("%s\n", str);

		mod_timer(&ppb_dbg_timer, jiffies + PPB3_LOG_DURATION);

		if ((ppb3_dbg_data.oc_count != prev_oc_count) && (cb_func_hpt != NULL))
			cb_func_hpt();

		prev_oc_count = ppb3_dbg_data.oc_count;
	}
}

static void __used ppb_print_dbg_log(struct timer_list *timer)
{
	unsigned long duration, time;
	struct xpu_dbg_t dbg_data;
	static ktime_t l_ktime;
	ktime_t ktime;
	unsigned int cpub_cnt, cpub_th_t, cpum_cnt, cpum_th_t, gpu_cnt, gpu_th_t;
	int cpub_sf = 0, cpum_sf = 0, gpu_sf = 0, cg_pwr = 0, combo = 0, cb_cnt = 0, i, offset, ret;
	char str[STR_SIZE];

	if (!ppb_ctrl.ppb_drv_done)
		return;

	ktime = ktime_get();
	duration = ktime_us_delta(ktime, l_ktime);
	time = ktime_to_us(ktime);
	l_ktime = ktime;

	get_xpu_debug_info(&dbg_data);
	cpub_cnt = dbg_data.cpub_cnt - last_klog_xpu_dbg.cpub_cnt;
	cpub_th_t = dbg_data.cpub_th_t - last_klog_xpu_dbg.cpub_th_t;
	cpum_cnt = dbg_data.cpum_cnt - last_klog_xpu_dbg.cpum_cnt;
	cpum_th_t = dbg_data.cpum_th_t - last_klog_xpu_dbg.cpum_th_t;
	gpu_cnt = dbg_data.gpu_cnt - last_klog_xpu_dbg.gpu_cnt;
	gpu_th_t = dbg_data.gpu_th_t - last_klog_xpu_dbg.gpu_th_t;

	if (cgppt_dbg_ops && cgppt_dbg_ops->get_cpub_sf)
		cpub_sf = cgppt_dbg_ops->get_cpub_sf();

	if (cgppt_dbg_ops && cgppt_dbg_ops->get_cpum_sf)
		cpum_sf = cgppt_dbg_ops->get_cpum_sf();

	if (cgppt_dbg_ops && cgppt_dbg_ops->get_gpu_sf)
		gpu_sf = cgppt_dbg_ops->get_gpu_sf();

	if (cgppt_dbg_ops && cgppt_dbg_ops->get_cg_bgt)
		cg_pwr = cgppt_dbg_ops->get_cg_bgt();

	if (cgppt_dbg_ops && cgppt_dbg_ops->get_combo)
		combo = cgppt_dbg_ops->get_combo();

	offset = 0;
	ret = snprintf(str + offset, STR_SIZE - offset,
		"t[k:%lu,d:%lu(ms)] bat[soc:%d t:%d] bdt[pwr:%d noer:%d] ppt[pwr:%d cb:%d c_cb(",
		time / 1000, duration / 1000, pb.soc, pb.temp, pb.sys_power, pb.sys_power_noerr, cg_pwr, combo);
	if (ret < 0)
		pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
	else
		offset = offset + ret;

	for (i = 0; i < CPU_PEAK_POWER_COMBO_TABLE_IDX_ROW_COUNT; i++) {
		if (cgppt_dbg_ops && cgppt_dbg_ops->get_cpucb_cnt)
			cb_cnt = cgppt_dbg_ops->get_cpucb_cnt(i);

		ret = snprintf(str + offset, STR_SIZE - offset, "%d ", cb_cnt);
		if (ret < 0)
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
		else
			offset = offset + ret;
	}

	ret = snprintf(str + offset, STR_SIZE - offset, ") g_cb(");
		if (ret < 0)
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
		else
			offset = offset + ret;

	for (i = 0; i < GPU_PEAK_POWER_COMBO_TABLE_IDX_ROW_COUNT; i++) {
		if (cgppt_dbg_ops && cgppt_dbg_ops->get_gpucb_cnt)
			cb_cnt = cgppt_dbg_ops->get_gpucb_cnt(i);

		ret = snprintf(str + offset, STR_SIZE - offset, "%d ", cb_cnt);
		if (ret < 0)
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
		else
			offset = offset + ret;
	}

	ret = snprintf(str + offset, STR_SIZE - offset,
		")] sf[cb(%d):%d,%d cm(%d):%d,%d g(%d):%d,%d]",
		cpub_sf, cpub_cnt, cpub_th_t, cpum_sf, cpum_cnt, cpum_th_t, gpu_sf, gpu_cnt, gpu_th_t);
	if (ret < 0)
		pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
	else
		offset = offset + ret;

	pr_info("%s\n", str);
	memcpy(&last_klog_xpu_dbg, &dbg_data, sizeof(struct xpu_dbg_t));
	mod_timer(&ppb_dbg_timer, jiffies + PPB_LOG_DURATION);
}

#if !IS_ENABLED(CONFIG_MTK_GPU_LEGACY)
static int __used ppb_gpueb_ipi_init(void)
{
	static bool ipi_init;
	int ret;

	if (!ipi_init) {
		channel_id = gpueb_get_send_PIN_ID_by_name("IPI_ID_PPB");
		if (channel_id == -1) {
			pr_info("get gpueb channel IPI_ID_PPB fail\n");
			return -1;
		}
		ret = mtk_ipi_register(get_gpueb_ipidev(), channel_id, NULL, NULL,
			(void *)&ack_data);
		if (ret) {
			pr_info("ipi_register fail, ret %d\n", ret);
			return -1;
		}
		ipi_init = true;
	}

	return 0;
}

static int __used notify_gpueb(void)
{
	struct ppb_ipi_data ipi_data;
	int ret;

	if (ppb_gpueb_ipi_init())
		return -1;

	ipi_data.cmd = 0;
	ret = mtk_ipi_send_compl_to_gpueb(channel_id, IPI_SEND_WAIT, &ipi_data,
			PPB_IPI_DATA_LEN, PPB_IPI_TIMEOUT_MS);

	return ret;
}
#endif

static bool __used ppb_func_enable_check(void)
{
	if (!ppb_ctrl.ppb_drv_done)
		return false;

	return true;
}

static unsigned int __used check_ppb_version(void)
{
	pr_info("[%s] [%d]\n", __func__, pb.version);

	return pb.version;
}


static bool __used ppb_update_table_info(enum ppb_kicker kicker, struct ppb *req_ppb)
{
	bool is_update = false;

	switch (kicker) {
	case KR_BUDGET:
		ppb.vsys_budget = req_ppb->vsys_budget;
		ppb.vsys_budget_noerr = req_ppb->vsys_budget_noerr;
		is_update = true;
		break;
	case KR_FLASHLIGHT:
		if (ppb.loading_flash != req_ppb->loading_flash) {
			ppb.loading_flash = req_ppb->loading_flash;
			is_update = true;
		}
		break;
	case KR_AUDIO:
		if (ppb.loading_audio != req_ppb->loading_audio) {
			ppb.loading_audio = req_ppb->loading_audio;
			is_update = true;
		}
		break;
	case KR_CAMERA:
		if (ppb.loading_camera != req_ppb->loading_camera) {
			ppb.loading_camera = req_ppb->loading_camera;
			is_update = true;
		}
		break;
	case KR_DISPLAY:
		if (ppb.loading_display != req_ppb->loading_display) {
			ppb.loading_display = req_ppb->loading_display;
			is_update = true;
		}
		break;
	case KR_APU:
		if (ppb.loading_apu != req_ppb->loading_apu) {
			ppb.loading_apu = req_ppb->loading_apu;
			is_update = true;
		}
		break;
	default:
		pr_info("[%s] ERROR, unknown kicker [%d]\n", __func__, kicker);
		WARN_ON_ONCE(1);
		break;
	}

	return is_update;
}

static void mtk_power_budget_manager(enum ppb_kicker kicker, struct ppb *req_ppb)
{
	bool ppb_enable = false;
	bool ppb_update = false;
	unsigned long flags;

	ppb_enable = ppb_func_enable_check();
	if (!ppb_enable)
		return;

	ppb_update = ppb_update_table_info(kicker, req_ppb);

	if (ppb_ctrl.ppb_stop)
		return;

	if (!ppb_update)
		return;

	spin_lock_irqsave(&ppb_lock, flags);
	ppb_allocate_budget_manager();
	spin_unlock_irqrestore(&ppb_lock, flags);
}

void kicker_ppb_request_power(enum ppb_kicker kicker, unsigned int power)
{
	bool ppb_enable = false;
	struct ppb ppb = {0};

	ppb_enable = ppb_func_enable_check();
	if (!ppb_enable)
		return;

	switch (kicker) {
	case KR_BUDGET:
		ppb.vsys_budget = power;
		ppb.vsys_budget_noerr = pb.bat_power_noerr;
		break;
	case KR_FLASHLIGHT:
		ppb.loading_flash = power;
		break;
	case KR_AUDIO:
		ppb.loading_audio = power;
		break;
	case KR_CAMERA:
		ppb.loading_camera = power;
		break;
	case KR_DISPLAY:
		ppb.loading_display = power;
		break;
	case KR_APU:
		ppb.loading_apu = power;
		break;
	default:
		pr_info("[%s] ERROR, unknown kicker [%d]\n", __func__, kicker);
		break;
	}

	mtk_power_budget_manager(kicker, &ppb);
}
EXPORT_SYMBOL(kicker_ppb_request_power);


static int __used read_dts_val(const struct device_node *np, const char *name, int *param, int unit)
{
	static unsigned int val;

	if (!of_property_read_u32(np, name, &val))
		*param = (int)val * unit;
	else {
		pr_info("Get %s no data !!!\n", name);
		return -1;
	}
	return 0;
}

static int __used read_dts_val_by_idx(const struct device_node *np, const char *name, int idx, int *param,
	int unit)
{
	unsigned int val = 0;

	if (!of_property_read_u32_index(np, name, idx, &val)) {
		*param = (int)val * unit;
	}  else {
		pr_info("Get %s no data, idx %d !!!\n", name, idx);
		return -1;
	}

	return 0;
}

static int __used interpolation(int i1, int b1, int i2, int b2, int i)
{
	int ret;

	if (i2 == i1)
		return b1;

	ret = (b2 - b1) * (i - i1) / (i2 - i1) + b1;
	return ret;
}

static int __used soc_to_ocv(int soc, unsigned int table_idx, unsigned int error)
{
	struct fg_info_t *info_p = &fg_data.fg_info[table_idx];
	struct ocv_table_t *table_p;
	int dod, ret, i;
	int high_dod, low_dod, high_volt, low_volt;

	dod = 10000 - soc + error;
	if (dod > 10000)
		dod = 10000;
	else if (dod < 0)
		dod = 0;

	for (i = 0; i < info_p->ocv_table_size; i++) {
		table_p = &info_p->ocv_table[i];
		if (table_p->dod >= dod)
			break;
	}

	if (i == 0) {
		ret = info_p->ocv_table[0].voltage;
	} else if (i >= info_p->ocv_table_size) {
		i = info_p->ocv_table_size - 1;
		ret = info_p->ocv_table[i].voltage;
	} else {
		high_dod = info_p->ocv_table[i-1].dod;
		low_dod = info_p->ocv_table[i].dod;
		high_volt = info_p->ocv_table[i-1].voltage;
		low_volt = info_p->ocv_table[i].voltage;
		ret = interpolation(high_dod, high_volt, low_dod, low_volt, dod);
	}

	return ret;
}

static int __used soc_to_rdc(int soc, unsigned int table_idx)
{
	struct fg_info_t *info_p = &fg_data.fg_info[table_idx];
	struct ocv_table_t *table_p;
	int dod, ret, i;
	int high_dod, low_dod, high_rdc, low_rdc;

	dod = 10000 - soc;
	if (dod > 10000)
		dod = 10000;
	else if (dod < 0)
		dod = 0;

	for (i = 0; i < info_p->ocv_table_size; i++) {
		table_p = &info_p->ocv_table[i];
		if (table_p->dod >= dod)
			break;
	}

	if (i == 0) {
		ret = info_p->ocv_table[0].rdc;
	} else if (i >= info_p->ocv_table_size) {
		i = info_p->ocv_table_size - 1;
		ret = info_p->ocv_table[i].rdc;
	} else {
		high_dod = info_p->ocv_table[i-1].dod;
		low_dod = info_p->ocv_table[i].dod;
		high_rdc = info_p->ocv_table[i-1].rdc;
		low_rdc = info_p->ocv_table[i].rdc;
		ret = interpolation(high_dod, high_rdc, low_dod, low_rdc, dod);
	}

	return ret;
}

void dump_ocv_table(unsigned int idx)
{
	int i, j, offset, cnt = 5;
	char str[256];

	if (mt_ppb_debug) {
		pr_info("table[%d] temp=%d qmax=%d table_size=%d [idx, mah, vol, soc, rdc]\n", idx,
			fg_data.fg_info[idx].temp, fg_data.fg_info[idx].qmax,
			fg_data.fg_info[idx].ocv_table_size);

		for (i = 0; i * cnt < fg_data.fg_info[idx].ocv_table_size; i++) {
			offset = 0;
			memset(str, 0, sizeof(str));
			for (j = 0; j < cnt; j++) {
				if (i*cnt+j >= fg_data.fg_info[idx].ocv_table_size)
					break;

				offset += snprintf(str + offset, 256 - offset,
					"(%3d %5d %5d %5d %5d) ",
					i*cnt+j, fg_data.fg_info[idx].ocv_table[i*cnt+j].mah,
					fg_data.fg_info[idx].ocv_table[i*cnt+j].voltage,
					fg_data.fg_info[idx].ocv_table[i*cnt+j].dod,
					fg_data.fg_info[idx].ocv_table[i*cnt+j].rdc);
			}
			pr_info("%s\n", str);
		}
	}
}

void update_ocv_table(int temp, int qmax)
{
	int i, j, ht, lt;
	struct fg_info_t *h_info_p, *l_info_p, *c_info_p;
	struct ocv_table_t *h_table_p, *l_table_p, *c_table_p;

	c_info_p = &fg_data.fg_info[0];

	if (c_info_p->temp == temp)
		return;

	for (i = 1; i <= fg_data.fg_info_size; i++) {
		if(temp > fg_data.fg_info[i].temp)
			break;
	}

	c_info_p->temp = temp;
	if (i == 1) {
		c_info_p->qmax = fg_data.fg_info[1].qmax;
		c_info_p->ocv_table_size = fg_data.fg_info[1].ocv_table_size;
		memcpy(&c_info_p->ocv_table[0], &fg_data.fg_info[1].ocv_table[0],
			sizeof(struct ocv_table_t) * c_info_p->ocv_table_size);
		if (qmax != 0) {
			c_info_p->qmax = qmax;
			for (j = 0; j < c_info_p->ocv_table_size; j++) {
				c_table_p = &c_info_p->ocv_table[j];
				c_table_p->dod = c_table_p->mah * 10000 / qmax;
			}
		}
		goto out;
	} else if (i == fg_data.fg_info_size + 1) {
		c_info_p->qmax = fg_data.fg_info[fg_data.fg_info_size].qmax;
		c_info_p->ocv_table_size = fg_data.fg_info[fg_data.fg_info_size].ocv_table_size;
		memcpy(&c_info_p->ocv_table[0],
			&fg_data.fg_info[fg_data.fg_info_size].ocv_table[0],
			sizeof(struct ocv_table_t) * c_info_p->ocv_table_size);
		if (qmax != 0) {
			c_info_p->qmax = qmax;
			for (j = 0; j < c_info_p->ocv_table_size; j++) {
				c_table_p = &c_info_p->ocv_table[j];
				c_table_p->dod = c_table_p->mah * 10000 / qmax;
			}
		}
		goto out;
	}

	h_info_p = &fg_data.fg_info[i-1];
	l_info_p = &fg_data.fg_info[i];
	ht = h_info_p->temp;
	lt = l_info_p->temp;

	if (qmax != 0)
		c_info_p->qmax = qmax;
	else
		c_info_p->qmax = interpolation(ht, h_info_p->qmax, lt, l_info_p->qmax, temp);

	for (i = 0; i < h_info_p->ocv_table_size; i++) {
		h_table_p = &h_info_p->ocv_table[i];
		l_table_p = &l_info_p->ocv_table[i];
		c_table_p = &c_info_p->ocv_table[i];

		c_table_p->mah = interpolation(ht, h_table_p->mah, lt, l_table_p->mah, temp);
		c_table_p->rdc = interpolation(ht, h_table_p->rdc, lt, l_table_p->rdc, temp);
		c_table_p->voltage = interpolation(ht, h_table_p->voltage, lt, l_table_p->voltage,
			temp);
		if (qmax != 0) {
			c_table_p->dod = c_table_p->mah * 10000 / qmax;
		} else {
			c_table_p->dod = interpolation(ht, h_table_p->dod, lt, l_table_p->dod,
				temp);
		}
	}
out:
	dump_ocv_table(0);
}

static int __used cal_imax(int vbat, int uvlo, int ocp, int rdc, int rac)
{
	int ret;

	if (vbat - ocp * (rac + rdc) / 2000 < uvlo)
		ret = (vbat - uvlo) * 2000 / (rdc + rac);
	else
		ret = ocp;

	return ret;
}

static int __used cal_uvlo(int vbat, int uvlo, int ocp, int rdc, int rac)
{
	int ret;

	if (vbat - ocp * (rdc + rac) / 2000 < uvlo)
		ret = uvlo;
	else
		ret = vbat - ocp * (rdc + rac) / 2000;

	return ret;
}

static int __used cal_max_bat_power(int vbat, int uvlo, int ocp, int rdc, int rac, int i)
{
	int ret;

	if (vbat - ocp * (rdc + rac) / 2000 <= uvlo)
		ret = vbat * i / 1000;
	else
		ret = vbat * ocp / 1000;

	return ret;
}

static int __used cal_max_sys_power(int bat_pwr, int imax, int rdc, int rac)
{
	int ret;

	ret = bat_pwr - (imax >> 2) * imax / 1000 * (rdc + rac) / 1000;

	return ret;
}

static int __used get_sys_power_budget(int ocv, int rdc, int rac, int ocp, int uvlo)
{
	int imax, uv, bat_pwr, sys_pwr;

	imax = cal_imax(ocv, uvlo, ocp, rdc, rac);
	uv = cal_uvlo(ocv, uvlo, ocp, rdc, rac);
	bat_pwr = cal_max_bat_power(ocv, uv, ocp, rdc, rac, imax);
	sys_pwr = cal_max_sys_power(bat_pwr, imax, rdc, rac);

	return sys_pwr;
}

static void bat_handler(struct work_struct *work)
{
	struct power_supply *psy = pb.psy, *psy_mtk;
	union power_supply_propval val;
	static int last_soc = MAX_VALUE, last_temp = MAX_VALUE, last_uisoc = MAX_VALUE, last_combo0_uisoc = MAX_VALUE;
	static unsigned int last_aging_stage;
	unsigned int temp_stage, uisoc_stage, aging_stage = 0;
	int ret = 0, soc, temp, volt, qmax, cycle, bat_rdc, uisoc, i, cb_idx;
	bool loop;

	if (!pb.psy)
		return;

	psy_mtk = power_supply_get_by_name("mtk-gauge");
	if (!psy_mtk || IS_ERR(psy_mtk)) {
		psy_mtk = devm_power_supply_get_by_phandle(pb.dev, "gauge");
		if (!psy_mtk || IS_ERR(psy_mtk)) {
			pr_info("psy_mtk can't get from mtk-gauge and phandle %p\n", psy_mtk);
			return;
		}
	}

	ret = power_supply_get_property(psy_mtk, POWER_SUPPLY_PROP_ENERGY_NOW, &val);
	if (ret)
		return;

	soc = val.intval / 100;
	if (soc == 0)
		return;

	ret = power_supply_get_property(psy_mtk, POWER_SUPPLY_PROP_ENERGY_FULL, &val);
	if (ret)
		qmax = 0;
	else
		qmax = val.intval;

	if (strcmp(psy->desc->name, "battery") != 0)
		return;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TEMP, &val);
	if (ret)
		return;

	temp = val.intval / 10;
	temp_stage = pb.temp_cur_stage;

	cycle = 0;
	if (pb.aging_max_stage > 0) {
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &val);
		if (!ret)
			cycle = val.intval;
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &val);
		if (ret)
			return;
	uisoc = val.intval;
	uisoc_stage = pb.uisoc_cur_stage;

	if (temp != last_temp) {
		do {
			loop = false;
			if (temp < last_temp && temp_stage < pb.temp_max_stage) {
				if (temp < pb.temp_thd[temp_stage]) {
					temp_stage++;
					loop = true;
				}
			} else if (temp > last_temp && temp_stage > 0) {
				if (temp >= pb.temp_thd[temp_stage-1]) {
					temp_stage--;
					loop = true;
				}
			}
		} while (loop);
		pb.cur_rdc = pb.rdc[temp_stage];
		pb.cur_rac = pb.rac[temp_stage];
		pb.temp_cur_stage = temp_stage;
		update_ocv_table(temp, qmax);
	}

	if (uisoc != last_uisoc) {
		do {
			loop = false;
			if (uisoc < last_uisoc && uisoc_stage < pb.uisoc_max_stage) {
				if (uisoc < pb.uisoc_thd[uisoc_stage]) {
					uisoc_stage++;
					loop = true;
				}
			} else if (uisoc > last_uisoc && uisoc_stage > 0) {
				if (uisoc >= pb.uisoc_thd[uisoc_stage-1]) {
					uisoc_stage--;
					loop = true;
				}
			}
		} while (loop);
		pb.uisoc_cur_stage = uisoc_stage;
	}

	if (pb.version >= 2) {
		for (i = 0; i < pb.aging_max_stage; i++) {
			if (cycle < pb.aging_thd[i])
				break;
		}
		aging_stage = i;
	}

	if (temp != last_temp || soc != last_soc || uisoc != last_uisoc || pb.combo0_uisoc != last_combo0_uisoc
		|| aging_stage != last_aging_stage) {
		if (timer_pending(&ppb_dbg_timer)) {
			del_timer_sync(&ppb_dbg_timer);
			if (pb.version == 2)
				ppb_print_dbg_log(NULL);
			if (pb.version == 3)
				ppb3_print_dbg_log(NULL);
		}

		volt = soc_to_ocv(soc * 100, 0, pb.soc_err);
		pb.ocv = volt / 10;

		if (pb.version >= 2) {
			bat_rdc = soc_to_rdc(soc * 100, 0) / 10;

			if (aging_stage > 0 && aging_stage <= pb.aging_max_stage)
				pb.aging_rdc = bat_rdc * pb.aging_multi[aging_stage-1] / 1000;
			else
				pb.aging_rdc = 0;

			pb.cur_rdc = bat_rdc + pb.circuit_rdc + pb.aging_rdc;
		}
		pb.sys_power = get_sys_power_budget(pb.ocv, pb.cur_rdc, pb.cur_rac, pb.ocp, pb.uvlo);

		volt = soc_to_ocv(soc * 100, 0, 0);
		if (pb.version >= 2)
			pb.cur_rdc = soc_to_rdc(soc * 100, 0) / 10 + pb.circuit_rdc + pb.aging_rdc;

		pb.ocv_noerr = volt / 10;

		cb_idx = (pb.uisoc_max_stage + 1) * pb.temp_cur_stage + pb.uisoc_cur_stage;
		if (pb.version >= 2 && (uisoc >= pb.combo0_uisoc ||
			(pb.combo0_uisoc == DEFAULT_COMBO0_UISOC && pb.fix_combo0[cb_idx] > 0)) ) {
			pb.sys_power_noerr = 100000;
		} else
			pb.sys_power_noerr = get_sys_power_budget(pb.ocv_noerr, pb.cur_rdc, pb.cur_rac, pb.ocp,
				pb.uvlo);

		pb.soc = soc;
		pb.temp = temp;
		pb.uisoc = uisoc;
		pb.aging_cur_stage = aging_stage;
		kicker_ppb_request_power(KR_BUDGET, pb.sys_power);
#if !IS_ENABLED(CONFIG_MTK_GPU_LEGACY)
		if (pb.version == 2)
			notify_gpueb();
#endif
		pr_info("%s: update vsys-er[p,v]=%d,%d vsys[p,v]=%d,%d SOC[soc,ui,s]=%d,%d,%d R[C,A,dc,ac]=%d,%d,%d,%d T[t,s]=%d,%d A[c,s]=%d,%d\n",
			__func__, pb.sys_power, pb.ocv, pb.sys_power_noerr,
			pb.ocv_noerr, soc, uisoc, pb.temp_cur_stage, pb.circuit_rdc, pb.aging_rdc, pb.cur_rdc,
			pb.cur_rac, temp, pb.temp_cur_stage, cycle, pb.aging_cur_stage);

		if (pb.sys_power_noerr <= MBRAIN_NOTIFY_BUDGET_THD && soc != last_soc && cb_func)
			cb_func();

		if (pb.version == 3 && pb.is_evb == 0)
			spbm_scmi_to_sspm_command(SPBM_SCMI_SOC_BAT_PWR,
		soc, pb.sys_power, pb.sys_power_noerr, SPBM_SCMI_SET);


		last_temp = temp;
		last_soc = soc;
		last_uisoc = uisoc;
		last_aging_stage = aging_stage;
		last_combo0_uisoc = pb.combo0_uisoc;
	}
}

int ppb_psy_event(struct notifier_block *nb, unsigned long event, void *v)
{
	struct power_supply *psy = v;

	if (!ppb_ctrl.ppb_stop) {
		pb.psy = psy;
		schedule_work(&pb.bat_work);
		return NOTIFY_DONE;
	}

	return NOTIFY_DONE;
}

static int __used read_mtk_gauge_dts(struct platform_device *pdev)
{
	int i, j, ret, num;
	struct ocv_table_t *table_p;
	struct fg_info_t *info_p;
	struct device_node *np;
	char str[STR_SIZE];

	np = of_find_node_by_name(NULL, "mtk-gauge");
	if (!np)
		dev_notice(&pdev->dev, "get gauge node fail\n");

	read_dts_val(np, "active-table", &fg_data.fg_info_size, 1);
	if (fg_data.fg_info_size > 10)
		fg_data.fg_info_size = 10;

#ifdef DYNAMIC_ALLOC_PB_INFO
	fg_data.fg_info = devm_kmalloc_array(&pdev->dev, fg_data.fg_info_size + 1,
		sizeof(struct fg_info_t), GFP_KERNEL);
	if (!fg_data.fg_info)
		return -ENOMEM;
#endif
	fg_data.fg_info[0].temp = MAX_VALUE;

	for (i = 0; i < fg_data.fg_info_size; i++) {
		info_p = &fg_data.fg_info[i+1];

		num = 4;
		read_dts_val(np, "g-q-max-row", &num, 1);
		read_dts_val_by_idx(np, "g-q-max", i*num+fg_data.bat_type,
			&info_p->qmax, 1);

		ret = snprintf(str, STR_SIZE, "temperature-t%d", i);
		if (ret < 0) {
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			break;
		}
		read_dts_val(np, str, &info_p->temp, 1);

		ret = snprintf(str, STR_SIZE, "battery%d-profile-t%d-num", fg_data.bat_type, i);
		if (ret < 0) {
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			break;
		}
		read_dts_val(np, str, &info_p->ocv_table_size, 1);

		if (info_p->ocv_table_size > 100)
			info_p->ocv_table_size = 100;

		if (i == 0) {
#ifdef DYNAMIC_ALLOC_PB_INFO
			fg_data.fg_info[0].ocv_table = devm_kmalloc_array(&pdev->dev,
				info_p->ocv_table_size, sizeof(struct ocv_table_t), GFP_KERNEL);
			if (!fg_data.fg_info[0].ocv_table)
				return -ENOMEM;
#endif
			fg_data.fg_info[0].ocv_table_size = info_p->ocv_table_size;
		} else{
			if (info_p->ocv_table_size != fg_data.fg_info[i].ocv_table_size) {
				pr_info("%s: table size not align %d %d !!!\n", __func__,
					info_p->ocv_table_size, fg_data.fg_info[i].ocv_table_size);
					break;
			}
		}

		num = sizeof(struct ocv_table_t) / sizeof(unsigned int);
		ret = snprintf(str, STR_SIZE, "battery%d-profile-t%d-col", fg_data.bat_type, i);
		if (ret < 0) {
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			break;
		}
		read_dts_val(np, str, &num, 1);
#ifdef DYNAMIC_ALLOC_PB_INFO
		info_p->ocv_table = devm_kmalloc_array(&pdev->dev, info_p->ocv_table_size,
			sizeof(struct ocv_table_t), GFP_KERNEL);
		if (!info_p->ocv_table)
			return -ENOMEM;
#endif
		ret = snprintf(str, STR_SIZE, "battery%d-profile-t%d", fg_data.bat_type, i);
		if (ret < 0) {
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			break;
		}

		for (j = 0; j < info_p->ocv_table_size; j++) {
			table_p = &info_p->ocv_table[j];
			read_dts_val_by_idx(np, str, j*num, &table_p->mah, 1);
			read_dts_val_by_idx(np, str, j*num+1, &table_p->voltage, 1);
			read_dts_val_by_idx(np, str, j*num+2, &table_p->rdc, 1);
			if (info_p->ocv_table[j].dod == 0 && info_p->qmax > 0)
				info_p->ocv_table[j].dod = info_p->ocv_table[j].mah * 1000 /
					info_p->qmax;

		}
	}
	return 0;
}

static int __used read_mtk_ppb_bat_dts(struct platform_device *pdev, struct device_node *np)
{
	int i, j, ret, num;
	struct ocv_table_t *table_p;
	struct fg_info_t *info_p;
	char str[STR_SIZE];

#ifdef DYNAMIC_ALLOC_PB_INFO
	fg_data.fg_info = devm_kmalloc_array(&pdev->dev, fg_data.fg_info_size + 1,
		sizeof(struct fg_info_t), GFP_KERNEL);
	if (!fg_data.fg_info)
		return -ENOMEM;
#endif
	fg_data.fg_info[0].temp = MAX_VALUE;

	for (i = 0; i < fg_data.fg_info_size; i++) {
		info_p = &fg_data.fg_info[i+1];

		ret = snprintf(str, STR_SIZE, "bat%d-ocv-table-t%d-qmax", fg_data.bat_type, i);
		if (ret < 0) {
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			break;
		}
		read_dts_val(np, str, &info_p->qmax, 1);

		ret = snprintf(str, STR_SIZE, "bat%d-ocv-table-t%d-temperature", fg_data.bat_type,
				i);
		if (ret < 0) {
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			break;
		}
		read_dts_val(np, str, &info_p->temp, 1);

		ret = snprintf(str, STR_SIZE, "bat%d-ocv-table-t%d-size", fg_data.bat_type, i);
		if (ret < 0) {
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			break;
		}
		read_dts_val(np, str, &info_p->ocv_table_size, 1);

		if (info_p->ocv_table_size > 100)
			info_p->ocv_table_size = 100;

		if (i == 0) {
#ifdef DYNAMIC_ALLOC_PB_INFO
			fg_data.fg_info[0].ocv_table = devm_kmalloc_array(&pdev->dev,
				info_p->ocv_table_size, sizeof(struct ocv_table_t), GFP_KERNEL);
			if (!fg_data.fg_info[0].ocv_table)
				return -ENOMEM;
#endif
			fg_data.fg_info[0].ocv_table_size = info_p->ocv_table_size;
		} else{
			//fixme
			if (info_p->ocv_table_size != fg_data.fg_info[i].ocv_table_size) {
				pr_info("%s: table size not align %d %d !!!\n", __func__,
					info_p->ocv_table_size, fg_data.fg_info[i].ocv_table_size);
					break;
			}
		}

		num = sizeof(struct ocv_table_t) / sizeof(unsigned int);
#ifdef DYNAMIC_ALLOC_PB_INFO
		info_p->ocv_table = devm_kmalloc_array(&pdev->dev, info_p->ocv_table_size,
			sizeof(struct ocv_table_t), GFP_KERNEL);
		if (!info_p->ocv_table)
			return -ENOMEM;
#endif
		ret = snprintf(str, STR_SIZE, "bat%d-ocv-table-t%d", fg_data.bat_type, i);
		if (ret < 0) {
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			break;
		}

		for (j = 0; j < info_p->ocv_table_size; j++) {
			table_p = &info_p->ocv_table[j];
			read_dts_val_by_idx(np, str, j*num, &table_p->mah, 1);
			read_dts_val_by_idx(np, str, j*num+1, &table_p->voltage, 1);
			read_dts_val_by_idx(np, str, j*num+2, &table_p->dod, 1);
			read_dts_val_by_idx(np, str, j*num+3, &table_p->rdc, 1);
			if (info_p->ocv_table[j].dod == 0 && info_p->qmax > 0)
				info_p->ocv_table[j].dod = info_p->ocv_table[j].mah * 1000 /
					info_p->qmax;

		}
	}
	return 0;
}

static int __used read_power_budget_dts(struct platform_device *pdev)
{
	int i, ret;
	int num, offset = 0;
	struct device_node *np;
	char str[STR_SIZE];

	np = of_find_node_by_name(NULL, "mtk-gauge");
	if (!np)
		dev_notice(&pdev->dev, "get gauge node fail\n");

	if (read_dts_val(np, "bat_type", &fg_data.bat_type, 1)) {
		fg_data.bat_type = 0;
		pr_info("warning: can't get bat_type in dts, set to 0\n");
	}

	np = pdev->dev.of_node;
	if (!np) {
		dev_notice(&pdev->dev, "get peak power budget dts fail\n");
		return -ENODATA;
	}

	if (read_dts_val(np, "ppb-version", &pb.version, 1))
		pb.version = 1;

	if (read_dts_val(np, "soc-error", &pb.soc_err, 1))
		pb.soc_err = SOC_ERROR;

	if (read_dts_val(np, "hpt-exclude-lbat-cg-throttle", &pb.hpt_exclude_lbat_cg_thl, 1))
		pb.hpt_exclude_lbat_cg_thl = 0;

	num = of_property_count_u32_elems(np, "temperature-threshold");
	if (num > 6 || num < 0) {
		pr_info("wrong temp_max_stage number %d, set to 0\n", num);
		num = 0;
	}

	if (num > 0)
		of_property_read_u32_array(np, "temperature-threshold", &pb.temp_thd[0], num);

	pb.temp_max_stage = num;

	num = of_property_count_u32_elems(np, "uisoc-threshold");
	if (num > 6 || num < 0) {
		pr_info("wrong temp_max_stage number %d, set to 0\n", num);
		num = 0;
	}

	if (num > 0)
		of_property_read_u32_array(np, "uisoc-threshold", &pb.uisoc_thd[0], num);

	pb.uisoc_max_stage = num;

	num = of_property_count_u32_elems(np, "ppt-fix-cb0");
	if (num != (pb.temp_max_stage + 1) * (pb.uisoc_max_stage + 1)) {
		pr_info("wrong ppt-fix-cb0 number %d, set to 0\n", num);
		num = 0;
	}

	if (num > 0)
		of_property_read_u32_array(np, "ppt-fix-cb0", &pb.fix_combo0[0], num);

	if (pb.version >= 2) {
		if (read_dts_val(np, "battery-circult-rdc", &pb.circuit_rdc, 1))
			pb.circuit_rdc = BAT_CIRCUIT_DEFAULT_RDC;

		if (read_dts_val(np, "soc-max-level", &pb.hpt_max_lv, 1))
			pb.hpt_max_lv = 0;

		if (pb.hpt_max_lv >= BATTERY_PERCENT_LEVEL_NUM)
			pb.hpt_max_lv = BATTERY_PERCENT_LEVEL_NUM - 1;

		pb.hpt_lv_t[0] = HPT_INIT_SETTING;
		for (i = 1; i <= pb.hpt_max_lv; i++) {
			ret = snprintf(str, STR_SIZE, "%s%d", "soc-hpt-ctrl-lv", i);
			if (ret < 0) {
				pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
				break;
			}
			if (read_dts_val(np, str, &pb.hpt_lv_t[i], 1))
				pb.hpt_lv_t[i] = HPT_INIT_SETTING;
		}
	} else {
		for (i = 0; i <= pb.temp_max_stage; i++) {
			ret = snprintf(str, STR_SIZE, "battery%d-path-rdc-t%d", fg_data.bat_type,
				i);
			if (ret < 0) {
				pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
				break;
			}
			if (read_dts_val(np, str, &pb.rdc[i], 1))
				pb.rdc[i] = BAT_PATH_DEFAULT_RDC;
		}
	}

	for (i = 0; i <= pb.temp_max_stage; i++) {
		ret = snprintf(str, STR_SIZE, "battery%d-path-rac-t%d", fg_data.bat_type, i);
		if (ret < 0) {
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			break;
		}
		if (read_dts_val(np, str, &pb.rac[i], 1))
			pb.rac[i] = BAT_PATH_DEFAULT_RAC;
	}

	ret = snprintf(str, STR_SIZE, "bat%d-aging-threshold", fg_data.bat_type);
	if (ret < 0)
		pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);

	num = of_property_count_u32_elems(np, str);
	if (num > 10 || num < 0) {
		pr_info("wrong aging_max_stage number %d, set to 0\n", num);
		num = 0;
	}

	pb.aging_max_stage = num;

	if (num > 0)
		of_property_read_u32_array(np, str, &pb.aging_thd[0], num);

	ret = snprintf(str, STR_SIZE, "bat%d-aging-multiples", fg_data.bat_type);
	if (ret < 0)
		pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);

	if (num > 0)
		of_property_read_u32_array(np, str, &pb.aging_multi[0], num);

	read_dts_val(np, "system-ocp", &pb.ocp, 1);
	read_dts_val(np, "system-uvlo", &pb.uvlo, 1);

	ret = snprintf(str, STR_SIZE, "ocp=%d uvlo=%d temp_max_s=%d aging_max_s=%d ",
		pb.ocp, pb.uvlo, pb.temp_max_stage,  pb.aging_max_stage);
	if (ret < 0)
		pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
	else
		offset += ret;

	for (i = 0; i <= pb.temp_max_stage; i++) {
		ret = snprintf(str + offset, STR_SIZE - offset, "[%d](Rdc,Rac)=(%d,%d) ",
			i, pb.rdc[i], pb.rac[i]);
		if (ret < 0) {
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			break;
		}
		offset += ret;
	}

	for (i = 0; i < pb.aging_max_stage; i++) {
		ret = snprintf(str + offset, STR_SIZE - offset, "[%d]AG(t,m)=(%u,%u) ",
			i, pb.aging_thd[i], pb.aging_multi[i]);
		if (ret < 0) {
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			break;
		}
		offset += ret;
	}

	pr_info("%s\n", str);

	ret = read_dts_val(np, "bat-ocv-table-num", &fg_data.fg_info_size, 1);

	if (ret || fg_data.fg_info_size == 0)
		read_mtk_gauge_dts(pdev);
	else
		read_mtk_ppb_bat_dts(pdev, np);

	return 0;
}

static int mt_ppb_debug_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "(MODE/CG_MIN/VSYS_BUDGET/VSYS_ACK)=%u,%u,%u,%u\n",
		ppb_read_sram(PPB_MODE),
		ppb_read_sram(PPB_CG_PWR),
		ppb_read_sram(PPB_VSYS_PWR),
		ppb_read_sram(PPB_VSYS_ACK));

	seq_printf(m, "(FLASH/AUDIO/CAMERA/DISPLAY/APU/DRAM)=%u,%u,%u,%u,%u,%u\n",
		ppb_read_sram(PPB_FLASH_PWR),
		ppb_read_sram(PPB_AUDIO_PWR),
		ppb_read_sram(PPB_CAMERA_PWR),
		ppb_read_sram(PPB_DISPLAY_PWR),
		ppb_read_sram(PPB_APU_PWR),
		ppb_read_sram(PPB_DRAM_PWR));

	seq_printf(m, "(MD/WIFI/APU_ACK/BOOT_MODE)=%u,%u,%u,%u\n",
		ppb_read_sram(PPB_MD_PWR),
		ppb_read_sram(PPB_WIFI_PWR),
		ppb_read_sram(PPB_APU_PWR_ACK),
		ppb_read_sram(PPB_BOOT_MODE));

	return 0;
}

static int mt_ppb_dump_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u\n",
		ppb_read_sram(PPB_MODE),
		ppb_read_sram(PPB_CG_PWR),
		ppb_read_sram(PPB_VSYS_PWR),
		ppb_read_sram(PPB_VSYS_ACK),
		ppb_read_sram(PPB_FLASH_PWR),
		ppb_read_sram(PPB_AUDIO_PWR),
		ppb_read_sram(PPB_CAMERA_PWR),
		ppb_read_sram(PPB_DISPLAY_PWR),
		ppb_read_sram(PPB_APU_PWR),
		ppb_read_sram(PPB_DRAM_PWR),
		ppb_read_sram(PPB_MD_PWR),
		ppb_read_sram(PPB_WIFI_PWR),
		ppb_read_sram(PPB_APU_PWR_ACK),
		ppb_read_sram(PPB_CG_PWR_THD),
		ppb_read_sram(PPB_CG_PWR_CNT));

	return 0;
}

static int mt_ppb_debug_log_proc_show(struct seq_file *m, void *v)
{
	if (mt_ppb_debug)
		seq_puts(m, "ppb debug log enabled\n");
	else
		seq_puts(m, "ppb debug log disabled\n");

	return 0;
}

static ssize_t mt_ppb_debug_log_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	unsigned int len = 0;
	int debug = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtoint(desc, 10, &debug) == 0) {
		if (debug == 0)
			mt_ppb_debug = 0;
		else if (debug == 1)
			mt_ppb_debug = 1;
		else
			pr_notice("should be [0:disable,1:enable]\n");
	} else
		pr_notice("should be [0:disable,1:enable]\n");

	return count;
}

static int mt_ppb_manual_mode_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "manual_mode: %d\n", ppb_ctrl.manual_mode);
	if (ppb_ctrl.manual_mode > 0) {
		seq_printf(m, "(VSYS_BUDGET/REMAIN_BUDGET)=%u,%u\n",
			ppb_manual.vsys_budget,
			ppb_manual.remain_budget);

		seq_printf(m, "(FLASH/AUDIO/CAMERA/DISPLAY/APU/DRAM)=%u,%u,%u,%u,%u,%u\n",
			ppb_manual.loading_flash,
			ppb_manual.loading_audio,
			ppb_manual.loading_camera,
			ppb_manual.loading_display,
			ppb_manual.loading_apu,
			ppb_manual.loading_dram);
	} else {
		seq_printf(m, "(VSYS_BUDGET/REMAIN_BUDGET)=%u,%u\n",
			ppb.vsys_budget,
			ppb.remain_budget);

		seq_printf(m, "(FLASH/AUDIO/CAMERAC/DISPLAY/APU/DRAM)=%u,%u,%u,%u,%u,%u\n",
			ppb.loading_flash,
			ppb.loading_audio,
			ppb.loading_camera,
			ppb.loading_display,
			ppb.loading_apu,
			ppb.loading_dram);
	}

	return 0;
}

static ssize_t mt_ppb_manual_mode_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64], cmd[21];
	unsigned int len = 0;
	int vsys_budget_noerr, vsys_budget, manual_mode = 0;
	int loading_flash, loading_audio, loading_camera;
	int loading_display, loading_apu, loading_dram;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%20s %d %d %d %d %d %d %d %d %d", cmd, &manual_mode, &vsys_budget_noerr,
		&vsys_budget, &loading_flash, &loading_audio, &loading_camera, &loading_display,
		&loading_apu, &loading_dram) != 10) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (strncmp(cmd, "manual", 6))
		return -EINVAL;

	if (manual_mode == 1) {
		ppb_ctrl.manual_mode = manual_mode;
		ppb_manual.vsys_budget_noerr = vsys_budget_noerr;
		ppb_manual.vsys_budget = vsys_budget;
		ppb_manual.loading_flash = loading_flash;
		ppb_manual.loading_audio = loading_audio;
		ppb_manual.loading_camera = loading_camera;
		ppb_manual.loading_display = loading_display;
		ppb_manual.loading_apu = loading_apu;
		ppb_manual.loading_dram = loading_dram;
		ppb_allocate_budget_manager();
	} else if (manual_mode == 0) {
		ppb_ctrl.manual_mode = manual_mode;
		ppb_allocate_budget_manager();
	} else
		pr_notice("ppb manual setting should be 0 or 1\n");

	return count;
}

int get_ppb_mbrain_data(struct ppb_mbrain_data *data)
{
	struct xpu_dbg_t dbg_data;
	int ppb_pwr, md_pwr, wifi_pwr;
	static ktime_t last_ktime;
	ktime_t ktime;

	if (!ppb_ctrl.ppb_drv_done)
		return -ENODEV;

	if (!data)
		return -EINVAL;

	get_xpu_debug_info(&dbg_data);

	ktime = ktime_get();
	data->duration = ktime_us_delta(ktime, last_ktime);
	data->kernel_time = ktime_to_us(ktime);
	last_ktime = ktime;
	data->soc = pb.soc;
	data->temp = pb.temp;
	data->soc_rdc = pb.cur_rdc;
	data->soc_rac = pb.cur_rac;
	data->hpt_bat_budget = pb.sys_power;
	ppb_pwr = ppb_read_sram(PPB_VSYS_PWR);
	md_pwr = ppb_read_sram(PPB_MD_PWR);
	wifi_pwr = ppb_read_sram(PPB_WIFI_PWR);
	data->hpt_cg_budget = ppb_pwr - md_pwr - wifi_pwr;
	data->hpt_cpub_thr_cnt = dbg_data.cpub_cnt - last_mbrain_xpu_dbg.cpub_cnt;
	data->hpt_cpub_thr_time = dbg_data.cpub_th_t - last_mbrain_xpu_dbg.cpub_th_t;
	data->hpt_cpum_thr_cnt = dbg_data.cpum_cnt - last_mbrain_xpu_dbg.cpum_cnt;
	data->hpt_cpum_thr_time = dbg_data.cpum_th_t - last_mbrain_xpu_dbg.cpum_th_t;
	data->hpt_gpu_thr_cnt = dbg_data.gpu_cnt - last_mbrain_xpu_dbg.gpu_cnt;
	data->hpt_gpu_thr_time = dbg_data.gpu_th_t - last_mbrain_xpu_dbg.gpu_th_t;
	if (cgppt_dbg_ops && cgppt_dbg_ops->get_cpub_sf)
		data->hpt_cpub_sf = cgppt_dbg_ops->get_cpub_sf();

	if (cgppt_dbg_ops && cgppt_dbg_ops->get_cpum_sf)
		data->hpt_cpum_sf = cgppt_dbg_ops->get_cpum_sf();

	if (cgppt_dbg_ops && cgppt_dbg_ops->get_gpu_sf)
		data->hpt_gpu_sf = cgppt_dbg_ops->get_gpu_sf();

	if (cgppt_dbg_ops && cgppt_dbg_ops->get_cg_bgt)
		data->ppb_cg_budget = cgppt_dbg_ops->get_cg_bgt();

	if (cgppt_dbg_ops && cgppt_dbg_ops->get_combo)
		data->ppb_combo = cgppt_dbg_ops->get_combo();

	data->ppb_c_combo0 = 0;
	data->ppb_g_combo0 = 0;
	memcpy(&last_mbrain_xpu_dbg, &dbg_data, sizeof(struct xpu_dbg_t));

	return 0;
}
EXPORT_SYMBOL(get_ppb_mbrain_data);

int get_hpt_mbrain_data(struct hpt_mbrain_data *data)
{
	struct ppb3_dbg_t ppb3_dbg_data;

	memset(&ppb3_dbg_data, 0, sizeof(ppb3_dbg_data));

	get_ppb3_debug_info(&ppb3_dbg_data);

	data->oc_count = ppb3_dbg_data.oc_count;
	data->oc_duration_us = ppb3_dbg_data.oc_duration_us;

	return 0;
}
EXPORT_SYMBOL(get_hpt_mbrain_data);

int register_ppb_cgppt_cb(struct ppb_cgppt_dbg_operation *ops)
{
	if (!ops)
		return -EINVAL;

	cgppt_dbg_ops = ops;
	return 0;
}
EXPORT_SYMBOL(register_ppb_cgppt_cb);

int register_ppb_mbrian_cb(ppb_mbrain_func func_p)
{
	if (!func_p)
		return -EINVAL;

	cb_func = func_p;
	return 0;
}
EXPORT_SYMBOL(register_ppb_mbrian_cb);

int unregister_ppb_mbrian_cb(void)
{
	cb_func = NULL;
	return 0;
}
EXPORT_SYMBOL(unregister_ppb_mbrian_cb);

int register_hpt_mbrian_cb(ppb_mbrain_func func_p)
{
	if (!func_p)
		return -EINVAL;

	cb_func_hpt = func_p;
	return 0;
}
EXPORT_SYMBOL(register_hpt_mbrian_cb);

int unregister_hpt_mbrian_cb(void)
{
	cb_func_hpt = NULL;
	return 0;
}
EXPORT_SYMBOL(unregister_hpt_mbrian_cb);

static int mt_ppb_stop_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "ppb stop: %d\n", ppb_ctrl.ppb_stop);
	return 0;
}

static ssize_t mt_ppb_stop_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64], cmd[21];
	unsigned int len = 0, stop = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%20s %d", cmd, &stop) != 2) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (strncmp(cmd, "stop", 4))
		return -EINVAL;

	if (stop == 0 || stop == 1)
		ppb_ctrl.ppb_stop = stop;
	else
		pr_notice("ppb stop should be 0 or 1\n");

	return count;
}

static int mt_peak_power_mode_proc_show(struct seq_file *m, void *v)
{
	int mode;

	mode = ppb_read_sram(PPB_MODE);
	seq_printf(m, "ppb_mode: %d\n", mode);
	return 0;
}

static ssize_t mt_peak_power_mode_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64], cmd[21];
	unsigned int len = 0, mode = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%20s %d", cmd, &mode) != 2) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (strncmp(cmd, "mode", 4))
		return -EINVAL;

	ppb_ctrl.ppb_mode = mode;
	ppb_write_sram(mode, PPB_MODE);
	lbat_set_ppb_mode(mode);
	bat_oc_set_ppb_mode(mode);

	return count;
}

static int mt_ppb_cg_min_power_proc_show(struct seq_file *m, void *v)
{
	unsigned int power;

	power = ppb_read_sram(PPB_CG_PWR);
	seq_printf(m, "ppb CG min power: %u\n", power);
	return 0;
}

static ssize_t mt_ppb_cg_min_power_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64], cmd[21];
	unsigned int len = 0, power = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%20s %u", cmd, &power) != 2) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	ppb_write_sram(power, PPB_CG_PWR);

	return count;
}

static int mt_ppb_camera_power_proc_show(struct seq_file *m, void *v)
{
	int manual_mode = ppb_ctrl.manual_mode;

	if (manual_mode == 0)
		seq_printf(m, "ppb manual mode: %d, camera power: %d\n",
						manual_mode, ppb.loading_camera);
	else
		seq_printf(m, "ppb manual mode: %d, camera power: %d\n",
						manual_mode, ppb_manual.loading_camera);

	return 0;
}

static ssize_t mt_ppb_camera_power_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64], cmd[21];
	unsigned int len = 0;
	int power = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%20s %d", cmd, &power) != 2) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (power < 0) {
		pr_notice("ppb camera power should not be negative value\n");
		return count;
	}

	kicker_ppb_request_power(KR_CAMERA, power);
	return count;
}

static int mt_ppb_cg_budget_thd_proc_show(struct seq_file *m, void *v)
{
	int thd = 0;

	thd = ppb_read_sram(PPB_CG_PWR_THD);
	seq_printf(m, "CG budget threshold: %d\n", thd);
	return 0;
}

static ssize_t mt_ppb_cg_budget_thd_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64];
	unsigned int len = 0;
	int thd = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtoint(desc, 10, &thd) != 0) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}
	if (thd < 0) {
		pr_notice("ppb camera power should not be negative value\n");
		return count;
	}

	ppb_write_sram(thd, PPB_CG_PWR_THD);
	return count;
}

static int mt_ppb_cg_budget_cnt_proc_show(struct seq_file *m, void *v)
{
	int cnt = 0;

	cnt = ppb_read_sram(PPB_CG_PWR_CNT);
	seq_printf(m, "CG budget threshold count: %d\n", cnt);
	return 0;
}

static ssize_t mt_ppb_cg_budget_cnt_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64];
	unsigned int len = 0;
	int cnt = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtoint(desc, 10, &cnt) != 0) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (cnt < 0) {
		pr_notice("ppb budget count should not be negative value\n");
		return count;
	}

	ppb_write_sram(cnt, PPB_CG_PWR_CNT);
	return count;
}

static int mt_hpt_debug_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "(SF_EN/VSYS_BUDGET/DELAY)=%u,%u,%u\n",
		ppb_read_sram(HPT_SF_ENABLE),
		ppb_read_sram(PPB_VSYS_PWR_NOERR),
		ppb_read_sram(HPT_DELAY_TIME));

	seq_printf(m, "(CPUB_SF_L1:L2/CPUM_SF_L1:L2/GPU_SF_L1:L2)=%u:%u/%u:%u/%u:%u\n",
		ppb_read_sram(HPT_CPU_B_SF_L1),
		ppb_read_sram(HPT_CPU_B_SF_L2),
		ppb_read_sram(HPT_CPU_M_SF_L1),
		ppb_read_sram(HPT_CPU_M_SF_L2),
		ppb_read_sram(HPT_GPU_SF_L1),
		ppb_read_sram(HPT_GPU_SF_L2));

	return 0;
}

static int mt_hpt_dump_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%u, %u\n",
		ppb_read_sram(HPT_SF_ENABLE),
		ppb_read_sram(PPB_VSYS_PWR_NOERR));

	return 0;
}

static int mt_hpt_ctrl_proc_show(struct seq_file *m, void *v)
{
	unsigned int reg = 0;

	if (pb.version == 2) {
		reg = hpt_ctrl_read(HPT_CTRL);
		seq_printf(m, "0x%x\n", reg);
	} else if (pb.version == 3) {
		reg = hpt_ctrl_read(HPT_CTRL);
		seq_printf(m, "hpt_ctrl: 0x%x\n", reg);
		reg = hpt_status_read(0);
		seq_printf(m, "hpt_status: 0x%x\n", reg);
	} else
		seq_printf(m, "hpt_ctrl not supported\n");
	return 0;
}

static ssize_t mt_hpt_ctrl_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64], cmd[21];
	unsigned int len = 0, val = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%20s %d", cmd, &val) != 2) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (strncmp(cmd, "ctrl", 4))
		return -EINVAL;

	if (val <= 7) {
		hpt_ctrl_write(val, HPT_CTRL_SET);
		val = ~val & 0x7;
		hpt_ctrl_write(val, HPT_CTRL_CLR);
	} else
		pr_notice("hpt ctrl should be 0 ~ 7\n");

	return count;
}

static int mt_hpt_sf_setting_proc_show(struct seq_file *m, void *v)
{
	unsigned int cpub_lv1, cpub_lv2, cpum_lv1, cpum_lv2, gpu_lv1, gpu_lv2, enable, delay;

	enable = ppb_read_sram(HPT_SF_ENABLE);
	cpub_lv1 = ppb_read_sram(HPT_CPU_B_SF_L1);
	cpub_lv2 = ppb_read_sram(HPT_CPU_B_SF_L2);
	cpum_lv1 = ppb_read_sram(HPT_CPU_M_SF_L1);
	cpum_lv2 = ppb_read_sram(HPT_CPU_M_SF_L2);
	gpu_lv1 = ppb_read_sram(HPT_GPU_SF_L1);
	gpu_lv2 = ppb_read_sram(HPT_GPU_SF_L2);
	delay = ppb_read_sram(HPT_DELAY_TIME);
	seq_printf(m, "%u, %u, %u, %u, %u, %u, %u, %u\n", enable, cpub_lv1, cpub_lv2,
		cpum_lv1, cpum_lv2, gpu_lv1, gpu_lv2, delay);
	return 0;
}

static ssize_t mt_hpt_sf_setting_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64], cmd[21];
	unsigned int len = 0, val = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%20s %u", cmd, &val) != 2) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (!strncmp(cmd, "ENABLE", 8)) {
		if (val > 1) {
			pr_notice("invalid input %s %d\n", cmd, val);
			return -EINVAL;
		}
		ppb_write_sram(val, HPT_SF_ENABLE);
	} else if (!strncmp(cmd, "CPUB_LV1", 8)) {
		if (val < 1 || val > 8) {
			pr_notice("invalid input %s %d\n", cmd, val);
			return -EINVAL;
		}
		ppb_write_sram(val, HPT_CPU_B_SF_L1);
	} else if (!strncmp(cmd, "CPUB_LV2", 8)) {
		if (val < 1 || val > 8) {
			pr_notice("invalid input %s %d\n", cmd, val);
			return -EINVAL;
		}
		ppb_write_sram(val, HPT_CPU_B_SF_L2);
	} else if (!strncmp(cmd, "CPUM_LV1", 8)) {
		if (val < 1 || val > 8) {
			pr_notice("invalid input %s %d\n", cmd, val);
			return -EINVAL;
		}
		ppb_write_sram(val, HPT_CPU_M_SF_L1);
	} else if (!strncmp(cmd, "CPUM_LV2", 8)) {
		if (val < 1 || val > 8) {
			pr_notice("invalid input %s %d\n", cmd, val);
			return -EINVAL;
		}
		ppb_write_sram(val, HPT_CPU_M_SF_L2);
	} else if (!strncmp(cmd, "GPU_LV1", 7)) {
		if (val < 1 || val > 8) {
			pr_notice("invalid input %s %d\n", cmd, val);
			return -EINVAL;
		}
		ppb_write_sram(val, HPT_GPU_SF_L1);
	} else if (!strncmp(cmd, "GPU_LV2", 7)) {
		if (val < 1 || val > 8) {
			pr_notice("invalid input %s %d\n", cmd, val);
			return -EINVAL;
		}
		ppb_write_sram(val, HPT_GPU_SF_L2);
	} else if (!strncmp(cmd, "delay", 8)) {
		if (val > 1000) {
			pr_notice("invalid input %s %d\n", cmd, val);
			return -EINVAL;
		}
		ppb_write_sram(val, HPT_DELAY_TIME);
	} else {
		pr_notice("invalid input %s %d\n", cmd, val);
		return -EINVAL;
	}

	return count;
}

static int mt_hpt_freq_setting_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "BCPU: %u\n", bcpu_hpt_freq);
	seq_printf(m, "MCPU: %u\n", mcpu_hpt_freq);
	seq_printf(m, "LCPU: %u\n", lcpu_hpt_freq);
	seq_printf(m, "DCPU: %u\n", dcpu_hpt_freq);
	seq_printf(m, "GPU: %u\n", gpu_hpt_freq);
	seq_printf(m, "APU: %u\n", apu_hpt_freq);
	return 0;
}

static ssize_t mt_hpt_freq_setting_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64];
	char xpu_type[5];
	unsigned int freq, len = 0;
	struct plt_ipi_data_s hwpt_data;
	int ret = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%4s %u", xpu_type, &freq) != 2) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	hwpt_data.cmd = PLT_HWPT_IPI_DATA;

	if (strcmp(xpu_type, "BCPU") == 0) {
		bcpu_hpt_freq = freq;
		freq /= 1000;
		hwpt_data.u.hwpt.BMCPU = (freq << 16);
		ret = mtk_ipi_send_compl(GET_MCUPM_IPIDEV(0), CHAN_PLATFORM, IPI_SEND_POLLING,
			&hwpt_data, sizeof(struct plt_ipi_data_s) / 4, 2000);
	} else if (strcmp(xpu_type, "MCPU") == 0) {
		mcpu_hpt_freq = freq;
		freq /= 1000;
		hwpt_data.u.hwpt.BMCPU = freq;
		ret = mtk_ipi_send_compl(GET_MCUPM_IPIDEV(0), CHAN_PLATFORM, IPI_SEND_POLLING,
			&hwpt_data, sizeof(struct plt_ipi_data_s) / 4, 2000);
	} else if (strcmp(xpu_type, "LCPU") == 0) {
		lcpu_hpt_freq = freq;
		freq /= 1000;
		hwpt_data.u.hwpt.LDCPU = (freq << 16);
		ret = mtk_ipi_send_compl(GET_MCUPM_IPIDEV(0), CHAN_PLATFORM, IPI_SEND_POLLING,
			&hwpt_data, sizeof(struct plt_ipi_data_s) / 4, 2000);
	} else if (strcmp(xpu_type, "DCPU") == 0) {
		dcpu_hpt_freq = freq;
		freq /= 1000;
		hwpt_data.u.hwpt.LDCPU = freq;
		ret = mtk_ipi_send_compl(GET_MCUPM_IPIDEV(0), CHAN_PLATFORM, IPI_SEND_POLLING,
			&hwpt_data, sizeof(struct plt_ipi_data_s) / 4, 2000);
	} else if (strcmp(xpu_type, "GPU") == 0) {
		gpu_hpt_freq = freq;
		gpufreq_set_mfgsys_config(CONFIG_PREUVLO, freq);
	} else if (strcmp(xpu_type, "APU") == 0) {
		apu_hpt_freq = freq;
		writel(apu_hpt_freq, (void __iomem *)(ppb_sram_base + APU_LIMIT_FREQ_OFFSET));
	} else {
		pr_notice("Invalid XPU type\n");
		return -EPERM;
	}
	if (ret)
		pr_info("Failed to send IPI message: %d\n", ret);
	return count;
}

static int mt_xpu_dbg_dump_proc_show(struct seq_file *m, void *v)
{
	struct xpu_dbg_t dbg_data;

	get_xpu_debug_info(&dbg_data);

	seq_printf(m, "%u, %u, %u, %u, %u, %u, %u, %u, %u\n", dbg_data.cpub_len, dbg_data.cpub_cnt,
		dbg_data.cpub_th_t, dbg_data.cpum_len, dbg_data.cpum_cnt, dbg_data.cpum_th_t,
		dbg_data.gpu_len, dbg_data.gpu_cnt, dbg_data.gpu_th_t);

	return 0;
}

static int mt_combo0_uisoc_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", pb.combo0_uisoc);

	return 0;
}

static ssize_t mt_combo0_uisoc_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64], cmd[21];
	unsigned int len = 0, val = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%20s %d", cmd, &val) != 2) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (strncmp(cmd, "uisoc", 5))
		return -EINVAL;

	pb.combo0_uisoc = val;
	bat_handler(NULL);

	return count;
}

static int mt_hpt_debug_info_proc_show(struct seq_file *m, void *v)
{
	struct ppb3_dbg_t ppb3_dbg_data;

	memset(&ppb3_dbg_data, 0, sizeof(ppb3_dbg_data));
	get_ppb3_debug_info(&ppb3_dbg_data);

	seq_printf(m, "%d,%d\n",
		ppb3_dbg_data.oc_count, ppb3_dbg_data.oc_duration_us);

	return 0;
}

#define PROC_FOPS_RW(name)						\
static int mt_ ## name ## _proc_open(struct inode *inode, struct file *file)\
{									\
	return single_open(file, mt_ ## name ## _proc_show, pde_data(inode));\
}									\
static const struct proc_ops mt_ ## name ## _proc_fops = {	\
	.proc_open		= mt_ ## name ## _proc_open,			\
	.proc_read		= seq_read,					\
	.proc_lseek		= seq_lseek,					\
	.proc_release		= single_release,				\
	.proc_write		= mt_ ## name ## _proc_write,			\
}

#define PROC_FOPS_RO(name)						\
static int mt_ ## name ## _proc_open(struct inode *inode, struct file *file)\
{									\
	return single_open(file, mt_ ## name ## _proc_show, pde_data(inode));\
}									\
static const struct proc_ops mt_ ## name ## _proc_fops = {	\
	.proc_open		= mt_ ## name ## _proc_open,		\
	.proc_read		= seq_read,				\
	.proc_lseek		= seq_lseek,				\
	.proc_release	= single_release,			\
}

#define PROC_ENTRY(name)	{__stringify(name), &mt_ ## name ## _proc_fops}
PROC_FOPS_RO(ppb_debug);
PROC_FOPS_RO(ppb_dump);
PROC_FOPS_RW(ppb_debug_log);
PROC_FOPS_RW(ppb_manual_mode);
PROC_FOPS_RW(ppb_stop);
PROC_FOPS_RW(peak_power_mode);
PROC_FOPS_RW(ppb_cg_min_power);
PROC_FOPS_RW(ppb_camera_power);
PROC_FOPS_RW(ppb_cg_budget_thd);
PROC_FOPS_RW(ppb_cg_budget_cnt);
PROC_FOPS_RO(hpt_debug);
PROC_FOPS_RO(hpt_dump);
PROC_FOPS_RW(hpt_ctrl);
PROC_FOPS_RW(hpt_sf_setting);
PROC_FOPS_RO(xpu_dbg_dump);
PROC_FOPS_RW(combo0_uisoc);
PROC_FOPS_RO(hpt_debug_info);
PROC_FOPS_RW(hpt_freq_setting);

static int mt_ppb_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(ppb_debug),
		PROC_ENTRY(ppb_dump),
		PROC_ENTRY(ppb_debug_log),
		PROC_ENTRY(ppb_manual_mode),
		PROC_ENTRY(ppb_stop),
		PROC_ENTRY(peak_power_mode),
		PROC_ENTRY(ppb_cg_min_power),
		PROC_ENTRY(ppb_camera_power),
		PROC_ENTRY(ppb_cg_budget_thd),
		PROC_ENTRY(ppb_cg_budget_cnt),
		PROC_ENTRY(hpt_debug),
		PROC_ENTRY(hpt_dump),
		PROC_ENTRY(hpt_ctrl),
		PROC_ENTRY(hpt_sf_setting),
		PROC_ENTRY(xpu_dbg_dump),
		PROC_ENTRY(combo0_uisoc),
		PROC_ENTRY(hpt_debug_info),
		PROC_ENTRY(hpt_freq_setting),
	};

	dir = proc_mkdir("ppb", NULL);

	if (!dir) {
		pr_notice("fail to create /proc/ppb @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0660, dir, entries[i].fops))
			pr_notice("@%s: create /proc/ppb/%s failed\n", __func__,
				    entries[i].name);
	}

	return 0;
}

static void __used get_md_dbm_info(void)
{
	int ret;
	u64 of_find;
	struct device_node *mddriver = NULL;

	mddriver = of_find_compatible_node(NULL, NULL, "mediatek,mddriver");
	if (!mddriver) {
		pr_info("mddriver not found in DTS\n");
		return;
	}

	ret =  of_property_read_u64(mddriver, "md_dbm_addr", &of_find);

	if (ret) {
		pr_info("address not found in DTS");
		return;
	}
	pr_info("%s md_dbm_addr: 0x%llx\n", __func__, of_find);
	ppb_write_sram((unsigned int)of_find, PPB_MD_SMEM_ADDR);

}

static void hpt_bp_cb(enum BATTERY_PERCENT_LEVEL_TAG level)
{
	int hpt_reg, hpt_enable = 0;

	if (level != pb.hpt_cur_lv && level < BATTERY_PERCENT_LEVEL_NUM) {
		hpt_reg = pb.hpt_lv_t[level];
		if (hpt_reg)
			hpt_enable = 1;

		if (!hpt_enable && pb.hpt_exclude_lbat_cg_thl)
			lbat_set_hpt_mode(hpt_enable);

		hpt_ctrl_write(hpt_reg, HPT_CTRL_SET);
		hpt_reg = ~hpt_reg & 0x7;
		hpt_ctrl_write(hpt_reg, HPT_CTRL_CLR);

		if (hpt_enable && pb.hpt_exclude_lbat_cg_thl)
			lbat_set_hpt_mode(hpt_enable);

		pb.hpt_cur_lv = level;
		pr_info("%s: hpt_reg=%d pb.hpt_cur_lv=%d\n", __func__, pb.hpt_lv_t[level], pb.hpt_cur_lv);
	}
}

static int peak_power_budget_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;
	void __iomem *addr;
	struct device_node *np;
	struct tag_bootmode *tag;
	int gpio_id = -1;

	pb.dev = &pdev->dev;
	np = of_find_compatible_node(NULL, NULL, "mediatek,peak_power_budget");
	if (!np) {
		dev_notice(&pdev->dev, "get peak_power_budget node fail\n");
		return -ENODATA;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ppb_sram");
	addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(addr))
		return PTR_ERR(addr);

	ppb_sram_base = addr;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ppb3_dbg");
	addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(addr))
		pr_info("%s:%d ppb3_dbg get addr error 0x%p\n", __func__, __LINE__, addr);
	else
		ppb3_dbg_base = addr;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "hpt_ctrl");
	addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(addr))
		pr_info("%s:%d hpt_ctrl get addr error 0x%p\n", __func__, __LINE__, addr);
	else
		hpt_ctrl_base = addr;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "hpt_status");
	addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(addr))
		pr_info("%s:%d hpt_ctrl get addr error 0x%p\n", __func__, __LINE__, addr);
	else
		hpt_status_base = addr;

	spin_lock_init(&ppb_lock);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gpu_dbg");
	addr = devm_ioremap_resource(&pdev->dev, res);

	if (IS_ERR(addr))
		pr_info("%s:%d gpu_dbg get addr error 0x%p\n", __func__, __LINE__, addr);
	else
		gpu_dbg_base = addr;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cpu_dbg");
	addr = devm_ioremap_resource(&pdev->dev, res);

	if (IS_ERR(addr))
		pr_info("%s:%d cpu_dbg get addr error 0x%p\n", __func__, __LINE__, addr);
	else
		cpu_dbg_base = addr;


	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "spbm_sram");
	addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(addr))
		pr_info("%s:%d spbm_sram get addr error 0x%p\n", __func__, __LINE__, addr);
	else
		spbm_csram_base = addr;

	/* CPU TCM */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "spbm_cputcm");
	addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(addr))
		pr_info("%s:%d spbm_cputcm get addr error 0x%p\n", __func__, __LINE__, addr);
	else
		spbm_cputcm_base = addr;


	/*check chip version from gpio*/
	gpio_id = of_get_named_gpio(np, "evb-phone-gpio", 0);
	if (gpio_id < 0) {
		pr_info("%s:%d evb-phone-gpio not supported, gpio_id=%d\n", __func__, __LINE__, gpio_id);
	} else {
		if (gpio_get_value(gpio_id) == 0) //gpio low
			pb.is_evb = 0; //gpio_id=0 => phone
		else if(gpio_get_value(gpio_id) == 1)  //gpio high
			pb.is_evb = 1; //gpio_id=1 => evb
	}

	pr_info("%s:evb-phone-gpio gpio_id=%d, val=%d, is_evb=%d\n", __func__,
		gpio_id, gpio_get_value(gpio_id), pb.is_evb);
	/*check chip version from gpio*/

	ret = sysfs_create_group(kernel_kobj, &spbm_attr_group);
	if (ret) {
		pr_info("%s:%d failed to create spbm sysfs, ret=%p\n", __func__, __LINE__, addr);
		return ret;
	}


	pb.combo0_uisoc = DEFAULT_COMBO0_UISOC;
	mt_ppb_create_procfs();

	INIT_WORK(&pb.bat_work, bat_handler);
	read_power_budget_dts(pdev); //get PPB version

	if ((pb.version == 2) && hpt_ctrl_read(HPT_CTRL) && pb.hpt_exclude_lbat_cg_thl)
		lbat_set_hpt_mode(1);

	ppb_nb.notifier_call = ppb_psy_event;
	ret = power_supply_reg_notifier(&ppb_nb);
	if (ret) {
		dev_notice(&pdev->dev, "power_supply_reg_notifier fail\n");
		return ret;
	}

	np = of_parse_phandle(pdev->dev.of_node, "bootmode", 0);
	if (!np)
		dev_notice(&pdev->dev, "get bootmode fail\n");
	else {
		tag = (struct tag_bootmode *)of_get_property(np, "atag,boot", NULL);
		if (!tag)
			dev_notice(&pdev->dev, "failed to get atag,boot\n");
		else {
			dev_notice(&pdev->dev, "bootmode:0x%x\n", tag->bootmode);
			ppb_write_sram((int)tag->bootmode, PPB_BOOT_MODE);
		}
	}
	get_md_dbm_info();
	ppb_set_wifi_pwr_addr_by_dts();

	if (pb.hpt_max_lv) {
		pb.hpt_cur_lv = -1;
		register_bp_thl_notify(&hpt_bp_cb, BATTERY_PERCENT_PRIO_HPT);
	}

	if (pb.version == 3) {
		timer_setup(&ppb_dbg_timer, ppb3_print_dbg_log, TIMER_DEFERRABLE);
		mod_timer(&ppb_dbg_timer, jiffies + PPB3_LOG_DURATION);
		spbm_scmi_init();
	} else {
		timer_setup(&ppb_dbg_timer, ppb_print_dbg_log, TIMER_DEFERRABLE);
		mod_timer(&ppb_dbg_timer, jiffies + PPB_LOG_DURATION);
	}

	ppb_ctrl.ppb_drv_done = 1;
	return 0;
}

static void peak_power_budget_remove(struct platform_device *pdev)
{
	sysfs_remove_group(kernel_kobj, &spbm_attr_group);
}

static const struct of_device_id peak_power_budget_of_match[] = {
	{ .compatible = "mediatek,peak_power_budget", },
	{},
};
MODULE_DEVICE_TABLE(of, peak_power_budget_of_match);

static struct platform_driver peak_power_budget_driver = {
	.probe = peak_power_budget_probe,
	.remove = peak_power_budget_remove,
	.driver = {
		.name = "mtk_peak_power_budget",
		.of_match_table = peak_power_budget_of_match,
	},
};
module_platform_driver(peak_power_budget_driver);

MODULE_AUTHOR("Samuel Hsieh");
MODULE_DESCRIPTION("MTK peak power budget");
MODULE_LICENSE("GPL");
