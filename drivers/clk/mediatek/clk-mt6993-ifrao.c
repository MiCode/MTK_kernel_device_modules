// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Kuan-hsin.Lee <kuan-hsin.lee@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6993-clk.h>

#define MT_CCF_BRINGUP         0

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs ifrao_cg_regs = {
	.set_ofs = 0xD24,
	.clr_ofs = 0xD28,
	.sta_ofs = 0xD20,
};

#define GATE_IFRAO(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFRAO_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

static const struct mtk_gate ifrao_clks[] = {
	GATE_IFRAO(IFRAO_DPMAIF_MAIN, "ifrao_dpmaif_main",
			"cksys_dpmaif_main_ck"/* parent */, 2),
	GATE_IFRAO_V(IFRAO_DPMAIF_MAIN_DPMAIF, "ifrao_dpmaif_main_dpmaif",
	        "ifrao_dpmaif_main"/* parent */),
	GATE_IFRAO(IFRAO_DPMAIF_26M, "ifrao_dpmaif_26m",
			"cksys_vlp_f26m_ck"/* parent */, 3),
	GATE_IFRAO_V(IFRAO_DPMAIF_26M_DPMAIF, "ifrao_dpmaif_26m_dpmaif",
	        "ifrao_dpmaif_26m"/* parent */),
};

static const struct mtk_clk_desc ifrao_mcd = {
	.clks = ifrao_clks,
	.num_clks = CLK_IFRAO_NR_CLK,
};

static int clk_mt6993_ifrao_probe(struct platform_device *pdev)
{
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static const struct of_device_id of_match_clk_mt6993_ifrao[] = {
	{
		.compatible = "mediatek,mt6993-apifrbus_ao_mem_reg",
		.data = &ifrao_mcd,
	},
	{}
};

static struct platform_driver clk_mt6993_ifrao_drv = {
	.probe = clk_mt6993_ifrao_probe,
	.driver = {
		.name = "clk-mt6993-ifrao",
		.of_match_table = of_match_clk_mt6993_ifrao,
	},
};

module_platform_driver(clk_mt6993_ifrao_drv);
MODULE_LICENSE("GPL");
