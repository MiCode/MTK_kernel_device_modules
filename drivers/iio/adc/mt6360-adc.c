// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/completion.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include <linux/mfd/mt6360-private.h>



/* PMU register defininition */
#define MT6360_PMU_DEV_INFO			(0x00)
#define MT6360_PMU_CORE_CTRL1			(0x01)
#define MT6360_PMU_RST1				(0x02)
#define MT6360_PMU_CRCEN			(0x03)
#define MT6360_PMU_RST_PAS_CODE1		(0x04)
#define MT6360_PMU_RST_PAS_CODE2		(0x05)
#define MT6360_PMU_CORE_CTRL2			(0x06)
#define MT6360_PMU_TM_PAS_CODE1			(0x07)
#define MT6360_PMU_TM_PAS_CODE2			(0x08)
#define MT6360_PMU_TM_PAS_CODE3			(0x09)
#define MT6360_PMU_TM_PAS_CODE4			(0x0A)
#define MT6360_PMU_IRQ_IND			(0x0B)
#define MT6360_PMU_IRQ_MASK			(0x0C)
#define MT6360_PMU_IRQ_SET			(0x0D)
#define MT6360_PMU_SHDN_CTRL			(0x0E)
#define MT6360_PMU_TM_INF			(0x0F)
#define MT6360_PMU_I2C_CTRL			(0x10)
#define MT6360_PMU_CHG_CTRL1			(0x11)
#define MT6360_PMU_CHG_CTRL2			(0x12)
#define MT6360_PMU_CHG_CTRL3			(0x13)
#define MT6360_PMU_CHG_CTRL4			(0x14)
#define MT6360_PMU_CHG_CTRL5			(0x15)
#define MT6360_PMU_CHG_CTRL6			(0x16)
#define MT6360_PMU_CHG_CTRL7			(0x17)
#define MT6360_PMU_CHG_CTRL8			(0x18)
#define MT6360_PMU_CHG_CTRL9			(0x19)
#define MT6360_PMU_CHG_CTRL10			(0x1A)
#define MT6360_PMU_CHG_CTRL11			(0x1B)
#define MT6360_PMU_CHG_CTRL12			(0x1C)
#define MT6360_PMU_CHG_CTRL13			(0x1D)
#define MT6360_PMU_CHG_CTRL14			(0x1E)
#define MT6360_PMU_CHG_CTRL15			(0x1F)
#define MT6360_PMU_CHG_CTRL16			(0x20)
#define MT6360_PMU_CHG_AICC_RESULT		(0x21)
#define MT6360_PMU_DEVICE_TYPE			(0x22)
#define MT6360_PMU_QC_CONTROL1			(0x23)
#define MT6360_PMU_QC_CONTROL2			(0x24)
#define MT6360_PMU_QC30_CONTROL1		(0x25)
#define MT6360_PMU_QC30_CONTROL2		(0x26)
#define MT6360_PMU_USB_STATUS1			(0x27)
#define MT6360_PMU_QC_STATUS1			(0x28)
#define MT6360_PMU_QC_STATUS2			(0x29)
#define MT6360_PMU_CHG_PUMP			(0x2A)
#define MT6360_PMU_CHG_CTRL17			(0x2B)
#define MT6360_PMU_CHG_CTRL18			(0x2C)
#define MT6360_PMU_CHRDET_CTRL1			(0x2D)
#define MT6360_PMU_CHRDET_CTRL2			(0x2E)
#define MT6360_PMU_DPDN_CTRL			(0x2F)
#define MT6360_PMU_CHG_HIDDEN_CTRL1		(0x30)
#define MT6360_PMU_CHG_HIDDEN_CTRL2		(0x31)
#define MT6360_PMU_CHG_HIDDEN_CTRL3		(0x32)
#define MT6360_PMU_CHG_HIDDEN_CTRL4		(0x33)
#define MT6360_PMU_CHG_HIDDEN_CTRL5		(0x34)
#define MT6360_PMU_CHG_HIDDEN_CTRL6		(0x35)
#define MT6360_PMU_CHG_HIDDEN_CTRL7		(0x36)
#define MT6360_PMU_CHG_HIDDEN_CTRL8		(0x37)
#define MT6360_PMU_CHG_HIDDEN_CTRL9		(0x38)
#define MT6360_PMU_CHG_HIDDEN_CTRL10		(0x39)
#define MT6360_PMU_CHG_HIDDEN_CTRL11		(0x3A)
#define MT6360_PMU_CHG_HIDDEN_CTRL12		(0x3B)
#define MT6360_PMU_CHG_HIDDEN_CTRL13		(0x3C)
#define MT6360_PMU_CHG_HIDDEN_CTRL14		(0x3D)
#define MT6360_PMU_CHG_HIDDEN_CTRL15		(0x3E)
#define MT6360_PMU_CHG_HIDDEN_CTRL16		(0x3F)
#define MT6360_PMU_CHG_HIDDEN_CTRL17		(0x40)
#define MT6360_PMU_CHG_HIDDEN_CTRL18		(0x41)
#define MT6360_PMU_CHG_HIDDEN_CTRL19		(0x42)
#define MT6360_PMU_CHG_HIDDEN_CTRL20		(0x43)
#define MT6360_PMU_CHG_HIDDEN_CTRL21		(0x44)
#define MT6360_PMU_CHG_HIDDEN_CTRL22		(0x45)
#define MT6360_PMU_CHG_HIDDEN_CTRL23		(0x46)
#define MT6360_PMU_CHG_HIDDEN_CTRL24		(0x47)
#define MT6360_PMU_CHG_HIDDEN_CTRL25		(0x48)
#define MT6360_PMU_BC12_CTRL			(0x49)
#define MT6360_PMU_CHG_STAT			(0x4A)
#define MT6360_PMU_RESV1			(0x4B)
#define MT6360_PMU_TYPEC_OTP_TH_SEL_CODEH	(0x4E)
#define MT6360_PMU_TYPEC_OTP_TH_SEL_CODEL	(0x4F)
#define MT6360_PMU_TYPEC_OTP_HYST_TH		(0x50)
#define MT6360_PMU_TYPEC_OTP_CTRL		(0x51)
#define MT6360_PMU_ADC_BAT_DATA_H		(0x52)
#define MT6360_PMU_ADC_BAT_DATA_L		(0x53)
#define MT6360_PMU_IMID_BACKBST_ON		(0x54)
#define MT6360_PMU_IMID_BACKBST_OFF		(0x55)
#define MT6360_PMU_ADC_CONFIG			(0x56)
#define MT6360_PMU_ADC_EN2			(0x57)
#define MT6360_PMU_ADC_IDLE_T			(0x58)
#define MT6360_PMU_ADC_RPT_1			(0x5A)
#define MT6360_PMU_ADC_RPT_2			(0x5B)
#define MT6360_PMU_ADC_RPT_3			(0x5C)
#define MT6360_PMU_ADC_RPT_ORG1			(0x5D)
#define MT6360_PMU_ADC_RPT_ORG2			(0x5E)
#define MT6360_PMU_BAT_OVP_TH_SEL_CODEH		(0x5F)
#define MT6360_PMU_BAT_OVP_TH_SEL_CODEL		(0x60)
#define MT6360_PMU_CHG_CTRL19			(0x61)
#define MT6360_PMU_VDDASUPPLY			(0x62)
#define MT6360_PMU_BC12_MANUAL			(0x63)
#define MT6360_PMU_CHGDET_FUNC			(0x64)
#define MT6360_PMU_FOD_CTRL			(0x65)
#define MT6360_PMU_CHG_CTRL20			(0x66)
#define MT6360_PMU_CHG_HIDDEN_CTRL26		(0x67)
#define MT6360_PMU_CHG_HIDDEN_CTRL27		(0x68)
#define MT6360_PMU_RESV2			(0x69)
#define MT6360_PMU_USBID_CTRL1			(0x6D)
#define MT6360_PMU_USBID_CTRL2			(0x6E)
#define MT6360_PMU_USBID_CTRL3			(0x6F)
#define MT6360_PMU_FLED_CFG			(0x70)
#define MT6360_PMU_RESV3			(0x71)
#define MT6360_PMU_FLED1_CTRL			(0x72)
#define MT6360_PMU_FLED_STRB_CTRL		(0x73)
#define MT6360_PMU_FLED1_STRB_CTRL2		(0x74)
#define MT6360_PMU_FLED1_TOR_CTRL		(0x75)
#define MT6360_PMU_FLED2_CTRL			(0x76)
#define MT6360_PMU_RESV4			(0x77)
#define MT6360_PMU_FLED2_STRB_CTRL2		(0x78)
#define MT6360_PMU_FLED2_TOR_CTRL		(0x79)
#define MT6360_PMU_FLED_VMIDTRK_CTRL1		(0x7A)
#define MT6360_PMU_FLED_VMID_RTM		(0x7B)
#define MT6360_PMU_FLED_VMIDTRK_CTRL2		(0x7C)
#define MT6360_PMU_FLED_PWSEL			(0x7D)
#define MT6360_PMU_FLED_EN			(0x7E)
#define MT6360_PMU_FLED_Hidden1			(0x7F)
#define MT6360_PMU_RGB_EN			(0x80)
#define MT6360_PMU_RGB1_ISNK			(0x81)
#define MT6360_PMU_RGB2_ISNK			(0x82)
#define MT6360_PMU_RGB3_ISNK			(0x83)
#define MT6360_PMU_RGB_ML_ISNK			(0x84)
#define MT6360_PMU_RGB1_DIM			(0x85)
#define MT6360_PMU_RGB2_DIM			(0x86)
#define MT6360_PMU_RGB3_DIM			(0x87)
#define MT6360_PMU_RESV5			(0x88)
#define MT6360_PMU_RGB12_Freq			(0x89)
#define MT6360_PMU_RGB34_Freq			(0x8A)
#define MT6360_PMU_RGB1_Tr			(0x8B)
#define MT6360_PMU_RGB1_Tf			(0x8C)
#define MT6360_PMU_RGB1_TON_TOFF		(0x8D)
#define MT6360_PMU_RGB2_Tr			(0x8E)
#define MT6360_PMU_RGB2_Tf			(0x8F)
#define MT6360_PMU_RGB2_TON_TOFF		(0x90)
#define MT6360_PMU_RGB3_Tr			(0x91)
#define MT6360_PMU_RGB3_Tf			(0x92)
#define MT6360_PMU_RGB3_TON_TOFF		(0x93)
#define MT6360_PMU_RGB_Hidden_CTRL1		(0x94)
#define MT6360_PMU_RGB_Hidden_CTRL2		(0x95)
#define MT6360_PMU_RESV6			(0x97)
#define MT6360_PMU_SPARE1			(0x9A)
#define MT6360_PMU_SPARE2			(0xA0)
#define MT6360_PMU_SPARE3			(0xB0)
#define MT6360_PMU_SPARE4			(0xC0)
#define MT6360_PMU_CHG_IRQ1			(0xD0)
#define MT6360_PMU_CHG_IRQ2			(0xD1)
#define MT6360_PMU_CHG_IRQ3			(0xD2)
#define MT6360_PMU_CHG_IRQ4			(0xD3)
#define MT6360_PMU_CHG_IRQ5			(0xD4)
#define MT6360_PMU_CHG_IRQ6			(0xD5)
#define MT6360_PMU_QC_IRQ			(0xD6)
#define MT6360_PMU_FOD_IRQ			(0xD7)
#define MT6360_PMU_BASE_IRQ			(0xD8)
#define MT6360_PMU_FLED_IRQ1			(0xD9)
#define MT6360_PMU_FLED_IRQ2			(0xDA)
#define MT6360_PMU_RGB_IRQ			(0xDB)
#define MT6360_PMU_BUCK1_IRQ			(0xDC)
#define MT6360_PMU_BUCK2_IRQ			(0xDD)
#define MT6360_PMU_LDO_IRQ1			(0xDE)
#define MT6360_PMU_LDO_IRQ2			(0xDF)
#define MT6360_PMU_CHG_STAT1			(0xE0)
#define MT6360_PMU_CHG_STAT2			(0xE1)
#define MT6360_PMU_CHG_STAT3			(0xE2)
#define MT6360_PMU_CHG_STAT4			(0xE3)
#define MT6360_PMU_CHG_STAT5			(0xE4)
#define MT6360_PMU_CHG_STAT6			(0xE5)
#define MT6360_PMU_QC_STAT			(0xE6)
#define MT6360_PMU_FOD_STAT			(0xE7)
#define MT6360_PMU_BASE_STAT			(0xE8)
#define MT6360_PMU_FLED_STAT1			(0xE9)
#define MT6360_PMU_FLED_STAT2			(0xEA)
#define MT6360_PMU_RGB_STAT			(0xEB)
#define MT6360_PMU_BUCK1_STAT			(0xEC)
#define MT6360_PMU_BUCK2_STAT			(0xED)
#define MT6360_PMU_LDO_STAT1			(0xEE)
#define MT6360_PMU_LDO_STAT2			(0xEF)
#define MT6360_PMU_CHG_MASK1			(0xF0)
#define MT6360_PMU_CHG_MASK2			(0xF1)
#define MT6360_PMU_CHG_MASK3			(0xF2)
#define MT6360_PMU_CHG_MASK4			(0xF3)
#define MT6360_PMU_CHG_MASK5			(0xF4)
#define MT6360_PMU_CHG_MASK6			(0xF5)
#define MT6360_PMU_QC_MASK			(0xF6)
#define MT6360_PMU_FOD_MASK			(0xF7)
#define MT6360_PMU_BASE_MASK			(0xF8)
#define MT6360_PMU_FLED_MASK1			(0xF9)
#define MT6360_PMU_FLED_MASK2			(0xFA)
#define MT6360_PMU_FAULTB_MASK			(0xFB)
#define MT6360_PMU_BUCK1_MASK			(0xFC)
#define MT6360_PMU_BUCK2_MASK			(0xFD)
#define MT6360_PMU_LDO_MASK1			(0xFE)
#define MT6360_PMU_LDO_MASK2			(0xFF)
#define MT6360_PMU_MAXREG			(MT6360_PMU_LDO_MASK2)

