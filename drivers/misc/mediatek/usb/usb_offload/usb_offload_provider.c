// SPDX-License-Identifier: GPL-2.0
/*
 * MTK USB Offload Memory Provider
 * *
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Yu-chen.Liu <yu-chen.liu@mediatek.com>
 */

#include <linux/genalloc.h>
#include "usb_offload.h"

int uop_register(struct device *dev, struct uo_provider *provider,
	enum uo_provider_type type, struct uo_provider_ops *ops)
{
	if (!provider) {
		USB_OFFLOAD_ERR("invalid provider, id:%d\n", type);
		return -EINVAL;
	}

	provider->dev = dev;
	provider->id = type;
	provider->is_init = false;
	provider->struct_cnt = 0;

	provider->ops.init = ops->init;
	provider->ops.alloc_dyn = ops->alloc_dyn;
	provider->ops.free_dyn = ops->free_dyn;
	provider->ops.power_ctrl = ops->power_ctrl;
	provider->ops.init_rsv = ops->init_rsv;
	provider->ops.deinit_rsv = ops->deinit_rsv;
	provider->ops.alloc_rsv = ops->alloc_rsv;
	provider->ops.free_rsv = ops->free_rsv;
	provider->ops.get_name = ops->get_name;

	uo_rst_rsv_region(&provider->rsv_region);
	provider->rsv_region.provider = provider;

	return 0;
}

char *uop_get_name(struct uo_provider *provider)
{
	if (!provider || !provider->ops.get_name)
		return "unknown";

	return provider->ops.get_name();
}

int uop_init(struct uo_provider *provider)
{
	int ret;

	if (!provider || !provider->ops.init) {
		ret = -EOPNOTSUPP;
		goto fail;
	}

	ret = provider->ops.init(provider->dev);
	if (!ret)
		provider->is_init = true;
fail:
	return ret;
}

void *uop_alloc_dyn(struct uo_provider *provider,
	dma_addr_t *phy, unsigned int size, int align)
{
	if (!provider || !provider->is_init || !provider->ops.alloc_dyn)
		goto fail;

	return provider->ops.alloc_dyn(provider->dev, phy, size, align);
fail:
	return NULL;
}

int uop_free_dyn(struct uo_provider *provider, dma_addr_t phy_addr)
{
	int ret;

	if (!provider || !provider->is_init || !provider->ops.free_dyn) {
		ret = -EOPNOTSUPP;
		goto fail;
	}

	ret = provider->ops.free_dyn(provider->dev, phy_addr);
fail:
	return ret;
}

int uop_pwr_ctrl(struct uo_provider *provider, bool is_on)
{
	int ret;

	if (!provider || !provider->is_init || !provider->ops.power_ctrl) {
		ret = -EOPNOTSUPP;
		goto fail;
	}

	ret = provider->ops.power_ctrl(provider->dev, is_on);
fail:
	return ret;
}

int uop_init_rsv(struct uo_provider *provider,
	unsigned int size, int min_order)
{
	int ret;

	if (!provider || !provider->is_init || !provider->ops.init_rsv) {
		ret = -EOPNOTSUPP;
		goto fail;
	}

	ret = provider->ops.init_rsv(provider->dev,
		&provider->rsv_region, size, min_order);
fail:
	return ret;
}

int uop_deinit_rsv(struct uo_provider *provider)
{
	int ret;

	if (!provider || !provider->is_init || !provider->ops.deinit_rsv) {
		ret = -EOPNOTSUPP;
		goto fail;
	}

	ret = provider->ops.deinit_rsv(provider->dev, &provider->rsv_region);
fail:
	return ret;
}

void *uop_alloc_rsv(struct uo_provider *provider,
					dma_addr_t *phy, unsigned int size, int align)
{
	if (!provider || !provider->is_init || !provider->ops.alloc_rsv)
		goto fail;

	return provider->ops.alloc_rsv(provider->dev,
		&provider->rsv_region, phy, size, align);
fail:
	return NULL;
}

int uop_free_rsv(struct uo_provider *provider, void *vir, unsigned int size)
{
	int ret;

	if (!provider || !provider->is_init || !provider->ops.free_rsv) {
		ret = -EOPNOTSUPP;
		goto fail;
	}

	ret = provider->ops.free_rsv(provider->dev,
		&provider->rsv_region, vir, size);
fail:
	return ret;
}


