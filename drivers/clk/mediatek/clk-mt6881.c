// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Claude Yen <claude.yen@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/trace_events.h>

#include "clk-mtk.h"
#include "clk-pll.h"
#include "clk-mux.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6881-clk.h>

/* bringup config */
#define MT_CCF_BRINGUP		0
#define MT_CCF_PLL_DISABLE	0
#define MT_CCF_MUX_DISABLE	0

/* Regular Number Definition */
#define INV_OFS	-1
#define INV_BIT	-1

/* TOPCK MUX SEL REG */
#define CLK_CFG_UPDATE				0x0004
#define CLK_CFG_UPDATE1				0x0008
#define CLK_CFG_UPDATE2				0x000C
#define CLK_CFG_UPDATE				0x0004
#define CLK_CFG_0				0x0010
#define CLK_CFG_0_SET				0x0014
#define CLK_CFG_0_CLR				0x0018
#define CLK_CFG_1				0x0020
#define CLK_CFG_1_SET				0x0024
#define CLK_CFG_1_CLR				0x0028
#define CLK_CFG_2				0x0030
#define CLK_CFG_2_SET				0x0034
#define CLK_CFG_2_CLR				0x0038
#define CLK_CFG_3				0x0040
#define CLK_CFG_3_SET				0x0044
#define CLK_CFG_3_CLR				0x0048
#define CLK_CFG_4				0x0050
#define CLK_CFG_4_SET				0x0054
#define CLK_CFG_4_CLR				0x0058
#define CLK_CFG_5				0x0060
#define CLK_CFG_5_SET				0x0064
#define CLK_CFG_5_CLR				0x0068
#define CLK_CFG_6				0x0070
#define CLK_CFG_6_SET				0x0074
#define CLK_CFG_6_CLR				0x0078
#define CLK_CFG_7				0x0080
#define CLK_CFG_7_SET				0x0084
#define CLK_CFG_7_CLR				0x0088
#define CLK_CFG_8				0x0090
#define CLK_CFG_8_SET				0x0094
#define CLK_CFG_8_CLR				0x0098
#define CLK_CFG_9				0x00A0
#define CLK_CFG_9_SET				0x00A4
#define CLK_CFG_9_CLR				0x00A8
#define CLK_CFG_10				0x00B0
#define CLK_CFG_10_SET				0x00B4
#define CLK_CFG_10_CLR				0x00B8
#define CLK_CFG_11				0x00C0
#define CLK_CFG_11_SET				0x00C4
#define CLK_CFG_11_CLR				0x00C8
#define CLK_CFG_12				0x00D0
#define CLK_CFG_12_SET				0x00D4
#define CLK_CFG_12_CLR				0x00D8
#define CLK_CFG_13				0x00E0
#define CLK_CFG_13_SET				0x00E4
#define CLK_CFG_13_CLR				0x00E8
#define CLK_CFG_14				0x00F0
#define CLK_CFG_14_SET				0x00F4
#define CLK_CFG_14_CLR				0x00F8
#define CLK_CFG_15				0x0100
#define CLK_CFG_15_SET				0x0104
#define CLK_CFG_15_CLR				0x0108
#define CLK_CFG_16				0x0110
#define CLK_CFG_16_SET				0x0114
#define CLK_CFG_16_CLR				0x0118
#define CLK_CFG_17				0x0120
#define CLK_CFG_17_SET				0x0124
#define CLK_CFG_17_CLR				0x0128
#define CLK_CFG_18				0x0130
#define CLK_CFG_18_SET				0x0134
#define CLK_CFG_18_CLR				0x0138
#define CLK_CFG_19				0x0140
#define CLK_CFG_19_SET				0x0144
#define CLK_CFG_19_CLR				0x0148
#define CLK_FENC_STATUS_MON_0			0x0544
#define CLK_FENC_STATUS_MON_1			0x0548
#define CLK_FENC_STATUS_MON_2			0x054C
#define CLK_AUDDIV_0				0x0320
#define VLP_CLK_CFG_0				0x0008
#define VLP_CLK_CFG_0_SET				0x000C
#define VLP_CLK_CFG_0_CLR				0x0010
#define VLP_CLK_CFG_1				0x0014
#define VLP_CLK_CFG_1_SET				0x0018
#define VLP_CLK_CFG_1_CLR				0x001C
#define VLP_CLK_CFG_2				0x0020
#define VLP_CLK_CFG_2_SET				0x0024
#define VLP_CLK_CFG_2_CLR				0x0028
#define VLP_CLK_CFG_3				0x002C
#define VLP_CLK_CFG_3_SET				0x0030
#define VLP_CLK_CFG_3_CLR				0x0034
#define VLP_CLK_CFG_4				0x0038
#define VLP_CLK_CFG_4_SET				0x003C
#define VLP_CLK_CFG_4_CLR				0x0040
#define VLP_CLK_CFG_5				0x0044
#define VLP_CLK_CFG_5_SET				0x0048
#define VLP_CLK_CFG_5_CLR				0x004C
#define VLP_OCIC_FENC_STATUS_MON_0		0x0328

/* TOPCK MUX SHIFT */
#define TOP_MUX_AXI_SHIFT			0
#define TOP_MUX_AXI_PERI_SHIFT			1
#define TOP_MUX_AXI_UFS_SHIFT			2
#define TOP_MUX_BUS_AXIMEM_SHIFT		3
#define TOP_MUX_DISP0_SHIFT			4
#define TOP_MUX_MMINFRA_SHIFT			5
#define TOP_MUX_MMUP_SHIFT			6
#define TOP_MUX_UART_SHIFT			7
#define TOP_MUX_UART3_SHIFT			8
#define TOP_MUX_UARTHUB_BCLK_SHIFT		9
#define TOP_MUX_SPI0_SHIFT			10
#define TOP_MUX_SPI1_SHIFT			11
#define TOP_MUX_SPI2_SHIFT			12
#define TOP_MUX_SPI3_SHIFT			13
#define TOP_MUX_SPI4_SHIFT			14
#define TOP_MUX_SPI5_SHIFT			15
#define TOP_MUX_SPI6_SHIFT			16
#define TOP_MUX_SPI7_SHIFT			17
#define TOP_MUX_MSDC_MACRO_1P_SHIFT		18
#define TOP_MUX_MSDC30_1_SHIFT			19
#define TOP_MUX_AUD_INTBUS_SHIFT		20
#define TOP_MUX_ATB_SHIFT			21
#define TOP_MUX_DISP_PWM_SHIFT			22
#define TOP_MUX_USB_TOP_SHIFT			23
#define TOP_MUX_SSUSB_XHCI_SHIFT		24
#define TOP_MUX_I2C_SHIFT			25
#define TOP_MUX_I2C_5_SHIFT			26
#define TOP_MUX_SENINF_SHIFT			27
#define TOP_MUX_SENINF1_SHIFT			28
#define TOP_MUX_SENINF2_SHIFT			29
#define TOP_MUX_SENINF3_SHIFT			30
#define TOP_MUX_AUD_ENGEN1_SHIFT		0
#define TOP_MUX_AUD_ENGEN2_SHIFT		1
#define TOP_MUX_AES_UFSFDE_SHIFT		2
#define TOP_MUX_UFS_SHIFT			3
#define TOP_MUX_UFS_MBIST_SHIFT			4
#define TOP_MUX_AUD_1_SHIFT			5
#define TOP_MUX_AUD_2_SHIFT			6
#define TOP_MUX_DPMAIF_MAIN_SHIFT		7
#define TOP_MUX_VENC_SHIFT			8
#define TOP_MUX_VDEC_SHIFT			9
#define TOP_MUX_PWM_SHIFT			10
#define TOP_MUX_AUDIO_H_SHIFT			11
#define TOP_MUX_MCUPM_SHIFT			12
#define TOP_MUX_MEM_SUB_SHIFT			13
#define TOP_MUX_MEM_SUB_PERI_SHIFT		14
#define TOP_MUX_MEM_SUB_UFS_SHIFT		15
#define TOP_MUX_EMISYS_SHIFT			16
#define TOP_MUX_DSI_OCC_SHIFT			17
#define TOP_MUX_AP2CONN_HOST_SHIFT		18
#define TOP_MUX_IMG1_SHIFT			19
#define TOP_MUX_IPE_SHIFT			20
#define TOP_MUX_CAM_SHIFT			21
#define TOP_MUX_CAMTM_SHIFT			22
#define TOP_MUX_MSDC_1P_RX_SHIFT		23
#define TOP_MUX_DSP_SHIFT			24
#define TOP_MUX_EMI_INTERFACE_546_SHIFT		25
#define TOP_MUX_SSR_PKA_SHIFT			26
#define TOP_MUX_SSR_DMA_SHIFT			27
#define TOP_MUX_SSR_KDF_SHIFT			28
#define TOP_MUX_SSR_RNG_SHIFT			29
#define TOP_MUX_SSR_PQC_SHIFT			30
#define TOP_MUX_MFG_REF_SHIFT			0
#define TOP_MUX_MFGSC_REF_SHIFT			1
#define TOP_MUX_MFG_EB_SHIFT			2
#define TOP_MUX_SPIS0_BCLK_SHIFT		3
#define TOP_MUX_SPIS1_BCLK_SHIFT		4
#define TOP_MUX_SPIS0_DEGLITCH_SHIFT		5
#define TOP_MUX_SPIS1_DEGLITCH_SHIFT		6
#define TOP_MUX_TL_SHIFT			7
#define TOP_MUX_PEXTP_MBIST_SHIFT		8
#define TOP_MUX_SSUSB_FRMCNT_SHIFT		9
#define TOP_MUX_ARMPLL_DIVIDER_PLL1_SHIFT	10
#define TOP_MUX_CAMTG0_SHIFT			11
#define TOP_MUX_CAMTG1_SHIFT			12
#define TOP_MUX_CAMTG2_SHIFT			13
#define TOP_MUX_CAMTG3_SHIFT			14
#define TOP_MUX_CAMTG4_SHIFT			15
#define TOP_MUX_CAMTG5_SHIFT			16
#define TOP_MUX_SCP_SHIFT			0
#define TOP_MUX_PWRAP_ULPOSC_SHIFT		1
#define TOP_MUX_SPMI_P_MST_SHIFT		2
#define TOP_MUX_SPMI_M_MST_SHIFT		3
#define TOP_MUX_DVFSRC_SHIFT			4
#define TOP_MUX_PWM_VLP_SHIFT			5
#define TOP_MUX_AXI_VLP_SHIFT			6
#define TOP_MUX_DBGAO_26M_SHIFT			7
#define TOP_MUX_SYSTIMER_26M_SHIFT		8
#define TOP_MUX_SSPM_SHIFT			9
#define TOP_MUX_SSPM_F26M_SHIFT			10
#define TOP_MUX_SRCK_SHIFT			11
#define TOP_MUX_SRAMRC_SHIFT			12
#define TOP_MUX_SCP_SPI_SHIFT			13
#define TOP_MUX_SCP_IIC_SHIFT			14
#define TOP_MUX_SCP_SPI_HIGH_SPD_SHIFT		15
#define TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT		16
#define TOP_MUX_SSPM_ULPOSC_SHIFT		17
#define TOP_MUX_TIA_ULPOSC_SHIFT		18
#define TOP_MUX_APXGPT_26M_SHIFT		19

/* TOPCK CKSTA REG */
#define CKSTA_REG				0x0230
#define CKSTA_REG1				0x0234
#define CKSTA_REG2				0x0238

/* TOPCK DIVIDER REG */
#define CLK_AUDDIV_2				0x0328
#define CLK_AUDDIV_3				0x0334
#define CLK_AUDDIV_4				0x0338
#define CLK_AUDDIV_5				0x033C

/* APMIXED PLL REG */
#define AP_PLL_CON3				0x008
#define APLL1_TUNER_CON0			0x024
#define APLL2_TUNER_CON0			0x028
#define FENC_ENABLE_CON0			0x058
#define MAINPLL_CON0				0x250
#define MAINPLL_CON1				0x254
#define MAINPLL_CON2				0x258
#define MAINPLL_CON3				0x25C
#define UNIVPLL_CON0				0x264
#define UNIVPLL_CON1				0x268
#define UNIVPLL_CON2				0x26C
#define UNIVPLL_CON3				0x270
#define MSDCPLL_CON0				0x278
#define MSDCPLL_CON1				0x27C
#define MSDCPLL_CON2				0x280
#define MSDCPLL_CON3				0x284
#define MMPLL_CON0				0x2A0
#define MMPLL_CON1				0x2A4
#define MMPLL_CON2				0x2A8
#define MMPLL_CON3				0x2AC
#define EMIPLL_CON0				0x28C
#define EMIPLL_CON1				0x290
#define EMIPLL_CON2				0x294
#define EMIPLL_CON3				0x298
#define APLL1_CON0				0x2C8
#define APLL1_CON1				0x2CC
#define APLL1_CON2				0x2D0
#define APLL1_CON3				0x2D4
#define APLL2_CON0				0x2E0
#define APLL2_CON1				0x2E4
#define APLL2_CON2				0x2E8
#define APLL2_CON3				0x2EC
#define TVDPLL_CON0				0x2B4
#define TVDPLL_CON1				0x2B8
#define TVDPLL_CON2				0x2BC
#define TVDPLL_CON3				0x2C0

