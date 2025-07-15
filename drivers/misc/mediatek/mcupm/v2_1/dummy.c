// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/device.h>

/*
 * driver initialization entry point
 */
static int __init mcupm_dummy_module_init(void)
{
	return 0;
}

static void __exit mcupm_dummy_module_exit(void)
{
}

MODULE_DESCRIPTION("MEDIATEK Module MCUPM dummy driver");
MODULE_LICENSE("GPL");

module_init(mcupm_dummy_module_init);
module_exit(mcupm_dummy_module_exit);

