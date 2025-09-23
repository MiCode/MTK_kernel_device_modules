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
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_DIP_NR1_DIP1_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate dip_nr1_dip1_clks[] = {
	GATE_DIP_NR1_DIP1(CLK_DIP_NR1_DIP1_LARB, "dip_nr1_dip1_larb",
			"img1_ck"/* parent */, 0),
	GATE_DIP_NR1_DIP1_V(CLK_DIP_NR1_DIP1_LARB_CAMERA_P2, "dip_nr1_dip1_larb_camera_p2",
			"dip_nr1_dip1_larb"/* parent */),
	GATE_DIP_NR1_DIP1(CLK_DIP_NR1_DIP1_DIP_NR1, "dip_nr1_dip1_dip_nr1",
			"img1_ck"/* parent */, 1),
	GATE_DIP_NR1_DIP1_V(CLK_DIP_NR1_DIP1_DIP_NR1_CAMERA_P2, "dip_nr1_dip1_dip_nr1_camera_p2",
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
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_DIP_NR2_DIP1_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate dip_nr2_dip1_clks[] = {
	GATE_DIP_NR2_DIP1(CLK_DIP_NR2_DIP1_DIP_NR, "dip_nr2_dip1_dip_nr",
			"img1_ck"/* parent */, 0),
	GATE_DIP_NR2_DIP1_V(CLK_DIP_NR2_DIP1_DIP_NR_CAMERA_P2, "dip_nr2_dip1_dip_nr_camera_p2",
			"dip_nr2_dip1_dip_nr"/* parent */),
	GATE_DIP_NR2_DIP1(CLK_DIP_NR2_DIP1_LARB15, "dip_nr2_dip1_larb15",
			"img1_ck"/* parent */, 1),
	GATE_DIP_NR2_DIP1_V(CLK_DIP_NR2_DIP1_LARB15_CAMERA_P2, "dip_nr2_dip1_larb15_camera_p2",
			"dip_nr2_dip1_larb15"/* parent */),
	GATE_DIP_NR2_DIP1(CLK_DIP_NR2_DIP1_LARB39, "dip_nr2_dip1_larb39",
			"img1_ck"/* parent */, 2),
	GATE_DIP_NR2_DIP1_V(CLK_DIP_NR2_DIP1_LARB39_CAMERA_P2, "dip_nr2_dip1_larb39_camera_p2",
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

#define GATE_DIP_TOP_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dip_top_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_DIP_TOP_DIP1_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate dip_top_dip1_clks[] = {
	GATE_DIP_TOP_DIP1(CLK_DIP_TOP_DIP1_DIP_TOP, "dip_dip1_dip_top",
			"img1_ck"/* parent */, 0),
	GATE_DIP_TOP_DIP1_V(CLK_DIP_TOP_DIP1_DIP_TOP_CAMERA_P2, "dip_dip1_dip_top_camera_p2",
			"dip_dip1_dip_top"/* parent */),
	GATE_DIP_TOP_DIP1(CLK_DIP_TOP_DIP1_DIP_TOP_GALS0, "dip_dip1_dip_gals0",
			"img1_ck"/* parent */, 1),
	GATE_DIP_TOP_DIP1_V(CLK_DIP_TOP_DIP1_DIP_TOP_GALS0_CAMERA_P2, "dip_dip1_dip_gals0_camera_p2",
			"dip_dip1_dip_gals0"/* parent */),
	GATE_DIP_TOP_DIP1(CLK_DIP_TOP_DIP1_DIP_TOP_GALS1, "dip_dip1_dip_gals1",
			"img1_ck"/* parent */, 2),
	GATE_DIP_TOP_DIP1_V(CLK_DIP_TOP_DIP1_DIP_TOP_GALS1_CAMERA_P2, "dip_dip1_dip_gals1_camera_p2",
			"dip_dip1_dip_gals1"/* parent */),
	GATE_DIP_TOP_DIP1(CLK_DIP_TOP_DIP1_DIP_TOP_GALS2, "dip_dip1_dip_gals2",
			"img1_ck"/* parent */, 3),
	GATE_DIP_TOP_DIP1_V(CLK_DIP_TOP_DIP1_DIP_TOP_GALS2_CAMERA_P2, "dip_dip1_dip_gals2_camera_p2",
			"dip_dip1_dip_gals2"/* parent */),
	GATE_DIP_TOP_DIP1(CLK_DIP_TOP_DIP1_DIP_TOP_GALS3, "dip_dip1_dip_gals3",
			"img1_ck"/* parent */, 4),
	GATE_DIP_TOP_DIP1_V(CLK_DIP_TOP_DIP1_DIP_TOP_GALS3_CAMERA_P2, "dip_dip1_dip_gals3_camera_p2",
			"dip_dip1_dip_gals3"/* parent */),
	GATE_DIP_TOP_DIP1(CLK_DIP_TOP_DIP1_LARB10, "dip_dip1_larb10",
			"img1_ck"/* parent */, 5),
	GATE_DIP_TOP_DIP1_V(CLK_DIP_TOP_DIP1_LARB10_CAMERA_P2, "dip_dip1_larb10_camera_p2",
			"dip_dip1_larb10"/* parent */),
	GATE_DIP_TOP_DIP1(CLK_DIP_TOP_DIP1_LARB15, "dip_dip1_larb15",
			"img1_ck"/* parent */, 6),
	GATE_DIP_TOP_DIP1_V(CLK_DIP_TOP_DIP1_LARB15_CAMERA_P2, "dip_dip1_larb15_camera_p2",
			"dip_dip1_larb15"/* parent */),
	GATE_DIP_TOP_DIP1_V(CLK_DIP_TOP_DIP1_LARB15_SMI, "dip_dip1_larb15_smi",
			"dip_dip1_larb15"/* parent */),
	GATE_DIP_TOP_DIP1(CLK_DIP_TOP_DIP1_LARB38, "dip_dip1_larb38",
			"img1_ck"/* parent */, 7),
	GATE_DIP_TOP_DIP1_V(CLK_DIP_TOP_DIP1_LARB38_CAMERA_P2, "dip_dip1_larb38_camera_p2",
			"dip_dip1_larb38"/* parent */),
	GATE_DIP_TOP_DIP1_V(CLK_DIP_TOP_DIP1_LARB38_SMI, "dip_dip1_larb38_smi",
			"dip_dip1_larb38"/* parent */),
	GATE_DIP_TOP_DIP1(CLK_DIP_TOP_DIP1_LARB39, "dip_dip1_larb39",
			"img1_ck"/* parent */, 8),
	GATE_DIP_TOP_DIP1_V(CLK_DIP_TOP_DIP1_LARB39_CAMERA_P2, "dip_dip1_larb39_camera_p2",
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

static const struct mtk_gate_regs img1_cg_regs = {
	.set_ofs = 0x54,
	.clr_ofs = 0x58,
	.sta_ofs = 0x50,
};

#define GATE_IMG0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &img0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMG0_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

#define GATE_IMG1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &img1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMG1_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate img_clks[] = {
	/* IMG0 */
	GATE_IMG0(CLK_IMG_LARB9, "img_larb9",
			"img1_ck"/* parent */, 0),
	GATE_IMG0_V(CLK_IMG_LARB9_CAMERA_P2, "img_larb9_camera_p2",
			"img_larb9"/* parent */),
	GATE_IMG0_V(CLK_IMG_LARB9_SMI, "img_larb9_smi",
			"img_larb9"/* parent */),
	GATE_IMG0(CLK_IMG_TRAW0, "img_traw0",
			"img1_ck"/* parent */, 1),
	GATE_IMG0_V(CLK_IMG_TRAW0_CAMERA_P2, "img_traw0_camera_p2",
			"img_traw0"/* parent */),
	GATE_IMG0(CLK_IMG_TRAW1, "img_traw1",
			"img1_ck"/* parent */, 2),
	GATE_IMG0_V(CLK_IMG_TRAW1_CAMERA_P2, "img_traw1_camera_p2",
			"img_traw1"/* parent */),
	GATE_IMG0(CLK_IMG_DIP0, "img_dip0",
			"img1_ck"/* parent */, 3),
	GATE_IMG0_V(CLK_IMG_DIP0_CAMERA_P2, "img_dip0_camera_p2",
			"img_dip0"/* parent */),
	GATE_IMG0(CLK_IMG_WPE0, "img_wpe0",
			"img1_ck"/* parent */, 4),
	GATE_IMG0_V(CLK_IMG_WPE0_CAMERA_P2, "img_wpe0_camera_p2",
			"img_wpe0"/* parent */),
	GATE_IMG0(CLK_IMG_IPE, "img_ipe",
			"img1_ck"/* parent */, 5),
	GATE_IMG0_V(CLK_IMG_IPE_CAMERA_P2, "img_ipe_camera_p2",
			"img_ipe"/* parent */),
	GATE_IMG0_V(CLK_IMG_IPE_CAMERA_MAE, "img_ipe_camera_mae",
			"img_ipe"/* parent */),
	GATE_IMG0(CLK_IMG_WPE1, "img_wpe1",
			"img1_ck"/* parent */, 6),
	GATE_IMG0_V(CLK_IMG_WPE1_CAMERA_P2, "img_wpe1_camera_p2",
			"img_wpe1"/* parent */),
	GATE_IMG0(CLK_IMG_WPE2, "img_wpe2",
			"img1_ck"/* parent */, 7),
	GATE_IMG0_V(CLK_IMG_WPE2_CAMERA_P2, "img_wpe2_camera_p2",
			"img_wpe2"/* parent */),
	GATE_IMG0(CLK_IMG_ADL_LARB, "img_adl_larb",
			"img1_ck"/* parent */, 8),
	GATE_IMG0_V(CLK_IMG_ADL_LARB_CAMERA_P2, "img_adl_larb_camera_p2",
			"img_adl_larb"/* parent */),
	GATE_IMG0_V(CLK_IMG_ADL_LARB_SMI, "img_adl_larb_smi",
			"img_adl_larb"/* parent */),
	GATE_IMG0(CLK_IMG_ADLRD, "img_adlrd",
			"img1_ck"/* parent */, 9),
	GATE_IMG0_V(CLK_IMG_ADLRD_CAMERA_P2, "img_adlrd_camera_p2",
			"img_adlrd"/* parent */),
	GATE_IMG0(CLK_IMG_ADLWR0, "img_adlwr0",
			"img1_ck"/* parent */, 10),
	GATE_IMG0_V(CLK_IMG_ADLWR0_CAMERA_P2, "img_adlwr0_camera_p2",
			"img_adlwr0"/* parent */),
	GATE_IMG0(CLK_IMG_AVS, "img_avs",
			"avs_img_ck"/* parent */, 11),
	GATE_IMG0_V(CLK_IMG_AVS_CAMERA_P2, "img_avs_camera_p2",
			"img_avs"/* parent */),
	GATE_IMG0(CLK_IMG_IPS, "img_ips",
			"img1_ck"/* parent */, 12),
	GATE_IMG0_V(CLK_IMG_IPS_CAMERA_P2, "img_ips_camera_p2",
			"img_ips"/* parent */),
	GATE_IMG0(CLK_IMG_ADLWR1, "img_adlwr1",
			"img1_ck"/* parent */, 13),
	GATE_IMG0_V(CLK_IMG_ADLWR1_CAMERA_P2, "img_adlwr1_camera_p2",
			"img_adlwr1"/* parent */),
	GATE_IMG0(CLK_IMG_ROOTCQ, "img_rootcq",
			"img1_ck"/* parent */, 14),
	GATE_IMG0_V(CLK_IMG_ROOTCQ_CAMERA_P2, "img_rootcq_camera_p2",
			"img_rootcq"/* parent */),
	GATE_IMG0(CLK_IMG_BLS, "img_bls",
			"img1_ck"/* parent */, 15),
	GATE_IMG0_V(CLK_IMG_BLS_CAMERA_P2, "img_bls_camera_p2",
			"img_bls"/* parent */),
	GATE_IMG0(CLK_IMG_SUB_COMMON0, "img_sub_common0",
			"img1_ck"/* parent */, 16),
	GATE_IMG0_V(CLK_IMG_SUB_COMMON0_CAMERA_P2, "img_sub_common0_camera_p2",
			"img_sub_common0"/* parent */),
	GATE_IMG0_V(CLK_IMG_SUB_COMMON0_SMI, "img_sub_common0_smi",
			"img_sub_common0"/* parent */),
	GATE_IMG0(CLK_IMG_SUB_COMMON1, "img_sub_common1",
			"img1_ck"/* parent */, 17),
	GATE_IMG0_V(CLK_IMG_SUB_COMMON1_CAMERA_P2, "img_sub_common1_camera_p2",
			"img_sub_common1"/* parent */),
	GATE_IMG0_V(CLK_IMG_SUB_COMMON1_SMI, "img_sub_common1_smi",
			"img_sub_common1"/* parent */),
	GATE_IMG0(CLK_IMG_SUB_COMMON2, "img_sub_common2",
			"img1_ck"/* parent */, 18),
	GATE_IMG0_V(CLK_IMG_SUB_COMMON2_CAMERA_P2, "img_sub_common2_camera_p2",
			"img_sub_common2"/* parent */),
	GATE_IMG0_V(CLK_IMG_SUB_COMMON2_CAMERA_MAE, "img_sub_common2_camera_mae",
			"img_sub_common2"/* parent */),
	GATE_IMG0_V(CLK_IMG_SUB_COMMON2_SMI, "img_sub_common2_smi",
			"img_sub_common2"/* parent */),
	GATE_IMG0(CLK_IMG_SUB_COMMON3, "img_sub_common3",
			"img1_ck"/* parent */, 19),
	GATE_IMG0_V(CLK_IMG_SUB_COMMON3_CAMERA_P2, "img_sub_common3_camera_p2",
			"img_sub_common3"/* parent */),
	GATE_IMG0_V(CLK_IMG_SUB_COMMON3_CAMERA_MAE, "img_sub_common3_camera_mae",
			"img_sub_common3"/* parent */),
	GATE_IMG0_V(CLK_IMG_SUB_COMMON3_SMI, "img_sub_common3_smi",
			"img_sub_common3"/* parent */),
	GATE_IMG0(CLK_IMG_SUB_COMMON4, "img_sub_common4",
			"img1_ck"/* parent */, 20),
	GATE_IMG0_V(CLK_IMG_SUB_COMMON4_CAMERA_P2, "img_sub_common4_camera_p2",
			"img_sub_common4"/* parent */),
	GATE_IMG0_V(CLK_IMG_SUB_COMMON4_SMI, "img_sub_common4_smi",
			"img_sub_common4"/* parent */),
	GATE_IMG0(CLK_IMG_GALS_RX_DIP0, "img_gals_rx_dip0",
			"img1_ck"/* parent */, 21),
	GATE_IMG0_V(CLK_IMG_GALS_RX_DIP0_CAMERA_P2, "img_gals_rx_dip0_camera_p2",
			"img_gals_rx_dip0"/* parent */),
	GATE_IMG0(CLK_IMG_GALS_RX_DIP1, "img_gals_rx_dip1",
			"img1_ck"/* parent */, 22),
	GATE_IMG0_V(CLK_IMG_GALS_RX_DIP1_CAMERA_P2, "img_gals_rx_dip1_camera_p2",
			"img_gals_rx_dip1"/* parent */),
	GATE_IMG0(CLK_IMG_GALS_RX_TRAW0, "img_gals_rx_traw0",
			"img1_ck"/* parent */, 23),
	GATE_IMG0_V(CLK_IMG_GALS_RX_TRAW0_CAMERA_P2, "img_gals_rx_traw0_camera_p2",
			"img_gals_rx_traw0"/* parent */),
	GATE_IMG0(CLK_IMG_GALS_RX_WPE0, "img_gals_rx_wpe0",
			"img1_ck"/* parent */, 24),
	GATE_IMG0_V(CLK_IMG_GALS_RX_WPE0_CAMERA_P2, "img_gals_rx_wpe0_camera_p2",
			"img_gals_rx_wpe0"/* parent */),
	GATE_IMG0(CLK_IMG_GALS_RX_WPE1, "img_gals_rx_wpe1",
			"img1_ck"/* parent */, 25),
	GATE_IMG0_V(CLK_IMG_GALS_RX_WPE1_CAMERA_P2, "img_gals_rx_wpe1_camera_p2",
			"img_gals_rx_wpe1"/* parent */),
	GATE_IMG0(CLK_IMG_GALS_RX_WPE2, "img_gals_rx_wpe2",
			"img1_ck"/* parent */, 26),
	GATE_IMG0_V(CLK_IMG_GALS_RX_WPE2_CAMERA_P2, "img_gals_rx_wpe2_camera_p2",
			"img_gals_rx_wpe2"/* parent */),
	GATE_IMG0(CLK_IMG_GALS_TRX_IPE0, "img_gals_trx_ipe0",
			"img1_ck"/* parent */, 27),
	GATE_IMG0_V(CLK_IMG_GALS_TRX_IPE0_CAMERA_P2, "img_gals_trx_ipe0_camera_p2",
			"img_gals_trx_ipe0"/* parent */),
	GATE_IMG0_V(CLK_IMG_GALS_TRX_IPE0_CAMERA_MAE, "img_gals_trx_ipe0_camera_mae",
			"img_gals_trx_ipe0"/* parent */),
	GATE_IMG0(CLK_IMG_GALS_TRX_IPE1, "img_gals_trx_ipe1",
			"img1_ck"/* parent */, 28),
	GATE_IMG0_V(CLK_IMG_GALS_TRX_IPE1_CAMERA_P2, "img_gals_trx_ipe1_camera_p2",
			"img_gals_trx_ipe1"/* parent */),
	GATE_IMG0_V(CLK_IMG_GALS_TRX_IPE1_CAMERA_MAE, "img_gals_trx_ipe1_camera_mae",
			"img_gals_trx_ipe1"/* parent */),
	GATE_IMG0(CLK_IMG26, "img26",
			"img_26m_ck"/* parent */, 29),
	GATE_IMG0_V(CLK_IMG26_CAMERA_P2, "img26_camera_p2",
			"img26"/* parent */),
	GATE_IMG0(CLK_IMG_BWR, "img_bwr",
			"img1_ck"/* parent */, 30),
	GATE_IMG0_V(CLK_IMG_BWR_CAMERA_P2, "img_bwr_camera_p2",
			"img_bwr"/* parent */),
	GATE_IMG0(CLK_IMG_GALS, "img_gals",
			"img1_ck"/* parent */, 31),
	GATE_IMG0_V(CLK_IMG_GALS_CAMERA_P2, "img_gals_camera_p2",
			"img_gals"/* parent */),
	GATE_IMG0_V(CLK_IMG_GALS_CAMERA_MAE, "img_gals_camera_mae",
			"img_gals"/* parent */),
	/* IMG1 */
	GATE_IMG1(CLK_IMG_FDVT, "img_fdvt",
			"ipe_ck"/* parent */, 0),
	GATE_IMG1_V(CLK_IMG_FDVT_CAMERA_MAE, "img_fdvt_camera_mae",
			"img_fdvt"/* parent */),
	GATE_IMG1(CLK_IMG_ME, "img_me",
			"ipe_ck"/* parent */, 1),
	GATE_IMG1_V(CLK_IMG_ME_CAMERA_P2, "img_me_camera_p2",
			"img_me"/* parent */),
	GATE_IMG1(CLK_IMG_MMG, "img_mmg",
			"ipe_ck"/* parent */, 2),
	GATE_IMG1_V(CLK_IMG_MMG_CAMERA_P2, "img_mmg_camera_p2",
			"img_mmg"/* parent */),
	GATE_IMG1(CLK_IMG_LARB12, "img_larb12",
			"ipe_ck"/* parent */, 3),
	GATE_IMG1_V(CLK_IMG_LARB12_CAMERA_P2, "img_larb12_camera_p2",
			"img_larb12"/* parent */),
	GATE_IMG1_V(CLK_IMG_LARB12_SMI, "img_larb12_smi",
			"img_larb12"/* parent */),
	GATE_IMG1_V(CLK_IMG_LARB12_CAMERA_MAE, "img_larb12_camera_mae",
			"img_larb12"/* parent */),
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

#define GATE_IMG_V(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &img_v_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMG_V_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate img_v_clks[] = {
	GATE_IMG_V(CLK_IMG_VCORE_GALS_DISP, "img_vcore_gals_disp",
			"mminfra_ck"/* parent */, 0),
	GATE_IMG_V_V(CLK_IMG_VCORE_GALS_DISP_CAMERA_P2, "img_vcore_gals_disp_camera_p2",
			"img_vcore_gals_disp"/* parent */),
	GATE_IMG_V_V(CLK_IMG_VCORE_GALS_DISP_CAMERA_MAE, "img_vcore_gals_disp_camera_mae",
			"img_vcore_gals_disp"/* parent */),
	GATE_IMG_V(CLK_IMG_VCORE_MAIN, "img_vcore_main",
			"mminfra_ck"/* parent */, 1),
	GATE_IMG_V_V(CLK_IMG_VCORE_MAIN_CAMERA_P2, "img_vcore_main_camera_p2",
			"img_vcore_main"/* parent */),
	GATE_IMG_V_V(CLK_IMG_VCORE_MAIN_CAMERA_MAE, "img_vcore_main_camera_mae",
			"img_vcore_main"/* parent */),
	GATE_IMG_V(CLK_IMG_VCORE_SUB0, "img_vcore_sub0",
			"mminfra_ck"/* parent */, 2),
	GATE_IMG_V_V(CLK_IMG_VCORE_SUB0_CAMERA_P2, "img_vcore_sub0_camera_p2",
			"img_vcore_sub0"/* parent */),
	GATE_IMG_V_V(CLK_IMG_VCORE_SUB0_CAMERA_MAE, "img_vcore_sub0_camera_mae",
			"img_vcore_sub0"/* parent */),
	GATE_IMG_V_V(CLK_IMG_VCORE_SUB0_SMI, "img_vcore_sub0_smi",
			"img_vcore_sub0"/* parent */),
	GATE_IMG_V(CLK_IMG_VCORE_SUB1, "img_vcore_sub1",
			"mminfra_ck"/* parent */, 3),
	GATE_IMG_V_V(CLK_IMG_VCORE_SUB1_CAMERA_P2, "img_vcore_sub1_camera_p2",
			"img_vcore_sub1"/* parent */),
	GATE_IMG_V_V(CLK_IMG_VCORE_SUB1_CAMERA_MAE, "img_vcore_sub1_camera_mae",
			"img_vcore_sub1"/* parent */),
	GATE_IMG_V_V(CLK_IMG_VCORE_SUB1_SMI, "img_vcore_sub1_smi",
			"img_vcore_sub1"/* parent */),
	GATE_IMG_V(CLK_IMG_VCORE_IMG_26M, "img_vcore_img_26m",
			"img_26m_ck"/* parent */, 4),
	GATE_IMG_V_V(CLK_IMG_VCORE_IMG_26M_CAMERA_P2, "img_vcore_img_26m_camera_p2",
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

#define GATE_TRAW_CAP_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &traw_cap_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_TRAW_CAP_DIP1_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate traw_cap_dip1_clks[] = {
	GATE_TRAW_CAP_DIP1(CLK_TRAW_CAP_DIP1_TRAW_CAP, "traw__dip1_cap",
			"img1_ck"/* parent */, 0),
	GATE_TRAW_CAP_DIP1_V(CLK_TRAW_CAP_DIP1_TRAW_CAP_CAMERA_P2, "traw__dip1_cap_camera_p2",
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

#define GATE_TRAW_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &traw_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_TRAW_DIP1_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate traw_dip1_clks[] = {
	GATE_TRAW_DIP1(CLK_TRAW_DIP1_LARB28, "traw_dip1_larb28",
			"img1_ck"/* parent */, 0),
	GATE_TRAW_DIP1_V(CLK_TRAW_DIP1_LARB28_CAMERA_P2, "traw_dip1_larb28_camera_p2",
			"traw_dip1_larb28"/* parent */),
	GATE_TRAW_DIP1_V(CLK_TRAW_DIP1_LARB28_SMI, "traw_dip1_larb28_smi",
			"traw_dip1_larb28"/* parent */),
	GATE_TRAW_DIP1(CLK_TRAW_DIP1_LARB40, "traw_dip1_larb40",
			"img1_ck"/* parent */, 1),
	GATE_TRAW_DIP1_V(CLK_TRAW_DIP1_LARB40_CAMERA_P2, "traw_dip1_larb40_camera_p2",
			"traw_dip1_larb40"/* parent */),
	GATE_TRAW_DIP1(CLK_TRAW_DIP1_TRAW, "traw_dip1_traw",
			"img1_ck"/* parent */, 2),
	GATE_TRAW_DIP1_V(CLK_TRAW_DIP1_TRAW_CAMERA_P2, "traw_dip1_traw_camera_p2",
			"traw_dip1_traw"/* parent */),
	GATE_TRAW_DIP1(CLK_TRAW_DIP1_GALS, "traw_dip1_gals",
			"img1_ck"/* parent */, 3),
	GATE_TRAW_DIP1_V(CLK_TRAW_DIP1_GALS_CAMERA_P2, "traw_dip1_gals_camera_p2",
			"traw_dip1_gals"/* parent */),
};

static const struct mtk_clk_desc traw_dip1_mcd = {
	.clks = traw_dip1_clks,
	.num_clks = CLK_TRAW_DIP1_NR_CLK,
};

static const struct mtk_gate_regs wpe1_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_WPE1_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &wpe1_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_WPE1_DIP1_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate wpe1_dip1_clks[] = {
	GATE_WPE1_DIP1(CLK_WPE1_DIP1_LARB11, "wpe1_dip1_larb11",
			"img1_ck"/* parent */, 0),
	GATE_WPE1_DIP1_V(CLK_WPE1_DIP1_LARB11_CAMERA_P2, "wpe1_dip1_larb11_camera_p2",
			"wpe1_dip1_larb11"/* parent */),
	GATE_WPE1_DIP1_V(CLK_WPE1_DIP1_LARB11_SMI, "wpe1_dip1_larb11_smi",
			"wpe1_dip1_larb11"/* parent */),
	GATE_WPE1_DIP1(CLK_WPE1_DIP1_WPE, "wpe1_dip1_wpe",
			"img1_ck"/* parent */, 1),
	GATE_WPE1_DIP1_V(CLK_WPE1_DIP1_WPE_CAMERA_P2, "wpe1_dip1_wpe_camera_p2",
			"wpe1_dip1_wpe"/* parent */),
	GATE_WPE1_DIP1(CLK_WPE1_DIP1_GALS0, "wpe1_dip1_gals0",
			"img1_ck"/* parent */, 2),
	GATE_WPE1_DIP1_V(CLK_WPE1_DIP1_GALS0_CAMERA_P2, "wpe1_dip1_gals0_camera_p2",
			"wpe1_dip1_gals0"/* parent */),
};

static const struct mtk_clk_desc wpe1_dip1_mcd = {
	.clks = wpe1_dip1_clks,
	.num_clks = CLK_WPE1_DIP1_NR_CLK,
};

static const struct mtk_gate_regs wpe2_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_WPE2_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &wpe2_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_WPE2_DIP1_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate wpe2_dip1_clks[] = {
	GATE_WPE2_DIP1(CLK_WPE2_DIP1_LARB11, "wpe2_dip1_larb11",
			"img1_ck"/* parent */, 0),
	GATE_WPE2_DIP1_V(CLK_WPE2_DIP1_LARB11_CAMERA_P2, "wpe2_dip1_larb11_camera_p2",
			"wpe2_dip1_larb11"/* parent */),
	GATE_WPE2_DIP1_V(CLK_WPE2_DIP1_LARB11_SMI, "wpe2_dip1_larb11_smi",
			"wpe2_dip1_larb11"/* parent */),
	GATE_WPE2_DIP1(CLK_WPE2_DIP1_WPE, "wpe2_dip1_wpe",
			"img1_ck"/* parent */, 1),
	GATE_WPE2_DIP1_V(CLK_WPE2_DIP1_WPE_CAMERA_P2, "wpe2_dip1_wpe_camera_p2",
			"wpe2_dip1_wpe"/* parent */),
	GATE_WPE2_DIP1(CLK_WPE2_DIP1_GALS0, "wpe2_dip1_gals0",
			"img1_ck"/* parent */, 2),
	GATE_WPE2_DIP1_V(CLK_WPE2_DIP1_GALS0_CAMERA_P2, "wpe2_dip1_gals0_camera_p2",
			"wpe2_dip1_gals0"/* parent */),
};

static const struct mtk_clk_desc wpe2_dip1_mcd = {
	.clks = wpe2_dip1_clks,
	.num_clks = CLK_WPE2_DIP1_NR_CLK,
};

static const struct mtk_gate_regs wpe3_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_WPE3_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &wpe3_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_WPE3_DIP1_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate wpe3_dip1_clks[] = {
	GATE_WPE3_DIP1(CLK_WPE3_DIP1_LARB11, "wpe3_dip1_larb11",
			"img1_ck"/* parent */, 0),
	GATE_WPE3_DIP1_V(CLK_WPE3_DIP1_LARB11_CAMERA_P2, "wpe3_dip1_larb11_camera_p2",
			"wpe3_dip1_larb11"/* parent */),
	GATE_WPE3_DIP1_V(CLK_WPE3_DIP1_LARB11_SMI, "wpe3_dip1_larb11_smi",
			"wpe3_dip1_larb11"/* parent */),
	GATE_WPE3_DIP1(CLK_WPE3_DIP1_WPE, "wpe3_dip1_wpe",
			"img1_ck"/* parent */, 1),
	GATE_WPE3_DIP1_V(CLK_WPE3_DIP1_WPE_CAMERA_P2, "wpe3_dip1_wpe_camera_p2",
			"wpe3_dip1_wpe"/* parent */),
	GATE_WPE3_DIP1(CLK_WPE3_DIP1_GALS0, "wpe3_dip1_gals0",
			"img1_ck"/* parent */, 2),
	GATE_WPE3_DIP1_V(CLK_WPE3_DIP1_GALS0_CAMERA_P2, "wpe3_dip1_gals0_camera_p2",
			"wpe3_dip1_gals0"/* parent */),
};

static const struct mtk_clk_desc wpe3_dip1_mcd = {
	.clks = wpe3_dip1_clks,
	.num_clks = CLK_WPE3_DIP1_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6899_img[] = {
	{
		.compatible = "mediatek,mt6899-dip_nr1_dip1",
		.data = &dip_nr1_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6899-dip_nr2_dip1",
		.data = &dip_nr2_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6899-dip_top_dip1",
		.data = &dip_top_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6899-imgsys_main",
		.data = &img_mcd,
	}, {
		.compatible = "mediatek,mt6899-img_vcore_d1a",
		.data = &img_v_mcd,
	}, {
		.compatible = "mediatek,mt6899-traw_cap_dip1",
		.data = &traw_cap_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6899-traw_dip1",
		.data = &traw_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6899-wpe1_dip1",
		.data = &wpe1_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6899-wpe2_dip1",
		.data = &wpe2_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6899-wpe3_dip1",
		.data = &wpe3_dip1_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6899_img_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6899_img_drv = {
	.probe = clk_mt6899_img_grp_probe,
	.driver = {
		.name = "clk-mt6899-img",
		.of_match_table = of_match_clk_mt6899_img,
	},
};

module_platform_driver(clk_mt6899_img_drv);
MODULE_LICENSE("GPL");
