// SPDX-License-Identifier: GPL-2.0
/*
 * MTK USB Offload Memory Management API
 * *
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Yu-chen.Liu <yu-chen.liu@mediatek.com>
 */

#include <linux/printk.h>
#include <linux/genalloc.h>

#include "usb_offload.h"

static void provider_auto_power(enum uo_provider_type id, bool is_on);

#define PARSE_INFO_LEN  128
static char parse_info[PARSE_INFO_LEN];
char *mtk_offload_parse_buffer(struct uo_buffer *buf)
{
	struct uo_provider *provider;
	int n;

	if (buf == NULL || buf->allocated == false || buf->provider == NULL)
		return "unknown";

	provider = buf->provider;
	n = snprintf(parse_info, PARSE_INFO_LEN,
		"(%s, %s) buf:%p vir:%p phy:0x%llx size:%zu is_rsv:%d",
		uop_get_name(provider), uo_struct_name(buf->type),
		buf, buf->virt, buf->phys, buf->size, buf->is_rsv);
	parse_info[n < PARSE_INFO_LEN ? n : PARSE_INFO_LEN - 1] = '\0';

	return parse_info;
}

static struct uo_provider *get_provider(enum uo_provider_type id)
{
	if (id >= UO_PROV_NUM)
		return NULL;

	return &uodev->provider[id];
}

static bool check_provider_valid(struct uo_provider *provider)
{
	if (!provider)
		return false;

	if (!provider->is_init)
		return false;

	return true;
}

static bool is_secondary_sram(enum uo_provider_type id)
{
	return id == UO_PROV_SRAM_2 ? false : true;
}

static void reset_buffer(struct uo_buffer *buf)
{
	buf->provider = NULL;
	buf->phys = 0;
	buf->virt = NULL;
	buf->size = 0;
	buf->allocated = false;
	buf->is_rsv = false;
	buf->type = 0;
}

static int register_and_init_provider(struct device *dev,
	enum uo_provider_type id, struct uo_provider_ops *ops)
{
	struct uo_provider *provider = get_provider(id);
	int ret = 0;

	if (!provider || !ops) {
		USB_OFFLOAD_ERR("provider or ops is invalid\n");
		return -EINVAL;
	}

	ret = uop_register(dev, provider, id, ops);
	if (ret) {
		USB_OFFLOAD_ERR("fail to register provider, id:%d\n", id);
		return ret;
	}

	ret = uop_init(provider);
	if (ret) {
		USB_OFFLOAD_ERR("fail to init %s provider\n", uop_get_name(provider));
		return ret;
	}

	USB_OFFLOAD_INFO("success to register %s provider\n", uop_get_name(provider));

	return ret;
}

static struct uo_provider_ops *get_sram_ops(enum uo_source_type source)
{
	switch (source) {
	case UO_SOURCE_USB_SRAM:
		return &uo_usb_sram_ops;
	case UO_SOURCE_AFE_SRAM:
		return &uo_afe_sram_ops;
	default:
		return NULL;
	}
}

