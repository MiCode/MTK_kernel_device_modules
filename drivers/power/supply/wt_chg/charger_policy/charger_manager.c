// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/power_supply.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <generated/autoconf.h>
#include <linux/reboot.h>	

#include "../charger_class/charger_class.h"
#include "dvchg_policy.h"
#include "charger_manager.h"
#include "../../../../misc/mediatek/typec/sc_tcpc/inc/tcpm.h"

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
SRCU_NOTIFIER_HEAD(main_chg_notifier);
EXPORT_SYMBOL_GPL(main_chg_notifier);
int main_chg_reg_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&main_chg_notifier, nb);
}
EXPORT_SYMBOL_GPL(main_chg_reg_notifier);

int main_chg_unreg_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&main_chg_notifier, nb);
}
EXPORT_SYMBOL_GPL(main_chg_unreg_notifier);

int main_chg_notifier_call_chain(unsigned long event, int val)
{
	return srcu_notifier_call_chain(&main_chg_notifier, event, &val);
}
EXPORT_SYMBOL_GPL(main_chg_notifier_call_chain);

int plugin_val;
#endif

enum sc6601_vbus_type {
	SC6601_VBUS_NONE,
	SC6601_VBUS_USB_SDP,
	SC6601_VBUS_USB_CDP, 
	SC6601_VBUS_USB_DCP,
	SC6601_VBUS_HVDCP,
	SC6601_VBUS_UNKNOWN,
	SC6601_VBUS_NONSTAND,
	SC6601_VBUS_OTG,
	SC6601_VBUS_TYPE_NUM,
};

static const char *bc12_result[] = {
	"None",
	"SDP",
	"CDP",
	"DCP",
	"QC",
	"Unknown",
	"Non-Stand",
	"QC3",
	"QC3+"
};

static const char *real_type_txt[] = {
	"None",
	"USB",
	"USB_CDP",
	"USB_DCP",
	"USB_HVDCP",
	"USB_FLOAT",//Unknown
	"USB_FLOAT",//Non-Stand,
	"USB_HVDCP_3",
	"USB_HVDCP_3P5",
	"USB_PD",
};

enum quick_charge_type {
	QUICK_CHARGE_NORMAL = 0,		/* USB、DCP、CDP、Float */
	QUICK_CHARGE_FAST,			/* PD、QC2、QC3 */
	QUICK_CHARGE_FLASH,
	QUICK_CHARGE_TURBE,			/* verified PD、QC3.5、QC3-27W */
};
struct quick_charge {
	enum vbus_type adap_type;
	enum quick_charge_type adap_cap;
};
static struct quick_charge quick_charge_map[6] = {
	{ VBUS_TYPE_SDP, QUICK_CHARGE_NORMAL },
	{ VBUS_TYPE_DCP, QUICK_CHARGE_NORMAL },
	{ VBUS_TYPE_CDP, QUICK_CHARGE_NORMAL },
	{ VBUS_TYPE_NON_STAND, QUICK_CHARGE_NORMAL },
	{ VBUS_TYPE_HVDCP, QUICK_CHARGE_FAST },
	{ 0, 0 },
};

#define POWER_SUPPLY_MANUFACTURER       "SouthChip"
#define POWER_SUPPLY_MODEL_NAME         "SC6601 Driver"

#define CHARGER_MANAGER_LOOP_TIME       5000          // 5s

/* cycle count */
#define ENABLE_SW_FCC 1
#define FFC_CV_1    4448
#define FFC_CV_2    4440
#define FFC_CV_3    4432
#define FFC_CV_4    4416
#define CHG_CYCLE_COUNT_LEVEL1  100
#define CHG_CYCLE_COUNT_LEVEL2  200
#define CHG_CYCLE_COUNT_LEVEL3  300

#define MAX_UEVENT_LENGTH                  50
#define SHUTDOWN_DELAY_VOL_HIGH            3400000
#define WAIT_USB_RDY_TIME               100
#define WAIT_USB_RDY_MAX_CNT            300
#define MIAN_CHG_ADC_LENGTH                180

static struct charger_manager {
	struct device *dev;
	wait_queue_head_t wait_queue;
	struct task_struct *thread;
	bool run_thread;

	struct timer_list charger_timer;
#ifdef CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE
	struct charger_dev *charger;
	struct notifier_block charger_nb;
#endif /* CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE */

	struct power_supply *usb_psy;
	struct power_supply *batt_psy;
	struct power_supply *ac_psy;
  	int uvent_quick_charge_type;
	int pd_active;
	int system_temp_level;
	int system_temp_level_max;
	int thermal_mitigation_current;
	int typec_mode;
	int soc;
	int vbat;
  	int chg_status;
	int ibus_ma;
	bool shutdown_delay;
	bool last_shutdown_delay;
	bool suspend_current;
	struct delayed_work power_off_check_work;
	struct delayed_work second_detect_work;
	struct delayed_work charger_set_input_work;
	int jeita_cc;
	int jeita_cv;
	struct delayed_work wait_usb_ready_work;
	int get_usb_rdy_cnt;
	struct device_node *usb_node;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	/*charger_enable_event 0:none 1:enable 2:disable*/
    int charger_enable_event;
    struct notifier_block   main_chg_nb;
#endif
	struct tcpc_device *tcpc;
    struct notifier_block pd_nb;
    int pd_curr_max;
	bool is_pr_swap;
	struct wakeup_source *chg_wakelock;
} *g_manager;

int thermal_mitigation[] =
{
	3600000,3500000,3400000,3300000,
	3200000,3100000,3000000,2900000,
	2800000,2700000,2600000,2500000,
	2400000,2300000,2200000,2100000,
	2000000,1900000,1800000,1700000,
	1600000,1500000,1400000,1300000,
	1200000,1100000,1000000,900000,
	800000,700000,600000,500000,
	400000,300000,200000,100000,
};

