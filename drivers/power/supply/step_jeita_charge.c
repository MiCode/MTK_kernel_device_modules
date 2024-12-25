/*
 * step/jeita charge controller
 *
 * published by the Free Software Foundation.

 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*	date			author			comment
 *	2021-06-01		chenyichun@xiaomi.com	create
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/power_supply.h>
#include <linux/iio/consumer.h>
#include <linux/time.h>
#include "mtk_charger.h"
#include "adapter_class.h"
#include "mtk_battery.h"
#include "pd_cp_manager.h"

/* low fast parameters */
static int low_fast_exit_soc = 41;
module_param_named(low_fast_exit_soc, low_fast_exit_soc, int, 0600);
/* low_fast_hyst_temp_xxx is for 25 ambient temp */
static int low_fast_hyst_temp_low = 380;
module_param_named(low_fast_hyst_temp_low, low_fast_hyst_temp_low, int, 0600);
/* current value that low enough to make sure temp won't exceed low_fast_critical_temp when  */
static int low_fast_hyst_temp_high = 395;
module_param_named(low_fast_hyst_temp_high, low_fast_hyst_temp_high, int, 0600);
/*	force charging with low_fast_high_temp_curr when temp is above low_fast_hyst_temp_high.
	if low_fast_high_temp_curr != -1, it will take effect when low fast is in process
	and temp is above low_fast_hyst_temp_high. And will charge with low_fast_high_temp_curr current.
	if low_fast_high_temp_curr == -1, it will not take effect. Just like it is turned off
	low fast will switch to noraml sic_vote when it's in process and temp is above low_fast_hyst_temp_high.
	And will charge with normal sic_vote current.
*/
static int low_fast_high_temp_curr = -1;
module_param_named(low_fast_high_temp_curr, low_fast_high_temp_curr, int, 0600);
/* must lower than low_fast_exit_soc */
static int low_fast_cooldown_soc = 40;
module_param_named(low_fast_cooldown_soc, low_fast_cooldown_soc, int, 0600);
/* really low current to make sure temp can cooldown when it's about to exit low fast.
won't take effect when set to -1 */
static int low_fast_cooldown_curr = 2000;
module_param_named(low_fast_cooldown_curr, low_fast_cooldown_curr, int, 0600);
/* low_fast_critical_temp_xxxx is for 35 ambient temp */
static int low_fast_critical_temp_low = 410;
module_param_named(low_fast_critical_temp_low, low_fast_critical_temp_low, int, 0600);
static int low_fast_critical_temp_high= 420;
module_param_named(low_fast_critical_temp_high, low_fast_critical_temp_high, int, 0600);
/* current value that low enough to make sure temp won't exceed low_fast_critical_temp_high
	won't take effect when set to -1*/
static int low_fast_critical_curr = 3500;
module_param_named(low_fast_critical_curr, low_fast_critical_curr, int, 0600);
/* is support cooldown? */
static int low_fast_is_support_cooldown = 0;
module_param_named(low_fast_is_support_cooldown, low_fast_is_support_cooldown, int, 0600);
/* end of low fast parameters */

static void get_index(struct step_jeita_cfg0 *cfg, int fallback_hyst, int forward_hyst, int value, int *index, bool ignore_hyst)
{
	int new_index = 0, i = 0;

	chr_err("%s: value = %d, index[0] = %d, index[1] = %d\n", __func__, value, index[0], index[1]);

	if (value < cfg[0].low_threshold) {
		index[0] = index[1] = 0;
		return;
	}

	if (value > cfg[STEP_JEITA_TUPLE_COUNT - 1].high_threshold)
		new_index = STEP_JEITA_TUPLE_COUNT - 1;

	for (i = 0; i < STEP_JEITA_TUPLE_COUNT; i++) {
		if (is_between(cfg[i].low_threshold, cfg[i].high_threshold, value)) {
			new_index = i;
			break;
		}
	}

	if (ignore_hyst) {
		index[0] = index[1] = new_index;
	} else {
		if (new_index > index[0]) {
			if (value < (cfg[new_index].low_threshold + forward_hyst))
				new_index = index[0];
		} else if (new_index < index[0]) {
			if (value > (cfg[new_index].high_threshold - fallback_hyst))
				new_index = index[0];
		}
		index[1] = index[0];
		index[0] = new_index;
	}

	chr_err("%s: value = %d, index[0] = %d, index[1] = %d, new_index = %d\n", __func__, value, index[0], index[1], new_index);
	return;
}

static void monitor_sw_cv(struct mtk_charger *info)
{
	int ibat = 0;

	if (info->step_chg_index[0] > info->step_chg_index[1] && (info->step_chg_cfg[info->step_chg_index[0]].value != info->step_chg_cfg[info->step_chg_index[1]].value)) {
		info->sw_cv_count = 0;
		info->sw_cv = info->step_chg_cfg[info->step_chg_index[0]].low_threshold + info->step_forward_hyst;
	}

	if (info->sw_cv || info->suspend_recovery) {
		ibat = info->current_now;
		if (ibat <= info->step_chg_fcc) {
			info->sw_cv_count++;
			if (info->sw_cv_count >= SW_CV_COUNT) {
				info->sw_cv = 0;
				info->sw_cv_count = 0;
				vote(info->fcc_votable, STEP_CHARGE_VOTER, true, info->step_chg_fcc);
			}
		} else {
			info->sw_cv_count = 0;
		}
	}
}

static void monitor_jeita_descent(struct mtk_charger *info)
{
	int current_fcc = 0;

	current_fcc = get_client_vote(info->fcc_votable, JEITA_CHARGE_VOTER);
	if (current_fcc != info->jeita_chg_fcc) {
		if (current_fcc >= info->jeita_chg_fcc + JEITA_FCC_DESCENT_STEP)
			vote(info->fcc_votable, JEITA_CHARGE_VOTER, true, current_fcc - JEITA_FCC_DESCENT_STEP);
		else if (current_fcc >= info->jeita_chg_fcc - JEITA_FCC_DESCENT_STEP)
			vote(info->fcc_votable, JEITA_CHARGE_VOTER, true, info->jeita_chg_fcc);
		else
			vote(info->fcc_votable, JEITA_CHARGE_VOTER, true, current_fcc + JEITA_FCC_DESCENT_STEP);
	chr_err("current_fcc:%d,%d,%d,%d",current_fcc,
	current_fcc - JEITA_FCC_DESCENT_STEP, info->jeita_chg_fcc, current_fcc + JEITA_FCC_DESCENT_STEP);
	}
}

static void charger_set_dpdm_voltage(struct mtk_charger *info, int dp, int dm)
{
	info->chg1_dev = get_charger_by_name("primary_chg");
	if (info->chg1_dev)
		chr_err("Found primary charger\n");
	else {
		chr_err("*** Error : can't find primary charger ***\n");
		return;
	}
	charger_dev_set_dpdm_voltage(info->chg1_dev, dp, dm);
}
static void usbotg_enable_otg(struct mtk_charger *info, bool is_on)
{
	info->chg1_dev = get_charger_by_name("primary_chg");
	if (info->chg1_dev)
		chr_err("Found primary charger\n");
	else {
		chr_err("*** Error : can't find primary charger ***\n");
		return;
	}
	charger_dev_enable_otg_regulator(info->chg1_dev, is_on);
}

static void monitor_usb_otg_burn(struct work_struct *work)
{
	int otg_monitor_delay_time = 5000;
	struct mtk_charger *info = container_of(work, struct mtk_charger, usb_otg_monitor_work.work);
	int type_temp = 0, pmic_vbus = 0, val = 0;

	usb_get_property(USB_PROP_TYPEC_NTC1_TEMP, &val);
	type_temp = val;
	usb_get_property(USB_PROP_TYPEC_NTC2_TEMP, &val);
	if (type_temp <= val)
		type_temp = val;

	usb_get_property(USB_PROP_PMIC_VBUS, &pmic_vbus);

	if(type_temp <= 450)
		otg_monitor_delay_time = 5000;
	else if(type_temp > 450 && type_temp <= 550)
		otg_monitor_delay_time = 2000;
	else if(type_temp > 550)
		otg_monitor_delay_time = 1000;
	chr_err("%s:get typec temp =%d otg_monitor_delay_time = %d\n", __func__, type_temp, otg_monitor_delay_time);

	if(type_temp >= TYPEC_BURN_TEMP && !info->typec_otg_burn) {
		info->typec_otg_burn = true;
		usbotg_enable_otg(info, false);
		chr_err("%s:disable otg\n", __func__);
		charger_dev_cp_reset_check(info->cp_master);
	} else if ((info->typec_otg_burn && type_temp <= (TYPEC_BURN_TEMP - TYPEC_BURN_HYST)) ||
            (pmic_vbus < 1000 && info->otg_enable && (type_temp <= TYPEC_BURN_TEMP - TYPEC_BURN_HYST))) {
		info->typec_otg_burn = false;
		usbotg_enable_otg(info, true);
		chr_err("%s:enable otg\n", __func__);
	}

	schedule_delayed_work(&info->usb_otg_monitor_work, msecs_to_jiffies(otg_monitor_delay_time));
}

#if defined(CONFIG_RUST_DETECTION)
static bool judge_four_condi_meet_two(const int *lpd_res)
{
	int count = 0, i = 0;
	if(lpd_res == NULL)
		return false;
	for(i=0; i < 4 ; i++)
	{
		if(lpd_res[i] < LPD_TRIGGER_THRESHOLD)
			count++;
	}
	if(count >= 2)
		return true;
	return false;
}

static void __maybe_unused generate_xm_lpd_uevent(struct mtk_charger *info);
static void monitor_usb_rust(struct mtk_charger *info, int pin)
{
	int res = 0;
	bool is_et7480 = false;
	charger_dev_rust_detection_is_et7480(info->et7480_chg_dev, &is_et7480);
	charger_dev_rust_detection_choose_channel(info->et7480_chg_dev, pin);
	charger_dev_rust_detection_enable(info->et7480_chg_dev, true);
	msleep(100);
	res = charger_dev_rust_detection_read_res(info->et7480_chg_dev);
	chr_err("%s: pin=%d, res=%d\n", __func__, pin, res);
	if(is_et7480 && (pin == RUST_DET_DP_PIN || pin == RUST_DET_DM_PIN))
	{
		res -= 6;
		if(res < 0)
			res = 0;
		chr_err("%s: pin=%d, et7480 cali_res=%d\n", __func__, pin, res);
	}
	info->lpd_res[pin] = res;
}

