#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include "pmic_voter.h"
#include "mtk_charger.h"
#include "adapter_class.h"
#include "pd_cp_manager.h"
#include "bq28z610.h"
#include "mtk_battery.h"

static struct platform_driver pdm_driver;
enum pdm_sm_state {
	PD_PM_STATE_ENTRY,
	PD_PM_STATE_INIT_VBUS,
	PD_PM_STATE_ENABLE_CP,
	PD_PM_STATE_TUNE,
	PD_PM_STATE_EXIT,
};

enum pdm_sm_status {
	PDM_SM_CONTINUE,
	PDM_SM_HOLD,
	PDM_SM_EXIT,
};

struct pdm_dts_config {
	int	fv;
	int	fv_ffc;
	int	max_fcc;
	int	max_vbus;
	int	max_ibus;
	int	fcc_low_hyst;
	int	fcc_high_hyst;
	int	low_tbat;
	int	high_tbat;
	int	high_vbat;
	int	high_soc;
	int	low_fcc;
	int	cv_vbat;
	int	cv_vbat_ffc;
	int	cv_ibat;
	int	min_pdo_vbus;
	int	max_pdo_vbus;
	int	switch1_1_enter;
	int	switch1_1_exit;
	int	switch2_1_enter;
	int	switch2_1_exit;
	int pdm_ibus_gap;
};

struct usbpd_pm {
	struct device *dev;
	struct charger_device *master_dev;
	struct charger_device *charger_dev;
	struct power_supply *batt_psy;
	struct power_supply *usb_psy;
	struct power_supply *bms_psy;

	struct delayed_work main_sm_work;
	struct work_struct psy_change_work;
	struct notifier_block nb;

	spinlock_t psy_change_lock;

	struct pdm_dts_config dts_config;
	struct votable *fcc_votable;
	struct votable *fv_votable;
	struct votable *icl_votable;
	struct timespec64 last_time;

	enum pdm_sm_state state;
	enum pdm_sm_status sm_status;
	enum adapter_event pd_type;
	bool	pdm_active;
	bool	pdm_start;
	bool	psy_change_running;
	bool	master_cp_enable;
	bool	master_cp_bypass;
	bool	switch2_1_enable;
	bool	bypass_support;
	bool	switch_mode;
	bool	ffc_enable;
	bool	charge_full;
	bool	input_suspend;
	bool	typec_burn;
	bool	no_delay;
	bool	pd_verify_done;
	bool	pd_4_1_mode;
	bool	pd_1_1_mode;
	bool	support_4_1_mode;
	bool	suspend_recovery;
	bool	pd_verifed;
	bool	night_charging;
	bool	is_bypss_cv_mode;
	int	master_cp_vbus;
	int	master_cp_vbatt;
	int	mt6375_vbus;
	int	master_cp_ibus;
	int	total_ibus;
	int	jeita_chg_index;
	int	soc;
	int	ibat;
	int	vbat;
	int	target_fcc;
	int	thermal_limit_fcc;
	int	step_chg_fcc;
	int	sw_cv;
	int	vbus_step;
	int	ibus_step;
	int	ibat_step;
	int	vbat_step;
	int	final_step;
	int	request_voltage;
	int	request_current;
	int	entry_vbus;
	int	entry_ibus;
	int	vbus_low_gap;
	int	vbus_high_gap;
	int	tune_vbus_count;
	int	adapter_adjust_count;
	int	enable_cp_count;
	int	taper_count;
	int     bms_slave_connect_error;
	int     adapter_id;

	int	apdo_max_vbus;
	int	apdo_min_vbus;
	int	apdo_max_ibus;
	int	apdo_max_watt;
	int	vbus_control_gpio;
	int	cp_work_mode;
	int	country_version;
	int	bms_i2c_error_count;
	int	ibus_gap;
	int	switch2_1_count;
	int	switch4_1_count;
	int	cv_count;
	struct adapter_power_cap cap;
	/* smart chg */
	struct smart_chg *smart_chg;
	/*lpd*/
	// bool lpd_flag;

	int	cycle_count;
};

static const unsigned char *pm_str[] = {
	"PD_PM_STATE_ENTRY",
	"PD_PM_STATE_INIT_VBUS",
	"PD_PM_STATE_ENABLE_CP",
	"PD_PM_STATE_TUNE",
	"PD_PM_STATE_EXIT",
};

static struct mtk_charger *pinfo = NULL;
static int log_level = 1;

