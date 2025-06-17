// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Copyright (c) 2025 Richtek Technology Corp.
 *
 * Author: Gene Chen <gene_chen@richtek.com>
 */

#include <dt-bindings/iio/adc/mediatek,mt6720_adc.h>
#include <linux/bitfield.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/regmap.h>

#include <linux/iio/buffer.h>
#include <linux/iio/iio.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>

/* MT6720 */
#define MT6720_REG_ADC_CONFG1		0x14E
#define MT6720_ZCVEN_MASK		BIT(6)
#define MT6720_MASK_VBAT_MON_EN		BIT(5)
#define MT6720_REG_ADC_CONFG2		0x14F
#define MT6720_ONESHOT_MASK		GENMASK(4, 0)
#define MT6720_ONESHOT_SHIFT		0
#define MT6720_REG_ADC_REPORT_CH	0x155
#define MT6720_MASK_ADC_REPORT_CH	GENMASK(4, 0)
#define MT6720_REG_ADC_REPORT_H		0x156

#define ADC_CONV_TIME_US		(2200 * 2)
#define ADC_POLL_TIME_US		100
#define ADC_POLL_TIMEOUT_US		1000

static const char * const mt6720_adc_labels[MT6720_ADC_MAX_CHANNEL] = {
	[MT6720_ADC_FGVBAT]		= "fg-vbat",
	[MT6720_ADC_CHGVIN]		= "chg-vin",
	[MT6720_ADC_USBDP]		= "usb-dp",
	[MT6720_ADC_VSYS]		= "vsys",
	[MT6720_ADC_VBAT]		= "vbat",
	[MT6720_ADC_IBUS]		= "ibus",
	[MT6720_ADC_IBAT]		= "ibat",
	[MT6720_ADC_USBDM]		= "usb-dm",
	[MT6720_ADC_TEMPJC]		= "temp-jc",
	[MT6720_ADC_VREFTS]		= "vref-ts",
	[MT6720_ADC_TS]			= "ts",
	[MT6720_ADC_PDVBUS]		= "pd-vbus",
	[MT6720_ADC_CC1]		= "cc1",
	[MT6720_ADC_CC2]		= "cc2",
	[MT6720_ADC_SBU1]		= "sbu1",
	[MT6720_ADC_SBU2]		= "sbu2",
	[MT6720_ADC_DIV2]		= "div2",
	[MT6720_ADC_ZCV]		= "zcv",
	[MT6720_ADC_VBATMON]		= "vbatmon (must be enabled in charger driver)",
};

struct mt6720_priv {
	struct device *dev;
	struct regmap *regmap;
	struct mutex lock;
	bool replace_chgvin;
	int zcv;
};

static int mt6720_adc_read_channel(struct mt6720_priv *priv, int chan, int *val)
{
	int ret = 0, retry_cnt = 1;
	__be16 be_val = 0;
	u32 regval;

	mutex_lock(&priv->lock);
	pm_stay_awake(priv->dev);

	switch (chan) {
	case MT6720_ADC_CHGVIN:
		chan = MT6720_ADC_PDVBUS;
		break;
	case MT6720_ADC_FGVBAT:
		dev_info(priv->dev, "%s: unknown usage fgvbat channel\n", __func__);
		goto adc_unlock;
	case MT6720_ADC_VBATMON:
		ret = regmap_read(priv->regmap, MT6720_REG_ADC_CONFG1, &regval);
		if (ret) {
			dev_info(priv->dev, "%s: Failed to read vbat_mon stat(%d)\n",
				 __func__, ret);
			goto adc_unlock;
		}
		if (!(regval & MT6720_MASK_VBAT_MON_EN)) {
			dev_info(priv->dev, "%s: vbat_mon is not enabled\n",
				 __func__);
			goto adc_unlock;
		}

		/* vbatmon should be enabled in charger first! */
		usleep_range(10000, 12000);
		retry_cnt = 50;
		*val = 0;
		goto bypass_oneshot;
	case MT6720_ADC_ZCV:
		*val = priv->zcv;
		ret = IIO_VAL_INT;
		goto adc_unlock;
	default:
		break;
	}

	ret = regmap_write(priv->regmap, MT6720_REG_ADC_CONFG2, chan << MT6720_ONESHOT_SHIFT);
	if (ret)
		goto adc_unlock;

	usleep_range(ADC_CONV_TIME_US, ADC_CONV_TIME_US * 3 / 2);

	ret = regmap_read_poll_timeout(priv->regmap, MT6720_REG_ADC_CONFG2, regval,
				       !(regval & MT6720_ONESHOT_MASK), ADC_POLL_TIME_US,
				       ADC_POLL_TIMEOUT_US);
	if (ret)
		goto adc_unlock;

bypass_oneshot:
	while (retry_cnt--) {
		/* select read report channel */
		if (chan == MT6720_ADC_VBATMON)
			ret = regmap_write_bits(priv->regmap, MT6720_REG_ADC_REPORT_CH,
						MT6720_MASK_ADC_REPORT_CH, 0);
		else
			ret = regmap_write_bits(priv->regmap, MT6720_REG_ADC_REPORT_CH,
						MT6720_MASK_ADC_REPORT_CH, chan);
		if (ret) {
			dev_info(priv->dev, "%s: Failed to select ADC report channel\n", __func__);
			goto adc_unlock;
		}

		usleep_range(1000, 1200);
		ret = regmap_raw_read(priv->regmap, MT6720_REG_ADC_REPORT_H,
				      &be_val, sizeof(be_val));
		if (ret) {
			dev_info(priv->dev, "%s, Failed to read ADC_REPORT\n", __func__);
			goto adc_unlock;
		}

		*val = be16_to_cpu(be_val);

		if (chan == MT6720_ADC_VBATMON) {
			if (*val != 0)
				break;
			usleep_range(2000, 10000);
		}
	}

	if (chan == MT6720_ADC_VBATMON) {
		if (retry_cnt <= 0)
			dev_info(priv->dev, "%s, Read vbat_mon TIMEOUT!!\n", __func__);
		else if (retry_cnt > 0 && retry_cnt <= 30)
			dev_info(priv->dev,
				 "%s, Read vbat_mon too long...remain retry cnt:%d\n",
				 __func__, retry_cnt);
	}

	ret = IIO_VAL_INT;
adc_unlock:
	pm_relax(priv->dev);
	mutex_unlock(&priv->lock);
	return ret;
}