int charger_manager_set_pd_active(int pd_active)
{
	g_manager->pd_active = pd_active;
	pr_err("%s pd_active=%d\n", __func__, pd_active);
	return 0;
}
EXPORT_SYMBOL(charger_manager_set_pd_active);

int charger_manager_get_pd_active(void)
{
	return g_manager->pd_active;
}
EXPORT_SYMBOL(charger_manager_get_pd_active);

extern int charger_get_typec_cc_orientation(void);
static int charger_usb_get_property(struct power_supply *psy,
						 enum power_supply_property psp,
						 union power_supply_propval *val)
{
	struct charger_manager *manager = power_supply_get_drvdata(psy);
	bool online = false;
	enum vbus_type vbus_type;
	int ret = 0;
	int real_type = 0;
	int volt = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = charger_get_chg_status1(manager->charger);
		break;
	case POWER_SUPPLY_PROP_TYPE:
		ret = charger_get_vbus_type(manager->charger, &vbus_type);
		if (vbus_type == SC6601_VBUS_USB_SDP)
			real_type = POWER_SUPPLY_TYPE_USB;
		else if (vbus_type == SC6601_VBUS_USB_CDP)
			real_type = POWER_SUPPLY_TYPE_USB_CDP;
		else if (vbus_type == SC6601_VBUS_USB_DCP)
			if(charger_manager_get_pd_active()){
				real_type = POWER_SUPPLY_TYPE_USB_PD;
				dev_err(manager->dev, "PD active true ");
			}				
			else
				real_type = POWER_SUPPLY_TYPE_USB_DCP;
		else
			real_type = POWER_SUPPLY_TYPE_UNKNOWN;
		if (ret < 0)
			val->intval = 0;
		dev_err(manager->dev, "real_type = %d\n",real_type);
		val->intval = real_type;
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		ret = charger_get_online(manager->charger, &online);
		if (ret < 0)
			val->intval = 0;
		val->intval = online;
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = 0;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = charger_get_adc(manager->charger, SC_ADC_VBUS, &volt);
		if (ret < 0) { 
			dev_err(manager->dev, "Couldn't read input volt ret=%d\n", ret);
			return ret;
		}
        dev_err(manager->dev, "volt=%d\n", volt);
		val->intval = volt;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = 0;
		break;

	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		val->intval = 0;
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = 0;
		break;

	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = POWER_SUPPLY_MANUFACTURER;
		break;

	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = POWER_SUPPLY_MODEL_NAME;
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static int charger_usb_set_property(struct power_supply *psy,
					enum power_supply_property prop,
					const union power_supply_propval *val)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int charger_ac_get_property(struct power_supply *psy,
						 enum power_supply_property psp,
						 union power_supply_propval *val)
{
	struct charger_manager *manager = power_supply_get_drvdata(psy);
	bool online = false;
	enum vbus_type vbus_type;
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = POWER_SUPPLY_TYPE_MAINS;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		charger_get_vbus_type(manager->charger, &vbus_type);
		ret = charger_get_online(manager->charger, &online);
		if (ret < 0)
			val->intval = 0;
		if (vbus_type == SC6601_VBUS_USB_CDP || vbus_type == SC6601_VBUS_USB_DCP
                   || vbus_type == SC6601_VBUS_HVDCP || vbus_type == SC6601_VBUS_NONSTAND
                   || manager->pd_active)
			val->intval = online;
		else
			val->intval = 0; 
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = POWER_SUPPLY_MANUFACTURER;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = POWER_SUPPLY_MODEL_NAME;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int charger_ac_set_property(struct power_supply *psy,
					enum power_supply_property prop,
					const union power_supply_propval *val)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_TYPE:
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property usb_props[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static char * sc_charger_desc_supplied_to[] = {
	"battery",
  
};

static const struct power_supply_desc usb_psy_desc = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB,
	.properties = usb_props,
	.num_properties = ARRAY_SIZE(usb_props),
	.get_property = charger_usb_get_property,
	.set_property = charger_usb_set_property,
};

int charger_manager_get_prop_system_temp_level(void)
{
	if (g_manager == NULL)
		return false;
	return g_manager->system_temp_level;
}
EXPORT_SYMBOL(charger_manager_get_prop_system_temp_level);

int charger_manager_get_prop_system_temp_level_max(void)
{
	if (g_manager == NULL)
		return false;
	return g_manager->system_temp_level_max;
}
EXPORT_SYMBOL(charger_manager_get_prop_system_temp_level_max);

void charger_manager_set_prop_system_temp_level(int temp_level)
{
	if (temp_level >= g_manager->system_temp_level_max)
		g_manager->system_temp_level = g_manager->system_temp_level_max - 1;
	else
		g_manager->system_temp_level = temp_level;
	pr_err("%s,thermal_current = %d,level = %d\n", __func__, thermal_mitigation[g_manager->system_temp_level], temp_level);
	g_manager->thermal_mitigation_current = thermal_mitigation[g_manager->system_temp_level];
}
EXPORT_SYMBOL(charger_manager_set_prop_system_temp_level);

#define JEITA_TEMP_T0 0
#define JEITA_TEMP_T10 100
#define JEITA_TEMP_T15 150
#define JEITA_TEMP_T20 200
#define JEITA_TEMP_T35 350
#define JEITA_TEMP_T45 450
#define JEITA_TEMP_T58 580
#define JEITA_TEMP_BELOW_T0_CC        0
/* 0 ~ 10 start charger*/
#define JEITA_TEMP_T0_TO_T10_CC       650
/* 10 ~ 15 start charger*/
#define JEITA_TEMP_T10_TO_T15_CC      1950
/* 15 ~ 20 start charger*/
#define JEITA_TEMP_T15_TO_T20_CC      3250
/* 20 ~ 35 start charger*/
#define JEITA_TEMP_T20_TO_T35_CC      3600
/* 35 ~ 45 start charger*/
#define JEITA_TEMP_T35_TO_T45_CC      3600
/* 45 ~ 60 start charger*/
#define JEITA_TEMP_T45_TO_T58_CC      3250
/* above 60 stop charger*/
#define JEITA_TEMP_ABOVE_T58_CC      0
#define JEITA_TEMP_BELOW_T45_CV      4448
#define JEITA_TEMP_ABOVE_T45_CV      4096
static int sc_jeita_current_set(struct charger_manager *manager)
{
	union power_supply_propval pval = {0, };
	int ret = 0;
	g_manager->batt_psy = power_supply_get_by_name("battery");
	if (IS_ERR_OR_NULL(g_manager->batt_psy)) {
		pr_err("%s:manager->batt_psy is_err_or_null\n", __func__);
		return PTR_ERR(g_manager->batt_psy);
	}

	ret = power_supply_get_property(g_manager->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		dev_err(g_manager->dev, "Couldn't get bat temp ret=%d\n", ret);
		return -EINVAL;
	}
	
	pr_err("%s temp = %d\n",__func__,pval.intval);
	if (pval.intval >= JEITA_TEMP_T0 && pval.intval < JEITA_TEMP_T10) {
		g_manager->jeita_cc = JEITA_TEMP_T0_TO_T10_CC;
		g_manager->jeita_cv = JEITA_TEMP_BELOW_T45_CV;
	} else if (pval.intval >= JEITA_TEMP_T10 && pval.intval < JEITA_TEMP_T15) {
		g_manager->jeita_cc = JEITA_TEMP_T10_TO_T15_CC;
		g_manager->jeita_cv = JEITA_TEMP_BELOW_T45_CV;
	} else if (pval.intval >= JEITA_TEMP_T15 && pval.intval < JEITA_TEMP_T20) {
		g_manager->jeita_cc = JEITA_TEMP_T15_TO_T20_CC;
		g_manager->jeita_cv = JEITA_TEMP_BELOW_T45_CV;
	} else if (pval.intval >= JEITA_TEMP_T20 && pval.intval < JEITA_TEMP_T35) {
		g_manager->jeita_cc = JEITA_TEMP_T20_TO_T35_CC;
		g_manager->jeita_cv = JEITA_TEMP_BELOW_T45_CV;
	} else if (pval.intval >= JEITA_TEMP_T35 && pval.intval < JEITA_TEMP_T45) {
		g_manager->jeita_cc = JEITA_TEMP_T35_TO_T45_CC;
		if (g_manager->jeita_cv == JEITA_TEMP_ABOVE_T45_CV) {
			if (pval.intval <= JEITA_TEMP_T45 - 20)
				g_manager->jeita_cv = JEITA_TEMP_BELOW_T45_CV;
		} else {
			g_manager->jeita_cv = JEITA_TEMP_BELOW_T45_CV;
		}
	} else if (pval.intval >= JEITA_TEMP_T45 && pval.intval < JEITA_TEMP_T58) {
		g_manager->jeita_cc = JEITA_TEMP_T45_TO_T58_CC;
		g_manager->jeita_cv = JEITA_TEMP_ABOVE_T45_CV;
	} else {
		g_manager->jeita_cc = JEITA_TEMP_BELOW_T0_CC;
	}
	pr_err("%s jeita_cc = %d mA,jeita_cv = %d mV\n",__func__,g_manager->jeita_cc,g_manager->jeita_cv);
	return 0;
}

bool get_jeita_high_temp_state(void)
{
	if (g_manager->jeita_cv == JEITA_TEMP_ABOVE_T45_CV)
		return TRUE;
	return FALSE;
}
EXPORT_SYMBOL(get_jeita_high_temp_state);

static int sc_select_charging_current(struct charger_manager *manager)
{
	int set_ibat = 0,ret = 0;
	enum vbus_type vbus_type = VBUS_TYPE_NONE;
	struct charger_dev *charger = manager->charger;

	ret = charger_get_vbus_type(manager->charger, &vbus_type);
	if (vbus_type == SC6601_VBUS_USB_SDP)
		set_ibat = 500;
	else if (vbus_type == SC6601_VBUS_USB_CDP)
		set_ibat = 1500;
	else if (vbus_type == SC6601_VBUS_USB_DCP)
		set_ibat = 2200;
	else if (vbus_type == SC6601_VBUS_HVDCP)
		set_ibat = 3600;
	else
		set_ibat = 1000;
	if (manager->pd_active)
		set_ibat = 3600;

	set_ibat = min(set_ibat,g_manager->thermal_mitigation_current/1000);
	set_ibat = min(set_ibat,g_manager->jeita_cc);
	pr_err("%s set_ibat = %d mA , ret =%d\n",__func__,set_ibat, ret);
	charger_set_ichg(charger,set_ibat);
	return 0;
}

static enum power_supply_property ac_props[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
};

static const struct power_supply_desc ac_psy_desc = {
	.name = "ac",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.properties = ac_props,
	.num_properties = ARRAY_SIZE(ac_props),
	.get_property = charger_ac_get_property,
	.set_property = charger_ac_set_property,
};

static int charger_manager_ac_psy_register(struct charger_manager *manager)
{
	struct power_supply_config ac_psy_cfg = { .drv_data = manager,};
	ac_psy_cfg.supplied_to = sc_charger_desc_supplied_to;
	ac_psy_cfg.num_supplicants = ARRAY_SIZE(sc_charger_desc_supplied_to);
	manager->ac_psy = devm_power_supply_register(manager->dev, &ac_psy_desc,
							&ac_psy_cfg);
	if (IS_ERR(manager->ac_psy)) {
		dev_err(manager->dev, "ac psy register failed\n");
		return PTR_ERR(manager->ac_psy);
	};
	return 0;
}

void charger_manager_set_shipmode(bool en)
{
	struct charger_dev *charger = g_manager->charger;
	charger_set_shipmode(charger,en);
	return;
}
EXPORT_SYMBOL(charger_manager_set_shipmode);

static int charger_manager_usb_psy_register(struct charger_manager *manager)
{
	struct power_supply_config usb_psy_cfg = { .drv_data = manager,};
	usb_psy_cfg.supplied_to = sc_charger_desc_supplied_to;
	usb_psy_cfg.num_supplicants = ARRAY_SIZE(sc_charger_desc_supplied_to);
	manager->usb_psy = devm_power_supply_register(manager->dev, &usb_psy_desc,
							&usb_psy_cfg);
	if (IS_ERR(manager->usb_psy)) {
		dev_err(manager->dev, "usb psy register failed\n");
		return PTR_ERR(manager->usb_psy);
	};

	return 0;
}

static void kpoc_power_off_check(struct charger_manager *manager)
{
	return;
}

static void charger_set_input_workfunc(struct work_struct *work){
	struct charger_manager *manager = container_of(work,
					struct charger_manager, charger_set_input_work.work);
	dev_err(manager->dev, "usb_suspend_control_ibus\n");
	charger_set_input_curr_lmt(manager->charger, manager->ibus_ma);
}

static void power_off_check_work(struct work_struct *work)
{
  	int ret = 0;
	struct charger_manager *manager = container_of(work,
					struct charger_manager, power_off_check_work.work);
  	union power_supply_propval pval = {0, };
	static char uevent_string[][MAX_UEVENT_LENGTH + 1] = {
		"POWER_SUPPLY_SHUTDOWN_DELAY=\n", //28
	};

	static char *envp[] = {
		uevent_string[0],
		NULL,
	};

	manager->batt_psy = power_supply_get_by_name("battery");
	if (IS_ERR_OR_NULL(manager->batt_psy)) {
		pr_err("%s:manager->batt_psy is_err_or_null\n", __func__);
		goto psy_out;
	}

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0)
		dev_err(manager->dev, "Couldn't get bat soc ret=%d\n", ret);
	else
  		manager->soc = pval.intval;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret < 0)
		dev_err(manager->dev, "Couldn't get bat temp ret=%d\n", ret);
	else
		manager->vbat = pval.intval;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
	if (ret < 0)
		dev_err(manager->dev, "Couldn't get bat status ret=%d\n", ret);
	else
		manager->chg_status = pval.intval;
	pr_err("%s:soc=%d,vbat=%d,chg_status=%d\n", __func__,manager->soc,manager->vbat,manager->chg_status);
	if (manager->soc == 1) {
		if (manager->vbat < SHUTDOWN_DELAY_VOL_HIGH && manager->chg_status != POWER_SUPPLY_STATUS_CHARGING){
				manager->shutdown_delay = true;
				pr_err("%s:shutdown_dealy= true\n", __func__);
		} else if (manager->chg_status == POWER_SUPPLY_STATUS_CHARGING
						&& manager->shutdown_delay) {
				manager->shutdown_delay = false;
				pr_err("%s:shutdown_dealy= false because of charging \n", __func__);
		} else {
			manager->shutdown_delay = false;
		}
		if (manager->chg_status == POWER_SUPPLY_STATUS_CHARGING 
                    				&& (manager->vbat < 3350000)) {
				    pr_err("%s plug_in shutdown\n", __func__);
				    kernel_power_off();
		}
	} else {
		manager->shutdown_delay = false;
	}

