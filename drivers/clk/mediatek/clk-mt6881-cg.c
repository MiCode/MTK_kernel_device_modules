// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Claude Yen <claude.yen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6881-clk.h>

#define MT_CCF_BRINGUP		1 //FIXME

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
	.set_ofs = 0x1C5C,
	.clr_ofs = 0x1C5C,
	.sta_ofs = 0x1C5C,
};

static const struct mtk_gate_regs afe4_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x4,
	.sta_ofs = 0x4,
};

static const struct mtk_gate_regs afe5_cg_regs = {
	.set_ofs = 0x70,
	.clr_ofs = 0x70,
	.sta_ofs = 0x70,
};

static const struct mtk_gate_regs afe6_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0x8,
	.sta_ofs = 0x8,
};

static const struct mtk_gate_regs afe7_cg_regs = {
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

#define GATE_AFE0_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
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
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
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

#define GATE_AFE6(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe6_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE6_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

#define GATE_AFE7(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe7_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE7_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate afe_clks[] = {
	/* AFE0 */
	GATE_AFE0(CLK_AFE_DL1_DAC_TML, "afe_dl1_dac_tml",
			"cksys_f26m_ck"/* parent */, 2),
	GATE_AFE0_V(CLK_AFE_DL1_DAC_TML_AUDIO, "afe_dl1_dac_tml_audio",
			"afe_dl1_dac_tml"/* parent */),
	GATE_AFE0(CLK_AFE_DL1_DAC_HIRES, "afe_dl1_dac_hires",
			"cksys_audio_h_ck"/* parent */, 3),
	GATE_AFE0_V(CLK_AFE_DL1_DAC_HIRES_AUDIO, "afe_dl1_dac_hires_audio",
			"afe_dl1_dac_hires"/* parent */),
	GATE_AFE0(CLK_AFE_DL1_DAC, "afe_dl1_dac",
			"cksys_f26m_ck"/* parent */, 4),
	GATE_AFE0_V(CLK_AFE_DL1_DAC_AUDIO, "afe_dl1_dac_audio",
			"afe_dl1_dac"/* parent */),
	GATE_AFE0(CLK_AFE_DL1_PREDIS, "afe_dl1_predis",
			"cksys_f26m_ck"/* parent */, 5),
	GATE_AFE0_V(CLK_AFE_DL1_PREDIS_AUDIO, "afe_dl1_predis_audio",
			"afe_dl1_predis"/* parent */),
	GATE_AFE0(CLK_AFE_DL1_NLE, "afe_dl1_nle",
			"cksys_f26m_ck"/* parent */, 6),
	GATE_AFE0_V(CLK_AFE_DL1_NLE_AUDIO, "afe_dl1_nle_audio",
			"afe_dl1_nle"/* parent */),
	GATE_AFE0(CLK_AFE_DL0_DAC_TML, "afe_dl0_dac_tml",
			"cksys_f26m_ck"/* parent */, 7),
	GATE_AFE0_V(CLK_AFE_DL0_DAC_TML_AUDIO, "afe_dl0_dac_tml_audio",
			"afe_dl0_dac_tml"/* parent */),
	GATE_AFE0(CLK_AFE_DL0_DAC_HIRES, "afe_dl0_dac_hires",
			"cksys_audio_h_ck"/* parent */, 8),
	GATE_AFE0_V(CLK_AFE_DL0_DAC_HIRES_AUDIO, "afe_dl0_dac_hires_audio",
			"afe_dl0_dac_hires"/* parent */),
	GATE_AFE0(CLK_AFE_DL0_DAC, "afe_dl0_dac",
			"cksys_f26m_ck"/* parent */, 9),
	GATE_AFE0_V(CLK_AFE_DL0_DAC_AUDIO, "afe_dl0_dac_audio",
			"afe_dl0_dac"/* parent */),
	GATE_AFE0(CLK_AFE_DL0_PREDIS, "afe_dl0_predis",
			"cksys_f26m_ck"/* parent */, 10),
	GATE_AFE0_V(CLK_AFE_DL0_PREDIS_AUDIO, "afe_dl0_predis_audio",
			"afe_dl0_predis"/* parent */),
	GATE_AFE0(CLK_AFE_DL0_NLE, "afe_dl0_nle",
			"cksys_f26m_ck"/* parent */, 11),
	GATE_AFE0_V(CLK_AFE_DL0_NLE_AUDIO, "afe_dl0_nle_audio",
			"afe_dl0_nle"/* parent */),
	GATE_AFE0(CLK_AFE_PCM1, "afe_pcm1",
			"cksys_f26m_ck"/* parent */, 13),
	GATE_AFE0_V(CLK_AFE_PCM1_AUDIO, "afe_pcm1_audio",
			"afe_pcm1"/* parent */),
	GATE_AFE0(CLK_AFE_PCM0, "afe_pcm0",
			"cksys_f26m_ck"/* parent */, 14),
	GATE_AFE0_V(CLK_AFE_PCM0_AUDIO, "afe_pcm0_audio",
			"afe_pcm0"/* parent */),
	GATE_AFE0(CLK_AFE_CM2, "afe_cm2",
			"cksys_f26m_ck"/* parent */, 16),
	GATE_AFE0_V(CLK_AFE_CM2_AUDIO, "afe_cm2_audio",
			"afe_cm2"/* parent */),
	GATE_AFE0(CLK_AFE_CM1, "afe_cm1",
			"cksys_f26m_ck"/* parent */, 17),
	GATE_AFE0_V(CLK_AFE_CM1_AUDIO, "afe_cm1_audio",
			"afe_cm1"/* parent */),
	GATE_AFE0(CLK_AFE_CM0, "afe_cm0",
			"cksys_f26m_ck"/* parent */, 18),
	GATE_AFE0_V(CLK_AFE_CM0_AUDIO, "afe_cm0_audio",
			"afe_cm0"/* parent */),
	GATE_AFE0(CLK_AFE_STF, "afe_stf",
			"cksys_f26m_ck"/* parent */, 19),
	GATE_AFE0_V(CLK_AFE_STF_AUDIO, "afe_stf_audio",
			"afe_stf"/* parent */),
	GATE_AFE0(CLK_AFE_HW_GAIN23, "afe_hw_gain23",
			"cksys_f26m_ck"/* parent */, 20),
	GATE_AFE0_V(CLK_AFE_HW_GAIN23_AUDIO, "afe_hw_gain23_audio",
			"afe_hw_gain23"/* parent */),
	GATE_AFE0(CLK_AFE_HW_GAIN01, "afe_hw_gain01",
			"cksys_f26m_ck"/* parent */, 21),
	GATE_AFE0_V(CLK_AFE_HW_GAIN01_AUDIO, "afe_hw_gain01_audio",
			"afe_hw_gain01"/* parent */),
	GATE_AFE0(CLK_AFE_FM_I2S, "afe_fm_i2s",
			"cksys_f26m_ck"/* parent */, 24),
	GATE_AFE0_V(CLK_AFE_FM_I2S_AUDIO, "afe_fm_i2s_audio",
			"afe_fm_i2s"/* parent */),
	GATE_AFE0(CLK_AFE_MTKAIFV4, "afe_mtkaifv4",
			"cksys_f26m_ck"/* parent */, 25),
	GATE_AFE0_V(CLK_AFE_MTKAIFV4_AUDIO, "afe_mtkaifv4_audio",
			"afe_mtkaifv4"/* parent */),
	/* AFE1 */
	GATE_AFE1(CLK_AFE_AUDIO_HOPPING, "afe_audio_hopping_ck",
			"cksys_f26m_ck"/* parent */, 0),
	GATE_AFE1_V(CLK_AFE_AUDIO_HOPPING_AUDIO, "afe_audio_hopping_ck_audio",
			"afe_audio_hopping_ck"/* parent */),
	GATE_AFE1(CLK_AFE_AUDIO_F26M, "afe_audio_f26m_ck",
			"cksys_f26m_ck"/* parent */, 1),
	GATE_AFE1_V(CLK_AFE_AUDIO_F26M_AUDIO, "afe_audio_f26m_ck_audio",
			"afe_audio_f26m_ck"/* parent */),
	GATE_AFE1(CLK_AFE_APLL1, "afe_apll1_ck",
			"cksys_aud_1_ck"/* parent */, 2),
	GATE_AFE1_V(CLK_AFE_APLL1_AUDIO, "afe_apll1_ck_audio",
			"afe_apll1_ck"/* parent */),
	GATE_AFE1(CLK_AFE_APLL2, "afe_apll2_ck",
			"cksys_aud_2_ck"/* parent */, 3),
	GATE_AFE1_V(CLK_AFE_APLL2_AUDIO, "afe_apll2_ck_audio",
			"afe_apll2_ck"/* parent */),
	GATE_AFE1(CLK_AFE_H208M, "afe_h208m_ck",
			"cksys_audio_h_ck"/* parent */, 4),
	GATE_AFE1_V(CLK_AFE_H208M_AUDIO, "afe_h208m_ck_audio",
			"afe_h208m_ck"/* parent */),
	GATE_AFE1(CLK_AFE_APLL_TUNER2, "afe_apll_tuner2",
			"cksys_aud_engen2_ck"/* parent */, 12),
	GATE_AFE1_V(CLK_AFE_APLL_TUNER2_AUDIO, "afe_apll_tuner2_audio",
			"afe_apll_tuner2"/* parent */),
	GATE_AFE1(CLK_AFE_APLL_TUNER1, "afe_apll_tuner1",
			"cksys_aud_engen1_ck"/* parent */, 13),
	GATE_AFE1_V(CLK_AFE_APLL_TUNER1_AUDIO, "afe_apll_tuner1_audio",
			"afe_apll_tuner1"/* parent */),
	/* AFE2 */
	GATE_AFE2(CLK_AFE_AUD_PAD_TOP_MOSI_EN, "afe_aud_pad_mosi",
			"cksys_f26m_ck"/* parent */, 7),
	GATE_AFE2_V(CLK_AFE_AUD_PAD_TOP_MOSI_EN_AUDIO, "afe_aud_pad_mosi_audio",
			"afe_aud_pad_mosi"/* parent */),
	/* AFE3 */
	GATE_AFE3(CLK_AFE_ETDM6_PADTOP_CK_EN, "afe_etdm6_padtop",
			"cksys_aud_engen1_ck"/* parent */, 4),
	GATE_AFE3_V(CLK_AFE_ETDM6_PADTOP_CK_EN_AUDIO, "afe_etdm6_padtop_audio",
			"afe_etdm6_padtop"/* parent */),
	GATE_AFE3(CLK_AFE_ETDM7_PADTOP_CK_EN, "afe_etdm7_padtop",
			"cksys_aud_engen1_ck"/* parent */, 5),
	GATE_AFE3_V(CLK_AFE_ETDM7_PADTOP_CK_EN_AUDIO, "afe_etdm7_padtop_audio",
			"afe_etdm7_padtop"/* parent */),
	/* AFE4 */
	GATE_AFE4(CLK_AFE_UL1_ADC_HIRES_TML, "afe_ul1_aht",
			"cksys_audio_h_ck"/* parent */, 16),
	GATE_AFE4_V(CLK_AFE_UL1_ADC_HIRES_TML_AUDIO, "afe_ul1_aht_audio",
			"afe_ul1_aht"/* parent */),
	GATE_AFE4(CLK_AFE_UL1_ADC_HIRES, "afe_ul1_adc_hires",
			"cksys_audio_h_ck"/* parent */, 17),
	GATE_AFE4_V(CLK_AFE_UL1_ADC_HIRES_AUDIO, "afe_ul1_adc_hires_audio",
			"afe_ul1_adc_hires"/* parent */),
	GATE_AFE4(CLK_AFE_UL1_TML, "afe_ul1_tml",
			"cksys_f26m_ck"/* parent */, 18),
	GATE_AFE4_V(CLK_AFE_UL1_TML_AUDIO, "afe_ul1_tml_audio",
			"afe_ul1_tml"/* parent */),
	GATE_AFE4(CLK_AFE_UL1_ADC, "afe_ul1_adc",
			"cksys_f26m_ck"/* parent */, 19),
	GATE_AFE4_V(CLK_AFE_UL1_ADC_AUDIO, "afe_ul1_adc_audio",
			"afe_ul1_adc"/* parent */),
	GATE_AFE4(CLK_AFE_UL0_ADC_HIRES_TML, "afe_ul0_aht",
			"cksys_audio_h_ck"/* parent */, 20),
	GATE_AFE4_V(CLK_AFE_UL0_ADC_HIRES_TML_AUDIO, "afe_ul0_aht_audio",
			"afe_ul0_aht"/* parent */),
	GATE_AFE4(CLK_AFE_UL0_ADC_HIRES, "afe_ul0_adc_hires",
			"cksys_audio_h_ck"/* parent */, 21),
	GATE_AFE4_V(CLK_AFE_UL0_ADC_HIRES_AUDIO, "afe_ul0_adc_hires_audio",
			"afe_ul0_adc_hires"/* parent */),
	GATE_AFE4(CLK_AFE_UL0_TML, "afe_ul0_tml",
			"cksys_f26m_ck"/* parent */, 22),
	GATE_AFE4_V(CLK_AFE_UL0_TML_AUDIO, "afe_ul0_tml_audio",
			"afe_ul0_tml"/* parent */),
	GATE_AFE4(CLK_AFE_UL0_ADC, "afe_ul0_adc",
			"cksys_f26m_ck"/* parent */, 23),
	GATE_AFE4_V(CLK_AFE_UL0_ADC_AUDIO, "afe_ul0_adc_audio",
			"afe_ul0_adc"/* parent */),
	/* AFE5 */
	GATE_AFE5(CLK_AFE_ETDM_IN_DMA0, "afe_etdm_in_dma0",
			"cksys_aud_engen1_ck"/* parent */, 7),
	GATE_AFE5_V(CLK_AFE_ETDM_IN_DMA0_AUDIO, "afe_etdm_in_dma0_audio",
			"afe_etdm_in_dma0"/* parent */),
	/* AFE6 */
	GATE_AFE6(CLK_AFE_ETDM_IN6, "afe_etdm_in6",
			"cksys_aud_engen1_ck"/* parent */, 7),
	GATE_AFE6_V(CLK_AFE_ETDM_IN6_AUDIO, "afe_etdm_in6_audio",
			"afe_etdm_in6"/* parent */),
	GATE_AFE6(CLK_AFE_ETDM_IN2, "afe_etdm_in2",
			"cksys_aud_engen1_ck"/* parent */, 11),
	GATE_AFE6_V(CLK_AFE_ETDM_IN2_AUDIO, "afe_etdm_in2_audio",
			"afe_etdm_in2"/* parent */),
	GATE_AFE6(CLK_AFE_ETDM_IN1, "afe_etdm_in1",
			"cksys_aud_engen1_ck"/* parent */, 12),
	GATE_AFE6_V(CLK_AFE_ETDM_IN1_AUDIO, "afe_etdm_in1_audio",
			"afe_etdm_in1"/* parent */),
	GATE_AFE6(CLK_AFE_ETDM_IN0, "afe_etdm_in0",
			"cksys_aud_engen1_ck"/* parent */, 13),
	GATE_AFE6_V(CLK_AFE_ETDM_IN0_AUDIO, "afe_etdm_in0_audio",
			"afe_etdm_in0"/* parent */),
	GATE_AFE6(CLK_AFE_ETDM_OUT6, "afe_etdm_out6",
			"cksys_aud_engen1_ck"/* parent */, 15),
	GATE_AFE6_V(CLK_AFE_ETDM_OUT6_AUDIO, "afe_etdm_out6_audio",
			"afe_etdm_out6"/* parent */),
	GATE_AFE6(CLK_AFE_ETDM_OUT2, "afe_etdm_out2",
			"cksys_aud_engen1_ck"/* parent */, 19),
	GATE_AFE6_V(CLK_AFE_ETDM_OUT2_AUDIO, "afe_etdm_out2_audio",
			"afe_etdm_out2"/* parent */),
	GATE_AFE6(CLK_AFE_ETDM_OUT1, "afe_etdm_out1",
			"cksys_aud_engen1_ck"/* parent */, 20),
	GATE_AFE6_V(CLK_AFE_ETDM_OUT1_AUDIO, "afe_etdm_out1_audio",
			"afe_etdm_out1"/* parent */),
	GATE_AFE6(CLK_AFE_ETDM_OUT0, "afe_etdm_out0",
			"cksys_aud_engen1_ck"/* parent */, 21),
	GATE_AFE6_V(CLK_AFE_ETDM_OUT0_AUDIO, "afe_etdm_out0_audio",
			"afe_etdm_out0"/* parent */),
	/* AFE7 */
	GATE_AFE7(CLK_AFE_GENERAL7_ASRC, "afe_general7_asrc",
			"cksys_f26m_ck"/* parent */, 17),
	GATE_AFE7_V(CLK_AFE_GENERAL7_ASRC_AUDIO, "afe_general7_asrc_audio",
			"afe_general7_asrc"/* parent */),
	GATE_AFE7(CLK_AFE_GENERAL6_ASRC, "afe_general6_asrc",
			"cksys_f26m_ck"/* parent */, 18),
	GATE_AFE7_V(CLK_AFE_GENERAL6_ASRC_AUDIO, "afe_general6_asrc_audio",
			"afe_general6_asrc"/* parent */),
	GATE_AFE7(CLK_AFE_GENERAL5_ASRC, "afe_general5_asrc",
			"cksys_f26m_ck"/* parent */, 19),
	GATE_AFE7_V(CLK_AFE_GENERAL5_ASRC_AUDIO, "afe_general5_asrc_audio",
			"afe_general5_asrc"/* parent */),
	GATE_AFE7(CLK_AFE_GENERAL4_ASRC, "afe_general4_asrc",
			"cksys_f26m_ck"/* parent */, 20),
	GATE_AFE7_V(CLK_AFE_GENERAL4_ASRC_AUDIO, "afe_general4_asrc_audio",
			"afe_general4_asrc"/* parent */),
	GATE_AFE7(CLK_AFE_GENERAL3_ASRC, "afe_general3_asrc",
			"cksys_f26m_ck"/* parent */, 21),
	GATE_AFE7_V(CLK_AFE_GENERAL3_ASRC_AUDIO, "afe_general3_asrc_audio",
			"afe_general3_asrc"/* parent */),
	GATE_AFE7(CLK_AFE_GENERAL2_ASRC, "afe_general2_asrc",
			"cksys_f26m_ck"/* parent */, 22),
	GATE_AFE7_V(CLK_AFE_GENERAL2_ASRC_AUDIO, "afe_general2_asrc_audio",
			"afe_general2_asrc"/* parent */),
	GATE_AFE7(CLK_AFE_GENERAL1_ASRC, "afe_general1_asrc",
			"cksys_f26m_ck"/* parent */, 23),
	GATE_AFE7_V(CLK_AFE_GENERAL1_ASRC_AUDIO, "afe_general1_asrc_audio",
			"afe_general1_asrc"/* parent */),
	GATE_AFE7(CLK_AFE_GENERAL0_ASRC, "afe_general0_asrc",
			"cksys_f26m_ck"/* parent */, 24),
	GATE_AFE7_V(CLK_AFE_GENERAL0_ASRC_AUDIO, "afe_general0_asrc_audio",
			"afe_general0_asrc"/* parent */),
	GATE_AFE7(CLK_AFE_CONNSYS_I2S_ASRC, "afe_connsys_i2s_asrc",
			"cksys_f26m_ck"/* parent */, 25),
	GATE_AFE7_V(CLK_AFE_CONNSYS_I2S_ASRC_AUDIO, "afe_connsys_i2s_asrc_audio",
			"afe_connsys_i2s_asrc"/* parent */),
};

