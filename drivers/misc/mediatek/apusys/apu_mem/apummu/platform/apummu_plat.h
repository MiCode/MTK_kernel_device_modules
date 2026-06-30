/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#ifndef __APUSYS_APUMMU_PLAT_H__
#define __APUSYS_APUMMU_PLAT_H__

#include <linux/platform_device.h>

/* apummu paltform data */
struct apummu_plat {
	unsigned int slb_wait_time;
	unsigned int encode_offset;
	unsigned int address_bits;
	bool is_general_SLB_support;
	bool alloc_DRAM_FB_in_session_create;
	bool is_ASE_support;
	const void *hw_ops;
	uint8_t reserved_session_num;
	bool is_SLC_support;
};

int apummu_plat_init(struct platform_device *pdev);

#endif
