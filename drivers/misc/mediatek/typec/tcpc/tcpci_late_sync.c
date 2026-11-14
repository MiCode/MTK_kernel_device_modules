// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#if IS_ENABLED(CONFIG_TCPC_CLASS)
#include <linux/init.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/slab.h>

#include "inc/tcpci.h"
#include "inc/tcpci_typec.h"

static int __init tcpc_class_complete_init(void)
{
	if (!IS_ERR(tcpc_class)) {
		class_for_each_device(tcpc_class, NULL, (void *)COMPLETE_TYPE_KO_TABLE,
			tcpc_class_complete_work);
	}
	return 0;
}
late_initcall_sync(tcpc_class_complete_init);

MODULE_DESCRIPTION("Richtek TypeC Port Late Sync Driver");
MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_VERSION("1.0.0_MTK");
#endif	/* CONFIG_TCPC_CLASS */
MODULE_LICENSE("GPL");