/* HW Voter REG */


static DEFINE_SPINLOCK(mt6881_clk_lock);

static const struct mtk_fixed_factor cksys_reg_divs[] = {
	FACTOR(CLK_CKSYS_REG_MAINPLL, "cksys_mainpll",
			"mainpll", 1, 1),
	FACTOR(CLK_CKSYS_REG_MAINPLL_D2, "cksys_mainpll_d2",
			"mainpll", 1, 2),
	FACTOR(CLK_CKSYS_REG_MAINPLL_D3, "cksys_mainpll_d3",
			"mainpll", 1, 3),
	FACTOR(CLK_CKSYS_REG_MAINPLL_D4, "cksys_mainpll_d4",
			"mainpll", 1, 4),
	FACTOR(CLK_CKSYS_REG_MAINPLL_D4_D2, "cksys_mainpll_d4_d2",
			"mainpll", 1, 8),
	FACTOR(CLK_CKSYS_REG_MAINPLL_D4_D4, "cksys_mainpll_d4_d4",
			"mainpll", 1, 16),
	FACTOR(CLK_CKSYS_REG_MAINPLL_D4_D8, "cksys_mainpll_d4_d8",
			"mainpll", 1, 32),
	FACTOR(CLK_CKSYS_REG_MAINPLL_D5, "cksys_mainpll_d5",
			"mainpll", 1, 5),
	FACTOR(CLK_CKSYS_REG_MAINPLL_D5_D2, "cksys_mainpll_d5_d2",
			"mainpll", 1, 10),
	FACTOR(CLK_CKSYS_REG_MAINPLL_D5_D4, "cksys_mainpll_d5_d4",
			"mainpll", 1, 20),
	FACTOR(CLK_CKSYS_REG_MAINPLL_D6, "cksys_mainpll_d6",
			"mainpll", 1, 6),
	FACTOR(CLK_CKSYS_REG_MAINPLL_D6_D2, "cksys_mainpll_d6_d2",
			"mainpll", 1, 12),
	FACTOR(CLK_CKSYS_REG_MAINPLL_D6_D4, "cksys_mainpll_d6_d4",
			"mainpll", 1, 24),
	FACTOR(CLK_CKSYS_REG_MAINPLL_D7, "cksys_mainpll_d7",
			"mainpll", 1, 7),
	FACTOR(CLK_CKSYS_REG_MAINPLL_D7_D2, "cksys_mainpll_d7_d2",
			"mainpll", 1, 14),
	FACTOR(CLK_CKSYS_REG_MAINPLL_D7_D4, "cksys_mainpll_d7_d4",
			"mainpll", 1, 28),
	FACTOR(CLK_CKSYS_REG_MAINPLL_D9, "cksys_mainpll_d9",
			"mainpll", 1, 9),
	FACTOR(CLK_CKSYS_REG_UNIVPLL, "cksys_univpll",
			"univpll", 1, 1),
	FACTOR(CLK_CKSYS_REG_UNIVPLL_D3, "cksys_univpll_d3",
			"univpll", 1, 3),
	FACTOR(CLK_CKSYS_REG_UNIVPLL_D4, "cksys_univpll_d4",
			"univpll", 1, 4),
	FACTOR(CLK_CKSYS_REG_UNIVPLL_D4_D2, "cksys_univpll_d4_d2",
			"univpll", 1, 8),
	FACTOR(CLK_CKSYS_REG_UNIVPLL_D4_D4, "cksys_univpll_d4_d4",
			"univpll", 1, 16),
	FACTOR(CLK_CKSYS_REG_UNIVPLL_D4_D8, "cksys_univpll_d4_d8",
			"univpll", 1, 32),
	FACTOR(CLK_CKSYS_REG_UNIVPLL_D5, "cksys_univpll_d5",
			"univpll", 1, 5),
	FACTOR(CLK_CKSYS_REG_UNIVPLL_D5_D2, "cksys_univpll_d5_d2",
			"univpll", 1, 10),
	FACTOR(CLK_CKSYS_REG_UNIVPLL_D5_D4, "cksys_univpll_d5_d4",
			"univpll", 1, 20),
	FACTOR(CLK_CKSYS_REG_UNIVPLL_D6, "cksys_univpll_d6",
			"univpll", 1, 6),
	FACTOR(CLK_CKSYS_REG_UNIVPLL_D6_D2, "cksys_univpll_d6_d2",
			"univpll", 1, 12),
	FACTOR(CLK_CKSYS_REG_UNIVPLL_D6_D4, "cksys_univpll_d6_d4",
			"univpll", 1, 24),
	FACTOR(CLK_CKSYS_REG_UNIVPLL_D6_D8, "cksys_univpll_d6_d8",
			"univpll", 1, 48),
	FACTOR(CLK_CKSYS_REG_UNIVPLL_D6_D16, "cksys_univpll_d6_d16",
			"univpll", 1, 96),
	FACTOR(CLK_CKSYS_REG_UNIVPLL_D7, "cksys_univpll_d7",
			"univpll", 1, 7),
	FACTOR(CLK_CKSYS_REG_UNIVPLL_D7_D2, "cksys_univpll_d7_d2",
			"univpll", 1, 14),
	FACTOR(CLK_CKSYS_REG_UNIVPLL_192M, "cksys_univpll_192m",
			"univpll", 1, 13),
	FACTOR(CLK_CKSYS_REG_UNIVPLL_192M_D2, "cksys_univpll_192m_d2",
			"univpll", 1, 26),
	FACTOR(CLK_CKSYS_REG_UNIVPLL_192M_D4, "cksys_univpll_192m_d4",
			"univpll", 1, 52),
	FACTOR(CLK_CKSYS_REG_UNIVPLL_192M_D8, "cksys_univpll_192m_d8",
			"univpll", 1, 104),
	FACTOR(CLK_CKSYS_REG_UNIVPLL_192M_D10, "cksys_univpll_192m_d10",
			"univpll", 1, 130),
	FACTOR(CLK_CKSYS_REG_UNIVPLL_192M_D16, "cksys_univpll_192m_d16",
			"univpll", 1, 208),
	FACTOR(CLK_CKSYS_REG_UNIVPLL_192M_D32, "cksys_univpll_192m_d32",
			"univpll", 1, 416),
	FACTOR(CLK_CKSYS_REG_APLL1, "cksys_apll1",
			"apll1", 1, 1),
	FACTOR(CLK_CKSYS_REG_APLL1_D2, "cksys_apll1_d2",
			"apll1", 1, 2),
	FACTOR(CLK_CKSYS_REG_APLL1_D4, "cksys_apll1_d4",
			"apll1", 1, 4),
	FACTOR(CLK_CKSYS_REG_APLL1_D8, "cksys_apll1_d8",
			"apll1", 1, 8),
	FACTOR(CLK_CKSYS_REG_APLL2, "cksys_apll2",
			"apll2", 1, 1),
	FACTOR(CLK_CKSYS_REG_APLL2_D2, "cksys_apll2_d2",
			"apll2", 1, 2),
	FACTOR(CLK_CKSYS_REG_APLL2_D4, "cksys_apll2_d4",
			"apll2", 1, 4),
	FACTOR(CLK_CKSYS_REG_APLL2_D8, "cksys_apll2_d8",
			"apll2", 1, 8),
	FACTOR(CLK_CKSYS_REG_TVDPLL, "cksys_tvdpll",
			"tvdpll", 1, 1),
	FACTOR(CLK_CKSYS_REG_MMPLL, "cksys_mmpll",
			"mmpll", 1, 1),
	FACTOR(CLK_CKSYS_REG_MMPLL_D4, "cksys_mmpll_d4",
			"mmpll", 1, 4),
	FACTOR(CLK_CKSYS_REG_MMPLL_D4_D2, "cksys_mmpll_d4_d2",
			"mmpll", 1, 8),
	FACTOR(CLK_CKSYS_REG_MMPLL_D5, "cksys_mmpll_d5",
			"mmpll", 1, 5),
	FACTOR(CLK_CKSYS_REG_MMPLL_D5_D2, "cksys_mmpll_d5_d2",
			"mmpll", 1, 10),
	FACTOR(CLK_CKSYS_REG_MMPLL_D6, "cksys_mmpll_d6",
			"mmpll", 1, 6),
	FACTOR(CLK_CKSYS_REG_MMPLL_D6_D2, "cksys_mmpll_d6_d2",
			"mmpll", 1, 12),
	FACTOR(CLK_CKSYS_REG_MMPLL_D7, "cksys_mmpll_d7",
			"mmpll", 1, 7),
	FACTOR(CLK_CKSYS_REG_MMPLL_D9, "cksys_mmpll_d9",
			"mmpll", 1, 9),
	FACTOR(CLK_CKSYS_REG_EMIPLL, "cksys_emipll",
			"emipll", 1, 1),
	FACTOR(CLK_CKSYS_REG_MSDCPLL, "cksys_msdcpll",
			"msdcpll", 1, 1),
	FACTOR(CLK_CKSYS_REG_MSDCPLL_D2, "cksys_msdcpll_d2",
			"msdcpll", 1, 2),
	FACTOR(CLK_CKSYS_REG_CLKRTC, "cksys_clkrtc",
			"clk32k", 1, 1),
	FACTOR(CLK_CKSYS_REG_TCK_26M_MX9, "cksys_tck_26m_mx9_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_CKSYS_REG_F26M_CK_D2, "cksys_f26m_d2",
			"clk13m", 1, 1),
	FACTOR(CLK_CKSYS_REG_F26M, "cksys_f26m_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_CKSYS_REG_OSC, "cksys_osc",
			"ulposc", 1, 1),
	FACTOR(CLK_CKSYS_REG_OSC_D2, "cksys_osc_d2",
			"ulposc", 1, 2),
	FACTOR(CLK_CKSYS_REG_OSC_D4, "cksys_osc_d4",
			"ulposc", 1, 4),
	FACTOR(CLK_CKSYS_REG_OSC_D7, "cksys_osc_d7",
			"ulposc", 1, 7),
	FACTOR(CLK_CKSYS_REG_OSC_D16, "cksys_osc_d16",
			"ulposc", 61, 973),
	FACTOR(CLK_CKSYS_REG_OSC_D10, "cksys_osc_d10",
			"ulposc", 1, 10),
	FACTOR(CLK_CKSYS_REG_OSC_D20, "cksys_osc_d20",
			"ulposc", 1, 20),
	FACTOR(CLK_CKSYS_REG_AXI_POST, "cksys_axi_post_ck",
			"cksys_axi_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_AXI_PERI, "cksys_axi_peri_ck",
			"cksys_axi_peri_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_HAXI_UFS, "cksys_haxi_ufs_ck",
			"cksys_axi_ufs_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_DISP0, "cksys_disp0_ck",
			"cksys_disp0_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_MMINFRA, "cksys_mminfra_ck",
			"cksys_mminfra_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_UART, "cksys_uart_ck",
			"cksys_uart_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_SPI0, "cksys_spi0_ck",
			"cksys_spi0_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_SPI1, "cksys_spi1_ck",
			"cksys_spi1_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_SPI2, "cksys_spi2_ck",
			"cksys_spi2_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_SPI3, "cksys_spi3_ck",
			"cksys_spi3_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_SPI4, "cksys_spi4_ck",
			"cksys_spi4_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_SPI5, "cksys_spi5_ck",
			"cksys_spi5_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_SPI6, "cksys_spi6_ck",
			"cksys_spi6_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_SPI7, "cksys_spi7_ck",
			"cksys_spi7_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_MSDC30_1, "cksys_msdc30_1_ck",
			"cksys_msdc30_1_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_DISP_PWM, "cksys_disp_pwm_ck",
			"cksys_disp_pwm_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_I2C, "cksys_i2c_ck",
			"cksys_i2c_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_I2C_5, "cksys_i2c_5_ck",
			"cksys_i2c_5_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_SENINF, "cksys_seninf_ck",
			"cksys_seninf_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_SENINF1, "cksys_seninf1_ck",
			"cksys_seninf1_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_SENINF2, "cksys_seninf2_ck",
			"cksys_seninf2_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_SENINF3, "cksys_seninf3_ck",
			"cksys_seninf3_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_AUD_ENGEN1, "cksys_aud_engen1_ck",
			"cksys_aud_engen1_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_AUD_ENGEN2, "cksys_aud_engen2_ck",
			"cksys_aud_engen2_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_AES_UFSFDE, "cksys_aes_ufsfde_ck",
			"cksys_aes_ufsfde_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_UFS, "cksys_ufs_ck",
			"cksys_ufs_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_AUD_1, "cksys_aud_1_ck",
			"cksys_aud_1_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_AUD_2, "cksys_aud_2_ck",
			"cksys_aud_2_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_DPMAIF_MAIN, "cksys_dpmaif_main_ck",
			"cksys_dpmaif_main_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_VENC, "cksys_venc_ck",
			"cksys_venc_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_VDEC, "cksys_vdec_ck",
			"cksys_vdec_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_AUDIO_H, "cksys_audio_h_ck",
			"cksys_audio_h_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_MEM_SUB_UFS, "cksys_mem_sub_ufs_ck",
			"cksys_mem_sub_ufs_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_IMG1, "cksys_img1_ck",
			"cksys_img1_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_IPE, "cksys_ipe_ck",
			"cksys_ipe_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_CAM, "cksys_cam_ck",
			"cksys_cam_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_CAMTM, "cksys_camtm_ck",
			"cksys_camtm_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_OCIC_SSR_DMA, "cksys_ocic_ssr_dma_ck",
			"cksys_ssr_dma_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_OCIC_IMG_AOV_26M, "cksys_ocic_img_aov_26m_ck",
			"cksys_tck_26m_mx9_ck", 1, 1),
};

