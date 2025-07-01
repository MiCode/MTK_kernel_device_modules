// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: KY Liu <ky.liu@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>

#include <dt-bindings/power/mt6858-power.h>

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
#include "clkchk-mt6858.h"
#include "clk-fmeter.h"
#include "clk-mt6858-fmeter.h"

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
	CLK_AFE_DL0_DAC_TML_CG = 0,
	CLK_AFE_DL0_DAC_HIRES_CG = 1,
	CLK_AFE_DL0_DAC_CG = 2,
	CLK_AFE_DL0_PREDIS_CG = 3,
	CLK_AFE_DL0_NLE_CG = 4,
	CLK_AFE_PCM1_CG = 5,
	CLK_AFE_PCM0_CG = 6,
	CLK_AFE_CM1_CG = 7,
	CLK_AFE_CM0_CG = 8,
	CLK_AFE_STF_CG = 9,
	CLK_AFE_HW_GAIN23_CG = 10,
	CLK_AFE_HW_GAIN01_CG = 11,
	CLK_AFE_AUDIO_HOPPING_CG = 12,
	CLK_AFE_AUDIO_F26M_CG = 13,
	CLK_AFE_APLL1_CG = 14,
	CLK_AFE_APLL2_CG = 15,
	CLK_AFE_H208M_CG = 16,
	CLK_AFE_APLL_TUNER2_CG = 17,
	CLK_AFE_APLL_TUNER1_CG = 18,
	CLK_AFE_AUD_PAD_TOP_MOSI_EN_CG = 19,
	CLK_AFE_UL1_ADC_HIRES_TML_CG = 20,
	CLK_AFE_UL1_ADC_HIRES_CG = 21,
	CLK_AFE_UL1_TML_CG = 22,
	CLK_AFE_UL1_ADC_CG = 23,
	CLK_AFE_UL0_ADC_HIRES_TML_CG = 24,
	CLK_AFE_UL0_ADC_HIRES_CG = 25,
	CLK_AFE_UL0_TML_CG = 26,
	CLK_AFE_UL0_ADC_CG = 27,
	CLK_AFE_ETDM_IN4_CG = 28,
	CLK_AFE_ETDM_IN2_CG = 29,
	CLK_AFE_ETDM_IN1_CG = 30,
	CLK_AFE_ETDM_OUT4_CG = 31,
	CLK_AFE_ETDM_OUT2_CG = 32,
	CLK_AFE_ETDM_OUT1_CG = 33,
	CLK_AFE_GENERAL3_ASRC_CG = 34,
	CLK_AFE_GENERAL2_ASRC_CG = 35,
	CLK_AFE_GENERAL1_ASRC_CG = 36,
	CLK_AFE_GENERAL0_ASRC_CG = 37,
	CLK_AFE_CONNSYS_I2S_ASRC_CG = 38,
	CLK_CAM_M_LARB13_CG = 39,
	CLK_CAM_M_LARB14_CG = 40,
	CLK_CAM_M_CAM_CG = 41,
	CLK_CAM_M_CAMTG_CG = 42,
	CLK_CAM_M_SENINF_CG = 43,
	CLK_CAM_M_CAMSV1_CG = 44,
	CLK_CAM_M_CAMSV2_CG = 45,
	CLK_CAM_M_CAMSV3_CG = 46,
	CLK_CAM_M_CAM2MM_GALS_CG = 47,
	CLK_CAM_M_PDA_CG = 48,
	CLK_CAM_RA_LARBX_CG = 49,
	CLK_CAM_RA_CAM_CG = 50,
	CLK_CAM_RA_CAMTG_CG = 51,
	CLK_CAM_RB_LARBX_CG = 52,
	CLK_CAM_RB_CAM_CG = 53,
	CLK_CAM_RB_CAMTG_CG = 54,
	CLK_MM_DISP_OVL0_2L_CG = 55,
	CLK_MM_DISP_OVL1_2L_CG = 56,
	CLK_MM_DISP_OVL2_2L_CG = 57,
	CLK_MM_DISP_RSZ1_CG = 58,
	CLK_MM_DISP_RSZ0_CG = 59,
	CLK_MM_DISP_TDSHP0_CG = 60,
	CLK_MM_DISP_C3D0_CG = 61,
	CLK_MM_DISP_COLOR0_CG = 62,
	CLK_MM_DISP_CCORR0_CG = 63,
	CLK_MM_DISP_CCORR1_CG = 64,
	CLK_MM_DISP_AAL0_CG = 65,
	CLK_MM_DISP_GAMMA0_CG = 66,
	CLK_MM_DISP_POSTMASK0_CG = 67,
	CLK_MM_DISP_DITHER0_CG = 68,
	CLK_MM_DISP_TDSHP1_CG = 69,
	CLK_MM_DISP_C3D1_CG = 70,
	CLK_MM_DISP_CCORR2_CG = 71,
	CLK_MM_DISP_CCORR3_CG = 72,
	CLK_MM_DISP_GAMMA1_CG = 73,
	CLK_MM_DISP_DITHER1_CG = 74,
	CLK_MM_DISP_DSC_WRAP0_CG = 75,
	CLK_MM_DISP_DSI0_CG = 76,
	CLK_MM_DISP_WDMA1_CG = 77,
	CLK_MM_DISP_APB_BUS_CG = 78,
	CLK_MM_DISP_FAKE_ENG0_CG = 79,
	CLK_MM_DISP_FAKE_ENG1_CG = 80,
	CLK_MM_DISP_MUTEX0_CG = 81,
	CLK_MM_SMI_COMMON_CG = 82,
	CLK_MM_DSI0_CG = 83,
	CLK_MM_26M_CG = 84,
	CLK_IMGSYS1_LARB9_CG = 85,
	CLK_IMGSYS1_LARB10_CG = 86,
	CLK_IMGSYS1_DIP_CG = 87,
	CLK_IMGSYS2_LARB9_CG = 88,
	CLK_IMGSYS2_MFB_CG = 89,
	CLK_IMGSYS2_WPE_CG = 90,
	CLK_IMGSYS2_MSS_CG = 91,
	CLK_IMPC_I2C1_CG = 92,
	CLK_IMPC_I2C3_CG = 93,
	CLK_IMPC_I2C5_CG = 94,
	CLK_IMPC_I2C6_CG = 95,
	CLK_IMPC_I2C10_CG = 96,
	CLK_IMPC_I2C12_CG = 97,
	CLK_IMPES_I2C4_CG = 98,
	CLK_IMPES_I2C7_CG = 99,
	CLK_IMPES_I2C8_CG = 100,
	CLK_IMPS_I2C0_CG = 101,
	CLK_IMPS_I2C2_CG = 102,
	CLK_IMPS_I2C9_CG = 103,
	CLK_IMPS_I2C11_CG = 104,
	CLK_INFRACFG_AO_REG_CCIF1_AP_CG = 105,
	CLK_INFRACFG_AO_REG_CCIF1_MD_CG = 106,
	CLK_INFRACFG_AO_REG_CCIF_AP_CG = 107,
	CLK_INFRACFG_AO_REG_CCIF_MD_CG = 108,
	CLK_INFRACFG_AO_REG_CLDMA_BCLK_CG = 109,
	CLK_INFRACFG_AO_REG_CCIF5_MD_CG = 110,
	CLK_INFRACFG_AO_REG_CCIF2_AP_CG = 111,
	CLK_INFRACFG_AO_REG_CCIF2_MD_CG = 112,
	CLK_INFRACFG_AO_REG_DPMAIF_MAIN_CG = 113,
	CLK_INFRACFG_AO_REG_CCIF4_MD_CG = 114,
	CLK_INFRACFG_AO_REG_RG_MMW_DPMAIF26M_CG = 115,
	CLK_IPE_LARB20_CG = 116,
	CLK_IPE_SMI_SUBCOM_CG = 117,
	CLK_IPE_FD_CG = 118,
	CLK_IPE_RSC_CG = 119,
	CLK_MDP_MUTEX0_CG = 120,
	CLK_MDP_APB_BUS_CG = 121,
	CLK_MDP_SMI0_CG = 122,
	CLK_MDP_RDMA0_CG = 123,
	CLK_MDP_FG0_CG = 124,
	CLK_MDP_HDR0_CG = 125,
	CLK_MDP_AAL0_CG = 126,
	CLK_MDP_RSZ0_CG = 127,
	CLK_MDP_TDSHP0_CG = 128,
	CLK_MDP_COLOR0_CG = 129,
	CLK_MDP_WROT0_CG = 130,
	CLK_MDP_FAKE_ENG0_CG = 131,
	CLK_MDP_DLI_ASYNC0_CG = 132,
	CLK_MDP_RSZ2_CG = 133,
	CLK_MDP_WROT2_CG = 134,
	CLK_MDP_DLO_ASYNC0_CG = 135,
	CLK_MDP_FMM_IMG_DL_ASYNC0_CG = 136,
	CLK_MDP_FMM_IMG_DL_ASYNC1_CG = 137,
	CLK_MDP_FIMG_IMG_DL_ASYNC0_CG = 138,
	CLK_MDP_FIMG_IMG_DL_ASYNC1_CG = 139,
	CLK_MMINFRA_GCE_D_CG = 140,
	CLK_MMINFRA_GCE_M_CG = 141,
	CLK_MMINFRA_GCE_26M_CG = 142,
	CLK_PERAO_P_UART0_CG = 143,
	CLK_PERAO_P_UART1_CG = 144,
	CLK_PERAO_P_UART2_CG = 145,
	CLK_PERAO_P_UART3_CG = 146,
	CLK_PERAO_P_PWM_H_CG = 147,
	CLK_PERAO_P_PWM_B_CG = 148,
	CLK_PERAO_P_PWM_FB1_CG = 149,
	CLK_PERAO_P_PWM_FB2_CG = 150,
	CLK_PERAO_P_PWM_FB3_CG = 151,
	CLK_PERAO_P_PWM_FB4_CG = 152,
	CLK_PERAO_P_DISP_PWM0_CG = 153,
	CLK_PERAO_P_DISP_PWM1_CG = 154,
	CLK_PERAO_P_SPI0_B_CG = 155,
	CLK_PERAO_P_SPI1_B_CG = 156,
	CLK_PERAO_P_SPI2_B_CG = 157,
	CLK_PERAO_P_SPI3_B_CG = 158,
	CLK_PERAO_P_SPI4_B_CG = 159,
	CLK_PERAO_P_SPI5_B_CG = 160,
	CLK_PERAO_P_SPI6_B_CG = 161,
	CLK_PERAO_P_SPI7_B_CG = 162,
	CLK_PERAO_P_DMA_B_CG = 163,
	CLK_PERAO_P_MSDC1_CG = 164,
	CLK_PERAO_P_MSDC1_H_CG = 165,
	CLK_PERAO_P_MSDC1_MST_F_CG = 166,
	CLK_PERAO_P_MSDC1_SLV_H_CG = 167,
	CLK_PERAO_P_AUDIO0_CG = 168,
	CLK_PERAO_P_AUDIO1_CG = 169,
	CLK_PERAO_P_AUDIO2_CG = 170,
	CLK_UFSAO_UNIPRO_TX_SYM_CG = 171,
	CLK_UFSAO_UNIPRO_SYS_CG = 172,
	CLK_UFSAO_UNIPRO_SAP_CFG_CG = 173,
	CLK_UFSPDN_UFSHCI_UFS_CG = 174,
	CLK_UFSPDN_UFSHCI_AES_CG = 175,
	CLK_UFSPDN_UFSHCI_UFS_AHB_CG = 176,
	CLK_UFSPDN_UFS_26M_CG = 177,
	CLK_VDE2_VDEC_CKEN_CG = 178,
	CLK_VDE2_VDEC_ACTIVE_CG = 179,
	CLK_VDE2_VDEC_CKEN_ENG_CG = 180,
	CLK_VDE2_LAT_CKEN_CG = 181,
	CLK_VDE2_LAT_ACTIVE_CG = 182,
	CLK_VDE2_LAT_CKEN_ENG_CG = 183,
	CLK_VEN1_CKE0_LARB_CG = 184,
	CLK_VEN1_CKE1_VENC_CG = 185,
	CLK_VEN1_CKE2_JPGENC_CG = 186,
	CLK_VEN1_CKE5_GALS_CG = 187,
	TRACE_CLK_NUM = 188,
};

