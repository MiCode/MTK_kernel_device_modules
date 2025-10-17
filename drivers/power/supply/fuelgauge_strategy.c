// SPDX-License-Identifier: GPL-2.0
/*
 *fuelgauge_strategy.c
 *
 * mca fuelgauge strategy driver
 *
 * Copyright (c) 2025-2025 Xiaomi Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/regmap.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/mca/common/mca_log.h>

#include "mtk_battery.h"
#include "fuelgauge_class.h"
#include "fuelgauge_strategy.h"

#ifndef MCA_LOG_TAG
#define MCA_LOG_TAG "fuelgauge_strategy"
#endif

static int log_level = 2;
#define fg_err(fmt, ...)					\
do {								\
	if (log_level >= 0)					\
			printk(KERN_ERR "" fmt, ##__VA_ARGS__);	\
} while (0)

#define fg_info(fmt, ...)					\
do {								\
	if (log_level >= 1)					\
			printk(KERN_ERR "" fmt, ##__VA_ARGS__);	\
} while (0)

#define fg_dbg(fmt, ...)					\
do {								\
	if (log_level >= 2)					\
			printk(KERN_ERR "" fmt, ##__VA_ARGS__);	\
} while (0)

#define FFC_SMOOTH_LEN			4
#define FG_MONITOR_DELAY_2S		2000
#define FG_MONITOR_DELAY_3S		3000
#define FG_MONITOR_DELAY_5S		5000
#define FG_MONITOR_DELAY_10S		10000
#define FG_MONITOR_DELAY_30S		30000

#define BQ_REPORT_FULL_SOC	9800
#define BQ_CHARGE_FULL_SOC	9750
#define BQ_RECHARGE_SOC		9800
#define BQ_DEFUALT_FULL_SOC	100

#define HW_REPORT_FULL_SOC 9500

// 67W
#define SOC_PROPORTION_67W 97
#define SOC_PROPORTION_67W_C 98
// 120W
#define SOC_PROPORTION_120W 94
#define SOC_PROPORTION_120W_C 95

#define SOC_PROPORTION_EEA    100
#define SOC_PROPORTION_EEA_C  101

#define BATT_COOL_THRESHOLD 150
#define BATT_COLD_THRESHOLD 0

#define BATTA_CAPACITY 3995
#define BATTB_CAPACITY 3505

static ktime_t probe_time = -1;
struct ffc_smooth {
	int curr_lim;
	int time;
};

static enum power_supply_property fg_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
};

struct ffc_smooth ffc_dischg_smooth[FFC_SMOOTH_LEN] = {
	{0,    150000},
	{300,  100000},
	{600,   72000},
	{1000,  50000},
};

enum strategy_fg_type {
    MCA_FG_TYPE_SINGLE = 0,
    MCA_FG_TYPE_PARALLEL,
    MCA_FG_TYPE_SERIES,
    MCA_FG_TYPE_MAX = MCA_FG_TYPE_SERIES,
};
static int fuelgauge_strategy_get_rm(struct strategy_fg *fg);

static bool battery_get_psy(struct strategy_fg *fg)
{
	fg->batt_psy = power_supply_get_by_name("battery");
	if (!fg->batt_psy) {
		mca_log_err("%s failed to get batt_psy", fg->log_tag);
		return false;
	}
	return true;
}

static int calc_delta_time(ktime_t time_last, int *delta_time)
{
	ktime_t time_now;

	time_now = ktime_get();

	*delta_time = ktime_ms_delta(time_now, time_last);
	if (*delta_time < 0)
		*delta_time = 0;

	fg_dbg("now:%lld, last:%lld, delta:%d\n", time_now, time_last, *delta_time);

	return 0;
}

static int bq_battery_soc_smooth_tracking_new(struct strategy_fg *fg, int raw_soc, int batt_soc, int batt_ma)
{
	static int system_soc, last_system_soc;
	int soc_changed = 0, unit_time = 10000, delta_time = 0, soc_delta = 0;
	static ktime_t last_change_time = -1;
	int change_delta = 0;
	int  rc, charging_status, i=0, batt_ma_avg = 0;
	union power_supply_propval pval = {0, };
	static int ibat_pos_count = 0;
	struct timespec64 time;
	ktime_t tmp_time = 0;


	tmp_time = ktime_get_boottime();
	time = ktime_to_timespec64(tmp_time);

	if((batt_ma > 0) && (ibat_pos_count < 10))
		ibat_pos_count++;
	else if(batt_ma <= 0)
		ibat_pos_count = 0;

	rc = power_supply_get_property(fg->batt_psy,
				POWER_SUPPLY_PROP_STATUS, &pval);
		if (rc < 0) {
			mca_log_info("failed get batt staus\n");
			return -EINVAL;
		}
	charging_status = pval.intval;
    fuelgauge_dev_update_monitor_delay(fg->fg_master_dev, &fg->monitor_delay);
	if (fg->tbat < 150) {
		fg->monitor_delay = FG_MONITOR_DELAY_3S;
	}
	if (!raw_soc) {
		fg->monitor_delay = FG_MONITOR_DELAY_10S;
	}
    fuelgauge_dev_update_eea_chg_support(fg->fg_master_dev, &fg->is_eea_model);
    fuelgauge_dev_get_charging_done_status(fg->fg_master_dev, &fg->charging_done);
    fuelgauge_dev_get_en_smooth_full_status(fg->fg_master_dev, &fg->en_smooth_full);
	/*Map system_soc value according to raw_soc */
	if (fg->is_eea_model) {
		if (fg->charging_done && raw_soc >= 10000) {
			system_soc = 100;
		} else if (fg->charging_done && fg->en_smooth_full) {
			system_soc = 100;
			if (fg->ui_soc == 100) {
				fg->en_smooth_full = false;
			}
		} else if (charging_status == POWER_SUPPLY_STATUS_CHARGING) {
			system_soc = ((raw_soc + SOC_PROPORTION_EEA) / SOC_PROPORTION_EEA_C);
			if(system_soc > 99)
				system_soc = 99;
		} else {
			system_soc = ((raw_soc + SOC_PROPORTION_EEA) / SOC_PROPORTION_EEA_C);
			if(system_soc > 100 || raw_soc >= 9900)
				system_soc = 100;
		}
	} else {
		if(raw_soc >= fg->report_full_rsoc) {
			system_soc = 100;
		}else if (fg->max_chg_power_120w) {
			system_soc = ((raw_soc + SOC_PROPORTION_120W) / SOC_PROPORTION_120W_C);
			if(system_soc > 99)
				system_soc = 99;
		} else {
			system_soc = ((raw_soc + SOC_PROPORTION_67W) / SOC_PROPORTION_67W_C);
			if(system_soc > 99)
				system_soc = 99;
		}
	}

	/*Get the initial value for the first time */
	if(last_change_time == -1) {
		last_change_time = ktime_get();
		if(system_soc != 0)
			last_system_soc = system_soc;
		else
			last_system_soc = batt_soc;
	}
    fuelgauge_strategy_get_rm(fg);
	if ((charging_status == POWER_SUPPLY_STATUS_DISCHARGING ||
		charging_status == POWER_SUPPLY_STATUS_NOT_CHARGING ) && 
		!fg->rm && fg->tbat < 150 && last_system_soc >= 1) 
	{
		fuelgauge_dev_read_avg_current(fg->fg_master_dev, &batt_ma_avg);
		for (i = FFC_SMOOTH_LEN-1; i >= 0; i--) {
			if (batt_ma_avg > ffc_dischg_smooth[i].curr_lim) {
				unit_time = ffc_dischg_smooth[i].time;
				break;
			}
		}
		mca_log_info("enter low temperature smooth unit_time=%d batt_ma_avg=%d\n", unit_time, batt_ma_avg);
	}

	if ((charging_status == POWER_SUPPLY_STATUS_DISCHARGING || charging_status == POWER_SUPPLY_STATUS_NOT_CHARGING ) &&
		fg->rsoc == 0 && last_system_soc >= 1 && ((fg->vbat <= fg->normal_shutdown_vbat + 50 && fg->tbat > 0) || ((fg->vbat <= fg->normal_shutdown_vbat - 50) && fg->tbat <= 0))) {
			fuelgauge_dev_read_avg_current(fg->fg_master_dev, &batt_ma_avg);
			if(batt_ma_avg >= 2000){
				unit_time = 5000;
				fg->monitor_delay = FG_MONITOR_DELAY_5S;
			}else if(batt_ma_avg >= 1000){
				unit_time = 8000;
				fg->monitor_delay = FG_MONITOR_DELAY_5S;
			}else if(batt_ma_avg >= 700){
				unit_time = 10000;
				fg->monitor_delay = FG_MONITOR_DELAY_10S;
			} else {
				unit_time = 10000;
				fg->monitor_delay = FG_MONITOR_DELAY_10S;
			}
			mca_log_info("enter quicky smooth unit_time=%d monitor_delay=%d\n", unit_time, fg->monitor_delay);
	}
	if (fg->en_smooth_full) {
		unit_time = 30000;
		mca_log_info("battery charging full smooth unit_time=%d monitor_delay=%d\n", unit_time, fg->monitor_delay);
	}
	/*If the soc jump, will smooth one cap every 10S */
	soc_delta = abs(system_soc - last_system_soc);
	if(soc_delta > 1 || (fg->vbat < 3400 && system_soc > 0) || (unit_time != 10000 && soc_delta == 1)){
		//unit_time != 10000 && soc_delta == 1 fix low temperature 2% jump to 0%
		calc_delta_time(last_change_time, &change_delta);
		delta_time = change_delta / unit_time;
		if (delta_time < 0) {
			last_change_time = ktime_get();
			delta_time = 0;
		}
		soc_changed = min(1, delta_time);
		if (soc_changed) {
			if((charging_status == POWER_SUPPLY_STATUS_CHARGING || charging_status == POWER_SUPPLY_STATUS_FULL)&& system_soc > last_system_soc)
				system_soc = last_system_soc + soc_changed;
			else if(charging_status == POWER_SUPPLY_STATUS_DISCHARGING && system_soc < last_system_soc)
				system_soc = last_system_soc - soc_changed;
		} else
			system_soc = last_system_soc;
		mca_log_info("fg jump smooth soc_changed=%d\n", soc_changed);
	}
	if(system_soc < last_system_soc)
		system_soc = last_system_soc - 1;
	/*Avoid mismatches between charging status and soc changes  */
	if (((charging_status == POWER_SUPPLY_STATUS_DISCHARGING) && (system_soc > last_system_soc)) || ((charging_status == POWER_SUPPLY_STATUS_CHARGING) && (system_soc < last_system_soc) && (ibat_pos_count < 3) && ((time.tv_sec > 10))))
		system_soc = last_system_soc;
	mca_log_info("smooth_new:sys_soc:%d last_sys_soc:%d soc_delta:%d charging_status:%d unit_time:%d batt_ma_avg=%d\n" ,
		system_soc, last_system_soc, soc_delta, charging_status, unit_time, batt_ma_avg);

	if(system_soc != last_system_soc){
		last_change_time = ktime_get();
		last_system_soc = system_soc;
	}
	if(system_soc > 100)
		system_soc =100;
	if(system_soc < 0)
		system_soc =0;

	if (fg->rsoc == 0 && ((fg->vbat < (fg->normal_shutdown_vbat -300) && fg->tbat > 0) || (fg->vbat < (fg->normal_shutdown_vbat - 350) && fg->tbat < 0))) {
		unit_time = 2000;
		fg->monitor_delay = FG_MONITOR_DELAY_2S;
		mca_log_err("uisoc::set 0 when volt = %dmV\n", fg->vbat);
	} else if((system_soc == 0) && (((fg->tbat < BATT_COOL_THRESHOLD) && (fg->vbat >= fg->normal_shutdown_vbat)) ||
										((fg->tbat >= BATT_COOL_THRESHOLD) && (fg->vbat >= fg->normal_shutdown_vbat)) || ((time.tv_sec <= 10)))) {
		system_soc = 1;
		mca_log_err("uisoc::hold 1 when volt = %dmV. \n", fg->vbat);
	}

	if(fg->last_soc != system_soc){
		fg->last_soc = system_soc;
	}

	return system_soc;
}

