// SPDX-License-Identifier: GPL-2.0
/*
 * MTK USB Offload USB Sram Provider
 * *
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Yu-chen.Liu <yu-chen.liu@mediatek.com>
 */

#include "usb_offload.h"

#define DEBUG_VIA_AFE
#ifdef DEBUG_VIA_AFE
#include "mtk-usb-offload-ops.h"
static struct mtk_audio_usb_offload *afe_intf;
#endif
static void *(*allocate_cb)(dma_addr_t *phys_addr, unsigned int size, int align);
static int (*free_cb)(dma_addr_t phys_addr);

int mtk_register_usb_sram_ops(
	void *(*allocate)(dma_addr_t *phys_addr, unsigned int size, int align),
	int (*free)(dma_addr_t phys_addr))
{
	if (!allocate || !free)
		return -EINVAL;

	allocate_cb = allocate;
	free_cb = free;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_register_usb_sram_ops);

static char *usb_sram_get_name(void)
{
	return "USB_SRAM";
}

static int usb_sram_init(struct device *dev)
{
	if (!of_find_compatible_node(NULL, NULL, "mediatek,usb-sram")) {
		USB_OFFLOAD_ERR("usb-sram didn't exist\n");
		return -EOPNOTSUPP;
	}

#ifdef DEBUG_VIA_AFE
	afe_intf = mtk_audio_usb_offload_register_ops(uodev->dev);
	if (!afe_intf) {
		USB_OFFLOAD_ERR("not support afe sram interface\n");
		return -EOPNOTSUPP;
	}
#endif
	return 0;
}

static void *usb_sram_alloc_dyn(struct device *dev,
    dma_addr_t *phys_addr, unsigned int size, int align)
{
	void *virt_addr;

	if (!allocate_cb)
		return NULL;

	virt_addr = allocate_cb(phys_addr, size, align);
	if (!virt_addr) {
		USB_OFFLOAD_ERR("(%s) fail allocating (size:%d align:%d)\n",
			usb_sram_get_name(), size, align);
		return NULL;
	}

	USB_OFFLOAD_MEM_DBG("(%s) success allocate (vir:%p phys:0x%llx size:%d)\n",
		usb_sram_get_name(), virt_addr, *phys_addr, size);

	return virt_addr;
}

static int usb_sram_free_dyn(struct device *dev, dma_addr_t addr)
{
	if (!free_cb)
		return -EINVAL;

	return free_cb(addr);
}

static int usb_sram_power_ctrl(struct device *dev, bool is_on)
{
#ifdef DEBUG_VIA_AFE
	if (is_on)
		afe_intf->ops->pm_runtime_control(0, is_on);
#endif
	return 0;
}

static int usb_sram_init_rsv(struct device *dev,
    struct uo_rsv_region *rsv_region, unsigned int size, int min_order)
{
	dma_addr_t phy_addr;
	void *vir;
	int ret = 0;

	if (rsv_region->is_valid)
		return 0;

	vir = usb_sram_alloc_dyn(dev, &phy_addr, size, -1);
	if (!vir) {
		ret = -ENOMEM;
		goto error;
	}

	rsv_region->physical = phy_addr;
	rsv_region->virtual = vir;
	rsv_region->size = (unsigned long long)size;
	rsv_region->is_valid = true;

	ret = uo_init_rsv_pool(dev, rsv_region, min_order);
	if (ret)
		goto error;
error:
	return ret;
}

static int usb_sram_deinit_rsv(struct device *dev, struct uo_rsv_region *rsv_region)
{
	uo_deinit_rsv_pool(dev, rsv_region);
	uo_rst_rsv_region(rsv_region);

	return 0;
}

struct uo_provider_ops uo_usb_sram_ops = {
	.get_name   = usb_sram_get_name,
	.init       = usb_sram_init,
	.alloc_dyn  = usb_sram_alloc_dyn,
	.free_dyn   = usb_sram_free_dyn,
	.power_ctrl = usb_sram_power_ctrl,
	.init_rsv   = usb_sram_init_rsv,
	.deinit_rsv = usb_sram_deinit_rsv,
	.alloc_rsv  = uo_generic_alloc_rsv,
	.free_rsv   = uo_generic_free_rsv,
};