// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Victor Lin <Victor-wc.lin@mediatek.com>
 */
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include "mtk_apu_power_throttling.h"
#include "mtk_battery_oc_throttling.h"
#include "mtk_low_battery_throttling.h"
#include "mtk_bp_thl.h"

#define APU_LIMIT_OPP0 0

static DEFINE_MUTEX(apu_freq_lock);

static bool pt_apu_drv_inited = 0;
static const char *efuse_field = "fab_info";
static bool switch_pt;
static int lbat_tb_num;
static int apu_pt_table_idx;

enum apu_pt_table_num {
	APU_PT_TABLE0,
	APU_PT_TABLE1,
	APU_PT_TABLE2,
	APU_PT_TABLE3,
	APU_PT_TABLE4,
	APU_PT_TABLE5,
	APU_PT_TABLE_MAX
};

struct apu_pt_priv {
	char max_lv_name[32];
	char limit_name[32];
	u32 max_lv;
	u32 *opp_limit;
	u32 cur_lv;
	apu_throttle_callback cb;
};

struct apu_pt_table {
	char limit_name[32];
	u32 *opp_limit;
};

static struct apu_pt_priv apu_pt_info[POWER_THROTTLING_TYPE_MAX] = {
	[LBAT_POWER_THROTTLING] = {
		.max_lv_name = "lbat-max-level",
		.limit_name = "lbat-limit-opp-lv",
		.max_lv = LOW_BATTERY_LEVEL_NUM - 1,
	},
	[OC_POWER_THROTTLING] = {
		.max_lv_name = "oc-max-level",
		.limit_name = "oc-limit-opp-lv",
		.max_lv = BATTERY_OC_LEVEL_NUM - 1,
	},
	[SOC_POWER_THROTTLING] = {
		.max_lv_name = "soc-max-level",
		.limit_name = "soc-limit-opp-lv",
		.max_lv = BATTERY_PERCENT_LEVEL_NUM - 1,
	}
};

static struct apu_pt_table apu_pt_tb[APU_PT_TABLE_MAX]= {
	[APU_PT_TABLE0] = {
		.limit_name = "lbat-limit-opp-lv",
	},
	[APU_PT_TABLE1] = {
		.limit_name = "lbat-limit-opp-tb1-lv",
	},
	[APU_PT_TABLE2] = {
		.limit_name = "lbat-limit-opp-tb2-lv",
	},
	[APU_PT_TABLE3] = {
		.limit_name = "lbat-limit-opp-tb3-lv",
	},
	[APU_PT_TABLE4] = {
		.limit_name = "lbat-limit-opp-tb4-lv",
	},
	[APU_PT_TABLE5] = {
		.limit_name = "lbat-limit-opp-tb5-lv",
	},
};

#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
static void apu_pt_low_battery_cb(enum LOW_BATTERY_LEVEL_TAG level, void *data)
{
	int ret = 0, id = 1;
	int opp_limit;

	apu_pt_info[LBAT_POWER_THROTTLING].cur_lv = level;
	if (!pt_apu_drv_inited)
		return;

	if (level > apu_pt_info[LBAT_POWER_THROTTLING].max_lv)
		return;

	if (!apu_pt_info[LBAT_POWER_THROTTLING].cb)
		return;

	if (level > LOW_BATTERY_LEVEL_0)
		opp_limit = apu_pt_info[LBAT_POWER_THROTTLING].opp_limit[level - 1];
	else
		opp_limit = APU_LIMIT_OPP0;

	ret = apu_pt_info[LBAT_POWER_THROTTLING].cb(&id, opp_limit);
	if (ret)
		pr_notice("[%s] apu pt low battery throttle failed:%d\n", __func__, ret);
}
#endif

