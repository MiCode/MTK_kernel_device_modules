// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
 */
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/slab.h>

#include "hq_printk.h"
#include "hq_voter.h"
#include "hq_jeita.h"

#ifdef TAG
#undef TAG
#define TAG "[HQ_CHG_JEITA]"
#endif


#define is_between(left, right, value) \
		(((left) >= (right) && (left) >= (value) \
			&& (value) >= (right)) \
		|| ((left) <= (right) && (left) <= (value) \
			&& (value) <= (right)))

struct ffc_cfg {
	int low_threshold;
	int high_threshold;
	int ffc_ibat_1;
	int ffc_vbat_1;
	int ffc_ibat_2;
	int ffc_vbat_2;
	int ffc_ibat_3;
	int ffc_vbat_3;
	int ffc_ibat_4;
	int ffc_iterm_1;
	int ffc_iterm_2;
	int ffc_iterm_3;
	int ffc_fv;
};

struct normal_cfg {
	int low_threshold;
	int high_threshold;
	int normal_ibat_1;
	int normal_vbat_1;
	int normal_ibat_2;
	int normal_iterm_1;
	int normal_iterm_2;
	int normal_iterm_3;
	int normal_fv;
};

struct rechg_fv_offset_cfg {
	int temp_level_l;
	int temp_level_h;
	int value;
};

struct chg_cycle_data {
	int cycle_array_size;
	int *cycle_data;
	int temp_array_size;
	int *temp_data;
	int **vol_data;
	int vol_array_rows;
	int vol_array_cols;
	int **curr_data;
	int curr_array_rows;
	int curr_array_cols;
	int last_curr_value;
	int last_vol_index;
	int last_temp_index;
};

struct hq_jeita_info {
	struct device		*dev;
	ktime_t			jeita_last_update_time;
	bool			config_is_read;
	bool			sw_jeita_cfg_valid;
	bool			batt_missing;
	bool			taper_fcc;
	int			get_config_retry_count;

	bool			pd_verifed;
	bool			is_charge_done;
#if IS_ENABLED(CONFIG_HUAQIN_SOFT_ITERM_SUPPORT)
	/*soft iterm*/
	int			soft_iterm_threshold;
	int			soft_fv_threshold;
	int			soft_soc_threshold;
	int			hard_iterm;
	int			soft_soc;
	int			soft_vbat;
	int			soft_ibat;
	int			soft_avg_ibat;
	int			soft_chg_status;
	int			soft_temp;
	int			soft_cc;
	int			soft_rechg_offset;
#endif

	struct wakeup_source	*hq_jeita_ws;

	int rechg_fv_offset_data_cnt;
	struct rechg_fv_offset_cfg *rechg_fv_offset_data;

	/* jeita config */
	struct ffc_cfg jeita_ffc_cfg[MAX_FFC_JEITA_NUM];
	struct normal_cfg jeita_normal_cfg[MAX_STEP_JEITA_NUM];
	int			jeita_normal_cnt;
	int			jeita_ffc_cnt;
	int			normal_jeita_index;
	int			ffc_jeita_index;
	int			normal_ibat;
	int			ffc_ibat;
	int			iterm_curr;
	int			fv;

	/* voter add here */
	struct votable *main_fcc_votable;
	struct votable *fv_votable;
	struct votable *main_icl_votable;
	struct votable *iterm_votable;
	struct votable *total_fcc_votable;

	/* psy add here */
	struct power_supply	*batt_psy;
	struct power_supply	*bms_psy;
	struct power_supply	*usb_psy;
	#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
	struct adapter_device *pd_adapter;
	#endif
	struct fuel_gauge_dev *fuel_gauge;
	struct charger_dev *charger;
	struct auth_device *auth_dev;

	struct delayed_work	status_change_work;
	struct delayed_work	get_config_work;
	struct chg_cycle_data *ffc_cycle_data;
	struct chg_cycle_data *normal_cycle_data;

	struct notifier_block nb;
};

static struct hq_jeita_info *the_chip;
static bool warm_stop_charge;

bool get_warm_stop_charge_state(void)
{
	return warm_stop_charge;
}
EXPORT_SYMBOL(get_warm_stop_charge_state);

static bool is_auth_dev_available(struct hq_jeita_info *chip)
{
	#if IS_ENABLED(CONFIG_ONEWIRE_GPIO)
	if (!chip->auth_dev) {
		chip->auth_dev = get_batt_auth_by_name("secret_ic");
	}
	#endif

	if (!chip->auth_dev)
		return false;

	return true;
}

static bool is_batt_available(struct hq_jeita_info *chip)
{
	if (!chip->batt_psy)
		chip->batt_psy = power_supply_get_by_name("battery");

	if (!chip->batt_psy)
		return false;

	return true;
}

static bool is_bms_available(struct hq_jeita_info *chip)
{
	if (!chip->bms_psy)
		chip->bms_psy = power_supply_get_by_name("bms");

	if (!chip->bms_psy)
		return false;

	return true;
}

static bool is_input_present(struct hq_jeita_info *chip)
{
	int rc = 0, input_present = 0;
	union power_supply_propval pval = {0, };

	if (!chip->usb_psy)
		chip->usb_psy = power_supply_get_by_name("usb");
	if (chip->usb_psy) {
		rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &pval);
		if (rc < 0)
			hq_err("Couldn't read USB Present status, rc=%d\n", rc);
		else
			input_present |= pval.intval;
	}

	if (input_present)
		return true;

	return false;
}

static int hq_jeita_get_normal_index(struct normal_cfg *cfg, int cfg_cnt, int value)
{
	int new_index = 0, i = 0;
	int index = 0;

	if (value < cfg[0].low_threshold) {
		index = 0;
		return index;
	}

	if (value > cfg[cfg_cnt - 1].high_threshold)
		new_index = cfg_cnt - 1;

	for (i = 0; i < cfg_cnt; i++) {
		if (is_between(cfg[i].low_threshold, cfg[i].high_threshold, value)) {
			new_index = i;
			break;
		}
	}
	index = new_index;

	return index;
}

