// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */
#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include "mtk_battery_oc_throttling.h"
#include "mtk_low_battery_throttling.h"
#include "mtk_bp_thl.h"
#include "mtk_cpu_power_throttling.h"
#include "core_ctl.h"

#if !IS_BUILTIN(CONFIG_MTK_CPU_POWER_THROTTLING)
#define CREATE_TRACE_POINTS
#endif
#include "mtk_low_battery_throttling_trace.h"

#define CPU_LIMIT_FREQ 900000
#define CPU_UNLIMIT_FREQ 2147483647
#define CLUSTER_NUM 3
#define CORE_NUM 8
#define NAME_LENGTH 32
#define DISABLE_MPMM 3

static int cpu_pt_table_idx;
static unsigned int system_boot_completed;
static bool bootup_pt_support;
static bool switch_pt;
static int lbat_tb_num;
static unsigned int mpmm_gear;

enum cpu_pt_table_num {
	CPU_PT_TABLE0,
	CPU_PT_TABLE1,
	CPU_PT_TABLE2,
	CPU_PT_TABLE3,
	CPU_PT_TABLE4,
	CPU_PT_TABLE5,
	CPU_PT_TABLE6,
	CPU_PT_TABLE_MAX
};

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

struct cpu_pt_priv {
	char max_lv_name[NAME_LENGTH];
	char freq_limit_name[NAME_LENGTH];
	char core_active_name[NAME_LENGTH];
	u32 max_lv;
	u32 cur_lv;
	u32 *freq_limit;
	u32 *core_active;
	u32 mpmm_enable;
	u32 mpmm_activate_lv;
};

struct cpu_bootup_pt_priv {
	char freq_limit_booting_name[NAME_LENGTH];
	u32 max_lv;
	u32 bootmode;
	u32 *freq_limit_booting;
};

struct cpu_pt_table {
	char freq_limit_name[NAME_LENGTH];
	u32 max_tb_num;
	u32 *freq_limit;
};

struct cpu_pause_tbl {
	int (*pause_func)(unsigned int cpu, bool is_pause, unsigned int request_mask);
};

static struct cpu_pt_priv cpu_pt_info[POWER_THROTTLING_TYPE_MAX] = {
	[LBAT_POWER_THROTTLING] = {
		.max_lv_name = "lbat-max-level",
		.freq_limit_name = "lbat-limit-freq-lv",
		.core_active_name = "",
		.max_lv = LOW_BATTERY_LEVEL_NUM - 1,
	},
	[OC_POWER_THROTTLING] = {
		.max_lv_name = "oc-max-level",
		.freq_limit_name = "oc-limit-freq-lv",
		.core_active_name = "",
		.max_lv = BATTERY_OC_LEVEL_NUM - 1,
	},
	[SOC_POWER_THROTTLING] = {
		.max_lv_name = "soc-max-level",
		.freq_limit_name = "soc-limit-freq-lv",
		.core_active_name = "soc-core-active-lv",
		.max_lv = BATTERY_PERCENT_LEVEL_NUM - 1,
	},
};

static struct cpu_pt_table cpu_pt_tb[CPU_PT_TABLE_MAX] = {
	[CPU_PT_TABLE0] = {
		.freq_limit_name = "lbat-limit-freq-lv",
	},
	[CPU_PT_TABLE1] = {
		.freq_limit_name = "lbat-limit-freq-tb1-lv",
	},
	[CPU_PT_TABLE2] = {
		.freq_limit_name = "lbat-limit-freq-tb2-lv",
	},
	[CPU_PT_TABLE3] = {
		.freq_limit_name = "lbat-limit-freq-tb3-lv",
	},
	[CPU_PT_TABLE4] = {
		.freq_limit_name = "lbat-limit-freq-tb4-lv",
	},
	[CPU_PT_TABLE5] = {
		.freq_limit_name = "lbat-limit-freq-tb5-lv",
	},
	[CPU_PT_TABLE6] = {
		.freq_limit_name = "lbat-limit-freq-tb6-lv",
	},
};

static struct cpu_bootup_pt_priv cpu_bootup_pt_info = {
	.freq_limit_booting_name = "limit-freq-bootmode",
	.max_lv = 1,
	.bootmode = 0,
};

static DEFINE_MUTEX(cpu_thr_lock);
static DEFINE_MUTEX(cpu_freq_lock);
static unsigned int cur_core_active[CORE_NUM];
static struct cpu_pause_tbl cicb = {
	.pause_func = NULL,
};

static LIST_HEAD(pt_policy_list);
static LIST_HEAD(bootup_pt_policy_list);

