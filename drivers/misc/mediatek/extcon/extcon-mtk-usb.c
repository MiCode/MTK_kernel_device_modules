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
#include <linux/completion.h>

#include "extcon-mtk-usb.h"
#include "mtk_charger.h"
#include "pd_cp_manager.h"
#include "charger_class.h"

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

#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
static unsigned int global_port = 0;
static unsigned int global_pd_connect_port = 0;
static unsigned int global_sink_port = 0;
static unsigned int global_after_sink_port = 0;
static unsigned int global_bc12_port = 0;

static void switch_usb_port(struct mtk_extcon_info *extcon, bool port0) {
	int ret = 0;
	if (extcon->enable && extcon->disable) {
		// 选择长边口
		if (port0) {
			ret = pinctrl_select_state(extcon->pinctrl, extcon->disable);
			if (ret < 0)
				dev_info(extcon->dev, "failed to select usb0\n");
		} else {
			// 选择短边口
			ret = pinctrl_select_state(extcon->pinctrl, extcon->enable);
			if (ret < 0)
				dev_info(extcon->dev, "failed to select usb1\n");
		}
	}
}

#endif

int usb_port_enable = PORT0_ENABLE;

int usb_port_use(void)
{
	return usb_port_enable == PORT1_ENABLE ? 1 : 0;
}
EXPORT_SYMBOL(usb_port_use);

static void mtk_usb_extcon_update_role(struct work_struct *work)
{
	struct usb_role_info *role = container_of(to_delayed_work(work),
					struct usb_role_info, dwork);
	struct mtk_extcon_info *extcon = role->extcon;
	unsigned int cur_dr, new_dr;

#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
	mca_log_err("port-%d cur_dr(%d) new_dr(%d)\n", extcon->cur_port , extcon->c_role,  role->d_role);

	if (role->d_role != USB_ROLE_NONE) {
		// 单口插入场景: 长边口插入设备
		if ((extcon->cur_port & PORT0_ENABLE) && !(extcon->cur_port & PORT1_ENABLE)) {
			mca_log_err("switch port1");
			switch_usb_port(extcon, true);
			extcon->enable_port = PORT0_ENABLE;
			usb_port_enable = PORT0_ENABLE;
		// 单口插入场景：短边口插入设备
		} else if ((extcon->cur_port & PORT1_ENABLE) && !(extcon->cur_port & PORT0_ENABLE)) {
			mca_log_err("switch port2");
			switch_usb_port(extcon, false);
			extcon->enable_port = PORT1_ENABLE;
			usb_port_enable = PORT1_ENABLE;
		}  else {
			// 双口插入场景
			mca_log_err("global_pd_connect_port = %d, enable_port = %d \n", global_pd_connect_port, extcon->enable_port);
			if (global_pd_connect_port > 0) {
				if (extcon->enable_port & global_pd_connect_port) {
					if (extcon->enable_port & PORT0_ENABLE) {
						mca_log_err("force switch port2");
						switch_usb_port(extcon, false);
						extcon->enable_port = PORT1_ENABLE;
						usb_port_enable = PORT1_ENABLE;
					} else if (extcon->enable_port & PORT1_ENABLE) {
						mca_log_err("force switch port1");
						switch_usb_port(extcon, true);
						extcon->enable_port = PORT0_ENABLE;
						usb_port_enable = PORT0_ENABLE;
					}
				}
			} else {
				extcon->delay_mode = role->d_role;
				mca_log_err("not set usb mode, delay set mode = %d \n", extcon->delay_mode);
				return;
			}
		}
	} else {
		if (!((extcon->cur_port & PORT0_ENABLE) || (extcon->cur_port & PORT1_ENABLE))) {
			mca_log_err(" port- %d not connect, set defalut port0\n", extcon->cur_port);
			switch_usb_port(extcon, true);
			extcon->delay_mode = USB_ROLE_NONE;
			extcon->enable_port = PORT0_ENABLE;
			usb_port_enable = PORT0_ENABLE;
		} else {
			mca_log_err("not set usb mode NONE, delay_mode = %d, enable_port = %d \n", extcon->delay_mode, extcon->enable_port);
			if (extcon->delay_mode != USB_ROLE_NONE && !(extcon->enable_port & extcon->cur_port) && global_pd_connect_port == 0) {
				mtk_usb_extcon_set_role(extcon, extcon->delay_mode);
			}
			return;
		}
	}
#endif


	cur_dr = extcon->c_role;
	new_dr = role->d_role;

	extcon->c_role = new_dr;
	global_role = new_dr;

	mca_log_err("port-%d cur_dr(%d) new_dr(%d)\n", extcon->cur_port , cur_dr, new_dr);

	/* none -> device */
	if (cur_dr == USB_ROLE_NONE &&
			new_dr == USB_ROLE_DEVICE) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB, true);
	/* none -> host */
	} else if (cur_dr == USB_ROLE_NONE &&
			new_dr == USB_ROLE_HOST) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB_HOST, true);
	/* device -> none */
	} else if (cur_dr == USB_ROLE_DEVICE &&
			new_dr == USB_ROLE_NONE) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB, false);
	/* host -> none */
	} else if (cur_dr == USB_ROLE_HOST &&
			new_dr == USB_ROLE_NONE) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB_HOST, false);
	/* device -> host */
	} else if (cur_dr == USB_ROLE_DEVICE &&
			new_dr == USB_ROLE_HOST) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB, false);
		extcon_set_state_sync(extcon->edev,	EXTCON_USB_HOST, true);
	/* host -> device */
	} else if (cur_dr == USB_ROLE_HOST &&
			new_dr == USB_ROLE_DEVICE) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB_HOST, false);
		extcon_set_state_sync(extcon->edev,	EXTCON_USB, true);
	}

	/* usb role switch */
	if (extcon->role_sw)
		usb_role_switch_set_role(extcon->role_sw, new_dr);

	kfree(role);
}