#define SIZE 3
static int get_median(int data) {
	static int queue[SIZE] = {25,25,25};
	static bool init_temp = true;
	int arr[SIZE] = {25,25,25};
	int i = 0, j = 0;
	int size = SIZE;

	mca_log_info("%s:current_temp=%d \n", __func__, data);
	if (init_temp) {
		for(j = 0; j < size; j++) {
			queue[j] = data;
		}
		mca_log_info("%s:first get tbat init queue tbat=%d \n", __func__, data);
		init_temp = false;
		return data;
	}

	for(j = 0; j < size-1; j++) {
		queue[j] = queue[j+1];
	}
	queue[size-1] = data;

	for (i=0;i<size;i++) {
		mca_log_info("%s:queue[%d]=%d \n", __func__, i, queue[i]);
	}

	for (i=0;i<size;i++) {
		arr[i] = queue[i];
	}

	for (int i = 0; i < size - 1; i++) {
		for (int j = 0; j < size - 1 - i; j++) {
			if (arr[j] > arr[j + 1]) {
				int temp = arr[j];
				arr[j] = arr[j + 1];
				arr[j + 1] = temp;
			}
		}
	}
	mca_log_info("%s:middle_temp=%d \n", __func__, arr[size / 2]);
	return arr[size / 2];
}