static int hq_jeita_get_ffc_index(struct ffc_cfg *cfg, int cfg_cnt, int value)
{
	int new_index = 0, i = 0;
	int index = 0;

	if (value < cfg[0].low_threshold) {
		index = 0;
		return index;
	}

	if (value > cfg[cfg_cnt - 1].high_threshold)
		new_index = cfg_cnt - 1;

	for (i = 0; i < cfg_cnt; i++) {
		if (is_between(cfg[i].low_threshold, cfg[i].high_threshold, value)) {
			new_index = i;
			break;
		}
	}
	index = new_index;

	return index;
}

static void judge_warm_stop_charge(int temp, int vbat)
{
	if (temp >= TEMP_LEVEL_45 && vbat >= TEMP_45_TO_58_VOL)
		warm_stop_charge = true;

	if (warm_stop_charge) {
		if (temp < (TEMP_LEVEL_45 - WARM_RECHG_TEMP_OFFSET) || vbat < (TEMP_45_TO_58_VOL - WARM_RECHG_VOLT_OFFSET))
			warm_stop_charge = false;
	}
}

static int charge_done(struct charger_manager *manager)
{
	if(manager->pd_active != 0) {
		if (IS_ERR_OR_NULL(g_policy->adapter)) {
			hq_err("g_policy->adapter is_err_or_null\n");
			return -EINVAL;
		}
		adapter_set_cap(g_policy->adapter, g_policy->cap_nr, 5000, 2000);
	} else if(manager->vbus_type == VBUS_TYPE_HVDCP) {
		charger_set_qc_term_vbus(manager->charger);
	}

	return 0;
}

