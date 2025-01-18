// SPDX-License-Identifier: GPL-2.0-only
/*
 * mt6379-chgdev.c -- Mediatek MT6379 Charger class Driver
 *
 * Copyright (c) 2023 MediaTek Inc.
 *
 * Author: SHIH CHIA CHANG <jeff_chang@richtek.com>
 */

#include <linux/power_supply.h>
#include <linux/regmap.h>
#include "mt6379-charger.h"

#include "charger_class.h"
#include "mtk_charger.h"

#define PE_POLL_TIME_US		(100 * 1000)

static int mt6379_plug_in(struct charger_device *chgdev)
{
	int ret = 0;
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	struct mt6379_charger_platform_data *pdata = dev_get_platdata(cdata->dev);

	if (pdata->wdt_en) {
		ret = mt6379_charger_field_set(cdata, F_WDT_EN, 1);
		if (ret < 0) {
			dev_err(cdata->dev, "Failed to enable WDT\n");
			return ret;
		}
	}
	return ret;
}

static int mt6379_plug_out(struct charger_device *chgdev)
{
	int ret = 0;
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	struct mt6379_charger_platform_data *pdata = dev_get_platdata(cdata->dev);

	if (pdata->wdt_en) {
		ret = mt6379_charger_field_set(cdata, F_WDT_EN, 0);
		if (ret < 0) {
			dev_err(cdata->dev, "Failed to disable WDT\n");
			return ret;
		}
	}
	return ret;
}

static int mt6379_enable_charging(struct charger_device *chgdev, bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0;

	mutex_lock(&cdata->cv_lock);
	ret = mt6379_charger_field_set(cdata, F_CHG_EN, en);
	mutex_unlock(&cdata->cv_lock);
	return ret;
}

static int mt6379_is_enabled(struct charger_device *chgdev, bool *en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0;
	u32 val = 0;

	ret = mt6379_charger_field_get(cdata, F_CHG_EN, &val);
	if (ret < 0)
		return ret;
	*en = val;
	return 0;
}

static int mt6379_set_ichg(struct charger_device *chgdev, u32 uA)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	union power_supply_propval val = { .intval = uA };

	return power_supply_set_property(cdata->psy,
					 POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
					 &val);
}

static int mt6379_get_ichg(struct charger_device *chgdev, u32 *uA)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	union power_supply_propval val;
	int ret = 0;

	ret = power_supply_get_property(cdata->psy,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
					&val);
	if (ret < 0)
		return ret;
	*uA = val.intval;
	return 0;
}

static int mt6379_get_min_ichg(struct charger_device *chgdev, u32 *uA)
{
	*uA = 300000;
	return 0;
}

#define RECHG_THRESHOLD		100000 /* uV */
static int mt6379_charger_set_cv(struct mt6379_charger_data *cdata, u32 uV)
{
	int ret = 0;
	bool done = false;
	union power_supply_propval val;
	u32 enabled = 0;

	mutex_lock(&cdata->cv_lock);
	if (cdata->batprotect_en) {
		dev_notice(cdata->dev, "batprotect enabled, should not set cv\n");
		goto out;
	}

	if (uV <= cdata->cv || uV >= cdata->cv + RECHG_THRESHOLD)
		goto out_cv;
	ret = power_supply_get_property(cdata->psy, POWER_SUPPLY_PROP_STATUS, &val);
	done = (val.intval == POWER_SUPPLY_STATUS_FULL);
	if (ret < 0 || !done)
		goto out_cv;
	ret = mt6379_charger_field_get(cdata, F_CHG_EN, &enabled);
	if (ret < 0 || !enabled)
		goto out_cv;

	if (mt6379_charger_field_set(cdata, F_CHG_EN, false) < 0)
		dev_notice(cdata->dev, "Failed to disable charging\n");
out_cv:
	val.intval = uV;
	ret = power_supply_set_property(cdata->psy,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
					&val);
	if (!ret)
		cdata->cv = uV;
	dev_info(cdata->dev, "cdata->cv = %d uV\n", cdata->cv);
	if (done && enabled)
		mt6379_charger_field_set(cdata, F_CHG_EN, true);
out:
	mutex_unlock(&cdata->cv_lock);
	return ret;
}

static int mt6379_set_cv(struct charger_device *chgdev, u32 uV)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);

	return mt6379_charger_set_cv(cdata, uV);
}

static int mt6379_get_cv(struct charger_device *chgdev, u32 *uV)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	union power_supply_propval val;
	int ret = 0;

	mutex_lock(&cdata->cv_lock);
	ret = power_supply_get_property(cdata->psy,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
					&val);
	if (!ret)
		*uV = val.intval;
	mutex_unlock(&cdata->cv_lock);
	return ret;
}