static const char * const cksys_axi_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d7_d2",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d5_d2",
	"cksys_mainpll_d6_d2",
	"cksys_osc_d4"
};

static const char * const cksys_axi_peri_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d7_d2",
	"cksys_osc_d4"
};

static const char * const cksys_axi_ufs_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d7_d2",
	"cksys_osc_d4"
};

static const char * const cksys_bus_aximem_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d7_d2",
	"cksys_mainpll_d5_d2",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d6"
};

static const char * const cksys_disp0_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d5_d2",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d6",
	"cksys_univpll_d6",
	"cksys_mainpll_d4",
	"cksys_tvdpll",
	"cksys_mainpll_d3",
	"cksys_univpll_d3"
};

static const char * const cksys_mminfra_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d2",
	"cksys_mainpll_d5_d2",
	"cksys_mmpll_d6_d2",
	"cksys_mainpll_d4_d2",
	"cksys_mmpll_d4_d2",
	"cksys_mainpll_d6",
	"cksys_mmpll_d7",
	"cksys_univpll_d6",
	"cksys_mainpll_d5",
	"cksys_mmpll_d6",
	"cksys_univpll_d5",
	"cksys_mainpll_d4",
	"cksys_univpll_d4",
	"cksys_univpll_d3",
	"cksys_mmpll_d5_d2"
};

static const char * const cksys_mmup_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d5_d2",
	"cksys_mmpll_d4_d2",
	"cksys_mainpll_d4",
	"cksys_univpll_d4",
	"cksys_mmpll_d4",
	"cksys_mainpll_d3"
};

static const char * const cksys_uart_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d8",
	"cksys_univpll_d6_d4",
	"cksys_univpll_d6_d2"
};

static const char * const cksys_uart3_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d8",
	"cksys_univpll_d6_d4",
	"cksys_univpll_d6_d2"
};

static const char * const cksys_uarthub_b_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d4",
	"cksys_univpll_d6_d2"
};

static const char * const cksys_spi0_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d2",
	"cksys_univpll_192m",
	"cksys_mainpll_d6_d2",
	"cksys_univpll_d4_d4",
	"cksys_mainpll_d4_d4",
	"cksys_univpll_d5_d4",
	"cksys_univpll_d6_d4"
};

static const char * const cksys_spi1_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d2",
	"cksys_univpll_192m",
	"cksys_mainpll_d6_d2",
	"cksys_univpll_d4_d4",
	"cksys_mainpll_d4_d4",
	"cksys_univpll_d5_d4",
	"cksys_univpll_d6_d4"
};

static const char * const cksys_spi2_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d2",
	"cksys_univpll_192m",
	"cksys_mainpll_d6_d2",
	"cksys_univpll_d4_d4",
	"cksys_mainpll_d4_d4",
	"cksys_univpll_d5_d4",
	"cksys_univpll_d6_d4"
};

static const char * const cksys_spi3_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d2",
	"cksys_univpll_192m",
	"cksys_mainpll_d6_d2",
	"cksys_univpll_d4_d4",
	"cksys_mainpll_d4_d4",
	"cksys_univpll_d5_d4",
	"cksys_univpll_d6_d4"
};

static const char * const cksys_spi4_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d2",
	"cksys_univpll_192m",
	"cksys_mainpll_d6_d2",
	"cksys_univpll_d4_d4",
	"cksys_mainpll_d4_d4",
	"cksys_univpll_d5_d4",
	"cksys_univpll_d6_d4"
};

static const char * const cksys_spi5_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d2",
	"cksys_univpll_192m",
	"cksys_mainpll_d6_d2",
	"cksys_univpll_d4_d4",
	"cksys_mainpll_d4_d4",
	"cksys_univpll_d5_d4",
	"cksys_univpll_d6_d4"
};

static const char * const cksys_spi6_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d2",
	"cksys_univpll_192m",
	"cksys_mainpll_d6_d2",
	"cksys_univpll_d4_d4",
	"cksys_mainpll_d4_d4",
	"cksys_univpll_d5_d4",
	"cksys_univpll_d6_d4"
};

static const char * const cksys_spi7_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d2",
	"cksys_univpll_192m",
	"cksys_mainpll_d6_d2",
	"cksys_univpll_d4_d4",
	"cksys_mainpll_d4_d4",
	"cksys_univpll_d5_d4",
	"cksys_univpll_d6_d4"
};

static const char * const cksys_msdc_macro_1p_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_msdcpll",
	"cksys_msdcpll_d2",
	"cksys_mainpll_d6_d2",
	"cksys_univpll_d4_d4"
};

static const char * const cksys_msdc30_1_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d2",
	"cksys_mainpll_d6_d2",
	"cksys_mainpll_d7_d2",
	"cksys_msdcpll_d2"
};

static const char * const cksys_aud_intbus_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d7_d4"
};

static const char * const cksys_atb_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d5_d2"
};

static const char * const cksys_disp_pwm_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d4",
	"cksys_osc_d2",
	"cksys_osc_d4",
	"cksys_osc_d16",
	"cksys_univpll_d5_d4",
	"cksys_mainpll_d4_d4"
};

static const char * const cksys_usb_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d5_d4",
	"cksys_univpll_d6_d4"
};

static const char * const cksys_usb_xhci_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d5_d4",
	"cksys_univpll_d6_d4"
};

static const char * const cksys_i2c_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d8",
	"cksys_univpll_d4_d8",
	"cksys_mainpll_d6_d4",
	"cksys_univpll_d5_d4"
};

static const char * const cksys_i2c_5_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d8",
	"cksys_univpll_d4_d8",
	"cksys_mainpll_d6_d4",
	"cksys_univpll_d5_d4"
};

static const char * const cksys_seninf_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d2",
	"cksys_osc",
	"cksys_univpll_d4_d2",
	"cksys_mmpll_d4_d2",
	"cksys_univpll_d6_d4",
	"cksys_univpll_d6",
	"cksys_univpll_d5"
};

static const char * const cksys_seninf1_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d2",
	"cksys_osc",
	"cksys_univpll_d4_d2",
	"cksys_mmpll_d4_d2",
	"cksys_univpll_d6_d4",
	"cksys_univpll_d6",
	"cksys_univpll_d5"
};

static const char * const cksys_seninf2_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d2",
	"cksys_osc",
	"cksys_univpll_d4_d2",
	"cksys_mmpll_d4_d2",
	"cksys_univpll_d6_d4",
	"cksys_univpll_d6",
	"cksys_univpll_d5"
};

static const char * const cksys_seninf3_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d2",
	"cksys_osc",
	"cksys_univpll_d4_d2",
	"cksys_mmpll_d4_d2",
	"cksys_univpll_d6_d4",
	"cksys_univpll_d6",
	"cksys_univpll_d5"
};

static const char * const cksys_aud_engen1_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_apll1_d2",
	"cksys_apll1_d4",
	"cksys_apll1_d8"
};

static const char * const cksys_aud_engen2_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_apll2_d2",
	"cksys_apll2_d4",
	"cksys_apll2_d8"
};

static const char * const cksys_aes_ufsfde_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d6",
	"cksys_mainpll_d4_d4",
	"cksys_univpll_d4_d2",
	"cksys_univpll_d6"
};

static const char * const cksys_ufs_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d8",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d5_d2",
	"cksys_mainpll_d6_d2",
	"cksys_univpll_d6_d2",
	"cksys_msdcpll_d2"
};

static const char * const cksys_ufs_mbist_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d2",
	"cksys_univpll_d4_d2"
};

static const char * const cksys_aud_1_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_apll1"
};

static const char * const cksys_aud_2_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_apll2"
};

static const char * const cksys_dpmaif_main_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6",
	"cksys_mainpll_d5",
	"cksys_mainpll_d6",
	"cksys_mainpll_d4_d2",
	"cksys_univpll_d4_d2"
};

static const char * const cksys_venc_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mmpll_d4_d2",
	"cksys_mainpll_d6",
	"cksys_univpll_d4_d2",
	"cksys_mainpll_d4_d2",
	"cksys_univpll_d6",
	"cksys_mmpll_d6",
	"cksys_mainpll_d5_d2",
	"cksys_mainpll_d6_d2",
	"cksys_mmpll_d9",
	"cksys_mmpll_d4",
	"cksys_mainpll_d4",
	"cksys_univpll_d4",
	"cksys_univpll_d5",
	"cksys_univpll_d5_d2",
	"cksys_mainpll_d5"
};

static const char * const cksys_vdec_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_192m_d2",
	"cksys_univpll_192m",
	"cksys_mainpll_d5",
	"cksys_mainpll_d5_d2",
	"cksys_mmpll_d6_d2",
	"cksys_univpll_d5_d2",
	"cksys_mainpll_d4_d2",
	"cksys_univpll_d4_d2",
	"cksys_univpll_d7",
	"cksys_mmpll_d7",
	"cksys_univpll_d5",
	"cksys_univpll_d6",
	"cksys_mainpll_d4",
	"cksys_mmpll_d4_d2",
	"cksys_mmpll_d5_d2"
};

static const char * const cksys_pwm_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d4_d8"
};

static const char * const cksys_audio_h_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d7_d2",
	"cksys_apll1",
	"cksys_apll2"
};

static const char * const cksys_mcupm_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d2",
	"cksys_mainpll_d5_d2",
	"cksys_mainpll_d6_d2"
};

static const char * const cksys_mem_sub_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d4_d4",
	"cksys_mainpll_d6_d2",
	"cksys_mainpll_d5_d2",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d6",
	"cksys_mmpll_d7",
	"cksys_mainpll_d5",
	"cksys_univpll_d5",
	"cksys_mainpll_d4",
	"cksys_univpll_d4"
};

static const char * const cksys_mem_sub_peri_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d4_d4",
	"cksys_mainpll_d5_d2",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d6",
	"cksys_mainpll_d5",
	"cksys_univpll_d5",
	"cksys_mainpll_d4",
	"cksys_univpll_d4"
};

static const char * const cksys_mem_sub_ufs_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d4_d4",
	"cksys_mainpll_d5_d2",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d6",
	"cksys_mainpll_d5",
	"cksys_univpll_d5",
	"cksys_mainpll_d4"
};

static const char * const cksys_emisys_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d2",
	"cksys_mainpll_d6_d2",
	"cksys_mainpll_d9",
	"cksys_mainpll_d6",
	"cksys_mainpll_d5",
	"cksys_mainpll_d4",
	"cksys_mainpll_d3"
};

static const char * const cksys_dsi_occ_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d2",
	"cksys_univpll_d5_d2",
	"cksys_univpll_d4_d2"
};

static const char * const cksys_ap2conn_host_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d7_d4"
};

static const char * const cksys_img1_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d4",
	"cksys_mmpll_d5",
	"cksys_mmpll_d6",
	"cksys_univpll_d6",
	"cksys_mmpll_d7",
	"cksys_mmpll_d4_d2",
	"cksys_univpll_d4_d2",
	"cksys_mainpll_d4_d2",
	"cksys_mmpll_d6_d2",
	"cksys_mmpll_d5_d2"
};

static const char * const cksys_ipe_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d4",
	"cksys_mmpll_d5",
	"cksys_mmpll_d6",
	"cksys_mainpll_d4",
	"cksys_mainpll_d6",
	"cksys_mmpll_d4_d2",
	"cksys_univpll_d5",
	"cksys_mainpll_d4_d2",
	"cksys_mmpll_d6_d2",
	"cksys_mmpll_d5_d2"
};

