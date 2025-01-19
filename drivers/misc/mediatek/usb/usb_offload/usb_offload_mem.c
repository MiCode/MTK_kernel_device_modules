// SPDX-License-Identifier: GPL-2.0
/*
 * MTK USB Offload Memory Management API
 * *
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Yu-chen.Liu <yu-chen.liu@mediatek.com>
 */

#include <linux/printk.h>
#include <linux/genalloc.h>

#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
#include <adsp_helper.h>
#endif

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
#include <scp.h>
#endif

#include "usb_offload.h"

#define PARSE_INFO_LEN  128
static char parse_info[PARSE_INFO_LEN];
char *mtk_offload_parse_buffer(struct usb_offload_buffer *buf)
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
	return (provider == NULL || provider->is_init == false) ? false : true;
}

LIST_HEAD(downgrade_list);

static bool is_buf_downgrade(struct usb_offload_buffer *buf)
{
	struct usb_offload_buffer *pos;
	bool found = false;

	list_for_each_entry(pos, &downgrade_list, list) {
		if (pos == buf) {
			found = true;
			break;
		}
	}

	return found;
}

bool mtk_offload_is_advlowpwr(struct usb_offload_dev *udev)
{
	/* if adv_lowpwr is false, it means that either sram feature is
	 * disabled in dts or basic sram is not supported in this platform.
	 */
	if (!udev->adv_lowpwr)
		return false;

	/* For downlink only adv_lowpwr, RX data was placed in DRAM and
	 * memory region might not be recored in memory downgrade_list.
	 */
	if (udev->adv_lowpwr_dl_only && udev->rx_streaming)
		return false;

	/* if list is empty, it means no structure falls to dram,
	 * so it's in advanced mode, in an other hands, it's basic
	 */
	return list_empty(&downgrade_list);
}
EXPORT_SYMBOL_GPL(mtk_offload_is_advlowpwr);


static void reset_buffer(struct usb_offload_buffer *buf)
{
	buf->provider = NULL;
	buf->phys = 0;
	buf->virt = NULL;
	buf->size = 0;
	buf->allocated = false;
	buf->is_rsv = false;
	buf->type = 0;
}

static bool is_sram(enum uo_provider_type id)
{
	return id == UO_PROV_SRAM ? true : false;
}

int mtk_offload_provider_register(struct device *dev, enum uo_provider_type id)
{
	struct uo_provider *provider = get_provider(id);
	struct device_node *node = dev->of_node;
	struct uo_provider_ops *ops;
	int ret;

	if (!provider) {
		USB_OFFLOAD_ERR("provider is invalid\n");
		return -EINVAL;
	}

	switch (id) {
	case UO_PROV_DRAM:
		ops = &uo_dram_ops;
		break;
	case UO_PROV_SRAM:
		if(of_property_read_bool(node, "mediatek,usb-sram"))
			ops = &uo_usb_sram_ops;
		else
			ops = &uo_afe_sram_ops;
		break;
	default:
		return -EINVAL;
	}

	ret = uop_register(dev, provider, id, ops);
	if (ret) {
		USB_OFFLOAD_ERR("fail to register provider, id:%d\n", id);
		return ret;
	}

	USB_OFFLOAD_INFO("success to register %s provider\n", uop_get_name(provider));

	ret = uop_init(provider);
	if (ret) {
		USB_OFFLOAD_ERR("fail to init %s provider\n", uop_get_name(provider));
		return ret;
	}

	USB_OFFLOAD_INFO("success to init %s provider\n", uop_get_name(provider));

	return 0;
}

int mtk_offload_init_rsv(enum uo_provider_type id)
{
	struct uo_provider *provider = get_provider(id);
	struct uo_rsv_region *rsv_region;
	unsigned int size;
	int min_alloc_order = MIN_USB_OFFLOAD_SHIFT;

	switch (id) {
	case UO_PROV_DRAM:
		/* don't care size of reserved dram */
		size = 0;
		break;
	case UO_PROV_SRAM:
		if (!uodev->adv_lowpwr)
			return 0;
		size = uodev->adv_lowpwr_dl_only ? 12288 : 16384;
		break;
	default:
		return 0;
	}

	if (!check_provider_valid(provider)) {
		USB_OFFLOAD_ERR("provider(id:%d) is invalid\n", id);
		return -EINVAL;
	}

	rsv_region = &provider->rsv_region;
	if (rsv_region->is_valid) {
		USB_OFFLOAD_INFO("[%s] rsv_region was already init\n", uop_get_name(provider));
		return 0;
	}

	if (uop_init_rsv(provider, size, min_alloc_order)) {
		USB_OFFLOAD_ERR("[%s] fail to init rsv region\n", uop_get_name(provider));
		goto error;
	}

	USB_OFFLOAD_INFO("[%s] success to init rsv_region, phys:0x%llx size:%lld\n",
		uop_get_name(provider), rsv_region->physical, rsv_region->size);

	return 0;
error:
	if (id == UO_PROV_SRAM) {
		USB_OFFLOAD_ERR("change mode, adv_lowpwr:%d->0\n", uodev->adv_lowpwr);
		uodev->adv_lowpwr = false;
	}
	return -EINVAL;
}

void mtk_offload_deinit_rsv(enum uo_provider_type id)
{
	struct uo_provider *provider = get_provider(id);
	struct uo_rsv_region *rsv_region;

	if (!uodev->adv_lowpwr && is_sram(id))
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

	if (uop_deinit_rsv(provider)) {
		USB_OFFLOAD_ERR("[%s] fail to deinit rsv region\n", uop_get_name(provider));
		return;
	}

	USB_OFFLOAD_INFO("[%s] success to deinit rsv_region\n", uop_get_name(provider));
}

