// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: KY Liu <ky.liu@mediatek.com>
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

#include <dt-bindings/clock/mt6858-clk.h>

/* bringup config */
#define MT_CCF_BRINGUP		1
#define MT_CCF_PLL_DISABLE	1
#define MT_CCF_MUX_DISABLE	1

/* Regular Number Definition */
#define INV_OFS	-1
#define INV_BIT	-1

/* TOPCK MUX SEL REG */
#define CLK_CFG_UPDATE				0x0004
#define CLK_CFG_UPDATE1				0x0008
#define CLK_CFG_UPDATE				0x0004
#define VLP_CLK_CFG_UPDATE			0x0004
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
#define VLP_CLK_CFG_6				0x0050
#define VLP_CLK_CFG_6_SET				0x0054
#define VLP_CLK_CFG_6_CLR				0x0058

/* TOPCK MUX SHIFT */
#define TOP_MUX_AXI_SHIFT			0
#define TOP_MUX_AXI_PERI_SHIFT			1
#define TOP_MUX_AXI_UFS_SHIFT			2
#define TOP_MUX_BUS_AXIMEM_SHIFT		3
#define TOP_MUX_DISP0_SHIFT			4
#define TOP_MUX_MMINFRA_SHIFT			5
#define TOP_MUX_MMUP_SHIFT			6
#define TOP_MUX_UART_SHIFT			7
#define TOP_MUX_SPI0_SHIFT			8
#define TOP_MUX_SPI1_SHIFT			9
#define TOP_MUX_SPI2_SHIFT			10
#define TOP_MUX_SPI3_SHIFT			11
#define TOP_MUX_SPI4_SHIFT			12
#define TOP_MUX_SPI5_SHIFT			13
#define TOP_MUX_SPI6_SHIFT			14
#define TOP_MUX_SPI7_SHIFT			15
#define TOP_MUX_MSDC_MACRO_1P_SHIFT		16
#define TOP_MUX_MSDC30_1_SHIFT			17
#define TOP_MUX_MSDC30_1_HCLK_SHIFT		18
#define TOP_MUX_AUD_INTBUS_SHIFT		19
#define TOP_MUX_ATB_SHIFT			20
#define TOP_MUX_DISP_PWM_SHIFT			21
#define TOP_MUX_USB_TOP_SHIFT			22
#define TOP_MUX_SSUSB_XHCI_SHIFT		23
#define TOP_MUX_I2C_SHIFT			24
#define TOP_MUX_SENINF_SHIFT			25
#define TOP_MUX_SENINF1_SHIFT			26
#define TOP_MUX_SENINF2_SHIFT			27
#define TOP_MUX_AUD_ENGEN1_SHIFT		28
#define TOP_MUX_AUD_ENGEN2_SHIFT		29
#define TOP_MUX_AES_UFSFDE_SHIFT		30
#define TOP_MUX_UFS_SHIFT			0
#define TOP_MUX_AUD_1_SHIFT			2
#define TOP_MUX_AUD_2_SHIFT			3
#define TOP_MUX_VENC_SHIFT			5
#define TOP_MUX_VDEC_SHIFT			6
#define TOP_MUX_PWM_SHIFT			7
#define TOP_MUX_AUDIO_H_SHIFT			8
#define TOP_MUX_MCUPM_SHIFT			9
#define TOP_MUX_MEM_SUB_SHIFT			10
#define TOP_MUX_MEM_SUB_PERI_SHIFT		11
#define TOP_MUX_MEM_SUB_UFS_SHIFT		12
#define TOP_MUX_EMI_N_SHIFT			13
#define TOP_MUX_DSI_OCC_SHIFT			14
#define TOP_MUX_AP2CONN_HOST_SHIFT		15
#define TOP_MUX_IMG1_SHIFT			16
#define TOP_MUX_IPE_SHIFT			17
#define TOP_MUX_CAM_SHIFT			18
#define TOP_MUX_CAMTM_SHIFT			19
#define TOP_MUX_EMI_INTERFACE_546_SHIFT		20
#define TOP_MUX_MFG_REF_SHIFT			22
#define TOP_MUX_MFGSC_REF_SHIFT			23
#define TOP_MUX_EFUSE_SHIFT			24
#define TOP_MUX_UNIPLL_SES_SHIFT		26
#define TOP_MUX_DRAMULP_SHIFT			28
#define TOP_MUX_SSUSB_FRMCNT_SHIFT		29
#define TOP_MUX_SCP_SHIFT			0
#define TOP_MUX_PWRAP_ULPOSC_SHIFT		1
#define TOP_MUX_SPMI_P_MST_SHIFT		2
#define TOP_MUX_SPMI_M_MST_SHIFT		3
#define TOP_MUX_DVFSRC_SHIFT			4
#define TOP_MUX_PWM_VLP_SHIFT			5
#define TOP_MUX_AXI_VLP_SHIFT			6
#define TOP_MUX_SYSTIMER_26M_SHIFT		7
#define TOP_MUX_SSPM_SHIFT			8
#define TOP_MUX_SSPM_F26M_SHIFT			9
#define TOP_MUX_SRCK_SHIFT			10
#define TOP_MUX_SCP_SPI_SHIFT			11
#define TOP_MUX_SCP_IIC_SHIFT			12
#define TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT		14
#define TOP_MUX_SSPM_ULPOSC_SHIFT		15
#define TOP_MUX_TIA_ULPOSC_SHIFT		16
#define TOP_MUX_APXGPT_26M_SHIFT		17
#define TOP_MUX_CAMTG0_SHIFT			18
#define TOP_MUX_CAMTG1_SHIFT			19
#define TOP_MUX_CAMTG2_SHIFT			20
#define TOP_MUX_CAMTG3_SHIFT			21
#define TOP_MUX_KP_IRQ_GEN_SHIFT		22
#define TOP_MUX_SSR_PKA_SHIFT			23
#define TOP_MUX_SSR_DMA_SHIFT			24
#define TOP_MUX_SSR_KDF_SHIFT			25
#define TOP_MUX_SSR_RNG_SHIFT			26

/* TOPCK CKSTA REG */
#define CKSTA_REG				0x0230
#define CKSTA_REG2				0x0238

/* TOPCK FENC REG */
#define CLK_FENC_STATUS_MON_0			0x0534
#define CLK_FENC_STATUS_MON_1			0x0538
#define VLP_OCIC_FENC_STATUS_MON_0		0x0328

/* TOPCK DIVIDER REG */
#define CLK_AUDDIV_2				0x0328

/* APMIXED PLL REG */
#define AP_PLL_CON3				0x00C
#define APLL1_TUNER_CON0			0x040
#define APLL2_TUNER_CON0			0x044
#define ARMPLL_LL_CON0				0x208
#define ARMPLL_LL_CON1				0x20C
#define ARMPLL_LL_CON2				0x210
#define ARMPLL_LL_CON3				0x214
#define ARMPLL_BL_CON0				0x218
#define ARMPLL_BL_CON1				0x21C
#define ARMPLL_BL_CON2				0x220
#define ARMPLL_BL_CON3				0x224
#define CCIPLL_CON0				0x238
#define CCIPLL_CON1				0x23C
#define CCIPLL_CON2				0x240
#define CCIPLL_CON3				0x244
#define MAINPLL_CON0				0x350
#define MAINPLL_CON1				0x354
#define MAINPLL_CON2				0x358
#define MAINPLL_CON3				0x35C
#define UNIVPLL_CON0				0x308
#define UNIVPLL_CON1				0x30C
#define UNIVPLL_CON2				0x310
#define UNIVPLL_CON3				0x314
#define MSDCPLL_CON0				0x360
#define MSDCPLL_CON1				0x364
#define MSDCPLL_CON2				0x368
#define MSDCPLL_CON3				0x36C
#define MMPLL_CON0				0x3A0
#define MMPLL_CON1				0x3A4
#define MMPLL_CON2				0x3A8
#define MMPLL_CON3				0x3AC
#define TVDPLL_CON0				0x248
#define TVDPLL_CON1				0x24C
#define TVDPLL_CON2				0x250
#define TVDPLL_CON3				0x254
#define EMIPLL_CON0				0x3B0
#define EMIPLL_CON1				0x3B4
#define EMIPLL_CON2				0x3B8
#define EMIPLL_CON3				0x3BC
#define ADSPPLL_CON0				0x380
#define ADSPPLL_CON1				0x384
#define ADSPPLL_CON2				0x388
#define ADSPPLL_CON3				0x38C
#define APLL1_CON0				0x328
#define APLL1_CON1				0x32C
#define APLL1_CON2				0x330
#define APLL1_CON3				0x334
#define APLL2_CON0				0x33C
#define APLL2_CON1				0x340
#define APLL2_CON2				0x344
#define APLL2_CON3				0x348

