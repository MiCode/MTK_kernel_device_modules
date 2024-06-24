// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: KY Liu <ky.liu@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>

#include <dt-bindings/power/mt6899-power.h>

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
#include <devapc_public.h>
#endif

#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER)
#include <mt-plat/dvfsrc-exp.h>
#endif

#ifdef CONFIG_MTK_SERROR_HOOK
#include <trace/hooks/traps.h>
#endif

#include "clkchk.h"
#include "clkchk-mt6899.h"
#include "clk-fmeter.h"
#include "clk-mt6899-fmeter.h"

#define BUG_ON_CHK_ENABLE		0
#define CHECK_VCORE_FREQ		0
#define CG_CHK_PWRON_ENABLE		0

#define HWV_INT_PLL_TRIGGER		0x0004
#define HWV_INT_CG_TRIGGER		0x10001

#define HWV_DOMAIN_KEY			0x055C
#define HWV_IRQ_STATUS			0x0500
#define HWV_CG_SET(xpu, id)		((0x0200 * (xpu)) + (id * 0x8))
#define HWV_CG_STA(id)			(0x1800 + (id * 0x4))
#define HWV_CG_EN(id)			(0x1900 + (id * 0x4))
#define HWV_CG_XPU_DONE(xpu)		(0x1B00 + (xpu * 0x8))
#define HWV_CG_DONE(id)			(0x1C00 + (id * 0x4))
#define HWV_TIMELINE_PTR		(0x1AA0)
#define HWV_TIMELINE_HIS(idx)		(0x1AA4 + (idx / 4))
#define HWV_CG_ADDR_HIS(idx)		(0x18C8 + (idx * 4))
#define HWV_CG_ADDR_14_HIS(idx)		(0x19C8 + ((idx - 14) * 4))
#define HWV_CG_DATA_HIS(idx)		(0x1AC8 + (idx * 4))
#define HWV_CG_DATA_14_HIS(idx)		(0x1BC8 + ((idx - 14) * 4))
#define HWV_IRQ_XPU_HIS_PTR		(0x1B50)
#define HWV_IRQ_ADDR_HIS(idx)		(0x1B54 + (idx * 4))
#define HWV_IRQ_DATA_HIS(idx)		(0x1B8C + (idx * 4))

#define EVT_LEN				40
#define CLK_ID_SHIFT			0
#define CLK_STA_SHIFT			8

static DEFINE_SPINLOCK(clk_trace_lock);
static unsigned int clk_event[EVT_LEN];
static unsigned int evt_cnt, suspend_cnt;

/* xpu*/
enum {
	APMCU = 0,
	MD,
	SSPM,
	MMUP,
	SCP,
	XPU_NUM,
};

