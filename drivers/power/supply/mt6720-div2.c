// SPDX-License-Identifier: GPL-2.0-only
/*
 * mt6720-div2.c -- Mediatek MT6720 devide by 2 Driver
 *
 * Copyright (c) 2025 MediaTek Inc.
 * Copyright (c) 2025 Richtek Technology Corp.
 *
 * Author: SHIH CHIA CHANG <jeff_chang@richtek.com>
 */

#include <dt-bindings/power/mtk-charger.h>
#include <linux/iio/consumer.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/linear_range.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/sysfs.h>
#include <linux/util_macros.h>
#include <linux/workqueue.h>

#include "charger_class.h"

#define MT6720_REG_CHG_WDT	0x114
#define MT6720_REG_CHGCTL1	0x200
#define MT6720_REG_CHGCTL2	0x201
#define MT6720_REG_CHGCTL3	0x203
#define MT6720_REG_PDEN		0x205
#define MT6720_REG_VBUSOVP	0x206
#define MT6720_REG_IBUSOCP	0x207
#define MT6720_REG_VBAT_SNS_OVP	0x208
#define MT6720_REG_IBUSUCP_TO	0x25D
#define MT6720_REG_OTHER1	0x25E

#define MT6720_REG_DIV2_STAT1	0x80
#define MT6720_REG_DIV2_STAT2	0x81
#define MT6720_REG_DIV2_STAT3	0x82

/* STAT1 */
#define MT6720_STAT_VBAT_OVP_MSK		BIT(4)
#define MT6720_STAT_IBUS_OCP_MSK		BIT(2)
#define MT6720_STAT_IBUS_UCP_FALL_MASK		BIT(0)

/* STAT2 */
#define MT6720_STAT_IBUS_OCP_H_MASK		BIT(5)
#define MT6720_STAT_VBUS_LOW_HIGH_ERR_MSK	GENMASK(2, 1)
#define MT6720_STAT_VAC_POR_MSK			BIT(0)

/* STAT3 */
#define MT6720_STAT_WDT_MSK		BIT(4)
#define MT6720_STAT_VAC_UVLO_MSK	BIT(4)
#define MT6720_STAT_VBUS_UVLO_MSK	BIT(3)
#define MT6720_STAT_IBUS_UCP_TO_MSK	BIT(2)
#define MT6720_STAT_CFLY_DIAG_MASK	BIT(0)

#define MT6720_DIV2_EVENT_QUEUE_TIME	5
static unsigned int mod_time = MT6720_DIV2_EVENT_QUEUE_TIME;
module_param(mod_time, uint, 0644);

static int test_ibusocp = 5000000;	//TODO
module_param(test_ibusocp, int, 0644);

static int rt_err_probe(struct device *dev, int ret, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	dev_info(dev, "error %pe: %pV", ERR_PTR(ret), &vaf);
	va_end(args);

	return ret;
}

enum mt6720_adc_chan {
	ADC_VBUS = 0,
	ADC_VBAT,
	ADC_IBUS,
	ADC_IBAT,
	ADC_TEMPJC,
	ADC_VOUT,
	ADC_MAX_CHANNEL
};

enum {
	MT6720_IRQ_DIV2_VAC_PD,
	MT6720_IRQ_DIV2_VBUS_PD,
	MT6720_IRQ_DIV2_VBAT_OVP,
	MT6720_IRQ_DIV2_IBUS_OCP,
	MT6720_IRQ_DIV2_IBUS_UCP_RISE,
	MT6720_IRQ_DIV2_IBUS_UCP_FALL,
	MT6720_IRQ_DIV2_IBUS_OCP_H,
	MT6720_IRQ_DIV2_VBUS_LOW_ERR,
	MT6720_IRQ_DIV2_VBUS_HIGH_ERR,
	MT6720_IRQ_DIV2_VAC_POR,
	MT6720_IRQ_DIV2_VBUS_INSERT,
	MT6720_IRQ_DIV2_VBAT_INSERT,
	MT6720_IRQ_DIV2_WDT,
	MT6720_IRQ_DIV2_VAC_UVLO,
	MT6720_IRQ_DIV2_VBUS_UVLO,
	MT6720_IRQ_DIV2_IBUS_UCP_TO,
	MT6720_IRQ_DIV2_CON_SWITCHING,
	MT6720_IRQ_DIV2_CFLY_DIAG,
	MT6720_IRQ_DIV2_MAX,
};

struct mt6720_div2_field {
	const char *name;
	const struct linear_range *range;
	const struct reg_field field;
	bool inited;
};

enum mt6720_fields {
	/* MT6720_REG_CHG_WDT 0x114 */
	F_BUCK_EN = 0,
	/*  MT6720_REG_CHGCTL1	0x200 */
	F_CHG_EN, F_OPERATION_MODE, F_WDT_CNT_RST, F_WDT_DIS, F_WDT_TMR,
	/* MT6720_REG_CHGCTL2	0x201 */
	F_FSW_SET, F_FREQ_SHIFT,
	/* MT6720_REG_CHGCTL3	0x203 */
	F_PROTECTION_CTRL,
	/* MT6720_REG_PDEN	0x205 */
	F_VAC_PD_EN, F_VBUS_PD_EN,
	/* MT6720_REG_VBUSOVP	0x206 */
	F_VBUS_OVP_EN, F_VBUSOVP,
	/* MT6720_REG_IBUSOCP	0x207 */
	F_IBUS_UCP_EN, F_IBUS_OCP_EN, F_IBUSOCP,
	/* MT6720_REG_VBAT_SNS_OVP	0x208 */
	F_VBAT_SNS_OVP_EN, F_VBAT_SNS_OVP,
	/* MT6720_REG_IBUSUCP_TO	0x25D */
	F_IBUS_UCP_TO, F_IBUS_FALL_DEGLITCH, F_IBUS_RISE_DEGLITCH,
	/* MT6720_REG_OTHER1	0x25E */
	F_VBUS_LOW_ERR_EN, F_VBUS_HIGH_ERR_EN, F_VBAT_OVP_EN, F_VAC_POR_CHG,
	/* MT6720_REG_DIV2_STAT2	0x81 */
	F_VBUS_LOW_ERR, F_VBUS_HIGH_ERR,
	/* MT6720_REG_DIV2_STAT3	0x82 */
	F_VBUS_INSERT, F_SWITCHING,
	F_MAX_FIELD
};

