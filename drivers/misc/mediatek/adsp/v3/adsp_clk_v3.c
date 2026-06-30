// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/of.h>
#include "adsp_clk_v3.h"
#include "adsp_core.h"

enum adsp_clk {
	CLK_PD_ADSP_TOP,
	CLK_PD_ADSP_AO,
	ADSP_CLK_NUM
};

struct adsp_clock_attr {
	const char *name;
	struct clk *clock;
};

struct tag_chipid {
	u32 size;
	u32 hw_code;
	u32 hw_subcode;
	u32 hw_ver;
	u32 sw_ver;
};

static struct adsp_clock_attr adsp_clks[ADSP_CLK_NUM] = {
	[CLK_PD_ADSP_TOP] = {"clk_pd_adsp_top", NULL},
	[CLK_PD_ADSP_AO] = {"clk_pd_adsp_ao", NULL},
};

static struct tag_chipid *chip_id;

static int adsp_get_chipid(void)
{
	struct device_node *node;

	if (!chip_id) {
		node = of_find_node_by_path("/chosen");
		if (!node)
			node = of_find_node_by_path("/chosen@0");
		if (node) {
			chip_id = (struct tag_chipid *) of_get_property(node, "atag,chipid", NULL);
			if (!chip_id) {
				pr_info("could not find atag,chipid in chosen\n");
				return -ENODEV;
			}
		} else {
			pr_info("chosen node not found in device tree\n");
			return -ENODEV;
		}
	}

	return chip_id->sw_ver;
}

int adsp_mt_enable_clock(void)
{
	int ret = 0;

	ret = clk_prepare_enable(adsp_clks[CLK_PD_ADSP_TOP].clock);
	if (ret) {
		pr_err("%s(), clk_prepare_enable %s fail, ret %d\n",
			__func__, adsp_clks[CLK_PD_ADSP_TOP].name, ret);
		return -EINVAL;
	}
	pr_debug("%s() done()\n", __func__);

	return ret;
}

void adsp_mt_disable_clock(void)
{
	clk_disable_unprepare(adsp_clks[CLK_PD_ADSP_TOP].clock);
	pr_debug("%s(), done\n", __func__);
}

/* clock init */
int adsp_clk_probe(struct platform_device *pdev, struct adsp_clk_operations *ops)
{
	int ret = 0;

	size_t i;
	struct device *dev = &pdev->dev;

	for (i = 0; i < ARRAY_SIZE(adsp_clks); i++) {
		adsp_clks[i].clock = devm_clk_get(dev, adsp_clks[i].name);
		if (IS_ERR(adsp_clks[i].clock)) {
			ret = PTR_ERR(adsp_clks[i].clock);
			pr_err("%s devm_clk_get %s fail %d\n", __func__,
			       adsp_clks[i].name, ret);
			return ret;
		}
	}

	ops->enable = adsp_mt_enable_clock;
	ops->disable = adsp_mt_disable_clock;
	/* ops->select not supported, PLL mux in adsp */

	/* A0 IC always on ADSP AO to prevent from PLL power leak */
	if (adsp_get_chipid() == 0) {
		ret = clk_prepare_enable(adsp_clks[CLK_PD_ADSP_AO].clock);
		if (ret) {
			pr_info("%s(), clk_prepare_enable %s fail, ret %d\n",
				__func__, adsp_clks[CLK_PD_ADSP_AO].name, ret);
			return -EINVAL;
		}
	}

	return 0;
}

/* clock deinit */
void adsp_clk_remove(void *dev)
{
	pr_debug("%s\n", __func__);
}