static u32 xpu_id[XPU_NUM] = {
	[APMCU] = 0,
	[MD] = 2,
	[SSPM] = 4,
	[MMUP] = 7,
	[SCP] = 9,
};

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
	CLK_AFE_CM1_CG = 12,
	CLK_AFE_CM0_CG = 13,
	CLK_AFE_STF_CG = 14,
	CLK_AFE_HW_GAIN23_CG = 15,
	CLK_AFE_HW_GAIN01_CG = 16,
	CLK_AFE_FM_I2S_CG = 17,
	CLK_AFE_MTKAIFV4_CG = 18,
	CLK_AFE_AUDIO_HOPPING_CG = 19,
	CLK_AFE_AUDIO_F26M_CG = 20,
	CLK_AFE_APLL1_CG = 21,
	CLK_AFE_APLL2_CG = 22,
	CLK_AFE_H208M_CG = 23,
	CLK_AFE_APLL_TUNER2_CG = 24,
	CLK_AFE_APLL_TUNER1_CG = 25,
	CLK_AFE_DMIC1_ADC_HIRES_TML_CG = 26,
	CLK_AFE_DMIC1_ADC_HIRES_CG = 27,
	CLK_AFE_DMIC1_TML_CG = 28,
	CLK_AFE_DMIC1_ADC_CG = 29,
	CLK_AFE_DMIC0_ADC_HIRES_TML_CG = 30,
	CLK_AFE_DMIC0_ADC_HIRES_CG = 31,
	CLK_AFE_DMIC0_TML_CG = 32,
	CLK_AFE_DMIC0_ADC_CG = 33,
	CLK_AFE_UL1_ADC_HIRES_TML_CG = 34,
	CLK_AFE_UL1_ADC_HIRES_CG = 35,
	CLK_AFE_UL1_TML_CG = 36,
	CLK_AFE_UL1_ADC_CG = 37,
	CLK_AFE_UL0_TML_CG = 38,
	CLK_AFE_UL0_ADC_CG = 39,
	CLK_AFE_ETDM_IN6_CG = 40,
	CLK_AFE_ETDM_IN4_CG = 41,
	CLK_AFE_ETDM_IN2_CG = 42,
	CLK_AFE_ETDM_IN1_CG = 43,
	CLK_AFE_ETDM_IN0_CG = 44,
	CLK_AFE_ETDM_OUT6_CG = 45,
	CLK_AFE_ETDM_OUT4_CG = 46,
	CLK_AFE_ETDM_OUT2_CG = 47,
	CLK_AFE_ETDM_OUT1_CG = 48,
	CLK_AFE_ETDM_OUT0_CG = 49,
	CLK_AFE_TDM_OUT_CG = 50,
	CLK_AFE_GENERAL3_ASRC_CG = 51,
	CLK_AFE_GENERAL2_ASRC_CG = 52,
	CLK_AFE_GENERAL1_ASRC_CG = 53,
	CLK_AFE_GENERAL0_ASRC_CG = 54,
	CLK_AFE_CONNSYS_I2S_ASRC_CG = 55,
	CLK_CAMSYS_IPE_LARB19_CG = 56,
	CLK_CAMSYS_IPE_DPE_CG = 57,
	CLK_CAMSYS_IPE_FUS_CG = 58,
	CLK_CAMSYS_IPE_GALS_CG = 59,
	CLK_CAM_MR_LARBX_CG = 60,
	CLK_CAM_MR_GALS_CG = 61,
	CLK_CAM_MR_CAMTG_CG = 62,
	CLK_CAM_MR_MRAW0_CG = 63,
	CLK_CAM_MR_MRAW1_CG = 64,
	CLK_CAM_MR_MRAW2_CG = 65,
	CLK_CAM_MR_PDA0_CG = 66,
	CLK_CAM_MR_PDA1_CG = 67,
	CLK_CAM_RA_LARBX_CG = 68,
	CLK_CAM_RA_CAM_CG = 69,
	CLK_CAM_RA_CAMTG_CG = 70,
	CLK_CAM_RA_RAW2MM_GALS_CG = 71,
	CLK_CAM_RA_YUV2RAW2MM_GALS_CG = 72,
	CLK_CAM_RB_LARBX_CG = 73,
	CLK_CAM_RB_CAM_CG = 74,
	CLK_CAM_RB_CAMTG_CG = 75,
	CLK_CAM_RB_RAW2MM_GALS_CG = 76,
	CLK_CAM_RB_YUV2RAW2MM_GALS_CG = 77,
	CLK_CAM_RC_LARBX_CG = 78,
	CLK_CAM_RC_CAM_CG = 79,
	CLK_CAM_RC_CAMTG_CG = 80,
	CLK_CAM_RC_RAW2MM_GALS_CG = 81,
	CLK_CAM_RC_YUV2RAW2MM_GALS_CG = 82,
	CLK_CAMSYS_RMSA_LARBX_CG = 83,
	CLK_CAMSYS_RMSA_CAM_CG = 84,
	CLK_CAMSYS_RMSA_CAMTG_CG = 85,
	CLK_CAMSYS_RMSB_LARBX_CG = 86,
	CLK_CAMSYS_RMSB_CAM_CG = 87,
	CLK_CAMSYS_RMSB_CAMTG_CG = 88,
	CLK_CAMSYS_RMSC_LARBX_CG = 89,
	CLK_CAMSYS_RMSC_CAM_CG = 90,
	CLK_CAMSYS_RMSC_CAMTG_CG = 91,
	CLK_CAM_YA_LARBX_CG = 92,
	CLK_CAM_YA_CAM_CG = 93,
	CLK_CAM_YA_CAMTG_CG = 94,
	CLK_CAM_YB_LARBX_CG = 95,
	CLK_CAM_YB_CAM_CG = 96,
	CLK_CAM_YB_CAMTG_CG = 97,
	CLK_CAM_YC_LARBX_CG = 98,
	CLK_CAM_YC_CAM_CG = 99,
	CLK_CAM_YC_CAMTG_CG = 100,
	CLK_CAM_MAIN_LARB13_CG = 101,
	CLK_CAM_MAIN_LARB14_CG = 102,
	CLK_CAM_MAIN_LARB27_CG = 103,
	CLK_CAM_MAIN_LARB29_CG = 104,
	CLK_CAM_MAIN_CAM_CG = 105,
	CLK_CAM_MAIN_CAM_SUBA_CG = 106,
	CLK_CAM_MAIN_CAM_SUBB_CG = 107,
	CLK_CAM_MAIN_CAM_SUBC_CG = 108,
	CLK_CAM_MAIN_CAM_MRAW_CG = 109,
	CLK_CAM_MAIN_CAMTG_CG = 110,
	CLK_CAM_MAIN_SENINF_CG = 111,
	CLK_CAM_MAIN_CAMSV_TOP_CG = 112,
	CLK_CAM_MAIN_ADLRD_CG = 113,
	CLK_CAM_MAIN_ADLWR_CG = 114,
	CLK_CAM_MAIN_FAKE_ENG_CG = 115,
	CLK_CAM_MAIN_CAM2MM0_GALS_CG = 116,
	CLK_CAM_MAIN_CAM2MM1_GALS_CG = 117,
	CLK_CAM_MAIN_CAM2SYS_GALS_CG = 118,
	CLK_CAM_MAIN_CAM2MM2_GALS_CG = 119,
	CLK_CAM_MAIN_IPS_CG = 120,
	CLK_CAM_MAIN_CAM_DPE_CG = 121,
	CLK_CAM_MAIN_CAM_ASG_CG = 122,
	CLK_CAM_MAIN_CAMSV_A_CON_1_CG = 123,
	CLK_CAM_MAIN_CAMSV_B_CON_1_CG = 124,
	CLK_CAM_MAIN_CAMSV_C_CON_1_CG = 125,
	CLK_CAM_MAIN_CAMSV_D_CON_1_CG = 126,
	CLK_CAM_MAIN_CAMSV_E_CON_1_CG = 127,
	CLK_CAM_MAIN_CAM_QOF_CON_1_CG = 128,
	CLK_CAM_MAIN_CAM_BLS_FULL_CON_1_CG = 129,
	CLK_CAM_MAIN_CAM_BLS_PART_CON_1_CG = 130,
	CLK_CAM_MAIN_CAM_BWR_CON_1_CG = 131,
	CLK_CAM_MAIN_CAM_RTCQ_CON_1_CG = 132,
	CLK_CAM_MAIN_CAM2MM0_SUB_COMMON_DCM_DIS_CG = 133,
	CLK_CAM_MAIN_CAM2MM1_SUB_COMMON_DCM_DIS_CG = 134,
	CLK_CAM_MAIN_CAM2SYS_SUB_COMMON_DCM_DIS_CG = 135,
	CLK_CAM_MAIN_CAM2MM2_SUB_COMMON_DCM_DIS_CG = 136,
	CLK_CAMV_CV_CAM2MM0_SC_DCM_DIS_CG = 137,
	CLK_CAMV_CV_MM0_SC_DCM_DIS_CG = 138,
	CLK_CAMV_CV_CAMVCORE_CG = 139,
	CLK_CAMV_CV_CAM_26M_CG = 140,
	CLK_CCU_LARB19_CON_CG = 141,
	CLK_CCU2INFRA_GALS_CON_CG = 142,
	CLK_CCUSYS_CCU0_CON_CG = 143,
	CLK_CCU2MM0_GALS_CON_CG = 144,
	CLK_DIP_NR1_DIP1_LARB_CG = 145,
	CLK_DIP_NR1_DIP1_DIP_NR1_CG = 146,
	CLK_DIP_NR2_DIP1_DIP_NR_CG = 147,
	CLK_DIP_NR2_DIP1_LARB15_CG = 148,
	CLK_DIP_NR2_DIP1_LARB39_CG = 149,
	CLK_DIP_TOP_DIP1_DIP_TOP_CG = 150,
	CLK_DIP_TOP_DIP1_DIP_TOP_GALS0_CG = 151,
	CLK_DIP_TOP_DIP1_DIP_TOP_GALS1_CG = 152,
	CLK_DIP_TOP_DIP1_DIP_TOP_GALS2_CG = 153,
	CLK_DIP_TOP_DIP1_DIP_TOP_GALS3_CG = 154,
	CLK_DIP_TOP_DIP1_LARB10_CG = 155,
	CLK_DIP_TOP_DIP1_LARB15_CG = 156,
	CLK_DIP_TOP_DIP1_LARB38_CG = 157,
	CLK_DIP_TOP_DIP1_LARB39_CG = 158,
	CLK_IMG_LARB9_CG = 159,
	CLK_IMG_TRAW0_CG = 160,
	CLK_IMG_TRAW1_CG = 161,
	CLK_IMG_DIP0_CG = 162,
	CLK_IMG_WPE0_CG = 163,
	CLK_IMG_IPE_CG = 164,
	CLK_IMG_WPE1_CG = 165,
	CLK_IMG_WPE2_CG = 166,
	CLK_IMG_ADL_LARB_CG = 167,
	CLK_IMG_ADLRD_CG = 168,
	CLK_IMG_ADLWR0_CG = 169,
	CLK_IMG_AVS_CG = 170,
	CLK_IMG_IPS_CG = 171,
	CLK_IMG_ADLWR1_CG = 172,
	CLK_IMG_ROOTCQ_CG = 173,
	CLK_IMG_BLS_CG = 174,
	CLK_IMG_SUB_COMMON0_CG = 175,
	CLK_IMG_SUB_COMMON1_CG = 176,
	CLK_IMG_SUB_COMMON2_CG = 177,
	CLK_IMG_SUB_COMMON3_CG = 178,
	CLK_IMG_SUB_COMMON4_CG = 179,
	CLK_IMG_GALS_RX_DIP0_CG = 180,
	CLK_IMG_GALS_RX_DIP1_CG = 181,
	CLK_IMG_GALS_RX_TRAW0_CG = 182,
	CLK_IMG_GALS_RX_WPE0_CG = 183,
	CLK_IMG_GALS_RX_WPE1_CG = 184,
	CLK_IMG_GALS_RX_WPE2_CG = 185,
	CLK_IMG_GALS_TRX_IPE0_CG = 186,
	CLK_IMG_GALS_TRX_IPE1_CG = 187,
	CLK_IMG26_CG = 188,
	CLK_IMG_BWR_CG = 189,
	CLK_IMG_GALS_CG = 190,
	CLK_IMG_FDVT_CG = 191,
	CLK_IMG_ME_CG = 192,
	CLK_IMG_MMG_CG = 193,
	CLK_IMG_LARB12_CG = 194,
	CLK_IMG_VCORE_GALS_DISP_CG = 195,
	CLK_IMG_VCORE_MAIN_CG = 196,
	CLK_IMG_VCORE_SUB0_CG = 197,
	CLK_IMG_VCORE_SUB1_CG = 198,
	CLK_IMG_VCORE_IMG_26M_CG = 199,
	CLK_TRAW_CAP_DIP1_TRAW_CAP_CG = 200,
	CLK_TRAW_DIP1_LARB28_CG = 201,
	CLK_TRAW_DIP1_LARB40_CG = 202,
	CLK_TRAW_DIP1_TRAW_CG = 203,
	CLK_TRAW_DIP1_GALS_CG = 204,
	CLK_WPE1_DIP1_LARB11_CG = 205,
	CLK_WPE1_DIP1_WPE_CG = 206,
	CLK_WPE1_DIP1_GALS0_CG = 207,
	CLK_WPE2_DIP1_LARB11_CG = 208,
	CLK_WPE2_DIP1_WPE_CG = 209,
	CLK_WPE2_DIP1_GALS0_CG = 210,
	CLK_WPE3_DIP1_LARB11_CG = 211,
	CLK_WPE3_DIP1_WPE_CG = 212,
	CLK_WPE3_DIP1_GALS0_CG = 213,
	CLK_MM1_DISPSYS1_CONFIG_CG = 214,
	CLK_MM1_DISP_MUTEX0_CG = 215,
	CLK_MM1_DISP_DLI_ASYNC0_CG = 216,
	CLK_MM1_DISP_DLI_ASYNC1_CG = 217,
	CLK_MM1_DISP_DLI_ASYNC2_CG = 218,
	CLK_MM1_MDP_RDMA0_CG = 219,
	CLK_MM1_DISP_R2Y0_CG = 220,
	CLK_MM1_DISP_SPLITTER0_CG = 221,
	CLK_MM1_DISP_SPLITTER1_CG = 222,
	CLK_MM1_DISP_VDCM0_CG = 223,
	CLK_MM1_DISP_DSC_WRAP0_CG = 224,
	CLK_MM1_DISP_DSC_WRAP1_CG = 225,
	CLK_MM1_DISP_DSC_WRAP2_CG = 226,
	CLK_MM1_DISP_DP_INTF0_CG = 227,
	CLK_MM1_DISP_DSI0_CG = 228,
	CLK_MM1_DISP_DSI1_CG = 229,
	CLK_MM1_DISP_DSI2_CG = 230,
	CLK_MM1_DISP_MERGE0_CG = 231,
	CLK_MM1_DISP_WDMA0_CG = 232,
	CLK_MM1_SMI_SUB_COMM0_CG = 233,
	CLK_MM1_DISP_WDMA1_CG = 234,
	CLK_MM1_DISP_WDMA2_CG = 235,
	CLK_MM1_DISP_GDMA0_CG = 236,
	CLK_MM1_DISP_DLI_ASYNC3_CG = 237,
	CLK_MM1_DISP_DLI_ASYNC4_CG = 238,
	CLK_MM1_MOD1_CG = 239,
	CLK_MM1_MOD2_CG = 240,
	CLK_MM1_MOD3_CG = 241,
	CLK_MM1_MOD4_CG = 242,
	CLK_MM1_MOD5_CG = 243,
	CLK_MM1_MOD6_CG = 244,
	CLK_MM1_MOD7_CG = 245,
	CLK_MM1_SUBSYS_CG = 246,
	CLK_MM1_DSI0_CG = 247,
	CLK_MM1_DSI1_CG = 248,
	CLK_MM1_DSI2_CG = 249,
	CLK_MM1_DP_CG = 250,
	CLK_MM1_F26M_CG = 251,
	CLK_MM_CONFIG_CG = 252,
	CLK_MM_DISP_MUTEX0_CG = 253,
	CLK_MM_DISP_AAL0_CG = 254,
	CLK_MM_DISP_AAL1_CG = 255,
	CLK_MM_DISP_C3D0_CG = 256,
	CLK_MM_DISP_C3D1_CG = 257,
	CLK_MM_DISP_CCORR0_CG = 258,
	CLK_MM_DISP_CCORR1_CG = 259,
	CLK_MM_DISP_CCORR2_CG = 260,
	CLK_MM_DISP_CCORR3_CG = 261,
	CLK_MM_DISP_CHIST0_CG = 262,
	CLK_MM_DISP_CHIST1_CG = 263,
	CLK_MM_DISP_COLOR0_CG = 264,
	CLK_MM_DISP_COLOR1_CG = 265,
	CLK_MM_DISP_DITHER0_CG = 266,
	CLK_MM_DISP_DITHER1_CG = 267,
	CLK_MM_DISP_DITHER2_CG = 268,
	CLK_MM_DLI_ASYNC0_CG = 269,
	CLK_MM_DLI_ASYNC1_CG = 270,
	CLK_MM_DLI_ASYNC2_CG = 271,
	CLK_MM_DLI_ASYNC3_CG = 272,
	CLK_MM_DLI_ASYNC4_CG = 273,
	CLK_MM_DLI_ASYNC5_CG = 274,
	CLK_MM_DLI_ASYNC6_CG = 275,
	CLK_MM_DLI_ASYNC7_CG = 276,
	CLK_MM_DLO_ASYNC0_CG = 277,
	CLK_MM_DLO_ASYNC1_CG = 278,
	CLK_MM_DLO_ASYNC2_CG = 279,
	CLK_MM_DLO_ASYNC3_CG = 280,
	CLK_MM_DLO_ASYNC4_CG = 281,
	CLK_MM_DISP_GAMMA0_CG = 282,
	CLK_MM_DISP_GAMMA1_CG = 283,
	CLK_MM_MDP_AAL0_CG = 284,
	CLK_MM_MDP_RDMA0_CG = 285,
	CLK_MM_DISP_ODDMR0_CG = 286,
	CLK_MM_DISP_POSTALIGN0_CG = 287,
	CLK_MM_DISP_POSTMASK0_CG = 288,
	CLK_MM_DISP_POSTMASK1_CG = 289,
	CLK_MM_DISP_RSZ0_CG = 290,
	CLK_MM_DISP_RSZ1_CG = 291,
	CLK_MM_DISP_SPR0_CG = 292,
	CLK_MM_DISP_TDSHP0_CG = 293,
	CLK_MM_DISP_TDSHP1_CG = 294,
	CLK_MM_DISP_WDMA1_CG = 295,
	CLK_MM_DISP_Y2R0_CG = 296,
	CLK_MM_MDP_AAL1_CG = 297,
	CLK_MM_SMI_SUB_COMM0_CG = 298,
	CLK_MM_DISP_RSZ0_MOUT_RELAY_CG = 299,
	CLK_MM_DISP_RSZ1_MOUT_RELAY_CG = 300,
	CLK_MMINFRA_AO_GCE_D_CG = 301,
	CLK_MMINFRA_AO_GCE_M_CG = 302,
	CLK_MMINFRA_AO_GCE_26M_CG = 303,
	CLK_OVLSYS_CONFIG_CG = 304,
	CLK_OVL_DISP_FAKE_ENG0_CG = 305,
	CLK_OVL_DISP_FAKE_ENG1_CG = 306,
	CLK_OVL_DISP_MUTEX0_CG = 307,
	CLK_OVL_DISP_OVL0_2L_CG = 308,
	CLK_OVL_DISP_OVL1_2L_CG = 309,
	CLK_OVL_DISP_OVL2_2L_CG = 310,
	CLK_OVL_DISP_OVL3_2L_CG = 311,
	CLK_OVL_DISP_RSZ1_CG = 312,
	CLK_OVL_MDP_RSZ0_CG = 313,
	CLK_OVL_DISP_WDMA0_CG = 314,
	CLK_OVL_DISP_UFBC_WDMA0_CG = 315,
	CLK_OVL_DISP_WDMA2_CG = 316,
	CLK_OVL_DISP_DLI_ASYNC0_CG = 317,
	CLK_OVL_DISP_DLI_ASYNC1_CG = 318,
	CLK_OVL_DISP_DLI_ASYNC2_CG = 319,
	CLK_OVL_DISP_DL0_ASYNC0_CG = 320,
	CLK_OVL_DISP_DL0_ASYNC1_CG = 321,
	CLK_OVL_DISP_DL0_ASYNC2_CG = 322,
	CLK_OVL_DISP_DL0_ASYNC3_CG = 323,
	CLK_OVL_DISP_DL0_ASYNC4_CG = 324,
	CLK_OVL_DISP_DL0_ASYNC5_CG = 325,
	CLK_OVL_DISP_DL0_ASYNC6_CG = 326,
	CLK_OVL_INLINEROT0_CG = 327,
	CLK_OVL_SMI_SUB_COMM0_CG = 328,
	CLK_OVL_DISP_Y2R0_CG = 329,
	CLK_OVL_DISP_Y2R1_CG = 330,
	CLK_OVL_DISP_OVL4_2L_CG = 331,
	CLK_IMPC_I2C10_CG = 332,
	CLK_IMPC_I2C11_CG = 333,
	CLK_IMPC_I2C12_CG = 334,
	CLK_IMPC_I2C13_CG = 335,
	CLK_IMPE_I3C4_CG = 336,
	CLK_IMPE_I3C8_CG = 337,
	CLK_IMPEN_I3C2_CG = 338,
	CLK_IMPES_I3C9_CG = 339,
	CLK_IMPN_I2C3_CG = 340,
	CLK_IMPN_I2C5_CG = 341,
	CLK_IMPS_I3C0_CG = 342,
	CLK_IMPS_I3C1_CG = 343,
	CLK_IMPS_I3C7_CG = 344,
	CLK_IMPW_I2C6_CG = 345,
	CLK_PERAOP_UART0_CG = 346,
	CLK_PERAOP_UART1_CG = 347,
	CLK_PERAOP_UART2_CG = 348,
	CLK_PERAOP_UART3_CG = 349,
	CLK_PERAOP_PWM_H_CG = 350,
	CLK_PERAOP_PWM_B_CG = 351,
	CLK_PERAOP_PWM_FB1_CG = 352,
	CLK_PERAOP_PWM_FB2_CG = 353,
	CLK_PERAOP_PWM_FB3_CG = 354,
	CLK_PERAOP_PWM_FB4_CG = 355,
	CLK_PERAOP_SPI0_B_CG = 356,
	CLK_PERAOP_SPI1_B_CG = 357,
	CLK_PERAOP_SPI2_B_CG = 358,
	CLK_PERAOP_SPI3_B_CG = 359,
	CLK_PERAOP_SPI4_B_CG = 360,
	CLK_PERAOP_SPI5_B_CG = 361,
	CLK_PERAOP_SPI6_B_CG = 362,
	CLK_PERAOP_SPI7_B_CG = 363,
	CLK_PERAOP_DMA_B_CG = 364,
	CLK_PERAOP_SSUSB0_FRMCNT_CG = 365,
	CLK_PERAOP_MSDC1_CG = 366,
	CLK_PERAOP_MSDC1_F_CG = 367,
	CLK_PERAOP_MSDC1_H_CG = 368,
	CLK_PERAOP_MSDC2_CG = 369,
	CLK_PERAOP_MSDC2_F_CG = 370,
	CLK_PERAOP_MSDC2_H_CG = 371,
	CLK_PERAOP_AUDIO_SLV_CG = 372,
	CLK_PERAOP_AUDIO_MST_CG = 373,
	CLK_PERAOP_AUDIO_INTBUS_CG = 374,
	CLK_UFSAO_UNIPRO_SYS_CG = 375,
	CLK_UFSAO_U_PHY_SAP_CG = 376,
	CLK_UFSAO_U_PHY_TOP_AHB_S_BUSCK_CG = 377,
	CLK_UFSAO_U_AO_FREE_26M_CG = 378,
	CLK_UFSPDN_UFSHCI_UFS_CG = 379,
	CLK_UFSPDN_UFSHCI_AES_CG = 380,
	CLK_UFSPDN_UFSHCI_U_AHB_CG = 381,
	CLK_UFSPDN_UFSHCI_U_AXI_CG = 382,
	CLK_UFSPDN_U_FREE_26M_CG = 383,
	CLK_MDP1_MDP_MUTEX0_CG = 384,
	CLK_MDP1_APB_BUS_CG = 385,
	CLK_MDP1_SMI0_CG = 386,
	CLK_MDP1_MDP_RDMA0_CG = 387,
	CLK_MDP1_MDP_RDMA2_CG = 388,
	CLK_MDP1_MDP_HDR0_CG = 389,
	CLK_MDP1_MDP_AAL0_CG = 390,
	CLK_MDP1_MDP_RSZ0_CG = 391,
	CLK_MDP1_MDP_TDSHP0_CG = 392,
	CLK_MDP1_MDP_COLOR0_CG = 393,
	CLK_MDP1_MDP_WROT0_CG = 394,
	CLK_MDP1_MDP_FAKE_ENG0_CG = 395,
	CLK_MDP1_MDP_DLI_ASYNC0_CG = 396,
	CLK_MDP1_APB_DB_CG = 397,
	CLK_MDP1_MDP_RSZ2_CG = 398,
	CLK_MDP1_MDP_WROT2_CG = 399,
	CLK_MDP1_MDP_DLO_ASYNC0_CG = 400,
	CLK_MDP1_MDP_BIRSZ0_CG = 401,
	CLK_MDP1_MDP_C3D0_CG = 402,
	CLK_MDP1_MDP_FG0_CG = 403,
	CLK_MDP_MUTEX0_CG = 404,
	CLK_MDP_APB_BUS_CG = 405,
	CLK_MDP_SMI0_CG = 406,
	CLK_MDP_RDMA0_CG = 407,
	CLK_MDP_RDMA2_CG = 408,
	CLK_MDP_HDR0_CG = 409,
	CLK_MDP_AAL0_CG = 410,
	CLK_MDP_RSZ0_CG = 411,
	CLK_MDP_TDSHP0_CG = 412,
	CLK_MDP_COLOR0_CG = 413,
	CLK_MDP_WROT0_CG = 414,
	CLK_MDP_FAKE_ENG0_CG = 415,
	CLK_MDP_APB_DB_CG = 416,
	CLK_MDP_BIRSZ0_CG = 417,
	CLK_MDP_C3D0_CG = 418,
	CLK_VDE2_VDEC_CKEN_CG = 419,
	CLK_VDE2_VDEC_ACTIVE_CG = 420,
	CLK_VDE2_VDEC_CKEN_ENG_CG = 421,
	CLK_VDE2_LAT_CKEN_CG = 422,
	CLK_VDE2_LAT_ACTIVE_CG = 423,
	CLK_VDE2_LAT_CKEN_ENG_CG = 424,
	CLK_VDE2_LARB1_CKEN_CG = 425,
	CLK_VDE1_VDEC_CKEN_CG = 426,
	CLK_VDE1_VDEC_ACTIVE_CG = 427,
	CLK_VDE1_VDEC_CKEN_ENG_CG = 428,
	CLK_VDE1_LAT_CKEN_CG = 429,
	CLK_VDE1_LAT_ACTIVE_CG = 430,
	CLK_VDE1_LAT_CKEN_ENG_CG = 431,
	CLK_VDE1_LARB1_CKEN_CG = 432,
	CLK_VEN1_CKE0_LARB_CG = 433,
	CLK_VEN1_CKE1_VENC_CG = 434,
	CLK_VEN1_CKE2_JPGENC_CG = 435,
	CLK_VEN1_CKE3_JPGDEC_CG = 436,
	CLK_VEN1_CKE4_JPGDEC_C1_CG = 437,
	CLK_VEN1_CKE5_GALS_CG = 438,
	CLK_VEN1_CKE6_GALS_SRAM_CG = 439,
	CLK_VEN2_CKE0_LARB_CG = 440,
	CLK_VEN2_CKE1_VENC_CG = 441,
	CLK_VEN2_CKE2_JPGENC_CG = 442,
	CLK_VEN2_CKE3_JPGDEC_CG = 443,
	CLK_VEN2_CKE5_GALS_CG = 444,
	CLK_VEN2_CKE6_GALS_SRAM_CG = 445,
	TRACE_CLK_NUM = 446,
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
	[CLK_AFE_DMIC1_ADC_HIRES_TML_CG] = "afe_dmic1_aht",
	[CLK_AFE_DMIC1_ADC_HIRES_CG] = "afe_dmic1_adc_hires",
	[CLK_AFE_DMIC1_TML_CG] = "afe_dmic1_tml",
	[CLK_AFE_DMIC1_ADC_CG] = "afe_dmic1_adc",
	[CLK_AFE_DMIC0_ADC_HIRES_TML_CG] = "afe_dmic0_aht",
	[CLK_AFE_DMIC0_ADC_HIRES_CG] = "afe_dmic0_adc_hires",
	[CLK_AFE_DMIC0_TML_CG] = "afe_dmic0_tml",
	[CLK_AFE_DMIC0_ADC_CG] = "afe_dmic0_adc",
	[CLK_AFE_UL1_ADC_HIRES_TML_CG] = "afe_ul1_aht",
	[CLK_AFE_UL1_ADC_HIRES_CG] = "afe_ul1_adc_hires",
	[CLK_AFE_UL1_TML_CG] = "afe_ul1_tml",
	[CLK_AFE_UL1_ADC_CG] = "afe_ul1_adc",
	[CLK_AFE_UL0_TML_CG] = "afe_ul0_tml",
	[CLK_AFE_UL0_ADC_CG] = "afe_ul0_adc",
	[CLK_AFE_ETDM_IN6_CG] = "afe_etdm_in6",
	[CLK_AFE_ETDM_IN4_CG] = "afe_etdm_in4",
	[CLK_AFE_ETDM_IN2_CG] = "afe_etdm_in2",
	[CLK_AFE_ETDM_IN1_CG] = "afe_etdm_in1",
	[CLK_AFE_ETDM_IN0_CG] = "afe_etdm_in0",
	[CLK_AFE_ETDM_OUT6_CG] = "afe_etdm_out6",
	[CLK_AFE_ETDM_OUT4_CG] = "afe_etdm_out4",
	[CLK_AFE_ETDM_OUT2_CG] = "afe_etdm_out2",
	[CLK_AFE_ETDM_OUT1_CG] = "afe_etdm_out1",
	[CLK_AFE_ETDM_OUT0_CG] = "afe_etdm_out0",
	[CLK_AFE_TDM_OUT_CG] = "afe_tdm_out",
	[CLK_AFE_GENERAL3_ASRC_CG] = "afe_general3_asrc",
	[CLK_AFE_GENERAL2_ASRC_CG] = "afe_general2_asrc",
	[CLK_AFE_GENERAL1_ASRC_CG] = "afe_general1_asrc",
	[CLK_AFE_GENERAL0_ASRC_CG] = "afe_general0_asrc",
	[CLK_AFE_CONNSYS_I2S_ASRC_CG] = "afe_connsys_i2s_asrc",
	[CLK_CAMSYS_IPE_LARB19_CG] = "camsys_ipe_larb19",
	[CLK_CAMSYS_IPE_DPE_CG] = "camsys_ipe_dpe",
	[CLK_CAMSYS_IPE_FUS_CG] = "camsys_ipe_fus",
	[CLK_CAMSYS_IPE_GALS_CG] = "camsys_ipe_gals",
	[CLK_CAM_MR_LARBX_CG] = "cam_mr_larbx",
	[CLK_CAM_MR_GALS_CG] = "cam_mr_gals",
	[CLK_CAM_MR_CAMTG_CG] = "cam_mr_camtg",
	[CLK_CAM_MR_MRAW0_CG] = "cam_mr_mraw0",
	[CLK_CAM_MR_MRAW1_CG] = "cam_mr_mraw1",
	[CLK_CAM_MR_MRAW2_CG] = "cam_mr_mraw2",
	[CLK_CAM_MR_PDA0_CG] = "cam_mr_pda0",
	[CLK_CAM_MR_PDA1_CG] = "cam_mr_pda1",
	[CLK_CAM_RA_LARBX_CG] = "cam_ra_larbx",
	[CLK_CAM_RA_CAM_CG] = "cam_ra_cam",
	[CLK_CAM_RA_CAMTG_CG] = "cam_ra_camtg",
	[CLK_CAM_RA_RAW2MM_GALS_CG] = "cam_ra_raw2mm_gals",
	[CLK_CAM_RA_YUV2RAW2MM_GALS_CG] = "cam_ra_yuv2raw2mm",
	[CLK_CAM_RB_LARBX_CG] = "cam_rb_larbx",
	[CLK_CAM_RB_CAM_CG] = "cam_rb_cam",
	[CLK_CAM_RB_CAMTG_CG] = "cam_rb_camtg",
	[CLK_CAM_RB_RAW2MM_GALS_CG] = "cam_rb_raw2mm_gals",
	[CLK_CAM_RB_YUV2RAW2MM_GALS_CG] = "cam_rb_yuv2raw2mm",
	[CLK_CAM_RC_LARBX_CG] = "cam_rc_larbx",
	[CLK_CAM_RC_CAM_CG] = "cam_rc_cam",
	[CLK_CAM_RC_CAMTG_CG] = "cam_rc_camtg",
	[CLK_CAM_RC_RAW2MM_GALS_CG] = "cam_rc_raw2mm_gals",
	[CLK_CAM_RC_YUV2RAW2MM_GALS_CG] = "cam_rc_yuv2raw2mm",
	[CLK_CAMSYS_RMSA_LARBX_CG] = "camsys_rmsa_larbx",
	[CLK_CAMSYS_RMSA_CAM_CG] = "camsys_rmsa_cam",
	[CLK_CAMSYS_RMSA_CAMTG_CG] = "camsys_rmsa_camtg",
	[CLK_CAMSYS_RMSB_LARBX_CG] = "camsys_rmsb_larbx",
	[CLK_CAMSYS_RMSB_CAM_CG] = "camsys_rmsb_cam",
	[CLK_CAMSYS_RMSB_CAMTG_CG] = "camsys_rmsb_camtg",
	[CLK_CAMSYS_RMSC_LARBX_CG] = "camsys_rmsc_larbx",
	[CLK_CAMSYS_RMSC_CAM_CG] = "camsys_rmsc_cam",
	[CLK_CAMSYS_RMSC_CAMTG_CG] = "camsys_rmsc_camtg",
	[CLK_CAM_YA_LARBX_CG] = "cam_ya_larbx",
	[CLK_CAM_YA_CAM_CG] = "cam_ya_cam",
	[CLK_CAM_YA_CAMTG_CG] = "cam_ya_camtg",
	[CLK_CAM_YB_LARBX_CG] = "cam_yb_larbx",
	[CLK_CAM_YB_CAM_CG] = "cam_yb_cam",
	[CLK_CAM_YB_CAMTG_CG] = "cam_yb_camtg",
	[CLK_CAM_YC_LARBX_CG] = "cam_yc_larbx",
	[CLK_CAM_YC_CAM_CG] = "cam_yc_cam",
	[CLK_CAM_YC_CAMTG_CG] = "cam_yc_camtg",
	[CLK_CAM_MAIN_LARB13_CG] = "cam_m_larb13",
	[CLK_CAM_MAIN_LARB14_CG] = "cam_m_larb14",
	[CLK_CAM_MAIN_LARB27_CG] = "cam_m_larb27",
	[CLK_CAM_MAIN_LARB29_CG] = "cam_m_larb29",
	[CLK_CAM_MAIN_CAM_CG] = "cam_m_cam",
	[CLK_CAM_MAIN_CAM_SUBA_CG] = "cam_m_cam_suba",
	[CLK_CAM_MAIN_CAM_SUBB_CG] = "cam_m_cam_subb",
	[CLK_CAM_MAIN_CAM_SUBC_CG] = "cam_m_cam_subc",
	[CLK_CAM_MAIN_CAM_MRAW_CG] = "cam_m_cam_mraw",
	[CLK_CAM_MAIN_CAMTG_CG] = "cam_m_camtg",
	[CLK_CAM_MAIN_SENINF_CG] = "cam_m_seninf",
	[CLK_CAM_MAIN_CAMSV_TOP_CG] = "cam_m_camsv",
	[CLK_CAM_MAIN_ADLRD_CG] = "cam_m_adlrd",
	[CLK_CAM_MAIN_ADLWR_CG] = "cam_m_adlwr",
	[CLK_CAM_MAIN_FAKE_ENG_CG] = "cam_m_fake_eng",
	[CLK_CAM_MAIN_CAM2MM0_GALS_CG] = "cam_m_cam2mm0_GCON_0",
	[CLK_CAM_MAIN_CAM2MM1_GALS_CG] = "cam_m_cam2mm1_GCON_0",
	[CLK_CAM_MAIN_CAM2SYS_GALS_CG] = "cam_m_cam2sys_GCON_0",
	[CLK_CAM_MAIN_CAM2MM2_GALS_CG] = "cam_m_cam2mm2_GCON_0",
	[CLK_CAM_MAIN_IPS_CG] = "cam_m_ips",
	[CLK_CAM_MAIN_CAM_DPE_CG] = "cam_m_cam_dpe",
	[CLK_CAM_MAIN_CAM_ASG_CG] = "cam_m_cam_asg",
	[CLK_CAM_MAIN_CAMSV_A_CON_1_CG] = "cam_m_camsv_a_con_1",
	[CLK_CAM_MAIN_CAMSV_B_CON_1_CG] = "cam_m_camsv_b_con_1",
	[CLK_CAM_MAIN_CAMSV_C_CON_1_CG] = "cam_m_camsv_c_con_1",
	[CLK_CAM_MAIN_CAMSV_D_CON_1_CG] = "cam_m_camsv_d_con_1",
	[CLK_CAM_MAIN_CAMSV_E_CON_1_CG] = "cam_m_camsv_e_con_1",
	[CLK_CAM_MAIN_CAM_QOF_CON_1_CG] = "cam_m_cam_qof_con_1",
	[CLK_CAM_MAIN_CAM_BLS_FULL_CON_1_CG] = "cam_m_cam_bls_full_con_1",
	[CLK_CAM_MAIN_CAM_BLS_PART_CON_1_CG] = "cam_m_cam_bls_part_con_1",
	[CLK_CAM_MAIN_CAM_BWR_CON_1_CG] = "cam_m_cam_bwr_con_1",
	[CLK_CAM_MAIN_CAM_RTCQ_CON_1_CG] = "cam_m_cam_rtcq_con_1",
	[CLK_CAM_MAIN_CAM2MM0_SUB_COMMON_DCM_DIS_CG] = "cam_m_cam2mm0_sub_c_dis",
	[CLK_CAM_MAIN_CAM2MM1_SUB_COMMON_DCM_DIS_CG] = "cam_m_cam2mm1_sub_c_dis",
	[CLK_CAM_MAIN_CAM2SYS_SUB_COMMON_DCM_DIS_CG] = "cam_m_cam2sys_sub_c_dis",
	[CLK_CAM_MAIN_CAM2MM2_SUB_COMMON_DCM_DIS_CG] = "cam_m_cam2mm2_sub_c_dis",
	[CLK_CAMV_CV_CAM2MM0_SC_DCM_DIS_CG] = "camv_cv_cam2mm0_subc_dis",
	[CLK_CAMV_CV_MM0_SC_DCM_DIS_CG] = "camv_cv_mm0_subc_dis",
	[CLK_CAMV_CV_CAMVCORE_CG] = "camv_cv_camvcore",
	[CLK_CAMV_CV_CAM_26M_CG] = "camv_cv_cam_26m",
	[CLK_CCU_LARB19_CON_CG] = "ccu_larb19_con",
	[CLK_CCU2INFRA_GALS_CON_CG] = "ccu2infra_GCON",
	[CLK_CCUSYS_CCU0_CON_CG] = "ccusys_ccu0_con",
	[CLK_CCU2MM0_GALS_CON_CG] = "ccu2mm0_GCON",
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
	[CLK_IMG_LARB9_CG] = "img_larb9",
	[CLK_IMG_TRAW0_CG] = "img_traw0",
	[CLK_IMG_TRAW1_CG] = "img_traw1",
	[CLK_IMG_DIP0_CG] = "img_dip0",
	[CLK_IMG_WPE0_CG] = "img_wpe0",
	[CLK_IMG_IPE_CG] = "img_ipe",
	[CLK_IMG_WPE1_CG] = "img_wpe1",
	[CLK_IMG_WPE2_CG] = "img_wpe2",
	[CLK_IMG_ADL_LARB_CG] = "img_adl_larb",
	[CLK_IMG_ADLRD_CG] = "img_adlrd",
	[CLK_IMG_ADLWR0_CG] = "img_adlwr0",
	[CLK_IMG_AVS_CG] = "img_avs",
	[CLK_IMG_IPS_CG] = "img_ips",
	[CLK_IMG_ADLWR1_CG] = "img_adlwr1",
	[CLK_IMG_ROOTCQ_CG] = "img_rootcq",
	[CLK_IMG_BLS_CG] = "img_bls",
	[CLK_IMG_SUB_COMMON0_CG] = "img_sub_common0",
	[CLK_IMG_SUB_COMMON1_CG] = "img_sub_common1",
	[CLK_IMG_SUB_COMMON2_CG] = "img_sub_common2",
	[CLK_IMG_SUB_COMMON3_CG] = "img_sub_common3",
	[CLK_IMG_SUB_COMMON4_CG] = "img_sub_common4",
	[CLK_IMG_GALS_RX_DIP0_CG] = "img_gals_rx_dip0",
	[CLK_IMG_GALS_RX_DIP1_CG] = "img_gals_rx_dip1",
	[CLK_IMG_GALS_RX_TRAW0_CG] = "img_gals_rx_traw0",
	[CLK_IMG_GALS_RX_WPE0_CG] = "img_gals_rx_wpe0",
	[CLK_IMG_GALS_RX_WPE1_CG] = "img_gals_rx_wpe1",
	[CLK_IMG_GALS_RX_WPE2_CG] = "img_gals_rx_wpe2",
	[CLK_IMG_GALS_TRX_IPE0_CG] = "img_gals_trx_ipe0",
	[CLK_IMG_GALS_TRX_IPE1_CG] = "img_gals_trx_ipe1",
	[CLK_IMG26_CG] = "img26",
	[CLK_IMG_BWR_CG] = "img_bwr",
	[CLK_IMG_GALS_CG] = "img_gals",
	[CLK_IMG_FDVT_CG] = "img_fdvt",
	[CLK_IMG_ME_CG] = "img_me",
	[CLK_IMG_MMG_CG] = "img_mmg",
	[CLK_IMG_LARB12_CG] = "img_larb12",
	[CLK_IMG_VCORE_GALS_DISP_CG] = "img_vcore_gals_disp",
	[CLK_IMG_VCORE_MAIN_CG] = "img_vcore_main",
	[CLK_IMG_VCORE_SUB0_CG] = "img_vcore_sub0",
	[CLK_IMG_VCORE_SUB1_CG] = "img_vcore_sub1",
	[CLK_IMG_VCORE_IMG_26M_CG] = "img_vcore_img_26m",
	[CLK_TRAW_CAP_DIP1_TRAW_CAP_CG] = "traw__dip1_cap",
	[CLK_TRAW_DIP1_LARB28_CG] = "traw_dip1_larb28",
	[CLK_TRAW_DIP1_LARB40_CG] = "traw_dip1_larb40",
	[CLK_TRAW_DIP1_TRAW_CG] = "traw_dip1_traw",
	[CLK_TRAW_DIP1_GALS_CG] = "traw_dip1_gals",
	[CLK_WPE1_DIP1_LARB11_CG] = "wpe1_dip1_larb11",
	[CLK_WPE1_DIP1_WPE_CG] = "wpe1_dip1_wpe",
	[CLK_WPE1_DIP1_GALS0_CG] = "wpe1_dip1_gals0",
	[CLK_WPE2_DIP1_LARB11_CG] = "wpe2_dip1_larb11",
	[CLK_WPE2_DIP1_WPE_CG] = "wpe2_dip1_wpe",
	[CLK_WPE2_DIP1_GALS0_CG] = "wpe2_dip1_gals0",
	[CLK_WPE3_DIP1_LARB11_CG] = "wpe3_dip1_larb11",
	[CLK_WPE3_DIP1_WPE_CG] = "wpe3_dip1_wpe",
	[CLK_WPE3_DIP1_GALS0_CG] = "wpe3_dip1_gals0",
	[CLK_MM1_DISPSYS1_CONFIG_CG] = "mm1_dispsys1_config",
	[CLK_MM1_DISP_MUTEX0_CG] = "mm1_disp_mutex0",
	[CLK_MM1_DISP_DLI_ASYNC0_CG] = "mm1_disp_dli_async0",
	[CLK_MM1_DISP_DLI_ASYNC1_CG] = "mm1_disp_dli_async1",
	[CLK_MM1_DISP_DLI_ASYNC2_CG] = "mm1_disp_dli_async2",
	[CLK_MM1_MDP_RDMA0_CG] = "mm1_mdp_rdma0",
	[CLK_MM1_DISP_R2Y0_CG] = "mm1_disp_r2y0",
	[CLK_MM1_DISP_SPLITTER0_CG] = "mm1_disp_splitter0",
	[CLK_MM1_DISP_SPLITTER1_CG] = "mm1_disp_splitter1",
	[CLK_MM1_DISP_VDCM0_CG] = "mm1_disp_vdcm0",
	[CLK_MM1_DISP_DSC_WRAP0_CG] = "mm1_disp_dsc_wrap0",
	[CLK_MM1_DISP_DSC_WRAP1_CG] = "mm1_disp_dsc_wrap1",
	[CLK_MM1_DISP_DSC_WRAP2_CG] = "mm1_disp_dsc_wrap2",
	[CLK_MM1_DISP_DP_INTF0_CG] = "mm1_DP_CLK",
	[CLK_MM1_DISP_DSI0_CG] = "mm1_CLK0",
	[CLK_MM1_DISP_DSI1_CG] = "mm1_CLK1",
	[CLK_MM1_DISP_DSI2_CG] = "mm1_CLK2",
	[CLK_MM1_DISP_MERGE0_CG] = "mm1_disp_merge0",
	[CLK_MM1_DISP_WDMA0_CG] = "mm1_disp_wdma0",
	[CLK_MM1_SMI_SUB_COMM0_CG] = "mm1_ssc",
	[CLK_MM1_DISP_WDMA1_CG] = "mm1_disp_wdma1",
	[CLK_MM1_DISP_WDMA2_CG] = "mm1_disp_wdma2",
	[CLK_MM1_DISP_GDMA0_CG] = "mm1_disp_gdma0",
	[CLK_MM1_DISP_DLI_ASYNC3_CG] = "mm1_disp_dli_async3",
	[CLK_MM1_DISP_DLI_ASYNC4_CG] = "mm1_disp_dli_async4",
	[CLK_MM1_MOD1_CG] = "mm1_mod1",
	[CLK_MM1_MOD2_CG] = "mm1_mod2",
	[CLK_MM1_MOD3_CG] = "mm1_mod3",
	[CLK_MM1_MOD4_CG] = "mm1_mod4",
	[CLK_MM1_MOD5_CG] = "mm1_mod5",
	[CLK_MM1_MOD6_CG] = "mm1_mod6",
	[CLK_MM1_MOD7_CG] = "mm1_mod7",
	[CLK_MM1_SUBSYS_CG] = "mm1_subsys_ck",
	[CLK_MM1_DSI0_CG] = "mm1_dsi0_ck",
	[CLK_MM1_DSI1_CG] = "mm1_dsi1_ck",
	[CLK_MM1_DSI2_CG] = "mm1_dsi2_ck",
	[CLK_MM1_DP_CG] = "mm1_dp_ck",
	[CLK_MM1_F26M_CG] = "mm1_f26m_ck",
	[CLK_MM_CONFIG_CG] = "mm_config",
	[CLK_MM_DISP_MUTEX0_CG] = "mm_disp_mutex0",
	[CLK_MM_DISP_AAL0_CG] = "mm_disp_aal0",
	[CLK_MM_DISP_AAL1_CG] = "mm_disp_aal1",
	[CLK_MM_DISP_C3D0_CG] = "mm_disp_c3d0",
	[CLK_MM_DISP_C3D1_CG] = "mm_disp_c3d1",
	[CLK_MM_DISP_CCORR0_CG] = "mm_disp_ccorr0",
	[CLK_MM_DISP_CCORR1_CG] = "mm_disp_ccorr1",
	[CLK_MM_DISP_CCORR2_CG] = "mm_disp_ccorr2",
	[CLK_MM_DISP_CCORR3_CG] = "mm_disp_ccorr3",
	[CLK_MM_DISP_CHIST0_CG] = "mm_disp_chist0",
	[CLK_MM_DISP_CHIST1_CG] = "mm_disp_chist1",
	[CLK_MM_DISP_COLOR0_CG] = "mm_disp_color0",
	[CLK_MM_DISP_COLOR1_CG] = "mm_disp_color1",
	[CLK_MM_DISP_DITHER0_CG] = "mm_disp_dither0",
	[CLK_MM_DISP_DITHER1_CG] = "mm_disp_dither1",
	[CLK_MM_DISP_DITHER2_CG] = "mm_disp_dither2",
	[CLK_MM_DLI_ASYNC0_CG] = "mm_dli_async0",
	[CLK_MM_DLI_ASYNC1_CG] = "mm_dli_async1",
	[CLK_MM_DLI_ASYNC2_CG] = "mm_dli_async2",
	[CLK_MM_DLI_ASYNC3_CG] = "mm_dli_async3",
	[CLK_MM_DLI_ASYNC4_CG] = "mm_dli_async4",
	[CLK_MM_DLI_ASYNC5_CG] = "mm_dli_async5",
	[CLK_MM_DLI_ASYNC6_CG] = "mm_dli_async6",
	[CLK_MM_DLI_ASYNC7_CG] = "mm_dli_async7",
	[CLK_MM_DLO_ASYNC0_CG] = "mm_dlo_async0",
	[CLK_MM_DLO_ASYNC1_CG] = "mm_dlo_async1",
	[CLK_MM_DLO_ASYNC2_CG] = "mm_dlo_async2",
	[CLK_MM_DLO_ASYNC3_CG] = "mm_dlo_async3",
	[CLK_MM_DLO_ASYNC4_CG] = "mm_dlo_async4",
	[CLK_MM_DISP_GAMMA0_CG] = "mm_disp_gamma0",
	[CLK_MM_DISP_GAMMA1_CG] = "mm_disp_gamma1",
	[CLK_MM_MDP_AAL0_CG] = "mm_mdp_aal0",
	[CLK_MM_MDP_RDMA0_CG] = "mm_mdp_rdma0",
	[CLK_MM_DISP_ODDMR0_CG] = "mm_disp_oddmr0",
	[CLK_MM_DISP_POSTALIGN0_CG] = "mm_disp_postalign0",
	[CLK_MM_DISP_POSTMASK0_CG] = "mm_disp_postmask0",
	[CLK_MM_DISP_POSTMASK1_CG] = "mm_disp_postmask1",
	[CLK_MM_DISP_RSZ0_CG] = "mm_disp_rsz0",
	[CLK_MM_DISP_RSZ1_CG] = "mm_disp_rsz1",
	[CLK_MM_DISP_SPR0_CG] = "mm_disp_spr0",
	[CLK_MM_DISP_TDSHP0_CG] = "mm_disp_tdshp0",
	[CLK_MM_DISP_TDSHP1_CG] = "mm_disp_tdshp1",
	[CLK_MM_DISP_WDMA1_CG] = "mm_disp_wdma1",
	[CLK_MM_DISP_Y2R0_CG] = "mm_disp_y2r0",
	[CLK_MM_MDP_AAL1_CG] = "mm_mdp_aal1",
	[CLK_MM_SMI_SUB_COMM0_CG] = "mm_ssc",
	[CLK_MM_DISP_RSZ0_MOUT_RELAY_CG] = "mm_disp_rsz0_mout_relay",
	[CLK_MM_DISP_RSZ1_MOUT_RELAY_CG] = "mm_disp_rsz1_mout_relay",
	[CLK_MMINFRA_AO_GCE_D_CG] = "mminfra_ao_gce_d",
	[CLK_MMINFRA_AO_GCE_M_CG] = "mminfra_ao_gce_m",
	[CLK_MMINFRA_AO_GCE_26M_CG] = "mminfra_ao_gce_26m",
	[CLK_OVLSYS_CONFIG_CG] = "ovlsys_config",
	[CLK_OVL_DISP_FAKE_ENG0_CG] = "ovl_disp_fake_eng0",
	[CLK_OVL_DISP_FAKE_ENG1_CG] = "ovl_disp_fake_eng1",
	[CLK_OVL_DISP_MUTEX0_CG] = "ovl_disp_mutex0",
	[CLK_OVL_DISP_OVL0_2L_CG] = "ovl_disp_ovl0_2l",
	[CLK_OVL_DISP_OVL1_2L_CG] = "ovl_disp_ovl1_2l",
	[CLK_OVL_DISP_OVL2_2L_CG] = "ovl_disp_ovl2_2l",
	[CLK_OVL_DISP_OVL3_2L_CG] = "ovl_disp_ovl3_2l",
	[CLK_OVL_DISP_RSZ1_CG] = "ovl_disp_rsz1",
	[CLK_OVL_MDP_RSZ0_CG] = "ovl_mdp_rsz0",
	[CLK_OVL_DISP_WDMA0_CG] = "ovl_disp_wdma0",
	[CLK_OVL_DISP_UFBC_WDMA0_CG] = "ovl_disp_ufbc_wdma0",
	[CLK_OVL_DISP_WDMA2_CG] = "ovl_disp_wdma2",
	[CLK_OVL_DISP_DLI_ASYNC0_CG] = "ovl_disp_dli_async0",
	[CLK_OVL_DISP_DLI_ASYNC1_CG] = "ovl_disp_dli_async1",
	[CLK_OVL_DISP_DLI_ASYNC2_CG] = "ovl_disp_dli_async2",
	[CLK_OVL_DISP_DL0_ASYNC0_CG] = "ovl_disp_dl0_async0",
	[CLK_OVL_DISP_DL0_ASYNC1_CG] = "ovl_disp_dl0_async1",
	[CLK_OVL_DISP_DL0_ASYNC2_CG] = "ovl_disp_dl0_async2",
	[CLK_OVL_DISP_DL0_ASYNC3_CG] = "ovl_disp_dl0_async3",
	[CLK_OVL_DISP_DL0_ASYNC4_CG] = "ovl_disp_dl0_async4",
	[CLK_OVL_DISP_DL0_ASYNC5_CG] = "ovl_disp_dl0_async5",
	[CLK_OVL_DISP_DL0_ASYNC6_CG] = "ovl_disp_dl0_async6",
	[CLK_OVL_INLINEROT0_CG] = "ovl_inlinerot0",
	[CLK_OVL_SMI_SUB_COMM0_CG] = "ovl_ssc",
	[CLK_OVL_DISP_Y2R0_CG] = "ovl_disp_y2r0",
	[CLK_OVL_DISP_Y2R1_CG] = "ovl_disp_y2r1",
	[CLK_OVL_DISP_OVL4_2L_CG] = "ovl_disp_ovl4_2l",
	[CLK_IMPC_I2C10_CG] = "impc_i2c10",
	[CLK_IMPC_I2C11_CG] = "impc_i2c11",
	[CLK_IMPC_I2C12_CG] = "impc_i2c12",
	[CLK_IMPC_I2C13_CG] = "impc_i2c13",
	[CLK_IMPE_I3C4_CG] = "impe_i3c4",
	[CLK_IMPE_I3C8_CG] = "impe_i3c8",
	[CLK_IMPEN_I3C2_CG] = "impen_i3c2",
	[CLK_IMPES_I3C9_CG] = "impes_i3c9",
	[CLK_IMPN_I2C3_CG] = "impn_i2c3",
	[CLK_IMPN_I2C5_CG] = "impn_i2c5",
	[CLK_IMPS_I3C0_CG] = "imps_i3c0",
	[CLK_IMPS_I3C1_CG] = "imps_i3c1",
	[CLK_IMPS_I3C7_CG] = "imps_i3c7",
	[CLK_IMPW_I2C6_CG] = "impw_i2c6",
	[CLK_PERAOP_UART0_CG] = "peraop_uart0",
	[CLK_PERAOP_UART1_CG] = "peraop_uart1",
	[CLK_PERAOP_UART2_CG] = "peraop_uart2",
	[CLK_PERAOP_UART3_CG] = "peraop_uart3",
	[CLK_PERAOP_PWM_H_CG] = "peraop_pwm_h",
	[CLK_PERAOP_PWM_B_CG] = "peraop_pwm_b",
	[CLK_PERAOP_PWM_FB1_CG] = "peraop_pwm_fb1",
	[CLK_PERAOP_PWM_FB2_CG] = "peraop_pwm_fb2",
	[CLK_PERAOP_PWM_FB3_CG] = "peraop_pwm_fb3",
	[CLK_PERAOP_PWM_FB4_CG] = "peraop_pwm_fb4",
	[CLK_PERAOP_SPI0_B_CG] = "peraop_spi0_b",
	[CLK_PERAOP_SPI1_B_CG] = "peraop_spi1_b",
	[CLK_PERAOP_SPI2_B_CG] = "peraop_spi2_b",
	[CLK_PERAOP_SPI3_B_CG] = "peraop_spi3_b",
	[CLK_PERAOP_SPI4_B_CG] = "peraop_spi4_b",
	[CLK_PERAOP_SPI5_B_CG] = "peraop_spi5_b",
	[CLK_PERAOP_SPI6_B_CG] = "peraop_spi6_b",
	[CLK_PERAOP_SPI7_B_CG] = "peraop_spi7_b",
	[CLK_PERAOP_DMA_B_CG] = "peraop_dma_b",
	[CLK_PERAOP_SSUSB0_FRMCNT_CG] = "peraop_ssusb0_frmcnt",
	[CLK_PERAOP_MSDC1_CG] = "peraop_msdc1",
	[CLK_PERAOP_MSDC1_F_CG] = "peraop_msdc1_f",
	[CLK_PERAOP_MSDC1_H_CG] = "peraop_msdc1_h",
	[CLK_PERAOP_MSDC2_CG] = "peraop_msdc2",
	[CLK_PERAOP_MSDC2_F_CG] = "peraop_msdc2_f",
	[CLK_PERAOP_MSDC2_H_CG] = "peraop_msdc2_h",
	[CLK_PERAOP_AUDIO_SLV_CG] = "peraop_audio_slv",
	[CLK_PERAOP_AUDIO_MST_CG] = "peraop_audio_mst",
	[CLK_PERAOP_AUDIO_INTBUS_CG] = "peraop_audio_intbus",
	[CLK_UFSAO_UNIPRO_SYS_CG] = "ufsao_unipro_sys",
	[CLK_UFSAO_U_PHY_SAP_CG] = "ufsao_u_phy_sap",
	[CLK_UFSAO_U_PHY_TOP_AHB_S_BUSCK_CG] = "ufsao_u_phy_ahb_s_busck",
	[CLK_UFSAO_U_AO_FREE_26M_CG] = "ufsao_u_ao_26m_ck",
	[CLK_UFSPDN_UFSHCI_UFS_CG] = "ufspdn_ufshci_ufs",
	[CLK_UFSPDN_UFSHCI_AES_CG] = "ufspdn_ufshci_aes",
	[CLK_UFSPDN_UFSHCI_U_AHB_CG] = "ufspdn_ufshci_u_ahb",
	[CLK_UFSPDN_UFSHCI_U_AXI_CG] = "ufspdn_ufshci_u_axi",
	[CLK_UFSPDN_U_FREE_26M_CG] = "ufspdn_u_26m_ck",
	[CLK_MDP1_MDP_MUTEX0_CG] = "mdp1_mdp_mutex0",
	[CLK_MDP1_APB_BUS_CG] = "mdp1_apb_bus",
	[CLK_MDP1_SMI0_CG] = "mdp1_smi0",
	[CLK_MDP1_MDP_RDMA0_CG] = "mdp1_mdp_rdma0",
	[CLK_MDP1_MDP_RDMA2_CG] = "mdp1_mdp_rdma2",
	[CLK_MDP1_MDP_HDR0_CG] = "mdp1_mdp_hdr0",
	[CLK_MDP1_MDP_AAL0_CG] = "mdp1_mdp_aal0",
	[CLK_MDP1_MDP_RSZ0_CG] = "mdp1_mdp_rsz0",
	[CLK_MDP1_MDP_TDSHP0_CG] = "mdp1_mdp_tdshp0",
	[CLK_MDP1_MDP_COLOR0_CG] = "mdp1_mdp_color0",
	[CLK_MDP1_MDP_WROT0_CG] = "mdp1_mdp_wrot0",
	[CLK_MDP1_MDP_FAKE_ENG0_CG] = "mdp1_mdp_fake_eng0",
	[CLK_MDP1_MDP_DLI_ASYNC0_CG] = "mdp1_mdp_dli_async0",
	[CLK_MDP1_APB_DB_CG] = "mdp1_apb_db",
	[CLK_MDP1_MDP_RSZ2_CG] = "mdp1_mdp_rsz2",
	[CLK_MDP1_MDP_WROT2_CG] = "mdp1_mdp_wrot2",
	[CLK_MDP1_MDP_DLO_ASYNC0_CG] = "mdp1_mdp_dlo_async0",
	[CLK_MDP1_MDP_BIRSZ0_CG] = "mdp1_mdp_birsz0",
	[CLK_MDP1_MDP_C3D0_CG] = "mdp1_mdp_c3d0",
	[CLK_MDP1_MDP_FG0_CG] = "mdp1_mdp_fg0",
	[CLK_MDP_MUTEX0_CG] = "mdp_mutex0",
	[CLK_MDP_APB_BUS_CG] = "mdp_apb_bus",
	[CLK_MDP_SMI0_CG] = "mdp_smi0",
	[CLK_MDP_RDMA0_CG] = "mdp_rdma0",
	[CLK_MDP_RDMA2_CG] = "mdp_rdma2",
	[CLK_MDP_HDR0_CG] = "mdp_hdr0",
	[CLK_MDP_AAL0_CG] = "mdp_aal0",
	[CLK_MDP_RSZ0_CG] = "mdp_rsz0",
	[CLK_MDP_TDSHP0_CG] = "mdp_tdshp0",
	[CLK_MDP_COLOR0_CG] = "mdp_color0",
	[CLK_MDP_WROT0_CG] = "mdp_wrot0",
	[CLK_MDP_FAKE_ENG0_CG] = "mdp_fake_eng0",
	[CLK_MDP_APB_DB_CG] = "mdp_apb_db",
	[CLK_MDP_BIRSZ0_CG] = "mdp_birsz0",
	[CLK_MDP_C3D0_CG] = "mdp_c3d0",
	[CLK_VDE2_VDEC_CKEN_CG] = "vde2_vdec_cken",
	[CLK_VDE2_VDEC_ACTIVE_CG] = "vde2_vdec_active",
	[CLK_VDE2_VDEC_CKEN_ENG_CG] = "vde2_vdec_cken_eng",
	[CLK_VDE2_LAT_CKEN_CG] = "vde2_lat_cken",
	[CLK_VDE2_LAT_ACTIVE_CG] = "vde2_lat_active",
	[CLK_VDE2_LAT_CKEN_ENG_CG] = "vde2_lat_cken_eng",
	[CLK_VDE2_LARB1_CKEN_CG] = "vde2_larb1_cken",
	[CLK_VDE1_VDEC_CKEN_CG] = "vde1_vdec_cken",
	[CLK_VDE1_VDEC_ACTIVE_CG] = "vde1_vdec_active",
	[CLK_VDE1_VDEC_CKEN_ENG_CG] = "vde1_vdec_cken_eng",
	[CLK_VDE1_LAT_CKEN_CG] = "vde1_lat_cken",
	[CLK_VDE1_LAT_ACTIVE_CG] = "vde1_lat_active",
	[CLK_VDE1_LAT_CKEN_ENG_CG] = "vde1_lat_cken_eng",
	[CLK_VDE1_LARB1_CKEN_CG] = "vde1_larb1_cken",
	[CLK_VEN1_CKE0_LARB_CG] = "ven1_larb",
	[CLK_VEN1_CKE1_VENC_CG] = "ven1_venc",
	[CLK_VEN1_CKE2_JPGENC_CG] = "ven1_jpgenc",
	[CLK_VEN1_CKE3_JPGDEC_CG] = "ven1_jpgdec",
	[CLK_VEN1_CKE4_JPGDEC_C1_CG] = "ven1_jpgdec_c1",
	[CLK_VEN1_CKE5_GALS_CG] = "ven1_gals",
	[CLK_VEN1_CKE6_GALS_SRAM_CG] = "ven1_gals_sram",
	[CLK_VEN2_CKE0_LARB_CG] = "ven2_larb",
	[CLK_VEN2_CKE1_VENC_CG] = "ven2_venc",
	[CLK_VEN2_CKE2_JPGENC_CG] = "ven2_jpgenc",
	[CLK_VEN2_CKE3_JPGDEC_CG] = "ven2_jpgdec",
	[CLK_VEN2_CKE5_GALS_CG] = "ven2_gals",
	[CLK_VEN2_CKE6_GALS_SRAM_CG] = "ven2_gals_sram",
	[TRACE_CLK_NUM] = "NULL",
};