static int pt_set_cpu_active(unsigned int cpu, bool active)
{
	int ret = -EPERM;
	bool pause;

	if (cur_core_active[cpu] == active)
		return 0;

	if (cicb.pause_func) {
		pause = (active == false) ? true : false;
		ret = cicb.pause_func(cpu, pause, POWER_THROTTLE_FORCE_PAUSE);
	}

	if (ret >= 0) {
		cur_core_active[cpu] = active;
		pr_info("%s: PT success to set cpu%d active=%d, ret=%d\n", __func__, cpu, active, ret);
	} else {
		pr_info("%s: PT failed to set cpu%d active=%d, ret=%d\n", __func__, cpu, active, ret);
	}
	return ret;
}

#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
static void cpu_pt_low_battery_cb(enum LOW_BATTERY_LEVEL_TAG level, void *data)
{
	struct cpu_pt_policy *pt_policy;
	int cpu;
	s32 freq_limit;

	mutex_lock(&cpu_freq_lock);
	cpu_pt_info[LBAT_POWER_THROTTLING].cur_lv = level;

	if (level > cpu_pt_info[LBAT_POWER_THROTTLING].max_lv) {
		mutex_unlock(&cpu_freq_lock);
		return;
	}
	list_for_each_entry(pt_policy, &pt_policy_list, cpu_pt_list) {
		if (pt_policy->pt_type == LBAT_POWER_THROTTLING) {
			if (level != LOW_BATTERY_LEVEL_0)
				freq_limit = pt_policy->freq_limit[level-1];
			else
				freq_limit = FREQ_QOS_MAX_DEFAULT_VALUE;
			freq_qos_update_request(&pt_policy->qos_req, freq_limit);
			cpu = pt_policy->cpu;
			trace_low_battery_throttling_cpu_freq(cpu, freq_limit);
		}
	}
	mutex_unlock(&cpu_freq_lock);
}
#endif

static void cpu_bootup_pt_trigger(void)
{
	struct cpu_bootup_pt_policy *bootup_pt_policy;
	s32 freq_limit;

	list_for_each_entry(bootup_pt_policy, &bootup_pt_policy_list, cpu_bootup_pt_list) {
		freq_limit = bootup_pt_policy->freq_limit_booting[0];
		pr_info("%s: freq_limit=%d\n", __func__, freq_limit);
		freq_qos_update_request(&bootup_pt_policy->qos_req, freq_limit);
	}
}

static void mpmm_enable (unsigned int gear)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_PT_CONTROL, PT_OP_SET_MPMM, gear, 0, 0, 0, 0, 0, &res);
	if (res.a0)
		pr_info("%s, SMC call fail", __func__);
}