static int mt6720_adc_read_scale(struct mt6720_priv *priv, int chan, int *val1, int *val2)
{
	if (chan >= MT6720_ADC_MAX_CHANNEL)
		return -EINVAL;

	switch (chan) {
	case MT6720_ADC_TEMPJC:
	case MT6720_ADC_ZCV:
		*val1 = 1;
		return IIO_VAL_INT;
	default:
		*val1 = 1000;
		return IIO_VAL_INT;
	}
}

static int mt6720_adc_read_offset(struct mt6720_priv *priv, int chan, int *val)
{
	/* Only for TEMPJC */
	*val = -64;
	return IIO_VAL_INT;
}

static int mt6720_adc_read_raw(struct iio_dev *indio_dev, struct iio_chan_spec const *chan,
			       int *val, int *val2, long mask)
{
	struct mt6720_priv *priv = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		return mt6720_adc_read_channel(priv, chan->channel, val);
	case IIO_CHAN_INFO_SCALE:
		return mt6720_adc_read_scale(priv, chan->channel, val, val2);
	case IIO_CHAN_INFO_OFFSET:
		return mt6720_adc_read_offset(priv, chan->channel, val);
	default:
		return -EINVAL;
	}
}

static int mt6720_adc_read_label(struct iio_dev *indio_dev,
				 struct iio_chan_spec const *chan, char *label)
{
	return sysfs_emit(label, "%s\n",
			  chan->channel >= 0 ? mt6720_adc_labels[chan->channel] : "INVALID");
}

static const struct iio_info mt6720_iio_info = {
	.read_raw = mt6720_adc_read_raw,
	.read_label = mt6720_adc_read_label,
};

#define MT6720_ADC_CHAN(_idx, _type, _extra_info) {		\
	.type = _type,						\
	.channel = MT6720_ADC_##_idx,				\
	.scan_index = MT6720_ADC_##_idx,			\
	.datasheet_name = #_idx,				\
	.scan_type =  {						\
		.sign = 'u',					\
		.realbits = 16,					\
		.storagebits = 16,				\
		.endianness = IIO_CPU,				\
	},							\
	.indexed = 1,						\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |		\
			      BIT(IIO_CHAN_INFO_SCALE) |	\
			      _extra_info,			\
}

static const struct iio_chan_spec mt6720_adc_channels[] = {
	MT6720_ADC_CHAN(FGVBAT, IIO_VOLTAGE, 0),
	MT6720_ADC_CHAN(CHGVIN, IIO_VOLTAGE, 0),
	MT6720_ADC_CHAN(USBDP, IIO_VOLTAGE, 0),
	MT6720_ADC_CHAN(VSYS, IIO_VOLTAGE, 0),
	MT6720_ADC_CHAN(VBAT, IIO_VOLTAGE, 0),
	MT6720_ADC_CHAN(IBUS, IIO_CURRENT, 0),
	MT6720_ADC_CHAN(IBAT, IIO_CURRENT, 0),
	MT6720_ADC_CHAN(USBDM, IIO_VOLTAGE, 0),
	MT6720_ADC_CHAN(TEMPJC, IIO_TEMP, BIT(IIO_CHAN_INFO_OFFSET)),
	MT6720_ADC_CHAN(VREFTS, IIO_VOLTAGE, 0),
	MT6720_ADC_CHAN(TS, IIO_VOLTAGE, 0),
	MT6720_ADC_CHAN(PDVBUS, IIO_VOLTAGE, 0),
	MT6720_ADC_CHAN(CC1, IIO_VOLTAGE, 0),
	MT6720_ADC_CHAN(CC2, IIO_VOLTAGE, 0),
	MT6720_ADC_CHAN(SBU1, IIO_VOLTAGE, 0),
	MT6720_ADC_CHAN(SBU2, IIO_VOLTAGE, 0),
	MT6720_ADC_CHAN(DIV2, IIO_VOLTAGE, 0),
	MT6720_ADC_CHAN(ZCV, IIO_VOLTAGE, 0),
	MT6720_ADC_CHAN(VBATMON, IIO_VOLTAGE, 0),
	IIO_CHAN_SOFT_TIMESTAMP(MT6720_ADC_MAX_CHANNEL)
};

