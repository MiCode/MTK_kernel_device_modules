// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2025 Huaqin Technology(Shanghai) Co., Ltd.
 */

#include <linux/string.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/stddef.h>
#include <linux/kthread.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/time.h>

#include "hq_charger_manager.h"
#include "xm_chg_dfs.h"
#include "hq_printk.h"
#include "hq_notify.h"
#include <linux/err.h>

#define xm_err     hq_err
#define xm_warn    hq_warn
#define xm_notice  hq_notice
#define xm_info    hq_info
#define xm_debug   hq_debug

#ifdef TAG
#undef TAG
#define TAG "[HQ_CHG_DFS]"
#endif

static struct class *dfs_class;
static struct device *dfs_class_device;
struct xm_chg_dfs *g_chg_dfs;

static struct xm_chg_dfx_evt_cond dfx_evt_cond[] = {
    	{
		.dfx_type = CHG_DFX_PD_AUTH_FAIL,
		.is_first_report = true,
		.boot_report = false,
		.charging_report = true,
		.max_report_times = 1,
		.report_times = 0
	},
        {
		.dfx_type = CHG_DFX_CP_ENABLE_ERR,
		.is_first_report = true,
		.boot_report = false,
		.charging_report = true,
		.max_report_times = 3,
		.report_times = 0
	},
	{
		.dfx_type = CHG_DFX_NONE_STANDARD_CHG,
		.is_first_report = true,
		.boot_report = false,
		.charging_report = true,
		.max_report_times = 1,
		.report_times = 0
	},
        {
		.dfx_type = CHG_DFX_CORROSION_DISCHARGE,
		.is_first_report = true,
		.boot_report = false,
		.charging_report = true,
		.max_report_times = 3,
		.report_times = 0
	},
        {
		.dfx_type = CHG_DFX_CP_IBUS_OCP,
		.is_first_report = true,
		.boot_report = false,
		.charging_report = true,
		.max_report_times = 3,
		.report_times = 0
	},
        {
		.dfx_type = CHG_DFX_CP_IBAT_OCP,
		.is_first_report = true,
		.boot_report = false,
		.charging_report = true,
		.max_report_times = 3,
		.report_times = 0
	},
        {
		.dfx_type = CHG_DFX_CP_VBUS_OVP,
		.is_first_report = true,
		.boot_report = false,
		.charging_report = true,
		.max_report_times = 3,
		.report_times = 0
	},
        {
		.dfx_type = CHG_DFX_CP_VBAT_OVP,
		.is_first_report = true,
		.boot_report = false,
		.charging_report = true,
		.max_report_times = 3,
		.report_times = 0
	},
 	{
		.dfx_type = CHG_DFX_CHG_BATT_CYCLE,
		.is_first_report = true,
		.boot_report = true,
		.charging_report = false,
		.max_report_times = 0xFFFF,
		.report_times = 0
	},
	{
		.dfx_type = CHG_DFX_SOC_NOT_FULL,
		.is_first_report = true,
		.boot_report = false,
		.charging_report = true,
		.max_report_times = 1,
		.report_times = 0
	},
        {
		.dfx_type = CHG_DFX_FG_IIC_ERR,
		.is_first_report = true,
		.boot_report = false,
		.charging_report = true,
		.max_report_times = 3,
		.report_times = 0
	},
        {
		.dfx_type = CHG_DFX_CP_I2C_ERR,
		.is_first_report = true,
		.boot_report = false,
		.charging_report = true,
		.max_report_times = 3,
		.report_times = 0
	},
        {
		.dfx_type = CHG_DFX_BATT_LINKER_ABSENT,
		.is_first_report = true,
		.boot_report = false,
		.charging_report = true,
		.max_report_times = 3,
		.report_times = 0
	},
        {
		.dfx_type = CHG_DFX_CP_TDIE_HOT,
		.is_first_report = true,
		.boot_report = false,
		.charging_report = true,
		.max_report_times = 3,
		.report_times = 0
	},
        {
		.dfx_type = CHG_DFX_CP_VBUS_LOW,
		.is_first_report = true,
		.boot_report = false,
		.charging_report = true,
		.max_report_times = 3,
		.report_times = 0
	},
	{
		.dfx_type = CHG_DFX_NOT_CHG_IN_LOW_TEMP,
		.is_first_report = true,
		.boot_report = false,
		.charging_report = true,
		.max_report_times = 3,
		.report_times = 0
	},
	{
		.dfx_type = CHG_DFX_NOT_CHG_IN_HIGH_TEMP,
		.is_first_report = true,
		.boot_report = false,
		.charging_report = true,
		.max_report_times = 3,
		.report_times = 0
	},
	{
		.dfx_type = CHG_DFX_BATT_AUTH_FAIL,
		.is_first_report = true,
		.boot_report = false,
		.charging_report = true,
		.max_report_times = 1,
		.report_times = 0
	},
	{
		.dfx_type = CHG_DFX_TBAT_HOT,
		.is_first_report = true,
		.boot_report = false,
		.charging_report = false,
		.max_report_times = 3,
		.report_times = 0
	},
	{
		.dfx_type = CHG_DFX_TBAT_COLD,
		.is_first_report = true,
		.boot_report = false,
		.charging_report = false,
		.max_report_times = 3,
		.report_times = 0
	},
};

