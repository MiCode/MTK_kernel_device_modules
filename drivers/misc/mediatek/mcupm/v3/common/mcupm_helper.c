// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include "mcupm_ipi_id.h"
#include "../include/mcupm_internal_driver.h"
#include "../include/mcupm_ipi.h"
#include "../include/mcupm_timesync.h"
#include "../include/mcupm_sysfs.h"

/* MCUPM HELPER. User is apmcu_sspm_mailbox_read/write*/
phys_addr_t mcupm_reserve_mem_get_phys(unsigned int id) { return 0; }
EXPORT_SYMBOL_GPL(mcupm_reserve_mem_get_phys);
phys_addr_t mcupm_reserve_mem_get_virt(unsigned int id) { return 0; }
EXPORT_SYMBOL_GPL(mcupm_reserve_mem_get_virt);
phys_addr_t mcupm_reserve_mem_get_size(unsigned int id) { return 0; }
EXPORT_SYMBOL_GPL(mcupm_reserve_mem_get_size);

void *get_mcupm_ipidev(void)
{
	return mcupm_ipidevs;
}
EXPORT_SYMBOL_GPL(get_mcupm_ipidev);

#if IS_ENABLED(CONFIG_PM)
static int pm_mcupm_suspend(struct device *dev)
{
	mcupm_timesync_suspend();
	return 0;
}
static int pm_mcupm_resume(struct device *dev)
{
	mcupm_timesync_resume();
	return 0;
}
static const struct dev_pm_ops mtk_mcupms_dev_pm_ops = {
	.suspend = pm_mcupm_suspend,
	.resume  = pm_mcupm_resume,
};
#endif
static const struct platform_device_id mcupms_id_table[] = {
	{ "mcupms_v0", 0},
	{ },
};
static const struct of_device_id mcupms_of_match[] = {
	{ .compatible = "mediatek,mcupms_v0", },
	{},
};

static struct platform_driver mtk_mcupms_driver = {
	.shutdown = NULL,
	.suspend = NULL,
	.resume = NULL,
	.probe = mcupms_device_probe,
	.remove = mcupms_device_remove,
	.driver = {
		.name = "mcupm_v3",
		.owner = THIS_MODULE,
		.of_match_table = mcupms_of_match,
#if IS_ENABLED(CONFIG_PM)
		.pm = &mtk_mcupms_dev_pm_ops,
#endif
	},
	.id_table = mcupms_id_table,
};

/*
 * driver initialization entry point
 */
static int __init mcupm_module_init(void)
{
	int ret = 0;

	pr_info("[MCUPM] mcupm module init.\n");

	ret = mcupms_sysfs_misc_init();
	if (ret) {
		pr_info("[MCUPM] mcupm_sysfs_misc_init fail, ret=%d\n", ret);
		return ret;
	}

	ret = platform_driver_register(&mtk_mcupms_driver);
	if (ret) {
		pr_info("[MCUPM] register mtk_mcupm_driver_v2 fail, ret %d\n", ret);
		return ret;
	}

	return ret;
}

static void __exit mcupm_module_exit(void)
{
    //Todo release resource
	pr_info("[MCUPM] mcupm module exit.\n");
}

MODULE_DESCRIPTION("MEDIATEK Module MCUPM driver");
MODULE_LICENSE("GPL");

module_init(mcupm_module_init);
module_exit(mcupm_module_exit);
