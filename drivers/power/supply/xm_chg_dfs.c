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
#include <linux/err.h>

#include "mtk_charger.h"
#include "xm_chg_dfs.h"

#define TAG                     "[HQ_CHG_DFS]" // [VENDOR_MODULE_SUBMODULE]
#define xm_err(fmt, ...)        pr_err(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)
#define xm_warn(fmt, ...)       pr_warn(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)
#define xm_notice(fmt, ...)     pr_notice(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)
#define xm_info(fmt, ...)       pr_info(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)
#define xm_debug(fmt, ...)      pr_debug(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)

#define CHG_DFX_EVT_DECLARE(type, pata_count, cnt_limit, period_limit) \
{\
	.dfx_type = CHG_DFX_##type, \
	.dfx_id = DFX_ID_CHG_##type, \
	.para_cnt = pata_count, \
	.report_cnt = 0, \
	.report_cnt_limit = cnt_limit, \
	.report_time = 0, \
	.report_period_limit = period_limit, \
}

static struct xm_chg_dfx_evt dfx_evts[CHG_DFX_MAX_TYPE] = {
	[CHG_DFX_PD_AUTH_FAIL] = CHG_DFX_EVT_DECLARE(PD_AUTH_FAIL, 2, 1, 0),
	[CHG_DFX_CP_EN_FAIL] = CHG_DFX_EVT_DECLARE(CP_EN_FAIL, 2, 3, 0),
	[CHG_DFX_NONE_STANDARD_CHG] = CHG_DFX_EVT_DECLARE(NONE_STANDARD_CHG, 1, 1, 0),
	[CHG_DFX_CORROSION_DISCHARGE] = CHG_DFX_EVT_DECLARE(CORROSION_DISCHARGE, 1, 1, 0),
	[CHG_DFX_LPD_DETECTED] = CHG_DFX_EVT_DECLARE(LPD_DETECTED, 2, 1, 0),
	[CHG_DFX_CP_VBUS_OVP] = CHG_DFX_EVT_DECLARE(CP_VBUS_OVP, 1, 3, 0),
	[CHG_DFX_CP_IBUS_OCP] = CHG_DFX_EVT_DECLARE(CP_IBUS_OCP, 1, 3, 0),
	[CHG_DFX_CP_VBAT_OVP] = CHG_DFX_EVT_DECLARE(CP_VBAT_OVP, 3, 3, 0),
	[CHG_DFX_CP_IBAT_OCP] = CHG_DFX_EVT_DECLARE(CP_IBAT_OCP, 1, 3, 0),
	[CHG_DFX_CP_VAC_OVP] = CHG_DFX_EVT_DECLARE(CP_VAC_OVP, 1, 3, 0),
	[CHG_DFX_ANTI_BURN_TRIG] = CHG_DFX_EVT_DECLARE(ANTI_BURN_TRIG, 2, 1, 0),
	[CHG_DFX_CHG_BATT_CYCLE] = CHG_DFX_EVT_DECLARE(CHG_BATT_CYCLE, 2, 0, 0),
	[CHG_DFX_SOC_NOT_FULL] = CHG_DFX_EVT_DECLARE(SOC_NOT_FULL, 4, 1, 0),
	[CHG_DFX_SMART_ENDURA_TRIG] = CHG_DFX_EVT_DECLARE(SMART_ENDURA_TRIG, 2, 1, 0),
	[CHG_DFX_SMART_NAVI_TRIG] = CHG_DFX_EVT_DECLARE(SMART_NAVI_TRIG, 2, 1, 0),
	[CHG_DFX_FG_I2C_ERR] = CHG_DFX_EVT_DECLARE(FG_I2C_ERR, 2, 0, REPORT_PERIOD_600S),
	[CHG_DFX_CP_I2C_ERR] = CHG_DFX_EVT_DECLARE(CP_I2C_ERR, 3, 0, REPORT_PERIOD_600S),
	[CHG_DFX_BATT_LINKER_ABSENT] = CHG_DFX_EVT_DECLARE(BATT_LINKER_ABSENT, 1, 1, 0),
	[CHG_DFX_CP_TDIE_HOT] = CHG_DFX_EVT_DECLARE(CP_TDIE_HOT, 2, 1, 0), /* NOTE: if slave cp exist, set para_cnt to 3 */
	[CHG_DFX_VBUS_UVLO] = CHG_DFX_EVT_DECLARE(VBUS_UVLO, 4, 1, 0),
	[CHG_DFX_NOT_CHG_IN_LOW_TEMP] = CHG_DFX_EVT_DECLARE(NOT_CHG_IN_LOW_TEMP, 2, 3, 0),
	[CHG_DFX_NOT_CHG_IN_HIGH_TEMP] = CHG_DFX_EVT_DECLARE(NOT_CHG_IN_HIGH_TEMP, 2, 3, 0),
	// CHG_DFX_EVT_DECLARE(DUAL_BATT_LINKER_ABSENT, 2, 3, 0),
	[CHG_DFX_VBAT_SOC_NOT_MATCH] = CHG_DFX_EVT_DECLARE(VBAT_SOC_NOT_MATCH, 4, 3, 0),
	[CHG_DFX_SMART_ENDURA_SOC_ERR] = CHG_DFX_EVT_DECLARE(SMART_ENDURA_SOC_ERR, 2, 1, 0),
	[CHG_DFX_SMART_NAVI_SOC_ERR] = CHG_DFX_EVT_DECLARE(SMART_NAVI_SOC_ERR, 2, 1, 0),
	[CHG_DFX_BATT_AUTH_FAIL] = CHG_DFX_EVT_DECLARE(BATT_AUTH_FAIL, 1, 1, 0),
	// [CHG_DFX_CHG_BATT_AUTH_FAIL]CHG_DFX_EVT_DECLARE(CHG_BATT_AUTH_FAIL, 2, 1, 0),
	[CHG_DFX_TBAT_HOT] = CHG_DFX_EVT_DECLARE(TBAT_HOT, 5, 0, REPORT_PERIOD_300S),
	[CHG_DFX_TBAT_COLD] = CHG_DFX_EVT_DECLARE(TBAT_COLD, 5, 0, REPORT_PERIOD_300S),
	[CHG_DFX_ANTI_FAIL] = CHG_DFX_EVT_DECLARE(ANTI_FAIL, 1, 1, 0),
	// [CHG_DFX_WLS_FACT_CHG_FAIL] = CHG_DFX_EVT_DECLARE(WLS_FACT_CHG_FAIL, 1, 1, 0),
	// [CHG_DFX_WLS_Q_LOW] = CHG_DFX_EVT_DECLARE(WLS_Q_LOW, 3, 1, 0),
	// [CHG_DFX_RX_OTP] = CHG_DFX_EVT_DECLARE(RX_OTP, 2, 1, 0),
	// [CHG_DFX_RX_OVP] = CHG_DFX_EVT_DECLARE(RX_OVP, 1, 1, 0),
	// [CHG_DFX_RX_OCP] = CHG_DFX_EVT_DECLARE(RX_OCP, 1, 1, 0),
	// [CHG_DFX_TRX_FOD] = CHG_DFX_EVT_DECLARE(TRX_FOD, 1, 1, 0),
	// [CHG_DFX_TRX_OCP] = CHG_DFX_EVT_DECLARE(TRX_OCP, 1, 1, 0),
	// [CHG_DFX_TRX_UVLO] = CHG_DFX_EVT_DECLARE(TRX_UVLO, 1, 1, 0),
	// [CHG_DFX_TRX_I2C_ERR] = CHG_DFX_EVT_DECLARE(TRX_I2C_ERR, 1, 1, 0),
	// [CHG_DFX_RX_I2C_ERR] = CHG_DFX_EVT_DECLARE(RX_I2C_ERR, 1, 1, 0),
	// [CHG_DFX_DUAL_VBAT_DIFF] = CHG_DFX_EVT_DECLARE(DUAL_VBAT_DIFF, 3, 1, 0),
};

