// SPDX-License-Identifier: GPL-2.0
/*
 *  mt6881-afe-clk.c  --  Mediatek 6881 afe clock ctrl
 *
 *  Copyright (c) 2025 MediaTek Inc.
 *  Author: Shawn Sung <shawn.sung@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/arm-smccc.h> /* for Kernel Native SMC API */
#include <linux/soc/mediatek/mtk_sip_svc.h> /* for SMC ID table */

#include "mt6881-afe-common.h"
#include "mt6881-afe-clk.h"

#if defined(IS_FPGA_EARLY_PORTING)
int mt6881_init_clock(struct mtk_base_afe *afe) { return 0; }
int mt6881_afe_enable_clock(struct mtk_base_afe *afe) { return 0; }
void mt6881_afe_disable_clock(struct mtk_base_afe *afe) {}
int mt6881_afe_sram_request(struct mtk_base_afe *afe) { return 0; }
void mt6881_afe_sram_release(struct mtk_base_afe *afe) {}
int mt6881_afe_dram_request(struct device *dev) { return 0; }
int mt6881_afe_dram_release(struct device *dev) { return 0; }
int mt6881_apll1_enable(struct mtk_base_afe *afe) { return 0; }
void mt6881_apll1_disable(struct mtk_base_afe *afe) {}
int mt6881_apll2_enable(struct mtk_base_afe *afe) { return 0; }
void mt6881_apll2_disable(struct mtk_base_afe *afe) {}
int mt6881_get_apll_rate(struct mtk_base_afe *afe, int apll) { return 0; }
int mt6881_get_apll_by_rate(struct mtk_base_afe *afe, int rate) { return 0; }
int mt6881_get_apll_by_name(struct mtk_base_afe *afe, const char *name) { return 0; }
int mt6881_mck_enable(struct mtk_base_afe *afe, int mck_id, int rate) { return 0; }
int mt6881_mck_disable(struct mtk_base_afe *afe, int mck_id, int rate) { return 0; }
int mt6881_set_audio_int_bus_parent(struct mtk_base_afe *afe, int clk_id) { return 0; }
#else
static DEFINE_MUTEX(mutex_request_dram);

static const char *aud_clks[CLK_NUM] = {
	[CLK_AFE_APLL1_AUDIO] = "afe_apll1_audio",
	[CLK_AFE_APLL2_AUDIO] = "afe_apll2_audio",
	[CLK_AFE_APLL_TUNER1_AUDIO] = "afe_apll_tuner1_audio",
	[CLK_AFE_APLL_TUNER2_AUDIO] = "afe_apll_tuner2_audio",
	[CLK_AFE_AUDIO_F26M_AUDIO] = "afe_audio_f26m_audio",
	[CLK_AFE_AUDIO_HOPPING_AUDIO] = "afe_audio_hopping_audio",
	[CLK_AFE_DL0_DAC_AUDIO] = "afe_dl0_dac_audio",
	[CLK_AFE_DL0_DAC_HIRES_AUDIO] = "afe_dl0_dac_hires_audio",
	[CLK_AFE_DL0_PREDIS_AUDIO] = "afe_dl0_predis_audio",
	[CLK_AFE_DL1_DAC_AUDIO] = "afe_dl1_dac_audio",
	[CLK_AFE_DL1_DAC_HIRES_AUDIO] = "afe_dl1_dac_hires_audio",
	[CLK_AFE_DL1_PREDIS_AUDIO] = "afe_dl1_predis_audio",
	[CLK_AFE_UL0_ADC_AUDIO] = "afe_ul0_adc_audio",
	[CLK_AFE_UL0_ADC_HIRES_AUDIO] = "afe_ul0_adc_hires_audio",
	[CLK_AFE_UL1_ADC_AUDIO] = "afe_ul1_adc_audio",
	[CLK_AFE_UL1_ADC_HIRES_AUDIO] = "afe_ul1_adc_hires_audio",
	[CLK_CKSYS_REG_AUD_1_SEL] = "clk_top_aud_1_sel",
	[CLK_CKSYS_REG_AUD_2_SEL] = "clk_top_aud_2_sel",
	[CLK_CKSYS_REG_AUD_ENGEN1_SEL] = "clk_top_aud_engen1_sel",
	[CLK_CKSYS_REG_AUD_ENGEN2_SEL] = "clk_top_aud_engen2_sel",
	[CLK_CKSYS_REG_AUD_INTBUS_SEL] = "clk_top_aud_intbus_sel",
	[CLK_CKSYS_REG_AUDIO_H_SEL] = "clk_top_audio_h_sel",
	[CLK_PERICFG_AO_REG_PERI_AUDIO0_AUDIO] = "perao_p_audio0_audio",
	[CLK_PERICFG_AO_REG_PERI_AUDIO1_AUDIO] = "perao_p_audio1_audio",
	[CLK_PERICFG_AO_REG_PERI_AUDIO2_AUDIO] = "perao_p_audio2_audio",
	[CLK_CKSYS_REG_APLL1] = "apll1_ck",
	[CLK_CKSYS_REG_APLL1_D2] = "top_apll1_d2",
	[CLK_CKSYS_REG_APLL1_D4] = "top_apll1_d4",
	[CLK_CKSYS_REG_APLL2] = "apll2_ck",
	[CLK_CKSYS_REG_APLL2_D2] = "top_apll2_d2",
	[CLK_CKSYS_REG_APLL2_D4] = "top_apll2_d4",
	[CLK_CKSYS_REG_APLL_IN1_MCK_SEL] = "apll_i2sin1_mck_sel",
	[CLK_CKSYS_REG_APLL12_CK_DIV_IN1] = "top_apll12_div_i2sin1",
	[CLK_CKSYS_REG_TCK_26M_MX9] = "tck_26m_mx9_ck",
};

