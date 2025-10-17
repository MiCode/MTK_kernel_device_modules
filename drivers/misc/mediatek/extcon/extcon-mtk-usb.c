// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/extcon-provider.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/slab.h>
#include <linux/usb/role.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/mca/common/mca_log.h>
#include <linux/mca/common/mca_event.h>

#include "extcon-mtk-usb.h"
#include "../../../power/supply/mtk_charger.h"
#include "../../../power/supply/pd_cp_manager.h"

#if IS_ENABLED(CONFIG_TCPC_CLASS)
#include "tcpm.h"
#endif

#ifndef MCA_LOG_TAG
#define MCA_LOG_TAG "mca_extcon_usb"
#endif

static const unsigned int usb_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};
static unsigned int global_role = 0;

static void mtk_usb_extcon_update_role(struct work_struct *work)
{
	struct usb_role_info *role = container_of(to_delayed_work(work),
					struct usb_role_info, dwork);
	struct mtk_extcon_info *extcon = role->extcon;
	unsigned int cur_dr, new_dr;

	cur_dr = extcon->c_role;
	new_dr = role->d_role;

	extcon->c_role = new_dr;
	global_role = new_dr;

	mca_log_err("port-%d cur_dr(%d) new_dr(%d)\n",
		gpio_is_valid(extcon->vdd_boost_en_gpio_b) ? 1 : 0, cur_dr, new_dr);

	/* usb role switch */
	if (extcon->role_sw)
		usb_role_switch_set_role(extcon->role_sw, new_dr);
	else {
		/* none -> device */
		if (cur_dr == USB_ROLE_NONE && new_dr == USB_ROLE_DEVICE) {
			extcon_set_state_sync(extcon->edev, EXTCON_USB, true);
		/* none -> host */
		} else if (cur_dr == USB_ROLE_NONE && new_dr == USB_ROLE_HOST) {
			extcon_set_state_sync(extcon->edev,
							EXTCON_USB_HOST, true);
		/* device -> none */
		} else if (cur_dr == USB_ROLE_DEVICE &&
				new_dr == USB_ROLE_NONE) {
			extcon_set_state_sync(extcon->edev, EXTCON_USB, false);
		/* host -> none */
		} else if (cur_dr == USB_ROLE_HOST && new_dr == USB_ROLE_NONE) {
			extcon_set_state_sync(extcon->edev,
							EXTCON_USB_HOST, false);
		/* device -> host */
		} else if (cur_dr == USB_ROLE_DEVICE &&
				new_dr == USB_ROLE_HOST) {
			extcon_set_state_sync(extcon->edev, EXTCON_USB, false);
			extcon_set_state_sync(extcon->edev,
							EXTCON_USB_HOST, true);
		/* host -> device */
		} else if (cur_dr == USB_ROLE_HOST &&
				new_dr == USB_ROLE_DEVICE) {
			extcon_set_state_sync(extcon->edev,
							EXTCON_USB_HOST, false);
			extcon_set_state_sync(extcon->edev, EXTCON_USB, true);
		}
	}

	kfree(role);
}

static int mtk_usb_extcon_set_role(struct mtk_extcon_info *extcon,
						unsigned int role)
{
	struct usb_role_info *role_info;

	/* create and prepare worker */
	role_info = kzalloc(sizeof(*role_info), GFP_ATOMIC);
	if (!role_info)
		return -ENOMEM;

	INIT_DELAYED_WORK(&role_info->dwork, mtk_usb_extcon_update_role);

	role_info->extcon = extcon;
	role_info->d_role = role;
	/* issue connection work */
	queue_delayed_work(extcon->extcon_wq, &role_info->dwork, 0);

	return 0;
}
static bool mi_pd_authentication_no_skip(void)
{
#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
	int p1_port_is_on = 0, p0_port_is_on = 0;
	usb_get_property(USB_PROP_TYPEC_PORT1_PLUGIN, &p1_port_is_on);
	usb_get_property(USB_PROP_TYPEC_PORT0_PLUGIN, &p0_port_is_on);
	if (p1_port_is_on == 3 && p0_port_is_on == 1) {
		mca_log_err("P1 port is on, skip PD authentication\n");
		return false;
	}
	return true;
#else
	return true;
#endif
}