static irqreturn_t mt6720_adc_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct mt6720_priv *priv = iio_priv(indio_dev);
	u16 *data;
	int64_t timestamp;
	int i = 0, bit, val = 0, ret;

	memset(&data, 0, sizeof(data));
	data = kzalloc(MT6720_ADC_MAX_CHANNEL * sizeof(u16) + sizeof(timestamp), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	for_each_set_bit(bit, indio_dev->active_scan_mask, indio_dev->masklength) {
		ret = mt6720_adc_read_channel(priv, bit, &val);
		if (ret < 0) {
			dev_err(priv->dev, "Failed to get channel %d conversion val\n", bit);
			goto out;
		}

		data[i++] = val;
	}

	iio_push_to_buffers_with_timestamp(indio_dev, &data, iio_get_time_ns(indio_dev));
out:
	iio_trigger_notify_done(indio_dev->trig);

	kfree(data);
	return IRQ_HANDLED;
}

static inline int mt6720_adc_reset(struct mt6720_priv *priv)
{
	__be16 be_val = 0;
	int ret;

	/* Select ZCV channel of report data */
	ret = regmap_write_bits(priv->regmap, MT6720_REG_ADC_REPORT_CH,
				MT6720_MASK_ADC_REPORT_CH, MT6720_ADC_ZCV);
	if (ret)
		dev_info(priv->dev, "%s, Failed to select ZCV report channel\n", __func__);

	usleep_range(1000, 1200);
	ret = regmap_raw_read(priv->regmap, MT6720_REG_ADC_REPORT_H, &be_val, sizeof(be_val));
	if (ret)
		dev_err(priv->dev, "%s, Failed to read ZCV val\n", __func__);

	priv->zcv = be16_to_cpu(be_val);
	dev_info(priv->dev, "%s, zcv = %d mV (boot voltage with first plug-in when ZCVEN enable)\n",
		 __func__, priv->zcv);

	/* Disable ZCV */
	return regmap_update_bits(priv->regmap, MT6720_REG_ADC_CONFG1, MT6720_ZCVEN_MASK, 0);
}

static int mt6720_adc_probe(struct platform_device *pdev)
{
	struct mt6720_priv *priv;
	struct regmap *regmap;
	struct iio_dev *indio_dev;
	int ret;

	regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!regmap) {
		dev_err(&pdev->dev, "Failed to get regmap\n");
		return -ENODEV;
	}

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*priv));
	if (!indio_dev)
		return -ENOMEM;

	priv = iio_priv(indio_dev);
	priv->dev = &pdev->dev;
	priv->regmap = regmap;
	mutex_init(&priv->lock);
	device_init_wakeup(&pdev->dev, true);

	ret = mt6720_adc_reset(priv);
	if (ret) {
		dev_err(&pdev->dev, "Failed to reset\n");
		return ret;
	}

	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->info = &mt6720_iio_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = mt6720_adc_channels;
	indio_dev->num_channels = ARRAY_SIZE(mt6720_adc_channels);

	ret = devm_iio_triggered_buffer_setup(&pdev->dev, indio_dev, NULL,
					      mt6720_adc_trigger_handler, NULL);
	if (ret) {
		dev_err(&pdev->dev, "Failed to allocate iio trigger buffer\n");
		return ret;
	}

	return devm_iio_device_register(&pdev->dev, indio_dev);
}

static const struct of_device_id mt6720_adc_of_match[] = {
	{ .compatible = "mediatek,mt6720-adc", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6720_adc_of_match);

static struct platform_driver mt6720_adc_driver = {
	.probe = mt6720_adc_probe,
	.driver = {
		.name = "mt6720-adc",
		.of_match_table = mt6720_adc_of_match,
	},
};
module_platform_driver(mt6720_adc_driver);

MODULE_AUTHOR("Gene Chen <gene_chen@richtek.com>");
MODULE_DESCRIPTION("MT6720 ADC Driver");
MODULE_LICENSE("GPL");
