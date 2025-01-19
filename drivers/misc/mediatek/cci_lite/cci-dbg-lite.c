// SPDX-License-Identifier: GPL-2.0
/*
 * cpufreq-dbg-lite.c - eem debug driver
 *
 * Copyright (c) 2024 MediaTek Inc.
 * Derek-HW Lin <derek-hw.lin@mediatek.com>
 */

/* system includes */
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include "../mcupm/include/mcupm_driver.h"
#include "../mcupm/include/mcupm_ipi_id.h"
#include "cci-dbg-lite.h"
#include "sugov/cpufreq.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[ccidvfs]: " fmt

static void __iomem *csram_base;

unsigned int user_ctrl_mode;
bool dsu_ctrl_deubg_enable;


void set_cci_mode(unsigned int mode)
{
	/* mode = 0(Normal as 50%) mode = 1(Perf as 70%) */
	csram_write(OFFS_CCI_TBL_MODE, mode);
}

unsigned int cpufreq_get_cci_mode(void)
{
	unsigned int mode;

	/* Normal mode: 0, Perf mode: 1*/
	mode = csram_read(OFFS_CCI_TBL_MODE);

	if (mode > 1)
		return -1;

	return mode;
}

int cpufreq_set_cci_mode(unsigned int mode)
{
	if (mode > 1) {
		pr_info("%s: invalid input value: %d.\n", __func__, mode);
		return -EINVAL;
	}

	user_ctrl_mode = mode;
	set_cci_mode(user_ctrl_mode);
	if (dsu_ctrl_deubg_enable) {
		pr_info("%s: debug mode.\n", __func__);
		return 0;
	}

	if (user_ctrl_mode)
		set_eas_dsu_ctrl(0);
	else
		set_eas_dsu_ctrl(1);

	return 0;
}
EXPORT_SYMBOL_GPL(cpufreq_set_cci_mode);

int set_dsu_ctrl_debug(unsigned int eas_ctrl_mode, bool debug_enable)
{
	dsu_ctrl_deubg_enable = debug_enable;

	if (dsu_ctrl_deubg_enable)
		set_eas_dsu_ctrl(eas_ctrl_mode);
	else {
		if (user_ctrl_mode)
			set_eas_dsu_ctrl(0);
		else
			set_eas_dsu_ctrl(1);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(set_dsu_ctrl_debug);

static int mtk_ccidvfs_init(void)
{
	struct device_node *dvfs_node;
	struct platform_device *pdev;
	struct resource *csram_res;

	dvfs_node = of_find_node_by_name(NULL, "cpuhvfs");
	if (dvfs_node == NULL) {
		pr_info("failed to find node @ %s\n", __func__);
		return -ENODEV;
	}

	pdev = of_find_device_by_node(dvfs_node);
	if (pdev == NULL) {
		pr_info("failed to find pdev @ %s\n", __func__);
		return -EINVAL;
	}

	csram_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	csram_base = ioremap(csram_res->start, resource_size(csram_res));
	if (csram_base == NULL) {
		pr_notice("failed to map csram_base @ %s\n", __func__);
		return -EINVAL;
	}
	dsu_ctrl_deubg_enable = false;
	user_ctrl_mode = 0;

	return 0;
}
module_init(mtk_ccidvfs_init);

static void mtk_ccidvfs_exit(void)
{
}
module_exit(mtk_ccidvfs_exit);

MODULE_DESCRIPTION("MTK CCI DVFS Platform Driver Helper v0.0.1");
MODULE_AUTHOR("Derek-HW Lin <derek-hw.lin@mediatek.com>");
MODULE_LICENSE("GPL v2");
