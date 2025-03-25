// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Kuan-hsin.Lee <kuan-hsin.lee@mediatek.com>
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

#include <dt-bindings/clock/mt6993-clk.h>

/* bringup config */

#define MT_CCF_BRINGUP		1
#define MT_CCF_PLL_DISABLE	0
#define MT_CCF_MUX_DISABLE	0

/* Regular Number Definition */
#define INV_OFS	-1
#define INV_BIT	-1

/* TOPCK MUX SEL REG */
#define CKSYS2_CLK_CFG_UPDATE			0x0004
#define CLK_CFG_UPDATE				0x0004
#define CLK_CFG_UPDATE1				0x0008
#define CLK_CFG_UPDATE				0x0004
#define CLK_CFG_UPDATE1				0x0008
#define CLK_CFG_UPDATE2				0x000c
#define CKSYS2_CLK_CFG_0			0x0010
#define CKSYS2_CLK_CFG_0_SET			0x0014
#define CKSYS2_CLK_CFG_0_CLR			0x0018
#define CKSYS2_CLK_CFG_1			0x0020
#define CKSYS2_CLK_CFG_1_SET			0x0024
#define CKSYS2_CLK_CFG_1_CLR			0x0028
#define CKSYS2_CLK_CFG_2			0x0030
#define CKSYS2_CLK_CFG_2_SET			0x0034
#define CKSYS2_CLK_CFG_2_CLR			0x0038
#define CKSYS2_CLK_CFG_3			0x0040
#define CKSYS2_CLK_CFG_3_SET			0x0044
#define CKSYS2_CLK_CFG_3_CLR			0x0048
#define CKSYS2_CLK_CFG_4			0x0050
#define CKSYS2_CLK_CFG_4_SET			0x0054
#define CKSYS2_CLK_CFG_4_CLR			0x0058
#define CKSYS2_CLK_CFG_5			0x0060
#define CKSYS2_CLK_CFG_5_SET			0x0064
#define CKSYS2_CLK_CFG_5_CLR			0x0068
#define VLP_CLK_CFG_0				0x0010
#define VLP_CLK_CFG_0_SET				0x0014
#define VLP_CLK_CFG_0_CLR				0x0018
#define VLP_CLK_CFG_1				0x0020
#define VLP_CLK_CFG_1_SET				0x0024
#define VLP_CLK_CFG_1_CLR				0x0028
#define VLP_CLK_CFG_2				0x0030
#define VLP_CLK_CFG_2_SET				0x0034
#define VLP_CLK_CFG_2_CLR				0x0038
#define VLP_CLK_CFG_3				0x0040
#define VLP_CLK_CFG_3_SET				0x0044
#define VLP_CLK_CFG_3_CLR				0x0048
#define VLP_CLK_CFG_4				0x0050
#define VLP_CLK_CFG_4_SET				0x0054
#define VLP_CLK_CFG_4_CLR				0x0058
#define VLP_CLK_CFG_5				0x0060
#define VLP_CLK_CFG_5_SET				0x0064
#define VLP_CLK_CFG_5_CLR				0x0068
#define VLP_CLK_CFG_6				0x0070
#define VLP_CLK_CFG_6_SET				0x0074
#define VLP_CLK_CFG_6_CLR				0x0078
#define VLP_CLK_CFG_7				0x0080
#define VLP_CLK_CFG_7_SET				0x0084
#define VLP_CLK_CFG_7_CLR				0x0088
#define VLP_CLK_CFG_8				0x0090
#define VLP_CLK_CFG_8_SET				0x0094
#define VLP_CLK_CFG_8_CLR				0x0098
#define VLP_CLK_CFG_9				0x00a0
#define VLP_CLK_CFG_9_SET				0x00a4
#define VLP_CLK_CFG_9_CLR				0x00a8
#define VLP_CLK_CFG_10				0x00b0
#define VLP_CLK_CFG_10_SET				0x00b4
#define VLP_CLK_CFG_10_CLR				0x00b8
#define VLP_CLK_CFG_11				0x00c0
#define VLP_CLK_CFG_11_SET				0x00c4
#define VLP_CLK_CFG_11_CLR				0x00c8
#define VLP_CLK_CFG_12				0x00d0
#define VLP_CLK_CFG_12_SET				0x00d4
#define VLP_CLK_CFG_12_CLR				0x00d8
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
#define CLK_CFG_9				0x00a0
#define CLK_CFG_9_SET				0x00a4
#define CLK_CFG_9_CLR				0x00a8
#define CLK_CFG_10				0x00b0
#define CLK_CFG_10_SET				0x00b4
#define CLK_CFG_10_CLR				0x00b8
#define CLK_CFG_11				0x00c0
#define CLK_CFG_11_SET				0x00c4
#define CLK_CFG_11_CLR				0x00c8
#define CLK_CFG_12				0x00d0
#define CLK_CFG_12_SET				0x00d4
#define CLK_CFG_12_CLR				0x00d8
#define CLK_CFG_13				0x00e0
#define CLK_CFG_13_SET				0x00e4
#define CLK_CFG_13_CLR				0x00e8
#define CLK_CFG_14				0x00f0
#define CLK_CFG_14_SET				0x00f4
#define CLK_CFG_14_CLR				0x00f8
#define CLK_CFG_15				0x0100
#define CLK_CFG_15_SET				0x0104
#define CLK_CFG_15_CLR				0x0108
#define CLK_CFG_16				0x0110
#define CLK_CFG_16_SET				0x0114
#define CLK_CFG_16_CLR				0x0118
#define CLK_CFG_17				0x0120
#define CLK_CFG_17_SET				0x0124
#define CLK_CFG_17_CLR				0x0128
#define CLK_CFG_20				0x0150
#define CLK_CFG_20_SET				0x0154
#define CLK_CFG_20_CLR				0x0158
#define CLK_AUDDIV_0				0x0800

/* TOPCK MUX SHIFT */
#define TOP_MUX_MMUP_SHIFT			0
#define TOP_MUX_MMINFRA_AO_SHIFT		1
#define TOP_MUX_MMINFRA_SHIFT			2
#define TOP_MUX_MMINFRA_SNOC_SHIFT		3
#define TOP_MUX_VENC_SHIFT			4
#define TOP_MUX_VENC_MDP_SHIFT			5
#define TOP_MUX_VDEC_SHIFT			6
#define TOP_MUX_IMG1_SHIFT			7
#define TOP_MUX_IPE_SHIFT			8
#define TOP_MUX_DISP_SHIFT			9
#define TOP_MUX_MML_SHIFT			10
#define TOP_MUX_DVO_DP_SHIFT			11
#define TOP_MUX_DVO_FAVT_DP_SHIFT		12
#define TOP_MUX_CAM_SHIFT			13
#define TOP_MUX_CAMTM_SHIFT			14
#define TOP_MUX_CCUSYS_SHIFT			15
#define TOP_MUX_CCUTM_SHIFT			16
#define TOP_MUX_SENINF0_SHIFT			17
#define TOP_MUX_SENINF1_SHIFT			18
#define TOP_MUX_SENINF2_SHIFT			19
#define TOP_MUX_SENINF3_SHIFT			20
#define TOP_MUX_SENINF4_SHIFT			21
#define TOP_MUX_SENINF5_SHIFT			22
#define TOP_MUX_MMINFRA_SNOC_SLOW_SHIFT		23
#define TOP_MUX_SSPM_26M_SHIFT			0
#define TOP_MUX_ULPOSC_SSPM_SHIFT		1
#define TOP_MUX_SSPM_SHIFT			2
#define TOP_MUX_SPM_SHIFT			3
#define TOP_MUX_AXI_VLP_SHIFT			4
#define TOP_MUX_NOC_VLP_SHIFT			5
#define TOP_MUX_PWM_VLP_SHIFT			6
#define TOP_MUX_SYSTIMER_26M_SHIFT		7
#define TOP_MUX_DPSW_SHIFT			8
#define TOP_MUX_DPSW_CENTRAL_SHIFT		9
#define TOP_MUX_SRCK_SHIFT			10
#define TOP_MUX_DVFSRC_SHIFT			11
#define TOP_MUX_KP_IRQ_GEN_SHIFT		12
#define TOP_MUX_DEBUG_ERR_FLAG_VLP_26M_SHIFT	13
#define TOP_MUX_IPS_SHIFT			14
#define TOP_MUX_DPMSRDMA_SHIFT			15
#define TOP_MUX_VLP_PBUS_SHIFT			16
#define TOP_MUX_VLP_PBUS_26M_SHIFT		17
#define TOP_MUX_VCORE_PBUS_SHIFT		18
#define TOP_MUX_VCORE_PBUS_26M_SHIFT		19
#define TOP_MUX_CAMTG0_SHIFT			20
#define TOP_MUX_CAMTG1_SHIFT			21
#define TOP_MUX_CAMTG2_SHIFT			22
#define TOP_MUX_CAMTG3_SHIFT			23
#define TOP_MUX_CAMTG4_SHIFT			24
#define TOP_MUX_CAMTG5_SHIFT			25
#define TOP_MUX_CAMTG6_SHIFT			26
#define TOP_MUX_CAMTG7_SHIFT			27
#define TOP_MUX_AUD_ENGEN1_SHIFT		28
#define TOP_MUX_AUD_ENGEN2_SHIFT		29
#define TOP_MUX_AUD_SW_ENGEN1_SHIFT		30
#define TOP_MUX_AUD_SW_ENGEN2_SHIFT		0
#define TOP_MUX_AUD_INTBUS_SHIFT		1
#define TOP_MUX_AUDIO_H_SHIFT			2
#define TOP_MUX_USB_TOP_SHIFT			3
#define TOP_MUX_SSUSB_XHCI_SHIFT		4
#define TOP_MUX_SPU0_VLP_SHIFT			6
#define TOP_MUX_SCP_SHIFT			9
#define TOP_MUX_SCP_SPI_SHIFT			10
#define TOP_MUX_SCP_IIC_SHIFT			11
#define TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT		12
#define TOP_MUX_USB_MEM_VLP_SHIFT		14
#define TOP_MUX_DISP_PWM_SHIFT			15
#define TOP_MUX_PWRAP_ULPOSC_SHIFT		17
#define TOP_MUX_TIA_SHIFT			18
#define TOP_MUX_SPMI_M_MST_SHIFT		19
#define TOP_MUX_HVS_SHIFT			20
#define TOP_MUX_AXI_SHIFT			0
#define TOP_MUX_PERI_AXI_SHIFT			1
#define TOP_MUX_CH_INFRA_AXI_SHIFT		2
#define TOP_MUX_CH_INFRA_SHIFT			3
#define TOP_MUX_MEM_SUB_SHIFT			4
#define TOP_MUX_HASH_SUB_SHIFT			5
#define TOP_MUX_PERI_FMEM_SUB_SHIFT		6
#define TOP_MUX_ZRAM_SUB_SHIFT			7
#define TOP_MUX_IO_NOC_SHIFT			8
#define TOP_MUX_HASH_NOC_SHIFT			9
#define TOP_MUX_PERI_NOC_SHIFT			10
#define TOP_MUX_EMI_INTERFACE_546_SHIFT		11
#define TOP_MUX_EMI_N_SHIFT			12
#define TOP_MUX_EMI_S_SHIFT			13
#define TOP_MUX_EMI_INFRA_SHIFT			14
#define TOP_MUX_EMI_INFRA_SSPM_SHIFT		15
#define TOP_MUX_ULPOSC_EMI_INFRA_SHIFT		16
#define TOP_MUX_EMI_INFRA_26M_SHIFT		17
#define TOP_MUX_CH_INFRA_SYS_26M_SHIFT		18
#define TOP_MUX_CBUS_PHY_SHIFT			19
#define TOP_MUX_ATB_SHIFT			20
#define TOP_MUX_CIRQ_SHIFT			21
#define TOP_MUX_MCU_INFRA_SHIFT			22
#define TOP_MUX_APU_EXT_SHIFT			26
#define TOP_MUX_SSR_PKA_SHIFT			28
#define TOP_MUX_SSR_DMA_SHIFT			29
#define TOP_MUX_SSR_KDF_SHIFT			30
#define TOP_MUX_SSR_RNG_SHIFT			0
#define TOP_MUX_EFUSE_SHIFT			2
#define TOP_MUX_DPSW_CMP_26M_SHIFT		4
#define TOP_MUX_ADSP_UARTHUB_BCLK_SHIFT		5
#define TOP_MUX_AUD_1_SHIFT			6
#define TOP_MUX_AUD_2_SHIFT			7
#define TOP_MUX_DPMAIF_MAIN_SHIFT		8
#define TOP_MUX_IPSEAST_SHIFT			9
#define TOP_MUX_IPSWEST_SHIFT			10
#define TOP_MUX_SMAPCK_SHIFT			11
#define TOP_MUX_IPIC_SHIFT			12
#define TOP_MUX_SPI0_BCLK_SHIFT			13
#define TOP_MUX_SPI1_BCLK_SHIFT			14
#define TOP_MUX_SPI2_BCLK_SHIFT			15
#define TOP_MUX_SPI3_BCLK_SHIFT			16
#define TOP_MUX_SPI4_BCLK_SHIFT			17
#define TOP_MUX_SPI5_BCLK_SHIFT			18
#define TOP_MUX_SPI6_BCLK_SHIFT			19
#define TOP_MUX_SPI7_BCLK_SHIFT			20
#define TOP_MUX_TL_SHIFT			22
#define TOP_MUX_TL_P1_SHIFT			23
#define TOP_MUX_PWM_SHIFT			24
#define TOP_MUX_AES_UFSFDE_0_SHIFT		25
#define TOP_MUX_UFS_0_SHIFT			26
#define TOP_MUX_AES_UFSFDE_1_SHIFT		28
#define TOP_MUX_UFS_1_SHIFT			29
#define TOP_MUX_UARTHUB_BCLK_SHIFT		0
#define TOP_MUX_UART_SHIFT			1
#define TOP_MUX_I2C_PERI_SHIFT			2
#define TOP_MUX_I2C_NORTH_SHIFT			3
#define TOP_MUX_I2C_EAST_SHIFT			4
#define TOP_MUX_I2C_WEST_SHIFT			5
#define TOP_MUX_MSDC_MACRO_1P_SHIFT		6
#define TOP_MUX_MSDC_MACRO_2P_SHIFT		7
#define TOP_MUX_MSDC30_1_SHIFT			8
#define TOP_MUX_MSDC30_2_SHIFT			9
#define TOP_MUX_GRIDSENSOR_SHIFT		19
#define TOP_MUX_AOV_26M_SHIFT			20
#define TOP_MUX_EMI_WDAT_SHIFT			21

/* TOPCK CKSTA REG */
#define CKSYS2_CKSTA_REG			0x0330
#define VLP_CKSTA_REG				0x0330
#define VLP_CKSTA_REG1				0x0334
#define CKSTA_REG				0x0330
#define CKSTA_REG1				0x0334
#define CKSTA_REG2				0x0338

/* TOPCK FENC REG */
#define CKSYS2_CLK_FENC_STATUS_MON_0		0x0270
#define VLP_CLK_FENC_STATUS_MON_0		0x0270
#define VLP_CLK_FENC_STATUS_MON_1		0x0274
#define CLK_FENC_STATUS_MON_0			0x0270
#define CLK_FENC_STATUS_MON_1			0x0274
#define CLK_FENC_STATUS_MON_2			0x0278

/* TOPCK DIVIDER REG */
#define CLK_AUDDIV_2				0x0808
#define CLK_AUDDIV_3				0x080c
#define CLK_AUDDIV_4				0x0810
#define CLK_AUDDIV_5				0x0814

/* APMIXED PLL REG */
#define CCIPLL_CON0				0x008
#define CCIPLL_CON1				0x00C
#define CCIPLL_CON2				0x010
#define CCIPLL_CON3				0x014
#define PTPPLL_CON0				0x008
#define PTPPLL_CON1				0x00C
#define PTPPLL_CON2				0x010
#define PTPPLL_CON3				0x014
#define MAINPLL2_CON0				0x008
#define MAINPLL2_CON1				0x00C
#define MAINPLL2_CON2				0x010
#define MAINPLL2_CON3				0x014
#define UNIVPLL2_CON0				0x008
#define UNIVPLL2_CON1				0x00C
#define UNIVPLL2_CON2				0x010
#define UNIVPLL2_CON3				0x014
#define MMPLL_CON0				0x008
#define MMPLL_CON1				0x00C
#define MMPLL_CON2				0x010
#define MMPLL_CON3				0x014
#define IMGPLL_CON0				0x008
#define IMGPLL_CON1				0x00C
#define IMGPLL_CON2				0x010
#define IMGPLL_CON3				0x014
#define TVDPLL_CON0				0x008
#define TVDPLL_CON1				0x00C
#define TVDPLL_CON2				0x010
#define TVDPLL_CON3				0x014
#define APLL1_TUNER_CON0			0x038
#define APLL1_PCW_CON1				0x040
#define APLL1_PCW_CON0				0x03C
#define APLL1_CON0				0x008
#define APLL1_CON1				0x00C
#define APLL1_CON2				0x010
#define APLL1_CON3				0x014
#define APLL2_TUNER_CON0			0x038
#define APLL2_PCW_CON1				0x040
#define APLL2_PCW_CON0				0x03C
#define APLL2_CON0				0x008
#define APLL2_CON1				0x00C
#define APLL2_CON2				0x010
#define APLL2_CON3				0x014
#define MAINPLL_CON0				0x008
#define MAINPLL_CON1				0x00C
#define MAINPLL_CON2				0x010
#define MAINPLL_CON3				0x014
#define UNIVPLL_CON0				0x008
#define UNIVPLL_CON1				0x00C
#define UNIVPLL_CON2				0x010
#define UNIVPLL_CON3				0x014
#define MSDCPLL_CON0				0x008
#define MSDCPLL_CON1				0x00C
#define MSDCPLL_CON2				0x010
#define MSDCPLL_CON3				0x014
#define EMIPLL_CON0				0x008
#define EMIPLL_CON1				0x00C
#define EMIPLL_CON2				0x010
#define EMIPLL_CON3				0x014
#define MFG_PLL0_CON0				0x008
#define MFG_PLL0_CON1				0x00C
#define MFG_PLL0_CON2				0x010
#define MFG_PLL0_CON3				0x014
#define MFG_PLL1_CON0				0x008
#define MFG_PLL1_CON1				0x00C
#define MFG_PLL1_CON2				0x010
#define MFG_PLL1_CON3				0x014

/* HW Voter REG */
#define AP_HWV_HWV_CG_GRP_0_SET			0x0
#define AP_HWV_HWV_CG_GRP_0_CLR			0x4
#define AP_HWV_HWV_CG_GRP_0_DONE		0x12600
#define AP_HWV_HWV_CG_GRP_1_SET			0xc
#define AP_HWV_HWV_CG_GRP_1_CLR			0x10
#define AP_HWV_HWV_CG_GRP_1_DONE		0x12604
#define AP_HWV_HWV_CG_GRP_2_SET			0x18
#define AP_HWV_HWV_CG_GRP_2_CLR			0x1c
#define AP_HWV_HWV_CG_GRP_2_DONE		0x12608
#define AP_HWV_HWV_CG_GRP_3_SET			0x24
#define AP_HWV_HWV_CG_GRP_3_CLR			0x28
#define AP_HWV_HWV_CG_GRP_3_DONE		0x1260C
#define AP_HWV_HWV_CG_GRP_4_SET			0x30
#define AP_HWV_HWV_CG_GRP_4_CLR			0x34
#define AP_HWV_HWV_CG_GRP_4_DONE		0x12610
#define AP_HWV_HWV_CG_GRP_5_SET			0x3c
#define AP_HWV_HWV_CG_GRP_5_CLR			0x40
#define AP_HWV_HWV_CG_GRP_5_DONE		0x12614
#define AP_HWV_HWV_CG_GRP_6_SET			0x48
#define AP_HWV_HWV_CG_GRP_6_CLR			0x4c
#define AP_HWV_HWV_CG_GRP_6_DONE		0x12618

#define AP_HWV_HWV_MUX_PWR_GRP_0_SET    0x600
#define AP_HWV_HWV_MUX_PWR_GRP_0_CLR    0x604
#define AP_HWV_HWV_MUX_PWR_GRP_0_DONE   0x12730
#define MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET    0x600
#define MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR    0x604
#define MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE   0x12730

