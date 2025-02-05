// SPDX-License-Identifier: GPL-2.0
/*
 * iMTK USB Offload AFE Sram Provider
 * *
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Yu-chen.Liu <yu-chen.liu@mediatek.com>
 */
#include "usb_offload.h"
#include "mtk-usb-offload-ops.h"

static struct mtk_audio_usb_offload *afe_intf;

/* afe may return serveral types of memory,
 * 0: afe sram
 * 1: adsp sram
 * 2: slb sram
 */
static bool allow_type[MEM_TYPE_NUM] = {true, false, false};

struct afe_memory {
	dma_addr_t phys_addr;
	unsigned char *vir_addr;
	unsigned int size;
	u32 type;
	struct list_head list;
};

/* store memory allocate via afe_alloc_dyn */
LIST_HEAD(dyn_list);

static struct afe_memory *new_afe_memory(dma_addr_t phys_addr, u32 type)
{
	struct afe_memory *afe;

	afe = kzalloc(sizeof(struct afe_memory), GFP_KERNEL);
	if (!afe)
		return NULL;

	afe->phys_addr = phys_addr;
	afe->type = type;
	list_add_tail(&afe->list, &dyn_list);

	return afe;
}

static struct afe_memory *find_afe_memory(dma_addr_t phys_addr)
{
	struct afe_memory *pos;
	bool found = false;

	list_for_each_entry(pos, &dyn_list, list) {
		if (pos->phys_addr == phys_addr) {
			found = true;
			goto found_afe;
		}
	}

found_afe:
	return found ? pos : NULL;
}

static int remove_afe_memory(dma_addr_t phys_addr)
{
	struct afe_memory *afe;
	u32 type;

	afe = find_afe_memory(phys_addr);
	if (afe) {
		list_del(&afe->list);
		type = afe->type;
		kfree(afe);
		return type;
	} else
		return -EINVAL;
}

static char *afe_get_name(void)
{
	return "Audio_SRAM";
}

static int afe_init(struct uo_provider *itself)
{
	int ret = 0;

	// workaround for switching to ddk
	/*afe_intf = mtk_audio_usb_offload_register_ops(itself->dev);*/
	afe_intf = NULL;
	if (!afe_intf) {
		USB_OFFLOAD_ERR("[%s] not support audio interface\n", afe_get_name());
		ret = -EOPNOTSUPP;
		goto error;
	}

	itself->type = UO_SOURCE_AFE_SRAM;
error:
	return ret;
}

static int afe_free_dyn(struct uo_provider *itself, dma_addr_t addr)
{
	struct afe_memory *afe;

	if (!afe_intf || !afe_intf->ops->free_sram) {
		USB_OFFLOAD_ERR("[%s] not support dynamic free\n", afe_get_name());
		return -EOPNOTSUPP;
	}

	afe = find_afe_memory(addr);
	if (!afe) {
		USB_OFFLOAD_ERR("[%s] fail to find afe (phys:0x%llx)\n", afe_get_name(), addr);
		return -EINVAL;
	}

	if (afe_intf->ops->free_sram(addr)) {
		USB_OFFLOAD_ERR("[%s] fail to free (phys:0x%llx)\n", afe_get_name(), addr);
		return -EINVAL;
	}

	USB_OFFLOAD_MEM_DBG("[%s] free [afe:%p virt:%p phys:0x%llx size:%d]\n",
		afe_get_name(), afe, afe->vir_addr, afe->phys_addr, afe->size);
	iounmap((void *)afe->vir_addr);
	remove_afe_memory(addr);

	return 0;
}

static void *afe_alloc_dyn(struct uo_provider *itself,
	dma_addr_t *phys_addr, unsigned int size, int align)
{
	struct mtk_audio_usb_mem *audio_mem;
	struct afe_memory *afe;
	void *vir;
	u32 type;

	if (!afe_intf || !afe_intf->ops->allocate_sram) {
		USB_OFFLOAD_ERR("[%s] not support dynamic allocate\n", afe_get_name());
		return NULL;
	}

	audio_mem = afe_intf->ops->allocate_sram(size);
	if (!audio_mem) {
		USB_OFFLOAD_ERR("[%s] fail allocating (size:%d align:%d)\n",
			afe_get_name(), size, align);
		return NULL;
	}

	type = audio_mem->type;
	if (!allow_type[type]) {
		USB_OFFLOAD_ERR("[%s] invalid type:%d\n", afe_get_name(), type);
		goto allocate_fail;
	}

	afe = new_afe_memory(audio_mem->phys_addr, audio_mem->type);
	if (afe) {
		*phys_addr = audio_mem->phys_addr;
		vir = (unsigned char *)ioremap_wc(audio_mem->phys_addr, size);
		afe->vir_addr = vir;
		afe->size = size;
		USB_OFFLOAD_MEM_DBG("[%s] allocate [afe:%p virt:%p phys:0x%llx size:%d]\n",
			afe_get_name(), afe, afe->vir_addr, afe->phys_addr, afe->size);
		return afe->vir_addr;
	} else
		USB_OFFLOAD_ERR("[%s] fail creating afe\n", afe_get_name());

allocate_fail:
	afe_free_dyn(itself, audio_mem->phys_addr);
	return NULL;
}

static int afe_power_ctrl(struct uo_provider *itself, bool is_on)
{
	int i, ret = 0;

	if (!afe_intf || !afe_intf->ops->pm_runtime_control){
		USB_OFFLOAD_ERR("[%s] not support power control\n", afe_get_name());
		return -EOPNOTSUPP;
	}

	for (i = 0; i < MEM_TYPE_NUM; i++) {
		if (allow_type[i])
			ret |= afe_intf->ops->pm_runtime_control(i, is_on);
	}

	return ret;
}

static int afe_init_rsv(struct uo_provider *itself, unsigned int size, int min_order)
{
	struct uo_rsv_region *rsv_region = &itself->rsv_region;
	dma_addr_t phy_addr;
	void *vir;
	int ret = 0;

	if (rsv_region->is_valid)
		return 0;

	vir = afe_alloc_dyn(itself, &phy_addr, size, -1);
	if (!vir) {
		USB_OFFLOAD_ERR("[%s] fail allocate rsv_region\n", afe_get_name());
		ret = -ENOMEM;
		goto allocate_fail;
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
	afe_free_dyn(itself, phy_addr);
allocate_fail:
	return ret;
}

static int afe_deinit_rsv(struct uo_provider *itself)
{
	struct uo_rsv_region *rsv_region = &itself->rsv_region;
	int ret = 0;

	ret = afe_free_dyn(itself, rsv_region->physical);
	if (ret)
		goto error;

	uo_deinit_rsv_pool(itself);
	uo_rst_rsv_region(rsv_region);
error:
	return ret;
}

struct uo_provider_ops uo_afe_sram_ops = {
	.get_name   = afe_get_name,
	.init       = afe_init,
	.alloc_dyn  = afe_alloc_dyn,
	.free_dyn   = afe_free_dyn,
	.power_ctrl = afe_power_ctrl,
	.init_rsv   = afe_init_rsv,
	.deinit_rsv = afe_deinit_rsv,
	.alloc_rsv  = uo_generic_alloc_rsv,
	.free_rsv   = uo_generic_free_rsv,
	.mpu_region = uo_generic_mpu_region,
};