/* HW Voter REG */


static DEFINE_SPINLOCK(mt6858_clk_lock);

static const struct mtk_fixed_factor top_divs[] = {
	FACTOR(CLK_TOP_MAINPLL, "top_mainpll",
			"mainpll", 1, 1),
	FACTOR(CLK_TOP_MAINPLL_D3, "top_mainpll_d3",
			"mainpll", 1, 3),
	FACTOR(CLK_TOP_MAINPLL_D4, "top_mainpll_d4",
			"mainpll", 1, 4),
	FACTOR(CLK_TOP_MAINPLL_D4_D2, "top_mainpll_d4_d2",
			"mainpll", 1, 8),
	FACTOR(CLK_TOP_MAINPLL_D4_D4, "top_mainpll_d4_d4",
			"mainpll", 1, 16),
	FACTOR(CLK_TOP_MAINPLL_D4_D8, "top_mainpll_d4_d8",
			"mainpll", 1, 32),
	FACTOR(CLK_TOP_MAINPLL_D5, "top_mainpll_d5",
			"mainpll", 1, 5),
	FACTOR(CLK_TOP_MAINPLL_D5_D2, "top_mainpll_d5_d2",
			"mainpll", 1, 10),
	FACTOR(CLK_TOP_MAINPLL_D5_D4, "top_mainpll_d5_d4",
			"mainpll", 1, 20),
	FACTOR(CLK_TOP_MAINPLL_D5_D8, "top_mainpll_d5_d8",
			"mainpll", 1, 40),
	FACTOR(CLK_TOP_MAINPLL_D6, "top_mainpll_d6",
			"mainpll", 1, 6),
	FACTOR(CLK_TOP_MAINPLL_D6_D2, "top_mainpll_d6_d2",
			"mainpll", 1, 12),
	FACTOR(CLK_TOP_MAINPLL_D6_D8, "top_mainpll_d6_d8",
			"mainpll", 1, 48),
	FACTOR(CLK_TOP_MAINPLL_D7, "top_mainpll_d7",
			"mainpll", 1, 7),
	FACTOR(CLK_TOP_MAINPLL_D7_D2, "top_mainpll_d7_d2",
			"mainpll", 1, 14),
	FACTOR(CLK_TOP_MAINPLL_D7_D4, "top_mainpll_d7_d4",
			"mainpll", 1, 28),
	FACTOR(CLK_TOP_MAINPLL_D7_D8, "top_mainpll_d7_d8",
			"mainpll", 1, 56),
	FACTOR(CLK_TOP_UNIVPLL, "top_univpll",
			"univpll", 1, 1),
	FACTOR(CLK_TOP_UNIVPLL_D2, "top_univpll_d2",
			"univpll", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL_D3, "top_univpll_d3",
			"univpll", 1, 3),
	FACTOR(CLK_TOP_UNIVPLL_D4, "top_univpll_d4",
			"univpll", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL_D4_D2, "top_univpll_d4_d2",
			"univpll", 1, 8),
	FACTOR(CLK_TOP_UNIVPLL_D4_D4, "top_univpll_d4_d4",
			"univpll", 1, 16),
	FACTOR(CLK_TOP_UNIVPLL_D4_D8, "top_univpll_d4_d8",
			"univpll", 1, 32),
	FACTOR(CLK_TOP_UNIVPLL_D5, "top_univpll_d5",
			"univpll", 1, 5),
	FACTOR(CLK_TOP_UNIVPLL_D5_D2, "top_univpll_d5_d2",
			"univpll", 1, 10),
	FACTOR(CLK_TOP_UNIVPLL_D5_D4, "top_univpll_d5_d4",
			"univpll", 1, 20),
	FACTOR(CLK_TOP_UNIVPLL_D6, "top_univpll_d6",
			"univpll", 1, 6),
	FACTOR(CLK_TOP_UNIVPLL_D6_D2, "top_univpll_d6_d2",
			"univpll", 1, 12),
	FACTOR(CLK_TOP_UNIVPLL_D6_D4, "top_univpll_d6_d4",
			"univpll", 1, 24),
	FACTOR(CLK_TOP_UNIVPLL_D6_D8, "top_univpll_d6_d8",
			"univpll", 1, 48),
	FACTOR(CLK_TOP_UNIVPLL_D6_D16, "top_univpll_d6_d16",
			"univpll", 1, 96),
	FACTOR(CLK_TOP_UNIVPLL_D7, "top_univpll_d7",
			"univpll", 1, 7),
	FACTOR(CLK_TOP_UNIVPLL_D7_D2, "top_univpll_d7_d2",
			"univpll", 1, 14),
	FACTOR(CLK_TOP_UNIVPLL_192M, "top_univpll_192m",
			"univpll", 1, 13),
	FACTOR(CLK_TOP_UNIVPLL_192M_D2, "top_univpll_192m_d2",
			"univpll", 1, 26),
	FACTOR(CLK_TOP_UNIVPLL_192M_D4, "top_univpll_192m_d4",
			"univpll", 1, 52),
	FACTOR(CLK_TOP_UNIVPLL_192M_D8, "top_univpll_192m_d8",
			"univpll", 1, 104),
	FACTOR(CLK_TOP_UNIVPLL_192M_D16, "top_univpll_192m_d16",
			"univpll", 1, 208),
	FACTOR(CLK_TOP_UNIVPLL_192M_D32, "top_univpll_192m_d32",
			"univpll", 1, 416),
	FACTOR(CLK_TOP_UNIVPLL_192M_D10, "top_univpll_192m_d10",
			"univpll", 1, 130),
	FACTOR(CLK_TOP_APLL1, "top_apll1_ck",
			"apll1", 1, 1),
	FACTOR(CLK_TOP_APLL1_D2, "top_apll1_d2",
			"apll1", 1, 2),
	FACTOR(CLK_TOP_APLL1_D4, "top_apll1_d4",
			"apll1", 1, 4),
	FACTOR(CLK_TOP_APLL1_D8, "top_apll1_d8",
			"apll1", 1, 8),
	FACTOR(CLK_TOP_APLL2, "top_apll2_ck",
			"apll2", 1, 1),
	FACTOR(CLK_TOP_APLL2_D2, "top_apll2_d2",
			"apll2", 1, 2),
	FACTOR(CLK_TOP_APLL2_D4, "top_apll2_d4",
			"apll2", 1, 4),
	FACTOR(CLK_TOP_APLL2_D8, "top_apll2_d8",
			"apll2", 1, 8),
	FACTOR(CLK_TOP_ADSPPLL, "top_adsppll",
			"adsppll", 1, 1),
	FACTOR(CLK_TOP_EMIPLL, "top_emipll",
			"emipll", 1, 1),
	FACTOR(CLK_TOP_EMIPLL2, "top_emipll2",
			"top_emipll2", 1, 1),
	FACTOR(CLK_TOP_TVDPLL, "top_tvdpll",
			"tvdpll", 1, 1),
	FACTOR(CLK_TOP_MSDCPLL, "top_msdcpll",
			"msdcpll", 1, 1),
	FACTOR(CLK_TOP_MSDCPLL_D2, "top_msdcpll_d2",
			"msdcpll", 1, 2),
	FACTOR(CLK_TOP_CLKRTC, "top_clkrtc",
			"clk32k", 1, 1),
	FACTOR(CLK_TOP_TCK_26M_MX9, "top_tck_26m_mx9_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_F26M, "top_f26m_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_F26M_CK_D2, "top_f26m_d2",
			"clk13m", 1, 1),
	FACTOR(CLK_TOP_OSC, "top_osc_ck",
			"ulposc", 1, 1),
	FACTOR(CLK_TOP_OSC_D2, "top_osc_d2",
			"ulposc", 1, 2),
	FACTOR(CLK_TOP_OSC_D4, "top_osc_d4",
			"ulposc", 1, 4),
	FACTOR(CLK_TOP_OSC_D7, "top_osc_d7",
			"ulposc", 1, 7),
	FACTOR(CLK_TOP_OSC_D8, "top_osc_d8",
			"ulposc", 1, 8),
	FACTOR(CLK_TOP_OSC_D10, "top_osc_d10",
			"ulposc", 1, 10),
	FACTOR(CLK_TOP_OSC_D16, "top_osc_d16",
			"ulposc", 1, 16),
	FACTOR(CLK_TOP_MMPLL_D4, "top_mmpll_d4",
			"mmpll", 1, 4),
	FACTOR(CLK_TOP_MMPLL_D4_D2, "top_mmpll_d4_d2",
			"mmpll", 1, 8),
	FACTOR(CLK_TOP_MMPLL_D4_D4, "top_mmpll_d4_d4",
			"mmpll", 1, 16),
	FACTOR(CLK_TOP_MMPLL_D5, "top_mmpll_d5",
			"mmpll", 1, 5),
	FACTOR(CLK_TOP_MMPLL_D5_D2, "top_mmpll_d5_d2",
			"mmpll", 1, 10),
	FACTOR(CLK_TOP_MMPLL_D6, "top_mmpll_d6",
			"mmpll", 1, 6),
	FACTOR(CLK_TOP_MMPLL_D6_D2, "top_mmpll_d6_d2",
			"mmpll", 1, 12),
	FACTOR(CLK_TOP_MMPLL_D7, "top_mmpll_d7",
			"mmpll", 1, 7),
	FACTOR(CLK_TOP_MMPLL_D9, "top_mmpll_d9",
			"mmpll", 1, 9),
	FACTOR(CLK_TOP_AXI, "top_axi_ck",
			"top_axi_sel", 1, 1),
	FACTOR(CLK_TOP_AXI_PERI, "top_axi_p_ck",
			"top_axi_p_sel", 1, 1),
	FACTOR(CLK_TOP_AXI_UFS, "top_axi_ufs_ck",
			"top_axi_ufs_sel", 1, 1),
	FACTOR(CLK_TOP_DISP0, "top_disp0_ck",
			"top_disp0_sel", 1, 1),
	FACTOR(CLK_TOP_UART, "top_uart_ck",
			"top_uart_sel", 1, 1),
	FACTOR(CLK_TOP_SPI0, "top_spi0_ck",
			"top_spi0_sel", 1, 1),
	FACTOR(CLK_TOP_SPI1, "top_spi1_ck",
			"top_spi1_sel", 1, 1),
	FACTOR(CLK_TOP_SPI2, "top_spi2_ck",
			"top_spi2_sel", 1, 1),
	FACTOR(CLK_TOP_SPI3, "top_spi3_ck",
			"top_spi3_sel", 1, 1),
	FACTOR(CLK_TOP_SPI4, "top_spi4_ck",
			"top_spi4_sel", 1, 1),
	FACTOR(CLK_TOP_SPI5, "top_spi5_ck",
			"top_spi5_sel", 1, 1),
	FACTOR(CLK_TOP_SPI6, "top_spi6_ck",
			"top_spi6_sel", 1, 1),
	FACTOR(CLK_TOP_SPI7, "top_spi7_ck",
			"top_spi7_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC30_1, "top_msdc30_1_ck",
			"top_msdc30_1_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC30_1_HCLK, "top_msdc30_1_h_ck",
			"top_msdc30_1_h_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_INTBUS, "top_aud_intbus_ck",
			"top_aud_intbus_sel", 1, 1),
	FACTOR(CLK_TOP_DISP_PWM, "top_disp_pwm_ck",
			"top_disp_pwm_sel", 1, 1),
	FACTOR(CLK_TOP_I2C, "top_i2c_ck",
			"top_i2c_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_ENGEN1, "top_aud_engen1_ck",
			"top_aud_engen1_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_ENGEN2, "top_aud_engen2_ck",
			"top_aud_engen2_sel", 1, 1),
	FACTOR(CLK_TOP_AES_UFSFDE, "top_aes_ufsfde_ck",
			"top_aes_ufsfde_sel", 1, 1),
	FACTOR(CLK_TOP_UFS, "top_ufs_ck",
			"top_ufs_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_1, "top_aud_1_ck",
			"top_aud_1_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_2, "top_aud_2_ck",
			"top_aud_2_sel", 1, 1),
	FACTOR(CLK_TOP_DPMAIF_MAIN, "top_dpmaif_main_ck",
			"top_dpmaif_main_sel", 1, 1),
	FACTOR(CLK_TOP_VENC, "top_venc_ck",
			"top_venc_sel", 1, 1),
	FACTOR(CLK_TOP_VDEC, "top_vdec_ck",
			"top_vdec_sel", 1, 1),
	FACTOR(CLK_TOP_PWM, "top_pwm_ck",
			"top_pwm_sel", 1, 1),
	FACTOR(CLK_TOP_AUDIO_H, "top_audio_h_ck",
			"top_audio_h_sel", 1, 1),
	FACTOR(CLK_TOP_IMG1, "top_img1_ck",
			"top_img1_sel", 1, 1),
	FACTOR(CLK_TOP_IPE, "top_ipe_ck",
			"top_ipe_sel", 1, 1),
	FACTOR(CLK_TOP_CAM, "top_cam_ck",
			"top_cam_sel", 1, 1),
};

