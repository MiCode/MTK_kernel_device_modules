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

static const struct mtk_gate_regs mm10_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mm11_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

#define GATE_MM10(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM10_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

#define GATE_MM11(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm11_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM11_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate mm1_clks[] = {
	/* MM10 */
	GATE_MM10(CLK_MM1_DISPSYS1_CONFIG, "mm1_dispsys1_config",
			"disp_ck"/* parent */, 0),
	GATE_MM10_V(CLK_MM1_DISPSYS1_CONFIG_DISP, "mm1_dispsys1_config_disp",
			"mm1_dispsys1_config"/* parent */),
	GATE_MM10(CLK_MM1_DISP_MUTEX0, "mm1_disp_mutex0",
			"disp_ck"/* parent */, 1),
	GATE_MM10_V(CLK_MM1_DISP_MUTEX0_DISP, "mm1_disp_mutex0_disp",
			"mm1_disp_mutex0"/* parent */),
	GATE_MM10(CLK_MM1_DISP_DLI_ASYNC0, "mm1_disp_dli_async0",
			"disp_ck"/* parent */, 2),
	GATE_MM10_V(CLK_MM1_DISP_DLI_ASYNC0_DISP, "mm1_disp_dli_async0_disp",
			"mm1_disp_dli_async0"/* parent */),
	GATE_MM10(CLK_MM1_DISP_DLI_ASYNC1, "mm1_disp_dli_async1",
			"disp_ck"/* parent */, 3),
	GATE_MM10_V(CLK_MM1_DISP_DLI_ASYNC1_DISP, "mm1_disp_dli_async1_disp",
			"mm1_disp_dli_async1"/* parent */),
	GATE_MM10(CLK_MM1_DISP_DLI_ASYNC2, "mm1_disp_dli_async2",
			"disp_ck"/* parent */, 4),
	GATE_MM10_V(CLK_MM1_DISP_DLI_ASYNC2_DISP, "mm1_disp_dli_async2_disp",
			"mm1_disp_dli_async2"/* parent */),
	GATE_MM10(CLK_MM1_MDP_RDMA0, "mm1_mdp_rdma0",
			"disp_ck"/* parent */, 5),
	GATE_MM10_V(CLK_MM1_MDP_RDMA0_DISP, "mm1_mdp_rdma0_disp",
			"mm1_mdp_rdma0"/* parent */),
	GATE_MM10(CLK_MM1_DISP_R2Y0, "mm1_disp_r2y0",
			"disp_ck"/* parent */, 6),
	GATE_MM10_V(CLK_MM1_DISP_R2Y0_DISP, "mm1_disp_r2y0_disp",
			"mm1_disp_r2y0"/* parent */),
	GATE_MM10(CLK_MM1_DISP_SPLITTER0, "mm1_disp_splitter0",
			"disp_ck"/* parent */, 7),
	GATE_MM10_V(CLK_MM1_DISP_SPLITTER0_DISP, "mm1_disp_splitter0_disp",
			"mm1_disp_splitter0"/* parent */),
	GATE_MM10(CLK_MM1_DISP_SPLITTER1, "mm1_disp_splitter1",
			"disp_ck"/* parent */, 8),
	GATE_MM10_V(CLK_MM1_DISP_SPLITTER1_DISP, "mm1_disp_splitter1_disp",
			"mm1_disp_splitter1"/* parent */),
	GATE_MM10(CLK_MM1_DISP_VDCM0, "mm1_disp_vdcm0",
			"disp_ck"/* parent */, 9),
	GATE_MM10_V(CLK_MM1_DISP_VDCM0_DISP, "mm1_disp_vdcm0_disp",
			"mm1_disp_vdcm0"/* parent */),
	GATE_MM10(CLK_MM1_DISP_DSC_WRAP0, "mm1_disp_dsc_wrap0",
			"disp_ck"/* parent */, 10),
	GATE_MM10_V(CLK_MM1_DISP_DSC_WRAP0_DISP, "mm1_disp_dsc_wrap0_disp",
			"mm1_disp_dsc_wrap0"/* parent */),
	GATE_MM10(CLK_MM1_DISP_DSC_WRAP1, "mm1_disp_dsc_wrap1",
			"disp_ck"/* parent */, 11),
	GATE_MM10_V(CLK_MM1_DISP_DSC_WRAP1_DISP, "mm1_disp_dsc_wrap1_disp",
			"mm1_disp_dsc_wrap1"/* parent */),
	GATE_MM10(CLK_MM1_DISP_DSC_WRAP2, "mm1_disp_dsc_wrap2",
			"disp_ck"/* parent */, 12),
	GATE_MM10_V(CLK_MM1_DISP_DSC_WRAP2_DISP, "mm1_disp_dsc_wrap2_disp",
			"mm1_disp_dsc_wrap2"/* parent */),
	GATE_MM10(CLK_MM1_DISP_DP_INTF0, "mm1_DP_CLK",
			"disp_ck"/* parent */, 13),
	GATE_MM10_V(CLK_MM1_DISP_DP_INTF0_DISP, "mm1_dp_clk_disp",
			"mm1_DP_CLK"/* parent */),
	GATE_MM10(CLK_MM1_DISP_DSI0, "mm1_CLK0",
			"disp_ck"/* parent */, 14),
	GATE_MM10_V(CLK_MM1_DISP_DSI0_DISP, "mm1_clk0_disp",
			"mm1_CLK0"/* parent */),
	GATE_MM10(CLK_MM1_DISP_DSI1, "mm1_CLK1",
			"disp_ck"/* parent */, 15),
	GATE_MM10_V(CLK_MM1_DISP_DSI1_DISP, "mm1_clk1_disp",
			"mm1_CLK1"/* parent */),
	GATE_MM10(CLK_MM1_DISP_DSI2, "mm1_CLK2",
			"disp_ck"/* parent */, 16),
	GATE_MM10_V(CLK_MM1_DISP_DSI2_DISP, "mm1_clk2_disp",
			"mm1_CLK2"/* parent */),
	GATE_MM10(CLK_MM1_DISP_MERGE0, "mm1_disp_merge0",
			"disp_ck"/* parent */, 17),
	GATE_MM10_V(CLK_MM1_DISP_MERGE0_DISP, "mm1_disp_merge0_disp",
			"mm1_disp_merge0"/* parent */),
	GATE_MM10(CLK_MM1_DISP_WDMA0, "mm1_disp_wdma0",
			"disp_ck"/* parent */, 18),
	GATE_MM10_V(CLK_MM1_DISP_WDMA0_DISP, "mm1_disp_wdma0_disp",
			"mm1_disp_wdma0"/* parent */),
	GATE_MM10(CLK_MM1_SMI_SUB_COMM0, "mm1_ssc",
			"disp_ck"/* parent */, 19),
	GATE_MM10_V(CLK_MM1_SMI_SUB_COMM0_DISP, "mm1_ssc_disp",
			"mm1_ssc"/* parent */),
	GATE_MM10_V(CLK_MM1_SMI_SUB_COMM0_SMI, "mm1_ssc_smi",
			"mm1_ssc"/* parent */),
	GATE_MM10_V(CLK_MM1_SMI_SUB_COMM0_GENPD, "mm1_ssc_genpd",
			"mm1_ssc"/* parent */),
	GATE_MM10(CLK_MM1_DISP_WDMA1, "mm1_disp_wdma1",
			"disp_ck"/* parent */, 20),
	GATE_MM10_V(CLK_MM1_DISP_WDMA1_DISP, "mm1_disp_wdma1_disp",
			"mm1_disp_wdma1"/* parent */),
	GATE_MM10(CLK_MM1_DISP_WDMA2, "mm1_disp_wdma2",
			"disp_ck"/* parent */, 21),
	GATE_MM10_V(CLK_MM1_DISP_WDMA2_DISP, "mm1_disp_wdma2_disp",
			"mm1_disp_wdma2"/* parent */),
	GATE_MM10(CLK_MM1_DISP_GDMA0, "mm1_disp_gdma0",
			"disp_ck"/* parent */, 22),
	GATE_MM10_V(CLK_MM1_DISP_GDMA0_DISP, "mm1_disp_gdma0_disp",
			"mm1_disp_gdma0"/* parent */),
	GATE_MM10(CLK_MM1_DISP_DLI_ASYNC3, "mm1_disp_dli_async3",
			"disp_ck"/* parent */, 23),
	GATE_MM10_V(CLK_MM1_DISP_DLI_ASYNC3_DISP, "mm1_disp_dli_async3_disp",
			"mm1_disp_dli_async3"/* parent */),
	GATE_MM10(CLK_MM1_DISP_DLI_ASYNC4, "mm1_disp_dli_async4",
			"disp_ck"/* parent */, 24),
	GATE_MM10_V(CLK_MM1_DISP_DLI_ASYNC4_DISP, "mm1_disp_dli_async4_disp",
			"mm1_disp_dli_async4"/* parent */),
	/* MM11 */
	GATE_MM11(CLK_MM1_MOD1, "mm1_mod1",
			"disp_ck"/* parent */, 0),
	GATE_MM11_V(CLK_MM1_MOD1_DISP, "mm1_mod1_disp",
			"mm1_mod1"/* parent */),
	GATE_MM11(CLK_MM1_MOD2, "mm1_mod2",
			"disp_ck"/* parent */, 1),
	GATE_MM11_V(CLK_MM1_MOD2_DISP, "mm1_mod2_disp",
			"mm1_mod2"/* parent */),
	GATE_MM11(CLK_MM1_MOD3, "mm1_mod3",
			"disp_ck"/* parent */, 2),
	GATE_MM11_V(CLK_MM1_MOD3_DISP, "mm1_mod3_disp",
			"mm1_mod3"/* parent */),
	GATE_MM11(CLK_MM1_MOD4, "mm1_mod4",
			"disp_ck"/* parent */, 3),
	GATE_MM11_V(CLK_MM1_MOD4_DISP, "mm1_mod4_disp",
			"mm1_mod4"/* parent */),
	GATE_MM11(CLK_MM1_MOD5, "mm1_mod5",
			"disp_ck"/* parent */, 4),
	GATE_MM11_V(CLK_MM1_MOD5_DISP, "mm1_mod5_disp",
			"mm1_mod5"/* parent */),
	GATE_MM11(CLK_MM1_MOD6, "mm1_mod6",
			"disp_ck"/* parent */, 5),
	GATE_MM11_V(CLK_MM1_MOD6_DISP, "mm1_mod6_disp",
			"mm1_mod6"/* parent */),
	GATE_MM11(CLK_MM1_MOD7, "mm1_mod7",
			"disp_ck"/* parent */, 6),
	GATE_MM11_V(CLK_MM1_MOD7_DISP, "mm1_mod7_disp",
			"mm1_mod7"/* parent */),
	GATE_MM11(CLK_MM1_SUBSYS, "mm1_subsys_ck",
			"disp_ck"/* parent */, 7),
	GATE_MM11_V(CLK_MM1_SUBSYS_DISP, "mm1_subsys_ck_disp",
			"mm1_subsys_ck"/* parent */),
	GATE_MM11(CLK_MM1_DSI0, "mm1_dsi0_ck",
			"disp_ck"/* parent */, 8),
	GATE_MM11_V(CLK_MM1_DSI0_DISP, "mm1_dsi0_ck_disp",
			"mm1_dsi0_ck"/* parent */),
	GATE_MM11(CLK_MM1_DSI1, "mm1_dsi1_ck",
			"disp_ck"/* parent */, 9),
	GATE_MM11_V(CLK_MM1_DSI1_DISP, "mm1_dsi1_ck_disp",
			"mm1_dsi1_ck"/* parent */),
	GATE_MM11(CLK_MM1_DSI2, "mm1_dsi2_ck",
			"disp_ck"/* parent */, 10),
	GATE_MM11_V(CLK_MM1_DSI2_DISP, "mm1_dsi2_ck_disp",
			"mm1_dsi2_ck"/* parent */),
	GATE_MM11(CLK_MM1_DP, "mm1_dp_ck",
			"disp_ck"/* parent */, 11),
	GATE_MM11_V(CLK_MM1_DP_DISP, "mm1_dp_ck_disp",
			"mm1_dp_ck"/* parent */),
	GATE_MM11(CLK_MM1_F26M, "mm1_f26m_ck",
			"disp_ck"/* parent */, 12),
	GATE_MM11_V(CLK_MM1_F26M_DISP, "mm1_f26m_ck_disp",
			"mm1_f26m_ck"/* parent */),
};

