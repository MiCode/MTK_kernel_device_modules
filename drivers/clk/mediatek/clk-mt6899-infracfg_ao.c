// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: KY Liu <ky.liu@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6899-clk.h>

#define MT_CCF_BRINGUP		0

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs infracfg_ao_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x84,
	.sta_ofs = 0x90,
};

#define GATE_INFRACFG_AO(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infracfg_ao_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_INFRACFG_AO_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate infracfg_ao_clks[] = {
	GATE_INFRACFG_AO(CLK_INFRACFG_AO_CCIF1_AP, "infracfg_ao_ccif1_ap",
			"axi_ck"/* parent */, 4),
	GATE_INFRACFG_AO_V(CLK_INFRACFG_AO_CCIF1_AP_CCCI, "infracfg_ao_ccif1_ap_ccci",
			"infracfg_ao_ccif1_ap"/* parent */),
	GATE_INFRACFG_AO(CLK_INFRACFG_AO_CCIF1_MD, "infracfg_ao_ccif1_md",
			"axi_ck"/* parent */, 5),
	GATE_INFRACFG_AO_V(CLK_INFRACFG_AO_CCIF1_MD_CCCI, "infracfg_ao_ccif1_md_ccci",
			"infracfg_ao_ccif1_md"/* parent */),
	GATE_INFRACFG_AO(CLK_INFRACFG_AO_CCIF_AP, "infracfg_ao_ccif_ap",
			"axi_ck"/* parent */, 6),
	GATE_INFRACFG_AO_V(CLK_INFRACFG_AO_CCIF_AP_CCCI, "infracfg_ao_ccif_ap_ccci",
			"infracfg_ao_ccif_ap"/* parent */),
	GATE_INFRACFG_AO(CLK_INFRACFG_AO_CCIF_MD, "infracfg_ao_ccif_md",
			"axi_ck"/* parent */, 8),
	GATE_INFRACFG_AO_V(CLK_INFRACFG_AO_CCIF_MD_CCCI, "infracfg_ao_ccif_md_ccci",
			"infracfg_ao_ccif_md"/* parent */),
	GATE_INFRACFG_AO(CLK_INFRACFG_AO_CLDMA_BCLK, "infracfg_ao_cldmabclk",
			"axi_ck"/* parent */, 12),
	GATE_INFRACFG_AO_V(CLK_INFRACFG_AO_CLDMA_BCLK_DPMAIF, "infracfg_ao_cldmabclk_dpmaif",
			"infracfg_ao_cldmabclk"/* parent */),
	GATE_INFRACFG_AO(CLK_INFRACFG_AO_CCIF5_MD, "infracfg_ao_ccif5_md",
			"axi_ck"/* parent */, 15),
	GATE_INFRACFG_AO_V(CLK_INFRACFG_AO_CCIF5_MD_CCCI, "infracfg_ao_ccif5_md_ccci",
			"infracfg_ao_ccif5_md"/* parent */),
	GATE_INFRACFG_AO(CLK_INFRACFG_AO_CCIF2_AP, "infracfg_ao_ccif2_ap",
			"axi_ck"/* parent */, 16),
	GATE_INFRACFG_AO_V(CLK_INFRACFG_AO_CCIF2_AP_CCCI, "infracfg_ao_ccif2_ap_ccci",
			"infracfg_ao_ccif2_ap"/* parent */),
	GATE_INFRACFG_AO(CLK_INFRACFG_AO_CCIF2_MD, "infracfg_ao_ccif2_md",
			"axi_ck"/* parent */, 17),
	GATE_INFRACFG_AO_V(CLK_INFRACFG_AO_CCIF2_MD_CCCI, "infracfg_ao_ccif2_md_ccci",
			"infracfg_ao_ccif2_md"/* parent */),
	GATE_INFRACFG_AO(CLK_INFRACFG_AO_DPMAIF_MAIN, "infracfg_ao_dpmaif_main",
			"dpmaif_main_ck"/* parent */, 22),
	GATE_INFRACFG_AO_V(CLK_INFRACFG_AO_DPMAIF_MAIN_DPMAIF, "infracfg_ao_dpmaif_main_dpmaif",
			"infracfg_ao_dpmaif_main"/* parent */),
	GATE_INFRACFG_AO(CLK_INFRACFG_AO_CCIF4_MD, "infracfg_ao_ccif4_md",
			"axi_ck"/* parent */, 24),
	GATE_INFRACFG_AO_V(CLK_INFRACFG_AO_CCIF4_MD_CCCI, "infracfg_ao_ccif4_md_ccci",
			"infracfg_ao_ccif4_md"/* parent */),
	GATE_INFRACFG_AO(CLK_INFRACFG_AO_RG_MMW_DPMAIF26M, "infracfg_ao_dpmaif_26m",
			"f26m_ck"/* parent */, 25),
	GATE_INFRACFG_AO_V(CLK_INFRACFG_AO_RG_MMW_DPMAIF26M_DPMAIF, "infracfg_ao_dpmaif_26m_dpmaif",
			"infracfg_ao_dpmaif_26m"/* parent */),
};

static const struct mtk_clk_desc infracfg_ao_mcd = {
	.clks = infracfg_ao_clks,
	.num_clks = CLK_INFRACFG_AO_NR_CLK,
};

static int clk_mt6899_infracfg_ao_probe(struct platform_device *pdev)
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

static const struct of_device_id of_match_clk_mt6899_infracfg_ao[] = {
	{
		.compatible = "mediatek,mt6899-infra_infracfg_ao_reg",
		.data = &infracfg_ao_mcd,
	},
	{}
};

static struct platform_driver clk_mt6899_infracfg_ao_drv = {
	.probe = clk_mt6899_infracfg_ao_probe,
	.driver = {
		.name = "clk-mt6899-infracfg_ao",
		.of_match_table = of_match_clk_mt6899_infracfg_ao,
	},
};

module_platform_driver(clk_mt6899_infracfg_ao_drv);
MODULE_LICENSE("GPL");
