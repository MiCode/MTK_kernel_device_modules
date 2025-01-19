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

#include "mcupm_ipi_id.h"
#include "include/mcupm_internal_driver.h"
#include "include/mcupm_plt.h"


extern int mcupms_plt_module_init(void);
extern int mcupms_init_ipi_mboxs(struct platform_device *pdev);
extern int mcupms_sysfs_misc_init(void);
extern int mcupm_sysfs_init(void);
extern int mcupms_plt_create_file(void);

extern int multi_mcupm_plt_ackdata[max_mcupm];
int mcupms_selfcheck(void)
{
	int ret;
	u32 ipinum = 0;
	struct mtk_ipi_device *ipidev;

	//MCUPM Check Alive
	ipinum = get_mcupms_ipidev_number();
	for(int i = 0; i < ipinum; i++) {
		struct mcupm_ipi_data_s ipi_data;

		multi_mcupm_plt_ackdata[i] = 0;
		ipi_data.cmd = 0xDEAD;
		ipidev = GET_MCUPM_IPIDEV(i);
		if (ipidev) {
			ret = mtk_ipi_send_compl(ipidev, CHAN_PLATFORM, IPI_SEND_WAIT,
						&ipi_data,
						sizeof(struct mcupm_ipi_data_s) / MCUPM_MBOX_SLOT_SIZE,
						2000);
			pr_info("[MCUPM] mtk_ipi_send_compl(%d) ack=%d ret=%d\n", i, multi_mcupm_plt_ackdata[i], ret);
		} else {
			pr_info("[MCUPM] GET_MCUPM_IPIDEV(%d) is empty\n", i);
		}
	}

	return 0;
}

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

	ret = mcupms_selfcheck();
	if (ret) {
		pr_info("[MCUPM] mcupms_selftest fail, ret %d\n", ret);
		return ret;
	}

	return 0;
}
void mcupms_device_remove(struct platform_device *pdev)
{
	//Todo implement remove ipi interface and memory
	return;
}


