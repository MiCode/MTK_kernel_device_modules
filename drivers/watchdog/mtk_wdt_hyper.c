// SPDX-License-Identifier: GPL-2.0+
/*
 * Mediatek Watchdog Driver
 *
 * Copyright (C) 2025 MediaTek Inc
 *
 * Xiwen Shao <xiwen.shao@mediatek.com>
 *
 * Based on mtk_wdt.c
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/watchdog.h>
#include "mtk_wdt_hyper.h"

#define DRV_NAME		"mtk-wdt-hyper"
#define DRV_VERSION		"1.0"

#define RETRY_KICK_SEC 5L

void __iomem *toprgu_base;
static bool nowayout = WATCHDOG_NOWAYOUT;
static unsigned int timeout;
struct timer_list retry_kick_timer;

__weak int mtk_wdt_hyper_kick(void) { return 0; }
__weak void mtk_wdt_hyper_suspend(void) { return; }
__weak void mtk_wdt_hyper_resume(void) { return; }

void mtk_wdt_set_sw_rst_status(void)
{
	u32 reg;

	if (!toprgu_base) {
		pr_info("%s: get toprgu base failed\n", __func__);
		return;
	}

	reg = ioread32(toprgu_base + WDT_STATUS);
	reg |= WDT_STATUS_SWWDT_RST;
	iowrite32(reg, toprgu_base + WDT_NONRST_REG);

	reg = ioread32(toprgu_base + WDT_NONRST_REG);
	pr_info("%s: nonrst reg = 0x%x\n", __func__, reg);
}
EXPORT_SYMBOL(mtk_wdt_set_sw_rst_status);

static void retry_kick_timer_func(struct timer_list *t)
{
	mtk_wdt_hyper_kick();
}

static int mtk_wdt_ping(struct watchdog_device *wdt_dev)
{
	int ret;

	ret = mtk_wdt_hyper_kick();
	if (ret && (!timer_pending(&retry_kick_timer))) {
		retry_kick_timer.expires = jiffies + RETRY_KICK_SEC * HZ;
		add_timer(&retry_kick_timer);
	}

	return 0;
}

static int mtk_wdt_set_timeout(struct watchdog_device *wdt_dev,
				unsigned int timeout)
{
	return 0;
}

static void mtk_wdt_init(struct watchdog_device *wdt_dev)
{
	set_bit(WDOG_HW_RUNNING, &wdt_dev->status);
}

static int mtk_wdt_stop(struct watchdog_device *wdt_dev)
{
	clear_bit(WDOG_HW_RUNNING, &wdt_dev->status);

	return 0;
}

static int mtk_wdt_start(struct watchdog_device *wdt_dev)
{
	set_bit(WDOG_HW_RUNNING, &wdt_dev->status);

	return 0;
}

static const struct watchdog_info mtk_wdt_info = {
	.identity	= DRV_NAME,
	.options	= WDIOF_SETTIMEOUT |
			  WDIOF_KEEPALIVEPING |
			  WDIOF_MAGICCLOSE,
};


static const struct watchdog_ops mtk_wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= mtk_wdt_start,
	.stop		= mtk_wdt_stop,
	.ping		= mtk_wdt_ping,
	.set_timeout	= mtk_wdt_set_timeout,
};

static int mtk_wdt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_wdt_dev *mtk_wdt;
	int err;

	mtk_wdt = devm_kzalloc(dev, sizeof(*mtk_wdt), GFP_KERNEL);
	if (!mtk_wdt)
		return -ENOMEM;

	platform_set_drvdata(pdev, mtk_wdt);

	mtk_wdt->wdt_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(mtk_wdt->wdt_base))
		return PTR_ERR(mtk_wdt->wdt_base);

	toprgu_base = mtk_wdt->wdt_base;

	mtk_wdt->wdt_dev.info = &mtk_wdt_info;

	mtk_wdt->wdt_dev.ops = &mtk_wdt_ops;
	mtk_wdt->wdt_dev.timeout = WDT_MAX_TIMEOUT;
	mtk_wdt->wdt_dev.max_hw_heartbeat_ms = WDT_MAX_TIMEOUT * 1000;
	mtk_wdt->wdt_dev.min_timeout = WDT_MIN_TIMEOUT;
	mtk_wdt->wdt_dev.parent = dev;

	watchdog_init_timeout(&mtk_wdt->wdt_dev, timeout, dev);
	watchdog_set_nowayout(&mtk_wdt->wdt_dev, nowayout);

	watchdog_set_drvdata(&mtk_wdt->wdt_dev, mtk_wdt);

	mtk_wdt_init(&mtk_wdt->wdt_dev);

	timer_setup(&retry_kick_timer, retry_kick_timer_func, 0);

	err = devm_watchdog_register_device(dev, &mtk_wdt->wdt_dev);
	if (unlikely(err))
		return err;

	dev_info(dev, "Watchdog enabled (timeout=%d sec, nowayout=%d)\n",
		 mtk_wdt->wdt_dev.timeout, nowayout);

	return 0;
}

static int mtk_wdt_suspend(struct device *dev)
{
	mtk_wdt_hyper_suspend();
	return 0;
}

static int mtk_wdt_resume(struct device *dev)
{
	mtk_wdt_hyper_resume();
	return 0;
}

static const struct dev_pm_ops mtk_wdt_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_wdt_suspend,
				mtk_wdt_resume)
};

static const struct of_device_id mtk_wdt_dt_ids[] = {
	{ .compatible = "mediatek,mt6589-wdt" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mtk_wdt_dt_ids);

static struct platform_driver mtk_wdt_driver = {
	.probe		= mtk_wdt_probe,
	.driver		= {
		.name		= DRV_NAME,
		.pm		= &mtk_wdt_pm_ops,
		.of_match_table	= mtk_wdt_dt_ids,
	},
};

module_platform_driver(mtk_wdt_driver);

module_param(timeout, uint, 0);
MODULE_PARM_DESC(timeout, "Watchdog heartbeat in seconds");

module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default="
			__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xiwen Shao <xiwen.shao@mediatek.com>");
MODULE_DESCRIPTION("Mediatek Virtual WatchDog Timer Driver");
MODULE_VERSION(DRV_VERSION);
