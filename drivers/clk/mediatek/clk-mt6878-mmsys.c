// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Chuan-wen Chen <chuan-wen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6878-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

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

#define GATE_MM1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate mm_clks[] = {
	/* MM0 */
	GATE_MM0(CLK_MM_DISP_OVL0_2L, "mm_disp_ovl0_2l",
			"top_disp0_ck"/* parent */, 0),
	GATE_MM0(CLK_MM_DISP_OVL1_2L, "mm_disp_ovl1_2l",
			"top_disp0_ck"/* parent */, 1),
	GATE_MM0(CLK_MM_DISP_OVL2_2L, "mm_disp_ovl2_2l",
			"top_disp0_ck"/* parent */, 2),
	GATE_MM0(CLK_MM_DISP_OVL3_2L, "mm_disp_ovl3_2l",
			"top_disp0_ck"/* parent */, 3),
	GATE_MM0(CLK_MM_DISP_UFBC_WDMA0, "mm_disp_ufbc_wdma0",
			"top_disp0_ck"/* parent */, 4),
	GATE_MM0(CLK_MM_DISP_RSZ1, "mm_disp_rsz1",
			"top_disp0_ck"/* parent */, 5),
	GATE_MM0(CLK_MM_DISP_RSZ0, "mm_disp_rsz0",
			"top_disp0_ck"/* parent */, 6),
	GATE_MM0(CLK_MM_DISP_TDSHP0, "mm_disp_tdshp0",
			"top_disp0_ck"/* parent */, 7),
	GATE_MM0(CLK_MM_DISP_C3D0, "mm_disp_c3d0",
			"top_disp0_ck"/* parent */, 8),
	GATE_MM0(CLK_MM_DISP_COLOR0, "mm_disp_color0",
			"top_disp0_ck"/* parent */, 9),
	GATE_MM0(CLK_MM_DISP_CCORR0, "mm_disp_ccorr0",
			"top_disp0_ck"/* parent */, 10),
	GATE_MM0(CLK_MM_DISP_CCORR1, "mm_disp_ccorr1",
			"top_disp0_ck"/* parent */, 11),
	GATE_MM0(CLK_MM_DISP_AAL0, "mm_disp_aal0",
			"top_disp0_ck"/* parent */, 12),
	GATE_MM0(CLK_MM_DISP_GAMMA0, "mm_disp_gamma0",
			"top_disp0_ck"/* parent */, 13),
	GATE_MM0(CLK_MM_DISP_POSTMASK0, "mm_disp_postmask0",
			"top_disp0_ck"/* parent */, 14),
	GATE_MM0(CLK_MM_DISP_DITHER0, "mm_disp_dither0",
			"top_disp0_ck"/* parent */, 15),
	GATE_MM0(CLK_MM_DISP_TDSHP1, "mm_disp_tdshp1",
			"top_disp0_ck"/* parent */, 16),
	GATE_MM0(CLK_MM_DISP_C3D1, "mm_disp_c3d1",
			"top_disp0_ck"/* parent */, 17),
	GATE_MM0(CLK_MM_DISP_CCORR2, "mm_disp_ccorr2",
			"top_disp0_ck"/* parent */, 18),
	GATE_MM0(CLK_MM_DISP_CCORR3, "mm_disp_ccorr3",
			"top_disp0_ck"/* parent */, 19),
	GATE_MM0(CLK_MM_DISP_GAMMA1, "mm_disp_gamma1",
			"top_disp0_ck"/* parent */, 20),
	GATE_MM0(CLK_MM_DISP_DITHER1, "mm_disp_dither1",
			"top_disp0_ck"/* parent */, 21),
	GATE_MM0(CLK_MM_DISP_SPLITTER0, "mm_disp_splitter0",
			"top_disp0_ck"/* parent */, 22),
	GATE_MM0(CLK_MM_DISP_DSC_WRAP0, "mm_disp_dsc_wrap0",
			"top_disp0_ck"/* parent */, 23),
	GATE_MM0(CLK_MM_DISP_DSI0, "mm_CLK0",
			"top_disp0_ck"/* parent */, 24),
	GATE_MM0(CLK_MM_DISP_DSI1, "mm_CLK1",
			"top_disp0_ck"/* parent */, 25),
	GATE_MM0(CLK_MM_DISP_WDMA1, "mm_disp_wdma1",
			"top_disp0_ck"/* parent */, 26),
	GATE_MM0(CLK_MM_DISP_APB_BUS, "mm_disp_apb_bus",
			"top_disp0_ck"/* parent */, 27),
	GATE_MM0(CLK_MM_DISP_FAKE_ENG0, "mm_disp_fake_eng0",
			"top_disp0_ck"/* parent */, 28),
	GATE_MM0(CLK_MM_DISP_FAKE_ENG1, "mm_disp_fake_eng1",
			"top_disp0_ck"/* parent */, 29),
	GATE_MM0(CLK_MM_DISP_MUTEX0, "mm_disp_mutex0",
			"top_disp0_ck"/* parent */, 30),
	GATE_MM0(CLK_MM_SMI_COMMON, "mm_smi_common",
			"top_disp0_ck"/* parent */, 31),
	/* MM1 */
	GATE_MM1(CLK_MM_DSI0, "mm_dsi0_ck",
			"top_disp0_ck"/* parent */, 0),
	GATE_MM1(CLK_MM_DSI1, "mm_dsi1_ck",
			"top_disp0_ck"/* parent */, 1),
	GATE_MM1(CLK_MM_26M, "mm_26m_ck",
			"top_disp0_ck"/* parent */, 11),
};