static void xm_chg_dfx_handle_event_report(struct xm_chg_dfs *chg_dfs);

static ssize_t dfs_test_store(struct device *dev,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	int ret;
	int value;

	ret = kstrtoint(buf, 10, &value);
	g_chg_dfs->dfx_type = value;
	xm_chg_dfx_handle_event_report(g_chg_dfs);

	return count;
}
static DEVICE_ATTR(dfs_test, 0664, NULL, dfs_test_store);

void mievent_upload(int miev_code, char *miev_param, int para_cnt)
{
	char buffer[128] = {0};
	char *p1, *p2, *key, *value;
	int intval;
	struct misight_mievent *event = cdev_tevent_alloc(miev_code);

	memcpy(buffer, miev_param, strlen(miev_param));

	xm_info("miev_code=%d para_cnt=%d\n", miev_code, para_cnt);

	p1 = buffer;
	while (p1 && (*p1 != '\0')) {
		p2 = strsep(&p1, ",");
		while (p2 && (*p2 != '\0')) {
			key = strsep(&p2, ":");
			if (key) {
				xm_info("[CHG_DFS] key:%s\n", key);
			} else {
				xm_err("[CHG_DFS] none key\n");
				key = "None";
			}

			value = strsep(&p2, ":");
			if (value) {
				xm_info("[CHG_DFS] value:%s\n", value);
			} else {
				xm_err("[CHG_DFS] none value\n");
				value = "None";
			}
		}

		if (kstrtoint(value, 10, &intval) != 0) {
			cdev_tevent_add_str(event, key, value);
			xm_info("type=string, %s:%s\n", key, value);
		} else {
			cdev_tevent_add_int(event, key, intval);
			xm_info("type=int, %s:%d\n", key, intval);
		}
	}

	cdev_tevent_write(event);
	cdev_tevent_destroy(event);

	return;
}