static void rust_detection_work_func(struct work_struct *work)
{
	struct timespec64 time;
	ktime_t tmp_time = 0;
	struct mtk_charger *info = container_of(work, struct mtk_charger, rust_detection_work.work);
	int i = 0;
	static int rust_det_interval = 3000;

	tmp_time = ktime_get_boottime();
	time = ktime_to_timespec64(tmp_time);
	if(time.tv_sec < 50) {
		chr_err("%s boot do not enter\n", __func__);
		//return;
		goto out;
	}

	if (info->et7480_chg_dev == NULL) {
		info->et7480_chg_dev = get_charger_by_name("et7480_chg");
		if (info->et7480_chg_dev)
			chr_err("Found et7480 charger\n");
		else {
			chr_err("*** Error : can't find et7480 charger ***\n");
			//return;
			goto out;
		}
	}

	charger_dev_rust_detection_init(info->et7480_chg_dev);

	for (i=1; i<=RUST_DET_SBU2_PIN; i++) {
		if (info->typec_attach) {
			return;
		}
		monitor_usb_rust(info, i);
	}

	if(info->first_lpd_det){
		rust_det_interval = 1000;
        info->first_lpd_det = false;
	}else if((info->lpd_res[1] < LPD_TRIGGER_THRESHOLD && info->lpd_res[2] < LPD_TRIGGER_THRESHOLD && 
					(info->lpd_res[3] < LPD_TRIGGER_THRESHOLD || info->lpd_res[4] < LPD_TRIGGER_THRESHOLD)) ||
					(info->lpd_flag && judge_four_condi_meet_two(info->lpd_res)))
	{
		info->lpd_debounce_times ++;
		rust_det_interval = 5000;
	}else{
		info->lpd_debounce_times = 0;
		info->lpd_flag = false;
		rust_det_interval = 10000;
	}
	chr_err("[XM_LPD] lpd_debounce_times=%d\n", info->lpd_debounce_times);
	if(info->lpd_debounce_times >= 2){
		chr_err("[XM_LPD] LPD appear UEVENT\n");
		info->lpd_flag = true;
	}else{
		chr_err("[XM_LPD] LPD disappare UEVENT\n");
	}
  	generate_xm_lpd_uevent(info);
out:
	if(info->screen_status == DISPLAY_SCREEN_ON || (info->screen_status == DISPLAY_SCREEN_OFF && info->lpd_flag))
		schedule_delayed_work(&info->rust_detection_work, msecs_to_jiffies(rust_det_interval));
}

#define MAX_UEVENT_LENGTH 100
static void __maybe_unused generate_xm_lpd_uevent(struct mtk_charger *info)
{
	static char uevent_string[MAX_UEVENT_LENGTH+1] = {
		"POWER_SUPPLY_MOISTURE_DET_STS=\n"  //length=30
	};
	u32 cnt=0, i=0;
	char *envp[5] = { NULL };  //the length of array need adjust when uevent number increase

	sprintf(uevent_string+30,"%d", info->lpd_flag);
	envp[cnt++] = uevent_string;

	envp[cnt]=NULL;
	for(i = 0; i < cnt; ++i)
	      chr_err("%s\n", envp[i]);
	kobject_uevent_env(&info->pdev->dev.kobj, KOBJ_CHANGE, envp);
	chr_err("%s: LPD_KOBJECT_UEVENT END\n", __func__);
	return;
}
#endif

static void monitor_typec_burn(struct work_struct *work)
{
	struct mtk_charger *info = container_of(work, struct mtk_charger, typec_burn_monitor_work.work);
	int type_temp = 0, retry_count = 2, val = 0;
	bool cp_master_enable = false;
	int typec_burn_delay_time = 5000;

	usb_get_property(USB_PROP_TYPEC_NTC1_TEMP, &val);
	type_temp = val;
	usb_get_property(USB_PROP_TYPEC_NTC2_TEMP, &val);
	if (type_temp <= val)
		type_temp = val;
	chr_err("%s get typec temp=%d otg_enable=%d\n", __func__, type_temp, info->otg_enable);

	if(type_temp <= 450)
		typec_burn_delay_time = 5000;
	else if(type_temp > 450 && type_temp <= 550)
		typec_burn_delay_time = 2000;
	else if(type_temp > 550)
		typec_burn_delay_time = 1000;
    if (type_temp > TYPEC_BURN_TEMP - TYPEC_BURN_HYST)
            update_connect_temp(info);
	if (type_temp >= TYPEC_BURN_TEMP && !info->typec_burn_status && !info->otg_enable) {
		info->typec_burn = true;
		while (retry_count) {
			if (info->cp_master) {
				charger_dev_is_enabled(info->cp_master, &cp_master_enable);
			}
			if (!cp_master_enable)
				break;
			msleep(80);
			retry_count--;
		}
		adapter_dev_set_cap_xm(info->pd_adapter, MTK_PD_APDO, 5000, 1000);
        msleep(200);
		if (info->real_type == XMUSB350_TYPE_HVCHG)
			charger_set_dpdm_voltage(info, 0, 0);

		usb_set_property(USB_PROP_INPUT_SUSPEND, info->typec_burn);
		vote(info->icl_votable, TYPEC_BURN_VOTER, true, 0);
		info->typec_burn_status = true;
		msleep(1000);
		if (info->mt6369_moscon1_control) {
			mtk_set_mt6369_moscon1(info, 1, 1);
			chr_err("mt6368_moscon1_control set high\n");
		} 
	} else if (info->typec_burn && type_temp <= TYPEC_BURN_TEMP - TYPEC_BURN_HYST && info->typec_burn_status) {
		info->typec_burn = false;
		if (info->mt6369_moscon1_control) {
			mtk_set_mt6369_moscon1(info, 0, 0);
			chr_err("mt6369_moscon1_control set low\n");
		} 

        usb_set_property(USB_PROP_INPUT_SUSPEND, info->typec_burn);
		vote(info->icl_votable, TYPEC_BURN_VOTER, false, 0);
		info->typec_burn_status = false;

	}

	if(info->plugged_status || info->typec_burn)
		schedule_delayed_work(&info->typec_burn_monitor_work, msecs_to_jiffies(typec_burn_delay_time));
}

static void handle_jeita_charge(struct mtk_charger *info)
{
	static bool jeita_vbat_low = true;
	static int last_temp = 0;

	get_index(info->jeita_fv_cfg, info->jeita_fallback_hyst, info->jeita_forward_hyst, info->temp_now, info->jeita_chg_index, false);

	vote(info->fv_votable, JEITA_CHARGE_VOTER, true, info->jeita_fv_cfg[info->jeita_chg_index[0]].value  - info->diff_fv_val + info->pmic_comp_v);

	if (jeita_vbat_low) {
		if (info->vbat_now < (info->jeita_fcc_cfg[info->jeita_chg_index[0]].extra_threshold + 50)) {
			info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].low_value;
		} else {
			info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].high_value;
			jeita_vbat_low = false;
		}
	} else {
		if (info->vbat_now < (info->jeita_fcc_cfg[info->jeita_chg_index[0]].extra_threshold - 100)) {
			info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].low_value;
			jeita_vbat_low = true;
		} else {
			info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].high_value;
		}
	}

	if(info->div_jeita_fcc_flag)
	{
		info->jeita_chg_fcc = info->jeita_chg_fcc * 8 / 10;
		chr_err("[%s]: soa or isp induce jeita current drop: %d\n", __func__, info->jeita_chg_fcc);
	}

	if ((info->temp_now < -100) || (info->temp_now > 580)) {
		info->jeita_chg_fcc = 0;
		chr_err("[%s]: temp_now:%d, not charging\n", __func__, info->temp_now);
	}

	if (is_between(info->jeita_fcc_cfg[0].low_threshold, info->jeita_fcc_cfg[STEP_JEITA_TUPLE_COUNT - 1].high_threshold, info->temp_now) && !info->input_suspend && !info->typec_burn)
        chr_err("handle_jeita_charge index = %d,jeita_chg_fcc = %d", info->jeita_chg_index[0], info->jeita_chg_fcc);

	if ((last_temp < 0 && info->temp_now >= 0) || (last_temp >= 0 && info->temp_now < 0)
           || (last_temp <= 480 && info->temp_now > 480) || (last_temp > 480 && info->temp_now <= 480))
		update_quick_chg_type(info);

	pr_err("[%s]:last_temp:%d, temp_now:%d\n", __func__, last_temp, info->temp_now);
	last_temp = info->temp_now;
	return;
}

static void handle_step_charge(struct mtk_charger *info)
{
	get_index(info->step_chg_cfg, info->step_fallback_hyst, info->step_forward_hyst, info->vbat_now, info->step_chg_index, false);

	if (info->step_chg_index[0] == STEP_JEITA_TUPLE_COUNT - 1)
		info->step_chg_fcc = info->step_chg_cfg[info->step_chg_index[0]].value + 100;
	else
		info->step_chg_fcc = info->step_chg_cfg[info->step_chg_index[0]].value;

	 chr_err("handle_step_charge index = %d", info->step_chg_index[0]);
	return;
}

static int get_smart_chg_level(struct mtk_charger *info)
{
	int i = 0;
	int al = info->lowfast_array_length;
	for(i = 0; i < al; i++) {
		if(info->thermal_board_temp <= info->low_fast_thermal[i])
			break;
	}
	return (i > al - 1) ? (al - 1) : i;
}