static void mievent_upload(int miev_code, char *miev_param, int para_cnt)
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
				xm_debug("[CHG_DFS] key:%s\n", key);
			} else {
				xm_err("[CHG_DFS] none key\n");
				key = "None";
			}

			value = strsep(&p2, ":");
			if (value) {
				xm_debug("[CHG_DFS] value:%s\n", value);
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

static void xm_chg_dfx_handle_event_report(struct xm_chg_dfs *chg_dfs, struct xm_chg_dfx_evt *dfx_evt)
{
	char para_str[PARAMETER_LEN_MAX] = {0};
	int len = 0;

	if (IS_ERR_OR_NULL(dfx_evt) || IS_ERR_OR_NULL(chg_dfs)) {
		xm_err("dfx event or charge dfs null pointer.\n");
		return;
	}

	switch (dfx_evt->dfx_type) {
	case CHG_DFX_PD_AUTH_FAIL:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "PdAuthFail");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:0x%x", "adapterId", chg_dfs->adapter_svid);
		break;

	case CHG_DFX_CP_EN_FAIL:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpEnFail");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "cpId", 0);
		break;

	case CHG_DFX_NONE_STANDARD_CHG:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "NoneStandartChg");
		break;

	case CHG_DFX_CORROSION_DISCHARGE:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CorrosionDischarge");
		break;

	case CHG_DFX_LPD_DETECTED:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "LpdDetected");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "lpdFlag", chg_dfs->lpd_flag);
		break;

	case CHG_DFX_CP_VBUS_OVP:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpVbusOVP");
		break;

	case CHG_DFX_CP_IBUS_OCP:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpIbusOCP");
		break;

	case CHG_DFX_CP_VBAT_OVP:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpVbatOVP");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "vbat", chg_dfs->vbat);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "vbatMax", chg_dfs->vbat_max);
		break;

	case CHG_DFX_CP_IBAT_OCP:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpIbatOCP");
		break;

	case CHG_DFX_CP_VAC_OVP:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpVacOVP");
		break;

	case CHG_DFX_ANTI_BURN_TRIG:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "AntiBurnTrig");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tconn", chg_dfs->tconn);
		break;

	case CHG_DFX_CHG_BATT_CYCLE:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgStatInfo", "chgBattCycle");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "cycleCnt", chg_dfs->cycle_cnt);
		break;

	case CHG_DFX_SOC_NOT_FULL:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "SocNotFull");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "vbat", chg_dfs->vbat);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "soc", chg_dfs->soc);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "rsoc", chg_dfs->rsoc);
		break;

	case CHG_DFX_SMART_ENDURA_TRIG:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgStatInfo", "SmartEnduraTrig");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "soc", chg_dfs->soc);
		break;

	case CHG_DFX_SMART_NAVI_TRIG:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgStatInfo", "SmartNaviTrig");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "soc", chg_dfs->soc);
		break;

	case CHG_DFX_FG_I2C_ERR:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "fgI2cErr");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "soc", chg_dfs->soc);
		break;

	case CHG_DFX_CP_I2C_ERR:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "CpI2CErr");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d","masterOk", chg_dfs->master_ok);
		//len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d","slaveOk", chg_dfs->slave_ok);;
		break;

	case CHG_DFX_BATT_LINKER_ABSENT:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "BattLinkerAbsent");
		break;

	case CHG_DFX_CP_TDIE_HOT:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgStatInfo", "CpTdieHot");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "masterTdie", chg_dfs->master_tdie);
		// len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "slaveTdie", chg_dfs->slave_tdie);
		break;

	case CHG_DFX_VBUS_UVLO:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "VbusUvlo");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "vbat", chg_dfs->vbat);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "vbus", chg_dfs->vbus);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "aiclTh", chg_dfs->aicl_threshold);
		break;

	case CHG_DFX_NOT_CHG_IN_LOW_TEMP:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "NotChgInLowTemp");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbat", chg_dfs->tbat);
		break;

	case CHG_DFX_NOT_CHG_IN_HIGH_TEMP:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "NotChgInHighTemp");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbat", chg_dfs->tbat);
		break;

	case CHG_DFX_VBAT_SOC_NOT_MATCH:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "VbatSocNotMatch");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "vbat", chg_dfs->vbat);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "soc", chg_dfs->soc);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "cyclecnt", chg_dfs->cycle_cnt);
		break;

	case CHG_DFX_SMART_ENDURA_SOC_ERR:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "SmartEnduraSocErr");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "soc", chg_dfs->soc);
		break;

	case CHG_DFX_SMART_NAVI_SOC_ERR:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "SmartNaviSocErr");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "soc", chg_dfs->soc);
		break;

	case CHG_DFX_BATT_AUTH_FAIL:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "BattAuthFail");
		break;

	case CHG_DFX_TBAT_HOT:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgStatInfo", "TbatHot");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbat", chg_dfs->tbat);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbatMax", (chg_dfs->tbat_max / 10));
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "isCharging", chg_dfs->adapter_plug_in);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tboard", chg_dfs->tboard);
		break;

	case CHG_DFX_TBAT_COLD:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgStatInfo", "TbatCold");
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbat", chg_dfs->tbat);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tbatMin", (chg_dfs->tbat_min / 10));
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "isCharging", chg_dfs->adapter_plug_in);
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), ",%s:%d", "tboard", chg_dfs->tboard);
		break;

	case CHG_DFX_ANTI_FAIL:
		len += scnprintf((para_str+len), (PARAMETER_LEN_MAX-len), "%s:%s", "chgErrInfo", "AntiFail");
		break;

	default:
		xm_err("[HQ_CHG_DFS]: unknown type to report\n");
		return;
	}

	dfx_evt->report_cnt++;
	/* using second is accurately enough */
	dfx_evt->report_time = ktime_get_seconds();

	xm_info("dfx_type = %d, dfx_id = %d, para_cnt = %u, report_cnt = %d, report_time = %llds\n",
		dfx_evt->dfx_type, dfx_evt->dfx_id, dfx_evt->para_cnt, dfx_evt->report_cnt, dfx_evt->report_time);

	mievent_upload(dfx_evt->dfx_id, para_str, dfx_evt->para_cnt);

	return;
}


