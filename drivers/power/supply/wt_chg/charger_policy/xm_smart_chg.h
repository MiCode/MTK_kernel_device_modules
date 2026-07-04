#ifndef __XM_SMART_CHG_H__
#define __XM_SMART_CHG_H__

#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/notifier.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/mod_devicetable.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/kthread.h>
#include <generated/autoconf.h>
#include "charger_manager.h"
#include "../charger_class/charger_class.h"
#define XM_CHARGE_WORK_MS		30000
#define XM_SMART_CHG_POST_INIT_MS	3000

enum smart_chg_functype{
	SMART_CHG_STATUS_FLAG = 0,
	SMART_CHG_FEATURE_MIN_NUM = 1,
	SMART_CHG_NAVIGATION = 1,
	SMART_CHG_OUTDOOR_CHARGE,
        SMART_CHG_LOW_FAST = 3,
        SMART_CHG_ENDURANCE_PRO = 4,
	/* add new func here */
	SMART_CHG_FEATURE_MAX_NUM = 15,
};

struct smart_chg {
	bool en_ret;
	int active_status;
	int func_val;
};

struct xm_smart_chg_info {
        struct device           *dev;
        struct power_supply     *batt_psy;
        struct delayed_work     xm_smart_chg_post_init_work;
        /* charger_plugin_event 0:none 1:plugin 2:plugout */
        int                     charger_plugin_event;
        struct notifier_block   main_chg_nb;
        /* smart_chg_functype 4:endurance */
        struct delayed_work     xm_charge_work;
        struct smart_chg        smart_charge[SMART_CHG_FEATURE_MAX_NUM + 1];
        int                     smart_chg_cmd;
        bool                    smart_ctrl_en;
        bool                    endurance_ctrl_en;
        int                     soc;
};

#endif