#ifndef __FUELGAUGE_STRATEGY_H
#define __FUELGAUGE_STRATEGY_H

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/alarmtimer.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include "fuelgauge_class.h"

#define BMS_STRATEGY_SYSFS_FIELD_RW(_name, _prop)	\
{									 \
	.attr	= __ATTR(_name, 0644, bms_strategy_sysfs_show, bms_strategy_sysfs_store),\
	.prop	= _prop,	\
	.set	= _name##_set,						\
	.get	= _name##_get,						\
}
#define BMS_STRATEGY_SYSFS_FIELD_RO(_name, _prop)	\
{			\
	.attr   = __ATTR(_name, 0444, bms_strategy_sysfs_show, bms_strategy_sysfs_store),\
	.prop   = _prop,				  \
	.get	= _name##_get,						\
}
enum bms_strategy_property {
	// BMS_PROP_FASTCHARGE_MODE,
	BMS_STRATEGY_PROP_MONITOR_DELAY,
	BMS_STRATEGY_PROP_FCC,
	BMS_STRATEGY_PROP_RM,
	BMS_STRATEGY_PROP_RSOC,
	BMS_STRATEGY_PROP_SHUTDOWN_DELAY,
	BMS_STRATEGY_PROP_CAPACITY_RAW,
	// BMS_PROP_SOC_DECIMAL,
	// BMS_PROP_SOC_DECIMAL_RATE,
	// BMS_PROP_RESISTANCE_ID,
	// BMS_PROP_AUTHENTIC,
	// BMS_PROP_SHUTDOWN_MODE,
	// BMS_PROP_CHIP_OK,
	// BMS_PROP_CHARGE_DONE,
	// BMS_PROP_SOH,
	BMS_STRATEGY_PROP_SOH,
	// BMS_PROP_SOH_NEW,
	// BMS_PROP_RESISTANCE,
	// BMS_PROP_I2C_ERROR_COUNT,
	BMS_STRATEGY_PROP_AV_CURRENT,
	// BMS_PROP_VOLTAGE_MAX,
	// BMS_PROP_TEMP_MAX,
	// BMS_PROP_TEMP_MIN,
	// BMS_PROP_TIME_OT,
	// BMS_PROP_BMS_SLAVE_CONNECT_ERROR,
	// BMS_PROP_CELL_SUPPLIER,
	// BMS_PROP_ISC_ALERT_LEVEL,
	// BMS_PROP_SOA_ALERT_LEVEL,
	// BMS_PROP_SHUTDOWN_VOL,
	// BMS_PROP_CHARGE_EOC,
	// BMS_PROP_CHARGING_DONE,
	// BMS_PROP_CALC_RVALUE,
	// BMS_PROP_AGED_IN_ADVANCE,
	// BMS_PROP_EEA_CHG_SUPPORT,
	// BMS_PROP_REAL_TEMP,
	// BMS_PROP_BATTERY_VENDOR,
	// BMS_PROP_BATTERY_PACK_VENDOR,
	// BMS_PROP_DOD_COUNT,
	// BMS_PROP_ADAP_POWER,
};

struct strategy_fg {
	struct device *dev;
	struct delayed_work monitor_work;
    struct fg_device *fg_master_dev;
	struct fg_device *fg_slave_dev;
	struct wakeup_source *bms_wakelock;
	struct power_supply *fg_psy;
	struct power_supply_desc fg_psy_d;
	struct fg_properties fg_props;
    struct power_supply *batt_psy;
    char log_tag[30];

    int cycle_count;
    int rsoc;
    int soh;
    int raw_soc;
    int ibat;
    int tbat;
    int vbat;
    int ui_soc;
	int rm;
    int monitor_delay;
    int fg_type;
    int report_full_rsoc;
    int normal_shutdown_vbat;
    int last_soc;
	int fake_soc;
	int i2c_error_count;
	int extreme_cold_temp;
	int dc;
    int fcc;
    int critical_shutdown_vbat;
    int fake_tbat;
    int fake_cycle_count;

    bool batt_fc;
    bool is_eea_model;
    bool charging_done;
    bool en_smooth_full;
	bool enable_shutdown_delay;
    bool shutdown_delay;
    bool shutdown_flag;
    bool max_chg_power_120w;
};

struct mtk_bms_strategy_sysfs_field_info {
	struct device_attribute attr;
	enum bms_strategy_property prop;
	int (*set)(struct strategy_fg *gm,
		struct mtk_bms_strategy_sysfs_field_info *attr, int val);
	int (*get)(struct strategy_fg *gm,
		struct mtk_bms_strategy_sysfs_field_info *attr, int *val);
};
int bms_strategy_set_property(enum bms_strategy_property bp,
			    int val);
int bms_strategy_get_property(enum bms_strategy_property bp,
                int *val);

#endif /* __PMIC_VOTER_H */