static void xm_chg_dfx_handle_event_report(struct xm_chg_dfs *chg_dfs)
{
	char para_str[PARAMETER_LEN_MAX] = {0};
	int para_cnt = 0;
	int len = 0;

	xm_info("dfx_type = %d\n", chg_dfs->dfx_type);

	switch (chg_dfs->dfx_type) {
	 case CHG_DFX_PD_AUTH_FAIL:
	 	len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "pdAuthFail");
	 	len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%x", "adapterId", chg_dfs->adapter_svid);
	 	para_cnt = 2;
		mievent_upload(DFX_ID_CHG_PD_AUTH_FAIL, para_str, para_cnt);
	 	break;

	 case CHG_DFX_NONE_STANDARD_CHG:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "noneStandartChg");
		para_cnt = 1;
		mievent_upload(DFX_ID_CHG_NONE_STANDARD_CHG, para_str, para_cnt);
		break;

	 case CHG_DFX_CORROSION_DISCHARGE:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CorrosionDischarge");
		para_cnt = 1;
		mievent_upload(DFX_ID_CHG_CORROSION_DISCHARGE, para_str, para_cnt);
		break;

	 case CHG_DFX_FG_IIC_ERR:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "fgI2cErr");
	 	para_cnt = 1;
	 	mievent_upload(DFX_ID_CHG_FG_IIC_ERR, para_str, para_cnt);
	 	break;

	 case CHG_DFX_CP_ENABLE_ERR:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpEnFail");
	 	para_cnt = 1;
	 	mievent_upload(DFX_ID_CHG_CP_ENABLE_ERR, para_str, para_cnt);
	 	break;

	 case CHG_DFX_BATT_LINKER_ABSENT:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "BattLinkerAbsent");
		para_cnt = 1;
		mievent_upload(DFX_ID_CHG_BATT_LINKER_ABSENT, para_str, para_cnt);
		break;

	 case CHG_DFX_NOT_CHG_IN_LOW_TEMP:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "NotChgInLowTemp");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbat", chg_dfs->tbat);
		para_cnt = 2;
		mievent_upload(DFX_ID_CHG_NOT_CHG_IN_LOW_TEMP, para_str, para_cnt);
		break;

	 case CHG_DFX_NOT_CHG_IN_HIGH_TEMP:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "NotChgInHighTemp");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbat", chg_dfs->tbat);
		para_cnt = 2;
		mievent_upload(DFX_ID_CHG_NOT_CHG_IN_HIGH_TEMP, para_str, para_cnt);
		break;

	 case CHG_DFX_CP_I2C_ERR:
	 	len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpI2CErr");
	 	len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d","cp_absent", chg_dfs->cp_absent_flag);
	 	para_cnt = 2;
	 	mievent_upload(DFX_ID_CHG_CP_I2C_ERR, para_str, para_cnt);
	 	break;

	 case CHG_DFX_CHG_BATT_CYCLE:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgStatInfo", "chgBattCycle");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "CycleCnt", chg_dfs->cycle_cnt);
		para_cnt = 2;
		mievent_upload(DFX_ID_CHG_BATT_CYCLE_COUNT, para_str, para_cnt);
		break;

         case CHG_DFX_CP_VBUS_LOW:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgStatInfo", "VbusUvlo");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "vbus", chg_dfs->vbus);
		para_cnt = 2;
		mievent_upload(DFX_ID_CHG_CP_VBUS_LOW, para_str, para_cnt);
		break;
            
	 case CHG_DFX_CP_IBAT_OCP:
	 	len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpIbatOcp");
	 	para_cnt = 1;
	 	mievent_upload(DFX_ID_CHG_CP_IBAT_OCP, para_str, para_cnt);
	 	break;

	 case CHG_DFX_CP_IBUS_OCP:
	 	len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpIbusOcp");
	 	para_cnt = 1;
	 	mievent_upload(DFX_ID_CHG_CP_IBUS_OCP, para_str, para_cnt);
	 	break;

	 case CHG_DFX_CP_VBAT_OVP:
	 	len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpVbatOvp");
	 	para_cnt = 1;
	 	mievent_upload(DFX_ID_CHG_CP_VBAT_OVP, para_str, para_cnt);
	 	break;

	 case CHG_DFX_CP_VBUS_OVP:
	 	len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpVbusOvp");
	 	para_cnt = 1;
	 	mievent_upload(DFX_ID_CHG_CP_VBUS_OVP, para_str, para_cnt);
	 	break;

	case CHG_DFX_SOC_NOT_FULL:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "SocNotFull");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "vbat", chg_dfs->vbat);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "soc", chg_dfs->soc);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "rsoc", chg_dfs->rsoc);
		para_cnt = 4;
		mievent_upload(DFX_ID_CHG_UISOC_NOT_FULL, para_str, para_cnt);
		break;

	case CHG_DFX_CP_TDIE_HOT:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpTdieHot");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tdie", chg_dfs->tdie);
		para_cnt = 2;
		mievent_upload(DFX_ID_CHG_CP_TDIE_HOT, para_str, para_cnt);
		break;

	case CHG_DFX_BATT_AUTH_FAIL:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "BattAuthFail");
		para_cnt = 1;
		mievent_upload(DFX_ID_CHG_BATT_AUTH_FAIL, para_str, para_cnt);
		break;

	case CHG_DFX_TBAT_HOT:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgStatInfo", "TbatHot");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbat", chg_dfs->tbat);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbatMax", (chg_dfs->tbat_max / 10));
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "isCharging", chg_dfs->adapter_plug_in);
		para_cnt = 4;
		mievent_upload(DFX_ID_CHG_TBAT_HOT, para_str, para_cnt);
		break;

	case CHG_DFX_TBAT_COLD:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgStatInfo", "TbatCold");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbat", chg_dfs->tbat);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbatMin", (chg_dfs->tbat_min / 10));
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "isCharging", chg_dfs->adapter_plug_in);
		para_cnt = 4;
		mievent_upload(DFX_ID_CHG_TBAT_COLD, para_str, para_cnt);
		break;

	default:
		xm_err("[HQ_CHG_DFS]: unknown type to report\n");
	}

	return;
}


static int xm_chg_dfx_report_events(struct xm_chg_dfs *chg_dfs)
{
	int evt_index = 0;

	xm_info("evt_report_bits = 0lx%lX\n", chg_dfs->evt_report_bits[0]);

	for_each_set_bit(evt_index, chg_dfs->evt_report_bits, CHG_DFX_MAX_TYPE) {
		chg_dfs->dfx_type = evt_index;
		xm_chg_dfx_handle_event_report(chg_dfs);
		clear_bit(evt_index, chg_dfs->evt_report_bits);
	}

	return 0;
}

static void wake_up_report(struct xm_chg_dfs *chg_dfs)
{
	if (!atomic_cmpxchg(&chg_dfs->run_report, 0, 1)) {
		wake_up_interruptible(&chg_dfs->report_wq);
	}
}

static int xm_chg_dfx_report_thread_fn(void *data)
{
	struct xm_chg_dfs *chg_dfs = data;

	while (!kthread_should_stop()) {
		wait_event_interruptible(chg_dfs->report_wq,
			(atomic_read(&chg_dfs->run_report) || kthread_should_stop()));

		atomic_set(&chg_dfs->run_report, 0);

		xm_chg_dfx_report_events(chg_dfs);
	}

	return 0;
}