static int handle_jeita(struct hq_jeita_info *chip)
{
	union power_supply_propval pval = {0, };
	struct charger_manager *manager;
	int ret = 0;
	int temp_now, vol_now, curr_now, batt_id;
	static bool normal_curr_shake = false;
	static bool ffc_curr_shake = false;
	enum vbus_type vbus_type;

	if (!is_batt_available(chip)) {
		hq_err("failed to get batt psy\n");
		return -EINVAL;
	}

	if (!is_auth_dev_available(chip)) {
		hq_err("failed to get auth device, defalut batt id = 0\n");
		batt_id = 0;
		//return -EINVAL;
	} else {
		batt_id = chip->auth_dev->battery_id;
	}

	#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
	chip->pd_adapter = get_adapter_by_name("pd_adapter");
	if (!chip->pd_adapter)
		hq_err("failed to pd_adapter\n");
	else
		chip->pd_verifed = chip->pd_adapter->verifed;
	#endif
	if (chip->fuel_gauge == NULL)
		chip->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
	if (IS_ERR_OR_NULL(chip->fuel_gauge)) {
		hq_err("failed to get fuel_gauge\n");
		return -EINVAL;
	}

	manager = (struct charger_manager *)power_supply_get_drvdata(chip->batt_psy);
	if (IS_ERR_OR_NULL(manager)) {
		hq_err("get charger manager fail\n");
		return -EINVAL;
        }

	ret = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
	if (ret < 0) {
		hq_err("Couldn't read status, ret=%d\n", ret);
	}
	if (pval.intval == POWER_SUPPLY_STATUS_DISCHARGING) {
		normal_curr_shake = false;
		ffc_curr_shake = false;
		return 0;
	}

	ret = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		hq_err("Couldn't read batt temp, ret=%d\n", ret);
	}
	temp_now = pval.intval;

	ret = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret < 0) {
		hq_err("Couldn't read batt voltage_now, ret=%d\n", ret);
	}
	vol_now = pval.intval / 1000;

	ret = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret < 0) {
		hq_err("Couldn't read batt current_now, ret=%d\n", ret);
	}
	curr_now = -pval.intval;

	if (temp_now < 0) {
		charger_set_rechg_volt(chip->charger, LOW_TEMP_RECHG_OFFSET);
	} else {
		charger_set_rechg_volt(chip->charger, NOR_TEMP_RECHG_OFFSET);
	}

	judge_warm_stop_charge(temp_now, vol_now);
	charger_is_charge_done(chip->charger, &(chip->is_charge_done));

	if (temp_now <= TEMP_LEVEL_NEGATIVE_10 || temp_now >= TEMP_LEVEL_58) {
		vote(chip->total_fcc_votable, JEITA_VOTER, true, 0);
		hq_info("temp_now < -10 or temp_now >= 58, stop charge");
		return 0;
	}

	if (warm_stop_charge) {
		vote(chip->total_fcc_votable, JEITA_VOTER, true, 0);
		hq_info("warm_stop_charge = true, stop charge");
		return 0;
	}

	/*nromal: ibat*/
	chip->normal_jeita_index = hq_jeita_get_normal_index(chip->jeita_normal_cfg, chip->jeita_normal_cnt, temp_now);
	if (chip->normal_jeita_index == INDEX_15_to_35 || chip->normal_jeita_index == INDEX_35_to_48) {
		//vbat < 4.1V and 15 < temp <45，ichg = 8A
		if (vol_now < chip->jeita_normal_cfg[chip->normal_jeita_index].normal_vbat_1) {
			if (normal_curr_shake) { //if limited,need vbat below 4V,than set ichg = 8A
				if ((vol_now < (chip->jeita_normal_cfg[chip->normal_jeita_index].normal_vbat_1 - COLD_RECHG_VOLT_OFFSET))) {
					chip->normal_ibat = chip->jeita_normal_cfg[chip->normal_jeita_index].normal_ibat_1;
					normal_curr_shake = false;
				}
			} else {
				chip->normal_ibat = chip->jeita_normal_cfg[chip->normal_jeita_index].normal_ibat_1;
			}
		} else { //vbat >= 4.1V, ichg = 5.4A
			chip->normal_ibat = chip->jeita_normal_cfg[chip->normal_jeita_index].normal_ibat_2;
			normal_curr_shake = true;
		}
	} else {
		chip->normal_ibat = chip->jeita_normal_cfg[chip->normal_jeita_index].normal_ibat_1;
	}

	/*ffc: ibat*/
	chip->ffc_jeita_index = hq_jeita_get_ffc_index(chip->jeita_ffc_cfg, chip->jeita_ffc_cnt, temp_now);
	if(chip->pd_verifed && !manager->en_single_cell) {
		if (temp_now > TEMP_LEVEL_15 && temp_now <= TEMP_LEVEL_45) {
			//vbat < 4.1V and 15 < temp <45，ichg = 8A
			if (vol_now <= chip->jeita_ffc_cfg[chip->ffc_jeita_index].ffc_vbat_1) {
				if (ffc_curr_shake) { //if limited,need vbat below 4V,than set ichg = 8A
					if ((vol_now < (chip->jeita_ffc_cfg[chip->ffc_jeita_index].ffc_vbat_1 - COLD_RECHG_VOLT_OFFSET))) {
						chip->ffc_ibat = chip->jeita_ffc_cfg[chip->ffc_jeita_index].ffc_ibat_1;
						ffc_curr_shake = false;
					}
				} else {
					chip->ffc_ibat = chip->jeita_ffc_cfg[chip->ffc_jeita_index].ffc_ibat_1;		//vbat<=4.1 ibat=8A
				}
			} else if (vol_now <= chip->jeita_ffc_cfg[chip->ffc_jeita_index].ffc_vbat_2) {
				chip->ffc_ibat = chip->jeita_ffc_cfg[chip->ffc_jeita_index].ffc_ibat_2;		//4.1<vabt<=4.3 ibat=6A
				ffc_curr_shake = true;
			} else if (vol_now <= chip->jeita_ffc_cfg[chip->ffc_jeita_index].ffc_vbat_3) {
				chip->ffc_ibat = chip->jeita_ffc_cfg[chip->ffc_jeita_index].ffc_ibat_3;		//4.3<vbat<=4.5 ibat=5.4A
			} else {
				chip->ffc_ibat = chip->jeita_ffc_cfg[chip->ffc_jeita_index].ffc_ibat_4;		//vbat>4.5 ibat=4.296A
			}
			vote(chip->total_fcc_votable, JEITA_VOTER, true, chip->ffc_ibat);
		} else {
			vote(chip->total_fcc_votable, JEITA_VOTER, true, chip->normal_ibat);
		}
	} else {
		vote(chip->total_fcc_votable, JEITA_VOTER, true, chip->normal_ibat / manager->half_cell);
	}

	/*normal:iterm + fv*/
	if (batt_id == 0)
		chip->iterm_curr = chip->jeita_normal_cfg[chip->normal_jeita_index].normal_iterm_1;
	else if (batt_id == 1)
		chip->iterm_curr = chip->jeita_normal_cfg[chip->normal_jeita_index].normal_iterm_2;
	else if (batt_id == 2)
		chip->iterm_curr = chip->jeita_normal_cfg[chip->normal_jeita_index].normal_iterm_3;
	else
		chip->iterm_curr = chip->jeita_normal_cfg[chip->normal_jeita_index].normal_iterm_1;
	chip->fv = chip->jeita_normal_cfg[chip->normal_jeita_index].normal_fv;
	
	/*raise SDP hard iterm*/
	charger_get_vbus_type(chip->charger, &vbus_type);

	if (vbus_type == VBUS_TYPE_SDP)
		chip->iterm_curr = chip->iterm_curr + HQ_SDP_HARD_ITERM_STEP;

	/*FFC: iterm + fv*/
	if (chip->pd_verifed && (g_policy->sm == PM_STATE_CHARGERPUMP_CC_CV || g_policy->cp_charge_done) && !manager->en_single_cell) {
		if (temp_now > TEMP_LEVEL_15 && temp_now <= TEMP_LEVEL_35) {
			if (batt_id == 0)
				chip->iterm_curr = chip->jeita_ffc_cfg[chip->ffc_jeita_index].ffc_iterm_1;
			if (batt_id == 1)
				chip->iterm_curr = chip->jeita_ffc_cfg[chip->ffc_jeita_index].ffc_iterm_2;
			if (batt_id == 2)
				chip->iterm_curr = chip->jeita_ffc_cfg[chip->ffc_jeita_index].ffc_iterm_3;
			chip->fv = chip->jeita_ffc_cfg[chip->ffc_jeita_index].ffc_fv;
			//chip->fv = (chip->fv - TERM_DELTA_CV); //To prevent excessive in the battery cell voltage, subtract 8mV
			#if IS_ENABLED(CONFIG_BQ_FUELGAUGE)
			fuel_gauge_set_fastcharge_mode(chip->fuel_gauge, true);
			#endif
		} else if (temp_now > TEMP_LEVEL_35 && temp_now <= TEMP_LEVEL_45) {
			if (batt_id == 0)
				chip->iterm_curr = chip->jeita_ffc_cfg[chip->ffc_jeita_index].ffc_iterm_1;
			if (batt_id == 1)
				chip->iterm_curr = chip->jeita_ffc_cfg[chip->ffc_jeita_index].ffc_iterm_2;
			if (batt_id == 2)
				chip->iterm_curr = chip->jeita_ffc_cfg[chip->ffc_jeita_index].ffc_iterm_3;
			chip->fv = chip->jeita_ffc_cfg[chip->ffc_jeita_index].ffc_fv;
			//chip->fv = (chip->fv - TERM_DELTA_CV); //To prevent excessive in the battery cell voltage, subtract 8mV
			#if IS_ENABLED(CONFIG_BQ_FUELGAUGE)
			fuel_gauge_set_fastcharge_mode(chip->fuel_gauge, true);
			#endif
		}
		#if IS_ENABLED(CONFIG_BQ_FUELGAUGE)
		else {
			fuel_gauge_set_fastcharge_mode(chip->fuel_gauge, false);
		}
		#endif

		if (chip->is_charge_done)
			adapter_set_cap(g_policy->adapter, g_policy->cap_nr, 5000, 2000);
	}

	vote(chip->iterm_votable, ITER_VOTER, true, chip->iterm_curr / manager->half_cell);
	vote(chip->fv_votable, JEITA_VOTER, true, chip->fv);

	hq_info("pd_verifed = %d, cp_charge_done = %d, normal_jeita_index = %d, ffc_jeita_index=%d, is_charge_done = %d\n",
				chip->pd_verifed, g_policy->cp_charge_done, chip->normal_jeita_index, chip->ffc_jeita_index, chip->is_charge_done);
	hq_info("normal_ibat = %d, ffc_ibat = %d, jeita_fv = %d, jeita_iterm = %d, batt_id = %d\n",
 				chip->normal_ibat, chip->ffc_ibat, chip->fv, chip->iterm_curr, batt_id);
	return 0;
}