#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
static void cpu_pt_over_current_cb(enum BATTERY_OC_LEVEL_TAG level, void *data)
{
	struct cpu_pt_policy *pt_policy;
	s32 freq_limit;
	if (level > cpu_pt_info[OC_POWER_THROTTLING].max_lv)
		return;
	list_for_each_entry(pt_policy, &pt_policy_list, cpu_pt_list) {
		if (pt_policy->pt_type == OC_POWER_THROTTLING) {
			if (level != BATTERY_OC_LEVEL_0)
				freq_limit = pt_policy->freq_limit[level-1];
			else
				freq_limit = FREQ_QOS_MAX_DEFAULT_VALUE;
			freq_qos_update_request(&pt_policy->qos_req, freq_limit);
		}
	}
}
#endif
#if IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING)
static void cpu_pt_battery_percent_cb(enum BATTERY_PERCENT_LEVEL_TAG level)
{
	struct cpu_pt_policy *pt_policy;
	struct cpu_pt_priv *pt_info_p = &cpu_pt_info[SOC_POWER_THROTTLING];
	s32 freq_limit;
	int idx = 0, i = 0, active;

	if (level > cpu_pt_info[SOC_POWER_THROTTLING].max_lv)
		return;

	list_for_each_entry(pt_policy, &pt_policy_list, cpu_pt_list) {
		if (pt_policy->pt_type == SOC_POWER_THROTTLING) {
			if (level != BATTERY_PERCENT_LEVEL_0)
				freq_limit = pt_policy->freq_limit[level-1];
			else
				freq_limit = FREQ_QOS_MAX_DEFAULT_VALUE;
			freq_qos_update_request(&pt_policy->qos_req, freq_limit);
		}
	}

	mutex_lock(&cpu_thr_lock);
	for_each_possible_cpu(i) {
		if (level == BATTERY_PERCENT_LEVEL_0)
			pt_set_cpu_active(i, true);
		else {
			idx = (level - 1) * CORE_NUM + i;
			active = (pt_info_p->core_active[idx] > 0) ? true : false;
			pt_set_cpu_active(i, active);
		}
	}
	pt_info_p->cur_lv = level;
	mutex_unlock(&cpu_thr_lock);

	if (pt_info_p->mpmm_enable != 0 && level >= pt_info_p->mpmm_activate_lv)
		mpmm_enable(mpmm_gear);
	else if (pt_info_p->mpmm_enable != 0 && level < pt_info_p->mpmm_activate_lv)
		mpmm_enable (DISABLE_MPMM);
}
#endif
static void __used cpu_limit_default_setting(struct device *dev, enum cpu_pt_type type)
{
	struct device_node *np = dev->of_node;
	int i, max_lv, ret;
	struct cpu_pt_priv *pt_info_p;

	pt_info_p = &cpu_pt_info[type];
	if (type == LBAT_POWER_THROTTLING)
		max_lv = 2;
	else if (type == OC_POWER_THROTTLING)
		max_lv = 1;
	else
		max_lv = 0;
	pt_info_p->max_lv = max_lv;
	if (!pt_info_p->max_lv)
		return;

	pt_info_p->freq_limit = kzalloc(sizeof(u32) * pt_info_p->max_lv * CLUSTER_NUM, GFP_KERNEL);
	pt_info_p->core_active = kzalloc(sizeof(u32) * pt_info_p->max_lv * CORE_NUM, GFP_KERNEL);
	for (i = 0; i < CLUSTER_NUM; i++) {
		pt_info_p->freq_limit[i] = CPU_LIMIT_FREQ;
		pt_info_p->core_active[i] = 1;
	}

	if (type == LBAT_POWER_THROTTLING) {
		ret = of_property_read_u32_array(np, "lbat_cpu_limit",
			&pt_info_p->freq_limit[0], 3);
		if (ret < 0)
			pr_notice("%s: get lbat cpu limit fail %d\n", __func__, ret);
	} else if (type == OC_POWER_THROTTLING) {
		ret = of_property_read_u32_array(np, "oc_cpu_limit",
			&pt_info_p->freq_limit[0], 3);
		if (ret < 0)
			pr_notice("%s: get oc cpu limit fail %d\n", __func__, ret);
	}
	for (i = 1; i < pt_info_p->max_lv; i++) {
		memcpy(&pt_info_p->freq_limit[i * CLUSTER_NUM], &pt_info_p->freq_limit[0],
			sizeof(u32) * CLUSTER_NUM);
		memcpy(&pt_info_p->core_active[i * CORE_NUM], &pt_info_p->core_active[0],
			sizeof(u32) * CORE_NUM);
	}
}

static bool parse_bootup_pt_table(struct device_node *np, struct tag_bootmode *tag)
{
	struct cpu_bootup_pt_priv *bootup_pt_info_p;
	int ret, k;
	char buf[NAME_LENGTH];

	bootup_pt_info_p = &cpu_bootup_pt_info;
	bootup_pt_info_p->max_lv = 1;
	bootup_pt_info_p->freq_limit_booting = kzalloc(sizeof(u32) * bootup_pt_info_p->max_lv *
		CLUSTER_NUM, GFP_KERNEL);
	if (!bootup_pt_info_p->freq_limit_booting)
		return false;

	memset(buf, 0, sizeof(buf));

	ret = snprintf(buf, sizeof(buf), "%s%d", bootup_pt_info_p->freq_limit_booting_name, tag->bootmode);
	if (ret < 0){
		pr_notice("can't merge %s %d\n", bootup_pt_info_p->freq_limit_booting_name, tag->bootmode);
		for (k = 0; k < CLUSTER_NUM; k++)
			bootup_pt_info_p->freq_limit_booting[k] = CPU_UNLIMIT_FREQ;
		return false;
	}
	ret = of_property_read_u32_array(np, buf,
		&bootup_pt_info_p->freq_limit_booting[0], CLUSTER_NUM);
	if (ret < 0) {
		pr_notice("%s: get %s fail %d\n", __func__, buf, ret);
		for (k = 0; k < CLUSTER_NUM; k++)
			bootup_pt_info_p->freq_limit_booting[k] = CPU_UNLIMIT_FREQ;
		return false;
	}
	pr_notice("%s: bootmode:0x%x\n", __func__, tag->bootmode);
	return true;

}