static const struct mtk_clk_desc mm1_mcd = {
	.clks = mm1_clks,
	.num_clks = CLK_MM1_NR_CLK,
};

static const struct mtk_gate_regs mm0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mm1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

#define GATE_MM0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM0_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

#define GATE_MM1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM1_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate mm_clks[] = {
	/* MM0 */
	GATE_MM0(CLK_MM_CONFIG, "mm_config",
			"disp_ck"/* parent */, 0),
	GATE_MM0_V(CLK_MM_CONFIG_DISP, "mm_config_disp",
			"mm_config"/* parent */),
	GATE_MM0(CLK_MM_DISP_MUTEX0, "mm_disp_mutex0",
			"disp_ck"/* parent */, 1),
	GATE_MM0_V(CLK_MM_DISP_MUTEX0_DISP, "mm_disp_mutex0_disp",
			"mm_disp_mutex0"/* parent */),
	GATE_MM0(CLK_MM_DISP_AAL0, "mm_disp_aal0",
			"disp_ck"/* parent */, 2),
	GATE_MM0_V(CLK_MM_DISP_AAL0_DISP, "mm_disp_aal0_disp",
			"mm_disp_aal0"/* parent */),
	GATE_MM0(CLK_MM_DISP_AAL1, "mm_disp_aal1",
			"disp_ck"/* parent */, 3),
	GATE_MM0_V(CLK_MM_DISP_AAL1_DISP, "mm_disp_aal1_disp",
			"mm_disp_aal1"/* parent */),
	GATE_MM0(CLK_MM_DISP_C3D0, "mm_disp_c3d0",
			"disp_ck"/* parent */, 4),
	GATE_MM0_V(CLK_MM_DISP_C3D0_DISP, "mm_disp_c3d0_disp",
			"mm_disp_c3d0"/* parent */),
	GATE_MM0(CLK_MM_DISP_C3D1, "mm_disp_c3d1",
			"disp_ck"/* parent */, 5),
	GATE_MM0_V(CLK_MM_DISP_C3D1_DISP, "mm_disp_c3d1_disp",
			"mm_disp_c3d1"/* parent */),
	GATE_MM0(CLK_MM_DISP_CCORR0, "mm_disp_ccorr0",
			"disp_ck"/* parent */, 6),
	GATE_MM0_V(CLK_MM_DISP_CCORR0_DISP, "mm_disp_ccorr0_disp",
			"mm_disp_ccorr0"/* parent */),
	GATE_MM0(CLK_MM_DISP_CCORR1, "mm_disp_ccorr1",
			"disp_ck"/* parent */, 7),
	GATE_MM0_V(CLK_MM_DISP_CCORR1_DISP, "mm_disp_ccorr1_disp",
			"mm_disp_ccorr1"/* parent */),
	GATE_MM0(CLK_MM_DISP_CCORR2, "mm_disp_ccorr2",
			"disp_ck"/* parent */, 8),
	GATE_MM0_V(CLK_MM_DISP_CCORR2_DISP, "mm_disp_ccorr2_disp",
			"mm_disp_ccorr2"/* parent */),
	GATE_MM0(CLK_MM_DISP_CCORR3, "mm_disp_ccorr3",
			"disp_ck"/* parent */, 9),
	GATE_MM0_V(CLK_MM_DISP_CCORR3_DISP, "mm_disp_ccorr3_disp",
			"mm_disp_ccorr3"/* parent */),
	GATE_MM0(CLK_MM_DISP_CHIST0, "mm_disp_chist0",
			"disp_ck"/* parent */, 10),
	GATE_MM0_V(CLK_MM_DISP_CHIST0_DISP, "mm_disp_chist0_disp",
			"mm_disp_chist0"/* parent */),
	GATE_MM0(CLK_MM_DISP_CHIST1, "mm_disp_chist1",
			"disp_ck"/* parent */, 11),
	GATE_MM0_V(CLK_MM_DISP_CHIST1_DISP, "mm_disp_chist1_disp",
			"mm_disp_chist1"/* parent */),
	GATE_MM0(CLK_MM_DISP_COLOR0, "mm_disp_color0",
			"disp_ck"/* parent */, 12),
	GATE_MM0_V(CLK_MM_DISP_COLOR0_DISP, "mm_disp_color0_disp",
			"mm_disp_color0"/* parent */),
	GATE_MM0(CLK_MM_DISP_COLOR1, "mm_disp_color1",
			"disp_ck"/* parent */, 13),
	GATE_MM0_V(CLK_MM_DISP_COLOR1_DISP, "mm_disp_color1_disp",
			"mm_disp_color1"/* parent */),
	GATE_MM0(CLK_MM_DISP_DITHER0, "mm_disp_dither0",
			"disp_ck"/* parent */, 14),
	GATE_MM0_V(CLK_MM_DISP_DITHER0_DISP, "mm_disp_dither0_disp",
			"mm_disp_dither0"/* parent */),
	GATE_MM0(CLK_MM_DISP_DITHER1, "mm_disp_dither1",
			"disp_ck"/* parent */, 15),
	GATE_MM0_V(CLK_MM_DISP_DITHER1_DISP, "mm_disp_dither1_disp",
			"mm_disp_dither1"/* parent */),
	GATE_MM0(CLK_MM_DISP_DITHER2, "mm_disp_dither2",
			"disp_ck"/* parent */, 16),
	GATE_MM0_V(CLK_MM_DISP_DITHER2_DISP, "mm_disp_dither2_disp",
			"mm_disp_dither2"/* parent */),
	GATE_MM0(CLK_MM_DLI_ASYNC0, "mm_dli_async0",
			"disp_ck"/* parent */, 17),
	GATE_MM0_V(CLK_MM_DLI_ASYNC0_DISP, "mm_dli_async0_disp",
			"mm_dli_async0"/* parent */),
	GATE_MM0(CLK_MM_DLI_ASYNC1, "mm_dli_async1",
			"disp_ck"/* parent */, 18),
	GATE_MM0_V(CLK_MM_DLI_ASYNC1_DISP, "mm_dli_async1_disp",
			"mm_dli_async1"/* parent */),
	GATE_MM0(CLK_MM_DLI_ASYNC2, "mm_dli_async2",
			"disp_ck"/* parent */, 19),
	GATE_MM0_V(CLK_MM_DLI_ASYNC2_DISP, "mm_dli_async2_disp",
			"mm_dli_async2"/* parent */),
	GATE_MM0(CLK_MM_DLI_ASYNC3, "mm_dli_async3",
			"disp_ck"/* parent */, 20),
	GATE_MM0_V(CLK_MM_DLI_ASYNC3_DISP, "mm_dli_async3_disp",
			"mm_dli_async3"/* parent */),
	GATE_MM0(CLK_MM_DLI_ASYNC4, "mm_dli_async4",
			"disp_ck"/* parent */, 21),
	GATE_MM0_V(CLK_MM_DLI_ASYNC4_DISP, "mm_dli_async4_disp",
			"mm_dli_async4"/* parent */),
	GATE_MM0(CLK_MM_DLI_ASYNC5, "mm_dli_async5",
			"disp_ck"/* parent */, 22),
	GATE_MM0_V(CLK_MM_DLI_ASYNC5_DISP, "mm_dli_async5_disp",
			"mm_dli_async5"/* parent */),
	GATE_MM0(CLK_MM_DLI_ASYNC6, "mm_dli_async6",
			"disp_ck"/* parent */, 23),
	GATE_MM0_V(CLK_MM_DLI_ASYNC6_DISP, "mm_dli_async6_disp",
			"mm_dli_async6"/* parent */),
	GATE_MM0(CLK_MM_DLI_ASYNC7, "mm_dli_async7",
			"disp_ck"/* parent */, 24),
	GATE_MM0_V(CLK_MM_DLI_ASYNC7_DISP, "mm_dli_async7_disp",
			"mm_dli_async7"/* parent */),
	GATE_MM0(CLK_MM_DLO_ASYNC0, "mm_dlo_async0",
			"disp_ck"/* parent */, 25),
	GATE_MM0_V(CLK_MM_DLO_ASYNC0_DISP, "mm_dlo_async0_disp",
			"mm_dlo_async0"/* parent */),
	GATE_MM0(CLK_MM_DLO_ASYNC1, "mm_dlo_async1",
			"disp_ck"/* parent */, 26),
	GATE_MM0_V(CLK_MM_DLO_ASYNC1_DISP, "mm_dlo_async1_disp",
			"mm_dlo_async1"/* parent */),
	GATE_MM0(CLK_MM_DLO_ASYNC2, "mm_dlo_async2",
			"disp_ck"/* parent */, 27),
	GATE_MM0_V(CLK_MM_DLO_ASYNC2_DISP, "mm_dlo_async2_disp",
			"mm_dlo_async2"/* parent */),
	GATE_MM0(CLK_MM_DLO_ASYNC3, "mm_dlo_async3",
			"disp_ck"/* parent */, 28),
	GATE_MM0_V(CLK_MM_DLO_ASYNC3_DISP, "mm_dlo_async3_disp",
			"mm_dlo_async3"/* parent */),
	GATE_MM0(CLK_MM_DLO_ASYNC4, "mm_dlo_async4",
			"disp_ck"/* parent */, 29),
	GATE_MM0_V(CLK_MM_DLO_ASYNC4_DISP, "mm_dlo_async4_disp",
			"mm_dlo_async4"/* parent */),
	GATE_MM0(CLK_MM_DISP_GAMMA0, "mm_disp_gamma0",
			"disp_ck"/* parent */, 30),
	GATE_MM0_V(CLK_MM_DISP_GAMMA0_DISP, "mm_disp_gamma0_disp",
			"mm_disp_gamma0"/* parent */),
	GATE_MM0(CLK_MM_DISP_GAMMA1, "mm_disp_gamma1",
			"disp_ck"/* parent */, 31),
	GATE_MM0_V(CLK_MM_DISP_GAMMA1_DISP, "mm_disp_gamma1_disp",
			"mm_disp_gamma1"/* parent */),
	/* MM1 */
	GATE_MM1(CLK_MM_MDP_AAL0, "mm_mdp_aal0",
			"disp_ck"/* parent */, 0),
	GATE_MM1_V(CLK_MM_MDP_AAL0_DISP, "mm_mdp_aal0_disp",
			"mm_mdp_aal0"/* parent */),
	GATE_MM1(CLK_MM_MDP_RDMA0, "mm_mdp_rdma0",
			"disp_ck"/* parent */, 1),
	GATE_MM1_V(CLK_MM_MDP_RDMA0_DISP, "mm_mdp_rdma0_disp",
			"mm_mdp_rdma0"/* parent */),
	GATE_MM1(CLK_MM_DISP_ODDMR0, "mm_disp_oddmr0",
			"disp_ck"/* parent */, 2),
	GATE_MM1_V(CLK_MM_DISP_ODDMR0_DISP, "mm_disp_oddmr0_disp",
			"mm_disp_oddmr0"/* parent */),
	GATE_MM1(CLK_MM_DISP_POSTALIGN0, "mm_disp_postalign0",
			"disp_ck"/* parent */, 3),
	GATE_MM1_V(CLK_MM_DISP_POSTALIGN0_DISP, "mm_disp_postalign0_disp",
			"mm_disp_postalign0"/* parent */),
	GATE_MM1(CLK_MM_DISP_POSTMASK0, "mm_disp_postmask0",
			"disp_ck"/* parent */, 4),
	GATE_MM1_V(CLK_MM_DISP_POSTMASK0_DISP, "mm_disp_postmask0_disp",
			"mm_disp_postmask0"/* parent */),
	GATE_MM1(CLK_MM_DISP_POSTMASK1, "mm_disp_postmask1",
			"disp_ck"/* parent */, 5),
	GATE_MM1_V(CLK_MM_DISP_POSTMASK1_DISP, "mm_disp_postmask1_disp",
			"mm_disp_postmask1"/* parent */),
	GATE_MM1(CLK_MM_DISP_RSZ0, "mm_disp_rsz0",
			"disp_ck"/* parent */, 6),
	GATE_MM1_V(CLK_MM_DISP_RSZ0_DISP, "mm_disp_rsz0_disp",
			"mm_disp_rsz0"/* parent */),
	GATE_MM1(CLK_MM_DISP_RSZ1, "mm_disp_rsz1",
			"disp_ck"/* parent */, 7),
	GATE_MM1_V(CLK_MM_DISP_RSZ1_DISP, "mm_disp_rsz1_disp",
			"mm_disp_rsz1"/* parent */),
	GATE_MM1(CLK_MM_DISP_SPR0, "mm_disp_spr0",
			"disp_ck"/* parent */, 8),
	GATE_MM1_V(CLK_MM_DISP_SPR0_DISP, "mm_disp_spr0_disp",
			"mm_disp_spr0"/* parent */),
	GATE_MM1(CLK_MM_DISP_TDSHP0, "mm_disp_tdshp0",
			"disp_ck"/* parent */, 9),
	GATE_MM1_V(CLK_MM_DISP_TDSHP0_DISP, "mm_disp_tdshp0_disp",
			"mm_disp_tdshp0"/* parent */),
	GATE_MM1(CLK_MM_DISP_TDSHP1, "mm_disp_tdshp1",
			"disp_ck"/* parent */, 10),
	GATE_MM1_V(CLK_MM_DISP_TDSHP1_DISP, "mm_disp_tdshp1_disp",
			"mm_disp_tdshp1"/* parent */),
	GATE_MM1(CLK_MM_DISP_WDMA1, "mm_disp_wdma1",
			"disp_ck"/* parent */, 11),
	GATE_MM1_V(CLK_MM_DISP_WDMA1_DISP, "mm_disp_wdma1_disp",
			"mm_disp_wdma1"/* parent */),
	GATE_MM1(CLK_MM_DISP_Y2R0, "mm_disp_y2r0",
			"disp_ck"/* parent */, 12),
	GATE_MM1_V(CLK_MM_DISP_Y2R0_DISP, "mm_disp_y2r0_disp",
			"mm_disp_y2r0"/* parent */),
	GATE_MM1(CLK_MM_MDP_AAL1, "mm_mdp_aal1",
			"disp_ck"/* parent */, 13),
	GATE_MM1_V(CLK_MM_MDP_AAL1_DISP, "mm_mdp_aal1_disp",
			"mm_mdp_aal1"/* parent */),
	GATE_MM1(CLK_MM_SMI_SUB_COMM0, "mm_ssc",
			"disp_ck"/* parent */, 14),
	GATE_MM1_V(CLK_MM_SMI_SUB_COMM0_DISP, "mm_ssc_disp",
			"mm_ssc"/* parent */),
	GATE_MM1_V(CLK_MM_SMI_SUB_COMM0_SMI, "mm_ssc_smi",
			"mm_ssc"/* parent */),
	GATE_MM1_V(CLK_MM_SMI_SUB_COMM0_GENPD, "mm_ssc_genpd",
			"mm_ssc"/* parent */),
	GATE_MM1(CLK_MM_DISP_RSZ0_MOUT_RELAY, "mm_disp_rsz0_mout_relay",
			"disp_ck"/* parent */, 15),
	GATE_MM1_V(CLK_MM_DISP_RSZ0_MOUT_RELAY_DISP, "mm_disp_rsz0_mout_relay_disp",
			"mm_disp_rsz0_mout_relay"/* parent */),
	GATE_MM1(CLK_MM_DISP_RSZ1_MOUT_RELAY, "mm_disp_rsz1_mout_relay",
			"disp_ck"/* parent */, 16),
	GATE_MM1_V(CLK_MM_DISP_RSZ1_MOUT_RELAY_DISP, "mm_disp_rsz1_mout_relay_disp",
			"mm_disp_rsz1_mout_relay"/* parent */),
};