enum mt6720_ranges {
	MT6720_RANGE_F_FSW_SET,
	MT6720_RANGE_F_VBAT_SNS_OVP,
	MT6720_RANGE_F_VBUSOVP,
	MT6720_RANGE_F_IBUSOCP,
	R_MAX_RANGE
};

enum mt6720_div2_dtprop_type {
	DTPROP_U32,
	DTPROP_BOOL,
};

struct mt6720_div2_dtprop {
	const char *name;
	size_t offset;
	enum mt6720_fields field;
	enum mt6720_div2_dtprop_type type;
};

/* All converted to microvolt or microamp */
static const struct linear_range mt6720_div2_ranges[R_MAX_RANGE] = {
	LINEAR_RANGE_IDX(MT6720_RANGE_F_FSW_SET, 100000, 0, 9, 100000),
	LINEAR_RANGE_IDX(MT6720_RANGE_F_VBAT_SNS_OVP, 4200000, 0, 31, 25000),
	LINEAR_RANGE_IDX(MT6720_RANGE_F_VBUSOVP, 6000000, 0, 63, 100000),
	LINEAR_RANGE_IDX(MT6720_RANGE_F_IBUSOCP, 1000000, 0, 31, 250000),
};

#define MT6720_DIV2_FIELD(_fd, _reg, _lsb, _msb)			\
[_fd] = {								\
	.name = #_fd,							\
	.range = NULL,							\
	.field = REG_FIELD(_reg, _lsb, _msb),				\
	.inited = true,							\
}

