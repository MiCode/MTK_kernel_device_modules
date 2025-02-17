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
#include "bq28z610.h"
#include "mtk_battery.h"
#include "pd_cp_manager.h"

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

static int calc_delta_time(ktime_t time_last, s64 *delta_time)
{
	ktime_t time_now;

	time_now = ktime_get();

	*delta_time = ktime_ms_delta(time_now, time_last);
	if (*delta_time < 0)
		*delta_time = 0;

	return 0;
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
		if ((-ibat) <= info->step_chg_fcc) {
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

static int typec_connect_ntc_set_vbus(struct mtk_charger *info, bool is_on)
{
	struct regulator *vbus = info->vbus_contral;
	int ret = 0, vbus_vol = 3300000;

	/* vbus is optional */
	if (!vbus)
		return 0;

	chr_err("contral vbus turn %s\n", is_on ? "on" : "off");

	if (is_on) {
		ret = regulator_set_voltage(vbus, vbus_vol, vbus_vol);
		if (ret)
			chr_err("vbus regulator set voltage failed\n");

		ret = regulator_enable(vbus);
		if (ret)
			chr_err("vbus regulator enable failed\n");
	} else {
		ret = regulator_disable(vbus);
		if (ret)
			chr_err("vbus regulator disable failed\n");
	}

	return 0;
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

#define HWID_N12A_P11	0x110002
#define HWID_N12A_P20	0x120000
#define HWID_N12A_P21	0x120001
#define HWID_N12A_MP	0x190000

static bool enable_anti_burn_confirm(void)
{
	return true;
}

static void monitor_typec_acceleration_of_con_temp(int con_temp, int* con_temp_acceleration)
{
	static ktime_t last_time = 0;
	static int last_connector_temp = 0;
	static bool first_boot_flag = true;
	ktime_t current_time, elapsed_time;
	int temp_interval = 0;

	if (first_boot_flag) {
		last_time = ktime_get();
		last_connector_temp = con_temp;
		first_boot_flag = false;
		*con_temp_acceleration = 0;
		return ;
	}

	current_time = ktime_get();
	elapsed_time = ktime_ms_delta(current_time, last_time);
	temp_interval = con_temp - last_connector_temp;
	chr_err("elapsed_time is %lld, temp_interval is %d, current time is %lld, last time is %lld\n",
		elapsed_time, temp_interval, current_time, last_time);
	if (elapsed_time <= 0) {
		chr_err("timestamp is error, elapsed_time is %lld, current time is %lld, last time is %lld\n",
			elapsed_time, current_time, last_time);
		*con_temp_acceleration = 0;
		last_time = current_time;
		return ;
	} else if (elapsed_time > 1000) {
		last_connector_temp = con_temp;
	}
	// read value of temp is 3 digits, 36.5 degree is represented as 365,
	// time unit is ms, so divided by 10 and multiple 1k
	// con_temp_acceleration will be negative when temp down fast
	*con_temp_acceleration = (temp_interval / 10 * 1000) / elapsed_time;
	last_time = current_time;
	chr_err("temp acceleration is %d\n", *con_temp_acceleration);
}

static void monitor_usb_otg_burn(struct work_struct *work)
{
	int otg_monitor_delay_time = 5000;
	struct mtk_charger *info = container_of(work, struct mtk_charger, usb_otg_monitor_work.work);
	int type_temp = 0, pmic_vbus = 0;
	int sub_temp = 0;
	int con_temp_acceleration = 0;
	int fitting_board_temp = 0;
	usb_get_property(USB_PROP_CONNECTOR_TEMP, &type_temp);
	usb_get_property(USB_PROP_SUB_CONNECTOR_TEMP, &sub_temp);
	usb_get_property(USB_PROP_PMIC_VBUS, &pmic_vbus);
	usb_get_property(USB_PROP_BOARD_TEMP, &fitting_board_temp);

	if(type_temp <= 450)
		otg_monitor_delay_time = 5000;
	else if(type_temp > 450 && type_temp <= 550)
		otg_monitor_delay_time = 2000;
	else if(type_temp > 550)
		otg_monitor_delay_time = 1000;
	chr_err("%s:get typec temp =%d sub_temp=%d otg_monitor_delay_time = %d\n",
		__func__, type_temp, sub_temp, otg_monitor_delay_time);
	if (sub_temp > type_temp) {
		type_temp = sub_temp;
	}
	monitor_typec_acceleration_of_con_temp(type_temp, &con_temp_acceleration);
	if(((type_temp >= TYPEC_NTC_TEMP_COMBINED_THRESHOLD && fitting_board_temp <= TYPEC_NTC_TEMP_FITTING_THRESHOLD) ||
		(con_temp_acceleration >= TYPEC_NTC_TEMP_ACCELERATION && type_temp >= TYPEC_NTC_TEMP_LOW_THRESHOLD) ||
		type_temp >= TYPEC_BURN_TEMP_THRESHOLD) && !info->typec_otg_burn &&
		enable_anti_burn_confirm()) {
		info->typec_otg_burn = true;
		info->typec_otg_burn_report_status = true;
		usbotg_enable_otg(info, false);
		chr_err("%s:disable otg\n", __func__);
		charger_dev_cp_reset_check(info->cp_master);
		update_connect_temp(info);
	}
	else if(con_temp_acceleration < TYPEC_NTC_TEMP_ACCELERATION && info->wd0_burn_status == false &&
		((info->typec_otg_burn && type_temp <= TYPEC_NTC_TEMP_RECOVER_THRESHOLD) ||
		(pmic_vbus < 1000 && info->otg_enable && type_temp <= TYPEC_NTC_TEMP_RECOVER_THRESHOLD))) {

		info->typec_otg_burn = false;
		info->typec_otg_burn_report_status = false;
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
	disable_uart_manual(true);
	charger_dev_rust_detection_init(info->et7480_chg_dev);

	for (i=1; i<=RUST_DET_SBU2_PIN; i++) {
		if (info->typec_attach) {
			disable_uart_manual(false);
			return;
		}
		monitor_usb_rust(info, i);
	}
	disable_uart_manual(false);
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
	int type_temp = 0, retry_count = 2;
	int sub_temp = 0;
	bool cp_master_enable = false;
	int typec_burn_delay_time = 5000;
	int con_temp_acceleration = 0;
	int fitting_board_temp = 0;
	usb_get_property(USB_PROP_CONNECTOR_TEMP, &type_temp);
	usb_get_property(USB_PROP_SUB_CONNECTOR_TEMP, &sub_temp);
	usb_get_property(USB_PROP_BOARD_TEMP, &fitting_board_temp);
	chr_err("%s typec_temp=%d-%d fit_temp=%d, otg_enable=%d, status:%d,%d,%d,%d\n",
                __func__, type_temp, sub_temp, fitting_board_temp, info->otg_enable, info->typec_burn,
		info->typec_burn_status, con_temp_acceleration, info->wd0_burn_status);
	if (sub_temp > type_temp) {
		type_temp = sub_temp;
	}
	monitor_typec_acceleration_of_con_temp(type_temp, &con_temp_acceleration);

#ifdef CONFIG_FACTORY_BUILD
	typec_burn_delay_time = 1000;
#else
	if(type_temp <= 450)
		typec_burn_delay_time = 5000;
	else if(type_temp > 450 && type_temp <= 550)
		typec_burn_delay_time = 2000;
	else if(type_temp > 550)
		typec_burn_delay_time = 1000;
#endif

	if (((type_temp >= TYPEC_NTC_TEMP_COMBINED_THRESHOLD && fitting_board_temp != 0 && fitting_board_temp <= TYPEC_NTC_TEMP_FITTING_THRESHOLD) ||
		(con_temp_acceleration >= TYPEC_NTC_TEMP_ACCELERATION && type_temp >= TYPEC_NTC_TEMP_LOW_THRESHOLD) ||
		type_temp > TYPEC_BURN_TEMP_THRESHOLD) && !info->typec_burn_status &&
		!info->otg_enable && enable_anti_burn_confirm()) {
		chr_err("%s: trigger anti burn now, temp=%d fit_temp=%d\n", __func__, type_temp, fitting_board_temp);	
		update_connect_temp(info);
		info->typec_burn = true;
		info->typec_burn_report_status = true;
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
			charger_set_dpdm_voltage(info, 600, 0);

		usb_set_property(USB_PROP_INPUT_SUSPEND, info->typec_burn);
		vote(info->icl_votable, TYPEC_BURN_VOTER, true, 0);
		info->typec_burn_status = true;
		msleep(1000);
		if (info->mt6368_moscon1_control) {
			mtk_set_mt6368_moscon1(info, 1, 1);
			chr_err("mt6368_moscon1_control set high\n");
		} else if (gpio_is_valid(info->burn_control_gpio)) {
			gpio_set_value(info->burn_control_gpio, 1);
			chr_err("burn_control_gpio gpio set high\n");
		} else {
			typec_connect_ntc_set_vbus(info, true);
		}
#ifdef CONFIG_FACTORY_BUILD
	} else if (info->typec_burn && type_temp <= TYPEC_NTC_TEMP_RECOVER_THRESHOLD &&
		info->typec_burn_status && con_temp_acceleration < TYPEC_NTC_TEMP_ACCELERATION) {
#else
	} else if (info->typec_burn && type_temp <= TYPEC_NTC_TEMP_RECOVER_THRESHOLD &&
		info->typec_burn_status && con_temp_acceleration < TYPEC_NTC_TEMP_ACCELERATION && info->wd0_burn_status == false) {
#endif
		chr_err("%s: anti burn recover now\n", __func__);
		info->typec_burn = false;
		info->typec_burn_report_status = false;
		if (info->mt6368_moscon1_control) {
			mtk_set_mt6368_moscon1(info, 0, 0);
			chr_err("mt6368_moscon1_control set low\n");
		} else if (gpio_is_valid(info->burn_control_gpio)) {
			gpio_set_value(info->burn_control_gpio, 0);
			chr_err("burn_control_gpio gpio set low\n");
		} else {
			typec_connect_ntc_set_vbus(info, false);
		}
		usb_set_property(USB_PROP_INPUT_SUSPEND, info->typec_burn);
		vote(info->icl_votable, TYPEC_BURN_VOTER, false, 0);
		info->typec_burn_status = false;
	}
	if(info->plugged_status || info->typec_burn || info->typec_otp)
		schedule_delayed_work(&info->typec_burn_monitor_work, msecs_to_jiffies(typec_burn_delay_time));
}

static void handle_jeita_charge(struct mtk_charger *info)
{
	static bool jeita_vbat_low = true;

	get_index(info->jeita_fv_cfg, info->jeita_fallback_hyst, info->jeita_forward_hyst, info->temp_now, info->jeita_chg_index, false);

	vote(info->fv_votable, JEITA_CHARGE_VOTER, true, info->jeita_fv_cfg[info->jeita_chg_index[0]].value  - info->diff_fv_val + info->pmic_comp_v);

	if(info->jeita_chg_index[0] == STEP_JEITA_TUPLE_COUNT-2 && info->jeita_chg_index[1] == STEP_JEITA_TUPLE_COUNT - 1)
	{
		info->high_temp_rec_soc = info->soc;
		chr_err("high temp recharging high_temp_rec_soc=%d\n",  info->high_temp_rec_soc);
	}

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
		info->jeita_chg_fcc = info->jeita_chg_fcc * 8 / 10 - 300;
		chr_err("[%s]: soa or isp induce jeita current drop: %d\n", __func__, info->jeita_chg_fcc);
	}

	if (is_between(info->jeita_fcc_cfg[0].low_threshold, info->jeita_fcc_cfg[STEP_JEITA_TUPLE_COUNT - 1].high_threshold, info->temp_now) && !info->input_suspend && !info->typec_burn && info->bms_i2c_error_count < 10)
        chr_err("handle_jeita_charge index = %d,jeita_chg_fcc = %d", info->jeita_chg_index[0], info->jeita_chg_fcc);

	if(info->bms_i2c_error_count >= 10)
	{
		vote_override(info->fcc_votable, FG_I2C_VOTER, true, 500);
		vote_override(info->icl_votable, FG_I2C_VOTER, true, 500);
		adapter_dev_set_cap_xm(info->pd_adapter, MTK_PD_APDO, 5000, 1000);
	}
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

static void monitor_thermal_limit(struct mtk_charger *info)
{
	int thermal_level = 0;
	//int iterm_effective = get_effective_result(info->iterm_votable);
	if (info->thermal_level < 0)
		thermal_level = -1 - info->thermal_level;
	else {
			thermal_level = info->thermal_level;
	}

	if (info->smart_snschg_support && info->smart_snschg_nonsic_support
			&& info->smart_chg[SMART_CHG_SENSING_CHG].active_status
			&& (info->smart_therm_lvl > LVL_NO_ADJUST && info->smart_therm_lvl < LVL_INVALID_ADJUST)) {
		if (info->smart_therm_lvl > LVL_INCREASE_REF) {
			if (info->smart_therm_lvl == LVL_INCREASE_MAX) {
				thermal_level = 13;
			} else {
				thermal_level += (info->smart_therm_lvl - LVL_INCREASE_REF);
				thermal_level = (thermal_level >= THERMAL_LIMIT_COUNT)? (THERMAL_LIMIT_COUNT-1) : thermal_level;
			}
		} else {
			if (info->smart_therm_lvl == LVL_DECREASE_MAX) {
				thermal_level = 0;
			} else {
				thermal_level -= (info->smart_therm_lvl - LVL_DECREASE_REF);
				thermal_level = (thermal_level < 0)? 0 : thermal_level;
			}
		}
		chr_err("SmartSensingChg trigger, adjust thermal level from %d to %d\n", info->thermal_level, thermal_level);
	}

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
		chr_err("not support psy_type to check charger parameters");
	}
	// if (info->last_thermal_level == 15 && thermal_level < 15) {
	// 	chr_err("[CHARGE_LOOP] disable TE\n");
	// 	charger_dev_enable_termination(info->chg1_dev, false);
	// 	info->disable_te_count = 0;
	// }
	// if(abs(info->current_now) <= (iterm_effective + 150))
	// 	info->disable_te_count ++;
	// else
	// 	info->disable_te_count = 0;
	// chr_err("[CHARGE_LOOP] disable_te_count = %d\n", info->disable_te_count);
	// if(info->disable_te_count == 5)
	// {
	// 	chr_err("[CHARGE_LOOP] enable TE\n");
	// 	charger_dev_enable_termination(info->chg1_dev, true);
	// }
	// info->last_thermal_level = thermal_level;
}

static int handle_ffc_charge(struct mtk_charger *info)
{
	int ret = 0, val = 0, iterm_ffc = 0, iterm = 0, raw_soc = 0;
	if(info->bms_psy){
		bms_get_property(BMS_PROP_CHARGE_DONE, &val);
		if (val != info->fg_full) {
			info->fg_full = val;
			power_supply_changed(info->psy1);
		}
	} else {
		chr_err("get ti_gauge bms failed\n");
		return 0;
	}

	if (info->cycle_count >= 100 && info->cycle_count <= 200) {
		info->ffc_high_soc = 92;
	} else if (info->cycle_count > 200) {
		info->ffc_high_soc = 90;
	} else {
		/* do nothing*/
	}
	
	if(info->switch_pmic_normal){
		info->ffc_enable = false;
	}else if ((!info->charge_full && !info->recharge && info->entry_soc <= info->ffc_high_soc && is_between(info->ffc_low_tbat, info->ffc_high_tbat, info->temp_now) &&
			info->high_temp_rec_soc <= info->ffc_high_soc &&
			(info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO && info->pd_adapter->verifed) &&
			(info->thermal_level <= ENABLE_FCC_ITERM_LEVEL) && info->jeita_chg_index[0] == 4) || info->suspend_recovery)
		info->ffc_enable = true;
	else
		info->ffc_enable = false;

	if (info->temp_now >= info->ffc_high_tbat )
		iterm = info->iterm_warm;
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

	bms_get_property(BMS_PROP_CAPACITY_RAW, &raw_soc);
	if (info->ffc_enable) {
		vote(info->fv_votable, FFC_VOTER, true, info->fv_ffc - info->diff_fv_val + info->pmic_comp_v);
		vote(info->iterm_votable, FFC_VOTER, true, iterm_ffc - 100);
		val = true;
		bms_set_property(BMS_PROP_FASTCHARGE_MODE, val);
		val = FG_MONITOR_DELAY_5S;
		bms_set_property(BMS_PROP_MONITOR_DELAY, val);
	} else if (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
		if (info->jeita_chg_index[0] == 4) {
			vote(info->fv_votable, FFC_VOTER, true, info->fv_normal  - info->diff_fv_val + info->pmic_comp_v);
		} else {
			vote(info->fv_votable, FFC_VOTER, true, info->fv  - info->diff_fv_val + info->pmic_comp_v);
		}
		vote(info->iterm_votable, FFC_VOTER, true, iterm);
		val = false;
		bms_set_property(BMS_PROP_FASTCHARGE_MODE, val);
		val = FG_MONITOR_DELAY_5S;
		bms_set_property(BMS_PROP_MONITOR_DELAY, val);
	} else {
		if (info->jeita_chg_index[0] == 4) {
			vote(info->fv_votable, FFC_VOTER, true, info->fv_normal  - info->diff_fv_val + info->pmic_comp_v);
		} else {
			vote(info->fv_votable, FFC_VOTER, true, info->fv  - info->diff_fv_val + info->pmic_comp_v);
		}
		vote(info->iterm_votable, FFC_VOTER, true, iterm);
		val = false;
		bms_set_property(BMS_PROP_FASTCHARGE_MODE, val);
		val = FG_MONITOR_DELAY_30S;
		bms_set_property(BMS_PROP_MONITOR_DELAY, val);
	}

	return ret;
}

static void check_full_recharge(struct mtk_charger *info)
{
	static int full_count = 0, recharge_count = 0, iterm = 0, threshold_mv = 0, eoc_stat_count = 0;
	static int eoc_count = 0;
	int iterm_effective = get_effective_result(info->iterm_votable);
	int fv_effective = get_effective_result(info->fv_votable) - info->pmic_comp_v;
	int raw_soc = 0, ret = 0, pmic_vbus = 0;
	u32 stat;
	bool is_masterCpEn = false;
	static ktime_t last_time = -1;
	s64 delta_time = 0;
	bool recharge_time_flag = false;

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
		if (info->temp_now >= info->ffc_high_tbat)
			iterm = info->iterm_warm;
		else
			iterm = info->iterm;
	}

	chr_err("%s diff_fv_val = %d, iterm = %d, iterm_effective = %d, fv_effective = %d, full_count = %d, recharge_count = %d, eoc_count=%d, sue=%d, time=%lld,%lld\n", __func__,
		info->diff_fv_val, iterm, iterm_effective, fv_effective, full_count, recharge_count, eoc_count,
		info->supplementary_electricity, last_time, info->sue_start_time);

	// susplementary electricity
	if (!info->supplementary_electricity) {
		calc_delta_time(last_time, &delta_time);
		if (info->last_ffc_enable && info->charge_full && is_between(info->ffc_low_tbat, info->ffc_high_tbat, info->temp_now)) {
			if (delta_time >= 5*60*1000) {
				recharge_time_flag = true;
				info->supplementary_electricity = true;
				chr_err("start recharge, recharge_time_flag = %d\n", recharge_time_flag);
			}
		} else {
			last_time = ktime_get();
		}
	}

	if (info->charge_full) {
		full_count = 0;
		eoc_count = 0;
		if (info->vbat_now <= fv_effective - 150)
			recharge_count++;
		else
			recharge_count = 0;

		bms_get_property(BMS_PROP_CAPACITY_RAW, &raw_soc);

		if ((((recharge_count >= 15) || (raw_soc < 9800) || !info->real_full) && (info->jeita_chg_index[0] <= 4)) || recharge_time_flag) {
			info->charge_full = false;
			info->charge_eoc = false;
                        info->warm_term = false;
			info->last_ffc_enable = false;

			chr_err("start recharge, recharge_time_flag = %d\n", recharge_time_flag);
			if (recharge_time_flag) {
				recharge_time_flag = false;
				info->sue_start_time = ktime_get();
			} else {
				info->sue_start_time = -1;
			}

			if(info->real_full){
				info->recharge = true;
				info->real_full = false;
			}
			recharge_count = 0;
			if(info->bms_i2c_error_count < 10)
				charger_dev_enable(info->chg1_dev, true);
			power_supply_changed(info->psy1);
		}
	} else {
		recharge_count = 0;

		if (info->ffc_enable) {
			if (info->temp_now > 0)
				threshold_mv = 40;
			else
				threshold_mv = 100;
		} else {
			if (info->temp_now > 15) {
				threshold_mv = 20;
			} else if (info->temp_now > 0) {
				threshold_mv = 35;
			} else {
				threshold_mv = 100;
			}
		}

		if ((info->vbat_now >= fv_effective - threshold_mv) &&
				((-info->current_now <= iterm) || ((-info->current_now <= iterm_effective + 100) && info->bbc_charge_done))) {
			full_count++;
		} else {
			full_count = 0;
		}

		if (full_count >= 6 && !info->charge_eoc) {
			full_count = 0;
			info->charge_eoc = true;
			chr_err("charge_full notify gauge eoc\n");
			if (info->ffc_enable) {
				bms_set_property(BMS_PROP_CHARGE_EOC, true);
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
			if (info->ffc_enable) {
				info->last_ffc_enable = true;
				chr_err("report charge full: last_ffc_enable = %d \n", info->last_ffc_enable);
			}
			if(info->jeita_chg_index[0] <= 4) {
				info->real_full = true;
				info->warm_term = false;
				charger_dev_do_event(info->chg1_dev, EVENT_FULL, 0);
				power_supply_changed(info->psy1);
			}
			charger_dev_enable(info->chg1_dev, false);//diable pmic
			charger_dev_enable_powerpath(info->chg1_dev, true);

			if(info->real_type == XMUSB350_TYPE_HVCHG || info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30 || info->pd_type == MTK_PD_CONNECT_PE_READY_SNK){
				int cnt = 0;
				pmic_vbus = get_vbus(info);
				if(info->real_type == XMUSB350_TYPE_HVCHG)
					charger_set_dpdm_voltage(info, 0, 0);
				else
					adapter_dev_set_cap(info->pd_adapter, MTK_PD_APDO, 5000, 3000);
				while(cnt <= 15 && pmic_vbus > 7000)
				{
					msleep(2);
					pmic_vbus = get_vbus(info);
					cnt++;
				}
				charger_dev_set_mivr(info->chg1_dev, info->data.min_charger_voltage);
				chr_err("report charge_full: set DP:0, DM:0 for setting 5V\n");
			}else{
				charger_dev_is_enabled(info->cp_master, &is_masterCpEn);
				pmic_vbus = get_vbus(info);
				chr_err("[%s]cp_master:%d, pmic_vbus:%d\n", __func__, is_masterCpEn, pmic_vbus);

				if(!is_masterCpEn && pmic_vbus > 2500){
					ret = adapter_dev_set_cap(info->pd_adapter, MTK_PD_APDO, 5000, 3000);
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

static void check_dfx_param_data(struct mtk_charger *info)
{
	union power_supply_propval pval = {0,};
	int ret = 0, val = 0;

	if (info == NULL)
		return;

	if (!info->bms_psy) {
		info->bms_psy = power_supply_get_by_name("bms");
		chr_err("failed to get bms_psy\n");
	}

	if (!info->cp_master) {
		info->cp_master = get_charger_by_name("cp_master");
		chr_err("failed to get master cp charger\n");
	}

	if (!info->cp_slave) {
		info->cp_slave = get_charger_by_name("cp_slave");
		chr_err("failed to get slave cp charger\n");
	}

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret) {
		chr_err("failed to get soc\n");
		info->soc = 15;
	} else {
		info->soc = pval.intval;
	}

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret) {
		chr_err("failed to get vbat\n");
		info->vbat_now = 3700;
	} else {
		info->vbat_now = pval.intval / 1000;
	}

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret) {
		chr_err("failed to get ibat\n");
		info->current_now = 0;
	} else {
		info->current_now = pval.intval / 1000;
	}

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret) {
		chr_err("failed to get tbat\n");
		info->temp_now = 150;
	} else {
		info->temp_now = pval.intval;
	}

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	if (ret) {
		chr_err("failed to get cyclecount\n");
		info->cycle_count = 0;
	} else {
		info->cycle_count = pval.intval;
	}

	ret = bms_get_property(BMS_PROP_AUTHENTIC, &val);
	if (ret) {
		chr_err("failed to get gauge authentic\n");
		info->gauge_authentic = 1;
	} else {
		info->gauge_authentic = val;
	}

	chr_err("soc=%d, vbat=%d, ibat=%d, tbat=%d, cycle=%d, auth=%d\n", info->soc, info->vbat_now, info->current_now, info->temp_now, info->cycle_count, info->gauge_authentic);
}

static void check_charge_data(struct mtk_charger *info)
{
	union power_supply_propval pval = {0,};
	int ret = 0, val = 0;
	struct mtk_battery_manager *bm;
	struct mtk_battery *battery_drvdata;

	// adapter register
	if (!info->pd_adapter) {
		info->pd_adapter = get_adapter_by_name("pd_adapter");
		if (!info->pd_adapter) {
			chr_err("failed to get pd_adapter\n");
			return;
		}
	}

	if (!info->bms_psy) {
		info->bms_psy = power_supply_get_by_name("bms");
		chr_err("failed to get bms_psy\n");
		return;
	}

	if (!info->battery_psy) {
		info->battery_psy = power_supply_get_by_name("battery");
		if (!info->battery_psy) {
			chr_err("failed to get battery_psy\n");
			return;
		}
	}

	if (!info->usb_psy) {
		info->usb_psy = power_supply_get_by_name("usb");
		chr_err("failed to get usb_psy\n");
		return;
	}

	if(info->battery_psy){
		bm = power_supply_get_drvdata(info->battery_psy);
		battery_drvdata = bm->gm1;
		info->smart_chg = battery_drvdata->smart_chg;
		chr_err("[XMCHG_MONITOR] set mtk_charger smart_chg done!\n");
	}

	if (!info->cp_master) {
		info->cp_master = get_charger_by_name("cp_master");
		chr_err("failed to get master cp charger\n");
		return;
	}

	if (!info->cp_slave) {
		info->cp_slave = get_charger_by_name("cp_slave");
		chr_err("failed to get slave cp charger\n");
		//return;
	}

	charger_dev_is_enabled(info->chg1_dev, &info->bbc_charge_enable);
	charger_dev_is_charging_done(info->chg1_dev, &info->bbc_charge_done);

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret)
		chr_err("failed to get soc\n");
	else
		info->soc = pval.intval;

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret)
		chr_err("failed to get vbat\n");
	else
		info->vbat_now = pval.intval / 1000;

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret)
		chr_err("failed to get ibat\n");
	else
		info->current_now = pval.intval / 1000;

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_TEMP, &pval);
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
		else
			info->thermal_level = pval.intval;
	}

	ret = bms_get_property(BMS_PROP_I2C_ERROR_COUNT, &val);
	if (ret)
		chr_err("failed to get i2c error count\n");
	else
		info->bms_i2c_error_count = val;

	ret = bms_get_property(BMS_PROP_ADAP_POWER, &val);
	if (ret)
		chr_err("failed to get adapting power\n");
	else
		info->adapting_power = val;

	ret = bms_get_property(BMS_PROP_AUTHENTIC, &val);
	if (ret)
		chr_err("failed to get gauge authentic\n");
	else
		info->gauge_authentic = val;

	ret = bms_get_property(BMS_PROP_CELL_SUPPLIER, &val);
	if (ret)
		chr_err("failed to get gauge batt_cell_supplier\n");
	else
		info->batt_cell_supplier = val;

	if ((info->night_charging || info->smart_chg[SMART_CHG_NAVIGATION].active_status) && info->supplementary_electricity) {
		info->supplementary_electricity = false;
	}

	chr_err("diff_fv_val:%d, and will clear diff_fv_val as 0\n", info->diff_fv_val);
	info->diff_fv_val = 0;

	/* FFC fv cyc */
	if (info->project_no != RODIN_GL) {
		if (info->ffc_enable)
		{
			if (info->cycle_count > 100 && info->cycle_count <= 300) {
				info->diff_fv_val =  20;
			} else if (info->cycle_count > 300 && info->cycle_count <= 800) {
				info->diff_fv_val =  30;
			} else if (info->cycle_count > 800) {
				info->diff_fv_val = 50;
			} else {
				/* do nothing*/
			}
		} else {  /* normal fv cyc */
			if (info->cycle_count > 100 && info->cycle_count <= 300) {
				info->diff_fv_val =  10;
			} else if (info->cycle_count > 300 && info->cycle_count <= 800) {
				info->diff_fv_val =  20;
			} else if (info->cycle_count > 800) {
				info->diff_fv_val = 40;
			} else {
				/* do nothing*/
			}
		}
	} else {
		if (info->ffc_enable)
		{
			if (info->cycle_count > 100 && info->cycle_count <= 300) {
				info->diff_fv_val =  20;
			} else if (info->cycle_count > 300) {
				info->diff_fv_val = 30;
			} else {
				/* do nothing*/
			}
		} else {  /* normal fv cyc */
			if (info->cycle_count > 100 && info->cycle_count <= 300) {
				info->diff_fv_val =  10;
			} else if (info->cycle_count > 300 && info->cycle_count <= 800) {
				info->diff_fv_val =  20;
			} else if (info->cycle_count > 800) {
				info->diff_fv_val = 40;
			} else {
				/* do nothing*/
			}
		}
	}

	info->diff_fv_val = max(info->diff_fv_val,  max(info->set_smart_batt_diff_fv, info->set_smart_fv_diff_fv));

	chr_err("[CHARGE_LOOP] TYPE = [%d %d %d %d], BMS = [%d %d %d %d], FULL = [%d %d %d %d %d], thermal_level=%d, FFC = %d, sw_cv=%d, gauge_authentic=%d, warm_term=%d, cell=%d, smart_batt:%d, smart_fv:%d\n",
		info->pd_type, info->pd_adapter->verifed, info->real_type, info->qc3_type, info->soc, info->vbat_now, info->current_now, info->temp_now,
		info->bbc_charge_enable, info->charge_full, info->fg_full, info->bbc_charge_done, info->recharge, info->thermal_level, info->ffc_enable,
		info->sw_cv, info->gauge_authentic, info->warm_term, info->batt_cell_supplier, info->set_smart_batt_diff_fv, info->set_smart_fv_diff_fv);
	if (info->input_suspend || info->typec_burn)
		chr_err("[CHARGE_LOOP] input_suspend = %d, typec_burn = %d\n", info->input_suspend, info->typec_burn);
}


