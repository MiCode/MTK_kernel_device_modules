// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 *
 * Filename:
 * ---------
 *    mtk_charger.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of Battery charging
 *
 * Author:
 * -------
 * Wy Chuang
 *
 */
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/reboot.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include <asm/setup.h>
#include <tcpm.h>
#include <linux/regmap.h>
#include <linux/of_platform.h>
#include <linux/hrtimer.h>
#include "mtk_charger.h"
#include "mtk_battery.h"
#include "pmic_voter.h"
#include "hq_fg_class.h"
#include "../../misc/mediatek/typec/tcpc/inc/tcpci.h"
#include "hq_fg_class.h"
#include "xm_chg_uevent.h"
#include "mtk_printk.h"

#define MT6369_STRUP_ANA_CON1 0x989

static struct platform_driver mtk_charger_driver;
static struct mtk_charger *pinfo = NULL;
int notify_adapter_event(struct notifier_block *notifier,
			unsigned long evt, void *val);
static void update_quick_chg_type(struct mtk_charger *info);
static int usb_get_property(struct mtk_charger *info, enum usb_property bp, int *val);

static DEFINE_SPINLOCK(typec_timer_lock);

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

SRCU_NOTIFIER_HEAD(charger_notifier);
EXPORT_SYMBOL_GPL(charger_notifier);
int charger_reg_notifier(struct notifier_block *nb)
{
	chr_err("%s: thermal notifier register\n", __func__);
	return srcu_notifier_chain_register(&charger_notifier, nb);
}
EXPORT_SYMBOL_GPL(charger_reg_notifier);
int charger_unreg_notifier(struct notifier_block *nb)
{
	chr_err("%s: thermal notifier\n", __func__);
	return srcu_notifier_chain_unregister(&charger_notifier, nb);
}
EXPORT_SYMBOL_GPL(charger_unreg_notifier);
int charger_notifier_call_chain(unsigned long event, int val)
{
	chr_err("%s: thermal notifier call chain\n", __func__);
	return srcu_notifier_call_chain(&charger_notifier, event, &val);
}
EXPORT_SYMBOL_GPL(charger_notifier_call_chain);

/* P16 code for BUGP16-900 by pengyuzhe1 at 2025/5/12 start */
static BLOCKING_NOTIFIER_HEAD(charger_framework_notifier);
int mtk_charger_fw_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&charger_framework_notifier, nb);
}
EXPORT_SYMBOL_GPL(mtk_charger_fw_notifier_register);

int mtk_charger_fw_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&charger_framework_notifier, nb);
}
EXPORT_SYMBOL_GPL(mtk_charger_fw_notifier_unregister);

int mtk_charger_fw_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&charger_framework_notifier, val, v);
}
EXPORT_SYMBOL_GPL(mtk_charger_fw_notifier_call_chain);
/* P16 code for BUGP16-900 by pengyuzhe1 at 2025/5/12 start */

#ifdef MODULE
static char __chg_cmdline[COMMAND_LINE_SIZE];
static char *chg_cmdline = __chg_cmdline;
static void usbpd_mi_vdm_received_cb(struct mtk_charger *info, struct tcp_ny_cvdm uvdm);

static struct blocking_notifier_head charge_current_nb;

int charge_current_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&charge_current_nb, nb);
}
EXPORT_SYMBOL(charge_current_register_notifier);

int charge_current_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&charge_current_nb, nb);
}
EXPORT_SYMBOL(charge_current_unregister_notifier);

int charge_current_notifier_call_chain(int val, void *v)
{
	return blocking_notifier_call_chain(&charge_current_nb, val, v);
}
EXPORT_SYMBOL(charge_current_notifier_call_chain);

extern int audio_status_notifier_register_client(struct notifier_block *nb);
extern int audio_status_notifier_unregister_client(struct notifier_block *nb);

const char *chg_get_cmd(void)
{
	struct device_node *of_chosen = NULL;
	char *bootargs = NULL;

	if (__chg_cmdline[0] != 0)
		return chg_cmdline;

	of_chosen = of_find_node_by_path("/chosen");
	if (of_chosen) {
		bootargs = (char *)of_get_property(
					of_chosen, "bootargs", NULL);
		if (!bootargs)
			chr_err("%s: failed to get bootargs\n", __func__);
		else {
			strcpy(__chg_cmdline, bootargs);
			chr_err("%s: bootargs: %s\n", __func__, bootargs);
		}
	} else
		chr_err("%s: failed to get /chosen\n", __func__);

	return chg_cmdline;
}

#else
const char *chg_get_cmd(void)
{
	return saved_command_line;
}
#endif

//弹窗返回值处理，正式开启反充判断
void set_reverse_quick_charge(bool en)
{
	struct mtk_charger *info = NULL;
	struct power_supply *chg_psy = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return;

	if (en) {
		chr_err("%s UI set reverse_quick_charge\n", __func__);
		schedule_delayed_work(&info->handle_reverse_charge_event_work, 0);
	} else {
		chr_err("%s user cancel reverse_quick_charge, do nothing \n", __func__);
	}
}

//在用户决定开启后，检查开启的条件是否满足，返回status值
/*
条件是：温度，soc这些
enum reverse_quick_charge_state{
	REVCHG_NORMAL = 0,
	REVCHG_QUICK,
	REVCHG_SCERRNON,
};
*/
static void get_reverse_svid(struct mtk_charger *info)
{
	uint32_t pd_vdos[8];
	int i = 0;

	if (info->reverse_adapter_svid != 0 && info->reverse_adapter_svid != 0xff00) {
		chr_err("%s:[REVCHG] alredy get svid:%x\n", __func__, info->reverse_adapter_svid);
		return;
	}

	tcpm_inquire_pd_partner_inform(info->tcpc, pd_vdos);
	for (i = 0; i < 8; i++)
			chr_err("[REVCHG] VDO[%d] : %08x\n", i, pd_vdos[i]);
	info->reverse_adapter_svid = pd_vdos[0] & 0x0000FFFF;
}

static void check_reverse_quick_charge(struct mtk_charger *info, int *status)
{
	int uisoc, battery_temp, reverse_vbus = 0;
	int soc_lmt = 40;

	uisoc = get_uisoc(info);
	reverse_vbus = get_vbus(info); /* mV */
	battery_temp = get_battery_temperature(info);
	get_reverse_svid(info);

	chr_err("[REVCHG] otg_stat:%d, soc:%d, bat_temp:%d, board_temp:%d, vid:0x%x, reverse_vbus:%d\n",
		info->otg_stat, uisoc, battery_temp, info->board_temp, info->reverse_adapter_svid, reverse_vbus);

	if (info->reverse_adapter_svid == 0x5ac) {//apple
		soc_lmt = 80;
		if(!info->otg_stat || battery_temp < 0 || info->board_temp > 400 || (reverse_vbus > 6500 && reverse_vbus < 7300))
			*status = 0;
		else if (uisoc >= soc_lmt || reverse_vbus >= 7300)
			*status = 1;
		else
			*status = 0;
	} else {
		soc_lmt = 40;
		if(!info->otg_stat || battery_temp < 0 || uisoc < soc_lmt || info->board_temp > 400)
			*status = 0;
		else
			*status = 1;
	}

	chr_err("%s:[REVCHG] status is %d\n", __func__, *status);
}

static void update_pdo_caps(struct mtk_charger *info, int pdo_caps)
{
	struct pd_port *pd_port = &info->tcpc->pd_port;

	chr_err("%s: [REVCHG] the pdo_caps is %d\n", __func__, pdo_caps);
	switch(pdo_caps) {
		case REVCHG_NORMAL:
			//5V1.5A
			pd_port->local_src_cap_default.pdos[0] =  0x26019096;
			break;
		case REVCHG_QUICK_9:
			//9V1A
			pd_port->local_src_cap_default.pdos[0] =  0x2602D064;
			break;
		case REVCHG_QUICK_22_5:
			//9V2.5A
			pd_port->local_src_cap_default.pdos[0] =  0x2602D0FA;
			break;
		default:
			break;
	}

	pd_port->local_src_cap_default.nr = 1;
	if (pdo_caps == REVCHG_NORMAL)
		tcpm_dpm_pd_hard_reset(info->tcpc, NULL);
	else
		tcpm_dpm_pd_soft_reset(info->tcpc, NULL);

	return;
}

//screen_state 1:灭屏 0：亮屏
static bool check_reverse_22_5(struct mtk_charger *info)
{
	//亮屏，清计数
	if(!info->screen_state) {
		info->ibat_check_cnt = 0;
		return false;
	}
	//灭屏状态连续两秒检测到ibat低于3A，判定为满足开启22.5W反充
	chr_err("%s: [REVCHG] ibat_check_cnt is %d\n", __func__, info->ibat_check_cnt);
	if( get_ibat(info) < 3000000)
		info->ibat_check_cnt ++;
	else
		info->ibat_check_cnt = 0;

	if( info->ibat_check_cnt < 2)
		return false;

	return true;
}

static void delay_disable_otg_workfunc(struct work_struct *work)
{
	struct mtk_charger *info = container_of(work,
					struct mtk_charger, delay_disable_otg_work.work);

	info->chg1_dev = get_charger_by_name("primary_chg");
	if ( !info->chg1_dev ) {
		chr_err("Error : can't find primary charger\n");
		return;
	}

	charger_dev_enable_otg_regulator(info->chg1_dev, false);
	charger_dev_enable(info->chg1_dev, false);

	chr_err("[REVCHG] delay_disable_otg_workfunc\n");
}

//3.反充主要处理逻辑
static void handle_reverse_charge_workfunc(struct work_struct *work)
{
	int revchg_enable = 0; //判断当前是否是反向充电的状态
	struct mtk_charger *info = container_of(work,
					struct mtk_charger, handle_reverse_charge_event_work.work);

	int i;
	u32 cp_ibus = 0;

	static int reverse_power_mode = REVCHG_QUICK_9;

	if (!info->reverse_charge_wakelock->active)
		__pm_stay_awake(info->reverse_charge_wakelock);

	check_reverse_quick_charge(info, &revchg_enable);
	if(!revchg_enable) {
		info->last_pdo_caps = 0;
		update_pdo_caps(info, REVCHG_NORMAL);
		return;
	}

	if (info->last_pdo_caps != reverse_power_mode) {
		chr_err("%s: [REVCHG] last_pdo_caps is %d, reverse_power_mode is %d\n", __func__,
					info->last_pdo_caps, reverse_power_mode);
		update_pdo_caps(info, 1);
		info->last_pdo_caps = reverse_power_mode;
	}

	switch(reverse_power_mode) {
		//case REVCHG_NORMAL:

		case REVCHG_QUICK_9:
			chr_err("%s: [REVCHG] in case REVCHG_QUICK_9\n", __func__);
			if (check_reverse_22_5(info)) {
				chr_err("%s:[REVCHG] ibat_check_cnt is %d\n", __func__, info->ibat_check_cnt);
				if (!info->revchg_bcl) {
					//发送uevent事件值3，通知打开bcl策略
					xm_charge_uevent_report(CHG_UEVENT_REVERSE_QUICK_CHARGE, 3);
				} else {
					reverse_power_mode = REVCHG_QUICK_22_5;
					if (info->last_pdo_caps != reverse_power_mode) {
						chr_err("%s: [REVCHG]2 last_pdo_caps is %d, reverse_power_mode is %d\n", __func__,
										info->last_pdo_caps, reverse_power_mode);
						update_pdo_caps(info, REVCHG_QUICK_22_5);
						info->last_pdo_caps = reverse_power_mode;
					}

				}
			}
			if(!info->screen_state && info->revchg_bcl && reverse_power_mode!= REVCHG_QUICK_22_5) {
				for(i = 0; i < 5; i++) {
				charger_dev_cp_enable_adc(info->cp_master, true);
				mdelay(20); //cp adc need 10ms to update ibus
				charger_dev_get_ibus(info->cp_master, &cp_ibus);
				chr_err("%s:[REVCHG] i = %d, cp_ibus =%d\n", __func__, i, cp_ibus);

				if(cp_ibus > 1500)
					goto next_loop;
				}
				//REVCHG_SCERRNON 2，上报uevent 2, 关闭bcl
				xm_charge_uevent_report(CHG_UEVENT_REVERSE_QUICK_CHARGE, 2);
			}
			break;
		case REVCHG_QUICK_22_5:
			//亮屏切换9V1A
			chr_err("%s: [REVCHG] in case REVCHG_QUICK_22_5\n", __func__);
			if(!info->screen_state) {
				reverse_power_mode = REVCHG_QUICK_9;
				if (info->last_pdo_caps != reverse_power_mode) {
					chr_err("%s: [REVCHG]3 last_pdo_caps is %d, reverse_power_mode is %d\n", __func__,
										info->last_pdo_caps, reverse_power_mode);
					update_pdo_caps(info, REVCHG_QUICK_9);
					info->last_pdo_caps = reverse_power_mode;
				}
			}
			break;

		default:
			break;

	}

next_loop:
	if (revchg_enable) {
		schedule_delayed_work(&info->handle_reverse_charge_event_work, msecs_to_jiffies(1000));
	} else {
		schedule_delayed_work(&info->handle_reverse_charge_event_work, msecs_to_jiffies(5000));
	}
}

static void check_revchg_status_workfunc(struct work_struct *work)
{
	struct mtk_charger *info = container_of(work,
					struct mtk_charger, check_revchg_status_work.work);

	int status = 0;

	check_reverse_quick_charge(info, &status);
	//发送uevent, 1 通知弹窗
	xm_charge_uevent_report(CHG_UEVENT_REVERSE_QUICK_CHARGE, status);

	if (info->otg_stat == CHARGER_OTG) {
		chr_err("[REVCHG] schedule check_revchg_status_workfunc\n");
		schedule_delayed_work(&info->check_revchg_status_work, msecs_to_jiffies(1000));
	}

}

static int chg_source_vbus(struct mtk_charger *info, int mv)
{
	int ret = 0;
	char *cp_name = NULL;

	if (info == NULL)
		return -ENODEV;

	info->chg1_dev = get_charger_by_name("primary_chg");
	if (info->chg1_dev)
		chr_err("Found primary charger\n");
	else {
		chr_err("*** Error : can't find primary charger ***\n");
		return -ENODEV;
	}

	struct pd_port *pd_port = &info->tcpc->pd_port;

	if (!info->cp_master) {
		info->cp_master = get_charger_by_name("cp_master");
		chr_err("failed to get master cp charger\n");
		return -ENODEV;
	}
	cp_name = charger_dev_get_cp_dev_name(info->cp_master);
	if (strstr(cp_name, "bq25960")) {
		pd_port->is_bq_cp = true;
	} else {
		pd_port->is_bq_cp = false;
	}

	chr_err("[REVCHG] source vbus: %dmv\n", mv);
	switch (mv)
	{
		case 0:
		/* code */
			ret |= charger_dev_cp_set_otg_config(info->cp_master, false);
			cancel_delayed_work_sync(&info->check_revchg_status_work);
			xm_charge_uevent_report(CHG_UEVENT_REVERSE_QUICK_CHARGE, 0);
			info->otg_stat = DIS_OTG;
			info->pd30_source = false;
			break;
		case 5000:
			if(info->otg_stat == HV_OTG){
				ret |= charger_dev_cp_set_otg_config(info->cp_master, false);
				chr_err("[REVCHG] 9V revchg to 5V!\n");
			}
			if (pd_port->is_bq_cp) {
				chr_err("[REVCHG] For bq25960 OTG ovp_gate\n");
				charger_dev_enable_acdrv_manual(info->cp_master, true);
			}
			info->otg_stat = CHARGER_OTG;
			if(info->pd30_source == true) {
				schedule_delayed_work(&info->check_revchg_status_work, 0);
			}
			break;
		case 9000:
			//ret |= charger_dev_enable_otg_regulator(info->chg1_dev, true);
			cancel_delayed_work_sync(&info->check_revchg_status_work);
			ret |= charger_dev_cp_set_otg_config(info->cp_master, true);
			ret |= tcpm_notify_vbus_stable(info->tcpc);
			if (pd_port->is_bq_cp) {
				chr_err("[REVCHG] is bq25960\n");
				schedule_delayed_work(&info->delay_disable_otg_work, msecs_to_jiffies(REVERSE_CHARGE_DELAY_DISOTG));
			} else {
				ret |= charger_dev_enable_otg_regulator(info->chg1_dev, false);
				ret |= charger_dev_enable(info->chg1_dev, false);
			}
			info->otg_stat = HV_OTG;
			break;

		default:
			break;
	}

	return ret;
}

int chr_get_debug_level(void)
{
	struct power_supply *psy;
	static struct mtk_charger *info;
	int ret;

	if (info == NULL) {
		psy = power_supply_get_by_name("mtk-master-charger");
		if (psy == NULL)
			ret = CHRLOG_DEBUG_LEVEL;
		else {
			info =
			(struct mtk_charger *)power_supply_get_drvdata(psy);
			if (info == NULL)
				ret = CHRLOG_DEBUG_LEVEL;
			else
				ret = info->log_level;
		}
	} else
		ret = info->log_level;

	return ret;
}
EXPORT_SYMBOL(chr_get_debug_level);

void _wake_up_charger(struct mtk_charger *info)
{
	unsigned long flags;

	if (info == NULL)
		return;
	spin_lock_irqsave(&info->slock, flags);
	if (!info->charger_wakelock->active)
		__pm_stay_awake(info->charger_wakelock);
	spin_unlock_irqrestore(&info->slock, flags);
	info->charger_thread_timeout = true;
	wake_up_interruptible(&info->wait_que);
}

bool is_disable_charger(struct mtk_charger *info)
{
	if (info == NULL)
		return true;

	if (info->disable_charger == true || IS_ENABLED(CONFIG_POWER_EXT))
		return true;
	else
		return false;
}

int _mtk_enable_charging(struct mtk_charger *info,
	bool en)
{
	chr_debug("%s en:%d\n", __func__, en);
	if (info->algo.enable_charging != NULL)
		return info->algo.enable_charging(info, en);
	return false;
}

int mtk_charger_notifier(struct mtk_charger *info, int event)
{
	return srcu_notifier_call_chain(&info->evt_nh, event, NULL);
}