#define MT6720_DIV2_FIELD_RANGE(_fd, _reg, _lsb, _msb)		\
[_fd] = {								\
	.name = #_fd,							\
	.range = &mt6720_div2_ranges[MT6720_RANGE_##_fd],		\
	.field = REG_FIELD(_reg, _lsb, _msb),				\
	.inited = true,							\
}

static const struct mt6720_div2_field mt6720_div2_fields[F_MAX_FIELD] = {
	MT6720_DIV2_FIELD(F_BUCK_EN, MT6720_REG_CHG_WDT, 5, 5),
	MT6720_DIV2_FIELD(F_CHG_EN, MT6720_REG_CHGCTL1, 6, 6),
	MT6720_DIV2_FIELD(F_OPERATION_MODE, MT6720_REG_CHGCTL1, 5, 5),
	MT6720_DIV2_FIELD(F_WDT_CNT_RST, MT6720_REG_CHGCTL1, 3, 3),
	MT6720_DIV2_FIELD(F_WDT_DIS, MT6720_REG_CHGCTL1, 2, 2),
	MT6720_DIV2_FIELD(F_WDT_TMR, MT6720_REG_CHGCTL1, 0, 1),
	MT6720_DIV2_FIELD_RANGE(F_FSW_SET, MT6720_REG_CHGCTL2, 4, 7),
	MT6720_DIV2_FIELD(F_FREQ_SHIFT, MT6720_REG_CHGCTL2, 2, 3),
	MT6720_DIV2_FIELD(F_PROTECTION_CTRL, MT6720_REG_CHGCTL3, 7, 7),
	MT6720_DIV2_FIELD(F_VAC_PD_EN, MT6720_REG_PDEN, 7, 7),
	MT6720_DIV2_FIELD(F_VBUS_PD_EN, MT6720_REG_PDEN, 6, 6),
	MT6720_DIV2_FIELD(F_VBUS_OVP_EN, MT6720_REG_VBUSOVP, 7, 7),
	MT6720_DIV2_FIELD_RANGE(F_VBUSOVP, MT6720_REG_VBUSOVP, 0, 5),
	MT6720_DIV2_FIELD(F_IBUS_UCP_EN, MT6720_REG_IBUSOCP, 7, 7),
	MT6720_DIV2_FIELD(F_IBUS_OCP_EN, MT6720_REG_IBUSOCP, 5, 5),
	MT6720_DIV2_FIELD_RANGE(F_IBUSOCP, MT6720_REG_IBUSOCP, 0, 4),
	MT6720_DIV2_FIELD(F_VBAT_SNS_OVP_EN, MT6720_REG_VBAT_SNS_OVP, 7, 7),
	MT6720_DIV2_FIELD_RANGE(F_VBAT_SNS_OVP, MT6720_REG_VBAT_SNS_OVP, 0, 4),
	MT6720_DIV2_FIELD(F_IBUS_UCP_TO, MT6720_REG_IBUSUCP_TO, 5, 7),
	MT6720_DIV2_FIELD(F_IBUS_FALL_DEGLITCH, MT6720_REG_IBUSUCP_TO, 3, 4),
	MT6720_DIV2_FIELD(F_IBUS_RISE_DEGLITCH, MT6720_REG_IBUSUCP_TO, 2, 2),
	MT6720_DIV2_FIELD(F_VBUS_LOW_ERR_EN, MT6720_REG_OTHER1, 5, 5),
	MT6720_DIV2_FIELD(F_VBUS_HIGH_ERR_EN, MT6720_REG_OTHER1, 4, 4),
	MT6720_DIV2_FIELD(F_VBAT_OVP_EN, MT6720_REG_OTHER1, 3, 3),
	MT6720_DIV2_FIELD(F_VAC_POR_CHG, MT6720_REG_OTHER1, 2, 2),
	MT6720_DIV2_FIELD(F_VBUS_LOW_ERR, MT6720_REG_DIV2_STAT2, 2, 2),
	MT6720_DIV2_FIELD(F_VBUS_HIGH_ERR, MT6720_REG_DIV2_STAT2, 1, 1),
	MT6720_DIV2_FIELD(F_VBUS_INSERT, MT6720_REG_DIV2_STAT3, 7, 7),
	MT6720_DIV2_FIELD(F_SWITCHING, MT6720_REG_DIV2_STAT3, 1, 1),
};

struct mt6720_div2_platform_data {
	const char *chg_name;
	u32 vbatovp;
	u32 vbusovp;
	u32 ibusocp;
	u32 wdt_tmr;
	u32 fsw;
	bool wdt_dis;
};

struct mt6720_data {
	struct device *dev;
	struct regmap *regmap;
	struct regmap_field *rm_fields[F_MAX_FIELD];
	struct power_supply *psy;
	struct mutex adc_lock;
	struct power_supply_desc psy_desc;
	struct charger_device *chgdev;
	struct charger_device *chg_dev; /* primary charger class device */
	struct delayed_work update_work;
	struct iio_channel *iio_adcs;
	unsigned int rg_resistor;
	unsigned int real_resistor;
	unsigned int irq_nums[MT6720_IRQ_DIV2_MAX];
	u8 stat[3];
	bool wdt_occured;
	bool last_bucken;
};

static int mt6720_div2_field_get(struct mt6720_data *data, enum mt6720_fields fd, u32 *val)
{
	u32 regval = 0, idx = fd;
	int ret = 0;

	if (!mt6720_div2_fields[idx].inited) {
		dev_info(data->dev, "%s, %s is not support\n", __func__,
			 mt6720_div2_fields[idx].name);
		return -EOPNOTSUPP;
	}

	ret = regmap_field_read(data->rm_fields[idx], &regval);
	if (ret)
		return ret;

	if (mt6720_div2_fields[idx].range)
		return linear_range_get_value(mt6720_div2_fields[idx].range, regval, val);

	*val = regval;
	return 0;
}

static int mt6720_div2_field_set(struct mt6720_data *data, enum mt6720_fields fd, unsigned int val)
{
	const struct linear_range *r;
	u32 idx = fd;

	if (!mt6720_div2_fields[idx].inited) {
		dev_info(data->dev, "%s, %s is not support\n", __func__,
			 mt6720_div2_fields[idx].name);
		return -EOPNOTSUPP;
	}

	if (mt6720_div2_fields[idx].range) {
		r = mt6720_div2_fields[idx].range;
		linear_range_get_selector_within(r, val, &val);
	}
	return regmap_field_write(data->rm_fields[idx], val);
}

static int mt6720_get_adc(struct mt6720_data *data, enum mt6720_adc_chan chan,
			  int *val)
{
	int ret = 0;

	ret = iio_read_channel_processed(&data->iio_adcs[chan], val);
	if (ret)
		dev_info(data->dev, "%s, Failed to get adc chan %d\n", __func__, chan);
	return ret;
}

static int mt6720_get_switching_state(struct mt6720_data *data, int *status)
{
	unsigned int switching_state;
	int ret;

	ret = mt6720_div2_field_get(data, F_SWITCHING, &switching_state);
	if (ret)
		return ret;

	if (switching_state)
		*status = POWER_SUPPLY_STATUS_CHARGING;
	else
		*status = POWER_SUPPLY_STATUS_NOT_CHARGING;

	return 0;
}

static int mt6720_update_stat(struct mt6720_data *data)
{
	int ret = 0;

	ret = regmap_raw_read(data->regmap, MT6720_REG_DIV2_STAT1, data->stat, 3);
	if (ret) {
		dev_info(data->dev, "%s, Failed to read div2 event\n", __func__);
		return ret;
	}

	dev_info(data->dev, "%s, div2 evt1 = 0x%02x, div2 evt2 = 0x%02x, div2 evt3 = 0x%02x\n",
		 __func__, data->stat[0], data->stat[1], data->stat[2]);
	return 0;
}

static int mt6720_get_charger_health(struct mt6720_data *data)
{
	int ret = 0;

	ret = mt6720_update_stat(data);
	if (ret) {
		dev_info(data->dev, "%s get status failed\n", __func__);
		return ret;
	}

	if (data->wdt_occured)
		return POWER_SUPPLY_HEALTH_WATCHDOG_TIMER_EXPIRE;

	if (data->stat[0] & MT6720_STAT_VBAT_OVP_MSK)
		return POWER_SUPPLY_HEALTH_OVERVOLTAGE;

	if (data->stat[0] & MT6720_STAT_IBUS_OCP_MSK)
		return POWER_SUPPLY_HEALTH_OVERCURRENT;

	if (data->stat[0] & MT6720_STAT_IBUS_UCP_FALL_MASK ||
	    data->stat[1] & MT6720_STAT_IBUS_OCP_H_MASK ||
	    data->stat[1] & MT6720_STAT_VBUS_LOW_HIGH_ERR_MSK ||
	    data->stat[2] & MT6720_STAT_VAC_UVLO_MSK ||
	    data->stat[2] & MT6720_STAT_VBUS_UVLO_MSK ||
	    data->stat[2] & MT6720_STAT_IBUS_UCP_TO_MSK ||
	    data->stat[2] & MT6720_STAT_CFLY_DIAG_MASK)
		return POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;

	return POWER_SUPPLY_HEALTH_GOOD;
}

static const char * const mt6720_manufacturer	= "Richtek Technology Corp.";
static const char * const mt6720_model		= "MT6720";

static int mt6720_psy_get_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   union power_supply_propval *val)
{
	struct mt6720_data *data = power_supply_get_drvdata(psy);
	int *pval = &val->intval;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		return mt6720_get_switching_state(data, pval);
	case POWER_SUPPLY_PROP_HEALTH:
		*pval = mt6720_get_charger_health(data);
		return 0;
	case POWER_SUPPLY_PROP_ONLINE:
		return mt6720_div2_field_get(data, F_VBUS_INSERT, pval);
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		return mt6720_div2_field_get(data, F_VBUSOVP, pval);
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		return mt6720_get_adc(data, ADC_VBUS, pval);
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		return mt6720_div2_field_get(data, F_IBUSOCP, pval);
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		return mt6720_get_adc(data, ADC_IBUS, pval);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		return mt6720_div2_field_get(data, F_VBAT_SNS_OVP, pval);
	case POWER_SUPPLY_PROP_TEMP:
		return mt6720_get_adc(data, ADC_TEMPJC, pval);
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = mt6720_model;
		return 0;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = mt6720_manufacturer;
		return 0;
	default:
		return -ENODATA;
	}
}

static int mt6720_psy_set_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   const union power_supply_propval *val)
{
	struct mt6720_data *data = power_supply_get_drvdata(psy);
	int intval = val->intval;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		return mt6720_div2_field_set(data, F_CHG_EN, !!intval);
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		return mt6720_div2_field_set(data, F_VBUSOVP, intval);
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (test_ibusocp != -1)
			return mt6720_div2_field_set(data, F_IBUSOCP, test_ibusocp);
		return mt6720_div2_field_set(data, F_IBUSOCP, intval);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		return mt6720_div2_field_set(data, F_VBAT_SNS_OVP, intval);
	default:
		return -EINVAL;
	}
}