/* CHG_CTRL3 0x13 */
#define MT6360_AICR_MASK	(0xFC)
#define MT6360_AICR_SHFT	(2)
#define MT6360_AICR_400MA	(0x6)
/* ADC_CONFIG 0x56 */
#define MT6360_ADCEN_SHFT	(7)
/* ADC_RPT_1 0x5A */
#define MT6360_PREFERCH_MASK	(0xF0)
#define MT6360_PREFERCH_SHFT	(4)
#define MT6360_RPTCH_MASK	(0x0F)

enum {
	MT6360_CHAN_USBID = 0,
	MT6360_CHAN_VBUSDIV5,
	MT6360_CHAN_VBUSDIV2,
	MT6360_CHAN_VSYS,
	MT6360_CHAN_VBAT,
	MT6360_CHAN_IBUS,
	MT6360_CHAN_IBAT,
	MT6360_CHAN_CHG_VDDP,
	MT6360_CHAN_TEMP_JC,
	MT6360_CHAN_VREF_TS,
	MT6360_CHAN_TS,
	MT6360_CHAN_MAX,
};

struct mt6360_adc_info {
	struct device *dev;
	struct regmap *regmap;
	struct task_struct *scan_task;
	struct completion adc_complete;
	struct mutex adc_lock;
	ktime_t last_off_timestamps[MT6360_CHAN_MAX];
	int irq;
};

