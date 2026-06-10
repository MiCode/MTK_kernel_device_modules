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

static const struct mtk_gate_regs mm0b0_cg_regs = {
	.set_ofs = 0xA64,
	.clr_ofs = 0xA68,
	.sta_ofs = 0xA60,
};

static const struct mtk_gate_regs mm0b1_cg_regs = {
	.set_ofs = 0xA70,
	.clr_ofs = 0xA74,
	.sta_ofs = 0xA6C,
};

static const struct mtk_gate_regs mm0b1_hwv_regs = {
	.set_ofs = 0x90,
	.clr_ofs = 0x94,
	.sta_ofs = 0x12630,
};

#define GATE_MM0B0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm0b0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
		.flags = RES_FRAMEWORK_VDISP,		\
	}

#define GATE_MM0B0_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_MM0B1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm0b1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_MM0B1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_HWV_MM0B1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &mm0b1_cg_regs,			\
		.hwv_regs = &mm0b1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VDISP | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate mm0b_clks[] = {
	/* MM0B0 */
	GATE_MM0B0(MM0B_CONFIG, "mm0b_config",
			"mm_disp_ck"/* parent */, 0),
	GATE_MM0B0_V(MM0B_CONFIG_DISP, "mm0b_config_disp",
	        "mm0b_config"/* parent */),
	GATE_MM0B0(MM0B_DISP_MUTEX0, "mm0b_disp_mutex0",
			"mm_disp_ck"/* parent */, 1),
	GATE_MM0B0_V(MM0B_DISP_MUTEX0_DISP, "mm0b_disp_mutex0_disp",
	        "mm0b_disp_mutex0"/* parent */),
	GATE_MM0B0(MM0B_DISP_AAL0, "mm0b_disp_aal0",
			"mm_disp_ck"/* parent */, 2),
	GATE_MM0B0_V(MM0B_DISP_AAL0_DISP, "mm0b_disp_aal0_disp",
	        "mm0b_disp_aal0"/* parent */),
	GATE_MM0B0(MM0B_DISP_C3D0, "mm0b_disp_c3d0",
			"mm_disp_ck"/* parent */, 3),
	GATE_MM0B0_V(MM0B_DISP_C3D0_DISP, "mm0b_disp_c3d0_disp",
	        "mm0b_disp_c3d0"/* parent */),
	GATE_MM0B0(MM0B_DISP_C3D1, "mm0b_disp_c3d1",
			"mm_disp_ck"/* parent */, 4),
	GATE_MM0B0_V(MM0B_DISP_C3D1_DISP, "mm0b_disp_c3d1_disp",
	        "mm0b_disp_c3d1"/* parent */),
	GATE_MM0B0(MM0B_DISP_CCORR0, "mm0b_disp_ccorr0",
			"mm_disp_ck"/* parent */, 5),
	GATE_MM0B0_V(MM0B_DISP_CCORR0_DISP, "mm0b_disp_ccorr0_disp",
	        "mm0b_disp_ccorr0"/* parent */),
	GATE_MM0B0(MM0B_DISP_CCORR1, "mm0b_disp_ccorr1",
			"mm_disp_ck"/* parent */, 6),
	GATE_MM0B0_V(MM0B_DISP_CCORR1_DISP, "mm0b_disp_ccorr1_disp",
	        "mm0b_disp_ccorr1"/* parent */),
	GATE_MM0B0(MM0B_DISP_COLOR0, "mm0b_disp_color0",
			"mm_disp_ck"/* parent */, 7),
	GATE_MM0B0_V(MM0B_DISP_COLOR0_DISP, "mm0b_disp_color0_disp",
	        "mm0b_disp_color0"/* parent */),
	GATE_MM0B0(MM0B_DISP_DITHER0, "mm0b_disp_dither0",
			"mm_disp_ck"/* parent */, 8),
	GATE_MM0B0_V(MM0B_DISP_DITHER0_DISP, "mm0b_disp_dither0_disp",
	        "mm0b_disp_dither0"/* parent */),
	GATE_MM0B0(MM0B_DISP_DITHER1, "mm0b_disp_dither1",
			"mm_disp_ck"/* parent */, 9),
	GATE_MM0B0_V(MM0B_DISP_DITHER1_DISP, "mm0b_disp_dither1_disp",
	        "mm0b_disp_dither1"/* parent */),
	GATE_MM0B0(MM0B_DISP_DLI_ASYNC0, "mm0b_disp_dli_as0",
			"mm_disp_ck"/* parent */, 10),
	GATE_MM0B0_V(MM0B_DISP_DLI_ASYNC0_DISP, "mm0b_disp_dli_as0_disp",
	        "mm0b_disp_dli_as0"/* parent */),
	GATE_MM0B0(MM0B_DISP_DLI_ASYNC1, "mm0b_disp_dli_as1",
			"mm_disp_ck"/* parent */, 11),
	GATE_MM0B0_V(MM0B_DISP_DLI_ASYNC1_DISP, "mm0b_disp_dli_as1_disp",
	        "mm0b_disp_dli_as1"/* parent */),
	GATE_MM0B0(MM0B_DISP_DLI_ASYNC2, "mm0b_disp_dli_as2",
			"mm_disp_ck"/* parent */, 12),
	GATE_MM0B0_V(MM0B_DISP_DLI_ASYNC2_DISP, "mm0b_disp_dli_as2_disp",
	        "mm0b_disp_dli_as2"/* parent */),
	GATE_MM0B0(MM0B_DISP_DLI_ASYNC3, "mm0b_disp_dli_as3",
			"mm_disp_ck"/* parent */, 13),
	GATE_MM0B0_V(MM0B_DISP_DLI_ASYNC3_DISP, "mm0b_disp_dli_as3_disp",
	        "mm0b_disp_dli_as3"/* parent */),
	GATE_MM0B0(MM0B_DISP_DLI_ASYNC4, "mm0b_disp_dli_as4",
			"mm_disp_ck"/* parent */, 14),
	GATE_MM0B0_V(MM0B_DISP_DLI_ASYNC4_DISP, "mm0b_disp_dli_as4_disp",
	        "mm0b_disp_dli_as4"/* parent */),
	GATE_MM0B0(MM0B_DISP_DLI_ASYNC5, "mm0b_disp_dli_as5",
			"mm_disp_ck"/* parent */, 15),
	GATE_MM0B0_V(MM0B_DISP_DLI_ASYNC5_DISP, "mm0b_disp_dli_as5_disp",
	        "mm0b_disp_dli_as5"/* parent */),
	GATE_MM0B0(MM0B_DISP_DLI_ASYNC6, "mm0b_disp_dli_as6",
			"mm_disp_ck"/* parent */, 16),
	GATE_MM0B0_V(MM0B_DISP_DLI_ASYNC6_DISP, "mm0b_disp_dli_as6_disp",
	        "mm0b_disp_dli_as6"/* parent */),
	GATE_MM0B0(MM0B_DISP_DLI_ASYNC7, "mm0b_disp_dli_as7",
			"mm_disp_ck"/* parent */, 17),
	GATE_MM0B0_V(MM0B_DISP_DLI_ASYNC7_DISP, "mm0b_disp_dli_as7_disp",
	        "mm0b_disp_dli_as7"/* parent */),
	GATE_MM0B0(MM0B_DISP_DLI_ASYNC8, "mm0b_disp_dli_as8",
			"mm_disp_ck"/* parent */, 18),
	GATE_MM0B0_V(MM0B_DISP_DLI_ASYNC8_DISP, "mm0b_disp_dli_as8_disp",
	        "mm0b_disp_dli_as8"/* parent */),
	GATE_MM0B0(MM0B_DISP_DLI_ASYNC9, "mm0b_disp_dli_as9",
			"mm_disp_ck"/* parent */, 19),
	GATE_MM0B0_V(MM0B_DISP_DLI_ASYNC9_DISP, "mm0b_disp_dli_as9_disp",
	        "mm0b_disp_dli_as9"/* parent */),
	GATE_MM0B0(MM0B_DISP_DLI_ASYNC10, "mm0b_disp_dli_as10",
			"mm_disp_ck"/* parent */, 20),
	GATE_MM0B0_V(MM0B_DISP_DLI_ASYNC10_DISP, "mm0b_disp_dli_as10_disp",
	        "mm0b_disp_dli_as10"/* parent */),
	GATE_MM0B0(MM0B_DISP_DLI_ASYNC11, "mm0b_disp_dli_as11",
			"mm_disp_ck"/* parent */, 21),
	GATE_MM0B0_V(MM0B_DISP_DLI_ASYNC11_DISP, "mm0b_disp_dli_as11_disp",
	        "mm0b_disp_dli_as11"/* parent */),
	GATE_MM0B0(MM0B_DISP_DLI_ASYNC12, "mm0b_disp_dli_as12",
			"mm_disp_ck"/* parent */, 22),
	GATE_MM0B0_V(MM0B_DISP_DLI_ASYNC12_DISP, "mm0b_disp_dli_as12_disp",
	        "mm0b_disp_dli_as12"/* parent */),
	GATE_MM0B0(MM0B_DISP_DLI_ASYNC13, "mm0b_disp_dli_as13",
			"mm_disp_ck"/* parent */, 23),
	GATE_MM0B0_V(MM0B_DISP_DLI_ASYNC13_DISP, "mm0b_disp_dli_as13_disp",
	        "mm0b_disp_dli_as13"/* parent */),
	GATE_MM0B0(MM0B_DISP_DLI_ASYNC14, "mm0b_disp_dli_as14",
			"mm_disp_ck"/* parent */, 24),
	GATE_MM0B0_V(MM0B_DISP_DLI_ASYNC14_DISP, "mm0b_disp_dli_as14_disp",
	        "mm0b_disp_dli_as14"/* parent */),
	GATE_MM0B0(MM0B_DISP_DLI_ASYNC15, "mm0b_disp_dli_as15",
			"mm_disp_ck"/* parent */, 25),
	GATE_MM0B0_V(MM0B_DISP_DLI_ASYNC15_DISP, "mm0b_disp_dli_as15_disp",
	        "mm0b_disp_dli_as15"/* parent */),
	GATE_MM0B0(MM0B_DISP_DLO_ASYNC0, "mm0b_disp_dlo_as0",
			"mm_disp_ck"/* parent */, 26),
	GATE_MM0B0_V(MM0B_DISP_DLO_ASYNC0_DISP, "mm0b_disp_dlo_as0_disp",
	        "mm0b_disp_dlo_as0"/* parent */),
	GATE_MM0B0(MM0B_DISP_DLO_ASYNC1, "mm0b_disp_dlo_as1",
			"mm_disp_ck"/* parent */, 27),
	GATE_MM0B0_V(MM0B_DISP_DLO_ASYNC1_DISP, "mm0b_disp_dlo_as1_disp",
	        "mm0b_disp_dlo_as1"/* parent */),
	GATE_MM0B0(MM0B_DISP_DLO_ASYNC2, "mm0b_disp_dlo_as2",
			"mm_disp_ck"/* parent */, 28),
	GATE_MM0B0_V(MM0B_DISP_DLO_ASYNC2_DISP, "mm0b_disp_dlo_as2_disp",
	        "mm0b_disp_dlo_as2"/* parent */),
	GATE_MM0B0(MM0B_DISP_DLO_ASYNC3, "mm0b_disp_dlo_as3",
			"mm_disp_ck"/* parent */, 29),
	GATE_MM0B0_V(MM0B_DISP_DLO_ASYNC3_DISP, "mm0b_disp_dlo_as3_disp",
	        "mm0b_disp_dlo_as3"/* parent */),
	GATE_MM0B0(MM0B_DISP_DLO_ASYNC4, "mm0b_disp_dlo_as4",
			"mm_disp_ck"/* parent */, 30),
	GATE_MM0B0_V(MM0B_DISP_DLO_ASYNC4_DISP, "mm0b_disp_dlo_as4_disp",
	        "mm0b_disp_dlo_as4"/* parent */),
	GATE_MM0B0(MM0B_DISP_DLO_ASYNC5, "mm0b_disp_dlo_as5",
			"mm_disp_ck"/* parent */, 31),
	GATE_MM0B0_V(MM0B_DISP_DLO_ASYNC5_DISP, "mm0b_disp_dlo_as5_disp",
	        "mm0b_disp_dlo_as5"/* parent */),
	/* MM0B1 */
	GATE_HWV_MM0B1(MM0B_DISP_DLO_ASYNC6, "mm0b_disp_dlo_as6",
			"mm_disp_ck"/* parent */, 0),
	GATE_MM0B1_V(MM0B_DISP_DLO_ASYNC6_DISP, "mm0b_disp_dlo_as6_disp",
	        "mm0b_disp_dlo_as6"/* parent */),
	GATE_HWV_MM0B1(MM0B_DISP_DLO_ASYNC7, "mm0b_disp_dlo_as7",
			"mm_disp_ck"/* parent */, 1),
	GATE_MM0B1_V(MM0B_DISP_DLO_ASYNC7_DISP, "mm0b_disp_dlo_as7_disp",
	        "mm0b_disp_dlo_as7"/* parent */),
	GATE_HWV_MM0B1(MM0B_DISP_DLO_ASYNC8, "mm0b_disp_dlo_as8",
			"mm_disp_ck"/* parent */, 2),
	GATE_MM0B1_V(MM0B_DISP_DLO_ASYNC8_DISP, "mm0b_disp_dlo_as8_disp",
	        "mm0b_disp_dlo_as8"/* parent */),
	GATE_HWV_MM0B1(MM0B_DISP_DLO_ASYNC9, "mm0b_disp_dlo_as9",
			"mm_disp_ck"/* parent */, 3),
	GATE_MM0B1_V(MM0B_DISP_DLO_ASYNC9_DISP, "mm0b_disp_dlo_as9_disp",
	        "mm0b_disp_dlo_as9"/* parent */),
	GATE_HWV_MM0B1(MM0B_DISP_DLO_ASYNC10, "mm0b_disp_dlo_as10",
			"mm_disp_ck"/* parent */, 4),
	GATE_MM0B1_V(MM0B_DISP_DLO_ASYNC10_DISP, "mm0b_disp_dlo_as10_disp",
	        "mm0b_disp_dlo_as10"/* parent */),
	GATE_HWV_MM0B1(MM0B_DISP_DLO_ASYNC11, "mm0b_disp_dlo_as11",
			"mm_disp_ck"/* parent */, 5),
	GATE_MM0B1_V(MM0B_DISP_DLO_ASYNC11_DISP, "mm0b_disp_dlo_as11_disp",
	        "mm0b_disp_dlo_as11"/* parent */),
	GATE_HWV_MM0B1(MM0B_DISP_DLO_ASYNC12, "mm0b_disp_dlo_as12",
			"mm_disp_ck"/* parent */, 6),
	GATE_MM0B1_V(MM0B_DISP_DLO_ASYNC12_DISP, "mm0b_disp_dlo_as12_disp",
	        "mm0b_disp_dlo_as12"/* parent */),
	GATE_HWV_MM0B1(MM0B_DISP_DLO_ASYNC13, "mm0b_disp_dlo_as13",
			"mm_disp_ck"/* parent */, 7),
	GATE_MM0B1_V(MM0B_DISP_DLO_ASYNC13_DISP, "mm0b_disp_dlo_as13_disp",
	        "mm0b_disp_dlo_as13"/* parent */),
	GATE_HWV_MM0B1(MM0B_DISP_DLO_ASYNC14, "mm0b_disp_dlo_as14",
			"mm_disp_ck"/* parent */, 8),
	GATE_MM0B1_V(MM0B_DISP_DLO_ASYNC14_DISP, "mm0b_disp_dlo_as14_disp",
	        "mm0b_disp_dlo_as14"/* parent */),
	GATE_HWV_MM0B1(MM0B_DISP_DLO_ASYNC15, "mm0b_disp_dlo_as15",
			"mm_disp_ck"/* parent */, 9),
	GATE_MM0B1_V(MM0B_DISP_DLO_ASYNC15_DISP, "mm0b_disp_dlo_as15_disp",
	        "mm0b_disp_dlo_as15"/* parent */),
	GATE_HWV_MM0B1(MM0B_DISP_DLO_ASYNC16, "mm0b_disp_dlo_as16",
			"mm_disp_ck"/* parent */, 10),
	GATE_MM0B1_V(MM0B_DISP_DLO_ASYNC16_DISP, "mm0b_disp_dlo_as16_disp",
	        "mm0b_disp_dlo_as16"/* parent */),
	GATE_HWV_MM0B1(MM0B_DISP_DLO_ASYNC17, "mm0b_disp_dlo_as17",
			"mm_disp_ck"/* parent */, 11),
	GATE_MM0B1_V(MM0B_DISP_DLO_ASYNC17_DISP, "mm0b_disp_dlo_as17_disp",
	        "mm0b_disp_dlo_as17"/* parent */),
	GATE_HWV_MM0B1(MM0B_DISP_DLO_ASYNC18, "mm0b_disp_dlo_as18",
			"mm_disp_ck"/* parent */, 12),
	GATE_MM0B1_V(MM0B_DISP_DLO_ASYNC18_DISP, "mm0b_disp_dlo_as18_disp",
	        "mm0b_disp_dlo_as18"/* parent */),
	GATE_HWV_MM0B1(MM0B_DISP_DLO_ASYNC19, "mm0b_disp_dlo_as19",
			"mm_disp_ck"/* parent */, 13),
	GATE_MM0B1_V(MM0B_DISP_DLO_ASYNC19_DISP, "mm0b_disp_dlo_as19_disp",
	        "mm0b_disp_dlo_as19"/* parent */),
	GATE_HWV_MM0B1(MM0B_DISP_DLO_ASYNC20, "mm0b_disp_dlo_as20",
			"mm_disp_ck"/* parent */, 14),
	GATE_MM0B1_V(MM0B_DISP_DLO_ASYNC20_DISP, "mm0b_disp_dlo_as20_disp",
	        "mm0b_disp_dlo_as20"/* parent */),
	GATE_HWV_MM0B1(MM0B_DISP_RELAY5, "mm0b_disp_relay5",
			"mm_disp_ck"/* parent */, 15),
	GATE_MM0B1_V(MM0B_DISP_RELAY5_DISP, "mm0b_disp_relay5_disp",
	        "mm0b_disp_relay5"/* parent */),
	GATE_HWV_MM0B1(MM0B_DISP_GAMMA0, "mm0b_disp_gamma0",
			"mm_disp_ck"/* parent */, 16),
	GATE_MM0B1_V(MM0B_DISP_GAMMA0_DISP, "mm0b_disp_gamma0_disp",
	        "mm0b_disp_gamma0"/* parent */),
	GATE_HWV_MM0B1(MM0B_MDP_AAL0, "mm0b_mdp_aal0",
			"mm_disp_ck"/* parent */, 17),
	GATE_MM0B1_V(MM0B_MDP_AAL0_DISP, "mm0b_mdp_aal0_disp",
	        "mm0b_mdp_aal0"/* parent */),
	GATE_HWV_MM0B1(MM0B_DISP_POSTMASK0, "mm0b_disp_postmask0",
			"mm_disp_ck"/* parent */, 18),
	GATE_MM0B1_V(MM0B_DISP_POSTMASK0_DISP, "mm0b_disp_postmask0_disp",
	        "mm0b_disp_postmask0"/* parent */),
	GATE_HWV_MM0B1(MM0B_MDP_RDMA0, "mm0b_mdp_rdma0",
			"mm_disp_ck"/* parent */, 19),
	GATE_MM0B1_V(MM0B_MDP_RDMA0_DISP, "mm0b_mdp_rdma0_disp",
	        "mm0b_mdp_rdma0"/* parent */),
	GATE_HWV_MM0B1(MM0B_DISP_SPR0, "mm0b_disp_spr0",
			"mm_disp_ck"/* parent */, 20),
	GATE_MM0B1_V(MM0B_DISP_SPR0_DISP, "mm0b_disp_spr0_disp",
	        "mm0b_disp_spr0"/* parent */),
	GATE_HWV_MM0B1(MM0B_DISP_ODDMR0, "mm0b_disp_oddmr0",
			"mm_disp_ck"/* parent */, 21),
	GATE_MM0B1_V(MM0B_DISP_ODDMR0_DISP, "mm0b_disp_oddmr0_disp",
	        "mm0b_disp_oddmr0"/* parent */),
	GATE_HWV_MM0B1(MM0B_MDP_RSZ0, "mm0b_mdp_rsz0",
			"mm_disp_ck"/* parent */, 22),
	GATE_MM0B1_V(MM0B_MDP_RSZ0_DISP, "mm0b_mdp_rsz0_disp",
	        "mm0b_mdp_rsz0"/* parent */),
	GATE_HWV_MM0B1(MM0B_DISP_TDSHP0, "mm0b_disp_tdshp0",
			"mm_disp_ck"/* parent */, 23),
	GATE_MM0B1_V(MM0B_DISP_TDSHP0_DISP, "mm0b_disp_tdshp0_disp",
	        "mm0b_disp_tdshp0"/* parent */),
	GATE_HWV_MM0B1(MM0B_SMI_SUB_COMM0, "mm0b_ssc",
			"mm_disp_ck"/* parent */, 24),
	GATE_MM0B1_V(MM0B_SMI_SUB_COMM0_DISP, "mm0b_ssc_disp",
	        "mm0b_ssc"/* parent */),
	GATE_MM0B1_V(MM0B_SMI_SUB_COMM0_SMI, "mm0b_ssc_smi",
	        "mm0b_ssc"/* parent */),
	GATE_HWV_MM0B1(MM0B_DISP_FAKE_ENG0, "mm0b_disp_fake_eng0",
			"mm_disp_ck"/* parent */, 25),
	GATE_MM0B1_V(MM0B_DISP_FAKE_ENG0_DISP, "mm0b_disp_fake_eng0_disp",
	        "mm0b_disp_fake_eng0"/* parent */),
	GATE_HWV_MM0B1(MM0B_DISP_DBG, "mm0b_disp_dbg",
			"mm_disp_ck"/* parent */, 26),
	GATE_MM0B1_V(MM0B_DISP_DBG_DISP, "mm0b_disp_dbg_disp",
	        "mm0b_disp_dbg"/* parent */),
};

static const struct mtk_clk_desc mm0b_mcd = {
	.clks = mm0b_clks,
	.num_clks = CLK_MM0B_NR_CLK,
};

static const struct mtk_gate_regs mm1b0_cg_regs = {
	.set_ofs = 0xA68,
	.clr_ofs = 0xA6C,
	.sta_ofs = 0xA64,
};

