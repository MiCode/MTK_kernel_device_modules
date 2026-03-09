/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: KY Liu <ky.liu@mediatek.com>
 */

#ifndef _CLK_MT6899_FMETER_H
#define _CLK_MT6899_FMETER_H

/* generate from clock_table.xlsx from TOPCKGEN DE */

/* CKGEN Part */
#define FM_AXI_CK				1
#define FMP_FAXI_CK				2
#define FM_U_FAXI_CK				3
#define FM_PEXTP_FAXI_CK			4
#define FM_BUS_AXIMEM_CK			5
#define FM_MEM_SUB_CK				6
#define FMP_FMEM_SUB_CK				7
#define FM_U_FMEM_SUB_CK			8
#define FM_PEXTP_FMEM_SUB_CK			9
#define FM_EMI_N_CK				10
#define FM_EMI_S_CK				11
#define FM_EMI_SLICE_N_CK			12
#define FM_EMI_SLICE_S_CK			13
#define FM_AP2CONN_HOST_CK			14
#define FM_ATB_CK				15
#define FM_CIRQ_CK				16
#define FM_EFUSE_CK				17
#define FM_MCU_L3GIC_CK				18
#define FM_MCU_INFRA_CK				19
#define FM_MCU_ACP_CK				20
#define FM_TL_CK				21
#define FM_EMI_INTERFACE_546_CK			22
#define FM_DSP_CK				23
#define FM_MFG_REF_CK				24
#define FM_MFGSC_REF_CK				25
#define FM_MFG_EB_CK				26
#define FM_UART_CK				27
#define FM_SPI0_B_CK				28
#define FM_SPI1_B_CK				29
#define FM_SPI2_B_CK				30
#define FM_SPI3_B_CK				31
#define FM_SPI4_B_CK				32
#define FM_SPI5_B_CK				33
#define FM_SPI6_B_CK				34
#define FM_SPI7_B_CK				35
#define FM_MSDC_MACRO_1P_CK			36
#define FM_MSDC_MACRO_2P_CK			37
#define FM_MSDC30_1_CK				38
#define FM_MSDC30_2_CK				39
#define FM_AUD_INTBUS_CK			40
#define FM_DISP_PWM_CK				41
#define FM_USB_CK				42
#define FM_USB_XHCI_CK				43
#define FM_MSDC30_1_H_CK			44
#define FM_I2C_CK				45
#define FM_AUD_ENGEN1_CK			46
#define FM_AUD_ENGEN2_CK			47
#define FM_AES_UFSFDE_CK			48
#define FM_U_CK					49
#define FM_U_MBIST_CK				50
#define FM_PEXTP_MBIST_CK			51
#define FM_AUD_1_CK				52
#define FM_AUD_2_CK				53
#define FM_AUDIO_H_CK				54
#define FM_ADSP_CK				55
#define FM_ADPS_UARTHUB_B_CK			56
#define FM_DPMAIF_MAIN_CK			57
#define FM_PWM_CK				58
#define FM_MCUPM_CK				59
#define FM_SDF_CK				60
#define FM_UARTHUB_B_CK				61
#define FM_SSR_PKA_CK				62
#define FM_SSR_DMA_CK				63
#define FM_SSR_KDF_CK				64
#define FM_SSR_RNG_CK				65
#define FM_DXCC_CK				66
#define FM_DPSW_CMP_26M_CK			67
#define FM_SMAPCK_CK				68
#define FM_MSDC30_2_H_CK			71
#define FM_CKGEN_NUM				72
/* ABIST Part */
#define FM_MDPLL_FS26M_CK			1
#define FM_RTC32K_I				2
#define FM_MAINPLL_CKDIV_CK			3
#define FM_UNIVPLL_CKDIV_CK			4
#define FM_APLL2_CKDIV_CK			5
#define FM_APLL1_CKDIV_CK			6
#define FM_ADSPPLL_CKDIV_CK			7
#define FM_MMPLL_CKDIV_CK			8
#define FM_EMIPLL2_CKDIV_CK			9
#define FM_EMIPLL_CKDIV_CK			10
#define FM_MSDCPLL_CKDIV_CK			11
#define FM_MCUSYS_ARM_OUT_ALL			12
#define FM_DSI1_LNTC_DSICLK_FQMTR_CK		13
#define FM_DSI0_MPPLL_TST_CK			14
#define FM_DSI0_LNTC_DSICLK_FQMTR_CK		15
#define FM_DSI0_MPPLL_TST_CK_2			16
#define FM_CSI_CDPHY_DELAYCAL_CK		17
#define FM_MDPLL1_FS26M_R_CK			18
#define FM_U_CLK2FREQ_CK			19
#define FM_466M_FMEM_INFRASYS_CK		24
#define FM_R0_OUT_FM				25
#define FM_466M_FMEM_INFRASYS_2ND		26
#define FM_STH_MEM2_CK				27
#define FM_NTH_MEM2_CK				28
#define FM_ABIST_NUM				29
/* CKGEN_2 Part */
#define FM_SENINF0_CK				1
#define FM_SENINF1_CK				2
#define FM_SENINF2_CK				3
#define FM_SENINF3_CK				4
#define FM_SENINF4_CK				5
#define FM_SENINF5_CK				6
#define FM_CCU_AHB_CK				7
#define FM_IMG1_CK				8
#define FM_IPE_CK				9
#define FM_CAM_CK				10
#define FM_CAMTM_CK				11
#define FM_DPE_CK				12
#define FM_VDEC_CK				13
#define FM_CCUSYS_CK				14
#define FM_CCUTM_CK				15
#define FM_VENC_CK				16
#define FM_DP_CORE_CK				17
#define FM_DP_CK				18
#define FM_DISP_CK				19
#define FM_MDP_CK				20
#define FM_MMINFRA_CK				21
#define FM_MMUP_CK				22
#define FM_IMG_26M_CK				25
#define FM_CAM_26M_CK				26
#define FM_CKGEN_2_NUM				27
/* ABIST_2 Part */
#define FM_MAINPLL_TST_CK			4
#define FM_UNIVPLL_TST_CK			5
#define FM_MMPLL_TST_CK				6
#define FM_MSDCPLL_TST_CK			7
#define FM_ADSPPLL_TST_CK			8
#define FM_TVDPLL_CKDIV_CK			9
#define FM_APLL1_TST_CK				10
#define FM_APLL2_TST_CK				11
#define FM_EMIPLL_TST_CK			12
#define FM_EMIPLL2_TST_CK			13
#define FM_IMGPLL_CKDIV_CK			14
#define FM_ABIST_2_NUM				15
/* VLPCK Part */
#define FM_SCP_CK				1
#define FM_SCP_SPI_CK				2
#define FM_SCP_IIC_CK				3
#define FM_SCP_SPI_HS_CK			4
#define FM_SCP_IIC_HS_CK			5
#define FM_PWRAP_ULPOSC_CK			6
#define FM_SPMI_M_TIA_CK			7
#define FM_APXGPT_26M_B_CK			8
#define FM_DPSW_CK				9
#define FM_SPMI_M_CK				10
#define FM_DVFSRC_CK				11
#define FM_PWM_VLP_CK				12
#define FM_AXI_VLP_CK				13
#define FM_SYSTIMER_26M_CK			14
#define FM_SSPM_CK				15
#define FM_SRCK_CK				16
#define FM_SRAMRC_CK				17
#define FM_SPMI_P_CK				18
#define FM_IPS_CK				19
#define FM_SSPM_26M_CK				20
#define FM_ULPOSC_SSPM_CK			21
#define FM_CAMTG0_CK				22
#define FM_CAMTG1_CK				23
#define FM_CAMTG2_CK				24
#define FM_CAMTG3_CK				25
#define FM_CAMTG4_CK				26
#define FM_CAMTG5_CK				27
#define FM_CAMTG6_CK				28
#define FM_CAMTG7_CK				29
#define FM_DPSW_CENTRAL_CK			30
#define FM_TOPRGU_26M_CK			31
#define FM_ULPOSC_CK				34
#define FM_ULPOSC2_CK				35
#define FM_PMIF_SPMI_M_SYS_CK			36
#define FM_VLPCK_NUM				37

enum fm_sys_id {
	FM_TOPCKGEN = 0,
	FM_APMIXEDSYS = 1,
	FM_VLP_CKSYS = 2,
	FM_MFGPLL = 3,
	FM_MFGSCPLL = 4,
	FM_ARMPLL_LL = 5,
	FM_ARMPLL_BL = 6,
	FM_ARMPLL_B = 7,
	FM_CCIPLL = 8,
	FM_PTPPLL = 9,
	FM_SYS_NUM = 10,
};

#endif /* _CLK_MT6899_FMETER_H */