static void mtk_charger_parse_dt(struct mtk_charger *info,
				struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 val = 0;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;

	boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
	if (!boot_node)
		chr_err("%s: failed to get boot mode phandle\n", __func__);
	else {
		tag = (struct tag_bootmode *)of_get_property(boot_node,
							"atag,boot", NULL);
		if (!tag)
			chr_err("%s: failed to get atag,boot\n", __func__);
		else {
			chr_err("%s: size:0x%x tag:0x%x bootmode:0x%x boottype:0x%x\n",
				__func__, tag->size, tag->tag,
				tag->bootmode, tag->boottype);
			info->bootmode = tag->bootmode;
			info->boottype = tag->boottype;
		}
	}

	if (of_property_read_string(np, "algorithm-name",
		&info->algorithm_name) < 0) {
		if (of_property_read_string(np, "algorithm_name",
			&info->algorithm_name) < 0) {
			chr_err("%s: no algorithm_name, use Basic\n", __func__);
			info->algorithm_name = "Basic";
		}
	}

	if (strcmp(info->algorithm_name, "Basic") == 0) {
		chr_err("found Basic\n");
		mtk_basic_charger_init(info);
	} else if (strcmp(info->algorithm_name, "Pulse") == 0) {
		chr_err("found Pulse\n");
		mtk_pulse_charger_init(info);
	}

	info->disable_charger = of_property_read_bool(np, "disable_charger")
		|| of_property_read_bool(np, "disable-charger");
	info->charger_unlimited = of_property_read_bool(np, "charger_unlimited")
		|| of_property_read_bool(np, "charger-unlimited");
	info->atm_enabled = of_property_read_bool(np, "atm_is_enabled")
		|| of_property_read_bool(np, "atm-is-enabled");
	info->enable_sw_safety_timer =
			of_property_read_bool(np, "enable_sw_safety_timer")
			|| of_property_read_bool(np, "enable-sw-safety-timer");
	info->sw_safety_timer_setting = info->enable_sw_safety_timer;
	info->disable_aicl = of_property_read_bool(np, "disable_aicl")
		|| of_property_read_bool(np, "disable-aicl");
	info->alg_new_arbitration = of_property_read_bool(np, "alg_new_arbitration")
		|| of_property_read_bool(np, "alg-new-arbitration");
	info->alg_unchangeable = of_property_read_bool(np, "alg_unchangeable")
		|| of_property_read_bool(np, "alg-unchangeable");

	/* common */

	if (of_property_read_u32(np, "charger_configuration", &val) >= 0)
		info->config = val;
	else if (of_property_read_u32(np, "charger-configuration", &val) >= 0)
		info->config = val;
	else {
		chr_err("use default charger_configuration:%d\n",
			SINGLE_CHARGER);
		info->config = SINGLE_CHARGER;
	}

	if (of_property_read_u32(np, "battery_cv", &val) >= 0)
		info->data.battery_cv = val;
	else if (of_property_read_u32(np, "battery-cv", &val) >= 0)
		info->data.battery_cv = val;
	else {
		chr_err("use default BATTERY_CV:%d\n", BATTERY_CV);
		info->data.battery_cv = BATTERY_CV;
	}


	info->enable_boot_volt =
		of_property_read_bool(np, "enable_boot_volt")
		|| of_property_read_bool(np, "enable-boot-volt");

	if (of_property_read_u32(np, "max_charger_voltage", &val) >= 0)
		info->data.max_charger_voltage = val;
	else if (of_property_read_u32(np, "max-charger-voltage", &val) >= 0)
		info->data.max_charger_voltage = val;
	else {
		chr_err("use default V_CHARGER_MAX:%d\n", V_CHARGER_MAX);
		info->data.max_charger_voltage = V_CHARGER_MAX;
	}
	info->data.max_charger_voltage_setting = info->data.max_charger_voltage;

	if (of_property_read_u32(np, "vbus_sw_ovp_voltage", &val) >= 0)
		info->data.vbus_sw_ovp_voltage = val;
	else if (of_property_read_u32(np, "vbus-sw-ovp-voltage", &val) >= 0)
		info->data.vbus_sw_ovp_voltage = val;
	else {
		chr_err("use default V_CHARGER_MAX:%d\n", V_CHARGER_MAX);
		info->data.vbus_sw_ovp_voltage = VBUS_OVP_VOLTAGE;
	}

	if (of_property_read_u32(np, "min_charger_voltage", &val) >= 0)
		info->data.min_charger_voltage = val;
	else if (of_property_read_u32(np, "min-charger-voltage", &val) >= 0)
		info->data.min_charger_voltage = val;
	else {
		chr_err("use default V_CHARGER_MIN:%d\n", V_CHARGER_MIN);
		info->data.min_charger_voltage = V_CHARGER_MIN;
	}

	if (of_property_read_u32(np, "enable_vbat_mon", &val) >= 0) {
		info->enable_vbat_mon = val;
		info->enable_vbat_mon_bak = val;
	} else if (of_property_read_u32(np, "enable-vbat-mon", &val) >= 0) {
		info->enable_vbat_mon = val;
		info->enable_vbat_mon_bak = val;
	} else {
		chr_err("use default enable 6pin\n");
		info->enable_vbat_mon = 0;
		info->enable_vbat_mon_bak = 0;
	}
	chr_err("enable_vbat_mon:%d\n", info->enable_vbat_mon);

	/* sw jeita */
	info->enable_sw_jeita = of_property_read_bool(np, "enable_sw_jeita")
		|| of_property_read_bool(np, "enable-sw-jeita");

	if (of_property_read_u32(np, "jeita_temp_above_t4_cv", &val) >= 0)
		info->data.jeita_temp_above_t4_cv = val;
	else if (of_property_read_u32(np, "jeita-temp-above-t4-cv", &val) >= 0)
		info->data.jeita_temp_above_t4_cv = val;
	else {
		chr_err("use default JEITA_TEMP_ABOVE_T4_CV:%d\n",
			JEITA_TEMP_ABOVE_T4_CV);
		info->data.jeita_temp_above_t4_cv = JEITA_TEMP_ABOVE_T4_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t3_to_t4_cv", &val) >= 0)
		info->data.jeita_temp_t3_to_t4_cv = val;
	else if (of_property_read_u32(np, "jeita-temp-t3-to-t4-cv", &val) >= 0)
		info->data.jeita_temp_t3_to_t4_cv = val;
	else {
		chr_err("use default JEITA_TEMP_T3_TO_T4_CV:%d\n",
			JEITA_TEMP_T3_TO_T4_CV);
		info->data.jeita_temp_t3_to_t4_cv = JEITA_TEMP_T3_TO_T4_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t2_to_t3_cv", &val) >= 0)
		info->data.jeita_temp_t2_to_t3_cv = val;
	else if (of_property_read_u32(np, "jeita-temp-t2-to-t3-cv", &val) >= 0)
		info->data.jeita_temp_t2_to_t3_cv = val;
	else {
		chr_err("use default JEITA_TEMP_T2_TO_T3_CV:%d\n",
			JEITA_TEMP_T2_TO_T3_CV);
		info->data.jeita_temp_t2_to_t3_cv = JEITA_TEMP_T2_TO_T3_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t1_to_t2_cv", &val) >= 0)
		info->data.jeita_temp_t1_to_t2_cv = val;
	else if (of_property_read_u32(np, "jeita-temp-t1-to-t2-cv", &val) >= 0)
		info->data.jeita_temp_t1_to_t2_cv = val;
	else {
		chr_err("use default JEITA_TEMP_T1_TO_T2_CV:%d\n",
			JEITA_TEMP_T1_TO_T2_CV);
		info->data.jeita_temp_t1_to_t2_cv = JEITA_TEMP_T1_TO_T2_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t0_to_t1_cv", &val) >= 0)
		info->data.jeita_temp_t0_to_t1_cv = val;
	else if (of_property_read_u32(np, "jeita-temp-t0-to-t1-cv", &val) >= 0)
		info->data.jeita_temp_t0_to_t1_cv = val;
	else {
		chr_err("use default JEITA_TEMP_T0_TO_T1_CV:%d\n",
			JEITA_TEMP_T0_TO_T1_CV);
		info->data.jeita_temp_t0_to_t1_cv = JEITA_TEMP_T0_TO_T1_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_below_t0_cv", &val) >= 0)
		info->data.jeita_temp_below_t0_cv = val;
	if (of_property_read_u32(np, "jeita-temp-below-t0-cv", &val) >= 0)
		info->data.jeita_temp_below_t0_cv = val;
	else {
		chr_err("use default JEITA_TEMP_BELOW_T0_CV:%d\n",
			JEITA_TEMP_BELOW_T0_CV);
		info->data.jeita_temp_below_t0_cv = JEITA_TEMP_BELOW_T0_CV;
	}

	if (of_property_read_u32(np, "temp_t4_thres", &val) >= 0)
		info->data.temp_t4_thres = val;
	else if (of_property_read_u32(np, "temp-t4-thres", &val) >= 0)
		info->data.temp_t4_thres = val;
	else {
		chr_err("use default TEMP_T4_THRES:%d\n",
			TEMP_T4_THRES);
		info->data.temp_t4_thres = TEMP_T4_THRES;
	}

	if (of_property_read_u32(np, "temp_t4_thres_minus_x_degree", &val) >= 0)
		info->data.temp_t4_thres_minus_x_degree = val;
	else if (of_property_read_u32(np, "temp-t4-thres-minus-x-degree", &val) >= 0)
		info->data.temp_t4_thres_minus_x_degree = val;
	else {
		chr_err("use default TEMP_T4_THRES_MINUS_X_DEGREE:%d\n",
			TEMP_T4_THRES_MINUS_X_DEGREE);
		info->data.temp_t4_thres_minus_x_degree =
					TEMP_T4_THRES_MINUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t3_thres", &val) >= 0)
		info->data.temp_t3_thres = val;
	else if (of_property_read_u32(np, "temp-t3-thres", &val) >= 0)
		info->data.temp_t3_thres = val;
	else {
		chr_err("use default TEMP_T3_THRES:%d\n",
			TEMP_T3_THRES);
		info->data.temp_t3_thres = TEMP_T3_THRES;
	}

	if (of_property_read_u32(np, "temp_t3_thres_minus_x_degree", &val) >= 0)
		info->data.temp_t3_thres_minus_x_degree = val;
	else if (of_property_read_u32(np, "temp-t3-thres-minus-x-degree", &val) >= 0)
		info->data.temp_t3_thres_minus_x_degree = val;
	else {
		chr_err("use default TEMP_T3_THRES_MINUS_X_DEGREE:%d\n",
			TEMP_T3_THRES_MINUS_X_DEGREE);
		info->data.temp_t3_thres_minus_x_degree =
					TEMP_T3_THRES_MINUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t2_thres", &val) >= 0)
		info->data.temp_t2_thres = val;
	else if (of_property_read_u32(np, "temp-t2-thres", &val) >= 0)
		info->data.temp_t2_thres = val;
	else {
		chr_err("use default TEMP_T2_THRES:%d\n",
			TEMP_T2_THRES);
		info->data.temp_t2_thres = TEMP_T2_THRES;
	}

	if (of_property_read_u32(np, "temp_t2_thres_plus_x_degree", &val) >= 0)
		info->data.temp_t2_thres_plus_x_degree = val;
	else if (of_property_read_u32(np, "temp-t2-thres-plus-x-degree", &val) >= 0)
		info->data.temp_t2_thres_plus_x_degree = val;
	else {
		chr_err("use default TEMP_T2_THRES_PLUS_X_DEGREE:%d\n",
			TEMP_T2_THRES_PLUS_X_DEGREE);
		info->data.temp_t2_thres_plus_x_degree =
					TEMP_T2_THRES_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t1_thres", &val) >= 0)
		info->data.temp_t1_thres = val;
	else if (of_property_read_u32(np, "temp-t1-thres", &val) >= 0)
		info->data.temp_t1_thres = val;
	else {
		chr_err("use default TEMP_T1_THRES:%d\n",
			TEMP_T1_THRES);
		info->data.temp_t1_thres = TEMP_T1_THRES;
	}

	if (of_property_read_u32(np, "temp_t1_thres_plus_x_degree", &val) >= 0)
		info->data.temp_t1_thres_plus_x_degree = val;
	else if (of_property_read_u32(np, "temp-t1-thres-plus-x-degree", &val) >= 0)
		info->data.temp_t1_thres_plus_x_degree = val;
	else {
		chr_err("use default TEMP_T1_THRES_PLUS_X_DEGREE:%d\n",
			TEMP_T1_THRES_PLUS_X_DEGREE);
		info->data.temp_t1_thres_plus_x_degree =
					TEMP_T1_THRES_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t0_thres", &val) >= 0)
		info->data.temp_t0_thres = val;
	else if (of_property_read_u32(np, "temp-t0-thres", &val) >= 0)
		info->data.temp_t0_thres = val;
	else {
		chr_err("use default TEMP_T0_THRES:%d\n",
			TEMP_T0_THRES);
		info->data.temp_t0_thres = TEMP_T0_THRES;
	}

	if (of_property_read_u32(np, "temp_t0_thres_plus_x_degree", &val) >= 0)
		info->data.temp_t0_thres_plus_x_degree = val;
	else if (of_property_read_u32(np, "temp-t0-thres-plus-x-degree", &val) >= 0)
		info->data.temp_t0_thres_plus_x_degree = val;
	else {
		chr_err("use default TEMP_T0_THRES_PLUS_X_DEGREE:%d\n",
			TEMP_T0_THRES_PLUS_X_DEGREE);
		info->data.temp_t0_thres_plus_x_degree =
					TEMP_T0_THRES_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_neg_10_thres", &val) >= 0)
		info->data.temp_neg_10_thres = val;
	else if (of_property_read_u32(np, "temp-neg-10-thres", &val) >= 0)
		info->data.temp_neg_10_thres = val;
	else {
		chr_err("use default TEMP_NEG_10_THRES:%d\n",
			TEMP_NEG_10_THRES);
		info->data.temp_neg_10_thres = TEMP_NEG_10_THRES;
	}

	/* battery temperature protection */
	info->thermal.sm = BAT_TEMP_NORMAL;
	info->thermal.enable_min_charge_temp =
		of_property_read_bool(np, "enable_min_charge_temp")
		|| of_property_read_bool(np, "enable-min-charge-temp");

	if (of_property_read_u32(np, "min_charge_temp", &val) >= 0)
		info->thermal.min_charge_temp = val;
	else if (of_property_read_u32(np, "min-charge-temp", &val) >= 0)
		info->thermal.min_charge_temp = val;
	else {
		chr_err("use default MIN_CHARGE_TEMP:%d\n",
			MIN_CHARGE_TEMP);
		info->thermal.min_charge_temp = MIN_CHARGE_TEMP;
	}

	if (of_property_read_u32(np, "min_charge_temp_plus_x_degree", &val)
		>= 0) {
		info->thermal.min_charge_temp_plus_x_degree = val;
	} else if (of_property_read_u32(np, "min-charge-temp-plus-x-degree", &val)
		>= 0) {
		info->thermal.min_charge_temp_plus_x_degree = val;
	} else {
		chr_err("use default MIN_CHARGE_TEMP_PLUS_X_DEGREE:%d\n",
			MIN_CHARGE_TEMP_PLUS_X_DEGREE);
		info->thermal.min_charge_temp_plus_x_degree =
					MIN_CHARGE_TEMP_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "max_charge_temp", &val) >= 0)
		info->thermal.max_charge_temp = val;
	else if (of_property_read_u32(np, "max-charge-temp", &val) >= 0)
		info->thermal.max_charge_temp = val;
	else {
		chr_err("use default MAX_CHARGE_TEMP:%d\n",
			MAX_CHARGE_TEMP);
		info->thermal.max_charge_temp = MAX_CHARGE_TEMP;
	}

	if (of_property_read_u32(np, "max_charge_temp_minus_x_degree", &val)
		>= 0) {
		info->thermal.max_charge_temp_minus_x_degree = val;
	} else if (of_property_read_u32(np, "max-charge-temp-minus-x-degree", &val)
		>= 0) {
		info->thermal.max_charge_temp_minus_x_degree = val;
	} else {
		chr_err("use default MAX_CHARGE_TEMP_MINUS_X_DEGREE:%d\n",
			MAX_CHARGE_TEMP_MINUS_X_DEGREE);
		info->thermal.max_charge_temp_minus_x_degree =
					MAX_CHARGE_TEMP_MINUS_X_DEGREE;
	}

	/* charging current */
	if (of_property_read_u32(np, "usb_charger_current", &val) >= 0)
		info->data.usb_charger_current = val;
	else if (of_property_read_u32(np, "usb-charger-current", &val) >= 0)
		info->data.usb_charger_current = val;
	else {
		chr_err("use default USB_CHARGER_CURRENT:%d\n",
			USB_CHARGER_CURRENT);
		info->data.usb_charger_current = USB_CHARGER_CURRENT;
	}

	if (of_property_read_u32(np, "ac_charger_current", &val) >= 0){
		info->data.ac_charger_current = val;
        } else if (of_property_read_u32(np, "ac-charger-current", &val) >= 0) {
		info->data.ac_charger_current = val;
	} else {
		chr_err("use default AC_CHARGER_CURRENT:%d\n",
			AC_CHARGER_CURRENT);
		info->data.ac_charger_current = AC_CHARGER_CURRENT;
	}

	if (of_property_read_u32(np, "ac_charger_input_current", &val) >= 0)
		info->data.ac_charger_input_current = val;
	else if (of_property_read_u32(np, "ac-charger-input-current", &val) >= 0)
		info->data.ac_charger_input_current = val;
	else {
		chr_err("use default AC_CHARGER_INPUT_CURRENT:%d\n",
			AC_CHARGER_INPUT_CURRENT);
		info->data.ac_charger_input_current = AC_CHARGER_INPUT_CURRENT;
	}

	if (of_property_read_u32(np, "charging_host_charger_current", &val)
		>= 0) {
		info->data.charging_host_charger_current = val;
	} else if (of_property_read_u32(np, "charging-host-charger-current", &val)
		>= 0) {
		info->data.charging_host_charger_current = val;
	} else {
		chr_err("use default CHARGING_HOST_CHARGER_CURRENT:%d\n",
			CHARGING_HOST_CHARGER_CURRENT);
		info->data.charging_host_charger_current =
					CHARGING_HOST_CHARGER_CURRENT;
	}

	/* dynamic mivr */
	info->enable_dynamic_mivr =
			of_property_read_bool(np, "enable_dynamic_mivr")
			|| of_property_read_bool(np, "enable-dynamic-mivr");

	if (of_property_read_u32(np, "min_charger_voltage_1", &val) >= 0)
		info->data.min_charger_voltage_1 = val;
	else if (of_property_read_u32(np, "min-charger-voltage-1", &val) >= 0)
		info->data.min_charger_voltage_1 = val;
	else {
		chr_err("use default V_CHARGER_MIN_1: %d\n", V_CHARGER_MIN_1);
		info->data.min_charger_voltage_1 = V_CHARGER_MIN_1;
	}

	if (of_property_read_u32(np, "min_charger_voltage_2", &val) >= 0)
		info->data.min_charger_voltage_2 = val;
	else if (of_property_read_u32(np, "min-charger-voltage-2", &val) >= 0)
		info->data.min_charger_voltage_2 = val;
	else {
		chr_err("use default V_CHARGER_MIN_2: %d\n", V_CHARGER_MIN_2);
		info->data.min_charger_voltage_2 = V_CHARGER_MIN_2;
	}

	if (of_property_read_u32(np, "max_dmivr_charger_current", &val) >= 0)
		info->data.max_dmivr_charger_current = val;
	else if (of_property_read_u32(np, "max-dmivr-charger-current", &val) >= 0)
		info->data.max_dmivr_charger_current = val;
	else {
		chr_err("use default MAX_DMIVR_CHARGER_CURRENT: %d\n",
			MAX_DMIVR_CHARGER_CURRENT);
		info->data.max_dmivr_charger_current =
					MAX_DMIVR_CHARGER_CURRENT;
	}
	/* fast charging algo support indicator */
	info->enable_fast_charging_indicator =
			of_property_read_bool(np, "enable_fast_charging_indicator")
			|| of_property_read_bool(np, "enable-fast-charging-indicator");

	if (of_property_read_u32(np, "fv", &val) >= 0)
		info->fv = val;
	else {
		chr_err("failed to parse fv use default\n");
		info->fv = 4450;
	}

	if (of_property_read_u32(np, "fv_normal", &val) >= 0)
		info->fv_normal = val;
	else {
		chr_err("failed to parse fv_normal use fv\n");
		info->fv_normal = info->fv;
	}

	if (of_property_read_u32(np, "fv_ffc", &val) >= 0)
		info->fv_ffc = val;
	else {
		chr_err("failed to parse fv_ffc use default\n");
		info->fv_ffc = 4450;
	}

	#if 0
	if (of_property_read_u32(np, "iterm", &val) >= 0)
		info->iterm = val;
	else {
		chr_err("failed to parse iterm use default\n");
		info->iterm = 200;
	}
	if (of_property_read_u32(np, "iterm_warm", &val) >= 0)
		info->iterm_warm = val;
	else {
		chr_err("failed to parse iterm use default\n");
		info->iterm_warm = info->iterm;
	}

	if (of_property_read_u32(np, "iterm_ffc", &val) >= 0)
		info->iterm_ffc = val;
	else {
		chr_err("failed to parse iterm_ffc use default\n");
		info->iterm_ffc = 700;
	}

	if (of_property_read_u32(np, "iterm_ffc_warm", &val) >= 0)
		info->iterm_ffc_warm = val;
	else {
		chr_err("failed to parse iterm_ffc_warm use default\n");
		info->iterm_ffc_warm = 800;
	}
	#endif

	if (of_property_read_u32(np, "iterm_2nd", &val) >= 0)
		info->iterm_2nd = val;
	else {
		chr_err("failed to parse iterm_2nd use default\n");
		info->iterm_2nd = 200;
	}
	if (of_property_read_u32(np, "iterm_warm_2nd", &val) >= 0)
		info->iterm_warm_2nd = val;
	else {
		chr_err("failed to parse iterm_warm_2nd use default\n");
		info->iterm_warm_2nd = info->iterm_2nd;
	}

	if (of_property_read_u32(np, "iterm_ffc_2nd", &val) >= 0)
		info->iterm_ffc_2nd = val;
	else {
		chr_err("failed to parse iterm_ffc_2nd use default\n");
		info->iterm_ffc_2nd = 700;
	}

	if (of_property_read_u32(np, "iterm_ffc_warm_2nd", &val) >= 0)
		info->iterm_ffc_warm_2nd = val;
	else {
		chr_err("failed to parse iterm_ffc_warm_2nd use default\n");
		info->iterm_ffc_warm_2nd = 800;
	}

	if (of_property_read_u32(np, "ffc_low_tbat", &val) >= 0)
		info->ffc_low_tbat = val;
	else {
		chr_err("failed to parse ffc_low_tbat use default\n");
		info->ffc_low_tbat = 160;
	}

	if (of_property_read_u32(np, "ffc_medium_tbat", &val) >= 0)
		info->ffc_medium_tbat = val;
	else {
		chr_err("failed to parse ffc_medium_tbat use default\n");
		info->ffc_medium_tbat = 360;
	}

	if (of_property_read_u32(np, "ffc_warm_tbat", &val) >= 0)
		info->ffc_warm_tbat = val;
	else {
		chr_err("failed to parse ffc_warm_tbat use default\n");
		info->ffc_warm_tbat = 360;
	}

	if (of_property_read_u32(np, "ffc_little_high_tbat", &val) >= 0)
		info->ffc_little_high_tbat = val;
	else {
		chr_err("failed to parse ffc_little_high_tbat use default\n");
		info->ffc_little_high_tbat = 400;
	}

	if (of_property_read_u32(np, "ffc_high_tbat", &val) >= 0)
		info->ffc_high_tbat = val;
	else {
		chr_err("failed to parse ffc_high_tbat use default\n");
		info->ffc_high_tbat = 460;
	}

	if (of_property_read_u32(np, "ffc_high_soc", &val) >= 0)
		info->ffc_high_soc = val;
	else {
		chr_err("failed to parse ffc_high_soc use default\n");
		info->ffc_high_soc = 96;
	}

	if (of_property_read_u32(np, "max_fcc", &val) >= 0)
	{
		info->max_fcc = val;
		chr_err("success to parse max_fcc max_fcc=%d\n", info->max_fcc);
	}
	else {
		chr_err("failed to parse max_fcc use default\n");
		info->max_fcc = 8200;
	}

	/* en_floatgnd */
	info->en_floatgnd = of_property_read_bool(np, "en_floatgnd");

	chr_info("parse fv = %d, fv_ffc = %d, iterm = %d, iterm_ffc = %d, ffc_low_tbat = %d, ffc_medium_tbat = %d, ffc_warm_tbat=%d, ffc_high_tbat = %d, ffc_high_soc = %d, max_fcc=%d\n",
		info->fv, info->fv_ffc, info->iterm, info->iterm_ffc, info->ffc_low_tbat, info->ffc_medium_tbat, info->ffc_warm_tbat, info->ffc_high_tbat, info->ffc_high_soc, info->max_fcc);
	chr_info("parse iterm_2nd = %d, iterm_ffc_2nd = %d,iterm_warm_2nd=%d,  iterm_ffc_warm_2nd=%d, en_floatgnd=%d\n",
		info->iterm_2nd, info->iterm_ffc_2nd, info->iterm_warm_2nd, info->iterm_ffc_warm_2nd, info->en_floatgnd);
}

static void mtk_charger_start_timer(struct mtk_charger *info)
{
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;

	/* If the timer was already set, cancel it */
	ret = alarm_try_to_cancel(&info->charger_timer);
	if (ret < 0) {
		chr_err("%s: callback was running, skip timer\n", __func__);
		return;
	}

	ktime_now = ktime_get_boottime();
	time_now = ktime_to_timespec64(ktime_now);
	end_time.tv_sec = time_now.tv_sec + info->polling_interval;
	end_time.tv_nsec = time_now.tv_nsec + 0;
	info->endtime = end_time;
	ktime = ktime_set(info->endtime.tv_sec, info->endtime.tv_nsec);

	chr_err("%s: alarm timer start:%d, %lld %ld\n", __func__, ret,
		info->endtime.tv_sec, info->endtime.tv_nsec);
	alarm_start(&info->charger_timer, ktime);
}

static void check_battery_exist(struct mtk_charger *info)
{
	unsigned int i = 0;
	int count = 0;
	//int boot_mode = get_boot_mode();

	if (is_disable_charger(info))
		return;

	for (i = 0; i < 3; i++) {
		if (is_battery_exist(info) == false)
			count++;
	}

#ifdef FIXME
	if (count >= 3) {
		if (boot_mode == META_BOOT || boot_mode == ADVMETA_BOOT ||
		    boot_mode == ATE_FACTORY_BOOT)
			chr_info("boot_mode = %d, bypass battery check\n",
				boot_mode);
		else {
			chr_err("battery doesn't exist, shutdown\n");
			orderly_poweroff(true);
		}
	}
#endif
}

static void check_dynamic_mivr(struct mtk_charger *info)
{
	int i = 0, ret = 0;
	int vbat = 0;
	bool is_fast_charge = false;
	struct chg_alg_device *alg = NULL;

	if (!info->enable_dynamic_mivr)
		return;

	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		if (alg == NULL)
			continue;

		ret = chg_alg_is_algo_ready(alg);
		if (ret == ALG_RUNNING) {
			is_fast_charge = true;
			break;
		}
	}

	if (!is_fast_charge) {
		vbat = get_battery_voltage(info);
		if (vbat < info->data.min_charger_voltage_2 / 1000 - 200)
			charger_dev_set_mivr(info->chg1_dev,
				info->data.min_charger_voltage_2);
		else if (vbat < info->data.min_charger_voltage_1 / 1000 - 200)
			charger_dev_set_mivr(info->chg1_dev,
				info->data.min_charger_voltage_1);
		else
			charger_dev_set_mivr(info->chg1_dev,
				info->data.min_charger_voltage);
	}
}

static void handle_cc_status_work_func(struct work_struct *work)
{
	chr_err("%s: enter; typec_attach = %d, screen_status = %d, audio_status = %d, ui_cc_toggle = %d\n",
			__func__, pinfo->typec_attach, pinfo->screen_status, pinfo->audio_status, pinfo->ui_cc_toggle);

	if (pinfo == NULL || pinfo->tcpc == NULL) {
		chr_err("%s: pinfo or pinfo->tcpc is NULL\n", __func__);
		return;
	}

	if (pinfo->typec_attach) {
		return;
	}

	if (pinfo->screen_status == SCREEN_STATE_BLACK) {
		if (pinfo->audio_status) {
			tcpci_set_cc(pinfo->tcpc, TYPEC_CC_DRP);
			chr_err("%s: set cc drp\n", __func__);
		} else {
			if (pinfo->ui_cc_toggle) {
				tcpci_set_cc(pinfo->tcpc, TYPEC_CC_DRP);
				chr_err("%s: set cc drp\n", __func__);
			} else {
				tcpci_set_cc(pinfo->tcpc, TYPEC_CC_RD);
				chr_err("%s: set cc rd\n", __func__);
			}
		}
	} else if (pinfo->screen_status == SCREEN_STATE_BRIGHT) {
		if (pinfo->ui_cc_toggle) {
			tcpci_set_cc(pinfo->tcpc, TYPEC_CC_DRP);
			chr_err("%s: set cc drp\n", __func__);
		}
	}

	chr_err("%s: end\n", __func__);

	return;
}

static enum alarmtimer_restart otg_ui_close_timer_handler(struct alarm *alarm, ktime_t now)
{
	if (pinfo != NULL) {
		pinfo->ui_cc_toggle = false;
		schedule_delayed_work(&pinfo->handle_cc_status_work, 0);
	}
	chr_err("%s: enter\n", __func__);
	return ALARMTIMER_NORESTART;
}

static enum alarmtimer_restart set_soft_cid_timer_handler(struct alarm *alarm, ktime_t now)
{
	if (pinfo != NULL) {
		schedule_delayed_work(&pinfo->dis_floatgnd_work, 0);
		schedule_delayed_work(&pinfo->handle_cc_status_work, 0);
	}
	chr_err("%s: enter\n", __func__);
	return ALARMTIMER_NORESTART;
}

/* sw jeita */
void do_sw_jeita_state_machine(struct mtk_charger *info)
{
	struct sw_jeita_data *sw_jeita;

	sw_jeita = &info->sw_jeita;
	sw_jeita->pre_sm = sw_jeita->sm;
	sw_jeita->charging = true;

	/* JEITA battery temp Standard */
	if (info->battery_temp >= info->data.temp_t4_thres) {
		chr_err("[SW_JEITA] Battery Over high Temperature(%d) !!\n",
			info->data.temp_t4_thres);

		sw_jeita->sm = TEMP_ABOVE_T4;
		sw_jeita->charging = false;
	} else if (info->battery_temp > info->data.temp_t3_thres) {
		/* control 45 degree to normal behavior */
		if ((sw_jeita->sm == TEMP_ABOVE_T4)
		    && (info->battery_temp
			>= info->data.temp_t4_thres_minus_x_degree)) {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d,not allow charging yet!!\n",
				info->data.temp_t4_thres_minus_x_degree,
				info->data.temp_t4_thres);

			sw_jeita->charging = false;
		} else {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d !!\n",
				info->data.temp_t3_thres,
				info->data.temp_t4_thres);

			sw_jeita->sm = TEMP_T3_TO_T4;
		}
	} else if (info->battery_temp >= info->data.temp_t2_thres) {
		if (((sw_jeita->sm == TEMP_T3_TO_T4)
		     && (info->battery_temp
			 >= info->data.temp_t3_thres_minus_x_degree))
		    || ((sw_jeita->sm == TEMP_T1_TO_T2)
			&& (info->battery_temp
			    <= info->data.temp_t2_thres_plus_x_degree))) {
			chr_err("[SW_JEITA] Battery Temperature not recovery to normal temperature charging mode yet!!\n");
		} else {
			chr_err("[SW_JEITA] Battery Normal Temperature between %d and %d !!\n",
				info->data.temp_t2_thres,
				info->data.temp_t3_thres);
			sw_jeita->sm = TEMP_T2_TO_T3;
		}
	} else if (info->battery_temp >= info->data.temp_t1_thres) {
		if ((sw_jeita->sm == TEMP_T0_TO_T1
		     || sw_jeita->sm == TEMP_BELOW_T0)
		    && (info->battery_temp
			<= info->data.temp_t1_thres_plus_x_degree)) {
			if (sw_jeita->sm == TEMP_T0_TO_T1) {
				chr_err("[SW_JEITA] Battery Temperature between %d and %d !!\n",
					info->data.temp_t1_thres_plus_x_degree,
					info->data.temp_t2_thres);
			}
			if (sw_jeita->sm == TEMP_BELOW_T0) {
				chr_err("[SW_JEITA] Battery Temperature between %d and %d,not allow charging yet!!\n",
					info->data.temp_t1_thres,
					info->data.temp_t1_thres_plus_x_degree);
				sw_jeita->charging = false;
			}
		} else {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d !!\n",
				info->data.temp_t1_thres,
				info->data.temp_t2_thres);

			sw_jeita->sm = TEMP_T1_TO_T2;
		}
	} else if (info->battery_temp >= info->data.temp_t0_thres) {
		if ((sw_jeita->sm == TEMP_BELOW_T0)
		    && (info->battery_temp
			<= info->data.temp_t0_thres_plus_x_degree)) {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d,not allow charging yet!!\n",
				info->data.temp_t0_thres,
				info->data.temp_t0_thres_plus_x_degree);

			sw_jeita->charging = false;
		} else {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d !!\n",
				info->data.temp_t0_thres,
				info->data.temp_t1_thres);

			sw_jeita->sm = TEMP_T0_TO_T1;
		}
	} else {
		chr_err("[SW_JEITA] Battery below low Temperature(%d) !!\n",
			info->data.temp_t0_thres);
		sw_jeita->sm = TEMP_BELOW_T0;
		sw_jeita->charging = false;
	}

	/* set CV after temperature changed */
	/* In normal range, we adjust CV dynamically */
	if (sw_jeita->sm != TEMP_T2_TO_T3) {
		if (sw_jeita->sm == TEMP_ABOVE_T4)
			sw_jeita->cv = info->data.jeita_temp_above_t4_cv;
		else if (sw_jeita->sm == TEMP_T3_TO_T4)
			sw_jeita->cv = info->data.jeita_temp_t3_to_t4_cv;
		else if (sw_jeita->sm == TEMP_T2_TO_T3)
			sw_jeita->cv = 0;
		else if (sw_jeita->sm == TEMP_T1_TO_T2)
			sw_jeita->cv = info->data.jeita_temp_t1_to_t2_cv;
		else if (sw_jeita->sm == TEMP_T0_TO_T1)
			sw_jeita->cv = info->data.jeita_temp_t0_to_t1_cv;
		else if (sw_jeita->sm == TEMP_BELOW_T0)
			sw_jeita->cv = info->data.jeita_temp_below_t0_cv;
		else
			sw_jeita->cv = info->data.battery_cv;
	} else {
		sw_jeita->cv = 0;
	}

	chr_err("[SW_JEITA]preState:%d newState:%d tmp:%d cv:%d\n",
		sw_jeita->pre_sm, sw_jeita->sm, info->battery_temp,
		sw_jeita->cv);
}

static int mtk_chgstat_notify(struct mtk_charger *info)
{
	int ret = 0;
	char *env[2] = { "CHGSTAT=1", NULL };

	chr_err("%s: 0x%x\n", __func__, info->notify_code);
	ret = kobject_uevent_env(&info->pdev->dev.kobj, KOBJ_CHANGE, env);
	if (ret)
		chr_err("%s: kobject_uevent_fail, ret=%d", __func__, ret);

	return ret;
}

static void mtk_charger_set_algo_log_level(struct mtk_charger *info, int level)
{
	struct chg_alg_device *alg;
	int i = 0, ret = 0;

	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		if (alg == NULL)
			continue;

		ret = chg_alg_set_prop(alg, ALG_LOG_LEVEL, level);
		if (ret < 0)
			chr_err("%s: set ALG_LOG_LEVEL fail, ret =%d", __func__, ret);
	}
}

static ssize_t sw_jeita_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->enable_sw_jeita);
	return sprintf(buf, "%d\n", pinfo->enable_sw_jeita);
}

static ssize_t sw_jeita_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp == 0)
			pinfo->enable_sw_jeita = false;
		else
			pinfo->enable_sw_jeita = true;

	} else {
		chr_err("%s: format error!\n", __func__);
	}
	return size;
}

static DEVICE_ATTR_RW(sw_jeita);
/* sw jeita end*/

static ssize_t sw_ovp_threshold_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->data.max_charger_voltage);
	return sprintf(buf, "%d\n", pinfo->data.max_charger_voltage);
}

static ssize_t sw_ovp_threshold_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp < 0)
			pinfo->data.max_charger_voltage = pinfo->data.vbus_sw_ovp_voltage;
		else
			pinfo->data.max_charger_voltage = temp;
		chr_err("%s: %d\n", __func__, pinfo->data.max_charger_voltage);

	} else {
		chr_err("%s: format error!\n", __func__);
	}
	return size;
}

static DEVICE_ATTR_RW(sw_ovp_threshold);

static ssize_t chr_type_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->chr_type);
	return sprintf(buf, "%d\n", pinfo->chr_type);
}

static ssize_t chr_type_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0)
		pinfo->chr_type = temp;
	else
		chr_err("%s: format error!\n", __func__);

	return size;
}

static DEVICE_ATTR_RW(chr_type);

static ssize_t pd_type_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;
	char *pd_type_name = "None";

	switch (pinfo->pd_type) {
	case MTK_PD_CONNECT_NONE:
		pd_type_name = "None";
		break;
	case MTK_PD_CONNECT_PE_READY_SNK:
		pd_type_name = "PD";
		break;
	case MTK_PD_CONNECT_PE_READY_SNK_PD30:
		pd_type_name = "PD";
		break;
	case MTK_PD_CONNECT_PE_READY_SNK_APDO:
		pd_type_name = "PD with PPS";
		break;
	case MTK_PD_CONNECT_TYPEC_ONLY_SNK:
		pd_type_name = "normal";
		break;
	}
	chr_err("%s: %d\n", __func__, pinfo->pd_type);
	return sprintf(buf, "%s\n", pd_type_name);
}

static DEVICE_ATTR_RO(pd_type);


static ssize_t Pump_Express_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int ret = 0, i = 0;
	bool is_ta_detected = false;
	struct mtk_charger *pinfo = dev->driver_data;
	struct chg_alg_device *alg = NULL;

	if (!pinfo) {
		chr_err("%s: pinfo is null\n", __func__);
		return sprintf(buf, "%d\n", is_ta_detected);
	}

	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = pinfo->alg[i];
		if (alg == NULL)
			continue;
		ret = chg_alg_is_algo_ready(alg);
		if (ret == ALG_RUNNING) {
			is_ta_detected = true;
			break;
		}
	}
	chr_err("%s: idx = %d, detect = %d\n", __func__, i, is_ta_detected);
	return sprintf(buf, "%d\n", is_ta_detected);
}

static DEVICE_ATTR_RO(Pump_Express);

static ssize_t Charging_mode_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int ret = 0, i = 0;
	char *alg_name = "normal";
	bool is_ta_detected = false;
	struct mtk_charger *pinfo = dev->driver_data;
	struct chg_alg_device *alg = NULL;

	if (!pinfo) {
		chr_err("%s: pinfo is null\n", __func__);
		return sprintf(buf, "%d\n", is_ta_detected);
	}

	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = pinfo->alg[i];
		if (alg == NULL)
			continue;
		ret = chg_alg_is_algo_ready(alg);
		if (ret == ALG_RUNNING) {
			is_ta_detected = true;
			break;
		}
	}
	if (alg == NULL)
		return sprintf(buf, "%s\n", alg_name);

	switch (alg->alg_id) {
	case PE_ID:
		alg_name = "PE";
		break;
	case PE2_ID:
		alg_name = "PE2";
		break;
	case PDC_ID:
		alg_name = "PDC";
		break;
	case PE4_ID:
		alg_name = "PE4";
		break;
	case PE5_ID:
		alg_name = "P5";
		break;
	case PE5P_ID:
		alg_name = "P5P";
		break;
	}
	chr_err("%s: charging_mode: %s\n", __func__, alg_name);
	return sprintf(buf, "%s\n", alg_name);
}