#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
static void apu_pt_over_current_cb(enum BATTERY_OC_LEVEL_TAG level, void *data)
{
	int ret = 0, id = 2;
	int opp_limit;

	if (!pt_apu_drv_inited)
		return;

	if (level > apu_pt_info[OC_POWER_THROTTLING].max_lv)
		return;

	if (!apu_pt_info[OC_POWER_THROTTLING].cb)
		return;

	if (level > BATTERY_OC_LEVEL_0)
		opp_limit = apu_pt_info[OC_POWER_THROTTLING].opp_limit[level - 1];
	else
		opp_limit = APU_LIMIT_OPP0;

	ret = apu_pt_info[LBAT_POWER_THROTTLING].cb(&id, opp_limit);
	if (ret)
		pr_notice("[%s] apu pt low battery throttle failed:%d\n", __func__, ret);
}
#endif

#if IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING)
static void apu_pt_battery_percent_cb(enum BATTERY_PERCENT_LEVEL_TAG level)
{
	int ret = 0, id = 3;
	int opp_limit;

	if (!pt_apu_drv_inited)
		return;

	if (level > apu_pt_info[SOC_POWER_THROTTLING].max_lv)
		return;

	if (!apu_pt_info[SOC_POWER_THROTTLING].cb)
		return;

	if (level > BATTERY_PERCENT_LEVEL_0)
		opp_limit = apu_pt_info[SOC_POWER_THROTTLING].opp_limit[level- 1];
	else
		opp_limit = APU_LIMIT_OPP0;

	ret = apu_pt_info[LBAT_POWER_THROTTLING].cb(&id, opp_limit);
	if (ret)
		pr_notice("[%s] apu pt low battery throttle failed:%d\n", __func__, ret);
}
#endif

int register_pt_low_battery_apu_cb(apu_throttle_callback cb)
{
	if (!pt_apu_drv_inited)
		return -ENODEV;

	if (!cb)
		return -EINVAL;

	if (apu_pt_info[LBAT_POWER_THROTTLING].cb)
		return -EEXIST;

	apu_pt_info[LBAT_POWER_THROTTLING].cb = cb;

	if (apu_pt_info[LBAT_POWER_THROTTLING].max_lv > 0)
#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
		register_low_battery_notify(&apu_pt_low_battery_cb, LOW_BATTERY_PRIO_APU, NULL);
#endif
	return 0;
}
EXPORT_SYMBOL(register_pt_low_battery_apu_cb);

int register_pt_over_current_apu_cb(apu_throttle_callback cb)
{
	if (!pt_apu_drv_inited)
		return -ENODEV;

	if (!cb)
		return -EINVAL;

	if (apu_pt_info[OC_POWER_THROTTLING].cb)
		return -EEXIST;

	apu_pt_info[OC_POWER_THROTTLING].cb = cb;

	if (apu_pt_info[OC_POWER_THROTTLING].max_lv > 0)
#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
		register_battery_oc_notify(&apu_pt_over_current_cb, BATTERY_OC_PRIO_APU, NULL);
#endif

	return 0;
}
EXPORT_SYMBOL(register_pt_over_current_apu_cb);

int register_pt_battery_percent_apu_cb(apu_throttle_callback cb)
{
	if (!pt_apu_drv_inited)
		return -ENODEV;

	if (!cb)
		return -EINVAL;

	if (apu_pt_info[SOC_POWER_THROTTLING].cb)
		return -EEXIST;

	apu_pt_info[SOC_POWER_THROTTLING].cb = cb;

	if (apu_pt_info[SOC_POWER_THROTTLING].max_lv > 0)
#if IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING)
		register_bp_thl_notify(&apu_pt_battery_percent_cb, BATTERY_PERCENT_PRIO_APU);
#endif

	return 0;
}
EXPORT_SYMBOL(register_pt_battery_percent_apu_cb);

static void __used dump_apu_setting(struct platform_device *pdev, enum apu_pt_type type)
{
	struct apu_pt_priv *apu_pt_data;
	int i = 0, r = 0;
	char str[128];
	size_t len;

	apu_pt_data = &apu_pt_info[type];
	len = sizeof(str) - 1;

	for (i = 0; i < apu_pt_data->max_lv; i ++) {
		r += snprintf(str + r, len - r, "%d OPP ", apu_pt_data->opp_limit[i]);
		if (r >= len)
			return;
	}
	pr_notice("[%s] type:%d, %s\n", __func__, type, str);
}

