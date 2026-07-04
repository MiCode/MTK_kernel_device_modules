// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2022 - 2023 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <linux/gpio.h>

#include "subpmic_irq.h"
#include "subpmic.h"
#include "../../../../../misc/mediatek/typec/sc_tcpc/inc/tcpci_typec.h"
#include "../../../../../misc/mediatek/typec/sc_tcpc/inc/tcpm.h"
#include <linux/hrtimer.h>
#define SUBPMIC_CID_VERSION             "0.0.1"

struct subpmic_cid_device {
    struct i2c_client *client;
    struct device *dev;
    struct regmap *rmap;

    struct work_struct cid_work;
    atomic_t cid_pending;
    // pd contrl
    struct tcpc_device *tcpc;
    int device_id;
    //bool otg_switch;
    bool typec_attach;
    struct notifier_block tcpc_rust_det_nb;
    struct delayed_work hrtime_otg_work;
    struct alarm rust_det_work_timer;
    struct delayed_work set_cc_drp_work;
    bool ui_cc_toggle;
    bool cid_status;
};

struct subpmic_cid_device *g_cid_device;
#define SUBPMIC_CID_FLAG            BIT(7)

void manual_set_cc_toggle(bool en)
{
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;
    struct subpmic_cid_device *sc = g_cid_device;

	if(sc == NULL)
		return;

	sc->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if(sc->tcpc == NULL)
		return;

    sc->ui_cc_toggle = en;
	dev_info(sc->dev,"%s,en:%d\n", __func__,en);

	if(!sc->typec_attach && en)
	{
		pr_err("typec is not attached set cc toggle\n");
		tcpci_set_cc(sc->tcpc, TYPEC_CC_DRP);
		//sc->otg_switch = true;
	}else if(!sc->typec_attach && !en){
		pr_err("set cc not toggle\n");
		tcpci_set_cc(sc->tcpc, TYPEC_CC_RD);
		//sc->otg_switch = false;
	}else{
		pr_err("typec is attached, not set cc toggle\n");
	}
	if(en && !sc->cid_status)
	{
		ret = alarm_try_to_cancel(&sc->rust_det_work_timer);
		if (ret < 0) {
			pr_err("%s: callback was running, skip timer\n", __func__);
			return;
		}
		ktime_now = ktime_get_boottime();
		time_now = ktime_to_timespec64(ktime_now);
		end_time.tv_sec = time_now.tv_sec + 600;
		end_time.tv_nsec = time_now.tv_nsec + 0;
		ktime = ktime_set(end_time.tv_sec,end_time.tv_nsec);
		pr_err("%s: alarm timer start:%d, %lld %ld\n", __func__, ret,
			end_time.tv_sec, end_time.tv_nsec);
		alarm_start(&sc->rust_det_work_timer, ktime);
		pr_err("ui set cc toggle : start hrtimer\n");
	}else{
		ret = alarm_try_to_cancel(&sc->rust_det_work_timer);
		if (ret < 0) {
			pr_err("%s: callback was running, skip timer\n", __func__);
			return;
		}
		pr_err("ui disable cc toggle : stop hrtimer\n");
	}
	return;
}
EXPORT_SYMBOL(manual_set_cc_toggle);

void manual_get_cc_toggle(bool *cc_toggle)
{
	if(g_cid_device == NULL)
		return;
	*cc_toggle = g_cid_device->ui_cc_toggle;
	pr_err("%s\n = %d", __func__, *cc_toggle);
	return;
}
EXPORT_SYMBOL(manual_get_cc_toggle);

bool manual_get_cid_status(void)
{
	if(g_cid_device == NULL)
		return true;
	pr_err("%s\n = %d", __func__, g_cid_device->cid_status);
	return g_cid_device->cid_status;
}
EXPORT_SYMBOL(manual_get_cid_status);

static void hrtime_otg_work_func(struct work_struct *work)
{
	if(g_cid_device != NULL && g_cid_device->tcpc != NULL) {
		tcpci_set_cc(g_cid_device->tcpc, TYPEC_CC_RD);
		//g_cid_device->otg_switch = false;
        }
	pr_err("hrtime_otg_work_func enter\n");
}
static enum alarmtimer_restart rust_det_work_timer_handler(struct alarm *alarm, ktime_t now)
{
	if(g_cid_device != NULL)
	{
		g_cid_device->ui_cc_toggle = false;
		schedule_delayed_work(&g_cid_device->hrtime_otg_work, 0);
	}
	pr_err("rust_det_work_timer_handler enter\n");
    return ALARMTIMER_NORESTART;
}

static void set_cc_drp_work_func(struct work_struct *work)
{
	if(g_cid_device != NULL && g_cid_device->tcpc != NULL) {
		tcpci_set_cc(g_cid_device->tcpc, TYPEC_CC_DRP);
		//g_cid_device->otg_switch = true;
        }
	pr_err("set_cc_drp_work_func enter\n");
}