static DEVICE_ATTR_RO(Charging_mode);

static ssize_t High_voltage_chg_enable_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_err("%s: hv_charging = %d\n", __func__, pinfo->enable_hv_charging);
	return sprintf(buf, "%d\n", pinfo->enable_hv_charging);
}

static DEVICE_ATTR_RO(High_voltage_chg_enable);

static ssize_t Rust_detect_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_err("%s: Rust detect = %d\n", __func__, pinfo->record_water_detected);
	return sprintf(buf, "%d\n", pinfo->record_water_detected);
}

static DEVICE_ATTR_RO(Rust_detect);

static ssize_t Thermal_throttle_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;
	struct charger_data *chg_data = &(pinfo->chg_data[CHG1_SETTING]);

	return sprintf(buf, "%d\n", chg_data->thermal_throttle_record);
}

static DEVICE_ATTR_RO(Thermal_throttle);

static ssize_t fast_chg_indicator_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_debug("%s: %d\n", __func__, pinfo->fast_charging_indicator);
	return sprintf(buf, "%d\n", pinfo->fast_charging_indicator);
}

static ssize_t fast_chg_indicator_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	unsigned int temp;

	if (kstrtouint(buf, 10, &temp) == 0)
		pinfo->fast_charging_indicator = temp;
	else
		chr_err("%s: format error!\n", __func__);

	if ((pinfo->fast_charging_indicator > 0) &&
	    (pinfo->bootmode == 8 || pinfo->bootmode == 9)) {
		pinfo->log_level = CHRLOG_DEBUG_LEVEL;
		mtk_charger_set_algo_log_level(pinfo, pinfo->log_level);
	}

	_wake_up_charger(pinfo);
	return size;
}

static DEVICE_ATTR_RW(fast_chg_indicator);

static ssize_t alg_new_arbitration_show(struct device *dev, struct device_attribute *attr,
						char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_debug("%s: %d\n", __func__, pinfo->alg_new_arbitration);
	return sprintf(buf, "%d\n", pinfo->alg_new_arbitration);
}

static ssize_t alg_new_arbitration_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	unsigned int temp;

	if (kstrtouint(buf, 10, &temp) == 0)
		pinfo->alg_new_arbitration = temp;
	else
		chr_err("%s: format error!\n", __func__);

	_wake_up_charger(pinfo);
	return size;
}

static DEVICE_ATTR_RW(alg_new_arbitration);

static ssize_t alg_unchangeable_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_debug("%s: %d\n", __func__, pinfo->alg_unchangeable);
	return sprintf(buf, "%d\n", pinfo->alg_unchangeable);
}

static ssize_t alg_unchangeable_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	unsigned int temp;

	if (kstrtouint(buf, 10, &temp) == 0)
		pinfo->alg_unchangeable = temp;
	else
		chr_err("%s: format error!\n", __func__);

	_wake_up_charger(pinfo);
	return size;
}

static DEVICE_ATTR_RW(alg_unchangeable);

static ssize_t enable_meta_current_limit_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_debug("%s: %d\n", __func__, pinfo->enable_meta_current_limit);
	return sprintf(buf, "%d\n", pinfo->enable_meta_current_limit);
}

static ssize_t enable_meta_current_limit_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	unsigned int temp;

	if (kstrtouint(buf, 10, &temp) == 0)
		pinfo->enable_meta_current_limit = temp;
	else
		chr_err("%s: format error!\n", __func__);

	if (pinfo->enable_meta_current_limit > 0) {
		pinfo->log_level = CHRLOG_DEBUG_LEVEL;
		mtk_charger_set_algo_log_level(pinfo, pinfo->log_level);
	}

	_wake_up_charger(pinfo);
	return size;
}

static DEVICE_ATTR_RW(enable_meta_current_limit);

static ssize_t vbat_mon_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_debug("%s: %d\n", __func__, pinfo->enable_vbat_mon);
	return sprintf(buf, "%d\n", pinfo->enable_vbat_mon);
}

static ssize_t vbat_mon_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	unsigned int temp;

	if (kstrtouint(buf, 10, &temp) == 0) {
		if (temp == 0)
			pinfo->enable_vbat_mon = false;
		else
			pinfo->enable_vbat_mon = true;
	} else {
		chr_err("%s: format error!\n", __func__);
	}

	_wake_up_charger(pinfo);
	return size;
}

static DEVICE_ATTR_RW(vbat_mon);

static ssize_t ADC_Charger_Voltage_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;
	int vbus = get_vbus(pinfo); /* mV */

	chr_err("%s: %d\n", __func__, vbus);
	return sprintf(buf, "%d\n", vbus);
}

static DEVICE_ATTR_RO(ADC_Charger_Voltage);

static ssize_t ADC_Charging_Current_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;
	int ibat = get_battery_current(pinfo); /* mA */

	chr_err("%s: %d\n", __func__, ibat);
	return sprintf(buf, "%d\n", ibat);
}

static DEVICE_ATTR_RO(ADC_Charging_Current);

static ssize_t input_current_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;
	int aicr = 0;

	aicr = pinfo->chg_data[CHG1_SETTING].thermal_input_current_limit;
	chr_err("%s: %d\n", __func__, aicr);
	return sprintf(buf, "%d\n", aicr);
}

static ssize_t input_current_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	struct charger_data *chg_data;
	signed int temp;

	chg_data = &pinfo->chg_data[CHG1_SETTING];
	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp < 0)
			chg_data->thermal_input_current_limit = 0;
		else
			chg_data->thermal_input_current_limit = temp;
	} else {
		chr_err("%s: format error!\n", __func__);
	}
	return size;
}

static DEVICE_ATTR_RW(input_current);

static ssize_t charger_log_level_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->log_level);
	return sprintf(buf, "%d\n", pinfo->log_level);
}

static ssize_t charger_log_level_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp < 0) {
			chr_err("%s: val is invalid: %d\n", __func__, temp);
			temp = 0;
		}
		pinfo->log_level = temp;
		chr_err("%s: log_level=%d\n", __func__, pinfo->log_level);

		mtk_charger_set_algo_log_level(pinfo, pinfo->log_level);
		_wake_up_charger(pinfo);

	} else {
		chr_err("%s: format error!\n", __func__);
	}
	return size;
}

static DEVICE_ATTR_RW(charger_log_level);

static ssize_t BatteryNotify_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_info("%s: 0x%x\n", __func__, pinfo->notify_code);

	return sprintf(buf, "%u\n", pinfo->notify_code);
}

static ssize_t BatteryNotify_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	unsigned int reg = 0;
	int ret = 0;

	if (buf != NULL && size != 0) {
		ret = kstrtouint(buf, 16, &reg);
		if (ret < 0) {
			chr_err("%s: failed, ret = %d\n", __func__, ret);
			return ret;
		}
		pinfo->notify_code = reg;
		chr_info("%s: store code=0x%x\n", __func__, pinfo->notify_code);
		mtk_chgstat_notify(pinfo);
	}
	return size;
}

static DEVICE_ATTR_RW(BatteryNotify);

/* procfs */
static int mtk_chg_set_cv_show(struct seq_file *m, void *data)
{
	struct mtk_charger *pinfo = m->private;

	seq_printf(m, "%d\n", pinfo->data.battery_cv);
	return 0;
}

static int mtk_chg_set_cv_open(struct inode *node, struct file *file)
{
	return single_open(file, mtk_chg_set_cv_show, pde_data(node));
}

static ssize_t mtk_chg_set_cv_write(struct file *file,
		const char *buffer, size_t count, loff_t *data)
{
	int len = 0, ret = 0;
	char desc[32] = {0};
	unsigned int cv = 0;
	struct mtk_charger *info = pde_data(file_inode(file));
	struct power_supply *psy = NULL;
	union  power_supply_propval dynamic_cv;

	if (!info)
		return -EINVAL;
	if (count <= 0)
		return -EINVAL;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	ret = kstrtou32(desc, 10, &cv);
	if (ret == 0) {
		if (cv >= BATTERY_CV) {
			info->data.battery_cv = BATTERY_CV;
			chr_info("%s: adjust charge voltage %dV too high, use default cv\n",
				  __func__, cv);
		} else {
			info->data.battery_cv = cv;
			chr_info("%s: adjust charge voltage = %dV\n", __func__, cv);
		}
		psy = power_supply_get_by_name("battery");
		if (!IS_ERR_OR_NULL(psy)) {
			dynamic_cv.intval = info->data.battery_cv;
			ret = power_supply_set_property(psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &dynamic_cv);
			if (ret < 0)
				chr_err("set gauge cv fail\n");
		}
		return count;
	}

	chr_err("%s: bad argument\n", __func__);
	return count;
}

static const struct proc_ops mtk_chg_set_cv_fops = {
	.proc_open = mtk_chg_set_cv_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = mtk_chg_set_cv_write,
};

static int mtk_chg_current_cmd_show(struct seq_file *m, void *data)
{
	struct mtk_charger *pinfo = m->private;

	seq_printf(m, "%d %d\n", pinfo->usb_unlimited, pinfo->cmd_discharging);
	return 0;
}

static int mtk_chg_current_cmd_open(struct inode *node, struct file *file)
{
	return single_open(file, mtk_chg_current_cmd_show, pde_data(node));
}

static ssize_t mtk_chg_current_cmd_write(struct file *file,
		const char *buffer, size_t count, loff_t *data)
{
	int len = 0;
	char desc[32] = {0};
	int current_unlimited = 0;
	int cmd_discharging = 0;
	struct mtk_charger *info = pde_data(file_inode(file));

	if (!info)
		return -EINVAL;
	if (count <= 0)
		return -EINVAL;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d", &current_unlimited, &cmd_discharging) == 2) {
		info->usb_unlimited = current_unlimited;
		if (cmd_discharging == 1) {
			info->cmd_discharging = true;
			charger_dev_enable(info->chg1_dev, false);
			charger_dev_do_event(info->chg1_dev,
					EVENT_DISCHARGE, 0);
		} else if (cmd_discharging == 0) {
			info->cmd_discharging = false;
			if (!info->smart_soclmt_trig)
				charger_dev_enable(info->chg1_dev, !info->charge_full);
			charger_dev_do_event(info->chg1_dev,
					EVENT_RECHARGE, 0);
		}

		chr_info("%s: current_unlimited=%d, cmd_discharging=%d\n",
			__func__, current_unlimited, cmd_discharging);
		return count;
	}

	chr_err("bad argument, echo [usb_unlimited] [disable] > current_cmd\n");
	return count;
}

static const struct proc_ops mtk_chg_current_cmd_fops = {
	.proc_open = mtk_chg_current_cmd_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = mtk_chg_current_cmd_write,
};

static int mtk_chg_en_power_path_show(struct seq_file *m, void *data)
{
	struct mtk_charger *pinfo = m->private;
	bool power_path_en = true;

	charger_dev_is_powerpath_enabled(pinfo->chg1_dev, &power_path_en);
	seq_printf(m, "%d\n", power_path_en);

	return 0;
}

static int mtk_chg_en_power_path_open(struct inode *node, struct file *file)
{
	return single_open(file, mtk_chg_en_power_path_show, pde_data(node));
}

static ssize_t mtk_chg_en_power_path_write(struct file *file,
		const char *buffer, size_t count, loff_t *data)
{
	int len = 0, ret = 0;
	char desc[32] = {0};
	unsigned int enable = 0;
	struct mtk_charger *info = pde_data(file_inode(file));

	if (!info)
		return -EINVAL;
	if (count <= 0)
		return -EINVAL;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	ret = kstrtou32(desc, 10, &enable);
	if (ret == 0) {
		charger_dev_enable_powerpath(info->chg1_dev, enable);
		chr_info("%s: enable power path = %d\n", __func__, enable);
		return count;
	}

	chr_err("bad argument, echo [enable] > en_power_path\n");
	return count;
}

static const struct proc_ops mtk_chg_en_power_path_fops = {
	.proc_open = mtk_chg_en_power_path_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = mtk_chg_en_power_path_write,
};

static int mtk_chg_en_safety_timer_show(struct seq_file *m, void *data)
{
	struct mtk_charger *pinfo = m->private;
	bool safety_timer_en = false;

	charger_dev_is_safety_timer_enabled(pinfo->chg1_dev, &safety_timer_en);
	seq_printf(m, "%d\n", safety_timer_en);

	return 0;
}

static int mtk_chg_en_safety_timer_open(struct inode *node, struct file *file)
{
	return single_open(file, mtk_chg_en_safety_timer_show, pde_data(node));
}

static ssize_t mtk_chg_en_safety_timer_write(struct file *file,
	const char *buffer, size_t count, loff_t *data)
{
	int len = 0, ret = 0;
	char desc[32] = {0};
	unsigned int enable = 0;
	struct mtk_charger *info = pde_data(file_inode(file));

	if (!info)
		return -EINVAL;
	if (count <= 0)
		return -EINVAL;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	ret = kstrtou32(desc, 10, &enable);
	if (ret == 0) {
		charger_dev_enable_safety_timer(info->chg1_dev, enable);
		info->safety_timer_cmd = (int)enable;
		chr_info("%s: enable safety timer = %d\n", __func__, enable);

		/* SW safety timer */
		if (info->sw_safety_timer_setting == true) {
			if (enable)
				info->enable_sw_safety_timer = true;
			else
				info->enable_sw_safety_timer = false;
		}

		return count;
	}

	chr_err("bad argument, echo [enable] > en_safety_timer\n");
	return count;
}

static const struct proc_ops mtk_chg_en_safety_timer_fops = {
	.proc_open = mtk_chg_en_safety_timer_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = mtk_chg_en_safety_timer_write,
};

int sc_get_sys_time(void)
{
	struct rtc_time tm_android = {0};
	struct timespec64 tv_android = {0};
	int timep = 0;

	ktime_get_real_ts64(&tv_android);
	rtc_time64_to_tm(tv_android.tv_sec, &tm_android);
	tv_android.tv_sec -= (uint64_t)sys_tz.tz_minuteswest * 60;
	rtc_time64_to_tm(tv_android.tv_sec, &tm_android);
	timep = tm_android.tm_sec + tm_android.tm_min * 60 + tm_android.tm_hour * 3600;

	return timep;
}

int sc_get_left_time(int s, int e, int now)
{
	if (e >= s) {
		if (now >= s && now < e)
			return e-now;
	} else {
		if (now >= s)
			return 86400 - now + e;
		else if (now < e)
			return e-now;
	}
	return 0;
}

char *sc_solToStr(int s)
{
	switch (s) {
	case SC_IGNORE:
		return "ignore";
	case SC_KEEP:
		return "keep";
	case SC_DISABLE:
		return "disable";
	case SC_REDUCE:
		return "reduce";
	default:
		return "none";
	}
}

int smart_charging(struct mtk_charger *info)
{
	int time_to_target = 0;
	int time_to_full_default_current = -1;
	int time_to_full_default_current_limit = -1;
	int ret_value = SC_KEEP;
	int sc_real_time = sc_get_sys_time();
	int sc_left_time = sc_get_left_time(info->sc.start_time, info->sc.end_time, sc_real_time);
	int sc_battery_percentage = get_uisoc(info) * 100;
	int sc_charger_current = get_battery_current(info);

	time_to_target = sc_left_time - info->sc.left_time_for_cv;

	if (info->sc.enable == false || sc_left_time <= 0
		|| sc_left_time < info->sc.left_time_for_cv
		|| (sc_charger_current <= 0 && info->sc.last_solution != SC_DISABLE))
		ret_value = SC_IGNORE;
	else {
		if (sc_battery_percentage > info->sc.target_percentage * 100) {
			if (time_to_target > 0)
				ret_value = SC_DISABLE;
		} else {
			if (sc_charger_current != 0)
				time_to_full_default_current =
					info->sc.battery_size * 3600 / 10000 *
					(10000 - sc_battery_percentage)
						/ sc_charger_current;
			else
				time_to_full_default_current =
					info->sc.battery_size * 3600 / 10000 *
					(10000 - sc_battery_percentage);
			chr_err("sc1: %d %d %d %d %d\n",
				time_to_full_default_current,
				info->sc.battery_size,
				sc_battery_percentage,
				sc_charger_current,
				info->sc.current_limit);

			if (time_to_full_default_current < time_to_target &&
				info->sc.current_limit != -1 &&
				sc_charger_current > info->sc.current_limit) {
				time_to_full_default_current_limit =
					info->sc.battery_size / 10000 *
					(10000 - sc_battery_percentage)
					/ info->sc.current_limit;

				chr_err("sc2: %d %d %d %d\n",
					time_to_full_default_current_limit,
					info->sc.battery_size,
					sc_battery_percentage,
					info->sc.current_limit);

				if (time_to_full_default_current_limit < time_to_target &&
					sc_charger_current > info->sc.current_limit)
					ret_value = SC_REDUCE;
			}
		}
	}
	info->sc.last_solution = ret_value;
	if (info->sc.last_solution == SC_DISABLE)
		info->sc.disable_charger = true;
	else
		info->sc.disable_charger = false;
	chr_err("[sc]disable_charger: %d\n", info->sc.disable_charger);
	chr_err("[sc1]en:%d t:%d,%d,%d,%d t:%d,%d,%d,%d c:%d,%d ibus:%d uisoc: %d,%d s:%d ans:%s\n",
		info->sc.enable, info->sc.start_time, info->sc.end_time,
		sc_real_time, sc_left_time, info->sc.left_time_for_cv,
		time_to_target, time_to_full_default_current, time_to_full_default_current_limit,
		sc_charger_current, info->sc.current_limit,
		get_ibus(info), get_uisoc(info), info->sc.target_percentage,
		info->sc.battery_size, sc_solToStr(info->sc.last_solution));

	return ret_value;
}

void sc_select_charging_current(struct mtk_charger *info, struct charger_data *pdata)
{
	if (info->bootmode == 4 || info->bootmode == 1
		|| info->bootmode == 8 || info->bootmode == 9) {
		info->sc.sc_ibat = -1;	/* not normal boot */
		return;
	}
	info->sc.solution = info->sc.last_solution;
	chr_debug("debug: %d, %d, %d\n", info->bootmode,
		info->sc.disable_in_this_plug, info->sc.solution);
	if (info->sc.disable_in_this_plug == false) {
		chr_debug("sck: %d %d %d %d %d\n",
			info->sc.pre_ibat,
			info->sc.sc_ibat,
			pdata->charging_current_limit,
			pdata->thermal_charging_current_limit,
			info->sc.solution);
		if (info->sc.pre_ibat == -1 || info->sc.solution == SC_IGNORE
			|| info->sc.solution == SC_DISABLE) {
			info->sc.sc_ibat = -1;
		} else {
			if (info->sc.pre_ibat == pdata->charging_current_limit
				&& info->sc.solution == SC_REDUCE
				&& ((pdata->charging_current_limit - 100000) >= 500000)) {
				if (info->sc.sc_ibat == -1)
					info->sc.sc_ibat = pdata->charging_current_limit - 100000;

				else {
					if (info->sc.sc_ibat - 100000 >= 500000)
						info->sc.sc_ibat = info->sc.sc_ibat - 100000;
					else
						info->sc.sc_ibat = 500000;
				}
			}
		}
	}
	info->sc.pre_ibat = pdata->charging_current_limit;

	if (pdata->thermal_charging_current_limit != -1) {
		if (pdata->thermal_charging_current_limit <
		    pdata->charging_current_limit)
			pdata->charging_current_limit =
					pdata->thermal_charging_current_limit;
		info->sc.disable_in_this_plug = true;
	} else if ((info->sc.solution == SC_REDUCE || info->sc.solution == SC_KEEP)
		&& info->sc.sc_ibat <
		pdata->charging_current_limit &&
		info->sc.disable_in_this_plug == false) {
		pdata->charging_current_limit = info->sc.sc_ibat;
	}
}

void sc_init(struct smartcharging *sc)
{
	sc->enable = false;
	sc->battery_size = 3000;
	sc->start_time = 0;
	sc->end_time = 80000;
	sc->current_limit = 2000;
	sc->target_percentage = 80;
	sc->left_time_for_cv = 3600;
	sc->pre_ibat = -1;
}

static ssize_t enable_sc_show(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	chr_err(
	"[enable smartcharging] : %d\n",
	info->sc.enable);

	return sprintf(buf, "%d\n", info->sc.enable);
}

