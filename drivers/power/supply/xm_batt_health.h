/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2024 Huaqin Technology(Shanghai) Co., Ltd.
 */

 #ifndef __XM_BATT_HEALTH_H__
 #define __XM_BATT_HEALTH_H__

 #include <linux/device.h>
 #include <linux/power_supply.h>
 #include <linux/workqueue.h>
 #include <linux/mutex.h>

 #include "pmic_voter.h"

 #define XM_BATT_HEALTH_VERSION "4.0"

 enum batt_health_func_type {
	 /* Battery Health 1.0 */
	 BATT_HEALTH_NIGHT_SMART_CHG                = 0,
	 BATT_HEALTH_BATTERY_MANAGER,
	 BATT_HEALTH_SMART_TIME_LOCATION_CAP,
	 /* Battery Health 2.0 */
	 BATT_HEALTH_CYCLE_DROP_FV_V1,
	 BATT_HEALTH_UI_SOH,
	 BATT_HEALTH_SHIPMODE,
	 /* Battery Health 3.0 */
	 BATT_HEALTH_CYCLE_DROP_FV_V2,
	 BATT_HEALTH_STEP_JEITA_PROTECT,
	 BATT_HEALTH_SMART_TEMPERATURE_CAP,
	 BATT_HEALTH_100_SOC,
	 BATT_HEALTH_SMART_RECHARGE,
	 /* Battery Health 4.0 */
	 BATT_HEALTH_COOL_SLOW_CHG,
	 BATT_HEALTH_SMART_ANTI_AGING,
	 BATT_HEALTH_TEMPERATURE_CAP_ADJUST,
	 BATT_HEALTH_UNTYPITAL_CAP_ADJUST,
	 BATT_HEALTH_TIMES_CHG_CAP_ADJUST,
	 BATT_HEALTH_OVER_FV_PROTECT,
	 BATT_HEALTH_SMART_SHALLOW_DISCHG,
 };

 struct xm_batt_health {
	 struct device *dev;
	 struct mutex batt_health_work_lock;

	 /* votable */
	 struct votable *charger_fcc_votable;
	 struct votable *fv_votable;

	 /* psy */
	 struct power_supply *batt_psy;
	 struct power_supply *bms_psy;
	 struct power_supply *usb_psy;

	 /* state */
	 int effective_fcc;
	 int effective_fv;
	 int soc;
	 int tbat;
	 int vbat;
	 int pd_type;
	 int smart_batt;
	 int smart_fv;

	 /* functions on/off */
	 bool over_fv_protect_on;
	 bool night_smart_charge_on;

	 /* flags */
	 bool over_fv_flag;
	 bool night_charging_flag;

	 struct delayed_work batt_health_work;
 };

int xm_batt_health_init(struct mtk_charger *info);
int xm_batt_health_deinit(struct mtk_charger *info);
int xm_batt_health_run(struct mtk_charger *info);
int xm_batt_health_stop(struct mtk_charger *info);

 #endif /* __XM_BATT_HEALTH_H__ */