static void update_dfx_cyclial_report(struct mtk_charger *info)
{
	bool cp_en_fail = false;
	int cp_m_fault = 0, cp_s_fault = 0;
	int cp_m_tdie = 0, cp_s_tdie = 0;
	int tconn = 0;

	if (info == NULL || info->dfx_report_info == NULL)
		return;

	if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_PD_AUTH_FAIL)) || !(info->dfx_cyclial_report_type & BIT_MASK(CHG_DFX_PD_AUTH_FAIL))) {
		if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_PD_AUTH_FAIL))
				|| (info->real_type == XMUSB350_TYPE_PD && !info->pd_verifed && info->pd_verify_done
				&& info->pd_adapter != NULL && info->pd_adapter->adapter_svid == USB_PD_MI_SVID)) {
			info->dfx_cyclial_report_type |= BIT_MASK(CHG_DFX_PD_AUTH_FAIL);
			info->dfx_fake_report_type &= ~BIT_MASK(CHG_DFX_PD_AUTH_FAIL);
			// prepare report info
			info->dfx_report_info->param_cnt[CHG_DFX_PD_AUTH_FAIL] = 2;
			info->dfx_report_info->format[CHG_DFX_PD_AUTH_FAIL][0] = "char";
			info->dfx_report_info->key[CHG_DFX_PD_AUTH_FAIL][0] = "chgErrInfo";
			info->dfx_report_info->val[CHG_DFX_PD_AUTH_FAIL][0].strval = xm_chg_dfx_report_text[CHG_DFX_PD_AUTH_FAIL];
			info->dfx_report_info->format[CHG_DFX_PD_AUTH_FAIL][1] = "int";
			info->dfx_report_info->key[CHG_DFX_PD_AUTH_FAIL][1] = "adapterId";
			info->dfx_report_info->val[CHG_DFX_PD_AUTH_FAIL][1].intval = info->pd_adapter->adapter_id;
			xm_handle_dfx_report(CHG_DFX_PD_AUTH_FAIL);
		}
	}

	if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_CP_EN_FAIL)) || !(info->dfx_cyclial_report_type & BIT_MASK(CHG_DFX_CP_EN_FAIL))) {
		if (info->cp_master)
			charger_dev_cp_get_en_fail_status(info->cp_master, &cp_en_fail);
		if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_CP_EN_FAIL)) || cp_en_fail) {
			info->dfx_cyclial_report_type |= BIT_MASK(CHG_DFX_CP_EN_FAIL);
			info->dfx_fake_report_type &= ~BIT_MASK(CHG_DFX_CP_EN_FAIL);
			// prepare report info
			info->dfx_report_info->param_cnt[CHG_DFX_CP_EN_FAIL] = 1;
			info->dfx_report_info->format[CHG_DFX_CP_EN_FAIL][0] = "char";
			info->dfx_report_info->key[CHG_DFX_CP_EN_FAIL][0] = "chgErrInfo";
			info->dfx_report_info->val[CHG_DFX_CP_EN_FAIL][0].strval = xm_chg_dfx_report_text[CHG_DFX_CP_EN_FAIL];
			xm_handle_dfx_report(CHG_DFX_CP_EN_FAIL);
		}
	}

	if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_NONE_STANDARD_CHG)) || !(info->dfx_cyclial_report_type & BIT_MASK(CHG_DFX_NONE_STANDARD_CHG))) {
		if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_NONE_STANDARD_CHG))
				|| (info->real_type == XMUSB350_TYPE_FLOAT && info->dfx_cyclial_recheck_count++ > DFX_CHG_NONE_STANDARD_RECHECK_CNT)) {
			info->dfx_cyclial_recheck_count = 0;
			info->dfx_cyclial_report_type |= BIT_MASK(CHG_DFX_NONE_STANDARD_CHG);
			info->dfx_fake_report_type &= ~BIT_MASK(CHG_DFX_NONE_STANDARD_CHG);
			// prepare report info
			info->dfx_report_info->param_cnt[CHG_DFX_NONE_STANDARD_CHG] = 1;
			info->dfx_report_info->format[CHG_DFX_NONE_STANDARD_CHG][0] = "char";
			info->dfx_report_info->key[CHG_DFX_NONE_STANDARD_CHG][0] = "chgErrInfo";
			info->dfx_report_info->val[CHG_DFX_NONE_STANDARD_CHG][0].strval = xm_chg_dfx_report_text[CHG_DFX_NONE_STANDARD_CHG];
			xm_handle_dfx_report(CHG_DFX_NONE_STANDARD_CHG);
		}
	}

	if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_CP_VBAT_OVP)) || !(info->dfx_cyclial_report_type & BIT_MASK(CHG_DFX_CP_VBAT_OVP))) {
		if (info->cp_master)
			charger_dev_cp_get_fault_type(info->cp_master, &cp_m_fault);
		if (info->cp_slave)
			charger_dev_cp_get_fault_type(info->cp_slave, &cp_s_fault);
		if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_CP_VBAT_OVP)) || cp_m_fault&BIT_MASK(7) || cp_s_fault&BIT_MASK(7)) {
			info->dfx_cyclial_report_type |= BIT_MASK(CHG_DFX_CP_VBAT_OVP);
			info->dfx_fake_report_type &= ~BIT_MASK(CHG_DFX_CP_VBAT_OVP);
			// prepare report info
			info->dfx_report_info->param_cnt[CHG_DFX_CP_VBAT_OVP] = 2;
			info->dfx_report_info->format[CHG_DFX_CP_VBAT_OVP][0] = "char";
			info->dfx_report_info->key[CHG_DFX_CP_VBAT_OVP][0] = "chgErrInfo";
			info->dfx_report_info->val[CHG_DFX_CP_VBAT_OVP][0].strval = xm_chg_dfx_report_text[CHG_DFX_CP_VBAT_OVP];
			info->dfx_report_info->format[CHG_DFX_CP_VBAT_OVP][1] = "int";
			info->dfx_report_info->key[CHG_DFX_CP_VBAT_OVP][1] = "vbat";
			info->dfx_report_info->val[CHG_DFX_CP_VBAT_OVP][1].intval = info->vbat_now;
			xm_handle_dfx_report(CHG_DFX_CP_VBAT_OVP);
		}
	}

	if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_ANTI_BURN_TRIGGER)) || !(info->dfx_cyclial_report_type & BIT_MASK(CHG_DFX_ANTI_BURN_TRIGGER))) {
		if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_ANTI_BURN_TRIGGER)) || info->typec_burn || info->typec_otg_burn) {
			info->dfx_cyclial_report_type |= BIT_MASK(CHG_DFX_ANTI_BURN_TRIGGER);
			info->dfx_fake_report_type &= ~BIT_MASK(CHG_DFX_ANTI_BURN_TRIGGER);
			// prepare report info
			usb_get_property(USB_PROP_CONNECTOR_TEMP, &tconn);
			info->dfx_report_info->param_cnt[CHG_DFX_ANTI_BURN_TRIGGER] = 2;
			info->dfx_report_info->format[CHG_DFX_ANTI_BURN_TRIGGER][0] =  "char";
			info->dfx_report_info->key[CHG_DFX_ANTI_BURN_TRIGGER][0] = "chgStatInfo";
			info->dfx_report_info->val[CHG_DFX_ANTI_BURN_TRIGGER][0].strval = xm_chg_dfx_report_text[CHG_DFX_ANTI_BURN_TRIGGER];
			info->dfx_report_info->format[CHG_DFX_ANTI_BURN_TRIGGER][1] = "int";
			info->dfx_report_info->key[CHG_DFX_ANTI_BURN_TRIGGER][1] = "tconn";
			info->dfx_report_info->val[CHG_DFX_ANTI_BURN_TRIGGER][1].intval = tconn;
			xm_handle_dfx_report(CHG_DFX_ANTI_BURN_TRIGGER);
		}
	}

	if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_ANTIBURN_ERR)) || !(info->dfx_cyclial_report_type & BIT_MASK(CHG_DFX_ANTIBURN_ERR))) {
		if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_ANTIBURN_ERR)) || (info->typec_burn && get_vbus(info) > 4100)) {
			info->dfx_cyclial_report_type |= BIT_MASK(CHG_DFX_ANTIBURN_ERR);
			info->dfx_fake_report_type &= ~BIT_MASK(CHG_DFX_ANTIBURN_ERR);
			// prepare report info
			usb_get_property(USB_PROP_CONNECTOR_TEMP, &tconn);
			info->dfx_report_info->param_cnt[CHG_DFX_ANTIBURN_ERR] = 2;
			info->dfx_report_info->format[CHG_DFX_ANTIBURN_ERR][0] =  "char";
			info->dfx_report_info->key[CHG_DFX_ANTIBURN_ERR][0] = "chgErrInfo";
			info->dfx_report_info->val[CHG_DFX_ANTIBURN_ERR][0].strval = xm_chg_dfx_report_text[CHG_DFX_ANTIBURN_ERR];
			info->dfx_report_info->format[CHG_DFX_ANTIBURN_ERR][1] = "int";
			info->dfx_report_info->key[CHG_DFX_ANTIBURN_ERR][1] = "tconn";
			info->dfx_report_info->val[CHG_DFX_ANTIBURN_ERR][1].intval = tconn;
			xm_handle_dfx_report(CHG_DFX_ANTIBURN_ERR);
		}
	}

	if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_SOC_NOT_FULL)) || !(info->dfx_cyclial_report_type & BIT_MASK(CHG_DFX_SOC_NOT_FULL))) {
		if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_SOC_NOT_FULL)) || (info->real_full && info->soc != 100)) {
			info->dfx_cyclial_report_type |= BIT_MASK(CHG_DFX_SOC_NOT_FULL);
			info->dfx_fake_report_type &= ~BIT_MASK(CHG_DFX_SOC_NOT_FULL);
			// prepare report info
			info->dfx_report_info->param_cnt[CHG_DFX_SOC_NOT_FULL] = 3;
			info->dfx_report_info->format[CHG_DFX_SOC_NOT_FULL][0] =  "char";
			info->dfx_report_info->key[CHG_DFX_SOC_NOT_FULL][0] = "chgErrInfo";
			info->dfx_report_info->val[CHG_DFX_SOC_NOT_FULL][0].strval = xm_chg_dfx_report_text[CHG_DFX_SOC_NOT_FULL];
			info->dfx_report_info->format[CHG_DFX_SOC_NOT_FULL][1] = "int";
			info->dfx_report_info->key[CHG_DFX_SOC_NOT_FULL][1] = "vbat";
			info->dfx_report_info->val[CHG_DFX_SOC_NOT_FULL][1].intval = info->vbat_now;
			info->dfx_report_info->format[CHG_DFX_SOC_NOT_FULL][2] = "int";
			info->dfx_report_info->key[CHG_DFX_SOC_NOT_FULL][2] = "soc";
			info->dfx_report_info->val[CHG_DFX_SOC_NOT_FULL][2].intval = info->soc;
			xm_handle_dfx_report(CHG_DFX_SOC_NOT_FULL);
		}
	}

	if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_LOW_TEMP_DISCHARGING)) || !(info->dfx_cyclial_report_type & BIT_MASK(CHG_DFX_LOW_TEMP_DISCHARGING))) {
		if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_LOW_TEMP_DISCHARGING)) || (!info->real_full && info->jeita_chg_index[0] <= 1 && !info->bbc_charge_enable)) {
			info->dfx_cyclial_report_type |= BIT_MASK(CHG_DFX_LOW_TEMP_DISCHARGING);
			info->dfx_fake_report_type &= ~BIT_MASK(CHG_DFX_LOW_TEMP_DISCHARGING);
			// prepare report info
			info->dfx_report_info->param_cnt[CHG_DFX_LOW_TEMP_DISCHARGING] = 3;
			info->dfx_report_info->format[CHG_DFX_LOW_TEMP_DISCHARGING][0] =  "char";
			info->dfx_report_info->key[CHG_DFX_LOW_TEMP_DISCHARGING][0] = "chgErrInfo";
			info->dfx_report_info->val[CHG_DFX_LOW_TEMP_DISCHARGING][0].strval = xm_chg_dfx_report_text[CHG_DFX_LOW_TEMP_DISCHARGING];
			info->dfx_report_info->format[CHG_DFX_LOW_TEMP_DISCHARGING][1] = "int";
			info->dfx_report_info->key[CHG_DFX_LOW_TEMP_DISCHARGING][1] = "vbat";
			info->dfx_report_info->val[CHG_DFX_LOW_TEMP_DISCHARGING][1].intval = info->vbat_now;
			info->dfx_report_info->format[CHG_DFX_LOW_TEMP_DISCHARGING][2] = "int";
			info->dfx_report_info->key[CHG_DFX_LOW_TEMP_DISCHARGING][2] = "soc";
			info->dfx_report_info->val[CHG_DFX_LOW_TEMP_DISCHARGING][2].intval = info->soc;
			xm_handle_dfx_report(CHG_DFX_LOW_TEMP_DISCHARGING);
		}
	}

	if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_HIGH_TEMP_DISCHARGING)) || !(info->dfx_cyclial_report_type & BIT_MASK(CHG_DFX_HIGH_TEMP_DISCHARGING))) {
		if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_HIGH_TEMP_DISCHARGING)) || (!info->warm_term && info->jeita_chg_index[0] == 5 && !info->bbc_charge_enable)) {
			info->dfx_cyclial_report_type |= BIT_MASK(CHG_DFX_HIGH_TEMP_DISCHARGING);
			info->dfx_fake_report_type &= ~BIT_MASK(CHG_DFX_HIGH_TEMP_DISCHARGING);
			// prepare report info
			info->dfx_report_info->param_cnt[CHG_DFX_HIGH_TEMP_DISCHARGING] = 3;
			info->dfx_report_info->format[CHG_DFX_HIGH_TEMP_DISCHARGING][0] =  "char";
			info->dfx_report_info->key[CHG_DFX_HIGH_TEMP_DISCHARGING][0] = "chgErrInfo";
			info->dfx_report_info->val[CHG_DFX_HIGH_TEMP_DISCHARGING][0].strval = xm_chg_dfx_report_text[CHG_DFX_HIGH_TEMP_DISCHARGING];
			info->dfx_report_info->format[CHG_DFX_HIGH_TEMP_DISCHARGING][1] = "int";
			info->dfx_report_info->key[CHG_DFX_HIGH_TEMP_DISCHARGING][1] = "vbat";
			info->dfx_report_info->val[CHG_DFX_HIGH_TEMP_DISCHARGING][1].intval = info->vbat_now;
			info->dfx_report_info->format[CHG_DFX_HIGH_TEMP_DISCHARGING][2] = "int";
			info->dfx_report_info->key[CHG_DFX_HIGH_TEMP_DISCHARGING][2] = "soc";
			info->dfx_report_info->val[CHG_DFX_HIGH_TEMP_DISCHARGING][2].intval = info->soc;
			xm_handle_dfx_report(CHG_DFX_HIGH_TEMP_DISCHARGING);
		}
	}

	if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_SMART_ENDURANCE_TRIGGER)) || !(info->dfx_cyclial_report_type & BIT_MASK(CHG_DFX_SMART_ENDURANCE_TRIGGER))) {
		if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_SMART_ENDURANCE_TRIGGER)) || info->smart_chg[SMART_CHG_ENDURANCE_PRO].active_status) {
			info->dfx_cyclial_report_type |= BIT_MASK(CHG_DFX_SMART_ENDURANCE_TRIGGER);
			info->dfx_fake_report_type &= ~BIT_MASK(CHG_DFX_SMART_ENDURANCE_TRIGGER);
			// prepare report info
			info->dfx_report_info->param_cnt[CHG_DFX_SMART_ENDURANCE_TRIGGER] = 2;
			info->dfx_report_info->format[CHG_DFX_SMART_ENDURANCE_TRIGGER][0] = "char";
			info->dfx_report_info->key[CHG_DFX_SMART_ENDURANCE_TRIGGER][0] = "chgStatInfo";
			info->dfx_report_info->val[CHG_DFX_SMART_ENDURANCE_TRIGGER][0].strval = xm_chg_dfx_report_text[CHG_DFX_SMART_ENDURANCE_TRIGGER];
			info->dfx_report_info->format[CHG_DFX_SMART_ENDURANCE_TRIGGER][1] = "int";
			info->dfx_report_info->key[CHG_DFX_SMART_ENDURANCE_TRIGGER][1] = "soc";
			info->dfx_report_info->val[CHG_DFX_SMART_ENDURANCE_TRIGGER][1].intval = info->soc;
			xm_handle_dfx_report(CHG_DFX_SMART_ENDURANCE_TRIGGER);
		}
	}

	if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_SMART_NAVIGATION_TRIGGER)) || !(info->dfx_cyclial_report_type & BIT_MASK(CHG_DFX_SMART_NAVIGATION_TRIGGER))) {
		if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_SMART_NAVIGATION_TRIGGER)) || info->smart_chg[SMART_CHG_NAVIGATION].active_status) {
			info->dfx_cyclial_report_type |= BIT_MASK(CHG_DFX_SMART_NAVIGATION_TRIGGER);
			info->dfx_fake_report_type &= ~BIT_MASK(CHG_DFX_SMART_NAVIGATION_TRIGGER);
			// prepare report info
			info->dfx_report_info->param_cnt[CHG_DFX_SMART_NAVIGATION_TRIGGER] = 2;
			info->dfx_report_info->format[CHG_DFX_SMART_NAVIGATION_TRIGGER][0] = "char";
			info->dfx_report_info->key[CHG_DFX_SMART_NAVIGATION_TRIGGER][0] = "chgStatInfo";
			info->dfx_report_info->val[CHG_DFX_SMART_NAVIGATION_TRIGGER][0].strval = xm_chg_dfx_report_text[CHG_DFX_SMART_NAVIGATION_TRIGGER];
			info->dfx_report_info->format[CHG_DFX_SMART_NAVIGATION_TRIGGER][1] = "int";
			info->dfx_report_info->key[CHG_DFX_SMART_NAVIGATION_TRIGGER][1] = "soc";
			info->dfx_report_info->val[CHG_DFX_SMART_NAVIGATION_TRIGGER][1].intval = info->soc;
			xm_handle_dfx_report(CHG_DFX_SMART_NAVIGATION_TRIGGER);
		}
	}

	if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_BATT_LINKER_ABSENT)) || !(info->dfx_cyclial_report_type & BIT_MASK(CHG_DFX_BATT_LINKER_ABSENT))) {
		if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_BATT_LINKER_ABSENT)) || info->slave_connector_abnormal) {
			info->dfx_cyclial_report_type |= BIT_MASK(CHG_DFX_BATT_LINKER_ABSENT);
			info->dfx_fake_report_type &= ~BIT_MASK(CHG_DFX_BATT_LINKER_ABSENT);
			// prepare report info
			info->dfx_report_info->param_cnt[CHG_DFX_BATT_LINKER_ABSENT] = 1;
			info->dfx_report_info->format[CHG_DFX_BATT_LINKER_ABSENT][0] = "char";
			info->dfx_report_info->key[CHG_DFX_BATT_LINKER_ABSENT][0] = "chgErrInfo";
			info->dfx_report_info->val[CHG_DFX_BATT_LINKER_ABSENT][0].strval = xm_chg_dfx_report_text[CHG_DFX_BATT_LINKER_ABSENT];
			xm_handle_dfx_report(CHG_DFX_BATT_LINKER_ABSENT);
		}
	}

	if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_CP_TDIE_HOT)) || !(info->dfx_cyclial_report_type & BIT_MASK(CHG_DFX_CP_TDIE_HOT))) {
		// set interval to reduce frequency
		if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_CP_TDIE_HOT)) || info->dfx_cyclial_check_count >= DFX_CHG_CP_TDIE_HOT_CHECK_CNT) {
			if (info->cp_master)
				charger_dev_cp_get_tdie(info->cp_master, &cp_m_tdie);
			if (info->cp_slave)
				charger_dev_cp_get_tdie(info->cp_slave, &cp_s_tdie);
			if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_CP_TDIE_HOT)) || cp_m_tdie > DFX_CHG_CP_TDIE_HOT_THRESHOLD || cp_s_tdie > DFX_CHG_CP_TDIE_HOT_THRESHOLD) {
				info->dfx_cyclial_report_type |= BIT_MASK(CHG_DFX_CP_TDIE_HOT);
				info->dfx_fake_report_type &= ~BIT_MASK(CHG_DFX_CP_TDIE_HOT);
				// prepare report info
				info->dfx_report_info->param_cnt[CHG_DFX_CP_TDIE_HOT] = 3;
				info->dfx_report_info->format[CHG_DFX_CP_TDIE_HOT][0] = "char";
				info->dfx_report_info->key[CHG_DFX_CP_TDIE_HOT][0] = "chgStatInfo";
				info->dfx_report_info->val[CHG_DFX_CP_TDIE_HOT][0].strval = xm_chg_dfx_report_text[CHG_DFX_CP_TDIE_HOT];
				info->dfx_report_info->format[CHG_DFX_CP_TDIE_HOT][1] = "int";
				info->dfx_report_info->key[CHG_DFX_CP_TDIE_HOT][1] = "masterTdie";
				info->dfx_report_info->val[CHG_DFX_CP_TDIE_HOT][1].intval = cp_m_tdie;
				info->dfx_report_info->format[CHG_DFX_CP_TDIE_HOT][2] = "int";
				info->dfx_report_info->key[CHG_DFX_CP_TDIE_HOT][2] = "slaveTdie";
				info->dfx_report_info->val[CHG_DFX_CP_TDIE_HOT][2].intval = cp_s_tdie;
				xm_handle_dfx_report(CHG_DFX_CP_TDIE_HOT);
			}
			info->dfx_cyclial_check_count = 0;
		} else
			info->dfx_cyclial_check_count++;
	}

	if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_SMART_ENDURANCE_SOC_ERR)) || !(info->dfx_cyclial_report_type & BIT_MASK(CHG_DFX_SMART_ENDURANCE_SOC_ERR))) {
		if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_SMART_ENDURANCE_SOC_ERR))
				|| (info->smart_chg[SMART_CHG_ENDURANCE_PRO].en_ret && info->entry_soc < SMART_CHG_SOCLMT_TRIG_DEFAULT && info->soc > SMART_CHG_SOCLMT_TRIG_DEFAULT)) {
			info->dfx_cyclial_report_type |= BIT_MASK(CHG_DFX_SMART_ENDURANCE_SOC_ERR);
			info->dfx_fake_report_type &= ~BIT_MASK(CHG_DFX_SMART_ENDURANCE_SOC_ERR);
			// prepare report info
			info->dfx_report_info->param_cnt[CHG_DFX_SMART_ENDURANCE_SOC_ERR] = 2;
			info->dfx_report_info->format[CHG_DFX_SMART_ENDURANCE_SOC_ERR][0] = "char";
			info->dfx_report_info->key[CHG_DFX_SMART_ENDURANCE_SOC_ERR][0] = "chgErrInfo";
			info->dfx_report_info->val[CHG_DFX_SMART_ENDURANCE_SOC_ERR][0].strval = xm_chg_dfx_report_text[CHG_DFX_SMART_ENDURANCE_SOC_ERR];
			info->dfx_report_info->format[CHG_DFX_SMART_ENDURANCE_SOC_ERR][1] = "int";
			info->dfx_report_info->key[CHG_DFX_SMART_ENDURANCE_SOC_ERR][1] = "soc";
			info->dfx_report_info->val[CHG_DFX_SMART_ENDURANCE_SOC_ERR][1].intval = info->soc;
			xm_handle_dfx_report(CHG_DFX_SMART_ENDURANCE_SOC_ERR);
		}
	}

	if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_SMART_NAVIGATION_SOC_ERR)) || !(info->dfx_cyclial_report_type & BIT_MASK(CHG_DFX_SMART_NAVIGATION_SOC_ERR))) {
		if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_SMART_NAVIGATION_SOC_ERR))
				|| (info->smart_chg[SMART_CHG_NAVIGATION].en_ret && info->entry_soc < info->smart_chg[SMART_CHG_NAVIGATION].func_val
				&& info->soc > info->smart_chg[SMART_CHG_NAVIGATION].func_val)) {
			info->dfx_cyclial_report_type |= BIT_MASK(CHG_DFX_SMART_NAVIGATION_SOC_ERR);
			info->dfx_fake_report_type &= ~BIT_MASK(CHG_DFX_SMART_NAVIGATION_SOC_ERR);
			// prepare report info
			info->dfx_report_info->param_cnt[CHG_DFX_SMART_NAVIGATION_SOC_ERR] = 2;
			info->dfx_report_info->format[CHG_DFX_SMART_NAVIGATION_SOC_ERR][0] = "char";
			info->dfx_report_info->key[CHG_DFX_SMART_NAVIGATION_SOC_ERR][0] = "chgErrInfo";
			info->dfx_report_info->val[CHG_DFX_SMART_NAVIGATION_SOC_ERR][0].strval = xm_chg_dfx_report_text[CHG_DFX_SMART_NAVIGATION_SOC_ERR];
			info->dfx_report_info->format[CHG_DFX_SMART_NAVIGATION_SOC_ERR][1] = "int";
			info->dfx_report_info->key[CHG_DFX_SMART_NAVIGATION_SOC_ERR][1] = "soc";
			info->dfx_report_info->val[CHG_DFX_SMART_NAVIGATION_SOC_ERR][1].intval = info->soc;
			xm_handle_dfx_report(CHG_DFX_SMART_NAVIGATION_SOC_ERR);
		}
	}

	chr_err("%s: single_type=0x%x, cyclial_type=0x%x, fake_type=0x%x\n",
		__func__, info->dfx_single_report_type, info->dfx_cyclial_report_type, info->dfx_fake_report_type);
}