static void mtk_usb_extcon_psy_detector(struct work_struct *work)
{
	struct mtk_extcon_info *extcon = container_of(to_delayed_work(work),
		struct mtk_extcon_info, wq_psy);

	union power_supply_propval pval;
	union power_supply_propval tval;
	int input_suspend = 0;
	int ret;

	if (gpio_is_valid(extcon->vdd_boost_en_gpio_b)) {
		dev_info(extcon->dev, "P1 port NOT support device!\n");
		return;
	}
	ret = usb_get_property(USB_PROP_INPUT_SUSPEND, &input_suspend);
	if (ret < 0) {
		dev_info(extcon->dev, "failed to get input_suspend\n");
		return;
	}

	ret = power_supply_get_property(extcon->usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &pval);
	if (ret < 0) {
		dev_info(extcon->dev, "failed to get online prop\n");
		return;
	}

	ret = power_supply_get_property(extcon->usb_psy,
				POWER_SUPPLY_PROP_TYPE, &tval);
	if (ret < 0) {
		dev_info(extcon->dev, "failed to get usb type\n");
		return;
	}

	mca_log_err("prot-%d online=%d, input_suspend=%d, type=%d, c_role=%d, global_role=%d\n",
		gpio_is_valid(extcon->vdd_boost_en_gpio_b) ? 1 : 0,
		pval.intval, input_suspend, tval.intval, extcon->c_role, global_role);

	/* Workaround for PR_SWAP, Host mode should not come to this function. */
	if ((extcon->c_role == USB_ROLE_HOST ||
		(tval.intval != POWER_SUPPLY_TYPE_USB && tval.intval != POWER_SUPPLY_TYPE_USB_CDP && pval.intval) ||
		global_role != extcon->c_role) && mi_pd_authentication_no_skip()) {
		mca_log_err("Remain HOST mode or usb_type not sdp or cdp\n");
		return;
	}

	if (pval.intval && (tval.intval == POWER_SUPPLY_TYPE_USB ||
			tval.intval == POWER_SUPPLY_TYPE_USB_CDP))
		mtk_usb_extcon_set_role(extcon, USB_ROLE_DEVICE);
	else if (input_suspend && (tval.intval == POWER_SUPPLY_TYPE_USB ||
			tval.intval == POWER_SUPPLY_TYPE_USB_CDP))
		mtk_usb_extcon_set_role(extcon, USB_ROLE_DEVICE);
	else
		mtk_usb_extcon_set_role(extcon, USB_ROLE_NONE);
}

static int mtk_usb_extcon_psy_notifier(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct power_supply *psy = data;
	struct mtk_extcon_info *extcon = container_of(nb,
					struct mtk_extcon_info, psy_nb);

	if (event != PSY_EVENT_PROP_CHANGED || psy != extcon->usb_psy)
		return NOTIFY_DONE;

	queue_delayed_work(system_power_efficient_wq, &extcon->wq_psy, 0);

	return NOTIFY_DONE;
}

static int mtk_usb_extcon_psy_init(struct mtk_extcon_info *extcon)
{
	int ret = 0;
	struct device *dev = extcon->dev;

	if (!of_property_read_bool(dev->of_node, "charger")) {
		ret = -EINVAL;
		goto fail;
	}

	extcon->usb_psy = devm_power_supply_get_by_phandle(dev, "charger");
	if (IS_ERR_OR_NULL(extcon->usb_psy)) {
		/* try to get by name */
		extcon->usb_psy = power_supply_get_by_name("primary_chg");
		if (IS_ERR_OR_NULL(extcon->usb_psy)) {
			mca_log_err("fail to get usb_psy\n");
			extcon->usb_psy = NULL;
			ret = -EINVAL;
			goto fail;
		}
	}

	INIT_DELAYED_WORK(&extcon->wq_psy, mtk_usb_extcon_psy_detector);

	extcon->psy_nb.notifier_call = mtk_usb_extcon_psy_notifier;
	ret = power_supply_reg_notifier(&extcon->psy_nb);
	if (ret)
		mca_log_err("fail to register notifer\n");
fail:
	return ret;
}

static void xm_enable_18w_reverse_chg(struct charger_device *cp_master,
                                      struct mtk_extcon_info *extcon, bool is_on)
{
	static bool last_on = false;
	struct charger_device *chg_dev = get_charger_by_name("primary_chg");

	if (last_on == is_on)
		return;
	if (!chg_dev) {
		mca_log_err("failed to get chg_dev");
		return;
	}

	if (is_on) {
		charger_dev_enable_powerpath(chg_dev, false);
		// enable revert cp boost
		mca_log_err("port-%d [REV_18W_CHG] start revert 1_2 cp",
			gpio_is_valid(extcon->vdd_boost_en_gpio_b) ? 1 : 0);
		charger_dev_enable_acdrv_manual(cp_master, true);
		charger_dev_cp_set_mode(cp_master, SC8561_REVERSE_1_2_CONVERTER_MODE);
		charger_dev_cp_rev_chg_config(cp_master, true);
		if (gpio_is_valid(extcon->vdd_boost_en_gpio_b)) {
			charger_dev_enable_cp_usb_gate(cp_master, false);
			charger_dev_cp_enable_wpcgate(cp_master, true);
		} else {
			charger_dev_cp_enable_wpcgate(cp_master, false);
			charger_dev_enable_cp_usb_gate(cp_master, true);
		}
		charger_dev_cp_set_qb(cp_master, true);
		msleep(1500);
		charger_dev_enable(cp_master, true);
		charger_dev_cp_dump_register(cp_master);
		extcon->vbus_vol_request = 9000;
	} else {
		mca_log_err("port-%d [REV_18W_CHG] end rev chg",
			gpio_is_valid(extcon->vdd_boost_en_gpio_b) ? 1 : 0);
		charger_dev_enable(cp_master, false);
		charger_dev_cp_set_qb(cp_master, false);
		charger_dev_cp_set_mode(cp_master, SC8561_FORWARD_2_1_CHARGER_MODE);
		charger_dev_cp_dump_register(cp_master);
		charger_dev_cp_rev_chg_config(cp_master, false);
		usb_set_property(USB_PROP_TYPEC_REVERSE_CHG, 0);
		charger_dev_enable_powerpath(chg_dev, true);
	}
	last_on = is_on;
}