static const char * const cksys_cam_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mmpll_d4",
	"cksys_univpll_d4",
	"cksys_mainpll_d4",
	"cksys_univpll_d5",
	"cksys_mmpll_d7",
	"cksys_mmpll_d6",
	"cksys_univpll_d6",
	"cksys_univpll_d4_d2",
	"cksys_mmpll_d9",
	"cksys_mmpll_d5_d2",
	"cksys_osc_d2"
};

static const char * const cksys_camtm_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d2",
	"cksys_univpll_d6_d2",
	"cksys_univpll_d6_d4"
};

static const char * const cksys_msdc_1p_rx_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d2",
	"cksys_mainpll_d6_d2",
	"cksys_mainpll_d7_d2",
	"cksys_msdcpll_d2"
};

static const char * const cksys_dsp_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d7_d2",
	"cksys_univpll_d6_d2",
	"cksys_univpll_d5_d2",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d7",
	"cksys_mainpll_d6",
	"cksys_univpll_d5"
};

static const char * const cksys_md_emi_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4"
};

static const char * const cksys_ssr_pka_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d7",
	"cksys_mainpll_d6",
	"cksys_mainpll_d5"
};

static const char * const cksys_ssr_dma_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d7",
	"cksys_mainpll_d6",
	"cksys_mainpll_d5"
};

static const char * const cksys_ssr_kdf_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d7"
};

static const char * const cksys_ssr_rng_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d5_d2",
	"cksys_mainpll_d4_d2"
};

static const char * const cksys_ssr_pqc_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d7",
	"cksys_mainpll_d5",
	"cksys_mainpll_d4",
	"cksys_mainpll_d3"
};

static const char * const cksys_mfg_ref_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d7_d2"
};

static const char * const cksys_mfgsc_ref_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d7_d2"
};

static const char * const cksys_mfg_eb_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d7_d2",
	"cksys_mainpll_d6_d2",
	"cksys_mainpll_d5_d2"
};

static const char * const cksys_spis0_b_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d7_d2",
	"cksys_univpll_d6_d2",
	"cksys_mainpll_d5_d2"
};

static const char * const cksys_spis1_b_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d7_d2",
	"cksys_univpll_d6_d2",
	"cksys_mainpll_d5_d2"
};

static const char * const cksys_spis0_deglitch_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d5",
	"cksys_univpll_d6"
};

static const char * const cksys_spis1_deglitch_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d5",
	"cksys_univpll_d6"
};

static const char * const cksys_tl_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d7_d4",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d5_d2"
};

static const char * const cksys_pextp_mbist_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d5_d2"
};

static const char * const cksys_usb_frmcnt_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_192m_d4"
};

static const char * const cksys_armpll_div_pll1_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d2"
};

static const char * const cksys_camtg0_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_192m_d8",
	"cksys_univpll_d6_d8",
	"cksys_univpll_192m_d4",
	"cksys_osc_d16",
	"cksys_osc_d20",
	"cksys_osc_d10",
	"cksys_univpll_d6_d16",
	"cksys_f26m_d2",
	"cksys_univpll_192m_d10",
	"cksys_univpll_192m_d16",
	"cksys_univpll_192m_d32"
};

static const char * const cksys_camtg1_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_192m_d8",
	"cksys_univpll_d6_d8",
	"cksys_univpll_192m_d4",
	"cksys_osc_d16",
	"cksys_osc_d20",
	"cksys_osc_d10",
	"cksys_univpll_d6_d16",
	"cksys_f26m_d2",
	"cksys_univpll_192m_d10",
	"cksys_univpll_192m_d16",
	"cksys_univpll_192m_d32"
};

static const char * const cksys_camtg2_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_192m_d8",
	"cksys_univpll_d6_d8",
	"cksys_univpll_192m_d4",
	"cksys_osc_d16",
	"cksys_osc_d20",
	"cksys_osc_d10",
	"cksys_univpll_d6_d16",
	"cksys_f26m_d2",
	"cksys_univpll_192m_d10",
	"cksys_univpll_192m_d16",
	"cksys_univpll_192m_d32"
};

static const char * const cksys_camtg3_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_192m_d8",
	"cksys_univpll_d6_d8",
	"cksys_univpll_192m_d4",
	"cksys_osc_d16",
	"cksys_osc_d20",
	"cksys_osc_d10",
	"cksys_univpll_d6_d16",
	"cksys_f26m_d2",
	"cksys_univpll_192m_d10",
	"cksys_univpll_192m_d16",
	"cksys_univpll_192m_d32"
};

static const char * const cksys_camtg4_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_192m_d8",
	"cksys_univpll_d6_d8",
	"cksys_univpll_192m_d4",
	"cksys_osc_d16",
	"cksys_osc_d20",
	"cksys_osc_d10",
	"cksys_univpll_d6_d16",
	"cksys_f26m_d2",
	"cksys_univpll_192m_d10",
	"cksys_univpll_192m_d16",
	"cksys_univpll_192m_d32"
};

static const char * const cksys_camtg5_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_192m_d8",
	"cksys_univpll_d6_d8",
	"cksys_univpll_192m_d4",
	"cksys_osc_d16",
	"cksys_osc_d20",
	"cksys_osc_d10",
	"cksys_univpll_d6_d16",
	"cksys_f26m_d2",
	"cksys_univpll_192m_d10",
	"cksys_univpll_192m_d16",
	"cksys_univpll_192m_d32"
};