static void update_dfx_single_report(struct mtk_charger *info)
{
	int fg_chipok = 1, cp_m_chipok = 1, cp_s_chipok = 1;
	int ret = 0;

	if (info == NULL || info->dfx_report_info == NULL)
		return;

#if defined(CONFIG_RUST_DETECTION)
	if (info->lpd_flag && !(info->dfx_single_report_type & BIT_MASK(CHG_DFX_LPD_DISCHARGE))) {
		info->dfx_single_report_type |= BIT_MASK(CHG_DFX_LPD_DISCHARGE);
		info->dfx_single_report_type &= ~BIT_MASK(CHG_DFX_LPD_DISCHARGE_RESET);
		// prepare report info
		info->dfx_report_info->param_cnt[CHG_DFX_LPD_DISCHARGE] = 2;
		info->dfx_report_info->format[CHG_DFX_LPD_DISCHARGE][0] = "char";
		info->dfx_report_info->key[CHG_DFX_LPD_DISCHARGE][0] = "lpdFlag";
		info->dfx_report_info->val[CHG_DFX_LPD_DISCHARGE][0].strval = xm_chg_dfx_report_text[CHG_DFX_LPD_DISCHARGE];
		info->dfx_report_info->format[CHG_DFX_LPD_DISCHARGE][1] = "int";
		info->dfx_report_info->key[CHG_DFX_LPD_DISCHARGE][1] = "status";
		info->dfx_report_info->val[CHG_DFX_LPD_DISCHARGE][1].intval = 1;
		xm_handle_dfx_report(CHG_DFX_LPD_DISCHARGE);
	} else if (!info->lpd_flag && (info->dfx_single_report_type & BIT_MASK(CHG_DFX_LPD_DISCHARGE)) && !(info->dfx_single_report_type & BIT_MASK(CHG_DFX_LPD_DISCHARGE_RESET))) {
		info->dfx_single_report_type &= ~BIT_MASK(CHG_DFX_LPD_DISCHARGE);
		info->dfx_single_report_type |= BIT_MASK(CHG_DFX_LPD_DISCHARGE_RESET);
		// prepare report info
		info->dfx_report_info->param_cnt[CHG_DFX_LPD_DISCHARGE_RESET] = 2;
		info->dfx_report_info->format[CHG_DFX_LPD_DISCHARGE_RESET][0] = "char";
		info->dfx_report_info->key[CHG_DFX_LPD_DISCHARGE_RESET][0] = "lpdFlag";
		info->dfx_report_info->val[CHG_DFX_LPD_DISCHARGE_RESET][0].strval = xm_chg_dfx_report_text[CHG_DFX_LPD_DISCHARGE_RESET];
		info->dfx_report_info->format[CHG_DFX_LPD_DISCHARGE_RESET][1] = "int";
		info->dfx_report_info->key[CHG_DFX_LPD_DISCHARGE_RESET][1] = "status";
		info->dfx_report_info->val[CHG_DFX_LPD_DISCHARGE_RESET][1].intval = 0;
		xm_handle_dfx_report(CHG_DFX_LPD_DISCHARGE_RESET);
	}
#endif

	if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_FG_IIC_ERR)) || !(info->dfx_single_report_type & BIT_MASK(CHG_DFX_FG_IIC_ERR))) {
		ret = bms_get_property(BMS_PROP_CHIP_OK, &fg_chipok);
		if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_FG_IIC_ERR)) || !fg_chipok || info->bms_i2c_error_count >= 10) {
			info->dfx_single_report_type |= BIT_MASK(CHG_DFX_FG_IIC_ERR);
			info->dfx_fake_report_type &= ~BIT_MASK(CHG_DFX_FG_IIC_ERR);
			// prepare report info
			info->dfx_report_info->param_cnt[CHG_DFX_FG_IIC_ERR] = 1;
			info->dfx_report_info->format[CHG_DFX_FG_IIC_ERR][0] = "char";
			info->dfx_report_info->key[CHG_DFX_FG_IIC_ERR][0] = "chgErrInfo";
			info->dfx_report_info->val[CHG_DFX_FG_IIC_ERR][0].strval = xm_chg_dfx_report_text[CHG_DFX_FG_IIC_ERR];
			xm_handle_dfx_report(CHG_DFX_FG_IIC_ERR);
		}
	}

	if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_CP_ERR)) || !(info->dfx_single_report_type & BIT_MASK(CHG_DFX_CP_ERR))) {
		if (info->cp_master)
			charger_dev_cp_chip_ok(info->cp_master, &cp_m_chipok);
		if (info->cp_slave)
			charger_dev_cp_chip_ok(info->cp_slave, &cp_s_chipok);
		if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_CP_ERR)) || !cp_m_chipok || !cp_s_chipok) {
			info->dfx_single_report_type |= BIT_MASK(CHG_DFX_CP_ERR);
			info->dfx_fake_report_type &= ~BIT_MASK(CHG_DFX_CP_ERR);
			// prepare report info
			info->dfx_report_info->param_cnt[CHG_DFX_CP_ERR] = 3;
			info->dfx_report_info->format[CHG_DFX_CP_ERR][0] = "char";
			info->dfx_report_info->key[CHG_DFX_CP_ERR][0] = "chgErrInfo";
			info->dfx_report_info->val[CHG_DFX_CP_ERR][0].strval = xm_chg_dfx_report_text[CHG_DFX_CP_ERR];
			info->dfx_report_info->format[CHG_DFX_CP_ERR][1] = "int";
			info->dfx_report_info->key[CHG_DFX_CP_ERR][1] = "masterOk";
			info->dfx_report_info->val[CHG_DFX_CP_ERR][1].intval = cp_m_chipok;
			info->dfx_report_info->format[CHG_DFX_CP_ERR][2] = "int";
			info->dfx_report_info->key[CHG_DFX_CP_ERR][2] = "slaveOk";
			info->dfx_report_info->val[CHG_DFX_CP_ERR][2].intval = cp_s_chipok;
			xm_handle_dfx_report(CHG_DFX_CP_ERR);
		}
	}

	if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_VBAT_SOC_NOT_MATCH)) || !(info->dfx_single_report_type & BIT_MASK(CHG_DFX_VBAT_SOC_NOT_MATCH))) {
		if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_VBAT_SOC_NOT_MATCH))
				|| (-info->current_now > 5000 && info->vbat_now > 3900 && info->soc < 2)
				|| (-info->current_now > 1000 && -info->current_now <= 5000 && info->vbat_now > 3850 && info->soc < 2)
				|| (-info->current_now <= 1000 && info->vbat_now > 3800 && info->soc < 2)) {
			info->dfx_single_report_type |= BIT_MASK(CHG_DFX_VBAT_SOC_NOT_MATCH);
			info->dfx_fake_report_type &= ~BIT_MASK(CHG_DFX_VBAT_SOC_NOT_MATCH);
			// prepare report info
			info->dfx_report_info->param_cnt[CHG_DFX_VBAT_SOC_NOT_MATCH] = 4;
			info->dfx_report_info->format[CHG_DFX_VBAT_SOC_NOT_MATCH][0] = "char";
			info->dfx_report_info->key[CHG_DFX_VBAT_SOC_NOT_MATCH][0] = "chgErrInfo";
			info->dfx_report_info->val[CHG_DFX_VBAT_SOC_NOT_MATCH][0].strval = xm_chg_dfx_report_text[CHG_DFX_VBAT_SOC_NOT_MATCH];
			info->dfx_report_info->format[CHG_DFX_VBAT_SOC_NOT_MATCH][1] = "int";
			info->dfx_report_info->key[CHG_DFX_VBAT_SOC_NOT_MATCH][1] = "vbat";
			info->dfx_report_info->val[CHG_DFX_VBAT_SOC_NOT_MATCH][1].intval = info->vbat_now;
			info->dfx_report_info->format[CHG_DFX_VBAT_SOC_NOT_MATCH][2] = "int";
			info->dfx_report_info->key[CHG_DFX_VBAT_SOC_NOT_MATCH][2] = "soc";
			info->dfx_report_info->val[CHG_DFX_VBAT_SOC_NOT_MATCH][2].intval = info->soc;
			info->dfx_report_info->format[CHG_DFX_VBAT_SOC_NOT_MATCH][3] = "int";
			info->dfx_report_info->key[CHG_DFX_VBAT_SOC_NOT_MATCH][3] = "cycleCnt";
			info->dfx_report_info->val[CHG_DFX_VBAT_SOC_NOT_MATCH][3].intval = info->cycle_count;
			xm_handle_dfx_report(CHG_DFX_VBAT_SOC_NOT_MATCH);
		}
	}

	if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_BATT_AUTH_FAIL)) || !(info->dfx_single_report_type & BIT_MASK(CHG_DFX_BATT_AUTH_FAIL))) {
		if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_BATT_AUTH_FAIL)) || !info->gauge_authentic) {
			info->dfx_single_report_type |= BIT_MASK(CHG_DFX_BATT_AUTH_FAIL);
			info->dfx_fake_report_type &= ~BIT_MASK(CHG_DFX_BATT_AUTH_FAIL);
			// prepare report info
			info->dfx_report_info->param_cnt[CHG_DFX_BATT_AUTH_FAIL] = 1;
			info->dfx_report_info->format[CHG_DFX_BATT_AUTH_FAIL][0] = "char";
			info->dfx_report_info->key[CHG_DFX_BATT_AUTH_FAIL][0] = "chgErrInfo";
			info->dfx_report_info->val[CHG_DFX_BATT_AUTH_FAIL][0].strval = xm_chg_dfx_report_text[CHG_DFX_BATT_AUTH_FAIL];
			xm_handle_dfx_report(CHG_DFX_BATT_AUTH_FAIL);
		}
	}

	chr_err("%s: type=0x%x\n", __func__, info->dfx_single_report_type);
}

