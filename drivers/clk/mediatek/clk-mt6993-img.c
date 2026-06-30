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

#define MT_CCF_BRINGUP		0

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs dip_cine_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs dip_cine_dip10_hwv_regs = {
	.set_ofs = 0x210,
	.clr_ofs = 0x214,
	.sta_ofs = 0x126B0,
};

#define GATE_DIP_CINE_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dip_cine_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
		.flags = RES_FRAMEWORK_MMINFRA,		\
	}

#define GATE_DIP_CINE_DIP1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_DIP_CINE_DIP10(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &dip_cine_dip1_cg_regs,			\
		.hwv_regs = &dip_cine_dip10_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate dip_cine_dip1_clks[] = {
	GATE_HWV_DIP_CINE_DIP10(DIP_CINE_DIP1_LARB, "dip_cine_dip1_larb",
			"mm_img1_ck"/* parent */, 0),
	GATE_DIP_CINE_DIP1_V(DIP_CINE_DIP1_LARB_CAM_P2, "dip_cine_dip1_larb_cam_p2",
	        "dip_cine_dip1_larb"/* parent */),
	GATE_HWV_DIP_CINE_DIP10(DIP_CINE_DIP1_DIP_CINE, "dip_cine_dip1_dip_cine",
			"mm_img1_ck"/* parent */, 1),
	GATE_DIP_CINE_DIP1_V(DIP_CINE_DIP1_DIP_CINE_CAM_P2, "dip_cine_dip1_dip_cine_cam_p2",
	        "dip_cine_dip1_dip_cine"/* parent */),
};

static const struct mtk_clk_desc dip_cine_dip1_mcd = {
	.clks = dip_cine_dip1_clks,
	.num_clks = CLK_DIP_CINE_DIP1_NR_CLK,
};

static const struct mtk_gate_regs dip_nr1_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_DIP_NR1_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dip_nr1_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA,		\
	}

#define GATE_DIP_NR1_DIP1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

static const struct mtk_gate dip_nr1_dip1_clks[] = {
	GATE_DIP_NR1_DIP1(DIP_NR1_DIP1_LARB, "dip_nr1_dip1_larb",
			"mm_img1_ck"/* parent */, 0),
	GATE_DIP_NR1_DIP1_V(DIP_NR1_DIP1_LARB_CAM_P2, "dip_nr1_dip1_larb_cam_p2",
	        "dip_nr1_dip1_larb"/* parent */),
	GATE_DIP_NR1_DIP1(DIP_NR1_DIP1_DIP_NR1, "dip_nr1_dip1_dip_nr1",
			"mm_img1_ck"/* parent */, 1),
	GATE_DIP_NR1_DIP1_V(DIP_NR1_DIP1_DIP_NR1_CAM_P2, "dip_nr1_dip1_dip_nr1_cam_p2",
	        "dip_nr1_dip1_dip_nr1"/* parent */),
};

static const struct mtk_clk_desc dip_nr1_dip1_mcd = {
	.clks = dip_nr1_dip1_clks,
	.num_clks = CLK_DIP_NR1_DIP1_NR_CLK,
};

static const struct mtk_gate_regs dip_nr2_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_DIP_NR2_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dip_nr2_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA,		\
	}

#define GATE_DIP_NR2_DIP1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

static const struct mtk_gate dip_nr2_dip1_clks[] = {
	GATE_DIP_NR2_DIP1(DIP_NR2_DIP1_DIP_NR, "dip_nr2_dip1_dip_nr",
			"mm_img1_ck"/* parent */, 0),
	GATE_DIP_NR2_DIP1_V(DIP_NR2_DIP1_DIP_NR_CAM_P2, "dip_nr2_dip1_dip_nr_cam_p2",
	        "dip_nr2_dip1_dip_nr"/* parent */),
	GATE_DIP_NR2_DIP1(DIP_NR2_DIP1_LARB15, "dip_nr2_dip1_larb15",
			"mm_img1_ck"/* parent */, 1),
	GATE_DIP_NR2_DIP1_V(DIP_NR2_DIP1_LARB15_CAM_P2, "dip_nr2_dip1_larb15_cam_p2",
	        "dip_nr2_dip1_larb15"/* parent */),
	GATE_DIP_NR2_DIP1(DIP_NR2_DIP1_LARB39, "dip_nr2_dip1_larb39",
			"mm_img1_ck"/* parent */, 2),
	GATE_DIP_NR2_DIP1_V(DIP_NR2_DIP1_LARB39_CAM_P2, "dip_nr2_dip1_larb39_cam_p2",
	        "dip_nr2_dip1_larb39"/* parent */),
};

static const struct mtk_clk_desc dip_nr2_dip1_mcd = {
	.clks = dip_nr2_dip1_clks,
	.num_clks = CLK_DIP_NR2_DIP1_NR_CLK,
};

static const struct mtk_gate_regs dip_top_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs dip_top_dip10_hwv_regs = {
	.set_ofs = 0x204,
	.clr_ofs = 0x208,
	.sta_ofs = 0x126AC,
};

#define GATE_DIP_TOP_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dip_top_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
		.flags = RES_FRAMEWORK_VMM,		\
	}

