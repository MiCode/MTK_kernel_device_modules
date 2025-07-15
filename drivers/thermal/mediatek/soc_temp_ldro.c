// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/thermal.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/bits.h>
#include <linux/string.h>
#include <linux/iopoll.h>
#include "soc_temp_ldro.h"
#include "thermal_interface.h"
#include "thermal_core.h"
#include <linux/math64.h>

#define REBOOT_STATUS_REG (2)
void __iomem *reboot_status[REBOOT_STATUS_REG];

static int of_update_ldro_data(struct ldro_data *ldro_data,
	struct platform_device *pdev)
{
	struct device *dev = ldro_data->dev;
	struct power_domain *domain;
	struct resource *res;
	//struct platform_ops *ops = &ldro_data->ops;
	unsigned int i;
	//int ret;
	int irq;

	domain = devm_kcalloc(dev, ldro_data->num_domain, sizeof(*domain),
			GFP_KERNEL);
	if (!domain)
		return -ENOMEM;

	// Get reboot status address
	for(i = 0; i < REBOOT_STATUS_REG; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			// Check service FAIL !, Cosimo suggested using pr_info
			dev_err(dev, "Error: No IO resource, index %d\n", i);
			return -ENXIO;
		}

		reboot_status[i] = devm_ioremap_resource(dev, res);
		if (IS_ERR(reboot_status[i])) {
			// Check service FAIL !, Cosimo suggested using pr_info
			dev_err(dev, "Error: Failed to remap io, index %d\n", i);
			return PTR_ERR(reboot_status[i]);
		}
	}

	for (i = 0; i < ldro_data->num_domain; i++) {
		/* Get interrupt number */
		irq = platform_get_irq(pdev, i);
		if (irq < 0) {
			dev_err(dev, "Error: No IRQ resource at index %d\n", i);
			return -ENOENT;
		}
		dev_info(dev, "domain[%d] irq_num=%d\n", i, irq);
		domain[i].irq_num = irq;
	}

	ldro_data->domain = domain;
	return 0;
}

static irqreturn_t irq_handler_ldro(int irq, void *dev_id)
{
	struct ldro_data *ldro_data = (struct ldro_data *) dev_id;
	struct device *dev = ldro_data->dev;
	static int LOG;
	unsigned int reboot_source, reboot_ldro;

	LOG = 0;
	if(LOG == 0) {
		LOG = 1;
		reboot_source = readl(reboot_status[0]);
		reboot_ldro = readl(reboot_status[1]);
		writel(reboot_source, (void __iomem *)(thermal_csram_base + 0x380));
		writel(reboot_ldro, (void __iomem *)(thermal_csram_base + 0x384));
		dev_err(dev, "Error: LDRO Reboot status: 0x%x - 0x%x\n", reboot_source, reboot_ldro);
	}
	return IRQ_HANDLED;
}

static int ldro_register_irq_handler(struct ldro_data *ldro_data)
{
	struct device *dev = ldro_data->dev;
	int i;
	int ret;

	for (i = 0; i < ldro_data->num_domain; i++) {
		ret = devm_request_irq(dev, ldro_data->domain[i].irq_num,
				irq_handler_ldro, IRQF_TRIGGER_HIGH, "mtk_ldro", ldro_data);
		dev_info(dev, "Register LDRO ISR %d\n", ldro_data->domain[i].irq_num);
	}

	return 0;
}

static int ldro_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ldro_data *ldro_data;
	int ret;

	ldro_data = (struct ldro_data *) of_device_get_match_data(dev);
	if (!ldro_data)	{
		dev_err(dev, "Error: Failed to get ldro platform data\n");
		return -ENODATA;
	}

	ldro_data->dev = &pdev->dev;

	//check_runtime_log_from_dts(lvts_data, pdev);

	ret = of_update_ldro_data(ldro_data, pdev);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, ldro_data);

	//if (lvts_data->mcu_sensor_id_remap)
	//	lvts_get_chipid();

	ret = ldro_register_irq_handler(ldro_data);
	if (ret)
		return ret;

	//mutex_init(&ldro_data->sen_data_lock);

	//ret = lvts_register_thermal_zones(lvts_data);
	//if (ret)
	//	return ret;

	return 0;
}

static void ldro_remove(struct platform_device *pdev)
{
}

static int ldro_suspend_noirq(struct device *dev)
{
	dev_info(dev, "[Thermal/LDRO]%s\n", __func__);

	return 0;
}

static int ldro_resume_noirq(struct device *dev)
{
	dev_info(dev, "[Thermal/LDRO]%s\n", __func__);

	return 0;
}

/*==================================================
 * LDRO MT6993
 *==================================================
 */
enum mt6993_ldro_domain {
	MT6993_MCU_DOMAIN,
	MT6993_NUM_DOMAIN
};
static struct ldro_data mt6993_ldro_data = {
	.num_domain = MT6993_NUM_DOMAIN,
};
/*==================================================
 * Support chips
 *==================================================
 */
static const struct dev_pm_ops ldro_pm_ops = {
	.suspend_noirq = ldro_suspend_noirq,
	.resume_noirq = ldro_resume_noirq,
};

static const struct of_device_id ldro_of_match[] = {
	{
		.compatible = "mediatek,mt6993-ldro",
		.data = (void *)&mt6993_ldro_data,
	},
	{
	},
};
MODULE_DEVICE_TABLE(of, ldro_of_match);
/*==================================================*/
static struct platform_driver soc_temp_ldro = {
	.probe = ldro_probe,
	.remove = ldro_remove,
	.driver = {
		.name = "mtk-soc-temp-ldro",
		.of_match_table = ldro_of_match,
		.pm = &ldro_pm_ops,
	},
};

module_platform_driver(soc_temp_ldro);
MODULE_AUTHOR("Mac Lee <mac.lee@mediatek.com>");
MODULE_DESCRIPTION("Mediatek soc temperature LDRO driver");
MODULE_LICENSE("GPL");