struct clkchk_fm {
	const char *fm_name;
	unsigned int fm_id;
	unsigned int fm_type;
};

/* check which fmeter clk you want to get freq */
enum {
	CHK_FM_NUM,
};

/* fill in the fmeter clk you want to get freq */
struct  clkchk_fm chk_fm_list[] = {
	{},
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

static void dump_clk_event(void)
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
	[top] = REGBASE_V(0x10000000, top, PD_NULL, CLK_NULL),
	[infracfg_ao] = REGBASE_V(0x10001000, infracfg_ao, PD_NULL, CLK_NULL),
	[apmixed] = REGBASE_V(0x1000C000, apmixed, PD_NULL, CLK_NULL),
	[infra_ifrbus_ao_reg_bus] = REGBASE_V(0x1002C000, infra_ifrbus_ao_reg_bus, PD_NULL, CLK_NULL),
	[emi_nemicfg_ao_mem_prot_reg_bus] = REGBASE_V(0x10270000, emi_nemicfg_ao_mem_prot_reg_bus, PD_NULL, CLK_NULL),
	[emi_semicfg_ao_mem_prot_reg_bus] = REGBASE_V(0x1030E000, emi_semicfg_ao_mem_prot_reg_bus, PD_NULL, CLK_NULL),
	[perao] = REGBASE_V(0x11036000, perao, PD_NULL, CLK_NULL),
	[afe] = REGBASE_V(0x11050000, afe, MT6899_CHK_PD_PERI_AUDIO, CLK_NULL),
	[impc] = REGBASE_V(0x11284000, impc, PD_NULL, "i2c_sel"),
	[ufscfg_ao_bus] = REGBASE_V(0x112B8000, ufscfg_ao_bus, PD_NULL, CLK_NULL),
	[ufsao] = REGBASE_V(0x112b8000, ufsao, PD_NULL, CLK_NULL),
	[ufspdn] = REGBASE_V(0x112bb000, ufspdn, PD_NULL, CLK_NULL),
	[impen] = REGBASE_V(0x11B71000, impen, PD_NULL, CLK_NULL),
	[impe] = REGBASE_V(0x11CB2000, impe, PD_NULL, "i2c_sel"),
	[imps] = REGBASE_V(0x11D03000, imps, PD_NULL, "i2c_sel"),
	[impes] = REGBASE_V(0x11D11000, impes, PD_NULL, "i2c_sel"),
	[impw] = REGBASE_V(0x11E01000, impw, PD_NULL, "i2c_sel"),
	[impn] = REGBASE_V(0x11F02000, impn, PD_NULL, "i2c_sel"),
	[mfg_ao] = REGBASE_V(0x13fa0000, mfg_ao, PD_NULL, CLK_NULL),
	[mfgsc_ao] = REGBASE_V(0x13fa0400, mfgsc_ao, PD_NULL, CLK_NULL),
	[mm] = REGBASE_V(0x14000000, mm, MT6899_CHK_PD_DIS1, CLK_NULL),
	[mm1] = REGBASE_V(0x14200000, mm1, MT6899_CHK_PD_DISP_VCORE, CLK_NULL),
	[ovl] = REGBASE_V(0x14400000, ovl, MT6899_CHK_PD_OVL0, CLK_NULL),
	[img] = REGBASE_V(0x15000000, img, MT6899_CHK_PD_ISP_MAIN, CLK_NULL),
	[dip_top_dip1] = REGBASE_V(0x15110000, dip_top_dip1, MT6899_CHK_PD_ISP_DIP1, CLK_NULL),
	[dip_nr1_dip1] = REGBASE_V(0x15130000, dip_nr1_dip1, MT6899_CHK_PD_ISP_DIP1, CLK_NULL),
	[dip_nr2_dip1] = REGBASE_V(0x15170000, dip_nr2_dip1, MT6899_CHK_PD_ISP_DIP1, CLK_NULL),
	[wpe1_dip1] = REGBASE_V(0x15220000, wpe1_dip1, MT6899_CHK_PD_ISP_DIP1, CLK_NULL),
	[wpe2_dip1] = REGBASE_V(0x15520000, wpe2_dip1, MT6899_CHK_PD_ISP_DIP1, CLK_NULL),
	[wpe3_dip1] = REGBASE_V(0x15620000, wpe3_dip1, MT6899_CHK_PD_ISP_DIP1, CLK_NULL),
	[traw_dip1] = REGBASE_V(0x15710000, traw_dip1, MT6899_CHK_PD_ISP_TRAW, CLK_NULL),
	[traw_cap_dip1] = REGBASE_V(0x15740000, traw_cap_dip1, MT6899_CHK_PD_ISP_TRAW, CLK_NULL),
	[img_v] = REGBASE_V(0x15780000, img_v, MT6899_CHK_PD_ISP_MAIN, CLK_NULL),
	[vde1] = REGBASE_V(0x1600f000, vde1, MT6899_CHK_PD_VDE1, CLK_NULL),
	[vde2] = REGBASE_V(0x1602f000, vde2, MT6899_CHK_PD_VDE0, CLK_NULL),
	[ven1] = REGBASE_V(0x17000000, ven1, MT6899_CHK_PD_VEN0, CLK_NULL),
	[ven2] = REGBASE_V(0x17800000, ven2, MT6899_CHK_PD_VEN1, CLK_NULL),
	[spm] = REGBASE_V(0x1C001000, spm, PD_NULL, CLK_NULL),
	[vlpcfg] = REGBASE_V(0x1C00C000, vlpcfg, PD_NULL, CLK_NULL),
	[vlp_ck] = REGBASE_V(0x1C013000, vlp_ck, PD_NULL, CLK_NULL),
	[cam_m] = REGBASE_V(0x1a000000, cam_m, MT6899_CHK_PD_CAM_VCORE, CLK_NULL),
	[cam_mr] = REGBASE_V(0x1a740000, cam_mr, MT6899_CHK_PD_CAM_MAIN, CLK_NULL),
	[camsys_ipe] = REGBASE_V(0x1a7a0000, camsys_ipe, MT6899_CHK_PD_CAM_MAIN, CLK_NULL),
	[cam_ra] = REGBASE_V(0x1a8c0000, cam_ra, MT6899_CHK_PD_CAM_MAIN, CLK_NULL),
	[camsys_rmsa] = REGBASE_V(0x1a8d0000, camsys_rmsa, MT6899_CHK_PD_CAM_MAIN, CLK_NULL),
	[cam_ya] = REGBASE_V(0x1a8e0000, cam_ya, MT6899_CHK_PD_CAM_MAIN, CLK_NULL),
	[cam_rb] = REGBASE_V(0x1a9c0000, cam_rb, MT6899_CHK_PD_CAM_MAIN, CLK_NULL),
	[camsys_rmsb] = REGBASE_V(0x1a9d0000, camsys_rmsb, MT6899_CHK_PD_CAM_MAIN, CLK_NULL),
	[cam_yb] = REGBASE_V(0x1a9e0000, cam_yb, MT6899_CHK_PD_CAM_MAIN, CLK_NULL),
	[cam_rc] = REGBASE_V(0x1aac0000, cam_rc, MT6899_CHK_PD_CAM_MAIN, CLK_NULL),
	[camsys_rmsc] = REGBASE_V(0x1aad0000, camsys_rmsc, MT6899_CHK_PD_CAM_MAIN, CLK_NULL),
	[cam_yc] = REGBASE_V(0x1aae0000, cam_yc, MT6899_CHK_PD_CAM_MAIN, CLK_NULL),
	[ccu] = REGBASE_V(0x1b800000, ccu, MT6899_CHK_PD_CAM_VCORE, CLK_NULL),
	[camv] = REGBASE_V(0x1b805000, camv, MT6899_CHK_PD_CAM_MAIN, CLK_NULL),
	[mminfra_ao_config] = REGBASE_V(0x1e8ff000, mminfra_ao_config, MT6899_CHK_PD_MM_INFRA, CLK_NULL),
	[mdp] = REGBASE_V(0x1f000000, mdp, MT6899_CHK_PD_MML0, CLK_NULL),
	[mdp1] = REGBASE_V(0x1f800000, mdp1, MT6899_CHK_PD_MML1, CLK_NULL),
	[cci] = REGBASE_V(0xc030000, cci, PD_NULL, CLK_NULL),
	[cpu_ll] = REGBASE_V(0xc030400, cpu_ll, PD_NULL, CLK_NULL),
	[cpu_bl] = REGBASE_V(0xc030800, cpu_bl, PD_NULL, CLK_NULL),
	[cpu_b] = REGBASE_V(0xc030c00, cpu_b, PD_NULL, CLK_NULL),
	[ptp] = REGBASE_V(0xc034000, ptp, PD_NULL, CLK_NULL),
	[hwv] = REGBASE_V(0x10320000, hwv, PD_NULL, CLK_NULL),
	[mm_hwv] = REGBASE_V(0x1ec3a000, mm_hwv, PD_NULL, CLK_NULL),
	[hfrp] = REGBASE_V(0x1163E000, hfrp, PD_NULL, CLK_NULL),
	[hfrp_bus] = REGBASE_V(0x116A5000, hfrp_bus, PD_NULL, CLK_NULL),
	{},
};

