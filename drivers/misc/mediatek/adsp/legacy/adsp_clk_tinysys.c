// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/clk.h>
#include "adsp_clk_tinysys.h"

enum adsp_clk {
	CLK_ADSP_INFRA_26M,
	CLK_ADSP_INFRA_32K,
	CLK_ADSP_INFRA,
	CLK_TOP_ADSP_SEL,
	CLK_TOP_CLK26M,
	CLK_TOP_ADSPPLL,
	ADSP_CLK_NUM
};

struct adsp_clock_attr {
	const char *name;
	struct clk *clock;
};

static struct device *pm_dev;

static struct adsp_clock_attr adsp_clks[ADSP_CLK_NUM] = {
	[CLK_ADSP_INFRA] = {"clk_adsp_infra", NULL},
	[CLK_ADSP_INFRA_26M] = {"clk_adsp_infra_26m", NULL},
	[CLK_ADSP_INFRA_32K] = {"clk_adsp_infra_32k", NULL},
	[CLK_TOP_ADSP_SEL] = {"clk_top_adsp_sel", NULL},
	[CLK_TOP_CLK26M] = {"clk_adsp_clk26m", NULL},
	[CLK_TOP_ADSPPLL] = {"clk_top_adsppll_ck", NULL},
};

static int adsp_set_top_mux(enum adsp_clk clk)
{
	int ret = 0;

	pr_debug("%s(%x)\n", __func__, clk);

	if (clk >= ADSP_CLK_NUM)
		return -EINVAL;

	ret = clk_set_parent(adsp_clks[CLK_TOP_ADSP_SEL].clock,
		      adsp_clks[clk].clock);
	if (IS_ERR(&ret))
		pr_info("[ADSP] %s clk_set_parent(clk_top_adsp_sel-%x) fail %d\n",
		      __func__, clk, ret);
	return ret;
}

void adsp_mt_select_clock_mode(enum adsp_clk_mode mode)
{
	switch (mode) {
	case CLK_LOW_POWER:
		adsp_set_top_mux(CLK_TOP_CLK26M);
		break;
	case CLK_HIGH_PERFORM:
	case CLK_DEFAULT_INIT:
		adsp_set_top_mux(CLK_TOP_ADSPPLL);
		break;
	default:
		break;
	}
}

int adsp_mt_enable_clock(void)
{
	int ret = 0;

	pr_debug("%s() ret %d\n", __func__, ret);

	ret = clk_prepare_enable(adsp_clks[CLK_ADSP_INFRA_26M].clock);
	if (IS_ERR(&ret)) {
		pr_info("%s(), clk_prepare_enable %s fail, ret %d\n",
			__func__, adsp_clks[CLK_ADSP_INFRA_26M].name, ret);
		return -EINVAL;
	}

	ret = clk_prepare_enable(adsp_clks[CLK_ADSP_INFRA_32K].clock);
	if (IS_ERR(&ret)) {
		pr_info("%s(), clk_prepare_enable %s fail, ret %d\n",
			__func__, adsp_clks[CLK_ADSP_INFRA_32K].name, ret);
		clk_disable_unprepare(adsp_clks[CLK_ADSP_INFRA_26M].clock);
		return -EINVAL;
	}

	ret = clk_prepare_enable(adsp_clks[CLK_ADSP_INFRA].clock);
	if (IS_ERR(&ret)) {
		pr_info("%s(), clk_prepare_enable %s fail, ret %d\n",
			__func__, adsp_clks[CLK_ADSP_INFRA].name, ret);
		clk_disable_unprepare(adsp_clks[CLK_ADSP_INFRA_32K].clock);
		clk_disable_unprepare(adsp_clks[CLK_ADSP_INFRA_26M].clock);
		return -EINVAL;
	}

	return 0;
}

void adsp_mt_disable_clock(void)
{
	pr_debug("%s() pm_dev %p\n", __func__, pm_dev);

	clk_disable_unprepare(adsp_clks[CLK_ADSP_INFRA].clock);
		clk_disable_unprepare(adsp_clks[CLK_ADSP_INFRA_32K].clock);
	clk_disable_unprepare(adsp_clks[CLK_ADSP_INFRA_26M].clock);
}

/* clock init */
int adsp_clk_probe(struct platform_device *pdev,
			  struct adsp_clk_operations *ops)
{
	int ret = 0;
	size_t i;
	struct device *dev = &pdev->dev;

	for (i = 0; i < ARRAY_SIZE(adsp_clks); i++) {
		adsp_clks[i].clock = devm_clk_get(dev, adsp_clks[i].name);
		if (IS_ERR(adsp_clks[i].clock)) {
			ret = PTR_ERR(adsp_clks[i].clock);
			pr_info("%s devm_clk_get %s fail %d\n", __func__,
				   adsp_clks[i].name, ret);
		}
	}

	pm_dev = &pdev->dev;

	ops->enable = adsp_mt_enable_clock;
	ops->disable = adsp_mt_disable_clock;
	ops->select = adsp_mt_select_clock_mode;

	return ret;
}

/* clock deinit */
void adsp_clk_remove(void *dev)
{
	pr_debug("%s dev %p\n", __func__, dev);
}