const char *trace_subsys_cgs[] = {
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
	[CLK_AFE_AUDIO_HOPPING_CG] = "afe_audio_hopping_ck",
	[CLK_AFE_AUDIO_F26M_CG] = "afe_audio_f26m_ck",
	[CLK_AFE_APLL1_CG] = "afe_apll1_ck",
	[CLK_AFE_APLL2_CG] = "afe_apll2_ck",
	[CLK_AFE_H208M_CG] = "afe_h208m_ck",
	[CLK_AFE_APLL_TUNER2_CG] = "afe_apll_tuner2",
	[CLK_AFE_APLL_TUNER1_CG] = "afe_apll_tuner1",
	[CLK_AFE_AUD_PAD_TOP_MOSI_EN_CG] = "afe_aud_pad_mosi",
	[CLK_AFE_UL1_ADC_HIRES_TML_CG] = "afe_ul1_aht",
	[CLK_AFE_UL1_ADC_HIRES_CG] = "afe_ul1_adc_hires",
	[CLK_AFE_UL1_TML_CG] = "afe_ul1_tml",
	[CLK_AFE_UL1_ADC_CG] = "afe_ul1_adc",
	[CLK_AFE_UL0_ADC_HIRES_TML_CG] = "afe_ul0_aht",
	[CLK_AFE_UL0_ADC_HIRES_CG] = "afe_ul0_adc_hires",
	[CLK_AFE_UL0_TML_CG] = "afe_ul0_tml",
	[CLK_AFE_UL0_ADC_CG] = "afe_ul0_adc",
	[CLK_AFE_ETDM_IN4_CG] = "afe_etdm_in4",
	[CLK_AFE_ETDM_IN2_CG] = "afe_etdm_in2",
	[CLK_AFE_ETDM_IN1_CG] = "afe_etdm_in1",
	[CLK_AFE_ETDM_OUT4_CG] = "afe_etdm_out4",
	[CLK_AFE_ETDM_OUT2_CG] = "afe_etdm_out2",
	[CLK_AFE_ETDM_OUT1_CG] = "afe_etdm_out1",
	[CLK_AFE_GENERAL3_ASRC_CG] = "afe_general3_asrc",
	[CLK_AFE_GENERAL2_ASRC_CG] = "afe_general2_asrc",
	[CLK_AFE_GENERAL1_ASRC_CG] = "afe_general1_asrc",
	[CLK_AFE_GENERAL0_ASRC_CG] = "afe_general0_asrc",
	[CLK_AFE_CONNSYS_I2S_ASRC_CG] = "afe_connsys_i2s_asrc",
	[CLK_CAM_M_LARB13_CG] = "cam_m_larb13",
	[CLK_CAM_M_LARB14_CG] = "cam_m_larb14",
	[CLK_CAM_M_CAM_CG] = "cam_m_cam",
	[CLK_CAM_M_CAMTG_CG] = "cam_m_camtg",
	[CLK_CAM_M_SENINF_CG] = "cam_m_seninf",
	[CLK_CAM_M_CAMSV1_CG] = "cam_m_camsv1",
	[CLK_CAM_M_CAMSV2_CG] = "cam_m_camsv2",
	[CLK_CAM_M_CAMSV3_CG] = "cam_m_camsv3",
	[CLK_CAM_M_CAM2MM_GALS_CG] = "cam_m_cam2mm_gals",
	[CLK_CAM_M_PDA_CG] = "cam_m_pda",
	[CLK_CAM_RA_LARBX_CG] = "cam_ra_larbx",
	[CLK_CAM_RA_CAM_CG] = "cam_ra_cam",
	[CLK_CAM_RA_CAMTG_CG] = "cam_ra_camtg",
	[CLK_CAM_RB_LARBX_CG] = "cam_rb_larbx",
	[CLK_CAM_RB_CAM_CG] = "cam_rb_cam",
	[CLK_CAM_RB_CAMTG_CG] = "cam_rb_camtg",
	[CLK_MM_DISP_OVL0_2L_CG] = "mm_disp_ovl0_2l",
	[CLK_MM_DISP_OVL1_2L_CG] = "mm_disp_ovl1_2l",
	[CLK_MM_DISP_OVL2_2L_CG] = "mm_disp_ovl2_2l",
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
	[CLK_MM_DISP_DSC_WRAP0_CG] = "mm_disp_dsc_wrap0",
	[CLK_MM_DISP_DSI0_CG] = "mm_CLK0",
	[CLK_MM_DISP_WDMA1_CG] = "mm_disp_wdma1",
	[CLK_MM_DISP_APB_BUS_CG] = "mm_disp_apb_bus",
	[CLK_MM_DISP_FAKE_ENG0_CG] = "mm_disp_fake_eng0",
	[CLK_MM_DISP_FAKE_ENG1_CG] = "mm_disp_fake_eng1",
	[CLK_MM_DISP_MUTEX0_CG] = "mm_disp_mutex0",
	[CLK_MM_SMI_COMMON_CG] = "mm_smi_common",
	[CLK_MM_DSI0_CG] = "mm_dsi0_ck",
	[CLK_MM_26M_CG] = "mm_26m_ck",
	[CLK_IMGSYS1_LARB9_CG] = "imgsys1_larb9",
	[CLK_IMGSYS1_LARB10_CG] = "imgsys1_larb10",
	[CLK_IMGSYS1_DIP_CG] = "imgsys1_dip",
	[CLK_IMGSYS2_LARB9_CG] = "imgsys2_larb9",
	[CLK_IMGSYS2_MFB_CG] = "imgsys2_mfb",
	[CLK_IMGSYS2_WPE_CG] = "imgsys2_wpe",
	[CLK_IMGSYS2_MSS_CG] = "imgsys2_mss",
	[CLK_IMPC_I2C1_CG] = "impc_i2c1",
	[CLK_IMPC_I2C3_CG] = "impc_i2c3",
	[CLK_IMPC_I2C5_CG] = "impc_i2c5",
	[CLK_IMPC_I2C6_CG] = "impc_i2c6",
	[CLK_IMPC_I2C10_CG] = "impc_i2c10",
	[CLK_IMPC_I2C12_CG] = "impc_i2c12",
	[CLK_IMPES_I2C4_CG] = "impes_i2c4",
	[CLK_IMPES_I2C7_CG] = "impes_i2c7",
	[CLK_IMPES_I2C8_CG] = "impes_i2c8",
	[CLK_IMPS_I2C0_CG] = "imps_i2c0",
	[CLK_IMPS_I2C2_CG] = "imps_i2c2",
	[CLK_IMPS_I2C9_CG] = "imps_i2c9",
	[CLK_IMPS_I2C11_CG] = "imps_i2c11",
	[CLK_INFRACFG_AO_REG_CCIF1_AP_CG] = "infracfg_ao_ccif1_ap",
	[CLK_INFRACFG_AO_REG_CCIF1_MD_CG] = "infracfg_ao_ccif1_md",
	[CLK_INFRACFG_AO_REG_CCIF_AP_CG] = "infracfg_ao_ccif_ap",
	[CLK_INFRACFG_AO_REG_CCIF_MD_CG] = "infracfg_ao_ccif_md",
	[CLK_INFRACFG_AO_REG_CLDMA_BCLK_CG] = "infracfg_ao_cldmabclk",
	[CLK_INFRACFG_AO_REG_CCIF5_MD_CG] = "infracfg_ao_ccif5_md",
	[CLK_INFRACFG_AO_REG_CCIF2_AP_CG] = "infracfg_ao_ccif2_ap",
	[CLK_INFRACFG_AO_REG_CCIF2_MD_CG] = "infracfg_ao_ccif2_md",
	[CLK_INFRACFG_AO_REG_DPMAIF_MAIN_CG] = "infracfg_ao_dpmaif_main",
	[CLK_INFRACFG_AO_REG_CCIF4_MD_CG] = "infracfg_ao_ccif4_md",
	[CLK_INFRACFG_AO_REG_RG_MMW_DPMAIF26M_CG] = "infracfg_ao_dpmaif_26m",
	[CLK_IPE_LARB20_CG] = "ipe_larb20",
	[CLK_IPE_SMI_SUBCOM_CG] = "ipe_smi_subcom",
	[CLK_IPE_FD_CG] = "ipe_fd",
	[CLK_IPE_RSC_CG] = "ipe_rsc",
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
	[CLK_MDP_RSZ2_CG] = "mdp_rsz2",
	[CLK_MDP_WROT2_CG] = "mdp_wrot2",
	[CLK_MDP_DLO_ASYNC0_CG] = "mdp_dlo_async0",
	[CLK_MDP_FMM_IMG_DL_ASYNC0_CG] = "mdp_fmm_img_dl_async0",
	[CLK_MDP_FMM_IMG_DL_ASYNC1_CG] = "mdp_fmm_img_dl_async1",
	[CLK_MDP_FIMG_IMG_DL_ASYNC0_CG] = "mdp_fimg_img_dl_async0",
	[CLK_MDP_FIMG_IMG_DL_ASYNC1_CG] = "mdp_fimg_img_dl_async1",
	[CLK_MMINFRA_GCE_D_CG] = "mminfra_gce_d",
	[CLK_MMINFRA_GCE_M_CG] = "mminfra_gce_m",
	[CLK_MMINFRA_GCE_26M_CG] = "mminfra_gce_26m",
	[CLK_PERAO_P_UART0_CG] = "perao_p_uart0",
	[CLK_PERAO_P_UART1_CG] = "perao_p_uart1",
	[CLK_PERAO_P_UART2_CG] = "perao_p_uart2",
	[CLK_PERAO_P_UART3_CG] = "perao_p_uart3",
	[CLK_PERAO_P_PWM_H_CG] = "perao_p_pwm_h",
	[CLK_PERAO_P_PWM_B_CG] = "perao_p_pwm_b",
	[CLK_PERAO_P_PWM_FB1_CG] = "perao_p_pwm_fb1",
	[CLK_PERAO_P_PWM_FB2_CG] = "perao_p_pwm_fb2",
	[CLK_PERAO_P_PWM_FB3_CG] = "perao_p_pwm_fb3",
	[CLK_PERAO_P_PWM_FB4_CG] = "perao_p_pwm_fb4",
	[CLK_PERAO_P_DISP_PWM0_CG] = "perao_p_disp_pwm0",
	[CLK_PERAO_P_DISP_PWM1_CG] = "perao_p_disp_pwm1",
	[CLK_PERAO_P_SPI0_B_CG] = "perao_p_spi0_b",
	[CLK_PERAO_P_SPI1_B_CG] = "perao_p_spi1_b",
	[CLK_PERAO_P_SPI2_B_CG] = "perao_p_spi2_b",
	[CLK_PERAO_P_SPI3_B_CG] = "perao_p_spi3_b",
	[CLK_PERAO_P_SPI4_B_CG] = "perao_p_spi4_b",
	[CLK_PERAO_P_SPI5_B_CG] = "perao_p_spi5_b",
	[CLK_PERAO_P_SPI6_B_CG] = "perao_p_spi6_b",
	[CLK_PERAO_P_SPI7_B_CG] = "perao_p_spi7_b",
	[CLK_PERAO_P_DMA_B_CG] = "perao_p_dma_b",
	[CLK_PERAO_P_MSDC1_CG] = "perao_p_msdc1",
	[CLK_PERAO_P_MSDC1_H_CG] = "perao_p_msdc1_h",
	[CLK_PERAO_P_MSDC1_MST_F_CG] = "perao_p_msdc1_mst_f",
	[CLK_PERAO_P_MSDC1_SLV_H_CG] = "perao_p_msdc1_slv_h",
	[CLK_PERAO_P_AUDIO0_CG] = "perao_p_audio0",
	[CLK_PERAO_P_AUDIO1_CG] = "perao_p_audio1",
	[CLK_PERAO_P_AUDIO2_CG] = "perao_p_audio2",
	[CLK_UFSAO_UNIPRO_TX_SYM_CG] = "ufsao_unipro_tx_sym",
	[CLK_UFSAO_UNIPRO_SYS_CG] = "ufsao_unipro_sys",
	[CLK_UFSAO_UNIPRO_SAP_CFG_CG] = "ufsao_unipro_sap_cfg",
	[CLK_UFSPDN_UFSHCI_UFS_CG] = "ufspdn_ufshci_ufs",
	[CLK_UFSPDN_UFSHCI_AES_CG] = "ufspdn_ufshci_aes",
	[CLK_UFSPDN_UFSHCI_UFS_AHB_CG] = "ufspdn_ufshci_ufs_ahb",
	[CLK_UFSPDN_UFS_26M_CG] = "ufspdn_ufs_26m_ck",
	[CLK_VDE2_VDEC_CKEN_CG] = "vde2_vdec_cken",
	[CLK_VDE2_VDEC_ACTIVE_CG] = "vde2_vdec_active",
	[CLK_VDE2_VDEC_CKEN_ENG_CG] = "vde2_vdec_cken_eng",
	[CLK_VDE2_LAT_CKEN_CG] = "vde2_lat_cken",
	[CLK_VDE2_LAT_ACTIVE_CG] = "vde2_lat_active",
	[CLK_VDE2_LAT_CKEN_ENG_CG] = "vde2_lat_cken_eng",
	[CLK_VEN1_CKE0_LARB_CG] = "ven1_larb",
	[CLK_VEN1_CKE1_VENC_CG] = "ven1_venc",
	[CLK_VEN1_CKE2_JPGENC_CG] = "ven1_jpgenc",
	[CLK_VEN1_CKE5_GALS_CG] = "ven1_gals",
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
	[top] = REGBASE_V(0x10000000, top, PD_NULL, CLK_NULL),
	[infra_infracfg_ao_reg] = REGBASE_V(0x10001000, infra_infracfg_ao_reg, PD_NULL, CLK_NULL),
	[apmixed] = REGBASE_V(0x1000C000, apmixed, PD_NULL, CLK_NULL),
	[nemicfg_ao_mem_reg_bus] = REGBASE_V(0x10270000, nemicfg_ao_mem_reg_bus, PD_NULL, CLK_NULL),
	[ssr_top] = REGBASE_V(0x10400000, ssr_top, PD_NULL, CLK_NULL),
	[perao] = REGBASE_V(0x11032000, perao, PD_NULL, CLK_NULL),
	[afe] = REGBASE_V(0x11050000, afe, MT6858_CHK_PD_AUDIO, CLK_NULL),
	[impc] = REGBASE_V(0x11280000, impc, PD_NULL, "top_i2c_sel"),
	[ufsao] = REGBASE_V(0x112b8000, ufsao, PD_NULL, CLK_NULL),
	[ufspdn] = REGBASE_V(0x112bb000, ufspdn, PD_NULL, CLK_NULL),
	[impes] = REGBASE_V(0x11c00000, impes, PD_NULL, "top_i2c_sel"),
	[imps] = REGBASE_V(0x11d00000, imps, PD_NULL, "top_i2c_sel"),
	[mm] = REGBASE_V(0x14000000, mm, MT6858_CHK_PD_DIS0, CLK_NULL),
	[imgsys1] = REGBASE_V(0x15020000, imgsys1, MT6858_CHK_PD_ISP_IMG1, CLK_NULL),
	[img_sub0_bus] = REGBASE_V(0x1502F000, img_sub0_bus, MT6858_CHK_PD_ISP_IMG1, CLK_NULL),
	[imgsys2] = REGBASE_V(0x15820000, imgsys2, MT6858_CHK_PD_ISP_IMG2, CLK_NULL),
	[vde2] = REGBASE_V(0x1602f000, vde2, MT6858_CHK_PD_VDE0, CLK_NULL),
	[ven1] = REGBASE_V(0x17000000, ven1, MT6858_CHK_PD_VEN0, CLK_NULL),
	[cam_sub1_bus] = REGBASE_V(0x1A00C000, cam_sub1_bus, MT6858_CHK_PD_CAM_MAIN, CLK_NULL),
	[cam_sub0_bus] = REGBASE_V(0x1A00D000, cam_sub0_bus, MT6858_CHK_PD_CAM_MAIN, CLK_NULL),
	[ipe_sub0_bus] = REGBASE_V(0x1B00E000, ipe_sub0_bus, MT6858_CHK_PD_ISP_IPE, CLK_NULL),
	[spm] = REGBASE_V(0x1C001000, spm, PD_NULL, CLK_NULL),
	[vlpcfg_reg_bus] = REGBASE_V(0x1C00C000, vlpcfg_reg_bus, PD_NULL, CLK_NULL),
	[vlp_top] = REGBASE_V(0x1C012000, vlp_top, PD_NULL, CLK_NULL),
	[cam_m] = REGBASE_V(0x1a000000, cam_m, MT6858_CHK_PD_CAM_MAIN, CLK_NULL),
	[cam_ra] = REGBASE_V(0x1a04f000, cam_ra, MT6858_CHK_PD_CAM_SUBA, CLK_NULL),
	[cam_rb] = REGBASE_V(0x1a06f000, cam_rb, MT6858_CHK_PD_CAM_SUBB, CLK_NULL),
	[ipe] = REGBASE_V(0x1b000000, ipe, MT6858_CHK_PD_ISP_IPE, CLK_NULL),
	[mminfra_config] = REGBASE_V(0x1e800000, mminfra_config, MT6858_CHK_PD_MM_INFRA, CLK_NULL),
	[mdp] = REGBASE_V(0x1f000000, mdp, MT6858_CHK_PD_DIS0, CLK_NULL),
	[hwv] = REGBASE_V(0x10320000, hwv, PD_NULL, CLK_NULL),
	{},
};

