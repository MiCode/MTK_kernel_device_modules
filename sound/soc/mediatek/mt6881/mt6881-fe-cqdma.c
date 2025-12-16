// SPDX-License-Identifier: GPL-2.0
/*
 *  MediaTek ALSA SoC Audio DAI CQDMA Control
 *
 *  Copyright (c) 2025 MediaTek Inc.
 *  Author: garry.zhang <garry.zhang@mediatek.com>
 */
#include <linux/bitops.h>
#include <linux/regmap.h>
#include <linux/err.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include <mt6881-reg.h>
#include "mtk-afe-fe-dai.h"
#include "mtk-base-afe.h"
#include "mt6881-fe-cqdma.h"

#define DMA_BIAS 0x80U
#define IQ_INTERNAL_ADDR 0x40000000U
#define DMA_CHUNK_LEN 0x100 /* 256 bytes */

static int mtk_cqdma_enable(struct mtk_base_afe *afe, int id, bool enable)
{
	struct mtk_base_afe_memif *memif = &afe->memif[id];

	mtk_regmap_update_bits(afe->regmap,
			       memif->data->en_reg,
			       memif->data->en_mask,
			       enable,
			       memif->data->en_shift);

	return 0;
}

static int mtk_cqdma_irq_enable(struct mtk_base_afe *afe, int id, bool enable)
{
	struct mtk_base_afe_memif *memif = &afe->memif[id];

	mtk_regmap_update_bits(afe->regmap,
			       memif->data->int_en_reg,
			       memif->data->int_en_mask,
			       enable,
			       memif->data->int_en_shift);

	return 0;
}

static int mtk_cqdma_get_irq_flag(struct mtk_base_afe *afe, int id)
{
	unsigned int flag = 0;

	struct mtk_base_afe_memif *memif = &afe->memif[id];

	regmap_read(afe->regmap,
			memif->data->int_flag_reg,
			&flag);

	return flag;
}

static int mtk_cqdma_irq_flag_clr(struct mtk_base_afe *afe, int id)
{
	struct mtk_base_afe_memif *memif = &afe->memif[id];

	mtk_regmap_update_bits(afe->regmap,
			       memif->data->int_flag_reg,
			       memif->data->int_flag_mask,
			       0,
			       memif->data->int_flag_shift);
	return 0;
}

static int mtk_cqdma_set_irq_cnt(struct mtk_base_afe *afe, int id,
	unsigned int count)
{
	struct mtk_base_afe_memif *memif = &afe->memif[id];

	mtk_regmap_update_bits(afe->regmap,
			       memif->data->irqs_cnt_reg,
			       memif->data->irqs_cnt_mask,
			       count,
			       memif->data->irqs_cnt_shift);
	return 0;
}

static int mtk_cqdma_set_irq_reloader_cnt(struct mtk_base_afe *afe, int id,
	unsigned int count)
{
	struct mtk_base_afe_memif *memif = &afe->memif[id];

	mtk_regmap_update_bits(afe->regmap,
			       memif->data->reloader_cnt_reg,
			       memif->data->reloader_cnt_mask,
			       count,
			       memif->data->reloader_cnt_shift);

	return 0;
}

static int mtk_cqdma_get_base_addr(struct mtk_base_afe *afe, int id,
	dma_addr_t *dma_addr)
{
	int ret = 0;
	unsigned int hw_base_lower32 = 0;
	unsigned int hw_base_upper32 = 0;
	struct mtk_base_afe_memif *memif = &afe->memif[id];

	ret = regmap_read(afe->regmap,
				memif->data->reg_ofs_base,
				&hw_base_lower32);
	if (ret) {
		dev_info(afe->dev, "%s hw_base_lower32 err\n", __func__);
		return ret;
	}

	ret = regmap_read(afe->regmap,
			memif->data->reg_ofs_base_msb,
			&hw_base_upper32);
	if (ret) {
		dev_info(afe->dev, "%s hw_base_upper32 err\n", __func__);
		return ret;
	}

	*dma_addr = TO_DMA_ADDR(hw_base_upper32, hw_base_lower32);

	return 0;
}