static inline int mt6360_adc_val_converte(int val, int multiplier,
					   int offset, int divisor)
{
	/* assume val = ((val * multiplier) + offset) / divisor */
	return ((val * multiplier) + offset) / divisor;
}

static int mt6360_adc_get_process_val(struct mt6360_adc_info *info,
				      int chan_idx, int *val)
{
	unsigned int regval = 0;
	int ret;
	const struct converter {
		int multiplier;
		int offset;
		int divisor;
	} adc_converter[MT6360_CHAN_MAX] = {
		{ 1250, 0, 1}, /* USBID */
		{ 6250, 0, 1}, /* VBUSDIV5 */
		{ 2500, 0, 1}, /* VBUSDIV2 */
		{ 1250, 0, 1}, /* VSYS */
		{ 1250, 0, 1}, /* VBAT */
		{ 2500, 0, 1}, /* IBUS */
		{ 2500, 0, 1}, /* IBAT */
		{ 1250, 0, 1}, /* CHG_VDDP */
		{ 105, -8000, 100}, /* TEMP_JC */
		{ 1250, 0, 1}, /* VREF_TS */
		{ 1250, 0, 1}, /* TS */
	}, sp_ibus_adc_converter = { 1900, 0, 1 }, *sel_converter;

	if (chan_idx < 0 || chan_idx >= MT6360_CHAN_MAX)
		return -EINVAL;
	sel_converter = adc_converter + chan_idx;
	if (chan_idx == MT6360_CHAN_IBUS) {
		/* ibus chan will be affected by aicr config */
		/* if aicr < 400, apply the special ibus converter */
		ret = regmap_read(info->regmap, MT6360_PMU_CHG_CTRL3, &regval);
		if (ret < 0)
			return ret;
		regval = (regval & MT6360_AICR_MASK) >> MT6360_AICR_SHFT;
		if (regval < MT6360_AICR_400MA)
			sel_converter = &sp_ibus_adc_converter;
	}
	*val = mt6360_adc_val_converte(*val, sel_converter->multiplier,
				 sel_converter->offset, sel_converter->divisor);
	return 0;
}