static void reset_dfx_cyclial_report(struct mtk_charger *info)
{
	int temp_cold_thres = -100, tbat_min = 0, tboard = 0;
	int temp_hot_thres = 550;

	if (info == NULL || info->dfx_report_info == NULL)
		return;

	if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_TBAT_COLD)) || !(info->dfx_cyclial_report_type & BIT_MASK(CHG_DFX_TBAT_COLD))) {
		if (info->project_no == ROTHKO_CN || info->project_no == ROTHKO_GL || info->project_no == DEGAS_CN || info->project_no == DEGAS_GL)
			temp_cold_thres = -200;
		if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_TBAT_COLD)) || info->temp_now < temp_cold_thres) {
			info->dfx_cyclial_report_type |= BIT_MASK(CHG_DFX_TBAT_COLD);
			info->dfx_fake_report_type &= ~BIT_MASK(CHG_DFX_TBAT_COLD);
			if(info->bms_psy)
				bms_get_property(BMS_PROP_TEMP_MIN, &tbat_min);
			usb_get_property(USB_PROP_BOARD_TEMP, &tboard);
			// prepare report info
			info->dfx_report_info->param_cnt[CHG_DFX_TBAT_COLD] = 5;
			info->dfx_report_info->format[CHG_DFX_TBAT_COLD][0] = "char";
			info->dfx_report_info->key[CHG_DFX_TBAT_COLD][0] = "chgStatInfo";
			info->dfx_report_info->val[CHG_DFX_TBAT_COLD][0].strval = xm_chg_dfx_report_text[CHG_DFX_TBAT_COLD];
			info->dfx_report_info->format[CHG_DFX_TBAT_COLD][1] = "int";
			info->dfx_report_info->key[CHG_DFX_TBAT_COLD][1] = "tbat";
			info->dfx_report_info->val[CHG_DFX_TBAT_COLD][1].intval = info->temp_now;
			info->dfx_report_info->format[CHG_DFX_TBAT_COLD][2] = "int";
			info->dfx_report_info->key[CHG_DFX_TBAT_COLD][2] = "isChgExist";
			info->dfx_report_info->val[CHG_DFX_TBAT_COLD][2].intval = is_charger_exist(info);
			info->dfx_report_info->format[CHG_DFX_TBAT_COLD][3] = "int";
			info->dfx_report_info->key[CHG_DFX_TBAT_COLD][3] = "tbatMin";
			info->dfx_report_info->val[CHG_DFX_TBAT_COLD][3].intval = tbat_min;
			info->dfx_report_info->format[CHG_DFX_TBAT_COLD][4] = "int";
			info->dfx_report_info->key[CHG_DFX_TBAT_COLD][4] = "tboard";
			info->dfx_report_info->val[CHG_DFX_TBAT_COLD][4].intval = tboard;
			xm_handle_dfx_report(CHG_DFX_TBAT_COLD);
		}
	}

	if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_TBAT_HOT)) || !(info->dfx_cyclial_report_type & BIT_MASK(CHG_DFX_TBAT_HOT))) {
		if ((info->dfx_fake_report_type & BIT_MASK(CHG_DFX_TBAT_HOT)) || info->temp_now > temp_hot_thres) {
			info->dfx_cyclial_report_type |= BIT_MASK(CHG_DFX_TBAT_HOT);
			info->dfx_fake_report_type &= ~BIT_MASK(CHG_DFX_TBAT_HOT);
			if(info->bms_psy)
				bms_get_property(BMS_PROP_TEMP_MIN, &tbat_min);
			usb_get_property(USB_PROP_BOARD_TEMP, &tboard);
			// prepare report info
			info->dfx_report_info->param_cnt[CHG_DFX_TBAT_HOT] = 5;
			info->dfx_report_info->format[CHG_DFX_TBAT_HOT][0] = "char";
			info->dfx_report_info->key[CHG_DFX_TBAT_HOT][0] = "chgStatInfo";
			info->dfx_report_info->val[CHG_DFX_TBAT_HOT][0].strval = xm_chg_dfx_report_text[CHG_DFX_TBAT_HOT];
			info->dfx_report_info->format[CHG_DFX_TBAT_HOT][1] = "int";
			info->dfx_report_info->key[CHG_DFX_TBAT_HOT][1] = "tbat";
			info->dfx_report_info->val[CHG_DFX_TBAT_HOT][1].intval = info->temp_now;
			info->dfx_report_info->format[CHG_DFX_TBAT_HOT][2] = "int";
			info->dfx_report_info->key[CHG_DFX_TBAT_HOT][2] = "isChgExist";
			info->dfx_report_info->val[CHG_DFX_TBAT_HOT][2].intval = is_charger_exist(info);
			info->dfx_report_info->format[CHG_DFX_TBAT_HOT][3] = "int";
			info->dfx_report_info->key[CHG_DFX_TBAT_HOT][3] = "tbatMin";
			info->dfx_report_info->val[CHG_DFX_TBAT_HOT][3].intval = tbat_min;
			info->dfx_report_info->format[CHG_DFX_TBAT_HOT][4] = "int";
			info->dfx_report_info->key[CHG_DFX_TBAT_HOT][4] = "tboard";
			info->dfx_report_info->val[CHG_DFX_TBAT_HOT][4].intval = tboard;
			xm_handle_dfx_report(CHG_DFX_TBAT_HOT);
		}
	}

	chr_err("%s: type=0x%x\n", __func__, info->dfx_cyclial_report_type);

	// clear report info
	if (info->dfx_cyclial_report_type != CHG_DFX_DEFAULT) {
		info->dfx_cyclial_report_type = 0;
		memset(info->dfx_report_info, 0, sizeof(struct chg_dfx_report_info));
	}
}

