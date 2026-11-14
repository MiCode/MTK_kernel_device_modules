/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_SOC_TEMP_LDRO_H__
#define __MTK_SOC_TEMP_LDRO_H__
/*==================================================
 * Definition or macro function
 *==================================================
 */

struct power_domain {
	//void __iomem *base;	/* LVTS base addresses */
	unsigned int irq_num;	/* LDRO interrupt numbers */
	//struct reset_control *reset;
};

struct ldro_data {
	struct device *dev;
	unsigned int num_domain;
	struct power_domain *domain;
};

#endif /* __MTK_SOC_TEMP_LDRO_H__ */