#define GATE_DIP_TOP_DIP1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_DIP_TOP_DIP10(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &dip_top_dip1_cg_regs,			\
		.hwv_regs = &dip_top_dip10_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate dip_top_dip1_clks[] = {
	GATE_HWV_DIP_TOP_DIP10(DIP_TOP_DIP1_DIP_TOP, "dip_dip1_dip_top",
			"mm_img1_ck"/* parent */, 0),
	GATE_DIP_TOP_DIP1_V(DIP_TOP_DIP1_DIP_TOP_CAM_P2, "dip_dip1_dip_top_cam_p2",
	        "dip_dip1_dip_top"/* parent */),
	GATE_HWV_DIP_TOP_DIP10(DIP_TOP_DIP1_DIP_TOP_GALS0, "dip_dip1_dip_gals0",
			"mm_img1_ck"/* parent */, 1),
	GATE_DIP_TOP_DIP1_V(DIP_TOP_DIP1_DIP_TOP_GALS0_CAM_P2, "dip_dip1_dip_gals0_cam_p2",
	        "dip_dip1_dip_gals0"/* parent */),
	GATE_HWV_DIP_TOP_DIP10(DIP_TOP_DIP1_DIP_TOP_GALS1, "dip_dip1_dip_gals1",
			"mm_img1_ck"/* parent */, 2),
	GATE_DIP_TOP_DIP1_V(DIP_TOP_DIP1_DIP_TOP_GALS1_CAM_P2, "dip_dip1_dip_gals1_cam_p2",
	        "dip_dip1_dip_gals1"/* parent */),
	GATE_HWV_DIP_TOP_DIP10(DIP_TOP_DIP1_DIP_TOP_GALS2, "dip_dip1_dip_gals2",
			"mm_img1_ck"/* parent */, 3),
	GATE_DIP_TOP_DIP1_V(DIP_TOP_DIP1_DIP_TOP_GALS2_CAM_P2, "dip_dip1_dip_gals2_cam_p2",
	        "dip_dip1_dip_gals2"/* parent */),
	GATE_HWV_DIP_TOP_DIP10(DIP_TOP_DIP1_DIP_TOP_GALS3, "dip_dip1_dip_gals3",
			"mm_img1_ck"/* parent */, 4),
	GATE_DIP_TOP_DIP1_V(DIP_TOP_DIP1_DIP_TOP_GALS3_CAM_P2, "dip_dip1_dip_gals3_cam_p2",
	        "dip_dip1_dip_gals3"/* parent */),
	GATE_HWV_DIP_TOP_DIP10(DIP_TOP_DIP1_LARB10, "dip_dip1_larb10",
			"mm_img1_ck"/* parent */, 5),
	GATE_DIP_TOP_DIP1_V(DIP_TOP_DIP1_LARB10_CAM_P2, "dip_dip1_larb10_cam_p2",
	        "dip_dip1_larb10"/* parent */),
	GATE_DIP_TOP_DIP1_V(DIP_TOP_DIP1_LARB10_SMI, "dip_dip1_larb10_smi",
	        "dip_dip1_larb10"/* parent */),
	GATE_HWV_DIP_TOP_DIP10(DIP_TOP_DIP1_LARB15, "dip_dip1_larb15",
			"mm_img1_ck"/* parent */, 6),
	GATE_DIP_TOP_DIP1_V(DIP_TOP_DIP1_LARB15_CAM_P2, "dip_dip1_larb15_cam_p2",
	        "dip_dip1_larb15"/* parent */),
	GATE_DIP_TOP_DIP1_V(DIP_TOP_DIP1_LARB15_SMI, "dip_dip1_larb15_smi",
	        "dip_dip1_larb15"/* parent */),
	GATE_HWV_DIP_TOP_DIP10(DIP_TOP_DIP1_LARB38, "dip_dip1_larb38",
			"mm_img1_ck"/* parent */, 7),
	GATE_DIP_TOP_DIP1_V(DIP_TOP_DIP1_LARB38_CAM_P2, "dip_dip1_larb38_cam_p2",
	        "dip_dip1_larb38"/* parent */),
	GATE_DIP_TOP_DIP1_V(DIP_TOP_DIP1_LARB38_SMI, "dip_dip1_larb38_smi",
	        "dip_dip1_larb38"/* parent */),
	GATE_HWV_DIP_TOP_DIP10(DIP_TOP_DIP1_LARB39, "dip_dip1_larb39",
			"mm_img1_ck"/* parent */, 8),
	GATE_DIP_TOP_DIP1_V(DIP_TOP_DIP1_LARB39_CAM_P2, "dip_dip1_larb39_cam_p2",
	        "dip_dip1_larb39"/* parent */),
	GATE_DIP_TOP_DIP1_V(DIP_TOP_DIP1_LARB39_SMI, "dip_dip1_larb39_smi",
	        "dip_dip1_larb39"/* parent */),
};

static const struct mtk_clk_desc dip_top_dip1_mcd = {
	.clks = dip_top_dip1_clks,
	.num_clks = CLK_DIP_TOP_DIP1_NR_CLK,
};

static const struct mtk_gate_regs img0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs img0_hwv_regs = {
	.set_ofs = 0x1bc,
	.clr_ofs = 0x1c0,
	.sta_ofs = 0x12694,
};

static const struct mtk_gate_regs img1_cg_regs = {
	.set_ofs = 0x58,
	.clr_ofs = 0x5C,
	.sta_ofs = 0x54,
};

static const struct mtk_gate_regs img1_hwv_regs = {
	.set_ofs = 0x1b0,
	.clr_ofs = 0x1b4,
	.sta_ofs = 0x12690,
};

static const struct mtk_gate_regs img2_cg_regs = {
	.set_ofs = 0x10,
	.clr_ofs = 0x14,
	.sta_ofs = 0xC,
};

static const struct mtk_gate_regs img2_hwv_regs = {
	.set_ofs = 0x1c8,
	.clr_ofs = 0x1cc,
	.sta_ofs = 0x12698,
};

#define GATE_IMG0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &img0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_IMG0_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_IMG0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &img0_cg_regs,			\
		.hwv_regs = &img0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

#define GATE_IMG1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &img1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_IMG1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_IMG1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &img1_cg_regs,			\
		.hwv_regs = &img1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

