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

static const struct mtk_gate_regs camsys_ipe_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAMSYS_IPE(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &camsys_ipe_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAMSYS_IPE_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate camsys_ipe_clks[] = {
	GATE_CAMSYS_IPE(CLK_CAMSYS_IPE_LARB19, "camsys_ipe_larb19",
			"dpe_ck"/* parent */, 0),
	GATE_CAMSYS_IPE_V(CLK_CAMSYS_IPE_LARB19_SMI, "camsys_ipe_larb19_smi",
			"camsys_ipe_larb19"/* parent */),
	GATE_CAMSYS_IPE_V(CLK_CAMSYS_IPE_LARB19_CAMERA_P2, "camsys_ipe_larb19_camera_p2",
			"camsys_ipe_larb19"/* parent */),
	GATE_CAMSYS_IPE_V(CLK_CAMSYS_IPE_LARB19_GENPD, "camsys_ipe_larb19_genpd",
			"camsys_ipe_larb19"/* parent */),
	GATE_CAMSYS_IPE(CLK_CAMSYS_IPE_DPE, "camsys_ipe_dpe",
			"dpe_ck"/* parent */, 1),
	GATE_CAMSYS_IPE_V(CLK_CAMSYS_IPE_DPE_CAMERA_P2, "camsys_ipe_dpe_camera_p2",
			"camsys_ipe_dpe"/* parent */),
	GATE_CAMSYS_IPE(CLK_CAMSYS_IPE_FUS, "camsys_ipe_fus",
			"dpe_ck"/* parent */, 2),
	GATE_CAMSYS_IPE_V(CLK_CAMSYS_IPE_FUS_CAMERA_P2, "camsys_ipe_fus_camera_p2",
			"camsys_ipe_fus"/* parent */),
	GATE_CAMSYS_IPE(CLK_CAMSYS_IPE_GALS, "camsys_ipe_gals",
			"dpe_ck"/* parent */, 4),
	GATE_CAMSYS_IPE_V(CLK_CAMSYS_IPE_GALS_CAMERA_P2, "camsys_ipe_gals_camera_p2",
			"camsys_ipe_gals"/* parent */),
};

static const struct mtk_clk_desc camsys_ipe_mcd = {
	.clks = camsys_ipe_clks,
	.num_clks = CLK_CAMSYS_IPE_NR_CLK,
};

static const struct mtk_gate_regs cam_mr_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_MR(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_mr_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_MR_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate cam_mr_clks[] = {
	GATE_CAM_MR(CLK_CAM_MR_LARBX, "cam_mr_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_MR_V(CLK_CAM_MR_LARBX_PDAF, "cam_mr_larbx_pdaf",
			"cam_mr_larbx"/* parent */),
	GATE_CAM_MR_V(CLK_CAM_MR_LARBX_SMI, "cam_mr_larbx_smi",
			"cam_mr_larbx"/* parent */),
	GATE_CAM_MR_V(CLK_CAM_MR_LARBX_GENPD, "cam_mr_larbx_genpd",
			"cam_mr_larbx"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_GALS, "cam_mr_gals",
			"cam_ck"/* parent */, 1),
	GATE_CAM_MR_V(CLK_CAM_MR_GALS_PDAF, "cam_mr_gals_pdaf",
			"cam_mr_gals"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_CAMTG, "cam_mr_camtg",
			"camtm_ck"/* parent */, 2),
	GATE_CAM_MR_V(CLK_CAM_MR_CAMTG_PDAF, "cam_mr_camtg_pdaf",
			"cam_mr_camtg"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_MRAW0, "cam_mr_mraw0",
			"cam_ck"/* parent */, 3),
	GATE_CAM_MR_V(CLK_CAM_MR_MRAW0_PDAF, "cam_mr_mraw0_pdaf",
			"cam_mr_mraw0"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_MRAW1, "cam_mr_mraw1",
			"cam_ck"/* parent */, 4),
	GATE_CAM_MR_V(CLK_CAM_MR_MRAW1_PDAF, "cam_mr_mraw1_pdaf",
			"cam_mr_mraw1"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_MRAW2, "cam_mr_mraw2",
			"cam_ck"/* parent */, 5),
	GATE_CAM_MR_V(CLK_CAM_MR_MRAW2_PDAF, "cam_mr_mraw2_pdaf",
			"cam_mr_mraw2"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_PDA0, "cam_mr_pda0",
			"cam_ck"/* parent */, 7),
	GATE_CAM_MR_V(CLK_CAM_MR_PDA0_PDAF, "cam_mr_pda0_pdaf",
			"cam_mr_pda0"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_PDA1, "cam_mr_pda1",
			"cam_ck"/* parent */, 8),
	GATE_CAM_MR_V(CLK_CAM_MR_PDA1_PDAF, "cam_mr_pda1_pdaf",
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

#define GATE_CAM_RA(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_ra_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_RA_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate cam_ra_clks[] = {
	GATE_CAM_RA(CLK_CAM_RA_LARBX, "cam_ra_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_RA_V(CLK_CAM_RA_LARBX_CAMRAWA, "cam_ra_larbx_camrawa",
			"cam_ra_larbx"/* parent */),
	GATE_CAM_RA_V(CLK_CAM_RA_LARBX_SMI, "cam_ra_larbx_smi",
			"cam_ra_larbx"/* parent */),
	GATE_CAM_RA_V(CLK_CAM_RA_LARBX_GENPD, "cam_ra_larbx_genpd",
			"cam_ra_larbx"/* parent */),
	GATE_CAM_RA(CLK_CAM_RA_CAM, "cam_ra_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_RA_V(CLK_CAM_RA_CAM_CAMRAWA, "cam_ra_cam_camrawa",
			"cam_ra_cam"/* parent */),
	GATE_CAM_RA(CLK_CAM_RA_CAMTG, "cam_ra_camtg",
			"cam_ck"/* parent */, 2),
	GATE_CAM_RA_V(CLK_CAM_RA_CAMTG_CAMRAWA, "cam_ra_camtg_camrawa",
			"cam_ra_camtg"/* parent */),
	GATE_CAM_RA(CLK_CAM_RA_RAW2MM_GALS, "cam_ra_raw2mm_gals",
			"cam_ck"/* parent */, 3),
	GATE_CAM_RA_V(CLK_CAM_RA_RAW2MM_GALS_CAMRAWA, "cam_ra_raw2mm_gals_camrawa",
			"cam_ra_raw2mm_gals"/* parent */),
	GATE_CAM_RA(CLK_CAM_RA_YUV2RAW2MM_GALS, "cam_ra_yuv2raw2mm",
			"cam_ck"/* parent */, 4),
	GATE_CAM_RA_V(CLK_CAM_RA_YUV2RAW2MM_GALS_CAMRAWA, "cam_ra_yuv2raw2mm_camrawa",
			"cam_ra_yuv2raw2mm"/* parent */),
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

#define GATE_CAM_RB(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_rb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_RB_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate cam_rb_clks[] = {
	GATE_CAM_RB(CLK_CAM_RB_LARBX, "cam_rb_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_RB_V(CLK_CAM_RB_LARBX_CAMRAWB, "cam_rb_larbx_camrawb",
			"cam_rb_larbx"/* parent */),
	GATE_CAM_RB_V(CLK_CAM_RB_LARBX_SMI, "cam_rb_larbx_smi",
			"cam_rb_larbx"/* parent */),
	GATE_CAM_RB_V(CLK_CAM_RB_LARBX_GENPD, "cam_rb_larbx_genpd",
			"cam_rb_larbx"/* parent */),
	GATE_CAM_RB(CLK_CAM_RB_CAM, "cam_rb_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_RB_V(CLK_CAM_RB_CAM_CAMRAWB, "cam_rb_cam_camrawb",
			"cam_rb_cam"/* parent */),
	GATE_CAM_RB(CLK_CAM_RB_CAMTG, "cam_rb_camtg",
			"cam_ck"/* parent */, 2),
	GATE_CAM_RB_V(CLK_CAM_RB_CAMTG_CAMRAWB, "cam_rb_camtg_camrawb",
			"cam_rb_camtg"/* parent */),
	GATE_CAM_RB(CLK_CAM_RB_RAW2MM_GALS, "cam_rb_raw2mm_gals",
			"cam_ck"/* parent */, 3),
	GATE_CAM_RB_V(CLK_CAM_RB_RAW2MM_GALS_CAMRAWB, "cam_rb_raw2mm_gals_camrawb",
			"cam_rb_raw2mm_gals"/* parent */),
	GATE_CAM_RB(CLK_CAM_RB_YUV2RAW2MM_GALS, "cam_rb_yuv2raw2mm",
			"cam_ck"/* parent */, 4),
	GATE_CAM_RB_V(CLK_CAM_RB_YUV2RAW2MM_GALS_CAMRAWB, "cam_rb_yuv2raw2mm_camrawb",
			"cam_rb_yuv2raw2mm"/* parent */),
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

#define GATE_CAM_RC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_rc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_RC_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate cam_rc_clks[] = {
	GATE_CAM_RC(CLK_CAM_RC_LARBX, "cam_rc_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_RC_V(CLK_CAM_RC_LARBX_CAMRAWC, "cam_rc_larbx_camrawc",
			"cam_rc_larbx"/* parent */),
	GATE_CAM_RC_V(CLK_CAM_RC_LARBX_SMI, "cam_rc_larbx_smi",
			"cam_rc_larbx"/* parent */),
	GATE_CAM_RC_V(CLK_CAM_RC_LARBX_GENPD, "cam_rc_larbx_genpd",
			"cam_rc_larbx"/* parent */),
	GATE_CAM_RC(CLK_CAM_RC_CAM, "cam_rc_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_RC_V(CLK_CAM_RC_CAM_CAMRAWC, "cam_rc_cam_camrawc",
			"cam_rc_cam"/* parent */),
	GATE_CAM_RC(CLK_CAM_RC_CAMTG, "cam_rc_camtg",
			"cam_ck"/* parent */, 2),
	GATE_CAM_RC_V(CLK_CAM_RC_CAMTG_CAMRAWC, "cam_rc_camtg_camrawc",
			"cam_rc_camtg"/* parent */),
	GATE_CAM_RC(CLK_CAM_RC_RAW2MM_GALS, "cam_rc_raw2mm_gals",
			"cam_ck"/* parent */, 3),
	GATE_CAM_RC_V(CLK_CAM_RC_RAW2MM_GALS_CAMRAWC, "cam_rc_raw2mm_gals_camrawc",
			"cam_rc_raw2mm_gals"/* parent */),
	GATE_CAM_RC(CLK_CAM_RC_YUV2RAW2MM_GALS, "cam_rc_yuv2raw2mm",
			"cam_ck"/* parent */, 4),
	GATE_CAM_RC_V(CLK_CAM_RC_YUV2RAW2MM_GALS_CAMRAWC, "cam_rc_yuv2raw2mm_camrawc",
			"cam_rc_yuv2raw2mm"/* parent */),
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

#define GATE_CAMSYS_RMSA(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &camsys_rmsa_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAMSYS_RMSA_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate camsys_rmsa_clks[] = {
	GATE_CAMSYS_RMSA(CLK_CAMSYS_RMSA_LARBX, "camsys_rmsa_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAMSYS_RMSA_V(CLK_CAMSYS_RMSA_LARBX_CAMRAWA, "camsys_rmsa_larbx_camrawa",
			"camsys_rmsa_larbx"/* parent */),
	GATE_CAMSYS_RMSA_V(CLK_CAMSYS_RMSA_LARBX_SMI, "camsys_rmsa_larbx_smi",
			"camsys_rmsa_larbx"/* parent */),
	GATE_CAMSYS_RMSA(CLK_CAMSYS_RMSA_CAM, "camsys_rmsa_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAMSYS_RMSA_V(CLK_CAMSYS_RMSA_CAM_CAMRAWA, "camsys_rmsa_cam_camrawa",
			"camsys_rmsa_cam"/* parent */),
	GATE_CAMSYS_RMSA(CLK_CAMSYS_RMSA_CAMTG, "camsys_rmsa_camtg",
			"cam_ck"/* parent */, 2),
	GATE_CAMSYS_RMSA_V(CLK_CAMSYS_RMSA_CAMTG_CAMRAWA, "camsys_rmsa_camtg_camrawa",
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

#define GATE_CAMSYS_RMSB(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &camsys_rmsb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAMSYS_RMSB_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate camsys_rmsb_clks[] = {
	GATE_CAMSYS_RMSB(CLK_CAMSYS_RMSB_LARBX, "camsys_rmsb_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAMSYS_RMSB_V(CLK_CAMSYS_RMSB_LARBX_CAMRAWB, "camsys_rmsb_larbx_camrawb",
			"camsys_rmsb_larbx"/* parent */),
	GATE_CAMSYS_RMSB_V(CLK_CAMSYS_RMSB_LARBX_SMI, "camsys_rmsb_larbx_smi",
			"camsys_rmsb_larbx"/* parent */),
	GATE_CAMSYS_RMSB(CLK_CAMSYS_RMSB_CAM, "camsys_rmsb_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAMSYS_RMSB_V(CLK_CAMSYS_RMSB_CAM_CAMRAWB, "camsys_rmsb_cam_camrawb",
			"camsys_rmsb_cam"/* parent */),
	GATE_CAMSYS_RMSB(CLK_CAMSYS_RMSB_CAMTG, "camsys_rmsb_camtg",
			"cam_ck"/* parent */, 2),
	GATE_CAMSYS_RMSB_V(CLK_CAMSYS_RMSB_CAMTG_CAMRAWB, "camsys_rmsb_camtg_camrawb",
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

#define GATE_CAMSYS_RMSC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &camsys_rmsc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAMSYS_RMSC_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate camsys_rmsc_clks[] = {
	GATE_CAMSYS_RMSC(CLK_CAMSYS_RMSC_LARBX, "camsys_rmsc_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAMSYS_RMSC_V(CLK_CAMSYS_RMSC_LARBX_CAMRAWC, "camsys_rmsc_larbx_camrawc",
			"camsys_rmsc_larbx"/* parent */),
	GATE_CAMSYS_RMSC_V(CLK_CAMSYS_RMSC_LARBX_SMI, "camsys_rmsc_larbx_smi",
			"camsys_rmsc_larbx"/* parent */),
	GATE_CAMSYS_RMSC(CLK_CAMSYS_RMSC_CAM, "camsys_rmsc_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAMSYS_RMSC_V(CLK_CAMSYS_RMSC_CAM_CAMRAWC, "camsys_rmsc_cam_camrawc",
			"camsys_rmsc_cam"/* parent */),
	GATE_CAMSYS_RMSC(CLK_CAMSYS_RMSC_CAMTG, "camsys_rmsc_camtg",
			"cam_ck"/* parent */, 2),
	GATE_CAMSYS_RMSC_V(CLK_CAMSYS_RMSC_CAMTG_CAMRAWC, "camsys_rmsc_camtg_camrawc",
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

#define GATE_CAM_YA(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_ya_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_YA_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate cam_ya_clks[] = {
	GATE_CAM_YA(CLK_CAM_YA_LARBX, "cam_ya_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_YA_V(CLK_CAM_YA_LARBX_CAMRAWA, "cam_ya_larbx_camrawa",
			"cam_ya_larbx"/* parent */),
	GATE_CAM_YA_V(CLK_CAM_YA_LARBX_SMI, "cam_ya_larbx_smi",
			"cam_ya_larbx"/* parent */),
	GATE_CAM_YA_V(CLK_CAM_YA_LARBX_GENPD, "cam_ya_larbx_genpd",
			"cam_ya_larbx"/* parent */),
	GATE_CAM_YA(CLK_CAM_YA_CAM, "cam_ya_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_YA_V(CLK_CAM_YA_CAM_CAMRAWA, "cam_ya_cam_camrawa",
			"cam_ya_cam"/* parent */),
	GATE_CAM_YA(CLK_CAM_YA_CAMTG, "cam_ya_camtg",
			"cam_ck"/* parent */, 2),
	GATE_CAM_YA_V(CLK_CAM_YA_CAMTG_CAMRAWA, "cam_ya_camtg_camrawa",
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

#define GATE_CAM_YB(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_yb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_YB_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate cam_yb_clks[] = {
	GATE_CAM_YB(CLK_CAM_YB_LARBX, "cam_yb_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_YB_V(CLK_CAM_YB_LARBX_CAMRAWB, "cam_yb_larbx_camrawb",
			"cam_yb_larbx"/* parent */),
	GATE_CAM_YB_V(CLK_CAM_YB_LARBX_SMI, "cam_yb_larbx_smi",
			"cam_yb_larbx"/* parent */),
	GATE_CAM_YB_V(CLK_CAM_YB_LARBX_GENPD, "cam_yb_larbx_genpd",
			"cam_yb_larbx"/* parent */),
	GATE_CAM_YB(CLK_CAM_YB_CAM, "cam_yb_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_YB_V(CLK_CAM_YB_CAM_CAMRAWB, "cam_yb_cam_camrawb",
			"cam_yb_cam"/* parent */),
	GATE_CAM_YB(CLK_CAM_YB_CAMTG, "cam_yb_camtg",
			"cam_ck"/* parent */, 2),
	GATE_CAM_YB_V(CLK_CAM_YB_CAMTG_CAMRAWB, "cam_yb_camtg_camrawb",
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

#define GATE_CAM_YC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_yc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_YC_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate cam_yc_clks[] = {
	GATE_CAM_YC(CLK_CAM_YC_LARBX, "cam_yc_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_YC_V(CLK_CAM_YC_LARBX_CAMRAWC, "cam_yc_larbx_camrawc",
			"cam_yc_larbx"/* parent */),
	GATE_CAM_YC_V(CLK_CAM_YC_LARBX_SMI, "cam_yc_larbx_smi",
			"cam_yc_larbx"/* parent */),
	GATE_CAM_YC_V(CLK_CAM_YC_LARBX_GENPD, "cam_yc_larbx_genpd",
			"cam_yc_larbx"/* parent */),
	GATE_CAM_YC(CLK_CAM_YC_CAM, "cam_yc_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_YC_V(CLK_CAM_YC_CAM_CAMRAWC, "cam_yc_cam_camrawc",
			"cam_yc_cam"/* parent */),
	GATE_CAM_YC(CLK_CAM_YC_CAMTG, "cam_yc_camtg",
			"cam_ck"/* parent */, 2),
	GATE_CAM_YC_V(CLK_CAM_YC_CAMTG_CAMRAWC, "cam_yc_camtg_camrawc",
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

static const struct mtk_gate_regs cam_m1_cg_regs = {
	.set_ofs = 0x50,
	.clr_ofs = 0x54,
	.sta_ofs = 0x4C,
};

static const struct mtk_gate_regs cam_m2_cg_regs = {
	.set_ofs = 0xC0,
	.clr_ofs = 0xC0,
	.sta_ofs = 0xC0,
};

#define GATE_CAM_M0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_m0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_M0_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

#define GATE_CAM_M1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_m1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_M1_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

#define GATE_CAM_M2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_m2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_CAM_M2_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate cam_m_clks[] = {
	/* CAM_M0 */
	GATE_CAM_M0(CLK_CAM_MAIN_LARB13, "cam_m_larb13",
			"cam_ck"/* parent */, 0),
	GATE_CAM_M0_V(CLK_CAM_MAIN_LARB13_CAMSV, "cam_m_larb13_camsv",
			"cam_m_larb13"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_LARB13_SMI, "cam_m_larb13_smi",
			"cam_m_larb13"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_LARB13_GENPD, "cam_m_larb13_genpd",
			"cam_m_larb13"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_LARB14, "cam_m_larb14",
			"cam_ck"/* parent */, 1),
	GATE_CAM_M0_V(CLK_CAM_MAIN_LARB14_CAMSV, "cam_m_larb14_camsv",
			"cam_m_larb14"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_LARB14_SMI, "cam_m_larb14_smi",
			"cam_m_larb14"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_LARB14_GENPD, "cam_m_larb14_genpd",
			"cam_m_larb14"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_LARB27, "cam_m_larb27",
			"cam_ck"/* parent */, 2),
	GATE_CAM_M0_V(CLK_CAM_MAIN_LARB27_SMI, "cam_m_larb27_smi",
			"cam_m_larb27"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_LARB27_GENPD, "cam_m_larb27_genpd",
			"cam_m_larb27"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_LARB29, "cam_m_larb29",
			"cam_ck"/* parent */, 3),
	GATE_CAM_M0_V(CLK_CAM_MAIN_LARB29_CAMSV, "cam_m_larb29_camsv",
			"cam_m_larb29"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_LARB29_SMI, "cam_m_larb29_smi",
			"cam_m_larb29"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_LARB29_GENPD, "cam_m_larb29_genpd",
			"cam_m_larb29"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM, "cam_m_cam",
			"cam_ck"/* parent */, 4),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_CAM_SENINF, "cam_m_cam_cam_seninf",
			"cam_m_cam"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_CAMRAWA, "cam_m_cam_camrawa",
			"cam_m_cam"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_CAMRAWB, "cam_m_cam_camrawb",
			"cam_m_cam"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_CAMRAWC, "cam_m_cam_camrawc",
			"cam_m_cam"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_CAMSV, "cam_m_cam_camsv",
			"cam_m_cam"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_SUBA, "cam_m_cam_suba",
			"cam_ck"/* parent */, 5),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_SUBA_CAMRAWA, "cam_m_cam_suba_camrawa",
			"cam_m_cam_suba"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_SUBA_GENPD, "cam_m_cam_suba_genpd",
			"cam_m_cam_suba"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_SUBB, "cam_m_cam_subb",
			"cam_ck"/* parent */, 6),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_SUBB_CAMRAWB, "cam_m_cam_subb_camrawb",
			"cam_m_cam_subb"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_SUBB_GENPD, "cam_m_cam_subb_genpd",
			"cam_m_cam_subb"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_SUBC, "cam_m_cam_subc",
			"cam_ck"/* parent */, 7),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_SUBC_CAMRAWC, "cam_m_cam_subc_camrawc",
			"cam_m_cam_subc"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_SUBC_GENPD, "cam_m_cam_subc_genpd",
			"cam_m_cam_subc"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_MRAW, "cam_m_cam_mraw",
			"cam_ck"/* parent */, 8),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_MRAW_PDAF, "cam_m_cam_mraw_pdaf",
			"cam_m_cam_mraw"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_MRAW_GENPD, "cam_m_cam_mraw_genpd",
			"cam_m_cam_mraw"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_CAMTG, "cam_m_camtg",
			"camtm_ck"/* parent */, 9),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAMTG_CAM_SENINF, "cam_m_camtg_cam_seninf",
			"cam_m_camtg"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAMTG_CAMRAWA, "cam_m_camtg_camrawa",
			"cam_m_camtg"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAMTG_CAMRAWB, "cam_m_camtg_camrawb",
			"cam_m_camtg"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAMTG_CAMRAWC, "cam_m_camtg_camrawc",
			"cam_m_camtg"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_SENINF, "cam_m_seninf",
			"cam_ck"/* parent */, 10),
	GATE_CAM_M0_V(CLK_CAM_MAIN_SENINF_CAM_SENINF, "cam_m_seninf_cam_seninf",
			"cam_m_seninf"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_CAMSV_TOP, "cam_m_camsv",
			"cam_ck"/* parent */, 11),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAMSV_TOP_CAMSV, "cam_m_camsv_camsv",
			"cam_m_camsv"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_ADLRD, "cam_m_adlrd",
			"cam_ck"/* parent */, 12),
	GATE_CAM_M0_V(CLK_CAM_MAIN_ADLRD_CAMRAWA, "cam_m_adlrd_camrawa",
			"cam_m_adlrd"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_ADLRD_CAMRAWB, "cam_m_adlrd_camrawb",
			"cam_m_adlrd"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_ADLRD_CAMRAWC, "cam_m_adlrd_camrawc",
			"cam_m_adlrd"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_ADLWR, "cam_m_adlwr",
			"cam_ck"/* parent */, 13),
	GATE_CAM_M0_V(CLK_CAM_MAIN_ADLWR_CAMRAWA, "cam_m_adlwr_camrawa",
			"cam_m_adlwr"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_ADLWR_CAMRAWB, "cam_m_adlwr_camrawb",
			"cam_m_adlwr"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_ADLWR_CAMRAWC, "cam_m_adlwr_camrawc",
			"cam_m_adlwr"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_FAKE_ENG, "cam_m_fake_eng",
			"cam_ck"/* parent */, 15),
	GATE_CAM_M0_V(CLK_CAM_MAIN_FAKE_ENG_CAMRAW, "cam_m_fake_eng_camraw",
			"cam_m_fake_eng"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM2MM0_GALS, "cam_m_cam2mm0_GCON_0",
			"cam_ck"/* parent */, 16),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM2MM0_GALS_CAMRAWA, "cam_m_cam2mm0_gcon_0_camrawa",
			"cam_m_cam2mm0_GCON_0"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM2MM0_GALS_CAMRAWB, "cam_m_cam2mm0_gcon_0_camrawb",
			"cam_m_cam2mm0_GCON_0"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM2MM0_GALS_CAMRAWC, "cam_m_cam2mm0_gcon_0_camrawc",
			"cam_m_cam2mm0_GCON_0"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM2MM0_GALS_CAMSV, "cam_m_cam2mm0_gcon_0_camsv",
			"cam_m_cam2mm0_GCON_0"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM2MM0_GALS_PDAF, "cam_m_cam2mm0_gcon_0_pdaf",
			"cam_m_cam2mm0_GCON_0"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM2MM1_GALS, "cam_m_cam2mm1_GCON_0",
			"cam_ck"/* parent */, 17),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM2MM1_GALS_CAMRAWA, "cam_m_cam2mm1_gcon_0_camrawa",
			"cam_m_cam2mm1_GCON_0"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM2MM1_GALS_CAMRAWB, "cam_m_cam2mm1_gcon_0_camrawb",
			"cam_m_cam2mm1_GCON_0"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM2MM1_GALS_CAMRAWC, "cam_m_cam2mm1_gcon_0_camrawc",
			"cam_m_cam2mm1_GCON_0"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM2MM1_GALS_CAMSV, "cam_m_cam2mm1_gcon_0_camsv",
			"cam_m_cam2mm1_GCON_0"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM2MM1_GALS_PDAF, "cam_m_cam2mm1_gcon_0_pdaf",
			"cam_m_cam2mm1_GCON_0"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM2SYS_GALS, "cam_m_cam2sys_GCON_0",
			"cam_ck"/* parent */, 18),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM2SYS_GALS_CAMRAWA, "cam_m_cam2sys_gcon_0_camrawa",
			"cam_m_cam2sys_GCON_0"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM2SYS_GALS_CAMRAWB, "cam_m_cam2sys_gcon_0_camrawb",
			"cam_m_cam2sys_GCON_0"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM2SYS_GALS_CAMRAWC, "cam_m_cam2sys_gcon_0_camrawc",
			"cam_m_cam2sys_GCON_0"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM2SYS_GALS_CAMSV, "cam_m_cam2sys_gcon_0_camsv",
			"cam_m_cam2sys_GCON_0"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM2SYS_GALS_PDAF, "cam_m_cam2sys_gcon_0_pdaf",
			"cam_m_cam2sys_GCON_0"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM2MM2_GALS, "cam_m_cam2mm2_GCON_0",
			"cam_ck"/* parent */, 19),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM2MM2_GALS_CAMRAWA, "cam_m_cam2mm2_gcon_0_camrawa",
			"cam_m_cam2mm2_GCON_0"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM2MM2_GALS_CAMRAWB, "cam_m_cam2mm2_gcon_0_camrawb",
			"cam_m_cam2mm2_GCON_0"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM2MM2_GALS_CAMRAWC, "cam_m_cam2mm2_gcon_0_camrawc",
			"cam_m_cam2mm2_GCON_0"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM2MM2_GALS_CAMSV, "cam_m_cam2mm2_gcon_0_camsv",
			"cam_m_cam2mm2_GCON_0"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM2MM2_GALS_PDAF, "cam_m_cam2mm2_gcon_0_pdaf",
			"cam_m_cam2mm2_GCON_0"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_IPS, "cam_m_ips",
			"cam_ck"/* parent */, 21),
	GATE_CAM_M0_V(CLK_CAM_MAIN_IPS_CAMRAW, "cam_m_ips_camraw",
			"cam_m_ips"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_DPE, "cam_m_cam_dpe",
			"dpe_ck"/* parent */, 26),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_DPE_CAMERA_P2, "cam_m_cam_dpe_camera_p2",
			"cam_m_cam_dpe"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_DPE_GENPD, "cam_m_cam_dpe_genpd",
			"cam_m_cam_dpe"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_ASG, "cam_m_cam_asg",
			"cam_ck"/* parent */, 27),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_ASG_CAM_SENINF, "cam_m_cam_asg_cam_seninf",
			"cam_m_cam_asg"/* parent */),
	/* CAM_M1 */
	GATE_CAM_M1(CLK_CAM_MAIN_CAMSV_A_CON_1, "cam_m_camsv_a_con_1",
			"cam_ck"/* parent */, 0),
	GATE_CAM_M1_V(CLK_CAM_MAIN_CAMSV_A_CON_1_CAMSV, "cam_m_camsv_a_con_1_camsv",
			"cam_m_camsv_a_con_1"/* parent */),
	GATE_CAM_M1(CLK_CAM_MAIN_CAMSV_B_CON_1, "cam_m_camsv_b_con_1",
			"cam_ck"/* parent */, 1),
	GATE_CAM_M1_V(CLK_CAM_MAIN_CAMSV_B_CON_1_CAMSV, "cam_m_camsv_b_con_1_camsv",
			"cam_m_camsv_b_con_1"/* parent */),
	GATE_CAM_M1(CLK_CAM_MAIN_CAMSV_C_CON_1, "cam_m_camsv_c_con_1",
			"cam_ck"/* parent */, 2),
	GATE_CAM_M1_V(CLK_CAM_MAIN_CAMSV_C_CON_1_CAMSV, "cam_m_camsv_c_con_1_camsv",
			"cam_m_camsv_c_con_1"/* parent */),
	GATE_CAM_M1(CLK_CAM_MAIN_CAMSV_D_CON_1, "cam_m_camsv_d_con_1",
			"cam_ck"/* parent */, 3),
	GATE_CAM_M1_V(CLK_CAM_MAIN_CAMSV_D_CON_1_CAMSV, "cam_m_camsv_d_con_1_camsv",
			"cam_m_camsv_d_con_1"/* parent */),
	GATE_CAM_M1(CLK_CAM_MAIN_CAMSV_E_CON_1, "cam_m_camsv_e_con_1",
			"cam_ck"/* parent */, 4),
	GATE_CAM_M1_V(CLK_CAM_MAIN_CAMSV_E_CON_1_CAMSV, "cam_m_camsv_e_con_1_camsv",
			"cam_m_camsv_e_con_1"/* parent */),
	GATE_CAM_M1(CLK_CAM_MAIN_CAM_QOF_CON_1, "cam_m_cam_qof_con_1",
			"cam_ck"/* parent */, 8),
	GATE_CAM_M1_V(CLK_CAM_MAIN_CAM_QOF_CON_1_CAMRAW, "cam_m_cam_qof_con_1_camraw",
			"cam_m_cam_qof_con_1"/* parent */),
	GATE_CAM_M1(CLK_CAM_MAIN_CAM_BLS_FULL_CON_1, "cam_m_cam_bls_full_con_1",
			"cam_ck"/* parent */, 9),
	GATE_CAM_M1_V(CLK_CAM_MAIN_CAM_BLS_FULL_CON_1_CAMRAW, "cam_m_cam_bls_full_con_1_camraw",
			"cam_m_cam_bls_full_con_1"/* parent */),
	GATE_CAM_M1(CLK_CAM_MAIN_CAM_BLS_PART_CON_1, "cam_m_cam_bls_part_con_1",
			"cam_ck"/* parent */, 10),
	GATE_CAM_M1_V(CLK_CAM_MAIN_CAM_BLS_PART_CON_1_CAMRAW, "cam_m_cam_bls_part_con_1_camraw",
			"cam_m_cam_bls_part_con_1"/* parent */),
	GATE_CAM_M1(CLK_CAM_MAIN_CAM_BWR_CON_1, "cam_m_cam_bwr_con_1",
			"cam_ck"/* parent */, 11),
	GATE_CAM_M1_V(CLK_CAM_MAIN_CAM_BWR_CON_1_CAMBWR, "cam_m_cam_bwr_con_1_cambwr",
			"cam_m_cam_bwr_con_1"/* parent */),
	GATE_CAM_M1(CLK_CAM_MAIN_CAM_RTCQ_CON_1, "cam_m_cam_rtcq_con_1",
			"cam_ck"/* parent */, 12),
	GATE_CAM_M1_V(CLK_CAM_MAIN_CAM_RTCQ_CON_1_CAMRAW, "cam_m_cam_rtcq_con_1_camraw",
			"cam_m_cam_rtcq_con_1"/* parent */),
	/* CAM_M2 */
	GATE_CAM_M2(CLK_CAM_MAIN_CAM2MM0_SUB_COMMON_DCM_DIS, "cam_m_cam2mm0_sub_c_dis",
			"cam_ck"/* parent */, 0),
	GATE_CAM_M2_V(CLK_CAM_MAIN_CAM2MM0_SUB_COMMON_DCM_DIS_CAMRAW, "cam_m_cam2mm0_sub_c_dis_camraw",
			"cam_m_cam2mm0_sub_c_dis"/* parent */),
	GATE_CAM_M2(CLK_CAM_MAIN_CAM2MM1_SUB_COMMON_DCM_DIS, "cam_m_cam2mm1_sub_c_dis",
			"cam_ck"/* parent */, 1),
	GATE_CAM_M2_V(CLK_CAM_MAIN_CAM2MM1_SUB_COMMON_DCM_DIS_CAMRAW, "cam_m_cam2mm1_sub_c_dis_camraw",
			"cam_m_cam2mm1_sub_c_dis"/* parent */),
	GATE_CAM_M2(CLK_CAM_MAIN_CAM2SYS_SUB_COMMON_DCM_DIS, "cam_m_cam2sys_sub_c_dis",
			"cam_ck"/* parent */, 2),
	GATE_CAM_M2_V(CLK_CAM_MAIN_CAM2SYS_SUB_COMMON_DCM_DIS_CAMRAW, "cam_m_cam2sys_sub_c_dis_camraw",
			"cam_m_cam2sys_sub_c_dis"/* parent */),
	GATE_CAM_M2(CLK_CAM_MAIN_CAM2MM2_SUB_COMMON_DCM_DIS, "cam_m_cam2mm2_sub_c_dis",
			"cam_ck"/* parent */, 5),
	GATE_CAM_M2_V(CLK_CAM_MAIN_CAM2MM2_SUB_COMMON_DCM_DIS_CAMRAW, "cam_m_cam2mm2_sub_c_dis_camraw",
			"cam_m_cam2mm2_sub_c_dis"/* parent */),
};