static const struct mtk_clk_desc mm_mcd = {
	.clks = mm_clks,
	.num_clks = CLK_MM_NR_CLK,
};

static const struct mtk_gate_regs mminfra_ao_config0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mminfra_ao_config1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

#define GATE_MMINFRA_AO_CONFIG0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mminfra_ao_config0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MMINFRA_AO_CONFIG0_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

#define GATE_MMINFRA_AO_CONFIG1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mminfra_ao_config1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MMINFRA_AO_CONFIG1_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate mminfra_ao_config_clks[] = {
	/* MMINFRA_AO_CONFIG0 */
	GATE_MMINFRA_AO_CONFIG0(CLK_MMINFRA_AO_GCE_D, "mminfra_ao_gce_d",
			"mminfra_ck"/* parent */, 0),
	GATE_MMINFRA_AO_CONFIG0_V(CLK_MMINFRA_AO_GCE_D_CMDQ, "mminfra_ao_gce_d_cmdq",
			"mminfra_ao_gce_d"/* parent */),
	GATE_MMINFRA_AO_CONFIG0(CLK_MMINFRA_AO_GCE_M, "mminfra_ao_gce_m",
			"mminfra_ck"/* parent */, 1),
	GATE_MMINFRA_AO_CONFIG0_V(CLK_MMINFRA_AO_GCE_M_CMDQ, "mminfra_ao_gce_m_cmdq",
			"mminfra_ao_gce_m"/* parent */),
	/* MMINFRA_AO_CONFIG1 */
	GATE_MMINFRA_AO_CONFIG1(CLK_MMINFRA_AO_GCE_26M, "mminfra_ao_gce_26m",
			"mminfra_ck"/* parent */, 17),
	GATE_MMINFRA_AO_CONFIG1_V(CLK_MMINFRA_AO_GCE_26M_CMDQ, "mminfra_ao_gce_26m_cmdq",
			"mminfra_ao_gce_26m"/* parent */),
};