#define pdm_err(fmt, ...)						\
do {									\
	if (log_level >= 0)						\
		printk(KERN_ERR "[XMCHG_PDM] " fmt, ##__VA_ARGS__);	\
} while (0)

#define pdm_info(fmt, ...)						\
do {									\
	if (log_level >= 1)						\
		printk(KERN_ERR "[XMCHG_PDM] " fmt, ##__VA_ARGS__);	\
} while (0)

#define pdm_dbg(fmt, ...)						\
do {									\
	if (log_level >= 2)						\
		printk(KERN_ERR "[XMCHG_PDM] " fmt, ##__VA_ARGS__);	\
} while (0)

#define is_between(left, right, value)				\
		(((left) >= (right) && (left) >= (value)	\
			&& (value) >= (right))			\
		|| ((left) <= (right) && (left) <= (value)	\
			&& (value) <= (right)))

#define cut_cap(value, min, max)	((min > value) ? min : ((value > max) ? max : value))

static bool pdm_check_cp_dev(struct usbpd_pm *pdpm)
{
	if (!pdpm->master_dev)
		pdpm->master_dev = get_charger_by_name("cp_master");

	if (!pdpm->master_dev) {
		pdm_err("failed to get master_dev\n");
		return false;
	}

	if (!pdpm->charger_dev)
		pdpm->charger_dev = get_charger_by_name("primary_chg");

	if (!pdpm->charger_dev) {
		pdm_err("failed to get charger_dev\n");
		return false;
	}

	return true;
}

static bool pdm_check_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->usb_psy)
		pdpm->usb_psy = power_supply_get_by_name("usb");

	if (!pdpm->usb_psy) {
		pdm_err("failed to get usb_psy\n");
		return false;
	} else {
        pinfo = (struct mtk_charger *)power_supply_get_drvdata(pdpm->usb_psy);
	}

	if (!pdpm->bms_psy)
		pdpm->bms_psy = power_supply_get_by_name("bms");

	if (!pdpm->bms_psy) {
		pdm_err("failed to get bms_psy\n");
		return false;
	}

	if (!pdpm->batt_psy)
		pdpm->batt_psy = power_supply_get_by_name("battery");

	if (!pdpm->batt_psy) {
		pdm_err("failed to get batt_psy\n");
		//return false;
	}

	return true;
}

static bool pdm_check_votable(struct usbpd_pm *pdpm)
{
	if (!pdpm->fcc_votable)
		pdpm->fcc_votable = find_votable("CHARGER_FCC");

	if (!pdpm->fcc_votable) {
		pdm_err("failed to get fcc_votable\n");
		return false;
	}

	if (!pdpm->fv_votable)
		pdpm->fv_votable = find_votable("CHARGER_FV");

	if (!pdpm->fv_votable) {
		pdm_err("failed to get fv_votable\n");
		return false;
	}

	if (!pdpm->icl_votable)
		pdpm->icl_votable = find_votable("CHARGER_ICL");

	if (!pdpm->icl_votable) {
		pdm_err("failed to get icl_votable\n");
		return false;
	}
	return true;
}

static bool pdm_evaluate_src_caps(struct usbpd_pm *pdpm)
{
	bool legal_pdo = false;
	int ret = 0, i = 0;
	int temp = 0;

	pdpm->apdo_max_vbus = 0;
	pdpm->apdo_min_vbus = 0;
	pdpm->apdo_max_ibus = 0;
	pdpm->apdo_max_watt = 0;

	ret = adapter_dev_get_cap(pinfo->pd_adapter, MTK_PD, &pdpm->cap);
	usb_get_property(USB_PROP_ADAPTER_ID, &temp);
	pdpm->adapter_id = temp;

  	usb_get_property(USB_PROP_CHARGE_FULL, &temp);
  	pdpm->charge_full = temp;

	for (i = 0; i < pdpm->cap.nr; i++) {
		/*some 3thd pps only 16v or 22v max_voltage, fix 67w can't request this pdos*/
		if (pdpm->cap.type[i] == MTK_PD_APDO && pdpm->cap.max_mv[i] > pdpm->dts_config.max_pdo_vbus) {
			pdpm->cap.max_mv[i] = pdpm->dts_config.max_pdo_vbus;
			pdpm->cap.maxwatt[i] = pdpm->cap.max_mv[i] * pdpm->cap.ma[i];
		}

		pdm_info("dump PDO min_mv=%d, max_mv=%d, ibus=%d, watt=%d, type=%d\n", pdpm->cap.min_mv[i], pdpm->cap.max_mv[i], pdpm->cap.ma[i], pdpm->cap.maxwatt[i]/1000000, pdpm->cap.type[i]);

		if (pdpm->cap.type[i] != MTK_PD_APDO || pdpm->cap.max_mv[i] < pdpm->dts_config.min_pdo_vbus || pdpm->cap.max_mv[i] > pdpm->dts_config.max_pdo_vbus) {
			pdpm->pd_4_1_mode = false;
			pdpm->pd_1_1_mode = false;
			continue;
		}

		/* adapter suport 4_1/2_1 mode */
		if (pdpm->cap.max_mv[i] > MAX_VBUS_90W && pdpm->cap.ma[i] > MIN_IBUS_90W){
			pdpm->apdo_max_vbus = pdpm->cap.max_mv[i];
			pdpm->apdo_min_vbus = pdpm->cap.min_mv[i];
			pdpm->apdo_max_ibus = pdpm->cap.ma[i];
			pdpm->apdo_max_watt = pdpm->cap.maxwatt[i];
			pdpm->pd_4_1_mode = true;
		} else if (pdpm->cap.maxwatt[i] > pdpm->apdo_max_watt) {
			pdpm->apdo_max_vbus = pdpm->cap.max_mv[i];
			pdpm->apdo_min_vbus = pdpm->cap.min_mv[i];
			pdpm->apdo_max_ibus = pdpm->cap.ma[i];
			pdpm->apdo_max_watt = pdpm->cap.maxwatt[i];
			pdpm->pd_4_1_mode = false;
		}

		/* adapter suport bypass mode */
		if (pdpm->cap.min_mv[i] >= 5000 || pdpm->cap.ma[i] <= 1750 ||
			(pdpm->apdo_max_watt/1000000) == 33 || (pdpm->apdo_max_watt/1000000) == 35 ||
			pdpm->adapter_id == 0x741b || pdpm->adapter_id == 0x7a65) {
			pdpm->pd_1_1_mode = false;
		} else {
			pdpm->pd_1_1_mode = true;
		}

		legal_pdo = true;
	}

	pdm_info("pd_type=%d, pd_4_1_mode=%d, pd_1_1_mode=%d\n",
		pdpm->pd_type, pdpm->pd_4_1_mode, pdpm->pd_1_1_mode);

	if (legal_pdo) {
		if (pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
			if (pdpm->apdo_max_ibus > MAX_IBUS_67W)
				pdpm->apdo_max_ibus = MAX_IBUS_67W;

//			ret = adapter_dev_set_cap_xm(pinfo->pd_adapter, MTK_PD_APDO_START, DEFAULT_PDO_VBUS_1S, DEFAULT_PDO_IBUS_1S);
			pdm_info("MAX_PDO = [%d %d %d]\n", pdpm->apdo_max_vbus, pdpm->apdo_max_ibus, pdpm->apdo_max_watt / 1000000);
		} else {
			legal_pdo = false;
			pdm_err("legal_pdo false\n");
		}
	} else {
		for (i = 0; i <= pdpm->cap.nr - 1; i++) {
			if (pdpm->cap.type[i] == MTK_PD_APDO || !is_between(pdpm->dts_config.min_pdo_vbus, pdpm->dts_config.max_pdo_vbus, pdpm->cap.max_mv[i]))
				continue;

			if (pdpm->cap.max_mv[i] == PD2_VBUS && !pdpm->charge_full) {
				ret = adapter_dev_set_cap(pinfo->pd_adapter, MTK_PD_APDO, pdpm->cap.max_mv[i], pdpm->cap.ma[i]);
				pdm_info("MAX_fixed_PDO = [%d %d %d], charge_full:%d\n", pdpm->cap.max_mv[i], pdpm->cap.ma[i], pdpm->cap.maxwatt[i] / 1000000, pdpm->charge_full);
			}
		}
	}
	pdm_info("legal_pdo = %d\n", legal_pdo);
	return legal_pdo;
}

static bool pdm_taper_charge(struct usbpd_pm *pdpm)
{
	int cv_vbat = 0, cv_vote = 0;
	int cv_ibat = 0;

	if (pdpm->cp_work_mode == SC8561_FORWARD_1_1_CHARGER_MODE) {
		cv_ibat = 600;
	} else if (pdpm->cp_work_mode == SC8561_FORWARD_2_1_CHARGER_MODE) {
		cv_ibat = 1000;
	} else {
		cv_ibat = pdpm->dts_config.cv_ibat;
	}
	cv_vote = get_effective_result(pdpm->fv_votable);
	cv_vbat = pdpm->ffc_enable ? pdpm->dts_config.cv_vbat_ffc : pdpm->dts_config.cv_vbat;
	cv_vbat = cv_vbat < (cv_vote - 20) ? cv_vbat : (cv_vote - 20);
	if (pdpm->charge_full)
		return true;

	if (pdpm->vbat > cv_vbat && (-pdpm->ibat) < cv_ibat)
		pdpm->taper_count++;
	else
		pdpm->taper_count = 0;
	pdm_err("pdm_taper_charge cv_vbat=%d, cv_ibat=%d, taper_count=%d\n", cv_vbat, cv_ibat, pdpm->taper_count);
	if (pdpm->taper_count > MAX_TAPER_COUNT)
		return true;
	else
		return false;
}

static void pdm_update_status(struct usbpd_pm *pdpm)
{
	union power_supply_propval val = {0,};
	int temp = 0;

    charger_dev_get_vbus(pdpm->master_dev, &pdpm->master_cp_vbus);
	charger_dev_get_ibus(pdpm->master_dev, &pdpm->master_cp_ibus);
	charger_dev_cp_get_vbatt(pdpm->master_dev, &pdpm->master_cp_vbatt);
	charger_dev_is_enabled(pdpm->master_dev, &pdpm->master_cp_enable);

    charger_dev_get_vbus(pdpm->charger_dev, &pdpm->mt6375_vbus);
	pdpm->total_ibus = pdpm->master_cp_ibus;
	charger_dev_cp_get_bypass_support(pdpm->master_dev, &pdpm->bypass_support);

	power_supply_get_property(pdpm->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	pdpm->ibat = val.intval / 1000;

	power_supply_get_property(pdpm->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	pdpm->vbat = val.intval / 1000;

	power_supply_get_property(pdpm->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	pdpm->soc = val.intval;

	power_supply_get_property(pdpm->bms_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &val);
	pdpm->cycle_count = val.intval;

	usb_get_property(USB_PROP_JEITA_CHG_INDEX, &temp);
	pdpm->jeita_chg_index = temp;

	usb_get_property(USB_PROP_FFC_ENABLE, &temp);
	pdpm->ffc_enable = temp;

	usb_get_property(USB_PROP_CHARGE_FULL, &temp);
	pdpm->charge_full = temp;

	usb_get_property(USB_PROP_SW_CV, &temp);
	pdpm->sw_cv = temp;

	usb_get_property(USB_PROP_INPUT_SUSPEND, &temp);
	pdpm->input_suspend = temp;

	usb_get_property(USB_PROP_TYPEC_BURN, &temp);
	pdpm->typec_burn = temp;

	usb_get_property(USB_PROP_PD_AUTHENTICATION, &temp);
	pdpm->pd_verifed = temp;

	bms_get_property(BMS_PROP_I2C_ERROR_COUNT, &temp);
	pdpm->bms_i2c_error_count = temp;

	bms_get_property(BMS_PROP_BMS_SLAVE_CONNECT_ERROR, &temp);
	pdpm->bms_slave_connect_error = temp;

	pdpm->night_charging = night_charging_get_flag();

	pdpm->step_chg_fcc = get_client_vote(pdpm->fcc_votable, STEP_CHARGE_VOTER);
	pdpm->thermal_limit_fcc = min(get_client_vote(pdpm->fcc_votable, THERMAL_VOTER), pdpm->dts_config.max_fcc);
	pdpm->target_fcc = get_effective_result(pdpm->fcc_votable);
	if (!pdpm->pd_verifed && pdpm->target_fcc > 5800)
		pdpm->target_fcc = 5800;
	pdpm->smart_chg = pinfo->smart_chg;
// #if defined(CONFIG_RUST_DETECTION)
// 	pdpm->lpd_flag = pinfo->lpd_flag;
// #else
// 	pdpm->lpd_flag = false;
// #endif
	pdm_info("mt6375 vbus= %d, master_vbus=%d, master_ibus=%d, master_vbatt=%d, total_ibus=%d, master_enable=%d\n",
		pdpm->mt6375_vbus, pdpm->master_cp_vbus, pdpm->master_cp_ibus,
		pdpm->master_cp_vbatt, pdpm->total_ibus, pdpm->master_cp_enable);
	pdm_info("soc=%d, vbatt=%d, ibatt=%d, jeita_index=%d, vbatt_step=%d, ibatt_step=%d, vbus_step=%d, ibus_step=%d, final_step=%d, target_fcc=%d, sw_cv=%d, charge_full=%d, input_suspend=%d, typec_burn=%d, i2c_error_count=%d, bms_slave_connect_error=%d, cycle_count=%d\n",
		pdpm->soc, pdpm->vbat, pdpm->ibat, pdpm->jeita_chg_index,
		pdpm->vbat_step, pdpm->ibat_step, pdpm->vbus_step, pdpm->ibus_step, pdpm->final_step,
		pdpm->target_fcc, pdpm->sw_cv, pdpm->charge_full, pdpm->input_suspend, pdpm->typec_burn,
		pdpm->bms_i2c_error_count, pdpm->bms_slave_connect_error, pdpm->cycle_count);
	pdm_info("master_cp_bypass=%d, bypass_support=%d, thermal_limit_fcc=%d, night_charging=%d, smart_chg_status=%#X\n",
		pdpm->master_cp_bypass, pdpm->bypass_support, pdpm->thermal_limit_fcc,  pdpm->night_charging, pdpm->smart_chg[0].active_status);
}


static int pdm_mode_switch_state_machine(struct usbpd_pm *pdpm)
{
	int cp_mode = pdpm->cp_work_mode;
	int entry_soc = 10;

	if(pdpm->adapter_id == 0xa565 || pdpm->adapter_id == 0xd561)
		entry_soc = 25;

	if (pdpm->target_fcc < pdpm->dts_config.switch1_1_enter) {
		if (pdpm->pd_1_1_mode == true && pdpm->soc > entry_soc) {
			cp_mode = SC8561_FORWARD_1_1_CHARGER_MODE;
		} else {
			cp_mode = SC8561_FORWARD_2_1_CHARGER_MODE;
		}
	} else if(pdpm->target_fcc < pdpm->dts_config.switch2_1_enter) {
		if ((cp_mode == SC8561_FORWARD_1_1_CHARGER_MODE) &&
				pdpm->target_fcc < pdpm->dts_config.switch1_1_exit) {
		} else {
			cp_mode = SC8561_FORWARD_2_1_CHARGER_MODE;
		}
	} else {
		if ((cp_mode == SC8561_FORWARD_2_1_CHARGER_MODE || cp_mode == SC8561_FORWARD_1_1_CHARGER_MODE) &&
				pdpm->target_fcc < pdpm->dts_config.switch2_1_exit) {
				cp_mode = SC8561_FORWARD_2_1_CHARGER_MODE;
		} else {
			if (pdpm->support_4_1_mode && pdpm->pd_4_1_mode == true) {
				cp_mode = SC8561_FORWARD_4_1_CHARGER_MODE;
			} else {
				cp_mode = SC8561_FORWARD_2_1_CHARGER_MODE;
			}
		}
	}

	pdm_info("%s: fcc=[%d,%d,%d,%d,%d], support=[%d,%d,%d], mode=%d\n", __func__,
		pdpm->target_fcc, pdpm->dts_config.switch1_1_enter, pdpm->dts_config.switch1_1_exit,
		pdpm->dts_config.switch2_1_enter, pdpm->dts_config.switch2_1_exit,
		pdpm->pd_1_1_mode, pdpm->pd_4_1_mode, pdpm->support_4_1_mode, cp_mode);

	return cp_mode;
}

#if 0
static bool pdm_enter_bypass_cv_charge(struct usbpd_pm *pdpm)
{
	int cv_vbat = 0;
	cv_vbat = pdpm->ffc_enable ? pdpm->dts_config.cv_vbat_ffc : pdpm->dts_config.cv_vbat;
	cv_vbat = cv_vbat - 50;

	if (pdpm->vbat > cv_vbat && (-pdpm->ibat) < pdpm->dts_config.switch1_1_enter)
		pdpm->cv_count++;
	else
		pdpm->cv_count= 0;

	if ((pdpm->cv_count >= MAX_TAPER_COUNT) && (pdpm->pd_1_1_mode == true)) {
		pdpm->is_bypss_cv_mode = true;
		pdpm->cv_count = MAX_TAPER_COUNT;
	}

	if ((-pdpm->ibat) >= pdpm->dts_config.switch1_1_exit) {
		pdpm->is_bypss_cv_mode = false;
		pdpm->cv_count = 0;
	}

	pdm_err("%s: bat=[%d,%d,%d,%d], cv_count=%d, is_bypss_cv_mode=%d\n", __func__,
		pdpm->vbat, pdpm->ibat,cv_vbat, pdpm->dts_config.cv_ibat,
		pdpm->cv_count, pdpm->is_bypss_cv_mode);

	return pdpm->is_bypss_cv_mode;
}
#endif

static void pdm_multi_mode_switch(struct usbpd_pm *pdpm)
{
	int res = 0;
	int cp_mode;

	/* check cp mode by target fcc */
	cp_mode = pdm_mode_switch_state_machine(pdpm);

	/* force enter 1_1 mode in cv charge */
	// if (pdm_enter_bypass_cv_charge(pdpm)) {
	// 	cp_mode = SC8561_FORWARD_1_1_CHARGER_MODE;
	// }

	/* switch cp mode if state change */
	if (pdpm->cp_work_mode != cp_mode) {
		pdpm->cp_work_mode = cp_mode;
		pdpm->switch_mode = true;
	} else {
		pdpm->switch_mode = false;
	}

	if (pdpm->cp_work_mode == SC8561_FORWARD_4_1_CHARGER_MODE) {
		res = 4;
	} else if(pdpm->cp_work_mode ==SC8561_FORWARD_2_1_CHARGER_MODE) {
		res = 2;
	} else {
		res = 1;
	}
	pdpm->vbus_low_gap = pdpm->vbat * res * 2/100;
	pdpm->vbus_high_gap = 	pdpm->vbat * res * 10/100;

	pdpm->ibus_gap = pdpm->dts_config.pdm_ibus_gap / res;

	pdpm->entry_vbus = min(min(((pdpm->vbat * res) + pdpm->vbus_low_gap), pdpm->dts_config.max_vbus), pdpm->apdo_max_vbus);
	pdpm->entry_ibus = min(min(((pdpm->target_fcc / res) + pdpm->ibus_gap), pdpm->dts_config.max_ibus), pdpm->apdo_max_ibus);
	pdm_info("%s: switch=%d, res=%d, gap=[%d,%d,%d], entry=[%d,%d] \n", __func__,
		pdpm->switch_mode, res,
		pdpm->vbus_low_gap, pdpm->vbus_high_gap, pdpm->ibus_gap,
		pdpm->entry_vbus, pdpm->entry_ibus);
}

static int pdm_tune_pdo(struct usbpd_pm *pdpm)
{
	int fv = 0, ibus_limit = 0, vbus_limit = 0, request_voltage = 0;
	int request_current = 0, final_step = 0, ret = 0, res = 0, fv_vote = 0;

	if (pdpm->cp_work_mode == SC8561_FORWARD_4_1_CHARGER_MODE) {
		res = 4;
	} else if(pdpm->cp_work_mode ==SC8561_FORWARD_2_1_CHARGER_MODE) {
		res = 2;
	} else {
		res = 1;
	}

	fv_vote = get_effective_result(pdpm->fv_votable);
	if (pdpm->sw_cv >= 4000)
	{
		fv = pdpm->sw_cv;
	}
	else
	{
		fv = pdpm->ffc_enable ? pdpm->dts_config.fv_ffc : ((pdpm->jeita_chg_index == 2 || pdpm->jeita_chg_index == 3)? pdpm->dts_config.fv - 10:pdpm->dts_config.fv);
		fv = fv < fv_vote ? fv : (fv_vote - 9);
	}
	pdm_err("[%s]fv_vote:%d, fv:%d, ffc:%d, final fv:%d\n", __func__, fv_vote, pdpm->dts_config.fv, pdpm->dts_config.fv_ffc, fv);
/*
	if (pdpm->request_voltage > pdpm->master_cp_vbus + pdpm->total_ibus * MAX_CABLE_RESISTANCE / 1000)
	{
		pdpm->request_voltage = pdpm->master_cp_vbus + pdpm->total_ibus * MAX_CABLE_RESISTANCE / 1000;
		pdm_info("request_voltage is too over\n");
	}
*/
	// /* reduce bus current in cv loop */
	// if(pdpm->sw_cv < 4000 && pdpm->vbat > fv - debug_fv_diff)
	// {
	// 	if(pdpm->vabt_gt_cv_count++ > debug_gt_cv_counts)
	// 	{
	// 		pdpm->vabt_gt_cv_count = 0;
	// 		pdpm->target_fcc -= BQ_TAPER_DECREASE_STEP_MA;
	// 		pdm_err("pd set taper fcc to : %d ma", pdpm->target_fcc);
	// 		if(pdpm->fcc_votable)
	// 		{
	// 			if(pdpm->target_fcc >= debug_lower_fcc){
	// 				vote(pdpm->fcc_votable, BQ_TAPER_FCC_VOTER,true, pdpm->target_fcc);
	// 				pdm_err("BQ_TAPER_FCC_VOTER \n");
	// 			}
	// 		}
	// 	}
	// }
	// else
	// {
	// 	pdpm->vabt_gt_cv_count = 0;
	// }

	pdpm->ibat_step = pdpm->vbat_step = pdpm->ibus_step = pdpm->vbus_step = 0;
	ibus_limit = min(min((pdpm->target_fcc / res + pdpm->ibus_gap), pdpm->apdo_max_ibus), pdpm->dts_config.max_ibus);
	if (pdpm->apdo_max_ibus <= 3000)
		ibus_limit = min(ibus_limit, pdpm->apdo_max_ibus - 200);

	vbus_limit = min(pdpm->dts_config.max_vbus, pdpm->apdo_max_vbus);

	if ((-pdpm->ibat) < (pdpm->target_fcc - pdpm->dts_config.fcc_low_hyst)) {
		if (((pdpm->target_fcc - pdpm->dts_config.fcc_low_hyst) - (-pdpm->ibat)) > LARGE_IBAT_DIFF)
			pdpm->ibat_step = LARGE_STEP;
		else if (((pdpm->target_fcc - pdpm->dts_config.fcc_low_hyst) - (-pdpm->ibat)) > MEDIUM_IBAT_DIFF)
			pdpm->ibat_step = MEDIUM_STEP;
		else
			pdpm->ibat_step = SMALL_STEP;
	} else if ((-pdpm->ibat) > (pdpm->target_fcc + pdpm->dts_config.fcc_high_hyst)) {
		if (((-pdpm->ibat) - (pdpm->target_fcc + pdpm->dts_config.fcc_high_hyst)) > LARGE_IBAT_DIFF)
			pdpm->ibat_step = -LARGE_STEP;
		else if (((-pdpm->ibat) - (pdpm->target_fcc + pdpm->dts_config.fcc_high_hyst)) > MEDIUM_IBAT_DIFF)
			pdpm->ibat_step = -MEDIUM_STEP;
		else
			pdpm->ibat_step = -SMALL_STEP;
	} else {
		pdpm->ibat_step = 0;
	}

	if (fv - pdpm->vbat > LARGE_VBAT_DIFF)
		pdpm->vbat_step = LARGE_STEP;
	else if (fv - pdpm->vbat > MEDIUM_VBAT_DIFF)
		pdpm->vbat_step = MEDIUM_STEP;
	else if (fv - pdpm->vbat > 5)
		pdpm->vbat_step = SMALL_STEP;
	else if (fv - pdpm->vbat < -5)
		pdpm->vbat_step = -MEDIUM_STEP;
	else if (fv - pdpm->vbat < 0)
		pdpm->vbat_step = -SMALL_STEP;

	if (ibus_limit - pdpm->total_ibus > LARGE_IBUS_DIFF)
		pdpm->ibus_step = LARGE_STEP;
	else if (ibus_limit - pdpm->total_ibus > MEDIUM_IBUS_DIFF)
		pdpm->ibus_step = MEDIUM_STEP;
	else if (ibus_limit - pdpm->total_ibus > SMALL_IBUS_DIFF)
		pdpm->ibus_step = SMALL_STEP;
	else if (ibus_limit - pdpm->total_ibus < -(SMALL_IBUS_DIFF + 30))
		pdpm->ibus_step = -SMALL_STEP;

	if (vbus_limit - pdpm->master_cp_vbus > LARGE_VBUS_DIFF)
		pdpm->vbus_step = LARGE_STEP;
	else if (vbus_limit - pdpm->master_cp_vbus > MEDIUM_VBUS_DIFF)
		pdpm->vbus_step = MEDIUM_STEP;
	else if (vbus_limit - pdpm->master_cp_vbus > 0)
		pdpm->vbus_step = SMALL_STEP;
	else
		pdpm->vbus_step = -SMALL_STEP;

	final_step = min(min(pdpm->ibat_step, pdpm->vbat_step), min(pdpm->ibus_step, pdpm->vbus_step));
	if (pdpm->step_chg_fcc != pdpm->dts_config.max_fcc || pdpm->sw_cv ) {
		if ((pdpm->final_step == SMALL_STEP && final_step == SMALL_STEP) || (pdpm->final_step == -SMALL_STEP && final_step == -SMALL_STEP))
			final_step = 0;
			pdm_err("tune PDO enter retune final step = %d\n", final_step);
	}
	pdpm->final_step = final_step;

	if (pdpm->final_step) {
		request_voltage = min(pdpm->request_voltage + pdpm->final_step * STEP_MV, vbus_limit);
		request_current = ibus_limit;
		pdm_err("tune PDO vbus=%d, ibus=%d\n", request_voltage, request_current);
		ret = adapter_dev_set_cap_xm(pinfo->pd_adapter, MTK_PD_APDO, request_voltage, request_current);
		if (!ret) {
			msleep(PDM_SM_DELAY_200MS);
			pdpm->request_voltage = request_voltage;
			pdpm->request_current = request_current;
		} else {
			pdm_err("failed to tune PDO\n");
		}
	}

	return ret;
}

static int pdm_check_condition(struct usbpd_pm *pdpm)
{
	int min_thermal_limit_fcc = 0;
	if (pdpm->cp_work_mode == SC8561_FORWARD_1_1_CHARGER_MODE) {
		min_thermal_limit_fcc = MIN_1_1_CHARGE_CURRENT;
	} else if (pdpm->cp_work_mode == SC8561_FORWARD_2_1_CHARGER_MODE) {
		min_thermal_limit_fcc = MIN_2_1_CHARGE_CURRENT;
	} else {
		min_thermal_limit_fcc = MIN_4_1_CHARGE_CURRENT;
	}
	pdm_err("min_thermal_limit_fcc =%d\n", min_thermal_limit_fcc);

	if (pdpm->state == PD_PM_STATE_TUNE && pdm_taper_charge(pdpm)) {
		pdm_err("PDM_SM_EXIT pdm_taper_charge state=%d\n", pdpm->state);
		return PDM_SM_EXIT;
	} else if (pdpm->state == PD_PM_STATE_TUNE && (!pdpm->master_cp_enable)) {
		pdm_err("PDM_SM_HOLD state=%d,cp_enable= %d\n", pdpm->state, pdpm->master_cp_enable);
		charger_dev_cp_dump_register(pdpm->master_dev);
		return PDM_SM_HOLD;
	}else if (pdpm->switch_mode) {
		pdm_err("PDM_SM_HOLD state=%d,switch_mode=%d\n", pdpm->state, pdpm->switch_mode);
		return PDM_SM_HOLD;
	} else if (pdpm->input_suspend || pdpm->typec_burn) {
		pdm_err("PDM_SM_HOLD input_suspend=%d,typec_burn=%d\n", pdpm->input_suspend, pdpm->typec_burn);
		return PDM_SM_HOLD;
	} else if (!is_between(MIN_JEITA_CHG_INDEX, MAX_JEITA_CHG_INDEX, pdpm->jeita_chg_index)) {
		pdm_err("PDM_SM_HOLD for jeita jeita_chg_index=%d\n", pdpm->jeita_chg_index);
		return PDM_SM_HOLD;
	} else if (pdpm->target_fcc < min_thermal_limit_fcc) {
		pdm_err("PDM_SM_HOLD for fcc thermal_limit_fcc=%d\n", pdpm->thermal_limit_fcc);
		return PDM_SM_HOLD;
	} else if (pdpm->state == PD_PM_STATE_ENTRY && pdpm->soc > pdpm->dts_config.high_soc && pdpm->pdm_start) {
		pdm_err("PDM_SM_EXIT state=%d,soc=%d,vbat=%d\n", pdpm->state, pdpm->soc, pdpm->vbat);
		return PDM_SM_EXIT;
	}else if(pdpm->night_charging && pdpm->soc >= 80)
	{
		pdm_err("PDM_SM_HOLD state=%d,soc=%d\n", pdpm->state, pdpm->soc);
		return PDM_SM_HOLD;
	}else if(pdpm->smart_chg[SMART_CHG_NAVIGATION].en_ret && pdpm->soc >= pdpm->smart_chg[SMART_CHG_NAVIGATION].func_val)
	{
		pdm_err("PDM_SM_HOLD Navigation state=%d,func_val=%d, soc=%d\n", pdpm->smart_chg[SMART_CHG_NAVIGATION].en_ret, pdpm->smart_chg[SMART_CHG_NAVIGATION].func_val, pdpm->soc);
		return PDM_SM_HOLD;
	}else if(pdpm->bms_i2c_error_count >= 10)
	{
		pdm_err("i2c_error_count=%d\n", pdpm->bms_i2c_error_count);
		return PDM_SM_EXIT;
	}else if (pdpm->bms_slave_connect_error == 1 ) {
		pdm_err("PDM_SM_EXIT bms_slave_connect_error=%d\n", pdpm->bms_slave_connect_error);
		return PDM_SM_EXIT;
    }
	// else if(pdpm->lpd_flag){
	// 	pdm_err("PDM_SM_EXIT lpd=%d\n", pdpm->lpd_flag);
	// 	return PDM_SM_HOLD;
	// }
	else
	{
		return PDM_SM_CONTINUE;
	}
}

static void pdm_move_sm(struct usbpd_pm *pdpm, enum pdm_sm_state state)
{
	pdm_info("state change:%s -> %s\n", pm_str[pdpm->state], pm_str[state]);
	pdpm->state = state;
	pdpm->no_delay = true;
}

static bool pdm_handle_sm(struct usbpd_pm *pdpm)
{
	int ret = 0, res = 0;
	bool power_path_en = true;
	int req_vbus = 0;
	int req_ibus = 0;

	switch (pdpm->state) {
	case PD_PM_STATE_ENTRY:
		pdpm->tune_vbus_count = 0;
		pdpm->adapter_adjust_count = 0;
		pdpm->enable_cp_count = 0;
		pdpm->taper_count = 0;
		pdpm->final_step = 0;
		pdpm->step_chg_fcc = 0;

		pdpm->sm_status = pdm_check_condition(pdpm);
		if (pdpm->sm_status == PDM_SM_EXIT) {
			pdm_info("PDM_SM_EXIT, don't start sm\n");
			return true;
		} else if (pdpm->sm_status == PDM_SM_HOLD) {
			break;
		} else {
			charger_dev_enable(pdpm->charger_dev, false);
			charger_dev_enable_powerpath(pdpm->charger_dev, false);
			msleep(200);
			charger_dev_is_powerpath_enabled(pdpm->charger_dev, &power_path_en);
			if (power_path_en) {
				charger_dev_enable(pdpm->charger_dev, false);
				charger_dev_enable_powerpath(pdpm->charger_dev, false);
				msleep(200);
			}
			if (pdpm->cp_work_mode == SC8561_FORWARD_4_1_CHARGER_MODE) {
				charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_4_1_CHARGER_MODE);
				charger_dev_cp_device_init(pdpm->master_dev, CP_FORWARD_4_TO_1);
			}else if ( pdpm->cp_work_mode == SC8561_FORWARD_1_1_CHARGER_MODE) {
				charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_1_1_CHARGER_MODE);
				charger_dev_cp_device_init(pdpm->master_dev, SC8561_FORWARD_1_1_CHARGER_MODE);
			}else {
				charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_2_1_CHARGER_MODE);
				charger_dev_cp_device_init(pdpm->master_dev, CP_FORWARD_2_TO_1);
				if(!pdpm->switch2_1_enable)
					pdpm->switch2_1_enable = true;
			}
			charger_dev_cp_enable_adc(pdpm->master_dev, true);
			pdm_info("enter pdpm_state_entry cp_work_mode=%d\n", pdpm->cp_work_mode);
			pdm_move_sm(pdpm, PD_PM_STATE_INIT_VBUS);
		}
		break;
	case PD_PM_STATE_INIT_VBUS:
		pdpm->sm_status = pdm_check_condition(pdpm);
		if (pdpm->sm_status == PDM_SM_EXIT) {
			pdm_info("[XMC_PDM] PDM_STATE_INIT_VBUS taper charge done\n");
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			break;
		} else if (pdpm->sm_status == PDM_SM_HOLD) {
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			break;
		}
/*
		charger_dev_is_powerpath_enabled(pdpm->charger_dev, &power_path_en);
		if(!power_path_en && pdpm->ibat < -200)
		{
			pdm_err("delay for pmic not charging power_path_en=%d\n", power_path_en);
			break;
		}
*/
		if (pdpm->cp_work_mode == SC8561_FORWARD_4_1_CHARGER_MODE)
			res = 4;
		else if(pdpm->cp_work_mode ==SC8561_FORWARD_2_1_CHARGER_MODE)
			res = 2;
		else
			res = 1;
		pdpm->tune_vbus_count++;
		if (pdpm->tune_vbus_count == 1) {
			if (pdpm->mt6375_vbus > 3600000 && gpio_is_valid(pdpm->vbus_control_gpio)) {
				gpio_direction_output(pdpm->vbus_control_gpio, 1);
				gpio_set_value(pdpm->vbus_control_gpio, 1); 
				pdm_err("set gpio value\n");
			}
			pdpm->request_voltage = pdpm->entry_vbus;
			pdpm->request_current = pdpm->entry_ibus;
			if (pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO)
				adapter_dev_set_cap_xm(pinfo->pd_adapter, MTK_PD_APDO_START, pdpm->request_voltage, pdpm->request_current);
			pdm_info("request first PDO = [%d %d]\n", pdpm->request_voltage, pdpm->request_current);
			break;
		}

		if (pdpm->tune_vbus_count >= MAX_VBUS_TUNE_COUNT) {
			pdm_err("failed to tune VBUS to target window, exit PDM\n");
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			break;
		} else if (pdpm->adapter_adjust_count >= MAX_ADAPTER_ADJUST_COUNT) {
			pdm_err("failed to request PDO, exit PDM\n");
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			break;
		}

		if (pdpm->master_cp_vbus <= pdpm->vbat * res + pdpm->vbus_low_gap) {
			pdpm->request_voltage += res * STEP_MV;
		} else if (pdpm->master_cp_vbus >= pdpm->vbat * res + pdpm->vbus_high_gap) {
			pdpm->request_voltage -= res * STEP_MV;
		} else {
			pdm_info("success to tune VBUS to target window\n");
			pdm_move_sm(pdpm, PD_PM_STATE_ENABLE_CP);
			break;
		}

		if (pdpm->master_cp_enable) {
			pdm_info("enable charge pump success\n");
			pdm_move_sm(pdpm, PD_PM_STATE_ENABLE_CP);
		}

		pdpm->request_current = min(min(pdpm->target_fcc / res, pdpm->apdo_max_ibus), pdpm->dts_config.max_ibus);
		pdm_info("init vbus request_voltage=%d reauest_current=%d ININ_VBUS time=%d\n", pdpm->request_voltage, pdpm->request_current, pdpm->tune_vbus_count);
		if (pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO)
			ret = adapter_dev_set_cap_xm(pinfo->pd_adapter, MTK_PD_APDO, pdpm->request_voltage, pdpm->request_current);
		if (ret == MTK_ADAPTER_REJECT) {
			pdpm->adapter_adjust_count++;
			pdm_err("failed to request PDO, try again\n");
			break;
		}
		break;
	case PD_PM_STATE_ENABLE_CP:
		pdpm->sm_status = pdm_check_condition(pdpm);
		if (pdpm->sm_status == PDM_SM_EXIT) {
			pdm_info("[XMC_PDM] PDM_STATE_ENABLE_CP taper charge done\n");
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			break;
		} else if (pdpm->sm_status == PDM_SM_HOLD) {
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			break;
		}

		pdpm->enable_cp_count++;
		if (pdpm->enable_cp_count >= MAX_ENABLE_CP_COUNT) {
			pdm_err("failed to enable charge pump, exit PDM\n");
			charger_dev_cp_dump_register(pdpm->master_dev);
			pdpm->sm_status = PDM_SM_EXIT;
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			break;
		}
		if (!pdpm->master_cp_enable)
			charger_dev_enable(pdpm->master_dev, true);

		// if (pdpm->master_cp_bypass != pdpm->bypass_enable)
		// 	charger_dev_cp_set_mode(pdpm->master_dev, pdpm->cp_work_mode);

		if (pdpm->master_cp_enable) {
			pdm_info("success to enable charge pump\n");
			charger_dev_enable_termination(pdpm->charger_dev, false);
			pdm_move_sm(pdpm, PD_PM_STATE_TUNE);
		} else {
			pdm_err("failed to enable charge pump, master_cp =%d, try again\n", pdpm->master_cp_enable);
			pdpm->tune_vbus_count = 0;
			pdm_move_sm(pdpm, PD_PM_STATE_INIT_VBUS);
			break;
		}
		break;
	case PD_PM_STATE_TUNE:
		pdpm->pdm_start = false;
		pdpm->sm_status = pdm_check_condition(pdpm);
		if (pdpm->sm_status == PDM_SM_EXIT) {
			pdm_err("taper charge done\n");
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
		} else if (pdpm->sm_status == PDM_SM_HOLD) {
			pdm_err("PDM_SM_HOLD\n");
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
		} else {
			ret = pdm_tune_pdo(pdpm);
			if (ret == MTK_ADAPTER_ERROR) {
				pdpm->sm_status = PDM_SM_HOLD;
				pdm_err("MTK_ADAPTER_ERROR\n");
				pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			}
		}
		break;
	case PD_PM_STATE_EXIT:
		pdpm->tune_vbus_count = 0;
		pdpm->adapter_adjust_count = 0;
		pdpm->enable_cp_count = 0;
		pdpm->taper_count = 0;
		pdpm->switch2_1_count = 0;
		pdpm->switch4_1_count = 0;
		pdpm->pdm_start = false;

		charger_dev_enable(pdpm->master_dev, false);
		msleep(100);

		if (pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
			req_vbus = DEFAULT_PDO_VBUS_1S;
			req_ibus = DEFAULT_PDO_IBUS_1S;
			if (pdpm->apdo_min_vbus > req_vbus) {
				req_vbus = pdpm->apdo_min_vbus;
			}
			if (pdpm->apdo_max_ibus < req_ibus) {
				req_ibus = pdpm->apdo_max_ibus;
			}
			pdm_err("exit request vbus=%d ibus=%d\n", req_vbus, req_ibus);
			adapter_dev_set_cap_xm(pinfo->pd_adapter, MTK_PD_APDO, req_vbus, req_ibus);
		}
		msleep(500);
		charger_dev_get_vbus(pdpm->charger_dev, &pdpm->mt6375_vbus);
		pdm_err("pdm exit vbus=%d\n", pdpm->mt6375_vbus);
		if (pdpm->mt6375_vbus < 10000000 && gpio_is_valid(pdpm->vbus_control_gpio) && !pdpm->night_charging)
			gpio_set_value(pdpm->vbus_control_gpio, 0);

		if (!pdpm->input_suspend && !pdpm->night_charging) {
			charger_dev_enable_powerpath(pdpm->charger_dev, true);
			if(pdpm->bms_i2c_error_count >= 3)
				vote(pinfo->icl_votable, ICL_VOTER, true, 500);
			else
				vote(pinfo->icl_votable, ICL_VOTER, true, 3000);
		}
		pdm_err("icl_effective_result = %d\n", get_effective_result(pdpm->icl_votable));
		// msleep(2000);
		// charger_dev_enable_termination(pdpm->charger_dev, true);

		if (pdpm->charge_full) {
			msleep(1000);
			pdm_err("charge_full pdm exit, disable charging\n");
			charger_dev_enable(pdpm->charger_dev, false);
		}else if (pdpm->night_charging || pdpm->smart_chg[SMART_CHG_NAVIGATION].active_status){
			msleep(1000);
			pdm_err("smart_chg pdm exit, disable charging\n");
			charger_dev_enable(pdpm->charger_dev, false);
		}
		if (pdpm->sm_status == PDM_SM_EXIT) {
			pdpm->is_bypss_cv_mode = false;
			if (pdpm->support_4_1_mode) {
				pdpm->cp_work_mode = SC8561_FORWARD_4_1_CHARGER_MODE;
				charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_4_1_CHARGER_MODE);
				charger_dev_cp_device_init(pdpm->master_dev, CP_FORWARD_4_TO_1);
			} else {
				pdpm->cp_work_mode = SC8561_FORWARD_2_1_CHARGER_MODE;
				charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_2_1_CHARGER_MODE);
				charger_dev_cp_device_init(pdpm->master_dev, CP_FORWARD_2_TO_1);
			}

			return true;
		} else if (pdpm->sm_status == PDM_SM_HOLD) {
			pdm_evaluate_src_caps(pdpm);
			pdm_move_sm(pdpm, PD_PM_STATE_ENTRY);
		}

		break;
	default:
		pdm_err("not supportted pdm_sm_state\n");
		break;
	}

	return false;
}

