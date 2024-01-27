// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

/* system includes */
#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/interconnect.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/math64.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/sched/rt.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <linux/sysfs.h>
#include <linux/time.h>
#include <linux/topology.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include "mtk_disp_notify.h"

#if IS_ENABLED(CONFIG_MTK_DVFSRC)
#include <dvfsrc-exp.h>
#include <linux/soc/mediatek/mtk_dvfsrc.h>
#endif /* CONFIG_MTK_DVFSRC */

#include <linux/kallsyms.h>
#include <linux/tracepoint.h>
#include <trace/events/power.h>

#include "mtk_cm_mgr_common.h"
#if IS_ENABLED(CONFIG_MTK_CM_IPI)
#include "mtk_cm_ipi.h"
#endif /* CONFIG_MTK_CM_IPI */

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V2) && !IS_ENABLED(CONFIG_MTK_CM_IPI)
#include <sspm_define.h>
#include <sspm_ipi_id.h>
#endif /* CONFIG_MTK_TINYSYS_SSPM_V2 && !IS_ENABLED(CONFIG_MTK_CM_IPI) */

/*****************************************************************************
 *  Variables
 *****************************************************************************/
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V2) && !IS_ENABLED(CONFIG_MTK_CM_IPI)
int cm_ipi_ackdata;
int cm_mgr_sspm_enable = 1;
static int cm_sspm_ready;
#endif /* CONFIG_MTK_TINYSYS_SSPM_V2 && !IS_ENABLED(CONFIG_MTK_CM_IPI) */

static int cm_mgr_aggr;
static int cm_mgr_blank_status;
static int *cm_mgr_buf;
static int cm_mgr_cpu_to_dram_opp;
static int *cm_mgr_cpu_opp_to_dram;
static int cm_mgr_cpu_opp_size;
static int cm_mgr_emi_demand_check = 1;
static int cm_mgr_disable_fb = 1;
static int cm_mgr_dram_level;
static int cm_mgr_dram_opp_base = -1;
static int cm_mgr_dram_opp_ceiling = -1;
static int cm_mgr_dram_opp_floor = -1;
static int cm_mgr_loading_enable;
static int cm_mgr_loading_level = 1000;
static int cm_mgr_loop_count;
static int cm_mgr_num_array;
static int cm_mgr_num_perf;
static int cm_mgr_perf_enable = 1;
static int cm_mgr_perf_force_enable;
static int cm_passive;

static int debounce_times_perf_down = 50;
static int debounce_times_perf_force_down = 100;
static int debounce_times_perf_down_local = -1;
static int debounce_times_perf_down_force_local = -1;
static int debounce_times_reset_adb;

static int dsu_enable = 1;
static int dsu_mode;
static int dsu_opp_send = 0xff;
static int dsu_perf;

static int light_load_cps = 1000;

static unsigned int cm_hint;
static unsigned int cm_mgr_sspm_version;
static unsigned int cm_perf_mode_enable;
static unsigned int cm_perf_mode_ceiling_opp;
static unsigned int cm_perf_mode_thd;
static unsigned int *cpu_power_ratio_down;
static unsigned int *cpu_power_ratio_up;
static unsigned int *vcore_power_ratio_down;
static unsigned int *vcore_power_ratio_up;
static unsigned int *debounce_times_down_adb;
static unsigned int *debounce_times_up_adb;

static struct cm_mgr_hook hk;
static struct delayed_work cm_mgr_work;
static struct icc_path *cm_mgr_bw_path;
static struct kobject *cm_mgr_kobj;
static struct platform_device *cm_mgr_pdev;

void __iomem *cm_mgr_base;

/*****************************************************************************
 *  DTS variables
 *****************************************************************************/
static int cm_mgr_arch;
static int cm_mgr_cpu_map_dram_enable;
static int cm_mgr_cpu_map_emi_opp = 1;
static int cm_mgr_cpu_map_skip_cpu_opp = 2;
static int cm_mgr_enable = 1; // After chip mt6897, read cm_mgr_enable from dts.
static int cm_mgr_use_bcpu_weight;
static int cm_mgr_use_cpu_to_dram_map = 1;
static int cm_mgr_use_cpu_to_dram_map_new;

static int cpu_power_bcpu_weight_max = 100;
static int cpu_power_bcpu_weight_min = 100;
static int cpu_power_bbcpu_weight_max = 100;
static int cpu_power_bbcpu_weight_min = 100;

static unsigned int cm_work_flag;

/*****************************************************************************
 *  Common api
 *****************************************************************************/
int cm_mgr_get_enable(void)
{
	return cm_mgr_enable;
}
EXPORT_SYMBOL_GPL(cm_mgr_get_enable);

int get_cm_step_num(void)
{
	return cm_hint;
}
EXPORT_SYMBOL_GPL(get_cm_step_num);

struct icc_path *cm_mgr_get_bw_path(void)
{
	return cm_mgr_bw_path;
}
EXPORT_SYMBOL_GPL(cm_mgr_get_bw_path);

struct icc_path *cm_mgr_set_bw_path(struct icc_path *bw_path)
{
	return cm_mgr_bw_path = bw_path;
}
EXPORT_SYMBOL_GPL(cm_mgr_set_bw_path);

int cm_mgr_get_blank_status(void)
{
	return cm_mgr_blank_status;
}
EXPORT_SYMBOL_GPL(cm_mgr_get_blank_status);

int cm_mgr_get_disable_fb(void)
{
	return cm_mgr_disable_fb;
}
EXPORT_SYMBOL_GPL(cm_mgr_get_disable_fb);

void cm_mgr_set_disable_fb(int disable_fb)
{
	cm_mgr_disable_fb = disable_fb;
	cm_mgr_to_sspm_command(IPI_CM_MGR_DISABLE_FB, cm_mgr_disable_fb);
}
EXPORT_SYMBOL_GPL(cm_mgr_set_disable_fb);

int cm_mgr_get_perf_enable(void)
{
	return cm_mgr_perf_enable;
}
EXPORT_SYMBOL_GPL(cm_mgr_get_perf_enable);

int cm_mgr_get_perf_force_enable(void)
{
	return cm_mgr_perf_force_enable;
}
EXPORT_SYMBOL_GPL(cm_mgr_get_perf_force_enable);

void cm_mgr_set_perf_force_enable(int enable)
{
	cm_mgr_perf_force_enable = enable;
}
EXPORT_SYMBOL_GPL(cm_mgr_set_perf_force_enable);

int debounce_times_perf_down_get(void)
{
	return debounce_times_perf_down;
}
EXPORT_SYMBOL_GPL(debounce_times_perf_down_get);

int debounce_times_perf_force_down_get(void)
{
	return debounce_times_perf_force_down;
}
EXPORT_SYMBOL_GPL(debounce_times_perf_force_down_get);

int debounce_times_perf_down_local_get(void)
{
	return debounce_times_perf_down_local;
}
EXPORT_SYMBOL_GPL(debounce_times_perf_down_local_get);

