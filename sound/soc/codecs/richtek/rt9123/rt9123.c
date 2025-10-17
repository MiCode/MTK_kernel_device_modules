// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 */
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#define RT9123_REG_DEVID		0x0
#define RT9123_REG_SYSCTL		0x1
#define RT9123_EN_ON_MASK		GENMASK(12, 12)
#define RT9123_EN_ON_SFT		12
#define RT9123_MUTE_MASK		GENMASK(14, 14)
#define RT9123_MUTE_SFT			14

#define RT9123_REG_I2S_CFG		0x2
#define RT9123_CH_SEL_MASK		GENMASK(5, 4)
#define RT9123_CH_SEL_SFT		4
#define RT9123_I2SFMT_MASK		GENMASK(11, 8)
#define RT9123_I2SFMT_SFT		8
#define RT9123_AUD_BITS_MASK	GENMASK(14, 12)
#define RT9123_AUD_BITS_SFT		12
#define RT9123_BCK_MODE_MASK	GENMASK(3, 0)
#define RT9123_BCK_MODE_SFT		0

#define RT9123_REG_MODE_CTRL	0x20

/* AUD_FMT */
#define RT9123_CFG_FMT_I2S		0
#define RT9123_CFG_FMT_LEFTJ		1
#define RT9123_CFG_FMT_RIGHTJ		2
#define RT9123_CFG_FMT_DSPA		3
#define RT9123_CFG_FMT_DSPB		7
#define RT9123_L_CH_SEL_SFT		1
#define RT9123_R_CH_SEL_SFT		2
#define RT9123_AUD_16BITS		0
#define RT9123_AUD_20BITS		1
#define RT9123_AUD_24BITS		2
#define RT9123_AUD_32BITS		3

/* BCK_MODE */
#define RT9123_BCK_32FS		0
#define RT9123_BCK_48FS		1
#define RT9123_BCK_64FS		2
#define RT9123_BCK_128FS	4
#define RT9123_BCK_256FS	6
/* L R Select */
#define RT9123_L_CH_SEL_SFT	1
#define RT9123_R_CH_SEL_SFT	2

#define RT9123_CHIPON_WAITMS	10

bool external_codec_done = false;
EXPORT_SYMBOL_GPL(external_codec_done);

enum rt9123_type {
	RT9123_L,
	RT9123_R,
};

struct rt9123_data {
	int addr;
	struct device *dev;
	struct regmap *regmap;
	struct gpio_desc *enable_gpio;
};

static const struct regmap_config rt9123_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 16,
	.max_register = 0x36,
	.cache_type = REGCACHE_NONE,
};

static int rt9123_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_component *comp = dai->component;
	struct rt9123_data *priv = snd_soc_component_get_drvdata(comp);
	unsigned int format;
	int ret = 0;

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		format = RT9123_CFG_FMT_I2S;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		format = RT9123_CFG_FMT_LEFTJ;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		format = RT9123_CFG_FMT_RIGHTJ;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		format = RT9123_CFG_FMT_DSPA;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		format = RT9123_CFG_FMT_DSPB;
		break;
	default:
		dev_info(dai->dev, "%s, Unknown dai format 0x%x\n", __func__, fmt);
		return -EINVAL;
	}

	ret = regmap_update_bits(priv->regmap, RT9123_REG_I2S_CFG,
			   RT9123_I2SFMT_MASK,
			   format << RT9123_I2SFMT_SFT);
	if (ret)
		dev_info(dai->dev, "%s, Failed to update fmt, ret %d\n", __func__, ret);

	return ret;
}