static void monitor_thermal_limit(struct mtk_charger *info)
{
	int thermal_level = 0;
	struct timespec64 ts;

	ktime_get_boottime_ts64(&ts);
	if ((u64)ts.tv_sec < 60) {
		chr_err("[charger] ignore thermal...\n");
		thermal_level = 0;
		goto out;
	}

	//int iterm_effective = get_effective_result(info->iterm_votable);
	if (info->thermal_level < 0)
		thermal_level = -1 - info->thermal_level;
	else if (info->thermal_level >= THERMAL_LIMIT_COUNT)
		info->thermal_level = THERMAL_LIMIT_COUNT - 1;
	else if (info->low_fast_in_process == true && (info->smart_chg[SMART_CHG_BATT_LOW_FAST].active_status == true)) {
		pr_err("%s low fast is precessing, cancel thermal limit\n", __func__);
		vote(info->fcc_votable, THERMAL_VOTER, false, 0);
		return;
	} else
		thermal_level = info->thermal_level;

	if (info->smart_snschg_support && info->smart_chg[SMART_CHG_SENSING_CHG].active_status)
	{
		if (info->smart_chg_lmt_lvl >= DECREASE_L_LVL)
		{
			thermal_level += (info->smart_chg_lmt_lvl - DECREASE_REF_LVL);
			thermal_level = (thermal_level >= THERMAL_LIMIT_COUNT)? (THERMAL_LIMIT_COUNT-1) : thermal_level;
		}
		else
		{
			thermal_level -= (info->smart_chg_lmt_lvl - INCREASE_REF_LVL);
			thermal_level = (thermal_level < 0)? 0 : thermal_level;
		}
		chr_err("smartchg-sensingchg trigger, adjust thermal level from %d to %d\n", info->thermal_level, thermal_level);
	}

out:
	switch(info->real_type) {
	case XMUSB350_TYPE_DCP:
		info->thermal_current = info->thermal_limit[0][thermal_level];
		vote(info->fcc_votable, THERMAL_VOTER, true, info->thermal_limit[0][thermal_level]);
		break;
	case XMUSB350_TYPE_HVDCP:
	case XMUSB350_TYPE_HVDCP_2:
		info->thermal_current = info->thermal_limit[1][thermal_level];
		vote(info->fcc_votable, THERMAL_VOTER, true, info->thermal_limit[1][thermal_level]);
		break;
	case XMUSB350_TYPE_HVDCP_3:
	case XMUSB350_TYPE_HVDCP_3_18:
		info->thermal_current = info->thermal_limit[2][thermal_level];
		vote(info->fcc_votable, THERMAL_VOTER, true, info->thermal_limit[2][thermal_level]);
		break;
	case XMUSB350_TYPE_HVDCP_3_27:
		info->thermal_current = info->thermal_limit[3][thermal_level];
		vote(info->fcc_votable, THERMAL_VOTER, true, info->thermal_limit[3][thermal_level]);
		break;
	case XMUSB350_TYPE_HVDCP_35_18:
	case XMUSB350_TYPE_HVDCP_35_27:
		info->thermal_current = info->thermal_limit[4][thermal_level];
		vote(info->fcc_votable, THERMAL_VOTER, true, info->thermal_limit[4][thermal_level]);
		break;
	case XMUSB350_TYPE_PD:
		info->thermal_current = info->thermal_limit[5][thermal_level];
		vote(info->fcc_votable, THERMAL_VOTER, true, info->thermal_limit[5][thermal_level]);
		break;
	case XMUSB350_TYPE_HVCHG:
		info->thermal_current = info->thermal_limit[1][thermal_level];
		vote(info->fcc_votable, THERMAL_VOTER, true, info->thermal_limit[1][thermal_level]);
		break;
	default:
		info->thermal_current = info->thermal_limit[0][thermal_level];
		vote(info->fcc_votable, THERMAL_VOTER, true, info->thermal_limit[0][thermal_level]);
		chr_err("real_type:%d, not support psy_type to check charger parameters", info->real_type);
	}
}

static int handle_ffc_charge(struct mtk_charger *info)
{
	int ret = 0, iterm_ffc = 0, iterm = 0;

	if (info->cycle_count >= 100 && info->cycle_count <= 200) {
		info->ffc_high_soc = 90;
	} else if (info->cycle_count > 200) {
		info->ffc_high_soc = 90;
	} else {
		/* do nothing*/
	}

	if ((!info->charge_full && !info->recharge && info->entry_soc <= info->ffc_high_soc && is_between(info->ffc_low_tbat, info->ffc_high_tbat, info->temp_now) &&
			((info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO && info->pd_adapter->verifed)) && info->jeita_chg_index[0] <= 4) || info->suspend_recovery)
		info->ffc_enable = true;
	else
		info->ffc_enable = false;

	if (info->batt_id == SWD_5110MAH || info->batt_id == NVT_5110MAH)
			iterm = info->iterm_warm;
	else if (info->batt_id == SWD_5500MAH || info->batt_id == NVT_5500MAH)
			iterm = info->iterm_warm_cn;
	else
			iterm = info->iterm;

	if (is_between(info->ffc_medium_tbat, info->ffc_warm_tbat, info->temp_now)) {
		iterm_ffc = min(info->iterm_ffc_little_warm, ITERM_FCC_WARM);
	} else if (is_between(info->ffc_warm_tbat, info->ffc_little_high_tbat, info->temp_now)) {
		iterm_ffc = min(info->iterm_ffc_warm, ITERM_FCC_WARM);
	} else if (is_between(info->ffc_little_high_tbat, info->ffc_high_tbat, info->temp_now)) {
		iterm_ffc = min(info->iterm_ffc_hot, ITERM_FCC_WARM);
	} else {
		iterm_ffc = min(info->iterm_ffc, ITERM_FCC_WARM);
	}

	if (!info->gauge_authentic)
		vote(info->fcc_votable, FFC_VOTER, true, 2000);
	else
		vote(info->fcc_votable, FFC_VOTER, true, 22000);

	if (info->ffc_enable) {
		vote(info->fv_votable, FFC_VOTER, true, info->fv_ffc - info->diff_fv_val + info->pmic_comp_v);
		vote(info->iterm_votable, FFC_VOTER, true, 100);
	} else if (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
		if (info->jeita_chg_index[0] == 4) {
			vote(info->fv_votable, FFC_VOTER, true, info->fv_normal  - info->diff_fv_val + info->pmic_comp_v);
		} else {
			vote(info->fv_votable, FFC_VOTER, true, info->fv  - info->diff_fv_val + info->pmic_comp_v);
		}
		vote(info->iterm_votable, FFC_VOTER, true, 100);
	} else {
		if (info->jeita_chg_index[0] == 4) {
			vote(info->fv_votable, FFC_VOTER, true, info->fv_normal  - info->diff_fv_val + info->pmic_comp_v);
		} else {
			vote(info->fv_votable, FFC_VOTER, true, info->fv  - info->diff_fv_val + info->pmic_comp_v);
		}
		vote(info->iterm_votable, FFC_VOTER, true, 100);
	}

	return ret;
}

static void check_full_recharge(struct mtk_charger *info)
{
	static int full_count = 0, recharge_count = 0, iterm = 0, threshold_mv = 0, eoc_stat_count = 0;
	static int eoc_count = 0;
	int iterm_effective = get_effective_result(info->iterm_votable);
	int fv_effective = get_effective_result(info->fv_votable) - info->pmic_comp_v;
	int ret = 0, pmic_vbus = 0;
	u32 stat;
	bool is_masterCpEn = false;

	if (info->ffc_enable) {
		if (is_between(info->ffc_medium_tbat, info->ffc_warm_tbat, info->temp_now)) {
			iterm = min(info->iterm_ffc_little_warm, ITERM_FCC_WARM);
		} else if (is_between(info->ffc_warm_tbat, info->ffc_little_high_tbat, info->temp_now)) {
			iterm = min(info->iterm_ffc_warm, ITERM_FCC_WARM);
		} else if (is_between(info->ffc_little_high_tbat, info->ffc_high_tbat, info->temp_now)) {
			iterm = min(info->iterm_ffc_hot, ITERM_FCC_WARM);
		} else {
			iterm = min(info->iterm_ffc, ITERM_FCC_WARM);
		}
	} else {
		if (info->batt_id == SWD_5110MAH || info->batt_id == NVT_5110MAH)
			iterm = info->iterm_warm;
		else if (info->batt_id == SWD_5500MAH || info->batt_id == NVT_5500MAH)
			iterm = info->iterm_warm_cn;
		else
			iterm = info->iterm;
	}

	chr_err("%s diff_fv_val = %d, iterm = %d, iterm_effective = %d, fv_effective = %d, full_count = %d, recharge_count = %d, eoc_count=%d\n", __func__,
		info->diff_fv_val, iterm, iterm_effective, fv_effective, full_count, recharge_count, eoc_count);

	if (info->charge_full) {
		full_count = 0;
		eoc_count = 0;
		if (info->vbat_now <= fv_effective - 300)
			recharge_count++;
		else
			recharge_count = 0;

		if (((recharge_count >= 15) || (info->is_full_flag == false)) && (info->jeita_chg_index[0] <= 4)) {
			info->charge_full = false;
			info->charge_eoc = false;
            info->warm_term = false;
			info->is_full_flag = false;
			chr_err("start recharge\n");
			if(info->real_full){
				info->recharge = true;
				info->real_full = false;
			}
			recharge_count = 0;
			charger_dev_enable(info->chg1_dev, true);
			power_supply_changed(info->psy1);
		}
	} else {
		recharge_count = 0;

		if (info->real_type == XMUSB350_TYPE_SDP)
			threshold_mv = 15;
		else if (info->temp_now > 0)
			threshold_mv = 35;
		else
			threshold_mv = 100;

		if ((info->vbat_now >= fv_effective - threshold_mv) &&
			((info->current_now <= iterm) || ((info->current_now <= iterm_effective + 100) && info->bbc_charge_done))) {
			full_count++;
		} else {
			full_count = 0;
		}

		if (full_count >= 6 && !info->charge_eoc) {
			full_count = 0;
			info->charge_eoc = true;
			chr_err("charge_full notify gauge eoc\n");
			if (info->ffc_enable) {
				;//battery_set_property(BAT_PROP_CHARGE_EOC, true);
			} else {
				eoc_count = 6;
			}
		}

		if (info->charge_eoc) {
			eoc_count++;
		} else {
			eoc_count = 0;
		}
		if (eoc_count >= 6) {
			eoc_count = 0;
			info->charge_full = true;
			info->warm_term = true;
			chr_err("report charge_full\n");

			if (info->ffc_enable)
				info->last_ffc_enable = true;

			if(info->jeita_chg_index[0] <= 4) {
				info->real_full = true;
				info->warm_term = false;
				info->is_full_flag = true;
				charger_dev_do_event(info->chg1_dev, EVENT_FULL, 0);
				power_supply_changed(info->psy1);
			}
			charger_dev_enable(info->chg1_dev, false);//diable pmic
			charger_dev_enable_powerpath(info->chg1_dev, true);

			if(info->real_type == XMUSB350_TYPE_HVCHG){
				int cnt = 0;
				pmic_vbus = get_vbus(info);
				charger_set_dpdm_voltage(info, 0, 0);
				while(cnt <= 15 && pmic_vbus > 7000)
				{
					msleep(2);
					pmic_vbus = get_vbus(info);
					cnt++;
				}
				charger_dev_set_mivr(info->chg1_dev, info->data.min_charger_voltage);
				chr_err("report charge_full: set DP:0, DM:0 for setting 5V\n");
			} else {
				charger_dev_is_enabled(info->cp_master, &is_masterCpEn);
				pmic_vbus = get_vbus(info);
				chr_err("[%s]cp_master:%d, pmic_vbus:%d\n", __func__, is_masterCpEn, pmic_vbus);

				if(!is_masterCpEn && pmic_vbus > 2500){
					ret = adapter_dev_set_cap_xm(info->pd_adapter, MTK_PD_APDO, 5000, 3000);
					chr_err("report charge_full: request 5V/3A:%d\n", ret);
				}
			}

			info->recharge = false;
			charger_dev_enable(info->chg1_dev, false);
		}

		ret = charger_dev_get_charge_ic_stat(info->chg1_dev, &stat);
		if(ret<0)
			chr_err("read F_IC_STAT failed\n");
		else
			chr_err("read F_IC_STAT success stat=%d\n", stat);
		if(stat == CHG_STAT_EOC)
			eoc_stat_count++;
		else
			eoc_stat_count = 0;

		chr_err("eoc_stat_count=%d\n", eoc_stat_count);

		if(eoc_stat_count >= 15 && !atomic_read(&info->ieoc_wkrd)) {
			eoc_stat_count = 0;
			if(info->vbat_now <= (fv_effective-40))
				info->pmic_comp_v = 30;
			else if(info->vbat_now <= (fv_effective-30))
				info->pmic_comp_v = 20;
			else if(info->vbat_now <= (fv_effective-20))
				info->pmic_comp_v = 10;
			else
				info->pmic_comp_v = 0;

			vote(info->fv_votable, JEITA_CHARGE_VOTER, true, fv_effective + info->pmic_comp_v);
			if(is_client_vote_enabled(info->fv_votable, FV_DEC_VOTER))
				vote(info->fv_votable, FFC_VOTER, true, fv_effective + info->pmic_comp_v + 10);
			else
				vote(info->fv_votable, FFC_VOTER, true, fv_effective + info->pmic_comp_v);
			msleep(10);
			ret = charger_dev_reset_eoc_state(info->chg1_dev);
			if(ret < 0)
				chr_err("failed to reset eoc stat\n");
			charger_dev_enable_termination(info->chg1_dev, true);
			atomic_set(&info->ieoc_wkrd, 1);
			chr_err("enter compensate cv=%d, vbat=%d, comp_cv=%d\n", fv_effective, info->vbat_now, info->pmic_comp_v);
		} else if(info->vbat_now > fv_effective && atomic_read(&info->ieoc_wkrd)) {
			info->pmic_comp_v = 0;
			chr_err("cancle compensate fv_effective=%d\n", fv_effective);
		}
	}
}