static int mt6360_adc_read_raw(struct iio_dev *iio_dev,
			       const struct iio_chan_spec *chan,
			       int *val, int *val2, long mask)
{
	struct mt6360_adc_info *mai = iio_priv(iio_dev);
	long timeout;
	u8 tmp[2], rpt[3];
	ktime_t start_t, predict_end_t;
	int ret;

	dev_dbg(&iio_dev->dev, "%s: channel [%d] s\n", __func__, chan->channel);
	mutex_lock(&mai->adc_lock);
	/* select preferred channel that we want */
	ret = regmap_update_bits(mai->regmap,
				 MT6360_PMU_ADC_RPT_1, MT6360_PREFERCH_MASK,
				 chan->channel << MT6360_PREFERCH_SHFT);
	if (ret < 0)
		goto err_adc_init;
	/* enable adc channel we want and adc_en */
	memset(tmp, 0, sizeof(tmp));
	tmp[0] |= BIT(MT6360_ADCEN_SHFT);
	tmp[(chan->channel / 8) ? 0 : 1] |= BIT(chan->channel % 8);
	ret = regmap_bulk_write(mai->regmap,
				MT6360_PMU_ADC_CONFIG, tmp, sizeof(tmp));
	if (ret < 0)
		goto err_adc_init;
	start_t = ktime_get();
	predict_end_t = ktime_add_ms(
				   mai->last_off_timestamps[chan->channel], 50);
	if (ktime_after(start_t, predict_end_t))
		predict_end_t = ktime_add_ms(start_t, 25);
	else
		predict_end_t = ktime_add_ms(start_t, 75);
	enable_irq(mai->irq);
retry:
	reinit_completion(&mai->adc_complete);
	/* wait for conversion to complete */
	timeout = wait_for_completion_interruptible_timeout(
				     &mai->adc_complete, msecs_to_jiffies(200));
	if (timeout == 0) {
		ret = -ETIMEDOUT;
		goto err_adc_conv;
	} else if (timeout < 0) {
		ret = -EINTR;
		goto err_adc_conv;
	}
	memset(rpt, 0, sizeof(rpt));
	ret = regmap_bulk_read(mai->regmap,
			       MT6360_PMU_ADC_RPT_1, rpt, sizeof(rpt));
	if (ret < 0)
		goto err_adc_conv;
	/* get report channel */
	if ((rpt[0] & MT6360_RPTCH_MASK) != chan->channel) {
		dev_dbg(&iio_dev->dev,
			"not wanted channel report [%02x]\n", rpt[0]);
		goto retry;
	}
	if (!ktime_after(ktime_get(), predict_end_t)) {
		dev_dbg(&iio_dev->dev, "time is not after 26ms chan_time\n");
		goto retry;
	}
	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		*val = (rpt[1] << 8) | rpt[2];
		break;
	case IIO_CHAN_INFO_PROCESSED:
		*val = (rpt[1] << 8) | rpt[2];
		ret = mt6360_adc_get_process_val(mai, chan->channel, val);
		if (ret < 0)
			goto err_adc_conv;
		break;
	default:
		break;
	}
	ret = IIO_VAL_INT;