#define MM_HW_CCF_HW_CCF_GRP_0_SET		0x0
#define MM_HW_CCF_HW_CCF_GRP_0_CLR		0x4
#define MM_HW_CCF_HW_CCF_GRP_0_DONE		0x12600
#define MM_HW_CCF_HW_CCF_GRP_1_SET		0xc
#define MM_HW_CCF_HW_CCF_GRP_1_CLR		0x10
#define MM_HW_CCF_HW_CCF_GRP_1_DONE		0x12604
#define MM_HW_CCF_HW_CCF_GRP_2_SET		0x18
#define MM_HW_CCF_HW_CCF_GRP_2_CLR		0x1c
#define MM_HW_CCF_HW_CCF_GRP_2_DONE		0x12608
#define MM_HW_CCF_HW_CCF_GRP_3_SET		0x24
#define MM_HW_CCF_HW_CCF_GRP_3_CLR		0x28
#define MM_HW_CCF_HW_CCF_GRP_3_DONE		0x1260C
#define MM_HW_CCF_HW_CCF_GRP_4_SET		0x30
#define MM_HW_CCF_HW_CCF_GRP_4_CLR		0x34
#define MM_HW_CCF_HW_CCF_GRP_4_DONE		0x12610
#define MM_HW_CCF_HW_CCF_GRP_5_SET		0x3c
#define MM_HW_CCF_HW_CCF_GRP_5_CLR		0x40
#define MM_HW_CCF_HW_CCF_GRP_5_DONE		0x12614
#define MM_HW_CCF_HW_CCF_GRP_6_SET		0x48
#define MM_HW_CCF_HW_CCF_GRP_6_CLR		0x4c
#define MM_HW_CCF_HW_CCF_GRP_6_DONE		0x12618
#define MM_HW_CCF_HW_CCF_GRP_7_SET		0x54
#define MM_HW_CCF_HW_CCF_GRP_7_CLR		0x58
#define MM_HW_CCF_HW_CCF_GRP_7_DONE		0x1261C
#define MM_HW_CCF_HW_CCF_GRP_8_SET		0x60
#define MM_HW_CCF_HW_CCF_GRP_8_CLR		0x64
#define MM_HW_CCF_HW_CCF_GRP_8_DONE		0x12620
#define MM_HW_CCF_HW_CCF_GRP_10_SET		0x78
#define MM_HW_CCF_HW_CCF_GRP_10_CLR		0x7c
#define MM_HW_CCF_HW_CCF_GRP_10_DONE		0x12628
#define MM_HW_CCF_HW_CCF_GRP_11_SET		0x84
#define MM_HW_CCF_HW_CCF_GRP_11_CLR		0x88
#define MM_HW_CCF_HW_CCF_GRP_11_DONE		0x1262C
#define MM_HW_CCF_HW_CCF_GRP_12_SET		0x90
#define MM_HW_CCF_HW_CCF_GRP_12_CLR		0x94
#define MM_HW_CCF_HW_CCF_GRP_12_DONE		0x12630
#define MM_HW_CCF_HW_CCF_GRP_13_SET		0x9c
#define MM_HW_CCF_HW_CCF_GRP_13_CLR		0xa0
#define MM_HW_CCF_HW_CCF_GRP_13_DONE		0x12634
#define MM_HW_CCF_HW_CCF_GRP_14_SET		0xa8
#define MM_HW_CCF_HW_CCF_GRP_14_CLR		0xac
#define MM_HW_CCF_HW_CCF_GRP_14_DONE		0x12638
#define MM_HW_CCF_HW_CCF_GRP_15_SET		0xb4
#define MM_HW_CCF_HW_CCF_GRP_15_CLR		0xb8
#define MM_HW_CCF_HW_CCF_GRP_15_DONE		0x1263C
#define MM_HW_CCF_HW_CCF_GRP_16_SET		0xc0
#define MM_HW_CCF_HW_CCF_GRP_16_CLR		0xc4
#define MM_HW_CCF_HW_CCF_GRP_16_DONE		0x12640
#define MM_HW_CCF_HW_CCF_GRP_17_SET		0xcc
#define MM_HW_CCF_HW_CCF_GRP_17_CLR		0xd0
#define MM_HW_CCF_HW_CCF_GRP_17_DONE		0x12644
#define MM_HW_CCF_HW_CCF_GRP_18_SET		0xd8
#define MM_HW_CCF_HW_CCF_GRP_18_CLR		0xdc
#define MM_HW_CCF_HW_CCF_GRP_18_DONE		0x12648
#define MM_HW_CCF_HW_CCF_GRP_19_SET		0xe4
#define MM_HW_CCF_HW_CCF_GRP_19_CLR		0xe8
#define MM_HW_CCF_HW_CCF_GRP_19_DONE		0x1264C
#define MM_HW_CCF_HW_CCF_GRP_20_SET		0xf0
#define MM_HW_CCF_HW_CCF_GRP_20_CLR		0xf4
#define MM_HW_CCF_HW_CCF_GRP_20_DONE		0x12650
#define MM_HW_CCF_HW_CCF_GRP_22_SET		0x108
#define MM_HW_CCF_HW_CCF_GRP_22_CLR		0x10c
#define MM_HW_CCF_HW_CCF_GRP_22_DONE		0x12658
#define MM_HW_CCF_HW_CCF_GRP_23_SET		0x114
#define MM_HW_CCF_HW_CCF_GRP_23_CLR		0x118
#define MM_HW_CCF_HW_CCF_GRP_23_DONE		0x1265C
#define MM_HW_CCF_HW_CCF_GRP_24_SET		0x120
#define MM_HW_CCF_HW_CCF_GRP_24_CLR		0x124
#define MM_HW_CCF_HW_CCF_GRP_24_DONE		0x12660
#define MM_HW_CCF_HW_CCF_GRP_25_SET		0x12c
#define MM_HW_CCF_HW_CCF_GRP_25_CLR		0x130
#define MM_HW_CCF_HW_CCF_GRP_25_DONE		0x12664
#define MM_HW_CCF_HW_CCF_GRP_26_SET		0x138
#define MM_HW_CCF_HW_CCF_GRP_26_CLR		0x13c
#define MM_HW_CCF_HW_CCF_GRP_26_DONE		0x12668
#define MM_HW_CCF_HW_CCF_GRP_27_SET		0x144
#define MM_HW_CCF_HW_CCF_GRP_27_CLR		0x148
#define MM_HW_CCF_HW_CCF_GRP_27_DONE		0x1266C
#define MM_HW_CCF_HW_CCF_GRP_28_SET		0x150
#define MM_HW_CCF_HW_CCF_GRP_28_CLR		0x154
#define MM_HW_CCF_HW_CCF_GRP_28_DONE		0x12670
#define MM_HW_CCF_HW_CCF_GRP_29_SET		0x15c
#define MM_HW_CCF_HW_CCF_GRP_29_CLR		0x160
#define MM_HW_CCF_HW_CCF_GRP_29_DONE		0x12674
#define MM_HW_CCF_HW_CCF_GRP_30_SET		0x168
#define MM_HW_CCF_HW_CCF_GRP_30_CLR		0x16c
#define MM_HW_CCF_HW_CCF_GRP_30_DONE		0x12678
#define MM_HW_CCF_HW_CCF_GRP_31_SET		0x174
#define MM_HW_CCF_HW_CCF_GRP_31_CLR		0x178
#define MM_HW_CCF_HW_CCF_GRP_31_DONE		0x1267C
#define MM_HW_CCF_HW_CCF_GRP_32_SET		0x180
#define MM_HW_CCF_HW_CCF_GRP_32_CLR		0x184
#define MM_HW_CCF_HW_CCF_GRP_32_DONE		0x12680
#define MM_HW_CCF_HW_CCF_GRP_33_SET		0x18c
#define MM_HW_CCF_HW_CCF_GRP_33_CLR		0x190
#define MM_HW_CCF_HW_CCF_GRP_33_DONE		0x12684
#define MM_HW_CCF_HW_CCF_GRP_34_SET		0x198
#define MM_HW_CCF_HW_CCF_GRP_34_CLR		0x19c
#define MM_HW_CCF_HW_CCF_GRP_34_DONE		0x12688
#define MM_HW_CCF_HW_CCF_GRP_35_SET		0x1a4
#define MM_HW_CCF_HW_CCF_GRP_35_CLR		0x1a8
#define MM_HW_CCF_HW_CCF_GRP_35_DONE		0x1268C
#define MM_HW_CCF_HW_CCF_GRP_36_SET		0x1b0
#define MM_HW_CCF_HW_CCF_GRP_36_CLR		0x1b4
#define MM_HW_CCF_HW_CCF_GRP_36_DONE		0x12690
#define MM_HW_CCF_HW_CCF_GRP_37_SET		0x1bc
#define MM_HW_CCF_HW_CCF_GRP_37_CLR		0x1c0
#define MM_HW_CCF_HW_CCF_GRP_37_DONE		0x12694
#define MM_HW_CCF_HW_CCF_GRP_38_SET		0x1c8
#define MM_HW_CCF_HW_CCF_GRP_38_CLR		0x1cc
#define MM_HW_CCF_HW_CCF_GRP_38_DONE		0x12698
#define MM_HW_CCF_HW_CCF_GRP_39_SET		0x1d4
#define MM_HW_CCF_HW_CCF_GRP_39_CLR		0x1d8
#define MM_HW_CCF_HW_CCF_GRP_39_DONE		0x1269C
#define MM_HW_CCF_HW_CCF_GRP_40_SET		0x1e0
#define MM_HW_CCF_HW_CCF_GRP_40_CLR		0x1e4
#define MM_HW_CCF_HW_CCF_GRP_40_DONE		0x126A0
#define MM_HW_CCF_HW_CCF_GRP_41_SET		0x1ec
#define MM_HW_CCF_HW_CCF_GRP_41_CLR		0x1f0
#define MM_HW_CCF_HW_CCF_GRP_41_DONE		0x126A4
#define MM_HW_CCF_HW_CCF_GRP_42_SET		0x1f8
#define MM_HW_CCF_HW_CCF_GRP_42_CLR		0x1fc
#define MM_HW_CCF_HW_CCF_GRP_42_DONE		0x126A8
#define MM_HW_CCF_HW_CCF_GRP_43_SET		0x204
#define MM_HW_CCF_HW_CCF_GRP_43_CLR		0x208
#define MM_HW_CCF_HW_CCF_GRP_43_DONE		0x126AC
#define MM_HW_CCF_HW_CCF_GRP_44_SET		0x210
#define MM_HW_CCF_HW_CCF_GRP_44_CLR		0x214
#define MM_HW_CCF_HW_CCF_GRP_44_DONE		0x126B0
#define MM_HW_CCF_HW_CCF_GRP_45_SET		0x21c
#define MM_HW_CCF_HW_CCF_GRP_45_CLR		0x220
#define MM_HW_CCF_HW_CCF_GRP_45_DONE		0x126B4
#define MM_HW_CCF_HW_CCF_GRP_46_SET		0x228
#define MM_HW_CCF_HW_CCF_GRP_46_CLR		0x22c
#define MM_HW_CCF_HW_CCF_GRP_46_DONE		0x126B8
#define MM_HW_CCF_HW_CCF_GRP_47_SET		0x234
#define MM_HW_CCF_HW_CCF_GRP_47_CLR		0x238
#define MM_HW_CCF_HW_CCF_GRP_47_DONE		0x126BC
#define MM_HW_CCF_HW_CCF_GRP_48_SET		0x240
#define MM_HW_CCF_HW_CCF_GRP_48_CLR		0x244
#define MM_HW_CCF_HW_CCF_GRP_48_DONE		0x126C0
#define MM_HW_CCF_HW_CCF_GRP_49_SET		0x24c
#define MM_HW_CCF_HW_CCF_GRP_49_CLR		0x250
#define MM_HW_CCF_HW_CCF_GRP_49_DONE		0x126C4
#define MM_HW_CCF_HW_CCF_GRP_50_SET		0x258
#define MM_HW_CCF_HW_CCF_GRP_50_CLR		0x25c
#define MM_HW_CCF_HW_CCF_GRP_50_DONE		0x126C8
#define MM_HW_CCF_HW_CCF_GRP_51_SET		0x264
#define MM_HW_CCF_HW_CCF_GRP_51_CLR		0x268
#define MM_HW_CCF_HW_CCF_GRP_51_DONE		0x126CC
#define MM_HW_CCF_HW_CCF_GRP_52_SET		0x0
#define MM_HW_CCF_HW_CCF_GRP_52_CLR		0x4
#define MM_HW_CCF_HW_CCF_GRP_52_DONE		0x12600

static DEFINE_SPINLOCK(mt6993_clk_lock);

static const struct mtk_fixed_factor cksys_top_divs[] = {
	FACTOR(CKSYS_TOP_CLKRTC, "cksys_clkrtc",
			"clk32k", 1, 1),
	FACTOR(CKSYS_TOP_TCK_26M_MX9, "cksys_tck_26m_mx9_ck",
			"clk26m", 1, 1),
	FACTOR(CKSYS_TOP_MAINPLL_D2, "cksys_mainpll_d2",
			"mainpll", 1, 2),
	FACTOR(CKSYS_TOP_MAINPLL_D3, "cksys_mainpll_d3",
			"mainpll", 1, 3),
	FACTOR(CKSYS_TOP_MAINPLL_D4, "cksys_mainpll_d4",
			"mainpll", 1, 4),
	FACTOR(CKSYS_TOP_MAINPLL_D4_D2, "cksys_mainpll_d4_d2",
			"mainpll", 1, 8),
	FACTOR(CKSYS_TOP_MAINPLL_D4_D4, "cksys_mainpll_d4_d4",
			"mainpll", 1, 16),
	FACTOR(CKSYS_TOP_MAINPLL_D4_D8, "cksys_mainpll_d4_d8",
			"mainpll", 1, 32),
	FACTOR(CKSYS_TOP_MAINPLL_D5, "cksys_mainpll_d5",
			"mainpll", 1, 5),
	FACTOR(CKSYS_TOP_MAINPLL_D5_D2, "cksys_mainpll_d5_d2",
			"mainpll", 1, 10),
	FACTOR(CKSYS_TOP_MAINPLL_D5_D4, "cksys_mainpll_d5_d4",
			"mainpll", 1, 20),
	FACTOR(CKSYS_TOP_MAINPLL_D5_D8, "cksys_mainpll_d5_d8",
			"mainpll", 1, 40),
	FACTOR(CKSYS_TOP_MAINPLL_D6, "cksys_mainpll_d6",
			"mainpll", 1, 6),
	FACTOR(CKSYS_TOP_MAINPLL_D6_D2, "cksys_mainpll_d6_d2",
			"mainpll", 1, 12),
	FACTOR(CKSYS_TOP_MAINPLL_D6_D8, "cksys_mainpll_d6_d8",
			"mainpll", 1, 48),
	FACTOR(CKSYS_TOP_MAINPLL_D7, "cksys_mainpll_d7",
			"mainpll", 1, 7),
	FACTOR(CKSYS_TOP_MAINPLL_D7_D2, "cksys_mainpll_d7_d2",
			"mainpll", 1, 14),
	FACTOR(CKSYS_TOP_MAINPLL_D7_D4, "cksys_mainpll_d7_d4",
			"mainpll", 1, 28),
	FACTOR(CKSYS_TOP_MAINPLL_D9, "cksys_mainpll_d9",
			"mainpll", 1, 9),
	FACTOR(CKSYS_TOP_UNIVPLL_D2, "cksys_univpll_d2",
			"univpll", 1, 2),
	FACTOR(CKSYS_TOP_UNIVPLL_D3, "cksys_univpll_d3",
			"univpll", 1, 3),
	FACTOR(CKSYS_TOP_UNIVPLL_D4, "cksys_univpll_d4",
			"univpll", 1, 4),
	FACTOR(CKSYS_TOP_UNIVPLL_D4_D2, "cksys_univpll_d4_d2",
			"univpll", 1, 8),
	FACTOR(CKSYS_TOP_UNIVPLL_D4_D4, "cksys_univpll_d4_d4",
			"univpll", 1, 16),
	FACTOR(CKSYS_TOP_UNIVPLL_D4_D8, "cksys_univpll_d4_d8",
			"univpll", 1, 32),
	FACTOR(CKSYS_TOP_UNIVPLL_D5, "cksys_univpll_d5",
			"univpll", 1, 5),
	FACTOR(CKSYS_TOP_UNIVPLL_D5_D2, "cksys_univpll_d5_d2",
			"univpll", 1, 10),
	FACTOR(CKSYS_TOP_UNIVPLL_D5_D4, "cksys_univpll_d5_d4",
			"univpll", 1, 20),
	FACTOR(CKSYS_TOP_UNIVPLL_D6, "cksys_univpll_d6",
			"univpll", 1, 6),
	FACTOR(CKSYS_TOP_UNIVPLL_D6_D2, "cksys_univpll_d6_d2",
			"univpll", 1, 12),
	FACTOR(CKSYS_TOP_UNIVPLL_D6_D4, "cksys_univpll_d6_d4",
			"univpll", 1, 24),
	FACTOR(CKSYS_TOP_UNIVPLL_D6_D8, "cksys_univpll_d6_d8",
			"univpll", 1, 48),
	FACTOR(CKSYS_TOP_UNIVPLL_D6_D16, "cksys_univpll_d6_d16",
			"univpll", 1, 96),
	FACTOR(CKSYS_TOP_UNIVPLL_192M, "cksys_univpll_192m",
			"univpll", 1, 13),
	FACTOR(CKSYS_TOP_UNIVPLL_192M_D8, "cksys_univpll_192m_d8",
			"univpll", 1, 104),
	FACTOR(CKSYS_TOP_UNIVPLL_192M_D16, "cksys_univpll_192m_d16",
			"univpll", 1, 208),
	FACTOR(CKSYS_TOP_UNIVPLL_192M_D32, "cksys_univpll_192m_d32",
			"univpll", 1, 416),
	FACTOR(CKSYS_TOP_UNIVPLL_192M_D10, "cksys_univpll_192m_d10",
			"univpll", 1, 130),
	FACTOR(CKSYS_TOP_MSDCPLL, "cksys_msdcpll_ck",
			"msdcpll", 1, 1),
	FACTOR(CKSYS_TOP_MSDCPLL_D2, "cksys_msdcpll_d2",
			"msdcpll", 1, 2),
	FACTOR(CKSYS_TOP_APLL1, "cksys_apll1_ck",
			"apll1", 1, 1),
	FACTOR(CKSYS_TOP_APLL1_D4, "cksys_apll1_d4",
			"apll1", 1, 4),
	FACTOR(CKSYS_TOP_APLL1_D8, "cksys_apll1_d8",
			"apll1", 1, 8),
	FACTOR(CKSYS_TOP_APLL2, "cksys_apll2_ck",
			"apll2", 1, 1),
	FACTOR(CKSYS_TOP_APLL2_D4, "cksys_apll2_d4",
			"apll2", 1, 4),
	FACTOR(CKSYS_TOP_APLL2_D8, "cksys_apll2_d8",
			"apll2", 1, 8),
	FACTOR(CKSYS_TOP_OSC, "cksys_osc",
			"ulposc", 1, 1),
	FACTOR(CKSYS_TOP_OSC_D2, "cksys_osc_d2",
			"ulposc", 1, 2),
	FACTOR(CKSYS_TOP_OSC_D3, "cksys_osc_d3",
			"ulposc", 1, 3),
	FACTOR(CKSYS_TOP_OSC_D4, "cksys_osc_d4",
			"ulposc", 1, 4),
	FACTOR(CKSYS_TOP_OSC_D5, "cksys_osc_d5",
			"ulposc", 1, 5),
	FACTOR(CKSYS_TOP_OSC_D7, "cksys_osc_d7",
			"ulposc", 1, 7),
	FACTOR(CKSYS_TOP_OSC_D8, "cksys_osc_d8",
			"ulposc", 1, 8),
	FACTOR(CKSYS_TOP_OSC_D10, "cksys_osc_d10",
			"ulposc", 1, 10),
	FACTOR(CKSYS_TOP_OSC_D20, "cksys_osc_d20",
			"ulposc", 1, 20),
	FACTOR(CKSYS_TOP_OSC_D32, "cksys_osc_d32",
			"ulposc", 1, 32),
	FACTOR(CKSYS_TOP_OSC_D40, "cksys_osc_d40",
			"ulposc", 1, 40),
	FACTOR(CKSYS_TOP_MMPLL_D2, "cksys_mmpll_d2",
			"mmpll", 1, 2),
	FACTOR(CKSYS_TOP_EMIPLL_D2, "cksys_emipll_d2",
			"emipll", 1, 2),
	FACTOR(CKSYS_TOP_EMIPLL_D3, "cksys_emipll_d3",
			"emipll", 1, 3),
	FACTOR(CKSYS_TOP_F26M, "cksys_f26m_ck",
			"clk26m", 1, 1),
	FACTOR(CKSYS_TOP_RTC, "cksys_rtc_ck",
			"clk32k", 1, 1),
	FACTOR(CKSYS_TOP_P_AXI, "cksys_peri_axi_ck",
			"cksys_peri_axi_sel", 1, 1),
	FACTOR(CKSYS_TOP_P_FMEM_SUB, "cksys_peri_fmem_sub_ck",
			"cksys_peri_fmem_sub_sel", 1, 1),
	FACTOR(CKSYS_TOP_SSR_PKA, "cksys_ssr_pka_ck",
			"cksys_ssr_pka_sel", 1, 1),
	FACTOR(CKSYS_TOP_SSR_DMA, "cksys_ssr_dma_ck",
			"cksys_ssr_dma_sel", 1, 1),
	FACTOR(CKSYS_TOP_SSR_KDF, "cksys_ssr_kdf_ck",
			"cksys_ssr_kdf_sel", 1, 1),
	FACTOR(CKSYS_TOP_SSR_RNG, "cksys_ssr_rng_ck",
			"cksys_ssr_rng_sel", 1, 1),
	FACTOR(CKSYS_TOP_DXCC, "cksys_dxcc_ck",
			"cksys_dxcc_sel", 1, 1),
	FACTOR(CKSYS_TOP_AUD_1, "cksys_aud_1_ck",
			"cksys_aud_1_sel", 1, 1),
	FACTOR(CKSYS_TOP_AUD_2, "cksys_aud_2_ck",
			"cksys_aud_2_sel", 1, 1),
	FACTOR(CKSYS_TOP_DPMAIF_MAIN, "cksys_dpmaif_main_ck",
			"cksys_dpmaif_main_sel", 1, 1),
	FACTOR(CKSYS_TOP_SPI0_BCLK, "cksys_spi0_b_ck",
			"cksys_spi0_b_sel", 1, 1),
	FACTOR(CKSYS_TOP_SPI1_BCLK, "cksys_spi1_b_ck",
			"cksys_spi1_b_sel", 1, 1),
	FACTOR(CKSYS_TOP_SPI2_BCLK, "cksys_spi2_b_ck",
			"cksys_spi2_b_sel", 1, 1),
	FACTOR(CKSYS_TOP_SPI3_BCLK, "cksys_spi3_b_ck",
			"cksys_spi3_b_sel", 1, 1),
	FACTOR(CKSYS_TOP_SPI4_BCLK, "cksys_spi4_b_ck",
			"cksys_spi4_b_sel", 1, 1),
	FACTOR(CKSYS_TOP_SPI5_BCLK, "cksys_spi5_b_ck",
			"cksys_spi5_b_sel", 1, 1),
	FACTOR(CKSYS_TOP_SPI6_BCLK, "cksys_spi6_b_ck",
			"cksys_spi6_b_sel", 1, 1),
	FACTOR(CKSYS_TOP_SPI7_BCLK, "cksys_spi7_b_ck",
			"cksys_spi7_b_sel", 1, 1),
	FACTOR(CKSYS_TOP_TL, "cksys_tl_ck",
			"cksys_tl_sel", 1, 1),
	FACTOR(CKSYS_TOP_TL_P1, "cksys_tl_p1_ck",
			"cksys_tl_p1_sel", 1, 1),
	FACTOR(CKSYS_TOP_AES_UFSFDE_0, "cksys_aes_ufsfde_0_ck",
			"cksys_aes_ufsfde_0_sel", 1, 1),
	FACTOR(CKSYS_TOP_U_0, "cksys_u_0_ck",
			"cksys_u_0_sel", 1, 1),
	FACTOR(CKSYS_TOP_AES_UFSFDE_1, "cksys_aes_ufsfde_1_ck",
			"cksys_aes_ufsfde_1_sel", 1, 1),
	FACTOR(CKSYS_TOP_U_1, "cksys_u_1_ck",
			"cksys_u_1_sel", 1, 1),
	FACTOR(CKSYS_TOP_UART, "cksys_uart_ck",
			"cksys_uart_sel", 1, 1),
	FACTOR(CKSYS_TOP_I2C_PERI, "cksys_i2c_peri_ck",
			"cksys_i2c_peri_sel", 1, 1),
	FACTOR(CKSYS_TOP_I2C_NORTH, "cksys_i2c_north_ck",
			"cksys_i2c_north_sel", 1, 1),
	FACTOR(CKSYS_TOP_I2C_EAST, "cksys_i2c_east_ck",
			"cksys_i2c_east_sel", 1, 1),
	FACTOR(CKSYS_TOP_I2C_WEST, "cksys_i2c_west_ck",
			"cksys_i2c_west_sel", 1, 1),
	FACTOR(CKSYS_TOP_MSDC30_1, "cksys_msdc30_1_ck",
			"cksys_msdc30_1_sel", 1, 1),
	FACTOR(CKSYS_TOP_MSDC30_2, "cksys_msdc30_2_ck",
			"cksys_msdc30_2_sel", 1, 1),
	FACTOR(CKSYS_TOP_AOV_26M, "cksys_aov_26m_ck",
			"cksys_aov_26m_sel", 1, 1),
	FACTOR(CKSYS_TOP_P_AXI_PERI, "cksys_peri_axi_peri_ck",
			"cksys_peri_axi_ck", 1, 1),
	FACTOR(CKSYS_TOP_P_AXI_UFS0, "cksys_peri_axi_ufs0_ck",
			"cksys_peri_axi_ck", 1, 1),
	FACTOR(CKSYS_TOP_P_AXI_UFS1, "cksys_peri_axi_ufs1_ck",
			"cksys_peri_axi_ck", 1, 1),
	FACTOR(CKSYS_TOP_P_AXI_PEXTP0, "cksys_peri_axi_pextp0_ck",
			"cksys_peri_axi_ck", 1, 1),
	FACTOR(CKSYS_TOP_P_AXI_PEXTP1, "cksys_peri_axi_pextp1_ck",
			"cksys_peri_axi_ck", 1, 1),
	FACTOR(CKSYS_TOP_P_M_PEXTP0, "cksys_peri_m_pextp0_ck",
			"cksys_peri_fmem_sub_ck", 1, 1),
	FACTOR(CKSYS_TOP_MSDC30_1_SRC, "cksys_msdc30_1_src_ck",
			"cksys_msdc30_1_ck", 1, 1),
	FACTOR(CKSYS_TOP_MSDC30_1_HCLK, "cksys_msdc30_1_h_ck",
			"cksys_msdc30_1_ck", 1, 1),
	FACTOR(CKSYS_TOP_MSDC30_2_SRC, "cksys_msdc30_2_src_ck",
			"cksys_msdc30_2_ck", 1, 1),
	FACTOR(CKSYS_TOP_MSDC30_2_HCLK, "cksys_msdc30_2_h_ck",
			"cksys_msdc30_2_ck", 1, 1),
	FACTOR(CKSYS_TOP_IMG_AOV_26M, "cksys_img_aov_26m_ck",
			"cksys_aov_26m_ck", 1, 1),
	FACTOR(CKSYS_TOP_CAM_AOV_26M, "cksys_cam_aov_26m_ck",
			"cksys_aov_26m_ck", 1, 1),
};