#define REGNAME(_base, _ofs, _name)	\
	{ .base = &rb[_base], .id = _base, .ofs = _ofs, .name = #_name }

static struct regname rn[] = {
	/* CKSYS_REG register */
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
	REGNAME(top, 0x0320, CLK_AUDDIV_0),
	REGNAME(top, 0x0328, CLK_AUDDIV_2),
	REGNAME(top, 0x0534, CLK_FENC_STATUS_MON_0),
	REGNAME(top, 0x0538, CLK_FENC_STATUS_MON_1),
	/* INFRA_INFRACFG_AO_REG register */
	REGNAME(infra_infracfg_ao_reg, 0x94, MODULE_CG_1),
	REGNAME(infra_infracfg_ao_reg, 0xAC, MODULE_CG_2),
	REGNAME(infra_infracfg_ao_reg, 0xC8, MODULE_CG_3),
	REGNAME(infra_infracfg_ao_reg, 0xE8, MODULE_CG_4),
	REGNAME(infra_infracfg_ao_reg, 0x0C50, INFRASYS_PROTECT_EN_STA_1),
	REGNAME(infra_infracfg_ao_reg, 0x0C5C, INFRASYS_PROTECT_RDY_STA_1),
	REGNAME(infra_infracfg_ao_reg, 0x0C40, INFRASYS_PROTECT_EN_STA_0),
	REGNAME(infra_infracfg_ao_reg, 0x0C4C, INFRASYS_PROTECT_RDY_STA_0),
	REGNAME(infra_infracfg_ao_reg, 0x0C90, MCU_CONNSYS_PROTECT_EN_STA_0),
	REGNAME(infra_infracfg_ao_reg, 0x0C9C, MCU_CONNSYS_PROTECT_RDY_STA_0),
	REGNAME(infra_infracfg_ao_reg, 0x0CA0, MD_MFGSYS_PROTECT_EN_STA_0),
	REGNAME(infra_infracfg_ao_reg, 0x0CAC, MD_MFGSYS_PROTECT_RDY_STA_0),
	REGNAME(infra_infracfg_ao_reg, 0x0C20, MMSYS_PROTECT_EN_STA_1),
	REGNAME(infra_infracfg_ao_reg, 0x0C2C, MMSYS_PROTECT_RDY_STA_1),
	REGNAME(infra_infracfg_ao_reg, 0x0CC0, DRAMC_CCUSYS_PROTECT_EN_STA_0),
	REGNAME(infra_infracfg_ao_reg, 0x0CCC, DRAMC_CCUSYS_PROTECT_RDY_STA_0),
	REGNAME(infra_infracfg_ao_reg, 0x0C60, EMISYS_PROTECT_EN_STA_0),
	REGNAME(infra_infracfg_ao_reg, 0x0C6C, EMISYS_PROTECT_RDY_STA_0),
	REGNAME(infra_infracfg_ao_reg, 0x0C80, PERISYS_PROTECT_EN_STA_0),
	REGNAME(infra_infracfg_ao_reg, 0x0C8C, PERISYS_PROTECT_RDY_STA_0),
	REGNAME(infra_infracfg_ao_reg, 0x0C10, MMSYS_PROTECT_EN_STA_0),
	REGNAME(infra_infracfg_ao_reg, 0x0C1C, MMSYS_PROTECT_RDY_STA_0),
	/* APMIXEDSYS register */
	REGNAME(apmixed, 0x000C, AP_PLL_CON3),
	REGNAME(apmixed, 0x0040, APLL1_TUNER_CON0),
	REGNAME(apmixed, 0x0044, APLL2_TUNER_CON0),
	REGNAME(apmixed, 0x0208, ARMPLL_LL_CON0),
	REGNAME(apmixed, 0x020c, ARMPLL_LL_CON1),
	REGNAME(apmixed, 0x0214, ARMPLL_LL_CON3),
	REGNAME(apmixed, 0x0210, ARMPLL_LL_CON2),
	REGNAME(apmixed, 0x0218, ARMPLL_BL_CON0),
	REGNAME(apmixed, 0x021c, ARMPLL_BL_CON1),
	REGNAME(apmixed, 0x0224, ARMPLL_BL_CON3),
	REGNAME(apmixed, 0x0220, ARMPLL_BL_CON2),
	REGNAME(apmixed, 0x0238, CCIPLL_CON0),
	REGNAME(apmixed, 0x023c, CCIPLL_CON1),
	REGNAME(apmixed, 0x0240, CCIPLL_CON2),
	REGNAME(apmixed, 0x0244, CCIPLL_CON3),
	REGNAME(apmixed, 0x0248, TVDPLL_CON0),
	REGNAME(apmixed, 0x024c, TVDPLL_CON1),
	REGNAME(apmixed, 0x0250, TVDPLL_CON2),
	REGNAME(apmixed, 0x0254, TVDPLL_CON3),
	REGNAME(apmixed, 0x0308, UNIVPLL_CON0),
	REGNAME(apmixed, 0x030c, UNIVPLL_CON1),
	REGNAME(apmixed, 0x0310, UNIVPLL_CON2),
	REGNAME(apmixed, 0x0314, UNIVPLL_CON3),
	REGNAME(apmixed, 0x0328, APLL1_CON0),
	REGNAME(apmixed, 0x032c, APLL1_CON1),
	REGNAME(apmixed, 0x0330, APLL1_CON2),
	REGNAME(apmixed, 0x0334, APLL1_CON3),
	REGNAME(apmixed, 0x0338, APLL1_CON4),
	REGNAME(apmixed, 0x033c, APLL2_CON0),
	REGNAME(apmixed, 0x0340, APLL2_CON1),
	REGNAME(apmixed, 0x0344, APLL2_CON2),
	REGNAME(apmixed, 0x0348, APLL2_CON3),
	REGNAME(apmixed, 0x034c, APLL2_CON4),
	REGNAME(apmixed, 0x0350, MAINPLL_CON0),
	REGNAME(apmixed, 0x0354, MAINPLL_CON1),
	REGNAME(apmixed, 0x0358, MAINPLL_CON2),
	REGNAME(apmixed, 0x035c, MAINPLL_CON3),
	REGNAME(apmixed, 0x0360, MSDCPLL_CON0),
	REGNAME(apmixed, 0x0364, MSDCPLL_CON1),
	REGNAME(apmixed, 0x0368, MSDCPLL_CON2),
	REGNAME(apmixed, 0x036c, MSDCPLL_CON3),
	REGNAME(apmixed, 0x0380, ADSPPLL_CON0),
	REGNAME(apmixed, 0x0384, ADSPPLL_CON1),
	REGNAME(apmixed, 0x0388, ADSPPLL_CON2),
	REGNAME(apmixed, 0x038C, ADSPPLL_CON3),
	REGNAME(apmixed, 0x03A0, MMPLL_CON0),
	REGNAME(apmixed, 0x03A4, MMPLL_CON1),
	REGNAME(apmixed, 0x03A8, MMPLL_CON2),
	REGNAME(apmixed, 0x03AC, MMPLL_CON3),
	REGNAME(apmixed, 0x03B0, EMIPLL_CON0),
	REGNAME(apmixed, 0x03B4, EMIPLL_CON1),
	REGNAME(apmixed, 0x03B8, EMIPLL_CON2),
	REGNAME(apmixed, 0x03BC, EMIPLL_CON3),
	REGNAME(apmixed, 0x040C, FENC_STATUS_CON0),
	REGNAME(apmixed, 0x0428, FENC_ENABLE_CON0),
	/* NEMICFG_AO_MEM_REG_BUS register */
	REGNAME(nemicfg_ao_mem_reg_bus, 0x80, GLITCH_PROTECT_EN),
	REGNAME(nemicfg_ao_mem_reg_bus, 0x8c, GLITCH_PROTECT_RDY),
	/* SSR_TOP register */
	REGNAME(ssr_top, 0x44, SSR_AO_CTRL0),
	REGNAME(ssr_top, 0x50, SSR_AO_STATUS0),
	/* PERICFG_AO register */
	REGNAME(perao, 0x10, PERI_CG_0),
	REGNAME(perao, 0x14, PERI_CG_1),
	REGNAME(perao, 0x18, PERI_CG_2),
	/* AFE register */
	REGNAME(afe, 0x1204, AFE_AUD_PAD_TOP_CFG0),
	REGNAME(afe, 0x0, AUDIO_TOP_0),
	REGNAME(afe, 0x4, AUDIO_TOP_1),
	REGNAME(afe, 0x8, AUDIO_TOP_2),
	REGNAME(afe, 0xC, AUDIO_TOP_3),
	REGNAME(afe, 0x10, AUDIO_TOP_4),
	/* IMP_IIC_WRAP_C register */
	REGNAME(impc, 0xE00, AP_CLOCK_CG),
	/* UFSCFG_AO register */
	REGNAME(ufsao, 0x4, UFS_AO_CG_0),
	/* UFSCFG_PDN register */
	REGNAME(ufspdn, 0x4, UFS_PDN_CG_0),
	/* IMP_IIC_WRAP_ES register */
	REGNAME(impes, 0xE00, AP_CLOCK_CG),
	/* IMP_IIC_WRAP_S register */
	REGNAME(imps, 0xE00, AP_CLOCK_CG),
	/* DISPSYS_CONFIG register */
	REGNAME(mm, 0x100, MMSYS_CG_0),
	REGNAME(mm, 0x110, MMSYS_CG_1),
	/* IMGSYS1 register */
	REGNAME(imgsys1, 0x0, IMG_CG),
	/* IMG_SUB0_BUS register */
	REGNAME(img_sub0_bus, 0x3c0, SMI_COMMON_PROTECT_EN),
	REGNAME(img_sub0_bus, 0x3c0, SMI_COMMON_PROTECT_EN),
	/* IMGSYS2 register */
	REGNAME(imgsys2, 0x0, IMG_CG),
	/* VDEC_GCON_BASE register */
	REGNAME(vde2, 0x200, LAT_CKEN),
	REGNAME(vde2, 0x0, VDEC_CKEN),
	/* VENC_GCON register */
	REGNAME(ven1, 0x0, VENCSYS_CG),
	/* CAM_SUB1_BUS register */
	REGNAME(cam_sub1_bus, 0x3c0, SMI_COMMON_PROTECT_EN),
	REGNAME(cam_sub1_bus, 0x3c0, SMI_COMMON_PROTECT_EN),
	/* CAM_SUB0_BUS register */
	REGNAME(cam_sub0_bus, 0x3c0, SMI_COMMON_PROTECT_EN),
	REGNAME(cam_sub0_bus, 0x3c0, SMI_COMMON_PROTECT_EN),
	/* IPE_SUB0_BUS register */
	REGNAME(ipe_sub0_bus, 0x3c0, SMI_COMMON_PROTECT_EN),
	REGNAME(ipe_sub0_bus, 0x3c0, SMI_COMMON_PROTECT_EN),
	/* SPM register */
	REGNAME(spm, 0xE00, MD1_PWR_CON),
	REGNAME(spm, 0xF3C, PWR_STATUS),
	REGNAME(spm, 0xF40, PWR_STATUS_2ND),
	REGNAME(spm, 0xF20, MD_BUCK_ISO_CON),
	REGNAME(spm, 0xE04, CONN_PWR_CON),
	REGNAME(spm, 0xE10, UFS0_PWR_CON),
	REGNAME(spm, 0xE18, AUDIO_PWR_CON),
	REGNAME(spm, 0xE28, ISP_IMG1_PWR_CON),
	REGNAME(spm, 0xE2C, ISP_IMG2_PWR_CON),
	REGNAME(spm, 0xE30, ISP_IPE_PWR_CON),
	REGNAME(spm, 0xE34, VDE0_PWR_CON),
	REGNAME(spm, 0xE3C, VEN0_PWR_CON),
	REGNAME(spm, 0xE44, CAM_MAIN_PWR_CON),
	REGNAME(spm, 0xE4C, CAM_SUBA_PWR_CON),
	REGNAME(spm, 0xE50, CAM_SUBB_PWR_CON),
	REGNAME(spm, 0xE6C, DIS0_PWR_CON),
	REGNAME(spm, 0xE74, MM_INFRA_PWR_CON),
	REGNAME(spm, 0xF24, SOC_BUCK_ISO_CON),
	REGNAME(spm, 0xE78, MM_PROC_PWR_CON),
	REGNAME(spm, 0xE98, CSI_RX_PWR_CON),
	REGNAME(spm, 0xE9C, SSRSYS_PWR_CON),
	REGNAME(spm, 0xEA4, SSUSB_PWR_CON),
	REGNAME(spm, 0xEB0, MFG0_PWR_CON),
	REGNAME(spm, 0xF4C, XPU_PWR_STATUS),
	REGNAME(spm, 0xF50, XPU_PWR_STATUS_2ND),
	/* VLPCFG_REG_BUS register */
	REGNAME(vlpcfg_reg_bus, 0x0210, VLP_TOPAXI_PROTECTEN),
	REGNAME(vlpcfg_reg_bus, 0x0220, VLP_TOPAXI_PROTECTEN_STA1),
	/* VLP_CKSYS_TOP register */
	REGNAME(vlp_top, 0x0008, VLP_CLK_CFG_0),
	REGNAME(vlp_top, 0x0014, VLP_CLK_CFG_1),
	REGNAME(vlp_top, 0x0020, VLP_CLK_CFG_2),
	REGNAME(vlp_top, 0x002C, VLP_CLK_CFG_3),
	REGNAME(vlp_top, 0x0038, VLP_CLK_CFG_4),
	REGNAME(vlp_top, 0x0044, VLP_CLK_CFG_5),
	REGNAME(vlp_top, 0x0050, VLP_CLK_CFG_6),
	REGNAME(vlp_top, 0x0328, VLP_OCIC_FENC_STATUS_MON_0),
	/* CAMSYS_MAIN register */
	REGNAME(cam_m, 0x0, CAMSYS_CG),
	/* CAMSYS_RAWA register */
	REGNAME(cam_ra, 0x0, CAMSYS_CG),
	/* CAMSYS_RAWB register */
	REGNAME(cam_rb, 0x0, CAMSYS_CG),
	/* IPESYS register */
	REGNAME(ipe, 0x0, IMG_CG),
	/* MMINFRA_CONFIG register */
	REGNAME(mminfra_config, 0x100, MMINFRA_CG_0),
	REGNAME(mminfra_config, 0x110, MMINFRA_CG_1),
	/* MDPSYS_CONFIG register */
	REGNAME(mdp, 0x100, MDPSYS_CG_0),
	REGNAME(mdp, 0x110, MDPSYS_CG_1),
	/* HWV register */
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

static const struct regname *get_all_mt6858_regnames(void)
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

u32 get_mt6858_reg_value(u32 id, u32 ofs)
{
	if (id >= chk_sys_num)
		return 0;

	return clk_readl(rb[id].virt + ofs);
}
EXPORT_SYMBOL_GPL(get_mt6858_reg_value);

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
	{"camsys_main", cam_m, spm, 0x0E44},
	{"camsys_rawa", cam_ra, spm, 0x0E4C},
	{"camsys_rawb", cam_rb, spm, 0x0E50},
	{"mmsys0", mm, spm, 0x0E6C},
	{"imgsys1", imgsys1, spm, 0x0E28},
	{"imgsys2", imgsys2, spm, 0x0E2C},
	{"ipesys", ipe, spm, 0x0E30},
	{"mdpsys", mdp, spm, 0x0E6C},
	{"ufscfg_pdn", ufspdn, spm, 0x0E10},
	{"vdecsys", vde2, spm, 0x0E34},
	{"vencsys", ven1, spm, 0x0E3C},
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
	int freq_table[4];
};

#define MTK_VF_TABLE(_n, _freq0, _freq1, _freq2, _freq3) {		\
		.name = _n,		\
		.freq_table = {_freq0, _freq1, _freq2, _freq3},	\
	}