static const struct mtk_gate_regs mm1b1_cg_regs = {
	.set_ofs = 0xA74,
	.clr_ofs = 0xA78,
	.sta_ofs = 0xA70,
};

static const struct mtk_gate_regs mm1b1_hwv_regs = {
	.set_ofs = 0xa8,
	.clr_ofs = 0xac,
	.sta_ofs = 0x12638,
};

#define GATE_MM1B0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm1b0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
		.flags = RES_FRAMEWORK_VDISP,		\
	}

#define GATE_MM1B0_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_MM1B1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm1b1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_MM1B1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_HWV_MM1B1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &mm1b1_cg_regs,			\
		.hwv_regs = &mm1b1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VDISP | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate mm1b_clks[] = {
	/* MM1B0 */
	GATE_MM1B0(MM1B_DISP1_CFG, "mm1b_disp1_cfg",
			"mm_disp_ck"/* parent */, 0),
	GATE_MM1B0_V(MM1B_DISP1_CFG_DISP, "mm1b_disp1_cfg_disp",
	        "mm1b_disp1_cfg"/* parent */),
	GATE_MM1B0(MM1B_DISP1_S_CFG, "mm1b_disp1_s_cfg",
			"mm_disp_ck"/* parent */, 1),
	GATE_MM1B0_V(MM1B_DISP1_S_CFG_DISP, "mm1b_disp1_s_cfg_disp",
	        "mm1b_disp1_s_cfg"/* parent */),
	GATE_MM1B0(MM1B_DISP_MUTEX0, "mm1b_disp_mutex0",
			"mm_disp_ck"/* parent */, 2),
	GATE_MM1B0_V(MM1B_DISP_MUTEX0_DISP, "mm1b_disp_mutex0_disp",
	        "mm1b_disp_mutex0"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLI_ASYNC20, "mm1b_disp_dli_as20",
			"mm_disp_ck"/* parent */, 3),
	GATE_MM1B0_V(MM1B_DISP_DLI_ASYNC20_DISP, "mm1b_disp_dli_as20_disp",
	        "mm1b_disp_dli_as20"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLI_ASYNC21, "mm1b_disp_dli_as21",
			"mm_disp_ck"/* parent */, 4),
	GATE_MM1B0_V(MM1B_DISP_DLI_ASYNC21_DISP, "mm1b_disp_dli_as21_disp",
	        "mm1b_disp_dli_as21"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLI_ASYNC22, "mm1b_disp_dli_as22",
			"mm_disp_ck"/* parent */, 5),
	GATE_MM1B0_V(MM1B_DISP_DLI_ASYNC22_DISP, "mm1b_disp_dli_as22_disp",
	        "mm1b_disp_dli_as22"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLI_ASYNC23, "mm1b_disp_dli_as23",
			"mm_disp_ck"/* parent */, 6),
	GATE_MM1B0_V(MM1B_DISP_DLI_ASYNC23_DISP, "mm1b_disp_dli_as23_disp",
	        "mm1b_disp_dli_as23"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLI_ASYNC24, "mm1b_disp_dli_as24",
			"mm_disp_ck"/* parent */, 7),
	GATE_MM1B0_V(MM1B_DISP_DLI_ASYNC24_DISP, "mm1b_disp_dli_as24_disp",
	        "mm1b_disp_dli_as24"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLI_ASYNC25, "mm1b_disp_dli_as25",
			"mm_disp_ck"/* parent */, 8),
	GATE_MM1B0_V(MM1B_DISP_DLI_ASYNC25_DISP, "mm1b_disp_dli_as25_disp",
	        "mm1b_disp_dli_as25"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLI_ASYNC26, "mm1b_disp_dli_as26",
			"mm_disp_ck"/* parent */, 9),
	GATE_MM1B0_V(MM1B_DISP_DLI_ASYNC26_DISP, "mm1b_disp_dli_as26_disp",
	        "mm1b_disp_dli_as26"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLI_ASYNC27, "mm1b_disp_dli_as27",
			"mm_disp_ck"/* parent */, 10),
	GATE_MM1B0_V(MM1B_DISP_DLI_ASYNC27_DISP, "mm1b_disp_dli_as27_disp",
	        "mm1b_disp_dli_as27"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLI_ASYNC28, "mm1b_disp_dli_as28",
			"mm_disp_ck"/* parent */, 11),
	GATE_MM1B0_V(MM1B_DISP_DLI_ASYNC28_DISP, "mm1b_disp_dli_as28_disp",
	        "mm1b_disp_dli_as28"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLI_ASYNC29, "mm1b_disp_dli_as29",
			"mm_disp_ck"/* parent */, 12),
	GATE_MM1B0_V(MM1B_DISP_DLI_ASYNC29_DISP, "mm1b_disp_dli_as29_disp",
	        "mm1b_disp_dli_as29"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLI_ASYNC30, "mm1b_disp_dli_as30",
			"mm_disp_ck"/* parent */, 13),
	GATE_MM1B0_V(MM1B_DISP_DLI_ASYNC30_DISP, "mm1b_disp_dli_as30_disp",
	        "mm1b_disp_dli_as30"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLI_ASYNC31, "mm1b_disp_dli_as31",
			"mm_disp_ck"/* parent */, 14),
	GATE_MM1B0_V(MM1B_DISP_DLI_ASYNC31_DISP, "mm1b_disp_dli_as31_disp",
	        "mm1b_disp_dli_as31"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLI_ASYNC32, "mm1b_disp_dli_as32",
			"mm_disp_ck"/* parent */, 15),
	GATE_MM1B0_V(MM1B_DISP_DLI_ASYNC32_DISP, "mm1b_disp_dli_as32_disp",
	        "mm1b_disp_dli_as32"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLI_ASYNC33, "mm1b_disp_dli_as33",
			"mm_disp_ck"/* parent */, 16),
	GATE_MM1B0_V(MM1B_DISP_DLI_ASYNC33_DISP, "mm1b_disp_dli_as33_disp",
	        "mm1b_disp_dli_as33"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLI_ASYNC34, "mm1b_disp_dli_as34",
			"mm_disp_ck"/* parent */, 17),
	GATE_MM1B0_V(MM1B_DISP_DLI_ASYNC34_DISP, "mm1b_disp_dli_as34_disp",
	        "mm1b_disp_dli_as34"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLI_ASYNC35, "mm1b_disp_dli_as35",
			"mm_disp_ck"/* parent */, 18),
	GATE_MM1B0_V(MM1B_DISP_DLI_ASYNC35_DISP, "mm1b_disp_dli_as35_disp",
	        "mm1b_disp_dli_as35"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLI_ASYNC36, "mm1b_disp_dli_as36",
			"mm_disp_ck"/* parent */, 19),
	GATE_MM1B0_V(MM1B_DISP_DLI_ASYNC36_DISP, "mm1b_disp_dli_as36_disp",
	        "mm1b_disp_dli_as36"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLI_ASYNC37, "mm1b_disp_dli_as37",
			"mm_disp_ck"/* parent */, 20),
	GATE_MM1B0_V(MM1B_DISP_DLI_ASYNC37_DISP, "mm1b_disp_dli_as37_disp",
	        "mm1b_disp_dli_as37"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLI_ASYNC38, "mm1b_disp_dli_as38",
			"mm_disp_ck"/* parent */, 21),
	GATE_MM1B0_V(MM1B_DISP_DLI_ASYNC38_DISP, "mm1b_disp_dli_as38_disp",
	        "mm1b_disp_dli_as38"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLO_ASYNC31, "mm1b_disp_dlo_as31",
			"mm_disp_ck"/* parent */, 22),
	GATE_MM1B0_V(MM1B_DISP_DLO_ASYNC31_DISP, "mm1b_disp_dlo_as31_disp",
	        "mm1b_disp_dlo_as31"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLO_ASYNC32, "mm1b_disp_dlo_as32",
			"mm_disp_ck"/* parent */, 23),
	GATE_MM1B0_V(MM1B_DISP_DLO_ASYNC32_DISP, "mm1b_disp_dlo_as32_disp",
	        "mm1b_disp_dlo_as32"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLO_ASYNC33, "mm1b_disp_dlo_as33",
			"mm_disp_ck"/* parent */, 24),
	GATE_MM1B0_V(MM1B_DISP_DLO_ASYNC33_DISP, "mm1b_disp_dlo_as33_disp",
	        "mm1b_disp_dlo_as33"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLO_ASYNC34, "mm1b_disp_dlo_as34",
			"mm_disp_ck"/* parent */, 25),
	GATE_MM1B0_V(MM1B_DISP_DLO_ASYNC34_DISP, "mm1b_disp_dlo_as34_disp",
	        "mm1b_disp_dlo_as34"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLO_ASYNC35, "mm1b_disp_dlo_as35",
			"mm_disp_ck"/* parent */, 26),
	GATE_MM1B0_V(MM1B_DISP_DLO_ASYNC35_DISP, "mm1b_disp_dlo_as35_disp",
	        "mm1b_disp_dlo_as35"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLO_ASYNC36, "mm1b_disp_dlo_as36",
			"mm_disp_ck"/* parent */, 27),
	GATE_MM1B0_V(MM1B_DISP_DLO_ASYNC36_DISP, "mm1b_disp_dlo_as36_disp",
	        "mm1b_disp_dlo_as36"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLO_ASYNC37, "mm1b_disp_dlo_as37",
			"mm_disp_ck"/* parent */, 28),
	GATE_MM1B0_V(MM1B_DISP_DLO_ASYNC37_DISP, "mm1b_disp_dlo_as37_disp",
	        "mm1b_disp_dlo_as37"/* parent */),
	GATE_MM1B0(MM1B_DISP_DLO_ASYNC38, "mm1b_disp_dlo_as38",
			"mm_disp_ck"/* parent */, 29),
	GATE_MM1B0_V(MM1B_DISP_DLO_ASYNC38_DISP, "mm1b_disp_dlo_as38_disp",
	        "mm1b_disp_dlo_as38"/* parent */),
	GATE_MM1B0(MM1B_DISP_RELAY0, "mm1b_disp_relay0",
			"mm_disp_ck"/* parent */, 30),
	GATE_MM1B0_V(MM1B_DISP_RELAY0_DISP, "mm1b_disp_relay0_disp",
	        "mm1b_disp_relay0"/* parent */),
	GATE_MM1B0(MM1B_DISP_RELAY1, "mm1b_disp_relay1",
			"mm_disp_ck"/* parent */, 31),
	GATE_MM1B0_V(MM1B_DISP_RELAY1_DISP, "mm1b_disp_relay1_disp",
	        "mm1b_disp_relay1"/* parent */),
	/* MM1B1 */
	GATE_HWV_MM1B1(MM1B_DISP_RELAY2, "mm1b_disp_relay2",
			"mm_disp_ck"/* parent */, 0),
	GATE_MM1B1_V(MM1B_DISP_RELAY2_DISP, "mm1b_disp_relay2_disp",
	        "mm1b_disp_relay2"/* parent */),
	GATE_HWV_MM1B1(MM1B_DISP_RELAY3, "mm1b_disp_relay3",
			"mm_disp_ck"/* parent */, 1),
	GATE_MM1B1_V(MM1B_DISP_RELAY3_DISP, "mm1b_disp_relay3_disp",
	        "mm1b_disp_relay3"/* parent */),
	GATE_HWV_MM1B1(MM1B_DISP_RELAY4, "mm1b_disp_relay4",
			"mm_disp_ck"/* parent */, 2),
	GATE_MM1B1_V(MM1B_DISP_RELAY4_DISP, "mm1b_disp_relay4_disp",
	        "mm1b_disp_relay4"/* parent */),
	GATE_HWV_MM1B1(MM1B_DISP_DSI0, "mm1b_0",
			"mm_disp_ck"/* parent */, 3),
	GATE_MM1B1_V(MM1B_DISP_DSI0_DISP, "mm1b_0_disp",
	        "mm1b_0"/* parent */),
	GATE_HWV_MM1B1(MM1B_DISP_DSI1, "mm1b_1",
			"mm_disp_ck"/* parent */, 4),
	GATE_MM1B1_V(MM1B_DISP_DSI1_DISP, "mm1b_1_disp",
	        "mm1b_1"/* parent */),
	GATE_HWV_MM1B1(MM1B_DISP_DVO0, "mm1b_disp_dvo0",
			"mm_disp_ck"/* parent */, 5),
	GATE_MM1B1_V(MM1B_DISP_DVO0_DISP, "mm1b_disp_dvo0_disp",
	        "mm1b_disp_dvo0"/* parent */),
	GATE_HWV_MM1B1(MM1B_DISP_WDMA0, "mm1b_disp_wdma0",
			"mm_disp_ck"/* parent */, 6),
	GATE_MM1B1_V(MM1B_DISP_WDMA0_DISP, "mm1b_disp_wdma0_disp",
	        "mm1b_disp_wdma0"/* parent */),
	GATE_HWV_MM1B1(MM1B_DISP_WDMA1, "mm1b_disp_wdma1",
			"mm_disp_ck"/* parent */, 7),
	GATE_MM1B1_V(MM1B_DISP_WDMA1_DISP, "mm1b_disp_wdma1_disp",
	        "mm1b_disp_wdma1"/* parent */),
	GATE_HWV_MM1B1(MM1B_DISP_DBI_COUNT0, "mm1b_disp_dbi_count0",
			"mm_disp_ck"/* parent */, 8),
	GATE_MM1B1_V(MM1B_DISP_DBI_COUNT0_DISP, "mm1b_disp_dbi_count0_disp",
	        "mm1b_disp_dbi_count0"/* parent */),
	GATE_HWV_MM1B1(MM1B_DISP_CHIST0, "mm1b_disp_chist0",
			"mm_disp_ck"/* parent */, 9),
	GATE_MM1B1_V(MM1B_DISP_CHIST0_DISP, "mm1b_disp_chist0_disp",
	        "mm1b_disp_chist0"/* parent */),
	GATE_HWV_MM1B1(MM1B_DISP_CHIST1, "mm1b_disp_chist1",
			"mm_disp_ck"/* parent */, 10),
	GATE_MM1B1_V(MM1B_DISP_CHIST1_DISP, "mm1b_disp_chist1_disp",
	        "mm1b_disp_chist1"/* parent */),
	GATE_HWV_MM1B1(MM1B_DISP_CHIST2, "mm1b_disp_chist2",
			"mm_disp_ck"/* parent */, 11),
	GATE_MM1B1_V(MM1B_DISP_CHIST2_DISP, "mm1b_disp_chist2_disp",
	        "mm1b_disp_chist2"/* parent */),
	GATE_HWV_MM1B1(MM1B_DISP_POSTALIGN0, "mm1b_disp_postalign0",
			"mm_disp_ck"/* parent */, 12),
	GATE_MM1B1_V(MM1B_DISP_POSTALIGN0_DISP, "mm1b_disp_postalign0_disp",
	        "mm1b_disp_postalign0"/* parent */),
	GATE_HWV_MM1B1(MM1B_DISP_SPLITTER0, "mm1b_disp_splitter0",
			"mm_disp_ck"/* parent */, 13),
	GATE_MM1B1_V(MM1B_DISP_SPLITTER0_DISP, "mm1b_disp_splitter0_disp",
	        "mm1b_disp_splitter0"/* parent */),
	GATE_HWV_MM1B1(MM1B_DISP_SPLITTER1, "mm1b_disp_splitter1",
			"mm_disp_ck"/* parent */, 14),
	GATE_MM1B1_V(MM1B_DISP_SPLITTER1_DISP, "mm1b_disp_splitter1_disp",
	        "mm1b_disp_splitter1"/* parent */),
	GATE_HWV_MM1B1(MM1B_DISP_DSC_WRAP0, "mm1b_disp_dsc_wrap0",
			"mm_disp_ck"/* parent */, 15),
	GATE_MM1B1_V(MM1B_DISP_DSC_WRAP0_DISP, "mm1b_disp_dsc_wrap0_disp",
	        "mm1b_disp_dsc_wrap0"/* parent */),
	GATE_HWV_MM1B1(MM1B_DISP_DSC_WRAP1, "mm1b_disp_dsc_wrap1",
			"mm_disp_ck"/* parent */, 16),
	GATE_MM1B1_V(MM1B_DISP_DSC_WRAP1_DISP, "mm1b_disp_dsc_wrap1_disp",
	        "mm1b_disp_dsc_wrap1"/* parent */),
	GATE_HWV_MM1B1(MM1B_DISP_R2Y0, "mm1b_disp_r2y0",
			"mm_disp_ck"/* parent */, 17),
	GATE_MM1B1_V(MM1B_DISP_R2Y0_DISP, "mm1b_disp_r2y0_disp",
	        "mm1b_disp_r2y0"/* parent */),
	GATE_HWV_MM1B1(MM1B_DISP_GDMA0, "mm1b_disp_gdma0",
			"mm_disp_ck"/* parent */, 18),
	GATE_MM1B1_V(MM1B_DISP_GDMA0_DISP, "mm1b_disp_gdma0_disp",
	        "mm1b_disp_gdma0"/* parent */),
	GATE_HWV_MM1B1(MM1B_DISP_MERGE0, "mm1b_disp_merge0",
			"mm_disp_ck"/* parent */, 19),
	GATE_MM1B1_V(MM1B_DISP_MERGE0_DISP, "mm1b_disp_merge0_disp",
	        "mm1b_disp_merge0"/* parent */),
	GATE_HWV_MM1B1(MM1B_DISP_MERGE1, "mm1b_disp_merge1",
			"mm_disp_ck"/* parent */, 20),
	GATE_MM1B1_V(MM1B_DISP_MERGE1_DISP, "mm1b_disp_merge1_disp",
	        "mm1b_disp_merge1"/* parent */),
	GATE_HWV_MM1B1(MM1B_SMI_LARB0, "mm1b_smi_larb0",
			"mm_disp_ck"/* parent */, 21),
	GATE_MM1B1_V(MM1B_SMI_LARB0_DISP, "mm1b_smi_larb0_disp",
	        "mm1b_smi_larb0"/* parent */),
	GATE_MM1B1_V(MM1B_SMI_LARB0_SMI, "mm1b_smi_larb0_smi",
	        "mm1b_smi_larb0"/* parent */),
	GATE_HWV_MM1B1(MM1B_DISP_FAKE_ENG1, "mm1b_disp_fake_eng1",
			"mm_disp_ck"/* parent */, 22),
	GATE_MM1B1_V(MM1B_DISP_FAKE_ENG1_DISP, "mm1b_disp_fake_eng1_disp",
	        "mm1b_disp_fake_eng1"/* parent */),
	GATE_HWV_MM1B1(MM1B_DISP_DBG, "mm1b_disp_dbg",
			"mm_disp_ck"/* parent */, 23),
	GATE_MM1B1_V(MM1B_DISP_DBG_DISP, "mm1b_disp_dbg_disp",
	        "mm1b_disp_dbg"/* parent */),
	GATE_MM1B1(MM1B_MOD1, "mm1b_mod1",
			"cksys_vlp_f26m_ck"/* parent */, 24),
	GATE_MM1B1_V(MM1B_MOD1_V, "mm1b_mod1_v",
			"mm1b_mod1"/* parent */),
	GATE_MM1B1(MM1B_MOD2, "mm1b_mod2",
			"cksys_vlp_f26m_ck"/* parent */, 25),
	GATE_MM1B1_V(MM1B_MOD2_V, "mm1b_mod2_v",
			"mm1b_mod2"/* parent */),
	GATE_MM1B1(MM1B_MOD3, "mm1b_mod3",
			"mm_dvo_dp_ck"/* parent */, 26),
	GATE_MM1B1_V(MM1B_MOD3_V, "mm1b_mod3_v",
			"mm1b_mod3"/* parent */),
	GATE_MM1B1(MM1B_MOD4, "mm1b_mod4",
			"mm_dvo_favt_dp_ck"/* parent */, 27),
	GATE_MM1B1_V(MM1B_MOD4_V, "mm1b_mod4_v",
			"mm1b_mod4"/* parent */),
};

static const struct mtk_clk_desc mm1b_mcd = {
	.clks = mm1b_clks,
	.num_clks = CLK_MM1B_NR_CLK,
};

static const struct mtk_gate_regs mm10_cg_regs = {
	.set_ofs = 0xA68,
	.clr_ofs = 0xA6C,
	.sta_ofs = 0xA64,
};

static const struct mtk_gate_regs mm11_cg_regs = {
	.set_ofs = 0xA74,
	.clr_ofs = 0xA78,
	.sta_ofs = 0xA70,
};

static const struct mtk_gate_regs mm11_hwv_regs = {
	.set_ofs = 0x9c,
	.clr_ofs = 0xa0,
	.sta_ofs = 0x12634,
};

#define GATE_MM10(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
		.flags = RES_FRAMEWORK_VDISP,		\
	}