static const struct mtk_fixed_factor vlp_top_divs[] = {
	FACTOR(CLK_VLP_TOP_APLL1, "vlp_apll1",
			"apll1", 1, 1),
	FACTOR(CLK_VLP_TOP_APLL2, "vlp_apll2",
			"apll2", 1, 1),
	FACTOR(CLK_VLP_TOP_OSC, "vlp_osc",
			"ulposc", 1, 1),
	FACTOR(CLK_VLP_TOP_ULPOSC, "vlp_ulposc_ck",
			"ulposc", 1, 1),
};

static const char * const top_axi_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4_d4",
	"top_mainpll_d7_d2",
	"top_mainpll_d4_d2",
	"top_mainpll_d5_d2",
	"top_mainpll_d6_d2",
	"top_osc_d4"
};

static const char * const top_axi_p_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4_d4",
	"top_mainpll_d7_d2",
	"top_osc_d4"
};

static const char * const top_axi_ufs_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4_d8",
	"top_mainpll_d7_d4",
	"top_osc_d8"
};

static const char * const top_bus_aximem_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d7_d2",
	"top_mainpll_d5_d2",
	"top_mainpll_d4_d2",
	"top_mainpll_d6"
};

static const char * const top_disp0_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d5_d2",
	"top_mmpll_d6_d2",
	"top_mmpll_d4_d2",
	"top_univpll_d6",
	"top_mainpll_d5",
	"tvdpll",
	"top_mmpll_d6",
	"top_mainpll_d5_d2",
	"top_mmpll_d4_d4"
};

static const char * const top_mminfra_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d2",
	"top_mainpll_d5_d2",
	"top_mmpll_d6_d2",
	"top_mainpll_d4_d2",
	"top_mmpll_d4_d2",
	"top_mainpll_d6",
	"top_mmpll_d7",
	"top_univpll_d6",
	"top_mainpll_d5",
	"top_mmpll_d6",
	"top_univpll_d5",
	"top_mainpll_d4",
	"top_univpll_d4",
	"top_mmpll_d4",
	"top_mmpll_d5_d2"
};

static const char * const top_mmup_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d5_d2",
	"top_mmpll_d4_d2",
	"top_mainpll_d4",
	"top_univpll_d4",
	"top_mmpll_d4",
	"top_mainpll_d3"
};

static const char * const top_uart_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d8"
};

static const char * const top_spi0_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d2",
	"top_univpll_192m",
	"top_mainpll_d6_d2",
	"top_univpll_d4_d4",
	"top_mainpll_d4_d4",
	"top_univpll_d5_d4",
	"top_univpll_d6_d4"
};

static const char * const top_spi1_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d2",
	"top_univpll_192m",
	"top_mainpll_d6_d2",
	"top_univpll_d4_d4",
	"top_mainpll_d4_d4",
	"top_univpll_d5_d4",
	"top_univpll_d6_d4"
};

static const char * const top_spi2_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d2",
	"top_univpll_192m",
	"top_mainpll_d6_d2",
	"top_univpll_d4_d4",
	"top_mainpll_d4_d4",
	"top_univpll_d5_d4",
	"top_univpll_d6_d4"
};

static const char * const top_spi3_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d2",
	"top_univpll_192m",
	"top_mainpll_d6_d2",
	"top_univpll_d4_d4",
	"top_mainpll_d4_d4",
	"top_univpll_d5_d4",
	"top_univpll_d6_d4"
};

static const char * const top_spi4_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d2",
	"top_univpll_192m",
	"top_mainpll_d6_d2",
	"top_univpll_d4_d4",
	"top_mainpll_d4_d4",
	"top_univpll_d5_d4",
	"top_univpll_d6_d4"
};

static const char * const top_spi5_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d2",
	"top_univpll_192m",
	"top_mainpll_d6_d2",
	"top_univpll_d4_d4",
	"top_mainpll_d4_d4",
	"top_univpll_d5_d4",
	"top_univpll_d6_d4"
};

static const char * const top_spi6_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d2",
	"top_mainpll_d6_d2",
	"top_univpll_d4_d4",
	"top_mainpll_d4_d4",
	"top_univpll_d5_d4",
	"top_univpll_d6_d4"
};

static const char * const top_spi7_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d2",
	"top_univpll_192m",
	"top_mainpll_d6_d2",
	"top_univpll_d4_d4",
	"top_mainpll_d4_d4",
	"top_univpll_d5_d4",
	"top_univpll_d6_d4"
};