static ssize_t enable_sc_store(
	struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (buf != NULL && size != 0) {
		chr_err("[enable smartcharging] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (ret == -ERANGE || ret == -EINVAL)
			return -EINVAL;
		if (val == 0)
			info->sc.enable = false;
		else
			info->sc.enable = true;

		chr_err(
			"[enable smartcharging]enable smartcharging=%d\n",
			info->sc.enable);
	}
	return size;
}
static DEVICE_ATTR_RW(enable_sc);

static ssize_t sc_stime_show(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	chr_err(
	"[smartcharging stime] : %d\n",
	info->sc.start_time);

	return sprintf(buf, "%d\n", info->sc.start_time);
}

static ssize_t sc_stime_store(
	struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	long val = 0;
	int ret;
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (buf != NULL && size != 0) {
		chr_err("[smartcharging stime] buf is %s\n", buf);
		ret = kstrtol(buf, 10, &val);
		if (ret == -ERANGE || ret == -EINVAL)
			return -EINVAL;
		if (val < 0) {
			chr_err(
				"[smartcharging stime] val is %ld ??\n",
				val);
			val = 0;
		}

		if (val >= 0)
			info->sc.start_time = (int)val;

		chr_err(
			"[smartcharging stime]enable smartcharging=%d\n",
			info->sc.start_time);
	}
	return size;
}
static DEVICE_ATTR_RW(sc_stime);

static ssize_t sc_etime_show(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	chr_err(
	"[smartcharging etime] : %d\n",
	info->sc.end_time);

	return sprintf(buf, "%d\n", info->sc.end_time);
}

static ssize_t sc_etime_store(
	struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	long val = 0;
	int ret;
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (buf != NULL && size != 0) {
		chr_err("[smartcharging etime] buf is %s\n", buf);
		ret = kstrtol(buf, 10, &val);
		if (ret == -ERANGE || ret == -EINVAL)
			return -EINVAL;
		if (val < 0) {
			chr_err(
				"[smartcharging etime] val is %ld ??\n",
				val);
			val = 0;
		}

		if (val >= 0)
			info->sc.end_time = (int)val;

		chr_err(
			"[smartcharging stime]enable smartcharging=%d\n",
			info->sc.end_time);
	}
	return size;
}
static DEVICE_ATTR_RW(sc_etime);

static ssize_t sc_tuisoc_show(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	chr_err(
	"[smartcharging target uisoc] : %d\n",
	info->sc.target_percentage);

	return sprintf(buf, "%d\n", info->sc.target_percentage);
}

static ssize_t sc_tuisoc_store(
	struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	long val = 0;
	int ret;
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (buf != NULL && size != 0) {
		chr_err("[smartcharging tuisoc] buf is %s\n", buf);
		ret = kstrtol(buf, 10, &val);
		if (ret == -ERANGE || ret == -EINVAL)
			return -EINVAL;
		if (val < 0) {
			chr_err(
				"[smartcharging tuisoc] val is %ld ??\n",
				val);
			val = 0;
		}

		if (val >= 0)
			info->sc.target_percentage = (int)val;

		chr_err(
			"[smartcharging stime]tuisoc=%d\n",
			info->sc.target_percentage);
	}
	return size;
}
static DEVICE_ATTR_RW(sc_tuisoc);

static ssize_t sc_ibat_limit_show(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	chr_err(
	"[smartcharging ibat limit] : %d\n",
	info->sc.current_limit);

	return sprintf(buf, "%d\n", info->sc.current_limit);
}

static ssize_t sc_ibat_limit_store(
	struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	long val = 0;
	int ret;
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (buf != NULL && size != 0) {
		chr_err("[smartcharging ibat limit] buf is %s\n", buf);
		ret = kstrtol(buf, 10, &val);
		if (ret == -ERANGE || ret == -EINVAL)
			return -EINVAL;
		if (val < 0) {
			chr_err(
				"[smartcharging ibat limit] val is %ld ??\n",
				val);
			val = 0;
		}

		if (val >= 0)
			info->sc.current_limit = (int)val;

		chr_err(
			"[smartcharging ibat limit]=%d\n",
			info->sc.current_limit);
	}
	return size;
}
static DEVICE_ATTR_RW(sc_ibat_limit);

static ssize_t enable_power_path_show(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;
	bool power_path_en = true;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	charger_dev_is_powerpath_enabled(info->chg1_dev, &power_path_en);
	return sprintf(buf, "%d\n", power_path_en);
}

static ssize_t enable_power_path_store(
	struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	long val = 0;
	int ret;
	bool enable = true;
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (buf != NULL && size != 0) {
		ret = kstrtoul(buf, 10, &val);
		if (ret == -ERANGE || ret == -EINVAL)
			return -EINVAL;
		if (val == 0)
			enable = false;
		else
			enable = true;

		charger_dev_enable_powerpath(info->chg1_dev, enable);
		info->cmd_pp = enable;
		chr_err("%s: enable power path = %d\n", __func__, enable);
	}

	return size;
}
static DEVICE_ATTR_RW(enable_power_path);


static ssize_t product_name_show(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return sprintf(buf, "%s\n", "unknown");

	return sprintf(buf, "%s\n", info->product_name);
}

static ssize_t product_name_store(
    struct device *dev, struct device_attribute *attr,
    const char *buf, size_t size)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (!chg_psy || IS_ERR(chg_psy)) {
		chr_err("%s: Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (!info)
		return -EINVAL;

	if (size >= 64) {
		chr_err("set product name error\n");
		strlcpy(info->product_name, "unknown", 8);
		return -EINVAL;
	}

	strlcpy(info->product_name, buf, 64);

	info->product_name_index = UNKNOWN;
	if (strstr(info->product_name, "eea")) {
		info->product_name_index = EEA;
	}

	chr_err("product name: %s, index: %d\n",
            info->product_name, info->product_name_index);

	return size;
}
static DEVICE_ATTR_RW(product_name);

int mtk_chg_enable_vbus_ovp(bool enable)
{
	static struct mtk_charger *pinfo;
	int ret = 0;
	u32 sw_ovp = 0;
	struct power_supply *psy;

	if (pinfo == NULL) {
		psy = power_supply_get_by_name("mtk-master-charger");
		if (psy == NULL) {
			chr_err("[%s]psy is not rdy\n", __func__);
			return -1;
		}

		pinfo = (struct mtk_charger *)power_supply_get_drvdata(psy);
		if (pinfo == NULL) {
			chr_err("[%s]mtk_gauge is not rdy\n", __func__);
			return -1;
		}
	}

	if (enable)
		sw_ovp = pinfo->data.max_charger_voltage_setting;
	else
		sw_ovp = pinfo->data.vbus_sw_ovp_voltage;

	/* Enable/Disable SW OVP status */
	pinfo->data.max_charger_voltage = sw_ovp;

	disable_hw_ovp(pinfo, enable);

	chr_err("[%s] en:%d ovp:%d\n",
			    __func__, enable, sw_ovp);
	return ret;
}
EXPORT_SYMBOL(mtk_chg_enable_vbus_ovp);

/* return false if vbus is over max_charger_voltage */
static bool mtk_chg_check_vbus(struct mtk_charger *info)
{
	int vchr = 0;

	vchr = get_vbus(info) * 1000; /* uV */
	if (vchr > info->data.max_charger_voltage) {
		chr_err("%s: vbus(%d mV) > %d mV\n", __func__, vchr / 1000,
			info->data.max_charger_voltage / 1000);
		return false;
	}
	return true;
}

static void mtk_battery_notify_VCharger_check(struct mtk_charger *info)
{
#if defined(BATTERY_NOTIFY_CASE_0001_VCHARGER)
	int vchr = 0;

	vchr = get_vbus(info) * 1000; /* uV */
	if (vchr < info->data.max_charger_voltage)
		info->notify_code &= ~CHG_VBUS_OV_STATUS;
	else {
		info->notify_code |= CHG_VBUS_OV_STATUS;
		chr_err("[BATTERY] charger_vol(%d mV) > %d mV\n",
			vchr / 1000, info->data.max_charger_voltage / 1000);
		mtk_chgstat_notify(info);
	}
#endif
}

static void mtk_battery_notify_VBatTemp_check(struct mtk_charger *info)
{
#if defined(BATTERY_NOTIFY_CASE_0002_VBATTEMP)
	if (info->battery_temp >= info->thermal.max_charge_temp) {
		info->notify_code |= CHG_BAT_OT_STATUS;
		chr_err("[BATTERY] bat_temp(%d) out of range(too high)\n",
			info->battery_temp);
		mtk_chgstat_notify(info);
	} else {
		info->notify_code &= ~CHG_BAT_OT_STATUS;
	}

	if (info->enable_sw_jeita == true) {
		if (info->battery_temp < info->data.temp_neg_10_thres) {
			info->notify_code |= CHG_BAT_LT_STATUS;
			chr_err("bat_temp(%d) out of range(too low)\n",
				info->battery_temp);
			mtk_chgstat_notify(info);
		} else {
			info->notify_code &= ~CHG_BAT_LT_STATUS;
		}
	} else {
#ifdef BAT_LOW_TEMP_PROTECT_ENABLE
		if (info->battery_temp < info->thermal.min_charge_temp) {
			info->notify_code |= CHG_BAT_LT_STATUS;
			chr_err("bat_temp(%d) out of range(too low)\n",
				info->battery_temp);
			mtk_chgstat_notify(info);
		} else {
			info->notify_code &= ~CHG_BAT_LT_STATUS;
		}
#endif
	}
#endif
}

static void mtk_battery_notify_VChargerDPDM_check(struct mtk_charger *info)
{
	if (!info->dpdmov_stat)
		info->notify_code &= ~CHG_DPDM_OV_STATUS;
	else {
		info->notify_code |= CHG_DPDM_OV_STATUS;
		chr_err("[BATTERY] DP/DM over voltage!\n");
	}
	if (info->dpdmov_stat != info->lst_dpdmov_stat) {
		mtk_chgstat_notify(info);
		info->lst_dpdmov_stat = info->dpdmov_stat;
	}
}

static void mtk_battery_notify_UI_test(struct mtk_charger *info)
{
	switch (info->notify_test_mode) {
	case 1:
		info->notify_code = CHG_VBUS_OV_STATUS;
		chr_debug("[%s] CASE_0001_VCHARGER\n", __func__);
		break;
	case 2:
		info->notify_code = CHG_BAT_OT_STATUS;
		chr_debug("[%s] CASE_0002_VBATTEMP\n", __func__);
		break;
	case 3:
		info->notify_code = CHG_OC_STATUS;
		chr_debug("[%s] CASE_0003_ICHARGING\n", __func__);
		break;
	case 4:
		info->notify_code = CHG_BAT_OV_STATUS;
		chr_debug("[%s] CASE_0004_VBAT\n", __func__);
		break;
	case 5:
		info->notify_code = CHG_ST_TMO_STATUS;
		chr_debug("[%s] CASE_0005_TOTAL_CHARGINGTIME\n", __func__);
		break;
	case 6:
		info->notify_code = CHG_BAT_LT_STATUS;
		chr_debug("[%s] CASE6: VBATTEMP_LOW\n", __func__);
		break;
	case 7:
		info->notify_code = CHG_TYPEC_WD_STATUS;
		chr_debug("[%s] CASE7: Moisture Detection\n", __func__);
		break;
	default:
		chr_debug("[%s] Unknown BN_TestMode Code: %x\n",
			__func__, info->notify_test_mode);
	}
	mtk_chgstat_notify(info);
}

static void mtk_battery_notify_check(struct mtk_charger *info)
{
	if (info->notify_test_mode == 0x0000) {
		mtk_battery_notify_VCharger_check(info);
		mtk_battery_notify_VBatTemp_check(info);
		mtk_battery_notify_VChargerDPDM_check(info);
	} else {
		mtk_battery_notify_UI_test(info);
	}
}

static void mtk_chg_get_tchg(struct mtk_charger *info)
{
	int ret;
	int tchg_min = -127, tchg_max = -127;
	struct charger_data *pdata;

	pdata = &info->chg_data[CHG1_SETTING];
	ret = charger_dev_get_temperature(info->chg1_dev, &tchg_min, &tchg_max);
	if (ret < 0) {
		pdata->junction_temp_min = -127;
		pdata->junction_temp_max = -127;
	} else {
		pdata->junction_temp_min = tchg_min;
		pdata->junction_temp_max = tchg_max;
	}

	if (info->chg2_dev) {
		pdata = &info->chg_data[CHG2_SETTING];
		ret = charger_dev_get_temperature(info->chg2_dev,
			&tchg_min, &tchg_max);

		if (ret < 0) {
			pdata->junction_temp_min = -127;
			pdata->junction_temp_max = -127;
		} else {
			pdata->junction_temp_min = tchg_min;
			pdata->junction_temp_max = tchg_max;
		}
	}

	if (info->dvchg1_dev) {
		pdata = &info->chg_data[DVCHG1_SETTING];
		ret = charger_dev_get_adc(info->dvchg1_dev,
					  ADC_CHANNEL_TEMP_JC,
					  &tchg_min, &tchg_max);
		if (ret < 0) {
			pdata->junction_temp_min = -127;
			pdata->junction_temp_max = -127;
		} else {
			pdata->junction_temp_min = tchg_min;
			pdata->junction_temp_max = tchg_max;
		}
	}

	if (info->dvchg2_dev) {
		pdata = &info->chg_data[DVCHG2_SETTING];
		ret = charger_dev_get_adc(info->dvchg2_dev,
					  ADC_CHANNEL_TEMP_JC,
					  &tchg_min, &tchg_max);
		if (ret < 0) {
			pdata->junction_temp_min = -127;
			pdata->junction_temp_max = -127;
		} else {
			pdata->junction_temp_min = tchg_min;
			pdata->junction_temp_max = tchg_max;
		}
	}

	if (info->hvdvchg1_dev) {
		pdata = &info->chg_data[HVDVCHG1_SETTING];
		ret = charger_dev_get_adc(info->hvdvchg1_dev,
					  ADC_CHANNEL_TEMP_JC,
					  &tchg_min, &tchg_max);
		if (ret < 0) {
			pdata->junction_temp_min = -127;
			pdata->junction_temp_max = -127;
		} else {
			pdata->junction_temp_min = tchg_min;
			pdata->junction_temp_max = tchg_max;
		}
	}

	if (info->hvdvchg2_dev) {
		pdata = &info->chg_data[HVDVCHG2_SETTING];
		ret = charger_dev_get_adc(info->hvdvchg2_dev,
					  ADC_CHANNEL_TEMP_JC,
					  &tchg_min, &tchg_max);
		if (ret < 0) {
			pdata->junction_temp_min = -127;
			pdata->junction_temp_max = -127;
		} else {
			pdata->junction_temp_min = tchg_min;
			pdata->junction_temp_max = tchg_max;
		}
	}
}

static int first_charger_type = 0;
static void get_first_charger_type(struct mtk_charger *info)
{
	struct timespec64 time_now;
	ktime_t ktime_now;
	ktime_now = ktime_get_boottime();
	time_now = ktime_to_timespec64(ktime_now);
	if (time_now.tv_sec <= 15 && (get_charger_type(info) == POWER_SUPPLY_TYPE_USB_CDP))
		first_charger_type = POWER_SUPPLY_TYPE_USB_CDP;
}

static void charger_check_status(struct mtk_charger *info)
{
	bool charging = true;
	bool chg_dev_chgen = true;
	int temperature;
	struct battery_thermal_protection_data *thermal;
	int uisoc = 0;

	if (get_charger_type(info) == POWER_SUPPLY_TYPE_UNKNOWN)
		return;

	if (get_charger_type(info) == POWER_SUPPLY_TYPE_USB_CDP)
		get_first_charger_type(info);

	temperature = info->battery_temp;
	thermal = &info->thermal;
	uisoc = get_uisoc(info);

	info->setting.vbat_mon_en = true;
	if (info->enable_sw_jeita == true || info->enable_vbat_mon != true ||
	    info->batpro_done == true)
		info->setting.vbat_mon_en = false;

	if (info->enable_sw_jeita == true) {
		do_sw_jeita_state_machine(info);
		if (info->sw_jeita.charging == false) {
			charging = false;
			goto stop_charging;
		}
	} else {

		if (thermal->enable_min_charge_temp) {
			if (temperature <= thermal->min_charge_temp) {
				chr_err("Battery Under Temperature or NTC fail %d %d\n",
					temperature, thermal->min_charge_temp);
				thermal->sm = BAT_TEMP_LOW;
				charging = false;
				goto stop_charging;
			} else if (thermal->sm == BAT_TEMP_LOW) {
				if (temperature >
				    thermal->min_charge_temp_plus_x_degree) {
					chr_err("Battery Temperature raise from %d to %d(%d), allow charging!!\n",
					thermal->min_charge_temp,
					temperature,
					thermal->min_charge_temp_plus_x_degree);
					thermal->sm = BAT_TEMP_NORMAL;
				} else {
					charging = false;
					goto stop_charging;
				}
			}
		}

		if (temperature >= thermal->max_charge_temp) {
			chr_err("Battery over Temperature or NTC fail %d %d\n",
				temperature, thermal->max_charge_temp);
			thermal->sm = BAT_TEMP_HIGH;
			charging = false;
			goto stop_charging;
		} else if (thermal->sm == BAT_TEMP_HIGH) {
			if (temperature
			    < thermal->max_charge_temp_minus_x_degree) {
				chr_err("Battery Temperature raise from %d to %d(%d), allow charging!!\n",
				thermal->max_charge_temp,
				temperature,
				thermal->max_charge_temp_minus_x_degree);
				thermal->sm = BAT_TEMP_NORMAL;
			} else {
				charging = false;
				goto stop_charging;
			}
		}
	}

	mtk_chg_get_tchg(info);

	if (!mtk_chg_check_vbus(info)) {
		charging = false;
		goto stop_charging;
	}

	if (info->cmd_discharging)
		charging = false;
	if (info->safety_timeout && (!info->is_mtbf_mode))
		charging = false;
	if (info->vbusov_stat)
		charging = false;
	if (info->dpdmov_stat)
		charging = false;
	if (info->sc.disable_charger == true)
		charging = false;
stop_charging:
	mtk_battery_notify_check(info);

	if (charging && uisoc < 80 && info->batpro_done == true) {
		info->setting.vbat_mon_en = true;
		info->batpro_done = false;
		info->stop_6pin_re_en = false;
	}

	chr_err("tmp:%d (jeita:%d sm:%d cv:%d en:%d) (sm:%d) en:%d c:%d s:%d ov:%d %d sc:%d %d %d saf_cmd:%d bat_mon:%d %d\n",
		temperature, info->enable_sw_jeita, info->sw_jeita.sm,
		info->sw_jeita.cv, info->sw_jeita.charging, thermal->sm,
		charging, info->cmd_discharging, info->safety_timeout,
		info->vbusov_stat, info->dpdmov_stat, info->sc.disable_charger,
		info->can_charging, charging, info->safety_timer_cmd,
		info->enable_vbat_mon, info->batpro_done);

	charger_dev_is_enabled(info->chg1_dev, &chg_dev_chgen);

	if (charging != info->can_charging)
		_mtk_enable_charging(info, charging);
	else if (charging == false && chg_dev_chgen == true)
		_mtk_enable_charging(info, charging);

	info->can_charging = charging;
}

static bool charger_init_algo(struct mtk_charger *info)
{
	struct chg_alg_device *alg;
	int idx = 0;

	info->chg1_dev = get_charger_by_name("primary_chg");
	if (info->chg1_dev)
		chr_err("%s, Found primary charger\n", __func__);
	else {
		chr_err("%s, *** Error : can't find primary charger ***\n"
			, __func__);
		return false;
	}

	alg = get_chg_alg_by_name("pe5p");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get pe5p fail\n");
	else {
		chr_err("get pe5p success\n");
		alg->config = info->config;
		alg->alg_id = PE5P_ID;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}
	idx++;

	alg = get_chg_alg_by_name("hvbp");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get hvbp fail\n");
	else {
		chr_err("get hvbp success\n");
		alg->config = info->config;
		alg->alg_id = HVBP_ID;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}
	idx++;

	alg = get_chg_alg_by_name("pe5");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get pe5 fail\n");
	else {
		chr_err("get pe5 success\n");
		alg->config = info->config;
		alg->alg_id = PE5_ID;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}
	idx++;

	alg = get_chg_alg_by_name("pe45");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get pe45 fail\n");
	else {
		chr_err("get pe45 success\n");
		alg->config = info->config;
		alg->alg_id = PE4_ID;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}
	idx++;

	alg = get_chg_alg_by_name("pe4");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get pe4 fail\n");
	else {
		chr_err("get pe4 success\n");
		alg->config = info->config;
		alg->alg_id = PE4_ID;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}
	idx++;

	alg = get_chg_alg_by_name("pd");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get pd fail\n");
	else {
		chr_err("get pd success\n");
		alg->config = info->config;
		alg->alg_id = PDC_ID;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}
	idx++;

	alg = get_chg_alg_by_name("pe2");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get pe2 fail\n");
	else {
		chr_err("get pe2 success\n");
		alg->config = info->config;
		alg->alg_id = PE2_ID;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}
	idx++;

	alg = get_chg_alg_by_name("pe");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get pe fail\n");
	else {
		chr_err("get pe success\n");
		alg->config = info->config;
		alg->alg_id = PE_ID;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}

	chr_err("config is %d\n", info->config);
	if (info->config == DUAL_CHARGERS_IN_SERIES) {
		info->chg2_dev = get_charger_by_name("secondary_chg");
		if (info->chg2_dev)
			chr_err("Found secondary charger\n");
		else {
			chr_err("*** Error : can't find secondary charger ***\n");
			return false;
		}
	} else if (info->config == DIVIDER_CHARGER ||
		   info->config == DUAL_DIVIDER_CHARGERS) {
		info->dvchg1_dev = get_charger_by_name("primary_dvchg");
		if (info->dvchg1_dev)
			chr_err("Found primary divider charger\n");
		else {
			chr_err("*** Error : can't find primary divider charger ***\n");
			return false;
		}
		if (info->config == DUAL_DIVIDER_CHARGERS) {
			info->dvchg2_dev =
				get_charger_by_name("secondary_dvchg");
			if (info->dvchg2_dev)
				chr_err("Found secondary divider charger\n");
			else {
				chr_err("*** Error : can't find secondary divider charger ***\n");
				return false;
			}
		}
	} else if (info->config == HVDIVIDER_CHARGER ||
		   info->config == DUAL_HVDIVIDER_CHARGERS) {
		info->hvdvchg1_dev = get_charger_by_name("hvdiv2_chg1");
		if (info->hvdvchg1_dev)
			chr_err("Found primary hvdivider charger\n");
		else {
			chr_err("*** Error : can't find primary hvdivider charger ***\n");
			return false;
		}
		if (info->config == DUAL_HVDIVIDER_CHARGERS) {
			info->hvdvchg2_dev = get_charger_by_name("hvdiv2_chg2");
			if (info->hvdvchg2_dev)
				chr_err("Found secondary hvdivider charger\n");
			else {
				chr_err("*** Error : can't find secondary hvdivider charger ***\n");
				return false;
			}
		}
	}

	chr_err("register chg1 notifier %d %d\n",
		info->chg1_dev != NULL, info->algo.do_event != NULL);
	if (info->chg1_dev != NULL && info->algo.do_event != NULL) {
		chr_err("register chg1 notifier done\n");
		info->chg1_nb.notifier_call = info->algo.do_event;
		register_charger_device_notifier(info->chg1_dev,
						&info->chg1_nb);
		charger_dev_set_drvdata(info->chg1_dev, info);
	}

	chr_err("register dvchg chg1 notifier %d %d\n",
		info->dvchg1_dev != NULL, info->algo.do_dvchg1_event != NULL);
	if (info->dvchg1_dev != NULL && info->algo.do_dvchg1_event != NULL) {
		chr_err("register dvchg chg1 notifier done\n");
		info->dvchg1_nb.notifier_call = info->algo.do_dvchg1_event;
		register_charger_device_notifier(info->dvchg1_dev,
						&info->dvchg1_nb);
		charger_dev_set_drvdata(info->dvchg1_dev, info);
	}

	chr_err("register dvchg chg2 notifier %d %d\n",
		info->dvchg2_dev != NULL, info->algo.do_dvchg2_event != NULL);
	if (info->dvchg2_dev != NULL && info->algo.do_dvchg2_event != NULL) {
		chr_err("register dvchg chg2 notifier done\n");
		info->dvchg2_nb.notifier_call = info->algo.do_dvchg2_event;
		register_charger_device_notifier(info->dvchg2_dev,
						 &info->dvchg2_nb);
		charger_dev_set_drvdata(info->dvchg2_dev, info);
	}

	chr_err("register hvdvchg chg1 notifier %d %d\n",
		info->hvdvchg1_dev != NULL,
		info->algo.do_hvdvchg1_event != NULL);
	if (info->hvdvchg1_dev != NULL &&
	    info->algo.do_hvdvchg1_event != NULL) {
		chr_err("register hvdvchg chg1 notifier done\n");
		info->hvdvchg1_nb.notifier_call = info->algo.do_hvdvchg1_event;
		register_charger_device_notifier(info->hvdvchg1_dev,
						 &info->hvdvchg1_nb);
		charger_dev_set_drvdata(info->hvdvchg1_dev, info);
	}

	chr_err("register hvdvchg chg2 notifier %d %d\n",
		info->hvdvchg2_dev != NULL,
		info->algo.do_hvdvchg2_event != NULL);
	if (info->hvdvchg2_dev != NULL &&
	    info->algo.do_hvdvchg2_event != NULL) {
		chr_err("register hvdvchg chg2 notifier done\n");
		info->hvdvchg2_nb.notifier_call = info->algo.do_hvdvchg2_event;
		register_charger_device_notifier(info->hvdvchg2_dev,
						 &info->hvdvchg2_nb);
		charger_dev_set_drvdata(info->hvdvchg2_dev, info);
	}

	return true;
}

static int mtk_charger_force_disable_power_path(struct mtk_charger *info,
	int idx, bool disable);
static int mtk_charger_plug_out(struct mtk_charger *info)
{
	struct charger_data *pdata1 = &info->chg_data[CHG1_SETTING];
	struct charger_data *pdata2 = &info->chg_data[CHG2_SETTING];
	struct chg_alg_device *alg;
	struct chg_alg_notify notify;
	int i;
	int intval = 0;

	chr_info("%s +++++\n", __func__);
	info->chr_type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->charger_thread_polling = false;
	info->pd_reset = false;
	info->pd_verify_done = false;
	info->pd_verifed = false;
	info->entry_soc = 0;
	info->apdo_max = 0;
	info->adapter_imax = -1;
	info->suspend_recovery = false;
	info->fg_full = false;
	info->charge_full = false;
	info->real_full = false;
	info->charge_eoc = false;
	info->recharge = false;
	info->warm_term = false;
	info->real_type = XMUSB350_TYPE_UNKNOW;
	info->thermal_current = 0;
	info->pmic_comp_v = 0;
	info->thermal_remove = false;
	info->plugged_status = false;
	info->cp_sm_run_state = false;
	info->last_ffc_enable = false;
	info->ffc_enable = false;
	info->rerun_ffc_enable = false;
	vote(info->fv_votable, FV_DEC_VOTER, false, 0);
	vote(info->fcc_votable, THERMAL_VOTER, false, 0);
	vote(info->fcc_votable, XM_BATT_HEALTH_VOTER, false, 0);
	atomic_set(&info->ieoc_wkrd, 0);
	charger_dev_do_event(info->chg1_dev, EVENT_DISCHARGE, 0);
	info->dpdmov_stat = false;
	info->lst_dpdmov_stat = false;
	info->plug_in_soc100_flag = false;
	info->hvdcp_setp_down = false;

	pdata1->disable_charging_count = 0;
	pdata1->input_current_limit_by_aicl = -1;
	pdata2->disable_charging_count = 0;

	notify.evt = EVT_PLUG_OUT;
	notify.value = 0;
	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		chg_alg_notifier_call(alg, &notify);
		chg_alg_plugout_reset(alg);
	}
	memset(&info->sc.data, 0, sizeof(struct scd_cmd_param_t_1));
	//wakeup_sc_algo_cmd(&info->sc.data, SC_EVENT_PLUG_OUT, 0);
	vote(info->icl_votable, ICL_VOTER, false, 0);
	vote(info->icl_votable, CHARGERIC_VOTER, false, 0);
	vote(info->fcc_votable, CHARGERIC_VOTER, false, 0);
	vote(info->icl_votable, FG_ERR_VOTER, false, 0);
	vote(info->fcc_votable, FG_ERR_VOTER, false, 0);
	vote(info->fv_votable, FG_ERR_VOTER, false, 0);
	vote(info->fcc_votable, CP_CHG_DONE, false, 0);
	vote(info->icl_votable, SINK_VBUS_VOTER, false, 0);
	charger_dev_set_input_current(info->chg1_dev, 100000);
	charger_dev_set_mivr(info->chg1_dev, info->data.min_charger_voltage);
	charger_dev_plug_out(info->chg1_dev);
	if (info->jeita_support) {
		charger_dev_enable_termination(info->chg1_dev, true);
		cancel_delayed_work_sync(&info->charge_monitor_work);
		cancel_delayed_work_sync(&info->supplement_charge_work);
		if (timer_pending(&info->supplement_charge_timer))
			del_timer_sync(&info->supplement_charge_timer);

		reset_step_jeita_charge(info);
		chr_err("%s cancel_monitor_delayed_work_sync\n", __func__);
	}

#if IS_ENABLED(CONFIG_XM_SMART_CHG)
	xm_smart_chg_stop(info);
#endif
#if IS_ENABLED(CONFIG_XM_BATTERY_HEALTH)
	xm_batt_health_stop(info);
#endif
	mtk_charger_force_disable_power_path(info, CHG1_SETTING, true);

	if (info->enable_vbat_mon)
		charger_dev_enable_6pin_battery_charging(info->chg1_dev, false);

	if (info->cp_master) {
		charger_dev_cp_clear_fault_type(info->cp_master);
		charger_dev_cp_set_en_fail_status(info->cp_master, false);
	}

	if (info) {
		usb_get_property(info, USB_PROP_QUICK_CHARGE_TYPE, &intval);
		xm_charge_uevent_report(CHG_UEVENT_QUICK_CHARGE_TYPE, intval);
	}

	cancel_delayed_work_sync(&info->start_vbus_check_work);
	info->vbus_check = false;

	/* report plugout event */
	mtk_charger_fw_notifier_call_chain(CHG_FW_EVT_ADAPTER_PLUGOUT, NULL);

	return 0;
}

static int mtk_charger_plug_in(struct mtk_charger *info,
				int chr_type)
{
	union power_supply_propval pval = {0,};
	struct chg_alg_device *alg;
	struct chg_alg_notify notify;
	int i, vbat;
	int ret = 0;

	chr_info("%s chr_type: %d +++++\n", __func__, chr_type);

	if (info->cp_master) {
		charger_dev_cp_init_check(info->cp_master);
	}


	if (info->bat_psy) {
		ret = power_supply_get_property(info->bat_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (ret)
			chr_err("failed to get vbat\n");
		else
			info->vbat_now = pval.intval / 1000;

		ret = power_supply_get_property(info->bat_psy, POWER_SUPPLY_PROP_TEMP, &pval);
		if (ret)
			chr_err("failed to get tbat\n");
		else
			info->temp_now = pval.intval;
	}

	if (info->fcc_votable)
		reset_step_jeita_charge(info);

	info->chr_type = chr_type;
	info->usb_type = get_usb_type(info);
	info->charger_thread_polling = true;

	info->can_charging = true;
	//info->enable_dynamic_cv = true;
	info->safety_timeout = false;
	info->vbusov_stat = false;
	info->vbusbad_stat = false;
	info->old_cv = 0;
	info->stop_6pin_re_en = false;
	info->batpro_done = false;

	info->entry_soc = get_uisoc(info);
	info->fg_full = false;
	info->charge_full = false;
	info->real_full = false;
	info->charge_eoc = false;
	info->warm_term = false;
	info->pmic_comp_v = 0;
	info->plugged_status = true;
	info->last_ffc_enable = false;
	info->cp_sm_run_state = false;
	atomic_set(&info->ieoc_wkrd, 0);
	smart_charging(info);

	vbat = get_battery_voltage(info);

	notify.evt = EVT_PLUG_IN;
	notify.value = 0;
	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		chg_alg_notifier_call(alg, &notify);
		chg_alg_set_prop(alg, ALG_REF_VBAT, vbat);
	}

	memset(&info->sc.data, 0, sizeof(struct scd_cmd_param_t_1));
	info->sc.disable_in_this_plug = false;
	//wakeup_sc_algo_cmd(&info->sc.data, SC_EVENT_PLUG_IN, 0);
	charger_dev_plug_in(info->chg1_dev);
	if (info->jeita_support) {
		info->supplement_chg_status = true;
		schedule_delayed_work(&info->charge_monitor_work, 0);
		charger_dev_enable_termination(info->chg1_dev, false);
		chr_err("%s schedule_monitor_delayed_work\n", __func__);
	}

#if IS_ENABLED(CONFIG_XM_SMART_CHG)
	xm_smart_chg_run(info);
#endif
#if IS_ENABLED(CONFIG_XM_BATTERY_HEALTH)
	xm_batt_health_run(info);
#endif

	typec_burn_timer_start(info);

	mtk_charger_force_disable_power_path(info, CHG1_SETTING, false);

	vote(info->fv_votable, FV_DEC_VOTER, false, 0);

	schedule_delayed_work(&info->start_vbus_check_work, 3000);

	/* report plugin event */
	mtk_charger_fw_notifier_call_chain(CHG_FW_EVT_ADAPTER_PLUGIN, NULL);

	return 0;
}

static bool mtk_is_charger_on(struct mtk_charger *info)
{
	int chr_type;

	chr_type = get_charger_type(info);
	if (chr_type == POWER_SUPPLY_TYPE_UNKNOWN) {
		if (info->chr_type != POWER_SUPPLY_TYPE_UNKNOWN) {
			mtk_charger_plug_out(info);
			mutex_lock(&info->cable_out_lock);
			info->cable_out_cnt = 0;
			mutex_unlock(&info->cable_out_lock);
		}
	} else {
		if (info->chr_type != chr_type)
			mtk_charger_plug_in(info, chr_type);

		if (info->cable_out_cnt > 0) {
			mtk_charger_plug_out(info);
			mtk_charger_plug_in(info, chr_type);
			mutex_lock(&info->cable_out_lock);
			info->cable_out_cnt = 0;
			mutex_unlock(&info->cable_out_lock);
		}
	}

	if (chr_type == POWER_SUPPLY_TYPE_UNKNOWN)
		return false;

	return true;
}

static void charger_send_kpoc_uevent(struct mtk_charger *info)
{
	static bool first_time = true;
	ktime_t ktime_now;

	if (first_time) {
		info->uevent_time_check = ktime_get();
		first_time = false;
	} else {
		ktime_now = ktime_get();
		if ((ktime_ms_delta(ktime_now, info->uevent_time_check) / 1000) >= 60) {
			mtk_chgstat_notify(info);
			info->uevent_time_check = ktime_now;
		}
	}
}

static void kpoc_power_off_check(struct mtk_charger *info)
{
	unsigned int boot_mode = info->bootmode;
	int vbus = 0;
	struct timespec64 time_now;
	ktime_t ktime_now;
	static int vcount = 0;
	ktime_now = ktime_get_boottime();
	time_now = ktime_to_timespec64(ktime_now);
	if (boot_mode == 8 || boot_mode == 9) {
		vbus = get_vbus(info);
		/*chr_err("kpoc_power_off_check vbus=%d\n", vbus);*/
		if (vbus < 2500 && (first_charger_type == POWER_SUPPLY_TYPE_USB_CDP) && time_now.tv_sec <= 15) {
			pr_info("%s msleep start\n", __func__);
			chr_err("kpoc_power_off_check vbus=%d\n", vbus);
			msleep(3500);
		}
	}
	/* 8 = KERNEL_POWER_OFF_CHARGING_BOOT */
	/* 9 = LOW_POWER_OFF_CHARGING_BOOT */
	if (boot_mode == 8 || boot_mode == 9) {
		vbus = get_vbus(info);
		if (vbus >= 0 && vbus < 2500 && !mtk_is_charger_on(info)
			&& !info->pd_reset && (time_now.tv_sec > 5)) {
			chr_err("Unplug Charger/USB in KPOC mode, vbus=%d, shutdown\n", vbus);
			if(vcount > 4) {
				if (info->is_suspend == false) {
					chr_err("%s, not in suspend, shutdown\n", __func__);
					chr_err("%s: system_state=%d\n", __func__, system_state);
					if (system_state != SYSTEM_POWER_OFF)
					{
						msleep(5000);
						kernel_power_off();
					}
				} else {
					chr_err("%s, suspend! cannot shutdown\n", __func__);
					msleep(20);
				}
			} else {
				vcount++;
			}
		} else {
			vcount = 0;
		}
		charger_send_kpoc_uevent(info);
	}
}

static void charger_status_check(struct mtk_charger *info)
{
	union power_supply_propval online = {0}, status = {0};
	struct power_supply *chg_psy = NULL;
	int ret;
	bool charging = true;

	chg_psy = power_supply_get_by_name("primary_chg");
	if (IS_ERR_OR_NULL(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
	} else {
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_ONLINE, &online);

		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_STATUS, &status);

		if (!online.intval)
			charging = false;
		else {
			if (status.intval == POWER_SUPPLY_STATUS_NOT_CHARGING)
				charging = false;
		}
	}
	if (charging != info->is_charging)
		power_supply_changed(info->psy1);
	info->is_charging = charging;
}