static int mt6379_set_aicr(struct charger_device *chgdev, u32 uA)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	union power_supply_propval val = { .intval = uA };
	int ret = 0;

	mutex_lock(&cdata->pe_lock);
	ret = power_supply_set_property(cdata->psy,
					POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
					&val);
	mutex_unlock(&cdata->pe_lock);
	return ret;
}

static int mt6379_get_aicr(struct charger_device *chgdev, u32 *uA)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	union power_supply_propval val;
	int ret = 0;

	mutex_lock(&cdata->pe_lock);
	ret = power_supply_get_property(cdata->psy,
					POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
					&val);
	if (!ret)
		*uA = val.intval;
	mutex_unlock(&cdata->pe_lock);
	return ret;
}

static int mt6379_get_min_aicr(struct charger_device *chgdev, u32 *uA)
{
	*uA = 100000;
	return 0;
}

static int mt6379_set_mivr(struct charger_device *chgdev, u32 uV)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	union power_supply_propval val = { .intval = uV };
	int ret = 0;

	mutex_lock(&cdata->pe_lock);
	ret = power_supply_set_property(cdata->psy,
					POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
					&val);
	mutex_unlock(&cdata->pe_lock);
	return ret;
}

static int mt6379_get_mivr(struct charger_device *chgdev, u32 *uV)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	union power_supply_propval val;
	int ret = 0;

	mutex_lock(&cdata->pe_lock);
	ret = power_supply_get_property(cdata->psy,
					POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
					&val);
	if (!ret)
		*uV = val.intval;
	mutex_unlock(&cdata->pe_lock);
	return ret;
}

static int mt6379_get_mivr_state(struct charger_device *chgdev, bool *active)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0;
	u32 val = 0;

	*active = false;
	ret = mt6379_charger_field_get(cdata, F_ST_MIVR, &val);
	if (ret < 0)
		return ret;
	*active = val;
	return 0;
}

static int mt6379_get_adc(struct charger_device *chgdev, enum adc_channel chan,
			  int *min, int *max)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	enum mt6379_adc_chan adc_chan;
	int ret = 0;

	switch (chan) {
	case ADC_CHANNEL_VBUS:
		adc_chan = ADC_CHAN_CHGVIN;
		break;
	case ADC_CHANNEL_VSYS:
		adc_chan = ADC_CHAN_VSYS;
		break;
	case ADC_CHANNEL_VBAT:
		adc_chan = ADC_CHAN_VBAT;
		break;
	case ADC_CHANNEL_IBUS:
		adc_chan = ADC_CHAN_IBUS;
		break;
	case ADC_CHANNEL_IBAT:
		adc_chan = ADC_CHAN_IBAT;
		break;
	case ADC_CHANNEL_TEMP_JC:
		adc_chan = ADC_CHAN_TEMPJC;
		break;
	case ADC_CHANNEL_USBID:
		adc_chan = ADC_CHAN_SBU2;
		break;
	default:
		return -EINVAL;
	}

	ret = iio_read_channel_processed(&cdata->iio_adcs[adc_chan], min);
	if (ret < 0) {
		dev_err(cdata->dev, "Failed to read adc channel[%d]\n", adc_chan);
		return ret;
	}
	//dev_info(cdata->dev, "%s: chan:%d, adc_chan:%d, val:%d\n", __func__,
	//	 chan, adc_chan, *min);
	*max = *min;
	return 0;
}

static int mt6379_get_vbus(struct charger_device *chgdev, u32 *vbus)
{
	return mt6379_get_adc(chgdev, ADC_CHANNEL_VBUS, vbus, vbus);
}

static int mt6379_get_ibus(struct charger_device *chgdev, u32 *ibus)
{
	return mt6379_get_adc(chgdev, ADC_CHANNEL_IBUS, ibus, ibus);
}

static int mt6379_get_ibat(struct charger_device *chgdev, u32 *ibat)
{
	return mt6379_get_adc(chgdev, ADC_CHANNEL_IBAT, ibat, ibat);
}

#define ABNORMAL_TEMP_JC	120
static int mt6379_get_tchg(struct charger_device *chgdev, int *min, int *max)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int temp_jc = 0, ret = 0, retry_cnt = 3;

	/* temp abnormal workaround */
	do {
		ret = mt6379_get_adc(chgdev, ADC_CHANNEL_TEMP_JC, &temp_jc, &temp_jc);
	} while ((ret < 0 || temp_jc >= ABNORMAL_TEMP_JC) && (--retry_cnt) > 0);
	if (ret < 0) {
		dev_err(cdata->dev, "Failed to get temp jc\n");
		return ret;
	}
	/* use last result if temp_jc is abnormal */
	if (temp_jc >= ABNORMAL_TEMP_JC)
		temp_jc = atomic_read(&cdata->tchg);
	else
		atomic_set(&cdata->tchg, temp_jc);
	*min = *max = temp_jc;
	dev_info(cdata->dev, "tchg = %d\n", temp_jc);
	return 0;
}

