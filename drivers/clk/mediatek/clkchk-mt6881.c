// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Claude Yen <claude.yen@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>

#include <dt-bindings/power/mt6881-power.h>

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
#include <linux/soc/mediatek/devapc_public.h>
#endif

#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER)
#include <mt-plat/dvfsrc-exp.h>
#endif

#ifdef CONFIG_MTK_SERROR_HOOK
#include <trace/hooks/traps.h>
#endif

#include "clkchk.h"
#include "clkchk-mt6881.h"
#include "clk-fmeter.h"
#include "clk-mt6881-fmeter.h"

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
#include "vcp_status.h"
#endif

#define BUG_ON_CHK_ENABLE		0
#define CHECK_VCORE_FREQ		0
#define CG_CHK_PWRON_ENABLE		0

#define HWV_INT_PLL_TRIGGER		0x0004
#define HWV_INT_CG_TRIGGER		0x10001

#define HWV_DOMAIN_KEY			0x055C
#define HWV_IRQ_STATUS			0x1500
#define HWV_CG_SET(xpu, id)		((0x0200 * (xpu)) + (id * 0x8))
#define HWV_CG_STA(id)			(0x1800 + (id * 0x4))
#define HWV_CG_EN(id)			(0x1900 + (id * 0x4))
#define HWV_CG_XPU_DONE(xpu)		(0x1B00 + (xpu * 0x8))
#define HWV_CG_DONE(id)			(0x1C00 + (id * 0x4))

#define EVT_LEN				40
#define CLK_ID_SHIFT			8
#define CLK_STA_SHIFT			0

static DEFINE_SPINLOCK(clk_trace_lock);
static unsigned int clk_event[EVT_LEN];
static unsigned int evt_cnt, suspend_cnt;
static bool clkchk_bug_on_flag = true;

/* trace all subsys cgs */
enum {
	CLK_AFE_DL1_DAC_TML_CG = 0,
	CLK_AFE_DL1_DAC_HIRES_CG = 1,
	CLK_AFE_DL1_DAC_CG = 2,
	CLK_AFE_DL1_PREDIS_CG = 3,
	CLK_AFE_DL1_NLE_CG = 4,
	CLK_AFE_DL0_DAC_TML_CG = 5,
	CLK_AFE_DL0_DAC_HIRES_CG = 6,
	CLK_AFE_DL0_DAC_CG = 7,
	CLK_AFE_DL0_PREDIS_CG = 8,
	CLK_AFE_DL0_NLE_CG = 9,
	CLK_AFE_PCM1_CG = 10,
	CLK_AFE_PCM0_CG = 11,
	CLK_AFE_CM2_CG = 12,
	CLK_AFE_CM1_CG = 13,
	CLK_AFE_CM0_CG = 14,
	CLK_AFE_STF_CG = 15,
	CLK_AFE_HW_GAIN23_CG = 16,
	CLK_AFE_HW_GAIN01_CG = 17,
	CLK_AFE_FM_I2S_CG = 18,
	CLK_AFE_MTKAIFV4_CG = 19,
	CLK_AFE_AUDIO_HOPPING_CG = 20,
	CLK_AFE_AUDIO_F26M_CG = 21,
	CLK_AFE_APLL1_CG = 22,
	CLK_AFE_APLL2_CG = 23,
	CLK_AFE_H208M_CG = 24,
	CLK_AFE_APLL_TUNER2_CG = 25,
	CLK_AFE_APLL_TUNER1_CG = 26,
	CLK_AFE_AUD_PAD_TOP_MOSI_EN_CG = 27,
	CLK_AFE_ETDM6_PADTOP_CK_EN_CG = 28,
	CLK_AFE_ETDM7_PADTOP_CK_EN_CG = 29,
	CLK_AFE_UL1_ADC_HIRES_TML_CG = 30,
	CLK_AFE_UL1_ADC_HIRES_CG = 31,
	CLK_AFE_UL1_TML_CG = 32,
	CLK_AFE_UL1_ADC_CG = 33,
	CLK_AFE_UL0_ADC_HIRES_TML_CG = 34,
	CLK_AFE_UL0_ADC_HIRES_CG = 35,
	CLK_AFE_UL0_TML_CG = 36,
	CLK_AFE_UL0_ADC_CG = 37,
	CLK_AFE_ETDM_IN_DMA0_CG = 38,
	CLK_AFE_ETDM_IN6_CG = 39,
	CLK_AFE_ETDM_IN2_CG = 40,
	CLK_AFE_ETDM_IN1_CG = 41,
	CLK_AFE_ETDM_IN0_CG = 42,
	CLK_AFE_ETDM_OUT6_CG = 43,
	CLK_AFE_ETDM_OUT2_CG = 44,
	CLK_AFE_ETDM_OUT1_CG = 45,
	CLK_AFE_ETDM_OUT0_CG = 46,
	CLK_AFE_GENERAL7_ASRC_CG = 47,
	CLK_AFE_GENERAL6_ASRC_CG = 48,
	CLK_AFE_GENERAL5_ASRC_CG = 49,
	CLK_AFE_GENERAL4_ASRC_CG = 50,
	CLK_AFE_GENERAL3_ASRC_CG = 51,
	CLK_AFE_GENERAL2_ASRC_CG = 52,
	CLK_AFE_GENERAL1_ASRC_CG = 53,
	CLK_AFE_GENERAL0_ASRC_CG = 54,
	CLK_AFE_CONNSYS_I2S_ASRC_CG = 55,
	CLK_CAM_MR_LARB13_CG = 56,
	CLK_CAM_MR_LARB14_CG = 57,
	CLK_CAM_MR_LARB19_CG = 58,
	CLK_CAM_MR_LARB25_CG = 59,
	CLK_CAM_MR_LARB26_CG = 60,
	CLK_CAM_MR_LARB29_CG = 61,
	CLK_CAM_MR_SENINF_CAMTM_CG = 62,
	CLK_CAM_MR_CAMSV_TOP_CG = 63,
	CLK_CAM_MR_CAMSV_A_CG = 64,
	CLK_CAM_MR_CAMSV_B_CG = 65,
	CLK_CAM_MR_CAMSV_C_CG = 66,
	CLK_CAM_MR_CAMSV_D_CG = 67,
	CLK_CAM_MR_CAMSV_E_CG = 68,
	CLK_CAM_MR_CAMSV_CG = 69,
	CLK_CAM_MR_PDA0_CG = 70,
	CLK_CAM_MR_PDA1_CG = 71,
	CLK_CAM_MR_FAKE_ENG_CG = 72,
	CLK_CAM_RA_LARBX_CG = 73,
	CLK_CAM_RA_CAM_CG = 74,
	CLK_CAM_RA_CAMTG_CG = 75,
	CLK_CAM_RA_CAM_26M_CG = 76,
	CLK_CAM_RB_LARBX_CG = 77,
	CLK_CAM_RB_CAM_CG = 78,
	CLK_CAM_RB_CAMTG_CG = 79,
	CLK_CAM_RB_CAM_26M_CG = 80,
	CLK_CAMSYS_RMSA_LARBX_CG = 81,
	CLK_CAMSYS_RMSA_CAM_CG = 82,
	CLK_CAMSYS_RMSA_CAMTG_CG = 83,
	CLK_CAMSYS_RMSB_LARBX_CG = 84,
	CLK_CAMSYS_RMSB_CAM_CG = 85,
	CLK_CAMSYS_RMSB_CAMTG_CG = 86,
	CLK_CAM_YA_LARBX_CG = 87,
	CLK_CAM_YA_CAM_CG = 88,
	CLK_CAM_YA_CAMTG_CG = 89,
	CLK_CAM_YB_LARBX_CG = 90,
	CLK_CAM_YB_CAM_CG = 91,
	CLK_CAM_YB_CAMTG_CG = 92,
	CLK_CAM_MAIN_CAM_MAIN_CG = 93,
	CLK_CAM_MAIN_CAM_SUBA_CG = 94,
	CLK_CAM_MAIN_CAM_SUBB_CG = 95,
	CLK_CAM_MAIN_CAM_SUBC_CG = 96,
	CLK_CAM_MAIN_CAM_SENINF_TG_SUBA_CG = 97,
	CLK_CAM_MAIN_CAM_SENINF_TG_SUBB_CG = 98,
	CLK_CAM_MAIN_CAMTG_CG = 99,
	CLK_CAM_MAIN_SENINF_CG = 100,
	CLK_CAM_MAIN_SUB_COMM_0C_0_CG = 101,
	CLK_CAM_MAIN_SUB_COMM_1_CG = 102,
	CLK_CAM_MAIN_IPS_CG = 103,
	CLK_CAM_MAIN_CAM_ASG_CG = 104,
	CLK_CAM_MAIN_CAM_QOF_CON_1_CG = 105,
	CLK_CAM_MAIN_CAM_BWR_CON_1_CG = 106,
	CLK_CAM_MAIN_CAM_RTCQ_CON_1_CG = 107,
	CLK_CAM_MAIN_CAM_SDLCQ_CON_1_CG = 108,
	CLK_CAM_MAIN_CAM_WLA_CON_1_CG = 109,
	CLK_CAM_MAIN_CAM_DVC_CON_1_CG = 110,
	CLK_CAM_MAIN_CAM_CVFS_CON_1_CG = 111,
	CLK__VCORE_CG = 112,
	CLK__26M_CG = 113,
	CLK__BLS_PART_CG = 114,
	CLK__BLS_FULL_CG = 115,
	CLK__RESV0_GALS_CG = 116,
	CLK__RESV1_GALS_CG = 117,
	CLK__VCORE_CAM2MM0_CG = 118,
	CLK__VCORE_CAM2MM1_CG = 119,
	CLK_DIP_NR1_DIP1_LARB_CG = 120,
	CLK_DIP_NR1_DIP1_DIP_NR1_CG = 121,
	CLK_DIP_NR2_DIP1_DIP_NR_CG = 122,
	CLK_DIP_NR2_DIP1_LARB15_CG = 123,
	CLK_DIP_NR2_DIP1_LARB39_CG = 124,
	CLK_DIP_TOP_DIP1_DIP_TOP_CG = 125,
	CLK_DIP_TOP_DIP1_DIP_TOP_GALS0_CG = 126,
	CLK_DIP_TOP_DIP1_DIP_TOP_GALS1_CG = 127,
	CLK_DIP_TOP_DIP1_DIP_TOP_GALS2_CG = 128,
	CLK_DIP_TOP_DIP1_DIP_TOP_GALS3_CG = 129,
	CLK_DIP_TOP_DIP1_LARB10_CG = 130,
	CLK_DIP_TOP_DIP1_LARB15_CG = 131,
	CLK_DIP_TOP_DIP1_LARB38_CG = 132,
	CLK_DIP_TOP_DIP1_LARB39_CG = 133,
	CLK_MM_DISP_OVL0_2L_CG = 134,
	CLK_MM_DISP_OVL1_2L_CG = 135,
	CLK_MM_DISP_OVL2_2L_CG = 136,
	CLK_MM_DISP_OVL3_2L_CG = 137,
	CLK_MM_DISP_RSZ1_CG = 138,
	CLK_MM_DISP_RSZ0_CG = 139,
	CLK_MM_DISP_TDSHP0_CG = 140,
	CLK_MM_DISP_C3D0_CG = 141,
	CLK_MM_DISP_COLOR0_CG = 142,
	CLK_MM_DISP_CCORR0_CG = 143,
	CLK_MM_DISP_CCORR1_CG = 144,
	CLK_MM_DISP_AAL0_CG = 145,
	CLK_MM_DISP_GAMMA0_CG = 146,
	CLK_MM_DISP_POSTMASK0_CG = 147,
	CLK_MM_DISP_DITHER0_CG = 148,
	CLK_MM_DISP_TDSHP1_CG = 149,
	CLK_MM_DISP_C3D1_CG = 150,
	CLK_MM_DISP_CCORR2_CG = 151,
	CLK_MM_DISP_CCORR3_CG = 152,
	CLK_MM_DISP_GAMMA1_CG = 153,
	CLK_MM_DISP_DITHER1_CG = 154,
	CLK_MM_DISP_SPLITTER0_CG = 155,
	CLK_MM_DISP_DSC_WRAP0_CG = 156,
	CLK_MM_DISP_DSI0_CG = 157,
	CLK_MM_DISP_DSI1_CG = 158,
	CLK_MM_DISP_WDMA1_CG = 159,
	CLK_MM_DISP_APB_BUS_CG = 160,
	CLK_MM_DISP_FAKE_ENG0_CG = 161,
	CLK_MM_DISP_FAKE_ENG1_CG = 162,
	CLK_MM_DISP_MUTEX0_CG = 163,
	CLK_MM_SMI_COMMON_CG = 164,
	CLK_MM_DSI0_CG = 165,
	CLK_MM_DSI1_CG = 166,
	CLK_MM_26M_CG = 167,
	CLK_IMG_LARB9_CG = 168,
	CLK_IMG_TRAW0_CG = 169,
	CLK_IMG_TRAW1_CG = 170,
	CLK_IMG_DIP0_CG = 171,
	CLK_IMG_WPE0_CG = 172,
	CLK_IMG_IPE_CG = 173,
	CLK_IMG_WPE1_CG = 174,
	CLK_IMG_WPE2_CG = 175,
	CLK_IMG_SUB_COMMON0_CG = 176,
	CLK_IMG_SUB_COMMON1_CG = 177,
	CLK_IMG_SUB_COMMON3_CG = 178,
	CLK_IMG_SUB_COMMON4_CG = 179,
	CLK_IMG_FDVT_CG = 180,
	CLK_IMG_LARB12_CG = 181,
	CLK_IMG_ODPM26_CG = 182,
	CLK_IMG26_CG = 183,
	CLK_IMG_VCORE_SUB0_CG = 184,
	CLK_IMG_VCORE_SUB1_CG = 185,
	CLK_IMG_VCORE_IMG_26M_CG = 186,
	CLK_IMP_IIC_TOP_WRAP_S_I2C2_CG = 187,
	CLK_IMP_IIC_TOP_WRAP_S_I2C4_CG = 188,
	CLK_IMP_IIC_TOP_WRAP_S_I2C7_CG = 189,
	CLK_IMP_IIC_TOP_WRAP_S_I2C8_CG = 190,
	CLK_IMP_IIC_TOP_WRAP_S_I2C9_CG = 191,
	CLK_IMP_IIC_TOP_WRAP_S_I2C10_CG = 192,
	CLK_IMP_IIC_TOP_WRAP_S_I2C11_CG = 193,
	CLK_IMP_IIC_TOP_WRAP_S_I2C12_CG = 194,
	CLK_IMP_IIC_TOP_WRAP_W_I2C0_CG = 195,
	CLK_IMP_IIC_TOP_WRAP_W_I2C1_CG = 196,
	CLK_IMP_IIC_TOP_WRAP_W_I2C3_CG = 197,
	CLK_IMP_IIC_TOP_WRAP_W_I2C5_CG = 198,
	CLK_IMP_IIC_TOP_WRAP_W_I2C6_CG = 199,
	CLK_INFRA_AO_REG_CCIF1_AP_CG = 200,
	CLK_INFRA_AO_REG_CCIF1_MD_CG = 201,
	CLK_INFRA_AO_REG_CCIF_AP_CG = 202,
	CLK_INFRA_AO_REG_CCIF_MD_CG = 203,
	CLK_INFRA_AO_REG_CLDMA_BCLK_CG = 204,
	CLK_INFRA_AO_REG_CCIF5_MD_CG = 205,
	CLK_INFRA_AO_REG_CCIF2_AP_CG = 206,
	CLK_INFRA_AO_REG_CCIF2_MD_CG = 207,
	CLK_INFRA_AO_REG_DPMAIF_MAIN_CG = 208,
	CLK_INFRA_AO_REG_CCIF4_MD_CG = 209,
	CLK_INFRA_AO_REG_RG_MMW_DPMAIF26M_CK_CG = 210,
	CLK_MDP_MUTEX0_CG = 211,
	CLK_MDP_APB_BUS_CG = 212,
	CLK_MDP_SMI0_CG = 213,
	CLK_MDP_RDMA0_CG = 214,
	CLK_MDP_FG0_CG = 215,
	CLK_MDP_HDR0_CG = 216,
	CLK_MDP_AAL0_CG = 217,
	CLK_MDP_RSZ0_CG = 218,
	CLK_MDP_TDSHP0_CG = 219,
	CLK_MDP_COLOR0_CG = 220,
	CLK_MDP_WROT0_CG = 221,
	CLK_MDP_FAKE_ENG0_CG = 222,
	CLK_MDP_DLI_ASYNC0_CG = 223,
	CLK_MDP_DLI_ASYNC1_CG = 224,
	CLK_MDP_RDMA1_CG = 225,
	CLK_MDP_FG1_CG = 226,
	CLK_MDP_HDR1_CG = 227,
	CLK_MDP_AAL1_CG = 228,
	CLK_MDP_RSZ1_CG = 229,
	CLK_MDP_TDSHP1_CG = 230,
	CLK_MDP_COLOR1_CG = 231,
	CLK_MDP_WROT1_CG = 232,
	CLK_MDP_RSZ2_CG = 233,
	CLK_MDP_WROT2_CG = 234,
	CLK_MDP_DLO_ASYNC0_CG = 235,
	CLK_MDP_RSZ3_CG = 236,
	CLK_MDP_WROT3_CG = 237,
	CLK_MDP_DLO_ASYNC1_CG = 238,
	CLK_MDP_HRE_TOP_MDPSYS_CG = 239,
	CLK_MDP_FMM_IMG_DL_ASYNC0_CG = 240,
	CLK_MDP_FMM_IMG_DL_ASYNC1_CG = 241,
	CLK_MDP_FIMG_IMG_DL_ASYNC0_CG = 242,
	CLK_MDP_FIMG_IMG_DL_ASYNC1_CG = 243,
	CLK_MIPI_CSI_RG_CSIRX_CSI_CK0_EN_CG = 244,
	CLK_MIPI_CSI_RG_CSIRX_CSI_CK1_EN_CG = 245,
	CLK_MIPI_CSI_RG_CSIRX_CSI_CK2_EN_CG = 246,
	CLK_MIPI_CSI_RG_CSIRX_CSI_CK3_EN_CG = 247,
	CLK_MMINFRA_GCE_D_CG = 248,
	CLK_MMINFRA_GCE_M_CG = 249,
	CLK_MMINFRA_SMI_CG = 250,
	CLK_MMINFRA_GCE_M2_CG = 251,
	CLK_MMINFRA_GCE_26M_CG = 252,
	CLK_PERICFG_AO_REG_PERI_UART0_CG = 253,
	CLK_PERICFG_AO_REG_PERI_UART1_CG = 254,
	CLK_PERICFG_AO_REG_PERI_UART2_CG = 255,
	CLK_PERICFG_AO_REG_PERI_UART3_CG = 256,
	CLK_PERICFG_AO_REG_PERI_UART4_CG = 257,
	CLK_PERICFG_AO_REG_PERI_UART5_CG = 258,
	CLK_PERICFG_AO_REG_PERI_PWM_H_CG = 259,
	CLK_PERICFG_AO_REG_PERI_PWM_B_CG = 260,
	CLK_PERICFG_AO_REG_PERI_DISP_PWM0_CG = 261,
	CLK_PERICFG_AO_REG_PERI_DISP_PWM1_CG = 262,
	CLK_PERICFG_AO_REG_PERI_SPI0_B_CG = 263,
	CLK_PERICFG_AO_REG_PERI_SPI1_B_CG = 264,
	CLK_PERICFG_AO_REG_PERI_SPI2_B_CG = 265,
	CLK_PERICFG_AO_REG_PERI_SPI3_B_CG = 266,
	CLK_PERICFG_AO_REG_PERI_SPI4_B_CG = 267,
	CLK_PERICFG_AO_REG_PERI_SPI5_B_CG = 268,
	CLK_PERICFG_AO_REG_PERI_SPI6_B_CG = 269,
	CLK_PERICFG_AO_REG_PERI_SPI7_B_CG = 270,
	CLK_PERICFG_AO_REG_PERI_DMA_B_CG = 271,
	CLK_PERICFG_AO_REG_PERI_MSDC1_CG = 272,
	CLK_PERICFG_AO_REG_PERI_MSDC1_DIV_CG = 273,
	CLK_PERICFG_AO_REG_PERI_MSDC1_MST_F_CG = 274,
	CLK_PERICFG_AO_REG_PERI_MSDC1_SLV_H_CG = 275,
	CLK_PERICFG_AO_REG_PERI_AUDIO0_CG = 276,
	CLK_PERICFG_AO_REG_PERI_AUDIO1_CG = 277,
	CLK_PERICFG_AO_REG_PERI_AUDIO2_CG = 278,
	CLK_TRAW_CAP_DIP1_TRAW_CAP_CG = 279,
	CLK_TRAW_DIP1_LARB28_CG = 280,
	CLK_TRAW_DIP1_LARB40_CG = 281,
	CLK_TRAW_DIP1_TRAW_CG = 282,
	CLK_UFSAO_UNIPRO_TX_SYM_CG = 283,
	CLK_UFSAO_UNIPRO_RX_SYM0_CG = 284,
	CLK_UFSAO_UNIPRO_RX_SYM1_CG = 285,
	CLK_UFSAO_UNIPRO_SYS_CG = 286,
	CLK_UFSAO_UNIPRO_SAP_CFG_CG = 287,
	CLK_UFSAO_UFS_PHY_TOP_AHB_S_BUS_CG = 288,
	CLK_UFSPDN_UFSHCI_UFS_CG = 289,
	CLK_UFSPDN_UFSHCI_AES_CG = 290,
	CLK_UFSPDN_UFSHCI_UFS_AHB_CG = 291,
	CLK_UFSPDN_UFSHCI_UFS_AXI_CG = 292,
	CLK_VDE2_VDEC_CKEN_CG = 293,
	CLK_VDE2_VDEC_ACTIVE_CG = 294,
	CLK_VDE2_VDEC_CKEN_ENG_CG = 295,
	CLK_VDE2_LARB1_CKEN_CG = 296,
	CLK_VEN1_CKE0_LARB_CG = 297,
	CLK_VEN1_CKE1_VENC_CG = 298,
	CLK_VEN1_CKE2_JPGENC_CG = 299,
	CLK_VEN1_CKE5_GALS_CG = 300,
	CLK_WPE_EIS_DIP1_LARB_U0_CG = 301,
	CLK_WPE_EIS_DIP1_LARB_U1_CG = 302,
	CLK_WPE_EIS_DIP1_GALS_U0_CG = 303,
	CLK_WPE_EIS_DIP1_GALS_U1_CG = 304,
	CLK_WPE_EIS_DIP1_WPE_MACRO_CG = 305,
	CLK_WPE_EIS_DIP1_WPE_CG = 306,
	CLK_WPE_EIS_DIP1_PQDIP_CG = 307,
	CLK_WPE_EIS_DIP1_PQDIP_DMA_CG = 308,
	CLK_WPE_EIS_DIP1_OMC_CG = 309,
	CLK_WPE_EIS_DIP1_DWPE_CG = 310,
	CLK_WPE_EIS_DIP1_ME_CG = 311,
	CLK_WPE_EIS_DIP1_MMG_CG = 312,
	CLK_WPE_EIS_DIP1_WPE_26M_EN_CG = 313,
	CLK_WPE_TNR_DIP1_LARB_U0_CG = 314,
	CLK_WPE_TNR_DIP1_LARB_U1_CG = 315,
	CLK_WPE_TNR_DIP1_GALS_U0_CG = 316,
	CLK_WPE_TNR_DIP1_GALS_U1_CG = 317,
	CLK_WPE_TNR_DIP1_WPE_MACRO_CG = 318,
	CLK_WPE_TNR_DIP1_WPE_CG = 319,
	CLK_WPE_TNR_DIP1_PQDIP_CG = 320,
	CLK_WPE_TNR_DIP1_PQDIP_DMA_CG = 321,
	CLK_WPE_TNR_DIP1_OMC_CG = 322,
	CLK_WPE_TNR_DIP1_DWPE_CG = 323,
	CLK_WPE_TNR_DIP1_ME_CG = 324,
	CLK_WPE_TNR_DIP1_MMG_CG = 325,
	TRACE_CLK_NUM = 326,
};