static int mtk_usb_extcon_set_vbus(struct mtk_extcon_info *extcon,
							bool is_on)
{
	struct regulator *vbus = extcon->vbus;
	struct device *dev = extcon->dev;
	struct charger_device *cp_master;
	static int last_rqc_vbus = 0;
	int ret;
	int rev_quick_chg = 0;

	/* vbus is optional */
	usb_get_property(USB_PROP_TYPEC_REVERSE_CHG, &rev_quick_chg);
	mca_log_err("rev_quick_chg=%d, last_rqc_vbus=%d, request_vbus = %d\n",
		rev_quick_chg, last_rqc_vbus, extcon->vbus_vol_request);
	if ((extcon->vbus_on == is_on && !rev_quick_chg && last_rqc_vbus == extcon->vbus_vol_request) || !vbus)
		return 0;
#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
	if (gpio_is_valid(extcon->vdd_boost_en_gpio_b))
		usb_set_property(USB_PROP_TYPEC_PORT_NUM, 1);
#endif

	cp_master = get_charger_by_name("cp_master");
	if (!cp_master) {
		mca_log_err("%s: failed to get cp_master\n", __func__);
		return -1;
	}

	mca_log_err("port-%d vbus turn %s\n",
		gpio_is_valid(extcon->vdd_boost_en_gpio_b) ? 1 : 0, is_on ? "on" : "off");

	if (is_on) {
		if (gpio_is_valid(extcon->vdd_boost_en_gpio_a) || gpio_is_valid(extcon->vdd_boost_en_gpio_b)) {
			charger_dev_enable_acdrv_manual(cp_master, true);
			if (gpio_is_valid(extcon->vdd_boost_en_gpio_a) && !rev_quick_chg) {
				charger_dev_enable_cp_usb_gate(cp_master, false);
				charger_dev_enable_cp_wpc_gate(cp_master, true);
				gpio_set_value(extcon->vdd_boost_en_gpio_a, is_on);
#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
				usb_set_property(USB_PROP_TYPEC_PORT0_PLUGIN, 3);
#endif
				mca_log_err("[BOOST] vdd_boost_en_gpio_a enable\n");
			} else if (gpio_is_valid(extcon->vdd_boost_en_gpio_b) && !rev_quick_chg) {
				charger_dev_enable_cp_wpc_gate(cp_master, false);
				charger_dev_enable_cp_usb_gate(cp_master, true);
				gpio_set_value(extcon->vdd_boost_en_gpio_b, is_on);
#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
				usb_set_property(USB_PROP_TYPEC_PORT1_PLUGIN, 3);
#endif
				mca_log_err("[BOOST] vdd_boost_en_gpio_b enable\n");
			}
			usb_set_property(USB_PROP_TYPEC_RQC_CONDITION_CHECK, 1);
			if(rev_quick_chg){
				xm_enable_18w_reverse_chg(cp_master, extcon, rev_quick_chg);
			}
			if (last_rqc_vbus > extcon->vbus_vol_request && !rev_quick_chg) {
				xm_enable_18w_reverse_chg(cp_master, extcon, rev_quick_chg);
			}
		} else {
			charger_dev_cp_set_mode(cp_master, SC8561_REVERSE_1_1_CONVERTER_MODE);
			charger_dev_enable_acdrv_manual(cp_master, true);
			if (extcon->vbus_vol) {
				ret = regulator_set_voltage(vbus,
					extcon->vbus_vol, extcon->vbus_vol_max);
				if (ret) {
					mca_log_err("vbus regulator set voltage failed\n");
					return ret;
				}
			}

			if (extcon->vbus_cur) {
				ret = regulator_set_current_limit(vbus,
					extcon->vbus_cur, extcon->vbus_cur);
				if (ret) {
					mca_log_err("vbus regulator set current failed\n");
					return ret;
				}
			}

			ret = regulator_enable(vbus);
			if (ret) {
				dev_info(dev, "vbus regulator enable failed\n");
				return ret;
			}
			mca_log_err("vbus regulator enable\n");
		}
	} else {
		/* Restore to default state */
		extcon->vbus_cur_inlimit = 0;
		if (gpio_is_valid(extcon->vdd_boost_en_gpio_a) || gpio_is_valid(extcon->vdd_boost_en_gpio_b)) {
			xm_enable_18w_reverse_chg(cp_master, extcon, false);
			usb_set_property(USB_PROP_TYPEC_RQC_CONDITION_CHECK, 0);
		}

		if (gpio_is_valid(extcon->vdd_boost_en_gpio_a)) {
			gpio_set_value(extcon->vdd_boost_en_gpio_a, is_on);
#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
			usb_set_property(USB_PROP_TYPEC_PORT0_PLUGIN, 0);
#endif
			mca_log_err("[BOOST] vdd_boost_en_gpio_a disable\n");
		} else if (gpio_is_valid(extcon->vdd_boost_en_gpio_b)) {
			gpio_set_value(extcon->vdd_boost_en_gpio_b, is_on);
#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
			usb_set_property(USB_PROP_TYPEC_PORT1_PLUGIN, 0);
#endif
			mca_log_err("[BOOST] vdd_boost_en_gpio_b disable\n");
		} else {
			charger_dev_cp_set_mode(cp_master, SC8561_FORWARD_4_1_CHARGER_MODE);
			regulator_disable(vbus);
			mca_log_err("vbus regulator disable\n");
		}
		charger_dev_enable_acdrv_manual(cp_master, false);
	}
	ret = usb_set_property(USB_PROP_OTG_ENABLE, is_on);
	if (ret < 0)
		dev_info(dev, "failed to set otg enable\n");
	extcon->vbus_on = is_on;
	last_rqc_vbus = extcon->vbus_vol_request;

	return 0;
}