#define GATE_MM10_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_MM11(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm11_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_MM11_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_HWV_MM11(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &mm11_cg_regs,			\
		.hwv_regs = &mm11_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VDISP | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate mm1_clks[] = {
	/* MM10 */
	GATE_MM10(MM1_DISP1_CFG, "mm1_disp1_cfg",
			"mm_disp_ck"/* parent */, 0),
	GATE_MM10_V(MM1_DISP1_CFG_DISP, "mm1_disp1_cfg_disp",
	        "mm1_disp1_cfg"/* parent */),
	GATE_MM10(MM1_DISP1_S_CFG, "mm1_disp1_s_cfg",
			"mm_disp_ck"/* parent */, 1),
	GATE_MM10_V(MM1_DISP1_S_CFG_DISP, "mm1_disp1_s_cfg_disp",
	        "mm1_disp1_s_cfg"/* parent */),
	GATE_MM10(MM1_DISP_MUTEX0, "mm1_disp_mutex0",
			"mm_disp_ck"/* parent */, 2),
	GATE_MM10_V(MM1_DISP_MUTEX0_DISP, "mm1_disp_mutex0_disp",
	        "mm1_disp_mutex0"/* parent */),
	GATE_MM10(MM1_DISP_DLI_ASYNC20, "mm1_disp_dli_as20",
			"mm_disp_ck"/* parent */, 3),
	GATE_MM10_V(MM1_DISP_DLI_ASYNC20_DISP, "mm1_disp_dli_as20_disp",
	        "mm1_disp_dli_as20"/* parent */),
	GATE_MM10(MM1_DISP_DLI_ASYNC21, "mm1_disp_dli_as21",
			"mm_disp_ck"/* parent */, 4),
	GATE_MM10_V(MM1_DISP_DLI_ASYNC21_DISP, "mm1_disp_dli_as21_disp",
	        "mm1_disp_dli_as21"/* parent */),
	GATE_MM10(MM1_DISP_DLI_ASYNC22, "mm1_disp_dli_as22",
			"mm_disp_ck"/* parent */, 5),
	GATE_MM10_V(MM1_DISP_DLI_ASYNC22_DISP, "mm1_disp_dli_as22_disp",
	        "mm1_disp_dli_as22"/* parent */),
	GATE_MM10(MM1_DISP_DLI_ASYNC23, "mm1_disp_dli_as23",
			"mm_disp_ck"/* parent */, 6),
	GATE_MM10_V(MM1_DISP_DLI_ASYNC23_DISP, "mm1_disp_dli_as23_disp",
	        "mm1_disp_dli_as23"/* parent */),
	GATE_MM10(MM1_DISP_DLI_ASYNC24, "mm1_disp_dli_as24",
			"mm_disp_ck"/* parent */, 7),
	GATE_MM10_V(MM1_DISP_DLI_ASYNC24_DISP, "mm1_disp_dli_as24_disp",
	        "mm1_disp_dli_as24"/* parent */),
	GATE_MM10(MM1_DISP_DLI_ASYNC25, "mm1_disp_dli_as25",
			"mm_disp_ck"/* parent */, 8),
	GATE_MM10_V(MM1_DISP_DLI_ASYNC25_DISP, "mm1_disp_dli_as25_disp",
	        "mm1_disp_dli_as25"/* parent */),
	GATE_MM10(MM1_DISP_DLI_ASYNC26, "mm1_disp_dli_as26",
			"mm_disp_ck"/* parent */, 9),
	GATE_MM10_V(MM1_DISP_DLI_ASYNC26_DISP, "mm1_disp_dli_as26_disp",
	        "mm1_disp_dli_as26"/* parent */),
	GATE_MM10(MM1_DISP_DLI_ASYNC27, "mm1_disp_dli_as27",
			"mm_disp_ck"/* parent */, 10),
	GATE_MM10_V(MM1_DISP_DLI_ASYNC27_DISP, "mm1_disp_dli_as27_disp",
	        "mm1_disp_dli_as27"/* parent */),
	GATE_MM10(MM1_DISP_DLI_ASYNC28, "mm1_disp_dli_as28",
			"mm_disp_ck"/* parent */, 11),
	GATE_MM10_V(MM1_DISP_DLI_ASYNC28_DISP, "mm1_disp_dli_as28_disp",
	        "mm1_disp_dli_as28"/* parent */),
	GATE_MM10(MM1_DISP_DLI_ASYNC29, "mm1_disp_dli_as29",
			"mm_disp_ck"/* parent */, 12),
	GATE_MM10_V(MM1_DISP_DLI_ASYNC29_DISP, "mm1_disp_dli_as29_disp",
	        "mm1_disp_dli_as29"/* parent */),
	GATE_MM10(MM1_DISP_DLI_ASYNC30, "mm1_disp_dli_as30",
			"mm_disp_ck"/* parent */, 13),
	GATE_MM10_V(MM1_DISP_DLI_ASYNC30_DISP, "mm1_disp_dli_as30_disp",
	        "mm1_disp_dli_as30"/* parent */),
	GATE_MM10(MM1_DISP_DLI_ASYNC31, "mm1_disp_dli_as31",
			"mm_disp_ck"/* parent */, 14),
	GATE_MM10_V(MM1_DISP_DLI_ASYNC31_DISP, "mm1_disp_dli_as31_disp",
	        "mm1_disp_dli_as31"/* parent */),
	GATE_MM10(MM1_DISP_DLI_ASYNC32, "mm1_disp_dli_as32",
			"mm_disp_ck"/* parent */, 15),
	GATE_MM10_V(MM1_DISP_DLI_ASYNC32_DISP, "mm1_disp_dli_as32_disp",
	        "mm1_disp_dli_as32"/* parent */),
	GATE_MM10(MM1_DISP_DLI_ASYNC33, "mm1_disp_dli_as33",
			"mm_disp_ck"/* parent */, 16),
	GATE_MM10_V(MM1_DISP_DLI_ASYNC33_DISP, "mm1_disp_dli_as33_disp",
	        "mm1_disp_dli_as33"/* parent */),
	GATE_MM10(MM1_DISP_DLI_ASYNC34, "mm1_disp_dli_as34",
			"mm_disp_ck"/* parent */, 17),
	GATE_MM10_V(MM1_DISP_DLI_ASYNC34_DISP, "mm1_disp_dli_as34_disp",
	        "mm1_disp_dli_as34"/* parent */),
	GATE_MM10(MM1_DISP_DLI_ASYNC35, "mm1_disp_dli_as35",
			"mm_disp_ck"/* parent */, 18),
	GATE_MM10_V(MM1_DISP_DLI_ASYNC35_DISP, "mm1_disp_dli_as35_disp",
	        "mm1_disp_dli_as35"/* parent */),
	GATE_MM10(MM1_DISP_DLI_ASYNC36, "mm1_disp_dli_as36",
			"mm_disp_ck"/* parent */, 19),
	GATE_MM10_V(MM1_DISP_DLI_ASYNC36_DISP, "mm1_disp_dli_as36_disp",
	        "mm1_disp_dli_as36"/* parent */),
	GATE_MM10(MM1_DISP_DLI_ASYNC37, "mm1_disp_dli_as37",
			"mm_disp_ck"/* parent */, 20),
	GATE_MM10_V(MM1_DISP_DLI_ASYNC37_DISP, "mm1_disp_dli_as37_disp",
	        "mm1_disp_dli_as37"/* parent */),
	GATE_MM10(MM1_DISP_DLI_ASYNC38, "mm1_disp_dli_as38",
			"mm_disp_ck"/* parent */, 21),
	GATE_MM10_V(MM1_DISP_DLI_ASYNC38_DISP, "mm1_disp_dli_as38_disp",
	        "mm1_disp_dli_as38"/* parent */),
	GATE_MM10(MM1_DISP_DLO_ASYNC31, "mm1_disp_dlo_as31",
			"mm_disp_ck"/* parent */, 22),
	GATE_MM10_V(MM1_DISP_DLO_ASYNC31_DISP, "mm1_disp_dlo_as31_disp",
	        "mm1_disp_dlo_as31"/* parent */),
	GATE_MM10(MM1_DISP_DLO_ASYNC32, "mm1_disp_dlo_as32",
			"mm_disp_ck"/* parent */, 23),
	GATE_MM10_V(MM1_DISP_DLO_ASYNC32_DISP, "mm1_disp_dlo_as32_disp",
	        "mm1_disp_dlo_as32"/* parent */),
	GATE_MM10(MM1_DISP_DLO_ASYNC33, "mm1_disp_dlo_as33",
			"mm_disp_ck"/* parent */, 24),
	GATE_MM10_V(MM1_DISP_DLO_ASYNC33_DISP, "mm1_disp_dlo_as33_disp",
	        "mm1_disp_dlo_as33"/* parent */),
	GATE_MM10(MM1_DISP_DLO_ASYNC34, "mm1_disp_dlo_as34",
			"mm_disp_ck"/* parent */, 25),
	GATE_MM10_V(MM1_DISP_DLO_ASYNC34_DISP, "mm1_disp_dlo_as34_disp",
	        "mm1_disp_dlo_as34"/* parent */),
	GATE_MM10(MM1_DISP_DLO_ASYNC35, "mm1_disp_dlo_as35",
			"mm_disp_ck"/* parent */, 26),
	GATE_MM10_V(MM1_DISP_DLO_ASYNC35_DISP, "mm1_disp_dlo_as35_disp",
	        "mm1_disp_dlo_as35"/* parent */),
	GATE_MM10(MM1_DISP_DLO_ASYNC36, "mm1_disp_dlo_as36",
			"mm_disp_ck"/* parent */, 27),
	GATE_MM10_V(MM1_DISP_DLO_ASYNC36_DISP, "mm1_disp_dlo_as36_disp",
	        "mm1_disp_dlo_as36"/* parent */),
	GATE_MM10(MM1_DISP_DLO_ASYNC37, "mm1_disp_dlo_as37",
			"mm_disp_ck"/* parent */, 28),
	GATE_MM10_V(MM1_DISP_DLO_ASYNC37_DISP, "mm1_disp_dlo_as37_disp",
	        "mm1_disp_dlo_as37"/* parent */),
	GATE_MM10(MM1_DISP_DLO_ASYNC38, "mm1_disp_dlo_as38",
			"mm_disp_ck"/* parent */, 29),
	GATE_MM10_V(MM1_DISP_DLO_ASYNC38_DISP, "mm1_disp_dlo_as38_disp",
	        "mm1_disp_dlo_as38"/* parent */),
	GATE_MM10(MM1_DISP_RELAY0, "mm1_disp_relay0",
			"mm_disp_ck"/* parent */, 30),
	GATE_MM10_V(MM1_DISP_RELAY0_DISP, "mm1_disp_relay0_disp",
	        "mm1_disp_relay0"/* parent */),
	GATE_MM10(MM1_DISP_RELAY1, "mm1_disp_relay1",
			"mm_disp_ck"/* parent */, 31),
	GATE_MM10_V(MM1_DISP_RELAY1_DISP, "mm1_disp_relay1_disp",
	        "mm1_disp_relay1"/* parent */),
	/* MM11 */
	GATE_HWV_MM11(MM1_DISP_RELAY2, "mm1_disp_relay2",
			"mm_disp_ck"/* parent */, 0),
	GATE_MM11_V(MM1_DISP_RELAY2_DISP, "mm1_disp_relay2_disp",
	        "mm1_disp_relay2"/* parent */),
	GATE_HWV_MM11(MM1_DISP_RELAY3, "mm1_disp_relay3",
			"mm_disp_ck"/* parent */, 1),
	GATE_MM11_V(MM1_DISP_RELAY3_DISP, "mm1_disp_relay3_disp",
	        "mm1_disp_relay3"/* parent */),
	GATE_HWV_MM11(MM1_DISP_RELAY4, "mm1_disp_relay4",
			"mm_disp_ck"/* parent */, 2),
	GATE_MM11_V(MM1_DISP_RELAY4_DISP, "mm1_disp_relay4_disp",
	        "mm1_disp_relay4"/* parent */),
	GATE_HWV_MM11(MM1_DISP_DSI0, "mm1_0",
			"mm_disp_ck"/* parent */, 3),
	GATE_MM11_V(MM1_DISP_DSI0_DISP, "mm1_0_disp",
	        "mm1_0"/* parent */),
	GATE_HWV_MM11(MM1_DISP_DSI1, "mm1_1",
			"mm_disp_ck"/* parent */, 4),
	GATE_MM11_V(MM1_DISP_DSI1_DISP, "mm1_1_disp",
	        "mm1_1"/* parent */),
	GATE_HWV_MM11(MM1_DISP_DVO0, "mm1_disp_dvo0",
			"mm_disp_ck"/* parent */, 5),
	GATE_MM11_V(MM1_DISP_DVO0_DISP, "mm1_disp_dvo0_disp",
	        "mm1_disp_dvo0"/* parent */),
	GATE_HWV_MM11(MM1_DISP_WDMA0, "mm1_disp_wdma0",
			"mm_disp_ck"/* parent */, 6),
	GATE_MM11_V(MM1_DISP_WDMA0_DISP, "mm1_disp_wdma0_disp",
	        "mm1_disp_wdma0"/* parent */),
	GATE_HWV_MM11(MM1_DISP_WDMA1, "mm1_disp_wdma1",
			"mm_disp_ck"/* parent */, 7),
	GATE_MM11_V(MM1_DISP_WDMA1_DISP, "mm1_disp_wdma1_disp",
	        "mm1_disp_wdma1"/* parent */),
	GATE_HWV_MM11(MM1_DISP_DBI_COUNT0, "mm1_disp_dbi_count0",
			"mm_disp_ck"/* parent */, 8),
	GATE_MM11_V(MM1_DISP_DBI_COUNT0_DISP, "mm1_disp_dbi_count0_disp",
	        "mm1_disp_dbi_count0"/* parent */),
	GATE_HWV_MM11(MM1_DISP_CHIST0, "mm1_disp_chist0",
			"mm_disp_ck"/* parent */, 9),
	GATE_MM11_V(MM1_DISP_CHIST0_DISP, "mm1_disp_chist0_disp",
	        "mm1_disp_chist0"/* parent */),
	GATE_HWV_MM11(MM1_DISP_CHIST1, "mm1_disp_chist1",
			"mm_disp_ck"/* parent */, 10),
	GATE_MM11_V(MM1_DISP_CHIST1_DISP, "mm1_disp_chist1_disp",
	        "mm1_disp_chist1"/* parent */),
	GATE_HWV_MM11(MM1_DISP_CHIST2, "mm1_disp_chist2",
			"mm_disp_ck"/* parent */, 11),
	GATE_MM11_V(MM1_DISP_CHIST2_DISP, "mm1_disp_chist2_disp",
	        "mm1_disp_chist2"/* parent */),
	GATE_HWV_MM11(MM1_DISP_POSTALIGN0, "mm1_disp_postalign0",
			"mm_disp_ck"/* parent */, 12),
	GATE_MM11_V(MM1_DISP_POSTALIGN0_DISP, "mm1_disp_postalign0_disp",
	        "mm1_disp_postalign0"/* parent */),
	GATE_HWV_MM11(MM1_DISP_SPLITTER0, "mm1_disp_splitter0",
			"mm_disp_ck"/* parent */, 13),
	GATE_MM11_V(MM1_DISP_SPLITTER0_DISP, "mm1_disp_splitter0_disp",
	        "mm1_disp_splitter0"/* parent */),
	GATE_HWV_MM11(MM1_DISP_SPLITTER1, "mm1_disp_splitter1",
			"mm_disp_ck"/* parent */, 14),
	GATE_MM11_V(MM1_DISP_SPLITTER1_DISP, "mm1_disp_splitter1_disp",
	        "mm1_disp_splitter1"/* parent */),
	GATE_HWV_MM11(MM1_DISP_DSC_WRAP0, "mm1_disp_dsc_wrap0",
			"mm_disp_ck"/* parent */, 15),
	GATE_MM11_V(MM1_DISP_DSC_WRAP0_DISP, "mm1_disp_dsc_wrap0_disp",
	        "mm1_disp_dsc_wrap0"/* parent */),
	GATE_HWV_MM11(MM1_DISP_DSC_WRAP1, "mm1_disp_dsc_wrap1",
			"mm_disp_ck"/* parent */, 16),
	GATE_MM11_V(MM1_DISP_DSC_WRAP1_DISP, "mm1_disp_dsc_wrap1_disp",
	        "mm1_disp_dsc_wrap1"/* parent */),
	GATE_HWV_MM11(MM1_DISP_R2Y0, "mm1_disp_r2y0",
			"mm_disp_ck"/* parent */, 17),
	GATE_MM11_V(MM1_DISP_R2Y0_DISP, "mm1_disp_r2y0_disp",
	        "mm1_disp_r2y0"/* parent */),
	GATE_HWV_MM11(MM1_DISP_GDMA0, "mm1_disp_gdma0",
			"mm_disp_ck"/* parent */, 18),
	GATE_MM11_V(MM1_DISP_GDMA0_DISP, "mm1_disp_gdma0_disp",
	        "mm1_disp_gdma0"/* parent */),
	GATE_HWV_MM11(MM1_DISP_MERGE0, "mm1_disp_merge0",
			"mm_disp_ck"/* parent */, 19),
	GATE_MM11_V(MM1_DISP_MERGE0_DISP, "mm1_disp_merge0_disp",
	        "mm1_disp_merge0"/* parent */),
	GATE_HWV_MM11(MM1_DISP_MERGE1, "mm1_disp_merge1",
			"mm_disp_ck"/* parent */, 20),
	GATE_MM11_V(MM1_DISP_MERGE1_DISP, "mm1_disp_merge1_disp",
	        "mm1_disp_merge1"/* parent */),
	GATE_HWV_MM11(MM1_SMI_LARB0, "mm1_smi_larb0",
			"mm_disp_ck"/* parent */, 21),
	GATE_MM11_V(MM1_SMI_LARB0_DISP, "mm1_smi_larb0_disp",
	        "mm1_smi_larb0"/* parent */),
	GATE_MM11_V(MM1_SMI_LARB0_SMI, "mm1_smi_larb0_smi",
	        "mm1_smi_larb0"/* parent */),
	GATE_HWV_MM11(MM1_DISP_FAKE_ENG1, "mm1_disp_fake_eng1",
			"mm_disp_ck"/* parent */, 22),
	GATE_MM11_V(MM1_DISP_FAKE_ENG1_DISP, "mm1_disp_fake_eng1_disp",
	        "mm1_disp_fake_eng1"/* parent */),
	GATE_HWV_MM11(MM1_DISP_DBG, "mm1_disp_dbg",
			"mm_disp_ck"/* parent */, 23),
	GATE_MM11_V(MM1_DISP_DBG_DISP, "mm1_disp_dbg_disp",
	        "mm1_disp_dbg"/* parent */),
	GATE_MM11(MM1_MOD1, "mm1_mod1",
			"cksys_vlp_f26m_ck"/* parent */, 24),
	GATE_MM11_V(MM1_MOD1_V, "mm1_mod1_v",
			"mm1_mod1"/* parent */),
	GATE_MM11(MM1_MOD2, "mm1_mod2",
			"cksys_vlp_f26m_ck"/* parent */, 25),
	GATE_MM11_V(MM1_MOD2_V, "mm1_mod2_v",
			"mm1_mod2"/* parent */),
	GATE_MM11(MM1_MOD3, "mm1_mod3",
			"mm_dvo_dp_ck"/* parent */, 26),
	GATE_MM11_V(MM1_MOD3_V, "mm1_mod3_v",
			"mm1_mod3"/* parent */),
	GATE_MM11(MM1_MOD4, "mm1_mod4",
			"mm_dvo_favt_dp_ck"/* parent */, 27),
	GATE_MM11_V(MM1_MOD4_V, "mm1_mod4_v",
			"mm1_mod4"/* parent */),
};

static const struct mtk_clk_desc mm1_mcd = {
	.clks = mm1_clks,
	.num_clks = CLK_MM1_NR_CLK,
};

static const struct mtk_gate_regs mm00_cg_regs = {
	.set_ofs = 0xA64,
	.clr_ofs = 0xA68,
	.sta_ofs = 0xA60,
};

static const struct mtk_gate_regs mm01_cg_regs = {
	.set_ofs = 0xA70,
	.clr_ofs = 0xA74,
	.sta_ofs = 0xA6C,
};

static const struct mtk_gate_regs mm01_hwv_regs = {
	.set_ofs = 0x84,
	.clr_ofs = 0x88,
	.sta_ofs = 0x1262C,
};

#define GATE_MM00(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm00_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
		.flags = RES_FRAMEWORK_VDISP,		\
	}

