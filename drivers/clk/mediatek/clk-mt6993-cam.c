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

static const struct mtk_gate_regs cam_mr_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs cam_mr0_hwv_regs = {
	.set_ofs = 0x198,
	.clr_ofs = 0x19c,
	.sta_ofs = 0x12688,
};

#define GATE_CAM_MR(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_mr_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_CAM_MR_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_CAM_MR0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &cam_mr_cg_regs,			\
		.hwv_regs = &cam_mr0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate cam_mr_clks[] = {
	GATE_HWV_CAM_MR0(CAM_MR_LARB13, "cam_mr_larb13",
			"mm_cam_ck"/* parent */, 0),
	GATE_CAM_MR_V(CAM_MR_LARB13_CAMSV, "cam_mr_larb13_camsv",
	        "cam_mr_larb13"/* parent */),
	GATE_CAM_MR_V(CAM_MR_LARB13_SMI, "cam_mr_larb13_smi",
	        "cam_mr_larb13"/* parent */),
	GATE_HWV_CAM_MR0(CAM_MR_LARB14, "cam_mr_larb14",
			"mm_cam_ck"/* parent */, 1),
	GATE_CAM_MR_V(CAM_MR_LARB14_CAMSV, "cam_mr_larb14_camsv",
	        "cam_mr_larb14"/* parent */),
	GATE_CAM_MR_V(CAM_MR_LARB14_SMI, "cam_mr_larb14_smi",
	        "cam_mr_larb14"/* parent */),
	GATE_HWV_CAM_MR0(CAM_MR_LARB19, "cam_mr_larb19",
			"mm_cam_ck"/* parent */, 2),
	GATE_CAM_MR_V(CAM_MR_LARB19_CAMSV, "cam_mr_larb19_camsv",
	        "cam_mr_larb19"/* parent */),
	GATE_CAM_MR_V(CAM_MR_LARB19_SMI, "cam_mr_larb19_smi",
	        "cam_mr_larb19"/* parent */),
	GATE_HWV_CAM_MR0(CAM_MR_LARB25, "cam_mr_larb25",
			"mm_cam_ck"/* parent */, 3),
	GATE_CAM_MR_V(CAM_MR_LARB25_CAMSV, "cam_mr_larb25_camsv",
	        "cam_mr_larb25"/* parent */),
	GATE_CAM_MR_V(CAM_MR_LARB25_SMI, "cam_mr_larb25_smi",
	        "cam_mr_larb25"/* parent */),
	GATE_HWV_CAM_MR0(CAM_MR_LARB26, "cam_mr_larb26",
			"mm_cam_ck"/* parent */, 4),
	GATE_CAM_MR_V(CAM_MR_LARB26_CAMSV, "cam_mr_larb26_camsv",
	        "cam_mr_larb26"/* parent */),
	GATE_CAM_MR_V(CAM_MR_LARB26_SMI, "cam_mr_larb26_smi",
	        "cam_mr_larb26"/* parent */),
	GATE_HWV_CAM_MR0(CAM_MR_LARB29, "cam_mr_larb29",
			"mm_cam_ck"/* parent */, 5),
	GATE_CAM_MR_V(CAM_MR_LARB29_CAMSV, "cam_mr_larb29_camsv",
	        "cam_mr_larb29"/* parent */),
	GATE_CAM_MR_V(CAM_MR_LARB29_SMI, "cam_mr_larb29_smi",
	        "cam_mr_larb29"/* parent */),
	GATE_HWV_CAM_MR0(CAM_MR_GALS0, "cam_mr_gals0",
			"mm_cam_ck"/* parent */, 6),
	GATE_CAM_MR_V(CAM_MR_GALS0_CAMSV, "cam_mr_gals0_camsv",
	        "cam_mr_gals0"/* parent */),
	GATE_HWV_CAM_MR0(CAM_MR_GALS1, "cam_mr_gals1",
			"mm_cam_ck"/* parent */, 7),
	GATE_CAM_MR_V(CAM_MR_GALS1_CAMSV, "cam_mr_gals1_camsv",
	        "cam_mr_gals1"/* parent */),
	GATE_HWV_CAM_MR0(CAM_MR_GALS2, "cam_mr_gals2",
			"mm_cam_ck"/* parent */, 8),
	GATE_CAM_MR_V(CAM_MR_GALS2_CAMSV, "cam_mr_gals2_camsv",
	        "cam_mr_gals2"/* parent */),
	GATE_HWV_CAM_MR0(CAM_MR_GALS3, "cam_mr_gals3",
			"mm_cam_ck"/* parent */, 9),
	GATE_CAM_MR_V(CAM_MR_GALS3_CAMSV, "cam_mr_gals3_camsv",
	        "cam_mr_gals3"/* parent */),
	GATE_HWV_CAM_MR0(CAM_MR_GALS4, "cam_mr_gals4",
			"mm_cam_ck"/* parent */, 10),
	GATE_CAM_MR_V(CAM_MR_GALS4_CAMSV, "cam_mr_gals4_camsv",
	        "cam_mr_gals4"/* parent */),
	GATE_HWV_CAM_MR0(CAM_MR_GALS5, "cam_mr_gals5",
			"mm_cam_ck"/* parent */, 11),
	GATE_CAM_MR_V(CAM_MR_GALS5_CAMSV, "cam_mr_gals5_camsv",
	        "cam_mr_gals5"/* parent */),
	GATE_HWV_CAM_MR0(CAM_MR_SENINF_CAMTM, "cam_mr_seninf_camtm",
			"mm_camtm_ck"/* parent */, 12),
	GATE_CAM_MR_V(CAM_MR_SENINF_CAMTM_CAMSV, "cam_mr_seninf_camtm_camsv",
	        "cam_mr_seninf_camtm"/* parent */),
	GATE_HWV_CAM_MR0(CAM_MR_CAMSV_TOP, "cam_mr_camsv_top",
			"mm_cam_ck"/* parent */, 13),
	GATE_CAM_MR_V(CAM_MR_CAMSV_TOP_CAMSV, "cam_mr_camsv_top_camsv",
	        "cam_mr_camsv_top"/* parent */),
	GATE_HWV_CAM_MR0(CAM_MR_CAMSV_A, "cam_mr_camsv_a",
			"mm_cam_ck"/* parent */, 14),
	GATE_CAM_MR_V(CAM_MR_CAMSV_A_CAMSV, "cam_mr_camsv_a_camsv",
	        "cam_mr_camsv_a"/* parent */),
	GATE_HWV_CAM_MR0(CAM_MR_CAMSV_B, "cam_mr_camsv_b",
			"mm_cam_ck"/* parent */, 15),
	GATE_CAM_MR_V(CAM_MR_CAMSV_B_CAMSV, "cam_mr_camsv_b_camsv",
	        "cam_mr_camsv_b"/* parent */),
	GATE_HWV_CAM_MR0(CAM_MR_CAMSV_C, "cam_mr_camsv_c",
			"mm_cam_ck"/* parent */, 16),
	GATE_CAM_MR_V(CAM_MR_CAMSV_C_CAMSV, "cam_mr_camsv_c_camsv",
	        "cam_mr_camsv_c"/* parent */),
	GATE_HWV_CAM_MR0(CAM_MR_CAMSV_D, "cam_mr_camsv_d",
			"mm_cam_ck"/* parent */, 17),
	GATE_CAM_MR_V(CAM_MR_CAMSV_D_CAMSV, "cam_mr_camsv_d_camsv",
	        "cam_mr_camsv_d"/* parent */),
	GATE_HWV_CAM_MR0(CAM_MR_CAMSV_E, "cam_mr_camsv_e",
			"mm_cam_ck"/* parent */, 18),
	GATE_CAM_MR_V(CAM_MR_CAMSV_E_CAMSV, "cam_mr_camsv_e_camsv",
	        "cam_mr_camsv_e"/* parent */),
	GATE_HWV_CAM_MR0(CAM_MR_CAMSV, "cam_mr_camsv",
			"mm_cam_ck"/* parent */, 19),
	GATE_CAM_MR_V(CAM_MR_CAMSV_CAMSV, "cam_mr_camsv_camsv",
	        "cam_mr_camsv"/* parent */),
	GATE_HWV_CAM_MR0(CAM_MR_PDA0, "cam_mr_pda0",
			"mm_cam_ck"/* parent */, 20),
	GATE_CAM_MR_V(CAM_MR_PDA0_PDA, "cam_mr_pda0_pda",
	        "cam_mr_pda0"/* parent */),
	GATE_HWV_CAM_MR0(CAM_MR_PDA1, "cam_mr_pda1",
			"mm_cam_ck"/* parent */, 21),
	GATE_CAM_MR_V(CAM_MR_PDA1_PDA, "cam_mr_pda1_pda",
	        "cam_mr_pda1"/* parent */),
};