static ssize_t vbus_limit_cur_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mtk_extcon_info *extcon = dev_get_drvdata(dev);
	struct regulator *vbus = extcon->vbus;

	if (!vbus)
		return sprintf(buf, "0");
	else
		return sprintf(buf, "%d\n", extcon->vbus_cur_inlimit);
}

static ssize_t vbus_limit_cur_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct mtk_extcon_info *extcon = dev_get_drvdata(dev);
	struct regulator *vbus = extcon->vbus;
	int ret, in_limit = 0;
	unsigned int vbus_cur = 0;
	/* Check whether we have vbus instance */
	if (!vbus) {
		dev_info(dev, "No vbus instance\n");
		return -EOPNOTSUPP;
	}
	if ((kstrtoint(buf, 10, &in_limit) != 0) || (in_limit != 1  && in_limit != 0)) {
		dev_info(dev, "Invalid input\n");
		return -EINVAL;
	}

	extcon->vbus_cur_inlimit = (in_limit == 1);
	/* Only operate while vbus is on */
	if (extcon->vbus_on) {
		if (extcon->vbus_cur_inlimit && extcon->vbus_limit_cur)
			vbus_cur = extcon->vbus_limit_cur;
		else if (!extcon->vbus_cur_inlimit && extcon->vbus_cur)
			vbus_cur = extcon->vbus_cur;
	}
	if (vbus_cur) {
		ret = regulator_set_current_limit(vbus, vbus_cur, vbus_cur);
		if (ret) {
			dev_info(dev, "vbus regulator set current failed\n");
			return -EIO;
		}
	} else
		dev_info(dev, "Do not change current\n");
	return count;
}

static DEVICE_ATTR_RW(vbus_limit_cur);

static ssize_t vbus_switch_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mtk_extcon_info *extcon = dev_get_drvdata(dev);
	struct regulator *vbus = extcon->vbus;

	if (!vbus)
		return sprintf(buf, "0");
	else
		return sprintf(buf, "%d\n", extcon->vbus_on);
}

static ssize_t vbus_switch_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct mtk_extcon_info *extcon = dev_get_drvdata(dev);
	struct regulator *vbus = extcon->vbus;
	int is_on = 0;

	/* Check whether we have vbus instance */
	if (!vbus) {
		dev_info(dev, "No vbus instance\n");
		return -EOPNOTSUPP;
	}
	if ((kstrtoint(buf, 10, &is_on) != 0) || (is_on != 1  && is_on != 0)) {
		dev_info(dev, "Invalid input\n");
		return -EINVAL;
	}

	mtk_usb_extcon_set_vbus(extcon, is_on);

	return count;
}

static DEVICE_ATTR_RW(vbus_switch);

