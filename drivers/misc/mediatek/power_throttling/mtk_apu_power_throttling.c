// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Victor Lin <Victor-wc.lin@mediatek.com>
 */
#include <linux/of.h>
#include <linux/platform_device.h>
#include "mtk_apu_power_throttling.h"
#include "mtk_battery_oc_throttling.h"
#include "mtk_low_battery_throttling.h"
#include "mtk_bp_thl.h"

#define APU_LIMIT_OPP0 0

static bool pt_apu_drv_inited = 0;

struct apu_pt_priv {
	char max_lv_name[32];
	char limit_name[32];
	u32 max_lv;
	u32 *opp_limit;
	apu_throttle_callback cb;
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

#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
static void apu_pt_low_battery_cb(enum LOW_BATTERY_LEVEL_TAG level, void *data)
{
	int ret = 0, id = 1;
	int opp_limit;

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


static int mtk_apu_power_throttling_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = parse_dts(pdev);
	if (ret) {
		pr_notice("%s:%d parse dts fail!\n", __func__, __LINE__);
		return ret;
	}
	pt_apu_drv_inited = 1;
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