static void pdm_main_sm(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm, main_sm_work.work);
	int internal = PDM_SM_DELAY_500MS;

	pdm_update_status(pdpm);
	pdm_multi_mode_switch(pdpm);

	if (!pdm_handle_sm(pdpm) && pdpm->pdm_active) {
		if (pdpm->no_delay) {
			internal = 0;
			pdpm->no_delay = false;
		} else {
			switch (pdpm->state) {
			case PD_PM_STATE_ENTRY:
			case PD_PM_STATE_EXIT:
			case PD_PM_STATE_INIT_VBUS:
				internal = PDM_SM_DELAY_300MS;
				break;
			case PD_PM_STATE_ENABLE_CP:
				internal = PDM_SM_DELAY_300MS;
				break;
			case PD_PM_STATE_TUNE:
				internal = PDM_SM_DELAY_500MS;
				break;
			default:
				pdm_err("not supportted pdm_sm_state\n");
				break;
			}
		}
		schedule_delayed_work(&pdpm->main_sm_work, msecs_to_jiffies(internal));
	}
}

static void usbpd_pm_disconnect(struct usbpd_pm *pdpm)
{
	cancel_delayed_work_sync(&pdpm->main_sm_work);
	charger_dev_enable(pdpm->master_dev, false);

	if (pdpm->support_4_1_mode) {
		pdpm->cp_work_mode = SC8561_FORWARD_4_1_CHARGER_MODE;
		charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_4_1_CHARGER_MODE);
		charger_dev_cp_device_init(pdpm->master_dev, CP_FORWARD_4_TO_1);
	} else {
		pdpm->cp_work_mode = SC8561_FORWARD_2_1_CHARGER_MODE;
		charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_2_1_CHARGER_MODE);
		charger_dev_cp_device_init(pdpm->master_dev, CP_FORWARD_2_TO_1);
	}

	charger_dev_cp_enable_adc(pdpm->master_dev, false);
	if (!pdpm->pd_4_1_mode && !pdpm->input_suspend) {
		charger_dev_enable_powerpath(pdpm->charger_dev, true);
		charger_dev_set_input_current(pdpm->charger_dev, 3000000);
	}

	pdpm->pd_type = MTK_PD_CONNECT_NONE;
	pdpm->pd_verify_done = false;
	pdpm->switch2_1_enable = false;
	pdpm->tune_vbus_count = 0;
	pdpm->adapter_adjust_count = 0;
	pdpm->enable_cp_count = 0;
	pdpm->apdo_max_vbus = 0;
	pdpm->apdo_max_ibus = 0;
	pdpm->apdo_max_watt = 0;
	pdpm->final_step = 0;
	pdpm->step_chg_fcc = 0;
	pdpm->thermal_limit_fcc = 0;
	pdpm->bms_i2c_error_count = 0;
	pdpm->last_time.tv_sec = 0;
	pdpm->switch2_1_count = 0;
	pdpm->switch4_1_count = 0;
	pdpm->is_bypss_cv_mode = false;
	pdpm->pd_4_1_mode = false;
	pdpm->pd_1_1_mode = false;
	memset(&pdpm->cap, 0, sizeof(struct adapter_power_cap));

	usb_set_property(USB_PROP_APDO_MAX, 0);
	if (gpio_is_valid(pdpm->vbus_control_gpio)) {
		gpio_set_value(pdpm->vbus_control_gpio, 0);
		pdm_err("pdm exit clear gpio set\n");
	}
	pdm_move_sm(pdpm, PD_PM_STATE_ENTRY);
}