static const char * const cksys_apll_in0_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_in1_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_in2_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_in3_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_in4_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_in6_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_out0_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_out1_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_out2_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_out3_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_out4_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_out6_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_fmi2s_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const struct mtk_mux cksys_reg_muxes[] = {
#if MT_CCF_MUX_DISABLE
	/* CLK_CFG_0 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_AXI_SEL/* dts */, "cksys_axi_sel",
		cksys_axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 31/* cksta shift */, CLK_FENC_STATUS_MON_0, 31),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_AXI_PERI_SEL/* dts */, "cksys_axi_peri_sel",
		cksys_axi_peri_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_PERI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 30/* cksta shift */, CLK_FENC_STATUS_MON_0, 30),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_AXI_UFS_SEL/* dts */, "cksys_axi_ufs_sel",
		cksys_axi_ufs_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_UFS_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 29/* cksta shift */, CLK_FENC_STATUS_MON_0, 29),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_BUS_AXIMEM_SEL/* dts */, "cksys_bus_aximem_sel",
		cksys_bus_aximem_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_BUS_AXIMEM_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 28/* cksta shift */, CLK_FENC_STATUS_MON_0, 28),
	/* CLK_CFG_1 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_DISP0_SEL/* dts */, "cksys_disp0_sel",
		cksys_disp0_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DISP0_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 27/* cksta shift */, CLK_FENC_STATUS_MON_0, 27),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MMINFRA_SEL/* dts */, "cksys_mminfra_sel",
		cksys_mminfra_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMINFRA_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 26/* cksta shift */, CLK_FENC_STATUS_MON_0, 26),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MMUP_SEL/* dts */, "cksys_mmup_sel",
		cksys_mmup_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMUP_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 25/* cksta shift */, CLK_FENC_STATUS_MON_0, 25),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_UART_SEL/* dts */, "cksys_uart_sel",
		cksys_uart_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UART_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 24/* cksta shift */, CLK_FENC_STATUS_MON_0, 24),
	/* CLK_CFG_2 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_UART3_SEL/* dts */, "cksys_uart3_sel",
		cksys_uart3_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UART3_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 23/* cksta shift */, CLK_FENC_STATUS_MON_0, 23),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_UARTHUB_BCLK_SEL/* dts */, "cksys_uarthub_b_sel",
		cksys_uarthub_b_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UARTHUB_BCLK_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 22/* cksta shift */, CLK_FENC_STATUS_MON_0, 22),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPI0_SEL/* dts */, "cksys_spi0_sel",
		cksys_spi0_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI0_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 21/* cksta shift */, CLK_FENC_STATUS_MON_0, 21),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPI1_SEL/* dts */, "cksys_spi1_sel",
		cksys_spi1_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI1_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 20/* cksta shift */, CLK_FENC_STATUS_MON_0, 20),
	/* CLK_CFG_3 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPI2_SEL/* dts */, "cksys_spi2_sel",
		cksys_spi2_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI2_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 19/* cksta shift */, CLK_FENC_STATUS_MON_0, 19),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPI3_SEL/* dts */, "cksys_spi3_sel",
		cksys_spi3_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI3_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 18/* cksta shift */, CLK_FENC_STATUS_MON_0, 18),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPI4_SEL/* dts */, "cksys_spi4_sel",
		cksys_spi4_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI4_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 17/* cksta shift */, CLK_FENC_STATUS_MON_0, 17),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPI5_SEL/* dts */, "cksys_spi5_sel",
		cksys_spi5_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI5_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 16/* cksta shift */, CLK_FENC_STATUS_MON_0, 16),
	/* CLK_CFG_4 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPI6_SEL/* dts */, "cksys_spi6_sel",
		cksys_spi6_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI6_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 15/* cksta shift */, CLK_FENC_STATUS_MON_0, 15),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPI7_SEL/* dts */, "cksys_spi7_sel",
		cksys_spi7_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI7_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 14/* cksta shift */, CLK_FENC_STATUS_MON_0, 14),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MSDC_MACRO_1P_SEL/* dts */, "cksys_msdc_macro_1p_sel",
		cksys_msdc_macro_1p_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC_MACRO_1P_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 13/* cksta shift */, CLK_FENC_STATUS_MON_0, 13),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MSDC30_1_SEL/* dts */, "cksys_msdc30_1_sel",
		cksys_msdc30_1_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC30_1_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 12/* cksta shift */, CLK_FENC_STATUS_MON_0, 12),
	/* CLK_CFG_5 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_AUD_INTBUS_SEL/* dts */, "cksys_aud_intbus_sel",
		cksys_aud_intbus_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AUD_INTBUS_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 11/* cksta shift */, CLK_FENC_STATUS_MON_0, 11),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_ATB_SEL/* dts */, "cksys_atb_sel",
		cksys_atb_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ATB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 10/* cksta shift */, CLK_FENC_STATUS_MON_0, 10),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_DISP_PWM_SEL/* dts */, "cksys_disp_pwm_sel",
		cksys_disp_pwm_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DISP_PWM_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 9/* cksta shift */, CLK_FENC_STATUS_MON_0, 9),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_USB_TOP_SEL/* dts */, "cksys_usb_sel",
		cksys_usb_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_USB_TOP_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 8/* cksta shift */, CLK_FENC_STATUS_MON_0, 8),
	/* CLK_CFG_6 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_USB_XHCI_SEL/* dts */, "cksys_usb_xhci_sel",
		cksys_usb_xhci_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSUSB_XHCI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 7/* cksta shift */, CLK_FENC_STATUS_MON_0, 7),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_I2C_SEL/* dts */, "cksys_i2c_sel",
		cksys_i2c_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_I2C_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 6/* cksta shift */, CLK_FENC_STATUS_MON_0, 6),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_I2C_5_SEL/* dts */, "cksys_i2c_5_sel",
		cksys_i2c_5_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_I2C_5_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 5/* cksta shift */, CLK_FENC_STATUS_MON_0, 5),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SENINF_SEL/* dts */, "cksys_seninf_sel",
		cksys_seninf_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 4/* cksta shift */, CLK_FENC_STATUS_MON_0, 4),
	/* CLK_CFG_7 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SENINF1_SEL/* dts */, "cksys_seninf1_sel",
		cksys_seninf1_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF1_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 3/* cksta shift */, CLK_FENC_STATUS_MON_0, 3),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SENINF2_SEL/* dts */, "cksys_seninf2_sel",
		cksys_seninf2_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF2_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 2/* cksta shift */, CLK_FENC_STATUS_MON_0, 2),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SENINF3_SEL/* dts */, "cksys_seninf3_sel",
		cksys_seninf3_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF3_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 1/* cksta shift */, CLK_FENC_STATUS_MON_0, 1),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_AUD_ENGEN1_SEL/* dts */, "cksys_aud_engen1_sel",
		cksys_aud_engen1_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_ENGEN1_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 31/* cksta shift */, CLK_FENC_STATUS_MON_0, 0),
	/* CLK_CFG_8 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_AUD_ENGEN2_SEL/* dts */, "cksys_aud_engen2_sel",
		cksys_aud_engen2_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_ENGEN2_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 30/* cksta shift */, CLK_FENC_STATUS_MON_1, 31),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_AES_UFSFDE_SEL/* dts */, "cksys_aes_ufsfde_sel",
		cksys_aes_ufsfde_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AES_UFSFDE_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 29/* cksta shift */, CLK_FENC_STATUS_MON_1, 30),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_UFS_SEL/* dts */, "cksys_ufs_sel",
		cksys_ufs_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_UFS_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 28/* cksta shift */, CLK_FENC_STATUS_MON_1, 29),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_UFS_MBIST_SEL/* dts */, "cksys_ufs_mbist_sel",
		cksys_ufs_mbist_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_UFS_MBIST_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 27/* cksta shift */, CLK_FENC_STATUS_MON_1, 28),
	/* CLK_CFG_9 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_AUD_1_SEL/* dts */, "cksys_aud_1_sel",
		cksys_aud_1_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_1_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 26/* cksta shift */, CLK_FENC_STATUS_MON_1, 27),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_AUD_2_SEL/* dts */, "cksys_aud_2_sel",
		cksys_aud_2_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_2_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 25/* cksta shift */, CLK_FENC_STATUS_MON_1, 26),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_DPMAIF_MAIN_SEL/* dts */, "cksys_dpmaif_main_sel",
		cksys_dpmaif_main_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DPMAIF_MAIN_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 24/* cksta shift */, CLK_FENC_STATUS_MON_1, 25),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_VENC_SEL/* dts */, "cksys_venc_sel",
		cksys_venc_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_VENC_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 23/* cksta shift */, CLK_FENC_STATUS_MON_1, 24),
	/* CLK_CFG_10 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_VDEC_SEL/* dts */, "cksys_vdec_sel",
		cksys_vdec_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_VDEC_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 22/* cksta shift */, CLK_FENC_STATUS_MON_1, 23),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_PWM_SEL/* dts */, "cksys_pwm_sel",
		cksys_pwm_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_PWM_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 21/* cksta shift */, CLK_FENC_STATUS_MON_1, 22),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_AUDIO_H_SEL/* dts */, "cksys_audio_h_sel",
		cksys_audio_h_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUDIO_H_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 20/* cksta shift */, CLK_FENC_STATUS_MON_1, 21),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MCUPM_SEL/* dts */, "cksys_mcupm_sel",
		cksys_mcupm_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MCUPM_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 19/* cksta shift */, CLK_FENC_STATUS_MON_1, 20),
	/* CLK_CFG_11 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MEM_SUB_SEL/* dts */, "cksys_mem_sub_sel",
		cksys_mem_sub_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 18/* cksta shift */, CLK_FENC_STATUS_MON_1, 19),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MEM_SUB_PERI_SEL/* dts */, "cksys_mem_sub_peri_sel",
		cksys_mem_sub_peri_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MEM_SUB_PERI_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 17/* cksta shift */, CLK_FENC_STATUS_MON_1, 18),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MEM_SUB_UFS_SEL/* dts */, "cksys_mem_sub_ufs_sel",
		cksys_mem_sub_ufs_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MEM_SUB_UFS_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 16/* cksta shift */, CLK_FENC_STATUS_MON_1, 17),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_EMISYS_SEL/* dts */, "cksys_emisys_sel",
		cksys_emisys_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_EMISYS_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 15/* cksta shift */, CLK_FENC_STATUS_MON_1, 16),
	/* CLK_CFG_12 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_DSI_OCC_SEL/* dts */, "cksys_dsi_occ_sel",
		cksys_dsi_occ_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DSI_OCC_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 14/* cksta shift */, CLK_FENC_STATUS_MON_1, 15),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_AP2CONN_HOST_SEL/* dts */, "cksys_ap2conn_host_sel",
		cksys_ap2conn_host_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AP2CONN_HOST_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 13/* cksta shift */, CLK_FENC_STATUS_MON_1, 14),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_IMG1_SEL/* dts */, "cksys_img1_sel",
		cksys_img1_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_IMG1_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 12/* cksta shift */, CLK_FENC_STATUS_MON_1, 13),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_IPE_SEL/* dts */, "cksys_ipe_sel",
		cksys_ipe_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_IPE_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 11/* cksta shift */, CLK_FENC_STATUS_MON_1, 12),
	/* CLK_CFG_13 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_CAM_SEL/* dts */, "cksys_cam_sel",
		cksys_cam_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_CAM_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 10/* cksta shift */, CLK_FENC_STATUS_MON_1, 11),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_CAMTM_SEL/* dts */, "cksys_camtm_sel",
		cksys_camtm_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_CAMTM_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 9/* cksta shift */, CLK_FENC_STATUS_MON_1, 10),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MSDC_1P_RX_SEL/* dts */, "cksys_msdc_1p_rx_sel",
		cksys_msdc_1p_rx_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MSDC_1P_RX_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 8/* cksta shift */, CLK_FENC_STATUS_MON_1, 9),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_DSP_SEL/* dts */, "cksys_dsp_sel",
		cksys_dsp_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DSP_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 7/* cksta shift */, CLK_FENC_STATUS_MON_1, 8),
	/* CLK_CFG_14 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_EMI_INTERFACE_546_SEL/* dts */, "cksys_md_emi_sel",
		cksys_md_emi_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_EMI_INTERFACE_546_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 6/* cksta shift */, CLK_FENC_STATUS_MON_1, 7),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SSR_PKA_SEL/* dts */, "cksys_ssr_pka_sel",
		cksys_ssr_pka_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SSR_PKA_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 5/* cksta shift */, CLK_FENC_STATUS_MON_1, 6),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SSR_DMA_SEL/* dts */, "cksys_ssr_dma_sel",
		cksys_ssr_dma_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SSR_DMA_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 4/* cksta shift */, CLK_FENC_STATUS_MON_1, 5),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SSR_KDF_SEL/* dts */, "cksys_ssr_kdf_sel",
		cksys_ssr_kdf_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SSR_KDF_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 3/* cksta shift */, CLK_FENC_STATUS_MON_1, 4),
	/* CLK_CFG_15 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SSR_RNG_SEL/* dts */, "cksys_ssr_rng_sel",
		cksys_ssr_rng_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SSR_RNG_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 2/* cksta shift */, CLK_FENC_STATUS_MON_1, 3),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SSR_PQC_SEL/* dts */, "cksys_ssr_pqc_sel",
		cksys_ssr_pqc_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SSR_PQC_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 1/* cksta shift */, CLK_FENC_STATUS_MON_1, 2),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MFG_REF_SEL/* dts */, "cksys_mfg_ref_sel",
		cksys_mfg_ref_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_MFG_REF_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 31/* cksta shift */, CLK_FENC_STATUS_MON_1, 1),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MFGSC_REF_SEL/* dts */, "cksys_mfgsc_ref_sel",
		cksys_mfgsc_ref_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_MFGSC_REF_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 30/* cksta shift */, CLK_FENC_STATUS_MON_1, 0),
	/* CLK_CFG_16 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MFG_EB_SEL/* dts */, "cksys_mfg_eb_sel",
		cksys_mfg_eb_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_MFG_EB_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 29/* cksta shift */, CLK_FENC_STATUS_MON_2, 31),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPIS0_BCLK_SEL/* dts */, "cksys_spis0_b_sel",
		cksys_spis0_b_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPIS0_BCLK_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 28/* cksta shift */, CLK_FENC_STATUS_MON_2, 30),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPIS1_BCLK_SEL/* dts */, "cksys_spis1_b_sel",
		cksys_spis1_b_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPIS1_BCLK_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 27/* cksta shift */, CLK_FENC_STATUS_MON_2, 29),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPIS0_DEGLITCH_SEL/* dts */, "cksys_spis0_deglitch_sel",
		cksys_spis0_deglitch_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPIS0_DEGLITCH_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 26/* cksta shift */, CLK_FENC_STATUS_MON_2, 28),
	/* CLK_CFG_17 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPIS1_DEGLITCH_SEL/* dts */, "cksys_spis1_deglitch_sel",
		cksys_spis1_deglitch_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPIS1_DEGLITCH_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 25/* cksta shift */, CLK_FENC_STATUS_MON_2, 27),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_TL_SEL/* dts */, "cksys_tl_sel",
		cksys_tl_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_TL_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 24/* cksta shift */, CLK_FENC_STATUS_MON_2, 26),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_PEXTP_MBIST_SEL/* dts */, "cksys_pextp_mbist_sel",
		cksys_pextp_mbist_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_PEXTP_MBIST_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 23/* cksta shift */, CLK_FENC_STATUS_MON_2, 25),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_USB_FRMCNT_SEL/* dts */, "cksys_usb_frmcnt_sel",
		cksys_usb_frmcnt_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSUSB_FRMCNT_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 22/* cksta shift */, CLK_FENC_STATUS_MON_2, 24),
	/* CLK_CFG_18 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_ARMPLL_DIVIDER_PLL1_SEL/* dts */, "cksys_armpll_div_pll1_sel",
		cksys_armpll_div_pll1_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_ARMPLL_DIVIDER_PLL1_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 21/* cksta shift */, CLK_FENC_STATUS_MON_2, 23),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_CAMTG0_SEL/* dts */, "cksys_camtg0_sel",
		cksys_camtg0_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_CAMTG0_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 20/* cksta shift */, CLK_FENC_STATUS_MON_2, 22),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_CAMTG1_SEL/* dts */, "cksys_camtg1_sel",
		cksys_camtg1_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_CAMTG1_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 19/* cksta shift */, CLK_FENC_STATUS_MON_2, 21),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_CAMTG2_SEL/* dts */, "cksys_camtg2_sel",
		cksys_camtg2_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_CAMTG2_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 18/* cksta shift */, CLK_FENC_STATUS_MON_2, 20),
	/* CLK_CFG_19 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_CAMTG3_SEL/* dts */, "cksys_camtg3_sel",
		cksys_camtg3_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_CAMTG3_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 17/* cksta shift */, CLK_FENC_STATUS_MON_2, 19),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_CAMTG4_SEL/* dts */, "cksys_camtg4_sel",
		cksys_camtg4_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_CAMTG4_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 16/* cksta shift */, CLK_FENC_STATUS_MON_2, 18),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_CAMTG5_SEL/* dts */, "cksys_camtg5_sel",
		cksys_camtg5_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_CAMTG5_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 15/* cksta shift */, CLK_FENC_STATUS_MON_2, 17),