static const struct mtk_clk_desc cam_mr_mcd = {
	.clks = cam_mr_clks,
	.num_clks = CLK_CAM_MR_NR_CLK,
};

static const struct mtk_gate_regs cam_ra_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs cam_ra0_hwv_regs = {
	.set_ofs = 0x120,
	.clr_ofs = 0x124,
	.sta_ofs = 0x12660,
};

#define GATE_CAM_RA(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_ra_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_CAM_RA_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_CAM_RA0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &cam_ra_cg_regs,			\
		.hwv_regs = &cam_ra0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate cam_ra_clks[] = {
	GATE_HWV_CAM_RA0(CAM_RA_LARBX, "cam_ra_larbx",
			"mm_cam_ck"/* parent */, 0),
	GATE_CAM_RA_V(CAM_RA_LARBX_CAM_RAWA, "cam_ra_larbx_cam_rawa",
	        "cam_ra_larbx"/* parent */),
	GATE_CAM_RA_V(CAM_RA_LARBX_SMI, "cam_ra_larbx_smi",
	        "cam_ra_larbx"/* parent */),
	GATE_HWV_CAM_RA0(CAM_RA_CAM, "cam_ra_cam",
			"mm_cam_ck"/* parent */, 1),
	GATE_CAM_RA_V(CAM_RA_CAM_CAM_RAWA, "cam_ra_cam_cam_rawa",
	        "cam_ra_cam"/* parent */),
	GATE_CAM_RA_V(CAM_RA_CAM_SMI, "cam_ra_cam_smi",
	        "cam_ra_cam"/* parent */),
	GATE_HWV_CAM_RA0(CAM_RA_CAMTG, "cam_ra_camtg",
			"mm_cam_ck"/* parent */, 2),
	GATE_CAM_RA_V(CAM_RA_CAMTG_CAM_RAWA, "cam_ra_camtg_cam_rawa",
	        "cam_ra_camtg"/* parent */),
	GATE_HWV_CAM_RA0(CAM_RA_RAW2MM_GALS, "cam_ra_raw2mm_gals",
			"mm_cam_ck"/* parent */, 3),
	GATE_CAM_RA_V(CAM_RA_RAW2MM_GALS_CAM_RAWA, "cam_ra_raw2mm_gals_cam_rawa",
	        "cam_ra_raw2mm_gals"/* parent */),
	GATE_HWV_CAM_RA0(CAM_RA_YUV2RAW2MM_GALS, "cam_ra_yuv2raw2mm",
			"mm_cam_ck"/* parent */, 4),
	GATE_CAM_RA_V(CAM_RA_YUV2RAW2MM_GALS_CAM_RAWA, "cam_ra_yuv2raw2mm_cam_rawa",
	        "cam_ra_yuv2raw2mm"/* parent */),
	GATE_HWV_CAM_RA0(CAM_RA_CAM_26M, "cam_ra_cam_26m",
			"mm_cam_ck"/* parent */, 5),
	GATE_CAM_RA_V(CAM_RA_CAM_26M_CAM_RAWA, "cam_ra_cam_26m_cam_rawa",
	        "cam_ra_cam_26m"/* parent */),
};

static const struct mtk_clk_desc cam_ra_mcd = {
	.clks = cam_ra_clks,
	.num_clks = CLK_CAM_RA_NR_CLK,
};

static const struct mtk_gate_regs cam_rb_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs cam_rb0_hwv_regs = {
	.set_ofs = 0x144,
	.clr_ofs = 0x148,
	.sta_ofs = 0x1266C,
};

#define GATE_CAM_RB(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_rb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_CAM_RB_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_CAM_RB0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &cam_rb_cg_regs,			\
		.hwv_regs = &cam_rb0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate cam_rb_clks[] = {
	GATE_HWV_CAM_RB0(CAM_RB_LARBX, "cam_rb_larbx",
			"mm_cam_ck"/* parent */, 0),
	GATE_CAM_RB_V(CAM_RB_LARBX_CAM_RAWB, "cam_rb_larbx_cam_rawb",
	        "cam_rb_larbx"/* parent */),
	GATE_CAM_RB_V(CAM_RB_LARBX_SMI, "cam_rb_larbx_smi",
	        "cam_rb_larbx"/* parent */),
	GATE_HWV_CAM_RB0(CAM_RB_CAM, "cam_rb_cam",
			"mm_cam_ck"/* parent */, 1),
	GATE_CAM_RB_V(CAM_RB_CAM_CAM_RAWB, "cam_rb_cam_cam_rawb",
	        "cam_rb_cam"/* parent */),
	GATE_CAM_RB_V(CAM_RB_CAM_SMI, "cam_rb_cam_smi",
	        "cam_rb_cam"/* parent */),
	GATE_HWV_CAM_RB0(CAM_RB_CAMTG, "cam_rb_camtg",
			"mm_cam_ck"/* parent */, 2),
	GATE_CAM_RB_V(CAM_RB_CAMTG_CAM_RAWB, "cam_rb_camtg_cam_rawb",
	        "cam_rb_camtg"/* parent */),
	GATE_HWV_CAM_RB0(CAM_RB_RAW2MM_GALS, "cam_rb_raw2mm_gals",
			"mm_cam_ck"/* parent */, 3),
	GATE_CAM_RB_V(CAM_RB_RAW2MM_GALS_CAM_RAWB, "cam_rb_raw2mm_gals_cam_rawb",
	        "cam_rb_raw2mm_gals"/* parent */),
	GATE_HWV_CAM_RB0(CAM_RB_YUV2RAW2MM_GALS, "cam_rb_yuv2raw2mm",
			"mm_cam_ck"/* parent */, 4),
	GATE_CAM_RB_V(CAM_RB_YUV2RAW2MM_GALS_CAM_RAWB, "cam_rb_yuv2raw2mm_cam_rawb",
	        "cam_rb_yuv2raw2mm"/* parent */),
	GATE_HWV_CAM_RB0(CAM_RB_CAM_26M, "cam_rb_cam_26m",
			"mm_cam_ck"/* parent */, 5),
	GATE_CAM_RB_V(CAM_RB_CAM_26M_CAM_RAWB, "cam_rb_cam_26m_cam_rawb",
	        "cam_rb_cam_26m"/* parent */),
};

static const struct mtk_clk_desc cam_rb_mcd = {
	.clks = cam_rb_clks,
	.num_clks = CLK_CAM_RB_NR_CLK,
};