err_adc_conv:
	disable_irq(mai->irq);
	/* whatever disable all channel and keep adc_en*/
	memset(tmp, 0, sizeof(tmp));
	tmp[0] |= BIT(MT6360_ADCEN_SHFT);
	regmap_bulk_write(mai->regmap, MT6360_PMU_ADC_CONFIG, tmp, sizeof(tmp));
	mai->last_off_timestamps[chan->channel] = ktime_get();
	/* set prefer channel to 0xf */
	regmap_update_bits(mai->regmap, MT6360_PMU_ADC_RPT_1,
			   MT6360_PREFERCH_MASK, 0xF << MT6360_PREFERCH_SHFT);
err_adc_init:
	mutex_unlock(&mai->adc_lock);
	dev_dbg(&iio_dev->dev, "%s: channel [%d] e\n", __func__, chan->channel);
	return ret;
}

static const struct iio_info mt6360_adc_iio_info = {
	.read_raw = mt6360_adc_read_raw,
};

#define MT6360_ADC_CHAN(_idx, _type) {				\
	.type = _type,						\
	.channel = MT6360_CHAN_##_idx,				\
	.scan_index = MT6360_CHAN_##_idx,			\
	.scan_type =  {						\
		.sign = 's',					\
		.realbits = 32,					\
		.storagebits = 32,				\
		.shift = 0,					\
		.endianness = IIO_CPU,				\
	},							\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |		\
				BIT(IIO_CHAN_INFO_PROCESSED),	\
	.datasheet_name = #_idx,					\
	.indexed = 1,						\
}