static int __used apu_limit_default_setting(struct device *dev, enum apu_pt_type type)
{
	struct apu_pt_priv *apu_pt_data;
	int i = 0;

	apu_pt_data = &apu_pt_info[type];

	if (type == LBAT_POWER_THROTTLING)
		apu_pt_data->max_lv = LOW_BATTERY_LEVEL_NUM - 1;
	else if (type == OC_POWER_THROTTLING)
		apu_pt_data->max_lv = 2;
	else
		apu_pt_data->max_lv = 1;

	apu_pt_data->opp_limit = kcalloc(apu_pt_data->max_lv, sizeof(u32), GFP_KERNEL);
	if (!apu_pt_data->opp_limit)
		return -ENOMEM;
	for (i = 0; i < apu_pt_data->max_lv; i ++)
		apu_pt_data->opp_limit[i] = APU_LIMIT_OPP0;

	return 0;
}

static bool parse_switchpt_table(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int i, j, ret = 0;
	struct apu_pt_table *apu_pt_table;
	char buf[32];

	for (i = 0; i < lbat_tb_num; i++) {
		apu_pt_table = &apu_pt_tb[i];

		apu_pt_table->opp_limit = kcalloc(apu_pt_info[LBAT_POWER_THROTTLING].max_lv, sizeof(u32), GFP_KERNEL);
		if (!apu_pt_table->opp_limit)
			return false;

		for (j = 0; j < apu_pt_info[LBAT_POWER_THROTTLING].max_lv; j++) {
			memset(buf, 0, sizeof(buf));
			ret = snprintf(buf, sizeof(buf), "%s%d", apu_pt_table->limit_name, j+1);
			if (ret < 0){
				pr_notice("can't merge %s %d\n", apu_pt_table->limit_name, j+1);
				kfree(apu_pt_table->opp_limit);
				return false;
			}
			ret |= of_property_read_u32(np, buf, &apu_pt_table->opp_limit[j]);
			if (ret < 0){
				pr_notice("%s: get lbat apu limit fail %d\n", __func__, ret);
				kfree(apu_pt_table->opp_limit);
				return false;
			}
		}
	}
	return true;
}

static int parse_dts(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct apu_pt_priv *apu_pt_data;
	int i, j, ret = 0, num = 0;
	char buf[32];

	for (i = 0; i < POWER_THROTTLING_TYPE_MAX; i++) {
		apu_pt_data = &apu_pt_info[i];
		ret = of_property_read_u32(np, apu_pt_data->max_lv_name, &num);
		if (ret) {
			apu_limit_default_setting(&pdev->dev, i);
			continue;
		} else if (num <= 0 || num > apu_pt_data->max_lv) {
			apu_pt_data->max_lv = 0;
			continue;
		}

		apu_pt_data->max_lv = num;
		apu_pt_data->opp_limit = kcalloc(apu_pt_data->max_lv, sizeof(u32), GFP_KERNEL);
		if (!apu_pt_data->opp_limit)
			return -ENOMEM;

		ret = 0;
		for (j = 0; j < apu_pt_data->max_lv; j++) {
			memset(buf, 0, sizeof(buf));
			ret = snprintf(buf, sizeof(buf), "%s%d", apu_pt_data->limit_name, j+1);
			if (ret < 0)
				pr_notice("can't merge %s %d\n", apu_pt_data->limit_name, j+1);

			ret |= of_property_read_u32(np, buf, &apu_pt_data->opp_limit[j]);
			if (ret < 0)
				pr_notice("%s: get lbat apu limit fail %d\n", __func__, ret);
		}

		if (ret < 0) {
			kfree(apu_pt_data->opp_limit);
			apu_limit_default_setting(&pdev->dev, i);
		} else {
			dump_apu_setting(pdev, i);
		}
	}

	return 0;
}

static ssize_t apu_pt_table_idx_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int len = 0;

	if (switch_pt == false){
		dev_info(dev, "not support switch_pt\n");
		return -EINVAL;
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "apu_pt_table_idx: %d\n", apu_pt_table_idx);

	return len;
}