#define GATE_IMG2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &img2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_IMG2_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_IMG2(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &img2_cg_regs,			\
		.hwv_regs = &img2_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate img_clks[] = {
	/* IMG0 */
	GATE_HWV_IMG0(IMG_LARB9, "img_larb9",
			"mm_img1_ck"/* parent */, 0),
	GATE_IMG0_V(IMG_LARB9_CAM_P2, "img_larb9_cam_p2",
	        "img_larb9"/* parent */),
	GATE_IMG0_V(IMG_LARB9_SMI, "img_larb9_smi",
	        "img_larb9"/* parent */),
	GATE_HWV_IMG0(IMG_TRAW0, "img_traw0",
			"mm_img1_ck"/* parent */, 1),
	GATE_IMG0_V(IMG_TRAW0_CAM_P2, "img_traw0_cam_p2",
	        "img_traw0"/* parent */),
	GATE_HWV_IMG0(IMG_TRAW1, "img_traw1",
			"mm_img1_ck"/* parent */, 2),
	GATE_IMG0_V(IMG_TRAW1_CAM_P2, "img_traw1_cam_p2",
	        "img_traw1"/* parent */),
	GATE_HWV_IMG0(IMG_DIP0, "img_dip0",
			"mm_img1_ck"/* parent */, 3),
	GATE_IMG0_V(IMG_DIP0_CAM_P2, "img_dip0_cam_p2",
	        "img_dip0"/* parent */),
	GATE_HWV_IMG0(IMG_WPE0, "img_wpe0",
			"mm_img1_ck"/* parent */, 4),
	GATE_IMG0_V(IMG_WPE0_CAM_P2, "img_wpe0_cam_p2",
	        "img_wpe0"/* parent */),
	GATE_HWV_IMG0(IMG_IPE, "img_ipe",
			"mm_img1_ck"/* parent */, 5),
	GATE_IMG0_V(IMG_IPE_CAM_P2, "img_ipe_cam_p2",
	        "img_ipe"/* parent */),
	GATE_HWV_IMG0(IMG_WPE1, "img_wpe1",
			"mm_img1_ck"/* parent */, 6),
	GATE_IMG0_V(IMG_WPE1_CAM_P2, "img_wpe1_cam_p2",
	        "img_wpe1"/* parent */),
	GATE_HWV_IMG0(IMG_WPE2, "img_wpe2",
			"mm_img1_ck"/* parent */, 7),
	GATE_IMG0_V(IMG_WPE2_CAM_P2, "img_wpe2_cam_p2",
	        "img_wpe2"/* parent */),
	GATE_HWV_IMG0(IMG_ADL_LARB, "img_adl_larb",
			"mm_img1_ck"/* parent */, 8),
	GATE_IMG0_V(IMG_ADL_LARB_CAM_P2, "img_adl_larb_cam_p2",
	        "img_adl_larb"/* parent */),
	GATE_IMG0_V(IMG_ADL_LARB_SMI, "img_adl_larb_smi",
	        "img_adl_larb"/* parent */),
	GATE_HWV_IMG0(IMG_ADLRD, "img_adlrd",
			"mm_img1_ck"/* parent */, 9),
	GATE_IMG0_V(IMG_ADLRD_CAM_P2, "img_adlrd_cam_p2",
	        "img_adlrd"/* parent */),
	GATE_HWV_IMG0(IMG_ADLWR0, "img_adlwr0",
			"mm_img1_ck"/* parent */, 10),
	GATE_IMG0_V(IMG_ADLWR0_CAM_P2, "img_adlwr0_cam_p2",
	        "img_adlwr0"/* parent */),
	GATE_HWV_IMG0(IMG_AVS, "img_avs",
			"mm_avs_img_ck"/* parent */, 11),
	GATE_IMG0_V(IMG_AVS_CAM_P2, "img_avs_cam_p2",
	        "img_avs"/* parent */),
	GATE_HWV_IMG0(IMG_IPS, "img_ips",
			"mm_img1_ck"/* parent */, 12),
	GATE_IMG0_V(IMG_IPS_CAM_P2, "img_ips_cam_p2",
	        "img_ips"/* parent */),
	GATE_HWV_IMG0(IMG_ADLWR1, "img_adlwr1",
			"mm_img1_ck"/* parent */, 13),
	GATE_IMG0_V(IMG_ADLWR1_CAM_P2, "img_adlwr1_cam_p2",
	        "img_adlwr1"/* parent */),
	GATE_HWV_IMG0(IMG_ROOTCQ, "img_rootcq",
			"mm_img1_ck"/* parent */, 14),
	GATE_IMG0_V(IMG_ROOTCQ_CAM_P2, "img_rootcq_cam_p2",
	        "img_rootcq"/* parent */),
	GATE_HWV_IMG0(IMG_BLS, "img_bls",
			"mm_img1_ck"/* parent */, 15),
	GATE_IMG0_V(IMG_BLS_CAM_P2, "img_bls_cam_p2",
	        "img_bls"/* parent */),
	GATE_HWV_IMG0(IMG_SDL0, "img_sdl0",
			"mm_img1_ck"/* parent */, 16),
	GATE_IMG0_V(IMG_SDL0_CAM_P2, "img_sdl0_cam_p2",
	        "img_sdl0"/* parent */),
	GATE_HWV_IMG0(IMG_SDL1, "img_sdl1",
			"mm_img1_ck"/* parent */, 17),
	GATE_IMG0_V(IMG_SDL1_CAM_P2, "img_sdl1_cam_p2",
	        "img_sdl1"/* parent */),
	GATE_HWV_IMG0(IMG_SDL2, "img_sdl2",
			"mm_img1_ck"/* parent */, 18),
	GATE_IMG0_V(IMG_SDL2_CAM_P2, "img_sdl2_cam_p2",
	        "img_sdl2"/* parent */),
	GATE_HWV_IMG0(IMG_SDL3, "img_sdl3",
			"mm_img1_ck"/* parent */, 19),
	GATE_IMG0_V(IMG_SDL3_CAM_P2, "img_sdl3_cam_p2",
	        "img_sdl3"/* parent */),
	GATE_HWV_IMG0(IMG_SUB_COMMON0, "img_sub_common0",
			"mm_img1_ck"/* parent */, 20),
	GATE_IMG0_V(IMG_SUB_COMMON0_CAM_P2, "img_sub_common0_cam_p2",
	        "img_sub_common0"/* parent */),
	GATE_IMG0_V(IMG_SUB_COMMON0_SMI, "img_sub_common0_smi",
	        "img_sub_common0"/* parent */),
	GATE_HWV_IMG0(IMG_SUB_COMMON1, "img_sub_common1",
			"mm_img1_ck"/* parent */, 21),
	GATE_IMG0_V(IMG_SUB_COMMON1_CAM_P2, "img_sub_common1_cam_p2",
	        "img_sub_common1"/* parent */),
	GATE_IMG0_V(IMG_SUB_COMMON1_SMI, "img_sub_common1_smi",
	        "img_sub_common1"/* parent */),
	GATE_HWV_IMG0(IMG_SUB_COMMON2, "img_sub_common2",
			"mm_img1_ck"/* parent */, 22),
	GATE_IMG0_V(IMG_SUB_COMMON2_CAM_P2, "img_sub_common2_cam_p2",
	        "img_sub_common2"/* parent */),
	GATE_IMG0_V(IMG_SUB_COMMON2_SMI, "img_sub_common2_smi",
	        "img_sub_common2"/* parent */),
	GATE_HWV_IMG0(IMG_SUB_COMMON3, "img_sub_common3",
			"mm_img1_ck"/* parent */, 23),
	GATE_IMG0_V(IMG_SUB_COMMON3_CAM_P2, "img_sub_common3_cam_p2",
	        "img_sub_common3"/* parent */),
	GATE_IMG0_V(IMG_SUB_COMMON3_SMI, "img_sub_common3_smi",
	        "img_sub_common3"/* parent */),
	GATE_HWV_IMG0(IMG_SUB_COMMON4, "img_sub_common4",
			"mm_img1_ck"/* parent */, 24),
	GATE_IMG0_V(IMG_SUB_COMMON4_CAM_P2, "img_sub_common4_cam_p2",
	        "img_sub_common4"/* parent */),
	GATE_IMG0_V(IMG_SUB_COMMON4_SMI, "img_sub_common4_smi",
	        "img_sub_common4"/* parent */),
	GATE_HWV_IMG0(IMG_SUB_COMMON5, "img_sub_common5",
			"mm_img1_ck"/* parent */, 25),
	GATE_IMG0_V(IMG_SUB_COMMON5_CAM_P2, "img_sub_common5_cam_p2",
	        "img_sub_common5"/* parent */),
	GATE_IMG0_V(IMG_SUB_COMMON5_SMI, "img_sub_common5_smi",
	        "img_sub_common5"/* parent */),
	GATE_HWV_IMG0(IMG_SUB_COMMON6, "img_sub_common6",
			"mm_img1_ck"/* parent */, 26),
	GATE_IMG0_V(IMG_SUB_COMMON6_CAM_P2, "img_sub_common6_cam_p2",
	        "img_sub_common6"/* parent */),
	GATE_IMG0_V(IMG_SUB_COMMON6_SMI, "img_sub_common6_smi",
	        "img_sub_common6"/* parent */),
	GATE_HWV_IMG0(IMG_GALS7, "img_gals7",
			"mm_img1_ck"/* parent */, 27),
	GATE_IMG0_V(IMG_GALS7_CAM_P2, "img_gals7_cam_p2",
	        "img_gals7"/* parent */),
	/* IMG1 */
	GATE_HWV_IMG1(IMG_FDVT, "img_fdvt",
			"mm_ipe_ck"/* parent */, 0),
	GATE_IMG1_V(IMG_FDVT_CAM_P2, "img_fdvt_cam_p2",
	        "img_fdvt"/* parent */),
	GATE_HWV_IMG1(IMG_LARB12, "img_larb12",
			"mm_ipe_ck"/* parent */, 3),
	GATE_IMG1_V(IMG_LARB12_CAM_P2, "img_larb12_cam_p2",
	        "img_larb12"/* parent */),
	GATE_IMG1_V(IMG_LARB12_SMI, "img_larb12_smi",
	        "img_larb12"/* parent */),
	GATE_HWV_IMG1(IMG_IPESYS_CVFS26, "img_ipesys_cvfs26",
			"cksys_img_aov_26m_ck"/* parent */, 4),
	GATE_IMG1_V(IMG_IPESYS_CVFS26_CAM_P2, "img_ipesys_cvfs26_cam_p2",
	        "img_ipesys_cvfs26"/* parent */),
	GATE_HWV_IMG1(IMG_ODPM26, "img_odpm26",
			"cksys_img_aov_26m_ck"/* parent */, 5),
	GATE_IMG1_V(IMG_ODPM26_CAM_P2, "img_odpm26_cam_p2",
	        "img_odpm26"/* parent */),
	/* IMG2 */
	GATE_HWV_IMG2(IMG_GALS, "img_gals",
			"mm_img1_ck"/* parent */, 0),
	GATE_IMG2_V(IMG_GALS_CAM_P2, "img_gals_cam_p2",
	        "img_gals"/* parent */),
	GATE_HWV_IMG2(IMG_GALS_RX_DIP0, "img_gals_rx_dip0",
			"mm_img1_ck"/* parent */, 1),
	GATE_IMG2_V(IMG_GALS_RX_DIP0_CAM_P2, "img_gals_rx_dip0_cam_p2",
	        "img_gals_rx_dip0"/* parent */),
	GATE_HWV_IMG2(IMG_GALS_RX_DIP1, "img_gals_rx_dip1",
			"mm_img1_ck"/* parent */, 2),
	GATE_IMG2_V(IMG_GALS_RX_DIP1_CAM_P2, "img_gals_rx_dip1_cam_p2",
	        "img_gals_rx_dip1"/* parent */),
	GATE_HWV_IMG2(IMG_GALS_RX_TRAW0, "img_gals_rx_traw0",
			"mm_img1_ck"/* parent */, 3),
	GATE_IMG2_V(IMG_GALS_RX_TRAW0_CAM_P2, "img_gals_rx_traw0_cam_p2",
	        "img_gals_rx_traw0"/* parent */),
	GATE_HWV_IMG2(IMG_GALS_RX_WPE0, "img_gals_rx_wpe0",
			"mm_img1_ck"/* parent */, 4),
	GATE_IMG2_V(IMG_GALS_RX_WPE0_CAM_P2, "img_gals_rx_wpe0_cam_p2",
	        "img_gals_rx_wpe0"/* parent */),
	GATE_HWV_IMG2(IMG_GALS_RX_WPE1, "img_gals_rx_wpe1",
			"mm_img1_ck"/* parent */, 5),
	GATE_IMG2_V(IMG_GALS_RX_WPE1_CAM_P2, "img_gals_rx_wpe1_cam_p2",
	        "img_gals_rx_wpe1"/* parent */),
	GATE_HWV_IMG2(IMG_GALS_RX_WPE2, "img_gals_rx_wpe2",
			"mm_img1_ck"/* parent */, 6),
	GATE_IMG2_V(IMG_GALS_RX_WPE2_CAM_P2, "img_gals_rx_wpe2_cam_p2",
	        "img_gals_rx_wpe2"/* parent */),
	GATE_HWV_IMG2(IMG_GALS_RX_WPE3, "img_gals_rx_wpe3",
			"mm_img1_ck"/* parent */, 7),
	GATE_IMG2_V(IMG_GALS_RX_WPE3_CAM_P2, "img_gals_rx_wpe3_cam_p2",
	        "img_gals_rx_wpe3"/* parent */),
	GATE_HWV_IMG2(IMG_GALS_TRX_IPE0, "img_gals_trx_ipe0",
			"mm_img1_ck"/* parent */, 8),
	GATE_IMG2_V(IMG_GALS_TRX_IPE0_CAM_P2, "img_gals_trx_ipe0_cam_p2",
	        "img_gals_trx_ipe0"/* parent */),
	GATE_HWV_IMG2(IMG_GALS_TRX_IPE1, "img_gals_trx_ipe1",
			"mm_img1_ck"/* parent */, 9),
	GATE_IMG2_V(IMG_GALS_TRX_IPE1_CAM_P2, "img_gals_trx_ipe1_cam_p2",
	        "img_gals_trx_ipe1"/* parent */),
	GATE_HWV_IMG2(IMG26, "img26",
			"cksys_img_aov_26m_ck"/* parent */, 10),
	GATE_IMG2_V(IMG26_CAM_P2, "img26_cam_p2",
	        "img26"/* parent */),
	GATE_HWV_IMG2(IMG_BWR, "img_bwr",
			"mm_img1_ck"/* parent */, 11),
	GATE_IMG2_V(IMG_BWR_CAM_P2, "img_bwr_cam_p2",
	        "img_bwr"/* parent */),
	GATE_HWV_IMG2(IMG_ISC, "img_isc",
			"mm_img1_ck"/* parent */, 12),
	GATE_IMG2_V(IMG_ISC_CAM_P2, "img_isc_cam_p2",
	        "img_isc"/* parent */),
	GATE_HWV_IMG2(IMGSYS_CVFS26, "imgsys_cvfs26",
			"cksys_img_aov_26m_ck"/* parent */, 13),
	GATE_IMG2_V(IMGSYS_CVFS26_CAM_P2, "imgsys_cvfs26_cam_p2",
	        "imgsys_cvfs26"/* parent */),
};