static int rt9123_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct snd_soc_component *comp = dai->component;
	struct rt9123_data *priv = snd_soc_component_get_drvdata(comp);
	unsigned int width = params_width(params);
	unsigned int param_width;
	int ret = 0;

	dev_info(dai->dev, "%s, width %d, rate %d, ch %d\n",
			__func__,
			params_width(params),
			params_rate(params),
			params_channels(params));

	switch (width) {
	case 16:
		param_width = RT9123_AUD_16BITS;
		break;
	case 20:
		param_width = RT9123_AUD_20BITS;
		break;
	case 24:
		param_width = RT9123_AUD_24BITS;
		break;
	case 32:
		param_width = RT9123_AUD_32BITS;
		break;
	default:
		dev_info(dai->dev, "%s, Unsupported width %d\n", __func__, width);
		return -EINVAL;
	}

	ret = regmap_update_bits(priv->regmap, RT9123_REG_I2S_CFG,
			   RT9123_AUD_BITS_MASK,
			   param_width << RT9123_AUD_BITS_SFT);
	if (ret)
		dev_info(dai->dev, "%s, Failed to update width(%d)\n", __func__, param_width);

	ret = regmap_update_bits(priv->regmap, RT9123_REG_MODE_CTRL,
			   RT9123_BCK_MODE_MASK,
			   RT9123_BCK_48FS);
	if (ret)
		dev_info(dai->dev, "%s, Failed to update bck, ret %d\n", __func__, ret);

	return ret;
}

static const struct snd_soc_dai_ops rt9123_dai_ops = {
	.set_fmt = rt9123_set_fmt,
	.hw_params = rt9123_hw_params,
};