static int fuelgauge_strategy_get_rsoc(struct strategy_fg *fg) {
	int rsoc_master = 0, rsoc_slave = 0;
	if(fg == NULL)
		return -1;
	fuelgauge_dev_read_rsoc(fg->fg_master_dev, &rsoc_master);
	fuelgauge_dev_read_rsoc(fg->fg_slave_dev, &rsoc_slave);
	fg->rsoc = (rsoc_master * BATTA_CAPACITY + rsoc_slave * BATTB_CAPACITY)/(BATTA_CAPACITY + BATTB_CAPACITY);
	return 0;
}

static int fuelgauge_strategy_get_raw_soc(struct strategy_fg *fg) {
	int raw_soc_master = 0, raw_soc_slave = 0;
	if(fg == NULL)
		return -1;
	fuelgauge_dev_get_raw_soc(fg->fg_master_dev, &raw_soc_master);
	fuelgauge_dev_get_raw_soc(fg->fg_slave_dev, &raw_soc_slave);
	fg->raw_soc = (raw_soc_master * BATTA_CAPACITY + raw_soc_slave * BATTB_CAPACITY)/(BATTA_CAPACITY + BATTB_CAPACITY);
	return 0;
}

static int fuelgauge_strategy_read_current(struct strategy_fg *fg) {
	int current_master = 0, current_slave = 0;
	if(fg == NULL)
		return -1;
	fuelgauge_dev_read_current(fg->fg_master_dev, &current_master);
	fuelgauge_dev_read_current(fg->fg_slave_dev, &current_slave);
	fg->ibat = current_master + current_slave;
	return 0;
}

static int fuelgauge_strategy_read_bat_voltage(struct strategy_fg *fg) {
	int vbat_master = 0, vbat_slave = 0;
	if(fg == NULL)
		return -1;
	fuelgauge_dev_read_volt(fg->fg_master_dev, &vbat_master);
	fuelgauge_dev_read_volt(fg->fg_slave_dev, &vbat_slave);
	fg->vbat = vbat_master > vbat_slave ? vbat_master : vbat_slave;
	return 0;
}

static int fuelgauge_strategy_get_i2c_error_count(struct strategy_fg *fg) {
	int count_master = 0, count_slave = 0;
	if(fg == NULL)
		return 0;
	fuelgauge_dev_read_i2c_error_count(fg->fg_master_dev, &count_master);
	fuelgauge_dev_read_i2c_error_count(fg->fg_slave_dev, &count_slave);
	fg->i2c_error_count = count_master > count_slave ? count_master : count_slave;
	return fg->i2c_error_count;
}

static int fuelgauge_strategy_get_fcc(struct strategy_fg *fg) {
	int fcc_master = 0, fcc_slave = 0;
	if(fg == NULL)
		return -1;
	fuelgauge_dev_read_fcc(fg->fg_master_dev, &fcc_master);
	fuelgauge_dev_read_fcc(fg->fg_slave_dev, &fcc_slave);
	fg->fcc = fcc_master + fcc_slave;
	return 0;
}

