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

static const struct mtk_gate_regs vde2_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vde20_hwv_regs = {
	.set_ofs = 0x18,
	.clr_ofs = 0x1c,
	.sta_ofs = 0x12608,
};

#define GATE_VDE2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE2_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_VDE20(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &vde2_cg_regs,			\
		.hwv_regs = &vde20_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate vde2_clks[] = {
	GATE_HWV_VDE20(VDE2_VDEC_CKEN, "vde2_vdec_cken",
			"mm_vdec_ck"/* parent */, 0),
	GATE_VDE2_V(VDE2_VDEC_CKEN_V, "vde2_vdec_cken_v",
	        "vde2_vdec_cken"/* parent */),
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
	.set_ofs = 0xc,
	.clr_ofs = 0x10,
	.sta_ofs = 0x12604,
};

static const struct mtk_gate_regs vde11_cg_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x204,
	.sta_ofs = 0x200,
};

static const struct mtk_gate_regs vde11_hwv_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x12600,
};

#define GATE_VDE10(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE10_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_VDE10(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &vde10_cg_regs,			\
		.hwv_regs = &vde10_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

#define GATE_VDE11(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde11_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE11_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
		.flags = RES_FRAMEWORK_VMM,		\
    }

#define GATE_HWV_VDE11(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &vde11_cg_regs,			\
		.hwv_regs = &vde11_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate vde1_clks[] = {
	/* VDE10 */
	GATE_HWV_VDE10(VDE1_VDEC_CKEN, "vde1_vdec_cken",
			"mm_vdec_ck"/* parent */, 0),
	GATE_VDE10_V(VDE1_VDEC_CKEN_V, "vde1_vdec_cken_v",
	        "vde1_vdec_cken"/* parent */),
	/* VDE11 */
	GATE_HWV_VDE11(VDE1_LAT_CKEN, "vde1_lat_cken",
			"mm_vdec_ck"/* parent */, 0),
	GATE_VDE11_V(VDE1_LAT_CKEN_V, "vde1_lat_cken_v",
	        "vde1_lat_cken"/* parent */),
};

static const struct mtk_clk_desc vde1_mcd = {
	.clks = vde1_clks,
	.num_clks = CLK_VDE1_NR_CLK,
};

static const struct mtk_gate_regs ven10_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs ven10_hwv_regs = {
	.set_ofs = 0x24,
	.clr_ofs = 0x28,
	.sta_ofs = 0x1260C,
};

static const struct mtk_gate_regs ven11_cg_regs = {
	.set_ofs = 0x10,
	.clr_ofs = 0x14,
	.sta_ofs = 0x10,
};

static const struct mtk_gate_regs ven11_hwv_regs = {
	.set_ofs = 0x30,
	.clr_ofs = 0x34,
	.sta_ofs = 0x12610,
};

#define GATE_VEN10(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ven10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VEN10_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_HWV_VEN10(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ven10_cg_regs,			\
		.hwv_regs = &ven10_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

#define GATE_VEN11(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ven11_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VEN11_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_HWV_VEN11(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ven11_cg_regs,					\
		.hwv_regs = &ven11_hwv_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops_inv,		\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,		\
		.flags = RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER| BYPASS_CHECK,		\
	}

static const struct mtk_gate ven1_clks[] = {
	/* VEN10 */
	GATE_HWV_VEN10(VEN1_CKE0_LARB, "ven1_larb",
			"mm_venc_ck"/* parent */, 0),
	GATE_VEN10_V(VEN1_CKE0_LARB_V, "ven1_larb_v",
	        "ven1_larb"/* parent */),
	GATE_HWV_VEN10(VEN1_CKE1_VENC, "ven1_venc",
			"mm_venc_ck"/* parent */, 4),
	GATE_VEN10_V(VEN1_CKE1_VENC_V, "ven1_venc_v",
	        "ven1_venc"/* parent */),
	/* VEN11 */
	GATE_HWV_VEN11(VEN1_VENCSYS_RESOURCE_SET, "ven1_vencsys_resource_set",
			"mm_venc_ck"/* parent */, 0),
	GATE_VEN11_V(VEN1_VENCSYS_RESOURCE_SET_V, "ven1_vencsys_resource_set_v",
	        "ven1_vencsys_resource_set"/* parent */),
};

static const struct mtk_clk_desc ven1_mcd = {
	.clks = ven1_clks,
	.num_clks = CLK_VEN1_NR_CLK,
};

static const struct mtk_gate_regs ven_c0_p1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs ven_c0_p10_hwv_regs = {
	.set_ofs = 0x3c,
	.clr_ofs = 0x40,
	.sta_ofs = 0x12614,
};