#else
	/* CLK_CFG_0 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_AXI_SEL/* dts */, "cksys_axi_sel",
		cksys_axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 31/* cksta shift */, CLK_FENC_STATUS_MON_0, 31),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_AXI_PERI_SEL/* dts */, "cksys_axi_peri_sel",
		cksys_axi_peri_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_PERI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 30/* cksta shift */, CLK_FENC_STATUS_MON_0, 30),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_AXI_UFS_SEL/* dts */, "cksys_axi_ufs_sel",
		cksys_axi_ufs_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_UFS_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 29/* cksta shift */, CLK_FENC_STATUS_MON_0, 29),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_BUS_AXIMEM_SEL/* dts */, "cksys_bus_aximem_sel",
		cksys_bus_aximem_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_BUS_AXIMEM_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 28/* cksta shift */, CLK_FENC_STATUS_MON_0, 28),
	/* CLK_CFG_1 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_DISP0_SEL/* dts */, "cksys_disp0_sel",
		cksys_disp0_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DISP0_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		27/* cksta shift */, CLK_FENC_STATUS_MON_0, 27),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MMINFRA_SEL/* dts */, "cksys_mminfra_sel",
		cksys_mminfra_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MMINFRA_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		26/* cksta shift */, CLK_FENC_STATUS_MON_0, 26),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MMUP_SEL/* dts */, "cksys_mmup_sel",
		cksys_mmup_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MMUP_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		25/* cksta shift */, CLK_FENC_STATUS_MON_0, 25),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_UART_SEL/* dts */, "cksys_uart_sel",
		cksys_uart_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_UART_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		24/* cksta shift */, CLK_FENC_STATUS_MON_0, 24),
	/* CLK_CFG_2 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_UART3_SEL/* dts */, "cksys_uart3_sel",
		cksys_uart3_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_UART3_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		23/* cksta shift */, CLK_FENC_STATUS_MON_0, 23),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_UARTHUB_BCLK_SEL/* dts */, "cksys_uarthub_b_sel",
		cksys_uarthub_b_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UARTHUB_BCLK_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 22/* cksta shift */, CLK_FENC_STATUS_MON_0, 22),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPI0_SEL/* dts */, "cksys_spi0_sel",
		cksys_spi0_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI0_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		21/* cksta shift */, CLK_FENC_STATUS_MON_0, 21),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPI1_SEL/* dts */, "cksys_spi1_sel",
		cksys_spi1_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI1_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		20/* cksta shift */, CLK_FENC_STATUS_MON_0, 20),
	/* CLK_CFG_3 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPI2_SEL/* dts */, "cksys_spi2_sel",
		cksys_spi2_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI2_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		19/* cksta shift */, CLK_FENC_STATUS_MON_0, 19),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPI3_SEL/* dts */, "cksys_spi3_sel",
		cksys_spi3_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI3_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		18/* cksta shift */, CLK_FENC_STATUS_MON_0, 18),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPI4_SEL/* dts */, "cksys_spi4_sel",
		cksys_spi4_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI4_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		17/* cksta shift */, CLK_FENC_STATUS_MON_0, 17),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPI5_SEL/* dts */, "cksys_spi5_sel",
		cksys_spi5_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI5_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		16/* cksta shift */, CLK_FENC_STATUS_MON_0, 16),
	/* CLK_CFG_4 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPI6_SEL/* dts */, "cksys_spi6_sel",
		cksys_spi6_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI6_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		15/* cksta shift */, CLK_FENC_STATUS_MON_0, 15),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPI7_SEL/* dts */, "cksys_spi7_sel",
		cksys_spi7_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI7_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		14/* cksta shift */, CLK_FENC_STATUS_MON_0, 14),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MSDC_MACRO_1P_SEL/* dts */, "cksys_msdc_macro_1p_sel",
		cksys_msdc_macro_1p_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC_MACRO_1P_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		13/* cksta shift */, CLK_FENC_STATUS_MON_0, 13),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MSDC30_1_SEL/* dts */, "cksys_msdc30_1_sel",
		cksys_msdc30_1_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC30_1_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		12/* cksta shift */, CLK_FENC_STATUS_MON_0, 12),
	/* CLK_CFG_5 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_AUD_INTBUS_SEL/* dts */, "cksys_aud_intbus_sel",
		cksys_aud_intbus_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AUD_INTBUS_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		11/* cksta shift */, CLK_FENC_STATUS_MON_0, 11),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_ATB_SEL/* dts */, "cksys_atb_sel",
		cksys_atb_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ATB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 10/* cksta shift */, CLK_FENC_STATUS_MON_0, 10),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_DISP_PWM_SEL/* dts */, "cksys_disp_pwm_sel",
		cksys_disp_pwm_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DISP_PWM_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 9/* cksta shift */, CLK_FENC_STATUS_MON_0, 9),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_USB_TOP_SEL/* dts */, "cksys_usb_sel",
		cksys_usb_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_USB_TOP_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		8/* cksta shift */, CLK_FENC_STATUS_MON_0, 8),
	/* CLK_CFG_6 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_USB_XHCI_SEL/* dts */, "cksys_usb_xhci_sel",
		cksys_usb_xhci_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SSUSB_XHCI_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		7/* cksta shift */, CLK_FENC_STATUS_MON_0, 7),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_I2C_SEL/* dts */, "cksys_i2c_sel",
		cksys_i2c_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_I2C_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		6/* cksta shift */, CLK_FENC_STATUS_MON_0, 6),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_I2C_5_SEL/* dts */, "cksys_i2c_5_sel",
		cksys_i2c_5_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_I2C_5_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		5/* cksta shift */, CLK_FENC_STATUS_MON_0, 5),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SENINF_SEL/* dts */, "cksys_seninf_sel",
		cksys_seninf_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		4/* cksta shift */, CLK_FENC_STATUS_MON_0, 4),
	/* CLK_CFG_7 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SENINF1_SEL/* dts */, "cksys_seninf1_sel",
		cksys_seninf1_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF1_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		3/* cksta shift */, CLK_FENC_STATUS_MON_0, 3),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SENINF2_SEL/* dts */, "cksys_seninf2_sel",
		cksys_seninf2_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF2_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		2/* cksta shift */, CLK_FENC_STATUS_MON_0, 2),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SENINF3_SEL/* dts */, "cksys_seninf3_sel",
		cksys_seninf3_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF3_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		1/* cksta shift */, CLK_FENC_STATUS_MON_0, 1),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_AUD_ENGEN1_SEL/* dts */, "cksys_aud_engen1_sel",
		cksys_aud_engen1_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_ENGEN1_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		31/* cksta shift */, CLK_FENC_STATUS_MON_0, 0),
	/* CLK_CFG_8 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_AUD_ENGEN2_SEL/* dts */, "cksys_aud_engen2_sel",
		cksys_aud_engen2_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_ENGEN2_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		30/* cksta shift */, CLK_FENC_STATUS_MON_1, 31),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_AES_UFSFDE_SEL/* dts */, "cksys_aes_ufsfde_sel",
		cksys_aes_ufsfde_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AES_UFSFDE_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		29/* cksta shift */, CLK_FENC_STATUS_MON_1, 30),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_UFS_SEL/* dts */, "cksys_ufs_sel",
		cksys_ufs_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_UFS_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		28/* cksta shift */, CLK_FENC_STATUS_MON_1, 29),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_UFS_MBIST_SEL/* dts */, "cksys_ufs_mbist_sel",
		cksys_ufs_mbist_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_UFS_MBIST_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		27/* cksta shift */, CLK_FENC_STATUS_MON_1, 28),
	/* CLK_CFG_9 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_AUD_1_SEL/* dts */, "cksys_aud_1_sel",
		cksys_aud_1_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 0/* lsb */, 1/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_1_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		26/* cksta shift */, CLK_FENC_STATUS_MON_1, 27),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_AUD_2_SEL/* dts */, "cksys_aud_2_sel",
		cksys_aud_2_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_2_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		25/* cksta shift */, CLK_FENC_STATUS_MON_1, 26),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_DPMAIF_MAIN_SEL/* dts */, "cksys_dpmaif_main_sel",
		cksys_dpmaif_main_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_DPMAIF_MAIN_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		24/* cksta shift */, CLK_FENC_STATUS_MON_1, 25),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_VENC_SEL/* dts */, "cksys_venc_sel",
		cksys_venc_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_VENC_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		23/* cksta shift */, CLK_FENC_STATUS_MON_1, 24),
	/* CLK_CFG_10 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_VDEC_SEL/* dts */, "cksys_vdec_sel",
		cksys_vdec_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_VDEC_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		22/* cksta shift */, CLK_FENC_STATUS_MON_1, 23),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_PWM_SEL/* dts */, "cksys_pwm_sel",
		cksys_pwm_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_PWM_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		21/* cksta shift */, CLK_FENC_STATUS_MON_1, 22),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_AUDIO_H_SEL/* dts */, "cksys_audio_h_sel",
		cksys_audio_h_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUDIO_H_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		20/* cksta shift */, CLK_FENC_STATUS_MON_1, 21),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MCUPM_SEL/* dts */, "cksys_mcupm_sel",
		cksys_mcupm_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MCUPM_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 19/* cksta shift */, CLK_FENC_STATUS_MON_1, 20),
	/* CLK_CFG_11 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MEM_SUB_SEL/* dts */, "cksys_mem_sub_sel",
		cksys_mem_sub_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 18/* cksta shift */, CLK_FENC_STATUS_MON_1, 19),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MEM_SUB_PERI_SEL/* dts */, "cksys_mem_sub_peri_sel",
		cksys_mem_sub_peri_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MEM_SUB_PERI_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 17/* cksta shift */, CLK_FENC_STATUS_MON_1, 18),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MEM_SUB_UFS_SEL/* dts */, "cksys_mem_sub_ufs_sel",
		cksys_mem_sub_ufs_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MEM_SUB_UFS_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 16/* cksta shift */, CLK_FENC_STATUS_MON_1, 17),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_EMISYS_SEL/* dts */, "cksys_emisys_sel",
		cksys_emisys_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_EMISYS_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 15/* cksta shift */, CLK_FENC_STATUS_MON_1, 16),
	/* CLK_CFG_12 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_DSI_OCC_SEL/* dts */, "cksys_dsi_occ_sel",
		cksys_dsi_occ_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_DSI_OCC_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		14/* cksta shift */, CLK_FENC_STATUS_MON_1, 15),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_AP2CONN_HOST_SEL/* dts */, "cksys_ap2conn_host_sel",
		cksys_ap2conn_host_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AP2CONN_HOST_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 13/* cksta shift */, CLK_FENC_STATUS_MON_1, 14),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_IMG1_SEL/* dts */, "cksys_img1_sel",
		cksys_img1_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 16/* lsb */, 4/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_IMG1_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		12/* cksta shift */, CLK_FENC_STATUS_MON_1, 13),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_IPE_SEL/* dts */, "cksys_ipe_sel",
		cksys_ipe_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_IPE_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		11/* cksta shift */, CLK_FENC_STATUS_MON_1, 12),
	/* CLK_CFG_13 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_CAM_SEL/* dts */, "cksys_cam_sel",
		cksys_cam_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_CAM_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		10/* cksta shift */, CLK_FENC_STATUS_MON_1, 11),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_CAMTM_SEL/* dts */, "cksys_camtm_sel",
		cksys_camtm_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_CAMTM_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		9/* cksta shift */, CLK_FENC_STATUS_MON_1, 10),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MSDC_1P_RX_SEL/* dts */, "cksys_msdc_1p_rx_sel",
		cksys_msdc_1p_rx_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MSDC_1P_RX_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 8/* cksta shift */, CLK_FENC_STATUS_MON_1, 9),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_DSP_SEL/* dts */, "cksys_dsp_sel",
		cksys_dsp_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DSP_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 7/* cksta shift */, CLK_FENC_STATUS_MON_1, 8),
	/* CLK_CFG_14 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_EMI_INTERFACE_546_SEL/* dts */, "cksys_md_emi_sel",
		cksys_md_emi_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_EMI_INTERFACE_546_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 6/* cksta shift */, CLK_FENC_STATUS_MON_1, 7),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SSR_PKA_SEL/* dts */, "cksys_ssr_pka_sel",
		cksys_ssr_pka_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSR_PKA_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		5/* cksta shift */, CLK_FENC_STATUS_MON_1, 6),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SSR_DMA_SEL/* dts */, "cksys_ssr_dma_sel",
		cksys_ssr_dma_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSR_DMA_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		4/* cksta shift */, CLK_FENC_STATUS_MON_1, 5),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SSR_KDF_SEL/* dts */, "cksys_ssr_kdf_sel",
		cksys_ssr_kdf_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSR_KDF_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		3/* cksta shift */, CLK_FENC_STATUS_MON_1, 4),
	/* CLK_CFG_15 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SSR_RNG_SEL/* dts */, "cksys_ssr_rng_sel",
		cksys_ssr_rng_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSR_RNG_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		2/* cksta shift */, CLK_FENC_STATUS_MON_1, 3),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SSR_PQC_SEL/* dts */, "cksys_ssr_pqc_sel",
		cksys_ssr_pqc_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSR_PQC_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		1/* cksta shift */, CLK_FENC_STATUS_MON_1, 2),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MFG_REF_SEL/* dts */, "cksys_mfg_ref_sel",
		cksys_mfg_ref_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_MFG_REF_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 31/* cksta shift */, CLK_FENC_STATUS_MON_1, 1),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MFGSC_REF_SEL/* dts */, "cksys_mfgsc_ref_sel",
		cksys_mfgsc_ref_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_MFGSC_REF_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 30/* cksta shift */, CLK_FENC_STATUS_MON_1, 0),
	/* CLK_CFG_16 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_MFG_EB_SEL/* dts */, "cksys_mfg_eb_sel",
		cksys_mfg_eb_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_MFG_EB_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 29/* cksta shift */, CLK_FENC_STATUS_MON_2, 31),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPIS0_BCLK_SEL/* dts */, "cksys_spis0_b_sel",
		cksys_spis0_b_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPIS0_BCLK_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 28/* cksta shift */, CLK_FENC_STATUS_MON_2, 30),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPIS1_BCLK_SEL/* dts */, "cksys_spis1_b_sel",
		cksys_spis1_b_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPIS1_BCLK_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 27/* cksta shift */, CLK_FENC_STATUS_MON_2, 29),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPIS0_DEGLITCH_SEL/* dts */, "cksys_spis0_deglitch_sel",
		cksys_spis0_deglitch_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPIS0_DEGLITCH_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 26/* cksta shift */, CLK_FENC_STATUS_MON_2, 28),
	/* CLK_CFG_17 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_SPIS1_DEGLITCH_SEL/* dts */, "cksys_spis1_deglitch_sel",
		cksys_spis1_deglitch_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPIS1_DEGLITCH_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 25/* cksta shift */, CLK_FENC_STATUS_MON_2, 27),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_TL_SEL/* dts */, "cksys_tl_sel",
		cksys_tl_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_TL_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 24/* cksta shift */, CLK_FENC_STATUS_MON_2, 26),
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_PEXTP_MBIST_SEL/* dts */, "cksys_pextp_mbist_sel",
		cksys_pextp_mbist_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_PEXTP_MBIST_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 23/* cksta shift */, CLK_FENC_STATUS_MON_2, 25),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_USB_FRMCNT_SEL/* dts */, "cksys_usb_frmcnt_sel",
		cksys_usb_frmcnt_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 24/* lsb */, 1/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_SSUSB_FRMCNT_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		22/* cksta shift */, CLK_FENC_STATUS_MON_2, 24),
	/* CLK_CFG_18 */
	MUX_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_ARMPLL_DIVIDER_PLL1_SEL/* dts */, "cksys_armpll_div_pll1_sel",
		cksys_armpll_div_pll1_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_ARMPLL_DIVIDER_PLL1_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 21/* cksta shift */, CLK_FENC_STATUS_MON_2, 23),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_CAMTG0_SEL/* dts */, "cksys_camtg0_sel",
		cksys_camtg0_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_CAMTG0_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		20/* cksta shift */, CLK_FENC_STATUS_MON_2, 22),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_CAMTG1_SEL/* dts */, "cksys_camtg1_sel",
		cksys_camtg1_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 16/* lsb */, 4/* width */,
		23/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_CAMTG1_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		19/* cksta shift */, CLK_FENC_STATUS_MON_2, 21),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_CAMTG2_SEL/* dts */, "cksys_camtg2_sel",
		cksys_camtg2_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_CAMTG2_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		18/* cksta shift */, CLK_FENC_STATUS_MON_2, 20),
	/* CLK_CFG_19 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_CAMTG3_SEL/* dts */, "cksys_camtg3_sel",
		cksys_camtg3_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_CAMTG3_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		17/* cksta shift */, CLK_FENC_STATUS_MON_2, 19),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_CAMTG4_SEL/* dts */, "cksys_camtg4_sel",
		cksys_camtg4_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_CAMTG4_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		16/* cksta shift */, CLK_FENC_STATUS_MON_2, 18),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CKSYS_REG_CAMTG5_SEL/* dts */, "cksys_camtg5_sel",
		cksys_camtg5_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 16/* lsb */, 4/* width */,
		23/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_CAMTG5_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		15/* cksta shift */, CLK_FENC_STATUS_MON_2, 17),