int mtk_usb_extcon_set_role(struct mtk_extcon_info *extcon,
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

static bool usb_is_online(struct mtk_extcon_info *extcon)
{
	union power_supply_propval pval;
	union power_supply_propval tval;
	int input_suspend = 0;
	int ret;

	ret = usb_get_property(USB_PROP_INPUT_SUSPEND, &input_suspend);
	if (ret < 0) {
		dev_info(extcon->dev, "failed to get input_suspend\n");
		return false;
	}

	ret = power_supply_get_property(extcon->usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &pval);
	if (ret < 0) {
		dev_info(extcon->dev, "failed to get online prop\n");
		return false;
	}

	ret = power_supply_get_property(extcon->usb_psy,
				POWER_SUPPLY_PROP_TYPE, &tval);
	if (ret < 0) {
		dev_info(extcon->dev, "failed to get usb type\n");
		return false;
	}

	mca_log_err("input_suspend= %d, online=%d, type=%d\n", input_suspend, pval.intval, tval.intval);

	if ((pval.intval && (tval.intval == POWER_SUPPLY_TYPE_USB ||
			tval.intval == POWER_SUPPLY_TYPE_USB_CDP)) ||
		(input_suspend && (tval.intval == POWER_SUPPLY_TYPE_USB ||
			tval.intval == POWER_SUPPLY_TYPE_USB_CDP)))
		return true;
	else
		return false;
}

#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
static void yili_mtk_usb_extcon_psy_detector(struct work_struct *work)
{
	struct mtk_extcon_info *extcon = container_of(to_delayed_work(work),
		struct mtk_extcon_info, wq_psy);

	union power_supply_propval pval;
	int ret;
	int input_suspend = 0;
	int	active_port = 0;

	ret = power_supply_get_property(extcon->usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &pval);
	if (ret < 0) {
		dev_info(extcon->dev, "failed to get online prop\n");
		return;
	}

	ret = usb_get_property(USB_PROP_INPUT_SUSPEND, &input_suspend);
	if (ret < 0) {
		dev_info(extcon->dev, "failed to get input_suspend\n");
		return;
	}

	if (global_sink_port <= 0 || global_sink_port > 3) {
		mca_log_err("invalied global_sink_port = %d\n", global_sink_port);
		return;
	}

	mca_log_err("prot-%d online=%d, input_suspend = %d", global_sink_port, pval.intval, input_suspend);
	/* Workaround for PR_SWAP, IF tcpc_dev, then do not switch role. */
	/* Since we will set USB to none when type-c plug out */
	/* 1. intval 为 0，直接清空角色 */
	if (!pval.intval && !input_suspend) {
		mtk_usb_extcon_set_role(extcon, USB_ROLE_NONE);
		return;
	}

	/* 2. 决定 active_port & 更新 global_bc12_port */
	if ((global_sink_port & PORT0_ENABLE) &&
		(global_sink_port & PORT1_ENABLE)) {

		/* 双 sink 场景 */
		if (global_bc12_port == 0) {
			if (global_after_sink_port & PORT0_ENABLE)
				global_bc12_port |= PORT1_ENABLE;
			else if (global_after_sink_port & PORT1_ENABLE)
				global_bc12_port |= PORT0_ENABLE;

			active_port = global_bc12_port;
		} else {
			global_bc12_port |= global_after_sink_port;
			active_port = global_after_sink_port;
		}

	} else {
		/* 非双 sink 场景 */
		global_bc12_port |= global_sink_port;
		active_port = global_sink_port;
	}

	/* 3. USB offline，统一处理 */
	if(!usb_is_online(extcon)) {
		mtk_usb_extcon_set_role(extcon, USB_ROLE_NONE);
		return;
	}

	/* 4. USB online 的统一路径 */
	if (extcon->data_port > 0 && !(extcon->data_port & active_port)) {
		usb_set_property(USB_PROP_TYPEC_PORTX_DPDM_ATTACH, 1);
	}

	extcon->data_port |= active_port;
	extcon->cur_port  |= active_port;

	if(extcon->c_role == USB_ROLE_HOST
			&& (extcon->cur_port == PORT0_ENABLE
			||  extcon->cur_port == PORT1_ENABLE))
	{
		mca_log_err("single port in host mode, skip set mode\n");
		return;
	}

	mtk_usb_extcon_set_role(extcon, USB_ROLE_DEVICE);
	mca_log_err("port-%d psy wake up set mode\n", extcon->cur_port);
}
#else

static void mtk_usb_extcon_psy_detector(struct work_struct *work)
{
	struct mtk_extcon_info *extcon = container_of(to_delayed_work(work),
		struct mtk_extcon_info, wq_psy);

	union power_supply_propval pval;
	union power_supply_propval tval;
	int input_suspend = 0;
	int ret;

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

	mca_log_err("online=%d, input_suspend=%d, type=%d, c_role=%d, global_role=%d\n",
		pval.intval, input_suspend, tval.intval, extcon->c_role, global_role);

	/* Workaround for PR_SWAP, IF tcpc_dev, then do not switch role. */
	/* Since we will set USB to none when type-c plug out */

	#if IS_ENABLED(CONFIG_TCPC_CLASS)
	if (extcon->tcpc_dev) {
		if (usb_is_online(extcon) && extcon->c_role == USB_ROLE_NONE)
			mtk_usb_extcon_set_role(extcon, USB_ROLE_DEVICE);
	} else {
	#endif
		if (usb_is_online(extcon))
			mtk_usb_extcon_set_role(extcon, USB_ROLE_DEVICE);
		else
			mtk_usb_extcon_set_role(extcon, USB_ROLE_NONE);
#if IS_ENABLED(CONFIG_TCPC_CLASS)
	}
#endif

	/* Workaround for PR_SWAP, Host mode should not come to this function. */
	if ((extcon->c_role == USB_ROLE_HOST ||
		(tval.intval != POWER_SUPPLY_TYPE_USB && tval.intval != POWER_SUPPLY_TYPE_USB_CDP && pval.intval) ||
		global_role != extcon->c_role)) {
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
#endif

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
#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
	INIT_DELAYED_WORK(&extcon->wq_psy, yili_mtk_usb_extcon_psy_detector);
#else
	INIT_DELAYED_WORK(&extcon->wq_psy, mtk_usb_extcon_psy_detector);
#endif

	extcon->psy_nb.notifier_call = mtk_usb_extcon_psy_notifier;
	ret = power_supply_reg_notifier(&extcon->psy_nb);
	if (ret)
		mca_log_err("fail to register notifer\n");
fail:
	return ret;
}

__maybe_unused
static void xm_enable_reverse_chg(struct charger_device *cp_master,
                                      struct mtk_extcon_info *extcon, bool revchg_is_on)
{
	static bool revchg_last_on = false;
	struct charger_device *chg_dev = get_charger_by_name("primary_chg");

	if (revchg_last_on == revchg_is_on) {
		if (!revchg_is_on)
			usb_set_property(USB_PROP_TYPEC_REVERSE_CHG, 0);
		return;
	}
	if (!chg_dev) {
		mca_log_err("failed to get chg_dev");
		return;
	}

	if (revchg_is_on) {
		// enable revert cp boost
		mca_log_err("[REV_CHG] start revert 1_2 cp");
		charger_dev_enable_acdrv_manual(cp_master, true);
		if (charger_dev_cp_set_revchg(cp_master, true)) {
			mca_log_err("set cp config failed");
		}
	} else {
		mca_log_err("[REV_CHG] end rev chg");
		if (charger_dev_cp_set_revchg(cp_master, false)) {
			mca_log_err("restore cp config failed");
		}
		charger_dev_enable_acdrv_manual(cp_master, false);
		usb_set_property(USB_PROP_TYPEC_REVERSE_CHG, 0);
	}
	revchg_last_on = revchg_is_on;
}
#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
static void xm_double_ports_enable_reverse_chg(struct charger_device *cp_master,
                                      struct mtk_extcon_info *extcon, bool revchg_is_on)
{
	static bool revchg_last_on = false;
	struct charger_device *chg_dev = get_charger_by_name("primary_chg");

	if (revchg_last_on == revchg_is_on)
		return;

	if (!chg_dev) {
		mca_log_err("failed to get chg_dev");
		return;
	}

	if (revchg_is_on) {
		charger_dev_enable_powerpath(chg_dev, false);
		// enable revert cp boost
		mca_log_err("[REV_CHG] port-%d start revert 1_2 cp", extcon->cur_port);
		charger_dev_enable_acdrv_manual(cp_master, true);
		charger_dev_cp_set_mode(cp_master, SC8541_REVERSE_1_2_CONVERTER_MODE);
		charger_dev_cp_device_init(cp_master, 1);
		charger_dev_cp_rev_chg_config(cp_master, true);
		if ((extcon->cur_port & PORT0_ENABLE) && (global_port & PORT0_ENABLE)) {
			charger_dev_enable_cp_wpc_gate(cp_master, false);
			charger_dev_enable_cp_usb_gate(cp_master, true);
		} else if ((extcon->cur_port & PORT1_ENABLE) && (global_port & PORT1_ENABLE)) {
			charger_dev_enable_cp_usb_gate(cp_master, false);
			charger_dev_enable_cp_wpc_gate(cp_master, true);
		}
		//charger_dev_cp_set_qb(cp_master, true);
		msleep(100);
		charger_dev_enable(cp_master, true);
		charger_dev_cp_dump_register(cp_master);
	} else {
		mca_log_err("port-%d [REV_CHG] end rev chg", extcon->cur_port);
		charger_dev_enable(cp_master, false);
		//charger_dev_cp_set_qb(cp_master, false);
		charger_dev_cp_set_mode(cp_master, SC8541_FORWARD_2_1_CHARGER_MODE);
		charger_dev_cp_dump_register(cp_master);
		charger_dev_cp_rev_chg_config(cp_master, false);
		usb_set_property(USB_PROP_TYPEC_REVERSE_CHG, 0);
		charger_dev_enable_powerpath(chg_dev, true);
	}

	revchg_last_on = revchg_is_on;
	return;
}

static int yili_mtk_usb_extcon_set_vbus(struct mtk_extcon_info *extcon,
							bool is_on)
{
	//struct regulator *vbus = extcon->vbus;
	struct device *dev = extcon->dev;
	struct charger_device *cp_master;
	int ret;
	static int last_rqc_vbus = 0;
	int rev_quick_chg = 0;
	int typec_port1 = 0;
	int typec_port0 = 0;
	int otg_burn_stat = 0;
	int short_otg_burn_stat = 0;

	/* vbus is optional */
	usb_get_property(USB_PROP_TYPEC_REVERSE_CHG, &rev_quick_chg);
	mca_log_err("rev_quick_chg=%d, last_rqc_vbus=%d, request_vbus = %d\n",
		rev_quick_chg, last_rqc_vbus, extcon->vbus_vol_request);


	usb_get_property(USB_PROP_TYPEC_PORT0_PLUGIN, &typec_port0);
	usb_get_property(USB_PROP_TYPEC_PORT1_PLUGIN, &typec_port1);

	mca_log_err("typec_port0 = %d, typec_port1 = %d\n", typec_port0, typec_port1);

	cp_master = get_charger_by_name("cp_master");
	if (!cp_master) {
		mca_log_err("%s: failed to get cp_master\n", __func__);
		return -1;
	}

	mca_log_err("port-%d vbus turn %s\n", extcon->cur_port, is_on ? "on" : "off");

	if (is_on) {
		charger_dev_enable_acdrv_manual(cp_master, true);
		if (!rev_quick_chg) {
			mca_log_err("[BOOST] start 5V reverse chg\n");
			if ((extcon->cur_port & PORT0_ENABLE) && (global_port & PORT0_ENABLE)) {
				charger_dev_enable_cp_usb_gate(cp_master, false);
				usb_get_property(USB_PROP_OTG_BURN_STATUS, &otg_burn_stat);
				if (!otg_burn_stat)
					gpio_set_value(extcon->vdd_boost_en_gpio_a, is_on);
				if (typec_port1 != 3)
					charger_dev_enable_cp_wpc_gate(cp_master, true);
				usb_set_property(USB_PROP_TYPEC_PORT0_PLUGIN, 3);
				mca_log_err("[BOOST] vdd_boost_en_gpio_a enable, otg_burn_stat: %d\n", otg_burn_stat);
			}

			if ((extcon->cur_port & PORT1_ENABLE) && (global_port & PORT1_ENABLE)) {
				charger_dev_enable_cp_wpc_gate(cp_master, false);
				usb_get_property(USB_PROP_SHORT_OTG_BURN_STATUS, &short_otg_burn_stat);
				if (!short_otg_burn_stat)
					gpio_set_value(extcon->vdd_boost_en_gpio_b, is_on);
				if (typec_port0 != 3)
					charger_dev_enable_cp_usb_gate(cp_master, true);
				usb_set_property(USB_PROP_TYPEC_PORT1_PLUGIN, 3);
				mca_log_err("[BOOST] vdd_boost_en_gpio_b enable, short_otg_burn_stat: %d\n", short_otg_burn_stat);
			}

			usb_set_property(USB_PROP_TYPEC_RQC_CONDITION_CHECK, 1);
			if (last_rqc_vbus > extcon->vbus_vol_request) {
				xm_double_ports_enable_reverse_chg(cp_master, extcon, rev_quick_chg);
			}
		} else {
			usb_set_property(USB_PROP_TYPEC_RQC_CONDITION_CHECK, 1);
			if (extcon->vbus_vol_request > 7000) {
				mca_log_err("[BOOST] start 9V reverse chg\n");
				xm_double_ports_enable_reverse_chg(cp_master, extcon, rev_quick_chg);
			}
		}
	} else {
		extcon->vbus_cur_inlimit = 0;
		xm_double_ports_enable_reverse_chg(cp_master, extcon, false);
		usb_set_property(USB_PROP_TYPEC_RQC_CONDITION_CHECK, 0);
		mca_log_err("HI MI when set vbus extcon->attached_now=%d,extcon->cur_port =%d",extcon->attached_now,extcon->cur_port);
		if (extcon->cur_port & PORT0_ENABLE && (global_port & PORT0_ENABLE)) {
			gpio_set_value(extcon->vdd_boost_en_gpio_a, is_on);
			charger_dev_enable_cp_usb_gate(cp_master, true);
			usb_set_property(USB_PROP_TYPEC_PORT0_PLUGIN, 0);
			if (typec_port0 == 3 && !(extcon->attached_now & PORT0_ENABLE)) {
				extcon->cur_port &= ~PORT0_ENABLE;
				mtk_usb_extcon_set_role(extcon, USB_ROLE_NONE);
				extcon->data_port &= ~PORT0_ENABLE;
				usb_set_property(USB_PROP_TYPEC_PORTX_DPDM_ATTACH, 0);
			}
			else
				mca_log_err("HI MI maybe we attched a dongle,so we do not set porta none\n");
			mca_log_err("[BOOST] vdd_boost_en_gpio_a disable\n");
		}
		if (extcon->cur_port & PORT1_ENABLE && (global_port & PORT1_ENABLE)) {
			gpio_set_value(extcon->vdd_boost_en_gpio_b, is_on);
			charger_dev_enable_cp_wpc_gate(cp_master, true);
			usb_set_property(USB_PROP_TYPEC_PORT1_PLUGIN, 0);
			if (typec_port1 == 3 && !(extcon->attached_now & PORT1_ENABLE)) {
				extcon->cur_port &= ~PORT1_ENABLE;
				extcon->data_port &= ~PORT1_ENABLE;
				usb_set_property(USB_PROP_TYPEC_PORTX_DPDM_ATTACH, 0);
				mtk_usb_extcon_set_role(extcon, USB_ROLE_NONE);
			}
			else
				mca_log_err("HI MI maybe we attched a dongle,so we do not set portb none\n");
			mca_log_err("[BOOST] vdd_boost_en_gpio_b disable\n");
		}
		charger_dev_enable_acdrv_manual(cp_master, false);

		mca_log_err("vbus regulator disable\n");
	}


	ret = usb_set_property(USB_PROP_OTG_ENABLE, is_on);
	if (ret < 0)
		dev_info(dev, "failed to set otg enable\n");
	extcon->vbus_on = is_on;
	last_rqc_vbus = extcon->vbus_vol_request;
	return 0;
}
#else
static int mtk_usb_extcon_set_vbus(struct mtk_extcon_info *extcon,
							bool is_on)
{
	struct regulator *vbus = extcon->vbus;
	struct device *dev = extcon->dev;
	struct charger_device *cp_master;
	static int last_rqc_vbus = 0;
	int rev_quick_chg = 0;
	int otg_burn_status = 0;
	int rev_cable_status = 0;
	int rev_cable_boostable = 0;
	int rev_cable_boosted = 0;
	int pmic_vbus = 0;
	int ret;

	usb_get_property(USB_PROP_TYPEC_REVERSE_CHG, &rev_quick_chg);
	mca_log_err("rev_quick_chg=%d, last_rqc_vbus=%d, request_vbus = %d\n",
		rev_quick_chg, last_rqc_vbus, extcon->vbus_vol_request);
	usb_get_property(USB_PROP_TYPEC_REV_CABLE_CHECK, &rev_cable_status);
	rev_cable_boostable = (rev_cable_status >> 16) & 0xFFFF;
	rev_cable_boosted = rev_cable_status & 0xFFFF;
	mca_log_err("rev cable present=%d, effective=%d\n", rev_cable_boostable, rev_cable_boosted);

	if ((extcon->vbus_on == is_on &&
		!(rev_quick_chg || rev_cable_boostable || rev_cable_boosted) &&
		last_rqc_vbus == extcon->vbus_vol_request) || !vbus) {
			return 0;
	}

	cp_master = get_charger_by_name("cp_master");
	if (!cp_master) {
		mca_log_err("%s: failed to get cp_master\n", __func__);
		return -1;
	}
	mca_log_err("vbus turn %s\n", is_on ? "on" : "off");
	ret = usb_set_property(USB_PROP_OTG_ENABLE, is_on);
	if (ret < 0)
		dev_info(dev, "failed to set otg enable\n");

	if (is_on) {
		usb_set_property(USB_PROP_TYPEC_RQC_CONDITION_CHECK, 1);
		if (extcon->vbus_vol_request > 7000 &&
			(rev_quick_chg || (rev_cable_boostable && rev_cable_boosted))) {
			xm_enable_reverse_chg(cp_master, extcon, true);
		}
		if (last_rqc_vbus > extcon->vbus_vol_request &&
			!(rev_quick_chg || (rev_cable_boostable && rev_cable_boosted))) {
			xm_enable_reverse_chg(cp_master, extcon, false);
		}
		usb_get_property(USB_PROP_OTG_BURN_STATUS, &otg_burn_status);
		if (otg_burn_status) {
			mca_log_err("otg in burn status, no vbus on");
		}
		if (!(rev_quick_chg || (rev_cable_boostable && rev_cable_boosted))) {
			charger_dev_cp_set_mode(cp_master, SC8561_REVERSE_1_1_CONVERTER_MODE);
			charger_dev_enable_acdrv_manual(cp_master, true);
		}

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

		usb_get_property(USB_PROP_PMIC_VBUS, &pmic_vbus);
		mca_log_err("pmic vbus_vol: %d\n", pmic_vbus);
		if (!rev_quick_chg && extcon->vbus_vol_request == 5000 && pmic_vbus > 1000) {
			msleep(60);
			usb_get_property(USB_PROP_PMIC_VBUS, &pmic_vbus);
			mca_log_err("pmic vbus_vol: %d\n", pmic_vbus);
		}
		if (pmic_vbus < 3600 && !otg_burn_status) {
			ret = regulator_enable(vbus);
			if (ret) {
				mca_log_err("vbus regulator enable failed\n");
				return ret;
			}
		}
		mca_log_err("vbus regulator enable\n");
	} else {
		regulator_disable(vbus);
		charger_dev_cp_set_mode(cp_master, SC8561_FORWARD_4_1_CHARGER_MODE);
		charger_dev_enable_acdrv_manual(cp_master, false);
		xm_enable_reverse_chg(cp_master, extcon, false);
		usb_set_property(USB_PROP_TYPEC_RQC_CONDITION_CHECK, 0);
		usb_set_property(USB_PROP_TYPEC_REV_CABLE_CHECK, 0);
		/* Restore to default state */
		extcon->vbus_cur_inlimit = 0;
		mca_log_err("vbus regulator disable\n");
	}

	extcon->vbus_on = is_on;
	last_rqc_vbus = extcon->vbus_vol_request;
	return 0;
}
#endif

static ssize_t vbus_limit_cur_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mtk_extcon_info *extcon = dev_get_drvdata(dev);
	struct regulator *vbus = extcon->vbus;

	if (!vbus)
		return snprintf(buf, PAGE_SIZE, "0");
	else
		return snprintf(buf, PAGE_SIZE, "%d\n", extcon->vbus_cur_inlimit);
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
		return snprintf(buf, PAGE_SIZE, "0");
	else
		return snprintf(buf, PAGE_SIZE, "%d\n", extcon->vbus_on);
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

#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
	yili_mtk_usb_extcon_set_vbus(extcon, is_on);
#else
	mtk_usb_extcon_set_vbus(extcon, is_on);
#endif
	return count;
}

static DEVICE_ATTR_RW(vbus_switch);

static int mtk_usb_extcon_vbus_init(struct mtk_extcon_info *extcon)
{
	int ret = 0;
	struct device *dev = extcon->dev;

#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
	/* otg external boost probe first */
	extcon->vdd_boost_en_gpio_a = of_get_named_gpio(dev->of_node, "vdd_boost_5v_en_a", 0);
	if (!gpio_is_valid(extcon->vdd_boost_en_gpio_a)){
		dev_err(dev, "failed to parse vdd_boost_5v_en_a\n");
	} else {
		gpio_direction_output(extcon->vdd_boost_en_gpio_a, 0);
		dev_err(dev, "parse vdd_boost_5v_en_a : %d OK!\n", extcon->vdd_boost_en_gpio_a);
	}
	extcon->vdd_boost_en_gpio_b = of_get_named_gpio(dev->of_node, "vdd_boost_5v_en_b", 0);
	if (!gpio_is_valid(extcon->vdd_boost_en_gpio_b)){
		dev_err(dev, "failed to parse vdd_boost_5v_en_b\n");
	} else {
		gpio_direction_output(extcon->vdd_boost_en_gpio_b, 0);
		dev_err(dev, "parse vdd_boost_5v_en_b : %d OK!\n", extcon->vdd_boost_en_gpio_b);
	}
#endif

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

#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY

static int yili_mtk_extcon_tcpc_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct extcon_notifier_block *tcpc_nb =
		container_of(nb, struct extcon_notifier_block, nb);
	struct mtk_extcon_info *extcon = tcpc_nb->extcon;
	bool vbus_on;
	int idx = tcpc_nb - extcon->tcpc_nb;
	uint8_t cc1, cc2;

	if (idx < 0 || idx > 1) {
		mca_log_err("idx is invaled error\n");
		return -EINVAL;
	}

	mca_log_err("port-%d plugin event = %d\n", 1 << idx, event);

	mutex_lock(&extcon->mutex);

	global_port = 1 << idx;
	mca_log_err("global_port = %d plugin\n", global_port);

	switch (event) {
	case TCP_NOTIFY_SOURCE_VBUS:
		mca_log_err("port-%d source vbus = %dmv\n", 1 << idx,
				 noti->vbus_state.mv);
		vbus_on = (noti->vbus_state.mv) ? true : false;
		if (vbus_on) {
			extcon->cur_port |= 1 << idx;
		}
		extcon->vbus_vol_request = noti->vbus_state.mv;
		yili_mtk_usb_extcon_set_vbus(extcon, vbus_on);
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		if (!extcon->tcpc_dev[idx])
			return -ENODEV;
		tcpm_inquire_remote_cc(extcon->tcpc_dev[idx], &cc1, &cc2, false);
		if ((cc1 == TYPEC_CC_VOLT_RA && cc2 == TYPEC_CC_VOLT_OPEN) ||
				(cc1 == TYPEC_CC_VOLT_OPEN && cc2 == TYPEC_CC_VOLT_RA) ||
				(cc1 == TYPEC_CC_VOLT_RD && cc2 == TYPEC_CC_VOLT_OPEN) ||
				(cc1 == TYPEC_CC_VOLT_OPEN && cc2 == TYPEC_CC_VOLT_RD) ||
				(cc1 == TYPEC_CC_VOLT_RA && cc2 == TYPEC_CC_VOLT_RD) ||
				(cc1 == TYPEC_CC_VOLT_RD && cc2 == TYPEC_CC_VOLT_RA)) {
			if (extcon->data_port > 0 && !(extcon->data_port & (1 << idx))) {
				usb_set_property(USB_PROP_TYPEC_PORTX_DPDM_ATTACH, 1);
			}
			extcon->data_port |= 1 << idx;
		}
		mca_log_err("port-%d old_state=%d, new_state=%d, cc1=%d, cc2=%d\n", 1 << idx,
				noti->typec_state.old_state,
				noti->typec_state.new_state, cc1, cc2);

		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			mca_log_err("port-%d Type-C SRC plug in\n", 1 << idx);
			extcon->cur_port |= 1 << idx;
			mtk_usb_extcon_set_role(extcon, USB_ROLE_HOST);
			extcon->attached_now |= (1 << idx);
			mca_log_err("HI MI port extcon->attached_now =%d\n", extcon->attached_now);
		} else if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			(noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
			noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC ||
			noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			noti->typec_state.new_state == TYPEC_ATTACHED_DBGACC_SNK)) {
			mca_log_err("port-%d Type-C SINK plug in\n", 1 << idx);
			// mtk_usb_extcon_set_role(extcon, USB_ROLE_DEVICE);
			if (global_sink_port > 0) {
					global_after_sink_port = 1 << idx;
			}
			global_sink_port |= 1 << idx;
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_SNK ||
			noti->typec_state.old_state == TYPEC_ATTACHED_NORP_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_DBGACC_SNK) &&
			noti->typec_state.new_state == TYPEC_UNATTACHED) {
			int typec_port1 = 0;
			int typec_port0 = 0;
			usb_get_property(USB_PROP_TYPEC_PORT0_PLUGIN, &typec_port0);
			usb_get_property(USB_PROP_TYPEC_PORT1_PLUGIN, &typec_port1);
			mca_log_err("port-%d Type-C plug out, port0 status %d, port1 status %d \n", 1 << idx, typec_port0, typec_port1);
			if ((typec_port0 != 3 && idx == 0) || (typec_port1 != 3 && idx == 1)) {
				extcon->cur_port &= ~(1 << idx);
				extcon->data_port &= ~(1 << idx);
				usb_set_property(USB_PROP_TYPEC_PORTX_DPDM_ATTACH, 0);
			}
			global_pd_connect_port &= ~(1 << idx);
			global_sink_port &= ~(1 << idx);
			global_after_sink_port = 0;
			global_bc12_port &= ~(1 << idx);
			mtk_usb_extcon_set_role(extcon, USB_ROLE_NONE);
			extcon->attached_now &= ~(1 << idx);
			pr_err("HI MI port extcon->attached_now =%d\n", extcon->attached_now);
		}
		break;
	case TCP_NOTIFY_PD_STATE:
		mca_log_err("port-%d pd state = %d\n", 1 << idx, noti->pd_state.connected);
		if (noti->pd_state.connected == PD_CONNECT_PE_READY_SNK_PD30 ||
			noti->pd_state.connected == PD_CONNECT_PE_READY_SNK_APDO ||
			noti->pd_state.connected == PD_CONNECT_PE_READY_SNK) {
			global_pd_connect_port |= 1 << idx;
			mca_log_err("port-%d, PD CONNECTED, global_pd_connect_port = %d\n", 1 << idx, global_pd_connect_port);
		}
		break;
	case TCP_NOTIFY_DR_SWAP:
		mca_log_err("%s port-%d dr_swap, new role=%d\n",
				__func__, 1 << idx, noti->swap_state.new_role);
		if (noti->swap_state.new_role == PD_ROLE_UFP &&
				extcon->c_role == USB_ROLE_HOST) {
			mca_log_err("switch role to device\n");
			extcon->cur_port |= 1 << idx;
			mtk_usb_extcon_set_role(extcon, USB_ROLE_DEVICE);
		} else if (noti->swap_state.new_role == PD_ROLE_DFP &&
				(extcon->c_role == USB_ROLE_DEVICE || extcon->c_role == USB_ROLE_NONE || global_role != extcon->c_role)) {
			mca_log_err("switch role to host\n");
			extcon->cur_port |= 1 << idx;
			mtk_usb_extcon_set_role(extcon, USB_ROLE_HOST);
		} else
			mca_log_err("wrong condition\n");

		break;
	}

	mutex_unlock(&extcon->mutex);

	return NOTIFY_OK;
}

