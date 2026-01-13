// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2024 Huaqin Technology(Shanghai) Co., Ltd.
 */
#include "mtk_charger.h"
#include "xm_batt_health.h"

#define TAG                     "[HQ_CHG_BATT_HEALTH]" // [VENDOR_MODULE_SUBMODULE]
#define xm_err(fmt, ...)        pr_err(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)
#define xm_warn(fmt, ...)       pr_warn(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)
#define xm_notice(fmt, ...)     pr_notice(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)
#define xm_info(fmt, ...)       pr_info(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)
#define xm_debug(fmt, ...)      pr_debug(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)

static int batt_health_over_fv_protect_func(struct xm_batt_health *batt_health)
{
	static int over_fv_cnt;

	if (!batt_health->over_fv_protect_on) {
		return 0;
	}

	if ((batt_health->vbat > batt_health->effective_fv) &&
		(!batt_health->over_fv_flag) &&
		(batt_health->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO)) {
		over_fv_cnt++;
		if (over_fv_cnt > 1000)
			over_fv_cnt = 3;
	}

	if (!batt_health->over_fv_flag && (over_fv_cnt >= 3) && (batt_health->effective_fcc >= 3000)) {
		xm_info("over fv protect function triggered\n");
		batt_health->over_fv_flag = true;
		over_fv_cnt = 0;
		batt_health->effective_fcc -= 1000;
		vote(batt_health->charger_fcc_votable, XM_BATT_HEALTH_VOTER, true, batt_health->effective_fcc);
	}

	if (batt_health->over_fv_flag && (batt_health->pd_type != MTK_PD_CONNECT_PE_READY_SNK_APDO)) {
		xm_info("over fv protect function deactivated\n");
		over_fv_cnt = 0;
		batt_health->over_fv_flag = false;
		vote(batt_health->charger_fcc_votable, XM_BATT_HEALTH_VOTER, false, 0);
	}

	return 0;
}

static int batt_health_night_smart_charge(struct xm_batt_health *batt_health)
{
	struct mtk_charger *info = NULL;
	bool chg_dev_chgen = false;

	info = dev_get_drvdata(batt_health->dev);
	if (IS_ERR_OR_NULL(info)) {
		xm_err("failed to get charger  form device\n");
		return -EFAULT;
	}

	charger_dev_is_enabled(info->chg1_dev, &chg_dev_chgen);

	if (batt_health->night_smart_charge_on && (batt_health->soc >= 80)) {
		xm_info("night smart charge function triggered\n");
		batt_health->night_charging_flag = true;

		/* TODO: disable buck/cp use votable */
		//charger_set_chg(info->charger, false);
		vote(batt_health->charger_fcc_votable, NIGHT_CHG_VOTER, true, 0);
		if (chg_dev_chgen) {
			charger_dev_enable(info->chg1_dev, false);
			xm_debug("night smart discharge\n");
		}
	}

	if (batt_health->night_charging_flag) {
		if (!batt_health->night_smart_charge_on || (batt_health->soc <= 75)) {
			xm_info("night smart charge function deactivated");
			batt_health->night_charging_flag = false;

			/* TODO: enable buck/cp use votable */
			vote(batt_health->charger_fcc_votable, NIGHT_CHG_VOTER, false, 0);
			if (!chg_dev_chgen && batt_health->effective_fcc != 0) {
				charger_dev_enable(info->chg1_dev, true);
				xm_debug("night smart enable charge\n");
			}
		}
	}

	xm_info("night_smart_charge_on: %d, night_charging_flag: %d chg_en: %d\n",
		batt_health->night_smart_charge_on, batt_health->night_charging_flag, chg_dev_chgen);

	return 0;
}

static int monitor_smart_batt_fv(struct xm_batt_health *batt_health)
{
	struct mtk_charger *info = NULL;
	
	info = dev_get_drvdata(batt_health->dev);
	if (IS_ERR_OR_NULL(info)) {
		xm_err("failed to get charger  form device\n");
		return -EFAULT;
	}
	info->set_smart_batt_diff_fv = max(batt_health->smart_batt, batt_health->smart_fv);

	xm_info("smart_batt: %dmv smart_fv: %dmv diff_fv: %dmv\n",
		batt_health->smart_batt, batt_health->smart_fv, info->set_smart_batt_diff_fv);

	return 0;
}

static int batt_health_battery_manager(struct xm_batt_health *batt_health)
{
	monitor_smart_batt_fv(batt_health);

	return 0;
}

// static int batt_health_smart_time_location_capacity(struct xm_batt_health *batt_health)
// {
// 	monitor_smart_batt_fv(batt_health);

// 	return 0;
// }