static const struct mtk_clk_desc cam_m_mcd = {
	.clks = cam_m_clks,
	.num_clks = CLK_CAM_M_NR_CLK,
};

static const struct mtk_gate_regs camv0_cg_regs = {
	.set_ofs = 0x2C,
	.clr_ofs = 0x2C,
	.sta_ofs = 0x2C,
};

static const struct mtk_gate_regs camv1_cg_regs = {
	.set_ofs = 0xA4,
	.clr_ofs = 0xA8,
	.sta_ofs = 0xA0,
};

#define GATE_CAMV0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &camv0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_CAMV0_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

#define GATE_CAMV1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &camv1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAMV1_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate camv_clks[] = {
	/* CAMV0 */
	GATE_CAMV0(CLK_CAMV_CV_CAM2MM0_SC_DCM_DIS, "camv_cv_cam2mm0_subc_dis",
			"mminfra_ck"/* parent */, 0),
	GATE_CAMV0_V(CLK_CAMV_CV_CAM2MM0_SC_DCM_DIS_CAMRAW, "camv_cv_cam2mm0_subc_dis_camraw",
			"camv_cv_cam2mm0_subc_dis"/* parent */),
	GATE_CAMV0_V(CLK_CAMV_CV_CAM2MM0_SC_DCM_DIS_CAM_SENINF, "camv_cv_cam2mm0_subc_dis_cam_seninf",
			"camv_cv_cam2mm0_subc_dis"/* parent */),
	GATE_CAMV0(CLK_CAMV_CV_MM0_SC_DCM_DIS, "camv_cv_mm0_subc_dis",
			"mminfra_ck"/* parent */, 1),
	GATE_CAMV0_V(CLK_CAMV_CV_MM0_SC_DCM_DIS_CCU, "camv_cv_mm0_subc_dis_ccu",
			"camv_cv_mm0_subc_dis"/* parent */),
	GATE_CAMV0_V(CLK_CAMV_CV_MM0_SC_DCM_DIS_CAMRAW, "camv_cv_mm0_subc_dis_camraw",
			"camv_cv_mm0_subc_dis"/* parent */),
	GATE_CAMV0_V(CLK_CAMV_CV_MM0_SC_DCM_DIS_CAM_SENINF, "camv_cv_mm0_subc_dis_cam_seninf",
			"camv_cv_mm0_subc_dis"/* parent */),
	GATE_CAMV0_V(CLK_CAMV_CV_MM0_SC_DCM_DIS_SMI, "camv_cv_mm0_subc_dis_smi",
			"camv_cv_mm0_subc_dis"/* parent */),
	/* CAMV1 */
	GATE_CAMV1(CLK_CAMV_CV_CAMVCORE, "camv_cv_camvcore",
			"mminfra_ck"/* parent */, 0),
	GATE_CAMV1_V(CLK_CAMV_CV_CAMVCORE_CAMRAW, "camv_cv_camvcore_camraw",
			"camv_cv_camvcore"/* parent */),
	GATE_CAMV1_V(CLK_CAMV_CV_CAMVCORE_CAM_SENINF, "camv_cv_camvcore_cam_seninf",
			"camv_cv_camvcore"/* parent */),
	GATE_CAMV1(CLK_CAMV_CV_CAM_26M, "camv_cv_cam_26m",
			"cam_26m_ck"/* parent */, 1),
	GATE_CAMV1_V(CLK_CAMV_CV_CAM_26M_CAMRAW, "camv_cv_cam_26m_camraw",
			"camv_cv_cam_26m"/* parent */),
	GATE_CAMV1_V(CLK_CAMV_CV_CAM_26M_CAM_SENINF, "camv_cv_cam_26m_cam_seninf",
			"camv_cv_cam_26m"/* parent */),
};

