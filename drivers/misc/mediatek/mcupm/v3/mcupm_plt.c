// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/io.h>


#include "mcupm_plt.h"
#include "mcupm_driver.h"
#include "mcupm_ipi_id.h"
#include "mcupm_sysfs.h"

int multi_mcupm_plt_ackdata[max_mcupm];

#if MCUPM_PLT_SERV_SUPPORT

int mcupms_plt_module_init(void)
{
	int ret = 0;
	u32 ipinum = 0;
	struct mtk_ipi_device *ipidev;

	ipinum = get_mcupms_ipidev_number();
	for(int i=0; i < ipinum; i++) {
		/* Initialize mcupm ipi driver */
		ret = mtk_ipi_register(GET_MCUPM_IPIDEV(i), CHAN_PLATFORM, NULL, NULL,
					       (void *) &multi_mcupm_plt_ackdata[i]);
		if(ret) {
				pr_info("[MCUPM] mtk_ipi_register(%d) failed ret=%d\n", i, ret);
				return -1;
		}
	}

	//MCUPM Check Alive
	for(int i=0; i < ipinum; i++) {
		struct mcupm_ipi_data_s ipi_data;
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
void mcupms_plt_module_exit(void)
{
	int ret = 0;
	u32 ipinum = get_mcupms_ipidev_number();

	for(int i=0; i < ipinum; i++) {
		ret = mtk_ipi_unregister(GET_MCUPM_IPIDEV(i), CHAN_PLATFORM);
		if(ret) {
			pr_info("[MCUPM] mtk_ipi_unregister(%d) failed ret=%d\n", i, ret);
			return;
		}
	}
	pr_info("[MCUPM] mcupms plt module exit.\n");
	return;

}
int mcupms_plt_create_file(void)
{
	u32 ret = 0;

	ret = mcupm_sysfs_create_mcupm_alive();
	if (unlikely(ret != 0))
		return ret;

	return 0;
}

#endif
