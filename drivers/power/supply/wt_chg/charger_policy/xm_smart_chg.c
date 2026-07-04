#include "xm_smart_chg.h"

struct xm_smart_chg_info info;
static int xm_smart_chg_post_init_rdy;

/*************** check if post_init ready ***************/
void set_xm_smart_chg_post_init_rdy(void)
{
	xm_smart_chg_post_init_rdy = 1;
}
bool is_xm_smart_chg_post_init_rdy(void)
{
	if (xm_smart_chg_post_init_rdy)
		return true;
	else
		return false;
}

#ifndef FACTORY_BUILD
extern int subpmic_chg_get_vbus_good(void);
#endif
/*************** smart_chg work ***************/
void monitor_smart_chg(void)
{
#ifndef FACTORY_BUILD
	struct power_supply *batt_psy;
	union power_supply_propval pval = {0,};
	int ret = 0;
	int enable = 0,vbus_good = 0;

	vbus_good = subpmic_chg_get_vbus_good();
	if (!vbus_good) {
		pr_err("%s no charger plug in\n", __func__);
		return;
	}

	batt_psy = power_supply_get_by_name("battery");
        if(!batt_psy){
                pr_err("%s failed to get batt_psy", __func__);
                return;
        }
	ret = power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0)
		pr_err("%s get battery soc error.\n", __func__);
	else
		info.soc = pval.intval;

	pr_err("%s ENDURANCE en_ret = %d, fun_val = %d, active_status = %d, endurance_ctrl_en = %d, ui_soc =%d\n",
                __func__,
		info.smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret,
		info.smart_charge[SMART_CHG_ENDURANCE_PRO].func_val,
		info.smart_charge[SMART_CHG_ENDURANCE_PRO].active_status,
                info.endurance_ctrl_en, info.soc);

        if((info.smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret && info.soc >= info.smart_charge[SMART_CHG_ENDURANCE_PRO].func_val) ||
        ((info.soc > (info.smart_charge[SMART_CHG_ENDURANCE_PRO].func_val - 5)) &&
        (info.soc < info.smart_charge[SMART_CHG_ENDURANCE_PRO].func_val) &&
        info.smart_charge[SMART_CHG_ENDURANCE_PRO].active_status))
	{
		info.smart_charge[SMART_CHG_ENDURANCE_PRO].active_status = true;
                // charger_adc_enable(info.charger, false);
		enable = 2;
		pr_err("%s notify charger disable", __func__);
		ret = main_chg_notifier_call_chain(MAIN_CHG_ENABLE_EVENT, enable);
		if (ret) {
			pr_err("%s: main_chg_notifier_call_chain error:%d\n", __func__, ret);
		}
		info.endurance_ctrl_en = true;
		pr_err("SMART_CHG: ENDURANCE disable charger, uisoc(%d) fuc_val(%d)\n", info.soc, info.smart_charge[SMART_CHG_ENDURANCE_PRO].func_val);
	}
	else if((((!info.smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret || info.soc <= (info.smart_charge[SMART_CHG_ENDURANCE_PRO].func_val - 5)) && info.smart_charge[SMART_CHG_ENDURANCE_PRO].active_status) ||
		(!info.smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret && !info.smart_charge[SMART_CHG_ENDURANCE_PRO].active_status)))
	{
		info.smart_charge[SMART_CHG_ENDURANCE_PRO].active_status = false;
                // charger_adc_enable(info.charger, true);
		enable = 1;
		pr_err("%s notify charger enable", __func__);
		ret = main_chg_notifier_call_chain(MAIN_CHG_ENABLE_EVENT, enable);
		if (ret) {
			pr_err("%s: main_chg_notifier_call_chain error:%d\n", __func__, ret);
		}
		info.endurance_ctrl_en = false;
                if(info.smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret){
		        pr_err("SMART_CHG: ENDURANCE enable charger, fuc_val(%d)\n", info.smart_charge[SMART_CHG_ENDURANCE_PRO].func_val);
                }
	}
#endif
	pr_info("%s ++\n", __func__);
}
#define MAX_CHECK_POST_INIT_READY_COUNT 1000
void xm_charge_work(struct work_struct *work)
{
        int i = 0;

        //make sure post init Ready
	for (i = 0; i < MAX_CHECK_POST_INIT_READY_COUNT; i++) {
		if (is_xm_smart_chg_post_init_rdy())
			break;
		msleep(150);
	}
        if (i == XM_SMART_CHG_POST_INIT_MS) {
		pr_err("%s check post init timeout\n", __func__);
        }

	monitor_smart_chg();
	schedule_delayed_work(&info.xm_charge_work, msecs_to_jiffies(5000));
}

