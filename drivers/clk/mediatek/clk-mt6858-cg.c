// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: KY Liu <ky.liu@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6858-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs afe0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs afe1_cg_regs = {
	.set_ofs = 0x10,
	.clr_ofs = 0x10,
	.sta_ofs = 0x10,
};

static const struct mtk_gate_regs afe2_cg_regs = {
	.set_ofs = 0x1204,
	.clr_ofs = 0x1204,
	.sta_ofs = 0x1204,
};

static const struct mtk_gate_regs afe3_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x4,
	.sta_ofs = 0x4,
};

static const struct mtk_gate_regs afe4_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0x8,
	.sta_ofs = 0x8,
};

static const struct mtk_gate_regs afe5_cg_regs = {
	.set_ofs = 0xC,
	.clr_ofs = 0xC,
	.sta_ofs = 0xC,
};

#define GATE_AFE0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE0_V(_id, _name, _parent) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_AFE1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE1_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_AFE2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_AFE2_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_AFE3(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe3_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE3_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_AFE4(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe4_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE4_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_AFE5(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe5_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE5_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate afe_clks[] = {
	/* AFE0 */
	GATE_AFE0(CLK_AFE_DL0_DAC_TML, "afe_dl0_dac_tml",
			"top_f26m_ck"/* parent */, 7),
	GATE_AFE0_V(CLK_AFE_DL0_DAC_TML_AUDIO, "afe_dl0_dac_tml_audio",
			"afe_dl0_dac_tml"/* parent */),
	GATE_AFE0(CLK_AFE_DL0_DAC_HIRES, "afe_dl0_dac_hires",
			"top_f26m_ck"/* parent */, 8),
	GATE_AFE0_V(CLK_AFE_DL0_DAC_HIRES_AUDIO, "afe_dl0_dac_hires_audio",
			"afe_dl0_dac_hires"/* parent */),
	GATE_AFE0(CLK_AFE_DL0_DAC, "afe_dl0_dac",
			"top_f26m_ck"/* parent */, 9),
	GATE_AFE0_V(CLK_AFE_DL0_DAC_AUDIO, "afe_dl0_dac_audio",
			"afe_dl0_dac"/* parent */),
	GATE_AFE0(CLK_AFE_DL0_PREDIS, "afe_dl0_predis",
			"top_f26m_ck"/* parent */, 10),
	GATE_AFE0_V(CLK_AFE_DL0_PREDIS_AUDIO, "afe_dl0_predis_audio",
			"afe_dl0_predis"/* parent */),
	GATE_AFE0(CLK_AFE_DL0_NLE, "afe_dl0_nle",
			"top_f26m_ck"/* parent */, 11),
	GATE_AFE0_V(CLK_AFE_DL0_NLE_AUDIO, "afe_dl0_nle_audio",
			"afe_dl0_nle"/* parent */),
	GATE_AFE0(CLK_AFE_PCM1, "afe_pcm1",
			"top_f26m_ck"/* parent */, 13),
	GATE_AFE0_V(CLK_AFE_PCM1_AUDIO, "afe_pcm1_audio",
			"afe_pcm1"/* parent */),
	GATE_AFE0(CLK_AFE_PCM0, "afe_pcm0",
			"top_f26m_ck"/* parent */, 14),
	GATE_AFE0_V(CLK_AFE_PCM0_AUDIO, "afe_pcm0_audio",
			"afe_pcm0"/* parent */),
	GATE_AFE0(CLK_AFE_CM1, "afe_cm1",
			"top_f26m_ck"/* parent */, 17),
	GATE_AFE0_V(CLK_AFE_CM1_AUDIO, "afe_cm1_audio",
			"afe_cm1"/* parent */),
	GATE_AFE0(CLK_AFE_CM0, "afe_cm0",
			"top_f26m_ck"/* parent */, 18),
	GATE_AFE0_V(CLK_AFE_CM0_AUDIO, "afe_cm0_audio",
			"afe_cm0"/* parent */),
	GATE_AFE0(CLK_AFE_STF, "afe_stf",
			"top_f26m_ck"/* parent */, 19),
	GATE_AFE0_V(CLK_AFE_STF_AUDIO, "afe_stf_audio",
			"afe_stf"/* parent */),
	GATE_AFE0(CLK_AFE_HW_GAIN23, "afe_hw_gain23",
			"top_f26m_ck"/* parent */, 20),
	GATE_AFE0_V(CLK_AFE_HW_GAIN23_AUDIO, "afe_hw_gain23_audio",
			"afe_hw_gain23"/* parent */),
	GATE_AFE0(CLK_AFE_HW_GAIN01, "afe_hw_gain01",
			"top_f26m_ck"/* parent */, 21),
	GATE_AFE0_V(CLK_AFE_HW_GAIN01_AUDIO, "afe_hw_gain01_audio",
			"afe_hw_gain01"/* parent */),
	/* AFE1 */
	GATE_AFE1(CLK_AFE_AUDIO_HOPPING, "afe_audio_hopping_ck",
			"top_f26m_ck"/* parent */, 0),
	GATE_AFE1_V(CLK_AFE_AUDIO_HOPPING_AUDIO, "afe_audio_hopping_ck_audio",
			"afe_audio_hopping_ck"/* parent */),
	GATE_AFE1(CLK_AFE_AUDIO_F26M, "afe_audio_f26m_ck",
			"top_f26m_ck"/* parent */, 1),
	GATE_AFE1_V(CLK_AFE_AUDIO_F26M_AUDIO, "afe_audio_f26m_ck_audio",
			"afe_audio_f26m_ck"/* parent */),
	GATE_AFE1(CLK_AFE_APLL1, "afe_apll1_ck",
			"top_aud_1_ck"/* parent */, 2),
	GATE_AFE1_V(CLK_AFE_APLL1_AUDIO, "afe_apll1_ck_audio",
			"afe_apll1_ck"/* parent */),
	GATE_AFE1(CLK_AFE_APLL2, "afe_apll2_ck",
			"top_aud_2_ck"/* parent */, 3),
	GATE_AFE1_V(CLK_AFE_APLL2_AUDIO, "afe_apll2_ck_audio",
			"afe_apll2_ck"/* parent */),
	GATE_AFE1(CLK_AFE_H208M, "afe_h208m_ck",
			"top_audio_h_ck"/* parent */, 4),
	GATE_AFE1_V(CLK_AFE_H208M_AUDIO, "afe_h208m_ck_audio",
			"afe_h208m_ck"/* parent */),
	GATE_AFE1(CLK_AFE_APLL_TUNER2, "afe_apll_tuner2",
			"top_aud_engen2_ck"/* parent */, 12),
	GATE_AFE1_V(CLK_AFE_APLL_TUNER2_AUDIO, "afe_apll_tuner2_audio",
			"afe_apll_tuner2"/* parent */),
	GATE_AFE1(CLK_AFE_APLL_TUNER1, "afe_apll_tuner1",
			"top_aud_engen1_ck"/* parent */, 13),
	GATE_AFE1_V(CLK_AFE_APLL_TUNER1_AUDIO, "afe_apll_tuner1_audio",
			"afe_apll_tuner1"/* parent */),
	/* AFE2 */
	GATE_AFE2(CLK_AFE_AUD_PAD_TOP_MOSI_EN, "afe_aud_pad_mosi",
			"top_f26m_ck"/* parent */, 7),
	GATE_AFE2_V(CLK_AFE_AUD_PAD_TOP_MOSI_EN_AUDIO, "afe_aud_pad_mosi_audio",
			"afe_aud_pad_mosi"/* parent */),
	/* AFE3 */
	GATE_AFE3(CLK_AFE_UL1_ADC_HIRES_TML, "afe_ul1_aht",
			"top_audio_h_ck"/* parent */, 16),
	GATE_AFE3_V(CLK_AFE_UL1_ADC_HIRES_TML_AUDIO, "afe_ul1_aht_audio",
			"afe_ul1_aht"/* parent */),
	GATE_AFE3(CLK_AFE_UL1_ADC_HIRES, "afe_ul1_adc_hires",
			"top_audio_h_ck"/* parent */, 17),
	GATE_AFE3_V(CLK_AFE_UL1_ADC_HIRES_AUDIO, "afe_ul1_adc_hires_audio",
			"afe_ul1_adc_hires"/* parent */),
	GATE_AFE3(CLK_AFE_UL1_TML, "afe_ul1_tml",
			"top_f26m_ck"/* parent */, 18),
	GATE_AFE3_V(CLK_AFE_UL1_TML_AUDIO, "afe_ul1_tml_audio",
			"afe_ul1_tml"/* parent */),
	GATE_AFE3(CLK_AFE_UL1_ADC, "afe_ul1_adc",
			"top_f26m_ck"/* parent */, 19),
	GATE_AFE3_V(CLK_AFE_UL1_ADC_AUDIO, "afe_ul1_adc_audio",
			"afe_ul1_adc"/* parent */),
	GATE_AFE3(CLK_AFE_UL0_ADC_HIRES_TML, "afe_ul0_aht",
			"top_audio_h_ck"/* parent */, 20),
	GATE_AFE3_V(CLK_AFE_UL0_ADC_HIRES_TML_AUDIO, "afe_ul0_aht_audio",
			"afe_ul0_aht"/* parent */),
	GATE_AFE3(CLK_AFE_UL0_ADC_HIRES, "afe_ul0_adc_hires",
			"top_audio_h_ck"/* parent */, 21),
	GATE_AFE3_V(CLK_AFE_UL0_ADC_HIRES_AUDIO, "afe_ul0_adc_hires_audio",
			"afe_ul0_adc_hires"/* parent */),
	GATE_AFE3(CLK_AFE_UL0_TML, "afe_ul0_tml",
			"top_f26m_ck"/* parent */, 22),
	GATE_AFE3_V(CLK_AFE_UL0_TML_AUDIO, "afe_ul0_tml_audio",
			"afe_ul0_tml"/* parent */),
	GATE_AFE3(CLK_AFE_UL0_ADC, "afe_ul0_adc",
			"top_f26m_ck"/* parent */, 23),
	GATE_AFE3_V(CLK_AFE_UL0_ADC_AUDIO, "afe_ul0_adc_audio",
			"afe_ul0_adc"/* parent */),
	/* AFE4 */
	GATE_AFE4(CLK_AFE_ETDM_IN4, "afe_etdm_in4",
			"top_f26m_ck"/* parent */, 9),
	GATE_AFE4_V(CLK_AFE_ETDM_IN4_AUDIO, "afe_etdm_in4_audio",
			"afe_etdm_in4"/* parent */),
	GATE_AFE4(CLK_AFE_ETDM_IN2, "afe_etdm_in2",
			"top_f26m_ck"/* parent */, 11),
	GATE_AFE4_V(CLK_AFE_ETDM_IN2_AUDIO, "afe_etdm_in2_audio",
			"afe_etdm_in2"/* parent */),
	GATE_AFE4(CLK_AFE_ETDM_IN1, "afe_etdm_in1",
			"top_f26m_ck"/* parent */, 12),
	GATE_AFE4_V(CLK_AFE_ETDM_IN1_AUDIO, "afe_etdm_in1_audio",
			"afe_etdm_in1"/* parent */),
	GATE_AFE4(CLK_AFE_ETDM_OUT4, "afe_etdm_out4",
			"top_f26m_ck"/* parent */, 17),
	GATE_AFE4_V(CLK_AFE_ETDM_OUT4_AUDIO, "afe_etdm_out4_audio",
			"afe_etdm_out4"/* parent */),
	GATE_AFE4(CLK_AFE_ETDM_OUT2, "afe_etdm_out2",
			"top_f26m_ck"/* parent */, 19),
	GATE_AFE4_V(CLK_AFE_ETDM_OUT2_AUDIO, "afe_etdm_out2_audio",
			"afe_etdm_out2"/* parent */),
	GATE_AFE4(CLK_AFE_ETDM_OUT1, "afe_etdm_out1",
			"top_f26m_ck"/* parent */, 20),
	GATE_AFE4_V(CLK_AFE_ETDM_OUT1_AUDIO, "afe_etdm_out1_audio",
			"afe_etdm_out1"/* parent */),
	/* AFE5 */
	GATE_AFE5(CLK_AFE_GENERAL3_ASRC, "afe_general3_asrc",
			"top_f26m_ck"/* parent */, 21),
	GATE_AFE5_V(CLK_AFE_GENERAL3_ASRC_AUDIO, "afe_general3_asrc_audio",
			"afe_general3_asrc"/* parent */),
	GATE_AFE5(CLK_AFE_GENERAL2_ASRC, "afe_general2_asrc",
			"top_f26m_ck"/* parent */, 22),
	GATE_AFE5_V(CLK_AFE_GENERAL2_ASRC_AUDIO, "afe_general2_asrc_audio",
			"afe_general2_asrc"/* parent */),
	GATE_AFE5(CLK_AFE_GENERAL1_ASRC, "afe_general1_asrc",
			"top_f26m_ck"/* parent */, 23),
	GATE_AFE5_V(CLK_AFE_GENERAL1_ASRC_AUDIO, "afe_general1_asrc_audio",
			"afe_general1_asrc"/* parent */),
	GATE_AFE5(CLK_AFE_GENERAL0_ASRC, "afe_general0_asrc",
			"top_f26m_ck"/* parent */, 24),
	GATE_AFE5_V(CLK_AFE_GENERAL0_ASRC_AUDIO, "afe_general0_asrc_audio",
			"afe_general0_asrc"/* parent */),
	GATE_AFE5(CLK_AFE_CONNSYS_I2S_ASRC, "afe_connsys_i2s_asrc",
			"top_f26m_ck"/* parent */, 25),
	GATE_AFE5_V(CLK_AFE_CONNSYS_I2S_ASRC_AUDIO, "afe_connsys_i2s_asrc_audio",
			"afe_connsys_i2s_asrc"/* parent */),
};

