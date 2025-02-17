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
	int	medium_tbat;
	int	high_vbat;
	int	high_soc;
	int	low_fcc;
	int	cv_vbat;
	int	cv_vbat_ffc;
	int	cv_ibat;
	int	cv_ibat_warm;
	int	cv_ibat_bypass;
	int	min_pdo_vbus;
	int	max_pdo_vbus;
	int	switch1_1_single_enter;
	int	switch1_1_single_exit;
	int	switch1_1_enter;
	int	switch1_1_exit;
	int	switch2_1_enter;
	int	switch2_1_exit;
	int     pdm_ibus_gap;
};

struct usbpd_pm {
	struct device *dev;
	struct charger_device *master_dev;
	struct charger_device *slave_dev;
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
	bool	psy_change_running;
	bool	master_cp_enable;
	bool	master_cp_bypass;
	bool	slave_cp_enable;
	bool	slave_cp_bypass;
	bool	bypass_support;
	bool	switch_mode;
	bool	ffc_enable;
	bool	charge_full;
	bool	input_suspend;
	bool	typec_burn;
	bool	no_delay;
	bool	pd_soft_reset;
	bool	pd_verify_done;
	bool	pd_4_1_mode;
	bool	pd_1_1_mode;
	bool	support_4_1_mode;
	bool	pd_nobypass;
	bool	suspend_recovery;
	bool	pd_verifed;
	bool	night_charging;
	int	master_cp_vbus;
	int	master_cp_vbatt;
	int	mt6375_vbus;
	int	master_cp_ibus;
	int	slave_cp_ibus;
	int	total_ibus;
	int	jeita_chg_index;
	int	soc;
	int	ibat;
	int	vbat;
	int	tbat;
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
	int	bms_slave_connect_error;
	int     adapter_id;

	int	apdo_max_vbus;
	int	apdo_min_vbus;
	int	apdo_max_ibus;
	int	apdo_max_watt;
	int vbus_control_gpio;
	int cp_work_mode;
	int bms_i2c_error_count;
	int eea_chg_support_flag;
	int	ibus_gap;
	struct adapter_power_cap cap;
	int     cycle_count;
	bool supported_4_1;

	/* smart chg */
	bool smart_soclmt_trig;
	bool smart_pwrboost_trig;
	bool lpd_flag;
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


static int vbus_low_gap_div = 800;
module_param_named(vbus_low_gap_div, vbus_low_gap_div, int, 0600);

static int vbus_high_gap_div = 950;
module_param_named(vbus_high_gap_div, vbus_high_gap_div, int, 0600);

static int ibus_gap_div = 450;
module_param_named(ibus_gap_div, ibus_gap_div, int, 0600);

static int vbus_low_gap_bypass = 200;
module_param_named(vbus_low_gap_bypass, vbus_low_gap_bypass, int, 0600);

static int vbus_high_gap_bypass = 260;
module_param_named(vbus_high_gap_bypass, vbus_high_gap_bypass, int, 0600);

static int ibus_gap_bypass = 400;
module_param_named(ibus_gap_bypass, ibus_gap_bypass, int, 0600);

static int bypass_final_step = -2;
module_param_named(bypass_final_step, bypass_final_step, int, 0600);

static int bypass_enter_fcc_set = 4000;
module_param_named(bypass_enter_fcc_set, bypass_enter_fcc_set, int, 0600);

static int bypass_exit_fcc_set = 6000;
module_param_named(bypass_exit_fcc_set, bypass_exit_fcc_set, int, 0600);

static int force_cp_mode = -1;
module_param_named(force_cp_mode, force_cp_mode, int, 0600);

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

	if (!pdpm->slave_dev)
		pdpm->slave_dev = get_charger_by_name("cp_slave");

