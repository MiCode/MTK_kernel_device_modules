// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6989-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs mdp10_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mdp11_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

static const struct mtk_gate_regs mdp12_cg_regs = {
	.set_ofs = 0x124,
	.clr_ofs = 0x128,
	.sta_ofs = 0x120,
};

#define GATE_MDP10(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mdp10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MDP11(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mdp11_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MDP12(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mdp12_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MDP10_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mdp10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_dummy,	\
	}

static const struct mtk_gate mdp1_clks[] = {
	/* MDP10 */
	GATE_MDP10(CLK_MDP1_MDP_MUTEX0, "mdp1_mdp_mutex0",
			"mdp_ck"/* parent */, 0),
	GATE_MDP10(CLK_MDP1_APB_BUS, "mdp1_apb_bus",
			"mdp_ck"/* parent */, 1),
	GATE_MDP10_DUMMY(CLK_MDP1_SMI0, "mdp1_smi0",
			"mdp_ck"/* parent */, 2),
	GATE_MDP10(CLK_MDP1_SMI0_PDCK, "mdp1_smi0_pdck",
			"mdp_ck"/* parent */, 2),
	GATE_MDP10(CLK_MDP1_MDP_RDMA0, "mdp1_mdp_rdma0",
			"mdp_ck"/* parent */, 3),
	GATE_MDP10(CLK_MDP1_MDP_RDMA2, "mdp1_mdp_rdma2",
			"mdp_ck"/* parent */, 4),
	GATE_MDP10(CLK_MDP1_MDP_HDR0, "mdp1_mdp_hdr0",
			"mdp_ck"/* parent */, 5),
	GATE_MDP10(CLK_MDP1_MDP_AAL0, "mdp1_mdp_aal0",
			"mdp_ck"/* parent */, 6),
	GATE_MDP10(CLK_MDP1_MDP_RSZ0, "mdp1_mdp_rsz0",
			"mdp_ck"/* parent */, 7),
	GATE_MDP10(CLK_MDP1_MDP_TDSHP0, "mdp1_mdp_tdshp0",
			"mdp_ck"/* parent */, 8),
	GATE_MDP10(CLK_MDP1_MDP_COLOR0, "mdp1_mdp_color0",
			"mdp_ck"/* parent */, 9),
	GATE_MDP10(CLK_MDP1_MDP_WROT0, "mdp1_mdp_wrot0",
			"mdp_ck"/* parent */, 10),
	GATE_MDP10(CLK_MDP1_MDP_FAKE_ENG0, "mdp1_mdp_fake_eng0",
			"mdp_ck"/* parent */, 11),
	GATE_MDP10(CLK_MDP1_MDP_DLI_ASYNC0, "mdp1_mdp_dli_async0",
			"mdp_ck"/* parent */, 12),
	GATE_MDP10(CLK_MDP1_MDP_DLI_ASYNC1, "mdp1_mdp_dli_async1",
			"mdp_ck"/* parent */, 13),
	GATE_MDP10(CLK_MDP1_APB_DB, "mdp1_apb_db",
			"mdp_ck"/* parent */, 14),
	GATE_MDP10(CLK_MDP1_MDP_RDMA1, "mdp1_mdp_rdma1",
			"mdp_ck"/* parent */, 15),
	GATE_MDP10(CLK_MDP1_MDP_RDMA3, "mdp1_mdp_rdma3",
			"mdp_ck"/* parent */, 16),
	GATE_MDP10(CLK_MDP1_MDP_HDR1, "mdp1_mdp_hdr1",
			"mdp_ck"/* parent */, 17),
	GATE_MDP10(CLK_MDP1_MDP_AAL1, "mdp1_mdp_aal1",
			"mdp_ck"/* parent */, 18),
	GATE_MDP10(CLK_MDP1_MDP_RSZ1, "mdp1_mdp_rsz1",
			"mdp_ck"/* parent */, 19),
	GATE_MDP10(CLK_MDP1_MDP_TDSHP1, "mdp1_mdp_tdshp1",
			"mdp_ck"/* parent */, 20),
	GATE_MDP10(CLK_MDP1_MDP_COLOR1, "mdp1_mdp_color1",
			"mdp_ck"/* parent */, 21),
	GATE_MDP10(CLK_MDP1_MDP_WROT1, "mdp1_mdp_wrot1",
			"mdp_ck"/* parent */, 22),
	GATE_MDP10(CLK_MDP1_MDP_RSZ2, "mdp1_mdp_rsz2",
			"mdp_ck"/* parent */, 24),
	GATE_MDP10(CLK_MDP1_MDP_WROT2, "mdp1_mdp_wrot2",
			"mdp_ck"/* parent */, 25),
	GATE_MDP10(CLK_MDP1_MDP_DLO_ASYNC0, "mdp1_mdp_dlo_async0",
			"mdp_ck"/* parent */, 26),
	GATE_MDP10(CLK_MDP1_MDP_RSZ3, "mdp1_mdp_rsz3",
			"mdp_ck"/* parent */, 28),
	GATE_MDP10(CLK_MDP1_MDP_WROT3, "mdp1_mdp_wrot3",
			"mdp_ck"/* parent */, 29),
	GATE_MDP10(CLK_MDP1_MDP_DLO_ASYNC1, "mdp1_mdp_dlo_async1",
			"mdp_ck"/* parent */, 30),
	GATE_MDP10(CLK_MDP1_MDP_DLI_ASYNC2, "mdp1_mdp_dli_async2",
			"mdp_ck"/* parent */, 31),
	/* MDP11 */
	GATE_MDP11(CLK_MDP1_MDP_DLI_ASYNC3, "mdp1_mdp_dli_async3",
			"mdp_ck"/* parent */, 0),
	GATE_MDP11(CLK_MDP1_MDP_DLO_ASYNC2, "mdp1_mdp_dlo_async2",
			"mdp_ck"/* parent */, 1),
	GATE_MDP11(CLK_MDP1_MDP_DLO_ASYNC3, "mdp1_mdp_dlo_async3",
			"mdp_ck"/* parent */, 2),
	GATE_MDP11(CLK_MDP1_MDP_BIRSZ0, "mdp1_mdp_birsz0",
			"mdp_ck"/* parent */, 3),
	GATE_MDP11(CLK_MDP1_MDP_BIRSZ1, "mdp1_mdp_birsz1",
			"mdp_ck"/* parent */, 4),
	GATE_MDP11(CLK_MDP1_IMG_DL_ASYNC0, "mdp1_img_dl_async0",
			"mdp_ck"/* parent */, 5),
	GATE_MDP11(CLK_MDP1_IMG_DL_ASYNC1, "mdp1_img_dl_async1",
			"mdp_ck"/* parent */, 6),
	GATE_MDP11(CLK_MDP1_HRE_TOP_MDPSYS, "mdp1_hre_mdpsys",
			"mdp_ck"/* parent */, 7),
	GATE_MDP11(CLK_MDP1_MDP_RROT0, "mdp1_mdp_rrot0",
			"mdp_ck"/* parent */, 8),
	GATE_MDP11(CLK_MDP1_MDP_RROT0_2ND, "mdp1_mdp_rrot0_2nd",
			"mdp_ck"/* parent */, 9),
	GATE_MDP11(CLK_MDP1_MDP_MERGE0, "mdp1_mdp_merge0",
			"mdp_ck"/* parent */, 10),
	GATE_MDP11(CLK_MDP1_MDP_C3D0, "mdp1_mdp_c3d0",
			"mdp_ck"/* parent */, 11),
	GATE_MDP11(CLK_MDP1_MDP_FG0, "mdp1_mdp_fg0",
			"mdp_ck"/* parent */, 12),
	/* MDP12 */
	GATE_MDP12(CLK_MDP1_IMG_DL_RELAY0, "mdp1_img_dl_relay0",
			"mdp_ck"/* parent */, 0),
	GATE_MDP12(CLK_MDP1_F26M_SLOW, "mdp1_f26m_slow_ck",
			"mdp_ck"/* parent */, 5),
	GATE_MDP12(CLK_MDP1_F32K_SLOW, "mdp1_f32k_slow_ck",
			"mdp_ck"/* parent */, 6),
	GATE_MDP12(CLK_MDP1_IMG_DL_RELAY1, "mdp1_img_dl_relay1",
			"mdp_ck"/* parent */, 8),
};