const char *trace_subsys_cgs[] = {
	[CLK_AFE_DL1_DAC_TML_CG] = "afe_dl1_dac_tml",
	[CLK_AFE_DL1_DAC_HIRES_CG] = "afe_dl1_dac_hires",
	[CLK_AFE_DL1_DAC_CG] = "afe_dl1_dac",
	[CLK_AFE_DL1_PREDIS_CG] = "afe_dl1_predis",
	[CLK_AFE_DL1_NLE_CG] = "afe_dl1_nle",
	[CLK_AFE_DL0_DAC_TML_CG] = "afe_dl0_dac_tml",
	[CLK_AFE_DL0_DAC_HIRES_CG] = "afe_dl0_dac_hires",
	[CLK_AFE_DL0_DAC_CG] = "afe_dl0_dac",
	[CLK_AFE_DL0_PREDIS_CG] = "afe_dl0_predis",
	[CLK_AFE_DL0_NLE_CG] = "afe_dl0_nle",
	[CLK_AFE_PCM1_CG] = "afe_pcm1",
	[CLK_AFE_PCM0_CG] = "afe_pcm0",
	[CLK_AFE_CM2_CG] = "afe_cm2",
	[CLK_AFE_CM1_CG] = "afe_cm1",
	[CLK_AFE_CM0_CG] = "afe_cm0",
	[CLK_AFE_STF_CG] = "afe_stf",
	[CLK_AFE_HW_GAIN23_CG] = "afe_hw_gain23",
	[CLK_AFE_HW_GAIN01_CG] = "afe_hw_gain01",
	[CLK_AFE_FM_I2S_CG] = "afe_fm_i2s",
	[CLK_AFE_MTKAIFV4_CG] = "afe_mtkaifv4",
	[CLK_AFE_AUDIO_HOPPING_CG] = "afe_audio_hopping_ck",
	[CLK_AFE_AUDIO_F26M_CG] = "afe_audio_f26m_ck",
	[CLK_AFE_APLL1_CG] = "afe_apll1_ck",
	[CLK_AFE_APLL2_CG] = "afe_apll2_ck",
	[CLK_AFE_H208M_CG] = "afe_h208m_ck",
	[CLK_AFE_APLL_TUNER2_CG] = "afe_apll_tuner2",
	[CLK_AFE_APLL_TUNER1_CG] = "afe_apll_tuner1",
	[CLK_AFE_AUD_PAD_TOP_MOSI_EN_CG] = "afe_aud_pad_mosi",
	[CLK_AFE_ETDM6_PADTOP_CK_EN_CG] = "afe_etdm6_padtop",
	[CLK_AFE_ETDM7_PADTOP_CK_EN_CG] = "afe_etdm7_padtop",
	[CLK_AFE_UL1_ADC_HIRES_TML_CG] = "afe_ul1_aht",
	[CLK_AFE_UL1_ADC_HIRES_CG] = "afe_ul1_adc_hires",
	[CLK_AFE_UL1_TML_CG] = "afe_ul1_tml",
	[CLK_AFE_UL1_ADC_CG] = "afe_ul1_adc",
	[CLK_AFE_UL0_ADC_HIRES_TML_CG] = "afe_ul0_aht",
	[CLK_AFE_UL0_ADC_HIRES_CG] = "afe_ul0_adc_hires",
	[CLK_AFE_UL0_TML_CG] = "afe_ul0_tml",
	[CLK_AFE_UL0_ADC_CG] = "afe_ul0_adc",
	[CLK_AFE_ETDM_IN_DMA0_CG] = "afe_etdm_in_dma0",
	[CLK_AFE_ETDM_IN6_CG] = "afe_etdm_in6",
	[CLK_AFE_ETDM_IN2_CG] = "afe_etdm_in2",
	[CLK_AFE_ETDM_IN1_CG] = "afe_etdm_in1",
	[CLK_AFE_ETDM_IN0_CG] = "afe_etdm_in0",
	[CLK_AFE_ETDM_OUT6_CG] = "afe_etdm_out6",
	[CLK_AFE_ETDM_OUT2_CG] = "afe_etdm_out2",
	[CLK_AFE_ETDM_OUT1_CG] = "afe_etdm_out1",
	[CLK_AFE_ETDM_OUT0_CG] = "afe_etdm_out0",
	[CLK_AFE_GENERAL7_ASRC_CG] = "afe_general7_asrc",
	[CLK_AFE_GENERAL6_ASRC_CG] = "afe_general6_asrc",
	[CLK_AFE_GENERAL5_ASRC_CG] = "afe_general5_asrc",
	[CLK_AFE_GENERAL4_ASRC_CG] = "afe_general4_asrc",
	[CLK_AFE_GENERAL3_ASRC_CG] = "afe_general3_asrc",
	[CLK_AFE_GENERAL2_ASRC_CG] = "afe_general2_asrc",
	[CLK_AFE_GENERAL1_ASRC_CG] = "afe_general1_asrc",
	[CLK_AFE_GENERAL0_ASRC_CG] = "afe_general0_asrc",
	[CLK_AFE_CONNSYS_I2S_ASRC_CG] = "afe_connsys_i2s_asrc",
	[CLK_CAM_MR_LARB13_CG] = "cam_mr_larb13",
	[CLK_CAM_MR_LARB14_CG] = "cam_mr_larb14",
	[CLK_CAM_MR_LARB19_CG] = "cam_mr_larb19",
	[CLK_CAM_MR_LARB25_CG] = "cam_mr_larb25",
	[CLK_CAM_MR_LARB26_CG] = "cam_mr_larb26",
	[CLK_CAM_MR_LARB29_CG] = "cam_mr_larb29",
	[CLK_CAM_MR_SENINF_CAMTM_CG] = "cam_mr_seninf_camtm",
	[CLK_CAM_MR_CAMSV_TOP_CG] = "cam_mr_camsv_top",
	[CLK_CAM_MR_CAMSV_A_CG] = "cam_mr_camsv_a",
	[CLK_CAM_MR_CAMSV_B_CG] = "cam_mr_camsv_b",
	[CLK_CAM_MR_CAMSV_C_CG] = "cam_mr_camsv_c",
	[CLK_CAM_MR_CAMSV_D_CG] = "cam_mr_camsv_d",
	[CLK_CAM_MR_CAMSV_E_CG] = "cam_mr_camsv_e",
	[CLK_CAM_MR_CAMSV_CG] = "cam_mr_camsv",
	[CLK_CAM_MR_PDA0_CG] = "cam_mr_pda0",
	[CLK_CAM_MR_PDA1_CG] = "cam_mr_pda1",
	[CLK_CAM_MR_FAKE_ENG_CG] = "cam_mr_fake_eng",
	[CLK_CAM_RA_LARBX_CG] = "cam_ra_larbx",
	[CLK_CAM_RA_CAM_CG] = "cam_ra_cam",
	[CLK_CAM_RA_CAMTG_CG] = "cam_ra_camtg",
	[CLK_CAM_RA_CAM_26M_CG] = "cam_ra_cam_26m",
	[CLK_CAM_RB_LARBX_CG] = "cam_rb_larbx",
	[CLK_CAM_RB_CAM_CG] = "cam_rb_cam",
	[CLK_CAM_RB_CAMTG_CG] = "cam_rb_camtg",
	[CLK_CAM_RB_CAM_26M_CG] = "cam_rb_cam_26m",
	[CLK_CAMSYS_RMSA_LARBX_CG] = "camsys_rmsa_larbx",
	[CLK_CAMSYS_RMSA_CAM_CG] = "camsys_rmsa_cam",
	[CLK_CAMSYS_RMSA_CAMTG_CG] = "camsys_rmsa_camtg",
	[CLK_CAMSYS_RMSB_LARBX_CG] = "camsys_rmsb_larbx",
	[CLK_CAMSYS_RMSB_CAM_CG] = "camsys_rmsb_cam",
	[CLK_CAMSYS_RMSB_CAMTG_CG] = "camsys_rmsb_camtg",
	[CLK_CAM_YA_LARBX_CG] = "cam_ya_larbx",
	[CLK_CAM_YA_CAM_CG] = "cam_ya_cam",
	[CLK_CAM_YA_CAMTG_CG] = "cam_ya_camtg",
	[CLK_CAM_YB_LARBX_CG] = "cam_yb_larbx",
	[CLK_CAM_YB_CAM_CG] = "cam_yb_cam",
	[CLK_CAM_YB_CAMTG_CG] = "cam_yb_camtg",
	[CLK_CAM_MAIN_CAM_MAIN_CG] = "cam_m_cam_main",
	[CLK_CAM_MAIN_CAM_SUBA_CG] = "cam_m_cam_suba",
	[CLK_CAM_MAIN_CAM_SUBB_CG] = "cam_m_cam_subb",
	[CLK_CAM_MAIN_CAM_SUBC_CG] = "cam_m_cam_subc",
	[CLK_CAM_MAIN_CAM_SENINF_TG_SUBA_CG] = "cam_m_cam_seninf_tg_suba",
	[CLK_CAM_MAIN_CAM_SENINF_TG_SUBB_CG] = "cam_m_cam_seninf_tg_subb",
	[CLK_CAM_MAIN_CAMTG_CG] = "cam_m_camtg",
	[CLK_CAM_MAIN_SENINF_CG] = "cam_m_seninf",
	[CLK_CAM_MAIN_SUB_COMM_0C_0_CG] = "cam_m_sub_comm_0c_0",
	[CLK_CAM_MAIN_SUB_COMM_1_CG] = "cam_m_sub_comm_1",
	[CLK_CAM_MAIN_IPS_CG] = "cam_m_ips",
	[CLK_CAM_MAIN_CAM_ASG_CG] = "cam_m_cam_asg",
	[CLK_CAM_MAIN_CAM_QOF_CON_1_CG] = "cam_m_cam_qof_con_1",
	[CLK_CAM_MAIN_CAM_BWR_CON_1_CG] = "cam_m_cam_bwr_con_1",
	[CLK_CAM_MAIN_CAM_RTCQ_CON_1_CG] = "cam_m_cam_rtcq_con_1",
	[CLK_CAM_MAIN_CAM_SDLCQ_CON_1_CG] = "cam_m_cam_sdlcq_con_1",
	[CLK_CAM_MAIN_CAM_WLA_CON_1_CG] = "cam_m_cam_wla_con_1",
	[CLK_CAM_MAIN_CAM_DVC_CON_1_CG] = "cam_m_cam_dvc_con_1",
	[CLK_CAM_MAIN_CAM_CVFS_CON_1_CG] = "cam_m_cam_cvfs_con_1",
	[CLK__VCORE_CG] = "_vcore",
	[CLK__26M_CG] = "_26m",
	[CLK__BLS_PART_CG] = "_bls_part",
	[CLK__BLS_FULL_CG] = "_bls_full",
	[CLK__RESV0_GALS_CG] = "_resv0_GCON_0",
	[CLK__RESV1_GALS_CG] = "_resv1_GCON_0",
	[CLK__VCORE_CAM2MM0_CG] = "_vcore_cam2mm0",
	[CLK__VCORE_CAM2MM1_CG] = "_vcore_cam2mm1",
	[CLK_DIP_NR1_DIP1_LARB_CG] = "dip_nr1_dip1_larb",
	[CLK_DIP_NR1_DIP1_DIP_NR1_CG] = "dip_nr1_dip1_dip_nr1",
	[CLK_DIP_NR2_DIP1_DIP_NR_CG] = "dip_nr2_dip1_dip_nr",
	[CLK_DIP_NR2_DIP1_LARB15_CG] = "dip_nr2_dip1_larb15",
	[CLK_DIP_NR2_DIP1_LARB39_CG] = "dip_nr2_dip1_larb39",
	[CLK_DIP_TOP_DIP1_DIP_TOP_CG] = "dip_dip1_dip_top",
	[CLK_DIP_TOP_DIP1_DIP_TOP_GALS0_CG] = "dip_dip1_dip_gals0",
	[CLK_DIP_TOP_DIP1_DIP_TOP_GALS1_CG] = "dip_dip1_dip_gals1",
	[CLK_DIP_TOP_DIP1_DIP_TOP_GALS2_CG] = "dip_dip1_dip_gals2",
	[CLK_DIP_TOP_DIP1_DIP_TOP_GALS3_CG] = "dip_dip1_dip_gals3",
	[CLK_DIP_TOP_DIP1_LARB10_CG] = "dip_dip1_larb10",
	[CLK_DIP_TOP_DIP1_LARB15_CG] = "dip_dip1_larb15",
	[CLK_DIP_TOP_DIP1_LARB38_CG] = "dip_dip1_larb38",
	[CLK_DIP_TOP_DIP1_LARB39_CG] = "dip_dip1_larb39",
	[CLK_MM_DISP_OVL0_2L_CG] = "mm_disp_ovl0_2l",
	[CLK_MM_DISP_OVL1_2L_CG] = "mm_disp_ovl1_2l",
	[CLK_MM_DISP_OVL2_2L_CG] = "mm_disp_ovl2_2l",
	[CLK_MM_DISP_OVL3_2L_CG] = "mm_disp_ovl3_2l",
	[CLK_MM_DISP_RSZ1_CG] = "mm_disp_rsz1",
	[CLK_MM_DISP_RSZ0_CG] = "mm_disp_rsz0",
	[CLK_MM_DISP_TDSHP0_CG] = "mm_disp_tdshp0",
	[CLK_MM_DISP_C3D0_CG] = "mm_disp_c3d0",
	[CLK_MM_DISP_COLOR0_CG] = "mm_disp_color0",
	[CLK_MM_DISP_CCORR0_CG] = "mm_disp_ccorr0",
	[CLK_MM_DISP_CCORR1_CG] = "mm_disp_ccorr1",
	[CLK_MM_DISP_AAL0_CG] = "mm_disp_aal0",
	[CLK_MM_DISP_GAMMA0_CG] = "mm_disp_gamma0",
	[CLK_MM_DISP_POSTMASK0_CG] = "mm_disp_postmask0",
	[CLK_MM_DISP_DITHER0_CG] = "mm_disp_dither0",
	[CLK_MM_DISP_TDSHP1_CG] = "mm_disp_tdshp1",
	[CLK_MM_DISP_C3D1_CG] = "mm_disp_c3d1",
	[CLK_MM_DISP_CCORR2_CG] = "mm_disp_ccorr2",
	[CLK_MM_DISP_CCORR3_CG] = "mm_disp_ccorr3",
	[CLK_MM_DISP_GAMMA1_CG] = "mm_disp_gamma1",
	[CLK_MM_DISP_DITHER1_CG] = "mm_disp_dither1",
	[CLK_MM_DISP_SPLITTER0_CG] = "mm_disp_splitter0",
	[CLK_MM_DISP_DSC_WRAP0_CG] = "mm_disp_dsc_wrap0",
	[CLK_MM_DISP_DSI0_CG] = "mm_CLK0",
	[CLK_MM_DISP_DSI1_CG] = "mm_CLK1",
	[CLK_MM_DISP_WDMA1_CG] = "mm_disp_wdma1",
	[CLK_MM_DISP_APB_BUS_CG] = "mm_disp_apb_bus",
	[CLK_MM_DISP_FAKE_ENG0_CG] = "mm_disp_fake_eng0",
	[CLK_MM_DISP_FAKE_ENG1_CG] = "mm_disp_fake_eng1",
	[CLK_MM_DISP_MUTEX0_CG] = "mm_disp_mutex0",
	[CLK_MM_SMI_COMMON_CG] = "mm_smi_common",
	[CLK_MM_DSI0_CG] = "mm_dsi0_ck",
	[CLK_MM_DSI1_CG] = "mm_dsi1_ck",
	[CLK_MM_26M_CG] = "mm_26m_ck",
	[CLK_IMG_LARB9_CG] = "img_larb9",
	[CLK_IMG_TRAW0_CG] = "img_traw0",
	[CLK_IMG_TRAW1_CG] = "img_traw1",
	[CLK_IMG_DIP0_CG] = "img_dip0",
	[CLK_IMG_WPE0_CG] = "img_wpe0",
	[CLK_IMG_IPE_CG] = "img_ipe",
	[CLK_IMG_WPE1_CG] = "img_wpe1",
	[CLK_IMG_WPE2_CG] = "img_wpe2",
	[CLK_IMG_SUB_COMMON0_CG] = "img_sub_common0",
	[CLK_IMG_SUB_COMMON1_CG] = "img_sub_common1",
	[CLK_IMG_SUB_COMMON3_CG] = "img_sub_common3",
	[CLK_IMG_SUB_COMMON4_CG] = "img_sub_common4",
	[CLK_IMG_FDVT_CG] = "img_fdvt",
	[CLK_IMG_LARB12_CG] = "img_larb12",
	[CLK_IMG_ODPM26_CG] = "img_odpm26",
	[CLK_IMG26_CG] = "img26",
	[CLK_IMG_VCORE_SUB0_CG] = "img_vcore_sub0",
	[CLK_IMG_VCORE_SUB1_CG] = "img_vcore_sub1",
	[CLK_IMG_VCORE_IMG_26M_CG] = "img_vcore_img_26m",
	[CLK_IMP_IIC_TOP_WRAP_S_I2C2_CG] = "imp_iic_wrap_s_i2c2",
	[CLK_IMP_IIC_TOP_WRAP_S_I2C4_CG] = "imp_iic_wrap_s_i2c4",
	[CLK_IMP_IIC_TOP_WRAP_S_I2C7_CG] = "imp_iic_wrap_s_i2c7",
	[CLK_IMP_IIC_TOP_WRAP_S_I2C8_CG] = "imp_iic_wrap_s_i2c8",
	[CLK_IMP_IIC_TOP_WRAP_S_I2C9_CG] = "imp_iic_wrap_s_i2c9",
	[CLK_IMP_IIC_TOP_WRAP_S_I2C10_CG] = "imp_iic_wrap_s_i2c10",
	[CLK_IMP_IIC_TOP_WRAP_S_I2C11_CG] = "imp_iic_wrap_s_i2c11",
	[CLK_IMP_IIC_TOP_WRAP_S_I2C12_CG] = "imp_iic_wrap_s_i2c12",
	[CLK_IMP_IIC_TOP_WRAP_W_I2C0_CG] = "imp_iic_wrap_w_i2c0",
	[CLK_IMP_IIC_TOP_WRAP_W_I2C1_CG] = "imp_iic_wrap_w_i2c1",
	[CLK_IMP_IIC_TOP_WRAP_W_I2C3_CG] = "imp_iic_wrap_w_i2c3",
	[CLK_IMP_IIC_TOP_WRAP_W_I2C5_CG] = "imp_iic_wrap_w_i2c5",
	[CLK_IMP_IIC_TOP_WRAP_W_I2C6_CG] = "imp_iic_wrap_w_i2c6",
	[CLK_INFRA_AO_REG_CCIF1_AP_CG] = "infra_ao_ccif1_ap",
	[CLK_INFRA_AO_REG_CCIF1_MD_CG] = "infra_ao_ccif1_md",
	[CLK_INFRA_AO_REG_CCIF_AP_CG] = "infra_ao_ccif_ap",
	[CLK_INFRA_AO_REG_CCIF_MD_CG] = "infra_ao_ccif_md",
	[CLK_INFRA_AO_REG_CLDMA_BCLK_CG] = "infra_ao_cldmabclk",
	[CLK_INFRA_AO_REG_CCIF5_MD_CG] = "infra_ao_ccif5_md",
	[CLK_INFRA_AO_REG_CCIF2_AP_CG] = "infra_ao_ccif2_ap",
	[CLK_INFRA_AO_REG_CCIF2_MD_CG] = "infra_ao_ccif2_md",
	[CLK_INFRA_AO_REG_DPMAIF_MAIN_CG] = "infra_ao_dpmaif_main",
	[CLK_INFRA_AO_REG_CCIF4_MD_CG] = "infra_ao_ccif4_md",
	[CLK_INFRA_AO_REG_RG_MMW_DPMAIF26M_CK_CG] = "infra_ao_dpmaif_26m_set",
	[CLK_MDP_MUTEX0_CG] = "mdp_mutex0",
	[CLK_MDP_APB_BUS_CG] = "mdp_apb_bus",
	[CLK_MDP_SMI0_CG] = "mdp_smi0",
	[CLK_MDP_RDMA0_CG] = "mdp_rdma0",
	[CLK_MDP_FG0_CG] = "mdp_fg0",
	[CLK_MDP_HDR0_CG] = "mdp_hdr0",
	[CLK_MDP_AAL0_CG] = "mdp_aal0",
	[CLK_MDP_RSZ0_CG] = "mdp_rsz0",
	[CLK_MDP_TDSHP0_CG] = "mdp_tdshp0",
	[CLK_MDP_COLOR0_CG] = "mdp_color0",
	[CLK_MDP_WROT0_CG] = "mdp_wrot0",
	[CLK_MDP_FAKE_ENG0_CG] = "mdp_fake_eng0",
	[CLK_MDP_DLI_ASYNC0_CG] = "mdp_dli_async0",
	[CLK_MDP_DLI_ASYNC1_CG] = "mdp_dli_async1",
	[CLK_MDP_RDMA1_CG] = "mdp_rdma1",
	[CLK_MDP_FG1_CG] = "mdp_fg1",
	[CLK_MDP_HDR1_CG] = "mdp_hdr1",
	[CLK_MDP_AAL1_CG] = "mdp_aal1",
	[CLK_MDP_RSZ1_CG] = "mdp_rsz1",
	[CLK_MDP_TDSHP1_CG] = "mdp_tdshp1",
	[CLK_MDP_COLOR1_CG] = "mdp_color1",
	[CLK_MDP_WROT1_CG] = "mdp_wrot1",
	[CLK_MDP_RSZ2_CG] = "mdp_rsz2",
	[CLK_MDP_WROT2_CG] = "mdp_wrot2",
	[CLK_MDP_DLO_ASYNC0_CG] = "mdp_dlo_async0",
	[CLK_MDP_RSZ3_CG] = "mdp_rsz3",
	[CLK_MDP_WROT3_CG] = "mdp_wrot3",
	[CLK_MDP_DLO_ASYNC1_CG] = "mdp_dlo_async1",
	[CLK_MDP_HRE_TOP_MDPSYS_CG] = "mdp_hre_mdpsys",
	[CLK_MDP_FMM_IMG_DL_ASYNC0_CG] = "mdp_fmm_img_dl_async0",
	[CLK_MDP_FMM_IMG_DL_ASYNC1_CG] = "mdp_fmm_img_dl_async1",
	[CLK_MDP_FIMG_IMG_DL_ASYNC0_CG] = "mdp_fimg_img_dl_async0",
	[CLK_MDP_FIMG_IMG_DL_ASYNC1_CG] = "mdp_fimg_img_dl_async1",
	[CLK_MIPI_CSI_RG_CSIRX_CSI_CK0_EN_CG] = "mipi_csi_ck0_en",
	[CLK_MIPI_CSI_RG_CSIRX_CSI_CK1_EN_CG] = "mipi_csi_ck1_en",
	[CLK_MIPI_CSI_RG_CSIRX_CSI_CK2_EN_CG] = "mipi_csi_ck2_en",
	[CLK_MIPI_CSI_RG_CSIRX_CSI_CK3_EN_CG] = "mipi_csi_ck3_en",
	[CLK_MMINFRA_GCE_D_CG] = "mminfra_gce_d",
	[CLK_MMINFRA_GCE_M_CG] = "mminfra_gce_m",
	[CLK_MMINFRA_SMI_CG] = "mminfra_smi",
	[CLK_MMINFRA_GCE_M2_CG] = "mminfra_gce_m2",
	[CLK_MMINFRA_GCE_26M_CG] = "mminfra_gce_26m",
	[CLK_PERICFG_AO_REG_PERI_UART0_CG] = "peri_ao_uart0",
	[CLK_PERICFG_AO_REG_PERI_UART1_CG] = "peri_ao_uart1",
	[CLK_PERICFG_AO_REG_PERI_UART2_CG] = "peri_ao_uart2",
	[CLK_PERICFG_AO_REG_PERI_UART3_CG] = "peri_ao_uart3",
	[CLK_PERICFG_AO_REG_PERI_UART4_CG] = "peri_ao_uart4",
	[CLK_PERICFG_AO_REG_PERI_UART5_CG] = "peri_ao_uart5",
	[CLK_PERICFG_AO_REG_PERI_PWM_H_CG] = "peri_ao_pwm_h",
	[CLK_PERICFG_AO_REG_PERI_PWM_B_CG] = "peri_ao_pwm_b",
	[CLK_PERICFG_AO_REG_PERI_DISP_PWM0_CG] = "peri_ao_disp_pwm0",
	[CLK_PERICFG_AO_REG_PERI_DISP_PWM1_CG] = "peri_ao_disp_pwm1",
	[CLK_PERICFG_AO_REG_PERI_SPI0_B_CG] = "peri_ao_spi0_b",
	[CLK_PERICFG_AO_REG_PERI_SPI1_B_CG] = "peri_ao_spi1_b",
	[CLK_PERICFG_AO_REG_PERI_SPI2_B_CG] = "peri_ao_spi2_b",
	[CLK_PERICFG_AO_REG_PERI_SPI3_B_CG] = "peri_ao_spi3_b",
	[CLK_PERICFG_AO_REG_PERI_SPI4_B_CG] = "peri_ao_spi4_b",
	[CLK_PERICFG_AO_REG_PERI_SPI5_B_CG] = "peri_ao_spi5_b",
	[CLK_PERICFG_AO_REG_PERI_SPI6_B_CG] = "peri_ao_spi6_b",
	[CLK_PERICFG_AO_REG_PERI_SPI7_B_CG] = "peri_ao_spi7_b",
	[CLK_PERICFG_AO_REG_PERI_DMA_B_CG] = "peri_ao_dma_b",
	[CLK_PERICFG_AO_REG_PERI_MSDC1_CG] = "peri_ao_msdc1",
	[CLK_PERICFG_AO_REG_PERI_MSDC1_DIV_CG] = "peri_ao_msdc1_div",
	[CLK_PERICFG_AO_REG_PERI_MSDC1_MST_F_CG] = "peri_ao_msdc1_mst_f",
	[CLK_PERICFG_AO_REG_PERI_MSDC1_SLV_H_CG] = "peri_ao_msdc1_slv_h",
	[CLK_PERICFG_AO_REG_PERI_AUDIO0_CG] = "peri_ao_audio0",
	[CLK_PERICFG_AO_REG_PERI_AUDIO1_CG] = "peri_ao_audio1",
	[CLK_PERICFG_AO_REG_PERI_AUDIO2_CG] = "peri_ao_audio2",
	[CLK_TRAW_CAP_DIP1_TRAW_CAP_CG] = "traw__dip1_cap",
	[CLK_TRAW_DIP1_LARB28_CG] = "traw_dip1_larb28",
	[CLK_TRAW_DIP1_LARB40_CG] = "traw_dip1_larb40",
	[CLK_TRAW_DIP1_TRAW_CG] = "traw_dip1_traw",
	[CLK_UFSAO_UNIPRO_TX_SYM_CG] = "ufsao_unipro_tx_sym",
	[CLK_UFSAO_UNIPRO_RX_SYM0_CG] = "ufsao_unipro_rx_sym0",
	[CLK_UFSAO_UNIPRO_RX_SYM1_CG] = "ufsao_unipro_rx_sym1",
	[CLK_UFSAO_UNIPRO_SYS_CG] = "ufsao_unipro_sys",
	[CLK_UFSAO_UNIPRO_SAP_CFG_CG] = "ufsao_unipro_sap_cfg",
	[CLK_UFSAO_UFS_PHY_TOP_AHB_S_BUS_CG] = "ufsao_ufs_phy_ahb_s_bus",
	[CLK_UFSPDN_UFSHCI_UFS_CG] = "ufspdn_ufshci_ufs",
	[CLK_UFSPDN_UFSHCI_AES_CG] = "ufspdn_ufshci_aes",
	[CLK_UFSPDN_UFSHCI_UFS_AHB_CG] = "ufspdn_ufshci_ufs_ahb",
	[CLK_UFSPDN_UFSHCI_UFS_AXI_CG] = "ufspdn_ufshci_ufs_axi",
	[CLK_VDE2_VDEC_CKEN_CG] = "vde2_vdec_cken",
	[CLK_VDE2_VDEC_ACTIVE_CG] = "vde2_vdec_active",
	[CLK_VDE2_VDEC_CKEN_ENG_CG] = "vde2_vdec_cken_eng",
	[CLK_VDE2_LARB1_CKEN_CG] = "vde2_larb1_cken",
	[CLK_VEN1_CKE0_LARB_CG] = "ven1_larb",
	[CLK_VEN1_CKE1_VENC_CG] = "ven1_venc",
	[CLK_VEN1_CKE2_JPGENC_CG] = "ven1_jpgenc",
	[CLK_VEN1_CKE5_GALS_CG] = "ven1_gals",
	[CLK_WPE_EIS_DIP1_LARB_U0_CG] = "wpe_eis_dip1_larb_u0",
	[CLK_WPE_EIS_DIP1_LARB_U1_CG] = "wpe_eis_dip1_larb_u1",
	[CLK_WPE_EIS_DIP1_GALS_U0_CG] = "wpe_eis_dip1_gals_u0",
	[CLK_WPE_EIS_DIP1_GALS_U1_CG] = "wpe_eis_dip1_gals_u1",
	[CLK_WPE_EIS_DIP1_WPE_MACRO_CG] = "wpe_eis_dip1_wpe_macro",
	[CLK_WPE_EIS_DIP1_WPE_CG] = "wpe_eis_dip1_wpe",
	[CLK_WPE_EIS_DIP1_PQDIP_CG] = "wpe_eis_dip1_pqdip",
	[CLK_WPE_EIS_DIP1_PQDIP_DMA_CG] = "wpe_eis_dip1_pqdip_dma",
	[CLK_WPE_EIS_DIP1_OMC_CG] = "wpe_eis_dip1_omc",
	[CLK_WPE_EIS_DIP1_DWPE_CG] = "wpe_eis_dip1_dwpe",
	[CLK_WPE_EIS_DIP1_ME_CG] = "wpe_eis_dip1_me",
	[CLK_WPE_EIS_DIP1_MMG_CG] = "wpe_eis_dip1_mmg",
	[CLK_WPE_EIS_DIP1_WPE_26M_EN_CG] = "wpe_eis_dip1_wpe_26m",
	[CLK_WPE_TNR_DIP1_LARB_U0_CG] = "wpe_tnr_dip1_larb_u0",
	[CLK_WPE_TNR_DIP1_LARB_U1_CG] = "wpe_tnr_dip1_larb_u1",
	[CLK_WPE_TNR_DIP1_GALS_U0_CG] = "wpe_tnr_dip1_gals_u0",
	[CLK_WPE_TNR_DIP1_GALS_U1_CG] = "wpe_tnr_dip1_gals_u1",
	[CLK_WPE_TNR_DIP1_WPE_MACRO_CG] = "wpe_tnr_dip1_wpe_macro",
	[CLK_WPE_TNR_DIP1_WPE_CG] = "wpe_tnr_dip1_wpe",
	[CLK_WPE_TNR_DIP1_PQDIP_CG] = "wpe_tnr_dip1_pqdip",
	[CLK_WPE_TNR_DIP1_PQDIP_DMA_CG] = "wpe_tnr_dip1_pqdip_dma",
	[CLK_WPE_TNR_DIP1_OMC_CG] = "wpe_tnr_dip1_omc",
	[CLK_WPE_TNR_DIP1_DWPE_CG] = "wpe_tnr_dip1_dwpe",
	[CLK_WPE_TNR_DIP1_ME_CG] = "wpe_tnr_dip1_me",
	[CLK_WPE_TNR_DIP1_MMG_CG] = "wpe_tnr_dip1_mmg",
	[TRACE_CLK_NUM] = "NULL",
};

