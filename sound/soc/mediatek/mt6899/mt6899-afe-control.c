// SPDX-License-Identifier: GPL-2.0
/*
 *  MediaTek ALSA SoC Audio Control
 *
 *  Copyright (c) 2023 MediaTek Inc.
 *  Author: Yen-cheng Wang <Yen-cheng.Wang@mediatek.com>
 */

#include "mt6899-afe-common.h"
#include <linux/pm_runtime.h>

#include "mtk-sram-manager.h"
#include "../common/mtk-afe-fe-dai.h"
//Luke: bypass power related func tmp
//#include "../../codecs/mt6681-private.h"


/* don't use this directly if not necessary */
static struct mtk_base_afe *local_afe;
//Luke: bypass power related func tmp
//static struct audio_swpm_data local_audio_data;


int mt6899_set_local_afe(struct mtk_base_afe *afe)
{
	local_afe = afe;
	return 0;
}

unsigned int mt6899_swpm_rate_transform(struct device *dev,
		unsigned int rate)
{
	switch (rate) {
	case 32000:
		return AUDIO_RATE_32K;
	case 48000:
		return AUDIO_RATE_48K;
	case 96000:
		return AUDIO_RATE_96K;
	case 192000:
		return AUDIO_RATE_192K;
	case 384000:
		return AUDIO_RATE_384K;
	default:
		dev_info(dev, "%s(), rate %u invalid, use %d!!!\n",
			 __func__,
			 rate, AUDIO_RATE_48K);
		return AUDIO_RATE_48K;
	}
}

unsigned int mt6899_general_rate_transform(struct device *dev,
		unsigned int rate)
{
	switch (rate) {
	case 8000:
		return MTK_AFE_IPM2P0_RATE_8K;
	case 11025:
		return MTK_AFE_IPM2P0_RATE_11K;
	case 12000:
		return MTK_AFE_IPM2P0_RATE_12K;
	case 16000:
		return MTK_AFE_IPM2P0_RATE_16K;
	case 22050:
		return MTK_AFE_IPM2P0_RATE_22K;
	case 24000:
		return MTK_AFE_IPM2P0_RATE_24K;
	case 32000:
		return MTK_AFE_IPM2P0_RATE_32K;
	case 44100:
		return MTK_AFE_IPM2P0_RATE_44K;
	case 48000:
		return MTK_AFE_IPM2P0_RATE_48K;
	case 88200:
		return MTK_AFE_IPM2P0_RATE_88K;
	case 96000:
		return MTK_AFE_IPM2P0_RATE_96K;
	case 176400:
		return MTK_AFE_IPM2P0_RATE_176K;
	case 192000:
		return MTK_AFE_IPM2P0_RATE_192K;
	/* not support 260K */
	case 352800:
		return MTK_AFE_IPM2P0_RATE_352K;
	case 384000:
		return MTK_AFE_IPM2P0_RATE_384K;
	default:
		dev_info(dev, "%s(), rate %u invalid, use %d!!!\n",
			 __func__,
			 rate, MTK_AFE_IPM2P0_RATE_48K);
		return MTK_AFE_IPM2P0_RATE_48K;
	}
}

unsigned int mt6899_general_rate_transform_inverse(struct device *dev,
		unsigned int rate)
{
	switch (rate) {
	case MTK_AFE_IPM2P0_RATE_8K:
		return 8000;
	case MTK_AFE_IPM2P0_RATE_11K:
		return 11025;
	case MTK_AFE_IPM2P0_RATE_12K:
		return 12000;
	case MTK_AFE_IPM2P0_RATE_16K:
		return 16000;
	case MTK_AFE_IPM2P0_RATE_22K:
		return 22050;
	case MTK_AFE_IPM2P0_RATE_24K:
		return 24000;
	case MTK_AFE_IPM2P0_RATE_32K:
		return 32000;
	case MTK_AFE_IPM2P0_RATE_44K:
		return 44100;
	case MTK_AFE_IPM2P0_RATE_48K:
		return 48000;
	case MTK_AFE_IPM2P0_RATE_88K:
		return 88200;
	case MTK_AFE_IPM2P0_RATE_96K:
		return 96000;
	case MTK_AFE_IPM2P0_RATE_176K:
		return 176400;
	case MTK_AFE_IPM2P0_RATE_192K:
		return 192000;
	/* not support 260K */
	case MTK_AFE_IPM2P0_RATE_352K:
		return 352800;
	case MTK_AFE_IPM2P0_RATE_384K:
		return 384000;
	default:
		dev_info(dev, "%s(), rate %u invalid, use %d!!!\n",
			 __func__,
			 rate, 48000);
		return 48000;
	}
}

static unsigned int pcm_rate_transform(struct device *dev,
				       unsigned int rate)
{
	switch (rate) {
	case 8000:
		return MTK_AFE_PCM_RATE_8K;
	case 16000:
		return MTK_AFE_PCM_RATE_16K;
	case 32000:
		return MTK_AFE_PCM_RATE_32K;
	case 48000:
		return MTK_AFE_PCM_RATE_48K;
	default:
		dev_info(dev, "%s(), rate %u invalid, use %d!!!\n",
			 __func__,
			 rate, MTK_AFE_PCM_RATE_32K);
		return MTK_AFE_PCM_RATE_32K;
	}
}

unsigned int mt6899_rate_transform(struct device *dev,
				   unsigned int rate, int aud_blk)
{
	switch (aud_blk) {
	case MT6899_DAI_PCM_0:
	case MT6899_DAI_PCM_1:
		return pcm_rate_transform(dev, rate);
	default:
		return mt6899_general_rate_transform(dev, rate);
	}
}

int mt6899_dai_set_priv(struct mtk_base_afe *afe, int id,
			int priv_size, const void *priv_data)
{
	struct mt6899_afe_private *afe_priv = afe->platform_priv;
	void *temp_data;

	temp_data = devm_kzalloc(afe->dev,
				 priv_size,
				 GFP_KERNEL);
	if (!temp_data)
		return -ENOMEM;

	if (priv_data)
		memcpy(temp_data, priv_data, priv_size);

	if (id < 0 || id >= MT6899_DAI_NUM) {
		dev_info(afe->dev, "%s(), invalid DAI id %d\n", __func__, id);
		return -EINVAL;
	}

	afe_priv->dai_priv[id] = temp_data;

	return 0;
}

/* DC compensation */
int mt6899_enable_dc_compensation(bool enable)
{
	if (!local_afe)
		return -EPERM;

	return 0;
}
EXPORT_SYMBOL(mt6899_enable_dc_compensation);

int mt6899_set_lch_dc_compensation(int value)
{
	if (!local_afe)
		return -EPERM;

	return 0;
}
EXPORT_SYMBOL(mt6899_set_lch_dc_compensation);

int mt6899_set_rch_dc_compensation(int value)
{
	if (!local_afe)
		return -EPERM;

	return 0;
}
EXPORT_SYMBOL(mt6899_set_rch_dc_compensation);

int mt6899_adda_dl_gain_control(bool mute)
{
	unsigned int dl_gain_ctl;

	if (!local_afe)
		return -EPERM;

	if (mute)
		dl_gain_ctl = MTK_AFE_ADDA_DL_GAIN_MUTE;
	else
		dl_gain_ctl = 0xf74ff74f;

	dev_info(local_afe->dev, "%s(), adda_dl_gain %x\n",
		 __func__, dl_gain_ctl);

	return 0;
}
EXPORT_SYMBOL(mt6899_adda_dl_gain_control);