static const struct mtk_clk_desc camv_mcd = {
	.clks = camv_clks,
	.num_clks = CLK_CAMV_NR_CLK,
};

static const struct mtk_gate_regs ccu_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CCU(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ccu_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CCU_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate ccu_clks[] = {
	GATE_CCU(CLK_CCU_LARB19_CON, "ccu_larb19_con",
			"ccusys_ck"/* parent */, 0),
	GATE_CCU_V(CLK_CCU_LARB19_CON_CCU, "ccu_larb19_con_ccu",
			"ccu_larb19_con"/* parent */),
	GATE_CCU_V(CLK_CCU_LARB19_CON_SMI, "ccu_larb19_con_smi",
			"ccu_larb19_con"/* parent */),
	GATE_CCU_V(CLK_CCU_LARB19_CON_GENPD, "ccu_larb19_con_genpd",
			"ccu_larb19_con"/* parent */),
	GATE_CCU(CLK_CCU2INFRA_GALS_CON, "ccu2infra_GCON",
			"ccusys_ck"/* parent */, 1),
	GATE_CCU_V(CLK_CCU2INFRA_GALS_CON_CCU, "ccu2infra_gcon_ccu",
			"ccu2infra_GCON"/* parent */),
	GATE_CCU(CLK_CCUSYS_CCU0_CON, "ccusys_ccu0_con",
			"ccusys_ck"/* parent */, 2),
	GATE_CCU_V(CLK_CCUSYS_CCU0_CON_CCU0, "ccusys_ccu0_con_ccu0",
			"ccusys_ccu0_con"/* parent */),
	GATE_CCU(CLK_CCU2MM0_GALS_CON, "ccu2mm0_GCON",
			"ccusys_ck"/* parent */, 4),
	GATE_CCU_V(CLK_CCU2MM0_GALS_CON_CCU, "ccu2mm0_gcon_ccu",
			"ccu2mm0_GCON"/* parent */),
};

static const struct mtk_clk_desc ccu_mcd = {
	.clks = ccu_clks,
	.num_clks = CLK_CCU_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6899_cam[] = {
	{
		.compatible = "mediatek,mt6899-camsys_ipe",
		.data = &camsys_ipe_mcd,
	}, {
		.compatible = "mediatek,mt6899-camsys_mraw",
		.data = &cam_mr_mcd,
	}, {
		.compatible = "mediatek,mt6899-camsys_rawa",
		.data = &cam_ra_mcd,
	}, {
		.compatible = "mediatek,mt6899-camsys_rawb",
		.data = &cam_rb_mcd,
	}, {
		.compatible = "mediatek,mt6899-camsys_rawc",
		.data = &cam_rc_mcd,
	}, {
		.compatible = "mediatek,mt6899-camsys_rmsa",
		.data = &camsys_rmsa_mcd,
	}, {
		.compatible = "mediatek,mt6899-camsys_rmsb",
		.data = &camsys_rmsb_mcd,
	}, {
		.compatible = "mediatek,mt6899-camsys_rmsc",
		.data = &camsys_rmsc_mcd,
	}, {
		.compatible = "mediatek,mt6899-camsys_yuva",
		.data = &cam_ya_mcd,
	}, {
		.compatible = "mediatek,mt6899-camsys_yuvb",
		.data = &cam_yb_mcd,
	}, {
		.compatible = "mediatek,mt6899-camsys_yuvc",
		.data = &cam_yc_mcd,
	}, {
		.compatible = "mediatek,mt6899-cam_main_r1a",
		.data = &cam_m_mcd,
	}, {
		.compatible = "mediatek,mt6899-cam_vcore_r1a",
		.data = &camv_mcd,
	}, {
		.compatible = "mediatek,mt6899-ccu",
		.data = &ccu_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6899_cam_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6899_cam_drv = {
	.probe = clk_mt6899_cam_grp_probe,
	.driver = {
		.name = "clk-mt6899-cam",
		.of_match_table = of_match_clk_mt6899_cam,
	},
};

module_platform_driver(clk_mt6899_cam_drv);
MODULE_LICENSE("GPL");