void debounce_times_perf_down_local_set(int num)
{
	debounce_times_perf_down_local = num;
}
EXPORT_SYMBOL_GPL(debounce_times_perf_down_local_set);

int debounce_times_perf_down_force_local_get(void)
{
	return debounce_times_perf_down_force_local;
}
EXPORT_SYMBOL_GPL(debounce_times_perf_down_force_local_get);

void debounce_times_perf_down_force_local_set(int num)
{
	debounce_times_perf_down_force_local = num;
}
EXPORT_SYMBOL_GPL(debounce_times_perf_down_force_local_set);

int cm_mgr_get_dram_opp_base(void)
{
	return cm_mgr_dram_opp_base;
}
EXPORT_SYMBOL_GPL(cm_mgr_get_dram_opp_base);

void cm_mgr_set_dram_opp_base(int num)
{
	cm_mgr_dram_opp_base = num;
}
EXPORT_SYMBOL_GPL(cm_mgr_set_dram_opp_base);

void cm_mgr_get_sspm_version(void)
{
	cm_mgr_sspm_version = cm_mgr_to_sspm_command(
		IPI_CM_MGR_SSPM_VER | IPI_CM_MGR_SCMI_GET, 0);
}
EXPORT_SYMBOL_GPL(cm_mgr_get_sspm_version);

int cm_mgr_get_num_perf(void)
{
	return cm_mgr_num_perf;
}
EXPORT_SYMBOL_GPL(cm_mgr_get_num_perf);

void cm_mgr_set_num_perf(int num)
{
	cm_mgr_num_perf = num;
}
EXPORT_SYMBOL_GPL(cm_mgr_set_num_perf);

int cm_mgr_get_num_array(void)
{
	return cm_mgr_num_array;
}
EXPORT_SYMBOL_GPL(cm_mgr_get_num_array);

void cm_mgr_set_num_array(int num)
{
	cm_mgr_num_array = num;
}
EXPORT_SYMBOL_GPL(cm_mgr_set_num_array);

void cm_mgr_set_pdev(struct platform_device *pdev)
{
	cm_mgr_pdev = pdev;
}
EXPORT_SYMBOL_GPL(cm_mgr_set_pdev);

void cm_mgr_perf_set_status(int enable)
{
	if (hk.cm_mgr_perf_set_status)
		hk.cm_mgr_perf_set_status(enable);
}
EXPORT_SYMBOL_GPL(cm_mgr_perf_set_status);

void cm_mgr_set_perf_mode_enable(int enable)
{
	cm_perf_mode_enable = enable;
}
EXPORT_SYMBOL_GPL(cm_mgr_set_perf_mode_enable);

int cm_mgr_get_perf_mode_enable(void)
{
	return cm_perf_mode_enable;
}
EXPORT_SYMBOL_GPL(cm_mgr_get_perf_mode_enable);

void cm_mgr_set_perf_mode_ceiling_opp(int opp)
{
	cm_perf_mode_ceiling_opp = opp;
}
EXPORT_SYMBOL_GPL(cm_mgr_set_perf_mode_ceiling_opp);

int cm_mgr_get_perf_mode_ceiling_opp(void)
{
	return cm_perf_mode_ceiling_opp;
}
EXPORT_SYMBOL_GPL(cm_mgr_get_perf_mode_ceiling_opp);

void cm_mgr_set_perf_mode_thd(int thd)
{
	cm_perf_mode_thd = thd;
}
EXPORT_SYMBOL_GPL(cm_mgr_set_perf_mode_thd);

int cm_mgr_get_perf_mode_thd(void)
{
	return cm_perf_mode_thd;
}
EXPORT_SYMBOL_GPL(cm_mgr_get_perf_mode_thd);

void cm_mgr_register_hook(struct cm_mgr_hook *hook)
{
	hk.cm_mgr_get_perfs = hook->cm_mgr_get_perfs;
	hk.cm_mgr_perf_set_force_status = hook->cm_mgr_perf_set_force_status;
	hk.check_cm_mgr_status = hook->check_cm_mgr_status;
	hk.cm_mgr_perf_platform_set_status =
		hook->cm_mgr_perf_platform_set_status;
	hk.cm_mgr_perf_set_status = hook->cm_mgr_perf_set_status;
	hk.cm_mgr_get_latency_awareness_model_indexes =
		hook->cm_mgr_get_latency_awareness_model_indexes;
}
EXPORT_SYMBOL_GPL(cm_mgr_register_hook);

void cm_mgr_unregister_hook(struct cm_mgr_hook *hook)
{
	hk.cm_mgr_get_perfs = NULL;
	hk.cm_mgr_perf_set_force_status = NULL;
	hk.check_cm_mgr_status = NULL;
	hk.cm_mgr_perf_platform_set_status = NULL;
	hk.cm_mgr_perf_set_status = NULL;
	hk.cm_mgr_get_latency_awareness_model_indexes = NULL;
}
EXPORT_SYMBOL_GPL(cm_mgr_unregister_hook);

static int cm_mgr_fb_notifier_callback(struct notifier_block *nb,
				       unsigned long value, void *v)
{
	int *data = (int *)v;

	if (value == MTK_DISP_EVENT_BLANK) {
		CM_DBG_PRINT("%s+\n", __func__);
		if (*data == MTK_DISP_BLANK_UNBLANK) {
			CM_DBG_PRINT("%s(%d): screen on\n", __func__, __LINE__);
			cm_mgr_blank_status = 0;
			cm_mgr_to_sspm_command(IPI_CM_MGR_BLANK, 0);
		} else if (*data == MTK_DISP_BLANK_POWERDOWN) {
			CM_DBG_PRINT("%s(%d): screen off\n", __func__, __LINE__);
			cm_mgr_blank_status = 1;
			cm_mgr_dram_opp_base = -1;
			if (hk.cm_mgr_perf_platform_set_status)
				hk.cm_mgr_perf_platform_set_status(0);
			cm_mgr_to_sspm_command(IPI_CM_MGR_BLANK, 1);
		}
		CM_DBG_PRINT("%s-\n", __func__);
	}

	return 0;
}

static struct notifier_block cm_mgr_fb_notifier = {
	.notifier_call = cm_mgr_fb_notifier_callback,
};