#ifndef FACTORY_BUILD
void set_error(void)
{
	info.smart_charge[SMART_CHG_STATUS_FLAG].en_ret = 1;
	pr_err("%s en_ret=%d\n", __func__, info.smart_charge[SMART_CHG_STATUS_FLAG].en_ret);
}
void set_success(void)
{
	info.smart_charge[SMART_CHG_STATUS_FLAG].en_ret = 0;
	pr_err("%s en_ret=%d\n", __func__, info.smart_charge[SMART_CHG_STATUS_FLAG].en_ret);
}
int smart_chg_is_error(void)
{
	return info.smart_charge[SMART_CHG_STATUS_FLAG].en_ret? true : false;
}
void handle_smart_chg_functype(const int func_type, const int en_ret, const int func_val)
{
	switch (func_type)
	{
	case SMART_CHG_FEATURE_MIN_NUM ... SMART_CHG_FEATURE_MAX_NUM:
		info.smart_charge[func_type].en_ret = en_ret;
		info.smart_charge[func_type].active_status = false;
		info.smart_charge[func_type].func_val = func_val;
		set_success();
		pr_err("%s set func_type:%d, en_ret: %d\n", __func__, func_type, en_ret);
		break;
	default:
		pr_err("%s ERROR: Not supported func type: %d\n", __func__, func_type);
		set_error();
		break;
	}
}

int handle_smart_chg_functype_status(void)
{
	int i;
	int all_func_status = 0;
	all_func_status |= !!info.smart_charge[SMART_CHG_STATUS_FLAG].en_ret;	//handle bit0
	pr_err("%s all_func_status: %#X, en_ret: %d\n", __func__, all_func_status, info.smart_charge[SMART_CHG_STATUS_FLAG].en_ret);
	//save functype[i] enable status in all_func_status bit[i]
	for(i = SMART_CHG_FEATURE_MIN_NUM; i <= SMART_CHG_FEATURE_MAX_NUM; i++){  //handle bit1 ~ bit SMART_CHG_FEATURE_MAX_NUM
		if(info.smart_charge[i].en_ret)
			all_func_status |= BIT_MASK(i);
		else
			all_func_status &= ~BIT_MASK(i);
		pr_err("%s type:%d, en_ret: %d, active_status: %d,func_val: %d, all_func_status: %#X\n",
			__func__, i, info.smart_charge[i].en_ret, info.smart_charge[i].active_status, info.smart_charge[i].func_val,all_func_status);
	}
	pr_err("%s all_func_status:%#X\n", __func__, all_func_status);
	return all_func_status;
}
#endif

/*************** smart_chg node ***************/
static ssize_t smart_chg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
#ifndef FACTORY_BUILD
    int val;
	bool en_ret;
	unsigned long func_type;
	int func_val;
	int bit_pos;
	int all_func_status;
	pr_info("smart_chg_store ++\n");
	if (kstrtoint(buf, 16, &val)) {
                pr_err("%s invalid value!\n", __func__);
		return -EINVAL;
        }
	en_ret = val & 0x1;
	func_type = (val & 0xFFFE) >> 1;
	func_val = val >> 16;
	pr_info("%s get val:%#X, func_type:%#lX, en_ret:%d, func_val:%d\n",
		__func__, val, func_type, en_ret, func_val);
	bit_pos = find_first_bit(&func_type, SMART_CHG_FEATURE_MAX_NUM);
        if(bit_pos == SMART_CHG_FEATURE_MAX_NUM || find_next_bit(&func_type, SMART_CHG_FEATURE_MAX_NUM , bit_pos + 1) != SMART_CHG_FEATURE_MAX_NUM){
                pr_err("%s ERROR: zero or more than one func type!\n", __func__);
                pr_err("%s find_next_bit = %lu, bit_pos = %d\n",
                        __func__, find_next_bit(&func_type, SMART_CHG_FEATURE_MAX_NUM , bit_pos + 1), bit_pos);
                set_error();
        } else
                set_success();
        // if func_type bit0 is 1, bit_pos = 0, not 1. so ++bit_pos.
        if(!smart_chg_is_error())
                handle_smart_chg_functype(++bit_pos, en_ret, func_val);
        //pdate smart_chg[0] status
        all_func_status = handle_smart_chg_functype_status();
        info.smart_chg_cmd = all_func_status;
        info.smart_charge[SMART_CHG_STATUS_FLAG].en_ret = all_func_status & 0x1;
        info.smart_charge[SMART_CHG_STATUS_FLAG].active_status = (all_func_status & 0xFFFE) >> 1;
        pr_info("%s smart_chg_cmd: %#X, en_ret:%#X, active_status:%d\n",
		__func__, info.smart_chg_cmd, info.smart_charge[SMART_CHG_STATUS_FLAG].en_ret, info.smart_charge[SMART_CHG_STATUS_FLAG].active_status);
        return count;
#else
	pr_info("%s ++\n");
        return 0;
#endif
}
static ssize_t smart_chg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	//pr_err("%s smart_chg_cmd = %d\n", __func__, info.smart_chg_cmd);
	return sprintf(buf, "%d\n", info.smart_chg_cmd);
}
static struct device_attribute smart_chg_attr =
		__ATTR(smart_chg, 0644, smart_chg_show, smart_chg_store);