static int mt6379_get_zcv(struct charger_device *chgdev, u32 *uV)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);

	*uV = M_TO_U(cdata->zcv);
	return 0;
}

static int mt6379_set_ieoc(struct charger_device *chgdev, u32 uA)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	union power_supply_propval val = { .intval = uA };

	return power_supply_set_property(cdata->psy,
					 POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
					 &val);
}

static int mt6379_enable_te(struct charger_device *chgdev, bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);

	return mt6379_charger_field_set(cdata, F_TE, en);
}

static int mt6379_reset_eoc_state(struct charger_device *chgdev)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);

	return mt6379_charger_field_set(cdata, F_EOC_RST, 1);
}

static int mt6379_sw_check_eoc(struct charger_device *chgdev, u32 uA)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0, ibat = 0;

	ret = mt6379_get_ibat(chgdev, &ibat);
	if (ret < 0) {
		dev_err(cdata->dev, "Failed to get ibat\n");
		return ret;
	}
	if (ibat <= uA) {
		/* if  it happens 3 times, trigger EOC event */
		if (atomic_read(&cdata->eoc_cnt) == 2) {
			atomic_set(&cdata->eoc_cnt, 0);
			dev_info(cdata->dev, "ieoc = %d, ibat = %d\n", uA, ibat);
			charger_dev_notify(cdata->chgdev, CHARGER_DEV_NOTIFY_EOC);
		} else
			atomic_inc(&cdata->eoc_cnt);
	} else
		atomic_set(&cdata->eoc_cnt, 0);

	return 0;

}

static int mt6379_is_charge_done(struct charger_device *chgdev, bool *done)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	union power_supply_propval val;
	int ret = 0;

	ret = power_supply_get_property(cdata->psy, POWER_SUPPLY_PROP_STATUS, &val);
	*done = (val.intval == POWER_SUPPLY_STATUS_FULL);
	return ret;
}

static int mt6379_enable_buck(struct charger_device *chgdev, bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);

	return mt6379_charger_field_set(cdata, F_BUCK_EN, en);
}

static int mt6379_is_buck_enabled(struct charger_device *chgdev, bool *en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0;
	u32 val = 0;

	ret = mt6379_charger_field_get(cdata, F_BUCK_EN, &val);
	*en = val;
	return ret;
}

static int mt6379_enable_charger_timer(struct charger_device *chgdev, bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);

	return mt6379_charger_field_set(cdata, F_CHG_TMR_EN, en);
}

static int mt6379_is_charger_timer_enabled(struct charger_device *chgdev, bool *en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0;
	u32 val = 0;

	ret = mt6379_charger_field_get(cdata, F_CHG_TMR_EN, &val);
	if (!ret)
		*en = val;

	return ret;
}

static int mt6379_kick_wdt(struct charger_device *chgdev)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);

	return mt6379_charger_field_set(cdata, F_WDT_RST, 1);
}

static int mt6379_charger_check_aicc_running(struct mt6379_charger_data *cdata,
					     bool *active)
{
	int ret = 0;
	u32 val = 0;

	ret = mt6379_charger_field_get(cdata, F_ST_PWR_RDY, &val);
	if (ret)
		return ret;
	if (!val) /* power not rdy -> detach */
		return -EINVAL;
	ret = mt6379_charger_field_get(cdata, F_ST_AICC_DONE, &val);
	if (ret)
		return ret;
	*active = val;
	return 0;
}

static int mt6379_run_aicc(struct charger_device *chgdev, u32 *uA)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0;
	bool active = false;
	ktime_t timeout = ktime_add(ktime_get(), ms_to_ktime(7000));

	ret = mt6379_get_mivr_state(chgdev, &active);
	if (ret < 0)
		return ret;
	if (!active) {
		dev_err(cdata->dev, "mivr loop is not active\n");
		return 0;
	}

	mutex_lock(&cdata->pe_lock);
	ret = mt6379_charger_field_set(cdata, F_AICC_EN, 1);
	if (ret)
		goto out;

	active = false;
	while (!ktime_after(ktime_get(), timeout)) {
		ret = mt6379_charger_check_aicc_running(cdata, &active);
		if (ret)
			goto out;
		if (active)
			break;
		msleep(128);
	}
	if (!active) /* Timeout */
		goto out;

	ret = mt6379_charger_field_get(cdata, F_AICC_RPT, uA);
	if (ret < 0) {
		dev_err(cdata->dev, "Failed to get aicc report\n");
		goto out;
	}
	dev_info(cdata->dev, "%s AICC Report = %d uA\n", __func__, *uA);
out:
	mt6379_charger_field_set(cdata, F_AICC_EN, 0);
	mutex_unlock(&cdata->pe_lock);
	return ret;
}