static const struct mtk_gate_regs cam_rc_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs cam_rc0_hwv_regs = {
	.set_ofs = 0x168,
	.clr_ofs = 0x16c,
	.sta_ofs = 0x12678,
};

#define GATE_CAM_RC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_rc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_CAM_RC_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_CAM_RC0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &cam_rc_cg_regs,			\
		.hwv_regs = &cam_rc0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate cam_rc_clks[] = {
	GATE_HWV_CAM_RC0(CAM_RC_LARBX, "cam_rc_larbx",
			"mm_cam_ck"/* parent */, 0),
	GATE_CAM_RC_V(CAM_RC_LARBX_CAM_RAWC, "cam_rc_larbx_cam_rawc",
	        "cam_rc_larbx"/* parent */),
	GATE_CAM_RC_V(CAM_RC_LARBX_SMI, "cam_rc_larbx_smi",
	        "cam_rc_larbx"/* parent */),
	GATE_HWV_CAM_RC0(CAM_RC_CAM, "cam_rc_cam",
			"mm_cam_ck"/* parent */, 1),
	GATE_CAM_RC_V(CAM_RC_CAM_CAM_RAWC, "cam_rc_cam_cam_rawc",
	        "cam_rc_cam"/* parent */),
	GATE_CAM_RC_V(CAM_RC_CAM_SMI, "cam_rc_cam_smi",
	        "cam_rc_cam"/* parent */),
	GATE_HWV_CAM_RC0(CAM_RC_CAMTG, "cam_rc_camtg",
			"mm_cam_ck"/* parent */, 2),
	GATE_CAM_RC_V(CAM_RC_CAMTG_CAM_RAWC, "cam_rc_camtg_cam_rawc",
	        "cam_rc_camtg"/* parent */),
	GATE_HWV_CAM_RC0(CAM_RC_RAW2MM_GALS, "cam_rc_raw2mm_gals",
			"mm_cam_ck"/* parent */, 3),
	GATE_CAM_RC_V(CAM_RC_RAW2MM_GALS_CAM_RAWC, "cam_rc_raw2mm_gals_cam_rawc",
	        "cam_rc_raw2mm_gals"/* parent */),
	GATE_HWV_CAM_RC0(CAM_RC_YUV2RAW2MM_GALS, "cam_rc_yuv2raw2mm",
			"mm_cam_ck"/* parent */, 4),
	GATE_CAM_RC_V(CAM_RC_YUV2RAW2MM_GALS_CAM_RAWC, "cam_rc_yuv2raw2mm_cam_rawc",
	        "cam_rc_yuv2raw2mm"/* parent */),
	GATE_HWV_CAM_RC0(CAM_RC_CAM_26M, "cam_rc_cam_26m",
			"mm_cam_ck"/* parent */, 5),
	GATE_CAM_RC_V(CAM_RC_CAM_26M_CAM_RAWC, "cam_rc_cam_26m_cam_rawc",
	        "cam_rc_cam_26m"/* parent */),
};

static const struct mtk_clk_desc cam_rc_mcd = {
	.clks = cam_rc_clks,
	.num_clks = CLK_CAM_RC_NR_CLK,
};

static const struct mtk_gate_regs camsys_rmsa_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs camsys_rmsa0_hwv_regs = {
	.set_ofs = 0x12c,
	.clr_ofs = 0x130,
	.sta_ofs = 0x12664,
};

#define GATE_CAMSYS_RMSA(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &camsys_rmsa_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_CAMSYS_RMSA_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_CAMSYS_RMSA0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &camsys_rmsa_cg_regs,			\
		.hwv_regs = &camsys_rmsa0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate camsys_rmsa_clks[] = {
	GATE_HWV_CAMSYS_RMSA0(CAMSYS_RMSA_LARBX, "camsys_rmsa_larbx",
			"mm_cam_ck"/* parent */, 0),
	GATE_CAMSYS_RMSA_V(CAMSYS_RMSA_LARBX_CAM_RMSA, "camsys_rmsa_larbx_cam_rmsa",
	        "camsys_rmsa_larbx"/* parent */),
	GATE_HWV_CAMSYS_RMSA0(CAMSYS_RMSA_CAM, "camsys_rmsa_cam",
			"mm_cam_ck"/* parent */, 1),
	GATE_CAMSYS_RMSA_V(CAMSYS_RMSA_CAM_CAM_RMSA, "camsys_rmsa_cam_cam_rmsa",
	        "camsys_rmsa_cam"/* parent */),
	GATE_HWV_CAMSYS_RMSA0(CAMSYS_RMSA_CAMTG, "camsys_rmsa_camtg",
			"mm_cam_ck"/* parent */, 2),
	GATE_CAMSYS_RMSA_V(CAMSYS_RMSA_CAMTG_CAM_RMSA, "camsys_rmsa_camtg_cam_rmsa",
	        "camsys_rmsa_camtg"/* parent */),
};

static const struct mtk_clk_desc camsys_rmsa_mcd = {
	.clks = camsys_rmsa_clks,
	.num_clks = CLK_CAMSYS_RMSA_NR_CLK,
};

static const struct mtk_gate_regs camsys_rmsb_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs camsys_rmsb0_hwv_regs = {
	.set_ofs = 0x150,
	.clr_ofs = 0x154,
	.sta_ofs = 0x12670,
};

#define GATE_CAMSYS_RMSB(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &camsys_rmsb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_CAMSYS_RMSB_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_CAMSYS_RMSB0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &camsys_rmsb_cg_regs,			\
		.hwv_regs = &camsys_rmsb0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate camsys_rmsb_clks[] = {
	GATE_HWV_CAMSYS_RMSB0(CAMSYS_RMSB_LARBX, "camsys_rmsb_larbx",
			"mm_cam_ck"/* parent */, 0),
	GATE_CAMSYS_RMSB_V(CAMSYS_RMSB_LARBX_CAM_RMSB, "camsys_rmsb_larbx_cam_rmsb",
	        "camsys_rmsb_larbx"/* parent */),
	GATE_HWV_CAMSYS_RMSB0(CAMSYS_RMSB_CAM, "camsys_rmsb_cam",
			"mm_cam_ck"/* parent */, 1),
	GATE_CAMSYS_RMSB_V(CAMSYS_RMSB_CAM_CAM_RMSB, "camsys_rmsb_cam_cam_rmsb",
	        "camsys_rmsb_cam"/* parent */),
	GATE_HWV_CAMSYS_RMSB0(CAMSYS_RMSB_CAMTG, "camsys_rmsb_camtg",
			"mm_cam_ck"/* parent */, 2),
	GATE_CAMSYS_RMSB_V(CAMSYS_RMSB_CAMTG_CAM_RMSB, "camsys_rmsb_camtg_cam_rmsb",
	        "camsys_rmsb_camtg"/* parent */),
};

static const struct mtk_clk_desc camsys_rmsb_mcd = {
	.clks = camsys_rmsb_clks,
	.num_clks = CLK_CAMSYS_RMSB_NR_CLK,
};

static const struct mtk_gate_regs camsys_rmsc_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs camsys_rmsc0_hwv_regs = {
	.set_ofs = 0x174,
	.clr_ofs = 0x178,
	.sta_ofs = 0x1267C,
};