static void trace_clk_event(const char *name, unsigned int clk_sta)
{
	unsigned long flags = 0;
	int i;

	spin_lock_irqsave(&clk_trace_lock, flags);

	if (!name)
		goto OUT;

	for (i = 0; i < TRACE_CLK_NUM; i++) {
		if (!strcmp(trace_subsys_cgs[i], name))
			break;
	}

	if (i == TRACE_CLK_NUM)
		goto OUT;

	clk_event[evt_cnt] = (i << CLK_ID_SHIFT) | (clk_sta << CLK_STA_SHIFT);
	evt_cnt++;
	if (evt_cnt >= EVT_LEN)
		evt_cnt = 0;

OUT:
	spin_unlock_irqrestore(&clk_trace_lock, flags);
}

void dump_clk_event(void)
{
	unsigned long flags = 0;
	int i;

	spin_lock_irqsave(&clk_trace_lock, flags);

	pr_notice("first idx: %d\n", evt_cnt);
	for (i = 0; i < EVT_LEN; i += 5)
		pr_notice("clk_evt[%d] = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
				i,
				clk_event[i],
				clk_event[i + 1],
				clk_event[i + 2],
				clk_event[i + 3],
				clk_event[i + 4]);

	spin_unlock_irqrestore(&clk_trace_lock, flags);
}

