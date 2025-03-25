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
	enum uo_provider_type id, struct uo_provider_ops *ops)
{
	if (!provider) {
		USB_OFFLOAD_ERR("invalid provider, id:%d\n", id);
		return -EINVAL;
	}

	provider->dev = dev;
	provider->id = id;
	provider->is_init = false;
	provider->struct_cnt = 0;

	provider->ops.init = ops->init;
	provider->ops.alloc_dyn = ops->alloc_dyn;
	provider->ops.free_dyn = ops->free_dyn;
	provider->ops.power_ctrl = ops->power_ctrl;
	provider->ops.mpu_region = ops->mpu_region;

	provider->ops.init_rsv = ops->init_rsv;
	provider->ops.deinit_rsv = ops->deinit_rsv;
	provider->ops.alloc_rsv = ops->alloc_rsv;
	provider->ops.free_rsv = ops->free_rsv;

	provider->ops.get_name = ops->get_name;

	uo_rst_rsv_region(&provider->rsv_region);

	return 0;
}

char *uop_get_name(struct uo_provider *provider)
{

	if (unlikely(!provider) || !provider->ops.get_name)
		return "unknown";

	return provider->ops.get_name();
}

int uop_init(struct uo_provider *provider)
{
	int ret;

	if (unlikely(!provider)) {
		ret = -EOPNOTSUPP;
		goto fail;
	}

	if (!provider->ops.init) {
		USB_OFFLOAD_INFO("[%s] unsupport init()\n", uop_get_name(provider));
		ret = -EOPNOTSUPP;
		goto fail;
	}

	ret = provider->ops.init(provider);
	if (!ret)
		provider->is_init = true;
fail:
	return ret;
}

void *uop_alloc_dyn(struct uo_provider *provider,
	dma_addr_t *phy, unsigned int size, int align)
{
	if (unlikely(!provider || !provider->is_init)) {
		goto fail;
	}

	if (!provider->ops.alloc_dyn) {
		USB_OFFLOAD_INFO("[%s] unsupport alloc_dyn()\n", uop_get_name(provider));
		goto fail;
	}

	return provider->ops.alloc_dyn(provider, phy, size, align);
fail:
	return NULL;
}

int uop_free_dyn(struct uo_provider *provider, dma_addr_t phy_addr)
{
	int ret;

	if (unlikely(!provider || !provider->is_init)) {
		ret = -EOPNOTSUPP;
		goto fail;
	}

	if (!provider->ops.free_dyn) {
		USB_OFFLOAD_INFO("[%s] unsupport free_dyn()\n", uop_get_name(provider));
		ret = -EOPNOTSUPP;
		goto fail;
	}

	ret = provider->ops.free_dyn(provider, phy_addr);
fail:
	return ret;
}

int uop_pwr_ctrl(struct uo_provider *provider, bool is_on)
{
	int ret;

	if (unlikely(!provider || !provider->is_init)) {
		ret = -EOPNOTSUPP;
		goto fail;
	}

	if (!provider->ops.power_ctrl) {
		USB_OFFLOAD_INFO("[%s] unsupport power_ctrl()\n", uop_get_name(provider));
		ret = -EOPNOTSUPP;
		goto fail;
	}

	ret = provider->ops.power_ctrl(provider, is_on);
fail:
	return ret;
}

int uop_init_rsv(struct uo_provider *provider,
	unsigned int size, int min_order)
{
	int ret;

	if (unlikely(!provider || !provider->is_init)) {
		ret = -EOPNOTSUPP;
		goto fail;
	}

	if (!provider->ops.power_ctrl) {
		USB_OFFLOAD_INFO("[%s] unsupport init_rsv()\n", uop_get_name(provider));
		ret = -EOPNOTSUPP;
		goto fail;
	}

	ret = provider->ops.init_rsv(provider, size, min_order);
fail:
	return ret;
}

int uop_deinit_rsv(struct uo_provider *provider)
{
	int ret;

	if (unlikely(!provider || !provider->is_init)) {
		ret = -EOPNOTSUPP;
		goto fail;
	}

	if (!provider->ops.deinit_rsv) {
		USB_OFFLOAD_INFO("[%s] unsupport deinit_rsv()\n", uop_get_name(provider));
		ret = -EOPNOTSUPP;
		goto fail;
	}

	ret = provider->ops.deinit_rsv(provider);
fail:
	return ret;
}

void *uop_alloc_rsv(struct uo_provider *provider,
					dma_addr_t *phy, unsigned int size, int align)
{
	if (unlikely(!provider || !provider->is_init))
		goto fail;

	if (!provider->ops.alloc_rsv) {
		USB_OFFLOAD_INFO("[%s] unsupport alloc_rsv()\n", uop_get_name(provider));
		goto fail;
	}

	return provider->ops.alloc_rsv(provider, phy, size, align);
fail:
	return NULL;
}