#define GATE_CAMSYS_RMSC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &camsys_rmsc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_CAMSYS_RMSC_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_CAMSYS_RMSC0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &camsys_rmsc_cg_regs,			\
		.hwv_regs = &camsys_rmsc0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate camsys_rmsc_clks[] = {
	GATE_HWV_CAMSYS_RMSC0(CAMSYS_RMSC_LARBX, "camsys_rmsc_larbx",
			"mm_cam_ck"/* parent */, 0),
	GATE_CAMSYS_RMSC_V(CAMSYS_RMSC_LARBX_CAM_RMSC, "camsys_rmsc_larbx_cam_rmsc",
	        "camsys_rmsc_larbx"/* parent */),
	GATE_HWV_CAMSYS_RMSC0(CAMSYS_RMSC_CAM, "camsys_rmsc_cam",
			"mm_cam_ck"/* parent */, 1),
	GATE_CAMSYS_RMSC_V(CAMSYS_RMSC_CAM_CAM_RMSC, "camsys_rmsc_cam_cam_rmsc",
	        "camsys_rmsc_cam"/* parent */),
	GATE_HWV_CAMSYS_RMSC0(CAMSYS_RMSC_CAMTG, "camsys_rmsc_camtg",
			"mm_cam_ck"/* parent */, 2),
	GATE_CAMSYS_RMSC_V(CAMSYS_RMSC_CAMTG_CAM_RMSC, "camsys_rmsc_camtg_cam_rmsc",
	        "camsys_rmsc_camtg"/* parent */),
};

static const struct mtk_clk_desc camsys_rmsc_mcd = {
	.clks = camsys_rmsc_clks,
	.num_clks = CLK_CAMSYS_RMSC_NR_CLK,
};

static const struct mtk_gate_regs cam_ya_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs cam_ya0_hwv_regs = {
	.set_ofs = 0x138,
	.clr_ofs = 0x13c,
	.sta_ofs = 0x12668,
};

#define GATE_CAM_YA(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_ya_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_CAM_YA_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_CAM_YA0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &cam_ya_cg_regs,			\
		.hwv_regs = &cam_ya0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate cam_ya_clks[] = {
	GATE_HWV_CAM_YA0(CAM_YA_LARBX, "cam_ya_larbx",
			"mm_cam_ck"/* parent */, 0),
	GATE_CAM_YA_V(CAM_YA_LARBX_CAM_YUVA, "cam_ya_larbx_cam_yuva",
	        "cam_ya_larbx"/* parent */),
	GATE_CAM_YA_V(CAM_YA_LARBX_SMI, "cam_ya_larbx_smi",
	        "cam_ya_larbx"/* parent */),
	GATE_HWV_CAM_YA0(CAM_YA_CAM, "cam_ya_cam",
			"mm_cam_ck"/* parent */, 1),
	GATE_CAM_YA_V(CAM_YA_CAM_CAM_YUVA, "cam_ya_cam_cam_yuva",
	        "cam_ya_cam"/* parent */),
	GATE_HWV_CAM_YA0(CAM_YA_CAMTG, "cam_ya_camtg",
			"mm_cam_ck"/* parent */, 2),
	GATE_CAM_YA_V(CAM_YA_CAMTG_CAM_YUVA, "cam_ya_camtg_cam_yuva",
	        "cam_ya_camtg"/* parent */),
};

static const struct mtk_clk_desc cam_ya_mcd = {
	.clks = cam_ya_clks,
	.num_clks = CLK_CAM_YA_NR_CLK,
};

static const struct mtk_gate_regs cam_yb_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs cam_yb0_hwv_regs = {
	.set_ofs = 0x15c,
	.clr_ofs = 0x160,
	.sta_ofs = 0x12674,
};

#define GATE_CAM_YB(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_yb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_CAM_YB_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_CAM_YB0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &cam_yb_cg_regs,			\
		.hwv_regs = &cam_yb0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate cam_yb_clks[] = {
	GATE_HWV_CAM_YB0(CAM_YB_LARBX, "cam_yb_larbx",
			"mm_cam_ck"/* parent */, 0),
	GATE_CAM_YB_V(CAM_YB_LARBX_CAM_YUVB, "cam_yb_larbx_cam_yuvb",
	        "cam_yb_larbx"/* parent */),
	GATE_CAM_YB_V(CAM_YB_LARBX_SMI, "cam_yb_larbx_smi",
	        "cam_yb_larbx"/* parent */),
	GATE_HWV_CAM_YB0(CAM_YB_CAM, "cam_yb_cam",
			"mm_cam_ck"/* parent */, 1),
	GATE_CAM_YB_V(CAM_YB_CAM_CAM_YUVB, "cam_yb_cam_cam_yuvb",
	        "cam_yb_cam"/* parent */),
	GATE_HWV_CAM_YB0(CAM_YB_CAMTG, "cam_yb_camtg",
			"mm_cam_ck"/* parent */, 2),
	GATE_CAM_YB_V(CAM_YB_CAMTG_CAM_YUVB, "cam_yb_camtg_cam_yuvb",
	        "cam_yb_camtg"/* parent */),
};

static const struct mtk_clk_desc cam_yb_mcd = {
	.clks = cam_yb_clks,
	.num_clks = CLK_CAM_YB_NR_CLK,
};

static const struct mtk_gate_regs cam_yc_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs cam_yc0_hwv_regs = {
	.set_ofs = 0x180,
	.clr_ofs = 0x184,
	.sta_ofs = 0x12680,
};

#define GATE_CAM_YC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_yc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_CAM_YC_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_CAM_YC0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &cam_yc_cg_regs,			\
		.hwv_regs = &cam_yc0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate cam_yc_clks[] = {
	GATE_HWV_CAM_YC0(CAM_YC_LARBX, "cam_yc_larbx",
			"mm_cam_ck"/* parent */, 0),
	GATE_CAM_YC_V(CAM_YC_LARBX_CAM_YUVC, "cam_yc_larbx_cam_yuvc",
	        "cam_yc_larbx"/* parent */),
	GATE_CAM_YC_V(CAM_YC_LARBX_SMI, "cam_yc_larbx_smi",
	        "cam_yc_larbx"/* parent */),
	GATE_HWV_CAM_YC0(CAM_YC_CAM, "cam_yc_cam",
			"mm_cam_ck"/* parent */, 1),
	GATE_CAM_YC_V(CAM_YC_CAM_CAM_YUVC, "cam_yc_cam_cam_yuvc",
	        "cam_yc_cam"/* parent */),
	GATE_HWV_CAM_YC0(CAM_YC_CAMTG, "cam_yc_camtg",
			"mm_cam_ck"/* parent */, 2),
	GATE_CAM_YC_V(CAM_YC_CAMTG_CAM_YUVC, "cam_yc_camtg_cam_yuvc",
	        "cam_yc_camtg"/* parent */),
};

static const struct mtk_clk_desc cam_yc_mcd = {
	.clks = cam_yc_clks,
	.num_clks = CLK_CAM_YC_NR_CLK,
};

static const struct mtk_gate_regs cam_m0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs cam_m0_hwv_regs = {
	.set_ofs = 0x108,
	.clr_ofs = 0x10c,
	.sta_ofs = 0x12658,
};

static const struct mtk_gate_regs cam_m1_cg_regs = {
	.set_ofs = 0x50,
	.clr_ofs = 0x54,
	.sta_ofs = 0x4C,
};

static const struct mtk_gate_regs cam_m1_hwv_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x1265C,
};

#define GATE_CAM_M0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_m0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_CAM_M0_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_CAM_M0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &cam_m0_cg_regs,			\
		.hwv_regs = &cam_m0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