#define GATE_MM00_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_MM01(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm01_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_MM01_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_HWV_MM01(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &mm01_cg_regs,			\
		.hwv_regs = &mm01_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VDISP | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate mm0_clks[] = {
	/* MM00 */
	GATE_MM00(MM0_CONFIG, "mm0_config",
			"mm_disp_ck"/* parent */, 0),
	GATE_MM00_V(MM0_CONFIG_DISP, "mm0_config_disp",
	        "mm0_config"/* parent */),
	GATE_MM00(MM0_DISP_MUTEX0, "mm0_disp_mutex0",
			"mm_disp_ck"/* parent */, 1),
	GATE_MM00_V(MM0_DISP_MUTEX0_DISP, "mm0_disp_mutex0_disp",
	        "mm0_disp_mutex0"/* parent */),
	GATE_MM00(MM0_DISP_AAL0, "mm0_disp_aal0",
			"mm_disp_ck"/* parent */, 2),
	GATE_MM00_V(MM0_DISP_AAL0_DISP, "mm0_disp_aal0_disp",
	        "mm0_disp_aal0"/* parent */),
	GATE_MM00(MM0_DISP_C3D0, "mm0_disp_c3d0",
			"mm_disp_ck"/* parent */, 3),
	GATE_MM00_V(MM0_DISP_C3D0_DISP, "mm0_disp_c3d0_disp",
	        "mm0_disp_c3d0"/* parent */),
	GATE_MM00(MM0_DISP_C3D1, "mm0_disp_c3d1",
			"mm_disp_ck"/* parent */, 4),
	GATE_MM00_V(MM0_DISP_C3D1_DISP, "mm0_disp_c3d1_disp",
	        "mm0_disp_c3d1"/* parent */),
	GATE_MM00(MM0_DISP_CCORR0, "mm0_disp_ccorr0",
			"mm_disp_ck"/* parent */, 5),
	GATE_MM00_V(MM0_DISP_CCORR0_DISP, "mm0_disp_ccorr0_disp",
	        "mm0_disp_ccorr0"/* parent */),
	GATE_MM00(MM0_DISP_CCORR1, "mm0_disp_ccorr1",
			"mm_disp_ck"/* parent */, 6),
	GATE_MM00_V(MM0_DISP_CCORR1_DISP, "mm0_disp_ccorr1_disp",
	        "mm0_disp_ccorr1"/* parent */),
	GATE_MM00(MM0_DISP_COLOR0, "mm0_disp_color0",
			"mm_disp_ck"/* parent */, 7),
	GATE_MM00_V(MM0_DISP_COLOR0_DISP, "mm0_disp_color0_disp",
	        "mm0_disp_color0"/* parent */),
	GATE_MM00(MM0_DISP_DITHER0, "mm0_disp_dither0",
			"mm_disp_ck"/* parent */, 8),
	GATE_MM00_V(MM0_DISP_DITHER0_DISP, "mm0_disp_dither0_disp",
	        "mm0_disp_dither0"/* parent */),
	GATE_MM00(MM0_DISP_DITHER1, "mm0_disp_dither1",
			"mm_disp_ck"/* parent */, 9),
	GATE_MM00_V(MM0_DISP_DITHER1_DISP, "mm0_disp_dither1_disp",
	        "mm0_disp_dither1"/* parent */),
	GATE_MM00(MM0_DISP_DLI_ASYNC0, "mm0_disp_dli_as0",
			"mm_disp_ck"/* parent */, 10),
	GATE_MM00_V(MM0_DISP_DLI_ASYNC0_DISP, "mm0_disp_dli_as0_disp",
	        "mm0_disp_dli_as0"/* parent */),
	GATE_MM00(MM0_DISP_DLI_ASYNC1, "mm0_disp_dli_as1",
			"mm_disp_ck"/* parent */, 11),
	GATE_MM00_V(MM0_DISP_DLI_ASYNC1_DISP, "mm0_disp_dli_as1_disp",
	        "mm0_disp_dli_as1"/* parent */),
	GATE_MM00(MM0_DISP_DLI_ASYNC2, "mm0_disp_dli_as2",
			"mm_disp_ck"/* parent */, 12),
	GATE_MM00_V(MM0_DISP_DLI_ASYNC2_DISP, "mm0_disp_dli_as2_disp",
	        "mm0_disp_dli_as2"/* parent */),
	GATE_MM00(MM0_DISP_DLI_ASYNC3, "mm0_disp_dli_as3",
			"mm_disp_ck"/* parent */, 13),
	GATE_MM00_V(MM0_DISP_DLI_ASYNC3_DISP, "mm0_disp_dli_as3_disp",
	        "mm0_disp_dli_as3"/* parent */),
	GATE_MM00(MM0_DISP_DLI_ASYNC4, "mm0_disp_dli_as4",
			"mm_disp_ck"/* parent */, 14),
	GATE_MM00_V(MM0_DISP_DLI_ASYNC4_DISP, "mm0_disp_dli_as4_disp",
	        "mm0_disp_dli_as4"/* parent */),
	GATE_MM00(MM0_DISP_DLI_ASYNC5, "mm0_disp_dli_as5",
			"mm_disp_ck"/* parent */, 15),
	GATE_MM00_V(MM0_DISP_DLI_ASYNC5_DISP, "mm0_disp_dli_as5_disp",
	        "mm0_disp_dli_as5"/* parent */),
	GATE_MM00(MM0_DISP_DLI_ASYNC6, "mm0_disp_dli_as6",
			"mm_disp_ck"/* parent */, 16),
	GATE_MM00_V(MM0_DISP_DLI_ASYNC6_DISP, "mm0_disp_dli_as6_disp",
	        "mm0_disp_dli_as6"/* parent */),
	GATE_MM00(MM0_DISP_DLI_ASYNC7, "mm0_disp_dli_as7",
			"mm_disp_ck"/* parent */, 17),
	GATE_MM00_V(MM0_DISP_DLI_ASYNC7_DISP, "mm0_disp_dli_as7_disp",
	        "mm0_disp_dli_as7"/* parent */),
	GATE_MM00(MM0_DISP_DLI_ASYNC8, "mm0_disp_dli_as8",
			"mm_disp_ck"/* parent */, 18),
	GATE_MM00_V(MM0_DISP_DLI_ASYNC8_DISP, "mm0_disp_dli_as8_disp",
	        "mm0_disp_dli_as8"/* parent */),
	GATE_MM00(MM0_DISP_DLI_ASYNC9, "mm0_disp_dli_as9",
			"mm_disp_ck"/* parent */, 19),
	GATE_MM00_V(MM0_DISP_DLI_ASYNC9_DISP, "mm0_disp_dli_as9_disp",
	        "mm0_disp_dli_as9"/* parent */),
	GATE_MM00(MM0_DISP_DLI_ASYNC10, "mm0_disp_dli_as10",
			"mm_disp_ck"/* parent */, 20),
	GATE_MM00_V(MM0_DISP_DLI_ASYNC10_DISP, "mm0_disp_dli_as10_disp",
	        "mm0_disp_dli_as10"/* parent */),
	GATE_MM00(MM0_DISP_DLI_ASYNC11, "mm0_disp_dli_as11",
			"mm_disp_ck"/* parent */, 21),
	GATE_MM00_V(MM0_DISP_DLI_ASYNC11_DISP, "mm0_disp_dli_as11_disp",
	        "mm0_disp_dli_as11"/* parent */),
	GATE_MM00(MM0_DISP_DLI_ASYNC12, "mm0_disp_dli_as12",
			"mm_disp_ck"/* parent */, 22),
	GATE_MM00_V(MM0_DISP_DLI_ASYNC12_DISP, "mm0_disp_dli_as12_disp",
	        "mm0_disp_dli_as12"/* parent */),
	GATE_MM00(MM0_DISP_DLI_ASYNC13, "mm0_disp_dli_as13",
			"mm_disp_ck"/* parent */, 23),
	GATE_MM00_V(MM0_DISP_DLI_ASYNC13_DISP, "mm0_disp_dli_as13_disp",
	        "mm0_disp_dli_as13"/* parent */),
	GATE_MM00(MM0_DISP_DLI_ASYNC14, "mm0_disp_dli_as14",
			"mm_disp_ck"/* parent */, 24),
	GATE_MM00_V(MM0_DISP_DLI_ASYNC14_DISP, "mm0_disp_dli_as14_disp",
	        "mm0_disp_dli_as14"/* parent */),
	GATE_MM00(MM0_DISP_DLI_ASYNC15, "mm0_disp_dli_as15",
			"mm_disp_ck"/* parent */, 25),
	GATE_MM00_V(MM0_DISP_DLI_ASYNC15_DISP, "mm0_disp_dli_as15_disp",
	        "mm0_disp_dli_as15"/* parent */),
	GATE_MM00(MM0_DISP_DLO_ASYNC0, "mm0_disp_dlo_as0",
			"mm_disp_ck"/* parent */, 26),
	GATE_MM00_V(MM0_DISP_DLO_ASYNC0_DISP, "mm0_disp_dlo_as0_disp",
	        "mm0_disp_dlo_as0"/* parent */),
	GATE_MM00(MM0_DISP_DLO_ASYNC1, "mm0_disp_dlo_as1",
			"mm_disp_ck"/* parent */, 27),
	GATE_MM00_V(MM0_DISP_DLO_ASYNC1_DISP, "mm0_disp_dlo_as1_disp",
	        "mm0_disp_dlo_as1"/* parent */),
	GATE_MM00(MM0_DISP_DLO_ASYNC2, "mm0_disp_dlo_as2",
			"mm_disp_ck"/* parent */, 28),
	GATE_MM00_V(MM0_DISP_DLO_ASYNC2_DISP, "mm0_disp_dlo_as2_disp",
	        "mm0_disp_dlo_as2"/* parent */),
	GATE_MM00(MM0_DISP_DLO_ASYNC3, "mm0_disp_dlo_as3",
			"mm_disp_ck"/* parent */, 29),
	GATE_MM00_V(MM0_DISP_DLO_ASYNC3_DISP, "mm0_disp_dlo_as3_disp",
	        "mm0_disp_dlo_as3"/* parent */),
	GATE_MM00(MM0_DISP_DLO_ASYNC4, "mm0_disp_dlo_as4",
			"mm_disp_ck"/* parent */, 30),
	GATE_MM00_V(MM0_DISP_DLO_ASYNC4_DISP, "mm0_disp_dlo_as4_disp",
	        "mm0_disp_dlo_as4"/* parent */),
	GATE_MM00(MM0_DISP_DLO_ASYNC5, "mm0_disp_dlo_as5",
			"mm_disp_ck"/* parent */, 31),
	GATE_MM00_V(MM0_DISP_DLO_ASYNC5_DISP, "mm0_disp_dlo_as5_disp",
	        "mm0_disp_dlo_as5"/* parent */),
	/* MM01 */
	GATE_HWV_MM01(MM0_DISP_DLO_ASYNC6, "mm0_disp_dlo_as6",
			"mm_disp_ck"/* parent */, 0),
	GATE_MM01_V(MM0_DISP_DLO_ASYNC6_DISP, "mm0_disp_dlo_as6_disp",
	        "mm0_disp_dlo_as6"/* parent */),
	GATE_HWV_MM01(MM0_DISP_DLO_ASYNC7, "mm0_disp_dlo_as7",
			"mm_disp_ck"/* parent */, 1),
	GATE_MM01_V(MM0_DISP_DLO_ASYNC7_DISP, "mm0_disp_dlo_as7_disp",
	        "mm0_disp_dlo_as7"/* parent */),
	GATE_HWV_MM01(MM0_DISP_DLO_ASYNC8, "mm0_disp_dlo_as8",
			"mm_disp_ck"/* parent */, 2),
	GATE_MM01_V(MM0_DISP_DLO_ASYNC8_DISP, "mm0_disp_dlo_as8_disp",
	        "mm0_disp_dlo_as8"/* parent */),
	GATE_HWV_MM01(MM0_DISP_DLO_ASYNC9, "mm0_disp_dlo_as9",
			"mm_disp_ck"/* parent */, 3),
	GATE_MM01_V(MM0_DISP_DLO_ASYNC9_DISP, "mm0_disp_dlo_as9_disp",
	        "mm0_disp_dlo_as9"/* parent */),
	GATE_HWV_MM01(MM0_DISP_DLO_ASYNC10, "mm0_disp_dlo_as10",
			"mm_disp_ck"/* parent */, 4),
	GATE_MM01_V(MM0_DISP_DLO_ASYNC10_DISP, "mm0_disp_dlo_as10_disp",
	        "mm0_disp_dlo_as10"/* parent */),
	GATE_HWV_MM01(MM0_DISP_DLO_ASYNC11, "mm0_disp_dlo_as11",
			"mm_disp_ck"/* parent */, 5),
	GATE_MM01_V(MM0_DISP_DLO_ASYNC11_DISP, "mm0_disp_dlo_as11_disp",
	        "mm0_disp_dlo_as11"/* parent */),
	GATE_HWV_MM01(MM0_DISP_DLO_ASYNC12, "mm0_disp_dlo_as12",
			"mm_disp_ck"/* parent */, 6),
	GATE_MM01_V(MM0_DISP_DLO_ASYNC12_DISP, "mm0_disp_dlo_as12_disp",
	        "mm0_disp_dlo_as12"/* parent */),
	GATE_HWV_MM01(MM0_DISP_DLO_ASYNC13, "mm0_disp_dlo_as13",
			"mm_disp_ck"/* parent */, 7),
	GATE_MM01_V(MM0_DISP_DLO_ASYNC13_DISP, "mm0_disp_dlo_as13_disp",
	        "mm0_disp_dlo_as13"/* parent */),
	GATE_HWV_MM01(MM0_DISP_DLO_ASYNC14, "mm0_disp_dlo_as14",
			"mm_disp_ck"/* parent */, 8),
	GATE_MM01_V(MM0_DISP_DLO_ASYNC14_DISP, "mm0_disp_dlo_as14_disp",
	        "mm0_disp_dlo_as14"/* parent */),
	GATE_HWV_MM01(MM0_DISP_DLO_ASYNC15, "mm0_disp_dlo_as15",
			"mm_disp_ck"/* parent */, 9),
	GATE_MM01_V(MM0_DISP_DLO_ASYNC15_DISP, "mm0_disp_dlo_as15_disp",
	        "mm0_disp_dlo_as15"/* parent */),
	GATE_HWV_MM01(MM0_DISP_DLO_ASYNC16, "mm0_disp_dlo_as16",
			"mm_disp_ck"/* parent */, 10),
	GATE_MM01_V(MM0_DISP_DLO_ASYNC16_DISP, "mm0_disp_dlo_as16_disp",
	        "mm0_disp_dlo_as16"/* parent */),
	GATE_HWV_MM01(MM0_DISP_DLO_ASYNC17, "mm0_disp_dlo_as17",
			"mm_disp_ck"/* parent */, 11),
	GATE_MM01_V(MM0_DISP_DLO_ASYNC17_DISP, "mm0_disp_dlo_as17_disp",
	        "mm0_disp_dlo_as17"/* parent */),
	GATE_HWV_MM01(MM0_DISP_DLO_ASYNC18, "mm0_disp_dlo_as18",
			"mm_disp_ck"/* parent */, 12),
	GATE_MM01_V(MM0_DISP_DLO_ASYNC18_DISP, "mm0_disp_dlo_as18_disp",
	        "mm0_disp_dlo_as18"/* parent */),
	GATE_HWV_MM01(MM0_DISP_DLO_ASYNC19, "mm0_disp_dlo_as19",
			"mm_disp_ck"/* parent */, 13),
	GATE_MM01_V(MM0_DISP_DLO_ASYNC19_DISP, "mm0_disp_dlo_as19_disp",
	        "mm0_disp_dlo_as19"/* parent */),
	GATE_HWV_MM01(MM0_DISP_DLO_ASYNC20, "mm0_disp_dlo_as20",
			"mm_disp_ck"/* parent */, 14),
	GATE_MM01_V(MM0_DISP_DLO_ASYNC20_DISP, "mm0_disp_dlo_as20_disp",
	        "mm0_disp_dlo_as20"/* parent */),
	GATE_HWV_MM01(MM0_DISP_RELAY5, "mm0_disp_relay5",
			"mm_disp_ck"/* parent */, 15),
	GATE_MM01_V(MM0_DISP_RELAY5_DISP, "mm0_disp_relay5_disp",
	        "mm0_disp_relay5"/* parent */),
	GATE_HWV_MM01(MM0_DISP_GAMMA0, "mm0_disp_gamma0",
			"mm_disp_ck"/* parent */, 16),
	GATE_MM01_V(MM0_DISP_GAMMA0_DISP, "mm0_disp_gamma0_disp",
	        "mm0_disp_gamma0"/* parent */),
	GATE_HWV_MM01(MM0_MDP_AAL0, "mm0_mdp_aal0",
			"mm_disp_ck"/* parent */, 17),
	GATE_MM01_V(MM0_MDP_AAL0_DISP, "mm0_mdp_aal0_disp",
	        "mm0_mdp_aal0"/* parent */),
	GATE_HWV_MM01(MM0_DISP_POSTMASK0, "mm0_disp_postmask0",
			"mm_disp_ck"/* parent */, 18),
	GATE_MM01_V(MM0_DISP_POSTMASK0_DISP, "mm0_disp_postmask0_disp",
	        "mm0_disp_postmask0"/* parent */),
	GATE_HWV_MM01(MM0_MDP_RDMA0, "mm0_mdp_rdma0",
			"mm_disp_ck"/* parent */, 19),
	GATE_MM01_V(MM0_MDP_RDMA0_DISP, "mm0_mdp_rdma0_disp",
	        "mm0_mdp_rdma0"/* parent */),
	GATE_HWV_MM01(MM0_DISP_SPR0, "mm0_disp_spr0",
			"mm_disp_ck"/* parent */, 20),
	GATE_MM01_V(MM0_DISP_SPR0_DISP, "mm0_disp_spr0_disp",
	        "mm0_disp_spr0"/* parent */),
	GATE_HWV_MM01(MM0_DISP_ODDMR0, "mm0_disp_oddmr0",
			"mm_disp_ck"/* parent */, 21),
	GATE_MM01_V(MM0_DISP_ODDMR0_DISP, "mm0_disp_oddmr0_disp",
	        "mm0_disp_oddmr0"/* parent */),
	GATE_HWV_MM01(MM0_MDP_RSZ0, "mm0_mdp_rsz0",
			"mm_disp_ck"/* parent */, 22),
	GATE_MM01_V(MM0_MDP_RSZ0_DISP, "mm0_mdp_rsz0_disp",
	        "mm0_mdp_rsz0"/* parent */),
	GATE_HWV_MM01(MM0_DISP_TDSHP0, "mm0_disp_tdshp0",
			"mm_disp_ck"/* parent */, 23),
	GATE_MM01_V(MM0_DISP_TDSHP0_DISP, "mm0_disp_tdshp0_disp",
	        "mm0_disp_tdshp0"/* parent */),
	GATE_HWV_MM01(MM0_SMI_SUB_COMM0, "mm0_ssc",
			"mm_disp_ck"/* parent */, 24),
	GATE_MM01_V(MM0_SMI_SUB_COMM0_DISP, "mm0_ssc_disp",
	        "mm0_ssc"/* parent */),
	GATE_MM01_V(MM0_SMI_SUB_COMM0_SMI, "mm0_ssc_smi",
	        "mm0_ssc"/* parent */),
	GATE_HWV_MM01(MM0_DISP_FAKE_ENG0, "mm0_disp_fake_eng0",
			"mm_disp_ck"/* parent */, 25),
	GATE_MM01_V(MM0_DISP_FAKE_ENG0_DISP, "mm0_disp_fake_eng0_disp",
	        "mm0_disp_fake_eng0"/* parent */),
	GATE_HWV_MM01(MM0_DISP_DBG, "mm0_disp_dbg",
			"mm_disp_ck"/* parent */, 26),
	GATE_MM01_V(MM0_DISP_DBG_DISP, "mm0_disp_dbg_disp",
	        "mm0_disp_dbg"/* parent */),
};

static const struct mtk_clk_desc mm0_mcd = {
	.clks = mm0_clks,
	.num_clks = CLK_MM0_NR_CLK,
};

static const struct mtk_gate_regs vdisp_ao_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs vdisp_ao0_hwv_regs = {
	.set_ofs = 0x78,
	.clr_ofs = 0x7c,
	.sta_ofs = 0x12628,
};

static const struct mtk_gate_regs vdisp_ao1_hwv_regs = {
	.set_ofs = 0xfc,
	.clr_ofs = 0x100,
	.sta_ofs = 0x12654,
};

#define GATE_VDISP_AO(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vdisp_ao_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_VDISP_AO_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_HWV_VDISP_AO0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &vdisp_ao_cg_regs,			\
		.hwv_regs = &vdisp_ao0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VDISP | CLK_USE_HW_VOTER,	\
	}

#define GATE_HWV_VDISP_AO1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &vdisp_ao_cg_regs,			\
		.hwv_regs = &vdisp_ao1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VDISP | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate vdisp_ao_clks[] = {
	GATE_HWV_VDISP_AO1(VDISP_AO_CONFIG, "vdisp_ao_config",
			"mm_disp_ck"/* parent */, 0),
	GATE_VDISP_AO_V(VDISP_AO_CONFIG_DISP, "vdisp_ao_config_disp",
	        "vdisp_ao_config"/* parent */),
	GATE_HWV_VDISP_AO1(VDISP_AO_DISP_PWM0, "vdisp_ao_disp_pwm0",
			"cksys_vlp_disp_pwm_ck"/* parent */, 1),
	GATE_VDISP_AO_V(VDISP_AO_DISP_PWM0_DISP, "vdisp_ao_disp_pwm0_disp",
	        "vdisp_ao_disp_pwm0"/* parent */),
	GATE_HWV_VDISP_AO1(VDISP_AO_DISP_PWM1, "vdisp_ao_disp_pwm1",
			"cksys_vlp_disp_pwm_ck"/* parent */, 2),
	GATE_VDISP_AO_V(VDISP_AO_DISP_PWM1_DISP, "vdisp_ao_disp_pwm1_disp",
	        "vdisp_ao_disp_pwm1"/* parent */),
	GATE_HWV_VDISP_AO0(VDISP_AO_DISP_DPC, "vdisp_ao_disp_dpc",
			"cksys_vlp_f26m_ck"/* parent */, 11),
	GATE_VDISP_AO_V(VDISP_AO_DISP_DPC_DISP, "vdisp_ao_disp_dpc_disp",
	        "vdisp_ao_disp_dpc"/* parent */),
	GATE_HWV_VDISP_AO1(VDISP_AO_DISP_BUS, "vdisp_ao_disp_bus",
			"mm_disp_ck"/* parent */, 23),
	GATE_VDISP_AO_V(VDISP_AO_DISP_BUS_DISP, "vdisp_ao_disp_bus_disp",
	        "vdisp_ao_disp_bus"/* parent */),
	GATE_VDISP_AO_V(VDISP_AO_DISP_BUS_SMI, "vdisp_ao_disp_bus_smi",
	        "vdisp_ao_disp_bus"/* parent */),
	GATE_HWV_VDISP_AO1(VDISP_AO_DISP_BWM0, "vdisp_ao_disp_bwm0",
			"mm_disp_ck"/* parent */, 24),
	GATE_VDISP_AO_V(VDISP_AO_DISP_BWM0_DISP, "vdisp_ao_disp_bwm0_disp",
	        "vdisp_ao_disp_bwm0"/* parent */),
	GATE_HWV_VDISP_AO1(VDISP_AO_DISP_BWM1, "vdisp_ao_disp_bwm1",
			"mm_disp_ck"/* parent */, 25),
	GATE_VDISP_AO_V(VDISP_AO_DISP_BWM1_DISP, "vdisp_ao_disp_bwm1_disp",
	        "vdisp_ao_disp_bwm1"/* parent */),
	GATE_HWV_VDISP_AO1(VDISP_AO_DISP_BWR, "vdisp_ao_disp_bwr",
			"mm_disp_ck"/* parent */, 26),
	GATE_VDISP_AO_V(VDISP_AO_DISP_BWR_DISP, "vdisp_ao_disp_bwr_disp",
	        "vdisp_ao_disp_bwr"/* parent */),
	GATE_HWV_VDISP_AO1(VDISP_AO_DISP_DEBUG_TOP, "vdisp_ao_disp_debug_top",
			"mm_disp_ck"/* parent */, 27),
	GATE_VDISP_AO_V(VDISP_AO_DISP_DEBUG_TOP_DISP, "vdisp_ao_disp_debug_top_disp",
	        "vdisp_ao_disp_debug_top"/* parent */),
	GATE_HWV_VDISP_AO1(VDISP_AO_DISP_CVFS, "vdisp_ao_disp_cvfs",
			"cksys_vlp_f26m_ck"/* parent */, 31),
	GATE_VDISP_AO_V(VDISP_AO_DISP_CVFS_DISP, "vdisp_ao_disp_cvfs_disp",
	        "vdisp_ao_disp_cvfs"/* parent */),
};

static const struct mtk_clk_desc vdisp_ao_mcd = {
	.clks = vdisp_ao_clks,
	.num_clks = CLK_VDISP_AO_NR_CLK,
};

static const struct mtk_gate_regs mml10_cg_regs = {
	.set_ofs = 0xA7C,
	.clr_ofs = 0xA80,
	.sta_ofs = 0xA78,
};

static const struct mtk_gate_regs mml10_hwv_regs = {
	.set_ofs = 0xf0,
	.clr_ofs = 0xf4,
	.sta_ofs = 0x12650,
};

static const struct mtk_gate_regs mml11_cg_regs = {
	.set_ofs = 0xA88,
	.clr_ofs = 0xA8C,
	.sta_ofs = 0xA84,
};

#define GATE_MML10(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mml10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
		.flags = RES_FRAMEWORK_VDISP,		\
	}

#define GATE_MML10_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_HWV_MML10(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &mml10_cg_regs,			\
		.hwv_regs = &mml10_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VDISP | CLK_USE_HW_VOTER,	\
	}

