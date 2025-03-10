// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */
#include <linux/of.h>
#include <linux/platform_device.h>
#include "mtk_gpu_power_throttling.h"
#include "mtk_battery_oc_throttling.h"
#include "mtk_low_battery_throttling.h"
#include "mtk_bp_thl.h"
#include "gpufreq_v2.h"

#define CREATE_TRACE_POINTS
#include "mtk_low_battery_throttling_trace.h"

#define GPU_LIMIT_FREQ 981000

static DEFINE_MUTEX(gpu_freq_lock);

static int gpu_pt_table_idx;
static unsigned int system_boot_completed;
static bool bootup_pt_support;
static bool switch_pt;
static int lbat_tb_num;

enum gpu_pt_table_num {
	GPU_PT_TABLE0,
	GPU_PT_TABLE1,
	GPU_PT_TABLE2,
	GPU_PT_TABLE3,
	GPU_PT_TABLE4,
	GPU_PT_TABLE5,
	GPU_PT_TABLE_MAX
};

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

struct gpu_pt_priv {
	char max_lv_name[32];
	char limit_name[32];
	u32 max_lv;
	u32 cur_lv;
	u32 *freq_limit;
};

struct gpu_bootup_pt_priv {
	char freq_limit_booting_name[32];
	u32 max_lv;
	u32 *freq_limit_booting;
};

struct gpu_pt_table {
	char limit_name[32];
	u32 max_tb_num;
	u32 *freq_limit;
};

static struct gpu_pt_priv gpu_pt_info[POWER_THROTTLING_TYPE_MAX] = {
	[LBAT_POWER_THROTTLING] = {
		.max_lv_name = "lbat-max-level",
		.limit_name = "lbat-limit-freq-lv",
		.max_lv = LOW_BATTERY_LEVEL_NUM - 1,
	},
	[OC_POWER_THROTTLING] = {
		.max_lv_name = "oc-max-level",
		.limit_name = "oc-limit-freq-lv",
		.max_lv = BATTERY_OC_LEVEL_NUM - 1,
	},
	[SOC_POWER_THROTTLING] = {
		.max_lv_name = "soc-max-level",
		.limit_name = "soc-limit-freq-lv",
		.max_lv = BATTERY_PERCENT_LEVEL_NUM - 1,
	},
};

static struct gpu_pt_table gpu_pt_tb[GPU_PT_TABLE_MAX]= {
	[GPU_PT_TABLE0] = {
		.limit_name = "lbat-limit-freq-lv",
	},
	[GPU_PT_TABLE1] = {
		.limit_name = "lbat-limit-freq-tb1-lv",
	},
	[GPU_PT_TABLE2] = {
		.limit_name = "lbat-limit-freq-tb2-lv",
	},
	[GPU_PT_TABLE3] = {
		.limit_name = "lbat-limit-freq-tb3-lv",
	},
	[GPU_PT_TABLE4] = {
		.limit_name = "lbat-limit-freq-tb4-lv",
	},
	[GPU_PT_TABLE5] = {
		.limit_name = "lbat-limit-freq-tb5-lv",
	},
};

static struct gpu_bootup_pt_priv gpu_bootup_pt_info = {
	.freq_limit_booting_name = "limit-freq-bootmode",
	.max_lv = 1,
};

#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
static void gpu_pt_low_battery_cb(enum LOW_BATTERY_LEVEL_TAG level, void *data)
{
	s32 freq_limit;

	mutex_lock(&gpu_freq_lock);
	gpu_pt_info[LBAT_POWER_THROTTLING].cur_lv = level;

	if (level > gpu_pt_info[LBAT_POWER_THROTTLING].max_lv) {
		mutex_unlock(&gpu_freq_lock);
		return;
	}

	if (level > LOW_BATTERY_LEVEL_0)
		freq_limit = gpu_pt_info[LBAT_POWER_THROTTLING].freq_limit[level - 1];
	else
		freq_limit = GPUPPM_RESET_IDX;

	trace_low_battery_throttling_gpu_freq(freq_limit);
	gpufreq_set_limit(TARGET_DEFAULT, LIMIT_LOW_BATT, freq_limit, GPUPPM_KEEP_IDX);
	mutex_unlock(&gpu_freq_lock);
}
#endif