static const char * const top_msdc_macro_1p_parents[] = {
	"top_tck_26m_mx9_ck",
	"msdcpll",
	"top_mainpll_d6_d2",
	"top_mainpll_d7_d2"
};

static const char * const top_msdc30_1_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d6_d2",
	"top_msdcpll_d2"
};

static const char * const top_msdc30_1_h_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d6_d2",
	"top_msdcpll_d2"
};

static const char * const top_aud_intbus_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4_d4",
	"top_mainpll_d7_d4"
};

static const char * const top_atb_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4_d2",
	"top_mainpll_d5_d2"
};

static const char * const top_disp_pwm_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d4",
	"top_osc_d2",
	"top_osc_d4",
	"top_osc_d16",
	"top_univpll_d5_d4",
	"top_mainpll_d4_d4"
};

static const char * const top_usb_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d5_d4",
	"top_univpll_d6_d4"
};

static const char * const top_usb_xhci_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d5_d4",
	"top_univpll_d6_d4"
};

static const char * const top_i2c_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4_d8",
	"top_univpll_d5_d4",
	"top_mainpll_d4_d4"
};

static const char * const top_seninf_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d2",
	"top_osc_ck",
	"top_mmpll_d5_d2",
	"top_univpll_d4_d2",
	"top_mmpll_d4_d2",
	"top_mmpll_d7",
	"top_univpll_d6",
	"top_univpll_d5"
};

static const char * const top_seninf1_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d2",
	"top_osc_ck",
	"top_mmpll_d5_d2",
	"top_univpll_d4_d2",
	"top_mmpll_d4_d2",
	"top_mmpll_d7",
	"top_univpll_d6",
	"top_univpll_d5"
};

static const char * const top_seninf2_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d2",
	"top_osc_ck",
	"top_mmpll_d5_d2",
	"top_univpll_d4_d2",
	"top_mmpll_d4_d2",
	"top_mmpll_d7",
	"top_univpll_d6",
	"top_univpll_d5"
};

static const char * const top_aud_engen1_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_apll1_d2",
	"top_apll1_d4",
	"top_apll1_d8"
};

static const char * const top_aud_engen2_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_apll2_d2",
	"top_apll2_d4",
	"top_apll2_d8"
};

static const char * const top_aes_ufsfde_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4",
	"top_mainpll_d4_d2",
	"top_mainpll_d6",
	"top_mainpll_d4_d4",
	"top_univpll_d4_d2",
	"top_univpll_d6"
};

static const char * const top_ufs_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4_d8",
	"top_mainpll_d4_d4",
	"top_mainpll_d5_d2",
	"top_mainpll_d6_d2",
	"top_univpll_d6_d2",
	"top_msdcpll_d2"
};

static const char * const top_aud_1_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_apll1_ck"
};

static const char * const top_aud_2_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_apll2_ck"
};

static const char * const top_venc_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mmpll_d4_d2",
	"top_mainpll_d6",
	"top_univpll_d4_d2",
	"top_mainpll_d4_d2",
	"top_univpll_d6",
	"top_mmpll_d6",
	"top_mainpll_d5_d2",
	"top_mainpll_d6_d2",
	"top_mmpll_d9",
	"top_mmpll_d4",
	"top_mainpll_d4",
	"top_univpll_d4",
	"top_univpll_d5",
	"top_univpll_d5_d2",
	"top_mainpll_d5"
};

static const char * const top_vdec_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_192m_d2",
	"top_univpll_d5_d4",
	"top_mainpll_d5",
	"top_mainpll_d5_d2",
	"top_mmpll_d6_d2",
	"top_univpll_d5_d2",
	"top_mainpll_d4_d2",
	"top_univpll_d4_d2",
	"top_univpll_d7",
	"top_mmpll_d7",
	"top_mmpll_d6",
	"top_univpll_d6",
	"top_mainpll_d4",
	"top_mmpll_d4_d2",
	"top_mmpll_d5_d2"
};

static const char * const top_pwm_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d4_d8"
};

static const char * const top_audio_h_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d7_d2",
	"top_apll1_ck",
	"top_apll2_ck"
};

static const char * const top_mcupm_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d2",
	"top_mainpll_d5_d2",
	"top_mainpll_d6_d2"
};

static const char * const top_mem_sub_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d4_d4",
	"top_mainpll_d6_d2",
	"top_mainpll_d5_d2",
	"top_mainpll_d4_d2",
	"top_mainpll_d6",
	"top_mmpll_d7",
	"top_mainpll_d5",
	"top_univpll_d5",
	"top_mainpll_d4",
	"top_univpll_d4"
};

static const char * const top_mem_sub_p_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d4_d4",
	"top_mainpll_d5_d2",
	"top_mainpll_d4_d2",
	"top_mainpll_d6",
	"top_mainpll_d5",
	"top_univpll_d5",
	"top_mainpll_d4"
};

static const char * const top_mem_sub_ufs_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d4_d4",
	"top_mainpll_d5_d2",
	"top_mainpll_d4_d2",
	"top_mainpll_d6",
	"top_mainpll_d5",
	"top_univpll_d5",
	"top_mainpll_d4"
};

static const char * const top_emi_n_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d6_d2",
	"top_osc_d2",
	"top_mainpll_d6",
	"top_mainpll_d4",
	"emipll"
};

static const char * const top_dsi_occ_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d2",
	"top_univpll_d5_d2",
	"top_univpll_d4_d2"
};

static const char * const top_ap2conn_host_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d7_d4"
};

static const char * const top_img1_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d4",
	"top_mmpll_d5",
	"top_mmpll_d6",
	"top_univpll_d6",
	"top_mmpll_d7",
	"top_mmpll_d4_d2",
	"top_univpll_d4_d2",
	"top_mainpll_d4_d2",
	"top_mmpll_d6_d2",
	"top_mmpll_d5_d2"
};

static const char * const top_ipe_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d4",
	"top_mainpll_d4",
	"top_mmpll_d6",
	"top_univpll_d6",
	"top_mainpll_d6",
	"top_mmpll_d4_d2",
	"top_univpll_d4_d2",
	"top_mainpll_d4_d2",
	"top_mmpll_d6_d2",
	"top_mmpll_d5_d2"
};

static const char * const top_cam_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4",
	"top_univpll_d4",
	"top_univpll_d5",
	"top_mmpll_d7",
	"top_mmpll_d6",
	"top_univpll_d6",
	"top_univpll_d4_d2",
	"top_mmpll_d9",
	"top_mmpll_d5_d2",
	"top_osc_d2"
};

static const char * const top_camtm_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d2",
	"top_univpll_d6_d2",
	"top_univpll_d6_d4"
};

static const char * const top_md_emi_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4"
};

static const char * const top_mfg_ref_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6",
	"top_mainpll_d5_d2"
};

static const char * const top_mfgsc_ref_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6",
	"top_mainpll_d5_d2"
};

static const char * const top_efuse_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d10"
};

static const char * const top_unipll_ses_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d2"
};

static const char * const top_dramulp_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d6_d2"
};

static const char * const top_usb_frmcnt_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_192m_d4"
};

static const char * const top_apll_i2sin1_m_parents[] = {
	"top_aud_1_sel",
	"top_aud_2_sel"
};

static const char * const top_apll_i2sin2_m_parents[] = {
	"top_aud_1_sel",
	"top_aud_2_sel"
};