static int mtk_usb_extcon_vbus_init(struct mtk_extcon_info *extcon)
{
	int ret = 0;
	struct device *dev = extcon->dev;

	/* otg external boost probe first */
	extcon->vdd_boost_en_gpio_a = of_get_named_gpio(dev->of_node, "vdd_boost_5v_en_a", 0);
	if (!gpio_is_valid(extcon->vdd_boost_en_gpio_a)){
		dev_info(dev, "failed to parse vdd_boost_5v_en_a\n");
	}else{
		gpio_direction_output(extcon->vdd_boost_en_gpio_a, 0);
		dev_info(dev, "parse vdd_boost_5v_en_a : %d OK!\n", extcon->vdd_boost_en_gpio_a);
	}
	extcon->vdd_boost_en_gpio_b = of_get_named_gpio(dev->of_node, "vdd_boost_5v_en_b", 0);
	if (!gpio_is_valid(extcon->vdd_boost_en_gpio_b)){
		dev_info(dev, "failed to parse vdd_boost_5v_en_b\n");
	}else{
		gpio_direction_output(extcon->vdd_boost_en_gpio_b, 0);
		dev_info(dev, "parse vdd_boost_5v_en_b : %d OK!\n", extcon->vdd_boost_en_gpio_b);
	}

	if (!of_property_read_bool(dev->of_node, "vbus-supply")) {
		ret = -EINVAL;
		goto fail;
	}

	extcon->vbus =  devm_regulator_get(dev, "vbus");
	if (IS_ERR(extcon->vbus)) {
		/* try to get by name */
		extcon->vbus =  devm_regulator_get(dev, "usb-otg-vbus");
		if (IS_ERR(extcon->vbus)) {
			mca_log_err("failed to get vbus\n");
			ret = PTR_ERR(extcon->vbus);
			extcon->vbus = NULL;
			goto fail;
		}
	}

	/* sync vbus state */
	extcon->vbus_on = regulator_is_enabled(extcon->vbus);
	mca_log_err("vbus is %s\n", extcon->vbus_on ? "on" : "off");

	if (!of_property_read_u32(dev->of_node, "vbus-voltage",
				&extcon->vbus_vol))
		dev_info(dev, "vbus-voltage=%d", extcon->vbus_vol);

	if (!of_property_read_u32(dev->of_node, "vbus-voltage-max",
				&extcon->vbus_vol_max))
		dev_info(dev, "vbus-voltage-max=%d", extcon->vbus_vol_max);
	else
		extcon->vbus_vol_max = extcon->vbus_vol;

	if (!of_property_read_u32(dev->of_node, "vbus-current",
				&extcon->vbus_cur))
		dev_info(dev, "vbus-current=%d", extcon->vbus_cur);
	if (!of_property_read_u32(dev->of_node, "vbus-limit-current",
				&extcon->vbus_limit_cur)) {
		dev_info(dev, "vbus limited current=%d", extcon->vbus_limit_cur);
		extcon->vbus_cur_inlimit = 0;
		ret = device_create_file(dev, &dev_attr_vbus_limit_cur);
		if (ret)
			dev_info(dev, "failed to create vbus currnet limit control node\n");
	}

	ret = device_create_file(dev, &dev_attr_vbus_switch);
	if (ret)
		dev_info(dev, "failed to create vbus switch node\n");

fail:
	return ret;
}

#if IS_ENABLED(CONFIG_TCPC_CLASS)
static int mtk_extcon_tcpc_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct mtk_extcon_info *extcon =
			container_of(nb, struct mtk_extcon_info, tcpc_nb);
	bool vbus_on;
	int rev_quick_chg = 0;

	switch (event) {
	case TCP_NOTIFY_SOURCE_VBUS:
		mca_log_err("port-%d source vbus = %dmv\n",
				 gpio_is_valid(extcon->vdd_boost_en_gpio_b) ? 1 : 0, noti->vbus_state.mv);
		vbus_on = (noti->vbus_state.mv) ? true : false;
		extcon->vbus_vol_request = noti->vbus_state.mv;
		mtk_usb_extcon_set_vbus(extcon, vbus_on);
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		mca_log_err("port-%d old_state=%d, new_state=%d\n",
				gpio_is_valid(extcon->vdd_boost_en_gpio_b) ? 1 : 0,
				noti->typec_state.old_state,
				noti->typec_state.new_state);
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			mca_log_err("port-%d Type-C SRC plug in\n",
				gpio_is_valid(extcon->vdd_boost_en_gpio_b) ? 1 : 0);
			mtk_usb_extcon_set_role(extcon, USB_ROLE_HOST);
		} else if (!(extcon->bypss_typec_sink) &&
			noti->typec_state.old_state == TYPEC_UNATTACHED &&
			(noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
			noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC ||
			noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			noti->typec_state.new_state == TYPEC_ATTACHED_DBGACC_SNK)) {
			mca_log_err("port-%d Type-C SINK plug in\n",
				gpio_is_valid(extcon->vdd_boost_en_gpio_b) ? 1 : 0);
			mtk_usb_extcon_set_role(extcon, USB_ROLE_DEVICE);
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_SNK ||
			noti->typec_state.old_state == TYPEC_ATTACHED_NORP_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_DBGACC_SNK) &&
			noti->typec_state.new_state == TYPEC_UNATTACHED) {
			mca_log_err("port-%d Type-C plug out\n",
				gpio_is_valid(extcon->vdd_boost_en_gpio_b) ? 1 : 0);
			mtk_usb_extcon_set_role(extcon, USB_ROLE_NONE);
		}
		break;
	case TCP_NOTIFY_DR_SWAP:
		mca_log_err("port-%d dr_swap, new role=%d\n",
				gpio_is_valid(extcon->vdd_boost_en_gpio_b) ? 1 : 0,
				noti->swap_state.new_role);
		usb_get_property(USB_PROP_TYPEC_REVERSE_CHG, &rev_quick_chg);
		if (noti->swap_state.new_role == PD_ROLE_UFP &&
				extcon->c_role == USB_ROLE_HOST) {
			mca_log_err("switch role to device\n");
			if(!rev_quick_chg)
				mtk_usb_extcon_set_role(extcon, USB_ROLE_DEVICE);
		} else if (noti->swap_state.new_role == PD_ROLE_DFP &&
				(extcon->c_role == USB_ROLE_DEVICE || extcon->c_role == USB_ROLE_NONE || global_role != extcon->c_role)) {
			mca_log_err("switch role to host\n");
			mtk_usb_extcon_set_role(extcon, USB_ROLE_HOST);
		} else
			mca_log_err("wrong condition\n");

		break;
	}

	return NOTIFY_OK;
}