static void chg_cycle_debug(struct chg_cycle_data *cycle_data)
{
	int i = 0, j = 0;

	for (i = 0; i <  cycle_data->temp_array_size; i++)
		hq_info("temp_array[i] = %d", cycle_data->temp_data[i]);

	for (i = 0; i < cycle_data->cycle_array_size; i++)
		hq_info("cycle_array[i] = %d", cycle_data->cycle_data[i]);

	for (i = 0; i < cycle_data->vol_array_rows; i++)
		for (j = 0; j < cycle_data->vol_array_cols; j++)
			hq_info("vol_array[%d][%d] = %d",i, j, cycle_data->vol_data[i][j]);

	for (i = 0; i < cycle_data->curr_array_rows; i++)
		for (j = 0; j < cycle_data->curr_array_cols; j++)
			hq_info("curr_array[%d][%d] = %d",i, j, cycle_data->curr_data[i][j]);
}

static int chg_cycle_get_temp_index(struct chg_cycle_data *cycle_data, int temp)
{
	int index = 0, i;

	for (i = 0; i < cycle_data->temp_array_size; i++) {
		if (temp > cycle_data->temp_data[i] && temp <= cycle_data->temp_data[cycle_data->temp_array_size - 1])
			index = i;
	}

	return index;
}

static int chg_cycle_get_cycle_count_index(struct chg_cycle_data *cycle_data, int cycle_count)
{
	int index = 0, i;

	for (i = 0; i < cycle_data->cycle_array_size; i++) {
		if (cycle_count >= cycle_data->cycle_data[i])
			index = i;
	}

	return index;
}

static int chg_cycle_get_vol_index(struct chg_cycle_data *cycle_data, int vol, int array_row)
{
	int index = 0, i;

	for (i = 0; i < cycle_data->vol_array_cols; i++) {
		if (vol <= cycle_data->vol_data[array_row][i]) {
			index = i;
			break;
		} else if (vol > cycle_data->vol_data[array_row][cycle_data->vol_array_cols -1]) {
			index = cycle_data->vol_array_cols -1;
			break;
		}
	}

	return index;
}

static void handle_chg_cycle(struct hq_jeita_info *chip)
{
	union power_supply_propval pval = {0, };
	int cycle_index = 0, temp_index = 0, vol_index = 0;
	struct charger_manager *manager;
	int ret;
	int fv_vote_value = 0, curr_vote_value = 0;
	int array_row;

	manager = (struct charger_manager *)power_supply_get_drvdata(chip->batt_psy);
	if (IS_ERR_OR_NULL(manager)) {
		hq_err("get charger manager fail\n");
		return;
	}

	ret = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	if (ret < 0) {
		hq_err("Couldn't read chg cycle, ret=%d\n", ret);
		return;
	}

	if (!IS_ERR_OR_NULL(chip->ffc_cycle_data) && (chip->pd_verifed &&
			(g_policy->sm == PM_STATE_CHARGERPUMP_CC_CV || g_policy->cp_charge_done) && !manager->en_single_cell)) {
		temp_index = chg_cycle_get_temp_index(chip->ffc_cycle_data, manager->tbat);
		cycle_index = chg_cycle_get_cycle_count_index(chip->ffc_cycle_data, pval.intval);

		array_row = temp_index * chip->ffc_cycle_data->vol_array_rows + cycle_index;
		fv_vote_value = chip->ffc_cycle_data->vol_data[array_row][chip->ffc_cycle_data->vol_array_cols - 1];

		vol_index = chg_cycle_get_vol_index(chip->ffc_cycle_data, manager->vbat, array_row);
		curr_vote_value = chip->ffc_cycle_data->curr_data[array_row][vol_index];
		if (curr_vote_value > chip->ffc_cycle_data->last_curr_value && temp_index == chip->ffc_cycle_data->last_temp_index) {
			if (abs(manager->vbat - chip->ffc_cycle_data->vol_data[array_row][chip->ffc_cycle_data->last_vol_index]) <= CHG_CYCLE_RESTORE_VOL)
				curr_vote_value = chip->ffc_cycle_data->last_curr_value == 0 ? curr_vote_value : chip->ffc_cycle_data->last_curr_value;
                        else
				chip->ffc_cycle_data->last_vol_index = vol_index >= 1 ? (vol_index -1) : vol_index;
		} else
			chip->ffc_cycle_data->last_vol_index = vol_index >= 1 ? (vol_index -1) : vol_index;
		chip->ffc_cycle_data->last_curr_value = curr_vote_value;
		chip->ffc_cycle_data->last_temp_index = temp_index;
	} else if (!IS_ERR_OR_NULL(chip->normal_cycle_data)) {
		temp_index = chg_cycle_get_temp_index(chip->normal_cycle_data, manager->tbat);
		cycle_index = chg_cycle_get_cycle_count_index(chip->normal_cycle_data, pval.intval);

		array_row = temp_index * chip->normal_cycle_data->vol_array_rows + cycle_index;
		fv_vote_value = chip->normal_cycle_data->vol_data[array_row][chip->normal_cycle_data->vol_array_cols - 1];

		vol_index = chg_cycle_get_vol_index(chip->normal_cycle_data, manager->vbat, array_row);
		curr_vote_value = chip->normal_cycle_data->curr_data[array_row][vol_index];
		if (curr_vote_value > chip->normal_cycle_data->last_curr_value && temp_index == chip->normal_cycle_data->last_temp_index) {
			if (abs(manager->vbat - chip->normal_cycle_data->vol_data[array_row][chip->normal_cycle_data->last_vol_index]) <= CHG_CYCLE_RESTORE_VOL)
				curr_vote_value = chip->normal_cycle_data->last_curr_value == 0 ? curr_vote_value : chip->normal_cycle_data->last_curr_value;
			else
				chip->normal_cycle_data->last_vol_index = vol_index;
		} else
			chip->normal_cycle_data->last_vol_index = vol_index;
		chip->normal_cycle_data->last_curr_value = curr_vote_value;
		chip->normal_cycle_data->last_temp_index = temp_index;
	}

	vote(chip->total_fcc_votable, CHG_CYCLE_VOTER, curr_vote_value == 0? false: true, curr_vote_value);
	vote(chip->fv_votable, CHG_CYCLE_VOTER, fv_vote_value == 0? false: true, fv_vote_value);
}