int mtk_offload_provider_register(struct usb_offload_dev *udev, enum uo_provider_type id)
{
	struct usb_offload_policy *policy = &udev->policy;
	struct uo_provider_ops *ops;

	if (!is_secondary_sram(id)) {
		USB_OFFLOAD_ERR("do not directly control secondary sram source\n");
		return -EINVAL;
	}

	switch (id) {
	case UO_PROV_DRAM:
		/* register dram provider */
		return register_and_init_provider(udev->dev, UO_PROV_DRAM, &uo_dram_ops);
		break;
	case UO_PROV_SRAM:
		/* register main sram provider */
		ops = get_sram_ops(policy->main_sram);
		if (!ops || register_and_init_provider(udev->dev, UO_PROV_SRAM, ops)) {
			USB_OFFLOAD_ERR("fail to register main sram provider\n");
			return -EINVAL;
		}

		/* register secondary sram provider */
		ops = get_sram_ops(policy->secondary_sram);
		if (!ops || register_and_init_provider(udev->dev, UO_PROV_SRAM_2, ops))
			USB_OFFLOAD_ERR("fail to register secondary sram provider\n");

		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int init_reserved_region(struct uo_provider *provider, u32 size)
{
	struct uo_rsv_region *rsv_region;
	int ret = 0;

	if (!check_provider_valid(provider)) {
		USB_OFFLOAD_ERR("provider is invalid\n");
		return -EINVAL;
	}

	rsv_region = &provider->rsv_region;
	if (rsv_region->is_valid) {
		USB_OFFLOAD_INFO("[%s] rsv_region was already init\n", uop_get_name(provider));
		return 0;
	}

	/* power on before init reserved region*/
	provider_auto_power(provider->id, true);

	if (uop_init_rsv(provider, size, MIN_USB_OFFLOAD_SHIFT)) {
		USB_OFFLOAD_ERR("[%s] fail to init rsv region\n", uop_get_name(provider));
		ret = -EINVAL;
		goto error;
	}

	USB_OFFLOAD_INFO("[%s] success to init rsv_region, phys:0x%llx size:%lld\n",
		uop_get_name(provider), rsv_region->physical, rsv_region->size);

error:
	/* then power off when we've done */
	provider_auto_power(provider->id, false);
	return ret;
}

int mtk_offload_init_rsv(struct usb_offload_dev *udev, enum uo_provider_type id)
{
	struct uo_provider *provider;
	int ret = 0;

	if (!is_secondary_sram(id)) {
		USB_OFFLOAD_ERR("do not directly control secondary sram source\n");
		return -EINVAL;
	}

	switch (id) {
	case UO_PROV_DRAM:
		provider = get_provider(UO_PROV_DRAM);
		/* don't care size of reserved dram */
		ret = init_reserved_region(provider, 0);
		break;
	case UO_PROV_SRAM:
		if (!uodev->adv_lowpwr)
			return 0;
		provider = get_provider(UO_PROV_SRAM);
		ret = init_reserved_region(provider, udev->policy.reserved_size);
		break;
	default:
		return -EOPNOTSUPP;
	}

	return ret;
}

void mtk_offload_deinit_rsv(enum uo_provider_type id)
{
	struct uo_provider *provider = get_provider(id);
	struct uo_rsv_region *rsv_region;

	if (!is_secondary_sram(id)) {
		USB_OFFLOAD_ERR("do not directly control secondary sram source\n");
		return;
	}

	if (!uodev->adv_lowpwr && id != UO_PROV_DRAM)
		return;

	if (!check_provider_valid(provider)) {
		USB_OFFLOAD_ERR("provider(id:%d) is invalid\n", id);
		return;
	}

	rsv_region = &provider->rsv_region;
	if (!rsv_region->is_valid) {
		USB_OFFLOAD_INFO("[%s] rsv_region was already deinit\n", uop_get_name(provider));
		return;
	}

	/* just in case, power on before deinit reserved region */
	provider_auto_power(id, true);

	if (uop_deinit_rsv(provider)) {
		USB_OFFLOAD_ERR("[%s] fail to deinit rsv region\n", uop_get_name(provider));
		goto error;
	}

	USB_OFFLOAD_INFO("[%s] success to deinit rsv_region\n", uop_get_name(provider));
error:
 	/* then power off*/
	provider_auto_power(id, false);
	return;
}

/* get reserved region of provider */
unsigned int mtk_offload_get_mpu_region(enum uo_provider_type id, dma_addr_t *phys)
{
	struct uo_provider *provider = get_provider(id);

	*phys = 0;

	if (!is_secondary_sram(id)) {
		USB_OFFLOAD_ERR("do not directly control secondary sram source\n");
		return 0;
	}

	if (!check_provider_valid(provider)) {
		USB_OFFLOAD_ERR("provider(id:%d) is invalid\n", id);
		return 0;
	}

	return uop_mpu_region(provider, phys);
}

/* power control exported outsiede in case there's issue in future */
void mtk_offload_provider_power(enum uo_provider_type id, bool is_on)
{
	struct uo_provider *provider = get_provider(id);

	if (!check_provider_valid(provider)) {
		USB_OFFLOAD_ERR("provider(id:%d) is invalid\n", id);
		return;
	}

	if (provider->power != is_on) {
		USB_OFFLOAD_INFO("[%s] power change to %s\n", uop_get_name(provider),
			is_on ? "on" : "off");
		provider->power = is_on;
		if (uop_pwr_ctrl(provider, is_on))
			USB_OFFLOAD_ERR("[%s] fail to control power\n", uop_get_name(provider));
	} else
		USB_OFFLOAD_MEM_DBG("[%s] power stay %s\n",	uop_get_name(provider),
			provider->power ? "on" : "off");
}

/* automatic power control according to count of structure on source */
static void provider_auto_power(enum uo_provider_type id, bool is_on)
{
	struct uo_provider *provider = get_provider(id);
	u32 cnt;

	if (!check_provider_valid(provider)) {
		USB_OFFLOAD_ERR("provider(id:%d) is invalid\n", id);
		return;
	}

	if (is_on)
		mtk_offload_provider_power(id, is_on);
	else {
		cnt = mtk_offload_provider_get_cnt(id);
		if (!cnt)
			mtk_offload_provider_power(id, is_on);
		else
			USB_OFFLOAD_MEM_DBG("[%s] still structure on it, cnt:%d\n",
				uop_get_name(provider), cnt);
	}
}

bool mtk_offload_hold_apsrc(void)
{
	struct uo_provider *provider = get_provider(UO_PROV_DRAM);
	u32 cnt;

	if (unlikely(!check_provider_valid(provider)))
		return false;

	cnt = uo_get_cnt_power_sensitive(provider);
	USB_OFFLOAD_INFO("[%s] power-sensitive-cnt:%d\n", uop_get_name(provider), cnt);

	/* if any power sensitive structure on dram, we'll hold apsrc */
	return cnt ? true : false;
}

bool mtk_offload_hold_vcore(void)
{
	struct uo_provider *provider;
	u32 cnt;
	int i = 0;

	for (i = UO_PROV_SRAM; i <= UO_PROV_SRAM_2; i++) {
		provider = get_provider(i);
		if (!check_provider_valid(provider))
			continue;
		cnt = uo_get_cnt_power_sensitive(provider);
		USB_OFFLOAD_INFO("[%s] power-sensitive-cnt:%d\n", uop_get_name(provider), cnt);

		/* if any power sensitive structure on afe sram, we'll hold vcore */
		if (cnt && provider->type == UO_SOURCE_AFE_SRAM)
			return true;
	}

	return false;
}

/* get structure count of provider */
u32 mtk_offload_provider_get_cnt(enum uo_provider_type id)
{
	struct uo_provider *provider = get_provider(id);

	if (!provider) {
		USB_OFFLOAD_ERR("provider(id:%d) is invalid\n", id);
		return 0;
	}

	return provider->struct_cnt;
}

bool mtk_offload_provider_is_valid(enum uo_provider_type id)
{
	return check_provider_valid(get_provider(id));
}

char *mtk_offload_provider_parse_count(enum uo_provider_type id)
{
	struct uo_provider *provider = get_provider(id);
	int n;

	if (!provider) {
		USB_OFFLOAD_ERR("provider(id:%d) is invalid\n", id);
		return "unknown";
	}

	n = snprintf(parse_info, PARSE_INFO_LEN, "%s: %s",
		uop_get_name(provider), uo_provider_parse_count(provider));
	parse_info[n < PARSE_INFO_LEN ? n : PARSE_INFO_LEN - 1] = '\0';

	return parse_info;
}

static void *allocate_from_source(enum uo_provider_type id, dma_addr_t *phys,
	unsigned int size, int align, bool *is_rsv)
{
	struct uo_provider *provider = get_provider(id);
	void *virtual = NULL;

	*phys = 0;

	if (!check_provider_valid(provider))
		goto error;

	/* power on before allocating */
	provider_auto_power(provider->id, true);

	switch (provider->id) {
	case UO_PROV_DRAM:
		/* dram provider always on reserved part */
		virtual = uop_alloc_rsv(provider, phys, size, align);
		if (!virtual)
			goto allocate_fail;
		*is_rsv = true;
		break;
	case UO_PROV_SRAM:
		if (*is_rsv)
			virtual = uop_alloc_rsv(provider, phys, size, align);
		else if (!uodev->policy.force_on_secondary)
			virtual = uop_alloc_dyn(provider, phys, size, align);

		if (!virtual)
			goto allocate_fail;
		break;
	case UO_PROV_SRAM_2:
		/* secondary sram provider always on dynamic part */
		virtual = uop_alloc_dyn(provider, phys, size, align);
		if (!virtual)
			goto allocate_fail;
		*is_rsv = false;
		break;
	default:
		break;
	}

	return virtual;

allocate_fail:
	/* power off if fail allocating and no stucture on it  */
	provider_auto_power(provider->id, false);
error:
	return NULL;
}

int mtk_offload_alloc_mem(struct uo_buffer *buf, unsigned int size, int align,
	enum uo_provider_type id, enum uo_struct type, bool is_rsv)
{
	struct uo_provider *provider;
	enum uo_provider_type source;
	bool reserved = is_rsv;
	dma_addr_t phys;
	void *virt;
	int ret = 0;

	if (buf == NULL || buf->allocated == true)
		return -EINVAL;

	if (!is_secondary_sram(id)) {
		USB_OFFLOAD_ERR("do not directly control secondary sram source\n");
		return -EINVAL;
	}

	if (uodev->adv_lowpwr && id == UO_PROV_SRAM) {
		/* first priority source: main sram */
		source = UO_PROV_SRAM;
		virt = allocate_from_source(UO_PROV_SRAM, &phys, size, align, &reserved);

		/* second priority source: secondary sram */
		if (!virt && uodev->policy.secondary_sram != UO_SOURCE_NUM) {
			source = UO_PROV_SRAM_2;
			virt = allocate_from_source(UO_PROV_SRAM_2, &phys, size, align, &reserved);
		}

		if (virt)
			goto allocate_success;
	}

	/* last priority source: dram*/
	source = UO_PROV_DRAM;
	virt = allocate_from_source(UO_PROV_DRAM, &phys, size, align, &reserved);
	if (!virt) {
		ret = -ENOMEM;
		goto allocate_fail;
	}

allocate_success:
	provider = get_provider(source);
	if (unlikely(!check_provider_valid(provider)))
		goto allocate_fail;

	buf->virt = virt;
	buf->phys = phys;
	buf->size = size;
	buf->allocated = true;
	buf->is_rsv = reserved;
	buf->type = type;
	buf->provider = provider;

	/* increase structrure count of current provider */
	if (uop_increase_cnt(provider, type))
		goto allocate_fail;

	USB_OFFLOAD_INFO("success to allocate %s\n", mtk_offload_parse_buffer(buf));
	USB_OFFLOAD_MEM_DBG("[%s] %s\n",
		uop_get_name(provider), uo_provider_parse_count(provider));
	return 0;
allocate_fail:
	if (buf->allocated && mtk_offload_free_mem(buf) < 0)
		USB_OFFLOAD_ERR("failt to free buf:%p\n", buf);
	USB_OFFLOAD_ERR("fail to allocate, (%s) size:%d align:%d is_rsv:%d\n",
		uo_struct_name(type), size, align, is_rsv);
	return ret;
}

int mtk_offload_free_mem(struct uo_buffer *buf)
{
	struct uo_provider *provider;
	int ret = 0;

	if (buf == NULL || buf->allocated == false)
		return 0;

	provider = buf->provider;
	if (!check_provider_valid(provider)) {
		USB_OFFLOAD_ERR("provider is invalid\n");
		return -EINVAL;
	}

	if (buf->is_rsv)
		ret = uop_free_rsv(provider, buf->virt, buf->size);
	else
		ret = uop_free_dyn(provider, buf->phys);

	if (!ret) {
		/* decrease structrure count of current provider */
		USB_OFFLOAD_INFO("success to free %s\n", mtk_offload_parse_buffer(buf));
		uop_decrease_cnt(provider, buf->type);
		USB_OFFLOAD_MEM_DBG("[%s] %s\n",
			uop_get_name(provider), uo_provider_parse_count(provider));
		/* power-off if no structure on source */
		provider_auto_power(provider->id, false);
		reset_buffer(buf);
	} else
		USB_OFFLOAD_ERR("fail to free [%s]\n", mtk_offload_parse_buffer(buf));

	return ret;
}

struct uo_buffer *uob_get_empty(enum uo_struct type)
{
	struct uo_buffer *array;
	int index, length;

	if (type >= UO_STRUCT_NUM || !uodev->buf_array[type].first_buf)
		return NULL;

	array = uodev->buf_array[type].first_buf;
	length = uodev->buf_array[type].length;

	for (index = 0; index < length; index++) {
		if (!array[index].allocated) {
			USB_OFFLOAD_MEM_DBG("get empty buffer (%s index:%d buf:%p)\n",
				uo_struct_name(type), index, &array[index]);
			return &array[index];
		}
	}

	USB_OFFLOAD_MEM_DBG("insufficent space on %s array\n", uo_struct_name(type));
	return NULL;
}

struct uo_buffer *uob_get_first(enum uo_struct type)
{
	if (type >= UO_STRUCT_NUM)
		return NULL;

	USB_OFFLOAD_MEM_DBG("get first buffer (%s, buf:%p)\n",
		uo_struct_name(type), uodev->buf_array[type].first_buf);

	return uodev->buf_array[type].first_buf;
}

struct uo_buffer *uob_search(enum uo_struct type, dma_addr_t phy)
{
	struct uo_buffer *array;
	int index, length;

	if (type >= UO_STRUCT_NUM || !uodev->buf_array[type].first_buf)
		return NULL;

	array = uodev->buf_array[type].first_buf;
	length = uodev->buf_array[type].length;

	for (index = 0; index < length; index++) {
		if (array[index].allocated && array[index].phys == phy) {
			USB_OFFLOAD_MEM_DBG("get match buffer (%s index:%d buf:%p)\n",
				uo_struct_name(type), index, &array[index]);
			return &array[index];
		}
	}

	return NULL;
}

void uob_assign_array(enum uo_struct type, void *first_buffer, int length)
{
	if (type >= UO_STRUCT_NUM)
		return;

	uodev->buf_array[type].first_buf = (struct uo_buffer *)first_buffer;
	uodev->buf_array[type].length = length;
}

int uob_init(enum uo_struct type)
{
	int ret = 0;

	if (type >= UO_STRUCT_NUM)
		return -EINVAL;

	uodev->buf_array[type].first_buf = kcalloc(uodev->buf_array[type].length,
		sizeof(struct uo_buffer), GFP_KERNEL);
	if (!uodev->buf_array[type].first_buf) {
		ret = -ENOMEM;
		USB_OFFLOAD_ERR("insufficent space for %s array\n", uo_struct_name(type));
	}

	return ret;
}

void uob_deinit(enum uo_struct type)
{
	if (type >= UO_STRUCT_NUM)
		return;

	if (uodev->buf_array[type].first_buf) {
		kfree(uodev->buf_array[type].first_buf);
		uodev->buf_array[type].first_buf = NULL;
	}
}