#if !IS_ENABLED(CONFIG_MTK_CM_IPI)
int cm_mgr_to_sspm_command(u32 cmd, int val)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V2)
	unsigned int ret = 0;
	struct cm_mgr_data cm_mgr_d;

	if (cm_sspm_ready != 1) {
		CM_DBG_PRINT("%s(%d): sspm not ready(%d) to receive cmd(%d)\n"
			, __func__, __LINE__, cm_sspm_ready, cmd);
		ret = -1;
		return ret;
	}
	cm_ipi_ackdata = 0;
	CM_DBG_PRINT("%s(%d): cm to sspm cmd: %d arg: %d\n"
		, __func__, __LINE__, cmd, val);
	switch (cmd) {
	case IPI_CM_MGR_INIT:
	case IPI_CM_MGR_ENABLE:
	case IPI_CM_MGR_OPP_ENABLE:
	case IPI_CM_MGR_SSPM_ENABLE:
	case IPI_CM_MGR_BLANK:
	case IPI_CM_MGR_DISABLE_FB:
	case IPI_CM_MGR_DRAM_TYPE:
	case IPI_CM_MGR_CPU_POWER_RATIO_UP:
	case IPI_CM_MGR_CPU_POWER_RATIO_DOWN:
	case IPI_CM_MGR_VCORE_POWER_RATIO_UP:
	case IPI_CM_MGR_VCORE_POWER_RATIO_DOWN:
	case IPI_CM_MGR_DEBOUNCE_UP:
	case IPI_CM_MGR_DEBOUNCE_DOWN:
	case IPI_CM_MGR_DEBOUNCE_TIMES_RESET_ADB:
	case IPI_CM_MGR_DRAM_LEVEL:
	case IPI_CM_MGR_LIGHT_LOAD_CPS:
	case IPI_CM_MGR_LOADING_ENABLE:
	case IPI_CM_MGR_LOADING_LEVEL:
	case IPI_CM_MGR_EMI_DEMAND_CHECK:
	case IPI_CM_MGR_OPP_FREQ_SET:
	case IPI_CM_MGR_OPP_VOLT_SET:
	case IPI_CM_MGR_BCPU_WEIGHT_MAX_SET:
	case IPI_CM_MGR_BCPU_WEIGHT_MIN_SET:
		cm_mgr_d.cmd = cmd;
		cm_mgr_d.arg = val;
		ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_CM,
					 IPI_SEND_POLLING, &cm_mgr_d,
					 CM_MGR_D_LEN, 2000);
		if (ret != 0) {
			CM_DBG_PRINT("%s(%d): cmd(%d) error, return %d\n", __func__,
				__LINE__, cmd, ret);
		} else if (!cm_ipi_ackdata) {
			ret = cm_ipi_ackdata;
			CM_DBG_PRINT("%s(%d): cmd(%d) ack fail %d\n", __func__,
				__LINE__, cmd, ret);
		}
		break;
	default:
		CM_DBG_PRINT("%s(%d): wrong cmd(%d)!!!\n", __func__, __LINE__, cmd);
		break;
	}

	return ret;
#else
	return -1;
#endif /* CONFIG_MTK_TINYSYS_SSPM_V2 */
}
EXPORT_SYMBOL_GPL(cm_mgr_to_sspm_command);
#endif /* CONFIG_MTK_CM_IPI */

int cm_mgr_get_latency_awareness_model_info(unsigned int *buf)
{
	if (!hk.cm_mgr_get_latency_awareness_model_indexes)
		return 0;

	return hk.cm_mgr_get_latency_awareness_model_indexes(buf);
}
EXPORT_SYMBOL_GPL(cm_mgr_get_latency_awareness_model_info);

int cm_mgr_judge_perfs_dram_opp(int dram_opp)
{
	int perf_num = cm_mgr_get_num_perf();

	if (cm_mgr_dram_opp_ceiling < 0 && cm_mgr_dram_opp_floor < 0)
		return dram_opp;

	if (cm_mgr_dram_opp_ceiling >= 0) {
		if (cm_mgr_dram_opp_floor >= 0 &&
		    cm_mgr_dram_opp_ceiling > cm_mgr_dram_opp_floor)
			return dram_opp;
		if (cm_mgr_dram_opp_ceiling <= perf_num &&
		    cm_mgr_dram_opp_ceiling > dram_opp)
			dram_opp = cm_mgr_dram_opp_ceiling;
	}

	return dram_opp;
}
EXPORT_SYMBOL_GPL(cm_mgr_judge_perfs_dram_opp);

void cm_mgr_set_dram_opp_ceiling(int opp)
{
	cm_mgr_dram_opp_ceiling = opp;
	cm_mgr_to_sspm_command(IPI_CM_MGR_DRAM_OPP_CEILING, opp);
}
EXPORT_SYMBOL_GPL(cm_mgr_set_dram_opp_ceiling);

void cm_mgr_set_dram_opp_floor(int opp)
{
	cm_mgr_dram_opp_floor = opp;
	cm_mgr_to_sspm_command(IPI_CM_MGR_DRAM_OPP_FLOOR, opp);
}
EXPORT_SYMBOL_GPL(cm_mgr_set_dram_opp_floor);

static void cm_mgr_cpu_map_update_table(void)
{
	int i;

	for (i = 0; i < cm_mgr_cpu_opp_size; i++) {
		if (i < cm_mgr_cpu_map_skip_cpu_opp)
			cm_mgr_cpu_opp_to_dram[i] = cm_mgr_cpu_map_emi_opp;
		CM_DBG_PRINT("CM CPU MAP TALBLE UPDATE [%d] %d\n", i, cm_mgr_cpu_opp_to_dram[i]);
	}
}

