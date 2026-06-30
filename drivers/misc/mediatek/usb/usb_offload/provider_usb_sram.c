// SPDX-License-Identifier: GPL-2.0
/*
 * MTK USB Offload USB Sram Provider
 * *
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Yu-chen.Liu <yu-chen.liu@mediatek.com>
 */

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include "usb_offload.h"

static dma_addr_t usb_sram_addr;
static unsigned int usb_sram_size;
static void *(*allocate_cb)(dma_addr_t *phys_addr, unsigned int size, int align);
static int (*free_cb)(dma_addr_t phys_addr);

int mtk_register_usb_sram_ops(
	void *(*allocate)(dma_addr_t *phys_addr,
						unsigned int size,
						int align),
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

static int usb_sram_init(struct uo_provider *itself)
{
	struct device_node *node;
	const __be32 *regaddr_p;
	u64 regaddr64, size64;
	int ret = 0;

	usb_sram_addr = 0;
	usb_sram_size = 0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,usb-sram");
	if (!node) {
		USB_OFFLOAD_ERR("usb-sram didn't exist\n");
		return -EOPNOTSUPP;
	}

	itself->type = UO_SOURCE_USB_SRAM;

	regaddr_p = of_get_address(node, 0, &size64, NULL);
	if (!regaddr_p) {
		USB_OFFLOAD_ERR("fail get usb-sram physical address\n");
		ret = -ENODEV;
		goto put_node;
	}

	regaddr64 = of_translate_address(node, regaddr_p);
	usb_sram_addr = (dma_addr_t)regaddr64;
	usb_sram_size = (unsigned int)size64;

put_node:
	of_node_put(node);
	USB_OFFLOAD_INFO("[%s] addr:0x%llx size:%d\n", usb_sram_get_name(),
		usb_sram_addr, usb_sram_size);
	return ret;
}

static void *usb_sram_alloc_dyn(struct uo_provider *itself,
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

static int usb_sram_free_dyn(struct uo_provider *itself, dma_addr_t addr)
{
	if (!free_cb)
		return -EINVAL;

	return free_cb(addr);
}

static int usb_sram_power_ctrl(struct uo_provider *itself, bool is_on)
{
	return 0;
}

static int usb_sram_init_rsv(struct uo_provider *itself, unsigned int size, int min_order)
{
	struct uo_rsv_region *rsv_region = &itself->rsv_region;
	dma_addr_t phy_addr;
	void *vir;
	int ret = 0;

	if (rsv_region->is_valid)
		return 0;

	vir = usb_sram_alloc_dyn(itself, &phy_addr, size, -1);
	if (!vir) {
		ret = -ENOMEM;
		goto alloca_fail;
	}

	rsv_region->physical = phy_addr;
	rsv_region->virtual = vir;
	rsv_region->size = (unsigned long long)size;
	rsv_region->is_valid = true;

	ret = uo_init_rsv_pool(itself, min_order);
	if (ret)
		goto init_fail;
	return 0;

init_fail:
	usb_sram_free_dyn(itself, phy_addr);
alloca_fail:
	return ret;
}

static int usb_sram_deinit_rsv(struct uo_provider *itself)
{
	struct uo_rsv_region *rsv_region = &itself->rsv_region;
	int ret = 0;

	ret = usb_sram_free_dyn(itself, rsv_region->physical);
	if (ret)
		goto error;

	uo_deinit_rsv_pool(itself);
	uo_rst_rsv_region(&itself->rsv_region);
error:
	return ret;
}

/* return whole usb sram */
static unsigned int usb_sram_mpu_region(struct uo_provider *itself, dma_addr_t *phy)
{
	*phy = usb_sram_addr;

	return usb_sram_size;
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
	.mpu_region = usb_sram_mpu_region,
};