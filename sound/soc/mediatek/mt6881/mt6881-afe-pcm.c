// SPDX-License-Identifier: GPL-2.0
/*
 *  Mediatek ALSA SoC AFE platform driver for 6881
 *
 *  Copyright (c) 2025 MediaTek Inc.
 *  Author: Lindsay Tsai <lindsay.tsai@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <sound/soc.h>
#include <linux/regmap.h>
#include <linux/of_device.h>
#include <linux/arm-smccc.h> /* for Kernel Native SMC API */
#include <linux/soc/mediatek/mtk_sip_svc.h> /* for SMC ID table */
#include <linux/sched.h>
#include <linux/kernel.h>

#include "mt6881-afe-common.h"
#include "mtk-afe-debug.h"
#include "mtk-afe-platform-driver.h"
#include "mtk-afe-fe-dai.h"
#include "mtk-sp-pcm-ops.h"
#include "mtk-sram-manager.h"
#include "mtk-mmap-ion.h"
#if !defined(SKIP_SB_USB_OFFLOAD)
#include "mtk-usb-offload-ops.h"
#endif

#include "mt6881-afe-cm.h"
#include "mt6881-afe-clk.h"
#include "mt6881-afe-gpio.h"
#include "mt6881-interconnection.h"
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
#include "../audio_dsp/mtk-dsp-common.h"
#endif
#if IS_ENABLED(CONFIG_MTK_ULTRASND_PROXIMITY) && !defined(SKIP_SB_ULTRA)
#include "../ultrasound/ultra_scp/mtk-scp-ultra-common.h"
#endif

// ALPS08709395: AudioQOS, set VIP
#ifndef SKIP_VIP
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include "vip.h"
#endif

#if IS_ENABLED(CONFIG_NEBULA_SND_PASSTHROUGH)
#include <linux/nebula/hvcall.h>
#include <linux/irq.h>
static int hwirq;
#define SMC_HYP_SECURE_ID 1
#endif

/* FORCE_FPGA_ENABLE_IRQ use irq in fpga */
#define FORCE_FPGA_ENABLE_IRQ


#define AFE_SYS_DEBUG_SIZE (1024 * 64) // 64K
#define MAX_DEBUG_WRITE_INPUT 256

static ssize_t mt6881_debug_read_reg(char *buffer, int size, struct mtk_base_afe *afe);

static const struct snd_pcm_hardware mt6881_afe_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
		 SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_MMAP_VALID),
	.formats = (SNDRV_PCM_FMTBIT_S16_LE |
		    SNDRV_PCM_FMTBIT_S24_LE |
		    SNDRV_PCM_FMTBIT_S32_LE),
	.period_bytes_min = 96,
	.period_bytes_max = 4 * 48 * 1024,
	.periods_min = 2,
	.periods_max = 256,
	.buffer_bytes_max = 256 * 1024,
	.fifo_size = 0,
};

static int mt6881_fe_startup(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_dai *cpu_dai = snd_soc_rtd_to_cpu(rtd, 0);
	int memif_num = cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	const struct snd_pcm_hardware *mtk_afe_hardware = afe->mtk_afe_hardware;
	int ret;

	memif->substream = substream;

	snd_pcm_hw_constraint_step(substream->runtime, 0,
				   SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 16);

	snd_soc_set_runtime_hwparams(substream, mtk_afe_hardware);

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		dev_info(afe->dev, "snd_pcm_hw_constraint_integer failed\n");

	/* dynamic allocate irq to memif */
	if (memif->irq_usage < 0) {
		int irq_id = mtk_dynamic_irq_acquire(afe);

		if (irq_id != afe->irqs_size) {
			/* link */
			memif->irq_usage = irq_id;
		} else {
			dev_info(afe->dev, "%s() error: no more asys irq\n",
				__func__);
			ret = -EBUSY;
		}
	}

	return ret;
}

void mt6881_fe_shutdown(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	struct snd_soc_dai *cpu_dai = snd_soc_rtd_to_cpu(rtd, 0);
	int memif_num = cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;

	if (memif->err_close_order) {
		dump_stack();
		memif->err_close_order = false;
	}

	memif->substream = NULL;
	afe_priv->irq_cnt[memif_num] = 0;
	afe_priv->xrun_assert[memif_num] = 0;

	if (!memif->const_irq) {
		mtk_dynamic_irq_release(afe, irq_id);
		memif->irq_usage = -1;
		memif->substream = NULL;
	}
}

int mt6881_fe_trigger(struct snd_pcm_substream *substream, int cmd,
		      struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *const runtime = substream->runtime;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	struct snd_soc_dai *cpu_dai = snd_soc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	int irq_id = memif->irq_usage;
	struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;
	unsigned int counter = runtime->period_size;
	unsigned int rate = runtime->rate;
	int fs;
	int ret = 0;
	unsigned int tmp_reg = 0;

	if (!in_interrupt()) {
		dev_info(afe->dev,
			 "%s(), %s cmd %d, irq_id %d, is_afe_need_triggered %d, no_period_wakeup %d\n",
			 __func__, memif->data->name, cmd, irq_id,
			 is_afe_need_triggered(memif),
			 runtime->no_period_wakeup);
		mt6881_aud_update_power_scenario();
	}

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		memif->pid = 0;
		memif->tid = 0;
		strscpy(memif->process_name, "NULL", sizeof(memif->process_name) - 1);

		/* add delay for bt memif to avoid dl noise */
		if (id == MT6881_MEMIF_DL23)
			mtk_memif_set_pbuf_size(afe, id, MT6881_MEMIF_PBUF_SIZE_32_BYTES);

		if (!strcmp(memif->data->name, "VUL_CM0")
			|| !strcmp(memif->data->name, "VUL_CM1")
		    || !strcmp(memif->data->name, "VUL_CM2"))
			mtk_memif_set_min_max_len(afe, id, MT6881_MEMIF_MAX_LEN_64_BYTES,
						MT6881_MEMIF_MAX_LEN_64_BYTES);

		if (is_afe_need_triggered(memif)) {
			ret = mtk_memif_set_enable(afe, id);

			if (ret) {
				dev_info(afe->dev,
					"%s(), error, id %d, memif enable, ret %d\n",
					__func__, id, ret);
				return ret;
			}
			if (!strcmp(memif->data->name, "VUL8") || !strcmp(memif->data->name, "VUL_CM0")) {
				if (mt6881_is_need_enable_cm(afe, CM0))
					mt6881_enable_cm(afe, CM0, 1);
			}
			if (!strcmp(memif->data->name, "VUL9") || !strcmp(memif->data->name, "VUL_CM1")) {
				if (mt6881_is_need_enable_cm(afe, CM1))
					mt6881_enable_cm(afe, CM1, 1);
			}
			if (!strcmp(memif->data->name, "VUL10") || !strcmp(memif->data->name, "VUL_CM2")) {
				if (mt6881_is_need_enable_cm(afe, CM2))
					mt6881_enable_cm(afe, CM2, 1);
			}
		}

		/*
		 * for small latency record
		 * ul memif need read some data before irq enable
		 */
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			if ((runtime->period_size * 1000) / rate <= 10)
				mt6881_aud_delay(300);
		}

		/* set irq counter */
		if (afe_priv->irq_cnt[id] > 0)
			counter = afe_priv->irq_cnt[id];

		mtk_regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				   irq_data->irq_cnt_maskbit,
				   counter, irq_data->irq_cnt_shift);

		/* set irq fs */
		fs = afe->irq_fs(substream, runtime->rate);
		if (fs < 0)
			return -EINVAL;

		mtk_regmap_update_bits(afe->regmap, irq_data->irq_fs_reg,
				   irq_data->irq_fs_maskbit,
				   fs, irq_data->irq_fs_shift);

		if (!runtime->no_period_wakeup)
			mtk_irq_set_enable(afe, irq_data, id);

		return 0;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		memif->pid = current->pid;
		memif->tid = current->tgid;
		strscpy(memif->process_name, current->comm, sizeof(memif->process_name) - 1);

		if (afe_priv->xrun_assert[id] > 0) {
			if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
				int avail = snd_pcm_capture_avail(runtime);

				if (avail >= runtime->buffer_size) {
					dev_info(afe->dev, "%s(), id %d, xrun assert\n",
						 __func__, id);
					AUDIO_AEE("xrun assert");
				}
			}
		}

		if (is_afe_need_triggered(memif)) {
			ret = mtk_memif_set_disable(afe, id);
			if (ret) {
				dev_info(afe->dev,
					"%s(), error, id %d, memif enable, ret %d\n",
					__func__, id, ret);
			}
			if (!strcmp(memif->data->name, "VUL8") || !strcmp(memif->data->name, "VUL_CM0"))
				mt6881_enable_cm(afe, CM0, 0);
			if (!strcmp(memif->data->name, "VUL9") || !strcmp(memif->data->name, "VUL_CM1"))
				mt6881_enable_cm(afe, CM1, 0);
			if (!strcmp(memif->data->name, "VUL10") || !strcmp(memif->data->name, "VUL_CM2"))
				mt6881_enable_cm(afe, CM2, 0);
		}

		if (!runtime->no_period_wakeup) {
			/* disable interrupt */
			mtk_irq_set_disable(afe, irq_data, id);

			/* clear pending IRQ */
			regmap_read(afe->regmap, irq_data->irq_clr_reg, &tmp_reg);
			regmap_update_bits(afe->regmap, irq_data->irq_clr_reg,
					   0xc0000000,
					   tmp_reg^0xc0000000);
		}

		return ret;
	default:
		return -EINVAL;
	}
}

static int mt6881_memif_fs(struct snd_pcm_substream *substream,
			   unsigned int rate)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = NULL;
	struct snd_soc_dai *cpu_dai = snd_soc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	unsigned int rate_reg = 0;
	int cm = 0;

	if (!component)
		return -EINVAL;

	afe = snd_soc_component_get_drvdata(component);

	if (!afe)
		return -EINVAL;

	rate_reg = mt6881_rate_transform(afe->dev, rate, id);

	switch (id) {
	case MT6881_MEMIF_VUL8:
	case MT6881_MEMIF_VUL_CM0:
		cm = 0x0;
		break;
	case MT6881_MEMIF_VUL9:
	case MT6881_MEMIF_VUL_CM1:
		cm = 0x1;
		break;
	case MT6881_MEMIF_VUL10:
	case MT6881_MEMIF_VUL_CM2:
		cm = 0x2;
		break;
	default:
		return rate_reg;
	}

	mt6881_set_cm_rate(cm, rate_reg);

	return rate_reg;
}

static int mt6881_get_dai_fs(struct mtk_base_afe *afe,
			     int dai_id, unsigned int rate)
{
	return mt6881_rate_transform(afe->dev, rate, dai_id);
}

static int mt6881_irq_fs(struct snd_pcm_substream *substream, unsigned int rate)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = NULL;

	if (!component)
		return -EINVAL;
	afe = snd_soc_component_get_drvdata(component);
	return mt6881_general_rate_transform(afe->dev, rate);
}

int mt6881_get_memif_pbuf_size(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	if ((runtime->period_size * 1000) / runtime->rate > 10)
		return MT6881_MEMIF_PBUF_SIZE_256_BYTES;
	else
		return MT6881_MEMIF_PBUF_SIZE_32_BYTES;
}

/* FE DAIs */
static const struct snd_soc_dai_ops mt6881_memif_dai_ops = {
	.startup        = mt6881_fe_startup,
	.shutdown       = mt6881_fe_shutdown,
	.hw_params      = mtk_afe_fe_hw_params,
	.hw_free        = mtk_afe_fe_hw_free,
	.prepare        = mtk_afe_fe_prepare,
	.trigger        = mt6881_fe_trigger,
};

#define MTK_PCM_RATES (SNDRV_PCM_RATE_8000_48000 |\
		       SNDRV_PCM_RATE_88200 |\
		       SNDRV_PCM_RATE_96000 |\
		       SNDRV_PCM_RATE_176400 |\
		       SNDRV_PCM_RATE_192000)

#define MTK_PCM_DAI_RATES (SNDRV_PCM_RATE_8000 |\
			   SNDRV_PCM_RATE_16000 |\
			   SNDRV_PCM_RATE_32000 |\
			   SNDRV_PCM_RATE_48000)

#define MTK_PCM_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			 SNDRV_PCM_FMTBIT_S24_LE |\
			 SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mt6881_memif_dai_driver[] = {
	/* FE DAIs: memory intefaces to CPU */
	{
		.name = "DL0",
		.id = MT6881_MEMIF_DL0,
		.playback = {
			.stream_name = "DL0",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "DL1",
		.id = MT6881_MEMIF_DL1,
		.playback = {
			.stream_name = "DL1",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "DL2",
		.id = MT6881_MEMIF_DL2,
		.playback = {
			.stream_name = "DL2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "DL3",
		.id = MT6881_MEMIF_DL3,
		.playback = {
			.stream_name = "DL3",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "DL4",
		.id = MT6881_MEMIF_DL4,
		.playback = {
			.stream_name = "DL4",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "DL5",
		.id = MT6881_MEMIF_DL5,
		.playback = {
			.stream_name = "DL5",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "DL6",
		.id = MT6881_MEMIF_DL6,
		.playback = {
			.stream_name = "DL6",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "DL7",
		.id = MT6881_MEMIF_DL7,
		.playback = {
			.stream_name = "DL7",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "DL8",
		.id = MT6881_MEMIF_DL8,
		.playback = {
			.stream_name = "DL8",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "DL23",
		.id = MT6881_MEMIF_DL23,
		.playback = {
			.stream_name = "DL23",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "DL24",
		.id = MT6881_MEMIF_DL24,
		.playback = {
			.stream_name = "DL24",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "DL25",
		.id = MT6881_MEMIF_DL25,
		.playback = {
			.stream_name = "DL25",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "DL_24CH",
		.id = MT6881_MEMIF_DL_24CH,
		.playback = {
			.stream_name = "DL_24CH",
			.channels_min = 1,
			.channels_max = 16,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "DL_48CH",
		.id = MT6881_MEMIF_DL_48CH,
		.playback = {
			.stream_name = "DL_48CH",
			.channels_min = 1,
			.channels_max = 32,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "UL0",
		.id = MT6881_MEMIF_VUL0,
		.capture = {
			.stream_name = "UL0",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "UL1",
		.id = MT6881_MEMIF_VUL1,
		.capture = {
			.stream_name = "UL1",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "UL2",
		.id = MT6881_MEMIF_VUL2,
		.capture = {
			.stream_name = "UL2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "UL3",
		.id = MT6881_MEMIF_VUL3,
		.capture = {
			.stream_name = "UL3",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "UL4",
		.id = MT6881_MEMIF_VUL4,
		.capture = {
			.stream_name = "UL4",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "UL5",
		.id = MT6881_MEMIF_VUL5,
		.capture = {
			.stream_name = "UL5",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "UL6",
		.id = MT6881_MEMIF_VUL6,
		.capture = {
			.stream_name = "UL6",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "UL7",
		.id = MT6881_MEMIF_VUL7,
		.capture = {
			.stream_name = "UL7",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "UL8",
		.id = MT6881_MEMIF_VUL8,
		.capture = {
			.stream_name = "UL8",
			.channels_min = 1,
			.channels_max = 8,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "UL9",
		.id = MT6881_MEMIF_VUL9,
		.capture = {
			.stream_name = "UL9",
			.channels_min = 1,
			.channels_max = 16,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "UL10",
		.id = MT6881_MEMIF_VUL10,
		.capture = {
			.stream_name = "UL10",
			.channels_min = 1,
			.channels_max = 32,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "UL24",
		.id = MT6881_MEMIF_VUL24,
		.capture = {
			.stream_name = "UL24",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "UL25",
		.id = MT6881_MEMIF_VUL25,
		.capture = {
			.stream_name = "UL25",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "UL_CM0",
		.id = MT6881_MEMIF_VUL_CM0,
		.capture = {
			.stream_name = "UL_CM0",
			.channels_min = 1,
			.channels_max = 8,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "UL_CM1",
		.id = MT6881_MEMIF_VUL_CM1,
		.capture = {
			.stream_name = "UL_CM1",
			.channels_min = 1,
			.channels_max = 16,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "UL_CM2",
		.id = MT6881_MEMIF_VUL_CM2,
		.capture = {
			.stream_name = "UL_CM2",
			.channels_min = 1,
			.channels_max = 32,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "UL_ETDM_IN1",
		.id = MT6881_MEMIF_ETDM_IN1,
		.capture = {
			.stream_name = "UL_ETDM_IN1",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "UL_ETDM_IN2",
		.id = MT6881_MEMIF_ETDM_IN2,
		.capture = {
			.stream_name = "UL_ETDM_IN2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
	{
		.name = "UL_ETDM_IN6",
		.id = MT6881_MEMIF_ETDM_IN6,
		.capture = {
			.stream_name = "UL_ETDM_IN6",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6881_memif_dai_ops,
	},
};

/* kcontrol */
static int mt6881_irq_cnt1_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] =
		afe_priv->irq_cnt[MT6881_PRIMARY_MEMIF];
	return 0;
}

static int mt6881_irq_cnt1_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6881_PRIMARY_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;
	int irq_cnt = afe_priv->irq_cnt[memif_num];

	dev_info(afe->dev, "%s(), irq_id %d, irq_cnt = %d, value = %ld\n",
		 __func__,
		 irq_id, irq_cnt,
		 ucontrol->value.integer.value[0]);

	if (irq_cnt == ucontrol->value.integer.value[0])
		return 0;

	irq_cnt = ucontrol->value.integer.value[0];
	afe_priv->irq_cnt[memif_num] = irq_cnt;

	if (pm_runtime_status_suspended(afe->dev) || irq_id < 0) {
		dev_info(afe->dev, "%s(), suspended || irq_id %d, not set\n",
			 __func__, irq_id);
	} else {
		struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
		const struct mtk_base_irq_data *irq_data = irqs->irq_data;

		regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				   irq_data->irq_cnt_maskbit
				   << irq_data->irq_cnt_shift,
				   irq_cnt << irq_data->irq_cnt_shift);
	}

	return 0;
}

static int mt6881_irq_cnt2_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] =
		afe_priv->irq_cnt[MT6881_RECORD_MEMIF];
	return 0;
}

static int mt6881_irq_cnt2_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6881_RECORD_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;
	int irq_cnt = afe_priv->irq_cnt[memif_num];

	dev_info(afe->dev, "%s(), irq_id %d, irq_cnt = %d, value = %ld\n",
		 __func__,
		 irq_id, irq_cnt,
		 ucontrol->value.integer.value[0]);

	if (irq_cnt == ucontrol->value.integer.value[0])
		return 0;

	irq_cnt = ucontrol->value.integer.value[0];
	afe_priv->irq_cnt[memif_num] = irq_cnt;

	if (pm_runtime_status_suspended(afe->dev) || irq_id < 0) {
		dev_info(afe->dev, "%s(), suspended || irq_id %d, not set\n",
			 __func__, irq_id);
	} else {
		struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
		const struct mtk_base_irq_data *irq_data = irqs->irq_data;

		regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				   irq_data->irq_cnt_maskbit
				   << irq_data->irq_cnt_shift,
				   irq_cnt << irq_data->irq_cnt_shift);
	}

	return 0;
}

static int mt6881_deep_irq_cnt_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->irq_cnt[MT6881_DEEP_MEMIF];
	return 0;
}

static int mt6881_deep_irq_cnt_set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6881_DEEP_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;
	int irq_cnt = afe_priv->irq_cnt[memif_num];

	dev_info(afe->dev, "%s(), irq_id %d, irq_cnt = %d, value = %ld\n",
		 __func__,
		 irq_id, irq_cnt,
		 ucontrol->value.integer.value[0]);

	if (irq_cnt == ucontrol->value.integer.value[0])
		return 0;

	irq_cnt = ucontrol->value.integer.value[0];
	afe_priv->irq_cnt[memif_num] = irq_cnt;

	if (pm_runtime_status_suspended(afe->dev) || irq_id < 0) {
		dev_info(afe->dev, "%s(), suspended || irq_id %d, not set\n",
			 __func__, irq_id);
	} else {
		struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
		const struct mtk_base_irq_data *irq_data = irqs->irq_data;

		regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				   irq_data->irq_cnt_maskbit
				   << irq_data->irq_cnt_shift,
				   irq_cnt << irq_data->irq_cnt_shift);
	}

	return 0;
}

static int mt6881_voip_rx_irq_cnt_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->irq_cnt[MT6881_VOIP_MEMIF];
	return 0;
}

static int mt6881_voip_rx_irq_cnt_set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6881_VOIP_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;
	int irq_cnt = afe_priv->irq_cnt[memif_num];

	dev_info(afe->dev, "%s(), irq_id %d, irq_cnt = %d, value = %ld\n",
		 __func__,
		 irq_id, irq_cnt,
		 ucontrol->value.integer.value[0]);

	if (irq_cnt == ucontrol->value.integer.value[0])
		return 0;

	irq_cnt = ucontrol->value.integer.value[0];
	afe_priv->irq_cnt[memif_num] = irq_cnt;

	if (pm_runtime_status_suspended(afe->dev) || irq_id < 0) {
		dev_info(afe->dev, "%s(), suspended || irq_id %d, not set\n",
			 __func__, irq_id);
	} else {
		struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
		const struct mtk_base_irq_data *irq_data = irqs->irq_data;

		regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				   irq_data->irq_cnt_maskbit
				   << irq_data->irq_cnt_shift,
				   irq_cnt << irq_data->irq_cnt_shift);
	}

	return 0;
}

static int mt6881_deep_scene_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->deep_playback_state;
	return 0;
}

static int mt6881_deep_scene_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6881_DEEP_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->deep_playback_state = ucontrol->value.integer.value[0];

	if (afe_priv->deep_playback_state == 1)
		memif->ack_enable = true;
	else
		memif->ack_enable = false;

	return 0;
}

static int mt6881_fast_scene_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->fast_playback_state;
	return 0;
}

static int mt6881_fast_scene_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6881_FAST_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->fast_playback_state = ucontrol->value.integer.value[0];

	if (afe_priv->fast_playback_state == 1)
		memif->use_dram_only = 1;
	else
		memif->use_dram_only = 0;

	return 0;
}

static int mt6881_primary_scene_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->primary_playback_state;
	return 0;
}

static int mt6881_primary_scene_set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6881_PRIMARY_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->primary_playback_state = ucontrol->value.integer.value[0];

	if (afe_priv->primary_playback_state == 1)
		memif->use_dram_only = 1;
	else
		memif->use_dram_only = 0;

	return 0;
}

static int mt6881_voip_scene_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->voip_rx_state;
	return 0;
}

static int mt6881_voip_scene_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6881_VOIP_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->voip_rx_state = ucontrol->value.integer.value[0];

	if (afe_priv->voip_rx_state == 1)
		memif->use_dram_only = 1;
	else
		memif->use_dram_only = 0;

	return 0;
}

static int mt6881_record_xrun_assert_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int xrun_assert = afe_priv->xrun_assert[MT6881_RECORD_MEMIF];

	ucontrol->value.integer.value[0] = xrun_assert;
	return 0;
}

static int mt6881_record_xrun_assert_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int xrun_assert = ucontrol->value.integer.value[0];

	if (xrun_assert != 0)
		dev_info(afe->dev, "%s(), xrun_assert %d\n", __func__, xrun_assert);
	afe_priv->xrun_assert[MT6881_RECORD_MEMIF] = xrun_assert;
	return 0;
}

static int mt6881_echo_ref_xrun_assert_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int xrun_assert = afe_priv->xrun_assert[MT6881_ECHO_REF_MEMIF];

	ucontrol->value.integer.value[0] = xrun_assert;
	return 0;
}

static int mt6881_echo_ref_xrun_assert_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int xrun_assert = ucontrol->value.integer.value[0];

	dev_info(afe->dev, "%s(), xrun_assert %d\n", __func__, xrun_assert);
	afe_priv->xrun_assert[MT6881_ECHO_REF_MEMIF] = xrun_assert;
	return 0;
}

#if !IS_ENABLED(CONFIG_NEBULA_SND_PASSTHROUGH)
static int mt6881_sram_size_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_audio_sram *sram = afe->sram;

	ucontrol->value.integer.value[0] =
		mtk_audio_sram_get_size(sram, sram->prefer_mode);

	return 0;
}
#endif

static int mt6881_vow_barge_in_irq_id_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int memif_num = MT6881_BARGE_IN_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;

	ucontrol->value.integer.value[0] = irq_id;
	return 0;
}


#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
static int mt6881_adsp_ref_mem_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_base_afe_memif *memif;
	int memif_num = get_dsp_task_attr(AUDIO_TASK_CAPTURE_UL1_ID,
					  ADSP_TASK_ATTR_MEMREF);
	if (memif_num < 0)
		return 0;

	memif = &afe->memif[memif_num];
	ucontrol->value.integer.value[0] = memif->use_adsp_share_mem;

	return 0;
}

static int mt6881_adsp_ref_mem_set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_base_afe_memif *memif;
	int memif_num = get_dsp_task_attr(AUDIO_TASK_CAPTURE_UL1_ID,
					  ADSP_TASK_ATTR_MEMREF);
	if (memif_num < 0)
		return 0;

	memif = &afe->memif[memif_num];
	memif->use_adsp_share_mem = ucontrol->value.integer.value[0];

	return 0;
}

static int mt6881_adsp_mem_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_base_afe_memif *memif;
	int memif_num = -1;
	int task_id = get_dsp_task_id_from_str(kcontrol->id.name);

	switch (task_id) {
	case AUDIO_TASK_PRIMARY_ID:
	case AUDIO_TASK_DEEPBUFFER_ID:
	case AUDIO_TASK_FAST_ID:
	case AUDIO_TASK_OFFLOAD_ID:
	case AUDIO_TASK_PLAYBACK_ID:
	case AUDIO_TASK_CALL_FINAL_ID:
	case AUDIO_TASK_KTV_ID:
	case AUDIO_TASK_VOIP_ID:
	case AUDIO_TASK_BTDL_ID:
	case AUDIO_TASK_ECHO_REF_DL_ID:
	case AUDIO_TASK_USBDL_ID:
	case AUDIO_TASK_MDUL_ID:
	case AUDIO_TASK_CALLDL_ID:
#if (IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP) && IS_ENABLED(CONFIG_MTK_ADSP_AUTO_MULTI_PLAYBACK_SUPPORT))
	case AUDIO_TASK_SUB_PLAYBACK_ID:
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_HFP_CLIENT_SUPPORT)
	case AUDIO_TASK_HFP_CLIENT_TX_ADSP_ID:
#endif
		memif_num = get_dsp_task_attr(task_id,
					      ADSP_TASK_ATTR_MEMDL);
		break;
	case AUDIO_TASK_CAPTURE_UL1_ID:
	case AUDIO_TASK_FM_ADSP_ID:
	case AUDIO_TASK_BTUL_ID:
	case AUDIO_TASK_ECHO_REF_ID:
	case AUDIO_TASK_USBUL_ID:
	case AUDIO_TASK_MDDL_ID:
	case AUDIO_TASK_CALLUL_ID:
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_HFP_CLIENT_SUPPORT)
	case AUDIO_TASK_HFP_CLIENT_RX_ADSP_ID:
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_ANC_SUPPORT)
	case AUDIO_TASK_ANC_ADSP_ID:
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_EXTSTREAM_SUPPORT)
	case AUDIO_TASK_EXTSTREAM1_ADSP_ID:
	case AUDIO_TASK_EXTSTREAM2_ADSP_ID:
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
	case AUDIO_TASK_CAPTURE_MCH_ID:
#endif
		memif_num = get_dsp_task_attr(task_id,
					      ADSP_TASK_ATTR_MEMUL);
		break;
	default:
		pr_info("%s(), task_id %d do not use shared mem\n",
			__func__, task_id);
		break;
	};

	if (memif_num < 0)
		return 0;

	memif = &afe->memif[memif_num];
	ucontrol->value.integer.value[0] = memif->use_adsp_share_mem;

	return 0;
}

static int mt6881_adsp_mem_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_base_afe_memif *memif;
	int dl_memif_num = -1;
	int ul_memif_num = -1;
	int ref_memif_num = -1;
	int task_id = get_dsp_task_id_from_str(kcontrol->id.name);
	int old_value;

	switch (task_id) {
	case AUDIO_TASK_PRIMARY_ID:
	case AUDIO_TASK_DEEPBUFFER_ID:
	case AUDIO_TASK_FAST_ID:
	case AUDIO_TASK_OFFLOAD_ID:
	case AUDIO_TASK_VOIP_ID:
	case AUDIO_TASK_BTDL_ID:
	case AUDIO_TASK_ECHO_REF_DL_ID:
	case AUDIO_TASK_USBDL_ID:
	case AUDIO_TASK_MDUL_ID:
	case AUDIO_TASK_CALLDL_ID:
		dl_memif_num = get_dsp_task_attr(task_id,
						 ADSP_TASK_ATTR_MEMDL);
		break;
	case AUDIO_TASK_CAPTURE_UL1_ID:
	case AUDIO_TASK_FM_ADSP_ID:
	case AUDIO_TASK_ECHO_REF_ID:
	case AUDIO_TASK_USBUL_ID:
	case AUDIO_TASK_MDDL_ID:
	case AUDIO_TASK_BTUL_ID:
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
	case AUDIO_TASK_CAPTURE_MCH_ID:
#endif
		ul_memif_num = get_dsp_task_attr(task_id,
						 ADSP_TASK_ATTR_MEMUL);
		break;
	case AUDIO_TASK_CALLUL_ID:
		ul_memif_num = get_dsp_task_attr(task_id,
						 ADSP_TASK_ATTR_MEMUL);
		ref_memif_num = get_dsp_task_attr(task_id,
						 ADSP_TASK_ATTR_MEMREF);
		break;
	case AUDIO_TASK_CALL_FINAL_ID:
	case AUDIO_TASK_PLAYBACK_ID:
	case AUDIO_TASK_KTV_ID:
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_HFP_CLIENT_SUPPORT)
	case AUDIO_TASK_HFP_CLIENT_RX_ADSP_ID:
	case AUDIO_TASK_HFP_CLIENT_TX_ADSP_ID:
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_ANC_SUPPORT)
	case AUDIO_TASK_ANC_ADSP_ID:
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_EXTSTREAM_SUPPORT)
	case AUDIO_TASK_EXTSTREAM1_ADSP_ID:
	case AUDIO_TASK_EXTSTREAM2_ADSP_ID:
#endif
#if (IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP) && IS_ENABLED(CONFIG_MTK_ADSP_AUTO_MULTI_PLAYBACK_SUPPORT))
	case AUDIO_TASK_SUB_PLAYBACK_ID:
#endif
		dl_memif_num = get_dsp_task_attr(task_id,
						 ADSP_TASK_ATTR_MEMDL);
		ul_memif_num = get_dsp_task_attr(task_id,
						 ADSP_TASK_ATTR_MEMUL);
		ref_memif_num = get_dsp_task_attr(task_id,
						  ADSP_TASK_ATTR_MEMREF);
		break;
	default:
		pr_info("%s(), task_id %d do not use shared mem\n",
			__func__, task_id);
		break;
	};

	if (dl_memif_num >= 0) {
		memif = &afe->memif[dl_memif_num];
		old_value = memif->use_adsp_share_mem;
		memif->use_adsp_share_mem = ucontrol->value.integer.value[0];

		pr_info("%s(), dl:%s, use_adsp_share_mem %d->%d\n",
			__func__, memif->data->name, old_value, memif->use_adsp_share_mem);

		if (memif->substream && memif->use_adsp_share_mem == 0 && old_value) {
			memif->err_close_order = true;
			AUDIO_AEE("disable adsp memory used before substream shutdown");
		}
	}

	if (ul_memif_num >= 0) {
		memif = &afe->memif[ul_memif_num];
		old_value = memif->use_adsp_share_mem;
		memif->use_adsp_share_mem = ucontrol->value.integer.value[0];

		pr_info("%s(), ul:%s, use_adsp_share_mem %d->%d\n",
			__func__, memif->data->name, old_value, memif->use_adsp_share_mem);

		if (memif->substream && memif->use_adsp_share_mem == 0 && old_value) {
			memif->err_close_order = true;
			AUDIO_AEE("disable adsp memory used before substream shutdown");
		}
	}

	if (ref_memif_num >= 0) {
		memif = &afe->memif[ref_memif_num];
		old_value = memif->use_adsp_share_mem;
		memif->use_adsp_share_mem = ucontrol->value.integer.value[0];

		pr_info("%s(), ref:%s, use_adsp_share_mem %d->%d\n",
			__func__, memif->data->name, old_value, memif->use_adsp_share_mem);

		if (memif->substream && memif->use_adsp_share_mem == 0 && old_value) {
			memif->err_close_order = true;
			AUDIO_AEE("disable adsp memory used before substream shutdown");
		}
	}

	return 0;
}
#endif

#if !IS_ENABLED(CONFIG_NEBULA_SND_PASSTHROUGH)
static int mt6881_mmap_dl_scene_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->mmap_playback_state;
	return 0;
}

static int mt6881_mmap_dl_scene_set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6881_MMAP_DL_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->mmap_playback_state = ucontrol->value.integer.value[0];

	if (afe_priv->mmap_playback_state == 1) {
		unsigned long phy_addr;
		void *vir_addr;

		mtk_get_mmap_dl_buffer(&phy_addr, &vir_addr);

		if (phy_addr != 0x0 && vir_addr)
			memif->use_mmap_share_mem = 1;
	} else {
		memif->use_mmap_share_mem = 0;
	}

	dev_info(afe->dev, "%s(), state %d, mem %d\n", __func__,
		 afe_priv->mmap_playback_state, memif->use_mmap_share_mem);
	return 0;
}

static int mt6881_mmap_ul_scene_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->mmap_record_state;
	return 0;
}

static int mt6881_mmap_ul_scene_set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6881_MMAP_UL_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->mmap_record_state = ucontrol->value.integer.value[0];

	if (afe_priv->mmap_record_state == 1) {
		unsigned long phy_addr;
		void *vir_addr;

		mtk_get_mmap_ul_buffer(&phy_addr, &vir_addr);

		if (phy_addr != 0x0 && vir_addr)
			memif->use_mmap_share_mem = 2;
	} else {
		memif->use_mmap_share_mem = 0;
	}

	dev_info(afe->dev, "%s(), state %d, mem %d\n", __func__,
		 afe_priv->mmap_record_state, memif->use_mmap_share_mem);
	return 0;
}

static int mt6881_mmap_ion_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int mt6881_mmap_ion_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);

	dev_info(afe->dev, "%s() successfully start\n", __func__);
	mtk_exporter_init(afe->dev);
	return 0;
}

static int mt6881_dl_mmap_fd_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int memif_num = MT6881_MMAP_DL_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	ucontrol->value.integer.value[0] = (memif->use_mmap_share_mem == 1) ?
					    mtk_get_mmap_dl_fd() : 0;
	dev_info(afe->dev, "%s, fd %ld\n", __func__,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int mt6881_dl_mmap_fd_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int mt6881_ul_mmap_fd_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int memif_num = MT6881_MMAP_UL_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	ucontrol->value.integer.value[0] = (memif->use_mmap_share_mem == 2) ?
					    mtk_get_mmap_ul_fd() : 0;
	dev_info(afe->dev, "%s, fd %ld\n", __func__,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int mt6881_ul_mmap_fd_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_NEBULA_SND_PASSTHROUGH)
static int mt6881_set_memif_sram_mode(struct device *dev,
				      enum mtk_audio_sram_mode sram_mode);
static int mt6881_set_sram_mode(struct device *dev,
				enum mtk_audio_sram_mode sram_mode);

static int mt6881_use_dram_only_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int i;

	for (i = 0; i < MT6881_MEMIF_NUM; i++)
		ucontrol->value.bytes.data[i] = afe->memif[i].use_dram_only;

	return 0;
}

static int mt6881_sram_mode_set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int rc = 0;

	rc = mt6881_set_sram_mode(afe->dev, ucontrol->value.bytes.data[0]);

	return rc;
}

#define VIRTIO_PASSTHROUGH_SHM_CMD_REG_DRAM 0
#define VIRTIO_PASSTHROUGH_SHM_CMD_UNREG_DRAM 1
#define VIRTIO_PASSTHROUGH_SHM_CMD_REG_SRAM 2
#define VIRTIO_PASSTHROUGH_SHM_CMD_UNREG_SRAM 3

struct virtio_passthrough_shm_msg {
	uint8_t cmd;
	uint8_t pcm_id;
	uint8_t padding[6];
	uint64_t pa;
	uint64_t bytes;
};

// according to mt6881_mt6681_dai_links
// c0d15c -> capture 9 -> UL9 -> memif id 24
unsigned int virtio_id_memif_index_mapping[MT6881_MEMIF_NUM] = {
	MT6881_MEMIF_DL0,
	MT6881_MEMIF_DL1,
	MT6881_MEMIF_DL2,
	MT6881_MEMIF_DL3,
	MT6881_MEMIF_DL4,
	MT6881_MEMIF_DL5,
	MT6881_MEMIF_DL6,
	MT6881_MEMIF_DL7,
	MT6881_MEMIF_DL8,
	MT6881_MEMIF_DL23,
	MT6881_MEMIF_DL24,
	MT6881_MEMIF_DL25,
	MT6881_MEMIF_DL_24CH,
	MT6881_MEMIF_DL_48CH,
	MT6881_MEMIF_VUL9,
	MT6881_MEMIF_VUL1,
	MT6881_MEMIF_VUL0,
	MT6881_MEMIF_VUL3,
	MT6881_MEMIF_VUL7,
	MT6881_MEMIF_VUL4,
	MT6881_MEMIF_VUL2,
	MT6881_MEMIF_VUL5,
	MT6881_MEMIF_VUL_CM0,
	MT6881_MEMIF_VUL_CM1,
	MT6881_MEMIF_VUL_CM2,
	MT6881_MEMIF_VUL10,
	MT6881_MEMIF_VUL6,
	MT6881_MEMIF_VUL25,
	MT6881_MEMIF_VUL8,
	MT6881_MEMIF_VUL24,
	MT6881_MEMIF_ETDM_IN1,
	MT6881_MEMIF_ETDM_IN2,
	MT6881_MEMIF_ETDM_IN6,
};

