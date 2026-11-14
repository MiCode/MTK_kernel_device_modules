// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2023 Huaqin Technology(Shanghai) Co., Ltd.
 */

#include "hq_charger_manager.h"
#include "hq_printk.h"
#include <linux/syscore_ops.h>
#ifdef TAG
#undef TAG
#define  TAG "[HQ_CHG_CM]"
#endif
#if IS_ENABLED(CONFIG_XIAOMI_USB_TOUCH_NOTIFIER)
#include <misc/xiaomi_usb_touch_notifier.h>
#endif
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
#include "xm_smart_chg.h"
#include "hq_notify.h"
#include "mtk_battery.h"

static int float_count = 0;

#if IS_ENABLED(CONFIG_BC12_RETRY_FOR_MI_PD)
static bool bc12_retry_flag = 1;
#endif

static const char *bc12_result[] = {
	"None",
	"SDP",
	"CDP",
	"DCP",
	"QC",
	"FLOAT",
	"Non-Stand",
	"QC3",
	"QC3+",
	"PD",
};

const static char * adc_name[] = {
	"VBUS","VSYS","VBAT","VAC","IBUS","IBAT","TSBUS","TSBAT","TDIE",
};

/*thermal board temp*/
SRCU_NOTIFIER_HEAD(charger_thermal_notifier);
EXPORT_SYMBOL_GPL(charger_thermal_notifier);

int charger_thermal_reg_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&charger_thermal_notifier, nb);
}
EXPORT_SYMBOL_GPL(charger_thermal_reg_notifier);

int charger_thermal_unreg_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&charger_thermal_notifier, nb);
}
EXPORT_SYMBOL_GPL(charger_thermal_unreg_notifier);

int charger_thermal_notifier_call_chain(unsigned long event, int val)
{
	return srcu_notifier_call_chain(&charger_thermal_notifier, event, &val);
}
EXPORT_SYMBOL_GPL(charger_thermal_notifier_call_chain);
#endif

static int charger_manager_wake_thread(struct charger_manager *manager)
{
	manager->run_thread = true;
	wake_up(&manager->wait_queue);
	return 0;
}

#if IS_ENABLED(CONFIG_MIEV)
/******************************************************************************/
/*                         POWER SUPPLY NOTIFIER                              */
/******************************************************************************/
static int charger_manager_psy_notifier_call(struct notifier_block *nb,
					unsigned long event, void *data)
{
	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	/* TODO: fire routines related to power supply property changed
	 * WARNING: psy notifier is atomic notifier
	 */

	return NOTIFY_OK;
}