/* get reserved region of provider */
unsigned int mtk_offload_get_rsv_region(enum uo_provider_type id, dma_addr_t *phys)
{
	struct uo_provider *provider = get_provider(id);
	struct uo_rsv_region *rsv_region;
	unsigned int size = 0;

	*phys = 0;

	if (!check_provider_valid(provider)) {
		USB_OFFLOAD_ERR("provider(id:%d) is invalid\n", id);
		return size;
	}

	rsv_region = &provider->rsv_region;
	if (rsv_region->is_valid) {
		*phys = rsv_region->physical;
		size = rsv_region->size;
	}

	return size;
}

void mtk_offload_provider_power(enum uo_provider_type id, bool is_on)
{
	struct uo_provider *provider = get_provider(id);

	if (!check_provider_valid(provider)) {
		USB_OFFLOAD_ERR("provider(id:%d) is invalid\n", id);
		return;
	}

	if (provider->power != is_on) {
		USB_OFFLOAD_INFO("[%s] power change to %s\n",
			uop_get_name(provider), is_on ? "on" : "off");
		provider->power = is_on;
		if (uop_pwr_ctrl(provider, is_on))
			USB_OFFLOAD_ERR("[%s] fail to control power\n", uop_get_name(provider));
	} else
		USB_OFFLOAD_INFO("[%s] power stay %s\n",
			uop_get_name(provider), provider->power ? "on" : "off");
}

/* get structure count of provider */
u32 mtk_offload_provider_get_cnt(enum uo_provider_type id)
{
	struct uo_provider *provider = get_provider(id);

	if (!check_provider_valid(provider)) {
		USB_OFFLOAD_ERR("provider(id:%d) is invalid\n", id);
		return 0;
	}

	return provider ? provider->struct_cnt : 0;
}

char *mtk_offload_provider_parse_count(enum uo_provider_type id)
{
	struct uo_provider *provider = get_provider(id);
	int n;

	if (!check_provider_valid(provider)) {
		USB_OFFLOAD_ERR("provider(id:%d) is invalid\n", id);
		return "unknown";
	}

	n = snprintf(parse_info, PARSE_INFO_LEN, "%s: %s",
		uop_get_name(provider), uo_provider_parse_count(provider));
	parse_info[n < PARSE_INFO_LEN ? n : PARSE_INFO_LEN - 1] = '\0';

	return parse_info;
}


int mtk_offload_alloc_mem(struct usb_offload_buffer *buf, unsigned int size, int align,
	enum uo_provider_type id, enum uo_struct type, bool is_rsv)
{
	struct uo_provider *provider = get_provider(id);
	bool reserved = is_rsv;
	dma_addr_t phys;
	void *virt;
	int ret = 0;

	if (!check_provider_valid(provider)) {
		USB_OFFLOAD_ERR("provider(id:%d) is invalid\n", id);
		return -EINVAL;
	}

	if (buf == NULL || buf->allocated == true)
		return -EINVAL;

	/* first priority on reserved or allocated part of sram provider */
	if (uodev->adv_lowpwr && provider->id == UO_PROV_SRAM) {
		if (reserved)
			virt = uop_alloc_rsv(provider, &phys, size, align);
		else
			virt = uop_alloc_dyn(provider, &phys, size, align);

		if (virt)
			goto ALLOC_SUCCESS;
		else {
			USB_OFFLOAD_MEM_DBG("[%s] fail to allocate [size:%d align:%d is_rsv:%d]\n",
				uop_get_name(provider), size, align, is_rsv);
			/* downgrade mode, only reserved part on dram */
			reserved = true;
		}
	} else
		/* dran only mode, only reserved part on dram */
		reserved = true;

	/* second priority on reserved part of dram provider */
	provider = get_provider(UO_PROV_DRAM);
	virt = uop_alloc_rsv(provider, &phys, size, align);
	if (!virt) {
		ret = -ENOMEM;
		goto ALLOC_FAIL;
	}

ALLOC_SUCCESS:
	buf->virt = virt;
	buf->phys = phys;
	buf->size = size;
	buf->allocated = true;
	buf->is_rsv = reserved;
	buf->type = type;
	buf->provider = provider;

	/* increase structrure count of current provider */
	uop_increase_cnt(provider, type);

	if (is_sram(provider->id) != is_sram(id)) {
		/* we requeset for sram, but turn out to be dram */
		USB_OFFLOAD_MEM_DBG("buf:%p falls from sram to dram\n", buf);
		list_add_tail(&buf->list, &downgrade_list);
	}

	USB_OFFLOAD_INFO("success allocate [%s]\n", mtk_offload_parse_buffer(buf));
	return 0;
ALLOC_FAIL:
	USB_OFFLOAD_ERR("fail to allocate, (%s %s) size:%d align:%d is_rsv:%d\n",
		uop_get_name(provider), uo_struct_name(type), size, align, is_rsv);
	return ret;
}

int mtk_offload_free_mem(struct usb_offload_buffer *buf)
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

	if (is_sram(provider->id) && is_buf_downgrade(buf))
		list_del(&buf->list);

	if (buf->is_rsv)
		ret = uop_free_rsv(provider, buf->virt, buf->size);
	else
		ret = uop_free_dyn(provider, buf->phys);

	if (!ret) {
		/* decrease structrure count of current provider */
		USB_OFFLOAD_INFO("success free [%s]\n", mtk_offload_parse_buffer(buf));
		uop_decrease_cnt(provider, buf->type);
		reset_buffer(buf);
	} else
		USB_OFFLOAD_ERR("fail to free [%s]\n", mtk_offload_parse_buffer(buf));

	return ret;
}
