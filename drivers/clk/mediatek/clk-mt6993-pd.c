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

static const struct mtk_gate_regs ap_pd_cg_regs_0 = {
	.sta_ofs = 0x12900,
};

static const struct mtk_gate_regs ap_hwv_regs_0 = {
	.set_ofs = 0x700,
	.clr_ofs = 0x704,
	.sta_ofs = 0x1132c,
};

#define NO_FALGS 0
#define PD_AP_HWV_0(_id, _name, _parent, _shift, _flags) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "hw-voter-regmap",				\
		.regs = &ap_pd_cg_regs_0,					\
		.hwv_regs = &ap_hwv_regs_0,					\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_ap_hwv_ops_inv,		\
		.flags = CLK_USE_HW_VOTER | TYPE_MTCMOS | _flags,	\
	}

#define PD_AP_HWV_V(_id, _name, _parent) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
	}

static const struct mtk_gate pd_ap_clks[] = {
	[CLK_PD_CONN] = PD_AP_HWV_0(CLK_PD_CONN, "conn",
			""/* parent */, 18, NO_FALGS),
	[CLK_PD_CONN_CONNSYS] = PD_AP_HWV_V(CLK_PD_CONN_CONNSYS, "conn_connsys",
	        "conn"/* parent */),
	[CLK_PD_SSUSB_DP_PHY_P0] = PD_AP_HWV_0(CLK_PD_SSUSB_DP_PHY_P0, "ssusb_dp_phy_p0",
			"ssusb_p0"/* parent */, 15, NO_FALGS),
	[CLK_PD_SSUSB_DP_PHY_P0_SSUSB] = PD_AP_HWV_V(CLK_PD_SSUSB_DP_PHY_P0_SSUSB, "ssusb_dp_phy_p0_ssusb",
	        "ssusb_dp_phy_p0"/* parent */),
	[CLK_PD_SSUSB_P0] = PD_AP_HWV_0(CLK_PD_SSUSB_P0, "ssusb_p0",
			""/* parent */, 14, NO_FALGS),
	[CLK_PD_SSUSB_P0_SSUSB] = PD_AP_HWV_V(CLK_PD_SSUSB_P0_SSUSB, "ssusb_p0_ssusb",
	        "ssusb_p0"/* parent */),
	[CLK_PD_PEXTP_MAC0] = PD_AP_HWV_0(CLK_PD_PEXTP_MAC0, "pextp_mac0",
			""/* parent */, 16, NO_FALGS),
	[CLK_PD_PEXTP_MAC0_PEXTP] = PD_AP_HWV_V(CLK_PD_PEXTP_MAC0_PEXTP, "pextp_mac0_pextp",
	        "pextp_mac0"/* parent */),
	[CLK_PD_PEXTP_MAC1] = PD_AP_HWV_0(CLK_PD_PEXTP_MAC1, "pextp_mac1",
			""/* parent */, 9, NO_FALGS),
	[CLK_PD_PEXTP_MAC1_PEXTP] = PD_AP_HWV_V(CLK_PD_PEXTP_MAC1_PEXTP, "pextp_mac1_pextp",
	        "pextp_mac1"/* parent */),
	[CLK_PD_PEXTP_PHY0] = PD_AP_HWV_0(CLK_PD_PEXTP_PHY0, "pextp_phy0",
			"pextp_mac0"/* parent */, 17, NO_FALGS),
	[CLK_PD_PEXTP_PHY0_PEXTP] = PD_AP_HWV_V(CLK_PD_PEXTP_PHY0_PEXTP, "pextp_phy0_pextp",
	        "pextp_phy0"/* parent */),
	[CLK_PD_PEXTP_PHY1] = PD_AP_HWV_0(CLK_PD_PEXTP_PHY1, "pextp_phy1",
			"pextp_mac1"/* parent */, 10, NO_FALGS),
	[CLK_PD_PEXTP_PHY1_PEXTP] = PD_AP_HWV_V(CLK_PD_PEXTP_PHY1_PEXTP, "pextp_phy1_pextp",
	        "pextp_phy1"/* parent */),
	[CLK_PD_AUDIO] = PD_AP_HWV_0(CLK_PD_AUDIO, "audio",
			"adsp_ao"/* parent */, 0, NO_FALGS),
	[CLK_PD_AUDIO_AUDIO] = PD_AP_HWV_V(CLK_PD_AUDIO_AUDIO, "audio_audio",
	        "audio"/* parent */),
	[CLK_PD_ADSP_TOP] = PD_AP_HWV_0(CLK_PD_ADSP_TOP, "adsp_top",
			"adsp_infra"/* parent */, 1, NO_FALGS),
	[CLK_PD_ADSP_TOP_ADSP_TOP] = PD_AP_HWV_V(CLK_PD_ADSP_TOP_ADSP_TOP, "adsp_top_adsp_top",
	        "adsp_top"/* parent */),
	[CLK_PD_ADSP_INFRA] = PD_AP_HWV_0(CLK_PD_ADSP_INFRA, "adsp_infra",
			"adsp_ao"/* parent */, 2, NO_FALGS),
	[CLK_PD_ADSP_INFRA_ADSP_INFRA] = PD_AP_HWV_V(CLK_PD_ADSP_INFRA_ADSP_INFRA, "adsp_infra_adsp_infra",
	        "adsp_infra"/* parent */),
	[CLK_PD_ADSP_AO] = PD_AP_HWV_0(CLK_PD_ADSP_AO, "adsp_ao",
			""/* parent */, 3, NO_FALGS),
	[CLK_PD_ADSP_AO_ADSP_AO] = PD_AP_HWV_V(CLK_PD_ADSP_AO_ADSP_AO, "adsp_ao_adsp_ao",
	        "adsp_ao"/* parent */),
	[CLK_PD_SSRSYS] = PD_AP_HWV_0(CLK_PD_SSRSYS, "ssrsys",
			""/* parent */, 11, NO_FALGS),
	[CLK_PD_SSRSYS_SSR] = PD_AP_HWV_V(CLK_PD_SSRSYS_SSR, "ssrsys_ssr",
	        "ssrsys"/* parent */),
};