static struct attribute *batt_psy_attrs[] = {
	&smart_chg_attr.attr,
	NULL,
};
static const struct attribute_group batt_psy_attrs_group = {
	.attrs = batt_psy_attrs,
};
int xm_smart_chg_add_to_batt_psy(void)
{
        struct power_supply *batt_psy;
        int ret = 0;
        pr_err("%s ++\n", __func__);

        batt_psy = power_supply_get_by_name("battery");
        if(!batt_psy){
                pr_err("%s failed to get batt_psy", __func__);
                return -EPROBE_DEFER;
        }
        pr_err("%s success to get batt_psy", __func__);

        ret = sysfs_create_group(&batt_psy->dev.kobj, &batt_psy_attrs_group);
        if(ret){
                pr_err("%s failed to creat batt_psy group, ret = %d\n", __func__, ret);
                return -EPROBE_DEFER;
        }

        pr_err("%s --", __func__);
        return ret;
}

/*************** main_chg notifier ***************/
static int main_chg_notifier_event_callback(struct notifier_block *notifier,
			unsigned long chg_event, void *val)
{
#ifndef FACTORY_BUILD
	struct power_supply *batt_psy;
	union power_supply_propval pval = {0,};
	int ret = 0;

	batt_psy = power_supply_get_by_name("battery");
        if(!batt_psy){
                pr_err("%s failed to get batt_psy", __func__);
                return 0;
        }
	ret = power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0)
		pr_err("%s get battery soc error.\n", __func__);
	else
		info.soc = pval.intval;
#endif
	switch (chg_event) {
        //charger_plugin_event 0:none 1:plugin 2:plugout
	case MAIN_CHG_USB_PLUGIN_EVENT:
		info.charger_plugin_event = *(int *)val;
		pr_info("%s: get charger_plugin_event: %d\n", __func__, info.charger_plugin_event);
#ifndef FACTORY_BUILD
		if(info.charger_plugin_event == 1) {
			if(info.smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret && info.soc >= info.smart_charge[SMART_CHG_ENDURANCE_PRO].func_val) {
                                pr_err("%s endurance is working, set active_status true\n", __func__);
                                info.smart_charge[SMART_CHG_ENDURANCE_PRO].active_status = true;
                        }
		}
		else if(info.charger_plugin_event == 2) {
			info.smart_charge[SMART_CHG_ENDURANCE_PRO].active_status = false;
			info.endurance_ctrl_en = false;
		}
#endif
		break;
	default:
		pr_err("%s: not supported charger notifier event: %lu\n", __func__, chg_event);
		break;
	}

	return NOTIFY_DONE;
}

/*************** post_init work ***************/
void xm_smart_chg_post_init_work(struct work_struct *work)
{
        int ret = 0;
        pr_err("%s ++\n", __func__);

        ret = xm_smart_chg_add_to_batt_psy();
        if(ret){
                pr_err("%s xm_smart_charge_add_to_batt_psy failed(%d)\n", __func__, ret);
        }

        //register main_chg_notifier
        info.main_chg_nb.notifier_call = main_chg_notifier_event_callback;
	main_chg_reg_notifier(&info.main_chg_nb);

        //set post init flag ready
        set_xm_smart_chg_post_init_rdy();
}

/******************* Module driver ***********************************/
static int xm_smart_chg_probe(struct platform_device *pdev)
{
	pr_err("%s ++\n", __func__);
        INIT_DELAYED_WORK(&info.xm_smart_chg_post_init_work, xm_smart_chg_post_init_work);
        INIT_DELAYED_WORK(&info.xm_charge_work, xm_charge_work);
        schedule_delayed_work(&info.xm_smart_chg_post_init_work, msecs_to_jiffies(XM_SMART_CHG_POST_INIT_MS));
        schedule_delayed_work(&info.xm_charge_work, msecs_to_jiffies(XM_CHARGE_WORK_MS));
	return 0;
}

static int xm_smart_chg_remove(struct platform_device *pdev)
{
	pr_err("%s ++\n", __func__);
        cancel_delayed_work(&info.xm_charge_work);
        cancel_delayed_work(&info.xm_smart_chg_post_init_work);
        main_chg_unreg_notifier(&info.main_chg_nb);
	return 0;
}

static const struct of_device_id xm_smart_chg_of_ids[] = {
	{
		.compatible = "xiaomi,xm_smart_chg",
	},
	{},
};
MODULE_DEVICE_TABLE(of, xm_smart_chg_of_ids);
static struct platform_driver xm_smart_chg_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "xm_smart_chg",
                   .of_match_table = xm_smart_chg_of_ids,
	},
	.probe = xm_smart_chg_probe,
	.remove = xm_smart_chg_remove,
};

module_platform_driver(xm_smart_chg_driver);
MODULE_DESCRIPTION("XM SMART CHARGE Driver");
MODULE_LICENSE("GPL");