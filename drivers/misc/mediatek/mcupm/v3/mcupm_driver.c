// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>

#include "mcupm_driver.h"
#include "mcupm_plt.h"
#include "mcupm_ipi_id.h"

extern int mcupms_plt_module_init(void);
extern int mcupms_init_ipi_mboxs(struct platform_device *pdev);
extern int mcupms_sysfs_misc_init(void);
extern int mcupm_sysfs_init(void);
extern int mcupms_plt_create_file(void);


int mcupms_device_probe(struct platform_device *pdev)
{
	int ret;

	pr_info("[MCUPM] mcupms_device_probe Enter\n");

	ret = mcupms_init_ipi_mboxs(pdev);
	if (ret) {
		pr_info("[MCUPM] mcupm_init_ipi_mboxs fail, ret %d\n", ret);
		return ret;
	}
	pr_info("MCUPM is ready to service IPI\n");

	ret = mcupms_plt_module_init();
	if (ret) {
		pr_info("[MCUPM] mcupm_plt_module_init fail, ret %d\n", ret);
		return ret;
	}

	ret = mcupms_plt_create_file();
	if (ret) {
		pr_info("[MCUPM] mcupms_plt_create_file fail, ret %d\n", ret);
		return ret;
	}

	return 0;
}
void mcupms_device_remove(struct platform_device *pdev)
{
	//Todo implement remove ipi interface and memory
	mcupm_plt_module_exit();
	return;
}