static bool parse_switchpt_table(struct device_node *np)
{
	struct cpu_pt_table *cpu_pt_table;
	int i, j, k, ret;
	char buf[NAME_LENGTH];

	for (i = 0; i < lbat_tb_num; i++) {
		cpu_pt_table = &cpu_pt_tb[i];
		cpu_pt_table->max_tb_num = lbat_tb_num;
		cpu_pt_table->freq_limit = kzalloc(sizeof(u32) * cpu_pt_info[LBAT_POWER_THROTTLING].max_lv *
			CLUSTER_NUM, GFP_KERNEL);
		if (!cpu_pt_table->freq_limit)
			return false;

		for (j = 0; j < cpu_pt_info[LBAT_POWER_THROTTLING].max_lv; j++) {
			memset(buf, 0, sizeof(buf));
			ret = snprintf(buf, sizeof(buf), "%s%d", cpu_pt_table->freq_limit_name, j+1);
			if (ret < 0) {
				pr_notice("can't merge %s %d\n", cpu_pt_table->freq_limit_name, j+1);
				for (k = 0; k < CLUSTER_NUM; k++)
					cpu_pt_table->freq_limit[j * CLUSTER_NUM + k] = CPU_UNLIMIT_FREQ;
				return false;
			}

			ret = of_property_read_u32_array(np, buf,
				&cpu_pt_table->freq_limit[j * CLUSTER_NUM], CLUSTER_NUM);
			if (ret < 0) {
				pr_notice("%s: get %s fail %d\n", __func__, buf, ret);
				for (k = 0; k < CLUSTER_NUM; k++)
					cpu_pt_table->freq_limit[j * CLUSTER_NUM + k] = CPU_UNLIMIT_FREQ;
				return false;
			}
		}
	}
	return true;
}

static int __used parse_cpu_limit_table(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct device_node *np_bootmode;
	int i, j, k, num, ret;
	struct cpu_pt_priv *pt_info_p;
	struct tag_bootmode *tag;

	char buf[NAME_LENGTH];

	for (i = 0; i < POWER_THROTTLING_TYPE_MAX; i++) {
		pt_info_p = &cpu_pt_info[i];
		ret = of_property_read_u32(np, pt_info_p->max_lv_name, &num);
		if (ret < 0) {
			cpu_limit_default_setting(dev, i);
			continue;
		} else if (num <= 0 || num > pt_info_p->max_lv) {
			pt_info_p->max_lv = 0;
			continue;
		}

		pt_info_p->max_lv = num;
		pt_info_p->freq_limit = kzalloc(sizeof(u32) * pt_info_p->max_lv * CLUSTER_NUM, GFP_KERNEL);
		if (!pt_info_p->freq_limit)
			return -ENOMEM;

		pt_info_p->core_active = kzalloc(sizeof(u32) * pt_info_p->max_lv * CORE_NUM, GFP_KERNEL);
		if (!pt_info_p->core_active)
			return -ENOMEM;

		for (j = 0; j < pt_info_p->max_lv; j++) {
			memset(buf, 0, sizeof(buf));
			ret = snprintf(buf, sizeof(buf), "%s%d", pt_info_p->freq_limit_name, j+1);
			if (ret < 0)
				pr_notice("can't merge %s %d\n", pt_info_p->freq_limit_name, j+1);

			ret = of_property_read_u32_array(np, buf,
				&pt_info_p->freq_limit[j * CLUSTER_NUM], CLUSTER_NUM);
			if (ret < 0) {
				pr_notice("%s: get %s fail %d\n", __func__, buf, ret);
				for (k = 0; k < CLUSTER_NUM; k++)
					pt_info_p->freq_limit[j * CLUSTER_NUM + k] = CPU_UNLIMIT_FREQ;
			}

			if (i == SOC_POWER_THROTTLING) {
				memset(buf, 0, sizeof(buf));
				ret = snprintf(buf, sizeof(buf), "%s%d", pt_info_p->core_active_name, j+1);
				if (ret < 0)
					pr_notice("can't merge %s %d\n", pt_info_p->core_active_name, j+1);
				ret = of_property_read_u32_array(np, buf,
					&pt_info_p->core_active[j * CORE_NUM], CORE_NUM);
				if (ret < 0) {
					pr_notice("%s: get %s fail %d set core limit to 1\n", __func__, buf, ret);
					for (k = 0; k < CORE_NUM; k++)
						pt_info_p->core_active[j * CORE_NUM + k] = 1;
				}
				ret = of_property_read_u32(np, "mpmm-enable", &pt_info_p->mpmm_enable);
				if (ret) {
					pt_info_p->mpmm_enable = 0;
					pr_info("Failed to read mpmm-enable property\n");
				}
				if (pt_info_p->mpmm_enable == 1) {
					ret = of_property_read_u32(np, "mpmm-gear", &mpmm_gear);
					if (ret) {
						pt_info_p->mpmm_enable = 0;
						pr_info("Failed to read mpmm-gear property\n");
					} else if (mpmm_gear > 2) {
						pt_info_p->mpmm_enable = 0;
						pr_info("mpmm-gear value out of range: %u\n", mpmm_gear);
					}
					ret = of_property_read_u32(np, "soc-limit-mpmm-lv",
						&pt_info_p->mpmm_activate_lv);
					if (ret || pt_info_p->mpmm_activate_lv > pt_info_p->max_lv) {
						pt_info_p->mpmm_activate_lv = pt_info_p->max_lv;
						pr_info("Failed to read soc-limit-mpmm-lv property\n");
					}
				}
			}
		}
	}
	/*cpu bootup pt*/
	np_bootmode = of_parse_phandle(dev->of_node, "bootmode", 0);
	if (!np_bootmode){
		pr_notice("%s: get bootmode fail\n", __func__);
		bootup_pt_support = false;
	} else {
		tag = (struct tag_bootmode *)of_get_property(np_bootmode, "atag,boot", NULL);
		if (!tag) {
			pr_notice("failed to get atag,boot\n");
			bootup_pt_support = false;
		} else {
			bootup_pt_support = parse_bootup_pt_table(np, tag);
		}
	}
	// cpu pt table switch
	ret = of_property_read_u32(np, "lbat-max-tb-num", &lbat_tb_num);
	if (ret || lbat_tb_num < 0 || lbat_tb_num > CPU_PT_TABLE_MAX) {
		pr_notice("%s: get lbat_tb_num %d fail set to default %d\n", __func__, lbat_tb_num, ret);
		lbat_tb_num = 1;
		switch_pt = false;
	} else {
		switch_pt = parse_switchpt_table(np);
	}
	return 0;
}

