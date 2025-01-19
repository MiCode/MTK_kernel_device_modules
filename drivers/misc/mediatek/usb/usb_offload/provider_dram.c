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
#include <scp.h>
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

static int dram_init(struct device *dev)
{
	USB_OFFLOAD_MEM_DBG("(%s) unnecessary to init dram\n", dram_get_name());
	return 0;
}

static void *dram_alloc_dyn(struct device *dev,
	dma_addr_t *phys_addr, unsigned int size, int align)
{
	USB_OFFLOAD_ERR("(%s) doesn't support dynamic alloc\n", dram_get_name());
	return NULL;
}

static int dram_free_dyn(struct device *dev, dma_addr_t addr)
{
	USB_OFFLOAD_ERR("(%s) not support dynamic free\n", dram_get_name());
	return -EOPNOTSUPP;
}

static int dram_power_ctrl(struct device *dev, bool is_on)
{
	USB_OFFLOAD_MEM_DBG("(%s) unnecessary to control dram power\n", dram_get_name());
	return 0;
}

static int dram_init_rsv(struct device *dev,
	struct uo_rsv_region *rsv_region, unsigned int size, int min_order)
{
	int ret = 0, dsp_type;

	dsp_type = get_adsp_type();
	switch (dsp_type) {
	case ADSP_TYPE_HIFI3:
		ret = from_adsp(dev, rsv_region);
		break;
#if 0
    case ADSP_TYPE_RV55:
		ret = from_scp(dev, rsv_region);
		break;
#endif
    default:
		ret = -ENOMEM;
		USB_OFFLOAD_ERR("(%s) fail to query reserved dram, dsp_type:%d\n",
			dram_get_name(), dsp_type);
        break;
    }

	if (!ret)
		ret = uo_init_rsv_pool(dev, rsv_region, min_order);

	return ret;
}


static int dram_deinit_rsv(struct device *dev, struct uo_rsv_region *rsv_region)
{
	uo_deinit_rsv_pool(dev, rsv_region);
	uo_rst_rsv_region(rsv_region);
	return 0;
}

struct uo_provider_ops uo_dram_ops = {
	.get_name   = dram_get_name,
	.init       = dram_init,
	.alloc_dyn  = dram_alloc_dyn,
	.free_dyn   = dram_free_dyn,
	.power_ctrl = dram_power_ctrl,
	.init_rsv   = dram_init_rsv,
	.deinit_rsv = dram_deinit_rsv,
	.alloc_rsv  = uo_generic_alloc_rsv,
	.free_rsv   = uo_generic_free_rsv,
};