static int mt6881_passthrough_shm_set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct virtio_passthrough_shm_msg *shm =
			(struct virtio_passthrough_shm_msg *)ucontrol->value.bytes.data;
	struct mtk_base_afe_memif *memif;
	int rc = 0;
	int ret;
	int memif_id = 0;

	if (shm->pcm_id >= MT6881_MEMIF_NUM) {
		pr_info("%s(), shm->pcm_id = %d is invalid\n", __func__, shm->pcm_id);
		return -EINVAL;
	}

	memif_id = virtio_id_memif_index_mapping[shm->pcm_id];
	if (!(memif_id >= 0 && memif_id < MT6881_MEMIF_NUM))
		return -EINVAL;

	// TODO check pa is in valid range

	memif = &afe->memif[memif_id];

	switch(shm->cmd) {
	case VIRTIO_PASSTHROUGH_SHM_CMD_REG_DRAM:
		if (shm->pa && shm->bytes) {
			unreg_dram_passthrough_shm(memif);
			memif->dram_dma_area = phys_to_virt(shm->pa);
			memif->dram_dma_addr = shm->pa;
			memif->dram_dma_bytes = shm->bytes;
		} else {
			rc = -EINVAL;
		}
		break;
	case VIRTIO_PASSTHROUGH_SHM_CMD_UNREG_DRAM:
		unreg_dram_passthrough_shm(memif);
		break;
	case VIRTIO_PASSTHROUGH_SHM_CMD_REG_SRAM:
		if (shm->pa && shm->bytes) {
			unreg_sram_passthrough_shm(memif);
			memif->sram_dma_area = ioremap(shm->pa, shm->bytes);
			memif->sram_dma_addr = shm->pa;
			memif->sram_dma_bytes = shm->bytes;
		} else {
			rc = -EINVAL;
		}
		break;
	case VIRTIO_PASSTHROUGH_SHM_CMD_UNREG_SRAM:
		unreg_sram_passthrough_shm(memif);
		break;
	default:
		rc = -EINVAL;
	}

	return rc;
}
#endif

static int record_miso1_en_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;

	pr_info("%s(), audio_r_miso1_enable = %d\n", __func__, afe_priv->audio_r_miso1_enable);

	ucontrol->value.integer.value[0] = afe_priv->audio_r_miso1_enable;
	return 0;
}

static int record_miso1_en_set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;

	afe_priv->audio_r_miso1_enable = ucontrol->value.integer.value[0];
	pr_info("%s(), audio_r_miso1_enable = %d\n", __func__, afe_priv->audio_r_miso1_enable);

	return 0;
}
#ifndef SKIP_VIP
// ALPS08709395: AudioQOS, set VIP
static int mt6881_audio_vip_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int mt6881_audio_vip_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int pid = ucontrol->value.integer.value[0];

	dev_info(afe->dev, "%s(), set VIP, thread %d\n", __func__, pid);

	// API has no return value
	set_task_basic_vip(pid);

	return 0;
}

// ALPS08907586: media.swcodec set VIP
static pid_t find_process_by_name(const char *name)
{
	struct task_struct *task;
	struct task_struct *thread;

	for_each_process(task) {
		for_each_thread(task, thread) {
			if (strcmp(thread->comm, name) == 0)
				return task->pid;
		}
	}

	return -1; // Process not found
}

static void set_swcodec_vip(const struct mtk_base_afe *afe,
		const char *name, const pid_t pid, const int enable)
{
	struct task_struct *task;
	struct task_struct *thread;

	dev_info(afe->dev, "%s(), pid %d, enable %d\n", __func__, pid, enable);
	for_each_process(task) {
		if ( pid != task->pid)
			continue;

		for_each_thread(task, thread) {
			if (strcmp(thread->comm, name) != 0)
				continue;

			if (enable == 1)
				set_task_basic_vip(thread->pid);
			else
				unset_task_basic_vip(thread->pid);
			dev_info(afe->dev, "%s(), Found thread, %s %d\n",
				 __func__, thread->comm, thread->pid);
		}
		return;
	}
	dev_info(afe->dev, "%s(), can't Found process\n", __func__);
}

static int mt6881_swcodec_vip_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int mt6881_swcodec_vip_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	static pid_t PidMediaSWCodec = -1;
	char name[20] = "oid.aac.encoder";
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int enable = ucontrol->value.integer.value[0];

	dev_info(afe->dev, "%s(), enable %d, pid %d\n", __func__,
		 enable, PidMediaSWCodec);
	if (enable == 1)
		PidMediaSWCodec = find_process_by_name(name);

	if (PidMediaSWCodec != -1)
		set_swcodec_vip(afe, name, PidMediaSWCodec, enable);
	dev_info(afe->dev, "%s()-\n", __func__);

	return 0;
}
#endif
static int mt6881_mmap_clean_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int mt6881_mmap_clean_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);

	mtk_clean_mmap_dl_buffer(afe->dev);

	return 0;
}

static const char *const off_on_function[] = {"Off", "On"};

static const struct soc_enum mt6881_pcm_type_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(off_on_function),
			    off_on_function),
};

static const struct snd_kcontrol_new mt6881_pcm_kcontrols[] = {
	SOC_SINGLE_EXT("Audio IRQ1 CNT", SND_SOC_NOPM, 0, 0x3ffff, 0,
		       mt6881_irq_cnt1_get, mt6881_irq_cnt1_set),
	SOC_SINGLE_EXT("Audio IRQ2 CNT", SND_SOC_NOPM, 0, 0x3ffff, 0,
		       mt6881_irq_cnt2_get, mt6881_irq_cnt2_set),
	SOC_SINGLE_EXT("deep_buffer_irq_cnt", SND_SOC_NOPM, 0, 0x3ffff, 0,
		       mt6881_deep_irq_cnt_get, mt6881_deep_irq_cnt_set),
	SOC_SINGLE_EXT("voip_rx_irq_cnt", SND_SOC_NOPM, 0, 0x3ffff, 0,
		       mt6881_voip_rx_irq_cnt_get, mt6881_voip_rx_irq_cnt_set),
	SOC_SINGLE_EXT("deep_buffer_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_deep_scene_get, mt6881_deep_scene_set),
	SOC_SINGLE_EXT("record_xrun_assert", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_record_xrun_assert_get,
		       mt6881_record_xrun_assert_set),
	SOC_SINGLE_EXT("echo_ref_xrun_assert", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_echo_ref_xrun_assert_get,
		       mt6881_echo_ref_xrun_assert_set),
	SOC_SINGLE_EXT("fast_play_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_fast_scene_get, mt6881_fast_scene_set),
	SOC_SINGLE_EXT("primary_play_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_primary_scene_get, mt6881_primary_scene_set),
	SOC_SINGLE_EXT("voip_rx_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_voip_scene_get, mt6881_voip_scene_set),
#if !IS_ENABLED(CONFIG_NEBULA_SND_PASSTHROUGH)
	SOC_SINGLE_EXT("sram_size", SND_SOC_NOPM, 0, 0xffffffff, 0,
		       mt6881_sram_size_get, NULL),
#endif
	SOC_SINGLE_EXT("vow_barge_in_irq_id", SND_SOC_NOPM, 0, 0x3ffff, 0,
		       mt6881_vow_barge_in_irq_id_get, NULL),
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
	SOC_SINGLE_EXT("adsp_primary_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_mem_get,
		       mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_deepbuffer_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_mem_get,
		       mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_dynamic_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_mem_get,
		       mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_voip_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_mem_get,
		       mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_playback_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_mem_get,
		       mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_call_final_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_mem_get,
		       mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_ktv_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_mem_get,
		       mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_fm_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_mem_get,
		       mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_offload_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_mem_get,
		       mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_capture_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_mem_get,
		       mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_ref_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_ref_mem_get,
		       mt6881_adsp_ref_mem_set),
	SOC_SINGLE_EXT("adsp_fast_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_mem_get,
		       mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_spatializer_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_mem_get,
		       mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_btdl_sharemem_scenario",
			   SND_SOC_NOPM, 0, 0x1, 0,
			   mt6881_adsp_mem_get,
			   mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_btul_sharemem_scenario",
			   SND_SOC_NOPM, 0, 0x1, 0,
			   mt6881_adsp_mem_get,
			   mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_echoref_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_mem_get,
		       mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_echodl_sharemem_scenario",
			   SND_SOC_NOPM, 0, 0x1, 0,
			   mt6881_adsp_mem_get,
			   mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_usbdl_sharemem_scenario",
			   SND_SOC_NOPM, 0, 0x1, 0,
			   mt6881_adsp_mem_get,
			   mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_usbul_sharemem_scenario",
			   SND_SOC_NOPM, 0, 0x1, 0,
			   mt6881_adsp_mem_get,
			   mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_mddl_sharemem_scenario",
			   SND_SOC_NOPM, 0, 0x1, 0,
			   mt6881_adsp_mem_get,
			   mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_mdul_sharemem_scenario",
			   SND_SOC_NOPM, 0, 0x1, 0,
			   mt6881_adsp_mem_get,
			   mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_calldl_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_mem_get,
		       mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_callul_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_mem_get,
		       mt6881_adsp_mem_set),
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_HFP_CLIENT_SUPPORT)
	SOC_SINGLE_EXT("adsp_hfp_client_rx_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_mem_get,
		       mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_hfp_client_tx_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_mem_get,
		       mt6881_adsp_mem_set),
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_ANC_SUPPORT)
	SOC_SINGLE_EXT("adsp_anc_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_mem_get,
		       mt6881_adsp_mem_set),
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_EXTSTREAM_SUPPORT)
	SOC_SINGLE_EXT("adsp_extstream1_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_mem_get,
		       mt6881_adsp_mem_set),
	SOC_SINGLE_EXT("adsp_extstream2_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_mem_get,
		       mt6881_adsp_mem_set),
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_MULTI_PLAYBACK_SUPPORT)
	SOC_SINGLE_EXT("adsp_subpb_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_mem_get,
		       mt6881_adsp_mem_set),
#endif
	SOC_SINGLE_EXT("adsp_capmch_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_adsp_mem_get,
		       mt6881_adsp_mem_set),
#endif
#endif
#if !IS_ENABLED(CONFIG_NEBULA_SND_PASSTHROUGH)
	SOC_SINGLE_EXT("mmap_play_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_mmap_dl_scene_get, mt6881_mmap_dl_scene_set),
	SOC_SINGLE_EXT("mmap_record_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6881_mmap_ul_scene_get, mt6881_mmap_ul_scene_set),
	SOC_SINGLE_EXT("aaudio_ion",
		       SND_SOC_NOPM, 0, 0xffffffff, 0,
		       mt6881_mmap_ion_get,
		       mt6881_mmap_ion_set),
	SOC_SINGLE_EXT("aaudio_dl_mmap_fd",
		       SND_SOC_NOPM, 0, 0xffffffff, 0,
		       mt6881_dl_mmap_fd_get,
		       mt6881_dl_mmap_fd_set),
	SOC_SINGLE_EXT("aaudio_ul_mmap_fd",
		       SND_SOC_NOPM, 0, 0xffffffff, 0,
		       mt6881_ul_mmap_fd_get,
		       mt6881_ul_mmap_fd_set),
#endif
	SOC_ENUM_EXT("MTK_RECORD_MISO1", mt6881_pcm_type_enum[0],
		     record_miso1_en_get, record_miso1_en_set),
#ifndef SKIP_VIP
	SOC_SINGLE_EXT("audio_vip", SND_SOC_NOPM, 0, 0x3fffff, 0,
		       mt6881_audio_vip_get, mt6881_audio_vip_set),
	SOC_SINGLE_EXT("SWCODEC_VIP", SND_SOC_NOPM, 0, 0x3fffff, 0,
		       mt6881_swcodec_vip_get, mt6881_swcodec_vip_set),
#endif
	SOC_SINGLE_EXT("aaudio_clean_buffer",
		       SND_SOC_NOPM, 0, 0xffffffff, 0,
		       mt6881_mmap_clean_get,
		       mt6881_mmap_clean_set),
#if IS_ENABLED(CONFIG_NEBULA_SND_PASSTHROUGH)
	SND_SOC_BYTES_EXT("use_dram_only", MT6881_MEMIF_NUM,
		      mt6881_use_dram_only_get, NULL),
	SND_SOC_BYTES_EXT("sram_mode", 1,
		      NULL, mt6881_sram_mode_set),
	SND_SOC_BYTES_EXT("passthrough_shm",
		      sizeof(struct virtio_passthrough_shm_msg),
		      NULL, mt6881_passthrough_shm_set),
#endif
};

enum {
	CM0_MUX_VUL8_2CH,
	CM0_MUX_VUL8_8CH,
	CM0_MUX_MASK,
};
enum {
	CM1_MUX_VUL9_2CH,
	CM1_MUX_VUL9_16CH,
	CM1_MUX_MASK,
};

static int ul_cm0_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol,
			int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	unsigned int channels = 0;

	dev_dbg(afe->dev, "%s(), event 0x%x, name %s\n",
		 __func__, event, w->name);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		channels = mtk_get_channel_value();
		mt6881_enable_cm_bypass(afe, CM0, 0x0);
		mt6881_set_cm(afe, CM0, 0x1, false, channels);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		mt6881_enable_cm_bypass(afe, CM0, 0x1);
		break;
	default:
		break;
	}
	return 0;
}

static int ul_cm1_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol,
			int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	unsigned int channels = 0;

	dev_dbg(afe->dev, "%s(), event 0x%x, name %s\n",
		 __func__, event, w->name);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		channels = mtk_get_channel_value();
		mt6881_enable_cm_bypass(afe, CM1, 0x0);
		mt6881_set_cm(afe, CM1, 0x1, false, channels);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* remove so we fix in normal mode */
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO)
		mt6881_enable_cm_bypass(afe, CM1, 0x1);
#endif
		break;
	default:
		break;
	}
	return 0;
}


static int ul_cm2_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol,
			int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	unsigned int channels = 0;

	dev_dbg(afe->dev, "%s(), event 0x%x, name %s\n",
		 __func__, event, w->name);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		channels = mtk_get_channel_value();
		mt6881_enable_cm_bypass(afe, CM2, 0x0);
		mt6881_set_cm(afe, CM2, 0x1, false, channels);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* remove so we fix in normal mode */
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO)
		mt6881_enable_cm_bypass(afe, CM2, 0x1);
#endif
		break;
	default:
		break;
	}
	return 0;
}

/* dma widget & routes*/
static const struct snd_kcontrol_new memif_ul0_ch1_mix[] = {
	/* Normal record */
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN018_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN018_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN018_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN018_0,
	I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN018_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN018_0,
				    I_ADDA_UL_CH6, 1, 0),
	/* FM */
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH1", AFE_CONN018_0,
				    I_CONNSYS_I2S_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN018_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN018_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN018_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN018_1,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN018_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN018_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH1", AFE_CONN018_1,
				    I_DL7_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL23_CH1", AFE_CONN018_3,
				    I_DL44_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN018_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_48CH_CH1", AFE_CONN018_2,
				    I_DL_48CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN018_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN018_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH1", AFE_CONN018_4,
				    I_I2SIN0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH1", AFE_CONN018_4,
				    I_I2SIN1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN2_CH1", AFE_CONN018_4,
					I_I2SIN2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH1", AFE_CONN018_6,
				    I_SRC_0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH1", AFE_CONN018_6,
				    I_SRC_2_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul0_ch2_mix[] = {
	/* Normal record */
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN019_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN019_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN019_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN019_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN019_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN019_0,
				    I_ADDA_UL_CH6, 1, 0),
	/* FM */
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH2", AFE_CONN019_0,
				    I_CONNSYS_I2S_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN019_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN019_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN019_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN019_1,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN019_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN019_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH2", AFE_CONN019_1,
				    I_DL7_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL23_CH2", AFE_CONN019_3,
				    I_DL44_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN019_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_48CH_CH2", AFE_CONN019_2,
				    I_DL_48CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN019_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN019_4,
				    I_PCM_0_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN019_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH2", AFE_CONN019_4,
				    I_PCM_1_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH2", AFE_CONN019_4,
				    I_I2SIN0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH2", AFE_CONN019_4,
				    I_I2SIN1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN2_CH2", AFE_CONN019_4,
					I_I2SIN2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH2", AFE_CONN019_6,
				    I_SRC_0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH2", AFE_CONN019_6,
				    I_SRC_2_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul1_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN020_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH1", AFE_CONN020_0,
				    I_CONNSYS_I2S_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN020_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN020_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN020_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN020_1,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN020_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN020_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH1", AFE_CONN020_1,
				    I_DL7_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL23_CH1", AFE_CONN020_3,
				    I_DL44_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH1", AFE_CONN020_3,
					I_DL45_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN020_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_48CH_CH1", AFE_CONN020_2,
				    I_DL_48CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN020_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN020_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH1", AFE_CONN020_4,
				    I_I2SIN0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH1", AFE_CONN020_4,
				    I_I2SIN1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN2_CH1", AFE_CONN020_4,
				    I_I2SIN2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH1", AFE_CONN020_5,
				    I_I2SIN6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH1", AFE_CONN020_6,
				    I_SRC_0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH1", AFE_CONN020_6,
				    I_SRC_2_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul1_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN021_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH2", AFE_CONN021_0,
				    I_CONNSYS_I2S_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN021_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN021_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN021_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN021_1,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN021_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN021_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH2", AFE_CONN021_1,
				    I_DL7_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL23_CH2", AFE_CONN021_3,
				    I_DL44_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH2", AFE_CONN021_3,
					I_DL45_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN021_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_48CH_CH2", AFE_CONN021_2,
				    I_DL_48CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN021_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN021_4,
				    I_PCM_0_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN021_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH2", AFE_CONN021_4,
				    I_PCM_1_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH2", AFE_CONN021_4,
				    I_I2SIN0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH2", AFE_CONN021_4,
				    I_I2SIN1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN2_CH2", AFE_CONN021_4,
				    I_I2SIN2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH2", AFE_CONN021_5,
				    I_I2SIN6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH2", AFE_CONN021_6,
				    I_SRC_0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH2", AFE_CONN021_6,
				    I_SRC_2_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul2_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN022_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN022_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN022_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN022_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN1_OUT_CH1", AFE_CONN022_0,
				    I_GAIN1_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH1", AFE_CONN022_6,
				    I_SRC_1_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH1", AFE_CONN022_5,
				    I_I2SIN6_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul2_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN023_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN023_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN023_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN023_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN1_OUT_CH2", AFE_CONN023_0,
				    I_GAIN1_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH2", AFE_CONN023_6,
				    I_SRC_1_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH2", AFE_CONN023_5,
				    I_I2SIN6_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul3_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN024_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH1", AFE_CONN024_4,
				    I_I2SIN0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH1", AFE_CONN024_4,
				    I_I2SIN1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH1", AFE_CONN024_5,
				    I_I2SIN6_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul3_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN025_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH2", AFE_CONN025_4,
				    I_I2SIN0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH2", AFE_CONN025_4,
				    I_I2SIN1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH2", AFE_CONN025_5,
				    I_I2SIN6_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul4_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN026_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN026_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN026_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN026_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN026_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN026_1,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN026_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_48CH_CH1", AFE_CONN026_2,
				    I_DL_48CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN026_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN026_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH1", AFE_CONN026_4,
				    I_I2SIN0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH1", AFE_CONN026_4,
				    I_I2SIN1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH1", AFE_CONN026_5,
				    I_I2SIN6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN0_OUT_CH1", AFE_CONN026_0,
				    I_GAIN0_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul4_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN027_0,
					I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN027_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN027_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN027_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN027_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN027_1,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN027_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_48CH_CH2", AFE_CONN027_2,
				    I_DL_48CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN027_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN027_4,
				    I_PCM_0_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN027_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH2", AFE_CONN027_4,
				    I_PCM_1_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH2", AFE_CONN027_4,
					I_I2SIN0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH2", AFE_CONN027_4,
				    I_I2SIN1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH2", AFE_CONN027_5,
				    I_I2SIN6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN0_OUT_CH2", AFE_CONN027_0,
				    I_GAIN0_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul5_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN028_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN028_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN028_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN028_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN028_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN028_1,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN028_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_48CH_CH1", AFE_CONN028_2,
				    I_DL_48CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN028_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN028_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("GAIN0_OUT_CH1", AFE_CONN028_0,
				    I_GAIN0_OUT_CH1, 1, 0),
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN028_1,
				    I_DL5_CH1, 1, 0),
#endif
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH1", AFE_CONN028_0,
				    I_CONNSYS_I2S_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul5_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN029_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN029_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN029_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN029_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN029_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN029_1,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN029_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_48CH_CH2", AFE_CONN029_2,
				    I_DL_48CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN029_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN029_4,
				    I_PCM_0_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN029_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH2", AFE_CONN029_4,
				    I_PCM_1_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("GAIN0_OUT_CH2", AFE_CONN029_0,
				    I_GAIN0_OUT_CH2, 1, 0),
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN029_1,
				    I_DL5_CH2, 1, 0),
#endif
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH2", AFE_CONN029_0,
				    I_CONNSYS_I2S_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul6_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN030_0,
					I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH1", AFE_CONN030_0,
				    I_CONNSYS_I2S_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN030_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN030_1,
				    I_DL2_CH1, 1, 0),
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN030_1,
				    I_DL6_CH1, 1, 0),
#endif
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN2_CH1", AFE_CONN030_4,
				    I_I2SIN2_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul6_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN031_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH2", AFE_CONN031_0,
				    I_CONNSYS_I2S_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN031_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN031_1,
				    I_DL2_CH2, 1, 0),
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN031_1,
				    I_DL6_CH2, 1, 0),
#endif
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN2_CH2", AFE_CONN031_4,
				    I_I2SIN2_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul7_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN032_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN032_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN032_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN032_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN032_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN032_0,
				    I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH1", AFE_CONN032_0,
				    I_CONNSYS_I2S_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN0_OUT_CH1", AFE_CONN032_0,
				    I_GAIN0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN032_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN032_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN2_CH1", AFE_CONN032_4,
					I_I2SIN2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH1", AFE_CONN032_5,
				    I_I2SIN6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH3", AFE_CONN032_5,
				    I_I2SIN6_CH3, 1, 0),
};

static const struct snd_kcontrol_new memif_ul7_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN033_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN033_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN033_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN033_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN033_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN033_0,
				    I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH2", AFE_CONN033_0,
				    I_CONNSYS_I2S_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN0_OUT_CH2", AFE_CONN033_0,
				    I_GAIN0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN033_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN033_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN2_CH2", AFE_CONN033_4,
					I_I2SIN2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH2", AFE_CONN033_5,
				    I_I2SIN6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH4", AFE_CONN033_5,
				    I_I2SIN6_CH6, 1, 0),
};

static const struct snd_kcontrol_new memif_ul8_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN034_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN034_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN034_4,
				    I_PCM_1_CAP_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul8_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN035_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN035_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN035_4,
				    I_PCM_0_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN035_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH2", AFE_CONN035_4,
				    I_PCM_1_CAP_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul9_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN036_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN036_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN036_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH1", AFE_CONN036_5,
				    I_I2SIN6_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul9_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN037_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN037_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN037_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN037_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH2", AFE_CONN037_5,
				    I_I2SIN6_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul10_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN038_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN038_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN038_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN038_0,
				    I_ADDA_UL_CH4, 1, 0),
};

static const struct snd_kcontrol_new memif_ul10_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN039_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN039_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN039_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN039_0,
				    I_ADDA_UL_CH4, 1, 0),
};

static const struct snd_kcontrol_new memif_ul24_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN066_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH1", AFE_CONN066_0,
				    I_CONNSYS_I2S_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN066_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN066_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN066_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN066_1,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN066_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN066_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH1", AFE_CONN066_1,
				    I_DL7_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL23_CH1", AFE_CONN066_3,
				    I_DL44_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH1", AFE_CONN066_3,
				    I_DL45_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN066_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_48CH_CH1", AFE_CONN066_2,
				    I_DL_48CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN066_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN066_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH1", AFE_CONN066_4,
				    I_I2SIN0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN2_CH1", AFE_CONN066_4,
				    I_I2SIN2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH1", AFE_CONN066_5,
				    I_I2SIN6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH1", AFE_CONN066_6,
				    I_SRC_0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH1", AFE_CONN066_6,
				    I_SRC_2_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul24_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN067_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH2", AFE_CONN067_0,
				    I_CONNSYS_I2S_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN067_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN067_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN067_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN067_1,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN067_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN067_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH2", AFE_CONN067_1,
				    I_DL7_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL23_CH2", AFE_CONN067_3,
				    I_DL44_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH2", AFE_CONN067_3,
				    I_DL45_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN067_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_48CH_CH2", AFE_CONN067_2,
				    I_DL_48CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN067_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN067_4,
				    I_PCM_0_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN067_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH2", AFE_CONN067_4,
				    I_PCM_1_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH2", AFE_CONN067_4,
				    I_I2SIN0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH2", AFE_CONN067_4,
				    I_I2SIN1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN2_CH2", AFE_CONN067_4,
				    I_I2SIN2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH2", AFE_CONN067_5,
				    I_I2SIN6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH2", AFE_CONN067_6,
				    I_SRC_0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH2", AFE_CONN067_6,
				    I_SRC_2_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_dsp_dl_playback_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL0", SND_SOC_NOPM, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL2", SND_SOC_NOPM, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL1", SND_SOC_NOPM, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL6", SND_SOC_NOPM, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL3", SND_SOC_NOPM, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL_24CH", SND_SOC_NOPM, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL_48CH", SND_SOC_NOPM, 0, 1, 0),
};

static const struct snd_kcontrol_new memif_ul_cm0_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN040_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN040_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN040_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN040_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN1_OUT_CH1", AFE_CONN040_0,
				    I_GAIN1_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH1", AFE_CONN040_6,
				    I_SRC_0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH1", AFE_CONN040_6,
				    I_SRC_1_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH1", AFE_CONN040_5,
				    I_I2SIN6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH3", AFE_CONN040_5,
				    I_I2SIN6_CH3, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm0_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN041_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN041_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN041_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN041_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN1_OUT_CH2", AFE_CONN041_0,
				    I_GAIN1_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH2", AFE_CONN041_6,
				    I_SRC_0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH2", AFE_CONN041_6,
				    I_SRC_1_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH2", AFE_CONN041_5,
				    I_I2SIN6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH4", AFE_CONN041_5,
				    I_I2SIN6_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm0_ch3_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN042_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN042_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN042_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN042_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH1", AFE_CONN042_5,
				    I_I2SIN6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH3", AFE_CONN042_5,
				    I_I2SIN6_CH3, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm0_ch4_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN043_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN043_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN043_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN043_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH2", AFE_CONN043_5,
				    I_I2SIN6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH4", AFE_CONN043_5,
				    I_I2SIN6_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm0_ch5_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN044_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN044_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN044_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN044_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH5", AFE_CONN044_5,
				    I_I2SIN6_CH5, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm0_ch6_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN045_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN045_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN045_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN045_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH6", AFE_CONN045_5,
				    I_I2SIN6_CH6, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm0_ch7_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN046_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN046_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN046_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN046_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH7", AFE_CONN046_5,
				    I_I2SIN6_CH7, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm0_ch8_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN047_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN047_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN047_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN047_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH8", AFE_CONN047_5,
				    I_I2SIN6_CH8, 1, 0),
};

static const struct snd_kcontrol_new memif_ul_cm1_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN048_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN048_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN048_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN048_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN048_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN048_0,
				    I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH1", AFE_CONN048_6,
				    I_SRC_0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH1", AFE_CONN048_5,
				    I_I2SIN6_CH1, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN049_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN049_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN049_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN049_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN049_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN049_0,
				    I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH2", AFE_CONN049_6,
				    I_SRC_0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH2", AFE_CONN049_5,
				    I_I2SIN6_CH2, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch3_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN050_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN050_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN050_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN050_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN050_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN050_0,
				    I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH3", AFE_CONN050_5,
						I_I2SIN6_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH1", AFE_CONN050_6,
				    I_SRC_1_OUT_CH1, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch4_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN051_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN051_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN051_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN051_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN051_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN051_0,
				    I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH4", AFE_CONN051_5,
				    I_I2SIN6_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH2", AFE_CONN051_6,
				    I_SRC_1_OUT_CH2, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch5_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN052_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN052_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN052_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN052_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN052_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN052_0,
				    I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH3", AFE_CONN052_5,
				    I_I2SIN6_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH5", AFE_CONN052_5,
				    I_I2SIN6_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH1", AFE_CONN052_6,
				    I_SRC_1_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH1", AFE_CONN052_6,
				    I_SRC_2_OUT_CH1, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch6_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN053_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN053_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN053_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN053_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN053_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN053_0,
				    I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH4", AFE_CONN053_5,
				    I_I2SIN6_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH6", AFE_CONN053_5,
				    I_I2SIN6_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH2", AFE_CONN053_6,
				    I_SRC_1_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH2", AFE_CONN053_6,
				    I_SRC_2_OUT_CH2, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch7_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN054_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN054_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN054_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN054_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH5", AFE_CONN054_5,
				    I_I2SIN6_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH7", AFE_CONN054_5,
				    I_I2SIN6_CH7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH1", AFE_CONN054_6,
				    I_SRC_2_OUT_CH1, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch8_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN055_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN055_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN055_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN055_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH6", AFE_CONN055_5,
				    I_I2SIN6_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH8", AFE_CONN055_5,
				    I_I2SIN6_CH8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH2", AFE_CONN055_6,
				    I_SRC_2_OUT_CH2, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch9_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN056_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN056_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN056_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN056_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH7", AFE_CONN056_5,
				    I_I2SIN6_CH7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH9", AFE_CONN056_5,
				    I_I2SIN6_CH9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_3_OUT_CH1", AFE_CONN056_6,
				    I_SRC_3_OUT_CH1, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch10_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN057_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN057_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN057_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN057_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH8", AFE_CONN057_5,
				    I_I2SIN6_CH8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH10", AFE_CONN057_5,
				    I_I2SIN6_CH10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_3_OUT_CH2", AFE_CONN057_6,
				    I_SRC_3_OUT_CH2, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch11_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN058_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN058_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN058_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN058_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH11", AFE_CONN058_5,
				    I_I2SIN6_CH11, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch12_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN059_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN059_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN059_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN059_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH12", AFE_CONN059_5,
				    I_I2SIN6_CH12, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch13_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN060_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN060_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN060_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN060_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH13", AFE_CONN060_5,
				    I_I2SIN6_CH13, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch14_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN061_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN061_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN061_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN061_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH14", AFE_CONN061_5,
				    I_I2SIN6_CH14, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch15_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN062_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN062_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN062_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN062_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH15", AFE_CONN062_5,
				    I_I2SIN6_CH15, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm1_ch16_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN063_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN063_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN063_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN063_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH16", AFE_CONN063_5,
				    I_I2SIN6_CH16, 1, 0),
};

/*cm2*/
static const struct snd_kcontrol_new memif_ul_cm2_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN064_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN064_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN064_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN064_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN064_0, I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN064_0, I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH1", AFE_CONN064_6,
				    I_SRC_0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH1", AFE_CONN064_5, I_I2SIN6_CH1, 1, 0),
	// SOC_DAPM_SINGLE_AUTODISABLE("I2SIN5_CH1", AFE_CONN064_4, I_I2SIN5_CH1, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN065_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN065_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN065_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN065_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN065_0, I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN065_0, I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH2", AFE_CONN065_6,
				    I_SRC_0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH2", AFE_CONN065_5, I_I2SIN6_CH2, 1, 0),
	// SOC_DAPM_SINGLE_AUTODISABLE("I2SIN5_CH2", AFE_CONN065_4, I_I2SIN5_CH2, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch3_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN066_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN066_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN066_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN066_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN066_0, I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN066_0, I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH3", AFE_CONN066_5, I_I2SIN6_CH3, 1, 0),
	// SOC_DAPM_SINGLE_AUTODISABLE("I2SIN5_CH3", AFE_CONN066_4, I_I2SIN5_CH3, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch4_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN067_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN067_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN067_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN067_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN067_0, I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN067_0, I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH4", AFE_CONN067_5, I_I2SIN6_CH4, 1, 0),
	// SOC_DAPM_SINGLE_AUTODISABLE("I2SIN5_CH4", AFE_CONN067_4, I_I2SIN5_CH4, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch5_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN068_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN068_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN068_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN068_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN068_0, I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN068_0, I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH5", AFE_CONN068_5, I_I2SIN6_CH5, 1, 0),
	// SOC_DAPM_SINGLE_AUTODISABLE("I2SIN5_CH5", AFE_CONN068_4, I_I2SIN5_CH5, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch6_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN069_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN069_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN069_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN069_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN069_0, I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN069_0, I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH6", AFE_CONN069_5, I_I2SIN6_CH6, 1, 0),
	// SOC_DAPM_SINGLE_AUTODISABLE("I2SIN5_CH6", AFE_CONN069_4, I_I2SIN5_CH6, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch7_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN070_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN070_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN070_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN070_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH7", AFE_CONN070_5, I_I2SIN6_CH7, 1, 0),
	// SOC_DAPM_SINGLE_AUTODISABLE("I2SIN5_CH7", AFE_CONN070_4, I_I2SIN5_CH7, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch8_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN071_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN071_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN071_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN071_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH8", AFE_CONN071_5, I_I2SIN6_CH8, 1, 0),
	// SOC_DAPM_SINGLE_AUTODISABLE("I2SIN5_CH8", AFE_CONN071_4, I_I2SIN5_CH8, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch9_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN072_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN072_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN072_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN072_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH9", AFE_CONN072_5, I_I2SIN6_CH9, 1, 0),
	// SOC_DAPM_SINGLE_AUTODISABLE("I2SIN5_CH9", AFE_CONN072_4, I_I2SIN5_CH9, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch10_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN073_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN073_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN073_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN073_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH10", AFE_CONN073_5, I_I2SIN6_CH10, 1, 0),
	// SOC_DAPM_SINGLE_AUTODISABLE("I2SIN5_CH10", AFE_CONN073_4, I_I2SIN5_CH10, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch11_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN074_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN074_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN074_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN074_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH11", AFE_CONN074_5, I_I2SIN6_CH11, 1, 0),
	// SOC_DAPM_SINGLE_AUTODISABLE("I2SIN5_CH11", AFE_CONN074_5, I_I2SIN5_CH11, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch12_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN075_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN075_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN075_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN075_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH12", AFE_CONN075_5, I_I2SIN6_CH12, 1, 0),
	// SOC_DAPM_SINGLE_AUTODISABLE("I2SIN5_CH12", AFE_CONN075_5, I_I2SIN5_CH12, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch13_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN076_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN076_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN076_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN076_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH13", AFE_CONN076_5, I_I2SIN6_CH13, 1, 0),
	// SOC_DAPM_SINGLE_AUTODISABLE("I2SIN5_CH13", AFE_CONN076_5, I_I2SIN5_CH13, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch14_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN077_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN077_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN077_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN077_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH14", AFE_CONN077_5, I_I2SIN6_CH14, 1, 0),
	// SOC_DAPM_SINGLE_AUTODISABLE("I2SIN5_CH14", AFE_CONN077_5, I_I2SIN5_CH14, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch15_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN078_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN078_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN078_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN078_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH15", AFE_CONN078_5, I_I2SIN6_CH15, 1, 0),
	// SOC_DAPM_SINGLE_AUTODISABLE("I2SIN5_CH15", AFE_CONN078_5, I_I2SIN5_CH15, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch16_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN079_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN079_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN079_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN079_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH16", AFE_CONN079_5, I_I2SIN6_CH16, 1, 0),
	// SOC_DAPM_SINGLE_AUTODISABLE("I2SIN5_CH16", AFE_CONN079_5, I_I2SIN5_CH16, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch17_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN080_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN080_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN080_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN080_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH17", AFE_CONN080_5, I_I2SIN6_CH17, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch18_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN081_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN081_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN081_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN081_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH18", AFE_CONN081_5, I_I2SIN6_CH18, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch19_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN082_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN082_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN082_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN082_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH19", AFE_CONN082_5, I_I2SIN6_CH19, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch20_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN083_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN083_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN083_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN083_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH20", AFE_CONN083_5, I_I2SIN6_CH20, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch21_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN084_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN084_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN084_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN084_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH21", AFE_CONN084_5, I_I2SIN6_CH21, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch22_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN085_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN085_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN085_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN085_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH22", AFE_CONN085_5, I_I2SIN6_CH22, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch23_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN086_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN086_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN086_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN086_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH23", AFE_CONN086_5, I_I2SIN6_CH23, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch24_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN087_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN087_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN087_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN087_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH24", AFE_CONN087_5, I_I2SIN6_CH24, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch25_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN088_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN088_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN088_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN088_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH25", AFE_CONN088_5, I_I2SIN6_CH25, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch26_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN089_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN089_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN089_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN089_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH26", AFE_CONN089_5, I_I2SIN6_CH26, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch27_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN090_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN090_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN090_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN090_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH27", AFE_CONN090_6, I_I2SIN6_CH27, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch28_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN091_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN091_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN091_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN091_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH28", AFE_CONN091_6, I_I2SIN6_CH28, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch29_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN092_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN092_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN092_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN092_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH29", AFE_CONN092_6, I_I2SIN6_CH29, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch30_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN093_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN093_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN093_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN093_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH30", AFE_CONN093_6, I_I2SIN6_CH30, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch31_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN094_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN094_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN094_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN094_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH31", AFE_CONN094_6, I_I2SIN6_CH31, 1, 0),
};
static const struct snd_kcontrol_new memif_ul_cm2_ch32_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN095_0, I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN095_0, I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN095_0, I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN095_0, I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH32", AFE_CONN095_6, I_I2SIN6_CH32, 1, 0),
};