static int mtk_cqdma_get_hw_ptr(struct mtk_base_afe *afe, int id,
	dma_addr_t *hw_ptr)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	struct mtk_afe_cqdma_priv *cqdma = afe_priv->dai_priv[id];
	unsigned int hw_ptr_lower32 = 0;
	unsigned int hw_ptr_upper32 = 0;
	int ret = 0;
	dma_addr_t base_addr = 0;

	mtk_cqdma_get_base_addr(afe, id, &base_addr);

	ret = regmap_read(afe->regmap,
				memif->data->reg_ofs_cur,
				&hw_ptr_lower32);
	if (ret) {
		dev_info(afe->dev, "%s hw_ptr_lower32 err\n", __func__);
		return ret;
	}

	ret = regmap_read(afe->regmap,
			memif->data->reg_ofs_cur_msb,
			&hw_ptr_upper32);
	if (ret) {
		dev_info(afe->dev, "%s hw_ptr_upper32 err\n", __func__);
		return ret;
	}

	*hw_ptr = TO_DMA_ADDR(hw_ptr_upper32, hw_ptr_lower32);

	// CQDMA RG will return the current HW pointer as NEXT position to write
	// Report START of buffer to not confuse ALSA
	if (base_addr + cqdma->total_buffer_bytes == *hw_ptr) {
		// *hw_ptr -= 1;
		*hw_ptr = base_addr;
	}

	return 0;
}

static int mtk_cqdma_update_ptr(struct mtk_base_afe *afe, int memif_num)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_cqdma_priv *cqdma = afe_priv->dai_priv[memif_num];
	dma_addr_t hw_ptr;
	unsigned int cur_ptr_offset = 0;
	unsigned int cur_irq_cnt = 0;
	unsigned int next_irq_cnt = 0;
	unsigned int next_reloader_cnt = 0;

	mtk_cqdma_get_hw_ptr(afe, memif_num, &hw_ptr);

	cur_ptr_offset = (unsigned int)(hw_ptr - cqdma->dma_start_addr);
	cur_irq_cnt = (cur_ptr_offset - (cur_ptr_offset % cqdma->chunk_size)) / cqdma->chunk_size;

	next_irq_cnt = (cur_irq_cnt - 1) + cqdma->irq_step;
	next_irq_cnt = next_irq_cnt % cqdma->buffer_chunk_cnt;
	next_reloader_cnt = cqdma->buffer_chunk_cnt - next_irq_cnt;

	mtk_cqdma_set_irq_cnt(afe, memif_num, next_irq_cnt);
	mtk_cqdma_set_irq_reloader_cnt(afe, memif_num, next_reloader_cnt);

	dev_info(afe->dev, "%s: offset 0x%X, chunk_size 0x%x, step 0x%x\n",
		__func__, cur_ptr_offset, cqdma->chunk_size, cqdma->irq_step);
	dev_info(afe->dev, "%s: total_chunk 0x%x, next_irq 0x%x, next_reloader 0x%x\n",
		__func__, cqdma->buffer_chunk_cnt, next_irq_cnt, next_reloader_cnt);

	return 0;
}

static int mtk_cqdma_flush(struct mtk_base_afe *afe, int id)
{
	int ret = 0;
	unsigned int val;
	struct mtk_base_afe_memif *memif = &afe->memif[id];

	mtk_regmap_update_bits(afe->regmap,
			       memif->data->flush_reg,
			       memif->data->flush_mask,
			       0x1,
			       memif->data->flush_shift);

	/*
	 * Whan flush is done, the en bit will clear, so wait for HW to clear.
	 * Delay for 1 ms each time until en bit is cleared.
	 * timeout: 10 ms
	 */
	ret = regmap_read_poll_timeout_atomic(afe->regmap, memif->data->en_ro_reg,
				val, (!(val & 1 << memif->data->en_ro_shift)),
				REGMAP_READ_POLL_MIN, REGMAP_READ_POLL_MAX);

	mtk_regmap_update_bits(afe->regmap,
			       memif->data->flush_reg,
			       memif->data->flush_mask,
			       0x0,
			       memif->data->flush_shift);

	return ret;
}