static int charger_manager_register_psy_notifier(struct charger_manager *manager)
{
	int ret = 0;

	manager->psy_nb.notifier_call = charger_manager_psy_notifier_call;

	ret = power_supply_reg_notifier(&manager->psy_nb);
	if (ret < 0) {
		hq_err("couldn't register psy notifier ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static void charger_manager_unregister_psy_notifier(struct charger_manager *manager)
{
	power_supply_unreg_notifier(&manager->psy_nb);
}


/******************************************************************************/
/*                            THERMAL NOTIFIER                                */
/******************************************************************************/
/*
static int charger_manager_thermal_notifier_call(struct notifier_block *notifier,
						unsigned long event, void *data)
{
	struct charger_manager *manager = container_of(notifier,
							struct charger_manager, thermal_nb);
	switch (event) {
	case EVENT_BOARDTEMP_CHANGE:
		manager->board_temp = (*(int *)data);
		hq_info("thermal notifier board_temp = %d\n", manager->board_temp);
		break;
	default:
		hq_err("unsupported thermal notifier event: %d\n", event);
		break;
	}
	return NOTIFY_DONE;
}

static int charger_manager_register_thermal_notifier(struct charger_manager *manager)
{
	int ret = 0;
	manager->thermal_nb.notifier_call = charger_manager_thermal_notifier_call;
	ret = mi_thermal_reg_notifier(&manager->thermal_nb);
	if (ret < 0) {
		hq_err("couldn't register thermal notifier, ret = %d\n", ret);
		return ret;
	}
	return ret;
}

static int charger_manager_unregister_thermal_notifier(struct charger_manager *manager)
{
	return mi_thermal_unreg_notifier(&manager->thermal_nb);
}
*/
/******************************************************************************/
/*                         CHARGER CHANGED NOTIFIER                           */
/******************************************************************************/
static int charger_manager_charger_changed_notifier_call(struct notifier_block *nb,
						unsigned long event, void *data)
{
	struct charger_manager *manager = container_of(nb,
							struct charger_manager, charger_changed_nb);
	charger_manager_wake_thread(manager);
	return NOTIFY_OK;
}

static int charger_manager_register_charger_changed_notifier(struct charger_manager *manager)
{
	int ret = 0;
	manager->charger_changed_nb.notifier_call = charger_manager_charger_changed_notifier_call;
	ret = charger_register_notifier(&manager->charger_changed_nb);
	if (ret < 0) {
		hq_err("couldn't register charger notifier, ret = %d\n", ret);
		return ret;
	}
	return ret;
}

static int charger_manager_unregister_charger_changed_notifier(struct charger_manager *manager)
{
	charger_unregister_notifier(&manager->charger_changed_nb);
        return NOTIFY_OK;
}

/******************************************************************************/
/*                             CHARGER NOTIFIER                               */
/******************************************************************************/
static int charger_manager_charger_notifier_call(struct notifier_block *nb,
						unsigned long event, void *data)
{
	// struct charger_manager *manager = container_of(nb,
	// 						struct charger_manager, charger_changed_nb);
	switch (event) {
	case CHARGER_EVENT_DEFAULT:
		hq_info("received charger notifier event: %lu\n", event);
		break;
	case CHARGER_EVENT_BC12_DONE:
		hq_info("received charger notifier event: %lu\n", event);
		break;
	case CHARGER_EVENT_HVDCP_DONE:
		hq_info("received charger notifier event: %lu\n", event);
		break;
	default:
		hq_err("unsupported charger notifier event: %lu\n", event);
		break;
	}
	return NOTIFY_OK;
}

static int charger_manager_fg_notifier_call(struct notifier_block *nb,
						unsigned long event, void *data)
{
	struct charger_manager *manager = container_of(nb,
							struct charger_manager, fg_nb);

	switch (event) {
	case FG_DAEMON_CMD_SET_AGING_FACTOR:
		manager->new_fg_raw_soh = (*(int *)data) / 100;
		schedule_delayed_work(&manager->update_fg_raw_soh_work, msecs_to_jiffies(2000));
		break;
	case FG_DAEMON_CMD_SET_BAT_CYCLES:
		manager->new_fg_cycle = (*(int *)data);
		schedule_delayed_work(&manager->update_fg_cycle_work, msecs_to_jiffies(0));
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static int charger_manager_register_charger_notifier(struct charger_manager *manager)
{
	int ret = 0;
	manager->charger_nb.notifier_call = charger_manager_charger_notifier_call;
	ret = hq_charger_notifier_register(&manager->charger_nb);
	if (ret < 0) {
		hq_err("couldn't register charger notifier, ret = %d\n", ret);
		return ret;
	}
	return ret;
}

static int charger_manager_register_fg_notifier(struct charger_manager *manager)
{
	int ret = 0;

	manager->fg_nb.notifier_call = charger_manager_fg_notifier_call;

	ret = hq_fg_notifier_register(&manager->fg_nb);
	if (ret < 0) {
		hq_err("couldn't register fg notifier, ret = %d\n", ret);
		return ret;
	}

	return ret;
}

static int charger_manager_unregister_charger_notifier(struct charger_manager *manager)
{
	return hq_charger_notifier_unregister(&manager->charger_nb);
}
#endif

#if IS_ENABLED(CONFIG_XIAOMI_USB_TOUCH_NOTIFIER)
static struct xiaomi_usb_notify_data xiaomi_touch_usb_data;
#endif

#if IS_ENABLED(CONFIG_HUAQIN_FUEL_ALGORITHM)
extern int hq_fuel_algo_init(struct charger_manager *manager);
#endif

static void hrtime_otg_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
					struct charger_manager, hrtime_otg_work.work);
	if(manager != NULL && manager->tcpc != NULL)
		tcpm_typec_change_role(manager->tcpc, TYPEC_ROLE_SNK);
	hq_info("hrtime_otg_work enter\n");
}

static enum alarmtimer_restart rust_det_work_timer_handler(struct alarm *alarm, ktime_t now)
{
	struct charger_manager *manager = container_of(alarm,
				struct charger_manager, rust_det_work_timer);
	if(manager != NULL)
	{
		manager->ui_cc_toggle = false;
		schedule_delayed_work(&manager->hrtime_otg_work, 0);
	}
	hq_info("rust_det_work_timer_handler enter\n");
    return ALARMTIMER_NORESTART;
}

static bool judge_cell_status(struct charger_manager *manager)
{
	int c_car_offset = 0;
	int v_car_offset = 0;
	static int count;

	if(manager->input_suspend){
		hq_info("[Detcell] input_suspend enable, skip Detcell\n");
		return false;
	}

	manager->c_car_out = fuel_gauge_get_c_car(manager->fuel_gauge);
	manager->v_car_out = fuel_gauge_get_v_car(manager->fuel_gauge);
	hq_info("[Detcell] c_car_out = %d, v_car_out = %d\n", manager->c_car_out, manager->v_car_out);

	c_car_offset = abs(manager->c_car_out - manager->c_car_in);
	v_car_offset = abs(manager->v_car_out - manager->v_car_in);
	hq_info("[Detcell] c_car_offset = %d, v_car_offset = %d\n", c_car_offset, v_car_offset);

	//18 fail，decrease to 17
	if ( v_car_offset  * 10 >= c_car_offset * 17) { //6*60 s
		if (count >= 1) {
			hq_info("[Detcell] cell is broken!\n");
			manager->half_cell = 2;
			return true;
		} else {
			count++;
			hq_info("[Detcell] Detcell count = %d\n", count);
			manager->half_cell = 1;
			return false;
		}
	} else {
		hq_info("[Detcell] Detcell cell is ok\n");
		manager->half_cell = 1;
		count = 0;
		return false;
	}

}

static enum alarmtimer_restart cell_det_work_timer_handler(struct alarm *alarm, ktime_t now)
{
	struct charger_manager *manager = container_of(alarm,
				struct charger_manager, cell_det_work_timer);
	if(manager != NULL)
	{
		manager->en_single_cell = judge_cell_status(manager);
		hq_info("[Detcell] cell_det_work_timer_handler enter\n");
	}

	return ALARMTIMER_NORESTART;
}

static enum alarmtimer_restart start_cell_det_work_timer_handler(struct alarm *alarm, ktime_t now)
{
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;

	struct charger_manager *manager = container_of(alarm,
				struct charger_manager, start_cell_det_work_timer);
	if(manager != NULL)
	{
		hq_info("[Detcell] cell_det_work_timer_handler enter\n");

		manager->v_car_in = fuel_gauge_get_v_car(manager->fuel_gauge);
		if (manager->tbat < 0 || manager->v_car_in <= 0) {
			hq_info("[Detcell] lowtemp or vcar error exit cell_det_work_timer\n");
			return ALARMTIMER_NORESTART;
		} else {
			manager->c_car_in = fuel_gauge_get_c_car(manager->fuel_gauge);
			hq_info("[Detcell] c_car_in = %d, v_car_in = %d\n", manager->c_car_in, manager->v_car_in);

			ret = alarm_try_to_cancel(&manager->cell_det_work_timer);
			if (ret < 0) {
				hq_err("[Detcell] callback was running, skip timer\n");
			}
			ktime_now = ktime_get_boottime();
			time_now = ktime_to_timespec64(ktime_now);
			end_time.tv_sec = time_now.tv_sec + 600;
			end_time.tv_nsec = time_now.tv_nsec + 0;
			ktime = ktime_set(end_time.tv_sec,end_time.tv_nsec);
			alarm_start(&manager->cell_det_work_timer, ktime);
		}
	}
	return ALARMTIMER_NORESTART;
}

int charger_manager_get_current(struct charger_manager *manager, int *curr)
{
	int val;
	int ret = 0;
	union power_supply_propval pval;

	*curr = 0;

	ret = charger_get_adc(manager->charger, CHG_ADC_IBUS, &val);
	if (ret < 0) {
		hq_err("Couldn't read input curr ret=%d\n", ret);
	} else
		*curr += val;

	if(manager->cp_master_psy){

		ret = power_supply_get_property(manager->cp_master_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
		if (ret < 0)
			hq_err("Couldn't get cp curr  by power supply ret=%d\n", ret);
		else
			*curr += pval.intval;
	}
	if(manager->cp_slave_psy){
		ret = power_supply_get_property(manager->cp_slave_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
		if (ret < 0)
			hq_err("Couldn't get cp curr  by power supply ret=%d\n", ret);
		else
			*curr += pval.intval;
	}
	return 0;
}
EXPORT_SYMBOL(charger_manager_get_current);

void hq_set_prop_system_temp_level(struct charger_manager *manager, char *voter_name)
{
	int rc;
	int thermal_level = 0;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	int tbat = 250;
	union power_supply_propval pval = {0,};
	struct timespec64 ts_boot;
	struct timespec64 ts_shiled;
#endif

	/* keep system_temp_level as last thermal level */
	thermal_level = manager->system_temp_level;

#ifdef KERNEL_FACTORY_BUILD
	/* force disable thermal policy in factory build */
	manager->thermal_enable = false;
#endif //KERNEL_FACTORY_BUILD

	if (manager->thermal_enable == false) {
		hq_err("thermal ibat limit is disable\n");
		return;
	}

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	/*
	 * force thermal level to zero within 60s of kernel bootup.
	 * battery temperature limitation to avoid trigger
	 * hot temperature shutdown when bootup.
	 */

	rc = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (rc < 0)
		hq_err("get battery temperature error.\n");
	else
		tbat = pval.intval;

	ktime_get_boottime_ts64(&ts_boot);
	ts_shiled.tv_sec = 60;
	ts_shiled.tv_nsec = 0;

	if ((timespec64_compare(&ts_boot, &ts_shiled) == -1) && (tbat < 450)) {
		hq_info("system_temp_level = %d, tbat = %d, force thermal level to zero.\n",
			manager->system_temp_level, tbat);
		thermal_level = 0;
	}
#endif

	if (thermal_level < 0)
		goto err;

	if (manager->pd_active == CHARGE_PD_PPS_ACTIVE || !strcmp(voter_name, CALL_THERMAL_DAEMON_VOTER)) {
		if (manager->thermal_parse_flags & PD_THERM_PARSE_ERROR) {
			hq_err("pd thermal dtsi parse error\n");
			goto err;
		}

		if (thermal_level > manager->pd_thermal_levels) {
			hq_err("thermal level is invalid\n");
			goto err;
		}

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
		if (manager->pps_fast_mode && (manager->low_fast_ffc >= 2150)) {
			vote(manager->total_fcc_votable, voter_name, true, manager->low_fast_ffc);
		} else {
			vote(manager->total_fcc_votable, voter_name, true,
				manager->pd_thermal_mitigation[thermal_level]);
		}
#else
		vote(manager->total_fcc_votable, voter_name, true,
			manager->pd_thermal_mitigation[thermal_level]);
#endif

	} else {
		if (manager->thermal_parse_flags & QC2_THERM_PARSE_ERROR) {
			hq_err("qc thermal dtsi parse error\n");
			goto err;
		}

		if (thermal_level > manager->qc2_thermal_levels) {
			hq_err("thermal level is invalid\n");
			goto err;
		}

		vote(manager->total_fcc_votable, voter_name, true,
			manager->qc2_thermal_mitigation[thermal_level]);
	}

	rc = get_client_vote_locked(manager->total_fcc_votable, voter_name);
	hq_info("%s: thermal vote susessful val = %d, current = %d\n", voter_name, thermal_level, rc);

	return;
err:
	vote(manager->total_fcc_votable, voter_name, false, 0);
	return;
}
EXPORT_SYMBOL(hq_set_prop_system_temp_level);

static int charge_manager_thermal_init(struct charger_manager *manager)
{
	int byte_len, rc, ret = 0;
	struct device_node *node = manager->dev->of_node;

	manager->thermal_enable = of_property_read_bool(node, "hq,thermal-enable");

	if (of_find_property(node, "hq,pd-thermal-mitigation", &byte_len)) {
		manager->pd_thermal_mitigation = devm_kzalloc(manager->dev, byte_len, GFP_KERNEL);
		if (IS_ERR_OR_NULL(manager->pd_thermal_mitigation)) {
			ret |= PD_THERM_PARSE_ERROR;
			hq_err("pd_thermal_mitigation kzalloc error\n");
		} else {
			manager->pd_thermal_levels = byte_len / sizeof(u32);
			rc = of_property_read_u32_array(node, "hq,pd-thermal-mitigation",
				manager->pd_thermal_mitigation, manager->pd_thermal_levels);
			if (rc < 0) {
				ret |= PD_THERM_PARSE_ERROR;
				hq_err("pd_thermal_mitigation parse error\n");
			}
		}
	} else {
		ret |= PD_THERM_PARSE_ERROR;
		hq_err("pd_thermal_mitigation not found\n");
	}

	if (of_find_property(node, "hq,qc2-thermal-mitigation", &byte_len)) {
		manager->qc2_thermal_mitigation = devm_kzalloc(manager->dev, byte_len, GFP_KERNEL);
		if (IS_ERR_OR_NULL(manager->qc2_thermal_mitigation)) {
			ret |= QC2_THERM_PARSE_ERROR;
			hq_err("qc2_thermal_mitigation kzalloc error\n");
		} else {
			manager->qc2_thermal_levels = byte_len / sizeof(u32);
			rc = of_property_read_u32_array(node, "hq,qc2-thermal-mitigation",
				manager->qc2_thermal_mitigation, manager->qc2_thermal_levels);
			if (rc < 0) {
				ret |= QC2_THERM_PARSE_ERROR;
				hq_err("qc2_thermal_mitigation parse error\n");
			}
		}
	} else {
		ret |= QC2_THERM_PARSE_ERROR;
		hq_err("qc2_thermal_mitigation not found\n");
	}
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
        if (of_find_property(node, "hq,pd-thermal-mitigation-fast", &byte_len)) {
		manager->pd_thermal_mitigation_fast = devm_kzalloc(manager->dev, byte_len, GFP_KERNEL);
		if (IS_ERR_OR_NULL(manager->pd_thermal_mitigation_fast)) {
			ret |= PD_THERM_PARSE_ERROR;
			hq_err("pd_thermal_mitigation_fast kzalloc error\n");
		} else {
			manager->pd_thermal_levels = byte_len / sizeof(u32);
			rc = of_property_read_u32_array(node, "hq,pd-thermal-mitigation-fast",
				manager->pd_thermal_mitigation_fast, manager->pd_thermal_levels);
			if (rc < 0) {
				ret |= PD_THERM_PARSE_ERROR;
				hq_err("pd_thermal_mitigation_fast parse error\n");
			}
		}
	} else {
		ret |= PD_THERM_PARSE_ERROR;
		hq_err("pd_thermal_mitigation_fast not found\n");
	}
#endif
	manager->thermal_parse_flags = ret;
	if (ret == (QC2_THERM_PARSE_ERROR | PD_THERM_PARSE_ERROR)) {
		manager->thermal_enable = false;
		ret = -EINVAL;
	}

	return ret;
}

static int main_chg_fcc_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

	if (value < 0) {
		hq_err("the value of main fcc is error.\n");
		return value;
	}

	ret = charger_set_ichg(manager->charger, value);
	if (ret < 0) {
		hq_err("charger set ichg fail.\n");
	}
	return ret;
}

static int main_chg_fv_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;
	ret = charger_set_term_volt(manager->charger, value);
	if (ret < 0) {
		hq_err("charger set term volt fail.\n");
	}

	return ret;
}

static int main_chg_icl_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

	if (value < 0) {
		hq_err("the value of main chg icl is error.\n");
		return value;
	}

	ret = charger_set_input_curr_lmt(manager->charger, value);
	if (ret < 0) {
		hq_err("charger set icl fail.\n");
	}
	return ret;
}

static int main_chg_iterm_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

	ret = charger_set_term_curr(manager->charger, value);
	if (ret < 0) {
		hq_err("charger set iterm fail.\n");
	}
	return ret;
}

static int total_fcc_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;

	if (value >= FASTCHARGE_MIN_CURR && (g_policy->state == POLICY_RUNNING)) {
		if (IS_ERR_OR_NULL(manager->main_icl_votable)) {
			hq_err("main_icl_votable not found\n");
			return PTR_ERR(manager->main_icl_votable);
		} else
			vote(manager->main_icl_votable, MAIN_FCC_MAX_VOTER, true, CP_EN_MAIN_CHG_CURR);
		if (IS_ERR_OR_NULL(manager->main_fcc_votable)) {
			hq_err("main_fcc_votable not found\n");
			return PTR_ERR(manager->main_fcc_votable);
		} else
			vote(manager->main_fcc_votable, MAIN_FCC_MAX_VOTER, true, CP_EN_MAIN_CHG_CURR);
	} else {
		if (IS_ERR_OR_NULL(manager->main_icl_votable)) {
			hq_err("-->main_icl_votable2 not found\n");
			return PTR_ERR(manager->main_icl_votable);
		} else
			vote(manager->main_icl_votable, MAIN_FCC_MAX_VOTER, false, 0);
		if (IS_ERR_OR_NULL(manager->main_fcc_votable)) {
			hq_err("-->main_fcc_votable not found\n");
			return PTR_ERR(manager->main_fcc_votable);
		} else {
			if (value >= 0)
				vote(manager->main_fcc_votable, MAIN_FCC_MAX_VOTER, true, value);
		}
	}

	return 0;
}

static int main_chg_disable_vote_callback(struct votable *votable, void *data, int enable, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

	ret = charger_disable_power_path(manager->charger, enable);
	if (ret < 0) {
		hq_err("charger disable_power_path fail.\n");
	}
	return ret;
}