static void  __maybe_unused monitor_lpd_chg(struct mtk_charger *info)
{
	return;

	if(info == NULL || !info->fcc_votable || !info->icl_votable)
	{
		chr_err("%s lpd limit charger get votable failed\n", __func__);
		return;
	}

#if defined(CONFIG_RUST_DETECTION)
	if(info->lpd_charging_limit && info->lpd_flag && !info->lpd_trigger_status)
	{
		info->lpd_trigger_status = true;
		adapter_dev_set_cap_xm(info->pd_adapter, MTK_PD_APDO, 5000, 1600);
		msleep(200);
		if (info->real_type == XMUSB350_TYPE_HVCHG)
			charger_set_dpdm_voltage(info, 0, 0);
		vote(info->icl_votable, LPD_TRIG_VOTER, true, 1600);
		vote(info->fcc_votable, LPD_TRIG_VOTER, true, 1600);
		chr_err("lpd trigger limit charging current 500ma\n");
	}else if(info->lpd_charging_limit && !info->lpd_flag && info->lpd_trigger_status){
		info->lpd_trigger_status = false;
		vote(info->icl_votable, LPD_TRIG_VOTER, false, 0);
		vote(info->fcc_votable, LPD_TRIG_VOTER, false, 0);
		chr_err("lpd not trigger limit charging current\n");
	}
#endif
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

	if (info->smart_soclmt_trig && info->bbc_charge_enable)
	{
		charger_dev_enable(info->chg1_dev, false);
		chr_err("smart_chg soclimit triggger, but chg is still enabled, need to re-stop charging\n");
	}
}