static const struct mtk_clk_desc afe_mcd = {
	.clks = afe_clks,
	.num_clks = CLK_AFE_NR_CLK,
};

static const struct mtk_gate_regs cam_m_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_M(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_m_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_M_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate cam_m_clks[] = {
	GATE_CAM_M(CLK_CAM_M_LARB13, "cam_m_larb13",
			"top_cam_ck"/* parent */, 0),
	GATE_CAM_M_V(CLK_CAM_M_LARB13_CAM, "cam_m_larb13_cam",
			"cam_m_larb13"/* parent */),
	GATE_CAM_M_V(CLK_CAM_M_LARB13_SMI, "cam_m_larb13_smi",
			"cam_m_larb13"/* parent */),
	GATE_CAM_M(CLK_CAM_M_LARB14, "cam_m_larb14",
			"top_cam_ck"/* parent */, 2),
	GATE_CAM_M_V(CLK_CAM_M_LARB14_CAM, "cam_m_larb14_cam",
			"cam_m_larb14"/* parent */),
	GATE_CAM_M_V(CLK_CAM_M_LARB14_SMI, "cam_m_larb14_smi",
			"cam_m_larb14"/* parent */),
	GATE_CAM_M(CLK_CAM_M_CAM, "cam_m_cam",
			"top_cam_ck"/* parent */, 6),
	GATE_CAM_M_V(CLK_CAM_M_CAM_CAM, "cam_m_cam_cam",
			"cam_m_cam"/* parent */),
	GATE_CAM_M(CLK_CAM_M_CAMTG, "cam_m_camtg",
			"top_cam_ck"/* parent */, 7),
	GATE_CAM_M_V(CLK_CAM_M_CAMTG_CAM, "cam_m_camtg_cam",
			"cam_m_camtg"/* parent */),
	GATE_CAM_M(CLK_CAM_M_SENINF, "cam_m_seninf",
			"top_cam_ck"/* parent */, 8),
	GATE_CAM_M_V(CLK_CAM_M_SENINF_CAM, "cam_m_seninf_cam",
			"cam_m_seninf"/* parent */),
	GATE_CAM_M(CLK_CAM_M_CAMSV1, "cam_m_camsv1",
			"top_cam_ck"/* parent */, 10),
	GATE_CAM_M_V(CLK_CAM_M_CAMSV1_CAM, "cam_m_camsv1_cam",
			"cam_m_camsv1"/* parent */),
	GATE_CAM_M(CLK_CAM_M_CAMSV2, "cam_m_camsv2",
			"top_cam_ck"/* parent */, 11),
	GATE_CAM_M_V(CLK_CAM_M_CAMSV2_CAM, "cam_m_camsv2_cam",
			"cam_m_camsv2"/* parent */),
	GATE_CAM_M(CLK_CAM_M_CAMSV3, "cam_m_camsv3",
			"top_cam_ck"/* parent */, 12),
	GATE_CAM_M_V(CLK_CAM_M_CAMSV3_CAM, "cam_m_camsv3_cam",
			"cam_m_camsv3"/* parent */),
	GATE_CAM_M(CLK_CAM_M_CAM2MM_GALS, "cam_m_cam2mm_gals",
			"top_cam_ck"/* parent */, 19),
	GATE_CAM_M_V(CLK_CAM_M_CAM2MM_GALS_CAM, "cam_m_cam2mm_gals_cam",
			"cam_m_cam2mm_gals"/* parent */),
	GATE_CAM_M_V(CLK_CAM_M_CAM2MM_GALS_GENPD, "cam_m_cam2mm_gals_genpd",
			"cam_m_cam2mm_gals"/* parent */),
	GATE_CAM_M(CLK_CAM_M_PDA, "cam_m_pda",
			"top_cam_ck"/* parent */, 21),
	GATE_CAM_M_V(CLK_CAM_M_PDA_PDA, "cam_m_pda_pda",
			"cam_m_pda"/* parent */),
};

static const struct mtk_clk_desc cam_m_mcd = {
	.clks = cam_m_clks,
	.num_clks = CLK_CAM_M_NR_CLK,
};

static const struct mtk_gate_regs cam_ra_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_RA(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_ra_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_RA_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate cam_ra_clks[] = {
	GATE_CAM_RA(CLK_CAM_RA_LARBX, "cam_ra_larbx",
			"top_cam_ck"/* parent */, 0),
	GATE_CAM_RA_V(CLK_CAM_RA_LARBX_CAM, "cam_ra_larbx_cam",
			"cam_ra_larbx"/* parent */),
	GATE_CAM_RA_V(CLK_CAM_RA_LARBX_SMI, "cam_ra_larbx_smi",
			"cam_ra_larbx"/* parent */),
	GATE_CAM_RA(CLK_CAM_RA_CAM, "cam_ra_cam",
			"top_cam_ck"/* parent */, 1),
	GATE_CAM_RA_V(CLK_CAM_RA_CAM_CAM, "cam_ra_cam_cam",
			"cam_ra_cam"/* parent */),
	GATE_CAM_RA(CLK_CAM_RA_CAMTG, "cam_ra_camtg",
			"top_cam_ck"/* parent */, 2),
	GATE_CAM_RA_V(CLK_CAM_RA_CAMTG_CAM, "cam_ra_camtg_cam",
			"cam_ra_camtg"/* parent */),
};

static const struct mtk_clk_desc cam_ra_mcd = {
	.clks = cam_ra_clks,
	.num_clks = CLK_CAM_RA_NR_CLK,
};

static const struct mtk_gate_regs cam_rb_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_RB(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_rb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_RB_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate cam_rb_clks[] = {
	GATE_CAM_RB(CLK_CAM_RB_LARBX, "cam_rb_larbx",
			"top_cam_ck"/* parent */, 0),
	GATE_CAM_RB_V(CLK_CAM_RB_LARBX_CAM, "cam_rb_larbx_cam",
			"cam_rb_larbx"/* parent */),
	GATE_CAM_RB_V(CLK_CAM_RB_LARBX_SMI, "cam_rb_larbx_smi",
			"cam_rb_larbx"/* parent */),
	GATE_CAM_RB(CLK_CAM_RB_CAM, "cam_rb_cam",
			"top_cam_ck"/* parent */, 1),
	GATE_CAM_RB_V(CLK_CAM_RB_CAM_CAM, "cam_rb_cam_cam",
			"cam_rb_cam"/* parent */),
	GATE_CAM_RB(CLK_CAM_RB_CAMTG, "cam_rb_camtg",
			"top_cam_ck"/* parent */, 2),
	GATE_CAM_RB_V(CLK_CAM_RB_CAMTG_CAM, "cam_rb_camtg_cam",
			"cam_rb_camtg"/* parent */),
};

static const struct mtk_clk_desc cam_rb_mcd = {
	.clks = cam_rb_clks,
	.num_clks = CLK_CAM_RB_NR_CLK,
};

static const struct mtk_gate_regs mm0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mm1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

#define GATE_MM0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM0_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_MM1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM1_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate mm_clks[] = {
	/* MM0 */
	GATE_MM0(CLK_MM_DISP_OVL0_2L, "mm_disp_ovl0_2l",
			"top_disp0_ck"/* parent */, 0),
	GATE_MM0_V(CLK_MM_DISP_OVL0_2L_DISP, "mm_disp_ovl0_2l_disp",
			"mm_disp_ovl0_2l"/* parent */),
	GATE_MM0(CLK_MM_DISP_OVL1_2L, "mm_disp_ovl1_2l",
			"top_disp0_ck"/* parent */, 1),
	GATE_MM0_V(CLK_MM_DISP_OVL1_2L_DISP, "mm_disp_ovl1_2l_disp",
			"mm_disp_ovl1_2l"/* parent */),
	GATE_MM0(CLK_MM_DISP_OVL2_2L, "mm_disp_ovl2_2l",
			"top_disp0_ck"/* parent */, 2),
	GATE_MM0_V(CLK_MM_DISP_OVL2_2L_DISP, "mm_disp_ovl2_2l_disp",
			"mm_disp_ovl2_2l"/* parent */),
	GATE_MM0(CLK_MM_DISP_RSZ1, "mm_disp_rsz1",
			"top_disp0_ck"/* parent */, 5),
	GATE_MM0_V(CLK_MM_DISP_RSZ1_DISP, "mm_disp_rsz1_disp",
			"mm_disp_rsz1"/* parent */),
	GATE_MM0(CLK_MM_DISP_RSZ0, "mm_disp_rsz0",
			"top_disp0_ck"/* parent */, 6),
	GATE_MM0_V(CLK_MM_DISP_RSZ0_DISP, "mm_disp_rsz0_disp",
			"mm_disp_rsz0"/* parent */),
	GATE_MM0(CLK_MM_DISP_TDSHP0, "mm_disp_tdshp0",
			"top_disp0_ck"/* parent */, 7),
	GATE_MM0_V(CLK_MM_DISP_TDSHP0_DISP, "mm_disp_tdshp0_disp",
			"mm_disp_tdshp0"/* parent */),
	GATE_MM0(CLK_MM_DISP_C3D0, "mm_disp_c3d0",
			"top_disp0_ck"/* parent */, 8),
	GATE_MM0_V(CLK_MM_DISP_C3D0_DISP, "mm_disp_c3d0_disp",
			"mm_disp_c3d0"/* parent */),
	GATE_MM0(CLK_MM_DISP_COLOR0, "mm_disp_color0",
			"top_disp0_ck"/* parent */, 9),
	GATE_MM0_V(CLK_MM_DISP_COLOR0_DISP, "mm_disp_color0_disp",
			"mm_disp_color0"/* parent */),
	GATE_MM0(CLK_MM_DISP_CCORR0, "mm_disp_ccorr0",
			"top_disp0_ck"/* parent */, 10),
	GATE_MM0_V(CLK_MM_DISP_CCORR0_DISP, "mm_disp_ccorr0_disp",
			"mm_disp_ccorr0"/* parent */),
	GATE_MM0(CLK_MM_DISP_CCORR1, "mm_disp_ccorr1",
			"top_disp0_ck"/* parent */, 11),
	GATE_MM0_V(CLK_MM_DISP_CCORR1_DISP, "mm_disp_ccorr1_disp",
			"mm_disp_ccorr1"/* parent */),
	GATE_MM0(CLK_MM_DISP_AAL0, "mm_disp_aal0",
			"top_disp0_ck"/* parent */, 12),
	GATE_MM0_V(CLK_MM_DISP_AAL0_DISP, "mm_disp_aal0_disp",
			"mm_disp_aal0"/* parent */),
	GATE_MM0(CLK_MM_DISP_GAMMA0, "mm_disp_gamma0",
			"top_disp0_ck"/* parent */, 13),
	GATE_MM0_V(CLK_MM_DISP_GAMMA0_DISP, "mm_disp_gamma0_disp",
			"mm_disp_gamma0"/* parent */),
	GATE_MM0(CLK_MM_DISP_POSTMASK0, "mm_disp_postmask0",
			"top_disp0_ck"/* parent */, 14),
	GATE_MM0_V(CLK_MM_DISP_POSTMASK0_DISP, "mm_disp_postmask0_disp",
			"mm_disp_postmask0"/* parent */),
	GATE_MM0(CLK_MM_DISP_DITHER0, "mm_disp_dither0",
			"top_disp0_ck"/* parent */, 15),
	GATE_MM0_V(CLK_MM_DISP_DITHER0_DISP, "mm_disp_dither0_disp",
			"mm_disp_dither0"/* parent */),
	GATE_MM0(CLK_MM_DISP_TDSHP1, "mm_disp_tdshp1",
			"top_disp0_ck"/* parent */, 16),
	GATE_MM0_V(CLK_MM_DISP_TDSHP1_DISP, "mm_disp_tdshp1_disp",
			"mm_disp_tdshp1"/* parent */),
	GATE_MM0(CLK_MM_DISP_C3D1, "mm_disp_c3d1",
			"top_disp0_ck"/* parent */, 17),
	GATE_MM0_V(CLK_MM_DISP_C3D1_DISP, "mm_disp_c3d1_disp",
			"mm_disp_c3d1"/* parent */),
	GATE_MM0(CLK_MM_DISP_CCORR2, "mm_disp_ccorr2",
			"top_disp0_ck"/* parent */, 18),
	GATE_MM0_V(CLK_MM_DISP_CCORR2_DISP, "mm_disp_ccorr2_disp",
			"mm_disp_ccorr2"/* parent */),
	GATE_MM0(CLK_MM_DISP_CCORR3, "mm_disp_ccorr3",
			"top_disp0_ck"/* parent */, 19),
	GATE_MM0_V(CLK_MM_DISP_CCORR3_DISP, "mm_disp_ccorr3_disp",
			"mm_disp_ccorr3"/* parent */),
	GATE_MM0(CLK_MM_DISP_GAMMA1, "mm_disp_gamma1",
			"top_disp0_ck"/* parent */, 20),
	GATE_MM0_V(CLK_MM_DISP_GAMMA1_DISP, "mm_disp_gamma1_disp",
			"mm_disp_gamma1"/* parent */),
	GATE_MM0(CLK_MM_DISP_DITHER1, "mm_disp_dither1",
			"top_disp0_ck"/* parent */, 21),
	GATE_MM0_V(CLK_MM_DISP_DITHER1_DISP, "mm_disp_dither1_disp",
			"mm_disp_dither1"/* parent */),
	GATE_MM0(CLK_MM_DISP_DSC_WRAP0, "mm_disp_dsc_wrap0",
			"top_disp0_ck"/* parent */, 23),
	GATE_MM0_V(CLK_MM_DISP_DSC_WRAP0_DISP, "mm_disp_dsc_wrap0_disp",
			"mm_disp_dsc_wrap0"/* parent */),
	GATE_MM0(CLK_MM_DISP_DSI0, "mm_CLK0",
			"top_disp0_ck"/* parent */, 24),
	GATE_MM0_V(CLK_MM_DISP_DSI0_DISP, "mm_clk0_disp",
			"mm_CLK0"/* parent */),
	GATE_MM0(CLK_MM_DISP_WDMA1, "mm_disp_wdma1",
			"top_disp0_ck"/* parent */, 26),
	GATE_MM0_V(CLK_MM_DISP_WDMA1_DISP, "mm_disp_wdma1_disp",
			"mm_disp_wdma1"/* parent */),
	GATE_MM0(CLK_MM_DISP_APB_BUS, "mm_disp_apb_bus",
			"top_disp0_ck"/* parent */, 27),
	GATE_MM0_V(CLK_MM_DISP_APB_BUS_DISP, "mm_disp_apb_bus_disp",
			"mm_disp_apb_bus"/* parent */),
	GATE_MM0(CLK_MM_DISP_FAKE_ENG0, "mm_disp_fake_eng0",
			"top_disp0_ck"/* parent */, 28),
	GATE_MM0_V(CLK_MM_DISP_FAKE_ENG0_DISP, "mm_disp_fake_eng0_disp",
			"mm_disp_fake_eng0"/* parent */),
	GATE_MM0(CLK_MM_DISP_FAKE_ENG1, "mm_disp_fake_eng1",
			"top_disp0_ck"/* parent */, 29),
	GATE_MM0_V(CLK_MM_DISP_FAKE_ENG1_DISP, "mm_disp_fake_eng1_disp",
			"mm_disp_fake_eng1"/* parent */),
	GATE_MM0(CLK_MM_DISP_MUTEX0, "mm_disp_mutex0",
			"top_disp0_ck"/* parent */, 30),
	GATE_MM0_V(CLK_MM_DISP_MUTEX0_DISP, "mm_disp_mutex0_disp",
			"mm_disp_mutex0"/* parent */),
	GATE_MM0(CLK_MM_SMI_COMMON, "mm_smi_common",
			"top_disp0_ck"/* parent */, 31),
	GATE_MM0_V(CLK_MM_SMI_COMMON_DISP, "mm_smi_common_disp",
			"mm_smi_common"/* parent */),
	GATE_MM0_V(CLK_MM_SMI_COMMON_SMI, "mm_smi_common_smi",
			"mm_smi_common"/* parent */),
	GATE_MM0_V(CLK_MM_SMI_COMMON_GENPD, "mm_smi_common_genpd",
			"mm_smi_common"/* parent */),
	/* MM1 */
	GATE_MM1(CLK_MM_DSI0, "mm_dsi0_ck",
			"top_disp0_ck"/* parent */, 0),
	GATE_MM1_V(CLK_MM_DSI0_DISP, "mm_dsi0_ck_disp",
			"mm_dsi0_ck"/* parent */),
	GATE_MM1(CLK_MM_26M, "mm_26m_ck",
			"top_disp0_ck"/* parent */, 11),
	GATE_MM1_V(CLK_MM_26M_DISP, "mm_26m_ck_disp",
			"mm_26m_ck"/* parent */),
};

static const struct mtk_clk_desc mm_mcd = {
	.clks = mm_clks,
	.num_clks = CLK_MM_NR_CLK,
};

static const struct mtk_gate_regs imgsys1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IMGSYS1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imgsys1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMGSYS1_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate imgsys1_clks[] = {
	GATE_IMGSYS1(CLK_IMGSYS1_LARB9, "imgsys1_larb9",
			"top_img1_ck"/* parent */, 0),
	GATE_IMGSYS1_V(CLK_IMGSYS1_LARB9_CAM_DIP, "imgsys1_larb9_cam_dip",
			"imgsys1_larb9"/* parent */),
	GATE_IMGSYS1_V(CLK_IMGSYS1_LARB9_SMI, "imgsys1_larb9_smi",
			"imgsys1_larb9"/* parent */),
	GATE_IMGSYS1(CLK_IMGSYS1_LARB10, "imgsys1_larb10",
			"top_img1_ck"/* parent */, 1),
	GATE_IMGSYS1_V(CLK_IMGSYS1_LARB10_SMI, "imgsys1_larb10_smi",
			"imgsys1_larb10"/* parent */),
	GATE_IMGSYS1(CLK_IMGSYS1_DIP, "imgsys1_dip",
			"top_img1_ck"/* parent */, 2),
	GATE_IMGSYS1_V(CLK_IMGSYS1_DIP_CAM_DIP, "imgsys1_dip_cam_dip",
			"imgsys1_dip"/* parent */),
};

static const struct mtk_clk_desc imgsys1_mcd = {
	.clks = imgsys1_clks,
	.num_clks = CLK_IMGSYS1_NR_CLK,
};

static const struct mtk_gate_regs imgsys2_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IMGSYS2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imgsys2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMGSYS2_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate imgsys2_clks[] = {
	GATE_IMGSYS2(CLK_IMGSYS2_LARB9, "imgsys2_larb9",
			"top_img1_ck"/* parent */, 0),
	GATE_IMGSYS2_V(CLK_IMGSYS2_LARB9_CAM_MSF, "imgsys2_larb9_cam_msf",
			"imgsys2_larb9"/* parent */),
	GATE_IMGSYS2_V(CLK_IMGSYS2_LARB9_CAM_WPE, "imgsys2_larb9_cam_wpe",
			"imgsys2_larb9"/* parent */),
	GATE_IMGSYS2_V(CLK_IMGSYS2_LARB9_CAM_MSS, "imgsys2_larb9_cam_mss",
			"imgsys2_larb9"/* parent */),
	GATE_IMGSYS2_V(CLK_IMGSYS2_LARB9_SMI, "imgsys2_larb9_smi",
			"imgsys2_larb9"/* parent */),
	GATE_IMGSYS2(CLK_IMGSYS2_MFB, "imgsys2_mfb",
			"top_img1_ck"/* parent */, 6),
	GATE_IMGSYS2_V(CLK_IMGSYS2_MFB_CAM_MSF, "imgsys2_mfb_cam_msf",
			"imgsys2_mfb"/* parent */),
	GATE_IMGSYS2(CLK_IMGSYS2_WPE, "imgsys2_wpe",
			"top_img1_ck"/* parent */, 7),
	GATE_IMGSYS2_V(CLK_IMGSYS2_WPE_CAM_WPE, "imgsys2_wpe_cam_wpe",
			"imgsys2_wpe"/* parent */),
	GATE_IMGSYS2(CLK_IMGSYS2_MSS, "imgsys2_mss",
			"top_img1_ck"/* parent */, 8),
	GATE_IMGSYS2_V(CLK_IMGSYS2_MSS_CAM_MSS, "imgsys2_mss_cam_mss",
			"imgsys2_mss"/* parent */),
};

static const struct mtk_clk_desc imgsys2_mcd = {
	.clks = imgsys2_clks,
	.num_clks = CLK_IMGSYS2_NR_CLK,
};

static const struct mtk_gate_regs impc_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMPC_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate impc_clks[] = {
	GATE_IMPC(CLK_IMPC_I2C1, "impc_i2c1",
			"top_i2c_ck"/* parent */, 0),
	GATE_IMPC_V(CLK_IMPC_I2C1_I2C, "impc_i2c1_i2c",
			"impc_i2c1"/* parent */),
	GATE_IMPC(CLK_IMPC_I2C3, "impc_i2c3",
			"top_i2c_ck"/* parent */, 1),
	GATE_IMPC_V(CLK_IMPC_I2C3_I2C, "impc_i2c3_i2c",
			"impc_i2c3"/* parent */),
	GATE_IMPC(CLK_IMPC_I2C5, "impc_i2c5",
			"top_i2c_ck"/* parent */, 2),
	GATE_IMPC_V(CLK_IMPC_I2C5_I2C, "impc_i2c5_i2c",
			"impc_i2c5"/* parent */),
	GATE_IMPC(CLK_IMPC_I2C6, "impc_i2c6",
			"top_i2c_ck"/* parent */, 3),
	GATE_IMPC_V(CLK_IMPC_I2C6_I2C, "impc_i2c6_i2c",
			"impc_i2c6"/* parent */),
	GATE_IMPC(CLK_IMPC_I2C10, "impc_i2c10",
			"top_i2c_ck"/* parent */, 4),
	GATE_IMPC_V(CLK_IMPC_I2C10_I2C, "impc_i2c10_i2c",
			"impc_i2c10"/* parent */),
	GATE_IMPC(CLK_IMPC_I2C12, "impc_i2c12",
			"top_i2c_ck"/* parent */, 5),
	GATE_IMPC_V(CLK_IMPC_I2C12_I2C, "impc_i2c12_i2c",
			"impc_i2c12"/* parent */),
};

static const struct mtk_clk_desc impc_mcd = {
	.clks = impc_clks,
	.num_clks = CLK_IMPC_NR_CLK,
};

static const struct mtk_gate_regs impes_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPES(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impes_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMPES_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate impes_clks[] = {
	GATE_IMPES(CLK_IMPES_I2C4, "impes_i2c4",
			"top_i2c_ck"/* parent */, 0),
	GATE_IMPES_V(CLK_IMPES_I2C4_I2C, "impes_i2c4_i2c",
			"impes_i2c4"/* parent */),
	GATE_IMPES(CLK_IMPES_I2C7, "impes_i2c7",
			"top_i2c_ck"/* parent */, 1),
	GATE_IMPES_V(CLK_IMPES_I2C7_I2C, "impes_i2c7_i2c",
			"impes_i2c7"/* parent */),
	GATE_IMPES(CLK_IMPES_I2C8, "impes_i2c8",
			"top_i2c_ck"/* parent */, 2),
	GATE_IMPES_V(CLK_IMPES_I2C8_I2C, "impes_i2c8_i2c",
			"impes_i2c8"/* parent */),
};

static const struct mtk_clk_desc impes_mcd = {
	.clks = impes_clks,
	.num_clks = CLK_IMPES_NR_CLK,
};

static const struct mtk_gate_regs imps_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPS(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imps_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMPS_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate imps_clks[] = {
	GATE_IMPS(CLK_IMPS_I2C0, "imps_i2c0",
			"top_i2c_ck"/* parent */, 0),
	GATE_IMPS_V(CLK_IMPS_I2C0_I2C, "imps_i2c0_i2c",
			"imps_i2c0"/* parent */),
	GATE_IMPS(CLK_IMPS_I2C2, "imps_i2c2",
			"top_i2c_ck"/* parent */, 1),
	GATE_IMPS_V(CLK_IMPS_I2C2_I2C, "imps_i2c2_i2c",
			"imps_i2c2"/* parent */),
	GATE_IMPS(CLK_IMPS_I2C9, "imps_i2c9",
			"top_i2c_ck"/* parent */, 2),
	GATE_IMPS_V(CLK_IMPS_I2C9_I2C, "imps_i2c9_i2c",
			"imps_i2c9"/* parent */),
	GATE_IMPS(CLK_IMPS_I2C11, "imps_i2c11",
			"top_i2c_ck"/* parent */, 3),
	GATE_IMPS_V(CLK_IMPS_I2C11_I2C, "imps_i2c11_i2c",
			"imps_i2c11"/* parent */),
};

static const struct mtk_clk_desc imps_mcd = {
	.clks = imps_clks,
	.num_clks = CLK_IMPS_NR_CLK,
};

static const struct mtk_gate_regs infra_infracfg_ao_reg0_cg_regs = {
	.set_ofs = 0x88,
	.clr_ofs = 0x8C,
	.sta_ofs = 0x94,
};

static const struct mtk_gate_regs infra_infracfg_ao_reg1_cg_regs = {
	.set_ofs = 0xA4,
	.clr_ofs = 0xA8,
	.sta_ofs = 0xAC,
};

static const struct mtk_gate_regs infra_infracfg_ao_reg2_cg_regs = {
	.set_ofs = 0xC0,
	.clr_ofs = 0xC4,
	.sta_ofs = 0xC8,
};

static const struct mtk_gate_regs infra_infracfg_ao_reg3_cg_regs = {
	.set_ofs = 0xE0,
	.clr_ofs = 0xE4,
	.sta_ofs = 0xE8,
};

#define GATE_INFRA_INFRACFG_AO_REG0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra_infracfg_ao_reg0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_INFRA_INFRACFG_AO_REG0_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_INFRA_INFRACFG_AO_REG1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra_infracfg_ao_reg1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_INFRA_INFRACFG_AO_REG1_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_INFRA_INFRACFG_AO_REG2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra_infracfg_ao_reg2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_INFRA_INFRACFG_AO_REG2_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_INFRA_INFRACFG_AO_REG3(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra_infracfg_ao_reg3_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_INFRA_INFRACFG_AO_REG3_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate infra_infracfg_ao_reg_clks[] = {
	/* INFRA_INFRACFG_AO_REG0 */
	GATE_INFRA_INFRACFG_AO_REG0(CLK_INFRACFG_AO_REG_CCIF1_AP, "infracfg_ao_ccif1_ap",
			"top_axi_ck"/* parent */, 12),
	GATE_INFRA_INFRACFG_AO_REG0_V(CLK_INFRACFG_AO_REG_CCIF1_AP_CCCI, "infracfg_ao_ccif1_ap_ccci",
			"infracfg_ao_ccif1_ap"/* parent */),
	GATE_INFRA_INFRACFG_AO_REG0(CLK_INFRACFG_AO_REG_CCIF1_MD, "infracfg_ao_ccif1_md",
			"top_axi_ck"/* parent */, 13),
	GATE_INFRA_INFRACFG_AO_REG0_V(CLK_INFRACFG_AO_REG_CCIF1_MD_CCCI, "infracfg_ao_ccif1_md_ccci",
			"infracfg_ao_ccif1_md"/* parent */),
	GATE_INFRA_INFRACFG_AO_REG0(CLK_INFRACFG_AO_REG_CCIF_AP, "infracfg_ao_ccif_ap",
			"top_axi_ck"/* parent */, 23),
	GATE_INFRA_INFRACFG_AO_REG0_V(CLK_INFRACFG_AO_REG_CCIF_AP_CCCI, "infracfg_ao_ccif_ap_ccci",
			"infracfg_ao_ccif_ap"/* parent */),
	GATE_INFRA_INFRACFG_AO_REG0(CLK_INFRACFG_AO_REG_CCIF_MD, "infracfg_ao_ccif_md",
			"top_axi_ck"/* parent */, 26),
	GATE_INFRA_INFRACFG_AO_REG0_V(CLK_INFRACFG_AO_REG_CCIF_MD_CCCI, "infracfg_ao_ccif_md_ccci",
			"infracfg_ao_ccif_md"/* parent */),
	/* INFRA_INFRACFG_AO_REG1 */
	GATE_INFRA_INFRACFG_AO_REG1(CLK_INFRACFG_AO_REG_CLDMA_BCLK, "infracfg_ao_cldmabclk",
			"top_axi_ck"/* parent */, 3),
	GATE_INFRA_INFRACFG_AO_REG1_V(CLK_INFRACFG_AO_REG_CLDMA_BCLK_DPMAIF, "infracfg_ao_cldmabclk_dpmaif",
			"infracfg_ao_cldmabclk"/* parent */),
	/* INFRA_INFRACFG_AO_REG2 */
	GATE_INFRA_INFRACFG_AO_REG2(CLK_INFRACFG_AO_REG_CCIF5_MD, "infracfg_ao_ccif5_md",
			"top_axi_ck"/* parent */, 10),
	GATE_INFRA_INFRACFG_AO_REG2_V(CLK_INFRACFG_AO_REG_CCIF5_MD_CCCI, "infracfg_ao_ccif5_md_ccci",
			"infracfg_ao_ccif5_md"/* parent */),
	GATE_INFRA_INFRACFG_AO_REG2(CLK_INFRACFG_AO_REG_CCIF2_AP, "infracfg_ao_ccif2_ap",
			"top_axi_ck"/* parent */, 16),
	GATE_INFRA_INFRACFG_AO_REG2_V(CLK_INFRACFG_AO_REG_CCIF2_AP_CCCI, "infracfg_ao_ccif2_ap_ccci",
			"infracfg_ao_ccif2_ap"/* parent */),
	GATE_INFRA_INFRACFG_AO_REG2(CLK_INFRACFG_AO_REG_CCIF2_MD, "infracfg_ao_ccif2_md",
			"top_axi_ck"/* parent */, 17),
	GATE_INFRA_INFRACFG_AO_REG2_V(CLK_INFRACFG_AO_REG_CCIF2_MD_CCCI, "infracfg_ao_ccif2_md_ccci",
			"infracfg_ao_ccif2_md"/* parent */),
	GATE_INFRA_INFRACFG_AO_REG2(CLK_INFRACFG_AO_REG_DPMAIF_MAIN, "infracfg_ao_dpmaif_main",
			"top_dpmaif_main_ck"/* parent */, 26),
	GATE_INFRA_INFRACFG_AO_REG2_V(CLK_INFRACFG_AO_REG_DPMAIF_MAIN_DPMAIF, "infracfg_ao_dpmaif_main_dpmaif",
			"infracfg_ao_dpmaif_main"/* parent */),
	GATE_INFRA_INFRACFG_AO_REG2(CLK_INFRACFG_AO_REG_CCIF4_MD, "infracfg_ao_ccif4_md",
			"top_axi_ck"/* parent */, 29),
	GATE_INFRA_INFRACFG_AO_REG2_V(CLK_INFRACFG_AO_REG_CCIF4_MD_CCCI, "infracfg_ao_ccif4_md_ccci",
			"infracfg_ao_ccif4_md"/* parent */),
	/* INFRA_INFRACFG_AO_REG3 */
	GATE_INFRA_INFRACFG_AO_REG3(CLK_INFRACFG_AO_REG_RG_MMW_DPMAIF26M, "infracfg_ao_dpmaif_26m",
			"top_f26m_ck"/* parent */, 17),
	GATE_INFRA_INFRACFG_AO_REG3_V(CLK_INFRACFG_AO_REG_RG_MMW_DPMAIF26M_DPMAIF, "infracfg_ao_dpmaif_26m_dpmaif",
			"infracfg_ao_dpmaif_26m"/* parent */),
};

static const struct mtk_clk_desc infra_infracfg_ao_reg_mcd = {
	.clks = infra_infracfg_ao_reg_clks,
	.num_clks = CLK_INFRA_INFRACFG_AO_REG_NR_CLK,
};

static const struct mtk_gate_regs ipe_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IPE(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ipe_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IPE_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate ipe_clks[] = {
	GATE_IPE(CLK_IPE_LARB20, "ipe_larb20",
			"top_ipe_ck"/* parent */, 1),
	GATE_IPE_V(CLK_IPE_LARB20_CAM_FD, "ipe_larb20_cam_fd",
			"ipe_larb20"/* parent */),
	GATE_IPE_V(CLK_IPE_LARB20_CAM_RSC, "ipe_larb20_cam_rsc",
			"ipe_larb20"/* parent */),
	GATE_IPE_V(CLK_IPE_LARB20_SMI, "ipe_larb20_smi",
			"ipe_larb20"/* parent */),
	GATE_IPE(CLK_IPE_SMI_SUBCOM, "ipe_smi_subcom",
			"top_ipe_ck"/* parent */, 2),
	GATE_IPE_V(CLK_IPE_SMI_SUBCOM_SMI, "ipe_smi_subcom_smi",
			"ipe_smi_subcom"/* parent */),
	GATE_IPE(CLK_IPE_FD, "ipe_fd",
			"top_ipe_ck"/* parent */, 3),
	GATE_IPE_V(CLK_IPE_FD_CAM_FD, "ipe_fd_cam_fd",
			"ipe_fd"/* parent */),
	GATE_IPE(CLK_IPE_RSC, "ipe_rsc",
			"top_ipe_ck"/* parent */, 5),
	GATE_IPE_V(CLK_IPE_RSC_CAM_RSC, "ipe_rsc_cam_rsc",
			"ipe_rsc"/* parent */),
};

static const struct mtk_clk_desc ipe_mcd = {
	.clks = ipe_clks,
	.num_clks = CLK_IPE_NR_CLK,
};

static const struct mtk_gate_regs mdp0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mdp1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

#define GATE_MDP0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mdp0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MDP0_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_MDP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mdp1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MDP1_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate mdp_clks[] = {
	/* MDP0 */
	GATE_MDP0(CLK_MDP_MUTEX0, "mdp_mutex0",
			"top_disp0_ck"/* parent */, 0),
	GATE_MDP0_V(CLK_MDP_MUTEX0_MDP, "mdp_mutex0_mdp",
			"mdp_mutex0"/* parent */),
	GATE_MDP0(CLK_MDP_APB_BUS, "mdp_apb_bus",
			"top_disp0_ck"/* parent */, 1),
	GATE_MDP0_V(CLK_MDP_APB_BUS_MDP, "mdp_apb_bus_mdp",
			"mdp_apb_bus"/* parent */),
	GATE_MDP0(CLK_MDP_SMI0, "mdp_smi0",
			"top_disp0_ck"/* parent */, 2),
	GATE_MDP0_V(CLK_MDP_SMI0_MDP, "mdp_smi0_mdp",
			"mdp_smi0"/* parent */),
	GATE_MDP0_V(CLK_MDP_SMI0_SMI, "mdp_smi0_smi",
			"mdp_smi0"/* parent */),
	GATE_MDP0_V(CLK_MDP_SMI0_GENPD, "mdp_smi0_genpd",
			"mdp_smi0"/* parent */),
	GATE_MDP0(CLK_MDP_RDMA0, "mdp_rdma0",
			"top_disp0_ck"/* parent */, 3),
	GATE_MDP0_V(CLK_MDP_RDMA0_MDP, "mdp_rdma0_mdp",
			"mdp_rdma0"/* parent */),
	GATE_MDP0(CLK_MDP_FG0, "mdp_fg0",
			"top_disp0_ck"/* parent */, 4),
	GATE_MDP0_V(CLK_MDP_FG0_MDP, "mdp_fg0_mdp",
			"mdp_fg0"/* parent */),
	GATE_MDP0(CLK_MDP_HDR0, "mdp_hdr0",
			"top_disp0_ck"/* parent */, 5),
	GATE_MDP0_V(CLK_MDP_HDR0_MDP, "mdp_hdr0_mdp",
			"mdp_hdr0"/* parent */),
	GATE_MDP0(CLK_MDP_AAL0, "mdp_aal0",
			"top_disp0_ck"/* parent */, 6),
	GATE_MDP0_V(CLK_MDP_AAL0_MDP, "mdp_aal0_mdp",
			"mdp_aal0"/* parent */),
	GATE_MDP0(CLK_MDP_RSZ0, "mdp_rsz0",
			"top_disp0_ck"/* parent */, 7),
	GATE_MDP0_V(CLK_MDP_RSZ0_MDP, "mdp_rsz0_mdp",
			"mdp_rsz0"/* parent */),
	GATE_MDP0(CLK_MDP_TDSHP0, "mdp_tdshp0",
			"top_disp0_ck"/* parent */, 8),
	GATE_MDP0_V(CLK_MDP_TDSHP0_MDP, "mdp_tdshp0_mdp",
			"mdp_tdshp0"/* parent */),
	GATE_MDP0(CLK_MDP_COLOR0, "mdp_color0",
			"top_disp0_ck"/* parent */, 9),
	GATE_MDP0_V(CLK_MDP_COLOR0_MDP, "mdp_color0_mdp",
			"mdp_color0"/* parent */),
	GATE_MDP0(CLK_MDP_WROT0, "mdp_wrot0",
			"top_disp0_ck"/* parent */, 10),
	GATE_MDP0_V(CLK_MDP_WROT0_MDP, "mdp_wrot0_mdp",
			"mdp_wrot0"/* parent */),
	GATE_MDP0(CLK_MDP_FAKE_ENG0, "mdp_fake_eng0",
			"top_disp0_ck"/* parent */, 11),
	GATE_MDP0_V(CLK_MDP_FAKE_ENG0_MDP, "mdp_fake_eng0_mdp",
			"mdp_fake_eng0"/* parent */),
	GATE_MDP0(CLK_MDP_DLI_ASYNC0, "mdp_dli_async0",
			"top_disp0_ck"/* parent */, 12),
	GATE_MDP0_V(CLK_MDP_DLI_ASYNC0_MDP, "mdp_dli_async0_mdp",
			"mdp_dli_async0"/* parent */),
	GATE_MDP0(CLK_MDP_RSZ2, "mdp_rsz2",
			"top_disp0_ck"/* parent */, 24),
	GATE_MDP0_V(CLK_MDP_RSZ2_MDP, "mdp_rsz2_mdp",
			"mdp_rsz2"/* parent */),
	GATE_MDP0(CLK_MDP_WROT2, "mdp_wrot2",
			"top_disp0_ck"/* parent */, 25),
	GATE_MDP0_V(CLK_MDP_WROT2_MDP, "mdp_wrot2_mdp",
			"mdp_wrot2"/* parent */),
	GATE_MDP0(CLK_MDP_DLO_ASYNC0, "mdp_dlo_async0",
			"top_disp0_ck"/* parent */, 26),
	GATE_MDP0_V(CLK_MDP_DLO_ASYNC0_MDP, "mdp_dlo_async0_mdp",
			"mdp_dlo_async0"/* parent */),
	/* MDP1 */
	GATE_MDP1(CLK_MDP_FMM_IMG_DL_ASYNC0, "mdp_fmm_img_dl_async0",
			"top_disp0_ck"/* parent */, 0),
	GATE_MDP1_V(CLK_MDP_FMM_IMG_DL_ASYNC0_MDP, "mdp_fmm_img_dl_async0_mdp",
			"mdp_fmm_img_dl_async0"/* parent */),
	GATE_MDP1(CLK_MDP_FMM_IMG_DL_ASYNC1, "mdp_fmm_img_dl_async1",
			"top_disp0_ck"/* parent */, 1),
	GATE_MDP1_V(CLK_MDP_FMM_IMG_DL_ASYNC1_MDP, "mdp_fmm_img_dl_async1_mdp",
			"mdp_fmm_img_dl_async1"/* parent */),
	GATE_MDP1(CLK_MDP_FIMG_IMG_DL_ASYNC0, "mdp_fimg_img_dl_async0",
			"top_disp0_ck"/* parent */, 2),
	GATE_MDP1_V(CLK_MDP_FIMG_IMG_DL_ASYNC0_MDP, "mdp_fimg_img_dl_async0_mdp",
			"mdp_fimg_img_dl_async0"/* parent */),
	GATE_MDP1(CLK_MDP_FIMG_IMG_DL_ASYNC1, "mdp_fimg_img_dl_async1",
			"top_disp0_ck"/* parent */, 3),
	GATE_MDP1_V(CLK_MDP_FIMG_IMG_DL_ASYNC1_MDP, "mdp_fimg_img_dl_async1_mdp",
			"mdp_fimg_img_dl_async1"/* parent */),
};

static const struct mtk_clk_desc mdp_mcd = {
	.clks = mdp_clks,
	.num_clks = CLK_MDP_NR_CLK,
};



static const struct mtk_gate_regs perao0_cg_regs = {
	.set_ofs = 0x24,
	.clr_ofs = 0x28,
	.sta_ofs = 0x10,
};

static const struct mtk_gate_regs perao1_cg_regs = {
	.set_ofs = 0x2C,
	.clr_ofs = 0x30,
	.sta_ofs = 0x14,
};

static const struct mtk_gate_regs perao2_cg_regs = {
	.set_ofs = 0x34,
	.clr_ofs = 0x38,
	.sta_ofs = 0x18,
};

#define GATE_PERAO0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PERAO0_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_PERAO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PERAO1_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_PERAO2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PERAO2_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate perao_clks[] = {
	/* PERAO0 */
	GATE_PERAO0(CLK_PERAO_P_UART0, "perao_p_uart0",
			"top_uart_ck"/* parent */, 0),
	GATE_PERAO0_V(CLK_PERAO_P_UART0_UART, "perao_p_uart0_uart",
			"perao_p_uart0"/* parent */),
	GATE_PERAO0(CLK_PERAO_P_UART1, "perao_p_uart1",
			"top_uart_ck"/* parent */, 1),
	GATE_PERAO0_V(CLK_PERAO_P_UART1_UART, "perao_p_uart1_uart",
			"perao_p_uart1"/* parent */),
	GATE_PERAO0(CLK_PERAO_P_UART2, "perao_p_uart2",
			"top_uart_ck"/* parent */, 2),
	GATE_PERAO0_V(CLK_PERAO_P_UART2_UART, "perao_p_uart2_uart",
			"perao_p_uart2"/* parent */),
	GATE_PERAO0(CLK_PERAO_P_UART3, "perao_p_uart3",
			"top_uart_ck"/* parent */, 3),
	GATE_PERAO0_V(CLK_PERAO_P_UART3_UART, "perao_p_uart3_uart",
			"perao_p_uart3"/* parent */),
	GATE_PERAO0(CLK_PERAO_P_PWM_H, "perao_p_pwm_h",
			"top_axi_p_ck"/* parent */, 4),
	GATE_PERAO0_V(CLK_PERAO_P_PWM_H_PWM, "perao_p_pwm_h_pwm",
			"perao_p_pwm_h"/* parent */),
	GATE_PERAO0(CLK_PERAO_P_PWM_B, "perao_p_pwm_b",
			"top_pwm_ck"/* parent */, 5),
	GATE_PERAO0_V(CLK_PERAO_P_PWM_B_PWM, "perao_p_pwm_b_pwm",
			"perao_p_pwm_b"/* parent */),
	GATE_PERAO0(CLK_PERAO_P_PWM_FB1, "perao_p_pwm_fb1",
			"top_pwm_ck"/* parent */, 6),
	GATE_PERAO0_V(CLK_PERAO_P_PWM_FB1_PWM, "perao_p_pwm_fb1_pwm",
			"perao_p_pwm_fb1"/* parent */),
	GATE_PERAO0(CLK_PERAO_P_PWM_FB2, "perao_p_pwm_fb2",
			"top_pwm_ck"/* parent */, 7),
	GATE_PERAO0_V(CLK_PERAO_P_PWM_FB2_PWM, "perao_p_pwm_fb2_pwm",
			"perao_p_pwm_fb2"/* parent */),
	GATE_PERAO0(CLK_PERAO_P_PWM_FB3, "perao_p_pwm_fb3",
			"top_pwm_ck"/* parent */, 8),
	GATE_PERAO0_V(CLK_PERAO_P_PWM_FB3_PWM, "perao_p_pwm_fb3_pwm",
			"perao_p_pwm_fb3"/* parent */),
	GATE_PERAO0(CLK_PERAO_P_PWM_FB4, "perao_p_pwm_fb4",
			"top_pwm_ck"/* parent */, 9),
	GATE_PERAO0_V(CLK_PERAO_P_PWM_FB4_PWM, "perao_p_pwm_fb4_pwm",
			"perao_p_pwm_fb4"/* parent */),
	GATE_PERAO0(CLK_PERAO_P_DISP_PWM0, "perao_p_disp_pwm0",
			"top_disp_pwm_ck"/* parent */, 10),
	GATE_PERAO0_V(CLK_PERAO_P_DISP_PWM0_DISP_PWM_0, "perao_p_disp_pwm0_disp_pwm_0",
			"perao_p_disp_pwm0"/* parent */),
	GATE_PERAO0(CLK_PERAO_P_DISP_PWM1, "perao_p_disp_pwm1",
			"top_disp_pwm_ck"/* parent */, 11),
	GATE_PERAO0_V(CLK_PERAO_P_DISP_PWM1_DISP_PWM_1, "perao_p_disp_pwm1_disp_pwm_1",
			"perao_p_disp_pwm1"/* parent */),
	GATE_PERAO0(CLK_PERAO_P_SPI0_B, "perao_p_spi0_b",
			"top_spi0_ck"/* parent */, 12),
	GATE_PERAO0_V(CLK_PERAO_P_SPI0_B_SPI, "perao_p_spi0_b_spi",
			"perao_p_spi0_b"/* parent */),
	GATE_PERAO0(CLK_PERAO_P_SPI1_B, "perao_p_spi1_b",
			"top_spi1_ck"/* parent */, 13),
	GATE_PERAO0_V(CLK_PERAO_P_SPI1_B_SPI, "perao_p_spi1_b_spi",
			"perao_p_spi1_b"/* parent */),
	GATE_PERAO0(CLK_PERAO_P_SPI2_B, "perao_p_spi2_b",
			"top_spi2_ck"/* parent */, 14),
	GATE_PERAO0_V(CLK_PERAO_P_SPI2_B_SPI, "perao_p_spi2_b_spi",
			"perao_p_spi2_b"/* parent */),
	GATE_PERAO0(CLK_PERAO_P_SPI3_B, "perao_p_spi3_b",
			"top_spi3_ck"/* parent */, 15),
	GATE_PERAO0_V(CLK_PERAO_P_SPI3_B_SPI, "perao_p_spi3_b_spi",
			"perao_p_spi3_b"/* parent */),
	GATE_PERAO0(CLK_PERAO_P_SPI4_B, "perao_p_spi4_b",
			"top_spi4_ck"/* parent */, 16),
	GATE_PERAO0_V(CLK_PERAO_P_SPI4_B_SPI, "perao_p_spi4_b_spi",
			"perao_p_spi4_b"/* parent */),
	GATE_PERAO0(CLK_PERAO_P_SPI5_B, "perao_p_spi5_b",
			"top_spi5_ck"/* parent */, 17),
	GATE_PERAO0_V(CLK_PERAO_P_SPI5_B_SPI, "perao_p_spi5_b_spi",
			"perao_p_spi5_b"/* parent */),
	GATE_PERAO0(CLK_PERAO_P_SPI6_B, "perao_p_spi6_b",
			"top_spi6_ck"/* parent */, 18),
	GATE_PERAO0_V(CLK_PERAO_P_SPI6_B_SPI, "perao_p_spi6_b_spi",
			"perao_p_spi6_b"/* parent */),
	GATE_PERAO0(CLK_PERAO_P_SPI7_B, "perao_p_spi7_b",
			"top_spi7_ck"/* parent */, 19),
	GATE_PERAO0_V(CLK_PERAO_P_SPI7_B_SPI, "perao_p_spi7_b_spi",
			"perao_p_spi7_b"/* parent */),
	GATE_PERAO0(CLK_PERAO_P_I2C, "perao_p_i2c",
			"top_axi_p_ck"/* parent */, 28),
	GATE_PERAO0_V(CLK_PERAO_P_I2C_I2C, "perao_p_i2c_i2c",
			"perao_p_i2c"/* parent */),
	GATE_PERAO0(CLK_PERAO_P_DMA_B, "perao_p_dma_b",
			"top_axi_p_ck"/* parent */, 29),
	GATE_PERAO0_V(CLK_PERAO_P_DMA_B_UART, "perao_p_dma_b_uart",
			"perao_p_dma_b"/* parent */),
	GATE_PERAO0_V(CLK_PERAO_P_DMA_B_I2C, "perao_p_dma_b_i2c",
			"perao_p_dma_b"/* parent */),
	/* PERAO1 */
	GATE_PERAO1(CLK_PERAO_P_MSDC1, "perao_p_msdc1",
			"top_msdc30_1_ck"/* parent */, 12),
	GATE_PERAO1_V(CLK_PERAO_P_MSDC1_MSDC, "perao_p_msdc1_msdc",
			"perao_p_msdc1"/* parent */),
	GATE_PERAO1(CLK_PERAO_P_MSDC1_H, "perao_p_msdc1_h",
			"top_msdc30_1_h_ck"/* parent */, 13),
	GATE_PERAO1_V(CLK_PERAO_P_MSDC1_H_MSDC, "perao_p_msdc1_h_msdc",
			"perao_p_msdc1_h"/* parent */),
	GATE_PERAO1(CLK_PERAO_P_MSDC1_MST_F, "perao_p_msdc1_mst_f",
			"top_axi_p_ck"/* parent */, 14),
	GATE_PERAO1_V(CLK_PERAO_P_MSDC1_MST_F_MSDC, "perao_p_msdc1_mst_f_msdc",
			"perao_p_msdc1_mst_f"/* parent */),
	GATE_PERAO1(CLK_PERAO_P_MSDC1_SLV_H, "perao_p_msdc1_slv_h",
			"top_axi_p_ck"/* parent */, 15),
	GATE_PERAO1_V(CLK_PERAO_P_MSDC1_SLV_H_MSDC, "perao_p_msdc1_slv_h_msdc",
			"perao_p_msdc1_slv_h"/* parent */),
	/* PERAO2 */
	GATE_PERAO2(CLK_PERAO_P_AUDIO0, "perao_p_audio0",
			"top_axi_p_ck"/* parent */, 0),
	GATE_PERAO2_V(CLK_PERAO_P_AUDIO0_AUDIO, "perao_p_audio0_audio",
			"perao_p_audio0"/* parent */),
	GATE_PERAO2(CLK_PERAO_P_AUDIO1, "perao_p_audio1",
			"top_axi_p_ck"/* parent */, 1),
	GATE_PERAO2_V(CLK_PERAO_P_AUDIO1_AUDIO, "perao_p_audio1_audio",
			"perao_p_audio1"/* parent */),
	GATE_PERAO2(CLK_PERAO_P_AUDIO2, "perao_p_audio2",
			"top_aud_intbus_ck"/* parent */, 2),
	GATE_PERAO2_V(CLK_PERAO_P_AUDIO2_AUDIO, "perao_p_audio2_audio",
			"perao_p_audio2"/* parent */),
};

static const struct mtk_clk_desc perao_mcd = {
	.clks = perao_clks,
	.num_clks = CLK_PERAO_NR_CLK,
};

static const struct mtk_gate_regs ufsao_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x4,
};

#define GATE_UFSAO(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufsao_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_UFSAO_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate ufsao_clks[] = {
	GATE_UFSAO(CLK_UFSAO_UNIPRO_TX_SYM, "ufsao_unipro_tx_sym",
			"top_f26m_ck"/* parent */, 0),
	GATE_UFSAO_V(CLK_UFSAO_UNIPRO_TX_SYM_UFS, "ufsao_unipro_tx_sym_ufs",
			"ufsao_unipro_tx_sym"/* parent */),
	GATE_UFSAO(CLK_UFSAO_UNIPRO_SYS, "ufsao_unipro_sys",
			"top_ufs_ck"/* parent */, 3),
	GATE_UFSAO_V(CLK_UFSAO_UNIPRO_SYS_UFS, "ufsao_unipro_sys_ufs",
			"ufsao_unipro_sys"/* parent */),
	GATE_UFSAO(CLK_UFSAO_UNIPRO_SAP_CFG, "ufsao_unipro_sap_cfg",
			"top_f26m_ck"/* parent */, 8),
	GATE_UFSAO_V(CLK_UFSAO_UNIPRO_SAP_CFG_UFS, "ufsao_unipro_sap_cfg_ufs",
			"ufsao_unipro_sap_cfg"/* parent */),
	GATE_UFSAO(CLK_UFSAO_UFS_AO_FREE_26M, "ufsao_ufs_ao_26m",
			"top_f26m_ck"/* parent */, 24),
	GATE_UFSAO_V(CLK_UFSAO_UFS_AO_FREE_26M_UFS, "ufsao_ufs_ao_26m_ufs",
			"ufsao_ufs_ao_26m"/* parent */),
};