static int cp_disable_vote_callback(struct votable *votable, void *data, int enable, const char *client)
{
	struct charger_manager *manager = data;
	struct chargerpump_dev *master_cp_chg = manager->master_cp_chg;
	struct chargerpump_dev *slave_cp_chg = manager->slave_cp_chg;
	int ret = 0;
	if(manager->cp_master_psy){
		ret = chargerpump_set_enable(master_cp_chg, enable);
		if (ret < 0) {
			hq_err("master_cp_chg set chg fail.\n");
		}
	}
	if(manager->cp_slave_psy){
		ret = chargerpump_set_enable(slave_cp_chg, enable);
		if (ret < 0) {
			hq_err("slave_cp_chg set chg fail.\n");
		}
	}
	return ret;
}

static int charger_manager_create_votable(struct charger_manager *manager)
{
	int ret = 0;

	if (manager->charger) {
		manager->main_fcc_votable = create_votable("MAIN_FCC", VOTE_MIN, main_chg_fcc_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->main_fcc_votable)) {
			hq_err("fail create MAIN_FCC voter.\n");
			return PTR_ERR(manager->main_fcc_votable);
		}

		manager->fv_votable = create_votable("MAIN_FV", VOTE_MIN, main_chg_fv_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->fv_votable)) {
			hq_err("fail create MAIN_FV voter.\n");
			return PTR_ERR(manager->fv_votable);
		}

		manager->main_icl_votable = create_votable("MAIN_ICL", VOTE_MIN, main_chg_icl_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->main_icl_votable)) {
			hq_err("fail create MAIN_ICL voter.\n");
			return PTR_ERR(manager->main_icl_votable);
		}

		manager->iterm_votable = create_votable("MAIN_ITERM", VOTE_MIN, main_chg_iterm_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->iterm_votable)) {
			hq_err("fail create MAIN_ICL voter.\n");
			return PTR_ERR(manager->iterm_votable);
		}

		manager->main_chg_disable_votable = create_votable("MAIN_CHG_DISABLE", VOTE_SET_ANY, main_chg_disable_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->main_chg_disable_votable)) {
			hq_err("fail create MAIN_CHG_DISABLE voter.\n");
			return PTR_ERR(manager->main_chg_disable_votable);
		}

		manager->total_fcc_votable = create_votable("TOTAL_FCC", VOTE_MIN, total_fcc_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->total_fcc_votable)) {
			hq_err("fail create TOTAL_FCC voter.\n");
			return PTR_ERR(manager->total_fcc_votable);
		}
	}

	if (manager->cp_master_psy || manager->cp_slave_psy) {
		manager->cp_disable_votable = create_votable("CP_DISABLE", VOTE_SET_ANY, cp_disable_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->cp_disable_votable)) {
			hq_err("fail create CP_DISABLE voter.\n");
			return PTR_ERR(manager->cp_disable_votable);
		}
	}
	return ret;
}

#if IS_ENABLED(CONFIG_TCPC_CLASS)

static void set_cc_drp_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
				struct charger_manager, set_cc_drp_work.work);
	if(manager != NULL && manager->tcpc != NULL)
		tcpm_typec_change_role(manager->tcpc, TYPEC_ROLE_DRP);
	hq_info("set_cc_drp_work_func enter\n");
}

static int charger_manager_tcpc_notifier_call(struct notifier_block *nb,
					unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct charger_manager *manager =
		container_of(nb, struct charger_manager, pd_nb);

	uint8_t old_state = TYPEC_UNATTACHED, new_state = TYPEC_UNATTACHED;
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;

	hq_info("noti event: %d %d\n", (int)event, (int)noti->pd_state.connected);
	switch (event) {
	case TCP_NOTIFY_SINK_VBUS:
		if (noti->vbus_state.type & TCP_VBUS_CTRL_PD_DETECT) {
			manager->pd_curr_max = noti->vbus_state.ma;
			manager->pd_volt_max = noti->vbus_state.mv;
			if (IS_ERR_OR_NULL(manager->main_icl_votable))
				hq_err("main_icl_votable not found\n");
			else
				vote(manager->main_icl_votable, TYPEC_SINK_VBUS_VOTER, true, manager->pd_curr_max);
			hq_info("TCP_NOTIFY_SINK_VBUS pd_curr_max = %d\n", manager->pd_curr_max);
			if (manager->pd_curr_max == 0) {
				chargerpump_set_enable(manager->master_cp_chg, false);
				hq_info("PD pd_curr_max = 0 disable cp !!!\n");
			}
		}
		break;
	case TCP_NOTIFY_SOURCE_VBUS:
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.new_state == TYPEC_UNATTACHED)
			manager->pd_active = CHARGE_PD_INVALID;

		old_state = noti->typec_state.old_state;
		new_state = noti->typec_state.new_state;

		if (IS_ERR_OR_NULL(manager) || IS_ERR_OR_NULL(manager->tcpc)) {
			hq_err("manager or tcpc is nullptr\n");
			break;
		}

		if (old_state == TYPEC_UNATTACHED &&
				new_state != TYPEC_UNATTACHED &&
				!manager->typec_attach) {
			hq_info("typec plug in, polarity = %d\n", noti->typec_state.polarity);
			manager->typec_attach = true;
			manager->cid_status = true;
			if(manager->ui_cc_toggle){
				ret = alarm_try_to_cancel(&manager->rust_det_work_timer);
				if (ret < 0) {
					hq_err("callback was running, skip timer\n");
				}
				hq_info("OTG ON:typec plug in, cancel hrtimer\n");
			}

		} else if (old_state != TYPEC_UNATTACHED &&
						new_state == TYPEC_UNATTACHED &&
						manager->typec_attach) {
			hq_info("typec plug out\n");
			manager->typec_attach = false;
			manager->cid_status = false;
			if(manager->ui_cc_toggle) {
				hq_err("OTG ON:typec plug out, ui set cc toggle\n");
				schedule_delayed_work(&manager->set_cc_drp_work, msecs_to_jiffies(500));

				ret = alarm_try_to_cancel(&manager->rust_det_work_timer);
				if (ret < 0) {
					hq_err("callback was running, skip timer\n");
				}
				ktime_now = ktime_get_boottime();
				time_now = ktime_to_timespec64(ktime_now);
				end_time.tv_sec = time_now.tv_sec + 600;
				end_time.tv_nsec = time_now.tv_nsec + 0;
				ktime = ktime_set(end_time.tv_sec,end_time.tv_nsec);

				hq_info("OTG ON:alarm timer start:%d, %lld %ld\n", ret,
						end_time.tv_sec, end_time.tv_nsec);
				alarm_start(&manager->rust_det_work_timer, ktime);

			}
		}
		break;
	case TCP_NOTIFY_PR_SWAP:
		manager->is_pr_swap = true;
		if (noti->swap_state.new_role == PD_ROLE_SINK)
			manager->pd_active = 10;
		break;
	case TCP_NOTIFY_DR_SWAP:
		manager->is_dr_swap = true;
		break;
	case TCP_NOTIFY_PD_STATE:
		switch (noti->pd_state.connected) {
		case PD_CONNECT_NONE:
			manager->pd_curr_max = 0;
			manager->pd_active = CHARGE_PD_INVALID;
			manager->is_pr_swap = false;
			manager->pd_contract_update = false;
			manager->is_dr_swap = false;
			break;
		case PD_CONNECT_PE_READY_SNK_APDO:
			manager->pd_contract_update = true;
			manager->pd_active = noti->pd_state.connected = CHARGE_PD_PPS_ACTIVE;
			hq_set_prop_system_temp_level(manager, TEMP_THERMAL_DAEMON_VOTER);
		#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
			xm_uevent_report(manager);
		#endif
			break;
		case PD_CONNECT_PE_READY_SNK:
		case PD_CONNECT_PE_READY_SNK_PD30:
			manager->pd_active = noti->pd_state.connected = CHARGE_PD_ACTIVE;
			manager->is_dr_swap = false;
			break;
		default:
			break;
		}
		charger_manager_wake_thread(manager);
		break;
	case TCP_NOTIFY_HARD_RESET_STATE:
		switch (noti->pd_state.connected) {
		case PD_CONNECT_NONE:
			manager->pd_curr_max = 0;
			manager->pd_active = CHARGE_PD_INVALID;
			manager->is_pr_swap = false;
			manager->pd_contract_update = false;
			manager->is_dr_swap = false;
			break;
		}
		break;
	default:
		break;
	}
	if ( !IS_ERR_OR_NULL(manager->charger) ) {
		manager->charger-> m_pd_active = manager->pd_active;
	}

	return NOTIFY_OK;
}
#endif

#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
static int charger_monitor_fg_i2c_status(struct charger_manager *manager) {
	int ret = 0;
	int vbus_volt = 0;

	ret = fuel_gauge_check_i2c_function(manager->fuel_gauge);
	if (!manager->vbus_type)
		return ret;
	if (ret) {
		charger_get_adc(manager->charger, CHG_ADC_VBUS, &vbus_volt);
		if (vbus_volt > FG_I2C_ERR_VBUS) {
			vote(manager->main_fcc_votable, FG_I2C_ERR, true, 300);
			vote(manager->main_icl_votable, FG_I2C_ERR, true, 300);
		} else {
			vote(manager->main_fcc_votable, FG_I2C_ERR, true, 500);
			vote(manager->main_icl_votable, FG_I2C_ERR, true, 500);
		}
	} else {
		vote(manager->main_fcc_votable, FG_I2C_ERR, false, 0);
		vote(manager->main_icl_votable, FG_I2C_ERR, false, 0);
	}
	return ret;
}
#endif

static void charger_manager_monitor(struct charger_manager *manager)
{
	union power_supply_propval pval = {0,};
	int ret = 0;
	uint32_t adc_buf_len = 0;
	uint8_t i = 0;
	char adc_buf[MIAN_CHG_ADC_LENGTH + 1] = {0};
	uint32_t iterm = 0;
	uint32_t fv = 0;
	int ichg = 0;
	bool charge_en = 0;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0)
		hq_err("get battery soc error.\n");
	else
		manager->soc = pval.intval;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret < 0)
		hq_err("get battery volt error.\n");
	else
		manager->vbat = pval.intval / 1000;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret < 0)
		hq_err("get battery current error.\n");
	else
		manager->ibat = pval.intval;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0)
		pr_err("get battery temperature error.\n");
	else
		manager->tbat = pval.intval;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
	if (ret < 0)
		hq_err("get charge status error.\n");
	else
		manager->chg_status = pval.intval;

	charger_get_term_curr(manager->charger, &iterm);

	charger_get_term_volt(manager->charger, &fv);

	charge_en = charger_get_chg(manager->charger);

	charger_get_ichg(manager->charger, &ichg);

	charger_get_input_curr_lmt(manager->charger, &manager->ibus);

	manager->rsoc = fuel_gauge_get_rsoc(manager->fuel_gauge);

	hq_info("[Battery] soc: %d, ibat: %d, vbat: %d, tbat: %d\n",
				manager->soc, manager->ibat, manager->vbat, manager->tbat);
	hq_info("[CHG_REG] ibus: %d, ichg: %d, charge_en: %d, iterm: %d, fv: %d\n",
				manager->ibus, ichg, charge_en, iterm, fv);

	power_supply_changed(manager->usb_psy);
	power_supply_changed(manager->batt_psy);
	power_supply_changed(manager->chg_psy);