#endif
};

static const char * const vlp_scp_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d4",
	"cksys_univpll_d3",
	"cksys_mainpll_d3",
	"cksys_univpll_d6",
	"cksys_apll1",
	"cksys_mainpll_d4",
	"cksys_mainpll_d7",
	"cksys_mainpll_d2",
	"cksys_osc_d10"
};

static const char * const vlp_pwrap_ulposc_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d10",
	"cksys_osc_d16",
	"cksys_osc_d7"
};

static const char * const vlp_spmi_p_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d10",
	"cksys_osc_d16",
	"cksys_osc_d7"
};

static const char * const vlp_spmi_m_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d10",
	"cksys_osc_d16",
	"cksys_osc_d7"
};

static const char * const vlp_dvfsrc_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d10"
};

static const char * const vlp_pwm_vlp_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d4",
	"cksys_clkrtc",
	"cksys_osc_d10",
	"cksys_mainpll_d4_d8"
};

static const char * const vlp_axi_vlp_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d10",
	"cksys_osc_d2",
	"cksys_mainpll_d7_d4",
	"cksys_mainpll_d7_d2"
};

static const char * const vlp_dbgao_26m_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d10"
};

static const char * const vlp_systimer_26m_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d10"
};

static const char * const vlp_sspm_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d10",
	"cksys_mainpll_d5_d2",
	"cksys_osc",
	"cksys_mainpll_d6"
};

static const char * const vlp_sspm_f26m_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d10"
};

static const char * const vlp_srck_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d10"
};

static const char * const vlp_sramrc_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d10"
};

static const char * const vlp_scp_spi_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d5_d4",
	"cksys_mainpll_d7_d2",
	"cksys_osc_d10"
};

static const char * const vlp_scp_iic_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d5_d4",
	"cksys_mainpll_d7_d2",
	"cksys_osc_d10"
};

static const char * const vlp_scp_spi_hs_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d5_d4",
	"cksys_mainpll_d7_d2",
	"cksys_osc_d10"
};

static const char * const vlp_scp_iic_hs_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d5_d4",
	"cksys_mainpll_d7_d2",
	"cksys_osc_d10"
};

static const char * const vlp_sspm_ulposc_parents[] = {
	"cksys_osc",
	"cksys_univpll_d5_d2"
};

static const char * const vlp_tia_ulposc_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d10",
	"cksys_osc_d16",
	"cksys_osc_d7"
};

static const char * const vlp_apxgpt_26m_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d10"
};

static const struct mtk_mux vlp_cksys_top_muxes[] = {
#if MT_CCF_MUX_DISABLE
	/* VLP_CLK_CFG_0 */
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SCP_SEL/* dts */, "vlp_scp_sel",
		vlp_scp_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_PWRAP_ULPOSC_SEL/* dts */, "vlp_pwrap_ulposc_sel",
		vlp_pwrap_ulposc_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWRAP_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SPMI_P_MST_SEL/* dts */, "vlp_spmi_p_sel",
		vlp_spmi_p_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_P_MST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SPMI_M_MST_SEL/* dts */, "vlp_spmi_m_sel",
		vlp_spmi_m_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_M_MST_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_1 */
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_DVFSRC_SEL/* dts */, "vlp_dvfsrc_sel",
		vlp_dvfsrc_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DVFSRC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_PWM_VLP_SEL/* dts */, "vlp_pwm_vlp_sel",
		vlp_pwm_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWM_VLP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_AXI_VLP_SEL/* dts */, "vlp_axi_vlp_sel",
		vlp_axi_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_VLP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_DBGAO_26M_SEL/* dts */, "vlp_dbgao_26m_sel",
		vlp_dbgao_26m_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DBGAO_26M_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_2 */
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SYSTIMER_26M_SEL/* dts */, "vlp_systimer_26m_sel",
		vlp_systimer_26m_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SYSTIMER_26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SSPM_SEL/* dts */, "vlp_sspm_sel",
		vlp_sspm_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SSPM_F26M_SEL/* dts */, "vlp_sspm_f26m_sel",
		vlp_sspm_f26m_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_F26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SRCK_SEL/* dts */, "vlp_srck_sel",
		vlp_srck_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRCK_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_3 */
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SRAMRC_SEL/* dts */, "vlp_sramrc_sel",
		vlp_sramrc_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRAMRC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SCP_SPI_SEL/* dts */, "vlp_scp_spi_sel",
		vlp_scp_spi_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SPI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SCP_IIC_SEL/* dts */, "vlp_scp_iic_sel",
		vlp_scp_iic_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SCP_SPI_HIGH_SPD_SEL/* dts */, "vlp_scp_spi_hs_sel",
		vlp_scp_spi_hs_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SPI_HIGH_SPD_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_4 */
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SCP_IIC_HIGH_SPD_SEL/* dts */, "vlp_scp_iic_hs_sel",
		vlp_scp_iic_hs_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SSPM_ULPOSC_SEL/* dts */, "vlp_sspm_ulposc_sel",
		vlp_sspm_ulposc_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_TIA_ULPOSC_SEL/* dts */, "vlp_tia_ulposc_sel",
		vlp_tia_ulposc_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_TIA_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_APXGPT_26M_SEL/* dts */, "vlp_apxgpt_26m_sel",
		vlp_apxgpt_26m_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_APXGPT_26M_SHIFT/* upd shift */),
#else
	/* VLP_CLK_CFG_0 */
	MUX_GATE_FENC_CLR_SET_UPD_NCHK(CLK_VLP_CKSYS_TOP_SCP_SEL/* dts */, "vlp_scp_sel",
		vlp_scp_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SCP_SHIFT/* upd shift */, VLP_OCIC_FENC_STATUS_MON_0, 31),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_PWRAP_ULPOSC_SEL/* dts */, "vlp_pwrap_ulposc_sel",
		vlp_pwrap_ulposc_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWRAP_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SPMI_P_MST_SEL/* dts */, "vlp_spmi_p_sel",
		vlp_spmi_p_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_P_MST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SPMI_M_MST_SEL/* dts */, "vlp_spmi_m_sel",
		vlp_spmi_m_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_M_MST_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_1 */
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_DVFSRC_SEL/* dts */, "vlp_dvfsrc_sel",
		vlp_dvfsrc_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DVFSRC_SHIFT/* upd shift */),
	MUX_GATE_FENC_CLR_SET_UPD_NCHK(CLK_VLP_CKSYS_TOP_PWM_VLP_SEL/* dts */, "vlp_pwm_vlp_sel",
		vlp_pwm_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_PWM_VLP_SHIFT/* upd shift */, VLP_OCIC_FENC_STATUS_MON_0, 26),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_AXI_VLP_SEL/* dts */, "vlp_axi_vlp_sel",
		vlp_axi_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_VLP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_DBGAO_26M_SEL/* dts */, "vlp_dbgao_26m_sel",
		vlp_dbgao_26m_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DBGAO_26M_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_2 */
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SYSTIMER_26M_SEL/* dts */, "vlp_systimer_26m_sel",
		vlp_systimer_26m_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SYSTIMER_26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SSPM_SEL/* dts */, "vlp_sspm_sel",
		vlp_sspm_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SSPM_F26M_SEL/* dts */, "vlp_sspm_f26m_sel",
		vlp_sspm_f26m_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_F26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SRCK_SEL/* dts */, "vlp_srck_sel",
		vlp_srck_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRCK_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_3 */
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SRAMRC_SEL/* dts */, "vlp_sramrc_sel",
		vlp_sramrc_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRAMRC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SCP_SPI_SEL/* dts */, "vlp_scp_spi_sel",
		vlp_scp_spi_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SPI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SCP_IIC_SEL/* dts */, "vlp_scp_iic_sel",
		vlp_scp_iic_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SCP_SPI_HIGH_SPD_SEL/* dts */, "vlp_scp_spi_hs_sel",
		vlp_scp_spi_hs_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SPI_HIGH_SPD_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_4 */
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SCP_IIC_HIGH_SPD_SEL/* dts */, "vlp_scp_iic_hs_sel",
		vlp_scp_iic_hs_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_SSPM_ULPOSC_SEL/* dts */, "vlp_sspm_ulposc_sel",
		vlp_sspm_ulposc_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_TIA_ULPOSC_SEL/* dts */, "vlp_tia_ulposc_sel",
		vlp_tia_ulposc_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_TIA_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FENC_NCHK(CLK_VLP_CKSYS_TOP_APXGPT_26M_SEL/* dts */, "vlp_apxgpt_26m_sel",
		vlp_apxgpt_26m_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_APXGPT_26M_SHIFT/* upd shift */),
#endif
};