static void smart_chg_report_sicmode_uevent(struct mtk_charger *info)
{
	int len = 0;
	char event[XM_UEVENT_NOTIFY_SIZE] = { 0 };
	struct xm_uevent_notify_data event_data = { 0 };

	len = snprintf(event, XM_UEVENT_NOTIFY_SIZE, "POWER_SUPPLY_SMART_SIC_MODE=%d", info->smart_sic_mode);
	event_data.event = event;
	event_data.event_len = len;
	generate_xm_single_uevent(info, &event_data);
}

static void sic_mode_report_work_func(struct work_struct *work)
{
	struct mtk_charger *info = container_of(work, struct mtk_charger, sic_mode_report_work.work);
	smart_chg_report_sicmode_uevent(info);
}

static void smart_chg_handle_sensing_chg_thermlvl(struct mtk_charger *info, bool active, int scene, int smart_sic_mode)
{
	int smart_thermlvl = LVL_NO_ADJUST;

	if (active) {
		if (scene == PHONE_REDIR_SCENE) {
			if (smart_sic_mode == BALANCED_MODE) {
				if (info->board_temp > 29000)
					smart_thermlvl = LVL_INCREASE_MAX;
				else if (info->board_temp > 28000)
					smart_thermlvl = LVL_INCREASE_H;
				else if (info->board_temp > 26000)
					smart_thermlvl = LVL_INCREASE_L;
			} else if (smart_sic_mode == SLIGHTCHG_MODE || smart_sic_mode == MIDDLE_MODE) {
				if (info->board_temp > 31000)
					smart_thermlvl = LVL_INCREASE_MAX;
				else if (info->board_temp > 30000)
					smart_thermlvl = LVL_INCREASE_H;
				else if (info->board_temp > 28000)
					smart_thermlvl = LVL_INCREASE_L;
			}
		} else if (scene == VIDEOCHAT_REDIR_SCENE) {
			if (smart_sic_mode == BALANCED_MODE) {
				if (info->board_temp > 36000)
					smart_thermlvl = LVL_INCREASE_MAX;
				else if (info->board_temp > 34000)
					smart_thermlvl = LVL_INCREASE_H;
				else if (info->board_temp > 32000)
					smart_thermlvl = LVL_INCREASE_M;
				else if (info->board_temp > 30000)
					smart_thermlvl = LVL_INCREASE_L;
			}
		}
	}

	info->smart_therm_lvl = smart_thermlvl;
}

