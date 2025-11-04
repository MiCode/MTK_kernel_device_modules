/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Claude Yen <claude.yen@mediatek.com>
 */

#ifndef _CLK_MT6881_FMETER_H
#define _CLK_MT6881_FMETER_H

/* generate from clock_table.xlsx from TOPCKGEN DE */

/* CKGEN Part */
#define FM_AXI_CK				1
#define FM_AXI_PERI_CK				2
#define FM_AXI_UFS_CK				3
#define FM_B					4
#define FM_DISP0_CK				5
#define FM_MMINFRA_CK				6
#define FM_MMUP_CK				7
#define FM_UART_CK				8
#define FM_UART3_CK				9
#define FM_UARTHUB_B_CK				10
#define FM_SPI0_CK				11
#define FM_SPI1_CK				12
#define FM_SPI2_CK				13
#define FM_SPI3_CK				14
#define FM_SPI4_CK				15
#define FM_SPI5_CK				16
#define FM_SPI6_CK				17
#define FM_SPI7_CK				18
#define FM_MSDC_MACRO_1P_CK			19
#define FM_MSDC30_1_CK				20
#define FM_AUD_INTBUS_CK			21
#define FM_ATB_CK				22
#define FM_DISP_PWM_CK				23
#define FM_USB_CK				24
#define FM_USB_XHCI_CK				25
#define FM_I2C_CK				26
#define FM_I2C_5_CK				27
#define FM_SENINF_CK				28
#define FM_SENINF1_CK				29
#define FM_SENINF2_CK				30
#define FM_SENINF3_CK				31
#define FM_AUD_ENGEN1_CK			32
#define FM_AUD_ENGEN2_CK			33
#define FM_AES_UFSFDE_CK			34
#define FM_UFS_CK				35
#define FM_UFS_MBIST_CK				36
#define FM_AUD_1_CK				37
#define FM_AUD_2_CK				38
#define FM_DPMAIF_MAIN_CK			39
#define FM_VENC_CK				40
#define FM_VDEC_CK				41
#define FM_PWM_CK				42
#define FM_AUDIO_H_CK				43
#define FM_MCUPM_CK				44
#define FM_MEM_SUB_CK				45
#define FM_MEM_SUB_PERI_CK			46
#define FM_MEM_SUB_UFS_CK			47
#define FM_EMISYS_CK				48
#define FM_DSI_OCC_CK				49
#define FM_AP2CONN_HOST_CK			50
#define FM_IMG1_CK				51
#define FM_IPE_CK				52
#define FM_CAM_CK				53
#define FM_CAMTM_CK				54
#define FM_MSDC_1P_RX_CK			55
#define FM_DSP_CK				56
#define FM_EMI_INTERFACE_546_CK			57
#define FM_OCIC_SSR_PKA_CK			58
#define FM_OCIC_SSR_DMA_CK			59
#define FM_OCIC_SSR_KDF_CK			60
#define FM_OCIC_SSR_RNG_CK			61
#define FM_OCIC_SSR_PQC_CK			62
#define FM_MFG_REF_CK				63
#define FM_MFGSC_REF_CK				64
#define FM_MFG_EB_CK				65
#define FM_SPIS0_B_CK				66
#define FM_SPIS1_B_CK				67
#define FM_SPIS0_DEGLITCH_CK			68
#define FM_SPIS1_DEGLITCH_CK			69
#define FM_TL_CK				70
#define FM_PEXTP_MBIST_CK			71
#define FM_USB_FRMCNT_CK			72
#define FM_ARMPLL_DIV_PLL1_CK			73
#define FM_OCIC_CAMTG0_CK			74
#define FM_OCIC_CAMTG1_CK			75
#define FM_OCIC_CAMTG2_CK			76
#define FM_OCIC_CAMTG3_CK			77
#define FM_OCIC_CAMTG4_CK			78
#define FM_OCIC_CAMTG5_CK			79
#define FM_CKGEN_NUM				80
/* ABIST Part */
#define FM_APLL1_CK				2
#define FM_APLL2_CK				3
#define FM_TVDPLL_CKDIV_CK			4
#define FM_TVDPLL_CK				5
#define FM_EMIPLL_CKDIV_CK			6
#define FM_EMIPLL_CK				7
#define FM_CSI_CDPHY_DELAYCAL_CK		12
#define FM_LVTS_CKMON_APU			16
#define FM_DSI1_LNTC_DSICLK_FQMTR_CK		18
#define FM_DSI1_MPPLL_TST_CK			19
#define FM_DSI0_LNTC_DSICLK_FQMTR_CK		20
#define FM_DSI0_MPPLL_TST_CK			21
#define FM_MAINPLL_CKDIV_CK			23
#define FM_MAINPLL_CK				24
#define FM_F26M_CK				25
#define FM_MMPLL_CKDIV_CK			26
#define FM_MMPLL_CK				27
#define FM_MMPLL_D3_CK				28
#define FM_MSDCPLL_CK				30
#define FM_ULPOSC2_MON_VCORE_CK			36
#define FM_ULPOSC_MON_VCORE_CK			37
#define FM_UNIVPLL_CK				38
#define FM_UNIPLL_CKDIV_CK			40
#define FM_UFS_CLK2FREQ_CK			41
#define FM_WBG_DIG_BPLL_CK			42
#define FM_WBG_DIG_WPLL_CK960			43
#define FM_EMISYS_CLK				50
#define FM_MCUSYS_ARM_OUT_ALL			51
#define FM_F32K_VCORE_CK			58
#define FM_ALVTS_TO_PLLGP_MON_L7		63
#define FM_ALVTS_TO_PLLGP_MON_L6		64
#define FM_ALVTS_TO_PLLGP_MON_L5		65
#define FM_ALVTS_TO_PLLGP_MON_L4		66
#define FM_ALVTS_TO_PLLGP_MON_L3		67
#define FM_ALVTS_TO_PLLGP_MON_L2		68
#define FM_ALVTS_TO_PLLGP_MON_L1		69
#define FM_ALVTS_TO_PLLGP_MON_LM		70
#define FM_APLL1_CKDIV_CK			71
#define FM_APLL2_CKDIV_CK			72
#define FM_MSDCPLL_CKDIV_CK			77
#define FM_ABIST_NUM				78
/* VLPCK Part */
#define FM_SCP_CK				1
#define FM_PWRAP_ULPOSC_CK			2
#define FM_SPMI_P_CK				3
#define FM_SPMI_M_CK				4
#define FM_DVFSRC_CK				5
#define FM_PWM_VLP_CK				6
#define FM_AXI_VLP_CK				7
#define FM_DBGAO_26M_CK				8
#define FM_SYSTIMER_26M_CK			9
#define FM_SSPM_CK				10
#define FM_SSPM_F26M_CK				11
#define FM_SRCK_CK				12
#define FM_SRAMRC_CK				13
#define FM_SCP_SPI_CK				14
#define FM_SCP_IIC_CK				15
#define FM_SCP_SPI_HS_CK			16
#define FM_SCP_IIC_HS_CK			17
#define FM_SSPM_ULPOSC_CK			18
#define FM_TIA_ULPOSC_CK			19
#define FM_APXGPT_26M_CK			20
#define FM_KP_IRQ_GEN_CK			21
#define FM_VLPCK_NUM				22

enum fm_sys_id {
	FM_CKSYS_REG = 0,
	FM_APMIXEDSYS = 1,
	FM_VLP_CKSYS_TOP = 2,
	FM_SYS_NUM = 3,
};

#endif /* _CLK_MT6881_FMETER_H */