#define GATE_MML11(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mml11_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MML11_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

static const struct mtk_gate mml1_clks[] = {
	/* MML10 */
	GATE_HWV_MML10(MML1_MDP_MUTEX0, "mml1_mdp_mutex0",
			"mm_mml_ck"/* parent */, 0),
	GATE_MML10_V(MML1_MDP_MUTEX0_MML, "mml1_mdp_mutex0_mml",
	        "mml1_mdp_mutex0"/* parent */),
	GATE_HWV_MML10(MML1_SMI0, "mml1_smi0",
			"mm_mml_ck"/* parent */, 1),
	GATE_MML10_V(MML1_SMI0_SMI, "mml1_smi0_smi",
	        "mml1_smi0"/* parent */),
	GATE_HWV_MML10(MML1_APB_BUS, "mml1_apb_bus",
			"mm_mml_ck"/* parent */, 2),
	GATE_MML10_V(MML1_APB_BUS_MML, "mml1_apb_bus_mml",
	        "mml1_apb_bus"/* parent */),
	GATE_HWV_MML10(MML1_MDP_RDMA2, "mml1_mdp_rdma2",
			"mm_mml_ck"/* parent */, 4),
	GATE_MML10_V(MML1_MDP_RDMA2_MML, "mml1_mdp_rdma2_mml",
	        "mml1_mdp_rdma2"/* parent */),
	GATE_HWV_MML10(MML1_MDP_BIRSZ0, "mml1_mdp_birsz0",
			"mm_mml_ck"/* parent */, 5),
	GATE_MML10_V(MML1_MDP_BIRSZ0_MML, "mml1_mdp_birsz0_mml",
	        "mml1_mdp_birsz0"/* parent */),
	GATE_HWV_MML10(MML1_MDP_HDR0, "mml1_mdp_hdr0",
			"mm_mml_ck"/* parent */, 6),
	GATE_MML10_V(MML1_MDP_HDR0_MML, "mml1_mdp_hdr0_mml",
	        "mml1_mdp_hdr0"/* parent */),
	GATE_HWV_MML10(MML1_MDP_AAL0, "mml1_mdp_aal0",
			"mm_mml_ck"/* parent */, 7),
	GATE_MML10_V(MML1_MDP_AAL0_MML, "mml1_mdp_aal0_mml",
	        "mml1_mdp_aal0"/* parent */),
	GATE_HWV_MML10(MML1_MDP_TDSHP0, "mml1_mdp_tdshp0",
			"mm_mml_ck"/* parent */, 10),
	GATE_MML10_V(MML1_MDP_TDSHP0_MML, "mml1_mdp_tdshp0_mml",
	        "mml1_mdp_tdshp0"/* parent */),
	GATE_HWV_MML10(MML1_MDP_COLOR0, "mml1_mdp_color0",
			"mm_mml_ck"/* parent */, 11),
	GATE_MML10_V(MML1_MDP_COLOR0_MML, "mml1_mdp_color0_mml",
	        "mml1_mdp_color0"/* parent */),
	GATE_HWV_MML10(MML1_MDP_WROT2, "mml1_mdp_wrot2",
			"mm_mml_ck"/* parent */, 13),
	GATE_MML10_V(MML1_MDP_WROT2_MML, "mml1_mdp_wrot2_mml",
	        "mml1_mdp_wrot2"/* parent */),
	GATE_HWV_MML10(MML1_MDP_FAKE_ENG0, "mml1_mdp_fake_eng0",
			"mm_mml_ck"/* parent */, 14),
	GATE_MML10_V(MML1_MDP_FAKE_ENG0_MML, "mml1_mdp_fake_eng0_mml",
	        "mml1_mdp_fake_eng0"/* parent */),
	GATE_HWV_MML10(MML1_MDP_FAKE_ENG1, "mml1_mdp_fake_eng1",
			"mm_mml_ck"/* parent */, 15),
	GATE_MML10_V(MML1_MDP_FAKE_ENG1_MML, "mml1_mdp_fake_eng1_mml",
	        "mml1_mdp_fake_eng1"/* parent */),
	GATE_HWV_MML10(MML1_APB_DB, "mml1_apb_db",
			"mm_mml_ck"/* parent */, 16),
	GATE_MML10_V(MML1_APB_DB_MML, "mml1_apb_db_mml",
	        "mml1_apb_db"/* parent */),
	GATE_HWV_MML10(MML1_MDP_DLI_ASYNC0, "mml1_mdp_dli_as0",
			"mm_mml_ck"/* parent */, 17),
	GATE_MML10_V(MML1_MDP_DLI_ASYNC0_MML, "mml1_mdp_dli_as0_mml",
	        "mml1_mdp_dli_as0"/* parent */),
	GATE_HWV_MML10(MML1_MDP_DLO_ASYNC0, "mml1_mdp_dlo_as0",
			"mm_mml_ck"/* parent */, 19),
	GATE_MML10_V(MML1_MDP_DLO_ASYNC0_MML, "mml1_mdp_dlo_as0_mml",
	        "mml1_mdp_dlo_as0"/* parent */),
	GATE_HWV_MML10(MML1_MDP_C3D0, "mml1_mdp_c3d0",
			"mm_mml_ck"/* parent */, 26),
	GATE_MML10_V(MML1_MDP_C3D0_MML, "mml1_mdp_c3d0_mml",
	        "mml1_mdp_c3d0"/* parent */),
	GATE_HWV_MML10(MML1_MDP_FG0, "mml1_mdp_fg0",
			"mm_mml_ck"/* parent */, 27),
	GATE_MML10_V(MML1_MDP_FG0_MML, "mml1_mdp_fg0_mml",
	        "mml1_mdp_fg0"/* parent */),
	/* MML11 */
	GATE_MML11(MML1_MDP_RSZ2, "mml1_mdp_rsz2",
			"mm_mml_ck"/* parent */, 4),
	GATE_MML11_V(MML1_MDP_RSZ2_MML, "mml1_mdp_rsz2_mml",
	        "mml1_mdp_rsz2"/* parent */),
	GATE_MML11(MML1_MDP_RSZ3, "mml1_mdp_rsz3",
			"mm_mml_ck"/* parent */, 5),
	GATE_MML11_V(MML1_MDP_RSZ3_MML, "mml1_mdp_rsz3_mml",
	        "mml1_mdp_rsz3"/* parent */),
	GATE_MML11(MML1_MDP_DISP_CHIST0, "mml1_mdp_disp_chist0",
			"mm_mml_ck"/* parent */, 6),
	GATE_MML11_V(MML1_MDP_DISP_CHIST0_MML, "mml1_mdp_disp_chist0_mml",
	        "mml1_mdp_disp_chist0"/* parent */),
	GATE_MML11(MML1_DISP_DBG, "mml1_disp_dbg",
			"mm_mml_ck"/* parent */, 9),
	GATE_MML11_V(MML1_DISP_DBG_MML, "mml1_disp_dbg_mml",
	        "mml1_disp_dbg"/* parent */),
};

static const struct mtk_clk_desc mml1_mcd = {
	.clks = mml1_clks,
	.num_clks = CLK_MML1_NR_CLK,
};

static const struct mtk_gate_regs mml20_cg_regs = {
	.set_ofs = 0xA7C,
	.clr_ofs = 0xA80,
	.sta_ofs = 0xA78,
};

static const struct mtk_gate_regs mml20_hwv_regs = {
	.set_ofs = 0xd8,
	.clr_ofs = 0xdc,
	.sta_ofs = 0x12648,
};

static const struct mtk_gate_regs mml21_cg_regs = {
	.set_ofs = 0xA88,
	.clr_ofs = 0xA8C,
	.sta_ofs = 0xA84,
};

#define GATE_MML20(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mml20_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
		.flags = RES_FRAMEWORK_VDISP,		\
	}

#define GATE_MML20_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_HWV_MML20(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &mml20_cg_regs,			\
		.hwv_regs = &mml20_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VDISP | CLK_USE_HW_VOTER,	\
	}

#define GATE_MML21(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mml21_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MML21_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

static const struct mtk_gate mml2_clks[] = {
	/* MML20 */
	GATE_HWV_MML20(MML2_MDP_MUTEX0, "mml2_mdp_mutex0",
			"mm_mml_ck"/* parent */, 0),
	GATE_MML20_V(MML2_MDP_MUTEX0_MML, "mml2_mdp_mutex0_mml",
	        "mml2_mdp_mutex0"/* parent */),
	GATE_HWV_MML20(MML2_SMI0, "mml2_smi0",
			"mm_mml_ck"/* parent */, 1),
	GATE_MML20_V(MML2_SMI0_SMI, "mml2_smi0_smi",
	        "mml2_smi0"/* parent */),
	GATE_HWV_MML20(MML2_APB_BUS, "mml2_apb_bus",
			"mm_mml_ck"/* parent */, 2),
	GATE_MML20_V(MML2_APB_BUS_MML, "mml2_apb_bus_mml",
	        "mml2_apb_bus"/* parent */),
	GATE_HWV_MML20(MML2_MDP_RDMA0, "mml2_mdp_rdma0",
			"mm_mml_ck"/* parent */, 3),
	GATE_MML20_V(MML2_MDP_RDMA0_MML, "mml2_mdp_rdma0_mml",
	        "mml2_mdp_rdma0"/* parent */),
	GATE_HWV_MML20(MML2_MDP_RSZ0, "mml2_mdp_rsz0",
			"mm_mml_ck"/* parent */, 8),
	GATE_MML20_V(MML2_MDP_RSZ0_MML, "mml2_mdp_rsz0_mml",
	        "mml2_mdp_rsz0"/* parent */),
	GATE_HWV_MML20(MML2_MDP_RSZ1, "mml2_mdp_rsz1",
			"mm_mml_ck"/* parent */, 9),
	GATE_MML20_V(MML2_MDP_RSZ1_MML, "mml2_mdp_rsz1_mml",
	        "mml2_mdp_rsz1"/* parent */),
	GATE_HWV_MML20(MML2_MDP_WROT0, "mml2_mdp_wrot0",
			"mm_mml_ck"/* parent */, 12),
	GATE_MML20_V(MML2_MDP_WROT0_MML, "mml2_mdp_wrot0_mml",
	        "mml2_mdp_wrot0"/* parent */),
	GATE_HWV_MML20(MML2_MDP_FAKE_ENG0, "mml2_mdp_fake_eng0",
			"mm_mml_ck"/* parent */, 14),
	GATE_MML20_V(MML2_MDP_FAKE_ENG0_MML, "mml2_mdp_fake_eng0_mml",
	        "mml2_mdp_fake_eng0"/* parent */),
	GATE_HWV_MML20(MML2_MDP_FAKE_ENG1, "mml2_mdp_fake_eng1",
			"mm_mml_ck"/* parent */, 15),
	GATE_MML20_V(MML2_MDP_FAKE_ENG1_MML, "mml2_mdp_fake_eng1_mml",
	        "mml2_mdp_fake_eng1"/* parent */),
	GATE_HWV_MML20(MML2_APB_DB, "mml2_apb_db",
			"mm_mml_ck"/* parent */, 16),
	GATE_MML20_V(MML2_APB_DB_MML, "mml2_apb_db_mml",
	        "mml2_apb_db"/* parent */),
	GATE_HWV_MML20(MML2_MDP_DLI_ASYNC0, "mml2_mdp_dli_as0",
			"mm_mml_ck"/* parent */, 17),
	GATE_MML20_V(MML2_MDP_DLI_ASYNC0_MML, "mml2_mdp_dli_as0_mml",
	        "mml2_mdp_dli_as0"/* parent */),
	GATE_HWV_MML20(MML2_MDP_DLI_ASYNC1, "mml2_mdp_dli_as1",
			"mm_mml_ck"/* parent */, 18),
	GATE_MML20_V(MML2_MDP_DLI_ASYNC1_MML, "mml2_mdp_dli_as1_mml",
	        "mml2_mdp_dli_as1"/* parent */),
	GATE_HWV_MML20(MML2_MDP_DLO_ASYNC0, "mml2_mdp_dlo_as0",
			"mm_mml_ck"/* parent */, 19),
	GATE_MML20_V(MML2_MDP_DLO_ASYNC0_MML, "mml2_mdp_dlo_as0_mml",
	        "mml2_mdp_dlo_as0"/* parent */),
	GATE_HWV_MML20(MML2_MDP_DLO_ASYNC1, "mml2_mdp_dlo_as1",
			"mm_mml_ck"/* parent */, 20),
	GATE_MML20_V(MML2_MDP_DLO_ASYNC1_MML, "mml2_mdp_dlo_as1_mml",
	        "mml2_mdp_dlo_as1"/* parent */),
	GATE_HWV_MML20(MML2_MDP_DLI_ASYNC2, "mml2_mdp_dli_as2",
			"mm_mml_ck"/* parent */, 21),
	GATE_MML20_V(MML2_MDP_DLI_ASYNC2_MML, "mml2_mdp_dli_as2_mml",
	        "mml2_mdp_dli_as2"/* parent */),
	GATE_HWV_MML20(MML2_MDP_DLO_ASYNC2, "mml2_mdp_dlo_as2",
			"mm_mml_ck"/* parent */, 22),
	GATE_MML20_V(MML2_MDP_DLO_ASYNC2_MML, "mml2_mdp_dlo_as2_mml",
	        "mml2_mdp_dlo_as2"/* parent */),
	GATE_HWV_MML20(MML2_MDP_DLO_ASYNC3, "mml2_mdp_dlo_as3",
			"mm_mml_ck"/* parent */, 23),
	GATE_MML20_V(MML2_MDP_DLO_ASYNC3_MML, "mml2_mdp_dlo_as3_mml",
	        "mml2_mdp_dlo_as3"/* parent */),
	GATE_HWV_MML20(MML2_MDP_RROT0, "mml2_mdp_rrot0",
			"mm_mml_ck"/* parent */, 24),
	GATE_MML20_V(MML2_MDP_RROT0_MML, "mml2_mdp_rrot0_mml",
	        "mml2_mdp_rrot0"/* parent */),
	GATE_HWV_MML20(MML2_MDP_MERGE0, "mml2_mdp_merge0",
			"mm_mml_ck"/* parent */, 25),
	GATE_MML20_V(MML2_MDP_MERGE0_MML, "mml2_mdp_merge0_mml",
	        "mml2_mdp_merge0"/* parent */),
	GATE_HWV_MML20(MML2_MDP_DLO_ASYNC4, "mml2_mdp_dlo_as4",
			"mm_mml_ck"/* parent */, 29),
	GATE_MML20_V(MML2_MDP_DLO_ASYNC4_MML, "mml2_mdp_dlo_as4_mml",
	        "mml2_mdp_dlo_as4"/* parent */),
	GATE_HWV_MML20(MML2_MDP_DLO_ASYNC5, "mml2_mdp_dlo_as5",
			"mm_mml_ck"/* parent */, 30),
	GATE_MML20_V(MML2_MDP_DLO_ASYNC5_MML, "mml2_mdp_dlo_as5_mml",
	        "mml2_mdp_dlo_as5"/* parent */),
	GATE_HWV_MML20(MML2_MDP_RDMA1, "mml2_mdp_rdma1",
			"mm_mml_ck"/* parent */, 31),
	GATE_MML20_V(MML2_MDP_RDMA1_MML, "mml2_mdp_rdma1_mml",
	        "mml2_mdp_rdma1"/* parent */),
	/* MML21 */
	GATE_MML21(MML2_MDP_RROT1, "mml2_mdp_rrot1",
			"mm_mml_ck"/* parent */, 0),
	GATE_MML21_V(MML2_MDP_RROT1_MML, "mml2_mdp_rrot1_mml",
	        "mml2_mdp_rrot1"/* parent */),
	GATE_MML21(MML2_MDP_WROT1, "mml2_mdp_wrot1",
			"mm_mml_ck"/* parent */, 1),
	GATE_MML21_V(MML2_MDP_WROT1_MML, "mml2_mdp_wrot1_mml",
	        "mml2_mdp_wrot1"/* parent */),
	GATE_MML21(MML2_MDP_DISP_WDMA0, "mml2_mdp_disp_wdma0",
			"mm_mml_ck"/* parent */, 2),
	GATE_MML21_V(MML2_MDP_DISP_WDMA0_MML, "mml2_mdp_disp_wdma0_mml",
	        "mml2_mdp_disp_wdma0"/* parent */),
	GATE_MML21(MML2_MDP_DISP_WDMA1, "mml2_mdp_disp_wdma1",
			"mm_mml_ck"/* parent */, 3),
	GATE_MML21_V(MML2_MDP_DISP_WDMA1_MML, "mml2_mdp_disp_wdma1_mml",
	        "mml2_mdp_disp_wdma1"/* parent */),
	GATE_MML21(MML2_MDP_DLI_ASYNC3, "mml2_mdp_dli_as3",
			"mm_mml_ck"/* parent */, 7),
	GATE_MML21_V(MML2_MDP_DLI_ASYNC3_MML, "mml2_mdp_dli_as3_mml",
	        "mml2_mdp_dli_as3"/* parent */),
	GATE_MML21(MML2_MDP_DLI_ASYNC4, "mml2_mdp_dli_as4",
			"mm_mml_ck"/* parent */, 8),
	GATE_MML21_V(MML2_MDP_DLI_ASYNC4_MML, "mml2_mdp_dli_as4_mml",
	        "mml2_mdp_dli_as4"/* parent */),
	GATE_MML21(MML2_DISP_DBG, "mml2_disp_dbg",
			"mm_mml_ck"/* parent */, 9),
	GATE_MML21_V(MML2_DISP_DBG_MML, "mml2_disp_dbg_mml",
	        "mml2_disp_dbg"/* parent */),
};

static const struct mtk_clk_desc mml2_mcd = {
	.clks = mml2_clks,
	.num_clks = CLK_MML2_NR_CLK,
};

static const struct mtk_gate_regs mml0_cg_regs = {
	.set_ofs = 0xA7C,
	.clr_ofs = 0xA80,
	.sta_ofs = 0xA78,
};

static const struct mtk_gate_regs mml0_hwv_regs = {
	.set_ofs = 0xe4,
	.clr_ofs = 0xe8,
	.sta_ofs = 0x1264C,
};

static const struct mtk_gate_regs mml1_cg_regs = {
	.set_ofs = 0xA88,
	.clr_ofs = 0xA8C,
	.sta_ofs = 0xA84,
};

#define GATE_MML0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mml0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
		.flags = RES_FRAMEWORK_VDISP,		\
	}

#define GATE_MML0_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_HWV_MML0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &mml0_cg_regs,			\
		.hwv_regs = &mml0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VDISP | CLK_USE_HW_VOTER,	\
	}

#define GATE_MML1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mml1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MML1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

static const struct mtk_gate mml_clks[] = {
	/* MML0 */
	GATE_HWV_MML0(MML_MDP_MUTEX0, "mml_mdp_mutex0",
			"mm_mml_ck"/* parent */, 0),
	GATE_MML0_V(MML_MDP_MUTEX0_MML, "mml_mdp_mutex0_mml",
	        "mml_mdp_mutex0"/* parent */),
	GATE_HWV_MML0(MML_SMI0, "mml_smi0",
			"mm_mml_ck"/* parent */, 1),
	GATE_MML0_V(MML_SMI0_SMI, "mml_smi0_smi",
	        "mml_smi0"/* parent */),
	GATE_HWV_MML0(MML_APB_BUS, "mml_apb_bus",
			"mm_mml_ck"/* parent */, 2),
	GATE_MML0_V(MML_APB_BUS_MML, "mml_apb_bus_mml",
	        "mml_apb_bus"/* parent */),
	GATE_HWV_MML0(MML_MDP_RDMA2, "mml_mdp_rdma2",
			"mm_mml_ck"/* parent */, 4),
	GATE_MML0_V(MML_MDP_RDMA2_MML, "mml_mdp_rdma2_mml",
	        "mml_mdp_rdma2"/* parent */),
	GATE_HWV_MML0(MML_MDP_BIRSZ0, "mml_mdp_birsz0",
			"mm_mml_ck"/* parent */, 5),
	GATE_MML0_V(MML_MDP_BIRSZ0_MML, "mml_mdp_birsz0_mml",
	        "mml_mdp_birsz0"/* parent */),
	GATE_HWV_MML0(MML_MDP_HDR0, "mml_mdp_hdr0",
			"mm_mml_ck"/* parent */, 6),
	GATE_MML0_V(MML_MDP_HDR0_MML, "mml_mdp_hdr0_mml",
	        "mml_mdp_hdr0"/* parent */),
	GATE_HWV_MML0(MML_MDP_AAL0, "mml_mdp_aal0",
			"mm_mml_ck"/* parent */, 7),
	GATE_MML0_V(MML_MDP_AAL0_MML, "mml_mdp_aal0_mml",
	        "mml_mdp_aal0"/* parent */),
	GATE_HWV_MML0(MML_MDP_TDSHP0, "mml_mdp_tdshp0",
			"mm_mml_ck"/* parent */, 10),
	GATE_MML0_V(MML_MDP_TDSHP0_MML, "mml_mdp_tdshp0_mml",
	        "mml_mdp_tdshp0"/* parent */),
	GATE_HWV_MML0(MML_MDP_COLOR0, "mml_mdp_color0",
			"mm_mml_ck"/* parent */, 11),
	GATE_MML0_V(MML_MDP_COLOR0_MML, "mml_mdp_color0_mml",
	        "mml_mdp_color0"/* parent */),
	GATE_HWV_MML0(MML_MDP_WROT2, "mml_mdp_wrot2",
			"mm_mml_ck"/* parent */, 13),
	GATE_MML0_V(MML_MDP_WROT2_MML, "mml_mdp_wrot2_mml",
	        "mml_mdp_wrot2"/* parent */),
	GATE_HWV_MML0(MML_MDP_FAKE_ENG0, "mml_mdp_fake_eng0",
			"mm_mml_ck"/* parent */, 14),
	GATE_MML0_V(MML_MDP_FAKE_ENG0_MML, "mml_mdp_fake_eng0_mml",
	        "mml_mdp_fake_eng0"/* parent */),
	GATE_HWV_MML0(MML_MDP_FAKE_ENG1, "mml_mdp_fake_eng1",
			"mm_mml_ck"/* parent */, 15),
	GATE_MML0_V(MML_MDP_FAKE_ENG1_MML, "mml_mdp_fake_eng1_mml",
	        "mml_mdp_fake_eng1"/* parent */),
	GATE_HWV_MML0(MML_APB_DB, "mml_apb_db",
			"mm_mml_ck"/* parent */, 16),
	GATE_MML0_V(MML_APB_DB_MML, "mml_apb_db_mml",
	        "mml_apb_db"/* parent */),
	GATE_HWV_MML0(MML_MDP_DLI_ASYNC0, "mml_mdp_dli_as0",
			"mm_mml_ck"/* parent */, 17),
	GATE_MML0_V(MML_MDP_DLI_ASYNC0_MML, "mml_mdp_dli_as0_mml",
	        "mml_mdp_dli_as0"/* parent */),
	GATE_HWV_MML0(MML_MDP_DLO_ASYNC0, "mml_mdp_dlo_as0",
			"mm_mml_ck"/* parent */, 19),
	GATE_MML0_V(MML_MDP_DLO_ASYNC0_MML, "mml_mdp_dlo_as0_mml",
	        "mml_mdp_dlo_as0"/* parent */),
	GATE_HWV_MML0(MML_MDP_C3D0, "mml_mdp_c3d0",
			"mm_mml_ck"/* parent */, 26),
	GATE_MML0_V(MML_MDP_C3D0_MML, "mml_mdp_c3d0_mml",
	        "mml_mdp_c3d0"/* parent */),
	GATE_HWV_MML0(MML_MDP_FG0, "mml_mdp_fg0",
			"mm_mml_ck"/* parent */, 27),
	GATE_MML0_V(MML_MDP_FG0_MML, "mml_mdp_fg0_mml",
	        "mml_mdp_fg0"/* parent */),
	/* MML1 */
	GATE_MML1(MML_MDP_RSZ2, "mml_mdp_rsz2",
			"mm_mml_ck"/* parent */, 4),
	GATE_MML1_V(MML_MDP_RSZ2_MML, "mml_mdp_rsz2_mml",
	        "mml_mdp_rsz2"/* parent */),
	GATE_MML1(MML_MDP_RSZ3, "mml_mdp_rsz3",
			"mm_mml_ck"/* parent */, 5),
	GATE_MML1_V(MML_MDP_RSZ3_MML, "mml_mdp_rsz3_mml",
	        "mml_mdp_rsz3"/* parent */),
	GATE_MML1(MML_MDP_DISP_CHIST0, "mml_mdp_disp_chist0",
			"mm_mml_ck"/* parent */, 6),
	GATE_MML1_V(MML_MDP_DISP_CHIST0_MML, "mml_mdp_disp_chist0_mml",
	        "mml_mdp_disp_chist0"/* parent */),
	GATE_MML1(MML_DISP_DBG, "mml_disp_dbg",
			"mm_mml_ck"/* parent */, 9),
	GATE_MML1_V(MML_DISP_DBG_MML, "mml_disp_dbg_mml",
	        "mml_disp_dbg"/* parent */),
};