static void smart_chg_handle_sensing_chg(struct mtk_charger *info)
{
	int scene = info->scene, sic_mode = info->sic_mode;
	int smart_sic_mode = (info->smart_snschg_v2_support)? -1 : BALANCED_MODE;
	int posture_st = info->smart_chg[SMART_CHG_SENSING_CHG].func_val;
	bool active_stat = false;

	// scene redirection
	if (scene == PHONE_SCENE) {
		scene = PHONE_REDIR_SCENE;
	} else if (scene == VIDEOCHAT_SCENE) {
		scene = VIDEOCHAT_REDIR_SCENE;
	} else if (scene == VIDEO_SCENE || scene == PER_VIDEO_SCENE) {
		scene = VIDEO_REDIR_SCENE;
	} else if (scene == CLASS0_SCENE || scene == PER_CLASS0_SCENE) {
		scene = CLASS0_REDIR_SCENE;
	}

	if (!info->smart_chg[SMART_CHG_SENSING_CHG].en_ret
			|| (posture_st <= UNKNOW_STAT || posture_st >= STAT_MAX_INDEX))
		goto _sensing_chg_quit;

	switch (scene) {
	case PHONE_REDIR_SCENE:
		active_stat = true;
		if(posture_st >= DESKTOP_STAT && posture_st <= HOLDER_STAT)
			smart_sic_mode = SUPERCHG_MODE;
		else if(posture_st >= ONEHAND_STAT && posture_st <= TWOHAND_V_STAT)
			smart_sic_mode = MIDDLE_MODE;
		else if (posture_st == ANS_CALL_STAT)
			smart_sic_mode = BALANCED_MODE;
		smart_sic_mode = (info->screen_status == DISPLAY_SCREEN_OFF)? smart_sic_mode/2 : smart_sic_mode;
		break;
	case VIDEOCHAT_REDIR_SCENE:
		active_stat = true;
		if(posture_st >= DESKTOP_STAT && posture_st <= HOLDER_STAT)
			smart_sic_mode = SUPERCHG_MODE;
		else if(posture_st >= ONEHAND_STAT && posture_st <= ANS_CALL_STAT)
			smart_sic_mode = BALANCED_MODE;
		smart_sic_mode = (info->screen_status == DISPLAY_SCREEN_OFF)? smart_sic_mode/2 : smart_sic_mode;
		break;
	case VIDEO_REDIR_SCENE:
	case CLASS0_REDIR_SCENE:
		active_stat = true;
		if(posture_st >= DESKTOP_STAT && posture_st <= HOLDER_STAT)
			smart_sic_mode = SUPERCHG_MODE;
		else if(posture_st >= ONEHAND_STAT && posture_st <= ANS_CALL_STAT)
			smart_sic_mode = BALANCED_MODE;
		break;
	default:
		goto _sensing_chg_quit;
	}

	if (info->smart_snschg_nonsic_support)
		smart_chg_handle_sensing_chg_thermlvl(info, active_stat, scene, smart_sic_mode);

_sensing_chg_quit:
	chr_err("%s act[%d->%d] scene[%d %d] post[%d] tbd[%d] sic[%d %d %d %d %d %d] lvl[%d]\n", __func__,
			info->smart_chg[SMART_CHG_SENSING_CHG].active_status, active_stat,
			info->screen_status, scene, posture_st, info->board_temp,
			info->sic_mode_setter, sic_mode, info->smart_sic_mode, info->tmp_sic_mode, info->tmp_sic_mode_chg,
			info->sic_current, info->smart_therm_lvl);

	if (active_stat != info->smart_chg[SMART_CHG_SENSING_CHG].active_status)
		info->smart_chg[SMART_CHG_SENSING_CHG].active_status = active_stat;

	if (info->smart_snschg_v2_support) {
		// version2
		if (smart_sic_mode != info->smart_sic_mode) {
			info->smart_sic_mode = smart_sic_mode;
			cancel_delayed_work_sync(&info->sic_mode_report_work);
			if (active_stat)
				schedule_delayed_work(&info->sic_mode_report_work, 0);
		} else {
			if (active_stat && smart_sic_mode != sic_mode)
				schedule_delayed_work(&info->sic_mode_report_work, msecs_to_jiffies(3000));
		}
	} else {
		// version1
		if (info->tmp_sic_mode == -1) {
			info->tmp_sic_mode = sic_mode;
			chr_err("%s, save upper orgin sic_mode %d.\n", __func__, info->tmp_sic_mode);
		} else if (info->tmp_sic_mode_chg == -1) {
			if ((info->sic_mode_setter == UPPER_SIC_MODE_SETTER) && (sic_mode != info->smart_sic_mode || (info->sic_mode_same_set && sic_mode == BALANCED_MODE))) {
				if (sic_mode == SUPERCHG_MODE) {
					info->tmp_sic_mode = sic_mode;
					chr_err("%s, reset upper orgin sic_mode %d.\n", __func__, info->tmp_sic_mode);
				} else {
					info->tmp_sic_mode_chg = sic_mode;
					chr_err("%s, save upper temporarily sic_mode %d.\n", __func__, info->tmp_sic_mode_chg);
				}
			}
		}

		if (active_stat) {
			if (smart_sic_mode != info->smart_sic_mode || smart_sic_mode != sic_mode) {
				chr_err("%s, apply new sic_mode %d.\n", __func__, smart_sic_mode);
				info->smart_sic_mode = smart_sic_mode;
				if (!info->smart_snschg_nonsic_support) {
					usb_set_property(USB_PROP_SMART_SIC_MODE, smart_sic_mode);
					power_supply_changed(info->usb_psy);
				}
			}
		} else {
			if ((info->tmp_sic_mode_chg == -1 && info->smart_sic_mode != info->tmp_sic_mode) || (info->tmp_sic_mode_chg != -1 && info->smart_sic_mode != info->tmp_sic_mode_chg)) {
				if (info->tmp_sic_mode_chg != -1)
					info->smart_sic_mode = info->tmp_sic_mode_chg;
				else
					info->smart_sic_mode = info->tmp_sic_mode;
				chr_err("%s, recover upper orgin or temporarily sic_mode %d.\n", __func__, info->smart_sic_mode);
				if (!info->smart_snschg_nonsic_support) {
					usb_set_property(USB_PROP_SMART_SIC_MODE, info->smart_sic_mode);
					power_supply_changed(info->usb_psy);
				}
			}
		}
	}
}

static void monitor_smart_chg(struct mtk_charger *info)
{
	if(info == NULL || !info->fcc_votable || !info->smart_chg)
		return;


	smart_chg_handle_soc_limit(info);
	if(info->smart_snschg_support || info->smart_snschg_v2_support)
		smart_chg_handle_sensing_chg(info);
}

static void monitor_slave_connector(struct mtk_charger *info)
{
	int ret = 0;
	int adc_val = 0, gpio_val = 0;
	bool conn_ok = true;

	if(info->slave_connector_auxadc == NULL) { // use gpio detection
		ret = bms_get_property(BMS_PROP_BMS_SLAVE_CONNECT_ERROR, &gpio_val);
		if (ret) {
			chr_err("failed to get slave_conn_gpio status\n");
			return;
		}
	} else { // use adc detection
		ret = iio_read_channel_processed(info->slave_connector_auxadc, &adc_val);
		if(ret < 0) {
			chr_err("failed to get slave_conn_auxadc \n");
			return;
		}
		// usb_set_property(USB_PROP_BATTCONT_ONLINE, adc_val);
	}

	if (gpio_val == 1 || adc_val >= 1400)
		conn_ok = false;
	chr_err("%s conn_ok:%d, adc:%d, gpio:%d\n",__func__, conn_ok, adc_val, gpio_val);

	if(!conn_ok && !info->slave_connector_abnormal)
	{
		vote_override(info->fcc_votable, MAIN_CON_ERR_VOTER, true, 500);
		vote_override(info->icl_votable, MAIN_CON_ERR_VOTER, true, 500);
		info->slave_connector_abnormal = true;
		usb_set_property(USB_PROP_BATTCONT_ONLINE, conn_ok);
		adapter_dev_set_cap_xm(info->pd_adapter, MTK_PD_APDO, 5000, 1000);
		chr_err("batt slave connector disconnect\n");
	}
	else if(conn_ok && info->slave_connector_abnormal)
	{
		vote(info->fcc_votable, MAIN_CON_ERR_VOTER, false, 0);
		vote(info->icl_votable, MAIN_CON_ERR_VOTER, false, 0);
		usb_set_property(USB_PROP_BATTCONT_ONLINE, conn_ok);
		info->slave_connector_abnormal = false;
		chr_err("batt slave connector connect\n");
	}
}

