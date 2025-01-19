// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Owen Chen  <owen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6991-clk.h>

#define MT_CCF_BRINGUP		1

#define pr_pd_err(fmt, ...) \
	pr_err("[CLKPD][Error] %s:%d: " fmt, __func__, __LINE__, ##__VA_ARGS__)

#define pr_pd_dbg(fmt, ...) \
	pr_notice("[CLKPD] %s:%d: " fmt, __func__, __LINE__, ##__VA_ARGS__)

static const struct mtk_gate_regs pd_cg_regs = {
	.sta_ofs = 0x5514,
};

static const struct mtk_gate_regs hwv_regs = {
	.set_ofs = 0x0218,
	.clr_ofs = 0x021C,
	.sta_ofs = 0x141C, /*done ofs*/
	//.set_sta_ofs = 0x146C, /*done ofs*/
	//.clr_sta_ofs = 0x1470, /*done ofs*/
};

static const struct mtk_gate_regs pd_cg_regs_2nd = {
	.sta_ofs = 0x5518,
};

static const struct mtk_gate_regs hwv_regs_2nd = {
	.set_ofs = 0x0220,
	.clr_ofs = 0x0224,
	.sta_ofs = 0x142C, /*done ofs*/
	//.set_sta_ofs = 0x146C, /*done ofs*/
	//.clr_sta_ofs = 0x1470, /*done ofs*/
};

#define PD_HWV_V(_id, _name, _parent) {					\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
	}

#define PD_AP_HWV(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "hw-voter-regmap",				\
		.regs = &pd_cg_regs,					\
		.hwv_regs = &hwv_regs,					\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops_inv,		\
		.flags = CLK_USE_HW_VOTER | TYPE_MTCMOS,		\
	}

#define PD_MM_HWV(_id, _name, _parent, _shift, _flags) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &pd_cg_regs,					\
		.hwv_regs = &hwv_regs,					\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops_inv,		\
		.flags = CLK_USE_HW_VOTER | TYPE_MTCMOS | _flags,	\
	}

#define PD_MM_HWV_2ND(_id, _name, _parent, _shift, _flags) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &pd_cg_regs_2nd,				\
		.hwv_regs = &hwv_regs_2nd,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops_inv,		\
		.flags = CLK_USE_HW_VOTER | TYPE_MTCMOS | _flags,	\
	}

/* need Auto Gen parent child */
static const struct mtk_gate ap_hwv_pds[] = {
	/* SSR */
	[MT6991_PD_SSR] = PD_AP_HWV(MT6991_PD_SSR, "ssr",
		/* parent */NULL, 0),
	/* SSR virtual node*/
	[MT6991_PD_SSR_SSR] = PD_HWV_V(MT6991_PD_SSR_SSR, "pd_ssr_ssr",
		/* parent */"ssr"),
};