#if IS_ENABLED(CONFIG_MIEV)
	if (manager->cp_master_psy != NULL)
	{
    	power_supply_changed(manager->cp_master_psy);
	} else {
		pr_err("cp_master_psy null error.\n");
	}
#endif


	for (i = 0; i < CHG_ADC_MAX; i++) {
		ret = charger_get_adc(manager->charger, i, &manager->chg_adc[i]);
		if (ret < 0) {
			hq_info("get adc failed\n");
			continue;
		}
		adc_buf_len += sprintf(adc_buf + adc_buf_len,
						"%s: %d, ", adc_name[i], manager->chg_adc[i]);
	}

	if (adc_buf_len > MIAN_CHG_ADC_LENGTH)
		adc_buf[MIAN_CHG_ADC_LENGTH] = '\0';
	hq_info("[CHG_ADC] %s\n", adc_buf);
}

static void low_vbat_power_off(struct charger_manager *manager)
{
	int rc = 0;
	static int count = 0;
	struct timespec64 ts;

	ktime_get_boottime_ts64(&ts);

	if (!manager->charger) {
		hq_err("failed to master_charge device\n");
		return;
	}

	if ((manager->vbat < ((manager->tbat >= BATTERY_COLD_TEMP ? SHUTDOWN_DELAY_VOL_LOW : SHUTDOWN_DELAY_VOL_COLD_TEMP) - 50))
		&& manager->vbat > 2700 && (u64)ts.tv_sec > 10 && manager->soc <= 1) {
		if (count < 3) {
			count ++;
			hq_info("count is =%d\n", count);
		} else {
			rc = charger_reset(manager->charger);
			if (rc < 0)
				hq_err("main chg reset failed.\n");
			msleep(1000);
			hq_info("vbat under 3.25V, poweroff. vbat=%d\n", manager->vbat);
			kernel_power_off();
		}
	} else {
		count = 0;
	}
}

static void power_off_check_work(struct charger_manager *manager)
{
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	int rc = 0;
#endif

	static char uevent_string[][MAX_UEVENT_LENGTH + 1] = {
		"POWER_SUPPLY_SHUTDOWN_DELAY=\n", //28
	};

	static char *envp[] = {
		uevent_string[0],
		NULL,
	};
	low_vbat_power_off(manager);
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	rc = fuel_gauge_check_i2c_function(manager->fuel_gauge);
	if (manager->soc == 1 || (rc && manager->vbat)) {
#else
	if (manager->soc == 1) {
#endif
		if ((manager->vbat >= SHUTDOWN_DELAY_VOL_LOW && manager->vbat < SHUTDOWN_DELAY_VOL_HIGH)
			&& manager->chg_status != POWER_SUPPLY_STATUS_CHARGING){
				manager->shutdown_delay = true;
		} else if (manager->chg_status == POWER_SUPPLY_STATUS_CHARGING
						&& manager->shutdown_delay) {
				manager->shutdown_delay = false;
		}
	} else {
		manager->shutdown_delay = false;
	}

	if (manager->last_shutdown_delay != manager->shutdown_delay) {
		manager->last_shutdown_delay = manager->shutdown_delay;
		power_supply_changed(manager->usb_psy);
		power_supply_changed(manager->batt_psy);
		if (manager->shutdown_delay == true) {
			strncpy(uevent_string[0] + 28, "1", MAX_UEVENT_LENGTH - 28);
			fuel_gauge_set_rsoc_update0(manager->fuel_gauge, true);
		} else {
			strncpy(uevent_string[0] + 28, "0", MAX_UEVENT_LENGTH - 28);
			fuel_gauge_set_rsoc_update0(manager->fuel_gauge, false);
		}
		mdelay(1000);
		hq_err("envp[0] = %s\n", envp[0]);
		kobject_uevent_env(&manager->dev->kobj, KOBJ_CHANGE, envp);
	}
}

static int charger_manager_check_vindpm(struct charger_manager *manager, uint32_t vbat)
{
	struct charger_dev *charger = manager->charger;
	int ret = 0;
#if CHARGER_VINDPM_USE_DYNAMIC
	if (vbat < CHARGER_VINDPM_DYNAMIC_BY_VBAT1) {
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE1);
	} else if (vbat < CHARGER_VINDPM_DYNAMIC_BY_VBAT2) {
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE2);
	} else if (vbat < CHARGER_VINDPM_DYNAMIC_BY_VBAT3) {
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE3);
	} else if (vbat < CHARGER_VINDPM_DYNAMIC_BY_VBAT4) {
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE4);
	} else {
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE5);
	}
#else
	ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE3);
#endif

	if (ret < 0){
		hq_err("Failed to set vindpm, ret = %d\n", ret);
		return ret;
	}
	return 0;
}

static int charger_manager_check_iindpm(struct charger_manager *manager, uint32_t vbus_type)
{
	int ret = 0;
	int ichg_ma = 0;
	int icl_ma = 0;

	switch (vbus_type) {
	case VBUS_TYPE_FLOAT:
		ichg_ma = manager->float_current;
		icl_ma = manager->float_current;
		break;
	case VBUS_TYPE_NONE:
		ichg_ma = 100;
		icl_ma = 100;
		break;
	case VBUS_TYPE_SDP:
		ichg_ma = manager->usb_current;
		icl_ma = manager->usb_current;
		break;
	case VBUS_TYPE_NON_STAND:
		ichg_ma = manager->float_current;
		icl_ma = manager->float_current;
		break;
	case VBUS_TYPE_CDP:
		ichg_ma = manager->cdp_current;
		icl_ma = manager->cdp_current;
		break;
	case VBUS_TYPE_DCP:
		ichg_ma = manager->dcp_current;
		icl_ma = manager->dcp_current;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
		if (manager->smart_charge[SMART_CHG_OUTDOOR_CHARGE].active_status || manager->pd_active) {
			ichg_ma = manager->xm_outdoor_current;
			icl_ma = manager->xm_outdoor_current;
			hq_info("xm_outdoor_current or cp_charge_done ichg_ma = %d!\n",ichg_ma = manager->xm_outdoor_current);
		}
#endif
		break;
	case VBUS_TYPE_HVDCP:
		ichg_ma = manager->hvdcp_charge_current;
		icl_ma = manager->hvdcp_input_current;
		break;
	case VBUS_TYPE_HVDCP_3:
	case VBUS_TYPE_HVDCP_3P5:
		ichg_ma = manager->hvdcp3_charge_current;
		icl_ma = manager->hvdcp3_input_current;
		break;
	default:
		ichg_ma = manager->usb_current;
		icl_ma = manager->usb_current;
		break;
	}

	if ((manager->pd_active == CHARGE_PD_ACTIVE && vbus_type) || manager->pd_active == 10) {
		if (manager->pd_volt_max == 5000) {  //C-to-C
			ichg_ma = manager->pd_curr_max;
			icl_ma = manager->pd_curr_max;
			hq_err("c-to-c ichg_ma = %d\n", ichg_ma);
		} else {  //PD2.0
			ichg_ma = manager->pd_curr_max * PD20_ICHG_MULTIPLE / 1000;  //1.8 of fixed current
			manager->pd_curr_max = min(manager->pd_curr_max, 2000);
			icl_ma = manager->pd_curr_max;
			hq_err("fixed current ichg_ma = %d\n", ichg_ma);
		}
	}

	if (is_mtbf_mode_func() && (vbus_type == VBUS_TYPE_SDP || vbus_type == VBUS_TYPE_CDP)) {
		ichg_ma = MTBF_CURRENT;
		icl_ma = MTBF_CURRENT;
		hq_info("is_mtbf_mode=%d icl=%d ichg=%d\n", is_mtbf_mode_func(), icl_ma, ichg_ma);
	}

	if (IS_ERR_OR_NULL(manager->main_icl_votable)) {
		hq_err("main_icl_votable not found\n");
		return PTR_ERR(manager->main_icl_votable);
	} else
		vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, true, icl_ma);

	if (IS_ERR_OR_NULL(manager->main_fcc_votable)) {
		hq_err("main_fcc_votable not found\n");
		return PTR_ERR(manager->main_fcc_votable);
	} else
		vote(manager->main_fcc_votable, CHARGER_TYPE_VOTER, true, ichg_ma);

	return ret;
}

static void charger_manager_check_inputsuspend(struct charger_manager *manager, int soc)
{
	int soc_low_thres = MBTF_SOC_LOW_THRESHOLD;
	int soc_high_thres = MBTF_SOC_HIGH_THRESHOLD;
	int mode = is_mtbf_mode_func();
	int in_input_suspend = get_effective_result(manager->main_chg_disable_votable);
	if (mode == DEFAULT_MODE) {
		hq_info("inputsuspend mode is default mode!\n");
		return;
	}
	if (mode == AGING_MODE) {
		soc_low_thres = AGING_SOC_LOW_THRESHOLD;
		soc_high_thres = AGING_SOC_HIGH_THRESHOLD;
	}

	if(soc >= soc_high_thres) {
		if (in_input_suspend != true) {
			vote(manager->main_chg_disable_votable, MAN_MADE_SUSPNED_VOTER, true, 1);
			hq_info("In MTBF MODE the soc > 75, need disable charge\n");
		}
	} else if(soc <= soc_low_thres) {
		if (in_input_suspend == true) {
			vote(manager->main_chg_disable_votable, MAN_MADE_SUSPNED_VOTER, false, 0);
			hq_info("In MTBF MODE the soc < 60, need enable charge\n");
		} else if (manager->ibus < 100) {
			charger_mtbf_regs_show(manager->charger);
			mdelay(100);
			vote(manager->main_chg_disable_votable, MAN_MADE_SUSPNED_VOTER, true, 1);
			mdelay(100);
			vote(manager->main_chg_disable_votable, MAN_MADE_SUSPNED_VOTER, false, 0);
			hq_info("In MTBF MODE the soc < 60 && input suspend = 0, but ibus < 100mA, need enable charge again\n");
		}
	}

	return;
}

static void charger_manager_timer_func(struct timer_list *timer)
{
	struct charger_manager *manager = container_of(timer,
							struct charger_manager, charger_timer);
	charger_manager_wake_thread(manager);
}

int charger_manager_start_timer(struct charger_manager *manager, uint32_t ms)
{
	del_timer(&manager->charger_timer);
	manager->charger_timer.expires = jiffies + msecs_to_jiffies(ms);
	manager->charger_timer.function = charger_manager_timer_func;
	add_timer(&manager->charger_timer);
	return 0;
}
EXPORT_SYMBOL(charger_manager_start_timer);

static int reset_vote(struct charger_manager *manager)
{
	vote(manager->main_fcc_votable, CHARGER_TYPE_VOTER, false, 0);
	vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, false, 0);
	vote(manager->total_fcc_votable, JEITA_VOTER, false, 0);
	vote(manager->total_fcc_votable, CHG_CYCLE_VOTER, false, 0);
	vote(manager->fv_votable, CHG_CYCLE_VOTER, false, 0);
	vote(manager->main_icl_votable, TYPEC_SINK_VBUS_VOTER, false, 0);
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	vote(manager->main_fcc_votable, FG_I2C_ERR, false, 0);
	vote(manager->main_icl_votable, FG_I2C_ERR, false, 0);
