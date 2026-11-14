/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
 */

#ifndef __LENOVO_JEITA_H__
#define __LENOVO_JEITA_H__

#include "hq_cp_policy.h"
#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
#include "../charger_class/xm_adapter_class.h"
#endif
#include "../charger_class/hq_fg_class.h"
#include "../charger_class/hq_charger_class.h"
#include "hq_charger_manager.h"
#if IS_ENABLED(CONFIG_ONEWIRE_GPIO)
//#include "../battery_secrete/battery_auth_class.h"
#include "../tiny_battery_secrete/battery_auth_class.h"
#endif
#define MAX_STEP_JEITA_NUM          7
#define MAX_FFC_JEITA_NUM           2
#define COLD_RECHG_VOLT_OFFSET      100
#define TEMP_45_TO_58_VOL           4090
#define CURRENT_NOW_1A              500
#define WARM_RECHG_VOLT_OFFSET      130
#define TEMP_LEVEL_NEGATIVE_10      -100
#define TEMP_LEVEL_15               150
#define TEMP_LEVEL_35               350
#define TEMP_LEVEL_45               450
#define TEMP_LEVEL_58               580
#define INDEX_15_to_35              4
#define INDEX_35_to_48              5
#define WARM_RECHG_TEMP_OFFSET      20
#define TERM_DELTA_CV               8
#define HEAVY_LOAD_VOLTAGE          4470

#define LOW_TEMP_RECHG_OFFSET       200
#define NOR_TEMP_RECHG_OFFSET       100
#if IS_ENABLED(CONFIG_HUAQIN_SOFT_ITERM_SUPPORT) 
#define SOFT_ITERM_TIME             3000
#define ST_THRESHOLD_LOW_LIMIT_FV(fv_mv) (fv_mv * 9900 / 10000)
#define ST_THRESHOLD_LOW_LIMIT_SDP_FV(fv_mv) (fv_mv * 9960 / 10000)
#define ST_THRESHOLD_LOW_LIMIT_ITERM_SDP(iterm_ma) (iterm_ma * 100 / 100)
#define ST_THRESHOLD_LOW_LIMIT_ITERM(iterm_ma) (iterm_ma * 105 / 100)
#define HW_THRESHOLD_LOW_LIMIT_ITERM(iterm_ma) (iterm_ma * 95 / 100)
void soft_iterm_work(struct work_struct *work);
#endif
#define CHG_CYCLE_RESTORE_VOL       100
#define BATT_CYCLE_COUNT_OVER_100   100
#define BATT_CYCLE_COUNT_OVER_800   800

#if IS_ENABLED(CONFIG_BUILD_TARGET_TANZANITE)
#define BATT_MA_AVG_SAMPLES             8
#endif

int hq_jeita_init(struct device *dev);
void hq_jeita_deinit(void);
bool get_warm_stop_charge_state(void);
extern struct chargerpump_policy *g_policy;
#define HQ_SDP_HARD_ITERM_STEP         50

#endif /* __STEP_CHG_H__ */