static void pdm_psy_change(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm, psy_change_work); 
	int ret = 0, val = 0;
	pdm_err("pdm_psy_change\n");
	ret = usb_get_property(USB_PROP_PD_TYPE, &val);
	if (ret) {
		pdm_err("Failed to read pd type!\n");
		goto out;
	} else {
		pdpm->pd_type = val;
	}

	ret = usb_get_property(USB_PROP_PD_VERIFY_DONE, &val);
	if (ret) {
		pdm_err("Failed to read pd_verify_done!\n");
		goto out;
	} else {
		pdpm->pd_verify_done = !!val;
	}

	ret = usb_get_property(USB_PROP_CP_CHARGE_RECOVERY, &val);
	if (ret) {
                pdm_err("Failed to read suspend_recovery!\n");
                goto out;
        } else {
                pdpm->suspend_recovery = !!val;
        }

	pdm_info("[pd_type pd_verify_process pdm_active suspend_recovery] = [%d %d %d %d]\n", pdpm->pd_type, pdpm->pd_verify_done, pdpm->pdm_active, pdpm->suspend_recovery);

	if ((!pdpm->pdm_active || pdpm->suspend_recovery) && (pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK || pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30 || pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) && pdpm->pd_verify_done) {
		if (pdm_evaluate_src_caps(pdpm)) {
			pdpm->pdm_active = true;
			pdpm->pdm_start= true;

			pdpm->cp_work_mode = SC8561_FORWARD_4_1_CHARGER_MODE;

			pdm_err("set main_sm_work\n");
			pdm_move_sm(pdpm, PD_PM_STATE_ENTRY);
			if(pdpm->suspend_recovery)
				usb_set_property(USB_PROP_CP_CHARGE_RECOVERY, !pdpm->suspend_recovery);
			schedule_delayed_work(&pdpm->main_sm_work, msecs_to_jiffies(1000));
		}
	} else if (pdpm->pdm_active && pdpm->pd_type == MTK_PD_CONNECT_NONE) {
		pdpm->pdm_active = false;
		pdpm->pdm_start = false;
		pdm_info("cancel state machine\n");
		usbpd_pm_disconnect(pdpm);
	}