static const struct mtk_fixed_factor cksys_vlp_divs[] = {
	FACTOR(CKSYS_VLP_CLKSQ, "cksys_vlp_clksq_ck",
			"clksq", 1, 1),
	FACTOR(CKSYS_VLP_F26M, "cksys_vlp_f26m_ck",
			"clk26m", 1, 1),
	FACTOR(CKSYS_VLP_F26M_CK_D2, "cksys_vlp_f26m_d2",
			"cksys_vlp_f26m_ck", 1, 2),
	FACTOR(CKSYS_VLP_OSC3, "cksys_vlp_osc3",
			"ulposc3", 1, 1),
	FACTOR(CKSYS_VLP_AUD_ENGEN1, "cksys_vlp_aud_engen1_ck",
			"vlp_aud_engen1_sel", 1, 1),
	FACTOR(CKSYS_VLP_AUD_ENGEN2, "cksys_vlp_aud_engen2_ck",
			"vlp_aud_engen2_sel", 1, 1),
	FACTOR(CKSYS_VLP_AUDIO_H, "cksys_vlp_audio_h_ck",
			"vlp_audio_h_sel", 1, 1),
	FACTOR(CKSYS_VLP_SPU0_VLP, "cksys_vlp_spu0_vlp_ck",
			"vlp_spu0_vlp_sel", 1, 1),
	FACTOR(CKSYS_VLP_DISP_PWM, "cksys_vlp_disp_pwm_ck",
			"vlp_disp_pwm_sel", 1, 1),
	FACTOR(CKSYS_VLP_AUD_VLPWIRE, "cksys_vlp_aud_vlpwire",
			"cksys_vlp_clksq_ck", 1, 1),
	FACTOR(CKSYS_VLP_USB_VLPWIRE, "cksys_vlp_usb_vlpwire",
			"cksys_vlp_clksq_ck", 1, 1),
	FACTOR(CKSYS_VLP_ULPOSC, "cksys_vlp_ulposc_ck",
			"cksys_osc", 1, 1),
};

static const struct mtk_fixed_factor cksys_mm_divs[] = {
	FACTOR(CKSYS_MM_MAINPLL_D3_D2, "mm_mainpll_d3_d2",
			"mainpll", 1, 6),
	FACTOR(CKSYS_MM_MAINPLL_D3_D3, "mm_mainpll_d3_d3",
			"mainpll", 1, 9),
	FACTOR(CKSYS_MM_MAINPLL2_D2, "mm_mainpll2_d2",
			"mainpll2", 1, 2),
	FACTOR(CKSYS_MM_MAINPLL2_D3, "mm_mainpll2_d3",
			"mainpll2", 1, 3),
	FACTOR(CKSYS_MM_MAINPLL2_D4, "mm_mainpll2_d4",
			"mainpll2", 1, 4),
	FACTOR(CKSYS_MM_MAINPLL2_D4_D2, "mm_mainpll2_d4_d2",
			"mainpll2", 1, 8),
	FACTOR(CKSYS_MM_MAINPLL2_D5, "mm_mainpll2_d5",
			"mainpll2", 1, 5),
	FACTOR(CKSYS_MM_MAINPLL2_D5_D2, "mm_mainpll2_d5_d2",
			"mainpll2", 1, 10),
	FACTOR(CKSYS_MM_MAINPLL2_D6, "mm_mainpll2_d6",
			"mainpll2", 1, 6),
	FACTOR(CKSYS_MM_MAINPLL2_D6_D2, "mm_mainpll2_d6_d2",
			"mainpll2", 1, 12),
	FACTOR(CKSYS_MM_MAINPLL2_D9, "mm_mainpll2_d9",
			"mainpll2", 1, 9),
	FACTOR(CKSYS_MM_UNIVPLL2_D2, "mm_univpll2_d2",
			"univpll2", 1, 2),
	FACTOR(CKSYS_MM_UNIVPLL2_D3, "mm_univpll2_d3",
			"univpll2", 1, 3),
	FACTOR(CKSYS_MM_UNIVPLL2_D4, "mm_univpll2_d4",
			"univpll2", 1, 4),
	FACTOR(CKSYS_MM_UNIVPLL2_D4_D2, "mm_univpll2_d4_d2",
			"univpll2", 1, 8),
	FACTOR(CKSYS_MM_UNIVPLL2_D5, "mm_univpll2_d5",
			"univpll2", 1, 5),
	FACTOR(CKSYS_MM_UNIVPLL2_D5_D2, "mm_univpll2_d5_d2",
			"univpll2", 1, 10),
	FACTOR(CKSYS_MM_UNIVPLL2_D6, "mm_univpll2_d6",
			"univpll2", 1, 6),
	FACTOR(CKSYS_MM_UNIVPLL2_D6_D2, "mm_univpll2_d6_d2",
			"univpll2", 1, 12),
	FACTOR(CKSYS_MM_UNIVPLL2_D6_D4, "mm_univpll2_d6_d4",
			"univpll2", 1, 24),
	FACTOR(CKSYS_MM_UNIVPLL2_D7, "mm_univpll2_d7",
			"univpll2", 1, 7),
	FACTOR(CKSYS_MM_MMPLL_D3, "mm_mmpll_d3",
			"mmpll", 1, 3),
	FACTOR(CKSYS_MM_MMPLL_D4, "mm_mmpll_d4",
			"mmpll", 1, 4),
	FACTOR(CKSYS_MM_MMPLL_D4_D2, "mm_mmpll_d4_d2",
			"mmpll", 1, 8),
	FACTOR(CKSYS_MM_MMPLL_D5, "mm_mmpll_d5",
			"mmpll", 1, 5),
	FACTOR(CKSYS_MM_MMPLL_D5_D2, "mm_mmpll_d5_d2",
			"mmpll", 1, 10),
	FACTOR(CKSYS_MM_MMPLL_D6, "mm_mmpll_d6",
			"mmpll", 1, 6),
	FACTOR(CKSYS_MM_MMPLL_D6_D2, "mm_mmpll_d6_d2",
			"mmpll", 1, 12),
	FACTOR(CKSYS_MM_MMPLL_D7, "mm_mmpll_d7",
			"mmpll", 1, 7),
	FACTOR(CKSYS_MM_IMGPLL_D2, "mm_imgpll_d2",
			"imgpll", 1, 2),
	FACTOR(CKSYS_MM_IMGPLL_D4, "mm_imgpll_d4",
			"imgpll", 1, 4),
	FACTOR(CKSYS_MM_IMGPLL_D5_D2, "mm_imgpll_d5_d2",
			"imgpll", 1, 10),
	FACTOR(CKSYS_MM_TVDPLL_D2, "mm_tvdpll_d2",
			"tvdpll", 1, 2),
	FACTOR(CKSYS_MM_TVDPLL_D4, "mm_tvdpll_d4",
			"tvdpll", 1, 4),
	FACTOR(CKSYS_MM_TVDPLL_D8, "mm_tvdpll_d8",
			"tvdpll", 1, 8),
	FACTOR(CKSYS_MM_TVDPLL_D3, "mm_tvdpll_d3",
			"tvdpll", 1, 3),
	FACTOR(CKSYS_MM_MMINFRA, "mm_mminfra_ck",
			"mm_mminfra_sel", 1, 1),
	FACTOR(CKSYS_MM_VENC, "mm_venc_ck",
			"mm_venc_sel", 1, 1),
	FACTOR(CKSYS_MM_VENC_MDP, "mm_venc_mdp_ck",
			"mm_venc_mdp_sel", 1, 1),
	FACTOR(CKSYS_MM_VDEC, "mm_vdec_ck",
			"mm_vdec_sel", 1, 1),
	FACTOR(CKSYS_MM_IMG1, "mm_img1_ck",
			"mm_img1_sel", 1, 1),
	FACTOR(CKSYS_MM_IPE, "mm_ipe_ck",
			"mm_ipe_sel", 1, 1),
	FACTOR(CKSYS_MM_DISP, "mm_disp_ck",
			"mm_disp_sel", 1, 1),
	FACTOR(CKSYS_MM_MML, "mm_mml_ck",
			"mm_mml_sel", 1, 1),
	FACTOR(CKSYS_MM_DVO_DP, "mm_dvo_dp_ck",
			"mm_dvo_dp_sel", 1, 1),
	FACTOR(CKSYS_MM_CAM, "mm_cam_ck",
			"mm_cam_sel", 1, 1),
	FACTOR(CKSYS_MM_CAMTM, "mm_camtm_ck",
			"mm_camtm_sel", 1, 1),
	FACTOR(CKSYS_MM_CCUSYS, "mm_ccusys_ck",
			"mm_ccusys_sel", 1, 1),
	FACTOR(CKSYS_MM_AVS_IMG, "mm_avs_img_ck",
			"cksys_vlp_f26m_ck", 1, 1),
};

static const char * const cksys_axi_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_osc_d4",
	"cksys_mainpll_d7_d2"
};

static const char * const cksys_peri_axi_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d8",
	"cksys_mainpll_d5_d4",
	"cksys_mainpll_d7_d2"
};

static const char * const cksys_ch_infra_axi_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_mainpll_d7_d2"
};

static const char * const cksys_ch_infra_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_mainpll_d7_d2",
	"cksys_mainpll_d5_d2",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d7",
	"cksys_mainpll_d5",
	"cksys_mainpll_d4",
	"cksys_univpll_d4",
	"cksys_mainpll_d3",
	"cksys_univpll_d3",
	"cksys_mainpll_d2",
	"cksys_univpll_d2",
	"cksys_emipll_d2"
};

static const char * const cksys_mem_sub_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_osc_d4",
	"cksys_mainpll_d5_d2",
	"cksys_mainpll_d6",
	"cksys_mainpll_d5",
	"cksys_mainpll_d4",
	"cksys_mainpll_d3"
};

static const char * const cksys_hash_sub_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_osc_d4",
	"cksys_mainpll_d5_d2",
	"cksys_mainpll_d6",
	"cksys_mainpll_d5",
	"cksys_mainpll_d4",
	"cksys_mainpll_d3"
};

static const char * const cksys_peri_fmem_sub_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d5_d4",
	"cksys_osc_d4",
	"cksys_mainpll_d5_d2",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d6",
	"cksys_mainpll_d5",
	"cksys_mainpll_d4"
};

static const char * const cksys_zram_sub_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_osc_d4",
	"cksys_mainpll_d5_d2",
	"cksys_mainpll_d6",
	"cksys_mainpll_d5",
	"cksys_mainpll_d4",
	"cksys_mainpll_d3"
};

static const char * const cksys_io_noc_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_osc_d4",
	"cksys_mainpll_d9"
};

static const char * const cksys_hash_noc_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_osc_d4",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d5",
	"cksys_mainpll_d4",
	"cksys_mainpll_d3",
	"cksys_mainpll_d2",
	"cksys_univpll_d2"
};

static const char * const cksys_peri_noc_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d4",
	"cksys_mainpll_d5_d2",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d6",
	"cksys_mainpll_d5",
	"cksys_mainpll_d4",
	"cksys_mainpll_d3"
};

static const char * const cksys_md_emi_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4"
};

static const char * const cksys_emi_n_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d4",
	"cksys_mainpll_d6_d2",
	"cksys_mainpll_d5_d2",
	"cksys_mainpll_d7",
	"cksys_mainpll_d5",
	"cksys_mainpll_d4",
	"cksys_mainpll_d3",
	"cksys_emipll_d3"
};

static const char * const cksys_emi_s_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d4",
	"cksys_mainpll_d6_d2",
	"cksys_mainpll_d5_d2",
	"cksys_mainpll_d7",
	"cksys_mainpll_d5",
	"cksys_mainpll_d4",
	"cksys_mainpll_d3",
	"cksys_emipll_d3"
};

static const char * const cksys_emi_infra_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d4",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d7",
	"cksys_mainpll_d5",
	"cksys_mainpll_d3",
	"cksys_mainpll_d2",
	"cksys_emipll_d2"
};

static const char * const cksys_emi_infra_sspm_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_mainpll_d6_d2",
	"cksys_mainpll_d5_d2",
	"cksys_osc_d2",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d6"
};

static const char * const cksys_osc_emi_ifr_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d2",
	"cksys_mainpll_d4_d2"
};

static const char * const cksys_emi_infra_26m_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20"
};

static const char * const cksys_infra_26m_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20"
};

static const char * const cksys_cbus_phy_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_mainpll_d6_d8",
	"cksys_mainpll_d5_d8",
	"cksys_mainpll_d7_d2",
	"cksys_mainpll_d5_d2"
};

static const char * const cksys_atb_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d5_d2",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d6",
	"cksys_mainpll_d4"
};

static const char * const cksys_cirq_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_mainpll_d7_d4"
};

static const char * const cksys_mcu_infra_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_mainpll_d5_d2",
	"cksys_mainpll_d9"
};

static const char * const cksys_apu_ext_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d5",
	"cksys_osc_d4",
	"cksys_osc_d3",
	"cksys_mainpll_d7_d2",
	"cksys_univpll_d5_d2"
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

static const char * const cksys_efuse_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20"
};

static const char * const cksys_dpsw_cmp_26m_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20"
};

static const char * const cksys_adsp_uarthub_b_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d4",
	"cksys_univpll_d6_d2"
};

static const char * const cksys_aud_1_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_apll1_ck"
};

static const char * const cksys_aud_2_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_apll2_ck"
};

static const char * const cksys_dpmaif_main_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d4_d4",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d6",
	"cksys_mainpll_d5",
	"cksys_univpll_d5",
	"cksys_mainpll_d4"
};

static const char * const cksys_ipseast_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d5",
	"cksys_osc_d2",
	"cksys_mainpll_d4",
	"cksys_mainpll_d3",
	"cksys_mainpll_d2"
};

static const char * const cksys_ipswest_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d5",
	"cksys_osc_d2",
	"cksys_mainpll_d4",
	"cksys_mainpll_d3",
	"cksys_mainpll_d2"
};

static const char * const cksys_smapck_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d8"
};

static const char * const cksys_ipic_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d7_d2",
	"cksys_osc_d3"
};

static const char * const cksys_spi0_b_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d4",
	"cksys_univpll_d5_d4",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d7_d2",
	"cksys_mainpll_d6_d2",
	"cksys_univpll_192m",
	"cksys_univpll_d6_d2"
};

static const char * const cksys_spi1_b_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d4",
	"cksys_univpll_d5_d4",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d7_d2",
	"cksys_mainpll_d6_d2",
	"cksys_univpll_192m",
	"cksys_univpll_d6_d2"
};

static const char * const cksys_spi2_b_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d4",
	"cksys_univpll_d5_d4",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d7_d2",
	"cksys_mainpll_d6_d2",
	"cksys_univpll_192m",
	"cksys_univpll_d6_d2"
};

static const char * const cksys_spi3_b_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d4",
	"cksys_univpll_d5_d4",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d7_d2",
	"cksys_mainpll_d6_d2",
	"cksys_univpll_192m",
	"cksys_univpll_d6_d2"
};

static const char * const cksys_spi4_b_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d4",
	"cksys_univpll_d5_d4",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d7_d2",
	"cksys_mainpll_d6_d2",
	"cksys_univpll_192m",
	"cksys_univpll_d6_d2"
};

static const char * const cksys_spi5_b_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d4",
	"cksys_univpll_d5_d4",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d7_d2",
	"cksys_mainpll_d6_d2",
	"cksys_univpll_192m",
	"cksys_univpll_d6_d2"
};

static const char * const cksys_spi6_b_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d4",
	"cksys_univpll_d5_d4",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d7_d2",
	"cksys_mainpll_d6_d2",
	"cksys_univpll_192m",
	"cksys_univpll_d6_d2"
};

static const char * const cksys_spi7_b_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d4",
	"cksys_univpll_d5_d4",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d7_d2",
	"cksys_mainpll_d6_d2",
	"cksys_univpll_192m",
	"cksys_univpll_d6_d2"
};

static const char * const cksys_tl_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d7_d4",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d5_d2"
};

static const char * const cksys_tl_p1_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d5_d2"
};

static const char * const cksys_pwm_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d7_d4",
	"cksys_univpll_d4_d8"
};

static const char * const cksys_aes_ufsfde_0_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d2",
	"cksys_univpll_d6",
	"cksys_mainpll_d4"
};

static const char * const cksys_u_0_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d5",
	"cksys_univpll_d5"
};

static const char * const cksys_aes_ufsfde_1_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d2",
	"cksys_univpll_d6",
	"cksys_mainpll_d4"
};

static const char * const cksys_u_1_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d5",
	"cksys_univpll_d5"
};

static const char * const cksys_uarthub_b_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d4",
	"cksys_univpll_d6_d2"
};

static const char * const cksys_uart_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d6_d8",
	"cksys_univpll_d6_d4",
	"cksys_univpll_d6_d2"
};

static const char * const cksys_i2c_peri_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d5_d4",
	"cksys_mainpll_d4_d4",
	"cksys_univpll_d5_d2"
};

static const char * const cksys_i2c_north_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d5_d4",
	"cksys_mainpll_d4_d4",
	"cksys_univpll_d5_d2"
};

static const char * const cksys_i2c_east_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d5_d4",
	"cksys_mainpll_d4_d4",
	"cksys_univpll_d5_d2"
};

static const char * const cksys_i2c_west_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_d5_d4",
	"cksys_mainpll_d4_d4",
	"cksys_univpll_d5_d2"
};

static const char * const cksys_msdc_macro_1p_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d4",
	"cksys_msdcpll_d2",
	"cksys_msdcpll_ck"
};

static const char * const cksys_msdc_macro_2p_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d4",
	"cksys_msdcpll_d2",
	"cksys_msdcpll_ck"
};

static const char * const cksys_msdc30_1_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d6_d2",
	"cksys_msdcpll_d2"
};

static const char * const cksys_msdc30_2_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d6_d2",
	"cksys_msdcpll_d2"
};

static const char * const cksys_gridsensor_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d5_d4"
};

static const char * const cksys_aov_26m_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20"
};

static const char * const cksys_emi_wdat_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d4",
	"cksys_mainpll_d4_d2",
	"cksys_univpll_d6",
	"cksys_mainpll_d5",
	"cksys_mainpll_d4",
	"cksys_univpll_d3",
	"cksys_mainpll_d2",
	"cksys_univpll_d2",
	"cksys_mmpll_d2"
};

static const char * const cksys_apll_i2sin0_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_i2sin1_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_i2sin2_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_i2sin3_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_i2sin4_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_i2sin6_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_i2sout0_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_i2sout1_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_i2sout2_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_i2sout3_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_i2sout4_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_i2sout6_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_fmi2s_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const char * const cksys_apll_tdmout_m_parents[] = {
	"cksys_aud_1_sel",
	"cksys_aud_2_sel"
};