static const struct mtk_clk_desc pd_ap_mcd = {
	.clks = pd_ap_clks,
	.num_clks = CLK_AP_PD_NR_CLK,
};

static const struct mtk_gate_regs mm_pd_cg_regs_1 = {
	.sta_ofs = 0x12904,
};

static const struct mtk_gate_regs mm_hwv_regs_1 = {
	.set_ofs = 0x70c,
	.clr_ofs = 0x710,
	.sta_ofs = 0x11344,
};

static const struct mtk_gate_regs mm_pd_cg_regs_0 = {
	.sta_ofs = 0x12900,
};

static const struct mtk_gate_regs mm_hwv_regs_0 = {
	.set_ofs = 0x700,
	.clr_ofs = 0x704,
	.sta_ofs = 0x1132c,
};

#define PD_MM_HWV_1(_id, _name, _parent, _shift, _flags) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &mm_pd_cg_regs_1,					\
		.hwv_regs = &mm_hwv_regs_1,					\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops_inv,		\
		.flags = CLK_USE_HW_VOTER | TYPE_MTCMOS | _flags,	\
	}

#define PD_MM_HWV_0(_id, _name, _parent, _shift, _flags) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "mm-hw-ccf-regmap",				\
		.regs = &mm_pd_cg_regs_0,					\
		.hwv_regs = &mm_hwv_regs_0,					\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_mm_hwv_ops_inv,		\
		.flags = CLK_USE_HW_VOTER | TYPE_MTCMOS | _flags,	\
	}

#define PD_MM_HWV_V(_id, _name, _parent) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
	}