static void status_change_work(struct work_struct *work)
{
	struct hq_jeita_info *chip = container_of(work,
			struct hq_jeita_info, status_change_work.work);
	int rc = 0;

	if (!is_batt_available(chip)|| !is_bms_available(chip))
		goto exit_work;

	rc = handle_jeita(chip);
	if (rc < 0)
		hq_err("Couldn't handle sw jeita rc = %d\n", rc);
	handle_chg_cycle(chip);

	if (! is_input_present(chip)) {
		vote(chip->main_icl_votable, JEITA_VOTER, false, 0);
	}

exit_work:
	__pm_relax(chip->hq_jeita_ws);
}

#if IS_ENABLED(CONFIG_HUAQIN_SOFT_ITERM_SUPPORT)
void get_term_and_rechg_offset(struct hq_jeita_info *chip)
{
	int i = 0;

	chip->soft_rechg_offset = NOR_TEMP_RECHG_OFFSET;

	for (i = 0; i < chip->rechg_fv_offset_data_cnt; i++) {
		if (chip->soft_temp >= chip->rechg_fv_offset_data[i].temp_level_l &&
				chip->soft_temp < chip->rechg_fv_offset_data[i].temp_level_h) {
			if (chip->rechg_fv_offset_data[i].value > 0)
				chip->soft_rechg_offset = chip->rechg_fv_offset_data[i].value;
		}
	}

	if (chip->soft_cc > BATT_CYCLE_COUNT_OVER_800) {
		chip->soft_rechg_offset += 80;
		chip->soft_soc_threshold = 98;
	} else if (chip->soft_cc > BATT_CYCLE_COUNT_OVER_100) {
		chip->soft_rechg_offset += 50;
		chip->soft_soc_threshold = 98;
	}
}

static void calculate_average_current(int batt_current, int *batt_ma_avg)
{
	static int samples_index = 0, samples_num = 0, batt_ma_avg_samples[BATT_MA_AVG_SAMPLES];
	static int batt_ma_prev;
	static int last_batt_ma_avg;
	int sum_ma = 0;
	int i;

	if (batt_current == batt_ma_prev)
		goto unchanged;
	else
		batt_ma_prev = batt_current;

	batt_ma_avg_samples[samples_index] = batt_current;
	samples_index = (samples_index + 1) % BATT_MA_AVG_SAMPLES;
	samples_num += 1;

	if (samples_num >= BATT_MA_AVG_SAMPLES)
		samples_num = BATT_MA_AVG_SAMPLES;

	for (i = 0; i <  samples_num; i++)
		sum_ma += batt_ma_avg_samples[i];

	last_batt_ma_avg = sum_ma / samples_num;

unchanged:
	*batt_ma_avg = last_batt_ma_avg;
}

static int charger_status_update(struct hq_jeita_info *chip)
{
	union power_supply_propval pval = {0, };
	int ret = 0;

	ret = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
	if (ret < 0)
		hq_err("get charge status error.\n");
	else
		chip->soft_chg_status = pval.intval;

	ret = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret < 0)
		hq_err("get battery current error.\n");
	else
		chip->soft_ibat = pval.intval;

	ret = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0)
		hq_err("get battery soc error.\n");
	else
		chip->soft_soc = pval.intval;

	ret = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret < 0)
		hq_err("get battery volt error.\n");
	else
		chip->soft_vbat = pval.intval / 1000;

	ret = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0)
		hq_err("get battery temp error\n");
	else
		chip->soft_temp = pval.intval;

	ret = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	if (ret < 0)
		hq_err("get cycle count error\n");
	else
		chip->soft_cc = pval.intval;

	get_term_and_rechg_offset(chip);

	calculate_average_current(chip->soft_ibat, &chip->soft_avg_ibat);

	return 0;
}