#define REGNAME(_base, _ofs, _name)	\
	{ .base = &rb[_base], .id = _base, .ofs = _ofs, .name = #_name }

static struct regname rn[] = {
	/* TOPCKGEN register */
	REGNAME(top, 0x0010, CLK_CFG_0),
	REGNAME(top, 0x0020, CLK_CFG_1),
	REGNAME(top, 0x0030, CLK_CFG_2),
	REGNAME(top, 0x0040, CLK_CFG_3),
	REGNAME(top, 0x0050, CLK_CFG_4),
	REGNAME(top, 0x0060, CLK_CFG_5),
	REGNAME(top, 0x0070, CLK_CFG_6),
	REGNAME(top, 0x0080, CLK_CFG_7),
	REGNAME(top, 0x0090, CLK_CFG_8),
	REGNAME(top, 0x00A0, CLK_CFG_9),
	REGNAME(top, 0x00B0, CLK_CFG_10),
	REGNAME(top, 0x00C0, CLK_CFG_11),
	REGNAME(top, 0x00D0, CLK_CFG_12),
	REGNAME(top, 0x00E0, CLK_CFG_13),
	REGNAME(top, 0x00F0, CLK_CFG_14),
	REGNAME(top, 0x0100, CLK_CFG_15),
	REGNAME(top, 0x0110, CLK_CFG_16),
	REGNAME(top, 0x0120, CLK_CFG_17),
	REGNAME(top, 0x0230, CKSTA_REG),
	REGNAME(top, 0x0234, CKSTA_REG1),
	REGNAME(top, 0x0238, CKSTA_REG2),
	REGNAME(top, 0x0320, CLK_AUDDIV_0),
	REGNAME(top, 0x0810, CKSYS2_CLK_CFG_0),
	REGNAME(top, 0x0820, CKSYS2_CLK_CFG_1),
	REGNAME(top, 0x0830, CKSYS2_CLK_CFG_2),
	REGNAME(top, 0x0840, CKSYS2_CLK_CFG_3),
	REGNAME(top, 0x0850, CKSYS2_CLK_CFG_4),
	REGNAME(top, 0x0860, CKSYS2_CLK_CFG_5),
	REGNAME(top, 0x0870, CKSYS2_CLK_CFG_6),
	REGNAME(top, 0x0328, CLK_AUDDIV_2),
	REGNAME(top, 0x0334, CLK_AUDDIV_3),
	REGNAME(top, 0x0338, CLK_AUDDIV_4),
	REGNAME(top, 0x033C, CLK_AUDDIV_5),
	/* INFRA_INFRACFG_AO_REG register */
	REGNAME(infracfg_ao, 0x90, MODULE_CG_0),
	/* APMIXEDSYS register */
	REGNAME(apmixed, 0x350, MAINPLL_CON0),
	REGNAME(apmixed, 0x354, MAINPLL_CON1),
	REGNAME(apmixed, 0x358, MAINPLL_CON2),
	REGNAME(apmixed, 0x35c, MAINPLL_CON3),
	REGNAME(apmixed, 0x308, UNIVPLL_CON0),
	REGNAME(apmixed, 0x30c, UNIVPLL_CON1),
	REGNAME(apmixed, 0x310, UNIVPLL_CON2),
	REGNAME(apmixed, 0x314, UNIVPLL_CON3),
	REGNAME(apmixed, 0x3a0, MMPLL_CON0),
	REGNAME(apmixed, 0x3a4, MMPLL_CON1),
	REGNAME(apmixed, 0x3a8, MMPLL_CON2),
	REGNAME(apmixed, 0x3ac, MMPLL_CON3),
	REGNAME(apmixed, 0x3b0, EMIPLL_CON0),
	REGNAME(apmixed, 0x3b4, EMIPLL_CON1),
	REGNAME(apmixed, 0x3b8, EMIPLL_CON2),
	REGNAME(apmixed, 0x3bc, EMIPLL_CON3),
	REGNAME(apmixed, 0x328, APLL1_CON0),
	REGNAME(apmixed, 0x32c, APLL1_CON1),
	REGNAME(apmixed, 0x330, APLL1_CON2),
	REGNAME(apmixed, 0x334, APLL1_CON3),
	REGNAME(apmixed, 0x338, APLL1_CON4),
	REGNAME(apmixed, 0x0040, APLL1_TUNER_CON0),
	REGNAME(apmixed, 0x000C, AP_PLL_CON3),
	REGNAME(apmixed, 0x33c, APLL2_CON0),
	REGNAME(apmixed, 0x340, APLL2_CON1),
	REGNAME(apmixed, 0x344, APLL2_CON2),
	REGNAME(apmixed, 0x348, APLL2_CON3),
	REGNAME(apmixed, 0x34c, APLL2_CON4),
	REGNAME(apmixed, 0x0044, APLL2_TUNER_CON0),
	REGNAME(apmixed, 0x000C, AP_PLL_CON3),
	REGNAME(apmixed, 0x360, MSDCPLL_CON0),
	REGNAME(apmixed, 0x364, MSDCPLL_CON1),
	REGNAME(apmixed, 0x368, MSDCPLL_CON2),
	REGNAME(apmixed, 0x36c, MSDCPLL_CON3),
	REGNAME(apmixed, 0x3c0, EMIPLL2_CON0),
	REGNAME(apmixed, 0x3c4, EMIPLL2_CON1),
	REGNAME(apmixed, 0x3c8, EMIPLL2_CON2),
	REGNAME(apmixed, 0x3cc, EMIPLL2_CON3),
	REGNAME(apmixed, 0x370, IMGPLL_CON0),
	REGNAME(apmixed, 0x374, IMGPLL_CON1),
	REGNAME(apmixed, 0x378, IMGPLL_CON2),
	REGNAME(apmixed, 0x37c, IMGPLL_CON3),
	REGNAME(apmixed, 0x248, TVDPLL_CON0),
	REGNAME(apmixed, 0x24c, TVDPLL_CON1),
	REGNAME(apmixed, 0x250, TVDPLL_CON2),
	REGNAME(apmixed, 0x254, TVDPLL_CON3),
	REGNAME(apmixed, 0x380, ADSPPLL_CON0),
	REGNAME(apmixed, 0x384, ADSPPLL_CON1),
	REGNAME(apmixed, 0x388, ADSPPLL_CON2),
	REGNAME(apmixed, 0x38c, ADSPPLL_CON3),
	/* INFRA_IFRBUS_AO_REG_BUS register */
	REGNAME(infra_ifrbus_ao_reg_bus, 0x020, INFRASYS_PROTECT_EN_1),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x02c, INFRASYS_PROTECT_RDY_STA_1),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x000, INFRASYS_PROTECT_EN_0),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x00c, INFRASYS_PROTECT_RDY_STA_0),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x120, EMISYS_PROTECT_EN_0),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x12c, EMISYS_PROTECT_RDY_STA_0),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x100, EMISYS_PROTECT_EN_1),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x10c, EMISYS_PROTECT_RDY_STA_1),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x1c0, CONNSYS_PROTECT_EN_0),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x1cc, CONNSYS_PROTECT_RDY_STA_0),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x160, MCUSYS_PROTECT_EN_0),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x16c, MCUSYS_PROTECT_RDY_STA_0),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x1a0, MFGSYS_PROTECT_EN_0),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x1ac, MFGSYS_PROTECT_RDY_STA_0),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x200, MMSYS_PROTECT_EN_1),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x20c, MMSYS_PROTECT_RDY_STA_1),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x260, CCUSYS_PROTECT_EN_0),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x26c, CCUSYS_PROTECT_RDY_STA_0),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x0e0, PERISYS_PROTECT_EN_0),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x0ec, PERISYS_PROTECT_RDY_STA_0),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x800, SSR_PROTECT_EN_0),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x80c, SSR_PROTECT_RDY_STA_0),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x220, MMSYS_PROTECT_EN_2),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x22c, MMSYS_PROTECT_RDY_STA_2),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x1e0, MMSYS_PROTECT_EN_0),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x1ec, MMSYS_PROTECT_RDY_STA_0),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x2A0, MMSYS_PROTECT_EN_3),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x2ac, MMSYS_PROTECT_RDY_STA_3),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x280, APUSYS_PROTECT_EN_0),
	REGNAME(infra_ifrbus_ao_reg_bus, 0x28c, APUSYS_PROTECT_RDY_STA_0),
	/* EMI_NEMICFG_AO_MEM_PROT_REG_BUS register */
	REGNAME(emi_nemicfg_ao_mem_prot_reg_bus, 0x80, GLITCH_PROTECT_EN),
	REGNAME(emi_nemicfg_ao_mem_prot_reg_bus, 0x8c, GLITCH_PROTECT_RDY),
	/* EMI_SEMICFG_AO_MEM_PROT_REG_BUS register */
	REGNAME(emi_semicfg_ao_mem_prot_reg_bus, 0x80, GLITCH_PROTECT_EN),
	REGNAME(emi_semicfg_ao_mem_prot_reg_bus, 0x8c, GLITCH_PROTECT_RDY),
	/* PERICFG_AO register */
	REGNAME(perao, 0x10, PERI_CG_0),
	REGNAME(perao, 0x14, PERI_CG_1),
	REGNAME(perao, 0x18, PERI_CG_2),
	/* AFE register */
	REGNAME(afe, 0x0, AUDIO_TOP_0),
	REGNAME(afe, 0x4, AUDIO_TOP_1),
	REGNAME(afe, 0x8, AUDIO_TOP_2),
	REGNAME(afe, 0xC, AUDIO_TOP_3),
	REGNAME(afe, 0x10, AUDIO_TOP_4),
	/* IMP_IIC_WRAP_C register */
	REGNAME(impc, 0xE00, AP_CLOCK_CG),
	/* UFSCFG_AO_BUS register */
	REGNAME(ufscfg_ao_bus, 0x50, UFS_AO2FE_SLPPROT_EN),
	REGNAME(ufscfg_ao_bus, 0x5c, UFS_AO2FE_SLPPROT_RDY_STA),
	/* UFSCFG_AO register */
	REGNAME(ufsao, 0x4, UFS_AO_CG_0),
	/* UFSCFG_PDN register */
	REGNAME(ufspdn, 0x4, UFS_PDN_CG_0),
	/* IMP_IIC_WRAP_EN register */
	REGNAME(impen, 0xE00, AP_CLOCK_CG),
	/* IMP_IIC_WRAP_E register */
	REGNAME(impe, 0xE00, AP_CLOCK_CG),
	/* IMP_IIC_WRAP_S register */
	REGNAME(imps, 0xE00, AP_CLOCK_CG),
	/* IMP_IIC_WRAP_ES register */
	REGNAME(impes, 0xE00, AP_CLOCK_CG),
	/* IMP_IIC_WRAP_W register */
	REGNAME(impw, 0xE00, AP_CLOCK_CG),
	/* IMP_IIC_WRAP_N register */
	REGNAME(impn, 0xE00, AP_CLOCK_CG),
	/* MFGPLL_PLL_CTRL register */
	REGNAME(mfg_ao, 0x8, MFGPLL_CON0),
	REGNAME(mfg_ao, 0xc, MFGPLL_CON1),
	REGNAME(mfg_ao, 0x10, MFGPLL_CON2),
	REGNAME(mfg_ao, 0x14, MFGPLL_CON3),
	/* MFGSCPLL_PLL_CTRL register */
	REGNAME(mfgsc_ao, 0x8, MFGSCPLL_CON0),
	REGNAME(mfgsc_ao, 0xc, MFGSCPLL_CON1),
	REGNAME(mfgsc_ao, 0x10, MFGSCPLL_CON2),
	REGNAME(mfgsc_ao, 0x14, MFGSCPLL_CON3),
	/* DISPSYS_CONFIG register */
	REGNAME(mm, 0x100, MMSYS_CG_0),
	REGNAME(mm, 0x110, MMSYS_CG_1),
	/* DISPSYS1_CONFIG register */
	REGNAME(mm1, 0x100, MMSYS1_CG_0),
	REGNAME(mm1, 0x110, MMSYS1_CG_1),
	/* OVLSYS_CONFIG register */
	REGNAME(ovl, 0x100, OVLSYS_CG_0),
	/* IMGSYS_MAIN register */
	REGNAME(img, 0x50, IMG_IPE_CG),
	REGNAME(img, 0x0, IMG_MAIN_CG),
	/* DIP_TOP_DIP1 register */
	REGNAME(dip_top_dip1, 0x0, MACRO_CG),
	/* DIP_NR1_DIP1 register */
	REGNAME(dip_nr1_dip1, 0x0, MACRO_CG),
	/* DIP_NR2_DIP1 register */
	REGNAME(dip_nr2_dip1, 0x0, MACRO_CG),
	/* WPE1_DIP1 register */
	REGNAME(wpe1_dip1, 0x0, MACRO_CG),
	/* WPE2_DIP1 register */
	REGNAME(wpe2_dip1, 0x0, MACRO_CG),
	/* WPE3_DIP1 register */
	REGNAME(wpe3_dip1, 0x0, MACRO_CG),
	/* TRAW_DIP1 register */
	REGNAME(traw_dip1, 0x0, MACRO_CG),
	/* TRAW_CAP_DIP1 register */
	REGNAME(traw_cap_dip1, 0x0, MACRO_CG),
	/* IMG_VCORE_D1A register */
	REGNAME(img_v, 0x0, IMG_VCORE_CG_0),
	/* VDEC_SOC_GCON_BASE register */
	REGNAME(vde1, 0x8, LARB_CKEN_CON),
	REGNAME(vde1, 0x200, LAT_CKEN),
	REGNAME(vde1, 0x0, VDEC_CKEN),
	/* VDEC_GCON_BASE register */
	REGNAME(vde2, 0x8, LARB_CKEN_CON),
	REGNAME(vde2, 0x200, LAT_CKEN),
	REGNAME(vde2, 0x0, VDEC_CKEN),
	/* VENC_GCON register */
	REGNAME(ven1, 0x0, VENCSYS_CG),
	/* VENC_GCON_CORE1 register */
	REGNAME(ven2, 0x0, VENCSYS_CG),
	/* SPM register */
	REGNAME(spm, 0xE00, MD1_PWR_CON),
	REGNAME(spm, 0xF28, PWR_STATUS),
	REGNAME(spm, 0xF2C, PWR_STATUS_2ND),
	REGNAME(spm, 0xF18, MD_BUCK_ISO_CON),
	REGNAME(spm, 0xE04, CONN_PWR_CON),
	REGNAME(spm, 0xE10, PERI_USB0_PWR_CON),
	REGNAME(spm, 0xE14, PERI_AUDIO_PWR_CON),
	REGNAME(spm, 0xE18, UFS0_PWR_CON),
	REGNAME(spm, 0xE1C, UFS0_PHY_PWR_CON),
	REGNAME(spm, 0xE2C, ADSP_TOP_PWR_CON),
	REGNAME(spm, 0xE30, ADSP_INFRA_PWR_CON),
	REGNAME(spm, 0xE34, ADSP_AO_PWR_CON),
	REGNAME(spm, 0xE38, ISP_TRAW_PWR_CON),
	REGNAME(spm, 0xE3C, ISP_DIP1_PWR_CON),
	REGNAME(spm, 0xE40, ISP_MAIN_PWR_CON),
	REGNAME(spm, 0xE44, ISP_VCORE_PWR_CON),
	REGNAME(spm, 0xE48, VDE0_PWR_CON),
	REGNAME(spm, 0xE4C, VDE1_PWR_CON),
	REGNAME(spm, 0xE50, VEN0_PWR_CON),
	REGNAME(spm, 0xE54, VEN1_PWR_CON),
	REGNAME(spm, 0xE58, CAM_MRAW_PWR_CON),
	REGNAME(spm, 0xE5C, CAM_SUBA_PWR_CON),
	REGNAME(spm, 0xE60, CAM_SUBB_PWR_CON),
	REGNAME(spm, 0xE64, CAM_SUBC_PWR_CON),
	REGNAME(spm, 0xE68, CAM_MAIN_PWR_CON),
	REGNAME(spm, 0xE6C, CAM_VCORE_PWR_CON),
	REGNAME(spm, 0xE70, CAM_CCU_PWR_CON),
	REGNAME(spm, 0xE74, CAM_CCU_AO_PWR_CON),
	REGNAME(spm, 0xE78, DISP_VCORE_PWR_CON),
	REGNAME(spm, 0xE7C, MML0_PWR_CON),
	REGNAME(spm, 0xE80, MML1_PWR_CON),
	REGNAME(spm, 0xE84, DIS0_PWR_CON),
	REGNAME(spm, 0xE88, DIS1_PWR_CON),
	REGNAME(spm, 0xE8C, OVL0_PWR_CON),
	REGNAME(spm, 0xE90, MM_INFRA_PWR_CON),
	REGNAME(spm, 0xE94, MM_PROC_PWR_CON),
	REGNAME(spm, 0xE98, DP_TX_PWR_CON),
	REGNAME(spm, 0xEB8, CSI_RX_PWR_CON),
	REGNAME(spm, 0xEBC, SSRSYS_PWR_CON),
	REGNAME(spm, 0xEC8, MFG0_PWR_CON),
	REGNAME(spm, 0x200, MFG_RPC_MFG_PWR_CON_STATUS),
	REGNAME(spm, 0x204, MFG_RPC_MFG_PWR_CON_2ND_STATUS),
	/* VLPCFG_BUS register */
	REGNAME(vlpcfg, 0x0210, VLP_TOPAXI_PROTECTEN),
	REGNAME(vlpcfg, 0x0220, VLP_TOPAXI_PROTECTEN_STA1),
	REGNAME(vlpcfg, 0x0230, VLP_TOPAXI_PROTECTEN1),
	REGNAME(vlpcfg, 0x0240, VLP_TOPAXI_PROTECTEN1_STA1),
	/* VLP_CKSYS register */
	REGNAME(vlp_ck, 0x0010, VLP_CLK_CFG_0),
	REGNAME(vlp_ck, 0x0020, VLP_CLK_CFG_1),
	REGNAME(vlp_ck, 0x0030, VLP_CLK_CFG_2),
	REGNAME(vlp_ck, 0x0040, VLP_CLK_CFG_3),
	REGNAME(vlp_ck, 0x0050, VLP_CLK_CFG_4),
	REGNAME(vlp_ck, 0x0060, VLP_CLK_CFG_5),
	REGNAME(vlp_ck, 0x0070, VLP_CLK_CFG_6),
	REGNAME(vlp_ck, 0x0080, VLP_CLK_CFG_7),
	/* HFRP register */
	REGNAME(hfrp, 0xE40, ISP_TRAW_PWR_CON),
	REGNAME(hfrp, 0xE44, ISP_DIP1_PWR_CON),
	REGNAME(hfrp, 0xE48, ISP_MAIN_PWR_CON),
	REGNAME(hfrp, 0xE4C, ISP_VCORE_PWR_CON),
	REGNAME(hfrp, 0xE50, VDE0_PWR_CON),
	REGNAME(hfrp, 0xE54, VDE1_PWR_CON),
	REGNAME(hfrp, 0xE60, VEN0_PWR_CON),
	REGNAME(hfrp, 0xE64, VEN1_PWR_CON),
	REGNAME(hfrp, 0xE6C, CAM_MRAW_PWR_CON),
	REGNAME(hfrp, 0xE70, CAM_SUBA_PWR_CON),
	REGNAME(hfrp, 0xE74, CAM_SUBB_PWR_CON),
	REGNAME(hfrp, 0xE78, CAM_SUBC_PWR_CON),
	REGNAME(hfrp, 0xE7C, CAM_MAIN_PWR_CON),
	REGNAME(hfrp, 0xE80, CAM_VCORE_PWR_CON),
	REGNAME(hfrp, 0xE84, CAM_CCU_PWR_CON),
	REGNAME(hfrp, 0xE88, CAM_CCU_AO_PWR_CON),
	REGNAME(hfrp, 0xE8C, DISP_VCORE_PWR_CON),
	REGNAME(hfrp, 0xE90, MML0_PWR_CON),
	REGNAME(hfrp, 0xE94, MML1_PWR_CON),
	REGNAME(hfrp, 0xE98, DIS0_PWR_CON),
	REGNAME(hfrp, 0xE9C, DIS1_PWR_CON),
	REGNAME(hfrp, 0xEA0, OVL0_PWR_CON),
	REGNAME(hfrp, 0xEA8, MM_INFRA_PWR_CON),
	REGNAME(hfrp, 0xEB0, DP_TX_PWR_CON),
	REGNAME(hfrp, 0xEB4, CSI_RX_PWR_CON),
	REGNAME(hfrp_bus, 0x450, HFRP_MMSYS_PROTECT_EN_0),
	REGNAME(hfrp_bus, 0x470, HFRP_MMSYS_PROTECT_RDY_STA_0),
	REGNAME(hfrp_bus, 0x454, HFRP_MMSYS_PROTECT_EN_1),
	REGNAME(hfrp_bus, 0x474, HFRP_MMSYS_PROTECT_RDY_STA_1),
	REGNAME(hfrp_bus, 0x458, HFRP_MMSYS_PROTECT_EN_2),
	REGNAME(hfrp_bus, 0x478, HFRP_MMSYS_PROTECT_RDY_STA_2),
	REGNAME(hfrp_bus, 0x45C, HFRP_MMSYS_PROTECT_EN_3),
	REGNAME(hfrp_bus, 0x47C, HFRP_MMSYS_PROTECT_RDY_STA_3),
	REGNAME(hfrp_bus, 0x460, HFRP_MMSYS_PROTECT_EN_4),
	REGNAME(hfrp_bus, 0x480, HFRP_MMSYS_PROTECT_RDY_STA_4),
	/* CAM_MAIN_R1A register */
	REGNAME(cam_m, 0x0, CAM_MAIN_CG_0),
	REGNAME(cam_m, 0x4C, CAM_MAIN_CG_1),
	REGNAME(cam_m, 0xC0, CAM_MAIN_FUNCTION_DIS),
	/* CAMSYS_MRAW register */
	REGNAME(cam_mr, 0x0, CAMSYS_CG),
	/* CAMSYS_IPE register */
	REGNAME(camsys_ipe, 0x0, CAMSYS_IPE_CG),
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
	/* CAMSYS_RAWC register */
	REGNAME(cam_rc, 0x0, CAMSYS_CG),
	/* CAMSYS_RMSC register */
	REGNAME(camsys_rmsc, 0x0, CAMSYS_CG),
	/* CAMSYS_YUVC register */
	REGNAME(cam_yc, 0x0, CAMSYS_CG),
	/* CCU_MAIN register */
	REGNAME(ccu, 0x0, CCUSYS_CG),
	/* CAM_VCORE_R1A register */
	REGNAME(camv, 0xA0, CAM_VCORE_CG_0),
	REGNAME(camv, 0x2C, CAM_VCORE_SUBCOMM_DCM_DIS),
	/* MMINFRA_AO_CONFIG register */
	REGNAME(mminfra_ao_config, 0x100, MMINFRA_CG_0),
	REGNAME(mminfra_ao_config, 0x110, MMINFRA_CG_1),
	/* MDPSYS_CONFIG register */
	REGNAME(mdp, 0x100, MDPSYS_CG_0),
	REGNAME(mdp, 0x110, MDPSYS_CG_1),
	/* MDPSYS1_CONFIG register */
	REGNAME(mdp1, 0x100, MDPSYS_CG_0),
	REGNAME(mdp1, 0x110, MDPSYS_CG_1),
	/* CCIPLL_PLL_CTRL register */
	REGNAME(cci, 0x8, CCIPLL_CON0),
	REGNAME(cci, 0xc, CCIPLL_CON1),
	REGNAME(cci, 0x10, CCIPLL_CON2),
	REGNAME(cci, 0x14, CCIPLL_CON3),
	/* ARMPLL_LL_PLL_CTRL register */
	REGNAME(cpu_ll, 0x8, ARMPLL_LL_CON0),
	REGNAME(cpu_ll, 0xc, ARMPLL_LL_CON1),
	REGNAME(cpu_ll, 0x10, ARMPLL_LL_CON2),
	REGNAME(cpu_ll, 0x14, ARMPLL_LL_CON3),
	/* ARMPLL_BL_PLL_CTRL register */
	REGNAME(cpu_bl, 0x8, ARMPLL_BL_CON0),
	REGNAME(cpu_bl, 0xc, ARMPLL_BL_CON1),
	REGNAME(cpu_bl, 0x10, ARMPLL_BL_CON2),
	REGNAME(cpu_bl, 0x14, ARMPLL_BL_CON3),
	/* ARMPLL_B_PLL_CTRL register */
	REGNAME(cpu_b, 0x8, ARMPLL_B_CON0),
	REGNAME(cpu_b, 0xc, ARMPLL_B_CON1),
	REGNAME(cpu_b, 0x10, ARMPLL_B_CON2),
	REGNAME(cpu_b, 0x14, ARMPLL_B_CON3),
	/* PTPPLL_PLL_CTRL register */
	REGNAME(ptp, 0x8, PTPPLL_CON0),
	REGNAME(ptp, 0xc, PTPPLL_CON1),
	REGNAME(ptp, 0x10, PTPPLL_CON2),
	REGNAME(ptp, 0x14, PTPPLL_CON3),
	/* HWV register */
	REGNAME(hwv, 0x1F04, HWV_ADDR_HISTORY_0),
	REGNAME(hwv, 0x1F08, HWV_ADDR_HISTORY_1),
	REGNAME(hwv, 0x1F0C, HWV_ADDR_HISTORY_2),
	REGNAME(hwv, 0x1F10, HWV_ADDR_HISTORY_3),
	REGNAME(hwv, 0x1F14, HWV_ADDR_HISTORY_4),
	REGNAME(hwv, 0x1F18, HWV_ADDR_HISTORY_5),
	REGNAME(hwv, 0x1F1C, HWV_ADDR_HISTORY_6),
	REGNAME(hwv, 0x1F20, HWV_ADDR_HISTORY_7),
	REGNAME(hwv, 0x1F24, HWV_ADDR_HISTORY_8),
	REGNAME(hwv, 0x1F28, HWV_ADDR_HISTORY_9),
	REGNAME(hwv, 0x1F2C, HWV_ADDR_HISTORY_10),
	REGNAME(hwv, 0x1F30, HWV_ADDR_HISTORY_11),
	REGNAME(hwv, 0x1F34, HWV_ADDR_HISTORY_12),
	REGNAME(hwv, 0x1F38, HWV_ADDR_HISTORY_13),
	REGNAME(hwv, 0x1F3C, HWV_ADDR_HISTORY_14),
	REGNAME(hwv, 0x1F40, HWV_ADDR_HISTORY_15),
	REGNAME(hwv, 0x1F44, HWV_DATA_HISTORY_0),
	REGNAME(hwv, 0x1F48, HWV_DATA_HISTORY_1),
	REGNAME(hwv, 0x1F4C, HWV_DATA_HISTORY_2),
	REGNAME(hwv, 0x1F50, HWV_DATA_HISTORY_3),
	REGNAME(hwv, 0x1F54, HWV_DATA_HISTORY_4),
	REGNAME(hwv, 0x1F58, HWV_DATA_HISTORY_5),
	REGNAME(hwv, 0x1F5C, HWV_DATA_HISTORY_6),
	REGNAME(hwv, 0x1F60, HWV_DATA_HISTORY_7),
	REGNAME(hwv, 0x1F64, HWV_DATA_HISTORY_8),
	REGNAME(hwv, 0x1F68, HWV_DATA_HISTORY_9),
	REGNAME(hwv, 0x1F6C, HWV_DATA_HISTORY_10),
	REGNAME(hwv, 0x1F70, HWV_DATA_HISTORY_11),
	REGNAME(hwv, 0x1F74, HWV_DATA_HISTORY_12),
	REGNAME(hwv, 0x1F78, HWV_DATA_HISTORY_13),
	REGNAME(hwv, 0x1F7C, HWV_DATA_HISTORY_14),
	REGNAME(hwv, 0x1F80, HWV_DATA_HISTORY_15),
	REGNAME(hwv, 0x155C, HWV_DOMAIN_KEY),
	REGNAME(hwv, 0x1F84, HWV_IDX_POINTER),
	REGNAME(hwv, 0x1C00, HWV_VIP_CG_DONE_0),
	REGNAME(hwv, 0x1C04, HWV_VIP_CG_DONE_1),
	REGNAME(hwv, 0x1C08, HWV_VIP_CG_DONE_2),
	REGNAME(hwv, 0x1C0C, HWV_VIP_CG_DONE_3),
	REGNAME(hwv, 0x1C10, HWV_VIP_CG_DONE_4),
	REGNAME(hwv, 0x1C14, HWV_VIP_CG_DONE_5),
	REGNAME(hwv, 0x1C18, HWV_VIP_CG_DONE_6),
	REGNAME(hwv, 0x1C1C, HWV_VIP_CG_DONE_7),
	REGNAME(hwv, 0x1C20, HWV_VIP_CG_DONE_8),
	REGNAME(hwv, 0x1C24, HWV_VIP_CG_DONE_9),
	REGNAME(hwv, 0x1C28, HWV_VIP_CG_DONE_10),
	REGNAME(hwv, 0x1C2C, HWV_VIP_CG_DONE_11),
	REGNAME(hwv, 0x1C30, HWV_VIP_CG_DONE_12),
	REGNAME(hwv, 0x1C34, HWV_VIP_CG_DONE_13),
	REGNAME(hwv, 0x1C38, HWV_VIP_CG_DONE_14),
	REGNAME(hwv, 0x1C3C, HWV_VIP_CG_DONE_15),
	REGNAME(hwv, 0x1C40, HWV_VIP_CG_DONE_16),
	REGNAME(hwv, 0x1C44, HWV_VIP_CG_DONE_17),
	REGNAME(hwv, 0x1C48, HWV_VIP_CG_DONE_18),
	REGNAME(hwv, 0x1C4C, HWV_VIP_CG_DONE_19),
	REGNAME(hwv, 0x1C50, HWV_VIP_CG_DONE_20),
	REGNAME(hwv, 0x1C54, HWV_VIP_CG_DONE_21),
	REGNAME(hwv, 0x1C58, HWV_VIP_CG_DONE_22),
	REGNAME(hwv, 0x1C5C, HWV_VIP_CG_DONE_23),
	REGNAME(hwv, 0x1C60, HWV_VIP_CG_DONE_24),
	REGNAME(hwv, 0x1C64, HWV_VIP_CG_DONE_25),
	REGNAME(hwv, 0x1C68, HWV_VIP_CG_DONE_26),
	REGNAME(hwv, 0x1C6C, HWV_VIP_CG_DONE_27),
	REGNAME(hwv, 0x1C70, HWV_VIP_CG_DONE_28),
	REGNAME(hwv, 0x1C74, HWV_VIP_CG_DONE_29),
	REGNAME(hwv, 0x1C78, HWV_VIP_CG_DONE_30),
	REGNAME(hwv, 0x1C7C, HWV_VIP_CG_DONE_31),
	REGNAME(hwv, 0x1C80, HWV_VIP_CG_DONE_32),
	REGNAME(hwv, 0x1C84, HWV_VIP_CG_DONE_33),
	REGNAME(hwv, 0x1C88, HWV_VIP_CG_DONE_34),
	REGNAME(hwv, 0x1C8C, HWV_VIP_CG_DONE_35),
	REGNAME(hwv, 0x1C90, HWV_VIP_CG_DONE_36),
	REGNAME(hwv, 0x1C94, HWV_VIP_CG_DONE_37),
	REGNAME(hwv, 0x1C98, HWV_VIP_CG_DONE_38),
	REGNAME(hwv, 0x1C9C, HWV_VIP_CG_DONE_39),
	REGNAME(hwv, 0x1CA0, HWV_VIP_CG_DONE_40),
	REGNAME(hwv, 0x1CA4, HWV_VIP_CG_DONE_41),
	REGNAME(hwv, 0x1CA8, HWV_VIP_CG_DONE_42),
	REGNAME(hwv, 0x1CAC, HWV_VIP_CG_DONE_43),
	REGNAME(hwv, 0x1CB0, HWV_VIP_CG_DONE_44),
	REGNAME(hwv, 0x1CB4, HWV_VIP_CG_DONE_45),
	REGNAME(hwv, 0x1CB8, HWV_VIP_CG_DONE_46),
	REGNAME(hwv, 0x1CBC, HWV_VIP_CG_DONE_47),
	REGNAME(hwv, 0x1CC0, HWV_VIP_CG_DONE_48),
	REGNAME(hwv, 0x1CC4, HWV_VIP_CG_DONE_49),
	REGNAME(hwv, 0x1900, HWV_VIP_CG_ENABLE_0),
	REGNAME(hwv, 0x1904, HWV_VIP_CG_ENABLE_1),
	REGNAME(hwv, 0x1908, HWV_VIP_CG_ENABLE_2),
	REGNAME(hwv, 0x190C, HWV_VIP_CG_ENABLE_3),
	REGNAME(hwv, 0x1910, HWV_VIP_CG_ENABLE_4),
	REGNAME(hwv, 0x1914, HWV_VIP_CG_ENABLE_5),
	REGNAME(hwv, 0x1918, HWV_VIP_CG_ENABLE_6),
	REGNAME(hwv, 0x191C, HWV_VIP_CG_ENABLE_7),
	REGNAME(hwv, 0x1920, HWV_VIP_CG_ENABLE_8),
	REGNAME(hwv, 0x1924, HWV_VIP_CG_ENABLE_9),
	REGNAME(hwv, 0x1928, HWV_VIP_CG_ENABLE_10),
	REGNAME(hwv, 0x192C, HWV_VIP_CG_ENABLE_11),
	REGNAME(hwv, 0x1930, HWV_VIP_CG_ENABLE_12),
	REGNAME(hwv, 0x1934, HWV_VIP_CG_ENABLE_13),
	REGNAME(hwv, 0x1938, HWV_VIP_CG_ENABLE_14),
	REGNAME(hwv, 0x193C, HWV_VIP_CG_ENABLE_15),
	REGNAME(hwv, 0x1940, HWV_VIP_CG_ENABLE_16),
	REGNAME(hwv, 0x1944, HWV_VIP_CG_ENABLE_17),
	REGNAME(hwv, 0x1948, HWV_VIP_CG_ENABLE_18),
	REGNAME(hwv, 0x194C, HWV_VIP_CG_ENABLE_19),
	REGNAME(hwv, 0x1950, HWV_VIP_CG_ENABLE_20),
	REGNAME(hwv, 0x1954, HWV_VIP_CG_ENABLE_21),
	REGNAME(hwv, 0x1958, HWV_VIP_CG_ENABLE_22),
	REGNAME(hwv, 0x195C, HWV_VIP_CG_ENABLE_23),
	REGNAME(hwv, 0x1960, HWV_VIP_CG_ENABLE_24),
	REGNAME(hwv, 0x1964, HWV_VIP_CG_ENABLE_25),
	REGNAME(hwv, 0x1968, HWV_VIP_CG_ENABLE_26),
	REGNAME(hwv, 0x196C, HWV_VIP_CG_ENABLE_27),
	REGNAME(hwv, 0x1970, HWV_VIP_CG_ENABLE_28),
	REGNAME(hwv, 0x1974, HWV_VIP_CG_ENABLE_29),
	REGNAME(hwv, 0x1978, HWV_VIP_CG_ENABLE_30),
	REGNAME(hwv, 0x197C, HWV_VIP_CG_ENABLE_31),
	REGNAME(hwv, 0x1980, HWV_VIP_CG_ENABLE_32),
	REGNAME(hwv, 0x1984, HWV_VIP_CG_ENABLE_33),
	REGNAME(hwv, 0x1988, HWV_VIP_CG_ENABLE_34),
	REGNAME(hwv, 0x198C, HWV_VIP_CG_ENABLE_35),
	REGNAME(hwv, 0x1990, HWV_VIP_CG_ENABLE_36),
	REGNAME(hwv, 0x1994, HWV_VIP_CG_ENABLE_37),
	REGNAME(hwv, 0x1998, HWV_VIP_CG_ENABLE_38),
	REGNAME(hwv, 0x199C, HWV_VIP_CG_ENABLE_39),
	REGNAME(hwv, 0x19A0, HWV_VIP_CG_ENABLE_40),
	REGNAME(hwv, 0x19A4, HWV_VIP_CG_ENABLE_41),
	REGNAME(hwv, 0x19A8, HWV_VIP_CG_ENABLE_42),
	REGNAME(hwv, 0x19AC, HWV_VIP_CG_ENABLE_43),
	REGNAME(hwv, 0x19B0, HWV_VIP_CG_ENABLE_44),
	REGNAME(hwv, 0x19B4, HWV_VIP_CG_ENABLE_45),
	REGNAME(hwv, 0x19B8, HWV_VIP_CG_ENABLE_46),
	REGNAME(hwv, 0x19BC, HWV_VIP_CG_ENABLE_47),
	REGNAME(hwv, 0x19C0, HWV_VIP_CG_ENABLE_48),
	REGNAME(hwv, 0x19C4, HWV_VIP_CG_ENABLE_49),
	REGNAME(hwv, 0x1800, HWV_VIP_CG_STATUS_0),
	REGNAME(hwv, 0x1804, HWV_VIP_CG_STATUS_1),
	REGNAME(hwv, 0x1808, HWV_VIP_CG_STATUS_2),
	REGNAME(hwv, 0x180C, HWV_VIP_CG_STATUS_3),
	REGNAME(hwv, 0x1810, HWV_VIP_CG_STATUS_4),
	REGNAME(hwv, 0x1814, HWV_VIP_CG_STATUS_5),
	REGNAME(hwv, 0x1818, HWV_VIP_CG_STATUS_6),
	REGNAME(hwv, 0x181C, HWV_VIP_CG_STATUS_7),
	REGNAME(hwv, 0x1820, HWV_VIP_CG_STATUS_8),
	REGNAME(hwv, 0x1824, HWV_VIP_CG_STATUS_9),
	REGNAME(hwv, 0x1828, HWV_VIP_CG_STATUS_10),
	REGNAME(hwv, 0x182C, HWV_VIP_CG_STATUS_11),
	REGNAME(hwv, 0x1830, HWV_VIP_CG_STATUS_12),
	REGNAME(hwv, 0x1834, HWV_VIP_CG_STATUS_13),
	REGNAME(hwv, 0x1838, HWV_VIP_CG_STATUS_14),
	REGNAME(hwv, 0x183C, HWV_VIP_CG_STATUS_15),
	REGNAME(hwv, 0x1840, HWV_VIP_CG_STATUS_16),
	REGNAME(hwv, 0x1844, HWV_VIP_CG_STATUS_17),
	REGNAME(hwv, 0x1848, HWV_VIP_CG_STATUS_18),
	REGNAME(hwv, 0x184C, HWV_VIP_CG_STATUS_19),
	REGNAME(hwv, 0x1850, HWV_VIP_CG_STATUS_20),
	REGNAME(hwv, 0x1854, HWV_VIP_CG_STATUS_21),
	REGNAME(hwv, 0x1858, HWV_VIP_CG_STATUS_22),
	REGNAME(hwv, 0x185C, HWV_VIP_CG_STATUS_23),
	REGNAME(hwv, 0x1860, HWV_VIP_CG_STATUS_24),
	REGNAME(hwv, 0x1864, HWV_VIP_CG_STATUS_25),
	REGNAME(hwv, 0x1868, HWV_VIP_CG_STATUS_26),
	REGNAME(hwv, 0x186C, HWV_VIP_CG_STATUS_27),
	REGNAME(hwv, 0x1870, HWV_VIP_CG_STATUS_28),
	REGNAME(hwv, 0x1874, HWV_VIP_CG_STATUS_29),
	REGNAME(hwv, 0x1878, HWV_VIP_CG_STATUS_30),
	REGNAME(hwv, 0x187C, HWV_VIP_CG_STATUS_31),
	REGNAME(hwv, 0x1880, HWV_VIP_CG_STATUS_32),
	REGNAME(hwv, 0x1884, HWV_VIP_CG_STATUS_33),
	REGNAME(hwv, 0x1888, HWV_VIP_CG_STATUS_34),
	REGNAME(hwv, 0x188C, HWV_VIP_CG_STATUS_35),
	REGNAME(hwv, 0x1890, HWV_VIP_CG_STATUS_36),
	REGNAME(hwv, 0x1894, HWV_VIP_CG_STATUS_37),
	REGNAME(hwv, 0x1898, HWV_VIP_CG_STATUS_38),
	REGNAME(hwv, 0x189C, HWV_VIP_CG_STATUS_39),
	REGNAME(hwv, 0x18A0, HWV_VIP_CG_STATUS_40),
	REGNAME(hwv, 0x18A4, HWV_VIP_CG_STATUS_41),
	REGNAME(hwv, 0x18A8, HWV_VIP_CG_STATUS_42),
	REGNAME(hwv, 0x18AC, HWV_VIP_CG_STATUS_43),
	REGNAME(hwv, 0x18B0, HWV_VIP_CG_STATUS_44),
	REGNAME(hwv, 0x18B4, HWV_VIP_CG_STATUS_45),
	REGNAME(hwv, 0x18B8, HWV_VIP_CG_STATUS_46),
	REGNAME(hwv, 0x18BC, HWV_VIP_CG_STATUS_47),
	REGNAME(hwv, 0x18C0, HWV_VIP_CG_STATUS_48),
	REGNAME(hwv, 0x18C4, HWV_VIP_CG_STATUS_49),
	REGNAME(hwv, 0x0B98, HW_CCF_APU_MTCMOS_SET),
	REGNAME(hwv, 0x0B90, HW_CCF_APU_PLL_SET),
	REGNAME(hwv, 0x0198, HW_CCF_AP_MTCMOS_SET),
	REGNAME(hwv, 0x0190, HW_CCF_AP_PLL_SET),
	REGNAME(hwv, 0x1198, HW_CCF_GCE_MTCMOS_SET),
	REGNAME(hwv, 0x1190, HW_CCF_GCE_PLL_SET),
	REGNAME(hwv, 0x0798, HW_CCF_GPU_MTCMOS_SET),
	REGNAME(hwv, 0x0790, HW_CCF_GPU_PLL_SET),
	REGNAME(hwv, 0x1500, HW_CCF_INT_STATUS),
	REGNAME(hwv, 0x0598, HW_CCF_MD_MTCMOS_SET),
	REGNAME(hwv, 0x0590, HW_CCF_MD_PLL_SET),
	REGNAME(hwv, 0x0F98, HW_CCF_MMUP_MTCMOS_SET),
	REGNAME(hwv, 0x0F90, HW_CCF_MMUP_PLL_SET),
	REGNAME(hwv, 0x1470, HW_CCF_MTCMOS_CLR_STATUS),
	REGNAME(hwv, 0x141C, HW_CCF_MTCMOS_DONE),
	REGNAME(hwv, 0x1410, HW_CCF_MTCMOS_ENABLE),
	REGNAME(hwv, 0x14AC, HW_CCF_MTCMOS_FLOW_FLAG_CLR),
	REGNAME(hwv, 0x14A8, HW_CCF_MTCMOS_FLOW_FLAG_SET),
	REGNAME(hwv, 0x146C, HW_CCF_MTCMOS_SET_STATUS),
	REGNAME(hwv, 0x1414, HW_CCF_MTCMOS_STATUS),
	REGNAME(hwv, 0x1454, HW_CCF_MTCMOS_STATUS_CLR),
	REGNAME(hwv, 0x1468, HW_CCF_PLL_CLR_STATUS),
	REGNAME(hwv, 0x140C, HW_CCF_PLL_DONE),
	REGNAME(hwv, 0x1400, HW_CCF_PLL_ENABLE),
	REGNAME(hwv, 0x1464, HW_CCF_PLL_SET_STATUS),
	REGNAME(hwv, 0x1404, HW_CCF_PLL_STATUS),
	REGNAME(hwv, 0x1450, HW_CCF_PLL_STATUS_CLR),
	REGNAME(hwv, 0x1398, HW_CCF_SCP_MTCMOS_SET),
	REGNAME(hwv, 0x1390, HW_CCF_SCP_PLL_SET),
	REGNAME(hwv, 0x0D98, HW_CCF_SPM_MTCMOS_SET),
	REGNAME(hwv, 0x0D90, HW_CCF_SPM_PLL_SET),
	REGNAME(hwv, 0x0998, HW_CCF_SSPM_MTCMOS_SET),
	REGNAME(hwv, 0x0990, HW_CCF_SSPM_PLL_SET),
	REGNAME(hwv, 0x0398, HW_CCF_TEE_MTCMOS_SET),
	REGNAME(hwv, 0x0390, HW_CCF_TEE_PLL_SET),
	/* MM_HWV register */
	REGNAME(mm_hwv, 0x1F04, HWV_ADDR_HISTORY_0),
	REGNAME(mm_hwv, 0x1F08, HWV_ADDR_HISTORY_1),
	REGNAME(mm_hwv, 0x1F0C, HWV_ADDR_HISTORY_2),
	REGNAME(mm_hwv, 0x1F10, HWV_ADDR_HISTORY_3),
	REGNAME(mm_hwv, 0x1F14, HWV_ADDR_HISTORY_4),
	REGNAME(mm_hwv, 0x1F18, HWV_ADDR_HISTORY_5),
	REGNAME(mm_hwv, 0x1F1C, HWV_ADDR_HISTORY_6),
	REGNAME(mm_hwv, 0x1F20, HWV_ADDR_HISTORY_7),
	REGNAME(mm_hwv, 0x1F24, HWV_ADDR_HISTORY_8),
	REGNAME(mm_hwv, 0x1F28, HWV_ADDR_HISTORY_9),
	REGNAME(mm_hwv, 0x1F2C, HWV_ADDR_HISTORY_10),
	REGNAME(mm_hwv, 0x1F30, HWV_ADDR_HISTORY_11),
	REGNAME(mm_hwv, 0x1F34, HWV_ADDR_HISTORY_12),
	REGNAME(mm_hwv, 0x1F38, HWV_ADDR_HISTORY_13),
	REGNAME(mm_hwv, 0x1F3C, HWV_ADDR_HISTORY_14),
	REGNAME(mm_hwv, 0x1F40, HWV_ADDR_HISTORY_15),
	REGNAME(mm_hwv, 0x1F44, HWV_DATA_HISTORY_0),
	REGNAME(mm_hwv, 0x1F48, HWV_DATA_HISTORY_1),
	REGNAME(mm_hwv, 0x1F4C, HWV_DATA_HISTORY_2),
	REGNAME(mm_hwv, 0x1F50, HWV_DATA_HISTORY_3),
	REGNAME(mm_hwv, 0x1F54, HWV_DATA_HISTORY_4),
	REGNAME(mm_hwv, 0x1F58, HWV_DATA_HISTORY_5),
	REGNAME(mm_hwv, 0x1F5C, HWV_DATA_HISTORY_6),
	REGNAME(mm_hwv, 0x1F60, HWV_DATA_HISTORY_7),
	REGNAME(mm_hwv, 0x1F64, HWV_DATA_HISTORY_8),
	REGNAME(mm_hwv, 0x1F68, HWV_DATA_HISTORY_9),
	REGNAME(mm_hwv, 0x1F6C, HWV_DATA_HISTORY_10),
	REGNAME(mm_hwv, 0x1F70, HWV_DATA_HISTORY_11),
	REGNAME(mm_hwv, 0x1F74, HWV_DATA_HISTORY_12),
	REGNAME(mm_hwv, 0x1F78, HWV_DATA_HISTORY_13),
	REGNAME(mm_hwv, 0x1F7C, HWV_DATA_HISTORY_14),
	REGNAME(mm_hwv, 0x1F80, HWV_DATA_HISTORY_15),
	REGNAME(mm_hwv, 0x155C, HWV_DOMAIN_KEY),
	REGNAME(mm_hwv, 0x1F84, HWV_IDX_POINTER),
	REGNAME(mm_hwv, 0x1C00, HWV_VIP_CG_DONE_0),
	REGNAME(mm_hwv, 0x1C04, HWV_VIP_CG_DONE_1),
	REGNAME(mm_hwv, 0x1C08, HWV_VIP_CG_DONE_2),
	REGNAME(mm_hwv, 0x1C0C, HWV_VIP_CG_DONE_3),
	REGNAME(mm_hwv, 0x1C10, HWV_VIP_CG_DONE_4),
	REGNAME(mm_hwv, 0x1C14, HWV_VIP_CG_DONE_5),
	REGNAME(mm_hwv, 0x1C18, HWV_VIP_CG_DONE_6),
	REGNAME(mm_hwv, 0x1C1C, HWV_VIP_CG_DONE_7),
	REGNAME(mm_hwv, 0x1C20, HWV_VIP_CG_DONE_8),
	REGNAME(mm_hwv, 0x1C24, HWV_VIP_CG_DONE_9),
	REGNAME(mm_hwv, 0x1C28, HWV_VIP_CG_DONE_10),
	REGNAME(mm_hwv, 0x1C2C, HWV_VIP_CG_DONE_11),
	REGNAME(mm_hwv, 0x1C30, HWV_VIP_CG_DONE_12),
	REGNAME(mm_hwv, 0x1C34, HWV_VIP_CG_DONE_13),
	REGNAME(mm_hwv, 0x1C38, HWV_VIP_CG_DONE_14),
	REGNAME(mm_hwv, 0x1C3C, HWV_VIP_CG_DONE_15),
	REGNAME(mm_hwv, 0x1C40, HWV_VIP_CG_DONE_16),
	REGNAME(mm_hwv, 0x1C44, HWV_VIP_CG_DONE_17),
	REGNAME(mm_hwv, 0x1C48, HWV_VIP_CG_DONE_18),
	REGNAME(mm_hwv, 0x1C4C, HWV_VIP_CG_DONE_19),
	REGNAME(mm_hwv, 0x1C50, HWV_VIP_CG_DONE_20),
	REGNAME(mm_hwv, 0x1C54, HWV_VIP_CG_DONE_21),
	REGNAME(mm_hwv, 0x1C58, HWV_VIP_CG_DONE_22),
	REGNAME(mm_hwv, 0x1C5C, HWV_VIP_CG_DONE_23),
	REGNAME(mm_hwv, 0x1C60, HWV_VIP_CG_DONE_24),
	REGNAME(mm_hwv, 0x1C64, HWV_VIP_CG_DONE_25),
	REGNAME(mm_hwv, 0x1C68, HWV_VIP_CG_DONE_26),
	REGNAME(mm_hwv, 0x1C6C, HWV_VIP_CG_DONE_27),
	REGNAME(mm_hwv, 0x1C70, HWV_VIP_CG_DONE_28),
	REGNAME(mm_hwv, 0x1C74, HWV_VIP_CG_DONE_29),
	REGNAME(mm_hwv, 0x1C78, HWV_VIP_CG_DONE_30),
	REGNAME(mm_hwv, 0x1C7C, HWV_VIP_CG_DONE_31),
	REGNAME(mm_hwv, 0x1C80, HWV_VIP_CG_DONE_32),
	REGNAME(mm_hwv, 0x1C84, HWV_VIP_CG_DONE_33),
	REGNAME(mm_hwv, 0x1C88, HWV_VIP_CG_DONE_34),
	REGNAME(mm_hwv, 0x1C8C, HWV_VIP_CG_DONE_35),
	REGNAME(mm_hwv, 0x1C90, HWV_VIP_CG_DONE_36),
	REGNAME(mm_hwv, 0x1C94, HWV_VIP_CG_DONE_37),
	REGNAME(mm_hwv, 0x1C98, HWV_VIP_CG_DONE_38),
	REGNAME(mm_hwv, 0x1C9C, HWV_VIP_CG_DONE_39),
	REGNAME(mm_hwv, 0x1CA0, HWV_VIP_CG_DONE_40),
	REGNAME(mm_hwv, 0x1CA4, HWV_VIP_CG_DONE_41),
	REGNAME(mm_hwv, 0x1CA8, HWV_VIP_CG_DONE_42),
	REGNAME(mm_hwv, 0x1CAC, HWV_VIP_CG_DONE_43),
	REGNAME(mm_hwv, 0x1CB0, HWV_VIP_CG_DONE_44),
	REGNAME(mm_hwv, 0x1CB4, HWV_VIP_CG_DONE_45),
	REGNAME(mm_hwv, 0x1CB8, HWV_VIP_CG_DONE_46),
	REGNAME(mm_hwv, 0x1CBC, HWV_VIP_CG_DONE_47),
	REGNAME(mm_hwv, 0x1CC0, HWV_VIP_CG_DONE_48),
	REGNAME(mm_hwv, 0x1CC4, HWV_VIP_CG_DONE_49),
	REGNAME(mm_hwv, 0x1900, HWV_VIP_CG_ENABLE_0),
	REGNAME(mm_hwv, 0x1904, HWV_VIP_CG_ENABLE_1),
	REGNAME(mm_hwv, 0x1908, HWV_VIP_CG_ENABLE_2),
	REGNAME(mm_hwv, 0x190C, HWV_VIP_CG_ENABLE_3),
	REGNAME(mm_hwv, 0x1910, HWV_VIP_CG_ENABLE_4),
	REGNAME(mm_hwv, 0x1914, HWV_VIP_CG_ENABLE_5),
	REGNAME(mm_hwv, 0x1918, HWV_VIP_CG_ENABLE_6),
	REGNAME(mm_hwv, 0x191C, HWV_VIP_CG_ENABLE_7),
	REGNAME(mm_hwv, 0x1920, HWV_VIP_CG_ENABLE_8),
	REGNAME(mm_hwv, 0x1924, HWV_VIP_CG_ENABLE_9),
	REGNAME(mm_hwv, 0x1928, HWV_VIP_CG_ENABLE_10),
	REGNAME(mm_hwv, 0x192C, HWV_VIP_CG_ENABLE_11),
	REGNAME(mm_hwv, 0x1930, HWV_VIP_CG_ENABLE_12),
	REGNAME(mm_hwv, 0x1934, HWV_VIP_CG_ENABLE_13),
	REGNAME(mm_hwv, 0x1938, HWV_VIP_CG_ENABLE_14),
	REGNAME(mm_hwv, 0x193C, HWV_VIP_CG_ENABLE_15),
	REGNAME(mm_hwv, 0x1940, HWV_VIP_CG_ENABLE_16),
	REGNAME(mm_hwv, 0x1944, HWV_VIP_CG_ENABLE_17),
	REGNAME(mm_hwv, 0x1948, HWV_VIP_CG_ENABLE_18),
	REGNAME(mm_hwv, 0x194C, HWV_VIP_CG_ENABLE_19),
	REGNAME(mm_hwv, 0x1950, HWV_VIP_CG_ENABLE_20),
	REGNAME(mm_hwv, 0x1954, HWV_VIP_CG_ENABLE_21),
	REGNAME(mm_hwv, 0x1958, HWV_VIP_CG_ENABLE_22),
	REGNAME(mm_hwv, 0x195C, HWV_VIP_CG_ENABLE_23),
	REGNAME(mm_hwv, 0x1960, HWV_VIP_CG_ENABLE_24),
	REGNAME(mm_hwv, 0x1964, HWV_VIP_CG_ENABLE_25),
	REGNAME(mm_hwv, 0x1968, HWV_VIP_CG_ENABLE_26),
	REGNAME(mm_hwv, 0x196C, HWV_VIP_CG_ENABLE_27),
	REGNAME(mm_hwv, 0x1970, HWV_VIP_CG_ENABLE_28),
	REGNAME(mm_hwv, 0x1974, HWV_VIP_CG_ENABLE_29),
	REGNAME(mm_hwv, 0x1978, HWV_VIP_CG_ENABLE_30),
	REGNAME(mm_hwv, 0x197C, HWV_VIP_CG_ENABLE_31),
	REGNAME(mm_hwv, 0x1980, HWV_VIP_CG_ENABLE_32),
	REGNAME(mm_hwv, 0x1984, HWV_VIP_CG_ENABLE_33),
	REGNAME(mm_hwv, 0x1988, HWV_VIP_CG_ENABLE_34),
	REGNAME(mm_hwv, 0x198C, HWV_VIP_CG_ENABLE_35),
	REGNAME(mm_hwv, 0x1990, HWV_VIP_CG_ENABLE_36),
	REGNAME(mm_hwv, 0x1994, HWV_VIP_CG_ENABLE_37),
	REGNAME(mm_hwv, 0x1998, HWV_VIP_CG_ENABLE_38),
	REGNAME(mm_hwv, 0x199C, HWV_VIP_CG_ENABLE_39),
	REGNAME(mm_hwv, 0x19A0, HWV_VIP_CG_ENABLE_40),
	REGNAME(mm_hwv, 0x19A4, HWV_VIP_CG_ENABLE_41),
	REGNAME(mm_hwv, 0x19A8, HWV_VIP_CG_ENABLE_42),
	REGNAME(mm_hwv, 0x19AC, HWV_VIP_CG_ENABLE_43),
	REGNAME(mm_hwv, 0x19B0, HWV_VIP_CG_ENABLE_44),
	REGNAME(mm_hwv, 0x19B4, HWV_VIP_CG_ENABLE_45),
	REGNAME(mm_hwv, 0x19B8, HWV_VIP_CG_ENABLE_46),
	REGNAME(mm_hwv, 0x19BC, HWV_VIP_CG_ENABLE_47),
	REGNAME(mm_hwv, 0x19C0, HWV_VIP_CG_ENABLE_48),
	REGNAME(mm_hwv, 0x19C4, HWV_VIP_CG_ENABLE_49),
	REGNAME(mm_hwv, 0x1800, HWV_VIP_CG_STATUS_0),
	REGNAME(mm_hwv, 0x1804, HWV_VIP_CG_STATUS_1),
	REGNAME(mm_hwv, 0x1808, HWV_VIP_CG_STATUS_2),
	REGNAME(mm_hwv, 0x180C, HWV_VIP_CG_STATUS_3),
	REGNAME(mm_hwv, 0x1810, HWV_VIP_CG_STATUS_4),
	REGNAME(mm_hwv, 0x1814, HWV_VIP_CG_STATUS_5),
	REGNAME(mm_hwv, 0x1818, HWV_VIP_CG_STATUS_6),
	REGNAME(mm_hwv, 0x181C, HWV_VIP_CG_STATUS_7),
	REGNAME(mm_hwv, 0x1820, HWV_VIP_CG_STATUS_8),
	REGNAME(mm_hwv, 0x1824, HWV_VIP_CG_STATUS_9),
	REGNAME(mm_hwv, 0x1828, HWV_VIP_CG_STATUS_10),
	REGNAME(mm_hwv, 0x182C, HWV_VIP_CG_STATUS_11),
	REGNAME(mm_hwv, 0x1830, HWV_VIP_CG_STATUS_12),
	REGNAME(mm_hwv, 0x1834, HWV_VIP_CG_STATUS_13),
	REGNAME(mm_hwv, 0x1838, HWV_VIP_CG_STATUS_14),
	REGNAME(mm_hwv, 0x183C, HWV_VIP_CG_STATUS_15),
	REGNAME(mm_hwv, 0x1840, HWV_VIP_CG_STATUS_16),
	REGNAME(mm_hwv, 0x1844, HWV_VIP_CG_STATUS_17),
	REGNAME(mm_hwv, 0x1848, HWV_VIP_CG_STATUS_18),
	REGNAME(mm_hwv, 0x184C, HWV_VIP_CG_STATUS_19),
	REGNAME(mm_hwv, 0x1850, HWV_VIP_CG_STATUS_20),
	REGNAME(mm_hwv, 0x1854, HWV_VIP_CG_STATUS_21),
	REGNAME(mm_hwv, 0x1858, HWV_VIP_CG_STATUS_22),
	REGNAME(mm_hwv, 0x185C, HWV_VIP_CG_STATUS_23),
	REGNAME(mm_hwv, 0x1860, HWV_VIP_CG_STATUS_24),
	REGNAME(mm_hwv, 0x1864, HWV_VIP_CG_STATUS_25),
	REGNAME(mm_hwv, 0x1868, HWV_VIP_CG_STATUS_26),
	REGNAME(mm_hwv, 0x186C, HWV_VIP_CG_STATUS_27),
	REGNAME(mm_hwv, 0x1870, HWV_VIP_CG_STATUS_28),
	REGNAME(mm_hwv, 0x1874, HWV_VIP_CG_STATUS_29),
	REGNAME(mm_hwv, 0x1878, HWV_VIP_CG_STATUS_30),
	REGNAME(mm_hwv, 0x187C, HWV_VIP_CG_STATUS_31),
	REGNAME(mm_hwv, 0x1880, HWV_VIP_CG_STATUS_32),
	REGNAME(mm_hwv, 0x1884, HWV_VIP_CG_STATUS_33),
	REGNAME(mm_hwv, 0x1888, HWV_VIP_CG_STATUS_34),
	REGNAME(mm_hwv, 0x188C, HWV_VIP_CG_STATUS_35),
	REGNAME(mm_hwv, 0x1890, HWV_VIP_CG_STATUS_36),
	REGNAME(mm_hwv, 0x1894, HWV_VIP_CG_STATUS_37),
	REGNAME(mm_hwv, 0x1898, HWV_VIP_CG_STATUS_38),
	REGNAME(mm_hwv, 0x189C, HWV_VIP_CG_STATUS_39),
	REGNAME(mm_hwv, 0x18A0, HWV_VIP_CG_STATUS_40),
	REGNAME(mm_hwv, 0x18A4, HWV_VIP_CG_STATUS_41),
	REGNAME(mm_hwv, 0x18A8, HWV_VIP_CG_STATUS_42),
	REGNAME(mm_hwv, 0x18AC, HWV_VIP_CG_STATUS_43),
	REGNAME(mm_hwv, 0x18B0, HWV_VIP_CG_STATUS_44),
	REGNAME(mm_hwv, 0x18B4, HWV_VIP_CG_STATUS_45),
	REGNAME(mm_hwv, 0x18B8, HWV_VIP_CG_STATUS_46),
	REGNAME(mm_hwv, 0x18BC, HWV_VIP_CG_STATUS_47),
	REGNAME(mm_hwv, 0x18C0, HWV_VIP_CG_STATUS_48),
	REGNAME(mm_hwv, 0x18C4, HWV_VIP_CG_STATUS_49),
	REGNAME(mm_hwv, 0x0B98, HW_CCF_APU_MTCMOS_SET),
	REGNAME(mm_hwv, 0x0B90, HW_CCF_APU_PLL_SET),
	REGNAME(mm_hwv, 0x0198, HW_CCF_AP_MTCMOS_SET),
	REGNAME(mm_hwv, 0x0190, HW_CCF_AP_PLL_SET),
	REGNAME(mm_hwv, 0x1198, HW_CCF_GCE_MTCMOS_SET),
	REGNAME(mm_hwv, 0x1190, HW_CCF_GCE_PLL_SET),
	REGNAME(mm_hwv, 0x0798, HW_CCF_GPU_MTCMOS_SET),
	REGNAME(mm_hwv, 0x0790, HW_CCF_GPU_PLL_SET),
	REGNAME(mm_hwv, 0x1500, HW_CCF_INT_STATUS),
	REGNAME(mm_hwv, 0x0598, HW_CCF_MD_MTCMOS_SET),
	REGNAME(mm_hwv, 0x0590, HW_CCF_MD_PLL_SET),
	REGNAME(mm_hwv, 0x0F98, HW_CCF_MMUP_MTCMOS_SET),
	REGNAME(mm_hwv, 0x0F90, HW_CCF_MMUP_PLL_SET),
	REGNAME(mm_hwv, 0x1470, HW_CCF_MTCMOS_CLR_STATUS),
	REGNAME(mm_hwv, 0x141C, HW_CCF_MTCMOS_DONE),
	REGNAME(mm_hwv, 0x1410, HW_CCF_MTCMOS_ENABLE),
	REGNAME(mm_hwv, 0x14AC, HW_CCF_MTCMOS_FLOW_FLAG_CLR),
	REGNAME(mm_hwv, 0x14A8, HW_CCF_MTCMOS_FLOW_FLAG_SET),
	REGNAME(mm_hwv, 0x146C, HW_CCF_MTCMOS_SET_STATUS),
	REGNAME(mm_hwv, 0x1414, HW_CCF_MTCMOS_STATUS),
	REGNAME(mm_hwv, 0x1454, HW_CCF_MTCMOS_STATUS_CLR),
	REGNAME(mm_hwv, 0x1468, HW_CCF_PLL_CLR_STATUS),
	REGNAME(mm_hwv, 0x140C, HW_CCF_PLL_DONE),
	REGNAME(mm_hwv, 0x1400, HW_CCF_PLL_ENABLE),
	REGNAME(mm_hwv, 0x1464, HW_CCF_PLL_SET_STATUS),
	REGNAME(mm_hwv, 0x1404, HW_CCF_PLL_STATUS),
	REGNAME(mm_hwv, 0x1450, HW_CCF_PLL_STATUS_CLR),
	REGNAME(mm_hwv, 0x1398, HW_CCF_SCP_MTCMOS_SET),
	REGNAME(mm_hwv, 0x1390, HW_CCF_SCP_PLL_SET),
	REGNAME(mm_hwv, 0x0D98, HW_CCF_SPM_MTCMOS_SET),
	REGNAME(mm_hwv, 0x0D90, HW_CCF_SPM_PLL_SET),
	REGNAME(mm_hwv, 0x0998, HW_CCF_SSPM_MTCMOS_SET),
	REGNAME(mm_hwv, 0x0990, HW_CCF_SSPM_PLL_SET),
	REGNAME(mm_hwv, 0x0398, HW_CCF_TEE_MTCMOS_SET),
	REGNAME(mm_hwv, 0x0390, HW_CCF_TEE_PLL_SET),
	{},
};

