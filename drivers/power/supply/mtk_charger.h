/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CHARGER_H
#define __MTK_CHARGER_H

#include <linux/alarmtimer.h>
#include "charger_class.h"
#include "adapter_class.h"
#include "pmic_voter.h"
#include "mtk_charger_algorithm_class.h"
#include <linux/power_supply.h>
#include <linux/types.h>
#include <linux/hrtimer.h>
#include "mtk_smartcharging.h"
#include "step_jeita_charge.h"
#include "../../gpu/drm/mediatek/mediatek_v2/mi_disp/mi_disp_notifier.h"

#define CHARGING_INTERVAL 10
#define CHARGING_FULL_INTERVAL 20

#define CHRLOG_ERROR_LEVEL	1
#define CHRLOG_INFO_LEVEL	2
#define CHRLOG_DEBUG_LEVEL	3

#define SC_TAG "smartcharging"

extern int chr_get_debug_level(void);

#define chr_err(fmt, args...)					\
do {								\
	if (chr_get_debug_level() >= CHRLOG_ERROR_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

#define chr_info(fmt, args...)					\
do {								\
	if (chr_get_debug_level() >= CHRLOG_INFO_LEVEL) {	\
		pr_notice_ratelimited(fmt, ##args);		\
	}							\
} while (0)

#define chr_debug(fmt, args...)					\
do {								\
	if (chr_get_debug_level() >= CHRLOG_DEBUG_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

#define is_between(left, right, value)				\
			(((left) >= (right) && (left) >= (value)	\
				&& (value) >= (right))			\
			|| ((left) <= (right) && (left) <= (value)	\
				&& (value) <= (right)))

struct mtk_charger;
struct charger_data;
#define BATTERY_CV 4350000
#define V_CHARGER_MAX 6500000 /* 6.5 V */
#define V_CHARGER_MIN 4600000 /* 4.6 V */
#define VBUS_OVP_VOLTAGE 15000000 /* 15V */

#define USB_CHARGER_CURRENT_SUSPEND		0 /* def CONFIG_USB_IF */
#define USB_CHARGER_CURRENT_UNCONFIGURED	70000 /* 70mA */
#define USB_CHARGER_CURRENT_CONFIGURED		500000 /* 500mA */
#define USB_CHARGER_CURRENT			500000 /* 500mA */
#define AC_CHARGER_CURRENT			2050000
#define AC_CHARGER_INPUT_CURRENT		3200000
#define NON_STD_AC_CHARGER_CURRENT		500000
#define CHARGING_HOST_CHARGER_CURRENT		650000

/* dynamic mivr */
#define V_CHARGER_MIN_1 4400000 /* 4.4 V */
#define V_CHARGER_MIN_2 4200000 /* 4.2 V */
#define MAX_DMIVR_CHARGER_CURRENT 1800000 /* 1.8 A */

/* battery warning */
#define BATTERY_NOTIFY_CASE_0001_VCHARGER
#define BATTERY_NOTIFY_CASE_0002_VBATTEMP

/* charging abnormal status */
#define CHG_VBUS_OV_STATUS	(1 << 0)
#define CHG_BAT_OT_STATUS	(1 << 1)
#define CHG_OC_STATUS		(1 << 2)
#define CHG_BAT_OV_STATUS	(1 << 3)
#define CHG_ST_TMO_STATUS	(1 << 4)
#define CHG_BAT_LT_STATUS	(1 << 5)
#define CHG_TYPEC_WD_STATUS	(1 << 6)
#define CHG_DPDM_OV_STATUS	(1 << 7)

/* Battery Temperature Protection */
#define MIN_CHARGE_TEMP  0
#define MIN_CHARGE_TEMP_PLUS_X_DEGREE	6
#define MAX_CHARGE_TEMP  50
#define MAX_CHARGE_TEMP_MINUS_X_DEGREE	47

/* Smart charger */
#define SMART_CHG_OUTDOOR_CHARGE_INC 300000

#define MAX_ALG_NO 10

#define RESET_BOOT_VOLT_TIME 50

/* LPD */
#define LPD_TRIGGER_THRESHOLD 39

#if defined(CONFIG_RUST_DETECTION)
enum RUST_DET_PIN{
	RUST_DET_CC_PIN,
	RUST_DET_DP_PIN,
	RUST_DET_DM_PIN,
	RUST_DET_SBU1_PIN,
	RUST_DET_SBU2_PIN,
};
#endif

enum bat_temp_state_enum {
	BAT_TEMP_LOW = 0,
	BAT_TEMP_NORMAL,
	BAT_TEMP_HIGH
};

enum chg_dev_notifier_events {
	EVENT_FULL,
	EVENT_RECHARGE,
	EVENT_DISCHARGE,
};

struct battery_thermal_protection_data {
	int sm;
	bool enable_min_charge_temp;
	int min_charge_temp;
	int min_charge_temp_plus_x_degree;
	int max_charge_temp;
	int max_charge_temp_minus_x_degree;
};

enum quick_charge_type {
	QUICK_CHARGE_NORMAL = 0,
	QUICK_CHARGE_FAST,
	QUICK_CHARGE_FLASH,
	QUICK_CHARGE_TURBE,
	QUICK_CHARGE_SUPER,
	QUICK_CHARGE_MAX,
};

enum xmusb350_chg_type {
	XMUSB350_TYPE_OCP = 0x1,
	XMUSB350_TYPE_FLOAT = 0x2,
	XMUSB350_TYPE_SDP = 0x3,
	XMUSB350_TYPE_CDP = 0x4,
	XMUSB350_TYPE_DCP = 0x5,
	XMUSB350_TYPE_HVDCP_2 = 0x6,
	XMUSB350_TYPE_HVDCP_3 = 0x7,
	XMUSB350_TYPE_HVDCP_35_18 = 0x8,
	XMUSB350_TYPE_HVDCP_35_27 = 0x9,
	XMUSB350_TYPE_HVDCP_3_18 = 0xA,
	XMUSB350_TYPE_HVDCP_3_27 = 0xB,
	XMUSB350_TYPE_PD = 0xC,
	XMUSB350_TYPE_PD_DR = 0xD,
	XMUSB350_TYPE_HVCHG = 0xE,
	XMUSB350_TYPE_HVDCP = 0x10,
	XMUSB350_TYPE_UNKNOW = 0x11,
};

enum xmusb350_pulse_type {
	QC3_DM_PULSE,
	QC3_DP_PULSE,
	QC35_DM_PULSE,
	QC35_DP_PULSE,
};

enum xmusb350_qc_mode {
	QC_MODE_QC2_5 = 1,
	QC_MODE_QC2_9,
	QC_MODE_QC2_12,
	QC_MODE_QC3_5,
	QC_MODE_QC35_5,
};

//enum mt6375_usbsw {
//	USBSW_CHG = 0,
//	USBSW_USB,
//};

enum hvdcp3_type {
	HVDCP3_NONE,
	HVDCP3_18,
	HVDCP3_27,
	HVDCP35_18,
	HVDCP35_27,
};

enum product_no{
	PRODUCT_NO,
	DUCHAMP_CN,
	DUCHAMP_GL,
};


enum product_name{
	UNKNOWN_PROJECT_NAME,
	DUCHAMP,
};

enum {
	CHG_STAT_SLEEP,
	CHG_STAT_VBUS_RDY,
	CHG_STAT_TRICKLE,
	CHG_STAT_PRE,
	CHG_STAT_FAST,
	CHG_STAT_EOC,
	CHG_STAT_BKGND,
	CHG_STAT_DONE,
	CHG_STAT_FAULT,
	CHG_STAT_OTG = 15,
	CHG_STAT_MAX,
};

/* battery cycle */
#define BATTERY_CYCLE_1_TO_100 1
#define BATTERY_CYCLE_100_TO_200 100
#define BATTERY_CYCLE_200_TO_300 200
#define BATTERY_CYCLE_300_TO_400 300
#define BATTERY_CYCLE_400_TO_500 400
#define BATTERY_CYCLE_500_TO_600 500
#define BATTERY_CYCLE_600_TO_800 600
#define BATTERY_CYCLE_800_TO_900 800
#define BATTERY_CYCLE_900_TO_950 900
#define BATTERY_CYCLE_950_TO_1000 950
#define BATTERY_CYCLE_1000_TO_1050 1000
#define BATTERY_CYCLE_1050_TO_1100 1050
#define BATTERY_CYCLE_1100_TO_1600 1100

/* sw jeita */
#define JEITA_TEMP_ABOVE_T4_CV	4240000
#define JEITA_TEMP_T3_TO_T4_CV	4240000
#define JEITA_TEMP_T2_TO_T3_CV	4340000
#define JEITA_TEMP_T1_TO_T2_CV	4240000
#define JEITA_TEMP_T0_TO_T1_CV	4040000
#define JEITA_TEMP_BELOW_T0_CV	4040000
#define TEMP_T4_THRES  50
#define TEMP_T4_THRES_MINUS_X_DEGREE 47
#define TEMP_T3_THRES  45
#define TEMP_T3_THRES_MINUS_X_DEGREE 39
#define TEMP_T2_THRES  10
#define TEMP_T2_THRES_PLUS_X_DEGREE 16
#define TEMP_T1_THRES  0
#define TEMP_T1_THRES_PLUS_X_DEGREE 6
#define TEMP_T0_THRES  0
#define TEMP_T0_THRES_PLUS_X_DEGREE  0
#define TEMP_NEG_10_THRES 0

/*
 * Software JEITA
 * T0: -10 degree Celsius
 * T1: 0 degree Celsius
 * T2: 10 degree Celsius
 * T3: 45 degree Celsius
 * T4: 50 degree Celsius
 */
enum sw_jeita_state_enum {
	TEMP_BELOW_T0 = 0,
	TEMP_T0_TO_T1,
	TEMP_T1_TO_T2,
	TEMP_T2_TO_T3,
	TEMP_T3_TO_T4,
	TEMP_ABOVE_T4
};

struct sw_jeita_data {
	int sm;
	int pre_sm;
	int cv;
	bool charging;
	bool error_recovery_flag;
};

struct mtk_charger_algorithm {

	int (*do_algorithm)(struct mtk_charger *info);
	int (*enable_charging)(struct mtk_charger *info, bool en);
	int (*do_event)(struct notifier_block *nb, unsigned long ev, void *v);
	int (*do_dvchg1_event)(struct notifier_block *nb, unsigned long ev,
			       void *v);
	int (*do_dvchg2_event)(struct notifier_block *nb, unsigned long ev,
			       void *v);
	int (*do_hvdvchg1_event)(struct notifier_block *nb, unsigned long ev,
			       void *v);
	int (*do_hvdvchg2_event)(struct notifier_block *nb, unsigned long ev,
			       void *v);
	int (*change_current_setting)(struct mtk_charger *info);
	void *algo_data;
};

struct charger_custom_data {
	int battery_cv;	/* uv */
	int max_charger_voltage;
	int max_charger_voltage_setting;
	int min_charger_voltage;
	int vbus_sw_ovp_voltage;

	int usb_charger_current;
	int ac_charger_current;
	int ac_charger_input_current;
	int pd2_input_current;
	int charging_host_charger_current;

	/* sw jeita */
	int jeita_temp_above_t4_cv;
	int jeita_temp_t3_to_t4_cv;
	int jeita_temp_t2_to_t3_cv;
	int jeita_temp_t1_to_t2_cv;
	int jeita_temp_t0_to_t1_cv;
	int jeita_temp_below_t0_cv;
	int temp_t4_thres;
	int temp_t4_thres_minus_x_degree;
	int temp_t3_thres;
	int temp_t3_thres_minus_x_degree;
	int temp_t2_thres;
	int temp_t2_thres_plus_x_degree;
	int temp_t1_thres;
	int temp_t1_thres_plus_x_degree;
	int temp_t0_thres;
	int temp_t0_thres_plus_x_degree;
	int temp_neg_10_thres;

	/* battery temperature protection */
	int mtk_temperature_recharge_support;
	int max_charge_temp;
	int max_charge_temp_minus_x_degree;
	int min_charge_temp;
	int min_charge_temp_plus_x_degree;

	/* dynamic mivr */
	int min_charger_voltage_1;
	int min_charger_voltage_2;
	int max_dmivr_charger_current;

};

struct charger_data {
	int input_current_limit;
	int charging_current_limit;

	int force_charging_current;
	int thermal_input_current_limit;
	int thermal_charging_current_limit;
	bool thermal_throttle_record;
	int disable_charging_count;
	int input_current_limit_by_aicl;
	int junction_temp_min;
	int junction_temp_max;
};

enum chg_data_idx_enum {
	CHG1_SETTING,
	CHG2_SETTING,
	DVCHG1_SETTING,
	DVCHG2_SETTING,
	HVDVCHG1_SETTING,
	HVDVCHG2_SETTING,
	CHGS_SETTING_MAX,
};

struct mtk_charger {
	struct platform_device *pdev;
	struct charger_device *chg1_dev;
	struct notifier_block chg1_nb;
	struct charger_device *chg2_dev;
	struct charger_device *dvchg1_dev;
	struct notifier_block dvchg1_nb;
	struct charger_device *dvchg2_dev;
	struct notifier_block dvchg2_nb;
	struct charger_device *hvdvchg1_dev;
	struct notifier_block hvdvchg1_nb;
	struct charger_device *hvdvchg2_dev;
	struct notifier_block hvdvchg2_nb;
	struct charger_device *bkbstchg_dev;
	struct notifier_block bkbstchg_nb;
	struct charger_device *usb350_dev;
	struct charger_device *cp_master;
	struct charger_device *cp_slave;

	struct charger_data chg_data[CHGS_SETTING_MAX];
	struct chg_limit_setting setting;
	enum charger_configuration config;

	struct power_supply_desc psy_desc1;
	struct power_supply_config psy_cfg1;
	struct power_supply *psy1;

	struct power_supply_desc psy_desc2;
	struct power_supply_config psy_cfg2;
	struct power_supply *psy2;

	struct power_supply_desc psy_dvchg_desc1;
	struct power_supply_config psy_dvchg_cfg1;
	struct power_supply *psy_dvchg1;

	struct power_supply_desc psy_dvchg_desc2;
	struct power_supply_config psy_dvchg_cfg2;
	struct power_supply *psy_dvchg2;

	struct power_supply_desc psy_hvdvchg_desc1;
	struct power_supply_config psy_hvdvchg_cfg1;
	struct power_supply *psy_hvdvchg1;

	struct power_supply_desc psy_hvdvchg_desc2;
	struct power_supply_config psy_hvdvchg_cfg2;
	struct power_supply *psy_hvdvchg2;

	struct power_supply  *chg_psy;
	struct power_supply  *bc12_psy;
	struct power_supply  *bat_psy;
	struct power_supply_desc usb_desc;
	struct power_supply_config usb_cfg;
	struct power_supply *usb_psy;
	struct power_supply *bms_psy;
	struct power_supply *battery_psy;

	struct adapter_device *pd_adapter;
	struct notifier_block pd_nb;
	struct mutex pd_lock;
	struct votable	*fcc_votable;
	struct votable	*fv_votable;
	struct votable	*icl_votable;
	struct votable	*iterm_votable;
	struct delayed_work charge_monitor_work;
	struct delayed_work usb_otg_monitor_work;
	struct delayed_work typec_burn_monitor_work;
	struct regulator *vbus_contral;
	struct step_jeita_cfg0 step_chg_cfg[STEP_JEITA_TUPLE_COUNT];
	struct step_jeita_cfg0 jeita_fv_cfg[STEP_JEITA_TUPLE_COUNT];
	struct step_jeita_cfg1 jeita_fcc_cfg[STEP_JEITA_TUPLE_COUNT];
	int step_fallback_hyst;
	int step_forward_hyst;
	int jeita_fallback_hyst;
	int jeita_forward_hyst;
	int sw_cv;
	int sw_cv_count;
	int step_chg_index[2];
	int jeita_chg_index[2];
	int step_chg_fcc;
	int jeita_chg_fcc;
	int current_now;
	int vbat_now;
	int temp_now;
	int soc;
	int entry_soc;
	int flag;
	int cycle_count;
	int battery_cycle;
	int switch_pd_wa;

	int thermal_level;
	int thermal_limit[THERMAL_LIMIT_TUPLE][THERMAL_LIMIT_COUNT];
	int thermal_current;
	int pd_type;
	bool pd_reset;
	bool pd_soft_reset;

	u32 bootmode;
	u32 boottype;

	int chr_type;
	int usb_type;
	int usb_state;
	int real_type;
	int qc3_type;
	bool ffc_enable;
	bool typec_burn;
	bool typec_burn_status;
	bool typec_otg_burn;
	bool typec_otg_burn_status;
	bool input_suspend;
	bool pd_verifying;
	bool fg_full;
	bool charge_full;
	bool real_full;
	bool charge_eoc;
	bool bbc_charge_done;
	bool bbc_charge_enable;
	bool recharge;
	bool otg_enable;
	bool pd_verify_done;
	bool pd_verifed;
        bool warm_term;
	bool last_ffc_enable;
	int apdo_max;
	int cc_orientation;
	int typec_mode;
	int fake_typec_temp;
	int fv;
	int fv_normal;
	int fv_ffc;
#ifdef CONFIG_TARGET_PRODUCT_PEARL
	int fv_ffc_large_cycle;
#endif
	int iterm;
	int iterm_warm;
	int iterm_ffc;
	int iterm_ffc_little_warm;
	int iterm_ffc_warm;
	int iterm_2nd;
	int iterm_warm_2nd;
	int iterm_ffc_2nd;
	int iterm_ffc_warm_2nd;
	int ffc_low_tbat;
	int ffc_medium_tbat;
	int ffc_little_high_tbat;
	int ffc_high_tbat;
	int ffc_high_soc;
	int sic_current;
	bool en_floatgnd;

	struct mutex cable_out_lock;
	int cable_out_cnt;

	/* system lock */
	spinlock_t slock;
	struct wakeup_source *charger_wakelock;
	struct mutex charger_lock;

	/* thread related */
	wait_queue_head_t  wait_que;
	bool charger_thread_timeout;
	unsigned int polling_interval;
	bool charger_thread_polling;

	/* alarm timer */
	struct alarm charger_timer;
	struct timespec64 endtime;
	bool is_suspend;
	struct notifier_block pm_notifier;
	ktime_t timer_cb_duration[8];

	/* notify charger user */
	struct srcu_notifier_head evt_nh;

	/* common info */
	int log_level;
	bool usb_unlimited;
	bool charger_unlimited;
	bool disable_charger;
	bool disable_aicl;
	int battery_temp;
	bool can_charging;
	bool cmd_discharging;
	bool safety_timeout;
	int safety_timer_cmd;
	bool vbusov_stat;
	bool dpdmov_stat;
	bool lst_dpdmov_stat;
	bool is_chg_done;
	/* ATM */
	bool atm_enabled;

	const char *algorithm_name;
	struct mtk_charger_algorithm algo;

	/* dtsi custom data */
	struct charger_custom_data data;

	/* battery warning */
	unsigned int notify_code;
	unsigned int notify_test_mode;

	/* sw safety timer */
	bool enable_sw_safety_timer;
	bool sw_safety_timer_setting;
	struct timespec64 charging_begin_time;

	/* vbat monitor, 6pin bat */
	bool batpro_done;
	bool enable_vbat_mon;
	bool enable_vbat_mon_bak;
	int old_cv;
	bool stop_6pin_re_en;
	int vbat0_flag;

	/* sw jeita */
	bool enable_sw_jeita;
	struct sw_jeita_data sw_jeita;

	/* battery thermal protection */
	struct battery_thermal_protection_data thermal;

	struct chg_alg_device *alg[MAX_ALG_NO];
	int lst_rnd_alg_idx;
	bool alg_new_arbitration;
	bool alg_unchangeable;
	struct notifier_block chg_alg_nb;
	bool enable_hv_charging;

	/* main connector detection */
	struct iio_channel *mt6373_adc3;
	bool mt6373_adc3_enable;

	/* water detection */
	bool water_detected;
	bool record_water_detected;

	bool enable_dynamic_mivr;

	/* fast charging algo support indicator */
	bool enable_fast_charging_indicator;
	unsigned int fast_charging_indicator;

	/* diasable meta current limit for testing */
	unsigned int enable_meta_current_limit;

	struct smartcharging sc;

	/*daemon related*/
	struct sock *daemo_nl_sk;
	u_int g_scd_pid;
	struct scd_cmd_param_t_1 sc_data;

	/*charger IC charging status*/
	bool is_charging;
	bool jeita_support;
	ktime_t uevent_time_check;
	struct regmap *mt6368_regmap;
	bool mt6368_moscon1_control;
	bool force_disable_pp[CHG2_SETTING + 1];
	bool enable_pp[CHG2_SETTING + 1];
	struct mutex pp_lock[CHG2_SETTING + 1];
	int cmd_pp;

	/* enable boot volt*/
	bool enable_boot_volt;
	bool reset_boot_volt_times;
	bool night_charging;
	bool night_charge_enable;
	bool sic_support;
	bool suspend_recovery;
	bool thermal_remove;
	bool plugged_status;
	int diff_fv_val;
	int set_smart_batt_diff_fv;
	int ov_check_only_once;
	int max_fcc;
	int product_name;
	int bms_i2c_error_count;
	int gauge_authentic;
	int batt_cell_supplier;
	int battcont_online_adc;
	int pmic_comp_v;
	int burn_control_gpio;
	int project_no;
	/*SOA and ISP jieta fcc * 0.8*/
	bool div_jeita_fcc_flag;
	atomic_t ieoc_wkrd;
	/* smart_chg, point to smart_chg array of struct mtk_battery  */
	struct smart_chg *smart_chg;
	struct notifier_block charger_notifier;
	int screen_status;

#if defined(CONFIG_RUST_DETECTION)
	struct charger_device *et7480_chg_dev;
	struct tcpc_device *tcpc;
	struct notifier_block tcpc_rust_det_nb;
	struct delayed_work rust_detection_work;
	struct delayed_work hrtime_otg_work;
	struct alarm rust_det_work_timer;
	struct delayed_work set_cc_drp_work;
	struct alarm lpd_plug_det_work_timer;
	bool typec_attach;
	bool ui_cc_toggle;
	bool cid_status;
	int lpd_res[5];
	int lpd_debounce_times;
	bool lpd_flag;
	bool lpd_trigger_status;
	bool lpd_update_en;
	bool first_lpd_det;
	int uart_control_gpio;
#endif
};

enum power_supply_typec_mode {
	POWER_SUPPLY_TYPEC_NONE,

	/* Acting as source */
	POWER_SUPPLY_TYPEC_SINK,			/* Rd only */
	POWER_SUPPLY_TYPEC_SINK_POWERED_CABLE,		/* Rd/Ra */
	POWER_SUPPLY_TYPEC_SINK_DEBUG_ACCESSORY,	/* Rd/Rd */
	POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER,		/* Ra/Ra */
	POWER_SUPPLY_TYPEC_POWERED_CABLE_ONLY,		/* Ra only */

	/* Acting as sink */
	POWER_SUPPLY_TYPEC_SOURCE_DEFAULT,		/* Rp default */
	POWER_SUPPLY_TYPEC_SOURCE_MEDIUM,		/* Rp 1.5A */
	POWER_SUPPLY_TYPEC_SOURCE_HIGH,			/* Rp 3A */
	POWER_SUPPLY_TYPEC_NON_COMPLIANT,
};

#define USB_SYSFS_FIELD_RW(_name, _prop)	\
{									 \
	.attr	= __ATTR(_name, 0644, usb_sysfs_show, usb_sysfs_store),\
	.prop	= _prop,	\
	.set	= _name##_set,						\
	.get	= _name##_get,						\
}
#define USB_SYSFS_FIELD_RO(_name, _prop)	\
{			\
	.attr   = __ATTR(_name, 0444, usb_sysfs_show, usb_sysfs_store),\
	.prop   = _prop,				  \
	.get	= _name##_get,						\
}
#define USB_SYSFS_FIELD_WO(_name, _prop)	\
{								   \
	.attr	= __ATTR(_name, 0200, usb_sysfs_show, usb_sysfs_store),\
	.prop	= _prop,	\
	.set	= _name##_set,						\
}
enum usb_property {
	USB_PROP_REAL_TYPE,
	USB_PROP_QUICK_CHARGE_TYPE,
	USB_PROP_PD_AUTHENTICATION,
	USB_PROP_PD_VERIFYING,
	USB_PROP_PD_TYPE,
	USB_PROP_APDO_MAX,
	USB_PROP_TYPEC_MODE,
	USB_PROP_TYPEC_CC_ORIENTATION,
	USB_PROP_FFC_ENABLE,
	USB_PROP_CHARGE_FULL,
	USB_PROP_CONNECTOR_TEMP,
	USB_PROP_TYPEC_BURN,
	USB_PROP_SW_CV,
	USB_PROP_INPUT_SUSPEND,
	USB_PROP_JEITA_CHG_INDEX,
	USB_PROP_POWER_MAX,
	USB_PROP_QC3_TYPE,
	USB_PROP_OTG_ENABLE,
	USB_PROP_PD_VERIFY_DONE,
	USB_PROP_CP_IBUS_DELTA,
	USB_PROP_MTBF_TEST,
	USB_PROP_CP_CHARGE_RECOVERY,
	USB_PROP_PMIC_IBAT,
	USB_PROP_PMIC_VBUS,
	USB_PROP_INPUT_CURRENT_NOW,
	USB_PROP_BATTCONT_ONLINE,
	USB_PROP_THERMAL_REMOVE,
	USB_PROP_WARM_TERM,
	USB_PROP_ADAPTER_ID,
};

struct mtk_usb_sysfs_field_info {
	struct device_attribute attr;
	enum usb_property prop;
	int (*set)(struct mtk_charger *gm,
		struct mtk_usb_sysfs_field_info *attr, int val);
	int (*get)(struct mtk_charger *gm,
		struct mtk_usb_sysfs_field_info *attr, int *val);
};

#define CP_SYSFS_FIELD_RO(_name, _prop)	\
{			\
	.attr   = __ATTR(_name, 0444, cp_sysfs_show, cp_sysfs_store),\
	.prop   = _prop,				  \
	.get	= _name##_get,						\
}

enum cp_property {
	CP_PROP_VBUS,
	CP_PROP_IBUS,
	CP_PROP_TDIE,
	CP_PROP_CHIP_OK,
};

static inline int mtk_chg_alg_notify_call(struct mtk_charger *info,
					  enum chg_alg_notifier_events evt,
					  int value)
{
	int i;
	struct chg_alg_notify notify = {
		.evt = evt,
		.value = value,
	};

	for (i = 0; i < MAX_ALG_NO; i++) {
		if (info->alg[i])
			chg_alg_notifier_call(info->alg[i], &notify);
	}
	return 0;
}

/* functions which framework needs*/
extern int mtk_basic_charger_init(struct mtk_charger *info);
extern int mtk_pulse_charger_init(struct mtk_charger *info);
extern int get_uisoc(struct mtk_charger *info);
extern int get_battery_voltage(struct mtk_charger *info);
extern int get_battery_temperature(struct mtk_charger *info);
extern int get_battery_current(struct mtk_charger *info);
extern int get_vbus(struct mtk_charger *info);
extern int get_ibat(struct mtk_charger *info);
extern int get_ibus(struct mtk_charger *info);
extern bool is_battery_exist(struct mtk_charger *info);
extern int get_charger_type(struct mtk_charger *info);
extern int get_usb_type(struct mtk_charger *info);
extern int disable_hw_ovp(struct mtk_charger *info, int en);
extern bool is_charger_exist(struct mtk_charger *info);
extern int get_charger_temperature(struct mtk_charger *info,
	struct charger_device *chg);
extern int get_charger_charging_current(struct mtk_charger *info,
	struct charger_device *chg);
extern int get_charger_input_current(struct mtk_charger *info,
	struct charger_device *chg);
extern int get_charger_zcv(struct mtk_charger *info,
	struct charger_device *chg);
extern void _wake_up_charger(struct mtk_charger *info);
extern int charger_manager_get_sic_current(void);
extern void charger_manager_set_sic_current(int sic_current);
extern void night_charging_set_flag(bool night_charging);
extern int night_charging_get_flag(void);
extern void set_soft_reset_status(int val);
extern int get_soft_reset_status(void);

extern int input_suspend_set_flag(int val);
extern int input_suspend_get_flag(void);
extern void update_quick_chg_type(struct mtk_charger *info);
extern void smart_batt_set_diff_fv(int val);
extern void update_connect_temp(struct mtk_charger *info);


/* functions for other */
extern int mtk_chg_enable_vbus_ovp(bool enable);
extern int usb_get_property(enum usb_property bp, int *val);
extern int usb_set_property(enum usb_property bp, int val);

#if defined(CONFIG_RUST_DETECTION)
extern void manual_set_cc_toggle(bool en);
extern void manual_get_cc_toggle(bool *cc_toggle);
extern bool manual_get_cid_status(void);
extern int lpd_dp_res_get_from_charger(int i);
extern void lpd_update_en_set_to_charger(int en);
extern void liquid_detectin_enable_man(int en);
extern void disable_uart_manual(int en);
#endif

enum attach_type {
	ATTACH_TYPE_NONE,
	ATTACH_TYPE_PWR_RDY,
	ATTACH_TYPE_TYPEC,
	ATTACH_TYPE_PD,
	ATTACH_TYPE_PD_SDP,
	ATTACH_TYPE_PD_DCP,
	ATTACH_TYPE_PD_NONSTD,
	ATTACH_TYPE_MAX,
};

#define ONLINE(idx, attach)		((idx & 0xf) << 4 | (attach & 0xf))
#define ONLINE_GET_IDX(online)		((online >> 4) & 0xf)
#define ONLINE_GET_ATTACH(online)	(online & 0xf)

#endif /* __MTK_CHARGER_H */