static void gpu_bootup_pt_trigger(void)
{
	s32 freq_limit;

	freq_limit = gpu_bootup_pt_info.freq_limit_booting[0];
	gpufreq_set_limit(TARGET_DEFAULT, LIMIT_LOW_BATT, freq_limit, GPUPPM_KEEP_IDX);
}

#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
static void gpu_pt_over_current_cb(enum BATTERY_OC_LEVEL_TAG level, void *data)
{
	s32 freq_limit;

	if (level > gpu_pt_info[OC_POWER_THROTTLING].max_lv)
		return;

	if (level > BATTERY_OC_LEVEL_0)
		freq_limit = gpu_pt_info[OC_POWER_THROTTLING].freq_limit[level - 1];
	else
		freq_limit = GPUPPM_RESET_IDX;

	gpufreq_set_limit(TARGET_DEFAULT, LIMIT_BATT_OC, freq_limit, GPUPPM_KEEP_IDX);
}
#endif

#if IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING)
static void gpu_pt_battery_percent_cb(enum BATTERY_PERCENT_LEVEL_TAG level)
{
	s32 freq_limit;

	if (level > gpu_pt_info[SOC_POWER_THROTTLING].max_lv)
		return;

	if (level > BATTERY_PERCENT_LEVEL_0)
		freq_limit = gpu_pt_info[SOC_POWER_THROTTLING].freq_limit[level- 1];
	else
		freq_limit = GPUPPM_RESET_IDX;

	gpufreq_set_limit(TARGET_DEFAULT, LIMIT_BATT_PERCENT, freq_limit, GPUPPM_KEEP_IDX);
}
#endif

static void __used dump_gpu_setting(struct platform_device *pdev, enum gpu_pt_type type)
{
	struct gpu_pt_priv *gpu_pt_data;
	int i = 0, r = 0;
	char str[128];
	size_t len;

	gpu_pt_data = &gpu_pt_info[type];
	len = sizeof(str) - 1;

	for (i = 0; i < gpu_pt_data->max_lv; i ++) {
		r += snprintf(str + r, len - r, "%d freq ", gpu_pt_data->freq_limit[i]);
		if (r >= len)
			return;
	}
	pr_notice("[%d] %s\n", i, str);
}

static void __used gpu_limit_default_setting(struct device *dev, enum gpu_pt_type type)
{
	struct gpu_pt_priv *gpu_pt_data;
	int i = 0;

	gpu_pt_data = &gpu_pt_info[type];

	if (type == LBAT_POWER_THROTTLING)
		gpu_pt_data->max_lv = LOW_BATTERY_LEVEL_NUM - 1;
	else if (type == OC_POWER_THROTTLING)
		gpu_pt_data->max_lv = 2;
	else
		gpu_pt_data->max_lv = 1;

	gpu_pt_data->freq_limit = kcalloc(gpu_pt_data->max_lv, sizeof(u32), GFP_KERNEL);
	for (i = 0; i < gpu_pt_data->max_lv; i ++)
		gpu_pt_data->freq_limit[i] = GPU_LIMIT_FREQ;
}

static int __used gpu_pt_table_default_setting(enum gpu_pt_table_num num)
{
	struct gpu_pt_table *gpu_pt_data;
	int i = 0;

	gpu_pt_data = &gpu_pt_tb[num];

	gpu_pt_data->freq_limit = kcalloc(gpu_pt_info[LBAT_POWER_THROTTLING].max_lv, sizeof(u32), GFP_KERNEL);
	if (!gpu_pt_data->freq_limit)
		return -ENOMEM;
	for (i = 0; i < gpu_pt_data->max_tb_num; i ++)
		gpu_pt_data->freq_limit[i] = GPU_LIMIT_FREQ;
	return 0;
}

static bool parse_switchpt_table(struct device_node *np)
{
	int i, j, ret = 0;
	struct gpu_pt_table *gpu_pt_table;
	char buf[32];

	for (i = 0; i < lbat_tb_num; i++) {
		gpu_pt_table = &gpu_pt_tb[i];
		gpu_pt_table->max_tb_num = lbat_tb_num;

		gpu_pt_table->freq_limit = kcalloc(gpu_pt_info[LBAT_POWER_THROTTLING].max_lv, sizeof(u32), GFP_KERNEL);
		if (!gpu_pt_table->freq_limit)
			return false;

		for (j = 0; j < gpu_pt_info[LBAT_POWER_THROTTLING].max_lv; j++) {
			memset(buf, 0, sizeof(buf));
			ret = snprintf(buf, sizeof(buf), "%s%d", gpu_pt_table->limit_name, j+1);
			if (ret < 0){
				pr_notice("can't merge %s %d\n", gpu_pt_table->limit_name, j+1);
				kfree(gpu_pt_table->freq_limit);
				gpu_pt_table_default_setting(i);
				return false;
			}
			ret |= of_property_read_u32(np, buf, &gpu_pt_table->freq_limit[j]);
			if (ret < 0){
				pr_notice("%s: get lbat gpu limit fail %d\n", __func__, ret);
				kfree(gpu_pt_table->freq_limit);
				gpu_pt_table_default_setting(i);
				return false;
			}
		}
	}
	return true;
}