static const struct mtk_clk_desc mml_mcd = {
	.clks = mml_clks,
	.num_clks = CLK_MML_NR_CLK,
};

static const struct mtk_gate_regs ovl10_cg_regs = {
	.set_ofs = 0xACC,
	.clr_ofs = 0xAD0,
	.sta_ofs = 0xAC8,
};

static const struct mtk_gate_regs ovl11_cg_regs = {
	.set_ofs = 0xAD8,
	.clr_ofs = 0xADC,
	.sta_ofs = 0xAD4,
};

static const struct mtk_gate_regs ovl11_hwv_regs = {
	.set_ofs = 0xc0,
	.clr_ofs = 0xc4,
	.sta_ofs = 0x12640,
};

#define GATE_OVL10(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ovl10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
		.flags = RES_FRAMEWORK_VDISP,		\
	}

#define GATE_OVL10_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_OVL11(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ovl11_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_OVL11_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_HWV_OVL11(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ovl11_cg_regs,			\
		.hwv_regs = &ovl11_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VDISP | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate ovl1_clks[] = {
	/* OVL10 */
	GATE_OVL10(OVL1_OVLSYS_CONFIG, "ovl1_ovlsys_config",
			"mm_disp_ck"/* parent */, 0),
	GATE_OVL10_V(OVL1_OVLSYS_CONFIG_DISP, "ovl1_ovlsys_config_disp",
	        "ovl1_ovlsys_config"/* parent */),
	GATE_OVL10(OVL1_OVL_MUTEX0, "ovl1_ovl_mutex0",
			"mm_disp_ck"/* parent */, 1),
	GATE_OVL10_V(OVL1_OVL_MUTEX0_DISP, "ovl1_ovl_mutex0_disp",
	        "ovl1_ovl_mutex0"/* parent */),
	GATE_OVL10(OVL1_OVL_EXDMA0, "ovl1_ovl_exdma0",
			"mm_disp_ck"/* parent */, 2),
	GATE_OVL10_V(OVL1_OVL_EXDMA0_DISP, "ovl1_ovl_exdma0_disp",
	        "ovl1_ovl_exdma0"/* parent */),
	GATE_OVL10(OVL1_OVL_EXDMA1, "ovl1_ovl_exdma1",
			"mm_disp_ck"/* parent */, 3),
	GATE_OVL10_V(OVL1_OVL_EXDMA1_DISP, "ovl1_ovl_exdma1_disp",
	        "ovl1_ovl_exdma1"/* parent */),
	GATE_OVL10(OVL1_OVL_EXDMA2, "ovl1_ovl_exdma2",
			"mm_disp_ck"/* parent */, 4),
	GATE_OVL10_V(OVL1_OVL_EXDMA2_DISP, "ovl1_ovl_exdma2_disp",
	        "ovl1_ovl_exdma2"/* parent */),
	GATE_OVL10(OVL1_OVL_EXDMA3, "ovl1_ovl_exdma3",
			"mm_disp_ck"/* parent */, 5),
	GATE_OVL10_V(OVL1_OVL_EXDMA3_DISP, "ovl1_ovl_exdma3_disp",
	        "ovl1_ovl_exdma3"/* parent */),
	GATE_OVL10(OVL1_OVL_EXDMA4, "ovl1_ovl_exdma4",
			"mm_disp_ck"/* parent */, 6),
	GATE_OVL10_V(OVL1_OVL_EXDMA4_DISP, "ovl1_ovl_exdma4_disp",
	        "ovl1_ovl_exdma4"/* parent */),
	GATE_OVL10(OVL1_OVL_EXDMA5, "ovl1_ovl_exdma5",
			"mm_disp_ck"/* parent */, 7),
	GATE_OVL10_V(OVL1_OVL_EXDMA5_DISP, "ovl1_ovl_exdma5_disp",
	        "ovl1_ovl_exdma5"/* parent */),
	GATE_OVL10(OVL1_OVL_EXDMA6, "ovl1_ovl_exdma6",
			"mm_disp_ck"/* parent */, 8),
	GATE_OVL10_V(OVL1_OVL_EXDMA6_DISP, "ovl1_ovl_exdma6_disp",
	        "ovl1_ovl_exdma6"/* parent */),
	GATE_OVL10(OVL1_OVL_EXDMA7, "ovl1_ovl_exdma7",
			"mm_disp_ck"/* parent */, 9),
	GATE_OVL10_V(OVL1_OVL_EXDMA7_DISP, "ovl1_ovl_exdma7_disp",
	        "ovl1_ovl_exdma7"/* parent */),
	GATE_OVL10(OVL1_OVL_BLENDER0, "ovl1_ovl_blender0",
			"mm_disp_ck"/* parent */, 10),
	GATE_OVL10_V(OVL1_OVL_BLENDER0_DISP, "ovl1_ovl_blender0_disp",
	        "ovl1_ovl_blender0"/* parent */),
	GATE_OVL10(OVL1_OVL_BLENDER1, "ovl1_ovl_blender1",
			"mm_disp_ck"/* parent */, 11),
	GATE_OVL10_V(OVL1_OVL_BLENDER1_DISP, "ovl1_ovl_blender1_disp",
	        "ovl1_ovl_blender1"/* parent */),
	GATE_OVL10(OVL1_OVL_BLENDER2, "ovl1_ovl_blender2",
			"mm_disp_ck"/* parent */, 12),
	GATE_OVL10_V(OVL1_OVL_BLENDER2_DISP, "ovl1_ovl_blender2_disp",
	        "ovl1_ovl_blender2"/* parent */),
	GATE_OVL10(OVL1_OVL_BLENDER3, "ovl1_ovl_blender3",
			"mm_disp_ck"/* parent */, 13),
	GATE_OVL10_V(OVL1_OVL_BLENDER3_DISP, "ovl1_ovl_blender3_disp",
	        "ovl1_ovl_blender3"/* parent */),
	GATE_OVL10(OVL1_OVL_BLENDER4, "ovl1_ovl_blender4",
			"mm_disp_ck"/* parent */, 14),
	GATE_OVL10_V(OVL1_OVL_BLENDER4_DISP, "ovl1_ovl_blender4_disp",
	        "ovl1_ovl_blender4"/* parent */),
	GATE_OVL10(OVL1_OVL_BLENDER5, "ovl1_ovl_blender5",
			"mm_disp_ck"/* parent */, 15),
	GATE_OVL10_V(OVL1_OVL_BLENDER5_DISP, "ovl1_ovl_blender5_disp",
	        "ovl1_ovl_blender5"/* parent */),
	GATE_OVL10(OVL1_OVL_BLENDER6, "ovl1_ovl_blender6",
			"mm_disp_ck"/* parent */, 16),
	GATE_OVL10_V(OVL1_OVL_BLENDER6_DISP, "ovl1_ovl_blender6_disp",
	        "ovl1_ovl_blender6"/* parent */),
	GATE_OVL10(OVL1_OVL_BLENDER7, "ovl1_ovl_blender7",
			"mm_disp_ck"/* parent */, 17),
	GATE_OVL10_V(OVL1_OVL_BLENDER7_DISP, "ovl1_ovl_blender7_disp",
	        "ovl1_ovl_blender7"/* parent */),
	GATE_OVL10(OVL1_OVL_OUTPROC0, "ovl1_ovl_outproc0",
			"mm_disp_ck"/* parent */, 18),
	GATE_OVL10_V(OVL1_OVL_OUTPROC0_DISP, "ovl1_ovl_outproc0_disp",
	        "ovl1_ovl_outproc0"/* parent */),
	GATE_OVL10(OVL1_OVL_OUTPROC1, "ovl1_ovl_outproc1",
			"mm_disp_ck"/* parent */, 19),
	GATE_OVL10_V(OVL1_OVL_OUTPROC1_DISP, "ovl1_ovl_outproc1_disp",
	        "ovl1_ovl_outproc1"/* parent */),
	GATE_OVL10(OVL1_OVL_OUTPROC2, "ovl1_ovl_outproc2",
			"mm_disp_ck"/* parent */, 20),
	GATE_OVL10_V(OVL1_OVL_OUTPROC2_DISP, "ovl1_ovl_outproc2_disp",
	        "ovl1_ovl_outproc2"/* parent */),
	GATE_OVL10(OVL1_OVL_OUTPROC3, "ovl1_ovl_outproc3",
			"mm_disp_ck"/* parent */, 21),
	GATE_OVL10_V(OVL1_OVL_OUTPROC3_DISP, "ovl1_ovl_outproc3_disp",
	        "ovl1_ovl_outproc3"/* parent */),
	GATE_OVL10(OVL1_OVL_MDP_RSZ0, "ovl1_ovl_mdp_rsz0",
			"mm_disp_ck"/* parent */, 22),
	GATE_OVL10_V(OVL1_OVL_MDP_RSZ0_DISP, "ovl1_ovl_mdp_rsz0_disp",
	        "ovl1_ovl_mdp_rsz0"/* parent */),
	GATE_OVL10(OVL1_OVL_ODPM0, "ovl1_ovl_odpm0",
			"mm_disp_ck"/* parent */, 23),
	GATE_OVL10_V(OVL1_OVL_ODPM0_DISP, "ovl1_ovl_odpm0_disp",
	        "ovl1_ovl_odpm0"/* parent */),
	GATE_OVL10(OVL1_INLINEROT0, "ovl1_inlinerot0",
			"mm_disp_ck"/* parent */, 24),
	GATE_OVL10_V(OVL1_INLINEROT0_DISP, "ovl1_inlinerot0_disp",
	        "ovl1_inlinerot0"/* parent */),
	GATE_OVL10(OVL1_OVL_FAKE_ENG0, "ovl1_ovl_fake_eng0",
			"mm_disp_ck"/* parent */, 25),
	GATE_OVL10_V(OVL1_OVL_FAKE_ENG0_DISP, "ovl1_ovl_fake_eng0_disp",
	        "ovl1_ovl_fake_eng0"/* parent */),
	GATE_OVL10(OVL1_OVL_FAKE_ENG1, "ovl1_ovl_fake_eng1",
			"mm_disp_ck"/* parent */, 26),
	GATE_OVL10_V(OVL1_OVL_FAKE_ENG1_DISP, "ovl1_ovl_fake_eng1_disp",
	        "ovl1_ovl_fake_eng1"/* parent */),
	GATE_OVL10(OVL1_OVL_FAKE_ENG2, "ovl1_ovl_fake_eng2",
			"mm_disp_ck"/* parent */, 27),
	GATE_OVL10_V(OVL1_OVL_FAKE_ENG2_DISP, "ovl1_ovl_fake_eng2_disp",
	        "ovl1_ovl_fake_eng2"/* parent */),
	GATE_OVL10(OVL1_OVL_FAKE_ENG3, "ovl1_ovl_fake_eng3",
			"mm_disp_ck"/* parent */, 28),
	GATE_OVL10_V(OVL1_OVL_FAKE_ENG3_DISP, "ovl1_ovl_fake_eng3_disp",
	        "ovl1_ovl_fake_eng3"/* parent */),
	GATE_OVL10(OVL1_OVL_DLI_ASYNC0, "ovl1_ovl_dli_as0",
			"mm_disp_ck"/* parent */, 29),
	GATE_OVL10_V(OVL1_OVL_DLI_ASYNC0_DISP, "ovl1_ovl_dli_as0_disp",
	        "ovl1_ovl_dli_as0"/* parent */),
	GATE_OVL10(OVL1_OVL_DLI_ASYNC1, "ovl1_ovl_dli_as1",
			"mm_disp_ck"/* parent */, 30),
	GATE_OVL10_V(OVL1_OVL_DLI_ASYNC1_DISP, "ovl1_ovl_dli_as1_disp",
	        "ovl1_ovl_dli_as1"/* parent */),
	GATE_OVL10(OVL1_OVL_DLI_ASYNC2, "ovl1_ovl_dli_as2",
			"mm_disp_ck"/* parent */, 31),
	GATE_OVL10_V(OVL1_OVL_DLI_ASYNC2_DISP, "ovl1_ovl_dli_as2_disp",
	        "ovl1_ovl_dli_as2"/* parent */),
	/* OVL11 */
	GATE_HWV_OVL11(OVL1_OVL_DLI_ASYNC3, "ovl1_ovl_dli_as3",
			"mm_disp_ck"/* parent */, 0),
	GATE_OVL11_V(OVL1_OVL_DLI_ASYNC3_DISP, "ovl1_ovl_dli_as3_disp",
	        "ovl1_ovl_dli_as3"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLI_ASYNC4, "ovl1_ovl_dli_as4",
			"mm_disp_ck"/* parent */, 1),
	GATE_OVL11_V(OVL1_OVL_DLI_ASYNC4_DISP, "ovl1_ovl_dli_as4_disp",
	        "ovl1_ovl_dli_as4"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLI_ASYNC5, "ovl1_ovl_dli_as5",
			"mm_disp_ck"/* parent */, 2),
	GATE_OVL11_V(OVL1_OVL_DLI_ASYNC5_DISP, "ovl1_ovl_dli_as5_disp",
	        "ovl1_ovl_dli_as5"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLI_ASYNC6, "ovl1_ovl_dli_as6",
			"mm_disp_ck"/* parent */, 3),
	GATE_OVL11_V(OVL1_OVL_DLI_ASYNC6_DISP, "ovl1_ovl_dli_as6_disp",
	        "ovl1_ovl_dli_as6"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLI_ASYNC7, "ovl1_ovl_dli_as7",
			"mm_disp_ck"/* parent */, 4),
	GATE_OVL11_V(OVL1_OVL_DLI_ASYNC7_DISP, "ovl1_ovl_dli_as7_disp",
	        "ovl1_ovl_dli_as7"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLI_ASYNC8, "ovl1_ovl_dli_as8",
			"mm_disp_ck"/* parent */, 5),
	GATE_OVL11_V(OVL1_OVL_DLI_ASYNC8_DISP, "ovl1_ovl_dli_as8_disp",
	        "ovl1_ovl_dli_as8"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLI_ASYNC9, "ovl1_ovl_dli_as9",
			"mm_disp_ck"/* parent */, 6),
	GATE_OVL11_V(OVL1_OVL_DLI_ASYNC9_DISP, "ovl1_ovl_dli_as9_disp",
	        "ovl1_ovl_dli_as9"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLI_ASYNC10, "ovl1_ovl_dli_as10",
			"mm_disp_ck"/* parent */, 7),
	GATE_OVL11_V(OVL1_OVL_DLI_ASYNC10_DISP, "ovl1_ovl_dli_as10_disp",
	        "ovl1_ovl_dli_as10"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLI_ASYNC11, "ovl1_ovl_dli_as11",
			"mm_disp_ck"/* parent */, 8),
	GATE_OVL11_V(OVL1_OVL_DLI_ASYNC11_DISP, "ovl1_ovl_dli_as11_disp",
	        "ovl1_ovl_dli_as11"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLI_ASYNC12, "ovl1_ovl_dli_as12",
			"mm_disp_ck"/* parent */, 9),
	GATE_OVL11_V(OVL1_OVL_DLI_ASYNC12_DISP, "ovl1_ovl_dli_as12_disp",
	        "ovl1_ovl_dli_as12"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLO_ASYNC0, "ovl1_ovl_dlo_as0",
			"mm_disp_ck"/* parent */, 10),
	GATE_OVL11_V(OVL1_OVL_DLO_ASYNC0_DISP, "ovl1_ovl_dlo_as0_disp",
	        "ovl1_ovl_dlo_as0"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLO_ASYNC1, "ovl1_ovl_dlo_as1",
			"mm_disp_ck"/* parent */, 11),
	GATE_OVL11_V(OVL1_OVL_DLO_ASYNC1_DISP, "ovl1_ovl_dlo_as1_disp",
	        "ovl1_ovl_dlo_as1"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLO_ASYNC2, "ovl1_ovl_dlo_as2",
			"mm_disp_ck"/* parent */, 12),
	GATE_OVL11_V(OVL1_OVL_DLO_ASYNC2_DISP, "ovl1_ovl_dlo_as2_disp",
	        "ovl1_ovl_dlo_as2"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLO_ASYNC3, "ovl1_ovl_dlo_as3",
			"mm_disp_ck"/* parent */, 13),
	GATE_OVL11_V(OVL1_OVL_DLO_ASYNC3_DISP, "ovl1_ovl_dlo_as3_disp",
	        "ovl1_ovl_dlo_as3"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLO_ASYNC4, "ovl1_ovl_dlo_as4",
			"mm_disp_ck"/* parent */, 14),
	GATE_OVL11_V(OVL1_OVL_DLO_ASYNC4_DISP, "ovl1_ovl_dlo_as4_disp",
	        "ovl1_ovl_dlo_as4"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLO_ASYNC5, "ovl1_ovl_dlo_as5",
			"mm_disp_ck"/* parent */, 15),
	GATE_OVL11_V(OVL1_OVL_DLO_ASYNC5_DISP, "ovl1_ovl_dlo_as5_disp",
	        "ovl1_ovl_dlo_as5"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLO_ASYNC6, "ovl1_ovl_dlo_as6",
			"mm_disp_ck"/* parent */, 16),
	GATE_OVL11_V(OVL1_OVL_DLO_ASYNC6_DISP, "ovl1_ovl_dlo_as6_disp",
	        "ovl1_ovl_dlo_as6"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLO_ASYNC7, "ovl1_ovl_dlo_as7",
			"mm_disp_ck"/* parent */, 17),
	GATE_OVL11_V(OVL1_OVL_DLO_ASYNC7_DISP, "ovl1_ovl_dlo_as7_disp",
	        "ovl1_ovl_dlo_as7"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLO_ASYNC8, "ovl1_ovl_dlo_as8",
			"mm_disp_ck"/* parent */, 18),
	GATE_OVL11_V(OVL1_OVL_DLO_ASYNC8_DISP, "ovl1_ovl_dlo_as8_disp",
	        "ovl1_ovl_dlo_as8"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLO_ASYNC9, "ovl1_ovl_dlo_as9",
			"mm_disp_ck"/* parent */, 19),
	GATE_OVL11_V(OVL1_OVL_DLO_ASYNC9_DISP, "ovl1_ovl_dlo_as9_disp",
	        "ovl1_ovl_dlo_as9"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLO_ASYNC10, "ovl1_ovl_dlo_as10",
			"mm_disp_ck"/* parent */, 20),
	GATE_OVL11_V(OVL1_OVL_DLO_ASYNC10_DISP, "ovl1_ovl_dlo_as10_disp",
	        "ovl1_ovl_dlo_as10"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLO_ASYNC11, "ovl1_ovl_dlo_as11",
			"mm_disp_ck"/* parent */, 21),
	GATE_OVL11_V(OVL1_OVL_DLO_ASYNC11_DISP, "ovl1_ovl_dlo_as11_disp",
	        "ovl1_ovl_dlo_as11"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLO_ASYNC12, "ovl1_ovl_dlo_as12",
			"mm_disp_ck"/* parent */, 22),
	GATE_OVL11_V(OVL1_OVL_DLO_ASYNC12_DISP, "ovl1_ovl_dlo_as12_disp",
	        "ovl1_ovl_dlo_as12"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLO_ASYNC13, "ovl1_ovl_dlo_as13",
			"mm_disp_ck"/* parent */, 23),
	GATE_OVL11_V(OVL1_OVL_DLO_ASYNC13_DISP, "ovl1_ovl_dlo_as13_disp",
	        "ovl1_ovl_dlo_as13"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLO_ASYNC14, "ovl1_ovl_dlo_as14",
			"mm_disp_ck"/* parent */, 24),
	GATE_OVL11_V(OVL1_OVL_DLO_ASYNC14_DISP, "ovl1_ovl_dlo_as14_disp",
	        "ovl1_ovl_dlo_as14"/* parent */),
	GATE_HWV_OVL11(OVL1_OVL_DLO_ASYNC15, "ovl1_ovl_dlo_as15",
			"mm_disp_ck"/* parent */, 25),
	GATE_OVL11_V(OVL1_OVL_DLO_ASYNC15_DISP, "ovl1_ovl_dlo_as15_disp",
	        "ovl1_ovl_dlo_as15"/* parent */),
	GATE_HWV_OVL11(OVL1_OVLSYS_RELAY0, "ovl1_ovlsys_relay0",
			"mm_disp_ck"/* parent */, 26),
	GATE_OVL11_V(OVL1_OVLSYS_RELAY0_DISP, "ovl1_ovlsys_relay0_disp",
	        "ovl1_ovlsys_relay0"/* parent */),
	GATE_HWV_OVL11(OVL1_SMI_SUB_COMM0, "ovl1_ssc",
			"mm_disp_ck"/* parent */, 27),
	GATE_OVL11_V(OVL1_SMI_SUB_COMM0_DISP, "ovl1_ssc_disp",
	        "ovl1_ssc"/* parent */),
	GATE_OVL11_V(OVL1_SMI_SUB_COMM0_SMI, "ovl1_ssc_smi",
	        "ovl1_ssc"/* parent */),
	GATE_HWV_OVL11(OVL1_DISP_DBG, "ovl1_disp_dbg",
			"mm_disp_ck"/* parent */, 28),
	GATE_OVL11_V(OVL1_DISP_DBG_DISP, "ovl1_disp_dbg_disp",
	        "ovl1_disp_dbg"/* parent */),
};