static int mtk_cqdma_warm_reset(struct mtk_base_afe *afe, int id)
{
	struct mtk_base_afe_memif *memif = &afe->memif[id];

	mtk_regmap_update_bits(afe->regmap,
			       memif->data->warm_rst_reg,
			       memif->data->warm_rst_mask,
			       0x1,
			       memif->data->warm_rst_shift);
	return 0;
}

static int mtk_cqdma_set_transfer_len(struct mtk_base_afe *afe, int id,
	unsigned int len)
{
	struct mtk_base_afe_memif *memif = &afe->memif[id];

	mtk_regmap_update_bits(afe->regmap,
			       memif->data->trans_len1_reg,
			       memif->data->trans_len1_mask,
			       len,
			       memif->data->trans_len1_shift);
	return 0;
}

static int mtk_cqdma_stop(struct mtk_base_afe *afe, int id)
{
	int ret = 0;

	mtk_cqdma_flush(afe, id);
	mtk_cqdma_enable(afe, id, false);
	mtk_cqdma_irq_enable(afe, id, false);
	mtk_cqdma_warm_reset(afe, id);
	mtk_cqdma_set_irq_cnt(afe, id, 0);
	mtk_cqdma_set_irq_reloader_cnt(afe, id, 0);

	return ret;
}

static int mtk_cqdma_pause(struct mtk_base_afe *afe, int id, unsigned int pause)
{
	struct mtk_base_afe_memif *memif = &afe->memif[id];

	mtk_regmap_update_bits(afe->regmap,
			       memif->data->pause_reg,
			       memif->data->pause_mask,
			       pause,
			       memif->data->pause_shift);
	return 0;
}

static void mtk_cqdma_shutdown(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	int id = snd_soc_rtd_to_cpu(rtd, 0)->id;
	struct mtk_base_afe_memif *memif = &afe->memif[id];

	memif->substream = NULL;
	memif->get_cqdma_base_addr = NULL;
	memif->get_cqdma_hw_ptr = NULL;
	memif->update_cqdma_ptr = NULL;
	memif->get_cqdma_irq_flag = NULL;
	memif->clr_cqdma_irq_flag = NULL;

	dev_info(afe->dev, "%s: id:%d\n", __func__, id);
}

static int mtk_cqdma_hw_free(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
	return snd_pcm_lib_free_pages(substream);
}

static int mtk_cqdma_trigger(struct snd_pcm_substream *substream, int cmd,
				 struct snd_soc_dai *dai)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	int id = snd_soc_rtd_to_cpu(rtd, 0)->id;

	dev_info(afe->dev, "%s: id:%d cmd: %d\n", __func__, id, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		mtk_cqdma_enable(afe, id, true);
		mtk_cqdma_irq_enable(afe, id, true);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		mtk_cqdma_irq_enable(afe, id, false);
		mtk_cqdma_irq_flag_clr(afe, id);
		mtk_cqdma_stop(afe, id);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		mtk_cqdma_pause(afe, id, true);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		mtk_cqdma_pause(afe, id, false);
		break;
	case SNDRV_PCM_TRIGGER_DRAIN:
		mtk_cqdma_flush(afe, id);
		break;
	}

	return ret;
}