static const struct mtk_gate mm_hwv_pds[] = {
	/* ISP_TRAW */
	[MT6991_PD_ISP_TRAW] = PD_MM_HWV(MT6991_PD_ISP_TRAW, "isp_traw",
		/* parent */"isp_main", 0, RES_FRAMEWORK_MMINFRA | RES_FRAMEWORK_VMM),
	[MT6991_PD_ISP_TRAW_SMI] = PD_HWV_V(MT6991_PD_ISP_TRAW_SMI, "isp_traw_smi",
		/* parent */"isp_traw"),
	/* ISP_DIP */
	[MT6991_PD_ISP_DIP] = PD_MM_HWV(MT6991_PD_ISP_DIP, "isp_dip",
		/* parent */"isp_main", 1, RES_FRAMEWORK_MMINFRA | RES_FRAMEWORK_VMM),
	[MT6991_PD_ISP_DIP_SMI] = PD_HWV_V(MT6991_PD_ISP_DIP_SMI, "isp_dip_smi",
		/* parent */"isp_dip"),
	/* ISP_MAIN */
	[MT6991_PD_ISP_MAIN] = PD_MM_HWV(MT6991_PD_ISP_MAIN, "isp_main",
		/* parent */"isp_vcore", 2, RES_FRAMEWORK_MMINFRA | RES_FRAMEWORK_VMM),
	[MT6991_PD_ISP_MAIN_SMI] = PD_HWV_V(MT6991_PD_ISP_MAIN_SMI, "isp_main_smi",
		/* parent */"isp_main"),
	[MT6991_PD_ISP_MAIN_IMGSYS_FW] = PD_HWV_V(MT6991_PD_ISP_MAIN_IMGSYS_FW, "isp_main_imgsys_fw",
		/* parent */"isp_main"),
	[MT6991_PD_ISP_MAIN_MAE] = PD_HWV_V(MT6991_PD_ISP_MAIN_MAE, "isp_main_mae",
		/* parent */"isp_main"),
	/* ISP_VCORE */
	[MT6991_PD_ISP_VCORE] = PD_MM_HWV(MT6991_PD_ISP_VCORE, "isp_vcore",
		/* parent */NULL, 3, RES_FRAMEWORK_MMINFRA | RES_FRAMEWORK_VMM),
	[MT6991_PD_ISP_VCORE_SMI] = PD_HWV_V(MT6991_PD_ISP_VCORE_SMI, "isp_vcore_smi",
		/* parent */"isp_vcore"),
	/* ISP_WPE_EIS */
	[MT6991_PD_ISP_WPE_EIS] = PD_MM_HWV(MT6991_PD_ISP_WPE_EIS, "isp_wpe_eis",
		/* parent */"isp_main", 4, RES_FRAMEWORK_MMINFRA | RES_FRAMEWORK_VMM),
	[MT6991_PD_ISP_WPE_EIS_SMI] = PD_HWV_V(MT6991_PD_ISP_WPE_EIS_SMI, "isp_wpe_eis_smi",
		/* parent */"isp_wpe_eis"),
	/* ISP_WPE_TNR */
	[MT6991_PD_ISP_WPE_TNR] = PD_MM_HWV(MT6991_PD_ISP_WPE_TNR, "isp_wpe_tnr",
		/* parent */"isp_main", 5, RES_FRAMEWORK_MMINFRA | RES_FRAMEWORK_VMM),
	[MT6991_PD_ISP_WPE_TNR_SMI] = PD_HWV_V(MT6991_PD_ISP_WPE_TNR_SMI, "isp_wpe_tnr_smi",
		/* parent */"isp_wpe_tnr"),
	/* ISP_WPE_LITE */
	[MT6991_PD_ISP_WPE_LITE] = PD_MM_HWV(MT6991_PD_ISP_WPE_LITE, "isp_wpe_lite",
		/* parent */"isp_main", 6, RES_FRAMEWORK_MMINFRA | RES_FRAMEWORK_VMM),
	[MT6991_PD_ISP_WPE_LITE_SMI] = PD_HWV_V(MT6991_PD_ISP_WPE_LITE_SMI, "isp_wpe_lite_smi",
		/* parent */"isp_wpe_lite"),
	/* VDE0 */
	[MT6991_PD_VDE0] = PD_MM_HWV(MT6991_PD_VDE0, "vde0",
		/* parent */"vde_vcore0", 7, RES_FRAMEWORK_MMINFRA | RES_FRAMEWORK_VMM),
	[MT6991_PD_VDE0_SMI] = PD_HWV_V(MT6991_PD_VDE0_SMI, "vde0_smi",
		/* parent */"vde0"),
	/* VDE1 */
	[MT6991_PD_VDE1] = PD_MM_HWV(MT6991_PD_VDE1, "vde1",
		/* parent */"vde_vcore0", 8, RES_FRAMEWORK_MMINFRA | RES_FRAMEWORK_VMM),
	[MT6991_PD_VDE1_SMI] = PD_HWV_V(MT6991_PD_VDE1_SMI, "vde1_smi",
		/* parent */"vde1"),
	/* VDE_VCORE */
	[MT6991_PD_VDE_VCORE0] = PD_MM_HWV(MT6991_PD_VDE_VCORE0, "vde_vcore0",
		/* parent */NULL, 9, RES_FRAMEWORK_MMINFRA | RES_FRAMEWORK_VMM),
	[MT6991_PD_VDE_VCORE0_SMI] = PD_HWV_V(MT6991_PD_VDE_VCORE0_SMI, "vde_vcore_smi",
		/* parent */"vde_vcore0"),
	/* VEN0 */
	[MT6991_PD_VEN0] = PD_MM_HWV(MT6991_PD_VEN0, "ven0",
		/* parent */NULL, 10, RES_FRAMEWORK_MMINFRA),
	[MT6991_PD_VEN0_SMI] = PD_HWV_V(MT6991_PD_VEN0_SMI, "ven0_smi",
		/* parent */"ven0"),
	/* VEN1 */
	[MT6991_PD_VEN1] = PD_MM_HWV(MT6991_PD_VEN1, "ven1",
		/* parent */"ven0", 11, RES_FRAMEWORK_MMINFRA),
	[MT6991_PD_VEN1_SMI] = PD_HWV_V(MT6991_PD_VEN1_SMI, "ven1_smi",
		/* parent */"ven1"),
	/* VEN2 */
	[MT6991_PD_VEN2] = PD_MM_HWV(MT6991_PD_VEN2, "ven2",
		/* parent */"ven1", 12, RES_FRAMEWORK_MMINFRA),
	[MT6991_PD_VEN2_SMI] = PD_HWV_V(MT6991_PD_VEN2_SMI, "ven2_smi",
		/* parent */"ven2"),
	/* CAM_MRAW */
	[MT6991_PD_CAM_MRAW] = PD_MM_HWV(MT6991_PD_CAM_MRAW, "cam_mraw",
		/* parent */"cam_main", 13, RES_FRAMEWORK_MMINFRA | RES_FRAMEWORK_VMM),
	[MT6991_PD_CAM_MRAW_SMI] = PD_HWV_V(MT6991_PD_CAM_MRAW_SMI, "cam_mraw_smi",
		/* parent */"cam_mraw"),
	/* CAM_RAWA */
	[MT6991_PD_CAM_RAWA] = PD_MM_HWV(MT6991_PD_CAM_RAWA, "cam_rawa",
		/* parent */"cam_main", 14, RES_FRAMEWORK_MMINFRA | RES_FRAMEWORK_VMM),
	[MT6991_PD_CAM_RAWA_SMI] = PD_HWV_V(MT6991_PD_CAM_RAWA_SMI, "cam_rawa_smi",
		/* parent */"cam_rawa"),
	/* CAM_RAWB */
	[MT6991_PD_CAM_RAWB] = PD_MM_HWV(MT6991_PD_CAM_RAWB, "cam_rawb",
		/* parent */"cam_main", 15, RES_FRAMEWORK_MMINFRA | RES_FRAMEWORK_VMM),
	[MT6991_PD_CAM_RAWB_SMI] = PD_HWV_V(MT6991_PD_CAM_RAWB_SMI, "cam_rawb_smi",
		/* parent */"cam_rawb"),
	/* CAM_RAWC */
	[MT6991_PD_CAM_RAWC] = PD_MM_HWV(MT6991_PD_CAM_RAWC, "cam_rawc",
		/* parent */"cam_main", 16, RES_FRAMEWORK_MMINFRA | RES_FRAMEWORK_VMM),
	[MT6991_PD_CAM_RAWC_SMI] = PD_HWV_V(MT6991_PD_CAM_RAWC_SMI, "cam_rawc_smi",
		/* parent */"cam_rawc"),
	/* CAM_RMSA */
	[MT6991_PD_CAM_RMSA] = PD_MM_HWV(MT6991_PD_CAM_RMSA, "cam_rmsa",
		/* parent */"cam_main", 17, RES_FRAMEWORK_MMINFRA | RES_FRAMEWORK_VMM),
	[MT6991_PD_CAM_RMSA_SMI] = PD_HWV_V(MT6991_PD_CAM_RMSA_SMI, "cam_rmsa_smi",
		/* parent */"cam_rmsa"),
	/* CAM_RMSB */
	[MT6991_PD_CAM_RMSB] = PD_MM_HWV(MT6991_PD_CAM_RMSB, "cam_rmsb",
		/* parent */"cam_main", 18, RES_FRAMEWORK_MMINFRA | RES_FRAMEWORK_VMM),
	[MT6991_PD_CAM_RMSB_SMI] = PD_HWV_V(MT6991_PD_CAM_RMSB_SMI, "cam_rmsb_smi",
		/* parent */"cam_rmsb"),
	/* CAM_RMSC */
	[MT6991_PD_CAM_RMSC] = PD_MM_HWV(MT6991_PD_CAM_RMSC, "cam_rmsc",
		/* parent */"cam_main", 19, RES_FRAMEWORK_MMINFRA | RES_FRAMEWORK_VMM),
	[MT6991_PD_CAM_RMSC_SMI] = PD_HWV_V(MT6991_PD_CAM_RMSC_SMI, "cam_rmsc_smi",
		/* parent */"cam_rmsc"),
	/* CAM_MAIN */
	[MT6991_PD_CAM_MAIN] = PD_MM_HWV(MT6991_PD_CAM_MAIN, "cam_main",
		/* parent */"cam_vcore", 20, RES_FRAMEWORK_MMINFRA | RES_FRAMEWORK_VMM),
	[MT6991_PD_CAM_MAIN_SMI] = PD_HWV_V(MT6991_PD_CAM_MAIN_SMI, "cam_main_smi",
		/* parent */"cam_main"),
	/* CAM_VCORE */
	[MT6991_PD_CAM_VCORE] = PD_MM_HWV(MT6991_PD_CAM_VCORE, "cam_vcore",
		/* parent */NULL, 21, RES_FRAMEWORK_MMINFRA | RES_FRAMEWORK_VMM),
	[MT6991_PD_CAM_VCORE_SMI] = PD_HWV_V(MT6991_PD_CAM_VCORE_SMI, "cam_vcore_smi",
		/* parent */"cam_vcore"),
	/* CAM_CCU */
	//PD_MM_HWV(MT6991_PD_CAM_CCU, "cam_ccu",
	//	/* parent */"cam_vcore", 22),
	/* DISP_VCORE */
	[MT6991_PD_DISP_VCORE] = PD_MM_HWV(MT6991_PD_DISP_VCORE, "disp_vcore",
		/* parent */NULL, 24, RES_FRAMEWORK_VDISP),
	[MT6991_PD_DISP_VCORE_SMI] = PD_HWV_V(MT6991_PD_DISP_VCORE_SMI, "disp_vcore_smi",
		/* parent */"disp_vcore"),
	/* DIS0 */
	[MT6991_PD_DIS0] = PD_MM_HWV(MT6991_PD_DIS0, "dis0",
		/* parent */"disp_vcore", 25, RES_FRAMEWORK_VDISP),
	[MT6991_PD_DIS0_SMI] = PD_HWV_V(MT6991_PD_DIS0_SMI, "dis0_smi",
		/* parent */"dis0"),
	/* DIS1 */
	[MT6991_PD_DIS1] = PD_MM_HWV(MT6991_PD_DIS1, "dis1",
		/* parent */"disp_vcore", 26, RES_FRAMEWORK_VDISP),
	[MT6991_PD_DIS1_SMI] = PD_HWV_V(MT6991_PD_DIS1_SMI, "dis1_smi",
		/* parent */"dis1"),
	/* OVL0 */
	[MT6991_PD_OVL0] = PD_MM_HWV(MT6991_PD_OVL0, "ovl0",
		/* parent */"disp_vcore", 27, RES_FRAMEWORK_VDISP),
	[MT6991_PD_OVL0_SMI] = PD_HWV_V(MT6991_PD_OVL0_SMI, "ovl0_smi",
		/* parent */"ovl0"),
	/* OVL1 */
	[MT6991_PD_OVL1] = PD_MM_HWV(MT6991_PD_OVL1, "ovl1",
		/* parent */"disp_vcore", 28, RES_FRAMEWORK_VDISP),
	[MT6991_PD_OVL1_SMI] = PD_HWV_V(MT6991_PD_OVL1_SMI, "pd_ovl1_smi",
		/* parent */"ovl1"),
	/* DISP_EDPTX */
	[MT6991_PD_DISP_EDPTX] = PD_MM_HWV(MT6991_PD_DISP_EDPTX, "edptx",
		/* parent */"disp_vcore", 29, RES_FRAMEWORK_VDISP),
	[MT6991_PD_DISP_EDPTX_SMI] = PD_HWV_V(MT6991_PD_DISP_EDPTX_SMI, "edptx_smi",
		/* parent */"edptx"),
	/* DISP_DPTX */
	[MT6991_PD_DISP_DPTX] = PD_MM_HWV(MT6991_PD_DISP_DPTX, "dptx",
		/* parent */"disp_vcore", 30, RES_FRAMEWORK_VDISP),
	[MT6991_PD_DISP_DPTX_SMI] = PD_HWV_V(MT6991_PD_DISP_DPTX_SMI, "dptx_smi",
		/* parent */"dptx"),
	/* MML0 */
	[MT6991_PD_MML0] = PD_MM_HWV(MT6991_PD_MML0, "mml0",
		/* parent */"disp_vcore", 31, RES_FRAMEWORK_VDISP),
	[MT6991_PD_MML0_SMI] = PD_HWV_V(MT6991_PD_MML0_SMI, "mml0_smi",
		/* parent */"mml0"),
	/* MML1 */
	[MT6991_PD_MML1] = PD_MM_HWV_2ND(MT6991_PD_MML1, "mml1",
		/* parent */"disp_vcore", 0, RES_FRAMEWORK_VDISP),
	[MT6991_PD_MML1_SMI] = PD_HWV_V(MT6991_PD_MML1_SMI, "mml1_smi",
		/* parent */"mml1"),
	/* MM_INFRA0 */
	[MT6991_PD_MM_INFRA0] = PD_MM_HWV_2ND(MT6991_PD_MM_INFRA0, "mminfra0",
		/* parent */NULL, 1, 0),
	/* MM_INFRA1 */
	[MT6991_PD_MM_INFRA1] = PD_MM_HWV_2ND(MT6991_PD_MM_INFRA1, "mminfra1",
		/* parent */NULL, 2, 0),
	/* MM_INFRA_AO */
	[MT6991_PD_MM_INFRA_AO] = PD_MM_HWV_2ND(MT6991_PD_MM_INFRA_AO, "mminfra_ao",
		/* parent */NULL, 3, 0),
	/* CSI_BS_RX */
	[MT6991_PD_CSI_BS_RX] = PD_MM_HWV_2ND(MT6991_PD_CSI_BS_RX, "csi_bs_rx",
		/* parent */NULL, 5, 0),
	[MT6991_PD_CSI_BS_RX_SMI] = PD_HWV_V(MT6991_PD_CSI_BS_RX_SMI, "csi_bs_rx_smi",
		/* parent */"csi_bs_rx"),
	/* CSI_LS_RX */
	[MT6991_PD_CSI_LS_RX] = PD_MM_HWV_2ND(MT6991_PD_CSI_LS_RX, "csi_ls_rx",
		/* parent */NULL, 6, 0),
	[MT6991_PD_CSI_LS_RX_SMI] = PD_HWV_V(MT6991_PD_CSI_LS_RX_SMI, "csi_ls_rx_smi",
		/* parent */"csi_ls_rx"),
	/* DSI_PHY0 */
	[MT6991_PD_DSI_PHY0] = PD_MM_HWV_2ND(MT6991_PD_DSI_PHY0, "dsi_phy0",
		/* parent */NULL, 7, 0),
	[MT6991_PD_DSI_PHY0_SMI] = PD_HWV_V(MT6991_PD_DSI_PHY0_SMI, "dsi_phy0_smi",
		/* parent */"dsi_phy0"),
	/* DSI_PHY1 */
	//PD_MM_HWV_2ND(MT6991_PD_DSI_PHY1, "dsi_phy1",
	//	/* parent */NULL, 8),
	/* DISP_PHY2 */
	//PD_MM_HWV_2ND(MT6991_PD_DISP_PHY2, "disp_phy2",
	//	/* parent */NULL, 9),
};

