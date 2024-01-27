// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Chong-ming Wei <chong-ming.wei@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6991-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs ifr_mem_cg_regs = {
	.set_ofs = 0xD04,
	.clr_ofs = 0xD08,
	.sta_ofs = 0xD00,
};

#define GATE_IFR_MEM(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifr_mem_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFR_MEM_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate ifr_mem_clks[] = {
	GATE_IFR_MEM(CLK_IFR_MEM_DPMAIF_MAIN, "ifr_mem_dpmaif_main",
		"ck_dpmaif_main_ck"/* parent */, 2),
	GATE_IFR_MEM_V(CLK_IFR_MEM_DPMAIF_MAIN_CCCI, "ifr_mem_dpmaif_main_ccci",
		"ifr_mem_dpmaif_main"/* parent */),
	GATE_IFR_MEM(CLK_IFR_MEM_DPMAIF_26M, "ifr_mem_dpmaif_26m",
		"vlp_infra_26m_ck"/* parent */, 3),
	GATE_IFR_MEM_V(CLK_IFR_MEM_DPMAIF_26M_CCCI, "ifr_mem_dpmaif_26m_ccci",
		"ifr_mem_dpmaif_26m"/* parent */),
};

static const struct mtk_clk_desc ifr_mem_mcd = {
	.clks = ifr_mem_clks,
	.num_clks = CLK_IFR_MEM_NR_CLK,
};

static const struct mtk_gate_regs ssr_top0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x4,
	.sta_ofs = 0x4,
};

static const struct mtk_gate_regs ssr_top1_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0x8,
	.sta_ofs = 0x8,
};

#define GATE_SSR_TOP0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ssr_top0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_SSR_TOP0_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_SSR_TOP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ssr_top1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_SSR_TOP1_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate ssr_top_clks[] = {
	/* SSR_TOP0 */
	GATE_SSR_TOP0(CLK_SSR_TOP_RG_RW_SPCK_EN, "ssr_rw_spu",
		"ck_spu0_ck"/* parent */, 0),
	GATE_SSR_TOP0_V(CLK_SSR_TOP_RG_RW_SPCK_EN_SPU, "ssr_rw_spu_spu",
		"ssr_rw_spu"/* parent */),
	GATE_SSR_TOP0(CLK_SSR_TOP_RG_RW_SEJ_BCLK_TEST_CK_EN, "ssr_rw_sej_b_test",
		"ck_ssr_rng_ck"/* parent */, 16),
	GATE_SSR_TOP0_V(CLK_SSR_TOP_RG_RW_SEJ_BCLK_TEST_CK_EN_SEJ, "ssr_rw_sej_b_test_sej",
		"ssr_rw_sej_b_test"/* parent */),
	GATE_SSR_TOP0(CLK_SSR_TOP_RG_RW_AES_TOP_BCLK_TEST_CK_EN, "ssr_rw_aes_b_test",
		"ck_ssr_rng_ck"/* parent */, 24),
	GATE_SSR_TOP0_V(CLK_SSR_TOP_RG_RW_AES_TOP_BCLK_TEST_CK_EN_AESTOP, "ssr_rw_aes_b_test_aestop",
		"ssr_rw_aes_b_test"/* parent */),
	/* SSR_TOP1 */
	GATE_SSR_TOP1(CLK_SSR_TOP_DXCC_PTCK, "ssr_DXCC_PTCK",
		"ck_dxcc_ck"/* parent */, 1),
	GATE_SSR_TOP1_V(CLK_SSR_TOP_DXCC_PTCK_SEJ, "ssr_dxcc_ptck_sej",
		"ssr_DXCC_PTCK"/* parent */),
	GATE_SSR_TOP1(CLK_SSR_TOP_RG_RW_SEJ_F13M_TEST_CK_EN, "ssr_rw_sej_f13m_test",
		"vlp_infra_26m_ck"/* parent */, 2),
	GATE_SSR_TOP1_V(CLK_SSR_TOP_RG_RW_SEJ_F13M_TEST_CK_EN_SEJ, "ssr_rw_sej_f13m_test_sej",
		"ssr_rw_sej_f13m_test"/* parent */),
};

static const struct mtk_clk_desc ssr_top_mcd = {
	.clks = ssr_top_clks,
	.num_clks = CLK_SSR_TOP_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6991_bus[] = {
	{
		.compatible = "mediatek,mt6991-apifrbus_ao_mem_reg",
		.data = &ifr_mem_mcd,
	}, {
		.compatible = "mediatek,mt6991-ssr_top",
		.data = &ssr_top_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6991_bus_grp_probe(struct platform_device *pdev)
{
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init begin\n", __func__, pdev->name);
#endif

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init end\n", __func__, pdev->name);
#endif

	return r;
}

static struct platform_driver clk_mt6991_bus_drv = {
	.probe = clk_mt6991_bus_grp_probe,
	.driver = {
		.name = "clk-mt6991-bus",
		.of_match_table = of_match_clk_mt6991_bus,
	},
};

module_platform_driver(clk_mt6991_bus_drv);
MODULE_LICENSE("GPL");