#define EXTCON_DEVM_KCALLOC(member)					\
	(extcon->member = devm_kcalloc(extcon->dev, extcon->nr_port,		\
				     sizeof(*extcon->member), GFP_KERNEL))\

static int yili_mtk_usb_extcon_tcpc_init(struct mtk_extcon_info *extcon)
{
	struct device_node *np = extcon->dev->of_node;
	char name[16];
	int ret, i;
	int retry_pcs = 0;

	ret = of_property_read_u32(np, "nr-port", &extcon->nr_port);
	if (ret < 0) {
		mca_log_err("%s read nr-port property fail(%d)\n", __func__, ret);
		extcon->nr_port = 1;
	}

	mca_log_err("%s  nr_port:%d ok\n", __func__, extcon->nr_port);

	EXTCON_DEVM_KCALLOC(tcpc_dev);
	EXTCON_DEVM_KCALLOC(tcpc_nb);
	for (i = 0; i < extcon->nr_port; i++) {
		ret = snprintf(name, sizeof(name), "type_c_port%d", i);
		if (ret >= sizeof(name))
			mca_log_err("%s type_c name is truncated\n", __func__);

		extcon->tcpc_dev[i] = tcpc_dev_get_by_name(name);
		retry_pcs = 0;
		while(!extcon->tcpc_dev[i] && retry_pcs <= 10){
			mca_log_err("%s port get %s retry %d fail\n", __func__, name ,retry_pcs);
			extcon->tcpc_dev[i] = tcpc_dev_get_by_name(name);
			retry_pcs++;
			msleep(50);
		}
		if(!extcon->tcpc_dev[i] && retry_pcs > 10)
		{
			mca_log_err("%s port get %s retry over ten times\n", __func__, name);
			return -ENODEV;
		}

		mca_log_err("%s  nr_port:%d  get %s  ok\n", __func__, extcon->nr_port, name);

		extcon->tcpc_nb[i].nb.notifier_call = yili_mtk_extcon_tcpc_notifier;
		extcon->tcpc_nb[i].extcon = extcon;
		ret = register_tcp_dev_notifier(extcon->tcpc_dev[i], &extcon->tcpc_nb[i].nb,
			TCP_NOTIFY_TYPE_USB | TCP_NOTIFY_TYPE_VBUS |
			TCP_NOTIFY_TYPE_MISC);
		if (ret < 0) {
			dev_err(extcon->dev, "%s register port%d notifier fail(%d)", __func__, i, ret);
			return -EINVAL;
		}
	}
	return 0;
}
#else

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
		mca_log_err("source vbus = %dmv\n",
				 noti->vbus_state.mv);
		vbus_on = (noti->vbus_state.mv) ? true : false;
		extcon->vbus_vol_request = noti->vbus_state.mv;
		mtk_usb_extcon_set_vbus(extcon, vbus_on);
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		mca_log_err("old_state=%d, new_state=%d\n",
				noti->typec_state.old_state,
				noti->typec_state.new_state);
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			mca_log_err("Type-C SRC plug in\n");
			mtk_usb_extcon_set_role(extcon, USB_ROLE_HOST);
		} else if (!(extcon->bypss_typec_sink) &&
			noti->typec_state.old_state == TYPEC_UNATTACHED &&
			(noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
			noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC ||
			noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			noti->typec_state.new_state == TYPEC_ATTACHED_DBGACC_SNK)) {
			mca_log_err("Type-C SINK plug in\n");
			mtk_usb_extcon_set_role(extcon, USB_ROLE_DEVICE);
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_SNK ||
			noti->typec_state.old_state == TYPEC_ATTACHED_NORP_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_DBGACC_SNK) &&
			noti->typec_state.new_state == TYPEC_UNATTACHED) {
			mca_log_err("Type-C plug out\n");
			mtk_usb_extcon_set_role(extcon, USB_ROLE_NONE);
		}
		break;
	case TCP_NOTIFY_DR_SWAP:
		mca_log_err("%s dr_swap, new role=%d\n",
				__func__, noti->swap_state.new_role);
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