static const struct mtk_mux cksys_top_muxes[] = {
#if MT_CCF_MUX_DISABLE
	/* CLK_CFG_0 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_AXI_SEL/* dts */, "cksys_axi_sel",
		cksys_axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 31/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 31/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_P_AXI_SEL/* dts */, "cksys_peri_axi_sel",
		cksys_peri_axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 30/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 30/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_CH_INFRA_AXI_SEL/* dts */, "cksys_ch_infra_axi_sel",
		cksys_ch_infra_axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CH_INFRA_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 29/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 29/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_CH_INFRA_SEL/* dts */, "cksys_ch_infra_sel",
		cksys_ch_infra_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CH_INFRA_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 28/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 28/* fenc shift */),
	/* CLK_CFG_1 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_MEM_SUB_SEL/* dts */, "cksys_mem_sub_sel",
		cksys_mem_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 27/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 27/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_HASH_SUB_SEL/* dts */, "cksys_hash_sub_sel",
		cksys_hash_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_HASH_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 26/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 26/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_P_FMEM_SUB_SEL/* dts */, "cksys_peri_fmem_sub_sel",
		cksys_peri_fmem_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_FMEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 25/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 25/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_ZRAM_SUB_SEL/* dts */, "cksys_zram_sub_sel",
		cksys_zram_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ZRAM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 24/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 24/* fenc shift */),
	/* CLK_CFG_2 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_IO_NOC_SEL/* dts */, "cksys_io_noc_sel",
		cksys_io_noc_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_IO_NOC_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 23/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 23/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_HASH_NOC_SEL/* dts */, "cksys_hash_noc_sel",
		cksys_hash_noc_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_HASH_NOC_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 22/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 22/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_P_NOC_SEL/* dts */, "cksys_peri_noc_sel",
		cksys_peri_noc_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_NOC_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 21/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 21/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_EMI_INTERFACE_546_SEL/* dts */, "cksys_md_emi_sel",
		cksys_md_emi_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_INTERFACE_546_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 20/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 20/* fenc shift */),
	/* CLK_CFG_3 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_EMI_N_SEL/* dts */, "cksys_emi_n_sel",
		cksys_emi_n_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_N_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 19/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 19/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_EMI_S_SEL/* dts */, "cksys_emi_s_sel",
		cksys_emi_s_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_S_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 18/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 18/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_EMI_INFRA_SEL/* dts */, "cksys_emi_infra_sel",
		cksys_emi_infra_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_INFRA_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 17/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 17/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_EMI_INFRA_SSPM_SEL/* dts */, "cksys_emi_infra_sspm_sel",
		cksys_emi_infra_sspm_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_INFRA_SSPM_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 16/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 16/* fenc shift */),
	/* CLK_CFG_4 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_OSC_EMI_IFR_SEL/* dts */, "cksys_osc_emi_ifr_sel",
		cksys_osc_emi_ifr_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ULPOSC_EMI_INFRA_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 15/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 15/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_EMI_INFRA_26M_SEL/* dts */, "cksys_emi_infra_26m_sel",
		cksys_emi_infra_26m_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_INFRA_26M_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 14/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 14/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_INFRA_26M_SEL/* dts */, "cksys_infra_26m_sel",
		cksys_infra_26m_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CH_INFRA_SYS_26M_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 13/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 13/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_CBUS_PHY_SEL/* dts */, "cksys_cbus_phy_sel",
		cksys_cbus_phy_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CBUS_PHY_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 12/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 12/* fenc shift */),
	/* CLK_CFG_5 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_ATB_SEL/* dts */, "cksys_atb_sel",
		cksys_atb_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ATB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 11/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 11/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_CIRQ_SEL/* dts */, "cksys_cirq_sel",
		cksys_cirq_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CIRQ_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 10/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 10/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_MCU_INFRA_SEL/* dts */, "cksys_mcu_infra_sel",
		cksys_mcu_infra_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MCU_INFRA_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 9/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 9/* fenc shift */),
	/* CLK_CFG_6 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_APU_EXT_SEL/* dts */, "cksys_apu_ext_sel",
		cksys_apu_ext_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_APU_EXT_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 5/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 5/* fenc shift */),
	/* CLK_CFG_7 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_SSR_PKA_SEL/* dts */, "cksys_ssr_pka_sel",
		cksys_ssr_pka_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSR_PKA_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 3/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 3/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_SSR_DMA_SEL/* dts */, "cksys_ssr_dma_sel",
		cksys_ssr_dma_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSR_DMA_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 2/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 2/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_SSR_KDF_SEL/* dts */, "cksys_ssr_kdf_sel",
		cksys_ssr_kdf_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSR_KDF_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 1/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 1/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_SSR_RNG_SEL/* dts */, "cksys_ssr_rng_sel",
		cksys_ssr_rng_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SSR_RNG_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 31/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 0/* fenc shift */),
	/* CLK_CFG_8 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_EFUSE_SEL/* dts */, "cksys_efuse_sel",
		cksys_efuse_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_EFUSE_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 29/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 30/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_DPSW_CMP_26M_SEL/* dts */, "cksys_dpsw_cmp_26m_sel",
		cksys_dpsw_cmp_26m_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DPSW_CMP_26M_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 27/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 28/* fenc shift */),
	/* CLK_CFG_9 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_ADSP_UARTHUB_BCLK_SEL/* dts */, "cksys_adsp_uarthub_b_sel",
		cksys_adsp_uarthub_b_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_ADSP_UARTHUB_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 26/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 27/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_AUD_1_SEL/* dts */, "cksys_aud_1_sel",
		cksys_aud_1_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_1_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 25/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 26/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_AUD_2_SEL/* dts */, "cksys_aud_2_sel",
		cksys_aud_2_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_2_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 24/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 25/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_DPMAIF_MAIN_SEL/* dts */, "cksys_dpmaif_main_sel",
		cksys_dpmaif_main_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DPMAIF_MAIN_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 23/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 24/* fenc shift */),
	/* CLK_CFG_10 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_IPSEAST_SEL/* dts */, "cksys_ipseast_sel",
		cksys_ipseast_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_IPSEAST_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 22/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 23/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_IPSWEST_SEL/* dts */, "cksys_ipswest_sel",
		cksys_ipswest_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_IPSWEST_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 21/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 22/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_SMAPCK_SEL/* dts */, "cksys_smapck_sel",
		cksys_smapck_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SMAPCK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 20/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 21/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_IPIC_SEL/* dts */, "cksys_ipic_sel",
		cksys_ipic_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_IPIC_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 19/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 20/* fenc shift */),
	/* CLK_CFG_11 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_SPI0_BCLK_SEL/* dts */, "cksys_spi0_b_sel",
		cksys_spi0_b_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPI0_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 18/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 19/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_SPI1_BCLK_SEL/* dts */, "cksys_spi1_b_sel",
		cksys_spi1_b_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPI1_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 17/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 18/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_SPI2_BCLK_SEL/* dts */, "cksys_spi2_b_sel",
		cksys_spi2_b_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPI2_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 16/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 17/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_SPI3_BCLK_SEL/* dts */, "cksys_spi3_b_sel",
		cksys_spi3_b_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPI3_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 15/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 16/* fenc shift */),
	/* CLK_CFG_12 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_SPI4_BCLK_SEL/* dts */, "cksys_spi4_b_sel",
		cksys_spi4_b_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPI4_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 14/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 15/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_SPI5_BCLK_SEL/* dts */, "cksys_spi5_b_sel",
		cksys_spi5_b_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPI5_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 13/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 14/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_SPI6_BCLK_SEL/* dts */, "cksys_spi6_b_sel",
		cksys_spi6_b_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPI6_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 12/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 13/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_SPI7_BCLK_SEL/* dts */, "cksys_spi7_b_sel",
		cksys_spi7_b_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPI7_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 11/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 12/* fenc shift */),
	/* CLK_CFG_13 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_TL_SEL/* dts */, "cksys_tl_sel",
		cksys_tl_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_TL_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 9/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 10/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_TL_P1_SEL/* dts */, "cksys_tl_p1_sel",
		cksys_tl_p1_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_TL_P1_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 8/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 9/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_PWM_SEL/* dts */, "cksys_pwm_sel",
		cksys_pwm_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_PWM_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 7/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 8/* fenc shift */),
	/* CLK_CFG_14 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_AES_UFSFDE_0_SEL/* dts */, "cksys_aes_ufsfde_0_sel",
		cksys_aes_ufsfde_0_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AES_UFSFDE_0_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 6/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 7/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_U_0_SEL/* dts */, "cksys_u_0_sel",
		cksys_u_0_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_UFS_0_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 5/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 6/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_AES_UFSFDE_1_SEL/* dts */, "cksys_aes_ufsfde_1_sel",
		cksys_aes_ufsfde_1_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AES_UFSFDE_1_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 3/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 4/* fenc shift */),
	/* CLK_CFG_15 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_U_1_SEL/* dts */, "cksys_u_1_sel",
		cksys_u_1_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_UFS_1_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 2/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 3/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_UARTHUB_BCLK_SEL/* dts */, "cksys_uarthub_b_sel",
		cksys_uarthub_b_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_UARTHUB_BCLK_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 31/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 1/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_UART_SEL/* dts */, "cksys_uart_sel",
		cksys_uart_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_UART_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 30/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 0/* fenc shift */),
	/* CLK_CFG_16 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_I2C_PERI_SEL/* dts */, "cksys_i2c_peri_sel",
		cksys_i2c_peri_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_I2C_PERI_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 29/* cksta shift */,
		CLK_FENC_STATUS_MON_2/* fenc ofs */, 31/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_I2C_NORTH_SEL/* dts */, "cksys_i2c_north_sel",
		cksys_i2c_north_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_I2C_NORTH_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 28/* cksta shift */,
		CLK_FENC_STATUS_MON_2/* fenc ofs */, 30/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_I2C_EAST_SEL/* dts */, "cksys_i2c_east_sel",
		cksys_i2c_east_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_I2C_EAST_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 27/* cksta shift */,
		CLK_FENC_STATUS_MON_2/* fenc ofs */, 29/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_I2C_WEST_SEL/* dts */, "cksys_i2c_west_sel",
		cksys_i2c_west_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_I2C_WEST_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 26/* cksta shift */,
		CLK_FENC_STATUS_MON_2/* fenc ofs */, 28/* fenc shift */),
	/* CLK_CFG_17 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_MSDC_MACRO_1P_SEL/* dts */, "cksys_msdc_macro_1p_sel",
		cksys_msdc_macro_1p_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_MSDC_MACRO_1P_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 25/* cksta shift */,
		CLK_FENC_STATUS_MON_2/* fenc ofs */, 27/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_MSDC_MACRO_2P_SEL/* dts */, "cksys_msdc_macro_2p_sel",
		cksys_msdc_macro_2p_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_MSDC_MACRO_2P_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 24/* cksta shift */,
		CLK_FENC_STATUS_MON_2/* fenc ofs */, 26/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_MSDC30_1_SEL/* dts */, "cksys_msdc30_1_sel",
		cksys_msdc30_1_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_MSDC30_1_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 23/* cksta shift */,
		CLK_FENC_STATUS_MON_2/* fenc ofs */, 25/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_MSDC30_2_SEL/* dts */, "cksys_msdc30_2_sel",
		cksys_msdc30_2_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_MSDC30_2_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 22/* cksta shift */,
		CLK_FENC_STATUS_MON_2/* fenc ofs */, 24/* fenc shift */),
	/* CLK_CFG_20 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_GRIDSENSOR_SEL/* dts */, "cksys_gridsensor_sel",
		cksys_gridsensor_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_GRIDSENSOR_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 12/* cksta shift */,
		CLK_FENC_STATUS_MON_2/* fenc ofs */, 14/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_AOV_26M_SEL/* dts */, "cksys_aov_26m_sel",
		cksys_aov_26m_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_AOV_26M_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 11/* cksta shift */,
		CLK_FENC_STATUS_MON_2/* fenc ofs */, 13/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_EMI_WDAT_SEL/* dts */, "cksys_emi_wdat_sel",
		cksys_emi_wdat_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_EMI_WDAT_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 10/* cksta shift */,
		CLK_FENC_STATUS_MON_2/* fenc ofs */, 12/* fenc shift */),
	/* CLK_AUDDIV_0 */
#else
	/* CLK_CFG_0 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_AXI_SEL/* dts */, "cksys_axi_sel",
		cksys_axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 31/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 31/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_P_AXI_SEL/* dts */, "cksys_peri_axi_sel",
		cksys_peri_axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 30/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 30/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_CH_INFRA_AXI_SEL/* dts */, "cksys_ch_infra_axi_sel",
		cksys_ch_infra_axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CH_INFRA_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 29/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 29/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_CH_INFRA_SEL/* dts */, "cksys_ch_infra_sel",
		cksys_ch_infra_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CH_INFRA_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 28/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 28/* fenc shift */),
	/* CLK_CFG_1 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_MEM_SUB_SEL/* dts */, "cksys_mem_sub_sel",
		cksys_mem_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 27/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 27/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_HASH_SUB_SEL/* dts */, "cksys_hash_sub_sel",
		cksys_hash_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_HASH_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 26/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 26/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_P_FMEM_SUB_SEL/* dts */, "cksys_peri_fmem_sub_sel",
		cksys_peri_fmem_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_FMEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 25/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 25/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_ZRAM_SUB_SEL/* dts */, "cksys_zram_sub_sel",
		cksys_zram_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_ZRAM_SUB_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		24/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		24/* fenc shift */),
	/* CLK_CFG_2 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_IO_NOC_SEL/* dts */, "cksys_io_noc_sel",
		cksys_io_noc_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_IO_NOC_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 23/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 23/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_HASH_NOC_SEL/* dts */, "cksys_hash_noc_sel",
		cksys_hash_noc_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_HASH_NOC_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 22/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 22/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_P_NOC_SEL/* dts */, "cksys_peri_noc_sel",
		cksys_peri_noc_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_NOC_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 21/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 21/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_EMI_INTERFACE_546_SEL/* dts */, "cksys_md_emi_sel",
		cksys_md_emi_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_INTERFACE_546_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 20/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 20/* fenc shift */),
	/* CLK_CFG_3 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_EMI_N_SEL/* dts */, "cksys_emi_n_sel",
		cksys_emi_n_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_N_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 19/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 19/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_EMI_S_SEL/* dts */, "cksys_emi_s_sel",
		cksys_emi_s_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_S_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 18/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 18/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_EMI_INFRA_SEL/* dts */, "cksys_emi_infra_sel",
		cksys_emi_infra_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_INFRA_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 17/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 17/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_EMI_INFRA_SSPM_SEL/* dts */, "cksys_emi_infra_sspm_sel",
		cksys_emi_infra_sspm_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_INFRA_SSPM_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 16/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 16/* fenc shift */),
	/* CLK_CFG_4 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_OSC_EMI_IFR_SEL/* dts */, "cksys_osc_emi_ifr_sel",
		cksys_osc_emi_ifr_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ULPOSC_EMI_INFRA_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 15/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 15/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_EMI_INFRA_26M_SEL/* dts */, "cksys_emi_infra_26m_sel",
		cksys_emi_infra_26m_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_INFRA_26M_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 14/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 14/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_INFRA_26M_SEL/* dts */, "cksys_infra_26m_sel",
		cksys_infra_26m_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CH_INFRA_SYS_26M_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 13/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 13/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_CBUS_PHY_SEL/* dts */, "cksys_cbus_phy_sel",
		cksys_cbus_phy_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CBUS_PHY_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 12/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 12/* fenc shift */),
	/* CLK_CFG_5 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_ATB_SEL/* dts */, "cksys_atb_sel",
		cksys_atb_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ATB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 11/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 11/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_CIRQ_SEL/* dts */, "cksys_cirq_sel",
		cksys_cirq_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CIRQ_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 10/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 10/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_MCU_INFRA_SEL/* dts */, "cksys_mcu_infra_sel",
		cksys_mcu_infra_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MCU_INFRA_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 9/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 9/* fenc shift */),
	/* CLK_CFG_6 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_APU_EXT_SEL/* dts */, "cksys_apu_ext_sel",
		cksys_apu_ext_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_APU_EXT_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 5/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 5/* fenc shift */),
	/* CLK_CFG_7 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_SSR_PKA_SEL/* dts */, "cksys_ssr_pka_sel",
		cksys_ssr_pka_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSR_PKA_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 3/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 3/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_SSR_DMA_SEL/* dts */, "cksys_ssr_dma_sel",
		cksys_ssr_dma_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSR_DMA_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 2/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 2/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_SSR_KDF_SEL/* dts */, "cksys_ssr_kdf_sel",
		cksys_ssr_kdf_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSR_KDF_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 1/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 1/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_SSR_RNG_SEL/* dts */, "cksys_ssr_rng_sel",
		cksys_ssr_rng_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SSR_RNG_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 31/* cksta shift */,
		CLK_FENC_STATUS_MON_0/* fenc ofs */, 0/* fenc shift */),
	/* CLK_CFG_8 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_EFUSE_SEL/* dts */, "cksys_efuse_sel",
		cksys_efuse_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_EFUSE_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 29/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 30/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_DPSW_CMP_26M_SEL/* dts */, "cksys_dpsw_cmp_26m_sel",
		cksys_dpsw_cmp_26m_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DPSW_CMP_26M_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 27/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 28/* fenc shift */),
	/* CLK_CFG_9 */
	MUX_GENERIC_HWV(CKSYS_TOP_ADSP_UARTHUB_BCLK_SEL/* dts */, "cksys_adsp_uarthub_b_sel",
		cksys_adsp_uarthub_b_parents/* parent */, CLK_CFG_9,
		CLK_CFG_9_SET, CLK_CFG_9_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, AP_HWV_HWV_CG_GRP_0_DONE,
		AP_HWV_HWV_CG_GRP_0_SET, AP_HWV_HWV_CG_GRP_0_CLR, /* hwv */
		0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_ADSP_UARTHUB_BCLK_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		26/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		27/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_AUD_1_SEL/* dts */, "cksys_aud_1_sel",
		cksys_aud_1_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_1_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		25/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		26/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_AUD_2_SEL/* dts */, "cksys_aud_2_sel",
		cksys_aud_2_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 16/* lsb */, 1/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_2_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		24/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		25/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_DPMAIF_MAIN_SEL/* dts */, "cksys_dpmaif_main_sel",
		cksys_dpmaif_main_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_DPMAIF_MAIN_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		23/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		24/* fenc shift */),
	/* CLK_CFG_10 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_IPSEAST_SEL/* dts */, "cksys_ipseast_sel",
		cksys_ipseast_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_IPSEAST_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		22/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		23/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_IPSWEST_SEL/* dts */, "cksys_ipswest_sel",
		cksys_ipswest_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_IPSWEST_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		21/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		22/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_SMAPCK_SEL/* dts */, "cksys_smapck_sel",
		cksys_smapck_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SMAPCK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 20/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 21/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_IPIC_SEL/* dts */, "cksys_ipic_sel",
		cksys_ipic_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_IPIC_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 19/* cksta shift */,
		CLK_FENC_STATUS_MON_1/* fenc ofs */, 20/* fenc shift */),
	/* CLK_CFG_11 */
	MUX_GENERIC_HWV(CKSYS_TOP_SPI0_BCLK_SEL/* dts */, "cksys_spi0_b_sel",
		cksys_spi0_b_parents/* parent */, CLK_CFG_11,
		CLK_CFG_11_SET, CLK_CFG_11_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, AP_HWV_HWV_CG_GRP_1_DONE,
		AP_HWV_HWV_CG_GRP_1_SET, AP_HWV_HWV_CG_GRP_1_CLR, /* hwv */
		0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SPI0_BCLK_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		18/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		19/* fenc shift */),
	MUX_GENERIC_HWV(CKSYS_TOP_SPI1_BCLK_SEL/* dts */, "cksys_spi1_b_sel",
		cksys_spi1_b_parents/* parent */, CLK_CFG_11,
		CLK_CFG_11_SET, CLK_CFG_11_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, AP_HWV_HWV_CG_GRP_1_DONE,
		AP_HWV_HWV_CG_GRP_1_SET, AP_HWV_HWV_CG_GRP_1_CLR, /* hwv */
		8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SPI1_BCLK_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		17/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		18/* fenc shift */),
	MUX_GENERIC_HWV(CKSYS_TOP_SPI2_BCLK_SEL/* dts */, "cksys_spi2_b_sel",
		cksys_spi2_b_parents/* parent */, CLK_CFG_11,
		CLK_CFG_11_SET, CLK_CFG_11_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, AP_HWV_HWV_CG_GRP_1_DONE,
		AP_HWV_HWV_CG_GRP_1_SET, AP_HWV_HWV_CG_GRP_1_CLR, /* hwv */
		16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SPI2_BCLK_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		16/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		17/* fenc shift */),
	MUX_GENERIC_HWV(CKSYS_TOP_SPI3_BCLK_SEL/* dts */, "cksys_spi3_b_sel",
		cksys_spi3_b_parents/* parent */, CLK_CFG_11,
		CLK_CFG_11_SET, CLK_CFG_11_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, AP_HWV_HWV_CG_GRP_1_DONE,
		AP_HWV_HWV_CG_GRP_1_SET, AP_HWV_HWV_CG_GRP_1_CLR, /* hwv */
		24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SPI3_BCLK_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		15/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		16/* fenc shift */),
	/* CLK_CFG_12 */
	MUX_GENERIC_HWV(CKSYS_TOP_SPI4_BCLK_SEL/* dts */, "cksys_spi4_b_sel",
		cksys_spi4_b_parents/* parent */, CLK_CFG_12,
		CLK_CFG_12_SET, CLK_CFG_12_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, AP_HWV_HWV_CG_GRP_2_DONE,
		AP_HWV_HWV_CG_GRP_2_SET, AP_HWV_HWV_CG_GRP_2_CLR, /* hwv */
		0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SPI4_BCLK_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		14/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		15/* fenc shift */),
	MUX_GENERIC_HWV(CKSYS_TOP_SPI5_BCLK_SEL/* dts */, "cksys_spi5_b_sel",
		cksys_spi5_b_parents/* parent */, CLK_CFG_12,
		CLK_CFG_12_SET, CLK_CFG_12_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, AP_HWV_HWV_CG_GRP_2_DONE,
		AP_HWV_HWV_CG_GRP_2_SET, AP_HWV_HWV_CG_GRP_2_CLR, /* hwv */
		8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SPI5_BCLK_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		13/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		14/* fenc shift */),
	MUX_GENERIC_HWV(CKSYS_TOP_SPI6_BCLK_SEL/* dts */, "cksys_spi6_b_sel",
		cksys_spi6_b_parents/* parent */, CLK_CFG_12,
		CLK_CFG_12_SET, CLK_CFG_12_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, AP_HWV_HWV_CG_GRP_2_DONE,
		AP_HWV_HWV_CG_GRP_2_SET, AP_HWV_HWV_CG_GRP_2_CLR, /* hwv */
		16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SPI6_BCLK_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		12/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		13/* fenc shift */),
	MUX_GENERIC_HWV(CKSYS_TOP_SPI7_BCLK_SEL/* dts */, "cksys_spi7_b_sel",
		cksys_spi7_b_parents/* parent */, CLK_CFG_12,
		CLK_CFG_12_SET, CLK_CFG_12_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, AP_HWV_HWV_CG_GRP_2_DONE,
		AP_HWV_HWV_CG_GRP_2_SET, AP_HWV_HWV_CG_GRP_2_CLR, /* hwv */
		24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SPI7_BCLK_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		11/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		12/* fenc shift */),
	/* CLK_CFG_13 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_TL_SEL/* dts */, "cksys_tl_sel",
		cksys_tl_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_TL_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		9/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		10/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_TL_P1_SEL/* dts */, "cksys_tl_p1_sel",
		cksys_tl_p1_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_TL_P1_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		8/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		9/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_PWM_SEL/* dts */, "cksys_pwm_sel",
		cksys_pwm_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_PWM_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		7/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		8/* fenc shift */),
	/* CLK_CFG_14 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_AES_UFSFDE_0_SEL/* dts */, "cksys_aes_ufsfde_0_sel",
		cksys_aes_ufsfde_0_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AES_UFSFDE_0_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		6/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		7/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_U_0_SEL/* dts */, "cksys_u_0_sel",
		cksys_u_0_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_UFS_0_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		5/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		6/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_AES_UFSFDE_1_SEL/* dts */, "cksys_aes_ufsfde_1_sel",
		cksys_aes_ufsfde_1_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AES_UFSFDE_1_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		3/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		4/* fenc shift */),
	/* CLK_CFG_15 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_U_1_SEL/* dts */, "cksys_u_1_sel",
		cksys_u_1_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_UFS_1_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		2/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		3/* fenc shift */),
	MUX_GENERIC_HWV(CKSYS_TOP_UARTHUB_BCLK_SEL/* dts */, "cksys_uarthub_b_sel",
		cksys_uarthub_b_parents/* parent */, CLK_CFG_15,
		CLK_CFG_15_SET, CLK_CFG_15_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, AP_HWV_HWV_CG_GRP_3_DONE,
		AP_HWV_HWV_CG_GRP_3_SET, AP_HWV_HWV_CG_GRP_3_CLR, /* hwv */
		16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_UARTHUB_BCLK_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		31/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		1/* fenc shift */),
	MUX_GENERIC_HWV(CKSYS_TOP_UART_SEL/* dts */, "cksys_uart_sel",
		cksys_uart_parents/* parent */, CLK_CFG_15,
		CLK_CFG_15_SET, CLK_CFG_15_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, AP_HWV_HWV_CG_GRP_3_DONE,
		AP_HWV_HWV_CG_GRP_3_SET, AP_HWV_HWV_CG_GRP_3_CLR, /* hwv */
		24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_UART_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		30/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		0/* fenc shift */),
	/* CLK_CFG_16 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_I2C_PERI_SEL/* dts */, "cksys_i2c_peri_sel",
		cksys_i2c_peri_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_I2C_PERI_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		29/* cksta shift */, CLK_FENC_STATUS_MON_2/* fenc ofs */,
		31/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_I2C_NORTH_SEL/* dts */, "cksys_i2c_north_sel",
		cksys_i2c_north_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_I2C_NORTH_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		28/* cksta shift */, CLK_FENC_STATUS_MON_2/* fenc ofs */,
		30/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_I2C_EAST_SEL/* dts */, "cksys_i2c_east_sel",
		cksys_i2c_east_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_I2C_EAST_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		27/* cksta shift */, CLK_FENC_STATUS_MON_2/* fenc ofs */,
		29/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_I2C_WEST_SEL/* dts */, "cksys_i2c_west_sel",
		cksys_i2c_west_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_I2C_WEST_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		26/* cksta shift */, CLK_FENC_STATUS_MON_2/* fenc ofs */,
		28/* fenc shift */),
	/* CLK_CFG_17 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_MSDC_MACRO_1P_SEL/* dts */, "cksys_msdc_macro_1p_sel",
		cksys_msdc_macro_1p_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_MSDC_MACRO_1P_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		25/* cksta shift */, CLK_FENC_STATUS_MON_2/* fenc ofs */,
		27/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_MSDC_MACRO_2P_SEL/* dts */, "cksys_msdc_macro_2p_sel",
		cksys_msdc_macro_2p_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_MSDC_MACRO_2P_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		24/* cksta shift */, CLK_FENC_STATUS_MON_2/* fenc ofs */,
		26/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_MSDC30_1_SEL/* dts */, "cksys_msdc30_1_sel",
		cksys_msdc30_1_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_MSDC30_1_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		23/* cksta shift */, CLK_FENC_STATUS_MON_2/* fenc ofs */,
		25/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_MSDC30_2_SEL/* dts */, "cksys_msdc30_2_sel",
		cksys_msdc30_2_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_MSDC30_2_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		22/* cksta shift */, CLK_FENC_STATUS_MON_2/* fenc ofs */,
		24/* fenc shift */),
	/* CLK_CFG_20 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_GRIDSENSOR_SEL/* dts */, "cksys_gridsensor_sel",
		cksys_gridsensor_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_GRIDSENSOR_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 12/* cksta shift */,
		CLK_FENC_STATUS_MON_2/* fenc ofs */, 14/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_AOV_26M_SEL/* dts */, "cksys_aov_26m_sel",
		cksys_aov_26m_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_AOV_26M_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 11/* cksta shift */,
		CLK_FENC_STATUS_MON_2/* fenc ofs */, 13/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_TOP_EMI_WDAT_SEL/* dts */, "cksys_emi_wdat_sel",
		cksys_emi_wdat_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_EMI_WDAT_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 10/* cksta shift */,
		CLK_FENC_STATUS_MON_2/* fenc ofs */, 12/* fenc shift */),
	/* CLK_AUDDIV_0 */