static void check_charge_data(struct mtk_charger *info)
{
	union power_supply_propval pval = {0,};
	int ret = 0;
	static struct power_supply *verify_psy = NULL;
	struct mtk_battery *battery_drvdata = NULL;

	if (!info->battery_psy) {
		info->battery_psy = power_supply_get_by_name("battery");
		chr_err("failed to get battery_psy\n");
		return;
	}

	if(info->battery_psy){
		battery_drvdata = power_supply_get_drvdata(info->battery_psy);
		info->smart_chg = battery_drvdata->smart_chg;
		chr_err("[XMCHG_MONITOR] set mtk_charger smart_chg done!\n");
	}

	if (!info->cp_master) {
		info->cp_master = get_charger_by_name("cp_master");
		chr_err("failed to get master cp charger\n");
		return;
	}

	charger_dev_is_enabled(info->chg1_dev, &info->bbc_charge_enable);
	charger_dev_is_charging_done(info->chg1_dev, &info->bbc_charge_done);

	ret = power_supply_get_property(info->battery_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret)
		chr_err("failed to get soc\n");
	else
		info->soc = pval.intval;

	ret = power_supply_get_property(info->battery_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret)
		chr_err("failed to get vbat\n");
	else
		info->vbat_now = pval.intval / 1000;

	ret = power_supply_get_property(info->battery_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret)
		chr_err("failed to get ibat\n");
	else
		info->current_now = pval.intval / 1000;

	ret = power_supply_get_property(info->battery_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret)
		chr_err("failed to get tbat\n");
	else
		info->temp_now = pval.intval;

	ret = power_supply_get_property(info->battery_psy, POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT, &pval);
	if (ret) {
		chr_err("failed to get thermal level\n");
	} else {
		if (info->thermal_remove)
			info->thermal_level = 0;
		else if (pval.intval >= THERMAL_LIMIT_COUNT)
			info->thermal_level = THERMAL_LIMIT_COUNT - 1;
		else
			info->thermal_level = pval.intval;
	}

	if (!verify_psy)
		verify_psy = power_supply_get_by_name("batt_verify");

	if (verify_psy) {
		ret = power_supply_get_property(verify_psy, POWER_SUPPLY_PROP_AUTHENTIC, &pval);
		if (ret < 0) {
			chr_err("Couldn't read authentic, ret=%d\n", ret);
			info->gauge_authentic = 0;
		} else
			info->gauge_authentic = pval.intval;
	} else
		info->gauge_authentic = 0;

	chr_err("diff_fv_val:%d, and will clear diff_fv_val as 0\n", info->diff_fv_val);
	info->diff_fv_val = 0;

        /* FFC fv cyc */
	if (info->ffc_enable) {
		if (info->cycle_count >= 200 && info->cycle_count < 800) {
			info->diff_fv_val =  20;
		} else if (info->cycle_count >= 1000 && info->cycle_count < 1100) {
			info->diff_fv_val =  20;
		} else if (info->cycle_count >= 1100) {
			info->diff_fv_val = 40;
		} else {
			/* do nothing*/
		}
	} else {  /* normal fv cyc */
		if (info->cycle_count > 800) {
			info->diff_fv_val = 40;
		} else if (info->cycle_count > 300) {
			info->diff_fv_val = 20;
		} else if (info->cycle_count > 100) {
			info->diff_fv_val = 10;
		} else {
			/* do nothing*/
		}
	}

	info->diff_fv_val = max(info->diff_fv_val, info->set_smart_batt_diff_fv);
	info->diff_fv_val = max(info->diff_fv_val, info->set_smart_fv_diff_fv);
	if (info->pd_adapter) {
		chr_err("[CHARGE_LOOP] TYPE = [%d %d %d], BMS = [%d %d %d %d], FULL = [%d %d %d %d], scrn =%d, low_fast[%d %d %d], thermal_temp=%d, thermal_level=%d, FFC = %d, sw_cv=%d, gauge_authentic=%d, warm_term=%d, smart_batt:%d, smart_fv:%d\n",
			info->pd_type, info->pd_adapter->verifed, info->real_type, info->soc, info->vbat_now, info->current_now, info->temp_now,
			info->bbc_charge_enable, info->charge_full, info->bbc_charge_done, info->recharge,
			info->screen_status==DISPLAY_SCREEN_ON, info->smart_chg[SMART_CHG_BATT_LOW_FAST].en_ret, info->smart_chg[SMART_CHG_BATT_LOW_FAST].active_status, info->low_fast_in_process,
			info->thermal_board_temp, info->thermal_level, info->ffc_enable, info->sw_cv, info->gauge_authentic, info->warm_term, info->set_smart_batt_diff_fv, info->set_smart_fv_diff_fv);
	}
	if (info->input_suspend || info->typec_burn)
		chr_err("[CHARGE_LOOP] input_suspend = %d, typec_burn = %d\n", info->input_suspend, info->typec_burn);
}