#define GATE_CAM_M1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_m1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_CAM_M1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_CAM_M1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &cam_m1_cg_regs,			\
		.hwv_regs = &cam_m1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate cam_m_clks[] = {
	/* CAM_M0 */
	GATE_HWV_CAM_M0(CAM_M_LARB27, "cam_m_larb27",
			"mm_cam_ck"/* parent */, 0),
	GATE_CAM_M0_V(CAM_M_LARB27_CAM_MAIN, "cam_m_larb27_cam_main",
	        "cam_m_larb27"/* parent */),
	GATE_CAM_M0_V(CAM_M_LARB27_SMI, "cam_m_larb27_smi",
	        "cam_m_larb27"/* parent */),
	GATE_HWV_CAM_M0(CAM_M_CAM, "cam_m_cam",
			"mm_cam_ck"/* parent */, 1),
	GATE_CAM_M0_V(CAM_M_CAM_CAM_MAIN, "cam_m_cam_cam_main",
	        "cam_m_cam"/* parent */),
	GATE_HWV_CAM_M0(CAM_M_CAM_SUBA, "cam_m_cam_suba",
			"mm_cam_ck"/* parent */, 2),
	GATE_CAM_M0_V(CAM_M_CAM_SUBA_CAM_MAIN, "cam_m_cam_suba_cam_main",
	        "cam_m_cam_suba"/* parent */),
	GATE_HWV_CAM_M0(CAM_M_CAM_SUBB, "cam_m_cam_subb",
			"mm_cam_ck"/* parent */, 3),
	GATE_CAM_M0_V(CAM_M_CAM_SUBB_CAM_MAIN, "cam_m_cam_subb_cam_main",
	        "cam_m_cam_subb"/* parent */),
	GATE_HWV_CAM_M0(CAM_M_CAM_SUBC, "cam_m_cam_subc",
			"mm_cam_ck"/* parent */, 4),
	GATE_CAM_M0_V(CAM_M_CAM_SUBC_CAM_MAIN, "cam_m_cam_subc_cam_main",
	        "cam_m_cam_subc"/* parent */),
	GATE_HWV_CAM_M0(CAM_M_CAM_MRAW, "cam_m_cam_mraw",
			"mm_cam_ck"/* parent */, 5),
	GATE_CAM_M0_V(CAM_M_CAM_MRAW_CAM_MAIN, "cam_m_cam_mraw_cam_main",
	        "cam_m_cam_mraw"/* parent */),
	GATE_HWV_CAM_M0(CAM_M_CAMTG, "cam_m_camtg",
			"mm_camtm_ck"/* parent */, 6),
	GATE_CAM_M0_V(CAM_M_CAMTG_CAM_MAIN, "cam_m_camtg_cam_main",
	        "cam_m_camtg"/* parent */),
	GATE_HWV_CAM_M0(CAM_M_SENINF, "cam_m_seninf",
			"mm_cam_ck"/* parent */, 7),
	GATE_CAM_M0_V(CAM_M_SENINF_CAM_MAIN, "cam_m_seninf_cam_main",
	        "cam_m_seninf"/* parent */),
	GATE_HWV_CAM_M0(CAM_M_ADLRD, "cam_m_adlrd",
			"mm_cam_ck"/* parent */, 8),
	GATE_CAM_M0_V(CAM_M_ADLRD_CAM_MAIN, "cam_m_adlrd_cam_main",
	        "cam_m_adlrd"/* parent */),
	GATE_HWV_CAM_M0(CAM_M_ADLWR, "cam_m_adlwr",
			"mm_cam_ck"/* parent */, 9),
	GATE_CAM_M0_V(CAM_M_ADLWR_CAM_MAIN, "cam_m_adlwr_cam_main",
	        "cam_m_adlwr"/* parent */),
	GATE_HWV_CAM_M0(CAM_M_UISP, "cam_m_uisp",
			"mm_cam_ck"/* parent */, 10),
	GATE_CAM_M0_V(CAM_M_UISP_CAM_MAIN, "cam_m_uisp_cam_main",
	        "cam_m_uisp"/* parent */),
	GATE_HWV_CAM_M0(CAM_M_SDL_0C_0, "cam_m_sdl_0c_0",
			"mm_cam_ck"/* parent */, 11),
	GATE_CAM_M0_V(CAM_M_SDL_0C_0_CAM_MAIN, "cam_m_sdl_0c_0_cam_main",
	        "cam_m_sdl_0c_0"/* parent */),
	GATE_HWV_CAM_M0(CAM_M_SDL_1, "cam_m_sdl_1",
			"mm_cam_ck"/* parent */, 12),
	GATE_CAM_M0_V(CAM_M_SDL_1_CAM_MAIN, "cam_m_sdl_1_cam_main",
	        "cam_m_sdl_1"/* parent */),
	GATE_HWV_CAM_M0(CAM_M_LARB27_GALS, "cam_m_larb27_gcon_0",
			"mm_cam_ck"/* parent */, 13),
	GATE_CAM_M0_V(CAM_M_LARB27_GALS_CAM_MAIN, "cam_m_larb27_gcon_0_cam_main",
	        "cam_m_larb27_gcon_0"/* parent */),
	GATE_CAM_M0_V(CAM_M_LARB27_GALS_SMI, "cam_m_larb27_gcon_0_smi",
	        "cam_m_larb27_gcon_0"/* parent */),
	GATE_HWV_CAM_M0(CAM_M_CAM2SYS_GALS, "cam_m_cam2sys_gcon_0",
			"mm_cam_ck"/* parent */, 14),
	GATE_CAM_M0_V(CAM_M_CAM2SYS_GALS_CAM_MAIN, "cam_m_cam2sys_gcon_0_cam_main",
	        "cam_m_cam2sys_gcon_0"/* parent */),
	GATE_HWV_CAM_M0(CAM_M_IPS, "cam_m_ips",
			"mm_cam_ck"/* parent */, 16),
	GATE_CAM_M0_V(CAM_M_IPS_CAM_MAIN, "cam_m_ips_cam_main",
	        "cam_m_ips"/* parent */),
	GATE_HWV_CAM_M0(CAM_M_CAM_ASG, "cam_m_cam_asg",
			"mm_cam_ck"/* parent */, 21),
	GATE_CAM_M0_V(CAM_M_CAM_ASG_CAM_MAIN, "cam_m_cam_asg_cam_main",
	        "cam_m_cam_asg"/* parent */),
	/* CAM_M1 */
	GATE_HWV_CAM_M1(CAM_M_CAM_QOF_CON_1, "cam_m_cam_qof_con_1",
			"mm_cam_ck"/* parent */, 0),
	GATE_CAM_M1_V(CAM_M_CAM_QOF_CON_1_CAM_MAIN, "cam_m_cam_qof_con_1_cam_main",
	        "cam_m_cam_qof_con_1"/* parent */),
	GATE_HWV_CAM_M1(CAM_M_CAM_BWR_CON_1, "cam_m_cam_bwr_con_1",
			"mm_cam_ck"/* parent */, 1),
	GATE_CAM_M1_V(CAM_M_CAM_BWR_CON_1_CAM_MAIN, "cam_m_cam_bwr_con_1_cam_main",
	        "cam_m_cam_bwr_con_1"/* parent */),
	GATE_HWV_CAM_M1(CAM_M_CAM_RTCQ_CON_1, "cam_m_cam_rtcq_con_1",
			"mm_cam_ck"/* parent */, 2),
	GATE_CAM_M1_V(CAM_M_CAM_RTCQ_CON_1_CAM_MAIN, "cam_m_cam_rtcq_con_1_cam_main",
	        "cam_m_cam_rtcq_con_1"/* parent */),
	GATE_HWV_CAM_M1(CAM_M_CAM_SDLCQ_CON_1, "cam_m_cam_sdlcq_con_1",
			"mm_cam_ck"/* parent */, 3),
	GATE_CAM_M1_V(CAM_M_CAM_SDLCQ_CON_1_CAM_MAIN, "cam_m_cam_sdlcq_con_1_cam_main",
	        "cam_m_cam_sdlcq_con_1"/* parent */),
	GATE_HWV_CAM_M1(CAM_M_CAM_WLA_CON_1, "cam_m_cam_wla_con_1",
			"mm_cam_ck"/* parent */, 4),
	GATE_CAM_M1_V(CAM_M_CAM_WLA_CON_1_CAM_MAIN, "cam_m_cam_wla_con_1_cam_main",
	        "cam_m_cam_wla_con_1"/* parent */),
	GATE_HWV_CAM_M1(CAM_M_CAM_DVC_CON_1, "cam_m_cam_dvc_con_1",
			"mm_cam_ck"/* parent */, 5),
	GATE_CAM_M1_V(CAM_M_CAM_DVC_CON_1_CAM_MAIN, "cam_m_cam_dvc_con_1_cam_main",
	        "cam_m_cam_dvc_con_1"/* parent */),
	GATE_HWV_CAM_M1(CAM_M_CAM_CVFS_CON_1, "cam_m_cam_cvfs_con_1",
			"cksys_cam_aov_26m_ck"/* parent */, 6),
	GATE_CAM_M1_V(CAM_M_CAM_CVFS_CON_1_CAM_MAIN, "cam_m_cam_cvfs_con_1_cam_main",
	        "cam_m_cam_cvfs_con_1"/* parent */),
};