#endif
#if IS_ENABLED(CONFIG_HUAQIN_SOFT_ITERM_SUPPORT)
	vote(manager->total_fcc_votable, SOFT_ITER_VOTER, false, 0);
	vote(manager->iterm_votable, HARD_ITERM_VOTER, false, 0);
#endif
	return 0;
}

static int rerun_vote(struct charger_manager *manager)
{
	rerun_election(manager->main_chg_disable_votable);
	rerun_election(manager->total_fcc_votable);
	rerun_election(manager->main_fcc_votable);
	rerun_election(manager->main_icl_votable);
	return 0;
}

static void apsd_second_detect_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
					struct charger_manager, second_detect_work.work);

	hq_info("apsd enter!\n");
	if (float_count <= 3) {
		hq_info("start count!\n");
		if ( float_count > 0 && manager->vbus_type != VBUS_TYPE_FLOAT) {
			hq_info("apsd alreay OK !\n");
			float_count = 0;
			return;
		}
		charger_force_dpdm(manager->charger);
		schedule_delayed_work(&manager->second_detect_work, msecs_to_jiffies(FLOAT_DELAY_TIME));
		float_count++;
	}
}

#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
static bool get_usb_ready(struct charger_manager *manager)
{
	bool ready = true;

	if (IS_ERR_OR_NULL(manager->usb_node))
		manager->usb_node = of_parse_phandle(manager->dev->of_node, "usb", 0);
	if (!IS_ERR_OR_NULL(manager->usb_node)) {
		ready = !of_property_read_bool(manager->usb_node, "cdp-block");
		if (ready || manager->get_usb_rdy_cnt % 10 == 0)
			hq_info("usb ready = %d\n", ready);
	} else
		hq_err("usb node missing or invalid\n");

	if (ready == false && (manager->get_usb_rdy_cnt >= WAIT_USB_RDY_MAX_CNT || manager->pd_active)) {
		if (manager->pd_active)
			manager->get_usb_rdy_cnt = 0;
		hq_info("cdp-block timeout or pd adapter\n");
		return true;
	}

	return ready;
}

static void wait_usb_ready_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
					struct charger_manager, wait_usb_ready_work.work);

	if (get_usb_ready(manager) || manager->get_usb_rdy_cnt >= WAIT_USB_RDY_MAX_CNT)
		charger_force_dpdm(manager->charger);
	else {
		manager->get_usb_rdy_cnt++;
		schedule_delayed_work(&manager->wait_usb_ready_work, msecs_to_jiffies(WAIT_USB_RDY_TIME));
	}
}
#endif

#if IS_ENABLED(CONFIG_HUAQIN_SOH2_SUPPORT)
static void batt_soh20_aging_test(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
			struct charger_manager, batt_soh20_aging_test.work);

	union power_supply_propval pval = {0,};
	int ret = 0;
	static int rsoc_count;
	int fake_cycle_count = 0;
	int fake_raw_soh = 100;

	hq_err("__enter__\n");

	if (manager->batt_psy == NULL) {
		manager->batt_psy = power_supply_get_by_name("battery");
		if (manager->batt_psy == NULL) {
			hq_err("manager->batt_psy is NULL\n");
			goto out;
		}
	}
	if (manager->fuel_gauge == NULL) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (manager->fuel_gauge == NULL) {
			hq_err("manager->fuel_gauge is NULL\n");
			goto out;
		}
	}

	ret = power_supply_get_property(manager->batt_psy,
		POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	if (ret < 0) {
		hq_err("failed to get cycle_count prop\n");
		goto out;
	} else
		fake_cycle_count = pval.intval;

	fake_raw_soh -= (fake_cycle_count/100);
	fuel_gauge_set_fake_soh(manager->fuel_gauge, fake_raw_soh);

	ret = power_supply_get_property(manager->batt_psy,
		POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret < 0) {
		hq_err("failed to get cycle_count prop\n");
		goto out;
	} else {
		if (pval.intval < 0)
			rsoc_count++;
	}

	if (rsoc_count > 13) {
		rsoc_count = 0;
		pval.intval = fake_cycle_count + 1;
		ret = power_supply_set_property(manager->batt_psy,
			POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
		if (ret < 0) {
			hq_err("failed to get cycle_count prop\n");
			goto out;
		}
	}

out:
	schedule_delayed_work(&manager->batt_soh20_aging_test, msecs_to_jiffies(3000));
}
#endif

static void update_fg_raw_soh_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
				struct charger_manager, update_fg_raw_soh_work.work);

	int delta_raw_soh = 0;
	int battery_raw_soh = 100;
	int set_raw_soh = 0;
	int tmp_raw_soh = 0;
	int new_fg_raw_soh = 100;
	int ret = 0;

	mutex_lock(&manager->update_auth_param_lock);

	/* WORKAROUND: to fix DDR Stress Test stick ic schedule in atomic context */
	if (unlikely(SHUTDOWN_DELAY_VOL_LOW == 3100) &&
		unlikely(SHUTDOWN_DELAY_VOL_HIGH == 3200)) {
		mutex_unlock(&manager->update_auth_param_lock);
		return;
	}

	/* step0: get new_fg_raw_soh from MTK FG */
	new_fg_raw_soh = manager->new_fg_raw_soh;
	/* step1: MTK fg deamon is ready once reach here, do some deffer initializations */
	if (manager->fg_raw_soh < 0) {
		/* initialize fg_raw_soh */
		manager->fg_raw_soh = new_fg_raw_soh;
		battery_raw_soh = auth_device_get_raw_soh(manager->auth_dev);
		if (battery_raw_soh < 0) {
			hq_err("[EEA] read auth raw_soh error\n");
			manager->batt_raw_soh = new_fg_raw_soh;
		} else {
			manager->batt_raw_soh = battery_raw_soh;
			hq_info("[EEA] read auth raw_soh success\n");
		}

		hq_info("success to init battery raw_soh, batt_raw_soh = %d, fg_raw_soh = %d\n",
				manager->batt_raw_soh, manager->fg_raw_soh);
	}

	/* step2:
	 * delta_raw_soh: get delta_raw_soh from MTK FG
	 * new_fg_raw_soh: get new_fg_raw_soh from MTK FG
	 * manager->fg_raw_soh: get old_fg_raw_soh from MTK FG
	 */
	delta_raw_soh = new_fg_raw_soh - manager->fg_raw_soh;
	hq_info("new_fg_raw_soh = %d, old_fg_raw_soh = %d, batt_raw_soh = %d, delta_raw_soh = %d\n",
			new_fg_raw_soh, manager->fg_raw_soh, manager->batt_raw_soh, delta_raw_soh);

	/* step3:
	 * Battery raw_soh update policy
	 * 1. EEA: save latest fg raw_soh to battery secret ic and update cm battery raw_soh
	 * 2. Non-EEA: keep battery raw_soh follow lastest fg raw_soh
	 */

	if (!manager->auth_dev) {
		hq_err("[EEA] failed to get battery auth device\n");
		goto rerun_work;
	}

	battery_raw_soh = auth_device_get_raw_soh(manager->auth_dev);
	if (battery_raw_soh < 0) {
		hq_err("[EEA] first read auth raw_soh error\n");
		goto rerun_work;
	}
	mdelay(20);

	set_raw_soh = battery_raw_soh + delta_raw_soh;
	ret = auth_device_set_raw_soh(manager->auth_dev, set_raw_soh);
	if (ret != 0) {
		hq_err("[EEA] write auth raw_soh error\n");
		goto rerun_work;
	}
	mdelay(20);

	tmp_raw_soh = auth_device_get_raw_soh(manager->auth_dev);
	if (tmp_raw_soh < 0) {
		hq_err("[EEA] second read auth raw_soh error\n");
		goto rerun_work;
	}

	if (set_raw_soh != tmp_raw_soh) {
		hq_err("[EEA] update raw_soh fail, retry update raw soh\n");
		goto rerun_work;
	}

	manager->fg_raw_soh = new_fg_raw_soh;
	manager->batt_raw_soh = tmp_raw_soh;
	hq_info("[EEA] success to update raw_soh count, batt_raw_soh = %d, fg_raw_soh = %d\n",manager->batt_raw_soh, manager->fg_raw_soh);
	
	fuel_gauge_set_soh(manager->fuel_gauge, manager->batt_raw_soh);

	mutex_unlock(&manager->update_auth_param_lock);
	return;

rerun_work:
	mutex_unlock(&manager->update_auth_param_lock);
	schedule_delayed_work(&manager->update_fg_raw_soh_work, msecs_to_jiffies(30000));
}

static void update_fg_cycle_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
				struct charger_manager, update_fg_cycle_work.work);
	int delta_cycle = 0;
	u32 battery_cycle = 0;
	u32 set_cycle = 0;
	u32 tmp_cycle = 0;
	int new_fg_cycle = 0;
	int ret = 0;

	mutex_lock(&manager->update_auth_param_lock);

	/* WORKAROUND: to fix DDR Stress Test stick ic schedule in atomic context */
	if (unlikely(SHUTDOWN_DELAY_VOL_LOW == 3100) &&
		unlikely(SHUTDOWN_DELAY_VOL_HIGH == 3200)) {
		mutex_unlock(&manager->update_auth_param_lock);
		return;
	}

	/* step0: get new_fg_cycle from MTK FG */
	new_fg_cycle = manager->new_fg_cycle;
	/* step1: MTK fg deamon is ready once reach here, do some deffer initializations */
	if (manager->fg_cycle < 0) {
		/* initialize fg_cycle */
		manager->fg_cycle = new_fg_cycle;

		ret = auth_device_get_cycle_count(manager->auth_dev, &battery_cycle);
		if (ret) {
			hq_err("[EEA] read auth cycle count error\n");
			manager->batt_cycle = new_fg_cycle;
		} else {
			manager->batt_cycle = battery_cycle;
			hq_info("[EEA] read auth cycle count success\n");
		}

		hq_info("success to init battery cycle, batt_cycle = %d, fg_cycle = %d\n",manager->batt_cycle, manager->fg_cycle);
	}

	/* step2:
	 * delta_cycle: get delta_cycle from MTK FG
	 * new_fg_cycle: get new_fg_cycle from MTK FG
	 * manager->fg_cycle: get old_fg_cycle from MTK FG
	 */
	delta_cycle = new_fg_cycle - manager->fg_cycle;
	hq_info("new_fg_cycle = %d, old_fg_cycle = %d, batt_cycle = %d, delta_cycle = %d\n",
			new_fg_cycle, manager->fg_cycle, manager->batt_cycle, delta_cycle);

	/* step3:
	 * Battery cycle update policy
	 * 1. EEA: save latest fg cycle to battery secret ic and update cm battery cycle
	 * 2. Non-EEA: keep battery cycle follow lastest fg cycle
	 */
	if (!manager->auth_dev) {
		hq_err("[EEA] failed to get battery auth device\n");
		goto rerun_work;
	}

	ret = auth_device_get_cycle_count(manager->auth_dev, &battery_cycle);
	if (ret != 0) {
		hq_err("[EEA] first read auth cycle count error\n");
		goto rerun_work;
	}
	mdelay(20);

	set_cycle = battery_cycle + delta_cycle;
	ret = auth_device_set_cycle_count(manager->auth_dev, set_cycle, battery_cycle);
	if (ret != 0) {
		hq_err("[EEA] write auth cycle count error\n");
		goto rerun_work;
	}
	mdelay(20);

	ret = auth_device_get_cycle_count(manager->auth_dev, &tmp_cycle);
	if (ret != 0) {
		hq_err("[EEA] second read auth cycle count error\n");
		goto rerun_work;
	}

	if (set_cycle != tmp_cycle) {
		hq_err("[EEA] update cycle fail, retry update cycle\n");
		goto rerun_work;
	}

	manager->fg_cycle = new_fg_cycle;
	manager->batt_cycle = tmp_cycle;
	hq_info("[EEA] success to update cycle count, batt_cycle = %d, fg_cycle = %d\n",manager->batt_cycle, manager->fg_cycle);

	mutex_unlock(&manager->update_auth_param_lock);
	return;

