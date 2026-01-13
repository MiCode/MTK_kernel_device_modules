// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2025 Huaqin Technology(Shanghai) Co., Ltd.
 */

#include <linux/string.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/stddef.h>
#include <linux/kthread.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/time.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/mutex.h>

#include "xm_chg_uevent.h"

#define TAG                     "[HQ_CHG_UEVENT]" // [VENDOR_MODULE_SUBMODULE]
#define xm_err(fmt, ...)        pr_err(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)
#define xm_warn(fmt, ...)       pr_warn(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)
#define xm_notice(fmt, ...)     pr_notice(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)
#define xm_info(fmt, ...)       pr_info(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)
#define xm_debug(fmt, ...)      pr_debug(TAG "[%s]:" fmt, __func__, ##__VA_ARGS__)

#define CHG_UEVENT_MAX_LENGHT (64)

struct xm_chg_uevent {
	struct platform_device *pdev;
	struct mutex report_lock;
	/* TODO: add uevent report record, e.g. timestamps, count... */
};

static struct xm_chg_uevent *g_chg_uevent;

static char *xm_chg_uevent_prefix_str[] = {
	[CHG_UEVENT_DEFAULT_TYPE]         = "CHG_UEVENT_DEFAULT_TYPE",
	[CHG_UEVENT_SOC_DECIMAL]          = "POWER_SUPPLY_SOC_DECIMAL=",
	[CHG_UEVENT_SOC_DECIMAL_RATE]     = "POWER_SUPPLY_SOC_DECIMAL_RATE=",
	[CHG_UEVENT_QUICK_CHARGE_TYPE]    = "POWER_SUPPLY_QUICK_CHARGE_TYPE=",
	[CHG_UEVENT_SHUTDOWN_DELAY]       = "POWER_SUPPLY_SHUTDOWN_DELAY=",
	[CHG_UEVENT_CONNECTOR_TEMP]       = "POWER_SUPPLY_CONNECTOR_TEMP=",
	[CHG_UEVENT_NTC_ALARM]            = "POWER_SUPPLY_NTC_ALARM=",
	[CHG_UEVENT_LPD_DETECTION]        = "POWER_SUPPLY_MOISTURE_DET_STS=",
	[CHG_UEVENT_REVERSE_QUICK_CHARGE] = "POWER_SUPPLY_REVERSE_QUICK_CHARGE=",
	[CHG_UEVENT_CC_SHORT_VBUS]        = "POWER_SUPPLY_CC_SHORT_VBUS",
};

static int do_charge_uevent_report(struct xm_chg_uevent *chg_uevent, int event_type, int event_value);


/**
 * xm_charge_uevent_report() - Report Xiaomi charge specific uevent to userspace
 * @event_type: Specific uevent type, reference to enum xm_chg_uevent_type
 * @event_value: Payload of uevent with integer type
 *
 * Return: On success return zero, negative integer otherwise
 */
int xm_charge_uevent_report(int event_type, int event_value)
{
	int ret = 0;

	if (!g_chg_uevent)
		return -EFAULT;

	ret = do_charge_uevent_report(g_chg_uevent, event_type, event_value);
	if (ret != 0) {
		xm_err("fail to report charge uevent\n");
	}

	return ret;
}
EXPORT_SYMBOL_GPL(xm_charge_uevent_report);

/**
 * xm_charge_uevents_bundle_report() - Report Xiaomi charge specific uevents bundle to userspace
 * @bundle_type: uevent bundle type, contains several uevents to report together
 *
 * Return: On success return zero, negative integer otherwise
 */