static const struct mtk_clk_desc img_mcd = {
	.clks = img_clks,
	.num_clks = CLK_IMG_NR_CLK,
};

static const struct mtk_gate_regs img_v_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs img_v0_hwv_regs = {
	.set_ofs = 0x1d4,
	.clr_ofs = 0x1d8,
	.sta_ofs = 0x1269C,
};

#define GATE_IMG_V(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &img_v_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_IMG_V_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_IMG_V0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &img_v_cg_regs,			\
		.hwv_regs = &img_v0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate img_v_clks[] = {
	GATE_HWV_IMG_V0(IMG_VCORE_GALS_DISP, "img_vcore_gals_disp",
			"mm_mminfra_ck"/* parent */, 0),
	GATE_IMG_V_V(IMG_VCORE_GALS_DISP_CAM_P2, "img_vcore_gals_disp_cam_p2",
	        "img_vcore_gals_disp"/* parent */),
	GATE_HWV_IMG_V0(IMG_VCORE_MAIN, "img_vcore_main",
			"mm_mminfra_ck"/* parent */, 1),
	GATE_IMG_V_V(IMG_VCORE_MAIN_CAM_P2, "img_vcore_main_cam_p2",
	        "img_vcore_main"/* parent */),
	GATE_HWV_IMG_V0(IMG_VCORE_SUB0, "img_vcore_sub0",
			"mm_mminfra_ck"/* parent */, 2),
	GATE_IMG_V_V(IMG_VCORE_SUB0_CAM_P2, "img_vcore_sub0_cam_p2",
	        "img_vcore_sub0"/* parent */),
	GATE_IMG_V_V(IMG_VCORE_SUB0_SMI, "img_vcore_sub0_smi",
	        "img_vcore_sub0"/* parent */),
	GATE_HWV_IMG_V0(IMG_VCORE_SUB1, "img_vcore_sub1",
			"mm_mminfra_ck"/* parent */, 3),
	GATE_IMG_V_V(IMG_VCORE_SUB1_CAM_P2, "img_vcore_sub1_cam_p2",
	        "img_vcore_sub1"/* parent */),
	GATE_IMG_V_V(IMG_VCORE_SUB1_SMI, "img_vcore_sub1_smi",
	        "img_vcore_sub1"/* parent */),
	GATE_HWV_IMG_V0(IMG_VCORE_SUB2, "img_vcore_sub2",
			"mm_mminfra_ck"/* parent */, 4),
	GATE_IMG_V_V(IMG_VCORE_SUB2_CAM_P2, "img_vcore_sub2_cam_p2",
	        "img_vcore_sub2"/* parent */),
	GATE_IMG_V_V(IMG_VCORE_SUB2_SMI, "img_vcore_sub2_smi",
	        "img_vcore_sub2"/* parent */),
	GATE_HWV_IMG_V0(IMG_VCORE_IMG_26M, "img_vcore_img_26m",
			"cksys_img_aov_26m_ck"/* parent */, 5),
	GATE_IMG_V_V(IMG_VCORE_IMG_26M_CAM_P2, "img_vcore_img_26m_cam_p2",
	        "img_vcore_img_26m"/* parent */),
};