int mt6881_set_audio_int_bus_parent(struct mtk_base_afe *afe,
				    int clk_id)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	if (clk_id < 0 || clk_id > CLK_CKSYS_REG_TCK_26M_MX9) {
		dev_info(afe->dev, "%s(), invalid clk_id %d\n", __func__, clk_id);
		return -EINVAL;
	}

	ret = clk_set_parent(afe_priv->clk[CLK_CKSYS_REG_AUD_INTBUS_SEL],
			     afe_priv->clk[clk_id]);
	if (ret) {
		dev_info(afe->dev, "%s() clk_set_parent %s-%s fail %d\n",
			__func__, aud_clks[CLK_CKSYS_REG_AUD_INTBUS_SEL],
			aud_clks[clk_id], ret);
	}

	return ret;
}

int mt6881_set_audio_h_parent(struct mtk_base_afe *afe,
				    int clk_id)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	if (clk_id < 0 || clk_id > CLK_CKSYS_REG_TCK_26M_MX9) {
		dev_info(afe->dev, "%s(), invalid clk_id %d\n", __func__, clk_id);
		return -EINVAL;
	}

	ret = clk_set_parent(afe_priv->clk[CLK_CKSYS_REG_AUDIO_H_SEL],
			     afe_priv->clk[clk_id]);
	if (ret) {
		dev_info(afe->dev, "%s() clk_set_parent %s-%s fail %d\n",
			__func__, aud_clks[CLK_CKSYS_REG_AUDIO_H_SEL],
			aud_clks[clk_id], ret);
	}
	return ret;
}

static int apll1_mux_setting(struct mtk_base_afe *afe, bool enable)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	if (enable) {
		ret = clk_prepare_enable(afe_priv->clk[CLK_CKSYS_REG_AUD_1_SEL]);
		if (ret) {
			dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_CKSYS_REG_AUD_1_SEL], ret);
			goto EXIT;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_CKSYS_REG_AUD_1_SEL],
				     afe_priv->clk[CLK_CKSYS_REG_APLL1]);
		if (ret) {
			dev_info(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_CKSYS_REG_AUD_1_SEL],
				aud_clks[CLK_CKSYS_REG_APLL1], ret);
			goto EXIT;
		}

		/* 180.6336 / 4 = 45.1584MHz */
		ret = clk_prepare_enable(afe_priv->clk[CLK_CKSYS_REG_AUD_ENGEN1_SEL]);
		if (ret) {
			dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_CKSYS_REG_AUD_ENGEN1_SEL], ret);
			goto EXIT;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_CKSYS_REG_AUD_ENGEN1_SEL],
				     afe_priv->clk[CLK_CKSYS_REG_APLL1_D4]);
		if (ret) {
			dev_info(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_CKSYS_REG_AUD_ENGEN1_SEL],
				aud_clks[CLK_CKSYS_REG_APLL1_D4], ret);
			goto EXIT;
		}

		ret = clk_prepare_enable(afe_priv->clk[CLK_CKSYS_REG_AUDIO_H_SEL]);
		if (ret) {
			dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				 __func__, aud_clks[CLK_CKSYS_REG_AUDIO_H_SEL], ret);
			goto EXIT;
		}
		mt6881_set_audio_h_parent(afe, CLK_CKSYS_REG_APLL1);
	} else {
		ret = clk_set_parent(afe_priv->clk[CLK_CKSYS_REG_AUD_ENGEN1_SEL],
				     afe_priv->clk[CLK_CKSYS_REG_TCK_26M_MX9]);
		if (ret) {
			dev_info(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_CKSYS_REG_AUD_ENGEN1_SEL],
				aud_clks[CLK_CKSYS_REG_TCK_26M_MX9], ret);
			goto EXIT;
		}
		clk_disable_unprepare(afe_priv->clk[CLK_CKSYS_REG_AUD_ENGEN1_SEL]);

		ret = clk_set_parent(afe_priv->clk[CLK_CKSYS_REG_AUD_1_SEL],
				     afe_priv->clk[CLK_CKSYS_REG_TCK_26M_MX9]);
		if (ret) {
			dev_info(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_CKSYS_REG_AUD_1_SEL],
				aud_clks[CLK_CKSYS_REG_TCK_26M_MX9], ret);
			goto EXIT;
		}
		clk_disable_unprepare(afe_priv->clk[CLK_CKSYS_REG_AUD_1_SEL]);

		mt6881_set_audio_h_parent(afe, CLK_CKSYS_REG_TCK_26M_MX9);
		clk_disable_unprepare(afe_priv->clk[CLK_CKSYS_REG_AUDIO_H_SEL]);
	}