static const struct mtk_clk_desc ufsao_mcd = {
	.clks = ufsao_clks,
	.num_clks = CLK_UFSAO_NR_CLK,
};

static const struct mtk_gate_regs ufspdn_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x4,
};

#define GATE_UFSPDN(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufspdn_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_UFSPDN_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate ufspdn_clks[] = {
	GATE_UFSPDN(CLK_UFSPDN_UFSHCI_UFS, "ufspdn_ufshci_ufs",
			"top_ufs_ck"/* parent */, 0),
	GATE_UFSPDN_V(CLK_UFSPDN_UFSHCI_UFS_UFS, "ufspdn_ufshci_ufs_ufs",
			"ufspdn_ufshci_ufs"/* parent */),
	GATE_UFSPDN(CLK_UFSPDN_UFSHCI_AES, "ufspdn_ufshci_aes",
			"top_aes_ufsfde_ck"/* parent */, 1),
	GATE_UFSPDN_V(CLK_UFSPDN_UFSHCI_AES_UFS, "ufspdn_ufshci_aes_ufs",
			"ufspdn_ufshci_aes"/* parent */),
	GATE_UFSPDN(CLK_UFSPDN_UFSHCI_UFS_AHB, "ufspdn_ufshci_ufs_ahb",
			"top_axi_ufs_ck"/* parent */, 3),
	GATE_UFSPDN_V(CLK_UFSPDN_UFSHCI_UFS_AHB_UFS, "ufspdn_ufshci_ufs_ahb_ufs",
			"ufspdn_ufshci_ufs_ahb"/* parent */),
	GATE_UFSPDN(CLK_UFSPDN_UFS_26M, "ufspdn_ufs_26m_ck",
			"top_f26m_ck"/* parent */, 6),
	GATE_UFSPDN_V(CLK_UFSPDN_UFS_26M_UFS, "ufspdn_ufs_26m_ck_ufs",
			"ufspdn_ufs_26m_ck"/* parent */),
};