static const struct mtk_clk_desc mdp1_mcd = {
	.clks = mdp1_clks,
	.num_clks = CLK_MDP1_NR_CLK,
};

static const struct mtk_gate_regs mdp0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mdp0_hwv_regs = {
	.set_ofs = 0x0048,
	.clr_ofs = 0x004C,
	.sta_ofs = 0x1C24,
};

static const struct mtk_gate_regs mdp1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

static const struct mtk_gate_regs mdp2_cg_regs = {
	.set_ofs = 0x124,
	.clr_ofs = 0x128,
	.sta_ofs = 0x120,
};

#define GATE_MDP0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mdp0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_MDP0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &mdp0_cg_regs,			\
		.hwv_regs = &mdp0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER | CLK_EN_MM_INFRA_PWR,	\
	}

#define GATE_MDP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mdp1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MDP2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mdp2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate mdp_clks[] = {
	/* MDP0 */
	GATE_MDP0(CLK_MDP_MUTEX0, "mdp_mutex0",
			"mdp_ck"/* parent */, 0),
	GATE_MDP0(CLK_MDP_APB_BUS, "mdp_apb_bus",
			"mdp_ck"/* parent */, 1),
	GATE_HWV_MDP0(CLK_MDP_SMI0, "mdp_smi0",
			"mdp_ck"/* parent */, 2),
	GATE_MDP0(CLK_MDP_RDMA0, "mdp_rdma0",
			"mdp_ck"/* parent */, 3),
	GATE_MDP0(CLK_MDP_RDMA2, "mdp_rdma2",
			"mdp_ck"/* parent */, 4),
	GATE_MDP0(CLK_MDP_HDR0, "mdp_hdr0",
			"mdp_ck"/* parent */, 5),
	GATE_MDP0(CLK_MDP_AAL0, "mdp_aal0",
			"mdp_ck"/* parent */, 6),
	GATE_MDP0(CLK_MDP_RSZ0, "mdp_rsz0",
			"mdp_ck"/* parent */, 7),
	GATE_MDP0(CLK_MDP_TDSHP0, "mdp_tdshp0",
			"mdp_ck"/* parent */, 8),
	GATE_MDP0(CLK_MDP_COLOR0, "mdp_color0",
			"mdp_ck"/* parent */, 9),
	GATE_MDP0(CLK_MDP_WROT0, "mdp_wrot0",
			"mdp_ck"/* parent */, 10),
	GATE_MDP0(CLK_MDP_FAKE_ENG0, "mdp_fake_eng0",
			"mdp_ck"/* parent */, 11),
	GATE_MDP0(CLK_MDP_DLI_ASYNC0, "mdp_dli_async0",
			"mdp_ck"/* parent */, 12),
	GATE_MDP0(CLK_MDP_DLI_ASYNC1, "mdp_dli_async1",
			"mdp_ck"/* parent */, 13),
	GATE_MDP0(CLK_MDP_APB_DB, "mdp_apb_db",
			"mdp_ck"/* parent */, 14),
	GATE_MDP0(CLK_MDP_RDMA1, "mdp_rdma1",
			"mdp_ck"/* parent */, 15),
	GATE_MDP0(CLK_MDP_RDMA3, "mdp_rdma3",
			"mdp_ck"/* parent */, 16),
	GATE_MDP0(CLK_MDP_HDR1, "mdp_hdr1",
			"mdp_ck"/* parent */, 17),
	GATE_MDP0(CLK_MDP_AAL1, "mdp_aal1",
			"mdp_ck"/* parent */, 18),
	GATE_MDP0(CLK_MDP_RSZ1, "mdp_rsz1",
			"mdp_ck"/* parent */, 19),
	GATE_MDP0(CLK_MDP_TDSHP1, "mdp_tdshp1",
			"mdp_ck"/* parent */, 20),
	GATE_MDP0(CLK_MDP_COLOR1, "mdp_color1",
			"mdp_ck"/* parent */, 21),
	GATE_MDP0(CLK_MDP_WROT1, "mdp_wrot1",
			"mdp_ck"/* parent */, 22),
	GATE_MDP0(CLK_MDP_RSZ2, "mdp_rsz2",
			"mdp_ck"/* parent */, 24),
	GATE_MDP0(CLK_MDP_WROT2, "mdp_wrot2",
			"mdp_ck"/* parent */, 25),
	GATE_MDP0(CLK_MDP_DLO_ASYNC0, "mdp_dlo_async0",
			"mdp_ck"/* parent */, 26),
	GATE_MDP0(CLK_MDP_RSZ3, "mdp_rsz3",
			"mdp_ck"/* parent */, 28),
	GATE_MDP0(CLK_MDP_WROT3, "mdp_wrot3",
			"mdp_ck"/* parent */, 29),
	GATE_MDP0(CLK_MDP_DLO_ASYNC1, "mdp_dlo_async1",
			"mdp_ck"/* parent */, 30),
	GATE_MDP0(CLK_MDP_DLI_ASYNC2, "mdp_dli_async2",
			"mdp_ck"/* parent */, 31),
	/* MDP1 */
	GATE_MDP1(CLK_MDP_DLI_ASYNC3, "mdp_dli_async3",
			"mdp_ck"/* parent */, 0),
	GATE_MDP1(CLK_MDP_DLO_ASYNC2, "mdp_dlo_async2",
			"mdp_ck"/* parent */, 1),
	GATE_MDP1(CLK_MDP_DLO_ASYNC3, "mdp_dlo_async3",
			"mdp_ck"/* parent */, 2),
	GATE_MDP1(CLK_MDP_BIRSZ0, "mdp_birsz0",
			"mdp_ck"/* parent */, 3),
	GATE_MDP1(CLK_MDP_BIRSZ1, "mdp_birsz1",
			"mdp_ck"/* parent */, 4),
	GATE_MDP1(CLK_MDP_IMG_DL_ASYNC0, "mdp_img_dl_async0",
			"mdp_ck"/* parent */, 5),
	GATE_MDP1(CLK_MDP_IMG_DL_ASYNC1, "mdp_img_dl_async1",
			"mdp_ck"/* parent */, 6),
	GATE_MDP1(CLK_MDP_HRE_TOP_MDPSYS, "mdp_hre_mdpsys",
			"mdp_ck"/* parent */, 7),
	GATE_MDP1(CLK_MDP_RROT0, "mdp_rrot0",
			"mdp_ck"/* parent */, 8),
	GATE_MDP1(CLK_MDP_RROT0_2ND, "mdp_rrot0_2nd",
			"mdp_ck"/* parent */, 9),
	GATE_MDP1(CLK_MDP_MERGE0, "mdp_merge0",
			"mdp_ck"/* parent */, 10),
	GATE_MDP1(CLK_MDP_C3D0, "mdp_c3d0",
			"mdp_ck"/* parent */, 11),
	GATE_MDP1(CLK_MDP_FG0, "mdp_fg0",
			"mdp_ck"/* parent */, 12),
	/* MDP2 */
	GATE_MDP2(CLK_MDP_IMG_DL_RELAY0, "mdp_img_dl_relay0",
			"mdp_ck"/* parent */, 0),
	GATE_MDP2(CLK_MDP_F26M_SLOW, "mdp_f26m_slow_ck",
			"mdp_ck"/* parent */, 5),
	GATE_MDP2(CLK_MDP_F32K_SLOW, "mdp_f32k_slow_ck",
			"mdp_ck"/* parent */, 6),
	GATE_MDP2(CLK_MDP_IMG_DL_RELAY1, "mdp_img_dl_relay1",
			"mdp_ck"/* parent */, 8),
};

static const struct mtk_clk_desc mdp_mcd = {
	.clks = mdp_clks,
	.num_clks = CLK_MDP_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6989_mdpsys[] = {
	{
		.compatible = "mediatek,mt6989-mdpsys1",
		.data = &mdp1_mcd,
	}, {
		.compatible = "mediatek,mt6989-mdpsys",
		.data = &mdp_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6989_mdpsys_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6989_mdpsys_drv = {
	.probe = clk_mt6989_mdpsys_grp_probe,
	.driver = {
		.name = "clk-mt6989-mdpsys",
		.of_match_table = of_match_clk_mt6989_mdpsys,
	},
};

module_platform_driver(clk_mt6989_mdpsys_drv);
MODULE_LICENSE("GPL");