static int rust_det_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	uint8_t old_state = TYPEC_UNATTACHED, new_state = TYPEC_UNATTACHED;
	//int i = 0;
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;
	pr_err("%s: event=%lu, state=%d,%d\n", __func__,
		event, noti->typec_state.old_state, noti->typec_state.new_state);
	switch (event) {
	case TCP_NOTIFY_TYPEC_STATE:
		old_state = noti->typec_state.old_state;
		new_state = noti->typec_state.new_state;
		if (old_state == TYPEC_UNATTACHED &&
				new_state != TYPEC_UNATTACHED &&
				!g_cid_device->typec_attach) {
			pr_info("%s typec plug in, polarity = %d\n",
					__func__, noti->typec_state.polarity);
			g_cid_device->typec_attach = true;
			g_cid_device->cid_status = true;
			if(g_cid_device->ui_cc_toggle){
				ret = alarm_try_to_cancel(&g_cid_device->rust_det_work_timer);
				if (ret < 0) {
					pr_err("%s: callback was running, skip timer\n", __func__);
				}
				pr_info("typec plug in, cancel   hrtimer\n");
			}
		} else if (old_state != TYPEC_UNATTACHED &&
				new_state == TYPEC_UNATTACHED &&
				g_cid_device->typec_attach) {
			pr_info("%s typec plug out\n", __func__);
			g_cid_device->typec_attach = false;
			g_cid_device->cid_status = false;
			if(g_cid_device->ui_cc_toggle){
				if(g_cid_device->tcpc != NULL)
				{
					pr_err("typec plug out, ui  set cc toggle\n");
					// tcpci_set_cc(g_cid_device->tcpc, TYPEC_CC_DRP);
					schedule_delayed_work(&g_cid_device->set_cc_drp_work, msecs_to_jiffies(500));
				}
				ret = alarm_try_to_cancel(&g_cid_device->rust_det_work_timer);
				if (ret < 0) {
					pr_err("%s: callback was running, skip timer\n", __func__);
				}
				ktime_now = ktime_get_boottime();
				time_now = ktime_to_timespec64(ktime_now);
				end_time.tv_sec = time_now.tv_sec + 600;
				end_time.tv_nsec = time_now.tv_nsec + 0;
				ktime = ktime_set(end_time.tv_sec,end_time.tv_nsec);
				pr_err("%s: alarm timer start:%d, %lld %ld\n", __func__, ret,
						end_time.tv_sec, end_time.tv_nsec);
				alarm_start(&g_cid_device->rust_det_work_timer, ktime);
				pr_info("typec plug out, start   hrtimer\n");
			}
		}
		break;
	default:
		pr_info("%s default event\n", __func__);
	}
	return NOTIFY_OK;
}

static void cid_detect_workfunc(struct work_struct *work)
{
    struct subpmic_cid_device *sc = container_of(work,
                 struct subpmic_cid_device, cid_work);
    int ret = 0;
    u8 state = 0;
    // must get tcpc device
    dev_info(sc->dev, "%s enter!\n", __func__);
    while(sc->tcpc == NULL) {
        sc->tcpc = tcpc_dev_get_by_name("type_c_port0");
        msleep(1000);
    }

    do {
        ret = regmap_bulk_read(sc->rmap, SUBPMIC_REG_HK_GEN_STATE, &state, 1);
        if (ret) {
            dev_err(sc->dev, "failed to read cid state.\n");
            return;
        }
        if (state & SUBPMIC_CID_FLAG) {
            dev_info(sc->dev, "sc6601 cid detect plug-in\n");
            tcpm_typec_change_role(sc->tcpc, TYPEC_ROLE_TRY_SNK);
        } else {
            dev_info(sc->dev, "sc6601 cid detect plug-out\n");
            tcpm_typec_change_role(sc->tcpc, TYPEC_ROLE_SNK);
        }
        atomic_dec_if_positive(&sc->cid_pending);
    } while(atomic_read(&sc->cid_pending));
}

extern bool cid_plugin_flag;
static void sy6976_cid_set_func(bool detect_in)
{
    struct subpmic_cid_device *sc = g_cid_device;

    // must get tcpc device
    pr_err("%s enter!\n", __func__);
    while(sc->tcpc == NULL) {
        sc->tcpc = tcpc_dev_get_by_name("type_c_port0");
        msleep(1000);
    }

    do {
        if (detect_in) {
            cid_plugin_flag = true;
            dev_info(sc->dev, "sy6976 cid detect plug-in\n");
            tcpm_typec_change_role(sc->tcpc, TYPEC_ROLE_TRY_SNK);
        } else {
            cid_plugin_flag = false;
            dev_info(sc->dev, "sy6976 cid detect plug-out\n");
            tcpm_typec_change_role(sc->tcpc, TYPEC_ROLE_SNK);
        }
        atomic_dec_if_positive(&sc->cid_pending);
    } while(atomic_read(&sc->cid_pending));
    return;
}

