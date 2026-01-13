// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2024 Huaqin Technology(Shanghai) Co., Ltd.
 */
#include "mtk_charger.h"
#include "xm_smart_chg.h"
#include "xm_batt_health.h"

#define TAG                     "[HQ_CHG_SMART_CHG]" // [VENDOR_MODULE_SUBMODULE]
#define xm_err(fmt, ...)        pr_err(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)
#define xm_warn(fmt, ...)       pr_warn(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)
#define xm_notice(fmt, ...)     pr_notice(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)
#define xm_info(fmt, ...)       pr_info(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)
#define xm_debug(fmt, ...)      pr_debug(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)

static void mtk_charger_from_psy(struct mtk_charger **info)
{
	struct power_supply *usb_psy = NULL;

	usb_psy = power_supply_get_by_name("usb");
	if (IS_ERR_OR_NULL(usb_psy)) {
		xm_err("psy is_err_or_null\n");
		return;
	}

	*info = power_supply_get_drvdata(usb_psy);
	if (IS_ERR_OR_NULL(*info)) {
		xm_err("mtk charger is_err_or_null\n");
		return;
	}
}

static int smart_chg_navigaition_discharge_func(struct xm_smart_chg *smart_chg)
{
	struct smart_chg_func *this = NULL;
	int soc_threshold = 0;
	int soc = 0;
	bool chg_dev_chgen = false;
	struct mtk_charger *info = NULL;

	mtk_charger_from_psy(&info);
	if(IS_ERR_OR_NULL(info)) {
		xm_err("mtk_charger is_err_or_null\n");
		return PTR_ERR(info);
	}

	charger_dev_is_enabled(info->chg1_dev, &chg_dev_chgen);

	if (IS_ERR_OR_NULL(smart_chg)) {
		return -EFAULT;
	}

	this = &(smart_chg->funcs[SMART_CHG_NAVI_DISCHARGE]);
	soc = smart_chg->soc;
	soc_threshold = this->func_val;
	if ((this->func_on && (soc >= soc_threshold)) ||
		(((soc > soc_threshold - 5) && (soc < soc_threshold)) && this->active_flag)) {
		this->active_flag = true;
		mtk_charger_fw_notifier_call_chain(CHG_FW_EVT_SMART_NAVI_TRIG, &(this->active_flag));

		xm_info("navigation discharge function active\n");
	} else if (((!this->func_on || (soc <= soc_threshold - 5)) && this->active_flag) ||
		(!this->func_on && !this->active_flag)) {
		this->active_flag = false;
		mtk_charger_fw_notifier_call_chain(CHG_FW_EVT_SMART_NAVI_TRIG, &(this->active_flag));

		xm_info("navigation discharge function deactive\n");
	}
	/* update stop charge status for sysfs */
	smart_chg->stop_charge = this->active_flag;
	vote(smart_chg->charger_fcc_votable, NAVIGATION_VOTER, this->active_flag, 0);

	if (this->active_flag) {
		if (chg_dev_chgen)
			charger_dev_enable(info->chg1_dev, false);
		xm_info("navigation discharge function true\n");
	} else {
		if (!chg_dev_chgen && smart_chg->effective_fcc != 0 && ((info->plug_in_soc100_flag != true) && (info->product_name_index != EEA)) && !info->charge_full && info->thermal.sm == BAT_TEMP_NORMAL)
			charger_dev_enable(info->chg1_dev, true);
		xm_info("navigation discharge function false\n");
	}

	xm_info("soc: %d, func[on: %d val: %d active: %d] chg_en: %d\n",
		smart_chg->soc, this->func_on, this->func_val, this->active_flag, chg_dev_chgen);

	return 0;
}

static int smart_chg_outdoor_charge_func(struct xm_smart_chg *smart_chg)
{
	struct smart_chg_func *this = NULL;

	if (IS_ERR_OR_NULL(smart_chg)) {
		return -EFAULT;
	}

	this = &(smart_chg->funcs[SMART_CHG_OUTDOOR_CHARGE]);

	if (this->func_on && (smart_chg->real_type == XMUSB350_TYPE_DCP)) {
		this->active_flag = true;
		vote(smart_chg->main_icl_votable, CHARGERIC_VOTER, true, OUTDOOR_DCP_CURRENT);
		vote(smart_chg->charger_fcc_votable, CHARGERIC_VOTER , true, OUTDOOR_DCP_CURRENT);
		xm_info("outdoor charge function active\n");
	} else {
		this->active_flag = false;
		xm_info("outdoor charge function deactive\n");
	}

	xm_info("real_type: %d, pd_active: %d, func[on: %d val: %d active: %d]\n",
		smart_chg->real_type, smart_chg->pd_type,
		this->func_on, this->func_val, this->active_flag);

	return 0;
}