static int batt_health_update_state(struct xm_batt_health *batt_health)
{
	union power_supply_propval pval = {0};
	int ret = 0;
	struct mtk_charger *info = NULL;

	info = dev_get_drvdata(batt_health->dev);
	if (IS_ERR_OR_NULL(info)) {
		xm_err("failed to get mtk charger form device\n");
		return -EFAULT;
	}

	batt_health->pd_type = info->pd_type;

	batt_health->effective_fcc = get_effective_result(batt_health->charger_fcc_votable);

	batt_health->effective_fv = get_effective_result(batt_health->fv_votable);

	ret = power_supply_get_property(batt_health->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0) {
		xm_err("get battery capacity failed, ret = %d\n", ret);
		return ret;
	}
	batt_health->soc = pval.intval;

	ret = power_supply_get_property(batt_health->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		xm_err("get battery temperature failed, ret = %d\n", ret);
		return ret;
	}
	batt_health->tbat = pval.intval / 10;

	ret = power_supply_get_property(batt_health->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret < 0) {
		xm_err("get battery voltage failed, ret = %d\n", ret);
		return ret;
	}
	batt_health->vbat = pval.intval / 1000;

	xm_info("tbat: %d, vbat: %dmv, soc: %d, effctive_fcc: %d, effective_fv: %d, pd_type: %d\n",
		batt_health->tbat, batt_health->vbat, batt_health->soc,
		batt_health->effective_fcc, batt_health->effective_fv,
		batt_health->pd_type);

	return 0;
}

static void handle_batt_health_work(struct work_struct *work)
{
	struct xm_batt_health *batt_health = container_of(work, struct xm_batt_health, batt_health_work.work);

	mutex_lock(&batt_health->batt_health_work_lock);

	batt_health_update_state(batt_health);

	batt_health_over_fv_protect_func(batt_health);

	batt_health_night_smart_charge(batt_health);

	batt_health_battery_manager(batt_health);

	/* NOTE: reuse battery manager function */
	//batt_health_smart_time_location_capacity(batt_health);

	mutex_unlock(&batt_health->batt_health_work_lock);

	schedule_delayed_work(&batt_health->batt_health_work, msecs_to_jiffies(1000));
}

int xm_batt_health_init(struct mtk_charger *info)
{
	struct xm_batt_health *batt_health = NULL;

	if (info->batt_health) {
		xm_err("battery health already initialized\n");
		return -EINVAL;
	}

	batt_health = devm_kzalloc(&info->pdev->dev, sizeof(*batt_health), GFP_KERNEL);
	if (!batt_health) {
		return -ENOMEM;
	}

	batt_health->dev = &info->pdev->dev;

	/* votable initialize */
	batt_health->charger_fcc_votable = find_votable("CHARGER_FCC");
	if (!batt_health->charger_fcc_votable) {
		xm_err("find CHARGER_FCC voltable failed\n");
	}

	batt_health->fv_votable = find_votable("CHARGER_FV");
	if (!batt_health->fv_votable) {
		xm_err("find CHARGER_FV voltable failed\n");
	}

	/* power supply/class initialize */
	batt_health->batt_psy = power_supply_get_by_name("battery");
	if (!batt_health->batt_psy) {
		xm_err("get battery power supply failed\n");
	}

	/* battery health work initialize */
	INIT_DELAYED_WORK(&batt_health->batt_health_work, handle_batt_health_work);

	/* battery health mutex lock initialize  */
	mutex_init(&batt_health->batt_health_work_lock);

	/* default dynamic function on/off switch */
	batt_health->over_fv_protect_on = true;
	batt_health->night_smart_charge_on = false;

	/* default flags */
	batt_health->over_fv_flag = false;
	batt_health->night_charging_flag = false;

	info->batt_health = batt_health;

	xm_info("battery health %s initialize success\n", XM_BATT_HEALTH_VERSION);

	return 0;
}

int xm_batt_health_deinit(struct mtk_charger *info)
{
	if (!info->batt_health) {
		return 0;
	}

	cancel_delayed_work_sync(&info->batt_health->batt_health_work);

	//devm_kfree(info->dev, info->batt_health);
	info->batt_health = NULL;

	xm_info("battery health %s deinitialize success\n", XM_BATT_HEALTH_VERSION);

	return 0;
}

int xm_batt_health_run(struct mtk_charger *info)
{
	struct xm_batt_health *batt_health = info->batt_health;

	schedule_delayed_work(&batt_health->batt_health_work, msecs_to_jiffies(3000));

	return 0;
}

int xm_batt_health_stop(struct mtk_charger *info)
{
	struct xm_batt_health *batt_health = info->batt_health;

	cancel_delayed_work_sync(&batt_health->batt_health_work);

	return 0;
}