static const struct mtk_clk_desc img_v_mcd = {
	.clks = img_v_clks,
	.num_clks = CLK_IMG_V_NR_CLK,
};

static const struct mtk_gate_regs traw_cap_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs traw_cap_dip10_hwv_regs = {
	.set_ofs = 0x228,
	.clr_ofs = 0x22c,
	.sta_ofs = 0x126B8,
};

#define GATE_TRAW_CAP_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &traw_cap_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_TRAW_CAP_DIP1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_TRAW_CAP_DIP10(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &traw_cap_dip1_cg_regs,			\
		.hwv_regs = &traw_cap_dip10_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate traw_cap_dip1_clks[] = {
	GATE_HWV_TRAW_CAP_DIP10(TRAW_CAP_DIP1_TRAW_CAP, "traw__dip1_cap",
			"mm_img1_ck"/* parent */, 0),
	GATE_TRAW_CAP_DIP1_V(TRAW_CAP_DIP1_TRAW_CAP_CAM_P2, "traw__dip1_cap_cam_p2",
	        "traw__dip1_cap"/* parent */),
};

static const struct mtk_clk_desc traw_cap_dip1_mcd = {
	.clks = traw_cap_dip1_clks,
	.num_clks = CLK_TRAW_CAP_DIP1_NR_CLK,
};

static const struct mtk_gate_regs traw_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs traw_dip10_hwv_regs = {
	.set_ofs = 0x21c,
	.clr_ofs = 0x220,
	.sta_ofs = 0x126B4,
};

#define GATE_TRAW_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &traw_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_TRAW_DIP1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_TRAW_DIP10(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &traw_dip1_cg_regs,			\
		.hwv_regs = &traw_dip10_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate traw_dip1_clks[] = {
	GATE_HWV_TRAW_DIP10(TRAW_DIP1_LARB28, "traw_dip1_larb28",
			"mm_img1_ck"/* parent */, 0),
	GATE_TRAW_DIP1_V(TRAW_DIP1_LARB28_CAM_P2, "traw_dip1_larb28_cam_p2",
	        "traw_dip1_larb28"/* parent */),
	GATE_TRAW_DIP1_V(TRAW_DIP1_LARB28_SMI, "traw_dip1_larb28_smi",
	        "traw_dip1_larb28"/* parent */),
	GATE_HWV_TRAW_DIP10(TRAW_DIP1_LARB40, "traw_dip1_larb40",
			"mm_img1_ck"/* parent */, 1),
	GATE_TRAW_DIP1_V(TRAW_DIP1_LARB40_CAM_P2, "traw_dip1_larb40_cam_p2",
	        "traw_dip1_larb40"/* parent */),
	GATE_TRAW_DIP1_V(TRAW_DIP1_LARB40_SMI, "traw_dip1_larb40_smi",
	        "traw_dip1_larb40"/* parent */),
	GATE_HWV_TRAW_DIP10(TRAW_DIP1_TRAW, "traw_dip1_traw",
			"mm_img1_ck"/* parent */, 2),
	GATE_TRAW_DIP1_V(TRAW_DIP1_TRAW_CAM_P2, "traw_dip1_traw_cam_p2",
	        "traw_dip1_traw"/* parent */),
	GATE_HWV_TRAW_DIP10(TRAW_DIP1_GALS, "traw_dip1_gals",
			"mm_img1_ck"/* parent */, 3),
	GATE_TRAW_DIP1_V(TRAW_DIP1_GALS_CAM_P2, "traw_dip1_gals_cam_p2",
	        "traw_dip1_gals"/* parent */),
};

static const struct mtk_clk_desc traw_dip1_mcd = {
	.clks = traw_dip1_clks,
	.num_clks = CLK_TRAW_DIP1_NR_CLK,
};

static const struct mtk_gate_regs wpe_eis_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs wpe_eis_dip10_hwv_regs = {
	.set_ofs = 0x1e0,
	.clr_ofs = 0x1e4,
	.sta_ofs = 0x126A0,
};