out:
	pdpm->psy_change_running = false;
}

static int usbpdm_psy_notifier_cb(struct notifier_block *nb, unsigned long event, void *data)
{
	struct usbpd_pm *pdpm = container_of(nb, struct usbpd_pm, nb);
	struct power_supply *psy = data;
	unsigned long flags;

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	spin_lock_irqsave(&pdpm->psy_change_lock, flags);
	if ((strcmp(psy->desc->name, "usb") == 0 || strcmp(psy->desc->name, "primary_chg") == 0) &&
			!pdpm->psy_change_running) {
		pdpm->psy_change_running = true;
		schedule_work(&pdpm->psy_change_work);
	}
	spin_unlock_irqrestore(&pdpm->psy_change_lock, flags);

	return NOTIFY_OK;
}

static ssize_t pdm_show_log_level(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", log_level);
	pdm_info("show log_level = %d\n", log_level);

	return ret;
}

static ssize_t pdm_store_log_level(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	ret = sscanf(buf, "%d", &log_level);
	pdm_info("store log_level = %d\n", log_level);

	return count;
}

static ssize_t pdm_show_request_vbus(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usbpd_pm *pdpm = dev_get_drvdata(dev);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pdpm->request_voltage);

	return ret;
}

static ssize_t pdm_show_request_ibus(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usbpd_pm *pdpm = dev_get_drvdata(dev);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pdpm->request_current);

	return ret;
}

