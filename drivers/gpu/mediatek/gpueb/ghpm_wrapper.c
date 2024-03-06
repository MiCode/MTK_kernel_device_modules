// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

/**
 * @file    ghpm_wrapper.c
 * @brief   GHPM Wrapper Init and API
 */

/**
 * ===============================================
 * SECTION : Include files
 * ===============================================
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <uapi/asm-generic/errno-base.h>

#include "gpueb_common.h"
#include "gpueb_helper.h"
#include "gpueb_ipi.h"
#include "ghpm_wrapper.h"
#include "ghpm_debug.h"

static struct ghpm_platform_fp *ghpm_fp;
unsigned int g_ghpm_support;
EXPORT_SYMBOL(g_ghpm_support);

int ghpm_ctrl(enum ghpm_state power, enum mfg0_off_state off_state)
{
	int ret = -ENOENT;

	if (!g_ghpm_support)
		return 0;

	if (ghpm_fp && ghpm_fp->ghpm_ctrl)
		ret = ghpm_fp->ghpm_ctrl(power, off_state);
	else
		gpueb_pr_err(GHPM_TAG, "null ghpm platform function pointer (ENOENT)");

	return ret;
}
EXPORT_SYMBOL(ghpm_ctrl);

int wait_gpueb(enum gpueb_low_power_event event)
{
	int ret = -ENOENT;

	if (!g_ghpm_support)
		return 0;

	if (ghpm_fp && ghpm_fp->wait_gpueb)
		ret = ghpm_fp->wait_gpueb(event);
	else
		gpueb_pr_err(GHPM_TAG, "null ghpm platform function pointer (ENOENT)");

	return ret;
}
EXPORT_SYMBOL(wait_gpueb);

int gpueb_ctrl(enum ghpm_state power,
	enum mfg0_off_state off_state, enum gpueb_low_power_event event)
{
	int ret = -ENOENT;

	if (!g_ghpm_support)
		return 0;

	ret = ghpm_ctrl(power, off_state);
	if (ret) {
		gpueb_pr_err(GHPM_TAG, "gpueb ctrl fail, power=%d, off_state=%d, event=%d, ret=%d",
			power, off_state, event, ret);
		return ret;
	}

	ret = wait_gpueb(event);
	if (ret) {
		gpueb_pr_err(GHPM_TAG, "wait gpueb fail, power=%d, off_state=%d, event=%d, ret=%d",
			power, off_state, event, ret);
		return ret;
	}

	return ret;
}
EXPORT_SYMBOL(gpueb_ctrl);

void dump_ghpm_info(void)
{
	if (!g_ghpm_support)
		return;

	if (ghpm_fp && ghpm_fp->dump_ghpm_info)
		ghpm_fp->dump_ghpm_info();
	else
		gpueb_pr_err(GHPM_TAG, "null ghpm platform function pointer (ENOENT)");
}
EXPORT_SYMBOL(dump_ghpm_info);

void ghpm_register_ghpm_fp(struct ghpm_platform_fp *platform_fp)
{
	if (!platform_fp) {
		gpueb_pr_err(GHPM_TAG, "null ghpm platform function pointer (ENOENT)");
		return;
	}

	ghpm_fp = platform_fp;
	gpueb_pr_debug(GHPM_TAG, "Hook ghpm platform function pointer done");
}
EXPORT_SYMBOL(ghpm_register_ghpm_fp);

void ghpm_wrapper_init(struct platform_device *pdev)
{
	of_property_read_u32(pdev->dev.of_node, "ghpm-support", &g_ghpm_support);

	if (g_ghpm_support == 0)
		gpueb_pr_info(GHPM_TAG, "no ghpm support");
	else
		ghpm_debug_init(pdev);
}