#define GATE_WPE_EIS_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &wpe_eis_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_WPE_EIS_DIP1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_WPE_EIS_DIP10(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &wpe_eis_dip1_cg_regs,			\
		.hwv_regs = &wpe_eis_dip10_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate wpe_eis_dip1_clks[] = {
	GATE_HWV_WPE_EIS_DIP10(WPE_EIS_DIP1_LARB_U0, "wpe_eis_dip1_larb_u0",
			"mm_img1_ck"/* parent */, 0),
	GATE_WPE_EIS_DIP1_V(WPE_EIS_DIP1_LARB_U0_CAM_P2, "wpe_eis_dip1_larb_u0_cam_p2",
	        "wpe_eis_dip1_larb_u0"/* parent */),
	GATE_WPE_EIS_DIP1_V(WPE_EIS_DIP1_LARB_U0_SMI, "wpe_eis_dip1_larb_u0_smi",
	        "wpe_eis_dip1_larb_u0"/* parent */),
	GATE_HWV_WPE_EIS_DIP10(WPE_EIS_DIP1_LARB_U1, "wpe_eis_dip1_larb_u1",
			"mm_img1_ck"/* parent */, 1),
	GATE_WPE_EIS_DIP1_V(WPE_EIS_DIP1_LARB_U1_CAM_P2, "wpe_eis_dip1_larb_u1_cam_p2",
	        "wpe_eis_dip1_larb_u1"/* parent */),
	GATE_WPE_EIS_DIP1_V(WPE_EIS_DIP1_LARB_U1_SMI, "wpe_eis_dip1_larb_u1_smi",
	        "wpe_eis_dip1_larb_u1"/* parent */),
	GATE_HWV_WPE_EIS_DIP10(WPE_EIS_DIP1_GALS_U0, "wpe_eis_dip1_gals_u0",
			"mm_img1_ck"/* parent */, 2),
	GATE_WPE_EIS_DIP1_V(WPE_EIS_DIP1_GALS_U0_CAM_P2, "wpe_eis_dip1_gals_u0_cam_p2",
	        "wpe_eis_dip1_gals_u0"/* parent */),
	GATE_HWV_WPE_EIS_DIP10(WPE_EIS_DIP1_GALS_U1, "wpe_eis_dip1_gals_u1",
			"mm_img1_ck"/* parent */, 3),
	GATE_WPE_EIS_DIP1_V(WPE_EIS_DIP1_GALS_U1_CAM_P2, "wpe_eis_dip1_gals_u1_cam_p2",
	        "wpe_eis_dip1_gals_u1"/* parent */),
	GATE_HWV_WPE_EIS_DIP10(WPE_EIS_DIP1_WPE_MACRO, "wpe_eis_dip1_wpe_macro",
			"mm_img1_ck"/* parent */, 4),
	GATE_WPE_EIS_DIP1_V(WPE_EIS_DIP1_WPE_MACRO_CAM_P2, "wpe_eis_dip1_wpe_macro_cam_p2",
	        "wpe_eis_dip1_wpe_macro"/* parent */),
	GATE_HWV_WPE_EIS_DIP10(WPE_EIS_DIP1_WPE, "wpe_eis_dip1_wpe",
			"mm_img1_ck"/* parent */, 5),
	GATE_WPE_EIS_DIP1_V(WPE_EIS_DIP1_WPE_CAM_P2, "wpe_eis_dip1_wpe_cam_p2",
	        "wpe_eis_dip1_wpe"/* parent */),
	GATE_HWV_WPE_EIS_DIP10(WPE_EIS_DIP1_PQDIP, "wpe_eis_dip1_pqdip",
			"mm_img1_ck"/* parent */, 6),
	GATE_WPE_EIS_DIP1_V(WPE_EIS_DIP1_PQDIP_CAM_P2, "wpe_eis_dip1_pqdip_cam_p2",
	        "wpe_eis_dip1_pqdip"/* parent */),
	GATE_HWV_WPE_EIS_DIP10(WPE_EIS_DIP1_PQDIP_DMA, "wpe_eis_dip1_pqdip_dma",
			"mm_img1_ck"/* parent */, 7),
	GATE_WPE_EIS_DIP1_V(WPE_EIS_DIP1_PQDIP_DMA_CAM_P2, "wpe_eis_dip1_pqdip_dma_cam_p2",
	        "wpe_eis_dip1_pqdip_dma"/* parent */),
	GATE_HWV_WPE_EIS_DIP10(WPE_EIS_DIP1_OMC, "wpe_eis_dip1_omc",
			"mm_img1_ck"/* parent */, 8),
	GATE_WPE_EIS_DIP1_V(WPE_EIS_DIP1_OMC_CAM_P2, "wpe_eis_dip1_omc_cam_p2",
	        "wpe_eis_dip1_omc"/* parent */),
	GATE_HWV_WPE_EIS_DIP10(WPE_EIS_DIP1_DPE, "wpe_eis_dip1_dpe",
			"mm_img1_ck"/* parent */, 9),
	GATE_WPE_EIS_DIP1_V(WPE_EIS_DIP1_DPE_CAM_P2, "wpe_eis_dip1_dpe_cam_p2",
	        "wpe_eis_dip1_dpe"/* parent */),
	GATE_HWV_WPE_EIS_DIP10(WPE_EIS_DIP1_DFPS, "wpe_eis_dip1_dfps",
			"mm_img1_ck"/* parent */, 10),
	GATE_WPE_EIS_DIP1_V(WPE_EIS_DIP1_DFPS_CAM_P2, "wpe_eis_dip1_dfps_cam_p2",
	        "wpe_eis_dip1_dfps"/* parent */),
	GATE_HWV_WPE_EIS_DIP10(WPE_EIS_DIP1_DFP0, "wpe_eis_dip1_dfp0",
			"mm_img1_ck"/* parent */, 11),
	GATE_WPE_EIS_DIP1_V(WPE_EIS_DIP1_DFP0_CAM_P2, "wpe_eis_dip1_dfp0_cam_p2",
	        "wpe_eis_dip1_dfp0"/* parent */),
	GATE_HWV_WPE_EIS_DIP10(WPE_EIS_DIP1_DFP1, "wpe_eis_dip1_dfp1",
			"mm_img1_ck"/* parent */, 12),
	GATE_WPE_EIS_DIP1_V(WPE_EIS_DIP1_DFP1_CAM_P2, "wpe_eis_dip1_dfp1_cam_p2",
	        "wpe_eis_dip1_dfp1"/* parent */),
	GATE_HWV_WPE_EIS_DIP10(WPE_EIS_DIP1_DWPE, "wpe_eis_dip1_dwpe",
			"mm_img1_ck"/* parent */, 13),
	GATE_WPE_EIS_DIP1_V(WPE_EIS_DIP1_DWPE_CAM_P2, "wpe_eis_dip1_dwpe_cam_p2",
	        "wpe_eis_dip1_dwpe"/* parent */),
	GATE_HWV_WPE_EIS_DIP10(WPE_EIS_DIP1_ME, "wpe_eis_dip1_me",
			"mm_img1_ck"/* parent */, 14),
	GATE_WPE_EIS_DIP1_V(WPE_EIS_DIP1_ME_CAM_P2, "wpe_eis_dip1_me_cam_p2",
	        "wpe_eis_dip1_me"/* parent */),
	GATE_HWV_WPE_EIS_DIP10(WPE_EIS_DIP1_MMG, "wpe_eis_dip1_mmg",
			"mm_img1_ck"/* parent */, 15),
	GATE_WPE_EIS_DIP1_V(WPE_EIS_DIP1_MMG_CAM_P2, "wpe_eis_dip1_mmg_cam_p2",
	        "wpe_eis_dip1_mmg"/* parent */),
	GATE_HWV_WPE_EIS_DIP10(WPE_EIS_DIP1_WPE_26M_EN, "wpe_eis_dip1_wpe_26m",
			"mm_img1_ck"/* parent */, 16),
	GATE_WPE_EIS_DIP1_V(WPE_EIS_DIP1_WPE_26M_EN_CAM_P2, "wpe_eis_dip1_wpe_26m_cam_p2",
	        "wpe_eis_dip1_wpe_26m"/* parent */),
};

static const struct mtk_clk_desc wpe_eis_dip1_mcd = {
	.clks = wpe_eis_dip1_clks,
	.num_clks = CLK_WPE_EIS_DIP1_NR_CLK,
};

static const struct mtk_gate_regs wpe_lite_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs wpe_lite_dip10_hwv_regs = {
	.set_ofs = 0x1f8,
	.clr_ofs = 0x1fc,
	.sta_ofs = 0x126A8,
};

