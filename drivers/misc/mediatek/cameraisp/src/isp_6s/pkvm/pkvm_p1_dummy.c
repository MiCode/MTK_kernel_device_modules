// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */


 /*******************************************************************************
  * Includes
  ******************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>


/*******************************************************************************
 * MACRO Definitions
 ******************************************************************************/
#define LogTag "[PKVM_P1_DUMMY]"
#define LOG_NOTICE(format, args...)                                            \
	pr_notice(LogTag "[%s] " format, __func__, ##args)


 /*******************************************************************************
  * Dummy APIs
  ******************************************************************************/
int pkvm_p1_uninit_by_isp(void)
{
	int ret = 0;

	LOG_NOTICE(" INFO: pkvm is NOT supported!\n");

	return ret;
}
EXPORT_SYMBOL(pkvm_p1_uninit_by_isp);

MODULE_LICENSE("GPL");
