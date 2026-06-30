// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include "mcupm_ipi_id.h"


/*
 * driver initialization entry point
 */
static int __init mcupm_test_module_init(void)
{
	int ret;

	ret = get_mcupms_ipidev_number();

	return ret;
}

static void __exit mcupm_test_module_exit(void)
{
    //Todo release resource
	pr_info("[MCUPM] mcupm test module exit.\n");
}

MODULE_DESCRIPTION("MEDIATEK Module MCUPM test driver");
MODULE_LICENSE("GPL");

module_init(mcupm_test_module_init);
module_exit(mcupm_test_module_exit);