static const struct mtk_mux top_muxes[] = {
#if MT_CCF_MUX_DISABLE
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AXI_SEL/* dts */, "top_axi_sel",
		top_axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 31/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AXI_P_SEL/* dts */, "top_axi_p_sel",
		top_axi_p_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_PERI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 30/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AXI_UFS_SEL/* dts */, "top_axi_ufs_sel",
		top_axi_ufs_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_UFS_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 29/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_BUS_AXIMEM_SEL/* dts */, "top_bus_aximem_sel",
		top_bus_aximem_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_BUS_AXIMEM_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 28/* cksta shift */),
	/* CLK_CFG_1 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_DISP0_SEL/* dts */, "top_disp0_sel",
		top_disp0_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DISP0_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 27/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MMINFRA_SEL/* dts */, "top_mminfra_sel",
		top_mminfra_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMINFRA_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 26/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MMUP_SEL/* dts */, "top_mmup_sel",
		top_mmup_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMUP_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 25/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_UART_SEL/* dts */, "top_uart_sel",
		top_uart_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UART_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 24/* cksta shift */),
	/* CLK_CFG_2 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SPI0_SEL/* dts */, "top_spi0_sel",
		top_spi0_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI0_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 23/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SPI1_SEL/* dts */, "top_spi1_sel",
		top_spi1_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI1_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 22/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SPI2_SEL/* dts */, "top_spi2_sel",
		top_spi2_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI2_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 21/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SPI3_SEL/* dts */, "top_spi3_sel",
		top_spi3_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI3_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 20/* cksta shift */),
	/* CLK_CFG_3 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SPI4_SEL/* dts */, "top_spi4_sel",
		top_spi4_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI4_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 19/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SPI5_SEL/* dts */, "top_spi5_sel",
		top_spi5_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI5_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 18/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SPI6_SEL/* dts */, "top_spi6_sel",
		top_spi6_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI6_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 17/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SPI7_SEL/* dts */, "top_spi7_sel",
		top_spi7_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI7_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 16/* cksta shift */),
	/* CLK_CFG_4 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MSDC_MACRO_1P_SEL/* dts */, "top_msdc_macro_1p_sel",
		top_msdc_macro_1p_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC_MACRO_1P_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 15/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MSDC30_1_SEL/* dts */, "top_msdc30_1_sel",
		top_msdc30_1_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC30_1_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 14/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MSDC30_1_HCLK_SEL/* dts */, "top_msdc30_1_h_sel",
		top_msdc30_1_h_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC30_1_HCLK_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 13/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AUD_INTBUS_SEL/* dts */, "top_aud_intbus_sel",
		top_aud_intbus_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AUD_INTBUS_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 12/* cksta shift */),
	/* CLK_CFG_5 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_ATB_SEL/* dts */, "top_atb_sel",
		top_atb_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ATB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 11/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_DISP_PWM_SEL/* dts */, "top_disp_pwm_sel",
		top_disp_pwm_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DISP_PWM_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 10/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_USB_TOP_SEL/* dts */, "top_usb_sel",
		top_usb_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_USB_TOP_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 9/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_USB_XHCI_SEL/* dts */, "top_usb_xhci_sel",
		top_usb_xhci_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSUSB_XHCI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 8/* cksta shift */),
	/* CLK_CFG_6 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_I2C_SEL/* dts */, "top_i2c_sel",
		top_i2c_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_I2C_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 7/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SENINF_SEL/* dts */, "top_seninf_sel",
		top_seninf_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 6/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SENINF1_SEL/* dts */, "top_seninf1_sel",
		top_seninf1_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF1_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 5/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SENINF2_SEL/* dts */, "top_seninf2_sel",
		top_seninf2_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF2_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 4/* cksta shift */),
	/* CLK_CFG_7 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AUD_ENGEN1_SEL/* dts */, "top_aud_engen1_sel",
		top_aud_engen1_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AUD_ENGEN1_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 3/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AUD_ENGEN2_SEL/* dts */, "top_aud_engen2_sel",
		top_aud_engen2_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AUD_ENGEN2_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 2/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AES_UFSFDE_SEL/* dts */, "top_aes_ufsfde_sel",
		top_aes_ufsfde_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AES_UFSFDE_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 1/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_UFS_SEL/* dts */, "top_ufs_sel",
		top_ufs_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_UFS_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 31/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AUD_1_SEL/* dts */, "top_aud_1_sel",
		top_aud_1_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_1_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 29/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AUD_2_SEL/* dts */, "top_aud_2_sel",
		top_aud_2_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_2_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 28/* cksta shift */),
	/* CLK_CFG_9 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_VENC_SEL/* dts */, "top_venc_sel",
		top_venc_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_VENC_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 26/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_VDEC_SEL/* dts */, "top_vdec_sel",
		top_vdec_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_VDEC_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 25/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_PWM_SEL/* dts */, "top_pwm_sel",
		top_pwm_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_PWM_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 24/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AUDIO_H_SEL/* dts */, "top_audio_h_sel",
		top_audio_h_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUDIO_H_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 23/* cksta shift */),
	/* CLK_CFG_10 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MCUPM_SEL/* dts */, "top_mcupm_sel",
		top_mcupm_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MCUPM_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 22/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MEM_SUB_SEL/* dts */, "top_mem_sub_sel",
		top_mem_sub_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 21/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MEM_SUB_P_SEL/* dts */, "top_mem_sub_p_sel",
		top_mem_sub_p_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MEM_SUB_PERI_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 20/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MEM_SUB_UFS_SEL/* dts */, "top_mem_sub_ufs_sel",
		top_mem_sub_ufs_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MEM_SUB_UFS_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 19/* cksta shift */),
	/* CLK_CFG_11 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_EMI_N_SEL/* dts */, "top_emi_n_sel",
		top_emi_n_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_EMI_N_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 18/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_DSI_OCC_SEL/* dts */, "top_dsi_occ_sel",
		top_dsi_occ_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DSI_OCC_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 17/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AP2CONN_HOST_SEL/* dts */, "top_ap2conn_host_sel",
		top_ap2conn_host_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AP2CONN_HOST_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 16/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_IMG1_SEL/* dts */, "top_img1_sel",
		top_img1_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_IMG1_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 15/* cksta shift */),
	/* CLK_CFG_12 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_IPE_SEL/* dts */, "top_ipe_sel",
		top_ipe_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_IPE_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 14/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_CAM_SEL/* dts */, "top_cam_sel",
		top_cam_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_CAM_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 13/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_CAMTM_SEL/* dts */, "top_camtm_sel",
		top_camtm_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_CAMTM_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 12/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_EMI_INTERFACE_546_SEL/* dts */, "top_md_emi_sel",
		top_md_emi_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_EMI_INTERFACE_546_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 11/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MFG_REF_SEL/* dts */, "top_mfg_ref_sel",
		top_mfg_ref_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MFG_REF_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 9/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MFGSC_REF_SEL/* dts */, "top_mfgsc_ref_sel",
		top_mfgsc_ref_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MFGSC_REF_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 8/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_EFUSE_SEL/* dts */, "top_efuse_sel",
		top_efuse_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_EFUSE_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 7/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_UNIPLL_SES_SEL/* dts */, "top_unipll_ses_sel",
		top_unipll_ses_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_UNIPLL_SES_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 5/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_DRAMULP_SEL/* dts */, "top_dramulp_sel",
		top_dramulp_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DRAMULP_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 3/* cksta shift */),
	/* CLK_CFG_15 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_USB_FRMCNT_SEL/* dts */, "top_usb_frmcnt_sel",
		top_usb_frmcnt_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SSUSB_FRMCNT_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 2/* cksta shift */),