static int fuelgauge_strategy_get_design_capacity(struct strategy_fg *fg) {
	int dc_master = 0, dc_slave = 0;
	if(fg == NULL)
		return -1;
	fuelgauge_dev_read_design_capacity(fg->fg_master_dev, &dc_master);
	fuelgauge_dev_read_design_capacity(fg->fg_slave_dev, &dc_slave);
	fg->dc = dc_master + dc_slave;
	return 0;
}

static int fuelgauge_strategy_get_cycle(struct strategy_fg *fg) {
	int cycle_master = 0, cycle_slave = 0;
	if(fg == NULL)
		return -1;
	fuelgauge_dev_read_cyclecount(fg->fg_master_dev, &cycle_master);
	fuelgauge_dev_read_cyclecount(fg->fg_slave_dev, &cycle_slave);
	fg->cycle_count = (cycle_master > cycle_slave) ? cycle_master : cycle_slave;
	return 0;
}

static int fuelgauge_strategy_get_rm(struct strategy_fg *fg) {
	int rm_master = 0, rm_slave = 0;
	if(fg == NULL)
		return -1;
	fuelgauge_dev_get_rm(fg->fg_master_dev, &rm_master);
	fuelgauge_dev_get_rm(fg->fg_slave_dev, &rm_slave);
	fg->rm = rm_master + rm_slave;
	return 0;
}

static int fuelgauge_strategy_get_soh(struct strategy_fg *fg) {
	int soh_master = 0, soh_slave = 0;
	if(fg == NULL)
		return -1;
	fuelgauge_dev_read_soh(fg->fg_master_dev, &soh_master);
	fuelgauge_dev_read_soh(fg->fg_slave_dev, &soh_slave);
	fg->soh = (soh_master > soh_slave) ? soh_master : soh_slave;
	return 0;
}

static int fuelgauge_strategy_read_avg_current(struct strategy_fg *fg) {
	int avg_current = 0, avg_current_master = 0, avg_current_slave = 0;
	if(fg == NULL)
		return 0;
	fuelgauge_dev_read_avg_current(fg->fg_master_dev, &avg_current_master);
	fuelgauge_dev_read_avg_current(fg->fg_slave_dev, &avg_current_slave);
	avg_current = avg_current_master + avg_current_slave;
	return avg_current;
}

static void fg_update_status(struct strategy_fg *fg)
{
	int temp_soc = 0,  delta_temp = 0;
	static int last_soc = -1, last_temp = 0;
	ktime_t time_now = -1;
	struct mtk_battery_manager *bm;
	struct mtk_battery *gm;
	int tbat_temp = 0;

	fuelgauge_strategy_get_cycle(fg);
	fuelgauge_strategy_get_rsoc(fg);
	fuelgauge_strategy_get_rm(fg);
	fuelgauge_strategy_get_soh(fg);
	fuelgauge_strategy_get_raw_soc(fg);
	fuelgauge_strategy_read_current(fg);
	// fg->tbat = fg_read_temperature(fg);
	fuelgauge_dev_read_temperature(fg->fg_master_dev, &tbat_temp);
	fg->tbat = get_median(tbat_temp);
	fuelgauge_strategy_read_bat_voltage(fg);
	fuelgauge_dev_read_status(fg->fg_master_dev, &fg->batt_fc);
	fuelgauge_dev_update_battery_shutdown_vol(fg->fg_master_dev, &fg->normal_shutdown_vbat);
	mca_log_err("fg_update rsoc=%d, raw_soc=%d, vbat=%d, cycle_count=%d\n", fg->rsoc, fg->raw_soc, fg->vbat, fg->cycle_count);

	if (!battery_get_psy(fg)) {
		mca_log_err("fg_update failed to get battery psy\n");
		fg->ui_soc = fg->rsoc;
		return;
	} else {
		bm = (struct mtk_battery_manager *)power_supply_get_drvdata(fg->batt_psy);
		gm = bm->gm1;
		time_now = ktime_get();
		if (probe_time != -1 && (time_now - probe_time < 10000 )) {
			fg->ui_soc = fg->rsoc;
			goto out;
		}
		fg->ui_soc = bq_battery_soc_smooth_tracking_new(fg, fg->raw_soc, fg->rsoc, fg->ibat);
		if(last_soc == -1)
			last_soc = fg->ui_soc;
		if((gm->night_charging || gm->smart_chg[SMART_CHG_ENDURANCE_PRO].en_ret) && fg->ui_soc > 80 && fg->ui_soc > last_soc){
			fg->ui_soc = last_soc;
			mca_log_err("%s last_soc = %d, soclmt[Night Endurance] = [%d %d]\n", __func__, last_soc, gm->night_charging, gm->smart_chg[SMART_CHG_ENDURANCE_PRO].en_ret);
		}

out:
		mca_log_err("[FG_STATUS] [UISOC RSOC RAWSOC TEMP_SOC SOH] = [%d %d %d %d %d], [VBAT IBAT TBAT FC] = [%d %d %d %d]\n",
			fg->ui_soc, fg->rsoc, fg->raw_soc, temp_soc, fg->soh, fg->vbat, fg->ibat, fg->tbat, fg->batt_fc);

		delta_temp = abs(last_temp - fg->tbat);
		if (fg->batt_psy && (last_soc != fg->ui_soc || delta_temp > 5 || fg->ui_soc == 0 || fg->rsoc == 0)) {
			mca_log_err("last_soc = %d, last_temp = %d, delta_temp = %d\n", last_soc, last_temp, delta_temp);
			power_supply_changed(fg->batt_psy);
		}

		last_soc = fg->ui_soc;
		if (delta_temp > 5)
			last_temp = fg->tbat;
	}
}

