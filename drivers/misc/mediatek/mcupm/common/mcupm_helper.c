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
#include "mcupm_timesync.h"
#include "mcupm_sysfs.h"

extern phys_addr_t mcupm_reserve_mem_get_phys(unsigned int id);
EXPORT_SYMBOL_GPL(mcupm_reserve_mem_get_phys);

extern phys_addr_t mcupm_reserve_mem_get_virt(unsigned int id);
EXPORT_SYMBOL_GPL(mcupm_reserve_mem_get_virt);

extern phys_addr_t mcupm_reserve_mem_get_size(unsigned int id);
EXPORT_SYMBOL_GPL(mcupm_reserve_mem_get_size);

/* MCUPM HELPER. User is apmcu_sspm_mailbox_read/write*/
extern int mcupm_mbox_read(unsigned int mbox, unsigned int slot, void *buf,
			unsigned int len);
EXPORT_SYMBOL_GPL(mcupm_mbox_read);

extern int mcupm_mbox_write(unsigned int mbox, unsigned int slot, void *buf,
			unsigned int len);
EXPORT_SYMBOL_GPL(mcupm_mbox_write);
extern u32 get_mcupms_ipidev_number(void);
EXPORT_SYMBOL_GPL(get_mcupms_ipidev_number);

extern void *get_mcupm_ipidev(void);
EXPORT_SYMBOL_GPL(get_mcupm_ipidev);

extern struct mtk_ipi_device *mcupm_ipidevs;
extern struct mtk_ipi_device mcupm_ipidev;
void *get_mcupm_ipidev(void)
{
	if(get_mcupms_ipidev_number() > 0) {
		return mcupm_ipidevs;
	}

	if(mcupm_ipidev.ipi_inited) {
		return &mcupm_ipidev;
	}
	return NULL;
}

#if IS_ENABLED(CONFIG_PM)
extern int mt6779_mcupm_suspend(struct device *dev);
extern int mt6779_mcupm_resume(struct device *dev);

static const struct dev_pm_ops mt6779_mcupm_dev_pm_ops = {
	.suspend = mt6779_mcupm_suspend,
	.resume  = mt6779_mcupm_resume,
};
#endif

static const struct of_device_id mcupm_of_match[] = {
	{ .compatible = "mediatek,mcupm", },
	{},
};

static const struct platform_device_id mcupm_id_legacy_table[] = {
	{ "mcupm_legacy", 0},
	{ },
};
int mcupm_device_probe(struct platform_device *pdev);
void mcupm_device_remove(struct platform_device *pdev);

static struct platform_driver mtk_mcupm_driver = {
	.shutdown = NULL,
	.suspend = NULL,
	.resume = NULL,
	.probe = mcupm_device_probe,
	.remove = mcupm_device_remove,
	.driver = {
		.name = "mcupm_v2",
		.owner = THIS_MODULE,
		.of_match_table = mcupm_of_match,
#if IS_ENABLED(CONFIG_PM)
		.pm = &mt6779_mcupm_dev_pm_ops,
#endif
	},
	.id_table = mcupm_id_legacy_table,
};

static const struct of_device_id mcupms_of_match[] = {
	{ .compatible = "mediatek,mcupms_v0", },
	{},
};

#if IS_ENABLED(CONFIG_PM)
int pm_mcupm_suspend(struct device *dev);
int pm_mcupm_resume(struct device *dev);

int pm_mcupm_suspend(struct device *dev)
{
	mcupm_timesync_suspend();
	return 0;
}

int pm_mcupm_resume(struct device *dev)
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
int mcupms_device_probe(struct platform_device *pdev);
void mcupms_device_remove(struct platform_device *pdev);

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

	ret = platform_driver_register(&mtk_mcupm_driver);
	if (ret) {
		pr_info("[MCUPM] register mtk_mcupm_driver fail, ret %d\n", ret);
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
MODULE_LICENSE("GPL v2");

module_init(mcupm_module_init);
module_exit(mcupm_module_exit);