static const char * const cm0_mux_map[] = {
	"CM0_2CH_PATH",
	"CM0_8CH_PATH",
};
static const char * const cm1_mux_map[] = {
	"CM1_2CH_PATH",
	"CM1_16CH_PATH",
};
static const char * const cm2_mux_map[] = {
	"CM2_2CH_PATH",
	"CM2_32CH_PATH",
};

static int cm_mux_get(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(widget->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);

	if (!strcmp(widget->name, "CM2_UL_MUX")) {
		ucontrol->value.integer.value[0] = mt6881_get_cm_mux(CM2);
	} else if (!strcmp(widget->name, "CM1_UL_MUX")) {
		ucontrol->value.integer.value[0] = mt6881_get_cm_mux(CM1);
	} else if (!strcmp(widget->name, "CM0_UL_MUX")) {
		ucontrol->value.integer.value[0] = mt6881_get_cm_mux(CM0);
	} else {
		dev_info(afe->dev, "%s(), No match: %s\n", __func__, widget->name);
		ucontrol->value.integer.value[0] = 0;
	}

	return 0;
}

static int cm_mux_set(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(widget->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);

	dev_info(afe->dev, "%s(), %s: fd %ld\n",
			 __func__, widget->name, ucontrol->value.integer.value[0]);

	if (!strcmp(widget->name, "CM2_UL_MUX"))
		mt6881_set_cm_mux(CM2, ucontrol->value.integer.value[0]);
	else if (!strcmp(widget->name, "CM1_UL_MUX"))
		mt6881_set_cm_mux(CM1, ucontrol->value.integer.value[0]);
	else if (!strcmp(widget->name, "CM0_UL_MUX"))
		mt6881_set_cm_mux(CM0, ucontrol->value.integer.value[0]);
	else
		dev_info(afe->dev, "%s(), No match: %s\n", __func__, widget->name);

	return snd_soc_dapm_put_enum_double(kcontrol, ucontrol);
}

static SOC_ENUM_SINGLE_DECL(ul_cm0_mux_map_enum,
			    SND_SOC_NOPM,
			    0,
			    cm0_mux_map);
static SOC_ENUM_SINGLE_DECL(ul_cm1_mux_map_enum,
			    SND_SOC_NOPM,
			    0,
			    cm1_mux_map);
static SOC_ENUM_SINGLE_DECL(ul_cm2_mux_map_enum,
			    SND_SOC_NOPM,
			    0,
			    cm2_mux_map);

static const struct snd_kcontrol_new ul_cm0_mux_control =
	SOC_DAPM_ENUM_EXT("CM0_UL_MUX Select", ul_cm0_mux_map_enum, cm_mux_get, cm_mux_set);
static const struct snd_kcontrol_new ul_cm1_mux_control =
	SOC_DAPM_ENUM_EXT("CM1_UL_MUX Select", ul_cm1_mux_map_enum, cm_mux_get, cm_mux_set);
static const struct snd_kcontrol_new ul_cm2_mux_control =
	SOC_DAPM_ENUM_EXT("CM2_UL_MUX Select", ul_cm2_mux_map_enum, cm_mux_get, cm_mux_set);

