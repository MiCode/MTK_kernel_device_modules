// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: KY Liu <ky.liu@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6899-clk.h>

#define MT_CCF_BRINGUP		0

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs vde20_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vde20_hwv_regs = {
	.set_ofs = 0x0048,
	.clr_ofs = 0x004C,
	.sta_ofs = 0x1C24,
};

static const struct mtk_gate_regs vde21_cg_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x204,
	.sta_ofs = 0x200,
};

static const struct mtk_gate_regs vde21_hwv_regs = {
	.set_ofs = 0x0040,
	.clr_ofs = 0x0044,
	.sta_ofs = 0x1C20,
};

#define GATE_VDE20(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde20_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE20_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

#define GATE_HWV_VDE20(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &vde20_cg_regs,			\
		.hwv_regs = &vde20_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

#define GATE_VDE21(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde21_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE21_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

#define GATE_HWV_VDE21(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &vde21_cg_regs,			\
		.hwv_regs = &vde21_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate vde2_clks[] = {
	/* VDE20 */
	GATE_HWV_VDE20(CLK_VDE2_VDEC_CKEN, "vde2_vdec_cken",
			"vdec_ck"/* parent */, 0),
	GATE_VDE20_V(CLK_VDE2_VDEC_CKEN_VDEC, "vde2_vdec_cken_vdec",
			"vde2_vdec_cken"/* parent */),
	GATE_HWV_VDE20(CLK_VDE2_VDEC_ACTIVE, "vde2_vdec_active",
			"vdec_ck"/* parent */, 4),
	GATE_VDE20_V(CLK_VDE2_VDEC_ACTIVE_VDEC, "vde2_vdec_active_vdec",
			"vde2_vdec_active"/* parent */),
	/* VDE21 */
	GATE_HWV_VDE21(CLK_VDE2_LAT_CKEN, "vde2_lat_cken",
			"vdec_ck"/* parent */, 0),
	GATE_VDE21_V(CLK_VDE2_LAT_CKEN_VDEC, "vde2_lat_cken_vdec",
			"vde2_lat_cken"/* parent */),
};

static const struct mtk_clk_desc vde2_mcd = {
	.clks = vde2_clks,
	.num_clks = CLK_VDE2_NR_CLK,
};

static const struct mtk_gate_regs vde10_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vde10_hwv_regs = {
	.set_ofs = 0x0060,
	.clr_ofs = 0x0064,
	.sta_ofs = 0x1C30,
};

static const struct mtk_gate_regs vde11_cg_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x204,
	.sta_ofs = 0x200,
};

static const struct mtk_gate_regs vde11_hwv_regs = {
	.set_ofs = 0x0058,
	.clr_ofs = 0x005C,
	.sta_ofs = 0x1C2C,
};

#define GATE_VDE10(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE10_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

#define GATE_HWV_VDE10(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &vde10_cg_regs,			\
		.hwv_regs = &vde10_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

#define GATE_VDE11(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde11_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE11_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

#define GATE_HWV_VDE11(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &vde11_cg_regs,			\
		.hwv_regs = &vde11_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate vde1_clks[] = {
	/* VDE10 */
	GATE_HWV_VDE10(CLK_VDE1_VDEC_CKEN, "vde1_vdec_cken",
			"vdec_ck"/* parent */, 0),
	GATE_VDE10_V(CLK_VDE1_VDEC_CKEN_VDEC, "vde1_vdec_cken_vdec",
			"vde1_vdec_cken"/* parent */),
	GATE_HWV_VDE10(CLK_VDE1_VDEC_ACTIVE, "vde1_vdec_active",
			"vdec_ck"/* parent */, 4),
	GATE_VDE10_V(CLK_VDE1_VDEC_ACTIVE_VDEC, "vde1_vdec_active_vdec",
			"vde1_vdec_active"/* parent */),
	/* VDE11 */
	GATE_HWV_VDE11(CLK_VDE1_LAT_CKEN, "vde1_lat_cken",
			"vdec_ck"/* parent */, 0),
	GATE_VDE11_V(CLK_VDE1_LAT_CKEN_VDEC, "vde1_lat_cken_vdec",
			"vde1_lat_cken"/* parent */),
	GATE_HWV_VDE11(CLK_VDE1_LAT_ACTIVE, "vde1_lat_active",
			"vdec_ck"/* parent */, 4),
	GATE_VDE11_V(CLK_VDE1_LAT_ACTIVE_VDEC, "vde1_lat_active_vdec",
			"vde1_lat_active"/* parent */),
};