/*
 * clkchk dump_regs
 */

#define REGBASE_V(_phys, _id_name, _pg, _pn) { .phys = _phys, .id = _id_name,	\
		.name = #_id_name, .pg = _pg, .pn = _pn}

static struct regbase rb[] = {
	[cksys_reg] = REGBASE_V(0x10000000, cksys_reg, PD_NULL, CLK_NULL),
	[infra_infracfg_ao_reg] = REGBASE_V(0x10001000, infra_infracfg_ao_reg, PD_NULL, CLK_NULL),
	[apmixed] = REGBASE_V(0x1000C000, apmixed, PD_NULL, CLK_NULL),
	[pericfg_ao_reg] = REGBASE_V(0x11036000, pericfg_ao_reg, PD_NULL, CLK_NULL),
	[afe] = REGBASE_V(0x11050000, afe, MT6881_CHK_PD_AUDIO, CLK_NULL),
	[ufsao] = REGBASE_V(0x112b8000, ufsao, PD_NULL, CLK_NULL),
	[ufspdn] = REGBASE_V(0x112bb000, ufspdn, PD_NULL, CLK_NULL),
	[imp_iic_top_wrap_s] = REGBASE_V(0x11D78000, imp_iic_top_wrap_s, PD_NULL, "cksys_i2c_sel"),
	[imp_iic_top_wrap_w] = REGBASE_V(0x11E05000, imp_iic_top_wrap_w, PD_NULL, "cksys_i2c_sel"),
	[mipi_csi_top_ctrl_0] = REGBASE_V(0x11ca0000, mipi_csi_top_ctrl_0, MT6881_CHK_PD_CSI_RX, CLK_NULL),
	[mm] = REGBASE_V(0x14000000, mm, MT6881_CHK_PD_DIS0, CLK_NULL),
	[img] = REGBASE_V(0x15010000, img, MT6881_CHK_PD_ISP_MAIN, CLK_NULL),
	[dip_top_dip1] = REGBASE_V(0x15100000, dip_top_dip1, MT6881_CHK_PD_ISP_DIP1, CLK_NULL),
	[dip_nr1_dip1] = REGBASE_V(0x15120000, dip_nr1_dip1, MT6881_CHK_PD_ISP_DIP1, CLK_NULL),
	[dip_nr2_dip1] = REGBASE_V(0x15150000, dip_nr2_dip1, MT6881_CHK_PD_ISP_DIP1, CLK_NULL),
	[wpe_eis_dip1] = REGBASE_V(0x15200000, wpe_eis_dip1, MT6881_CHK_PD_ISP_DIP1, CLK_NULL),
	[wpe_tnr_dip1] = REGBASE_V(0x15500000, wpe_tnr_dip1, MT6881_CHK_PD_ISP_DIP1, CLK_NULL),
	[traw_dip1] = REGBASE_V(0x15700000, traw_dip1, MT6881_CHK_PD_ISP_DIP1, CLK_NULL),
	[traw_cap_dip1] = REGBASE_V(0x15740000, traw_cap_dip1, MT6881_CHK_PD_ISP_DIP1, CLK_NULL),
	[img_v] = REGBASE_V(0x15780000, img_v, MT6881_CHK_PD_ISP_VCORE, CLK_NULL),
	[vde2] = REGBASE_V(0x1602f000, vde2, MT6881_CHK_PD_VDE0, CLK_NULL),
	[ven1] = REGBASE_V(0x17000000, ven1, MT6881_CHK_PD_VEN0, CLK_NULL),
	[spm] = REGBASE_V(0x1C001000, spm, PD_NULL, CLK_NULL),
	[vlpcfg_reg_bus] = REGBASE_V(0x1C00C000, vlpcfg_reg_bus, PD_NULL, CLK_NULL),
	[vlp_cksys_top] = REGBASE_V(0x1C012000, vlp_cksys_top, PD_NULL, CLK_NULL),
	[ssr_top] = REGBASE_V(0x1E200000, ssr_top, PD_NULL, CLK_NULL),
	[cam_m] = REGBASE_V(0x1a010000, cam_m, MT6881_CHK_PD_CAM_MAIN, CLK_NULL),
	[cam_mr] = REGBASE_V(0x1a680000, cam_mr, MT6881_CHK_PD_CAM_MAIN, CLK_NULL),
	[cam_ra] = REGBASE_V(0x1a7d0000, cam_ra, MT6881_CHK_PD_CAM_SUBA, CLK_NULL),
	[camsys_rmsa] = REGBASE_V(0x1a7f0000, camsys_rmsa, MT6881_CHK_PD_CAM_SUBA, CLK_NULL),
	[cam_ya] = REGBASE_V(0x1a810000, cam_ya, MT6881_CHK_PD_CAM_SUBA, CLK_NULL),
	[cam_rb] = REGBASE_V(0x1a9d0000, cam_rb, MT6881_CHK_PD_CAM_SUBB, CLK_NULL),
	[camsys_rmsb] = REGBASE_V(0x1a9f0000, camsys_rmsb, MT6881_CHK_PD_CAM_SUBB, CLK_NULL),
	[cam_yb] = REGBASE_V(0x1aa10000, cam_yb, MT6881_CHK_PD_CAM_SUBB, CLK_NULL),
	[cam_v] = REGBASE_V(0x1b80d000, cam_v, MT6881_CHK_PD_CAM_VCORE, CLK_NULL),
	[mminfra_config] = REGBASE_V(0x1e800000, mminfra_config, MT6881_CHK_PD_MM_INFRA, CLK_NULL),
	[mdp] = REGBASE_V(0x1f000000, mdp, MT6881_CHK_PD_DIS0, CLK_NULL),
	[hwv] = REGBASE_V(0x10320000, hwv, PD_NULL, CLK_NULL),
	{},
};