static const struct regname *get_all_mt6899_regnames(void)
{
	return rn;
}

static void init_regbase(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rb) - 1; i++) {
		if (!rb[i].phys)
			continue;

		if (i == hwv || i == mm_hwv)
			rb[i].virt = ioremap(rb[i].phys, 0x2000);
		else
			rb[i].virt = ioremap(rb[i].phys, 0x1000);
	}
}

u32 get_mt6899_reg_value(u32 id, u32 ofs)
{
	if (id >= chk_sys_num)
		return 0;

	return clk_readl(rb[id].virt + ofs);
}
EXPORT_SYMBOL_GPL(get_mt6899_reg_value);

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
	{"audiosys", afe, spm, 0x0E14},
	{"camsys_ipe", camsys_ipe, spm, 0x0E68},
	{"camsys_mraw", cam_mr, spm, 0x0E68},
	{"camsys_rawa", cam_ra, spm, 0x0E68},
	{"camsys_rawb", cam_rb, spm, 0x0E68},
	{"camsys_rawc", cam_rc, spm, 0x0E68},
	{"camsys_rmsa", camsys_rmsa, spm, 0x0E68},
	{"camsys_rmsb", camsys_rmsb, spm, 0x0E68},
	{"camsys_rmsc", camsys_rmsc, spm, 0x0E68},
	{"camsys_yuva", cam_ya, spm, 0x0E68},
	{"camsys_yuvb", cam_yb, spm, 0x0E68},
	{"camsys_yuvc", cam_yc, spm, 0x0E68},
	{"cam_main_r1a", cam_m, spm, 0x0E6C},
	{"cam_vcore_r1a", camv, spm, 0x0E68},
	{"ccu", ccu, spm, 0x0E6C},
	{"dip_nr1_dip1", dip_nr1_dip1, spm, 0x0E3C},
	{"dip_nr2_dip1", dip_nr2_dip1, spm, 0x0E3C},
	{"dip_top_dip1", dip_top_dip1, spm, 0x0E3C},
	{"mmsys1", mm1, spm, 0x0E78},
	{"mmsys0", mm, spm, 0x0E88},
	{"imgsys_main", img, spm, 0x0E40},
	{"img_vcore_d1a", img_v, spm, 0x0E40},
	{"mdpsys1", mdp1, spm, 0x0E80},
	{"mdpsys", mdp, spm, 0x0E7C},
	{"mminfra_ao_config", mminfra_ao_config, spm, 0x0E90},
	{"ovlsys_config", ovl, spm, 0x0E8C},
	{"traw_cap_dip1", traw_cap_dip1, spm, 0x0E38},
	{"traw_dip1", traw_dip1, spm, 0x0E38},
	{"vdecsys", vde2, spm, 0x0E48},
	{"vdecsys_soc", vde1, spm, 0x0E4C},
	{"vencsys", ven1, spm, 0x0E50},
	{"vencsys_c1", ven2, spm, 0x0E54},
	{"wpe1_dip1", wpe1_dip1, spm, 0x0E3C},
	{"wpe2_dip1", wpe2_dip1, spm, 0x0E3C},
	{"wpe3_dip1", wpe3_dip1, spm, 0x0E3C},
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
	int freq_table[7];
};