static void monitor_isc_and_soa_charge(struct mtk_charger *info)//NFG1000A & NFG1000B支持该功能
{
	int ret = 0;
	int batt_isc_alert_level = 0;
	int batt_soa_alert_level = 0;

	ret = bms_get_property(BMS_PROP_ISC_ALERT_LEVEL, &batt_isc_alert_level);
	if(ret < 0)
	{
		chr_err("[%s]:get isc occur error.\n", __func__);
		return;
	}

	ret = bms_get_property(BMS_PROP_SOA_ALERT_LEVEL, &batt_soa_alert_level);
	if(ret < 0)
	{
		chr_err("[%s]:get soa occur error.\n", __func__);
		return;
	}
	chr_err("[%s]:isc_alert_level=%d, soa_alert_level=%d\n", __func__, batt_isc_alert_level, batt_soa_alert_level);
	if((batt_isc_alert_level == 3) || batt_soa_alert_level)
	{
		info->diff_fv_val= max(info->diff_fv_val, 20);
		info->div_jeita_fcc_flag = true;
	}
}
static void monitor_fv_descent(struct mtk_charger *info)
{
	bool isEffective = false;
	int fv_vote = 0, fv_vote_dec = 0, ffc_fv_vote = 0, jeita_fv_vote = 0;
	static int last_fv_vote = 0, last_ffc_fv_vote = 0, last_jeita_fv_vote = 0;
	static bool pre_state = false;

	if(!info->cp_sm_run_state && ((info->set_smart_batt_diff_fv == 0 && info->set_smart_fv_diff_fv == 0) || info->diff_fv_val > max(info->set_smart_batt_diff_fv, info->set_smart_fv_diff_fv))) {
		if (info->cp_sm_run_state != pre_state) {
			pre_state = info->cp_sm_run_state;
			chr_err("[%s] skill reduce cv:%d,%d\n", __func__, pre_state, info->cp_sm_run_state);
			return;
		}

		isEffective = is_client_vote_enabled(info->fv_votable, FV_DEC_VOTER);
		ffc_fv_vote = get_client_vote(info->fv_votable, FFC_VOTER);
		jeita_fv_vote = get_client_vote(info->fv_votable, JEITA_CHARGE_VOTER);

		if(!isEffective){
			fv_vote = get_effective_result(info->fv_votable);
			if(info->vbat_now >= (fv_vote - 13)){
				vote(info->fv_votable, FV_DEC_VOTER, true, fv_vote - 10);
				chr_err("[%s] fv vote reduce cv 10 mv:%d\n", __func__, isEffective);
			}else{
				vote(info->fv_votable, FV_DEC_VOTER, false, 0);
			}
		}else if(isEffective && (last_ffc_fv_vote != ffc_fv_vote || last_jeita_fv_vote != jeita_fv_vote)){
			fv_vote_dec = get_effective_result(info->fv_votable);
			vote(info->fv_votable, FV_DEC_VOTER, false, 0);
			fv_vote = get_effective_result(info->fv_votable);

			if(last_fv_vote == fv_vote)
				vote(info->fv_votable, FV_DEC_VOTER, true, fv_vote_dec);
			else if(info->vbat_now >= (fv_vote - 10)){
				vote(info->fv_votable, FV_DEC_VOTER, true, fv_vote - 10);
				chr_err("[%s] fv vote reduce cv 10 mv:%d\n", __func__, isEffective);
			}else{
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

	pre_state = info->cp_sm_run_state;
}

static void charge_monitor_func(struct work_struct *work)
{
	struct mtk_charger *info = container_of(work, struct mtk_charger, charge_monitor_work.work);

	check_charge_data(info);

	update_dfx_cyclial_report(info);

	monitor_isc_and_soa_charge(info);

	handle_ffc_charge(info);

	check_full_recharge(info);

	monitor_thermal_limit(info);

	handle_step_charge(info);

	handle_jeita_charge(info);

	monitor_smart_chg(info);

	monitor_lpd_chg(info);

	monitor_sw_cv(info);

	monitor_jeita_descent(info);

	monitor_slave_connector(info);

	monitor_fv_descent(info);

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
	char* cell_supplier = "";

	if(info->project_no == RODIN_GL)
	{
		ret = bms_get_property(BMS_PROP_CELL_SUPPLIER, &val);
		if (ret)
			chr_err("failed to get gauge batt_cell_supplier\n");
		else
			info->batt_cell_supplier = val;

		if (info->batt_cell_supplier == 3) {
			cell_supplier = "_cos";
		} else {
			cell_supplier = "_atl";
		}
		ret = snprintf(name, sizeof(name), "step_chg_cfg_%d_cycle%s", cycle, cell_supplier);
		if (ret >= sizeof(name))
			chr_err("%s: type_c name is truncated\n", __func__);
		chr_err("%s: parse %s\n", __func__, name);

		total_length = of_property_count_elems_of_size(np, name, sizeof(u32));
		if (total_length < 0) {
			chr_err("Charger DTS is not compatible, switch back to regular DTS decoding.\n");
			cell_supplier = "";
		}
	}
	/* step parameter */
	ret = snprintf(name, sizeof(name), "step_chg_cfg_%d_cycle%s", cycle, cell_supplier);
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
	ret = snprintf(name, sizeof(name), "iterm_ffc_%d_cycle%s", cycle, cell_supplier);
	if (ret >= sizeof(name))
		chr_err("%s: type_c name is truncated\n", __func__);

	chr_err("%s: parse %s\n", __func__, name);

	if (of_property_read_u32(np, name, &val) >= 0)
		info->iterm_ffc = val;
	else {
		chr_err("failed to parse %s use default\n", name);
		info->iterm_ffc = 700;
	}

	ret = snprintf(name, sizeof(name), "iterm_ffc_little_warm_%d_cycle%s", cycle, cell_supplier);
	if (ret >= sizeof(name))
		chr_err("%s: type_c name is truncated\n", __func__);

	chr_err("%s: parse %s\n", __func__, name);

	if (of_property_read_u32(np, name, &val) >= 0)
		info->iterm_ffc_little_warm = val;
	else {
		chr_err("failed to parse %s use default\n", name);
		info->iterm_ffc_little_warm = 800;
	}

	ret = snprintf(name, sizeof(name), "iterm_ffc_warm_%d_cycle%s", cycle, cell_supplier);
	if (ret >= sizeof(name))
		chr_err("%s: type_c name is truncated\n", __func__);

	chr_err("%s: parse %s\n", __func__, name);

	if (of_property_read_u32(np, name, &val) >= 0)
		info->iterm_ffc_warm = val;
	else {
		chr_err("failed to parse %s use default\n", name);
		info->iterm_ffc_warm = 800;
	}

	ret = snprintf(name, sizeof(name), "iterm_ffc_hot_%d_cycle%s", cycle, cell_supplier);
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
	bool update = true;
	int battery_cycle = BATTERY_CYCLE_1_TO_100;
	int cycle_count = 0;
	int aged_in_advance = 0;
	int ret = 0;

	if (!info->bms_psy)
		info->bms_psy = power_supply_get_by_name("bms");

	if (info->bms_psy) {
		ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
		if (ret)
			pval.intval = 0;
	} else {
		pval.intval = 0;
	}

	bms_get_property(BMS_PROP_AGED_IN_ADVANCE, &aged_in_advance);
	chr_err("%s: aged_in_advance = %d \n", __func__, aged_in_advance);

	if (debug_cycle_count > 0)
		info->cycle_count = debug_cycle_count;
	else
		info->cycle_count = pval.intval;

	cycle_count = info->cycle_count;

	if (cycle_count <= 800 && (info->project_no == ROTHKO_GL || info->project_no == DEGAS_GL) && aged_in_advance)
		cycle_count = 888;
	chr_err("%s cycle_count = %d \n", __func__, cycle_count);

	if (cycle_count <= 100) {
		battery_cycle = BATTERY_CYCLE_1_TO_100;
	} else if (cycle_count > 100 && cycle_count <= 300) {
		battery_cycle = BATTERY_CYCLE_100_TO_200;
	} else if (cycle_count > 300 && cycle_count <= 800) {
		battery_cycle = BATTERY_CYCLE_300_TO_400;
	} else if (cycle_count > 800 && cycle_count <= 1000) {
		battery_cycle = BATTERY_CYCLE_800_TO_900;
	}else{
		battery_cycle = BATTERY_CYCLE_1000_TO_1100;
	}

	if (battery_cycle != info->battery_cycle) {
		info->battery_cycle = battery_cycle;
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
	vote(info->fcc_votable, JEITA_CHARGE_VOTER, true, info->jeita_chg_fcc);
	chr_err(" fcc:%d,low_value:%d,high_value:%d",info->jeita_chg_fcc,
	info->jeita_fcc_cfg[info->jeita_chg_index[0]].low_value, info->jeita_fcc_cfg[info->jeita_chg_index[0]].high_value);

	parse_step_charge_config(info, false);
}

void start_dfx_report_timer(struct mtk_charger *info)
{
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;

	/* If the timer was already set, cancel it */
	ret = alarm_try_to_cancel(&info->dfx_report_timer);
	if (ret < 0) {
		chr_err("%s: callback was running, skip timer\n", __func__);
		return;
	}

	ktime_now = ktime_get_boottime();
	time_now = ktime_to_timespec64(ktime_now);
	end_time.tv_sec = time_now.tv_sec + info->dfx_report_interval;
	end_time.tv_nsec = time_now.tv_nsec + 0;
	ktime = ktime_set(end_time.tv_sec, end_time.tv_nsec);

	chr_err("%s: alarm timer start:%d, now=%lld, next=%lld\n", __func__, ret, time_now.tv_sec, end_time.tv_sec);
	alarm_start(&info->dfx_report_timer, ktime);
}

static void dfx_report_det_work_func(struct work_struct *work)
{
	struct mtk_charger *info = container_of(work, struct mtk_charger, dfx_report_det_work.work);

	check_dfx_param_data(info);
	update_dfx_single_report(info);
	reset_dfx_cyclial_report(info);
	start_dfx_report_timer(info);
}

static enum alarmtimer_restart dfx_report_timer_handler(struct alarm *alarm, ktime_t now)
{
	struct mtk_charger *info = container_of(alarm, struct mtk_charger, dfx_report_timer);

	cancel_delayed_work(&info->dfx_report_det_work);
	schedule_delayed_work(&info->dfx_report_det_work, msecs_to_jiffies(1000));
	return ALARMTIMER_NORESTART;
}

int step_jeita_init(struct mtk_charger *info, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int total_length = 0, i = 0, ret = 0;
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
		if (info->thermal_limit[5][i] > MAX_THERMAL_FCC) {
			chr_err("thermal_limit_pd over range\n");
			info->thermal_limit[5][i] = MAX_THERMAL_FCC;
		}else if(info->thermal_limit[5][i] < MIN_THERMAL_FCC){
			chr_err("thermal_limit_pd less range\n");
			info->thermal_limit[5][i] = MIN_THERMAL_FCC;
		}
		if (i != 0) {
			if (info->thermal_limit[5][i] > info->thermal_limit[5][i - 1]) {
				chr_err("thermal_limit_pd order error\n");
				return -1;
			}
		}
	}

	info->cycle_count = 0;
	ret = parse_step_charge_config(info, true);
	if (ret) {
		chr_err("failed to parse step_chg_cfg\n");
		return ret;
	}

	if (info->project_no != RODIN_GL) {
		total_length = of_property_count_elems_of_size(np, "jeita_fcc_cfg", sizeof(u32));
		if (total_length < 0) {
			chr_err("failed to read total_length of config\n");
			return -EINVAL;
		}

		ret = of_property_read_u32_array(np, "jeita_fcc_cfg", (u32 *)info->jeita_fcc_cfg, total_length);
		if (ret) {
			chr_err("failed to parse jeita_fcc_cfg\n");
			return ret;
		}
	} else {
		total_length = of_property_count_elems_of_size(np, "jeita_fcc_cfg_gl", sizeof(u32));
		if (total_length < 0) {
			chr_err("failed to read total_length of config\n");
			return -EINVAL;
		}

		ret = of_property_read_u32_array(np, "jeita_fcc_cfg_gl", (u32 *)info->jeita_fcc_cfg, total_length);
		if (ret) {
			chr_err("failed to parse jeita_fcc_cfg\n");
			return ret;
		}
	}

	for (i = 0; i < STEP_JEITA_TUPLE_COUNT; i++)
		chr_info("JEITA_FCC %d %d %d %d %d\n", info->jeita_fcc_cfg[i].low_threshold, info->jeita_fcc_cfg[i].high_threshold,
				info->jeita_fcc_cfg[i].extra_threshold, info->jeita_fcc_cfg[i].low_value, info->jeita_fcc_cfg[i].high_value);

	if (info->project_no != RODIN_GL) {
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
	} else {
		total_length = of_property_count_elems_of_size(np, "jeita_fv_cfg_gl", sizeof(u32));
		if (total_length < 0) {
			chr_err("failed to read total_length of config\n");
			return -EINVAL;
		}

		ret = of_property_read_u32_array(np, "jeita_fv_cfg_gl", (u32 *)info->jeita_fv_cfg, total_length);
		if (ret) {
			chr_err("failed to parse jeita_fv_cfg\n");
			return ret;
		}
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

	info->mt6368_moscon1_control = of_property_read_bool(np, "mt6368_moscon1_control");
	if (info->mt6368_moscon1_control)
	{
		chr_err("successed to parse mt6368_moscon1_control\n");
	}

	info->burn_control_gpio = of_get_named_gpio(np, "burn_control_gpio", 0);
	if (!gpio_is_valid(info->burn_control_gpio)) {
		chr_err("failed to parse burn_control_gpio\n");
	}

	info->smart_snschg_support =  of_property_read_bool(np, "smart_snschg_support");
	info->smart_snschg_nonsic_support =  of_property_read_bool(np, "smart_snschg_nonsic_support");
	info->smart_snschg_v2_support = of_property_read_bool(np, "smart_snschg_v2_support");

	info->sic_support =  of_property_read_bool(np, "sic_support");
	if (!info->sic_support)
		chr_err("failed to parse sic_support\n");

	info->slave_connector_auxadc = devm_iio_channel_get(dev, "slave_connector_auxadc");
	ret = IS_ERR(info->slave_connector_auxadc);
	if(ret) {
		info->slave_connector_auxadc= NULL;
		pr_err("failed get iio_channel, ret = %d\n", ret);
	}
	else
		pr_err("success get iio_channel, ret = %d\n", ret);

	if (gpio_is_valid(info->burn_control_gpio)) {
		gpio_direction_output(info->burn_control_gpio, 0);
	}
	INIT_DELAYED_WORK(&info->charge_monitor_work, charge_monitor_func);
	INIT_DELAYED_WORK(&info->usb_otg_monitor_work, monitor_usb_otg_burn);
	INIT_DELAYED_WORK(&info->typec_burn_monitor_work, monitor_typec_burn);
	if (info->smart_snschg_v2_support)
		INIT_DELAYED_WORK(&info->sic_mode_report_work, sic_mode_report_work_func);
#if defined(CONFIG_RUST_DETECTION)
	INIT_DELAYED_WORK(&info->rust_detection_work, rust_detection_work_func);
#endif
	INIT_DELAYED_WORK(&info->dfx_report_det_work, dfx_report_det_work_func);
	alarm_init(&info->dfx_report_timer, ALARM_BOOTTIME, dfx_report_timer_handler);
	info->dfx_single_report_type = CHG_DFX_DEFAULT;
	info->dfx_cyclial_report_type = CHG_DFX_DEFAULT;
	info->dfx_cyclial_check_count = 0;
	info->dfx_report_interval = DFX_CHG_REPORT_INTERVAL_TIME;
	start_dfx_report_timer(info);
	schedule_delayed_work(&info->dfx_report_det_work, msecs_to_jiffies(30000)); // do first detecion during bootup

	return ret;
}