static int xm_chg_dfx_report_events(struct xm_chg_dfs *chg_dfs)
{
	int evt_index = 0;

	xm_info("evt_report_bits = 0x%lX\n", chg_dfs->evt_report_bits[0]);

	for_each_set_bit(evt_index, chg_dfs->evt_report_bits, CHG_DFX_MAX_TYPE) {

		xm_chg_dfx_handle_event_report(chg_dfs, &dfx_evts[evt_index]);
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

		xm_chg_dfx_report_events(chg_dfs);

		atomic_set(&chg_dfs->run_report, 0);
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
	struct mtk_charger *info = NULL;

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	/* process usb psy changed */
	if (strcmp(psy->desc->name, "usb") == 0) {
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (ret != 0) {
			return NOTIFY_DONE;
		}
		chg_dfs->vbus = pval.intval;

		info = power_supply_get_drvdata(psy);
		if (!info) {
			xm_err("fail to get usb psy driver data\n");
			return NOTIFY_DONE;
		}
		chg_dfs->real_type = info->real_type;

		chg_dfs->tboard = info->board_temp;

		/* pd verify */
		if (!IS_ERR_OR_NULL(info->pd_adapter)) {
			chg_dfs->adapter_svid = info->pd_adapter->adapter_svid;
		} else { /* TODO: remove these after decouple */
			xm_err("pd adapter is null\n");
		}
		chg_dfs->pd_verify_done = info->pd_verify_done;
		chg_dfs->pd_auth_fail = !info->pd_verifed;
	}

	/* process battery psy changed */
	if (strcmp(psy->desc->name, "battery") == 0) {
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
		if (ret != 0) {
			return NOTIFY_DONE;
		}
		chg_dfs->soc = pval.intval;

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TEMP, &pval);
		if (ret != 0) {
			return NOTIFY_DONE;
		}
		chg_dfs->tbat = pval.intval;
		chg_dfs->tbat_max = max(chg_dfs->tbat, chg_dfs->tbat_max);
		chg_dfs->tbat_min = min(chg_dfs->tbat, chg_dfs->tbat_min);

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
		if (ret != 0) {
			return NOTIFY_DONE;
		}
		chg_dfs->cycle_cnt = pval.intval;

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (ret != 0) {
			return NOTIFY_DONE;
		}
		chg_dfs->vbat = pval.intval;
		chg_dfs->vbat_max = max(chg_dfs->vbat, chg_dfs->vbat_max);

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS, &pval);
		if (ret != 0) {
			return NOTIFY_DONE;
		}
		chg_dfs->chg_status = pval.intval;
	}

	/* process cp_standalone psy changed */
	if (strcmp(psy->desc->name, "cp_standalone") == 0) {
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TEMP, &pval);
		if (ret != 0) {
			return NOTIFY_DONE;
		}
		chg_dfs->master_tdie = pval.intval;
	}

	return NOTIFY_DONE;
}