static bool parse_bootup_pt_table(struct device_node *np, struct tag_bootmode *tag)
{
	int ret;
	struct gpu_bootup_pt_priv *gpu_bootup_pt_data;
	char buf[32];

	gpu_bootup_pt_data = &gpu_bootup_pt_info;
	gpu_bootup_pt_data->max_lv = 1;
	gpu_bootup_pt_data->freq_limit_booting = kcalloc(gpu_bootup_pt_data->max_lv, sizeof(u32), GFP_KERNEL);
	if (!gpu_bootup_pt_data->freq_limit_booting)
		return false;
	ret = 0;
	memset(buf, 0, sizeof(buf));
	ret = snprintf(buf, sizeof(buf), "%s%d", gpu_bootup_pt_data->freq_limit_booting_name, tag->bootmode);
	if (ret < 0){
		pr_notice("can't merge %s %d\n", gpu_bootup_pt_data->freq_limit_booting_name, tag->bootmode);
		gpu_bootup_pt_data->freq_limit_booting[0] = GPU_LIMIT_FREQ;
		return false;
	}

	ret |= of_property_read_u32(np, buf, &gpu_bootup_pt_data->freq_limit_booting[0]);
	if (ret < 0){
		pr_notice("%s: get gpu bootup limit fail %d\n", __func__, ret);
		gpu_bootup_pt_data->freq_limit_booting[0] = GPU_LIMIT_FREQ;
		return false;
	}
	pr_notice("%s: freq_limit_booting=%d, limit_name=%s\n", __func__,
		gpu_bootup_pt_data->freq_limit_booting[0],
		gpu_bootup_pt_data->freq_limit_booting_name);
	gpu_bootup_pt_trigger();
	pr_notice("%s: bootmode:0x%x\n", __func__, tag->bootmode);
	return true;
}

/*****************************************************************************
 * modify gpu throttle freq
 ******************************************************************************/
static ssize_t modify_gpu_throttle_freq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int len = 0, i, j;
	struct gpu_pt_table *pt_info_p;
	u32 max_tb_num = 0;
	u32 max_lv = 0;

	if (switch_pt == false){
		dev_info(dev, "not support switch_pt\n");
		return -EINVAL;
	}
	max_tb_num = gpu_pt_tb[GPU_PT_TABLE0].max_tb_num;
	max_lv = gpu_pt_info[LBAT_POWER_THROTTLING].max_lv;

	if (max_tb_num > GPU_PT_TABLE_MAX)
		max_tb_num = 1;
	if (max_lv > LOW_BATTERY_LEVEL_NUM - 1)
		max_lv = 3;
	for (i = 0; i < max_tb_num; i++) {
		pt_info_p = &gpu_pt_tb[i];
		len += snprintf(buf + len, PAGE_SIZE - len, "gpu table%d limit freq:", i);

		for (j = 0; j < max_lv; j++)
			len += snprintf(buf + len, PAGE_SIZE - len, " %d", pt_info_p->freq_limit[j]);

		len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	}
	return len;
}