static char *dump_charger_type(int chg_type, int usb_type)
{
	switch (chg_type) {
	case POWER_SUPPLY_TYPE_UNKNOWN:
		return "none";
	case POWER_SUPPLY_TYPE_USB:
		if (usb_type == POWER_SUPPLY_USB_TYPE_SDP)
			return "usb";
		else if (usb_type == POWER_SUPPLY_USB_TYPE_DCP &&
				pinfo != NULL && !pinfo->pd_type) {
			pinfo->real_type = XMUSB350_TYPE_FLOAT;
			return "nonstd";
		} else
			return "unknown";
	case POWER_SUPPLY_TYPE_USB_CDP:
		return "usb-h";
	case POWER_SUPPLY_TYPE_USB_DCP:
		return "std";
	//case POWER_SUPPLY_TYPE_USB_FLOAT:
	//	return "nonstd";
	default:
		return "unknown";
	}
}

static void check_fg_status(struct mtk_charger *info)
{
	struct fuel_gauge_dev *gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
	struct timespec64 time;
	int fg_status = fuel_gauge_check_fg_status(gauge);
	int ret = 0;
	int effective_fcc = 8000, fcc_value = 0;
	int vbus = 0;
	bool fg_i2c_err = (bool)(fg_status & FG_EER_I2C_FAIL);
	ktime_t tmp_time = 0;
	tmp_time = ktime_get_boottime();
	time = ktime_to_timespec64(tmp_time);

	if(time.tv_sec < 50) {
		chr_err("%s boot do not start\n", __func__);
		if (fg_status & FG_ERR_AUTH_FAIL){
			vote(info->fcc_votable, FG_ERR_VOTER, true, 2000);
			vote(info->icl_votable, FG_ERR_VOTER, true, 2000);
			goto out;
		} else {
			vote(info->fcc_votable, FG_ERR_VOTER, false, 0);
			vote(info->icl_votable, FG_ERR_VOTER, false, 0);
		}
	}

	mtk_charger_fw_notifier_call_chain(CHG_FW_EVT_FG_I2C_ERR, &fg_i2c_err);

	if ((fg_status & FG_ERR_AUTH_FAIL && fg_status & FG_BATT_AUTH_DONE) || fg_status & FG_EER_I2C_FAIL || fg_status & FG_ERR_CHG_WATT || !info->cp_master_ok) {
		if (info->cp_master_ok) {
			vote(info->fv_votable, FG_ERR_VOTER, true, 4100);
		}

		if (fg_status & FG_EER_I2C_FAIL || !info->cp_master_ok) {
			vote(info->fcc_votable, FG_ERR_VOTER, true, 500);
			vote(info->icl_votable, FG_ERR_VOTER, true, 500);
		} else if (fg_status & FG_ERR_AUTH_FAIL || fg_status & FG_ERR_CHG_WATT){
			vote(info->fcc_votable, FG_ERR_VOTER, true, 2000);
			vote(info->icl_votable, FG_ERR_VOTER, true, 2000);
		}

		if (info->real_type == XMUSB350_TYPE_HVCHG) {
			charger_dev_set_dpdm_voltage(info->chg1_dev, 0, 0);
			charger_dev_set_mivr(info->chg1_dev, info->data.min_charger_voltage);
			info->hvdcp_setp_down = true;
			chr_err("fg err = %d hvdcp vbus fall 5v\n", fg_status);
		} else if (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
			ret = adapter_dev_set_cap_xm(info->pd_adapter, MTK_PD_APDO, 5000, 3000);
			if (ret == MTK_ADAPTER_ERROR || ret == MTK_ADAPTER_ADJUST) {
				adapter_dev_set_cap_xm(info->pd_adapter, MTK_PD_APDO, 5000, 2000);
				chr_err("request 5v/3v fail, retry vbus = %d, ibus = %d\n", 5000, 2000);
			}
		}
		chr_err("fg err = %d to limit charge current and fv", fg_status);
	} else {
		vote(info->fcc_votable, FG_ERR_VOTER, false, 0);
		vote(info->fv_votable, FG_ERR_VOTER, false, 0);
		vote(info->icl_votable, FG_ERR_VOTER, false, 0);

		vbus = get_vbus(info);

		if (info->real_type == XMUSB350_TYPE_HVCHG && vbus < 7200 && vbus > 4200 && !info->lpd_charging_limit && info->hvdcp_setp_down) {
			charger_dev_set_dpdm_voltage(info->chg1_dev, 3300, 600);
			info->hvdcp_setp_down = false;
			chr_err("fg err = %d hvdcp vbus %dmv raise 9v\n", fg_status, vbus);
		}
	}

out:
	if ((fg_status & FG_ERR_ISC_ALARM) == FG_ERR_ISC_ALARM) {
		info->isc_diff_fv = 15;
		fcc_value = get_client_vote(info->fcc_votable, STEP_CHARGE_VOTER);
		if (fcc_value >= 0)
			effective_fcc = effective_fcc > fcc_value ? fcc_value : effective_fcc;
		effective_fcc = effective_fcc * 8 / 10;
		vote(info->fcc_votable, ISC_ALERT_VOTER, true, effective_fcc);

		chr_err("fg isc alarm, diff fv = %d, vote fcc = %d", info->isc_diff_fv, effective_fcc);
	} else {
		info->isc_diff_fv = 0;
		vote(info->fcc_votable, ISC_ALERT_VOTER, false, 0);
	}

}

static void en_floating_ground_work_func(struct work_struct *work)
{
	if (pinfo != NULL && pinfo->tcpc != NULL)
		tcpci_enable_floating_ground(pinfo->tcpc, true);
	chr_err("%s: enter\n", __func__);
}

static void dis_floating_ground_work_func(struct work_struct *work)
{
	if (pinfo != NULL && pinfo->tcpc != NULL)
		tcpci_enable_floating_ground(pinfo->tcpc, false);
	chr_err("%s: enter\n", __func__);
}

static void otg_state_check_work(struct work_struct *work)
{
	int battery_temp = 0;
	int uisoc = 0;
	bool need_limit = false;
	struct pd_port *pd_port = NULL;

	if (pinfo == NULL || pinfo->tcpc == NULL) {
		chr_err("%s: pinfo or tcpc is NULL\n", __func__);
		goto retry;
	}

	pd_port = &pinfo->tcpc->pd_port;

	uisoc = get_uisoc(pinfo);
	battery_temp = get_battery_temperature(pinfo);

	if (battery_temp > 0 && uisoc <= 5)
		need_limit = true;
	else if (battery_temp <= 0 && battery_temp > -10 && uisoc <= 15)
		need_limit = true;
	else if (battery_temp <= -10 && uisoc <= 50)
		need_limit = true;
	else
		need_limit = false;

	//source cap 5v300mA
	if (need_limit && !pinfo->cc_curr_limit) {
		pd_port->local_src_cap_default.pdos[0] =  0x2601901e;
		pd_port->local_src_cap_default.nr = 1;
		tcpm_dpm_pd_soft_reset(pinfo->tcpc, NULL);
		pinfo->cc_curr_limit = true;
	}
	//source cap 5v1500mA
	if (!need_limit && pinfo->cc_curr_limit){
		pd_port->local_src_cap_default.pdos[0] =  0x26019096;
		pd_port->local_src_cap_default.nr = 1;
		tcpm_dpm_pd_soft_reset(pinfo->tcpc, NULL);
		pinfo->cc_curr_limit = false;
	}

retry:
	schedule_delayed_work(&pinfo->otg_state_check_work, 2000);

	return;
}

static int charger_thermal_notifier_call(struct notifier_block *notifier,
	unsigned long event, void *val)
{
	struct mtk_charger *info;
	info = container_of(notifier, struct mtk_charger, thermal_nb);

	switch (event) {
	case THERMAL_BOARD_TEMP:
		info->board_temp = *(int *)val;
		chr_err("%s: get board_temp: %d\n", __func__, info->board_temp);
		break;
	default:
		chr_err("%s: not supported charger notifier event: %lu\n", __func__, event);
		break;
	}
	return NOTIFY_DONE;
}

static int mtk_charger_tcpc_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	uint8_t old_state = TYPEC_UNATTACHED, new_state = TYPEC_UNATTACHED;
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;
	u32 boot_mode = 0;
	bool report_psy = true;

	chr_err("%s: event=%lu, state=%d,%d\n", __func__,
		event, noti->typec_state.old_state, noti->typec_state.new_state);
	switch (event) {
	case TCP_NOTIFY_VBUS_SHORT_CC:
		if (noti->vsc_status) {
			chr_err("%s enter short status, CC%s%s\n", __func__,
				noti->vsc_status & BIT(TCPC_POLARITY_CC1) ? "1" : "",
				noti->vsc_status & BIT(TCPC_POLARITY_CC2) ? "2" : "");
				xm_charge_uevent_report(CHG_UEVENT_CC_SHORT_VBUS, true);
		} else {
			chr_err("%s exit short status\n", __func__);
		}
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		old_state = noti->typec_state.old_state;
		new_state = noti->typec_state.new_state;

		if (old_state == TYPEC_UNATTACHED &&
				new_state != TYPEC_UNATTACHED &&
				!pinfo->typec_attach) {
			chr_err("%s typec plug in, polarity = %d\n",
					__func__, noti->typec_state.polarity);
			pinfo->typec_attach = true;
			pinfo->cid_status = true;
			if (pinfo->ui_cc_toggle) {
				ret = alarm_try_to_cancel(&pinfo->otg_ui_close_timer);
				if (ret < 0) {
					chr_err("%s: callback was running, skip timer\n", __func__);
				}
				chr_err("typec plug in, cancel otg_ui_close_timer\n");
			}
			#if IS_ENABLED(CONFIG_RUST_DETECTION)
			schedule_delayed_work(&pinfo->rust_detection_work, msecs_to_jiffies(0));
			#endif
		} else if (old_state != TYPEC_UNATTACHED &&
				new_state == TYPEC_UNATTACHED &&
				pinfo->typec_attach) {
			chr_err("%s typec plug out\n", __func__);
			pinfo->typec_attach = false;
			pinfo->cid_status = false;
			pinfo->pd30_source = false;
			if (pinfo->ui_cc_toggle) {
				ret = alarm_try_to_cancel(&pinfo->otg_ui_close_timer);
				if (ret < 0) {
					chr_err("%s: callback was running, skip timer\n", __func__);
				}
				ktime_now = ktime_get_boottime();
				time_now = ktime_to_timespec64(ktime_now);
				end_time.tv_sec = time_now.tv_sec + 600;
				end_time.tv_nsec = time_now.tv_nsec + 0;
				ktime = ktime_set(end_time.tv_sec,end_time.tv_nsec);

				chr_err("%s: alarm timer start:%d, %lld %ld\n", __func__, ret,
						end_time.tv_sec, end_time.tv_nsec);
				alarm_start(&pinfo->otg_ui_close_timer, ktime);
				chr_err("typec plug out, start otg_ui_close_timer\n");
			}

			//拔出时清状态
			if (pinfo->last_pdo_caps != 0) {
				chr_err("[REVCHG] Plug out ,stop reverse_quick_charging\n");
				tcpm_typec_change_role_postpone(pinfo->tcpc, TYPEC_ROLE_SRC, true);
				schedule_delayed_work(&pinfo->handle_cc_status_work, msecs_to_jiffies(2000));
				//reverse_power_mode = REVCHG_NORMAL;
				update_pdo_caps(pinfo, REVCHG_NORMAL);
				pinfo->last_pdo_caps = 0;
				pinfo->ibat_check_cnt = 0;
				charger_dev_cp_enable_adc(pinfo->cp_master, false);
			} else {
				schedule_delayed_work(&pinfo->handle_cc_status_work, 0);
			}
			
			xm_charge_uevent_report(CHG_UEVENT_REVERSE_QUICK_CHARGE, 0);
			cancel_delayed_work_sync(&pinfo->check_revchg_status_work);
			cancel_delayed_work_sync(&pinfo->handle_reverse_charge_event_work);
			__pm_relax(pinfo->reverse_charge_wakelock);
			cancel_delayed_work_sync(&pinfo->delay_disable_otg_work);
			#if IS_ENABLED(CONFIG_RUST_DETECTION)
			cancel_delayed_work_sync(&pinfo->rust_detection_work);
			vote(pinfo->fcc_votable, LPD_DECTEED_VOTER, false, 0);
			vote(pinfo->icl_votable, LPD_DECTEED_VOTER, false, 0);
			#endif
			cancel_delayed_work_sync(&pinfo->otg_state_check_work);
			pinfo->cc_curr_limit = false;
			pinfo->tcpc->adapt_pid = 0;
			pinfo->tcpc->adapt_vid = 0;
		}
		break;
	/*tcp call_chain event to mtk_pd_adapter,
	 *mtk_pd_adapter call chain event to mtk_charger,
	 *if pinfo->pd_adapter is NULL, tcpc call_chain event to charger, start.
	 */
	case TCP_NOTIFY_PD_STATE:
		if (pinfo->pd_adapter) {
			chr_debug("%s already get pd_adapter\n", __func__);
			switch (noti->pd_state.connected) {
			case PD_CONNECT_NONE:
				pinfo->reverse_adapter_svid = 0;
				break;
			}
			break;
		} else {
			chr_err("%s get not pd_adapter\n", __func__);
		}

		switch (noti->pd_state.connected) {
		case PD_CONNECT_NONE:
			mutex_lock(&pinfo->pd_lock);
			chr_err("PD Notify Detach\n");
			pinfo->pd_type = MTK_PD_CONNECT_NONE;
			pinfo->pd_reset = false;
			pinfo->real_type = XMUSB350_TYPE_UNKNOW;
			mutex_unlock(&pinfo->pd_lock);
			mtk_chg_alg_notify_call(pinfo, EVT_DETACH, 0);
			/* reset PE40 */
			break;

		case PD_CONNECT_HARD_RESET:
			mutex_lock(&pinfo->pd_lock);
			chr_err("PD Notify HardReset\n");
			pinfo->pd_type = MTK_PD_CONNECT_NONE;
			pinfo->pd_reset = true;
			mutex_unlock(&pinfo->pd_lock);
			mtk_chg_alg_notify_call(pinfo, EVT_HARDRESET, 0);
			_wake_up_charger(pinfo);
			/* reset PE40 */
			break;

		case PD_CONNECT_SOFT_RESET:
			mutex_lock(&pinfo->pd_lock);
			chr_err("PD Notify SoftReset\n");
			pinfo->pd_type = MTK_PD_CONNECT_SOFT_RESET;
			pinfo->pd_reset = false;
			mutex_unlock(&pinfo->pd_lock);
			mtk_chg_alg_notify_call(pinfo, EVT_SOFTRESET, 0);
			_wake_up_charger(pinfo);
			/* reset PE50 */
			break;

		case PD_CONNECT_PE_READY_SNK:
			mutex_lock(&pinfo->pd_lock);
			chr_err("PD Notify fixed voltage ready\n");
			pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK;
			pinfo->pd_reset = false;
			pinfo->real_type = XMUSB350_TYPE_PD;
			mutex_unlock(&pinfo->pd_lock);
			/* PD is ready */
			break;

		case PD_CONNECT_PE_READY_SNK_PD30:
			mutex_lock(&pinfo->pd_lock);
			chr_err("PD Notify PD30 ready\r\n");
			pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK_PD30;
			pinfo->pd_reset = false;
			pinfo->real_type = XMUSB350_TYPE_PD;
			mutex_unlock(&pinfo->pd_lock);
			/* PD30 is ready */
			break;

		case PD_CONNECT_PE_READY_SNK_APDO:
			mutex_lock(&pinfo->pd_lock);
			chr_err("PD Notify APDO Ready\n");
			pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK_APDO;
			pinfo->pd_reset = false;
			pinfo->real_type = XMUSB350_TYPE_PD_PPS;
			mutex_unlock(&pinfo->pd_lock);
			/* PE40 is ready */
			_wake_up_charger(pinfo);
			break;

		case PD_CONNECT_TYPEC_ONLY_SNK:
			mutex_lock(&pinfo->pd_lock);
			chr_err("PD Notify Type-C Ready\n");
			pinfo->pd_type = MTK_PD_CONNECT_TYPEC_ONLY_SNK;
			pinfo->pd_reset = false;
			mutex_unlock(&pinfo->pd_lock);
			/* type C is ready */
			_wake_up_charger(pinfo);
			break;
			}
		break;
	case TCP_NOTIFY_WD_STATUS:
		if (pinfo->pd_adapter) {
			chr_err("%s already get pd_adapter\n", __func__);
			break;
		} else {
			chr_err("%s get not pd_adapter\n", __func__);
		}

		boot_mode = pinfo->bootmode;
		chr_err("wd status = %d\n", noti->wd_status.water_detected);
		pinfo->water_detected = noti->wd_status.water_detected;
		if (pinfo->water_detected == true) {
			pinfo->notify_code |= CHG_TYPEC_WD_STATUS;
			pinfo->record_water_detected = true;
			if (boot_mode == 8 || boot_mode == 9)
				pinfo->enable_hv_charging = false;
		} else {
			pinfo->notify_code &= ~CHG_TYPEC_WD_STATUS;
			if (boot_mode == 8 || boot_mode == 9)
				pinfo->enable_hv_charging = true;
		}
		mtk_chgstat_notify(pinfo);
		report_psy = boot_mode == 8 || boot_mode == 9;
		if (report_psy)
			power_supply_changed(pinfo->psy1);
		break;
	case TCP_NOTIFY_CC_HI:
		if (pinfo->pd_adapter) {
			chr_err("%s already get pd_adapter\n", __func__);
			break;
		} else {
			chr_err("%s get not pd_adapter\n", __func__);
		}

		chr_err("cc_hi = %d\n", noti->cc_hi);
		pinfo->cc_hi = noti->cc_hi;
		_wake_up_charger(pinfo);
		break;
	case TCP_NOTIFY_CVDM:
		if (pinfo->pd_adapter) {
			chr_err("%s already get pd_adapter\n", __func__);
			break;
		} else {
			chr_err("%s get not pd_adapter\n", __func__);
		}

		mutex_lock(&pinfo->pd_lock);
		usbpd_mi_vdm_received_cb(pinfo, noti->cvdm_msg);
		mutex_unlock(&pinfo->pd_lock);
		break;
	/*tcp call_chain event to mtk_pd_adapter,
	 *mtk_pd_adapter call chain event to mtk_charger,
	 *if pinfo->pd_adapter is NULL, tcpc call_chain event to charger, end.
	 */

	case TCP_NOTIFY_SOURCE_VBUS:
		chr_err("%s vbus_state.type= %d\n", __func__, noti->vbus_state.type);
		if(noti->vbus_state.type & TCP_VBUS_CTRL_PD_DETECT)
			pinfo->pd30_source = true;
		if (noti->vbus_state.mv)
			schedule_delayed_work(&pinfo->otg_state_check_work, 0);
		chg_source_vbus(pinfo, noti->vbus_state.mv);
		break;
	case TCP_NOTIFY_SINK_VBUS:
		if (IS_ERR_OR_NULL(pinfo->chg1_dev) || IS_ERR_OR_NULL(pinfo->cp_master)) {
			chr_err("%s: chg1_dev or cp_master not found\n", __func__);
			break;
		}
		if (noti->vbus_state.type & TCP_VBUS_CTRL_PD_DETECT) {
			if (noti->vbus_state.ma != PD_STANDYBY_CURRENT) {
				vote(pinfo->icl_votable, SINK_VBUS_VOTER, true, noti->vbus_state.ma);
				if (pinfo->real_type == XMUSB350_TYPE_PD && noti->vbus_state.mv >= 5000) {
					if (noti->vbus_state.mv == 5000)
						vote(pinfo->fcc_votable, CHARGERIC_VOTER, true, noti->vbus_state.ma);
					else
						vote(pinfo->fcc_votable, CHARGERIC_VOTER, true, noti->vbus_state.ma * 2);
					chr_err("%s adapter_imax = %d\n", __func__, noti->vbus_state.ma);
				}
			}
			if (noti->vbus_state.mv >= 5000 && noti->vbus_state.ma < 100) {
				charger_dev_enable_powerpath(pinfo->chg1_dev, false);
				charger_dev_enable(pinfo->cp_master, false);
				chr_err("%s ibus low = %d, stop charger\n", __func__, noti->vbus_state.ma);
			} else if (noti->vbus_state.mv >= 5000 && noti->vbus_state.ma <= 500) {
				charger_dev_enable(pinfo->cp_master, false);
				chr_err("%s ibus low = %d, stop cp\n", __func__, noti->vbus_state.ma);
			}
		}
		break;

	default:
		chr_err("%s default event\n", __func__);
	}
	return NOTIFY_OK;
}

static int screen_state_for_charger_callback(struct notifier_block *nb, unsigned long val, void *v)
{
	struct mi_disp_notifier *evdata = v;
	struct mtk_charger *pinfo = container_of(nb,
							struct mtk_charger, disp_nb);
	unsigned int blank;
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;

	if (!(val == MI_DISP_DPMS_EARLY_EVENT ||
		val == MI_DISP_DPMS_EVENT)) {
		chr_err("event(%lu) do not need process\n", val);
		return NOTIFY_OK;
	}

	if (evdata && evdata->data) {
		blank = *(int *)(evdata->data);
		if ((val == MI_DISP_DPMS_EVENT) && (blank == MI_DISP_DPMS_POWERDOWN
				|| blank == MI_DISP_DPMS_LP1 || blank == MI_DISP_DPMS_LP2)) {
			pinfo->screen_status = SCREEN_STATE_BLACK;
			pinfo->screen_state = 1;

			ret = alarm_try_to_cancel(&pinfo->set_soft_cid_timer);
			if (ret < 0) {
				chr_err("%s: callback was running, skip timer\n", __func__);
			}
			ktime_now = ktime_get_boottime();
			time_now = ktime_to_timespec64(ktime_now);
			end_time.tv_sec = time_now.tv_sec + 5;
			end_time.tv_nsec = time_now.tv_nsec + 0;
			ktime = ktime_set(end_time.tv_sec,end_time.tv_nsec);

			chr_err("%s: set_soft_cid_timer alarm timer start:%d, %lld %ld\n", __func__, ret,
				end_time.tv_sec, end_time.tv_nsec);
			alarm_start(&pinfo->set_soft_cid_timer, ktime);
		} else if ((val == MI_DISP_DPMS_EVENT) && (blank == MI_DISP_DPMS_ON)) {
			pinfo->screen_status = SCREEN_STATE_BRIGHT;
			pinfo->screen_state = 0;

			ret = alarm_try_to_cancel(&pinfo->set_soft_cid_timer);
			if (ret < 0) {
				chr_err("%s: callback was running, skip timer\n", __func__);
			}
			chr_err("%s stop set_soft_cid_timer\n", __func__);
			schedule_delayed_work(&pinfo->en_floatgnd_work, 0);
			schedule_delayed_work(&pinfo->handle_cc_status_work, 0);
		}
		chr_err("%s screen_status = %d, val = %lu, balnk = %u\n", __func__,pinfo->screen_status, val, blank);
	} else {
		chr_err("%s can not get screen_state!\n", __func__);
	}

    return NOTIFY_OK;
}

static int audio_state_for_charger_callback(struct notifier_block *nb, unsigned long val, void *v)
{

	struct mtk_charger *pinfo = container_of(nb,
							struct mtk_charger, audio_nb);

	pinfo->audio_status = val;

	schedule_delayed_work(&pinfo->handle_cc_status_work, 0);

	chr_err("%s audio_status = %lu\n", __func__, val);

	return NOTIFY_OK;
}


static int charger_routine_thread(void *arg)
{
	struct mtk_charger *info = arg;
	unsigned long flags;
	unsigned int init_times = 3;
	static bool is_module_init_done;
	bool is_charger_on;
	int ret;
	int vbat_min = 0;
	int vbat_max = 0;
	u32 chg_cv = 0;

	while (1) {
		// adapter register
		if (!pinfo->pd_adapter) {
			pinfo->pd_adapter = get_adapter_by_name("pd_adapter");
			if (!pinfo->pd_adapter) {
				chr_err("%s: No pd adapter found flag=%d\n", __func__, info->flag);
				if (info->flag < 3) {
					info->flag++;
					if (info->flag > 5)
						info->flag = 5;
					msleep(100);
					continue;
				}
			} else {
				pinfo->pd_nb.notifier_call = notify_adapter_event;
				register_adapter_device_notifier(pinfo->pd_adapter,
						&pinfo->pd_nb);
				chr_err("%s: register adapter ok\n", __func__);
			}
		}

		if (!info->tcpc) {
			info->tcpc = tcpc_dev_get_by_name("type_c_port0");
			chr_err("get tcpc dev again\n");
			if(info->tcpc) {
				info->tcpc_nb.notifier_call = mtk_charger_tcpc_notifier_call;
				register_tcp_dev_notifier(info->tcpc,
						 &info->tcpc_nb, TCP_NOTIFY_TYPE_ALL);
				chr_err("register tcpc_nb ok\n");
			}
			else
				chr_err("get tcpc dev again failed\n");
		}

		ret = wait_event_interruptible(info->wait_que,
			(info->charger_thread_timeout == true));
		if (ret < 0) {
			chr_err("%s: wait event been interrupted(%d)\n", __func__, ret);
			continue;
		}

		while (is_module_init_done == false) {
			if (charger_init_algo(info) == true) {
				is_module_init_done = true;
				if (info->charger_unlimited) {
					info->enable_sw_safety_timer = false;
					charger_dev_enable_safety_timer(info->chg1_dev, false);
				}
			}
			else {
				if (init_times > 0) {
					chr_err("retry to init charger\n");
					init_times = init_times - 1;
					msleep(10000);
				} else {
					chr_err("holding to init charger\n");
					msleep(60000);
				}
			}
		}

		mutex_lock(&info->charger_lock);
		spin_lock_irqsave(&info->slock, flags);
		if (!info->charger_wakelock->active)
			__pm_stay_awake(info->charger_wakelock);
		spin_unlock_irqrestore(&info->slock, flags);
		info->charger_thread_timeout = false;

		info->battery_temp = get_battery_temperature(info);
		ret = charger_dev_get_adc(info->chg1_dev,
			ADC_CHANNEL_VBAT, &vbat_min, &vbat_max);
		if (ret < 0)
			chr_err("failed to get vbat_min\n");
		ret = charger_dev_get_constant_voltage(info->chg1_dev, &chg_cv);

		if (vbat_min != 0)
			vbat_min = vbat_min / 1000;

		chr_err("Vbat=%d vbats=%d vbus:%d ibus:%d I=%d T=%d uisoc:%d type:%s>%s pd:%d swchg_ibat:%d cv:%d cmd_pp:%d\n",
			get_battery_voltage(info),
			vbat_min,
			get_vbus(info),
			get_ibus(info),
			get_battery_current(info),
			info->battery_temp,
			get_uisoc(info),
			dump_charger_type(info->chr_type, info->usb_type),
			dump_charger_type(get_charger_type(info), get_usb_type(info)),
			info->pd_type, get_ibat(info), chg_cv, info->cmd_pp);

		if (get_charger_type(info) == POWER_SUPPLY_TYPE_USB && get_usb_type(info) == POWER_SUPPLY_USB_TYPE_DCP && (!info->pd_type))
			info->real_type = XMUSB350_TYPE_FLOAT;

		is_charger_on = mtk_is_charger_on(info);
		power_supply_changed(info->usb_psy);

		if (info->charger_thread_polling == true || info->is_mtbf_mode == true)
			mtk_charger_start_timer(info);

		check_battery_exist(info);
		check_dynamic_mivr(info);
		charger_check_status(info);
		kpoc_power_off_check(info);

		if (is_disable_charger(info) == false &&
			is_charger_on == true &&
			info->can_charging == true) {
			if (info->algo.do_algorithm)
				info->algo.do_algorithm(info);
			charger_status_check(info);
		} else {
			chr_debug("disable charging %d %d %d\n",
			    is_disable_charger(info), is_charger_on, info->can_charging);
		}
		if (info->bootmode != 1 && info->bootmode != 2 && info->bootmode != 4
			&& info->bootmode != 8 && info->bootmode != 9)
			smart_charging(info);
		spin_lock_irqsave(&info->slock, flags);
		__pm_relax(info->charger_wakelock);
		spin_unlock_irqrestore(&info->slock, flags);
		chr_debug("%s end , %d\n",
			__func__, info->charger_thread_timeout);
		mutex_unlock(&info->charger_lock);

		if (info->enable_boot_volt &&
			ktime_get_seconds() > RESET_BOOT_VOLT_TIME &&
			!info->reset_boot_volt_times) {
			ret = charger_dev_set_boot_volt_times(info->chg1_dev, 0);
			if (ret < 0)
				chr_err("reset boot_battery_voltage times fails %d\n", ret);
			else {
				info->reset_boot_volt_times = 1;
				chr_err("reset boot_battery_voltage times\n");
			}
		}
		check_fg_status(info);
	}

	return 0;
}