static void fg_monitor_workfunc(struct work_struct *work)
{
	struct strategy_fg *fg = container_of(work, struct strategy_fg, monitor_work.work);

	fg_update_status(fg);

	schedule_delayed_work(&fg->monitor_work, msecs_to_jiffies(fg->monitor_delay));
	if (fg->bms_wakelock->active)
		__pm_relax(fg->bms_wakelock);
}



static int strategy_fg_parse_dt(struct strategy_fg *fg)
{
	struct device_node *node = fg->dev->of_node;
	int ret = 0;

	if (!node) {
		mca_log_err("No DT data Failing Probe\n");
		return -EINVAL;
	}

	fg->enable_shutdown_delay = of_property_read_bool(node, "enable_shutdown_delay");
	ret |= of_property_read_u32(node, "fg_type", &fg->fg_type);
	if (ret) {
		mca_log_err("strategy fg parse dt failed, ret=%d\n", ret);
		ret = 0;
	}
    ret |= of_property_read_u32(node, "report_full_rsoc_1s", &fg->report_full_rsoc);
	if (ret)
		mca_log_err("%s failed to parse report_full_rsoc_1s\n", fg->log_tag);

    fg->max_chg_power_120w = of_property_read_bool(node, "max_chg_power_120w");
	ret = of_property_read_u32(node, "critical_shutdown_vbat_1s", &fg->critical_shutdown_vbat);
	if (ret)
		mca_log_err("%s failed to parse critical_shutdown_vbat_1s\n", fg->log_tag);

	return ret;
}