static int mtk_cqdma_prepare(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int id = snd_soc_rtd_to_cpu(rtd, 0)->id;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mtk_afe_cqdma_priv *cqdma = afe_priv->dai_priv[id];

	unsigned int period_bytes = frames_to_bytes(runtime, runtime->period_size);
	unsigned int buffer_bytes = frames_to_bytes(runtime, runtime->buffer_size);
	unsigned int total_irq_count = buffer_bytes / DMA_CHUNK_LEN;
	unsigned int irq_step = period_bytes  / DMA_CHUNK_LEN; // Chunks for one period

	unsigned int first_irq_count = 0;
	unsigned int reloader_irq_count = 0;

	// irq_count starts at index 0. Minus 1 otherwise it will write 1 more chunk than we expect.
	first_irq_count = irq_step - 1;
	reloader_irq_count = total_irq_count - first_irq_count;

	dev_info(afe->dev,
		"%s: dma_buf = 0x%x, period (size: %lu, bytes: 0x%x)\n",
		__func__, (period_bytes * runtime->periods), runtime->period_size, period_bytes);

	dev_info(afe->dev,
		"Prepare '%s' device(%d,%s) access(%d) format(%d) ch(%u) rate(%u) period_size(%lu) periods(%u)\n",
		snd_pcm_stream_str(substream), substream->pcm->device,
		substream->name, runtime->access,
		runtime->format, runtime->channels,
		runtime->rate, runtime->period_size, runtime->periods);
	dev_info(afe->dev,
		"[%s: chunk: %#x, first_irq_count: %#x, total_irq_count: %#x, reloader_irq_count: %#x\n",
		__func__, DMA_CHUNK_LEN, first_irq_count, total_irq_count,
		reloader_irq_count);

	cqdma->chunk_size = DMA_CHUNK_LEN;
	cqdma->irq_step = irq_step;
	cqdma->buffer_chunk_cnt = total_irq_count;
	cqdma->total_buffer_bytes = period_bytes * runtime->periods;
	cqdma->dma_start_addr = substream->runtime->dma_addr;

	mtk_cqdma_set_transfer_len(afe, id, DMA_CHUNK_LEN);
	/* IRQS_CNT */
	mtk_cqdma_set_irq_cnt(afe, id, first_irq_count);
	/* RELOADER_CNT */
	mtk_cqdma_set_irq_reloader_cnt(afe, id, reloader_irq_count);

	return ret;
}

static int mtk_cqdma_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params,
				   struct snd_soc_dai *dai)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	int id = snd_soc_rtd_to_cpu(rtd, 0)->id;
	struct mtk_base_afe_memif *memif = &afe->memif[id];

	unsigned int lower_32bit = 0;
	unsigned int upper_32bit = 0;

	ret = snd_pcm_lib_malloc_pages(substream,
						params_buffer_bytes(params));
	if (ret < 0) {
		dev_info(afe->dev, "%s: malloc pages failed, ret=%d\n",
			__func__, ret);
		return ret;
	}

	dev_info(afe->dev, "%s: id:%d dma_addr: %pa, bytes: 0x%zX\n",
		__func__, id,
		&substream->runtime->dma_addr,
		substream->runtime->dma_bytes);

	lower_32bit = lower_32_bits(substream->runtime->dma_addr);
	upper_32bit = upper_32_bits(substream->runtime->dma_addr);
	mtk_regmap_write(afe->regmap,
			       memif->data->reg_ofs_cur,
			       lower_32bit);
	mtk_regmap_write(afe->regmap,
			       memif->data->reg_ofs_cur_msb,
			       upper_32bit);

	/* dma source addr */
	mtk_regmap_write(afe->regmap,
			       memif->data->src_addr_reg,
			       IQ_INTERNAL_ADDR * (id - MT6881_CQDMA0));

	mtk_regmap_update_bits(afe->regmap,
			       memif->data->rsize_reg,
			       memif->data->rsize_mask,
			       CQDMA_RWSIZE_8BYTE,
			       memif->data->rsize_shift);
	mtk_regmap_update_bits(afe->regmap,
			       memif->data->wsize_reg,
			       memif->data->wsize_mask,
			       CQDMA_RWSIZE_8BYTE,
			       memif->data->wsize_shift);
	mtk_regmap_update_bits(afe->regmap,
			       memif->data->burst_len_reg,
			       memif->data->burst_len_mask,
			       CQDMA_BURST_LEN_3_8,
			       memif->data->burst_len_shift);

	return ret;
}