	if (!pdpm->slave_dev) {
		pdm_err("failed to get slave_dev\n");
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
		return false;
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
			(pdpm->apdo_max_watt/1000000) <= 50 ||
			pdpm->adapter_id == 0x741b || pdpm->adapter_id == 0x7a65 ||
			pdpm->adapter_id == 0x731a || pdpm->adapter_id == 0xd561 ||
			!pdpm->pd_verifed) {
			pdpm->pd_1_1_mode = false;
		} else {
			pdpm->pd_1_1_mode = true;
		}
		/* adapter suport 4:1 mode */
		if (pdpm->adapter_id == 0x3007) {
			pdpm->pd_4_1_mode = false;
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

			if ( pdpm->cap.max_mv[i] == PD2_VBUS) {
				ret = adapter_dev_set_cap(pinfo->pd_adapter, MTK_PD_APDO, pdpm->cap.max_mv[i], pdpm->cap.ma[i]);
				pdm_info("MAX_fixed_PDO = [%d %d %d]\n", pdpm->cap.max_mv[i], pdpm->cap.ma[i], pdpm->cap.maxwatt[i] / 1000000);
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

	if (pdpm->cp_work_mode == SC8561_FORWARD_1_1_CHARGER_MODE ||
			pdpm->cp_work_mode == SC8561_FORWARD_1_1_CHARGER_MODE_SINGLE) {
		cv_ibat = 600;
	} else if (pdpm->cp_work_mode == SC8561_FORWARD_2_1_CHARGER_MODE) {
		cv_ibat = 1000;
	} else {
		cv_ibat = pdpm->dts_config.cv_ibat;
	}

	//eea 100W PPS with third-party chargers
	if(!pdpm->pd_verifed && pdpm->eea_chg_support_flag)
	{
		cv_ibat = pdpm->dts_config.cv_ibat;
	}

	cv_vote = get_effective_result(pdpm->fv_votable);
	cv_vbat = pdpm->ffc_enable ? pdpm->dts_config.cv_vbat_ffc : pdpm->dts_config.cv_vbat;
	cv_vbat = cv_vbat < cv_vote ? cv_vbat : cv_vote;
	if (pdpm->charge_full)
		return true;
	if(!pdpm->pd_verifed && pdpm->eea_chg_support_flag)
		cv_vbat = cv_vbat - 15;
	else
		cv_vbat = cv_vbat - 10;

	if (pinfo->project_no == DEGAS_GL && pdpm->ffc_enable && pdpm->cycle_count > 400)
		cv_vbat = cv_vbat -5;

	if (pdpm->vbat > cv_vbat && (-pdpm->ibat) < cv_ibat)
		pdpm->taper_count++;
	else
		pdpm->taper_count = 0;

	pdm_err("pdm_taper_charge cv_vbat=%d, cv_ibat=%d, taper_count=%d, soc=%d\n", cv_vbat, cv_ibat, pdpm->taper_count, pdpm->soc);
	if (pdpm->taper_count > MAX_TAPER_COUNT){
		pdm_err("pdm_taper_charge true\n");
		return true;
	}
	else{
		pdm_err("pdm_taper_charge false\n");
		return false;
	}
}

static void pdm_update_status(struct usbpd_pm *pdpm)
{
	union power_supply_propval val = {0,};
	int temp = 0;

	charger_dev_get_vbus(pdpm->master_dev, &pdpm->master_cp_vbus);
	charger_dev_get_ibus(pdpm->master_dev, &pdpm->master_cp_ibus);
	charger_dev_cp_get_vbatt(pdpm->master_dev, &pdpm->master_cp_vbatt);
	charger_dev_get_ibus(pdpm->slave_dev, &pdpm->slave_cp_ibus);
	charger_dev_is_enabled(pdpm->master_dev, &pdpm->master_cp_enable);
	charger_dev_is_enabled(pdpm->slave_dev, &pdpm->slave_cp_enable);
	charger_dev_is_bypass_enabled(pdpm->master_dev, &pdpm->master_cp_bypass);
	charger_dev_is_bypass_enabled(pdpm->slave_dev, &pdpm->slave_cp_bypass);

	charger_dev_get_vbus(pdpm->charger_dev, &pdpm->mt6375_vbus);
	pdpm->total_ibus = pdpm->master_cp_ibus + pdpm->slave_cp_ibus;
	charger_dev_cp_get_bypass_support(pdpm->master_dev, &pdpm->bypass_support);

	power_supply_get_property(pdpm->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	pdpm->ibat = val.intval / 1000;

	power_supply_get_property(pdpm->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	pdpm->vbat = val.intval / 1000;

	power_supply_get_property(pdpm->bms_psy, POWER_SUPPLY_PROP_TEMP, &val);
	pdpm->tbat = val.intval;

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

	usb_get_property(USB_PROP_SUPPORT_EEA_CHARGE, &temp);
	pdpm->eea_chg_support_flag = temp;

	bms_get_property(BMS_PROP_I2C_ERROR_COUNT, &temp);
	pdpm->bms_i2c_error_count = temp;

	bms_get_property(BMS_PROP_BMS_SLAVE_CONNECT_ERROR, &temp);
	pdpm->bms_slave_connect_error = temp;

	pdpm->pd_soft_reset = get_soft_reset_status();

	pdpm->night_charging = night_charging_get_flag();

	pdpm->smart_soclmt_trig = smart_soclmt_get_flag();
	pdpm->smart_pwrboost_trig = smart_pwrboost_get_flag();

	pdpm->step_chg_fcc = get_client_vote(pdpm->fcc_votable, STEP_CHARGE_VOTER);
	pdpm->thermal_limit_fcc = min(get_client_vote(pdpm->fcc_votable, THERMAL_VOTER), pdpm->dts_config.max_fcc);
	pdpm->target_fcc = min(get_effective_result(pdpm->fcc_votable), pdpm->dts_config.max_fcc);


	if (!pdpm->pd_verifed) {
		if (pdpm->eea_chg_support_flag == true) {
			if (pdpm->dts_config.max_pdo_vbus == 21000 && pdpm->target_fcc > 20000) {
				pdpm->target_fcc = 19800;
			} else if (pdpm->dts_config.max_pdo_vbus == 11000 && pdpm->target_fcc > 10000) {
				pdpm->target_fcc = 9800;
			}
		} else if (pdpm->smart_pwrboost_trig == true) {
			if (pdpm->dts_config.max_pdo_vbus == 21000 && pdpm->target_fcc > 12000) {
				pdpm->target_fcc = 12000;
			} else if (pdpm->dts_config.max_pdo_vbus == 11000 && pdpm->target_fcc > 10000) {
				pdpm->target_fcc = 9800;
			}
		} else if (pdpm->target_fcc > 6000) {
			pdpm->target_fcc = 6000;
		}
	}

#if defined(CONFIG_RUST_DETECTION)
	if(pinfo->lpd_charging_limit)
		pdpm->lpd_flag = pinfo->lpd_flag;
	else
		pdpm->lpd_flag = false;
#else
	pdpm->lpd_flag = false;
#endif

	pdm_info("mt6375 vbus= %d, master_vbus=%d, master_ibus=%d, master_vbatt=%d, slave_ibus=%d, total_ibus=%d, master_enable=%d, slave_enable=%d, cycle_count=%d\n",
		pdpm->mt6375_vbus, pdpm->master_cp_vbus, pdpm->master_cp_ibus, pdpm->master_cp_vbatt, pdpm->slave_cp_ibus, pdpm->total_ibus,
		pdpm->master_cp_enable, pdpm->slave_cp_enable, pdpm->cycle_count);
	pdm_info("soc=%d, tbat = %d, vbatt=%d, ibatt=%d, jeita_index=%d, vbatt_step=%d, ibatt_step=%d, vbus_step=%d, ibus_step=%d, final_step=%d, target_fcc=%d, sw_cv=%d, charge_full=%d, input_suspend=%d, typec_burn=%d, i2c_error_count=%d, bms_slave_connect_error=%d\n",
		pdpm->soc, pdpm->tbat, pdpm->vbat, pdpm->ibat, pdpm->jeita_chg_index,
		pdpm->vbat_step, pdpm->ibat_step, pdpm->vbus_step, pdpm->ibus_step, pdpm->final_step,
		pdpm->target_fcc, pdpm->sw_cv, pdpm->charge_full, pdpm->input_suspend, pdpm->typec_burn, pdpm->bms_i2c_error_count, pdpm->bms_slave_connect_error);
	pdm_info("master_cp_bypass=%d, slave_cp_bypass=%d, bypass_support=%d, thermal_limit_fcc=%d, smart_soclmt=%d, smart_pwrboost=%d, eea_chg_support_flag=%d\n",
		pdpm->master_cp_bypass, pdpm->slave_cp_bypass, pdpm->bypass_support, pdpm->thermal_limit_fcc, pdpm->smart_soclmt_trig, pdpm->smart_pwrboost_trig, pdpm->eea_chg_support_flag);
}

static int pdm_mode_switch_state_machine(struct usbpd_pm *pdpm)
{
	int cp_mode = pdpm->cp_work_mode;
	int entry_soc = 10;
	int high_soc = pdpm->dts_config.high_soc;

	if (force_cp_mode != -1) {
		switch(force_cp_mode) {
		case SC8561_FORWARD_4_1_CHARGER_MODE:
			cp_mode = SC8561_FORWARD_4_1_CHARGER_MODE;
			break;
		case SC8561_FORWARD_2_1_CHARGER_MODE:
			cp_mode = SC8561_FORWARD_2_1_CHARGER_MODE;
			break;
		case SC8561_FORWARD_1_1_CHARGER_MODE:
			cp_mode = SC8561_FORWARD_1_1_CHARGER_MODE;
			break;
		case SC8561_FORWARD_1_1_CHARGER_MODE_SINGLE:
			cp_mode = SC8561_FORWARD_1_1_CHARGER_MODE_SINGLE;
			break;
		default:
			/* do nothing */
			break;
		}
		pdm_err("force_cp_mode=%d, cp_mode=%d\n", force_cp_mode, cp_mode);
		return cp_mode;
	}

	if(pdpm->adapter_id == 0xa565 || pdpm->adapter_id == 0xd561)
		entry_soc = 25;

	if (is_between(100, 200, pdpm->cycle_count)) {
		high_soc = 92;
	} else if (pdpm->cycle_count > 200) {
		high_soc = 90;
	} else {
		high_soc = pdpm->dts_config.high_soc;
	}

	if (pdpm->soc > high_soc) {
		pdm_err("high_soc donot switch mode, soc=%d, mode=%d\n", pdpm->soc, cp_mode);
		return cp_mode;
	}

	if (pdpm->target_fcc < pdpm->dts_config.switch1_1_single_enter) {
		if (pdpm->pd_1_1_mode == true && pdpm->soc > entry_soc) {
			cp_mode = SC8561_FORWARD_1_1_CHARGER_MODE_SINGLE;
		} else {
			cp_mode = SC8561_FORWARD_2_1_CHARGER_MODE;
		}
	} else if (pdpm->target_fcc < pdpm->dts_config.switch1_1_enter) {
		if (pdpm->pd_1_1_mode == true && pdpm->soc > entry_soc) {
			if ((cp_mode == SC8561_FORWARD_1_1_CHARGER_MODE_SINGLE) &&
					pdpm->target_fcc < pdpm->dts_config.switch1_1_single_exit) {
			} else {
				cp_mode = SC8561_FORWARD_1_1_CHARGER_MODE;
			}
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

static void pdm_multi_mode_switch(struct usbpd_pm *pdpm)
{
	int res = 0;
	int cp_mode;

	/* check cp mode by target fcc */
	cp_mode = pdm_mode_switch_state_machine(pdpm);

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
	pdpm->vbus_low_gap = pdpm->vbat * res * 3/100;
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
	int fv = 0, ibus_limit = 0, vbus_limit = 0, request_voltage = 0, request_current = 0, final_step = 0, ret = 0, res = 0, fv_vote = 0;
	int apdo_max_ibus_temp = pdpm->apdo_max_ibus;
	int ibus_limit_thr = 0, max_vbus_gap = 0;

	if (pdpm->cp_work_mode == SC8561_FORWARD_4_1_CHARGER_MODE)
		res = 4;
	else if(pdpm->cp_work_mode ==SC8561_FORWARD_2_1_CHARGER_MODE)
		res = 2;
	else
		res = 1;

	fv_vote = get_effective_result(pdpm->fv_votable);
	if (pdpm->sw_cv >= 4000)
	{
		fv = pdpm->sw_cv;
	}
	else
	{
		fv = pdpm->ffc_enable ? pdpm->dts_config.fv_ffc : pdpm->dts_config.fv;
		if (!pdpm->pd_verifed && pdpm->eea_chg_support_flag)
			fv = fv < fv_vote ? fv : (fv_vote - 10);
		else
			fv = fv < fv_vote ? fv : (fv_vote - 5);
		if (pinfo->project_no == DEGAS_GL && pdpm->ffc_enable && pdpm->cycle_count > 400)
		{
			fv = fv -5;
			pdm_err("%s fv reduce 5mv in 400+ cycle ",__func__);
		}
		if (pinfo->project_no == ROTHKO_CN && pdpm->ffc_enable && pinfo->set_smart_batt_diff_fv > 0)
		{
			fv = fv -5;
			pdm_err("%s fv reduce 5mV in smart_batt", __func__);
		}
	}

	/* aviod request vbus voltage is apdo max vbus cause TA enter cc mode */
	if (pdpm->apdo_max_vbus > 10000)
		max_vbus_gap = 500;
	else
		max_vbus_gap = 300;
	if (pdpm->request_voltage > pdpm->apdo_max_vbus - max_vbus_gap)
		pdpm->request_voltage = pdpm->apdo_max_vbus - max_vbus_gap;

	if(pdpm->cp_work_mode == SC8561_FORWARD_1_1_CHARGER_MODE ||
			pdpm->cp_work_mode == SC8561_FORWARD_1_1_CHARGER_MODE_SINGLE)
		apdo_max_ibus_temp = pdpm->apdo_max_ibus*80/100;

	pdpm->ibat_step = pdpm->vbat_step = pdpm->ibus_step = pdpm->vbus_step = 0;
	ibus_limit = min(min((pdpm->target_fcc / res + pdpm->ibus_gap), apdo_max_ibus_temp), pdpm->dts_config.max_ibus);
	if(pinfo->project_no == DEGAS_GL && !pdpm->pd_verifed && pdpm->eea_chg_support_flag && pdpm->adapter_id == 0 && pdpm->apdo_max_ibus == 5000) {
		ibus_limit = min(ibus_limit, 4000);
		pdm_err("%s keep special 55W eea_adapter_ibus not exceed 4A", __func__);
	}
	if (pdpm->apdo_max_ibus <= 3100)
		ibus_limit = min(ibus_limit, pdpm->apdo_max_ibus - 300);
	else if (pdpm->apdo_max_ibus <= 3250 && !pdpm->pd_verifed)
		ibus_limit = min(ibus_limit, pdpm->apdo_max_ibus - 300);
	vbus_limit = min(pdpm->dts_config.max_vbus, pdpm->apdo_max_vbus);

	ibus_limit_thr = MEDIUM_IBUS_DIFF;

	pdm_err("%s vbus_limit=%d ibus_limit=%d apdo_max_ibus_temp=%d, ibus_limit_thr=%d", __func__, vbus_limit, ibus_limit, apdo_max_ibus_temp, ibus_limit_thr);
	/* ibat loop */
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

	/* vbat loop */
	if (fv - pdpm->vbat > LARGE_VBAT_DIFF)
		pdpm->vbat_step = LARGE_STEP;
	else if (fv - pdpm->vbat > MEDIUM_VBAT_DIFF)
		pdpm->vbat_step = MEDIUM_STEP;
	else if (fv - pdpm->vbat > 5)
		pdpm->vbat_step = SMALL_STEP;
	else if (fv - pdpm->vbat < -5){
		if(pinfo->project_no == DEGAS_GL){
			pdpm->vbat_step = -(MEDIUM_STEP * 2);
		}else{
			pdpm->vbat_step = -MEDIUM_STEP;
		}
	}
	else if (fv - pdpm->vbat < 0)
		pdpm->vbat_step = -SMALL_STEP;
	/* ibus loop */
	if (ibus_limit - pdpm->total_ibus > LARGE_IBUS_DIFF)
		pdpm->ibus_step = LARGE_STEP;
	else if (ibus_limit - pdpm->total_ibus > MEDIUM_IBUS_DIFF)
		pdpm->ibus_step = MEDIUM_STEP;
	else if (ibus_limit - pdpm->total_ibus > -ibus_limit_thr)
		pdpm->ibus_step = SMALL_STEP;
	else if (ibus_limit - pdpm->total_ibus <= -(ibus_limit_thr+50))
		pdpm->ibus_step = -SMALL_STEP;

	/* vbus loop */
	if (vbus_limit - pdpm->master_cp_vbus > LARGE_VBUS_DIFF)
		pdpm->vbus_step = LARGE_STEP;
	else if (vbus_limit - pdpm->master_cp_vbus > MEDIUM_VBUS_DIFF)
		pdpm->vbus_step = MEDIUM_STEP;
	else if (vbus_limit - pdpm->master_cp_vbus > 0)
		pdpm->vbus_step = SMALL_STEP;
	else
		pdpm->vbus_step = -SMALL_STEP;

	final_step = min(min(pdpm->ibat_step, pdpm->vbat_step), min(pdpm->ibus_step, pdpm->vbus_step));
	if (pdpm->step_chg_fcc != pdpm->dts_config.max_fcc || pdpm->sw_cv) {
		pdm_err("tune PDO enter retune final step = %d\n", final_step);
		if ((pdpm->final_step == SMALL_STEP && final_step == SMALL_STEP) ||
				(pdpm->final_step == MEDIUM_STEP && final_step == MEDIUM_STEP))
			final_step = 0;
	}
	pdpm->final_step = final_step;

	if (pdpm->cp_work_mode == SC8561_FORWARD_1_1_CHARGER_MODE ||
			pdpm->cp_work_mode == SC8561_FORWARD_1_1_CHARGER_MODE_SINGLE)
		cut_cap(pdpm->final_step, bypass_final_step, abs(bypass_final_step));

	if (pdpm->final_step) {
		request_voltage = min(pdpm->request_voltage + pdpm->final_step * STEP_MV, vbus_limit);
		request_current = pdpm->apdo_max_ibus;
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
	static int diff_current_count = 0;
	int min_thermal_limit_fcc = 0;

	if (pdpm->cp_work_mode == SC8561_FORWARD_1_1_CHARGER_MODE_SINGLE) {
		min_thermal_limit_fcc = MIN_1_1_CHARGE_CURRENT;
	} else if (pdpm->cp_work_mode == SC8561_FORWARD_1_1_CHARGER_MODE) {
		min_thermal_limit_fcc = MIN_1_1_CHARGE_CURRENT_DUAL;
	} else if (pdpm->cp_work_mode == SC8561_FORWARD_2_1_CHARGER_MODE) {
		min_thermal_limit_fcc = MIN_2_1_CHARGE_CURRENT_DUAL;
	} else {
		min_thermal_limit_fcc = MIN_4_1_CHARGE_CURRENT_DUAL;
	}
	pdm_err("min_thermal_limit_fcc =%d\n", min_thermal_limit_fcc);

	if (pdpm->slave_cp_ibus - pdpm->master_cp_ibus > 1000)
		diff_current_count++;
	else
		diff_current_count = 0;

  	if (pdpm->pd_soft_reset) {
		pdm_err("PDM_SM_HOLD state = %d, pd_soft_reset=%d\n", pdpm->state, pdpm->pd_soft_reset);
		return PDM_SM_HOLD;
	} else if (pdpm->state == PD_PM_STATE_TUNE && pdm_taper_charge(pdpm)) {
		pdm_err("PDM_SM_EXIT pdm_taper_charge state=%d\n", pdpm->state);
		return PDM_SM_EXIT;
	} else if (pdpm->state == PD_PM_STATE_TUNE && (!pdpm->master_cp_enable || (!pdpm->slave_cp_enable && pdpm->cp_work_mode != SC8561_FORWARD_1_1_CHARGER_MODE_SINGLE))) {
		pdm_err("PDM_SM_HOLD state=%d,cp_enable= %d-%d\n", pdpm->state, pdpm->master_cp_enable, pdpm->slave_cp_enable);
		charger_dev_cp_dump_register(pdpm->master_dev);
		charger_dev_cp_dump_register(pdpm->slave_dev);
		return PDM_SM_HOLD;
	} else if (diff_current_count >= 5) {
          	diff_current_count = 0;
		pdm_err("PDM_SM_HOLD Main battery buckle is disconnected\n");
		return PDM_SM_HOLD;
	} else if (pdpm->switch_mode) {
		pdm_err("PDM_SM_HOLD state=%d,switch_mode=%d\n", pdpm->state, pdpm->switch_mode);
		return PDM_SM_HOLD;
	} else if (pdpm->input_suspend || pdpm->typec_burn) {
		pdm_err("PDM_SM_HOLD input_suspend=%d,typec_burn=%d\n", pdpm->input_suspend, pdpm->typec_burn);
		return PDM_SM_HOLD;
	} else if (!is_between(MIN_JEITA_CHG_INDEX, MAX_JEITA_CHG_INDEX, pdpm->jeita_chg_index)) {
		pdm_err("PDM_SM_HOLD for jeita jeita_chg_index=%d\n", pdpm->jeita_chg_index);
		return PDM_SM_HOLD;
	} else if (pdpm->target_fcc < min_thermal_limit_fcc) {
		pdm_err("PDM_SM_HOLD for fcc target_fcc=%d\n", pdpm->target_fcc);
		return PDM_SM_HOLD;
	} else if ((pdpm->state == PD_PM_STATE_ENTRY) && ((pdpm->soc > pdpm->dts_config.high_soc)
		|| (is_between(100, 200, pdpm->cycle_count) && (pdpm->soc > 92))
		|| ((pdpm->cycle_count > 200) && (pdpm->soc > 90)))) {
		pdm_err("PDM_SM_EXIT state=%d,soc=%d,vbat=%d\n", pdpm->state, pdpm->soc, pdpm->vbat);
		return PDM_SM_EXIT;
	} else if (pdpm->smart_soclmt_trig) {
		pdm_err("PDM_SM_HOLD smart soclmt state=%d,soc=%d\n", pdpm->state, pdpm->soc);
		return PDM_SM_HOLD;
	} else if(pdpm->bms_i2c_error_count >= 10) {
		pdm_err("i2c_error_count=%d\n", pdpm->bms_i2c_error_count);
		return PDM_SM_EXIT;
	}else if (pdpm->bms_slave_connect_error == 1){
		pdm_err("PDM_SM_EXIT bms_slave_connect_error=%d\n", pdpm->bms_slave_connect_error);
		return PDM_SM_EXIT;
	}else if(pdpm->lpd_flag){
		pdm_err("PDM_SM_HOLD lpd=%d\n", pdpm->lpd_flag);
		return PDM_SM_HOLD;
	}else
		return PDM_SM_CONTINUE;
}

static void pdm_move_sm(struct usbpd_pm *pdpm, enum pdm_sm_state state)
{
	pdm_info("state change:%s -> %s\n", pm_str[pdpm->state], pm_str[state]);
	pdpm->state = state;
	pdpm->no_delay = true;
}

static bool pdm_handle_sm(struct usbpd_pm *pdpm)
{
	int ret = 0, res = 0, temp =0;
	int apdo_max_ibus_temp = pdpm->apdo_max_ibus;
	bool cp_enable_success = false;
	bool power_path_en = true;
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
			adapter_dev_set_cap_xm(pinfo->pd_adapter, MTK_PD_APDO, DEFAULT_PDO_VBUS_1S, DEFAULT_PDO_IBUS_1S);
			return true;
		} else if (pdpm->sm_status == PDM_SM_HOLD) {
			adapter_dev_set_cap_xm(pinfo->pd_adapter, MTK_PD_APDO, DEFAULT_PDO_VBUS_1S, DEFAULT_PDO_IBUS_1S);
			if (pdpm->pd_soft_reset) {
				set_soft_reset_status(false);
				msleep(500);
			}
			break;
		} else {
			usb_set_property(USB_PROP_CP_SM_RUN_STATE, true);
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
				charger_dev_cp_set_mode(pdpm->slave_dev, SC8561_FORWARD_4_1_CHARGER_MODE);
				charger_dev_cp_device_init(pdpm->master_dev, CP_FORWARD_4_TO_1);
				charger_dev_cp_device_init(pdpm->slave_dev, CP_FORWARD_4_TO_1);
			}else if (pdpm->cp_work_mode == SC8561_FORWARD_1_1_CHARGER_MODE ||
					pdpm->cp_work_mode == SC8561_FORWARD_1_1_CHARGER_MODE_SINGLE) {
				charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_1_1_CHARGER_MODE);
				charger_dev_cp_set_mode(pdpm->slave_dev, SC8561_FORWARD_1_1_CHARGER_MODE);
				charger_dev_cp_device_init(pdpm->master_dev, CP_FORWARD_1_TO_1);
				charger_dev_cp_device_init(pdpm->slave_dev, CP_FORWARD_1_TO_1);
			} else {
				charger_dev_cp_set_mode(pdpm->master_dev, SC8561_FORWARD_2_1_CHARGER_MODE);
				charger_dev_cp_set_mode(pdpm->slave_dev, SC8561_FORWARD_2_1_CHARGER_MODE);
				charger_dev_cp_device_init(pdpm->master_dev, CP_FORWARD_2_TO_1);
				charger_dev_cp_device_init(pdpm->slave_dev, CP_FORWARD_2_TO_1);
			}

			charger_dev_cp_enable_adc(pdpm->master_dev, true);
			charger_dev_cp_enable_adc(pdpm->slave_dev, true);
			pdm_move_sm(pdpm, PD_PM_STATE_INIT_VBUS);
			pdm_info("enter pdpm_state_entry cp_work_mode=%d\n", pdpm->cp_work_mode);
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

		if (pdpm->cp_work_mode == SC8561_FORWARD_4_1_CHARGER_MODE)
			res = 4;
		else if(pdpm->cp_work_mode ==SC8561_FORWARD_2_1_CHARGER_MODE)
			res = 2;
		else
			res = 1;

		if (pdpm->cp_work_mode != SC8561_FORWARD_1_1_CHARGER_MODE_SINGLE) {
			cp_enable_success = pdpm->master_cp_enable && pdpm->slave_cp_enable;
		} else {
			cp_enable_success = pdpm->master_cp_enable;
		}

		if (cp_enable_success) {
			pdm_info("enable charge pump success\n");
			pdm_move_sm(pdpm, PD_PM_STATE_ENABLE_CP);
			break;
		}

		pdpm->tune_vbus_count++;
		if (pdpm->tune_vbus_count == 1) {
			if (pdpm->mt6375_vbus > 3600000 && gpio_is_valid(pdpm->vbus_control_gpio)) {
				gpio_direction_output(pdpm->vbus_control_gpio, 1);
				gpio_set_value(pdpm->vbus_control_gpio, 1); 
				pdm_err("set gpio value\n");
			}
			pdpm->request_voltage = pdpm->entry_vbus;
			pdpm->request_current = pdpm->entry_ibus;
			if (pdpm->request_voltage < 3600)
				pdpm->request_voltage = 3600;
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

		pdpm->request_current = min(min(pdpm->target_fcc / res, apdo_max_ibus_temp), pdpm->dts_config.max_ibus);
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
			charger_dev_cp_set_en_fail_status(pdpm->master_dev, true);
			pdpm->sm_status = PDM_SM_EXIT;
			pdm_move_sm(pdpm, PD_PM_STATE_EXIT);
			break;
		}
		if (!pdpm->master_cp_enable)
			charger_dev_enable(pdpm->master_dev, true);

		if (!pdpm->slave_cp_enable && pdpm->cp_work_mode != SC8561_FORWARD_1_1_CHARGER_MODE_SINGLE) {
			msleep(100);
			charger_dev_enable(pdpm->slave_dev, true);
		}

		if (pdpm->cp_work_mode != SC8561_FORWARD_1_1_CHARGER_MODE_SINGLE) {
			cp_enable_success = pdpm->master_cp_enable && pdpm->slave_cp_enable;
		} else {
			cp_enable_success = pdpm->master_cp_enable;
		}

		if (cp_enable_success) {
			pdm_info("success to enable charge pump\n");
			charger_dev_enable_termination(pdpm->charger_dev, false);
			pdm_move_sm(pdpm, PD_PM_STATE_TUNE);
		} else {
			pdm_err("failed to enable charge pump, master_cp =%d, slave_cp=%d, try again\n", pdpm->master_cp_enable, pdpm->slave_cp_enable);
			pdpm->tune_vbus_count = 0;
			pdm_move_sm(pdpm, PD_PM_STATE_INIT_VBUS);
			charger_dev_cp_dump_register(pdpm->master_dev);
			charger_dev_cp_dump_register(pdpm->slave_dev);
			break;
		}
		break;
	case PD_PM_STATE_TUNE:
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
		charger_dev_enable(pdpm->master_dev, false);
		charger_dev_enable(pdpm->slave_dev, false);
		msleep(100);

		if (pdpm->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO)
			adapter_dev_set_cap_xm(pinfo->pd_adapter, MTK_PD_APDO, DEFAULT_PDO_VBUS_1S, DEFAULT_PDO_IBUS_1S);

		msleep(500);
		charger_dev_get_vbus(pdpm->charger_dev, &pdpm->mt6375_vbus);
		pdm_err("pdm exit vbus=%d\n", pdpm->mt6375_vbus);
                charger_dev_set_mivr(pdpm->charger_dev, 4600000);
		if (pdpm->mt6375_vbus < 10000000 && gpio_is_valid(pdpm->vbus_control_gpio))
			gpio_set_value(pdpm->vbus_control_gpio, 0);

		if (!pdpm->input_suspend) {
			charger_dev_enable_powerpath(pdpm->charger_dev, true);
			if(pdpm->bms_i2c_error_count >= 3)
				charger_dev_set_input_current(pdpm->charger_dev, 500000);
			else
				charger_dev_set_input_current(pdpm->charger_dev, 3000000);
		}
		pdm_err("icl_effective_result = %d\n", get_effective_result(pdpm->icl_votable));

		msleep(100);
		charger_dev_enable_termination(pdpm->charger_dev, true);
		usb_set_property(USB_PROP_CP_SM_RUN_STATE, false);
		if (pdpm->charge_full) {
			msleep(1000);
			usb_get_property(USB_PROP_CHARGE_FULL, &temp);
			pdpm->charge_full = temp;
			if(pdpm->charge_full && pdpm->soc == 100){
				pdm_err("charge_full pdm exit, disable charging\n");
				charger_dev_enable(pdpm->charger_dev, false);
			}
		} else if (pdpm->smart_soclmt_trig)
			charger_dev_enable(pdpm->charger_dev, false);

		if (pdpm->sm_status == PDM_SM_EXIT) {
			return true;
		} else if (pdpm->sm_status == PDM_SM_HOLD) {
			if (pdpm->pd_soft_reset) {
				set_soft_reset_status(false);
				msleep(500);
			}
			if (!pdpm->switch_mode) {
				pdm_evaluate_src_caps(pdpm);
			}
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
	charger_dev_enable(pdpm->slave_dev, false);

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
	charger_dev_cp_enable_adc(pdpm->slave_dev, false);
	if (!pdpm->pd_4_1_mode && !pdpm->input_suspend) {
		charger_dev_enable_powerpath(pdpm->charger_dev, true);
		charger_dev_set_input_current(pdpm->charger_dev, 3000000);
	}

	set_soft_reset_status(false);

	pdpm->pd_type = MTK_PD_CONNECT_NONE;
	pdpm->pd_verify_done = false;
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
	pdpm->eea_chg_support_flag = 0;
	pdpm->last_time.tv_sec = 0;
	pdpm->pd_4_1_mode = false;
	pdpm->pd_1_1_mode = false;
	memset(&pdpm->cap, 0, sizeof(struct adapter_power_cap));

	usb_set_property(USB_PROP_APDO_MAX, 0);
	usb_set_property(USB_PROP_CP_SM_RUN_STATE, false);
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
			pdm_err("set main_sm_work\n");
			pdm_move_sm(pdpm, PD_PM_STATE_ENTRY);
			schedule_delayed_work(&pdpm->main_sm_work, msecs_to_jiffies(1000));
		}
	} else if (pdpm->pdm_active && pdpm->pd_type == MTK_PD_CONNECT_NONE) {
		pdpm->pdm_active = false;
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
	if (strcmp(psy->desc->name, "usb") == 0 && !pdpm->psy_change_running) {
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

static DEVICE_ATTR(log_level, S_IRUGO | S_IWUSR, pdm_show_log_level, pdm_store_log_level);
static DEVICE_ATTR(request_vbus, S_IRUGO, pdm_show_request_vbus, NULL);
static DEVICE_ATTR(request_ibus, S_IRUGO, pdm_show_request_ibus, NULL);

static struct attribute *pdm_attributes[] = {
	&dev_attr_log_level.attr,
	&dev_attr_request_vbus.attr,
	&dev_attr_request_ibus.attr,
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
	rc = of_property_read_u32(node, "cv_ibat_bypass", &pdpm->dts_config.cv_ibat_bypass);

	rc = of_property_read_u32(node, "vbus_low_gap_div", &vbus_low_gap_div);
	rc = of_property_read_u32(node, "vbus_high_gap_div", &vbus_high_gap_div);
	rc = of_property_read_u32(node, "min_pdo_vbus", &pdpm->dts_config.min_pdo_vbus);
	rc = of_property_read_u32(node, "max_pdo_vbus", &pdpm->dts_config.max_pdo_vbus);
	pdpm->support_4_1_mode = of_property_read_bool(node, "support_4_1_mode");
	rc = of_property_read_u32(node, "switch1_1_single_enter", &pdpm->dts_config.switch1_1_single_enter);
	rc = of_property_read_u32(node, "switch1_1_single_exit", &pdpm->dts_config.switch1_1_single_exit);
	rc = of_property_read_u32(node, "switch1_1_enter", &pdpm->dts_config.switch1_1_enter);
	rc = of_property_read_u32(node, "switch1_1_exit", &pdpm->dts_config.switch1_1_exit);
	rc = of_property_read_u32(node, "switch2_1_enter", &pdpm->dts_config.switch2_1_enter);
	rc = of_property_read_u32(node, "switch2_1_exit", &pdpm->dts_config.switch2_1_exit);
	rc = of_property_read_u32(node, "pdm_ibus_gap", &pdpm->dts_config.pdm_ibus_gap);

	if (of_property_read_u32(node, "cv_ibat_warm", &pdpm->dts_config.cv_ibat_warm) < 0)
		pdpm->dts_config.cv_ibat_warm = pdpm->dts_config.cv_ibat;

	if (of_property_read_u32(node, "medium_tbat", &pdpm->dts_config.medium_tbat) < 0)
		pdpm->dts_config.medium_tbat = 350;
	pdpm->supported_4_1 =  of_property_read_bool(node, "supported_4_1");
	if (!pdpm->supported_4_1)
		chr_err("failed to parse supported_4_1\n");

	pdpm->vbus_control_gpio = of_get_named_gpio(node, "mt6375_control_gpio", 0);
	if (!gpio_is_valid(pdpm->vbus_control_gpio))
		pdm_err("failed to parse vbus_control_gpio\n");
#ifdef CONFIG_FACTORY_BUILD
	vbus_low_gap_div += 200;
	vbus_high_gap_div += 200;
#endif
	pdm_info("parse config, FV = %d, FV_FFC = %d, FCC = [%d %d %d], MAX_VBUS = %d, MAX_IBUS = %d, CV = [%d %d %d %d], ENTRY = [%d %d %d %d %d], PDO_GAP = [%d %d %d %d]\n",
			pdpm->dts_config.fv, pdpm->dts_config.fv_ffc, pdpm->dts_config.max_fcc, pdpm->dts_config.fcc_low_hyst, pdpm->dts_config.fcc_high_hyst,
			pdpm->dts_config.max_vbus, pdpm->dts_config.max_ibus, pdpm->dts_config.cv_vbat, pdpm->dts_config.cv_vbat_ffc, pdpm->dts_config.cv_ibat, pdpm->dts_config.cv_ibat_warm,
			pdpm->dts_config.low_tbat, pdpm->dts_config.high_tbat, pdpm->dts_config.high_vbat, pdpm->dts_config.high_soc, pdpm->dts_config.low_fcc,
			vbus_low_gap_div, vbus_high_gap_div, pdpm->dts_config.min_pdo_vbus, pdpm->dts_config.max_pdo_vbus);
	pdm_info("parse config, switch_mode =[%d, %d, %d, %d, %d]\n",
			pdpm->supported_4_1, pdpm->dts_config.switch2_1_enter, pdpm->dts_config.switch2_1_exit, pdpm->dts_config.switch1_1_enter, pdpm->dts_config.switch1_1_exit);

	return rc;
}

#if 1
static const struct platform_device_id pdm_id[] = {
	{ "pd_cp_manager", 1},
	{ "pd_cp_manager_gl", 2},
	{},
};
MODULE_DEVICE_TABLE(platform, pdm_id);

static const struct of_device_id pdm_of_match[] = {
	{ .compatible = "pd_cp_manager", .data = &pdm_id[0],},
	{ .compatible = "pd_cp_manager_gl", .data = &pdm_id[1],},
	{},
};
MODULE_DEVICE_TABLE(of, pdm_of_match);
#else
static const struct platform_device_id pdm_id[] = {
	{ "pd_cp_manager", 0},
	{},
};
MODULE_DEVICE_TABLE(platform, pdm_id);

static const struct of_device_id pdm_of_match[] = {
	{ .compatible = "pd_cp_manager", },
	{},
};
MODULE_DEVICE_TABLE(of, pdm_of_match);
#endif

static int pdm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct usbpd_pm *pdpm;
	const struct of_device_id *of_id;

	project_no = 1;
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
	pdpm->last_time.tv_sec = 0;
	spin_lock_init(&pdpm->psy_change_lock);
	platform_set_drvdata(pdev, pdpm);

	ret = pd_policy_parse_dt(pdpm);
	if (ret < 0) {
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
	pdpm->cp_work_mode = SC8561_FORWARD_2_1_CHARGER_MODE;

	ret = sysfs_create_group(&pdpm->dev->kobj, &pdm_attr_group);
	if (ret) {
		pdm_err("failed to register sysfs\n");
		return ret;
	}

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
MODULE_AUTHOR("liujiquan");
MODULE_DESCRIPTION("charge pump manager for PD");
MODULE_LICENSE("GPL");