#endif
};

static const char * const vlp_sspm_26m_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20"
};

static const char * const vlp_ulposc_sspm_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d2",
	"cksys_mainpll_d4_d2"
};

static const char * const vlp_sspm_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_mainpll_d5_d2",
	"cksys_osc_d2",
	"cksys_mainpll_d6"
};

static const char * const vlp_spm_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d8",
	"cksys_mainpll_d7_d4",
	"cksys_osc_d4"
};

static const char * const vlp_axi_vlp_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_mainpll_d7_d4",
	"cksys_osc_d4",
	"cksys_mainpll_d7_d2"
};

static const char * const vlp_noc_vlp_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_osc_d4",
	"cksys_mainpll_d9"
};

static const char * const vlp_pwm_vlp_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_clkrtc",
	"cksys_osc_d20",
	"cksys_osc_d8",
	"cksys_mainpll_d4_d8"
};

static const char * const vlp_systimer_26m_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20"
};

static const char * const vlp_dpsw_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d10",
	"cksys_osc_d7",
	"cksys_mainpll_d7_d4"
};

static const char * const vlp_dpsw_central_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d10",
	"cksys_osc_d7",
	"cksys_mainpll_d7_d4"
};

static const char * const vlp_srck_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20"
};

static const char * const vlp_dvfsrc_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20"
};

static const char * const vlp_dbg_err_vlp_26m_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20"
};

static const char * const vlp_ips_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d4"
};

static const char * const vlp_dpmsrdma_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d7_d2"
};

static const char * const vlp_vlp_pbus_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_osc_d2",
	"cksys_mainpll_d7"
};

static const char * const vlp_vlp_pbus_26m_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20"
};

static const char * const vlp_vcore_pbus_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_osc_d2",
	"cksys_mainpll_d7"
};

static const char * const vlp_vcore_pbus_26m_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20"
};

static const char * const vlp_camtg0_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_192m_d32",
	"cksys_univpll_192m_d16",
	"cksys_vlp_f26m_d2",
	"cksys_osc_d40",
	"cksys_osc_d32",
	"cksys_univpll_192m_d10",
	"cksys_univpll_192m_d8",
	"cksys_univpll_d6_d16",
	"cksys_vlp_osc3",
	"cksys_osc_d20",
	"cksys_univpll_d6_d8"
};

static const char * const vlp_camtg1_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_192m_d32",
	"cksys_univpll_192m_d16",
	"cksys_vlp_f26m_d2",
	"cksys_osc_d40",
	"cksys_osc_d32",
	"cksys_univpll_192m_d10",
	"cksys_univpll_192m_d8",
	"cksys_univpll_d6_d16",
	"cksys_vlp_osc3",
	"cksys_osc_d20",
	"cksys_univpll_d6_d8"
};

static const char * const vlp_camtg2_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_192m_d32",
	"cksys_univpll_192m_d16",
	"cksys_vlp_f26m_d2",
	"cksys_osc_d40",
	"cksys_osc_d32",
	"cksys_univpll_192m_d10",
	"cksys_univpll_192m_d8",
	"cksys_univpll_d6_d16",
	"cksys_osc_d20",
	"cksys_univpll_d6_d8"
};

static const char * const vlp_camtg3_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_192m_d32",
	"cksys_univpll_192m_d16",
	"cksys_vlp_f26m_d2",
	"cksys_osc_d40",
	"cksys_osc_d32",
	"cksys_univpll_192m_d10",
	"cksys_univpll_192m_d8",
	"cksys_univpll_d6_d16",
	"cksys_osc_d20",
	"cksys_univpll_d6_d8"
};

static const char * const vlp_camtg4_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_192m_d32",
	"cksys_univpll_192m_d16",
	"cksys_vlp_f26m_d2",
	"cksys_osc_d40",
	"cksys_osc_d32",
	"cksys_univpll_192m_d10",
	"cksys_univpll_192m_d8",
	"cksys_univpll_d6_d16",
	"cksys_osc_d20",
	"cksys_univpll_d6_d8"
};

static const char * const vlp_camtg5_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_192m_d32",
	"cksys_univpll_192m_d16",
	"cksys_vlp_f26m_d2",
	"cksys_osc_d40",
	"cksys_osc_d32",
	"cksys_univpll_192m_d10",
	"cksys_univpll_192m_d8",
	"cksys_univpll_d6_d16",
	"cksys_osc_d20",
	"cksys_univpll_d6_d8"
};

static const char * const vlp_camtg6_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_192m_d32",
	"cksys_univpll_192m_d16",
	"cksys_vlp_f26m_d2",
	"cksys_osc_d40",
	"cksys_osc_d32",
	"cksys_univpll_192m_d10",
	"cksys_univpll_192m_d8",
	"cksys_univpll_d6_d16",
	"cksys_osc_d20",
	"cksys_univpll_d6_d8"
};

static const char * const vlp_camtg7_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_univpll_192m_d32",
	"cksys_univpll_192m_d16",
	"cksys_vlp_f26m_d2",
	"cksys_osc_d40",
	"cksys_osc_d32",
	"cksys_univpll_192m_d10",
	"cksys_univpll_192m_d8",
	"cksys_univpll_d6_d16",
	"cksys_osc_d20",
	"cksys_univpll_d6_d8"
};

static const char * const vlp_aud_engen1_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_vlp_clksq_ck",
	"cksys_apll1_d8",
	"cksys_apll1_d4"
};

static const char * const vlp_aud_engen2_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_vlp_clksq_ck",
	"cksys_apll2_d8",
	"cksys_apll2_d4"
};

static const char * const vlp_aud_sw_engen1_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_vlp_clksq_ck",
	"cksys_apll1_d8"
};

static const char * const vlp_aud_sw_engen2_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_vlp_clksq_ck",
	"cksys_apll2_d8"
};

static const char * const vlp_aud_intbus_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_vlp_clksq_ck",
	"cksys_mainpll_d7_d4",
	"cksys_mainpll_d4_d4"
};

static const char * const vlp_audio_h_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_vlp_clksq_ck",
	"cksys_apll1_ck",
	"cksys_apll2_ck"
};

static const char * const vlp_usb_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d9",
	"cksys_osc_d2"
};

static const char * const vlp_usb_xhci_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d9",
	"cksys_osc_d2"
};

static const char * const vlp_spu0_vlp_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_mainpll_d4_d4",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d7",
	"cksys_mainpll_d6",
	"cksys_mainpll_d5"
};

static const char * const vlp_scp_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_apll1_ck"
};

static const char * const vlp_scp_spi_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_mainpll_d7_d2",
	"cksys_mainpll_d5_d2"
};

static const char * const vlp_scp_iic_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_mainpll_d7_d2"
};

static const char * const vlp_scp_iic_hs_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_mainpll_d7_d2",
	"cksys_mainpll_d7"
};

static const char * const vlp_usb_mem_vlp_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d3"
};

static const char * const vlp_disp_pwm_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d32",
	"cksys_osc_d8",
	"cksys_univpll_d6_d4",
	"cksys_univpll_d5_d4",
	"cksys_osc_d4",
	"cksys_mainpll_d4_d4"
};

static const char * const vlp_pwrap_ulposc_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_osc_d10"
};

static const char * const vlp_tia_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20",
	"cksys_osc_d10"
};

static const char * const vlp_spmi_m_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d32",
	"cksys_osc_d20",
	"cksys_osc_d10"
};

static const char * const vlp_hvs_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d20"
};

