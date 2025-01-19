// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/cpufreq.h>
#include <linux/kthread.h>
#include <linux/irq_work.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include "sugov/cpufreq.h"
#include "dsu_interface.h"

static void __iomem *clkg_sram_base_addr;
static void __iomem *wlc_sram_base_addr;
static void __iomem *l3ctl_sram_base_addr;
static void __iomem *cdsu_sram_base_addr;

/* dsu fine grained */
int default_dsu_fine_ctrl_enabled ;
static void __iomem *dsu_fine_ctrl_enabled_addr;
static void __iomem *dsu_fine_ctrl_addr;
static void __iomem *dsu_fine_val_C0_addr;
static void __iomem *dsu_fine_val_C1_addr;
static void __iomem *dsu_fine_val_C2_addr;

void __iomem *get_clkg_sram_base_addr(void)
{
	return clkg_sram_base_addr;
}
EXPORT_SYMBOL_GPL(get_clkg_sram_base_addr);

void __iomem *get_l3ctl_sram_base_addr(void)
{
	return l3ctl_sram_base_addr;
}
EXPORT_SYMBOL_GPL(get_l3ctl_sram_base_addr);

void __iomem *get_cdsu_sram_base_addr(void)
{
	return cdsu_sram_base_addr;
}
EXPORT_SYMBOL_GPL(get_cdsu_sram_base_addr);

int dsu_pwr_swpm_init(void)
{

	struct device_node *dev_node;
	struct platform_device *pdev_temp;
	struct resource *sram_res;
	int ret = 0;

	/* init l3ctl sram*/
	dev_node = of_find_node_by_name(NULL, "cpuqos-v3");
	if (!dev_node) {
		pr_info("failed to find node cpuqos-v3 @ %s\n", __func__);
		return -ENODEV;
	}

	pdev_temp = of_find_device_by_node(dev_node);
	if (!pdev_temp) {
		pr_info("failed to find cpuqos_v3 pdev @ %s\n", __func__);
		return -EINVAL;
	}

	sram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 0);
	if (sram_res) {
		l3ctl_sram_base_addr = ioremap(sram_res->start,
				resource_size(sram_res));
	} else {
		pr_info("%s can't get cpuqos_v3 resource\n", __func__);
		return -EINVAL;
	}

	/* init wlc sram */
	dev_node = of_find_node_by_name(NULL, "wl-info");
	if (!dev_node) {
		pr_info("failed to find node wl_info @ %s\n", __func__);
		return -ENODEV;
	}

	pdev_temp = of_find_device_by_node(dev_node);
	if (!pdev_temp) {
		pr_info("failed to find wl_info pdev @ %s\n", __func__);
		return -EINVAL;
	}

	sram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 0);
	if (sram_res) {
		wlc_sram_base_addr = ioremap(sram_res->start,
				resource_size(sram_res));
	} else {
		pr_info("%s can't get wl_info resource\n", __func__);
		return -EINVAL;
	}

	/* init clkg sram */
	dev_node = of_find_node_by_name(NULL, "cpuhvfs");
	if (!dev_node) {
		pr_info("failed to find node cpuhvfs @ %s\n", __func__);
		return -ENODEV;
	}

	pdev_temp = of_find_device_by_node(dev_node);
	if (!pdev_temp) {
		pr_info("failed to find cpuhvfs pdev @ %s\n", __func__);
		return -EINVAL;
	}

	sram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 0);

	if (sram_res)
		clkg_sram_base_addr = ioremap(sram_res->start,
				resource_size(sram_res));
	else {
		pr_info("%s can't get cpuhvfs resource\n", __func__);
		return -ENODEV;
	}

	sram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 1);

	if (sram_res)
		cdsu_sram_base_addr = ioremap(sram_res->start,
				resource_size(sram_res));
	else {
		pr_info("%s can't get cpuhvfs resource\n", __func__);
		return -ENODEV;
	}

	return ret;
}