static const struct iio_chan_spec mt6360_adc_channels[] = {
	MT6360_ADC_CHAN(USBID, IIO_VOLTAGE),
	MT6360_ADC_CHAN(VBUSDIV5, IIO_VOLTAGE),
	MT6360_ADC_CHAN(VBUSDIV2, IIO_VOLTAGE),
	MT6360_ADC_CHAN(VSYS, IIO_VOLTAGE),
	MT6360_ADC_CHAN(VBAT, IIO_VOLTAGE),
	MT6360_ADC_CHAN(IBUS, IIO_CURRENT),
	MT6360_ADC_CHAN(IBAT, IIO_CURRENT),
	MT6360_ADC_CHAN(CHG_VDDP, IIO_VOLTAGE),
	MT6360_ADC_CHAN(TEMP_JC, IIO_TEMP),
	MT6360_ADC_CHAN(VREF_TS, IIO_VOLTAGE),
	MT6360_ADC_CHAN(TS, IIO_VOLTAGE),
	IIO_CHAN_SOFT_TIMESTAMP(MT6360_CHAN_MAX),
};

static irqreturn_t mt6360_pmu_adc_donei_handler(int irq, void *data)
{
	struct mt6360_adc_info *mai = iio_priv(data);

	dev_dbg(mai->dev, "%s\n", __func__);
	complete(&mai->adc_complete);
	return IRQ_HANDLED;
}