static const struct mtk_clk_desc afe_mcd = {
	.clks = afe_clks,
	.num_clks = CLK_AFE_NR_CLK,
};

static const struct mtk_gate_regs cam_mr_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_MR(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_mr_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_MR_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate cam_mr_clks[] = {
	GATE_CAM_MR(CLK_CAM_MR_LARB13, "cam_mr_larb13",
			"cksys_cam_ck"/* parent */, 0),
	GATE_CAM_MR_V(CLK_CAM_MR_LARB13_CAMSV, "cam_mr_larb13_camsv",
			"cam_mr_larb13"/* parent */),
	GATE_CAM_MR_V(CLK_CAM_MR_LARB13_SMI, "cam_mr_larb13_smi",
			"cam_mr_larb13"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_LARB14, "cam_mr_larb14",
			"cksys_cam_ck"/* parent */, 1),
	GATE_CAM_MR_V(CLK_CAM_MR_LARB14_CAMSV, "cam_mr_larb14_camsv",
			"cam_mr_larb14"/* parent */),
	GATE_CAM_MR_V(CLK_CAM_MR_LARB14_SMI, "cam_mr_larb14_smi",
			"cam_mr_larb14"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_LARB19, "cam_mr_larb19",
			"cksys_cam_ck"/* parent */, 2),
	GATE_CAM_MR_V(CLK_CAM_MR_LARB19_CAMSV, "cam_mr_larb19_camsv",
			"cam_mr_larb19"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_LARB25, "cam_mr_larb25",
			"cksys_cam_ck"/* parent */, 3),
	GATE_CAM_MR_V(CLK_CAM_MR_LARB25_CAMSV, "cam_mr_larb25_camsv",
			"cam_mr_larb25"/* parent */),
	GATE_CAM_MR_V(CLK_CAM_MR_LARB25_PDA, "cam_mr_larb25_pda",
			"cam_mr_larb25"/* parent */),
	GATE_CAM_MR_V(CLK_CAM_MR_LARB25_SMI, "cam_mr_larb25_smi",
			"cam_mr_larb25"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_LARB26, "cam_mr_larb26",
			"cksys_cam_ck"/* parent */, 4),
	GATE_CAM_MR_V(CLK_CAM_MR_LARB26_CAMSV, "cam_mr_larb26_camsv",
			"cam_mr_larb26"/* parent */),
	GATE_CAM_MR_V(CLK_CAM_MR_LARB26_PDA, "cam_mr_larb26_pda",
			"cam_mr_larb26"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_LARB29, "cam_mr_larb29",
			"cksys_cam_ck"/* parent */, 5),
	GATE_CAM_MR_V(CLK_CAM_MR_LARB29_CAMSV, "cam_mr_larb29_camsv",
			"cam_mr_larb29"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_SENINF_CAMTM, "cam_mr_seninf_camtm",
			"cksys_camtm_ck"/* parent */, 12),
	GATE_CAM_MR_V(CLK_CAM_MR_SENINF_CAMTM_CAMSV, "cam_mr_seninf_camtm_camsv",
			"cam_mr_seninf_camtm"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_CAMSV_TOP, "cam_mr_camsv_top",
			"cksys_cam_ck"/* parent */, 13),
	GATE_CAM_MR_V(CLK_CAM_MR_CAMSV_TOP_CAMSV, "cam_mr_camsv_top_camsv",
			"cam_mr_camsv_top"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_CAMSV_A, "cam_mr_camsv_a",
			"cksys_cam_ck"/* parent */, 14),
	GATE_CAM_MR_V(CLK_CAM_MR_CAMSV_A_CAMSV, "cam_mr_camsv_a_camsv",
			"cam_mr_camsv_a"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_CAMSV_B, "cam_mr_camsv_b",
			"cksys_cam_ck"/* parent */, 15),
	GATE_CAM_MR_V(CLK_CAM_MR_CAMSV_B_CAMSV, "cam_mr_camsv_b_camsv",
			"cam_mr_camsv_b"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_CAMSV_C, "cam_mr_camsv_c",
			"cksys_cam_ck"/* parent */, 16),
	GATE_CAM_MR_V(CLK_CAM_MR_CAMSV_C_CAMSV, "cam_mr_camsv_c_camsv",
			"cam_mr_camsv_c"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_CAMSV_D, "cam_mr_camsv_d",
			"cksys_cam_ck"/* parent */, 17),
	GATE_CAM_MR_V(CLK_CAM_MR_CAMSV_D_CAMSV, "cam_mr_camsv_d_camsv",
			"cam_mr_camsv_d"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_CAMSV_E, "cam_mr_camsv_e",
			"cksys_cam_ck"/* parent */, 18),
	GATE_CAM_MR_V(CLK_CAM_MR_CAMSV_E_CAMSV, "cam_mr_camsv_e_camsv",
			"cam_mr_camsv_e"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_CAMSV, "cam_mr_camsv",
			"cksys_cam_ck"/* parent */, 19),
	GATE_CAM_MR_V(CLK_CAM_MR_CAMSV_CAMSV, "cam_mr_camsv_camsv",
			"cam_mr_camsv"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_PDA0, "cam_mr_pda0",
			"cksys_cam_ck"/* parent */, 20),
	GATE_CAM_MR_V(CLK_CAM_MR_PDA0_PDAF, "cam_mr_pda0_pdaf",
			"cam_mr_pda0"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_PDA1, "cam_mr_pda1",
			"cksys_cam_ck"/* parent */, 21),
	GATE_CAM_MR_V(CLK_CAM_MR_PDA1_PDAF, "cam_mr_pda1_pdaf",
			"cam_mr_pda1"/* parent */),
	GATE_CAM_MR(CLK_CAM_MR_FAKE_ENG, "cam_mr_fake_eng",
			"cksys_cam_ck"/* parent */, 22),
	GATE_CAM_MR_V(CLK_CAM_MR_FAKE_ENG_CAMSV, "cam_mr_fake_eng_camsv",
			"cam_mr_fake_eng"/* parent */),
};

static const struct mtk_clk_desc cam_mr_mcd = {
	.clks = cam_mr_clks,
	.num_clks = CLK_CAM_MR_NR_CLK,
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
			"cksys_cam_ck"/* parent */, 0),
	GATE_CAM_RA_V(CLK_CAM_RA_LARBX_CAM_RAW, "cam_ra_larbx_cam_raw",
			"cam_ra_larbx"/* parent */),
	GATE_CAM_RA_V(CLK_CAM_RA_LARBX_SMI, "cam_ra_larbx_smi",
			"cam_ra_larbx"/* parent */),
	GATE_CAM_RA(CLK_CAM_RA_CAM, "cam_ra_cam",
			"cksys_cam_ck"/* parent */, 1),
	GATE_CAM_RA_V(CLK_CAM_RA_CAM_CAM_RAW, "cam_ra_cam_cam_raw",
			"cam_ra_cam"/* parent */),
	GATE_CAM_RA(CLK_CAM_RA_CAMTG, "cam_ra_camtg",
			"cksys_cam_ck"/* parent */, 2),
	GATE_CAM_RA_V(CLK_CAM_RA_CAMTG_CAM_RAW, "cam_ra_camtg_cam_raw",
			"cam_ra_camtg"/* parent */),
	GATE_CAM_RA(CLK_CAM_RA_CAM_26M, "cam_ra_cam_26m",
			"cksys_cam_ck"/* parent */, 5),
	GATE_CAM_RA_V(CLK_CAM_RA_CAM_26M_CAM_RAW, "cam_ra_cam_26m_cam_raw",
			"cam_ra_cam_26m"/* parent */),
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
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate cam_rb_clks[] = {
	GATE_CAM_RB(CLK_CAM_RB_LARBX, "cam_rb_larbx",
			"cksys_cam_ck"/* parent */, 0),
	GATE_CAM_RB_V(CLK_CAM_RB_LARBX_CAM_RAW, "cam_rb_larbx_cam_raw",
			"cam_rb_larbx"/* parent */),
	GATE_CAM_RB_V(CLK_CAM_RB_LARBX_SMI, "cam_rb_larbx_smi",
			"cam_rb_larbx"/* parent */),
	GATE_CAM_RB(CLK_CAM_RB_CAM, "cam_rb_cam",
			"cksys_cam_ck"/* parent */, 1),
	GATE_CAM_RB_V(CLK_CAM_RB_CAM_CAM_RAW, "cam_rb_cam_cam_raw",
			"cam_rb_cam"/* parent */),
	GATE_CAM_RB(CLK_CAM_RB_CAMTG, "cam_rb_camtg",
			"cksys_cam_ck"/* parent */, 2),
	GATE_CAM_RB_V(CLK_CAM_RB_CAMTG_CAM_RAW, "cam_rb_camtg_cam_raw",
			"cam_rb_camtg"/* parent */),
	GATE_CAM_RB(CLK_CAM_RB_CAM_26M, "cam_rb_cam_26m",
			"cksys_cam_ck"/* parent */, 5),
	GATE_CAM_RB_V(CLK_CAM_RB_CAM_26M_CAM_RAW, "cam_rb_cam_26m_cam_raw",
			"cam_rb_cam_26m"/* parent */),
};

static const struct mtk_clk_desc cam_rb_mcd = {
	.clks = cam_rb_clks,
	.num_clks = CLK_CAM_RB_NR_CLK,
};

static const struct mtk_gate_regs camsys_rmsa_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAMSYS_RMSA(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &camsys_rmsa_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAMSYS_RMSA_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate camsys_rmsa_clks[] = {
	GATE_CAMSYS_RMSA(CLK_CAMSYS_RMSA_LARBX, "camsys_rmsa_larbx",
			"cksys_cam_ck"/* parent */, 0),
	GATE_CAMSYS_RMSA_V(CLK_CAMSYS_RMSA_LARBX_CAM_RAW, "camsys_rmsa_larbx_cam_raw",
			"camsys_rmsa_larbx"/* parent */),
	GATE_CAMSYS_RMSA(CLK_CAMSYS_RMSA_CAM, "camsys_rmsa_cam",
			"cksys_cam_ck"/* parent */, 1),
	GATE_CAMSYS_RMSA_V(CLK_CAMSYS_RMSA_CAM_CAM_RAW, "camsys_rmsa_cam_cam_raw",
			"camsys_rmsa_cam"/* parent */),
	GATE_CAMSYS_RMSA(CLK_CAMSYS_RMSA_CAMTG, "camsys_rmsa_camtg",
			"cksys_cam_ck"/* parent */, 2),
	GATE_CAMSYS_RMSA_V(CLK_CAMSYS_RMSA_CAMTG_CAM_RAW, "camsys_rmsa_camtg_cam_raw",
			"camsys_rmsa_camtg"/* parent */),
};

static const struct mtk_clk_desc camsys_rmsa_mcd = {
	.clks = camsys_rmsa_clks,
	.num_clks = CLK_CAMSYS_RMSA_NR_CLK,
};

static const struct mtk_gate_regs camsys_rmsb_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAMSYS_RMSB(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &camsys_rmsb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAMSYS_RMSB_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate camsys_rmsb_clks[] = {
	GATE_CAMSYS_RMSB(CLK_CAMSYS_RMSB_LARBX, "camsys_rmsb_larbx",
			"cksys_cam_ck"/* parent */, 0),
	GATE_CAMSYS_RMSB_V(CLK_CAMSYS_RMSB_LARBX_CAM_RAW, "camsys_rmsb_larbx_cam_raw",
			"camsys_rmsb_larbx"/* parent */),
	GATE_CAMSYS_RMSB(CLK_CAMSYS_RMSB_CAM, "camsys_rmsb_cam",
			"cksys_cam_ck"/* parent */, 1),
	GATE_CAMSYS_RMSB_V(CLK_CAMSYS_RMSB_CAM_CAM_RAW, "camsys_rmsb_cam_cam_raw",
			"camsys_rmsb_cam"/* parent */),
	GATE_CAMSYS_RMSB(CLK_CAMSYS_RMSB_CAMTG, "camsys_rmsb_camtg",
			"cksys_cam_ck"/* parent */, 2),
	GATE_CAMSYS_RMSB_V(CLK_CAMSYS_RMSB_CAMTG_CAM_RAW, "camsys_rmsb_camtg_cam_raw",
			"camsys_rmsb_camtg"/* parent */),
};

static const struct mtk_clk_desc camsys_rmsb_mcd = {
	.clks = camsys_rmsb_clks,
	.num_clks = CLK_CAMSYS_RMSB_NR_CLK,
};

static const struct mtk_gate_regs cam_ya_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_YA(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_ya_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_YA_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate cam_ya_clks[] = {
	GATE_CAM_YA(CLK_CAM_YA_LARBX, "cam_ya_larbx",
			"cksys_cam_ck"/* parent */, 0),
	GATE_CAM_YA_V(CLK_CAM_YA_LARBX_CAM_RAW, "cam_ya_larbx_cam_raw",
			"cam_ya_larbx"/* parent */),
	GATE_CAM_YA_V(CLK_CAM_YA_LARBX_SMI, "cam_ya_larbx_smi",
			"cam_ya_larbx"/* parent */),
	GATE_CAM_YA(CLK_CAM_YA_CAM, "cam_ya_cam",
			"cksys_cam_ck"/* parent */, 1),
	GATE_CAM_YA_V(CLK_CAM_YA_CAM_CAM_RAW, "cam_ya_cam_cam_raw",
			"cam_ya_cam"/* parent */),
	GATE_CAM_YA(CLK_CAM_YA_CAMTG, "cam_ya_camtg",
			"cksys_cam_ck"/* parent */, 2),
	GATE_CAM_YA_V(CLK_CAM_YA_CAMTG_CAM_RAW, "cam_ya_camtg_cam_raw",
			"cam_ya_camtg"/* parent */),
};

static const struct mtk_clk_desc cam_ya_mcd = {
	.clks = cam_ya_clks,
	.num_clks = CLK_CAM_YA_NR_CLK,
};

static const struct mtk_gate_regs cam_yb_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_YB(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_yb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_YB_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate cam_yb_clks[] = {
	GATE_CAM_YB(CLK_CAM_YB_LARBX, "cam_yb_larbx",
			"cksys_cam_ck"/* parent */, 0),
	GATE_CAM_YB_V(CLK_CAM_YB_LARBX_CAM_RAW, "cam_yb_larbx_cam_raw",
			"cam_yb_larbx"/* parent */),
	GATE_CAM_YB_V(CLK_CAM_YB_LARBX_SMI, "cam_yb_larbx_smi",
			"cam_yb_larbx"/* parent */),
	GATE_CAM_YB(CLK_CAM_YB_CAM, "cam_yb_cam",
			"cksys_cam_ck"/* parent */, 1),
	GATE_CAM_YB_V(CLK_CAM_YB_CAM_CAM_RAW, "cam_yb_cam_cam_raw",
			"cam_yb_cam"/* parent */),
	GATE_CAM_YB(CLK_CAM_YB_CAMTG, "cam_yb_camtg",
			"cksys_cam_ck"/* parent */, 2),
	GATE_CAM_YB_V(CLK_CAM_YB_CAMTG_CAM_RAW, "cam_yb_camtg_cam_raw",
			"cam_yb_camtg"/* parent */),
};

static const struct mtk_clk_desc cam_yb_mcd = {
	.clks = cam_yb_clks,
	.num_clks = CLK_CAM_YB_NR_CLK,
};

static const struct mtk_gate_regs cam_m0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs cam_m1_cg_regs = {
	.set_ofs = 0x50,
	.clr_ofs = 0x54,
	.sta_ofs = 0x4C,
};

#define GATE_CAM_M0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_m0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_M0_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

#define GATE_CAM_M1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_m1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_M1_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate cam_m_clks[] = {
	/* CAM_M0 */
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_MAIN, "cam_m_cam_main",
			"cksys_cam_ck"/* parent */, 0),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_MAIN_CAM_RAW, "cam_m_cam_main_cam_raw",
			"cam_m_cam_main"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_SUBA, "cam_m_cam_suba",
			"cksys_cam_ck"/* parent */, 1),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_SUBA_CAM_RAW, "cam_m_cam_suba_cam_raw",
			"cam_m_cam_suba"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_SUBB, "cam_m_cam_subb",
			"cksys_cam_ck"/* parent */, 2),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_SUBB_CAM_RAW, "cam_m_cam_subb_cam_raw",
			"cam_m_cam_subb"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_SUBC, "cam_m_cam_subc",
			"cksys_cam_ck"/* parent */, 3),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_SUBC_CAM_RAW, "cam_m_cam_subc_cam_raw",
			"cam_m_cam_subc"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_SENINF_TG_SUBA, "cam_m_cam_seninf_tg_suba",
			"cksys_cam_ck"/* parent */, 4),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_SENINF_TG_SUBA_CAM_RAW, "cam_m_cam_seninf_tg_suba_cam_raw",
			"cam_m_cam_seninf_tg_suba"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_SENINF_TG_SUBB, "cam_m_cam_seninf_tg_subb",
			"cksys_cam_ck"/* parent */, 5),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_SENINF_TG_SUBB_CAM_RAW, "cam_m_cam_seninf_tg_subb_cam_raw",
			"cam_m_cam_seninf_tg_subb"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_CAMTG, "cam_m_camtg",
			"cksys_cam_ck"/* parent */, 6),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAMTG_CAM_RAW, "cam_m_camtg_cam_raw",
			"cam_m_camtg"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_SENINF, "cam_m_seninf",
			"cksys_cam_ck"/* parent */, 7),
	GATE_CAM_M0_V(CLK_CAM_MAIN_SENINF_CAM_RAW, "cam_m_seninf_cam_raw",
			"cam_m_seninf"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_SUB_COMM_0C_0, "cam_m_sub_comm_0c_0",
			"cksys_cam_ck"/* parent */, 14),
	GATE_CAM_M0_V(CLK_CAM_MAIN_SUB_COMM_0C_0_CAM_RAW, "cam_m_sub_comm_0c_0_cam_raw",
			"cam_m_sub_comm_0c_0"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_SUB_COMM_0C_0_SMI, "cam_m_sub_comm_0c_0_smi",
			"cam_m_sub_comm_0c_0"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_SUB_COMM_1, "cam_m_sub_comm_1",
			"cksys_cam_ck"/* parent */, 15),
	GATE_CAM_M0_V(CLK_CAM_MAIN_SUB_COMM_1_CAM_RAW, "cam_m_sub_comm_1_cam_raw",
			"cam_m_sub_comm_1"/* parent */),
	GATE_CAM_M0_V(CLK_CAM_MAIN_SUB_COMM_1_SMI, "cam_m_sub_comm_1_smi",
			"cam_m_sub_comm_1"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_IPS, "cam_m_ips",
			"cksys_cam_ck"/* parent */, 16),
	GATE_CAM_M0_V(CLK_CAM_MAIN_IPS_CAM_RAW, "cam_m_ips_cam_raw",
			"cam_m_ips"/* parent */),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_ASG, "cam_m_cam_asg",
			"cksys_cam_ck"/* parent */, 21),
	GATE_CAM_M0_V(CLK_CAM_MAIN_CAM_ASG_CAM_RAW, "cam_m_cam_asg_cam_raw",
			"cam_m_cam_asg"/* parent */),
	/* CAM_M1 */
	GATE_CAM_M1(CLK_CAM_MAIN_CAM_QOF_CON_1, "cam_m_cam_qof_con_1",
			"cksys_cam_ck"/* parent */, 0),
	GATE_CAM_M1_V(CLK_CAM_MAIN_CAM_QOF_CON_1_CAM_RAW, "cam_m_cam_qof_con_1_cam_raw",
			"cam_m_cam_qof_con_1"/* parent */),
	GATE_CAM_M1(CLK_CAM_MAIN_CAM_BWR_CON_1, "cam_m_cam_bwr_con_1",
			"cksys_cam_ck"/* parent */, 1),
	GATE_CAM_M1_V(CLK_CAM_MAIN_CAM_BWR_CON_1_CAM_RAW, "cam_m_cam_bwr_con_1_cam_raw",
			"cam_m_cam_bwr_con_1"/* parent */),
	GATE_CAM_M1(CLK_CAM_MAIN_CAM_RTCQ_CON_1, "cam_m_cam_rtcq_con_1",
			"cksys_cam_ck"/* parent */, 2),
	GATE_CAM_M1_V(CLK_CAM_MAIN_CAM_RTCQ_CON_1_CAM_RAW, "cam_m_cam_rtcq_con_1_cam_raw",
			"cam_m_cam_rtcq_con_1"/* parent */),
	GATE_CAM_M1(CLK_CAM_MAIN_CAM_SDLCQ_CON_1, "cam_m_cam_sdlcq_con_1",
			"cksys_cam_ck"/* parent */, 3),
	GATE_CAM_M1_V(CLK_CAM_MAIN_CAM_SDLCQ_CON_1_CAM_RAW, "cam_m_cam_sdlcq_con_1_cam_raw",
			"cam_m_cam_sdlcq_con_1"/* parent */),
	GATE_CAM_M1(CLK_CAM_MAIN_CAM_WLA_CON_1, "cam_m_cam_wla_con_1",
			"cksys_cam_ck"/* parent */, 4),
	GATE_CAM_M1_V(CLK_CAM_MAIN_CAM_WLA_CON_1_CAM_RAW, "cam_m_cam_wla_con_1_cam_raw",
			"cam_m_cam_wla_con_1"/* parent */),
	GATE_CAM_M1(CLK_CAM_MAIN_CAM_DVC_CON_1, "cam_m_cam_dvc_con_1",
			"cksys_cam_ck"/* parent */, 5),
	GATE_CAM_M1_V(CLK_CAM_MAIN_CAM_DVC_CON_1_CAM_RAW, "cam_m_cam_dvc_con_1_cam_raw",
			"cam_m_cam_dvc_con_1"/* parent */),
	GATE_CAM_M1(CLK_CAM_MAIN_CAM_CVFS_CON_1, "cam_m_cam_cvfs_con_1",
			"cksys_cam_ck"/* parent */, 6),
	GATE_CAM_M1_V(CLK_CAM_MAIN_CAM_CVFS_CON_1_CAM_RAW, "cam_m_cam_cvfs_con_1_cam_raw",
			"cam_m_cam_cvfs_con_1"/* parent */),
};