/*****************************************************************************
 * modify cpu throttle freq
 ******************************************************************************/
static ssize_t modify_cpu_throttle_freq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct cpu_pt_table *pt_info_p;
	u32 max_tb_num = 0;
	u32 max_lv = 0;
	int i = 0,j = 0;

	if (switch_pt == false) {
		dev_info(dev, "not support switch_pt\n");
		return -EINVAL;
	}
	max_tb_num = cpu_pt_tb[CPU_PT_TABLE0].max_tb_num;
	max_lv = cpu_pt_info[LBAT_POWER_THROTTLING].max_lv;

	if (max_tb_num > CPU_PT_TABLE_MAX)
		max_tb_num = 1;
	if (max_lv > LOW_BATTERY_LEVEL_NUM - 1)
		max_lv = 3;

	for (i = 0; i < max_tb_num; i++ ) {
		pt_info_p = &cpu_pt_tb[i];
		len += snprintf(buf + len, PAGE_SIZE - len, "cpu table%d limit freq:", i);
		for (j = 0; j < max_lv * CLUSTER_NUM; j++)
			len += snprintf(buf + len, PAGE_SIZE - len, " %d", pt_info_p->freq_limit[j]);

		len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	}
	return len;
}

static ssize_t modify_cpu_throttle_freq_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int i, j, k = 0, len = 0;
	u32 freq_limit[LOW_BATTERY_LEVEL_NUM * CLUSTER_NUM];
	u32 table_idx;
	struct cpu_pt_policy *pt_policy;
	u32 max_tb_num = 0;
	u32 max_lv = 0;

	if (switch_pt == false) {
		dev_info(dev, "not support switch_pt\n");
		return -EINVAL;
	}
	max_tb_num = cpu_pt_tb[CPU_PT_TABLE0].max_tb_num;
	max_lv = cpu_pt_info[LBAT_POWER_THROTTLING].max_lv;

	if (sscanf(buf, "%u%n", &table_idx, &len)!=1) {
		dev_info(dev, "parameter number not correct\n");
		return -EINVAL;
	}
	buf += len;
	if (table_idx > max_tb_num - 1) {
		dev_info(dev, "Invalid table_idx: %u\n", table_idx);
		return -EINVAL;
	}
	for (i = 0; i < max_lv * CLUSTER_NUM; i++) {
		if (sscanf(buf, "%u%n", &freq_limit[i], &len) != 1) {
			dev_info(dev, "Failed to read freq_limit[%d]\n", i);
			return -EINVAL;
		}
		buf += len;

		if (freq_limit[i] <= 0 || freq_limit[i] > CPU_UNLIMIT_FREQ) {
			dev_info(dev, "Invalid freq_limit[%d]: %u\n", i, freq_limit[i]);
			return -EINVAL;
		}
	}

	mutex_lock(&cpu_freq_lock);
	for (i = 0; i < max_lv * CLUSTER_NUM; i++)
		cpu_pt_tb[table_idx].freq_limit[i] = freq_limit[i];
	if (table_idx==cpu_pt_table_idx){
		list_for_each_entry(pt_policy, &pt_policy_list, cpu_pt_list) {
			if (pt_policy->pt_type == LBAT_POWER_THROTTLING) {
				for (j = 0; j < cpu_pt_info[LBAT_POWER_THROTTLING].max_lv; j++) {
					pt_policy->freq_limit[j] = cpu_pt_tb[table_idx].freq_limit[j * CLUSTER_NUM + k];
					dev_notice (dev, "i:%d, k:%d, pt_policy->freq_limit: %d, cpu_pt_info[i].freq_limit %d\n",
					i, k, pt_policy->freq_limit[j],	cpu_pt_tb[table_idx].freq_limit[j * CLUSTER_NUM
					+ k]);
				}
				k++;
			}
		}
	}
	mutex_unlock(&cpu_freq_lock);

	cpu_pt_low_battery_cb(cpu_pt_info[LBAT_POWER_THROTTLING].cur_lv, NULL);

	for (i = 0; i < max_lv * CLUSTER_NUM; i++)
		dev_notice(dev, "freq_limit[%d]: %d\n", i, cpu_pt_tb[table_idx].freq_limit[i]);
	return size;
}
static DEVICE_ATTR_RW(modify_cpu_throttle_freq);
static ssize_t boot_notify_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int len = 0;

	if (bootup_pt_support == false){
		dev_info(dev, "not support cpu bootup\n");
		return -EINVAL;
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "system_boot_completed: %u\n", system_boot_completed);
	return len;
}