int dsu_freq_init(void)
{
	struct device_node *dev_node;
	struct platform_device *pdev_temp;
	struct resource *sram_res;
	int ret = 0;
	int dfg_support;

	/* init dsu fine grained */
	dev_node = of_find_node_by_name(NULL, "dfg-info");
	if (!dev_node) {
		pr_info("failed to find node dfg_info @ %s\n", __func__);
		return -ENODEV;
	}

	pdev_temp = of_find_device_by_node(dev_node);
	if (!pdev_temp) {
		pr_info("failed to find dfg_info pdev @ %s\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(dev_node, "dfg-support", &dfg_support);
	default_dsu_fine_ctrl_enabled = dfg_support;
	if (ret)
		dfg_support = 0;

	sram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 0);
	if (sram_res) {
		dsu_fine_ctrl_addr = ioremap(sram_res->start,
				resource_size(sram_res));
	} else {
		pr_info("%s can't dfg_info resource\n", __func__);
		return -ENODEV;
	}

	sram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 1);
	if (sram_res) {
		dsu_fine_val_C2_addr = ioremap(sram_res->start,
				resource_size(sram_res));
	} else {
		pr_info("%s can't dfg_info resource\n", __func__);
		return -ENODEV;
	}

	sram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 2);
	if (sram_res) {
		dsu_fine_val_C1_addr = ioremap(sram_res->start,
				resource_size(sram_res));
	} else {
		pr_info("%s can't dfg_info resource\n", __func__);
		return -ENODEV;
	}

	sram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM,3);
	if (sram_res) {
		dsu_fine_val_C0_addr = ioremap(sram_res->start,
				resource_size(sram_res));
	} else {
		pr_info("%s can't dfg_info resource\n", __func__);
		return -ENODEV;
	}

	sram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 4);
	if (sram_res) {
		dsu_fine_ctrl_enabled_addr = ioremap(sram_res->start,
				resource_size(sram_res));
		iowrite8(default_dsu_fine_ctrl_enabled, dsu_fine_ctrl_enabled_addr);
	} else {
		pr_info("%s can't dfg_info resource\n", __func__);
		return -ENODEV;
	}
	return ret;
}

/* write pelt weight and pelt sum */
void update_pelt_data(unsigned int pelt_weight, unsigned int pelt_sum)
{
	iowrite32(pelt_sum, l3ctl_sram_base_addr+PELT_SUM_OFFSET);
	iowrite32(pelt_weight, l3ctl_sram_base_addr+PELT_WET_OFFSET);
}
EXPORT_SYMBOL_GPL(update_pelt_data);

/* get workload type */
unsigned int get_wl(unsigned int wl_idx)
{
	unsigned int wl;
	unsigned int offset;

	wl = ioread32(wlc_sram_base_addr);
	offset = wl_idx * 0x8;
	wl = (wl >> offset) & 0xff;

	return wl;
}

/* dsu fine grained */
void set_dsu_fine_ctrl_enable(int set)
{
	iowrite8(set, dsu_fine_ctrl_enabled_addr);
}
EXPORT_SYMBOL(set_dsu_fine_ctrl_enable);

int get_dsu_fine_ctrl_enable(void)
{
	int dsu_fine_ctrl_enable = 0;

	if (dsu_fine_ctrl_enabled_addr)
		dsu_fine_ctrl_enable = ioread8(dsu_fine_ctrl_enabled_addr);

	return dsu_fine_ctrl_enable;
}
EXPORT_SYMBOL(get_dsu_fine_ctrl_enable);

bool get_dsu_fine_ctrl(void)
{
    int dsu_fine_ctrl = 0;

	if(get_dsu_fine_ctrl_enable())
		dsu_fine_ctrl = ioread8(dsu_fine_ctrl_addr);

    return dsu_fine_ctrl;
}
EXPORT_SYMBOL(get_dsu_fine_ctrl);

int get_fine_value_pct_gear(int gearid)
{
	unsigned int fine_value_pct = 0;

	switch (gearid){
	case 0:
		fine_value_pct = ioread8(dsu_fine_val_C0_addr);
		break;
	case 1:
		fine_value_pct = ioread8(dsu_fine_val_C1_addr);
		break;
	case 2:
		fine_value_pct = ioread8(dsu_fine_val_C2_addr);
		break;
	default:
		return 0;
	}
    return fine_value_pct;
}
EXPORT_SYMBOL(get_fine_value_pct_gear);

int get_fine_value_pct_cpu(int cpu)
{
	int fine_value_pct = 0;
	int gearid = topology_cluster_id(cpu);

	fine_value_pct = get_fine_value_pct_gear(gearid);

	return fine_value_pct;
}
EXPORT_SYMBOL(get_fine_value_pct_cpu);