void uo_rst_rsv_region(struct uo_rsv_region *rsv_region)
{
	/* do not erase rsv_region->provider */
	rsv_region->physical = 0;
	rsv_region->virtual = NULL;
	rsv_region->size = 0;
	rsv_region->is_valid = false;
	rsv_region->pool = NULL;
}

int uo_init_rsv_pool(struct device *dev,
	struct uo_rsv_region *rsv_region, int min_alloc_order)
{
	struct uo_provider *provider;
	struct gen_pool *pool;
	unsigned long vir;
	phys_addr_t phys;
	size_t size;
	int ret = 0;

	provider = rsv_region->provider;
	if (unlikely(!rsv_region || !provider)) {
		USB_OFFLOAD_ERR("weird condition....\n");
		ret = -EINVAL;
		goto error;
	}

	if (!rsv_region->is_valid) {
		USB_OFFLOAD_ERR("[%s] invalid rsv_region\n", uop_get_name(provider));
		ret = 0;
		goto error;
	}

	phys = (phys_addr_t)rsv_region->physical;
	vir = (unsigned long)rsv_region->virtual;
	size = rsv_region->size;
	pool = gen_pool_create(min_alloc_order, -1);
	if (!pool || !vir || !size) {
		USB_OFFLOAD_ERR("[%s] fail to create pool\n", uop_get_name(provider));
		ret = -ENOMEM;
		goto error;
	}

	ret = gen_pool_add_virt(pool, vir, phys, size, -1);
	if (ret) {
		USB_OFFLOAD_ERR("[%s] fail add to pool%p\n", uop_get_name(provider), pool);
		goto error;
	}

	if (rsv_region->pool) {
		USB_OFFLOAD_INFO("[%s] destroy pool%p\n", uop_get_name(provider), rsv_region->pool);
		gen_pool_destroy(rsv_region->pool);
	}

	rsv_region->pool = pool;

error:
	return ret;
}

void uo_deinit_rsv_pool(struct device *dev, struct uo_rsv_region *rsv_region)
{
	struct uo_provider *provider;

	provider = rsv_region->provider;
	if (unlikely(!rsv_region || !provider)) {
		USB_OFFLOAD_ERR("weird condition....\n");
		return;
	}

	if (rsv_region->pool) {
		USB_OFFLOAD_MEM_DBG("[%s] destroy pool%p\n", uop_get_name(provider), rsv_region->pool);
		gen_pool_destroy(rsv_region->pool);
	}
}

int uo_generic_free_rsv(struct device *dev, struct uo_rsv_region *rsv_region,
	void *vir, unsigned int size)
{
	struct uo_provider *provider;
	struct gen_pool *pool;
	int ret = 0;

	provider = rsv_region->provider;
	if (unlikely(!rsv_region || !provider)) {
		USB_OFFLOAD_ERR("weird condition....\n");
		ret = -EINVAL;
		goto error;
	}

	if (!rsv_region->is_valid) {
		USB_OFFLOAD_ERR("[%s] invalid rsv_region\n", uop_get_name(provider));
		ret = -EINVAL;
		goto error;
	}

	pool = rsv_region->pool;
	if (!pool) {
		USB_OFFLOAD_ERR("[%s] invalid pool\n", uop_get_name(provider));
		ret = -EINVAL;
		goto error;
	}

	if (!gen_pool_has_addr(pool, (unsigned long)vir, (size_t)size)) {
		USB_OFFLOAD_ERR("[%s] virt:%p is not in pool%p\n", uop_get_name(provider), vir, pool);
		ret = -EINVAL;
		goto error;
	}

	gen_pool_free(pool, (unsigned long)vir, size);

error:
	return ret;
}

void *uo_generic_alloc_rsv(struct device *dev, struct uo_rsv_region *rsv_region,
	dma_addr_t *phy, unsigned int size, int align)
{
	struct uo_provider *provider;
	struct gen_pool *pool;

	provider = rsv_region->provider;
	if (unlikely(!rsv_region || !provider)) {
		USB_OFFLOAD_ERR("weird condition....\n");
		goto error;
	}

	pool = rsv_region->pool;
	if (!pool) {
		USB_OFFLOAD_ERR("[%s] invalid pool\n", uop_get_name(provider));
		goto error;
	}

	return gen_pool_dma_zalloc_align(pool, size, phy, align);
error:
	return NULL;
}

struct uo_struct_info {
    u8 mask;
    u8 shift;
    char *name;
};