static const struct mtk_composite top_composites[] = {
	/* CLK_AUDDIV_0 */
	MUX(CLK_CKSYS_REG_APLL_IN0_MCK_SEL/* dts */, "cksys_apll_in0_m_sel",
		cksys_apll_in0_m_parents/* parent */, 0x0320/* ofs */,
		16/* lsb */, 1/* width */),
	MUX(CLK_CKSYS_REG_APLL_IN1_MCK_SEL/* dts */, "cksys_apll_in1_m_sel",
		cksys_apll_in1_m_parents/* parent */, 0x0320/* ofs */,
		17/* lsb */, 1/* width */),
	MUX(CLK_CKSYS_REG_APLL_IN2_MCK_SEL/* dts */, "cksys_apll_in2_m_sel",
		cksys_apll_in2_m_parents/* parent */, 0x0320/* ofs */,
		18/* lsb */, 1/* width */),
	MUX(CLK_CKSYS_REG_APLL_IN3_MCK_SEL/* dts */, "cksys_apll_in3_m_sel",
		cksys_apll_in3_m_parents/* parent */, 0x0320/* ofs */,
		19/* lsb */, 1/* width */),
	MUX(CLK_CKSYS_REG_APLL_IN4_MCK_SEL/* dts */, "cksys_apll_in4_m_sel",
		cksys_apll_in4_m_parents/* parent */, 0x0320/* ofs */,
		20/* lsb */, 1/* width */),
	MUX(CLK_CKSYS_REG_APLL_IN6_MCK_SEL/* dts */, "cksys_apll_in6_m_sel",
		cksys_apll_in6_m_parents/* parent */, 0x0320/* ofs */,
		21/* lsb */, 1/* width */),
	MUX(CLK_CKSYS_REG_APLL_OUT0_MCK_SEL/* dts */, "cksys_apll_out0_m_sel",
		cksys_apll_out0_m_parents/* parent */, 0x0320/* ofs */,
		22/* lsb */, 1/* width */),
	MUX(CLK_CKSYS_REG_APLL_OUT1_MCK_SEL/* dts */, "cksys_apll_out1_m_sel",
		cksys_apll_out1_m_parents/* parent */, 0x0320/* ofs */,
		23/* lsb */, 1/* width */),
	MUX(CLK_CKSYS_REG_APLL_OUT2_MCK_SEL/* dts */, "cksys_apll_out2_m_sel",
		cksys_apll_out2_m_parents/* parent */, 0x0320/* ofs */,
		24/* lsb */, 1/* width */),
	MUX(CLK_CKSYS_REG_APLL_OUT3_MCK_SEL/* dts */, "cksys_apll_out3_m_sel",
		cksys_apll_out3_m_parents/* parent */, 0x0320/* ofs */,
		25/* lsb */, 1/* width */),
	MUX(CLK_CKSYS_REG_APLL_OUT4_MCK_SEL/* dts */, "cksys_apll_out4_m_sel",
		cksys_apll_out4_m_parents/* parent */, 0x0320/* ofs */,
		26/* lsb */, 1/* width */),
	MUX(CLK_CKSYS_REG_APLL_OUT6_MCK_SEL/* dts */, "cksys_apll_out6_m_sel",
		cksys_apll_out6_m_parents/* parent */, 0x0320/* ofs */,
		27/* lsb */, 1/* width */),
	MUX(CLK_CKSYS_REG_APLL_FMI2S_MCK_SEL/* dts */, "cksys_apll_fmi2s_m_sel",
		cksys_apll_fmi2s_m_parents/* parent */, 0x0320/* ofs */,
		28/* lsb */, 1/* width */),
	MUX(CLK_CKSYS_REG_APLL_MCK_SEL/* dts */, "cksys_apll_m_sel",
		cksys_apll_m_parents/* parent */, 0x0320/* ofs */,
		29/* lsb */, 1/* width */),
	/* CLK_AUDDIV_2 */
	DIV_GATE(CLK_CKSYS_REG_APLL12_CK_DIV_IN0/* dts */, "cksys_apll12_div_in0"/* ccf */,
		"cksys_apll_in0_m_sel"/* parent */, 0x0320/* pdn ofs */,
		0/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_CKSYS_REG_APLL12_CK_DIV_IN1/* dts */, "cksys_apll12_div_in1"/* ccf */,
		"cksys_apll_in1_m_sel"/* parent */, 0x0320/* pdn ofs */,
		1/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_CKSYS_REG_APLL12_CK_DIV_IN2/* dts */, "cksys_apll12_div_in2"/* ccf */,
		"cksys_apll_in2_m_sel"/* parent */, 0x0320/* pdn ofs */,
		2/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		16/* lsb */),
	DIV_GATE(CLK_CKSYS_REG_APLL12_CK_DIV_IN3/* dts */, "cksys_apll12_div_in3"/* ccf */,
		"cksys_apll_in3_m_sel"/* parent */, 0x0320/* pdn ofs */,
		3/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		24/* lsb */),
	/* CLK_AUDDIV_3 */
	DIV_GATE(CLK_CKSYS_REG_APLL12_CK_DIV_IN4/* dts */, "cksys_apll12_div_in4"/* ccf */,
		"cksys_apll_in4_m_sel"/* parent */, 0x0320/* pdn ofs */,
		4/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_CKSYS_REG_APLL12_CK_DIV_IN6/* dts */, "cksys_apll12_div_in6"/* ccf */,
		"cksys_apll_in6_m_sel"/* parent */, 0x0320/* pdn ofs */,
		5/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_CKSYS_REG_APLL12_CK_DIV_OUT0/* dts */, "cksys_apll12_div_out0"/* ccf */,
		"cksys_apll_out0_m_sel"/* parent */, 0x0320/* pdn ofs */,
		6/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		16/* lsb */),
	DIV_GATE(CLK_CKSYS_REG_APLL12_CK_DIV_OUT1/* dts */, "cksys_apll12_div_out1"/* ccf */,
		"cksys_apll_out1_m_sel"/* parent */, 0x0320/* pdn ofs */,
		7/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		24/* lsb */),
	/* CLK_AUDDIV_4 */
	DIV_GATE(CLK_CKSYS_REG_APLL12_CK_DIV_OUT2/* dts */, "cksys_apll12_div_out2"/* ccf */,
		"cksys_apll_out2_m_sel"/* parent */, 0x0320/* pdn ofs */,
		8/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_CKSYS_REG_APLL12_CK_DIV_OUT3/* dts */, "cksys_apll12_div_out3"/* ccf */,
		"cksys_apll_out3_m_sel"/* parent */, 0x0320/* pdn ofs */,
		9/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_CKSYS_REG_APLL12_CK_DIV_OUT4/* dts */, "cksys_apll12_div_out4"/* ccf */,
		"cksys_apll_out4_m_sel"/* parent */, 0x0320/* pdn ofs */,
		10/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		16/* lsb */),
	DIV_GATE(CLK_CKSYS_REG_APLL12_CK_DIV_OUT6/* dts */, "cksys_apll12_div_out6"/* ccf */,
		"cksys_apll_out6_m_sel"/* parent */, 0x0320/* pdn ofs */,
		11/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		24/* lsb */),
	/* CLK_AUDDIV_5 */
	DIV_GATE(CLK_CKSYS_REG_APLL12_CK_DIV_FMI2S/* dts */, "cksys_apll12_div_fmi2s"/* ccf */,
		"cksys_apll_fmi2s_m_sel"/* parent */, 0x0320/* pdn ofs */,
		12/* pdn bit */, CLK_AUDDIV_5/* ofs */, 8/* width */,
		0/* lsb */),
};

enum subsys_id {
	APMIXEDSYS = 0,
	PLL_SYS_NUM,
};

static const struct mtk_pll_data *plls_data[PLL_SYS_NUM];
static void __iomem *plls_base[PLL_SYS_NUM];

#define MT6881_PLL_FMAX		(3800UL * MHZ)
#define MT6881_PLL_FMIN		(1500UL * MHZ)
#define MT6881_INTEGER_BITS	8

#if MT_CCF_PLL_DISABLE
#define PLL_CFLAGS		PLL_AO
#else
#define PLL_CFLAGS		(0)
#endif

#define PLL_FENC(_id, _name, _fenc_sta_ofs, _fenc_sta_bit,		\
			_flags, _pd_reg, _pd_shift,			\
			 _pcw_reg, _pcw_shift, _pcwbits) {		\
		.id = _id,						\
		.name = _name,						\
		.reg = 0,						\
		.fenc_sta_ofs = _fenc_sta_ofs,				\
		.fenc_sta_bit = _fenc_sta_bit,				\
		.flags = (_flags | PLL_CFLAGS),				\
		.fmax = MT6881_PLL_FMAX,				\
		.fmin = MT6881_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6881_INTEGER_BITS,			\
		.ops = &mtk_pll_fenc_ops,				\
	}


static const struct mtk_pll_data apmixed_plls[] = {
	PLL_FENC(CLK_APMIXED_MAINPLL, "mainpll",
		FENC_ENABLE_CON0/*fenc*/, 7, HAVE_RST_BAR,
		MAINPLL_CON1, 24/*pd*/,
		MAINPLL_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED_UNIVPLL, "univpll",
		FENC_ENABLE_CON0/*fenc*/, 6, HAVE_RST_BAR,
		UNIVPLL_CON1, 24/*pd*/,
		UNIVPLL_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED_MSDCPLL, "msdcpll",
		FENC_ENABLE_CON0/*fenc*/, 5, 0,
		MSDCPLL_CON1, 24/*pd*/,
		MSDCPLL_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED_MMPLL, "mmpll",
		FENC_ENABLE_CON0/*fenc*/, 3, HAVE_RST_BAR,
		MMPLL_CON1, 24/*pd*/,
		MMPLL_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED_EMIPLL, "emipll",
		FENC_ENABLE_CON0/*fenc*/, 4, 0,
		EMIPLL_CON1, 24/*pd*/,
		EMIPLL_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED_TVDPLL, "tvdpll",
		FENC_ENABLE_CON0/*fenc*/, 2, HAVE_RST_BAR,
		TVDPLL_CON1, 24/*pd*/,
		TVDPLL_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED_APLL1, "apll1",
		FENC_ENABLE_CON0/*fenc*/, 1, 0,
		APLL1_CON1, 24/*pd*/,
		APLL1_CON2, 0, 32/*pcw*/),
	PLL_FENC(CLK_APMIXED_APLL2, "apll2",
		FENC_ENABLE_CON0/*fenc*/, 0, 0,
		APLL2_CON1, 24/*pd*/,
	APLL2_CON2, 0, 32/*pcw*/),
};

static int clk_mt6881_pll_registration(enum subsys_id id,
		const struct mtk_pll_data *plls,
		struct platform_device *pdev,
		int num_plls)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	if (id >= PLL_SYS_NUM) {
		pr_notice("%s init invalid id(%d)\n", __func__, id);
		return 0;
	}

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(num_plls);

	mtk_clk_register_plls(node, plls, num_plls,
			clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	plls_data[id] = plls;
	plls_base[id] = base;

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

#if IS_ENABLED(CONFIG_MTK_CLKMGR_FTRACE)
static int clk_set_trace_event(struct platform_device *pdev)
{
	trace_set_clr_event(NULL, "clk_disable", 1);
	trace_set_clr_event(NULL, "clk_disable_complete", 1);
	trace_set_clr_event(NULL, "clk_enable", 1);
	trace_set_clr_event(NULL, "clk_enable_complete", 1);
	trace_set_clr_event(NULL, "clk_prepare", 1);
	trace_set_clr_event(NULL, "clk_prepare_complete", 1);
	trace_set_clr_event(NULL, "clk_set_parent", 1);
	trace_set_clr_event(NULL, "clk_set_parent_complete", 1);
	trace_set_clr_event(NULL, "clk_set_rate", 1);
	trace_set_clr_event(NULL, "clk_set_rate_complete", 1);
	trace_set_clr_event(NULL, "clk_unprepare", 1);
	trace_set_clr_event(NULL, "clk_unprepare_complete", 1);
	trace_set_clr_event(NULL, "rpm_idle", 1);
	trace_set_clr_event(NULL, "rpm_resume", 1);
	trace_set_clr_event(NULL, "rpm_return_int", 1);
	trace_set_clr_event(NULL, "rpm_suspend", 1);
	trace_set_clr_event(NULL, "rpm_usage", 1);
	pr_notice("%s init end\n",__func__);

	return 0;
}
#endif

static int clk_mt6881_apmixed_probe(struct platform_device *pdev)
{
	return clk_mt6881_pll_registration(APMIXEDSYS, apmixed_plls,
			pdev, ARRAY_SIZE(apmixed_plls));
}

static int clk_mt6881_cksys_reg_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_CKSYS_REG_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_factors(cksys_reg_divs, ARRAY_SIZE(cksys_reg_divs),
			clk_data);

	mtk_clk_register_muxes(cksys_reg_muxes, ARRAY_SIZE(cksys_reg_muxes), node,
			&mt6881_clk_lock, clk_data);

	mtk_clk_register_composites(top_composites, ARRAY_SIZE(top_composites),
			base, &mt6881_clk_lock, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6881_vlp_cksys_top_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_VLP_CKSYS_TOP_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_muxes(vlp_cksys_top_muxes, ARRAY_SIZE(vlp_cksys_top_muxes), node,
			&mt6881_clk_lock, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

/* for suspend LDVT only */
static void pll_force_off_internal(const struct mtk_pll_data *plls,
		void __iomem *base)
{
	void __iomem *rst_reg, *en_reg, *pwr_reg;

	for (; plls->name; plls++) {
		/* do not pwrdn the AO PLLs */
		if ((plls->flags & PLL_AO) == PLL_AO)
			continue;

		if ((plls->flags & HAVE_RST_BAR) == HAVE_RST_BAR) {
			rst_reg = base + plls->en_reg;
			writel(readl(rst_reg) & ~plls->rst_bar_mask,
				rst_reg);
		}

		en_reg = base + plls->en_reg;

		pwr_reg = base + plls->pwr_reg;

		writel(readl(en_reg) & ~plls->en_mask,
				en_reg);
		writel(readl(pwr_reg) | (0x2),
				pwr_reg);
		writel(readl(pwr_reg) & ~(0x1),
				pwr_reg);
	}
}

void mt6881_pll_force_off(void)
{
	int i;

	for (i = 0; i < PLL_SYS_NUM; i++)
		pll_force_off_internal(plls_data[i], plls_base[i]);
}
EXPORT_SYMBOL_GPL(mt6881_pll_force_off);

static const struct of_device_id of_match_clk_mt6881[] = {
#if IS_ENABLED(CONFIG_MTK_CLKMGR_FTRACE)
	{
		.compatible = "clk-ftrace",
		.data = clk_set_trace_event,
	},
#endif
	{
		.compatible = "mediatek,mt6881-apmixedsys",
		.data = clk_mt6881_apmixed_probe,
	}, {
		.compatible = "mediatek,mt6881-topckgen",
		.data = clk_mt6881_cksys_reg_probe,
	}, {
		.compatible = "mediatek,mt6881-vlp_cksys_top",
		.data = clk_mt6881_vlp_cksys_top_probe,
	}, {
		/* sentinel */
	}
};

static int clk_mt6881_probe(struct platform_device *pdev)
{
	int (*clk_probe)(struct platform_device *pd);
	int r;

	clk_probe = of_device_get_match_data(&pdev->dev);
	if (!clk_probe)
		return -EINVAL;

	r = clk_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt6881_drv = {
	.probe = clk_mt6881_probe,
	.driver = {
		.name = "clk-mt6881",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6881,
	},
};

module_platform_driver(clk_mt6881_drv);
MODULE_LICENSE("GPL");