static const struct mtk_clk_desc ufspdn_mcd = {
	.clks = ufspdn_clks,
	.num_clks = CLK_UFSPDN_NR_CLK,
};

static const struct mtk_gate_regs vde20_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vde21_cg_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x204,
	.sta_ofs = 0x200,
};

#define GATE_VDE20(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde20_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE20_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

#define GATE_VDE21(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde21_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE21_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate vde2_clks[] = {
	/* VDE20 */
	GATE_VDE20(CLK_VDE2_VDEC_CKEN, "vde2_vdec_cken",
			"top_vdec_ck"/* parent */, 0),
	GATE_VDE20_V(CLK_VDE2_VDEC_CKEN_VDEC, "vde2_vdec_cken_vdec",
			"vde2_vdec_cken"/* parent */),
	GATE_VDE20(CLK_VDE2_VDEC_ACTIVE, "vde2_vdec_active",
			"top_vdec_ck"/* parent */, 4),
	GATE_VDE20_V(CLK_VDE2_VDEC_ACTIVE_VDEC, "vde2_vdec_active_vdec",
			"vde2_vdec_active"/* parent */),
	GATE_VDE20(CLK_VDE2_VDEC_CKEN_ENG, "vde2_vdec_cken_eng",
			"top_vdec_ck"/* parent */, 8),
	GATE_VDE20_V(CLK_VDE2_VDEC_CKEN_ENG_VDEC, "vde2_vdec_cken_eng_vdec",
			"vde2_vdec_cken_eng"/* parent */),
	/* VDE21 */
	GATE_VDE21(CLK_VDE2_LAT_CKEN, "vde2_lat_cken",
			"top_vdec_ck"/* parent */, 0),
	GATE_VDE21_V(CLK_VDE2_LAT_CKEN_VDEC, "vde2_lat_cken_vdec",
			"vde2_lat_cken"/* parent */),
	GATE_VDE21(CLK_VDE2_LAT_ACTIVE, "vde2_lat_active",
			"top_vdec_ck"/* parent */, 4),
	GATE_VDE21_V(CLK_VDE2_LAT_ACTIVE_VDEC, "vde2_lat_active_vdec",
			"vde2_lat_active"/* parent */),
	GATE_VDE21(CLK_VDE2_LAT_CKEN_ENG, "vde2_lat_cken_eng",
			"top_vdec_ck"/* parent */, 8),
	GATE_VDE21_V(CLK_VDE2_LAT_CKEN_ENG_VDEC, "vde2_lat_cken_eng_vdec",
			"vde2_lat_cken_eng"/* parent */),
};