#ifdef CONFIG_PM
static int charger_pm_event(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	ktime_t ktime_now;
	struct timespec64 now;
	struct mtk_charger *info;

	info = container_of(notifier,
		struct mtk_charger, pm_notifier);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		info->is_suspend = true;
		chr_debug("%s: enter PM_SUSPEND_PREPARE\n", __func__);
		break;
	case PM_POST_SUSPEND:
		info->is_suspend = false;
		chr_debug("%s: enter PM_POST_SUSPEND\n", __func__);
		ktime_now = ktime_get_boottime();
		now = ktime_to_timespec64(ktime_now);

		if (timespec64_compare(&now, &info->endtime) >= 0 &&
			info->endtime.tv_sec != 0 &&
			info->endtime.tv_nsec != 0) {
			chr_err("%s: alarm timeout, wake up charger\n",
				__func__);
			__pm_relax(info->charger_wakelock);
			info->endtime.tv_sec = 0;
			info->endtime.tv_nsec = 0;
			_wake_up_charger(info);
		}
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}
#endif /* CONFIG_PM */

static enum alarmtimer_restart
	mtk_charger_alarm_timer_func(struct alarm *alarm, ktime_t now)
{
	struct mtk_charger *info =
	container_of(alarm, struct mtk_charger, charger_timer);

	if (info->is_suspend == false) {
		_wake_up_charger(info);
	} else {
		__pm_stay_awake(info->charger_wakelock);
	}

	return ALARMTIMER_NORESTART;
}

static void mtk_charger_init_timer(struct mtk_charger *info)
{
	alarm_init(&info->charger_timer, ALARM_BOOTTIME,
			mtk_charger_alarm_timer_func);
	mtk_charger_start_timer(info);

}

static int mtk_charger_setup_files(struct platform_device *pdev)
{
	int ret = 0;
	struct proc_dir_entry *battery_dir = NULL, *entry = NULL;
	struct mtk_charger *info = platform_get_drvdata(pdev);

	ret = device_create_file(&(pdev->dev), &dev_attr_sw_jeita);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_sw_ovp_threshold);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_chr_type);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_enable_meta_current_limit);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_fast_chg_indicator);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_Charging_mode);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_pd_type);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_High_voltage_chg_enable);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_Rust_detect);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_Thermal_throttle);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_alg_new_arbitration);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_alg_unchangeable);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_vbat_mon);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_Pump_Express);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_ADC_Charger_Voltage);
	if (ret)
		goto _out;
	ret = device_create_file(&(pdev->dev), &dev_attr_ADC_Charging_Current);
	if (ret)
		goto _out;
	ret = device_create_file(&(pdev->dev), &dev_attr_input_current);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_charger_log_level);
	if (ret)
		goto _out;

	/* Battery warning */
	ret = device_create_file(&(pdev->dev), &dev_attr_BatteryNotify);
	if (ret)
		goto _out;

	/* sysfs node */
	ret = device_create_file(&(pdev->dev), &dev_attr_enable_sc);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_sc_stime);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_sc_etime);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_sc_tuisoc);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_sc_ibat_limit);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_enable_power_path);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_product_name);
	if (ret)
		goto _out;

	battery_dir = proc_mkdir("mtk_battery_cmd", NULL);
	if (!battery_dir) {
		chr_err("%s: mkdir /proc/mtk_battery_cmd failed\n", __func__);
		return -ENOMEM;
	}

	entry = proc_create_data("current_cmd", 0644, battery_dir,
			&mtk_chg_current_cmd_fops, info);
	if (!entry) {
		ret = -ENODEV;
		goto fail_procfs;
	}
	entry = proc_create_data("en_power_path", 0644, battery_dir,
			&mtk_chg_en_power_path_fops, info);
	if (!entry) {
		ret = -ENODEV;
		goto fail_procfs;
	}
	entry = proc_create_data("en_safety_timer", 0644, battery_dir,
			&mtk_chg_en_safety_timer_fops, info);
	if (!entry) {
		ret = -ENODEV;
		goto fail_procfs;
	}
	entry = proc_create_data("set_cv", 0644, battery_dir,
			&mtk_chg_set_cv_fops, info);
	if (!entry) {
		ret = -ENODEV;
		goto fail_procfs;
	}

	return 0;

fail_procfs:
	remove_proc_subtree("mtk_battery_cmd", NULL);
_out:
	return ret;
}

void mtk_charger_get_atm_mode(struct mtk_charger *info)
{
	char atm_str[64] = {0};
	char *ptr = NULL, *ptr_e = NULL;
	char keyword[] = "androidboot.atm=";
	int size = 0;

	ptr = strstr(chg_get_cmd(), keyword);
	if (ptr != 0) {
		ptr_e = strstr(ptr, " ");
		if (ptr_e == 0)
			goto end;

		size = ptr_e - (ptr + strlen(keyword));
		if (size <= 0)
			goto end;
		strncpy(atm_str, ptr + strlen(keyword), size);
		atm_str[size] = '\0';
		chr_err("%s: atm_str: %s\n", __func__, atm_str);

		if (!strncmp(atm_str, "enable", strlen("enable")))
			info->atm_enabled = true;
	}
end:
	chr_err("%s: atm_enabled = %d\n", __func__, info->atm_enabled);
}

static int psy_charger_property_is_writeable(struct power_supply *psy,
					       enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		return 1;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		return 1;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		return 1;
	default:
		return 0;
	}
}

static const enum power_supply_usb_type charger_psy_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_PPS,
};

static const enum power_supply_property charger_psy_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_VOLTAGE_BOOT,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_POWER_NOW,
};

static enum power_supply_property mt_usb_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static int psy_charger_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mtk_charger *info;
	struct charger_device *chg;
	int ret = 0, idx = 0, chg_vbat = 0, vsys_min = 0, vsys_max = 0, vbat_max = 0;
	struct chg_alg_device *alg = NULL;

	info = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (info == NULL) {
		chr_err("%s: get info failed\n", __func__);
		return -EINVAL;
	}
	chr_debug("%s psp:%d\n", __func__, psp);

	if (info->psy1 == psy) {
		chg = info->chg1_dev;
		idx = CHG1_SETTING;
	} else if (info->psy2 == psy) {
		chg = info->chg2_dev;
		idx = CHG2_SETTING;
	} else if (info->psy_dvchg1 == psy) {
		chg = info->dvchg1_dev;
		idx = DVCHG1_SETTING;
	} else if (info->psy_dvchg2 == psy) {
		chg = info->dvchg2_dev;
		idx = DVCHG2_SETTING;
	} else if (info->psy_hvdvchg1 == psy) {
		chg = info->hvdvchg1_dev;
		idx = HVDVCHG1_SETTING;
	} else if (info->psy_hvdvchg2 == psy) {
		chg = info->hvdvchg2_dev;
		idx = HVDVCHG2_SETTING;
	} else {
		chr_err("%s fail\n", __func__);
		return 0;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (idx == DVCHG1_SETTING || idx == DVCHG2_SETTING ||
		    idx == HVDVCHG1_SETTING || idx == HVDVCHG2_SETTING) {
			val->intval = false;
			alg = get_chg_alg_by_name("pe5");
			if (alg == NULL)
				chr_err("get pe5 fail\n");
			else {
				ret = chg_alg_is_algo_ready(alg);
				if (ret == ALG_RUNNING)
					val->intval = true;
			}
			break;
		}

		val->intval = is_charger_exist(info);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (chg != NULL)
			val->intval = true;
		else
			val->intval = false;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = info->enable_hv_charging;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = get_vbus(info);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = info->chg_data[idx].junction_temp_max * 10;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
	val->intval = get_charger_charging_current(info, chg);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	val->intval = get_charger_input_current(info, chg);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_BOOT:
		val->intval = get_charger_zcv(info, chg);
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		switch (info->pd_type) {
		case MTK_PD_CONNECT_PE_READY_SNK_APDO:
			val->intval = POWER_SUPPLY_USB_TYPE_PD_PPS;
			break;
		case MTK_PD_CONNECT_PE_READY_SNK:
		case MTK_PD_CONNECT_PE_READY_SNK_PD30:
			val->intval = POWER_SUPPLY_USB_TYPE_PD;
			break;
		default:
			val->intval = info->usb_type;
			break;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = charger_dev_get_adc(info->chg1_dev,
			ADC_CHANNEL_VBAT, &chg_vbat, &vbat_max);
		if (ret < 0)
			val->intval = 0;
		else
			val->intval = chg_vbat;
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		ret = charger_dev_get_adc(info->chg1_dev,
			ADC_CHANNEL_VSYS, &vsys_min, &vsys_max);
		if (ret < 0)
			val->intval = 0;
		else
			val->intval = vsys_min;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/*
static int mtk_charger_enable_power_path(struct mtk_charger *info,
	int idx, bool en)
{
	int ret = 0;
	bool is_en = true;
	struct charger_device *chg_dev = NULL;

	if (!info)
		return -EINVAL;

	switch (idx) {
	case CHG1_SETTING:
		chg_dev = get_charger_by_name("primary_chg");
		break;
	case CHG2_SETTING:
		chg_dev = get_charger_by_name("secondary_chg");
		break;
	default:
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(chg_dev)) {
		chr_err("%s: chg_dev not found\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&info->pp_lock[idx]);
	info->enable_pp[idx] = en;

	if (info->force_disable_pp[idx])
		goto out;

	ret = charger_dev_is_powerpath_enabled(chg_dev, &is_en);
	if (ret < 0) {
		chr_err("%s: get is power path enabled failed\n", __func__);
		goto out;
	}
	if (is_en == en) {
		chr_err("%s: power path is already en = %d\n", __func__, is_en);
		goto out;
	}

	if (info->input_suspend)
		en = !info->input_suspend;

	pr_info("%s: enable power path = %d\n", __func__, en);
	ret = charger_dev_enable_powerpath(chg_dev, en);
out:
	mutex_unlock(&info->pp_lock[idx]);
	return ret;
}
*/

static int mtk_charger_force_disable_power_path(struct mtk_charger *info,
	int idx, bool disable)
{
	int ret = 0;
	struct charger_device *chg_dev = NULL;

	if (!info)
		return -EINVAL;

	switch (idx) {
	case CHG1_SETTING:
		chg_dev = get_charger_by_name("primary_chg");
		break;
	case CHG2_SETTING:
		chg_dev = get_charger_by_name("secondary_chg");
		break;
	default:
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(chg_dev)) {
		chr_err("%s: chg_dev not found\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&info->pp_lock[idx]);

	chr_err("[MTK-CHARGE]%s: disable: %d, force_disable_pp:%d, input_suspend:%d, enable_pp:%d\n",
		__func__, disable, info->force_disable_pp[idx], info->input_suspend, info->enable_pp[idx]);

	if (disable == info->force_disable_pp[idx])
		goto out;

	if (info->input_suspend)
		disable = info->input_suspend;

	info->force_disable_pp[idx] = disable;
	ret = charger_dev_enable_powerpath(chg_dev,
		info->force_disable_pp[idx] ? false : info->enable_pp[idx]);
out:
	mutex_unlock(&info->pp_lock[idx]);
	return ret;
}

static int psy_charger_set_property(struct power_supply *psy,
			enum power_supply_property psp,
			const union power_supply_propval *val)
{
	struct mtk_charger *info;
	int idx;

	chr_err("%s: prop:%d %d\n", __func__, psp, val->intval);

	info = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (info == NULL) {
		chr_err("%s: failed to get info\n", __func__);
		return -EINVAL;
	}

	if (info->psy1 == psy)
		idx = CHG1_SETTING;
	else if (info->psy2 == psy)
		idx = CHG2_SETTING;
	else if (info->psy_dvchg1 == psy)
		idx = DVCHG1_SETTING;
	else if (info->psy_dvchg2 == psy)
		idx = DVCHG2_SETTING;
	else if (info->psy_hvdvchg1 == psy)
		idx = HVDVCHG1_SETTING;
	else if (info->psy_hvdvchg2 == psy)
		idx = HVDVCHG2_SETTING;
	else {
		chr_err("%s fail\n", __func__);
		return 0;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (val->intval > 0)
			info->enable_hv_charging = true;
		else
			info->enable_hv_charging = false;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		info->chg_data[idx].thermal_charging_current_limit =
			val->intval;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		info->chg_data[idx].thermal_input_current_limit =
			val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		if ((info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) || (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30) || (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK))
			break;
		if (val->intval > 0)
			charger_dev_enable_powerpath(info->chg1_dev, false);
		else
			charger_dev_enable_powerpath(info->chg1_dev, true);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		if (val->intval > 0)
			mtk_charger_force_disable_power_path(info, idx, true);
		else
			mtk_charger_force_disable_power_path(info, idx, false);
		break;
	default:
		return -EINVAL;
	}
	_wake_up_charger(info);

	return 0;
}

static void mtk_charger_external_power_changed(struct power_supply *psy)
{
	struct mtk_charger *info;
	union power_supply_propval prop = {0};
	union power_supply_propval prop2 = {0};
	union power_supply_propval vbat0 = {0};
	struct power_supply *chg_psy = NULL;
	int ret;

	info = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (info == NULL) {
		pr_notice("%s: failed to get info\n", __func__);
		return;
	}
	chg_psy = info->chg_psy;

	if (IS_ERR_OR_NULL(chg_psy)) {
		pr_notice("%s Couldn't get chg_psy\n", __func__);
		chg_psy = power_supply_get_by_name("primary_chg");
		info->chg_psy = chg_psy;
	} else {
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_ONLINE, &prop);
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_USB_TYPE, &prop2);
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_ENERGY_EMPTY, &vbat0);
	}

	if (info->vbat0_flag != vbat0.intval) {
		if (vbat0.intval) {
			info->enable_vbat_mon = false;
			charger_dev_enable_6pin_battery_charging(info->chg1_dev, false);
		} else
			info->enable_vbat_mon = info->enable_vbat_mon_bak;

		info->vbat0_flag = vbat0.intval;
	}

	if (!IS_ERR_OR_NULL(info->bat_psy))
		power_supply_changed(info->bat_psy);

	pr_notice("%s event, name:%s online:%d type:%d vbus:%d\n", __func__,
		psy->desc->name, prop.intval, prop2.intval,
		get_vbus(info));

	_wake_up_charger(info);
}

static void mtk_charger_external_power_usb_changed(struct power_supply *psy)
{
	struct mtk_charger *info;
	union power_supply_propval prop;
	struct power_supply *chg_psy = NULL;
	int ret;
	info = (struct mtk_charger *)power_supply_get_drvdata(psy);
	chg_psy = info->chg_psy;

	if (!info->bat_psy)
		info->bat_psy = power_supply_get_by_name("battery");

	if (IS_ERR_OR_NULL(chg_psy)) {
		pr_notice("%s Couldn't get chg_psy\n", __func__);
		chg_psy = devm_power_supply_get_by_phandle(&info->pdev->dev,
						       "charger");
		info->chg_psy = chg_psy;
	} else {
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_ONLINE, &prop);
	}
	pr_err("%s event, name:%s online:%d type:%d\n", __func__,
		psy->desc->name, prop.intval, info->real_type);

	if (!IS_ERR_OR_NULL(info->bat_psy))
		power_supply_changed(info->bat_psy);
}

static const char * const power_supply_type_text[] = {
	"Unknown", "Battery", "UPS", "Mains", "USB", "USB_DCP", "USB_CDP", "USB_ACA",
	"USB_C", "USB_PD", "USB_PD_DRP", "BrickID", "Wireless"
};

static const char *get_type_name(int type)
{
	u32 i;
	for (i = 0; i < ARRAY_SIZE(power_supply_type_text); i++) {
		if (i == type)
			return power_supply_type_text[i];
	}
	return "Unknown";
}

static int mt_usb_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mtk_charger *info;
	info = (struct mtk_charger *)power_supply_get_drvdata(psy);
	u32 cp_ibus = 0;

	info->usb_desc.type = get_charger_type(info);
	if((info->usb_desc.type == POWER_SUPPLY_TYPE_USB) && ((info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) ||
		(info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30) || (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK)))
	{
		info->usb_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
		chr_err("%s chg_det abnormal, set usb_type as cdp\n",__func__);
	}
	val->strval = get_type_name(info->usb_desc.type);
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO)
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = is_charger_exist(info);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (info != NULL)
			val->intval = true;
		else
			val->intval = false;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = info->enable_hv_charging;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = get_vbus(info);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = get_ibus(info);
		if (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
			charger_dev_get_ibus(info->cp_master, &cp_ibus);
			val->intval = val->intval +  cp_ibus;
		}
		charge_current_notifier_call_chain(val->intval, NULL);
		break;
	default:
		return -EINVAL;
	}

	chr_debug("%s psp:%d val:%d\n", __func__, psp, val->intval);

	return 0;
}

struct quick_charge_desc {
	enum xmusb350_chg_type psy_type;
	enum quick_charge_type type;
};

struct quick_charge_desc quick_charge_table[15] = {
	{ XMUSB350_TYPE_SDP,		QUICK_CHARGE_NORMAL },
	{ XMUSB350_TYPE_CDP,		QUICK_CHARGE_NORMAL },
	{ XMUSB350_TYPE_DCP,		QUICK_CHARGE_NORMAL },
	{ XMUSB350_TYPE_FLOAT,		QUICK_CHARGE_NORMAL },
	{ XMUSB350_TYPE_HVDCP,	QUICK_CHARGE_FAST },
	{ XMUSB350_TYPE_HVDCP_2,	QUICK_CHARGE_FAST },
	{ XMUSB350_TYPE_HVDCP_3,	QUICK_CHARGE_FAST },
	{ XMUSB350_TYPE_PD,		QUICK_CHARGE_FAST },
	{ XMUSB350_TYPE_PD_PPS,		QUICK_CHARGE_FAST },
	{ XMUSB350_TYPE_HVDCP_35_18,	QUICK_CHARGE_FAST },
	{ XMUSB350_TYPE_HVDCP_35_27,	QUICK_CHARGE_FAST },
	{ XMUSB350_TYPE_HVDCP_3_18,	QUICK_CHARGE_FAST },
	{ XMUSB350_TYPE_HVCHG,	QUICK_CHARGE_FAST },
	{ XMUSB350_TYPE_HVDCP_3_27,	QUICK_CHARGE_FLASH },
	{0, 0},
};

static int get_quick_charge_type(struct mtk_charger *info)
{
	int j = 0;
	if (!info || !info->usb_psy || info->typec_burn)
		return QUICK_CHARGE_NORMAL;
	if (info->temp_now > 480 || info->temp_now < 0)
		return QUICK_CHARGE_NORMAL;
	if ((info->real_type == XMUSB350_TYPE_PD && info->pd_verifed) ||
			(info->pd_adapter != NULL &&
			 (info->pd_adapter->adapter_svid == USB_PD_MI_SVID || info->pd_adapter->adapter_svid == 0x2B01) &&
			 info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO)) {
			return QUICK_CHARGE_TURBE;
	}
	while (quick_charge_table[j].psy_type != 0) {
		if (info->real_type == quick_charge_table[j].psy_type) {
			return quick_charge_table[j].type;
		}
		j++;
	}
	return QUICK_CHARGE_NORMAL;
}

static int real_type_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = info->real_type;
	else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int pmic_ibat_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = get_ibat(info);
	else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int real_type_set(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (info)
		info->real_type = val;
	chr_err("%s %d\n", __func__, val);
	return 0;
}

static int quick_charge_type_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	*val = get_quick_charge_type(info);
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int pd_authentication_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = info->pd_verifed;
	else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int pd_authentication_set(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (info) {
		info->pd_verifed = !!val;
		power_supply_changed(info->usb_psy);
		chr_err("%s %d\n", __func__, info->pd_verifed);
	}
	return 0;
}

static int pd_verifying_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = info->pd_verifying;
	else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int pd_verifying_set(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (info)
		info->pd_verifying = val;
	chr_err("%s %d\n", __func__, val);
	return 0;
}

static int pd_type_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = info->pd_type;
	else
		*val = MTK_PD_CONNECT_NONE;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int adapter_imax_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = info->adapter_imax;
	else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int adapter_imax_set(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (info)
		info->adapter_imax = val;
	chr_err("%s %d\n", __func__, val);
	return 0;
}

static int apdo_max_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = info->apdo_max;
	else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int apdo_max_set(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	int intval[3] = {0};
	if (info)
		info->apdo_max = val;
	chr_err("%s %d\n", __func__, val);

	usb_get_property(info, USB_PROP_QUICK_CHARGE_TYPE, &intval[0]);
	usb_get_property(info, USB_PROP_SOC_DECIMAL, &intval[1]);
	usb_get_property(info, USB_PROP_SOC_DECIMAL_RATE, &intval[2]);
	//xm_charge_uevent_report(CHG_UEVENT_QUICK_CHARGE_TYPE, intval);
	xm_charge_uevents_bundle_report(CHG_UEVENT_BUNDLE_CHG_ANIMATION,
				intval[0], intval[1], intval[2]);

	return 0;
}

static int typec_mode_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = info->typec_mode;
	else
		*val = 0;
	return 0;
}

static int typec_mode_set(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (info)
		info->typec_mode = val;
	return 0;
}

static int typec_cc_orientation_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = info->cc_orientation + 1;
	else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int typec_cc_orientation_set(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (info)
		info->cc_orientation = val;
	chr_err("%s %d\n", __func__, val);
	return 0;
}

static int ffc_enable_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = info->ffc_enable;
	else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int charge_full_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = info->real_full;
	else
		*val = 0;
	return 0;
}

static int typec_ntc1_temp_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info) {
		if (!info->fake_typec_temp)
			charger_dev_get_typec_ntc1_temp(info->chg1_dev, val);
		else
			*val = info->fake_typec_temp;
	} else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}
static int typec_ntc1_temp_set(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (info)
		info->fake_typec_temp = val;
	chr_err("%s %d\n", __func__, val);
	return 0;
}
static int typec_ntc2_temp_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info) {
		if (!info->fake_typec_temp)
			charger_dev_get_typec_ntc2_temp(info->chg1_dev, val);
		else
			*val = info->fake_typec_temp;
	} else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}
static int typec_ntc2_temp_set(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (info)
		info->fake_typec_temp = val;
	chr_err("%s %d\n", __func__, val);
	return 0;
}

static int typec_burn_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = info->typec_burn;
	else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int sw_cv_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = info->sw_cv;
	else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int input_suspend_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = info->input_suspend;
	else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int input_suspend_set(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	bool input_suspend = 0;
	input_suspend = !!val;
	if (info) {
		info->input_suspend = input_suspend;
		charger_dev_enable_powerpath(info->chg1_dev, !input_suspend);
		power_supply_changed(info->psy1);
		if (!input_suspend) {
			info->suspend_recovery = true;
			power_supply_changed(info->usb_psy);
		}
	}
	chr_err("%s %d input_suspend =%d\n", __func__, val, info->input_suspend);
	return 0;
}

static int jeita_chg_index_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = info->jeita_chg_index[0];
	else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int power_max_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = info->apdo_max;
	else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int otg_enable_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = info->otg_enable;
	else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int otg_enable_set(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (info)
		info->otg_enable = !!val;

	if (info->otg_enable) {
		charger_dev_enable_cp_usb_gate(info->cp_master, true);
	} else {
		charger_dev_enable_cp_usb_gate(info->cp_master, false);
	}
	chr_err("%s %d\n", __func__, info->otg_enable);
	return 0;
}

/* P16 code for HQFEAT-93945 by songweijie at 2025/03/11 start */
static int usb_otg_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = info->usb_otg;
	else
		*val = 0;
	chr_info("%s %d\n", __func__, *val);
	return 0;
}

static int usb_otg_set(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (info) {
		info->usb_otg = !!val;
		chr_info("%s %d\n", __func__, info->usb_otg);
	} else
		chr_info("%s info maybe null\n", __func__);

	if (info->usb_otg)
		typec_burn_timer_start(info);

	return 0;
}
/* P16 code for HQFEAT-93945 by songweijie at 2025/03/11 end */

static int pd_verify_done_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = info->pd_verify_done;
	else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int pd_verify_done_set(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (info) {
		info->pd_verify_done = !!val;
		//if (info->pd_verify_done)
		//	 power_supply_changed(info->usb_psy);
	}
	chr_err("%s %d\n", __func__, info->pd_verify_done);
	return 0;
}

static int cp_charge_recovery_get(struct mtk_charger *info,
        struct mtk_usb_sysfs_field_info *attr,
        int *val)
{
        if (info)
                *val = info->suspend_recovery;
        else
                *val = 0;
        chr_err("%s %d\n", __func__, *val);
        return 0;
}

static int cp_charge_recovery_set(struct mtk_charger *info,
        struct mtk_usb_sysfs_field_info *attr,
        int val)
{
        if (info)
                info->suspend_recovery = val;
        chr_err("%s %d\n", __func__, val);
        return 0;
}

static int pmic_vbus_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = get_vbus(info);
	else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int input_current_now_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = get_ibus(info);
	else
		*val = 0;
	chr_err("%s %d\n", __func__, *val);
	return 0;
}

static int entry_soc_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	*val = info->entry_soc;
	chr_err("%s val:%d\n", __func__, *val);
	return 0;
}

static int entry_soc_set(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	info->entry_soc = val;
	chr_err("%s val:%d\n", __func__, val);
	return 0;
}

static int thermal_remove_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = info->thermal_remove;
	else
		*val = 0;
	chr_err("%s val:%d\n", __func__, *val);
	return 0;
}

static int thermal_remove_set(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (info)
		info->thermal_remove = !!val;
	chr_err("%s val:%d\n", __func__, val);
	return 0;
}

static int cp_sm_run_state_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = info->cp_sm_run_state;
	else
		*val = 0;
	chr_err("%s val:%d\n", __func__, *val);
	return 0;
}

static int cp_sm_run_state_set(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (info)
		info->cp_sm_run_state = !!val;
	chr_err("%s val:%d\n", __func__, val);
	return 0;
}

static int warm_term_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info) {
		*val = info->warm_term;
	}
	else
		*val = 0;
	chr_err("%s val:%d\n", __func__, *val);
	return 0;
}

static int adapter_id_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info){
		*val = info->pd_adapter->adapter_id;
	}
	else
		*val = 0;
	chr_err("%s val:0x%08x\n", __func__, *val);
	return 0;
}

static const char * const power_supply_typec_mode_text[] = {
	"Nothing attached", "Sink attached", "Powered cable w/ sink",
	"Debug Accessory", "Audio Adapter", "Powered cable w/o sink",
	"Source attached (default current)",
	"Source attached (medium current)",
	"Source attached (high current)",
	"Non compliant",
};

static const char *get_typec_mode_name(int typec_mode)
{
	u32 i;
	for (i = 0; i < ARRAY_SIZE(power_supply_typec_mode_text); i++) {
		if (i == typec_mode)
			return power_supply_typec_mode_text[i];
	}
	return "Nothing attached";
}

static const char * const power_supply_usb_type_text[] = {
	"Unknown", "OCP", "USB_FLOAT", "USB", "USB_CDP", "USB_DCP", "USB_HVDCP_2", "USB_HVDCP_3",
	"USB_HVDCP_3P5", "USB_HVDCP_3P5", "USB_HVDCP_3", "USB_HVDCP_3", "USB_PD", "PD_DRP", "USB_HVDCP", "PD_PPS", "USB_HVDCP1", "Unknown"
};

static const char *get_usb_type_name(int usb_type)
{
	u32 i;
	for (i = 0; i < ARRAY_SIZE(power_supply_usb_type_text); i++) {
		if (i == usb_type)
			return power_supply_usb_type_text[i];
	}
	return "Unknown";
}

static ssize_t usb_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy;
	struct mtk_charger *info;
	struct mtk_usb_sysfs_field_info *usb_attr;
	int val;
	ssize_t ret;
	ret = kstrtos32(buf, 0, &val);
	if (ret < 0)
		return ret;
	psy = dev_get_drvdata(dev);
	info = (struct mtk_charger *)power_supply_get_drvdata(psy);
	usb_attr = container_of(attr,
		struct mtk_usb_sysfs_field_info, attr);
	if (usb_attr->set != NULL)
		usb_attr->set(info, usb_attr, val);
	return count;
}

static ssize_t usb_sysfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct mtk_charger *info;
	struct mtk_usb_sysfs_field_info *usb_attr;
	int val = 0;
	ssize_t count;
	psy = dev_get_drvdata(dev);
	info = (struct mtk_charger *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct mtk_usb_sysfs_field_info, attr);
	if (usb_attr->get != NULL)
		usb_attr->get(info, usb_attr, &val);
	if (usb_attr->prop == USB_PROP_REAL_TYPE) {
		count = scnprintf(buf, PAGE_SIZE, "%s\n", get_usb_type_name(val));
		chr_err("real type = %s\n", get_usb_type_name(val));
		return count;
	} else if (usb_attr->prop == USB_PROP_TYPEC_MODE) {
		count = scnprintf(buf, PAGE_SIZE, "%s\n", get_typec_mode_name(val));
		return count;
	}
	count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
	return count;
}

static int shipmode_count_reset_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info)
		*val = info->shipmode_flag;
	else
		*val = 0;
	chr_info("[%s] shipmode_flag = %d\n", __func__, *val);
	return 0;
}

static int shipmode_count_reset_set(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (info) {
		info->shipmode_flag = !!val;
		chr_info("[%s] shipmode_flag = %d\n", __func__, info->shipmode_flag);
	} else {
		chr_err("[%s] info is NULL\n", __func__);
	}

	return 0;
}