static int mtk_cqdma_startup(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	int id = snd_soc_rtd_to_cpu(rtd, 0)->id;
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	const struct snd_pcm_hardware *mtk_afe_hardware = afe->mtk_afe_hardware;
	int ret = 0;

	dev_info(afe->dev, "%s:id:%d\n", __func__, id);

	memif->substream = substream;
	memif->get_cqdma_base_addr = mtk_cqdma_get_base_addr;
	memif->get_cqdma_hw_ptr = mtk_cqdma_get_hw_ptr;
	memif->update_cqdma_ptr = mtk_cqdma_update_ptr;
	memif->get_cqdma_irq_flag = mtk_cqdma_get_irq_flag;
	memif->clr_cqdma_irq_flag = mtk_cqdma_irq_flag_clr;

	snd_soc_set_runtime_hwparams(substream, mtk_afe_hardware);
	snd_pcm_hw_constraint_step(substream->runtime, 0,
				   SNDRV_PCM_HW_PARAM_PERIOD_BYTES, DMA_CHUNK_LEN);

	/* dynamic allocate irq to memif */
	if (memif->irq_usage < 0) {
		int irq_id = mtk_dynamic_irq_acquire(afe);

		if (irq_id != afe->irqs_size) {
			/* link */
			memif->irq_usage = irq_id;
		} else {
			dev_err(afe->dev, "%s() error: no more asys irq\n",
				__func__);
			ret = -EBUSY;
		}
	}

	return ret;
}

const struct snd_soc_dai_ops mtk_dai_cqdma_ops = {
	.startup	= mtk_cqdma_startup,
	.shutdown	= mtk_cqdma_shutdown,
	.hw_params	= mtk_cqdma_hw_params,
	.hw_free	= mtk_cqdma_hw_free,
	.prepare	= mtk_cqdma_prepare,
	.trigger	= mtk_cqdma_trigger,
};

static const struct snd_kcontrol_new mt6881_dai_cqdma_controls[] = {

};

static const struct snd_soc_dapm_widget mt6881_dai_cqdma_widgets[] = {

};

static const struct snd_soc_dapm_route mt6881_dai_cqdma_routes[] = {
	{"CQDMA0", NULL, "I2SIN_DMA0"},
};

#define MTK_PCM_RATES (SNDRV_PCM_RATE_8000_48000 |\
		       SNDRV_PCM_RATE_88200 |\
		       SNDRV_PCM_RATE_96000 |\
		       SNDRV_PCM_RATE_176400 |\
		       SNDRV_PCM_RATE_192000)

static struct snd_soc_dai_driver mt6881_dai_cqdma_driver[DAI_CQDMA_NUM] = {
	{
		.name = "CQDMA0",
		.id = MT6881_CQDMA0,
		.capture = {
			.stream_name = "CQDMA0",
			.channels_min = 1,
			.channels_max = 8,
			.rates = (MTK_PCM_RATES | SNDRV_PCM_RATE_KNOT),
			.formats = SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mtk_dai_cqdma_ops,
	},
	{
		.name = "CQDMA1",
		.id = MT6881_CQDMA1,
		.capture = {
			.stream_name = "CQDMA1",
			.channels_min = 1,
			.channels_max = 8,
			.rates = (MTK_PCM_RATES | SNDRV_PCM_RATE_KNOT),
			.formats = SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mtk_dai_cqdma_ops,
	},
};

int mt6881_dai_cqdma_register(struct mtk_base_afe *afe)
{
	int i = 0, ret = 0;
	struct mtk_base_afe_dai *dai;

	dev_info(afe->dev, "%s() successfully start\n", __func__);

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mt6881_dai_cqdma_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mt6881_dai_cqdma_driver);

	dai->controls = mt6881_dai_cqdma_controls;
	dai->num_controls = ARRAY_SIZE(mt6881_dai_cqdma_controls);
	dai->dapm_widgets = mt6881_dai_cqdma_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mt6881_dai_cqdma_widgets);
	dai->dapm_routes = mt6881_dai_cqdma_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mt6881_dai_cqdma_routes);

	/* set dai priv */
	for (i = 0; i < DAI_CQDMA_NUM; i++) {
		ret = mt6881_dai_set_priv(afe, i + MT6881_CQDMA0,
				sizeof(struct mtk_afe_cqdma_priv),
				NULL);
		if (ret)
			return ret;
	}

	return 0;
}