static const struct mtk_clk_desc vde2_mcd = {
	.clks = vde2_clks,
	.num_clks = CLK_VDE2_NR_CLK,
};

static const struct mtk_gate_regs ven1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_VEN1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ven1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VEN1_V(_id, _name, _parent) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
	}

static const struct mtk_gate ven1_clks[] = {
	GATE_VEN1(CLK_VEN1_CKE0_LARB, "ven1_larb",
			"top_venc_ck"/* parent */, 0),
	GATE_VEN1_V(CLK_VEN1_CKE0_LARB_SMI, "ven1_larb_smi",
			"ven1_larb"/* parent */),
	GATE_VEN1(CLK_VEN1_CKE1_VENC, "ven1_venc",
			"top_venc_ck"/* parent */, 4),
	GATE_VEN1_V(CLK_VEN1_CKE1_VENC_VENC, "ven1_venc_venc",
			"ven1_venc"/* parent */),
	GATE_VEN1(CLK_VEN1_CKE2_JPGENC, "ven1_jpgenc",
			"top_venc_ck"/* parent */, 8),
	GATE_VEN1_V(CLK_VEN1_CKE2_JPGENC_JPEGENC, "ven1_jpgenc_jpegenc",
			"ven1_jpgenc"/* parent */),
	GATE_VEN1(CLK_VEN1_CKE5_GALS, "ven1_gals",
			"top_venc_ck"/* parent */, 28),
	GATE_VEN1_V(CLK_VEN1_CKE5_GALS_VENC, "ven1_gals_venc",
			"ven1_gals"/* parent */),
};