static ssize_t apu_pt_table_idx_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	char cmd[20];
	int j;

	if (switch_pt == false){
		dev_info(dev, "not support switch_pt\n");
		return -EINVAL;
	}
	if ((sscanf(buf, "%9s %u\n", cmd, &apu_pt_table_idx) != 2) || (strncmp(cmd, "table_idx", 9) != 0)) {
		dev_info(dev, "parameter number not correct\n");
		return -EINVAL;
	}
	if (apu_pt_table_idx < 0 || apu_pt_table_idx >= lbat_tb_num) {
		pr_info("Invalid voltage table index.\n");
		return -EINVAL;
	}
	mutex_lock(&apu_freq_lock);

	for (j = 0; j < apu_pt_info[LBAT_POWER_THROTTLING].max_lv; j++) {
		apu_pt_info[LBAT_POWER_THROTTLING].opp_limit[j] = apu_pt_tb[apu_pt_table_idx].opp_limit[j];
		pr_notice("%s: opp_limit=%d, apu_pt_table_idx=%d\n", __func__,
			apu_pt_info[LBAT_POWER_THROTTLING].opp_limit[j],
			apu_pt_table_idx);
	}
	mutex_unlock(&apu_freq_lock);

	apu_pt_low_battery_cb(apu_pt_info[LBAT_POWER_THROTTLING].cur_lv, NULL);

	return size;
}
static DEVICE_ATTR_RW(apu_pt_table_idx);

static int mtk_apu_power_throttling_probe(struct platform_device *pdev)
{
	int ret = 0;
	size_t len;
	struct device_node *es_np;
	struct nvmem_cell *cell;
	u32 *nvmem_buf, value;

	cell = nvmem_cell_get(&pdev->dev, efuse_field);
	if (!IS_ERR(cell)) {
		nvmem_buf = (u32 *)nvmem_cell_read(cell, &len);
		nvmem_cell_put(cell);
		if (!IS_ERR(nvmem_buf)) {
			value = *nvmem_buf;
			pr_info("[%s]:fab_value = %u", __func__, value);
			if (value == 0) {
				es_np = of_find_compatible_node(NULL, NULL, "mediatek,es-apu-power-throttling");
				if (es_np != NULL)
					pdev->dev.of_node = es_np;
				else
					pr_info("[%s]:es_np is NULL", __func__);
			}
			kfree(nvmem_buf);
		} else
			pr_info ("[%s]:get fab_info failed", __func__);
	}

	ret = parse_dts(pdev);
	if (ret) {
		pr_notice("%s:%d parse dts fail!\n", __func__, __LINE__);
		return ret;
	}
	pt_apu_drv_inited = 1;

	// apu pt table switch
	ret = of_property_read_u32(pdev->dev.of_node, "lbat-max-tb-num", &lbat_tb_num);
	if (ret || lbat_tb_num < 0 || lbat_tb_num > APU_PT_TABLE_MAX) {
		pr_notice("%s: get lbat_tb_num %d fail set to default %d\n", __func__, lbat_tb_num, ret);
		lbat_tb_num = 1;
		switch_pt = false;
	} else {
		switch_pt = parse_switchpt_table(pdev);
	}

	ret = device_create_file(&(pdev->dev),
		&dev_attr_apu_pt_table_idx);
	if (ret) {
		dev_notice(&pdev->dev, "create file error ret=%d\n", ret);
		return ret;
	}

	return 0;
}

static const struct of_device_id apu_power_throttling_of_match[] = {
	{ .compatible = "mediatek,apu-power-throttling", },
	{},
};


MODULE_DEVICE_TABLE(of, apu_power_throttling_of_match);
static struct platform_driver apu_power_throttling_driver = {
	.probe = mtk_apu_power_throttling_probe,
	.driver = {
		.name = "mtk-apu_power_throttling",
		.of_match_table = apu_power_throttling_of_match,
	},
};
module_platform_driver(apu_power_throttling_driver);
MODULE_AUTHOR("Victor Lin");
MODULE_DESCRIPTION("MTK apu power throttling driver");
MODULE_LICENSE("GPL");