static const struct mtk_clk_desc mm_mcd = {
	.clks = mm_clks,
	.num_clks = CLK_MM_NR_CLK,
};

static const struct mtk_gate_regs mminfra_config0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mminfra_config0_hwv_regs = {
	.set_ofs = 0x0000,
	.clr_ofs = 0x0004,
	.sta_ofs = 0x1C00,
};

static const struct mtk_gate_regs mminfra_config1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

static const struct mtk_gate_regs mminfra_config1_hwv_regs = {
	.set_ofs = 0x0008,
	.clr_ofs = 0x000C,
	.sta_ofs = 0x1C04,
};

#define GATE_MMINFRA_CONFIG0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mminfra_config0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_MMINFRA_CONFIG0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "hw-voter-regmap",					\
		.regs = &mminfra_config0_cg_regs,			\
		.hwv_regs = &mminfra_config0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

#define GATE_MMINFRA_CONFIG1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mminfra_config1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_MMINFRA_CONFIG1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "hw-voter-regmap",					\
		.regs = &mminfra_config1_cg_regs,			\
		.hwv_regs = &mminfra_config1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

static const struct mtk_gate mminfra_config_clks[] = {
	/* MMINFRA_CONFIG0 */
	GATE_HWV_MMINFRA_CONFIG0(CLK_MMINFRA_GCE_D, "mminfra_gce_d",
			"top_mminfra_ck"/* parent */, 0),
	GATE_HWV_MMINFRA_CONFIG0(CLK_MMINFRA_GCE_M, "mminfra_gce_m",
			"top_mminfra_ck"/* parent */, 1),
	/* MMINFRA_CONFIG1 */
	GATE_HWV_MMINFRA_CONFIG1(CLK_MMINFRA_GCE_26M, "mminfra_gce_26m",
			"top_mminfra_ck"/* parent */, 17),
};

static const struct mtk_clk_desc mminfra_config_mcd = {
	.clks = mminfra_config_clks,
	.num_clks = CLK_MMINFRA_CONFIG_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6878_mmsys[] = {
	{
		.compatible = "mediatek,mt6878-mmsys0",
		.data = &mm_mcd,
	}, {
		.compatible = "mediatek,mt6878-mminfra_config",
		.data = &mminfra_config_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6878_mmsys_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6878_mmsys_drv = {
	.probe = clk_mt6878_mmsys_grp_probe,
	.driver = {
		.name = "clk-mt6878-mmsys",
		.of_match_table = of_match_clk_mt6878_mmsys,
	},
};

module_platform_driver(clk_mt6878_mmsys_drv);
MODULE_LICENSE("GPL");