static ssize_t dbg_cm_mgr_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buff)
{
	int i;
	int len = 0;

#define cm_mgr_print(...)                                                      \
	snprintf(buff + len, (4096 - len) > 0 ? (4096 - len) : 0, __VA_ARGS__)

	len += cm_mgr_print("cm_mgr_enable %d\n", cm_mgr_enable);
	len += cm_mgr_print("cm_mgr_arch %d\n", cm_mgr_arch);
	len += cm_mgr_print("cm_mgr_sspm_version v%d.%d.%d\n",
			    (cm_mgr_sspm_version >> 16) & 0xFF,
			    (cm_mgr_sspm_version >> 8) & 0xFF,
			    (cm_mgr_sspm_version)&0xFF);

	len += cm_mgr_print("cm_mgr_perf_enable %d\n", cm_mgr_perf_enable);
	len += cm_mgr_print("cm_mgr_dram_opp_ceiling %d\n",
			    cm_mgr_dram_opp_ceiling);
	len += cm_mgr_print("cm_mgr_dram_opp_floor %d\n",
			    cm_mgr_dram_opp_floor);
	len += cm_mgr_print("cm_mgr_dram_level %d\n", cm_mgr_dram_level);

	if (cm_mgr_use_cpu_to_dram_map)
		len += cm_mgr_print("cm_mgr_cpu_map_dram_enable %d\n",
				    cm_mgr_cpu_map_dram_enable);
	if (cm_mgr_use_cpu_to_dram_map_new) {
		len += cm_mgr_print("cm_mgr_cpu_map_emi_opp %d\n",
				    cm_mgr_cpu_map_emi_opp);
		len += cm_mgr_print("cm_mgr_cpu_map_skip_cpu_opp %d\n",
				    cm_mgr_cpu_map_skip_cpu_opp);

		len += cm_mgr_print("cm_mgr_cpu_opp_to_dram table");
		for (i = 0; i < cm_mgr_cpu_opp_size; i++)
			len += cm_mgr_print(" %d", cm_mgr_cpu_opp_to_dram[i]);
		len += cm_mgr_print("\n");
	}
	len += cm_mgr_print("cm_dbg_info %d\n", cm_dbg_info);
	len += cm_mgr_print("cm_passive %d\n", cm_passive);
	len += cm_mgr_print("cm_perf_mode_enable %d\n",
		    cm_mgr_get_perf_mode_enable());
	len += cm_mgr_print("cm_perf_mode_ceiling_opp %d\n",
		    cm_mgr_get_perf_mode_ceiling_opp());
	len += cm_mgr_print("cm_perf_mode_thd %d\n",
		    cm_mgr_get_perf_mode_thd());
	len += cm_mgr_print("cpu_power_ratio_up");
	for (i = 0; i < cm_mgr_num_array; i++)
		len += cm_mgr_print(" %d", cpu_power_ratio_up[i]);
	len += cm_mgr_print("\n");

	len += cm_mgr_print("cpu_power_ratio_down");
	for (i = 0; i < cm_mgr_num_array; i++)
		len += cm_mgr_print(" %d", cpu_power_ratio_down[i]);
	len += cm_mgr_print("\n");

	len += cm_mgr_print("vcore_power_ratio_up");
	for (i = 0; i < cm_mgr_num_array; i++)
		len += cm_mgr_print(" %d", vcore_power_ratio_up[i]);
	len += cm_mgr_print("\n");

	len += cm_mgr_print("vcore_power_ratio_down");
	for (i = 0; i < cm_mgr_num_array; i++)
		len += cm_mgr_print(" %d", vcore_power_ratio_down[i]);
	len += cm_mgr_print("\n");

	len += cm_mgr_print("debounce_times_up_adb");
	for (i = 0; i < cm_mgr_num_array; i++)
		len += cm_mgr_print(" %d", debounce_times_up_adb[i]);
	len += cm_mgr_print("\n");

	len += cm_mgr_print("debounce_times_down_adb");
	for (i = 0; i < cm_mgr_num_array; i++)
		len += cm_mgr_print(" %d", debounce_times_down_adb[i]);
	len += cm_mgr_print("\n");

	len += cm_mgr_print("debounce_times_reset_adb %d\n",
			    debounce_times_reset_adb);
	len += cm_mgr_print("debounce_times_perf_down %d\n",
			    debounce_times_perf_down);

	if (cm_mgr_arch == CM_MGR_ARCH_V1) {
#if IS_ENABLED(CONFIG_MTK_CM_IPI)
		len += cm_mgr_print("cm_ipi_enable %d\n", cm_get_ipi_enable());
#else
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V2)
		len += cm_mgr_print("cm_mgr_sspm_enable %d\n",
				    cm_mgr_sspm_enable);
#endif /* CONFIG_MTK_TINYSYS_SSPM_V2 */
#endif /* CONFIG_MTK_CM_IPI */

		len += cm_mgr_print("cm_mgr_disable_fb %d\n",
				    cm_mgr_disable_fb);
		len += cm_mgr_print("light_load_cps %d\n", light_load_cps);
		len += cm_mgr_print("cm_mgr_loop_count %d\n",
				    cm_mgr_loop_count);
		len += cm_mgr_print("cm_mgr_loading_level %d\n",
				    cm_mgr_loading_level);
		len += cm_mgr_print("cm_mgr_loading_enable %d\n",
				    cm_mgr_loading_enable);
		len += cm_mgr_print("cm_mgr_emi_demand_check %d\n",
				    cm_mgr_emi_demand_check);

		if (cm_mgr_use_bcpu_weight) {
			len += cm_mgr_print("cpu_power_bcpu_weight_max %d\n",
					    cpu_power_bcpu_weight_max);
			len += cm_mgr_print("cpu_power_bcpu_weight_min %d\n",
					    cpu_power_bcpu_weight_min);
			len += cm_mgr_print("cpu_power_bbcpu_weight_max %d\n",
					    cpu_power_bbcpu_weight_max);
			len += cm_mgr_print("cpu_power_bbcpu_weight_min %d\n",
					    cpu_power_bbcpu_weight_min);
		}
#if IS_ENABLED(CONFIG_MTK_CM_IPI)
		len += cm_mgr_print("dsu_enable %d\n", dsu_enable);
		len += cm_mgr_print("dsu_opp_send %d\n", dsu_opp_send);
		len += cm_mgr_print("dsu_mode_change %d\n", dsu_mode);
		len += cm_mgr_print("dsu_perf %d\n", dsu_perf);
		len += cm_mgr_print("cm_mgr_aggr %d\n", cm_mgr_aggr);
		len += cm_mgr_print("cm_hint %d\n", cm_hint);
#endif
	}

	return (len > 4096) ? 4096 : len;
}