static int chg_dfs_cp_master_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct xm_chg_dfs *chg_dfs = container_of(nb, struct xm_chg_dfs, cp_master_nb);
	//struct chgdev_notify *notify_data = data;

	switch (event) {
	case CHARGER_DEV_NOTIFY_IBATOCP:
		/* chargepump cann't trigger ibat ocp as ibat adc not work */
		// xm_info("cp master ibat ocp triggered\n");
		// chg_dfs->cp_ibat_ocp = true;
		break;

	case CHARGER_DEV_NOTIFY_IBUSOCP:
		xm_info("cp master ibus ocp triggered\n");
		chg_dfs->cp_ibus_ocp = true;
		break;

	case CHARGER_DEV_NOTIFY_BAT_OVP:
		xm_info("cp master vbat ovp triggered\n");
		chg_dfs->cp_vbat_ovp = true;
		break;

	case CHARGER_DEV_NOTIFY_VBUS_OVP:
		xm_info("cp master vbus ovp triggered\n");
		chg_dfs->cp_vbus_ovp = true;
		break;

	case CHARGER_DEV_NOTIFY_VAC_OVP:
		xm_info("cp master vac ovp triggered\n");
		chg_dfs->cp_vac_ovp = true;
		break;

	case CHARGER_DEV_NOTIFY_TDIE_FLT:
		xm_info("cp master tdie fault triggered\n");
		chg_dfs->cp_tdie_flt = true;
		break;

	default:
		xm_info("receive event not support: %lu\n", event);
		return NOTIFY_DONE;
	}

	return NOTIFY_DONE;
}