static struct snd_soc_dai_driver rt9123_dai[] = {
{
		.name = "rt9123_aif",
		.playback = {
			.stream_name = "AIF Playback",
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE |
						SNDRV_PCM_FMTBIT_S32_LE,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &rt9123_dai_ops,
	},
};

static const char * const mute_control_text[] = {
	"Unmute", "Mute"
};

static const struct soc_enum mute_control_enum =
	SOC_ENUM_SINGLE(RT9123_REG_SYSCTL, 14, ARRAY_SIZE(mute_control_text),
			mute_control_text);
static const struct snd_kcontrol_new rt9123_snd_controls[] = {
	// SOC_SINGLE_TLV("RT9123 MS Volume", RT9123_REG_VOL, 0, 2047, 1, digital_tlv),
	// SOC_SINGLE_TLV("SPK Gain Volume", RT9123_REG_SYSCTL, 0, 7, 0, classd_tlv),
	SOC_ENUM("RT9123 Mute", mute_control_enum),
};

static const struct snd_soc_dapm_widget rt9123_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("AIF_RX", "AIF Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_MIXER("DMIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("AMPON", RT9123_REG_SYSCTL, 12, 0, NULL, 0),
	SND_SOC_DAPM_DAC("DAC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_PGA("SPK PA", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_OUTPUT("SPK"),
};

static const struct snd_soc_dapm_route rt9123_dapm_routes[] = {
	{ "DMIX", NULL, "AIF_RX" },

	{ "DAC", NULL, "AMPON" },
	{ "DAC", NULL, "DMIX" },
	{ "SPK PA", NULL, "DAC" },
	{ "SPK", NULL, "SPK PA" },
};

static int rt9123_codec_probe(struct snd_soc_component *comp)
{
	int ret = 0;
	unsigned int i2s_ch_sel = 0;
	struct rt9123_data *priv = snd_soc_component_get_drvdata(comp);

	pm_runtime_get_sync(comp->dev);

	// config L/R channel
	switch (priv->addr) {
	case 0x5c:
		i2s_ch_sel = RT9123_L_CH_SEL_SFT;
		break;
	case 0x5e:
		i2s_ch_sel = RT9123_R_CH_SEL_SFT;
		break;
	default:
		dev_info(priv->dev, "%s, invalid addr(0x%x)\n", __func__, priv->addr);
		return -ENXIO;
	}

	ret = regmap_update_bits(priv->regmap, RT9123_REG_I2S_CFG,
			   RT9123_CH_SEL_MASK,
			   i2s_ch_sel << RT9123_CH_SEL_SFT);

	dev_info(priv->dev, "%s, 0x%x done, ret %d\n", __func__, priv->addr, ret);

	pm_runtime_mark_last_busy(comp->dev);
	pm_runtime_put(comp->dev);

	return ret;
}

static int rt9123_codec_suspend(struct snd_soc_component *comp)
{
	return pm_runtime_force_suspend(comp->dev);
}

static int rt9123_codec_resume(struct snd_soc_component *comp)
{
	return pm_runtime_force_resume(comp->dev);
}

static const struct snd_soc_component_driver rt9123_component_driver = {
	.probe = rt9123_codec_probe,
	.suspend = rt9123_codec_suspend,
	.resume = rt9123_codec_resume,
	.controls = rt9123_snd_controls,
	.num_controls = ARRAY_SIZE(rt9123_snd_controls),
	.dapm_widgets = rt9123_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(rt9123_dapm_widgets),
	.dapm_routes = rt9123_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(rt9123_dapm_routes),
};

static int rt9123_check_device_id(struct rt9123_data *priv)
{
	unsigned int devid;
	int ret;

	ret = regmap_read(priv->regmap, RT9123_REG_DEVID, &devid);
	if (ret)
		return ret;

	dev_info(priv->dev, "DEVID  [0x%0x]\n", devid);
	return 0;
}

static int rt9123_i2c_probe(struct i2c_client *i2c)
{
	int ret = 0;
	struct rt9123_data *priv;

	priv = devm_kzalloc(&i2c->dev, sizeof(struct rt9123_data), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->addr = i2c->addr;
	priv->dev = &i2c->dev;
	priv->enable_gpio = devm_gpiod_get_optional(&i2c->dev, "rt,enable", GPIOD_OUT_HIGH);
	if (IS_ERR(priv->enable_gpio))
		dev_info(&i2c->dev, "%s, Failed to get enable gpio\n", __func__);

	priv->regmap = devm_regmap_init_i2c(i2c, &rt9123_i2c_regmap);
	if (IS_ERR(priv->regmap)) {
		ret = PTR_ERR(priv->regmap);
		dev_info(&i2c->dev, "%s, Failed to init regmap\n", __func__);
		return ret;
	}

	i2c_set_clientdata(i2c, priv);

	if (priv->enable_gpio) {
		gpiod_set_value(priv->enable_gpio, 1);
		msleep(RT9123_CHIPON_WAITMS);
	}

	ret = rt9123_check_device_id(priv);
	if (ret) {
		dev_info(&i2c->dev, "%s, Failed to check devind\n", __func__);
		return ret;
	}

	pm_runtime_set_autosuspend_delay(&i2c->dev, 1000);
	pm_runtime_use_autosuspend(&i2c->dev);
	pm_runtime_set_active(&i2c->dev);
	pm_runtime_mark_last_busy(&i2c->dev);
	pm_runtime_enable(&i2c->dev);

	ret = devm_snd_soc_register_component(&i2c->dev,
			&rt9123_component_driver, rt9123_dai, ARRAY_SIZE(rt9123_dai));

	external_codec_done = true;

	dev_info(&i2c->dev, "%s 0x%x done\n", __func__, i2c->addr);

	return ret;
}

static void rt9123_i2c_remove(struct i2c_client *i2c)
{
	pm_runtime_disable(&i2c->dev);
	pm_runtime_set_suspended(&i2c->dev);
}


static int __maybe_unused rt9123_runtime_suspend(struct device *dev)
{
	struct rt9123_data *priv = dev_get_drvdata(dev);

	if (priv && priv->enable_gpio)
		gpiod_set_value(priv->enable_gpio, 0);

	return 0;
}

static int __maybe_unused rt9123_runtime_resume(struct device *dev)
{
	struct rt9123_data *priv = dev_get_drvdata(dev);

	if (priv && priv->enable_gpio) {
		gpiod_set_value(priv->enable_gpio, 1);
		msleep(RT9123_CHIPON_WAITMS);
	}

	return 0;
}

static const struct dev_pm_ops rt9123_pm_ops = {
	SET_RUNTIME_PM_OPS(rt9123_runtime_suspend, rt9123_runtime_resume, NULL)
};

#if defined(CONFIG_OF)
static const struct of_device_id rt9123_of_id[] = {
	{ .compatible = "richtek,rt9123",},
	{},
};
#endif

static const struct i2c_device_id rt9123_i2c_id[] = {
	{"rt9123", RT9123_L},
	{"rt9123", RT9123_R},
	{},
};

static struct i2c_driver rt9123_i2c_driver = {
	.driver = {
		.name = "rt9123",
#if defined(CONFIG_OF)
		.of_match_table = of_match_ptr(rt9123_of_id),
#endif
		.pm = &rt9123_pm_ops,
	},
	.probe = rt9123_i2c_probe,
	.remove = rt9123_i2c_remove,
	.id_table = rt9123_i2c_id,
};

module_i2c_driver(rt9123_i2c_driver);

MODULE_DESCRIPTION("RT9123 Audio Amplifier Driver");
MODULE_LICENSE("GPL");