static const struct mtk_mux cksys_vlp_muxes[] = {
#if MT_CCF_MUX_DISABLE
	/* VLP_CLK_CFG_0 */
	MUX_FENC_CLR_SET_UPD_CHK(VLP_SSPM_26M_SEL/* dts */, "vlp_sspm_26m_sel",
		vlp_sspm_26m_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_26M_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 31/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 31/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_ULPOSC_SSPM_SEL/* dts */, "vlp_ulposc_sspm_sel",
		vlp_ulposc_sspm_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ULPOSC_SSPM_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 30/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 30/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_SSPM_SEL/* dts */, "vlp_sspm_sel",
		vlp_sspm_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 29/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 29/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_SPM_SEL/* dts */, "vlp_spm_sel",
		vlp_spm_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPM_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 28/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 28/* fenc shift */),
	/* VLP_CLK_CFG_1 */
	MUX_FENC_CLR_SET_UPD_CHK(VLP_AXI_VLP_SEL/* dts */, "vlp_axi_vlp_sel",
		vlp_axi_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_VLP_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 27/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 27/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_NOC_VLP_SEL/* dts */, "vlp_noc_vlp_sel",
		vlp_noc_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_NOC_VLP_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 26/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 26/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_PWM_VLP_SEL/* dts */, "vlp_pwm_vlp_sel",
		vlp_pwm_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWM_VLP_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 25/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 25/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_SYSTIMER_26M_SEL/* dts */, "vlp_systimer_26m_sel",
		vlp_systimer_26m_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SYSTIMER_26M_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 24/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 24/* fenc shift */),
	/* VLP_CLK_CFG_2 */
	MUX_FENC_CLR_SET_UPD_CHK(VLP_DPSW_SEL/* dts */, "vlp_dpsw_sel",
		vlp_dpsw_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DPSW_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 23/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 23/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_DPSW_CENTRAL_SEL/* dts */, "vlp_dpsw_central_sel",
		vlp_dpsw_central_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DPSW_CENTRAL_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 22/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 22/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_SRCK_SEL/* dts */, "vlp_srck_sel",
		vlp_srck_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRCK_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 21/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 21/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_DVFSRC_SEL/* dts */, "vlp_dvfsrc_sel",
		vlp_dvfsrc_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DVFSRC_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 20/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 20/* fenc shift */),
	/* VLP_CLK_CFG_3 */
	MUX_FENC_CLR_SET_UPD_CHK(VLP_DBG_ERR_VLP_26M_SEL/* dts */, "vlp_dbg_err_vlp_26m_sel",
		vlp_dbg_err_vlp_26m_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DEBUG_ERR_FLAG_VLP_26M_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 18/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 18/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_IPS_SEL/* dts */, "vlp_ips_sel",
		vlp_ips_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_IPS_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 17/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 17/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_DPMSRDMA_SEL/* dts */, "vlp_dpmsrdma_sel",
		vlp_dpmsrdma_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DPMSRDMA_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 16/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 16/* fenc shift */),
	/* VLP_CLK_CFG_4 */
	MUX_FENC_CLR_SET_UPD_CHK(VLP_VLP_PBUS_SEL/* dts */, "vlp_vlp_pbus_sel",
		vlp_vlp_pbus_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VLP_PBUS_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 15/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 15/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_VLP_PBUS_26M_SEL/* dts */, "vlp_vlp_pbus_26m_sel",
		vlp_vlp_pbus_26m_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VLP_PBUS_26M_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 14/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 14/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_VCORE_PBUS_SEL/* dts */, "vlp_vcore_pbus_sel",
		vlp_vcore_pbus_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VCORE_PBUS_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 13/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 13/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_VCORE_PBUS_26M_SEL/* dts */, "vlp_vcore_pbus_26m_sel",
		vlp_vcore_pbus_26m_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VCORE_PBUS_26M_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 12/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 12/* fenc shift */),
	/* VLP_CLK_CFG_5 */
	MUX_FENC_CLR_SET_UPD_CHK(VLP_CAMTG0_SEL/* dts */, "vlp_camtg0_sel",
		vlp_camtg0_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG0_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 11/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 11/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_CAMTG1_SEL/* dts */, "vlp_camtg1_sel",
		vlp_camtg1_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG1_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 10/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 10/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_CAMTG2_SEL/* dts */, "vlp_camtg2_sel",
		vlp_camtg2_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG2_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 9/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 9/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_CAMTG3_SEL/* dts */, "vlp_camtg3_sel",
		vlp_camtg3_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG3_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 8/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 8/* fenc shift */),
	/* VLP_CLK_CFG_6 */
	MUX_FENC_CLR_SET_UPD_CHK(VLP_CAMTG4_SEL/* dts */, "vlp_camtg4_sel",
		vlp_camtg4_parents/* parent */, VLP_CLK_CFG_6, VLP_CLK_CFG_6_SET,
		VLP_CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG4_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 7/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 7/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_CAMTG5_SEL/* dts */, "vlp_camtg5_sel",
		vlp_camtg5_parents/* parent */, VLP_CLK_CFG_6, VLP_CLK_CFG_6_SET,
		VLP_CLK_CFG_6_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG5_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 6/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 6/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_CAMTG6_SEL/* dts */, "vlp_camtg6_sel",
		vlp_camtg6_parents/* parent */, VLP_CLK_CFG_6, VLP_CLK_CFG_6_SET,
		VLP_CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG6_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 5/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 5/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_CAMTG7_SEL/* dts */, "vlp_camtg7_sel",
		vlp_camtg7_parents/* parent */, VLP_CLK_CFG_6, VLP_CLK_CFG_6_SET,
		VLP_CLK_CFG_6_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG7_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 4/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 4/* fenc shift */),
	/* VLP_CLK_CFG_7 */
	MUX_FENC_CLR_SET_UPD_CHK(VLP_AUD_ENGEN1_SEL/* dts */, "vlp_aud_engen1_sel",
		vlp_aud_engen1_parents/* parent */, VLP_CLK_CFG_7, VLP_CLK_CFG_7_SET,
		VLP_CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AUD_ENGEN1_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 3/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 3/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_AUD_ENGEN2_SEL/* dts */, "vlp_aud_engen2_sel",
		vlp_aud_engen2_parents/* parent */, VLP_CLK_CFG_7, VLP_CLK_CFG_7_SET,
		VLP_CLK_CFG_7_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AUD_ENGEN2_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 2/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 2/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_AUD_SW_ENGEN1_SEL/* dts */, "vlp_aud_sw_engen1_sel",
		vlp_aud_sw_engen1_parents/* parent */, VLP_CLK_CFG_7, VLP_CLK_CFG_7_SET,
		VLP_CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AUD_SW_ENGEN1_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 1/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 1/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_AUD_SW_ENGEN2_SEL/* dts */, "vlp_aud_sw_engen2_sel",
		vlp_aud_sw_engen2_parents/* parent */, VLP_CLK_CFG_7, VLP_CLK_CFG_7_SET,
		VLP_CLK_CFG_7_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_SW_ENGEN2_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 31/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 0/* fenc shift */),
	/* VLP_CLK_CFG_8 */
	MUX_FENC_CLR_SET_UPD_CHK(VLP_AUD_INTBUS_SEL/* dts */, "vlp_aud_intbus_sel",
		vlp_aud_intbus_parents/* parent */, VLP_CLK_CFG_8, VLP_CLK_CFG_8_SET,
		VLP_CLK_CFG_8_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_INTBUS_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 30/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 31/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_AUDIO_H_SEL/* dts */, "vlp_audio_h_sel",
		vlp_audio_h_parents/* parent */, VLP_CLK_CFG_8, VLP_CLK_CFG_8_SET,
		VLP_CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUDIO_H_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 29/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 30/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_USB_TOP_SEL/* dts */, "vlp_usb_sel",
		vlp_usb_parents/* parent */, VLP_CLK_CFG_8, VLP_CLK_CFG_8_SET,
		VLP_CLK_CFG_8_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_USB_TOP_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 28/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 29/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_USB_XHCI_SEL/* dts */, "vlp_usb_xhci_sel",
		vlp_usb_xhci_parents/* parent */, VLP_CLK_CFG_8, VLP_CLK_CFG_8_SET,
		VLP_CLK_CFG_8_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SSUSB_XHCI_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 27/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 28/* fenc shift */),
	/* VLP_CLK_CFG_9 */
	MUX_FENC_CLR_SET_UPD_CHK(VLP_SPU0_VLP_SEL/* dts */, "vlp_spu0_vlp_sel",
		vlp_spu0_vlp_parents/* parent */, VLP_CLK_CFG_9, VLP_CLK_CFG_9_SET,
		VLP_CLK_CFG_9_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPU0_VLP_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 25/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 26/* fenc shift */),
	/* VLP_CLK_CFG_10 */
	MUX_FENC_CLR_SET_UPD_CHK(VLP_SCP_SEL/* dts */, "vlp_scp_sel",
		vlp_scp_parents/* parent */, VLP_CLK_CFG_10, VLP_CLK_CFG_10_SET,
		VLP_CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SCP_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 22/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 23/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_SCP_SPI_SEL/* dts */, "vlp_scp_spi_sel",
		vlp_scp_spi_parents/* parent */, VLP_CLK_CFG_10, VLP_CLK_CFG_10_SET,
		VLP_CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SCP_SPI_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 21/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 22/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_SCP_IIC_SEL/* dts */, "vlp_scp_iic_sel",
		vlp_scp_iic_parents/* parent */, VLP_CLK_CFG_10, VLP_CLK_CFG_10_SET,
		VLP_CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SCP_IIC_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 20/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 21/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_SCP_IIC_HIGH_SPD_SEL/* dts */, "vlp_scp_iic_hs_sel",
		vlp_scp_iic_hs_parents/* parent */, VLP_CLK_CFG_10, VLP_CLK_CFG_10_SET,
		VLP_CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 19/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 20/* fenc shift */),
	/* VLP_CLK_CFG_11 */
	MUX_FENC_CLR_SET_UPD_CHK(VLP_USB_MEM_VLP_SEL/* dts */, "vlp_usb_mem_vlp_sel",
		vlp_usb_mem_vlp_parents/* parent */, VLP_CLK_CFG_11, VLP_CLK_CFG_11_SET,
		VLP_CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_USB_MEM_VLP_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 17/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 18/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_DISP_PWM_SEL/* dts */, "vlp_disp_pwm_sel",
		vlp_disp_pwm_parents/* parent */, VLP_CLK_CFG_11, VLP_CLK_CFG_11_SET,
		VLP_CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DISP_PWM_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 16/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 17/* fenc shift */),
	/* VLP_CLK_CFG_12 */
	MUX_FENC_CLR_SET_UPD_CHK(VLP_PWRAP_ULPOSC_SEL/* dts */, "vlp_pwrap_ulposc_sel",
		vlp_pwrap_ulposc_parents/* parent */, VLP_CLK_CFG_12, VLP_CLK_CFG_12_SET,
		VLP_CLK_CFG_12_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_PWRAP_ULPOSC_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 14/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 15/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_TIA_SEL/* dts */, "vlp_tia_sel",
		vlp_tia_parents/* parent */, VLP_CLK_CFG_12, VLP_CLK_CFG_12_SET,
		VLP_CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_TIA_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 13/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 14/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_SPMI_M_MST_SEL/* dts */, "vlp_spmi_m_sel",
		vlp_spmi_m_parents/* parent */, VLP_CLK_CFG_12, VLP_CLK_CFG_12_SET,
		VLP_CLK_CFG_12_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPMI_M_MST_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 12/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 13/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_HVS_SEL/* dts */, "vlp_hvs_sel",
		vlp_hvs_parents/* parent */, VLP_CLK_CFG_12, VLP_CLK_CFG_12_SET,
		VLP_CLK_CFG_12_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_HVS_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 11/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 12/* fenc shift */),
#else
	/* VLP_CLK_CFG_0 */
	MUX_FENC_CLR_SET_UPD_CHK(VLP_SSPM_26M_SEL/* dts */, "vlp_sspm_26m_sel",
		vlp_sspm_26m_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_26M_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 31/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 31/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_ULPOSC_SSPM_SEL/* dts */, "vlp_ulposc_sspm_sel",
		vlp_ulposc_sspm_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ULPOSC_SSPM_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 30/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 30/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_SSPM_SEL/* dts */, "vlp_sspm_sel",
		vlp_sspm_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 29/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 29/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_SPM_SEL/* dts */, "vlp_spm_sel",
		vlp_spm_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPM_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 28/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 28/* fenc shift */),
	/* VLP_CLK_CFG_1 */
	MUX_FENC_CLR_SET_UPD_CHK(VLP_AXI_VLP_SEL/* dts */, "vlp_axi_vlp_sel",
		vlp_axi_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_VLP_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 27/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 27/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_NOC_VLP_SEL/* dts */, "vlp_noc_vlp_sel",
		vlp_noc_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_NOC_VLP_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 26/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 26/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(VLP_PWM_VLP_SEL/* dts */, "vlp_pwm_vlp_sel",
		vlp_pwm_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_PWM_VLP_SHIFT/* upd shift */, VLP_CKSTA_REG/* cksta ofs */,
		25/* cksta shift */, VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		25/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_SYSTIMER_26M_SEL/* dts */, "vlp_systimer_26m_sel",
		vlp_systimer_26m_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SYSTIMER_26M_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 24/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 24/* fenc shift */),
	/* VLP_CLK_CFG_2 */
	MUX_FENC_CLR_SET_UPD_CHK(VLP_DPSW_SEL/* dts */, "vlp_dpsw_sel",
		vlp_dpsw_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DPSW_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 23/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 23/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_DPSW_CENTRAL_SEL/* dts */, "vlp_dpsw_central_sel",
		vlp_dpsw_central_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DPSW_CENTRAL_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 22/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 22/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_SRCK_SEL/* dts */, "vlp_srck_sel",
		vlp_srck_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRCK_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 21/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 21/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_DVFSRC_SEL/* dts */, "vlp_dvfsrc_sel",
		vlp_dvfsrc_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DVFSRC_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 20/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 20/* fenc shift */),
	/* VLP_CLK_CFG_3 */
	MUX_FENC_CLR_SET_UPD_CHK(VLP_DBG_ERR_VLP_26M_SEL/* dts */, "vlp_dbg_err_vlp_26m_sel",
		vlp_dbg_err_vlp_26m_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DEBUG_ERR_FLAG_VLP_26M_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 18/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 18/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(VLP_IPS_SEL/* dts */, "vlp_ips_sel",
		vlp_ips_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_IPS_SHIFT/* upd shift */, VLP_CKSTA_REG/* cksta ofs */,
		17/* cksta shift */, VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		17/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_DPMSRDMA_SEL/* dts */, "vlp_dpmsrdma_sel",
		vlp_dpmsrdma_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DPMSRDMA_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 16/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 16/* fenc shift */),
	/* VLP_CLK_CFG_4 */
	MUX_FENC_CLR_SET_UPD_CHK(VLP_VLP_PBUS_SEL/* dts */, "vlp_vlp_pbus_sel",
		vlp_vlp_pbus_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VLP_PBUS_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 15/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 15/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_VLP_PBUS_26M_SEL/* dts */, "vlp_vlp_pbus_26m_sel",
		vlp_vlp_pbus_26m_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VLP_PBUS_26M_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 14/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 14/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_VCORE_PBUS_SEL/* dts */, "vlp_vcore_pbus_sel",
		vlp_vcore_pbus_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VCORE_PBUS_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 13/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 13/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_VCORE_PBUS_26M_SEL/* dts */, "vlp_vcore_pbus_26m_sel",
		vlp_vcore_pbus_26m_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VCORE_PBUS_26M_SHIFT/* upd shift */,
		VLP_CKSTA_REG/* cksta ofs */, 12/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */, 12/* fenc shift */),
	/* VLP_CLK_CFG_5 */
	MUX_GENERIC_HWV(VLP_CAMTG0_SEL/* dts */, "vlp_camtg0_sel",
		vlp_camtg0_parents/* parent */, VLP_CLK_CFG_5,
		VLP_CLK_CFG_5_SET, VLP_CLK_CFG_5_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, AP_HWV_HWV_CG_GRP_5_DONE,
		AP_HWV_HWV_CG_GRP_5_SET, AP_HWV_HWV_CG_GRP_5_CLR, /* hwv */
		0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG0_SHIFT/* upd shift */, VLP_CKSTA_REG/* cksta ofs */,
		11/* cksta shift */, VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		11/* fenc shift */),
	MUX_GENERIC_HWV(VLP_CAMTG1_SEL/* dts */, "vlp_camtg1_sel",
		vlp_camtg1_parents/* parent */, VLP_CLK_CFG_5,
		VLP_CLK_CFG_5_SET, VLP_CLK_CFG_5_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, AP_HWV_HWV_CG_GRP_5_DONE,
		AP_HWV_HWV_CG_GRP_5_SET, AP_HWV_HWV_CG_GRP_5_CLR, /* hwv */
		8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG1_SHIFT/* upd shift */, VLP_CKSTA_REG/* cksta ofs */,
		10/* cksta shift */, VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		10/* fenc shift */),
	MUX_GENERIC_HWV(VLP_CAMTG2_SEL/* dts */, "vlp_camtg2_sel",
		vlp_camtg2_parents/* parent */, VLP_CLK_CFG_5,
		VLP_CLK_CFG_5_SET, VLP_CLK_CFG_5_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, AP_HWV_HWV_CG_GRP_5_DONE,
		AP_HWV_HWV_CG_GRP_5_SET, AP_HWV_HWV_CG_GRP_5_CLR, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG2_SHIFT/* upd shift */, VLP_CKSTA_REG/* cksta ofs */,
		9/* cksta shift */, VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		9/* fenc shift */),
	MUX_GENERIC_HWV(VLP_CAMTG3_SEL/* dts */, "vlp_camtg3_sel",
		vlp_camtg3_parents/* parent */, VLP_CLK_CFG_5,
		VLP_CLK_CFG_5_SET, VLP_CLK_CFG_5_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, AP_HWV_HWV_CG_GRP_5_DONE,
		AP_HWV_HWV_CG_GRP_5_SET, AP_HWV_HWV_CG_GRP_5_CLR, /* hwv */
		24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG3_SHIFT/* upd shift */, VLP_CKSTA_REG/* cksta ofs */,
		8/* cksta shift */, VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		8/* fenc shift */),
	/* VLP_CLK_CFG_6 */
	MUX_GENERIC_HWV(VLP_CAMTG4_SEL/* dts */, "vlp_camtg4_sel",
		vlp_camtg4_parents/* parent */, VLP_CLK_CFG_6,
		VLP_CLK_CFG_6_SET, VLP_CLK_CFG_6_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, AP_HWV_HWV_CG_GRP_6_DONE,
		AP_HWV_HWV_CG_GRP_6_SET, AP_HWV_HWV_CG_GRP_6_CLR, /* hwv */
		0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG4_SHIFT/* upd shift */, VLP_CKSTA_REG/* cksta ofs */,
		7/* cksta shift */, VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		7/* fenc shift */),
	MUX_GENERIC_HWV(VLP_CAMTG5_SEL/* dts */, "vlp_camtg5_sel",
		vlp_camtg5_parents/* parent */, VLP_CLK_CFG_6,
		VLP_CLK_CFG_6_SET, VLP_CLK_CFG_6_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, AP_HWV_HWV_CG_GRP_6_DONE,
		AP_HWV_HWV_CG_GRP_6_SET, AP_HWV_HWV_CG_GRP_6_CLR, /* hwv */
		8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG5_SHIFT/* upd shift */, VLP_CKSTA_REG/* cksta ofs */,
		6/* cksta shift */, VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		6/* fenc shift */),
	MUX_GENERIC_HWV(VLP_CAMTG6_SEL/* dts */, "vlp_camtg6_sel",
		vlp_camtg6_parents/* parent */, VLP_CLK_CFG_6,
		VLP_CLK_CFG_6_SET, VLP_CLK_CFG_6_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, AP_HWV_HWV_CG_GRP_6_DONE,
		AP_HWV_HWV_CG_GRP_6_SET, AP_HWV_HWV_CG_GRP_6_CLR, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG6_SHIFT/* upd shift */, VLP_CKSTA_REG/* cksta ofs */,
		5/* cksta shift */, VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		5/* fenc shift */),
	MUX_GENERIC_HWV(VLP_CAMTG7_SEL/* dts */, "vlp_camtg7_sel",
		vlp_camtg7_parents/* parent */, VLP_CLK_CFG_6,
		VLP_CLK_CFG_6_SET, VLP_CLK_CFG_6_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, AP_HWV_HWV_CG_GRP_6_DONE,
		AP_HWV_HWV_CG_GRP_6_SET, AP_HWV_HWV_CG_GRP_6_CLR, /* hwv */
		24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG7_SHIFT/* upd shift */, VLP_CKSTA_REG/* cksta ofs */,
		4/* cksta shift */, VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		4/* fenc shift */),
	/* VLP_CLK_CFG_7 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(VLP_AUD_ENGEN1_SEL/* dts */, "vlp_aud_engen1_sel",
		vlp_aud_engen1_parents/* parent */, VLP_CLK_CFG_7, VLP_CLK_CFG_7_SET,
		VLP_CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AUD_ENGEN1_SHIFT/* upd shift */, VLP_CKSTA_REG/* cksta ofs */,
		3/* cksta shift */, VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		3/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(VLP_AUD_ENGEN2_SEL/* dts */, "vlp_aud_engen2_sel",
		vlp_aud_engen2_parents/* parent */, VLP_CLK_CFG_7, VLP_CLK_CFG_7_SET,
		VLP_CLK_CFG_7_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AUD_ENGEN2_SHIFT/* upd shift */, VLP_CKSTA_REG/* cksta ofs */,
		2/* cksta shift */, VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		2/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(VLP_AUD_SW_ENGEN1_SEL/* dts */, "vlp_aud_sw_engen1_sel",
		vlp_aud_sw_engen1_parents/* parent */, VLP_CLK_CFG_7, VLP_CLK_CFG_7_SET,
		VLP_CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AUD_SW_ENGEN1_SHIFT/* upd shift */, VLP_CKSTA_REG/* cksta ofs */,
		1/* cksta shift */, VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		1/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(VLP_AUD_SW_ENGEN2_SEL/* dts */, "vlp_aud_sw_engen2_sel",
		vlp_aud_sw_engen2_parents/* parent */, VLP_CLK_CFG_7, VLP_CLK_CFG_7_SET,
		VLP_CLK_CFG_7_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_SW_ENGEN2_SHIFT/* upd shift */, VLP_CKSTA_REG1/* cksta ofs */,
		31/* cksta shift */, VLP_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		0/* fenc shift */),
	/* VLP_CLK_CFG_8 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(VLP_AUD_INTBUS_SEL/* dts */, "vlp_aud_intbus_sel",
		vlp_aud_intbus_parents/* parent */, VLP_CLK_CFG_8, VLP_CLK_CFG_8_SET,
		VLP_CLK_CFG_8_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_INTBUS_SHIFT/* upd shift */, VLP_CKSTA_REG1/* cksta ofs */,
		30/* cksta shift */, VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */,
		31/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(VLP_AUDIO_H_SEL/* dts */, "vlp_audio_h_sel",
		vlp_audio_h_parents/* parent */, VLP_CLK_CFG_8, VLP_CLK_CFG_8_SET,
		VLP_CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUDIO_H_SHIFT/* upd shift */, VLP_CKSTA_REG1/* cksta ofs */,
		29/* cksta shift */, VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */,
		30/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(VLP_USB_TOP_SEL/* dts */, "vlp_usb_sel",
		vlp_usb_parents/* parent */, VLP_CLK_CFG_8, VLP_CLK_CFG_8_SET,
		VLP_CLK_CFG_8_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_USB_TOP_SHIFT/* upd shift */, VLP_CKSTA_REG1/* cksta ofs */,
		28/* cksta shift */, VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */,
		29/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(VLP_USB_XHCI_SEL/* dts */, "vlp_usb_xhci_sel",
		vlp_usb_xhci_parents/* parent */, VLP_CLK_CFG_8, VLP_CLK_CFG_8_SET,
		VLP_CLK_CFG_8_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSUSB_XHCI_SHIFT/* upd shift */, VLP_CKSTA_REG1/* cksta ofs */,
		27/* cksta shift */, VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */,
		28/* fenc shift */),
	/* VLP_CLK_CFG_9 */
	MUX_FENC_CLR_SET_UPD_CHK(VLP_SPU0_VLP_SEL/* dts */, "vlp_spu0_vlp_sel",
		vlp_spu0_vlp_parents/* parent */, VLP_CLK_CFG_9, VLP_CLK_CFG_9_SET,
		VLP_CLK_CFG_9_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPU0_VLP_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 25/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 26/* fenc shift */),
	/* VLP_CLK_CFG_10 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(VLP_SCP_SEL/* dts */, "vlp_scp_sel",
		vlp_scp_parents/* parent */, VLP_CLK_CFG_10, VLP_CLK_CFG_10_SET,
		VLP_CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SCP_SHIFT/* upd shift */, VLP_CKSTA_REG1/* cksta ofs */,
		22/* cksta shift */, VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */,
		23/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_SCP_SPI_SEL/* dts */, "vlp_scp_spi_sel",
		vlp_scp_spi_parents/* parent */, VLP_CLK_CFG_10, VLP_CLK_CFG_10_SET,
		VLP_CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SCP_SPI_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 21/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 22/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_SCP_IIC_SEL/* dts */, "vlp_scp_iic_sel",
		vlp_scp_iic_parents/* parent */, VLP_CLK_CFG_10, VLP_CLK_CFG_10_SET,
		VLP_CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SCP_IIC_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 20/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 21/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_SCP_IIC_HIGH_SPD_SEL/* dts */, "vlp_scp_iic_hs_sel",
		vlp_scp_iic_hs_parents/* parent */, VLP_CLK_CFG_10, VLP_CLK_CFG_10_SET,
		VLP_CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 19/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 20/* fenc shift */),
	/* VLP_CLK_CFG_11 */
	MUX_FENC_CLR_SET_UPD_CHK(VLP_USB_MEM_VLP_SEL/* dts */, "vlp_usb_mem_vlp_sel",
		vlp_usb_mem_vlp_parents/* parent */, VLP_CLK_CFG_11, VLP_CLK_CFG_11_SET,
		VLP_CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_USB_MEM_VLP_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 17/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 18/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(VLP_DISP_PWM_SEL/* dts */, "vlp_disp_pwm_sel",
		vlp_disp_pwm_parents/* parent */, VLP_CLK_CFG_11, VLP_CLK_CFG_11_SET,
		VLP_CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_DISP_PWM_SHIFT/* upd shift */, VLP_CKSTA_REG1/* cksta ofs */,
		16/* cksta shift */, VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */,
		17/* fenc shift */),
	/* VLP_CLK_CFG_12 */
	MUX_FENC_CLR_SET_UPD_CHK(VLP_PWRAP_ULPOSC_SEL/* dts */, "vlp_pwrap_ulposc_sel",
		vlp_pwrap_ulposc_parents/* parent */, VLP_CLK_CFG_12, VLP_CLK_CFG_12_SET,
		VLP_CLK_CFG_12_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_PWRAP_ULPOSC_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 14/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 15/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_TIA_SEL/* dts */, "vlp_tia_sel",
		vlp_tia_parents/* parent */, VLP_CLK_CFG_12, VLP_CLK_CFG_12_SET,
		VLP_CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_TIA_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 13/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 14/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_SPMI_M_MST_SEL/* dts */, "vlp_spmi_m_sel",
		vlp_spmi_m_parents/* parent */, VLP_CLK_CFG_12, VLP_CLK_CFG_12_SET,
		VLP_CLK_CFG_12_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPMI_M_MST_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 12/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 13/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(VLP_HVS_SEL/* dts */, "vlp_hvs_sel",
		vlp_hvs_parents/* parent */, VLP_CLK_CFG_12, VLP_CLK_CFG_12_SET,
		VLP_CLK_CFG_12_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_HVS_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 11/* cksta shift */,
		VLP_CLK_FENC_STATUS_MON_1/* fenc ofs */, 12/* fenc shift */),
#endif
};

static const char * const mm_mmup_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d2",
	"cksys_osc",
	"cksys_mainpll_d4",
	"mm_mainpll2_d3"
};

static const char * const mm_mminfra_ao_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d4",
	"cksys_mainpll_d3"
};

static const char * const mm_mminfra_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d4",
	"cksys_mainpll_d7_d2",
	"cksys_mainpll_d5_d2",
	"cksys_mainpll_d7",
	"mm_mmpll_d6_d2",
	"mm_mainpll2_d4_d2",
	"mm_mainpll_d3_d2",
	"mm_univpll2_d6",
	"mm_mainpll2_d5",
	"mm_mmpll_d6",
	"mm_univpll2_d5",
	"mm_mainpll2_d4",
	"mm_univpll2_d4",
	"mm_mainpll2_d3",
	"mm_mmpll_d3"
};

static const char * const mm_mminfra_snoc_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d4",
	"cksys_mainpll_d7_d2",
	"mm_mainpll_d3_d3",
	"cksys_mainpll_d7",
	"mm_mainpll_d3_d2",
	"mm_mmpll_d4_d2",
	"mm_mmpll_d6",
	"cksys_mainpll_d4",
	"mm_univpll2_d4",
	"mm_mmpll_d4",
	"mm_mainpll2_d3",
	"mm_univpll2_d3",
	"mm_mainpll2_d2",
	"mm_univpll2_d2"
};

static const char * const mm_venc_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"mm_mainpll2_d5_d2",
	"mm_univpll2_d5_d2",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d7",
	"mm_mmpll_d4_d2",
	"mm_mmpll_d6",
	"mm_univpll2_d4",
	"mm_mmpll_d4",
	"mm_univpll2_d3",
	"mm_mmpll_d3"
};

static const char * const mm_venc_mdp_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"mm_mainpll2_d5_d2",
	"mm_univpll2_d5_d2",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d7",
	"mm_mmpll_d4_d2",
	"mm_mmpll_d6",
	"mm_univpll2_d4",
	"mm_mmpll_d4",
	"mm_univpll2_d3",
	"mm_mmpll_d3"
};

static const char * const mm_vdec_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"mm_mainpll2_d6_d2",
	"cksys_mainpll_d5_d2",
	"mm_mainpll2_d9",
	"mm_mainpll2_d4_d2",
	"mm_univpll2_d6",
	"mm_mainpll2_d5",
	"mm_univpll2_d5",
	"mm_mainpll2_d4",
	"mm_imgpll_d2"
};

static const char * const mm_img1_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d4",
	"cksys_osc_d3",
	"mm_mmpll_d6_d2",
	"cksys_osc_d2",
	"mm_imgpll_d5_d2",
	"mm_mmpll_d5_d2",
	"mm_univpll2_d4_d2",
	"mm_mmpll_d4_d2",
	"mm_mmpll_d7",
	"mm_univpll2_d6",
	"mm_mmpll_d6",
	"mm_univpll2_d5",
	"mm_mmpll_d5",
	"mm_univpll2_d4",
	"mm_imgpll_d4"
};

static const char * const mm_ipe_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d4",
	"cksys_osc_d3",
	"cksys_osc_d2",
	"mm_univpll2_d6",
	"mm_mmpll_d6",
	"mm_univpll2_d5",
	"cksys_mainpll_d4",
	"mm_univpll2_d4",
	"mm_mmpll_d4",
	"cksys_mainpll_d3"
};

static const char * const mm_disp_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d7",
	"mm_mainpll_d3_d2",
	"cksys_mainpll_d5",
	"mm_mmpll_d6",
	"mm_mainpll2_d4",
	"mm_univpll2_d4",
	"mm_mainpll2_d3",
	"mm_univpll2_d3"
};

static const char * const mm_mml_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_mainpll_d4_d2",
	"cksys_mainpll_d7",
	"mm_mainpll_d3_d2",
	"cksys_mainpll_d5",
	"mm_mmpll_d6",
	"mm_mainpll2_d4",
	"mm_univpll2_d4",
	"mm_mainpll2_d3",
	"mm_univpll2_d3"
};

static const char * const mm_dvo_dp_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"mm_tvdpll_d8",
	"mm_tvdpll_d4",
	"mm_tvdpll_d3",
	"mm_tvdpll_d2"
};