static ssize_t pdm_show_switch1_1_enter(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usbpd_pm *pdpm = dev_get_drvdata(dev);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pdpm->dts_config.switch1_1_enter);
	pdm_info("show switch1_1_enter= %d\n", pdpm->dts_config.switch1_1_enter);

	return ret;
}

static ssize_t pdm_store_switch1_1_enter(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct usbpd_pm *pdpm = dev_get_drvdata(dev);
	int ret = 0;
	int val = 0;

	ret = sscanf(buf, "%d", &val);
	pdpm->dts_config.switch1_1_enter  = val;
	pdm_info("store switch1_1_enter = %d\n", pdpm->dts_config.switch1_1_enter);

	return count;
}

static ssize_t pdm_show_switch1_1_exit(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usbpd_pm *pdpm = dev_get_drvdata(dev);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pdpm->dts_config.switch1_1_exit);
	pdm_info("show switch1_1_exit = %d\n", pdpm->dts_config.switch1_1_exit);

	return ret;
}

static ssize_t pdm_store_switch1_1_exit(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct usbpd_pm *pdpm = dev_get_drvdata(dev);
	int ret = 0;
	int val = 0;

	ret = sscanf(buf, "%d", &val);
	pdpm->dts_config.switch1_1_exit = val;
	pdm_info("store switch1_1_exit = %d\n", pdpm->dts_config.switch1_1_exit);

	return count;
}