static const struct mtk_clk_desc ap_pd_mcd = {
	.clks = ap_hwv_pds,
	.num_clks = MT6991_AP_PD_NR_CLK, //defined in dt-binding
};

static const struct mtk_clk_desc mm_pd_mcd = {
	.clks = mm_hwv_pds,
	.num_clks = MT6991_MM_PD_NR_CLK, //defined in dt-binding
};

static const struct of_device_id of_match_clk_pd_mt6991[] = {
	{
		/* each mtcmos with mapping compatible name*/
		.compatible = "mediatek,mt6991-ap-pd",
		/* use of_device_get_match_data to get ap_pd_mcd*/
		.data = &ap_pd_mcd,
	}, {
		/* each mtcmos with mapping compatible name*/
		.compatible = "mediatek,mt6991-mm-pd",
		/* use of_device_get_match_data to get mm_pd_mcd*/
		.data = &mm_pd_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6991_pd_probe(struct platform_device *pdev)
{
	int r;

#if MT_CCF_BRINGUP
	pr_pd_dbg("%s: %s init begin\n", __func__, pdev->name);
#endif

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_pd_dbg("%s: %s init end\n", __func__, pdev->name);
#endif

	return r;
}

static struct platform_driver clk_mt6991_pd_drv = {
	.probe = clk_mt6991_pd_probe,
	.driver = {
		.name = "clk-mt6991-pd",
		.of_match_table = of_match_clk_pd_mt6991,
	},
};

module_platform_driver(clk_mt6991_pd_drv);
MODULE_LICENSE("GPL");