rerun_work:
	mutex_unlock(&manager->update_auth_param_lock);
	schedule_delayed_work(&manager->update_fg_cycle_work, msecs_to_jiffies(30000));
}

static void charger_manager_charger_type_detect(struct charger_manager *manager)
{
	struct votable		*fcc_votable;
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;

        fcc_votable = find_votable("TOTAL_FCC");
        if (!fcc_votable) {
                pr_err("%s failed to get fcc_votable\n", __func__);
                return;
        }

	charger_get_online(manager->charger, &manager->usb_online);
	charger_get_vbus_type(manager->charger, &manager->vbus_type);

	if (manager->usb_online != manager->adapter_plug_in) {
		manager->adapter_plug_in = manager->usb_online;
		if (manager->adapter_plug_in) {
			pm_stay_awake(manager->dev);
			hq_info("adapter plug in\n");
			if (IS_ERR_OR_NULL(manager->main_icl_votable)) {
				hq_info("main_icl_votable is NULL\n");
			} else {
				vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, true, 100);
				hq_info("main_icl_votable icl 100\n");
			}
			if (manager->single_cell_det && !manager->input_suspend && manager->vbat <= 4300) {
				//before start timer,try to cancle
				ret = alarm_try_to_cancel(&manager->start_cell_det_work_timer);
				if (ret < 0) {
					hq_err("[Detcell] callback was running, skip timer\n");
				}
				ktime_now = ktime_get_boottime();
				time_now = ktime_to_timespec64(ktime_now);
				end_time.tv_sec = time_now.tv_sec + 60;
				end_time.tv_nsec = time_now.tv_nsec + 0;
				ktime = ktime_set(end_time.tv_sec,end_time.tv_nsec);
				alarm_start(&manager->start_cell_det_work_timer, ktime);
				hq_info("[Detcell] start start_cell_det_work_timer\n");
			}

#if IS_ENABLED(CONFIG_XIAOMI_USB_TOUCH_NOTIFIER)
			xiaomi_touch_usb_data.usb_touch_enable = XIAOMI_USB_ENABLE;
			xiaomi_usb_touch_notifier_call_chain(XIAOMI_TOUCH_USB_SWITCH,&xiaomi_touch_usb_data);
#endif
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
                        if(manager->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret && manager->soc >= manager->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val) {
                                hq_err("smart_chg: endurance is working, set active_status true.\n");
                                manager->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status = true;
                        }
                        if ((manager->soc <= 20) && (manager->thermal_board_temp <= 390)) {
                                manager->low_fast_plugin_flag = true;
                        }
			schedule_delayed_work(&manager->xm_charge_work, msecs_to_jiffies(3000));
#endif
#if IS_ENABLED(CONFIG_HUAQIN_SOFT_ITERM_SUPPORT)
			schedule_delayed_work(&manager->soft_iterm_work, msecs_to_jiffies(3000));
			manager->soft_term_check_cnt = 0;
#endif
			manager->qc_detected = false;
			charger_adc_enable(manager->charger, true);
			chargerpump_set_enable_adc(manager->master_cp_chg, true);
			chargerpump_set_enable_adc(manager->slave_cp_chg, true);
			charger_set_term(manager->charger, true);
			rerun_vote(manager);
			vote(manager->total_fcc_votable, JEITA_VOTER, true, 500);
#if IS_ENABLED(CONFIG_MIEV)
                        hq_chargermanager_notifier_call_chain(manager->adapter_plug_in, NULL);
#endif
		} else {
			chargerpump_set_enable_adc(manager->master_cp_chg, false);
			chargerpump_set_enable_adc(manager->slave_cp_chg, false);
			charger_adc_enable(manager->charger, false);
#if IS_ENABLED(CONFIG_BC12_RETRY_FOR_MI_PD)
			bc12_retry_flag = 1;
#endif
			hq_info("adapter plug out\n");
			if (IS_ERR_OR_NULL(manager->main_icl_votable)) {
				hq_info("main_icl_votable is NULL\n");
			} else {
				vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, true, 100);
				hq_info("main_icl_votable icl 100\n");
			}

			if(manager->single_cell_det) {
				ret = alarm_try_to_cancel(&manager->start_cell_det_work_timer);
				if (ret < 0) {
					hq_err("[Detcell] start_cell_det_work_timer callback was running, skip timer\n");
				}
				ret = alarm_try_to_cancel(&manager->cell_det_work_timer);
				if (ret < 0) {
					hq_err("[Detcell] cell_det_work_timer callback was running, skip timer\n");
				}
				hq_info(" [Detcell] alarm_try_to_cancel\n");
			}

#if IS_ENABLED(CONFIG_XIAOMI_USB_TOUCH_NOTIFIER)
			xiaomi_touch_usb_data.usb_touch_enable = XIAOMI_USB_DISABLE;
			xiaomi_usb_touch_notifier_call_chain(XIAOMI_TOUCH_USB_SWITCH,&xiaomi_touch_usb_data);
#endif
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
			vote(fcc_votable, ENDURANCE_VOTER, false, 0);
			rerun_election(fcc_votable);
			manager->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status = false;
			manager->endurance_ctrl_en = false;
			cancel_delayed_work(&manager->xm_charge_work);
			manager->low_fast_plugin_flag = false;
			manager->pps_fast_mode = false;
			manager->b_flag = B_NORMAL;
			manager->fv_overvoltage_flag = false;
			vote(manager->total_fcc_votable, FV_OVERVOLTAGE_VOTER,false, 0);
#endif
#if IS_ENABLED(CONFIG_HUAQIN_SOFT_ITERM_SUPPORT)
			manager->soft_term_status = POWER_SUPPLY_STATUS_DISCHARGING;
			cancel_delayed_work(&manager->soft_iterm_work);
			manager->soft_term_check_cnt = 0;
#endif
			manager->plug_in_soc100_flag = false;
			cancel_delayed_work_sync(&manager->second_detect_work);
			float_count = 0;
			#if IS_ENABLED(CONFIG_BQ_FUELGAUGE)
			fuel_gauge_set_fastcharge_mode(manager->fuel_gauge, false);
			#endif
			chargerpump_policy_stop(g_policy);
			reset_vote(manager);
#if IS_ENABLED(CONFIG_MIEV)
            hq_chargermanager_notifier_call_chain(manager->adapter_plug_in, NULL);
#endif
			pm_relax(manager->dev);
		}
	}

	hq_info("usb_online= %d, bc_type = %s, input_suspend = %d, pd_active = %d, single_cell = %d\n",
				manager->usb_online, bc12_result[manager->vbus_type], manager->input_suspend, manager->pd_active, manager->en_single_cell);
#if IS_ENABLED(CONFIG_BC12_RETRY_FOR_MI_PD)
	if (manager->pd_active == 2 && manager->vbus_type != VBUS_TYPE_NONE && manager->vbus_type != VBUS_TYPE_DCP && bc12_retry_flag) {
		charger_force_dpdm(manager->charger);
		bc12_retry_flag = 0;
		hq_info("Retry for bc_type!\n");
	}
#endif
	if (!manager->adapter_plug_in)
		return;

	if (!manager->is_pr_swap) {
		switch (manager->vbus_type) {
			case VBUS_TYPE_NONE:
				charger_force_dpdm(manager->charger);
				break;
			case VBUS_TYPE_NON_STAND:
			case VBUS_TYPE_FLOAT:
				if (float_count == 0 && manager->pd_active != 1) {
					hq_info("float type!\n");
					schedule_delayed_work(&manager->second_detect_work, msecs_to_jiffies(0));
				}
				rerun_election(manager->main_icl_votable);
				break;
			case VBUS_TYPE_SDP:
				#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
				if (!get_usb_ready(manager)) {
					if (manager->get_usb_rdy_cnt == 0)
						schedule_delayed_work(&manager->wait_usb_ready_work, msecs_to_jiffies(0));
				}
				#endif
				break;
			default:
				break;
		}
	} else
		manager->vbus_type = VBUS_TYPE_FLOAT;

	if (manager->vbus_type == VBUS_TYPE_SDP || manager->vbus_type == VBUS_TYPE_CDP)
		manager->usb_psy_desc.type = POWER_SUPPLY_TYPE_USB;
	else
		manager->usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_PD;

#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	if (charger_monitor_fg_i2c_status(manager)) {
		hq_info("fg i2c error\n");
}
#endif

	if (manager->pd_contract_update) {
		manager->pd_contract_update = false;
		if (g_policy->state != POLICY_RUNNING)
			chargerpump_policy_stop(g_policy);
	}

	if ((manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (g_policy->state == POLICY_NO_START))
		chargerpump_policy_start(g_policy);

	if (g_policy->state != POLICY_RUNNING) {
		if(manager->vbus_type == VBUS_TYPE_DCP && !manager->qc_detected && !manager->pd_active) {
			manager->qc_detected = true;
			charger_qc_identify(manager->charger, manager->qc3_mode);
		}
	}

	/*
	 * WORKAROUNG: fix samsung EP-TA200 adapter HVDCP issue,
	 * We shouldn't set icl/ichg before vbus rise to 9v,
	 * as it would trigered vindpm.
	 * TODO: add notifier chain between main_chg.ko and subpmic_xxx.ko to notify hvdcp done.
	 */
	msleep(100);

	charger_manager_check_vindpm(manager, manager->chg_adc[CHG_ADC_VBAT]);
	charger_manager_check_iindpm(manager, manager->vbus_type);

	charger_manager_check_inputsuspend(manager, manager->soc);
}