static const struct mtk_clk_desc cam_m_mcd = {
	.clks = cam_m_clks,
	.num_clks = CLK_CAM_M_NR_CLK,
};

static const struct mtk_gate_regs cam_v_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_V(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_v_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_V_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate cam_v_clks[] = {
	GATE_CAM_V(CLK__VCORE, "_vcore",
			"cksys_mminfra_ck"/* parent */, 0),
	GATE_CAM_V_V(CLK__VCORE_CAM_RAW, "_vcore_cam_raw",
			"_vcore"/* parent */),
	GATE_CAM_V(CLK__26M, "_26m",
			"cksys_mminfra_ck"/* parent */, 1),
	GATE_CAM_V_V(CLK__26M_CAM_RAW, "_26m_cam_raw",
			"_26m"/* parent */),
	GATE_CAM_V(CLK__BLS_PART, "_bls_part",
			"cksys_mminfra_ck"/* parent */, 2),
	GATE_CAM_V_V(CLK__BLS_PART_CAM_RAW, "_bls_part_cam_raw",
			"_bls_part"/* parent */),
	GATE_CAM_V(CLK__BLS_FULL, "_bls_full",
			"cksys_mminfra_ck"/* parent */, 3),
	GATE_CAM_V_V(CLK__BLS_FULL_CAM_RAW, "_bls_full_cam_raw",
			"_bls_full"/* parent */),
	GATE_CAM_V(CLK__RESV0_GALS, "_resv0_GCON_0",
			"cksys_mminfra_ck"/* parent */, 4),
	GATE_CAM_V_V(CLK__RESV0_GALS_CAM_RAW, "_resv0_gcon_0_cam_raw",
			"_resv0_GCON_0"/* parent */),
	GATE_CAM_V(CLK__RESV1_GALS, "_resv1_GCON_0",
			"cksys_mminfra_ck"/* parent */, 5),
	GATE_CAM_V_V(CLK__RESV1_GALS_CAM_RAW, "_resv1_gcon_0_cam_raw",
			"_resv1_GCON_0"/* parent */),
	GATE_CAM_V(CLK__VCORE_CAM2MM0, "_vcore_cam2mm0",
			"cksys_mminfra_ck"/* parent */, 12),
	GATE_CAM_V_V(CLK__VCORE_CAM2MM0_CAM_RAW, "_vcore_cam2mm0_cam_raw",
			"_vcore_cam2mm0"/* parent */),
	GATE_CAM_V(CLK__VCORE_CAM2MM1, "_vcore_cam2mm1",
			"cksys_mminfra_ck"/* parent */, 13),
	GATE_CAM_V_V(CLK__VCORE_CAM2MM1_CAM_RAW, "_vcore_cam2mm1_cam_raw",
			"_vcore_cam2mm1"/* parent */),
};

static const struct mtk_clk_desc cam_v_mcd = {
	.clks = cam_v_clks,
	.num_clks = CLK_CAM_V_NR_CLK,
};

static const struct mtk_gate_regs dip_nr1_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_DIP_NR1_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dip_nr1_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_DIP_NR1_DIP1_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate dip_nr1_dip1_clks[] = {
	GATE_DIP_NR1_DIP1(CLK_DIP_NR1_DIP1_LARB, "dip_nr1_dip1_larb",
			"cksys_img1_ck"/* parent */, 0),
	GATE_DIP_NR1_DIP1_V(CLK_DIP_NR1_DIP1_LARB_CAMERA_P2, "dip_nr1_dip1_larb_camera_p2",
			"dip_nr1_dip1_larb"/* parent */),
	GATE_DIP_NR1_DIP1(CLK_DIP_NR1_DIP1_DIP_NR1, "dip_nr1_dip1_dip_nr1",
			"cksys_img1_ck"/* parent */, 1),
	GATE_DIP_NR1_DIP1_V(CLK_DIP_NR1_DIP1_DIP_NR1_CAMERA_P2, "dip_nr1_dip1_dip_nr1_camera_p2",
			"dip_nr1_dip1_dip_nr1"/* parent */),
};

static const struct mtk_clk_desc dip_nr1_dip1_mcd = {
	.clks = dip_nr1_dip1_clks,
	.num_clks = CLK_DIP_NR1_DIP1_NR_CLK,
};

static const struct mtk_gate_regs dip_nr2_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_DIP_NR2_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dip_nr2_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_DIP_NR2_DIP1_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate dip_nr2_dip1_clks[] = {
	GATE_DIP_NR2_DIP1(CLK_DIP_NR2_DIP1_DIP_NR, "dip_nr2_dip1_dip_nr",
			"cksys_img1_ck"/* parent */, 0),
	GATE_DIP_NR2_DIP1_V(CLK_DIP_NR2_DIP1_DIP_NR_CAMERA_P2, "dip_nr2_dip1_dip_nr_camera_p2",
			"dip_nr2_dip1_dip_nr"/* parent */),
	GATE_DIP_NR2_DIP1(CLK_DIP_NR2_DIP1_LARB15, "dip_nr2_dip1_larb15",
			"cksys_img1_ck"/* parent */, 1),
	GATE_DIP_NR2_DIP1_V(CLK_DIP_NR2_DIP1_LARB15_CAMERA_P2, "dip_nr2_dip1_larb15_camera_p2",
			"dip_nr2_dip1_larb15"/* parent */),
	GATE_DIP_NR2_DIP1(CLK_DIP_NR2_DIP1_LARB39, "dip_nr2_dip1_larb39",
			"cksys_img1_ck"/* parent */, 2),
	GATE_DIP_NR2_DIP1_V(CLK_DIP_NR2_DIP1_LARB39_CAMERA_P2, "dip_nr2_dip1_larb39_camera_p2",
			"dip_nr2_dip1_larb39"/* parent */),
};

static const struct mtk_clk_desc dip_nr2_dip1_mcd = {
	.clks = dip_nr2_dip1_clks,
	.num_clks = CLK_DIP_NR2_DIP1_NR_CLK,
};

static const struct mtk_gate_regs dip_top_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_DIP_TOP_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dip_top_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_DIP_TOP_DIP1_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate dip_top_dip1_clks[] = {
	GATE_DIP_TOP_DIP1(CLK_DIP_TOP_DIP1_DIP_TOP, "dip_dip1_dip_top",
			"cksys_img1_ck"/* parent */, 0),
	GATE_DIP_TOP_DIP1_V(CLK_DIP_TOP_DIP1_DIP_TOP_CAMERA_P2, "dip_dip1_dip_top_camera_p2",
			"dip_dip1_dip_top"/* parent */),
	GATE_DIP_TOP_DIP1(CLK_DIP_TOP_DIP1_DIP_TOP_GALS0, "dip_dip1_dip_gals0",
			"cksys_img1_ck"/* parent */, 1),
	GATE_DIP_TOP_DIP1_V(CLK_DIP_TOP_DIP1_DIP_TOP_GALS0_CAMERA_P2, "dip_dip1_dip_gals0_camera_p2",
			"dip_dip1_dip_gals0"/* parent */),
	GATE_DIP_TOP_DIP1(CLK_DIP_TOP_DIP1_DIP_TOP_GALS1, "dip_dip1_dip_gals1",
			"cksys_img1_ck"/* parent */, 2),
	GATE_DIP_TOP_DIP1_V(CLK_DIP_TOP_DIP1_DIP_TOP_GALS1_CAMERA_P2, "dip_dip1_dip_gals1_camera_p2",
			"dip_dip1_dip_gals1"/* parent */),
	GATE_DIP_TOP_DIP1(CLK_DIP_TOP_DIP1_DIP_TOP_GALS2, "dip_dip1_dip_gals2",
			"cksys_img1_ck"/* parent */, 3),
	GATE_DIP_TOP_DIP1_V(CLK_DIP_TOP_DIP1_DIP_TOP_GALS2_CAMERA_P2, "dip_dip1_dip_gals2_camera_p2",
			"dip_dip1_dip_gals2"/* parent */),
	GATE_DIP_TOP_DIP1(CLK_DIP_TOP_DIP1_DIP_TOP_GALS3, "dip_dip1_dip_gals3",
			"cksys_img1_ck"/* parent */, 4),
	GATE_DIP_TOP_DIP1_V(CLK_DIP_TOP_DIP1_DIP_TOP_GALS3_CAMERA_P2, "dip_dip1_dip_gals3_camera_p2",
			"dip_dip1_dip_gals3"/* parent */),
	GATE_DIP_TOP_DIP1(CLK_DIP_TOP_DIP1_LARB10, "dip_dip1_larb10",
			"cksys_img1_ck"/* parent */, 5),
	GATE_DIP_TOP_DIP1_V(CLK_DIP_TOP_DIP1_LARB10_CAMERA_P2, "dip_dip1_larb10_camera_p2",
			"dip_dip1_larb10"/* parent */),
	GATE_DIP_TOP_DIP1(CLK_DIP_TOP_DIP1_LARB15, "dip_dip1_larb15",
			"cksys_img1_ck"/* parent */, 6),
	GATE_DIP_TOP_DIP1_V(CLK_DIP_TOP_DIP1_LARB15_SMI, "dip_dip1_larb15_smi",
			"dip_dip1_larb15"/* parent */),
	GATE_DIP_TOP_DIP1(CLK_DIP_TOP_DIP1_LARB38, "dip_dip1_larb38",
			"cksys_img1_ck"/* parent */, 7),
	GATE_DIP_TOP_DIP1_V(CLK_DIP_TOP_DIP1_LARB38_SMI, "dip_dip1_larb38_smi",
			"dip_dip1_larb38"/* parent */),
	GATE_DIP_TOP_DIP1(CLK_DIP_TOP_DIP1_LARB39, "dip_dip1_larb39",
			"cksys_img1_ck"/* parent */, 8),
	GATE_DIP_TOP_DIP1_V(CLK_DIP_TOP_DIP1_LARB39_CAMERA_P2, "dip_dip1_larb39_camera_p2",
			"dip_dip1_larb39"/* parent */),
};

static const struct mtk_clk_desc dip_top_dip1_mcd = {
	.clks = dip_top_dip1_clks,
	.num_clks = CLK_DIP_TOP_DIP1_NR_CLK,
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
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
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
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate mm_clks[] = {
	/* MM0 */
	GATE_MM0(CLK_MM_DISP_OVL0_2L, "mm_disp_ovl0_2l",
			"cksys_disp0_ck"/* parent */, 0),
	GATE_MM0_V(CLK_MM_DISP_OVL0_2L_DISP, "mm_disp_ovl0_2l_disp",
			"mm_disp_ovl0_2l"/* parent */),
	GATE_MM0(CLK_MM_DISP_OVL1_2L, "mm_disp_ovl1_2l",
			"cksys_disp0_ck"/* parent */, 1),
	GATE_MM0_V(CLK_MM_DISP_OVL1_2L_DISP, "mm_disp_ovl1_2l_disp",
			"mm_disp_ovl1_2l"/* parent */),
	GATE_MM0(CLK_MM_DISP_OVL2_2L, "mm_disp_ovl2_2l",
			"cksys_disp0_ck"/* parent */, 2),
	GATE_MM0_V(CLK_MM_DISP_OVL2_2L_DISP, "mm_disp_ovl2_2l_disp",
			"mm_disp_ovl2_2l"/* parent */),
	GATE_MM0(CLK_MM_DISP_OVL3_2L, "mm_disp_ovl3_2l",
			"cksys_disp0_ck"/* parent */, 3),
	GATE_MM0_V(CLK_MM_DISP_OVL3_2L_DISP, "mm_disp_ovl3_2l_disp",
			"mm_disp_ovl3_2l"/* parent */),
	GATE_MM0(CLK_MM_DISP_RSZ1, "mm_disp_rsz1",
			"cksys_disp0_ck"/* parent */, 5),
	GATE_MM0_V(CLK_MM_DISP_RSZ1_DISP, "mm_disp_rsz1_disp",
			"mm_disp_rsz1"/* parent */),
	GATE_MM0(CLK_MM_DISP_RSZ0, "mm_disp_rsz0",
			"cksys_disp0_ck"/* parent */, 6),
	GATE_MM0_V(CLK_MM_DISP_RSZ0_DISP, "mm_disp_rsz0_disp",
			"mm_disp_rsz0"/* parent */),
	GATE_MM0(CLK_MM_DISP_TDSHP0, "mm_disp_tdshp0",
			"cksys_disp0_ck"/* parent */, 7),
	GATE_MM0_V(CLK_MM_DISP_TDSHP0_DISP, "mm_disp_tdshp0_disp",
			"mm_disp_tdshp0"/* parent */),
	GATE_MM0(CLK_MM_DISP_C3D0, "mm_disp_c3d0",
			"cksys_disp0_ck"/* parent */, 8),
	GATE_MM0_V(CLK_MM_DISP_C3D0_DISP, "mm_disp_c3d0_disp",
			"mm_disp_c3d0"/* parent */),
	GATE_MM0(CLK_MM_DISP_COLOR0, "mm_disp_color0",
			"cksys_disp0_ck"/* parent */, 9),
	GATE_MM0_V(CLK_MM_DISP_COLOR0_DISP, "mm_disp_color0_disp",
			"mm_disp_color0"/* parent */),
	GATE_MM0(CLK_MM_DISP_CCORR0, "mm_disp_ccorr0",
			"cksys_disp0_ck"/* parent */, 10),
	GATE_MM0_V(CLK_MM_DISP_CCORR0_DISP, "mm_disp_ccorr0_disp",
			"mm_disp_ccorr0"/* parent */),
	GATE_MM0(CLK_MM_DISP_CCORR1, "mm_disp_ccorr1",
			"cksys_disp0_ck"/* parent */, 11),
	GATE_MM0_V(CLK_MM_DISP_CCORR1_DISP, "mm_disp_ccorr1_disp",
			"mm_disp_ccorr1"/* parent */),
	GATE_MM0(CLK_MM_DISP_AAL0, "mm_disp_aal0",
			"cksys_disp0_ck"/* parent */, 12),
	GATE_MM0_V(CLK_MM_DISP_AAL0_DISP, "mm_disp_aal0_disp",
			"mm_disp_aal0"/* parent */),
	GATE_MM0(CLK_MM_DISP_GAMMA0, "mm_disp_gamma0",
			"cksys_disp0_ck"/* parent */, 13),
	GATE_MM0_V(CLK_MM_DISP_GAMMA0_DISP, "mm_disp_gamma0_disp",
			"mm_disp_gamma0"/* parent */),
	GATE_MM0(CLK_MM_DISP_POSTMASK0, "mm_disp_postmask0",
			"cksys_disp0_ck"/* parent */, 14),
	GATE_MM0_V(CLK_MM_DISP_POSTMASK0_DISP, "mm_disp_postmask0_disp",
			"mm_disp_postmask0"/* parent */),
	GATE_MM0(CLK_MM_DISP_DITHER0, "mm_disp_dither0",
			"cksys_disp0_ck"/* parent */, 15),
	GATE_MM0_V(CLK_MM_DISP_DITHER0_DISP, "mm_disp_dither0_disp",
			"mm_disp_dither0"/* parent */),
	GATE_MM0(CLK_MM_DISP_TDSHP1, "mm_disp_tdshp1",
			"cksys_disp0_ck"/* parent */, 16),
	GATE_MM0_V(CLK_MM_DISP_TDSHP1_DISP, "mm_disp_tdshp1_disp",
			"mm_disp_tdshp1"/* parent */),
	GATE_MM0(CLK_MM_DISP_C3D1, "mm_disp_c3d1",
			"cksys_disp0_ck"/* parent */, 17),
	GATE_MM0_V(CLK_MM_DISP_C3D1_DISP, "mm_disp_c3d1_disp",
			"mm_disp_c3d1"/* parent */),
	GATE_MM0(CLK_MM_DISP_CCORR2, "mm_disp_ccorr2",
			"cksys_disp0_ck"/* parent */, 18),
	GATE_MM0_V(CLK_MM_DISP_CCORR2_DISP, "mm_disp_ccorr2_disp",
			"mm_disp_ccorr2"/* parent */),
	GATE_MM0(CLK_MM_DISP_CCORR3, "mm_disp_ccorr3",
			"cksys_disp0_ck"/* parent */, 19),
	GATE_MM0_V(CLK_MM_DISP_CCORR3_DISP, "mm_disp_ccorr3_disp",
			"mm_disp_ccorr3"/* parent */),
	GATE_MM0(CLK_MM_DISP_GAMMA1, "mm_disp_gamma1",
			"cksys_disp0_ck"/* parent */, 20),
	GATE_MM0_V(CLK_MM_DISP_GAMMA1_DISP, "mm_disp_gamma1_disp",
			"mm_disp_gamma1"/* parent */),
	GATE_MM0(CLK_MM_DISP_DITHER1, "mm_disp_dither1",
			"cksys_disp0_ck"/* parent */, 21),
	GATE_MM0_V(CLK_MM_DISP_DITHER1_DISP, "mm_disp_dither1_disp",
			"mm_disp_dither1"/* parent */),
	GATE_MM0(CLK_MM_DISP_SPLITTER0, "mm_disp_splitter0",
			"cksys_disp0_ck"/* parent */, 22),
	GATE_MM0_V(CLK_MM_DISP_SPLITTER0_DISP, "mm_disp_splitter0_disp",
			"mm_disp_splitter0"/* parent */),
	GATE_MM0(CLK_MM_DISP_DSC_WRAP0, "mm_disp_dsc_wrap0",
			"cksys_disp0_ck"/* parent */, 23),
	GATE_MM0_V(CLK_MM_DISP_DSC_WRAP0_DISP, "mm_disp_dsc_wrap0_disp",
			"mm_disp_dsc_wrap0"/* parent */),
	GATE_MM0(CLK_MM_DISP_DSI0, "mm_CLK0",
			"cksys_disp0_ck"/* parent */, 24),
	GATE_MM0_V(CLK_MM_DISP_DSI0_DISP, "mm_clk0_disp",
			"mm_CLK0"/* parent */),
	GATE_MM0(CLK_MM_DISP_DSI1, "mm_CLK1",
			"cksys_disp0_ck"/* parent */, 25),
	GATE_MM0_V(CLK_MM_DISP_DSI1_DISP, "mm_clk1_disp",
			"mm_CLK1"/* parent */),
	GATE_MM0(CLK_MM_DISP_WDMA1, "mm_disp_wdma1",
			"cksys_disp0_ck"/* parent */, 26),
	GATE_MM0_V(CLK_MM_DISP_WDMA1_DISP, "mm_disp_wdma1_disp",
			"mm_disp_wdma1"/* parent */),
	GATE_MM0(CLK_MM_DISP_APB_BUS, "mm_disp_apb_bus",
			"cksys_disp0_ck"/* parent */, 27),
	GATE_MM0_V(CLK_MM_DISP_APB_BUS_DISP, "mm_disp_apb_bus_disp",
			"mm_disp_apb_bus"/* parent */),
	GATE_MM0(CLK_MM_DISP_FAKE_ENG0, "mm_disp_fake_eng0",
			"cksys_disp0_ck"/* parent */, 28),
	GATE_MM0_V(CLK_MM_DISP_FAKE_ENG0_DISP, "mm_disp_fake_eng0_disp",
			"mm_disp_fake_eng0"/* parent */),
	GATE_MM0(CLK_MM_DISP_FAKE_ENG1, "mm_disp_fake_eng1",
			"cksys_disp0_ck"/* parent */, 29),
	GATE_MM0_V(CLK_MM_DISP_FAKE_ENG1_DISP, "mm_disp_fake_eng1_disp",
			"mm_disp_fake_eng1"/* parent */),
	GATE_MM0(CLK_MM_DISP_MUTEX0, "mm_disp_mutex0",
			"cksys_disp0_ck"/* parent */, 30),
	GATE_MM0_V(CLK_MM_DISP_MUTEX0_DISP, "mm_disp_mutex0_disp",
			"mm_disp_mutex0"/* parent */),
	GATE_MM0(CLK_MM_SMI_COMMON, "mm_smi_common",
			"cksys_disp0_ck"/* parent */, 31),
	GATE_MM0_V(CLK_MM_SMI_COMMON_DISP, "mm_smi_common_disp",
			"mm_smi_common"/* parent */),
	GATE_MM0_V(CLK_MM_SMI_COMMON_SMI, "mm_smi_common_smi",
			"mm_smi_common"/* parent */),
	GATE_MM0_V(CLK_MM_SMI_COMMON_GENPD, "mm_smi_common_genpd",
			"mm_smi_common"/* parent */),

	/* MM1 */
	GATE_MM1(CLK_MM_DSI0, "mm_dsi0_ck",
			"cksys_disp0_ck"/* parent */, 0),
	GATE_MM1_V(CLK_MM_DSI0_DISP, "mm_dsi0_ck_disp",
			"mm_dsi0_ck"/* parent */),
	GATE_MM1(CLK_MM_DSI1, "mm_dsi1_ck",
			"cksys_disp0_ck"/* parent */, 1),
	GATE_MM1_V(CLK_MM_DSI1_DISP, "mm_dsi1_ck_disp",
			"mm_dsi1_ck"/* parent */),
	GATE_MM1(CLK_MM_26M, "mm_26m_ck",
			"cksys_disp0_ck"/* parent */, 11),
	GATE_MM1_V(CLK_MM_26M_DISP, "mm_26m_ck_disp",
			"mm_26m_ck"/* parent */),
};

static const struct mtk_clk_desc mm_mcd = {
	.clks = mm_clks,
	.num_clks = CLK_MM_NR_CLK,
};

static const struct mtk_gate_regs img0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs img1_cg_regs = {
	.set_ofs = 0x58,
	.clr_ofs = 0x5C,
	.sta_ofs = 0x54,
};

static const struct mtk_gate_regs img2_cg_regs = {
	.set_ofs = 0x10,
	.clr_ofs = 0x14,
	.sta_ofs = 0xC,
};

#define GATE_IMG0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &img0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMG0_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

#define GATE_IMG1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &img1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMG1_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

#define GATE_IMG2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &img2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMG2_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate img_clks[] = {
	/* IMG0 */
	GATE_IMG0(CLK_IMG_LARB9, "img_larb9",
			"cksys_img1_ck"/* parent */, 0),
	GATE_IMG0_V(CLK_IMG_LARB9_SMI, "img_larb9_smi",
			"img_larb9"/* parent */),
	GATE_IMG0(CLK_IMG_TRAW0, "img_traw0",
			"cksys_img1_ck"/* parent */, 1),
	GATE_IMG0_V(CLK_IMG_TRAW0_CAMERA_P2, "img_traw0_camera_p2",
			"img_traw0"/* parent */),
	GATE_IMG0(CLK_IMG_TRAW1, "img_traw1",
			"cksys_img1_ck"/* parent */, 2),
	GATE_IMG0_V(CLK_IMG_TRAW1_CAMERA_P2, "img_traw1_camera_p2",
			"img_traw1"/* parent */),
	GATE_IMG0(CLK_IMG_DIP0, "img_dip0",
			"cksys_img1_ck"/* parent */, 3),
	GATE_IMG0_V(CLK_IMG_DIP0_CAMERA_P2, "img_dip0_camera_p2",
			"img_dip0"/* parent */),
	GATE_IMG0(CLK_IMG_WPE0, "img_wpe0",
			"cksys_img1_ck"/* parent */, 4),
	GATE_IMG0_V(CLK_IMG_WPE0_CAMERA_P2, "img_wpe0_camera_p2",
			"img_wpe0"/* parent */),
	GATE_IMG0(CLK_IMG_IPE, "img_ipe",
			"cksys_ipe_ck"/* parent */, 5),
	GATE_IMG0_V(CLK_IMG_IPE_CAMERA_P2, "img_ipe_camera_p2",
			"img_ipe"/* parent */),
	GATE_IMG0(CLK_IMG_WPE1, "img_wpe1",
			"cksys_img1_ck"/* parent */, 6),
	GATE_IMG0_V(CLK_IMG_WPE1_CAMERA_P2, "img_wpe1_camera_p2",
			"img_wpe1"/* parent */),
	GATE_IMG0(CLK_IMG_WPE2, "img_wpe2",
			"cksys_img1_ck"/* parent */, 7),
	GATE_IMG0_V(CLK_IMG_WPE2_CAMERA_P2, "img_wpe2_camera_p2",
			"img_wpe2"/* parent */),
	GATE_IMG0(CLK_IMG_SUB_COMMON0, "img_sub_common0",
			"cksys_img1_ck"/* parent */, 20),
	GATE_IMG0_V(CLK_IMG_SUB_COMMON0_SMI, "img_sub_common0_smi",
			"img_sub_common0"/* parent */),
	GATE_IMG0(CLK_IMG_SUB_COMMON1, "img_sub_common1",
			"cksys_img1_ck"/* parent */, 21),
	GATE_IMG0_V(CLK_IMG_SUB_COMMON1_SMI, "img_sub_common1_smi",
			"img_sub_common1"/* parent */),
	GATE_IMG0(CLK_IMG_SUB_COMMON3, "img_sub_common3",
			"cksys_img1_ck"/* parent */, 23),
	GATE_IMG0_V(CLK_IMG_SUB_COMMON3_SMI, "img_sub_common3_smi",
			"img_sub_common3"/* parent */),
	GATE_IMG0(CLK_IMG_SUB_COMMON4, "img_sub_common4",
			"cksys_img1_ck"/* parent */, 24),
	GATE_IMG0_V(CLK_IMG_SUB_COMMON4_SMI, "img_sub_common4_smi",
			"img_sub_common4"/* parent */),
	/* IMG1 */
	GATE_IMG1(CLK_IMG_FDVT, "img_fdvt",
			"cksys_ipe_ck"/* parent */, 0),
	GATE_IMG1_V(CLK_IMG_FDVT_CAMERA_P2, "img_fdvt_camera_p2",
			"img_fdvt"/* parent */),
	GATE_IMG1(CLK_IMG_LARB12, "img_larb12",
			"cksys_ipe_ck"/* parent */, 3),
	GATE_IMG1_V(CLK_IMG_LARB12_SMI, "img_larb12_smi",
			"img_larb12"/* parent */),
	GATE_IMG1(CLK_IMG_ODPM26, "img_odpm26",
			"cksys_ocic_img_aov_26m_ck"/* parent */, 5),
	GATE_IMG1_V(CLK_IMG_ODPM26_CAMERA_P2, "img_odpm26_camera_p2",
			"img_odpm26"/* parent */),
	/* IMG2 */
	GATE_IMG2(CLK_IMG26, "img26",
			"cksys_ocic_img_aov_26m_ck"/* parent */, 10),
	GATE_IMG2_V(CLK_IMG26_CAMERA_P2, "img26_camera_p2",
			"img26"/* parent */),
};

static const struct mtk_clk_desc img_mcd = {
	.clks = img_clks,
	.num_clks = CLK_IMG_NR_CLK,
};

static const struct mtk_gate_regs img_v_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IMG_V(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &img_v_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMG_V_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate img_v_clks[] = {
	GATE_IMG_V(CLK_IMG_VCORE_SUB0, "img_vcore_sub0",
			"cksys_mminfra_ck"/* parent */, 2),
	GATE_IMG_V_V(CLK_IMG_VCORE_SUB0_SMI, "img_vcore_sub0_smi",
			"img_vcore_sub0"/* parent */),
	GATE_IMG_V(CLK_IMG_VCORE_SUB1, "img_vcore_sub1",
			"cksys_mminfra_ck"/* parent */, 3),
	GATE_IMG_V_V(CLK_IMG_VCORE_SUB1_SMI, "img_vcore_sub1_smi",
			"img_vcore_sub1"/* parent */),
	GATE_IMG_V(CLK_IMG_VCORE_IMG_26M, "img_vcore_img_26m",
			"cksys_ocic_img_aov_26m_ck"/* parent */, 5),
	GATE_IMG_V_V(CLK_IMG_VCORE_IMG_26M_CAMERA_P2, "img_vcore_img_26m_camera_p2",
			"img_vcore_img_26m"/* parent */),
};

static const struct mtk_clk_desc img_v_mcd = {
	.clks = img_v_clks,
	.num_clks = CLK_IMG_V_NR_CLK,
};

static const struct mtk_gate_regs imp_iic_top_wrap_s_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMP_IIC_TOP_WRAP_S(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imp_iic_top_wrap_s_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMP_IIC_TOP_WRAP_S_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate imp_iic_top_wrap_s_clks[] = {
	GATE_IMP_IIC_TOP_WRAP_S(CLK_IMP_IIC_TOP_WRAP_S_I2C2, "imp_iic_wrap_s_i2c2",
			"cksys_i2c_ck"/* parent */, 0),
	GATE_IMP_IIC_TOP_WRAP_S_V(CLK_IMP_IIC_TOP_WRAP_S_I2C2_I2C, "imp_iic_wrap_s_i2c2_i2c",
			"imp_iic_wrap_s_i2c2"/* parent */),
	GATE_IMP_IIC_TOP_WRAP_S(CLK_IMP_IIC_TOP_WRAP_S_I2C4, "imp_iic_wrap_s_i2c4",
			"cksys_i2c_ck"/* parent */, 1),
	GATE_IMP_IIC_TOP_WRAP_S_V(CLK_IMP_IIC_TOP_WRAP_S_I2C4_I2C, "imp_iic_wrap_s_i2c4_i2c",
			"imp_iic_wrap_s_i2c4"/* parent */),
	GATE_IMP_IIC_TOP_WRAP_S(CLK_IMP_IIC_TOP_WRAP_S_I2C7, "imp_iic_wrap_s_i2c7",
			"cksys_i2c_ck"/* parent */, 2),
	GATE_IMP_IIC_TOP_WRAP_S_V(CLK_IMP_IIC_TOP_WRAP_S_I2C7_I2C, "imp_iic_wrap_s_i2c7_i2c",
			"imp_iic_wrap_s_i2c7"/* parent */),
	GATE_IMP_IIC_TOP_WRAP_S(CLK_IMP_IIC_TOP_WRAP_S_I2C8, "imp_iic_wrap_s_i2c8",
			"cksys_i2c_ck"/* parent */, 3),
	GATE_IMP_IIC_TOP_WRAP_S_V(CLK_IMP_IIC_TOP_WRAP_S_I2C8_I2C, "imp_iic_wrap_s_i2c8_i2c",
			"imp_iic_wrap_s_i2c8"/* parent */),
	GATE_IMP_IIC_TOP_WRAP_S(CLK_IMP_IIC_TOP_WRAP_S_I2C9, "imp_iic_wrap_s_i2c9",
			"cksys_i2c_ck"/* parent */, 4),
	GATE_IMP_IIC_TOP_WRAP_S_V(CLK_IMP_IIC_TOP_WRAP_S_I2C9_I2C, "imp_iic_wrap_s_i2c9_i2c",
			"imp_iic_wrap_s_i2c9"/* parent */),
	GATE_IMP_IIC_TOP_WRAP_S(CLK_IMP_IIC_TOP_WRAP_S_I2C10, "imp_iic_wrap_s_i2c10",
			"cksys_i2c_ck"/* parent */, 5),
	GATE_IMP_IIC_TOP_WRAP_S_V(CLK_IMP_IIC_TOP_WRAP_S_I2C10_I2C, "imp_iic_wrap_s_i2c10_i2c",
			"imp_iic_wrap_s_i2c10"/* parent */),
	GATE_IMP_IIC_TOP_WRAP_S(CLK_IMP_IIC_TOP_WRAP_S_I2C11, "imp_iic_wrap_s_i2c11",
			"cksys_i2c_ck"/* parent */, 6),
	GATE_IMP_IIC_TOP_WRAP_S_V(CLK_IMP_IIC_TOP_WRAP_S_I2C11_I2C, "imp_iic_wrap_s_i2c11_i2c",
			"imp_iic_wrap_s_i2c11"/* parent */),
	GATE_IMP_IIC_TOP_WRAP_S(CLK_IMP_IIC_TOP_WRAP_S_I2C12, "imp_iic_wrap_s_i2c12",
			"cksys_i2c_ck"/* parent */, 7),
	GATE_IMP_IIC_TOP_WRAP_S_V(CLK_IMP_IIC_TOP_WRAP_S_I2C12_I2C, "imp_iic_wrap_s_i2c12_i2c",
			"imp_iic_wrap_s_i2c12"/* parent */),
};

static const struct mtk_clk_desc imp_iic_top_wrap_s_mcd = {
	.clks = imp_iic_top_wrap_s_clks,
	.num_clks = CLK_IMP_IIC_TOP_WRAP_S_NR_CLK,
};

static const struct mtk_gate_regs imp_iic_top_wrap_w_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMP_IIC_TOP_WRAP_W(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imp_iic_top_wrap_w_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMP_IIC_TOP_WRAP_W_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate imp_iic_top_wrap_w_clks[] = {
	GATE_IMP_IIC_TOP_WRAP_W(CLK_IMP_IIC_TOP_WRAP_W_I2C0, "imp_iic_wrap_w_i2c0",
			"cksys_i2c_5_ck"/* parent */, 0),
	GATE_IMP_IIC_TOP_WRAP_W_V(CLK_IMP_IIC_TOP_WRAP_W_I2C0_I2C, "imp_iic_wrap_w_i2c0_i2c",
			"imp_iic_wrap_w_i2c0"/* parent */),
	GATE_IMP_IIC_TOP_WRAP_W(CLK_IMP_IIC_TOP_WRAP_W_I2C1, "imp_iic_wrap_w_i2c1",
			"cksys_i2c_5_ck"/* parent */, 1),
	GATE_IMP_IIC_TOP_WRAP_W_V(CLK_IMP_IIC_TOP_WRAP_W_I2C1_I2C, "imp_iic_wrap_w_i2c1_i2c",
			"imp_iic_wrap_w_i2c1"/* parent */),
	GATE_IMP_IIC_TOP_WRAP_W(CLK_IMP_IIC_TOP_WRAP_W_I2C3, "imp_iic_wrap_w_i2c3",
			"cksys_i2c_5_ck"/* parent */, 2),
	GATE_IMP_IIC_TOP_WRAP_W_V(CLK_IMP_IIC_TOP_WRAP_W_I2C3_I2C, "imp_iic_wrap_w_i2c3_i2c",
			"imp_iic_wrap_w_i2c3"/* parent */),
	GATE_IMP_IIC_TOP_WRAP_W(CLK_IMP_IIC_TOP_WRAP_W_I2C5, "imp_iic_wrap_w_i2c5",
			"cksys_i2c_5_ck"/* parent */, 3),
	GATE_IMP_IIC_TOP_WRAP_W_V(CLK_IMP_IIC_TOP_WRAP_W_I2C5_I2C, "imp_iic_wrap_w_i2c5_i2c",
			"imp_iic_wrap_w_i2c5"/* parent */),
	GATE_IMP_IIC_TOP_WRAP_W(CLK_IMP_IIC_TOP_WRAP_W_I2C6, "imp_iic_wrap_w_i2c6",
			"cksys_i2c_5_ck"/* parent */, 4),
	GATE_IMP_IIC_TOP_WRAP_W_V(CLK_IMP_IIC_TOP_WRAP_W_I2C6_I2C, "imp_iic_wrap_w_i2c6_i2c",
			"imp_iic_wrap_w_i2c6"/* parent */),
};

static const struct mtk_clk_desc imp_iic_top_wrap_w_mcd = {
	.clks = imp_iic_top_wrap_w_clks,
	.num_clks = CLK_IMP_IIC_TOP_WRAP_W_NR_CLK,
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
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
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
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
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
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
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
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate infra_infracfg_ao_reg_clks[] = {
	/* INFRA_INFRACFG_AO_REG0 */
	GATE_INFRA_INFRACFG_AO_REG0(CLK_INFRA_AO_REG_CCIF1_AP, "infra_ao_ccif1_ap",
			"cksys_axi_post_ck"/* parent */, 12),
	GATE_INFRA_INFRACFG_AO_REG0_V(CLK_INFRA_AO_REG_CCIF1_AP_CCCI, "infra_ao_ccif1_ap_ccci",
			"infra_ao_ccif1_ap"/* parent */),
	GATE_INFRA_INFRACFG_AO_REG0(CLK_INFRA_AO_REG_CCIF1_MD, "infra_ao_ccif1_md",
			"cksys_axi_post_ck"/* parent */, 13),
	GATE_INFRA_INFRACFG_AO_REG0_V(CLK_INFRA_AO_REG_CCIF1_MD_CCCI, "infra_ao_ccif1_md_ccci",
			"infra_ao_ccif1_md"/* parent */),
	GATE_INFRA_INFRACFG_AO_REG0(CLK_INFRA_AO_REG_CCIF_AP, "infra_ao_ccif_ap",
			"cksys_axi_post_ck"/* parent */, 23),
	GATE_INFRA_INFRACFG_AO_REG0_V(CLK_INFRA_AO_REG_CCIF_AP_CCCI, "infra_ao_ccif_ap_ccci",
			"infra_ao_ccif_ap"/* parent */),
	GATE_INFRA_INFRACFG_AO_REG0(CLK_INFRA_AO_REG_CCIF_MD, "infra_ao_ccif_md",
			"cksys_axi_post_ck"/* parent */, 26),
	GATE_INFRA_INFRACFG_AO_REG0_V(CLK_INFRA_AO_REG_CCIF_MD_CCCI, "infra_ao_ccif_md_ccci",
			"infra_ao_ccif_md"/* parent */),
	/* INFRA_INFRACFG_AO_REG1 */
	GATE_INFRA_INFRACFG_AO_REG1(CLK_INFRA_AO_REG_CLDMA_BCLK, "infra_ao_cldmabclk",
			"cksys_axi_post_ck"/* parent */, 3),
	GATE_INFRA_INFRACFG_AO_REG1_V(CLK_INFRA_AO_REG_CLDMA_BCLK_DPMAIF, "infra_ao_cldmabclk_dpmaif",
			"infra_ao_cldmabclk"/* parent */),
	/* INFRA_INFRACFG_AO_REG2 */
	GATE_INFRA_INFRACFG_AO_REG2(CLK_INFRA_AO_REG_CCIF5_MD, "infra_ao_ccif5_md",
			"cksys_axi_post_ck"/* parent */, 10),
	GATE_INFRA_INFRACFG_AO_REG2_V(CLK_INFRA_AO_REG_CCIF5_MD_CCCI, "infra_ao_ccif5_md_ccci",
			"infra_ao_ccif5_md"/* parent */),
	GATE_INFRA_INFRACFG_AO_REG2(CLK_INFRA_AO_REG_CCIF2_AP, "infra_ao_ccif2_ap",
			"cksys_axi_post_ck"/* parent */, 16),
	GATE_INFRA_INFRACFG_AO_REG2_V(CLK_INFRA_AO_REG_CCIF2_AP_CCCI, "infra_ao_ccif2_ap_ccci",
			"infra_ao_ccif2_ap"/* parent */),
	GATE_INFRA_INFRACFG_AO_REG2(CLK_INFRA_AO_REG_CCIF2_MD, "infra_ao_ccif2_md",
			"cksys_axi_post_ck"/* parent */, 17),
	GATE_INFRA_INFRACFG_AO_REG2_V(CLK_INFRA_AO_REG_CCIF2_MD_CCCI, "infra_ao_ccif2_md_ccci",
			"infra_ao_ccif2_md"/* parent */),
	GATE_INFRA_INFRACFG_AO_REG2(CLK_INFRA_AO_REG_DPMAIF_MAIN, "infra_ao_dpmaif_main",
			"cksys_dpmaif_main_ck"/* parent */, 26),
	GATE_INFRA_INFRACFG_AO_REG2_V(CLK_INFRA_AO_REG_DPMAIF_MAIN_DPMAIF, "infra_ao_dpmaif_main_dpmaif",
			"infra_ao_dpmaif_main"/* parent */),
	GATE_INFRA_INFRACFG_AO_REG2(CLK_INFRA_AO_REG_CCIF4_MD, "infra_ao_ccif4_md",
			"cksys_axi_post_ck"/* parent */, 29),
	GATE_INFRA_INFRACFG_AO_REG2_V(CLK_INFRA_AO_REG_CCIF4_MD_CCCI, "infra_ao_ccif4_md_ccci",
			"infra_ao_ccif4_md"/* parent */),
	/* INFRA_INFRACFG_AO_REG3 */
	GATE_INFRA_INFRACFG_AO_REG3(CLK_INFRA_AO_REG_RG_MMW_DPMAIF26M_CK, "infra_ao_dpmaif_26m_set",
			"cksys_f26m_ck"/* parent */, 17),
	GATE_INFRA_INFRACFG_AO_REG3_V(CLK_INFRA_AO_REG_RG_MMW_DPMAIF26M_CK_DPMAIF, "infra_ao_dpmaif_26m_set_dpmaif",
			"infra_ao_dpmaif_26m_set"/* parent */),
};

static const struct mtk_clk_desc infra_infracfg_ao_reg_mcd = {
	.clks = infra_infracfg_ao_reg_clks,
	.num_clks = CLK_INFRA_INFRACFG_AO_REG_NR_CLK,
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
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
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
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate mdp_clks[] = {
	/* MDP0 */
	GATE_MDP0(CLK_MDP_MUTEX0, "mdp_mutex0",
			"cksys_disp0_ck"/* parent */, 0),
	GATE_MDP0_V(CLK_MDP_MUTEX0_MML, "mdp_mutex0_mml",
			"mdp_mutex0"/* parent */),
	GATE_MDP0(CLK_MDP_APB_BUS, "mdp_apb_bus",
			"cksys_disp0_ck"/* parent */, 1),
	GATE_MDP0_V(CLK_MDP_APB_BUS_MML, "mdp_apb_bus_mml",
			"mdp_apb_bus"/* parent */),
	GATE_MDP0(CLK_MDP_SMI0, "mdp_smi0",
			"cksys_disp0_ck"/* parent */, 2),
	GATE_MDP0_V(CLK_MDP_SMI0_MML, "mdp_smi0_mml",
			"mdp_smi0"/* parent */),
	GATE_MDP0_V(CLK_MDP_SMI0_SMI, "mdp_smi0_smi",
			"mdp_smi0"/* parent */),
	GATE_MDP0_V(CLK_MDP_SMI0_GENPD, "mdp_smi0_genpd",
			"mdp_smi0"/* parent */),
	GATE_MDP0(CLK_MDP_RDMA0, "mdp_rdma0",
			"cksys_disp0_ck"/* parent */, 3),
	GATE_MDP0_V(CLK_MDP_RDMA0_MML, "mdp_rdma0_mml",
			"mdp_rdma0"/* parent */),
	GATE_MDP0(CLK_MDP_FG0, "mdp_fg0",
			"cksys_disp0_ck"/* parent */, 4),
	GATE_MDP0_V(CLK_MDP_FG0_MML, "mdp_fg0_mml",
			"mdp_fg0"/* parent */),
	GATE_MDP0(CLK_MDP_HDR0, "mdp_hdr0",
			"cksys_disp0_ck"/* parent */, 5),
	GATE_MDP0_V(CLK_MDP_HDR0_MML, "mdp_hdr0_mml",
			"mdp_hdr0"/* parent */),
	GATE_MDP0(CLK_MDP_AAL0, "mdp_aal0",
			"cksys_disp0_ck"/* parent */, 6),
	GATE_MDP0_V(CLK_MDP_AAL0_MML, "mdp_aal0_mml",
			"mdp_aal0"/* parent */),
	GATE_MDP0(CLK_MDP_RSZ0, "mdp_rsz0",
			"cksys_disp0_ck"/* parent */, 7),
	GATE_MDP0_V(CLK_MDP_RSZ0_MML, "mdp_rsz0_mml",
			"mdp_rsz0"/* parent */),
	GATE_MDP0(CLK_MDP_TDSHP0, "mdp_tdshp0",
			"cksys_disp0_ck"/* parent */, 8),
	GATE_MDP0_V(CLK_MDP_TDSHP0_MML, "mdp_tdshp0_mml",
			"mdp_tdshp0"/* parent */),
	GATE_MDP0(CLK_MDP_COLOR0, "mdp_color0",
			"cksys_disp0_ck"/* parent */, 9),
	GATE_MDP0_V(CLK_MDP_COLOR0_MML, "mdp_color0_mml",
			"mdp_color0"/* parent */),
	GATE_MDP0(CLK_MDP_WROT0, "mdp_wrot0",
			"cksys_disp0_ck"/* parent */, 10),
	GATE_MDP0_V(CLK_MDP_WROT0_MML, "mdp_wrot0_mml",
			"mdp_wrot0"/* parent */),
	GATE_MDP0(CLK_MDP_FAKE_ENG0, "mdp_fake_eng0",
			"cksys_disp0_ck"/* parent */, 11),
	GATE_MDP0_V(CLK_MDP_FAKE_ENG0_MML, "mdp_fake_eng0_mml",
			"mdp_fake_eng0"/* parent */),
	GATE_MDP0(CLK_MDP_DLI_ASYNC0, "mdp_dli_async0",
			"cksys_disp0_ck"/* parent */, 12),
	GATE_MDP0_V(CLK_MDP_DLI_ASYNC0_MML, "mdp_dli_async0_mml",
			"mdp_dli_async0"/* parent */),
	GATE_MDP0(CLK_MDP_DLI_ASYNC1, "mdp_dli_async1",
			"cksys_disp0_ck"/* parent */, 13),
	GATE_MDP0_V(CLK_MDP_DLI_ASYNC1_MML, "mdp_dli_async1_mml",
			"mdp_dli_async1"/* parent */),
	GATE_MDP0(CLK_MDP_RDMA1, "mdp_rdma1",
			"cksys_disp0_ck"/* parent */, 15),
	GATE_MDP0_V(CLK_MDP_RDMA1_MML, "mdp_rdma1_mml",
			"mdp_rdma1"/* parent */),
	GATE_MDP0(CLK_MDP_FG1, "mdp_fg1",
			"cksys_disp0_ck"/* parent */, 16),
	GATE_MDP0_V(CLK_MDP_FG1_MML, "mdp_fg1_mml",
			"mdp_fg1"/* parent */),
	GATE_MDP0(CLK_MDP_HDR1, "mdp_hdr1",
			"cksys_disp0_ck"/* parent */, 17),
	GATE_MDP0_V(CLK_MDP_HDR1_MML, "mdp_hdr1_mml",
			"mdp_hdr1"/* parent */),
	GATE_MDP0(CLK_MDP_AAL1, "mdp_aal1",
			"cksys_disp0_ck"/* parent */, 18),
	GATE_MDP0_V(CLK_MDP_AAL1_MML, "mdp_aal1_mml",
			"mdp_aal1"/* parent */),
	GATE_MDP0(CLK_MDP_RSZ1, "mdp_rsz1",
			"cksys_disp0_ck"/* parent */, 19),
	GATE_MDP0_V(CLK_MDP_RSZ1_MML, "mdp_rsz1_mml",
			"mdp_rsz1"/* parent */),
	GATE_MDP0(CLK_MDP_TDSHP1, "mdp_tdshp1",
			"cksys_disp0_ck"/* parent */, 20),
	GATE_MDP0_V(CLK_MDP_TDSHP1_MML, "mdp_tdshp1_mml",
			"mdp_tdshp1"/* parent */),
	GATE_MDP0(CLK_MDP_COLOR1, "mdp_color1",
			"cksys_disp0_ck"/* parent */, 21),
	GATE_MDP0_V(CLK_MDP_COLOR1_MML, "mdp_color1_mml",
			"mdp_color1"/* parent */),
	GATE_MDP0(CLK_MDP_WROT1, "mdp_wrot1",
			"cksys_disp0_ck"/* parent */, 22),
	GATE_MDP0_V(CLK_MDP_WROT1_MML, "mdp_wrot1_mml",
			"mdp_wrot1"/* parent */),
	GATE_MDP0(CLK_MDP_RSZ2, "mdp_rsz2",
			"cksys_disp0_ck"/* parent */, 24),
	GATE_MDP0_V(CLK_MDP_RSZ2_MML, "mdp_rsz2_mml",
			"mdp_rsz2"/* parent */),
	GATE_MDP0(CLK_MDP_WROT2, "mdp_wrot2",
			"cksys_disp0_ck"/* parent */, 25),
	GATE_MDP0_V(CLK_MDP_WROT2_MML, "mdp_wrot2_mml",
			"mdp_wrot2"/* parent */),
	GATE_MDP0(CLK_MDP_DLO_ASYNC0, "mdp_dlo_async0",
			"cksys_disp0_ck"/* parent */, 26),
	GATE_MDP0_V(CLK_MDP_DLO_ASYNC0_MML, "mdp_dlo_async0_mml",
			"mdp_dlo_async0"/* parent */),
	GATE_MDP0(CLK_MDP_RSZ3, "mdp_rsz3",
			"cksys_disp0_ck"/* parent */, 28),
	GATE_MDP0_V(CLK_MDP_RSZ3_MML, "mdp_rsz3_mml",
			"mdp_rsz3"/* parent */),
	GATE_MDP0(CLK_MDP_WROT3, "mdp_wrot3",
			"cksys_disp0_ck"/* parent */, 29),
	GATE_MDP0_V(CLK_MDP_WROT3_MML, "mdp_wrot3_mml",
			"mdp_wrot3"/* parent */),
	GATE_MDP0(CLK_MDP_DLO_ASYNC1, "mdp_dlo_async1",
			"cksys_disp0_ck"/* parent */, 30),
	GATE_MDP0_V(CLK_MDP_DLO_ASYNC1_MML, "mdp_dlo_async1_mml",
			"mdp_dlo_async1"/* parent */),
	GATE_MDP0(CLK_MDP_HRE_TOP_MDPSYS, "mdp_hre_mdpsys",
			"cksys_disp0_ck"/* parent */, 31),
	GATE_MDP0_V(CLK_MDP_HRE_TOP_MDPSYS_MML, "mdp_hre_mdpsys_mml",
			"mdp_hre_mdpsys"/* parent */),
	/* MDP1 */
	GATE_MDP1(CLK_MDP_FMM_IMG_DL_ASYNC0, "mdp_fmm_img_dl_async0",
			"cksys_disp0_ck"/* parent */, 0),
	GATE_MDP1_V(CLK_MDP_FMM_IMG_DL_ASYNC0_MML, "mdp_fmm_img_dl_async0_mml",
			"mdp_fmm_img_dl_async0"/* parent */),
	GATE_MDP1(CLK_MDP_FMM_IMG_DL_ASYNC1, "mdp_fmm_img_dl_async1",
			"cksys_disp0_ck"/* parent */, 1),
	GATE_MDP1_V(CLK_MDP_FMM_IMG_DL_ASYNC1_MML, "mdp_fmm_img_dl_async1_mml",
			"mdp_fmm_img_dl_async1"/* parent */),
	GATE_MDP1(CLK_MDP_FIMG_IMG_DL_ASYNC0, "mdp_fimg_img_dl_async0",
			"cksys_disp0_ck"/* parent */, 2),
	GATE_MDP1_V(CLK_MDP_FIMG_IMG_DL_ASYNC0_MML, "mdp_fimg_img_dl_async0_mml",
			"mdp_fimg_img_dl_async0"/* parent */),
	GATE_MDP1(CLK_MDP_FIMG_IMG_DL_ASYNC1, "mdp_fimg_img_dl_async1",
			"cksys_disp0_ck"/* parent */, 3),
	GATE_MDP1_V(CLK_MDP_FIMG_IMG_DL_ASYNC1_MML, "mdp_fimg_img_dl_async1_mml",
			"mdp_fimg_img_dl_async1"/* parent */),
};

static const struct mtk_clk_desc mdp_mcd = {
	.clks = mdp_clks,
	.num_clks = CLK_MDP_NR_CLK,
};

static const struct mtk_gate_regs mipi_csi_top_ctrl_0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x4,
	.sta_ofs = 0x4,
};

#define GATE_MIPI_CSI_TOP_CTRL_0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mipi_csi_top_ctrl_0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_MIPI_CSI_TOP_CTRL_0_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate mipi_csi_top_ctrl_0_clks[] = {
	GATE_MIPI_CSI_TOP_CTRL_0(CLK_MIPI_CSI_RG_CSIRX_CSI_CK0_EN, "mipi_csi_ck0_en",
			"cksys_seninf_ck"/* parent */, 0),
	GATE_MIPI_CSI_TOP_CTRL_0_V(CLK_MIPI_CSI_RG_CSIRX_CSI_CK0_EN_CAMERA_SENINF, "mipi_csi_ck0_en_camera_seninf",
			"mipi_csi_ck0_en"/* parent */),
	GATE_MIPI_CSI_TOP_CTRL_0(CLK_MIPI_CSI_RG_CSIRX_CSI_CK1_EN, "mipi_csi_ck1_en",
			"cksys_seninf1_ck"/* parent */, 4),
	GATE_MIPI_CSI_TOP_CTRL_0_V(CLK_MIPI_CSI_RG_CSIRX_CSI_CK1_EN_CAMERA_SENINF, "mipi_csi_ck1_en_camera_seninf",
			"mipi_csi_ck1_en"/* parent */),
	GATE_MIPI_CSI_TOP_CTRL_0(CLK_MIPI_CSI_RG_CSIRX_CSI_CK2_EN, "mipi_csi_ck2_en",
			"cksys_seninf2_ck"/* parent */, 8),
	GATE_MIPI_CSI_TOP_CTRL_0_V(CLK_MIPI_CSI_RG_CSIRX_CSI_CK2_EN_CAMERA_SENINF, "mipi_csi_ck2_en_camera_seninf",
			"mipi_csi_ck2_en"/* parent */),
	GATE_MIPI_CSI_TOP_CTRL_0(CLK_MIPI_CSI_RG_CSIRX_CSI_CK3_EN, "mipi_csi_ck3_en",
			"cksys_seninf3_ck"/* parent */, 12),
	GATE_MIPI_CSI_TOP_CTRL_0_V(CLK_MIPI_CSI_RG_CSIRX_CSI_CK3_EN_CAMERA_SENINF, "mipi_csi_ck3_en_camera_seninf",
			"mipi_csi_ck3_en"/* parent */),
};

static const struct mtk_clk_desc mipi_csi_top_ctrl_0_mcd = {
	.clks = mipi_csi_top_ctrl_0_clks,
	.num_clks = CLK_MIPI_CSI_TOP_CTRL_0_NR_CLK,
};

static const struct mtk_gate_regs mminfra_config0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mminfra_config1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

#define GATE_MMINFRA_CONFIG0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mminfra_config0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MMINFRA_CONFIG0_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

#define GATE_MMINFRA_CONFIG1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mminfra_config1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MMINFRA_CONFIG1_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate mminfra_config_clks[] = {
	/* MMINFRA_CONFIG0 */
	GATE_MMINFRA_CONFIG0(CLK_MMINFRA_GCE_D, "mminfra_gce_d",
			"cksys_mminfra_ck"/* parent */, 0),
	GATE_MMINFRA_CONFIG0_V(CLK_MMINFRA_GCE_D_GCE, "mminfra_gce_d_gce",
			"mminfra_gce_d"/* parent */),
	GATE_MMINFRA_CONFIG0(CLK_MMINFRA_GCE_M, "mminfra_gce_m",
			"cksys_mminfra_ck"/* parent */, 1),
	GATE_MMINFRA_CONFIG0_V(CLK_MMINFRA_GCE_M_GCE, "mminfra_gce_m_gce",
			"mminfra_gce_m"/* parent */),
	GATE_MMINFRA_CONFIG0(CLK_MMINFRA_GCE_M2, "mminfra_gce_m2",
			"cksys_mminfra_ck"/* parent */, 4),
	GATE_MMINFRA_CONFIG0_V(CLK_MMINFRA_GCE_M2_GCE, "mminfra_gce_m2_gce",
			"mminfra_gce_m2"/* parent */),
	/* MMINFRA_CONFIG1 */
	GATE_MMINFRA_CONFIG1(CLK_MMINFRA_GCE_26M, "mminfra_gce_26m",
			"cksys_mminfra_ck"/* parent */, 17),
	GATE_MMINFRA_CONFIG1_V(CLK_MMINFRA_GCE_26M_GCE, "mminfra_gce_26m_gce",
			"mminfra_gce_26m"/* parent */),
};

static const struct mtk_clk_desc mminfra_config_mcd = {
	.clks = mminfra_config_clks,
	.num_clks = CLK_MMINFRA_CONFIG_NR_CLK,
};

static const struct mtk_gate_regs pericfg_ao_reg0_cg_regs = {
	.set_ofs = 0x24,
	.clr_ofs = 0x28,
	.sta_ofs = 0x10,
};

static const struct mtk_gate_regs pericfg_ao_reg1_cg_regs = {
	.set_ofs = 0x2C,
	.clr_ofs = 0x30,
	.sta_ofs = 0x14,
};

static const struct mtk_gate_regs pericfg_ao_reg2_cg_regs = {
	.set_ofs = 0x34,
	.clr_ofs = 0x38,
	.sta_ofs = 0x18,
};

#define GATE_PERICFG_AO_REG0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &pericfg_ao_reg0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PERICFG_AO_REG0_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

#define GATE_PERICFG_AO_REG1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &pericfg_ao_reg1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PERICFG_AO_REG1_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

#define GATE_PERICFG_AO_REG2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &pericfg_ao_reg2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PERICFG_AO_REG2_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate pericfg_ao_reg_clks[] = {
	/* PERICFG_AO_REG0 */
	GATE_PERICFG_AO_REG0(CLK_PERICFG_AO_REG_PERI_UART0, "peri_ao_uart0",
			"cksys_uart_ck"/* parent */, 0),
	GATE_PERICFG_AO_REG0_V(CLK_PERICFG_AO_REG_PERI_UART0_UART, "peri_ao_uart0_uart",
			"peri_ao_uart0"/* parent */),
	GATE_PERICFG_AO_REG0(CLK_PERICFG_AO_REG_PERI_UART1, "peri_ao_uart1",
			"cksys_uart_ck"/* parent */, 1),
	GATE_PERICFG_AO_REG0_V(CLK_PERICFG_AO_REG_PERI_UART1_UART, "peri_ao_uart1_uart",
			"peri_ao_uart1"/* parent */),
	GATE_PERICFG_AO_REG0(CLK_PERICFG_AO_REG_PERI_UART2, "peri_ao_uart2",
			"cksys_uart_ck"/* parent */, 2),
	GATE_PERICFG_AO_REG0_V(CLK_PERICFG_AO_REG_PERI_UART2_UART, "peri_ao_uart2_uart",
			"peri_ao_uart2"/* parent */),
	GATE_PERICFG_AO_REG0(CLK_PERICFG_AO_REG_PERI_UART3, "peri_ao_uart3",
			"cksys_uart_ck"/* parent */, 3),
	GATE_PERICFG_AO_REG0_V(CLK_PERICFG_AO_REG_PERI_UART3_UART, "peri_ao_uart3_uart",
			"peri_ao_uart3"/* parent */),
	GATE_PERICFG_AO_REG0(CLK_PERICFG_AO_REG_PERI_UART4, "peri_ao_uart4",
			"cksys_uart_ck"/* parent */, 4),
	GATE_PERICFG_AO_REG0_V(CLK_PERICFG_AO_REG_PERI_UART4_UART, "peri_ao_uart4_uart",
			"peri_ao_uart4"/* parent */),
	GATE_PERICFG_AO_REG0(CLK_PERICFG_AO_REG_PERI_UART5, "peri_ao_uart5",
			"cksys_uart_ck"/* parent */, 5),
	GATE_PERICFG_AO_REG0_V(CLK_PERICFG_AO_REG_PERI_UART5_UART, "peri_ao_uart5_uart",
			"peri_ao_uart5"/* parent */),
	GATE_PERICFG_AO_REG0(CLK_PERICFG_AO_REG_PERI_PWM_H, "peri_ao_pwm_h",
			"cksys_axi_peri_ck"/* parent */, 6),
	GATE_PERICFG_AO_REG0_V(CLK_PERICFG_AO_REG_PERI_PWM_H_PWM, "peri_ao_pwm_h_pwm",
			"peri_ao_pwm_h"/* parent */),
	GATE_PERICFG_AO_REG0(CLK_PERICFG_AO_REG_PERI_PWM_B, "peri_ao_pwm_b",
			"cksys_axi_peri_ck"/* parent */, 7),
	GATE_PERICFG_AO_REG0_V(CLK_PERICFG_AO_REG_PERI_PWM_B_PWM, "peri_ao_pwm_b_pwm",
			"peri_ao_pwm_b"/* parent */),
	GATE_PERICFG_AO_REG0(CLK_PERICFG_AO_REG_PERI_DISP_PWM0, "peri_ao_disp_pwm0",
			"cksys_disp_pwm_ck"/* parent */, 8),
	GATE_PERICFG_AO_REG0_V(CLK_PERICFG_AO_REG_PERI_DISP_PWM0_DISP_PWM_0, "peri_ao_disp_pwm0_disp_pwm_0",
			"peri_ao_disp_pwm0"/* parent */),
	GATE_PERICFG_AO_REG0(CLK_PERICFG_AO_REG_PERI_DISP_PWM1, "peri_ao_disp_pwm1",
			"cksys_disp_pwm_ck"/* parent */, 9),
	GATE_PERICFG_AO_REG0_V(CLK_PERICFG_AO_REG_PERI_DISP_PWM1_DISP_PWM_1, "peri_ao_disp_pwm1_disp_pwm_1",
			"peri_ao_disp_pwm1"/* parent */),
	GATE_PERICFG_AO_REG0(CLK_PERICFG_AO_REG_PERI_SPI0_B, "peri_ao_spi0_b",
			"cksys_spi0_ck"/* parent */, 10),
	GATE_PERICFG_AO_REG0_V(CLK_PERICFG_AO_REG_PERI_SPI0_B_SPI, "peri_ao_spi0_b_spi",
			"peri_ao_spi0_b"/* parent */),
	GATE_PERICFG_AO_REG0(CLK_PERICFG_AO_REG_PERI_SPI1_B, "peri_ao_spi1_b",
			"cksys_spi1_ck"/* parent */, 11),
	GATE_PERICFG_AO_REG0_V(CLK_PERICFG_AO_REG_PERI_SPI1_B_SPI, "peri_ao_spi1_b_spi",
			"peri_ao_spi1_b"/* parent */),
	GATE_PERICFG_AO_REG0(CLK_PERICFG_AO_REG_PERI_SPI2_B, "peri_ao_spi2_b",
			"cksys_spi2_ck"/* parent */, 12),
	GATE_PERICFG_AO_REG0_V(CLK_PERICFG_AO_REG_PERI_SPI2_B_SPI, "peri_ao_spi2_b_spi",
			"peri_ao_spi2_b"/* parent */),
	GATE_PERICFG_AO_REG0(CLK_PERICFG_AO_REG_PERI_SPI3_B, "peri_ao_spi3_b",
			"cksys_spi3_ck"/* parent */, 13),
	GATE_PERICFG_AO_REG0_V(CLK_PERICFG_AO_REG_PERI_SPI3_B_SPI, "peri_ao_spi3_b_spi",
			"peri_ao_spi3_b"/* parent */),
	GATE_PERICFG_AO_REG0(CLK_PERICFG_AO_REG_PERI_SPI4_B, "peri_ao_spi4_b",
			"cksys_spi4_ck"/* parent */, 14),
	GATE_PERICFG_AO_REG0_V(CLK_PERICFG_AO_REG_PERI_SPI4_B_SPI, "peri_ao_spi4_b_spi",
			"peri_ao_spi4_b"/* parent */),
	GATE_PERICFG_AO_REG0(CLK_PERICFG_AO_REG_PERI_SPI5_B, "peri_ao_spi5_b",
			"cksys_spi5_ck"/* parent */, 15),
	GATE_PERICFG_AO_REG0_V(CLK_PERICFG_AO_REG_PERI_SPI5_B_SPI, "peri_ao_spi5_b_spi",
			"peri_ao_spi5_b"/* parent */),
	GATE_PERICFG_AO_REG0(CLK_PERICFG_AO_REG_PERI_SPI6_B, "peri_ao_spi6_b",
			"cksys_spi6_ck"/* parent */, 16),
	GATE_PERICFG_AO_REG0_V(CLK_PERICFG_AO_REG_PERI_SPI6_B_SPI, "peri_ao_spi6_b_spi",
			"peri_ao_spi6_b"/* parent */),
	GATE_PERICFG_AO_REG0(CLK_PERICFG_AO_REG_PERI_SPI7_B, "peri_ao_spi7_b",
			"cksys_spi7_ck"/* parent */, 17),
	GATE_PERICFG_AO_REG0_V(CLK_PERICFG_AO_REG_PERI_SPI7_B_SPI, "peri_ao_spi7_b_spi",
			"peri_ao_spi7_b"/* parent */),
	GATE_PERICFG_AO_REG0(CLK_PERICFG_AO_REG_PERI_DMA_B, "peri_ao_dma_b",
			"cksys_axi_peri_ck"/* parent */, 27),
	GATE_PERICFG_AO_REG0_V(CLK_PERICFG_AO_REG_PERI_DMA_B_UART, "peri_ao_dma_b_uart",
			"peri_ao_dma_b"/* parent */),
	GATE_PERICFG_AO_REG0_V(CLK_PERICFG_AO_REG_PERI_DMA_B_I2C, "peri_ao_dma_b_i2c",
			"peri_ao_dma_b"/* parent */),
	/* PERICFG_AO_REG1 */
	GATE_PERICFG_AO_REG1(CLK_PERICFG_AO_REG_PERI_MSDC1, "peri_ao_msdc1",
			"cksys_msdc30_1_ck"/* parent */, 7),
	GATE_PERICFG_AO_REG1_V(CLK_PERICFG_AO_REG_PERI_MSDC1_MSDC1, "peri_ao_msdc1_msdc1",
			"peri_ao_msdc1"/* parent */),
	GATE_PERICFG_AO_REG1(CLK_PERICFG_AO_REG_PERI_MSDC1_DIV, "peri_ao_msdc1_div",
			"cksys_msdc30_1_ck"/* parent */, 8),
	GATE_PERICFG_AO_REG1_V(CLK_PERICFG_AO_REG_PERI_MSDC1_DIV_MSDC1, "peri_ao_msdc1_div_msdc1",
			"peri_ao_msdc1_div"/* parent */),
	GATE_PERICFG_AO_REG1(CLK_PERICFG_AO_REG_PERI_MSDC1_MST_F, "peri_ao_msdc1_mst_f",
			"cksys_msdc30_1_ck"/* parent */, 9),
	GATE_PERICFG_AO_REG1_V(CLK_PERICFG_AO_REG_PERI_MSDC1_MST_F_MSDC1, "peri_ao_msdc1_mst_f_msdc1",
			"peri_ao_msdc1_mst_f"/* parent */),
	GATE_PERICFG_AO_REG1(CLK_PERICFG_AO_REG_PERI_MSDC1_SLV_H, "peri_ao_msdc1_slv_h",
			"cksys_msdc30_1_ck"/* parent */, 10),
	GATE_PERICFG_AO_REG1_V(CLK_PERICFG_AO_REG_PERI_MSDC1_SLV_H_MSDC1, "peri_ao_msdc1_slv_h_msdc1",
			"peri_ao_msdc1_slv_h"/* parent */),
	/* PERICFG_AO_REG2 */
	GATE_PERICFG_AO_REG2(CLK_PERICFG_AO_REG_PERI_AUDIO0, "peri_ao_audio0",
			"cksys_audio_h_ck"/* parent */, 0),
	GATE_PERICFG_AO_REG2_V(CLK_PERICFG_AO_REG_PERI_AUDIO0_AUDIO, "peri_ao_audio0_audio",
			"peri_ao_audio0"/* parent */),
	GATE_PERICFG_AO_REG2(CLK_PERICFG_AO_REG_PERI_AUDIO1, "peri_ao_audio1",
			"cksys_audio_h_ck"/* parent */, 1),
	GATE_PERICFG_AO_REG2_V(CLK_PERICFG_AO_REG_PERI_AUDIO1_AUDIO, "peri_ao_audio1_audio",
			"peri_ao_audio1"/* parent */),
	GATE_PERICFG_AO_REG2(CLK_PERICFG_AO_REG_PERI_AUDIO2, "peri_ao_audio2",
			"cksys_audio_h_ck"/* parent */, 2),
	GATE_PERICFG_AO_REG2_V(CLK_PERICFG_AO_REG_PERI_AUDIO2_AUDIO, "peri_ao_audio2_audio",
			"peri_ao_audio2"/* parent */),
};

static const struct mtk_clk_desc pericfg_ao_reg_mcd = {
	.clks = pericfg_ao_reg_clks,
	.num_clks = CLK_PERICFG_AO_REG_NR_CLK,
};

static const struct mtk_gate_regs traw_cap_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_TRAW_CAP_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &traw_cap_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_TRAW_CAP_DIP1_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate traw_cap_dip1_clks[] = {
	GATE_TRAW_CAP_DIP1(CLK_TRAW_CAP_DIP1_TRAW_CAP, "traw__dip1_cap",
			"cksys_img1_ck"/* parent */, 0),
	GATE_TRAW_CAP_DIP1_V(CLK_TRAW_CAP_DIP1_TRAW_CAP_CAMERA_P2, "traw__dip1_cap_camera_p2",
			"traw__dip1_cap"/* parent */),
};

static const struct mtk_clk_desc traw_cap_dip1_mcd = {
	.clks = traw_cap_dip1_clks,
	.num_clks = CLK_TRAW_CAP_DIP1_NR_CLK,
};

static const struct mtk_gate_regs traw_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_TRAW_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &traw_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_TRAW_DIP1_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate traw_dip1_clks[] = {
	GATE_TRAW_DIP1(CLK_TRAW_DIP1_LARB28, "traw_dip1_larb28",
			"cksys_img1_ck"/* parent */, 0),
	GATE_TRAW_DIP1_V(CLK_TRAW_DIP1_LARB28_SMI, "traw_dip1_larb28_smi",
			"traw_dip1_larb28"/* parent */),
	GATE_TRAW_DIP1(CLK_TRAW_DIP1_LARB40, "traw_dip1_larb40",
			"cksys_img1_ck"/* parent */, 1),
	GATE_TRAW_DIP1_V(CLK_TRAW_DIP1_LARB40_CAMERA_P2, "traw_dip1_larb40_camera_p2",
			"traw_dip1_larb40"/* parent */),
	GATE_TRAW_DIP1(CLK_TRAW_DIP1_TRAW, "traw_dip1_traw",
			"cksys_img1_ck"/* parent */, 2),
	GATE_TRAW_DIP1_V(CLK_TRAW_DIP1_TRAW_CAMERA_P2, "traw_dip1_traw_camera_p2",
			"traw_dip1_traw"/* parent */),
};

static const struct mtk_clk_desc traw_dip1_mcd = {
	.clks = traw_dip1_clks,
	.num_clks = CLK_TRAW_DIP1_NR_CLK,
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
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate ufsao_clks[] = {
	GATE_UFSAO(CLK_UFSAO_UNIPRO_TX_SYM, "ufsao_unipro_tx_sym",
			"cksys_f26m_ck"/* parent */, 0),
	GATE_UFSAO_V(CLK_UFSAO_UNIPRO_TX_SYM_UFS, "ufsao_unipro_tx_sym_ufs",
			"ufsao_unipro_tx_sym"/* parent */),
	GATE_UFSAO(CLK_UFSAO_UNIPRO_RX_SYM0, "ufsao_unipro_rx_sym0",
			"cksys_f26m_ck"/* parent */, 1),
	GATE_UFSAO_V(CLK_UFSAO_UNIPRO_RX_SYM0_UFS, "ufsao_unipro_rx_sym0_ufs",
			"ufsao_unipro_rx_sym0"/* parent */),
	GATE_UFSAO(CLK_UFSAO_UNIPRO_RX_SYM1, "ufsao_unipro_rx_sym1",
			"cksys_f26m_ck"/* parent */, 2),
	GATE_UFSAO_V(CLK_UFSAO_UNIPRO_RX_SYM1_UFS, "ufsao_unipro_rx_sym1_ufs",
			"ufsao_unipro_rx_sym1"/* parent */),
	GATE_UFSAO(CLK_UFSAO_UNIPRO_SYS, "ufsao_unipro_sys",
			"cksys_ufs_ck"/* parent */, 3),
	GATE_UFSAO_V(CLK_UFSAO_UNIPRO_SYS_UFS, "ufsao_unipro_sys_ufs",
			"ufsao_unipro_sys"/* parent */),
	GATE_UFSAO(CLK_UFSAO_UNIPRO_SAP_CFG, "ufsao_unipro_sap_cfg",
			"cksys_f26m_ck"/* parent */, 8),
	GATE_UFSAO_V(CLK_UFSAO_UNIPRO_SAP_CFG_UFS, "ufsao_unipro_sap_cfg_ufs",
			"ufsao_unipro_sap_cfg"/* parent */),
	GATE_UFSAO(CLK_UFSAO_UFS_PHY_TOP_AHB_S_BUS, "ufsao_ufs_phy_ahb_s_bus",
			"cksys_haxi_ufs_ck"/* parent */, 9),
	GATE_UFSAO_V(CLK_UFSAO_UFS_PHY_TOP_AHB_S_BUS_UFS, "ufsao_ufs_phy_ahb_s_bus_ufs",
			"ufsao_ufs_phy_ahb_s_bus"/* parent */),
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
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate ufspdn_clks[] = {
	GATE_UFSPDN(CLK_UFSPDN_UFSHCI_UFS, "ufspdn_ufshci_ufs",
			"cksys_ufs_ck"/* parent */, 0),
	GATE_UFSPDN_V(CLK_UFSPDN_UFSHCI_UFS_UFS, "ufspdn_ufshci_ufs_ufs",
			"ufspdn_ufshci_ufs"/* parent */),
	GATE_UFSPDN(CLK_UFSPDN_UFSHCI_AES, "ufspdn_ufshci_aes",
			"cksys_aes_ufsfde_ck"/* parent */, 1),
	GATE_UFSPDN_V(CLK_UFSPDN_UFSHCI_AES_UFS, "ufspdn_ufshci_aes_ufs",
			"ufspdn_ufshci_aes"/* parent */),
	GATE_UFSPDN(CLK_UFSPDN_UFSHCI_UFS_AHB, "ufspdn_ufshci_ufs_ahb",
			"cksys_haxi_ufs_ck"/* parent */, 3),
	GATE_UFSPDN_V(CLK_UFSPDN_UFSHCI_UFS_AHB_UFS, "ufspdn_ufshci_ufs_ahb_ufs",
			"ufspdn_ufshci_ufs_ahb"/* parent */),
	GATE_UFSPDN(CLK_UFSPDN_UFSHCI_UFS_AXI, "ufspdn_ufshci_ufs_axi",
			"cksys_mem_sub_ufs_ck"/* parent */, 5),
	GATE_UFSPDN_V(CLK_UFSPDN_UFSHCI_UFS_AXI_UFS, "ufspdn_ufshci_ufs_axi_ufs",
			"ufspdn_ufshci_ufs_axi"/* parent */),
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
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x8,
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
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
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
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate vde2_clks[] = {
	/* VDE20 */
	GATE_VDE20(CLK_VDE2_VDEC_CKEN, "vde2_vdec_cken",
			"cksys_vdec_ck"/* parent */, 0),
	GATE_VDE20_V(CLK_VDE2_VDEC_CKEN_VDEC, "vde2_vdec_cken_vdec",
			"vde2_vdec_cken"/* parent */),
	GATE_VDE20(CLK_VDE2_VDEC_ACTIVE, "vde2_vdec_active",
			"cksys_vdec_ck"/* parent */, 4),
	GATE_VDE20_V(CLK_VDE2_VDEC_ACTIVE_VDEC, "vde2_vdec_active_vdec",
			"vde2_vdec_active"/* parent */),
	GATE_VDE20(CLK_VDE2_VDEC_CKEN_ENG, "vde2_vdec_cken_eng",
			"cksys_vdec_ck"/* parent */, 8),
	GATE_VDE20_V(CLK_VDE2_VDEC_CKEN_ENG_VDEC, "vde2_vdec_cken_eng_vdec",
			"vde2_vdec_cken_eng"/* parent */),
	/* VDE21 */
	GATE_VDE21(CLK_VDE2_LARB1_CKEN, "vde2_larb1_cken",
			"cksys_vdec_ck"/* parent */, 0),
	GATE_VDE21_V(CLK_VDE2_LARB1_CKEN_SMI, "vde2_larb1_cken_smi",
			"vde2_larb1_cken"/* parent */),
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
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate ven1_clks[] = {
	GATE_VEN1(CLK_VEN1_CKE0_LARB, "ven1_larb",
			"cksys_venc_ck"/* parent */, 0),
	GATE_VEN1_V(CLK_VEN1_CKE0_LARB_SMI, "ven1_larb_smi",
			"ven1_larb"/* parent */),
	GATE_VEN1(CLK_VEN1_CKE1_VENC, "ven1_venc",
			"cksys_venc_ck"/* parent */, 4),
	GATE_VEN1_V(CLK_VEN1_CKE1_VENC_VENC, "ven1_venc_venc",
			"ven1_venc"/* parent */),
	GATE_VEN1(CLK_VEN1_CKE2_JPGENC, "ven1_jpgenc",
			"cksys_venc_ck"/* parent */, 8),
	GATE_VEN1_V(CLK_VEN1_CKE2_JPGENC_JPGENC, "ven1_jpgenc_jpgenc",
			"ven1_jpgenc"/* parent */),
	GATE_VEN1(CLK_VEN1_CKE5_GALS, "ven1_gals",
			"cksys_venc_ck"/* parent */, 28),
	GATE_VEN1_V(CLK_VEN1_CKE5_GALS_VENC, "ven1_gals_venc",
			"ven1_gals"/* parent */),
};

static const struct mtk_clk_desc ven1_mcd = {
	.clks = ven1_clks,
	.num_clks = CLK_VEN1_NR_CLK,
};

static const struct mtk_gate_regs wpe_eis_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_WPE_EIS_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &wpe_eis_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_WPE_EIS_DIP1_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate wpe_eis_dip1_clks[] = {
	GATE_WPE_EIS_DIP1(CLK_WPE_EIS_DIP1_LARB_U0, "wpe_eis_dip1_larb_u0",
			"cksys_img1_ck"/* parent */, 0),
	GATE_WPE_EIS_DIP1_V(CLK_WPE_EIS_DIP1_LARB_U0_SMI, "wpe_eis_dip1_larb_u0_smi",
			"wpe_eis_dip1_larb_u0"/* parent */),
	GATE_WPE_EIS_DIP1(CLK_WPE_EIS_DIP1_LARB_U1, "wpe_eis_dip1_larb_u1",
			"cksys_img1_ck"/* parent */, 1),
	GATE_WPE_EIS_DIP1_V(CLK_WPE_EIS_DIP1_LARB_U1_CAMERA_P2, "wpe_eis_dip1_larb_u1_camera_p2",
			"wpe_eis_dip1_larb_u1"/* parent */),
	GATE_WPE_EIS_DIP1(CLK_WPE_EIS_DIP1_GALS_U0, "wpe_eis_dip1_gals_u0",
			"cksys_img1_ck"/* parent */, 2),
	GATE_WPE_EIS_DIP1_V(CLK_WPE_EIS_DIP1_GALS_U0_CAMERA_P2, "wpe_eis_dip1_gals_u0_camera_p2",
			"wpe_eis_dip1_gals_u0"/* parent */),
	GATE_WPE_EIS_DIP1(CLK_WPE_EIS_DIP1_GALS_U1, "wpe_eis_dip1_gals_u1",
			"cksys_img1_ck"/* parent */, 3),
	GATE_WPE_EIS_DIP1_V(CLK_WPE_EIS_DIP1_GALS_U1_CAMERA_P2, "wpe_eis_dip1_gals_u1_camera_p2",
			"wpe_eis_dip1_gals_u1"/* parent */),
	GATE_WPE_EIS_DIP1(CLK_WPE_EIS_DIP1_WPE_MACRO, "wpe_eis_dip1_wpe_macro",
			"cksys_img1_ck"/* parent */, 4),
	GATE_WPE_EIS_DIP1_V(CLK_WPE_EIS_DIP1_WPE_MACRO_CAMERA_P2, "wpe_eis_dip1_wpe_macro_camera_p2",
			"wpe_eis_dip1_wpe_macro"/* parent */),
	GATE_WPE_EIS_DIP1(CLK_WPE_EIS_DIP1_WPE, "wpe_eis_dip1_wpe",
			"cksys_img1_ck"/* parent */, 5),
	GATE_WPE_EIS_DIP1_V(CLK_WPE_EIS_DIP1_WPE_CAMERA_P2, "wpe_eis_dip1_wpe_camera_p2",
			"wpe_eis_dip1_wpe"/* parent */),
	GATE_WPE_EIS_DIP1(CLK_WPE_EIS_DIP1_PQDIP, "wpe_eis_dip1_pqdip",
			"cksys_img1_ck"/* parent */, 6),
	GATE_WPE_EIS_DIP1_V(CLK_WPE_EIS_DIP1_PQDIP_CAMERA_P2, "wpe_eis_dip1_pqdip_camera_p2",
			"wpe_eis_dip1_pqdip"/* parent */),
	GATE_WPE_EIS_DIP1(CLK_WPE_EIS_DIP1_PQDIP_DMA, "wpe_eis_dip1_pqdip_dma",
			"cksys_img1_ck"/* parent */, 7),
	GATE_WPE_EIS_DIP1_V(CLK_WPE_EIS_DIP1_PQDIP_DMA_CAMERA_P2, "wpe_eis_dip1_pqdip_dma_camera_p2",
			"wpe_eis_dip1_pqdip_dma"/* parent */),
	GATE_WPE_EIS_DIP1(CLK_WPE_EIS_DIP1_OMC, "wpe_eis_dip1_omc",
			"cksys_img1_ck"/* parent */, 8),
	GATE_WPE_EIS_DIP1_V(CLK_WPE_EIS_DIP1_OMC_CAMERA_P2, "wpe_eis_dip1_omc_camera_p2",
			"wpe_eis_dip1_omc"/* parent */),
	GATE_WPE_EIS_DIP1(CLK_WPE_EIS_DIP1_DWPE, "wpe_eis_dip1_dwpe",
			"cksys_img1_ck"/* parent */, 13),
	GATE_WPE_EIS_DIP1_V(CLK_WPE_EIS_DIP1_DWPE_CAMERA_P2, "wpe_eis_dip1_dwpe_camera_p2",
			"wpe_eis_dip1_dwpe"/* parent */),
	GATE_WPE_EIS_DIP1(CLK_WPE_EIS_DIP1_ME, "wpe_eis_dip1_me",
			"cksys_img1_ck"/* parent */, 14),
	GATE_WPE_EIS_DIP1_V(CLK_WPE_EIS_DIP1_ME_CAMERA_P2, "wpe_eis_dip1_me_camera_p2",
			"wpe_eis_dip1_me"/* parent */),
	GATE_WPE_EIS_DIP1(CLK_WPE_EIS_DIP1_MMG, "wpe_eis_dip1_mmg",
			"cksys_img1_ck"/* parent */, 15),
	GATE_WPE_EIS_DIP1_V(CLK_WPE_EIS_DIP1_MMG_CAMERA_P2, "wpe_eis_dip1_mmg_camera_p2",
			"wpe_eis_dip1_mmg"/* parent */),
	GATE_WPE_EIS_DIP1(CLK_WPE_EIS_DIP1_WPE_26M_EN, "wpe_eis_dip1_wpe_26m",
			"cksys_img1_ck"/* parent */, 16),
	GATE_WPE_EIS_DIP1_V(CLK_WPE_EIS_DIP1_WPE_26M_EN_CAMERA_P2, "wpe_eis_dip1_wpe_26m_camera_p2",
			"wpe_eis_dip1_wpe_26m"/* parent */),
};

static const struct mtk_clk_desc wpe_eis_dip1_mcd = {
	.clks = wpe_eis_dip1_clks,
	.num_clks = CLK_WPE_EIS_DIP1_NR_CLK,
};

static const struct mtk_gate_regs wpe_tnr_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_WPE_TNR_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &wpe_tnr_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_WPE_TNR_DIP1_V(_id, _name, _parent) {	\
		.id = _id,			  \
		.name = _name,			  \
		.parent_name = _parent,		 \
	}

static const struct mtk_gate wpe_tnr_dip1_clks[] = {
	GATE_WPE_TNR_DIP1(CLK_WPE_TNR_DIP1_LARB_U0, "wpe_tnr_dip1_larb_u0",
			"cksys_img1_ck"/* parent */, 0),
	GATE_WPE_TNR_DIP1_V(CLK_WPE_TNR_DIP1_LARB_U0_SMI, "wpe_tnr_dip1_larb_u0_smi",
			"wpe_tnr_dip1_larb_u0"/* parent */),
	GATE_WPE_TNR_DIP1(CLK_WPE_TNR_DIP1_LARB_U1, "wpe_tnr_dip1_larb_u1",
			"cksys_img1_ck"/* parent */, 1),
	GATE_WPE_TNR_DIP1_V(CLK_WPE_TNR_DIP1_LARB_U1_CAMERA_P2, "wpe_tnr_dip1_larb_u1_camera_p2",
			"wpe_tnr_dip1_larb_u1"/* parent */),
	GATE_WPE_TNR_DIP1(CLK_WPE_TNR_DIP1_GALS_U0, "wpe_tnr_dip1_gals_u0",
			"cksys_img1_ck"/* parent */, 2),
	GATE_WPE_TNR_DIP1_V(CLK_WPE_TNR_DIP1_GALS_U0_CAMERA_P2, "wpe_tnr_dip1_gals_u0_camera_p2",
			"wpe_tnr_dip1_gals_u0"/* parent */),
	GATE_WPE_TNR_DIP1(CLK_WPE_TNR_DIP1_GALS_U1, "wpe_tnr_dip1_gals_u1",
			"cksys_img1_ck"/* parent */, 3),
	GATE_WPE_TNR_DIP1_V(CLK_WPE_TNR_DIP1_GALS_U1_CAMERA_P2, "wpe_tnr_dip1_gals_u1_camera_p2",
			"wpe_tnr_dip1_gals_u1"/* parent */),
	GATE_WPE_TNR_DIP1(CLK_WPE_TNR_DIP1_WPE_MACRO, "wpe_tnr_dip1_wpe_macro",
			"cksys_img1_ck"/* parent */, 4),
	GATE_WPE_TNR_DIP1_V(CLK_WPE_TNR_DIP1_WPE_MACRO_CAMERA_P2, "wpe_tnr_dip1_wpe_macro_camera_p2",
			"wpe_tnr_dip1_wpe_macro"/* parent */),
	GATE_WPE_TNR_DIP1(CLK_WPE_TNR_DIP1_WPE, "wpe_tnr_dip1_wpe",
			"cksys_img1_ck"/* parent */, 5),
	GATE_WPE_TNR_DIP1_V(CLK_WPE_TNR_DIP1_WPE_CAMERA_P2, "wpe_tnr_dip1_wpe_camera_p2",
			"wpe_tnr_dip1_wpe"/* parent */),
	GATE_WPE_TNR_DIP1(CLK_WPE_TNR_DIP1_PQDIP, "wpe_tnr_dip1_pqdip",
			"cksys_img1_ck"/* parent */, 6),
	GATE_WPE_TNR_DIP1_V(CLK_WPE_TNR_DIP1_PQDIP_CAMERA_P2, "wpe_tnr_dip1_pqdip_camera_p2",
			"wpe_tnr_dip1_pqdip"/* parent */),
	GATE_WPE_TNR_DIP1(CLK_WPE_TNR_DIP1_PQDIP_DMA, "wpe_tnr_dip1_pqdip_dma",
			"cksys_img1_ck"/* parent */, 7),
	GATE_WPE_TNR_DIP1_V(CLK_WPE_TNR_DIP1_PQDIP_DMA_CAMERA_P2, "wpe_tnr_dip1_pqdip_dma_camera_p2",
			"wpe_tnr_dip1_pqdip_dma"/* parent */),
	GATE_WPE_TNR_DIP1(CLK_WPE_TNR_DIP1_OMC, "wpe_tnr_dip1_omc",
			"cksys_img1_ck"/* parent */, 8),
	GATE_WPE_TNR_DIP1_V(CLK_WPE_TNR_DIP1_OMC_CAMERA_P2, "wpe_tnr_dip1_omc_camera_p2",
			"wpe_tnr_dip1_omc"/* parent */),
	GATE_WPE_TNR_DIP1(CLK_WPE_TNR_DIP1_DWPE, "wpe_tnr_dip1_dwpe",
			"cksys_img1_ck"/* parent */, 13),
	GATE_WPE_TNR_DIP1_V(CLK_WPE_TNR_DIP1_DWPE_CAMERA_P2, "wpe_tnr_dip1_dwpe_camera_p2",
			"wpe_tnr_dip1_dwpe"/* parent */),
	GATE_WPE_TNR_DIP1(CLK_WPE_TNR_DIP1_ME, "wpe_tnr_dip1_me",
			"cksys_img1_ck"/* parent */, 14),
	GATE_WPE_TNR_DIP1_V(CLK_WPE_TNR_DIP1_ME_CAMERA_P2, "wpe_tnr_dip1_me_camera_p2",
			"wpe_tnr_dip1_me"/* parent */),
	GATE_WPE_TNR_DIP1(CLK_WPE_TNR_DIP1_MMG, "wpe_tnr_dip1_mmg",
			"cksys_img1_ck"/* parent */, 15),
	GATE_WPE_TNR_DIP1_V(CLK_WPE_TNR_DIP1_MMG_CAMERA_P2, "wpe_tnr_dip1_mmg_camera_p2",
			"wpe_tnr_dip1_mmg"/* parent */),
};

static const struct mtk_clk_desc wpe_tnr_dip1_mcd = {
	.clks = wpe_tnr_dip1_clks,
	.num_clks = CLK_WPE_TNR_DIP1_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6881_cg[] = {
	{
		.compatible = "mediatek,mt6881-audiosys",
		.data = &afe_mcd,
	}, {
		.compatible = "mediatek,mt6881-camsys_mraw",
		.data = &cam_mr_mcd,
	}, {
		.compatible = "mediatek,mt6881-camsys_rawa",
		.data = &cam_ra_mcd,
	}, {
		.compatible = "mediatek,mt6881-camsys_rawb",
		.data = &cam_rb_mcd,
	}, {
		.compatible = "mediatek,mt6881-camsys_rmsa",
		.data = &camsys_rmsa_mcd,
	}, {
		.compatible = "mediatek,mt6881-camsys_rmsb",
		.data = &camsys_rmsb_mcd,
	}, {
		.compatible = "mediatek,mt6881-camsys_yuva",
		.data = &cam_ya_mcd,
	}, {
		.compatible = "mediatek,mt6881-camsys_yuvb",
		.data = &cam_yb_mcd,
	}, {
		.compatible = "mediatek,mt6881-cam_main_r1a",
		.data = &cam_m_mcd,
	}, {
		.compatible = "mediatek,mt6881-cam_vcore_r1a",
		.data = &cam_v_mcd,
	}, {
		.compatible = "mediatek,mt6881-dip_nr1_dip1",
		.data = &dip_nr1_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6881-dip_nr2_dip1",
		.data = &dip_nr2_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6881-dip_top_dip1",
		.data = &dip_top_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6881-mmsys0",
		.data = &mm_mcd,
	}, {
		.compatible = "mediatek,mt6881-imgsys_main",
		.data = &img_mcd,
	}, {
		.compatible = "mediatek,mt6881-img_vcore_d1a",
		.data = &img_v_mcd,
	}, {
		.compatible = "mediatek,mt6881-imp_iic_top_wrap_s",
		.data = &imp_iic_top_wrap_s_mcd,
	}, {
		.compatible = "mediatek,mt6881-imp_iic_top_wrap_w",
		.data = &imp_iic_top_wrap_w_mcd,
	}, {
		.compatible = "mediatek,mt6881-infra_infracfg_ao_reg",
		.data = &infra_infracfg_ao_reg_mcd,
	}, {
		.compatible = "mediatek,mt6881-mdpsys",
		.data = &mdp_mcd,
	}, {
		.compatible = "mediatek,mt6881-mipi_csi_top_ctrl_0",
		.data = &mipi_csi_top_ctrl_0_mcd,
	}, {
		.compatible = "mediatek,mt6881-mminfra_config",
		.data = &mminfra_config_mcd,
	}, {
		.compatible = "mediatek,mt6881-pericfg_ao_reg",
		.data = &pericfg_ao_reg_mcd,
	}, {
		.compatible = "mediatek,mt6881-traw_cap_dip1",
		.data = &traw_cap_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6881-traw_dip1",
		.data = &traw_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6881-ufscfg_ao",
		.data = &ufsao_mcd,
	}, {
		.compatible = "mediatek,mt6881-ufscfg_pdn",
		.data = &ufspdn_mcd,
	}, {
		.compatible = "mediatek,mt6881-vdecsys",
		.data = &vde2_mcd,
	}, {
		.compatible = "mediatek,mt6881-vencsys",
		.data = &ven1_mcd,
	}, {
		.compatible = "mediatek,mt6881-wpe_eis_dip1",
		.data = &wpe_eis_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6881-wpe_tnr_dip1",
		.data = &wpe_tnr_dip1_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6881_cg_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6881_cg_drv = {
	.probe = clk_mt6881_cg_grp_probe,
	.driver = {
		.name = "clk-mt6881-cg",
		.of_match_table = of_match_clk_mt6881_cg,
	},
};

module_platform_driver(clk_mt6881_cg_drv);
MODULE_LICENSE("GPL");