static int mtk_usb_extcon_tcpc_init(struct mtk_extcon_info *extcon)
{
	struct tcpc_device *tcpc_dev;
	struct device_node *np = extcon->dev->of_node;
	const char *tcpc_name;
	int ret;

	ret = of_property_read_string(np, "tcpc", &tcpc_name);
	if (ret < 0)
		return -ENODEV;

	tcpc_dev = tcpc_dev_get_by_name(tcpc_name);
	if (!tcpc_dev) {
		dev_err(extcon->dev, "get tcpc device fail\n");
		return -ENODEV;
	}

	extcon->tcpc_nb.notifier_call = mtk_extcon_tcpc_notifier;
	ret = register_tcp_dev_notifier(tcpc_dev, &extcon->tcpc_nb,
		TCP_NOTIFY_TYPE_USB | TCP_NOTIFY_TYPE_VBUS |
		TCP_NOTIFY_TYPE_MISC);
	if (ret < 0) {
		dev_err(extcon->dev, "register notifer fail\n");
		return -EINVAL;
	}

	extcon->tcpc_dev = tcpc_dev;

	return 0;
}
#endif

static void mtk_usb_extcon_detect_cable(struct work_struct *work)
{
	struct mtk_extcon_info *extcon = container_of(to_delayed_work(work),
		struct mtk_extcon_info, wq_detcable);
	enum usb_role role;
	int id, vbus;

	/* check ID and VBUS */
	id = extcon->id_gpiod ?
		gpiod_get_value_cansleep(extcon->id_gpiod) : 1;
	vbus = extcon->vbus_gpiod ?
		gpiod_get_value_cansleep(extcon->vbus_gpiod) : id;

	/* check if vbus detect by charger */
	if (extcon->usb_psy)
		vbus = 0;

	if (!id)
		role = USB_ROLE_HOST;
	else if (vbus)
		role = USB_ROLE_DEVICE;
	else
		role = USB_ROLE_NONE;

	dev_dbg(extcon->dev, "id %d, vbus %d, set role: %s\n",
			id, vbus, usb_role_string(role));

	if (role == USB_ROLE_HOST)
		mtk_usb_extcon_set_vbus(extcon, true);
	else
		mtk_usb_extcon_set_vbus(extcon, false);

	mtk_usb_extcon_set_role(extcon, role);
}

static irqreturn_t mtk_usb_gpio_handle(int irq, void *dev_id)
{
	struct mtk_extcon_info *extcon = dev_id;

	/* issue detection work */
	queue_delayed_work(system_power_efficient_wq, &extcon->wq_detcable, 0);

	return IRQ_HANDLED;
}

static int mtk_usb_extcon_gpio_init(struct mtk_extcon_info *extcon)
{
	struct device *dev = extcon->dev;
	int ret = 0;

	extcon->id_gpiod = devm_gpiod_get_optional(dev, "id", GPIOD_IN);
	if (IS_ERR(extcon->id_gpiod)) {
		dev_info(dev, "get id gpio err\n");
		return PTR_ERR(extcon->id_gpiod);
	}

	extcon->vbus_gpiod = devm_gpiod_get_optional(dev, "vbus", GPIOD_IN);
	if (IS_ERR(extcon->vbus_gpiod)) {
		dev_info(dev, "get vbus gpio err\n");
		return PTR_ERR(extcon->vbus_gpiod);
	}

	if (!extcon->id_gpiod && !extcon->vbus_gpiod) {
		dev_info(dev, "failed to get gpios\n");
		return -ENODEV;
	}

	if (extcon->id_gpiod)
		ret = gpiod_set_debounce(extcon->id_gpiod, USB_GPIO_DEB_US);
	if (extcon->vbus_gpiod)
		ret = gpiod_set_debounce(extcon->vbus_gpiod, USB_GPIO_DEB_US);

	INIT_DELAYED_WORK(&extcon->wq_detcable, mtk_usb_extcon_detect_cable);

	if (extcon->id_gpiod) {
		extcon->id_irq = gpiod_to_irq(extcon->id_gpiod);
		if (extcon->id_irq < 0) {
			dev_info(dev, "failed to get ID IRQ\n");
			return extcon->id_irq;
		}
		ret = devm_request_threaded_irq(dev, extcon->id_irq,
						NULL, mtk_usb_gpio_handle,
						USB_GPIO_IRQ_FLAG,
						dev_name(dev), extcon);
		if (ret < 0) {
			dev_info(dev, "failed to request ID IRQ\n");
			return ret;
		}
	}

	if (extcon->vbus_gpiod) {
		extcon->vbus_irq = gpiod_to_irq(extcon->vbus_gpiod);
		if (extcon->vbus_irq < 0) {
			dev_info(dev, "failed to get VBUS IRQ\n");
			return extcon->vbus_irq;
		}
		ret = devm_request_threaded_irq(dev, extcon->vbus_irq,
						NULL, mtk_usb_gpio_handle,
						USB_GPIO_IRQ_FLAG,
						dev_name(dev), extcon);
		if (ret < 0) {
			dev_info(dev, "failed to request VBUS IRQ\n");
			return ret;
		}
	}

	/* get id/vbus pin value when boot on */
	queue_delayed_work(system_power_efficient_wq,
			   &extcon->wq_detcable,
			   msecs_to_jiffies(10));

	device_init_wakeup(dev, true);

	return 0;
}