static const struct mtk_clk_desc ovl1_mcd = {
	.clks = ovl1_clks,
	.num_clks = CLK_OVL1_NR_CLK,
};

static const struct mtk_gate_regs ovl20_cg_regs = {
	.set_ofs = 0xACC,
	.clr_ofs = 0xAD0,
	.sta_ofs = 0xAC8,
};

static const struct mtk_gate_regs ovl21_cg_regs = {
	.set_ofs = 0xAD8,
	.clr_ofs = 0xADC,
	.sta_ofs = 0xAD4,
};

static const struct mtk_gate_regs ovl21_hwv_regs = {
	.set_ofs = 0xcc,
	.clr_ofs = 0xd0,
	.sta_ofs = 0x12644,
};

#define GATE_OVL20(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ovl20_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
		.flags = RES_FRAMEWORK_VDISP,		\
	}

#define GATE_OVL20_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_OVL21(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ovl21_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_OVL21_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_HWV_OVL21(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ovl21_cg_regs,			\
		.hwv_regs = &ovl21_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VDISP | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate ovl2_clks[] = {
	/* OVL20 */
	GATE_OVL20(OVL2_OVLSYS_CONFIG, "ovl2_ovlsys_config",
			"mm_disp_ck"/* parent */, 0),
	GATE_OVL20_V(OVL2_OVLSYS_CONFIG_DISP, "ovl2_ovlsys_config_disp",
	        "ovl2_ovlsys_config"/* parent */),
	GATE_OVL20(OVL2_OVL_MUTEX0, "ovl2_ovl_mutex0",
			"mm_disp_ck"/* parent */, 1),
	GATE_OVL20_V(OVL2_OVL_MUTEX0_DISP, "ovl2_ovl_mutex0_disp",
	        "ovl2_ovl_mutex0"/* parent */),
	GATE_OVL20(OVL2_OVL_EXDMA0, "ovl2_ovl_exdma0",
			"mm_disp_ck"/* parent */, 2),
	GATE_OVL20_V(OVL2_OVL_EXDMA0_DISP, "ovl2_ovl_exdma0_disp",
	        "ovl2_ovl_exdma0"/* parent */),
	GATE_OVL20(OVL2_OVL_EXDMA1, "ovl2_ovl_exdma1",
			"mm_disp_ck"/* parent */, 3),
	GATE_OVL20_V(OVL2_OVL_EXDMA1_DISP, "ovl2_ovl_exdma1_disp",
	        "ovl2_ovl_exdma1"/* parent */),
	GATE_OVL20(OVL2_OVL_EXDMA2, "ovl2_ovl_exdma2",
			"mm_disp_ck"/* parent */, 4),
	GATE_OVL20_V(OVL2_OVL_EXDMA2_DISP, "ovl2_ovl_exdma2_disp",
	        "ovl2_ovl_exdma2"/* parent */),
	GATE_OVL20(OVL2_OVL_EXDMA3, "ovl2_ovl_exdma3",
			"mm_disp_ck"/* parent */, 5),
	GATE_OVL20_V(OVL2_OVL_EXDMA3_DISP, "ovl2_ovl_exdma3_disp",
	        "ovl2_ovl_exdma3"/* parent */),
	GATE_OVL20(OVL2_OVL_EXDMA4, "ovl2_ovl_exdma4",
			"mm_disp_ck"/* parent */, 6),
	GATE_OVL20_V(OVL2_OVL_EXDMA4_DISP, "ovl2_ovl_exdma4_disp",
	        "ovl2_ovl_exdma4"/* parent */),
	GATE_OVL20(OVL2_OVL_EXDMA5, "ovl2_ovl_exdma5",
			"mm_disp_ck"/* parent */, 7),
	GATE_OVL20_V(OVL2_OVL_EXDMA5_DISP, "ovl2_ovl_exdma5_disp",
	        "ovl2_ovl_exdma5"/* parent */),
	GATE_OVL20(OVL2_OVL_EXDMA6, "ovl2_ovl_exdma6",
			"mm_disp_ck"/* parent */, 8),
	GATE_OVL20_V(OVL2_OVL_EXDMA6_DISP, "ovl2_ovl_exdma6_disp",
	        "ovl2_ovl_exdma6"/* parent */),
	GATE_OVL20(OVL2_OVL_EXDMA7, "ovl2_ovl_exdma7",
			"mm_disp_ck"/* parent */, 9),
	GATE_OVL20_V(OVL2_OVL_EXDMA7_DISP, "ovl2_ovl_exdma7_disp",
	        "ovl2_ovl_exdma7"/* parent */),
	GATE_OVL20(OVL2_OVL_BLENDER0, "ovl2_ovl_blender0",
			"mm_disp_ck"/* parent */, 10),
	GATE_OVL20_V(OVL2_OVL_BLENDER0_DISP, "ovl2_ovl_blender0_disp",
	        "ovl2_ovl_blender0"/* parent */),
	GATE_OVL20(OVL2_OVL_BLENDER1, "ovl2_ovl_blender1",
			"mm_disp_ck"/* parent */, 11),
	GATE_OVL20_V(OVL2_OVL_BLENDER1_DISP, "ovl2_ovl_blender1_disp",
	        "ovl2_ovl_blender1"/* parent */),
	GATE_OVL20(OVL2_OVL_BLENDER2, "ovl2_ovl_blender2",
			"mm_disp_ck"/* parent */, 12),
	GATE_OVL20_V(OVL2_OVL_BLENDER2_DISP, "ovl2_ovl_blender2_disp",
	        "ovl2_ovl_blender2"/* parent */),
	GATE_OVL20(OVL2_OVL_BLENDER3, "ovl2_ovl_blender3",
			"mm_disp_ck"/* parent */, 13),
	GATE_OVL20_V(OVL2_OVL_BLENDER3_DISP, "ovl2_ovl_blender3_disp",
	        "ovl2_ovl_blender3"/* parent */),
	GATE_OVL20(OVL2_OVL_BLENDER4, "ovl2_ovl_blender4",
			"mm_disp_ck"/* parent */, 14),
	GATE_OVL20_V(OVL2_OVL_BLENDER4_DISP, "ovl2_ovl_blender4_disp",
	        "ovl2_ovl_blender4"/* parent */),
	GATE_OVL20(OVL2_OVL_BLENDER5, "ovl2_ovl_blender5",
			"mm_disp_ck"/* parent */, 15),
	GATE_OVL20_V(OVL2_OVL_BLENDER5_DISP, "ovl2_ovl_blender5_disp",
	        "ovl2_ovl_blender5"/* parent */),
	GATE_OVL20(OVL2_OVL_BLENDER6, "ovl2_ovl_blender6",
			"mm_disp_ck"/* parent */, 16),
	GATE_OVL20_V(OVL2_OVL_BLENDER6_DISP, "ovl2_ovl_blender6_disp",
	        "ovl2_ovl_blender6"/* parent */),
	GATE_OVL20(OVL2_OVL_BLENDER7, "ovl2_ovl_blender7",
			"mm_disp_ck"/* parent */, 17),
	GATE_OVL20_V(OVL2_OVL_BLENDER7_DISP, "ovl2_ovl_blender7_disp",
	        "ovl2_ovl_blender7"/* parent */),
	GATE_OVL20(OVL2_OVL_OUTPROC0, "ovl2_ovl_outproc0",
			"mm_disp_ck"/* parent */, 18),
	GATE_OVL20_V(OVL2_OVL_OUTPROC0_DISP, "ovl2_ovl_outproc0_disp",
	        "ovl2_ovl_outproc0"/* parent */),
	GATE_OVL20(OVL2_OVL_OUTPROC1, "ovl2_ovl_outproc1",
			"mm_disp_ck"/* parent */, 19),
	GATE_OVL20_V(OVL2_OVL_OUTPROC1_DISP, "ovl2_ovl_outproc1_disp",
	        "ovl2_ovl_outproc1"/* parent */),
	GATE_OVL20(OVL2_OVL_OUTPROC2, "ovl2_ovl_outproc2",
			"mm_disp_ck"/* parent */, 20),
	GATE_OVL20_V(OVL2_OVL_OUTPROC2_DISP, "ovl2_ovl_outproc2_disp",
	        "ovl2_ovl_outproc2"/* parent */),
	GATE_OVL20(OVL2_OVL_OUTPROC3, "ovl2_ovl_outproc3",
			"mm_disp_ck"/* parent */, 21),
	GATE_OVL20_V(OVL2_OVL_OUTPROC3_DISP, "ovl2_ovl_outproc3_disp",
	        "ovl2_ovl_outproc3"/* parent */),
	GATE_OVL20(OVL2_OVL_MDP_RSZ0, "ovl2_ovl_mdp_rsz0",
			"mm_disp_ck"/* parent */, 22),
	GATE_OVL20_V(OVL2_OVL_MDP_RSZ0_DISP, "ovl2_ovl_mdp_rsz0_disp",
	        "ovl2_ovl_mdp_rsz0"/* parent */),
	GATE_OVL20(OVL2_OVL_ODPM0, "ovl2_ovl_odpm0",
			"mm_disp_ck"/* parent */, 23),
	GATE_OVL20_V(OVL2_OVL_ODPM0_DISP, "ovl2_ovl_odpm0_disp",
	        "ovl2_ovl_odpm0"/* parent */),
	GATE_OVL20(OVL2_INLINEROT0, "ovl2_inlinerot0",
			"mm_disp_ck"/* parent */, 24),
	GATE_OVL20_V(OVL2_INLINEROT0_DISP, "ovl2_inlinerot0_disp",
	        "ovl2_inlinerot0"/* parent */),
	GATE_OVL20(OVL2_OVL_FAKE_ENG0, "ovl2_ovl_fake_eng0",
			"mm_disp_ck"/* parent */, 25),
	GATE_OVL20_V(OVL2_OVL_FAKE_ENG0_DISP, "ovl2_ovl_fake_eng0_disp",
	        "ovl2_ovl_fake_eng0"/* parent */),
	GATE_OVL20(OVL2_OVL_FAKE_ENG1, "ovl2_ovl_fake_eng1",
			"mm_disp_ck"/* parent */, 26),
	GATE_OVL20_V(OVL2_OVL_FAKE_ENG1_DISP, "ovl2_ovl_fake_eng1_disp",
	        "ovl2_ovl_fake_eng1"/* parent */),
	GATE_OVL20(OVL2_OVL_FAKE_ENG2, "ovl2_ovl_fake_eng2",
			"mm_disp_ck"/* parent */, 27),
	GATE_OVL20_V(OVL2_OVL_FAKE_ENG2_DISP, "ovl2_ovl_fake_eng2_disp",
	        "ovl2_ovl_fake_eng2"/* parent */),
	GATE_OVL20(OVL2_OVL_FAKE_ENG3, "ovl2_ovl_fake_eng3",
			"mm_disp_ck"/* parent */, 28),
	GATE_OVL20_V(OVL2_OVL_FAKE_ENG3_DISP, "ovl2_ovl_fake_eng3_disp",
	        "ovl2_ovl_fake_eng3"/* parent */),
	GATE_OVL20(OVL2_OVL_DLI_ASYNC0, "ovl2_ovl_dli_as0",
			"mm_disp_ck"/* parent */, 29),
	GATE_OVL20_V(OVL2_OVL_DLI_ASYNC0_DISP, "ovl2_ovl_dli_as0_disp",
	        "ovl2_ovl_dli_as0"/* parent */),
	GATE_OVL20(OVL2_OVL_DLI_ASYNC1, "ovl2_ovl_dli_as1",
			"mm_disp_ck"/* parent */, 30),
	GATE_OVL20_V(OVL2_OVL_DLI_ASYNC1_DISP, "ovl2_ovl_dli_as1_disp",
	        "ovl2_ovl_dli_as1"/* parent */),
	GATE_OVL20(OVL2_OVL_DLI_ASYNC2, "ovl2_ovl_dli_as2",
			"mm_disp_ck"/* parent */, 31),
	GATE_OVL20_V(OVL2_OVL_DLI_ASYNC2_DISP, "ovl2_ovl_dli_as2_disp",
	        "ovl2_ovl_dli_as2"/* parent */),
	/* OVL21 */
	GATE_HWV_OVL21(OVL2_OVL_DLI_ASYNC3, "ovl2_ovl_dli_as3",
			"mm_disp_ck"/* parent */, 0),
	GATE_OVL21_V(OVL2_OVL_DLI_ASYNC3_DISP, "ovl2_ovl_dli_as3_disp",
	        "ovl2_ovl_dli_as3"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLI_ASYNC4, "ovl2_ovl_dli_as4",
			"mm_disp_ck"/* parent */, 1),
	GATE_OVL21_V(OVL2_OVL_DLI_ASYNC4_DISP, "ovl2_ovl_dli_as4_disp",
	        "ovl2_ovl_dli_as4"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLI_ASYNC5, "ovl2_ovl_dli_as5",
			"mm_disp_ck"/* parent */, 2),
	GATE_OVL21_V(OVL2_OVL_DLI_ASYNC5_DISP, "ovl2_ovl_dli_as5_disp",
	        "ovl2_ovl_dli_as5"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLI_ASYNC6, "ovl2_ovl_dli_as6",
			"mm_disp_ck"/* parent */, 3),
	GATE_OVL21_V(OVL2_OVL_DLI_ASYNC6_DISP, "ovl2_ovl_dli_as6_disp",
	        "ovl2_ovl_dli_as6"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLI_ASYNC7, "ovl2_ovl_dli_as7",
			"mm_disp_ck"/* parent */, 4),
	GATE_OVL21_V(OVL2_OVL_DLI_ASYNC7_DISP, "ovl2_ovl_dli_as7_disp",
	        "ovl2_ovl_dli_as7"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLI_ASYNC8, "ovl2_ovl_dli_as8",
			"mm_disp_ck"/* parent */, 5),
	GATE_OVL21_V(OVL2_OVL_DLI_ASYNC8_DISP, "ovl2_ovl_dli_as8_disp",
	        "ovl2_ovl_dli_as8"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLI_ASYNC9, "ovl2_ovl_dli_as9",
			"mm_disp_ck"/* parent */, 6),
	GATE_OVL21_V(OVL2_OVL_DLI_ASYNC9_DISP, "ovl2_ovl_dli_as9_disp",
	        "ovl2_ovl_dli_as9"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLI_ASYNC10, "ovl2_ovl_dli_as10",
			"mm_disp_ck"/* parent */, 7),
	GATE_OVL21_V(OVL2_OVL_DLI_ASYNC10_DISP, "ovl2_ovl_dli_as10_disp",
	        "ovl2_ovl_dli_as10"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLI_ASYNC11, "ovl2_ovl_dli_as11",
			"mm_disp_ck"/* parent */, 8),
	GATE_OVL21_V(OVL2_OVL_DLI_ASYNC11_DISP, "ovl2_ovl_dli_as11_disp",
	        "ovl2_ovl_dli_as11"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLI_ASYNC12, "ovl2_ovl_dli_as12",
			"mm_disp_ck"/* parent */, 9),
	GATE_OVL21_V(OVL2_OVL_DLI_ASYNC12_DISP, "ovl2_ovl_dli_as12_disp",
	        "ovl2_ovl_dli_as12"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLO_ASYNC0, "ovl2_ovl_dlo_as0",
			"mm_disp_ck"/* parent */, 10),
	GATE_OVL21_V(OVL2_OVL_DLO_ASYNC0_DISP, "ovl2_ovl_dlo_as0_disp",
	        "ovl2_ovl_dlo_as0"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLO_ASYNC1, "ovl2_ovl_dlo_as1",
			"mm_disp_ck"/* parent */, 11),
	GATE_OVL21_V(OVL2_OVL_DLO_ASYNC1_DISP, "ovl2_ovl_dlo_as1_disp",
	        "ovl2_ovl_dlo_as1"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLO_ASYNC2, "ovl2_ovl_dlo_as2",
			"mm_disp_ck"/* parent */, 12),
	GATE_OVL21_V(OVL2_OVL_DLO_ASYNC2_DISP, "ovl2_ovl_dlo_as2_disp",
	        "ovl2_ovl_dlo_as2"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLO_ASYNC3, "ovl2_ovl_dlo_as3",
			"mm_disp_ck"/* parent */, 13),
	GATE_OVL21_V(OVL2_OVL_DLO_ASYNC3_DISP, "ovl2_ovl_dlo_as3_disp",
	        "ovl2_ovl_dlo_as3"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLO_ASYNC4, "ovl2_ovl_dlo_as4",
			"mm_disp_ck"/* parent */, 14),
	GATE_OVL21_V(OVL2_OVL_DLO_ASYNC4_DISP, "ovl2_ovl_dlo_as4_disp",
	        "ovl2_ovl_dlo_as4"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLO_ASYNC5, "ovl2_ovl_dlo_as5",
			"mm_disp_ck"/* parent */, 15),
	GATE_OVL21_V(OVL2_OVL_DLO_ASYNC5_DISP, "ovl2_ovl_dlo_as5_disp",
	        "ovl2_ovl_dlo_as5"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLO_ASYNC6, "ovl2_ovl_dlo_as6",
			"mm_disp_ck"/* parent */, 16),
	GATE_OVL21_V(OVL2_OVL_DLO_ASYNC6_DISP, "ovl2_ovl_dlo_as6_disp",
	        "ovl2_ovl_dlo_as6"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLO_ASYNC7, "ovl2_ovl_dlo_as7",
			"mm_disp_ck"/* parent */, 17),
	GATE_OVL21_V(OVL2_OVL_DLO_ASYNC7_DISP, "ovl2_ovl_dlo_as7_disp",
	        "ovl2_ovl_dlo_as7"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLO_ASYNC8, "ovl2_ovl_dlo_as8",
			"mm_disp_ck"/* parent */, 18),
	GATE_OVL21_V(OVL2_OVL_DLO_ASYNC8_DISP, "ovl2_ovl_dlo_as8_disp",
	        "ovl2_ovl_dlo_as8"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLO_ASYNC9, "ovl2_ovl_dlo_as9",
			"mm_disp_ck"/* parent */, 19),
	GATE_OVL21_V(OVL2_OVL_DLO_ASYNC9_DISP, "ovl2_ovl_dlo_as9_disp",
	        "ovl2_ovl_dlo_as9"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLO_ASYNC10, "ovl2_ovl_dlo_as10",
			"mm_disp_ck"/* parent */, 20),
	GATE_OVL21_V(OVL2_OVL_DLO_ASYNC10_DISP, "ovl2_ovl_dlo_as10_disp",
	        "ovl2_ovl_dlo_as10"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLO_ASYNC11, "ovl2_ovl_dlo_as11",
			"mm_disp_ck"/* parent */, 21),
	GATE_OVL21_V(OVL2_OVL_DLO_ASYNC11_DISP, "ovl2_ovl_dlo_as11_disp",
	        "ovl2_ovl_dlo_as11"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLO_ASYNC12, "ovl2_ovl_dlo_as12",
			"mm_disp_ck"/* parent */, 22),
	GATE_OVL21_V(OVL2_OVL_DLO_ASYNC12_DISP, "ovl2_ovl_dlo_as12_disp",
	        "ovl2_ovl_dlo_as12"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLO_ASYNC13, "ovl2_ovl_dlo_as13",
			"mm_disp_ck"/* parent */, 23),
	GATE_OVL21_V(OVL2_OVL_DLO_ASYNC13_DISP, "ovl2_ovl_dlo_as13_disp",
	        "ovl2_ovl_dlo_as13"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLO_ASYNC14, "ovl2_ovl_dlo_as14",
			"mm_disp_ck"/* parent */, 24),
	GATE_OVL21_V(OVL2_OVL_DLO_ASYNC14_DISP, "ovl2_ovl_dlo_as14_disp",
	        "ovl2_ovl_dlo_as14"/* parent */),
	GATE_HWV_OVL21(OVL2_OVL_DLO_ASYNC15, "ovl2_ovl_dlo_as15",
			"mm_disp_ck"/* parent */, 25),
	GATE_OVL21_V(OVL2_OVL_DLO_ASYNC15_DISP, "ovl2_ovl_dlo_as15_disp",
	        "ovl2_ovl_dlo_as15"/* parent */),
	GATE_HWV_OVL21(OVL2_OVLSYS_RELAY0, "ovl2_ovlsys_relay0",
			"mm_disp_ck"/* parent */, 26),
	GATE_OVL21_V(OVL2_OVLSYS_RELAY0_DISP, "ovl2_ovlsys_relay0_disp",
	        "ovl2_ovlsys_relay0"/* parent */),
	GATE_HWV_OVL21(OVL2_SMI_SUB_COMM0, "ovl2_ssc",
			"mm_disp_ck"/* parent */, 27),
	GATE_OVL21_V(OVL2_SMI_SUB_COMM0_DISP, "ovl2_ssc_disp",
	        "ovl2_ssc"/* parent */),
	GATE_OVL21_V(OVL2_SMI_SUB_COMM0_SMI, "ovl2_ssc_smi",
	        "ovl2_ssc"/* parent */),
	GATE_HWV_OVL21(OVL2_DISP_DBG, "ovl2_disp_dbg",
			"mm_disp_ck"/* parent */, 28),
	GATE_OVL21_V(OVL2_DISP_DBG_DISP, "ovl2_disp_dbg_disp",
	        "ovl2_disp_dbg"/* parent */),
};