static void smart_chg_handle_soc_limit(struct mtk_charger *info)
{
	/*
	 * Priority: Endurance > Navigation > NightChg
	 */
	if(info->smart_chg[SMART_CHG_ENDURANCE_PRO].en_ret && !info->smart_chg[SMART_CHG_ENDURANCE_PRO].active_status && info->soc >= SMART_CHG_SOCLMT_TRIG_DEFAULT)
	{
		info->smart_chg[SMART_CHG_ENDURANCE_PRO].active_status = true;
	}
	else if((!info->smart_chg[SMART_CHG_ENDURANCE_PRO].en_ret || info->soc <= SMART_CHG_SOCLMT_TRIG_DEFAULT - SMART_CHG_SOCLMT_CANCEL_HYS_DEFAULT) && info->smart_chg[SMART_CHG_ENDURANCE_PRO].active_status)
	{
		info->smart_chg[SMART_CHG_ENDURANCE_PRO].active_status = false;
	}
	else if(info->smart_chg[SMART_CHG_NAVIGATION].en_ret && !info->smart_chg[SMART_CHG_NAVIGATION].active_status && info->soc >= info->smart_chg[SMART_CHG_NAVIGATION].func_val)
	{
		info->smart_chg[SMART_CHG_NAVIGATION].active_status = true;
	}
	else if((!info->smart_chg[SMART_CHG_NAVIGATION].en_ret || info->soc <= info->smart_chg[SMART_CHG_NAVIGATION].func_val - SMART_CHG_SOCLMT_CANCEL_HYS_DEFAULT) && info->smart_chg[SMART_CHG_NAVIGATION].active_status)
	{
		info->smart_chg[SMART_CHG_NAVIGATION].active_status = false;
	}
	else if(info->night_charging && !info->night_charge_enable && info->soc >= SMART_CHG_SOCLMT_TRIG_DEFAULT)
	{
		info->night_charge_enable = true;
	}
	else if((!info->night_charging || info->soc <=75) && info->night_charge_enable)
	{
		info->night_charge_enable = false;
	}

	if((info->smart_chg[SMART_CHG_ENDURANCE_PRO].active_status || info->smart_chg[SMART_CHG_NAVIGATION].active_status || info->night_charge_enable) && !info->smart_soclmt_trig)
	{
		info->smart_soclmt_trig = true;
		charger_dev_enable(info->chg1_dev, false);
		chr_err("smart_chg soclimit triggger, stop charging\n");
	}
	else if(!info->smart_chg[SMART_CHG_ENDURANCE_PRO].active_status && !info->smart_chg[SMART_CHG_NAVIGATION].active_status && !info->night_charge_enable && info->smart_soclmt_trig)
	{
		info->smart_soclmt_trig = false;
		charger_dev_enable(info->chg1_dev, true);
		chr_err("smart_chg soclimit cancel, restart charging\n");
	}
}
/*
static void smart_chg_handle_sensing_chg(struct mtk_charger *info)
{
	int scene = NORMAL_SCENE, board_temp = 0, chg_lmt_lvl = DEFAULT_LMT_LVL, active_stat = 0;
	int posture_st = info->smart_chg[SMART_CHG_SENSING_CHG].func_val;
	static int last_sic_current;
	int current_tmp = 0;

	usb_get_property(USB_PROP_SCENE, &scene);
	usb_get_property(USB_PROP_BOARD_TEMP, &board_temp);

	// scene redirection
	if (scene == PHONE_SCENE)
		scene = PHONE_REDIR_SCENE;
	else if (scene == VIDEOCHAT_SCENE)
		scene = VIDEOCHAT_REDIR_SCENE;
	else if (scene == TGAME_SCENE || scene == MGAME_SCENE || scene == YUANSHEN_SCENE || scene == XINGTIE_SCENE)
		scene = GAME_REDIR_SCENE;
	else if (scene == CLASS0_SCENE || scene == PER_CLASS0_SCENE)
		scene = CLASS0_REDIR_SCENE;
	else if (scene == VIDEO_SCENE || scene == PER_VIDEO_SCENE || scene == YOUTUBE_SCENE || scene == PER_YOUTUBE_SCENE)
		scene = VIDEO_REDIR_SCENE;

	if(!info->smart_chg[SMART_CHG_SENSING_CHG].en_ret
			|| (posture_st <= UNKNOW_STAT || posture_st >= STAT_MAX_INDEX)
			|| (scene < PHONE_REDIR_SCENE || scene > VIDEO_REDIR_SCENE))
	{
		info->smart_chg[SMART_CHG_SENSING_CHG].active_status = false;
		chg_lmt_lvl = DEFAULT_LMT_LVL;
		chr_err("%s sensing chg turnoff, or invalid posture/scene detected.\n", __func__);
	}
	else if(scene == PHONE_REDIR_SCENE || scene == VIDEOCHAT_REDIR_SCENE)
	{
		if((posture_st >= ONEHAND_STAT && posture_st <= ANS_CALL_STAT) && board_temp >= 39000)
		{
			active_stat = true;
			chg_lmt_lvl = DECREASE_H_LVL;
		}
		else if((posture_st == DESKTOP_STAT || posture_st == HOLDER_STAT) && board_temp < 34000)
		{
			active_stat = true;
			if(board_temp < 30000)
				chg_lmt_lvl = INCREASE_H_LVL;
			else if(board_temp < 32000)
				chg_lmt_lvl = INCREASE_M_LVL;
			else
				chg_lmt_lvl = INCREASE_L_LVL;
		}
		else if(scene == PHONE_REDIR_SCENE && (posture_st >= ONEHAND_STAT && posture_st <= TWOHAND_V_STAT) && board_temp < 34000)
		{
			active_stat = true;
			if(board_temp < 30000)
				chg_lmt_lvl = INCREASE_M_LVL;
			else
				chg_lmt_lvl = INCREASE_L_LVL;
		}
		else
		{
			active_stat = false;
			chg_lmt_lvl = DEFAULT_LMT_LVL;
		}
	}
	else if(scene == GAME_REDIR_SCENE && (posture_st == TWOHAND_H_STAT || posture_st == TWOHAND_V_STAT) && board_temp > 41000)
	{
		active_stat = true;
		if(board_temp > 45000)
			chg_lmt_lvl = DECREASE_H_LVL;
		else if(board_temp > 43000)
			chg_lmt_lvl = DECREASE_M_LVL;
		else
			chg_lmt_lvl = DECREASE_L_LVL;
	}
	else if(scene == CLASS0_REDIR_SCENE && posture_st == ONEHAND_STAT && board_temp > 39000)
	{
		active_stat = true;
		if(board_temp > 41000)
			chg_lmt_lvl = DECREASE_M_LVL;
		else
			chg_lmt_lvl = DECREASE_L_LVL;
	}
	else
	{
		active_stat = false;
		chg_lmt_lvl = DEFAULT_LMT_LVL;
	}

	// update sic
	if(info->sic_support && info->sic_current > 0
			&& (active_stat != info->smart_chg[SMART_CHG_SENSING_CHG].active_status || chg_lmt_lvl != info->smart_chg_lmt_lvl || last_sic_current != info->sic_current))
	{
		if(active_stat == false || chg_lmt_lvl == DEFAULT_LMT_LVL)
		{
			vote(info->fcc_votable, SIC_VOTER, true, info->sic_current);
		}
		else
		{
			if(chg_lmt_lvl >= DECREASE_L_LVL)
			{
				current_tmp = info->sic_current - (chg_lmt_lvl - DECREASE_REF_LVL) * 700;
				current_tmp = (current_tmp < 300)? 300 : current_tmp;
				vote(info->fcc_votable, SIC_VOTER, true, current_tmp);
			}
		}
	}

	info->smart_chg_lmt_lvl = chg_lmt_lvl;
	info->smart_chg[SMART_CHG_SENSING_CHG].active_status = active_stat;
	last_sic_current = info->sic_current;
	chr_err("%s scene:%d, tboard:%d, post:%d --> lmt:%d, act:%d, cur:%d \n", __func__, scene, board_temp, posture_st, chg_lmt_lvl, active_stat, current_tmp);
}
*/

static void smart_chg_handle_low_fast_chg(struct mtk_charger *info)
{
	int sic_current_incre = 0;
	static ktime_t last_change_time = -1;
	static int screen_status_last;
	int screen_on_timeout = 0,screen_off_soc_low = 0;
	static int need_cooldown;
	static int board_temp_is_high;

	/* low battery, faster charge */
		/** screen control logic */
	if (info->smart_chg[SMART_CHG_BATT_LOW_FAST].active_status == true &&
		screen_status_last == DISPLAY_SCREEN_OFF && info->screen_status == DISPLAY_SCREEN_ON){
		last_change_time = ktime_get();
	}
	else if (info->smart_chg[SMART_CHG_BATT_LOW_FAST].active_status == false &&
		screen_status_last == DISPLAY_SCREEN_ON && info->screen_status == DISPLAY_SCREEN_OFF &&
		info->soc < low_fast_exit_soc)
	{
		last_change_time = -1;
		screen_off_soc_low = true;
		screen_on_timeout = false;
	}
	else if (screen_status_last == DISPLAY_SCREEN_OFF && info->screen_status == DISPLAY_SCREEN_OFF)
	{
		last_change_time = -1;
		screen_off_soc_low = false;
	} else
	{
		screen_off_soc_low = false;
	}

	screen_status_last = info->screen_status;
	if(last_change_time >= 0)
		screen_on_timeout = (ktime_ms_delta(ktime_get(), last_change_time) > LOW_FAST_SCREEN_TIMEOUT_MS) ? true : false;

	/** board temp control logic */
	if(info->thermal_board_temp >= low_fast_hyst_temp_high &&
		info->smart_chg[SMART_CHG_BATT_LOW_FAST].active_status == true) {
		board_temp_is_high = true;
	}
	if(info->thermal_board_temp <= low_fast_hyst_temp_low &&
		info->smart_chg[SMART_CHG_BATT_LOW_FAST].active_status == false) {
		board_temp_is_high = false;
	}

	/** board temp control logic */
	if(info->soc >= low_fast_cooldown_soc && info->thermal_board_temp >= low_fast_hyst_temp_low &&
		info->low_fast_in_process == true && low_fast_is_support_cooldown) {
		need_cooldown = true;
	}
	if(info->low_fast_in_process == false) {
		need_cooldown = false;
	}

	chr_err("%s in_process:%d,soc[%d %d],scrn[%d,%d],battlow[%d %d]\n",
		__func__,info->low_fast_in_process, info->entry_soc, info->soc, screen_off_soc_low, screen_on_timeout,
		info->smart_chg[SMART_CHG_BATT_LOW_FAST].en_ret,info->smart_chg[SMART_CHG_BATT_LOW_FAST].active_status);

	/** entry low fast, set in-process flag */
	if(info->low_fast_in_process == false &&
		info->soc <= LOW_FAST_ENTRY_SOC &&
		info->screen_status == DISPLAY_SCREEN_OFF &&
		info->thermal_board_temp <= low_fast_hyst_temp_high &&
		info->smart_chg[SMART_CHG_BATT_LOW_FAST].en_ret == true &&
		info->smart_chg[SMART_CHG_BATT_LOW_FAST].active_status == false &&
		info->real_type == XMUSB350_TYPE_PD && info->pd_verifed == true)
	{
		info->low_fast_in_process = true;
		info->smart_chg[SMART_CHG_BATT_LOW_FAST].active_status = true;
		chr_err("%s Enter battery low fast!\n", __func__);
	}

/** low fast restart while it's still in process*/
	if(info->low_fast_in_process && info->smart_chg[SMART_CHG_BATT_LOW_FAST].active_status == false &&
		(screen_off_soc_low || (!board_temp_is_high && info->soc < LOW_FAST_NOT_RESTART_SOC)))
	{
		info->smart_chg[SMART_CHG_BATT_LOW_FAST].active_status = true;
		chr_err("%s battery low fast restart!soc:%d,screen_off_soc_low:%d,board temp high:%d\n",
			__func__,info->soc, screen_off_soc_low,board_temp_is_high);
	}

	/** low fast stop while it's still in process*/
	if (info->low_fast_in_process && info->smart_chg[SMART_CHG_BATT_LOW_FAST].active_status == true &&
		(screen_on_timeout || (board_temp_is_high && low_fast_high_temp_curr == -1)))
	{
		info->smart_chg[SMART_CHG_BATT_LOW_FAST].active_status = false;
		chr_err("%s battery low fast stop!soc:%d, low fast en:%d, board temp high:%d,screen_on_timeout:%d\n",
			__func__, info->soc,info->smart_chg[SMART_CHG_BATT_LOW_FAST].en_ret,
			board_temp_is_high,screen_on_timeout);
	}

	/** exit low fast, reset in-process flag */
	if(info->low_fast_in_process == true &&
		(info->soc >= low_fast_exit_soc ||
		info->smart_chg[SMART_CHG_BATT_LOW_FAST].en_ret == false ||
		info->real_type != XMUSB350_TYPE_PD))
	{
		info->low_fast_in_process = false;
		info->smart_chg[SMART_CHG_BATT_LOW_FAST].active_status = false;
		vote(info->fcc_votable, SIC_INCRE_VOTER, false, 0);
		chr_err("%s exit battery low fast!\n", __func__);
		chr_err("%s soc:%d,scrn_stat:%s,thermal_temp:%d,en_ret:%d,active_stat:%d",
			__func__, info->soc, info->screen_status==DISPLAY_SCREEN_ON ? "on":"off",
			info->thermal_board_temp,
			info->smart_chg[SMART_CHG_BATT_LOW_FAST].en_ret,
			info->smart_chg[SMART_CHG_BATT_LOW_FAST].active_status);
	}

	/** Do low fast, vote for FCC */
	if(info->smart_chg[SMART_CHG_BATT_LOW_FAST].active_status == true)
	{
		info->sic_thermal_level = get_smart_chg_level(info);
		sic_current_incre = info->low_fast_current[info->sic_thermal_level];
		sic_current_incre = (sic_current_incre >= MAX_THERMAL_FCC) ? MAX_THERMAL_FCC : sic_current_incre;
		if (need_cooldown && low_fast_cooldown_curr != -1){
			vote(info->fcc_votable, SIC_INCRE_VOTER, true, low_fast_cooldown_curr);
			chr_err("%s soc is %d, cooldown_curr is %d,thermal_temp=%d\n",
				__func__, info->soc, low_fast_cooldown_curr, info->thermal_board_temp);
		} else if(board_temp_is_high && low_fast_high_temp_curr != -1){
			vote(info->fcc_votable, SIC_INCRE_VOTER, true, low_fast_high_temp_curr);
			chr_err("%s board_temp_is_high is %d, high_temp_curr is %d, thermal_temp=%d\n",
			__func__, board_temp_is_high, low_fast_high_temp_curr, info->thermal_board_temp);
		} else {
			vote(info->fcc_votable, SIC_INCRE_VOTER, true, sic_current_incre);
		}
		chr_err("%s sic_incr is %d, sic_level=%d, T_temp:%d\n",
			__func__, sic_current_incre, info->sic_thermal_level, info->thermal_board_temp);
	} else {
		vote(info->fcc_votable, SIC_INCRE_VOTER, false, 0);
		chr_err("%s battery low fast not active! sic_incr is %d, sic_level=%d\n",
				__func__, sic_current_incre, info->sic_thermal_level);
	}
}