/*
 * Opp0 : 0p725v
 * Opp1 : 0p650v
 * Opp2 : 0p600v
 * Opp3 : 0p550v
 */
static struct mtk_vf vf_table[] = {
	/* Opp0, Opp1, Opp2, Opp3 */
	MTK_VF_TABLE("top_axi_sel", 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("top_axi_p_sel", 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("top_axi_ufs_sel", 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("top_bus_aximem_sel", 364000, 273000, 273000, 218400),
	MTK_VF_TABLE("top_disp0_sel", 594000, 458333, 343750, 229167),
	MTK_VF_TABLE("top_mminfra_sel", 624000, 436800, 343750, 229167),
	MTK_VF_TABLE("top_mmup_sel", 728000, 728000, 728000, 728000),
	MTK_VF_TABLE("top_uart_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("top_spi0_sel", 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("top_spi1_sel", 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("top_spi2_sel", 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("top_spi3_sel", 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("top_spi4_sel", 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("top_spi5_sel", 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("top_spi6_sel", 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("top_spi7_sel", 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("top_msdc_macro_1p_sel", 384000, 384000, 384000, 384000),
	MTK_VF_TABLE("top_msdc30_1_sel", 192000, 192000, 192000, 192000),
	MTK_VF_TABLE("top_msdc30_1_h_sel", 192000, 192000, 192000, 192000),
	MTK_VF_TABLE("top_aud_intbus_sel", 136500, 136500, 136500, 136500),
	MTK_VF_TABLE("top_atb_sel", 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("top_disp_pwm_sel", 130000, 130000, 130000, 130000),
	MTK_VF_TABLE("top_usb_sel", 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("top_usb_xhci_sel", 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("top_i2c_sel", 136500, 136500, 136500, 136500),
	MTK_VF_TABLE("top_seninf_sel", 499200, 499200, 392857, 275000),
	MTK_VF_TABLE("top_seninf1_sel", 499200, 499200, 392857, 275000),
	MTK_VF_TABLE("top_seninf2_sel", 499200, 499200, 392857, 275000),
	MTK_VF_TABLE("top_aud_engen1_sel", 45158, 45158, 45158, 45158),
	MTK_VF_TABLE("top_aud_engen2_sel", 49152, 49152, 49152, 49152),
	MTK_VF_TABLE("top_aes_ufsfde_sel", 546000, 546000, 546000, 546000),
	MTK_VF_TABLE("top_ufs_sel", 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("top_aud_1_sel", 180634, 180634, 180634, 180634),
	MTK_VF_TABLE("top_aud_2_sel", 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("top_dpmaif_main_sel", 436800, 416000, 364000, 273000),
	MTK_VF_TABLE("top_venc_sel", 624000, 458333, 364000, 249600),
	MTK_VF_TABLE("top_vdec_sel", 546000, 416000, 312000, 218400),
	MTK_VF_TABLE("top_pwm_sel", 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("top_audio_h_sel", 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("top_mcupm_sel", 218400, 218400, 218400, 218400),
	MTK_VF_TABLE("top_mem_sub_sel", 546000, 436800, 273000, 218400),
	MTK_VF_TABLE("top_mem_sub_p_sel", 546000, 436800, 273000, 218400),
	MTK_VF_TABLE("top_mem_sub_ufs_sel", 546000, 436800, 273000, 218400),
	MTK_VF_TABLE("top_emi_n_sel", 688000, 688000, 688000, 688000),
	MTK_VF_TABLE("top_dsi_occ_sel", 312000, 312000, 249600, 208000),
	MTK_VF_TABLE("top_ap2conn_host_sel", 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("top_img1_sel", 624000, 458333, 343750, 229167),
	MTK_VF_TABLE("top_ipe_sel", 546000, 416000, 312000, 229167),
	MTK_VF_TABLE("top_cam_sel", 624000, 546000, 392857, 275000),
	MTK_VF_TABLE("top_camtm_sel", 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("top_md_emi_sel", 546000, 546000, 546000, 546000),
	MTK_VF_TABLE("top_mfg_ref_sel", 416000, 416000, 416000, 416000),
	MTK_VF_TABLE("top_mfgsc_ref_sel", 416000, 416000, 416000, 416000),
	MTK_VF_TABLE("top_efuse_sel", 26000, 26000, 26000, 26000),
	MTK_VF_TABLE("top_unipll_ses_sel", 1248000, 1248000, 1248000, 1248000),
	MTK_VF_TABLE("top_dramulp_sel", 182000, 182000, 182000, 182000),
	MTK_VF_TABLE("top_usb_frmcnt_sel", 48000, 48000, 48000, 48000),
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

void set_subsys_reg_dump_mt6858(enum chk_sys_id id[])
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
EXPORT_SYMBOL_GPL(set_subsys_reg_dump_mt6858);

void get_subsys_reg_dump_mt6858(void)
{
	const struct regname *rns = &rn[0];
	int i;

	for (i = 0; i < ARRAY_SIZE(rn) - 1; i++, rns++) {
		if (reg_dump_valid[i])
			pr_info("%-18s: [0x%08x] = 0x%08x\n",
					rns->name, reg_dump_addr[i], reg_dump_val[i]);
	}
}
EXPORT_SYMBOL_GPL(get_subsys_reg_dump_mt6858);

void print_subsys_reg_mt6858(enum chk_sys_id id)
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
EXPORT_SYMBOL_GPL(print_subsys_reg_mt6858);

void clkchk_debug_dump_mt6858(enum chk_sys_id id[],
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

	set_subsys_reg_dump_mt6858(id);
	get_subsys_reg_dump_mt6858();

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
EXPORT_SYMBOL_GPL(clkchk_debug_dump_mt6858);

/* debug dump register */
static enum chk_sys_id debug_dump_id[] = {
	spm,
	infra_infracfg_ao_reg,
	vlpcfg_reg_bus,
	top,
	apmixed,
	vlp_top,
	hwv,
	chk_sys_num,
};

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
static void devapc_dump(void)
{
	clkchk_debug_dump_mt6858(debug_dump_id, "clk_devapc", false, false);
}
static struct devapc_vio_callbacks devapc_vio_handle = {
	.id = DEVAPC_SUBSYS_CLKMGR,
	.debug_dump = devapc_dump,
};
#endif


#ifdef CONFIG_MTK_SERROR_HOOK
static void serror_dump(void)
{
	clkchk_debug_dump_mt6858(debug_dump_id, "clk_serror", false, false);
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
	"univpll",
	"msdcpll",
	"mmpll",
	"tvdpll",
	"apll1",
	"apll2",
	"univpll",
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
	"adsppll",
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
#if (BUG_ON_CHK_ENABLE) || (IS_ENABLED(CONFIG_MTK_CLKMGR_DEBUG))
	return true;
#endif
	return false;
}

static const char * const off_mux_names[] = {
	"top_disp0_sel",
	"top_mminfra_sel",
	"top_mmup_sel",
	"top_uart_sel",
	"top_spi0_sel",
	"top_spi1_sel",
	"top_spi2_sel",
	"top_spi3_sel",
	"top_spi4_sel",
	"top_spi5_sel",
	"top_spi6_sel",
	"top_spi7_sel",
	"top_msdc_macro_1p_sel",
	"top_msdc30_1_sel",
	"top_msdc30_1_h_sel",
	"top_aud_intbus_sel",
	"top_disp_pwm_sel",
	"top_i2c_sel",
	"top_seninf_sel",
	"top_seninf1_sel",
	"top_seninf2_sel",
	"top_aes_ufsfde_sel",
	"top_ufs_sel",
	"top_dpmaif_main_sel",
	"top_venc_sel",
	"top_vdec_sel",
	"top_pwm_sel",
	"top_dsi_occ_sel",
	"top_img1_sel",
	"top_ipe_sel",
	"top_cam_sel",
	"top_camtm_sel",
	"top_unipll_ses_sel",
	"top_usb_frmcnt_sel",
	"vlp_pwrap_sel",
	"vlp_pwm_vlp_sel",
	"vlp_camtg0_sel",
	"vlp_camtg1_sel",
	"vlp_camtg2_sel",
	"vlp_camtg3_sel",
	NULL
};


static const char * const notice_mux_names[] = {
	"top_aud_1_sel",
	"top_aud_2_sel",
	"top_aud_engen1_sel",
	"top_aud_engen2_sel",
	"top_audio_h_sel",
	NULL
};

static const char * const bypass_mux_name[] = {
	"top_usb_sel",
	"top_usb_xhci_sel",
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
	clkchk_debug_dump_mt6858(debug_dump_id, "hwv_cg_timeout", false, true);
}

static void dump_pll_reg(bool bug_on)
{
	clkchk_debug_dump_mt6858(debug_dump_id, "pll_abnormal", false, bug_on);
}

static void external_dump(void)
{
	clkchk_debug_dump_mt6858(debug_dump_id, NULL, false, false);
}

static void check_hwv_irq_sta(void)
{
	clkchk_debug_dump_mt6858(debug_dump_id, "hwv_irq", false, false);
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

static struct clkchk_ops clkchk_mt6858_ops = {
	.get_all_regnames = get_all_mt6858_regnames,
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

static int clk_chk_mt6858_probe(struct platform_device *pdev)
{
#ifdef CONFIG_MTK_SERROR_HOOK
	int ret = 0;
#endif

	suspend_cnt = 0;

	init_regbase();

	set_clkchk_notify();

	set_clkchk_ops(&clkchk_mt6858_ops);

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

static const struct of_device_id of_match_clkchk_mt6858[] = {
	{
		.compatible = "mediatek,mt6858-clkchk",
	}, {
		/* sentinel */
	}
};

static struct platform_driver clk_chk_mt6858_drv = {
	.probe = clk_chk_mt6858_probe,
	.driver = {
		.name = "clk-chk-mt6858",
		.owner = THIS_MODULE,
		.pm = &clk_chk_dev_pm_ops,
		.of_match_table = of_match_clkchk_mt6858,
	},
};

/*
 * init functions
 */

static int __init clkchk_mt6858_init(void)
{
	return platform_driver_register(&clk_chk_mt6858_drv);
}

static void __exit clkchk_mt6858_exit(void)
{
	platform_driver_unregister(&clk_chk_mt6858_drv);
}

subsys_initcall(clkchk_mt6858_init);
module_exit(clkchk_mt6858_exit);
MODULE_LICENSE("GPL");