static ssize_t dbg_cm_mgr_store(struct kobject *kobj,
				struct kobj_attribute *attr, const char *buff,
				size_t count)
{
	char cmd[64];
	int ret;
	u32 val_1;
	u32 val_2;

	ret = sscanf(buff, "%63s %d %d", cmd, &val_1, &val_2);
	if (ret < 1) {
		ret = -EPERM;
		goto out;
	}

	if (!strcmp(cmd, "cm_mgr_enable")) {
		cm_mgr_enable = val_1;
		cm_mgr_to_sspm_command(IPI_CM_MGR_ENABLE, cm_mgr_enable);
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V2) && !IS_ENABLED(CONFIG_MTK_CM_IPI)
	} else if (!strcmp(cmd, "cm_mgr_sspm_enable")) {
		cm_mgr_sspm_enable = val_1;
		cm_mgr_to_sspm_command(IPI_CM_MGR_SSPM_ENABLE,
				       cm_mgr_sspm_enable);
#endif /* CONFIG_MTK_TINYSYS_SSPM_V2 && !IS_ENABLED(CONFIG_MTK_CM_IPI) */
	} else if (!strcmp(cmd, "cm_mgr_perf_enable")) {
		cm_mgr_perf_enable = val_1;
	} else if (!strcmp(cmd, "cm_mgr_perf_force_enable")) {
		cm_mgr_perf_force_enable = val_1;
		if (hk.cm_mgr_perf_set_force_status)
			hk.cm_mgr_perf_set_force_status(val_1);
	} else if (!strcmp(cmd, "cm_mgr_disable_fb")) {
		cm_mgr_disable_fb = val_1;
		cm_mgr_to_sspm_command(IPI_CM_MGR_DISABLE_FB,
				       cm_mgr_disable_fb);
	} else if (!strcmp(cmd, "light_load_cps")) {
		light_load_cps = val_1;
		cm_mgr_to_sspm_command(IPI_CM_MGR_LIGHT_LOAD_CPS, val_1);
	} else if (!strcmp(cmd, "cm_mgr_loop_count")) {
		cm_mgr_loop_count = val_1;
	} else if (!strcmp(cmd, "cm_mgr_dram_level")) {
		cm_mgr_dram_level = val_1;
		cm_mgr_to_sspm_command(IPI_CM_MGR_DRAM_LEVEL, val_1);
	} else if (!strcmp(cmd, "cm_mgr_loading_level")) {
		cm_mgr_loading_level = val_1;
		cm_mgr_to_sspm_command(IPI_CM_MGR_LOADING_LEVEL, val_1);
	} else if (!strcmp(cmd, "cm_mgr_loading_enable")) {
		cm_mgr_loading_enable = val_1;
		cm_mgr_to_sspm_command(IPI_CM_MGR_LOADING_ENABLE, val_1);
	} else if (!strcmp(cmd, "cm_mgr_emi_demand_check")) {
		cm_mgr_emi_demand_check = val_1;
		cm_mgr_to_sspm_command(IPI_CM_MGR_EMI_DEMAND_CHECK, val_1);
	} else if (!strcmp(cmd, "cpu_power_ratio_up")) {
		if (ret == 3 && val_1 < cm_mgr_num_array)
			cpu_power_ratio_up[val_1] = val_2;
		cm_mgr_to_sspm_command(IPI_CM_MGR_CPU_POWER_RATIO_UP,
				       val_1 << 16 | val_2);
	} else if (!strcmp(cmd, "cpu_power_ratio_down")) {
		if (ret == 3 && val_1 < cm_mgr_num_array)
			cpu_power_ratio_down[val_1] = val_2;
		cm_mgr_to_sspm_command(IPI_CM_MGR_CPU_POWER_RATIO_DOWN,
				       val_1 << 16 | val_2);
	} else if (!strcmp(cmd, "vcore_power_ratio_up")) {
		if (ret == 3 && val_1 < cm_mgr_num_array)
			vcore_power_ratio_up[val_1] = val_2;
		cm_mgr_to_sspm_command(IPI_CM_MGR_VCORE_POWER_RATIO_UP,
				       val_1 << 16 | val_2);
	} else if (!strcmp(cmd, "vcore_power_ratio_down")) {
		if (ret == 3 && val_1 < cm_mgr_num_array)
			vcore_power_ratio_down[val_1] = val_2;
		cm_mgr_to_sspm_command(IPI_CM_MGR_VCORE_POWER_RATIO_DOWN,
				       val_1 << 16 | val_2);
	} else if (!strcmp(cmd, "debounce_times_up_adb")) {
		if (ret == 3 && val_1 < cm_mgr_num_array)
			debounce_times_up_adb[val_1] = val_2;
		cm_mgr_to_sspm_command(IPI_CM_MGR_DEBOUNCE_UP,
				       val_1 << 16 | val_2);
	} else if (!strcmp(cmd, "debounce_times_down_adb")) {
		if (ret == 3 && val_1 < cm_mgr_num_array)
			debounce_times_down_adb[val_1] = val_2;
		cm_mgr_to_sspm_command(IPI_CM_MGR_DEBOUNCE_DOWN,
				       val_1 << 16 | val_2);
	} else if (!strcmp(cmd, "debounce_times_reset_adb")) {
		debounce_times_reset_adb = val_1;
		cm_mgr_to_sspm_command(IPI_CM_MGR_DEBOUNCE_TIMES_RESET_ADB,
				       debounce_times_reset_adb);
	} else if (!strcmp(cmd, "cpu_power_bcpu_weight_max")) {
		if (cpu_power_bcpu_weight_max < cpu_power_bcpu_weight_min) {
			ret = -1;
		} else {
			cpu_power_bcpu_weight_max = val_1;
			cm_mgr_to_sspm_command(IPI_CM_MGR_BCPU_WEIGHT_MAX_SET,
					       val_1);
		}
	} else if (!strcmp(cmd, "cpu_power_bcpu_weight_min")) {
		if (cpu_power_bcpu_weight_max < cpu_power_bcpu_weight_min) {
			ret = -1;
		} else {
			cpu_power_bcpu_weight_min = val_1;
			cm_mgr_to_sspm_command(IPI_CM_MGR_BCPU_WEIGHT_MIN_SET,
					       val_1);
		}
	} else if (!strcmp(cmd, "cpu_power_bbcpu_weight_max")) {
		if (cpu_power_bbcpu_weight_max < cpu_power_bbcpu_weight_min) {
			ret = -1;
		} else {
			cpu_power_bbcpu_weight_max = val_1;
			cm_mgr_to_sspm_command(IPI_CM_MGR_BBCPU_WEIGHT_MAX_SET,
					       val_1);
		}
	} else if (!strcmp(cmd, "cpu_power_bbcpu_weight_min")) {
		if (cpu_power_bbcpu_weight_max < cpu_power_bbcpu_weight_min) {
			ret = -1;
		} else {
			cpu_power_bbcpu_weight_min = val_1;
			cm_mgr_to_sspm_command(IPI_CM_MGR_BBCPU_WEIGHT_MIN_SET,
					       val_1);
		}
	} else if (!strcmp(cmd, "debounce_times_perf_down")) {
		debounce_times_perf_down = val_1;
	} else if (!strcmp(cmd, "debounce_times_perf_force_down")) {
		debounce_times_perf_force_down = val_1;
	} else if (!strcmp(cmd, "1")) {
		cm_mgr_perf_force_enable = 1;
		if (hk.cm_mgr_perf_set_force_status)
			hk.cm_mgr_perf_set_force_status(1);
	} else if (!strcmp(cmd, "0")) {
		cm_mgr_perf_force_enable = 0;
		if (hk.cm_mgr_perf_set_force_status)
			hk.cm_mgr_perf_set_force_status(0);
	} else if (!strcmp(cmd, "cm_mgr_cpu_map_dram_enable")) {
		cm_mgr_cpu_map_dram_enable = !!val_1;
		cm_mgr_disable_fb = !val_1;
		cm_mgr_to_sspm_command(IPI_CM_MGR_CPU_MAP_DRAM_ENABLE, val_1);
	} else if (!strcmp(cmd, "cm_mgr_cpu_map_skip_cpu_opp")) {
		cm_mgr_cpu_map_skip_cpu_opp = val_1;
		cm_mgr_cpu_map_update_table();
	} else if (!strcmp(cmd, "cm_mgr_cpu_map_emi_opp")) {
		cm_mgr_cpu_map_emi_opp = val_1;
		cm_mgr_cpu_map_update_table();
#if IS_ENABLED(CONFIG_MTK_CM_IPI)
	} else if (!strcmp(cmd, "dsu_enable")) {
		dsu_enable = val_1;
		cm_mgr_to_sspm_command(IPI_CM_MGR_DSU_ENABLE, val_1);
	} else if (!strcmp(cmd, "dsu_opp_send")) {
		dsu_opp_send = val_1;
		cm_mgr_to_sspm_command(IPI_CM_MGR_DSU_OPP_SEND, val_1);
	} else if (!strcmp(cmd, "dsu_mode")) {
		dsu_mode = val_1;
		cm_mgr_to_sspm_command(IPI_CM_MGR_DSU_MODE, val_1);
	} else if (!strcmp(cmd, "dsu_perf")) {
		dsu_perf = val_1;
		cm_mgr_to_sspm_command(IPI_CM_MGR_DSU_PERF_HINT, val_1);
	} else if (!strcmp(cmd, "cm_mgr_aggr")) {
		cm_mgr_aggr = val_1;
		cm_mgr_to_sspm_command(IPI_CM_MGR_AGGRESSIVE, val_1);
	} else if (!strcmp(cmd, "cm_passive")) {
		cm_passive = val_1;
		cm_mgr_to_sspm_command(IPI_CM_MGR_PASSIVE, val_1);
	} else if (!strcmp(cmd, "cm_hint")) {
		cm_hint = val_1;
	} else if (!strcmp(cmd, "cm_mgr_dram_opp_ceiling")) {
		cm_mgr_dram_opp_ceiling = val_1;
		cm_mgr_to_sspm_command(IPI_CM_MGR_DRAM_OPP_CEILING, val_1);
	} else if (!strcmp(cmd, "cm_mgr_dram_opp_floor")) {
		cm_mgr_dram_opp_floor = val_1;
		cm_mgr_to_sspm_command(IPI_CM_MGR_DRAM_OPP_FLOOR, val_1);
	} else if (!strcmp(cmd, "cm_perf_mode_enable")) {
		cm_mgr_set_perf_mode_enable(val_1);
		cm_mgr_to_sspm_command(IPI_CM_MGR_PERF_MODE_ENABLE, val_1);
	} else if (!strcmp(cmd, "cm_perf_mode_ceiling_opp")) {
		cm_mgr_set_perf_mode_ceiling_opp(val_1);
		cm_mgr_to_sspm_command(IPI_CM_MGR_PERF_MODE_CEILING_OPP, val_1);
	} else if (!strcmp(cmd, "cm_perf_mode_thd")) {
		cm_mgr_set_perf_mode_thd(val_1);
		cm_mgr_to_sspm_command(IPI_CM_MGR_PERF_MODE_THD, val_1);
#endif
	} else if (!strcmp(cmd, "cm_dbg_info")) {
		cm_dbg_info = val_1;
	} else if (!strcmp(cmd, "cm_test_fpsgo_perf_hint")) {
		if (hk.cm_mgr_perf_set_status)
			hk.cm_mgr_perf_set_status(val_1);
	}