static void monitor_smart_chg(struct mtk_charger *info)
{
	if(info == NULL || !info->fcc_votable || !info->smart_chg)
		return;

	smart_chg_handle_soc_limit(info);
	smart_chg_handle_low_fast_chg(info);
	// if(info->smart_snschg_support)
	// 	smart_chg_handle_sensing_chg(info);
}

static void monitor_fv_descent(struct mtk_charger *info)
{
	bool isEffective = false;
	int fv_vote = 0, fv_vote_dec = 0, ffc_fv_vote = 0, jeita_fv_vote = 0;
	static int last_fv_vote = 0, last_ffc_fv_vote = 0, last_jeita_fv_vote = 0;
	int diff_fv_result = max(info->set_smart_batt_diff_fv, info->set_smart_fv_diff_fv);

	if(!info->cp_sm_run_state && (diff_fv_result == 0 || info->diff_fv_val > diff_fv_result)){

		isEffective = is_client_vote_enabled(info->fv_votable, FV_DEC_VOTER);
		ffc_fv_vote = get_client_vote(info->fv_votable, FFC_VOTER);
		jeita_fv_vote = get_client_vote(info->fv_votable, JEITA_CHARGE_VOTER);

		if(!isEffective) {
			fv_vote = get_effective_result(info->fv_votable);
			if(info->vbat_now >= (fv_vote -7)) {
				vote(info->fv_votable, FV_DEC_VOTER, true, fv_vote - 10);
				chr_err("[%s] fv vote reduce cv 10 mv:%d\n", __func__, isEffective);
			} else {
				vote(info->fv_votable, FV_DEC_VOTER, false, 0);
			}
		} else if (isEffective && (last_ffc_fv_vote != ffc_fv_vote || last_jeita_fv_vote != jeita_fv_vote)){
			fv_vote_dec = get_effective_result(info->fv_votable);
			vote(info->fv_votable, FV_DEC_VOTER, false, 0);
			fv_vote = get_effective_result(info->fv_votable);

			if(last_fv_vote == fv_vote)
				vote(info->fv_votable, FV_DEC_VOTER, true, fv_vote_dec);
			else if(info->vbat_now >= (fv_vote -7)) {
				vote(info->fv_votable, FV_DEC_VOTER, true, fv_vote - 10);
				chr_err("[%s] fv vote reduce cv 10 mv:%d\n", __func__, isEffective);
			} else {
				vote(info->fv_votable, FV_DEC_VOTER, false, 0);
				chr_err("[%s] cancel fv vote reduce cv 10 mv:%d\n", __func__, isEffective);
			}
		}
		chr_err("[%s][%d]  fv_vote[%d,%d,%d], FFC[%d, %d], jeita[%d,%d], isEffective:%d\n",
					__func__, info->cp_sm_run_state, fv_vote, last_fv_vote, fv_vote_dec, 
					ffc_fv_vote, last_ffc_fv_vote, jeita_fv_vote, last_jeita_fv_vote, isEffective);
		last_fv_vote = fv_vote;
		last_ffc_fv_vote = ffc_fv_vote;
		last_jeita_fv_vote = jeita_fv_vote;
	}else{
		last_fv_vote = last_ffc_fv_vote = last_jeita_fv_vote = 0;
		vote(info->fv_votable, FV_DEC_VOTER, false, 0);
	}
}

static void monitor_fv_overvoltage(struct mtk_charger *info)
{
	static int fv_overvoltage_count = 0;
	int fcc_vote = 0, fv_vote = 0;

	fcc_vote = get_effective_result(info->fcc_votable);
	fv_vote = get_effective_result(info->fv_votable);
	chr_err("[%s] %d:%d:%d:%d:%d\n", __func__, fv_overvoltage_count, info->vbat_now, fv_vote, info->ffc_enable, fcc_vote);

	if((info->vbat_now > fv_vote) && info->ffc_enable) {
		fv_overvoltage_count++;
		if(fv_overvoltage_count > 1000)
			fv_overvoltage_count = 3;
	}
	if(!info->fv_overvoltage_flag && (fv_overvoltage_count >= 3) && info->ffc_enable && (fcc_vote >= 3000)) {
		chr_err("[%s] enable\n", __func__);
		info->fv_overvoltage_flag = true;
		fv_overvoltage_count = 0;
		fcc_vote -= 1000;
		vote(info->fcc_votable, FV_OVERVOLTAGE_VOTER,true, fcc_vote);
	}
	if(info->fv_overvoltage_flag && !info->ffc_enable)
	{
		chr_err("[%s] disable\n", __func__);
		info->fv_overvoltage_flag = false;
		fv_overvoltage_count = 0;
		vote(info->fcc_votable, FV_OVERVOLTAGE_VOTER,false, 0);
	}
}

static void charge_monitor_func(struct work_struct *work)
{
	struct mtk_charger *info = container_of(work, struct mtk_charger, charge_monitor_work.work);

	check_charge_data(info);

	handle_ffc_charge(info);

	check_full_recharge(info);

	monitor_thermal_limit(info);

	handle_step_charge(info);

	handle_jeita_charge(info);

	monitor_smart_chg(info);

	monitor_sw_cv(info);

	monitor_jeita_descent(info);

	monitor_fv_descent(info);

	monitor_fv_overvoltage(info);

	schedule_delayed_work(&info->charge_monitor_work, msecs_to_jiffies(FCC_DESCENT_DELAY));
}

static int debug_cycle_count = 0;
module_param_named(debug_cycle_count, debug_cycle_count, int, 0600);

static int parse_battery_cycle_dts(struct mtk_charger *info, int cycle)
{
	struct device_node *np = info->pdev->dev.of_node;
	int total_length = 0, i = 0, ret = 0;
	char name[64];
	u32 val = 0;

	/* step parameter */
	if (info->batt_id == SWD_5110MAH || info->batt_id == NVT_5110MAH)
		ret = snprintf(name, sizeof(name), "step_chg_cfg_%d_cycle_gl", cycle);
	else
		ret = snprintf(name, sizeof(name), "step_chg_cfg_%d_cycle", cycle);
	if (ret >= sizeof(name))
		chr_err("%s: type_c name is truncated\n", __func__);

	chr_err("%s: parse %s\n", __func__, name);

	total_length = of_property_count_elems_of_size(np, name, sizeof(u32));
	if (total_length < 0) {
		chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, name, (u32 *)info->step_chg_cfg, total_length);
	if (ret) {
		chr_err("%s: failed to parse %s\n", __func__, name);
		return ret;
	}

	for (i = 0; i < STEP_JEITA_TUPLE_COUNT; i++) {
		chr_err("%s: cycle=%d,%d STEP %d %d %d\n", __func__,
			info->cycle_count, cycle, info->step_chg_cfg[i].low_threshold,
			info->step_chg_cfg[i].high_threshold, info->step_chg_cfg[i].value);
	}

	/* iterm_ffc parameter */
	ret = snprintf(name, sizeof(name), "iterm_ffc_%d_cycle_%d", cycle, info->batt_id);
	if (ret >= sizeof(name))
		chr_err("%s: type_c name is truncated\n", __func__);

	chr_err("%s: parse %s\n", __func__, name);

	if (of_property_read_u32(np, name, &val) >= 0)
		info->iterm_ffc = val;
	else {
		chr_err("failed to parse %s use default\n", name);
		info->iterm_ffc = 700;
	}

	ret = snprintf(name, sizeof(name), "iterm_ffc_little_warm_%d_cycle_%d", cycle, info->batt_id);
	if (ret >= sizeof(name))
		chr_err("%s: type_c name is truncated\n", __func__);

	chr_err("%s: parse %s\n", __func__, name);

	if (of_property_read_u32(np, name, &val) >= 0)
		info->iterm_ffc_little_warm = val;
	else {
		chr_err("failed to parse %s use default\n", name);
		info->iterm_ffc_little_warm = 800;
	}

	ret = snprintf(name, sizeof(name), "iterm_ffc_warm_%d_cycle_%d", cycle, info->batt_id);
	if (ret >= sizeof(name))
		chr_err("%s: type_c name is truncated\n", __func__);

	chr_err("%s: parse %s\n", __func__, name);

	if (of_property_read_u32(np, name, &val) >= 0)
		info->iterm_ffc_warm = val;
	else {
		chr_err("failed to parse %s use default\n", name);
		info->iterm_ffc_warm = 800;
	}

	ret = snprintf(name, sizeof(name), "iterm_ffc_hot_%d_cycle_%d", cycle, info->batt_id);
	if (ret >= sizeof(name))
		chr_err("%s: type_c name is truncated\n", __func__);

	chr_err("%s: parse %s\n", __func__, name);

	if (of_property_read_u32(np, name, &val) >= 0)
		info->iterm_ffc_hot = val;
	else {
		chr_err("failed to parse %s use default\n", name);
		info->iterm_ffc_hot = 800;
	}

	chr_err("%s: iterm_ffc=%d,%d,%d,%d \n", __func__,
		info->iterm_ffc, info->iterm_ffc_little_warm, info->iterm_ffc_warm, info->iterm_ffc_hot);

	return ret;
}