static ssize_t modify_gpu_throttle_freq_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int i, len = 0;
	u32 table_idx;
	u32 freq_limit[LOW_BATTERY_LEVEL_NUM];
	u32 max_lv = 0;
	u32 max_tb_num = 0;

	if (switch_pt == false){
		dev_info(dev, "not support switch_pt\n");
		return -EINVAL;
	}
	max_tb_num = gpu_pt_tb[GPU_PT_TABLE0].max_tb_num;
	max_lv = gpu_pt_info[LBAT_POWER_THROTTLING].max_lv;
	if (sscanf(buf, "%u%n", &table_idx, &len) != 1) {
		dev_info(dev, "Failed to read table_idx\n");
		return -EINVAL;
	}
	buf += len;
	if (table_idx > max_tb_num - 1) {
		dev_info(dev, "Invalid table_idx: %u\n", table_idx);
		return -EINVAL;
	}

	for (i = 0; i < max_lv; i++) {
		if (sscanf(buf, "%u%n", &freq_limit[i], &len) != 1) {
			dev_info(dev, "Failed to read freq_limit[%d]\n", i);
			return -EINVAL;
		}
		buf += len;

		if (freq_limit[i] <= 0 || freq_limit[i] > GPU_LIMIT_FREQ) {
			dev_info(dev, "Invalid freq_limit[%d]: %u\n", i, freq_limit[i]);
			return -EINVAL;
		}
	}
	mutex_lock(&gpu_freq_lock);
	for (i = 0; i < max_lv; i++){
		gpu_pt_tb[table_idx].freq_limit[i] = freq_limit[i];
		dev_notice(dev, "freq_limit[%d]: %d\n", i, gpu_pt_tb[table_idx].freq_limit[i]);
		if (table_idx == gpu_pt_table_idx)
			gpu_pt_info[LBAT_POWER_THROTTLING].freq_limit[i] = gpu_pt_tb[gpu_pt_table_idx].freq_limit[i];
	}
	mutex_unlock(&gpu_freq_lock);

	gpu_pt_low_battery_cb(gpu_pt_info[LBAT_POWER_THROTTLING].cur_lv, NULL);

	return size;
}
static DEVICE_ATTR_RW(modify_gpu_throttle_freq);

static ssize_t gpu_pt_table_idx_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int len = 0;

	if (switch_pt == false){
		dev_info(dev, "not support switch_pt\n");
		return -EINVAL;
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "gpu_pt_table_idx: %d\n", gpu_pt_table_idx);

	return len;
}

static ssize_t gpu_pt_table_idx_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	char cmd[20];
	int j;

	if (switch_pt == false){
		dev_info(dev, "not support switch_pt\n");
		return -EINVAL;
	}
	if ((sscanf(buf, "%9s %u\n", cmd, &gpu_pt_table_idx) != 2) || (strncmp(cmd, "table_idx", 9) != 0)) {
		dev_info(dev, "parameter number not correct\n");
		return -EINVAL;
	}
	if (gpu_pt_table_idx < 0 || gpu_pt_table_idx >= lbat_tb_num) {
		pr_info("Invalid voltage table index.\n");
		return -EINVAL;
	}
	mutex_lock(&gpu_freq_lock);

	for (j = 0; j < gpu_pt_info[LBAT_POWER_THROTTLING].max_lv; j++) {
		gpu_pt_info[LBAT_POWER_THROTTLING].freq_limit[j] = gpu_pt_tb[gpu_pt_table_idx].freq_limit[j];
		pr_notice("%s: freq_limit=%d, limit_name=%s\n", __func__,
			gpu_pt_info[LBAT_POWER_THROTTLING].freq_limit[j],
			gpu_pt_info[LBAT_POWER_THROTTLING].limit_name);
	}
	mutex_unlock(&gpu_freq_lock);

	gpu_pt_low_battery_cb(gpu_pt_info[LBAT_POWER_THROTTLING].cur_lv, NULL);

	return size;
}
static DEVICE_ATTR_RW(gpu_pt_table_idx);