static int mtbf_mode_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (info) {
		*val = info->is_mtbf_mode;
	} else {
		*val = 0;
	}
	chr_info("[%s] is_mtbf_mode = %d\n", __func__, *val);
	return 0;
}

static int mtbf_mode_set(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (info) {
		info->is_mtbf_mode = !!val;
		chr_info("[%s] is_mtbf_mode = %d\n", __func__, info->is_mtbf_mode);
	} else {
		chr_err("[%s] info is NULL\n", __func__);
	}

	return 0;
}

static int soc_decimal_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,  int *val)
{
	int soc_decimal = 0;
	struct fuel_gauge_dev *fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");

	soc_decimal = fuel_gauge_get_soc_decimal(fuel_gauge);
	if (soc_decimal < 0)
		soc_decimal = 0;

	*val = soc_decimal;

	return 0;
}

static int soc_decimal_rate_get(struct mtk_charger *info,
	struct mtk_usb_sysfs_field_info *attr,  int *val)
{
	int soc_decimal_rate = 0;
	struct fuel_gauge_dev *fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");

	soc_decimal_rate = fuel_gauge_get_soc_decimal_rate(fuel_gauge);
	if (soc_decimal_rate < 0 || soc_decimal_rate > 100)
		soc_decimal_rate = 0;

	*val = soc_decimal_rate;

	return 0;
}

/* Must be in the same order as USB_PROP_* */
static struct mtk_usb_sysfs_field_info usb_sysfs_field_tbl[] = {
	USB_SYSFS_FIELD_RW(real_type, USB_PROP_REAL_TYPE),
	USB_SYSFS_FIELD_RO(quick_charge_type, USB_PROP_QUICK_CHARGE_TYPE),
	USB_SYSFS_FIELD_RW(pd_authentication, USB_PROP_PD_AUTHENTICATION),
	USB_SYSFS_FIELD_RW(pd_verifying, USB_PROP_PD_VERIFYING),
	USB_SYSFS_FIELD_RO(pd_type, USB_PROP_PD_TYPE),
	USB_SYSFS_FIELD_RW(apdo_max, USB_PROP_APDO_MAX),
	USB_SYSFS_FIELD_RW(typec_mode, USB_PROP_TYPEC_MODE),
	USB_SYSFS_FIELD_RW(typec_cc_orientation, USB_PROP_TYPEC_CC_ORIENTATION),
	USB_SYSFS_FIELD_RO(ffc_enable, USB_PROP_FFC_ENABLE),
	USB_SYSFS_FIELD_RO(charge_full, USB_PROP_CHARGE_FULL),
	USB_SYSFS_FIELD_RW(typec_ntc1_temp, USB_PROP_TYPEC_NTC1_TEMP),
	USB_SYSFS_FIELD_RW(typec_ntc2_temp, USB_PROP_TYPEC_NTC2_TEMP),
	USB_SYSFS_FIELD_RO(typec_burn, USB_PROP_TYPEC_BURN),
	USB_SYSFS_FIELD_RO(sw_cv, USB_PROP_SW_CV),
	USB_SYSFS_FIELD_RW(input_suspend, USB_PROP_INPUT_SUSPEND),
	USB_SYSFS_FIELD_RO(jeita_chg_index, USB_PROP_JEITA_CHG_INDEX),
	USB_SYSFS_FIELD_RO(power_max, USB_PROP_POWER_MAX),
	USB_SYSFS_FIELD_RW(otg_enable, USB_PROP_OTG_ENABLE),
	USB_SYSFS_FIELD_RW(pd_verify_done, USB_PROP_PD_VERIFY_DONE),
	USB_SYSFS_FIELD_RW(cp_charge_recovery, USB_PROP_CP_CHARGE_RECOVERY),
	USB_SYSFS_FIELD_RO(pmic_ibat, USB_PROP_PMIC_IBAT),
	USB_SYSFS_FIELD_RO(pmic_vbus, USB_PROP_PMIC_VBUS),
	USB_SYSFS_FIELD_RO(input_current_now, USB_PROP_INPUT_CURRENT_NOW),
	USB_SYSFS_FIELD_RW(thermal_remove, USB_PROP_THERMAL_REMOVE),
	USB_SYSFS_FIELD_RO(warm_term, USB_PROP_WARM_TERM),
	USB_SYSFS_FIELD_RO(adapter_id, USB_PROP_ADAPTER_ID),
	USB_SYSFS_FIELD_RW(entry_soc, USB_PROP_ENTRY_SOC),
	USB_SYSFS_FIELD_RW(cp_sm_run_state, USB_PROP_CP_SM_RUN_STATE),
	USB_SYSFS_FIELD_RW(adapter_imax, USB_PROP_ADAPTER_IMAX),
	USB_SYSFS_FIELD_RW(usb_otg, USB_PROP_USB_OTG),
	USB_SYSFS_FIELD_RW(shipmode_count_reset, USB_PROP_SHIPMODE_COUNT_RESET),
	USB_SYSFS_FIELD_RW(mtbf_mode, USB_PROP_MTBF_MODE),
	USB_SYSFS_FIELD_RO(soc_decimal, USB_PROP_SOC_DECIMAL),
	USB_SYSFS_FIELD_RO(soc_decimal_rate, USB_PROP_SOC_DECIMAL_RATE),
};

static struct attribute *
	usb_sysfs_attrs[ARRAY_SIZE(usb_sysfs_field_tbl) + 1];
static const struct attribute_group usb_sysfs_attr_group = {
	.attrs = usb_sysfs_attrs,
};

static void usb_sysfs_init_attrs(void)
{
	int i, limit = ARRAY_SIZE(usb_sysfs_field_tbl);
	for (i = 0; i < limit; i++)
		usb_sysfs_attrs[i] = &usb_sysfs_field_tbl[i].attr.attr;
	usb_sysfs_attrs[limit] = NULL; /* Has additional entry for this */
}

static int usb_sysfs_create_group(struct power_supply *psy)
{
	usb_sysfs_init_attrs();
	return sysfs_create_group(&psy->dev.kobj,
			&usb_sysfs_attr_group);
}

static void usbpd_mi_vdm_received_cb(struct mtk_charger *pinfo, struct tcp_ny_cvdm uvdm)
{
	int i, cmd;
	chr_err("adapter_svid = 0x%x\n", pinfo->pd_adapter->adapter_svid);
	if (pinfo->pd_adapter->adapter_svid != USB_PD_MI_SVID && pinfo->pd_adapter->adapter_svid != 0x2B01)
		return;
	cmd = UVDM_HDR_CMD(uvdm.data[0]);
	chr_err("cmd = %d\n", cmd);
	switch (cmd) {
	case USBPD_UVDM_CHARGER_VERSION:
		pinfo->pd_adapter->vdm_data.ta_version = uvdm.data[1];
		chr_err("ta_version:%x\n", pinfo->pd_adapter->vdm_data.ta_version);
		break;
	case USBPD_UVDM_CHARGER_TEMP:
		pinfo->pd_adapter->vdm_data.ta_temp = (uvdm.data[1] & 0xFFFF) * 10;
		chr_err("pinfo->pd_adapter->vdm_data.ta_temp:%d\n", pinfo->pd_adapter->vdm_data.ta_temp);
		break;
	case USBPD_UVDM_CHARGER_VOLTAGE:
		pinfo->pd_adapter->vdm_data.ta_voltage = (uvdm.data[1] & 0xFFFF) * 10;
		pinfo->pd_adapter->vdm_data.ta_voltage *= 1000;
		chr_err("ta_voltage:%d\n", pinfo->pd_adapter->vdm_data.ta_voltage);
		break;
	case USBPD_UVDM_SESSION_SEED:
		for (i = 0; i < USBPD_UVDM_SS_LEN; i++) {
			pinfo->pd_adapter->vdm_data.s_secert[i] = uvdm.data[i+1];
			chr_err("usbpd s_secert uvdm.uvdm_data[%d]=0x%x", i+1, uvdm.data[i+1]);
		}
		break;
	case USBPD_UVDM_AUTHENTICATION:
		for (i = 0; i < USBPD_UVDM_SS_LEN; i++) {
			pinfo->pd_adapter->vdm_data.digest[i] = uvdm.data[i+1];
			chr_err("usbpd digest[%d]=0x%x", i+1, uvdm.data[i+1]);
		}
		break;
	case USBPD_UVDM_REVERSE_AUTHEN:
		pinfo->pd_adapter->vdm_data.reauth = (uvdm.data[1] & 0xFFFF);
		break;
	default:
		break;
	}
	pinfo->pd_adapter->uvdm_state = cmd;
}

int notify_adapter_event(struct notifier_block *notifier,
			unsigned long evt, void *val)
{
	struct mtk_charger *pinfo = NULL;
	u32 boot_mode = 0;
	bool report_psy = true;

	chr_err("%s %lu\n", __func__, evt);

	pinfo = container_of(notifier,
		struct mtk_charger, pd_nb);
	boot_mode = pinfo->bootmode;

	switch (evt) {
	case MTK_PD_CONNECT_NONE:
		mutex_lock(&pinfo->pd_lock);
		chr_err("notify_adapter_event PD Notify Detach\n");
		pinfo->pd_type = MTK_PD_CONNECT_NONE;
		pinfo->pd_reset = false;
		pinfo->real_type = XMUSB350_TYPE_UNKNOW;
		mutex_unlock(&pinfo->pd_lock);
		mtk_chg_alg_notify_call(pinfo, EVT_DETACH, 0);
		/* reset PE40 */
		break;

	case MTK_PD_CONNECT_HARD_RESET:
		mutex_lock(&pinfo->pd_lock);
		chr_err("PD Notify HardReset\n");
		pinfo->pd_type = MTK_PD_CONNECT_NONE;
		pinfo->pd_reset = true;
		mutex_unlock(&pinfo->pd_lock);
		mtk_chg_alg_notify_call(pinfo, EVT_HARDRESET, 0);
		_wake_up_charger(pinfo);
		/* reset PE40 */
		break;

	case MTK_PD_CONNECT_SOFT_RESET:
		mutex_lock(&pinfo->pd_lock);
		chr_err("PD Notify SoftReset\n");
		pinfo->pd_type = MTK_PD_CONNECT_SOFT_RESET;
		pinfo->pd_reset = false;
		mutex_unlock(&pinfo->pd_lock);
		mtk_chg_alg_notify_call(pinfo, EVT_SOFTRESET, 0);
		_wake_up_charger(pinfo);
		/* reset PE50 */
		break;

	case MTK_PD_CONNECT_PE_READY_SNK:
		mutex_lock(&pinfo->pd_lock);
		chr_err("PD Notify fixed voltage ready\n");
		pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK;
		pinfo->pd_reset = false;
		pinfo->real_type = XMUSB350_TYPE_PD;
		mutex_unlock(&pinfo->pd_lock);
		/* PD is ready */
		break;

	case MTK_PD_CONNECT_PE_READY_SNK_PD30:
		mutex_lock(&pinfo->pd_lock);
		chr_err("PD Notify PD30 ready\r\n");
		pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK_PD30;
		pinfo->pd_reset = false;
		pinfo->real_type = XMUSB350_TYPE_PD;
		mutex_unlock(&pinfo->pd_lock);
		/* PD30 is ready */
		break;

	case MTK_PD_CONNECT_PE_READY_SNK_APDO:
		mutex_lock(&pinfo->pd_lock);
		chr_err("PD Notify APDO Ready\n");
		pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK_APDO;
		pinfo->pd_reset = false;
		pinfo->real_type = XMUSB350_TYPE_PD_PPS;
		mutex_unlock(&pinfo->pd_lock);
		/* PE40 is ready */
		_wake_up_charger(pinfo);
		break;

	case MTK_PD_CONNECT_TYPEC_ONLY_SNK:
		mutex_lock(&pinfo->pd_lock);
		chr_err("PD Notify Type-C Ready\n");
		pinfo->pd_type = MTK_PD_CONNECT_TYPEC_ONLY_SNK;
		pinfo->pd_reset = false;
		mutex_unlock(&pinfo->pd_lock);
		/* type C is ready */
		_wake_up_charger(pinfo);
		break;
	case MTK_TYPEC_WD_STATUS:
		chr_err("wd status = %d\n", *(bool *)val);
		pinfo->water_detected = *(bool *)val;
		if (pinfo->water_detected == true) {
			pinfo->notify_code |= CHG_TYPEC_WD_STATUS;
			pinfo->record_water_detected = true;
			if (boot_mode == 8 || boot_mode == 9)
				pinfo->enable_hv_charging = false;
		} else {
			pinfo->notify_code &= ~CHG_TYPEC_WD_STATUS;
			if (boot_mode == 8 || boot_mode == 9)
				pinfo->enable_hv_charging = true;
		}
		mtk_chgstat_notify(pinfo);
		report_psy = boot_mode == 8 || boot_mode == 9;
		break;
	case MTK_TYPEC_CC_HI_STATUS:
		chr_err("cc_hi = %d\n", *(int *)val);
		pinfo->cc_hi = *(int *)val;
		_wake_up_charger(pinfo);
		break;
	case MTK_PD_UVDM:
		mutex_lock(&pinfo->pd_lock);
		usbpd_mi_vdm_received_cb(pinfo, *(struct tcp_ny_cvdm *)val);
		mutex_unlock(&pinfo->pd_lock);
		break;
	}
	if (report_psy)
		power_supply_changed(pinfo->psy1);
	return NOTIFY_DONE;
}

int chg_alg_event(struct notifier_block *notifier,
			unsigned long event, void *data)
{
	chr_err("%s: evt:%lu\n", __func__, event);

	return NOTIFY_DONE;
}

static struct regmap *pmic_get_regmap(const char *name)
{
	struct device_node *np;
	struct platform_device *pdev;
	np = of_find_node_by_name(NULL, name);
	if (!np) {
		chr_err("%s: device node %s not found!\n", __func__, name);
		return NULL;
	}
	pdev = of_find_device_by_node(np->child);
	if (!pdev) {
		chr_err("%s: mt6369 platform device not found!\n", __func__);
		return NULL;
	}
	return dev_get_regmap(pdev->dev.parent, NULL);
}

static char *mtk_charger_supplied_to[] = {
	"battery"
};

static char *mtk_usb_supplied_to[] = {
	"battery",
};

static void jeita_init_workfunc(struct work_struct *work)
{
	struct mtk_charger *info = container_of(work, struct mtk_charger, jeita_init_work.work);
	union power_supply_propval pval = {0,};
	char info_bufer[32] = {0};
	static int count;
	int ret = 0;

	if (count >= 3)
		goto init_jeita;

	if (!info->bat_psy)
		info->bat_psy = power_supply_get_by_name("battery");
	if (info->bat_psy) {
		ret = power_supply_get_property(info->bat_psy, POWER_SUPPLY_PROP_MODEL_NAME, &pval);
		if (ret < 0) {
			chr_err("failed to read battery info from fg\n");
			goto err;
		}
		strcpy(info_bufer, pval.strval);
		if (strstr(info_bufer, "UNKNOWN") != NULL) {
			chr_err("batt info err\n");
			goto err;
		}
	} else {
		chr_err("failed to get battery psy\n");
		goto err;
	}

init_jeita:
	ret = step_jeita_init(info, &info->pdev->dev);
	if (ret < 0) {
		chr_err("failed to register step_jeita charge\n");
		info->jeita_support = false;
	} else
		info->jeita_support = true;

#if IS_ENABLED(CONFIG_XM_SMART_CHG)
	chr_err("in CONFIG_XM_SMART_CHG\n");
	ret = xm_smart_chg_init(info);
	if (ret < 0) {
		chr_err("xm smart charge feature init failed, ret = %d\n", ret);
		return;
	}
#endif

#if IS_ENABLED(CONFIG_XM_BATTERY_HEALTH)
	chr_err("in CONFIG_XM_SMART_CHG\n");
	ret = xm_batt_health_init(info);
	if (ret < 0) {
		chr_err("xm battery health feature init failed, ret = %d\n", ret);
		return ;
	}
#endif

	return;
err:
	count ++;
	schedule_delayed_work(&info->jeita_init_work, msecs_to_jiffies(1000));
	return;
}

/*******************************add mtk_charger ops start*******************************/
static void night_charging_set_flag(struct mtk_charger *info, bool night_charging)
{
	info->night_charging = night_charging;
	chr_err("%s night_charging=%d\n", __func__, info->night_charging);
}

static void night_charging_get_flag(struct mtk_charger *info, bool *night_charging)
{
	*night_charging = info->night_charging;
	chr_err("%s night_charging=%d\n", __func__, info->night_charging);
}

static void smart_batt_set_diff_fv(struct mtk_charger *info, int val)
{
	chr_err("%s set_smart_batt_diff_fv=%d\n", __func__, val);
	info->set_smart_batt_diff_fv = val;
}

static void smart_soclmt_get_flag(struct mtk_charger *info, bool *smart_soclmt_trig)
{
	chr_err("%s smart_soclmt_trig=%d\n", __func__, info->smart_soclmt_trig);
	*smart_soclmt_trig = info->smart_soclmt_trig;
}

static void manual_set_cc_toggle(struct mtk_charger *info, bool en)
{
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;

	chr_err("into %s\n", __func__);

	if(info->tcpc == NULL){
		chr_err("%s get tcpc fail\n", __func__);
		return;
	}

	if(!info->en_floatgnd) {
		chr_err("%s floatgnd not enable\n", __func__);
		return;
	}

	info->ui_cc_toggle = en;

	if (!info->typec_attach && en) {
		chr_err("%s set cc toggle\n", __func__);
		schedule_delayed_work(&info->handle_cc_status_work, 0);
	} else if (!info->typec_attach && !en){
		chr_err("%s set cc not toggle\n", __func__);
		schedule_delayed_work(&info->handle_cc_status_work, 0);
	} else {
		chr_err("%s typec is attached, not set cc\n", __func__);
	}

	if(en && !info->cid_status)
	{
		ret = alarm_try_to_cancel(&info->otg_ui_close_timer);
		if (ret < 0) {
			chr_err("%s: callback was running, skip timer\n", __func__);
			return;
		}
		ktime_now = ktime_get_boottime();
		time_now = ktime_to_timespec64(ktime_now);
		end_time.tv_sec = time_now.tv_sec + 600;
		end_time.tv_nsec = time_now.tv_nsec + 0;
		ktime = ktime_set(end_time.tv_sec,end_time.tv_nsec);

		chr_err("%s: alarm timer start:%d, %lld %ld\n", __func__, ret,
			end_time.tv_sec, end_time.tv_nsec);
		alarm_start(&info->otg_ui_close_timer, ktime);
		chr_err("%s ui set cc toggle : start otg_ui_close_timer\n", __func__);
	} else {
		ret = alarm_try_to_cancel(&info->otg_ui_close_timer);
		if (ret < 0) {
			chr_err("%s: callback was running, skip timer\n", __func__);
			return;
		}
		chr_err("%s ui disable cc toggle : stop otg_ui_close_timer\n", __func__);
	}
	chr_err("%s\n", __func__);

	return;
}

static void manual_get_cc_toggle(struct mtk_charger *info, bool *cc_toggle)
{
	*cc_toggle = info->ui_cc_toggle;
	chr_err("%s = %d\n", __func__, *cc_toggle);
}

static void manual_get_cid_status(struct mtk_charger *info, bool *cid_status)
{
	chr_err("%s = %d\n", __func__, info->cid_status);
	*cid_status = info->cid_status;
}

static void set_soft_reset_status(struct mtk_charger *info, int pd_soft_reset)
{
	info->pd_soft_reset = !!pd_soft_reset;
	chr_err("%s:pd_soft_reset = %d\n", __func__, info->pd_soft_reset);
}

static void get_soft_reset_status(struct mtk_charger *info, int *pd_soft_reset)
{
	*pd_soft_reset = pinfo->pd_soft_reset;
}

static void input_suspend_get_flag(struct mtk_charger *info, bool *input_suspend)
{
	chr_err("%s input_suspend=%d\n", __func__, info->input_suspend);
	*input_suspend = info->input_suspend;
}

static void input_suspend_set_flag(struct mtk_charger *info, int input_suspend)
{
	info->input_suspend = !!input_suspend;
	if (!info->chg1_dev || !info->psy1 || !info->usb_psy)
		return;
	charger_dev_enable_powerpath(info->chg1_dev, !input_suspend);
	power_supply_changed(info->psy1);
	if (!input_suspend) {
		power_supply_changed(info->usb_psy);
	}
	chr_err("%s input_suspend =%d\n", __func__, info->input_suspend);
}

static void update_quick_chg_type(struct mtk_charger *info)
{
#if 0
	if (!info->bat_psy)
		info->bat_psy = power_supply_get_by_name("battery");

	if(info->bat_psy != NULL) {
		generate_xm_charge_uevent(info);
		xm_uevent_report(info);
	}
#endif
}

static void update_connect_temp(struct mtk_charger *info)
{
#if 0
	if (info)
		generate_xm_charge_uevent(info);
#endif
}

static int mtk_set_mt6369_moscon1(struct mtk_charger *info, bool en, int drv_sel)
{
	if(en)
		return regmap_set_bits(info-> mt6369_regmap, MT6369_STRUP_ANA_CON1,
				(en << 1 | drv_sel << 2));
	else
		return regmap_clear_bits(info-> mt6369_regmap, MT6369_STRUP_ANA_CON1, 0x6);
}

static int usb_get_property(struct mtk_charger *info, enum usb_property bp, int *val)
{
	if (usb_sysfs_field_tbl[bp].prop == bp)
		usb_sysfs_field_tbl[bp].get(info,
			&usb_sysfs_field_tbl[bp], val);
	else {
		chr_err("%s usb bp:%d idx error\n", __func__, bp);
		return -ENOTSUPP;
	}
	return 0;
}

static int usb_set_property(struct mtk_charger *info, enum usb_property bp, int val)
{
	if (usb_sysfs_field_tbl[bp].prop == bp)
		usb_sysfs_field_tbl[bp].set(info,
			&usb_sysfs_field_tbl[bp], val);
	else {
		chr_err("%s usb bp:%d idx error\n", __func__, bp);
		return -ENOTSUPP;
	}
	return 0;
}

static int reverse_quick_charge_get_flag(struct mtk_charger *info, bool *reverse_quick_charge)
{
	*reverse_quick_charge = info->reverse_quick_charge;
	chr_err("%s reverse_quick_charge=%d\n", __func__, info->reverse_quick_charge);

	return 0;
}

static int reverse_quick_charge_set_flag(struct mtk_charger *info, bool reverse_quick_charge)
{
	info->reverse_quick_charge = reverse_quick_charge;
	set_reverse_quick_charge(info->reverse_quick_charge);
	chr_err("%s reverse_quick_charge =%d\n", __func__, info->reverse_quick_charge);

	return 0;
}

static int revchg_bcl_get_flag(struct mtk_charger *info, bool *revchg_bcl)
{
	*revchg_bcl = info->revchg_bcl;
	chr_err("%s get revchg_bcl%d\n", __func__, info->revchg_bcl);

	return 0;
}

static int revchg_bcl_set_flag(struct mtk_charger *info, bool revchg_bcl)
{

	info->revchg_bcl = revchg_bcl;
	chr_err("%s set revchg_bcl =%d\n", __func__, info->revchg_bcl);
	return 0;
}

/*******************************add mtk_charger ops end*******************************/

/*******************************add charger dev ops start*******************************/
static int mtk_charger_night_charging_set_flag(struct charger_device *chg, bool night_charging) {
	struct mtk_charger *info = charger_get_data(chg);

	if (!info)
		return -ENOMEM;

	night_charging_set_flag(info, night_charging);

	return 0;
}

static int mtk_charger_night_charging_get_flag(struct charger_device *chg, bool *night_charging) {
	struct mtk_charger *info = charger_get_data(chg);

	if (!info)
		return -ENOMEM;

	night_charging_get_flag(info, night_charging);

	return 0;
}

static int mtk_charger_smart_batt_set_diff_fv(struct charger_device *chg, int val) {
	struct mtk_charger *info = charger_get_data(chg);

	if (!info)
		return -ENOMEM;

	smart_batt_set_diff_fv(info, val);

	return 0;
}

static int mtk_charger_smart_soclmt_get_flag(struct charger_device *chg, bool *smart_soclmt_trig) {
	struct mtk_charger *info = charger_get_data(chg);

	if (!info)
		return -ENOMEM;

	smart_soclmt_get_flag(info, smart_soclmt_trig);

	return 0;
}

static int mtk_charger_manual_set_cc_toggle(struct charger_device *chg, bool en) {
	struct mtk_charger *info = charger_get_data(chg);

	if (!info)
		return -ENOMEM;

	manual_set_cc_toggle(info, en);

	return 0;
}

static int mtk_charger_manual_get_cc_toggle(struct charger_device *chg, bool *en) {
	struct mtk_charger *info = charger_get_data(chg);

	if (!info)
		return -ENOMEM;

	manual_get_cc_toggle(info, en);

	return 0;
}

static int mtk_charger_manual_get_cid_status(struct charger_device *chg, bool *cid_status) {
	struct mtk_charger *info = charger_get_data(chg);

	if (!info)
		return -ENOMEM;

	manual_get_cid_status(info, cid_status);

	return 0;
}

static int mtk_charger_set_soft_reset_status(struct charger_device *chg, int pd_soft_reset) {
	struct mtk_charger *info = charger_get_data(chg);

	if (!info)
		return -ENOMEM;

	set_soft_reset_status(info, pd_soft_reset);

	return 0;
}

static int mtk_charger_get_soft_reset_status(struct charger_device *chg, int *pd_soft_reset) {
	struct mtk_charger *info = charger_get_data(chg);

	if (!info)
		return -ENOMEM;

	get_soft_reset_status(info, pd_soft_reset);

	return 0;
}

static int mtk_charger_input_suspend_get_flag(struct charger_device *chg, bool *input_suspend) {
	struct mtk_charger *info = charger_get_data(chg);

	if (!info)
		return -ENOMEM;

	input_suspend_get_flag(info, input_suspend);

	return 0;
}

static int mtk_charger_input_suspend_set_flag(struct charger_device *chg, bool input_suspend) {
	struct mtk_charger *info = charger_get_data(chg);

	if (!info)
		return -ENOMEM;

	input_suspend_set_flag(info, input_suspend);

	return 0;
}

static int mtk_charger_update_quick_chg_type(struct charger_device *chg) {
	struct mtk_charger *info = charger_get_data(chg);

	if (!info)
		return -ENOMEM;

	update_quick_chg_type(info);

	return 0;
}

static int mtk_charger_update_connect_temp(struct charger_device *chg) {
	struct mtk_charger *info = charger_get_data(chg);

	if (!info)
		return -ENOMEM;

	update_connect_temp(info);

	return 0;
}

static int mtk_charger_mtk_set_mt6369_moscon1(struct charger_device *chg, bool en, int drv_sel) {
	struct mtk_charger *info = charger_get_data(chg);

	if (!info)
		return -ENOMEM;

	mtk_set_mt6369_moscon1(info, en, drv_sel);

	return 0;
}

static int mtk_charger_usb_get_property(struct charger_device *chg, enum usb_property bp, int *val) {
	struct mtk_charger *info = charger_get_data(chg);

	if (!info)
		return -ENOMEM;

	usb_get_property(info, bp, val);

	return 0;
}

static int mtk_charger_usb_set_property(struct charger_device *chg, enum usb_property bp, int val) {
	struct mtk_charger *info = charger_get_data(chg);

	if (!info)
		return -ENOMEM;

	usb_set_property(info, bp, val);

	return 0;
}

static int mtk_charger_reverse_quick_charge_get_flag(struct charger_device *chg, bool *reverse_quick_charge) {
	struct mtk_charger *info = charger_get_data(chg);

	if (!info)
		return -ENOMEM;

	reverse_quick_charge_get_flag(info, reverse_quick_charge);

	return 0;
}

static int mtk_charger_reverse_quick_charge_set_flag(struct charger_device *chg, bool reverse_quick_charge) {
	struct mtk_charger *info = charger_get_data(chg);

	if (!info)
		return -ENOMEM;

	reverse_quick_charge_set_flag(info, reverse_quick_charge);

	return 0;
}

static int mtk_charger_revchg_bcl_get_flag(struct charger_device *chg, bool *revchg_bcl) {
	struct mtk_charger *info = charger_get_data(chg);

	if (!info)
		return -ENOMEM;

	revchg_bcl_get_flag(info, revchg_bcl);

	return 0;
}

static int mtk_charger_revchg_bcl_set_flag(struct charger_device *chg, bool revchg_bcl) {
	struct mtk_charger *info = charger_get_data(chg);

	if (!info)
		return -ENOMEM;

	revchg_bcl_set_flag(info, revchg_bcl);

	return 0;
}

static const struct charger_properties mtk_charger_props = {
	.alias_name = "mtk_charger",
};

