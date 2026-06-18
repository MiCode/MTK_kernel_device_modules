// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: KY Liu <ky.liu@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6899-clk.h>

#define MT_CCF_BRINGUP		0

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

#define GATE_AFE0_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
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
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

#define GATE_AFE2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE2_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
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
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
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
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate afe_clks[] = {
	/* AFE0 */
	GATE_AFE0(CLK_AFE_DL1_DAC_TML, "afe_dl1_dac_tml",
			"f26m_ck"/* parent */, 2),
	GATE_AFE0_V(CLK_AFE_DL1_DAC_TML_AFE, "afe_dl1_dac_tml_afe",
			"afe_dl1_dac_tml"/* parent */),
	GATE_AFE0(CLK_AFE_DL1_DAC_HIRES, "afe_dl1_dac_hires",
			"audio_h_ck"/* parent */, 3),
	GATE_AFE0_V(CLK_AFE_DL1_DAC_HIRES_AFE, "afe_dl1_dac_hires_afe",
			"afe_dl1_dac_hires"/* parent */),
	GATE_AFE0(CLK_AFE_DL1_DAC, "afe_dl1_dac",
			"f26m_ck"/* parent */, 4),
	GATE_AFE0_V(CLK_AFE_DL1_DAC_AFE, "afe_dl1_dac_afe",
			"afe_dl1_dac"/* parent */),
	GATE_AFE0(CLK_AFE_DL1_PREDIS, "afe_dl1_predis",
			"f26m_ck"/* parent */, 5),
	GATE_AFE0_V(CLK_AFE_DL1_PREDIS_AFE, "afe_dl1_predis_afe",
			"afe_dl1_predis"/* parent */),
	GATE_AFE0(CLK_AFE_DL1_NLE, "afe_dl1_nle",
			"f26m_ck"/* parent */, 6),
	GATE_AFE0_V(CLK_AFE_DL1_NLE_AFE, "afe_dl1_nle_afe",
			"afe_dl1_nle"/* parent */),
	GATE_AFE0(CLK_AFE_DL0_DAC_TML, "afe_dl0_dac_tml",
			"f26m_ck"/* parent */, 7),
	GATE_AFE0_V(CLK_AFE_DL0_DAC_TML_AFE, "afe_dl0_dac_tml_afe",
			"afe_dl0_dac_tml"/* parent */),
	GATE_AFE0(CLK_AFE_DL0_DAC_HIRES, "afe_dl0_dac_hires",
			"audio_h_ck"/* parent */, 8),
	GATE_AFE0_V(CLK_AFE_DL0_DAC_HIRES_AFE, "afe_dl0_dac_hires_afe",
			"afe_dl0_dac_hires"/* parent */),
	GATE_AFE0(CLK_AFE_DL0_DAC, "afe_dl0_dac",
			"f26m_ck"/* parent */, 9),
	GATE_AFE0_V(CLK_AFE_DL0_DAC_AFE, "afe_dl0_dac_afe",
			"afe_dl0_dac"/* parent */),
	GATE_AFE0(CLK_AFE_DL0_PREDIS, "afe_dl0_predis",
			"f26m_ck"/* parent */, 10),
	GATE_AFE0_V(CLK_AFE_DL0_PREDIS_AFE, "afe_dl0_predis_afe",
			"afe_dl0_predis"/* parent */),
	GATE_AFE0(CLK_AFE_DL0_NLE, "afe_dl0_nle",
			"f26m_ck"/* parent */, 11),
	GATE_AFE0_V(CLK_AFE_DL0_NLE_AFE, "afe_dl0_nle_afe",
			"afe_dl0_nle"/* parent */),
	GATE_AFE0(CLK_AFE_PCM1, "afe_pcm1",
			"f26m_ck"/* parent */, 13),
	GATE_AFE0_V(CLK_AFE_PCM1_AFE, "afe_pcm1_afe",
			"afe_pcm1"/* parent */),
	GATE_AFE0(CLK_AFE_PCM0, "afe_pcm0",
			"f26m_ck"/* parent */, 14),
	GATE_AFE0_V(CLK_AFE_PCM0_AFE, "afe_pcm0_afe",
			"afe_pcm0"/* parent */),
	GATE_AFE0(CLK_AFE_CM1, "afe_cm1",
			"f26m_ck"/* parent */, 17),
	GATE_AFE0_V(CLK_AFE_CM1_AFE, "afe_cm1_afe",
			"afe_cm1"/* parent */),
	GATE_AFE0(CLK_AFE_CM0, "afe_cm0",
			"f26m_ck"/* parent */, 18),
	GATE_AFE0_V(CLK_AFE_CM0_AFE, "afe_cm0_afe",
			"afe_cm0"/* parent */),
	GATE_AFE0(CLK_AFE_STF, "afe_stf",
			"f26m_ck"/* parent */, 19),
	GATE_AFE0_V(CLK_AFE_STF_AFE, "afe_stf_afe",
			"afe_stf"/* parent */),
	GATE_AFE0(CLK_AFE_HW_GAIN23, "afe_hw_gain23",
			"f26m_ck"/* parent */, 20),
	GATE_AFE0_V(CLK_AFE_HW_GAIN23_AFE, "afe_hw_gain23_afe",
			"afe_hw_gain23"/* parent */),
	GATE_AFE0(CLK_AFE_HW_GAIN01, "afe_hw_gain01",
			"f26m_ck"/* parent */, 21),
	GATE_AFE0_V(CLK_AFE_HW_GAIN01_AFE, "afe_hw_gain01_afe",
			"afe_hw_gain01"/* parent */),
	GATE_AFE0(CLK_AFE_FM_I2S, "afe_fm_i2s",
			"f26m_ck"/* parent */, 24),
	GATE_AFE0_V(CLK_AFE_FM_I2S_AFE, "afe_fm_i2s_afe",
			"afe_fm_i2s"/* parent */),
	GATE_AFE0(CLK_AFE_MTKAIFV4, "afe_mtkaifv4",
			"f26m_ck"/* parent */, 25),
	GATE_AFE0_V(CLK_AFE_MTKAIFV4_AFE, "afe_mtkaifv4_afe",
			"afe_mtkaifv4"/* parent */),
	/* AFE1 */
	GATE_AFE1(CLK_AFE_AUDIO_HOPPING, "afe_audio_hopping_ck",
			"f26m_ck"/* parent */, 0),
	GATE_AFE1_V(CLK_AFE_AUDIO_HOPPING_AFE, "afe_audio_hopping_ck_afe",
			"afe_audio_hopping_ck"/* parent */),
	GATE_AFE1(CLK_AFE_AUDIO_F26M, "afe_audio_f26m_ck",
			"f26m_ck"/* parent */, 1),
	GATE_AFE1_V(CLK_AFE_AUDIO_F26M_AFE, "afe_audio_f26m_ck_afe",
			"afe_audio_f26m_ck"/* parent */),
	GATE_AFE1(CLK_AFE_APLL1, "afe_apll1_ck",
			"aud_engen1_ck"/* parent */, 2),
	GATE_AFE1_V(CLK_AFE_APLL1_AFE, "afe_apll1_ck_afe",
			"afe_apll1_ck"/* parent */),
	GATE_AFE1(CLK_AFE_APLL2, "afe_apll2_ck",
			"aud_engen2_ck"/* parent */, 3),
	GATE_AFE1_V(CLK_AFE_APLL2_AFE, "afe_apll2_ck_afe",
			"afe_apll2_ck"/* parent */),
	GATE_AFE1(CLK_AFE_H208M, "afe_h208m_ck",
			"audio_h_ck"/* parent */, 4),
	GATE_AFE1_V(CLK_AFE_H208M_AFE, "afe_h208m_ck_afe",
			"afe_h208m_ck"/* parent */),
	GATE_AFE1(CLK_AFE_APLL_TUNER2, "afe_apll_tuner2",
			"aud_engen2_ck"/* parent */, 12),
	GATE_AFE1_V(CLK_AFE_APLL_TUNER2_AFE, "afe_apll_tuner2_afe",
			"afe_apll_tuner2"/* parent */),
	GATE_AFE1(CLK_AFE_APLL_TUNER1, "afe_apll_tuner1",
			"aud_engen1_ck"/* parent */, 13),
	GATE_AFE1_V(CLK_AFE_APLL_TUNER1_AFE, "afe_apll_tuner1_afe",
			"afe_apll_tuner1"/* parent */),
	/* AFE2 */
	GATE_AFE2(CLK_AFE_DMIC1_ADC_HIRES_TML, "afe_dmic1_aht",
			"audio_h_ck"/* parent */, 0),
	GATE_AFE2_V(CLK_AFE_DMIC1_ADC_HIRES_TML_AFE, "afe_dmic1_aht_afe",
			"afe_dmic1_aht"/* parent */),
	GATE_AFE2(CLK_AFE_DMIC1_ADC_HIRES, "afe_dmic1_adc_hires",
			"audio_h_ck"/* parent */, 1),
	GATE_AFE2_V(CLK_AFE_DMIC1_ADC_HIRES_AFE, "afe_dmic1_adc_hires_afe",
			"afe_dmic1_adc_hires"/* parent */),
	GATE_AFE2(CLK_AFE_DMIC1_TML, "afe_dmic1_tml",
			"f26m_ck"/* parent */, 2),
	GATE_AFE2_V(CLK_AFE_DMIC1_TML_AFE, "afe_dmic1_tml_afe",
			"afe_dmic1_tml"/* parent */),
	GATE_AFE2(CLK_AFE_DMIC1_ADC, "afe_dmic1_adc",
			"f26m_ck"/* parent */, 3),
	GATE_AFE2_V(CLK_AFE_DMIC1_ADC_AFE, "afe_dmic1_adc_afe",
			"afe_dmic1_adc"/* parent */),
	GATE_AFE2(CLK_AFE_DMIC0_ADC_HIRES_TML, "afe_dmic0_aht",
			"audio_h_ck"/* parent */, 4),
	GATE_AFE2_V(CLK_AFE_DMIC0_ADC_HIRES_TML_AFE, "afe_dmic0_aht_afe",
			"afe_dmic0_aht"/* parent */),
	GATE_AFE2(CLK_AFE_DMIC0_ADC_HIRES, "afe_dmic0_adc_hires",
			"audio_h_ck"/* parent */, 5),
	GATE_AFE2_V(CLK_AFE_DMIC0_ADC_HIRES_AFE, "afe_dmic0_adc_hires_afe",
			"afe_dmic0_adc_hires"/* parent */),
	GATE_AFE2(CLK_AFE_DMIC0_TML, "afe_dmic0_tml",
			"f26m_ck"/* parent */, 6),
	GATE_AFE2_V(CLK_AFE_DMIC0_TML_AFE, "afe_dmic0_tml_afe",
			"afe_dmic0_tml"/* parent */),
	GATE_AFE2(CLK_AFE_DMIC0_ADC, "afe_dmic0_adc",
			"f26m_ck"/* parent */, 7),
	GATE_AFE2_V(CLK_AFE_DMIC0_ADC_AFE, "afe_dmic0_adc_afe",
			"afe_dmic0_adc"/* parent */),
	GATE_AFE2(CLK_AFE_UL1_ADC_HIRES_TML, "afe_ul1_aht",
			"audio_h_ck"/* parent */, 16),
	GATE_AFE2_V(CLK_AFE_UL1_ADC_HIRES_TML_AFE, "afe_ul1_aht_afe",
			"afe_ul1_aht"/* parent */),
	GATE_AFE2(CLK_AFE_UL1_ADC_HIRES, "afe_ul1_adc_hires",
			"audio_h_ck"/* parent */, 17),
	GATE_AFE2_V(CLK_AFE_UL1_ADC_HIRES_AFE, "afe_ul1_adc_hires_afe",
			"afe_ul1_adc_hires"/* parent */),
	GATE_AFE2(CLK_AFE_UL1_TML, "afe_ul1_tml",
			"f26m_ck"/* parent */, 18),
	GATE_AFE2_V(CLK_AFE_UL1_TML_AFE, "afe_ul1_tml_afe",
			"afe_ul1_tml"/* parent */),
	GATE_AFE2(CLK_AFE_UL1_ADC, "afe_ul1_adc",
			"f26m_ck"/* parent */, 19),
	GATE_AFE2_V(CLK_AFE_UL1_ADC_AFE, "afe_ul1_adc_afe",
			"afe_ul1_adc"/* parent */),
	GATE_AFE2(CLK_AFE_UL0_TML, "afe_ul0_tml",
			"f26m_ck"/* parent */, 22),
	GATE_AFE2_V(CLK_AFE_UL0_TML_AFE, "afe_ul0_tml_afe",
			"afe_ul0_tml"/* parent */),
	GATE_AFE2(CLK_AFE_UL0_ADC, "afe_ul0_adc",
			"f26m_ck"/* parent */, 23),
	GATE_AFE2_V(CLK_AFE_UL0_ADC_AFE, "afe_ul0_adc_afe",
			"afe_ul0_adc"/* parent */),
	/* AFE3 */
	GATE_AFE3(CLK_AFE_ETDM_IN6, "afe_etdm_in6",
			"aud_engen2_ck"/* parent */, 7),
	GATE_AFE3_V(CLK_AFE_ETDM_IN6_AFE, "afe_etdm_in6_afe",
			"afe_etdm_in6"/* parent */),
	GATE_AFE3(CLK_AFE_ETDM_IN4, "afe_etdm_in4",
			"aud_engen2_ck"/* parent */, 9),
	GATE_AFE3_V(CLK_AFE_ETDM_IN4_AFE, "afe_etdm_in4_afe",
			"afe_etdm_in4"/* parent */),
	GATE_AFE3(CLK_AFE_ETDM_IN2, "afe_etdm_in2",
			"aud_engen2_ck"/* parent */, 11),
	GATE_AFE3_V(CLK_AFE_ETDM_IN2_AFE, "afe_etdm_in2_afe",
			"afe_etdm_in2"/* parent */),
	GATE_AFE3(CLK_AFE_ETDM_IN1, "afe_etdm_in1",
			"aud_engen2_ck"/* parent */, 12),
	GATE_AFE3_V(CLK_AFE_ETDM_IN1_AFE, "afe_etdm_in1_afe",
			"afe_etdm_in1"/* parent */),
	GATE_AFE3(CLK_AFE_ETDM_IN0, "afe_etdm_in0",
			"aud_engen2_ck"/* parent */, 13),
	GATE_AFE3_V(CLK_AFE_ETDM_IN0_AFE, "afe_etdm_in0_afe",
			"afe_etdm_in0"/* parent */),
	GATE_AFE3(CLK_AFE_ETDM_OUT6, "afe_etdm_out6",
			"aud_engen2_ck"/* parent */, 15),
	GATE_AFE3_V(CLK_AFE_ETDM_OUT6_AFE, "afe_etdm_out6_afe",
			"afe_etdm_out6"/* parent */),
	GATE_AFE3(CLK_AFE_ETDM_OUT4, "afe_etdm_out4",
			"aud_engen2_ck"/* parent */, 17),
	GATE_AFE3_V(CLK_AFE_ETDM_OUT4_AFE, "afe_etdm_out4_afe",
			"afe_etdm_out4"/* parent */),
	GATE_AFE3(CLK_AFE_ETDM_OUT2, "afe_etdm_out2",
			"aud_engen2_ck"/* parent */, 19),
	GATE_AFE3_V(CLK_AFE_ETDM_OUT2_AFE, "afe_etdm_out2_afe",
			"afe_etdm_out2"/* parent */),
	GATE_AFE3(CLK_AFE_ETDM_OUT1, "afe_etdm_out1",
			"aud_engen2_ck"/* parent */, 20),
	GATE_AFE3_V(CLK_AFE_ETDM_OUT1_AFE, "afe_etdm_out1_afe",
			"afe_etdm_out1"/* parent */),
	GATE_AFE3(CLK_AFE_ETDM_OUT0, "afe_etdm_out0",
			"aud_engen2_ck"/* parent */, 21),
	GATE_AFE3_V(CLK_AFE_ETDM_OUT0_AFE, "afe_etdm_out0_afe",
			"afe_etdm_out0"/* parent */),
	GATE_AFE3(CLK_AFE_TDM_OUT, "afe_tdm_out",
			"aud_2_ck"/* parent */, 24),
	GATE_AFE3_V(CLK_AFE_TDM_OUT_AFE, "afe_tdm_out_afe",
			"afe_tdm_out"/* parent */),
	/* AFE4 */
	GATE_AFE4(CLK_AFE_GENERAL3_ASRC, "afe_general3_asrc",
			"f26m_ck"/* parent */, 21),
	GATE_AFE4_V(CLK_AFE_GENERAL3_ASRC_AFE, "afe_general3_asrc_afe",
			"afe_general3_asrc"/* parent */),
	GATE_AFE4(CLK_AFE_GENERAL2_ASRC, "afe_general2_asrc",
			"f26m_ck"/* parent */, 22),
	GATE_AFE4_V(CLK_AFE_GENERAL2_ASRC_AFE, "afe_general2_asrc_afe",
			"afe_general2_asrc"/* parent */),
	GATE_AFE4(CLK_AFE_GENERAL1_ASRC, "afe_general1_asrc",
			"f26m_ck"/* parent */, 23),
	GATE_AFE4_V(CLK_AFE_GENERAL1_ASRC_AFE, "afe_general1_asrc_afe",
			"afe_general1_asrc"/* parent */),
	GATE_AFE4(CLK_AFE_GENERAL0_ASRC, "afe_general0_asrc",
			"f26m_ck"/* parent */, 24),
	GATE_AFE4_V(CLK_AFE_GENERAL0_ASRC_AFE, "afe_general0_asrc_afe",
			"afe_general0_asrc"/* parent */),
	GATE_AFE4(CLK_AFE_CONNSYS_I2S_ASRC, "afe_connsys_i2s_asrc",
			"f26m_ck"/* parent */, 25),
	GATE_AFE4_V(CLK_AFE_CONNSYS_I2S_ASRC_AFE, "afe_connsys_i2s_asrc_afe",
			"afe_connsys_i2s_asrc"/* parent */),
};

static const struct mtk_clk_desc afe_mcd = {
	.clks = afe_clks,
	.num_clks = CLK_AFE_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6899_adsp[] = {
	{
		.compatible = "mediatek,mt6899-audiosys",
		.data = &afe_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6899_adsp_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6899_adsp_drv = {
	.probe = clk_mt6899_adsp_grp_probe,
	.driver = {
		.name = "clk-mt6899-adsp",
		.of_match_table = of_match_clk_mt6899_adsp,
	},
};

module_platform_driver(clk_mt6899_adsp_drv);
MODULE_LICENSE("GPL");