static int chg_dfs_psy_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct xm_chg_dfs *chg_dfs = container_of(nb, struct xm_chg_dfs, psy_nb);
	struct power_supply *psy = data;
	union power_supply_propval pval;
	int ret = 0;
	struct charger_manager *manager = power_supply_get_drvdata(psy);

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;
  
  	if (strcmp(psy->desc->name, "sc-cp-master") == 0) {
			ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TEMP, &pval);
			if (ret != 0)
				return NOTIFY_DONE;
			chg_dfs->tdie = pval.intval;  
	
			ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
			if (ret != 0)
				return NOTIFY_DONE;
			chg_dfs->vbus = pval.intval;  
	
			ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE, &pval);
			if (ret != 0)
				return NOTIFY_DONE;
			chg_dfs->cp_absent_flag = pval.intval;
	}
  
	if (strcmp(psy->desc->name, "battery") == 0) {
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
		if (ret != 0)
			return NOTIFY_OK; 
		chg_dfs->cycle_cnt = pval.intval;

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TEMP, &pval);
		if (ret != 0)
			return NOTIFY_OK;
		chg_dfs->tbat = pval.intval;

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_AUTHENTIC, &pval);
		if (ret != 0)
			return NOTIFY_OK;
		chg_dfs->batt_auth = pval.intval;

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
		if (ret != 0)
			return NOTIFY_OK;
		chg_dfs->soc = pval.intval;

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (ret != 0)
			return NOTIFY_OK;
		chg_dfs->vbat = pval.intval;

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT, &pval);
		if (ret != 0)
			return NOTIFY_OK;
		chg_dfs->batt_linker_absent_flag = pval.intval;

		
		chg_dfs->vbus_type = manager->vbus_type;
		chg_dfs->rsoc = manager->rsoc;
		//chg_dfs->tboard = manager->board_temp;
		chg_dfs->tbat_max = max(chg_dfs->tbat, chg_dfs->tbat_max);
		chg_dfs->tbat_min = min(chg_dfs->tbat, chg_dfs->tbat_min);
	}
  
    	/* process usb psy changed */
	if (strcmp(psy->desc->name, "usb") == 0) {
		//chg_dfs->real_type = manager->real_type;
		/* pd verify */
          	if (!IS_ERR_OR_NULL(manager->pd_adapter)) {
			chg_dfs->adapter_svid = manager->pd_adapter->adapter_svid;
                        chg_dfs->pd_verify_done = manager->pd_adapter->verify_process;
		        chg_dfs->pd_auth_fail = !manager->pd_adapter->verifed;
                } else { /* TODO: remove these after decouple */
			xm_err("pd adapter is null\n");
		}
	}

	if (strcmp(psy->desc->name, "bms") == 0) {
			ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT, &pval);
			if (ret != 0)
				return NOTIFY_OK;
			chg_dfs->fg_absent_flag = pval.intval;
	}

	return NOTIFY_DONE;
}

static int chg_dfs_cp_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
        struct xm_chg_dfs *chg_dfs = container_of(nb, struct xm_chg_dfs, cp_nb);

        switch (event) {
        case CP_EVENT_VBUS_OVP: /* VBUS_OVP */
		xm_info("receive CHG_DFX_VBUS_OVP event\n");
		chg_dfs->vbus_ovp_flag = 1;
		break;
        case CP_EVENT_IBUS_OCP: /* IBUS_OCP */
		xm_info("receive CHG_DFX_IBUS_OCP event\n");
		chg_dfs->ibus_ocp_flag = 1;
		break;
        case CP_EVENT_VBAT_OVP: /* VBAT_OVP */
		xm_info("receive CHG_DFX_VBAT_OVP event\n");
		chg_dfs->vbat_ovp_flag = 1;
		break;
        case CP_EVENT_IBAT_OCP: /* IBAT_OCP */
		xm_info("receive CHG_DFX_IBAT_OCP event\n");
		chg_dfs->ibat_ocp_flag = 1;
		break;
        case CP_EVENT_ENABLE_ERR: /* CP enable err */
		xm_info("receive CP_EVENT_ENABLE_ERR event\n");
		chg_dfs->cp_enable_err_flag = 1;
		break;
        default:
              return NOTIFY_DONE;
        }

	return NOTIFY_DONE;
}

static int chg_dfs_charger_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	/*struct xm_chg_dfs *chg_dfs = container_of(nb, struct xm_chg_dfs, charger_nb);

        switch (event) {
        case CHARGER_EVENT_CID: 
		xm_info("receive CHG_DFX_VBUS_OVP event\n");
		chg_dfs->cid_flag = 1;
		break;
        default:
              return NOTIFY_DONE;
        }*/

	return NOTIFY_DONE;
}

static int chg_dfs_fg_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	// struct xm_chg_dfs *chg_dfs = container_of(nb, struct xm_chg_dfs, fg_nb);

	return NOTIFY_DONE;
}