static ssize_t boot_notify_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int boot_completed = 0;
	struct cpu_bootup_pt_policy *bootup_pt_policy;
	s32 freq_limit;

	if (bootup_pt_support == false){
		dev_info(dev, "not support cpu bootup\n");
		return -EINVAL;
	}
	if (sscanf(buf, "%u\n", &boot_completed) != 1) {
		dev_info(dev, "parameter number not correct\n");
		return -EINVAL;
	}
	if (boot_completed > 1) {
		dev_info(dev, "invalid input %u\n", boot_completed);
		return -EINVAL;
	}
	system_boot_completed = boot_completed;

	list_for_each_entry(bootup_pt_policy, &bootup_pt_policy_list, cpu_bootup_pt_list) {
		freq_limit = FREQ_QOS_MAX_DEFAULT_VALUE;
		pr_info("%s: freq_limit=%d\n", __func__, freq_limit);
		freq_qos_update_request(&bootup_pt_policy->qos_req, freq_limit);
	}

	return size;
}

static DEVICE_ATTR_RW(boot_notify);

static ssize_t cpu_pt_table_idx_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int len = 0;

	if (switch_pt == false) {
		dev_info(dev, "not support switch_pt\n");
		return -EINVAL;
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "cpu_pt_table_idx: %d\n", cpu_pt_table_idx);

	return len;
}

static ssize_t cpu_pt_table_idx_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	char cmd[20];
	int k = 0;
	struct cpu_pt_policy *pt_policy;

	if (switch_pt == false) {
		dev_info(dev, "not support switch_pt\n");
		return -EINVAL;
	}
	if ((sscanf(buf, "%9s %u\n", cmd, &cpu_pt_table_idx) != 2) || (strncmp(cmd, "table_idx", 9) != 0)) {
		dev_info(dev, "parameter number not correct\n");
		return -EINVAL;
	}
	if (cpu_pt_table_idx < 0 || cpu_pt_table_idx >= lbat_tb_num) {
		pr_info("Invalid voltage table index.\n");
		return -EINVAL;
	}
	mutex_lock(&cpu_freq_lock);

	list_for_each_entry(pt_policy, &pt_policy_list, cpu_pt_list) {
		if (pt_policy->pt_type == LBAT_POWER_THROTTLING) {
			for (int j = 0; j < cpu_pt_info[LBAT_POWER_THROTTLING].max_lv; j++) {
				pt_policy->freq_limit[j] = cpu_pt_tb[cpu_pt_table_idx].freq_limit[j * CLUSTER_NUM + k];
				dev_notice(dev, "k:%d, pt_policy->freq_limit: %d, cpu_pt_tb[i].freq_limit %d\n", k,
				pt_policy->freq_limit[j], cpu_pt_tb[cpu_pt_table_idx].freq_limit[j * CLUSTER_NUM + k]);
			}
			k++;
		}
	}

	mutex_unlock(&cpu_freq_lock);

	cpu_pt_low_battery_cb(cpu_pt_info[LBAT_POWER_THROTTLING].cur_lv, NULL);

	return size;
}
static DEVICE_ATTR_RW(cpu_pt_table_idx);