static int mt6379_run_pe(struct mt6379_charger_data *cdata, bool pe20)
{
	int ret = 0;
	unsigned long timeout = pe20 ? 1400 : 2800; /* ms */
	unsigned int regval = 0;

	/* 800 mA */
	ret = mt6379_charger_field_set(cdata, F_IBUS_AICR, 800000);
	if (ret < 0)
		return ret;
	/* 2 A */
	ret = mt6379_charger_field_set(cdata, F_CC, 2000000);
	if (ret < 0)
		return ret;
	ret = mt6379_charger_field_set(cdata, F_CHG_EN, 1);
	if (ret < 0)
		return ret;
	ret = mt6379_charger_field_set(cdata, F_PE_SEL, pe20);
	if (ret < 0)
		return ret;
	ret = mt6379_charger_field_set(cdata, F_PE_EN, 1);
	if (ret < 0)
		return ret;

	return regmap_read_poll_timeout(cdata->rmap, MT6379_REG_CHG_PUMPX, regval,
					!(regval & BIT(7)), PE_POLL_TIME_US,
					timeout * 1000);
}

static int mt6379_set_pe_current_pattern(struct charger_device *chgdev,
					 bool inc)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0;

	mutex_lock(&cdata->pe_lock);
	dev_info(cdata->dev, "inc = %d\n", inc);
	ret = mt6379_charger_field_set(cdata, F_PE10_INC, inc);
	if (ret < 0) {
		dev_err(cdata->dev, "Failed to set pe10 up/down\n");
		goto out;
	}
	ret = mt6379_run_pe(cdata, false);
out:
	mutex_unlock(&cdata->pe_lock);
	return ret;
}

static int mt6379_set_pe20_efficiency_table(struct charger_device *chgdev)
{
	return 0;
}

static int mt6379_set_pe20_current_pattern(struct charger_device *chgdev, u32 uV)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0;

	mutex_lock(&cdata->pe_lock);
	dev_info(cdata->dev, "pe20 = %d\n", uV);
	ret = mt6379_charger_field_set(cdata, F_PE20_CODE, uV);
	if (ret < 0) {
		dev_err(cdata->dev, "Failed to set pe20 code\n");
		goto out;
	}
	ret = mt6379_run_pe(cdata, true);
out:
	mutex_unlock(&cdata->pe_lock);
	return ret;
}

static int mt6379_reset_pe_ta(struct charger_device *chgdev)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0;

	mutex_lock(&cdata->pe_lock);
	ret = mt6379_charger_field_set(cdata, F_VBUS_MIVR, 4600);
	if (ret < 0)
		goto out;
	ret = mt6379_charger_field_set(cdata, F_IBUS_AICR, 100);
	if (ret < 0)
		goto out;
	msleep(250);
	ret = mt6379_charger_field_set(cdata, F_IBUS_AICR, 500);
out:
	mutex_unlock(&cdata->pe_lock);
	return ret;
}

static int mt6379_enable_pe_cable_drop_comp(struct charger_device *chgdev, bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0;

	if (en)
		return 0;

	mutex_lock(&cdata->pe_lock);
	dev_info(cdata->dev, "en = %d\n", en);
	ret = mt6379_charger_field_set(cdata, F_PE20_CODE, 0x1F);
	if (ret < 0) {
		dev_err(cdata->dev, "Failed to set cable drop comp code\n");
		goto out;
	}
	ret = mt6379_run_pe(cdata, true);
out:	mutex_unlock(&cdata->pe_lock);
	return ret;
}

static int mt6379_enable_discharge(struct charger_device *chgdev, bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int i = 0, ret = 0;
	const int dischg_retry_cnt = 3;
	u32 val = 0;

	dev_info(cdata->dev, "%s en = %d\n", __func__, en);
	ret = mt6379_enable_hm(cdata, true);
	if (ret < 0)
		return ret;

	ret = mt6379_charger_field_set(cdata, F_FORCE_VBUS_SINK, en);
	if (ret < 0) {
		dev_err(cdata->dev, "Failed to set discharge %d\n", en);
		goto out;
	}
	/* for disable, make sure it is disabled */
	if (!en) {
		for (i = 0; i < dischg_retry_cnt; i++) {
			ret = mt6379_charger_field_get(cdata, F_FORCE_VBUS_SINK,
						       &val);
			if (ret < 0)
				continue;
			if (!val)
				break;
			ret = mt6379_charger_field_set(cdata, F_FORCE_VBUS_SINK, 0);
		}
		if (i == dischg_retry_cnt) {
			dev_err(cdata->dev, "Failed to disable discharging\n");
			ret = -EINVAL;
		}
	}
out:
	mt6379_enable_hm(cdata, false);
	return ret;
}

static int mt6379_enable_chg_type_det(struct charger_device *chgdev, bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	union power_supply_propval val;

	dev_info(cdata->dev, "%s en = %d\n", __func__, en);
	val.intval = en ? ATTACH_TYPE_TYPEC : ATTACH_TYPE_NONE;
	return power_supply_set_property(cdata->psy, POWER_SUPPLY_PROP_ONLINE, &val);
}