static const enum power_supply_property mt6720_psy_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static int mt6720_psy_property_is_writeable(struct power_supply *psy,
					    enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		return 1;
	default:
		return 0;
	}
}

static const unsigned int mt6720_wdt_millisecond[] = { 500, 1000, 5000, 30000 };

static ssize_t watchdog_timer_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = to_power_supply(dev);
	struct mt6720_data *data = power_supply_get_drvdata(psy);
	unsigned int wdt_tmr_now = 0, wdt_sel, wdt_dis;
	int ret;

	ret = mt6720_div2_field_get(data, F_WDT_DIS, &wdt_dis);
	if (ret)
		return ret;

	if (!wdt_dis) {
		ret = mt6720_div2_field_get(data, F_WDT_TMR, &wdt_sel);
		if (ret)
			return ret;

		wdt_tmr_now = mt6720_wdt_millisecond[wdt_sel];
	}

	return sysfs_emit(buf, "%d\n", wdt_tmr_now);
}

static ssize_t watchdog_timer_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct power_supply *psy = to_power_supply(dev);
	struct mt6720_data *data = power_supply_get_drvdata(psy);
	unsigned int wdt_set, wdt_sel;
	int ret;

	ret = kstrtouint(buf, 10, &wdt_set);
	if (ret)
		return ret;

	ret = mt6720_div2_field_set(data, F_WDT_DIS, 1);
	if (ret)
		return ret;

	wdt_sel = find_closest(wdt_set, mt6720_wdt_millisecond,
			       ARRAY_SIZE(mt6720_wdt_millisecond));

	ret = mt6720_div2_field_set(data, F_WDT_TMR, wdt_sel);
	if (ret)
		return ret;

	if (wdt_set) {
		ret = mt6720_div2_field_set(data, F_WDT_DIS, 0);
		if (ret)
			return ret;
	}

	return count;
}

static ssize_t battery_voltage_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = to_power_supply(dev);
	struct mt6720_data *data = power_supply_get_drvdata(psy);
	int vbat_now, ret;

	ret = mt6720_get_adc(data, ADC_VBAT, &vbat_now);
	if (ret)
		return ret;

	return sysfs_emit(buf, "%d\n", vbat_now);
}

static ssize_t battery_current_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = to_power_supply(dev);
	struct mt6720_data *data = power_supply_get_drvdata(psy);
	int ibat_now, ret;

	ret = mt6720_get_adc(data, ADC_IBAT, &ibat_now);
	if (ret)
		return ret;

	return sysfs_emit(buf, "%d\n", ibat_now);
}

static DEVICE_ATTR_RW(watchdog_timer);
static DEVICE_ATTR_RO(battery_voltage);
static DEVICE_ATTR_RO(battery_current);

static struct attribute *mt6720_sysfs_attrs[] = {
	&dev_attr_watchdog_timer.attr,
	&dev_attr_battery_voltage.attr,
	&dev_attr_battery_current.attr,
	NULL
};

ATTRIBUTE_GROUPS(mt6720_sysfs);

static int mt6720_register_psy(struct mt6720_data *data)
{
	struct mt6720_div2_platform_data *pdata = dev_get_platdata(data->dev);
	struct device *dev = data->dev;
	struct power_supply_desc *desc = &data->psy_desc;
	struct power_supply_config cfg = {};

	cfg.drv_data = data;
	cfg.of_node = dev->of_node;
	cfg.attr_grp = mt6720_sysfs_groups;

	desc->name = pdata->chg_name;
	desc->type = POWER_SUPPLY_TYPE_USB;
	desc->properties = mt6720_psy_properties;
	desc->num_properties = ARRAY_SIZE(mt6720_psy_properties);
	desc->property_is_writeable = mt6720_psy_property_is_writeable;
	desc->get_property = mt6720_psy_get_property;
	desc->set_property = mt6720_psy_set_property;

	data->psy = devm_power_supply_register(dev, desc, &cfg);

	return PTR_ERR_OR_ZERO(data->psy);
}

static int mt6720_config_batsense_resistor(struct mt6720_data *data)
{
	//TODO
	return 0;
}

static const struct mt6720_div2_platform_data mt6720_div2_pdata_def = {
	.chg_name = "primary_divchg",
	.vbatovp = 4350000,
	.vbusovp = 8900000,
	.ibusocp = 4250000,
	.wdt_dis = false,
	.wdt_tmr = 0,
	.fsw = 500000,
};

static irqreturn_t mt6720_div2_fl_vac_pd_handler(int irq, void *data)
{
	struct mt6720_data *mdata = data;

	dev_info(mdata->dev, "%s, irq = %d\n", __func__, irq);
	mod_delayed_work(system_wq, &mdata->update_work, msecs_to_jiffies(5));
	return IRQ_HANDLED;
}

static irqreturn_t mt6720_div2_fl_vbus_pd_handler(int irq, void *data)
{
	struct mt6720_data *mdata = data;

	dev_info(mdata->dev, "%s, irq = %d\n", __func__, irq);
	mod_delayed_work(system_wq, &mdata->update_work, msecs_to_jiffies(mod_time));
	return IRQ_HANDLED;
}

static irqreturn_t mt6720_div2_fl_vbat_ovp_handler(int irq, void *data)
{
	struct mt6720_data *mdata = data;

	dev_info(mdata->dev, "%s, irq = %d\n", __func__, irq);
	mod_delayed_work(system_wq, &mdata->update_work, msecs_to_jiffies(mod_time));
	charger_dev_notify(mdata->chgdev, CHARGER_DEV_NOTIFY_BAT_OVP);
	return IRQ_HANDLED;
}

static irqreturn_t mt6720_div2_fl_ibus_ocp_handler(int irq, void *data)
{
	struct mt6720_data *mdata = data;

	dev_info(mdata->dev, "%s, irq = %d\n", __func__, irq);
	mod_delayed_work(system_wq, &mdata->update_work, msecs_to_jiffies(mod_time));
	charger_dev_notify(mdata->chgdev, CHARGER_DEV_NOTIFY_IBUSOCP);
	return IRQ_HANDLED;
}