static const struct snd_soc_dapm_widget mt6881_memif_widgets[] = {
	/* inter-connections */
	SND_SOC_DAPM_MIXER("UL0_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul0_ch1_mix, ARRAY_SIZE(memif_ul0_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL0_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul0_ch2_mix, ARRAY_SIZE(memif_ul0_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL1_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul1_ch1_mix, ARRAY_SIZE(memif_ul1_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL1_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul1_ch2_mix, ARRAY_SIZE(memif_ul1_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL2_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul2_ch1_mix, ARRAY_SIZE(memif_ul2_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL2_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul2_ch2_mix, ARRAY_SIZE(memif_ul2_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL3_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul3_ch1_mix, ARRAY_SIZE(memif_ul3_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL3_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul3_ch2_mix, ARRAY_SIZE(memif_ul3_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL4_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul4_ch1_mix, ARRAY_SIZE(memif_ul4_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL4_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul4_ch2_mix, ARRAY_SIZE(memif_ul4_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL5_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul5_ch1_mix, ARRAY_SIZE(memif_ul5_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL5_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul5_ch2_mix, ARRAY_SIZE(memif_ul5_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL6_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul6_ch1_mix, ARRAY_SIZE(memif_ul6_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL6_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul6_ch2_mix, ARRAY_SIZE(memif_ul6_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL7_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul7_ch1_mix, ARRAY_SIZE(memif_ul7_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL7_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul7_ch2_mix, ARRAY_SIZE(memif_ul7_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL8_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul8_ch1_mix, ARRAY_SIZE(memif_ul8_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL8_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul8_ch2_mix, ARRAY_SIZE(memif_ul8_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL9_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul9_ch1_mix, ARRAY_SIZE(memif_ul9_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL9_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul9_ch2_mix, ARRAY_SIZE(memif_ul9_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL10_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul10_ch1_mix, ARRAY_SIZE(memif_ul10_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL10_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul10_ch2_mix, ARRAY_SIZE(memif_ul10_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL24_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul24_ch1_mix, ARRAY_SIZE(memif_ul24_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL24_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul24_ch2_mix, ARRAY_SIZE(memif_ul24_ch2_mix)),

	SND_SOC_DAPM_MIXER("DSP_DL", SND_SOC_NOPM, 0, 0,
			   mtk_dsp_dl_playback_mix,
			   ARRAY_SIZE(mtk_dsp_dl_playback_mix)),

	SND_SOC_DAPM_MIXER("UL_CM0_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch1_mix, ARRAY_SIZE(memif_ul_cm0_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL_CM0_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch2_mix, ARRAY_SIZE(memif_ul_cm0_ch2_mix)),
	SND_SOC_DAPM_MIXER("UL_CM0_CH3", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch3_mix, ARRAY_SIZE(memif_ul_cm0_ch3_mix)),
	SND_SOC_DAPM_MIXER("UL_CM0_CH4", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch4_mix, ARRAY_SIZE(memif_ul_cm0_ch4_mix)),
	SND_SOC_DAPM_MIXER("UL_CM0_CH5", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch5_mix, ARRAY_SIZE(memif_ul_cm0_ch5_mix)),
	SND_SOC_DAPM_MIXER("UL_CM0_CH6", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch6_mix, ARRAY_SIZE(memif_ul_cm0_ch6_mix)),
	SND_SOC_DAPM_MIXER("UL_CM0_CH7", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch7_mix, ARRAY_SIZE(memif_ul_cm0_ch7_mix)),
	SND_SOC_DAPM_MIXER("UL_CM0_CH8", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm0_ch8_mix, ARRAY_SIZE(memif_ul_cm0_ch8_mix)),
	SND_SOC_DAPM_MUX("CM0_UL_MUX", SND_SOC_NOPM, 0, 0,
			   &ul_cm0_mux_control),

	SND_SOC_DAPM_MIXER("UL_CM1_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch1_mix, ARRAY_SIZE(memif_ul_cm1_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch2_mix, ARRAY_SIZE(memif_ul_cm1_ch2_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH3", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch3_mix, ARRAY_SIZE(memif_ul_cm1_ch3_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH4", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch4_mix, ARRAY_SIZE(memif_ul_cm1_ch4_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH5", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch5_mix, ARRAY_SIZE(memif_ul_cm1_ch5_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH6", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch6_mix, ARRAY_SIZE(memif_ul_cm1_ch6_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH7", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch7_mix, ARRAY_SIZE(memif_ul_cm1_ch7_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH8", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch8_mix, ARRAY_SIZE(memif_ul_cm1_ch8_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH9", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch9_mix, ARRAY_SIZE(memif_ul_cm1_ch9_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH10", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch10_mix, ARRAY_SIZE(memif_ul_cm1_ch10_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH11", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch11_mix, ARRAY_SIZE(memif_ul_cm1_ch11_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH12", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch12_mix, ARRAY_SIZE(memif_ul_cm1_ch12_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH13", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch13_mix, ARRAY_SIZE(memif_ul_cm1_ch13_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH14", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch14_mix, ARRAY_SIZE(memif_ul_cm1_ch14_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH15", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch15_mix, ARRAY_SIZE(memif_ul_cm1_ch15_mix)),
	SND_SOC_DAPM_MIXER("UL_CM1_CH16", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm1_ch16_mix, ARRAY_SIZE(memif_ul_cm1_ch16_mix)),
	SND_SOC_DAPM_MUX("CM1_UL_MUX", SND_SOC_NOPM, 0, 0,
			   &ul_cm1_mux_control),

	SND_SOC_DAPM_MIXER("UL_CM2_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch1_mix, ARRAY_SIZE(memif_ul_cm2_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch2_mix, ARRAY_SIZE(memif_ul_cm2_ch2_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH3", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch3_mix, ARRAY_SIZE(memif_ul_cm2_ch3_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH4", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch4_mix, ARRAY_SIZE(memif_ul_cm2_ch4_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH5", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch5_mix, ARRAY_SIZE(memif_ul_cm2_ch5_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH6", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch6_mix, ARRAY_SIZE(memif_ul_cm2_ch6_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH7", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch7_mix, ARRAY_SIZE(memif_ul_cm2_ch7_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH8", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch8_mix, ARRAY_SIZE(memif_ul_cm2_ch8_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH9", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch9_mix, ARRAY_SIZE(memif_ul_cm2_ch9_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH10", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch10_mix, ARRAY_SIZE(memif_ul_cm2_ch10_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH11", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch11_mix, ARRAY_SIZE(memif_ul_cm2_ch11_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH12", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch12_mix, ARRAY_SIZE(memif_ul_cm2_ch12_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH13", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch13_mix, ARRAY_SIZE(memif_ul_cm2_ch13_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH14", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch14_mix, ARRAY_SIZE(memif_ul_cm2_ch14_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH15", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch15_mix, ARRAY_SIZE(memif_ul_cm2_ch15_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH16", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch16_mix, ARRAY_SIZE(memif_ul_cm2_ch16_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH17", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch17_mix, ARRAY_SIZE(memif_ul_cm2_ch17_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH18", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch18_mix, ARRAY_SIZE(memif_ul_cm2_ch18_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH19", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch19_mix, ARRAY_SIZE(memif_ul_cm2_ch19_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH20", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch20_mix, ARRAY_SIZE(memif_ul_cm2_ch20_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH21", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch21_mix, ARRAY_SIZE(memif_ul_cm2_ch21_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH22", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch22_mix, ARRAY_SIZE(memif_ul_cm2_ch22_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH23", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch23_mix, ARRAY_SIZE(memif_ul_cm2_ch23_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH24", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch24_mix, ARRAY_SIZE(memif_ul_cm2_ch24_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH25", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch25_mix, ARRAY_SIZE(memif_ul_cm2_ch25_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH26", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch26_mix, ARRAY_SIZE(memif_ul_cm2_ch26_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH27", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch27_mix, ARRAY_SIZE(memif_ul_cm2_ch27_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH28", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch28_mix, ARRAY_SIZE(memif_ul_cm2_ch28_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH29", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch29_mix, ARRAY_SIZE(memif_ul_cm2_ch29_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH30", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch30_mix, ARRAY_SIZE(memif_ul_cm2_ch30_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH31", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch31_mix, ARRAY_SIZE(memif_ul_cm2_ch31_mix)),
	SND_SOC_DAPM_MIXER("UL_CM2_CH32", SND_SOC_NOPM, 0, 0,
			   memif_ul_cm2_ch32_mix, ARRAY_SIZE(memif_ul_cm2_ch32_mix)),
	SND_SOC_DAPM_MUX("CM2_UL_MUX", SND_SOC_NOPM, 0, 0,
			  &ul_cm2_mux_control),

	SND_SOC_DAPM_SUPPLY("CM0_Enable",
			AFE_CM0_CON0, AFE_CM0_ON_SFT, 0,
			ul_cm0_event,
			SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_PRE_PMD),
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO)
	SND_SOC_DAPM_SUPPLY("CM1_Enable",
			AFE_CM1_CON0, AFE_CM1_ON_SFT, 0,
			ul_cm1_event,
			SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY("CM2_Enable",
			AFE_CM2_CON0, AFE_CM2_ON_SFT, 0,
			ul_cm2_event,
			SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_PRE_PMD),
#else
	SND_SOC_DAPM_SUPPLY("CM1_Enable",
			SND_SOC_NOPM, 0, 0,
			ul_cm1_event,
			SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY("CM2_Enable",
			SND_SOC_NOPM, 0, 0,
			ul_cm2_event,
			SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_PRE_PMD),
#endif

	SND_SOC_DAPM_INPUT("UL1_VIRTUAL_INPUT"),
	SND_SOC_DAPM_INPUT("UL2_VIRTUAL_INPUT"),
	SND_SOC_DAPM_INPUT("UL4_VIRTUAL_INPUT"),
	SND_SOC_DAPM_INPUT("UL24_VIRTUAL_INPUT"),
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
	SND_SOC_DAPM_INPUT("UL5_VIRTUAL_INPUT"),
	SND_SOC_DAPM_INPUT("UL6_VIRTUAL_INPUT"),
#endif

	SND_SOC_DAPM_OUTPUT("DL_TO_DSP"),
	SND_SOC_DAPM_OUTPUT("DL6_VIRTUAL_OUTPUT"),
};

static const struct snd_soc_dapm_route mt6881_memif_routes[] = {
	{"UL0", NULL, "UL0_CH1"},
	{"UL0", NULL, "UL0_CH2"},
	/* Normal record */
	{"UL0_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL0_CH1", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL0_CH1", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL0_CH1", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL0_CH2", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL0_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL0_CH2", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL0_CH2", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	/* FM */
	{"UL0_CH1", "CONNSYS_I2S_CH1", "Connsys I2S"},
	{"UL0_CH2", "CONNSYS_I2S_CH2", "Connsys I2S"},

	{"UL0_CH1", "I2SIN0_CH1", "I2SIN0"},
	{"UL0_CH2", "I2SIN0_CH2", "I2SIN0"},
	{"UL0_CH1", "I2SIN1_CH1", "I2SIN1"},
	{"UL0_CH2", "I2SIN1_CH2", "I2SIN1"},
	{"UL0_CH1", "I2SIN2_CH1", "I2SIN2"},
	{"UL0_CH2", "I2SIN2_CH2", "I2SIN2"},

	{"UL0_CH1", "PCM_0_CAP_CH1", "PCM 0 Capture"},
	{"UL0_CH2", "PCM_0_CAP_CH1", "PCM 0 Capture"},
	{"UL0_CH1", "PCM_1_CAP_CH1", "PCM 1 Capture"},
	{"UL0_CH2", "PCM_1_CAP_CH1", "PCM 1 Capture"},


	{"UL0_CH1", "HW_SRC_0_OUT_CH1", "HW_SRC_0_Out"},
	{"UL0_CH2", "HW_SRC_0_OUT_CH2", "HW_SRC_0_Out"},
	{"UL0_CH1", "HW_SRC_2_OUT_CH1", "HW_SRC_2_Out"},
	{"UL0_CH2", "HW_SRC_2_OUT_CH2", "HW_SRC_2_Out"},

	{"UL1", NULL, "UL1_CH1"},
	{"UL1", NULL, "UL1_CH2"},
	/* cannot connect FE to FE directly */
	{"UL1_CH1", "DL0_CH1", "Hostless_UL1 UL"},
	{"UL1_CH2", "DL0_CH2", "Hostless_UL1 UL"},
	{"UL1_CH1", "DL1_CH1", "Hostless_UL1 UL"},
	{"UL1_CH2", "DL1_CH2", "Hostless_UL1 UL"},
	{"UL1_CH1", "DL6_CH1", "Hostless_UL1 UL"},
	{"UL1_CH2", "DL6_CH2", "Hostless_UL1 UL"},
	{"UL1_CH1", "DL2_CH1", "Hostless_UL1 UL"},
	{"UL1_CH2", "DL2_CH2", "Hostless_UL1 UL"},
	{"UL1_CH1", "DL3_CH1", "Hostless_UL1 UL"},
	{"UL1_CH2", "DL3_CH2", "Hostless_UL1 UL"},
	{"UL1_CH1", "DL4_CH1", "Hostless_UL1 UL"},
	{"UL1_CH2", "DL4_CH2", "Hostless_UL1 UL"},
	{"UL1_CH1", "DL7_CH1", "Hostless_UL1 UL"},
	{"UL1_CH2", "DL7_CH2", "Hostless_UL1 UL"},
	{"UL1_CH1", "DL23_CH1", "Hostless_UL1 UL"},
	{"UL1_CH2", "DL23_CH2", "Hostless_UL1 UL"},
	{"UL1_CH1", "DL24_CH1", "Hostless_UL1 UL"},
	{"UL1_CH2", "DL24_CH2", "Hostless_UL1 UL"},
	{"UL1_CH1", "DL_24CH_CH1", "Hostless_UL1 UL"},
	{"UL1_CH2", "DL_24CH_CH2", "Hostless_UL1 UL"},
	{"UL1_CH1", "DL_48CH_CH1", "Hostless_UL1 UL"},
	{"UL1_CH2", "DL_48CH_CH2", "Hostless_UL1 UL"},

	{"Hostless_UL1 UL", NULL, "UL1_VIRTUAL_INPUT"},
	{"UL1_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL1_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},

	{"UL1_CH1", "I2SIN0_CH1", "I2SIN0"},
	{"UL1_CH2", "I2SIN0_CH2", "I2SIN0"},
	{"UL1_CH1", "I2SIN1_CH1", "I2SIN1"},
	{"UL1_CH2", "I2SIN1_CH2", "I2SIN1"},
	{"UL1_CH1", "I2SIN2_CH1", "I2SIN2"},
	{"UL1_CH2", "I2SIN2_CH2", "I2SIN2"},
	{"UL1_CH1", "I2SIN6_CH1", "I2SIN6"},
	{"UL1_CH2", "I2SIN6_CH2", "I2SIN6"},

	{"UL1_CH1", "PCM_0_CAP_CH1", "PCM 0 Capture"},
	{"UL1_CH2", "PCM_0_CAP_CH1", "PCM 0 Capture"},
	{"UL1_CH1", "PCM_1_CAP_CH1", "PCM 1 Capture"},
	{"UL1_CH2", "PCM_1_CAP_CH1", "PCM 1 Capture"},

	{"UL1_CH1", "CONNSYS_I2S_CH1", "Connsys I2S"},
	{"UL1_CH2", "CONNSYS_I2S_CH2", "Connsys I2S"},

	{"UL1_CH1", "HW_SRC_0_OUT_CH1", "HW_SRC_0_Out"},
	{"UL1_CH2", "HW_SRC_0_OUT_CH2", "HW_SRC_0_Out"},
	{"UL1_CH1", "HW_SRC_2_OUT_CH1", "HW_SRC_2_Out"},
	{"UL1_CH2", "HW_SRC_2_OUT_CH2", "HW_SRC_2_Out"},

	{"UL2", NULL, "UL2_CH1"},
	{"UL2", NULL, "UL2_CH2"},
	{"UL2_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL2_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL2_CH1", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL2_CH2", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL2_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL2_CH2", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL2_CH2", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL2_CH1", "HW_GAIN1_OUT_CH1", "HW Gain 1 Out"},
	{"UL2_CH2", "HW_GAIN1_OUT_CH2", "HW Gain 1 Out"},
	{"UL2_CH1", "HW_SRC_1_OUT_CH1", "HW_SRC_1_Out"},
	{"UL2_CH2", "HW_SRC_1_OUT_CH2", "HW_SRC_1_Out"},
	{"UL2_CH1", "I2SIN6_CH1", "I2SIN6"},
	{"UL2_CH2", "I2SIN6_CH2", "I2SIN6"},

	{"UL3", NULL, "UL3_CH1"},
	{"UL3", NULL, "UL3_CH2"},

	{"UL3_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL3_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL3_CH1", "I2SIN0_CH1", "I2SIN0"},
	{"UL3_CH2", "I2SIN0_CH2", "I2SIN0"},
	{"UL3_CH1", "I2SIN1_CH1", "I2SIN1"},
	{"UL3_CH2", "I2SIN1_CH2", "I2SIN1"},
	{"UL3_CH1", "I2SIN6_CH1", "I2SIN6"},
	{"UL3_CH2", "I2SIN6_CH2", "I2SIN6"},

	{"UL4", NULL, "UL4_CH1"},
	{"UL4", NULL, "UL4_CH2"},
	{"UL4_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL4_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL4_CH1", "I2SIN0_CH1", "I2SIN0"},
	{"UL4_CH2", "I2SIN0_CH2", "I2SIN0"},
	{"UL4_CH1", "I2SIN1_CH1", "I2SIN1"},
	{"UL4_CH2", "I2SIN1_CH2", "I2SIN1"},
	{"UL4_CH1", "I2SIN6_CH1", "I2SIN6"},
	{"UL4_CH2", "I2SIN6_CH2", "I2SIN6"},

	{"UL4_CH1", "DL0_CH1", "Hostless_UL4 UL"},
	{"UL4_CH2", "DL0_CH2", "Hostless_UL4 UL"},
	{"UL4_CH1", "DL2_CH1", "Hostless_UL4 UL"},
	{"UL4_CH2", "DL2_CH2", "Hostless_UL4 UL"},
	{"UL4_CH1", "DL1_CH1", "Hostless_UL4 UL"},
	{"UL4_CH2", "DL1_CH2", "Hostless_UL4 UL"},
	{"UL4_CH1", "DL6_CH1", "Hostless_UL4 UL"},
	{"UL4_CH2", "DL6_CH2", "Hostless_UL4 UL"},
	{"UL4_CH1", "DL3_CH1", "Hostless_UL4 UL"},
	{"UL4_CH2", "DL3_CH2", "Hostless_UL4 UL"},
	{"UL4_CH1", "DL_24CH_CH1", "Hostless_UL4 UL"},
	{"UL4_CH2", "DL_24CH_CH2", "Hostless_UL4 UL"},
	{"UL4_CH1", "DL_48CH_CH1", "Hostless_UL4 UL"},
	{"UL4_CH2", "DL_48CH_CH2", "Hostless_UL4 UL"},
	{"Hostless_UL4 UL", NULL, "UL4_VIRTUAL_INPUT"},

	{"UL4_CH1", "PCM_0_CAP_CH1", "PCM 0 Capture"},
	{"UL4_CH2", "PCM_0_CAP_CH1", "PCM 0 Capture"},
	{"UL4_CH1", "PCM_1_CAP_CH1", "PCM 1 Capture"},
	{"UL4_CH2", "PCM_1_CAP_CH1", "PCM 1 Capture"},
	{"UL4_CH1", "HW_GAIN0_OUT_CH1", "HW Gain 0 Out"},
	{"UL4_CH2", "HW_GAIN0_OUT_CH2", "HW Gain 0 Out"},

	{"UL5", NULL, "UL5_CH1"},
	{"UL5", NULL, "UL5_CH2"},

	{"UL5_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL5_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL5_CH1", "PCM_0_CAP_CH1", "PCM 0 Capture"},
	{"UL5_CH2", "PCM_0_CAP_CH1", "PCM 0 Capture"},
	{"UL5_CH1", "PCM_1_CAP_CH1", "PCM 1 Capture"},
	{"UL5_CH2", "PCM_1_CAP_CH1", "PCM 1 Capture"},
	{"UL5_CH1", "GAIN0_OUT_CH1", "HW Gain 0 Out"},
	{"UL5_CH2", "GAIN0_OUT_CH2", "HW Gain 0 Out"},
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
	{"UL5_CH1", "DL5_CH1", "Hostless_UL5 UL"},
	{"UL5_CH2", "DL5_CH2", "Hostless_UL5 UL"},
	{"Hostless_UL5 UL", NULL, "UL5_VIRTUAL_INPUT"},
#endif
	{"UL5_CH1", "CONNSYS_I2S_CH1", "Connsys I2S"},
	{"UL5_CH2", "CONNSYS_I2S_CH2", "Connsys I2S"},

	{"UL6", NULL, "UL6_CH1"},
	{"UL6", NULL, "UL6_CH2"},
	{"UL6_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL6_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL6_CH1", "CONNSYS_I2S_CH1", "Connsys I2S"},
	{"UL6_CH2", "CONNSYS_I2S_CH2", "Connsys I2S"},
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
	{"UL6_CH1", "DL6_CH1", "Hostless_UL6 UL"},
	{"UL6_CH2", "DL6_CH2", "Hostless_UL6 UL"},
	{"Hostless_UL6 UL", NULL, "UL6_VIRTUAL_INPUT"},
#endif
	{"UL6_CH1", "I2SIN2_CH1", "I2SIN2"},
	{"UL6_CH2", "I2SIN2_CH2", "I2SIN2"},

	{"UL7", NULL, "UL7_CH1"},
	{"UL7", NULL, "UL7_CH2"},
	{"UL7_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL7_CH1", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL7_CH1", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL7_CH1", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL7_CH2", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL7_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL7_CH2", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL7_CH2", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},

	{"UL7_CH1", "CONNSYS_I2S_CH1", "Connsys I2S"},
	{"UL7_CH2", "CONNSYS_I2S_CH2", "Connsys I2S"},
	{"UL7_CH1", "I2SIN2_CH1", "I2SIN2"},
	{"UL7_CH2", "I2SIN2_CH2", "I2SIN2"},
	{"UL7_CH1", "I2SIN6_CH1", "I2SIN6"},
	{"UL7_CH2", "I2SIN6_CH2", "I2SIN6"},
	{"UL7_CH1", "I2SIN6_CH3", "I2SIN6"},
	{"UL7_CH2", "I2SIN6_CH4", "I2SIN6"},
	{"UL7_CH1", "HW_GAIN0_OUT_CH1", "HW Gain 0 Out"},
	{"UL7_CH2", "HW_GAIN0_OUT_CH2", "HW Gain 0 Out"},

	{"UL8", NULL, "CM0_UL_MUX"},
	{"CM0_UL_MUX", "CM0_2CH_PATH", "UL8_CH1"},
	{"CM0_UL_MUX", "CM0_2CH_PATH", "UL8_CH2"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH1"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH2"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH3"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH4"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH5"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH6"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH7"},
	{"CM0_UL_MUX", "CM0_8CH_PATH", "UL_CM0_CH8"},

	{"UL_CM0_CH1", "I2SIN6_CH1", "I2SIN6"},
	{"UL_CM0_CH2", "I2SIN6_CH2", "I2SIN6"},
	{"UL_CM0_CH3", "I2SIN6_CH3", "I2SIN6"},
	{"UL_CM0_CH4", "I2SIN6_CH4", "I2SIN6"},
	{"UL_CM0_CH5", "I2SIN6_CH5", "I2SIN6"},
	{"UL_CM0_CH6", "I2SIN6_CH6", "I2SIN6"},
	{"UL_CM0_CH7", "I2SIN6_CH7", "I2SIN6"},
	{"UL_CM0_CH8", "I2SIN6_CH8", "I2SIN6"},

	{"UL_CM0_CH1", NULL, "CM0_Enable"},
	{"UL_CM0_CH2", NULL, "CM0_Enable"},
	{"UL_CM0_CH3", NULL, "CM0_Enable"},
	{"UL_CM0_CH4", NULL, "CM0_Enable"},
	{"UL_CM0_CH5", NULL, "CM0_Enable"},
	{"UL_CM0_CH6", NULL, "CM0_Enable"},
	{"UL_CM0_CH7", NULL, "CM0_Enable"},
	{"UL_CM0_CH8", NULL, "CM0_Enable"},

	/* UL8 o34o35 <- ADDA */
	{"UL8_CH1", "PCM_0_CAP_CH1", "PCM 0 Capture"},
	{"UL8_CH1", "PCM_1_CAP_CH1", "PCM 1 Capture"},
	{"UL8_CH2", "PCM_0_CAP_CH2", "PCM 0 Capture"},
	{"UL8_CH2", "PCM_1_CAP_CH2", "PCM 1 Capture"},

	{"UL9", NULL, "CM1_UL_MUX"},
	{"CM1_UL_MUX", "CM1_2CH_PATH", "UL9_CH1"},
	{"CM1_UL_MUX", "CM1_2CH_PATH", "UL9_CH2"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH1"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH2"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH3"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH4"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH5"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH6"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH7"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH8"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH9"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH10"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH11"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH12"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH13"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH14"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH15"},
	{"CM1_UL_MUX", "CM1_16CH_PATH", "UL_CM1_CH16"},

	/* I2SIN6 CH1 ~ CH8 -> CM1 CH1 ~ CH8*/
	{"UL_CM1_CH1", "I2SIN6_CH1", "I2SIN6"},
	{"UL_CM1_CH2", "I2SIN6_CH2", "I2SIN6"},
	{"UL_CM1_CH3", "I2SIN6_CH3", "I2SIN6"},
	{"UL_CM1_CH4", "I2SIN6_CH4", "I2SIN6"},
	{"UL_CM1_CH5", "I2SIN6_CH5", "I2SIN6"},
	{"UL_CM1_CH6", "I2SIN6_CH6", "I2SIN6"},
	{"UL_CM1_CH7", "I2SIN6_CH7", "I2SIN6"},
	{"UL_CM1_CH8", "I2SIN6_CH8", "I2SIN6"},

	/* I2SIN6 CH3 ~ CH8 -> CM1 CH5 ~ CH10*/
	{"UL_CM1_CH5", "I2SIN6_CH3", "I2SIN6"},
	{"UL_CM1_CH6", "I2SIN6_CH4", "I2SIN6"},
	{"UL_CM1_CH7", "I2SIN6_CH5", "I2SIN6"},
	{"UL_CM1_CH8", "I2SIN6_CH6", "I2SIN6"},
	{"UL_CM1_CH9", "I2SIN6_CH7", "I2SIN6"},
	{"UL_CM1_CH10", "I2SIN6_CH8", "I2SIN6"},

	{"UL_CM1_CH1", NULL, "CM1_Enable"},
	{"UL_CM1_CH2", NULL, "CM1_Enable"},
	{"UL_CM1_CH3", NULL, "CM1_Enable"},
	{"UL_CM1_CH4", NULL, "CM1_Enable"},
	{"UL_CM1_CH5", NULL, "CM1_Enable"},
	{"UL_CM1_CH6", NULL, "CM1_Enable"},
	{"UL_CM1_CH7", NULL, "CM1_Enable"},
	{"UL_CM1_CH8", NULL, "CM1_Enable"},
	{"UL_CM1_CH9", NULL, "CM1_Enable"},
	{"UL_CM1_CH10", NULL, "CM1_Enable"},
	{"UL_CM1_CH11", NULL, "CM1_Enable"},
	{"UL_CM1_CH12", NULL, "CM1_Enable"},
	{"UL_CM1_CH13", NULL, "CM1_Enable"},
	{"UL_CM1_CH14", NULL, "CM1_Enable"},
	{"UL_CM1_CH15", NULL, "CM1_Enable"},
	{"UL_CM1_CH16", NULL, "CM1_Enable"},

	/* UL9 o36o37 <- ADDA */
	{"UL9_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL9_CH1", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL9_CH1", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL9_CH1", "I2SIN6_CH1", "I2SIN6"},
	{"UL9_CH2", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL9_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL9_CH2", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL9_CH2", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL9_CH2", "I2SIN6_CH2", "I2SIN6"},

	{"UL10", NULL, "CM2_UL_MUX"},
	{"CM2_UL_MUX", "CM2_2CH_PATH", "UL10_CH1"},
	{"CM2_UL_MUX", "CM2_2CH_PATH", "UL10_CH2"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH1"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH2"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH3"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH4"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH5"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH6"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH7"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH8"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH9"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH10"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH11"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH12"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH13"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH14"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH15"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH16"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH17"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH18"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH19"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH20"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH21"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH22"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH23"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH24"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH25"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH26"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH27"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH28"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH29"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH30"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH31"},
	{"CM2_UL_MUX", "CM2_32CH_PATH", "UL_CM2_CH32"},

	{"UL_CM2_CH1", "I2SIN6_CH1", "I2SIN6"},
	{"UL_CM2_CH2", "I2SIN6_CH2", "I2SIN6"},
	{"UL_CM2_CH3", "I2SIN6_CH3", "I2SIN6"},
	{"UL_CM2_CH4", "I2SIN6_CH4", "I2SIN6"},
	{"UL_CM2_CH5", "I2SIN6_CH5", "I2SIN6"},
	{"UL_CM2_CH6", "I2SIN6_CH6", "I2SIN6"},
	{"UL_CM2_CH7", "I2SIN6_CH7", "I2SIN6"},
	{"UL_CM2_CH8", "I2SIN6_CH8", "I2SIN6"},
	{"UL_CM2_CH9", "I2SIN6_CH9", "I2SIN6"},
	{"UL_CM2_CH10", "I2SIN6_CH10", "I2SIN6"},
	{"UL_CM2_CH11", "I2SIN6_CH11", "I2SIN6"},
	{"UL_CM2_CH12", "I2SIN6_CH12", "I2SIN6"},
	{"UL_CM2_CH13", "I2SIN6_CH13", "I2SIN6"},
	{"UL_CM2_CH14", "I2SIN6_CH14", "I2SIN6"},
	{"UL_CM2_CH15", "I2SIN6_CH15", "I2SIN6"},
	{"UL_CM2_CH16", "I2SIN6_CH16", "I2SIN6"},
	{"UL_CM2_CH17", "I2SIN6_CH17", "I2SIN6"},
	{"UL_CM2_CH18", "I2SIN6_CH18", "I2SIN6"},
	{"UL_CM2_CH19", "I2SIN6_CH19", "I2SIN6"},
	{"UL_CM2_CH20", "I2SIN6_CH20", "I2SIN6"},
	{"UL_CM2_CH21", "I2SIN6_CH21", "I2SIN6"},
	{"UL_CM2_CH22", "I2SIN6_CH22", "I2SIN6"},
	{"UL_CM2_CH23", "I2SIN6_CH23", "I2SIN6"},
	{"UL_CM2_CH24", "I2SIN6_CH24", "I2SIN6"},
	{"UL_CM2_CH25", "I2SIN6_CH25", "I2SIN6"},
	{"UL_CM2_CH26", "I2SIN6_CH26", "I2SIN6"},
	{"UL_CM2_CH27", "I2SIN6_CH27", "I2SIN6"},
	{"UL_CM2_CH28", "I2SIN6_CH28", "I2SIN6"},
	{"UL_CM2_CH29", "I2SIN6_CH29", "I2SIN6"},
	{"UL_CM2_CH30", "I2SIN6_CH30", "I2SIN6"},
	{"UL_CM2_CH31", "I2SIN6_CH31", "I2SIN6"},
	{"UL_CM2_CH32", "I2SIN6_CH32", "I2SIN6"},

	{"UL_CM2_CH1", NULL, "CM2_Enable"},
	{"UL_CM2_CH2", NULL, "CM2_Enable"},
	{"UL_CM2_CH3", NULL, "CM2_Enable"},
	{"UL_CM2_CH4", NULL, "CM2_Enable"},
	{"UL_CM2_CH5", NULL, "CM2_Enable"},
	{"UL_CM2_CH6", NULL, "CM2_Enable"},
	{"UL_CM2_CH7", NULL, "CM2_Enable"},
	{"UL_CM2_CH8", NULL, "CM2_Enable"},
	{"UL_CM2_CH9", NULL, "CM2_Enable"},
	{"UL_CM2_CH10", NULL, "CM2_Enable"},
	{"UL_CM2_CH11", NULL, "CM2_Enable"},
	{"UL_CM2_CH12", NULL, "CM2_Enable"},
	{"UL_CM2_CH13", NULL, "CM2_Enable"},
	{"UL_CM2_CH14", NULL, "CM2_Enable"},
	{"UL_CM2_CH15", NULL, "CM2_Enable"},
	{"UL_CM2_CH16", NULL, "CM2_Enable"},
	{"UL_CM2_CH17", NULL, "CM2_Enable"},
	{"UL_CM2_CH18", NULL, "CM2_Enable"},
	{"UL_CM2_CH19", NULL, "CM2_Enable"},
	{"UL_CM2_CH20", NULL, "CM2_Enable"},
	{"UL_CM2_CH21", NULL, "CM2_Enable"},
	{"UL_CM2_CH22", NULL, "CM2_Enable"},
	{"UL_CM2_CH23", NULL, "CM2_Enable"},
	{"UL_CM2_CH24", NULL, "CM2_Enable"},
	{"UL_CM2_CH25", NULL, "CM2_Enable"},
	{"UL_CM2_CH26", NULL, "CM2_Enable"},
	{"UL_CM2_CH27", NULL, "CM2_Enable"},
	{"UL_CM2_CH28", NULL, "CM2_Enable"},
	{"UL_CM2_CH29", NULL, "CM2_Enable"},
	{"UL_CM2_CH30", NULL, "CM2_Enable"},
	{"UL_CM2_CH31", NULL, "CM2_Enable"},
	{"UL_CM2_CH32", NULL, "CM2_Enable"},

	/* UL10 o38o39 <- ADDA */
	{"UL10_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL10_CH1", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL10_CH1", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL10_CH2", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL10_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL10_CH2", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL10_CH2", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
/*lindsay ul24,25 改 37,38?*/
	{"UL24", NULL, "UL24_CH1"},
	{"UL24", NULL, "UL24_CH2"},
	{"UL24_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL24_CH1", "I2SIN6_CH1", "I2SIN6"},
	{"UL24_CH2", "I2SIN6_CH2", "I2SIN6"},
	{"UL24_CH1", "DL0_CH1", "Hostless_UL24 UL"},
	{"UL24_CH2", "DL0_CH2", "Hostless_UL24 UL"},
	{"UL24_CH1", "DL1_CH1", "Hostless_UL24 UL"},
	{"UL24_CH2", "DL1_CH2", "Hostless_UL24 UL"},
	{"UL24_CH1", "DL6_CH1", "Hostless_UL24 UL"},
	{"UL24_CH2", "DL6_CH2", "Hostless_UL24 UL"},
	{"UL24_CH1", "DL2_CH1", "Hostless_UL24 UL"},
	{"UL24_CH2", "DL2_CH2", "Hostless_UL24 UL"},
	{"UL24_CH1", "DL3_CH1", "Hostless_UL24 UL"},
	{"UL24_CH2", "DL3_CH2", "Hostless_UL24 UL"},
	{"UL24_CH1", "DL4_CH1", "Hostless_UL24 UL"},
	{"UL24_CH2", "DL4_CH2", "Hostless_UL24 UL"},
	{"UL24_CH1", "DL7_CH1", "Hostless_UL24 UL"},
	{"UL24_CH2", "DL7_CH2", "Hostless_UL24 UL"},
	{"UL24_CH1", "DL23_CH1", "Hostless_UL24 UL"},
	{"UL24_CH2", "DL23_CH2", "Hostless_UL24 UL"},
	{"UL24_CH1", "DL24_CH1", "Hostless_UL24 UL"},
	{"UL24_CH2", "DL24_CH2", "Hostless_UL24 UL"},
	{"UL24_CH1", "DL_24CH_CH1", "Hostless_UL24 UL"},
	{"UL24_CH2", "DL_24CH_CH2", "Hostless_UL24 UL"},
	{"UL24_CH1", "DL_48CH_CH1", "Hostless_UL24 UL"},
	{"UL24_CH2", "DL_48CH_CH2", "Hostless_UL24 UL"},
	{"Hostless_UL24 UL", NULL, "UL24_VIRTUAL_INPUT"},

	// {"UL25", NULL, "UL25_CH1"},
	// {"UL25", NULL, "UL25_CH2"},
	// {"UL25_CH1", "I2SIN6_CH1", "I2SIN6"},
	// {"UL25_CH2", "I2SIN6_CH2", "I2SIN6"},
	// {"UL25_CH1", "I2SIN0_CH1", "I2SIN0"},
	// {"UL25_CH2", "I2SIN0_CH2", "I2SIN0"},

	// {"UL26", NULL, "UL26_CH1"},
	// {"UL26", NULL, "UL26_CH2"},
	// {"UL26_CH1", "I2SIN6_CH1", "I2SIN6"},
	// {"UL26_CH2", "I2SIN6_CH2", "I2SIN6"},
	// {"UL26_CH1", "I2SIN0_CH1", "I2SIN0"},
	// {"UL26_CH2", "I2SIN0_CH2", "I2SIN0"},

	{"DL_TO_DSP", NULL, "Hostless_DSP_DL DL"},
	{"Hostless_DSP_DL DL", NULL, "DSP_DL"},

	{"DSP_DL", "DSP_DL0", "DL0"},
	{"DSP_DL", "DSP_DL2", "DL2"},
	{"DSP_DL", "DSP_DL1", "DL1"},
	{"DSP_DL", "DSP_DL6", "DL6"},
	{"DSP_DL", "DSP_DL3", "DL3"},
	{"DSP_DL", "DSP_DL_24CH", "DL_24CH"},
	{"DSP_DL", "DSP_DL_48CH", "DL_48CH"},

	{"HW_GAIN1_IN_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"HW_GAIN1_IN_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},

	{"UL_CM0", NULL, "UL_CM0_CH1"},
	{"UL_CM0", NULL, "UL_CM0_CH2"},
	{"UL_CM0", NULL, "UL_CM0_CH3"},
	{"UL_CM0", NULL, "UL_CM0_CH4"},
	{"UL_CM0", NULL, "UL_CM0_CH5"},
	{"UL_CM0", NULL, "UL_CM0_CH6"},
	{"UL_CM0", NULL, "UL_CM0_CH7"},
	{"UL_CM0", NULL, "UL_CM0_CH8"},
	{"UL_CM0_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM0_CH1", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM0_CH1", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM0_CH1", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL_CM0_CH2", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM0_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM0_CH2", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM0_CH2", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL_CM0_CH3", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM0_CH3", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM0_CH3", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM0_CH3", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL_CM0_CH4", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM0_CH4", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM0_CH4", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM0_CH4", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL_CM0_CH1", "HW_GAIN1_OUT_CH1", "HW Gain 1 Out"},
	{"UL_CM0_CH2", "HW_GAIN1_OUT_CH2", "HW Gain 1 Out"},
	{"UL_CM0_CH1", "HW_SRC_0_OUT_CH1", "HW_SRC_0_Out"},
	{"UL_CM0_CH2", "HW_SRC_0_OUT_CH2", "HW_SRC_0_Out"},
	{"UL_CM0_CH1", "HW_SRC_1_OUT_CH1", "HW_SRC_1_Out"},
	{"UL_CM0_CH2", "HW_SRC_1_OUT_CH2", "HW_SRC_1_Out"},

	{"UL_CM1", NULL, "UL_CM1_CH1"},
	{"UL_CM1", NULL, "UL_CM1_CH2"},
	{"UL_CM1", NULL, "UL_CM1_CH3"},
	{"UL_CM1", NULL, "UL_CM1_CH4"},
	{"UL_CM1", NULL, "UL_CM1_CH5"},
	{"UL_CM1", NULL, "UL_CM1_CH6"},
	{"UL_CM1", NULL, "UL_CM1_CH7"},
	{"UL_CM1", NULL, "UL_CM1_CH8"},
	{"UL_CM1", NULL, "UL_CM1_CH9"},
	{"UL_CM1", NULL, "UL_CM1_CH10"},
	{"UL_CM1", NULL, "UL_CM1_CH11"},
	{"UL_CM1", NULL, "UL_CM1_CH12"},
	{"UL_CM1", NULL, "UL_CM1_CH13"},
	{"UL_CM1", NULL, "UL_CM1_CH14"},
	{"UL_CM1", NULL, "UL_CM1_CH15"},
	{"UL_CM1", NULL, "UL_CM1_CH16"},
	{"UL_CM1_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM1_CH1", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM1_CH1", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH1", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH2", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM1_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM1_CH2", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH2", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH3", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM1_CH3", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM1_CH3", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH3", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH4", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM1_CH4", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM1_CH4", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH4", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH5", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM1_CH5", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM1_CH5", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH5", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH6", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM1_CH6", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM1_CH6", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH6", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	{"UL_CM1_CH1", "HW_SRC_0_OUT_CH1", "HW_SRC_0_Out"},
	{"UL_CM1_CH2", "HW_SRC_0_OUT_CH2", "HW_SRC_0_Out"},
	{"UL_CM1_CH3", "HW_SRC_1_OUT_CH1", "HW_SRC_1_Out"},
	{"UL_CM1_CH4", "HW_SRC_1_OUT_CH2", "HW_SRC_1_Out"},
	{"UL_CM1_CH5", "HW_SRC_1_OUT_CH1", "HW_SRC_1_Out"},
	{"UL_CM1_CH6", "HW_SRC_1_OUT_CH2", "HW_SRC_1_Out"},
	{"UL_CM1_CH7", "HW_SRC_2_OUT_CH1", "HW_SRC_2_Out"},
	{"UL_CM1_CH8", "HW_SRC_2_OUT_CH2", "HW_SRC_2_Out"},
	{"UL_CM1_CH9", "HW_SRC_3_OUT_CH1", "HW_SRC_3_Out"},
	{"UL_CM1_CH10", "HW_SRC_3_OUT_CH2", "HW_SRC_3_Out"},
	{"UL_CM1_CH5", "HW_SRC_2_OUT_CH1", "HW_SRC_2_Out"},
	{"UL_CM1_CH6", "HW_SRC_2_OUT_CH2", "HW_SRC_2_Out"},

	{"UL_CM2", NULL, "UL_CM2_CH1"},
	{"UL_CM2", NULL, "UL_CM2_CH2"},
	{"UL_CM2", NULL, "UL_CM2_CH3"},
	{"UL_CM2", NULL, "UL_CM2_CH4"},
	{"UL_CM2", NULL, "UL_CM2_CH5"},
	{"UL_CM2", NULL, "UL_CM2_CH6"},
	{"UL_CM2", NULL, "UL_CM2_CH7"},
	{"UL_CM2", NULL, "UL_CM2_CH8"},
	{"UL_CM2", NULL, "UL_CM2_CH9"},
	{"UL_CM2", NULL, "UL_CM2_CH10"},
	{"UL_CM2", NULL, "UL_CM2_CH11"},
	{"UL_CM2", NULL, "UL_CM2_CH12"},
	{"UL_CM2", NULL, "UL_CM2_CH13"},
	{"UL_CM2", NULL, "UL_CM2_CH14"},
	{"UL_CM2", NULL, "UL_CM2_CH15"},
	{"UL_CM2", NULL, "UL_CM2_CH16"},
	{"UL_CM2", NULL, "UL_CM2_CH17"},
	{"UL_CM2", NULL, "UL_CM2_CH18"},
	{"UL_CM2", NULL, "UL_CM2_CH19"},
	{"UL_CM2", NULL, "UL_CM2_CH20"},
	{"UL_CM2", NULL, "UL_CM2_CH21"},
	{"UL_CM2", NULL, "UL_CM2_CH22"},
	{"UL_CM2", NULL, "UL_CM2_CH23"},
	{"UL_CM2", NULL, "UL_CM2_CH24"},
	{"UL_CM2", NULL, "UL_CM2_CH25"},
	{"UL_CM2", NULL, "UL_CM2_CH26"},
	{"UL_CM2", NULL, "UL_CM2_CH27"},
	{"UL_CM2", NULL, "UL_CM2_CH28"},
	{"UL_CM2", NULL, "UL_CM2_CH29"},
	{"UL_CM2", NULL, "UL_CM2_CH30"},
	{"UL_CM2", NULL, "UL_CM2_CH31"},
	{"UL_CM2", NULL, "UL_CM2_CH32"},
	{"UL_CM2_CH1", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM2_CH1", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM2_CH1", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM2_CH1", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	// {"UL_CM2_CH1", "ADDA_UL_CH5", "ADDA_CH56_UL_Mux"},
	// {"UL_CM2_CH1", "ADDA_UL_CH6", "ADDA_CH56_UL_Mux"},
	{"UL_CM2_CH2", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM2_CH2", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM2_CH2", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM2_CH2", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	// {"UL_CM2_CH2", "ADDA_UL_CH5", "ADDA_CH56_UL_Mux"},
	// {"UL_CM2_CH2", "ADDA_UL_CH6", "ADDA_CH56_UL_Mux"},
	{"UL_CM2_CH3", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM2_CH3", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM2_CH3", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM2_CH3", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	// {"UL_CM2_CH3", "ADDA_UL_CH5", "ADDA_CH56_UL_Mux"},
	// {"UL_CM2_CH3", "ADDA_UL_CH6", "ADDA_CH56_UL_Mux"},
	{"UL_CM2_CH4", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM2_CH4", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM2_CH4", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM2_CH4", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	// {"UL_CM2_CH4", "ADDA_UL_CH5", "ADDA_CH56_UL_Mux"},
	// {"UL_CM2_CH4", "ADDA_UL_CH6", "ADDA_CH56_UL_Mux"},
	{"UL_CM2_CH5", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM2_CH5", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM2_CH5", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM2_CH5", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	// {"UL_CM2_CH5", "ADDA_UL_CH5", "ADDA_CH56_UL_Mux"},
	// {"UL_CM2_CH5", "ADDA_UL_CH6", "ADDA_CH56_UL_Mux"},
	{"UL_CM2_CH6", "ADDA_UL_CH1", "ADDA_UL_Mux"},
	{"UL_CM2_CH6", "ADDA_UL_CH2", "ADDA_UL_Mux"},
	{"UL_CM2_CH6", "ADDA_UL_CH3", "ADDA_CH34_UL_Mux"},
	{"UL_CM2_CH6", "ADDA_UL_CH4", "ADDA_CH34_UL_Mux"},
	// {"UL_CM2_CH6", "ADDA_UL_CH5", "ADDA_CH56_UL_Mux"},
	// {"UL_CM2_CH6", "ADDA_UL_CH6", "ADDA_CH56_UL_Mux"},
	{"UL_CM2_CH1", "HW_SRC_0_OUT_CH1", "HW_SRC_0_Out"},
	{"UL_CM2_CH2", "HW_SRC_0_OUT_CH2", "HW_SRC_0_Out"},
	{"UL_CM1_CH1", "I2SIN6_CH1", "I2SIN6"},
	{"UL_CM1_CH2", "I2SIN6_CH2", "I2SIN6"},

	{"DL6_VIRTUAL_OUTPUT", NULL, "Hostless_UL1 DL"},
	{"Hostless_UL1 DL", NULL, "DL6"},
};

static const struct mtk_base_memif_data memif_data[MT6881_MEMIF_NUM] = {
	[MT6881_MEMIF_DL0] = {
		.name = "DL0",
		.id = MT6881_MEMIF_DL0,
		.reg_ofs_base = AFE_DL0_BASE,
		.reg_ofs_cur = AFE_DL0_CUR,
		.reg_ofs_end = AFE_DL0_END,
		.reg_ofs_base_msb = AFE_DL0_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL0_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL0_END_MSB,
		.fs_reg = AFE_DL0_CON0,
		.fs_shift = DL0_SEL_FS_SFT,
		.fs_maskbit = DL0_SEL_FS_MASK,
		.mono_reg = AFE_DL0_CON0,
		.mono_shift = DL0_MONO_SFT,
		.enable_reg = AFE_DL0_CON0,
		.enable_shift = DL0_ON_SFT,
		.hd_reg = AFE_DL0_CON0,
		.hd_mask = DL0_HD_MODE_MASK,
		.hd_shift = DL0_HD_MODE_SFT,
		.hd_align_reg = AFE_DL0_CON0,
		.hd_align_mshift = DL0_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL0_CON0,
		.pbuf_mask = DL0_PBUF_SIZE_MASK,
		.pbuf_shift = DL0_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL0_CON0,
		.minlen_mask = DL0_MINLEN_MASK,
		.minlen_shift = DL0_MINLEN_SFT,
		.maxlen_reg = AFE_DL0_CON0,
		.maxlen_mask = DL0_MAXLEN_MASK,
		.maxlen_shift = DL0_MAXLEN_SFT,
	},
	[MT6881_MEMIF_DL1] = {
		.name = "DL1",
		.id = MT6881_MEMIF_DL1,
		.reg_ofs_base = AFE_DL1_BASE,
		.reg_ofs_cur = AFE_DL1_CUR,
		.reg_ofs_end = AFE_DL1_END,
		.reg_ofs_base_msb = AFE_DL1_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL1_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL1_END_MSB,
		.fs_reg = AFE_DL1_CON0,
		.fs_shift = DL1_SEL_FS_SFT,
		.fs_maskbit = DL1_SEL_FS_MASK,
		.mono_reg = AFE_DL1_CON0,
		.mono_shift = DL1_MONO_SFT,
		.enable_reg = AFE_DL1_CON0,
		.enable_shift = DL1_ON_SFT,
		.hd_reg = AFE_DL1_CON0,
		.hd_mask = DL1_HD_MODE_MASK,
		.hd_shift = DL1_HD_MODE_SFT,
		.hd_align_reg = AFE_DL1_CON0,
		.hd_align_mshift = DL1_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL1_CON0,
		.pbuf_mask = DL1_PBUF_SIZE_MASK,
		.pbuf_shift = DL1_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL1_CON0,
		.minlen_mask = DL1_MINLEN_MASK,
		.minlen_shift = DL1_MINLEN_SFT,
		.maxlen_reg = AFE_DL1_CON0,
		.maxlen_mask = DL1_MAXLEN_MASK,
		.maxlen_shift = DL1_MAXLEN_SFT,
	},
	[MT6881_MEMIF_DL2] = {
		.name = "DL2",
		.id = MT6881_MEMIF_DL2,
		.reg_ofs_base = AFE_DL2_BASE,
		.reg_ofs_cur = AFE_DL2_CUR,
		.reg_ofs_end = AFE_DL2_END,
		.reg_ofs_base_msb = AFE_DL2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL2_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL2_END_MSB,
		.fs_reg = AFE_DL2_CON0,
		.fs_shift = DL2_SEL_FS_SFT,
		.fs_maskbit = DL2_SEL_FS_MASK,
		.mono_reg = AFE_DL2_CON0,
		.mono_shift = DL2_MONO_SFT,
		.enable_reg = AFE_DL2_CON0,
		.enable_shift = DL2_ON_SFT,
		.hd_reg = AFE_DL2_CON0,
		.hd_mask = DL2_HD_MODE_MASK,
		.hd_shift = DL2_HD_MODE_SFT,
		.hd_align_reg = AFE_DL2_CON0,
		.hd_align_mshift = DL2_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL2_CON0,
		.pbuf_mask = DL2_PBUF_SIZE_MASK,
		.pbuf_shift = DL2_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL2_CON0,
		.minlen_mask = DL2_MINLEN_MASK,
		.minlen_shift = DL2_MINLEN_SFT,
		.maxlen_reg = AFE_DL2_CON0,
		.maxlen_mask = DL2_MAXLEN_MASK,
		.maxlen_shift = DL2_MAXLEN_SFT,
	},
	[MT6881_MEMIF_DL3] = {
		.name = "DL3",
		.id = MT6881_MEMIF_DL3,
		.reg_ofs_base = AFE_DL3_BASE,
		.reg_ofs_cur = AFE_DL3_CUR,
		.reg_ofs_end = AFE_DL3_END,
		.reg_ofs_base_msb = AFE_DL3_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL3_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL3_END_MSB,
		.fs_reg = AFE_DL3_CON0,
		.fs_shift = DL3_SEL_FS_SFT,
		.fs_maskbit = DL3_SEL_FS_MASK,
		.mono_reg = AFE_DL3_CON0,
		.mono_shift = DL3_MONO_SFT,
		.enable_reg = AFE_DL3_CON0,
		.enable_shift = DL3_ON_SFT,
		.hd_reg = AFE_DL3_CON0,
		.hd_mask = DL3_HD_MODE_MASK,
		.hd_shift = DL3_HD_MODE_SFT,
		.hd_align_reg = AFE_DL3_CON0,
		.hd_align_mshift = DL3_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL3_CON0,
		.pbuf_mask = DL3_PBUF_SIZE_MASK,
		.pbuf_shift = DL3_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL3_CON0,
		.minlen_mask = DL3_MINLEN_MASK,
		.minlen_shift = DL3_MINLEN_SFT,
		.maxlen_reg = AFE_DL3_CON0,
		.maxlen_mask = DL3_MAXLEN_MASK,
		.maxlen_shift = DL3_MAXLEN_SFT,
	},
	[MT6881_MEMIF_DL4] = {
		.name = "DL4",
		.id = MT6881_MEMIF_DL4,
		.reg_ofs_base = AFE_DL4_BASE,
		.reg_ofs_cur = AFE_DL4_CUR,
		.reg_ofs_end = AFE_DL4_END,
		.reg_ofs_base_msb = AFE_DL4_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL4_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL4_END_MSB,
		.fs_reg = AFE_DL4_CON0,
		.fs_shift = DL4_SEL_FS_SFT,
		.fs_maskbit = DL4_SEL_FS_MASK,
		.mono_reg = AFE_DL4_CON0,
		.mono_shift = DL4_MONO_SFT,
		.enable_reg = AFE_DL4_CON0,
		.enable_shift = DL4_ON_SFT,
		.hd_reg = AFE_DL4_CON0,
		.hd_mask = DL4_HD_MODE_MASK,
		.hd_shift = DL4_HD_MODE_SFT,
		.hd_align_reg = AFE_DL4_CON0,
		.hd_align_mshift = DL4_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL4_CON0,
		.pbuf_mask = DL4_PBUF_SIZE_MASK,
		.pbuf_shift = DL4_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL4_CON0,
		.minlen_mask = DL4_MINLEN_MASK,
		.minlen_shift = DL4_MINLEN_SFT,
		.maxlen_reg = AFE_DL4_CON0,
		.maxlen_mask = DL4_MAXLEN_MASK,
		.maxlen_shift = DL4_MAXLEN_SFT,
	},
	[MT6881_MEMIF_DL5] = {
		.name = "DL5",
		.id = MT6881_MEMIF_DL5,
		.reg_ofs_base = AFE_DL5_BASE,
		.reg_ofs_cur = AFE_DL5_CUR,
		.reg_ofs_end = AFE_DL5_END,
		.reg_ofs_base_msb = AFE_DL5_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL5_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL5_END_MSB,
		.fs_reg = AFE_DL5_CON0,
		.fs_shift = DL5_SEL_FS_SFT,
		.fs_maskbit = DL5_SEL_FS_MASK,
		.mono_reg = AFE_DL5_CON0,
		.mono_shift = DL5_MONO_SFT,
		.enable_reg = AFE_DL5_CON0,
		.enable_shift = DL5_ON_SFT,
		.hd_reg = AFE_DL5_CON0,
		.hd_mask = DL5_HD_MODE_MASK,
		.hd_shift = DL5_HD_MODE_SFT,
		.hd_align_reg = AFE_DL5_CON0,
		.hd_align_mshift = DL5_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL5_CON0,
		.pbuf_mask = DL5_PBUF_SIZE_MASK,
		.pbuf_shift = DL5_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL5_CON0,
		.minlen_mask = DL5_MINLEN_MASK,
		.minlen_shift = DL5_MINLEN_SFT,
		.maxlen_reg = AFE_DL5_CON0,
		.maxlen_mask = DL5_MAXLEN_MASK,
		.maxlen_shift = DL5_MAXLEN_SFT,
	},
	[MT6881_MEMIF_DL6] = {
		.name = "DL6",
		.id = MT6881_MEMIF_DL6,
		.reg_ofs_base = AFE_DL6_BASE,
		.reg_ofs_cur = AFE_DL6_CUR,
		.reg_ofs_end = AFE_DL6_END,
		.reg_ofs_base_msb = AFE_DL6_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL6_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL6_END_MSB,
		.fs_reg = AFE_DL6_CON0,
		.fs_shift = DL6_SEL_FS_SFT,
		.fs_maskbit = DL6_SEL_FS_MASK,
		.mono_reg = AFE_DL6_CON0,
		.mono_shift = DL6_MONO_SFT,
		.enable_reg = AFE_DL6_CON0,
		.enable_shift = DL6_ON_SFT,
		.hd_reg = AFE_DL6_CON0,
		.hd_mask = DL6_HD_MODE_MASK,
		.hd_shift = DL6_HD_MODE_SFT,
		.hd_align_reg = AFE_DL6_CON0,
		.hd_align_mshift = DL6_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL6_CON0,
		.pbuf_mask = DL6_PBUF_SIZE_MASK,
		.pbuf_shift = DL6_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL6_CON0,
		.minlen_mask = DL6_MINLEN_MASK,
		.minlen_shift = DL6_MINLEN_SFT,
		.maxlen_reg = AFE_DL6_CON0,
		.maxlen_mask = DL6_MAXLEN_MASK,
		.maxlen_shift = DL6_MAXLEN_SFT,
	},
	[MT6881_MEMIF_DL7] = {
		.name = "DL7",
		.id = MT6881_MEMIF_DL7,
		.reg_ofs_base = AFE_DL7_BASE,
		.reg_ofs_cur = AFE_DL7_CUR,
		.reg_ofs_end = AFE_DL7_END,
		.reg_ofs_base_msb = AFE_DL7_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL7_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL7_END_MSB,
		.fs_reg = AFE_DL7_CON0,
		.fs_shift = DL7_SEL_FS_SFT,
		.fs_maskbit = DL7_SEL_FS_MASK,
		.mono_reg = AFE_DL7_CON0,
		.mono_shift = DL7_MONO_SFT,
		.enable_reg = AFE_DL7_CON0,
		.enable_shift = DL7_ON_SFT,
		.hd_reg = AFE_DL7_CON0,
		.hd_mask = DL7_HD_MODE_MASK,
		.hd_shift = DL7_HD_MODE_SFT,
		.hd_align_reg = AFE_DL7_CON0,
		.hd_align_mshift = DL7_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL7_CON0,
		.pbuf_mask = DL7_PBUF_SIZE_MASK,
		.pbuf_shift = DL7_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL7_CON0,
		.minlen_mask = DL7_MINLEN_MASK,
		.minlen_shift = DL7_MINLEN_SFT,
		.maxlen_reg = AFE_DL7_CON0,
		.maxlen_mask = DL7_MAXLEN_MASK,
		.maxlen_shift = DL7_MAXLEN_SFT,
	},
	[MT6881_MEMIF_DL8] = {
		.name = "DL8",
		.id = MT6881_MEMIF_DL8,
		.reg_ofs_base = AFE_DL8_BASE,
		.reg_ofs_cur = AFE_DL8_CUR,
		.reg_ofs_end = AFE_DL8_END,
		.reg_ofs_base_msb = AFE_DL8_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL8_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL8_END_MSB,
		.fs_reg = AFE_DL8_CON0,
		.fs_shift = DL8_SEL_FS_SFT,
		.fs_maskbit = DL8_SEL_FS_MASK,
		.mono_reg = AFE_DL8_CON0,
		.mono_shift = DL8_MONO_SFT,
		.enable_reg = AFE_DL8_CON0,
		.enable_shift = DL8_ON_SFT,
		.hd_reg = AFE_DL8_CON0,
		.hd_mask = DL8_HD_MODE_MASK,
		.hd_shift = DL8_HD_MODE_SFT,
		.hd_align_reg = AFE_DL8_CON0,
		.hd_align_mshift = DL8_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL8_CON0,
		.pbuf_mask = DL8_PBUF_SIZE_MASK,
		.pbuf_shift = DL8_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL8_CON0,
		.minlen_mask = DL8_MINLEN_MASK,
		.minlen_shift = DL8_MINLEN_SFT,
		.maxlen_reg = AFE_DL8_CON0,
		.maxlen_mask = DL8_MAXLEN_MASK,
		.maxlen_shift = DL8_MAXLEN_SFT,
	},
	[MT6881_MEMIF_DL23] = {
		.name = "DL23",
		.id = MT6881_MEMIF_DL23,
		.reg_ofs_base = AFE_DL44_BASE,
		.reg_ofs_cur = AFE_DL44_CUR,
		.reg_ofs_end = AFE_DL44_END,
		.reg_ofs_base_msb = AFE_DL44_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL44_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL44_END_MSB,
		.fs_reg = AFE_DL44_CON0,
		.fs_shift = DL44_SEL_FS_SFT,
		.fs_maskbit = DL44_SEL_FS_MASK,
		.mono_reg = AFE_DL44_CON0,
		.mono_shift = DL44_MONO_SFT,
		.enable_reg = AFE_DL44_CON0,
		.enable_shift = DL44_ON_SFT,
		.hd_reg = AFE_DL44_CON0,
		.hd_mask = DL44_HD_MODE_MASK,
		.hd_shift = DL44_HD_MODE_SFT,
		.hd_align_reg = AFE_DL44_CON0,
		.hd_align_mshift = DL44_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL44_CON0,
		.pbuf_mask = DL44_PBUF_SIZE_MASK,
		.pbuf_shift = DL44_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL44_CON0,
		.minlen_mask = DL44_MINLEN_MASK,
		.minlen_shift = DL44_MINLEN_SFT,
		.maxlen_reg = AFE_DL44_CON0,
		.maxlen_mask = DL44_MAXLEN_MASK,
		.maxlen_shift = DL44_MAXLEN_SFT,
	},
	[MT6881_MEMIF_DL24] = {
		.name = "DL24",
		.id = MT6881_MEMIF_DL24,
		.reg_ofs_base = AFE_DL45_BASE,
		.reg_ofs_cur = AFE_DL45_CUR,
		.reg_ofs_end = AFE_DL45_END,
		.reg_ofs_base_msb = AFE_DL45_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL45_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL45_END_MSB,
		.fs_reg = AFE_DL45_CON0,
		.fs_shift = DL45_SEL_FS_SFT,
		.fs_maskbit = DL45_SEL_FS_MASK,
		.mono_reg = AFE_DL45_CON0,
		.mono_shift = DL45_MONO_SFT,
		.enable_reg = AFE_DL45_CON0,
		.enable_shift = DL45_ON_SFT,
		.hd_reg = AFE_DL45_CON0,
		.hd_mask = DL45_HD_MODE_MASK,
		.hd_shift = DL45_HD_MODE_SFT,
		.hd_align_reg = AFE_DL45_CON0,
		.hd_align_mshift = DL45_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL45_CON0,
		.pbuf_mask = DL45_PBUF_SIZE_MASK,
		.pbuf_shift = DL45_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL45_CON0,
		.minlen_mask = DL45_MINLEN_MASK,
		.minlen_shift = DL45_MINLEN_SFT,
		.maxlen_reg = AFE_DL45_CON0,
		.maxlen_mask = DL45_MAXLEN_MASK,
		.maxlen_shift = DL45_MAXLEN_SFT,
	},
	[MT6881_MEMIF_DL25] = {
		.name = "DL25",
		.id = MT6881_MEMIF_DL25,
		.reg_ofs_base = AFE_DL46_BASE,
		.reg_ofs_cur = AFE_DL46_CUR,
		.reg_ofs_end = AFE_DL46_END,
		.reg_ofs_base_msb = AFE_DL46_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL46_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL46_END_MSB,
		.fs_reg = AFE_DL46_CON0,
		.fs_shift = DL46_SEL_FS_SFT,
		.fs_maskbit = DL46_SEL_FS_MASK,
		.mono_reg = AFE_DL46_CON0,
		.mono_shift = DL46_MONO_SFT,
		.enable_reg = AFE_DL46_CON0,
		.enable_shift = DL46_ON_SFT,
		.hd_reg = AFE_DL46_CON0,
		.hd_mask = DL46_HD_MODE_MASK,
		.hd_shift = DL46_HD_MODE_SFT,
		.hd_align_reg = AFE_DL46_CON0,
		.hd_align_mshift = DL46_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL46_CON0,
		.pbuf_mask = DL46_PBUF_SIZE_MASK,
		.pbuf_shift = DL46_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL46_CON0,
		.minlen_mask = DL46_MINLEN_MASK,
		.minlen_shift = DL46_MINLEN_SFT,
		.maxlen_reg = AFE_DL46_CON0,
		.maxlen_mask = DL46_MAXLEN_MASK,
		.maxlen_shift = DL46_MAXLEN_SFT,
	},
	[MT6881_MEMIF_DL_24CH] = {
		.name = "DL_24CH",
		.id = MT6881_MEMIF_DL_24CH,
		.reg_ofs_base = AFE_DL_24CH_BASE,
		.reg_ofs_cur = AFE_DL_24CH_CUR,
		.reg_ofs_end = AFE_DL_24CH_END,
		.reg_ofs_base_msb = AFE_DL_24CH_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL_24CH_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL_24CH_END_MSB,
		.fs_reg = AFE_DL_24CH_CON0,
		.fs_shift = DL_24CH_SEL_FS_SFT,
		.fs_maskbit = DL_24CH_SEL_FS_MASK,
		.mono_reg = -1,
		.mono_shift = -1,
		.enable_reg = AFE_DL_24CH_CON0,
		.enable_shift = DL_24CH_ON_SFT,
		.hd_reg = AFE_DL_24CH_CON0,
		.hd_mask = DL_24CH_HD_MODE_MASK,
		.hd_shift = DL_24CH_HD_MODE_SFT,
		.hd_align_reg = AFE_DL_24CH_CON0,
		.hd_align_mshift = DL_24CH_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL_24CH_CON0,
		.pbuf_mask = DL_24CH_PBUF_SIZE_MASK,
		.pbuf_shift = DL_24CH_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL_24CH_CON0,
		.minlen_mask = DL_24CH_MINLEN_MASK,
		.minlen_shift = DL_24CH_MINLEN_SFT,
		.maxlen_reg = AFE_DL_24CH_CON0,
		.maxlen_mask = DL_24CH_MAXLEN_MASK,
		.maxlen_shift = DL_24CH_MAXLEN_SFT,
		.ch_num_reg = AFE_DL_24CH_CON0,
		.ch_num_maskbit = DL_24CH_NUM_MASK,
		.ch_num_shift = DL_24CH_NUM_SFT,
	},
	[MT6881_MEMIF_DL_48CH] = {
		.name = "DL_48CH",
		.id = MT6881_MEMIF_DL_48CH,
		.reg_ofs_base = AFE_DL_48CH_BASE,
		.reg_ofs_cur = AFE_DL_48CH_CUR,
		.reg_ofs_end = AFE_DL_48CH_END,
		.reg_ofs_base_msb = AFE_DL_48CH_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL_48CH_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL_48CH_END_MSB,
		.fs_reg = AFE_DL_48CH_CON0,
		.fs_shift = DL_48CH_SEL_FS_SFT,
		.fs_maskbit = DL_48CH_SEL_FS_MASK,
		.mono_reg = -1,
		.mono_shift = -1,
		.enable_reg = AFE_DL_48CH_CON0,
		.enable_shift = DL_48CH_ON_SFT,
		.hd_reg = AFE_DL_48CH_CON0,
		.hd_mask = DL_48CH_HD_MODE_MASK,
		.hd_shift = DL_48CH_HD_MODE_SFT,
		.hd_align_reg = AFE_DL_48CH_CON0,
		.hd_align_mshift = DL_48CH_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_DL_48CH_CON0,
		.pbuf_mask = DL_48CH_PBUF_SIZE_MASK,
		.pbuf_shift = DL_48CH_PBUF_SIZE_SFT,
		.minlen_reg = AFE_DL_48CH_CON0,
		.minlen_mask = DL_48CH_MINLEN_MASK,
		.minlen_shift = DL_48CH_MINLEN_SFT,
		.maxlen_reg = AFE_DL_48CH_CON0,
		.maxlen_mask = DL_48CH_MAXLEN_MASK,
		.maxlen_shift = DL_48CH_MAXLEN_SFT,
		.ch_num_reg = AFE_DL_48CH_CON0,
		.ch_num_maskbit = DL_48CH_NUM_MASK,
		.ch_num_shift = DL_48CH_NUM_SFT,
	},
	[MT6881_MEMIF_VUL0] = {
		.name = "VUL0",
		.id = MT6881_MEMIF_VUL0,
		.reg_ofs_base = AFE_VUL0_BASE,
		.reg_ofs_cur = AFE_VUL0_CUR,
		.reg_ofs_end = AFE_VUL0_END,
		.reg_ofs_base_msb = AFE_VUL0_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL0_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL0_END_MSB,
		.fs_reg = AFE_VUL0_CON0,
		.fs_shift = VUL0_SEL_FS_SFT,
		.fs_maskbit = VUL0_SEL_FS_MASK,
		.mono_reg = AFE_VUL0_CON0,
		.mono_shift = VUL0_MONO_SFT,
		.enable_reg = AFE_VUL0_CON0,
		.enable_shift = VUL0_ON_SFT,
		.hd_reg = AFE_VUL0_CON0,
		.hd_mask = VUL0_HD_MODE_MASK,
		.hd_shift = VUL0_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL0_CON0,
		.hd_align_mshift = VUL0_HALIGN_SFT,
		.minlen_reg = AFE_VUL0_CON0,
		.minlen_mask = VUL0_MINLEN_MASK,
		.minlen_shift = VUL0_MINLEN_SFT,
		.maxlen_reg = AFE_VUL0_CON0,
		.maxlen_mask = VUL0_MAXLEN_MASK,
		.maxlen_shift = VUL0_MAXLEN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6881_MEMIF_VUL1] = {
		.name = "VUL1",
		.id = MT6881_MEMIF_VUL1,
		.reg_ofs_base = AFE_VUL1_BASE,
		.reg_ofs_cur = AFE_VUL1_CUR,
		.reg_ofs_end = AFE_VUL1_END,
		.reg_ofs_base_msb = AFE_VUL1_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL1_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL1_END_MSB,
		.fs_reg = AFE_VUL1_CON0,
		.fs_shift = VUL1_SEL_FS_SFT,
		.fs_maskbit = VUL1_SEL_FS_MASK,
		.mono_reg = AFE_VUL1_CON0,
		.mono_shift = VUL1_MONO_SFT,
		.enable_reg = AFE_VUL1_CON0,
		.enable_shift = VUL1_ON_SFT,
		.hd_reg = AFE_VUL1_CON0,
		.hd_mask = VUL1_HD_MODE_MASK,
		.hd_shift = VUL1_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL1_CON0,
		.hd_align_mshift = VUL1_HALIGN_SFT,
		.minlen_reg = AFE_VUL1_CON0,
		.minlen_mask = VUL1_MINLEN_MASK,
		.minlen_shift = VUL1_MINLEN_SFT,
		.maxlen_reg = AFE_VUL1_CON0,
		.maxlen_mask = VUL1_MAXLEN_MASK,
		.maxlen_shift = VUL1_MAXLEN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6881_MEMIF_VUL2] = {
		.name = "VUL2",
		.id = MT6881_MEMIF_VUL2,
		.reg_ofs_base = AFE_VUL2_BASE,
		.reg_ofs_cur = AFE_VUL2_CUR,
		.reg_ofs_end = AFE_VUL2_END,
		.reg_ofs_base_msb = AFE_VUL2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL2_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL2_END_MSB,
		.fs_reg = AFE_VUL2_CON0,
		.fs_shift = VUL2_SEL_FS_SFT,
		.fs_maskbit = VUL2_SEL_FS_MASK,
		.mono_reg = AFE_VUL2_CON0,
		.mono_shift = VUL2_MONO_SFT,
		.enable_reg = AFE_VUL2_CON0,
		.enable_shift = VUL2_ON_SFT,
		.hd_reg = AFE_VUL2_CON0,
		.hd_mask = VUL2_HD_MODE_MASK,
		.hd_shift = VUL2_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL2_CON0,
		.hd_align_mshift = VUL2_HALIGN_SFT,
		.minlen_reg = AFE_VUL2_CON0,
		.minlen_mask = VUL2_MINLEN_MASK,
		.minlen_shift = VUL2_MINLEN_SFT,
		.maxlen_reg = AFE_VUL2_CON0,
		.maxlen_mask = VUL2_MAXLEN_MASK,
		.maxlen_shift = VUL2_MAXLEN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6881_MEMIF_VUL3] = {
		.name = "VUL3",
		.id = MT6881_MEMIF_VUL3,
		.reg_ofs_base = AFE_VUL3_BASE,
		.reg_ofs_cur = AFE_VUL3_CUR,
		.reg_ofs_end = AFE_VUL3_END,
		.reg_ofs_base_msb = AFE_VUL3_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL3_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL3_END_MSB,
		.fs_reg = AFE_VUL3_CON0,
		.fs_shift = VUL3_SEL_FS_SFT,
		.fs_maskbit = VUL3_SEL_FS_MASK,
		.mono_reg = AFE_VUL3_CON0,
		.mono_shift = VUL3_MONO_SFT,
		.enable_reg = AFE_VUL3_CON0,
		.enable_shift = VUL3_ON_SFT,
		.hd_reg = AFE_VUL3_CON0,
		.hd_mask = VUL3_HD_MODE_MASK,
		.hd_shift = VUL3_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL3_CON0,
		.hd_align_mshift = VUL3_HALIGN_SFT,
		.minlen_reg = AFE_VUL3_CON0,
		.minlen_mask = VUL3_MINLEN_MASK,
		.minlen_shift = VUL3_MINLEN_SFT,
		.maxlen_reg = AFE_VUL3_CON0,
		.maxlen_mask = VUL3_MAXLEN_MASK,
		.maxlen_shift = VUL3_MAXLEN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6881_MEMIF_VUL4] = {
		.name = "VUL4",
		.id = MT6881_MEMIF_VUL4,
		.reg_ofs_base = AFE_VUL4_BASE,
		.reg_ofs_cur = AFE_VUL4_CUR,
		.reg_ofs_end = AFE_VUL4_END,
		.reg_ofs_base_msb = AFE_VUL4_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL4_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL4_END_MSB,
		.fs_reg = AFE_VUL4_CON0,
		.fs_shift = VUL4_SEL_FS_SFT,
		.fs_maskbit = VUL4_SEL_FS_MASK,
		.mono_reg = AFE_VUL4_CON0,
		.mono_shift = VUL4_MONO_SFT,
		.enable_reg = AFE_VUL4_CON0,
		.enable_shift = VUL4_ON_SFT,
		.hd_reg = AFE_VUL4_CON0,
		.hd_mask = VUL4_HD_MODE_MASK,
		.hd_shift = VUL4_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL4_CON0,
		.hd_align_mshift = VUL4_HALIGN_SFT,
		.minlen_reg = AFE_VUL4_CON0,
		.minlen_mask = VUL4_MINLEN_MASK,
		.minlen_shift = VUL4_MINLEN_SFT,
		.maxlen_reg = AFE_VUL4_CON0,
		.maxlen_mask = VUL4_MAXLEN_MASK,
		.maxlen_shift = VUL4_MAXLEN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6881_MEMIF_VUL5] = {
		.name = "VUL5",
		.id = MT6881_MEMIF_VUL5,
		.reg_ofs_base = AFE_VUL5_BASE,
		.reg_ofs_cur = AFE_VUL5_CUR,
		.reg_ofs_end = AFE_VUL5_END,
		.reg_ofs_base_msb = AFE_VUL5_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL5_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL5_END_MSB,
		.fs_reg = AFE_VUL5_CON0,
		.fs_shift = VUL5_SEL_FS_SFT,
		.fs_maskbit = VUL5_SEL_FS_MASK,
		.mono_reg = AFE_VUL5_CON0,
		.mono_shift = VUL5_MONO_SFT,
		.enable_reg = AFE_VUL5_CON0,
		.enable_shift = VUL5_ON_SFT,
		.hd_reg = AFE_VUL5_CON0,
		.hd_mask = VUL5_HD_MODE_MASK,
		.hd_shift = VUL5_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL5_CON0,
		.hd_align_mshift = VUL5_HALIGN_SFT,
		.minlen_reg = AFE_VUL5_CON0,
		.minlen_mask = VUL5_MINLEN_MASK,
		.minlen_shift = VUL5_MINLEN_SFT,
		.maxlen_reg = AFE_VUL5_CON0,
		.maxlen_mask = VUL5_MAXLEN_MASK,
		.maxlen_shift = VUL5_MAXLEN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6881_MEMIF_VUL6] = {
		.name = "VUL6",
		.id = MT6881_MEMIF_VUL6,
		.reg_ofs_base = AFE_VUL6_BASE,
		.reg_ofs_cur = AFE_VUL6_CUR,
		.reg_ofs_end = AFE_VUL6_END,
		.reg_ofs_base_msb = AFE_VUL6_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL6_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL6_END_MSB,
		.fs_reg = AFE_VUL6_CON0,
		.fs_shift = VUL6_SEL_FS_SFT,
		.fs_maskbit = VUL6_SEL_FS_MASK,
		.mono_reg = AFE_VUL6_CON0,
		.mono_shift = VUL6_MONO_SFT,
		.enable_reg = AFE_VUL6_CON0,
		.enable_shift = VUL6_ON_SFT,
		.hd_reg = AFE_VUL6_CON0,
		.hd_mask = VUL6_HD_MODE_MASK,
		.hd_shift = VUL6_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL6_CON0,
		.hd_align_mshift = VUL6_HALIGN_SFT,
		.minlen_reg = AFE_VUL6_CON0,
		.minlen_mask = VUL6_MINLEN_MASK,
		.minlen_shift = VUL6_MINLEN_SFT,
		.maxlen_reg = AFE_VUL6_CON0,
		.maxlen_mask = VUL6_MAXLEN_MASK,
		.maxlen_shift = VUL6_MAXLEN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6881_MEMIF_VUL7] = {
		.name = "VUL7",
		.id = MT6881_MEMIF_VUL7,
		.reg_ofs_base = AFE_VUL7_BASE,
		.reg_ofs_cur = AFE_VUL7_CUR,
		.reg_ofs_end = AFE_VUL7_END,
		.reg_ofs_base_msb = AFE_VUL7_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL7_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL7_END_MSB,
		.fs_reg = AFE_VUL7_CON0,
		.fs_shift = VUL7_SEL_FS_SFT,
		.fs_maskbit = VUL7_SEL_FS_MASK,
		.mono_reg = AFE_VUL7_CON0,
		.mono_shift = VUL7_MONO_SFT,
		.enable_reg = AFE_VUL7_CON0,
		.enable_shift = VUL7_ON_SFT,
		.hd_reg = AFE_VUL7_CON0,
		.hd_mask = VUL7_HD_MODE_MASK,
		.hd_shift = VUL7_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL7_CON0,
		.hd_align_mshift = VUL7_HALIGN_SFT,
		.minlen_reg = AFE_VUL7_CON0,
		.minlen_mask = VUL7_MINLEN_MASK,
		.minlen_shift = VUL7_MINLEN_SFT,
		.maxlen_reg = AFE_VUL7_CON0,
		.maxlen_mask = VUL7_MAXLEN_MASK,
		.maxlen_shift = VUL7_MAXLEN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6881_MEMIF_VUL8] = {
		.name = "VUL8",
		.id = MT6881_MEMIF_VUL8,
		.reg_ofs_base = AFE_VUL8_BASE,
		.reg_ofs_cur = AFE_VUL8_CUR,
		.reg_ofs_end = AFE_VUL8_END,
		.reg_ofs_base_msb = AFE_VUL8_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL8_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL8_END_MSB,
		.fs_reg = AFE_VUL8_CON0,
		.fs_shift = VUL8_SEL_FS_SFT,
		.fs_maskbit = VUL8_SEL_FS_MASK,
		.mono_reg = AFE_VUL8_CON0,
		.mono_shift = VUL8_MONO_SFT,
		.enable_reg = AFE_VUL8_CON0,
		.enable_shift = VUL8_ON_SFT,
		.hd_reg = AFE_VUL8_CON0,
		.hd_mask = VUL8_HD_MODE_MASK,
		.hd_shift = VUL8_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL8_CON0,
		.hd_align_mshift = VUL8_HALIGN_SFT,
		.minlen_reg = AFE_VUL8_CON0,
		.minlen_mask = VUL8_MINLEN_MASK,
		.minlen_shift = VUL8_MINLEN_SFT,
		.maxlen_reg = AFE_VUL8_CON0,
		.maxlen_mask = VUL8_MAXLEN_MASK,
		.maxlen_shift = VUL8_MAXLEN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6881_MEMIF_VUL9] = {
		.name = "VUL9",
		.id = MT6881_MEMIF_VUL9,
		.reg_ofs_base = AFE_VUL9_BASE,
		.reg_ofs_cur = AFE_VUL9_CUR,
		.reg_ofs_end = AFE_VUL9_END,
		.reg_ofs_base_msb = AFE_VUL9_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL9_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL9_END_MSB,
		.fs_reg = AFE_VUL9_CON0,
		.fs_shift = VUL9_SEL_FS_SFT,
		.fs_maskbit = VUL9_SEL_FS_MASK,
		.mono_reg = AFE_VUL9_CON0,
		.mono_shift = VUL9_MONO_SFT,
		.enable_reg = AFE_VUL9_CON0,
		.enable_shift = VUL9_ON_SFT,
		.hd_reg = AFE_VUL9_CON0,
		.hd_mask = VUL9_HD_MODE_MASK,
		.hd_shift = VUL9_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL9_CON0,
		.hd_align_mshift = VUL9_HALIGN_SFT,
		.minlen_reg = AFE_VUL9_CON0,
		.minlen_mask = VUL9_MINLEN_MASK,
		.minlen_shift = VUL9_MINLEN_SFT,
		.maxlen_reg = AFE_VUL9_CON0,
		.maxlen_mask = VUL9_MAXLEN_MASK,
		.maxlen_shift = VUL9_MAXLEN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6881_MEMIF_VUL10] = {
		.name = "VUL10",
		.id = MT6881_MEMIF_VUL10,
		.reg_ofs_base = AFE_VUL10_BASE,
		.reg_ofs_cur = AFE_VUL10_CUR,
		.reg_ofs_end = AFE_VUL10_END,
		.reg_ofs_base_msb = AFE_VUL10_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL10_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL10_END_MSB,
		.fs_reg = AFE_VUL10_CON0,
		.fs_shift = VUL10_SEL_FS_SFT,
		.fs_maskbit = VUL10_SEL_FS_MASK,
		.mono_reg = AFE_VUL10_CON0,
		.mono_shift = VUL10_MONO_SFT,
		.enable_reg = AFE_VUL10_CON0,
		.enable_shift = VUL10_ON_SFT,
		.hd_reg = AFE_VUL10_CON0,
		.hd_mask = VUL10_HD_MODE_MASK,
		.hd_shift = VUL10_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL10_CON0,
		.hd_align_mshift = VUL10_HALIGN_SFT,
		.minlen_reg = AFE_VUL10_CON0,
		.minlen_mask = VUL10_MINLEN_MASK,
		.minlen_shift = VUL10_MINLEN_SFT,
		.maxlen_reg = AFE_VUL10_CON0,
		.maxlen_mask = VUL10_MAXLEN_MASK,
		.maxlen_shift = VUL10_MAXLEN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6881_MEMIF_VUL24] = {
		.name = "VUL24",
		.id = MT6881_MEMIF_VUL24,
		.reg_ofs_base = AFE_VUL37_BASE,
		.reg_ofs_cur = AFE_VUL37_CUR,
		.reg_ofs_end = AFE_VUL37_END,
		.reg_ofs_base_msb = AFE_VUL37_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL37_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL37_END_MSB,
		.fs_reg = AFE_VUL37_CON0,
		.fs_shift = VUL37_SEL_FS_SFT,
		.fs_maskbit = VUL37_SEL_FS_MASK,
		.mono_reg = AFE_VUL37_CON0,
		.mono_shift = VUL37_MONO_SFT,
		.enable_reg = AFE_VUL37_CON0,
		.enable_shift = VUL37_ON_SFT,
		.hd_reg = AFE_VUL37_CON0,
		.hd_mask = VUL37_HD_MODE_MASK,
		.hd_shift = VUL37_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL37_CON0,
		.hd_align_mshift = VUL37_HALIGN_SFT,
		.minlen_reg = AFE_VUL37_CON0,
		.minlen_mask = VUL37_MINLEN_MASK,
		.minlen_shift = VUL37_MINLEN_SFT,
		.maxlen_reg = AFE_VUL37_CON0,
		.maxlen_mask = VUL37_MAXLEN_MASK,
		.maxlen_shift = VUL37_MAXLEN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.out_on_use_reg = AFE_VUL37_CON0,
		.out_on_use_mask = OUT_ON_USE_VUL37_MASK,
		.out_on_use_shift = OUT_ON_USE_VUL37_SFT,
	},
	[MT6881_MEMIF_VUL25] = {
		.name = "VUL25",
		.id = MT6881_MEMIF_VUL25,
		.reg_ofs_base = AFE_VUL38_BASE,
		.reg_ofs_cur = AFE_VUL38_CUR,
		.reg_ofs_end = AFE_VUL38_END,
		.reg_ofs_base_msb = AFE_VUL38_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL38_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL38_END_MSB,
		.fs_reg = AFE_VUL38_CON0,
		.fs_shift = VUL38_SEL_FS_SFT,
		.fs_maskbit = VUL38_SEL_FS_MASK,
		.mono_reg = AFE_VUL38_CON0,
		.mono_shift = VUL38_MONO_SFT,
		.enable_reg = AFE_VUL38_CON0,
		.enable_shift = VUL38_ON_SFT,
		.hd_reg = AFE_VUL38_CON0,
		.hd_mask = VUL38_HD_MODE_MASK,
		.hd_shift = VUL38_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL38_CON0,
		.hd_align_mshift = VUL38_HALIGN_SFT,
		.minlen_reg = AFE_VUL38_CON0,
		.minlen_mask = VUL38_MINLEN_MASK,
		.minlen_shift = VUL38_MINLEN_SFT,
		.maxlen_reg = AFE_VUL38_CON0,
		.maxlen_mask = VUL38_MAXLEN_MASK,
		.maxlen_shift = VUL38_MAXLEN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.out_on_use_reg = AFE_VUL38_CON0,
		.out_on_use_mask = OUT_ON_USE_VUL38_MASK,
		.out_on_use_shift = OUT_ON_USE_VUL38_SFT,
	},
	[MT6881_MEMIF_VUL_CM0] = {
		.name = "VUL_CM0",
		.id = MT6881_MEMIF_VUL_CM0,
		.reg_ofs_base = AFE_VUL_CM0_BASE,
		.reg_ofs_cur = AFE_VUL_CM0_CUR,
		.reg_ofs_end = AFE_VUL_CM0_END,
		.reg_ofs_base_msb = AFE_VUL_CM0_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL_CM0_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL_CM0_END_MSB,
		//.fs_reg = AFE_CM0_CON0,
		//.fs_shift = AFE_CM0_1X_EN_SEL_FS_SFT,
		//.fs_maskbit = AFE_CM0_1X_EN_SEL_FS_MASK,
		//.mono_reg = AFE_VUL_CM0_CON0,
		//.mono_shift = VUL_CM0_MONO_SFT,
		.enable_reg = AFE_VUL_CM0_CON0,
		.enable_shift = VUL_CM0_ON_SFT,
		.hd_reg = AFE_VUL_CM0_CON0,
		.hd_mask = VUL_CM0_HD_MODE_MASK,
		.hd_shift = VUL_CM0_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL_CM0_CON0,
		.hd_align_mshift = VUL_CM0_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.minlen_reg = AFE_VUL_CM0_CON0,
		.minlen_mask = VUL_CM0_AXI_REQ_MINLEN_MASK,
		.minlen_shift = VUL_CM0_AXI_REQ_MINLEN_SFT,
		.maxlen_reg = AFE_VUL_CM0_CON0,
		.maxlen_mask = VUL_CM0_AXI_REQ_MAXLEN_MASK,
		.maxlen_shift = VUL_CM0_AXI_REQ_MAXLEN_SFT,
	},
	[MT6881_MEMIF_VUL_CM1] = {
		.name = "VUL_CM1",
		.id = MT6881_MEMIF_VUL_CM1,
		.reg_ofs_base = AFE_VUL_CM1_BASE,
		.reg_ofs_cur = AFE_VUL_CM1_CUR,
		.reg_ofs_end = AFE_VUL_CM1_END,
		.reg_ofs_base_msb = AFE_VUL_CM1_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL_CM1_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL_CM1_END_MSB,
		//.fs_reg = AFE_CM1_CON0,
		//.fs_shift = AFE_CM1_1X_EN_SEL_FS_SFT,
		//.fs_maskbit = AFE_CM1_1X_EN_SEL_FS_MASK,
		//.mono_reg = AFE_VUL_CM1_CON0,
		//.mono_shift = VUL_CM1_MONO_SFT,
		.enable_reg = AFE_VUL_CM1_CON0,
		.enable_shift = VUL_CM1_ON_SFT,
		.hd_reg = AFE_VUL_CM1_CON0,
		.hd_mask = VUL_CM1_HD_MODE_MASK,
		.hd_shift = VUL_CM1_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL_CM1_CON0,
		.hd_align_mshift = VUL_CM1_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.minlen_reg = AFE_VUL_CM1_CON0,
		.minlen_mask = VUL_CM1_AXI_REQ_MINLEN_MASK,
		.minlen_shift = VUL_CM1_AXI_REQ_MINLEN_SFT,
		.maxlen_reg = AFE_VUL_CM1_CON0,
		.maxlen_mask = VUL_CM1_AXI_REQ_MAXLEN_MASK,
		.maxlen_shift = VUL_CM1_AXI_REQ_MAXLEN_SFT,
	},
	[MT6881_MEMIF_VUL_CM2] = {
		.name = "VUL_CM2",
		.id = MT6881_MEMIF_VUL_CM2,
		.reg_ofs_base = AFE_VUL_CM2_BASE,
		.reg_ofs_cur = AFE_VUL_CM2_CUR,
		.reg_ofs_end = AFE_VUL_CM2_END,
		.reg_ofs_base_msb = AFE_VUL_CM2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL_CM2_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL_CM2_END_MSB,
		//.fs_reg = AFE_CM2_CON0,
		//.fs_shift = AFE_CM2_1X_EN_SEL_FS_SFT,
		//.fs_maskbit = AFE_CM2_1X_EN_SEL_FS_MASK,
		//.mono_reg = AFE_VUL_CM2_CON0,
		//.mono_shift = VUL_CM2_MONO_SFT,
		.enable_reg = AFE_VUL_CM2_CON0,
		.enable_shift = VUL_CM2_ON_SFT,
		.hd_reg = AFE_VUL_CM2_CON0,
		.hd_mask = VUL_CM2_HD_MODE_MASK,
		.hd_shift = VUL_CM2_HD_MODE_SFT,
		.hd_align_reg = AFE_VUL_CM2_CON0,
		.hd_align_mshift = VUL_CM2_HALIGN_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.minlen_reg = AFE_VUL_CM2_CON0,
		.minlen_mask = VUL_CM2_AXI_REQ_MINLEN_MASK,
		.minlen_shift = VUL_CM2_AXI_REQ_MINLEN_SFT,
		.maxlen_reg = AFE_VUL_CM2_CON0,
		.maxlen_mask = VUL_CM2_AXI_REQ_MAXLEN_MASK,
		.maxlen_shift = VUL_CM2_AXI_REQ_MAXLEN_SFT,
	},
	[MT6881_MEMIF_ETDM_IN1] = {
		.name = "ETDM_IN1",
		.id = MT6881_MEMIF_ETDM_IN1,
		.reg_ofs_base = AFE_ETDM_IN1_BASE,
		.reg_ofs_cur = AFE_ETDM_IN1_CUR,
		.reg_ofs_end = AFE_ETDM_IN1_END,
		.reg_ofs_base_msb = AFE_ETDM_IN1_BASE_MSB,
		.reg_ofs_cur_msb = AFE_ETDM_IN1_CUR_MSB,
		.reg_ofs_end_msb = AFE_ETDM_IN1_END_MSB,
		.fs_reg = ETDM_IN1_CON3,
		.fs_shift = REG_FS_TIMING_SEL_SFT,
		.fs_maskbit = REG_FS_TIMING_SEL_MASK,
		//.mono_reg = ,
		//.mono_shift = ,
		.enable_reg = AFE_ETDM_IN1_CON0,
		.enable_shift = ETDM_IN1_ON_SFT,
		.hd_reg = AFE_ETDM_IN1_CON0,
		.hd_mask = ETDM_IN1_HD_MODE_MASK,
		.hd_shift = ETDM_IN1_HD_MODE_SFT,
		.hd_align_reg = AFE_ETDM_IN1_CON0,
		.hd_align_mshift = ETDM_IN1_HALIGN_SFT,
		.hd_msb_shift = -1,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6881_MEMIF_ETDM_IN2] = {
		.name = "ETDM_IN2",
		.id = MT6881_MEMIF_ETDM_IN2,
		.reg_ofs_base = AFE_ETDM_IN2_BASE,
		.reg_ofs_cur = AFE_ETDM_IN2_CUR,
		.reg_ofs_end = AFE_ETDM_IN2_END,
		.reg_ofs_base_msb = AFE_ETDM_IN2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_ETDM_IN2_CUR_MSB,
		.reg_ofs_end_msb = AFE_ETDM_IN2_END_MSB,
		.fs_reg = ETDM_IN2_CON3,
		.fs_shift = REG_FS_TIMING_SEL_SFT,
		.fs_maskbit = REG_FS_TIMING_SEL_MASK,
		//.mono_reg = ,
		//.mono_shift = ,
		.enable_reg = AFE_ETDM_IN2_CON0,
		.enable_shift = ETDM_IN2_ON_SFT,
		.hd_reg = AFE_ETDM_IN2_CON0,
		.hd_mask = ETDM_IN2_HD_MODE_MASK,
		.hd_shift = ETDM_IN2_HD_MODE_SFT,
		.hd_align_reg = AFE_ETDM_IN2_CON0,
		.hd_align_mshift = ETDM_IN2_HALIGN_SFT,
		.hd_msb_shift = -1,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6881_MEMIF_ETDM_IN6] = {
		.name = "ETDM_IN6",
		.id = MT6881_MEMIF_ETDM_IN6,
		.reg_ofs_base = AFE_ETDM_IN6_BASE,
		.reg_ofs_cur = AFE_ETDM_IN6_CUR,
		.reg_ofs_end = AFE_ETDM_IN6_END,
		.reg_ofs_base_msb = AFE_ETDM_IN6_BASE_MSB,
		.reg_ofs_cur_msb = AFE_ETDM_IN6_CUR_MSB,
		.reg_ofs_end_msb = AFE_ETDM_IN6_END_MSB,
		.fs_reg = ETDM_IN6_CON3,
		.fs_shift = REG_FS_TIMING_SEL_SFT,
		.fs_maskbit = REG_FS_TIMING_SEL_MASK,
		//.mono_reg = ,
		//.mono_shift = ,
		.enable_reg = AFE_ETDM_IN6_CON0,
		.enable_shift = ETDM_IN6_ON_SFT,
		.hd_reg = AFE_ETDM_IN6_CON0,
		.hd_mask = ETDM_IN6_HD_MODE_MASK,
		.hd_shift = ETDM_IN6_HD_MODE_SFT,
		.hd_align_reg = AFE_ETDM_IN6_CON0,
		.hd_align_mshift = ETDM_IN6_HALIGN_SFT,
		.hd_msb_shift = -1,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
};

static const struct mtk_base_irq_data irq_data[MT6881_IRQ_NUM] = {
	[MT6881_IRQ_0] = {
		.id = MT6881_IRQ_0,
		.irq_cnt_reg = AFE_IRQ0_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ0_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ0_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ0_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ0_MCU_CFG0,
		.irq_en_shift = AFE_IRQ0_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ0_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ0_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ0_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_1] = {
		.id = MT6881_IRQ_1,
		.irq_cnt_reg = AFE_IRQ1_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ1_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ1_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ1_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ1_MCU_CFG0,
		.irq_en_shift = AFE_IRQ1_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ1_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ1_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ1_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_2] = {
		.id = MT6881_IRQ_2,
		.irq_cnt_reg = AFE_IRQ2_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ2_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ2_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ2_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ2_MCU_CFG0,
		.irq_en_shift = AFE_IRQ2_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ2_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ2_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ2_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_3] = {
		.id = MT6881_IRQ_3,
		.irq_cnt_reg = AFE_IRQ3_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ3_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ3_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ3_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ3_MCU_CFG0,
		.irq_en_shift = AFE_IRQ3_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ3_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ3_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ3_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_4] = {
		.id = MT6881_IRQ_4,
		.irq_cnt_reg = AFE_IRQ4_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ4_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ4_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ4_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ4_MCU_CFG0,
		.irq_en_shift = AFE_IRQ4_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ4_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ4_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ4_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_5] = {
		.id = MT6881_IRQ_5,
		.irq_cnt_reg = AFE_IRQ5_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ5_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ5_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ5_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ5_MCU_CFG0,
		.irq_en_shift = AFE_IRQ5_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ5_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ5_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ5_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_6] = {
		.id = MT6881_IRQ_6,
		.irq_cnt_reg = AFE_IRQ6_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ6_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ6_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ6_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ6_MCU_CFG0,
		.irq_en_shift = AFE_IRQ6_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ6_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ6_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ6_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_7] = {
		.id = MT6881_IRQ_7,
		.irq_cnt_reg = AFE_IRQ7_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ7_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ7_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ7_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ7_MCU_CFG0,
		.irq_en_shift = AFE_IRQ7_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ7_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ7_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ7_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_8] = {
		.id = MT6881_IRQ_8,
		.irq_cnt_reg = AFE_IRQ8_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ8_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ8_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ8_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ8_MCU_CFG0,
		.irq_en_shift = AFE_IRQ8_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ8_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ8_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ8_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_9] = {
		.id = MT6881_IRQ_9,
		.irq_cnt_reg = AFE_IRQ9_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ9_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ9_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ9_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ9_MCU_CFG0,
		.irq_en_shift = AFE_IRQ9_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ9_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ9_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ9_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_10] = {
		.id = MT6881_IRQ_10,
		.irq_cnt_reg = AFE_IRQ10_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ10_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ10_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ10_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ10_MCU_CFG0,
		.irq_en_shift = AFE_IRQ10_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ10_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ10_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ10_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_11] = {
		.id = MT6881_IRQ_11,
		.irq_cnt_reg = AFE_IRQ11_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ11_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ11_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ11_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ11_MCU_CFG0,
		.irq_en_shift = AFE_IRQ11_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ11_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ11_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ11_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_12] = {
		.id = MT6881_IRQ_12,
		.irq_cnt_reg = AFE_IRQ12_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ12_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ12_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ12_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ12_MCU_CFG0,
		.irq_en_shift = AFE_IRQ12_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ12_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ12_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ12_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_13] = {
		.id = MT6881_IRQ_13,
		.irq_cnt_reg = AFE_IRQ13_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ13_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ13_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ13_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ13_MCU_CFG0,
		.irq_en_shift = AFE_IRQ13_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ13_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ13_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ13_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_14] = {
		.id = MT6881_IRQ_14,
		.irq_cnt_reg = AFE_IRQ14_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ14_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ14_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ14_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ14_MCU_CFG0,
		.irq_en_shift = AFE_IRQ14_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ14_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ14_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ14_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_15] = {
		.id = MT6881_IRQ_15,
		.irq_cnt_reg = AFE_IRQ15_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ15_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ15_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ15_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ15_MCU_CFG0,
		.irq_en_shift = AFE_IRQ15_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ15_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ15_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ15_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_16] = {
		.id = MT6881_IRQ_16,
		.irq_cnt_reg = AFE_IRQ16_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ16_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ16_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ16_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ16_MCU_CFG0,
		.irq_en_shift = AFE_IRQ16_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ16_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ16_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ16_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_17] = {
		.id = MT6881_IRQ_17,
		.irq_cnt_reg = AFE_IRQ17_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ17_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ17_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ17_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ17_MCU_CFG0,
		.irq_en_shift = AFE_IRQ17_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ17_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ17_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ17_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_18] = {
		.id = MT6881_IRQ_18,
		.irq_cnt_reg = AFE_IRQ18_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ18_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ18_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ18_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ18_MCU_CFG0,
		.irq_en_shift = AFE_IRQ18_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ18_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ18_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ18_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_19] = {
		.id = MT6881_IRQ_19,
		.irq_cnt_reg = AFE_IRQ19_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ19_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ19_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ19_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ19_MCU_CFG0,
		.irq_en_shift = AFE_IRQ19_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ19_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ19_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ19_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_20] = {
		.id = MT6881_IRQ_20,
		.irq_cnt_reg = AFE_IRQ20_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ20_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ20_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ20_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ20_MCU_CFG0,
		.irq_en_shift = AFE_IRQ20_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ20_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ20_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ20_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_21] = {
		.id = MT6881_IRQ_21,
		.irq_cnt_reg = AFE_IRQ21_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ21_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ21_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ21_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ21_MCU_CFG0,
		.irq_en_shift = AFE_IRQ21_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ21_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ21_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ21_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_22] = {
		.id = MT6881_IRQ_22,
		.irq_cnt_reg = AFE_IRQ22_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ22_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ22_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ22_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ22_MCU_CFG0,
		.irq_en_shift = AFE_IRQ22_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ22_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ22_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ22_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_23] = {
		.id = MT6881_IRQ_23,
		.irq_cnt_reg = AFE_IRQ23_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ23_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ23_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ23_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ23_MCU_CFG0,
		.irq_en_shift = AFE_IRQ23_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ23_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ23_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ23_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_24] = {
		.id = MT6881_IRQ_24,
		.irq_cnt_reg = AFE_IRQ24_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ24_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ24_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ24_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ24_MCU_CFG0,
		.irq_en_shift = AFE_IRQ24_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ24_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ24_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ24_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_25] = {
		.id = MT6881_IRQ_25,
		.irq_cnt_reg = AFE_IRQ25_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ25_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ25_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ25_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ25_MCU_CFG0,
		.irq_en_shift = AFE_IRQ25_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ25_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ25_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ25_MCU_SCP_EN_SFT,
	},
	[MT6881_IRQ_26] = {
		.id = MT6881_IRQ_26,
		.irq_cnt_reg = AFE_IRQ26_MCU_CFG1,
		.irq_cnt_shift = AFE_IRQ_CNT_SHIFT,
		.irq_cnt_maskbit = AFE_IRQ_CNT_MASK,
		.irq_fs_reg = AFE_IRQ26_MCU_CFG0,
		.irq_fs_shift = AFE_IRQ26_MCU_FS_SFT,
		.irq_fs_maskbit = AFE_IRQ26_MCU_FS_MASK,
		.irq_en_reg = AFE_IRQ26_MCU_CFG0,
		.irq_en_shift = AFE_IRQ26_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ26_MCU_CFG1,
		.irq_clr_shift = AFE_IRQ26_CLR_CFG_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_scp_en_reg = AFE_IRQ_MCU_SCP_EN,
		.irq_scp_en_shift = IRQ26_MCU_SCP_EN_SFT,
	},
};

#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO)
static const int memif_irq_usage[MT6881_MEMIF_NUM] = {
	/* TODO: verify each memif & irq */
	[MT6881_MEMIF_DL0] = MT6881_IRQ_4,
	[MT6881_MEMIF_DL1] = MT6881_IRQ_1,
	[MT6881_MEMIF_DL2] = MT6881_IRQ_2,
	[MT6881_MEMIF_DL3] = MT6881_IRQ_3,
	[MT6881_MEMIF_DL4] = MT6881_IRQ_3,
	[MT6881_MEMIF_DL5] = MT6881_IRQ_3,
	[MT6881_MEMIF_DL6] = MT6881_IRQ_3,
	[MT6881_MEMIF_DL7] = MT6881_IRQ_7,
	[MT6881_MEMIF_DL8] = MT6881_IRQ_8,
	[MT6881_MEMIF_DL23] = MT6881_IRQ_9,
	[MT6881_MEMIF_DL24] = MT6881_IRQ_10,
	[MT6881_MEMIF_DL25] = MT6881_IRQ_11,
	[MT6881_MEMIF_DL_24CH] = MT6881_IRQ_12,
	[MT6881_MEMIF_DL_48CH] = MT6881_IRQ_9,
	[MT6881_MEMIF_VUL0] = MT6881_IRQ_13,
	[MT6881_MEMIF_VUL1] = MT6881_IRQ_14,
	[MT6881_MEMIF_VUL2] = MT6881_IRQ_15,
	[MT6881_MEMIF_VUL3] = MT6881_IRQ_16,
	[MT6881_MEMIF_VUL4] = MT6881_IRQ_17,
	[MT6881_MEMIF_VUL5] = MT6881_IRQ_18,
	[MT6881_MEMIF_VUL6] = MT6881_IRQ_19,
	[MT6881_MEMIF_VUL7] = MT6881_IRQ_20,
	[MT6881_MEMIF_VUL8] = MT6881_IRQ_21,
	[MT6881_MEMIF_VUL9] = MT6881_IRQ_22,
	[MT6881_MEMIF_VUL10] = MT6881_IRQ_23,
	[MT6881_MEMIF_VUL24] = MT6881_IRQ_24,
	[MT6881_MEMIF_VUL25] = MT6881_IRQ_25,
	[MT6881_MEMIF_VUL_CM0] = MT6881_IRQ_26,
	[MT6881_MEMIF_VUL_CM1] = MT6881_IRQ_0,
	[MT6881_MEMIF_VUL_CM2] = MT6881_IRQ_0,
	[MT6881_MEMIF_ETDM_IN1] = MT6881_IRQ_0,
	[MT6881_MEMIF_ETDM_IN2] = MT6881_IRQ_0,
	[MT6881_MEMIF_ETDM_IN6] = MT6881_IRQ_0,
};
#else
static const int memif_irq_usage[MT6881_MEMIF_NUM] = {
	/* ADSP will use IRQ 29/30 for urgent IRQ, please do not use */
	/* TODO: verify each memif & irq */
	[MT6881_MEMIF_DL0] = MT6881_IRQ_0,
	[MT6881_MEMIF_DL1] = MT6881_IRQ_1,
	[MT6881_MEMIF_DL2] = MT6881_IRQ_2,
	[MT6881_MEMIF_DL3] = MT6881_IRQ_3,
	[MT6881_MEMIF_DL4] = MT6881_IRQ_4,
	[MT6881_MEMIF_DL5] = MT6881_IRQ_5,
	[MT6881_MEMIF_DL6] = MT6881_IRQ_6,
	[MT6881_MEMIF_DL7] = MT6881_IRQ_7,
	[MT6881_MEMIF_DL8] = MT6881_IRQ_8,
	[MT6881_MEMIF_DL23] = MT6881_IRQ_9,
	[MT6881_MEMIF_DL24] = MT6881_IRQ_10,
	[MT6881_MEMIF_DL25] = MT6881_IRQ_11,
	[MT6881_MEMIF_DL_24CH] = MT6881_IRQ_12,
	[MT6881_MEMIF_VUL0] = MT6881_IRQ_13,
	[MT6881_MEMIF_VUL1] = MT6881_IRQ_14,
	[MT6881_MEMIF_VUL2] = MT6881_IRQ_15,
	[MT6881_MEMIF_VUL3] = MT6881_IRQ_16,
	[MT6881_MEMIF_VUL4] = MT6881_IRQ_17,
	[MT6881_MEMIF_VUL5] = MT6881_IRQ_18,
	[MT6881_MEMIF_VUL6] = MT6881_IRQ_19,
	[MT6881_MEMIF_VUL7] = MT6881_IRQ_20,
	[MT6881_MEMIF_VUL8] = MT6881_IRQ_21,
	[MT6881_MEMIF_VUL9] = MT6881_IRQ_22,
	[MT6881_MEMIF_VUL10] = MT6881_IRQ_23,
	[MT6881_MEMIF_VUL24] = MT6881_IRQ_24,
	[MT6881_MEMIF_VUL25] = MT6881_IRQ_25,
	[MT6881_MEMIF_VUL_CM0] = MT6881_IRQ_26,
	[MT6881_MEMIF_VUL_CM1] = MT6881_IRQ_0,
	[MT6881_MEMIF_ETDM_IN1] = MT6881_IRQ_0,
	[MT6881_MEMIF_ETDM_IN2] = MT6881_IRQ_0,
	[MT6881_MEMIF_ETDM_IN6] = MT6881_IRQ_0,
};
#endif

static bool mt6881_is_volatile_reg(struct device *dev, unsigned int reg)
{
	/* these auto-gen reg has read-only bit, so put it as volatile */
	/* volatile reg cannot be cached, so cannot be set when power off */
	switch (reg) {
	case AUDIO_TOP_CON0:    /* reg bit controlled by CCF */
	case AUDIO_TOP_CON1:    /* reg bit controlled by CCF */
	case AUDIO_TOP_CON2:
	case AUDIO_TOP_CON3:
	case AUDIO_TOP_CON4:
	case AUDIO_TOP_CON5:
	case AUD_TOP_MON_RG:
	case AFE_APLL1_TUNER_MON0:
	case AFE_APLL2_TUNER_MON0:
	case AFE_SPM_CONTROL_ACK:
	case AUDIO_TOP_IP_VERSION:
	case AUDIO_ENGEN_CON0_MON:
	case AFE_CONNSYS_I2S_IPM_VER_MON:
	case AFE_CONNSYS_I2S_MON:
	case AFE_PCM_INTF_MON:
	case AFE_PCM_TOP_IP_VERSION:
	case AFE_IRQ_MCU_STATUS:
	case AFE_CUSTOM_IRQ_MCU_STATUS:
	case AFE_IRQ_MCU_MON0:
	case AFE_IRQ_MCU_MON1:
	case AFE_IRQ_MCU_MON2:
	case AFE_IRQ0_CNT_MON:
	case AFE_IRQ1_CNT_MON:
	case AFE_IRQ2_CNT_MON:
	case AFE_IRQ3_CNT_MON:
	case AFE_IRQ4_CNT_MON:
	case AFE_IRQ5_CNT_MON:
	case AFE_IRQ6_CNT_MON:
	case AFE_IRQ7_CNT_MON:
	case AFE_IRQ8_CNT_MON:
	case AFE_IRQ9_CNT_MON:
	case AFE_IRQ10_CNT_MON:
	case AFE_IRQ11_CNT_MON:
	case AFE_IRQ12_CNT_MON:
	case AFE_IRQ13_CNT_MON:
	case AFE_IRQ14_CNT_MON:
	case AFE_IRQ15_CNT_MON:
	case AFE_IRQ16_CNT_MON:
	case AFE_IRQ17_CNT_MON:
	case AFE_IRQ18_CNT_MON:
	case AFE_IRQ19_CNT_MON:
	case AFE_IRQ20_CNT_MON:
	case AFE_IRQ21_CNT_MON:
	case AFE_IRQ22_CNT_MON:
	case AFE_IRQ23_CNT_MON:
	case AFE_IRQ24_CNT_MON:
	case AFE_IRQ25_CNT_MON:
	case AFE_IRQ26_CNT_MON:
	case AFE_STF_MON:
	case AFE_STF_IP_VERSION:
	case AFE_CM0_MON:
	case AFE_CM0_IP_VERSION:
	case AFE_CM1_MON:
	case AFE_CM1_IP_VERSION:
	case AFE_ADDA_UL0_SRC_DEBUG_MON0:
	case AFE_ADDA_UL0_SRC_MON0:
	case AFE_ADDA_UL0_SRC_MON1:
	case AFE_ADDA_UL0_IP_VERSION:
	case AFE_ADDA_UL1_SRC_DEBUG_MON0:
	case AFE_ADDA_UL1_SRC_MON0:
	case AFE_ADDA_UL1_SRC_MON1:
	case AFE_ADDA_UL1_IP_VERSION:
	case AFE_MTKAIF_IPM_VER_MON:
	case AFE_MTKAIF_MON:
	case AFE_AUD_PAD_TOP_MON:
	case ETDM_IN1_MON:
	case ETDM_IN2_MON:
	case ETDM_IN6_MON:
	case ETDM_OUT1_MON:
	case ETDM_OUT2_MON:
	case ETDM_OUT6_MON:
	case AFE_CONN_MON0:
	case AFE_CONN_MON1:
	case AFE_CONN_MON2:
	case AFE_CONN_MON3:
	case AFE_CONN_MON4:
	case AFE_CONN_MON5:
	case AFE_CBIP_SLV_DECODER_MON0:
	case AFE_CBIP_SLV_DECODER_MON1:
	case AFE_CBIP_SLV_MUX_MON0:
	case AFE_CBIP_SLV_MUX_MON1:
	case AFE_DL0_CUR_MSB:
	case AFE_DL0_CUR:
	case AFE_DL0_RCH_MON:
	case AFE_DL0_LCH_MON:
	case AFE_DL1_CUR_MSB:
	case AFE_DL1_CUR:
	case AFE_DL1_RCH_MON:
	case AFE_DL1_LCH_MON:
	case AFE_DL2_CUR_MSB:
	case AFE_DL2_CUR:
	case AFE_DL2_RCH_MON:
	case AFE_DL2_LCH_MON:
	case AFE_DL3_CUR_MSB:
	case AFE_DL3_CUR:
	case AFE_DL3_RCH_MON:
	case AFE_DL3_LCH_MON:
	case AFE_DL4_CUR_MSB:
	case AFE_DL4_CUR:
	case AFE_DL4_RCH_MON:
	case AFE_DL4_LCH_MON:
	case AFE_DL5_CUR_MSB:
	case AFE_DL5_CUR:
	case AFE_DL5_RCH_MON:
	case AFE_DL5_LCH_MON:
	case AFE_DL6_CUR_MSB:
	case AFE_DL6_CUR:
	case AFE_DL6_RCH_MON:
	case AFE_DL6_LCH_MON:
	case AFE_DL7_CUR_MSB:
	case AFE_DL7_CUR:
	case AFE_DL7_RCH_MON:
	case AFE_DL7_LCH_MON:
	case AFE_DL8_CUR_MSB:
	case AFE_DL8_CUR:
	case AFE_DL8_RCH_MON:
	case AFE_DL8_LCH_MON:
	case AFE_DL_24CH_CUR_MSB:
	case AFE_DL_24CH_CUR:
	case AFE_DL_48CH_CUR_MSB:
	case AFE_DL_48CH_CUR:
	case AFE_DL44_CUR_MSB:
	case AFE_DL44_CUR:
	case AFE_DL44_RCH_MON:
	case AFE_DL44_LCH_MON:
	case AFE_DL45_CUR_MSB:
	case AFE_DL45_CUR:
	case AFE_DL45_RCH_MON:
	case AFE_DL45_LCH_MON:
	case AFE_DL46_CUR_MSB:
	case AFE_DL46_CUR:
	case AFE_DL46_RCH_MON:
	case AFE_DL46_LCH_MON:
	case AFE_VUL0_CUR_MSB:
	case AFE_VUL0_CUR:
	case AFE_VUL1_CUR_MSB:
	case AFE_VUL1_CUR:
	case AFE_VUL2_CUR_MSB:
	case AFE_VUL2_CUR:
	case AFE_VUL3_CUR_MSB:
	case AFE_VUL3_CUR:
	case AFE_VUL4_CUR_MSB:
	case AFE_VUL4_CUR:
	case AFE_VUL5_CUR_MSB:
	case AFE_VUL5_CUR:
	case AFE_VUL6_CUR_MSB:
	case AFE_VUL6_CUR:
	case AFE_VUL7_CUR_MSB:
	case AFE_VUL7_CUR:
	case AFE_VUL8_CUR_MSB:
	case AFE_VUL8_CUR:
	case AFE_VUL9_CUR_MSB:
	case AFE_VUL9_CUR:
	case AFE_VUL10_CUR_MSB:
	case AFE_VUL10_CUR:
	case AFE_VUL37_CUR_MSB:
	case AFE_VUL37_CUR:
	case AFE_VUL38_CUR_MSB:
	case AFE_VUL38_CUR:
	case AFE_VUL38_RCH_MON:
	case AFE_VUL38_LCH_MON:
	case AFE_VUL_CM0_CUR_MSB:
	case AFE_VUL_CM0_CUR:
	case AFE_VUL_CM1_CUR_MSB:
	case AFE_VUL_CM1_CUR:
	case AFE_VUL_CM2_CUR_MSB:
	case AFE_VUL_CM2_CUR:
	case AFE_ETDM_IN1_CUR_MSB:
	case AFE_ETDM_IN1_CUR:
	case AFE_ETDM_IN2_CUR_MSB:
	case AFE_ETDM_IN2_CUR:
	case AFE_ETDM_IN6_CUR_MSB:
	case AFE_ETDM_IN6_CUR:
	case AFE_ASRC_NEW_CON0:
	case AFE_ASRC_NEW_CON6:
	case AFE_ASRC_NEW_CON8:
	case AFE_ASRC_NEW_CON9:
	case AFE_ASRC_NEW_CON12:
	case AFE_ASRC_NEW_IP_VERSION:
	case AFE_GASRC0_NEW_CON0:
	case AFE_GASRC0_NEW_CON6:
	case AFE_GASRC0_NEW_CON8:
	case AFE_GASRC0_NEW_CON9:
	case AFE_GASRC0_NEW_CON10:
	case AFE_GASRC0_NEW_CON11:
	case AFE_GASRC0_NEW_CON12:
	case AFE_GASRC0_NEW_IP_VERSION:
	case AFE_GASRC1_NEW_CON0:
	case AFE_GASRC1_NEW_CON6:
	case AFE_GASRC1_NEW_CON8:
	case AFE_GASRC1_NEW_CON9:
	case AFE_GASRC1_NEW_CON12:
	case AFE_GASRC1_NEW_IP_VERSION:
	case AFE_GASRC2_NEW_CON0:
	case AFE_GASRC2_NEW_CON6:
	case AFE_GASRC2_NEW_CON8:
	case AFE_GASRC2_NEW_CON9:
	case AFE_GASRC2_NEW_CON12:
	case AFE_GASRC2_NEW_IP_VERSION:
	case AFE_GASRC3_NEW_CON0:
	case AFE_GASRC3_NEW_CON6:
	case AFE_GASRC3_NEW_CON8:
	case AFE_GASRC3_NEW_CON9:
	case AFE_GASRC3_NEW_CON12:
	case AFE_GASRC3_NEW_IP_VERSION:
	case AFE_GAIN0_CUR_L:
	case AFE_GAIN0_CUR_R:
	case AFE_GAIN1_CUR_L:
	case AFE_GAIN1_CUR_R:
	case AFE_GAIN2_CUR_L:
	case AFE_GAIN2_CUR_R:
	case AFE_GAIN3_CUR_L:
	case AFE_GAIN3_CUR_R:
	/* these reg would change in adsp */
	case AFE_IRQ_MCU_EN:
	case AFE_IRQ_MCU_DSP_EN:
	case AFE_IRQ_MCU_DSP2_EN:
	case AFE_DL5_CON0:
	case AFE_DL6_CON0:
	case AFE_DL44_CON0:
	case AFE_DL_24CH_CON0:
	case AFE_DL_48CH_CON0:
	case AFE_VUL1_CON0:
	case AFE_VUL3_CON0:
	case AFE_VUL4_CON0:
	case AFE_VUL5_CON0:
	case AFE_VUL9_CON0:
	case AFE_VUL38_CON0:
	case AFE_IRQ0_MCU_CFG0:
	case AFE_IRQ1_MCU_CFG0:
	case AFE_IRQ2_MCU_CFG0:
	case AFE_IRQ3_MCU_CFG0:
	case AFE_IRQ4_MCU_CFG0:
	case AFE_IRQ5_MCU_CFG0:
	case AFE_IRQ6_MCU_CFG0:
	case AFE_IRQ7_MCU_CFG0:
	case AFE_IRQ8_MCU_CFG0:
	case AFE_IRQ9_MCU_CFG0:
	case AFE_IRQ10_MCU_CFG0:
	case AFE_IRQ11_MCU_CFG0:
	case AFE_IRQ12_MCU_CFG0:
	case AFE_IRQ13_MCU_CFG0:
	case AFE_IRQ14_MCU_CFG0:
	case AFE_IRQ15_MCU_CFG0:
	case AFE_IRQ16_MCU_CFG0:
	case AFE_IRQ17_MCU_CFG0:
	case AFE_IRQ18_MCU_CFG0:
	case AFE_IRQ19_MCU_CFG0:
	case AFE_IRQ20_MCU_CFG0:
	case AFE_IRQ21_MCU_CFG0:
	case AFE_IRQ22_MCU_CFG0:
	case AFE_IRQ23_MCU_CFG0:
	case AFE_IRQ24_MCU_CFG0:
	case AFE_IRQ25_MCU_CFG0:
	case AFE_IRQ26_MCU_CFG0:
	case AFE_IRQ0_MCU_CFG1:
	case AFE_IRQ1_MCU_CFG1:
	case AFE_IRQ2_MCU_CFG1:
	case AFE_IRQ3_MCU_CFG1:
	case AFE_IRQ4_MCU_CFG1:
	case AFE_IRQ5_MCU_CFG1:
	case AFE_IRQ6_MCU_CFG1:
	case AFE_IRQ7_MCU_CFG1:
	case AFE_IRQ8_MCU_CFG1:
	case AFE_IRQ9_MCU_CFG1:
	case AFE_IRQ10_MCU_CFG1:
	case AFE_IRQ11_MCU_CFG1:
	case AFE_IRQ12_MCU_CFG1:
	case AFE_IRQ13_MCU_CFG1:
	case AFE_IRQ14_MCU_CFG1:
	case AFE_IRQ15_MCU_CFG1:
	case AFE_IRQ16_MCU_CFG1:
	case AFE_IRQ17_MCU_CFG1:
	case AFE_IRQ18_MCU_CFG1:
	case AFE_IRQ19_MCU_CFG1:
	case AFE_IRQ20_MCU_CFG1:
	case AFE_IRQ21_MCU_CFG1:
	case AFE_IRQ22_MCU_CFG1:
	case AFE_IRQ23_MCU_CFG1:
	case AFE_IRQ24_MCU_CFG1:
	case AFE_IRQ25_MCU_CFG1:
	case AFE_IRQ26_MCU_CFG1:
	case AFE_CM0_CON0:
	case AFE_CM1_CON0:
	case AFE_CM2_CON0:
	//case AFE_SRAM_9100:
	//case AFE_SRAM_9200:
	/* for vow using */
	case AFE_IRQ_MCU_SCP_EN:
	case AFE_VUL_CM0_BASE_MSB:
	case AFE_VUL_CM0_BASE:
	case AFE_VUL_CM0_END_MSB:
	case AFE_VUL_CM0_END:
	case AFE_VUL_CM0_CON0:
	//case AFE_VOWIF_CFG:
	//case AFE_VOWIF_MON:
#if IS_ENABLED(CONFIG_NEBULA_SND_PASSTHROUGH)
	/* these reg would change in hypervisor */
	case AFE_DL0_CON0:
	case AFE_DL1_CON0:
	case AFE_DL2_CON0:
	case AFE_DL3_CON0:
	case AFE_DL4_CON0:
	case AFE_DL7_CON0:
	case AFE_DL8_CON0:
	case AFE_DL45_CON0:
	case AFE_DL46_CON0:
	case AFE_VUL0_CON0:
	case AFE_VUL2_CON0:
	case AFE_VUL6_CON0:
	case AFE_VUL7_CON0:
	case AFE_VUL8_CON0:
	case AFE_VUL10_CON0:
	case AFE_VUL37_CON0:
	case AFE_VUL_CM1_CON0:
	case AFE_VUL_CM2_CON0:
	case AFE_ETDM_IN1_CON0:
	case AFE_ETDM_IN2_CON0:
	case AFE_ETDM_IN6_CON0:
	case AFE_CUSTOM_IRQ0_MCU_CFG0:
	case AFE_CUSTOM_IRQ0_MCU_CFG1:
	case AFE_CUSTOM_IRQ_MCU_EN:
	case AFE_CUSTOM_IRQ_MCU_DSP_EN:
	case AFE_CUSTOM_IRQ_MCU_DSP2_EN:
#endif
		return true;
	default:
		return false;
	};
return 0;
}

static const struct regmap_config mt6881_afe_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,

	.volatile_reg = mt6881_is_volatile_reg,

	.max_register = AFE_MAX_REGISTER,
	.num_reg_defaults_raw = AFE_MAX_REGISTER,

	.cache_type = REGCACHE_FLAT,
};

#if !defined(IS_FPGA_EARLY_PORTING) || defined(FORCE_FPGA_ENABLE_IRQ)
static irqreturn_t mt6881_afe_irq_handler(int irq_id, void *dev)
{
	struct mtk_base_afe *afe = dev;
	struct mtk_base_afe_irq *irq;
	unsigned int status = 0;
	unsigned int status_mcu;
	unsigned int mcu_en = 0;
	unsigned int cus_status = 0;
	unsigned int cus_status_mcu;
	unsigned int cus_mcu_en = 0;
	unsigned int tmp_reg = 0;
	int ret, cus_ret;
	int i, j;
	struct timespec64 ts64;
	unsigned long long t1, t2;
	/* one interrupt period = 5ms */
	unsigned long long timeout_limit = 5000000;

#if !IS_ENABLED(CONFIG_NEBULA_SND_PASSTHROUGH)
	/* get irq that is sent to MCU */
	regmap_read(afe->regmap, AFE_IRQ_MCU_EN, &mcu_en);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_EN, &cus_mcu_en);

	ret = regmap_read(afe->regmap, AFE_IRQ_MCU_STATUS, &status);
	cus_ret = regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_STATUS, &cus_status);
	/* only care IRQ which is sent to MCU */
	status_mcu = status & mcu_en & AFE_IRQ_STATUS_BITS;
	cus_status_mcu = cus_status & cus_mcu_en & AFE_IRQ_STATUS_BITS;
	if ((ret || (status_mcu == 0)) &&
	    (cus_ret || (cus_status_mcu == 0))) {
		dev_info_ratelimited(
			afe->dev,
			"%s(), irq status err, ret %d, status 0x%x, mcu_en 0x%x, cus_ret %d, cus_status_mcu 0x%x, cus_mcu_en 0x%x\n",
			 __func__,
			 ret, status, mcu_en,
			 cus_ret, cus_status_mcu, cus_mcu_en);
		/* make sure all irq status are cleared for error handle to avoid burst irq */
		for (j = 0; j < 2; ++j) {
			for (i = 0; i < MT6881_IRQ_NUM; ++i) {
				regmap_read(afe->regmap, irq_data[i].irq_clr_reg, &tmp_reg);
				regmap_update_bits(afe->regmap, irq_data[i].irq_clr_reg,
						0xc0000000,
						tmp_reg^0xc0000000);
			}
		}
		return IRQ_HANDLED;
	}
#else
	struct arm_smccc_res res;
	unsigned long r7 = SMC_HYP_SECURE_ID << 16;

	arm_smccc_smc(SMC_SC_NBL_VHM_REQ, 0x90000000, hwirq, 0, 0, 0, 0, r7, &res);
	mcu_en = 1;

	status = res.a0;//regmap_read(afe->regmap, AFE_IRQ_MCU_STATUS, &status);
	/* only care IRQ which is sent to MCU */
	status_mcu = status /*& mcu_en*/ & AFE_IRQ_STATUS_BITS;

	if (status_mcu == 0) {
		dev_info(afe->dev, "%s(), irq status err, ret %d, status 0x%x, mcu_en 0x%x\n",
			__func__, ret, status, mcu_en);
		arm_smccc_smc(SMC_SC_NBL_VHM_REQ, 0x90000001, hwirq, AFE_IRQ_STATUS_BITS,
				res.a1, res.a2, 0, r7, &res);
		return IRQ_HANDLED;
	}
#endif

	ktime_get_ts64(&ts64);
	t1 = timespec64_to_ns(&ts64);

	for (i = 0; i < MT6881_MEMIF_NUM; i++) {
		struct mtk_base_afe_memif *memif = &afe->memif[i];

		if (!memif->substream)
			continue;

		if (memif->irq_usage < 0)
			continue;
		irq = &afe->irqs[memif->irq_usage];

		if (status_mcu & (0x1 << irq->irq_data->id))
			snd_pcm_period_elapsed(memif->substream);
	}

	ktime_get_ts64(&ts64);
	t2 = timespec64_to_ns(&ts64);
	t2 = t2 - t1; /* in ns (10^9) */

	if (t2 > timeout_limit) {
		dev_info(afe->dev, "%s(), mcu_en 0x%x, cus_mcu_en 0x%x, timeout %llu, limit %llu, ret %d\n",
			__func__, mcu_en, cus_mcu_en,
			t2, timeout_limit, ret);
	}

	/* clear irq */
#if !IS_ENABLED(CONFIG_NEBULA_SND_PASSTHROUGH)
	for (i = 0; i < MT6881_IRQ_NUM; ++i) {
		if ((status_mcu & (0x1 << irq_data[i].id))) {
			regmap_read(afe->regmap, irq_data[i].irq_clr_reg, &tmp_reg);
			regmap_update_bits(afe->regmap, irq_data[i].irq_clr_reg,
					0xc0000000,
					tmp_reg^0xc0000000);
		}
	}
#else
	arm_smccc_smc(SMC_SC_NBL_VHM_REQ, 0x90000001, hwirq, AFE_IRQ_STATUS_BITS, res.a1,
			res.a2, 0, r7, &res);
#endif
	return IRQ_HANDLED;
}
#endif

static int mt6881_afe_runtime_suspend(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	unsigned int value = 0;
	unsigned int tmp_reg = 0;
	int ret = 0, i;

	dev_info_ratelimited(afe->dev, "%s() ready to stop\n", __func__);

	if (!afe->regmap) {
		dev_info(afe->dev, "%s() skip regmap\n", __func__);
		goto skip_regmap;
	}

	/* Add to be off for free run*/
	/* disable AFE */
	regmap_update_bits(afe->regmap, AUDIO_ENGEN_CON0, 0x1, 0x0);

	ret = regmap_read_poll_timeout(afe->regmap,
				       AUDIO_ENGEN_CON0_MON,
				       value,
				       (value & AUDIO_ENGEN_MON_SFT) == 0,
				       20,
				       1 * 1000 * 1000);

	if (ret)
		dev_info(afe->dev, "%s(), ret %d\n", __func__, ret);

	/* make sure all irq status are cleared */
	for (i = 0; i < MT6881_IRQ_NUM; ++i) {
		regmap_read(afe->regmap, irq_data[i].irq_clr_reg, &tmp_reg);
		regmap_update_bits(afe->regmap, irq_data[i].irq_clr_reg,
				0xc0000000,
				tmp_reg^0xc0000000);
	}

	/* reset sgen */
	regmap_write(afe->regmap, AFE_SINEGEN_CON0, 0x0);
	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
			   SINE_DOMAIN_MASK_SFT,
			   0x0 << SINE_DOMAIN_SFT);
	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
			   SINE_MODE_MASK_SFT,
			   0x0 << SINE_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
			   INNER_LOOP_BACKI_SEL_MASK_SFT,
			   0x0 << INNER_LOOP_BACKI_SEL_SFT);
	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
			   INNER_LOOP_BACK_MODE_MASK_SFT,
			   0xff << INNER_LOOP_BACK_MODE_SFT);

	regmap_write(afe->regmap, AUDIO_TOP_CON4, 0x7fff);

	/* reset audio 26M request */
	regmap_update_bits(afe->regmap, AFE_SPM_CONTROL_REQ,
			   AFE_SRCCLKENA_REQ_MASK_SFT, 0x0 << AFE_SRCCLKENA_REQ_SFT);

	/* cache only */
	regcache_cache_only(afe->regmap, true);
	regcache_mark_dirty(afe->regmap);

skip_regmap:
	mt6881_afe_sram_release(afe);
	mt6881_afe_disable_clock(afe);
	return 0;
}

static int mt6881_afe_runtime_resume(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	int ret = 0;

	ret = mt6881_afe_enable_clock(afe);
	dev_info_ratelimited(afe->dev, "%s(), enable_clock ret %d\n", __func__, ret);

	if (ret)
		return ret;
	mt6881_afe_sram_request(afe);

	if (!afe->regmap) {
		dev_info(afe->dev, "%s() skip regmap\n", __func__);
		goto skip_regmap;
	}
	regcache_cache_only(afe->regmap, false);
	regcache_sync(afe->regmap);

	/* set audio 26M request */
	regmap_update_bits(afe->regmap, AFE_SPM_CONTROL_REQ,
			   AFE_SRCCLKENA_REQ_MASK_SFT, 0x1 << AFE_SRCCLKENA_REQ_SFT);

	/* IPM2.0: Clear AUDIO_TOP_CON4 for enabling AP side module clk */
	regmap_write(afe->regmap, AUDIO_TOP_CON4, 0x0);

	/* Add to be on for free run */
	regmap_write(afe->regmap, AUDIO_TOP_CON0, 0x0);
	regmap_write(afe->regmap, AUDIO_TOP_CON1, 0x0);
	regmap_write(afe->regmap, AUDIO_TOP_CON2, 0x0);
	regmap_write(afe->regmap, AUDIO_TOP_CON5, 0x0);

#if !defined(IS_FPGA_EARLY_PORTING)
	/* Can't set AUDIO_TOP_CON3 to be 0x0, it will hang in FPGA env */
	regmap_write(afe->regmap, AUDIO_TOP_CON3, 0x0);
#endif

	regmap_update_bits(afe->regmap, AFE_CBIP_CFG0, 0x1, 0x1);

	/* force cpu use 8_24 format when writing 32bit data */
	regmap_update_bits(afe->regmap, AFE_MEMIF_CON0,
			   CPU_HD_ALIGN_MASK_SFT, 0 << CPU_HD_ALIGN_SFT);

	/* enable AFE */
	regmap_update_bits(afe->regmap, AUDIO_ENGEN_CON0, 0x1, 0x1);

skip_regmap:
	return 0;
}

static int mt6881_afe_pcm_copy(struct snd_pcm_substream *substream,
			       int channel, unsigned long hwoff,
			       struct iov_iter *buf, unsigned long bytes,
			       mtk_sp_copy_f sp_copy)
{
	int ret = 0;

	ret = sp_copy(substream, channel, hwoff, buf, bytes);

	return ret;
}

static int mt6881_set_memif_sram_mode(struct device *dev,
				      enum mtk_audio_sram_mode sram_mode)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	int reg_bit = sram_mode == MTK_AUDIO_SRAM_NORMAL_MODE ? 1 : 0;

	regmap_update_bits(afe->regmap, AFE_DL0_CON0,
			   DL0_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL0_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL1_CON0,
			   DL1_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL1_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL2_CON0,
			   DL2_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL2_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL3_CON0,
			   DL3_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL3_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL4_CON0,
			   DL4_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL4_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL5_CON0,
			   DL5_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL5_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL6_CON0,
			   DL6_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL6_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL7_CON0,
			   DL7_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL7_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL8_CON0,
			   DL8_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL8_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL44_CON0,
			   DL44_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL44_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL45_CON0,
			   DL45_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL45_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL46_CON0,
			   DL46_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL46_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL_24CH_CON0,
			   DL_24CH_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL_24CH_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_DL_48CH_CON0,
			   DL_48CH_NORMAL_MODE_MASK_SFT,
			   reg_bit << DL_48CH_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL0_CON0,
			   VUL0_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL0_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL1_CON0,
			   VUL1_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL1_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL2_CON0,
			   VUL2_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL2_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL3_CON0,
			   VUL3_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL3_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL4_CON0,
			   VUL4_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL4_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL5_CON0,
			   VUL5_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL5_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL6_CON0,
			   VUL6_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL6_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL7_CON0,
			   VUL7_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL7_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL8_CON0,
			   VUL8_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL8_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL9_CON0,
			   VUL9_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL9_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL10_CON0,
			   VUL10_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL10_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL37_CON0,
			   VUL37_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL37_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL38_CON0,
			   VUL38_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL38_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL_CM0_CON0,
			   VUL_CM0_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL_CM0_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL_CM1_CON0,
			   VUL_CM1_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL_CM1_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_VUL_CM2_CON0,
			   VUL_CM2_NORMAL_MODE_MASK_SFT,
			   reg_bit << VUL_CM2_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_ETDM_IN1_CON0,
			   ETDM_IN1_NORMAL_MODE_MASK_SFT,
			   reg_bit << ETDM_IN1_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_ETDM_IN2_CON0,
			   ETDM_IN2_NORMAL_MODE_MASK_SFT,
			   reg_bit << ETDM_IN2_NORMAL_MODE_SFT);
	regmap_update_bits(afe->regmap, AFE_ETDM_IN6_CON0,
			   ETDM_IN6_NORMAL_MODE_MASK_SFT,
			   reg_bit << ETDM_IN6_NORMAL_MODE_SFT);
	return 0;
}

static int mt6881_set_sram_mode(struct device *dev,
				enum mtk_audio_sram_mode sram_mode)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);

	/* set memif sram mode */
	mt6881_set_memif_sram_mode(dev, sram_mode);

	if (sram_mode == MTK_AUDIO_SRAM_COMPACT_MODE)
		/* cpu use compact mode when access sram data */
		regmap_update_bits(afe->regmap, AFE_MEMIF_CON0,
				   CPU_COMPACT_MODE_MASK_SFT,
				   0x1 << CPU_COMPACT_MODE_SFT);
	else
		/* cpu use normal mode when access sram data */
		regmap_update_bits(afe->regmap, AFE_MEMIF_CON0,
				   CPU_COMPACT_MODE_MASK_SFT,
				   0x0 << CPU_COMPACT_MODE_SFT);

	return 0;
}

static const struct mtk_audio_sram_ops mt6881_sram_ops = {
	.set_sram_mode = mt6881_set_sram_mode,
};

static u32 copy_from_buffer_request(void *dest, size_t destsize, const void *src,
				    size_t srcsize, u32 offset, size_t request)
{
	/* if request == -1, offset == 0, copy full srcsize */
	if (offset + request > srcsize)
		request = srcsize - offset;

	/* if destsize == -1, don't check the request size */
	if (!dest || destsize < request) {
		pr_info("%s, buffer null or not enough space", __func__);
		return 0;
	}

	memcpy(dest, src + offset, request);
	return request;
}

/*
 * sysfs bin_attribute node
 */

static ssize_t afe_sysfs_debug_read(struct file *filep, struct kobject *kobj,
				    struct bin_attribute *attr,
				    char *buf, loff_t offset, size_t size)
{
	size_t read_size, ceil_size, page_mask;
	ssize_t ret;
	struct mtk_base_afe *afe = (struct mtk_base_afe *)attr->private;
	char *buffer = NULL; /* for reduce kernel stack */

	buffer = kmalloc(AFE_SYS_DEBUG_SIZE, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	// sys fs op align with page size
	read_size = mt6881_debug_read_reg(buffer, AFE_SYS_DEBUG_SIZE, afe);
	page_mask = ~(PAGE_SIZE-1);
	ceil_size = (read_size&page_mask) + PAGE_SIZE;

	ret = copy_from_buffer_request(buf, -1, buffer, ceil_size, offset, size);
	kfree(buffer);

	return ret;
}

/*
 * sysfs bin_attribute node
 */
static ssize_t afe_sysfs_debug_write(struct file *filep, struct kobject *kobj,
				     struct bin_attribute *attr,
				     char *buf, loff_t offset, size_t size)
{
	struct mtk_base_afe *afe = (struct mtk_base_afe *)attr->private;

	char input[MAX_DEBUG_WRITE_INPUT];
	char *temp, *command, *str_begin;
	char delim[] = " ,";

	if (!size) {
		dev_info(afe->dev, "%s(), count is 0, return directly\n",
			 __func__);
		goto exit;
	}

	if (size >= MAX_DEBUG_WRITE_INPUT)
		size = MAX_DEBUG_WRITE_INPUT - 1;

	memset((void *)input, 0, MAX_DEBUG_WRITE_INPUT);
	memcpy(input, buf, size);
	input[(int)size] = '\0';

	str_begin = kstrndup(input, MAX_DEBUG_WRITE_INPUT - 1,
			     GFP_KERNEL);

	if (!str_begin) {
		dev_info(afe->dev, "%s(), kstrdup fail\n", __func__);
		goto exit;
	}
	temp = str_begin;

	command = strsep(&temp, delim);

	if (strcmp("write_reg", command) == 0)
		mtk_afe_write_reg(afe, (void *)temp);
exit:

	return size;
}

struct bin_attribute bin_attr_afe_dump = {
	.attr = {
		.name = "mtk_afe_node",
		.mode = 0444,
	},
	.size = AFE_SYS_DEBUG_SIZE,
	.read = afe_sysfs_debug_read,
	.write = afe_sysfs_debug_write,
};

static struct bin_attribute *afe_bin_attrs[] = {
	&bin_attr_afe_dump,
	NULL,
};

struct attribute_group afe_bin_attr_group = {
	.name = "mtk_afe_attrs",
	.bin_attrs = afe_bin_attrs,
};


static int mt6881_afe_component_probe(struct snd_soc_component *component)
{
	struct mtk_base_afe *afe = NULL;
	struct snd_soc_card *sndcard = NULL;
	struct snd_card *card = NULL;
	int ret = 0;

	if (component) {
		afe = snd_soc_component_get_drvdata(component);
		sndcard = component->card;
		card = sndcard->snd_card;

		mtk_afe_add_sub_dai_control(component);
		mt6881_add_misc_control(component);

		bin_attr_afe_dump.private = (void *)afe;
		ret = snd_card_add_dev_attr(card, &afe_bin_attr_group);
		if (ret)
			pr_info("snd_card_add_dev_attr fail\n");
	}

	return 0;
}

static const struct snd_soc_component_driver mt6881_afe_component = {
	.name = AFE_PCM_NAME,
	.probe = mt6881_afe_component_probe,
	.pcm_construct = mtk_afe_pcm_new,
	.pcm_destruct = mtk_afe_pcm_free,
	.open = mtk_afe_pcm_open,
	.pointer = mtk_afe_pcm_pointer,
	.copy = mtk_afe_pcm_copy_user,
};

static ssize_t mt6881_debug_read_reg(char *buffer, int size, struct mtk_base_afe *afe)
{
	int n = 0, i = 0;
	unsigned int value;
	struct mt6881_afe_private *afe_priv = afe->platform_priv;

	if (!buffer)
		return -ENOMEM;

	n += scnprintf(buffer + n, size - n,
		       "mtkaif calibration phase %d, %d, %d, %d\n",
		       afe_priv->mtkaif_chosen_phase[0],
		       afe_priv->mtkaif_chosen_phase[1],
		       afe_priv->mtkaif_chosen_phase[2],
		       afe_priv->mtkaif_chosen_phase[3]);

	n += scnprintf(buffer + n, size - n,
		       "mtkaif calibration cycle %d, %d, %d, %d\n",
		       afe_priv->mtkaif_phase_cycle[0],
		       afe_priv->mtkaif_phase_cycle[1],
		       afe_priv->mtkaif_phase_cycle[2],
		       afe_priv->mtkaif_phase_cycle[3]);

	for (i = 0; i < afe->memif_size; i++) {
		n += scnprintf(buffer + n, size - n,
			       "memif[%d], irq_usage %d\n",
			       i, afe->memif[i].irq_usage);
	}
#if !defined(IS_FPGA_EARLY_PORTING) && !defined(SKIP_SB_CLK)
	regmap_read(afe_priv->topckgen, CLK_CFG_5, &value);
	n += scnprintf(buffer + n, size - n,
		"CLK_CFG_5 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_CFG_6, &value);
	n += scnprintf(buffer + n, size - n,
		"CLK_CFG_6 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_CFG_7, &value);
	n += scnprintf(buffer + n, size - n,
		"CLK_CFG_7 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_CFG_8, &value);
	n += scnprintf(buffer + n, size - n,
		"CLK_CFG_8 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_CFG_9, &value);
	n += scnprintf(buffer + n, size - n,
		"CLK_CFG_9 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_CFG_10, &value);
	n += scnprintf(buffer + n, size - n,
		"CLK_CFG_10 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_CFG_11, &value);
	n += scnprintf(buffer + n, size - n,
		"CLK_CFG_11 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_CFG_12, &value);
	n += scnprintf(buffer + n, size - n,
		"CLK_CFG_12 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_CFG_13, &value);
	n += scnprintf(buffer + n, size - n,
		"CLK_CFG_13 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_CFG_UPDATE, &value);
	n += scnprintf(buffer + n, size - n,
		"CLK_CFG_UPDATE = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_CFG_UPDATE1, &value);
	n += scnprintf(buffer + n, size - n,
		"CLK_CFG_UPDATE1 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_0, &value);
	n += scnprintf(buffer + n, size - n,
		"CLK_AUDDIV_0 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_1, &value);
	n += scnprintf(buffer + n, size - n,
		"CLK_AUDDIV_1 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_2, &value);
	n += scnprintf(buffer + n, size - n,
		"CLK_AUDDIV_2 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_3, &value);
	n += scnprintf(buffer + n, size - n,
		"CLK_AUDDIV_3 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_4, &value);
	n += scnprintf(buffer + n, size - n,
		"CLK_AUDDIV_4 = 0x%x\n", value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_5, &value);
	n += scnprintf(buffer + n, size - n,
		"CLK_AUDDIV_5 = 0x%x\n", value);

	regmap_read(afe_priv->apmixed, AP_PLL_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AP_PLL_CON3 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, PLLEN_ALL, &value);
	n += scnprintf(buffer + n, size - n,
		"PLLEN_ALL = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, PLLEN_ALL_SET, &value);
	n += scnprintf(buffer + n, size - n,
		"PLLEN_ALL_SET = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, PLLEN_ALL_CLR, &value);
	n += scnprintf(buffer + n, size - n,
		"PLLEN_ALL_CLR = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL1_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"APLL1_CON1 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL1_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"APLL1_CON2 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL1_TUNER_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"APLL1_TUNER_CON0 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL2_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"APLL2_CON1 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL2_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"APLL2_CON2 = 0x%x\n", value);
	regmap_read(afe_priv->apmixed, APLL2_TUNER_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"APLL2_TUNER_CON0 = 0x%x\n", value);
#endif
	regmap_read(afe->regmap, AUDIO_TOP_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_ENGEN_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_ENGEN_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_ENGEN_CON0_USER1, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_ENGEN_CON0_USER1 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_ENGEN_CON0_USER2, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_ENGEN_CON0_USER2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SINEGEN_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SINEGEN_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SINEGEN_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SINEGEN_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SINEGEN_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SINEGEN_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SINEGEN_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SINEGEN_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_APLL1_TUNER_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_APLL1_TUNER_CFG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_APLL1_TUNER_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_APLL1_TUNER_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_APLL2_TUNER_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_APLL2_TUNER_CFG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_APLL2_TUNER_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_APLL2_TUNER_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_RG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_RG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_RG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_RG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_RG2, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_RG2 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_RG3, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_RG3 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_RG4, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_RG4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SPM_CONTROL_REQ, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SPM_CONTROL_REQ = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SPM_CONTROL_ACK, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SPM_CONTROL_ACK = 0x%x\n", value);
	regmap_read(afe->regmap, AUD_TOP_CFG_VCORE_RG, &value);
	n += scnprintf(buffer + n, size - n,
		"AUD_TOP_CFG_VCORE_RG = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_TOP_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_ENGEN_CON0_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_ENGEN_CON0_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_PROJECT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_PROJECT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AUD_TOP_CFG_VLP_RG, &value);
	n += scnprintf(buffer + n, size - n,
		"AUD_TOP_CFG_VLP_RG = 0x%x\n", value);
	regmap_read(afe->regmap, AUD_TOP_MON_RG, &value);
	n += scnprintf(buffer + n, size - n,
		"AUD_TOP_MON_RG = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_USE_DEFAULT_DELSEL0, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_USE_DEFAULT_DELSEL0 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_USE_DEFAULT_DELSEL1, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_USE_DEFAULT_DELSEL1 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_USE_DEFAULT_DELSEL2, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_USE_DEFAULT_DELSEL2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONNSYS_I2S_IPM_VER_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONNSYS_I2S_IPM_VER_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONNSYS_I2S_MON_SEL, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONNSYS_I2S_MON_SEL = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONNSYS_I2S_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONNSYS_I2S_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONNSYS_I2S_CON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONNSYS_I2S_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_PCM0_INTF_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_PCM0_INTF_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_PCM0_INTF_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_PCM0_INTF_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_PCM_INTF_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_PCM_INTF_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_PCM1_INTF_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_PCM1_INTF_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_PCM1_INTF_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_PCM1_INTF_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_PCM_TOP_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_PCM_TOP_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN0_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN0_CON1_R, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN0_CON1_R = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN0_CON1_L, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN0_CON1_L = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN0_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN0_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN0_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN0_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN0_CUR_R, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN0_CUR_R = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN0_CUR_L, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN0_CUR_L = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CON1_R, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN1_CON1_R = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CON1_L, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN1_CON1_L = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN1_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN1_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CUR_R, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN1_CUR_R = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CUR_L, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN1_CUR_L = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CON1_R, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN2_CON1_R = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CON1_L, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN2_CON1_L = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN2_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN2_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CUR_R, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN2_CUR_R = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CUR_L, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN2_CUR_L = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN3_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN3_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN3_CON1_R, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN3_CON1_R = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN3_CON1_L, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN3_CON1_L = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN3_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN3_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN3_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN3_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN3_CUR_R, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN3_CUR_R = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN3_CUR_L, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN3_CUR_L = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN_0_1_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN_0_1_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN_2_3_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GAIN_2_3_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_IPM_VER_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_IPM_VER_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_SRC_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_SRC_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC_DEBUG_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_SRC_DEBUG_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_PREDIS_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_PREDIS_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_PREDIS_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_PREDIS_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_PREDIS_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_PREDIS_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_PREDIS_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_PREDIS_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_DCCOMP_CON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_SDM_DCCOMP_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_TEST, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_SDM_TEST = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_DC_COMP_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_DC_COMP_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_DC_COMP_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_DC_COMP_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_OUT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_SDM_OUT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_SRC_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_SRC_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC_DEBUG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_SRC_DEBUG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_DITHER_CON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_SDM_DITHER_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_AUTO_RESET_CON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_SDM_AUTO_RESET_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP1_TAP2_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP1_TAP2_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP3_TAP4_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP3_TAP4_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP5_TAP6_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP5_TAP6_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP7_TAP8_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP7_TAP8_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP9_TAP10_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP9_TAP10_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP11_TAP12_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP11_TAP12_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP13_TAP14_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP13_TAP14_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP15_TAP16_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP15_TAP16_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP17_TAP18_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP17_TAP18_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP19_TAP20_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP19_TAP20_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP21_TAP22_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP21_TAP22_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP23_TAP24_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP23_TAP24_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP25_TAP26_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP25_TAP26_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP27_TAP28_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP27_TAP28_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP29_TAP30_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP29_TAP30_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP31_TAP32_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP31_TAP32_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP33_TAP34_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP33_TAP34_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP35_TAP36_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP35_TAP36_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP37_TAP38_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP37_TAP38_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP39_TAP40_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP39_TAP40_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP41_TAP42_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP41_TAP42_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP43_TAP44_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP43_TAP44_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP45_TAP46_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP45_TAP46_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP47_TAP48_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP47_TAP48_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP49_TAP50_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP49_TAP50_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP51_TAP52_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP51_TAP52_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP53_TAP54_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP53_TAP54_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_HBF1_SCF1_TAP55_TAP56_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_HBF1_SCF1_TAP55_TAP56_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_NLE_R_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_NLE_R_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_NLE_R_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_NLE_R_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_NLE_L_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_NLE_L_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_NLE_L_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_NLE_L_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_NLE_R_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_NLE_R_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_NLE_R_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_NLE_R_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_NLE_R_MON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_NLE_R_MON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_NLE_L_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_NLE_L_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_NLE_L_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_NLE_L_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_NLE_L_MON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_NLE_L_MON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_NLE_GAIN_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_NLE_GAIN_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_SRC_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_SRC_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_SRC_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_SRC_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_SRC_DEBUG_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_SRC_DEBUG_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_PREDIS_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_PREDIS_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_PREDIS_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_PREDIS_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_PREDIS_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_PREDIS_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_PREDIS_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_PREDIS_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_SDM_DCCOMP_CON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_SDM_DCCOMP_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_SDM_TEST, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_SDM_TEST = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_DC_COMP_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_DC_COMP_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_DC_COMP_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_DC_COMP_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_SRC_DEBUG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_SRC_DEBUG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_SRC_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_SRC_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_SRC_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_SRC_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_SDM_OUT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_SDM_OUT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_SDM_DITHER_CON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_SDM_DITHER_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_SDM_AUTO_RESET_CON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_SDM_AUTO_RESET_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP1_TAP2_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP1_TAP2_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP3_TAP4_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP3_TAP4_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP5_TAP6_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP5_TAP6_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP7_TAP8_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP7_TAP8_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP9_TAP10_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP9_TAP10_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP11_TAP12_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP11_TAP12_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP13_TAP14_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP13_TAP14_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP15_TAP16_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP15_TAP16_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP17_TAP18_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP17_TAP18_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP19_TAP20_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP19_TAP20_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP21_TAP22_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP21_TAP22_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP23_TAP24_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP23_TAP24_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP25_TAP26_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP25_TAP26_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP27_TAP28_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP27_TAP28_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP29_TAP30_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP29_TAP30_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP31_TAP32_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP31_TAP32_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP33_TAP34_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP33_TAP34_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP35_TAP36_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP35_TAP36_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP37_TAP38_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP37_TAP38_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP39_TAP40_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP39_TAP40_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP41_TAP42_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP41_TAP42_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP43_TAP44_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP43_TAP44_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP45_TAP46_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP45_TAP46_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP47_TAP48_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP47_TAP48_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP49_TAP50_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP49_TAP50_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP51_TAP52_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP51_TAP52_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP53_TAP54_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP53_TAP54_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_2ND_DL_HBF1_SCF1_TAP55_TAP56_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_2ND_DL_HBF1_SCF1_TAP55_TAP56_CONFIG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DEM_IDWA_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DEM_IDWA_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, DEM_RECONSTRUCT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"DEM_RECONSTRUCT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_STF_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_STF_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_STF_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_STF_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_STF_COEFF, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_STF_COEFF = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_STF_GAIN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_STF_GAIN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_STF_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_STF_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_STF_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_STF_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CM0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CM0_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CM0_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CM0_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CM0_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CM0_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CM1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CM1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CM1_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CM1_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CM1_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CM1_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CM2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CM2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CM2_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CM2_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CM2_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CM2_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_SRC_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_SRC_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_SRC_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_DEBUG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_SRC_DEBUG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_DEBUG_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_SRC_DEBUG_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_SRC_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_SRC_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IIR_COEF_02_01, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_IIR_COEF_02_01 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IIR_COEF_04_03, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_IIR_COEF_04_03 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IIR_COEF_06_05, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_IIR_COEF_06_05 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IIR_COEF_08_07, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_IIR_COEF_08_07 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IIR_COEF_10_09, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_IIR_COEF_10_09 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_02_01, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_02_01 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_04_03, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_04_03 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_06_05, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_06_05 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_08_07, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_08_07 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_10_09, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_10_09 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_12_11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_12_11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_14_13, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_14_13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_16_15, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_16_15 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_18_17, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_18_17 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_20_19, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_20_19 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_22_21, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_22_21 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_24_23, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_24_23 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_26_25, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_26_25 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_28_27, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_28_27 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_30_29, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_30_29 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_ULCF_CFG_32_31, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_ULCF_CFG_32_31 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL0_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL0_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_SRC_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_SRC_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_SRC_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_SRC_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_SRC_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_SRC_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_SRC_DEBUG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_SRC_DEBUG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_SRC_DEBUG_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_SRC_DEBUG_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_SRC_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_SRC_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_SRC_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_SRC_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_IIR_COEF_02_01, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_IIR_COEF_02_01 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_IIR_COEF_04_03, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_IIR_COEF_04_03 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_IIR_COEF_06_05, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_IIR_COEF_06_05 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_IIR_COEF_08_07, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_IIR_COEF_08_07 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_IIR_COEF_10_09, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_IIR_COEF_10_09 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_02_01, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_02_01 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_04_03, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_04_03 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_06_05, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_06_05 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_08_07, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_08_07 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_10_09, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_10_09 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_12_11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_12_11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_14_13, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_14_13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_16_15, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_16_15 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_18_17, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_18_17 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_20_19, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_20_19 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_22_21, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_22_21 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_24_23, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_24_23 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_26_25, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_26_25 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_28_27, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_28_27 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_30_29, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_30_29 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_ULCF_CFG_32_31, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_ULCF_CFG_32_31 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL1_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_UL1_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_PROXIMITY_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_PROXIMITY_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_ULSRC_PHASE_CLK_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_ULSRC_PHASE_CLK_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_ULSRC_PHASE_CLK_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_ULSRC_PHASE_CLK_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_ULSRC_PHASE_CLK_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_ULSRC_PHASE_CLK_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_ULSRC_PHASE_CLK_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_ULSRC_PHASE_CLK_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_ULSRC_PHASE_CLK_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_ULSRC_PHASE_CLK_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_ULSRC_PHASE_ENGEN_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_ULSRC_PHASE_ENGEN_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_ULSRC_PHASE_ENGEN_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_ULSRC_PHASE_ENGEN_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_ULSRC_PHASE_RST_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_ULSRC_PHASE_RST_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF_IPM_VER_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF_IPM_VER_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF_MON_SEL, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF_MON_SEL = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF0_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF0_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF0_TX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF0_TX_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF0_RX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF0_RX_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF0_RX_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF0_RX_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF0_RX_CFG2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF0_RX_CFG2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF1_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF1_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF1_TX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF1_TX_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF1_RX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF1_RX_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF1_RX_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF1_RX_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MTKAIF1_RX_CFG2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MTKAIF1_RX_CFG2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AUD_PAD_TOP_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_AUD_PAD_TOP_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AUD_PAD_TOP_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_AUD_PAD_TOP_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIFV4_TX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_MTKAIFV4_TX_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_MTKAIFV4_TX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_MTKAIFV4_TX_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIFV4_RX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_MTKAIFV4_RX_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIFV4_RX_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_MTKAIFV4_RX_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_MTKAIFV4_RX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_MTKAIFV4_RX_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_MTKAIFV4_RX_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_MTKAIFV4_RX_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIFV4_TX_SYNCWORD_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_MTKAIFV4_TX_SYNCWORD_CFG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIFV4_RX_SYNCWORD_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_MTKAIFV4_RX_SYNCWORD_CFG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIFV4_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_MTKAIFV4_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIFV4_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_MTKAIFV4_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA6_MTKAIFV4_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_MTKAIFV4_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN0_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN0_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN0_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN0_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN0_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN0_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN0_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN0_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN0_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN0_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN0_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN0_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN0_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN0_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN0_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN0_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN0_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN0_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN0_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN0_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN0_MON = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN1_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN1_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN1_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN1_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN1_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN1_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN1_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN1_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN1_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN1_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN1_MON = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN2_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN2_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN2_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN2_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN2_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN2_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN2_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN2_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN2_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN2_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN2_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN2_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN2_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN2_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN2_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN2_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN2_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN2_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN2_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN2_MON = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN6_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN6_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN6_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN6_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN6_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN6_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN6_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN6_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN6_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN6_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN6_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN6_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN6_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN6_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN6_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN6_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN6_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN6_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN6_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN6_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN6_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN6_MON = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT0_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT0_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT0_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT0_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT0_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT0_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT0_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT0_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT0_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT0_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT0_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT0_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT0_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT0_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT0_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT0_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT0_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT0_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT0_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT0_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT0_MON = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT1_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT1_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT1_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT1_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT1_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT1_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT1_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT1_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT1_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT1_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT1_MON = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT2_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT2_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT2_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT2_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT2_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT2_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT2_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT2_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT2_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT2_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT2_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT2_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT2_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT2_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT2_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT2_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT2_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT2_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT2_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT2_MON = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT6_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT6_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT6_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT6_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT6_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT6_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT6_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT6_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT6_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT6_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT6_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT6_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT6_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT6_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT6_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT6_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT6_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT6_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT6_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT6_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_OUT6_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_OUT6_MON = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_0_3_COWORK_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_0_3_COWORK_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_0_3_COWORK_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_0_3_COWORK_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_0_3_COWORK_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_0_3_COWORK_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_0_3_COWORK_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_0_3_COWORK_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_4_7_COWORK_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_4_7_COWORK_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_4_7_COWORK_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_4_7_COWORK_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_4_7_COWORK_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_4_7_COWORK_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_4_7_COWORK_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_4_7_COWORK_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM67_PADTOP_CON, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM67_PADTOP_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN004_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN004_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN004_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN004_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN004_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN004_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN004_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN004_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN004_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN004_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN004_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN004_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN004_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN004_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN005_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN005_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN005_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN005_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN005_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN005_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN005_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN005_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN005_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN005_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN005_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN005_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN005_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN005_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN006_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN006_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN006_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN006_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN006_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN006_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN006_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN006_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN006_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN006_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN006_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN006_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN006_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN006_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN007_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN007_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN007_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN007_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN007_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN007_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN007_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN007_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN007_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN007_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN007_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN007_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN007_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN007_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN008_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN008_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN008_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN008_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN008_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN008_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN008_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN008_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN008_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN008_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN008_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN008_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN008_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN008_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN009_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN009_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN009_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN009_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN009_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN009_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN009_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN009_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN009_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN009_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN009_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN009_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN009_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN009_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN010_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN010_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN010_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN010_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN010_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN010_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN010_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN010_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN010_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN010_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN010_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN010_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN010_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN010_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN011_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN011_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN011_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN011_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN011_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN011_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN011_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN011_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN011_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN011_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN011_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN011_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN011_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN011_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN012_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN012_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN012_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN012_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN012_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN012_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN012_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN012_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN012_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN012_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN012_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN012_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN012_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN012_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN014_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN014_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN014_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN014_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN014_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN014_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN014_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN014_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN014_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN014_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN014_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN014_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN014_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN014_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN015_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN015_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN015_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN015_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN015_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN015_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN015_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN015_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN015_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN015_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN015_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN015_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN015_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN015_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN016_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN016_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN016_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN016_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN016_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN016_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN016_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN016_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN016_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN016_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN016_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN016_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN016_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN016_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN017_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN017_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN017_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN017_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN017_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN017_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN017_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN017_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN017_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN017_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN017_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN017_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN017_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN017_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN018_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN018_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN018_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN018_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN018_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN018_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN018_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN018_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN018_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN018_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN018_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN018_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN018_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN018_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN019_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN019_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN019_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN019_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN019_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN019_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN019_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN019_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN019_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN019_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN019_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN019_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN019_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN019_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN020_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN020_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN020_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN020_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN020_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN020_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN020_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN020_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN020_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN020_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN020_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN020_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN020_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN020_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN021_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN021_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN021_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN021_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN021_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN021_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN021_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN021_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN021_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN021_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN021_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN021_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN021_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN021_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN022_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN022_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN022_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN022_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN022_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN022_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN022_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN022_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN022_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN022_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN022_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN022_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN022_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN022_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN023_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN023_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN023_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN023_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN023_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN023_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN023_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN023_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN023_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN023_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN023_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN023_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN023_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN023_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN024_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN024_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN024_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN024_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN024_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN024_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN024_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN024_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN024_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN024_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN024_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN024_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN024_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN024_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN025_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN025_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN025_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN025_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN025_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN025_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN025_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN025_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN025_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN025_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN025_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN025_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN025_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN025_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN026_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN026_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN026_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN026_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN026_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN026_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN026_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN026_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN026_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN026_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN026_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN026_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN026_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN026_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN027_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN027_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN027_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN027_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN027_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN027_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN027_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN027_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN027_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN027_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN027_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN027_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN027_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN027_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN028_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN028_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN028_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN028_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN028_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN028_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN028_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN028_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN028_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN028_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN028_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN028_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN028_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN028_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN029_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN029_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN029_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN029_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN029_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN029_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN029_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN029_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN029_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN029_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN029_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN029_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN029_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN029_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN030_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN030_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN030_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN030_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN030_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN030_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN030_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN030_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN030_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN030_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN030_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN030_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN030_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN030_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN031_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN031_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN031_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN031_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN031_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN031_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN031_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN031_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN031_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN031_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN031_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN031_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN031_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN031_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN032_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN032_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN032_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN032_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN032_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN032_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN032_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN032_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN032_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN032_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN032_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN032_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN032_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN032_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN033_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN033_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN033_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN033_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN033_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN033_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN033_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN033_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN033_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN033_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN033_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN033_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN033_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN033_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN034_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN034_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN034_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN034_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN034_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN034_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN034_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN034_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN034_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN034_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN034_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN034_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN034_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN034_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN035_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN035_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN035_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN035_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN035_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN035_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN035_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN035_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN035_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN035_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN035_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN035_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN035_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN035_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN036_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN036_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN036_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN036_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN036_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN036_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN036_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN036_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN036_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN036_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN036_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN036_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN036_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN036_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN037_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN037_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN037_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN037_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN037_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN037_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN037_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN037_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN037_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN037_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN037_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN037_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN037_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN037_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN038_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN038_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN038_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN038_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN038_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN038_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN038_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN038_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN038_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN038_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN038_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN038_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN038_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN038_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN039_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN039_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN039_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN039_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN039_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN039_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN039_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN039_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN039_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN039_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN039_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN039_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN039_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN039_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN040_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN040_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN040_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN040_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN040_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN040_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN040_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN040_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN040_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN040_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN040_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN040_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN040_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN040_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN041_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN041_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN041_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN041_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN041_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN041_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN041_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN041_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN041_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN041_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN041_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN041_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN041_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN041_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN042_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN042_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN042_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN042_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN042_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN042_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN042_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN042_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN042_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN042_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN042_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN042_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN042_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN042_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN043_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN043_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN043_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN043_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN043_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN043_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN043_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN043_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN043_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN043_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN043_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN043_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN043_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN043_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN044_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN044_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN044_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN044_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN044_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN044_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN044_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN044_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN044_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN044_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN044_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN044_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN044_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN044_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN045_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN045_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN045_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN045_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN045_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN045_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN045_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN045_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN045_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN045_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN045_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN045_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN045_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN045_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN046_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN046_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN046_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN046_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN046_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN046_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN046_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN046_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN046_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN046_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN046_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN046_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN046_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN046_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN047_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN047_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN047_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN047_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN047_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN047_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN047_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN047_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN047_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN047_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN047_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN047_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN047_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN047_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN048_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN048_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN048_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN048_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN048_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN048_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN048_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN048_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN048_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN048_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN048_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN048_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN048_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN048_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN049_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN049_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN049_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN049_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN049_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN049_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN049_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN049_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN049_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN049_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN049_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN049_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN049_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN049_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN050_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN050_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN050_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN050_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN050_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN050_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN050_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN050_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN050_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN050_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN050_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN050_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN050_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN050_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN051_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN051_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN051_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN051_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN051_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN051_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN051_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN051_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN051_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN051_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN051_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN051_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN051_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN051_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN052_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN052_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN052_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN052_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN052_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN052_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN052_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN052_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN052_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN052_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN052_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN052_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN052_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN052_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN053_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN053_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN053_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN053_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN053_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN053_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN053_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN053_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN053_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN053_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN053_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN053_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN053_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN053_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN054_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN054_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN054_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN054_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN054_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN054_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN054_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN054_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN054_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN054_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN054_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN054_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN054_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN054_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN055_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN055_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN055_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN055_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN055_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN055_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN055_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN055_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN055_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN055_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN055_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN055_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN055_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN055_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN056_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN056_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN056_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN056_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN056_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN056_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN056_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN056_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN056_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN056_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN056_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN056_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN056_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN056_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN057_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN057_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN057_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN057_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN057_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN057_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN057_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN057_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN057_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN057_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN057_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN057_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN057_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN057_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN058_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN058_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN058_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN058_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN058_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN058_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN058_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN058_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN058_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN058_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN058_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN058_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN058_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN058_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN059_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN059_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN059_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN059_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN059_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN059_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN059_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN059_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN059_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN059_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN059_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN059_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN059_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN059_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN060_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN060_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN060_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN060_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN060_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN060_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN060_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN060_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN060_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN060_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN060_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN060_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN060_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN060_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN061_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN061_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN061_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN061_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN061_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN061_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN061_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN061_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN061_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN061_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN061_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN061_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN061_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN061_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN062_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN062_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN062_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN062_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN062_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN062_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN062_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN062_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN062_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN062_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN062_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN062_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN062_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN062_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN063_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN063_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN063_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN063_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN063_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN063_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN063_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN063_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN063_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN063_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN063_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN063_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN063_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN063_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN064_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN064_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN064_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN064_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN064_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN064_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN064_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN064_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN064_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN064_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN064_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN064_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN064_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN064_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN065_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN065_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN065_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN065_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN065_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN065_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN065_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN065_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN065_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN065_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN065_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN065_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN065_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN065_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN066_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN066_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN066_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN066_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN066_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN066_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN066_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN066_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN066_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN066_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN066_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN066_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN066_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN066_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN067_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN067_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN067_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN067_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN067_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN067_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN067_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN067_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN067_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN067_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN067_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN067_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN067_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN067_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN068_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN068_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN068_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN068_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN068_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN068_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN068_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN068_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN068_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN068_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN068_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN068_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN068_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN068_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN069_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN069_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN069_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN069_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN069_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN069_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN069_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN069_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN069_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN069_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN069_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN069_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN069_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN069_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN070_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN070_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN070_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN070_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN070_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN070_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN070_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN070_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN070_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN070_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN070_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN070_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN070_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN070_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN071_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN071_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN071_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN071_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN071_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN071_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN071_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN071_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN071_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN071_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN071_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN071_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN071_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN071_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN072_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN072_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN072_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN072_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN072_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN072_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN072_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN072_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN072_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN072_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN072_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN072_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN072_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN072_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN073_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN073_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN073_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN073_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN073_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN073_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN073_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN073_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN073_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN073_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN073_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN073_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN073_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN073_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN074_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN074_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN074_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN074_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN074_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN074_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN074_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN074_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN074_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN074_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN074_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN074_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN074_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN074_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN075_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN075_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN075_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN075_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN075_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN075_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN075_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN075_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN075_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN075_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN075_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN075_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN075_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN075_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN076_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN076_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN076_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN076_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN076_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN076_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN076_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN076_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN076_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN076_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN076_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN076_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN076_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN076_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN077_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN077_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN077_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN077_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN077_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN077_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN077_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN077_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN077_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN077_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN077_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN077_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN077_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN077_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN078_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN078_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN078_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN078_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN078_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN078_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN078_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN078_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN078_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN078_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN078_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN078_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN078_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN078_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN079_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN079_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN079_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN079_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN079_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN079_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN079_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN079_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN079_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN079_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN079_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN079_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN079_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN079_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN080_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN080_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN080_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN080_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN080_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN080_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN080_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN080_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN080_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN080_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN080_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN080_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN080_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN080_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN081_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN081_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN081_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN081_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN081_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN081_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN081_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN081_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN081_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN081_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN081_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN081_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN081_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN081_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN082_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN082_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN082_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN082_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN082_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN082_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN082_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN082_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN082_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN082_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN082_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN082_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN082_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN082_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN083_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN083_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN083_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN083_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN083_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN083_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN083_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN083_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN083_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN083_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN083_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN083_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN083_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN083_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN084_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN084_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN084_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN084_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN084_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN084_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN084_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN084_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN084_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN084_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN084_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN084_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN084_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN084_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN085_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN085_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN085_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN085_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN085_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN085_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN085_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN085_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN085_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN085_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN085_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN085_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN085_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN085_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN086_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN086_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN086_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN086_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN086_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN086_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN086_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN086_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN086_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN086_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN086_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN086_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN086_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN086_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN087_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN087_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN087_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN087_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN087_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN087_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN087_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN087_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN087_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN087_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN087_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN087_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN087_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN087_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN088_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN088_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN088_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN088_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN088_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN088_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN088_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN088_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN088_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN088_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN088_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN088_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN088_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN088_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN089_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN089_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN089_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN089_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN089_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN089_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN089_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN089_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN089_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN089_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN089_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN089_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN089_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN089_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN090_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN090_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN090_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN090_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN090_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN090_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN090_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN090_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN090_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN090_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN090_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN090_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN090_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN090_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN091_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN091_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN091_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN091_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN091_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN091_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN091_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN091_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN091_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN091_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN091_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN091_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN091_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN091_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN092_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN092_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN092_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN092_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN092_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN092_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN092_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN092_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN092_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN092_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN092_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN092_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN092_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN092_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN093_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN093_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN093_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN093_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN093_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN093_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN093_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN093_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN093_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN093_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN093_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN093_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN093_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN093_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN094_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN094_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN094_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN094_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN094_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN094_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN094_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN094_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN094_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN094_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN094_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN094_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN094_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN094_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN095_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN095_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN095_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN095_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN095_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN095_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN095_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN095_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN095_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN095_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN095_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN095_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN095_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN095_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN096_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN096_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN096_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN096_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN096_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN096_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN096_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN096_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN096_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN096_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN096_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN096_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN096_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN096_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN097_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN097_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN097_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN097_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN097_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN097_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN097_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN097_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN097_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN097_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN097_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN097_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN097_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN097_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN098_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN098_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN098_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN098_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN098_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN098_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN098_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN098_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN098_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN098_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN098_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN098_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN098_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN098_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN099_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN099_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN099_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN099_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN099_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN099_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN099_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN099_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN099_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN099_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN099_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN099_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN099_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN099_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN100_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN100_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN100_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN100_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN100_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN100_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN100_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN100_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN100_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN100_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN100_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN100_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN100_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN100_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN102_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN102_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN102_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN102_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN102_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN102_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN102_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN102_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN102_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN102_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN102_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN102_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN102_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN102_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN103_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN103_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN103_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN103_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN103_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN103_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN103_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN103_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN103_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN103_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN103_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN103_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN103_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN103_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN104_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN104_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN104_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN104_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN104_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN104_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN104_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN104_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN104_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN104_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN104_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN104_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN104_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN104_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN105_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN105_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN105_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN105_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN105_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN105_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN105_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN105_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN105_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN105_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN105_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN105_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN105_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN105_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN106_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN106_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN106_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN106_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN106_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN106_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN106_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN106_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN106_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN106_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN106_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN106_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN106_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN106_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN108_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN108_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN108_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN108_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN108_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN108_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN108_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN108_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN108_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN108_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN108_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN108_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN108_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN108_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN109_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN109_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN109_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN109_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN109_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN109_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN109_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN109_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN109_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN109_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN109_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN109_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN109_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN109_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN110_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN110_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN110_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN110_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN110_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN110_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN110_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN110_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN110_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN110_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN110_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN110_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN110_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN110_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN111_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN111_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN111_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN111_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN111_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN111_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN111_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN111_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN111_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN111_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN111_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN111_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN111_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN111_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN112_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN112_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN112_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN112_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN112_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN112_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN112_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN112_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN112_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN112_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN112_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN112_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN112_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN112_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN113_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN113_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN113_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN113_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN113_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN113_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN113_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN113_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN113_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN113_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN113_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN113_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN113_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN113_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN148_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN148_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN148_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN148_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN148_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN148_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN148_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN148_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN148_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN148_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN148_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN148_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN148_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN148_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN149_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN149_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN149_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN149_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN149_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN149_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN149_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN149_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN149_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN149_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN149_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN149_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN149_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN149_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN150_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN150_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN150_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN150_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN150_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN150_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN150_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN150_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN150_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN150_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN150_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN150_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN150_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN150_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN151_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN151_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN151_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN151_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN151_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN151_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN151_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN151_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN151_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN151_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN151_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN151_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN151_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN151_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN152_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN152_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN152_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN152_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN152_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN152_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN152_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN152_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN152_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN152_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN152_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN152_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN152_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN152_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN153_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN153_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN153_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN153_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN153_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN153_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN153_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN153_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN153_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN153_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN153_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN153_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN153_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN153_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN154_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN154_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN154_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN154_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN154_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN154_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN154_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN154_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN154_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN154_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN154_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN154_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN154_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN154_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN155_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN155_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN155_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN155_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN155_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN155_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN155_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN155_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN155_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN155_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN155_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN155_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN155_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN155_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN156_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN156_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN156_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN156_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN156_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN156_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN156_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN156_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN156_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN156_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN156_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN156_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN156_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN156_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN157_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN157_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN157_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN157_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN157_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN157_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN157_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN157_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN157_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN157_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN157_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN157_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN157_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN157_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN158_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN158_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN158_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN158_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN158_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN158_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN158_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN158_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN158_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN158_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN158_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN158_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN158_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN158_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN159_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN159_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN159_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN159_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN159_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN159_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN159_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN159_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN159_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN159_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN159_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN159_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN159_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN159_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN160_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN160_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN160_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN160_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN160_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN160_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN160_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN160_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN160_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN160_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN160_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN160_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN160_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN160_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN161_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN161_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN161_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN161_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN161_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN161_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN161_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN161_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN161_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN161_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN161_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN161_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN161_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN161_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN162_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN162_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN162_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN162_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN162_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN162_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN162_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN162_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN162_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN162_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN162_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN162_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN162_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN162_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN163_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN163_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN163_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN163_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN163_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN163_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN163_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN163_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN163_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN163_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN163_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN163_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN163_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN163_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN164_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN164_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN164_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN164_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN164_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN164_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN164_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN164_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN164_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN164_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN164_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN164_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN164_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN164_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN165_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN165_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN165_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN165_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN165_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN165_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN165_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN165_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN165_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN165_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN165_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN165_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN165_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN165_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN166_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN166_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN166_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN166_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN166_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN166_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN166_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN166_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN166_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN166_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN166_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN166_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN166_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN166_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN167_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN167_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN167_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN167_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN167_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN167_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN167_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN167_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN167_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN167_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN167_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN167_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN167_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN167_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN168_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN168_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN168_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN168_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN168_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN168_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN168_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN168_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN168_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN168_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN168_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN168_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN168_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN168_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN169_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN169_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN169_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN169_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN169_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN169_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN169_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN169_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN169_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN169_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN169_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN169_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN169_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN169_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN170_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN170_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN170_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN170_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN170_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN170_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN170_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN170_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN170_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN170_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN170_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN170_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN170_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN170_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN171_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN171_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN171_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN171_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN171_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN171_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN171_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN171_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN171_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN171_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN171_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN171_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN171_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN171_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN172_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN172_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN172_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN172_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN172_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN172_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN172_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN172_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN172_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN172_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN172_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN172_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN172_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN172_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN173_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN173_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN173_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN173_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN173_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN173_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN173_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN173_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN173_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN173_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN173_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN173_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN173_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN173_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN174_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN174_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN174_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN174_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN174_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN174_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN174_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN174_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN174_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN174_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN174_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN174_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN174_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN174_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN175_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN175_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN175_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN175_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN175_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN175_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN175_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN175_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN175_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN175_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN175_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN175_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN175_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN175_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN176_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN176_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN176_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN176_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN176_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN176_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN176_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN176_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN176_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN176_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN176_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN176_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN176_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN176_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN177_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN177_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN177_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN177_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN177_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN177_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN177_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN177_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN177_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN177_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN177_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN177_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN177_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN177_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN178_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN178_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN178_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN178_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN178_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN178_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN178_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN178_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN178_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN178_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN178_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN178_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN178_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN178_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN179_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN179_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN179_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN179_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN179_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN179_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN179_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN179_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN179_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN179_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN179_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN179_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN179_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN179_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN180_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN180_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN180_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN180_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN180_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN180_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN180_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN180_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN180_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN180_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN180_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN180_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN180_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN180_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN181_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN181_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN181_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN181_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN181_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN181_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN181_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN181_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN181_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN181_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN181_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN181_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN181_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN181_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN182_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN182_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN182_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN182_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN182_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN182_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN182_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN182_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN182_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN182_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN182_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN182_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN182_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN182_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN183_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN183_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN183_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN183_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN183_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN183_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN183_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN183_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN183_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN183_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN183_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN183_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN183_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN183_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN184_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN184_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN184_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN184_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN184_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN184_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN184_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN184_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN184_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN184_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN184_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN184_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN184_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN184_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN185_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN185_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN185_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN185_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN185_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN185_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN185_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN185_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN185_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN185_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN185_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN185_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN185_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN185_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN186_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN186_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN186_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN186_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN186_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN186_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN186_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN186_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN186_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN186_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN186_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN186_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN186_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN186_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN187_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN187_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN187_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN187_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN187_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN187_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN187_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN187_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN187_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN187_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN187_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN187_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN187_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN187_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN188_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN188_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN188_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN188_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN188_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN188_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN188_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN188_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN188_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN188_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN188_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN188_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN188_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN188_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN189_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN189_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN189_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN189_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN189_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN189_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN189_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN189_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN189_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN189_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN189_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN189_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN189_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN189_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN190_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN190_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN190_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN190_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN190_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN190_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN190_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN190_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN190_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN190_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN190_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN190_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN190_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN190_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN191_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN191_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN191_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN191_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN191_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN191_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN191_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN191_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN191_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN191_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN191_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN191_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN191_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN191_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN192_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN192_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN192_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN192_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN192_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN192_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN192_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN192_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN192_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN192_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN192_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN192_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN192_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN192_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN193_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN193_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN193_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN193_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN193_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN193_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN193_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN193_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN193_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN193_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN193_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN193_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN193_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN193_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN194_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN194_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN194_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN194_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN194_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN194_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN194_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN194_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN194_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN194_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN194_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN194_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN194_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN194_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN195_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN195_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN195_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN195_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN195_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN195_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN195_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN195_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN195_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN195_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN195_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN195_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN195_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN195_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_MON_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_MON_CFG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_MON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_MON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_MON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_MON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_MON4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_MON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_MON5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_MON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_RS_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_RS_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_RS_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_RS_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_RS_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_RS_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_RS_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_RS_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_RS_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_RS_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_RS_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_RS_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_RS_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_RS_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_DI_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_DI_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_DI_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_DI_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_DI_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_DI_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_DI_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_DI_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_DI_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_DI_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_DI_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_DI_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_DI_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_DI_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_16BIT_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_16BIT_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_16BIT_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_16BIT_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_16BIT_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_16BIT_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_16BIT_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_16BIT_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_16BIT_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_16BIT_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_16BIT_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_16BIT_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_16BIT_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_16BIT_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_24BIT_0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_24BIT_0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_24BIT_1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_24BIT_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_24BIT_2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_24BIT_2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_24BIT_3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_24BIT_3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_24BIT_4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_24BIT_4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_24BIT_5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_24BIT_5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_24BIT_6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_24BIT_6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_TOP_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CONN_TOP_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CBIP_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_SLV_DECODER_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CBIP_SLV_DECODER_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_SLV_DECODER_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CBIP_SLV_DECODER_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_SLV_MUX_MON_CFG, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CBIP_SLV_MUX_MON_CFG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_SLV_MUX_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CBIP_SLV_MUX_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_SLV_MUX_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CBIP_SLV_MUX_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MEMIF_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MEMIF_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_RD_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MEMIF_RD_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_WR_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MEMIF_WR_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_CFG_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MEMIF_CFG_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_BUS_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_BUS_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_BUS_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_BUS_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_BUS_MON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_BUS_MON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_ONE_HEART, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MEMIF_ONE_HEART = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_SRAM_DIS0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MEMIF_SRAM_DIS0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_SRAM_DIS1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MEMIF_SRAM_DIS1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_SRAM_DIS2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MEMIF_SRAM_DIS2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_SRAM_DIS3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_MEMIF_SRAM_DIS3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL0_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL0_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL0_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL0_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL0_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL0_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL0_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL0_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL0_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL0_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL0_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL0_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL0_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL0_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL0_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL0_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL0_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL0_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL0_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL0_MEM_UP_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL0_MEM_UP_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL0_MEM_UP, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL0_MEM_UP = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL1_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL1_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL1_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL1_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL1_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL1_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL1_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL1_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL1_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_MEM_UP_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL1_MEM_UP_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_MEM_UP, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL1_MEM_UP = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL2_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL2_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL2_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL2_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL2_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL2_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL2_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL2_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_MEM_UP_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL2_MEM_UP_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_MEM_UP, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL2_MEM_UP = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL3_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL3_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL3_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL3_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL3_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL3_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL3_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL3_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL3_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL3_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_MEM_UP_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL3_MEM_UP_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_MEM_UP, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL3_MEM_UP = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL4_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL4_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL4_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL4_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL4_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL4_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL4_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL4_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL4_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL4_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_MEM_UP_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL4_MEM_UP_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL4_MEM_UP, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL4_MEM_UP = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL5_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL5_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL5_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL5_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL5_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL5_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL5_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL5_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL5_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL5_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_MEM_UP_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL5_MEM_UP_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL5_MEM_UP, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL5_MEM_UP = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL6_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL6_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL6_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL6_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL6_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL6_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL6_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL6_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL6_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL6_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_MEM_UP_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL6_MEM_UP_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL6_MEM_UP, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL6_MEM_UP = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL7_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL7_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL7_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL7_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL7_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL7_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL7_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL7_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL7_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL7_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_MEM_UP_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL7_MEM_UP_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL7_MEM_UP, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL7_MEM_UP = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL8_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL8_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL8_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL8_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL8_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL8_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL8_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL8_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL8_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL8_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_MEM_UP_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL8_MEM_UP_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL8_MEM_UP, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL8_MEM_UP = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_MEM_UP_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_MEM_UP_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_MEM_UP, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_MEM_UP = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_MEM_UP_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_MEM_UP_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_MEM_UP, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_MEM_UP = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL44_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL44_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL44_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL44_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL44_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL44_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL44_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL44_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL44_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL44_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL44_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL44_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL44_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL44_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL44_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL44_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL44_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL44_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL44_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL44_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL44_MEM_UP_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL44_MEM_UP_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL44_MEM_UP, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL44_MEM_UP = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL45_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL45_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL45_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL45_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL45_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL45_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL45_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL45_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL45_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL45_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL45_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL45_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL45_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL45_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL45_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL45_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL45_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL45_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL45_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL45_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL45_MEM_UP_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL45_MEM_UP_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL45_MEM_UP, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL45_MEM_UP = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL46_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL46_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL46_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL46_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL46_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL46_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL46_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL46_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL46_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL46_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL46_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL46_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL46_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL46_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL46_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL46_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL46_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL46_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL46_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL46_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL46_MEM_UP_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL46_MEM_UP_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL46_MEM_UP, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL46_MEM_UP = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL0_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL0_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL0_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL0_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL0_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL0_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL0_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL0_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL0_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL0_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL0_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL0_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL0_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL0_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL0_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL0_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL0_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL0_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL0_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL1_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL1_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL1_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL1_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL1_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL1_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL1_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL1_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL1_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL1_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL1_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL1_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL1_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL1_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL1_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL1_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL1_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL1_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL2_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL2_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL2_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL2_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL2_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL2_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL2_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL2_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL3_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL3_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL3_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL3_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL3_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL3_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL3_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL3_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL3_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL3_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL3_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL4_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL4_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL4_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL4_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL4_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL4_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL4_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL4_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL4_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL4_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL4_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL5_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL5_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL5_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL5_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL5_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL5_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL5_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL5_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL5_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL5_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL5_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL6_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL6_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL6_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL6_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL6_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL6_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL6_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL6_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL6_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL6_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL6_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL7_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL7_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL7_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL7_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL7_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL7_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL7_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL7_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL7_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL7_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL7_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL7_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL7_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL7_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL7_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL7_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL7_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL7_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL7_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL7_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL8_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL8_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL8_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL8_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL8_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL8_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL8_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL8_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL8_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL8_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL8_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL8_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL8_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL8_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL8_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL8_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL8_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL8_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL8_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL8_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL9_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL9_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL9_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL9_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL9_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL9_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL9_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL9_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL9_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL9_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL9_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL9_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL9_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL9_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL9_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL9_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL9_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL9_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL9_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL9_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL10_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL10_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL10_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL10_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL10_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL10_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL10_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL10_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL10_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL10_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL10_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL10_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL10_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL10_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL10_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL10_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL10_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL10_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL10_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL10_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL37_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL37_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL37_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL37_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL37_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL37_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL37_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL37_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL37_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL37_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL37_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL37_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL37_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL37_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL37_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL37_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL38_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL38_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL38_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL38_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL38_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL38_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL38_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL38_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL38_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL38_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL38_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL38_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL38_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL38_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL38_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL38_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM0_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM0_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM0_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM0_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM0_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM0_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM0_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM0_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM0_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM0_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM0_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM0_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM0_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM0_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM0_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM1_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM1_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM1_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM1_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM1_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM1_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM1_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM1_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM1_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM1_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM1_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM1_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM1_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM1_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM2_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM2_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM2_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM2_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM2_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM2_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM2_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM2_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM2_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM2_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM2_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM2_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM2_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN0_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN0_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN0_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN0_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN0_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN0_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN0_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN0_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN1_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN1_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN1_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN1_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN1_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN1_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN1_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN2_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN2_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN2_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN2_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN2_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN2_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN2_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN2_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN2_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN2_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN2_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN2_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN2_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN6_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN6_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN6_BASE, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN6_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN6_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN6_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN6_CUR, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN6_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN6_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN6_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN6_END, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN6_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN6_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN6_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN6_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN6_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL37_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL37_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL37_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL37_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL38_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL38_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL38_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL38_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM0_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM0_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM0_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM0_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM1_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM1_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM1_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM1_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM2_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM2_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CM2_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_VUL_CM2_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN0_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN0_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN0_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN1_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN1_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN1_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN2_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN2_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN2_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN2_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN6_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN6_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ETDM_IN6_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ETDM_IN6_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_CH0_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_CH0_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_CH1_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_CH1_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_CH2_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_CH2_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_CH3_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_CH3_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_CH4_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_CH4_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_CH5_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_CH5_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_CH6_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_CH6_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_24CH_CH7_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_24CH_CH7_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH0_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH0_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH1_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH1_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH2_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH2_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH3_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH3_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH4_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH4_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH5_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH5_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH6_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH6_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH7_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH7_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_NS_SIDEBAND0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_NS_SIDEBAND0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_NS_SIDEBAND1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_NS_SIDEBAND1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_NS_SIDEBAND2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_NS_SIDEBAND2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_NS_SIDEBAND3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_NS_SIDEBAND3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_PROT_SIDEBAND0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_PROT_SIDEBAND0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_PROT_SIDEBAND1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_PROT_SIDEBAND1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_PROT_SIDEBAND2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_PROT_SIDEBAND2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_PROT_SIDEBAND3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_PROT_SIDEBAND3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_PROT_SIDEBAND4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_PROT_SIDEBAND4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_PROT_SIDEBAND5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_PROT_SIDEBAND5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_PROT_SIDEBAND6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_PROT_SIDEBAND6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_PROT_SIDEBAND7, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_PROT_SIDEBAND7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_PROT_SIDEBAND8, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_PROT_SIDEBAND8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_PROT_SIDEBAND9, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_PROT_SIDEBAND9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_PROT_SIDEBAND10, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_PROT_SIDEBAND10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_PROT_SIDEBAND11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_PROT_SIDEBAND11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_DOMAIN_SIDEBAND0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_DOMAIN_SIDEBAND1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_DOMAIN_SIDEBAND2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_DOMAIN_SIDEBAND3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_DOMAIN_SIDEBAND4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_DOMAIN_SIDEBAND5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_DOMAIN_SIDEBAND6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND7, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_DOMAIN_SIDEBAND7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND8, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_DOMAIN_SIDEBAND8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND9, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_DOMAIN_SIDEBAND9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND10, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_DOMAIN_SIDEBAND10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SE_DOMAIN_SIDEBAND11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SE_DOMAIN_SIDEBAND11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SID_SIDEBAND0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SID_SIDEBAND0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SID_SIDEBAND1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SID_SIDEBAND1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SID_SIDEBAND2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SID_SIDEBAND2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SID_SIDEBAND3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SID_SIDEBAND3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SID_SIDEBAND4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SID_SIDEBAND4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SID_SIDEBAND5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SID_SIDEBAND5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SID_SIDEBAND6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SID_SIDEBAND6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SID_SIDEBAND7, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SID_SIDEBAND7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SID_SIDEBAND8, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SID_SIDEBAND8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SID_SIDEBAND9, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SID_SIDEBAND9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SID_SIDEBAND10, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SID_SIDEBAND10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SID_SIDEBAND11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SID_SIDEBAND11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CQDMA_SIDEBAND, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CQDMA_SIDEBAND = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH0_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH0_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH1_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH1_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH2_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH2_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH3_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH3_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH4_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH4_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH5_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH5_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH6_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH6_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH7_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH7_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH8_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH8_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH9_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH9_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH10_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH10_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH11_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH11_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH12_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH12_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH13_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH13_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH14_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH14_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH15_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH15_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH16_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH16_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH17_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH17_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH18_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH18_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH19_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH19_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH20_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH20_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH21_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH21_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH22_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH22_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH23_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH23_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH24_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH24_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH25_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH25_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH26_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH26_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH27_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH27_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH28_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH28_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH29_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH29_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH30_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH30_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL_48CH_CH31_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_DL_48CH_CH31_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_MULTI_USER1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_MULTI_USER1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_MULTI_USER1_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_MULTI_USER1_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_MULTI_USER1_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_MULTI_USER1_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_MULTI_USER2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_MULTI_USER2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_MULTI_USER2_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_MULTI_USER2_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_MULTI_USER2_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_MULTI_USER2_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON13, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_CON14, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_CON14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_NEW_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_ASRC_NEW_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON13, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_CON14, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_CON14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC0_NEW_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC0_NEW_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON13, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_CON14, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_CON14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC1_NEW_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC1_NEW_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON13, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_CON14, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_CON14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC2_NEW_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC2_NEW_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON13, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_CON14, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_CON14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC3_NEW_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC3_NEW_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC4_NEW_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC4_NEW_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC4_NEW_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC4_NEW_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC4_NEW_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC4_NEW_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC4_NEW_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC4_NEW_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC4_NEW_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC4_NEW_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC4_NEW_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC4_NEW_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC4_NEW_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC4_NEW_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC4_NEW_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC4_NEW_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC4_NEW_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC4_NEW_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC4_NEW_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC4_NEW_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC4_NEW_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC4_NEW_CON10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC4_NEW_CON11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC4_NEW_CON11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC4_NEW_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC4_NEW_CON12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC4_NEW_CON13, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC4_NEW_CON13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC4_NEW_CON14, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC4_NEW_CON14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC4_NEW_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC4_NEW_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC5_NEW_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC5_NEW_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC5_NEW_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC5_NEW_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC5_NEW_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC5_NEW_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC5_NEW_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC5_NEW_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC5_NEW_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC5_NEW_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC5_NEW_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC5_NEW_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC5_NEW_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC5_NEW_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC5_NEW_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC5_NEW_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC5_NEW_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC5_NEW_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC5_NEW_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC5_NEW_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC5_NEW_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC5_NEW_CON10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC5_NEW_CON11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC5_NEW_CON11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC5_NEW_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC5_NEW_CON12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC5_NEW_CON13, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC5_NEW_CON13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC5_NEW_CON14, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC5_NEW_CON14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC5_NEW_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC5_NEW_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC6_NEW_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC6_NEW_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC6_NEW_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC6_NEW_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC6_NEW_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC6_NEW_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC6_NEW_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC6_NEW_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC6_NEW_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC6_NEW_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC6_NEW_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC6_NEW_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC6_NEW_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC6_NEW_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC6_NEW_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC6_NEW_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC6_NEW_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC6_NEW_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC6_NEW_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC6_NEW_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC6_NEW_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC6_NEW_CON10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC6_NEW_CON11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC6_NEW_CON11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC6_NEW_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC6_NEW_CON12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC6_NEW_CON13, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC6_NEW_CON13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC6_NEW_CON14, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC6_NEW_CON14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC6_NEW_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC6_NEW_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC7_NEW_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC7_NEW_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC7_NEW_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC7_NEW_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC7_NEW_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC7_NEW_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC7_NEW_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC7_NEW_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC7_NEW_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC7_NEW_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC7_NEW_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC7_NEW_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC7_NEW_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC7_NEW_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC7_NEW_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC7_NEW_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC7_NEW_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC7_NEW_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC7_NEW_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC7_NEW_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC7_NEW_CON10, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC7_NEW_CON10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC7_NEW_CON11, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC7_NEW_CON11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC7_NEW_CON12, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC7_NEW_CON12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC7_NEW_CON13, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC7_NEW_CON13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC7_NEW_CON14, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC7_NEW_CON14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GASRC7_NEW_IP_VERSION, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_GASRC7_NEW_IP_VERSION = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SOUNDWIRE_ULSRC_PHASE_CLK_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SOUNDWIRE_ULSRC_PHASE_CLK_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SOUNDWIRE_ULSRC_PHASE_CLK_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SOUNDWIRE_ULSRC_PHASE_CLK_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SOUNDWIRE_ULSRC_PHASE_CLK_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SOUNDWIRE_ULSRC_PHASE_CLK_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SOUNDWIRE_ULSRC_PHASE_CLK_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SOUNDWIRE_ULSRC_PHASE_CLK_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SOUNDWIRE_ULSRC_PHASE_CLK_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SOUNDWIRE_ULSRC_PHASE_CLK_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SOUNDWIRE_ULSRC_PHASE_ENGEN_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SOUNDWIRE_ULSRC_PHASE_ENGEN_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SOUNDWIRE_ULSRC_PHASE_ENGEN_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SOUNDWIRE_ULSRC_PHASE_ENGEN_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SOUNDWIRE_ULSRC_PHASE_RST_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_SOUNDWIRE_ULSRC_PHASE_RST_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_DSP_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_DSP_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_DSP2_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_DSP2_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_SCP_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_SCP_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ_MCU_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_DSP_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ_MCU_DSP_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_DSP2_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ_MCU_DSP2_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_SCP_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ_MCU_SCP_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_STATUS, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_STATUS = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_STATUS, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ_MCU_STATUS = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ0_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ0_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ0_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ0_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ1_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ1_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ1_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ1_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ2_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ2_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ2_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ2_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ3_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ3_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ3_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ3_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ4_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ4_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ4_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ4_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ5_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ5_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ5_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ5_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ6_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ6_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ6_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ6_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ7_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ7_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ7_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ7_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ8_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ8_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ8_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ8_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ9_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ9_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ9_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ9_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ10_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ10_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ10_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ10_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ11_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ11_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ11_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ11_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ12_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ12_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ12_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ12_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ13_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ13_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ13_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ13_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ14_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ14_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ14_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ14_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ15_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ15_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ15_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ15_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ16_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ16_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ16_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ16_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ17_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ17_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ17_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ17_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ18_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ18_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ18_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ18_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ19_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ19_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ19_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ19_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ20_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ20_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ20_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ20_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ21_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ21_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ21_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ21_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ22_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ22_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ22_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ22_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ23_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ23_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ23_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ23_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ24_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ24_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ24_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ24_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ25_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ25_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ25_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ25_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ26_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ26_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ26_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ26_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ22_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ22_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ22_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ22_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ23_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ23_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ23_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ23_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ30_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ30_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ30_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ30_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ0_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ0_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ1_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ1_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ2_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ2_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ3_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ3_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ4_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ4_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ5_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ5_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ6_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ6_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ7_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ7_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ8_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ8_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ9_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ9_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ10_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ10_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ11_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ11_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ12_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ12_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ13_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ13_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ14_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ14_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ15_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ15_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ16_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ16_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ17_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ17_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ18_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ18_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ19_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ19_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ20_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ20_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ21_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ21_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ22_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ22_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ23_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ23_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ24_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ24_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ25_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ25_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ26_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ26_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_DSP3_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_DSP3_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_DSP3_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ_MCU_DSP3_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ_MCU_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ_MCU_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ_MCU_DSP_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ_MCU_DSP_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ_MCU_DSP2_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ_MCU_DSP2_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ_MCU_DSP3_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ_MCU_DSP3_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ_MCU_SCP_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ_MCU_SCP_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_MON3, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_MON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_MON0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_MON1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_MON2, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_MON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ_MISS_FLAG_MCU_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ_MISS_FLAG_MCU_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ_DELAY_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ_DELAY_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ_MCU_STATUS, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ_MCU_STATUS = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ0_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ0_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ0_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ0_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ0_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ0_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ0_MCU_DELAY_CNT_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ0_MCU_DELAY_CNT_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ1_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ1_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ1_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ1_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ1_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ1_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ1_MCU_DELAY_CNT_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ1_MCU_DELAY_CNT_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ2_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ2_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ2_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ2_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ2_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ2_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ2_MCU_DELAY_CNT_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ2_MCU_DELAY_CNT_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ3_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ3_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ3_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ3_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ3_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ3_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ3_MCU_DELAY_CNT_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ3_MCU_DELAY_CNT_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ4_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ4_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ4_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ4_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ4_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ4_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ4_MCU_DELAY_CNT_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ4_MCU_DELAY_CNT_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ5_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ5_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ5_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ5_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ5_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ5_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ5_MCU_DELAY_CNT_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ5_MCU_DELAY_CNT_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ6_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ6_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ6_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ6_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ6_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ6_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ6_MCU_DELAY_CNT_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ6_MCU_DELAY_CNT_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ7_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ7_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ7_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ7_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ7_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ7_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ7_MCU_DELAY_CNT_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ7_MCU_DELAY_CNT_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ8_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ8_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ8_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ8_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ8_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ8_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ8_MCU_DELAY_CNT_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ8_MCU_DELAY_CNT_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ9_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ9_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ9_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ9_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ9_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ9_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ9_MCU_DELAY_CNT_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ9_MCU_DELAY_CNT_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ10_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ10_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ10_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ10_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ10_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ10_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ10_MCU_DELAY_CNT_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ10_MCU_DELAY_CNT_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ24_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ24_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ24_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ24_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ24_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ24_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ24_MCU_DELAY_CNT_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ24_MCU_DELAY_CNT_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ25_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ25_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ25_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ25_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ25_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ25_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ25_MCU_DELAY_CNT_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ25_MCU_DELAY_CNT_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ30_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ30_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ30_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ30_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ30_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ30_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ30_MCU_DELAY_CNT_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ30_MCU_DELAY_CNT_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ31_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ31_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ31_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ31_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ31_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ31_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ31_MCU_DELAY_CNT_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ31_MCU_DELAY_CNT_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ0_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ0_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ0_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ0_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ0_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ0_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ0_MCU_DELAY_CNT_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ0_MCU_DELAY_CNT_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ4_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ4_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ4_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ4_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ4_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ4_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ4_MCU_DELAY_CNT_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ4_MCU_DELAY_CNT_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ8_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ8_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ8_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ8_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ8_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ8_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ8_MCU_DELAY_CNT_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ8_MCU_DELAY_CNT_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ9_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ9_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ9_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ9_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ9_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ9_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ9_MCU_DELAY_CNT_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ9_MCU_DELAY_CNT_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ10_MCU_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ10_MCU_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ10_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ10_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ10_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ10_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ10_MCU_DELAY_CNT_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ10_MCU_DELAY_CNT_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ_MISS_FLAG_MCU_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ_MISS_FLAG_MCU_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ_DELAY_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ_DELAY_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ_MCU_STATUS, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ_MCU_STATUS = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ_MCU_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ_MCU_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ_MCU_DSP_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ_MCU_DSP_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ_MCU_DSP2_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ_MCU_DSP2_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ_MCU_DSP3_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ_MCU_DSP3_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ_MCU_DSP_WLA_SIDEBAND_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ_MCU_DSP_WLA_SIDEBAND_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ_MCU_SCP_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ_MCU_SCP_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ_MCU_DSP_WLA_SIDEBAND_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ_MCU_DSP_WLA_SIDEBAND_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_DSP_WLA_SIDEBAND_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_DSP_WLA_SIDEBAND_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_COMMON2_IRQ_MCU_STATUS, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_COMMON2_IRQ_MCU_STATUS = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_COMMON2_IRQ_MCU_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_COMMON2_IRQ_MCU_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_COMMON2_IRQ_MCU_DSP_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_COMMON2_IRQ_MCU_DSP_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_COMMON2_IRQ_MCU_DSP2_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_COMMON2_IRQ_MCU_DSP2_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_COMMON2_IRQ_MCU_DSP3_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_COMMON2_IRQ_MCU_DSP3_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_COMMON2_IRQ_MCU_DSP_WLA_SIDEBAND_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_COMMON2_IRQ_MCU_DSP_WLA_SIDEBAND_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_COMMON2_IRQ_MCU_SCP_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_COMMON2_IRQ_MCU_SCP_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_DSP_WLA_SIDEBAND_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ_MCU_DSP_WLA_SIDEBAND_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_VM0_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_VM0_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_VM1_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_VM1_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_VM2_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_VM2_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_VM3_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_VM3_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_VM4_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_VM4_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_VM5_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_VM5_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_COMMON2_IRQ_MCU_VM0_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_COMMON2_IRQ_MCU_VM0_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_COMMON2_IRQ_MCU_VM1_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_COMMON2_IRQ_MCU_VM1_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_COMMON2_IRQ_MCU_VM2_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_COMMON2_IRQ_MCU_VM2_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_COMMON2_IRQ_MCU_VM3_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_COMMON2_IRQ_MCU_VM3_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_COMMON2_IRQ_MCU_VM4_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_COMMON2_IRQ_MCU_VM4_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_COMMON2_IRQ_MCU_VM5_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_COMMON2_IRQ_MCU_VM5_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_VM0_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ_MCU_VM0_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_VM1_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ_MCU_VM1_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_VM2_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ_MCU_VM2_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_VM3_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ_MCU_VM3_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_VM4_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ_MCU_VM4_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_VM5_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ_MCU_VM5_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ_MCU_VM0_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ_MCU_VM0_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ_MCU_VM1_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ_MCU_VM1_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ_MCU_VM2_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ_MCU_VM2_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ_MCU_VM3_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ_MCU_VM3_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ_MCU_VM4_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ_MCU_VM4_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ_MCU_VM5_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ_MCU_VM5_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ_MCU_VM0_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ_MCU_VM0_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ_MCU_VM1_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ_MCU_VM1_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ_MCU_VM2_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ_MCU_VM2_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ_MCU_VM3_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ_MCU_VM3_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ_MCU_VM4_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ_MCU_VM4_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ_MCU_VM5_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ_MCU_VM5_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_DSP_WLA_PLAYBACK_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_IRQ_MCU_DSP_WLA_PLAYBACK_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_COMMON2_IRQ_MCU_DSP_WLA_PLAYBACK_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_COMMON2_IRQ_MCU_DSP_WLA_PLAYBACK_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ_MCU_DSP_WLA_PLAYBACK_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ_MCU_DSP_WLA_PLAYBACK_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM2_IRQ_MCU_DSP_WLA_PLAYBACK_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM2_IRQ_MCU_DSP_WLA_PLAYBACK_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM3_IRQ_MCU_DSP_WLA_PLAYBACK_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM3_IRQ_MCU_DSP_WLA_PLAYBACK_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CUSTOM_IRQ31_MCU_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
		"AFE_CUSTOM_IRQ31_MCU_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_CUSTOM_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_CUSTOM_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN_DMA0_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN_DMA0_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN_DMA0_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN_DMA0_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN_DMA0_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN_DMA0_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN_DMA0_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN_DMA0_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN_DMA0_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN_DMA0_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN_DMA0_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN_DMA0_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN_DMA0_CON6, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN_DMA0_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN_DMA0_CON7, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN_DMA0_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN_DMA0_CON8, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN_DMA0_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN_DMA0_CON9, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN_DMA0_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_IN_DMA0_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_IN_DMA0_MON = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_DMA_0_3_COWORK_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_DMA_0_3_COWORK_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_DMA_0_3_COWORK_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_DMA_0_3_COWORK_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_DMA_0_3_COWORK_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_DMA_0_3_COWORK_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_DMA_0_3_COWORK_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_DMA_0_3_COWORK_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_DMA_TOP_MON, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_DMA_TOP_MON = 0x%x\n", value);
	regmap_read(afe->regmap, ETDM_DMA_ASYNC_FIFO_CON, &value);
	n += scnprintf(buffer + n, size - n,
		"ETDM_DMA_ASYNC_FIFO_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_INT_FLAG, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_INT_FLAG = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_INT_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_INT_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_CON, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_SRC_ADDR, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_SRC_ADDR = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_DST_ADDR, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_DST_ADDR = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_LEN1, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_LEN1 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_CONNECT, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_CONNECT = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_SRC_ADDR2, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_SRC_ADDR2 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_DST_ADDR2, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_DST_ADDR2 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_IRQS_CNT, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_IRQS_CNT = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_DSTADDR_CNT, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_DSTADDR_CNT = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_INT_EN_RO, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_INT_EN_RO = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_EN_RO, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_EN_RO = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_CON_RO, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_CON_RO = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_SRC_ADDR_RO, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_SRC_ADDR_RO = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_DST_ADDR_RO, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_DST_ADDR_RO = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_LEN1_RO, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_LEN1_RO = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_CONNECT_RO, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_CONNECT_RO = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_SRC_ADDR2_RO, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_SRC_ADDR2_RO = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_DST_ADDR2_RO, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_DST_ADDR2_RO = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_RST, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_RST = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_STOP, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_STOP = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_0_FLUSH, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_0_FLUSH = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_INT_FLAG, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_INT_FLAG = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_INT_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_INT_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_EN, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_CON, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_SRC_ADDR, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_SRC_ADDR = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_DST_ADDR, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_DST_ADDR = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_LEN1, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_LEN1 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_CONNECT, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_CONNECT = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_SRC_ADDR2, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_SRC_ADDR2 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_DST_ADDR2, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_DST_ADDR2 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_IRQS_CNT, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_IRQS_CNT = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_DSTADDR_CNT, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_DSTADDR_CNT = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_INT_EN_RO, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_INT_EN_RO = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_EN_RO, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_EN_RO = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_CON_RO, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_CON_RO = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_SRC_ADDR_RO, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_SRC_ADDR_RO = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_DST_ADDR_RO, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_DST_ADDR_RO = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_LEN1_RO, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_LEN1_RO = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_CONNECT_RO, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_CONNECT_RO = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_SRC_ADDR2_RO, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_SRC_ADDR2_RO = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_DST_ADDR2_RO, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_DST_ADDR2_RO = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_RST, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_RST = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_STOP, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_STOP = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_DMA_G_DMA_1_FLUSH, &value);
	n += scnprintf(buffer + n, size - n,
		"AUDIO_DMA_G_DMA_1_FLUSH = 0x%x\n", value);
	return n;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
static ssize_t mt6881_debugfs_read(struct file *file, char __user *buf,
				   size_t count, loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	const int size = AFE_SYS_DEBUG_SIZE;
	char *buffer = NULL; /* for reduce kernel stack */
	int n = 0, ret = 0;

	buffer = kmalloc(size, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	n = mt6881_debug_read_reg(buffer, size, afe);

	ret = simple_read_from_buffer(buf, count, pos, buffer, n);
	kfree(buffer);
	return ret;
}

static const struct mtk_afe_debug_cmd mt6881_debug_cmds[] = {
	MTK_AFE_DBG_CMD("write_reg", mtk_afe_debug_write_reg),
	{}
};

static const struct file_operations mt6881_debugfs_ops = {
	.open = mtk_afe_debugfs_open,
	.write = mtk_afe_debugfs_write,
	.read = mt6881_debugfs_read,
};
#endif

static int mt6881_dai_memif_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mt6881_memif_dai_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mt6881_memif_dai_driver);

	dai->controls = mt6881_pcm_kcontrols;
	dai->num_controls = ARRAY_SIZE(mt6881_pcm_kcontrols);
	dai->dapm_widgets = mt6881_memif_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mt6881_memif_widgets);
	dai->dapm_routes = mt6881_memif_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mt6881_memif_routes);
	return 0;
}

typedef int (*dai_register_cb)(struct mtk_base_afe *);
static const dai_register_cb dai_register_cbs[] = {
	mt6881_dai_adda_register,
	mt6881_dai_i2s_register,
	mt6881_dai_hw_gain_register,
	mt6881_dai_src_register,
	mt6881_dai_pcm_register,
	mt6881_dai_hostless_register,
	mt6881_dai_memif_register,
};

static int mt6881_afe_pcm_dev_probe(struct platform_device *pdev)
{
	int ret, i;
	unsigned int tmp_reg = 0;
#if !defined(IS_FPGA_EARLY_PORTING) || defined(FORCE_FPGA_ENABLE_IRQ)
	int irq_id;
#endif
	struct mtk_base_afe *afe;
	struct mt6881_afe_private *afe_priv;
	struct resource *res;
	struct device *dev;
#if !defined(SKIP_SB_SMCC)
	struct arm_smccc_res smccc_res;
#endif
	struct device_node *np;
	struct platform_device *pmic_pdev = NULL;
	struct regmap *map;
#if IS_ENABLED(CONFIG_NEBULA_SND_PASSTHROUGH)
	struct irq_desc *desc;
#endif

	pr_info("+%s()\n", __func__);

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));
	if (ret)
		return ret;

	afe = devm_kzalloc(&pdev->dev, sizeof(*afe), GFP_KERNEL);
	if (!afe)
		return -ENOMEM;

	platform_set_drvdata(pdev, afe);
	mt6881_set_local_afe(afe);

	afe->platform_priv = devm_kzalloc(&pdev->dev, sizeof(*afe_priv),
					  GFP_KERNEL);
	if (!afe->platform_priv)
		return -ENOMEM;

	afe_priv = afe->platform_priv;

	afe->dev = &pdev->dev;
	dev = afe->dev;

	/* init audio related clock */
	ret = mt6881_init_clock(afe);
	if (ret) {
		dev_info(dev, "init clock error: %d\n", ret);
#if !defined(SKIP_SB_CLK)
		/* Ignore errors during bring-up, as clocks like topckgen and
		 * infracfg may not be fully operational at this time
		 */
		return ret;
#endif
	}

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev))
		goto err_pm_disable;

	/* Audio device is part of genpd.
	 * Set audio as syscore device to prevent
	 * genpd automatically power off audio
	 * device when suspend
	 */
	dev_pm_syscore_device(&pdev->dev, true);

	/* regmap init */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	afe->base_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(afe->base_addr))
		return PTR_ERR(afe->base_addr);

	/* enable clock for regcache get default value from hw */
	ret = pm_runtime_resume_and_get(&pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: failed to resume: %d\n",
			__func__, ret);
		return ret;
	}

	afe->regmap = devm_regmap_init_mmio(&pdev->dev, afe->base_addr,
					    &mt6881_afe_regmap_config);
	if (IS_ERR(afe->regmap))
		return PTR_ERR(afe->regmap);

	/* IPM2.0 clock flow, need debug */

	regmap_read(afe->regmap, AFE_IRQ_MCU_EN, &tmp_reg);
	regmap_write(afe->regmap, AFE_IRQ_MCU_EN, 0xffffffff);
	regmap_read(afe->regmap, AFE_IRQ_MCU_EN, &tmp_reg);
	/* IPM2.0 clock flow, need debug */

	pm_runtime_put_sync(&pdev->dev);

	regcache_cache_only(afe->regmap, true);
	regcache_mark_dirty(afe->regmap);

	/* init gpio */
	ret = mt6881_afe_gpio_init(afe);
	if (ret)
		dev_info(dev, "init gpio error\n");

#if !IS_ENABLED(CONFIG_NEBULA_SND_PASSTHROUGH)
	/* init sram */
	afe->sram = devm_kzalloc(&pdev->dev, sizeof(struct mtk_audio_sram),
				 GFP_KERNEL);
	if (!afe->sram)
		return -ENOMEM;

	ret = mtk_audio_sram_init(dev, afe->sram, &mt6881_sram_ops);
	if (ret)
		return ret;
#endif
	/* init memif */
	/* IPM2.0 no need banding */
	afe->is_memif_bit_banding = 0;
	afe->memif_32bit_supported = 1;
	afe->memif_size = MT6881_MEMIF_NUM;
	afe->memif = devm_kcalloc(dev, afe->memif_size, sizeof(*afe->memif),
				  GFP_KERNEL);

	if (!afe->memif)
		return -ENOMEM;

	for (i = 0; i < afe->memif_size; i++) {
		afe->memif[i].data = &memif_data[i];
		afe->memif[i].irq_usage = memif_irq_usage[i];
		afe->memif[i].const_irq = 1;
	}
	afe->memif[MT6881_DEEP_MEMIF].ack = mtk_sp_clean_written_buffer_ack;
	afe->memif[MT6881_FAST_MEMIF].fast_palyback = 1;

	mutex_init(&afe->irq_alloc_lock);       /* needed when dynamic irq */

	/* init irq */
	afe->irqs_size = MT6881_IRQ_NUM;
	afe->irqs = devm_kcalloc(dev, afe->irqs_size, sizeof(*afe->irqs),
				 GFP_KERNEL);

	if (!afe->irqs)
		return -ENOMEM;

	for (i = 0; i < afe->irqs_size; i++)
		afe->irqs[i].irq_data = &irq_data[i];

#if !defined(IS_FPGA_EARLY_PORTING) || defined(FORCE_FPGA_ENABLE_IRQ)
	/* request irq */
	irq_id = platform_get_irq(pdev, 0);
	if (irq_id <= 0) {
		dev_info(dev, "%pOFn no irq found\n", dev->of_node);
		return irq_id < 0 ? irq_id : -ENXIO;
	}
#if IS_ENABLED(CONFIG_NEBULA_SND_PASSTHROUGH)
	desc = irq_to_desc(irq_id);
	if (!desc){
		dev_info(dev, "failed to get irq_desc\n");
		return -EINVAL;
	}
	hwirq = desc->irq_data.hwirq;
#endif
	ret = devm_request_irq(dev, irq_id, mt6881_afe_irq_handler,
			       IRQF_TRIGGER_NONE,
			       "Afe_ISR_Handle", (void *)afe);
	if (ret) {
		dev_info(dev, "could not request_irq for Afe_ISR_Handle\n");
		return ret;
	}
	ret = enable_irq_wake(irq_id);
	if (ret < 0)
		dev_info(dev, "enable_irq_wake %d err: %d\n", irq_id, ret);
#endif

#if !defined(SKIP_SB_SMCC)
	/* init arm_smccc_smc call */
	arm_smccc_smc(MTK_SIP_AUDIO_CONTROL, MTK_AUDIO_SMC_OP_INIT,
		      0, 0, 0, 0, 0, 0, &smccc_res);
#endif

	/* init sub_dais */
	INIT_LIST_HEAD(&afe->sub_dais);

	for (i = 0; i < ARRAY_SIZE(dai_register_cbs); i++) {
		ret = dai_register_cbs[i](afe);
		if (ret) {
			dev_info(afe->dev, "dai register i %d fail, ret %d\n",
				 i, ret);
			goto err_pm_disable;
		}
	}

	/* init dai_driver and component_driver */
	ret = mtk_afe_combine_sub_dai(afe);
	if (ret) {
		dev_info(afe->dev, "mtk_afe_combine_sub_dai fail, ret %d\n",
			 ret);
		goto err_pm_disable;
	}

	/* others */
	afe->mtk_afe_hardware = &mt6881_afe_hardware;
	afe->memif_fs = mt6881_memif_fs;
	afe->irq_fs = mt6881_irq_fs;
	afe->get_dai_fs = mt6881_get_dai_fs;
	afe->get_memif_pbuf_size = mt6881_get_memif_pbuf_size;

	afe->runtime_resume = mt6881_afe_runtime_resume;
	afe->runtime_suspend = mt6881_afe_runtime_suspend;

	afe->request_dram_resource = mt6881_afe_dram_request;
	afe->release_dram_resource = mt6881_afe_dram_release;

	afe->copy = mt6881_afe_pcm_copy;

	/* IPM2.0: No need */
	/* afe->is_scp_sema_support = 1; */

#if IS_ENABLED(CONFIG_DEBUG_FS)
	/* debugfs */
	afe->debug_cmds = mt6881_debug_cmds;
	afe->debugfs = debugfs_create_file("mtksocaudio",
					   S_IFREG | 0444, NULL,
					   afe, &mt6881_debugfs_ops);
#endif
	/* usb audio offload hook */
#if !defined(SKIP_SB_USB_OFFLOAD)
	ret = mtk_audio_usb_offload_afe_hook(afe);
	if (ret)
		dev_info(dev, "Hook usb offload interface err: %d\n", ret);
#endif

	/* register component */
	ret = devm_snd_soc_register_component(&pdev->dev,
					      &mt6881_afe_component,
					      afe->dai_drivers,
					      afe->num_dai_drivers);
	if (ret) {
		dev_info(dev, "afe component err: %d\n", ret);
		goto err_pm_disable;
	}
	/* get pmic6363 regmap */
	np = of_find_node_by_name(NULL, "pmic");
	if (!np) {
		dev_info(&pdev->dev, "pmic node not found\n");
		goto err_find_pmic;
	}

	pmic_pdev = of_find_device_by_node(np->child);
	if (!pmic_pdev) {
		dev_info(&pdev->dev, "pmic child device not found\n");
		goto err_find_pmic;
	}

	map = dev_get_regmap(pmic_pdev->dev.parent, NULL);
	afe_priv->pmic_regmap = map;

	//init cm mux 0/1/2 to 0
	mt6881_set_cm_mux(CM0, 0);
	mt6881_set_cm_mux(CM1, 0);
err_find_pmic:
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
	audio_set_dsp_afe(afe);
#endif
#if IS_ENABLED(CONFIG_MTK_ULTRASND_PROXIMITY) && !defined(SKIP_SB_ULTRA)
	ultra_set_dsp_afe(afe);
#endif
	return 0;

err_pm_disable:
	pm_runtime_disable(&pdev->dev);
	return ret;
}

static void mt6881_afe_pcm_dev_remove(struct platform_device *pdev)
{
	struct mtk_base_afe *afe = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		mt6881_afe_runtime_suspend(&pdev->dev);
#if !defined(SKIP_SB_USB_OFFLOAD)
	/* usb audio offload unhook */
	mtk_audio_usb_offload_afe_hook(NULL);
#endif
	/* disable afe clock */
	mt6881_afe_disable_clock(afe);
}

static const struct of_device_id mt6881_afe_pcm_dt_match[] = {
	{ .compatible = "mediatek,mt6881-sound", },
	{},
};
MODULE_DEVICE_TABLE(of, mt6881_afe_pcm_dt_match);

static const struct dev_pm_ops mt6881_afe_pm_ops = {
	SET_RUNTIME_PM_OPS(mt6881_afe_runtime_suspend,
			   mt6881_afe_runtime_resume, NULL)
};

static struct platform_driver mt6881_afe_pcm_driver = {
	.driver = {
		.name = "mt6881-audio",
		.of_match_table = mt6881_afe_pcm_dt_match,
#if IS_ENABLED(CONFIG_PM)
		.pm = &mt6881_afe_pm_ops,
#endif
	},
	.probe = mt6881_afe_pcm_dev_probe,
	.remove = mt6881_afe_pcm_dev_remove,
};

module_platform_driver(mt6881_afe_pcm_driver);

MODULE_DESCRIPTION("Mediatek ALSA SoC AFE platform driver for 6881");
MODULE_AUTHOR("Shane Chien <shane.chien@mediatek.com>");
MODULE_LICENSE("GPL");