static const struct mtk_clk_desc cam_m_mcd = {
	.clks = cam_m_clks,
	.num_clks = CLK_CAM_M_NR_CLK,
};

static const struct mtk_gate_regs cam_vcore_r1a0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs cam_vcore_r1a0_hwv_regs = {
	.set_ofs = 0x1a4,
	.clr_ofs = 0x1a8,
	.sta_ofs = 0x1268C,
};

static const struct mtk_gate_regs cam_vcore_r1a1_cg_regs = {
	.set_ofs = 0x14,
	.clr_ofs = 0x18,
	.sta_ofs = 0x10,
};

#define GATE_CAM_VCORE_R1A0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_vcore_r1a0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_CAM_VCORE_R1A0_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_CAM_VCORE_R1A0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &cam_vcore_r1a0_cg_regs,			\
		.hwv_regs = &cam_vcore_r1a0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

#define GATE_CAM_VCORE_R1A1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_vcore_r1a1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA,		\
	}

#define GATE_CAM_VCORE_R1A1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

static const struct mtk_gate cam_vcore_r1a_clks[] = {
	/* CAM_VCORE_R1A0 */
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_G, "cam_v_r1a_g",
			"mm_mminfra_ck"/* parent */, 0),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_G_CAM_VCORE, "cam_v_r1a_g_cam_vcore",
	        "cam_v_r1a_g"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_26M, "cam_v_r1a_26m",
			"cksys_cam_aov_26m_ck"/* parent */, 1),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_26M_CAM_VCORE, "cam_v_r1a_26m_cam_vcore",
	        "cam_v_r1a_26m"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_BLS_PART, "cam_v_r1a_bls_part",
			"mm_mminfra_ck"/* parent */, 2),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_BLS_PART_CAM_VCORE, "cam_v_r1a_bls_part_cam_vcore",
	        "cam_v_r1a_bls_part"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_BLS_FULL, "cam_v_r1a_bls_full",
			"mm_mminfra_ck"/* parent */, 3),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_BLS_FULL_CAM_VCORE, "cam_v_r1a_bls_full_cam_vcore",
	        "cam_v_r1a_bls_full"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_SV0_GALS, "cam_v_r1a_sv0_gcon_0",
			"mm_mminfra_ck"/* parent */, 4),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_SV0_GALS_CAM_VCORE, "cam_v_r1a_sv0_gcon_0_cam_vcore",
	        "cam_v_r1a_sv0_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_SV1_GALS, "cam_v_r1a_sv1_gcon_0",
			"mm_mminfra_ck"/* parent */, 5),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_SV1_GALS_CAM_VCORE, "cam_v_r1a_sv1_gcon_0_cam_vcore",
	        "cam_v_r1a_sv1_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_TX6_GALS, "cam_v_r1a_tx6_gcon_0",
			"mm_mminfra_ck"/* parent */, 6),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_TX6_GALS_CAM_VCORE, "cam_v_r1a_tx6_gcon_0_cam_vcore",
	        "cam_v_r1a_tx6_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_TX13_GALS, "cam_v_r1a_tx13_gcon_0",
			"mm_mminfra_ck"/* parent */, 7),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_TX13_GALS_CAM_VCORE, "cam_v_r1a_tx13_gcon_0_cam_vcore",
	        "cam_v_r1a_tx13_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_TX47_GALS, "cam_v_r1a_tx47_gcon_0",
			"mm_mminfra_ck"/* parent */, 8),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_TX47_GALS_CAM_VCORE, "cam_v_r1a_tx47_gcon_0_cam_vcore",
	        "cam_v_r1a_tx47_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_TX16_GALS, "cam_v_r1a_tx16_gcon_0",
			"mm_mminfra_ck"/* parent */, 9),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_TX16_GALS_CAM_VCORE, "cam_v_r1a_tx16_gcon_0_cam_vcore",
	        "cam_v_r1a_tx16_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_TX22_GALS, "cam_v_r1a_tx22_gcon_0",
			"mm_mminfra_ck"/* parent */, 10),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_TX22_GALS_CAM_VCORE, "cam_v_r1a_tx22_gcon_0_cam_vcore",
	        "cam_v_r1a_tx22_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_TX90_GALS, "cam_v_r1a_tx90_gcon_0",
			"mm_mminfra_ck"/* parent */, 11),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_TX90_GALS_CAM_VCORE, "cam_v_r1a_tx90_gcon_0_cam_vcore",
	        "cam_v_r1a_tx90_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_SUB_COMM0, "cam_v_r1a_sub_comm0",
			"mm_mminfra_ck"/* parent */, 12),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_SUB_COMM0_CAM_VCORE, "cam_v_r1a_sub_comm0_cam_vcore",
	        "cam_v_r1a_sub_comm0"/* parent */),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_SUB_COMM0_SMI, "cam_v_r1a_sub_comm0_smi",
	        "cam_v_r1a_sub_comm0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_SUB_COMM1, "cam_v_r1a_sub_comm1",
			"mm_mminfra_ck"/* parent */, 13),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_SUB_COMM1_CAM_VCORE, "cam_v_r1a_sub_comm1_cam_vcore",
	        "cam_v_r1a_sub_comm1"/* parent */),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_SUB_COMM1_SMI, "cam_v_r1a_sub_comm1_smi",
	        "cam_v_r1a_sub_comm1"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_SUB_COMM2, "cam_v_r1a_sub_comm2",
			"mm_mminfra_ck"/* parent */, 14),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_SUB_COMM2_CAM_VCORE, "cam_v_r1a_sub_comm2_cam_vcore",
	        "cam_v_r1a_sub_comm2"/* parent */),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_SUB_COMM2_SMI, "cam_v_r1a_sub_comm2_smi",
	        "cam_v_r1a_sub_comm2"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_SUB_COMM3, "cam_v_r1a_sub_comm3",
			"mm_mminfra_ck"/* parent */, 15),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_SUB_COMM3_CAM_VCORE, "cam_v_r1a_sub_comm3_cam_vcore",
	        "cam_v_r1a_sub_comm3"/* parent */),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_SUB_COMM3_SMI, "cam_v_r1a_sub_comm3_smi",
	        "cam_v_r1a_sub_comm3"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_X51_GALS, "cam_v_r1a_x51_gcon_0",
			"mm_mminfra_ck"/* parent */, 16),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_X51_GALS_CAM_VCORE, "cam_v_r1a_x51_gcon_0_cam_vcore",
	        "cam_v_r1a_x51_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_X50_GALS, "cam_v_r1a_x50_gcon_0",
			"mm_mminfra_ck"/* parent */, 17),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_X50_GALS_CAM_VCORE, "cam_v_r1a_x50_gcon_0_cam_vcore",
	        "cam_v_r1a_x50_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_X45_GALS, "cam_v_r1a_x45_gcon_0",
			"mm_mminfra_ck"/* parent */, 18),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_X45_GALS_CAM_VCORE, "cam_v_r1a_x45_gcon_0_cam_vcore",
	        "cam_v_r1a_x45_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_X43_GALS, "cam_v_r1a_x43_gcon_0",
			"mm_mminfra_ck"/* parent */, 19),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_X43_GALS_CAM_VCORE, "cam_v_r1a_x43_gcon_0_cam_vcore",
	        "cam_v_r1a_x43_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_X52_GALS, "cam_v_r1a_x52_gcon_0",
			"mm_mminfra_ck"/* parent */, 20),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_X52_GALS_CAM_VCORE, "cam_v_r1a_x52_gcon_0_cam_vcore",
	        "cam_v_r1a_x52_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_X44_GALS, "cam_v_r1a_x44_gcon_0",
			"mm_mminfra_ck"/* parent */, 21),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_X44_GALS_CAM_VCORE, "cam_v_r1a_x44_gcon_0_cam_vcore",
	        "cam_v_r1a_x44_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_X91_GALS, "cam_v_r1a_x91_gcon_0",
			"mm_mminfra_ck"/* parent */, 22),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_X91_GALS_CAM_VCORE, "cam_v_r1a_x91_gcon_0_cam_vcore",
	        "cam_v_r1a_x91_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_X54_GALS, "cam_v_r1a_x54_gcon_0",
			"mm_mminfra_ck"/* parent */, 23),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_X54_GALS_CAM_VCORE, "cam_v_r1a_x54_gcon_0_cam_vcore",
	        "cam_v_r1a_x54_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_X48_GALS, "cam_v_r1a_x48_gcon_0",
			"mm_mminfra_ck"/* parent */, 24),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_X48_GALS_CAM_VCORE, "cam_v_r1a_x48_gcon_0_cam_vcore",
	        "cam_v_r1a_x48_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_X93_GALS, "cam_v_r1a_x93_gcon_0",
			"mm_mminfra_ck"/* parent */, 25),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_X93_GALS_CAM_VCORE, "cam_v_r1a_x93_gcon_0_cam_vcore",
	        "cam_v_r1a_x93_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_X46_GALS, "cam_v_r1a_x46_gcon_0",
			"mm_mminfra_ck"/* parent */, 26),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_X46_GALS_CAM_VCORE, "cam_v_r1a_x46_gcon_0_cam_vcore",
	        "cam_v_r1a_x46_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_X53_GALS, "cam_v_r1a_x53_gcon_0",
			"mm_mminfra_ck"/* parent */, 27),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_X53_GALS_CAM_VCORE, "cam_v_r1a_x53_gcon_0_cam_vcore",
	        "cam_v_r1a_x53_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_X94_GALS, "cam_v_r1a_x94_gcon_0",
			"mm_mminfra_ck"/* parent */, 28),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_X94_GALS_CAM_VCORE, "cam_v_r1a_x94_gcon_0_cam_vcore",
	        "cam_v_r1a_x94_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_X22_GALS, "cam_v_r1a_x22_gcon_0",
			"mm_mminfra_ck"/* parent */, 29),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_X22_GALS_CAM_VCORE, "cam_v_r1a_x22_gcon_0_cam_vcore",
	        "cam_v_r1a_x22_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_X60_GALS, "cam_v_r1a_x60_gcon_0",
			"mm_ccusys_ck"/* parent */, 30),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_X60_GALS_CAM_VCORE, "cam_v_r1a_x60_gcon_0_cam_vcore",
	        "cam_v_r1a_x60_gcon_0"/* parent */),
	GATE_HWV_CAM_VCORE_R1A0(CAM_V_R1A_X90_GALS, "cam_v_r1a_x90_gcon_0",
			"mm_ccusys_ck"/* parent */, 31),
	GATE_CAM_VCORE_R1A0_V(CAM_V_R1A_X90_GALS_CAM_VCORE, "cam_v_r1a_x90_gcon_0_cam_vcore",
	        "cam_v_r1a_x90_gcon_0"/* parent */),
	/* CAM_VCORE_R1A1 */
	GATE_CAM_VCORE_R1A1(CAM_V_R1A_GALS51_CON_1, "cam_v_r1a_gals51_con_1",
			"mm_cam_ck"/* parent */, 0),
	GATE_CAM_VCORE_R1A1_V(CAM_V_R1A_GALS51_CON_1_CAM_VCORE, "cam_v_r1a_gals51_con_1_cam_vcore",
	        "cam_v_r1a_gals51_con_1"/* parent */),
	GATE_CAM_VCORE_R1A1(CAM_V_R1A_GALS50_CON_1, "cam_v_r1a_gals50_con_1",
			"mm_cam_ck"/* parent */, 1),
	GATE_CAM_VCORE_R1A1_V(CAM_V_R1A_GALS50_CON_1_CAM_VCORE, "cam_v_r1a_gals50_con_1_cam_vcore",
	        "cam_v_r1a_gals50_con_1"/* parent */),
	GATE_CAM_VCORE_R1A1(CAM_V_R1A_GALS45_CON_1, "cam_v_r1a_gals45_con_1",
			"mm_cam_ck"/* parent */, 2),
	GATE_CAM_VCORE_R1A1_V(CAM_V_R1A_GALS45_CON_1_CAM_VCORE, "cam_v_r1a_gals45_con_1_cam_vcore",
	        "cam_v_r1a_gals45_con_1"/* parent */),
	GATE_CAM_VCORE_R1A1(CAM_V_R1A_GALS43_CON_1, "cam_v_r1a_gals43_con_1",
			"mm_cam_ck"/* parent */, 3),
	GATE_CAM_VCORE_R1A1_V(CAM_V_R1A_GALS43_CON_1_CAM_VCORE, "cam_v_r1a_gals43_con_1_cam_vcore",
	        "cam_v_r1a_gals43_con_1"/* parent */),
	GATE_CAM_VCORE_R1A1(CAM_V_R1A_GALS52_CON_1, "cam_v_r1a_gals52_con_1",
			"mm_cam_ck"/* parent */, 4),
	GATE_CAM_VCORE_R1A1_V(CAM_V_R1A_GALS52_CON_1_CAM_VCORE, "cam_v_r1a_gals52_con_1_cam_vcore",
	        "cam_v_r1a_gals52_con_1"/* parent */),
	GATE_CAM_VCORE_R1A1(CAM_V_R1A_GALS44_CON_1, "cam_v_r1a_gals44_con_1",
			"mm_cam_ck"/* parent */, 5),
	GATE_CAM_VCORE_R1A1_V(CAM_V_R1A_GALS44_CON_1_CAM_VCORE, "cam_v_r1a_gals44_con_1_cam_vcore",
	        "cam_v_r1a_gals44_con_1"/* parent */),
	GATE_CAM_VCORE_R1A1(CAM_V_R1A_GALS91_CON_1, "cam_v_r1a_gals91_con_1",
			"mm_cam_ck"/* parent */, 6),
	GATE_CAM_VCORE_R1A1_V(CAM_V_R1A_GALS91_CON_1_CAM_VCORE, "cam_v_r1a_gals91_con_1_cam_vcore",
	        "cam_v_r1a_gals91_con_1"/* parent */),
	GATE_CAM_VCORE_R1A1(CAM_V_R1A_GALS54_CON_1, "cam_v_r1a_gals54_con_1",
			"mm_cam_ck"/* parent */, 7),
	GATE_CAM_VCORE_R1A1_V(CAM_V_R1A_GALS54_CON_1_CAM_VCORE, "cam_v_r1a_gals54_con_1_cam_vcore",
	        "cam_v_r1a_gals54_con_1"/* parent */),
	GATE_CAM_VCORE_R1A1(CAM_V_R1A_GALS48_CON_1, "cam_v_r1a_gals48_con_1",
			"mm_cam_ck"/* parent */, 8),
	GATE_CAM_VCORE_R1A1_V(CAM_V_R1A_GALS48_CON_1_CAM_VCORE, "cam_v_r1a_gals48_con_1_cam_vcore",
	        "cam_v_r1a_gals48_con_1"/* parent */),
	GATE_CAM_VCORE_R1A1(CAM_V_R1A_GALS93_CON_1, "cam_v_r1a_gals93_con_1",
			"mm_cam_ck"/* parent */, 9),
	GATE_CAM_VCORE_R1A1_V(CAM_V_R1A_GALS93_CON_1_CAM_VCORE, "cam_v_r1a_gals93_con_1_cam_vcore",
	        "cam_v_r1a_gals93_con_1"/* parent */),
	GATE_CAM_VCORE_R1A1(CAM_V_R1A_GALS46_CON_1, "cam_v_r1a_gals46_con_1",
			"mm_cam_ck"/* parent */, 10),
	GATE_CAM_VCORE_R1A1_V(CAM_V_R1A_GALS46_CON_1_CAM_VCORE, "cam_v_r1a_gals46_con_1_cam_vcore",
	        "cam_v_r1a_gals46_con_1"/* parent */),
	GATE_CAM_VCORE_R1A1(CAM_V_R1A_GALS53_CON_1, "cam_v_r1a_gals53_con_1",
			"mm_cam_ck"/* parent */, 11),
	GATE_CAM_VCORE_R1A1_V(CAM_V_R1A_GALS53_CON_1_CAM_VCORE, "cam_v_r1a_gals53_con_1_cam_vcore",
	        "cam_v_r1a_gals53_con_1"/* parent */),
	GATE_CAM_VCORE_R1A1(CAM_V_R1A_GALS94_CON_1, "cam_v_r1a_gals94_con_1",
			"mm_cam_ck"/* parent */, 12),
	GATE_CAM_VCORE_R1A1_V(CAM_V_R1A_GALS94_CON_1_CAM_VCORE, "cam_v_r1a_gals94_con_1_cam_vcore",
	        "cam_v_r1a_gals94_con_1"/* parent */),
	GATE_CAM_VCORE_R1A1(CAM_V_R1A_GALS22_CON_1, "cam_v_r1a_gals22_con_1",
			"mm_cam_ck"/* parent */, 13),
	GATE_CAM_VCORE_R1A1_V(CAM_V_R1A_GALS22_CON_1_CAM_VCORE, "cam_v_r1a_gals22_con_1_cam_vcore",
	        "cam_v_r1a_gals22_con_1"/* parent */),
};

