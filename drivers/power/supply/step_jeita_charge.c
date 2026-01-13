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
#include <linux/kconfig.h>
#include "mtk_charger.h"
#include "adapter_class.h"
#include "mtk_battery.h"
#include "pd_cp_manager.h"
#include "hq_fg_class.h"
#include "mtk_printk.h"
#include "xm_chg_uevent.h"

static void get_index(struct step_jeita_cfg0 *cfg, int fallback_hyst, int forward_hyst, int value, int *index, bool ignore_hyst, int max_count)
{
	int new_index = 0, i = 0;

	chr_err("%s: value = %d, index[0] = %d, index[1] = %d\n", __func__, value, index[0], index[1]);
	if (value < cfg[0].low_threshold) {
		index[0] = index[1] = 0;
		return;
	}

	if (value > cfg[max_count - 1].high_threshold)
		new_index = max_count - 1;
	for (i = 0; i < max_count; i++) {
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
#if ENABLE_JEITA_DESCENT
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
#else
	vote(info->fcc_votable, JEITA_CHARGE_VOTER, true, info->jeita_chg_fcc);
#endif
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

static void handle_jeita_charge(struct mtk_charger *info)
{
	static bool jeita_vbat_low = true;
	static int last_temp = 0;
	int intval = 0;

	get_index(info->jeita_fv_cfg, info->jeita_fallback_hyst, info->jeita_forward_hyst, info->temp_now, info->jeita_chg_index, false, JEITA_TUPLE_COUNT);
	if (!info->ffc_enable) {
		if (info->vbat_now > 4095 && info->temp_now > 450) {
			chr_err("%s fix warm stop chg fv\n", __func__);
			vote(info->fv_votable, JEITA_CHARGE_VOTER, true, info->fv_normal  - info->diff_fv_val + info->pmic_comp_v);
			charger_dev_enable(info->chg1_dev, false);
			info->warm_term = true;
			info->charge_full = true;
		} else {
			chr_err("%s set cfg chg fv\n", __func__);
			vote(info->fv_votable, JEITA_CHARGE_VOTER, true, info->jeita_fv_cfg[info->jeita_chg_index[0]].value  - info->diff_fv_val + info->pmic_comp_v);
		}
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
		info->jeita_chg_fcc = info->jeita_chg_fcc * 8 / 10;
		chr_err("[%s]: soa or isp induce jeita current drop: %d\n", __func__, info->jeita_chg_fcc);
	}

	if (is_between(info->jeita_fcc_cfg[0].low_threshold, info->jeita_fcc_cfg[JEITA_TUPLE_COUNT - 1].high_threshold, info->temp_now) && !info->input_suspend && !info->typec_burn)
        chr_err("handle_jeita_charge index = %d,jeita_chg_fcc = %d", info->jeita_chg_index[0], info->jeita_chg_fcc);

	if ((last_temp < 0 && info->temp_now >= 0) || (last_temp >= 0 && info->temp_now < 0)
		|| (last_temp <= 480 && info->temp_now > 480) || (last_temp > 480 && info->temp_now <= 480)) {
		charger_dev_usb_get_property(info->mtk_charger, USB_PROP_QUICK_CHARGE_TYPE, &intval);
		xm_charge_uevent_report(CHG_UEVENT_QUICK_CHARGE_TYPE, intval);
	}

	pr_err("[%s]:last_temp:%d, temp_now:%d\n", __func__, last_temp, info->temp_now);
	last_temp = info->temp_now;

	return;
}

static void handle_step_charge(struct mtk_charger *info)
{
	get_index(info->step_chg_cfg, info->step_fallback_hyst, info->step_forward_hyst, info->vbat_now, info->step_chg_index, false, STEP_JEITA_TUPLE_COUNT);

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
	else if (info->thermal_level >= THERMAL_LIMIT_COUNT)
		thermal_level = THERMAL_LIMIT_COUNT - 1;
	else
	{
		if(info->sic_support)
			thermal_level = (info->thermal_level < 14) ? 0 : info->thermal_level;
		else
			thermal_level = info->thermal_level;
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
	case XMUSB350_TYPE_PD_PPS:
		if ((info->smart_chg->funcs[SMART_CHG_LOW_BATT_FAST_CHG].func_on) && (info->smart_chg->funcs[SMART_CHG_LOW_BATT_FAST_CHG].active_flag)) {
			info->thermal_current = info->thermal_limit[6][thermal_level];
			vote(info->fcc_votable, THERMAL_VOTER, true, info->thermal_limit[6][thermal_level]);
		} else {
			info->thermal_current = info->thermal_limit[5][thermal_level];
			vote(info->fcc_votable, THERMAL_VOTER, true, info->thermal_limit[5][thermal_level]);
		}
		break;
	case XMUSB350_TYPE_HVCHG:
		info->thermal_current = info->thermal_limit[1][thermal_level];
		vote(info->fcc_votable, THERMAL_VOTER, true, info->thermal_limit[1][thermal_level]);
		break;
	default:
		info->thermal_current = info->thermal_limit[0][thermal_level];
		vote(info->fcc_votable, THERMAL_VOTER, true, info->thermal_limit[0][thermal_level]);
	}
	chr_err("info->thermal_level =  %d, thermal_level = %d, info->thermal_current = %d, real_type = %d\n",
		 info->thermal_level, thermal_level, info->thermal_current, info->real_type);
}

static int parse_step_charge_config(struct mtk_charger *info, bool force_update);
static int handle_ffc_charge(struct mtk_charger *info)
{
	int ret = 0, iterm_ffc = 0, iterm = 0, target_fcc = 0, target_iterm = 0;
	struct fuel_gauge_dev *gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
	int fg_status = fuel_gauge_check_fg_status(gauge);
	static bool last_ffc_enable = false;
	const char *fcc_client = NULL;

	target_fcc = get_effective_result(info->fcc_votable);
	target_iterm = get_effective_result(info->iterm_votable);
	fcc_client = get_effective_client(info->fcc_votable);
	if (info->cycle_count >= 100 && info->cycle_count <= 200) {
		info->ffc_high_soc = 92;
	} else if (info->cycle_count > 200) {
		info->ffc_high_soc = 90;
	} else {
		/* do nothing*/
	}
	if (!(fg_status & FG_ERR_AUTH_FAIL) && !info->recharge && (info->current_now > 0 && info->soc < 90) && is_between(info->ffc_low_tbat, info->ffc_high_tbat, info->temp_now) &&
			((info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO && info->pd_adapter->verifed))) {
		info->rerun_ffc_enable = true;
		vote(info->iterm_votable, SUPPLEMENT_CHG_VOTER, false, 0);
		vote(info->fv_votable, SUPPLEMENT_CHG_VOTER, false, 0);
		vote(info->fcc_votable, SUPPLEMENT_CHG_VOTER, false, 0);
		pr_info("%s rerun_ffc_enable = %d\n", __func__, info->rerun_ffc_enable);
	}

	if ((!(fg_status & FG_ERR_AUTH_FAIL) && !info->recharge && (info->entry_soc <= info->ffc_high_soc || info->rerun_ffc_enable) && is_between(info->ffc_low_tbat, info->ffc_high_tbat, info->temp_now) &&
			((info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO && info->pd_adapter->verifed))) || info->suspend_recovery) {
		pr_info("%s target_fcc:%d, target_iterm:%d, ffc_enable:%d\n", __func__, target_fcc, target_iterm, info->ffc_enable);
		if (!info->ffc_enable && info->soc < 95)
			info->ffc_enable = true;
		else if (info->ffc_enable && info->soc >= 95 && (target_fcc <= (target_iterm + 200) || (info->thermal_level >= 26 && -info->current_now <= (target_iterm + 100))) && !strcmp(fcc_client, THERMAL_VOTER))
			info->ffc_enable = false;
	} else
		info->ffc_enable = false;

	if (last_ffc_enable != info->ffc_enable) {
		parse_step_charge_config(info, true);
		pr_err("%s ffc status update\n", __func__);
	}
	last_ffc_enable = info->ffc_enable;

	ret = fuel_gauge_set_fastcharge_mode(gauge, info->ffc_enable);
	if (ret < 0)
		chr_err("set fast charge mode err");

	if (info->temp_now >= info->ffc_high_tbat)
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

	if (info->ffc_enable) {
		vote(info->fv_votable, FFC_VOTER, true, info->fv_ffc - info->diff_fv_val + info->pmic_comp_v);
		vote(info->fv_votable, JEITA_CHARGE_VOTER, false, 0);
		vote(info->iterm_votable, FFC_VOTER, true, iterm_ffc - 100);
	} else if (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
		if (info->jeita_chg_index[0] == 5) {
			vote(info->fv_votable, FFC_VOTER, true, info->fv_normal  - info->diff_fv_val + info->pmic_comp_v);
		} else {
			vote(info->fv_votable, FFC_VOTER, true, info->fv  - info->diff_fv_val + info->pmic_comp_v);
		}
		vote(info->iterm_votable, FFC_VOTER, true, iterm);
	} else {
		if (info->jeita_chg_index[0] == 5) {
			vote(info->fv_votable, FFC_VOTER, true, info->fv_normal  - info->diff_fv_val + info->pmic_comp_v);
		} else {
			vote(info->fv_votable, FFC_VOTER, true, info->fv  - info->diff_fv_val + info->pmic_comp_v);
		}
		vote(info->iterm_votable, FFC_VOTER, true, iterm);
	}

	return ret;
}

static void monitor_report_fg_soc100(struct mtk_charger *info)
{
	int iterm_effective = get_effective_result(info->iterm_votable);
	int fv_effective = get_effective_result(info->fv_votable) - info->pmic_comp_v;
	static int soc100_count = 0, iterm = 0, threshold_mv = 0;
	struct fuel_gauge_dev *gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
	union power_supply_propval batt_status = {0,};
	int rsoc = fuel_gauge_get_rsoc(gauge);
	int ret = 0;

	if (!info->bat_psy) {
		chr_err("not found battery psy");
		return;
	}

	ret = power_supply_get_property(info->bat_psy, POWER_SUPPLY_PROP_STATUS, &batt_status);
	if (ret) {
		chr_err("failed to get batt status\n");
		return;
	}

	if (rsoc < 0 || rsoc >= 100 || info->temp_now > 450) {
		soc100_count = 0;
		return;
	}

	if (!(batt_status.intval == POWER_SUPPLY_STATUS_CHARGING || batt_status.intval == POWER_SUPPLY_STATUS_FULL)) {
		soc100_count = 0;
		return;
	}

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

	if (info->real_type == XMUSB350_TYPE_SDP)
			threshold_mv = 15;
	else if (info->temp_now > 0)
			threshold_mv = 25;
	else
			threshold_mv = 100;

	if ((info->vbat_now >= fv_effective - threshold_mv) &&
			((-info->current_now <= iterm * 1180/1000) || ((-info->current_now <= iterm_effective * 1180/1000 + 100) && info->bbc_charge_done))) {
			soc100_count++;
	} else {
			soc100_count = 0;
	}

	if (soc100_count >= 6) {
		ret = fuel_gauge_report_fg_soc100(gauge);
		if (ret < 0) {
			chr_err("report fg soc to 100 err");
		}
		chr_err("report fg soc to 100");
	}
}

static void check_full_recharge_EEA(struct mtk_charger *info)
{
	static int full_count = 0, iterm = 0, threshold_mv = 0, eoc_stat_count = 0;
	static int eoc_count = 0;
	int iterm_effective = get_effective_result(info->iterm_votable);
	int fv_effective = get_effective_result(info->fv_votable) - info->pmic_comp_v;
	int ret = 0, pmic_vbus = 0;
	u32 stat;
	bool is_masterCpEn = false;
	int  iterm_offset = 0;

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

	iterm_offset = min(iterm / 10, 100);

	chr_err("%s diff_fv_val = %d, iterm = %d, iterm_offset = %d, iterm_effective = %d, fv_effective = %d, full_count = %d, eoc_count=%d\n", __func__,
		info->diff_fv_val, iterm, iterm_offset, iterm_effective, fv_effective, full_count, eoc_count);
	if (info->charge_full || info->plug_in_soc100_flag) {
		full_count = 0;
		eoc_count = 0;

		if (!info->input_suspend && ((info->soc < 95 && !info->warm_term) || ((info->temp_now <= 425 || info->vbat_now <= 3950) && info->warm_term))) {
			info->charge_full = false;
			info->charge_eoc = false;
			info->warm_term = false;
			info->plug_in_soc100_flag = false;
			chr_err("start recharge EEA\n");
			if(info->real_full){
				info->recharge = true;
				info->real_full = false;
			}

			charger_dev_enable(info->chg1_dev, true);
			power_supply_changed(info->psy1);
		}
	} else {
		if (info->recharge && info->soc < 90) {
			info->recharge = false;
			pr_info("eea:soc < 90, recharge=%d\n", info->recharge);
		}
		if (info->real_type == XMUSB350_TYPE_SDP)
			threshold_mv = 15;
		else if (info->temp_now > 0)
			threshold_mv = 35;
		else
			threshold_mv = 100;
		if ((info->vbat_now >= fv_effective - threshold_mv) &&
			((-info->current_now < (iterm + iterm_offset)) || ((-info->current_now <= iterm_effective + 100) && info->bbc_charge_done))) {
			full_count++;
		} else {
			full_count = 0;
		}
		if (full_count >= 6 && !info->charge_eoc) {
			full_count = 0;
			info->charge_eoc = true;
			chr_err("charge_full notify gauge eoc\n");
			if (info->ffc_enable) {
				;//battery_set_property(BAT_PROP_CHARGE_EOC, true);yyh
			} else {
				eoc_count = 6;
			}
		}
		if (info->charge_eoc) {
			eoc_count = 6;
		} else {
			eoc_count = 0;
		}
		if (eoc_count >= 6) {
			eoc_count = 0;
			info->charge_full = true;
			info->warm_term = true;
			vote(info->fcc_votable, CP_CHG_DONE, false, 0);
			chr_err("report charge_full\n");
			if (info->ffc_enable)
				info->last_ffc_enable = true;
			if(info->jeita_chg_index[0] <= 5) {
				info->real_full = true;
				info->warm_term = false;
				charger_dev_do_event(info->chg1_dev, EVENT_FULL, 0);
				power_supply_changed(info->psy1);
				if (info->ffc_enable && info->cycle_count > 300 && info->supplement_chg_status) {
					chr_err("%s EEA charge full at ffc mode, ready to supplement charge\n", __func__);
					mod_timer(&info->supplement_charge_timer, jiffies + msecs_to_jiffies(300000));
					info->supplement_chg_status = false;
				}
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
					chr_err("report charge_full: request 5V/3A:%d\n", ret);
					ret = adapter_dev_set_cap_xm(info->pd_adapter, MTK_PD_APDO, 5000, 3000);
					if (ret == MTK_ADAPTER_ERROR || ret == MTK_ADAPTER_ADJUST) {
						adapter_dev_set_cap_xm(info->pd_adapter, MTK_PD_APDO, 5000, 2000);
						chr_err("request 5V/3A fail, retry vbus=%d ibus=%d\n", 5000, 2000);
					}
					charger_dev_set_mivr(info->chg1_dev, info->data.min_charger_voltage);
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

static void check_full_recharge(struct mtk_charger *info)
{
	static int full_count = 0, iterm = 0, threshold_mv = 0, eoc_stat_count = 0;
	static int eoc_count = 0;
	int iterm_effective = get_effective_result(info->iterm_votable);
	int fv_effective = get_effective_result(info->fv_votable) - info->pmic_comp_v;
	int ret = 0, pmic_vbus = 0;
	u32 stat;
	bool is_masterCpEn = false;
	struct fuel_gauge_dev *gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
	int fg_status = 0;
	int raw_soc = 0;
	int iterm_offset = 0;

	fg_status = fuel_gauge_check_fg_status(gauge);
	raw_soc = fuel_gauge_get_raw_soc(gauge);

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

	iterm_offset = min(iterm / 10, 100);

	chr_err("%s diff_fv_val = %d, iterm = %d, iterm_offset = %d, iterm_effective = %d, fv_effective = %d, full_count = %d, eoc_count=%d\n", __func__,
		info->diff_fv_val, iterm, iterm_offset, iterm_effective, fv_effective, full_count, eoc_count);
	if (info->charge_full) {
		full_count = 0;
		eoc_count = 0;
		if (!info->input_suspend && ((raw_soc < 9700 && !info->warm_term) || ((info->temp_now <= 425 || info->vbat_now <= 3950) && info->warm_term))) {
			info->charge_full = false;
			info->charge_eoc = false;
			info->warm_term = false;
			chr_err("%s start recharge\n", __func__);
			if(info->real_full){
				info->recharge = true;
				info->real_full = false;
			}

			charger_dev_enable(info->chg1_dev, true);
			power_supply_changed(info->psy1);
		}
	} else {
		if (info->recharge && info->soc < 90) {
			info->recharge = false;
			pr_info("soc < 90, recharge=%d\n", info->recharge);
		}
		if (info->real_type == XMUSB350_TYPE_SDP)
			threshold_mv = 15;
		else if (info->temp_now > 0)
			threshold_mv = 35;
		else
			threshold_mv = 100;
		if ((info->vbat_now >= (fv_effective - threshold_mv)) &&
			((-info->current_now < (iterm + iterm_offset)) || ((-info->current_now <= iterm_effective + 100) && info->bbc_charge_done)) &&
				(!((fg_status & FG_ERR_AUTH_FAIL) || (fg_status & FG_EER_I2C_FAIL) || (fg_status & FG_ERR_CHG_WATT)))) {
			full_count++;
		} else {
			full_count = 0;
		}
		if (full_count >= 6 && !info->charge_eoc) {
			full_count = 0;
			info->charge_eoc = true;
			chr_err("%s charge_full notify gauge eoc\n", __func__);
			if (info->ffc_enable) {
				;//battery_set_property(BAT_PROP_CHARGE_EOC, true);yyh
			} else {
				eoc_count = 6;
			}
		}
		if (info->charge_eoc) {
			eoc_count = 6;
		} else {
			eoc_count = 0;
		}
		if (eoc_count >= 6) {
			eoc_count = 0;
			info->charge_full = true;
			info->warm_term = true;
			vote(info->fcc_votable, CP_CHG_DONE, false, 0);
			chr_err("%s report charge_full\n", __func__);
			if (info->ffc_enable)
				info->last_ffc_enable = true;
			if(info->jeita_chg_index[0] <= 5) {
				info->real_full = true;
				info->warm_term = false;
				charger_dev_do_event(info->chg1_dev, EVENT_FULL, 0);
				power_supply_changed(info->psy1);
				if (info->ffc_enable && info->supplement_chg_status) {
					chr_err("%s charge full at ffc mode, ready to supplement charge\n", __func__);
					mod_timer(&info->supplement_charge_timer, jiffies + msecs_to_jiffies(300000));
					info->supplement_chg_status = false;
				}
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
				chr_err("%s report charge_full: set DP:0, DM:0 for setting 5V\n", __func__);
			} else {
				charger_dev_is_enabled(info->cp_master, &is_masterCpEn);
				pmic_vbus = get_vbus(info);
				chr_err("%s cp_master:%d, pmic_vbus:%d\n", __func__, is_masterCpEn, pmic_vbus);
				if(!is_masterCpEn && pmic_vbus > 2500){
					chr_err("%s report charge_full: request 5V/3A:%d\n", __func__, ret);
					ret = adapter_dev_set_cap_xm(info->pd_adapter, MTK_PD_APDO, 5000, 3000);
					if (ret == MTK_ADAPTER_ERROR || ret == MTK_ADAPTER_ADJUST) {
						adapter_dev_set_cap_xm(info->pd_adapter, MTK_PD_APDO, 5000, 2000);
						chr_err("%s request 5V/3A fail, retry vbus=%d ibus=%d\n", __func__, 5000, 2000);
					}
					charger_dev_set_mivr(info->chg1_dev, info->data.min_charger_voltage);
				}
			}

			info->recharge = false;
			charger_dev_enable(info->chg1_dev, false);
		}

		ret = charger_dev_get_charge_ic_stat(info->chg1_dev, &stat);
		if(ret<0)
			chr_err("%s read F_IC_STAT failed\n", __func__);
		else
			chr_err("%s read F_IC_STAT success stat=%d\n", __func__, stat);

		if(stat == CHG_STAT_EOC)
			eoc_stat_count++;
		else
			eoc_stat_count = 0;
		chr_err("%s eoc_stat_count=%d\n", __func__, eoc_stat_count);

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
				chr_err("%s failed to reset eoc stat\n", __func__);
			charger_dev_enable_termination(info->chg1_dev, true);
			atomic_set(&info->ieoc_wkrd, 1);
			chr_err("%s enter compensate cv=%d, vbat=%d, comp_cv=%d\n", __func__, fv_effective, info->vbat_now, info->pmic_comp_v);
		} else if(info->vbat_now > fv_effective && atomic_read(&info->ieoc_wkrd)) {
			info->pmic_comp_v = 0;
			chr_err("%s cancle compensate fv_effective=%d\n", __func__, fv_effective);
		}
	}
}

static void check_charge_data(struct mtk_charger *info)
{
	union power_supply_propval pval = {0,};
	int ret = 0;
	//struct mtk_battery *battery_drvdata;

	if (!info->bat_psy) {
		info->bat_psy = power_supply_get_by_name("battery");
		chr_err("failed to get bat_psy\n");
		return;
	}
	/*
	if(info->bat_psy){
		battery_drvdata = power_supply_get_drvdata(info->bat_psy);
		info->smart_chg = battery_drvdata->smart_chg;
		chr_err("[XMCHG_MONITOR] set mtk_charger smart_chg done!\n");
	}
	*/
	if (!info->cp_master) {
		info->cp_master = get_charger_by_name("cp_master");
		chr_err("failed to get master cp charger\n");
		return;
	}

	charger_dev_is_enabled(info->chg1_dev, &info->bbc_charge_enable);
	charger_dev_is_charging_done(info->chg1_dev, &info->bbc_charge_done);
	ret = power_supply_get_property(info->bat_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret)
		chr_err("failed to get soc\n");
	else
		info->soc = pval.intval;

	ret = power_supply_get_property(info->bat_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret)
		chr_err("failed to get vbat\n");
	else
		info->vbat_now = pval.intval / 1000;

	ret = power_supply_get_property(info->bat_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret)
		chr_err("failed to get ibat\n");
	else
		info->current_now = pval.intval / 1000;

	ret = power_supply_get_property(info->bat_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret)
		chr_err("failed to get tbat\n");
	else
		info->temp_now = pval.intval;

	ret = power_supply_get_property(info->bat_psy, POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT, &pval);
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

	chr_err("diff_fv_val:%d, and will clear diff_fv_val as 0\n", info->diff_fv_val);
	info->diff_fv_val = 0;
        /* FFC fv cyc */
	if (info->ffc_enable) {
		if (info->cycle_count >= 101 && info->cycle_count <= 300) {
			info->diff_fv_val =  20;
		} else if (info->cycle_count >= 301 && info->cycle_count <= 800) {
			info->diff_fv_val =  50;
		} else if (info->cycle_count > 800) {
			info->diff_fv_val = 60;
		} else {
			/* do nothing*/
		}
	} else {  /* normal fv cyc */
		if (info->cycle_count >= 101 && info->cycle_count <= 300) {
			info->diff_fv_val = 10;
		} else if (info->cycle_count >= 301 && info->cycle_count <= 800) {
			info->diff_fv_val = 20;
		} else if (info->cycle_count > 800) {
			info->diff_fv_val = 40;
		} else {
			/* do nothing*/
		}
	}

	if (info->cycle_count < 101)
		info->diff_fv_val = max(info->isc_diff_fv, info->set_smart_batt_diff_fv);
	else
		info->diff_fv_val = info->diff_fv_val + info->isc_diff_fv;
	if (info->pd_adapter) {
		chr_err("[CHARGE_LOOP] TYPE = [%d %d %d], BMS = [%d %d %d %d], FULL = [%d %d %d %d], thermal_level=%d, FFC = %d, sw_cv=%d, warm_term=%d, smart_batt:%d\n",
			info->pd_type, info->pd_adapter->verifed, info->real_type, info->soc, info->vbat_now, info->current_now, info->temp_now,
			info->bbc_charge_enable, info->charge_full, info->bbc_charge_done, info->recharge, info->thermal_level, info->ffc_enable,
			info->sw_cv, info->warm_term, info->set_smart_batt_diff_fv);
	}
	if (info->input_suspend || info->typec_burn)
		chr_err("[CHARGE_LOOP] input_suspend = %d, typec_burn = %d\n", info->input_suspend, info->typec_burn);
}

static void __maybe_unused monitor_smart_chg(struct mtk_charger *info)
{
	if(info == NULL || !info->fcc_votable || !info->smart_chg)
		return;
}

static void monitor_fv_descent(struct mtk_charger *info)
{
	bool isEffective = false;
	int fv_vote = 0, fv_vote_dec = 0, ffc_fv_vote = 0, jeita_fv_vote = 0, fg_err_vote = 0;
	static int last_fv_vote = 0, last_ffc_fv_vote = 0, last_jeita_fv_vote = 0, last_fg_err_vote = 0;

	if(!info->cp_sm_run_state){
		isEffective = is_client_vote_enabled(info->fv_votable, FV_DEC_VOTER);
		ffc_fv_vote = get_client_vote(info->fv_votable, FFC_VOTER);
		jeita_fv_vote = get_client_vote(info->fv_votable, JEITA_CHARGE_VOTER);
		fg_err_vote = get_client_vote(info->fv_votable, FG_ERR_VOTER);
		if(!isEffective){
			fv_vote = get_effective_result(info->fv_votable);
			if(info->vbat_now >= (fv_vote - 7)){
				vote(info->fv_votable, FV_DEC_VOTER, true, fv_vote - 10);
				chr_err("[%s] fv vote reduce cv 10 mv:%d\n", __func__, isEffective);
			}else{
				vote(info->fv_votable, FV_DEC_VOTER, false, 0);
			}
		}else if(isEffective &&
			(last_ffc_fv_vote != ffc_fv_vote || last_jeita_fv_vote != jeita_fv_vote ||
				last_fg_err_vote != fg_err_vote)){
			fv_vote_dec = get_effective_result(info->fv_votable);
			vote(info->fv_votable, FV_DEC_VOTER, false, 0);
			fv_vote = get_effective_result(info->fv_votable);
			if(last_fv_vote == fv_vote)
				vote(info->fv_votable, FV_DEC_VOTER, true, fv_vote_dec);
			else if(info->vbat_now >= (fv_vote - 7)){
				vote(info->fv_votable, FV_DEC_VOTER, true, fv_vote - 10);
				chr_err("[%s] fv vote reduce cv 10 mv:%d\n", __func__, isEffective);
			}else{
				vote(info->fv_votable, FV_DEC_VOTER, false, 0);
				chr_err("[%s] cancel fv vote reduce cv 10 mv:%d\n", __func__, isEffective);
			}
		}
		chr_err("[%s][%d]  fv_vote[%d,%d,%d], FFC[%d, %d], jeita[%d,%d], fg_err[%d, %d], isEffective:%d\n",
					__func__, info->cp_sm_run_state, fv_vote, last_fv_vote, fv_vote_dec, 
					ffc_fv_vote, last_ffc_fv_vote, jeita_fv_vote, last_jeita_fv_vote, fg_err_vote, last_fg_err_vote, isEffective);
		last_fv_vote = fv_vote;
		last_ffc_fv_vote = ffc_fv_vote;
		last_jeita_fv_vote = jeita_fv_vote;
		last_fg_err_vote = fg_err_vote;
	}else{
		last_fv_vote = last_ffc_fv_vote = last_jeita_fv_vote = last_fg_err_vote = 0;
		vote(info->fv_votable, FV_DEC_VOTER, false, 0);
	}
}

static void charge_monitor_func(struct work_struct *work)
{
	struct mtk_charger *info = container_of(work, struct mtk_charger, charge_monitor_work.work);
	check_charge_data(info);
	handle_ffc_charge(info);
	monitor_report_fg_soc100(info);
	if (info->product_name_index == EEA) {
		check_full_recharge_EEA(info);
	} else {
		check_full_recharge(info);
	}

	monitor_thermal_limit(info);
	handle_step_charge(info);
	handle_jeita_charge(info);
	//monitor_smart_chg(info);
	monitor_sw_cv(info);
	monitor_jeita_descent(info);
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

	/* step parameter */
	if (info->ffc_enable){
		ret = snprintf(name, sizeof(name), "step_chg_cfg_%d_cycle_%s_%s", cycle, info->area_name, info->batt_vendor);
		if (ret >= sizeof(name))
			chr_err("%s: type_c name is truncated\n", __func__);
	} else {
		ret = snprintf(name, sizeof(name), "step_chg_cfg_%d_cycle_normal_%s_%s", cycle, info->area_name, info->batt_vendor);
		if (ret >= sizeof(name))
			chr_err("%s: type_c name is truncated\n", __func__);
	}
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
	ret = snprintf(name, sizeof(name), "iterm_ffc_%d_cycle_%s_%s", cycle, info->area_name, info->batt_vendor);
	if (ret >= sizeof(name))
		chr_err("%s: type_c name is truncated\n", __func__);
	chr_err("%s: parse %s\n", __func__, name);

	if (of_property_read_u32(np, name, &val) >= 0)
		info->iterm_ffc = val;
	else {
		chr_err("failed to parse %s use default\n", name);
		info->iterm_ffc = 700;
	}

	ret = snprintf(name, sizeof(name), "iterm_ffc_little_warm_%d_cycle_%s_%s", cycle, info->area_name, info->batt_vendor);
	if (ret >= sizeof(name))
		chr_err("%s: type_c name is truncated\n", __func__);
	chr_err("%s: parse %s\n", __func__, name);
	if (of_property_read_u32(np, name, &val) >= 0)
		info->iterm_ffc_little_warm = val;
	else {
		chr_err("failed to parse %s use default\n", name);
		info->iterm_ffc_little_warm = 800;
	}

	ret = snprintf(name, sizeof(name), "iterm_ffc_warm_%d_cycle_%s_%s", cycle, info->area_name, info->batt_vendor);
	if (ret >= sizeof(name))
		chr_err("%s: type_c name is truncated\n", __func__);
	chr_err("%s: parse %s\n", __func__, name);
	if (of_property_read_u32(np, name, &val) >= 0)
		info->iterm_ffc_warm = val;
	else {
		chr_err("failed to parse %s use default\n", name);
		info->iterm_ffc_warm = 800;
	}

	ret = snprintf(name, sizeof(name), "iterm_ffc_hot_%d_cycle_%s_%s", cycle, info->area_name, info->batt_vendor);
	if (ret >= sizeof(name))
		chr_err("%s: type_c name is truncated\n", __func__);
	chr_err("%s: parse %s\n", __func__, name);
	if (of_property_read_u32(np, name, &val) >= 0)
		info->iterm_ffc_hot = val;
	else {
		chr_err("failed to parse %s use default\n", name);
		info->iterm_ffc_hot = 800;
	}

	ret = snprintf(name, sizeof(name), "supplement_charge_ffc_%d_cycle_%s", cycle, info->area_name);
	if (ret >= sizeof(name))
		chr_err("%s: supplement_charge name is truncated\n", __func__);
	chr_err("%s: parse %s\n", __func__, name);
	total_length = of_property_count_elems_of_size(np, name, sizeof(u32));
	if (total_length < 0) {
		chr_err("failed to read total_length of config\n");
	} else {
		ret = of_property_read_u32_array(np, name, (u32 *)info->supplement_charge_cfg, total_length);
		if (ret)
			chr_err("failed to parse %s\n", name);
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

	if (!info->bat_psy)
		info->bat_psy = power_supply_get_by_name("battery");

	if (info->bat_psy) {
		ret = power_supply_get_property(info->bat_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
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
	} else if (cycle_count > 100 && cycle_count <= 300) {
		battery_cycle = BATTERY_CYCLE_100_TO_300;
	} else if (cycle_count > 300 && cycle_count <= 800) {
		battery_cycle = BATTERY_CYCLE_300_TO_800;
	} else {
		battery_cycle = BATTERY_CYCLE_UP_800;
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
	get_index(info->step_chg_cfg, info->step_fallback_hyst, info->step_forward_hyst, info->vbat_now, info->step_chg_index, true, STEP_JEITA_TUPLE_COUNT);
	info->step_chg_fcc = info->step_chg_cfg[info->step_chg_index[0]].value;
	vote(info->fcc_votable, STEP_CHARGE_VOTER, true, info->step_chg_fcc);
	get_index(info->jeita_fv_cfg, info->jeita_fallback_hyst, info->jeita_forward_hyst, info->temp_now, info->jeita_chg_index, true, JEITA_TUPLE_COUNT);

	if (info->vbat_now < info->jeita_fcc_cfg[info->jeita_chg_index[0]].extra_threshold)
		info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].low_value;
	else
		info->jeita_chg_fcc = info->jeita_fcc_cfg[info->jeita_chg_index[0]].high_value;
	vote(info->fcc_votable, JEITA_CHARGE_VOTER, true, info->jeita_chg_fcc);
	chr_err(" fcc:%d,low_value:%d,high_value:%d",info->jeita_chg_fcc,
	info->jeita_fcc_cfg[info->jeita_chg_index[0]].low_value, info->jeita_fcc_cfg[info->jeita_chg_index[0]].high_value);
	vote(info->iterm_votable, SUPPLEMENT_CHG_VOTER, false, 0);
	vote(info->fv_votable, SUPPLEMENT_CHG_VOTER, false, 0);
	vote(info->fcc_votable, SUPPLEMENT_CHG_VOTER, false, 0);
	parse_step_charge_config(info, false);
}

static void supplement_charge_policy(struct work_struct *work)
{
	struct mtk_charger *info = container_of(work, struct mtk_charger, supplement_charge_work.work);

	if (!(info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO && info->pd_adapter->verifed)
		|| info->recharge || info->vbat_now >= info->supplement_charge_cfg[1]) {
		chr_err("vbat_now is %d,supplement_charge is %d, stop supplement charge\n",info->vbat_now, info->supplement_charge_cfg[1]);
		return;
	}

	vote(info->iterm_votable, SUPPLEMENT_CHG_VOTER, true, info->supplement_charge_cfg[2]);
	vote(info->fv_votable, SUPPLEMENT_CHG_VOTER, true, info->supplement_charge_cfg[1]);
	vote(info->fcc_votable, SUPPLEMENT_CHG_VOTER, true, info->supplement_charge_cfg[0]);
	info->charge_full = false;
	info->charge_eoc = false;
        info->warm_term = false;
	chr_err("start supplement charge\n");
	info->recharge = true;
	info->real_full = false;
	charger_dev_enable(info->chg1_dev, true);
	power_supply_changed(info->psy1);
}

static void supplement_charge_init(struct timer_list *t)
{
	struct mtk_charger *info = from_timer(info, t, supplement_charge_timer);

	schedule_delayed_work(&info->supplement_charge_work, msecs_to_jiffies(0));
}

int step_jeita_init(struct mtk_charger *info, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int total_length = 0, i = 0, ret = 0;
	union power_supply_propval pval = {0,};
	char parse_bufer[22] = {0};

	if (!np) {
		chr_err("no device node\n");
		return -EINVAL;
	}

	if (info->bat_psy) {
		ret = power_supply_get_property(info->bat_psy, POWER_SUPPLY_PROP_MANUFACTURER, &pval);
		if (ret < 0) {
			chr_err("failed to read battery info from fg\n");
			strcpy(info->area_name, "jn");
			strcpy(info->batt_vendor, "nvt");
		} else {
			strcpy(info->batt_info, pval.strval);
			if (strstr(info->batt_info, "cn") != NULL)
				strcpy(info->area_name, "cn");
			else if (strstr(info->batt_info, "gl") != NULL)
				strcpy(info->area_name, "gl");
			else if (strstr(info->batt_info, "jn") != NULL)
				strcpy(info->area_name, "jn");
			else
				strcpy(info->area_name, "jn");

			if (strstr(info->batt_info, "swd") != NULL)
				strcpy(info->batt_vendor, "swd");
			else if (strstr(info->batt_info, "nvt") != NULL)
				strcpy(info->batt_vendor, "nvt");
			else if (strstr(info->batt_info, "cos") != NULL)
				strcpy(info->batt_vendor, "cos");
			else
				strcpy(info->batt_vendor, "nvt");
		}
	} else {
		chr_err("battery psy not found\n");
		strcpy(info->area_name, "jn");
		strcpy(info->batt_vendor, "nvt");
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

#if IS_ENABLED(CONFIG_XM_SMART_CHG)
	total_length = of_property_count_elems_of_size(np, "thermal_limit_smartchg", sizeof(u32));
	if (total_length < 0) {
		chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}
	ret = of_property_read_u32_array(np, "thermal_limit_smartchg", (u32 *)(info->thermal_limit[6]), total_length);
	if (ret) {
		chr_err("failed to parse thermal_limit_smartchg\n");
		return ret;
	}
	for (i = 0; i < THERMAL_LIMIT_COUNT; i++) {
		chr_info("thermal_limit_smartchg %d\n", info->thermal_limit[6][i]);
		if (info->thermal_limit[6][i] > MAX_THERMAL_FCC || info->thermal_limit[6][i] < MIN_THERMAL_FCC) {
			chr_err("thermal_limit_smartchg over range\n");
			return -1;
		}
		if (i != 0) {
			if (info->thermal_limit[6][i] > info->thermal_limit[6][i - 1]) {
				chr_err("thermal_limit_smartchg order error\n");
				return -1;
			}
		}
	}
#endif

	info->cycle_count = 0;
	ret = parse_step_charge_config(info, true);
	if (ret) {
		chr_err("failed to parse step_chg_cfg\n");
		return ret;
	}

	snprintf(parse_bufer, sizeof(parse_bufer), "jeita_fcc_cfg_%s", info->area_name);
	total_length = of_property_count_elems_of_size(np, parse_bufer, sizeof(u32));
	if (total_length < 0) {
		chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, parse_bufer, (u32 *)info->jeita_fcc_cfg, total_length);
	if (ret) {
		chr_err("failed to parse %s\n", parse_bufer);
		return ret;
	}
	for (i = 0; i < JEITA_TUPLE_COUNT; i++)
		chr_info("JEITA_FCC %d %d %d %d %d\n", info->jeita_fcc_cfg[i].low_threshold, info->jeita_fcc_cfg[i].high_threshold, info->jeita_fcc_cfg[i].extra_threshold, info->jeita_fcc_cfg[i].low_value, info->jeita_fcc_cfg[i].high_value);


	snprintf(parse_bufer, sizeof(parse_bufer), "iterm_%s", info->area_name);
	if (of_property_read_u32(np, parse_bufer, &info->iterm) < 0) {
		chr_err("failed to parse iterm use default\n");
		info->iterm = 200;
	}
	snprintf(parse_bufer, sizeof(parse_bufer), "iterm_warm_%s", info->area_name);
	if (of_property_read_u32(np, parse_bufer, &info->iterm_warm) < 0) {
		chr_err("failed to parse iterm use default\n");
		info->iterm_warm = info->iterm;
	}

	snprintf(parse_bufer, sizeof(parse_bufer), "jeita_fv_cfg_%s", info->area_name);
	total_length = of_property_count_elems_of_size(np, parse_bufer, sizeof(u32));
	if (total_length < 0) {
		chr_err("failed to read total_length of config\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, parse_bufer, (u32 *)info->jeita_fv_cfg, total_length);
	if (ret) {
		chr_err("failed to parse %s\n", parse_bufer);
		return ret;
	}

	for (i = 0; i < JEITA_TUPLE_COUNT; i++)
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

	info->smart_snschg_support =  of_property_read_bool(np, "smart_sensing_chg_support");
	if (!info->smart_snschg_support)
		chr_err("failed to parse smart_snschg_support\n");
	info->sic_support =  of_property_read_bool(np, "sic_support");

	if (!info->sic_support)
		chr_err("failed to parse sic_support\n");

	INIT_DELAYED_WORK(&info->charge_monitor_work, charge_monitor_func);
	INIT_DELAYED_WORK(&info->supplement_charge_work, supplement_charge_policy);
	info->supplement_charge_timer.expires = jiffies + msecs_to_jiffies(50000);
	timer_setup(&info->supplement_charge_timer, supplement_charge_init, 0);

	return ret;
}