static int parse_step_charge_config(struct mtk_charger *info, bool force_update)
{
	union power_supply_propval pval = {0,};
	bool update = false;
	int battery_cycle = BATTERY_CYCLE_1_TO_100;
	int cycle_count = 0;
	int ret = 0;

	if (!info->battery_psy)
		info->battery_psy = power_supply_get_by_name("battery");

	if (info->battery_psy) {
		ret = power_supply_get_property(info->battery_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
		if (ret)
			pval.intval = 0;
	} else {
		pval.intval = 0;
	}

	if (debug_cycle_count > 0)
		info->cycle_count = debug_cycle_count;
	else
		info->cycle_count = pval.intval;

	cycle_count = info->cycle_count;
	chr_err("%s cycle_count = %d \n", __func__, cycle_count);

	if (cycle_count <= 100) {
		battery_cycle = BATTERY_CYCLE_1_TO_100;
	} else if (cycle_count > 100 && cycle_count <= 200) {
		battery_cycle = BATTERY_CYCLE_100_TO_200;
	} else if (cycle_count > 200 && cycle_count <= 300) {
		battery_cycle = BATTERY_CYCLE_200_TO_300;
	} else if (cycle_count > 300 && cycle_count <= 400) {
		battery_cycle = BATTERY_CYCLE_300_TO_400;
	} else if (cycle_count > 400 && cycle_count <= 500) {
		battery_cycle = BATTERY_CYCLE_400_TO_500;
	} else if (cycle_count > 500 && cycle_count <= 600) {
		battery_cycle = BATTERY_CYCLE_500_TO_600;
	} else if (cycle_count > 600 && cycle_count <= 700) {
		battery_cycle = BATTERY_CYCLE_600_TO_700;
	} else if (cycle_count > 700 && cycle_count <= 800) {
		battery_cycle = BATTERY_CYCLE_700_TO_800;
	} else if (cycle_count > 800 && cycle_count <= 900) {
		battery_cycle = BATTERY_CYCLE_800_TO_900;
	} else if (cycle_count > 900 && cycle_count <= 1000) {
		battery_cycle = BATTERY_CYCLE_900_TO_1000;
	} else if (cycle_count > 1000 && cycle_count <= 1100) {
		battery_cycle = BATTERY_CYCLE_1000_TO_1100;
	} else if (cycle_count > 1100 && cycle_count <= 1200) {
		battery_cycle = BATTERY_CYCLE_1100_TO_1200;
	} else if (cycle_count > 1200 && cycle_count <= 1600) {
		battery_cycle = BATTERY_CYCLE_1200_TO_1600;
	} else {
		battery_cycle = BATTERY_CYCLE_1200_TO_1600;
	}

	if (battery_cycle != info->battery_cycle) {
		info->battery_cycle = battery_cycle;
		update = true;
	}

	chr_err("%s: battery_cycle=%d, cycle_count=%d,%d,%d, update=%d,%d\n", __func__,
		battery_cycle, cycle_count, info->cycle_count, debug_cycle_count, update, force_update);

	if (update || force_update) {
		parse_battery_cycle_dts(info, battery_cycle);
	}

	return ret;
}

void reset_step_jeita_charge(struct mtk_charger *info)
{
	get_index(info->step_chg_cfg, info->step_fallback_hyst, info->step_forward_hyst, info->vbat_now, info->step_chg_index, true);
	info->step_chg_fcc = info->step_chg_cfg[info->step_chg_index[0]].value;
	vote(info->fcc_votable, STEP_CHARGE_VOTER, true, info->step_chg_fcc);

	get_index(info->jeita_fv_cfg, info->jeita_fallback_hyst, info->jeita_forward_hyst, info->temp_now, info->jeita_chg_index, true);
	if (info->vbat_now < info->jeita_fcc_cfg[info->jeita_chg_index[0]].extra_threshold)
		info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].low_value;
	else
		info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].high_value;

	if ((info->temp_now < -100) || (info->temp_now > 580)) {
		info->jeita_chg_fcc = 0;
		chr_err("[%s]: temp_now:%d, not charging\n", __func__, info->temp_now);
	}

	vote(info->fcc_votable, JEITA_CHARGE_VOTER, true, info->jeita_chg_fcc);
	chr_err(" fcc:%d,low_value:%d,high_value:%d",info->jeita_chg_fcc,
	info->jeita_fcc_cfg[info->jeita_chg_index[0]].low_value, info->jeita_fcc_cfg[info->jeita_chg_index[0]].high_value);

	parse_step_charge_config(info, false);
}

void update_step_jeita_fcc(int batt_id)
{
	struct power_supply *psy;
	static struct mtk_charger *info;
	int i = 0;

	if (info == NULL) {
		psy = power_supply_get_by_name("mtk-master-charger");
		if (psy == NULL) {
			pr_err("%s charger is not init\n", __func__);
			return;
		} else {
			info =
			(struct mtk_charger *)power_supply_get_drvdata(psy);
		}
	}

	if (info != NULL) {
		if (batt_id == SWD_5500MAH || batt_id == NVT_5500MAH) {
			memset(info->jeita_fcc_cfg, 0, sizeof(info->jeita_fcc_cfg));
			memcpy(info->jeita_fcc_cfg, info->jeita_fcc_cfg_cn, sizeof(info->jeita_fcc_cfg));
			for (i = 0; i < STEP_JEITA_TUPLE_COUNT; i++)
				chr_err("CN:JEITA_FCC %d %d %d %d %d\n",
					info->jeita_fcc_cfg[i].low_threshold, info->jeita_fcc_cfg[i].high_threshold,
					info->jeita_fcc_cfg[i].extra_threshold, info->jeita_fcc_cfg[i].low_value,
					info->jeita_fcc_cfg[i].high_value);
		} else {
			memset(info->jeita_fcc_cfg, 0, sizeof(info->jeita_fcc_cfg));
			memcpy(info->jeita_fcc_cfg, info->jeita_fcc_cfg_gl, sizeof(info->jeita_fcc_cfg));
			for (i = 0; i < STEP_JEITA_TUPLE_COUNT; i++)
				chr_err("GL:JEITA_FCC %d %d %d %d %d\n",
					info->jeita_fcc_cfg[i].low_threshold, info->jeita_fcc_cfg[i].high_threshold,
					info->jeita_fcc_cfg[i].extra_threshold, info->jeita_fcc_cfg[i].low_value,
					info->jeita_fcc_cfg[i].high_value);
		}

		if (info->batt_id != batt_id) {
			info->batt_id = batt_id;
			parse_step_charge_config(info, true);
		}
	}
}
EXPORT_SYMBOL(update_step_jeita_fcc);

