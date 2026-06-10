/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef ADSP_DRIVER_V3_H
#define ADSP_DRIVER_V3_H

#include <linux/soc/mediatek/mtk-mbox.h>
#include "adsp_platform_driver.h"

#define READ_U32_PROPERTY_DEFAULT(node, prop_name, val, default_val) \
	({ \
		int ret = 0; \
		ret = of_property_read_u32(node, prop_name, val); \
		if (ret) { \
			dev_warn(dev, "Failed to read property %s, using default value\n", prop_name); \
			*(val) = (default_val); \
		} \
		ret; \
	})

#define READ_U64_PROPERTY_DEFAULT(node, prop_name, val, default_val) \
	({ \
		int ret = 0; \
		ret = of_property_read_u64(node, prop_name, val); \
		if (ret) { \
			dev_warn(dev, "Failed to read property %s, using default value\n", prop_name); \
			*(val) = (default_val); \
		} \
		ret; \
	})

/* symbol need from adsp.ko */
extern int adsp_system_bootup(void);
extern void register_adspsys(struct adspsys_priv *mt_adspsys);
extern void register_adsp_core(struct adsp_priv *pdata);

extern int adsp_mbox_probe(struct platform_device *pdev);
extern struct mtk_mbox_pin_send *get_adsp_mbox_pin_send(int index);
extern struct mtk_mbox_pin_recv *get_adsp_mbox_pin_recv(int index);

extern int adsp_mem_device_probe(struct platform_device *pdev);

extern int adsp_after_bootup(struct adsp_priv *pdata);
extern int adsp_core0_init(struct adsp_priv *pdata);
extern int adsp_core1_init(struct adsp_priv *pdata);

#endif /* ADSP_DRIVER_V3_H */
