// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/errno.h>
#include "apummu_cmn.h"
#include "apummu_boot.h"

static const struct ammu_hw_ops *hw_ops = NULL;


void ammu_set_hw_ops(const void *ops)
{
	hw_ops = ops;
}

int rv_boot(u32 uP_seg_output, u8 uP_hw_thread,
	u32 logger_seg_output, enum eAPUMMUPAGESIZE logger_page_size,
	u32 XPU_seg_output, enum eAPUMMUPAGESIZE XPU_page_size)
{
	int ret;

	if (!hw_ops) {
		AMMU_LOG_ERR("hw_ops is null\n");
		ret = -EOPNOTSUPP;
		goto exit;
	}

	if (!hw_ops->rv_boot) {
		AMMU_LOG_ERR("rv_boot is null\n");
		ret = -EOPNOTSUPP;
		goto exit;
	}

	ret = hw_ops->rv_boot(uP_seg_output, uP_hw_thread, logger_seg_output,
		logger_page_size, XPU_seg_output, XPU_page_size);
exit:
	return ret;
}

