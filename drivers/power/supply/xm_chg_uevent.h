/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2025 Huaqin Technology(Shanghai) Co., Ltd.
 */

#ifndef __XM_CHG_UEVENT_H__
#define __XM_CHG_UEVENT_H__

#define MAX_BUNDLE_UEVENT_CNT (5)

enum xm_chg_uevent_type {
	CHG_UEVENT_DEFAULT_TYPE,
	CHG_UEVENT_SOC_DECIMAL,
	CHG_UEVENT_SOC_DECIMAL_RATE,
	CHG_UEVENT_QUICK_CHARGE_TYPE,
	CHG_UEVENT_SHUTDOWN_DELAY,
	CHG_UEVENT_CONNECTOR_TEMP,
	CHG_UEVENT_NTC_ALARM,
	CHG_UEVENT_LPD_DETECTION,
	CHG_UEVENT_REVERSE_QUICK_CHARGE,
	CHG_UEVENT_MAX_TYPE,
	CHG_UEVENT_CC_SHORT_VBUS,
};


enum xm_chg_uevent_bundle_type {
	CHG_UEVENT_BUNDLE_CHG_ANIMATION,
	CHG_UEVENT_BUNDLE_MAX_TYPE,
};

extern int xm_charge_uevent_report(int event_type, int event_value);
extern int xm_charge_uevents_bundle_report(int bundle_type, ...);

#endif /* __XM_CHG_UEVENT_H__ */