#define MTK_VF_TABLE(_n, _freq0, _freq1, _freq2, _freq3, _freq4, _freq5, _freq6) {		\
		.name = _n,		\
		.freq_table = {_freq0, _freq1, _freq2, _freq3, _freq4, _freq5, _freq6},	\
	}

/*
 * Opp0 : 0p875v
 * Opp1 : 0p825v
 * Opp2 : 0p725v
 * Opp3 : 0p65v
 * Opp4 : 0p60v
 * Opp5 : 0p575v
 * Opp6 : 0p55v
 */
static struct mtk_vf vf_table[] = {
	/* Opp0, Opp1, Opp2, Opp3, Opp4, Opp5, Opp6 */
	MTK_VF_TABLE("axi_sel", 156000, 156000, 156000, 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("peri_faxi_sel", 156000, 156000, 156000, 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("ufs_faxi_sel", 156000, 156000, 156000, 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("pextp_faxi_sel", 156000, 156000, 156000, 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("bus_aximem_sel", 364000, 364000, 364000, 273000, 218400, 218400, 218400),
	MTK_VF_TABLE("mem_sub_sel", 728000, 728000, 546000, 436800, 364000, 218400, 218400),
	MTK_VF_TABLE("peri_fmem_sub_sel", 546000, 546000, 546000, 436800, 364000, 218400, 218400),
	MTK_VF_TABLE("ufs_fmem_sub_sel", 546000, 546000, 546000, 436800, 364000, 218400, 218400),
	MTK_VF_TABLE("pextp_fmem_sub_sel", 546000, 546000, 546000, 436800, 364000, 218400, 218400),
	MTK_VF_TABLE("emi_n_sel", 800000, 800000, 800000, 800000, 800000, 800000, 800000),
	MTK_VF_TABLE("emi_s_sel", 800000, 800000, 800000, 800000, 800000, 800000, 800000),
	MTK_VF_TABLE("emi_slice_n_sel", 1200000, 1200000, 1200000, 1200000, 1200000, 1200000, 1200000),
	MTK_VF_TABLE("emi_slice_s_sel", 1200000, 1200000, 1200000, 1200000, 1200000, 1200000, 1200000),
	MTK_VF_TABLE("ap2conn_host_sel", 78000, 78000, 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("atb_sel", 364000, 364000, 364000, 364000, 273000, 273000, 273000),
	MTK_VF_TABLE("cirq_sel", 78000, 78000, 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("efuse_sel", 26000, 26000, 26000, 26000, 26000, 26000, 26000),
	MTK_VF_TABLE("mcu_l3gic_sel", 156000, 156000, 156000, 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("mcu_infra_sel", 364000, 364000, 364000, 273000, 218400, 218400, 218400),
	MTK_VF_TABLE("mcu_acp_sel", 728000, 728000, 728000, 436800, 364000, 218400, 156000),
	MTK_VF_TABLE("tl_sel", 136500, 136500, 136500, 136500, 136500, 136500, 136500),
	MTK_VF_TABLE("md_emi_sel", 546000, 546000, 546000, 546000, 546000, 546000, 546000),
	MTK_VF_TABLE("dsp_sel", 260000, 260000, 260000, 260000, 260000, 260000, 260000),
	MTK_VF_TABLE("mfg_ref_sel", 156000, 156000, 156000, 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("mfgsc_ref_sel", 156000, 156000, 156000, 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("mfg_eb_sel", 218400, 218400, 218400, 218400, 218400, 218400, 218400),
	MTK_VF_TABLE("uart_sel", 208000, 208000, 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("spi0_b_sel", 208000, 208000, 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("spi1_b_sel", 208000, 208000, 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("spi2_b_sel", 208000, 208000, 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("spi3_b_sel", 208000, 208000, 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("spi4_b_sel", 208000, 208000, 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("spi5_b_sel", 208000, 208000, 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("spi6_b_sel", 208000, 208000, 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("spi7_b_sel", 208000, 208000, 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("msdc30_1_sel", 192000, 192000, 192000, 192000, 192000, 192000, 192000),
	MTK_VF_TABLE("msdc30_2_sel", 192000, 192000, 192000, 192000, 192000, 192000, 192000),
	MTK_VF_TABLE("aud_intbus_sel", 136500, 136500, 136500, 136500, 136500, 136500, 136500),
	MTK_VF_TABLE("disp_pwm_sel", 130000, 130000, 130000, 130000, 130000, 130000, 130000),
	MTK_VF_TABLE("usb_sel", 124800, 124800, 124800, 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("ssusb_xhci_sel", 124800, 124800, 124800, 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("msdc30_1_h_sel", 192000, 192000, 192000, 192000, 192000, 192000, 192000),
	MTK_VF_TABLE("i2c_sel", 136500, 136500, 136500, 136500, 136500, 136500, 136500),
	MTK_VF_TABLE("aud_engen1_sel", 45200, 45200, 45200, 45200, 45200, 45200, 45200),
	MTK_VF_TABLE("aud_engen2_sel", 49200, 49200, 49200, 49200, 49200, 49200, 49200),
	MTK_VF_TABLE("aes_ufsfde_sel", 546000, 546000, 546000, 546000, 416000, 416000, 416000),
	MTK_VF_TABLE("ufs_sel", 499200, 499200, 499200, 499200, 273000, 273000, 273000),
	MTK_VF_TABLE("ufs_mbist_sel", 594000, 594000, 594000, 594000, 297000, 297000, 297000),
	MTK_VF_TABLE("pextp_mbist_sel", 249600, 249600, 249600, 249600, 249600, 249600, 249600),
	MTK_VF_TABLE("aud_1_sel", 180600, 180600, 180600, 180600, 180600, 180600, 180600),
	MTK_VF_TABLE("aud_2_sel", 196600, 196600, 196600, 196600, 196600, 196600, 196600),
	MTK_VF_TABLE("audio_h_sel", 196600, 196600, 196600, 196600, 196600, 196600, 196600),
	MTK_VF_TABLE("adsp_sel", 800000, 800000, 800000, 800000, 800000, 800000, 800000),
	MTK_VF_TABLE("adps_uarthub_b_sel", 208000, 208000, 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("dpmaif_main_sel", 546000, 546000, 436800, 436800, 364000, 273000, 273000),
	MTK_VF_TABLE("pwm_sel", 78000, 78000, 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("mcupm_sel", 218400, 218400, 218400, 218400, 218400, 218400, 218400),
	MTK_VF_TABLE("dpsw_cmp_26m_sel", 26000, 26000, 26000, 26000, 26000, 26000, 26000),
	MTK_VF_TABLE("msdc30_2_h_sel", 192000, 192000, 192000, 192000, 192000, 192000, 192000),
	MTK_VF_TABLE("seninf0_sel", 499200, 499200, 499200, 416000, 364000, 312000, 312000),
	MTK_VF_TABLE("seninf1_sel", 499200, 499200, 499200, 416000, 364000, 312000, 312000),
	MTK_VF_TABLE("seninf2_sel", 499200, 499200, 499200, 416000, 364000, 312000, 312000),
	MTK_VF_TABLE("seninf3_sel", 499200, 499200, 499200, 416000, 364000, 312000, 312000),
	MTK_VF_TABLE("seninf4_sel", 499200, 499200, 499200, 416000, 364000, 312000, 312000),
	MTK_VF_TABLE("seninf5_sel", 499200, 499200, 499200, 416000, 364000, 312000, 312000),
	MTK_VF_TABLE("ccu_ahb_sel", 275000, 275000, 275000, 275000, 275000, 275000, 275000),
	MTK_VF_TABLE("vdec_sel", 880000, 880000, 624000, 436800, 273000, 218400, 218400),
	MTK_VF_TABLE("ccusys_sel", 832000, 832000, 832000, 832000, 832000, 832000, 832000),
	MTK_VF_TABLE("ccutm_sel", 208000, 208000, 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("venc_sel", 880000, 624000, 458333, 312000, 249600, 249600),
	MTK_VF_TABLE("dp_core_sel", 148500, 148500, 148500, 148500, 148500, 148500, 148500),
	MTK_VF_TABLE("mminfra_sel", 728000, 728000, 624000, 458333, 364000, 218400, 218400),
	MTK_VF_TABLE("mmup_sel", 728000, 728000, 728000, 728000, 728000, 728000, 728000),
	MTK_VF_TABLE("img_26m_sel", 26000, 26000, 26000, 26000, 26000, 26000, 26000),
	MTK_VF_TABLE("cam_26m_sel", 26000, 26000, 26000, 26000, 26000, 26000, 26000),
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

void set_subsys_reg_dump_mt6899(enum chk_sys_id id[])
{
	const struct regname *rns = &rn[0];
	int i, j, k;

	for (i = 0; i < ARRAY_SIZE(rn) - 1; i++, rns++) {
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

		reg_dump_addr[i] = PHYSADDR(rns);
		reg_dump_val[i] = clk_readl(ADDR(rns));
		/* record each register dump index validation */
		reg_dump_valid[i] = true;
	}
}
EXPORT_SYMBOL_GPL(set_subsys_reg_dump_mt6899);

void get_subsys_reg_dump_mt6899(void)
{
	const struct regname *rns = &rn[0];
	int i;

	for (i = 0; i < ARRAY_SIZE(rn) - 1; i++, rns++) {
		if (reg_dump_valid[i])
			pr_info("%-18s: [0x%08x] = 0x%08x\n",
					rns->name, reg_dump_addr[i], reg_dump_val[i]);
	}
}
EXPORT_SYMBOL_GPL(get_subsys_reg_dump_mt6899);

void print_subsys_reg_mt6899(enum chk_sys_id id)
{
	struct regbase *rb_dump;
	const struct regname *rns = &rn[0];
	int pwr_idx = PD_NULL;
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

		if (pwr_idx != PD_NULL) {
			if (!pwr_hw_is_on(PWR_CON_STA, pwr_idx))
				return;
		}

		pr_info("%-18s: [0x%08x] = 0x%08x\n",
			rns->name, PHYSADDR(rns), clk_readl(ADDR(rns)));
	}
}
EXPORT_SYMBOL_GPL(print_subsys_reg_mt6899);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
static enum chk_sys_id devapc_dump_id[] = {
	spm,
	hfrp,
	emi_nemicfg_ao_mem_prot_reg_bus,
	emi_semicfg_ao_mem_prot_reg_bus,
	infra_ifrbus_ao_reg_bus,
	top,
	apmixed,
	vlp_ck,
	hwv,
	chk_sys_num,
};

static void devapc_dump(void)
{
	const struct fmeter_clk *fclks;

	fclks = mt_get_fmeter_clks();
	set_subsys_reg_dump_mt6899(devapc_dump_id);
	get_subsys_reg_dump_mt6899();

	dump_clk_event();
	pdchk_dump_trace_evt();

	for (; fclks != NULL && fclks->type != FT_NULL; fclks++) {
		if (fclks->type != VLPCK && fclks->type != SUBSYS)
			pr_notice("[%s] %d khz\n", fclks->name,
				mt_get_fmeter_freq(fclks->id, fclks->type));
	}
}

static struct devapc_vio_callbacks devapc_vio_handle = {
	.id = DEVAPC_SUBSYS_CLKMGR,
	.debug_dump = devapc_dump,
};
#endif


#ifdef CONFIG_MTK_SERROR_HOOK
static void serror_dump(void)
{
	const struct fmeter_clk *fclks;

	fclks = mt_get_fmeter_clks();

	set_subsys_reg_dump_mt6899(devapc_dump_id);
	get_subsys_reg_dump_mt6899();

	dump_clk_event();
	pdchk_dump_trace_evt();

	for (; fclks != NULL && fclks->type != FT_NULL; fclks++) {
		if (fclks->type != VLPCK && fclks->type != SUBSYS)
			pr_notice("[%s] %d khz\n", fclks->name,
				mt_get_fmeter_freq(fclks->id, fclks->type));
	}
}

static void clkchk_arm64_serror_panic_hook(void *data,
		struct pt_regs *regs, unsigned long esr)
{
	serror_dump();
}
#endif

static const char * const off_pll_names[] = {
	NULL
};

static const char * const notice_pll_names[] = {
	"mainpll",
	"mmpll",
	"emipll",
	"msdcpll",
	"emipll2",
	"imgpll",
	"tvdpll",
	"adsppll",
	"mfgpll",
	"mfgscpll",
	"adsppll",
	"apll1",
	"apll2",
	NULL
};

static const char * const bypass_pll_name[] = {
	"univpll",
	NULL
};

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
#if (BUG_ON_CHK_ENABLE) || (IS_ENABLED(CONFIG_MTK_CLKMGR_DEBUG))
	return true;
#endif
	return false;
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

static enum chk_sys_id history_dump_id[] = {
	top,
	apmixed,
	hwv,
	chk_sys_num,
};

static void dump_hwv_history(struct regmap *regmap, u32 id)
{
	u32 set[XPU_NUM] = {0}, sta = 0, en = 0, done = 0;
	int i;

	set_subsys_reg_dump_mt6899(history_dump_id);

	if (regmap != NULL) {
		for (i = 0; i < XPU_NUM; i++)
			regmap_read(regmap, HWV_CG_SET(xpu_id[i], id), &set[i]);

		regmap_read(regmap, HWV_CG_STA(id), &sta);
		regmap_read(regmap, HWV_CG_EN(id), &en);
		regmap_read(regmap, HWV_CG_DONE(id), &done);


		for (i = 0; i < XPU_NUM; i++)
			pr_notice("set: (%x)%x", HWV_CG_SET(xpu_id[i], id), set[i]);
		pr_notice("[%d] (%x)%x, (%x)%x, (%x)%x\n",
				id,
				HWV_CG_STA(id), sta,
				HWV_CG_EN(id), en,
				HWV_CG_DONE(id), done);
	}

	get_subsys_reg_dump_mt6899();
}

static enum chk_sys_id bus_dump_id[] = {
	top,
	apmixed,
	chk_sys_num,
};

static void get_bus_reg(void)
{
	set_subsys_reg_dump_mt6899(bus_dump_id);
}

static void dump_bus_reg(struct regmap *regmap, u32 ofs)
{
	const struct fmeter_clk *fclks;

	fclks = mt_get_fmeter_clks();
	set_subsys_reg_dump_mt6899(bus_dump_id);
	get_subsys_reg_dump_mt6899();
	for (; fclks != NULL && fclks->type != FT_NULL; fclks++) {
		if (fclks->type != VLPCK && fclks->type != SUBSYS)
			pr_notice("[%s] %d khz\n", fclks->name,
				mt_get_fmeter_freq(fclks->id, fclks->type));
	}

	/* sspm need some time to run isr */
	mdelay(1000);
	BUG_ON(1);
}

static enum chk_sys_id pll_dump_id[] = {
	apmixed,
	top,
	hwv,
	chk_sys_num,
};

static void dump_pll_reg(bool bug_on)
{
	set_subsys_reg_dump_mt6899(pll_dump_id);
	get_subsys_reg_dump_mt6899();

	if (bug_on) {
		mdelay(100);
		BUG_ON(1);
	}
}

static void check_hwv_irq_sta(void)
{
	u32 irq_sta;

	irq_sta = get_mt6899_reg_value(hwv, HWV_IRQ_STATUS);

	if ((irq_sta & HWV_INT_CG_TRIGGER) == HWV_INT_CG_TRIGGER) {
		dump_hwv_history(NULL, 0);
		dump_bus_reg(NULL, 0);
	}
	if ((irq_sta & HWV_INT_PLL_TRIGGER) == HWV_INT_PLL_TRIGGER)
		dump_pll_reg(true);
}

/*
 * init functions
 */

static struct clkchk_ops clkchk_mt6899_ops = {
	.get_all_regnames = get_all_mt6899_regnames,
	.get_pvd_pwr_data_idx = get_pvd_pwr_data_idx,
	.get_pwr_status = get_pwr_status,
	.is_cg_chk_pwr_on = is_cg_chk_pwr_on,
	.get_off_pll_names = get_off_pll_names,
	.get_notice_pll_names = get_notice_pll_names,
	.get_bypass_pll_name = get_bypass_pll_name,
	.is_pll_chk_bug_on = is_pll_chk_bug_on,
	.get_vf_name = get_vf_name,
	.get_vf_opp = get_vf_opp,
	.get_vf_num = get_vf_num,
	.get_vcore_opp = get_vcore_opp,
	.devapc_dump = devapc_dump,
	.dump_hwv_history = dump_hwv_history,
	.get_bus_reg = get_bus_reg,
	.dump_bus_reg = dump_bus_reg,
	.dump_pll_reg = dump_pll_reg,
	.trace_clk_event = trace_clk_event,
	.check_hwv_irq_sta = check_hwv_irq_sta,
	.is_suspend_retry_stop = is_suspend_retry_stop,
};

static int clk_chk_mt6899_probe(struct platform_device *pdev)
{
	suspend_cnt = 0;

	init_regbase();

	set_clkchk_notify();

	set_clkchk_ops(&clkchk_mt6899_ops);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
	register_devapc_vio_callback(&devapc_vio_handle);
#endif

#ifdef CONFIG_MTK_SERROR_HOOK
	int ret = 0;

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

static const struct of_device_id of_match_clkchk_mt6899[] = {
	{
		.compatible = "mediatek,mt6899-clkchk",
	}, {
		/* sentinel */
	}
};

static struct platform_driver clk_chk_mt6899_drv = {
	.probe = clk_chk_mt6899_probe,
	.driver = {
		.name = "clk-chk-mt6899",
		.owner = THIS_MODULE,
		.pm = &clk_chk_dev_pm_ops,
		.of_match_table = of_match_clkchk_mt6899,
	},
};

/*
 * init functions
 */

static int __init clkchk_mt6899_init(void)
{
	return platform_driver_register(&clk_chk_mt6899_drv);
}

static void __exit clkchk_mt6899_exit(void)
{
	platform_driver_unregister(&clk_chk_mt6899_drv);
}

subsys_initcall(clkchk_mt6899_init);
module_exit(clkchk_mt6899_exit);
MODULE_LICENSE("GPL");