static const struct mtk_clk_desc vde1_mcd = {
	.clks = vde1_clks,
	.num_clks = CLK_VDE1_NR_CLK,
};

static const struct mtk_gate_regs ven1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs ven1_hwv_regs = {
	.set_ofs = 0x0068,
	.clr_ofs = 0x006C,
	.sta_ofs = 0x1C34,
};

#define GATE_VEN1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ven1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VEN1_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

#define GATE_HWV_VEN1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ven1_cg_regs,			\
		.hwv_regs = &ven1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate ven1_clks[] = {
	GATE_HWV_VEN1(CLK_VEN1_CKE0_LARB, "ven1_larb",
			"venc_ck"/* parent */, 0),
	GATE_VEN1_V(CLK_VEN1_CKE0_LARB_SMI, "ven1_larb_smi",
			"ven1_larb"/* parent */),
	GATE_HWV_VEN1(CLK_VEN1_CKE1_VENC, "ven1_venc",
			"venc_ck"/* parent */, 4),
	GATE_VEN1_V(CLK_VEN1_CKE1_VENC_VENC, "ven1_venc_venc",
			"ven1_venc"/* parent */),
	GATE_VEN1(CLK_VEN1_CKE2_JPGENC, "ven1_jpgenc",
			"venc_ck"/* parent */, 8),
	GATE_VEN1_V(CLK_VEN1_CKE2_JPGENC_JPGENC, "ven1_jpgenc_jpgenc",
			"ven1_jpgenc"/* parent */),
	GATE_VEN1(CLK_VEN1_CKE3_JPGDEC, "ven1_jpgdec",
			"venc_ck"/* parent */, 12),
	GATE_VEN1_V(CLK_VEN1_CKE3_JPGDEC_JPGDEC, "ven1_jpgdec_jpgdec",
			"ven1_jpgdec"/* parent */),
	GATE_VEN1(CLK_VEN1_CKE4_JPGDEC_C1, "ven1_jpgdec_c1",
			"venc_ck"/* parent */, 16),
	GATE_VEN1_V(CLK_VEN1_CKE4_JPGDEC_C1_JPGDEC, "ven1_jpgdec_c1_jpgdec",
			"ven1_jpgdec_c1"/* parent */),
	GATE_HWV_VEN1(CLK_VEN1_CKE5_GALS, "ven1_gals",
			"venc_ck"/* parent */, 28),
	GATE_VEN1_V(CLK_VEN1_CKE5_GALS_VENC, "ven1_gals_venc",
			"ven1_gals"/* parent */),
	GATE_VEN1_V(CLK_VEN1_CKE5_GALS_SMI, "ven1_gals_smi",
			"ven1_gals"/* parent */),
	GATE_HWV_VEN1(CLK_VEN1_CKE6_GALS_SRAM, "ven1_gals_sram",
			"venc_ck"/* parent */, 31),
	GATE_VEN1_V(CLK_VEN1_CKE6_GALS_SRAM_VENC, "ven1_gals_sram_venc",
			"ven1_gals_sram"/* parent */),
	GATE_VEN1_V(CLK_VEN1_CKE6_GALS_SRAM_SMI, "ven1_gals_sram_smi",
			"ven1_gals_sram"/* parent */),
};