static int soft_term_and_recharge_check(struct hq_jeita_info *chip, struct charger_manager *manager)
{
	int fv = get_effective_result(manager->fv_votable);
	int soft_term_cnt = 3;
	int batt_curr = chip->soft_ibat;

	chip->soft_fv_threshold = ST_THRESHOLD_LOW_LIMIT_FV(fv);
	chip->soft_iterm_threshold = ST_THRESHOLD_LOW_LIMIT_ITERM(chip->iterm_curr);

	charger_status_update(chip);

	if (manager->vbus_type == VBUS_TYPE_SDP) {
		soft_term_cnt = 10;
		batt_curr = chip->soft_avg_ibat;
		chip->soft_fv_threshold = ST_THRESHOLD_LOW_LIMIT_SDP_FV(fv);
		chip->soft_iterm_threshold = ST_THRESHOLD_LOW_LIMIT_ITERM_SDP(chip->iterm_curr);
	}

	/*soft iterm function need satisfy
	  *1:uisoc = 100, ERP:uisoc = 99
	  *2:the state of charge must be charging
	  *3:not lower than cut-off voltage
	  *4:iterm standard
	  */
	if (manager->product_name_index == EEA) {
		if (chip->soft_soc >= 99 &&
			chip->soft_chg_status == POWER_SUPPLY_STATUS_CHARGING &&
			chip->soft_vbat >= chip->soft_fv_threshold &&
			(-batt_curr / 1000) <= chip->soft_iterm_threshold &&
			batt_curr < 0) {
			if (manager->soft_term_check_cnt >= soft_term_cnt) {
				vote(manager->total_fcc_votable, SOFT_ITER_VOTER, true, 0);
				manager->soft_term_status = POWER_SUPPLY_STATUS_FULL;
				manager->soft_term_check_cnt = 0;
				charge_done(manager);
			} else {
				manager->soft_term_check_cnt++;
			}
		}
		//soc == 100% when usb plug in, need disable charge immediately
		else if (manager->plug_in_soc100_flag && chip->soft_soc == 100 &&
					chip->soft_chg_status == POWER_SUPPLY_STATUS_CHARGING) {
			vote(manager->total_fcc_votable, SOFT_ITER_VOTER, true, 0);
			manager->soft_term_status = POWER_SUPPLY_STATUS_FULL;
			manager->soft_term_check_cnt = 0;
			charge_done(manager);
		}
	} else {
		if (chip->soft_soc >= chip->soft_soc_threshold &&
			chip->soft_chg_status == POWER_SUPPLY_STATUS_CHARGING &&
			chip->soft_vbat >= chip->soft_fv_threshold &&
			(-batt_curr / 1000) <= chip->soft_iterm_threshold &&
			batt_curr < 0) {
			if (manager->soft_term_check_cnt >= 3) {
				vote(manager->total_fcc_votable, SOFT_ITER_VOTER, true, 0);
				manager->soft_term_status = POWER_SUPPLY_STATUS_FULL;
				manager->soft_term_check_cnt = 0;
				charge_done(manager);
			} else {
				manager->soft_term_check_cnt++;
			}
		}
	}

	/*recharger situation
	 *1:the state of charge must be full
	 *2:other:reach the threshold of recharge or soc lower 100
	 *3:ERP:soc lower 95
	 */
	if (manager->product_name_index == EEA) {
		if (manager->soft_term_status == POWER_SUPPLY_STATUS_FULL && chip->soft_soc < 95) {
			vote(manager->total_fcc_votable, SOFT_ITER_VOTER, false, 0);
			manager->soft_term_status = POWER_SUPPLY_STATUS_CHARGING;
			manager->plug_in_soc100_flag = false;
			hq_info("soft term:eea recharge!!!\n");
		}
	} else {
		if (manager->soft_term_status == POWER_SUPPLY_STATUS_FULL &&
			(chip->soft_vbat < (fv - chip->soft_rechg_offset) ||
			chip->soft_soc < chip->soft_soc_threshold)) {
			vote(manager->total_fcc_votable, SOFT_ITER_VOTER, false, 0);
			manager->soft_term_status = POWER_SUPPLY_STATUS_CHARGING;
			hq_info("soft term:recharge!!!\n");
		}
	}
	hq_info("soft term:check_cnt=%d, ssoc=%d, chg_status=%d, vbat=%d, ibat=%d, avg_ibat=%d, rechg_fv_offset=%d, product_name=%d\n",
			manager->soft_term_check_cnt, chip->soft_soc, chip->soft_chg_status,
				chip->soft_vbat, chip->soft_ibat, chip->soft_avg_ibat,
					chip->soft_rechg_offset, manager->product_name_index);
	return 0;
}

void soft_iterm_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work, struct charger_manager, soft_iterm_work.work);

	if (IS_ERR_OR_NULL(manager) || IS_ERR_OR_NULL(the_chip)) {
		hq_err("manager or chip is_err_or_null\n");
		return;
	}


	if ((manager->vbus_type == VBUS_TYPE_SDP) &&
		(manager->product_name_index != EEA))
		return;

	soft_term_and_recharge_check(the_chip, manager);
	the_chip->hard_iterm = HW_THRESHOLD_LOW_LIMIT_ITERM(the_chip->iterm_curr);
	vote(the_chip->iterm_votable, HARD_ITERM_VOTER, true, the_chip->hard_iterm);
	schedule_delayed_work(&manager->soft_iterm_work, msecs_to_jiffies(SOFT_ITERM_TIME));
}
#endif

static int jeita_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	struct power_supply *psy = v;
	struct hq_jeita_info*chip = container_of(nb, struct hq_jeita_info, nb);

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if ((strcmp(psy->desc->name, "battery") == 0)
			|| (strcmp(psy->desc->name, "usb") == 0)) {
		__pm_stay_awake(chip->hq_jeita_ws);
		schedule_delayed_work(&chip->status_change_work, 0);
	}

	return NOTIFY_OK;
}

static int jeita_register_notifier(struct hq_jeita_info *chip)
{
	int rc;

	chip->nb.notifier_call = jeita_notifier_call;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0) {
		hq_err("Couldn't register psy notifier rc = %d\n", rc);
		return rc;
	}

	return 0;
}

static struct chg_cycle_data* chg_cycle_cfg_parse_dts(struct hq_jeita_info *chip, struct device_node *node)
{
	struct chg_cycle_data *cycle_data;
	int total_length = 0, cycle_data_length = 0, temp_data_length = 0;
	int vol_data_length = 0, curr_data_length = 0, ret = 0, row, col;

	cycle_data_length = of_property_count_elems_of_size(node, "cycle_count_data", sizeof(u32));
	if (cycle_data_length < 0) {
		hq_err("failed to read total_length of cycle_count_data\n");
		return NULL;
	}

	temp_data_length = of_property_count_elems_of_size(node, "cycle_temp_data", sizeof(u32));
	if (temp_data_length < 0) {
		hq_err("failed to read total_length of temp_data_length\n");
		return NULL;
	}

	vol_data_length = of_property_count_elems_of_size(node, "cycle_vol_data", sizeof(u32));
	if (vol_data_length < 0) {
		hq_err("failed to read total_length of vol_data_length\n");
		return NULL;
	}

	curr_data_length = of_property_count_elems_of_size(node, "cycle_curr_data", sizeof(u32));
	if (curr_data_length < 0) {
		hq_err("failed to read total_length of curr_data_length\n");
		return NULL;
	}
	total_length = cycle_data_length + temp_data_length + vol_data_length + curr_data_length + 9 * sizeof(int);

	cycle_data = (struct chg_cycle_data *)devm_kzalloc(chip->dev, sizeof(int) * total_length, GFP_KERNEL);
	if (IS_ERR_OR_NULL(cycle_data)) {
		hq_err("malloc cycle_data fail\n");
		return NULL;
	}
	cycle_data->cycle_array_size = cycle_data_length;
	cycle_data->temp_array_size = temp_data_length;
	cycle_data->vol_array_rows = cycle_data_length * (temp_data_length - 1);
	cycle_data->vol_array_cols = vol_data_length / cycle_data->vol_array_rows;
	cycle_data->curr_array_rows = cycle_data->vol_array_rows;
	cycle_data->curr_array_cols = cycle_data->vol_array_cols;