out:
	if (ret < 0)
		return ret;
	return count;
}

int cm_mgr_check_dts_setting(struct platform_device *pdev)
{
	const char *buf;
	int ret;
	int opp_count;
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;

	ret = of_property_read_u32(node, "cm-mgr-enable", &cm_mgr_enable);
	if (ret)
		CM_DBG_PRINT("%s(%d): fail to get cm_mgr_enable from dts. ret %d\n",
			__func__, __LINE__, ret);
	else
		CM_DBG_PRINT("%s(%d): cm_mgr_enable %d\n", __func__, __LINE__,
			cm_mgr_enable);

	ret = of_property_read_string(node, "cm-mgr-arch", (const char **)&buf);
	if (!ret) {
		if (!strcmp(buf, "v1p"))
			cm_mgr_arch = CM_MGR_ARCH_V1P;
		else {
			CM_DBG_PRINT("%s(%d): fail to get correct cm_mgr_arch from dts.\n",
				__func__, __LINE__);
			ret = -1;
			goto ERROR;
		}
	} else
		cm_mgr_arch = CM_MGR_ARCH_V1;
	CM_DBG_PRINT("%s(%d): cm_mgr_arch %d\n", __func__, __LINE__, cm_mgr_arch);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cm_mgr_base");
	cm_mgr_base = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)cm_mgr_base)) {
		CM_DBG_PRINT("%s(%d): fail to ioremap registers.\n", __func__,
			__LINE__);
		ret = -1;
		goto ERROR;
	} else
		CM_DBG_PRINT("%s(%d): cm_mgr_base %p\n", __func__, __LINE__,
			cm_mgr_base);

	opp_count = of_count_phandle_with_args(node, "cm-mgr-cpu-opp-to-dram",
					       NULL);
	if (opp_count > 0) {
		CM_DBG_PRINT("%s(%d): opp_count %d\n", __func__, __LINE__,
			opp_count);
		cm_mgr_cpu_opp_size = opp_count;
	} else {
		CM_DBG_PRINT("%s(%d): fail to get opp_count from dts.\n", __func__,
			__LINE__);
		ret = -1;
		goto ERROR;
	}

	cm_mgr_cpu_opp_to_dram = devm_kzalloc(
		dev, sizeof(int) * cm_mgr_cpu_opp_size, GFP_KERNEL);
	if (!cm_mgr_cpu_opp_to_dram) {
		ret = -ENOMEM;
		goto ERROR;
	}

	ret = of_property_read_u32_array(node, "cm-mgr-cpu-opp-to-dram",
					 cm_mgr_cpu_opp_to_dram,
					 cm_mgr_cpu_opp_size);
	if (ret) {
		CM_DBG_PRINT("%s(%d): fail to get cm_mgr_cpu_opp_to_dram from dts. ret %d\n",
			__func__, __LINE__, ret);
		goto ERROR1;
	}

	ret = of_property_read_string(node, "use-cpu-to-dram-map",
				      (const char **)&buf);
	if (!ret) {
		if (!strcmp(buf, "enable"))
			cm_mgr_use_cpu_to_dram_map = 1;
		else
			cm_mgr_use_cpu_to_dram_map = 0;
	} else
		cm_mgr_use_cpu_to_dram_map = 0;
	CM_DBG_PRINT("%s(%d): cm_mgr_use_cpu_to_dram_map %d\n", __func__, __LINE__,
		cm_mgr_use_cpu_to_dram_map);

	ret = of_property_read_string(node, "use-cpu-to-dram-map-new",
				      (const char **)&buf);
	if (!ret) {
		if (!strcmp(buf, "enable"))
			cm_mgr_use_cpu_to_dram_map_new = 1;
		else
			cm_mgr_use_cpu_to_dram_map_new = 0;
	} else
		cm_mgr_use_cpu_to_dram_map_new = 0;
	CM_DBG_PRINT("%s(%d): cm_mgr_use_cpu_to_dram_map_new %d\n", __func__,
		__LINE__, cm_mgr_use_cpu_to_dram_map_new);

	if (cm_mgr_arch == CM_MGR_ARCH_V1) {
		ret = of_property_read_string(node, "use-bcpu-weight",
					      (const char **)&buf);
		if (!ret) {
			if (!strcmp(buf, "enable"))
				cm_mgr_use_bcpu_weight = 1;
			else
				cm_mgr_use_bcpu_weight = 0;
		} else
			cm_mgr_use_bcpu_weight = 0;
		CM_DBG_PRINT("%s(%d): cm_mgr_use_bcpu_weight %d\n", __func__,
			__LINE__, cm_mgr_use_bcpu_weight);

		ret = of_property_read_s32(node, "cpu-power-bcpu-weight-max",
					   &cpu_power_bcpu_weight_max);
		if (ret)
			cpu_power_bcpu_weight_max = 100;
		CM_DBG_PRINT("%s(%d): cpu_power_bcpu_weight_max %d\n", __func__,
			__LINE__, cpu_power_bcpu_weight_max);

		ret = of_property_read_s32(node, "cpu-power-bcpu-weight-min",
					   &cpu_power_bcpu_weight_min);
		if (ret)
			cpu_power_bcpu_weight_min = 100;
		CM_DBG_PRINT("%s(%d): cpu_power_bcpu_weight_min %d\n", __func__,
			__LINE__, cpu_power_bcpu_weight_min);

		ret = of_property_read_s32(node, "cpu-power-bbcpu-weight-max",
					   &cpu_power_bbcpu_weight_max);
		if (ret)
			cpu_power_bbcpu_weight_max = 100;

		CM_DBG_PRINT("%s(%d): cpu_power_bbcpu_weight_max %d\n", __func__,
			__LINE__, cpu_power_bbcpu_weight_max);

		ret = of_property_read_s32(node, "cpu-power-bbcpu-weight-min",
					   &cpu_power_bbcpu_weight_min);
		if (ret)
			cpu_power_bbcpu_weight_min = 100;
		CM_DBG_PRINT("%s(%d): cpu_power_bbcpu_weight_min %d\n", __func__,
			__LINE__, cpu_power_bbcpu_weight_min);
	}

	cm_mgr_buf = devm_kzalloc(dev, sizeof(int) * 6 * cm_mgr_num_array,
				  GFP_KERNEL);

	if (!cm_mgr_buf) {
		ret = -ENOMEM;
		goto ERROR1;
	}

	cpu_power_ratio_down = cm_mgr_buf;
	cpu_power_ratio_up = cpu_power_ratio_down + cm_mgr_num_array;
	debounce_times_down_adb = cpu_power_ratio_up + cm_mgr_num_array;
	debounce_times_up_adb = debounce_times_down_adb + cm_mgr_num_array;
	vcore_power_ratio_down = debounce_times_up_adb + cm_mgr_num_array;
	vcore_power_ratio_up = vcore_power_ratio_down + cm_mgr_num_array;

	ret = of_property_read_u32_array(
		node, "cm-mgr,cp-down", cpu_power_ratio_down, cm_mgr_num_array);
	ret = of_property_read_u32_array(node, "cm-mgr,cp-up",
					 cpu_power_ratio_up, cm_mgr_num_array);
	ret = of_property_read_u32_array(node, "cm-mgr,dt-down",
					 debounce_times_down_adb,
					 cm_mgr_num_array);
	ret = of_property_read_u32_array(
		node, "cm-mgr,dt-up", debounce_times_up_adb, cm_mgr_num_array);
	ret = of_property_read_u32_array(node, "cm-mgr,vp-down",
					 vcore_power_ratio_down,
					 cm_mgr_num_array);
	ret = of_property_read_u32_array(
		node, "cm-mgr,vp-up", vcore_power_ratio_up, cm_mgr_num_array);

	return 0;