#define GATE_WPE_LITE_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &wpe_lite_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_WPE_LITE_DIP1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_WPE_LITE_DIP10(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &wpe_lite_dip1_cg_regs,			\
		.hwv_regs = &wpe_lite_dip10_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate wpe_lite_dip1_clks[] = {
	GATE_HWV_WPE_LITE_DIP10(WPE_LITE_DIP1_LARB_U0, "wpe_lite_dip1_larb_u0",
			"mm_img1_ck"/* parent */, 0),
	GATE_WPE_LITE_DIP1_V(WPE_LITE_DIP1_LARB_U0_CAM_P2, "wpe_lite_dip1_larb_u0_cam_p2",
	        "wpe_lite_dip1_larb_u0"/* parent */),
	GATE_WPE_LITE_DIP1_V(WPE_LITE_DIP1_LARB_U0_SMI, "wpe_lite_dip1_larb_u0_smi",
	        "wpe_lite_dip1_larb_u0"/* parent */),
	GATE_HWV_WPE_LITE_DIP10(WPE_LITE_DIP1_LARB_U1, "wpe_lite_dip1_larb_u1",
			"mm_img1_ck"/* parent */, 1),
	GATE_WPE_LITE_DIP1_V(WPE_LITE_DIP1_LARB_U1_CAM_P2, "wpe_lite_dip1_larb_u1_cam_p2",
	        "wpe_lite_dip1_larb_u1"/* parent */),
	GATE_WPE_LITE_DIP1_V(WPE_LITE_DIP1_LARB_U1_SMI, "wpe_lite_dip1_larb_u1_smi",
	        "wpe_lite_dip1_larb_u1"/* parent */),
	GATE_HWV_WPE_LITE_DIP10(WPE_LITE_DIP1_GALS_U0, "wpe_lite_dip1_gals_u0",
			"mm_img1_ck"/* parent */, 2),
	GATE_WPE_LITE_DIP1_V(WPE_LITE_DIP1_GALS_U0_CAM_P2, "wpe_lite_dip1_gals_u0_cam_p2",
	        "wpe_lite_dip1_gals_u0"/* parent */),
	GATE_HWV_WPE_LITE_DIP10(WPE_LITE_DIP1_GALS_U1, "wpe_lite_dip1_gals_u1",
			"mm_img1_ck"/* parent */, 3),
	GATE_WPE_LITE_DIP1_V(WPE_LITE_DIP1_GALS_U1_CAM_P2, "wpe_lite_dip1_gals_u1_cam_p2",
	        "wpe_lite_dip1_gals_u1"/* parent */),
	GATE_HWV_WPE_LITE_DIP10(WPE_LITE_DIP1_WPE_MACRO, "wpe_lite_dip1_wpe_macro",
			"mm_img1_ck"/* parent */, 4),
	GATE_WPE_LITE_DIP1_V(WPE_LITE_DIP1_WPE_MACRO_CAM_P2, "wpe_lite_dip1_wpe_macro_cam_p2",
	        "wpe_lite_dip1_wpe_macro"/* parent */),
	GATE_HWV_WPE_LITE_DIP10(WPE_LITE_DIP1_WPE, "wpe_lite_dip1_wpe",
			"mm_img1_ck"/* parent */, 5),
	GATE_WPE_LITE_DIP1_V(WPE_LITE_DIP1_WPE_CAM_P2, "wpe_lite_dip1_wpe_cam_p2",
	        "wpe_lite_dip1_wpe"/* parent */),
	GATE_HWV_WPE_LITE_DIP10(WPE_LITE_DIP1_PQDIP, "wpe_lite_dip1_pqdip",
			"mm_img1_ck"/* parent */, 6),
	GATE_WPE_LITE_DIP1_V(WPE_LITE_DIP1_PQDIP_CAM_P2, "wpe_lite_dip1_pqdip_cam_p2",
	        "wpe_lite_dip1_pqdip"/* parent */),
	GATE_HWV_WPE_LITE_DIP10(WPE_LITE_DIP1_PQDIP_DMA, "wpe_lite_dip1_pqdip_dma",
			"mm_img1_ck"/* parent */, 7),
	GATE_WPE_LITE_DIP1_V(WPE_LITE_DIP1_PQDIP_DMA_CAM_P2, "wpe_lite_dip1_pqdip_dma_cam_p2",
	        "wpe_lite_dip1_pqdip_dma"/* parent */),
	GATE_HWV_WPE_LITE_DIP10(WPE_LITE_DIP1_OMC, "wpe_lite_dip1_omc",
			"mm_img1_ck"/* parent */, 8),
	GATE_WPE_LITE_DIP1_V(WPE_LITE_DIP1_OMC_CAM_P2, "wpe_lite_dip1_omc_cam_p2",
	        "wpe_lite_dip1_omc"/* parent */),
	GATE_HWV_WPE_LITE_DIP10(WPE_LITE_DIP1_DPE, "wpe_lite_dip1_dpe",
			"mm_img1_ck"/* parent */, 9),
	GATE_WPE_LITE_DIP1_V(WPE_LITE_DIP1_DPE_CAM_P2, "wpe_lite_dip1_dpe_cam_p2",
	        "wpe_lite_dip1_dpe"/* parent */),
	GATE_HWV_WPE_LITE_DIP10(WPE_LITE_DIP1_DFPS, "wpe_lite_dip1_dfps",
			"mm_img1_ck"/* parent */, 10),
	GATE_WPE_LITE_DIP1_V(WPE_LITE_DIP1_DFPS_CAM_P2, "wpe_lite_dip1_dfps_cam_p2",
	        "wpe_lite_dip1_dfps"/* parent */),
	GATE_HWV_WPE_LITE_DIP10(WPE_LITE_DIP1_DFP0, "wpe_lite_dip1_dfp0",
			"mm_img1_ck"/* parent */, 11),
	GATE_WPE_LITE_DIP1_V(WPE_LITE_DIP1_DFP0_CAM_P2, "wpe_lite_dip1_dfp0_cam_p2",
	        "wpe_lite_dip1_dfp0"/* parent */),
	GATE_HWV_WPE_LITE_DIP10(WPE_LITE_DIP1_DFP1, "wpe_lite_dip1_dfp1",
			"mm_img1_ck"/* parent */, 12),
	GATE_WPE_LITE_DIP1_V(WPE_LITE_DIP1_DFP1_CAM_P2, "wpe_lite_dip1_dfp1_cam_p2",
	        "wpe_lite_dip1_dfp1"/* parent */),
	GATE_HWV_WPE_LITE_DIP10(WPE_LITE_DIP1_DWPE, "wpe_lite_dip1_dwpe",
			"mm_img1_ck"/* parent */, 13),
	GATE_WPE_LITE_DIP1_V(WPE_LITE_DIP1_DWPE_CAM_P2, "wpe_lite_dip1_dwpe_cam_p2",
	        "wpe_lite_dip1_dwpe"/* parent */),
	GATE_HWV_WPE_LITE_DIP10(WPE_LITE_DIP1_ME, "wpe_lite_dip1_me",
			"mm_img1_ck"/* parent */, 14),
	GATE_WPE_LITE_DIP1_V(WPE_LITE_DIP1_ME_CAM_P2, "wpe_lite_dip1_me_cam_p2",
	        "wpe_lite_dip1_me"/* parent */),
	GATE_HWV_WPE_LITE_DIP10(WPE_LITE_DIP1_MMG, "wpe_lite_dip1_mmg",
			"mm_img1_ck"/* parent */, 15),
	GATE_WPE_LITE_DIP1_V(WPE_LITE_DIP1_MMG_CAM_P2, "wpe_lite_dip1_mmg_cam_p2",
	        "wpe_lite_dip1_mmg"/* parent */),
};

static const struct mtk_clk_desc wpe_lite_dip1_mcd = {
	.clks = wpe_lite_dip1_clks,
	.num_clks = CLK_WPE_LITE_DIP1_NR_CLK,
};

static const struct mtk_gate_regs wpe_tnr_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs wpe_tnr_dip10_hwv_regs = {
	.set_ofs = 0x1ec,
	.clr_ofs = 0x1f0,
	.sta_ofs = 0x126A4,
};

