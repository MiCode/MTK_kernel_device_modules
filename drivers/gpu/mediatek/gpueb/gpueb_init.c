// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

/**
 * @file    mtk_common_init.c
 * @brief   GPUEB driver init and probe
 */

/**
 * ===============================================
 * SECTION : Include files
 * ===============================================
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/random.h>
#include <linux/regulator/consumer.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <mboot_params.h>

#include "gpueb_helper.h"
#include "gpueb_ipi.h"
#include "gpueb_logger.h"
#include "gpueb_reserved_mem.h"
#include "gpueb_plat_service.h"
#include "gpueb_hwvoter_dbg.h"
#include "gpueb_debug.h"
#include "gpueb_timesync.h"

/*
 * ===============================================
 * SECTION : Local functions declaration
 * ===============================================
 */

static int __mt_gpueb_pdrv_probe(struct platform_device *pdev);

/*
 * ===============================================
 * SECTION : Local variables definition
 * ===============================================
 */

static bool g_probe_done;
static struct platform_device *g_pdev;
static struct workqueue_struct *gpueb_logger_workqueue;

#if IS_ENABLED(CONFIG_PM)
static int gpueb_suspend(struct device *dev)
{
	gpueb_timesync_suspend();
	return 0;
}

static int gpueb_resume(struct device *dev)
{
	gpueb_timesync_resume();
	return 0;
}

static const struct dev_pm_ops gpueb_dev_pm_ops = {
	.suspend = gpueb_suspend,
	.resume  = gpueb_resume,
};
#endif

static const struct of_device_id g_gpueb_of_match[] = {
	{ .compatible = "mediatek,gpueb" },
	{ /* sentinel */ }
};

static struct platform_driver g_gpueb_pdrv = {
	.probe = __mt_gpueb_pdrv_probe,
	.remove = NULL,
	.driver = {
		.name = "gpueb",
		.owner = THIS_MODULE,
		.of_match_table = g_gpueb_of_match,
#if IS_ENABLED(CONFIG_PM)
		.pm = &gpueb_dev_pm_ops,
#endif
	},
};

const struct file_operations gpueb_log_file_ops = {
	.owner = THIS_MODULE,
	.read = gpueb_log_if_read,
	.open = gpueb_log_if_open,
	.poll = gpueb_log_if_poll,
};

static struct miscdevice gpueb_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gpueb",
	.fops = &gpueb_log_file_ops
};

static int gpueb_create_files(void)
{
	int ret = 0;

	ret = misc_register(&gpueb_device);
	if (unlikely(ret != 0)) {
		gpueb_pr_info("misc register failed");
		return ret;
	}

	ret = device_create_file(gpueb_device.this_device,
			&dev_attr_gpueb_mobile_log);
	if (unlikely(ret != 0))
		return ret;

	return 0;
}
/*
 * GPUEB driver probe
 */
static int __mt_gpueb_pdrv_probe(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int gpueb_support = 0;
	unsigned int gpueb_logger_support = 0;
	struct device_node *node;

	gpueb_pr_info("GPUEB driver probe start");

	node = of_find_matching_node(NULL, g_gpueb_of_match);
	if (!node)
		gpueb_pr_info("find GPUEB node failed");

	of_property_read_u32(pdev->dev.of_node, "gpueb-support",
			&gpueb_support);
	if (gpueb_support == 0) {
		gpueb_pr_info("Bypass the GPUEB driver probe");
		return 0;
	}

	ret = gpueb_ipi_init(pdev);
	if (ret != 0)
		gpueb_pr_info("ipi init fail");

	ret = gpueb_reserved_mem_init(pdev);
	if (ret != 0)
		gpueb_pr_info("reserved mem init fail");

	/*
	ret = gpueb_plat_service_init(pdev);
	if (ret != 0)
		gpueb_pr_info("plat service init fail");
	*/

	of_property_read_u32(pdev->dev.of_node, "gpueb-logger-support",
			&gpueb_logger_support);
	if (gpueb_logger_support == 1) {
		gpueb_logger_workqueue = create_singlethread_workqueue("GPUEB_LOG_WQ");
		if (gpueb_logger_init(pdev,
				gpueb_get_reserve_mem_virt(0),
				gpueb_get_reserve_mem_size(0)) == -1) {
			gpueb_pr_info("logger init fail");
			goto err;
		}

		ret = gpueb_create_files();
		if (unlikely(ret != 0)) {
			gpueb_pr_info("create files fail");
			goto err;
		}
	} else {
		gpueb_pr_info("gpueb no logger support.");
	}

	gpueb_hw_voter_dbg_init();

	/* init gpufreq debug */
	gpueb_debug_init(pdev);

	ret = gpueb_timesync_init();
	if (ret) {
		gpueb_pr_info("GPUEB timesync init fail");
		return ret;
	}

	g_pdev = pdev;
	g_probe_done = true;
	gpueb_pr_info("GPUEB driver probe done");

	return 0;

err:
	return -1;
}

/*
 * Register the GPUEB driver
 */
static int __init __mt_gpueb_init(void)
{
	int ret = 0;

	gpueb_pr_debug("start to initialize gpueb driver");

#ifdef CONFIG_PROC_FS
	// Create PROC FS
#endif

	// Register platform driver
	ret = platform_driver_register(&g_gpueb_pdrv);
	if (ret)
		gpueb_pr_info("fail to register gpueb driver");

	return ret;
}

/*
 * Unregister the GPUEB driver
 */
static void __exit __mt_gpueb_exit(void)
{
	platform_driver_unregister(&g_gpueb_pdrv);
}

module_init(__mt_gpueb_init);
module_exit(__mt_gpueb_exit);

MODULE_DEVICE_TABLE(of, g_gpueb_of_match);
MODULE_DESCRIPTION("MediaTek GPUEB-PLAT driver");
MODULE_LICENSE("GPL");
