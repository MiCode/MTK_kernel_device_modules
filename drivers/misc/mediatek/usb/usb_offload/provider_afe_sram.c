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
int dyn_cnt[MEM_TYPE_NUM];

struct afe_memory {
	dma_addr_t phys_addr;
	unsigned char *vir_addr;
	unsigned int size;
	u32 type;
	struct list_head list;
};

#define MAX_INFO_LEN 50
static char parse_info[MAX_INFO_LEN];
static char *parse_afe_memory(struct afe_memory *afe)
{
	int n;

	n = snprintf(parse_info, MAX_INFO_LEN, "afe:%p virt:%p phys:0x%llx size:%d\n",
		afe, afe->vir_addr, afe->phys_addr, afe->size);
	parse_info[n < MEM_TYPE_NUM ? n : MEM_TYPE_NUM - 1] = '\0';

	return parse_info;
}

struct afe_memory *rsv_afe;

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

static int afe_init(struct device *dev)
{
	int i, ret = 0;

	afe_intf = mtk_audio_usb_offload_register_ops(uodev->dev);
	if (!afe_intf) {
		USB_OFFLOAD_ERR("(%s) not support audio interface\n", afe_get_name());
		ret = -EOPNOTSUPP;
		goto error;
	}

	for (i = 0; i < MEM_TYPE_NUM; i++)
		dyn_cnt[i] = 0;
error:
	return ret;
}

static int afe_free_dyn(struct device *dev, dma_addr_t addr)
{
	struct afe_memory *afe;
	int type;

	if (!afe_intf || !afe_intf->ops->free_sram) {
		USB_OFFLOAD_ERR("(%s) not support dynamic free\n", afe_get_name());
		return -EOPNOTSUPP;
	}

	if (afe_intf->ops->free_sram(addr))
		return -EINVAL;

	afe = find_afe_memory(addr);
	if (afe) {
		USB_OFFLOAD_MEM_DBG("(%s) free [%s]\n", afe_get_name(), parse_afe_memory(afe));
		iounmap((void *)afe->vir_addr);
		type = remove_afe_memory(addr);
		if (type >= 0 && type < MEM_TYPE_NUM)
			dyn_cnt[type]--;
	}

	return 0;
}

static void *afe_alloc_dyn(struct device *dev,
	dma_addr_t *phys_addr, unsigned int size, int align)
{
	struct mtk_audio_usb_mem *audio_mem;
	struct afe_memory *afe;
	void *vir;
	u32 type;

	if (!afe_intf || !afe_intf->ops->allocate_sram) {
		USB_OFFLOAD_ERR("(%s) not support dynamic allocate\n", afe_get_name());
		return NULL;
	}

	audio_mem = afe_intf->ops->allocate_sram(size);
	if (!audio_mem) {
		USB_OFFLOAD_ERR("(%s) space is insufficient\n", afe_get_name());
		return NULL;
	}

	afe = new_afe_memory(audio_mem->phys_addr, audio_mem->type);
	if (afe) {
		type = audio_mem->type;
		if (type >= 0 && type < MEM_TYPE_NUM)
			dyn_cnt[type]++;
		*phys_addr = audio_mem->phys_addr;
		vir = (unsigned char *)ioremap_wc(audio_mem->phys_addr, size);
		afe->vir_addr = vir;
		afe->size = size;
		return afe->vir_addr;
	} else {
		afe_free_dyn(dev, audio_mem->phys_addr);
		USB_OFFLOAD_ERR("(%s) fail creating afe instance\n", afe_get_name());
		return NULL;
	}
}

static int afe_power_ctrl(struct device *dev, bool is_on)
{
	int i, ret = 0;

	if (!afe_intf || !afe_intf->ops->pm_runtime_control){
		USB_OFFLOAD_ERR("(%s) not support power control\n", afe_get_name());
		return -EOPNOTSUPP;
	}

	for (i = 0; i < MEM_TYPE_NUM; i++) {
		if (dyn_cnt[i])
			ret |= afe_intf->ops->pm_runtime_control(i, is_on);
	}

	return ret;
}

static int afe_init_rsv(struct device *dev,
	struct uo_rsv_region *rsv_region, unsigned int size, int min_order)
{
	dma_addr_t phy_addr;
	void *vir;
	int ret = 0;

	if (rsv_region->is_valid)
		return 0;

	vir = afe_alloc_dyn(dev, &phy_addr, size, -1);
	if (!vir) {
		USB_OFFLOAD_ERR("(%s) fail allocate rsv_region\n", afe_get_name());
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

	rsv_afe = find_afe_memory(phy_addr);
	if (!rsv_afe)
		USB_OFFLOAD_ERR("(%s) fail creating afe instance\n", afe_get_name());

error:
	return ret;
}

static int afe_deinit_rsv(struct device *dev, struct uo_rsv_region *rsv_region)
{
	int ret = 0;

	ret = afe_free_dyn(dev, rsv_region->physical);
	if (ret)
		goto error;

	uo_deinit_rsv_pool(dev, rsv_region);
	uo_rst_rsv_region(rsv_region);
	rsv_afe = NULL;
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
};