static int mt6360_adc_scan_task_threadfn(void *data)
{
	struct iio_dev *indio_dev = data;
	int channel_vals[MT6360_CHAN_MAX];
	int i, bit, var = 0;
	int ret;

	dev_dbg(&indio_dev->dev, "%s ++\n", __func__);
	while (!kthread_should_stop()) {
		memset(channel_vals, 0, sizeof(channel_vals));
		i = 0;
		for_each_set_bit(bit, indio_dev->active_scan_mask,
				 indio_dev->masklength) {
			ret = mt6360_adc_read_raw(indio_dev,
						  mt6360_adc_channels + bit,
						  &var, NULL,
						  IIO_CHAN_INFO_PROCESSED);
			if (ret < 0)
				dev_err(&indio_dev->dev, "get adc[%d] fail\n", bit);
			channel_vals[i++] = var;
			if (kthread_should_stop())
				goto out;
		}
		iio_push_to_buffers_with_timestamp(indio_dev, channel_vals,
						   iio_get_time_ns(indio_dev));
	}
out:
	dev_dbg(&indio_dev->dev, "%s --\n", __func__);
//do_exit(0);
	return 0;
}

static int mt6360_adc_iio_post_enable(struct iio_dev *iio_dev)
{
	struct mt6360_adc_info *mai = iio_priv(iio_dev);

	dev_dbg(&iio_dev->dev, "%s ++\n", __func__);
	mai->scan_task = kthread_run(mt6360_adc_scan_task_threadfn, iio_dev,
				     "scan_thread.%s", dev_name(&iio_dev->dev));
	dev_dbg(&iio_dev->dev, "%s --\n", __func__);
	return PTR_ERR_OR_ZERO(mai->scan_task);
}

static int mt6360_adc_iio_pre_disable(struct iio_dev *iio_dev)
{
	struct mt6360_adc_info *mai = iio_priv(iio_dev);

	dev_dbg(&iio_dev->dev, "%s ++\n", __func__);
	if (mai->scan_task) {
		kthread_stop(mai->scan_task);
		mai->scan_task = NULL;
	}
	dev_dbg(&iio_dev->dev, "%s --\n", __func__);
	return 0;
}

static const struct iio_buffer_setup_ops mt6360_adc_iio_setup_ops = {
	.postenable = mt6360_adc_iio_post_enable,
	.predisable = mt6360_adc_iio_pre_disable,
};
static int mt6360_adc_iio_device_register(struct iio_dev *indio_dev)
{
	struct mt6360_adc_info *mai = iio_priv(indio_dev);
	//struct iio_buffer *buffer;
	int ret;

	dev_dbg(mai->dev, "%s ++\n", __func__);
	indio_dev->name = dev_name(mai->dev);
	indio_dev->dev.parent = mai->dev;
	indio_dev->dev.of_node = mai->dev->of_node;
	indio_dev->info = &mt6360_adc_iio_info;
	indio_dev->channels = mt6360_adc_channels;
	indio_dev->num_channels = ARRAY_SIZE(mt6360_adc_channels);
	indio_dev->modes = INDIO_DIRECT_MODE | INDIO_BUFFER_SOFTWARE;
	indio_dev->setup_ops = &mt6360_adc_iio_setup_ops;
	ret = devm_iio_kfifo_buffer_setup_ext(mai->dev,indio_dev,NULL,NULL);
	if (ret < 0) {
		dev_err(mai->dev, "iio device  attach buffer  fail\n");
		return ret;
	}
	//iio_device_attach_buffer(indio_dev, buffer);
	ret = devm_iio_device_register(mai->dev, indio_dev);
	if (ret < 0) {
		dev_err(mai->dev, "iio device  register fail\n");
		return ret;
	}
	dev_dbg(mai->dev, "%s --\n", __func__);
	return 0;
}