static int smart_chg_low_battery_fast_charge_func(struct xm_smart_chg *smart_chg)
{
	struct smart_chg_func *this = NULL;
	time64_t time_now = 0, delta_time = 0;
	static time64_t time_last = 0;

	if (IS_ERR_OR_NULL(smart_chg)) {
		return -EFAULT;
	}

	this = &(smart_chg->funcs[SMART_CHG_LOW_BATT_FAST_CHG]);

	if (this->func_on) {
		if ((smart_chg->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) &&
		    (smart_chg->plugin_soc <= 20) &&
		    (smart_chg->plugin_board_temp <= 390) &&
		    (smart_chg->soc <= 40 )) {
				if(((smart_chg->screen_state == SCREEN_STATE_UNKONW) || (smart_chg->screen_state == SCREEN_STATE_BLACK)) && !smart_chg->screen_back_state) {  //black to bright
					smart_chg->screen_state = SCREEN_STATE_BLACK_TO_BRIGHT;
					time_last = ktime_get_seconds();
					this->active_flag = true;
					pr_err("%s switch to bright time_last = %lld\n", __func__, time_last);
				}  else if ((smart_chg->screen_state == SCREEN_STATE_BLACK_TO_BRIGHT || smart_chg->screen_state == SCREEN_STATE_BRIGHT) && !smart_chg->screen_back_state) {  //still bright
					smart_chg->screen_state = SCREEN_STATE_BRIGHT;
					time_now = ktime_get_seconds();
					delta_time = time_now - time_last;
					pr_err("%s still_bright time_now = %lld, time_last = %lld, delta_time = %lld\n", __func__, time_now, time_last, delta_time);
					if(delta_time <= 10) {
						this->active_flag = true;
						pr_err("%s still_bright delta_time = %lld, stay fast\n", __func__, delta_time);
					} else {
						this->active_flag = false;
						pr_err("%s still_bright delta_time = %lld, exit fast\n", __func__, delta_time);
					}
				} else { //black
					smart_chg->screen_state = SCREEN_STATE_BLACK;
					this->active_flag = true;
					pr_err("%s black stay fast, delta_time = %lld\n", __func__, delta_time);
				}
		}

		if((smart_chg->board_temp >= 420) || (smart_chg->soc > 40)){
			this->active_flag = false;
		}
	} else {
		this->active_flag = false;
	}

	if (this->active_flag) {
		if ((smart_chg->soc > 37) && (smart_chg->board_temp > 410)) {
			if(smart_chg->low_fast_ffc >= 4300){
				smart_chg->low_fast_ffc = 4000;
			}
			xm_info("%s stay fast but low_fast_ffc = 4500, board_temp = %d, low_fast_ffc = %d\n", __func__, smart_chg->board_temp, smart_chg->low_fast_ffc);
		} else if ((smart_chg->soc > 38) && (smart_chg->board_temp > 380)) {
			if (smart_chg->low_fast_ffc <= 4000) {
					smart_chg->low_fast_ffc = 4000;
			} else {
					 smart_chg->low_fast_ffc = 4200;
			}
			xm_info("%s stay fast but cool down, board_temp = %d, low_fast_ffc = %d\n", __func__, smart_chg->board_temp, smart_chg->low_fast_ffc);
		}
		vote(smart_chg->charger_fcc_votable, THERMAL_VOTER, true, smart_chg->low_fast_ffc);
		xm_info("%s stay fastchg, low_fast_ffc = %d\n", __func__, smart_chg->low_fast_ffc);
	} else {
		vote(smart_chg->charger_fcc_votable, THERMAL_VOTER, true, smart_chg->normal_fast_ffc);
		xm_info("low battery fast charge function deactive\n");
	}

	xm_info("soc: %d, pd_type: %d, plugin_soc: %d, board_temp: %d, screen_state: %d, screen_back_state:%d, func[on: %d val: %d active: %d]\n",
		smart_chg->soc, smart_chg->pd_type, smart_chg->plugin_soc,
		smart_chg->board_temp, smart_chg->screen_state, smart_chg->screen_back_state,
		this->func_on, this->func_val, this->active_flag);

	return 0;
}