ERROR1:
	kfree(cm_mgr_cpu_opp_to_dram);
ERROR:
	return ret;
}
EXPORT_SYMBOL_GPL(cm_mgr_check_dts_setting);

void cm_mgr_update_dram_by_cpu_opp(int cpu_opp)
{
	int ret = 0;
	int dram_opp = 0;

	if (!cm_mgr_use_cpu_to_dram_map || !cm_work_flag)
		return;

	if (cm_mgr_arch == CM_MGR_ARCH_V1 && cm_mgr_disable_fb == 1 &&
	    cm_mgr_blank_status == 1) {
		if (cm_mgr_cpu_to_dram_opp != cm_mgr_num_perf) {
			cm_mgr_cpu_to_dram_opp = cm_mgr_num_perf;
			ret = schedule_delayed_work(&cm_mgr_work, 1);
		}
		return;
	}

	if (!cm_mgr_cpu_map_dram_enable) {
		if (cm_mgr_cpu_to_dram_opp != cm_mgr_num_perf) {
			cm_mgr_cpu_to_dram_opp = cm_mgr_num_perf;
			ret = schedule_delayed_work(&cm_mgr_work, 1);
		}
		return;
	}

	if ((cpu_opp >= 0) && (cpu_opp < cm_mgr_cpu_opp_size))
		dram_opp = cm_mgr_cpu_opp_to_dram[cpu_opp];

	cm_mgr_cpu_to_dram_opp = dram_opp;
	ret = schedule_delayed_work(&cm_mgr_work, 1);
}
EXPORT_SYMBOL_GPL(cm_mgr_update_dram_by_cpu_opp);

struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool registered;
};

static void cm_mgr_cpu_frequency_tracer(void *ignore, unsigned int frequency,
					unsigned int cpu_id)
{
	int ret = 0;
	int cpu = 0, cluster = 0;
	struct cpufreq_policy *policy = NULL;
	unsigned int idx = 0;

#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG)
	u64 ts[2];

	ts[0] = sched_clock();
#endif

	if (!cm_mgr_cpu_map_dram_enable) {
		if (cm_work_flag && cm_mgr_cpu_to_dram_opp != cm_mgr_num_perf) {
			cm_mgr_cpu_to_dram_opp = cm_mgr_num_perf;
			ret = schedule_delayed_work(&cm_mgr_work, 1);
		}
		return;
	}

	policy = cpufreq_cpu_get(cpu_id);
	if (!policy)
		return;
	if (cpu_id != cpumask_first(policy->related_cpus)) {
		cpufreq_cpu_put(policy);
		return;
	}
	cpufreq_cpu_put(policy);

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			break;
		cpu = cpumask_first(policy->related_cpus);
		if (cpu == cpu_id)
			break;
		cpu = cpumask_last(policy->related_cpus);
		cluster++;
		cpufreq_cpu_put(policy);
	}

	if (policy) {
		idx = cpufreq_frequency_table_target(policy, frequency,
						     CPUFREQ_RELATION_L);
		if (hk.check_cm_mgr_status)
			hk.check_cm_mgr_status(cluster, frequency, idx);
		cpufreq_cpu_put(policy);
	}

#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG)
	ts[1] = sched_clock();
	if ((ts[1] - ts[0] > 300000ULL) && in_hardirq()) {
		printk_deferred("%s duration %llu, ts[0]=%llu, ts[1]=%llu\n",
				__func__, ts[1] - ts[0], ts[0], ts[1]);
	}
#endif
}

struct tracepoints_table cm_mgr_tracepoints[] = {
	{
		.name = "cpu_frequency",
		.func = cm_mgr_cpu_frequency_tracer,
	},
};

#define FOR_EACH_TRACEPOINT(i)                                                 \
	for (i = 0; i < sizeof(cm_mgr_tracepoints) /                           \
				sizeof(struct tracepoints_table);              \
	     i++)