int xm_charge_uevents_bundle_report(int bundle_type, ...)
{
	int ret = 0;
	va_list va;
	int intval = 0;
	char *envp[MAX_BUNDLE_UEVENT_CNT + 1] = {NULL};
	char uevent_str[MAX_BUNDLE_UEVENT_CNT][CHG_UEVENT_MAX_LENGHT] = {0};
	int bundle_uevent_cnt = 0;
	int i = 0;

	if (!g_chg_uevent)
		return -EFAULT;

	mutex_lock(&g_chg_uevent->report_lock);

	va_start(va, bundle_type);

	switch (bundle_type) {
	case CHG_UEVENT_BUNDLE_CHG_ANIMATION:
		intval = va_arg(va, int);
		snprintf(uevent_str[0], CHG_UEVENT_MAX_LENGHT, "%s%d",
			xm_chg_uevent_prefix_str[CHG_UEVENT_QUICK_CHARGE_TYPE], intval);
		envp[0] = uevent_str[0]; /* quick charge type */
		bundle_uevent_cnt++;

		intval = va_arg(va, int);
		snprintf(uevent_str[1], CHG_UEVENT_MAX_LENGHT, "%s%d",
			xm_chg_uevent_prefix_str[CHG_UEVENT_SOC_DECIMAL], intval);
		envp[1] = uevent_str[1]; /* soc decimal */
		bundle_uevent_cnt++;

		intval = va_arg(va, int);
		snprintf(uevent_str[2], CHG_UEVENT_MAX_LENGHT, "%s%d",
			xm_chg_uevent_prefix_str[CHG_UEVENT_SOC_DECIMAL_RATE], intval);
		envp[2] = uevent_str[2];  /* soc devimal rate */
		bundle_uevent_cnt++;

		envp[3] = NULL; /* end of uevent */

		xm_info("bundle type %d contains %d uevents\n", bundle_type, bundle_uevent_cnt);
		break;

	default:
		xm_info("bundle type %d not support\n", bundle_type);
		va_end(va);
		mutex_unlock(&g_chg_uevent->report_lock);
		return -EINVAL;
	}

	va_end(va);

	kobject_uevent_env(&g_chg_uevent->pdev->dev.kobj, KOBJ_CHANGE, envp);

	for (i = 0; envp[i] != NULL; i++) {
		xm_info("envp[%d] = %s\n", i, envp[i]);
	}

	mutex_unlock(&g_chg_uevent->report_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(xm_charge_uevents_bundle_report);

/* TODO: change "int event_value" to union type to support more payload type */
static int do_charge_uevent_report(struct xm_chg_uevent *chg_uevent, int event_type, int event_value)
{
	char uevent_str[CHG_UEVENT_MAX_LENGHT] = {0};
	char *envp[2] = {
		uevent_str,
		NULL,
	};

	if (!chg_uevent) {
		return -EFAULT;
	}

	mutex_lock(&chg_uevent->report_lock);

	switch (event_type) {
	/* with out payload */
	case CHG_UEVENT_DEFAULT_TYPE:
		snprintf(uevent_str, sizeof(uevent_str), "%s",
			xm_chg_uevent_prefix_str[event_type]);
		break;

	/* payload with integer type */
	case CHG_UEVENT_SOC_DECIMAL:
	case CHG_UEVENT_SOC_DECIMAL_RATE:
	case CHG_UEVENT_QUICK_CHARGE_TYPE:
	case CHG_UEVENT_SHUTDOWN_DELAY:
	case CHG_UEVENT_CONNECTOR_TEMP:
	case CHG_UEVENT_NTC_ALARM:
	case CHG_UEVENT_LPD_DETECTION:
	case CHG_UEVENT_REVERSE_QUICK_CHARGE:
		snprintf(uevent_str, sizeof(uevent_str), "%s%d",
			xm_chg_uevent_prefix_str[event_type], event_value);
		break;
	case CHG_UEVENT_CC_SHORT_VBUS:
		snprintf(uevent_str, sizeof(uevent_str), "%s",
			xm_chg_uevent_prefix_str[event_type]);
		break;

	default:
		xm_info("event_type %d not support\n", event_type);
		mutex_unlock(&chg_uevent->report_lock);
		return -EINVAL;
	}

	kobject_uevent_env(&chg_uevent->pdev->dev.kobj, KOBJ_CHANGE, envp);

	xm_info("event_type:%d, event_value:%d, uevent_str:%s\n",
		event_type, event_value, uevent_str);

	mutex_unlock(&chg_uevent->report_lock);

	return 0;
}

static int xm_chg_uevent_probe(struct platform_device *pdev)
{
	struct xm_chg_uevent *chg_uevent = NULL;

	chg_uevent = devm_kzalloc(&pdev->dev, sizeof(*chg_uevent), GFP_KERNEL);
	if (!chg_uevent)
		return -ENOMEM;

	platform_set_drvdata(pdev, chg_uevent);
	chg_uevent->pdev = pdev;

	mutex_init(&chg_uevent->report_lock);

	g_chg_uevent = chg_uevent;

	xm_info("success...\n");

	return 0;
}

static int xm_chg_uevent_remove(struct platform_device *pdev)
{
	g_chg_uevent = NULL;

	return 0;
}

static const struct of_device_id xm_chg_uevent_of_match[] = {
	{ .compatible = "xiaomi,xm-chg-uevent", },
	{ },
};
MODULE_DEVICE_TABLE(of, xm_chg_uevent_of_match);

static struct platform_driver xm_chg_uevent_driver = {
	.probe = xm_chg_uevent_probe,
	.remove = xm_chg_uevent_remove,
	.driver = {
		.name = "xm_chg_uevent",
		.of_match_table = of_match_ptr(xm_chg_uevent_of_match),
	},
};

static int __init xm_chg_uevent_init(void)
{
	return platform_driver_register(&xm_chg_uevent_driver);
}
module_init(xm_chg_uevent_init);

static void __exit xm_chg_uevent_exit(void)
{
	return platform_driver_unregister(&xm_chg_uevent_driver);
}
module_exit(xm_chg_uevent_exit);

MODULE_DESCRIPTION("Xiaomi Charge Uevent Report Driver");
MODULE_AUTHOR("pengyuzhe@huaqin.com");
MODULE_LICENSE("GPL v2");
