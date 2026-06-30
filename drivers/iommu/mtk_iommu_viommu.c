// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Zhengnan Chen <zhengnan.chen@mediatek.com>
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt)    "mtk_iommu_viommu: " fmt

#include <dt-bindings/memory/mtk-memory-port.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/iommu.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <uapi/linux/virtio_iommu.h>
#include "mtk_iommu_viommu.h"

#define TAB_MAX 2
#define MTK_IOMMU_GROUP_MAX	MTK_M4U_DOM_NR_MAX

struct iommu_group {
	struct kobject kobj;
	struct kobject *devices_kobj;
	struct list_head devices;
	struct xarray pasid_array;
	struct mutex mutex;
	void *iommu_data;
	void (*iommu_data_release)(void *iommu_data);
	char *name;
	int id;
	struct iommu_domain *default_domain;
	struct iommu_domain *blocking_domain;
	struct iommu_domain *domain;
	struct list_head entry;
	unsigned int owner_cnt;
	void *owner;
};

struct mtk_viommu_domain {
	struct iommu_domain *domain;
	unsigned int id;
	unsigned int endpoint;
	struct list_head list;
};

static DEFINE_MUTEX(dom_mutex);
static LIST_HEAD(dom_list);

static int mtk_viommu_get_iommu_domain_id(uint32_t endpoint, uint32_t *domain_id)
{
	unsigned int tabid, domid;
	struct iommu_group *group = NULL;

	if (!domain_id) {
		pr_notice("%s domain_id is NULL\n", __func__);
		return -EINVAL;
	}

	tabid = MTK_M4U_TO_TAB(endpoint);
	domid = MTK_M4U_TO_DOM(endpoint);

	if ((tabid >= TAB_MAX) || (domid >= MTK_IOMMU_GROUP_MAX)) {
		pr_notice("%s tabid:%d--domid:%d--invalid\n",
			   __func__, tabid, domid);
		return -EINVAL;
	}

	group = mtk_iommu_viommu_get_vgroup(tabid);

	if (!group) {
		pr_notice("%s tabid:%d--domid:%d--group:%p\n",
			   __func__, tabid, domid, group);
		return -EINVAL;
	}

	pr_debug("%s (tabid,domid):(%d,%d)-->group->domain:0x%p, group->id:%d\n",
		  __func__, tabid, domid, group->domain, group->id);

	*domain_id = group->id;

	return 0;
}

static struct iommu_domain *mtk_viommu_get_iommu_group_domain(uint32_t endpoint)
{
	unsigned int tabid, domid;
	struct iommu_group *group = NULL;

	tabid = MTK_M4U_TO_TAB(endpoint);
	domid = MTK_M4U_TO_DOM(endpoint);

	if ((tabid >= TAB_MAX) || (domid >= MTK_IOMMU_GROUP_MAX)) {
		pr_notice("%s tabid:%d--domid:%d--invalid\n",
			   __func__, tabid, domid);
		return NULL;
	}

	group = mtk_iommu_viommu_get_vgroup(tabid);
	if (!group) {
		pr_notice("%s tabid:%d--domid:%d--group:%p\n",
			   __func__, tabid, domid, group);
		return NULL;
	}

	if (!group->domain) {
		pr_notice("%s tabid:%d--domid:%d--group->domain:%p\n",
			   __func__, tabid, domid, group->domain);
		return NULL;
	}

	pr_debug("%s (tabid,domid):(%d,%d)-->group->domain:0x%p, group->id:%d\n",
		  __func__, tabid, domid, group->domain, group->id);

	if (group->domain != group->default_domain)
		pr_notice("%s warning group->domain != group->default_domain\n",
			   __func__);

	return group->domain;
}

static struct iommu_domain *mtk_viommu_get_domain(uint32_t domain_id)
{
	struct mtk_viommu_domain *dom;

	list_for_each_entry(dom, &dom_list, list) {
		if (dom->id == domain_id)
			return dom->domain;
	}

	return NULL;
}