static ssize_t set_mpmm_show(
		struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE,"%d\n", mpmm_gear);
}

static ssize_t set_mpmm_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf,
					   size_t size)
{
	unsigned int val;
	char cmd[9];

	if (sscanf(buf, "%8s %u\n", cmd, &val) != 2) {
		dev_info(dev, "parameter number not correct\n");
		return -EINVAL;
	}
	if (strncmp(cmd, "set_mpmm", 8))
		return -EINVAL;
	if (val < 4)
		mpmm_gear = val;

	mpmm_enable(mpmm_gear);
	pr_info("[%s] smc send", __func__);
	return size;
}
static DEVICE_ATTR_RW(set_mpmm);


int register_pt_isolate_cb(cpu_isolate_cb cb_func)
{
	int ret = 0, idx;
	unsigned int i = 0;
	bool active;
	struct cpu_pt_priv *pt_info_p = &cpu_pt_info[SOC_POWER_THROTTLING];

	if (cb_func) {
		cicb.pause_func = cb_func;

		mutex_lock(&cpu_thr_lock);
		if (pt_info_p->core_active && pt_info_p->cur_lv > 0) {
			for (i = 0; i < CORE_NUM; i++) {
				idx = (pt_info_p->cur_lv - 1) * CORE_NUM + i;
				active = (pt_info_p->core_active[idx] > 0) ? true : false;
				pt_set_cpu_active(i, active);
			}
		}
		mutex_unlock(&cpu_thr_lock);

	} else
		ret = -EINVAL;

	return ret;
}
EXPORT_SYMBOL(register_pt_isolate_cb);

static int mtk_cpu_power_throttling_probe(struct platform_device *pdev)
{
	struct cpufreq_policy *policy;
	struct cpu_pt_policy *pt_policy;
	struct cpu_bootup_pt_policy *bootup_pt_policy;
	unsigned int i = 0, j = 0, k = 0;
	int cpu, ret, max_lv;
	s32 *freq_limit_t;
	s32 *freq_limit_booting_t;

	switch_pt = false;
	bootup_pt_support = false;
	ret = parse_cpu_limit_table(&pdev->dev);
	if (ret != 0)
		return ret;
	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;
		if (policy->cpu == cpu) {
			for (i = 0; i < POWER_THROTTLING_TYPE_MAX; i++) {
				pt_policy = kzalloc(sizeof(*pt_policy), GFP_KERNEL);
				if (!pt_policy)
					return -ENOMEM;
				max_lv = (cpu_pt_info[i].max_lv > 1) ? cpu_pt_info[i].max_lv : 1;
				freq_limit_t = kcalloc(max_lv, sizeof(u32), GFP_KERNEL);
				if (!freq_limit_t) {
					kfree(pt_policy);
					return -ENOMEM;
				}

				for (j = 0; j < cpu_pt_info[i].max_lv; j++)
					freq_limit_t[j] = cpu_pt_info[i].freq_limit[j * CLUSTER_NUM + k];
				pt_policy->pt_type = (enum cpu_pt_type)i;
				pt_policy->policy = policy;
				pt_policy->cpu = cpu;
				pt_policy->pt_max_lv = cpu_pt_info[i].max_lv;
				pt_policy->freq_limit = freq_limit_t;
				pr_notice("%s: pt_policy->freq_limit:%d\n", __func__, pt_policy->freq_limit[0]);

				ret = freq_qos_add_request(&policy->constraints,
					&pt_policy->qos_req, FREQ_QOS_MAX,
					FREQ_QOS_MAX_DEFAULT_VALUE);
				if (ret < 0) {
					pr_notice("%s: Fail to add freq constraint (%d)\n",
						__func__, ret);
					kfree(pt_policy);
					kfree(freq_limit_t);
					return ret;
				}
				list_add_tail(&pt_policy->cpu_pt_list, &pt_policy_list);
			}
			if (bootup_pt_support == true){
				/*cpu bootup pt*/
				bootup_pt_policy = kzalloc(sizeof(*bootup_pt_policy), GFP_KERNEL);
				if (!bootup_pt_policy)
					return -ENOMEM;
				freq_limit_booting_t = kcalloc(cpu_bootup_pt_info.max_lv, sizeof(u32), GFP_KERNEL);
				if (!freq_limit_booting_t) {
					kfree(bootup_pt_policy);
					return -ENOMEM;
				}
				freq_limit_booting_t[0] = cpu_bootup_pt_info.freq_limit_booting[k];
				pr_notice("%s: freq_limit_booting_t: %d,\n", __func__, freq_limit_booting_t[0]);
				bootup_pt_policy->policy = policy;
				bootup_pt_policy->cpu = cpu;
				bootup_pt_policy->pt_max_lv = cpu_bootup_pt_info.max_lv;
				bootup_pt_policy->freq_limit_booting= freq_limit_booting_t;
				pr_notice("%s: bootup_pt_policy->freq_limit_booting:%d\n", __func__,
					bootup_pt_policy->freq_limit_booting[0]);

				ret = freq_qos_add_request(&policy->constraints,
					&bootup_pt_policy->qos_req, FREQ_QOS_MAX,
					FREQ_QOS_MAX_DEFAULT_VALUE);
				if (ret < 0) {
					pr_notice("%s: Fail to add freq constraint (%d)\n",
						__func__, ret);
					kfree(bootup_pt_policy);
					kfree(freq_limit_booting_t);
					return ret;
				}
				list_add_tail(&bootup_pt_policy->cpu_bootup_pt_list, &bootup_pt_policy_list);
			}
			k++;
		}
	}
	if (bootup_pt_support)
		cpu_bootup_pt_trigger();