int uop_free_rsv(struct uo_provider *provider, void *vir, unsigned int size)
{
	int ret;

	if (unlikely(!provider || !provider->is_init)) {
		ret = -EOPNOTSUPP;
		goto fail;
	}

	if (!provider->ops.free_rsv) {
		USB_OFFLOAD_INFO("[%s] unsupport free_rsv()\n", uop_get_name(provider));
		ret = -EOPNOTSUPP;
		goto fail;
	}

	ret = provider->ops.free_rsv(provider, vir, size);
fail:
	return ret;
}

unsigned int uop_mpu_region(struct uo_provider *provider, dma_addr_t *phy)
{
	if (unlikely(!provider || !provider->is_init))
		goto fail;

	if (!provider->ops.mpu_region) {
		USB_OFFLOAD_INFO("[%s] unsupport mpu_region()\n", uop_get_name(provider));
		goto fail;
	}

	return provider->ops.mpu_region(provider, phy);
fail:
	return 0;
}

void uo_rst_rsv_region(struct uo_rsv_region *rsv_region)
{
	rsv_region->physical = 0;
	rsv_region->virtual = NULL;
	rsv_region->size = 0;
	rsv_region->is_valid = false;
	rsv_region->pool = NULL;
}

int uo_init_rsv_pool(struct uo_provider *itself, int min_alloc_order)
{
	struct uo_rsv_region *rsv_region;
	struct gen_pool *pool;
	unsigned long vir;
	phys_addr_t phys;
	size_t size;
	int ret = 0;

	if (unlikely(!itself)) {
		USB_OFFLOAD_ERR("weird condition....\n");
		ret = -EINVAL;
		goto error;
	}
	rsv_region = &itself->rsv_region;;

	phys = (phys_addr_t)rsv_region->physical;
	vir = (unsigned long)rsv_region->virtual;
	size = rsv_region->size;
	pool = gen_pool_create(min_alloc_order, -1);
	if (!pool || !vir || !size) {
		USB_OFFLOAD_ERR("[%s] fail to create pool\n", uop_get_name(itself));
		ret = -ENOMEM;
		goto error;
	}

	ret = gen_pool_add_virt(pool, vir, phys, size, -1);
	if (ret) {
		USB_OFFLOAD_ERR("[%s] fail add to pool%p\n", uop_get_name(itself), pool);
		goto error;
	}

	if (rsv_region->pool) {
		USB_OFFLOAD_INFO("[%s] destroy pool%p\n", uop_get_name(itself), rsv_region->pool);
		gen_pool_destroy(rsv_region->pool);
	}

	rsv_region->pool = pool;
error:
	return ret;
}

void uo_deinit_rsv_pool(struct uo_provider *itself)
{
	struct uo_rsv_region *rsv_region;

	if (unlikely(!itself)) {
		USB_OFFLOAD_ERR("weird condition....\n");
		return;
	}
	rsv_region = &itself->rsv_region;

	if (rsv_region->pool) {
		USB_OFFLOAD_MEM_DBG("[%s] destroy pool%p\n", uop_get_name(itself), rsv_region->pool);
		gen_pool_destroy(rsv_region->pool);
	}
}

int uo_generic_free_rsv(struct uo_provider *itself, void *vir, unsigned int size)
{
	struct uo_rsv_region *rsv_region;
	struct gen_pool *pool;
	int ret = 0;

	if (unlikely(!itself)) {
		USB_OFFLOAD_ERR("weird condition....\n");
		ret = -EINVAL;
		goto error;
	}
	rsv_region = &itself->rsv_region;

	if (!rsv_region->is_valid) {
		USB_OFFLOAD_ERR("[%s] invalid rsv_region\n", uop_get_name(itself));
		ret = -EINVAL;
		goto error;
	}

	pool = rsv_region->pool;
	if (!pool) {
		USB_OFFLOAD_ERR("[%s] invalid pool\n", uop_get_name(itself));
		ret = -EINVAL;
		goto error;
	}

	if (!gen_pool_has_addr(pool, (unsigned long)vir, (size_t)size)) {
		USB_OFFLOAD_ERR("[%s] virt:%p is not in pool%p\n", uop_get_name(itself), vir, pool);
		ret = -EINVAL;
		goto error;
	}

	gen_pool_free(pool, (unsigned long)vir, size);

error:
	return ret;
}

void *uo_generic_alloc_rsv(struct uo_provider *itself,
	dma_addr_t *phy, unsigned int size, int align)
{
	struct uo_rsv_region *rsv_region;
	struct gen_pool *pool;
	void *virt = NULL;

	if (unlikely(!itself)) {
		USB_OFFLOAD_ERR("weird condition....\n");
		goto error;
	}

	rsv_region = &itself->rsv_region;
	if (!rsv_region->is_valid) {
		USB_OFFLOAD_ERR("[%s] invalid rsv_region\n", uop_get_name(itself));
		goto error;
	}

	pool = rsv_region->pool;
	if (!pool) {
		USB_OFFLOAD_ERR("[%s] invalid pool\n", uop_get_name(itself));
		goto error;
	}

	virt = gen_pool_dma_zalloc_align(pool, size, phy, align);
	if (!virt)
		goto error;

	return virt;

error:
	USB_OFFLOAD_ERR("[%s] fail allocating (size:%d align:%d)\n",
		uop_get_name(itself), size, align);
	return NULL;
}