EXIT:
	return 0;
}

static int apll2_mux_setting(struct mtk_base_afe *afe, bool enable)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	if (enable) {
		ret = clk_prepare_enable(afe_priv->clk[CLK_CKSYS_REG_AUD_2_SEL]);
		if (ret) {
			dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_CKSYS_REG_AUD_2_SEL], ret);
			goto EXIT;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_CKSYS_REG_AUD_2_SEL],
				     afe_priv->clk[CLK_CKSYS_REG_APLL2]);
		if (ret) {
			dev_info(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_CKSYS_REG_AUD_2_SEL],
				aud_clks[CLK_CKSYS_REG_APLL2], ret);
			goto EXIT;
		}

		/* 196.608 / 4 = 49.152MHz */
		ret = clk_prepare_enable(afe_priv->clk[CLK_CKSYS_REG_AUD_ENGEN2_SEL]);
		if (ret) {
			dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_CKSYS_REG_AUD_ENGEN2_SEL], ret);
			goto EXIT;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_CKSYS_REG_AUD_ENGEN2_SEL],
				     afe_priv->clk[CLK_CKSYS_REG_APLL2_D4]);
		if (ret) {
			dev_info(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_CKSYS_REG_AUD_ENGEN2_SEL],
				aud_clks[CLK_CKSYS_REG_APLL2_D4], ret);
			goto EXIT;
		}

		ret = clk_prepare_enable(afe_priv->clk[CLK_CKSYS_REG_AUDIO_H_SEL]);
		if (ret) {
			dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				 __func__, aud_clks[CLK_CKSYS_REG_AUDIO_H_SEL], ret);
			goto EXIT;
		}

		mt6881_set_audio_h_parent(afe, CLK_CKSYS_REG_APLL2);
	} else {
		ret = clk_set_parent(afe_priv->clk[CLK_CKSYS_REG_AUD_ENGEN2_SEL],
				     afe_priv->clk[CLK_CKSYS_REG_TCK_26M_MX9]);
		if (ret) {
			dev_info(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_CKSYS_REG_AUD_ENGEN2_SEL],
				aud_clks[CLK_CKSYS_REG_TCK_26M_MX9], ret);
			goto EXIT;
		}
		clk_disable_unprepare(afe_priv->clk[CLK_CKSYS_REG_AUD_ENGEN2_SEL]);

		ret = clk_set_parent(afe_priv->clk[CLK_CKSYS_REG_AUD_2_SEL],
				     afe_priv->clk[CLK_CKSYS_REG_TCK_26M_MX9]);
		if (ret) {
			dev_info(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_CKSYS_REG_AUD_2_SEL],
				aud_clks[CLK_CKSYS_REG_TCK_26M_MX9], ret);
			goto EXIT;
		}
		clk_disable_unprepare(afe_priv->clk[CLK_CKSYS_REG_AUD_2_SEL]);

		mt6881_set_audio_h_parent(afe, CLK_CKSYS_REG_TCK_26M_MX9);
		clk_disable_unprepare(afe_priv->clk[CLK_CKSYS_REG_AUDIO_H_SEL]);
	}

EXIT:
	return 0;
}

int mt6881_afe_disable_apll(struct mtk_base_afe *afe)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	dev_dbg(afe->dev, "%s() successfully start\n", __func__);

	ret = clk_prepare_enable(afe_priv->clk[CLK_CKSYS_REG_AUDIO_H_SEL]);
	if (ret) {
		dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			 __func__, aud_clks[CLK_CKSYS_REG_AUDIO_H_SEL], ret);
		goto EXIT;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_CKSYS_REG_AUD_1_SEL]);
	if (ret) {
		dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_CKSYS_REG_AUD_1_SEL], ret);
		goto EXIT;
	}

	ret = clk_set_parent(afe_priv->clk[CLK_CKSYS_REG_AUD_1_SEL],
			     afe_priv->clk[CLK_CKSYS_REG_TCK_26M_MX9]);
	if (ret) {
		dev_info(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
			__func__, aud_clks[CLK_CKSYS_REG_AUD_1_SEL],
			aud_clks[CLK_CKSYS_REG_TCK_26M_MX9], ret);
		goto EXIT;
	}
	ret = clk_prepare_enable(afe_priv->clk[CLK_CKSYS_REG_AUD_2_SEL]);
	if (ret) {
		dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_CKSYS_REG_AUD_2_SEL], ret);
		goto EXIT;
	}

	ret = clk_set_parent(afe_priv->clk[CLK_CKSYS_REG_AUD_2_SEL],
			     afe_priv->clk[CLK_CKSYS_REG_TCK_26M_MX9]);
	if (ret) {
		dev_info(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
			__func__, aud_clks[CLK_CKSYS_REG_AUD_2_SEL],
			aud_clks[CLK_CKSYS_REG_TCK_26M_MX9], ret);
		goto EXIT;
	}

	clk_disable_unprepare(afe_priv->clk[CLK_CKSYS_REG_AUD_1_SEL]);
	clk_disable_unprepare(afe_priv->clk[CLK_CKSYS_REG_AUD_2_SEL]);
	mt6881_set_audio_h_parent(afe, CLK_CKSYS_REG_TCK_26M_MX9);
	clk_disable_unprepare(afe_priv->clk[CLK_CKSYS_REG_AUDIO_H_SEL]);

	return 0;