#define DUMP_REG_BUF_SIZE	1024
enum {
	ADC_DUMP_VBUS = 0, /* CHGVIN */
	ADC_DUMP_IBUS,
	ADC_DUMP_VBAT,
	ADC_DUMP_IBAT,
	ADC_DUMP_VSYS,
	ADC_DUMP_MAX,
};

static int mt6379_dump_registers(struct charger_device *chgdev)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int i = 0, ret = 0, vbus = 0, ibus = 0;
	u32 val = 0;
	char buf[DUMP_REG_BUF_SIZE] = "\0";
	static const struct {
		const char *name;
		const char *unit;
		enum mt6379_charger_reg_field fd;
	} settings[] = {
		{ .fd = F_CC, .name = "CC", .unit = "mA" },
		{ .fd = F_IBUS_AICR, .name = "IBUS-AICR", .unit = "mA" },
		{ .fd = F_VBUS_MIVR, .name = "VBUS-MIVR", .unit = "mV" },
		{ .fd = F_IEOC, .name = "IEOC", .unit = "mA" },
		{ .fd = F_CV, .name = "CV", .unit = "mV" },
		{ .fd = F_WLIN_AICR, .name = "WLIN-AICR", .unit = "mA" },
		{ .fd = F_WLIN_MIVR, .name = "WLIN-MIVR", .unit = "mV" },
	};
	static struct {
		const char *name;
		const char *unit;
		enum mt6379_adc_chan chan;
		u32 value;
	} adcs[ADC_DUMP_MAX] = {
		{ .chan = ADC_CHAN_CHGVIN, .name = "VBUS", .unit = "mV" },
		{ .chan = ADC_CHAN_IBUS, .name = "IBUS", .unit = "mA" },
		{ .chan = ADC_CHAN_VBAT, .name = "VBAT", .unit = "mV" },
		{ .chan = ADC_CHAN_IBAT, .name = "IBAT", .unit = "mA" },
		{ .chan = ADC_CHAN_VSYS, .name = "VSYS", .unit = "mV" },
	};
	static const struct {
		const u16 reg;
		const char *name;
	} regs[] = {
		{ .reg = MT6379_REG_CHG_STAT, .name = "CHG_STAT" },
		{ .reg = MT6379_REG_CHG_STAT0, .name = "CHG_STAT0" },
		{ .reg = MT6379_REG_CHG_STAT1, .name = "CHG_STAT1" },
		{ .reg = MT6379_REG_CHG_TOP1, .name = "CHG_TOP1" },
		{ .reg = MT6379_REG_CHG_TOP2, .name = "CHG_TOP2" },
		{ .reg = MT6379_REG_CHG_EOC2, .name = "CHG_EOC2" },
		{ .reg = MT6379_REG_CHG_WDT, .name = "CHG_WDT" },
		{ .reg = MT6379_REG_CHG_HD_BOBU5, .name = "HD_BOBU5" },
		{ .reg = MT6379_REG_CHG_HD_TRIM6, .name = "HD_TRIM6" },
	};

	for (i = 0; i < ARRAY_SIZE(settings); i++) {
		ret = mt6379_charger_field_get(cdata, settings[i].fd, &val);
		if (ret < 0) {
			dev_err(cdata->dev, "Failed to get %s\n",
				settings[i].name);
			return ret;
		}
		val = U_TO_M(val);
		if (i == ARRAY_SIZE(settings) - 1) {
			ret = scnprintf(buf + strlen(buf), DUMP_REG_BUF_SIZE,
					"%s = %d%s\n", settings[i].name, val,
					settings[i].unit);
		} else {
			ret = scnprintf(buf + strlen(buf), DUMP_REG_BUF_SIZE,
					"%s = %d%s, ", settings[i].name, val,
					settings[i].unit);
		}
		if (ret < 0)
			return ret;
	}

	for (i = 0; i < ARRAY_SIZE(adcs); i++) {
		ret = iio_read_channel_processed(&cdata->iio_adcs[adcs[i].chan],
						 &adcs[i].value);
		if (ret < 0) {
			dev_err(cdata->dev, "Faled to read adc %s\n",
				adcs[i].name);
			return ret;
		}
		adcs[i].value = U_TO_M(adcs[i].value);
		if (i == ARRAY_SIZE(adcs) - 1)
			ret = scnprintf(buf + strlen(buf), DUMP_REG_BUF_SIZE,
					"%s = %d%s\n", adcs[i].name, adcs[i].value,
					adcs[i].unit);
		else
			ret = scnprintf(buf + strlen(buf), DUMP_REG_BUF_SIZE,
					"%s = %d%s, ", adcs[i].name, adcs[i].value,
					adcs[i].unit);
		if (ret < 0)
			return ret;
	}

	/* set fsw_control by vbus & ibus */
	vbus = adcs[ADC_DUMP_VBUS].value;
	ibus = adcs[ADC_DUMP_IBUS].value;
	if (vbus <= 5500 && ibus >= 500) {
		if (!cdata->fsw_control) {
			ret = mt6379_enable_hm(cdata, true);
			ret |= regmap_update_bits(cdata->rmap, MT6379_REG_CHG_HD_BOBU5,
						  MT6379_CHG_RAMPUP_COMP_MSK,
						  3 << MT6379_CHG_RAMPUP_COMP_SFT);
			ret |= regmap_update_bits(cdata->rmap, MT6379_REG_CHG_HD_TRIM6,
						  MT6379_CHG_IEOC_FLOW_RB_MSK, 0);
			ret = mt6379_enable_hm(cdata, false);
			cdata->fsw_control = true;
		}
	} else {
		if (cdata->fsw_control) {
			ret = mt6379_enable_hm(cdata, true);
			ret |= regmap_update_bits(cdata->rmap, MT6379_REG_CHG_HD_BOBU5,
						  MT6379_CHG_RAMPUP_COMP_MSK,
						  1 << MT6379_CHG_RAMPUP_COMP_SFT);
			ret |= regmap_update_bits(cdata->rmap, MT6379_REG_CHG_HD_TRIM6,
						  MT6379_CHG_IEOC_FLOW_RB_MSK,
						  MT6379_CHG_IEOC_FLOW_RB_MSK);
			cdata->fsw_control = false;
		}
	}

	if (cdata->batprotect_en) {
		ret = iio_read_channel_processed(&cdata->iio_adcs[ADC_CHAN_VBATMON], &val);
		if (ret) {
			dev_info(cdata->dev, "Failed to read VBATMON(ret:%d)\n", ret);
			return ret;
		}

		ret = scnprintf(buf + strlen(buf), DUMP_REG_BUF_SIZE,
				"VBATCELL(VBAT_MON) = %dmV\n", val);
	}

	for (i = 0; i < ARRAY_SIZE(regs); i++) {
		ret = regmap_read(cdata->rmap, regs[i].reg, &val);
		if (ret) {
			dev_err(cdata->dev, "Failed to read %s\n", regs[i].name);
			return ret;
		}

		if (i == ARRAY_SIZE(regs) - 1)
			ret = scnprintf(buf + strlen(buf), DUMP_REG_BUF_SIZE,
					"%s = 0x%02x\n", regs[i].name, val);
		else
			ret = scnprintf(buf + strlen(buf), DUMP_REG_BUF_SIZE,
					"%s = 0x%02x, ", regs[i].name, val);
		if (ret < 0)
			return ret;
	}
	dev_info(cdata->dev, "%s %s", __func__, buf);
	return 0;

}