#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
	if (role == USB_ROLE_HOST)
		yili_mtk_usb_extcon_set_vbus(extcon, true);
	else
		yili_mtk_usb_extcon_set_vbus(extcon, false);
#else
	if (role == USB_ROLE_HOST)
		mtk_usb_extcon_set_vbus(extcon, true);
	else
		mtk_usb_extcon_set_vbus(extcon, false);
#endif


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

#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
	extcon->tcpc_dev[0] = tcpc_dev_get_by_name(tcpc_name);
	if (!extcon->tcpc_dev[0])
		return -ENODEV;

	tcpm_inquire_remote_cc(extcon->tcpc_dev[0], &cc1, &cc2, false);
#else
	extcon->tcpc_dev = tcpc_dev_get_by_name(tcpc_name);
	if (!extcon->tcpc_dev)
		return -ENODEV;

	tcpm_inquire_remote_cc(extcon->tcpc_dev, &cc1, &cc2, false);
#endif
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

#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
static int usb_switch_pinctrl_init(struct mtk_extcon_info *extcon)
{
	struct device *dev = extcon->dev;
	int ret = 0;

	extcon->pinctrl = devm_pinctrl_get(dev);

	if (IS_ERR(extcon->pinctrl)) {
		ret = PTR_ERR(extcon->pinctrl);
		dev_info(dev, "failed to get pinctrl, ret=%d\n", ret);
		return ret;
	}

	extcon->enable =
		pinctrl_lookup_state(extcon->pinctrl, "enable");

	if (IS_ERR(extcon->enable)) {
		dev_info(dev, "Can *NOT* find enable\n");
		extcon->enable = NULL;
	}

	extcon->disable =
		pinctrl_lookup_state(extcon->pinctrl, "disable");

	if (IS_ERR(extcon->disable)) {
		dev_info(dev, "Can *NOT* find disable\n");
		extcon->disable = NULL;
	}

	switch_usb_port(extcon, false);

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

	mca_log_err("mtk_usb_extcon_probe");
	extcon = devm_kzalloc(&pdev->dev, sizeof(*extcon), GFP_KERNEL);
	if (!extcon)
		return -ENOMEM;

	extcon->dev = dev;

	/* extcon */
	extcon->edev = devm_extcon_dev_allocate(dev, usb_extcon_cable);
	if (IS_ERR(extcon->edev)) {
		mca_log_err("failed to allocate extcon device\n");
		return -ENOMEM;
	}
#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
	usb_switch_pinctrl_init(extcon);
	extcon->cur_port = 0;
	extcon->data_port = 0;
	mutex_init(&extcon->mutex);
#endif
	extcon->attached_now = 0;
	ret = devm_extcon_dev_register(dev, extcon->edev);
	if (ret < 0) {
		dev_info(dev, "failed to register extcon device\n");
		return ret;
	}

	/* usb role switch */
	extcon->role_sw = usb_role_switch_get(extcon->dev);
	if (IS_ERR(extcon->role_sw)) {
		dev_err(dev, "failed to get usb role\n");
		return PTR_ERR(extcon->role_sw);
	}

	/* initial usb role */
	if (extcon->role_sw)
		extcon->c_role = USB_ROLE_NONE;

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
#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
	ret = yili_mtk_usb_extcon_tcpc_init(extcon);
#else
	ret = mtk_usb_extcon_tcpc_init(extcon);
#endif
	if (ret < 0)
		mca_log_err("failed to init tcpc\n");
#endif

	platform_set_drvdata(pdev, extcon);

	return 0;
}

static void mtk_usb_extcon_remove(struct platform_device *pdev)
{

}

static void mtk_usb_extcon_shutdown(struct platform_device *pdev)
{
	struct mtk_extcon_info *extcon = platform_get_drvdata(pdev);
	struct charger_device *cp_master = get_charger_by_name("cp_master");
	struct charger_device *chg_dev = get_charger_by_name("primary_chg");

	dev_info(extcon->dev, "shutdown\n");

	if(cp_master){
		charger_dev_enable_acdrv_manual(cp_master, false);
	}

	if(chg_dev){
		charger_dev_enable_powerpath(chg_dev, true);
	}

#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
	yili_mtk_usb_extcon_set_vbus(extcon, false);
#else
	mtk_usb_extcon_set_vbus(extcon, false);
#endif
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