static const struct mtk_gate pd_mm_clks[] = {
	[CLK_PD_IMG_DIP] = PD_MM_HWV_1(CLK_PD_IMG_DIP, "img_dip",
			"img_main"/* parent */, 6, RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA),
	[CLK_PD_IMG_DIP_CAM_P2] = PD_MM_HWV_V(CLK_PD_IMG_DIP_CAM_P2, "img_dip_cam_p2",
	        "img_dip"/* parent */),
	[CLK_PD_IMG_DIP_CINE] = PD_MM_HWV_1(CLK_PD_IMG_DIP_CINE, "img_dip_cine",
			"img_main"/* parent */, 7, RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA),
	[CLK_PD_IMG_DIP_CINE_CAM_P2] = PD_MM_HWV_V(CLK_PD_IMG_DIP_CINE_CAM_P2, "img_dip_cine_cam_p2",
	        "img_dip_cine"/* parent */),
	[CLK_PD_IMG_TRAW] = PD_MM_HWV_1(CLK_PD_IMG_TRAW, "img_traw",
			"img_main"/* parent */, 8, RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA),
	[CLK_PD_IMG_TRAW_CAM_P2] = PD_MM_HWV_V(CLK_PD_IMG_TRAW_CAM_P2, "img_traw_cam_p2",
	        "img_traw"/* parent */),
	[CLK_PD_IMG_MAIN] = PD_MM_HWV_1(CLK_PD_IMG_MAIN, "img_main",
			"img_vcore"/* parent */, 2, RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA),
	[CLK_PD_IMG_MAIN_CAM_P2] = PD_MM_HWV_V(CLK_PD_IMG_MAIN_CAM_P2, "img_main_cam_p2",
	        "img_main"/* parent */),
	[CLK_PD_IMG_VCORE] = PD_MM_HWV_1(CLK_PD_IMG_VCORE, "img_vcore",
			"mm_infra0"/* parent */, 1, RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA),
	[CLK_PD_IMG_VCORE_CAM_P2] = PD_MM_HWV_V(CLK_PD_IMG_VCORE_CAM_P2, "img_vcore_cam_p2",
	        "img_vcore"/* parent */),
	[CLK_PD_IMG_WPE_EIS] = PD_MM_HWV_1(CLK_PD_IMG_WPE_EIS, "img_wpe_eis",
			"img_main"/* parent */, 3, RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA),
	[CLK_PD_IMG_WPE_EIS_CAM_P2] = PD_MM_HWV_V(CLK_PD_IMG_WPE_EIS_CAM_P2, "img_wpe_eis_cam_p2",
	        "img_wpe_eis"/* parent */),
	[CLK_PD_IMG_WPE_TNR] = PD_MM_HWV_1(CLK_PD_IMG_WPE_TNR, "img_wpe_tnr",
			"img_main"/* parent */, 4, RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA),
	[CLK_PD_IMG_WPE_TNR_CAM_P2] = PD_MM_HWV_V(CLK_PD_IMG_WPE_TNR_CAM_P2, "img_wpe_tnr_cam_p2",
	        "img_wpe_tnr"/* parent */),
	[CLK_PD_IMG_WPE_LITE] = PD_MM_HWV_1(CLK_PD_IMG_WPE_LITE, "img_wpe_lite",
			"img_main"/* parent */, 5, RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA),
	[CLK_PD_IMG_WPE_LITE_CAM_P2] = PD_MM_HWV_V(CLK_PD_IMG_WPE_LITE_CAM_P2, "img_wpe_lite_cam_p2",
	        "img_wpe_lite"/* parent */),
	[CLK_PD_VDE0] = PD_MM_HWV_0(CLK_PD_VDE0, "vde0",
			"vde_vcore0"/* parent */, 0, RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA),
	[CLK_PD_VDE0_VDE] = PD_MM_HWV_V(CLK_PD_VDE0_VDE, "vde0_vde",
	        "vde0"/* parent */),
	[CLK_PD_VDE1] = PD_MM_HWV_0(CLK_PD_VDE1, "vde1",
			"vde_vcore0"/* parent */, 1, RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA),
	[CLK_PD_VDE1_VDE] = PD_MM_HWV_V(CLK_PD_VDE1_VDE, "vde1_vde",
	        "vde1"/* parent */),
	[CLK_PD_VDE_VCORE0] = PD_MM_HWV_0(CLK_PD_VDE_VCORE0, "vde_vcore0",
			"mm_infra0"/* parent */, 2, RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA),
	[CLK_PD_VDE_VCORE0_VDE] = PD_MM_HWV_V(CLK_PD_VDE_VCORE0_VDE, "vde_vcore0_vde",
	        "vde_vcore0"/* parent */),
	[CLK_PD_VEN0] = PD_MM_HWV_0(CLK_PD_VEN0, "ven0",
			"mm_infra0"/* parent */, 3, RES_FRAMEWORK_MMINFRA),
	[CLK_PD_VEN0_VEN] = PD_MM_HWV_V(CLK_PD_VEN0_VEN, "ven0_ven",
	        "ven0"/* parent */),
	[CLK_PD_VEN1] = PD_MM_HWV_0(CLK_PD_VEN1, "ven1",
			"ven0"/* parent */, 4, RES_FRAMEWORK_MMINFRA),
	[CLK_PD_VEN1_VEN] = PD_MM_HWV_V(CLK_PD_VEN1_VEN, "ven1_ven",
	        "ven1"/* parent */),
	[CLK_PD_VEN2] = PD_MM_HWV_0(CLK_PD_VEN2, "ven2",
			"ven1"/* parent */, 5, RES_FRAMEWORK_MMINFRA),
	[CLK_PD_VEN2_VEN] = PD_MM_HWV_V(CLK_PD_VEN2_VEN, "ven2_ven",
	        "ven2"/* parent */),
	[CLK_PD_VEN3] = PD_MM_HWV_0(CLK_PD_VEN3, "ven3",
			"ven2"/* parent */, 6, RES_FRAMEWORK_MMINFRA),
	[CLK_PD_VEN3_VEN] = PD_MM_HWV_V(CLK_PD_VEN3_VEN, "ven3_ven",
	        "ven3"/* parent */),
	[CLK_PD_VEN_MDP] = PD_MM_HWV_0(CLK_PD_VEN_MDP, "ven_mdp",
			"mm_infra0"/* parent */, 7, RES_FRAMEWORK_MMINFRA),
	[CLK_PD_VEN_MDP_VENC_MDP_JPGDEC] = PD_MM_HWV_V(CLK_PD_VEN_MDP_VENC_MDP_JPGDEC, "ven_mdp_venc_mdp_jpgdec",
	        "ven_mdp"/* parent */),
	[CLK_PD_CAM_MRAW] = PD_MM_HWV_0(CLK_PD_CAM_MRAW, "cam_mraw",
			"cam_main"/* parent */, 8, RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA),
	[CLK_PD_CAM_MRAW_CAMSV] = PD_MM_HWV_V(CLK_PD_CAM_MRAW_CAMSV, "cam_mraw_camsv",
	        "cam_mraw"/* parent */),
	[CLK_PD_CAM_RAWA] = PD_MM_HWV_0(CLK_PD_CAM_RAWA, "cam_rawa",
			"cam_main"/* parent */, 9, RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA),
	[CLK_PD_CAM_RAWA_CAM_RAWA] = PD_MM_HWV_V(CLK_PD_CAM_RAWA_CAM_RAWA, "cam_rawa_cam_rawa",
	        "cam_rawa"/* parent */),
	[CLK_PD_CAM_RAWB] = PD_MM_HWV_0(CLK_PD_CAM_RAWB, "cam_rawb",
			"cam_main"/* parent */, 10, RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA),
	[CLK_PD_CAM_RAWB_CAM_RAWB] = PD_MM_HWV_V(CLK_PD_CAM_RAWB_CAM_RAWB, "cam_rawb_cam_rawb",
	        "cam_rawb"/* parent */),
	[CLK_PD_CAM_RAWC] = PD_MM_HWV_0(CLK_PD_CAM_RAWC, "cam_rawc",
			"cam_main"/* parent */, 11, RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA),
	[CLK_PD_CAM_RAWC_CAM_RAWC] = PD_MM_HWV_V(CLK_PD_CAM_RAWC_CAM_RAWC, "cam_rawc_cam_rawc",
	        "cam_rawc"/* parent */),
	[CLK_PD_CAM_RMSA] = PD_MM_HWV_0(CLK_PD_CAM_RMSA, "cam_rmsa",
			"cam_main"/* parent */, 12, RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA),
	[CLK_PD_CAM_RMSA_CAM_RMSA] = PD_MM_HWV_V(CLK_PD_CAM_RMSA_CAM_RMSA, "cam_rmsa_cam_rmsa",
	        "cam_rmsa"/* parent */),
	[CLK_PD_CAM_RMSB] = PD_MM_HWV_0(CLK_PD_CAM_RMSB, "cam_rmsb",
			"cam_main"/* parent */, 13, RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA),
	[CLK_PD_CAM_RMSB_CAM_RMSB] = PD_MM_HWV_V(CLK_PD_CAM_RMSB_CAM_RMSB, "cam_rmsb_cam_rmsb",
	        "cam_rmsb"/* parent */),
	[CLK_PD_CAM_RMSC] = PD_MM_HWV_0(CLK_PD_CAM_RMSC, "cam_rmsc",
			"cam_main"/* parent */, 14, RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA),
	[CLK_PD_CAM_RMSC_CAM_RMSC] = PD_MM_HWV_V(CLK_PD_CAM_RMSC_CAM_RMSC, "cam_rmsc_cam_rmsc",
	        "cam_rmsc"/* parent */),
	[CLK_PD_CAM_MAIN] = PD_MM_HWV_0(CLK_PD_CAM_MAIN, "cam_main",
			"cam_vcore"/* parent */, 15, RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA),
	[CLK_PD_CAM_MAIN_CAM_MAIN] = PD_MM_HWV_V(CLK_PD_CAM_MAIN_CAM_MAIN, "cam_main_cam_main",
	        "cam_main"/* parent */),
	[CLK_PD_CAM_VCORE] = PD_MM_HWV_0(CLK_PD_CAM_VCORE, "cam_vcore",
			"mm_infra0"/* parent */, 16, RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA),
	[CLK_PD_CAM_VCORE_CAM_VCORE] = PD_MM_HWV_V(CLK_PD_CAM_VCORE_CAM_VCORE, "cam_vcore_cam_vcore",
	        "cam_vcore"/* parent */),
	[CLK_PD_CAM_CCU] = PD_MM_HWV_0(CLK_PD_CAM_CCU, "cam_ccu",
			"cam_vcore"/* parent */, 17, RES_FRAMEWORK_VMM | RES_FRAMEWORK_MMINFRA),
	[CLK_PD_CAM_CCU_CCU] = PD_MM_HWV_V(CLK_PD_CAM_CCU_CCU, "cam_ccu_ccu",
	        "cam_ccu"/* parent */),
	[CLK_PD_DISP_VCORE] = PD_MM_HWV_0(CLK_PD_DISP_VCORE, "disp_vcore",
			"mm_infra0"/* parent */, 18, NO_FALGS),
	[CLK_PD_DISP_VCORE_DISP] = PD_MM_HWV_V(CLK_PD_DISP_VCORE_DISP, "disp_vcore_disp",
	        "disp_vcore"/* parent */),
	[CLK_PD_DIS0_A] = PD_MM_HWV_0(CLK_PD_DIS0_A, "dis0_a",
			"disp_vcore"/* parent */, 19, RES_FRAMEWORK_VDISP),
	[CLK_PD_DIS0_A_DISP] = PD_MM_HWV_V(CLK_PD_DIS0_A_DISP, "dis0_a_disp",
	        "dis0_a"/* parent */),
	[CLK_PD_DIS0_B] = PD_MM_HWV_0(CLK_PD_DIS0_B, "dis0_b",
			"disp_vcore"/* parent */, 20, RES_FRAMEWORK_VDISP),
	[CLK_PD_DIS0_B_DISP] = PD_MM_HWV_V(CLK_PD_DIS0_B_DISP, "dis0_b_disp",
	        "dis0_b"/* parent */),
	[CLK_PD_DIS1_A] = PD_MM_HWV_0(CLK_PD_DIS1_A, "dis1_a",
			"disp_vcore"/* parent */, 21, RES_FRAMEWORK_VDISP),
	[CLK_PD_DIS1_A_DISP] = PD_MM_HWV_V(CLK_PD_DIS1_A_DISP, "dis1_a_disp",
	        "dis1_a"/* parent */),
	[CLK_PD_DIS1_B] = PD_MM_HWV_0(CLK_PD_DIS1_B, "dis1_b",
			"disp_vcore"/* parent */, 22, RES_FRAMEWORK_VDISP),
	[CLK_PD_DIS1_B_DISP] = PD_MM_HWV_V(CLK_PD_DIS1_B_DISP, "dis1_b_disp",
	        "dis1_b"/* parent */),
	[CLK_PD_OVL0] = PD_MM_HWV_0(CLK_PD_OVL0, "ovl0",
			"disp_vcore"/* parent */, 23, RES_FRAMEWORK_VDISP),
	[CLK_PD_OVL0_DISP] = PD_MM_HWV_V(CLK_PD_OVL0_DISP, "ovl0_disp",
	        "ovl0"/* parent */),
	[CLK_PD_OVL1] = PD_MM_HWV_0(CLK_PD_OVL1, "ovl1",
			"disp_vcore"/* parent */, 24, RES_FRAMEWORK_VDISP),
	[CLK_PD_OVL1_DISP] = PD_MM_HWV_V(CLK_PD_OVL1_DISP, "ovl1_disp",
	        "ovl1"/* parent */),
	[CLK_PD_OVL2] = PD_MM_HWV_0(CLK_PD_OVL2, "ovl2",
			"disp_vcore"/* parent */, 25, RES_FRAMEWORK_VDISP),
	[CLK_PD_OVL2_DISP] = PD_MM_HWV_V(CLK_PD_OVL2_DISP, "ovl2_disp",
	        "ovl2"/* parent */),
	[CLK_PD_DISP_DPTX] = PD_MM_HWV_1(CLK_PD_DISP_DPTX, "disp_dptx",
			"disp_vcore"/* parent */, 10, RES_FRAMEWORK_VDISP),
	[CLK_PD_DISP_DPTX_DISP] = PD_MM_HWV_V(CLK_PD_DISP_DPTX_DISP, "disp_dptx_disp",
	        "disp_dptx"/* parent */),
	[CLK_PD_VDISP_PERI] = PD_MM_HWV_1(CLK_PD_VDISP_PERI, "vdisp_peri",
			"disp_vcore"/* parent */, 9, RES_FRAMEWORK_VDISP),
	[CLK_PD_VDISP_PERI_DISP] = PD_MM_HWV_V(CLK_PD_VDISP_PERI_DISP, "vdisp_peri_disp",
	        "vdisp_peri"/* parent */),
	[CLK_PD_MML0] = PD_MM_HWV_0(CLK_PD_MML0, "mml0",
			"disp_vcore"/* parent */, 26, RES_FRAMEWORK_VDISP),
	[CLK_PD_MML0_MML] = PD_MM_HWV_V(CLK_PD_MML0_MML, "mml0_mml",
	        "mml0"/* parent */),
	[CLK_PD_MML1] = PD_MM_HWV_0(CLK_PD_MML1, "mml1",
			"disp_vcore"/* parent */, 27, RES_FRAMEWORK_VDISP),
	[CLK_PD_MML1_MML] = PD_MM_HWV_V(CLK_PD_MML1_MML, "mml1_mml",
	        "mml1"/* parent */),
	[CLK_PD_MML2] = PD_MM_HWV_0(CLK_PD_MML2, "mml2",
			"disp_vcore"/* parent */, 28, RES_FRAMEWORK_VDISP),
	[CLK_PD_MML2_MML] = PD_MM_HWV_V(CLK_PD_MML2_MML, "mml2_mml",
	        "mml2"/* parent */),
	[CLK_PD_MM_INFRA0] = PD_MM_HWV_0(CLK_PD_MM_INFRA0, "mm_infra0",
			"mm_infra_ao"/* parent */, 29, RES_FRAMEWORK_MMINFRA),
	[CLK_PD_MM_INFRA0_MMINFRA] = PD_MM_HWV_V(CLK_PD_MM_INFRA0_MMINFRA, "mm_infra0_mminfra",
	        "mm_infra0"/* parent */),
	[CLK_PD_MM_INFRA1] = PD_MM_HWV_0(CLK_PD_MM_INFRA1, "mm_infra1",
			"mm_infra0"/* parent */, 30, RES_FRAMEWORK_MMINFRA),
	[CLK_PD_MM_INFRA1_MMINFRA] = PD_MM_HWV_V(CLK_PD_MM_INFRA1_MMINFRA, "mm_infra1_mminfra",
	        "mm_infra1"/* parent */),
	[CLK_PD_MM_INFRA2] = PD_MM_HWV_1(CLK_PD_MM_INFRA2, "mm_infra2",
			"mm_infra0"/* parent */, 0, RES_FRAMEWORK_MMINFRA),
	[CLK_PD_MM_INFRA2_MMINFRA] = PD_MM_HWV_V(CLK_PD_MM_INFRA2_MMINFRA, "mm_infra2_mminfra",
	        "mm_infra2"/* parent */),
	[CLK_PD_MM_INFRA_AO] = PD_MM_HWV_0(CLK_PD_MM_INFRA_AO, "mm_infra_ao",
			"clk_null"/* mm_proc parent */, 31, NO_FALGS),
	[CLK_PD_MM_INFRA_AO_MMINFRA] = PD_MM_HWV_V(CLK_PD_MM_INFRA_AO_MMINFRA, "mm_infra_ao_mminfra",
	        "mm_infra_ao"/* parent */),
	[CLK_PD_CSI_BS_RX] = PD_MM_HWV_1(CLK_PD_CSI_BS_RX, "csi_bs_rx",
			"clk_null"/* mm_proc parent */, 11, NO_FALGS),
	[CLK_PD_CSI_BS_RX_SENINF] = PD_MM_HWV_V(CLK_PD_CSI_BS_RX_SENINF, "csi_bs_rx_seninf",
	        "csi_bs_rx"/* parent */),
	[CLK_PD_DSI_PHY0] = PD_MM_HWV_1(CLK_PD_DSI_PHY0, "dsi_phy0",
			"clk_null"/* mm_proc parent */, 13, NO_FALGS),
	[CLK_PD_DSI_PHY0_DISP] = PD_MM_HWV_V(CLK_PD_DSI_PHY0_DISP, "dsi_phy0_disp",
	        "dsi_phy0"/* parent */),
	[CLK_PD_DSI_PHY1] = PD_MM_HWV_1(CLK_PD_DSI_PHY1, "dsi_phy1",
			"clk_null"/* mm_proc parent */, 14, NO_FALGS),
	[CLK_PD_DSI_PHY1_DISP] = PD_MM_HWV_V(CLK_PD_DSI_PHY1_DISP, "dsi_phy1_disp",
	        "dsi_phy1"/* parent */),
	[CLK_PD_DSI_PHY2] = PD_MM_HWV_1(CLK_PD_DSI_PHY2, "dsi_phy2",
			"clk_null"/* mm_proc parent */, 15, NO_FALGS),
	[CLK_PD_DSI_PHY2_DISP] = PD_MM_HWV_V(CLK_PD_DSI_PHY2_DISP, "dsi_phy2_disp",
	        "dsi_phy2"/* parent */),
};

static const struct mtk_clk_desc pd_mm_mcd = {
	.clks = pd_mm_clks,
	.num_clks = CLK_PD_MM_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6993_pd[] = {
	{
		.compatible = "mediatek,mt6993-mm_pd_clk",
		.data = &pd_mm_mcd,
	}, {
		.compatible = "mediatek,mt6993-ap_pd_clk",
		.data = &pd_ap_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6993_pd_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6993_pd_drv = {
	.probe = clk_mt6993_pd_probe,
	.driver = {
		.name = "clk-mt6993-pd",
		.of_match_table = of_match_clk_mt6993_pd,
	},
};

module_platform_driver(clk_mt6993_pd_drv);
MODULE_LICENSE("GPL");