EXIT:
	return ret;

}

int mt6881_afe_enable_ao_clock(struct mtk_base_afe *afe)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	dev_dbg(afe->dev, "%s() successfully start\n", __func__);
	/* Peri clock AO enable */
	ret = clk_prepare_enable(afe_priv->clk[CLK_PERICFG_AO_REG_PERI_AUDIO2_AUDIO]);
	if (ret) {
		dev_info(afe->dev, "%s() clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_PERICFG_AO_REG_PERI_AUDIO2_AUDIO], ret);
		goto CLK_PERICFG_AO_REG_PERI_AUDIO2_AUDIO_ERR;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_PERICFG_AO_REG_PERI_AUDIO0_AUDIO]);
	if (ret) {
		dev_info(afe->dev, "%s() clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_PERICFG_AO_REG_PERI_AUDIO0_AUDIO], ret);
		goto CLK_PERICFG_AO_REG_PERI_AUDIO0_AUDIO_ERR;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_PERICFG_AO_REG_PERI_AUDIO1_AUDIO]);
	if (ret) {
		dev_info(afe->dev, "%s() clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_PERICFG_AO_REG_PERI_AUDIO1_AUDIO], ret);
		goto CLK_PERICFG_AO_REG_PERI_AUDIO1_AUDIO_ERR;
	}
	return 0;
CLK_PERICFG_AO_REG_PERI_AUDIO1_AUDIO_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_PERICFG_AO_REG_PERI_AUDIO1_AUDIO]);
CLK_PERICFG_AO_REG_PERI_AUDIO0_AUDIO_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_PERICFG_AO_REG_PERI_AUDIO0_AUDIO]);
CLK_PERICFG_AO_REG_PERI_AUDIO2_AUDIO_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_PERICFG_AO_REG_PERI_AUDIO2_AUDIO]);

	return ret;
}

static void apll_enable(struct mtk_base_afe *afe, u32 apll, bool enable)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	u32 en, clr, con1, sdm, tuner;
	u32 val = enable ? GENMASK(31, 0) : 0;

	switch (apll) {
	case MT6881_APLL1:
		en = APLL1_EN;
		clr = APLL1_CLR;
		con1 = APLL1_CON1;
		sdm = APLL1_SDM_PCW_CHG;
		tuner = APLL1_TUNER_EN;
		break;
	case MT6881_APLL2:
		en = APLL2_EN;
		clr = APLL2_CLR;
		con1 = APLL2_CON1;
		sdm = APLL2_SDM_PCW_CHG;
		tuner = APLL2_TUNER_EN;
		break;
	default:
		pr_info("%s: invalid apll id: %d\n", __func__, apll);
		return;
	}
	if (enable)
		regmap_update_bits(afe_priv->apmixed, PLLEN_ALL_SET, en, en);
	else
		regmap_update_bits(afe_priv->apmixed, PLLEN_ALL_CLR, clr, clr);

	regmap_update_bits(afe_priv->apmixed, con1, sdm, val);
	regmap_update_bits(afe_priv->apmixed, AP_PLL_CON3, tuner, val);
}

int mt6881_afe_apll_init(struct mtk_base_afe *afe)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;

	/* APLL1_CON2 = 0x6f28bd4c
	 * APLL2_CON2 = 0x78FD5264
	 * APLL1_TUNER_CON0 = 0x6f28bd4d
	 * APLL2_TUNER_CON0 = 0x78fd5265
	 */
	regmap_write(afe_priv->apmixed, APLL1_TUNER_CON0, 0x6f28bd4d);
	regmap_write(afe_priv->apmixed, APLL2_TUNER_CON0, 0x78fd5265);

	return 0;
}


int mt6881_afe_enable_clock(struct mtk_base_afe *afe)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;
#if !defined(SKIP_SB_SMCC)
	struct arm_smccc_res res;
#endif

	/* IPM2.0: USE HOPPING & 26M */
	ret = clk_prepare_enable(afe_priv->clk[CLK_AFE_AUDIO_HOPPING_AUDIO]);
	if (ret) {
		dev_info(afe->dev, "%s() clk_prepare_enable %s fail %d\n",
			 __func__, aud_clks[CLK_AFE_AUDIO_HOPPING_AUDIO], ret);
		goto CLK_AFE_ERR;
	}
	ret = clk_prepare_enable(afe_priv->clk[CLK_AFE_AUDIO_F26M_AUDIO]);
	if (ret) {
		dev_info(afe->dev, "%s() clk_prepare_enable %s fail %d\n",
			 __func__, aud_clks[CLK_AFE_AUDIO_F26M_AUDIO], ret);
		goto CLK_AFE_ERR;
	}

	/* IPM2.0: Remove CLK_MUX_AUDIO */

	ret = clk_prepare_enable(afe_priv->clk[CLK_CKSYS_REG_AUD_INTBUS_SEL]);
	if (ret) {
		dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			 __func__, aud_clks[CLK_CKSYS_REG_AUD_INTBUS_SEL], ret);
		goto CLK_MUX_AUDIO_INTBUS_ERR;
	}
	ret = mt6881_set_audio_int_bus_parent(afe, CLK_CKSYS_REG_TCK_26M_MX9);

	ret = clk_prepare_enable(afe_priv->clk[CLK_CKSYS_REG_AUDIO_H_SEL]);
	if (ret) {
		dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			 __func__, aud_clks[CLK_CKSYS_REG_AUDIO_H_SEL], ret);
		goto CLK_MUX_AUDIO_H_PARENT_ERR;
	}

	ret = mt6881_set_audio_h_parent(afe, CLK_CKSYS_REG_TCK_26M_MX9);