static int smart_chg_long_charge_protect_func(struct xm_smart_chg *smart_chg)
{
	struct smart_chg_func *this = NULL;
	int soc_threshold = 0;
	int soc = 0;
	bool chg_dev_chgen = false;
	struct mtk_charger *info = NULL;

	mtk_charger_from_psy(&info);
	if(IS_ERR_OR_NULL(info)) {
		xm_err("mtk_charger is_err_or_null\n");
		return PTR_ERR(info);
	}
	charger_dev_is_enabled(info->chg1_dev, &chg_dev_chgen);

	if (IS_ERR_OR_NULL(smart_chg)) {
		return -EFAULT;
	}

	this = &(smart_chg->funcs[SMART_CHG_LONG_CHG_PROTECT]);
	soc = smart_chg->soc;
	soc_threshold = this->func_val;

	if ((this->func_on && (soc >= soc_threshold)) ||
		(((soc > soc_threshold - 5) && (soc < soc_threshold)) && this->active_flag)) {
		this->active_flag = true;
		mtk_charger_fw_notifier_call_chain(CHG_FW_EVT_SMART_ENDURA_TRIG, &(this->active_flag));

		xm_info("long charge protect function active\n");
	} else if (((!this->func_on || (soc <= soc_threshold - 5)) && this->active_flag) ||
		(!this->func_on && !this->active_flag)) {
		this->active_flag = false;
		mtk_charger_fw_notifier_call_chain(CHG_FW_EVT_SMART_ENDURA_TRIG, &(this->active_flag));

		xm_info("long charge protect function deactive\n");
	}

	/* update stop charge status for sysfs */
	smart_chg->stop_charge = this->active_flag;

	vote(smart_chg->charger_fcc_votable, ENDURANCE_VOTER, this->active_flag, 0);

	if (this->active_flag) {
		if (chg_dev_chgen)
			charger_dev_enable(info->chg1_dev, false);
		xm_info("ENDURANCE discharge function true\n");
	} else {
		if (!chg_dev_chgen && smart_chg->effective_fcc != 0 && ((info->plug_in_soc100_flag != true) && (info->product_name_index != EEA)) && !info->charge_full && info->thermal.sm == BAT_TEMP_NORMAL)
			charger_dev_enable(info->chg1_dev, true);
		xm_info("ENDURANCE discharge function false\n");
	}

	xm_info("soc: %d, func[on: %d val: %d active: %d] chg_en: %d\n",
		smart_chg->soc, this->func_on, this->func_val, this->active_flag, chg_dev_chgen);

	return 0;
}

static int smart_chg_update_state(struct xm_smart_chg *smart_chg)
{
	union power_supply_propval pval = {0};
	int ret = 0;
	struct mtk_charger *info = NULL;

	info = dev_get_drvdata(smart_chg->dev);
	if (IS_ERR_OR_NULL(info)) {
		xm_err("failed to get mtk charger form device\n");
		return -EFAULT;
	}
	smart_chg->effective_fcc = get_effective_result(smart_chg->charger_fcc_votable);
	//smart_chg->vbus_type = info->vbus_type;
	smart_chg->real_type = info->real_type;

	//smart_chg->pd_active = info->pd_active;
	smart_chg->pd_type = info->pd_type;

	//smart_chg->thermal_level = info->thermal_policy->thermal_level;
	smart_chg->thermal_level = info->thermal_level;
	smart_chg->normal_fast_ffc = info->thermal_limit[5][ info->thermal_level];
	smart_chg->low_fast_ffc = info->thermal_limit[6][ info->thermal_level];

	smart_chg->board_temp = info->board_temp;

	smart_chg->screen_back_state = info->screen_state;
	smart_chg->screen_state = info->screen_status;

	ret = power_supply_get_property(smart_chg->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0) {
		xm_err("get battery capacity failed, ret = %d\n", ret);
		return ret;
	}
	smart_chg->soc = pval.intval;

	xm_info("soc: %d, real_type: %d, pd_type: %d, thermal_level: %d, board_temp: %d, screen_state: %d\n",
		smart_chg->soc, smart_chg->real_type, smart_chg->pd_type,
		smart_chg->thermal_level, smart_chg->board_temp, smart_chg->screen_state);

	return 0;
}