static int charger_manager_thread_fn(void *data)
{
	struct charger_manager *manager = data;
	int ret = 0;

	while (true) {
		ret = wait_event_interruptible(manager->wait_queue,
							manager->run_thread);
		if (kthread_should_stop() || ret) {
			hq_err("exits(%d)\n", ret);
			break;
		}

		manager->run_thread = false;

		charger_manager_monitor(manager);

		charger_manager_charger_type_detect(manager);

		power_off_check_work(manager);

		if (!manager->adapter_plug_in)
			charger_manager_start_timer(manager, CHARGER_MANAGER_LOOP_TIME_OUT);
		else
			charger_manager_start_timer(manager, CHARGER_MANAGER_LOOP_TIME);
	}
	return 0;
}

static int charger_manager_notifer_call(struct notifier_block *nb, unsigned long event, void *data)
{
	struct charger_manager *manager = container_of(nb,
							struct charger_manager, charger_nb);
	charger_manager_wake_thread(manager);

	return NOTIFY_OK;
}

static void shipmode_syscore_shutdown(void)
{
	int ret = 0;
	struct charger_dev *charger = charger_find_dev_by_name("primary_chg");

	if (charger != NULL && charger->shipmode_flag) {
		pr_info("shipmode syscore shutdown flag:%d\n", charger->shipmode_flag);
		charger->shipmode_flag = false;
		ret = charger_set_shipmode(charger, true);
		if (ret < 0)
			pr_info("set ship mode fail\n");
		else
			pr_info("set ship mode success\n");
	}
}

static struct syscore_ops shipmode_syscore_ops = {
	.shutdown = shipmode_syscore_shutdown,
};
static void charger_manager_check_dev(struct charger_manager *manager)
{

	if(IS_ERR_OR_NULL(manager)) {
		hq_err("manager is err or null\n");
		return;
	}

	manager->charger = charger_find_dev_by_name("primary_chg");
	if (!manager->charger)
		hq_err("failed to master_charge device\n");

	manager->master_cp_chg = chargerpump_find_dev_by_name("master_cp_chg");
	if (!manager->master_cp_chg)
		hq_err("failed to master_cp_chg device\n");

	manager->slave_cp_chg = chargerpump_find_dev_by_name("slave_cp_chg");
	if (!manager->slave_cp_chg)
		hq_err("failed to slave_cp_chg device\n");

	manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
	if (!manager->fuel_gauge)
		hq_err("failed to fuel_gauge device\n");

	manager->cp_master_psy = power_supply_get_by_name("sc-cp-master");
	if (!manager->cp_master_psy)
		hq_err("failed to cp_master_psy\n");

	manager->cp_slave_psy = power_supply_get_by_name("sc-cp-slave");
	if (!manager->cp_slave_psy)
		hq_err("failed to cp_slave_psy\n");

	manager->fg_psy = power_supply_get_by_name("bms");
	if (IS_ERR_OR_NULL(manager->fg_psy))
		hq_err("failed to get bms psy\n");

#if IS_ENABLED(CONFIG_TCPC_CLASS)
	manager->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (!manager->tcpc)
		hq_err("get tcpc dev failed\n");
#endif

#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
	manager->pd_adapter = get_adapter_by_name("pd_adapter");
	if (!manager->pd_adapter)
		hq_err("failed to pd_adapter\n");
#endif

#if IS_ENABLED(CONFIG_ONEWIRE_GPIO)
	manager->auth_dev = get_batt_auth_by_name("secret_ic");
	if (!manager->auth_dev) {
		hq_err("failed to get battery auth device\n");
	}
#endif	
}

#if IS_ENABLED(CONFIG_MIEV)
static int charger_manager_register_notifiers(struct charger_manager *manager)
{
	int ret = 0;
	ret = charger_manager_register_charger_changed_notifier(manager);
	if (ret < 0) {
		hq_err("couldn't register charger changed notifier, ret = %d\n", ret);
		// return ret;
	}
	ret = charger_manager_register_psy_notifier(manager);
	if (ret < 0) {
		hq_err("couldn't register psy notifier ret = %d\n", ret);
		// return ret;
	}
#if 0
	ret = charger_manager_register_thermal_notifier(manager);
	if (ret < 0) {
		hq_err("couldn't register thermal notifier, ret = %d\n", ret);
		// return ret;
	}
#endif
	ret = charger_manager_register_fg_notifier(manager);
	if (ret < 0) {
		hq_err("couldn't register fg notifier, ret = %d\n", ret);
		// return ret;
	}

	ret = charger_manager_register_charger_notifier(manager);
	if (ret < 0) {
		hq_err("couldn't register charger notifier, ret = %d\n", ret);
		// return ret;
	}

	return ret;
}


static int charger_manager_unregister_notifiers(struct charger_manager *manager)
{
	int ret = 0;
	ret = charger_manager_unregister_charger_changed_notifier(manager);
	if (ret < 0) {
		hq_err("couldn't unregister charger changed notifier, ret = %d\n", ret);
		// return ret;
	}
	charger_manager_unregister_psy_notifier(manager);

	/*ret = charger_manager_unregister_thermal_notifier(manager);
	if (ret < 0) {
		hq_err("couldn't unregister thermal notifier, ret = %d\n", ret);
		// return ret;
	}*/
        ret = charger_manager_unregister_charger_notifier(manager);
	if (ret < 0) {
		hq_err("couldn't unregister charger notifier, ret = %d\n", ret);
		// return ret;
	}

	return ret;
}
#endif

static int charger_manager_parse_dts(struct charger_manager *manager)
{
	struct device_node *node = manager->dev->of_node;
	int ret = false;
	int i;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
        int retv = false;
#endif

	manager->en_floatgnd = of_property_read_bool(node, "hq_chg_manager,en_floatgnd");
#ifndef KERNEL_FACTORY_BUILD
	manager->single_cell_det = of_property_read_bool(node, "hq_chg_manager,single_cell_det");
#endif //KERNEL_FACTORY_BUILD
	hq_info("manager en_floatgnd is %d, en_detcells is %d\n", manager->en_floatgnd, manager->single_cell_det);

	ret |= of_property_read_u32(node, "hq_chg_manager,charge_control_limit_max", &manager->charge_control_limit_max);
	ret |= of_property_read_u32(node, "hq_chg_manager,QC3_mode", &manager->qc3_mode);
	ret |= of_property_read_u32(node, "hq_chg_manager,usb_charger_current", &manager->usb_current);
	ret |= of_property_read_u32(node, "hq_chg_manager,float_charger_current", &manager->float_current);
	ret |= of_property_read_u32(node, "hq_chg_manager,ac_charger_current", &manager->dcp_current);
	ret |= of_property_read_u32(node, "hq_chg_manager,cdp_charger_current", &manager->cdp_current);
	ret |= of_property_read_u32(node, "hq_chg_manager,hvdcp_charger_current", &manager->hvdcp_charge_current);
	ret |= of_property_read_u32(node, "hq_chg_manager,hvdcp_input_current", &manager->hvdcp_input_current);
	ret |= of_property_read_u32(node, "hq_chg_manager,hvdcp3_charger_current", &manager->hvdcp3_charge_current);
	ret |= of_property_read_u32(node, "hq_chg_manager,hvdcp3_input_current", &manager->hvdcp3_input_current);
	ret |= of_property_read_u32(node, "hq_chg_manager,pd2_charger_current", &manager->pd2_charge_current);
	ret |= of_property_read_u32(node, "hq_chg_manager,pd2_input_current", &manager->pd2_input_current);
	ret |= of_property_read_u32(node, "hq_chg_manager,input_power_over", &manager->input_power_over);
	ret |= of_property_read_string(node, "hq_chg_manager,model_name", &manager->model_name);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	ret |= of_property_read_u32(node, "hq_chg_manager,xm_outdoor_current", &manager->xm_outdoor_current);
        retv = of_property_read_u32_array(node, "hq_chg_manager,cyclecount", manager->cyclecount, CYCLE_COUNT_MAX);
        if(retv){
                pr_err("use default CYCLE_COUNT: 0\n");
                for(i = 0; i < CYCLE_COUNT_MAX; i++)
                        manager->cyclecount[i] = 0;
        }
        ret |= retv;
#endif

	if (ret)
		return false;
	else
		return true;
}

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
static int charger_thermal_notifier_event(struct notifier_block *notifier,
			unsigned long chg_event, void *val)
{
	struct charger_manager *manager = container_of(notifier,
							struct charger_manager, charger_thermal_nb);

	switch (chg_event) {
	case THERMAL_BOARD_TEMP:
		manager->thermal_board_temp = *(int *)val;
		pr_info("%s: get thermal_board_temp: %d\n", __func__, manager->thermal_board_temp);
		break;
	default:
		pr_err("%s: not supported charger notifier event: %lu\n", __func__, chg_event);
		break;
	}

	return NOTIFY_DONE;
}

static int screen_state_for_charger_callback(struct notifier_block *nb,
                                            unsigned long val, void *v)
{
        int blank = *(int *)v;
        struct charger_manager *manager = container_of(nb, struct charger_manager, sm.charger_panel_notifier);

        if (!(val == MTK_DISP_EARLY_EVENT_BLANK|| val == MTK_DISP_EVENT_BLANK)) {
                pr_err("%s event(%lu) do not need process\n", __func__, val);
                return NOTIFY_OK;
        }

        switch (blank) {
        case MTK_DISP_BLANK_UNBLANK: //power on
                manager->sm.screen_state = 0;
                pr_info("%s screen_state = %d\n", __func__, manager->sm.screen_state);
                break;
        case MTK_DISP_BLANK_POWERDOWN: //power off
                manager->sm.screen_state = 1;
                pr_info("%s screen_state = %d\n", __func__, manager->sm.screen_state);
                break;
        }
        return NOTIFY_OK;
}
#endif

static int charger_manager_fg_param_init(struct charger_manager *manager)
{
	int ret = 0;
	int i = 0;

	/* initialize battery cycle get actual battery cycle count as soon as possible */

	for (i = 0; i < 3; i++) {
		ret = auth_device_get_cycle_count(manager->auth_dev, &manager->batt_cycle);
		if (ret != 0) {
			manager->batt_cycle = 0;
			hq_err("[EEA] failed to init battery cycle count\n");
		} else {
			hq_info("[EEA] success to init battery cycle count, batt_cycle = %d\n",
				manager->batt_cycle);
			break;
		}
	}


	mdelay(20);
	/* initialize battery soh get actual battery soh as soon as possible */

	for (i = 0; i < 3; i++) {
		manager->batt_raw_soh = auth_device_get_raw_soh(manager->auth_dev);
		if (manager->batt_raw_soh < 0) {
			manager->batt_raw_soh = 100;
			hq_err("[EEA] failed to init battery raw soh\n");
		} else {
			hq_info("[EEA] success to init battery raw soh, batt_raw_soh = %d\n",
				manager->batt_raw_soh);
			break;
		}
	}

	/* just set invalid value here as fg deamon not ready */
	manager->fg_cycle = -1;
	manager->fg_raw_soh = -1;

	/* update_fg_param_work init */
	mutex_init(&manager->update_auth_param_lock);

	INIT_DELAYED_WORK(&manager->update_fg_raw_soh_work, update_fg_raw_soh_work);
	INIT_DELAYED_WORK(&manager->update_fg_cycle_work, update_fg_cycle_work);

	return ret;
}