static const struct mtk_clk_desc ven1_mcd = {
	.clks = ven1_clks,
	.num_clks = CLK_VEN1_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6858_cg[] = {
	{
		.compatible = "mediatek,mt6858-audiosys",
		.data = &afe_mcd,
	}, {
		.compatible = "mediatek,mt6858-camsys_main",
		.data = &cam_m_mcd,
	}, {
		.compatible = "mediatek,mt6858-camsys_rawa",
		.data = &cam_ra_mcd,
	}, {
		.compatible = "mediatek,mt6858-camsys_rawb",
		.data = &cam_rb_mcd,
	}, {
		.compatible = "mediatek,mt6858-mmsys0",
		.data = &mm_mcd,
	}, {
		.compatible = "mediatek,mt6858-imgsys1",
		.data = &imgsys1_mcd,
	}, {
		.compatible = "mediatek,mt6858-imgsys2",
		.data = &imgsys2_mcd,
	}, {
		.compatible = "mediatek,mt6858-imp_iic_wrap_c",
		.data = &impc_mcd,
	}, {
		.compatible = "mediatek,mt6858-imp_iic_wrap_es",
		.data = &impes_mcd,
	}, {
		.compatible = "mediatek,mt6858-imp_iic_wrap_s",
		.data = &imps_mcd,
	}, {
		.compatible = "mediatek,mt6858-infra_infracfg_ao_reg",
		.data = &infra_infracfg_ao_reg_mcd,
	}, {
		.compatible = "mediatek,mt6858-ipesys",
		.data = &ipe_mcd,
	}, {
		.compatible = "mediatek,mt6858-mdpsys",
		.data = &mdp_mcd,
	}, {
		.compatible = "mediatek,mt6858-pericfg_ao",
		.data = &perao_mcd,
	}, {
		.compatible = "mediatek,mt6858-ufscfg_ao",
		.data = &ufsao_mcd,
	}, {
		.compatible = "mediatek,mt6858-ufscfg_pdn",
		.data = &ufspdn_mcd,
	}, {
		.compatible = "mediatek,mt6858-vdecsys",
		.data = &vde2_mcd,
	}, {
		.compatible = "mediatek,mt6858-vencsys",
		.data = &ven1_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6858_cg_grp_probe(struct platform_device *pdev)
{
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init begin\n", __func__, pdev->name);
#endif

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init end\n", __func__, pdev->name);
#endif

	return r;
}

static struct platform_driver clk_mt6858_cg_drv = {
	.probe = clk_mt6858_cg_grp_probe,
	.driver = {
		.name = "clk-mt6858-cg",
		.of_match_table = of_match_clk_mt6858_cg,
	},
};

module_platform_driver(clk_mt6858_cg_drv);
MODULE_LICENSE("GPL");