#if !defined(SKIP_SB_SMCC)
	/* use arm_smccc_smc to notify SPM */
	arm_smccc_smc(MTK_SIP_AUDIO_CONTROL,
		      MTK_AUDIO_SMC_OP_DOMAIN_SIDEBANDS,
		      0, 0, 0, 0, 0, 0, &res);
#endif
	return 0;

CLK_AFE_ERR:
	/* IPM2.0: Use HOPPING & 26M */
	clk_disable_unprepare(afe_priv->clk[CLK_AFE_AUDIO_HOPPING_AUDIO]);
	clk_disable_unprepare(afe_priv->clk[CLK_AFE_AUDIO_F26M_AUDIO]);

CLK_MUX_AUDIO_H_PARENT_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_CKSYS_REG_AUDIO_H_SEL]);
CLK_MUX_AUDIO_INTBUS_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_CKSYS_REG_AUD_INTBUS_SEL]);

	return ret;
}

void mt6881_afe_disable_clock(struct mtk_base_afe *afe)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;

	mt6881_set_audio_int_bus_parent(afe, CLK_CKSYS_REG_TCK_26M_MX9);
	clk_disable_unprepare(afe_priv->clk[CLK_CKSYS_REG_AUD_INTBUS_SEL]);
	mt6881_set_audio_h_parent(afe, CLK_CKSYS_REG_TCK_26M_MX9);
	clk_disable_unprepare(afe_priv->clk[CLK_CKSYS_REG_AUDIO_H_SEL]);

	/* IPM2.0: Use HOPPING & 26M */
	clk_disable_unprepare(afe_priv->clk[CLK_AFE_AUDIO_HOPPING_AUDIO]);
	clk_disable_unprepare(afe_priv->clk[CLK_AFE_AUDIO_F26M_AUDIO]);

}

int mt6881_afe_dram_request(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;

	dev_dbg(dev, "%s(), dram_resource_counter %d\n",
		__func__, afe_priv->dram_resource_counter);

	mutex_lock(&mutex_request_dram);

	/* use arm_smccc_smc to notify SPM */
	if (afe_priv->dram_resource_counter == 0) {
		/* set dram request */
		regmap_update_bits(afe->regmap, AFE_SPM_CONTROL_REQ,
				   AFE_DDREN_REQ_MASK_SFT,
				   AFE_DDREN_REQ_MASK_SFT);
		regmap_update_bits(afe->regmap, AFE_SPM_CONTROL_REQ,
				   AFE_APSRC_REQ_MASK_SFT,
				   AFE_APSRC_REQ_MASK_SFT);
	}
	afe_priv->dram_resource_counter++;
	mutex_unlock(&mutex_request_dram);
	return 0;
}

int mt6881_afe_dram_release(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;

	dev_dbg(dev, "%s(), dram_resource_counter %d\n",
		 __func__, afe_priv->dram_resource_counter);

	mutex_lock(&mutex_request_dram);
	afe_priv->dram_resource_counter--;

	/* use arm_smccc_smc to notify SPM */
	if (afe_priv->dram_resource_counter == 0) {
		/* reset dram request */
		regmap_update_bits(afe->regmap, AFE_SPM_CONTROL_REQ,
				   AFE_DDREN_REQ_MASK_SFT, 0);
		regmap_update_bits(afe->regmap, AFE_SPM_CONTROL_REQ,
				   AFE_APSRC_REQ_MASK_SFT, 0);
	}

	if (afe_priv->dram_resource_counter < 0) {
		dev_info(dev, "%s(), dram_resource_counter %d\n",
			 __func__, afe_priv->dram_resource_counter);
		afe_priv->dram_resource_counter = 0;
	}
	mutex_unlock(&mutex_request_dram);
	return 0;
}

int mt6881_afe_sram_request(struct mtk_base_afe *afe)
{
#if !defined(SKIP_SB_SMCC)
	struct arm_smccc_res res;

	/* use arm_smccc_smc to notify SPM */
	arm_smccc_smc(MTK_SIP_AUDIO_CONTROL,
		      MTK_AUDIO_SMC_OP_SRAM_REQUEST,
		      0, 0, 0, 0, 0, 0, &res);
#endif
	return 0;
}