#else
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AXI_SEL/* dts */, "top_axi_sel",
		top_axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 31/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AXI_P_SEL/* dts */, "top_axi_p_sel",
		top_axi_p_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_PERI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 30/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AXI_UFS_SEL/* dts */, "top_axi_ufs_sel",
		top_axi_ufs_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_UFS_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 29/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_BUS_AXIMEM_SEL/* dts */, "top_bus_aximem_sel",
		top_bus_aximem_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_BUS_AXIMEM_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 28/* cksta shift */),
	/* CLK_CFG_1 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_DISP0_SEL/* dts */, "top_disp0_sel",
		top_disp0_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DISP0_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		27/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		27/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_MMINFRA_SEL/* dts */, "top_mminfra_sel",
		top_mminfra_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MMINFRA_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		26/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		26/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_MMUP_SEL/* dts */, "top_mmup_sel",
		top_mmup_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MMUP_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		25/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		25/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_UART_SEL/* dts */, "top_uart_sel",
		top_uart_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 1/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_UART_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		24/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		24/* fenc shift */),
	/* CLK_CFG_2 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_SPI0_SEL/* dts */, "top_spi0_sel",
		top_spi0_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI0_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		23/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		23/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_SPI1_SEL/* dts */, "top_spi1_sel",
		top_spi1_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI1_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		22/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		22/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_SPI2_SEL/* dts */, "top_spi2_sel",
		top_spi2_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI2_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		21/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		21/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_SPI3_SEL/* dts */, "top_spi3_sel",
		top_spi3_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI3_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		20/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		20/* fenc shift */),
	/* CLK_CFG_3 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_SPI4_SEL/* dts */, "top_spi4_sel",
		top_spi4_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI4_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		19/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		19/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_SPI5_SEL/* dts */, "top_spi5_sel",
		top_spi5_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI5_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		18/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		18/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_SPI6_SEL/* dts */, "top_spi6_sel",
		top_spi6_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI6_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		17/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		17/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_SPI7_SEL/* dts */, "top_spi7_sel",
		top_spi7_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI7_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		16/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		16/* fenc shift */),
	/* CLK_CFG_4 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_MSDC_MACRO_1P_SEL/* dts */, "top_msdc_macro_1p_sel",
		top_msdc_macro_1p_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC_MACRO_1P_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		15/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		15/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_MSDC30_1_SEL/* dts */, "top_msdc30_1_sel",
		top_msdc30_1_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC30_1_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		14/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		14/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_MSDC30_1_HCLK_SEL/* dts */, "top_msdc30_1_h_sel",
		top_msdc30_1_h_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC30_1_HCLK_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		13/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		13/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_AUD_INTBUS_SEL/* dts */, "top_aud_intbus_sel",
		top_aud_intbus_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AUD_INTBUS_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		12/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		12/* fenc shift */),
	/* CLK_CFG_5 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_ATB_SEL/* dts */, "top_atb_sel",
		top_atb_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ATB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 11/* cksta shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_DISP_PWM_SEL/* dts */, "top_disp_pwm_sel",
		top_disp_pwm_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DISP_PWM_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		10/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		10/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_USB_TOP_SEL/* dts */, "top_usb_sel",
		top_usb_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_USB_TOP_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		9/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		9/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_USB_XHCI_SEL/* dts */, "top_usb_xhci_sel",
		top_usb_xhci_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SSUSB_XHCI_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		8/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		8/* fenc shift */),
	/* CLK_CFG_6 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_I2C_SEL/* dts */, "top_i2c_sel",
		top_i2c_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_I2C_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		7/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		7/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_SENINF_SEL/* dts */, "top_seninf_sel",
		top_seninf_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		6/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		6/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_SENINF1_SEL/* dts */, "top_seninf1_sel",
		top_seninf1_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 4/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF1_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		5/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		5/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_SENINF2_SEL/* dts */, "top_seninf2_sel",
		top_seninf2_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF2_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		4/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		4/* fenc shift */),
	/* CLK_CFG_7 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_AUD_ENGEN1_SEL/* dts */, "top_aud_engen1_sel",
		top_aud_engen1_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AUD_ENGEN1_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		3/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		3/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_AUD_ENGEN2_SEL/* dts */, "top_aud_engen2_sel",
		top_aud_engen2_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AUD_ENGEN2_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		2/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		2/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_AES_UFSFDE_SEL/* dts */, "top_aes_ufsfde_sel",
		top_aes_ufsfde_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AES_UFSFDE_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		1/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		1/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_UFS_SEL/* dts */, "top_ufs_sel",
		top_ufs_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_UFS_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		31/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		0/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_AUD_1_SEL/* dts */, "top_aud_1_sel",
		top_aud_1_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_1_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		29/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		30/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_AUD_2_SEL/* dts */, "top_aud_2_sel",
		top_aud_2_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 16/* lsb */, 1/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_2_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		28/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		29/* fenc shift */),
	/* CLK_CFG_9 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_VENC_SEL/* dts */, "top_venc_sel",
		top_venc_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_VENC_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		26/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		27/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_VDEC_SEL/* dts */, "top_vdec_sel",
		top_vdec_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_VDEC_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		25/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		26/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_PWM_SEL/* dts */, "top_pwm_sel",
		top_pwm_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 16/* lsb */, 1/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_PWM_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		24/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		25/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_AUDIO_H_SEL/* dts */, "top_audio_h_sel",
		top_audio_h_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUDIO_H_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		23/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		24/* fenc shift */),
	/* CLK_CFG_10 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MCUPM_SEL/* dts */, "top_mcupm_sel",
		top_mcupm_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MCUPM_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 22/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MEM_SUB_SEL/* dts */, "top_mem_sub_sel",
		top_mem_sub_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 21/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MEM_SUB_P_SEL/* dts */, "top_mem_sub_p_sel",
		top_mem_sub_p_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MEM_SUB_PERI_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 20/* cksta shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_MEM_SUB_UFS_SEL/* dts */, "top_mem_sub_ufs_sel",
		top_mem_sub_ufs_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_MEM_SUB_UFS_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		19/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		20/* fenc shift */),
	/* CLK_CFG_11 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_EMI_N_SEL/* dts */, "top_emi_n_sel",
		top_emi_n_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_EMI_N_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 18/* cksta shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_DSI_OCC_SEL/* dts */, "top_dsi_occ_sel",
		top_dsi_occ_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_DSI_OCC_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		17/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		18/* fenc shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AP2CONN_HOST_SEL/* dts */, "top_ap2conn_host_sel",
		top_ap2conn_host_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AP2CONN_HOST_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 16/* cksta shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_IMG1_SEL/* dts */, "top_img1_sel",
		top_img1_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_IMG1_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		15/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		16/* fenc shift */),
	/* CLK_CFG_12 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_IPE_SEL/* dts */, "top_ipe_sel",
		top_ipe_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_IPE_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		14/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		15/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_CAM_SEL/* dts */, "top_cam_sel",
		top_cam_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_CAM_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		13/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		14/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_CAMTM_SEL/* dts */, "top_camtm_sel",
		top_camtm_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_CAMTM_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		12/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		13/* fenc shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_EMI_INTERFACE_546_SEL/* dts */, "top_md_emi_sel",
		top_md_emi_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_EMI_INTERFACE_546_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 11/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MFG_REF_SEL/* dts */, "top_mfg_ref_sel",
		top_mfg_ref_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MFG_REF_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 9/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MFGSC_REF_SEL/* dts */, "top_mfgsc_ref_sel",
		top_mfgsc_ref_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MFGSC_REF_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 8/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_EFUSE_SEL/* dts */, "top_efuse_sel",
		top_efuse_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_EFUSE_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 7/* cksta shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_UNIPLL_SES_SEL/* dts */, "top_unipll_ses_sel",
		top_unipll_ses_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_UNIPLL_SES_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		5/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		6/* fenc shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_DRAMULP_SEL/* dts */, "top_dramulp_sel",
		top_dramulp_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DRAMULP_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 3/* cksta shift */),
	/* CLK_CFG_15 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_TOP_USB_FRMCNT_SEL/* dts */, "top_usb_frmcnt_sel",
		top_usb_frmcnt_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 0/* lsb */, 1/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSUSB_FRMCNT_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		2/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		3/* fenc shift */),
#endif
};

static const char * const vlp_scp_parents[] = {
	"top_tck_26m_mx9_ck",
	"adsppll",
	"top_univpll_d4",
	"top_univpll_d3",
	"top_mainpll_d3",
	"top_univpll_d6",
	"apll1",
	"top_mainpll_d4",
	"top_mainpll_d7",
	"top_osc_d10"
};

static const char * const vlp_pwrap_ulposc_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d10",
	"top_osc_d7",
	"top_osc_d8",
	"top_osc_d16",
	"top_mainpll_d7_d8"
};

static const char * const vlp_spmi_p_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_f26m_d2",
	"top_osc_d8",
	"top_osc_d10",
	"top_osc_d16",
	"top_osc_d7",
	"top_clkrtc",
	"top_mainpll_d7_d8",
	"top_mainpll_d6_d8",
	"top_mainpll_d5_d8"
};

static const char * const vlp_spmi_m_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_f26m_d2",
	"top_osc_d8",
	"top_osc_d10",
	"top_osc_d16",
	"top_osc_d7",
	"top_clkrtc",
	"top_mainpll_d7_d8",
	"top_mainpll_d6_d8",
	"top_mainpll_d5_d8"
};

static const char * const vlp_dvfsrc_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d10"
};

static const char * const vlp_pwm_vlp_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d4",
	"top_clkrtc",
	"top_osc_d10",
	"top_mainpll_d4_d8"
};

static const char * const vlp_axi_vlp_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d10",
	"top_osc_d2",
	"top_mainpll_d7_d4",
	"top_mainpll_d7_d2"
};

static const char * const vlp_systimer_26m_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d10"
};

static const char * const vlp_sspm_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d10",
	"top_mainpll_d5_d2",
	"vlp_osc",
	"top_mainpll_d6"
};

static const char * const vlp_sspm_f26m_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d10"
};

static const char * const vlp_srck_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d10"
};

static const char * const vlp_scp_spi_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d5_d4",
	"top_mainpll_d7_d2",
	"top_osc_d10"
};

static const char * const vlp_scp_iic_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d5_d4",
	"top_mainpll_d7_d2",
	"top_osc_d10"
};

static const char * const vlp_scp_iic_hs_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d5_d4",
	"top_mainpll_d7_d2",
	"top_osc_d10"
};

static const char * const vlp_sspm_ulposc_parents[] = {
	"top_tck_26m_mx9_ck",
	"vlp_osc",
	"top_univpll_d5_d2"
};

static const char * const vlp_tia_ulposc_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d10",
	"top_osc_d7",
	"top_osc_d8",
	"top_osc_d16",
	"top_mainpll_d7_d8"
};

static const char * const vlp_apxgpt_26m_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d10"
};

static const char * const vlp_camtg0_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_192m_d8",
	"top_univpll_d6_d8",
	"top_univpll_192m_d4",
	"top_univpll_d6_d16",
	"top_f26m_d2",
	"top_univpll_192m_d10",
	"top_univpll_192m_d16",
	"top_univpll_192m_d32"
};

