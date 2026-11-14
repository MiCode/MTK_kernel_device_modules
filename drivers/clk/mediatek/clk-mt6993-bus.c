// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Kuan-hsin.Lee <kuan-hsin.lee@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6993-clk.h>

#define MT_CCF_BRINGUP         0

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs ssr_top0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs ssr_top1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x4,
	.sta_ofs = 0x4,
};

#define GATE_SSR_TOP0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ssr_top0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_SSR_TOP0_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_SSR_TOP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ssr_top1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_SSR_TOP1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

static const struct mtk_gate ssr_top_clks[] = {
	/* SSR_TOP0 */
	GATE_SSR_TOP0(SSR_TOP_RG_RW_SSR_RNG_CK_EN, "ssr_rw_ssr_rng",
			"cksys_ssr_rng_ck"/* parent */, 0),
	GATE_SSR_TOP0_V(SSR_TOP_RG_RW_SSR_RNG_CK_EN_V, "ssr_rw_ssr_rng_v",
	        "ssr_rw_ssr_rng"/* parent */),
	GATE_SSR_TOP0(SSR_TOP_RQ_RW_SSR_DMA_CK_EN, "ssr_rq_rw_ssr_dma",
			"cksys_ssr_dma_ck"/* parent */, 8),
	GATE_SSR_TOP0_V(SSR_TOP_RQ_RW_SSR_DMA_CK_EN_V, "ssr_rq_rw_ssr_dma_v",
	        "ssr_rq_rw_ssr_dma"/* parent */),
	GATE_SSR_TOP0(SSR_TOP_RQ_RW_SSR_KDF_CK_EN, "ssr_rq_rw_ssr_kdf",
			"cksys_ssr_kdf_ck"/* parent */, 16),
	GATE_SSR_TOP0_V(SSR_TOP_RQ_RW_SSR_KDF_CK_EN_V, "ssr_rq_rw_ssr_kdf_v",
	        "ssr_rq_rw_ssr_kdf"/* parent */),
	GATE_SSR_TOP0(SSR_TOP_RQ_RW_SSR_PKA_CK_EN, "ssr_rq_rw_ssr_pka",
			"cksys_ssr_pka_ck"/* parent */, 24),
	GATE_SSR_TOP0_V(SSR_TOP_RQ_RW_SSR_PKA_CK_EN_V, "ssr_rq_rw_ssr_pka_v",
	        "ssr_rq_rw_ssr_pka"/* parent */),
	/* SSR_TOP1 */
	GATE_SSR_TOP1(SSR_TOP_RG_RW_SPU_CK_EN, "ssr_rw_spu",
			"cksys_vlp_spu0_vlp_ck"/* parent */, 0),
	GATE_SSR_TOP1_V(SSR_TOP_RG_RW_SPU_CK_EN_V, "ssr_rw_spu_v",
	        "ssr_rw_spu"/* parent */),
};

static const struct mtk_clk_desc ssr_top_mcd = {
	.clks = ssr_top_clks,
	.num_clks = CLK_SSR_TOP_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6993_bus[] = {
	{
		.compatible = "mediatek,mt6993-ssr_top",
		.data = &ssr_top_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6993_bus_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6993_bus_drv = {
	.probe = clk_mt6993_bus_grp_probe,
	.driver = {
		.name = "clk-mt6993-bus",
		.of_match_table = of_match_clk_mt6993_bus,
	},
};

module_platform_driver(clk_mt6993_bus_drv);
MODULE_LICENSE("GPL");