static struct uo_struct_info str_info[UO_STRUCT_NUM] = {
    { /* UO_STRUCT_DCBAA */
        .mask  = 0x1,
        .shift = 0x0,
        .name  = "DCBAA",
    },
    { /* UO_STRUCT_CTX */
        .mask  = 0x7,
        .shift = 0x1,
        .name  = "CONTEXT",
    },
    { /* UO_STRUCT_ERST */
        .mask  = 0x1,
        .shift = 0x4,
        .name  = "ERST",
    },
    { /* UO_STRUCT_EVRING */
        .mask  = 0x7,
        .shift = 0x5,
        .name  = "EV_RING",
    },
    { /* UO_STRUCT_TRRING */
        .mask  = 0x7,
        .shift = 0x8,
        .name  = "TR_RING",
    },
    { /* UO_STRUCT_URB */
        .mask  = 0x7,
        .shift = 0xb,
        .name  = "URB",
    },
};

char *uo_struct_name(enum uo_struct type)
{
	return type < UO_STRUCT_NUM ? str_info[type].name : "unknow";
}

#define PARSE_INFO_LEN  128
static char parse_info[PARSE_INFO_LEN];

char *uo_provider_parse_count(struct uo_provider *provider)
{
	int n = 0;

	if (!provider || !provider->is_init)
		return "unknow";

	n = snprintf(parse_info, PARSE_INFO_LEN, "cnt:%d, dcbaa:%d ctx:%d erst:%d ev:%d tr:%d urb:%d",
		provider->struct_cnt,
		(provider->struct_cnt >> str_info[UO_STRUCT_DCBAA].shift) & str_info[UO_STRUCT_DCBAA].mask,
		(provider->struct_cnt >> str_info[UO_STRUCT_CTX].shift) & str_info[UO_STRUCT_CTX].mask,
		(provider->struct_cnt >> str_info[UO_STRUCT_ERST].shift) & str_info[UO_STRUCT_ERST].mask,
		(provider->struct_cnt >> str_info[UO_STRUCT_EVRING].shift) & str_info[UO_STRUCT_EVRING].mask,
		(provider->struct_cnt >> str_info[UO_STRUCT_TRRING].shift) & str_info[UO_STRUCT_TRRING].mask,
		(provider->struct_cnt >> str_info[UO_STRUCT_URB].shift) & str_info[UO_STRUCT_URB].mask);
	parse_info[n < PARSE_INFO_LEN ? n : PARSE_INFO_LEN - 1] = '\0';

	return parse_info;
}

void uop_increase_cnt(struct uo_provider *provider, enum uo_struct type)
{
	struct uo_struct_info *info;
	u32 cnt;

	if (type >= UO_STRUCT_NUM) {
		USB_OFFLOAD_ERR("invalid input, type:%d\n", type);
		return;
	}

	info = &str_info[(int)type];

	cnt = ((provider->struct_cnt >> info->shift) & info->mask);
	if ((cnt + 1) > info->mask)
		goto error;
	cnt++;

	provider->struct_cnt &= ~(info->mask << info->shift);
	provider->struct_cnt |= ((cnt & info->mask) << info->shift);

	USB_OFFLOAD_INFO("[%s] %s\n",
		uop_get_name(provider), uo_provider_parse_count(provider));

	return;
error:
	USB_OFFLOAD_ERR("fail to increase, (%s, %s) cnt:%d max:%d\n",
		uop_get_name(provider), uo_struct_name(type), cnt, info->mask);
	return;
}

void uop_decrease_cnt(struct uo_provider *provider,	enum uo_struct type)
{
	struct uo_struct_info *info;
	u32 cnt;

	if (type >= UO_STRUCT_NUM) {
		USB_OFFLOAD_ERR("invalid input, type:%d\n", type);
		return;
	}

	info = &str_info[(int)type];

	cnt = ((provider->struct_cnt >> info->shift) & info->mask);
	if (!cnt)
		goto error;
	cnt--;

	provider->struct_cnt &= ~(info->mask << info->shift);
	provider->struct_cnt |= ((cnt & info->mask) << info->shift);

	USB_OFFLOAD_INFO("[%s] %s\n",
		uop_get_name(provider), uo_provider_parse_count(provider));

	return;
error:
	USB_OFFLOAD_ERR("fail to decrease, (%s, %s) cnt:%d max:%d\n",
		uop_get_name(provider), uo_struct_name(type), cnt, info->mask);
	return;
}