static int chg_dfs_cm_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct xm_chg_dfs *chg_dfs = container_of(nb, struct xm_chg_dfs, cm_nb);
	int i = 0;
	struct xm_chg_dfx_evt *dfx_evt = NULL;

	switch (event) {
	case CHG_FW_EVT_ADAPTER_PLUGIN:
		xm_info("receive adapter plugin event\n");

		/* clear report count */
		for (i = 0; i < ARRAY_SIZE(dfx_evts); i++) {
			dfx_evt = &dfx_evts[i];

			if ((dfx_evt->dfx_type != CHG_DFX_CHG_BATT_CYCLE) &&
				(dfx_evt->dfx_type != DFX_ID_CHG_FG_I2C_ERR) &&
				(dfx_evt->dfx_type != DFX_ID_CHG_CP_I2C_ERR) &&
				(dfx_evt->dfx_type != DFX_ID_CHG_TBAT_HOT) &&
				(dfx_evt->dfx_type != DFX_ID_CHG_TBAT_COLD)) {
				dfx_evt->report_cnt = 0;
			}
		}

		chg_dfs->check_interval = DFS_CHECK_5S_INTERVAL;
		chg_dfs->adapter_plug_in = 1;
		break;

	case CHG_FW_EVT_ADAPTER_PLUGOUT:
		xm_info("receive adapter plugout event\n");

		chg_dfs->check_interval = DFS_CHECK_10S_INTERVAL;
		chg_dfs->adapter_plug_in = 0;

		/* clear event flags */
		/* CHG_DFX_PD_AUTH_FAIL */
		chg_dfs->pd_auth_fail = 0;
		chg_dfs->pd_verify_done = 0;
		/* CHG_DFX_CP_EN_FAIL */
		chg_dfs->cp_en_fail = 0;
		/* CHG_DFX_CP_IBAT_OCP */
		chg_dfs->cp_ibat_ocp = 0;
		/* CHG_DFX_CP_IBUS_OCP */
		chg_dfs->cp_ibus_ocp = 0;
		/* CHG_DFX_CP_VBUS_OVP */
		chg_dfs->cp_vbus_ovp = 0;
		/* CHG_DFX_CP_VBAT_OVP */
		chg_dfs->cp_vbat_ovp = 0;
		/* CHG_DFX_CP_VAC_OVP */
		chg_dfs->cp_vac_ovp = 0;
		/* CHG_DFX_CP_TDIE_HOT */
		chg_dfs->cp_tdie_flt = 0;

		break;

	case CHG_FW_EVT_CP_EN:
		xm_info("receive cp en event, data = %d\n", *(bool *)data);
		chg_dfs->cp_en_fail = !(*(bool *)data);
		break;

	case CHG_FW_EVT_LPD:
		xm_info("receive lpd event, data = %d\n", *(bool *)data);
		chg_dfs->lpd_flag = (*(bool *)data);
		break;

	case CHG_FW_EVT_TYPEC_BURN:
		xm_info("receive typec burn event, data = %d %d %d\n",
			*((int *)data), *((int *)data + 1), *((int *)data + 2));
		chg_dfs->typec_burn = *((int *)data);
		chg_dfs->tconn = *((int *)data + 1);
		chg_dfs->anti_burn_fail = *((int *)data + 2);
		break;

	case CHG_FW_EVT_FG_I2C_ERR:
		xm_info("receive fg i2c error event, data = %d\n", *(bool *)data);
		chg_dfs->fg_i2c_err = (*(bool *)data);
		break;

	case CHG_FW_EVT_SMART_ENDURA_TRIG:
		xm_info("receive smart endura trigger event, data = %d\n", *(bool *)data);
		chg_dfs->smart_endura_trig = (*(bool *)data);
		break;

	case CHG_FW_EVT_SMART_NAVI_TRIG:
		xm_info("receive smart navi trigger event, data = %d\n", *(bool *)data);
		chg_dfs->smart_navi_trig = (*(bool *)data);
		break;

	case CHG_FW_EVT_CP_IBAT_OCP:
		xm_info("receive cp ibat ocp event\n");
		chg_dfs->cp_ibat_ocp = 1;
		break;

	default:
		xm_info("receive event not support: %lu\n", event);
		break;
	}

	return NOTIFY_DONE;
}

