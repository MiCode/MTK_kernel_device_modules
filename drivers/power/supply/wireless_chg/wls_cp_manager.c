/* Copyright (c) 2022 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/* this driver is compatible for wireless quick charge policy engine */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/ktime.h>
#include "../pmic_voter.h"
#include "../mtk_charger.h"
#include "../bq28z610.h"
#include "wireless_charger_class.h"
#include "wls_cp_manager.h"
#include <linux/mca/common/mca_log.h>
#include <linux/mca/common/mca_event.h>
#include <linux/mca/common/mca_charge_mievent.h>

#ifndef MCA_LOG_TAG
#define MCA_LOG_TAG "mca_wls_sm_info"
#endif

static struct platform_driver wls_cp_driver;
static int log_level = 1;
static int vbus_low_gap_div = 1000;
module_param_named(vbus_low_gap_div, vbus_low_gap_div, int, 0600);
static int vbus_high_gap_div = 1150;
module_param_named(vbus_high_gap_div, vbus_high_gap_div, int, 0600);
static int ibus_gap_div = 50;
module_param_named(ibus_gap_div, ibus_gap_div, int, 0600);
static int force_cp_mode = -1;
module_param_named(force_cp_mode, force_cp_mode, int, 0600);

enum wls_sm_state {
	WLS_PM_STATE_ENTRY,
	WLS_PM_STATE_INIT_VBUS,
	WLS_PM_STATE_ENABLE_CP,
	WLS_PM_STATE_TUNE,
	WLS_PM_STATE_EXIT,
};
enum wls_sm_status {
	WLS_SM_CONTINUE,
	WLS_SM_HOLD,
	WLS_SM_EXIT,
};
struct wls_dts_config {
	int	fv;
	int	fv_ffc;
	int	max_fcc;
	int	max_vbus;
	int	max_ibus;
	int	fcc_low_hyst;
	int	fcc_high_hyst;
	int	low_tbat;
	int	medium_tbat;
	int	high_tbat;
	int	high_vbat;
	int	high_soc;
	int	low_fcc;
	int	cv_vbat;
	int	cv_vbat_ffc;
	int	cv_ibat;
	int	cv_ibat_warm;
	int	wls_cm_ibus_gap;
	int	switch2_1_enter;
	int	switch2_1_exit;
	int	pmic_parachg_icl;
	int	max_curr_dur_4_1;
	int	max_curr_dur_2_1;
};
struct wls_cp_pm {
	struct device *dev;
	struct charger_device *master_dev;
	struct charger_device *charger_dev;
	struct wireless_charger_device *wls_dev;
	struct power_supply *usb_psy;
	struct power_supply *bms_psy;
	struct power_supply *wls_psy;
	struct delayed_work main_sm_work;
	struct work_struct psy_change_work;
	struct notifier_block nb;
	spinlock_t psy_change_lock;
	struct wls_dts_config dts_config;
	struct votable *fcc_votable;
	struct votable *fv_votable;
	struct votable *icl_votable;
	ktime_t fastchg_time_start;
	enum wls_sm_state state;
	enum wls_sm_status sm_status;
	bool wls_cm_active;
	bool psy_change_running;
	bool master_cp_enable;
	bool primary_chg_enable;
	bool ffc_enable;
	bool charge_full;
	bool input_suspend;
	bool no_delay;
	bool night_charging;
	bool ucp_en_flag;
	bool switch_mode;
	bool smart_soclmt_trig;
	bool support_pmic_parachg;
	bool max_curr_chg_flag;
	int rx_vout_setted;
	int master_cp_vbus;
	int master_cp_vbatt;
	int primary_chg_vbus;
	int master_cp_ibus;
	int total_ibus;
	int jeita_chg_index;
	int soc;
	int ibat;
	int vbat;
	int tbat;
	int target_fcc;
	int thermal_limit_fcc;
	int step_chg_fcc;
	int target_icl;
	int sw_cv;
	int vbus_step;
	int ibus_step;
	int ibat_step;
	int vbat_step;
	int final_step;
	int request_voltage;
	int request_current;
	int entry_vbus;
	int entry_ibus;
	int vbus_low_gap;
	int vbus_high_gap;
	int tune_vbus_count;
	int enable_cp_count;
	int taper_count;
	int cp_work_mode;
	int bms_i2c_error_count;
	int ibus_gap;
	int cycle_count;
	int wls_qc_enable;
	int wls_adapter_type;
	int adp_mode;
};
static const unsigned char *wls_str[] = {
	"WLS_PM_STATE_ENTRY",
	"WLS_PM_STATE_INIT_VBUS",
	"WLS_PM_STATE_ENABLE_CP",
	"WLS_PM_STATE_TUNE",
	"WLS_PM_STATE_EXIT",
};

static const char * const wls_vendor[] = {
	[WLS_CHIP_VENDOR_FUDA1652] = "nuvolta_1652",
	[WLS_CHIP_VENDOR_SC96281] = "sc96281",
};

static struct mtk_charger *pinfo = NULL;