static const char * const mm_dvo_favt_dp_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"mm_tvdpll_d8",
	"mm_tvdpll_d4",
	"cksys_apll1_ck",
	"cksys_apll2_ck",
	"mm_tvdpll_d3",
	"mm_tvdpll_d2"
};

static const char * const mm_cam_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d10",
	"cksys_osc_d4",
	"cksys_osc_d3",
	"cksys_osc_d2",
	"mm_mmpll_d5_d2",
	"mm_univpll2_d4_d2",
	"mm_mmpll_d4_d2",
	"mm_univpll2_d7",
	"mm_mmpll_d7",
	"mm_univpll2_d6",
	"mm_mmpll_d6",
	"mm_univpll2_d5",
	"mm_mmpll_d5",
	"mm_univpll2_d4",
	"mm_imgpll_d4"
};

static const char * const mm_camtm_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"mm_univpll2_d6_d4",
	"cksys_osc_d4",
	"cksys_osc_d3",
	"mm_univpll2_d6_d2"
};

static const char * const mm_ccusys_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d4",
	"cksys_osc_d3",
	"cksys_osc_d2",
	"mm_mainpll2_d3",
	"mm_univpll2_d3"
};

static const char * const mm_ccutm_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"mm_univpll2_d6_d4",
	"cksys_osc_d4",
	"cksys_osc_d3",
	"mm_univpll2_d6_d2"
};

static const char * const mm_seninf0_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d10",
	"cksys_osc_d8",
	"cksys_osc_d5",
	"cksys_osc_d4",
	"mm_univpll2_d6_d2",
	"mm_mainpll2_d9",
	"cksys_osc_d2",
	"mm_mainpll2_d4_d2",
	"mm_univpll2_d4_d2",
	"mm_mmpll_d4_d2",
	"mm_univpll2_d7",
	"mm_mainpll2_d6",
	"mm_mmpll_d7",
	"mm_univpll2_d6",
	"mm_univpll2_d5"
};

static const char * const mm_seninf1_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d10",
	"cksys_osc_d8",
	"cksys_osc_d5",
	"cksys_osc_d4",
	"mm_univpll2_d6_d2",
	"mm_mainpll2_d9",
	"cksys_osc_d2",
	"mm_mainpll2_d4_d2",
	"mm_univpll2_d4_d2",
	"mm_mmpll_d4_d2",
	"mm_univpll2_d7",
	"mm_mainpll2_d6",
	"mm_mmpll_d7",
	"mm_univpll2_d6",
	"mm_univpll2_d5"
};

static const char * const mm_seninf2_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d10",
	"cksys_osc_d8",
	"cksys_osc_d5",
	"cksys_osc_d4",
	"mm_univpll2_d6_d2",
	"mm_mainpll2_d9",
	"cksys_osc_d2",
	"mm_mainpll2_d4_d2",
	"mm_univpll2_d4_d2",
	"mm_mmpll_d4_d2",
	"mm_univpll2_d7",
	"mm_mainpll2_d6",
	"mm_mmpll_d7",
	"mm_univpll2_d6",
	"mm_univpll2_d5"
};

static const char * const mm_seninf3_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d10",
	"cksys_osc_d8",
	"cksys_osc_d5",
	"cksys_osc_d4",
	"mm_univpll2_d6_d2",
	"mm_mainpll2_d9",
	"cksys_osc_d2",
	"mm_mainpll2_d4_d2",
	"mm_univpll2_d4_d2",
	"mm_mmpll_d4_d2",
	"mm_univpll2_d7",
	"mm_mainpll2_d6",
	"mm_mmpll_d7",
	"mm_univpll2_d6",
	"mm_univpll2_d5"
};

static const char * const mm_seninf4_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d10",
	"cksys_osc_d8",
	"cksys_osc_d5",
	"cksys_osc_d4",
	"mm_univpll2_d6_d2",
	"mm_mainpll2_d9",
	"cksys_osc_d2",
	"mm_mainpll2_d4_d2",
	"mm_univpll2_d4_d2",
	"mm_mmpll_d4_d2",
	"mm_univpll2_d7",
	"mm_mainpll2_d6",
	"mm_mmpll_d7",
	"mm_univpll2_d6",
	"mm_univpll2_d5"
};

static const char * const mm_seninf5_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d10",
	"cksys_osc_d8",
	"cksys_osc_d5",
	"cksys_osc_d4",
	"mm_univpll2_d6_d2",
	"mm_mainpll2_d9",
	"cksys_osc_d2",
	"mm_mainpll2_d4_d2",
	"mm_univpll2_d4_d2",
	"mm_mmpll_d4_d2",
	"mm_univpll2_d7",
	"mm_mainpll2_d6",
	"mm_mmpll_d7",
	"mm_univpll2_d6",
	"mm_univpll2_d5"
};

static const char * const mm_mminfra_snoc_slow_parents[] = {
	"cksys_tck_26m_mx9_ck",
	"cksys_osc_d4",
	"cksys_mainpll_d7_d2",
	"cksys_mainpll_d5_d2",
	"cksys_mainpll_d7",
	"mm_mmpll_d6_d2",
	"mm_mainpll2_d4_d2",
	"mm_mainpll_d3_d2",
	"mm_univpll2_d6",
	"mm_mainpll2_d5",
	"mm_mmpll_d6",
	"mm_univpll2_d5",
	"mm_mainpll2_d4",
	"mm_univpll2_d4",
	"mm_mainpll2_d3",
	"mm_mmpll_d3"
};

static const struct mtk_mux cksys_mm_muxes[] = {
#if MT_CCF_MUX_DISABLE
	/* CKSYS2_CLK_CFG_0 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_MMUP_SEL/* dts */, "mm_mmup_sel",
		mm_mmup_parents/* parent */, CKSYS2_CLK_CFG_0, CKSYS2_CLK_CFG_0_SET,
		CKSYS2_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMUP_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 31/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 31/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_MMINFRA_AO_SEL/* dts */, "mm_mminfra_ao_sel",
		mm_mminfra_ao_parents/* parent */, CKSYS2_CLK_CFG_0, CKSYS2_CLK_CFG_0_SET,
		CKSYS2_CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMINFRA_AO_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 30/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 30/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_MMINFRA_SEL/* dts */, "mm_mminfra_sel",
		mm_mminfra_parents/* parent */, CKSYS2_CLK_CFG_0, CKSYS2_CLK_CFG_0_SET,
		CKSYS2_CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMINFRA_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 29/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 29/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_MMINFRA_SNOC_SEL/* dts */, "mm_mminfra_snoc_sel",
		mm_mminfra_snoc_parents/* parent */, CKSYS2_CLK_CFG_0, CKSYS2_CLK_CFG_0_SET,
		CKSYS2_CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMINFRA_SNOC_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 28/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 28/* fenc shift */),
	/* CKSYS2_CLK_CFG_1 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_VENC_SEL/* dts */, "mm_venc_sel",
		mm_venc_parents/* parent */, CKSYS2_CLK_CFG_1, CKSYS2_CLK_CFG_1_SET,
		CKSYS2_CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VENC_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 27/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 27/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_VENC_MDP_SEL/* dts */, "mm_venc_mdp_sel",
		mm_venc_mdp_parents/* parent */, CKSYS2_CLK_CFG_1, CKSYS2_CLK_CFG_1_SET,
		CKSYS2_CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VENC_MDP_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 26/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 26/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_VDEC_SEL/* dts */, "mm_vdec_sel",
		mm_vdec_parents/* parent */, CKSYS2_CLK_CFG_1, CKSYS2_CLK_CFG_1_SET,
		CKSYS2_CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VDEC_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 25/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 25/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_IMG1_SEL/* dts */, "mm_img1_sel",
		mm_img1_parents/* parent */, CKSYS2_CLK_CFG_1, CKSYS2_CLK_CFG_1_SET,
		CKSYS2_CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_IMG1_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 24/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 24/* fenc shift */),
	/* CKSYS2_CLK_CFG_2 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_IPE_SEL/* dts */, "mm_ipe_sel",
		mm_ipe_parents/* parent */, CKSYS2_CLK_CFG_2, CKSYS2_CLK_CFG_2_SET,
		CKSYS2_CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_IPE_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 23/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 23/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_DISP_SEL/* dts */, "mm_disp_sel",
		mm_disp_parents/* parent */, CKSYS2_CLK_CFG_2, CKSYS2_CLK_CFG_2_SET,
		CKSYS2_CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DISP_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 22/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 22/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_MML_SEL/* dts */, "mm_mml_sel",
		mm_mml_parents/* parent */, CKSYS2_CLK_CFG_2, CKSYS2_CLK_CFG_2_SET,
		CKSYS2_CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MML_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 21/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 21/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_DVO_DP_SEL/* dts */, "mm_dvo_dp_sel",
		mm_dvo_dp_parents/* parent */, CKSYS2_CLK_CFG_2, CKSYS2_CLK_CFG_2_SET,
		CKSYS2_CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DVO_DP_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 20/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 20/* fenc shift */),
	/* CKSYS2_CLK_CFG_3 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_DVO_FAVT_DP_SEL/* dts */, "mm_dvo_favt_dp_sel",
		mm_dvo_favt_dp_parents/* parent */, CKSYS2_CLK_CFG_3, CKSYS2_CLK_CFG_3_SET,
		CKSYS2_CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DVO_FAVT_DP_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 19/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 19/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_CAM_SEL/* dts */, "mm_cam_sel",
		mm_cam_parents/* parent */, CKSYS2_CLK_CFG_3, CKSYS2_CLK_CFG_3_SET,
		CKSYS2_CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAM_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 18/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 18/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_CAMTM_SEL/* dts */, "mm_camtm_sel",
		mm_camtm_parents/* parent */, CKSYS2_CLK_CFG_3, CKSYS2_CLK_CFG_3_SET,
		CKSYS2_CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTM_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 17/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 17/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_CCUSYS_SEL/* dts */, "mm_ccusys_sel",
		mm_ccusys_parents/* parent */, CKSYS2_CLK_CFG_3, CKSYS2_CLK_CFG_3_SET,
		CKSYS2_CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CCUSYS_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 16/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 16/* fenc shift */),
	/* CKSYS2_CLK_CFG_4 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_CCUTM_SEL/* dts */, "mm_ccutm_sel",
		mm_ccutm_parents/* parent */, CKSYS2_CLK_CFG_4, CKSYS2_CLK_CFG_4_SET,
		CKSYS2_CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CCUTM_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 15/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 15/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_SENINF0_SEL/* dts */, "mm_seninf0_sel",
		mm_seninf0_parents/* parent */, CKSYS2_CLK_CFG_4, CKSYS2_CLK_CFG_4_SET,
		CKSYS2_CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF0_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 14/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 14/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_SENINF1_SEL/* dts */, "mm_seninf1_sel",
		mm_seninf1_parents/* parent */, CKSYS2_CLK_CFG_4, CKSYS2_CLK_CFG_4_SET,
		CKSYS2_CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF1_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 13/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 13/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_SENINF2_SEL/* dts */, "mm_seninf2_sel",
		mm_seninf2_parents/* parent */, CKSYS2_CLK_CFG_4, CKSYS2_CLK_CFG_4_SET,
		CKSYS2_CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF2_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 12/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 12/* fenc shift */),
	/* CKSYS2_CLK_CFG_5 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_SENINF3_SEL/* dts */, "mm_seninf3_sel",
		mm_seninf3_parents/* parent */, CKSYS2_CLK_CFG_5, CKSYS2_CLK_CFG_5_SET,
		CKSYS2_CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF3_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 11/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 11/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_SENINF4_SEL/* dts */, "mm_seninf4_sel",
		mm_seninf4_parents/* parent */, CKSYS2_CLK_CFG_5, CKSYS2_CLK_CFG_5_SET,
		CKSYS2_CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF4_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 10/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 10/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_SENINF5_SEL/* dts */, "mm_seninf5_sel",
		mm_seninf5_parents/* parent */, CKSYS2_CLK_CFG_5, CKSYS2_CLK_CFG_5_SET,
		CKSYS2_CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF5_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 9/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 9/* fenc shift */),
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_MMINFRA_SNOC_SLOW_SEL/* dts */, "mm_mminfra_snoc_slow_sel",
		mm_mminfra_snoc_slow_parents/* parent */, CKSYS2_CLK_CFG_5, CKSYS2_CLK_CFG_5_SET,
		CKSYS2_CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMINFRA_SNOC_SLOW_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 8/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 8/* fenc shift */),
#else
	/* CKSYS2_CLK_CFG_0 */
	MUX_FENC_CLR_SET_UPD_CHK(CKSYS_MM_MMUP_SEL/* dts */, "mm_mmup_sel",
		mm_mmup_parents/* parent */, CKSYS2_CLK_CFG_0, CKSYS2_CLK_CFG_0_SET,
		CKSYS2_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMUP_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 31/* cksta shift */,
		CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */, 31/* fenc shift */),
	MUX_GENERIC_HWV_AL(CKSYS_MM_MMINFRA_AO_SEL/* dts */, "mm_mminfra_ao_sel",
		mm_mminfra_ao_parents/* parent */, CKSYS2_CLK_CFG_0,
		CKSYS2_CLK_CFG_0_SET, CKSYS2_CLK_CFG_0_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		8/* lsb */, 2/* width */,
		12/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MMINFRA_AO_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		30/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		30/* fenc shift */),
	MUX_GENERIC_HWV_AL(CKSYS_MM_MMINFRA_SEL/* dts */, "mm_mminfra_sel",
		mm_mminfra_parents/* parent */, CKSYS2_CLK_CFG_0,
		CKSYS2_CLK_CFG_0_SET, CKSYS2_CLK_CFG_0_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		16/* lsb */, 4/* width */,
		10/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MMINFRA_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		29/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		29/* fenc shift */),
	MUX_GENERIC_HWV_AL(CKSYS_MM_MMINFRA_SNOC_SEL/* dts */, "mm_mminfra_snoc_sel",
		mm_mminfra_snoc_parents/* parent */, CKSYS2_CLK_CFG_0,
		CKSYS2_CLK_CFG_0_SET, CKSYS2_CLK_CFG_0_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		24/* lsb */, 4/* width */,
		11/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MMINFRA_SNOC_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		28/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		28/* fenc shift */),
	/* CKSYS2_CLK_CFG_1 */
	MUX_GENERIC_HWV_AL(CKSYS_MM_VENC_SEL/* dts */, "mm_venc_sel",
		mm_venc_parents/* parent */, CKSYS2_CLK_CFG_1,
		CKSYS2_CLK_CFG_1_SET, CKSYS2_CLK_CFG_1_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		0/* lsb */, 4/* width */,
		6/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_VENC_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		27/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		27/* fenc shift */),
	MUX_GENERIC_HWV_AL(CKSYS_MM_VENC_MDP_SEL/* dts */, "mm_venc_mdp_sel",
		mm_venc_mdp_parents/* parent */, CKSYS2_CLK_CFG_1,
		CKSYS2_CLK_CFG_1_SET, CKSYS2_CLK_CFG_1_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		8/* lsb */, 4/* width */,
		9/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_VENC_MDP_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		26/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		26/* fenc shift */),
	MUX_GENERIC_HWV_AL(CKSYS_MM_VDEC_SEL/* dts */, "mm_vdec_sel",
		mm_vdec_parents/* parent */, CKSYS2_CLK_CFG_1,
		CKSYS2_CLK_CFG_1_SET, CKSYS2_CLK_CFG_1_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		16/* lsb */, 4/* width */,
		4/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_VDEC_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		25/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		25/* fenc shift */),
	MUX_GENERIC_HWV_AL(CKSYS_MM_IMG1_SEL/* dts */, "mm_img1_sel",
		mm_img1_parents/* parent */, CKSYS2_CLK_CFG_1,
		CKSYS2_CLK_CFG_1_SET, CKSYS2_CLK_CFG_1_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		24/* lsb */, 4/* width */,
		0/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_IMG1_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		24/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		24/* fenc shift */),
	/* CKSYS2_CLK_CFG_2 */
	MUX_GENERIC_HWV_AL(CKSYS_MM_IPE_SEL/* dts */, "mm_ipe_sel",
		mm_ipe_parents/* parent */, CKSYS2_CLK_CFG_2,
		CKSYS2_CLK_CFG_2_SET, CKSYS2_CLK_CFG_2_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		0/* lsb */, 4/* width */,
		1/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_IPE_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		23/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		23/* fenc shift */),
	MUX_GENERIC_HWV_AL(CKSYS_MM_DISP_SEL/* dts */, "mm_disp_sel",
		mm_disp_parents/* parent */, CKSYS2_CLK_CFG_2,
		CKSYS2_CLK_CFG_2_SET, CKSYS2_CLK_CFG_2_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		8/* lsb */, 4/* width */,
		7/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DISP_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		22/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		22/* fenc shift */),
	MUX_GENERIC_HWV_AL(CKSYS_MM_MML_SEL/* dts */, "mm_mml_sel",
		mm_mml_parents/* parent */, CKSYS2_CLK_CFG_2,
		CKSYS2_CLK_CFG_2_SET, CKSYS2_CLK_CFG_2_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		16/* lsb */, 4/* width */,
		8/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MML_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		21/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		21/* fenc shift */),
	MUX_GENERIC_HWV_AL(CKSYS_MM_DVO_DP_SEL/* dts */, "mm_dvo_dp_sel",
		mm_dvo_dp_parents/* parent */, CKSYS2_CLK_CFG_2,
		CKSYS2_CLK_CFG_2_SET, CKSYS2_CLK_CFG_2_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		24/* lsb */, 3/* width */,
		14/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DVO_DP_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		20/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		20/* fenc shift */),
	/* CKSYS2_CLK_CFG_3 */
	MUX_GENERIC_HWV(CKSYS_MM_DVO_FAVT_DP_SEL/* dts */, "mm_dvo_favt_dp_sel",
		mm_dvo_favt_dp_parents/* parent */, CKSYS2_CLK_CFG_3,
		CKSYS2_CLK_CFG_3_SET, CKSYS2_CLK_CFG_3_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		0/* lsb */, 3/* width */,
		15/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DVO_FAVT_DP_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		19/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		19/* fenc shift */),
	MUX_GENERIC_HWV_AL(CKSYS_MM_CAM_SEL/* dts */, "mm_cam_sel",
		mm_cam_parents/* parent */, CKSYS2_CLK_CFG_3,
		CKSYS2_CLK_CFG_3_SET, CKSYS2_CLK_CFG_3_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		8/* lsb */, 4/* width */,
		2/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAM_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		18/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		18/* fenc shift */),
	MUX_GENERIC_HWV(CKSYS_MM_CAMTM_SEL/* dts */, "mm_camtm_sel",
		mm_camtm_parents/* parent */, CKSYS2_CLK_CFG_3,
		CKSYS2_CLK_CFG_3_SET, CKSYS2_CLK_CFG_3_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		16/* lsb */, 3/* width */,
		3/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTM_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		17/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		17/* fenc shift */),
	MUX_GENERIC_HWV_AL(CKSYS_MM_CCUSYS_SEL/* dts */, "mm_ccusys_sel",
		mm_ccusys_parents/* parent */, CKSYS2_CLK_CFG_3,
		CKSYS2_CLK_CFG_3_SET, CKSYS2_CLK_CFG_3_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		24/* lsb */, 3/* width */,
		5/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CCUSYS_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		16/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		16/* fenc shift */),
	/* CKSYS2_CLK_CFG_4 */
	MUX_GENERIC_HWV_AL(CKSYS_MM_CCUTM_SEL/* dts */, "mm_ccutm_sel",
		mm_ccutm_parents/* parent */, CKSYS2_CLK_CFG_4,
		CKSYS2_CLK_CFG_4_SET, CKSYS2_CLK_CFG_4_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		0/* lsb */, 3/* width */,
		16/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CCUTM_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		15/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		15/* fenc shift */),
	MUX_GENERIC_HWV(CKSYS_MM_SENINF0_SEL/* dts */, "mm_seninf0_sel",
		mm_seninf0_parents/* parent */, CKSYS2_CLK_CFG_4,
		CKSYS2_CLK_CFG_4_SET, CKSYS2_CLK_CFG_4_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		8/* lsb */, 4/* width */,
		17/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF0_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		14/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		14/* fenc shift */),
	MUX_GENERIC_HWV(CKSYS_MM_SENINF1_SEL/* dts */, "mm_seninf1_sel",
		mm_seninf1_parents/* parent */, CKSYS2_CLK_CFG_4,
		CKSYS2_CLK_CFG_4_SET, CKSYS2_CLK_CFG_4_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		16/* lsb */, 4/* width */,
		18/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF1_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		13/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		13/* fenc shift */),
	MUX_GENERIC_HWV(CKSYS_MM_SENINF2_SEL/* dts */, "mm_seninf2_sel",
		mm_seninf2_parents/* parent */, CKSYS2_CLK_CFG_4,
		CKSYS2_CLK_CFG_4_SET, CKSYS2_CLK_CFG_4_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		24/* lsb */, 4/* width */,
		19/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF2_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		12/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		12/* fenc shift */),
	/* CKSYS2_CLK_CFG_5 */
	MUX_GENERIC_HWV(CKSYS_MM_SENINF3_SEL/* dts */, "mm_seninf3_sel",
		mm_seninf3_parents/* parent */, CKSYS2_CLK_CFG_5,
		CKSYS2_CLK_CFG_5_SET, CKSYS2_CLK_CFG_5_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		0/* lsb */, 4/* width */,
		20/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF3_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		11/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		11/* fenc shift */),
	MUX_GENERIC_HWV(CKSYS_MM_SENINF4_SEL/* dts */, "mm_seninf4_sel",
		mm_seninf4_parents/* parent */, CKSYS2_CLK_CFG_5,
		CKSYS2_CLK_CFG_5_SET, CKSYS2_CLK_CFG_5_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		8/* lsb */, 4/* width */,
		21/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF4_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		10/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		10/* fenc shift */),
	MUX_GENERIC_HWV(CKSYS_MM_SENINF5_SEL/* dts */, "mm_seninf5_sel",
		mm_seninf5_parents/* parent */, CKSYS2_CLK_CFG_5,
		CKSYS2_CLK_CFG_5_SET, CKSYS2_CLK_CFG_5_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		16/* lsb */, 4/* width */,
		22/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF5_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		9/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		9/* fenc shift */),
	MUX_GENERIC_HWV_AL(CKSYS_MM_MMINFRA_SNOC_SLOW_SEL/* dts */, "mm_mminfra_snoc_slow_sel",
		mm_mminfra_snoc_slow_parents/* parent */, CKSYS2_CLK_CFG_5,
		CKSYS2_CLK_CFG_5_SET, CKSYS2_CLK_CFG_5_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_DONE,
		MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_SET, MM_HW_CCF_HW_CCF_MUX_PWR_GRP_0_CLR, /* hwv */
		24/* lsb */, 4/* width */,
		23/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MMINFRA_SNOC_SLOW_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		8/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		8/* fenc shift */),