static int mtk_viommu_handle_iommu_attach(struct virtio_iommu_req_attach *req)
{
	struct mtk_viommu_domain *entry;
	struct iommu_domain *domain;

	if (!req) {
		pr_notice("%s req is NULL\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&dom_mutex);
	domain = mtk_viommu_get_domain(req->domain);
	if (domain) {
		mutex_unlock(&dom_mutex);
		return 0;
	}

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry) {
		mutex_unlock(&dom_mutex);
		return -ENOMEM;
	}
	entry->domain = mtk_viommu_get_iommu_group_domain(req->endpoint);
	entry->id = req->domain;
	entry->endpoint = req->endpoint;
	list_add_tail(&entry->list, &dom_list);

	pr_debug("[iommu debug] entry->domain:0x%p, req->domain id:%d\n",
		  entry->domain, req->domain);
	mutex_unlock(&dom_mutex);

	return 0;
}

static __s8 mtk_viommu_map(struct virtio_iommu_req_map *req)
{
	int ret;
	size_t size;
	struct iommu_domain *domain;

	if (!req) {
		pr_notice("%s req is NULL\n", __func__);
		return -EINVAL;
	}

	domain = mtk_viommu_get_domain(req->domain);
	if (domain == NULL) {
		pr_notice("failed to get iommu domain for domain id:%d\n",
			   req->domain);
		return VIRTIO_IOMMU_S_INVAL;
	}

	pr_debug("%s domain:%p\n", __func__, domain);

	size = req->virt_end - req->virt_start + 1;

	pr_debug("map: req->phys_start=%llx, req->virt_start=%llx\n",
		  req->phys_start, req->virt_start);

	ret = iommu_map(domain, req->virt_start, req->phys_start,
			size, req->flags, GFP_KERNEL);
	if (ret)
		return VIRTIO_IOMMU_S_INVAL;

	return VIRTIO_IOMMU_S_OK;
}

static __s8 mtk_viommu_map_sg(struct virtio_iommu_req_map_sg *req)
{
	int ret, i;
	struct iommu_domain *domain;
	struct virtio_iommu_map_record *record;
	struct sg_table *table;
	struct scatterlist *sg;

	if (!req) {
		pr_notice("%s req is NULL\n", __func__);
		return -EINVAL;
	}

	domain = mtk_viommu_get_domain(req->domain);
	if (domain == NULL) {
		pr_notice("failed to get iommu domain for domain id:%d\n",
			   req->domain);
		return VIRTIO_IOMMU_S_INVAL;
	}

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return VIRTIO_IOMMU_S_NOMEM;

	ret = sg_alloc_table(table, req->count, GFP_KERNEL);
	if (ret) {
		pr_notice("%s err: sg_alloc_table failed\n", __func__);
		kfree(table);
		return VIRTIO_IOMMU_S_NOMEM;
	}

	for_each_sgtable_sg(table, sg, i) {
		record = &req->record[i];
		sg_set_page(sg, phys_to_page(record->phys_start),
			    record->size, 0);
	}

	ret = iommu_map_sg(domain, req->record[0].virt_start, table->sgl,
			   table->orig_nents, req->record[0].flags, GFP_KERNEL);
	if (ret < 0) {
		pr_notice("%s map fail, ret = %d\n", __func__, ret);
		sg_free_table(table);
		kfree(table);
		WARN_ON(1);
		return VIRTIO_IOMMU_S_INVAL;
	}

	sg_free_table(table);
	kfree(table);

	return VIRTIO_IOMMU_S_OK;
}

static __s8 mtk_viommu_unmap(struct virtio_iommu_req_unmap *req)
{
	struct iommu_domain *domain;
	size_t size, unmapped_size;

	if (!req) {
		pr_notice("%s req is NULL\n", __func__);
		return -EINVAL;
	}

	domain = mtk_viommu_get_domain(req->domain);
	if (domain == NULL) {
		pr_notice("failed to get iommu domain for domain id:%d\n",
			   req->domain);
		return VIRTIO_IOMMU_S_INVAL;
	}

	pr_debug("%s domain:0x%p\n", __func__, domain);

	size = req->virt_end - req->virt_start + 1;

	pr_debug("unmap: req->virt_start=0x%llx, req->virt_end=0x%llx, size=%lx\n",
		  req->virt_start, req->virt_end, size);

	unmapped_size = iommu_unmap(domain, req->virt_start, size);
	if (unmapped_size != size) {
		pr_notice("failed to do iommu unmap: 0x%zx, 0x%zx, domain:%d\n",
			   size, unmapped_size, req->domain);
		return VIRTIO_IOMMU_S_INVAL;
	}

	return VIRTIO_IOMMU_S_OK;
}

struct virtio_iommu_pal_ops mtk_pal_ops = {
	.get_iova_space = NULL,
	.get_iommu_domain_id = mtk_viommu_get_iommu_domain_id,
	.get_reserved_mems = NULL,
	.handle_iommu_attach = mtk_viommu_handle_iommu_attach,
	.handle_iommu_map = mtk_viommu_map,
	.handle_iommu_map_sg = mtk_viommu_map_sg,
	.handle_iommu_unmap = mtk_viommu_unmap,
};

struct virtio_iommu_pal_ops *get_virtio_iommu_pal_ops(void)
{
	return &mtk_pal_ops;
}
EXPORT_SYMBOL_GPL(get_virtio_iommu_pal_ops);

MODULE_DESCRIPTION("Vhost iommu api used by virtual iommu back end driver");
MODULE_LICENSE("GPL");