#define REGNAME(_base, _ofs, _name)	\
	{ .base = &rb[_base], .id = _base, .ofs = _ofs, .name = #_name }

static struct regname rn[] = { //FIXME
	/* CKSYS_REG register */
	REGNAME(cksys_reg, 0x0010, CLK_CFG_0),
	REGNAME(cksys_reg, 0x0020, CLK_CFG_1),
	REGNAME(cksys_reg, 0x0030, CLK_CFG_2),
	REGNAME(cksys_reg, 0x0040, CLK_CFG_3),
	REGNAME(cksys_reg, 0x0050, CLK_CFG_4),
	REGNAME(cksys_reg, 0x0060, CLK_CFG_5),
	REGNAME(cksys_reg, 0x0070, CLK_CFG_6),
	REGNAME(cksys_reg, 0x0080, CLK_CFG_7),
	REGNAME(cksys_reg, 0x0090, CLK_CFG_8),
	REGNAME(cksys_reg, 0x00A0, CLK_CFG_9),
	REGNAME(cksys_reg, 0x00B0, CLK_CFG_10),
	REGNAME(cksys_reg, 0x00C0, CLK_CFG_11),
	REGNAME(cksys_reg, 0x00D0, CLK_CFG_12),
	REGNAME(cksys_reg, 0x00E0, CLK_CFG_13),
	REGNAME(cksys_reg, 0x00F0, CLK_CFG_14),
	REGNAME(cksys_reg, 0x0100, CLK_CFG_15),
	REGNAME(cksys_reg, 0x0110, CLK_CFG_16),
	REGNAME(cksys_reg, 0x0120, CLK_CFG_17),
	REGNAME(cksys_reg, 0x0130, CLK_CFG_18),
	REGNAME(cksys_reg, 0x0140, CLK_CFG_19),
	REGNAME(cksys_reg, 0x0320, CLK_AUDDIV_0),
	REGNAME(cksys_reg, 0x0328, CLK_AUDDIV_2),
	REGNAME(cksys_reg, 0x0334, CLK_AUDDIV_3),
	REGNAME(cksys_reg, 0x0338, CLK_AUDDIV_4),
	REGNAME(cksys_reg, 0x033C, CLK_AUDDIV_5),
	REGNAME(cksys_reg, 0x320, CLK_AUDDIV_0),
	/* INFRA_INFRACFG_AO_REG register */
	REGNAME(infra_infracfg_ao_reg, 0x6C, HRE_INFRA_BUS_CTRL),
	REGNAME(infra_infracfg_ao_reg, 0x94, MODULE_CG_1),
	REGNAME(infra_infracfg_ao_reg, 0xAC, MODULE_CG_2),
	REGNAME(infra_infracfg_ao_reg, 0xC8, MODULE_CG_3),
	REGNAME(infra_infracfg_ao_reg, 0xE8, MODULE_CG_4),
	/* INFRA_INFRACFG_AO_REG_BUS register */
	REGNAME(infra_infracfg_ao_reg, 0x0C50, INFRASYS_PROTECT_EN_STA_1),
	REGNAME(infra_infracfg_ao_reg, 0x0C5C, INFRASYS_PROTECT_RDY_STA_1),
	REGNAME(infra_infracfg_ao_reg, 0x0C90, MCU_CONNSYS_PROTECT_EN_STA_0),
	REGNAME(infra_infracfg_ao_reg, 0x0C9C, MCU_CONNSYS_PROTECT_RDY_STA_0),
	REGNAME(infra_infracfg_ao_reg, 0x0C40, INFRASYS_PROTECT_EN_STA_0),
	REGNAME(infra_infracfg_ao_reg, 0x0C4C, INFRASYS_PROTECT_RDY_STA_0),
	REGNAME(infra_infracfg_ao_reg, 0x0C80, PERISYS_PROTECT_EN_STA_0),
	REGNAME(infra_infracfg_ao_reg, 0x0C8C, PERISYS_PROTECT_RDY_STA_0),
	REGNAME(infra_infracfg_ao_reg, 0x0C10, MMSYS_PROTECT_EN_STA_0),
	REGNAME(infra_infracfg_ao_reg, 0x0C1C, MMSYS_PROTECT_RDY_STA_0),
	REGNAME(infra_infracfg_ao_reg, 0x0C20, MMSYS_PROTECT_EN_STA_1),
	REGNAME(infra_infracfg_ao_reg, 0x0C2C, MMSYS_PROTECT_RDY_STA_1),
	/* APMIXEDSYS register */
	REGNAME(apmixed, 0x0024, APLL1_TUNER_CON0),
	REGNAME(apmixed, 0x0008, AP_PLL_CON3),
	REGNAME(apmixed, 0x0028, APLL2_TUNER_CON0),
	REGNAME(apmixed, 0x0008, AP_PLL_CON3),
	/* PERICFG_AO_REG register */
	REGNAME(pericfg_ao_reg, 0x10, PERI_CG_0),
	REGNAME(pericfg_ao_reg, 0x14, PERI_CG_1),
	REGNAME(pericfg_ao_reg, 0x18, PERI_CG_2),
	/* AFE register */
	REGNAME(afe, 0x1204, AFE_AUD_PAD_TOP_CFG0),
	REGNAME(afe, 0x0, AUDIO_TOP_0),
	REGNAME(afe, 0x4, AUDIO_TOP_1),
	REGNAME(afe, 0x8, AUDIO_TOP_2),
	REGNAME(afe, 0xC, AUDIO_TOP_3),
	REGNAME(afe, 0x10, AUDIO_TOP_4),
	REGNAME(afe, 0x70, AUDIO_TOP_5),
	REGNAME(afe, 0x1C5C, ETDM67_PADTOP),
	/* UFSCFG_AO register */
	REGNAME(ufsao, 0x4, UFS_AO_CG_0),
	REGNAME(ufsao, 0x50, UFS_AO2FE_SLPPROT_EN),
	REGNAME(ufsao, 0x5c, UFS_AO2FE_SLPPROT_RDY_STA),
	/* UFSCFG_PDN register */
	REGNAME(ufspdn, 0x4, UFS_PDN_CG_0),
	/* IMP_IIC_TOP_WRAP_S register */
	REGNAME(imp_iic_top_wrap_s, 0xE00, AP_CLOCK_CG),
	/* IMP_IIC_TOP_WRAP_W register */
	REGNAME(imp_iic_top_wrap_w, 0xE00, AP_CLOCK_CG),
	/* MIPI_CSI_TOP_CTRL_0 register */
	REGNAME(mipi_csi_top_ctrl_0, 0x4, CSI_CSR_TOP_CK_EN),
	/* DISPSYS_CONFIG register */
	REGNAME(mm, 0x100, MMSYS_CG_0),
	REGNAME(mm, 0x110, MMSYS_CG_1),
	/* IMGSYS_MAIN register */
	REGNAME(img, 0x54, IMG_IPE_CG),
	REGNAME(img, 0x0, IMG_MAIN_CG0),
	REGNAME(img, 0xC, IMG_MAIN_CG1),
	/* DIP_TOP_DIP1 register */
	REGNAME(dip_top_dip1, 0x0, MACRO_CG),
	/* DIP_NR1_DIP1 register */
	REGNAME(dip_nr1_dip1, 0x0, MACRO_CG),
	/* DIP_NR2_DIP1 register */
	REGNAME(dip_nr2_dip1, 0x0, MACRO_CG),
	/* WPE_EIS_DIP1 register */
	REGNAME(wpe_eis_dip1, 0x0, MACRO_CG),
	/* WPE_TNR_DIP1 register */
	REGNAME(wpe_tnr_dip1, 0x0, MACRO_CG),
	/* TRAW_DIP1 register */
	REGNAME(traw_dip1, 0x0, MACRO_CG),
	/* TRAW_CAP_DIP1 register */
	REGNAME(traw_cap_dip1, 0x0, MACRO_CG),
	/* IMG_VCORE_D1A register */
	REGNAME(img_v, 0x0, IMG_VCORE_CG_0),
	/* VDEC_GCON_BASE register */
	REGNAME(vde2, 0x8, LARB_CKEN_CON),
	REGNAME(vde2, 0x0, VDEC_CKEN),
	/* VENC_GCON register */
	REGNAME(ven1, 0x0, VENCSYS_CG),
	/* SPM register */
	REGNAME(spm, 0xE04, CONN_PWR_CON),
	REGNAME(spm, 0xF40, PWR_STATUS),
	REGNAME(spm, 0xF44, PWR_STATUS_2ND),
	REGNAME(spm, 0xE10, UFS0_PWR_CON),
	REGNAME(spm, 0xE14, UFS0_PHY_PWR_CON),
	REGNAME(spm, 0xE18, AUDIO_PWR_CON),
	REGNAME(spm, 0xE28, ISP_MAIN_PWR_CON),
	REGNAME(spm, 0xE2C, ISP_DIP1_PWR_CON),
	REGNAME(spm, 0xE34, ISP_VCORE_PWR_CON),
	REGNAME(spm, 0xE38, VDE0_PWR_CON),
	REGNAME(spm, 0xE40, VEN0_PWR_CON),
	REGNAME(spm, 0xE48, CAM_MAIN_PWR_CON),
	REGNAME(spm, 0xE50, CAM_SUBA_PWR_CON),
	REGNAME(spm, 0xE54, CAM_SUBB_PWR_CON),
	REGNAME(spm, 0xE5C, CAM_VCORE_PWR_CON),
	REGNAME(spm, 0xE70, DIS0_PWR_CON),
	REGNAME(spm, 0xE78, MM_INFRA_PWR_CON),
	REGNAME(spm, 0xE7C, MM_PROC_PWR_CON),
	REGNAME(spm, 0xE9C, CSI_RX_PWR_CON),
	REGNAME(spm, 0xEA0, SSRSYS_PWR_CON),
	REGNAME(spm, 0xEA8, SSUSB_PWR_CON),
	/* VLPCFG_REG_BUS register */
	REGNAME(vlpcfg_reg_bus, 0x0210, VLP_TOPAXI_PROTECTEN),
	REGNAME(vlpcfg_reg_bus, 0x0220, VLP_TOPAXI_PROTECTEN_STA1),
	/* VLP_CKSYS_TOP register */
	REGNAME(vlp_cksys_top, 0x0008, VLP_CLK_CFG_0),
	REGNAME(vlp_cksys_top, 0x0014, VLP_CLK_CFG_1),
	REGNAME(vlp_cksys_top, 0x0020, VLP_CLK_CFG_2),
	REGNAME(vlp_cksys_top, 0x002C, VLP_CLK_CFG_3),
	REGNAME(vlp_cksys_top, 0x0038, VLP_CLK_CFG_4),
	REGNAME(vlp_cksys_top, 0x0044, VLP_CLK_CFG_5),
	/* SSR_TOP_BUS register */
	REGNAME(ssr_top, 0x0090, SSR_TOP_PWR_PROTECT_EN),
	REGNAME(ssr_top, 0x0094, SSR_TOP_PWR_PROTECT_RDY),
	/* CAM_MAIN_R1A register */
	REGNAME(cam_m, 0x0, CAM_MAIN_CG_0),
	REGNAME(cam_m, 0x4C, CAM_MAIN_CG_1),
	/* CAMSYS_MRAW register */
	REGNAME(cam_mr, 0x0, CAMSYS_CG),
	/* CAMSYS_RAWA register */
	REGNAME(cam_ra, 0x0, CAMSYS_CG),
	/* CAMSYS_RMSA register */
	REGNAME(camsys_rmsa, 0x0, CAMSYS_CG),
	/* CAMSYS_YUVA register */
	REGNAME(cam_ya, 0x0, CAMSYS_CG),
	/* CAMSYS_RAWB register */
	REGNAME(cam_rb, 0x0, CAMSYS_CG),
	/* CAMSYS_RMSB register */
	REGNAME(camsys_rmsb, 0x0, CAMSYS_CG),
	/* CAMSYS_YUVB register */
	REGNAME(cam_yb, 0x0, CAMSYS_CG),
	/* CAM_VCORE_R1A register */
	REGNAME(cam_v, 0x0, CAM_VCORE_CG_0),
	/* SSR_TOP register */
	REGNAME(ssr_top, 0x0, SSR_TOP_CLK_CFG),
	/* MMINFRA_CONFIG register */
	REGNAME(mminfra_config, 0x100, MMINFRA_CG_0),
	REGNAME(mminfra_config, 0x110, MMINFRA_CG_1),
	/* MDPSYS_CONFIG register */
	REGNAME(mdp, 0x100, MDPSYS_CG_0),
	REGNAME(mdp, 0x110, MDPSYS_CG_1),
	/* HWV register // FIXME */
	REGNAME(hwv, 0x0, HW_CCF_AP_CG0_SET),
	REGNAME(hwv, 0x8, HW_CCF_AP_CG1_SET),
	REGNAME(hwv, 0x10, HW_CCF_AP_CG2_SET),
	REGNAME(hwv, 0x18, HW_CCF_AP_CG3_SET),
	REGNAME(hwv, 0x20, HW_CCF_AP_CG4_SET),
	REGNAME(hwv, 0x28, HW_CCF_AP_CG5_SET),
	REGNAME(hwv, 0x30, HW_CCF_AP_CG6_SET),
	REGNAME(hwv, 0x38, HW_CCF_AP_CG7_SET),
	REGNAME(hwv, 0x40, HW_CCF_AP_CG8_SET),
	REGNAME(hwv, 0x800 + 0x0, HW_CCF_SSPM_CG0_SET),
	REGNAME(hwv, 0x800 + 0x8, HW_CCF_SSPM_CG1_SET),
	REGNAME(hwv, 0x200 + 0x10, HW_CCF_TEE_CG2_SET),
	REGNAME(hwv, 0x200 + 0x20, HW_CCF_TEE_CG4_SET),
	REGNAME(hwv, 0x200 + 0x48, HW_CCF_TEE_CG5_SET),
	REGNAME(hwv, 0x400 + 0x18, HW_CCF_VCP_CG3_SET),
	REGNAME(hwv, 0x400 + 0x30, HW_CCF_VCP_CG6_SET),
	REGNAME(hwv, 0x400 + 0x38, HW_CCF_VCP_CG7_SET),
	REGNAME(hwv, 0x400 + 0x40, HW_CCF_VCP_CG8_SET),
	REGNAME(hwv, 0x198, HW_CCF_AP_MTCMOS_SET),
	REGNAME(hwv, 0x800 + 0x198, HW_CCF_SSPM_MTCMOS_SET),
	REGNAME(hwv, 0x1500, HW_CCF_INT_STATUS),
	REGNAME(hwv, 0x1410, HW_CCF_MTCMOS_ENABLE),
	REGNAME(hwv, 0x1414, HW_CCF_MTCMOS_STATUS),
	REGNAME(hwv, 0x141c, HW_CCF_MTCMOS_DONE),
	REGNAME(hwv, 0x1454, HW_CCF_MTCMOS_STATUS_CLR),
	REGNAME(hwv, 0x146c, HW_CCF_MTCMOS_SET_STATUS),
	REGNAME(hwv, 0x1470, HW_CCF_MTCMOS_CLR_STATUS),
	REGNAME(hwv, 0x14ac, HW_CCF_MTCMOS_FLOW_FLAG_CLR),
	REGNAME(hwv, 0x14a8, HW_CCF_MTCMOS_FLOW_FLAG_SET),
	REGNAME(hwv, 0x1800, HW_CCF_CG0_STATUS),
	REGNAME(hwv, 0x1804, HW_CCF_CG1_STATUS),
	REGNAME(hwv, 0x1808, HW_CCF_CG2_STATUS),
	REGNAME(hwv, 0x180c, HW_CCF_CG3_STATUS),
	REGNAME(hwv, 0x1810, HW_CCF_CG4_STATUS),
	REGNAME(hwv, 0x1814, HW_CCF_CG5_STATUS),
	REGNAME(hwv, 0x1818, HW_CCF_CG6_STATUS),
	REGNAME(hwv, 0x181c, HW_CCF_CG7_STATUS),
	REGNAME(hwv, 0x1820, HW_CCF_CG8_STATUS),
	REGNAME(hwv, 0x1900, HW_CCF_CG0_ENABLE),
	REGNAME(hwv, 0x1904, HW_CCF_CG1_ENABLE),
	REGNAME(hwv, 0x1908, HW_CCF_CG2_ENABLE),
	REGNAME(hwv, 0x190c, HW_CCF_CG3_ENABLE),
	REGNAME(hwv, 0x1910, HW_CCF_CG4_ENABLE),
	REGNAME(hwv, 0x1914, HW_CCF_CG5_ENABLE),
	REGNAME(hwv, 0x1918, HW_CCF_CG6_ENABLE),
	REGNAME(hwv, 0x191c, HW_CCF_CG7_ENABLE),
	REGNAME(hwv, 0x1920, HW_CCF_CG8_ENABLE),
	REGNAME(hwv, 0x1c00, HW_CCF_CG0_DONE),
	REGNAME(hwv, 0x1c04, HW_CCF_CG1_DONE),
	REGNAME(hwv, 0x1c08, HW_CCF_CG2_DONE),
	REGNAME(hwv, 0x1c0c, HW_CCF_CG3_DONE),
	REGNAME(hwv, 0x1c10, HW_CCF_CG4_DONE),
	REGNAME(hwv, 0x1c14, HW_CCF_CG5_DONE),
	REGNAME(hwv, 0x1c18, HW_CCF_CG6_DONE),
	REGNAME(hwv, 0x1c1c, HW_CCF_CG7_DONE),
	REGNAME(hwv, 0x1c20, HW_CCF_CG8_DONE),
	/* HWV history */
	REGNAME(hwv, 0x1aa0, HWV_INPUT_TIMELINE_POINTER),
	REGNAME(hwv, 0x1aa4, HWV_INPUT_TIMELINE_HISTORY_4_0),
	REGNAME(hwv, 0x1aa8, HWV_INPUT_TIMELINE_HISTORY_9_5),
	REGNAME(hwv, 0x1aac, HWV_INPUT_TIMELINE_HISTORY_14_10),
	REGNAME(hwv, 0x1ab0, HWV_INPUT_TIMELINE_HISTORY_19_15),
	REGNAME(hwv, 0x1ab4, HWV_INPUT_TIMELINE_HISTORY_24_20),
	REGNAME(hwv, 0x1ab8, HWV_INPUT_TIMELINE_HISTORY_29_25),
	REGNAME(hwv, 0x1abc, HWV_INPUT_TIMELINE_HISTORY_34_30),
	REGNAME(hwv, 0x1ac0, HWV_INPUT_TIMELINE_HISTORY_39_35),
	REGNAME(hwv, 0x1f84, HWV_IDX_POINTER),
	REGNAME(hwv, 0x1f04, HWV_ADDR_HISTORY_0),
	REGNAME(hwv, 0x1f08, HWV_ADDR_HISTORY_1),
	REGNAME(hwv, 0x1f0c, HWV_ADDR_HISTORY_2),
	REGNAME(hwv, 0x1f10, HWV_ADDR_HISTORY_3),
	REGNAME(hwv, 0x1f14, HWV_ADDR_HISTORY_4),
	REGNAME(hwv, 0x1f18, HWV_ADDR_HISTORY_5),
	REGNAME(hwv, 0x1f1c, HWV_ADDR_HISTORY_6),
	REGNAME(hwv, 0x1f20, HWV_ADDR_HISTORY_7),
	REGNAME(hwv, 0x1f24, HWV_ADDR_HISTORY_8),
	REGNAME(hwv, 0x1f28, HWV_ADDR_HISTORY_9),
	REGNAME(hwv, 0x1f2c, HWV_ADDR_HISTORY_10),
	REGNAME(hwv, 0x1f30, HWV_ADDR_HISTORY_11),
	REGNAME(hwv, 0x1f34, HWV_ADDR_HISTORY_12),
	REGNAME(hwv, 0x1f38, HWV_ADDR_HISTORY_13),
	REGNAME(hwv, 0x1f3c, HWV_ADDR_HISTORY_14),
	REGNAME(hwv, 0x1f40, HWV_ADDR_HISTORY_15),
	REGNAME(hwv, 0x1f44, HWV_DATA_HISTORY_0),
	REGNAME(hwv, 0x1f48, HWV_DATA_HISTORY_1),
	REGNAME(hwv, 0x1f4c, HWV_DATA_HISTORY_2),
	REGNAME(hwv, 0x1f50, HWV_DATA_HISTORY_3),
	REGNAME(hwv, 0x1f54, HWV_DATA_HISTORY_4),
	REGNAME(hwv, 0x1f58, HWV_DATA_HISTORY_5),
	REGNAME(hwv, 0x1f5c, HWV_DATA_HISTORY_6),
	REGNAME(hwv, 0x1f60, HWV_DATA_HISTORY_7),
	REGNAME(hwv, 0x1f64, HWV_DATA_HISTORY_8),
	REGNAME(hwv, 0x1f68, HWV_DATA_HISTORY_9),
	REGNAME(hwv, 0x1f6c, HWV_DATA_HISTORY_10),
	REGNAME(hwv, 0x1f70, HWV_DATA_HISTORY_11),
	REGNAME(hwv, 0x1f74, HWV_DATA_HISTORY_12),
	REGNAME(hwv, 0x1f7c, HWV_DATA_HISTORY_14),
	REGNAME(hwv, 0x1f78, HWV_DATA_HISTORY_13),
	REGNAME(hwv, 0x1f80, HWV_DATA_HISTORY_15),
	REGNAME(hwv, 0x1b50, HWV_VOTE_IRQ_HIS_INDEX_POINTER),
	REGNAME(hwv, 0x1b54, HWV_XPU_VOTE_IRQ_ADDR_HISTORY_0),
	REGNAME(hwv, 0x1b58, HWV_XPU_VOTE_IRQ_ADDR_HISTORY_1),
	REGNAME(hwv, 0x1b5c, HWV_XPU_VOTE_IRQ_ADDR_HISTORY_2),
	REGNAME(hwv, 0x1b60, HWV_XPU_VOTE_IRQ_ADDR_HISTORY_3),
	REGNAME(hwv, 0x1b64, HWV_XPU_VOTE_IRQ_ADDR_HISTORY_4),
	REGNAME(hwv, 0x1b68, HWV_XPU_VOTE_IRQ_ADDR_HISTORY_5),
	REGNAME(hwv, 0x1b6c, HWV_XPU_VOTE_IRQ_ADDR_HISTORY_6),
	REGNAME(hwv, 0x1b70, HWV_XPU_VOTE_IRQ_ADDR_HISTORY_7),
	REGNAME(hwv, 0x1b74, HWV_XPU_VOTE_IRQ_ADDR_HISTORY_8),
	REGNAME(hwv, 0x1b78, HWV_XPU_VOTE_IRQ_ADDR_HISTORY_9),
	REGNAME(hwv, 0x1b7c, HWV_XPU_VOTE_IRQ_ADDR_HISTORY_10),
	REGNAME(hwv, 0x1b80, HWV_XPU_VOTE_IRQ_ADDR_HISTORY_11),
	REGNAME(hwv, 0x1b84, HWV_XPU_VOTE_IRQ_ADDR_HISTORY_12),
	REGNAME(hwv, 0x1b88, HWV_XPU_VOTE_IRQ_ADDR_HISTORY_13),
	REGNAME(hwv, 0x1b8c, HWV_XPU_VOTE_IRQ_DATA_HISTORY_0),
	REGNAME(hwv, 0x1b90, HWV_XPU_VOTE_IRQ_DATA_HISTORY_1),
	REGNAME(hwv, 0x1b94, HWV_XPU_VOTE_IRQ_DATA_HISTORY_2),
	REGNAME(hwv, 0x1b98, HWV_XPU_VOTE_IRQ_DATA_HISTORY_3),
	REGNAME(hwv, 0x1b9c, HWV_XPU_VOTE_IRQ_DATA_HISTORY_4),
	REGNAME(hwv, 0x1ba0, HWV_XPU_VOTE_IRQ_DATA_HISTORY_5),
	REGNAME(hwv, 0x1ba4, HWV_XPU_VOTE_IRQ_DATA_HISTORY_6),
	REGNAME(hwv, 0x1ba8, HWV_XPU_VOTE_IRQ_DATA_HISTORY_7),
	REGNAME(hwv, 0x1bac, HWV_XPU_VOTE_IRQ_DATA_HISTORY_8),
	REGNAME(hwv, 0x1bb0, HWV_XPU_VOTE_IRQ_DATA_HISTORY_9),
	REGNAME(hwv, 0x1bb4, HWV_XPU_VOTE_IRQ_DATA_HISTORY_10),
	REGNAME(hwv, 0x1bb8, HWV_XPU_VOTE_IRQ_DATA_HISTORY_11),
	REGNAME(hwv, 0x1bbc, HWV_XPU_VOTE_IRQ_DATA_HISTORY_12),
	REGNAME(hwv, 0x1bc0, HWV_XPU_VOTE_IRQ_DATA_HISTORY_13),
	REGNAME(hwv, 0x18C8, HWV_XPU_VOTE_CG_ADDR_HISTORY_0),
	REGNAME(hwv, 0x18CC, HWV_XPU_VOTE_CG_ADDR_HISTORY_1),
	REGNAME(hwv, 0x18D0, HWV_XPU_VOTE_CG_ADDR_HISTORY_2),
	REGNAME(hwv, 0x18D4, HWV_XPU_VOTE_CG_ADDR_HISTORY_3),
	REGNAME(hwv, 0x18D8, HWV_XPU_VOTE_CG_ADDR_HISTORY_4),
	REGNAME(hwv, 0x18DC, HWV_XPU_VOTE_CG_ADDR_HISTORY_5),
	REGNAME(hwv, 0x18E0, HWV_XPU_VOTE_CG_ADDR_HISTORY_6),
	REGNAME(hwv, 0x18E4, HWV_XPU_VOTE_CG_ADDR_HISTORY_7),
	REGNAME(hwv, 0x18E8, HWV_XPU_VOTE_CG_ADDR_HISTORY_8),
	REGNAME(hwv, 0x18EC, HWV_XPU_VOTE_CG_ADDR_HISTORY_9),
	REGNAME(hwv, 0x18F0, HWV_XPU_VOTE_CG_ADDR_HISTORY_10),
	REGNAME(hwv, 0x18F4, HWV_XPU_VOTE_CG_ADDR_HISTORY_11),
	REGNAME(hwv, 0x18F8, HWV_XPU_VOTE_CG_ADDR_HISTORY_12),
	REGNAME(hwv, 0x18FC, HWV_XPU_VOTE_CG_ADDR_HISTORY_13),
	REGNAME(hwv, 0x19C8, HWV_XPU_VOTE_CG_ADDR_HISTORY_14),
	REGNAME(hwv, 0x19CC, HWV_XPU_VOTE_CG_ADDR_HISTORY_15),
	REGNAME(hwv, 0x19D0, HWV_XPU_VOTE_CG_ADDR_HISTORY_16),
	REGNAME(hwv, 0x19D4, HWV_XPU_VOTE_CG_ADDR_HISTORY_17),
	REGNAME(hwv, 0x19D8, HWV_XPU_VOTE_CG_ADDR_HISTORY_18),
	REGNAME(hwv, 0x19DC, HWV_XPU_VOTE_CG_ADDR_HISTORY_19),
	REGNAME(hwv, 0x19E0, HWV_XPU_VOTE_CG_ADDR_HISTORY_20),
	REGNAME(hwv, 0x19E4, HWV_XPU_VOTE_CG_ADDR_HISTORY_21),
	REGNAME(hwv, 0x19E8, HWV_XPU_VOTE_CG_ADDR_HISTORY_22),
	REGNAME(hwv, 0x19EC, HWV_XPU_VOTE_CG_ADDR_HISTORY_23),
	REGNAME(hwv, 0x19F0, HWV_XPU_VOTE_CG_ADDR_HISTORY_24),
	REGNAME(hwv, 0x19F4, HWV_XPU_VOTE_CG_ADDR_HISTORY_25),
	REGNAME(hwv, 0x19F8, HWV_XPU_VOTE_CG_ADDR_HISTORY_26),
	REGNAME(hwv, 0x19FC, HWV_XPU_VOTE_CG_ADDR_HISTORY_27),
	REGNAME(hwv, 0x1AC8, HWV_XPU_VOTE_CG_DATA_HISTORY_0),
	REGNAME(hwv, 0x1ACC, HWV_XPU_VOTE_CG_DATA_HISTORY_1),
	REGNAME(hwv, 0x1AD0, HWV_XPU_VOTE_CG_DATA_HISTORY_2),
	REGNAME(hwv, 0x1AD4, HWV_XPU_VOTE_CG_DATA_HISTORY_3),
	REGNAME(hwv, 0x1AD8, HWV_XPU_VOTE_CG_DATA_HISTORY_4),
	REGNAME(hwv, 0x1ADC, HWV_XPU_VOTE_CG_DATA_HISTORY_5),
	REGNAME(hwv, 0x1AE0, HWV_XPU_VOTE_CG_DATA_HISTORY_6),
	REGNAME(hwv, 0x1AE4, HWV_XPU_VOTE_CG_DATA_HISTORY_7),
	REGNAME(hwv, 0x1AE8, HWV_XPU_VOTE_CG_DATA_HISTORY_8),
	REGNAME(hwv, 0x1AEC, HWV_XPU_VOTE_CG_DATA_HISTORY_9),
	REGNAME(hwv, 0x1AF0, HWV_XPU_VOTE_CG_DATA_HISTORY_10),
	REGNAME(hwv, 0x1AF4, HWV_XPU_VOTE_CG_DATA_HISTORY_11),
	REGNAME(hwv, 0x1AF8, HWV_XPU_VOTE_CG_DATA_HISTORY_12),
	REGNAME(hwv, 0x1AFC, HWV_XPU_VOTE_CG_DATA_HISTORY_13),
	REGNAME(hwv, 0x1BC8, HWV_XPU_VOTE_CG_DATA_HISTORY_14),
	REGNAME(hwv, 0x1BCC, HWV_XPU_VOTE_CG_DATA_HISTORY_15),
	REGNAME(hwv, 0x1BD0, HWV_XPU_VOTE_CG_DATA_HISTORY_16),
	REGNAME(hwv, 0x1BD4, HWV_XPU_VOTE_CG_DATA_HISTORY_17),
	REGNAME(hwv, 0x1BD8, HWV_XPU_VOTE_CG_DATA_HISTORY_18),
	REGNAME(hwv, 0x1BDC, HWV_XPU_VOTE_CG_DATA_HISTORY_19),
	REGNAME(hwv, 0x1BE0, HWV_XPU_VOTE_CG_DATA_HISTORY_20),
	REGNAME(hwv, 0x1BE4, HWV_XPU_VOTE_CG_DATA_HISTORY_21),
	REGNAME(hwv, 0x1BE8, HWV_XPU_VOTE_CG_DATA_HISTORY_22),
	REGNAME(hwv, 0x1BEC, HWV_XPU_VOTE_CG_DATA_HISTORY_23),
	REGNAME(hwv, 0x1BF0, HWV_XPU_VOTE_CG_DATA_HISTORY_24),
	REGNAME(hwv, 0x1BF4, HWV_XPU_VOTE_CG_DATA_HISTORY_25),
	REGNAME(hwv, 0x1BF8, HWV_XPU_VOTE_CG_DATA_HISTORY_26),
	REGNAME(hwv, 0x1BFC, HWV_XPU_VOTE_CG_DATA_HISTORY_27),
	REGNAME(hwv, 0x155c, HWV_DOMAIN_KEY),
	{},
};

static const struct regname *get_all_mt6881_regnames(void)
{
	return rn;
}

static void init_regbase(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rb) - 1; i++) {
		if (!rb[i].phys)
			continue;
		if (i == hwv || i == afe)
			rb[i].virt = ioremap(rb[i].phys, 0x2000);
		else
			rb[i].virt = ioremap(rb[i].phys, 0x1000);
	}
}