static inline int mt6360_adc_reset(struct mt6360_adc_info *info)
{
	u8 tmp[3] = {0x80, 0, 0};
	ktime_t all_off_time;
	int i;

	all_off_time = ktime_get();
	for (i = 0; i < MT6360_CHAN_MAX; i++)
		info->last_off_timestamps[i] = all_off_time;
	/* enable adc_en, clear adc_chn_en/zcv/en/adc_wait_t/adc_idle_t */
	return regmap_bulk_write(info->regmap,
				 MT6360_PMU_ADC_CONFIG, tmp, sizeof(tmp));
}

static int mt6360_adc_probe(struct platform_device *pdev)
{
	struct mt6360_adc_info *mai;
	struct iio_dev *indio_dev;
	int ret;

	dev_dbg(&pdev->dev, "%s\n", __func__);
	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*mai));
	if (!indio_dev)
		return -ENOMEM;
	mai = iio_priv(indio_dev);
	mai->dev = &pdev->dev;
	init_completion(&mai->adc_complete);
	mutex_init(&mai->adc_lock);
	platform_set_drvdata(pdev, indio_dev);

	/* get parent regmap */
	mai->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!mai->regmap) {
		dev_err(&pdev->dev, "Faled to get parent regmap\n");
		return -ENODEV;
	}
	/* first reset all channels before use */
	ret = mt6360_adc_reset(mai);
	if (ret < 0) {
		dev_err(&pdev->dev, "adc reset fail\n");
		return ret;
	}
	/* adc iio device register */
	ret = mt6360_adc_iio_device_register(indio_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "iio dev register fail\n");
		return ret;
	}
	/* irq register */
	mai->irq = platform_get_irq_byname(pdev, "adc_donei");
	if (mai->irq < 0) {
		dev_err(&pdev->dev, "Failed to get adc_done irq\n");
		return mai->irq;
	}
	/* default disable adc_donei irq by default */
	irq_set_status_flags(mai->irq, IRQ_NOAUTOEN);
	ret = devm_request_threaded_irq(&pdev->dev, mai->irq, NULL,
					mt6360_pmu_adc_donei_handler,
					IRQF_TRIGGER_FALLING, "adc_donei",
					platform_get_drvdata(pdev));
	if (ret < 0) {
		dev_err(&pdev->dev, "Fail to register adc_done irq\n");
		return ret;
	}
	dev_info(&pdev->dev, "Successfully probed\n");
	return 0;
}

static int mt6360_adc_remove(struct platform_device *pdev)
{
	struct mt6360_adc_info *mai = platform_get_drvdata(pdev);

	if (mai->scan_task)
		kthread_stop(mai->scan_task);
	return 0;
}

static const struct of_device_id __maybe_unused mt6360_adc_of_id[] = {
	{ .compatible = "mediatek,mt6360_adc", },
	{},
};
MODULE_DEVICE_TABLE(of, mt6360_adc_of_id);

static const struct platform_device_id mt6360_adc_id[] = {
	{ "mt6360_adc", 0 },
	{},
};
MODULE_DEVICE_TABLE(platform, mt6360_adc_id);

static struct platform_driver mt6360_adc_driver = {
	.driver = {
		.name = "mt6360_adc",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mt6360_adc_of_id),
	},
	.probe = mt6360_adc_probe,
	.remove = mt6360_adc_remove,
	.id_table = mt6360_adc_id,
};
module_platform_driver(mt6360_adc_driver);

MODULE_AUTHOR("CY_Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6360 ADC Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