static irqreturn_t mt6720_div2_fl_ibus_ucp_rise_handler(int irq, void *data)
{
	struct mt6720_data *mdata = data;

	dev_info(mdata->dev, "%s, irq = %d\n", __func__, irq);
	mod_delayed_work(system_wq, &mdata->update_work, msecs_to_jiffies(mod_time));
	return IRQ_HANDLED;
}

static irqreturn_t mt6720_div2_fl_ibus_ucp_fall_handler(int irq, void *data)
{
	struct mt6720_data *mdata = data;

	dev_info(mdata->dev, "%s, irq = %d\n", __func__, irq);
	mod_delayed_work(system_wq, &mdata->update_work, msecs_to_jiffies(mod_time));
	charger_dev_notify(mdata->chgdev, CHARGER_DEV_NOTIFY_IBUSUCP_FALL);
	return IRQ_HANDLED;
}

static irqreturn_t mt6720_div2_fl_ibus_ocp_h_handler(int irq, void *data)
{
	struct mt6720_data *mdata = data;

	dev_info(mdata->dev, "%s, irq = %d\n", __func__, irq);
	mod_delayed_work(system_wq, &mdata->update_work, msecs_to_jiffies(mod_time));
	charger_dev_notify(mdata->chgdev, CHARGER_DEV_NOTIFY_IBUSOCP);
	return IRQ_HANDLED;
}

static irqreturn_t mt6720_div2_fl_vbus_low_err_handler(int irq, void *data)
{
	struct mt6720_data *mdata = data;

	dev_info(mdata->dev, "%s, irq = %d\n", __func__, irq);
	mod_delayed_work(system_wq, &mdata->update_work, msecs_to_jiffies(mod_time));
	return IRQ_HANDLED;
}

static irqreturn_t mt6720_div2_fl_vbus_high_err_handler(int irq, void *data)
{
	struct mt6720_data *mdata = data;

	dev_info(mdata->dev, "%s, irq = %d\n", __func__, irq);
	mod_delayed_work(system_wq, &mdata->update_work, msecs_to_jiffies(mod_time));
	charger_dev_notify(mdata->chgdev, CHARGER_DEV_NOTIFY_VBUS_OVP);
	return IRQ_HANDLED;
}

static irqreturn_t mt6720_div2_fl_vac_por_handler(int irq, void *data)
{
	struct mt6720_data *mdata = data;

	dev_info(mdata->dev, "%s, irq = %d\n", __func__, irq);
	mod_delayed_work(system_wq, &mdata->update_work, msecs_to_jiffies(mod_time));
	return IRQ_HANDLED;
}