u32 get_mt6881_reg_value(u32 id, u32 ofs)
{
	if (id >= chk_sys_num)
		return 0;

	return clk_readl(rb[id].virt + ofs);
}
EXPORT_SYMBOL_GPL(get_mt6881_reg_value);

/*
 * clkchk pwr_data
 */

struct pwr_data {
	const char *pvdname;
	enum chk_sys_id id;
	u32 base;
	u32 ofs;
};

/*
 * clkchk pwr_data
 */
static struct pwr_data pvd_pwr_data[] = {
	{"audiosys", afe, spm, 0x0E18},
	{"camsys_mraw", cam_mr, spm, 0x0E48},
	{"camsys_rawa", cam_ra, spm, 0x0E50},
	{"camsys_rawb", cam_rb, spm, 0x0E54},
	{"camsys_rmsa", camsys_rmsa, spm, 0x0E50},
	{"camsys_rmsb", camsys_rmsb, spm, 0x0E54},
	{"camsys_yuva", cam_ya, spm, 0x0E50},
	{"camsys_yuvb", cam_yb, spm, 0x0E54},
	{"cam_main_r1a", cam_m, spm, 0x0E48},
	{"cam_vcore_r1a", cam_v, spm, 0x0E5C},
	{"dip_nr1_dip1", dip_nr1_dip1, spm, 0x0E2C},
	{"dip_nr2_dip1", dip_nr2_dip1, spm, 0x0E2C},
	{"dip_top_dip1", dip_top_dip1, spm, 0x0E2C},
	{"mmsys0", mm, spm, 0x0E70},
	{"imgsys_main", img, spm, 0x0E28},
	{"img_vcore_d1a", img_v, spm, 0x0E34},
	{"mdpsys", mdp, spm, 0x0E70},
	{"mipi_csi_top_ctrl_0", mipi_csi_top_ctrl_0, spm, 0x0E9C},
	{"traw_cap_dip1", traw_cap_dip1, spm, 0x0E2C},
	{"traw_dip1", traw_dip1, spm, 0x0E2C},
	{"ufscfg_pdn", ufspdn, spm, 0x0E10},
	{"vdecsys", vde2, spm, 0x0E38},
	{"vencsys", ven1, spm, 0x0E40},
	{"wpe_eis_dip1", wpe_eis_dip1, spm, 0x0E2C},
	{"wpe_tnr_dip1", wpe_tnr_dip1, spm, 0x0E2C},
	{"mminfra_config", mminfra_config, spm, 0x0E78},
};