static ssize_t pdm_show_switch2_1_enter(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usbpd_pm *pdpm = dev_get_drvdata(dev);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pdpm->dts_config.switch2_1_enter);
	pdm_info("show switch2_1_enter= %d\n", pdpm->dts_config.switch2_1_enter);

	return ret;
}

static ssize_t pdm_store_switch2_1_enter(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct usbpd_pm *pdpm = dev_get_drvdata(dev);
	int ret = 0;
	int val = 0;

	ret = sscanf(buf, "%d", &val);
	pdpm->dts_config.switch2_1_enter  = val;
	pdm_info("store switch2_1_enter = %d\n", pdpm->dts_config.switch2_1_enter);

	return count;
}

static ssize_t pdm_show_switch2_1_exit(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usbpd_pm *pdpm = dev_get_drvdata(dev);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pdpm->dts_config.switch2_1_exit);
	pdm_info("show switch2_1_exit = %d\n", pdpm->dts_config.switch2_1_exit);

	return ret;
}

static ssize_t pdm_store_switch2_1_exit(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct usbpd_pm *pdpm = dev_get_drvdata(dev);
	int ret = 0;
	int val = 0;

	ret = sscanf(buf, "%d", &val);
	pdpm->dts_config.switch2_1_exit = val;
	pdm_info("store switch2_1_exit = %d\n", pdpm->dts_config.switch2_1_exit);

	return count;
}
static DEVICE_ATTR(log_level, S_IRUGO | S_IWUSR, pdm_show_log_level, pdm_store_log_level);
static DEVICE_ATTR(request_vbus, S_IRUGO, pdm_show_request_vbus, NULL);
static DEVICE_ATTR(request_ibus, S_IRUGO, pdm_show_request_ibus, NULL);
static DEVICE_ATTR(switch1_1_enter, S_IRUGO | S_IWUSR, pdm_show_switch1_1_enter, pdm_store_switch1_1_enter);
static DEVICE_ATTR(switch1_1_exit, S_IRUGO | S_IWUSR, pdm_show_switch1_1_exit, pdm_store_switch1_1_exit);
static DEVICE_ATTR(switch2_1_enter, S_IRUGO | S_IWUSR, pdm_show_switch2_1_enter, pdm_store_switch2_1_enter);
static DEVICE_ATTR(switch2_1_exit, S_IRUGO | S_IWUSR, pdm_show_switch2_1_exit, pdm_store_switch2_1_exit);

static struct attribute *pdm_attributes[] = {
	&dev_attr_log_level.attr,
	&dev_attr_request_vbus.attr,
	&dev_attr_request_ibus.attr,
	&dev_attr_switch1_1_enter.attr,
	&dev_attr_switch1_1_exit.attr,
	&dev_attr_switch2_1_enter.attr,
	&dev_attr_switch2_1_exit.attr,
	NULL,
};