static const struct mtk_clk_desc cam_vcore_r1a_mcd = {
	.clks = cam_vcore_r1a_clks,
	.num_clks = CLK_CAM_VCORE_R1A_NR_CLK,
};

static const struct mtk_gate_regs ccu_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs ccu0_hwv_regs = {
	.set_ofs = 0x18c,
	.clr_ofs = 0x190,
	.sta_ofs = 0x12684,
};

#define GATE_CCU(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ccu_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_CCU_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_CCU0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ccu_cg_regs,			\
		.hwv_regs = &ccu0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate ccu_clks[] = {
	GATE_HWV_CCU0(CCU_LARB19_CON, "ccu_larb19_con",
			"mm_ccusys_ck"/* parent */, 0),
	GATE_CCU_V(CCU_LARB19_CON_CCU, "ccu_larb19_con_ccu",
	        "ccu_larb19_con"/* parent */),
	GATE_CCU_V(CCU_LARB19_CON_SMI, "ccu_larb19_con_smi",
	        "ccu_larb19_con"/* parent */),
	GATE_HWV_CCU0(CCU2INFRA_GALS_CON, "ccu2infra_gcon",
			"mm_ccusys_ck"/* parent */, 1),
	GATE_CCU_V(CCU2INFRA_GALS_CON_CCU, "ccu2infra_gcon_ccu",
	        "ccu2infra_gcon"/* parent */),
	GATE_CCU_V(CCU2INFRA_GALS_CON_SMI, "ccu2infra_gcon_smi",
	        "ccu2infra_gcon"/* parent */),
	GATE_HWV_CCU0(CCUSYS_CCU0_CON, "ccusys_ccu0_con",
			"mm_ccusys_ck"/* parent */, 2),
	GATE_CCU_V(CCUSYS_CCU0_CON_CCU, "ccusys_ccu0_con_ccu",
	        "ccusys_ccu0_con"/* parent */),
	GATE_HWV_CCU0(CCU2MM0_GALS_CON, "ccu2mm0_gcon",
			"mm_ccusys_ck"/* parent */, 4),
	GATE_CCU_V(CCU2MM0_GALS_CON_CCU, "ccu2mm0_gcon_ccu",
	        "ccu2mm0_gcon"/* parent */),
	GATE_CCU_V(CCU2MM0_GALS_CON_SMI, "ccu2mm0_gcon_smi",
	        "ccu2mm0_gcon"/* parent */),
};

