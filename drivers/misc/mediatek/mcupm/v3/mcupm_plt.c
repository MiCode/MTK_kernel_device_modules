// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/io.h>


#include "mcupm_ipi_id.h"
#include "include/mcupm_plt.h"
#include "include/mcupm_internal_driver.h"
#include "include/mcupm_sysfs.h"
#include "include/mcupm_timesync.h"

int multi_mcupm_plt_ackdata[max_mcupm];

#if MCUPM_PLT_SERV_SUPPORT
int mcupms_plt_module_init(void)
{
	int ret = 0;
	u32 ipinum = 0;

	ipinum = get_mcupms_ipidev_number();
	for(int i=0; i < ipinum; i++) {
		struct mtk_ipi_device *ipi_dev = GET_MCUPM_IPIDEV(i);

		if (ipi_dev == NULL) {
			pr_info("[MCUPM] GET_MCUPM_IPIDEV(%d) is NULL\n", i);
			return -1;
		}
		/* Initialize mcupm ipi driver */
		ret = mtk_ipi_register(ipi_dev, CHAN_PLATFORM, NULL, NULL,
					       (void *) &multi_mcupm_plt_ackdata[i]);
		if (ret) {
			pr_info("[MCUPM] mtk_ipi_register(%d) failed ret=%d\n", i, ret);
			return -1;
		}
	}

	//MCUPM AP Timesync(Boardcast)
	ret = mcupm_timesync_init();
	if (ret) {
		pr_info("[MCUPM] mcupm_timesync_init fail, ret %d\n", ret);
		return ret;
	}

	return 0;
}
void mcupms_plt_module_exit(void)
{
	int ret = 0;
	u32 ipinum = get_mcupms_ipidev_number();

	for(int i=0; i < ipinum; i++) {
		struct mtk_ipi_device *ipi_dev = GET_MCUPM_IPIDEV(i);

		if (ipi_dev == NULL) {
			pr_info("[MCUPM] GET_MCUPM_IPIDEV(%d) is NULL\n", i);
			return;
		}
		ret = mtk_ipi_unregister(ipi_dev, CHAN_PLATFORM);
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