static const struct charger_ops mtk_charger_ops = {
	.night_charging_set_flag = mtk_charger_night_charging_set_flag,
	.night_charging_get_flag = mtk_charger_night_charging_get_flag,
	.smart_batt_set_diff_fv = mtk_charger_smart_batt_set_diff_fv,
	.smart_soclmt_get_flag = mtk_charger_smart_soclmt_get_flag,
	.manual_set_cc_toggle = mtk_charger_manual_set_cc_toggle,
	.manual_get_cc_toggle = mtk_charger_manual_get_cc_toggle,
	.manual_get_cid_status = mtk_charger_manual_get_cid_status,
	.set_soft_reset_status = mtk_charger_set_soft_reset_status,
	.get_soft_reset_status = mtk_charger_get_soft_reset_status,
	.input_suspend_get_flag = mtk_charger_input_suspend_get_flag,
	.input_suspend_set_flag = mtk_charger_input_suspend_set_flag,
	.update_quick_chg_type = mtk_charger_update_quick_chg_type,
	.update_connect_temp = mtk_charger_update_connect_temp,
	.mtk_set_mt6369_moscon1 = mtk_charger_mtk_set_mt6369_moscon1,
	.usb_get_property = mtk_charger_usb_get_property,
	.usb_set_property = mtk_charger_usb_set_property,
	.reverse_quick_charge_get_flag = mtk_charger_reverse_quick_charge_get_flag,
	.reverse_quick_charge_set_flag = mtk_charger_reverse_quick_charge_set_flag,
	.revchg_bcl_get_flag = mtk_charger_revchg_bcl_get_flag,
	.revchg_bcl_set_flag = mtk_charger_revchg_bcl_set_flag,
};

static int mtk_charger_init_chgdev(struct mtk_charger *info)
{
	info->mtk_charger = charger_device_register("mtk_charger", &info->pdev->dev,
						info, &mtk_charger_ops,
						&mtk_charger_props);

	return IS_ERR(info->mtk_charger) ? PTR_ERR(info->mtk_charger) : 0;
}
/*******************************add charger dev ops end*******************************/
void typec_burn_timer_start(struct mtk_charger *info)
{
    unsigned long flags;

    spin_lock_irqsave(&typec_timer_lock, flags);
    if (!timer_pending(&info->typec_burn_timer))
        mod_timer(&info->typec_burn_timer, jiffies + msecs_to_jiffies(1000));
    spin_unlock_irqrestore(&typec_timer_lock, flags);
}

static void monitor_typec_burn(struct work_struct *work)
{
	struct mtk_charger *info = container_of(work, struct mtk_charger, typec_burn_monitor_work.work);
	int type_temp = 0, retry_count = 3, val = 0, pmic_vbus = 0;
	unsigned char vdm_data[4] = {0};
	int typec_burn_noti[3] = {0, 0, 0};
	int vbus_down_fail = 0;


	usb_get_property(info, USB_PROP_TYPEC_NTC1_TEMP, &val);
	type_temp = val;
	usb_get_property(info, USB_PROP_TYPEC_NTC2_TEMP, &val);
	if (type_temp <= val)
		type_temp = val;

	if ((type_temp - info->last_typec_temp) >= 40 && type_temp >= 350 && info->last_typec_temp != -1000 && !info->usb_otg)
		info->typec_burn_status = true;
	else if (type_temp >= 600 && info->board_temp <= 500)
		info->typec_burn_status = true;
	else if (type_temp >= 650)
		info->typec_burn_status = true;

	if (info->typec_burn_status && !info->last_typec_burn_status) {
		info->last_typec_burn_status = info->typec_burn_status;

		/* typec burn uevent update */
		xm_charge_uevent_report(CHG_UEVENT_CONNECTOR_TEMP, type_temp);
		xm_charge_uevent_report(CHG_UEVENT_NTC_ALARM, info->typec_burn_status);

		adapter_dev_request_vdm_cmd(info->pd_adapter, USBPD_UVDM_DISABLE_VBUS, vdm_data, 3);
		msleep(50);
		if (info->otg_stat == HV_OTG) {
			charger_dev_cp_set_otg_config(info->cp_master, false);
			chr_err("%s disabe cp typec_burn_status = %d otg_stat = %d\n", __func__, info->typec_burn_status, info->otg_stat);
		} else	
			adapter_dev_set_cap_xm(info->pd_adapter, MTK_PD_APDO, 5000, 1000);
		if (info->real_type == XMUSB350_TYPE_HVCHG)
			charger_dev_set_dpdm_voltage(info->chg1_dev, 0, 0);
		usb_set_property(info, USB_PROP_INPUT_SUSPEND, info->typec_burn_status);
		vote(info->icl_votable, TYPEC_BURN_VOTER, true, 0);
		info->typec_burn_status = true;

		msleep(200);
		while (retry_count--) {
			pmic_vbus = get_vbus(info);
			if (pmic_vbus <= 6000) {
				break;
			} else {

			}
			msleep(200);
		}

		if (info->mt6369_moscon1_control) {
			mtk_set_mt6369_moscon1(info, 1, 1);
			chr_err("mt6368_moscon1_control set high\n");
		}

		retry_count = 3;
		msleep(50);
		while (retry_count--) {
			pmic_vbus = get_vbus(info);
			if (pmic_vbus <= 4000) {
				vbus_down_fail = 0;
				break;
			} else {
				vbus_down_fail = 1;
			}
			msleep(200);
		}

		typec_burn_noti[0] = info->typec_burn_status;
		typec_burn_noti[1] = type_temp;
		typec_burn_noti[2] = vbus_down_fail;
		mtk_charger_fw_notifier_call_chain(CHG_FW_EVT_TYPEC_BURN, &typec_burn_noti);
	} else if (info->typec_burn_status && !(info->plugged_status || info->otg_enable)) {
		if ((type_temp - info->last_typec_temp) < 40 &&  type_temp <= 550) {
			info->typec_burn_status = false;
			info->last_typec_burn_status = false;

			/* typec burn uevent update */
			xm_charge_uevent_report(CHG_UEVENT_CONNECTOR_TEMP, type_temp);
			xm_charge_uevent_report(CHG_UEVENT_NTC_ALARM, info->typec_burn_status);

			if (info->mt6369_moscon1_control) {
				mtk_set_mt6369_moscon1(info, 0, 0);
				chr_err("mt6369_moscon1_control set low\n");
			}
			usb_set_property(info, USB_PROP_INPUT_SUSPEND, info->typec_burn_status);
			vote(info->icl_votable, TYPEC_BURN_VOTER, false, 0);

			typec_burn_noti[0] = info->typec_burn_status;
			typec_burn_noti[1] = type_temp;
			typec_burn_noti[2] = vbus_down_fail;
			mtk_charger_fw_notifier_call_chain(CHG_FW_EVT_TYPEC_BURN, &typec_burn_noti);

		}
	}

	chr_err("%s typec_burn_status = %d, last_typec_burn_status = %d, type_temp = %d, last_typec_temp = %d, board_temp = %d\n",
		__func__, info->typec_burn_status, info->last_typec_burn_status, type_temp, info->last_typec_temp, info->board_temp);
	info->last_typec_temp = type_temp;
	if (info->usb_otg || (!timer_pending(&info->typec_burn_timer) && info->typec_burn_status))
		schedule_delayed_work(&info->typec_burn_monitor_work, msecs_to_jiffies(1000));
}

static void monitor_typec_burn_policy(struct timer_list *t)
{
	struct mtk_charger *info = from_timer(info, t, typec_burn_timer);
	unsigned long flags;

	spin_lock_irqsave(&typec_timer_lock, flags);
	if (info->typec_burn_status || info->plugged_status || info->usb_otg) {
		schedule_delayed_work(&info->typec_burn_monitor_work, msecs_to_jiffies(0));
		if (!info->usb_otg)
			mod_timer(&info->typec_burn_timer, jiffies+msecs_to_jiffies(1000));
	} else {
		info->last_typec_temp = -1000;
		info->last_typec_burn_status = false;
	}
	spin_unlock_irqrestore(&typec_timer_lock, flags);
}

static void start_vbus_check(struct work_struct *work)
{
	struct mtk_charger *info =
		container_of(work, struct mtk_charger, start_vbus_check_work.work);

	info->vbus_check = true;
	chr_err("%s: vbus_check true\n", __func__);
}

#if IS_ENABLED(CONFIG_RUST_DETECTION)
static void rust_detection_work_func(struct work_struct *work)
{
	struct timespec64 time;
	ktime_t tmp_time = 0;
	struct mtk_charger *info = container_of(work, struct mtk_charger, rust_detection_work.work);
	int res, vbus = 0;
	static int rust_det_interval = 5000;
	tmp_time = ktime_get_boottime();
	time = ktime_to_timespec64(tmp_time);

	if(time.tv_sec < 50) {
		chr_err("%s boot do not enter\n", __func__);
		goto out;
	}

	if (info->typec_switch_chg == NULL) {
		info->typec_switch_chg = get_charger_by_name("typec_switch_chg");
		if (info->typec_switch_chg)
			chr_err("Found typec_switch_chg\n");
		else {
			chr_err("can't find typec_switch_chg\n");
			goto out;
		}
	}

	charger_dev_rust_detection_enable(info->typec_switch_chg, true);
	msleep(50);
	res = charger_dev_rust_detection_read_res(info->typec_switch_chg);
	chr_err("%s: res=%d\n", __func__, res);
	if (res == true) {
		chr_err("typec is detected lpd\n");
		info->lpd_flag = true;
		mtk_charger_fw_notifier_call_chain(CHG_FW_EVT_LPD, &info->lpd_flag);
	} else if (res < 0) {
		chr_err("typec is detected error\n");
	} else {
		info->lpd_flag = false;
		chr_err("typec is not detected lpd\n");
		mtk_charger_fw_notifier_call_chain(CHG_FW_EVT_LPD, &info->lpd_flag);
	}
	xm_charge_uevent_report(CHG_UEVENT_LPD_DETECTION, info->lpd_flag);
	if (info->lpd_charging_limit) {
		vote(info->fcc_votable, LPD_DECTEED_VOTER, true, 1500);
		vote(info->icl_votable, LPD_DECTEED_VOTER, true, 1500);
		if (info->real_type == XMUSB350_TYPE_HVCHG) {
			vbus = get_vbus(info);
			if (vbus > 7200) {
				charger_dev_set_dpdm_voltage(info->chg1_dev, 0, 0);
				charger_dev_set_mivr(info->chg1_dev, info->data.min_charger_voltage);
				chr_err("limit:%d hvdcp vbus fall 5v\n", info->lpd_charging_limit);
			}
		}
	} else {
		vote(info->fcc_votable, LPD_DECTEED_VOTER, false, 0);
		vote(info->icl_votable, LPD_DECTEED_VOTER, false, 0);
	}
out:
	schedule_delayed_work(&info->rust_detection_work, msecs_to_jiffies(rust_det_interval));
}
#endif

static int mtk_charger_probe(struct platform_device *pdev)
{
	struct mtk_charger *info = NULL;
	//struct mtk_battery *battery_drvdata;
	int i;
	int ret = 0;
	char *name = NULL;

	chr_err("%s: starts\n", __func__);

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	pinfo = info;
	platform_set_drvdata(pdev, info);
	info->pdev = pdev;
	info->night_charging = false;
	info->diff_fv_val = 0;
	info->set_smart_batt_diff_fv = 0;
	info->ov_check_only_once = 0;
	info->pmic_comp_v = 0;
	info->div_jeita_fcc_flag = false;
	info->plugged_status = false;
	info->cp_sm_run_state = false;
	info->pd30_source = false;
	info->hvdcp_setp_down = false;
	info->cp_master_ok = 0;

	/*
	if (!info->bat_psy) {
		info->smart_chg = devm_kzalloc(&pdev->dev, sizeof(info->smart_chg)*(SMART_CHG_FEATURE_MAX_NUM+1), GFP_KERNEL);
		chr_err("%s No bat_psy!\n", __func__);
	} else if (!info->smart_chg){
		battery_drvdata = power_supply_get_drvdata(info->bat_psy);
		info->smart_chg = battery_drvdata->smart_chg;
		chr_err("[XMCHG_MONITOR] set mtk_charger smart_chg done!\n");
	} else
		chr_err("%s smart_chg already has value!\n", __func__);
	*/

	//smart_chg TBD
	//info->smart_chg = devm_kzalloc(&pdev->dev, sizeof(info->smart_chg)*(15+1), GFP_KERNEL);

	mtk_charger_parse_dt(info, &pdev->dev);
	ret = mtk_charger_init_chgdev(info);
	if (ret < 0) {
		chr_err("failed to init chgdev\n");
	}

	mutex_init(&info->cable_out_lock);
	mutex_init(&info->charger_lock);
	mutex_init(&info->pd_lock);
	for (i = 0; i < CHG2_SETTING + 1; i++) {
		mutex_init(&info->pp_lock[i]);
		info->force_disable_pp[i] = false;
		info->enable_pp[i] = true;
	}
	name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "%s",
		"reverse_charge suspend wakelock");
	info->reverse_charge_wakelock =
		wakeup_source_register(NULL, name);
	name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "%s",
		"charger suspend wakelock");
	info->charger_wakelock =
		wakeup_source_register(NULL, name);
	spin_lock_init(&info->slock);

	info->last_typec_temp = -1000;
	INIT_DELAYED_WORK(&info->typec_burn_monitor_work, monitor_typec_burn);
	info->typec_burn_timer.expires = jiffies + msecs_to_jiffies(1000);
	timer_setup(&info->typec_burn_timer, monitor_typec_burn_policy, 0);

	INIT_DELAYED_WORK(&info->start_vbus_check_work, start_vbus_check);

	init_waitqueue_head(&info->wait_que);
	info->polling_interval = CHARGING_INTERVAL;
	mtk_charger_init_timer(info);
#ifdef CONFIG_PM
	if (register_pm_notifier(&info->pm_notifier)) {
		chr_err("%s: register pm failed\n", __func__);
		return -ENODEV;
	}
	info->pm_notifier.notifier_call = charger_pm_event;
#endif /* CONFIG_PM */
	srcu_init_notifier_head(&info->evt_nh);
	mtk_charger_setup_files(pdev);
	mtk_charger_get_atm_mode(info);

	for (i = 0; i < CHGS_SETTING_MAX; i++) {
		info->chg_data[i].thermal_charging_current_limit = -1;
		info->chg_data[i].thermal_input_current_limit = -1;
		info->chg_data[i].input_current_limit_by_aicl = -1;
	}
	info->enable_hv_charging = true;

	info->psy_desc1.name = "mtk-master-charger";
	info->psy_desc1.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_desc1.usb_types = charger_psy_usb_types;
	info->psy_desc1.num_usb_types = ARRAY_SIZE(charger_psy_usb_types);
	info->psy_desc1.properties = charger_psy_properties;
	info->psy_desc1.num_properties = ARRAY_SIZE(charger_psy_properties);
	info->psy_desc1.get_property = psy_charger_get_property;
	info->psy_desc1.set_property = psy_charger_set_property;
	info->psy_desc1.property_is_writeable =
			psy_charger_property_is_writeable;
	info->psy_desc1.external_power_changed =
		mtk_charger_external_power_changed;
	info->psy_cfg1.drv_data = info;
	info->psy_cfg1.supplied_to = mtk_charger_supplied_to;
	info->psy_cfg1.num_supplicants = ARRAY_SIZE(mtk_charger_supplied_to);
	info->psy1 = power_supply_register(&pdev->dev, &info->psy_desc1,
			&info->psy_cfg1);

	info->chg_psy = power_supply_get_by_name("primary_chg");
	if (IS_ERR_OR_NULL(info->chg_psy))
		chr_err("%s: devm power fail to get chg_psy\n", __func__);

	info->bc12_psy = power_supply_get_by_name("primary_chg");
	if (IS_ERR_OR_NULL(info->bc12_psy))
		chr_err("%s: devm power fail to get bc12_psy\n", __func__);

	/* bq28z610 gauge register batter/bms psy, force get battery psy */
	// info->bat_psy = devm_power_supply_get_by_phandle(&pdev->dev,
	// 	"gauge");
	// if (IS_ERR_OR_NULL(info->bat_psy))
	// 	chr_err("%s: devm power fail to get bat_psy\n", __func__);

	info->bat_psy = power_supply_get_by_name("battery");
	if (IS_ERR_OR_NULL(info->bat_psy))
		chr_err("%s: devm power fail to get bat_psy\n", __func__);

	if (IS_ERR(info->psy1))
		chr_err("register psy1 fail:%ld\n",
			PTR_ERR(info->psy1));

	info->psy_desc2.name = "mtk-slave-charger";
	info->psy_desc2.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_desc2.usb_types = charger_psy_usb_types;
	info->psy_desc2.num_usb_types = ARRAY_SIZE(charger_psy_usb_types);
	info->psy_desc2.properties = charger_psy_properties;
	info->psy_desc2.num_properties = ARRAY_SIZE(charger_psy_properties);
	info->psy_desc2.get_property = psy_charger_get_property;
	info->psy_desc2.set_property = psy_charger_set_property;
	info->psy_desc2.property_is_writeable =
			psy_charger_property_is_writeable;
	info->psy_cfg2.drv_data = info;
	info->psy2 = power_supply_register(&pdev->dev, &info->psy_desc2,
			&info->psy_cfg2);

	if (IS_ERR(info->psy2))
		chr_err("register psy2 fail:%ld\n",
			PTR_ERR(info->psy2));

	info->psy_dvchg_desc1.name = "mtk-mst-div-chg";
	info->psy_dvchg_desc1.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_dvchg_desc1.usb_types = charger_psy_usb_types;
	info->psy_dvchg_desc1.num_usb_types = ARRAY_SIZE(charger_psy_usb_types);
	info->psy_dvchg_desc1.properties = charger_psy_properties;
	info->psy_dvchg_desc1.num_properties =
		ARRAY_SIZE(charger_psy_properties);
	info->psy_dvchg_desc1.get_property = psy_charger_get_property;
	info->psy_dvchg_desc1.set_property = psy_charger_set_property;
	info->psy_dvchg_desc1.property_is_writeable =
		psy_charger_property_is_writeable;
	info->psy_dvchg_cfg1.drv_data = info;
	info->psy_dvchg1 = power_supply_register(&pdev->dev,
						 &info->psy_dvchg_desc1,
						 &info->psy_dvchg_cfg1);
	if (IS_ERR(info->psy_dvchg1))
		chr_err("register psy dvchg1 fail:%ld\n",
			PTR_ERR(info->psy_dvchg1));

	info->psy_dvchg_desc2.name = "mtk-slv-div-chg";
	info->psy_dvchg_desc2.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_dvchg_desc2.usb_types = charger_psy_usb_types;
	info->psy_dvchg_desc2.num_usb_types = ARRAY_SIZE(charger_psy_usb_types);
	info->psy_dvchg_desc2.properties = charger_psy_properties;
	info->psy_dvchg_desc2.num_properties =
		ARRAY_SIZE(charger_psy_properties);
	info->psy_dvchg_desc2.get_property = psy_charger_get_property;
	info->psy_dvchg_desc2.set_property = psy_charger_set_property;
	info->psy_dvchg_desc2.property_is_writeable =
		psy_charger_property_is_writeable;
	info->psy_dvchg_cfg2.drv_data = info;
	info->psy_dvchg2 = power_supply_register(&pdev->dev,
						 &info->psy_dvchg_desc2,
						 &info->psy_dvchg_cfg2);
	if (IS_ERR(info->psy_dvchg2))
		chr_err("register psy dvchg2 fail:%ld\n",
			PTR_ERR(info->psy_dvchg2));

	info->psy_hvdvchg_desc1.name = "mtk-mst-hvdiv-chg";
	info->psy_hvdvchg_desc1.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_hvdvchg_desc1.usb_types = charger_psy_usb_types;
	info->psy_hvdvchg_desc1.num_usb_types = ARRAY_SIZE(charger_psy_usb_types);
	info->psy_hvdvchg_desc1.properties = charger_psy_properties;
	info->psy_hvdvchg_desc1.num_properties =
					     ARRAY_SIZE(charger_psy_properties);
	info->psy_hvdvchg_desc1.get_property = psy_charger_get_property;
	info->psy_hvdvchg_desc1.set_property = psy_charger_set_property;
	info->psy_hvdvchg_desc1.property_is_writeable =
					      psy_charger_property_is_writeable;
	info->psy_hvdvchg_cfg1.drv_data = info;
	info->psy_hvdvchg1 = power_supply_register(&pdev->dev,
						   &info->psy_hvdvchg_desc1,
						   &info->psy_hvdvchg_cfg1);
	if (IS_ERR(info->psy_hvdvchg1))
		chr_err("register psy hvdvchg1 fail:%ld\n",
					PTR_ERR(info->psy_hvdvchg1));

	info->psy_hvdvchg_desc2.name = "mtk-slv-hvdiv-chg";
	info->psy_hvdvchg_desc2.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_hvdvchg_desc2.usb_types = charger_psy_usb_types;
	info->psy_hvdvchg_desc2.num_usb_types = ARRAY_SIZE(charger_psy_usb_types);
	info->psy_hvdvchg_desc2.properties = charger_psy_properties;
	info->psy_hvdvchg_desc2.num_properties =
					     ARRAY_SIZE(charger_psy_properties);
	info->psy_hvdvchg_desc2.get_property = psy_charger_get_property;
	info->psy_hvdvchg_desc2.set_property = psy_charger_set_property;
	info->psy_hvdvchg_desc2.property_is_writeable =
					      psy_charger_property_is_writeable;
	info->psy_hvdvchg_cfg2.drv_data = info;
	info->psy_hvdvchg2 = power_supply_register(&pdev->dev,
						   &info->psy_hvdvchg_desc2,
						   &info->psy_hvdvchg_cfg2);
	if (IS_ERR(info->psy_hvdvchg2))
		chr_err("register psy hvdvchg2 fail:%ld\n",
					PTR_ERR(info->psy_hvdvchg2));

	info->usb_desc.name = "usb";
	info->usb_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->usb_desc.properties = mt_usb_properties;
	info->usb_desc.num_properties = ARRAY_SIZE(mt_usb_properties);
	info->usb_desc.get_property = mt_usb_get_property;
	info->usb_desc.external_power_changed = mtk_charger_external_power_usb_changed;
	info->usb_cfg.supplied_to = mtk_usb_supplied_to;
	info->usb_cfg.num_supplicants = ARRAY_SIZE(mtk_usb_supplied_to);
	info->usb_cfg.drv_data = info;

	info->usb_psy = power_supply_register(&pdev->dev,
		&info->usb_desc, &info->usb_cfg);
	if (IS_ERR(info->usb_psy))
		chr_err("register psy usb fail:%ld\n",
			PTR_ERR(info->usb_psy));
	else
        usb_sysfs_create_group(info->usb_psy);

	info->log_level = CHRLOG_INFO_LEVEL;

	info->pd_adapter = get_adapter_by_name("pd_adapter");
	if (!info->pd_adapter)
		chr_err("%s: No pd adapter found\n", __func__);
	else {
		info->pd_nb.notifier_call = notify_adapter_event;
		register_adapter_device_notifier(info->pd_adapter,
						 &info->pd_nb);
	}

	info->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (!info->tcpc)
		chr_err("get tcpc dev failed\n");
	else {
		info->tcpc_nb.notifier_call = mtk_charger_tcpc_notifier_call;
		register_tcp_dev_notifier(info->tcpc,
						 &info->tcpc_nb, TCP_NOTIFY_TYPE_ALL);
		chr_err("register tcpc_nb ok\n");
	}

	info->cp_master = get_charger_by_name("cp_master");
	if (!info->cp_master)
		chr_err("get cp master failed\n");
	else {
		charger_dev_cp_chip_ok(info->cp_master, &info->cp_master_ok);
		chr_info("%s cp master chip ok = %d\n", __func__, info->cp_master_ok);
	}

	info->disp_nb.notifier_call = screen_state_for_charger_callback;
	ret = mi_disp_register_client(&info->disp_nb);
	if (ret < 0) {
		chr_err("%s register screen state callback failed\n",__func__);
	}

	info->audio_nb.notifier_call = audio_state_for_charger_callback;
	ret = audio_status_notifier_register_client(&info->audio_nb);
	if (ret < 0) {
		chr_err("%s register audio state callback failed\n",__func__);
	}

	alarm_init(&info->otg_ui_close_timer, ALARM_BOOTTIME, otg_ui_close_timer_handler);
	alarm_init(&info->set_soft_cid_timer, ALARM_BOOTTIME, set_soft_cid_timer_handler);

	INIT_DELAYED_WORK(&info->handle_cc_status_work, handle_cc_status_work_func);
	INIT_DELAYED_WORK(&info->en_floatgnd_work, en_floating_ground_work_func);
	INIT_DELAYED_WORK(&info->dis_floatgnd_work, dis_floating_ground_work_func);
	info->cid_status = false;
	INIT_DELAYED_WORK(&info->check_revchg_status_work, check_revchg_status_workfunc);
	INIT_DELAYED_WORK(&info->delay_disable_otg_work, delay_disable_otg_workfunc);
	INIT_DELAYED_WORK(&info->handle_reverse_charge_event_work, handle_reverse_charge_workfunc);
	INIT_DELAYED_WORK(&info->otg_state_check_work, otg_state_check_work);

	#if IS_ENABLED(CONFIG_RUST_DETECTION)
	INIT_DELAYED_WORK(&info->rust_detection_work, rust_detection_work_func);
	#endif

	sc_init(&info->sc);
	info->chg_alg_nb.notifier_call = chg_alg_event;
	info->thermal_nb.notifier_call = charger_thermal_notifier_call;
	charger_reg_notifier(&info->thermal_nb);

	info->fast_charging_indicator = 0;
	info->enable_meta_current_limit = 1;
	info->is_charging = false;
	info->pd_verifying = true;
	info->night_charge_enable = false;
	info->smart_soclmt_trig = false;
	info->safety_timer_cmd = -1;
	info->cmd_pp = -1;
	info->adapter_imax = -1;

	info->mt6369_regmap = pmic_get_regmap("second_pmic");
	if(IS_ERR(info->mt6369_regmap) || !info->mt6369_regmap)
		chr_err("%s: mt6369 regmap not found!\n", __func__);

	/* 8 = KERNEL_POWER_OFF_CHARGING_BOOT */
	/* 9 = LOW_POWER_OFF_CHARGING_BOOT */
	if (info != NULL && info->bootmode != 8 && info->bootmode != 9)
		mtk_charger_force_disable_power_path(info, CHG1_SETTING, true);

	info->bat_psy = power_supply_get_by_name("battery");
	if (info->bat_psy) {
		ret = step_jeita_init(info, &info->pdev->dev);
		if (ret < 0) {
			chr_err("failed to register step_jeita charge\n");
			info->jeita_support = false;
		} else
			info->jeita_support = true;

		#if IS_ENABLED(CONFIG_XM_SMART_CHG)
		ret = xm_smart_chg_init(info);
		if (ret < 0) {
			chr_err("xm smart charge feature init failed, ret = %d\n", ret);
		}
		#endif

		#if IS_ENABLED(CONFIG_XM_BATTERY_HEALTH)
		ret = xm_batt_health_init(info);
		if (ret < 0) {
			chr_err("xm battery health feature init failed, ret = %d\n", ret);
		}
		#endif
	} else {
		chr_err("%s: fail to get battery psy\n", __func__);
		INIT_DELAYED_WORK(&info->jeita_init_work, jeita_init_workfunc);
		schedule_delayed_work(&info->jeita_init_work, msecs_to_jiffies(1000));
	}

	kthread_run(charger_routine_thread, info, "charger_thread");

	return 0;
}

static int mtk_charger_remove(struct platform_device *dev)
{
	struct mtk_charger *info = platform_get_drvdata(dev);

#if IS_ENABLED(CONFIG_RUST_DETECTION)
	cancel_delayed_work_sync(&info->rust_detection_work);
#endif

#if IS_ENABLED(CONFIG_XM_SMART_CHG)
	xm_smart_chg_deinit(info);
#endif
#if IS_ENABLED(CONFIG_XM_BATTERY_HEALTH)
	xm_batt_health_deinit(info);
#endif
	//rember to check
	audio_status_notifier_unregister_client(&info->audio_nb);
	mi_disp_unregister_client(&info->disp_nb);

	return 0;
}

static void mtk_charger_shutdown(struct platform_device *dev)
{
	struct mtk_charger *info = platform_get_drvdata(dev);
	int i;

#if IS_ENABLED(CONFIG_RUST_DETECTION)
	cancel_delayed_work_sync(&info->rust_detection_work);
#endif
	for (i = 0; i < MAX_ALG_NO; i++) {
		if (info->alg[i] == NULL)
			continue;
		chg_alg_stop_algo(info->alg[i]);
	}
}

static const struct of_device_id mtk_charger_of_match[] = {
	{.compatible = "mediatek,charger",},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_charger_of_match);

struct platform_device mtk_charger_device = {
	.name = "charger",
	.id = -1,
};

static struct platform_driver mtk_charger_driver = {
	.probe = mtk_charger_probe,
	.remove = mtk_charger_remove,
	.shutdown = mtk_charger_shutdown,
	.driver = {
		   .name = "charger",
		   .of_match_table = mtk_charger_of_match,
	},
};

static int __init mtk_charger_init(void)
{
	return platform_driver_register(&mtk_charger_driver);
}
module_init(mtk_charger_init);

static void __exit mtk_charger_exit(void)
{
	platform_driver_unregister(&mtk_charger_driver);
}
module_exit(mtk_charger_exit);


MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK Charger Driver");
MODULE_LICENSE("GPL");
