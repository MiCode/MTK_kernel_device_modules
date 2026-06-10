/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef ADSP_CLK_V3_H
#define ADSP_CLK_V3_H

#include <linux/platform_device.h>
#include "adsp_clk.h"

int adsp_clk_probe(struct platform_device *pdev,
			  struct adsp_clk_operations *ops);
void adsp_clk_remove(void *dev);

#endif /* ADSP_CLK_V3_H */