static const struct mtk_clk_desc mminfra_ao_config_mcd = {
	.clks = mminfra_ao_config_clks,
	.num_clks = CLK_MMINFRA_AO_CONFIG_NR_CLK,
};

static const struct mtk_gate_regs ovl_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

#define GATE_OVL(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ovl_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_OVL_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate ovl_clks[] = {
	GATE_OVL(CLK_OVLSYS_CONFIG, "ovlsys_config",
			"disp_ck"/* parent */, 0),
	GATE_OVL_V(CLK_OVLSYS_CONFIG_DISP, "ovlsys_config_disp",
			"ovlsys_config"/* parent */),
	GATE_OVL(CLK_OVL_DISP_FAKE_ENG0, "ovl_disp_fake_eng0",
			"disp_ck"/* parent */, 1),
	GATE_OVL_V(CLK_OVL_DISP_FAKE_ENG0_DISP, "ovl_disp_fake_eng0_disp",
			"ovl_disp_fake_eng0"/* parent */),
	GATE_OVL(CLK_OVL_DISP_FAKE_ENG1, "ovl_disp_fake_eng1",
			"disp_ck"/* parent */, 2),
	GATE_OVL_V(CLK_OVL_DISP_FAKE_ENG1_DISP, "ovl_disp_fake_eng1_disp",
			"ovl_disp_fake_eng1"/* parent */),
	GATE_OVL(CLK_OVL_DISP_MUTEX0, "ovl_disp_mutex0",
			"disp_ck"/* parent */, 3),
	GATE_OVL_V(CLK_OVL_DISP_MUTEX0_DISP, "ovl_disp_mutex0_disp",
			"ovl_disp_mutex0"/* parent */),
	GATE_OVL(CLK_OVL_DISP_OVL0_2L, "ovl_disp_ovl0_2l",
			"disp_ck"/* parent */, 4),
	GATE_OVL_V(CLK_OVL_DISP_OVL0_2L_DISP, "ovl_disp_ovl0_2l_disp",
			"ovl_disp_ovl0_2l"/* parent */),
	GATE_OVL(CLK_OVL_DISP_OVL1_2L, "ovl_disp_ovl1_2l",
			"disp_ck"/* parent */, 5),
	GATE_OVL_V(CLK_OVL_DISP_OVL1_2L_DISP, "ovl_disp_ovl1_2l_disp",
			"ovl_disp_ovl1_2l"/* parent */),
	GATE_OVL(CLK_OVL_DISP_OVL2_2L, "ovl_disp_ovl2_2l",
			"disp_ck"/* parent */, 6),
	GATE_OVL_V(CLK_OVL_DISP_OVL2_2L_DISP, "ovl_disp_ovl2_2l_disp",
			"ovl_disp_ovl2_2l"/* parent */),
	GATE_OVL(CLK_OVL_DISP_OVL3_2L, "ovl_disp_ovl3_2l",
			"disp_ck"/* parent */, 7),
	GATE_OVL_V(CLK_OVL_DISP_OVL3_2L_DISP, "ovl_disp_ovl3_2l_disp",
			"ovl_disp_ovl3_2l"/* parent */),
	GATE_OVL(CLK_OVL_DISP_RSZ1, "ovl_disp_rsz1",
			"disp_ck"/* parent */, 8),
	GATE_OVL_V(CLK_OVL_DISP_RSZ1_DISP, "ovl_disp_rsz1_disp",
			"ovl_disp_rsz1"/* parent */),
	GATE_OVL(CLK_OVL_MDP_RSZ0, "ovl_mdp_rsz0",
			"disp_ck"/* parent */, 9),
	GATE_OVL_V(CLK_OVL_MDP_RSZ0_DISP, "ovl_mdp_rsz0_disp",
			"ovl_mdp_rsz0"/* parent */),
	GATE_OVL(CLK_OVL_DISP_WDMA0, "ovl_disp_wdma0",
			"disp_ck"/* parent */, 10),
	GATE_OVL_V(CLK_OVL_DISP_WDMA0_DISP, "ovl_disp_wdma0_disp",
			"ovl_disp_wdma0"/* parent */),
	GATE_OVL(CLK_OVL_DISP_UFBC_WDMA0, "ovl_disp_ufbc_wdma0",
			"disp_ck"/* parent */, 11),
	GATE_OVL_V(CLK_OVL_DISP_UFBC_WDMA0_DISP, "ovl_disp_ufbc_wdma0_disp",
			"ovl_disp_ufbc_wdma0"/* parent */),
	GATE_OVL(CLK_OVL_DISP_WDMA2, "ovl_disp_wdma2",
			"disp_ck"/* parent */, 12),
	GATE_OVL_V(CLK_OVL_DISP_WDMA2_DISP, "ovl_disp_wdma2_disp",
			"ovl_disp_wdma2"/* parent */),
	GATE_OVL(CLK_OVL_DISP_DLI_ASYNC0, "ovl_disp_dli_async0",
			"disp_ck"/* parent */, 13),
	GATE_OVL_V(CLK_OVL_DISP_DLI_ASYNC0_DISP, "ovl_disp_dli_async0_disp",
			"ovl_disp_dli_async0"/* parent */),
	GATE_OVL(CLK_OVL_DISP_DLI_ASYNC1, "ovl_disp_dli_async1",
			"disp_ck"/* parent */, 14),
	GATE_OVL_V(CLK_OVL_DISP_DLI_ASYNC1_DISP, "ovl_disp_dli_async1_disp",
			"ovl_disp_dli_async1"/* parent */),
	GATE_OVL(CLK_OVL_DISP_DLI_ASYNC2, "ovl_disp_dli_async2",
			"disp_ck"/* parent */, 15),
	GATE_OVL_V(CLK_OVL_DISP_DLI_ASYNC2_DISP, "ovl_disp_dli_async2_disp",
			"ovl_disp_dli_async2"/* parent */),
	GATE_OVL(CLK_OVL_DISP_DL0_ASYNC0, "ovl_disp_dl0_async0",
			"disp_ck"/* parent */, 16),
	GATE_OVL_V(CLK_OVL_DISP_DL0_ASYNC0_DISP, "ovl_disp_dl0_async0_disp",
			"ovl_disp_dl0_async0"/* parent */),
	GATE_OVL(CLK_OVL_DISP_DL0_ASYNC1, "ovl_disp_dl0_async1",
			"disp_ck"/* parent */, 17),
	GATE_OVL_V(CLK_OVL_DISP_DL0_ASYNC1_DISP, "ovl_disp_dl0_async1_disp",
			"ovl_disp_dl0_async1"/* parent */),
	GATE_OVL(CLK_OVL_DISP_DL0_ASYNC2, "ovl_disp_dl0_async2",
			"disp_ck"/* parent */, 18),
	GATE_OVL_V(CLK_OVL_DISP_DL0_ASYNC2_DISP, "ovl_disp_dl0_async2_disp",
			"ovl_disp_dl0_async2"/* parent */),
	GATE_OVL(CLK_OVL_DISP_DL0_ASYNC3, "ovl_disp_dl0_async3",
			"disp_ck"/* parent */, 19),
	GATE_OVL_V(CLK_OVL_DISP_DL0_ASYNC3_DISP, "ovl_disp_dl0_async3_disp",
			"ovl_disp_dl0_async3"/* parent */),
	GATE_OVL(CLK_OVL_DISP_DL0_ASYNC4, "ovl_disp_dl0_async4",
			"disp_ck"/* parent */, 20),
	GATE_OVL_V(CLK_OVL_DISP_DL0_ASYNC4_DISP, "ovl_disp_dl0_async4_disp",
			"ovl_disp_dl0_async4"/* parent */),
	GATE_OVL(CLK_OVL_DISP_DL0_ASYNC5, "ovl_disp_dl0_async5",
			"disp_ck"/* parent */, 21),
	GATE_OVL_V(CLK_OVL_DISP_DL0_ASYNC5_DISP, "ovl_disp_dl0_async5_disp",
			"ovl_disp_dl0_async5"/* parent */),
	GATE_OVL(CLK_OVL_DISP_DL0_ASYNC6, "ovl_disp_dl0_async6",
			"disp_ck"/* parent */, 22),
	GATE_OVL_V(CLK_OVL_DISP_DL0_ASYNC6_DISP, "ovl_disp_dl0_async6_disp",
			"ovl_disp_dl0_async6"/* parent */),
	GATE_OVL(CLK_OVL_INLINEROT0, "ovl_inlinerot0",
			"disp_ck"/* parent */, 23),
	GATE_OVL_V(CLK_OVL_INLINEROT0_DISP, "ovl_inlinerot0_disp",
			"ovl_inlinerot0"/* parent */),
	GATE_OVL(CLK_OVL_SMI_SUB_COMM0, "ovl_ssc",
			"disp_ck"/* parent */, 24),
	GATE_OVL_V(CLK_OVL_SMI_SUB_COMM0_DISP, "ovl_ssc_disp",
			"ovl_ssc"/* parent */),
	GATE_OVL_V(CLK_OVL_SMI_SUB_COMM0_SMI, "ovl_ssc_smi",
			"ovl_ssc"/* parent */),
	GATE_OVL_V(CLK_OVL_SMI_SUB_COMM0_GENPD, "ovl_ssc_genpd",
			"ovl_ssc"/* parent */),
	GATE_OVL(CLK_OVL_DISP_Y2R0, "ovl_disp_y2r0",
			"disp_ck"/* parent */, 25),
	GATE_OVL_V(CLK_OVL_DISP_Y2R0_DISP, "ovl_disp_y2r0_disp",
			"ovl_disp_y2r0"/* parent */),
	GATE_OVL(CLK_OVL_DISP_Y2R1, "ovl_disp_y2r1",
			"disp_ck"/* parent */, 26),
	GATE_OVL_V(CLK_OVL_DISP_Y2R1_DISP, "ovl_disp_y2r1_disp",
			"ovl_disp_y2r1"/* parent */),
	GATE_OVL(CLK_OVL_DISP_OVL4_2L, "ovl_disp_ovl4_2l",
			"disp_ck"/* parent */, 27),
	GATE_OVL_V(CLK_OVL_DISP_OVL4_2L_DISP, "ovl_disp_ovl4_2l_disp",
			"ovl_disp_ovl4_2l"/* parent */),
};

static const struct mtk_clk_desc ovl_mcd = {
	.clks = ovl_clks,
	.num_clks = CLK_OVL_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6899_mmsys[] = {
	{
		.compatible = "mediatek,mt6899-mmsys1",
		.data = &mm1_mcd,
	}, {
		.compatible = "mediatek,mt6899-mmsys0",
		.data = &mm_mcd,
	}, {
		.compatible = "mediatek,mt6899-mminfra_ao_config",
		.data = &mminfra_ao_config_mcd,
	}, {
		.compatible = "mediatek,mt6899-ovlsys_config",
		.data = &ovl_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6899_mmsys_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6899_mmsys_drv = {
	.probe = clk_mt6899_mmsys_grp_probe,
	.driver = {
		.name = "clk-mt6899-mmsys",
		.of_match_table = of_match_clk_mt6899_mmsys,
	},
};

module_platform_driver(clk_mt6899_mmsys_drv);
MODULE_LICENSE("GPL");