	cycle_data->cycle_data = (int *)devm_kzalloc(chip->dev, sizeof(int) * cycle_data_length, GFP_KERNEL);
	if (IS_ERR_OR_NULL(cycle_data->cycle_data)) {
		hq_err("malloc cycle_data->cycle_data fail\n");
		return NULL;
	}
	ret |= of_property_read_u32_array(node, "cycle_count_data", cycle_data->cycle_data, cycle_data_length);
	if (ret)
	{
		hq_err("failed to parse cycle_data\n");
		return NULL;
	}

	cycle_data->temp_data = (int *)devm_kzalloc(chip->dev, sizeof(int) * temp_data_length, GFP_KERNEL);
	if (IS_ERR_OR_NULL(cycle_data->temp_data)) {
		hq_err("malloc cycle_data->temp_data fail\n");
		return NULL;
	}
	ret |= of_property_read_u32_array(node, "cycle_temp_data", cycle_data->temp_data, temp_data_length);
	if (ret)
	{
		hq_err("failed to parse temp_data\n");
		return NULL;
	}

	cycle_data->vol_data = (int **)devm_kzalloc(chip->dev, sizeof(int*) * vol_data_length, GFP_KERNEL);
	if (IS_ERR_OR_NULL(cycle_data->vol_data)) {
		hq_err("malloc cycle_data->vol_data fail\n");
		return NULL;
	}
	for (row = 0; row < cycle_data->vol_array_rows; row++) {
		cycle_data->vol_data[row] = (int *)devm_kzalloc(chip->dev, sizeof(int) * cycle_data->vol_array_cols, GFP_KERNEL);
		if (IS_ERR_OR_NULL(cycle_data->vol_data[row])) {
			hq_err("malloc cycle_data->vol_data[%d] fail\n", row);
			return NULL;
		}
		for (col = 0; col < cycle_data->vol_array_cols; col++) {
			ret = of_property_read_u32_index(node, "cycle_vol_data", row * cycle_data->vol_array_cols + col, &(cycle_data->vol_data[row][col]));
			if (ret) {
				hq_err("failed to parse cycle_vol_data\n");
				return NULL;
			}
		}
	}

	cycle_data->curr_data = (int **)devm_kzalloc(chip->dev, sizeof(int*) * curr_data_length, GFP_KERNEL);
	if (IS_ERR_OR_NULL(cycle_data->curr_data)) {
		hq_err("malloc cycle_data->curr_data fail\n");
		return NULL;
	}
	for (row = 0; row < cycle_data->curr_array_rows; row++) {
		cycle_data->curr_data[row] = (int *)devm_kzalloc(chip->dev, sizeof(int) * cycle_data->curr_array_cols, GFP_KERNEL);
		if (IS_ERR_OR_NULL(cycle_data->curr_data[row])) {
			hq_err("malloc cycle_data->curr_data[%d] fail\n", row);
			return NULL;
		}
		for (col = 0; col < cycle_data->curr_array_cols; col++) {
			ret = of_property_read_u32_index(node, "cycle_curr_data", row * cycle_data->curr_array_cols + col, &(cycle_data->curr_data[row][col]));
			if (ret) {
				hq_err("failed to parse cycle_curr_data\n");
				return NULL;
			}
		}
	}

	chg_cycle_debug(cycle_data);

	return cycle_data;
}

static void chg_cycle_cfg_init(struct hq_jeita_info *chip)
{
	struct device_node *node = chip->dev->of_node;
	struct device_node *ffc_cycle_chg_node = NULL;
	struct device_node *normal_cycle_chg_node = NULL;

	if (!IS_ERR_OR_NULL(node)) {
		ffc_cycle_chg_node = of_find_node_by_name(node, "ffc_chg_cfg_bat_cycle");
		//power_max <= 45W, use eea configs
		normal_cycle_chg_node = of_find_node_by_name(node, "eea_normal_chg_cfg_bat_cycle");
	}

	if (!IS_ERR_OR_NULL(ffc_cycle_chg_node))
		chip->ffc_cycle_data = chg_cycle_cfg_parse_dts(chip, ffc_cycle_chg_node);
	else
		hq_info("cat not found ffc_cycle_chg_node\n");


	if (!IS_ERR_OR_NULL(normal_cycle_chg_node))
		chip->normal_cycle_data = chg_cycle_cfg_parse_dts(chip, normal_cycle_chg_node);
	else
		hq_info("cat not found normal_cycle_chg_node\n");
}