void handle_evt_report_check_work(struct work_struct *work)
{
	struct xm_chg_dfs *chg_dfs = container_of(work, struct xm_chg_dfs, evt_report_check_work.work);
	int i = 0;
	struct xm_chg_dfx_evt *dfx_evt = NULL;
	time64_t time_now;

	mutex_lock(&chg_dfs->report_check_lock);

	time_now = ktime_get_seconds();

	if (IS_ERR_OR_NULL(chg_dfs->cp_master)) {
		chg_dfs->master_ok = 0;
	} else {
		charger_dev_cp_chip_ok(chg_dfs->cp_master, &chg_dfs->master_ok);
	}

	chg_dfs->batt_auth_fail = !fuel_gauge_get_batt_auth(chg_dfs->fuel_gauge);

	chg_dfs->rsoc = fuel_gauge_get_rsoc(chg_dfs->fuel_gauge);

	xm_info("t:[%d %d %d %d] v:[%d %d] soc:[%d %d] chg:[%d %d %d] usb:[%d %d %d %d 0x%x] cp:[%d %d %d %d %d %d %d %d] fg:[%d %d %d]\n",
		chg_dfs->tbat, chg_dfs->tboard, chg_dfs->tconn, chg_dfs->master_tdie,
		chg_dfs->vbat, chg_dfs->vbus,
		chg_dfs->soc, chg_dfs->rsoc,
		chg_dfs->adapter_plug_in, chg_dfs->real_type, chg_dfs->chg_status,
		chg_dfs->pd_auth_fail, chg_dfs->typec_burn, chg_dfs->anti_burn_fail, chg_dfs->lpd_flag, chg_dfs->adapter_svid,
		chg_dfs->cp_ibat_ocp, chg_dfs->cp_ibus_ocp, chg_dfs->cp_vbus_ovp, chg_dfs->cp_vbat_ovp, chg_dfs->cp_vac_ovp,
		chg_dfs->master_ok, chg_dfs->cp_en_fail, chg_dfs->cp_tdie_flt,
		chg_dfs->cycle_cnt, chg_dfs->fg_i2c_err, chg_dfs->batt_auth_fail);

	for (i = 0; i < ARRAY_SIZE(dfx_evts); i++) {
		dfx_evt = &dfx_evts[i];

		switch (dfx_evt->dfx_type) {
		case CHG_DFX_PD_AUTH_FAIL:
			if (chg_dfs->adapter_svid == USB_PD_MI_SVID) {
				if (chg_dfs->pd_auth_fail && chg_dfs->pd_verify_done) {
					if (dfx_evt->report_cnt < dfx_evt->report_cnt_limit) {
						set_bit(CHG_DFX_PD_AUTH_FAIL, chg_dfs->evt_report_bits);
					} else {
						//clear_bit(CHG_DFX_PD_AUTH_FAIL, chg_dfs->evt_report_bits);
					}
				}
			}
			break;

		case CHG_DFX_CP_EN_FAIL:
			if (chg_dfs->cp_en_fail) {
				if (dfx_evt->report_cnt < dfx_evt->report_cnt_limit) {
					set_bit(CHG_DFX_CP_EN_FAIL, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_CP_EN_FAIL, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_NONE_STANDARD_CHG:
			if (chg_dfs->real_type == XMUSB350_TYPE_FLOAT) {
				if (dfx_evt->report_cnt < dfx_evt->report_cnt_limit) {
					set_bit(CHG_DFX_NONE_STANDARD_CHG, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_NONE_STANDARD_CHG, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_CORROSION_DISCHARGE:
			/* TODO: rp short to vbus */
			break;

		case CHG_DFX_LPD_DETECTED:
			if (chg_dfs->lpd_flag) {
				if (dfx_evt->report_cnt < dfx_evt->report_cnt_limit) {
					set_bit(CHG_DFX_LPD_DETECTED, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_LPD_DETECTED, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_CP_VBUS_OVP:
			if (chg_dfs->cp_vbus_ovp) {
				if (dfx_evt->report_cnt < dfx_evt->report_cnt_limit) {
					set_bit(CHG_DFX_CP_VBUS_OVP, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_CP_VBUS_OVP, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_CP_IBUS_OCP:
			if (chg_dfs->cp_ibus_ocp) {
				if (dfx_evt->report_cnt < dfx_evt->report_cnt_limit) {
					set_bit(CHG_DFX_CP_IBUS_OCP, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_CP_IBUS_OCP, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_CP_VBAT_OVP:
			if (chg_dfs->cp_vbat_ovp) {
				if (dfx_evt->report_cnt < dfx_evt->report_cnt_limit) {
					set_bit(CHG_DFX_CP_VBAT_OVP, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_CP_VBAT_OVP, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_CP_IBAT_OCP:
			if (chg_dfs->cp_ibat_ocp) {
				if (dfx_evt->report_cnt < dfx_evt->report_cnt_limit) {
					set_bit(CHG_DFX_CP_IBAT_OCP, chg_dfs->evt_report_bits);
					chg_dfs->cp_ibat_ocp = false;
				} else {
					//clear_bit(CHG_DFX_CP_IBAT_OCP, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_CP_VAC_OVP:
			if (chg_dfs->cp_vac_ovp) {
				if (dfx_evt->report_cnt < dfx_evt->report_cnt_limit) {
					set_bit(CHG_DFX_CP_VAC_OVP, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_CP_VAC_OVP, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_ANTI_BURN_TRIG:
			if (chg_dfs->typec_burn) {
				if (dfx_evt->report_cnt < dfx_evt->report_cnt_limit) {
					set_bit(CHG_DFX_ANTI_BURN_TRIG, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_ANTI_BURN_TRIG, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_CHG_BATT_CYCLE:
			if (chg_dfs->cycle_cnt != chg_dfs->last_cycle_count) {
				if ((chg_dfs->cycle_cnt % 100) == 0) {
					set_bit(CHG_DFX_CHG_BATT_CYCLE, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_CHG_BATT_CYCLE, chg_dfs->evt_report_bits);
				}
				chg_dfs->last_cycle_count = chg_dfs->cycle_cnt;
			}
			break;

		case CHG_DFX_SOC_NOT_FULL:
			if ((chg_dfs->chg_status == POWER_SUPPLY_STATUS_FULL) && (chg_dfs->soc != 100)) {
				if (dfx_evt->report_cnt < dfx_evt->report_cnt_limit) {
					set_bit(CHG_DFX_SOC_NOT_FULL, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_SOC_NOT_FULL, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_SMART_ENDURA_TRIG:
			if (chg_dfs->smart_endura_trig && chg_dfs->adapter_plug_in) {
				if (dfx_evt->report_cnt < dfx_evt->report_cnt_limit) {
					set_bit(CHG_DFX_SMART_ENDURA_TRIG, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_SMART_ENDURA_TRIG, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_SMART_NAVI_TRIG:
			if (chg_dfs->smart_navi_trig && chg_dfs->adapter_plug_in) {
				if (dfx_evt->report_cnt < dfx_evt->report_cnt_limit) {
					set_bit(CHG_DFX_SMART_NAVI_TRIG, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_SMART_NAVI_TRIG, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_FG_I2C_ERR:
			if (chg_dfs->fg_i2c_err) {
				if ((dfx_evt->report_time == 0) ||
					(time_now - dfx_evt->report_time > dfx_evt->report_period_limit)) {
					set_bit(CHG_DFX_FG_I2C_ERR, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_FG_I2C_ERR, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_CP_I2C_ERR:
			if (!chg_dfs->master_ok) {
				if ((dfx_evt->report_time == 0) ||
					(time_now - dfx_evt->report_time > dfx_evt->report_period_limit)) {
					set_bit(CHG_DFX_CP_I2C_ERR, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_CP_I2C_ERR, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_BATT_LINKER_ABSENT:
			if (chg_dfs->batt_absent && chg_dfs->adapter_plug_in) {
				if (dfx_evt->report_cnt < dfx_evt->report_cnt_limit) {
					set_bit(CHG_DFX_BATT_LINKER_ABSENT, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_BATT_LINKER_ABSENT, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_CP_TDIE_HOT:
			if (chg_dfs->cp_tdie_flt && chg_dfs->adapter_plug_in) {
				if (dfx_evt->report_cnt < dfx_evt->report_cnt_limit) {
					set_bit(CHG_DFX_CP_TDIE_HOT, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_CP_TDIE_HOT, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_VBUS_UVLO:
			break;

		case CHG_DFX_NOT_CHG_IN_LOW_TEMP:
			if (chg_dfs->tbat < 50 && chg_dfs->tbat > -100 && chg_dfs->adapter_plug_in) {
				if (chg_dfs->chg_status != POWER_SUPPLY_STATUS_CHARGING &&
					chg_dfs->chg_status != POWER_SUPPLY_STATUS_FULL) {
					if (dfx_evt->report_cnt < dfx_evt->report_cnt_limit) {
						set_bit(CHG_DFX_NOT_CHG_IN_LOW_TEMP, chg_dfs->evt_report_bits);
					}
				} else {
					//clear_bit(CHG_DFX_NOT_CHG_IN_LOW_TEMP, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_NOT_CHG_IN_HIGH_TEMP:
			if (chg_dfs->tbat < 550 && chg_dfs->tbat > 480 && chg_dfs->adapter_plug_in) {
				if (chg_dfs->chg_status != POWER_SUPPLY_STATUS_CHARGING &&
					chg_dfs->chg_status != POWER_SUPPLY_STATUS_FULL) {
					if (dfx_evt->report_cnt < dfx_evt->report_cnt_limit) {
						set_bit(CHG_DFX_NOT_CHG_IN_HIGH_TEMP, chg_dfs->evt_report_bits);
					}
				} else {
					//clear_bit(CHG_DFX_NOT_CHG_IN_HIGH_TEMP, chg_dfs->evt_report_bits);
				}
			}
			break;
		
		case CHG_DFX_VBAT_SOC_NOT_MATCH:
			break;
		case CHG_DFX_SMART_ENDURA_SOC_ERR:
			break;
		case CHG_DFX_SMART_NAVI_SOC_ERR:
			break;
		case CHG_DFX_BATT_AUTH_FAIL:
			if (chg_dfs->batt_auth_fail) {
				if (dfx_evt->report_cnt < dfx_evt->report_cnt_limit) {
					set_bit(CHG_DFX_BATT_AUTH_FAIL, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_BATT_AUTH_FAIL, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_TBAT_HOT:
			if (chg_dfs->tbat > DFX_BAT_HOT_TEMP) {
				if ((dfx_evt->report_time == 0) ||
					(time_now - dfx_evt->report_time > dfx_evt->report_period_limit)) {
					set_bit(CHG_DFX_TBAT_HOT, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_TBAT_HOT, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_TBAT_COLD:
			if (chg_dfs->tbat < DFX_BAT_COLD_TEMP) {
				if ((dfx_evt->report_time == 0) ||
					(time_now - dfx_evt->report_time > dfx_evt->report_period_limit)) {
					set_bit(CHG_DFX_TBAT_COLD, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_TBAT_COLD, chg_dfs->evt_report_bits);
				}
			}
			break;

		case CHG_DFX_ANTI_FAIL:
			if (chg_dfs->typec_burn && chg_dfs->anti_burn_fail && chg_dfs->adapter_plug_in) {
				if (dfx_evt->report_cnt < dfx_evt->report_cnt_limit) {
					set_bit(CHG_DFX_ANTI_FAIL, chg_dfs->evt_report_bits);
				} else {
					//clear_bit(CHG_DFX_ANTI_FAIL, chg_dfs->evt_report_bits);
				}
			}
			break;

		default:
			xm_err("[HQ_CHG_DFS]: unknown type to report, type = %d\n", dfx_evt->dfx_type);
		}
	}

	if (bitmap_weight(chg_dfs->evt_report_bits, CHG_DFX_MAX_TYPE) > 0) {
		xm_info("%d dfx event need report\n", bitmap_weight(chg_dfs->evt_report_bits, CHG_DFX_MAX_TYPE));
		wake_up_report(chg_dfs);
	}

	mutex_unlock(&chg_dfs->report_check_lock);

	schedule_delayed_work(&chg_dfs->evt_report_check_work, msecs_to_jiffies(chg_dfs->check_interval));
}

static int xm_chg_dfs_probe(struct platform_device *pdev)
{
	struct xm_chg_dfs *chg_dfs = NULL;
	int ret = 0;

	chg_dfs = devm_kzalloc(&pdev->dev, sizeof(*chg_dfs), GFP_KERNEL);
	if (!chg_dfs)
		return -ENOMEM;

	platform_set_drvdata(pdev, chg_dfs);

	init_waitqueue_head(&chg_dfs->report_wq);

	chg_dfs->report_thread = kthread_run(xm_chg_dfx_report_thread_fn, chg_dfs,
							"xm_chg_dfx_report_thread");

	chg_dfs->cp_master = get_charger_by_name("cp_master");
	if (!chg_dfs->cp_master) {
		chg_dfs->master_ok = 0;
		// return -EPROBE_DEFER;
	}

	chg_dfs->pd_adapter = get_adapter_by_name("pd_adapter");
	if (!chg_dfs->pd_adapter) {
		return -EPROBE_DEFER;
	}

	chg_dfs->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
	if (!chg_dfs->fuel_gauge) {
		return -EPROBE_DEFER;
	}

	chg_dfs->last_cycle_count = -1;
	chg_dfs->check_interval = DFS_CHECK_10S_INTERVAL;
	chg_dfs->tbat_max = INT_MIN;
	chg_dfs->tbat_min = INT_MAX;
	chg_dfs->tboard = 0;
	chg_dfs->adapter_svid = 0x0;
	chg_dfs->pd_auth_fail = false;
	chg_dfs->batt_auth_fail = false;
	chg_dfs->vbat_max = INT_MIN;

	mutex_init(&chg_dfs->report_check_lock);
	bitmap_zero(chg_dfs->evt_report_bits, CHG_DFX_MAX_TYPE);
	atomic_set(&chg_dfs->run_report, 0);
	INIT_DELAYED_WORK(&chg_dfs->evt_report_check_work, handle_evt_report_check_work);
	schedule_delayed_work(&chg_dfs->evt_report_check_work, msecs_to_jiffies(60000));

	chg_dfs->psy_nb.notifier_call = chg_dfs_psy_notifier_call;
	ret = power_supply_reg_notifier(&chg_dfs->psy_nb);
	if (ret < 0) {
		xm_err("couldn't register psy notifier ret = %d\n", ret);
		return ret;
	}

	/* register cp master charger device notifier if exist */
	if (!IS_ERR_OR_NULL(chg_dfs->cp_master)) {
		chg_dfs->cp_master_nb.notifier_call = chg_dfs_cp_master_notifier_call;
		ret = register_charger_device_notifier(chg_dfs->cp_master, &chg_dfs->cp_master_nb);
		if (ret < 0) {
			xm_err("couldn't register charger device notifier, ret = %d\n", ret);
			return ret;
		}
	}

	// chg_dfs->pd_adapter_nb.notifier_call = chg_dfs_pd_adapter_notifier_call;
	// ret = register_adapter_device_notifier(chg_dfs->pd_adapter, &chg_dfs->pd_adapter_nb);
	// if (ret < 0) {
	// 	xm_err("couldn't register adapter device notifier, ret = %d\n", ret);
	// 	return ret;
	// }

	chg_dfs->cm_nb.notifier_call = chg_dfs_cm_notifier_call;
	ret = mtk_charger_fw_notifier_register(&chg_dfs->cm_nb);
	if (ret < 0) {
		xm_err("couldn't register charger manager notifier, ret = %d\n", ret);
		return ret;
	}

	xm_info("success...\n");

	return 0;
}

static int xm_chg_dfs_remove(struct platform_device *pdev)
{
	struct xm_chg_dfs *chg_dfs = NULL;

	chg_dfs = platform_get_drvdata(pdev);
	if (!chg_dfs)
		return 0;

	mtk_charger_fw_notifier_unregister(&chg_dfs->cm_nb);

	if (!IS_ERR_OR_NULL(chg_dfs->cp_master)) {
		unregister_charger_device_notifier(chg_dfs->cp_master, &chg_dfs->cp_master_nb);
	}

	power_supply_unreg_notifier(&chg_dfs->psy_nb);

	xm_info("success...\n");

	return 0;
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