static const struct attribute_group pdm_attr_group = {
	.attrs = pdm_attributes,
};

static int pd_policy_parse_dt(struct usbpd_pm *pdpm)
{
	struct device_node *node = pdpm->dev->of_node;
	int rc = 0;

	if (!node) {
		pdm_err("device tree node missing\n");
		return -EINVAL;
	}

	rc = of_property_read_u32(node, "fv_ffc", &pdpm->dts_config.fv_ffc);
	rc = of_property_read_u32(node, "fv", &pdpm->dts_config.fv);
	rc = of_property_read_u32(node, "max_fcc", &pdpm->dts_config.max_fcc);
	rc = of_property_read_u32(node, "max_vbus", &pdpm->dts_config.max_vbus);
	rc = of_property_read_u32(node, "max_ibus", &pdpm->dts_config.max_ibus);
	rc = of_property_read_u32(node, "fcc_low_hyst", &pdpm->dts_config.fcc_low_hyst);
	rc = of_property_read_u32(node, "fcc_high_hyst", &pdpm->dts_config.fcc_high_hyst);
	rc = of_property_read_u32(node, "low_tbat", &pdpm->dts_config.low_tbat);
	rc = of_property_read_u32(node, "high_tbat", &pdpm->dts_config.high_tbat);
	rc = of_property_read_u32(node, "high_vbat", &pdpm->dts_config.high_vbat);
	rc = of_property_read_u32(node, "high_soc", &pdpm->dts_config.high_soc);
	rc = of_property_read_u32(node, "low_fcc", &pdpm->dts_config.low_fcc);
	rc = of_property_read_u32(node, "cv_vbat", &pdpm->dts_config.cv_vbat);
	rc = of_property_read_u32(node, "cv_vbat_ffc", &pdpm->dts_config.cv_vbat_ffc);
	rc = of_property_read_u32(node, "cv_ibat", &pdpm->dts_config.cv_ibat);
	rc = of_property_read_u32(node, "switch1_1_enter", &pdpm->dts_config.switch1_1_enter);
	rc = of_property_read_u32(node, "switch1_1_exit", &pdpm->dts_config.switch1_1_exit);
	rc = of_property_read_u32(node, "switch2_1_enter", &pdpm->dts_config.switch2_1_enter);
	rc = of_property_read_u32(node, "switch2_1_exit", &pdpm->dts_config.switch2_1_exit);
	rc = of_property_read_u32(node, "pdm_ibus_gap", &pdpm->dts_config.pdm_ibus_gap);
	rc = of_property_read_u32(node, "min_pdo_vbus", &pdpm->dts_config.min_pdo_vbus);
	rc = of_property_read_u32(node, "max_pdo_vbus", &pdpm->dts_config.max_pdo_vbus);
	pdpm->support_4_1_mode = of_property_read_bool(node, "support_4_1_mode");

	pdpm->vbus_control_gpio = of_get_named_gpio(node, "mt6375_control_gpio", 0);
	if (!gpio_is_valid(pdpm->vbus_control_gpio))
		pdm_err("failed to parse vbus_control_gpio\n");
	pdm_info("parse config, FV = %d, FV_FFC = %d, FCC = [%d %d %d], MAX_VBUS = %d, MAX_IBUS = %d, CV = [%d %d %d], ENTRY = [%d %d %d %d %d], PDO_GAP = [%d %d %d]\n",
			pdpm->dts_config.fv, pdpm->dts_config.fv_ffc, pdpm->dts_config.max_fcc, pdpm->dts_config.fcc_low_hyst, pdpm->dts_config.fcc_high_hyst,
			pdpm->dts_config.max_vbus, pdpm->dts_config.max_ibus, pdpm->dts_config.cv_vbat, pdpm->dts_config.cv_vbat_ffc, pdpm->dts_config.cv_ibat,
			pdpm->dts_config.low_tbat, pdpm->dts_config.high_tbat, pdpm->dts_config.high_vbat, pdpm->dts_config.high_soc, pdpm->dts_config.low_fcc,
			pdpm->dts_config.min_pdo_vbus, pdpm->dts_config.max_pdo_vbus, pdpm->support_4_1_mode);

	pdm_info("parse config, switch_mode =[%d, %d]\n", pdpm->dts_config.switch1_1_enter, pdpm->dts_config.switch1_1_exit);

	return rc;
}

#if 1
static const struct platform_device_id pdm_id[] = {
	{ "pd_cp_manager", 1},
	{ "pd_cp_manager_gl", 2},
	{},
};
#else
static const struct platform_device_id pdm_id[] = {
	{ "pd_cp_manager", 0},
	{},
};
#endif
MODULE_DEVICE_TABLE(platform, pdm_id);

#if 1
static const struct of_device_id pdm_of_match[] = {
	{ .compatible = "pd_cp_manager", .data = &pdm_id[0],},
	{ .compatible = "pd_cp_manager_gl", .data = &pdm_id[1],},
	{},
};
#else
static const struct of_device_id pdm_of_match[] = {
	{ .compatible = "pd_cp_manager", },
	{},
};
#endif
MODULE_DEVICE_TABLE(of, pdm_of_match);

static int pdm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct usbpd_pm *pdpm;
	const struct of_device_id *of_id;
#if 1
	int project_no = 0;

	project_no = 1;
	pdm_err("%s: project_no=%d\n", __func__, project_no);
#endif
	of_id = of_match_device(pdm_of_match, &pdev->dev);
	pdev->id_entry = of_id->data;
#if 1
	if (pdev->id_entry->driver_data == project_no) {
		pdm_err("%s ++\n", __func__);
	} else {
		pdm_err("pdm_probe driver_data(%lu) and cmdline(%d) not match, don't probe\n", pdev->id_entry->driver_data, project_no);
          	return -ENODEV;
	}
#endif
	pdpm = kzalloc(sizeof(struct usbpd_pm), GFP_KERNEL);
	if (!pdpm)
		return -ENOMEM;

	pdpm->dev = dev;
	pdpm->switch2_1_enable = false;
	pdpm->last_time.tv_sec = 0;
	pdpm->switch2_1_count = 0;
	pdpm->switch4_1_count = 0;
	spin_lock_init(&pdpm->psy_change_lock);
	platform_set_drvdata(pdev, pdpm);

	ret = pd_policy_parse_dt(pdpm);
	if (ret < 0) {
		pdm_err("success use pd_single_cp_manager\n");
		pdm_err("failed to parse DTS\n");
		return ret;
	}

	if (!pdm_check_cp_dev(pdpm)) {
		pdm_err("failed to check charger device\n");
		return -ENODEV;
	}

	if (!pdm_check_psy(pdpm)) {
		pdm_err("failed to check psy\n");
		return -ENODEV;
	}

    if (!pdm_check_votable(pdpm)) {
		pdm_err("failed to check votable\n");
		return -ENODEV;
	}

	if (gpio_is_valid(pdpm->vbus_control_gpio))
		gpio_direction_output(pdpm->vbus_control_gpio, 0);
	INIT_WORK(&pdpm->psy_change_work, pdm_psy_change);
	INIT_DELAYED_WORK(&pdpm->main_sm_work, pdm_main_sm);

	pdpm->nb.notifier_call = usbpdm_psy_notifier_cb;
	power_supply_reg_notifier(&pdpm->nb);

	pdpm->cp_work_mode = SC8561_FORWARD_4_1_CHARGER_MODE;

	ret = sysfs_create_group(&pdpm->dev->kobj, &pdm_attr_group);
	if (ret) {
		pdm_err("failed to register sysfs\n");
		return ret;
	}
	pdm_err("success use pd_single_cp_manager\n");
	pdm_err("PDM probe success\n");
	return ret;
}

static int pdm_remove(struct platform_device *pdev)
{
	struct usbpd_pm *pdpm = platform_get_drvdata(pdev);

	power_supply_unreg_notifier(&pdpm->nb);
	cancel_delayed_work(&pdpm->main_sm_work);
	cancel_work_sync(&pdpm->psy_change_work);

	return 0;
}

static struct platform_driver pdm_driver = {
	.driver = {
		.name = "pd_cp_manager",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(pdm_of_match),
	},
	.probe = pdm_probe,
	.remove = pdm_remove,
	.id_table = pdm_id,
};

module_platform_driver(pdm_driver);
MODULE_AUTHOR("xiezhichang");
MODULE_DESCRIPTION("charge pump manager for PD");
MODULE_LICENSE("GPL");
