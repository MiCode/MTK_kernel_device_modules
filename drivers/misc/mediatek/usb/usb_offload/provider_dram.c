// SPDX-License-Identifier: GPL-2.0
/*
 * MTK USB Offload Dram Provider
 * *
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Yu-chen.Liu <yu-chen.liu@mediatek.com>
 */

#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
#include <adsp_helper.h>
#endif

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
#include <scp_rv.h>
#endif

#include "usb_offload.h"

static int from_adsp(struct device *dev, struct uo_rsv_region *rsv_region)
{
	if (!adsp_get_reserve_mem_phys(ADSP_XHCI_MEM_ID)) {
		USB_OFFLOAD_ERR("fail to get reserved dram\n");
		rsv_region->is_valid = false;
		return -EPROBE_DEFER;
	}
	rsv_region->physical = (dma_addr_t)adsp_get_reserve_mem_phys(ADSP_XHCI_MEM_ID);
	rsv_region->virtual = adsp_get_reserve_mem_virt(ADSP_XHCI_MEM_ID);
	rsv_region->size = adsp_get_reserve_mem_size(ADSP_XHCI_MEM_ID);
	rsv_region->is_valid = true;

	return 0;
}

#if 0
static int from_scp(struct device *dev,struct uo_rsv_region *rsv_region)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	if (!scp_get_reserve_mem_phys(SCP_XHCI_MEM_ID)) {
		USB_OFFLOAD_ERR("fail to get reserved dram\n");
		rsv_region->is_valid = false;
		return -EPROBE_DEFER;
	}
	rsv_region->phy_addr = scp_get_reserve_mem_phys(SCP_XHCI_MEM_ID);
	rsv_region->va_addr =
		(unsigned long long) scp_get_reserve_mem_virt(SCP_XHCI_MEM_ID);
	rsv_region->vir_addr = (unsigned char *)scp_get_reserve_mem_virt(SCP_XHCI_MEM_ID);
	rsv_region->size = scp_get_reserve_mem_size(SCP_XHCI_MEM_ID);
	rsv_region->is_valid = true;
	return 0;
#else
	USB_OFFLOAD_ERR("Failed to get reservied DRAM for SCP\n");
	return -ENOMEM;
#endif
}
#endif

static char *dram_get_name(void)
{
	return "DRAM";
}

static int dram_init(struct uo_provider *itself)
{
	itself->type = UO_SOURCE_DRAM;
	return 0;
}

static int dram_power_ctrl(struct uo_provider *itself, bool is_on)
{
	return 0;
}

static int dram_init_rsv(struct uo_provider *itself, unsigned int size, int min_order)
{
	struct uo_rsv_region *rsv_region = &itself->rsv_region;
	int ret = 0, dsp_type;

	dsp_type = get_adsp_type();
	switch (dsp_type) {
	case ADSP_TYPE_HIFI3:
		ret = from_adsp(itself->dev, rsv_region);
		break;
#if 0
    case ADSP_TYPE_RV55:
		ret = from_scp(itself, rsv_region);
		break;
#endif
    default:
		ret = -ENOMEM;
		USB_OFFLOAD_ERR("(%s) fail to query reserved dram, dsp_type:%d\n",
			dram_get_name(), dsp_type);
        break;
    }

	if (!ret)
		ret = uo_init_rsv_pool(itself, min_order);

	return ret;
}


static int dram_deinit_rsv(struct uo_provider *itself)
{
	uo_deinit_rsv_pool(itself);
	uo_rst_rsv_region(&itself->rsv_region);
	return 0;
}

struct uo_provider_ops uo_dram_ops = {
	.get_name   = dram_get_name,
	.init       = dram_init,
	.alloc_dyn  = NULL,
	.free_dyn   = NULL,
	.power_ctrl = dram_power_ctrl,
	.init_rsv   = dram_init_rsv,
	.deinit_rsv = dram_deinit_rsv,
	.alloc_rsv  = uo_generic_alloc_rsv,
	.free_rsv   = uo_generic_free_rsv,
	.mpu_region = uo_generic_mpu_region,
};