#if IS_ENABLED(CONFIG_TCPC_CLASS)
#define PROC_FILE_SMT "mtk_typec"
#define FILE_SMT_U2_CC_MODE "smt_u2_cc_mode"

static int usb_cc_smt_procfs_show(struct seq_file *s, void *unused)
{
	struct mtk_extcon_info *extcon = s->private;
	struct device_node *np = extcon->dev->of_node;
	const char *tcpc_name;
	uint8_t cc1, cc2;
	int ret;

	ret = of_property_read_string(np, "tcpc", &tcpc_name);
	if (ret < 0)
		return -ENODEV;

	extcon->tcpc_dev = tcpc_dev_get_by_name(tcpc_name);
	if (!extcon->tcpc_dev)
		return -ENODEV;

	tcpm_inquire_remote_cc(extcon->tcpc_dev, &cc1, &cc2, false);
	dev_info(extcon->dev, "cc1=%d, cc2=%d\n", cc1, cc2);

	if (cc1 == TYPEC_CC_VOLT_OPEN || cc1 == TYPEC_CC_DRP_TOGGLING)
		seq_puts(s, "0\n");
	else if (cc2 == TYPEC_CC_VOLT_OPEN || cc2 == TYPEC_CC_DRP_TOGGLING)
		seq_puts(s, "0\n");
	else
		seq_puts(s, "1\n");

	return 0;
}

static int usb_cc_smt_procfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, usb_cc_smt_procfs_show, pde_data(inode));
}

static const struct  proc_ops usb_cc_smt_procfs_fops = {
	.proc_open = usb_cc_smt_procfs_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int mtk_usb_extcon_procfs_init(struct mtk_extcon_info *extcon)
{
	struct proc_dir_entry *file, *root;
	int ret = 0;

	root = proc_mkdir(PROC_FILE_SMT, NULL);
	if (!root) {
		dev_info(extcon->dev, "fail creating proc dir: %s\n",
			PROC_FILE_SMT);
		ret = -ENOMEM;
		goto error;
	}

	file = proc_create_data(FILE_SMT_U2_CC_MODE, 0400, root,
		&usb_cc_smt_procfs_fops, extcon);
	if (!file) {
		dev_info(extcon->dev, "fail creating proc file: %s\n",
			FILE_SMT_U2_CC_MODE);
		ret = -ENOMEM;
		goto error;
	}

	dev_info(extcon->dev, "success creating proc file: %s\n",
		FILE_SMT_U2_CC_MODE);

error:
	dev_info(extcon->dev, "%s ret:%d\n", __func__, ret);
	return ret;
}
#endif

static int mtk_usb_extcon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_extcon_info *extcon;
#if IS_ENABLED(CONFIG_TCPC_CLASS)
	const char *tcpc_name;
#endif
	int ret;

	extcon = devm_kzalloc(&pdev->dev, sizeof(*extcon), GFP_KERNEL);
	if (!extcon)
		return -ENOMEM;

	extcon->dev = dev;

	/* usb role switch */
	extcon->role_sw = usb_role_switch_get(extcon->dev);
	if (IS_ERR(extcon->role_sw)) {
		mca_log_err("failed to get usb role\n");
		return PTR_ERR(extcon->role_sw);
	}

	/* initial usb role */
	if (extcon->role_sw)
		extcon->c_role = USB_ROLE_NONE;
	else {
		/* extcon */
		extcon->edev = devm_extcon_dev_allocate(dev, usb_extcon_cable);
		if (IS_ERR(extcon->edev)) {
			dev_info(dev, "failed to allocate extcon device\n");
			return -ENOMEM;
		}

		ret = devm_extcon_dev_register(dev, extcon->edev);
		if (ret < 0) {
			dev_info(dev, "failed to register extcon device\n");
			return ret;
		}
	}

	/* vbus */
	ret = mtk_usb_extcon_vbus_init(extcon);
	if (ret < 0)
		mca_log_err("failed to init vbus\n");

	extcon->bypss_typec_sink =
		of_property_read_bool(dev->of_node,
			"mediatek,bypss-typec-sink");

#if IS_ENABLED(CONFIG_TCPC_CLASS)
	ret = of_property_read_string(dev->of_node, "tcpc", &tcpc_name);
	if (of_property_read_bool(dev->of_node, "mediatek,u2") && ret == 0
		&& strcmp(tcpc_name, "type_c_port0") == 0) {
		mtk_usb_extcon_procfs_init(extcon);
	}
#endif

	extcon->extcon_wq = create_singlethread_workqueue("extcon_usb");
	if (!extcon->extcon_wq)
		return -ENOMEM;

	/* power psy */
	ret = mtk_usb_extcon_psy_init(extcon);
	if (ret < 0)
		mca_log_err("failed to init psy\n");

	/* get id/vbus gpio resources */
	ret = mtk_usb_extcon_gpio_init(extcon);
	if (ret < 0)
		dev_info(dev, "failed to init id/vbus pin\n");

#if IS_ENABLED(CONFIG_TCPC_CLASS)
	/* tcpc */
	ret = mtk_usb_extcon_tcpc_init(extcon);
	if (ret < 0)
		mca_log_err("failed to init tcpc\n");
#endif

	platform_set_drvdata(pdev, extcon);

	return 0;
}