static int get_pvd_pwr_data_idx(const char *pvdname)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pvd_pwr_data); i++) {
		if (pvd_pwr_data[i].pvdname == NULL)
			continue;
		if (!strcmp(pvdname, pvd_pwr_data[i].pvdname))
			return i;
	}

	return -1;
}

/*
 * clkchk pwr_status
 */
static u32 get_pwr_status(s32 idx)
{
	if (idx < 0 || idx >= ARRAY_SIZE(pvd_pwr_data))
		return 0;

	if (pvd_pwr_data[idx].id >= chk_sys_num)
		return 0;

	return clk_readl(rb[pvd_pwr_data[idx].base].virt + pvd_pwr_data[idx].ofs);
}

static bool is_cg_chk_pwr_on(void)
{
#if CG_CHK_PWRON_ENABLE
	return true;
#endif
	return false;
}

#if CHECK_VCORE_FREQ
/*
 * clkchk vf table
 */

struct mtk_vf {
	const char *name;
	int freq_table[5];
};

#define MTK_VF_TABLE(_n, _freq0, _freq1, _freq2, _freq3, _freq4) {		\
		.name = _n,		\
		.freq_table = {_freq0, _freq1, _freq2, _freq3, _freq4},	\
	}

/*
 * Opp0 : 0p825v
 * Opp1 : 0p725v
 * Opp2 : 0p650v
 * Opp3 : 0p600v
 * Opp4 : 0p575v
 */