static int charger_manager_probe(struct platform_device *pdev)
{
	struct charger_manager *manager;
	int ret = 0;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	struct timespec64 ts_boot;
	struct timespec64 ts_delay;
	struct timespec64 ts_shiled;
#endif

	hq_info("running (%s)\n", CHARGER_MANAGER_VERSION);

	manager = devm_kzalloc(&pdev->dev, sizeof(*manager), GFP_KERNEL);
	if (!manager)
		return -ENOMEM;

	manager->dev = &pdev->dev;
	platform_set_drvdata(pdev, manager);

	charger_manager_check_dev(manager);

	ret = charger_manager_parse_dts(manager);
	if (!ret)
		hq_err("charger_manager_parse_dts failed\n");

	charger_manager_create_votable(manager);

	charger_manager_usb_psy_register(manager);

	manager->fake_batt_cycle = 0xFFFF;
	charger_manager_batt_psy_register(manager);

	charger_manager_chg_psy_register(manager);

	if (!IS_ERR_OR_NULL(manager->batt_psy)) {
		ret = hq_batt_sysfs_create_group(manager);
		if (ret < 0)
			hq_err("create batt sysfs nodes failed\n");
	}

	if (!IS_ERR_OR_NULL(manager->usb_psy)) {
		ret = hq_usb_sysfs_create_group(manager);
		if (ret < 0)
			hq_err("create usb nodes failed\n");
	}

	hq_jeita_init(manager->dev);

	ret = charge_manager_thermal_init(manager);
	if (ret < 0)
		hq_err("charge_manager_thermal_init failed, ret = %d\n", ret);

	init_waitqueue_head(&manager->wait_queue);
#if IS_ENABLED(CONFIG_MIEV)
        ret = charger_manager_register_notifiers(manager);
	if (ret < 0) {
		hq_err("charger manager register notifiers failed\n");
		return -EINVAL;
	}
#endif
	charger_manager_fg_param_init(manager);
	manager->charger_nb.notifier_call = charger_manager_notifer_call;
	charger_register_notifier(&manager->charger_nb);

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	INIT_DELAYED_WORK(&manager->xm_charge_work, xm_charge_work);

	manager->thermal_board_temp = 250;
	manager->charger_thermal_nb.notifier_call = charger_thermal_notifier_event;
	charger_thermal_reg_notifier(&manager->charger_thermal_nb);

	manager->sm.charger_panel_notifier.notifier_call = screen_state_for_charger_callback;
	ret = mtk_disp_notifier_register("screen state", &manager->sm.charger_panel_notifier);
	if (ret) 
		hq_err("register screen state callback fail(%d)\n", ret);
	
	manager->smart_batt = 0;
	manager->night_charging = false;
	manager->night_charging_flag = false;
	manager->low_fast_plugin_flag = false;
	manager->pps_fast_mode = false;
	manager->b_flag = B_NORMAL;

	INIT_DELAYED_WORK(&manager->thermal_restore_work, thermal_restore_work);
	ktime_get_boottime_ts64(&ts_boot);
	ts_shiled.tv_sec = 60;
	ts_shiled.tv_nsec = 0;
	ts_delay = timespec64_sub(ts_shiled, ts_boot);
	//hq_info("ts_delay: tv_sec = %llds, tv_nsec = %ldns, jiffies = %lu\n",
	//	ts_delay.tv_sec, ts_delay.tv_nsec, timespec64_to_jiffies(&ts_delay));
	schedule_delayed_work(&manager->thermal_restore_work, timespec64_to_jiffies(&ts_delay));

#endif //CONFIG_XIAOMI_SMART_CHG

#if IS_ENABLED(CONFIG_HUAQIN_SOFT_ITERM_SUPPORT)
	INIT_DELAYED_WORK(&manager->soft_iterm_work, soft_iterm_work);
#endif //CONFIG_HUAQIN_SOFT_ITERM_SUPPORT

	INIT_DELAYED_WORK(&manager->second_detect_work, apsd_second_detect_work);
#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
	INIT_DELAYED_WORK(&manager->wait_usb_ready_work, wait_usb_ready_work);
#endif //CONFIG_USB_MTK_HDRC

#if IS_ENABLED(CONFIG_HUAQIN_SOH2_SUPPORT)
	INIT_DELAYED_WORK(&manager->batt_soh20_aging_test, batt_soh20_aging_test);
#endif

#if IS_ENABLED(CONFIG_TCPC_CLASS)
	if (IS_ERR_OR_NULL(manager->tcpc)) {
		hq_err("manager->tcpc is null\n");
	} else {
		manager->pd_nb.notifier_call = charger_manager_tcpc_notifier_call;
		ret = register_tcp_dev_notifier(manager->tcpc, &manager->pd_nb,
								TCP_NOTIFY_TYPE_ALL);
		if (ret < 0) {
			hq_err("register tcpc notifier fail(%d)\n", ret);
			return ret;
		}
	}

	alarm_init(&manager->rust_det_work_timer, ALARM_BOOTTIME,rust_det_work_timer_handler);
	INIT_DELAYED_WORK(&manager->hrtime_otg_work, hrtime_otg_work);
	INIT_DELAYED_WORK(&manager->set_cc_drp_work, set_cc_drp_work);
	manager->cid_status = false;
#endif //CONFIG_TCPC_CLASS

	if (manager->single_cell_det){
		alarm_init(&manager->start_cell_det_work_timer, ALARM_BOOTTIME,start_cell_det_work_timer_handler);
		alarm_init(&manager->cell_det_work_timer, ALARM_BOOTTIME,cell_det_work_timer_handler);
	}
	manager->half_cell = 1;

	device_init_wakeup(manager->dev, true);
	manager->run_thread = true;
	manager->thread = kthread_run(charger_manager_thread_fn, manager,
								"charger_manager_thread");
#if IS_ENABLED(CONFIG_HUAQIN_FUEL_ALGORITHM)
	ret = hq_fuel_algo_init(manager);
	if (ret < 0)
		hq_err("hq_fuel_algo_init fail\n");

#endif //CONFIG_HUAQIN_FUEL_ALGORITHM

	/*****************for shipmode**********************
	 *Register syscore ops, just for, when restart in shipmode,
	 *it will reproduce any device shutdown fail
	 *So first, do device_shutdown, and then syscore ops->shutdown
	 */
	register_syscore_ops(&shipmode_syscore_ops);

	hq_info("success\n");
	return 0;
}

static void charger_manager_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct charger_manager *manager = platform_get_drvdata(pdev);
	hq_jeita_deinit();
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	cancel_delayed_work(&manager->xm_charge_work);
#endif

#if IS_ENABLED(CONFIG_HUAQIN_SOFT_ITERM_SUPPORT)
	cancel_delayed_work(&manager->soft_iterm_work);
#endif

#if IS_ENABLED(CONFIG_HUAQIN_USE_AUTH_CYCLE_COUNT)
	cancel_delayed_work(&manager->update_fg_raw_soh_work);
	cancel_delayed_work(&manager->update_fg_cycle_work);
#endif

#if IS_ENABLED(CONFIG_TCPC_CLASS)
	ret = unregister_tcp_dev_notifier(manager->tcpc, &manager->pd_nb,
					  TCP_NOTIFY_TYPE_ALL);
	if (ret < 0)
		hq_err("unregister tcpc notifier fail(%d)\n", ret);
#endif
	charger_unregister_notifier(&manager->charger_nb);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
        charger_thermal_unreg_notifier(&manager->charger_thermal_nb);
#if IS_ENABLED(CONFIG_MIEV)
        charger_manager_unregister_notifiers(manager);
#endif
        ret = mtk_disp_notifier_unregister(&manager->sm.charger_panel_notifier);
        if (ret < 0)
		hq_err("unregister screen state notifier fail(%d)\n", ret);
#endif

}

static void charger_manager_shutdown(struct platform_device *pdev)
{
	int ret = 0;
	struct charger_manager *manager = platform_get_drvdata(pdev);
	struct charger_dev *charger = charger_find_dev_by_name("primary_chg");

	//Avoid raising VBUS to 12V after shutdown
	if (manager == NULL) {
		pr_err("manager get null!!\n");
		return;
	}

	if(manager->vbus_type == VBUS_TYPE_HVDCP){
		ret = charger_qc2_vbus_mode(charger, 5000);
		if (ret < 0)
			pr_err("set qc 5v fail\n");
		else
			pr_info("set qc 5v success\n");
	}

	if (manager->tcpc->ops->set_dis_vconn_ov) {
		manager->tcpc->ops->set_dis_vconn_ov(manager->tcpc, true);
		pr_info("set ship mode TCPC_V10_REG_FAULT_CTRL_DIS_VCONN_OV\n");
	}
}

static const struct of_device_id charger_manager_match[] = {
	{.compatible = "huaqin,hq_chg_manager",},
	{},
};
MODULE_DEVICE_TABLE(of, charger_manager_match);

static struct platform_driver charger_manager_driver = {
	.probe = charger_manager_probe,
	.remove = charger_manager_remove,
	.shutdown = charger_manager_shutdown,
	.driver = {
		.name = "hq_chg_manager",
		.of_match_table = charger_manager_match,
	},
};

static int __init charger_manager_init(void)
{
	hq_err("---->\n");
	return platform_driver_register(&charger_manager_driver);
}

late_initcall(charger_manager_init);

MODULE_DESCRIPTION("Huaqin Charger Manager Core");
MODULE_LICENSE("GPL v2");
/*
 * HQ main_chg Release Note
 *
 *
 * 1.1.2
 *  (1)Add a flag to mark the new BC detection
 *  Therefore, a BC detection was added after the PD encryption was completed.
 *
 * 1.1.1
 * (1)Add logic to not handle negative values at main_fcc, main_icl and total_fcc
 *
 * 1.1.0
 * (1)Modify usb_psy register：replace usb_psy_desc with manager->usb_psy_desc to register usb_psy
 *
 * 1.0.9
 * (1)encapsulate two APIs : charger_manager_monitor and charger_manager_charger_type_detect
 * (2)Add low_vbat_power_off_check
 *
 * 1.0.8
 * (1) Separate sysfs[psy and other]: creat sysfs in hq_charge_sysfs.c
 *
 * 1.0.7
 * (1) Add hq_thermal func: charge_manager_thermal_init
 *
 * 1.0.6
 * (1) Add hq_jeita func: hq_jeita_init
 *
 * 1.0.5
 * (1) Do not let the system sleep during charging:pm_stay_awake/pm_relax
 * (2) Start qc detection, it must PD not active
 * (3) Start cp policy sm, need PD active=2
 * (4) Add new tcpc notifier: handle tcpc event
 *
 * 1.0.4
 * (1) Add usb psy new node: real_type, typec_orientation, otg and input_suspend
 *
 * 1.0.3
 * (1) Add usb psy new node: real_type, typec_orientation, otg and input_suspend
 *
 * 1.0.2
 * (1) Add hq voter policy: charger_manager_create_votable
 *
 * 1.0.1
 * (1) Add battery psy node: charger_manager_batt_psy_register
 *
 * 1.0.0
 * (1) Add driver for hq charger manager
 * (2) charger_manager_thread_fn todo : 1)BC12 detect 2)cp policy entry 3)power off check .etc
 */