static const struct mtk_clk_desc ovl2_mcd = {
	.clks = ovl2_clks,
	.num_clks = CLK_OVL2_NR_CLK,
};

static const struct mtk_gate_regs ovl0_cg_regs = {
	.set_ofs = 0xACC,
	.clr_ofs = 0xAD0,
	.sta_ofs = 0xAC8,
};

static const struct mtk_gate_regs ovl1_cg_regs = {
	.set_ofs = 0xAD8,
	.clr_ofs = 0xADC,
	.sta_ofs = 0xAD4,
};

static const struct mtk_gate_regs ovl1_hwv_regs = {
	.set_ofs = 0xb4,
	.clr_ofs = 0xb8,
	.sta_ofs = 0x1263C,
};

#define GATE_OVL0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ovl0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
		.flags = RES_FRAMEWORK_VDISP,		\
	}

#define GATE_OVL0_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_OVL1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ovl1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr,	\
	}

#define GATE_OVL1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_HWV_OVL1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ovl1_cg_regs,			\
		.hwv_regs = &ovl1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr,			\
		.flags = RES_FRAMEWORK_VDISP | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate ovl_clks[] = {
	/* OVL0 */
	GATE_OVL0(OVLSYS_CONFIG, "ovlsys_config",
			"mm_disp_ck"/* parent */, 0),
	GATE_OVL0_V(OVLSYS_CONFIG_DISP, "ovlsys_config_disp",
	        "ovlsys_config"/* parent */),
	GATE_OVL0(OVL_MUTEX0, "ovl_mutex0",
			"mm_disp_ck"/* parent */, 1),
	GATE_OVL0_V(OVL_MUTEX0_DISP, "ovl_mutex0_disp",
	        "ovl_mutex0"/* parent */),
	GATE_OVL0(OVL_EXDMA0, "ovl_exdma0",
			"mm_disp_ck"/* parent */, 2),
	GATE_OVL0_V(OVL_EXDMA0_DISP, "ovl_exdma0_disp",
	        "ovl_exdma0"/* parent */),
	GATE_OVL0(OVL_EXDMA1, "ovl_exdma1",
			"mm_disp_ck"/* parent */, 3),
	GATE_OVL0_V(OVL_EXDMA1_DISP, "ovl_exdma1_disp",
	        "ovl_exdma1"/* parent */),
	GATE_OVL0(OVL_EXDMA2, "ovl_exdma2",
			"mm_disp_ck"/* parent */, 4),
	GATE_OVL0_V(OVL_EXDMA2_DISP, "ovl_exdma2_disp",
	        "ovl_exdma2"/* parent */),
	GATE_OVL0(OVL_EXDMA3, "ovl_exdma3",
			"mm_disp_ck"/* parent */, 5),
	GATE_OVL0_V(OVL_EXDMA3_DISP, "ovl_exdma3_disp",
	        "ovl_exdma3"/* parent */),
	GATE_OVL0(OVL_EXDMA4, "ovl_exdma4",
			"mm_disp_ck"/* parent */, 6),
	GATE_OVL0_V(OVL_EXDMA4_DISP, "ovl_exdma4_disp",
	        "ovl_exdma4"/* parent */),
	GATE_OVL0(OVL_EXDMA5, "ovl_exdma5",
			"mm_disp_ck"/* parent */, 7),
	GATE_OVL0_V(OVL_EXDMA5_DISP, "ovl_exdma5_disp",
	        "ovl_exdma5"/* parent */),
	GATE_OVL0(OVL_EXDMA6, "ovl_exdma6",
			"mm_disp_ck"/* parent */, 8),
	GATE_OVL0_V(OVL_EXDMA6_DISP, "ovl_exdma6_disp",
	        "ovl_exdma6"/* parent */),
	GATE_OVL0(OVL_EXDMA7, "ovl_exdma7",
			"mm_disp_ck"/* parent */, 9),
	GATE_OVL0_V(OVL_EXDMA7_DISP, "ovl_exdma7_disp",
	        "ovl_exdma7"/* parent */),
	GATE_OVL0(OVL_BLENDER0, "ovl_blender0",
			"mm_disp_ck"/* parent */, 10),
	GATE_OVL0_V(OVL_BLENDER0_DISP, "ovl_blender0_disp",
	        "ovl_blender0"/* parent */),
	GATE_OVL0(OVL_BLENDER1, "ovl_blender1",
			"mm_disp_ck"/* parent */, 11),
	GATE_OVL0_V(OVL_BLENDER1_DISP, "ovl_blender1_disp",
	        "ovl_blender1"/* parent */),
	GATE_OVL0(OVL_BLENDER2, "ovl_blender2",
			"mm_disp_ck"/* parent */, 12),
	GATE_OVL0_V(OVL_BLENDER2_DISP, "ovl_blender2_disp",
	        "ovl_blender2"/* parent */),
	GATE_OVL0(OVL_BLENDER3, "ovl_blender3",
			"mm_disp_ck"/* parent */, 13),
	GATE_OVL0_V(OVL_BLENDER3_DISP, "ovl_blender3_disp",
	        "ovl_blender3"/* parent */),
	GATE_OVL0(OVL_BLENDER4, "ovl_blender4",
			"mm_disp_ck"/* parent */, 14),
	GATE_OVL0_V(OVL_BLENDER4_DISP, "ovl_blender4_disp",
	        "ovl_blender4"/* parent */),
	GATE_OVL0(OVL_BLENDER5, "ovl_blender5",
			"mm_disp_ck"/* parent */, 15),
	GATE_OVL0_V(OVL_BLENDER5_DISP, "ovl_blender5_disp",
	        "ovl_blender5"/* parent */),
	GATE_OVL0(OVL_BLENDER6, "ovl_blender6",
			"mm_disp_ck"/* parent */, 16),
	GATE_OVL0_V(OVL_BLENDER6_DISP, "ovl_blender6_disp",
	        "ovl_blender6"/* parent */),
	GATE_OVL0(OVL_BLENDER7, "ovl_blender7",
			"mm_disp_ck"/* parent */, 17),
	GATE_OVL0_V(OVL_BLENDER7_DISP, "ovl_blender7_disp",
	        "ovl_blender7"/* parent */),
	GATE_OVL0(OVL_OUTPROC0, "ovl_outproc0",
			"mm_disp_ck"/* parent */, 18),
	GATE_OVL0_V(OVL_OUTPROC0_DISP, "ovl_outproc0_disp",
	        "ovl_outproc0"/* parent */),
	GATE_OVL0(OVL_OUTPROC1, "ovl_outproc1",
			"mm_disp_ck"/* parent */, 19),
	GATE_OVL0_V(OVL_OUTPROC1_DISP, "ovl_outproc1_disp",
	        "ovl_outproc1"/* parent */),
	GATE_OVL0(OVL_OUTPROC2, "ovl_outproc2",
			"mm_disp_ck"/* parent */, 20),
	GATE_OVL0_V(OVL_OUTPROC2_DISP, "ovl_outproc2_disp",
	        "ovl_outproc2"/* parent */),
	GATE_OVL0(OVL_OUTPROC3, "ovl_outproc3",
			"mm_disp_ck"/* parent */, 21),
	GATE_OVL0_V(OVL_OUTPROC3_DISP, "ovl_outproc3_disp",
	        "ovl_outproc3"/* parent */),
	GATE_OVL0(OVL_MDP_RSZ0, "ovl_mdp_rsz0",
			"mm_disp_ck"/* parent */, 22),
	GATE_OVL0_V(OVL_MDP_RSZ0_DISP, "ovl_mdp_rsz0_disp",
	        "ovl_mdp_rsz0"/* parent */),
	GATE_OVL0(OVL_ODPM0, "ovl_odpm0",
			"mm_disp_ck"/* parent */, 23),
	GATE_OVL0_V(OVL_ODPM0_DISP, "ovl_odpm0_disp",
	        "ovl_odpm0"/* parent */),
	GATE_OVL0(OVL_INLINEROT0, "ovl_inlinerot0",
			"mm_disp_ck"/* parent */, 24),
	GATE_OVL0_V(OVL_INLINEROT0_DISP, "ovl_inlinerot0_disp",
	        "ovl_inlinerot0"/* parent */),
	GATE_OVL0(OVL_FAKE_ENG0, "ovl_fake_eng0",
			"mm_disp_ck"/* parent */, 25),
	GATE_OVL0_V(OVL_FAKE_ENG0_DISP, "ovl_fake_eng0_disp",
	        "ovl_fake_eng0"/* parent */),
	GATE_OVL0(OVL_FAKE_ENG1, "ovl_fake_eng1",
			"mm_disp_ck"/* parent */, 26),
	GATE_OVL0_V(OVL_FAKE_ENG1_DISP, "ovl_fake_eng1_disp",
	        "ovl_fake_eng1"/* parent */),
	GATE_OVL0(OVL_FAKE_ENG2, "ovl_fake_eng2",
			"mm_disp_ck"/* parent */, 27),
	GATE_OVL0_V(OVL_FAKE_ENG2_DISP, "ovl_fake_eng2_disp",
	        "ovl_fake_eng2"/* parent */),
	GATE_OVL0(OVL_FAKE_ENG3, "ovl_fake_eng3",
			"mm_disp_ck"/* parent */, 28),
	GATE_OVL0_V(OVL_FAKE_ENG3_DISP, "ovl_fake_eng3_disp",
	        "ovl_fake_eng3"/* parent */),
	GATE_OVL0(OVL_DLI_ASYNC0, "ovl_dli_as0",
			"mm_disp_ck"/* parent */, 29),
	GATE_OVL0_V(OVL_DLI_ASYNC0_DISP, "ovl_dli_as0_disp",
	        "ovl_dli_as0"/* parent */),
	GATE_OVL0(OVL_DLI_ASYNC1, "ovl_dli_as1",
			"mm_disp_ck"/* parent */, 30),
	GATE_OVL0_V(OVL_DLI_ASYNC1_DISP, "ovl_dli_as1_disp",
	        "ovl_dli_as1"/* parent */),
	GATE_OVL0(OVL_DLI_ASYNC2, "ovl_dli_as2",
			"mm_disp_ck"/* parent */, 31),
	GATE_OVL0_V(OVL_DLI_ASYNC2_DISP, "ovl_dli_as2_disp",
	        "ovl_dli_as2"/* parent */),
	/* OVL1 */
	GATE_HWV_OVL1(OVL_DLI_ASYNC3, "ovl_dli_as3",
			"mm_disp_ck"/* parent */, 0),
	GATE_OVL1_V(OVL_DLI_ASYNC3_DISP, "ovl_dli_as3_disp",
	        "ovl_dli_as3"/* parent */),
	GATE_HWV_OVL1(OVL_DLI_ASYNC4, "ovl_dli_as4",
			"mm_disp_ck"/* parent */, 1),
	GATE_OVL1_V(OVL_DLI_ASYNC4_DISP, "ovl_dli_as4_disp",
	        "ovl_dli_as4"/* parent */),
	GATE_HWV_OVL1(OVL_DLI_ASYNC5, "ovl_dli_as5",
			"mm_disp_ck"/* parent */, 2),
	GATE_OVL1_V(OVL_DLI_ASYNC5_DISP, "ovl_dli_as5_disp",
	        "ovl_dli_as5"/* parent */),
	GATE_HWV_OVL1(OVL_DLI_ASYNC6, "ovl_dli_as6",
			"mm_disp_ck"/* parent */, 3),
	GATE_OVL1_V(OVL_DLI_ASYNC6_DISP, "ovl_dli_as6_disp",
	        "ovl_dli_as6"/* parent */),
	GATE_HWV_OVL1(OVL_DLI_ASYNC7, "ovl_dli_as7",
			"mm_disp_ck"/* parent */, 4),
	GATE_OVL1_V(OVL_DLI_ASYNC7_DISP, "ovl_dli_as7_disp",
	        "ovl_dli_as7"/* parent */),
	GATE_HWV_OVL1(OVL_DLI_ASYNC8, "ovl_dli_as8",
			"mm_disp_ck"/* parent */, 5),
	GATE_OVL1_V(OVL_DLI_ASYNC8_DISP, "ovl_dli_as8_disp",
	        "ovl_dli_as8"/* parent */),
	GATE_HWV_OVL1(OVL_DLI_ASYNC9, "ovl_dli_as9",
			"mm_disp_ck"/* parent */, 6),
	GATE_OVL1_V(OVL_DLI_ASYNC9_DISP, "ovl_dli_as9_disp",
	        "ovl_dli_as9"/* parent */),
	GATE_HWV_OVL1(OVL_DLI_ASYNC10, "ovl_dli_as10",
			"mm_disp_ck"/* parent */, 7),
	GATE_OVL1_V(OVL_DLI_ASYNC10_DISP, "ovl_dli_as10_disp",
	        "ovl_dli_as10"/* parent */),
	GATE_HWV_OVL1(OVL_DLI_ASYNC11, "ovl_dli_as11",
			"mm_disp_ck"/* parent */, 8),
	GATE_OVL1_V(OVL_DLI_ASYNC11_DISP, "ovl_dli_as11_disp",
	        "ovl_dli_as11"/* parent */),
	GATE_HWV_OVL1(OVL_DLI_ASYNC12, "ovl_dli_as12",
			"mm_disp_ck"/* parent */, 9),
	GATE_OVL1_V(OVL_DLI_ASYNC12_DISP, "ovl_dli_as12_disp",
	        "ovl_dli_as12"/* parent */),
	GATE_HWV_OVL1(OVL_DLO_ASYNC0, "ovl_dlo_as0",
			"mm_disp_ck"/* parent */, 10),
	GATE_OVL1_V(OVL_DLO_ASYNC0_DISP, "ovl_dlo_as0_disp",
	        "ovl_dlo_as0"/* parent */),
	GATE_HWV_OVL1(OVL_DLO_ASYNC1, "ovl_dlo_as1",
			"mm_disp_ck"/* parent */, 11),
	GATE_OVL1_V(OVL_DLO_ASYNC1_DISP, "ovl_dlo_as1_disp",
	        "ovl_dlo_as1"/* parent */),
	GATE_HWV_OVL1(OVL_DLO_ASYNC2, "ovl_dlo_as2",
			"mm_disp_ck"/* parent */, 12),
	GATE_OVL1_V(OVL_DLO_ASYNC2_DISP, "ovl_dlo_as2_disp",
	        "ovl_dlo_as2"/* parent */),
	GATE_HWV_OVL1(OVL_DLO_ASYNC3, "ovl_dlo_as3",
			"mm_disp_ck"/* parent */, 13),
	GATE_OVL1_V(OVL_DLO_ASYNC3_DISP, "ovl_dlo_as3_disp",
	        "ovl_dlo_as3"/* parent */),
	GATE_HWV_OVL1(OVL_DLO_ASYNC4, "ovl_dlo_as4",
			"mm_disp_ck"/* parent */, 14),
	GATE_OVL1_V(OVL_DLO_ASYNC4_DISP, "ovl_dlo_as4_disp",
	        "ovl_dlo_as4"/* parent */),
	GATE_HWV_OVL1(OVL_DLO_ASYNC5, "ovl_dlo_as5",
			"mm_disp_ck"/* parent */, 15),
	GATE_OVL1_V(OVL_DLO_ASYNC5_DISP, "ovl_dlo_as5_disp",
	        "ovl_dlo_as5"/* parent */),
	GATE_HWV_OVL1(OVL_DLO_ASYNC6, "ovl_dlo_as6",
			"mm_disp_ck"/* parent */, 16),
	GATE_OVL1_V(OVL_DLO_ASYNC6_DISP, "ovl_dlo_as6_disp",
	        "ovl_dlo_as6"/* parent */),
	GATE_HWV_OVL1(OVL_DLO_ASYNC7, "ovl_dlo_as7",
			"mm_disp_ck"/* parent */, 17),
	GATE_OVL1_V(OVL_DLO_ASYNC7_DISP, "ovl_dlo_as7_disp",
	        "ovl_dlo_as7"/* parent */),
	GATE_HWV_OVL1(OVL_DLO_ASYNC8, "ovl_dlo_as8",
			"mm_disp_ck"/* parent */, 18),
	GATE_OVL1_V(OVL_DLO_ASYNC8_DISP, "ovl_dlo_as8_disp",
	        "ovl_dlo_as8"/* parent */),
	GATE_HWV_OVL1(OVL_DLO_ASYNC9, "ovl_dlo_as9",
			"mm_disp_ck"/* parent */, 19),
	GATE_OVL1_V(OVL_DLO_ASYNC9_DISP, "ovl_dlo_as9_disp",
	        "ovl_dlo_as9"/* parent */),
	GATE_HWV_OVL1(OVL_DLO_ASYNC10, "ovl_dlo_as10",
			"mm_disp_ck"/* parent */, 20),
	GATE_OVL1_V(OVL_DLO_ASYNC10_DISP, "ovl_dlo_as10_disp",
	        "ovl_dlo_as10"/* parent */),
	GATE_HWV_OVL1(OVL_DLO_ASYNC11, "ovl_dlo_as11",
			"mm_disp_ck"/* parent */, 21),
	GATE_OVL1_V(OVL_DLO_ASYNC11_DISP, "ovl_dlo_as11_disp",
	        "ovl_dlo_as11"/* parent */),
	GATE_HWV_OVL1(OVL_DLO_ASYNC12, "ovl_dlo_as12",
			"mm_disp_ck"/* parent */, 22),
	GATE_OVL1_V(OVL_DLO_ASYNC12_DISP, "ovl_dlo_as12_disp",
	        "ovl_dlo_as12"/* parent */),
	GATE_HWV_OVL1(OVL_DLO_ASYNC13, "ovl_dlo_as13",
			"mm_disp_ck"/* parent */, 23),
	GATE_OVL1_V(OVL_DLO_ASYNC13_DISP, "ovl_dlo_as13_disp",
	        "ovl_dlo_as13"/* parent */),
	GATE_HWV_OVL1(OVL_DLO_ASYNC14, "ovl_dlo_as14",
			"mm_disp_ck"/* parent */, 24),
	GATE_OVL1_V(OVL_DLO_ASYNC14_DISP, "ovl_dlo_as14_disp",
	        "ovl_dlo_as14"/* parent */),
	GATE_HWV_OVL1(OVL_DLO_ASYNC15, "ovl_dlo_as15",
			"mm_disp_ck"/* parent */, 25),
	GATE_OVL1_V(OVL_DLO_ASYNC15_DISP, "ovl_dlo_as15_disp",
	        "ovl_dlo_as15"/* parent */),
	GATE_HWV_OVL1(OVLSYS_RELAY0, "ovlsys_relay0",
			"mm_disp_ck"/* parent */, 26),
	GATE_OVL1_V(OVLSYS_RELAY0_DISP, "ovlsys_relay0_disp",
	        "ovlsys_relay0"/* parent */),
	GATE_HWV_OVL1(OVL_SMI_SUB_COMM0, "ovl_ssc",
			"mm_disp_ck"/* parent */, 27),
	GATE_OVL1_V(OVL_SMI_SUB_COMM0_DISP, "ovl_ssc_disp",
	        "ovl_ssc"/* parent */),
	GATE_OVL1_V(OVL_SMI_SUB_COMM0_SMI, "ovl_ssc_smi",
	        "ovl_ssc"/* parent */),
	GATE_HWV_OVL1(OVL_DISP_DBG, "ovl_disp_dbg",
			"mm_disp_ck"/* parent */, 28),
	GATE_OVL1_V(OVL_DISP_DBG_DISP, "ovl_disp_dbg_disp",
	        "ovl_disp_dbg"/* parent */),
};

static const struct mtk_clk_desc ovl_mcd = {
	.clks = ovl_clks,
	.num_clks = CLK_OVL_NR_CLK,
};

static const struct mtk_gate_regs vdisp_rc_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

#define GATE_VDISP_RC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vdisp_rc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_VDISP_RC_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

static const struct mtk_gate vdisp_rc_clks[] = {
	GATE_VDISP_RC(VDISP_RC_DVFSRC_EN, "vdisp_rc_dvfsrc_en",
			"cksys_vlp_f26m_ck"/* parent */, 0),
	GATE_VDISP_RC_V(VDISP_RC_DVFSRC_EN_DISP, "vdisp_rc_dvfsrc_en_disp",
	        "vdisp_rc_dvfsrc_en"/* parent */),
};

static const struct mtk_clk_desc vdisp_rc_mcd = {
	.clks = vdisp_rc_clks,
	.num_clks = CLK_VDISP_RC_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6993_mmsys[] = {
	{
		.compatible = "mediatek,mt6993-disp0b_dispsys_config",
		.data = &mm0b_mcd,
	}, {
		.compatible = "mediatek,mt6993-disp1b_dispsys1_config",
		.data = &mm1b_mcd,
	}, {
		.compatible = "mediatek,mt6993-mmsys1",
		.data = &mm1_mcd,
	}, {
		.compatible = "mediatek,mt6993-mmsys0",
		.data = &mm0_mcd,
	}, {
		.compatible = "mediatek,mt6993-disp_vdisp_ao_config",
		.data = &vdisp_ao_mcd,
	}, {
		.compatible = "mediatek,mt6993-mml1_mmlsys_config",
		.data = &mml1_mcd,
	}, {
		.compatible = "mediatek,mt6993-mml2_mmlsys_config",
		.data = &mml2_mcd,
	}, {
		.compatible = "mediatek,mt6993-mmlsys_config",
		.data = &mml_mcd,
	}, {
		.compatible = "mediatek,mt6993-ovl1_ovlsys_config",
		.data = &ovl1_mcd,
	}, {
		.compatible = "mediatek,mt6993-ovl2_ovlsys_config",
		.data = &ovl2_mcd,
	}, {
		.compatible = "mediatek,mt6993-ovlsys_config",
		.data = &ovl_mcd,
	}, {
		.compatible = "mediatek,mt6993-vdisp_dvfsrc_apb",
		.data = &vdisp_rc_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6993_mmsys_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6993_mmsys_drv = {
	.probe = clk_mt6993_mmsys_grp_probe,
	.driver = {
		.name = "clk-mt6993-mmsys",
		.of_match_table = of_match_clk_mt6993_mmsys,
	},
};

module_platform_driver(clk_mt6993_mmsys_drv);
MODULE_LICENSE("GPL");