static int mt6379_enable_hz(struct charger_device *chgdev, bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);

	return mt6379_charger_field_set(cdata, F_HZ, en ? 1 : 0);
}

static int mt6379_do_event(struct charger_device *chgdev, u32 event, u32 args)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);

	switch (event) {
	case EVENT_FULL:
	case EVENT_RECHARGE:
	case EVENT_DISCHARGE:
		power_supply_changed(cdata->psy);
		break;
	default:
		break;
	}
	return 0;
}

static int mt6379_enable_6pin_battery_charging(struct charger_device *chgdev,
					       bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	struct mt6379_charger_platform_data *pdata = dev_get_platdata(cdata->dev);
	u16 batend_code;
	u32 vbat, cv;
	int ret = 0;

	mutex_lock(&cdata->cv_lock);
	if (cdata->batprotect_en == en)
		goto out;
	dev_info(cdata->dev, "%s en = %d\n", __func__, en);
	if (!en)
		goto dis_pro;

	/* If no 6pin used, always bypass this function */
	if (atomic_read(&cdata->no_6pin_used) == 1)
		goto dis_pro;

	/* avoid adc too fast, write 0 for make sure read adc succuessfully */
	ret = iio_read_channel_processed(&cdata->iio_adcs[ADC_CHAN_VBATMON], &vbat);
	if (ret) {
		dev_info(cdata->dev, "Failed to read VBATMON(ret:%d)\n", ret);
		goto dis_mon;
	}

	ret = mt6379_charger_field_get(cdata, F_CV, &cv);
	if (ret < 0) {
		dev_err(cdata->dev, "Failed to get cv\n");
		goto dis_mon;
	}
	dev_info(cdata->dev, "%s vbat = %dmV, cv = %dmV\n", __func__, vbat / 1000, cv / 1000);
	if (vbat >= cv) {
		dev_info(cdata->dev, "vbat(%d) >= cv(%d), should not start\n", vbat, cv);
		goto dis_mon;
	} else if (vbat <= pdata->pmic_uvlo) {
		/*
		 * If no 6pin used, vbat detected by vbat mon will be much
		 * lower than the PMIC UVLO.
		 */
		atomic_set(&cdata->no_6pin_used, 1);
			dev_notice(cdata->dev, "vbat <= PMIC UVLO(%d mV), should not start\n",
				   pdata->pmic_uvlo);
		goto dis_mon;

	}
	ret = mt6379_charger_field_set(cdata, F_BATINT, cv);
	if (ret < 0) {
		dev_err(cdata->dev, "Failed to set batint\n");
		goto dis_mon;
	}
	batend_code = ADC_TO_VBAT_RAW(cv);
	batend_code = cpu_to_be16(batend_code);
	dev_info(cdata->dev, "%s batend code = 0x%04x\n", __func__, batend_code);
	ret = regmap_bulk_write(cdata->rmap, MT6379_REG_BATEND_CODE, &batend_code, 2);
	if (ret < 0) {
		dev_err(cdata->dev, "Failed to set batend code\n");
		goto dis_mon;
	}
	ret = mt6379_charger_field_set(cdata, F_BATPROTECT_EN, 1);
	if (ret < 0) {
		dev_err(cdata->dev, "Failed to enable bat protect\n");
		goto dis_mon;
	}
	/* set Max CV */
	ret = mt6379_charger_field_set(cdata, F_CV, 4710000);
	if (ret < 0) {
		dev_err(cdata->dev, "failed to set maximum cv\n");
		goto dis_pro;
	}
	cdata->batprotect_en = true;
	dev_info(cdata->dev, "%s successfully\n", __func__);
	goto out;
dis_pro:
	if (mt6379_charger_field_set(cdata, F_BATPROTECT_EN, 0) < 0)
		dev_notice(cdata->dev, "Failed to disable bat protect\n");
	if (mt6379_charger_field_set(cdata, F_CV, cdata->cv) < 0)
		dev_notice(cdata->dev, "Failed to set cv\n");
dis_mon:
	cdata->batprotect_en = false;
out:
	mutex_unlock(&cdata->cv_lock);
	return ret;

}

