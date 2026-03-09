/*
 * virhaptic.c -- VirHaptic ALSA SoC audio driver
 *
 * Copyright 2024 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/pm_runtime.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/gpio.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/of_irq.h>
#include <linux/completion.h>
#include <linux/err.h>
#include <linux/firmware.h>
#include <linux/timekeeping.h>
#include <linux/uaccess.h>
#include <asm/atomic.h>
#include <sound/pcm.h>
#include "./include/aw_wrapper.h"


#define VIR_HAPTIC_NAME "VIR_HAPTIC_PCM"
#define MTK_PCM_RATES (SNDRV_PCM_RATE_8000_48000 |\
			SNDRV_PCM_RATE_88200 |\
			SNDRV_PCM_RATE_96000 |\
			SNDRV_PCM_RATE_176400 |\
			SNDRV_PCM_RATE_192000)

static const struct snd_pcm_hardware vir_haptic_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
			SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_RESUME |
			SNDRV_PCM_INFO_MMAP_VALID),
	.formats = (SNDRV_PCM_FMTBIT_S16_LE |
			SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S32_LE),
	.rates = MTK_PCM_RATES,
	.channels_min = 1,
	.channels_max = 2,
	.period_bytes_min = 256,
	.period_bytes_max = 4 * 48 * 1024,
	.periods_min = 2,
	.periods_max = 4096,
	.buffer_bytes_max = 4 * 48 * 1024,
	.fifo_size = 0,
};

enum vir_haptic_type {
	TYPE_DEFAULT	    = 0,
	TYPE_AW_HAPTIC		= 1,
	TYPE_CS_HAPTIC		= 2,
};

static enum vir_haptic_type virHapticType = TYPE_AW_HAPTIC;

static int vir_haptic_type_select_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = virHapticType;
	return 0;
}

static int vir_haptic_type_select_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	virHapticType = ucontrol->value.integer.value[0];
	return 0;
}


static const char * const delta_enum_text[] = {"aw","cs"};
static const struct soc_enum delta_enum = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(delta_enum_text), delta_enum_text);

static const struct snd_kcontrol_new vir_haptic_controls[] = {
	SOC_ENUM_EXT("vir_haptic_type", delta_enum, vir_haptic_type_select_get, vir_haptic_type_select_put),
};

static int vir_haptic_probe(struct snd_soc_component *component)
{
	int ret = 0;

	//create work routine
	switch (virHapticType)
	{
	case TYPE_AW_HAPTIC:
		ret = aw_haptic_work_routine();
		break;
	case TYPE_CS_HAPTIC:
		break;
	default:
		pr_err("not't support this haptic type!\n");
		break;
	}

	if(ret < 0) {
		pr_err("aw_haptic_work_routine fail!\n");
	}
	return ret;
}

static int vir_haptic_open(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	int ret = 0;
	pr_err("vir_haptic_open enter\n");
	ret = snd_soc_set_runtime_hwparams(substream, &vir_haptic_hardware);
	if(ret < 0) {
		pr_err("snd_soc_set_runtime_hwparams fail!\n");
	}

	switch (virHapticType)
	{
	case TYPE_AW_HAPTIC:
		ret = aw_haptic_open(substream);
		break;
	case TYPE_CS_HAPTIC:
		break;
	default:
		pr_err("not't support this haptic type!\n");
		break;
	}

	if(ret < 0) {
		pr_err("aw_haptic_open fail!\n");
	}
	return ret;
}

static int vir_haptic_write(struct snd_soc_component *component,
		unsigned int reg, unsigned int val)
{
	int ret = 0;
    pr_debug("vir_haptic_write enter\n");
	return ret;
}

static int vir_haptic_copy(struct snd_soc_component *component,
						   struct snd_pcm_substream *substream, int channel,
						   unsigned long pos, struct iov_iter *buf,
						   unsigned long bytes)
{
	int ret = 0;
	static char writebuf[2048];
    pr_err("vir_haptic_copy enter bytes:%ld\n",bytes);
	ret = copy_from_iter(writebuf, bytes, buf);
	if (ret != bytes) {
  		pr_err("%s copy_from_user fail!\n", __func__);
  		return -1;
  	}

	switch (virHapticType)
	{
	case TYPE_AW_HAPTIC:
		ret = aw_haptic_copy(writebuf,bytes);
		break;
	case TYPE_CS_HAPTIC:
		break;
	default:
		pr_err("not't support this haptic type!\n");
		break;
	}

	if(ret < 0) {
		pr_err("%s aw_haptic_copy zero data!\n", __func__);
	}

	pr_err("vir_haptic_copy exit bytes:%d\n",ret);
	return ret;
}

static int vir_haptic_prepare(struct snd_soc_component *component,
							  struct snd_pcm_substream *substream)
{
	int ret = 0;
	pr_err("vir_haptic_prepare enter\n");
	if (substream->runtime->stop_threshold == ~(0U)) {
		substream->runtime->stop_threshold = ULONG_MAX;
	}
	return ret;
}

static snd_pcm_uframes_t vir_haptic_pointer(struct snd_soc_component *component,
							  struct snd_pcm_substream *substream)
{
	snd_pcm_sframes_t frames = 0;
	switch (virHapticType)
	{
	case TYPE_AW_HAPTIC:
		frames = aw_haptic_pointer(substream);
		break;
	case TYPE_CS_HAPTIC:
		break;
	default:
		pr_err("not't support this haptic type!\n");
		break;
	}

	return frames;
}

static int vir_haptic_close(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
    int ret = 0;
	pr_err("vir_haptic_close enter!\n");

	switch (virHapticType)
	{
	case TYPE_AW_HAPTIC:
		ret = aw_haptic_close();
		break;
	case TYPE_CS_HAPTIC:
		break;
	default:
		pr_err("not't support this haptic type!\n");
		break;
	}

	if(ret < 0) {
		pr_err("vir_haptic_close fail!\n");
	}
	return ret;
}

const struct snd_soc_component_driver vir_haptic_pcm_platform = {
	.name = VIR_HAPTIC_NAME,
	.probe = vir_haptic_probe,
	.controls = vir_haptic_controls,
	.num_controls = ARRAY_SIZE(vir_haptic_controls),
	.open = vir_haptic_open,
	.write = vir_haptic_write,
	.copy = vir_haptic_copy,
	.prepare = vir_haptic_prepare,
	.pointer= vir_haptic_pointer,
	.close = vir_haptic_close,
};
EXPORT_SYMBOL_GPL(vir_haptic_pcm_platform);

static int vir_haptic_dev_probe(struct platform_device *pdev)
{
	int ret = 0;
    ret = snd_soc_register_component(&pdev->dev,
					 &vir_haptic_pcm_platform,
					 NULL,
					 0);
	if(ret < 0) {
		pr_err("snd_soc_register_component fail\n");
	}
	return ret;
}

static const struct of_device_id vir_haptic_dt_match[] = {
	{ .compatible = "mediatek,snd-vir-haptic", },
	{},
};

MODULE_DEVICE_TABLE(of, vir_haptic_dt_match);
static struct platform_driver vir_haptic_driver = {
	.driver = {
		   .name = "snd-vir-haptic",
		   .owner = THIS_MODULE,
		   .of_match_table = vir_haptic_dt_match,
	},
	.probe = vir_haptic_dev_probe,
};
module_platform_driver(vir_haptic_driver);
MODULE_DESCRIPTION("ASoC VIRHAPTIC driver");
MODULE_AUTHOR("XiaoMi Inc");
MODULE_LICENSE("GPL");