#define GATE_VEN_C0_P1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ven_c0_p1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VEN_C0_P1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_HWV_VEN_C0_P10(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ven_c0_p1_cg_regs,			\
		.hwv_regs = &ven_c0_p10_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate ven_c0_p1_clks[] = {
	GATE_HWV_VEN_C0_P10(VEN_C0_P1_CKE0_LARB, "ven_c0_p1_larb",
			"mm_venc_ck"/* parent */, 0),
	GATE_VEN_C0_P1_V(VEN_C0_P1_CKE0_LARB_V, "ven_c0_p1_larb_v",
	        "ven_c0_p1_larb"/* parent */),
	GATE_HWV_VEN_C0_P10(VEN_C0_P1_CKE1_VENC, "ven_c0_p1_venc",
			"mm_venc_ck"/* parent */, 4),
	GATE_VEN_C0_P1_V(VEN_C0_P1_CKE1_VENC_V, "ven_c0_p1_venc_v",
	        "ven_c0_p1_venc"/* parent */),
};

static const struct mtk_clk_desc ven_c0_p1_mcd = {
	.clks = ven_c0_p1_clks,
	.num_clks = CLK_VEN_C0_P1_NR_CLK,
};

static const struct mtk_gate_regs ven2_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs ven20_hwv_regs = {
	.set_ofs = 0x48,
	.clr_ofs = 0x4c,
	.sta_ofs = 0x12618,
};

#define GATE_VEN2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ven2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VEN2_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_HWV_VEN20(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ven2_cg_regs,			\
		.hwv_regs = &ven20_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate ven2_clks[] = {
	GATE_HWV_VEN20(VEN2_CKE0_LARB, "ven2_larb",
			"mm_venc_ck"/* parent */, 0),
	GATE_VEN2_V(VEN2_CKE0_LARB_V, "ven2_larb_v",
	        "ven2_larb"/* parent */),
	GATE_HWV_VEN20(VEN2_CKE1_VENC, "ven2_venc",
			"mm_venc_ck"/* parent */, 4),
	GATE_VEN2_V(VEN2_CKE1_VENC_V, "ven2_venc_v",
	        "ven2_venc"/* parent */),
};

static const struct mtk_clk_desc ven2_mcd = {
	.clks = ven2_clks,
	.num_clks = CLK_VEN2_NR_CLK,
};

static const struct mtk_gate_regs ven_c1_p1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs ven_c1_p10_hwv_regs = {
	.set_ofs = 0x54,
	.clr_ofs = 0x58,
	.sta_ofs = 0x1261C,
};

#define GATE_VEN_C1_P1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ven_c1_p1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VEN_C1_P1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_HWV_VEN_C1_P10(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ven_c1_p1_cg_regs,			\
		.hwv_regs = &ven_c1_p10_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops_inv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr_inv,			\
		.flags = RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER,	\
	}

static const struct mtk_gate ven_c1_p1_clks[] = {
	GATE_HWV_VEN_C1_P10(VEN_C1_P1_CKE0_LARB, "ven_c1_p1_larb",
			"mm_venc_ck"/* parent */, 0),
	GATE_VEN_C1_P1_V(VEN_C1_P1_CKE0_LARB_V, "ven_c1_p1_larb_v",
	        "ven_c1_p1_larb"/* parent */),
	GATE_HWV_VEN_C1_P10(VEN_C1_P1_CKE1_VENC, "ven_c1_p1_venc",
			"mm_venc_ck"/* parent */, 4),
	GATE_VEN_C1_P1_V(VEN_C1_P1_CKE1_VENC_V, "ven_c1_p1_venc_v",
	        "ven_c1_p1_venc"/* parent */),
};

static const struct mtk_clk_desc ven_c1_p1_mcd = {
	.clks = ven_c1_p1_clks,
	.num_clks = CLK_VEN_C1_P1_NR_CLK,
};

static const struct mtk_gate_regs ven_mdp_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs ven_mdp0_hwv_regs = {
	.set_ofs = 0x60,
	.clr_ofs = 0x64,
	.sta_ofs = 0x12620,
};

#define GATE_VEN_MDP(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ven_mdp_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_mm_gate_ops_setclr_inv,	\
	}

#define GATE_VEN_MDP_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }
/* BYPASS_CHECK: MDP with unexpect desgin, DMA back flow can not be used */
#define GATE_HWV_VEN_MDP0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &ven_mdp_cg_regs,			\
		.hwv_regs = &ven_mdp0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops_inv,				\
		.dma_ops = &mtk_clk_mm_gate_ops_setclr_inv,			\
		.flags = RES_FRAMEWORK_MMINFRA | CLK_USE_HW_VOTER | BYPASS_CHECK,	\
	}

