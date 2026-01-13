/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2024 Huaqin Technology(Shanghai) Co., Ltd.
 */

#ifndef __XM_SMART_CHG_H__
#define __XM_SMART_CHG_H__
#include <linux/device.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/platform_device.h>

#include "pmic_voter.h"

#define XM_SMART_CHG_VERSION "2.0"

#define OUTDOOR_DCP_CURRENT (1900)

enum smart_chg_status {
	SMART_CHG_SUCCESS = 0,
	SMART_CHG_ERROR   = 1,
};

enum smart_chg_func_type {
	SMART_CHG_FUNC_MIN             = 0,
	SMART_CHG_NAVI_DISCHARGE       = SMART_CHG_FUNC_MIN,
	SMART_CHG_OUTDOOR_CHARGE,
	SMART_CHG_LOW_BATT_FAST_CHG,
	SMART_CHG_LONG_CHG_PROTECT,
	SMART_CHG_FUNC_MAX             = 15,
};

struct smart_chg_func {
	bool func_on;            /* fucntion on/off from user space */
	bool active_flag;        /* function active */
	int func_val;            /* function parameter set from user space */
};

struct charger_screen_monitor {
	struct notifier_block disp_nb;
	int screen_state;
};

struct xm_smart_chg {
	struct device *dev;
	struct mutex smart_chg_work_lock;

	struct delayed_work smart_chg_work;

	/* votable */
	struct votable *charger_fcc_votable;//TBD
	struct votable *main_icl_votable;
	struct votable *fv_votable;

	/* psy */
	struct power_supply *batt_psy;
	struct power_supply *bms_psy;
	struct power_supply *usb_psy;

	struct smart_chg_func funcs[SMART_CHG_FUNC_MAX];

	int status;
	bool stop_charge;

	int soc;
	int plugin_soc;
	int plugin_board_temp;
	int vbat;
	int real_type;//vbus_type;
	int pd_type;//pd_active;
	int screen_state;
	int screen_back_state;
	int board_temp;
	int thermal_level;
	int low_fast_ffc;
	int normal_fast_ffc;
	int effective_fcc;
};

int xm_smart_chg_init(struct mtk_charger *info);
int xm_smart_chg_deinit(struct mtk_charger *info);
int xm_smart_chg_run(struct mtk_charger *info);
int xm_smart_chg_stop(struct mtk_charger *info);

/*---------------------- smart_chg_node store --------------------------------*/
/* |    byte3    |    byte2    |    byte1    |         byte0       | */
/* |         func value        |         func type        | on/off | */
/* 
 * bit0: function on/off bit set from user space of this function type
 * bit1~bit15: function type bits(eg, set bit1 means navigation discharge function)
 * bit16~bit31: function paramerter value set from user space of this function type
 */
/*----------------------------------------------------------------------------*/

/*---------------------- smart_chg_node show --------------------------------*/
/* |    byte3    |    byte2    |    byte1    |         byte0       | */
/* |           reserve         |        on/off bits       | status | */
/* 
 * bit0: smart charge status bit
 * bit1: SMART_CHG_NAVI_DISCHARGE on/off bit
 * bit2: SMART_CHG_OUTDOOR_CHARGE on/off bit
 * bit3: SMART_CHG_LOW_BATT_FAST_CHG on/off bit
 * bit4: SMART_CHG_LONG_CHG_PROTECT on/off bit
 * bit5~bit15: function on/off bits reserve
 * bit16~bit31: reserve
 */
/*----------------------------------------------------------------------------*/


#endif /* __XM_SMART_CHG_H__ */