static const struct mtk_clk_desc ven1_mcd = {
	.clks = ven1_clks,
	.num_clks = CLK_VEN1_NR_CLK,
};

static const struct mtk_gate_regs ven2_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs ven2_hwv_regs = {
	.set_ofs = 0x0070,
	.clr_ofs = 0x0074,
	.sta_ofs = 0x1C38,
};

#define GATE_VEN2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ven2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VEN2_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

#define GATE_HWV_VEN2(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ven2_cg_regs,			\
		.hwv_regs = &ven2_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate ven2_clks[] = {
	GATE_HWV_VEN2(CLK_VEN2_CKE0_LARB, "ven2_larb",
			"venc_ck"/* parent */, 0),
	GATE_VEN2_V(CLK_VEN2_CKE0_LARB_VENC, "ven2_larb_venc",
			"ven2_larb"/* parent */),
	GATE_VEN2_V(CLK_VEN2_CKE0_LARB_SMI, "ven2_larb_smi",
			"ven2_larb"/* parent */),
	GATE_HWV_VEN2(CLK_VEN2_CKE1_VENC, "ven2_venc",
			"venc_ck"/* parent */, 4),
	GATE_VEN2_V(CLK_VEN2_CKE1_VENC_VENC, "ven2_venc_venc",
			"ven2_venc"/* parent */),
	GATE_VEN2(CLK_VEN2_CKE2_JPGENC, "ven2_jpgenc",
			"venc_ck"/* parent */, 8),
	GATE_VEN2_V(CLK_VEN2_CKE2_JPGENC_JPGENC, "ven2_jpgenc_jpgenc",
			"ven2_jpgenc"/* parent */),
	GATE_VEN2(CLK_VEN2_CKE3_JPGDEC, "ven2_jpgdec",
			"venc_ck"/* parent */, 12),
	GATE_VEN2_V(CLK_VEN2_CKE3_JPGDEC_JPGDEC, "ven2_jpgdec_jpgdec",
			"ven2_jpgdec"/* parent */),
	GATE_HWV_VEN2(CLK_VEN2_CKE5_GALS, "ven2_gals",
			"venc_ck"/* parent */, 28),
	GATE_VEN2_V(CLK_VEN2_CKE5_GALS_VENC, "ven2_gals_venc",
			"ven2_gals"/* parent */),
	GATE_VEN2_V(CLK_VEN2_CKE5_GALS_SMI, "ven2_gals_smi",
			"ven2_gals"/* parent */),
	GATE_HWV_VEN2(CLK_VEN2_CKE6_GALS_SRAM, "ven2_gals_sram",
			"venc_ck"/* parent */, 31),
	GATE_VEN2_V(CLK_VEN2_CKE6_GALS_SRAM_VENC, "ven2_gals_sram_venc",
			"ven2_gals_sram"/* parent */),
	GATE_VEN2_V(CLK_VEN2_CKE6_GALS_SRAM_SMI, "ven2_gals_sram_smi",
			"ven2_gals_sram"/* parent */),
};

static const struct mtk_clk_desc ven2_mcd = {
	.clks = ven2_clks,
	.num_clks = CLK_VEN2_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6899_vcodec[] = {
	{
		.compatible = "mediatek,mt6899-vdecsys",
		.data = &vde2_mcd,
	}, {
		.compatible = "mediatek,mt6899-vdecsys_soc",
		.data = &vde1_mcd,
	}, {
		.compatible = "mediatek,mt6899-vencsys",
		.data = &ven1_mcd,
	}, {
		.compatible = "mediatek,mt6899-vencsys_c1",
		.data = &ven2_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6899_vcodec_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6899_vcodec_drv = {
	.probe = clk_mt6899_vcodec_grp_probe,
	.driver = {
		.name = "clk-mt6899-vcodec",
		.of_match_table = of_match_clk_mt6899_vcodec,
	},
};

module_platform_driver(clk_mt6899_vcodec_drv);
MODULE_LICENSE("GPL");