#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
	if (cpu_pt_info[LBAT_POWER_THROTTLING].max_lv > 0)
		register_low_battery_notify(&cpu_pt_low_battery_cb, LOW_BATTERY_PRIO_CPU_B, NULL);
#endif
#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
	if (cpu_pt_info[OC_POWER_THROTTLING].max_lv > 0)
		register_battery_oc_notify(&cpu_pt_over_current_cb, BATTERY_OC_PRIO_CPU_B, NULL);
#endif
#if IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING)
	if (cpu_pt_info[SOC_POWER_THROTTLING].max_lv > 0) {
		for (i = 0; i < CORE_NUM; i++)
			cur_core_active[i] = 1;

		register_bp_thl_notify(&cpu_pt_battery_percent_cb, BATTERY_PERCENT_PRIO_CPU_B);
	}
#endif
	ret = device_create_file(&(pdev->dev),
		&dev_attr_modify_cpu_throttle_freq);
	if (ret) {
		dev_notice(&pdev->dev, "create file error ret=%d\n", ret);
		return ret;
	}
	ret = device_create_file(&(pdev->dev),
		&dev_attr_cpu_pt_table_idx);
	if (ret) {
		dev_notice(&pdev->dev, "create file error ret=%d\n", ret);
		return ret;
	}
	ret = device_create_file(&(pdev->dev),
		&dev_attr_boot_notify);
	if (ret) {
		dev_notice(&pdev->dev, "create file error ret=%d\n", ret);
		return ret;
	}
	ret = device_create_file(&(pdev->dev),
		&dev_attr_set_mpmm);
	if (ret) {
		dev_notice(&pdev->dev, "create file error ret=%d\n", ret);
		return ret;
	}
	cpu_pt_table_idx = 0;
	system_boot_completed = 0;

	return 0;
}
static void mtk_cpu_power_throttling_remove(struct platform_device *pdev)
{
	struct cpu_pt_policy *pt_policy, *pt_policy_t;
	list_for_each_entry_safe(pt_policy, pt_policy_t, &pt_policy_list, cpu_pt_list) {
		freq_qos_remove_request(&pt_policy->qos_req);
		cpufreq_cpu_put(pt_policy->policy);
		list_del(&pt_policy->cpu_pt_list);
		kfree(pt_policy);
	}
}
static const struct of_device_id cpu_power_throttling_of_match[] = {
	{ .compatible = "mediatek,cpu-power-throttling", },
	{},
};
MODULE_DEVICE_TABLE(of, cpu_power_throttling_of_match);
static struct platform_driver cpu_power_throttling_driver = {
	.probe = mtk_cpu_power_throttling_probe,
	.remove = mtk_cpu_power_throttling_remove,
	.driver = {
		.name = "mtk-cpu_power_throttling",
		.of_match_table = cpu_power_throttling_of_match,
	},
};
module_platform_driver(cpu_power_throttling_driver);
MODULE_AUTHOR("Samuel Hsieh");
MODULE_DESCRIPTION("MTK cpu power throttling driver");
MODULE_LICENSE("GPL");