static const struct mtk_clk_desc ccu_mcd = {
	.clks = ccu_clks,
	.num_clks = CLK_CCU_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6993_cam[] = {
	{
		.compatible = "mediatek,mt6993-camsys_mraw",
		.data = &cam_mr_mcd,
	}, {
		.compatible = "mediatek,mt6993-camsys_rawa",
		.data = &cam_ra_mcd,
	}, {
		.compatible = "mediatek,mt6993-camsys_rawb",
		.data = &cam_rb_mcd,
	}, {
		.compatible = "mediatek,mt6993-camsys_rawc",
		.data = &cam_rc_mcd,
	}, {
		.compatible = "mediatek,mt6993-camsys_rmsa",
		.data = &camsys_rmsa_mcd,
	}, {
		.compatible = "mediatek,mt6993-camsys_rmsb",
		.data = &camsys_rmsb_mcd,
	}, {
		.compatible = "mediatek,mt6993-camsys_rmsc",
		.data = &camsys_rmsc_mcd,
	}, {
		.compatible = "mediatek,mt6993-camsys_yuva",
		.data = &cam_ya_mcd,
	}, {
		.compatible = "mediatek,mt6993-camsys_yuvb",
		.data = &cam_yb_mcd,
	}, {
		.compatible = "mediatek,mt6993-camsys_yuvc",
		.data = &cam_yc_mcd,
	}, {
		.compatible = "mediatek,mt6993-cam_main_r1a",
		.data = &cam_m_mcd,
	}, {
		.compatible = "mediatek,mt6993-cam_vcore_r1a",
		.data = &cam_vcore_r1a_mcd,
	}, {
		.compatible = "mediatek,mt6993-ccu",
		.data = &ccu_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6993_cam_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6993_cam_drv = {
	.probe = clk_mt6993_cam_grp_probe,
	.driver = {
		.name = "clk-mt6993-cam",
		.of_match_table = of_match_clk_mt6993_cam,
	},
};

module_platform_driver(clk_mt6993_cam_drv);
MODULE_LICENSE("GPL");