void handle_smart_chg_work(struct work_struct *work)
{
	struct xm_smart_chg *smart_chg = container_of(work, struct xm_smart_chg, smart_chg_work.work);

	mutex_lock(&smart_chg->smart_chg_work_lock);

	smart_chg_update_state(smart_chg);

	smart_chg_navigaition_discharge_func(smart_chg);
	smart_chg_outdoor_charge_func(smart_chg);
	smart_chg_low_battery_fast_charge_func(smart_chg);
	smart_chg_long_charge_protect_func(smart_chg);

	mutex_unlock(&smart_chg->smart_chg_work_lock);

	schedule_delayed_work(&smart_chg->smart_chg_work, msecs_to_jiffies(3000));
}

static int smart_chg_on_plugin_routines(struct xm_smart_chg *smart_chg)
{
	union power_supply_propval pval = {0};
	int ret = 0;
	struct mtk_charger *info = NULL;

	info = dev_get_drvdata(smart_chg->dev);
	if (IS_ERR_OR_NULL(info)) {
		xm_err("failed to get charger manager form device\n");
		return -EFAULT;
	}

	/* record soc on plugin */
	ret = power_supply_get_property(smart_chg->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0) {
		xm_err("get battery capacity failed, ret = %d\n", ret);
		return ret;
	}
	smart_chg->plugin_soc = pval.intval;

	/* record board temperature on plugin */
	smart_chg->plugin_board_temp = info->board_temp;

	smart_chg->stop_charge = false;

	return 0;
}

static int smart_chg_on_plugout_routines(struct xm_smart_chg *smart_chg)
{
	struct smart_chg_func *long_chg_protect = &(smart_chg->funcs[SMART_CHG_LONG_CHG_PROTECT]);

	/* clean up all recorders */
	smart_chg->plugin_soc = 0;
	smart_chg->plugin_board_temp = 0;
	smart_chg->screen_state = SCREEN_STATE_UNKONW;
	smart_chg->stop_charge = false;
	long_chg_protect->active_flag = false;

	return 0;
}

/* 定义多个节点的show函数 */
static ssize_t smart_chg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 smart_chg_node;
	struct mtk_charger *info = NULL;
	bool func_on;
	int func_val;
	unsigned long func_type = 0;
	unsigned long func_type_bitmap[1] = {0};

	mtk_charger_from_psy(&info);
	if(IS_ERR_OR_NULL(info)) {
		xm_err("mtk_charger is_err_or_null\n");
		return PTR_ERR(info);
	}

	if (IS_ERR_OR_NULL(info->smart_chg))
		return PTR_ERR(info->smart_chg);

	if (kstrtou32(buf, 0, &smart_chg_node))
		return -EINVAL;

	xm_info("smart_chg_node = 0x%08X\n", smart_chg_node);

	func_on = !!(smart_chg_node & 0x1);
	func_type_bitmap[0] = (smart_chg_node & 0xFFFE) >> 1;
	func_val = (smart_chg_node & 0xFFFF0000) >> 16;

	if (bitmap_weight(func_type_bitmap, SMART_CHG_FUNC_MAX) != 1) {
		xm_err("none or more than one function type bit set\n");
		info->smart_chg->status = SMART_CHG_ERROR;
		return -EINVAL;
	}

	func_type = find_first_bit(func_type_bitmap, SMART_CHG_FUNC_MAX);
	if (func_type >= SMART_CHG_FUNC_MAX) {
		xm_err("failed to find function type bit\n");
		info->smart_chg->status = SMART_CHG_ERROR;
		return -EINVAL;
	}

	info->smart_chg->funcs[func_type].func_on = func_on;
	info->smart_chg->funcs[func_type].func_val = func_val;
	info->smart_chg->status = SMART_CHG_SUCCESS;

	xm_info("set smart_chg = 0x%08X, func_type = %lu, func_on = %d, func_value = %d\n",
		smart_chg_node, func_type, func_on, func_val);

	return count;
}