void mt6881_afe_sram_release(struct mtk_base_afe *afe)
{
#if !defined(SKIP_SB_SMCC)
	struct arm_smccc_res res;

	/* use arm_smccc_smc to notify SPM */
	arm_smccc_smc(MTK_SIP_AUDIO_CONTROL,
		      MTK_AUDIO_SMC_OP_SRAM_RELEASE,
		      0, 0, 0, 0, 0, 0, &res);
#endif
}

int mt6881_apll1_enable(struct mtk_base_afe *afe)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int ret;

	/* setting for APLL */
	apll1_mux_setting(afe, true);

	ret = clk_prepare_enable(afe_priv->clk[CLK_AFE_APLL1_AUDIO]);
	if (ret) {
		dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_AFE_APLL1_AUDIO], ret);
		goto ERR_CLK_APLL1;
	}

	apll_enable(afe, MT6881_APLL1, true);

	ret = clk_prepare_enable(afe_priv->clk[CLK_AFE_APLL_TUNER1_AUDIO]);
	if (ret) {
		dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_AFE_APLL_TUNER1_AUDIO], ret);
		goto ERR_CLK_APLL1_TUNER;
	}

	regmap_update_bits(afe->regmap, AFE_APLL1_TUNER_CFG,
			   0x0000FFF7, 0x00000372);
	regmap_update_bits(afe->regmap, AFE_APLL1_TUNER_CFG, 0x1, 0x1);

	regmap_update_bits(afe->regmap, AUDIO_ENGEN_CON0,
			   AUDIO_APLL1_EN_ON_MASK_SFT,
			   0x1 << AUDIO_APLL1_EN_ON_SFT);

	return 0;

ERR_CLK_APLL1_TUNER:
	clk_disable_unprepare(afe_priv->clk[CLK_AFE_APLL_TUNER1_AUDIO]);
ERR_CLK_APLL1:
	clk_disable_unprepare(afe_priv->clk[CLK_AFE_APLL1_AUDIO]);

	return ret;
}

void mt6881_apll1_disable(struct mtk_base_afe *afe)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;

	regmap_update_bits(afe->regmap, AUDIO_ENGEN_CON0,
			   AUDIO_APLL1_EN_ON_MASK_SFT,
			   0x0 << AUDIO_APLL1_EN_ON_SFT);

	regmap_update_bits(afe->regmap, AFE_APLL1_TUNER_CFG, 0x1, 0x0);

	apll_enable(afe, MT6881_APLL1, false);

	clk_disable_unprepare(afe_priv->clk[CLK_AFE_APLL_TUNER1_AUDIO]);
	clk_disable_unprepare(afe_priv->clk[CLK_AFE_APLL1_AUDIO]);

	apll1_mux_setting(afe, false);
}

int mt6881_apll2_enable(struct mtk_base_afe *afe)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int ret;

	/* setting for APLL */
	apll2_mux_setting(afe, true);

	ret = clk_prepare_enable(afe_priv->clk[CLK_AFE_APLL2_AUDIO]);
	if (ret) {
		dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_AFE_APLL2_AUDIO], ret);
		goto ERR_CLK_APLL2;
	}

	apll_enable(afe, MT6881_APLL2, true);

	ret = clk_prepare_enable(afe_priv->clk[CLK_AFE_APLL_TUNER2_AUDIO]);
	if (ret) {
		dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_AFE_APLL_TUNER2_AUDIO], ret);
		goto ERR_CLK_APLL2_TUNER;
	}

	regmap_update_bits(afe->regmap, AFE_APLL2_TUNER_CFG,
			   0x0000FFF7, 0x00000374);
	regmap_update_bits(afe->regmap, AFE_APLL2_TUNER_CFG, 0x1, 0x1);

	regmap_update_bits(afe->regmap, AUDIO_ENGEN_CON0,
			   AUDIO_APLL2_EN_ON_MASK_SFT,
			   0x1 << AUDIO_APLL2_EN_ON_SFT);

	return 0;

ERR_CLK_APLL2_TUNER:
	clk_disable_unprepare(afe_priv->clk[CLK_AFE_APLL_TUNER2_AUDIO]);
ERR_CLK_APLL2:
	clk_disable_unprepare(afe_priv->clk[CLK_AFE_APLL2_AUDIO]);

	return ret;

	return 0;
}

void mt6881_apll2_disable(struct mtk_base_afe *afe)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;

	apll_enable(afe, MT6881_APLL2, false);

	regmap_update_bits(afe->regmap, AUDIO_ENGEN_CON0,
			   AUDIO_APLL2_EN_ON_MASK_SFT,
			   0x0 << AUDIO_APLL2_EN_ON_SFT);

	regmap_update_bits(afe->regmap, AFE_APLL2_TUNER_CFG, 0x1, 0x0);

	clk_disable_unprepare(afe_priv->clk[CLK_AFE_APLL_TUNER2_AUDIO]);
	clk_disable_unprepare(afe_priv->clk[CLK_AFE_APLL2_AUDIO]);

	apll2_mux_setting(afe, false);
}

int mt6881_get_apll_rate(struct mtk_base_afe *afe, int apll)
{
	return (apll == MT6881_APLL1) ? 180633600 : 196608000;
}