static const char * const vlp_camtg1_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_192m_d8",
	"top_univpll_d6_d8",
	"top_univpll_192m_d4",
	"top_univpll_d6_d16",
	"top_f26m_d2",
	"top_univpll_192m_d10",
	"top_univpll_192m_d16",
	"top_univpll_192m_d32"
};

static const char * const vlp_camtg2_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_192m_d8",
	"top_univpll_d6_d8",
	"top_univpll_192m_d4",
	"top_univpll_d6_d16",
	"top_f26m_d2",
	"top_univpll_192m_d10",
	"top_univpll_192m_d16",
	"top_univpll_192m_d32"
};

static const char * const vlp_camtg3_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_192m_d8",
	"top_univpll_d6_d8",
	"top_univpll_192m_d4",
	"top_univpll_d6_d16",
	"top_f26m_d2",
	"top_univpll_192m_d10",
	"top_univpll_192m_d16",
	"top_univpll_192m_d32"
};

static const char * const vlp_kp_irq_gen_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d7_d4"
};

static const char * const vlp_ssr_pka_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4_d4",
	"top_mainpll_d4_d2",
	"top_mainpll_d7",
	"top_mainpll_d6",
	"top_mainpll_d5"
};

static const char * const vlp_ssr_dma_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4_d4",
	"top_mainpll_d4_d2",
	"top_mainpll_d7",
	"top_mainpll_d6",
	"top_mainpll_d5"
};

static const char * const vlp_ssr_kdf_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4_d4",
	"top_mainpll_d4_d2",
	"top_mainpll_d7",
	"top_mainpll_d6"
};

static const char * const vlp_ssr_rng_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4_d4",
	"top_mainpll_d5_d2",
	"top_mainpll_d4_d2"
};