static int mtk_usb_extcon_remove(struct platform_device *pdev)
{
	return 0;
}

static void mtk_usb_extcon_shutdown(struct platform_device *pdev)
{
	struct mtk_extcon_info *extcon = platform_get_drvdata(pdev);
	struct charger_device *cp_master = get_charger_by_name("cp_master");
	struct charger_device *chg_dev = get_charger_by_name("primary_chg");
	dev_info(extcon->dev, "shutdown\n");
	if(cp_master){
#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
		mca_log_err("turn off mos\n");
		charger_dev_enable_cp_wpc_gate(cp_master, true);
		charger_dev_enable_cp_usb_gate(cp_master, true);
#endif
		charger_dev_enable_acdrv_manual(cp_master, false);
	}
	if(chg_dev){
		charger_dev_enable_powerpath(chg_dev, true);
	}
	mtk_usb_extcon_set_vbus(extcon, false);
}

static int __maybe_unused mtk_usb_extcon_suspend(struct device *dev)
{
	struct mtk_extcon_info *extcon = dev_get_drvdata(dev);

	if (device_may_wakeup(dev)) {
		if (extcon->id_gpiod)
			enable_irq_wake(extcon->id_irq);
		if (extcon->vbus_gpiod)
			enable_irq_wake(extcon->vbus_irq);
		return 0;
	}

	if (extcon->id_gpiod)
		disable_irq(extcon->id_irq);
	if (extcon->vbus_gpiod)
		disable_irq(extcon->vbus_irq);

	pinctrl_pm_select_sleep_state(dev);

	return 0;
}

static int __maybe_unused mtk_usb_extcon_resume(struct device *dev)
{
	struct mtk_extcon_info *extcon = dev_get_drvdata(dev);

	if (device_may_wakeup(dev)) {
		if (extcon->id_gpiod)
			disable_irq_wake(extcon->id_irq);
		if (extcon->vbus_gpiod)
			disable_irq_wake(extcon->vbus_irq);
		return 0;
	}

	pinctrl_pm_select_default_state(dev);

	if (extcon->id_gpiod)
		enable_irq(extcon->id_irq);
	if (extcon->vbus_gpiod)
		enable_irq(extcon->vbus_irq);

	if (extcon->id_gpiod || extcon->vbus_gpiod)
		queue_delayed_work(system_power_efficient_wq,
				   &extcon->wq_detcable, 0);

	return 0;
}

static SIMPLE_DEV_PM_OPS(mtk_usb_extcon_pm_ops,
			mtk_usb_extcon_suspend, mtk_usb_extcon_resume);

static const struct of_device_id mtk_usb_extcon_of_match[] = {
	{ .compatible = "mediatek,extcon-usb", },
	{ },
};
MODULE_DEVICE_TABLE(of, mtk_usb_extcon_of_match);

static struct platform_driver mtk_usb_extcon_driver = {
	.probe		= mtk_usb_extcon_probe,
	.remove		= mtk_usb_extcon_remove,
	.shutdown	= mtk_usb_extcon_shutdown,
	.driver		= {
		.name	= "mtk-extcon-usb",
		.pm	= &mtk_usb_extcon_pm_ops,
		.of_match_table = mtk_usb_extcon_of_match,
	},
};

static int __init mtk_usb_extcon_init(void)
{
	return platform_driver_register(&mtk_usb_extcon_driver);
}
late_initcall(mtk_usb_extcon_init);

static void __exit mtk_usb_extcon_exit(void)
{
	platform_driver_unregister(&mtk_usb_extcon_driver);
}
module_exit(mtk_usb_extcon_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Extcon USB Driver");