	if (manager->last_shutdown_delay != manager->shutdown_delay) {
		manager->last_shutdown_delay = manager->shutdown_delay;
		power_supply_changed(manager->usb_psy);
		power_supply_changed(manager->batt_psy);
		if (manager->shutdown_delay == true)
			strncpy(uevent_string[0] + 28, "1", MAX_UEVENT_LENGTH - 28);
		else
			strncpy(uevent_string[0] + 28, "0", MAX_UEVENT_LENGTH - 28);
		mdelay(1000);
		pr_err("envp[0] = %s\n", envp[0]);
		kobject_uevent_env(&manager->dev->kobj, KOBJ_CHANGE, envp);
	}

psy_out:
	schedule_delayed_work(&manager->power_off_check_work, msecs_to_jiffies(5000));
}

static int charger_manager_check_vindpm(struct charger_manager *manager,
											uint32_t vbat)
{
#ifdef CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE
	struct charger_dev *charger = manager->charger;
	int ret = 0;

	if (vbat < 3800) {
		ret = charger_set_input_volt_lmt(charger, 4000);
	} else if (vbat < 4200) {
		ret = charger_set_input_volt_lmt(charger, 4400);
	} else {
		ret = charger_set_input_volt_lmt(charger, 4500);
	}

	if (ret < 0)
		return ret;
#endif /* CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE */
	return 0;
}