/* return whole reserved region */
unsigned int uo_generic_mpu_region(struct uo_provider *itself, dma_addr_t *phys)
{
	struct uo_rsv_region *rsv_region;
	unsigned int size = 0;

	*phys = 0;

	if (unlikely(!itself)) {
		USB_OFFLOAD_ERR("weird condition....\n");
		goto error;
	}

	rsv_region = &itself->rsv_region;
	if (rsv_region->is_valid) {
		*phys = rsv_region->physical;
		size = rsv_region->size;
	}

error:
	return size;
}

struct uo_struct_info {
	u32 mask;
	u32 shift;
	u32 max;
	char *name;
};

static struct uo_struct_info str_info[UO_STRUCT_NUM] = {
	{ /* UO_STRUCT_DCBAA */
		.mask  = GENMASK(1, 0),
		.shift = 0x0,
		.max = BUF_DCBAA_SIZE, /* max: 1 dcbaa*/
		.name  = "DCBAA",
	},
	{ /* UO_STRUCT_CTX */
		.mask  = GENMASK(7, 2),
		.shift = 2,
		.max = BUF_CTX_SIZE, /* max: 31 contextes */
		.name  = "CONTEXT",
	},
	{ /* UO_STRUCT_ERST */
		.mask  = GENMASK(9, 8),
		.shift = 8,
		.max = BUF_ERST_SIZE, /* max: 1 event ring table */
		.name  = "ERST",
	},
	{ /* UO_STRUCT_EVRING */
		.mask  = GENMASK(13, 12),
		.shift = 12,
		.max = BUF_EV_RING_SIZE, /* max: 1 event ring*/
		.name  = "EV_RING",
	},
	{ /* UO_STRUCT_TRRING */
		.mask  = GENMASK(23, 16),
		.shift = 16,
		.max = BUF_TR_RING_SIZE, /* max: 62 trasnfer ring */
		.name  = "TR_RING",
	},
	{ /* UO_STRUCT_URB */
		.mask  = GENMASK(25, 24),
		.shift = 24,
		.max = BUF_URB_SIZE, /* max: 5 urbs */
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

	if (!provider)
		return "unknow";

	n = snprintf(parse_info, PARSE_INFO_LEN, "cnt:%d, dcbaa:%d ctx:%d erst:%d ev:%d tr:%d urb:%d",
		provider->struct_cnt,
		(provider->struct_cnt & str_info[UO_STRUCT_DCBAA].mask) >> str_info[UO_STRUCT_DCBAA].shift,
		(provider->struct_cnt & str_info[UO_STRUCT_CTX].mask)>> str_info[UO_STRUCT_CTX].shift,
		(provider->struct_cnt & str_info[UO_STRUCT_ERST].mask)>> str_info[UO_STRUCT_ERST].shift,
		(provider->struct_cnt & str_info[UO_STRUCT_EVRING].mask)>> str_info[UO_STRUCT_EVRING].shift,
		(provider->struct_cnt & str_info[UO_STRUCT_TRRING].mask)>> str_info[UO_STRUCT_TRRING].shift,
		(provider->struct_cnt & str_info[UO_STRUCT_URB].mask)>> str_info[UO_STRUCT_URB].shift);
	parse_info[n < PARSE_INFO_LEN ? n : PARSE_INFO_LEN - 1] = '\0';

	return parse_info;
}

u32 uo_get_cnt_power_sensitive(struct uo_provider *provider)
{
	int i;
	u32 value;

	if (!provider || !provider->is_init)
		return 0;

	/* erase dcbaa & ctx count */
	value = provider->struct_cnt;
	for (i = 0; i <= UO_STRUCT_CTX && i < UO_STRUCT_NUM; i++)
		value &= ~(str_info[i].mask);

	return value;
}

int uop_increase_cnt(struct uo_provider *provider, enum uo_struct type)
{
	struct uo_struct_info *info;
	u32 cnt;

	if (type >= UO_STRUCT_NUM) {
		USB_OFFLOAD_ERR("invalid input, type:%d\n", type);
		return -EINVAL;
	}

	info = &str_info[(int)type];

	cnt = ((provider->struct_cnt & info->mask) >> info->shift);
	if ((cnt + 1) > info->max)
		goto error;
	cnt++;

	provider->struct_cnt &= ~(info->mask);
	provider->struct_cnt |= ((cnt << info->shift) & info->mask);

	return 0;
error:
	USB_OFFLOAD_ERR("fail to increase, (%s, %s) cnt:%d max:%d\n",
		uop_get_name(provider), uo_struct_name(type), cnt, info->max);
	return -EINVAL;
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

	cnt = ((provider->struct_cnt & info->mask) >> info->shift);
	if (!cnt)
		goto error;
	cnt--;

	provider->struct_cnt &= ~(info->mask);
	provider->struct_cnt |= ((cnt << info->shift) & info->mask);

	return;

error:
	USB_OFFLOAD_ERR("fail to decrease, (%s, %s) cnt:%d max:%d\n",
		uop_get_name(provider), uo_struct_name(type), cnt, info->max);
}