int mt6881_get_apll_by_rate(struct mtk_base_afe *afe, int rate)
{
	return ((rate % 8000) == 0) ? MT6881_APLL2 : MT6881_APLL1;
}

int mt6881_get_apll_by_name(struct mtk_base_afe *afe, const char *name)
{
	if (strcmp(name, APLL1_W_NAME) == 0)
		return MT6881_APLL1;
	else
		return MT6881_APLL2;
}

/* mck */
struct mt6881_mck_div {
	int m_sel_id;
	int div_clk_id;
	/* below will be deprecated */
	int div_pdn_reg;
	int div_pdn_mask_sft;
	int div_reg;
	int div_mask_sft;
	int div_mask;
	int div_sft;
	int div_msb_clk_id;
	int div_msb_reg;
	int div_msb_mask_sft;
	int div_msb_mask;
	int div_msb_sft;
	int div_apll_sel_reg;
	int div_apll_sel_mask_sft;
	int div_apll_sel_sft;
	int div_inv_reg;
	int div_inv_mask_sft;
};


static const struct mt6881_mck_div mck_div[MT6881_MCK_NUM] = {
	[MT6881_I2SIN1_MCK] = {
		.m_sel_id = CLK_CKSYS_REG_APLL_IN1_MCK_SEL,
		.div_clk_id = CLK_CKSYS_REG_APLL12_CK_DIV_IN1,
		.div_pdn_reg = CLK_AUDDIV_0,
		.div_pdn_mask_sft = APLL12_DIV_I2SIN1_PDN_MASK_SFT,
		.div_reg = CLK_AUDDIV_2,
		.div_mask_sft = APLL12_CK_DIV_I2SIN1_MASK_SFT,
		.div_mask = APLL12_CK_DIV_I2SIN1_MASK,
		.div_sft = APLL12_CK_DIV_I2SIN1_SFT,
		.div_apll_sel_reg = CLK_AUDDIV_0,
		.div_apll_sel_mask_sft = APLL_I2SIN1_MCK_SEL_MASK_SFT,
		.div_apll_sel_sft = APLL_I2SIN1_MCK_SEL_SFT,
		.div_inv_reg = CLK_AUDDIV_1,
		.div_inv_mask_sft = APLL12_DIV_I2SIN1_INV_SFT,
	},
	/* [MT6881_FMI2S_MCK] = {
	 *	.m_sel_id = CLK_CKSYS_REG_FMI2S_M_SEL,
	 *	.div_clk_id = CLK_CKSYS_REG_APLL12_DIV_FMI2S,
	 *	.div_pdn_reg = CLK_AUDDIV_0,
	 *	.div_pdn_mask_sft = APLL12_DIV_FMI2S_PDN_MASK_SFT,
	 *	.div_reg = CLK_AUDDIV_5,
	 *	.div_mask_sft = APLL12_CK_DIV_FMI2S_MASK_SFT,
	 *	.div_mask = APLL12_CK_DIV_FMI2S_MASK,
	 *	.div_sft = APLL12_CK_DIV_FMI2S_SFT,
	 *	.div_apll_sel_reg = CLK_AUDDIV_0,
	 *	.div_apll_sel_mask_sft = APLL_FMI2S_MCK_SEL_MASK_SFT,
	 *	.div_apll_sel_sft = APLL_FMI2S_MCK_SEL_SFT,
	 *	.div_inv_reg = CLK_AUDDIV_1,
	 *	.div_inv_mask_sft = APLL12_DIV_FMI2S_INV_SFT,
	 * },
	 */
};