static struct mtk_vf vf_table[] = {
	/* Opp0, Opp1, Opp2, Opp3, Opp4 */
	MTK_VF_TABLE("cksys_axi_sel", 156000, 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("cksys_axi_peri_sel", 156000, 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("cksys_axi_ufs_sel", 156000, 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("cksys_bus_aximem_sel", 364000, 364000, 273000, 273000, 218400),
	MTK_VF_TABLE("cksys_disp0_sel", 832000, 728000, 546000, 364000, 273000),
	MTK_VF_TABLE("cksys_mminfra_sel", 832000, 624000, 458333, 364000, 273000),
	MTK_VF_TABLE("cksys_mmup_sel", 728000, 728000, 728000, 728000, 728000),
	MTK_VF_TABLE("cksys_uart_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("cksys_uart3_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("cksys_uarthub_b_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("cksys_spi0_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("cksys_spi1_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("cksys_spi2_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("cksys_spi3_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("cksys_spi4_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("cksys_spi5_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("cksys_spi6_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("cksys_spi7_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("cksys_msdc_macro_1p_sel", 416000, 416000, 416000, 416000, 416000),
	MTK_VF_TABLE("cksys_msdc30_1_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("cksys_aud_intbus_sel", 136500, 136500, 136500, 136500, 136500),
	MTK_VF_TABLE("cksys_atb_sel", 273000, 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("cksys_disp_pwm_sel", 130000, 130000, 130000, 130000, 130000),
	MTK_VF_TABLE("cksys_usb_sel", 124800, 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("cksys_usb_xhci_sel", 124800, 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("cksys_i2c_sel", 124800, 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("cksys_i2c_5_sel", 124800, 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("cksys_seninf_sel", 499200, 499200, 416000, 343750, 312000),
	MTK_VF_TABLE("cksys_seninf1_sel", 499200, 499200, 416000, 343750, 312000),
	MTK_VF_TABLE("cksys_seninf2_sel", 499200, 499200, 416000, 343750, 312000),
	MTK_VF_TABLE("cksys_seninf3_sel", 499200, 499200, 416000, 343750, 312000),
	MTK_VF_TABLE("cksys_aud_engen1_sel", 45158, 45158, 45158, 45158, 45158),
	MTK_VF_TABLE("cksys_aud_engen2_sel", 49152, 49152, 49152, 49152, 49152),
	MTK_VF_TABLE("cksys_aes_ufsfde_sel", 546000, 546000, 546000, 546000, 546000),
	MTK_VF_TABLE("cksys_ufs_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("cksys_ufs_mbist_sel", 312000, 312000, 312000, 312000, 312000),
	MTK_VF_TABLE("cksys_aud_1_sel", 180634, 180634, 180634, 180634, 180634),
	MTK_VF_TABLE("cksys_aud_2_sel", 196608, 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("cksys_dpmaif_main_sel", 436800, 436800, 416000, 364000, 273000),
	MTK_VF_TABLE("cksys_venc_sel", 624000, 624000, 458333, 312000, 249600),
	MTK_VF_TABLE("cksys_vdec_sel", 546000, 546000, 416000, 312000, 218400),
	MTK_VF_TABLE("cksys_pwm_sel", 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("cksys_audio_h_sel", 196608, 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("cksys_mcupm_sel", 218400, 218400, 218400, 218400, 218400),
	MTK_VF_TABLE("cksys_mem_sub_sel", 624000, 546000, 436800, 273000, 218400),
	MTK_VF_TABLE("cksys_mem_sub_peri_sel", 624000, 546000, 436800, 273000, 218400),
	MTK_VF_TABLE("cksys_mem_sub_ufs_sel", 546000, 546000, 436800, 273000, 218400),
	MTK_VF_TABLE("cksys_emisys_sel", 728000, 728000, 436800, 364000, 242667),
	MTK_VF_TABLE("cksys_dsi_occ_sel", 312000, 312000, 312000, 249600, 208000),
	MTK_VF_TABLE("cksys_ap2conn_host_sel", 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("cksys_msdc_1p_rx_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("cksys_dsp_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("cksys_md_emi_sel", 546000, 546000, 546000, 546000, 546000),
	MTK_VF_TABLE("cksys_ssr_pka_sel", 436800, 436800, 364000, 273000, 136500),
	MTK_VF_TABLE("cksys_ssr_dma_sel", 436800, 436800, 312000, 273000, 136500),
	MTK_VF_TABLE("cksys_ssr_kdf_sel", 312000, 312000, 312000, 273000, 136500),
	MTK_VF_TABLE("cksys_ssr_rng_sel", 273000, 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("cksys_ssr_pqc_sel", 546000, 436800, 312000, 312000, 26000),
	MTK_VF_TABLE("cksys_mfg_ref_sel", 156000, 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("cksys_mfg_eb_sel", 218400, 218400, 218400, 218400, 218400),
	MTK_VF_TABLE("cksys_spis0_b_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("cksys_spis1_b_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("cksys_spis0_deglitch_sel", 416000, 416000, 416000, 416000, 416000),
	MTK_VF_TABLE("cksys_spis1_deglitch_sel", 416000, 416000, 416000, 416000, 416000),
	MTK_VF_TABLE("cksys_tl_sel", 218400, 218400, 218400, 218400, 218400),
	MTK_VF_TABLE("cksys_pextp_mbist_sel", 249600, 249600, 249600, 249600, 249600),
	MTK_VF_TABLE("cksys_usb_frmcnt_sel", 48000, 48000, 48000, 48000, 48000),
	MTK_VF_TABLE("cksys_armpll_div_pll1_sel", 1092000, 1092000, 1092000, 1092000, 1092000),
	MTK_VF_TABLE("cksys_camtg0_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("cksys_camtg1_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("cksys_camtg2_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("cksys_camtg3_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("cksys_camtg4_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("cksys_camtg5_sel", 52000, 52000, 52000, 52000, 52000),
	{},
};
#endif

static const char *get_vf_name(int id)
{
#if CHECK_VCORE_FREQ
	if (id < 0) {
		pr_err("[%s]Negative index detected\n", __func__);
		return NULL;
	}

	return vf_table[id].name;

#else
	return NULL;
#endif
}

static int get_vf_opp(int id, int opp)
{
#if CHECK_VCORE_FREQ
	if (id < 0 || opp < 0) {
		pr_err("[%s]Negative index detected\n", __func__);
		return 0;
	}

	return vf_table[id].freq_table[opp];
#else
	return 0;
#endif
}

static u32 get_vf_num(void)
{
#if CHECK_VCORE_FREQ
	return ARRAY_SIZE(vf_table) - 1;
#else
	return 0;
#endif
}

static int get_vcore_opp(void)
{
#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER) && CHECK_VCORE_FREQ
	return mtk_dvfsrc_query_opp_info(MTK_DVFSRC_SW_REQ_VCORE_OPP);
#else
	return VCORE_NULL;
#endif
}

static unsigned int reg_dump_addr[ARRAY_SIZE(rn) - 1];
static unsigned int reg_dump_val[ARRAY_SIZE(rn) - 1];
static bool reg_dump_valid[ARRAY_SIZE(rn) - 1];

void set_subsys_reg_dump_mt6881(enum chk_sys_id id[])
{
	const struct regname *rns = &rn[0];
	struct clk *clk = NULL;
	int i, j, k;

	for (i = 0; i < ARRAY_SIZE(rn) - 1; i++, rns++) {
		reg_dump_addr[i] = 0;
		reg_dump_val[i] = 0;
		reg_dump_valid[i] = false;
		int pwr_idx = PD_NULL;

		if (!is_valid_reg(ADDR(rns)))
			continue;

		for (j = 0; id[j] != chk_sys_num; j++) {
			/* filter out the subsys that we don't want */
			if (rns->id == id[j])
				break;
		}

		if (id[j] == chk_sys_num)
			continue;

		for (k = 0; k < ARRAY_SIZE(pvd_pwr_data); k++) {
			if (pvd_pwr_data[k].id == id[j]) {
				pwr_idx = k;
				break;
			}
		}

		if (pwr_idx != PD_NULL)
			if (!pwr_hw_is_on(PWR_CON_STA, pwr_idx))
				continue;

		if (rns->base->pn != CLK_NULL) {
			clk = clk_chk_lookup(rns->base->pn);
			if(!(clk && __clk_is_enabled(clk)))
				continue;
		}

		reg_dump_addr[i] = PHYSADDR(rns);
		reg_dump_val[i] = clk_readl(ADDR(rns));
		/* record each register dump index validation */
		reg_dump_valid[i] = true;
	}
}
EXPORT_SYMBOL_GPL(set_subsys_reg_dump_mt6881);

void get_subsys_reg_dump_mt6881(void)
{
	const struct regname *rns = &rn[0];
	int i;

	for (i = 0; i < ARRAY_SIZE(rn) - 1; i++, rns++) {
		if (reg_dump_valid[i])
			pr_info("%-18s: [0x%08x] = 0x%08x\n",
					rns->name, reg_dump_addr[i], reg_dump_val[i]);
	}
}
EXPORT_SYMBOL_GPL(get_subsys_reg_dump_mt6881);

void print_subsys_reg_mt6881(enum chk_sys_id id)
{
	struct regbase *rb_dump;
	const struct regname *rns = &rn[0];
	int pwr_idx = PD_NULL;
	struct clk *clk = NULL;
	int i;

	if (id >= chk_sys_num) {
		pr_info("wrong id:%d\n", id);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(pvd_pwr_data); i++) {
		if (pvd_pwr_data[i].id == id) {
			pwr_idx = i;
			break;
		}
	}

	rb_dump = &rb[id];

	for (i = 0; i < ARRAY_SIZE(rn) - 1; i++, rns++) {
		if (!is_valid_reg(ADDR(rns)))
			return;

		/* filter out the subsys that we don't want */
		if (rns->base != rb_dump)
			continue;

		if ((pwr_idx != PD_NULL && !pwr_hw_is_on(PWR_CON_STA, pwr_idx))) {
			pr_info("pvd_pwr_data[%d] mtcmos off\n", pwr_idx);
			break;
		}

		if (rns->base->pn != CLK_NULL) {
			clk = clk_chk_lookup(rns->base->pn);
			if(!(clk && __clk_is_enabled(clk)))
				break;
		}

		pr_info("pvd_pwr_data[%d] mtcmos on\n", pwr_idx);
		pr_info("%-18s: [0x%08x] = 0x%08x\n",
				rns->name, PHYSADDR(rns), clk_readl(ADDR(rns)));
	}
}
EXPORT_SYMBOL_GPL(print_subsys_reg_mt6881);

void clkchk_debug_dump_mt6881(enum chk_sys_id id[],
		char *exception_name, bool trigger_vcp_dump, bool trigger_bugon)
{
	const struct fmeter_clk *fclks;

	fclks = mt_get_fmeter_clks();
	for (; fclks != NULL && fclks->type != FT_NULL; fclks++) {
		if (fclks->type != VLPCK && fclks->type != SUBSYS)
			pr_notice("[%s] %d khz\n", fclks->name,
				mt_get_fmeter_freq(fclks->id, fclks->type));
	}

	dump_clk_event();
	pdchk_dump_trace_evt();

	set_subsys_reg_dump_mt6881(id);
	get_subsys_reg_dump_mt6881();

	/* flag set false when trigger by adb cmd to avoid system abnormal */
	if (clkchk_bug_on_flag) {
		if (trigger_vcp_dump) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
			vcp_cmd_ex(HWCCF_FEATURE_ID, VCP_DUMP, exception_name);
#endif
			mdelay(10);
		}

		if (trigger_bugon)
			BUG_ON(1);
	}
}
EXPORT_SYMBOL_GPL(clkchk_debug_dump_mt6881);

/* debug dump register */
static enum chk_sys_id debug_dump_id[] = {
	spm,
	infra_infracfg_ao_reg,
	vlpcfg_reg_bus,
	cksys_reg,
	apmixed,
	vlp_cksys_top,
	hwv,
	chk_sys_num,
};

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
static void devapc_dump(void)
{
	clkchk_debug_dump_mt6881(debug_dump_id, "clk_devapc", false, false);
}
static struct devapc_vio_callbacks devapc_vio_handle = {
	.id = DEVAPC_SUBSYS_CLKMGR,
	.debug_dump = devapc_dump,
};
#endif


#ifdef CONFIG_MTK_SERROR_HOOK
static void serror_dump(void)
{
	clkchk_debug_dump_mt6881(debug_dump_id, "clk_serror", false, false);
}

static void clkchk_arm64_serror_panic_hook(void *data,
		struct pt_regs *regs, unsigned long esr)
{
	serror_dump();
}
#endif

#if BYPASS_SUSPEND_CLK_PWR_CHK
static const char * const off_pll_names[] = {
	NULL
};

static const char * const notice_pll_names[] = {
	"mainpll",
	"univpll",
	"msdcpll",
	"mmpll",
	"emipll",
	"apll1",
	"apll2",
	"tvdpll",
	NULL
};

static const char * const bypass_pll_name[] = {
	NULL
};
#else
static const char * const off_pll_names[] = {
	"univpll",
	"msdcpll",
	"mmpll",
	"tvdpll",
	NULL
};

static const char * const notice_pll_names[] = {
	"apll1",
	"apll2",
	NULL
};

static const char * const bypass_pll_name[] = {
	"univpll",
	NULL
};
#endif

static const char * const *get_off_pll_names(void)
{
	return off_pll_names;
}

static const char * const *get_notice_pll_names(void)
{
	return notice_pll_names;
}

static const char * const *get_bypass_pll_name(void)
{
	return bypass_pll_name;
}

static bool is_pll_chk_bug_on(void)
{
#if (BUG_ON_CHK_ENABLE) && (IS_ENABLED(CONFIG_MTK_CLKMGR_DEBUG))
	return true;
#endif
	return false;
}

static const char * const off_mux_names[] = {
	"cksys_disp0_sel",
	"cksys_mminfra_sel",
	"cksys_mmup_sel",
	"cksys_uart_sel",
	"cksys_uart3_sel",
	"cksys_spi0_sel",
	"cksys_spi1_sel",
	"cksys_spi2_sel",
	"cksys_spi3_sel",
	"cksys_spi4_sel",
	"cksys_spi5_sel",
	"cksys_spi6_sel",
	"cksys_spi7_sel",
	"cksys_msdc_macro_1p_sel",
	"cksys_msdc30_1_sel",
	"cksys_aud_intbus_sel",
	"cksys_i2c_sel",
	"cksys_i2c_5_sel",
	"cksys_seninf_sel",
	"cksys_seninf1_sel",
	"cksys_seninf2_sel",
	"cksys_seninf3_sel",
	"cksys_aes_ufsfde_sel",
	"cksys_ufs_sel",
	"cksys_ufs_mbist_sel",
	"cksys_dpmaif_main_sel",
	"cksys_venc_sel",
	"cksys_vdec_sel",
	"cksys_pwm_sel",
	"cksys_dsi_occ_sel",
	"cksys_img1_sel",
	"cksys_ipe_sel",
	"cksys_cam_sel",
	"cksys_camtm_sel",
	"cksys_ssr_pka_sel",
	"cksys_ssr_dma_sel",
	"cksys_ssr_kdf_sel",
	"cksys_ssr_rng_sel",
	"cksys_ssr_pqc_sel",
	"cksys_usb_frmcnt_sel",
	"cksys_camtg0_sel",
	"cksys_camtg1_sel",
	"cksys_camtg2_sel",
	"cksys_camtg3_sel",
	"cksys_camtg4_sel",
	"cksys_camtg5_sel",
	"vlp_scp_sel",
	"vlp_pwm_vlp_sel",
	NULL
};


static const char * const notice_mux_names[] = {
	"cksys_aud_engen1_sel",
	"cksys_aud_engen2_sel",
	"cksys_aud_1_sel",
	"cksys_aud_2_sel",
	"cksys_audio_h_sel",
	NULL
};

static const char * const bypass_mux_name[] = {
	"cksys_usb_sel",
	"cksys_usb_xhci_sel",
	NULL
};

static const char * const *get_off_mux_names(void)
{
	return off_mux_names;
}

static const char * const *get_notice_mux_names(void)
{
	return notice_mux_names;
}

static const char * const *get_bypass_mux_name(void)
{
	return bypass_mux_name;
}

static bool is_suspend_retry_stop(bool reset_cnt)
{
	if (reset_cnt == true) {
		suspend_cnt = 0;
		return true;
	}

	suspend_cnt++;
	pr_notice("%s: suspend cnt: %d\n", __func__, suspend_cnt);

	if (suspend_cnt < 2)
		return false;

	return true;
}

static void dump_bus_reg(struct regmap *regmap, u32 ofs)
{
	clkchk_debug_dump_mt6881(debug_dump_id, "hwv_cg_timeout", false, true);
}

static void dump_pll_reg(bool bug_on)
{
	clkchk_debug_dump_mt6881(debug_dump_id, "pll_abnormal", false, bug_on);
}

static void external_dump(void)
{
	clkchk_debug_dump_mt6881(debug_dump_id, NULL, false, false);
}

static void check_hwv_irq_sta(void)
{
	clkchk_debug_dump_mt6881(debug_dump_id, "hwv_irq", false, false);
}

static void cg_timeout_handle(struct regmap *regmap, u32 id, u32 shift)
{
	dump_bus_reg(NULL, 0);
}

static void verify_debug_flow(void)
{
	clkchk_bug_on_flag = false;
	devapc_dump();
#ifdef CONFIG_MTK_SERROR_HOOK
	serror_dump();
#endif
	dump_pll_reg(false);
	check_hwv_irq_sta();
	external_dump();
	clkchk_bug_on_flag = true;
}

/*
 * init functions
 */

static struct clkchk_ops clkchk_mt6881_ops = {
	.get_all_regnames = get_all_mt6881_regnames,
	.get_pvd_pwr_data_idx = get_pvd_pwr_data_idx,
	.get_pwr_status = get_pwr_status,
	.is_cg_chk_pwr_on = is_cg_chk_pwr_on,
	.get_off_pll_names = get_off_pll_names,
	.get_notice_pll_names = get_notice_pll_names,
	.get_bypass_pll_name = get_bypass_pll_name,
	.is_pll_chk_bug_on = is_pll_chk_bug_on,
	.get_off_mux_names = get_off_mux_names,
	.get_notice_mux_names = get_notice_mux_names,
	.get_bypass_mux_name = get_bypass_mux_name,
	.get_vf_name = get_vf_name,
	.get_vf_opp = get_vf_opp,
	.get_vf_num = get_vf_num,
	.get_vcore_opp = get_vcore_opp,
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
	.devapc_dump = devapc_dump,
#endif
	.trace_clk_event = trace_clk_event,
	.dump_bus_reg = dump_bus_reg,
	.dump_pll_reg = dump_pll_reg,
	.external_dump = external_dump,
	.trace_clk_event = trace_clk_event,
	.check_hwv_irq_sta = check_hwv_irq_sta,
	.is_suspend_retry_stop = is_suspend_retry_stop,
	.cg_timeout_handle = cg_timeout_handle,
	.verify_debug_flow = verify_debug_flow,
};

static int clk_chk_mt6881_probe(struct platform_device *pdev)
{
#ifdef CONFIG_MTK_SERROR_HOOK
	int ret = 0;
#endif

	suspend_cnt = 0;

	init_regbase();

	set_clkchk_notify();

	set_clkchk_ops(&clkchk_mt6881_ops);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
	register_devapc_vio_callback(&devapc_vio_handle);
#endif

#ifdef CONFIG_MTK_SERROR_HOOK
	ret = register_trace_android_rvh_arm64_serror_panic(
			clkchk_arm64_serror_panic_hook, NULL);
	if (ret)
		pr_info("register android_rvh_arm64_serror_panic failed!\n");
#endif

#if CHECK_VCORE_FREQ
	mtk_clk_check_muxes();
#endif

	clkchk_hwv_irq_init(pdev);

	return 0;
}

static const struct of_device_id of_match_clkchk_mt6881[] = {
	{
		.compatible = "mediatek,mt6881-clkchk",
	}, {
		/* sentinel */
	}
};

static struct platform_driver clk_chk_mt6881_drv = {
	.probe = clk_chk_mt6881_probe,
	.driver = {
		.name = "clk-chk-mt6881",
		.owner = THIS_MODULE,
		.pm = &clk_chk_dev_pm_ops,
		.of_match_table = of_match_clkchk_mt6881,
	},
};

/*
 * init functions
 */

static int __init clkchk_mt6881_init(void)
{
	return platform_driver_register(&clk_chk_mt6881_drv);
}

static void __exit clkchk_mt6881_exit(void)
{
	platform_driver_unregister(&clk_chk_mt6881_drv);
}

subsys_initcall(clkchk_mt6881_init);
module_exit(clkchk_mt6881_exit);
MODULE_LICENSE("GPL");
