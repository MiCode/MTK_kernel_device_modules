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

#define GATE_MDP10(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mdp10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MDP10_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

#define GATE_MDP11(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mdp11_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MDP11_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate mdp1_clks[] = {
	/* MDP10 */
	GATE_MDP10(CLK_MDP1_MDP_MUTEX0, "mdp1_mdp_mutex0",
			"mdp_ck"/* parent */, 0),
	GATE_MDP10_V(CLK_MDP1_MDP_MUTEX0_MML, "mdp1_mdp_mutex0_mml",
			"mdp1_mdp_mutex0"/* parent */),
	GATE_MDP10(CLK_MDP1_APB_BUS, "mdp1_apb_bus",
			"mdp_ck"/* parent */, 1),
	GATE_MDP10_V(CLK_MDP1_APB_BUS_MML, "mdp1_apb_bus_mml",
			"mdp1_apb_bus"/* parent */),
	GATE_MDP10(CLK_MDP1_SMI0, "mdp1_smi0",
			"mdp_ck"/* parent */, 2),
	GATE_MDP10_V(CLK_MDP1_SMI0_MML, "mdp1_smi0_mml",
			"mdp1_smi0"/* parent */),
	GATE_MDP10_V(CLK_MDP1_SMI0_SMI, "mdp1_smi0_smi",
			"mdp1_smi0"/* parent */),
	GATE_MDP10_V(CLK_MDP1_SMI0_GENPD, "mdp1_smi0_genpd",
			"mdp1_smi0"/* parent */),
	GATE_MDP10(CLK_MDP1_MDP_RDMA0, "mdp1_mdp_rdma0",
			"mdp_ck"/* parent */, 3),
	GATE_MDP10_V(CLK_MDP1_MDP_RDMA0_MML, "mdp1_mdp_rdma0_mml",
			"mdp1_mdp_rdma0"/* parent */),
	GATE_MDP10(CLK_MDP1_MDP_RDMA2, "mdp1_mdp_rdma2",
			"mdp_ck"/* parent */, 4),
	GATE_MDP10_V(CLK_MDP1_MDP_RDMA2_MML, "mdp1_mdp_rdma2_mml",
			"mdp1_mdp_rdma2"/* parent */),
	GATE_MDP10(CLK_MDP1_MDP_HDR0, "mdp1_mdp_hdr0",
			"mdp_ck"/* parent */, 5),
	GATE_MDP10_V(CLK_MDP1_MDP_HDR0_MML, "mdp1_mdp_hdr0_mml",
			"mdp1_mdp_hdr0"/* parent */),
	GATE_MDP10(CLK_MDP1_MDP_AAL0, "mdp1_mdp_aal0",
			"mdp_ck"/* parent */, 6),
	GATE_MDP10_V(CLK_MDP1_MDP_AAL0_MML, "mdp1_mdp_aal0_mml",
			"mdp1_mdp_aal0"/* parent */),
	GATE_MDP10(CLK_MDP1_MDP_RSZ0, "mdp1_mdp_rsz0",
			"mdp_ck"/* parent */, 7),
	GATE_MDP10_V(CLK_MDP1_MDP_RSZ0_MML, "mdp1_mdp_rsz0_mml",
			"mdp1_mdp_rsz0"/* parent */),
	GATE_MDP10(CLK_MDP1_MDP_TDSHP0, "mdp1_mdp_tdshp0",
			"mdp_ck"/* parent */, 8),
	GATE_MDP10_V(CLK_MDP1_MDP_TDSHP0_MML, "mdp1_mdp_tdshp0_mml",
			"mdp1_mdp_tdshp0"/* parent */),
	GATE_MDP10(CLK_MDP1_MDP_COLOR0, "mdp1_mdp_color0",
			"mdp_ck"/* parent */, 9),
	GATE_MDP10_V(CLK_MDP1_MDP_COLOR0_MML, "mdp1_mdp_color0_mml",
			"mdp1_mdp_color0"/* parent */),
	GATE_MDP10(CLK_MDP1_MDP_WROT0, "mdp1_mdp_wrot0",
			"mdp_ck"/* parent */, 10),
	GATE_MDP10_V(CLK_MDP1_MDP_WROT0_MML, "mdp1_mdp_wrot0_mml",
			"mdp1_mdp_wrot0"/* parent */),
	GATE_MDP10(CLK_MDP1_MDP_FAKE_ENG0, "mdp1_mdp_fake_eng0",
			"mdp_ck"/* parent */, 11),
	GATE_MDP10_V(CLK_MDP1_MDP_FAKE_ENG0_MML, "mdp1_mdp_fake_eng0_mml",
			"mdp1_mdp_fake_eng0"/* parent */),
	GATE_MDP10(CLK_MDP1_MDP_DLI_ASYNC0, "mdp1_mdp_dli_async0",
			"mdp_ck"/* parent */, 12),
	GATE_MDP10_V(CLK_MDP1_MDP_DLI_ASYNC0_MML, "mdp1_mdp_dli_async0_mml",
			"mdp1_mdp_dli_async0"/* parent */),
	GATE_MDP10(CLK_MDP1_APB_DB, "mdp1_apb_db",
			"mdp_ck"/* parent */, 14),
	GATE_MDP10_V(CLK_MDP1_APB_DB_MML, "mdp1_apb_db_mml",
			"mdp1_apb_db"/* parent */),
	GATE_MDP10(CLK_MDP1_MDP_RSZ2, "mdp1_mdp_rsz2",
			"mdp_ck"/* parent */, 24),
	GATE_MDP10_V(CLK_MDP1_MDP_RSZ2_MML, "mdp1_mdp_rsz2_mml",
			"mdp1_mdp_rsz2"/* parent */),
	GATE_MDP10(CLK_MDP1_MDP_WROT2, "mdp1_mdp_wrot2",
			"mdp_ck"/* parent */, 25),
	GATE_MDP10_V(CLK_MDP1_MDP_WROT2_MML, "mdp1_mdp_wrot2_mml",
			"mdp1_mdp_wrot2"/* parent */),
	GATE_MDP10(CLK_MDP1_MDP_DLO_ASYNC0, "mdp1_mdp_dlo_async0",
			"mdp_ck"/* parent */, 26),
	GATE_MDP10_V(CLK_MDP1_MDP_DLO_ASYNC0_MML, "mdp1_mdp_dlo_async0_mml",
			"mdp1_mdp_dlo_async0"/* parent */),
	/* MDP11 */
	GATE_MDP11(CLK_MDP1_MDP_BIRSZ0, "mdp1_mdp_birsz0",
			"mdp_ck"/* parent */, 3),
	GATE_MDP11_V(CLK_MDP1_MDP_BIRSZ0_MML, "mdp1_mdp_birsz0_mml",
			"mdp1_mdp_birsz0"/* parent */),
	GATE_MDP11(CLK_MDP1_MDP_C3D0, "mdp1_mdp_c3d0",
			"mdp_ck"/* parent */, 11),
	GATE_MDP11_V(CLK_MDP1_MDP_C3D0_MML, "mdp1_mdp_c3d0_mml",
			"mdp1_mdp_c3d0"/* parent */),
	GATE_MDP11(CLK_MDP1_MDP_FG0, "mdp1_mdp_fg0",
			"mdp_ck"/* parent */, 12),
	GATE_MDP11_V(CLK_MDP1_MDP_FG0_MML, "mdp1_mdp_fg0_mml",
			"mdp1_mdp_fg0"/* parent */),
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

