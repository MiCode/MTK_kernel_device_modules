// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include "apummu_cmn.h"
#include "apummu_boot.h"
#include "apummu_plat.h"
#include "apummu_device.h"


static struct apummu_plat mt6897_drv = {
	.slb_wait_time                   = 0,
	.encode_offset                   = 0x20000000,
	.address_bits                    = 34,
	.is_general_SLB_support          = true,
	.alloc_DRAM_FB_in_session_create = true,
	.is_ASE_support                  = false,
	.hw_ops                          = NULL,
	.reserved_session_num            = 0,
	.is_SLC_support                  = false,
};

static struct apummu_plat mt6989_drv = {
	.slb_wait_time                   = 0,
	.encode_offset                   = 0x20000000,
	.address_bits                    = 34,
	.is_general_SLB_support          = true,
	.alloc_DRAM_FB_in_session_create = true,
	.is_ASE_support                  = false,
	.hw_ops                          = NULL,
	.reserved_session_num            = 0,
	.is_SLC_support                  = false,
};

static struct apummu_plat mt6878_drv = {
	.slb_wait_time                   = 0,
	.encode_offset                   = 0x20000000,
	.address_bits                    = 34,
	.is_general_SLB_support          = false,
	.alloc_DRAM_FB_in_session_create = true,
	.is_ASE_support                  = false,
	.hw_ops                          = NULL,
	.reserved_session_num            = 0,
	.is_SLC_support                  = false,
};

static struct apummu_plat mt6991_drv = {
	.slb_wait_time                   = 0,
	.encode_offset                   = 0x20000000,
	.address_bits                    = 34,
	.is_general_SLB_support          = true,
	.alloc_DRAM_FB_in_session_create = false,
	.is_ASE_support                  = false,
	.hw_ops                          = NULL,
	.reserved_session_num            = 0,
	.is_SLC_support                  = false,
};

static struct apummu_plat mt6899_drv = {
	.slb_wait_time                   = 0,
	.encode_offset                   = 0x20000000,
	.address_bits                    = 34,
	.is_general_SLB_support          = true,
	.alloc_DRAM_FB_in_session_create = false,
	.is_ASE_support                  = false,
	.hw_ops                          = NULL,
	.reserved_session_num            = 0,
	.is_SLC_support                  = false,
};

static struct apummu_plat mt6993_drv = {
	.slb_wait_time                   = 0,
	.encode_offset                   = 0x00000000,
	.address_bits                    = 36,
	.is_general_SLB_support          = false,
	.alloc_DRAM_FB_in_session_create = false,
	.is_ASE_support                  = true,
	.hw_ops                          = &mt6993_ammudata.ops,
	.reserved_session_num            = 2,
	.is_SLC_support                  = true,
};

static const struct of_device_id apummu_of_match[] = {
	{ .compatible = "mediatek,rv-apummu",        .data = &mt6897_drv },
	{ .compatible = "mediatek,rv-apummu-mt6989", .data = &mt6989_drv },
	{ .compatible = "mediatek,rv-apummu-mt6878", .data = &mt6878_drv },
	{ .compatible = "mediatek,rv-apummu-mt6991", .data = &mt6991_drv },
	{ .compatible = "mediatek,rv-apummu-mt6899", .data = &mt6899_drv },
	{ .compatible = "mediatek,rv-apummu-mt6993", .data = &mt6993_drv },
	{ /* end of list */ },
};

MODULE_DEVICE_TABLE(of, apummu_of_match);

const struct of_device_id *apummu_get_of_device_id(void)
{
	return apummu_of_match;
}