static int quick_charge_type_uevent(void);
static int chg_get_quick_charger_type(void);
static ssize_t real_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *usb_psy;
	struct charger_manager *manager;
	enum vbus_type vbus_type = VBUS_TYPE_NONE;
	int ret;
	if (IS_ERR_OR_NULL(dev)) {
		pr_err("%s:dev is_err_or_null\n", __func__);
		goto err;
	}
	usb_psy = dev_get_drvdata(dev);
	if (IS_ERR_OR_NULL(usb_psy)) {
		pr_err("%s:dev is_err_or_null\n", __func__);
		goto err;
	}
	manager = power_supply_get_drvdata(usb_psy);
	if (IS_ERR_OR_NULL(manager)) {
		pr_err("%s:manager is_err_or_null\n", __func__);
		goto err;
	}
	if (IS_ERR_OR_NULL(manager->charger)) {
		pr_err("%s:manager->charger is_err_or_null\n", __func__);
		goto out;
	}
	ret = charger_get_vbus_type(manager->charger, &vbus_type);
	if (ret < 0){
		dev_err(manager->dev, "Couldn't get usb type ret=%d\n", ret);
		goto out;
	}
out:
	if (manager->pd_active && vbus_type != VBUS_TYPE_NONE)
		vbus_type = VBUS_TYPE_PD;
	chg_get_quick_charger_type();
	quick_charge_type_uevent();
