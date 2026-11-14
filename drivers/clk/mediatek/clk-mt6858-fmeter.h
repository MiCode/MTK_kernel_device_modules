/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: KY Liu <ky.liu@mediatek.com>
 */

#ifndef _CLK_MT6858_FMETER_H
#define _CLK_MT6858_FMETER_H

/* generate from clock_table.xlsx from TOPCKGEN DE */

/* CKGEN Part */
#define FM_AXI_CK				1
#define FM_AXI_P_CK				2
#define FM_HAXI_UFS_CK				3
#define FM_B					4
#define FM_DISP0_CK				5
#define FM_MMINFRA_CK				6
#define FM_MMUP_CK				7
#define FM_UART_CK				8
#define FM_SPI0_CK				9
#define FM_SPI1_CK				10
#define FM_SPI2_CK				11
#define FM_SPI3_CK				12
#define FM_SPI4_CK				13
#define FM_SPI5_CK				14
#define FM_SPI6_CK				15
#define FM_SPI7_CK				16
#define FM_MSDC_MACRO_1P_CK			17
#define FM_MSDC30_1_CK				18
#define FM_MSDC30_1_H_CK			19
#define FM_AUD_INTBUS_CK			20
#define FM_ATB_CK				21
#define FM_DISP_PWM_CK				22
#define FM_USB_CK				23
#define FM_USB_XHCI_CK				24
#define FM_I2C_CK				25
#define FM_SENINF_CK				26
#define FM_SENINF1_CK				27
#define FM_SENINF2_CK				28
#define FM_AUD_ENGEN1_CK			29
#define FM_AUD_ENGEN2_CK			30
#define FM_AES_UFSFDE_CK			31
#define FM_UFS_CK				32
#define FM_UFS_MBIST_CK				33
#define FM_AUD_1_CK				34
#define FM_AUD_2_CK				35
#define FM_DPMAIF_MAIN_CK			36
#define FM_VENC_CK				37
#define FM_VDEC_CK				38
#define FM_PWM_CK				39
#define FM_AUDIO_H_CK				40
#define FM_MCUPM_CK				41
#define FM_MEM_SUB_CK				42
#define FM_MEM_SUB_P_CK				43
#define FM_MEM_SUB_UFS_CK			44
#define FM_EMI_N_CK				45
#define FM_DSI_OCC_CK				46
#define FM_AP2CONN_HOST_CK			47
#define FM_IMG1_CK				48
#define FM_IPE_CK				49
#define FM_CAM_CK				50
#define FM_CAMTM_CK				51
#define FM_EMI_INTERFACE_546_CK			52
#define FM_DXCC_CK				53
#define FM_MFG_REF_CK				54
#define FM_MFGSC_REF_CK				55
#define FM_EFUSE_CK				56
#define FM_IRRX_RUN_P_P_CK			57
#define FM_UNIPLL_SES_CK			58
#define FM_DPM_CK				59
#define FM_DRAMULP_CK				60
#define FM_USB_FRMCNT_CK			61
#define FM_IOBIST_208M_CK			62
#define FM_CKGEN_NUM				63
/* ABIST Part */
#define FM_APLL1_CK				2
#define FM_APLL2_CK				3
#define FM_PLLGP_MIN_FM_CK			4
#define FM_ARMPLL_BL_CK				6
#define FM_ARMPLL_BL_CKDIV_CK			7
#define FM_ARMPLL_LL_CK				8
#define FM_ARMPLL_LL_CKDIV_CK			9
#define FM_CCIPLL_CK				10
#define FM_CCIPLL_CKDIV_CK			11
#define FM_CSI0B_DPHY_DELAYCAL_CK		12
#define FM_CSI0A_DPHY_DELAYCAL_CK		13
#define FM_LVTS_CKMON_APU			16
#define FM_TVDPLL_CK				18
#define FM_EMIPLL_CK				19
#define FM_DSI0_LNTC_DSICLK			20
#define FM_DSI0_MPLL_TST_CK			21
#define FM_ADSPPLL_CK				22
#define FM_MAINPLL_CKDIV_CK			23
#define FM_MAINPLL_CK				24
#define FM_MDPLL1_FS26M_GUIDE			25
#define FM_MMPLL_CKDIV_CK			26
#define FM_MMPLL_CK				27
#define FM_MMPLL_D3_CK				28
#define FM_MSDCPLL_CK				30
#define FM_ULPOSC2_MON_V_VCORE_CK		36
#define FM_ULPOSC_MON_V_VCORE_CK		37
#define FM_UNIVPLL_CK				38
#define FM_UNIVPLL_192M_CK			40
#define FM_UFS_CLK2FREQ_CK			41
#define FM_WBG_DIG_BPLL_CK			42
#define FM_WBG_DIG_WPLL_CK960			43
#define FM_466M_FMEM_INFRASYS			50
#define FM_MCUSYS_ARM_OUT_ALL			51
#define FM_MSDC11_IN_CK				54
#define FM_MSDC12_IN_CK				55
#define FM_MSDC21_IN_CK				56
#define FM_MSDC22_IN_CK				57
#define FM_F32K_VCORE_CK			58
#define FM_LVTS_CKMON_L7			63
#define FM_LVTS_CKMON_L6			64
#define FM_LVTS_CKMON_L5			65
#define FM_LVTS_CKMON_L4			66
#define FM_LVTS_CKMON_L3			67
#define FM_LVTS_CKMON_L2			68
#define FM_LVTS_CKMON_L1			69
#define FM_LVTS_CKMON_LM			70
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
#define FM_SYSTIMER_26M_CK			8
#define FM_PWRMCCK				9
#define FM_SSPM_F26M_CK				10
#define FM_SRCK_CK				11
#define FM_SCP_SPI_CK				12
#define FM_SCP_IIC_CK				13
#define FM_SCP_SPI_HS_CK			14
#define FM_SCP_IIC_HS_CK			15
#define FM_SSPM_ULPOSC_CK			16
#define FM_TIA_ULPOSC_CK			17
#define FM_APXGPT_26M_CK			18
#define FM_CAMTG0_CK				19
#define FM_CAMTG1_CK				20
#define FM_CAMTG2_CK				21
#define FM_CAMTG3_CK				22
#define FM_KP_IRQ_GEN_CK			23
#define FM_SSR_PKA_CK				24
#define FM_SSR_DMA_CK				25
#define FM_SSR_KDF_CK				26
#define FM_SSR_RNG_CK				27
#define FM_DXCC_CK_2				28
#define FM_ULPOSC_CORE_CK			31
#define FM_VLPCK_NUM				32

enum fm_sys_id {
	FM_CKSYS_REG = 0,
	FM_APMIXEDSYS = 1,
	FM_VLP_CKSYS_TOP = 2,
	FM_SYS_NUM = 3,
};

#endif /* _CLK_MT6858_FMETER_H */