#define SHUTDOWN_DELAY_VOL	3300
#define SHUTDOWN_VOL	3400
static int fg_strategy_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val)
{
	struct strategy_fg *bq = power_supply_get_drvdata(psy);
	static bool last_shutdown_delay = false;
	union power_supply_propval pval = {0, };
	int tem = 0;
	struct mtk_battery_manager *bm;
	struct mtk_battery *gm;

	switch (psp) {
	// case POWER_SUPPLY_PROP_MODEL_NAME:
	// 	val->strval = bq->model_name;
	// 	break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		fuelgauge_strategy_read_bat_voltage(bq);
		val->intval = bq->vbat * 1000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		fuelgauge_strategy_read_current(bq);
		val->intval = bq->ibat * 1000;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (bq->fake_soc) {
			val->intval = bq->fake_soc;
			break;
		}

		if (fuelgauge_strategy_get_i2c_error_count(bq) >= 1) {
			val->intval = 15;
			break;
		}

		val->intval = bq->ui_soc;
		//add shutdown delay feature
		if (bq->enable_shutdown_delay) {
			if (val->intval <= 1) {
				tem = bq->tbat;
				if (!battery_get_psy(bq)) {
					mca_log_err("%s get capacity failed to get battery psy\n", bq->log_tag);
					break;
				} else
					power_supply_get_property(bq->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
				if (pval.intval != POWER_SUPPLY_STATUS_CHARGING) {
					if(bq->shutdown_delay == true) {
						val->intval = 1;
					} else if ((bq->vbat <= bq->critical_shutdown_vbat) &&
							bq->shutdown_flag == false) {
						bq->shutdown_delay = true;
						val->intval = 1;
					} else {
						bq->shutdown_delay = false;
					}
					mca_log_err("%s last_shutdown= %d. shutdown= %d, soc =%d, voltage =%d\n", bq->log_tag, last_shutdown_delay, bq->shutdown_delay, val->intval, bq->vbat);
				} else {
					bq->shutdown_delay = false;
					if ((bq->vbat >= (bq->critical_shutdown_vbat - 350)) &&
							bq->shutdown_flag == false) {
						val->intval = 1;
					}
				}
			} else {
				bq->shutdown_delay = false;
			}

			if (val->intval <= 0)
				bq->shutdown_flag = true;
			else
				bq->shutdown_flag = false;

			if (bq->shutdown_flag)
				val->intval = 0;

			if (last_shutdown_delay != bq->shutdown_delay || val->intval == 0) {
				last_shutdown_delay = bq->shutdown_delay;
				if (bq->fg_psy)
					power_supply_changed(bq->fg_psy);
				mca_log_err("%s power_supply_changed\n", bq->log_tag);
			}
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (bq->fake_tbat) {
			val->intval = bq->fake_tbat;
			break;
		}
		if (!battery_get_psy(bq)) {
			mca_log_err("%s fg_update failed to get battery psy\n", bq->log_tag);
		} else {
			bm = (struct mtk_battery_manager *)power_supply_get_drvdata(bq->batt_psy);
			gm = bm->gm1;
			if(gm != NULL){
				if (gm->extreme_cold_chg_flag) {
					if (bq->tbat > 50)
						bq->extreme_cold_temp = 60;
					else
						bq->extreme_cold_temp = 50;
				} else
					bq->extreme_cold_temp = 0;
			}
		}
		// mca_log_info("%s update tbat=%d, extreme_cold_temp = %d\n", bq->log_tag, bq->tbat, bq->extreme_cold_temp);
		val->intval = bq->tbat - bq->extreme_cold_temp;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		fuelgauge_strategy_get_fcc(bq);
		val->intval = bq->fcc;
		val->intval *= 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = bq->dc;
		val->intval *= 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = bq->rm * 1000;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = bq->cycle_count;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int fg_strategy_set_property(struct power_supply *psy, enum power_supply_property prop, const union power_supply_propval *val)
{
	struct strategy_fg *bq = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
		bq->fake_tbat = val->intval;
		power_supply_changed(bq->fg_psy);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		bq->fake_soc = val->intval;
		power_supply_changed(bq->fg_psy);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		bq->fake_cycle_count = val->intval;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static char *mtk_bms_supplied_to[] = {
        "battery",
        "usb",
};

static int fg_strategy_prop_is_writeable(struct power_supply *psy, enum power_supply_property prop)
{
	int ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static ssize_t bms_strategy_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy;
	struct strategy_fg *gm;
	struct mtk_bms_strategy_sysfs_field_info *usb_attr;
	int val;
	ssize_t ret;

	ret = kstrtos32(buf, 0, &val);
	if (ret < 0)
		return ret;

	psy = dev_get_drvdata(dev);
	gm = (struct strategy_fg *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct mtk_bms_strategy_sysfs_field_info, attr);
	if (usb_attr->set != NULL)
		usb_attr->set(gm, usb_attr, val);

	return count;
}

static ssize_t bms_strategy_sysfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct strategy_fg *gm;
	struct mtk_bms_strategy_sysfs_field_info *usb_attr;
	int val = 0;
	ssize_t count;

	psy = dev_get_drvdata(dev);
	gm = (struct strategy_fg *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct mtk_bms_strategy_sysfs_field_info, attr);
	if (usb_attr->get != NULL)
		usb_attr->get(gm, usb_attr, &val);

	count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
	return count;
}

static int fcc_get(struct strategy_fg *gm,
	struct mtk_bms_strategy_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->fcc;
	else
		*val = 0;
	mca_log_err("%s %d\n", __func__, *val);
	return 0;
}

static int monitor_delay_get(struct strategy_fg *gm,
	struct mtk_bms_strategy_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->monitor_delay;
	else
		*val = 0;
	mca_log_err("%s %d\n", __func__, *val);
	return 0;
}

static int monitor_delay_set(struct strategy_fg *gm,
	struct mtk_bms_strategy_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->monitor_delay = val;
	mca_log_err("%s %d\n", __func__, val);
	return 0;
}

static int rm_get(struct strategy_fg *gm,
	struct mtk_bms_strategy_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->rm;
	else
		*val = 0;
	mca_log_err("%s %d\n", __func__, *val);
	return 0;
}

static int rsoc_get(struct strategy_fg *gm,
	struct mtk_bms_strategy_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->rsoc;
	else
		*val = 0;
	mca_log_err("%s %d\n", __func__, *val);
	return 0;
}

static int shutdown_delay_get(struct strategy_fg *gm,
	struct mtk_bms_strategy_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->shutdown_delay;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int capacity_raw_get(struct strategy_fg *gm,
	struct mtk_bms_strategy_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->raw_soc;
	else
		*val = 0;
	mca_log_err("%s %d\n", __func__, *val);
	return 0;
}

static int av_current_get(struct strategy_fg *gm,
	struct mtk_bms_strategy_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = fuelgauge_strategy_read_avg_current(gm);
	else
		*val = 0;
	mca_log_err("%s %d\n", __func__, *val);
	return 0;
}

static int soh_get(struct strategy_fg *gm,
	struct mtk_bms_strategy_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->soh;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

/* Must be in the same order as BMS_PROP_* */
static struct mtk_bms_strategy_sysfs_field_info bms_sysfs_field_tbl[] = {
	// BMS_STRATEGY_SYSFS_FIELD_RW(fastcharge_mode, BMS_PROP_FASTCHARGE_MODE),
	BMS_STRATEGY_SYSFS_FIELD_RW(monitor_delay, BMS_STRATEGY_PROP_MONITOR_DELAY),
	BMS_STRATEGY_SYSFS_FIELD_RO(fcc, BMS_STRATEGY_PROP_FCC),
	BMS_STRATEGY_SYSFS_FIELD_RO(rm, BMS_STRATEGY_PROP_RM),
	BMS_STRATEGY_SYSFS_FIELD_RO(rsoc, BMS_STRATEGY_PROP_RSOC),
	BMS_STRATEGY_SYSFS_FIELD_RO(shutdown_delay, BMS_STRATEGY_PROP_SHUTDOWN_DELAY),
	BMS_STRATEGY_SYSFS_FIELD_RO(capacity_raw, BMS_STRATEGY_PROP_CAPACITY_RAW),
	// BMS_STRATEGY_SYSFS_FIELD_RO(soc_decimal, BMS_PROP_SOC_DECIMAL),
	// BMS_STRATEGY_SYSFS_FIELD_RO(soc_decimal_rate, BMS_PROP_SOC_DECIMAL_RATE),
	// BMS_STRATEGY_SYSFS_FIELD_RO(resistance_id, BMS_PROP_RESISTANCE_ID),
	// BMS_STRATEGY_SYSFS_FIELD_RW(authentic, BMS_PROP_AUTHENTIC),
	// BMS_STRATEGY_SYSFS_FIELD_RW(shutdown_mode, BMS_PROP_SHUTDOWN_MODE),
	// BMS_STRATEGY_SYSFS_FIELD_RO(chip_ok, BMS_PROP_CHIP_OK),
	// BMS_STRATEGY_SYSFS_FIELD_RO(charge_done, BMS_PROP_CHARGE_DONE),
	BMS_STRATEGY_SYSFS_FIELD_RO(soh, BMS_STRATEGY_PROP_SOH),
	// BMS_STRATEGY_SYSFS_FIELD_RO(soh_new, BMS_PROP_SOH_NEW),
	// BMS_STRATEGY_SYSFS_FIELD_RO(resistance, BMS_PROP_RESISTANCE),
	// BMS_STRATEGY_SYSFS_FIELD_RW(i2c_error_count, BMS_PROP_I2C_ERROR_COUNT),
	BMS_STRATEGY_SYSFS_FIELD_RO(av_current, BMS_STRATEGY_PROP_AV_CURRENT),
	// BMS_STRATEGY_SYSFS_FIELD_RO(voltage_max, BMS_PROP_VOLTAGE_MAX),
	// BMS_STRATEGY_SYSFS_FIELD_RO(temp_max, BMS_PROP_TEMP_MAX),
	// BMS_STRATEGY_SYSFS_FIELD_RO(temp_min, BMS_PROP_TEMP_MIN),
	// BMS_STRATEGY_SYSFS_FIELD_RO(time_ot, BMS_PROP_TIME_OT),
	// BMS_STRATEGY_SYSFS_FIELD_RO(bms_slave_connect_error, BMS_PROP_BMS_SLAVE_CONNECT_ERROR),
	// BMS_STRATEGY_SYSFS_FIELD_RO(cell_supplier, BMS_PROP_CELL_SUPPLIER),
	// BMS_STRATEGY_SYSFS_FIELD_RO(isc_alert_level, BMS_PROP_ISC_ALERT_LEVEL),
	// BMS_STRATEGY_SYSFS_FIELD_RO(soa_alert_level, BMS_PROP_SOA_ALERT_LEVEL),
	// BMS_STRATEGY_SYSFS_FIELD_RO(shutdown_voltage, BMS_PROP_SHUTDOWN_VOL),
	// BMS_STRATEGY_SYSFS_FIELD_RW(charge_eoc, BMS_PROP_CHARGE_EOC),
	// BMS_STRATEGY_SYSFS_FIELD_RW(charging_done, BMS_PROP_CHARGING_DONE),
	// BMS_STRATEGY_SYSFS_FIELD_RO(calc_rvalue, BMS_PROP_CALC_RVALUE),
	// BMS_STRATEGY_SYSFS_FIELD_RW(aged_in_advance, BMS_PROP_AGED_IN_ADVANCE),
	// BMS_STRATEGY_SYSFS_FIELD_RW(eea_chg_support, BMS_PROP_EEA_CHG_SUPPORT),
	// BMS_STRATEGY_SYSFS_FIELD_RO(real_temp, BMS_PROP_REAL_TEMP),
	// BMS_STRATEGY_SYSFS_FIELD_RO(vendor, BMS_PROP_BATTERY_VENDOR),
	// BMS_STRATEGY_SYSFS_FIELD_RO(pack_vendor, BMS_PROP_BATTERY_PACK_VENDOR),
	// BMS_STRATEGY_SYSFS_FIELD_RW(dod_count, BMS_PROP_DOD_COUNT),
	// BMS_STRATEGY_SYSFS_FIELD_RO(adapting_power, BMS_PROP_ADAP_POWER),
};

int bms_strategy_get_property(enum bms_strategy_property bp,
			    int *val)
{
	struct strategy_fg *gm;
	struct power_supply *psy;

	psy = power_supply_get_by_name("bms_strategy");
	if (psy == NULL)
		return -ENODEV;

	gm = (struct strategy_fg *)power_supply_get_drvdata(psy);
	if (bms_sysfs_field_tbl[bp].prop == bp)
		bms_sysfs_field_tbl[bp].get(gm,
			&bms_sysfs_field_tbl[bp], val);
	else {
		mca_log_err("%s usb bp:%d idx error\n", __func__, bp);
		return -ENOTSUPP;
	}

	return 0;
}
EXPORT_SYMBOL(bms_strategy_get_property);

int bms_strategy_set_property(enum bms_strategy_property bp,
			    int val)
{
	struct strategy_fg *gm;
	struct power_supply *psy;

	psy = power_supply_get_by_name("bms_strategy");
	if (psy == NULL)
		return -ENODEV;

	gm = (struct strategy_fg *)power_supply_get_drvdata(psy);

	if (bms_sysfs_field_tbl[bp].prop == bp)
		bms_sysfs_field_tbl[bp].set(gm,
			&bms_sysfs_field_tbl[bp], val);
	else {
		mca_log_err("%s usb bp:%d idx error\n", __func__, bp);
		return -ENOTSUPP;
	}
	return 0;
}
EXPORT_SYMBOL(bms_strategy_set_property);

static struct attribute *
	bms_sysfs_attrs[ARRAY_SIZE(bms_sysfs_field_tbl) + 1];

static const struct attribute_group bms_sysfs_attr_group = {
	.attrs = bms_sysfs_attrs,
};

static void bms_sysfs_init_attrs(void)
{
	int i, limit = ARRAY_SIZE(bms_sysfs_field_tbl);

	for (i = 0; i < limit; i++)
		bms_sysfs_attrs[i] = &bms_sysfs_field_tbl[i].attr.attr;

	bms_sysfs_attrs[limit] = NULL; /* Has additional entry for this */
}

static int bms_sysfs_create_group(struct power_supply *psy)
{
	bms_sysfs_init_attrs();

	return sysfs_create_group(&psy->dev.kobj,
			&bms_sysfs_attr_group);
}

static int fg_strategy_init_psy(struct strategy_fg *bq)
{
	struct power_supply_config fg_psy_cfg = {};

	bq->fg_psy_d.name = "bms_strategy";
	bq->fg_psy_d.type = POWER_SUPPLY_TYPE_BATTERY;
	bq->fg_psy_d.properties = fg_props;
	bq->fg_psy_d.num_properties = ARRAY_SIZE(fg_props);
	bq->fg_psy_d.get_property = fg_strategy_get_property;
	bq->fg_psy_d.set_property = fg_strategy_set_property;
	bq->fg_psy_d.property_is_writeable = fg_strategy_prop_is_writeable;
	fg_psy_cfg.supplied_to = mtk_bms_supplied_to;
	fg_psy_cfg.num_supplicants = ARRAY_SIZE(mtk_bms_supplied_to);
	fg_psy_cfg.drv_data = bq;

	bq->fg_psy = devm_power_supply_register(bq->dev, &bq->fg_psy_d, &fg_psy_cfg);
	if (IS_ERR(bq->fg_psy)) {
		mca_log_err("%s failed to register fg_psy", bq->log_tag);
		return PTR_ERR(bq->fg_psy);
	} else
	    bms_sysfs_create_group(bq->fg_psy);

	return 0;
}

static int strategy_fg_probe(struct platform_device *pdev)
{
	struct strategy_fg *fg;
	char *name = NULL;
	int ret = 0;

	mca_log_info("%s probe_begin\n", __func__);
	fg = devm_kzalloc(&pdev->dev, sizeof(*fg), GFP_KERNEL);
	if (!fg) {
		mca_log_err("out of memory\n");
		return -ENOMEM;
	}
	fg->dev = &pdev->dev;
	fg->raw_soc = -ENODATA;
	fg->last_soc = -EINVAL;
	fg->extreme_cold_temp = 0;
	fg->monitor_delay = FG_MONITOR_DELAY_30S;
	fg->shutdown_flag = false;
	strcpy(fg->log_tag, "[FUELGAUGE_STARTEGY]");
	platform_set_drvdata(pdev, fg);
	name = devm_kasprintf(fg->dev, GFP_KERNEL, "%s", "fg suspend wakelock");
	fg->bms_wakelock = wakeup_source_register(NULL, name);
	ret = strategy_fg_parse_dt(fg);
	// if (ret)
	// 	return ret;
    probe_time = ktime_get();
	fg->fg_master_dev = get_fuelgauge_by_name("fg_master");
	if (!fg->fg_master_dev) {
		mca_log_err("failed to get fg_master_dev\n");
		return -1;
	}
	fg->fg_slave_dev = get_fuelgauge_by_name("fg_slave");
	if (!fg->fg_slave_dev) {
		mca_log_err("failed to get fg_slave_dev\n");
		return -1;
	}
	ret = fg_strategy_init_psy(fg);
	if (ret) {
		mca_log_err("%s failed to init fg_strategy psy\n", fg->log_tag);
		return ret;
	}

	INIT_DELAYED_WORK(&fg->monitor_work, fg_monitor_workfunc);
	schedule_delayed_work(&fg->monitor_work, msecs_to_jiffies(5000));
	fuelgauge_strategy_get_design_capacity(fg);

	mca_log_info("%s probe end\n", __func__);
	return ret;
}

static int strategy_fg_remove(struct platform_device *pdev)
{
	return 0;
}

static int strategy_fg_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct strategy_fg *fg = (struct strategy_fg *)platform_get_drvdata(pdev);

	if (!fg)
		return -1;
	fuelgauge_dev_set_fg_in_sleep(fg->fg_master_dev, 0);
	fuelgauge_dev_set_fg_in_sleep(fg->fg_slave_dev, 0);
	cancel_delayed_work_sync(&fg->monitor_work);
	mca_log_err("%s strategy_fg_suspend\n", fg->log_tag);
	return 0;
}

static int strategy_fg_resume(struct platform_device *pdev)
{
	struct strategy_fg *fg = (struct strategy_fg *)platform_get_drvdata(pdev);

	if (!fg)
		return -1;
	fuelgauge_dev_set_fg_in_sleep(fg->fg_master_dev, 1);
	fuelgauge_dev_set_fg_in_sleep(fg->fg_slave_dev, 1);
	if (!fg->bms_wakelock->active)
		__pm_stay_awake(fg->bms_wakelock);
	schedule_delayed_work(&fg->monitor_work, 0);
	mca_log_err("%s strategy_fg_resume\n", fg->log_tag);
	return 0;
}

static void strategy_fg_shutdown(struct platform_device *pdev)
{
	struct strategy_fg *fg = (struct strategy_fg *)platform_get_drvdata(pdev);

	mca_log_err("%s strategy_fg_shutdown\n", fg->log_tag);
}

static const struct of_device_id match_table[] = {
	{.compatible = "xiaomi,strategy_fg"},
	{},
};

static struct platform_driver strategy_fg_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "strategy_fg",
		.of_match_table = match_table,
	},
	.probe = strategy_fg_probe,
	.remove = strategy_fg_remove,
	.suspend = strategy_fg_suspend,
	.resume = strategy_fg_resume,
	.shutdown = strategy_fg_shutdown,
};

static int __init strategy_fg_init(void)
{
	return platform_driver_register(&strategy_fg_driver);
}
module_init(strategy_fg_init);

static void __exit strategy_fg_exit(void)
{
	platform_driver_unregister(&strategy_fg_driver);
}
module_exit(strategy_fg_exit);

MODULE_DESCRIPTION("Strategy Fuel Gauge");
MODULE_AUTHOR("xiezhichang@xiaomi.com");
MODULE_LICENSE("GPL v2");