err:
	return sprintf(buf, "%s\n", real_type_txt[vbus_type]);
}

static struct device_attribute real_type_attr =
	__ATTR(real_type, 0644, real_type_show, NULL);

static ssize_t typec_cc_orientation_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", charger_get_typec_cc_orientation());
}

static struct device_attribute typec_cc_orientation_attr =
	__ATTR(typec_cc_orientation, 0644, typec_cc_orientation_show, NULL);

static int chg_get_quick_charger_type(void)
{
	enum vbus_type vbus_type = VBUS_TYPE_NONE;
	enum quick_charge_type quick_charge_type = QUICK_CHARGE_NORMAL;
	union power_supply_propval pval = {0, };
	int ret = 0;
	int i = 0;

	g_manager->batt_psy = power_supply_get_by_name("battery");
	if (IS_ERR_OR_NULL(g_manager->batt_psy)) {
		pr_err("%s:manager->batt_psy is_err_or_null\n", __func__);
		return PTR_ERR(g_manager->batt_psy);
	}

	ret = power_supply_get_property(g_manager->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		dev_err(g_manager->dev, "Couldn't get bat temp ret=%d\n", ret);
		return -EINVAL;
	}
	ret = charger_get_vbus_type(g_manager->charger, &vbus_type);
	if (ret < 0){
		dev_err(g_manager->dev, "Couldn't get usb type ret=%d\n", ret);
		return ret;
	}

	while (quick_charge_map[i].adap_type != 0) {
		if (vbus_type == quick_charge_map[i].adap_type) {
			quick_charge_type = quick_charge_map[i].adap_cap;
		}
		i++;
	}

	if(g_manager->pd_active)
		quick_charge_type = QUICK_CHARGE_FAST;
	
	if(pval.intval >= 450){
		quick_charge_type = QUICK_CHARGE_NORMAL;
	}
	g_manager->uvent_quick_charge_type = quick_charge_type;
	return quick_charge_type;
}

static ssize_t quick_charge_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	enum quick_charge_type quick_charge_type = QUICK_CHARGE_NORMAL;
	quick_charge_type = chg_get_quick_charger_type();
	return scnprintf(buf, PAGE_SIZE, "%d\n", quick_charge_type);
}

static int quick_charge_type_uevent(void)
{
	int ret;
	char quick_charge_string[64];
	char *envp[] = {quick_charge_string, NULL,};
	sprintf(quick_charge_string, "POWER_SUPPLY_QUICK_CHARGE_TYPE=%d", g_manager->uvent_quick_charge_type);
	ret = kobject_uevent_env(&g_manager->dev->kobj, KOBJ_CHANGE, envp);
	if (ret) {
		dev_err(g_manager->dev, "%s: failed to sent quick charge type event\n", __func__);
		return ret;
	}
	dev_err(g_manager->dev, "envp = %s\n", envp[0]);
	return ret;
}

static struct device_attribute quick_charge_type_attr =
	__ATTR(quick_charge_type, 0644, quick_charge_type_show, NULL);

static const char * const usb_typec_mode_text[] = {
	"Nothing attached", "Source attached", "Sink attached",
	"Audio Adapter", "Non compliant",
};

int chg_set_typec_mode(int mode)
{
	g_manager->typec_mode = mode;
	return 0;
}
EXPORT_SYMBOL(chg_set_typec_mode);

static ssize_t typec_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n",usb_typec_mode_text[g_manager->typec_mode]);
}
static struct device_attribute typec_mode_attr =
	__ATTR(typec_mode, 0644, typec_mode_show, NULL);

static struct attribute *usb_psy_attrs[] = {
	&real_type_attr.attr,
	&typec_cc_orientation_attr.attr,
	&quick_charge_type_attr.attr,
	&typec_mode_attr.attr,
	NULL,
};

static const struct attribute_group usb_psy_attrs_group = {
	.attrs = usb_psy_attrs,
};

static int usb_sysfs_create_group(struct charger_manager *manager)
{
	return sysfs_create_group(&manager->usb_psy->dev.kobj,
					&usb_psy_attrs_group);
}

static int charger_manager_check_iindpm(struct charger_manager *manager,
											uint32_t vbus_type)
{
#ifdef CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE
	struct charger_dev *charger = manager->charger;
	int ret = 0;
#endif /* CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE */
	int ma = 0;

	switch (vbus_type) {
	case VBUS_TYPE_NONE:
		ma = 100;
		break;
	case VBUS_TYPE_UNKNOWN:
		ma = 1000;
		if (manager->pd_active)
		{
			ma = 2000;
			printk("[%s] VBUS_TYPE_UNKNOWN pd active \n", __func__);
		}
		break;

	case VBUS_TYPE_SDP:
		ma = 500;
		if (manager->pd_active)
		{
			ma = 1500;
			printk("[%s] VBUS_TYPE_SDP pd active \n", __func__);
		}
		break;

	case VBUS_TYPE_NON_STAND:
		ma = 1000;
		break;
	case VBUS_TYPE_CDP:
		ma = 1500;
		break;
	case VBUS_TYPE_DCP:
	case VBUS_TYPE_HVDCP:
		ma = 2000;
		break;
	default:
		ma = 500;
		break;
	}
	if (manager->pd_active)
		ma = manager->pd_curr_max;
	if (manager->suspend_current)
		ma = 100;

#ifdef CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE
	ret = charger_set_input_curr_lmt(charger, ma);
	if (ret < 0)
		return ret;
#endif /* CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE */

	return 0;
}