int step_jeita_init(struct mtk_charger *info, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int total_length = 0, i = 0, ret = 0;
	int lt = 0,lc = 0;

	if (!np) {
		chr_err("no device node\n");
		return -EINVAL;
	}
	total_length = of_property_count_elems_of_size(np, "thermal_limit_dcp", sizeof(u32));
	if (total_length < 0) {
		chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "thermal_limit_dcp", (u32 *)(info->thermal_limit[0]), total_length);
	if (ret) {
		chr_err("failed to parse thermal_limit_dcp\n");
		return ret;
	}

	for (i = 0; i < THERMAL_LIMIT_COUNT; i++) {
		chr_info("thermal_limit_dcp %d\n", info->thermal_limit[0][i]);
		if (info->thermal_limit[0][i] > MAX_THERMAL_FCC || info->thermal_limit[0][i] < MIN_THERMAL_FCC) {
			chr_err("thermal_limit_dcp over range\n");
			return -1;
		}
		if (i != 0) {
			if (info->thermal_limit[0][i] > info->thermal_limit[0][i - 1]) {
				chr_err("thermal_limit_dcp order error\n");
				return -1;
			}
		}
	}

	total_length = of_property_count_elems_of_size(np, "thermal_limit_qc2", sizeof(u32));
	if (total_length < 0) {
		chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "thermal_limit_qc2", (u32 *)(info->thermal_limit[1]), total_length);
	if (ret) {
		chr_err("failed to parse thermal_limit_qc2\n");
		return ret;
	}

	for (i = 0; i < THERMAL_LIMIT_COUNT; i++) {
		chr_info("thermal_limit_qc2 %d\n", info->thermal_limit[1][i]);
		if (info->thermal_limit[1][i] > MAX_THERMAL_FCC || info->thermal_limit[1][i] < MIN_THERMAL_FCC) {
			chr_err("thermal_limit_qc2 over range\n");
			return -1;
		}
		if (i != 0) {
			if (info->thermal_limit[1][i] > info->thermal_limit[1][i - 1]) {
				chr_err("thermal_limit_qc2 order error\n");
				return -1;
			}
		}
	}

	total_length = of_property_count_elems_of_size(np, "thermal_limit_qc3_18w", sizeof(u32));
	if (total_length < 0) {
		chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "thermal_limit_qc3_18w", (u32 *)(info->thermal_limit[2]), total_length);
	if (ret) {
		chr_err("failed to parse thermal_limit_qc3_18w\n");
		return ret;
	}

	for (i = 0; i < THERMAL_LIMIT_COUNT; i++) {
		chr_info("thermal_limit_qc3_18w %d\n", info->thermal_limit[2][i]);
		if (info->thermal_limit[2][i] > MAX_THERMAL_FCC || info->thermal_limit[2][i] < MIN_THERMAL_FCC) {
			chr_err("thermal_limit_qc3_18w over range\n");
			return -1;
		}
		if (i != 0) {
			if (info->thermal_limit[2][i] > info->thermal_limit[2][i - 1]) {
				chr_err("thermal_limit_qc3_18w order error\n");
				return -1;
			}
		}
	}

	total_length = of_property_count_elems_of_size(np, "thermal_limit_qc3_27w", sizeof(u32));
	if (total_length < 0) {
		chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "thermal_limit_qc3_27w", (u32 *)(info->thermal_limit[3]), total_length);
	if (ret) {
		chr_err("failed to parse thermal_limit_qc3_27w\n");
		return ret;
	}

	for (i = 0; i < THERMAL_LIMIT_COUNT; i++) {
		chr_info("thermal_limit_qc3_27w %d\n", info->thermal_limit[3][i]);
		if (info->thermal_limit[3][i] > MAX_THERMAL_FCC || info->thermal_limit[3][i] < MIN_THERMAL_FCC) {
			chr_err("thermal_limit_qc3_27w over range\n");
			return -1;
		}
		if (i != 0) {
			if (info->thermal_limit[3][i] > info->thermal_limit[3][i - 1]) {
				chr_err("thermal_limit_qc3_27w order error\n");
				return -1;
			}
		}
	}

	total_length = of_property_count_elems_of_size(np, "thermal_limit_qc35", sizeof(u32));
	if (total_length < 0) {
		chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "thermal_limit_qc35", (u32 *)(info->thermal_limit[4]), total_length);
	if (ret) {
		chr_err("failed to parse thermal_limit_qc35\n");
		return ret;
	}

	for (i = 0; i < THERMAL_LIMIT_COUNT; i++) {
		chr_info("thermal_limit_qc35 %d\n", info->thermal_limit[4][i]);
		if (info->thermal_limit[4][i] > MAX_THERMAL_FCC || info->thermal_limit[4][i] < MIN_THERMAL_FCC) {
			chr_err("thermal_limit_qc35 over range\n");
			return -1;
		}
		if (i != 0) {
			if (info->thermal_limit[4][i] > info->thermal_limit[4][i - 1]) {
				chr_err("thermal_limit_qc35 order error\n");
				return -1;
			}
		}
	}

	total_length = of_property_count_elems_of_size(np, "thermal_limit_pd", sizeof(u32));
	if (total_length < 0) {
		chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "thermal_limit_pd", (u32 *)(info->thermal_limit[5]), total_length);
	if (ret) {
		chr_err("failed to parse thermal_limit_pd\n");
		return ret;
	}

	for (i = 0; i < THERMAL_LIMIT_COUNT; i++) {
		chr_info("thermal_limit_pd %d\n", info->thermal_limit[5][i]);
		if (info->thermal_limit[5][i] > MAX_THERMAL_FCC || info->thermal_limit[5][i] < MIN_THERMAL_FCC) {
			chr_err("thermal_limit_pd over range\n");
			return -1;
		}
		if (i != 0) {
			if (info->thermal_limit[5][i] > info->thermal_limit[5][i - 1]) {
				chr_err("thermal_limit_pd order error\n");
				return -1;
			}
		}
	}

	lt = of_property_count_elems_of_size(np, "smart_chg_lowfast_thermal", sizeof(u32));
	if (lt < 0) {
	chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}

	lc = of_property_count_elems_of_size(np, "smart_chg_lowfast_current", sizeof(u32));
	if (lc < 0) {
		chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}

	info->lowfast_array_length = (lt >= lc) ? lt : lc;
	info->low_fast_thermal = (s32*) devm_kzalloc(dev, info->lowfast_array_length * sizeof(s32), GFP_KERNEL);
	info->low_fast_current = (s32*) devm_kzalloc(dev, info->lowfast_array_length * sizeof(s32), GFP_KERNEL);
	ret = of_property_read_u32_array(np, "smart_chg_lowfast_thermal", (s32 *)(info->low_fast_thermal), lt);
	if (ret) {
		chr_err("failed to parse smart_chg_lowfast_thermal\n");
		return ret;
	}

	ret = of_property_read_u32_array(np, "smart_chg_lowfast_current", (s32 *)(info->low_fast_current), lc);
	if (ret) {
		chr_err("failed to parse smart_chg_lowfast_current\n");
		return ret;
	}

	info->cycle_count = 0;
	ret = parse_step_charge_config(info, true);
	if (ret) {
		chr_err("failed to parse step_chg_cfg\n");
		return ret;
	}

	total_length = of_property_count_elems_of_size(np, "jeita_fcc_cfg_gl", sizeof(u32));
	if (total_length < 0) {
		chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "jeita_fcc_cfg_gl", (u32 *)info->jeita_fcc_cfg_gl, total_length);
	if (ret) {
		chr_err("failed to parse jeita_fcc_cfg_gl\n");
		return ret;
	}

	for (i = 0; i < STEP_JEITA_TUPLE_COUNT; i++)
		chr_err("GL:JEITA_FCC %d %d %d %d %d\n",
			info->jeita_fcc_cfg_gl[i].low_threshold, info->jeita_fcc_cfg_gl[i].high_threshold,
			info->jeita_fcc_cfg_gl[i].extra_threshold, info->jeita_fcc_cfg_gl[i].low_value,
			info->jeita_fcc_cfg_gl[i].high_value);

	total_length = of_property_count_elems_of_size(np, "jeita_fcc_cfg_cn", sizeof(u32));
	if (total_length < 0) {
		chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "jeita_fcc_cfg_cn", (u32 *)info->jeita_fcc_cfg_cn, total_length);
	if (ret) {
		chr_err("failed to parse jeita_fcc_cfg_cn\n");
		return ret;
	}

	for (i = 0; i < STEP_JEITA_TUPLE_COUNT; i++)
		chr_err("CN:JEITA_FCC %d %d %d %d %d\n",
			info->jeita_fcc_cfg_cn[i].low_threshold, info->jeita_fcc_cfg_cn[i].high_threshold,
			info->jeita_fcc_cfg_cn[i].extra_threshold, info->jeita_fcc_cfg_cn[i].low_value,
			info->jeita_fcc_cfg_cn[i].high_value);

	memcpy(info->jeita_fcc_cfg, info->jeita_fcc_cfg_gl, sizeof(info->jeita_fcc_cfg));
	for (i = 0; i < STEP_JEITA_TUPLE_COUNT; i++)
		chr_info("JEITA_FCC %d %d %d %d %d\n",
			info->jeita_fcc_cfg[i].low_threshold, info->jeita_fcc_cfg[i].high_threshold,
			info->jeita_fcc_cfg[i].extra_threshold, info->jeita_fcc_cfg[i].low_value,
			info->jeita_fcc_cfg[i].high_value);

	total_length = of_property_count_elems_of_size(np, "jeita_fv_cfg", sizeof(u32));
	if (total_length < 0) {
		chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "jeita_fv_cfg", (u32 *)info->jeita_fv_cfg, total_length);
	if (ret) {
		chr_err("failed to parse jeita_fv_cfg\n");
		return ret;
	}

	for (i = 0; i < STEP_JEITA_TUPLE_COUNT; i++)
		chr_info("JEITA_FV %d %d %d\n", info->jeita_fv_cfg[i].low_threshold, info->jeita_fv_cfg[i].high_threshold, info->jeita_fv_cfg[i].value);

	ret = of_property_read_u32(np, "step_fallback_hyst", &info->step_fallback_hyst);
	if (ret) {
		chr_err("failed to parse step_fallback_hyst\n");
		return ret;
	}

	ret = of_property_read_u32(np, "step_forward_hyst", &info->step_forward_hyst);
	if (ret) {
		chr_err("failed to parse step_forward_hyst\n");
		return ret;
	}

	ret = of_property_read_u32(np, "jeita_fallback_hyst", &info->jeita_fallback_hyst);
	if (ret) {
		chr_err("failed to parse jeita_fallback_hyst\n");
		return ret;
	}

	ret = of_property_read_u32(np, "jeita_forward_hyst", &info->jeita_forward_hyst);
	if (ret) {
		chr_err("failed to parse jeita_forward_hyst\n");
		return ret;
	}

	info->vbus_contral = devm_regulator_get(dev, "vbus_control");
	if (IS_ERR(info->vbus_contral))
		chr_err("failed to get vbus contral\n");

	info->mt6369_moscon1_control = of_property_read_bool(np, "mt6369_moscon1_control");
	if (info->mt6369_moscon1_control)
	{
		chr_err("successed to parse mt6369_moscon1_control\n");
	}

	info->burn_control_gpio = of_get_named_gpio(np, "burn_control_gpio", 0);
	if (!gpio_is_valid(info->burn_control_gpio)) {
		chr_err("failed to parse burn_control_gpio\n");
	}

	info->smart_snschg_support =  of_property_read_bool(np, "smart_sensing_chg_support");
	if (!info->smart_snschg_support)
		chr_err("failed to parse smart_snschg_support\n");

	info->sic_support =  of_property_read_bool(np, "sic_support");
	if (!info->sic_support)
		chr_err("failed to parse sic_support\n");


	if (gpio_is_valid(info->burn_control_gpio)) {
		gpio_direction_output(info->burn_control_gpio, 0);
	}
	INIT_DELAYED_WORK(&info->charge_monitor_work, charge_monitor_func);
	INIT_DELAYED_WORK(&info->usb_otg_monitor_work, monitor_usb_otg_burn);
	INIT_DELAYED_WORK(&info->typec_burn_monitor_work, monitor_typec_burn);
#if defined(CONFIG_RUST_DETECTION)
	INIT_DELAYED_WORK(&info->rust_detection_work, rust_detection_work_func);
#endif

	return ret;
}