int sy6976_subpmic_cid_detect(void)
{
    static bool old_state = false,curr_state = false;
    int ret = 0;
    u8 state = 0;
    struct subpmic_cid_device *sc;

    // must get tcpc device
    pr_err("%s enter!\n", __func__);
    if(!g_cid_device){
        pr_err("%s g_cid_device is NULL !\n", __func__);
        return -1; 
    }
    sc = g_cid_device;
 
    ret = regmap_bulk_read(sc->rmap, SUBPMIC_REG_HK_INT_STAT, &state, 1);
    if (ret) {
        dev_err(sc->dev, "%s:failed to read cid state.\n",__func__);
        return -1;
    }
    dev_err(sc->dev, "%s:state:%x.\n",__func__,state);
    curr_state = state & SUBPMIC_CID_FLAG;
    dev_err(sc->dev, "%s:curr state:%d old state:%d.\n",__func__,curr_state,old_state);
    if (curr_state != old_state) {
        atomic_inc(&sc->cid_pending);
        if (curr_state)
            sy6976_cid_set_func(1);
        else
            sy6976_cid_set_func(0);
        old_state = curr_state;
    }
    return 0;
}
EXPORT_SYMBOL(sy6976_subpmic_cid_detect);

static irqreturn_t subpmic_cid_alert_handler(int irq, void *data)
{
    struct subpmic_cid_device *sc = data;
    if(sc->device_id == 1)
    {
    	atomic_inc(&sc->cid_pending);
    	schedule_work(&sc->cid_work);    
    }
    return IRQ_HANDLED;
}

static int subpmic_cid_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct subpmic_cid_device *sc;
    int ret = 0;
    int reg_val = 0;

    dev_info(dev, "%s (%s)\n", __func__, SUBPMIC_CID_VERSION);

    sc = devm_kzalloc(dev, sizeof(*sc), GFP_KERNEL);
    if (!sc)
        return -ENOMEM;
    sc->rmap = dev_get_regmap(dev->parent, NULL);
    if (!sc->rmap) {
        dev_err(dev, "failed to get regmap\n");
        return -ENODEV;
    }
    sc->dev = dev;
    g_cid_device = sc;
    platform_set_drvdata(pdev, sc);

    INIT_WORK(&sc->cid_work, cid_detect_workfunc);
    atomic_set(&sc->cid_pending, 0);

    ret = platform_get_irq_byname(to_platform_device(sc->dev), "CID");
    if (ret < 0) {
        dev_err(sc->dev, "failed to get irq CID\n");
        return ret;
    }

    ret = devm_request_threaded_irq(sc->dev, ret, NULL,
                subpmic_cid_alert_handler, IRQF_ONESHOT,
                dev_name(sc->dev), sc);
    if (ret < 0) {
        dev_err(sc->dev, "failed to request irq CID\n");
        return ret;
    }

    ret = regmap_bulk_read(sc->rmap, SUBPMIC_REG_DEVICE_ID, &reg_val, 1);
    if (ret) {
        dev_err(sc->dev, "failed to get device_id \n");
        return ret;
    }
    if (reg_val == 0x62)
        sc->device_id = 1;
    else if (reg_val == 0x67)
        sc->device_id = 2;
    else
        sc->device_id = 0;
        
    if(sc->device_id == 1)
        schedule_work(&sc->cid_work);

    alarm_init(&sc->rust_det_work_timer, ALARM_BOOTTIME,rust_det_work_timer_handler);
    INIT_DELAYED_WORK(&sc->hrtime_otg_work, hrtime_otg_work_func);
    INIT_DELAYED_WORK(&sc->set_cc_drp_work, set_cc_drp_work_func);
    sc->cid_status = false;

    sc->tcpc = tcpc_dev_get_by_name("type_c_port0");
    if(sc->tcpc)
    {
        sc->tcpc_rust_det_nb.notifier_call = rust_det_notifier_call;
        register_tcp_dev_notifier(sc->tcpc,
                    &sc->tcpc_rust_det_nb, TCP_NOTIFY_TYPE_ALL);
        pr_err("register tcpc_rust_det_nb ok\n");
    }
    dev_info(dev, "%s probe success\n", __func__);

    return 0;
}

static int subpmic_cid_remove(struct platform_device *pdev)
{
    return 0;
}

static void subpmic_cid_shutdown(struct platform_device *pdev)
{

}

static const struct of_device_id subpmic_cid_of_match[] = {
    {.compatible = "southchip,subpmic_cid",},
    {.compatible = "sy6976,subpmic_cid",},
    {},
};

static struct platform_driver subpmic_cid_driver = {
    .driver = {
        .name = "subpmic_cid",
        .of_match_table = of_match_ptr(subpmic_cid_of_match),
    },
    .probe = subpmic_cid_probe,
    .remove = subpmic_cid_remove,
    .shutdown = subpmic_cid_shutdown,
};

module_platform_driver(subpmic_cid_driver);

MODULE_AUTHOR("Boqiang Liu <air-liu@southchip.com>");
MODULE_DESCRIPTION("sc6601A CID driver");
MODULE_LICENSE("GPL v2");