static ssize_t boot_notify_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int len = 0;

	if (bootup_pt_support == false){
		dev_info(dev, "not support gpu bootup\n");
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
	s32 freq_limit;

	if (bootup_pt_support == false){
		dev_info(dev, "not support gpu bootup\n");
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

	freq_limit = GPUPPM_RESET_IDX;
	gpufreq_set_limit(TARGET_DEFAULT, LIMIT_LOW_BATT, freq_limit, GPUPPM_KEEP_IDX);

	return size;
}

static DEVICE_ATTR_RW(boot_notify);

static int mtk_gpu_power_throttling_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *np_bootmode;
	struct gpu_pt_priv *gpu_pt_data;
	struct tag_bootmode *tag;
	int i, j, ret = 0, num = 0;
	char buf[32];
	switch_pt = false;
	bootup_pt_support = false;

	for (i = 0; i < POWER_THROTTLING_TYPE_MAX; i++) {
		gpu_pt_data = &gpu_pt_info[i];
		ret = of_property_read_u32(np, gpu_pt_data->max_lv_name, &num);
		if (ret) {
			gpu_limit_default_setting(&pdev->dev, i);
			continue;
		} else if (num <= 0 || num > gpu_pt_data->max_lv) {
			gpu_pt_data->max_lv = 0;
			continue;
		}

		gpu_pt_data->max_lv = num;
		gpu_pt_data->freq_limit = kcalloc(gpu_pt_data->max_lv, sizeof(u32), GFP_KERNEL);
		if (!gpu_pt_data->freq_limit)
			return -ENOMEM;

		ret = 0;
		for (j = 0; j < gpu_pt_data->max_lv; j++) {
			memset(buf, 0, sizeof(buf));
			ret = snprintf(buf, sizeof(buf), "%s%d", gpu_pt_data->limit_name, j+1);
			if (ret < 0)
				pr_notice("can't merge %s %d\n", gpu_pt_data->limit_name, j+1);

			ret |= of_property_read_u32(np, buf, &gpu_pt_data->freq_limit[j]);
			if (ret < 0)
				pr_notice("%s: get lbat gpu limit fail %d\n", __func__, ret);
		}

		if (ret < 0) {
			kfree(gpu_pt_data->freq_limit);
			gpu_limit_default_setting(&pdev->dev, i);
		} else {
			dump_gpu_setting(pdev, i);
		}
	}
	// gpu pt table switch
	ret = of_property_read_u32(np, "lbat-max-tb-num", &lbat_tb_num);
	if (ret || lbat_tb_num < 0 || lbat_tb_num > GPU_PT_TABLE_MAX) {
		pr_notice("%s: get lbat_tb_num %d fail set to default %d\n", __func__, lbat_tb_num, ret);
		lbat_tb_num = 1;
		switch_pt = false;
	} else {
		switch_pt = parse_switchpt_table(np);
	}

	//gpu bootup pt
	np_bootmode = of_parse_phandle(pdev->dev.of_node, "bootmode", 0);
	if (!np_bootmode){
		pr_notice("%s: get bootmode fail\n", __func__);
		bootup_pt_support = false;
	} else {
		tag = (struct tag_bootmode *)of_get_property(np_bootmode, "atag,boot", NULL);
		if (!tag)
			pr_notice("failed to get atag,boot\n");
		else
			bootup_pt_support = parse_bootup_pt_table (np, tag);
	}

#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
	if (gpu_pt_info[LBAT_POWER_THROTTLING].max_lv > 0)
		register_low_battery_notify(&gpu_pt_low_battery_cb, LOW_BATTERY_PRIO_GPU, NULL);
#endif
#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
	if (gpu_pt_info[OC_POWER_THROTTLING].max_lv > 0)
		register_battery_oc_notify(&gpu_pt_over_current_cb, BATTERY_OC_PRIO_GPU, NULL);
#endif
#if IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING)
	if (gpu_pt_info[SOC_POWER_THROTTLING].max_lv > 0)
		register_bp_thl_notify(&gpu_pt_battery_percent_cb, BATTERY_PERCENT_PRIO_GPU);
#endif
	ret = device_create_file(&(pdev->dev),
		&dev_attr_modify_gpu_throttle_freq);
	if (ret) {
		dev_notice(&pdev->dev, "create file error ret=%d\n", ret);
		return ret;
	}
	ret = device_create_file(&(pdev->dev),
		&dev_attr_gpu_pt_table_idx);
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
	gpu_pt_table_idx = 0;
	system_boot_completed = 0;

	return 0;
}

static const struct of_device_id gpu_power_throttling_of_match[] = {
	{ .compatible = "mediatek,gpu-power-throttling", },
	{},
};

static void mtk_gpu_power_throttling_remove(struct platform_device *pdev)
{

}

MODULE_DEVICE_TABLE(of, gpu_power_throttling_of_match);
static struct platform_driver gpu_power_throttling_driver = {
	.probe = mtk_gpu_power_throttling_probe,
	.remove = mtk_gpu_power_throttling_remove,
	.driver = {
		.name = "mtk-gpu_power_throttling",
		.of_match_table = gpu_power_throttling_of_match,
	},
};
module_platform_driver(gpu_power_throttling_driver);
MODULE_AUTHOR("Victor Lin");
MODULE_DESCRIPTION("MTK gpu power throttling driver");
MODULE_LICENSE("GPL");
