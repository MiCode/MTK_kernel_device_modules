// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Kuan-hsin.Lee <kuan-hsin.lee@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6993-clk.h>

#define MT_CCF_BRINGUP         0

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
	.set_ofs = 0x4,
	.clr_ofs = 0x4,
	.sta_ofs = 0x4,
};


static const struct mtk_gate_regs afe3_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0x8,
	.sta_ofs = 0x8,
};


static const struct mtk_gate_regs afe4_cg_regs = {
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

#define GATE_AFE0_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_AFE1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_AFE2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE2_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_AFE3(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe3_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE3_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_AFE4(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe4_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE4_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

static const struct mtk_gate afe_clks[] = {
	/* AFE0 */
	GATE_AFE0(AFE_PCM1, "afe_pcm1",
			"cksys_vlp_aud_vlpwire"/* parent */, 13),
	GATE_AFE0_V(AFE_PCM1_AUDIO, "afe_pcm1_audio",
	        "afe_pcm1"/* parent */),
	GATE_AFE0(AFE_PCM0, "afe_pcm0",
			"cksys_vlp_aud_vlpwire"/* parent */, 14),
	GATE_AFE0_V(AFE_PCM0_AUDIO, "afe_pcm0_audio",
	        "afe_pcm0"/* parent */),
	GATE_AFE0(AFE_CM2, "afe_cm2",
			"cksys_vlp_aud_vlpwire"/* parent */, 16),
	GATE_AFE0_V(AFE_CM2_AUDIO, "afe_cm2_audio",
	        "afe_cm2"/* parent */),
	GATE_AFE0(AFE_CM1, "afe_cm1",
			"cksys_vlp_aud_vlpwire"/* parent */, 17),
	GATE_AFE0_V(AFE_CM1_AUDIO, "afe_cm1_audio",
	        "afe_cm1"/* parent */),
	GATE_AFE0(AFE_CM0, "afe_cm0",
			"cksys_vlp_aud_vlpwire"/* parent */, 18),
	GATE_AFE0_V(AFE_CM0_AUDIO, "afe_cm0_audio",
	        "afe_cm0"/* parent */),
	GATE_AFE0(AFE_STF, "afe_stf",
			"cksys_vlp_aud_vlpwire"/* parent */, 19),
	GATE_AFE0_V(AFE_STF_AUDIO, "afe_stf_audio",
	        "afe_stf"/* parent */),
	GATE_AFE0(AFE_HW_GAIN23, "afe_hw_gain23",
			"cksys_vlp_aud_vlpwire"/* parent */, 20),
	GATE_AFE0_V(AFE_HW_GAIN23_AUDIO, "afe_hw_gain23_audio",
	        "afe_hw_gain23"/* parent */),
	GATE_AFE0(AFE_HW_GAIN01, "afe_hw_gain01",
			"cksys_vlp_aud_vlpwire"/* parent */, 21),
	GATE_AFE0_V(AFE_HW_GAIN01_AUDIO, "afe_hw_gain01_audio",
	        "afe_hw_gain01"/* parent */),
	GATE_AFE0(AFE_FM_I2S, "afe_fm_i2s",
			"cksys_vlp_aud_vlpwire"/* parent */, 24),
	GATE_AFE0_V(AFE_FM_I2S_AUDIO, "afe_fm_i2s_audio",
	        "afe_fm_i2s"/* parent */),
	GATE_AFE0(AFE_MTKAIFV4, "afe_mtkaifv4",
			"cksys_vlp_aud_vlpwire"/* parent */, 25),
	GATE_AFE0_V(AFE_MTKAIFV4_AUDIO, "afe_mtkaifv4_audio",
	        "afe_mtkaifv4"/* parent */),
	GATE_AFE0(AFE_VOWIF, "afe_vowif",
			"cksys_vlp_aud_vlpwire"/* parent */, 26),
	GATE_AFE0_V(AFE_VOWIF_AUDIO, "afe_vowif_audio",
	        "afe_vowif"/* parent */),
	/* AFE1 */
	GATE_AFE1(AFE_AUDIO_HOPPING, "afe_audio_hopping_ck",
			"cksys_vlp_aud_vlpwire"/* parent */, 0),
	GATE_AFE1_V(AFE_AUDIO_HOPPING_AUDIO, "afe_audio_hopping_ck_audio",
	        "afe_audio_hopping_ck"/* parent */),
	GATE_AFE1(AFE_AUDIO_F26M, "afe_audio_f26m_ck",
			"cksys_vlp_aud_vlpwire"/* parent */, 1),
	GATE_AFE1_V(AFE_AUDIO_F26M_AUDIO, "afe_audio_f26m_ck_audio",
	        "afe_audio_f26m_ck"/* parent */),
	GATE_AFE1(AFE_APLL1, "afe_apll1_ck",
			"cksys_aud_1_ck"/* parent */, 2),
	GATE_AFE1_V(AFE_APLL1_AUDIO, "afe_apll1_ck_audio",
	        "afe_apll1_ck"/* parent */),
	GATE_AFE1(AFE_APLL2, "afe_apll2_ck",
			"cksys_aud_2_ck"/* parent */, 3),
	GATE_AFE1_V(AFE_APLL2_AUDIO, "afe_apll2_ck_audio",
	        "afe_apll2_ck"/* parent */),
	GATE_AFE1(AFE_H208M, "afe_h208m_ck",
			"cksys_vlp_audio_h_ck"/* parent */, 4),
	GATE_AFE1_V(AFE_H208M_AUDIO, "afe_h208m_ck_audio",
	        "afe_h208m_ck"/* parent */),
	GATE_AFE1(AFE_APLL_TUNER2, "afe_apll_tuner2",
			"cksys_vlp_aud_engen2_ck"/* parent */, 12),
	GATE_AFE1_V(AFE_APLL_TUNER2_AUDIO, "afe_apll_tuner2_audio",
	        "afe_apll_tuner2"/* parent */),
	GATE_AFE1(AFE_APLL_TUNER1, "afe_apll_tuner1",
			"cksys_vlp_aud_engen1_ck"/* parent */, 13),
	GATE_AFE1_V(AFE_APLL_TUNER1_AUDIO, "afe_apll_tuner1_audio",
	        "afe_apll_tuner1"/* parent */),
	/* AFE2 */
	GATE_AFE2(AFE_UL2_ADC_HIRES_TML, "afe_ul2_aht",
			"cksys_vlp_audio_h_ck"/* parent */, 12),
	GATE_AFE2_V(AFE_UL2_ADC_HIRES_TML_AUDIO, "afe_ul2_aht_audio",
	        "afe_ul2_aht"/* parent */),
	GATE_AFE2(AFE_UL2_ADC_HIRES, "afe_ul2_adc_hires",
			"cksys_vlp_audio_h_ck"/* parent */, 13),
	GATE_AFE2_V(AFE_UL2_ADC_HIRES_AUDIO, "afe_ul2_adc_hires_audio",
	        "afe_ul2_adc_hires"/* parent */),
	GATE_AFE2(AFE_UL2_TML, "afe_ul2_tml",
			"cksys_vlp_aud_vlpwire"/* parent */, 14),
	GATE_AFE2_V(AFE_UL2_TML_AUDIO, "afe_ul2_tml_audio",
	        "afe_ul2_tml"/* parent */),
	GATE_AFE2(AFE_UL2_ADC, "afe_ul2_adc",
			"cksys_vlp_aud_vlpwire"/* parent */, 15),
	GATE_AFE2_V(AFE_UL2_ADC_AUDIO, "afe_ul2_adc_audio",
	        "afe_ul2_adc"/* parent */),
	GATE_AFE2(AFE_UL1_ADC_HIRES_TML, "afe_ul1_aht",
			"cksys_vlp_audio_h_ck"/* parent */, 16),
	GATE_AFE2_V(AFE_UL1_ADC_HIRES_TML_AUDIO, "afe_ul1_aht_audio",
	        "afe_ul1_aht"/* parent */),
	GATE_AFE2(AFE_UL1_ADC_HIRES, "afe_ul1_adc_hires",
			"cksys_vlp_audio_h_ck"/* parent */, 17),
	GATE_AFE2_V(AFE_UL1_ADC_HIRES_AUDIO, "afe_ul1_adc_hires_audio",
	        "afe_ul1_adc_hires"/* parent */),
	GATE_AFE2(AFE_UL1_TML, "afe_ul1_tml",
			"cksys_vlp_aud_vlpwire"/* parent */, 18),
	GATE_AFE2_V(AFE_UL1_TML_AUDIO, "afe_ul1_tml_audio",
	        "afe_ul1_tml"/* parent */),
	GATE_AFE2(AFE_UL1_ADC, "afe_ul1_adc",
			"cksys_vlp_aud_vlpwire"/* parent */, 19),
	GATE_AFE2_V(AFE_UL1_ADC_AUDIO, "afe_ul1_adc_audio",
	        "afe_ul1_adc"/* parent */),
	GATE_AFE2(AFE_UL0_ADC_HIRES_TML, "afe_ul0_aht",
			"cksys_vlp_audio_h_ck"/* parent */, 20),
	GATE_AFE2_V(AFE_UL0_ADC_HIRES_TML_AUDIO, "afe_ul0_aht_audio",
	        "afe_ul0_aht"/* parent */),
	GATE_AFE2(AFE_UL0_ADC_HIRES, "afe_ul0_adc_hires",
			"cksys_vlp_audio_h_ck"/* parent */, 21),
	GATE_AFE2_V(AFE_UL0_ADC_HIRES_AUDIO, "afe_ul0_adc_hires_audio",
	        "afe_ul0_adc_hires"/* parent */),
	GATE_AFE2(AFE_UL0_TML, "afe_ul0_tml",
			"cksys_vlp_aud_vlpwire"/* parent */, 22),
	GATE_AFE2_V(AFE_UL0_TML_AUDIO, "afe_ul0_tml_audio",
	        "afe_ul0_tml"/* parent */),
	GATE_AFE2(AFE_UL0_ADC, "afe_ul0_adc",
			"cksys_vlp_aud_vlpwire"/* parent */, 23),
	GATE_AFE2_V(AFE_UL0_ADC_AUDIO, "afe_ul0_adc_audio",
	        "afe_ul0_adc"/* parent */),
	/* AFE3 */
	GATE_AFE3(AFE_ETDM_IN6, "afe_etdm_in6",
			"cksys_vlp_f26m_ck"/* parent */, 7),
	GATE_AFE3_V(AFE_ETDM_IN6_AUDIO, "afe_etdm_in6_audio",
	        "afe_etdm_in6"/* parent */),
	GATE_AFE3(AFE_ETDM_IN5, "afe_etdm_in5",
			"cksys_vlp_f26m_ck"/* parent */, 8),
	GATE_AFE3_V(AFE_ETDM_IN5_AUDIO, "afe_etdm_in5_audio",
	        "afe_etdm_in5"/* parent */),
	GATE_AFE3(AFE_ETDM_IN4, "afe_etdm_in4",
			"cksys_vlp_f26m_ck"/* parent */, 9),
	GATE_AFE3_V(AFE_ETDM_IN4_AUDIO, "afe_etdm_in4_audio",
	        "afe_etdm_in4"/* parent */),
	GATE_AFE3(AFE_ETDM_IN3, "afe_etdm_in3",
			"cksys_vlp_f26m_ck"/* parent */, 10),
	GATE_AFE3_V(AFE_ETDM_IN3_AUDIO, "afe_etdm_in3_audio",
	        "afe_etdm_in3"/* parent */),
	GATE_AFE3(AFE_ETDM_IN2, "afe_etdm_in2",
			"cksys_vlp_f26m_ck"/* parent */, 11),
	GATE_AFE3_V(AFE_ETDM_IN2_AUDIO, "afe_etdm_in2_audio",
	        "afe_etdm_in2"/* parent */),
	GATE_AFE3(AFE_ETDM_IN1, "afe_etdm_in1",
			"cksys_vlp_f26m_ck"/* parent */, 12),
	GATE_AFE3_V(AFE_ETDM_IN1_AUDIO, "afe_etdm_in1_audio",
	        "afe_etdm_in1"/* parent */),
	GATE_AFE3(AFE_ETDM_IN0, "afe_etdm_in0",
			"cksys_vlp_f26m_ck"/* parent */, 13),
	GATE_AFE3_V(AFE_ETDM_IN0_AUDIO, "afe_etdm_in0_audio",
	        "afe_etdm_in0"/* parent */),
	GATE_AFE3(AFE_ETDM_OUT6, "afe_etdm_out6",
			"cksys_vlp_f26m_ck"/* parent */, 15),
	GATE_AFE3_V(AFE_ETDM_OUT6_AUDIO, "afe_etdm_out6_audio",
	        "afe_etdm_out6"/* parent */),
	GATE_AFE3(AFE_ETDM_OUT5, "afe_etdm_out5",
			"cksys_vlp_f26m_ck"/* parent */, 16),
	GATE_AFE3_V(AFE_ETDM_OUT5_AUDIO, "afe_etdm_out5_audio",
	        "afe_etdm_out5"/* parent */),
	GATE_AFE3(AFE_ETDM_OUT4, "afe_etdm_out4",
			"cksys_vlp_f26m_ck"/* parent */, 17),
	GATE_AFE3_V(AFE_ETDM_OUT4_AUDIO, "afe_etdm_out4_audio",
	        "afe_etdm_out4"/* parent */),
	GATE_AFE3(AFE_ETDM_OUT3, "afe_etdm_out3",
			"cksys_vlp_f26m_ck"/* parent */, 18),
	GATE_AFE3_V(AFE_ETDM_OUT3_AUDIO, "afe_etdm_out3_audio",
	        "afe_etdm_out3"/* parent */),
	GATE_AFE3(AFE_ETDM_OUT2, "afe_etdm_out2",
			"cksys_vlp_f26m_ck"/* parent */, 19),
	GATE_AFE3_V(AFE_ETDM_OUT2_AUDIO, "afe_etdm_out2_audio",
	        "afe_etdm_out2"/* parent */),
	GATE_AFE3(AFE_ETDM_OUT1, "afe_etdm_out1",
			"cksys_vlp_f26m_ck"/* parent */, 20),
	GATE_AFE3_V(AFE_ETDM_OUT1_AUDIO, "afe_etdm_out1_audio",
	        "afe_etdm_out1"/* parent */),
	GATE_AFE3(AFE_ETDM_OUT0, "afe_etdm_out0",
			"cksys_vlp_f26m_ck"/* parent */, 21),
	GATE_AFE3_V(AFE_ETDM_OUT0_AUDIO, "afe_etdm_out0_audio",
	        "afe_etdm_out0"/* parent */),
	GATE_AFE3(AFE_TDM_OUT, "afe_tdm_out",
			"cksys_vlp_f26m_ck"/* parent */, 24),
	GATE_AFE3_V(AFE_TDM_OUT_AUDIO, "afe_tdm_out_audio",
	        "afe_tdm_out"/* parent */),
	/* AFE4 */
	GATE_AFE4(AFE_GENERAL15_ASRC, "afe_general15_asrc",
			"cksys_vlp_aud_vlpwire"/* parent */, 9),
	GATE_AFE4_V(AFE_GENERAL15_ASRC_AUDIO, "afe_general15_asrc_audio",
	        "afe_general15_asrc"/* parent */),
	GATE_AFE4(AFE_GENERAL14_ASRC, "afe_general14_asrc",
			"cksys_vlp_aud_vlpwire"/* parent */, 10),
	GATE_AFE4_V(AFE_GENERAL14_ASRC_AUDIO, "afe_general14_asrc_audio",
	        "afe_general14_asrc"/* parent */),
	GATE_AFE4(AFE_GENERAL13_ASRC, "afe_general13_asrc",
			"cksys_vlp_aud_vlpwire"/* parent */, 11),
	GATE_AFE4_V(AFE_GENERAL13_ASRC_AUDIO, "afe_general13_asrc_audio",
	        "afe_general13_asrc"/* parent */),
	GATE_AFE4(AFE_GENERAL12_ASRC, "afe_general12_asrc",
			"cksys_vlp_aud_vlpwire"/* parent */, 12),
	GATE_AFE4_V(AFE_GENERAL12_ASRC_AUDIO, "afe_general12_asrc_audio",
	        "afe_general12_asrc"/* parent */),
	GATE_AFE4(AFE_GENERAL11_ASRC, "afe_general11_asrc",
			"cksys_vlp_aud_vlpwire"/* parent */, 13),
	GATE_AFE4_V(AFE_GENERAL11_ASRC_AUDIO, "afe_general11_asrc_audio",
	        "afe_general11_asrc"/* parent */),
	GATE_AFE4(AFE_GENERAL10_ASRC, "afe_general10_asrc",
			"cksys_vlp_aud_vlpwire"/* parent */, 14),
	GATE_AFE4_V(AFE_GENERAL10_ASRC_AUDIO, "afe_general10_asrc_audio",
	        "afe_general10_asrc"/* parent */),
	GATE_AFE4(AFE_GENERAL9_ASRC, "afe_general9_asrc",
			"cksys_vlp_aud_vlpwire"/* parent */, 15),
	GATE_AFE4_V(AFE_GENERAL9_ASRC_AUDIO, "afe_general9_asrc_audio",
	        "afe_general9_asrc"/* parent */),
	GATE_AFE4(AFE_GENERAL8_ASRC, "afe_general8_asrc",
			"cksys_vlp_aud_vlpwire"/* parent */, 16),
	GATE_AFE4_V(AFE_GENERAL8_ASRC_AUDIO, "afe_general8_asrc_audio",
	        "afe_general8_asrc"/* parent */),
	GATE_AFE4(AFE_GENERAL7_ASRC, "afe_general7_asrc",
			"cksys_vlp_aud_vlpwire"/* parent */, 17),
	GATE_AFE4_V(AFE_GENERAL7_ASRC_AUDIO, "afe_general7_asrc_audio",
	        "afe_general7_asrc"/* parent */),
	GATE_AFE4(AFE_GENERAL6_ASRC, "afe_general6_asrc",
			"cksys_vlp_aud_vlpwire"/* parent */, 18),
	GATE_AFE4_V(AFE_GENERAL6_ASRC_AUDIO, "afe_general6_asrc_audio",
	        "afe_general6_asrc"/* parent */),
	GATE_AFE4(AFE_GENERAL5_ASRC, "afe_general5_asrc",
			"cksys_vlp_aud_vlpwire"/* parent */, 19),
	GATE_AFE4_V(AFE_GENERAL5_ASRC_AUDIO, "afe_general5_asrc_audio",
	        "afe_general5_asrc"/* parent */),
	GATE_AFE4(AFE_GENERAL4_ASRC, "afe_general4_asrc",
			"cksys_vlp_aud_vlpwire"/* parent */, 20),
	GATE_AFE4_V(AFE_GENERAL4_ASRC_AUDIO, "afe_general4_asrc_audio",
	        "afe_general4_asrc"/* parent */),
	GATE_AFE4(AFE_GENERAL3_ASRC, "afe_general3_asrc",
			"cksys_vlp_aud_vlpwire"/* parent */, 21),
	GATE_AFE4_V(AFE_GENERAL3_ASRC_AUDIO, "afe_general3_asrc_audio",
	        "afe_general3_asrc"/* parent */),
	GATE_AFE4(AFE_GENERAL2_ASRC, "afe_general2_asrc",
			"cksys_vlp_aud_vlpwire"/* parent */, 22),
	GATE_AFE4_V(AFE_GENERAL2_ASRC_AUDIO, "afe_general2_asrc_audio",
	        "afe_general2_asrc"/* parent */),
	GATE_AFE4(AFE_GENERAL1_ASRC, "afe_general1_asrc",
			"cksys_vlp_aud_vlpwire"/* parent */, 23),
	GATE_AFE4_V(AFE_GENERAL1_ASRC_AUDIO, "afe_general1_asrc_audio",
	        "afe_general1_asrc"/* parent */),
	GATE_AFE4(AFE_GENERAL0_ASRC, "afe_general0_asrc",
			"cksys_vlp_aud_vlpwire"/* parent */, 24),
	GATE_AFE4_V(AFE_GENERAL0_ASRC_AUDIO, "afe_general0_asrc_audio",
	        "afe_general0_asrc"/* parent */),
	GATE_AFE4(AFE_CONNSYS_I2S_ASRC, "afe_connsys_i2s_asrc",
			"cksys_vlp_aud_vlpwire"/* parent */, 25),
	GATE_AFE4_V(AFE_CONNSYS_I2S_ASRC_AUDIO, "afe_connsys_i2s_asrc_audio",
	        "afe_connsys_i2s_asrc"/* parent */),
};

static const struct mtk_clk_desc afe_mcd = {
	.clks = afe_clks,
	.num_clks = CLK_AFE_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6993_adsp[] = {
	{
		.compatible = "mediatek,mt6993-audiosys",
		.data = &afe_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6993_adsp_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6993_adsp_drv = {
	.probe = clk_mt6993_adsp_grp_probe,
	.driver = {
		.name = "clk-mt6993-adsp",
		.of_match_table = of_match_clk_mt6993_adsp,
	},
};

module_platform_driver(clk_mt6993_adsp_drv);
MODULE_LICENSE("GPL");