static ssize_t smart_chg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_charger *info = NULL;
	int i = 0;
	u32 smart_chg_node = 0;

	mtk_charger_from_psy(&info);
	if(IS_ERR_OR_NULL(info)) {
		xm_err("mtk_charger is_err_or_null\n");
		return PTR_ERR(info);
	}

	if (IS_ERR_OR_NULL(info->smart_chg))
		return PTR_ERR(info->smart_chg);

	/* fill functions on/off bit */
	for (i = SMART_CHG_FUNC_MIN; i < SMART_CHG_FUNC_MAX; i++) {
		if (info->smart_chg->funcs[i].func_on) {
			smart_chg_node |= BIT_MASK(i);
		} else {
			smart_chg_node &= ~BIT_MASK(i);
		}
	}

	/* fill smart charge status bit: 0 -> success 1 -> error */
	smart_chg_node = ((smart_chg_node < 1) | !!info->smart_chg->status);

	xm_info("get smart_chg = 0x%08X", smart_chg_node);

	return sprintf(buf, "%u\n", smart_chg_node);
}

static struct device_attribute smart_chg_attr =
		__ATTR(smart_chg, 0644, smart_chg_show, smart_chg_store);

#if IS_ENABLED(CONFIG_XM_BATTERY_HEALTH)
static ssize_t smart_batt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_charger *info = NULL;
	int val = 0;

	mtk_charger_from_psy(&info);
	if(IS_ERR_OR_NULL(info)) {
		xm_err("mtk_charger is_err_or_null\n");
		return PTR_ERR(info);
	}

	if (IS_ERR_OR_NULL(info->batt_health))
		return PTR_ERR(info->batt_health);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	info->batt_health->smart_batt = val;

	xm_info("set smart_batt =%d\n", info->batt_health->smart_batt);

	return count;
}

static ssize_t smart_batt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_charger *info = NULL;

	mtk_charger_from_psy(&info);
	if(IS_ERR_OR_NULL(info)) {
		xm_err("mtk_charger is_err_or_null\n");
		return PTR_ERR(info);
	}

	if (IS_ERR_OR_NULL(info->batt_health))
		return PTR_ERR(info->batt_health);

	return sprintf(buf, "%d\n", info->batt_health->smart_batt);
}

static struct device_attribute smart_batt_attr =
		__ATTR(smart_batt, 0644, smart_batt_show, smart_batt_store);

static ssize_t smart_fv_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_charger *info = NULL;
	int val = 0;

	mtk_charger_from_psy(&info);
	if(IS_ERR_OR_NULL(info)) {
		xm_err("mtk_charger is_err_or_null\n");
		return PTR_ERR(info);
	}

	if (IS_ERR_OR_NULL(info->batt_health))
		return PTR_ERR(info->batt_health);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	info->batt_health->smart_fv = val;

	xm_info("set smart_fv = %d\n", info->batt_health->smart_fv);

	return count;
}

static ssize_t smart_fv_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_charger *info = NULL;

	mtk_charger_from_psy(&info);
	if(IS_ERR_OR_NULL(info)) {
		xm_err("mtk_charger is_err_or_null\n");
		return PTR_ERR(info);
	}
	if (IS_ERR_OR_NULL(info->batt_health))
		return PTR_ERR(info->batt_health);

	return sprintf(buf, "%d\n", info->batt_health->smart_fv);
}

static struct device_attribute smart_fv_attr =
		__ATTR(smart_fv, 0644, smart_fv_show, smart_fv_store);


static ssize_t night_charging_store(struct device *dev,
            struct device_attribute *attr, const char *buf, size_t count) {
	struct mtk_charger *info = NULL;
	bool val = 0;

	mtk_charger_from_psy(&info);
	if(IS_ERR_OR_NULL(info)) {
		xm_err("mtk_charger is_err_or_null\n");
		return PTR_ERR(info);
	}

	if (IS_ERR_OR_NULL(info->batt_health))
		return PTR_ERR(info->batt_health);
	
	if (kstrtobool(buf, &val))
		return -EINVAL;

	info->batt_health->night_smart_charge_on = val;

	return count;
}

static ssize_t night_charging_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
	struct mtk_charger *info = NULL;
	mtk_charger_from_psy(&info);
	if(IS_ERR_OR_NULL(info)) {
		xm_err("mtk_charger is_err_or_null\n");
		return PTR_ERR(info);
	}

	if (IS_ERR_OR_NULL(info->batt_health))
		return PTR_ERR(info->batt_health);

	return sprintf(buf, "%d\n", info->batt_health->night_smart_charge_on);
}
static struct device_attribute night_charging_attr =
		__ATTR(night_charging, 0644, night_charging_show, night_charging_store);
