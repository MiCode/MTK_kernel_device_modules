// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "apummu_cmn.h"
#include "apummu_tbl.h"
#include "apummu_plat.h"
#include "apummu_drv.h"

int apummu_plat_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct apummu_plat *aplat;
	struct apummu_dev_info *adv = platform_get_drvdata(pdev);
	int ret = 0;

	if (!adv) {
		AMMU_LOG_ERR("No apummu_dev_info!\n");
		ret = -EINVAL;
		goto out;
	}

	aplat = (struct apummu_plat *) of_device_get_match_data(dev);
	if (!aplat) {
		AMMU_LOG_ERR("No apummu_plat!\n");
		ret = -EINVAL;
		goto out;
	}

	/* get platform data */
	AMMU_LOG_INFO("slb_wait_time: %d\n", aplat->slb_wait_time);
	AMMU_LOG_INFO("encode_offset: 0x%x\n", aplat->encode_offset);
	AMMU_LOG_INFO("address_bits: %d\n", aplat->address_bits);
	AMMU_LOG_INFO("is_general_SLB_support: %d\n", aplat->is_general_SLB_support);
	AMMU_LOG_INFO("is_ASE_support: %d\n", aplat->is_ASE_support);

	adv->plat.slb_wait_time = aplat->slb_wait_time;
	adv->plat.encode_offset = aplat->encode_offset;
	adv->plat.address_bits = aplat->address_bits;
	adv->plat.is_general_SLB_support = aplat->is_general_SLB_support;
	adv->plat.alloc_DRAM_FB_in_session_create = aplat->alloc_DRAM_FB_in_session_create;
	adv->plat.is_ASE_support = aplat->is_ASE_support;
	adv->plat.reserved_session_num = aplat->reserved_session_num;
	adv->plat.is_SLC_support = aplat->is_SLC_support;
	ammu_set_hw_ops(aplat->hw_ops);
	mutex_init(&adv->plat.slb_mtx);

out:
	return ret;
}