static const struct mtk_gate ven_mdp_clks[] = {
	GATE_HWV_VEN_MDP0(VEN_MDP_CKE0_LARB, "ven_mdp_larb",
			"mm_venc_mdp_ck"/* parent */, 0),
	GATE_VEN_MDP_V(VEN_MDP_CKE0_LARB_CKE0_LARB_JPGENC, "ven_mdp_larb_cke0_larb_jpgenc",
	        "ven_mdp_larb"/* parent */),
	GATE_VEN_MDP_V(VEN_MDP_CKE0_LARB_CKE0_LARB_JPGDEC, "ven_mdp_larb_cke0_larb_jpgdec",
	        "ven_mdp_larb"/* parent */),
	GATE_VEN_MDP_V(VEN_MDP_CKE0_LARB_SMI, "ven_mdp_larb_smi",
	        "ven_mdp_larb"/* parent */),
	GATE_HWV_VEN_MDP0(VEN_MDP_CKE2_JPGENC, "ven_mdp_jpgenc",
			"mm_venc_mdp_ck"/* parent */, 2),
	GATE_VEN_MDP_V(VEN_MDP_CKE2_JPGENC_CKE2_JPGENC, "ven_mdp_jpgenc_cke2_jpgenc",
	        "ven_mdp_jpgenc"/* parent */),
	GATE_HWV_VEN_MDP0(VEN_MDP_CKE3_JPGENC_C1, "ven_mdp_jpgenc_c1",
			"mm_venc_mdp_ck"/* parent */, 3),
	GATE_VEN_MDP_V(VEN_MDP_CKE3_JPGENC_C1_CKE3_JPGENC_C1, "ven_mdp_jpgenc_c1_cke3_jpgenc_c1",
	        "ven_mdp_jpgenc_c1"/* parent */),
	GATE_HWV_VEN_MDP0(VEN_MDP_CKE4_JPGDEC, "ven_mdp_jpgdec",
			"mm_venc_mdp_ck"/* parent */, 4),
	GATE_VEN_MDP_V(VEN_MDP_CKE4_JPGDEC_CKE4_JPGDEC_JPGDEC, "ven_mdp_jpgdec_cke4_jpgdec_jpgdec",
	        "ven_mdp_jpgdec"/* parent */),
	GATE_HWV_VEN_MDP0(VEN_MDP_CKE5_JPGDEC_C1, "ven_mdp_jpgdec_c1",
			"mm_venc_mdp_ck"/* parent */, 5),
	GATE_VEN_MDP_V(VEN_MDP_CKE5_JPGDEC_C1_CKE5_JPGDEC_C1_JPGDEC, "ven_mdp_jpgdec_c1_cke5_jpgdec_c1_jpgdec",
	        "ven_mdp_jpgdec_c1"/* parent */),
	GATE_HWV_VEN_MDP0(VEN_MDP_CKE6_JPGDEC_C2, "ven_mdp_jpgdec_c2",
			"mm_venc_mdp_ck"/* parent */, 6),
	GATE_VEN_MDP_V(VEN_MDP_CKE6_JPGDEC_C2_CKE6_JPGDEC_C2_JPGDEC, "ven_mdp_jpgdec_c2_cke6_jpgdec_c2_jpgdec",
	        "ven_mdp_jpgdec_c2"/* parent */),
	GATE_HWV_VEN_MDP0(VEN_MDP_CKE7_VENC_ADAB_CTRL, "ven_mdp_venc_adab_ctrl",
			"mm_venc_mdp_ck"/* parent */, 7),
	GATE_VEN_MDP_V(VEN_MDP_CKE7_VENC_ADAB_CTRL_V, "ven_mdp_venc_adab_ctrl_v",
	        "ven_mdp_venc_adab_ctrl"/* parent */),
};

static const struct mtk_clk_desc ven_mdp_mcd = {
	.clks = ven_mdp_clks,
	.num_clks = ARRAY_SIZE(ven_mdp_clks),
};

static const struct of_device_id of_match_clk_mt6993_vcodec[] = {
	{
		.compatible = "mediatek,mt6993-vdecsys",
		.data = &vde2_mcd,
	}, {
		.compatible = "mediatek,mt6993-vdecsys_soc",
		.data = &vde1_mcd,
	}, {
		.compatible = "mediatek,mt6993-vencsys",
		.data = &ven1_mcd,
	}, {
		.compatible = "mediatek,mt6993-venc_gcon_core0_p1",
		.data = &ven_c0_p1_mcd,
	}, {
		.compatible = "mediatek,mt6993-vencsys_c1",
		.data = &ven2_mcd,
	}, {
		.compatible = "mediatek,mt6993-venc_gcon_core1_p1",
		.data = &ven_c1_p1_mcd,
	}, {
		.compatible = "mediatek,mt6993-venc_gcon_mdp",
		.data = &ven_mdp_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6993_vcodec_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6993_vcodec_drv = {
	.probe = clk_mt6993_vcodec_grp_probe,
	.driver = {
		.name = "clk-mt6993-vcodec",
		.of_match_table = of_match_clk_mt6993_vcodec,
	},
};

module_platform_driver(clk_mt6993_vcodec_drv);
MODULE_LICENSE("GPL");