#endif

/* 定义属性数组（必须以NULL结尾） */
static struct attribute *batt_psy_attrs[] = {
#if IS_ENABLED(CONFIG_XM_SMART_CHG)
	&smart_chg_attr.attr,
#endif /* CONFIG_XM_SMART_CHG */
#if IS_ENABLED(CONFIG_XM_BATTERY_HEALTH)
	&smart_batt_attr.attr,
	&smart_fv_attr.attr,
	&night_charging_attr.attr,
#endif /* CONFIG_XM_BATTERY_HEALTH */
	NULL,
};

static const struct attribute_group smart_chg_attr_group = {
	.attrs = batt_psy_attrs,
};

int xm_smart_chg_init(struct mtk_charger *info)
{
	struct xm_smart_chg *smart_chg = NULL;
	int i = 0;
	int ret = 0;

	xm_err("smart charge init\n");
	if (info->smart_chg) {
		xm_err("smart charge already initialized\n");
		return -EINVAL;
	}

	smart_chg = devm_kzalloc(&info->pdev->dev, sizeof(*smart_chg), GFP_KERNEL);
	if (!smart_chg) {
		return -ENOMEM;
	}

	smart_chg->dev = &info->pdev->dev;

	/* votable initialize */
	smart_chg->charger_fcc_votable = find_votable("CHARGER_FCC");
	if (!smart_chg->charger_fcc_votable) {
		xm_err("find fcc_votable voltable failed\n");
	}

	smart_chg->main_icl_votable = find_votable("CHARGER_ICL");
	if (!smart_chg->main_icl_votable) {
		xm_err("find MAIN_ICL voltable failed\n");
	}

	smart_chg->fv_votable = find_votable("CHARGER_FV");
	if (!smart_chg->fv_votable) {
		xm_err("find MAIN_FV voltable failed\n");
	}

	/* power supply/class initialize */
	smart_chg->batt_psy = power_supply_get_by_name("battery");
	if (!smart_chg->batt_psy) {
		xm_err("get battery power supply failed\n");
		return ret;
	}

	    /* 一次性添加整个属性组 */
	ret = sysfs_create_group(&smart_chg->batt_psy->dev.kobj, &smart_chg_attr_group);
	if (ret) {
		power_supply_put(smart_chg->batt_psy);
		xm_err("Failed to create sysfs group: %d\n", ret);
		return ret;
	}

	/* smart charge work initialize */
	INIT_DELAYED_WORK(&smart_chg->smart_chg_work, handle_smart_chg_work);

	/* smart charge mutex lock initialize */
	mutex_init(&smart_chg->smart_chg_work_lock);

	/* default dynamic function on/off switch */
	for (i = SMART_CHG_FUNC_MIN; i < SMART_CHG_FUNC_MAX; i++) {
		smart_chg->funcs[i].func_on = false;
	}

	/* default flags */
	smart_chg->status = SMART_CHG_SUCCESS;
	smart_chg->stop_charge = 0;

	info->smart_chg = smart_chg;

	xm_err("smart charge %s initialize success\n", XM_SMART_CHG_VERSION);

	return 0;
}

int xm_smart_chg_deinit(struct mtk_charger *info)
{
	if (!info->smart_chg) {
		return 0;
	}

	cancel_delayed_work_sync(&info->smart_chg->smart_chg_work);

	//devm_kfree(info->pdev->dev, info->smart_chg);
	info->smart_chg = NULL;

	xm_info("smart charge %s deinitialize success\n", XM_SMART_CHG_VERSION);

	return 0;
}

int xm_smart_chg_run(struct mtk_charger *info)
{
	struct xm_smart_chg *smart_chg = info->smart_chg;

	/* NOTE: Don't add any code before this */
	smart_chg_on_plugin_routines(smart_chg);

	schedule_delayed_work(&smart_chg->smart_chg_work, msecs_to_jiffies(3000));

	return 0;
}

int xm_smart_chg_stop(struct mtk_charger *info)
{
	struct xm_smart_chg *smart_chg = info->smart_chg;

	cancel_delayed_work_sync(&smart_chg->smart_chg_work);

	/* NOTE: Don't add any code after this */
	smart_chg_on_plugout_routines(smart_chg);

	return 0;
}