static int mt6379_enable_usbid(struct charger_device *chgdev, bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	int ret = 0;

	ret = mt6379_charger_field_set(cdata, F_PD_MDEN, en ? 0 : 1);
	if (ret < 0)
		return ret;
	return mt6379_charger_field_set(cdata, F_USBID_EN, en ? 1 : 0);
}

static const u32 mt6379_usbid_rup[] = {
	500000, 75000, 5000, 1000,
};

static const u32 mt6379_usbid_src_ton[] = {
	400, 1000, 4000, 10000, 40000, 100000, 400000,
};

static inline u32 mt6379_trans_usbid_rup(u32 rup)
{
	int i = 0;
	int maxidx = ARRAY_SIZE(mt6379_usbid_rup) - 1;

	if (rup >= mt6379_usbid_rup[0])
		return 0;
	if (rup <= mt6379_usbid_rup[maxidx])
		return maxidx;

	for (i = 0; i < maxidx; i++) {
		if (rup == mt6379_usbid_rup[i])
			return i;
		if (rup < mt6379_usbid_rup[i] &&
				rup > mt6379_usbid_rup[i + 1]) {
			if ((mt6379_usbid_rup[i] - rup) <=
					(rup - mt6379_usbid_rup[i + 1]))
				return i;
			else
				return i + 1;
		}
	}
	return maxidx;
}

static int mt6379_set_usbid_rup(struct charger_device *chgdev, u32 rup)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	u32 val = mt6379_trans_usbid_rup(rup);

	return mt6379_charger_field_set(cdata, F_ID_RUPSEL, val);
}

static inline u32 mt6379_trans_usbid_src_ton(u32 src_ton)
{
	int i = 0;
	int maxidx = ARRAY_SIZE(mt6379_usbid_src_ton) - 1;

	/* There is actually an option, always on, after 400000 */
	if (src_ton == 0)
		return maxidx + 1;
	if (src_ton <= mt6379_usbid_src_ton[0])
		return 0;
	if (src_ton >= mt6379_usbid_src_ton[maxidx])
		return maxidx;

	for (i = 0; i < maxidx; i++) {
		if (src_ton == mt6379_usbid_src_ton[i])
			return i;
		if (src_ton > mt6379_usbid_src_ton[i] &&
				src_ton < mt6379_usbid_src_ton[i + 1]) {
			if ((src_ton - mt6379_usbid_src_ton[i]) <=
					(mt6379_usbid_src_ton[i + 1] - src_ton))
				return i;
			else
				return i + 1;
		}
	}
	return maxidx;
}

static int mt6379_set_usbid_src_ton(struct charger_device *chgdev, u32 src_ton)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);
	u32 val = mt6379_trans_usbid_src_ton(src_ton);

	return mt6379_charger_field_set(cdata, F_IS_TDET, val);
}

static int mt6379_enable_usbid_floating(struct charger_device *chgdev, bool en)
{
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);

	return mt6379_charger_field_set(cdata, F_USBID_FLOATING, en);
}