#define GATE_WPE_TNR_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &wpe_tnr_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_WPE_TNR_DIP1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_WPE_TNR_DIP10(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &wpe_tnr_dip1_cg_regs,			\
		.hwv_regs = &wpe_tnr_dip10_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate wpe_tnr_dip1_clks[] = {
	GATE_HWV_WPE_TNR_DIP10(WPE_TNR_DIP1_LARB_U0, "wpe_tnr_dip1_larb_u0",
			"mm_img1_ck"/* parent */, 0),
	GATE_WPE_TNR_DIP1_V(WPE_TNR_DIP1_LARB_U0_CAM_P2, "wpe_tnr_dip1_larb_u0_cam_p2",
	        "wpe_tnr_dip1_larb_u0"/* parent */),
	GATE_WPE_TNR_DIP1_V(WPE_TNR_DIP1_LARB_U0_SMI, "wpe_tnr_dip1_larb_u0_smi",
	        "wpe_tnr_dip1_larb_u0"/* parent */),
	GATE_HWV_WPE_TNR_DIP10(WPE_TNR_DIP1_LARB_U1, "wpe_tnr_dip1_larb_u1",
			"mm_img1_ck"/* parent */, 1),
	GATE_WPE_TNR_DIP1_V(WPE_TNR_DIP1_LARB_U1_CAM_P2, "wpe_tnr_dip1_larb_u1_cam_p2",
	        "wpe_tnr_dip1_larb_u1"/* parent */),
	GATE_WPE_TNR_DIP1_V(WPE_TNR_DIP1_LARB_U1_SMI, "wpe_tnr_dip1_larb_u1_smi",
	        "wpe_tnr_dip1_larb_u1"/* parent */),
	GATE_HWV_WPE_TNR_DIP10(WPE_TNR_DIP1_GALS_U0, "wpe_tnr_dip1_gals_u0",
			"mm_img1_ck"/* parent */, 2),
	GATE_WPE_TNR_DIP1_V(WPE_TNR_DIP1_GALS_U0_CAM_P2, "wpe_tnr_dip1_gals_u0_cam_p2",
	        "wpe_tnr_dip1_gals_u0"/* parent */),
	GATE_HWV_WPE_TNR_DIP10(WPE_TNR_DIP1_GALS_U1, "wpe_tnr_dip1_gals_u1",
			"mm_img1_ck"/* parent */, 3),
	GATE_WPE_TNR_DIP1_V(WPE_TNR_DIP1_GALS_U1_CAM_P2, "wpe_tnr_dip1_gals_u1_cam_p2",
	        "wpe_tnr_dip1_gals_u1"/* parent */),
	GATE_HWV_WPE_TNR_DIP10(WPE_TNR_DIP1_WPE_MACRO, "wpe_tnr_dip1_wpe_macro",
			"mm_img1_ck"/* parent */, 4),
	GATE_WPE_TNR_DIP1_V(WPE_TNR_DIP1_WPE_MACRO_CAM_P2, "wpe_tnr_dip1_wpe_macro_cam_p2",
	        "wpe_tnr_dip1_wpe_macro"/* parent */),
	GATE_HWV_WPE_TNR_DIP10(WPE_TNR_DIP1_WPE, "wpe_tnr_dip1_wpe",
			"mm_img1_ck"/* parent */, 5),
	GATE_WPE_TNR_DIP1_V(WPE_TNR_DIP1_WPE_CAM_P2, "wpe_tnr_dip1_wpe_cam_p2",
	        "wpe_tnr_dip1_wpe"/* parent */),
	GATE_HWV_WPE_TNR_DIP10(WPE_TNR_DIP1_PQDIP, "wpe_tnr_dip1_pqdip",
			"mm_img1_ck"/* parent */, 6),
	GATE_WPE_TNR_DIP1_V(WPE_TNR_DIP1_PQDIP_CAM_P2, "wpe_tnr_dip1_pqdip_cam_p2",
	        "wpe_tnr_dip1_pqdip"/* parent */),
	GATE_HWV_WPE_TNR_DIP10(WPE_TNR_DIP1_PQDIP_DMA, "wpe_tnr_dip1_pqdip_dma",
			"mm_img1_ck"/* parent */, 7),
	GATE_WPE_TNR_DIP1_V(WPE_TNR_DIP1_PQDIP_DMA_CAM_P2, "wpe_tnr_dip1_pqdip_dma_cam_p2",
	        "wpe_tnr_dip1_pqdip_dma"/* parent */),
	GATE_HWV_WPE_TNR_DIP10(WPE_TNR_DIP1_OMC, "wpe_tnr_dip1_omc",
			"mm_img1_ck"/* parent */, 8),
	GATE_WPE_TNR_DIP1_V(WPE_TNR_DIP1_OMC_CAM_P2, "wpe_tnr_dip1_omc_cam_p2",
	        "wpe_tnr_dip1_omc"/* parent */),
	GATE_HWV_WPE_TNR_DIP10(WPE_TNR_DIP1_DPE, "wpe_tnr_dip1_dpe",
			"mm_img1_ck"/* parent */, 9),
	GATE_WPE_TNR_DIP1_V(WPE_TNR_DIP1_DPE_CAM_P2, "wpe_tnr_dip1_dpe_cam_p2",
	        "wpe_tnr_dip1_dpe"/* parent */),
	GATE_HWV_WPE_TNR_DIP10(WPE_TNR_DIP1_DFPS, "wpe_tnr_dip1_dfps",
			"mm_img1_ck"/* parent */, 10),
	GATE_WPE_TNR_DIP1_V(WPE_TNR_DIP1_DFPS_CAM_P2, "wpe_tnr_dip1_dfps_cam_p2",
	        "wpe_tnr_dip1_dfps"/* parent */),
	GATE_HWV_WPE_TNR_DIP10(WPE_TNR_DIP1_DFP0, "wpe_tnr_dip1_dfp0",
			"mm_img1_ck"/* parent */, 11),
	GATE_WPE_TNR_DIP1_V(WPE_TNR_DIP1_DFP0_CAM_P2, "wpe_tnr_dip1_dfp0_cam_p2",
	        "wpe_tnr_dip1_dfp0"/* parent */),
	GATE_HWV_WPE_TNR_DIP10(WPE_TNR_DIP1_DFP1, "wpe_tnr_dip1_dfp1",
			"mm_img1_ck"/* parent */, 12),
	GATE_WPE_TNR_DIP1_V(WPE_TNR_DIP1_DFP1_CAM_P2, "wpe_tnr_dip1_dfp1_cam_p2",
	        "wpe_tnr_dip1_dfp1"/* parent */),
	GATE_HWV_WPE_TNR_DIP10(WPE_TNR_DIP1_DWPE, "wpe_tnr_dip1_dwpe",
			"mm_img1_ck"/* parent */, 13),
	GATE_WPE_TNR_DIP1_V(WPE_TNR_DIP1_DWPE_CAM_P2, "wpe_tnr_dip1_dwpe_cam_p2",
	        "wpe_tnr_dip1_dwpe"/* parent */),
	GATE_HWV_WPE_TNR_DIP10(WPE_TNR_DIP1_ME, "wpe_tnr_dip1_me",
			"mm_img1_ck"/* parent */, 14),
	GATE_WPE_TNR_DIP1_V(WPE_TNR_DIP1_ME_CAM_P2, "wpe_tnr_dip1_me_cam_p2",
	        "wpe_tnr_dip1_me"/* parent */),
	GATE_HWV_WPE_TNR_DIP10(WPE_TNR_DIP1_MMG, "wpe_tnr_dip1_mmg",
			"mm_img1_ck"/* parent */, 15),
	GATE_WPE_TNR_DIP1_V(WPE_TNR_DIP1_MMG_CAM_P2, "wpe_tnr_dip1_mmg_cam_p2",
	        "wpe_tnr_dip1_mmg"/* parent */),
};

static const struct mtk_clk_desc wpe_tnr_dip1_mcd = {
	.clks = wpe_tnr_dip1_clks,
	.num_clks = CLK_WPE_TNR_DIP1_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6993_img[] = {
	{
		.compatible = "mediatek,mt6993-dip_cine_dip1",
		.data = &dip_cine_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6993-dip_nr1_dip1",
		.data = &dip_nr1_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6993-dip_nr2_dip1",
		.data = &dip_nr2_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6993-dip_top_dip1",
		.data = &dip_top_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6993-imgsys_main",
		.data = &img_mcd,
	}, {
		.compatible = "mediatek,mt6993-img_vcore_d1a",
		.data = &img_v_mcd,
	}, {
		.compatible = "mediatek,mt6993-traw_cap_dip1",
		.data = &traw_cap_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6993-traw_dip1",
		.data = &traw_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6993-wpe_eis_dip1",
		.data = &wpe_eis_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6993-wpe_lite_dip1",
		.data = &wpe_lite_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6993-wpe_tnr_dip1",
		.data = &wpe_tnr_dip1_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6993_img_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6993_img_drv = {
	.probe = clk_mt6993_img_grp_probe,
	.driver = {
		.name = "clk-mt6993-img",
		.of_match_table = of_match_clk_mt6993_img,
	},
};

module_platform_driver(clk_mt6993_img_drv);
MODULE_LICENSE("GPL");