int mt6881_mck_enable(struct mtk_base_afe *afe, int mck_id, int rate)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int apll = mt6881_get_apll_by_rate(afe, rate);
	int apll_clk_id = apll == MT6881_APLL1 ?
			  CLK_CKSYS_REG_AUD_1_SEL : CLK_CKSYS_REG_AUD_2_SEL;
	int m_sel_id = 0;
	int div_clk_id = 0;
	int ret = 0;

	if (mck_id < 0) {
		dev_info(afe->dev, "%s(), invalid mck_id %d\n", __func__, mck_id);
		return -EINVAL;
	}
	m_sel_id = mck_div[mck_id].m_sel_id;
	div_clk_id = mck_div[mck_id].div_clk_id;

	/* select apll */
	if (m_sel_id >= 0) {
		ret = clk_prepare_enable(afe_priv->clk[m_sel_id]);
		if (ret) {
			dev_info(afe->dev, "%s(), clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[m_sel_id], ret);
			return ret;
		}
		ret = clk_set_parent(afe_priv->clk[m_sel_id],
				     afe_priv->clk[apll_clk_id]);
		if (ret) {
			dev_info(afe->dev, "%s(), clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[m_sel_id],
				aud_clks[apll_clk_id], ret);
			return ret;
		}
	}

	/* enable div, set rate */
	if (div_clk_id < 0) {
		dev_info(afe->dev, "%s(), invalid div_clk_id %d\n", __func__, div_clk_id);
		return -EINVAL;
	}

	ret = clk_prepare_enable(afe_priv->clk[div_clk_id]);
	if (ret) {
		dev_info(afe->dev, "%s(), clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[div_clk_id], ret);
		return ret;
	}
	ret = clk_set_rate(afe_priv->clk[div_clk_id], rate);
	if (ret) {
		dev_info(afe->dev, "%s(), clk_set_rate %s, rate %d, fail %d\n",
			__func__, aud_clks[div_clk_id],
			rate, ret);
		return ret;
	}

	/* enable div, set rate */
	div_clk_id = mck_div[mck_id].div_msb_clk_id;

	if (div_clk_id < 0) {
		dev_info(afe->dev, "%s(), invalid div_clk_id %d\n", __func__, div_clk_id);
		return -EINVAL;
	}
	ret = clk_prepare_enable(afe_priv->clk[div_clk_id]);
	if (ret) {
		dev_info(afe->dev, "%s(), clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[div_clk_id], ret);
		return ret;
	}
	ret = clk_set_rate(afe_priv->clk[div_clk_id], rate);
	if (ret) {
		dev_info(afe->dev, "%s(), clk_set_rate %s, rate %d, fail %d\n",
			__func__, aud_clks[div_clk_id],
			rate, ret);
		return ret;
	}

	return 0;
}


int mt6881_mck_disable(struct mtk_base_afe *afe, int mck_id, int rate)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int apll = mt6881_get_apll_by_rate(afe, rate);
	int apll_clk_id = apll == MT6881_APLL1 ?
			  CLK_CKSYS_REG_AUD_1_SEL : CLK_CKSYS_REG_AUD_2_SEL;
	int m_sel_id = 0;
	int div_clk_id = 0;
	int ret = 0;

	if (mck_id < 0) {
		dev_info(afe->dev, "%s(), mck_id = %d < 0\n",
				__func__, mck_id);
		return -EINVAL;
	}

	m_sel_id = mck_div[mck_id].m_sel_id;
	div_clk_id = mck_div[mck_id].div_clk_id;

	if (div_clk_id < 0) {
		dev_info(afe->dev, "%s(), div_clk_id = %d < 0\n",
				__func__, div_clk_id);
		return -EINVAL;
	}
	ret = clk_prepare_enable(afe_priv->clk[apll_clk_id]);
	if (ret) {
		dev_info(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[apll_clk_id], ret);
		return ret;
	}

	ret = clk_set_parent(afe_priv->clk[apll_clk_id],
			     afe_priv->clk[CLK_CKSYS_REG_TCK_26M_MX9]);
	if (ret) {
		dev_info(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
			__func__, aud_clks[apll_clk_id],
			aud_clks[CLK_CKSYS_REG_TCK_26M_MX9], ret);
		return ret;
	}
	clk_disable_unprepare(afe_priv->clk[div_clk_id]);
	clk_disable_unprepare(afe_priv->clk[apll_clk_id]);

	if (m_sel_id >= 0)
		clk_disable_unprepare(afe_priv->clk[m_sel_id]);

	/* releae audio Vcore request after AUD_1/AUD_2 select to 26M */
	regmap_update_bits(afe->regmap, AFE_SPM_CONTROL_REQ,
			   AFE_VCORE_REQ_MASK_SFT, 0x0 << AFE_VCORE_REQ_SFT);

	return 0;
}

int mt6881_init_clock(struct mtk_base_afe *afe)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int i = 0;

	afe_priv->clk = devm_kcalloc(afe->dev, CLK_NUM, sizeof(*afe_priv->clk),
				     GFP_KERNEL);
	if (!afe_priv->clk)
		return -ENOMEM;

	for (i = 0; i < CLK_NUM; i++) {
		if (aud_clks[i] == NULL) {
			dev_info(afe->dev, "%s(), clk id %d not define!!!\n",
			 __func__, i);
			//AUDIO_AEE("clk not define");
			//return 0;
		}

		afe_priv->clk[i] = devm_clk_get(afe->dev, aud_clks[i]);
		if (IS_ERR(afe_priv->clk[i])) {
			dev_info(afe->dev, "%s devm_clk_get %s fail, ret %ld\n",
				 __func__,
				 aud_clks[i], PTR_ERR(afe_priv->clk[i]));
			/*return PTR_ERR(clks[i]);*/
			afe_priv->clk[i] = NULL;
		}
	}

	afe_priv->apmixed = syscon_regmap_lookup_by_phandle(afe->dev->of_node,
			    "apmixedsys");
	if (IS_ERR(afe_priv->apmixed)) {
		dev_info(afe->dev, "%s() Cannot find apmixedsys: %ld\n",
			__func__, PTR_ERR(afe_priv->apmixed));
		return PTR_ERR(afe_priv->apmixed);
	}

	afe_priv->topckgen = syscon_regmap_lookup_by_phandle(afe->dev->of_node,
			     "topckgen");
	if (IS_ERR(afe_priv->topckgen)) {
		dev_info(afe->dev, "%s() Cannot find topckgen controller: %ld\n",
			__func__, PTR_ERR(afe_priv->topckgen));
		return PTR_ERR(afe_priv->topckgen);
	}

	mt6881_afe_apll_init(afe);
	mt6881_afe_disable_apll(afe);
	mt6881_afe_enable_ao_clock(afe);
	return 0;
}
#endif