static const struct mtk_gate_regs mdp1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

#define GATE_MDP0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mdp0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MDP0_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

#define GATE_MDP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mdp1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MDP1_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate mdp_clks[] = {
	/* MDP0 */
	GATE_MDP0(CLK_MDP_MUTEX0, "mdp_mutex0",
			"mdp_ck"/* parent */, 0),
	GATE_MDP0_V(CLK_MDP_MUTEX0_MML, "mdp_mutex0_mml",
			"mdp_mutex0"/* parent */),
	GATE_MDP0(CLK_MDP_APB_BUS, "mdp_apb_bus",
			"mdp_ck"/* parent */, 1),
	GATE_MDP0_V(CLK_MDP_APB_BUS_MML, "mdp_apb_bus_mml",
			"mdp_apb_bus"/* parent */),
	GATE_MDP0(CLK_MDP_SMI0, "mdp_smi0",
			"mdp_ck"/* parent */, 2),
	GATE_MDP0_V(CLK_MDP_SMI0_MML, "mdp_smi0_mml",
			"mdp_smi0"/* parent */),
	GATE_MDP0_V(CLK_MDP_SMI0_SMI, "mdp_smi0_smi",
			"mdp_smi0"/* parent */),
	GATE_MDP0_V(CLK_MDP_SMI0_GENPD, "mdp_smi0_genpd",
			"mdp_smi0"/* parent */),
	GATE_MDP0(CLK_MDP_RDMA0, "mdp_rdma0",
			"mdp_ck"/* parent */, 3),
	GATE_MDP0_V(CLK_MDP_RDMA0_MML, "mdp_rdma0_mml",
			"mdp_rdma0"/* parent */),
	GATE_MDP0(CLK_MDP_RDMA2, "mdp_rdma2",
			"mdp_ck"/* parent */, 4),
	GATE_MDP0_V(CLK_MDP_RDMA2_MML, "mdp_rdma2_mml",
			"mdp_rdma2"/* parent */),
	GATE_MDP0(CLK_MDP_HDR0, "mdp_hdr0",
			"mdp_ck"/* parent */, 5),
	GATE_MDP0_V(CLK_MDP_HDR0_MML, "mdp_hdr0_mml",
			"mdp_hdr0"/* parent */),
	GATE_MDP0(CLK_MDP_AAL0, "mdp_aal0",
			"mdp_ck"/* parent */, 6),
	GATE_MDP0_V(CLK_MDP_AAL0_MML, "mdp_aal0_mml",
			"mdp_aal0"/* parent */),
	GATE_MDP0(CLK_MDP_RSZ0, "mdp_rsz0",
			"mdp_ck"/* parent */, 7),
	GATE_MDP0_V(CLK_MDP_RSZ0_MML, "mdp_rsz0_mml",
			"mdp_rsz0"/* parent */),
	GATE_MDP0(CLK_MDP_TDSHP0, "mdp_tdshp0",
			"mdp_ck"/* parent */, 8),
	GATE_MDP0_V(CLK_MDP_TDSHP0_MML, "mdp_tdshp0_mml",
			"mdp_tdshp0"/* parent */),
	GATE_MDP0(CLK_MDP_COLOR0, "mdp_color0",
			"mdp_ck"/* parent */, 9),
	GATE_MDP0_V(CLK_MDP_COLOR0_MML, "mdp_color0_mml",
			"mdp_color0"/* parent */),
	GATE_MDP0(CLK_MDP_WROT0, "mdp_wrot0",
			"mdp_ck"/* parent */, 10),
	GATE_MDP0_V(CLK_MDP_WROT0_MML, "mdp_wrot0_mml",
			"mdp_wrot0"/* parent */),
	GATE_MDP0(CLK_MDP_FAKE_ENG0, "mdp_fake_eng0",
			"mdp_ck"/* parent */, 11),
	GATE_MDP0_V(CLK_MDP_FAKE_ENG0_MML, "mdp_fake_eng0_mml",
			"mdp_fake_eng0"/* parent */),
	GATE_MDP0(CLK_MDP_APB_DB, "mdp_apb_db",
			"mdp_ck"/* parent */, 14),
	GATE_MDP0_V(CLK_MDP_APB_DB_MML, "mdp_apb_db_mml",
			"mdp_apb_db"/* parent */),
	/* MDP1 */
	GATE_MDP1(CLK_MDP_BIRSZ0, "mdp_birsz0",
			"mdp_ck"/* parent */, 3),
	GATE_MDP1_V(CLK_MDP_BIRSZ0_MML, "mdp_birsz0_mml",
			"mdp_birsz0"/* parent */),
	GATE_MDP1(CLK_MDP_C3D0, "mdp_c3d0",
			"mdp_ck"/* parent */, 11),
	GATE_MDP1_V(CLK_MDP_C3D0_MML, "mdp_c3d0_mml",
			"mdp_c3d0"/* parent */),
};

static const struct mtk_clk_desc mdp_mcd = {
	.clks = mdp_clks,
	.num_clks = CLK_MDP_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6899_mdpsys[] = {
	{
		.compatible = "mediatek,mt6899-mdpsys1",
		.data = &mdp1_mcd,
	}, {
		.compatible = "mediatek,mt6899-mdpsys",
		.data = &mdp_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6899_mdpsys_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6899_mdpsys_drv = {
	.probe = clk_mt6899_mdpsys_grp_probe,
	.driver = {
		.name = "clk-mt6899-mdpsys",
		.of_match_table = of_match_clk_mt6899_mdpsys,
	},
};

module_platform_driver(clk_mt6899_mdpsys_drv);
MODULE_LICENSE("GPL");