#endif
};

static const struct mtk_composite cksys_top_composites[] = {
	/* CLK_AUDDIV_0 */
	MUX(CKSYS_TOP_APLL_I2SIN0_MCK_SEL/* dts */, "cksys_apll_i2sin0_m_sel",
		cksys_apll_i2sin0_m_parents/* parent */, 0x0800/* ofs */,
		16/* lsb */, 1/* width */),
	MUX(CKSYS_TOP_APLL_I2SIN1_MCK_SEL/* dts */, "cksys_apll_i2sin1_m_sel",
		cksys_apll_i2sin1_m_parents/* parent */, 0x0800/* ofs */,
		17/* lsb */, 1/* width */),
	MUX(CKSYS_TOP_APLL_I2SIN2_MCK_SEL/* dts */, "cksys_apll_i2sin2_m_sel",
		cksys_apll_i2sin2_m_parents/* parent */, 0x0800/* ofs */,
		18/* lsb */, 1/* width */),
	MUX(CKSYS_TOP_APLL_I2SIN3_MCK_SEL/* dts */, "cksys_apll_i2sin3_m_sel",
		cksys_apll_i2sin3_m_parents/* parent */, 0x0800/* ofs */,
		19/* lsb */, 1/* width */),
	MUX(CKSYS_TOP_APLL_I2SIN4_MCK_SEL/* dts */, "cksys_apll_i2sin4_m_sel",
		cksys_apll_i2sin4_m_parents/* parent */, 0x0800/* ofs */,
		20/* lsb */, 1/* width */),
	MUX(CKSYS_TOP_APLL_I2SIN6_MCK_SEL/* dts */, "cksys_apll_i2sin6_m_sel",
		cksys_apll_i2sin6_m_parents/* parent */, 0x0800/* ofs */,
		21/* lsb */, 1/* width */),
	MUX(CKSYS_TOP_APLL_I2SOUT0_MCK_SEL/* dts */, "cksys_apll_i2sout0_m_sel",
		cksys_apll_i2sout0_m_parents/* parent */, 0x0800/* ofs */,
		22/* lsb */, 1/* width */),
	MUX(CKSYS_TOP_APLL_I2SOUT1_MCK_SEL/* dts */, "cksys_apll_i2sout1_m_sel",
		cksys_apll_i2sout1_m_parents/* parent */, 0x0800/* ofs */,
		23/* lsb */, 1/* width */),
	MUX(CKSYS_TOP_APLL_I2SOUT2_MCK_SEL/* dts */, "cksys_apll_i2sout2_m_sel",
		cksys_apll_i2sout2_m_parents/* parent */, 0x0800/* ofs */,
		24/* lsb */, 1/* width */),
	MUX(CKSYS_TOP_APLL_I2SOUT3_MCK_SEL/* dts */, "cksys_apll_i2sout3_m_sel",
		cksys_apll_i2sout3_m_parents/* parent */, 0x0800/* ofs */,
		25/* lsb */, 1/* width */),
	MUX(CKSYS_TOP_APLL_I2SOUT4_MCK_SEL/* dts */, "cksys_apll_i2sout4_m_sel",
		cksys_apll_i2sout4_m_parents/* parent */, 0x0800/* ofs */,
		26/* lsb */, 1/* width */),
	MUX(CKSYS_TOP_APLL_I2SOUT6_MCK_SEL/* dts */, "cksys_apll_i2sout6_m_sel",
		cksys_apll_i2sout6_m_parents/* parent */, 0x0800/* ofs */,
		27/* lsb */, 1/* width */),
	MUX(CKSYS_TOP_APLL_FMI2S_MCK_SEL/* dts */, "cksys_apll_fmi2s_m_sel",
		cksys_apll_fmi2s_m_parents/* parent */, 0x0800/* ofs */,
		28/* lsb */, 1/* width */),
	MUX(CKSYS_TOP_APLL_TDMOUT_MCK_SEL/* dts */, "cksys_apll_tdmout_m_sel",
		cksys_apll_tdmout_m_parents/* parent */, 0x0800/* ofs */,
		29/* lsb */, 1/* width */),
	/* CLK_AUDDIV_2 */
	DIV_GATE(CKSYS_TOP_APLL12_CK_DIV_I2SIN0/* dts */, "cksys_apll12_div_i2sin0"/* ccf */,
		"cksys_apll_i2sin0_m_sel"/* parent */, 0x0800/* pdn ofs */,
		0/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CKSYS_TOP_APLL12_CK_DIV_I2SIN1/* dts */, "cksys_apll12_div_i2sin1"/* ccf */,
		"cksys_apll_i2sin1_m_sel"/* parent */, 0x0800/* pdn ofs */,
		1/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CKSYS_TOP_APLL12_CK_DIV_I2SIN2/* dts */, "cksys_apll12_div_i2sin2"/* ccf */,
		"cksys_apll_i2sin2_m_sel"/* parent */, 0x0800/* pdn ofs */,
		2/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		16/* lsb */),
	DIV_GATE(CKSYS_TOP_APLL12_CK_DIV_I2SIN3/* dts */, "cksys_apll12_div_i2sin3"/* ccf */,
		"cksys_apll_i2sin3_m_sel"/* parent */, 0x0800/* pdn ofs */,
		3/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		24/* lsb */),
	/* CLK_AUDDIV_3 */
	DIV_GATE(CKSYS_TOP_APLL12_CK_DIV_I2SIN4/* dts */, "cksys_apll12_div_i2sin4"/* ccf */,
		"cksys_apll_i2sin4_m_sel"/* parent */, 0x0800/* pdn ofs */,
		4/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CKSYS_TOP_APLL12_CK_DIV_I2SIN6/* dts */, "cksys_apll12_div_i2sin6"/* ccf */,
		"cksys_apll_i2sin6_m_sel"/* parent */, 0x0800/* pdn ofs */,
		5/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CKSYS_TOP_APLL12_CK_DIV_I2SOUT0/* dts */, "cksys_apll12_div_i2sout0"/* ccf */,
		"cksys_apll_i2sout0_m_sel"/* parent */, 0x0800/* pdn ofs */,
		6/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		16/* lsb */),
	DIV_GATE(CKSYS_TOP_APLL12_CK_DIV_I2SOUT1/* dts */, "cksys_apll12_div_i2sout1"/* ccf */,
		"cksys_apll_i2sout1_m_sel"/* parent */, 0x0800/* pdn ofs */,
		7/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		24/* lsb */),
	/* CLK_AUDDIV_4 */
	DIV_GATE(CKSYS_TOP_APLL12_CK_DIV_I2SOUT2/* dts */, "cksys_apll12_div_i2sout2"/* ccf */,
		"cksys_apll_i2sout2_m_sel"/* parent */, 0x0800/* pdn ofs */,
		8/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CKSYS_TOP_APLL12_CK_DIV_I2SOUT3/* dts */, "cksys_apll12_div_i2sout3"/* ccf */,
		"cksys_apll_i2sout3_m_sel"/* parent */, 0x0800/* pdn ofs */,
		9/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CKSYS_TOP_APLL12_CK_DIV_I2SOUT4/* dts */, "cksys_apll12_div_i2sout4"/* ccf */,
		"cksys_apll_i2sout4_m_sel"/* parent */, 0x0800/* pdn ofs */,
		10/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		16/* lsb */),
	DIV_GATE(CKSYS_TOP_APLL12_CK_DIV_I2SOUT6/* dts */, "cksys_apll12_div_i2sout6"/* ccf */,
		"cksys_apll_i2sout6_m_sel"/* parent */, 0x0800/* pdn ofs */,
		11/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		24/* lsb */),
	/* CLK_AUDDIV_5 */
	DIV_GATE(CKSYS_TOP_APLL12_CK_DIV_FMI2S/* dts */, "cksys_apll12_div_fmi2s"/* ccf */,
		"cksys_apll_fmi2s_m_sel"/* parent */, 0x0800/* pdn ofs */,
		12/* pdn bit */, CLK_AUDDIV_5/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CKSYS_TOP_APLL12_CK_DIV_TDMOUT_M/* dts */, "cksys_apll12_div_tdmout_m"/* ccf */,
		"cksys_apll_tdmout_m_sel"/* parent */, 0x0800/* pdn ofs */,
		13/* pdn bit */, CLK_AUDDIV_5/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CKSYS_TOP_APLL12_CK_DIV_TDMOUT_B/* dts */, "cksys_apll12_div_tdmout_b"/* ccf */,
		"cksys_apll_tdmout_m_sel"/* parent */, 0x0800/* pdn ofs */,
		14/* pdn bit */, CLK_AUDDIV_5/* ofs */, 8/* width */,
		16/* lsb */),
};


enum subsys_id {
	TVDPLL_CTRL = 0,
	UNIVPLL2_CTRL = 1,
	IMGPLL_CTRL = 2,
	MMPLL_CTRL = 3,
	MAINPLL_CTRL = 4,
	MSDCPLL_CTRL = 5,
	APLL1_CTRL = 6,
	APLL2_CTRL = 7,
	EMIPLL_CTRL = 8,
	MAINPLL2_CTRL = 9,
	UNIVPLL_CTRL = 10,
	PLL_SYS_NUM,
};

static const struct mtk_pll_data *plls_data[PLL_SYS_NUM];
static void __iomem *plls_base[PLL_SYS_NUM];

#define MT6993_PLL_FMAX		(3800UL * MHZ)
#define MT6993_PLL_FMIN		(1500UL * MHZ)
#define MT6993_INTEGER_BITS	8

#if MT_CCF_PLL_DISABLE
#define PLL_CFLAGS		PLL_AO
#else
#define PLL_CFLAGS		(0)
#endif

#define PLL(_id, _name, _reg, _en_reg, _en_mask, _pll_en_bit,		\
			_flags, _rst_bar_mask,				\
			_pd_reg, _pd_shift, _tuner_reg,			\
			_tuner_en_reg, _tuner_en_bit,			\
			_pcw_reg, _pcw_shift, _pcwbits) {		\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.en_reg = _en_reg,					\
		.en_mask = _en_mask,					\
		.pll_en_bit = _pll_en_bit,				\
		.flags = (_flags | PLL_CFLAGS),				\
		.rst_bar_mask = _rst_bar_mask,				\
		.fmax = MT6993_PLL_FMAX,				\
		.fmin = MT6993_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,			\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6993_INTEGER_BITS,			\
		.ops = &mtk_pll_ops,					\
	}

#define PLL_FENC(_id, _name, _reg, _fenc_sta_ofs,		\
		_fenc_sta_bit, _flags, _pd_reg, _pd_shift,		\
		_pcw_reg, _pcw_shift, _pcwbits) {		\
		.id = (_id),						\
		.name = (_name),						\
		.reg = (_reg),						\
		.fenc_sta_ofs = (_fenc_sta_ofs),				\
		.fenc_sta_bit = (_fenc_sta_bit),				\
		.flags = (_flags | PLL_CFLAGS),				\
		.fmax = MT6993_PLL_FMAX,				\
		.fmin = MT6993_PLL_FMIN,				\
		.pd_reg = (_pd_reg),					\
		.pd_shift = (_pd_shift),					\
		.pcw_reg = (_pcw_reg),					\
		.pcw_shift = (_pcw_shift),				\
		.pcwbits = (_pcwbits),					\
		.pcwibits = MT6993_INTEGER_BITS,			\
		.ops = (&mtk_pll_fenc_ops),				\
	}

static const struct mtk_pll_data mmpll_ctrl_plls[] = {
	PLL_FENC(MMPLL_CTRL_MMPLL, "mmpll", MMPLL_CON0,
	0x0034/*fenc*/, 4, 0,
		MMPLL_CON1, 24/*pd*/,
				MMPLL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data mainpll2_ctrl_plls[] = {
	PLL_FENC(MAINPLL2_CTRL_MAINPLL2, "mainpll2", MAINPLL2_CON0,
	0x0034/*fenc*/, 4, 0,
		MAINPLL2_CON1, 24/*pd*/,
				MAINPLL2_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data tvdpll_ctrl_plls[] = {
	PLL_FENC(TVDPLL_CTRL_TVDPLL, "tvdpll", TVDPLL_CON0,
	0x0034/*fenc*/, 4, 0,
		TVDPLL_CON1, 24/*pd*/,
				TVDPLL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data univpll2_ctrl_plls[] = {
	PLL_FENC(UNIVPLL2_CTRL_UNIVPLL2, "univpll2", UNIVPLL2_CON0,
	0x0034/*fenc*/, 4, 0,
		UNIVPLL2_CON1, 24/*pd*/,
				UNIVPLL2_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data univpll_ctrl_plls[] = {
	PLL_FENC(UNIVPLL_CTRL_UNIVPLL, "univpll", UNIVPLL_CON0,
	0x0034/*fenc*/, 4, 0,
		UNIVPLL_CON1, 24/*pd*/,
				UNIVPLL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data imgpll_ctrl_plls[] = {
	PLL_FENC(IMGPLL_CTRL_IMGPLL, "imgpll", IMGPLL_CON0,
	0x0034/*fenc*/, 4, 0,
		IMGPLL_CON1, 24/*pd*/,
				IMGPLL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data msdcpll_ctrl_plls[] = {
	PLL_FENC(MSDCPLL_CTRL_MSDCPLL, "msdcpll", MSDCPLL_CON0,
	0x0034/*fenc*/, 4, 0,
		MSDCPLL_CON1, 24/*pd*/,
				MSDCPLL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data apll1_ctrl_plls[] = {
	PLL_FENC(APLL1_CTRL_APLL1, "apll1", APLL1_CON0,
	0x0034/*fenc*/, 4, 0,
		APLL1_CON1, 24/*pd*/,
				APLL1_PCW_CON0, 0, 32/*pcw*/),
};

static const struct mtk_pll_data apll2_ctrl_plls[] = {
	PLL_FENC(APLL2_CTRL_APLL2, "apll2", APLL2_CON0,
	0x0034/*fenc*/, 4, 0,
		APLL2_CON1, 24/*pd*/,
				APLL2_PCW_CON0, 0, 32/*pcw*/),
};

static const struct mtk_pll_data mainpll_ctrl_plls[] = {
	PLL_FENC(MAINPLL_CTRL_MAINPLL, "mainpll", MAINPLL_CON0,
	0x0034/*fenc*/, 4, 0,
		MAINPLL_CON1, 24/*pd*/,
				MAINPLL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data emipll_ctrl_plls[] = {
	PLL_FENC(EMIPLL_CTRL_EMIPLL, "emipll", EMIPLL_CON0,
	0x0034/*fenc*/, 4, 0,
		EMIPLL_CON1, 24/*pd*/,
				EMIPLL_CON1, 0, 22/*pcw*/),
};

static int clk_mt6993_pll_registration(enum subsys_id id,
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
		base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
		if (IS_ERR(base)) {
			pr_err("%s(): ioremap failed\n", __func__);
			return PTR_ERR(base);
		}
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

static int clk_mt6993_apll1_ctrl_probe(struct platform_device *pdev)
{
	return clk_mt6993_pll_registration(APLL1_CTRL, apll1_ctrl_plls,
			pdev, ARRAY_SIZE(apll1_ctrl_plls));
}

static int clk_mt6993_apll2_ctrl_probe(struct platform_device *pdev)
{
	return clk_mt6993_pll_registration(APLL2_CTRL, apll2_ctrl_plls,
			pdev, ARRAY_SIZE(apll2_ctrl_plls));
}

static int clk_mt6993_cksys_mm_probe(struct platform_device *pdev)
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
		base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
		if (IS_ERR(base)) {
			pr_err("%s(): ioremap failed\n", __func__);
			return PTR_ERR(base);
		}
	}

	clk_data = mtk_alloc_clk_data(CLK_CKSYS_MM_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_factors(cksys_mm_divs, ARRAY_SIZE(cksys_mm_divs), clk_data);

	mtk_clk_register_muxes(cksys_mm_muxes, ARRAY_SIZE(cksys_mm_muxes), node, &mt6993_clk_lock, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6993_cksys_top_probe(struct platform_device *pdev)
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
		base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
		if (IS_ERR(base)) {
			pr_err("%s(): ioremap failed\n", __func__);
			return PTR_ERR(base);
		}
	}

	clk_data = mtk_alloc_clk_data(CLK_CKSYS_TOP_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_factors(cksys_top_divs, ARRAY_SIZE(cksys_top_divs),
			clk_data);

	mtk_clk_register_muxes(cksys_top_muxes, ARRAY_SIZE(cksys_top_muxes), node,
			&mt6993_clk_lock, clk_data);

	mtk_clk_register_composites(cksys_top_composites, ARRAY_SIZE(cksys_top_composites),
			base, &mt6993_clk_lock, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6993_cksys_vlp_probe(struct platform_device *pdev)
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
		base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
		if (IS_ERR(base)) {
			pr_err("%s(): ioremap failed\n", __func__);
			return PTR_ERR(base);
		}
	}

	clk_data = mtk_alloc_clk_data(CLK_CKSYS_VLP_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_factors(cksys_vlp_divs, ARRAY_SIZE(cksys_vlp_divs),
			clk_data);

	mtk_clk_register_muxes(cksys_vlp_muxes, ARRAY_SIZE(cksys_vlp_muxes), node,
			&mt6993_clk_lock, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6993_emipll_ctrl_probe(struct platform_device *pdev)
{
	return clk_mt6993_pll_registration(EMIPLL_CTRL, emipll_ctrl_plls,
			pdev, ARRAY_SIZE(emipll_ctrl_plls));
}

static int clk_mt6993_imgpll_ctrl_probe(struct platform_device *pdev)
{
	return clk_mt6993_pll_registration(IMGPLL_CTRL, imgpll_ctrl_plls,
			pdev, ARRAY_SIZE(imgpll_ctrl_plls));
}

static int clk_mt6993_mainpll2_ctrl_probe(struct platform_device *pdev)
{
	return clk_mt6993_pll_registration(MAINPLL2_CTRL, mainpll2_ctrl_plls,
			pdev, ARRAY_SIZE(mainpll2_ctrl_plls));
}

static int clk_mt6993_mainpll_ctrl_probe(struct platform_device *pdev)
{
	return clk_mt6993_pll_registration(MAINPLL_CTRL, mainpll_ctrl_plls,
			pdev, ARRAY_SIZE(mainpll_ctrl_plls));
}

static int clk_mt6993_mmpll_ctrl_probe(struct platform_device *pdev)
{
	return clk_mt6993_pll_registration(MMPLL_CTRL, mmpll_ctrl_plls,
			pdev, ARRAY_SIZE(mmpll_ctrl_plls));
}

static int clk_mt6993_msdcpll_ctrl_probe(struct platform_device *pdev)
{
	return clk_mt6993_pll_registration(MSDCPLL_CTRL, msdcpll_ctrl_plls,
			pdev, ARRAY_SIZE(msdcpll_ctrl_plls));
}

static int clk_mt6993_tvdpll_ctrl_probe(struct platform_device *pdev)
{
	return clk_mt6993_pll_registration(TVDPLL_CTRL, tvdpll_ctrl_plls,
			pdev, ARRAY_SIZE(tvdpll_ctrl_plls));
}

static int clk_mt6993_univpll2_ctrl_probe(struct platform_device *pdev)
{
	return clk_mt6993_pll_registration(UNIVPLL2_CTRL, univpll2_ctrl_plls,
			pdev, ARRAY_SIZE(univpll2_ctrl_plls));
}

static int clk_mt6993_univpll_ctrl_probe(struct platform_device *pdev)
{
	return clk_mt6993_pll_registration(UNIVPLL_CTRL, univpll_ctrl_plls,
			pdev, ARRAY_SIZE(univpll_ctrl_plls));
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

void mt6993_pll_force_off(void)
{
	int i;

	for (i = 0; i < PLL_SYS_NUM; i++)
		pll_force_off_internal(plls_data[i], plls_base[i]);
}
EXPORT_SYMBOL_GPL(mt6993_pll_force_off);

static const struct of_device_id of_match_clk_mt6993[] = {
#if IS_ENABLED(CONFIG_MTK_CLKMGR_FTRACE)
    {
        .compatible = "clk-ftrace",
        .data = clk_set_trace_event,
    },
#endif
	{
		.compatible = "mediatek,mt6993-apll1_ctrl",
		.data = clk_mt6993_apll1_ctrl_probe,
	}, {
		.compatible = "mediatek,mt6993-apll2_ctrl",
		.data = clk_mt6993_apll2_ctrl_probe,
		}, {
		.compatible = "mediatek,mt6993-cksys_mm",
		.data = clk_mt6993_cksys_mm_probe,
	}, {
		.compatible = "mediatek,mt6993-cksys_top",
		.data = clk_mt6993_cksys_top_probe,
	}, {
		.compatible = "mediatek,mt6993-cksys_vlp",
		.data = clk_mt6993_cksys_vlp_probe,
	}, {
		.compatible = "mediatek,mt6993-emipll_ctrl",
		.data = clk_mt6993_emipll_ctrl_probe,
	}, {
		.compatible = "mediatek,mt6993-imgpll_ctrl",
		.data = clk_mt6993_imgpll_ctrl_probe,
	}, {
		.compatible = "mediatek,mt6993-mainpll2_ctrl",
		.data = clk_mt6993_mainpll2_ctrl_probe,
	}, {
		.compatible = "mediatek,mt6993-mainpll_ctrl",
		.data = clk_mt6993_mainpll_ctrl_probe,
	}, {
		.compatible = "mediatek,mt6993-mmpll_ctrl",
		.data = clk_mt6993_mmpll_ctrl_probe,
	}, {
		.compatible = "mediatek,mt6993-msdcpll_ctrl",
		.data = clk_mt6993_msdcpll_ctrl_probe,
	}, {
		.compatible = "mediatek,mt6993-tvdpll_ctrl",
		.data = clk_mt6993_tvdpll_ctrl_probe,
	}, {
		.compatible = "mediatek,mt6993-univpll2_ctrl",
		.data = clk_mt6993_univpll2_ctrl_probe,
	}, {
		.compatible = "mediatek,mt6993-univpll_ctrl",
		.data = clk_mt6993_univpll_ctrl_probe,
	}, {
		/* sentinel */
	}
};

static int clk_mt6993_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6993_drv = {
	.probe = clk_mt6993_probe,
	.driver = {
		.name = "clk-mt6993",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6993,
	},
};

module_platform_driver(clk_mt6993_drv);
MODULE_LICENSE("GPL");