#define wls_cp_err(fmt, ...)						\
do {									\
	if (log_level >= 0)						\
		printk(KERN_ERR "[XMCHG_WLS] " fmt, ##__VA_ARGS__);	\
} while (0)

#define wls_cp_info(fmt, ...)						\
do {									\
	if (log_level >= 1)						\
		printk(KERN_ERR "[XMCHG_WLS] " fmt, ##__VA_ARGS__);	\
} while (0)

#define wls_cp_dbg(fmt, ...)						\
do {									\
	if (log_level >= 2)						\
		printk(KERN_ERR "[XMCHG_WLS] " fmt, ##__VA_ARGS__);	\
} while (0)

#define is_between(left, right, value)				\
		(((left) >= (right) && (left) >= (value)	\
			&& (value) >= (right))			\
		|| ((left) <= (right) && (left) <= (value)	\
			&& (value) <= (right)))


static bool wls_check_cp_dev(struct wls_cp_pm *wls_cm)
{
	if (!wls_cm->master_dev)
		wls_cm->master_dev = get_charger_by_name("cp_master");
	if (!wls_cm->master_dev) {
		wls_cp_err("failed to get master_dev\n");
		return false;
	}
	if (!wls_cm->charger_dev)
		wls_cm->charger_dev = get_charger_by_name("primary_chg");
	if (!wls_cm->charger_dev) {
		wls_cp_err("failed to get charger_dev\n");
		return false;
	}
	if (!wls_cm->wls_dev)
		wls_cm->wls_dev = get_wireless_charger_by_name(wls_vendor[wls_get_chip_vendor()]);
	if (!wls_cm->wls_dev) {
		wls_cp_err("failed to get wireless charge dev\n");
		return false;
	}
	return true;
}

static bool wls_check_psy(struct wls_cp_pm *wls_cm)
{
	if (!wls_cm->usb_psy)
		wls_cm->usb_psy = power_supply_get_by_name("usb");
	if (!wls_cm->usb_psy) {
		wls_cp_err("failed to get usb_psy\n");
		return false;
	} else {
		pinfo = (struct mtk_charger *)power_supply_get_drvdata(wls_cm->usb_psy);
	}

	if (!wls_cm->wls_psy)
		wls_cm->wls_psy = power_supply_get_by_name("wireless");
	if (!wls_cm->wls_psy) {
		wls_cp_err("failed to get wls_psy\n");
		return false;
	}

	if (!wls_cm->bms_psy)
		wls_cm->bms_psy = power_supply_get_by_name("bms");
	if (!wls_cm->bms_psy) {
		wls_cp_err("failed to get bms_psy\n");
		return false;
	}

	return true;
}

static bool wls_check_votable(struct wls_cp_pm *wls_cm)
{
	if (!wls_cm->fcc_votable)
		wls_cm->fcc_votable = find_votable("CHARGER_FCC");
	if (!wls_cm->fcc_votable) {
		wls_cp_err("failed to get fcc_votable\n");
		return false;
	}
	if (!wls_cm->fv_votable)
		wls_cm->fv_votable = find_votable("CHARGER_FV");
	if (!wls_cm->fv_votable) {
		wls_cp_err("failed to get fv_votable\n");
		return false;
	}
	if (!wls_cm->icl_votable)
		wls_cm->icl_votable = find_votable("CHARGER_ICL");
	if (!wls_cm->icl_votable) {
		wls_cp_err("failed to get icl_votable\n");
		return false;
	}
	return true;
}

static void wls_update_status(struct wls_cp_pm *wls_cm)
{
	union power_supply_propval val = {0,};
	int temp = 0;
	charger_dev_get_vbus(wls_cm->master_dev, &wls_cm->master_cp_vbus);
	charger_dev_get_ibus(wls_cm->master_dev, &wls_cm->master_cp_ibus);
	charger_dev_cp_get_vbatt(wls_cm->master_dev, &wls_cm->master_cp_vbatt);
	charger_dev_is_enabled(wls_cm->master_dev, &wls_cm->master_cp_enable);
	charger_dev_is_enabled(wls_cm->charger_dev, &wls_cm->primary_chg_enable);
	charger_dev_get_vbus(wls_cm->charger_dev, &wls_cm->primary_chg_vbus);
	wireless_charger_dev_wls_get_vout(wls_cm->wls_dev, &wls_cm->rx_vout_setted);
	wls_cm->total_ibus = wls_cm->master_cp_ibus;
	power_supply_get_property(wls_cm->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	wls_cm->ibat = val.intval / 1000;
	power_supply_get_property(wls_cm->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	wls_cm->vbat = val.intval / 1000;
	power_supply_get_property(wls_cm->bms_psy, POWER_SUPPLY_PROP_TEMP, &val);
	wls_cm->tbat = val.intval;
	power_supply_get_property(wls_cm->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	wls_cm->soc = val.intval;
	power_supply_get_property(wls_cm->bms_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &val);
	wls_cm->cycle_count = val.intval;
	usb_get_property(USB_PROP_JEITA_CHG_INDEX, &temp);
	wls_cm->jeita_chg_index = temp;
	usb_get_property(USB_PROP_FFC_ENABLE, &temp);
	wls_cm->ffc_enable = temp;
	usb_get_property(USB_PROP_CHARGE_FULL, &temp);
	wls_cm->charge_full = temp;
	usb_get_property(USB_PROP_SW_CV, &temp);
	wls_cm->sw_cv = temp;
	usb_get_property(USB_PROP_INPUT_SUSPEND, &temp);
	wls_cm->input_suspend = temp;
	bms_get_property(BMS_PROP_I2C_ERROR_COUNT, &temp);
	wls_cm->bms_i2c_error_count = temp;
	wls_cm->night_charging = night_charging_get_flag();
	wls_cm->step_chg_fcc = get_client_vote(wls_cm->fcc_votable, STEP_CHARGE_VOTER);
	wls_cm->thermal_limit_fcc = min(get_client_vote(wls_cm->fcc_votable, WLS_THERMAL_VOTER), wls_cm->dts_config.max_fcc);
	wls_cm->target_fcc = get_effective_result(wls_cm->fcc_votable);
	wls_cm->smart_soclmt_trig = smart_soclmt_get_flag();
	wls_cm->target_icl = get_effective_result(wls_cm->icl_votable);

	mca_log_err("bat[soc:%d v:%d i:%d t:%d cycnt:%d errcnt:%d], cp[en:%d vbus:%d ibus:%d vbat:%d], pmic[en:%d vbus:%d]",
		wls_cm->soc, wls_cm->vbat, wls_cm->ibat, wls_cm->tbat, wls_cm->cycle_count, wls_cm->bms_i2c_error_count,
		wls_cm->master_cp_enable, wls_cm->master_cp_vbus, wls_cm->master_cp_ibus, wls_cm->master_cp_vbatt,
		wls_cm->primary_chg_enable, wls_cm->primary_chg_vbus/1000);
	mca_log_err("st[jeita:%d ffc:%d full:%d sw_cv:%d susp:%d night:%d soclmt:%d icl:%d fcc:%d step_fcc:%d therm_fcc:%d vout:%d]",
		wls_cm->jeita_chg_index, wls_cm->ffc_enable, wls_cm->charge_full, wls_cm->sw_cv, wls_cm->input_suspend,
		wls_cm->night_charging, wls_cm->smart_soclmt_trig, wls_cm->target_icl, wls_cm->target_fcc, wls_cm->step_chg_fcc, wls_cm->thermal_limit_fcc, wls_cm->rx_vout_setted);
}

static int wls_mode_switch_state_machine(struct wls_cp_pm *wls_cm)
{
	int cp_mode = wls_cm->cp_work_mode;
	int high_soc = wls_cm->dts_config.high_soc;

//#ifdef CONFIG_FACTORY_BUILD
	//force_cp_mode = SC8561_FORWARD_4_1_CHARGER_MODE;
//#endif

	if (force_cp_mode != -1) {
		switch(force_cp_mode) {
		case SC8561_FORWARD_4_1_CHARGER_MODE:
			cp_mode = SC8561_FORWARD_4_1_CHARGER_MODE;
			break;
		case SC8561_FORWARD_2_1_CHARGER_MODE:
			cp_mode = SC8561_FORWARD_2_1_CHARGER_MODE;
			break;
		default:
			/* do nothing */
			break;
		}
		goto exit;
	}

	wireless_charger_dev_wls_get_adapter_chg_mode(wls_cm->wls_dev, &wls_cm->adp_mode);
	if (wls_cm->adp_mode == SC8561_FORWARD_2_1_CHARGER_MODE) {
		cp_mode = SC8561_FORWARD_2_1_CHARGER_MODE;
		goto exit;
	}

	if (wls_cm->soc > high_soc)
		goto exit;

	if(wls_cm->target_fcc < wls_cm->dts_config.switch2_1_enter) {
		cp_mode = SC8561_FORWARD_2_1_CHARGER_MODE;
	} else {
		if (cp_mode == SC8561_FORWARD_2_1_CHARGER_MODE && wls_cm->target_fcc < wls_cm->dts_config.switch2_1_exit)
			cp_mode = SC8561_FORWARD_2_1_CHARGER_MODE;
		else
			cp_mode = SC8561_FORWARD_4_1_CHARGER_MODE;
	}

exit:
	mca_log_err("mode:%d[force:%d adp:%d] soc:%d fcc:%d[2_1:%d %d]\n",
		cp_mode, force_cp_mode, wls_cm->adp_mode, wls_cm->soc,
		wls_cm->target_fcc, wls_cm->dts_config.switch2_1_enter, wls_cm->dts_config.switch2_1_exit);
	return cp_mode;
}

static void wls_multi_mode_switch(struct wls_cp_pm *wls_cm)
{
	int res = 0;
	int cp_mode;

	/* check cp mode by target fcc */
	cp_mode = wls_mode_switch_state_machine(wls_cm);
	if (wls_cm->cp_work_mode != cp_mode) {
		wls_cm->cp_work_mode = cp_mode;
		wls_cm->switch_mode = true;
	} else {
		wls_cm->switch_mode = false;
	}

	if(wls_cm->cp_work_mode ==SC8561_FORWARD_2_1_CHARGER_MODE)
		res = 2;
	else
		res = 4;

#ifdef CONFIG_FACTORY_BUILD
	wls_cm->vbus_low_gap = wls_cm->vbat * res * 5/100;
#else
	wls_cm->vbus_low_gap = wls_cm->vbat * res * 3/100;
#endif
	wls_cm->vbus_high_gap = wls_cm->vbat * res * 11/100;
	wls_cm->ibus_gap = wls_cm->dts_config.wls_cm_ibus_gap / res;

	if (wls_cm->wls_adapter_type <= WLS_ADAPTER_VOICE_BOX) {
		wls_cm->vbus_low_gap = wls_cm->vbus_low_gap / 2;
		wls_cm->vbus_high_gap = wls_cm->vbus_high_gap / 2;
	}

	wls_cm->entry_vbus = min(((wls_cm->vbat * res) + wls_cm->vbus_low_gap), wls_cm->dts_config.max_vbus);
	wls_cm->entry_ibus = min(((wls_cm->target_fcc / res) + wls_cm->ibus_gap), wls_cm->dts_config.max_ibus);

	mca_log_err("switch=%d, res=%d, gap=[%d,%d,%d], entry=[%d,%d] \n",
		wls_cm->switch_mode, res,
		wls_cm->vbus_low_gap, wls_cm->vbus_high_gap, wls_cm->ibus_gap,
		wls_cm->entry_vbus, wls_cm->entry_ibus);
}

static bool wls_taper_charge(struct wls_cp_pm *wls_cm)
{
	int cv_vbat = 0, cv_vote = 0, cv_ibat = 0;
	int para_icl = 0, res = 0;
	bool taper_chg = false;
	static int cnt = 0;

	if (is_between(wls_cm->dts_config.medium_tbat, wls_cm->dts_config.high_tbat, wls_cm->tbat))
		cv_ibat = wls_cm->dts_config.cv_ibat_warm;
	else
		cv_ibat = wls_cm->dts_config.cv_ibat;

	cv_vote = get_effective_result(wls_cm->fv_votable);
	cv_vbat = wls_cm->ffc_enable ? wls_cm->dts_config.cv_vbat_ffc : wls_cm->dts_config.cv_vbat;
	cv_vbat = cv_vbat < cv_vote ? cv_vbat : cv_vote;

	if (wls_cm->charge_full) {
		taper_chg = true;
		cnt = 0;
		goto exit;
	}

	if (wls_cm->vbat > cv_vbat && (-wls_cm->ibat) < cv_ibat)
		cnt++;
	else
		cnt = 0;
	taper_chg = (cnt > MAX_TAPER_COUNT)? true : false;

	if (wls_cm->taper_count != cnt && wls_cm->support_pmic_parachg) {
		res = (wls_cm->cp_work_mode == SC8561_FORWARD_4_1_CHARGER_MODE) ? 2 : 3;
		para_icl = (cnt != 0)? (wls_cm->dts_config.pmic_parachg_icl * res) : wls_cm->dts_config.pmic_parachg_icl;
		vote(wls_cm->icl_votable, WLS_PARACHG_VOTER, true, para_icl);
	}

exit:
	mca_log_err("taper:%d [soc:%d cv_vbat:%d cv_vote:%d cv_ibat:%d cnt:%d-%d full:%d]\n",
		taper_chg, wls_cm->soc, cv_vbat, cv_vote, cv_ibat, cnt, wls_cm->taper_count, wls_cm->charge_full);
	wls_cm->taper_count = cnt;
	return taper_chg;
}

static int wls_check_condition(struct wls_cp_pm *wls_cm)
{
	int min_fcc = 0;
	int switch_normal_chg = 0;
	int delta_time_s = 0, max_curr_duration_s = 0;

	if (wls_cm->state == WLS_PM_STATE_TUNE && wls_cm->target_fcc >= MAX_CURRENT) {
		if (wls_cm->fastchg_time_start == -1)
			wls_cm->fastchg_time_start = ktime_get();
		if (!wls_cm->max_curr_chg_flag) {
			delta_time_s = ktime_ms_delta(ktime_get(), wls_cm->fastchg_time_start) / 1000;
			max_curr_duration_s = (wls_cm->cp_work_mode == SC8561_FORWARD_2_1_CHARGER_MODE)? wls_cm->dts_config.max_curr_dur_2_1 : wls_cm->dts_config.max_curr_dur_4_1;
			mca_log_err("max curr ctrl, delta_time_s=%d, max_curr_duration_s=%d\n", delta_time_s, max_curr_duration_s);
			if (delta_time_s >= max_curr_duration_s) {
				wls_cm->max_curr_chg_flag = true;
				vote(wls_cm->fcc_votable, WLS_POWER_REDUCE_VOTER, true, MAX_CURRENT_REDUCE);
			}
		}
	} else {
		if (wls_cm->max_curr_chg_flag && wls_cm->target_fcc != MAX_CURRENT_REDUCE) {
			wls_cm->fastchg_time_start = -1;
			wls_cm->max_curr_chg_flag = false;
			vote(wls_cm->fcc_votable, WLS_POWER_REDUCE_VOTER, false, 0);
		}
	}

	if (wls_cm->cp_work_mode == SC8561_FORWARD_2_1_CHARGER_MODE)
		min_fcc = MIN_2_1_CHARGE_CURRENT;
	else
		min_fcc = MIN_4_1_CHARGE_CURRENT;

	usb_get_property(USB_PROP_SWITCH_NORMAL_CHG, &switch_normal_chg);

	if (wls_cm->state == WLS_PM_STATE_TUNE && wls_taper_charge(wls_cm)) {
		mca_log_err("WLS_SM_EXIT wls_taper_charge state=%d\n", wls_cm->state);
		return WLS_SM_EXIT;
	} else if (wls_cm->state == WLS_PM_STATE_TUNE && (!wls_cm->master_cp_enable)) {
		mca_log_err("WLS_SM_HOLD state=%d, cp_enable= %d\n", wls_cm->state, wls_cm->master_cp_enable);
		return WLS_SM_HOLD;
	} else if (wls_cm->switch_mode) {
		mca_log_err("WLS_SM_HOLD state=%d, switch_mode=%d\n", wls_cm->state, wls_cm->switch_mode);
		return WLS_SM_HOLD;
	} else if (wls_cm->input_suspend) {
		mca_log_err("WLS_SM_HOLD input_suspend=%d\n", wls_cm->input_suspend);
		return WLS_SM_HOLD;
	} else if (!is_between(MIN_JEITA_CHG_INDEX_WLS, MAX_JEITA_CHG_INDEX, wls_cm->jeita_chg_index)) {
		mca_log_err("WLS_SM_HOLD for jeita jeita_chg_index=%d\n", wls_cm->jeita_chg_index);
		return WLS_SM_HOLD;
	} else if (wls_cm->target_fcc < min_fcc) {
		mca_log_err("WLS_SM_HOLD for fcc target_fcc=%d, min=%d\n", wls_cm->target_fcc, min_fcc);
		return WLS_SM_HOLD;
	} else if ((wls_cm->state == WLS_PM_STATE_ENTRY) && ((wls_cm->soc > wls_cm->dts_config.high_soc)
			|| (!is_between(100, 200, wls_cm->cycle_count) && (wls_cm->soc > 92))
			|| ((wls_cm->cycle_count > 200) && (wls_cm->soc > 90)))) {
		mca_log_err("WLS_SM_EXIT state=%d,soc=%d,vbat=%d\n", wls_cm->state, wls_cm->soc, wls_cm->vbat);
		return WLS_SM_EXIT;
	} else if (wls_cm->smart_soclmt_trig) {
		mca_log_err("WLS_SM_HOLD smart soclmt state=%d,soc=%d\n", wls_cm->state, wls_cm->soc);
		return WLS_SM_HOLD;
	} else if (wls_cm->bms_i2c_error_count >= 10) {
		mca_log_err("WLS_SM_EXIT i2c_error_count=%d\n", wls_cm->bms_i2c_error_count);
		return WLS_SM_EXIT;
	} else if(switch_normal_chg == 1){
		mca_log_err("WLS_SM_EXIT switch_normal_chg=%d\n", switch_normal_chg);
		return WLS_SM_EXIT;
	} else
		return WLS_SM_CONTINUE;
}

static int wls_cm_tune_rx_vout(struct wls_cp_pm *wls_cm)
{
	int fv = 0, ibus_limit = 0, vbus_limit = 0;
	int request_voltage = 0, request_current = 0;
	int final_step = 0, ret = 0, res = 0, fv_vote = 0;

	if(wls_cm->cp_work_mode == SC8561_FORWARD_2_1_CHARGER_MODE)
		res = 2;
	else
		res = 4;

	fv_vote = get_effective_result(wls_cm->fv_votable);
	if (wls_cm->sw_cv >= 4000) {
		fv = wls_cm->sw_cv;
	} else {
		fv = wls_cm->ffc_enable ? wls_cm->dts_config.fv_ffc : wls_cm->dts_config.fv;
		fv = fv < fv_vote ? fv : fv_vote;
	}

	if (wls_cm->request_voltage > wls_cm->master_cp_vbus + wls_cm->total_ibus * MAX_CABLE_RESISTANCE / 1000)
		wls_cm->request_voltage = wls_cm->master_cp_vbus + wls_cm->total_ibus * MAX_CABLE_RESISTANCE / 1000;
	wls_cm->ibat_step = wls_cm->vbat_step = wls_cm->ibus_step = wls_cm->vbus_step = 0;
	ibus_limit = min((wls_cm->target_fcc / res + wls_cm->ibus_gap), wls_cm->dts_config.max_ibus);
	vbus_limit = wls_cm->dts_config.max_vbus;

	if ((-wls_cm->ibat) < (wls_cm->target_fcc - wls_cm->dts_config.fcc_low_hyst)) {
		if (((wls_cm->target_fcc - wls_cm->dts_config.fcc_low_hyst) - (-wls_cm->ibat)) > LARGE_IBAT_DIFF)
			wls_cm->ibat_step = LARGE_STEP;
		else if (((wls_cm->target_fcc - wls_cm->dts_config.fcc_low_hyst) - (-wls_cm->ibat)) > MEDIUM_IBAT_DIFF)
			wls_cm->ibat_step = MEDIUM_STEP;
		else
			wls_cm->ibat_step = SMALL_STEP;
	} else if ((-wls_cm->ibat) > (wls_cm->target_fcc + wls_cm->dts_config.fcc_high_hyst)) {
		if (((-wls_cm->ibat) - (wls_cm->target_fcc + wls_cm->dts_config.fcc_high_hyst)) > MEDIUM_IBAT_DIFF)
			wls_cm->ibat_step = -LARGE_STEP;
		else if (((-wls_cm->ibat) - (wls_cm->target_fcc + wls_cm->dts_config.fcc_high_hyst)) > SMALL_IBAT_DIFF)
			wls_cm->ibat_step = -MEDIUM_STEP;
		else
			wls_cm->ibat_step = -SMALL_STEP;
	} else {
		wls_cm->ibat_step = 0;
	}

	if (fv - wls_cm->vbat > LARGE_VBAT_DIFF)
		wls_cm->vbat_step = LARGE_STEP;
	else if (fv - wls_cm->vbat > MEDIUM_VBAT_DIFF)
		wls_cm->vbat_step = MEDIUM_STEP;
	else if (fv - wls_cm->vbat > 10)
		wls_cm->vbat_step = SMALL_STEP;
	else if (fv - wls_cm->vbat <= -1)
		wls_cm->vbat_step = -MEDIUM_STEP;
	else if (fv - wls_cm->vbat < 6)
		wls_cm->vbat_step = -SMALL_STEP;

	if (ibus_limit - wls_cm->total_ibus > (LARGE_IBUS_DIFF / (res / 2)))
		wls_cm->ibus_step = LARGE_STEP;
	else if (ibus_limit - wls_cm->total_ibus > (MEDIUM_IBUS_DIFF / (res / 2)))
		wls_cm->ibus_step = SMALL_STEP;
	else if (ibus_limit - wls_cm->total_ibus > SMALL_IBUS_DIFF)
		wls_cm->ibus_step = SMALL_STEP;
	else if (ibus_limit - wls_cm->total_ibus < -SMALL_IBUS_DIFF)
		wls_cm->ibus_step = -SMALL_STEP;

	if (vbus_limit - wls_cm->master_cp_vbus > LARGE_VBUS_DIFF)
		wls_cm->vbus_step = LARGE_STEP;
	else if (vbus_limit - wls_cm->master_cp_vbus > MEDIUM_VBUS_DIFF)
		wls_cm->vbus_step = SMALL_STEP;
	else if (vbus_limit - wls_cm->master_cp_vbus > 0)
		wls_cm->vbus_step = SMALL_STEP;
	else
		wls_cm->vbus_step = -SMALL_STEP;

	final_step = min(min(wls_cm->ibat_step, wls_cm->vbat_step), min(wls_cm->ibus_step, wls_cm->vbus_step));
	wls_cm->final_step = final_step;
	mca_log_err("Final_step:%d ibat_step:%d[fcc:%d hy:%d %d], vbat_step:%d[fv:%d], ibus_step:%d[lmt:%d], vbus_step:%d[lmt:%d]",
		wls_cm->final_step,
		wls_cm->ibat_step, wls_cm->target_fcc, wls_cm->dts_config.fcc_high_hyst, wls_cm->dts_config.fcc_low_hyst,
		wls_cm->vbat_step, fv, wls_cm->ibus_step, ibus_limit, wls_cm->vbus_step, vbus_limit);

	if (wls_cm->final_step) {
		request_voltage = min(wls_cm->request_voltage + wls_cm->final_step * STEP_MV, vbus_limit);
		request_current = ibus_limit;
		ret = wireless_charger_dev_wls_set_vout(wls_cm->wls_dev, request_voltage);
		if (!ret) {
			mca_log_err("tune rx success, req_vol=%d, req_cur=%d\n", request_voltage, request_current);
			wls_cm->request_voltage = request_voltage;
			wls_cm->request_current = request_current;
		} else {
			mca_log_err("tune rx fail, keep ori req_vol=%d, req_cur=%d\n", wls_cm->request_voltage, wls_cm->request_current);
		}
	}

	if ((!wls_cm->ucp_en_flag) && (wls_cm->master_cp_ibus > 500)) {
		wls_cm->ucp_en_flag = true;
		charger_dev_enable_cp_ucp(wls_cm->master_dev, true);
	}

	return ret;
}

static void wls_move_sm(struct wls_cp_pm *wls_cm, enum wls_sm_state state)
{
	mca_log_err("state change:%s -> %s\n", wls_str[wls_cm->state], wls_str[state]);
	wls_cm->state = state;
	wls_cm->no_delay = true;
}

static bool wls_handle_sm(struct wls_cp_pm *wls_cm)
{
	int res = 0, temp =0;
	int cnt = 0;

	switch (wls_cm->state) {
	case WLS_PM_STATE_ENTRY:
		wls_cm->tune_vbus_count = 0;
		wls_cm->enable_cp_count = 0;
		wls_cm->taper_count = 0;
		wls_cm->final_step = 0;
		wls_cm->step_chg_fcc = 0;
		wls_cm->fastchg_time_start = -1;

		wls_cm->sm_status = wls_check_condition(wls_cm);
		if (wls_cm->sm_status == WLS_SM_EXIT) {
			wls_move_sm(wls_cm, WLS_PM_STATE_EXIT);
			wls_cp_info("WLS_SM_EXIT, exit sm\n");
			// return true;
		} else if (wls_cm->sm_status == WLS_SM_HOLD) {
			wireless_charger_dev_wls_set_vout(wls_cm->wls_dev, EXIT_CP_SM_VOLTAGE);
			msleep(500);
			charger_dev_get_vbus(wls_cm->charger_dev, &wls_cm->primary_chg_vbus);
			wls_cp_err("wls get vbus=%d\n", wls_cm->primary_chg_vbus);
			if (!wls_cm->support_pmic_parachg && wls_cm->primary_chg_vbus < 12000000)
				charger_dev_enable_pmic_ovp(wls_cm->master_dev, true);
			break;
		} else {
			if (wls_cm->cp_work_mode == SC8561_FORWARD_2_1_CHARGER_MODE) {
				charger_dev_cp_set_mode(wls_cm->master_dev, SC8561_FORWARD_2_1_CHARGER_MODE);
				charger_dev_cp_device_init(wls_cm->master_dev, CP_FORWARD_2_TO_1);
			} else {
				charger_dev_cp_set_mode(wls_cm->master_dev, SC8561_FORWARD_4_1_CHARGER_MODE);
				charger_dev_cp_device_init(wls_cm->master_dev, CP_FORWARD_4_TO_1);
			}
			charger_dev_cp_enable_adc(wls_cm->master_dev, true);
			charger_dev_enable_cp_ucp(wls_cm->master_dev, false);
			wls_move_sm(wls_cm, WLS_PM_STATE_INIT_VBUS);
			mca_log_err("enter wls_cp_pm_state_entry cp_work_mode=%d\n", wls_cm->cp_work_mode);
		}
		break;
	case WLS_PM_STATE_INIT_VBUS:
		if(wls_cm->cp_work_mode == SC8561_FORWARD_2_1_CHARGER_MODE)
			res = 2;
		else
			res = 4;
		wireless_charger_dev_wls_set_parallel_charge(wls_cm->wls_dev, true);
		wls_cm->tune_vbus_count++;
		if (wls_cm->tune_vbus_count == 1) {
			if (wls_cm->support_pmic_parachg) {
				mca_log_err("pmic cocooperate charging with %dmA\n", wls_cm->dts_config.pmic_parachg_icl);
				wls_set_property(WLS_PROP_PMIC_PARACHG_MODE, 1);
				vote(wls_cm->icl_votable, WLS_PARACHG_VOTER, true, wls_cm->dts_config.pmic_parachg_icl);
			} else if (!wls_cm->support_pmic_parachg && wls_cm->primary_chg_vbus > 3600000) {
				mca_log_err("wireless_charger_dev_wls_notify_cp_status pmic switch cp\n");
				wireless_charger_dev_wls_notify_cp_status(wls_cm->wls_dev, 1);
				charger_dev_enable_pmic_ovp(wls_cm->master_dev, false);
			}
			wls_cm->request_voltage = wls_cm->entry_vbus;
			wls_cm->request_current = wls_cm->entry_ibus;
			if (wls_cm->request_voltage < 3600)
				wls_cm->request_voltage = 3600;
			/* set entry vbus to rx */
			wireless_charger_dev_wls_set_vout(wls_cm->wls_dev, wls_cm->request_voltage);
			msleep(1000);
			break;
		}
		if (wls_cm->tune_vbus_count >= MAX_VBUS_TUNE_COUNT) {
			mca_log_err("failed to tune VBUS to target window, exit wls sm\n");
			wls_cm->sm_status = WLS_SM_EXIT;
			wls_move_sm(wls_cm, WLS_PM_STATE_EXIT);
			break;
		}
		if (wls_cm->master_cp_vbus <= wls_cm->vbat * res + wls_cm->vbus_low_gap) {
			wls_cm->request_voltage += res * STEP_MV;
		} else if (wls_cm->master_cp_vbus >= wls_cm->vbat * res + wls_cm->vbus_high_gap) {
			wls_cm->request_voltage -= res * STEP_MV;
		} else {
			mca_log_err("success to tune VBUS to target window\n");
			wls_move_sm(wls_cm, WLS_PM_STATE_ENABLE_CP);
			break;
		}
		wireless_charger_dev_wls_set_vout(wls_cm->wls_dev, wls_cm->request_voltage);
		msleep(500);
		break;
	case WLS_PM_STATE_ENABLE_CP:
		wls_cm->enable_cp_count++;
		if (wls_cm->enable_cp_count >= MAX_ENABLE_CP_COUNT) {
			mca_log_err("failed to enable charge pump, exit WLS sm\n");
			charger_dev_cp_set_en_fail_status(wls_cm->master_dev, true);
			wls_cm->sm_status = WLS_SM_EXIT;
			wls_move_sm(wls_cm, WLS_PM_STATE_EXIT);
			break;
		}
		if (!wls_cm->master_cp_enable)
			charger_dev_enable(wls_cm->master_dev, true);
		if (wls_cm->master_cp_enable) {
			mca_log_err("success to enable charge pump\n");
			charger_dev_cp_dump_register(wls_cm->master_dev);
			charger_dev_enable_termination(wls_cm->charger_dev, false);
			wls_move_sm(wls_cm, WLS_PM_STATE_TUNE);
		} else {
			mca_log_err("failed to enable cp, rx vout settted =%d, try again\n", wls_cm->rx_vout_setted);
			charger_dev_cp_dump_register(wls_cm->master_dev);
			wls_cm->request_voltage += 50;
			wireless_charger_dev_wls_set_vout(wls_cm->wls_dev, wls_cm->request_voltage);
			msleep(500);
			break;
		}
		break;
	case WLS_PM_STATE_TUNE:
		wls_cm->sm_status = wls_check_condition(wls_cm);
		if (wls_cm->sm_status == WLS_SM_EXIT) {
			wireless_charger_dev_wls_notify_cp_status(wls_cm->wls_dev, 2);
			mca_log_err("taper charge done\n");
			wls_move_sm(wls_cm, WLS_PM_STATE_EXIT);
		} else if (wls_cm->sm_status == WLS_SM_HOLD) {
			wireless_charger_dev_wls_notify_cp_status(wls_cm->wls_dev, 2);
			mca_log_err("WLS_SM_HOLD\n");
			wls_move_sm(wls_cm, WLS_PM_STATE_EXIT);
		} else {
			wls_cm_tune_rx_vout(wls_cm);
		}
		break;
	case WLS_PM_STATE_EXIT:
		wls_cm->tune_vbus_count = 0;
		wls_cm->enable_cp_count = 0;
		wls_cm->taper_count = 0;
		wls_cm->ucp_en_flag = false;
		wls_cm->fastchg_time_start = -1;
		wireless_charger_dev_wls_set_vout(wls_cm->wls_dev, EXIT_CP_SM_VOLTAGE);
		do {
			msleep(500);
			charger_dev_get_vbus(wls_cm->master_dev, &wls_cm->master_cp_vbus);
			charger_dev_get_vbus(wls_cm->charger_dev, &wls_cm->primary_chg_vbus);
			wls_cp_err("wls sm exit vbus=%d %d\n", wls_cm->master_cp_vbus, wls_cm->primary_chg_vbus);
			if (wls_cm->master_cp_vbus < 1300000 && wls_cm->primary_chg_vbus < 13000000)
				break;
		} while (cnt++ < 5);
		if (!wls_cm->support_pmic_parachg && wls_cm->primary_chg_vbus < 12000000)
			charger_dev_enable_pmic_ovp(wls_cm->master_dev, true);
		if (!wls_cm->input_suspend)
			charger_dev_enable_powerpath(wls_cm->charger_dev, true);
		vote(wls_cm->icl_votable, WLS_PARACHG_VOTER, false, 0);
		vote(wls_cm->icl_votable, WLS_CHG_VOTER, true, 850);
		if (wls_cm->max_curr_chg_flag)
			vote(wls_cm->fcc_votable, WLS_POWER_REDUCE_VOTER, false, 0);
		wls_cm->max_curr_chg_flag = false;
		if (wls_cm->support_pmic_parachg)
			wls_set_property(WLS_PROP_PMIC_PARACHG_MODE, 0);
		mca_log_err("icl[%d %s] fcc[%d %s]\n",
			get_effective_result(wls_cm->icl_votable), get_effective_client(wls_cm->icl_votable),
			get_effective_result(wls_cm->fcc_votable), get_effective_client(wls_cm->fcc_votable));
		charger_dev_enable_cp_ucp(wls_cm->master_dev, true);
		charger_dev_enable(wls_cm->master_dev, false);
		msleep(2000);
		wireless_charger_dev_wls_set_parallel_charge(wls_cm->wls_dev, false);
		charger_dev_enable_termination(wls_cm->charger_dev, true);
		if (wls_cm->charge_full) {
			msleep(1000);
			usb_get_property(USB_PROP_CHARGE_FULL, &temp);
			wls_cm->charge_full = temp;
			if (wls_cm->charge_full && wls_cm->soc == 100){
				mca_log_err("charge_full wls exit, disable charging\n");
				charger_dev_enable(wls_cm->charger_dev, false);
			}
		} else if (wls_cm->smart_soclmt_trig)
			charger_dev_enable(wls_cm->charger_dev, false);
		charger_dev_cp_dump_register(wls_cm->master_dev);
		if (wls_cm->sm_status == WLS_SM_EXIT) {
			wls_cm->cp_work_mode = SC8561_FORWARD_4_1_CHARGER_MODE;
			charger_dev_cp_set_mode(wls_cm->master_dev, SC8561_FORWARD_4_1_CHARGER_MODE);
			charger_dev_cp_device_init(wls_cm->master_dev, CP_FORWARD_4_TO_1);
			return true;
		} else if (wls_cm->sm_status == WLS_SM_HOLD)
			wls_move_sm(wls_cm, WLS_PM_STATE_ENTRY);
		break;
	default:
		wls_cp_err("not supportted wls_sm_state\n");
		break;
	}
	return false;
}

static void wls_main_sm(struct work_struct *work)
{
	struct wls_cp_pm *wls_cm = container_of(work, struct wls_cp_pm, main_sm_work.work);
	int internal = WLS_SM_DELAY_500MS;

	wls_update_status(wls_cm);
	wls_multi_mode_switch(wls_cm);

	if (!wls_handle_sm(wls_cm) && wls_cm->wls_cm_active) {
		if (wls_cm->no_delay) {
			internal = 0;
			wls_cm->no_delay = false;
		} else {
			switch (wls_cm->state) {
			case WLS_PM_STATE_ENTRY:
			case WLS_PM_STATE_EXIT:
			case WLS_PM_STATE_INIT_VBUS:
				internal = WLS_SM_DELAY_300MS;
				break;
			case WLS_PM_STATE_ENABLE_CP:
				internal = WLS_SM_DELAY_500MS;
				break;
			case WLS_PM_STATE_TUNE:
				internal = WLS_SM_DELAY_1500MS;
				break;
			default:
				wls_cp_err("not supportted pdm_sm_state\n");
				break;
			}
		}
		schedule_delayed_work(&wls_cm->main_sm_work, msecs_to_jiffies(internal));
	}
}

static void wls_pm_disconnect(struct wls_cp_pm *wls_cm)
{
	cancel_delayed_work_sync(&wls_cm->main_sm_work);
	charger_dev_enable(wls_cm->master_dev, false);
	charger_dev_cp_enable_adc(wls_cm->master_dev, false);
	charger_dev_enable_cp_ucp(wls_cm->master_dev, true);
	//charger_dev_enable_powerpath(wls_cm->charger_dev, true);
	vote(wls_cm->fcc_votable, WLS_POWER_REDUCE_VOTER, false, 0);
	wls_cm->wls_adapter_type = WLS_ADAPTER_UNKNOWN;
	wls_cm->wls_qc_enable = 0;
	wls_cm->tune_vbus_count = 0;
	wls_cm->enable_cp_count = 0;
	wls_cm->final_step = 0;
	wls_cm->step_chg_fcc = 0;
	wls_cm->thermal_limit_fcc = 0;
	wls_cm->bms_i2c_error_count = 0;
	wls_cm->ucp_en_flag = false;
	wls_cm->max_curr_chg_flag = false;
	wls_cm->fastchg_time_start = -1;
	wls_cm->adp_mode = SC8561_FORWARD_4_1_CHARGER_MODE;
	if (!wls_cm->support_pmic_parachg)
		charger_dev_enable_pmic_ovp(wls_cm->master_dev, true);
	wls_move_sm(wls_cm, WLS_PM_STATE_ENTRY);
}

static void wls_psy_change(struct work_struct *work)
{
	struct wls_cp_pm *wls_cm = container_of(work, struct wls_cp_pm, psy_change_work);
	int ret = 0, val = 0;
	bool is_vout_setting_ready = false;

	wls_cp_info("wls_psy_change\n");

	ret = wls_get_property(WLS_PROP_QC_ENABLE, &val);
	if (ret < 0) {
		wls_cp_err("failed to get qc enable\n");
		goto out;
	} else
		wls_cm->wls_qc_enable = val;

	ret = wls_get_property(WLS_PROP_TX_ADAPTER, &val);
	if (ret < 0) {
		wls_cp_err("failed to get tx adapter\n");
		goto out;
	} else
		wls_cm->wls_adapter_type = val;

	wireless_charger_dev_is_vout_range_set_done(wls_cm->wls_dev, &is_vout_setting_ready);

	mca_log_err("adapter:%d, wls_qc_enable:%d, vout_range_ready:%d\n", wls_cm->wls_adapter_type, wls_cm->wls_qc_enable, is_vout_setting_ready);

	if ((!wls_cm->wls_cm_active) && wls_cm->wls_qc_enable && is_vout_setting_ready && (wls_cm->wls_adapter_type >= WLS_ADAPTER_XIAOMI_QC3)) {
		wls_cm->wls_cm_active = true;
		wls_cp_info("set main_sm_work\n");
		wls_move_sm(wls_cm, WLS_PM_STATE_ENTRY);
		schedule_delayed_work(&wls_cm->main_sm_work, msecs_to_jiffies(1000));
	} else if (wls_cm->wls_cm_active && wls_cm->wls_adapter_type == WLS_ADAPTER_UNKNOWN) {
		wls_cm->wls_cm_active = false;
		wls_cp_info("cancel wls state machine\n");
		wls_pm_disconnect(wls_cm);
	}

out:
	wls_cm->psy_change_running = false;
}

static int wls_cm_psy_notifier_cb(struct notifier_block *nb, unsigned long event, void *data)
{
	struct wls_cp_pm *wls_cm = container_of(nb, struct wls_cp_pm, nb);
	struct power_supply *psy = data;
	unsigned long flags;

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	spin_lock_irqsave(&wls_cm->psy_change_lock, flags);
	if (strcmp(psy->desc->name, "wireless") == 0 && !wls_cm->psy_change_running) {
		wls_cm->psy_change_running = true;
		schedule_work(&wls_cm->psy_change_work);
	}
	spin_unlock_irqrestore(&wls_cm->psy_change_lock, flags);

	return NOTIFY_OK;
}


static ssize_t wls_show_log_level(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = snprintf(buf, PAGE_SIZE, "%d\n", log_level);
	wls_cp_info("show log_level = %d\n", log_level);
	return ret;
}

static ssize_t wls_store_log_level(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	ret = sscanf(buf, "%d", &log_level);
	wls_cp_info("store log_level = %d\n", log_level);
	return count;
}

static ssize_t wls_show_switch2_1_enter(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct wls_cp_pm *wls_cm = dev_get_drvdata(dev);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", wls_cm->dts_config.switch2_1_enter);
	wls_cp_info("show switch2_1_enter= %d\n", wls_cm->dts_config.switch2_1_enter);

	return ret;
}

static ssize_t wls_store_switch2_1_enter(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct wls_cp_pm *wls_cm = dev_get_drvdata(dev);
	int ret = 0;
	int val = 0;

	ret = sscanf(buf, "%d", &val);
	wls_cm->dts_config.switch2_1_enter  = val;
	wls_cp_info("store switch2_1_enter = %d\n", wls_cm->dts_config.switch2_1_enter);

	return count;
}

static ssize_t wls_show_switch2_1_exit(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct wls_cp_pm *wls_cm = dev_get_drvdata(dev);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", wls_cm->dts_config.switch2_1_exit);
	wls_cp_info("show switch2_1_exit = %d\n", wls_cm->dts_config.switch2_1_exit);

	return ret;
}

static ssize_t wls_store_switch2_1_exit(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct wls_cp_pm *wls_cm = dev_get_drvdata(dev);
	int ret = 0;
	int val = 0;

	ret = sscanf(buf, "%d", &val);
	wls_cm->dts_config.switch2_1_exit = val;
	wls_cp_info("store switch2_1_exit = %d\n", wls_cm->dts_config.switch2_1_exit);

	return count;
}

static DEVICE_ATTR(log_level, S_IRUGO | S_IWUSR, wls_show_log_level, wls_store_log_level);
static DEVICE_ATTR(switch2_1_enter, S_IRUGO | S_IWUSR, wls_show_switch2_1_enter, wls_store_switch2_1_enter);
static DEVICE_ATTR(switch2_1_exit, S_IRUGO | S_IWUSR, wls_show_switch2_1_exit, wls_store_switch2_1_exit);

static struct attribute *wls_attributes[] = {
	&dev_attr_log_level.attr,
	&dev_attr_switch2_1_enter.attr,
	&dev_attr_switch2_1_exit.attr,
	NULL,
};

static const struct attribute_group wls_attr_group = {
	.attrs = wls_attributes,
};

static int wls_policy_parse_dt(struct wls_cp_pm *wls_cm)
{
	struct device_node *node = wls_cm->dev->of_node;
	int rc = 0;

	if (!node) {
		wls_cp_err("device tree node missing\n");
		return -EINVAL;
	}

	rc = of_property_read_u32(node, "fv_ffc", &wls_cm->dts_config.fv_ffc);
	rc = of_property_read_u32(node, "fv", &wls_cm->dts_config.fv);
	rc = of_property_read_u32(node, "max_fcc", &wls_cm->dts_config.max_fcc);
	rc = of_property_read_u32(node, "max_vbus", &wls_cm->dts_config.max_vbus);
	rc = of_property_read_u32(node, "max_ibus", &wls_cm->dts_config.max_ibus);
	rc = of_property_read_u32(node, "fcc_low_hyst", &wls_cm->dts_config.fcc_low_hyst);
	rc = of_property_read_u32(node, "fcc_high_hyst", &wls_cm->dts_config.fcc_high_hyst);
	rc = of_property_read_u32(node, "low_tbat", &wls_cm->dts_config.low_tbat);
	rc = of_property_read_u32(node, "high_tbat", &wls_cm->dts_config.high_tbat);
	rc = of_property_read_u32(node, "high_vbat", &wls_cm->dts_config.high_vbat);
	rc = of_property_read_u32(node, "high_soc", &wls_cm->dts_config.high_soc);
	rc = of_property_read_u32(node, "low_fcc", &wls_cm->dts_config.low_fcc);
	rc = of_property_read_u32(node, "cv_vbat", &wls_cm->dts_config.cv_vbat);
	rc = of_property_read_u32(node, "cv_vbat_ffc", &wls_cm->dts_config.cv_vbat_ffc);
	rc = of_property_read_u32(node, "cv_ibat", &wls_cm->dts_config.cv_ibat);
	rc = of_property_read_u32(node, "wls_cm_ibus_gap", &wls_cm->dts_config.wls_cm_ibus_gap);
	rc = of_property_read_u32(node, "switch2_1_enter", &wls_cm->dts_config.switch2_1_enter);
	rc = of_property_read_u32(node, "switch2_1_exit", &wls_cm->dts_config.switch2_1_exit);

	if (of_property_read_u32(node, "cv_ibat_warm", &wls_cm->dts_config.cv_ibat_warm) < 0)
		wls_cm->dts_config.cv_ibat_warm = wls_cm->dts_config.cv_ibat;
	if (of_property_read_u32(node, "medium_tbat", &wls_cm->dts_config.medium_tbat) < 0)
		wls_cm->dts_config.medium_tbat = 350;
	if (of_property_read_u32(node, "max_curr_duration_4_1", &wls_cm->dts_config.max_curr_dur_4_1) < 0)
		wls_cm->dts_config.max_curr_dur_4_1 = 240;
	if (of_property_read_u32(node, "max_curr_duration_2_1", &wls_cm->dts_config.max_curr_dur_2_1) < 0)
		wls_cm->dts_config.max_curr_dur_2_1 = 90;

	wls_cm->support_pmic_parachg = of_property_read_bool(node, "support_pmic_parachg");
	if (wls_cm->support_pmic_parachg) {
		if (of_property_read_u32(node, "pmic_parachg_icl", &wls_cm->dts_config.pmic_parachg_icl) < 0)
			wls_cm->dts_config.pmic_parachg_icl = PMIC_PARALLEL_ICL;
	}

	mca_log_err("parse config, FV=%d, FV_FFC=%d, FCC=[%d %d %d], MAX_BUS=[%d %d], CV=[%d %d %d %d], ENTRY=[%d %d %d %d %d], SWICH=[%d %d] PARACHG=[%d %d]\n",
			wls_cm->dts_config.fv, wls_cm->dts_config.fv_ffc, wls_cm->dts_config.max_fcc, wls_cm->dts_config.fcc_low_hyst, wls_cm->dts_config.fcc_high_hyst,
			wls_cm->dts_config.max_vbus, wls_cm->dts_config.max_ibus, wls_cm->dts_config.cv_vbat, wls_cm->dts_config.cv_vbat_ffc, wls_cm->dts_config.cv_ibat, wls_cm->dts_config.cv_ibat_warm,
			wls_cm->dts_config.low_tbat, wls_cm->dts_config.high_tbat, wls_cm->dts_config.high_vbat, wls_cm->dts_config.high_soc, wls_cm->dts_config.low_fcc,
			wls_cm->dts_config.switch2_1_enter, wls_cm->dts_config.switch2_1_exit, wls_cm->support_pmic_parachg, wls_cm->dts_config.pmic_parachg_icl);
	return rc;
}

static const struct platform_device_id wls_cp_id[] = {
	{ "wls_cp_manager", 0},
	{},
};
MODULE_DEVICE_TABLE(platform, wls_cp_id);

static const struct of_device_id wls_of_match[] = {
	{ .compatible = "wls_cp_manager", },
	{},
};
MODULE_DEVICE_TABLE(of, wls_of_match);

static int wls_cp_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct wls_cp_pm *wls_cm;
	const struct of_device_id *of_id;

	of_id = of_match_device(wls_of_match, &pdev->dev);
	pdev->id_entry = of_id->data;
	wls_cm = kzalloc(sizeof(struct wls_cp_pm), GFP_KERNEL);
	if (!wls_cm)
		return -ENOMEM;
	wls_cm->dev = dev;
	spin_lock_init(&wls_cm->psy_change_lock);
	platform_set_drvdata(pdev, wls_cm);

	wls_cp_info("enter wls_cp probe\n");
	ret = wls_policy_parse_dt(wls_cm);
	if (ret < 0) {
		wls_cp_err("failed to parse DTS\n");
		return ret;
	}
	if (!wls_check_cp_dev(wls_cm)) {
		wls_cp_err("failed to check charger device\n");
		return -ENODEV;
	}
	if (!wls_check_psy(wls_cm)) {
		wls_cp_err("failed to check psy\n");
		return -ENODEV;
	}
	if (!wls_check_votable(wls_cm)) {
		wls_cp_err("failed to check votable\n");
		return -ENODEV;
	}

	if (!wls_cm->support_pmic_parachg)
		charger_dev_enable_pmic_ovp(wls_cm->master_dev, true);

	INIT_WORK(&wls_cm->psy_change_work, wls_psy_change);
	INIT_DELAYED_WORK(&wls_cm->main_sm_work, wls_main_sm);

	wls_cm->nb.notifier_call = wls_cm_psy_notifier_cb;
	power_supply_reg_notifier(&wls_cm->nb);

	wls_cm->cp_work_mode = SC8561_FORWARD_4_1_CHARGER_MODE;
	ret = sysfs_create_group(&wls_cm->dev->kobj, &wls_attr_group);
	if (ret) {
		wls_cp_err("failed to register sysfs\n");
		return ret;
	}

	wls_cp_info("wls_cp probe success\n");
	return ret;
}
static int wls_cp_remove(struct platform_device *pdev)
{
	struct wls_cp_pm *wls_cm = platform_get_drvdata(pdev);
	power_supply_unreg_notifier(&wls_cm->nb);
	cancel_delayed_work(&wls_cm->main_sm_work);
	cancel_work_sync(&wls_cm->psy_change_work);
	return 0;
}
static struct platform_driver wls_cp_driver = {
	.driver = {
		.name = "wls_cp_manager",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(wls_of_match),
	},
	.probe = wls_cp_probe,
	.remove = wls_cp_remove,
	.id_table = wls_cp_id,
};
module_platform_driver(wls_cp_driver);
MODULE_AUTHOR("xiezhichang");
MODULE_DESCRIPTION("charge pump manager for wireless");
MODULE_LICENSE("GPL");