static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_TRACEPOINT(i)
	{
		if (strcmp(cm_mgr_tracepoints[i].name, tp->name) == 0)
			cm_mgr_tracepoints[i].tp = tp;
	}
}

void tracepoint_cleanup(void)
{
	int i;

	FOR_EACH_TRACEPOINT(i)
	{
		if (cm_mgr_tracepoints[i].registered) {
			tracepoint_probe_unregister(cm_mgr_tracepoints[i].tp,
						    cm_mgr_tracepoints[i].func,
						    NULL);
			cm_mgr_tracepoints[i].registered = false;
		}
	}
}

void cm_mgr_process(struct work_struct *work)
{
	if (hk.cm_mgr_get_perfs)
		icc_set_bw(cm_mgr_bw_path, 0,
			   hk.cm_mgr_get_perfs(cm_mgr_cpu_to_dram_opp));
}
EXPORT_SYMBOL_GPL(cm_mgr_process);

static struct kobj_attribute dbg_cm_mgr_attribute =
	__ATTR(dbg_cm_mgr, 0660, dbg_cm_mgr_show, dbg_cm_mgr_store);

static struct attribute *attrs[] = {
	&dbg_cm_mgr_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

int cm_mgr_common_init(void)
{
	int i;
	int ret;

	CM_DBG_PRINT("CM COMMON INIT\n");
	cm_mgr_kobj = kobject_create_and_add("cm_mgr", kernel_kobj);
	if (!cm_mgr_kobj) {
		CM_DBG_PRINT("%s(%d): fail to create kobj.\n",
			__func__, __LINE__);
		ret = -ENOMEM;
		goto ERROR;
	}

	CM_DBG_PRINT("CM COMMON SYSFS CREATE.\n");
	ret = sysfs_create_group(cm_mgr_kobj, &attr_group);
	if (ret) {
		kobject_put(cm_mgr_kobj);
		CM_DBG_PRINT("%s(%d): fail to create sysfs group. ret %d\n",
			__func__, __LINE__, ret);
		goto ERROR;
	}

#if IS_ENABLED(CONFIG_MTK_CM_IPI)
	CM_DBG_PRINT("CM_IPI_INIT CALL\n");
	cm_ipi_init();
#else
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V2)
	CM_DBG_PRINT("CM REG IPI\n");
	ret = mtk_ipi_register(&sspm_ipidev, IPIS_C_CM, NULL, NULL,
			       (void *)&cm_ipi_ackdata);
	if (ret) {
		CM_DBG_PRINT("%s(%d): IPIS_C_CM ipi_register fail. ret %d\n",
			__func__, __LINE__, ret);
		cm_sspm_ready = -1;
		goto ERROR;
	}
	CM_DBG_PRINT("%s(%d): SSPM is ready to service CM IPI.\n", __func__,
		__LINE__);
	cm_sspm_ready = 1;
#endif /* CONFIG_MTK_TINYSYS_SSPM_V2 */
#endif /* CONFIG_MTK_CM_IPI */
	CM_DBG_PRINT("CM KERNEL TRACEPOINT\n");
	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	FOR_EACH_TRACEPOINT(i)
	{
		if (cm_mgr_tracepoints[i].tp == NULL) {
			CM_DBG_PRINT("%s(%d): %s not found.\n", __func__, __LINE__,
				cm_mgr_tracepoints[i].name);
			tracepoint_cleanup();
			return -1;
		}
	}
	ret = tracepoint_probe_register(cm_mgr_tracepoints[0].tp,
					cm_mgr_tracepoints[0].func, NULL);
	if (ret) {
		CM_DBG_PRINT("%s(%d): fail to activate tracepoint.\n", __func__,
			__LINE__);
		goto fail_reg_cpu_frequency_entry;
	}
	cm_mgr_tracepoints[0].registered = true;

fail_reg_cpu_frequency_entry:

	if (cm_mgr_arch == CM_MGR_ARCH_V1) {
		ret = mtk_disp_notifier_register("cm_mgr", &cm_mgr_fb_notifier);
		if (ret) {
			CM_DBG_PRINT("%s(%d): fail to register fb client. ret %d\n",
				__func__, __LINE__, ret);
			return ret;
		}

		cm_mgr_to_sspm_command(IPI_CM_MGR_ENABLE, cm_mgr_enable);

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V2) && !IS_ENABLED(CONFIG_MTK_CM_IPI)
		cm_mgr_to_sspm_command(IPI_CM_MGR_SSPM_ENABLE,
				       cm_mgr_sspm_enable);
#endif /* CONFIG_MTK_TINYSYS_SSPM_V2 && !IS_ENABLED(CONFIG_MTK_CM_IPI) */

		cm_mgr_to_sspm_command(IPI_CM_MGR_EMI_DEMAND_CHECK,
				       cm_mgr_emi_demand_check);

		cm_mgr_to_sspm_command(IPI_CM_MGR_LOADING_LEVEL,
				       cm_mgr_loading_level);

		cm_mgr_to_sspm_command(IPI_CM_MGR_LOADING_ENABLE,
				       cm_mgr_loading_enable);

		cm_mgr_to_sspm_command(IPI_CM_MGR_DEBOUNCE_TIMES_RESET_ADB,
				       debounce_times_reset_adb);

		if (cm_mgr_use_bcpu_weight) {
			cm_mgr_to_sspm_command(IPI_CM_MGR_BCPU_WEIGHT_MAX_SET,
					       cpu_power_bcpu_weight_max);

			cm_mgr_to_sspm_command(IPI_CM_MGR_BCPU_WEIGHT_MIN_SET,
					       cpu_power_bcpu_weight_min);

			cm_mgr_to_sspm_command(IPI_CM_MGR_BBCPU_WEIGHT_MAX_SET,
					       cpu_power_bbcpu_weight_max);

			cm_mgr_to_sspm_command(IPI_CM_MGR_BBCPU_WEIGHT_MIN_SET,
					       cpu_power_bbcpu_weight_min);
		}
	}

	INIT_DELAYED_WORK(&cm_mgr_work, cm_mgr_process);
	cm_work_flag = 1;

	return 0;

ERROR:
	return ret;
}
EXPORT_SYMBOL_GPL(cm_mgr_common_init);

void cm_mgr_common_exit(void)
{
	int ret;

	kfree(cm_mgr_cpu_opp_to_dram);
	kfree(cm_mgr_buf);

	kobject_put(cm_mgr_kobj);

	if (cm_mgr_arch == CM_MGR_ARCH_V1) {
		ret = mtk_disp_notifier_unregister(&cm_mgr_fb_notifier);
		if (ret)
			CM_DBG_PRINT("%s(%d): fail to unregister fb client. ret %d\n",
				__func__, __LINE__, ret);
	}
}
EXPORT_SYMBOL_GPL(cm_mgr_common_exit);
MODULE_LICENSE("GPL");