static void apsd_second_detect_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
					struct charger_manager, second_detect_work.work);
	pr_err("apsd_second_detect_work enter!\n");
	charger_force_dpdm(manager->charger);
}

void usb_suspend_control_ibus(int ma)
{
	if (charger_is_sy6976(g_manager->charger)) {
		pr_err("%s set ibus:%d\n",__func__, ma);
		g_manager -> ibus_ma = ma;
		schedule_delayed_work(&g_manager->charger_set_input_work, msecs_to_jiffies(20));
        }
}
EXPORT_SYMBOL(usb_suspend_control_ibus);

static bool get_usb_ready(struct charger_manager *manager)
{
	bool ready = true;
	if (IS_ERR_OR_NULL(manager->usb_node))
		manager->usb_node = of_parse_phandle(manager->dev->of_node, "usb", 0);
	if (!IS_ERR_OR_NULL(manager->usb_node)) {
		ready = !of_property_read_bool(manager->usb_node, "cdp-block");
		if (ready || manager->get_usb_rdy_cnt % 10 == 0)
			pr_info("usb ready = %d\n", ready);
	} else
		pr_err("usb node missing or invalid\n");
	if (ready == false && (manager->get_usb_rdy_cnt >= WAIT_USB_RDY_MAX_CNT || manager->pd_active)) {
		if (manager->pd_active)
			manager->get_usb_rdy_cnt = 0;
		pr_info("cdp-block timeout or pd adapter\n");
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

static int charger_manager_wake_thread(struct charger_manager *manager)
{
	manager->run_thread = true;
	wake_up(&manager->wait_queue);
	return 0;
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

static u32 swchg_get_cycle_count_level(struct charger_manager *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	u32 ffc_constant_voltage = 0;
	int ret;
	psy = power_supply_get_by_name("battery");
	if (!psy) {
		pr_err("%s: failed to get battery psy\n", __func__);
		return PTR_ERR(psy);
	}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &val);
	if (ret) {
		pr_err("%s: failed to get prop: %d\n", __func__, POWER_SUPPLY_PROP_CYCLE_COUNT);
		return ret;
	}
	pr_err("%s: prop cycle count = %d\n", __func__, val.intval);
	if (val.intval >=0 && val.intval < CHG_CYCLE_COUNT_LEVEL1)
		ffc_constant_voltage = FFC_CV_1;
	else if (val.intval >= CHG_CYCLE_COUNT_LEVEL1 && val.intval < CHG_CYCLE_COUNT_LEVEL2)
		ffc_constant_voltage = FFC_CV_2;
	else if (val.intval >= CHG_CYCLE_COUNT_LEVEL2 && val.intval < CHG_CYCLE_COUNT_LEVEL3)
		ffc_constant_voltage = FFC_CV_3;
	else if (val.intval >= CHG_CYCLE_COUNT_LEVEL3)
		ffc_constant_voltage = FFC_CV_4;
	return ffc_constant_voltage;
}

static void swchg_select_cv(struct charger_manager *info)
{
	//u32 constant_voltage;
	int ffc_constant_voltage;
	int select_cv;

	ffc_constant_voltage = swchg_get_cycle_count_level(info);
	select_cv = min(ffc_constant_voltage,g_manager->jeita_cv);
	if (ENABLE_SW_FCC && ffc_constant_voltage != 0) {
		pr_err("%s: select_cv = %d\n", __func__, select_cv);
		charger_set_term_volt(info->charger, select_cv);
		return;
	}
}

const static char * adc_name[] = {
	"VBUS","VSYS","VBAT","VAC","IBUS","IBAT","TSBUS","TSBAT","TDIE",
};

void charger_set_suspend_current(bool en)
{
	struct charger_dev *manager = g_manager->charger;

	if (g_manager == NULL || manager == NULL)
          	return;
	printk("%s suspend_current_en:%d\n",__func__,en);
	if (en) {
		g_manager->suspend_current = true;
		charger_set_ichg(manager , 50);
	} else {
		g_manager->suspend_current = false;
		charger_set_ichg(manager , 3600);
	}
	return;
}
EXPORT_SYMBOL(charger_set_suspend_current);

static int charger_manager_thread_fn(void *data)
{
	struct charger_manager *manager = data;
#ifdef CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE
	struct charger_dev *charger = manager->charger;
#endif /* CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE */
	int ret = 0;
	bool adapter_plug_in = false;
	uint32_t adc[SC_ADC_MAX] = {0};
	uint32_t adc_buf_len = 0;
	char adc_buf[MIAN_CHG_ADC_LENGTH + 1];
	uint8_t i = 0;
	bool online = false;
	bool fast_charging = false;
	enum vbus_type vbus_type;
	bool qc_detect = false;
	uint8_t bc_retry = 0;
	static int float_count = 0;
	int retry_count = 0;

	while (true) {
		ret = wait_event_interruptible(manager->wait_queue,
						   manager->run_thread);
		if (kthread_should_stop() || ret) {
			dev_notice(manager->dev, "%s exits(%d)\n", __func__, ret);
			break;
		}

		manager->run_thread = false;
		msleep(300);

#ifdef CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE
		ret = charger_get_online(charger, &online);
		ret |= charger_get_vbus_type(charger, &vbus_type);
		if (ret < 0)
			continue;

		if (online != adapter_plug_in) {
			adapter_plug_in = online;
			if (adapter_plug_in) {
				dev_info(manager->dev, "adapter plug in\n");
				qc_detect = false;
				charger_adc_enable(charger, true);
				if ((manager->chg_wakelock) && (!manager->chg_wakelock->active))
  			        __pm_stay_awake(manager->chg_wakelock);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
				plugin_val = 1;
				pr_err("%s notify plugin", __func__);
				ret = main_chg_notifier_call_chain(MAIN_CHG_USB_PLUGIN_EVENT, plugin_val);
				if (ret) {
					pr_err("%s: main_chg_notifier_call_chain error:%d\n", __func__, ret);
				}
#endif
			} else {
				__pm_relax(manager->chg_wakelock);
				bc_retry = 0;
				cancel_delayed_work_sync(&manager->second_detect_work);
				float_count = 0;
				charger_adc_enable(charger, false);
				chg_get_quick_charger_type();
				quick_charge_type_uevent();
				fast_charging = false;
				dev_info(manager->dev, "adapter plug out\n");
				charger_set_input_curr_lmt(charger, 100);
				charger_set_suspend_current(false);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
				plugin_val = 2;
				pr_err("%s notify plugout", __func__);
				ret = main_chg_notifier_call_chain(MAIN_CHG_USB_PLUGIN_EVENT, plugin_val);
				if (ret) {
					pr_err("%s: main_chg_notifier_call_chain error:%d\n", __func__, ret);
				}
#endif
			}
		}

		power_supply_changed(manager->usb_psy);
		if (!adapter_plug_in)
			continue;

		// BC1.2 Detect
		switch (vbus_type) {
		case VBUS_TYPE_NONE:
		case VBUS_TYPE_UNKNOWN:
			  if (charger_is_sy6976(charger) && manager->is_pr_swap && manager->pd_active) {
                        	pr_err("pd authenticating,do noting!\n");
                        	break;
			}
		      if (g_manager->typec_mode != 2 && g_manager->typec_mode != 3) {
				if(manager->pd_active)
                    			retry_count = 4;
               			 else
                    			retry_count = 20;
				if (bc_retry < retry_count) {
					charger_force_dpdm(charger);
					bc_retry++;
				}else if (float_count <= 3 && manager->pd_active != true) {
					pr_err("float type!\n");
					schedule_delayed_work(&manager->second_detect_work, msecs_to_jiffies(5000));
					float_count++;
				}
			}
			break;
		case VBUS_TYPE_DCP:
			break;
		case VBUS_TYPE_SDP:
			if (!get_usb_ready(manager)) {
				if (manager->get_usb_rdy_cnt == 0)
					schedule_delayed_work(&manager->wait_usb_ready_work, msecs_to_jiffies(0));
			}
			break;
		default:
			break;
		}

		if (vbus_type == VBUS_TYPE_DCP && !qc_detect && !manager->pd_active) {
			qc_detect = true;
			charger_qc_identify(charger);
		}
		
		pr_err("fast_charging:%d,vbus_type:%d,pd_active:%d !\n",fast_charging,vbus_type,manager->pd_active);
		if (!fast_charging && (vbus_type == VBUS_TYPE_HVDCP || manager->pd_active)) {
			chg_get_quick_charger_type();
			quick_charge_type_uevent();
			fast_charging = true;
		}
		adc_buf_len = 0;
		for (i = 0; i < SC_ADC_BATTID; i++) {
			ret = charger_get_adc(charger, i, &adc[i]);
			if (ret < 0) {
				dev_info(manager->dev, "get adc failed\n");
				continue;
			}
			adc_buf_len += sprintf(adc_buf + adc_buf_len,
							"%s : %d,", adc_name[i], adc[i]);
		}
		dev_info(manager->dev, "adc_buf_len=%d\n", adc_buf_len);
		if (adc_buf_len > MIAN_CHG_ADC_LENGTH)
			adc_buf[MIAN_CHG_ADC_LENGTH] = '\0';
		dev_info(manager->dev, "%s, bc_type = %s\n",
						adc_buf, bc12_result[vbus_type]);
		sc_jeita_current_set(manager);
		swchg_select_cv(manager);
		charger_manager_check_vindpm(manager, adc[SC_ADC_VBAT]);
		charger_manager_check_iindpm(manager, vbus_type);
		sc_select_charging_current(manager);
		kpoc_power_off_check(manager);

		charger_manager_start_timer(manager, CHARGER_MANAGER_LOOP_TIME);
#endif /* CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE */
	}

	return 0;
}

void charger_set_chg_en(bool en)
{
	struct charger_dev *manager = g_manager->charger;
	if (g_manager == NULL || manager == NULL)
		return;
	printk("%s chg_en:%d\n",__func__,en);
	if (en) {
		charger_set_chg(manager, true);
	} else {
		charger_set_chg(manager, false);
	}
	return;
}
EXPORT_SYMBOL(charger_set_chg_en);

#ifdef CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE
static int charger_manager_notifer_call(struct notifier_block *nb, 
					unsigned long event, void *data)
{
	// struct charger_dev *charger = data;

	charger_manager_wake_thread(g_manager);

	return NOTIFY_OK;
}
#endif /* CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE */

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
bool endurance_protect = false;
EXPORT_SYMBOL(endurance_protect);
static int main_chg_notifier_event_callback(struct notifier_block *notifier,
			unsigned long chg_event, void *val)
{
	switch (chg_event) {
        //charger_enable_event 0:none 1:enable 2:disable
	case MAIN_CHG_ENABLE_EVENT:
		g_manager->charger_enable_event = *(int *)val;
		pr_info("%s: get charger_enable_event: %d\n", __func__, g_manager->charger_enable_event);
		if(g_manager->charger_enable_event == 1) {
			charger_set_chg_en(true);
			endurance_protect = false;
			pr_info("%s enable charging...\n", __func__);
		}
		else if(g_manager->charger_enable_event == 2) {
			charger_set_chg_en(false);
			endurance_protect = true;
			pr_info("%s disable charging...\n", __func__);
		}
		break;
	default:
		pr_err("%s: not supported charger notifier event: %lu\n", __func__, chg_event);
		break;
	}
	return NOTIFY_DONE;
}
#endif

static int charger_manager_tcpc_notifier_call(struct notifier_block *nb,
                    unsigned long event, void *data)
{
    struct tcp_notify *noti = data;
    struct charger_manager *manager =
        container_of(nb, struct charger_manager, pd_nb);
    struct charger_dev *charger = manager->charger;
    pr_err("%s %d\n", __func__, (int)event);
    switch (event) {
    case TCP_NOTIFY_SINK_VBUS:
        if (noti->vbus_state.type & TCP_VBUS_CTRL_PD_DETECT) {
            manager->pd_curr_max = noti->vbus_state.ma;
            pr_err("pd set curr:%d mA\n",manager->pd_curr_max);
            charger_set_input_curr_lmt(charger, manager->pd_curr_max);
        }
        break;
    case TCP_NOTIFY_PR_SWAP:
            manager->is_pr_swap = true;
        break;
    case TCP_NOTIFY_PD_STATE:
        switch (noti->pd_state.connected) {
        case PD_CONNECT_NONE:
            manager->pd_curr_max = 0;
            manager->pd_active = false;
	    manager->is_pr_swap = false;
            pr_err("pd set curr:%d mA\n",manager->pd_curr_max);
            break;
        case PD_CONNECT_PE_READY_SNK_APDO:
        case PD_CONNECT_PE_READY_SNK:
        case PD_CONNECT_PE_READY_SNK_PD30:
            manager->pd_active = true;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return NOTIFY_OK;
}

static int charger_manager_probe(struct platform_device *pdev)
{
	struct charger_manager *manager;
	int ret;
	dev_info(&pdev->dev, "%s running\n", __func__);

	manager = devm_kzalloc(&pdev->dev, sizeof(*manager), GFP_KERNEL);
	if (!manager)
		return -ENOMEM;

	g_manager = manager;
	g_manager->system_temp_level_max = sizeof(thermal_mitigation)/sizeof(thermal_mitigation[0]);
	g_manager->thermal_mitigation_current = thermal_mitigation[0];
	g_manager->suspend_current = false;
	manager->dev = &pdev->dev;
	manager->chg_wakelock = wakeup_source_register(NULL, "charging_wakelock");
	platform_set_drvdata(pdev, manager);
#ifdef CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE
	manager->charger = charger_find_dev_by_name("sc_charger");
	if (!manager->charger) 
		return -EPROBE_DEFER;
	init_waitqueue_head(&manager->wait_queue);
	manager->charger_nb.notifier_call = charger_manager_notifer_call;
	charger_register_notifier(&manager->charger_nb);
#endif /* CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE */
	charger_manager_ac_psy_register(manager);
	manager->tcpc = tcpc_dev_get_by_name("type_c_port0");
    if (!manager->tcpc) {
        pr_err("%s get tcpc dev failed\n", __func__);
        return PTR_ERR(manager->tcpc);
    }
    manager->pd_nb.notifier_call = charger_manager_tcpc_notifier_call;
    ret = register_tcp_dev_notifier(manager->tcpc, &manager->pd_nb,
                                TCP_NOTIFY_TYPE_ALL);
    if (ret < 0) {
        pr_err("%s register tcpc notifier fail(%d)\n",
				   __func__, ret);
        return ret;
    }
	charger_manager_usb_psy_register(manager);
	if (!IS_ERR_OR_NULL(manager->usb_psy)) {
		ret = usb_sysfs_create_group(manager);
		if (ret < 0)
			dev_info(&pdev->dev, "%s create some usb nodes failed\n", __func__);
	}
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	//register main_chg_notifier
        manager->main_chg_nb.notifier_call = main_chg_notifier_event_callback;
	main_chg_reg_notifier(&manager->main_chg_nb);
#endif
	manager->run_thread = true;
	manager->thread = kthread_run(charger_manager_thread_fn, manager,
								"charger_manager_thread");
	INIT_DELAYED_WORK(&manager->second_detect_work, apsd_second_detect_work);
	INIT_DELAYED_WORK(&manager->wait_usb_ready_work, wait_usb_ready_work);
	INIT_DELAYED_WORK(&manager->power_off_check_work, power_off_check_work);
	INIT_DELAYED_WORK(&manager->charger_set_input_work,charger_set_input_workfunc);
	schedule_delayed_work(&manager->power_off_check_work, msecs_to_jiffies(5000));
	dev_info(&pdev->dev, "%s success\n", __func__);
	return 0;
}

static int charger_manager_remove(struct platform_device *pdev)
{
	struct charger_manager *manager = platform_get_drvdata(pdev);
	charger_unregister_notifier(&manager->charger_nb);
	cancel_delayed_work(&manager->power_off_check_work);
	return 0;
}

static const struct of_device_id charger_manager_match[] = {
    {.compatible = "southchip,charger_manager",},
    {},
};
MODULE_DEVICE_TABLE(of, charger_manager_match);

static struct platform_driver charger_manager_driver = {
    .probe = charger_manager_probe,
    .remove = charger_manager_remove,
    .driver = {
        .name = "charger_manager",
        .of_match_table = charger_manager_match,
    },
};

static int __init charger_manager_init(void)
{
	printk("%s\n", __func__);
	return platform_driver_register(&charger_manager_driver);
}

static void __exit charger_manager_exit(void)
{
    platform_driver_unregister(&charger_manager_driver);
}
late_initcall(charger_manager_init);
module_exit(charger_manager_exit);

MODULE_AUTHOR("Boqiang Liu <air-liu@southchip.com>");
MODULE_DESCRIPTION("Southchip Charger Manager Core");
MODULE_LICENSE("GPL v2");