#define BOOT_TIMES_MASK         0xC0
#define BOOT_TIMES_SHIFT        6
static int mt6379_set_boot_volt_times(struct charger_device *chgdev, u32 val)
{
	u16 data = 0, ret;
	struct mt6379_charger_data *cdata = charger_get_data(chgdev);

	ret = regmap_bulk_read(cdata->rmap, MT6379_REG_FGADC_SYS_INFO_CON0, &data, sizeof(data));
	if (ret)
		return ret;
	data &= ~BOOT_TIMES_MASK;
	data |= (val << BOOT_TIMES_SHIFT & BOOT_TIMES_MASK);
	ret = regmap_bulk_write(cdata->rmap, MT6379_REG_FGADC_SYS_INFO_CON0,
				&data, sizeof(data));
	if (ret)
		return ret;

	return 0;
}

static const struct charger_ops mt6379_charger_ops = {
	/* cable plug in/out */
	.plug_in = mt6379_plug_in,
	.plug_out = mt6379_plug_out,
	/* enable */
	.enable = mt6379_enable_charging,
	.is_enabled = mt6379_is_enabled,

	/* charging current */
	.set_charging_current = mt6379_set_ichg,
	.get_charging_current = mt6379_get_ichg,
	.get_min_charging_current = mt6379_get_min_ichg,
	/* charging voltage */
	.set_constant_voltage = mt6379_set_cv,
	.get_constant_voltage = mt6379_get_cv,
	/* input current limit */
	.set_input_current = mt6379_set_aicr,
	.get_input_current = mt6379_get_aicr,
	.get_min_input_current = mt6379_get_min_aicr,
	/* MIVR */
	.set_mivr = mt6379_set_mivr,
	.get_mivr = mt6379_get_mivr,
	.get_mivr_state = mt6379_get_mivr_state,
	/* ADC */
	.get_adc = mt6379_get_adc,
	.get_vbus_adc = mt6379_get_vbus,
	.get_ibus_adc = mt6379_get_ibus,
	.get_ibat_adc = mt6379_get_ibat,
	.get_tchg_adc = mt6379_get_tchg,
	.get_zcv = mt6379_get_zcv,
	/* charging termination */
	.set_eoc_current = mt6379_set_ieoc,
	.enable_termination = mt6379_enable_te,
	.reset_eoc_state = mt6379_reset_eoc_state,
	.safety_check = mt6379_sw_check_eoc,
	.is_charging_done = mt6379_is_charge_done,
	/* power path */
	.enable_powerpath = mt6379_enable_buck,
	.is_powerpath_enabled = mt6379_is_buck_enabled,
	/* timer */
	.enable_safety_timer = mt6379_enable_charger_timer,
	.is_safety_timer_enabled = mt6379_is_charger_timer_enabled,
	.kick_wdt = mt6379_kick_wdt,
	/* AICL */
	.run_aicl = mt6379_run_aicc,
	/* PE+/PE+20 */
	.send_ta_current_pattern = mt6379_set_pe_current_pattern,
	.set_pe20_efficiency_table = mt6379_set_pe20_efficiency_table,
	.send_ta20_current_pattern = mt6379_set_pe20_current_pattern,
	.reset_ta = mt6379_reset_pe_ta,
	.enable_cable_drop_comp = mt6379_enable_pe_cable_drop_comp,
	.enable_discharge = mt6379_enable_discharge,
	/* charger type detection */
	.enable_chg_type_det = mt6379_enable_chg_type_det,
	/* misc */
	.dump_registers = mt6379_dump_registers,
	.enable_hz = mt6379_enable_hz,
	/* event */
	.event = mt6379_do_event,
	.enable_6pin_battery_charging = mt6379_enable_6pin_battery_charging,
	/* TypeC */
	.enable_usbid = mt6379_enable_usbid,
	.set_usbid_rup = mt6379_set_usbid_rup,
	.set_usbid_src_ton = mt6379_set_usbid_src_ton,
	.enable_usbid_floating = mt6379_enable_usbid_floating,
	/* set boot volt times */
	.set_boot_volt_times = mt6379_set_boot_volt_times,
};

static const struct charger_properties mt6379_charger_props = {
	.alias_name = "mt6379_chg",
};

static void mt6379_charger_init_destroy(void *data)
{
	struct charger_device *charger_dev = data;

	charger_device_unregister(charger_dev);
}

int mt6379_charger_init_chgdev(struct mt6379_charger_data *cdata)
{
	struct mt6379_charger_platform_data *pdata = dev_get_platdata(cdata->dev);

	cdata->chgdev = charger_device_register(pdata->chgdev_name, cdata->dev,
						cdata, &mt6379_charger_ops,
						&mt6379_charger_props);
	if (IS_ERR(cdata->chgdev))
		return PTR_ERR(cdata->chgdev);

	return devm_add_action_or_reset(cdata->dev, mt6379_charger_init_destroy, cdata->chgdev);
}