static bool hq_parse_jeita_dt(struct device_node *node, struct hq_jeita_info *chip)
{
	int total_length = 0;
	bool ret = 0;
	int i = 0;

	total_length = of_property_count_elems_of_size(node, "jeita_normal_cfg", sizeof(u32));
	if (total_length < 0) {
		hq_err("failed to read total_length of jeita_normal_cfg\n");
		return 0;
	}

	if (total_length % 9) {
		hq_err("jeita_normal_cfg not a tuple element\n");
		return 0;
	}

	chip->jeita_normal_cnt = total_length / 9;
	if (chip->jeita_normal_cnt > MAX_STEP_JEITA_NUM) {
		hq_err("jeita_normal_cfg exceed max step jeita num: %d\n", MAX_STEP_JEITA_NUM);
		return 0;
	}

	ret |= of_property_read_u32_array(node, "jeita_normal_cfg", (u32 *)chip->jeita_normal_cfg, total_length);
	if (ret) {
		hq_err("failed to parse jeita_normal_cfg\n");
		return 0;
	}

	total_length = of_property_count_elems_of_size(node, "jeita_ffc_cfg", sizeof(u32));
	if (total_length < 0) {
		hq_err("failed to read total_length of jeita_ffc_cfg\n");
		return 0;
	}


	if (total_length % 13) {
		hq_err("jeita_ffc_cfg not a tuple element\n");
		return 0;
	}

	chip->jeita_ffc_cnt = total_length / 13;
	if (chip->jeita_ffc_cnt > MAX_FFC_JEITA_NUM) {
		hq_err("jeita_ffc_cfg exceed max step jeita num: %d\n", MAX_STEP_JEITA_NUM);
		return 0;
	}

	ret |= of_property_read_u32_array(node, "jeita_ffc_cfg", (u32 *)chip->jeita_ffc_cfg, total_length);
	if (ret) {
		hq_err("failed to parse jeita_ffc_cfg\n");
		return 0;
	}

	total_length = of_property_count_elems_of_size(node, "rechg_fv_offset_cfg", sizeof(u32));
	if (total_length < 0) {
		hq_err("failed to read total_length of rechg_fv_cfg\n");
		return 0;
	}

	if (total_length % 3) {
		hq_err("rechg_fv_cfg not a tuple element\n");
		return 0;
	}

	chip->rechg_fv_offset_data = devm_kzalloc(chip->dev, sizeof(int) * total_length, GFP_KERNEL);
	if (!chip->rechg_fv_offset_data) {
		hq_err("malloc rechg_fv_cfg fail\n");
		return 0;
	}

	chip->rechg_fv_offset_data_cnt = total_length / 3;
	ret |= of_property_read_u32_array(node, "rechg_fv_offset_cfg", (u32 *)chip->rechg_fv_offset_data, total_length);
	if (ret) {
		hq_err("failed to parse rechg_fv_offset_cfg\n");
		return 0;
	}

	for (i = 0; i < chip->jeita_normal_cnt; i++)
		hq_info("[jeita_normal_cfg]%d %d %d %d %d %d %d %d %d\n",
					chip->jeita_normal_cfg[i].low_threshold, chip->jeita_normal_cfg[i].high_threshold, chip->jeita_normal_cfg[i].normal_ibat_1,
					chip->jeita_normal_cfg[i].normal_vbat_1, chip->jeita_normal_cfg[i].normal_ibat_2, chip->jeita_normal_cfg[i].normal_iterm_1,
					chip->jeita_normal_cfg[i].normal_iterm_2, chip->jeita_normal_cfg[i].normal_iterm_3, chip->jeita_normal_cfg[i].normal_fv);

	for (i = 0; i < chip->jeita_ffc_cnt; i++)
		hq_info("[jeita_ffc_cfg]%d %d %d %d %d %d %d %d %d %d %d %d %d\n",
					chip->jeita_ffc_cfg[i].low_threshold, chip->jeita_ffc_cfg[i].high_threshold, chip->jeita_ffc_cfg[i].ffc_ibat_1,
					chip->jeita_ffc_cfg[i].ffc_vbat_1, chip->jeita_ffc_cfg[i].ffc_ibat_2, chip->jeita_ffc_cfg[i].ffc_vbat_2,
					chip->jeita_ffc_cfg[i].ffc_ibat_3, chip->jeita_ffc_cfg[i].ffc_vbat_3, chip->jeita_ffc_cfg[i].ffc_ibat_4,
					chip->jeita_ffc_cfg[i].ffc_iterm_1, chip->jeita_ffc_cfg[i].ffc_iterm_2, chip->jeita_ffc_cfg[i].ffc_iterm_3,
					chip->jeita_ffc_cfg[i].ffc_fv);

	return !ret;
}

int hq_jeita_init(struct device *dev)
{
	struct device_node *node = dev->of_node;
	struct device_node *step_jeita_node = NULL;
	struct hq_jeita_info *chip = NULL;

	int rc = 0;

	if (node) {
		step_jeita_node = of_find_node_by_name(node, "step_jeita");
	}

	if (the_chip) {
		hq_err("Already initialized\n");
		return -EINVAL;
	}

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->hq_jeita_ws = wakeup_source_register(dev, "hq_jeita");
	if (!chip->hq_jeita_ws)
		return -EINVAL;

	chip->dev = dev;

	rc = hq_parse_jeita_dt(step_jeita_node, chip);
	if(!rc){
		hq_err("hq_parse_jeita_dt failed\n");
		goto release_wakeup_source;
	}

	chg_cycle_cfg_init(chip);

	chip->total_fcc_votable = find_votable("TOTAL_FCC");
	if (!chip->total_fcc_votable){
		hq_err("find TOTAL_FCC voltable failed\n");
		goto release_wakeup_source;
	}

	chip->fv_votable = find_votable("MAIN_FV");
	if (!chip->fv_votable){
		hq_err("find MAIN_FV voltable failed\n");
		goto release_wakeup_source;
	}

	chip->iterm_votable = find_votable("MAIN_ITERM");
	if (!chip->iterm_votable){
		hq_err("find MAIN_FV voltable failed\n");
		goto release_wakeup_source;
	}

	chip->charger = charger_find_dev_by_name("primary_chg");
	if (chip->charger == NULL) {
		hq_err("failed get charger\n");
		goto release_wakeup_source;
	}

	INIT_DELAYED_WORK(&chip->status_change_work, status_change_work);

	rc = jeita_register_notifier(chip);
	if (rc < 0) {
		hq_err("Couldn't register psy notifier rc = %d\n", rc);
		goto release_wakeup_source;
	}

	chip->soft_soc_threshold = 100;

	the_chip = chip;
	hq_info("hq_jeita_init success\n");

	return 0;

release_wakeup_source:
	wakeup_source_unregister(chip->hq_jeita_ws);
	return rc;
}

void hq_jeita_deinit(void)
{
	struct hq_jeita_info *chip = the_chip;

	if (!chip)
		return;

	cancel_delayed_work_sync(&chip->status_change_work);
	power_supply_unreg_notifier(&chip->nb);
	wakeup_source_unregister(chip->hq_jeita_ws);
	the_chip = NULL;
}
/*
 * HQ jeita Release Note
 * 2.0.0
 * (1) Add CONFIG_HUAQIN_ERP_SUPPORT to load the ERP soft iterm function.
 * Distinguish between the EU region and other regions based on the board ID.
 * ERP:report 100% after the cutoff of SOC 99%, below 95% recharge.
 *
 * 1.0.0
 * (1)  Add CONFIG_HUAQIN_SOFT_ITERM_SUPPORT to load the soft iterm function.
 * 	Hq charger manager start work,the implementation of work is in this driver.
 * 	Work calls soft iterm function.
 * 	Determine whether to stop charging based on some charging status information：soc，ibat,vbat,chg status.
 */