static int chg_dfs_cm_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct xm_chg_dfs *chg_dfs = container_of(nb, struct xm_chg_dfs, cm_nb);
	int i = 0;
	struct xm_chg_dfx_evt_cond *cond = NULL;

	switch (event) {
	case 0: /* adapter plugout */
		xm_info("receive adapter plugout event\n");

		chg_dfs->check_interval_ms = 10000;
		chg_dfs->adapter_plug_in = 0;

		break;

	case 1: /* adapter plugin */
		xm_info("receive adapter plugin event\n");

		for (i = 0; i < ARRAY_SIZE(dfx_evt_cond); i++) {
			cond = &dfx_evt_cond[i];

			if (cond->dfx_type != CHG_DFX_CHG_BATT_CYCLE)
				cond->report_times = 0;
		}

		chg_dfs->check_interval_ms = 5000;
		chg_dfs->adapter_plug_in = 1;

		break;

	}

	return NOTIFY_DONE;
}

void handle_evt_cond_check_work(struct work_struct *work)
{
	struct xm_chg_dfs *chg_dfs = container_of(work, struct xm_chg_dfs, evt_cond_check_work.work);
	int i = 0;
	struct xm_chg_dfx_evt_cond *cond = NULL;
	int status = 0;
	int ret = 0;
	//struct timespec64 ts;

	mutex_lock(&chg_dfs->cond_check_lock);

	// ktime_get_boottime_ts64(&ts);
	// if ((u64)ts.tv_sec < 60) {
	// 	xm_info("kernel boot time less than 60s, skip check\n");
	// 	mutex_unlock(&chg_dfs->cond_check_lock);
	// 	return;
	// }

	ret = charger_get_chip_chg_status(chg_dfs->charger, &status);
	if (ret < 0) {
		xm_err("failed to get chg status from charger\n");
	}

	chg_dfs->chip_chg_status = status;

	xm_info("cycle_cnt: %d, tbat: %d [%d %d], batt_auth: %d, soc: %d, rsoc: %d, vbat: %d, chip_chg_status: %d, vbus_type: %d,vbus: %d,tdie: %d\n",
		chg_dfs->cycle_cnt, chg_dfs->tbat, chg_dfs->tbat_min, chg_dfs->tbat_max, chg_dfs->batt_auth, chg_dfs->soc, chg_dfs->rsoc, chg_dfs->vbat, chg_dfs->chip_chg_status, chg_dfs->vbus_type,chg_dfs->vbus,chg_dfs->tdie);


	for (i = 0; i < ARRAY_SIZE(dfx_evt_cond); i++) {
		cond = &dfx_evt_cond[i];

		switch (cond->dfx_type) {
		case CHG_DFX_NONE_STANDARD_CHG:
			if (chg_dfs->vbus_type == VBUS_TYPE_FLOAT) {
				if (cond->report_times < cond->max_report_times) {
					set_bit(CHG_DFX_NONE_STANDARD_CHG, chg_dfs->evt_report_bits);
					cond->report_times++;
					cond->is_first_report = false;
				} else {
					//clear_bit(CHG_DFX_NONE_STANDARD_CHG, chg_dfs->evt_report_bits);
				}
			}
			break;

		/*case CHG_DFX_CORROSION_DISCHARGE:
			if (chg_dfs->cid_flag == 1) {
				if (cond->report_times < cond->max_report_times) {
					set_bit(CHG_DFX_CORROSION_DISCHARGE, chg_dfs->evt_report_bits);
					cond->report_times++;
					cond->is_first_report = false;
				} else {
					//clear_bit(CHG_DFX_TBAT_COLD, chg_dfs->evt_report_bits);
				}
			}
			break;*/

		case CHG_DFX_NOT_CHG_IN_LOW_TEMP:
			if (chg_dfs->tbat > -100 && chg_dfs->tbat < 0 && chg_dfs->adapter_plug_in) {
				if (chg_dfs->chip_chg_status == 0) {
					if (cond->report_times < cond->max_report_times) {
						set_bit(CHG_DFX_NOT_CHG_IN_LOW_TEMP, chg_dfs->evt_report_bits);
						cond->report_times++;
						cond->is_first_report = false;
					}
				} else {
					//clear_bit(CHG_DFX_NOT_CHG_IN_LOW_TEMP, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_NOT_CHG_IN_HIGH_TEMP:
			if (chg_dfs->tbat < 600 && chg_dfs->tbat > 450 && chg_dfs->adapter_plug_in) {
				if (chg_dfs->chip_chg_status == 0) {
					if (cond->report_times < cond->max_report_times) {
						set_bit(CHG_DFX_NOT_CHG_IN_HIGH_TEMP, chg_dfs->evt_report_bits);
						cond->report_times++;
						cond->is_first_report = false;
					}
				} else {
					//clear_bit(CHG_DFX_NOT_CHG_IN_HIGH_TEMP, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_CHG_BATT_CYCLE:
			if (chg_dfs->cycle_cnt != chg_dfs->last_cycle_count) {
				if ((chg_dfs->cycle_cnt % 100) == 0 || cond->is_first_report) {
					if (cond->report_times < cond->max_report_times) {
						set_bit(CHG_DFX_CHG_BATT_CYCLE, chg_dfs->evt_report_bits);
						cond->report_times++;
						cond->is_first_report = false;
					} else {
						//clear_bit(CHG_DFX_CHG_BATT_CYCLE, chg_dfs->evt_report_bits);
					}
				}
				chg_dfs->last_cycle_count = chg_dfs->cycle_cnt;
			}
			break;

		case CHG_DFX_SOC_NOT_FULL:
			if ((chg_dfs->chip_chg_status == 5) && (chg_dfs->soc != 100)) {
				if (cond->report_times < cond->max_report_times) {
					set_bit(CHG_DFX_SOC_NOT_FULL, chg_dfs->evt_report_bits);
					cond->report_times++;
					cond->is_first_report = false;
				} else {
					//clear_bit(CHG_DFX_SOC_NOT_FULL, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_BATT_AUTH_FAIL:
			if (chg_dfs->batt_auth == 0) {
				if (cond->report_times < cond->max_report_times) {
					set_bit(CHG_DFX_BATT_AUTH_FAIL, chg_dfs->evt_report_bits);
					cond->report_times++;
					cond->is_first_report = false;
				} else {
					//clear_bit(CHG_DFX_BATT_AUTH_FAIL, chg_dfs->evt_report_bits);
				}
			}
			break;

                case CHG_DFX_CP_VBUS_LOW:
			if ((chg_dfs->vbus >= 2000) && (chg_dfs->vbus <= 3000)) {
				if (cond->report_times < cond->max_report_times) {
					set_bit(CHG_DFX_CP_VBUS_LOW, chg_dfs->evt_report_bits);
					cond->report_times++;
					cond->is_first_report = false;
				} else {
					//clear_bit(CHG_DFX_SOC_NOT_FULL, chg_dfs->evt_report_bits);
				}
			}
			break;
                    
		case CHG_DFX_TBAT_HOT:
			if (chg_dfs->tbat > 550) {
				if (cond->report_times < cond->max_report_times) {
					set_bit(CHG_DFX_TBAT_HOT, chg_dfs->evt_report_bits);
					cond->report_times++;
					cond->is_first_report = false;
				} else {
					//clear_bit(CHG_DFX_TBAT_HOT, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_TBAT_COLD:
			if (chg_dfs->tbat < -100) {
				if (cond->report_times < cond->max_report_times) {
					set_bit(CHG_DFX_TBAT_COLD, chg_dfs->evt_report_bits);
					cond->report_times++;
					cond->is_first_report = false;
				} else {
					//clear_bit(CHG_DFX_TBAT_COLD, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_BATT_LINKER_ABSENT:
			if (chg_dfs->batt_linker_absent_flag == 0) {
				if (cond->report_times < cond->max_report_times) {
					set_bit(CHG_DFX_BATT_LINKER_ABSENT, chg_dfs->evt_report_bits);
					cond->report_times++;
					cond->is_first_report = false;
				} else {
					//clear_bit(CHG_DFX_TBAT_COLD, chg_dfs->evt_report_bits);
				}
			}
			break;
		case CHG_DFX_CP_VBUS_OVP:
			if (chg_dfs->vbus_ovp_flag == 1) {
				if (cond->report_times < cond->max_report_times) {
					set_bit(CHG_DFX_CP_VBUS_OVP, chg_dfs->evt_report_bits);
					cond->report_times++;
					cond->is_first_report = false;
				} else {
					//clear_bit(CHG_DFX_TBAT_COLD, chg_dfs->evt_report_bits);
				}
			}
			break;
		case CHG_DFX_CP_IBUS_OCP:
			if (chg_dfs->ibus_ocp_flag == 1) {
				if (cond->report_times < cond->max_report_times) {
					set_bit(CHG_DFX_CP_IBUS_OCP, chg_dfs->evt_report_bits);
					cond->report_times++;
					cond->is_first_report = false;
				} else {
					//clear_bit(CHG_DFX_TBAT_COLD, chg_dfs->evt_report_bits);
				}
			}
			break;
		case CHG_DFX_CP_VBAT_OVP:
			if (chg_dfs->vbat_ovp_flag == 1) {
				if (cond->report_times < cond->max_report_times) {
					set_bit(CHG_DFX_CP_VBAT_OVP, chg_dfs->evt_report_bits);
					cond->report_times++;
					cond->is_first_report = false;
				} else {
					//clear_bit(CHG_DFX_TBAT_COLD, chg_dfs->evt_report_bits);
				}
			}
			break;
		case CHG_DFX_CP_IBAT_OCP:
			if (chg_dfs->ibat_ocp_flag == 1) {
				if (cond->report_times < cond->max_report_times) {
					set_bit(CHG_DFX_CP_IBAT_OCP, chg_dfs->evt_report_bits);
					cond->report_times++;
					cond->is_first_report = false;
				} else {
					//clear_bit(CHG_DFX_TBAT_COLD, chg_dfs->evt_report_bits);
				}
			}
			break;
		case CHG_DFX_CP_TDIE_HOT:
			if (chg_dfs->tdie >= 90) {
				if (cond->report_times < cond->max_report_times) {
					set_bit(CHG_DFX_CP_TDIE_HOT, chg_dfs->evt_report_bits);
					cond->report_times++;
					cond->is_first_report = false;
				} else {
					//clear_bit(CHG_DFX_TBAT_COLD, chg_dfs->evt_report_bits);
				}
			}
			break;
                case CHG_DFX_PD_AUTH_FAIL:
			if (chg_dfs->adapter_svid == USB_PD_MI_SVID) {
				if (chg_dfs->pd_auth_fail && chg_dfs->pd_verify_done) {
					if (cond->report_times < cond->max_report_times) {
						set_bit(CHG_DFX_PD_AUTH_FAIL, chg_dfs->evt_report_bits);
						cond->report_times++;
						cond->is_first_report = false;
					} else {
						//clear_bit(CHG_DFX_PD_AUTH_FAIL, chg_dfs->evt_report_bits);
					}
				}
			}
			break;
		case CHG_DFX_CP_I2C_ERR:
			if (chg_dfs->cp_absent_flag == 1) {
				if (cond->report_times < cond->max_report_times) {
					set_bit(CHG_DFX_CP_I2C_ERR, chg_dfs->evt_report_bits);
					cond->report_times++;
					cond->is_first_report = false;
				} else {
					//clear_bit(CHG_DFX_TBAT_COLD, chg_dfs->evt_report_bits);
				}
			}
			break;
		case CHG_DFX_FG_IIC_ERR:
			if (chg_dfs->fg_absent_flag == 0) {
				if (cond->report_times < cond->max_report_times) {
					set_bit(CHG_DFX_FG_IIC_ERR, chg_dfs->evt_report_bits);
					cond->report_times++;
					cond->is_first_report = false;
				} else {
					//clear_bit(CHG_DFX_TBAT_COLD, chg_dfs->evt_report_bits);
				}
			}
			break;
		case CHG_DFX_CP_ENABLE_ERR:
			if (chg_dfs->cp_enable_err_flag == 1) {
				if (cond->report_times < cond->max_report_times) {
					set_bit(CHG_DFX_CP_ENABLE_ERR, chg_dfs->evt_report_bits);
					cond->report_times++;
					cond->is_first_report = false;
				} else {
					//clear_bit(CHG_DFX_TBAT_COLD, chg_dfs->evt_report_bits);
				}
			}
			break;

		default:
			xm_err("[HQ_CHG_DFS]: unknown type to report,dfx_type = %d\n", chg_dfs->dfx_type);
		}
	}

	if (bitmap_weight(chg_dfs->evt_report_bits, CHG_DFX_MAX_TYPE) > 0) {
		xm_info("%d dfx event need report\n", bitmap_weight(chg_dfs->evt_report_bits, CHG_DFX_MAX_TYPE));
		wake_up_report(chg_dfs);
	}

	mutex_unlock(&chg_dfs->cond_check_lock);

	schedule_delayed_work(&chg_dfs->evt_cond_check_work, msecs_to_jiffies(chg_dfs->check_interval_ms));
}
/*static void xm_chg_dfs_pcba_init(struct xm_chg_dfs *chg_dfs)
{
	struct PCBA_MSG *pcba_msg = get_pcba_msg();

	chg_dfs->board_version = OTHER_VERSION;
	if (!IS_ERR_OR_NULL(pcba_msg->sku)) {
		if ((strstr(pcba_msg->sku, "eea")))
			chg_dfs->board_version = EEA_VERSION;
		xm_info("board version = %d\n", chg_dfs->board_version);
	} else {
		xm_err("get board version fail\n");
	}
}*/

static void xm_chg_dfs_class_init(void)
{
	dfs_class = class_create("dfs_class");
	if (IS_ERR(dfs_class))
	{
		pr_err("dfs_class is error\n");
		return;
	}
	dfs_class_device = device_create(dfs_class, NULL, 0, NULL, "dfs");
	if (IS_ERR(dfs_class_device))
	{
		pr_err("dfs_class_device is error\n");
		class_destroy(dfs_class);
		return;
	}
	if (device_create_file(dfs_class_device, &dev_attr_dfs_test)) {
		device_destroy(dfs_class, 0);
		class_destroy(dfs_class);
		return;
	}
	pr_err("init dfs_test node done\n");

}

static int xm_chg_dfs_probe(struct platform_device *pdev)
{
	struct xm_chg_dfs *chg_dfs = NULL;
	int ret = 0;

	chg_dfs = devm_kzalloc(&pdev->dev, sizeof(*chg_dfs), GFP_KERNEL);
	if (!chg_dfs)
		return -ENOMEM;

	g_chg_dfs = chg_dfs;
	xm_chg_dfs_class_init();

	platform_set_drvdata(pdev, chg_dfs);

	init_waitqueue_head(&chg_dfs->report_wq);

	chg_dfs->report_thread = kthread_run(xm_chg_dfx_report_thread_fn, chg_dfs,
							"xm_chg_dfx_report_thread");
  
	chg_dfs->pd_adapter = get_adapter_by_name("pd_adapter");
	if (!chg_dfs->pd_adapter) {
		return -EPROBE_DEFER;
	}
  
	chg_dfs->adapter_svid = 0x0;
	chg_dfs->pd_auth_fail = false;
	chg_dfs->pd_verify_done = false;
	chg_dfs->last_cycle_count = -1;
	chg_dfs->check_interval_ms = 10000;
	chg_dfs->tbat_max = INT_MIN;
	chg_dfs->tbat_min = INT_MAX;
	chg_dfs->cp_absent_flag = 0;
	chg_dfs->tdie = 0;
	chg_dfs->ibat_ocp_flag = 0;
	chg_dfs->vbat_ovp_flag = 0;
	chg_dfs->ibus_ocp_flag = 0;
	chg_dfs->vbus_ovp_flag = 0;
        chg_dfs->fg_absent_flag = 1;
	chg_dfs->cp_enable_err_flag = 0;
	chg_dfs->batt_linker_absent_flag = 1;
	//xm_chg_dfs_pcba_init(chg_dfs);

	mutex_init(&chg_dfs->cond_check_lock);
	bitmap_zero(chg_dfs->evt_report_bits, CHG_DFX_MAX_TYPE);
	atomic_set(&chg_dfs->run_report, 0);
	INIT_DELAYED_WORK(&chg_dfs->evt_cond_check_work, handle_evt_cond_check_work);
	schedule_delayed_work(&chg_dfs->evt_cond_check_work, msecs_to_jiffies(60000));

	chg_dfs->psy_nb.notifier_call = chg_dfs_psy_notifier_call;
	ret = power_supply_reg_notifier(&chg_dfs->psy_nb);
	if (ret < 0) {
		xm_err("couldn't register psy notifier ret = %d\n", ret);
		return ret;
	}

	chg_dfs->charger_nb.notifier_call = chg_dfs_charger_notifier_call;
	ret = hq_charger_notifier_register(&chg_dfs->charger_nb);
	if (ret < 0) {
		xm_err("couldn't register charger notifier, ret = %d\n", ret);
		return ret;
	}

	chg_dfs->cp_nb.notifier_call = chg_dfs_cp_notifier_call;
	ret = hq_cp_notifier_register(&chg_dfs->cp_nb);
	if (ret < 0) {
		xm_err("couldn't register cp notifier, ret = %d\n", ret);
		return ret;
	}

	chg_dfs->fg_nb.notifier_call = chg_dfs_fg_notifier_call;
	ret = hq_fg_notifier_register(&chg_dfs->fg_nb);
	if (ret < 0) {
		xm_err("couldn't register fg notifier, ret = %d\n", ret);
		return ret;
	}

	chg_dfs->cm_nb.notifier_call = chg_dfs_cm_notifier_call;
	ret = hq_chargermanager_notifier_register(&chg_dfs->cm_nb);
	if (ret < 0) {
		xm_err("couldn't register charger manager notifier, ret = %d\n", ret);
		return ret;
	}

	chg_dfs->charger = charger_find_dev_by_name("primary_chg");
	if (!chg_dfs->charger) {
		xm_err("failed to find primary_chg device\n");
		return -EPROBE_DEFER;
	}

	xm_info("success...\n");

	return 0;
}

static void xm_chg_dfs_remove(struct platform_device *pdev)
{
	return;
}

static const struct of_device_id xm_chg_dfs_of_match[] = {
	{ .compatible = "xiaomi,xm-chg-dfs", },
	{ },
};
MODULE_DEVICE_TABLE(of, xm_chg_dfs_of_match);

static struct platform_driver xm_chg_dfs_driver = {
	.probe = xm_chg_dfs_probe,
	.remove = xm_chg_dfs_remove,
	.driver = {
		.name = "xm_chg_dfs",
		.of_match_table = of_match_ptr(xm_chg_dfs_of_match),
	},
};
module_platform_driver(xm_chg_dfs_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Xiaomi Charge DFS Driver");
MODULE_AUTHOR("pengyuzhe@huaqin.com");