static irqreturn_t mt6720_div2_fl_vbus_insert_handler(int irq, void *data)
{
	struct mt6720_data *mdata = data;

	dev_info(mdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6720_div2_fl_vbat_insert_handler(int irq, void *data)
{
	struct mt6720_data *mdata = data;

	dev_info(mdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6720_div2_fl_wdt_handler(int irq, void *data)
{
	struct mt6720_data *mdata = data;

	dev_info(mdata->dev, "%s, irq = %d\n", __func__, irq);
	mdata->wdt_occured = true;
	return IRQ_HANDLED;
}

static irqreturn_t mt6720_div2_fl_vac_uvlo_handler(int irq, void *data)
{
	struct mt6720_data *mdata = data;

	dev_info(mdata->dev, "%s, irq = %d\n", __func__, irq);
	mod_delayed_work(system_wq, &mdata->update_work, msecs_to_jiffies(mod_time));
	return IRQ_HANDLED;
}

static irqreturn_t mt6720_div2_fl_vbus_uvlo_handler(int irq, void *data)
{
	struct mt6720_data *mdata = data;

	dev_info(mdata->dev, "%s, irq = %d\n", __func__, irq);
	mod_delayed_work(system_wq, &mdata->update_work, msecs_to_jiffies(mod_time));
	return IRQ_HANDLED;
}

static irqreturn_t mt6720_div2_fl_ibus_ucp_to_handler(int irq, void *data)
{
	struct mt6720_data *mdata = data;

	dev_info(mdata->dev, "%s, irq = %d\n", __func__, irq);
	mod_delayed_work(system_wq, &mdata->update_work, msecs_to_jiffies(mod_time));
	return IRQ_HANDLED;
}

static irqreturn_t mt6720_div2_fl_con_switching_handler(int irq, void *data)
{
	struct mt6720_data *mdata = data;

	dev_info(mdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6720_div2_fl_cfly_diag_handler(int irq, void *data)
{
	struct mt6720_data *mdata = data;

	dev_info(mdata->dev, "%s, irq = %d\n", __func__, irq);
	mod_delayed_work(system_wq, &mdata->update_work, msecs_to_jiffies(mod_time));
	return IRQ_HANDLED;
}

#define MT6720_DIV2_IRQ(_name)					\
{								\
	.name = #_name,						\
	.handler = mt6720_##_name##_handler,			\
}

static int mt6720_div2_init_irq(struct mt6720_data *data)
{
	struct device *dev = data->dev;
	int i = 0, ret = 0;
	const struct {
		char *name;
		irq_handler_t handler;
	} mt6720_div2_irqs[] = {
		MT6720_DIV2_IRQ(div2_fl_vac_pd),
		MT6720_DIV2_IRQ(div2_fl_vbus_pd),
		MT6720_DIV2_IRQ(div2_fl_vbat_ovp),
		MT6720_DIV2_IRQ(div2_fl_ibus_ocp),
		MT6720_DIV2_IRQ(div2_fl_ibus_ucp_rise),
		MT6720_DIV2_IRQ(div2_fl_ibus_ucp_fall),
		MT6720_DIV2_IRQ(div2_fl_ibus_ocp_h),
		MT6720_DIV2_IRQ(div2_fl_vbus_low_err),
		MT6720_DIV2_IRQ(div2_fl_vbus_high_err),
		MT6720_DIV2_IRQ(div2_fl_vac_por),
		MT6720_DIV2_IRQ(div2_fl_vbus_insert),
		MT6720_DIV2_IRQ(div2_fl_vbat_insert),
		MT6720_DIV2_IRQ(div2_fl_wdt),
		MT6720_DIV2_IRQ(div2_fl_vac_uvlo),
		MT6720_DIV2_IRQ(div2_fl_vbus_uvlo),
		MT6720_DIV2_IRQ(div2_fl_ibus_ucp_to),
		MT6720_DIV2_IRQ(div2_fl_con_switching),
		MT6720_DIV2_IRQ(div2_fl_cfly_diag),
	};

	for (i = 0; i < ARRAY_SIZE(mt6720_div2_irqs); i++) {
		ret = platform_get_irq_byname(to_platform_device(data->dev),
					     mt6720_div2_irqs[i].name);
		if (ret < 0) {
			dev_info(dev, "%s, %s not declare in dts\n",
				 __func__, mt6720_div2_irqs[i].name);
			continue;
		}
		dev_info(dev, "%s, request \"%s\", irq number:%d\n",
			 __func__, mt6720_div2_irqs[i].name, ret);
		data->irq_nums[i] = ret;
		ret = devm_request_threaded_irq(dev, ret, NULL,
						mt6720_div2_irqs[i].handler,
						IRQF_TRIGGER_FALLING,
						mt6720_div2_irqs[i].name, data);
		if (ret) {
			dev_info(dev, "%s, Failed to request irq %s\n",
				 __func__, mt6720_div2_irqs[i].name);
			return ret;
		}
		disable_irq(data->irq_nums[i]);
	}
	return 0;
}

static int mt6720_div2_get_iio_adc(struct mt6720_data *data)
{
	data->iio_adcs = devm_iio_channel_get_all(data->dev);
	if (IS_ERR(data->iio_adcs))
		return PTR_ERR(data->iio_adcs);

	return 0;
}

static inline enum mt6720_adc_chan to_mt6720_adc(enum adc_channel chan)
{
	enum mt6720_adc_chan adc_chan;

	switch (chan) {
	case ADC_CHANNEL_VBUS:
		adc_chan = ADC_VBUS;
		break;
	case ADC_CHANNEL_IBUS:
		adc_chan = ADC_IBUS;
		break;
	case ADC_CHANNEL_VBAT:
		adc_chan = ADC_VBAT;
		break;
	case ADC_CHANNEL_IBAT:
		adc_chan = ADC_IBAT;
		break;
	case ADC_CHANNEL_TEMP_JC:
		adc_chan = ADC_TEMPJC;
		break;
	case ADC_CHANNEL_VOUT:
		adc_chan = ADC_VOUT;
		break;
	default:
		adc_chan = ADC_MAX_CHANNEL; /* not support */
		break;
	}
	return adc_chan;
}

static const u32 mt6720_adc_accuracy_tbl[ADC_MAX_CHANNEL] = {
	75000, /* VBUS */
	15000, /* VBAT */
	150000, /* IBUS */
	300000, /* IBAT */
	0,	/* TEMPJC */
	20000, /* VOUT */
};

static int mt6720_div2_get_adc_accuracy(struct charger_device *chgdev, enum adc_channel chan,
					int *min, int *max)
{
	enum mt6720_adc_chan adc_chan = to_mt6720_adc(chan);

	if (adc_chan == ADC_MAX_CHANNEL)
		return -EINVAL;

	*min = *max = mt6720_adc_accuracy_tbl[adc_chan];
	return 0;
}

static void mt6720_enable_irq(struct mt6720_data *data)
{
	unsigned int i = 0;

	for (i = 0; i < MT6720_IRQ_DIV2_MAX; i++)
		enable_irq(data->irq_nums[i]);
}

static void mt6720_disable_irq(struct mt6720_data *data)
{
	unsigned int i = 0;

	for (i = 0; i < MT6720_IRQ_DIV2_MAX; i++)
		disable_irq(data->irq_nums[i]);
}

static int mt6720_div2_enable_chg(struct charger_device *chgdev, bool en);
static int mt6720_div2_is_vbuslowerr(struct charger_device *chgdev, bool *err)
{
	struct mt6720_data *data = charger_get_data(chgdev);
	int ret = 0;

	ret = mt6720_div2_enable_chg(chgdev, true);
	if (ret)
		dev_info(data->dev, "%s, enable div2 failed, update health stat\n",
			 __func__);

	*err = (data->stat[1] & BIT(2));
	dev_info(data->dev, "%s err = %d\n", __func__, *err);

	ret = mt6720_div2_enable_chg(chgdev, false);
	if (ret)
		dev_info(data->dev, "%s, disable div2 en failed\n", __func__);

	return ret;
}

static int mt6720_div2_init_chip(struct charger_device *chgdev)
{
	/*
	 * initial setting will be in preloader.  Maybe
	 */
	return 0;
}

static int mt6720_div2_set_ibatocp(struct charger_device *chgdev, u32 uA)
{
	return 0;
}

static int mt6720_div2_set_vbatovp(struct charger_device *chgdev, u32 uV)
{
	struct mt6720_data *data = charger_get_data(chgdev);
	union power_supply_propval val = { .intval = uV };
	int ret = 0;

	ret = power_supply_set_property(data->psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX, &val);
	if (ret)
		dev_info(data->dev, "%s, Failed to set vbat ovp\n", __func__);
	return ret;
}

static int mt6720_div2_set_ibusocp(struct charger_device *chgdev, u32 uA)
{
	struct mt6720_data *data = charger_get_data(chgdev);
	union power_supply_propval val = { .intval = uA };
	int ret = 0;

	ret = power_supply_set_property(data->psy, POWER_SUPPLY_PROP_CURRENT_MAX, &val);
	if (ret)
		dev_info(data->dev, "%s, Failed to set ibus ocp\n", __func__);
	return ret;
}

static int mt6720_div2_set_vbusovp(struct charger_device *chgdev, u32 uV)
{
	struct mt6720_data *data = charger_get_data(chgdev);
	union power_supply_propval val = { .intval = uV };
	int ret = 0;

	ret = power_supply_set_property(data->psy, POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
	if (ret)
		dev_info(data->dev, "%s, Failed to set vbus ovp\n", __func__);
	return ret;
}

static int  mt6720_div2_get_adc(struct charger_device *chgdev, enum adc_channel chan, int *min, int *max)
{
	struct mt6720_data *data = charger_get_data(chgdev);
	enum mt6720_adc_chan adc_chan = to_mt6720_adc(chan);

	if (adc_chan == ADC_MAX_CHANNEL)
		return -EINVAL;

	return mt6720_get_adc(data, adc_chan, min);
}

static int mt6720_div2_is_chg_enabled(struct charger_device *chgdev, bool *en)
{
	struct mt6720_data *data = charger_get_data(chgdev);
	union power_supply_propval val = {0};
	int ret = 0;

	ret = power_supply_get_property(data->psy, POWER_SUPPLY_PROP_STATUS, &val);
	if (ret < 0)
		dev_info(data->dev, "%s, Failed to get chg_en\n", __func__);
	else
		*en = val.intval;
	return ret;
}

static int mt6720_div2_enable_chg(struct charger_device *chgdev, bool en)
{
	struct mt6720_data *data = charger_get_data(chgdev);
	int ret = 0;
	unsigned int check_health_retry = 5;

	dev_info(data->dev, "%s en = %d\n", __func__, en);

	if (en) {
		ret = charger_dev_is_powerpath_enabled(data->chg_dev, &data->last_bucken);
		if (ret) {
			dev_info(data->dev, "%s, failed to get buck en\n", __func__);
			return ret;
		}

		dev_info(data->dev, "%s, last_bucken = %d\n", __func__, data->last_bucken);

		ret = charger_dev_enable_powerpath(data->chg_dev, false);
		if (ret) {
			dev_info(data->dev, "%s, disable BUCK_EN failed\n", __func__);
			return ret;
		}
	}

	ret = mt6720_div2_field_set(data, F_CHG_EN, en);
	if (ret) {
		dev_info(data->dev, "%s, Failed to set F_CHG_EN\n", __func__);
		return ret;
	}

	ret = mt6720_div2_field_set(data, F_PROTECTION_CTRL, en);
	if (ret) {
		dev_info(data->dev, "%s, Failed to set F_PROTECTION_CTRL\n", __func__);
		return ret;
	}

	if (en) { /* enable div2 case */
		while (check_health_retry--) {
			mdelay(10);
			if (mt6720_get_charger_health(data) != POWER_SUPPLY_HEALTH_GOOD) {
				dev_info(data->dev, "%s, div2 health is not good\n", __func__);
			} else {
				dev_info(data->dev, "%s, div2 health is good\n", __func__);
				mt6720_enable_irq(data);
				return 0;
			}
		}
		ret = mt6720_div2_field_set(data, F_PROTECTION_CTRL, false);
		if (ret)
			dev_info(data->dev, "%s, disable protection en fail\n", __func__);
		ret = mt6720_div2_field_set(data, F_CHG_EN, false);
		if (ret) {
			dev_info(data->dev, "%s, disable div2 en fail\n", __func__);
			return ret;
		}
		return -EINVAL;
	}

	/* disable div2 case */
	data->wdt_occured = false;
	mt6720_disable_irq(data);
	ret = charger_dev_enable_powerpath(data->chg_dev, data->last_bucken);
	if (ret)
		dev_info(data->dev, "%s, failed to set last_bucken(%d)\n",
			 __func__, data->last_bucken);
	return ret;
}

static int mt6720_div2_set_vbatovp_alarm(struct charger_device *chgdev, u32 uV)
{
	return 0;
}

static int mt6720_div2_reset_vbatovp_alarm(struct charger_device *chgdev)
{
	return 0;
}

static int mt6720_div2_set_vbusovp_alarm(struct charger_device *chgdev, u32 uV)
{
	return 0;
}

static int mt6720_div2_reset_vbusovp_alarm(struct charger_device *chgdev)
{
	return 0;
}

static const struct charger_ops mt6720_div2_chg_ops = {
	.enable = mt6720_div2_enable_chg,
	.is_enabled = mt6720_div2_is_chg_enabled,
	.get_adc = mt6720_div2_get_adc,
	.set_vbusovp = mt6720_div2_set_vbusovp,
	.set_ibusocp = mt6720_div2_set_ibusocp,
	.set_vbatovp = mt6720_div2_set_vbatovp,
	.set_ibatocp = mt6720_div2_set_ibatocp,
	.init_chip = mt6720_div2_init_chip,
	.set_vbatovp_alarm = mt6720_div2_set_vbatovp_alarm,
	.reset_vbatovp_alarm = mt6720_div2_reset_vbatovp_alarm,
	.set_vbusovp_alarm = mt6720_div2_set_vbusovp_alarm,
	.reset_vbusovp_alarm = mt6720_div2_reset_vbusovp_alarm,
	.is_vbuslowerr = mt6720_div2_is_vbuslowerr,
	.get_adc_accuracy = mt6720_div2_get_adc_accuracy,
};

static void mt6720_div2_destroy(void *data)
{
	struct charger_device *charger_dev = data;

	charger_device_unregister(charger_dev);
}

static const struct charger_properties mt6720_div2_props = {
	.alias_name = "mt6720_div2",
};

int mt6720_div2_init_chgdev(struct mt6720_data *data)
{
	struct mt6720_div2_platform_data *pdata = dev_get_platdata(data->dev);

	data->chgdev = charger_device_register(pdata->chg_name, data->dev,
						data, &mt6720_div2_chg_ops,
						&mt6720_div2_props);
	if (IS_ERR(data->chgdev)) {
		dev_info(data->dev, "%s, Failed to register charger device\n", __func__);
		return PTR_ERR(data->chgdev);
	}

	return devm_add_action_or_reset(data->dev, mt6720_div2_destroy, data->chgdev);
}

#define MT6720_DIV2_DTPROP(_name, _member, _field, _type)			\
{										\
	.name = _name,								\
	.field = _field,							\
	.type = _type,								\
	.offset = offsetof(struct mt6720_div2_platform_data, _member),		\
}

const struct mt6720_div2_dtprop mt6720_div2_dtprops[] = {
	MT6720_DIV2_DTPROP("wdt-dis", wdt_dis, F_WDT_DIS, DTPROP_BOOL),
	MT6720_DIV2_DTPROP("vbatovp", vbatovp, F_VBAT_SNS_OVP, DTPROP_U32),
	MT6720_DIV2_DTPROP("vbusovp", vbusovp, F_VBUSOVP, DTPROP_U32),
	MT6720_DIV2_DTPROP("ibusocp", ibusocp, F_IBUSOCP, DTPROP_U32),
	MT6720_DIV2_DTPROP("wdt-timer", wdt_tmr, F_WDT_TMR, DTPROP_U32),
};

void mt6720_div2_parse_dt_helper(struct device *dev, void *pdata,
				 const struct mt6720_div2_dtprop *dp)
{
	void *val = pdata + dp->offset;
	int ret = 0;

	if (dp->type == DTPROP_BOOL)
		*((bool *)val) = device_property_read_bool(dev, dp->name);
	else {
		ret = device_property_read_u32(dev, dp->name, val);
		if (ret < 0)
			dev_info(dev, "%s, Failed to get \"%s\" property\n", __func__, dp->name);
	}

}

static u32 pdata_get_val(void *pdata, const struct mt6720_div2_dtprop *dp)
{
	if (dp->type == DTPROP_BOOL)
		return *((bool *)(pdata + dp->offset));

	return *((u32 *)(pdata + dp->offset));
}

static int mt6720_div2_apply_pdata(struct mt6720_data *data)
{
	const struct mt6720_div2_dtprop *dp;
	u32 val = 0;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(mt6720_div2_dtprops); i++) {
		dp = &mt6720_div2_dtprops[i];
		if (dp->field >= F_MAX_FIELD)
			continue;

		val = pdata_get_val(dev_get_platdata(data->dev), dp);
		dev_info(data->dev, "%s, dp-name = %s, val = %d\n", __func__, dp->name, val);

		ret = mt6720_div2_field_set(data, dp->field, val);
		if (ret == -EOPNOTSUPP) {
			dev_info(data->dev, "%s, dp-name = %s not support\n", __func__, dp->name);
			continue;
		} else if (ret < 0) {
			dev_info(data->dev, "%s, Failed to apply pdata %s\n", __func__, dp->name);
			return ret;
		}
	}

	return 0;
}

static int mt6720_div2_get_pdata(struct device *dev)
{
	struct mt6720_div2_platform_data *pdata = dev_get_platdata(dev);
	struct device_node *np = dev->of_node;
	int i = 0;

	if (!np)
		return -ENODEV;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	memcpy(pdata, &mt6720_div2_pdata_def, sizeof(*pdata));
	for (i = 0; i < ARRAY_SIZE(mt6720_div2_dtprops); i++)
		mt6720_div2_parse_dt_helper(dev, pdata, &mt6720_div2_dtprops[i]);

	if (of_property_read_string(np, "chgdev-name", &pdata->chg_name))
		dev_info(dev, "%s, Failed to get \"chgdev-name\" property\n", __func__);

	dev->platform_data = pdata;
	return 0;
}

static int mt6720_div2_init_rmap_fields(struct mt6720_data *data)
{
	const struct mt6720_div2_field *fds = mt6720_div2_fields;
	int i = 0;

	for (i = 0; i < F_MAX_FIELD; i++) {
		if (!fds[i].inited)
			continue;
		data->rm_fields[i] = devm_regmap_field_alloc(data->dev, data->regmap, fds[i].field);
		if (IS_ERR(data->rm_fields[i])) {
			dev_info(data->dev, "%s, Failed to allocate regmap fields[%s]\n",
				 __func__, fds[i].name);
			return PTR_ERR(data->rm_fields[i]);
		}
	}
	return 0;
}

static void mt6720_update_stat_func(struct work_struct *work)
{
	struct mt6720_data *data = container_of(work, struct mt6720_data, update_work.work);
	int ret = 0;

	dev_info(data->dev, "%s ++\n", __func__);
	ret = mt6720_update_stat(data);
	if (ret)
		dev_info(data->dev, "%s, mt6720 update stat failed\n", __func__);
}

static void mt6720_update_work_destroy(void *d)
{
	struct delayed_work *work = d;

	cancel_delayed_work(work);
}

static int mt6720_div2_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mt6720_data *data;
	int ret;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = dev;
	mutex_init(&data->adc_lock);
	platform_set_drvdata(pdev, data);

	data->regmap = dev_get_regmap(dev->parent, NULL);
	if (IS_ERR(data->regmap))
		return rt_err_probe(dev, PTR_ERR(data->regmap), "Failed to init regmap\n");

	ret = mt6720_div2_init_rmap_fields(data);
	if (ret)
		rt_err_probe(dev, ret, "Failed to init regmap field\n");

	ret = mt6720_div2_get_pdata(dev);
	if (ret)
		return rt_err_probe(dev, ret, "Failed to get pdata\n");

	ret = mt6720_div2_apply_pdata(data);
	if (ret)
		return rt_err_probe(dev, ret, "Failed to apply pdata\n");

	ret = mt6720_config_batsense_resistor(data);
	if (ret)
		return rt_err_probe(dev, ret, "Failed to config batsense resistor\n");

	ret = mt6720_div2_get_iio_adc(data);
	if (ret)
		return rt_err_probe(dev, ret, "Failed to get iio adc\n");

	ret = mt6720_register_psy(data);
	if (ret)
		return rt_err_probe(dev, ret, "Failed to init power supply\n");

	ret = mt6720_div2_init_chgdev(data);
	if (ret)
		return rt_err_probe(dev, ret, "Failed to init chgdev\n");

	INIT_DELAYED_WORK(&data->update_work, mt6720_update_stat_func);
	ret = devm_add_action_or_reset(dev, mt6720_update_work_destroy, &data->update_work);
	if (ret)
		rt_err_probe(dev, ret, "Failed to add update_work action\n");

	/* get switching charger class device */
	data->chg_dev = get_charger_by_name("primary_chg");
	if (!data->chg_dev) {
		dev_info(dev, "%s, get primary_chg failed\n", __func__);
		return -ENODEV;
	}

	return mt6720_div2_init_irq(data);
}

static void mt6720_div2_remove(struct platform_device *pdev)
{
	struct mt6720_data *data = platform_get_drvdata(pdev);

	charger_device_unregister(data->chgdev);
}

static const struct of_device_id mt6720_device_match_table[] = {
	{ .compatible = "mediatek,mt6720-div2" },
	{}
};
MODULE_DEVICE_TABLE(of, mt6720_device_match_table);

static struct platform_driver mt6720_div2_driver = {
	.driver = {
		.name = "mt6720-div2",
		.of_match_table = mt6720_device_match_table,
	},
	.probe = mt6720_div2_probe,
	.remove = mt6720_div2_remove,
};
module_platform_driver(mt6720_div2_driver);

MODULE_DESCRIPTION("Richtek MT6720 charger driver");
MODULE_AUTHOR("SHIH CHIA CHANG <jeff_chang@richtek.com>");
MODULE_LICENSE("GPL");