static const struct mtk_mux vlp_top_muxes[] = {
#if MT_CCF_MUX_DISABLE
	/* VLP_CLK_CFG_0 */
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SCP_SEL/* dts */, "vlp_scp_sel",
		vlp_scp_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_PWRAP_ULPOSC_SEL/* dts */, "vlp_pwrap_ulposc_sel",
		vlp_pwrap_ulposc_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWRAP_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SPMI_P_MST_SEL/* dts */, "vlp_spmi_p_sel",
		vlp_spmi_p_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_P_MST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SPMI_M_MST_SEL/* dts */, "vlp_spmi_m_sel",
		vlp_spmi_m_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_M_MST_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_1 */
	MUX_CLR_SET_UPD(CLK_VLP_TOP_DVFSRC_SEL/* dts */, "vlp_dvfsrc_sel",
		vlp_dvfsrc_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DVFSRC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_PWM_VLP_SEL/* dts */, "vlp_pwm_vlp_sel",
		vlp_pwm_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWM_VLP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_AXI_VLP_SEL/* dts */, "vlp_axi_vlp_sel",
		vlp_axi_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_VLP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SYSTIMER_26M_SEL/* dts */, "vlp_systimer_26m_sel",
		vlp_systimer_26m_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SYSTIMER_26M_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_2 */
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SSPM_SEL/* dts */, "vlp_sspm_sel",
		vlp_sspm_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SSPM_F26M_SEL/* dts */, "vlp_sspm_f26m_sel",
		vlp_sspm_f26m_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_F26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SRCK_SEL/* dts */, "vlp_srck_sel",
		vlp_srck_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRCK_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SCP_SPI_SEL/* dts */, "vlp_scp_spi_sel",
		vlp_scp_spi_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SPI_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_3 */
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SCP_IIC_SEL/* dts */, "vlp_scp_iic_sel",
		vlp_scp_iic_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SCP_IIC_HIGH_SPD_SEL/* dts */, "vlp_scp_iic_hs_sel",
		vlp_scp_iic_hs_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SSPM_ULPOSC_SEL/* dts */, "vlp_sspm_ulposc_sel",
		vlp_sspm_ulposc_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_ULPOSC_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_4 */
	MUX_CLR_SET_UPD(CLK_VLP_TOP_TIA_ULPOSC_SEL/* dts */, "vlp_tia_ulposc_sel",
		vlp_tia_ulposc_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_TIA_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_APXGPT_26M_SEL/* dts */, "vlp_apxgpt_26m_sel",
		vlp_apxgpt_26m_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_APXGPT_26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_CAMTG0_SEL/* dts */, "vlp_camtg0_sel",
		vlp_camtg0_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG0_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_CAMTG1_SEL/* dts */, "vlp_camtg1_sel",
		vlp_camtg1_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG1_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_5 */
	MUX_CLR_SET_UPD(CLK_VLP_TOP_CAMTG2_SEL/* dts */, "vlp_camtg2_sel",
		vlp_camtg2_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_CAMTG3_SEL/* dts */, "vlp_camtg3_sel",
		vlp_camtg3_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG3_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_KP_IRQ_GEN_SEL/* dts */, "vlp_kp_irq_gen_sel",
		vlp_kp_irq_gen_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_KP_IRQ_GEN_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SSR_PKA_SEL/* dts */, "vlp_ssr_pka_sel",
		vlp_ssr_pka_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSR_PKA_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_6 */
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SSR_DMA_SEL/* dts */, "vlp_ssr_dma_sel",
		vlp_ssr_dma_parents/* parent */, VLP_CLK_CFG_6, VLP_CLK_CFG_6_SET,
		VLP_CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSR_DMA_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SSR_KDF_SEL/* dts */, "vlp_ssr_kdf_sel",
		vlp_ssr_kdf_parents/* parent */, VLP_CLK_CFG_6, VLP_CLK_CFG_6_SET,
		VLP_CLK_CFG_6_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSR_KDF_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SSR_RNG_SEL/* dts */, "vlp_ssr_rng_sel",
		vlp_ssr_rng_parents/* parent */, VLP_CLK_CFG_6, VLP_CLK_CFG_6_SET,
		VLP_CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSR_RNG_SHIFT/* upd shift */),
#else
	/* VLP_CLK_CFG_0 */
	MUX_GATE_FENC_CLR_SET_UPD(CLK_VLP_TOP_SCP_SEL/* dts */, "vlp_scp_sel",
		vlp_scp_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SCP_SHIFT/* upd shift */, VLP_OCIC_FENC_STATUS_MON_0/* fenc ofs */,
		31/* fenc shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_PWRAP_ULPOSC_SEL/* dts */, "vlp_pwrap_ulposc_sel",
		vlp_pwrap_ulposc_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWRAP_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SPMI_P_MST_SEL/* dts */, "vlp_spmi_p_sel",
		vlp_spmi_p_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_P_MST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SPMI_M_MST_SEL/* dts */, "vlp_spmi_m_sel",
		vlp_spmi_m_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_M_MST_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_1 */
	MUX_CLR_SET_UPD(CLK_VLP_TOP_DVFSRC_SEL/* dts */, "vlp_dvfsrc_sel",
		vlp_dvfsrc_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DVFSRC_SHIFT/* upd shift */),
	MUX_GATE_FENC_CLR_SET_UPD(CLK_VLP_TOP_PWM_VLP_SEL/* dts */, "vlp_pwm_vlp_sel",
		vlp_pwm_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_PWM_VLP_SHIFT/* upd shift */, VLP_OCIC_FENC_STATUS_MON_0/* fenc ofs */,
		26/* fenc shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_AXI_VLP_SEL/* dts */, "vlp_axi_vlp_sel",
		vlp_axi_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_VLP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SYSTIMER_26M_SEL/* dts */, "vlp_systimer_26m_sel",
		vlp_systimer_26m_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SYSTIMER_26M_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_2 */
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SSPM_SEL/* dts */, "vlp_sspm_sel",
		vlp_sspm_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SSPM_F26M_SEL/* dts */, "vlp_sspm_f26m_sel",
		vlp_sspm_f26m_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_F26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SRCK_SEL/* dts */, "vlp_srck_sel",
		vlp_srck_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRCK_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SCP_SPI_SEL/* dts */, "vlp_scp_spi_sel",
		vlp_scp_spi_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SPI_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_3 */
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SCP_IIC_SEL/* dts */, "vlp_scp_iic_sel",
		vlp_scp_iic_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SCP_IIC_HIGH_SPD_SEL/* dts */, "vlp_scp_iic_hs_sel",
		vlp_scp_iic_hs_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SSPM_ULPOSC_SEL/* dts */, "vlp_sspm_ulposc_sel",
		vlp_sspm_ulposc_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_ULPOSC_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_4 */
	MUX_CLR_SET_UPD(CLK_VLP_TOP_TIA_ULPOSC_SEL/* dts */, "vlp_tia_ulposc_sel",
		vlp_tia_ulposc_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_TIA_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_APXGPT_26M_SEL/* dts */, "vlp_apxgpt_26m_sel",
		vlp_apxgpt_26m_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_APXGPT_26M_SHIFT/* upd shift */),
	MUX_GATE_FENC_CLR_SET_UPD(CLK_VLP_TOP_CAMTG0_SEL/* dts */, "vlp_camtg0_sel",
		vlp_camtg0_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 4/* width */,
		23/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG0_SHIFT/* upd shift */, VLP_OCIC_FENC_STATUS_MON_0/* fenc ofs */,
		13/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD(CLK_VLP_TOP_CAMTG1_SEL/* dts */, "vlp_camtg1_sel",
		vlp_camtg1_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 4/* width */,
		31/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG1_SHIFT/* upd shift */, VLP_OCIC_FENC_STATUS_MON_0/* fenc ofs */,
		12/* fenc shift */),
	/* VLP_CLK_CFG_5 */
	MUX_GATE_FENC_CLR_SET_UPD(CLK_VLP_TOP_CAMTG2_SEL/* dts */, "vlp_camtg2_sel",
		vlp_camtg2_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG2_SHIFT/* upd shift */, VLP_OCIC_FENC_STATUS_MON_0/* fenc ofs */,
		11/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD(CLK_VLP_TOP_CAMTG3_SEL/* dts */, "vlp_camtg3_sel",
		vlp_camtg3_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 4/* width */,
		15/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG3_SHIFT/* upd shift */, VLP_OCIC_FENC_STATUS_MON_0/* fenc ofs */,
		10/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD(CLK_VLP_TOP_KP_IRQ_GEN_SEL/* dts */, "vlp_kp_irq_gen_sel",
		vlp_kp_irq_gen_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 1/* width */,
		23/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_KP_IRQ_GEN_SHIFT/* upd shift */, VLP_OCIC_FENC_STATUS_MON_0/* fenc ofs */,
		9/* fenc shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SSR_PKA_SEL/* dts */, "vlp_ssr_pka_sel",
		vlp_ssr_pka_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSR_PKA_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_6 */
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SSR_DMA_SEL/* dts */, "vlp_ssr_dma_sel",
		vlp_ssr_dma_parents/* parent */, VLP_CLK_CFG_6, VLP_CLK_CFG_6_SET,
		VLP_CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSR_DMA_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SSR_KDF_SEL/* dts */, "vlp_ssr_kdf_sel",
		vlp_ssr_kdf_parents/* parent */, VLP_CLK_CFG_6, VLP_CLK_CFG_6_SET,
		VLP_CLK_CFG_6_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSR_KDF_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_TOP_SSR_RNG_SEL/* dts */, "vlp_ssr_rng_sel",
		vlp_ssr_rng_parents/* parent */, VLP_CLK_CFG_6, VLP_CLK_CFG_6_SET,
		VLP_CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSR_RNG_SHIFT/* upd shift */),
#endif
};

static const struct mtk_composite top_composites[] = {
	/* CLK_AUDDIV_2 */
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SIN1/* dts */, "top_apll12_div_i2sin1"/* ccf */,
		"top_apll_i2sin1_m_sel"/* parent */, 0x0320/* pdn ofs */,
		1/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SIN2/* dts */, "top_apll12_div_i2sin2"/* ccf */,
		"top_apll_i2sin2_m_sel"/* parent */, 0x0320/* pdn ofs */,
		2/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		16/* lsb */),
};


enum subsys_id {
	APMIXEDSYS = 0,
	PLL_SYS_NUM,
};

static const struct mtk_pll_data *plls_data[PLL_SYS_NUM];
static void __iomem *plls_base[PLL_SYS_NUM];

#define MT6858_PLL_FMAX		(3800UL * MHZ)
#define MT6858_PLL_FMIN		(1500UL * MHZ)
#define MT6858_INTEGER_BITS	8

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
		.fmax = MT6858_PLL_FMAX,				\
		.fmin = MT6858_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6858_INTEGER_BITS,			\
		.ops = &mtk_pll_fenc_ops,				\
	}

static const struct mtk_pll_data apmixed_plls[] = {
	PLL_FENC(CLK_APMIXED_ARMPLL_LL, "armpll-ll",
		0x0428/*fenc*/, 2, PLL_AO,
		ARMPLL_LL_CON1, 24/*pd*/,
		ARMPLL_LL_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED_ARMPLL_BL, "armpll-bl",
		0x0428/*fenc*/, 1, PLL_AO,
		ARMPLL_BL_CON1, 24/*pd*/,
		ARMPLL_BL_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED_CCIPLL, "ccipll",
		0x0428/*fenc*/, 0, PLL_AO,
		CCIPLL_CON1, 24/*pd*/,
		CCIPLL_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED_MAINPLL, "mainpll",
		0x0428/*fenc*/, 11, HAVE_RST_BAR,
		MAINPLL_CON1, 24/*pd*/,
		MAINPLL_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED_UNIVPLL, "univpll",
		0x0428/*fenc*/, 10, HAVE_RST_BAR,
		UNIVPLL_CON1, 24/*pd*/,
		UNIVPLL_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED_MSDCPLL, "msdcpll",
		0x0428/*fenc*/, 9, 0,
		MSDCPLL_CON1, 24/*pd*/,
		MSDCPLL_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED_MMPLL, "mmpll",
		0x0428/*fenc*/, 4, HAVE_RST_BAR,
		MMPLL_CON1, 24/*pd*/,
		MMPLL_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED_TVDPLL, "tvdpll",
		0x0428/*fenc*/, 3, 0,
		TVDPLL_CON1, 24/*pd*/,
		TVDPLL_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED_EMIPLL, "emipll",
		0x0428/*fenc*/, 7, 0,
		EMIPLL_CON1, 24/*pd*/,
		EMIPLL_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED_ADSPPLL, "adsppll",
		0x0428/*fenc*/, 8, 0,
		ADSPPLL_CON1, 24/*pd*/,
		ADSPPLL_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED_APLL1, "apll1",
		0x0428/*fenc*/, 6, 0,
		APLL1_CON1, 24/*pd*/,
		APLL1_CON2, 0, 32/*pcw*/),
	PLL_FENC(CLK_APMIXED_APLL2, "apll2",
		0x0428/*fenc*/, 5, 0,
		APLL2_CON1, 24/*pd*/,
		APLL2_CON2, 0, 32/*pcw*/),
};

static int clk_mt6858_pll_registration(enum subsys_id id,
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

static int clk_mt6858_apmixed_probe(struct platform_device *pdev)
{
	return clk_mt6858_pll_registration(APMIXEDSYS, apmixed_plls,
			pdev, ARRAY_SIZE(apmixed_plls));
}

static int clk_mt6858_top_probe(struct platform_device *pdev)
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

	clk_data = mtk_alloc_clk_data(CLK_TOP_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_factors(top_divs, ARRAY_SIZE(top_divs),
			clk_data);

	mtk_clk_register_muxes(top_muxes, ARRAY_SIZE(top_muxes), node,
			&mt6858_clk_lock, clk_data);

	mtk_clk_register_composites(top_composites, ARRAY_SIZE(top_composites),
			base, &mt6858_clk_lock, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6858_vlp_top_probe(struct platform_device *pdev)
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

	clk_data = mtk_alloc_clk_data(CLK_VLP_TOP_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_factors(vlp_top_divs, ARRAY_SIZE(vlp_top_divs),
			clk_data);

	mtk_clk_register_muxes(vlp_top_muxes, ARRAY_SIZE(vlp_top_muxes), node,
			&mt6858_clk_lock, clk_data);

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

void mt6858_pll_force_off(void)
{
	int i;

	for (i = 0; i < PLL_SYS_NUM; i++)
		pll_force_off_internal(plls_data[i], plls_base[i]);
}
EXPORT_SYMBOL_GPL(mt6858_pll_force_off);

static const struct of_device_id of_match_clk_mt6858[] = {
#if IS_ENABLED(CONFIG_MTK_CLKMGR_FTRACE)
	{
		.compatible = "clk-ftrace",
		.data = clk_set_trace_event,
	},
#endif
	{
		.compatible = "mediatek,mt6858-apmixedsys",
		.data = clk_mt6858_apmixed_probe,
	}, {
		.compatible = "mediatek,mt6858-topckgen",
		.data = clk_mt6858_top_probe,
	}, {
		.compatible = "mediatek,mt6858-vlp_cksys_top",
		.data = clk_mt6858_vlp_top_probe,
	}, {
		/* sentinel */
	}
};

static int clk_mt6858_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6858_drv = {
	.probe = clk_mt6858_probe,
	.driver = {
		.name = "clk-mt6858",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6858,
	},
};

module_platform_driver(clk_mt6858_drv);
MODULE_LICENSE("GPL");

