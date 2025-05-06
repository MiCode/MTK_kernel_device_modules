// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF mtk_sec heap exporter
 *
 * Copyright (C) 2021 MediaTek Inc.
 *
 */

#define pr_fmt(fmt) "dma_heap: mtk_sec " fmt

#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/iommu.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/list_sort.h>
#include <linux/vmalloc.h>
#include "mtk_heap_priv.h"
#include "mtk_heap.h"
#include "mtk_sec_heap.h"
#include "mtk_sec_heap_priv.h"
#include "mtk_tmem_interop.h"
#include "mtk_iommu.h"
#include "mtk-smmu-v3.h"
#if !IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)
#include "iommu_pseudo.h"
#endif

struct dmabuf_page_pool *sec_pools[SEC_NUM_ORDERS];

bool smmu_v3_enable;

enum sec_heap_region_type {
	/* MM heap */
	MM_HEAP_START,
	SVP_REGION,
	PROT_REGION,
	PROT_2D_FR_REGION,
	WFD_REGION,
	TUI_REGION,
	MM_HEAP_END,

	/* APU heap */
	APU_HEAP_START,
	SAPU_DATA_SHM_REGION,
	SAPU_ENGINE_SHM_REGION,
	APU_HEAP_END,

	REGION_HEAPS_NUM,
};

enum sec_heap_page_type {
	SVP_PAGE,
	PROT_PAGE,
	WFD_PAGE,
	SAPU_PAGE,
	TEE_PAGE,
	PAGE_HEAPS_NUM,
};

#define NAME_LEN 32

static struct secure_heap_page *mtk_sec_heap_page;
static u32 mtk_sec_page_count;

static struct secure_heap_region *mtk_sec_heap_region;
static u32 mtk_sec_region_count;

/* function declare */
static int is_region_base_dmabuf(const struct dma_buf *dmabuf);
static int is_page_base_dmabuf(const struct dma_buf *dmabuf);

static struct iova_cache_data *get_iova_cache(struct mtk_sec_heap_buffer *buffer, u64 tab_id)
{
	struct iova_cache_data *cache_data;

	list_for_each_entry(cache_data, &buffer->iova_caches, iova_caches) {
		if (cache_data->tab_id == tab_id)
			return cache_data;
	}
	return NULL;
}

void free_iova_cache(struct mtk_sec_heap_buffer *buffer,
		     struct device *iommu_dev, int is_region_base)
{
	struct iova_cache_data *cache_data, *temp_data;
	struct sg_table *table;
	struct mtk_heap_dev_info dev_info;
	unsigned long attrs;
	int skip_unmap, j = 0;

	list_for_each_entry_safe(cache_data, temp_data, &buffer->iova_caches, iova_caches) {
		for (j = 0; j < MTK_M4U_DOM_NR_MAX; j++) {
			if (!cache_data->mapped[j])
				continue;

		        table = cache_data->mapped_table[j];
			dev_info = cache_data->dev_info[j];
			attrs = dev_info.map_attrs | DMA_ATTR_SKIP_CPU_SYNC;
			skip_unmap = 0;
			if (is_region_base) {
				if (smmu_v3_enable)
					skip_unmap = get_smmu_tab_id(dev_info.dev) ==
						     get_smmu_tab_id(iommu_dev);
				else
					skip_unmap = dev_is_normal_region(dev_info.dev);
			}

			if (skip_unmap)
				continue;

			pr_debug("%s: free tab:%llu, dom:%d iova:0x%lx, dev:%s\n",
				 __func__, cache_data->tab_id, j,
				 (unsigned long)sg_dma_address(table->sgl),
				 dev_name(dev_info.dev));
			dma_unmap_sgtable(dev_info.dev, table,
					  dev_info.direction, attrs);
			cache_data->mapped[j] = false;
			sg_free_table(table);
			kfree(table);
		}
		list_del(&cache_data->iova_caches);
		kfree(cache_data);
	}
}

bool region_heap_is_aligned(struct dma_heap *heap)
{
	if (strstr(dma_heap_get_name(heap), "aligned"))
		return true;

	return false;
}

static int get_heap_base_type(const struct dma_heap *heap)
{
	int i, j, k;

	for (i = 0; i < mtk_sec_region_count; i++) {
		for (j = REGION_HEAP_NORMAL; j < REGION_TYPE_NUM; j++) {
			if (mtk_sec_heap_region[i].heap[j] == heap)
				return REGION_BASE;
		}
	}
	for (k = 0; k < mtk_sec_page_count; k++) {
		if (mtk_sec_heap_page[k].heap == heap)
			return PAGE_BASE;
	}
	return HEAP_TYPE_INVALID;
}

struct secure_heap_region *sec_heap_region_get(const struct dma_heap *heap)
{
	int i, j;

	for (i = 0; i < mtk_sec_region_count; i++) {
		for (j = REGION_HEAP_NORMAL; j < REGION_TYPE_NUM; j++) {
			if (mtk_sec_heap_region[i].heap[j] == heap)
				return &mtk_sec_heap_region[i];
		}
	}
	return NULL;
}

struct secure_heap_page *sec_heap_page_get(const struct dma_heap *heap)
{
	int i = 0;

	for (i = 0; i < mtk_sec_page_count; i++) {
		if (mtk_sec_heap_page[i].heap == heap)
			return &mtk_sec_heap_page[i];
	}
	return NULL;
}

static struct sg_table *dup_sg_table_sec(struct sg_table *table)
{
	struct sg_table *new_table;
	int ret, i;
	struct scatterlist *sg, *new_sg;

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(new_table, table->orig_nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return ERR_PTR(-ENOMEM);
	}

	new_sg = new_table->sgl;
	for_each_sgtable_sg(table, sg, i) {
		/* skip copy dma_address, need get via map_attachment */
		sg_set_page(new_sg, sg_page(sg), sg->length, sg->offset);
		new_sg = sg_next(new_sg);
	}

	return new_table;
}

static int mtk_sec_heap_attach(struct dma_buf *dmabuf,
			       struct dma_buf_attachment *attachment)
{
	struct mtk_sec_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;
	struct sg_table *table;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	table = dup_sg_table_sec(&buffer->sg_table);
	if (IS_ERR(table)) {
		kfree(a);
		return -ENOMEM;
	}

	dmabuf_name_check(dmabuf, attachment->dev);

	a->table = table;
	a->dev = attachment->dev;
	INIT_LIST_HEAD(&a->list);
	a->mapped = false;
	a->uncached = buffer->uncached;
	attachment->priv = a;

	mutex_lock(&buffer->lock);
	list_add(&a->list, &buffer->attachments);
	mutex_unlock(&buffer->lock);

	return 0;
}

static void mtk_sec_heap_detach(struct dma_buf *dmabuf,
				struct dma_buf_attachment *attachment)
{
	struct mtk_sec_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a = attachment->priv;

	mutex_lock(&buffer->lock);
	list_del(&a->list);
	mutex_unlock(&buffer->lock);

	sg_free_table(a->table);
	kfree(a->table);
	kfree(a);
}

/* source copy to dest */
int copy_sec_sg_table(struct sg_table *source, struct sg_table *dest)
{
	int i;
	struct scatterlist *sgl, *dest_sgl;

	if (source->orig_nents != dest->orig_nents) {
		pr_err("%s err, nents not match %d-%d\n", __func__,
		       source->orig_nents, dest->orig_nents);

		return -EINVAL;
	}

	/* copy mapped nents */
	dest->nents = source->nents;

	dest_sgl = dest->sgl;
	for_each_sg(source->sgl, sgl, source->orig_nents, i) {
		if (!sgl || !dest_sgl) {
			pr_info("%s err, sgl or dest_sgl null at %d\n",
				__func__, i);
			return -EINVAL;
		}
		memcpy(dest_sgl, sgl, sizeof(*sgl));
		dest_sgl = sg_next(dest_sgl);
	}

	return 0;
};

/*
 * must check domain info before call fill_buffer_info
 * @Return 0: pass
 */
static int fill_sec_buffer_info(struct mtk_sec_heap_buffer *buf,
				struct sg_table *table,
				struct dma_buf_attachment *a,
				enum dma_data_direction dir,
				unsigned int tab_id, unsigned int dom_id)
{
	struct iova_cache_data *cache_data;
	struct sg_table *new_table = NULL;
	int ret = 0;

	/*
	 * devices without iommus attribute,
	 * use common flow, skip set buf_info
	 */
	if (!smmu_v3_enable &&
	   (tab_id >= MTK_M4U_TAB_NR_MAX ||
	    dom_id >= MTK_M4U_DOM_NR_MAX))
		return 0;

	cache_data = get_iova_cache(buf, tab_id);
	if (cache_data != NULL && cache_data->mapped[dom_id]) {
		pr_info("%s err: already mapped, no need fill again\n",
			__func__);
		return -EINVAL;
	}

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return -ENOMEM;

	ret = sg_alloc_table(new_table, table->orig_nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return -ENOMEM;
	}

	ret = copy_sec_sg_table(table, new_table);
	if (ret)
		return ret;

	if (!cache_data) {
		cache_data = kzalloc(sizeof(*cache_data), GFP_KERNEL);
		if (!cache_data) {
			kfree(new_table);
			return -ENOMEM;
		}
		cache_data->tab_id = tab_id;
		list_add(&cache_data->iova_caches, &buf->iova_caches);
	}

	cache_data->mapped_table[dom_id] = new_table;
	cache_data->mapped[dom_id] = true;
	cache_data->dev_info[dom_id].dev = a->dev;
	cache_data->dev_info[dom_id].direction = dir;
	cache_data->dev_info[dom_id].map_attrs = a->dma_map_attrs;

	return 0;
}

static int check_map_alignment(struct sg_table *table)
{
	if (!trusted_mem_is_page_v2_enabled()) {
		int i;
		struct scatterlist *sgl;

		for_each_sg(table->sgl, sgl, table->orig_nents, i) {
			unsigned int len;
			phys_addr_t s_phys;

			if (!sgl) {
				pr_info("%s err, sgl null at %d\n", __func__, i);
				return -EINVAL;
			}

			len = sgl->length;
			s_phys = sg_phys(sgl);

			if (!IS_ALIGNED(len, SZ_1M)) {
				pr_info("%s err, size(0x%x) is not 1MB alignment\n",
				       __func__, len);
				return -EINVAL;
			}
			if (!IS_ALIGNED(s_phys, SZ_1M)) {
				pr_info("%s err, s_phys(%pa) is not 1MB alignment\n",
				       __func__, &s_phys);
				return -EINVAL;
			}
		}
	}

	return 0;
}

static struct sg_table *
mtk_sec_heap_page_map_dma_buf(struct dma_buf_attachment *attachment,
			      enum dma_data_direction direction)
{
	int ret;
	struct dma_heap_attachment *a = attachment->priv;
	struct sg_table *table = a->table;
	struct dma_buf *dmabuf = attachment->dmabuf;
	struct mtk_sec_heap_buffer *buffer;
	struct secure_heap_page *sec_heap;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(attachment->dev);
	unsigned int tab_id = MTK_M4U_TAB_NR_MAX, dom_id = MTK_M4U_DOM_NR_MAX;
	int attr = attachment->dma_map_attrs | DMA_ATTR_SKIP_CPU_SYNC;
	struct iova_cache_data *cache_data = NULL;

	/* non-iommu master */
	if (!fwspec) {
		ret = dma_map_sgtable(attachment->dev, table, direction, attr);
		if (ret) {
			pr_err("%s err, non-iommu-dev(%s) dma_map_sgtable failed\n",
			       __func__, dev_name(attachment->dev));
			return ERR_PTR(ret);
		}
		a->mapped = true;
		pr_debug("%s done, non-iommu-dev(%s)\n", __func__,
			 dev_name(attachment->dev));
		return table;
	}

	buffer = dmabuf->priv;
	sec_heap = sec_heap_page_get(buffer->heap);
	if (!sec_heap) {
		pr_err("%s, dma_sec_heap_get failed\n", __func__);
		return NULL;
	}

	mutex_lock(&buffer->map_lock);
	if (!smmu_v3_enable) {
		dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);
		tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
		cache_data = get_iova_cache(buffer, tab_id);
	} else {
		tab_id = get_smmu_tab_id(attachment->dev);
		cache_data = get_iova_cache(buffer, tab_id);
		dom_id = 0;
	}

	/* device with iommus attribute and mapped before */
	if (fwspec && cache_data != NULL && cache_data->mapped[dom_id]) {
		/* mapped before, return saved table */
		ret = copy_sec_sg_table(cache_data->mapped_table[dom_id],
					table);
		if (ret) {
			pr_err("%s err, copy_sec_sg_table failed, dev:%s\n",
			       __func__, dev_name(attachment->dev));
			mutex_unlock(&buffer->map_lock);
			return ERR_PTR(-EINVAL);
		}
		a->mapped = true;
		pr_debug(
			"%s done(has mapped), dev:%s(%s), sec_handle:%llu, len:%#x, iova:%#lx, id:(%d,%d)\n",
			__func__,
			dev_name(cache_data->dev_info[dom_id].dev),
			dev_name(attachment->dev), buffer->sec_handle,
			buffer->len, (unsigned long)sg_dma_address(table->sgl),
			tab_id, dom_id);
		mutex_unlock(&buffer->map_lock);
		return table;
	}

	ret = check_map_alignment(table);
	if (ret) {
		pr_err("%s err, size or PA is not 1MB alignment, dev:%s\n",
		       __func__, dev_name(attachment->dev));
		mutex_unlock(&buffer->map_lock);
		return ERR_PTR(ret);
	}
	ret = dma_map_sgtable(attachment->dev, table, direction, attr);
	if (ret) {
		pr_err("%s err, iommu-dev(%s) dma_map_sgtable failed\n",
		       __func__, dev_name(attachment->dev));
		mutex_unlock(&buffer->map_lock);

		if (dmabuf_rbtree_dump_by_domain)
			dmabuf_rbtree_dump_by_domain(tab_id, dom_id);
		return ERR_PTR(ret);
	}
	ret = fill_sec_buffer_info(buffer, table, attachment, direction, tab_id,
				   dom_id);
	if (ret) {
		pr_err("%s failed, fill_sec_buffer_info failed, dev:%s\n",
		       __func__, dev_name(attachment->dev));
		mutex_unlock(&buffer->map_lock);
		return ERR_PTR(-ENOMEM);
	}
	a->mapped = true;

	pr_debug(
		"%s done, dev:%s, sec_handle:%llu, len:%#x, iova:%#lx, id:(%d,%d)\n",
		__func__, dev_name(attachment->dev), buffer->sec_handle,
		buffer->len, (unsigned long)sg_dma_address(table->sgl), tab_id,
		dom_id);
	mutex_unlock(&buffer->map_lock);

	return table;
}

static struct sg_table *
mtk_sec_heap_region_map_dma_buf(struct dma_buf_attachment *attachment,
				enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	struct sg_table *table = a->table;
	struct dma_buf *dmabuf = attachment->dmabuf;
	struct mtk_sec_heap_buffer *buffer;
	struct secure_heap_region *sec_heap;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(attachment->dev);
	unsigned int tab_id = MTK_M4U_TAB_NR_MAX, dom_id = MTK_M4U_DOM_NR_MAX;
	int ret;
	dma_addr_t dma_address;
	uint64_t phy_addr = 0;
	/* for iommu mapping, should be skip cache sync */
	int attr = attachment->dma_map_attrs | DMA_ATTR_SKIP_CPU_SYNC;
	struct device *iommu_dev; /* The dev to map iova */
	struct iova_cache_data *cache_data = NULL;
	unsigned int region_tab_id; /* The region heap's smmu tab id */

	/* non-iommu master */
	if (!fwspec) {
		ret = dma_map_sgtable(attachment->dev, table, direction, attr);
		if (ret) {
			pr_err("%s err, non-iommu-dev(%s) dma_map_sgtable failed\n",
			       __func__, dev_name(attachment->dev));
			return ERR_PTR(ret);
		}
		a->mapped = true;
		pr_debug("%s done, non-iommu-dev(%s), pa:0x%lx\n", __func__,
			 dev_name(attachment->dev),
			 (unsigned long)sg_dma_address(table->sgl));
		return table;
	}

#if !IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)
	if (is_disable_map_sec()) {
		pr_debug("%s : Secure buffer mapping not supported, dev:%s\n", __func__,
			 dev_name(attachment->dev));
		return ERR_PTR(-EINVAL);
	}
#endif

	buffer = dmabuf->priv;
	sec_heap = sec_heap_region_get(buffer->heap);
	if (!sec_heap) {
		pr_err("%s, sec_heap_region_get failed\n", __func__);
		return NULL;
	}
	iommu_dev = sec_heap->iommu_dev;

	mutex_lock(&sec_heap->heap_lock);
	if (!sec_heap->heap_mapped) {
		ret = check_map_alignment(sec_heap->region_table);
		if (ret) {
			pr_err("%s err, heap_region size or PA is not 1MB alignment, dev:%s\n",
			       __func__, dev_name(iommu_dev));
			mutex_unlock(&sec_heap->heap_lock);
			return ERR_PTR(ret);
		}
		if (dma_map_sgtable(iommu_dev, sec_heap->region_table,
				    DMA_BIDIRECTIONAL,
				    DMA_ATTR_SKIP_CPU_SYNC)) {
			pr_err("%s err, heap_region(%s) dma_map_sgtable failed\n",
			       __func__, dev_name(iommu_dev));
			mutex_unlock(&sec_heap->heap_lock);
			return ERR_PTR(-ENOMEM);
		}
		sec_heap->heap_mapped = true;
		pr_debug(
			"%s heap_region map success, heap:%s, iova:0x%lx, sz:0x%lx\n",
			__func__, dma_heap_get_name(buffer->heap),
			(unsigned long)sg_dma_address(
				sec_heap->region_table->sgl),
			(unsigned long)sg_dma_len(sec_heap->region_table->sgl));
	}
	mutex_unlock(&sec_heap->heap_lock);

	mutex_lock(&buffer->map_lock);
	if (!smmu_v3_enable) {
		dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);
		tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
		cache_data = get_iova_cache(buffer, tab_id);
	} else {
		tab_id = get_smmu_tab_id(attachment->dev);
		cache_data = get_iova_cache(buffer, tab_id);
		dom_id = 0;
		region_tab_id = get_smmu_tab_id(iommu_dev);
	}
	phy_addr = (uint64_t)sg_phys(table->sgl);
	/* device with iommus attribute and mapped before */
	if (cache_data && cache_data->mapped[dom_id]) {
		/* mapped before, return saved table */
		ret = copy_sec_sg_table(cache_data->mapped_table[dom_id],
					table);
		if (ret) {
			pr_err("%s err, copy_sec_sg_table failed, dev:%s\n",
			       __func__, dev_name(attachment->dev));
			mutex_unlock(&buffer->map_lock);
			return ERR_PTR(-EINVAL);
		}
		a->mapped = true;
		pr_debug(
			"%s done(has mapped), dev:%s(%s), sec_handle:%llu, len:%#x, pa:%#llx, iova:%#lx, id:(%d,%d)\n",
			__func__,
			dev_name(cache_data->dev_info[dom_id].dev),
			dev_name(attachment->dev), buffer->sec_handle,
			buffer->len, phy_addr,
			(unsigned long)sg_dma_address(table->sgl), tab_id,
			dom_id);
		mutex_unlock(&buffer->map_lock);
		return table;
	}

	/* For iommu, remap to normal iova domain if necessary */
	if (!smmu_v3_enable && !dev_is_normal_region(attachment->dev)) {
		ret = check_map_alignment(table);
		if (ret) {
			pr_err("%s err, size or PA is not 1MB alignment, dev:%s\n",
			       __func__, dev_name(attachment->dev));
			mutex_unlock(&buffer->map_lock);
			return ERR_PTR(ret);
		}
		ret = dma_map_sgtable(attachment->dev, table, direction, attr);
		if (ret) {
			pr_err("%s err, iommu-dev(%s) dma_map_sgtable failed\n",
			       __func__, dev_name(attachment->dev));
			mutex_unlock(&buffer->map_lock);

			if (dmabuf_rbtree_dump_by_domain)
				dmabuf_rbtree_dump_by_domain(tab_id, dom_id);
			return ERR_PTR(ret);
		}
		pr_debug(
			"%s reserve_iommu-dev(%s) dma_map_sgtable done, iova:%#lx, id:(%d,%d)\n",
			__func__, dev_name(attachment->dev),
			(unsigned long)sg_dma_address(table->sgl), tab_id,
			dom_id);
		goto map_done;
	}

	/* For smmu, remap to target pgtable if necessary */
	if (smmu_v3_enable && tab_id != region_tab_id) {
		ret = check_map_alignment(table);
		if (ret) {
			pr_err("%s err, size or PA is not 1MB alignment, dev:%s\n",
			       __func__, dev_name(attachment->dev));
			mutex_unlock(&buffer->map_lock);
			return ERR_PTR(ret);
		}
		ret = dma_map_sgtable(attachment->dev, table, direction, attr);
		if (ret) {
			pr_err("%s err, iommu-dev(%s) dma_map_sgtable failed\n",
			       __func__, dev_name(attachment->dev));
			mutex_unlock(&buffer->map_lock);
			return ERR_PTR(ret);
		}
		pr_debug(
			"%s reserve_iommu-dev(%s) dma_map_sgtable done, iova:%#lx, id:(%d,%d)\n",
			__func__, dev_name(attachment->dev),
			(unsigned long)sg_dma_address(table->sgl), tab_id,
			dom_id);
		goto map_done;
	}

	if (buffer->len <= 0 || buffer->len > sec_heap->region_size ||
	    phy_addr < sec_heap->region_pa ||
	    phy_addr > sec_heap->region_pa + sec_heap->region_size ||
	    phy_addr + buffer->len >
		    sec_heap->region_pa + sec_heap->region_size ||
	    phy_addr + buffer->len <= sec_heap->region_pa) {
		pr_err("%s err. req size/pa is invalid! heap:%s\n", __func__,
		       dma_heap_get_name(buffer->heap));
		mutex_unlock(&buffer->map_lock);
		return ERR_PTR(-ENOMEM);
	}
	dma_address = phy_addr - sec_heap->region_pa +
		      sg_dma_address(sec_heap->region_table->sgl);
	sg_dma_address(table->sgl) = dma_address;
	sg_dma_len(table->sgl) = buffer->len;

map_done:
	ret = fill_sec_buffer_info(buffer, table, attachment, direction, tab_id,
				   dom_id);
	if (ret) {
		pr_err("%s failed, fill_sec_buffer_info failed, dev:%s\n",
		       __func__, dev_name(attachment->dev));
		mutex_unlock(&buffer->map_lock);
		return ERR_PTR(-ENOMEM);
	}
	a->mapped = true;

	pr_debug(
		"%s done, dev:%s, sec_handle:%llu, len:%#x(%#x), pa:%#llx, iova:%#lx, id:(%d,%d)\n",
		__func__, dev_name(attachment->dev), buffer->sec_handle,
		buffer->len, sg_dma_len(table->sgl), phy_addr,
		(unsigned long)sg_dma_address(table->sgl), tab_id, dom_id);
	mutex_unlock(&buffer->map_lock);

	return table;
}

static void mtk_sec_heap_unmap_dma_buf(struct dma_buf_attachment *attachment,
				       struct sg_table *table,
				       enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	int attr = attachment->dma_map_attrs | DMA_ATTR_SKIP_CPU_SYNC;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(attachment->dev);

	if (!fwspec) {
		pr_debug("%s, non-iommu-dev unmap, dev:%s\n", __func__,
			 dev_name(attachment->dev));
		dma_unmap_sgtable(attachment->dev, table, direction, attr);
	}
	a->mapped = false;
}

static bool heap_is_region_base(enum HEAP_BASE_TYPE heap_type)
{
	if (heap_type == REGION_BASE)
		return true;

	return false;
}

int fill_heap_sgtable(struct secure_heap_region *sec_heap,
		struct mtk_sec_heap_buffer *buffer)
{
	int ret;

	if (!heap_is_region_base(sec_heap->heap_type)) {
		pr_info("%s skip page base filled\n", __func__);
		return 0;
	}

	mutex_lock(&sec_heap->heap_lock);
	if (sec_heap->heap_filled) {
		mutex_unlock(&sec_heap->heap_lock);
		pr_debug("%s, %s already filled\n", __func__,
			 dma_heap_get_name(buffer->heap));
		return 0;
	}

	ret = trusted_mem_api_get_region_info(sec_heap->tmem_type,
					      &sec_heap->region_pa,
					      &sec_heap->region_size);
	if (!ret) {
		mutex_unlock(&sec_heap->heap_lock);
		pr_err("%s, [%s],get_region_info failed!\n", __func__,
		       dma_heap_get_name(buffer->heap));
		return -EINVAL;
	}

	sec_heap->region_table =
		kzalloc(sizeof(*sec_heap->region_table), GFP_KERNEL);
	if (!sec_heap->region_table) {
		mutex_unlock(&sec_heap->heap_lock);
		pr_err("%s, [%s] kzalloc_sgtable failed\n", __func__,
		       dma_heap_get_name(buffer->heap));
		return ret;
	}
	ret = sg_alloc_table(sec_heap->region_table, 1, GFP_KERNEL);
	if (ret) {
		mutex_unlock(&sec_heap->heap_lock);
		pr_err("%s, [%s] alloc_sgtable failed\n", __func__,
		       dma_heap_get_name(buffer->heap));
		return ret;
	}
	sg_set_page(sec_heap->region_table->sgl,
		    phys_to_page(sec_heap->region_pa), sec_heap->region_size,
		    0);
	sec_heap->heap_filled = true;
	mutex_unlock(&sec_heap->heap_lock);
	pr_debug("%s [%s] fill done, region_pa:%pa, region_size:0x%x\n",
		 __func__, dma_heap_get_name(buffer->heap),
		 &sec_heap->region_pa, sec_heap->region_size);
	return 0;
}

static int mtk_sec_heap_dma_buf_get_flags(struct dma_buf *dmabuf,
					  unsigned long *flags)
{
	struct mtk_sec_heap_buffer *buffer = dmabuf->priv;

	*flags = buffer->uncached;

	return 0;
}

const struct dma_buf_ops sec_buf_region_ops = {
	/* one attachment can only map once */
	.cache_sgt_mapping = 1,
	.attach = mtk_sec_heap_attach,
	.detach = mtk_sec_heap_detach,
	.map_dma_buf = mtk_sec_heap_region_map_dma_buf,
	.unmap_dma_buf = mtk_sec_heap_unmap_dma_buf,
	.release = tmem_region_free,
	.get_flags = mtk_sec_heap_dma_buf_get_flags,
};

const struct dma_buf_ops sec_buf_page_ops = {
	/* one attachment can only map once */
	.cache_sgt_mapping = 1,
	.attach = mtk_sec_heap_attach,
	.detach = mtk_sec_heap_detach,
	.map_dma_buf = mtk_sec_heap_page_map_dma_buf,
	.unmap_dma_buf = mtk_sec_heap_unmap_dma_buf,
	.release = tmem_page_free,
	.get_flags = mtk_sec_heap_dma_buf_get_flags,
};

static int is_region_base_dmabuf(const struct dma_buf *dmabuf)
{
	return dmabuf && dmabuf->ops == &sec_buf_region_ops;
}

static int is_page_base_dmabuf(const struct dma_buf *dmabuf)
{
	return dmabuf && dmabuf->ops == &sec_buf_page_ops;
}

void init_buffer_info(struct dma_heap *heap,
		struct mtk_sec_heap_buffer *buffer)
{
	struct task_struct *task = current->group_leader;

	// add x, result for 32bit project compile the arithmetic division
	unsigned long long x;

	INIT_LIST_HEAD(&buffer->attachments);
	INIT_LIST_HEAD(&buffer->iova_caches);
	mutex_init(&buffer->lock);
	mutex_init(&buffer->map_lock);
	/* add alloc pid & tid info */
	get_task_comm(buffer->pid_name, task);
	get_task_comm(buffer->tid_name, current);
	buffer->pid = task_pid_nr(task);
	buffer->tid = task_pid_nr(current);

	/*
	 * use do_div to instead of "/" division
	 *
	 * orginal arithmetic division code as following
	 * buffer->ts  = sched_clock() / 1000;
	 */
	x = sched_clock();
	do_div(x, 1000);
	buffer->ts = x;
}

struct dma_buf *alloc_dmabuf(struct dma_heap *heap,
		struct mtk_sec_heap_buffer *buffer,
		const struct dma_buf_ops *ops, u32 fd_flags)
{
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	/* create the dmabuf */
	exp_info.exp_name = dma_heap_get_name(heap);
	exp_info.ops = ops;
	exp_info.size = buffer->len;
	exp_info.flags = fd_flags;
	exp_info.priv = buffer;

	return dma_buf_export(&exp_info);
}

const struct dma_heap_ops sec_heap_page_ops = {
	.allocate = tmem_page_alloc,
};

const struct dma_heap_ops sec_heap_region_ops = {
	.allocate = tmem_region_alloc,
};

int sec_buf_priv_dump(const struct dma_buf *dmabuf, struct seq_file *s)
{
	struct iova_cache_data *cache_data, *temp_data;
	unsigned int i = 0, j = 0;
	dma_addr_t iova = 0;
	int region_buf = 0;
	struct mtk_sec_heap_buffer *buf = dmabuf->priv;
	u64 sec_handle = 0;

	dmabuf_dump(
		s,
		"\t\tbuf_priv: uncached:%d alloc_pid:%d(%s)tid:%d(%s) alloc_time:%lluus\n",
		!!buf->uncached, buf->pid, buf->pid_name, buf->tid,
		buf->tid_name, buf->ts);

	/* region base, only has secure handle */
	if (is_page_base_dmabuf(dmabuf)) {
		region_buf = 0;
	} else if (is_region_base_dmabuf(dmabuf)) {
		region_buf = 1;
	} else {
		WARN_ON(1);
		return 0;
	}

	list_for_each_entry_safe(cache_data, temp_data, &buf->iova_caches, iova_caches) {
		for (j = 0; j < MTK_M4U_DOM_NR_MAX; j++) {
			bool mapped = cache_data->mapped[j];
			struct device *dev = cache_data->dev_info[j].dev;
			struct sg_table *sgt = cache_data->mapped_table[j];
			char tmp_str[40];
			int len = 0;

			if (!sgt || !sgt->sgl || !dev ||
			    !dev_iommu_fwspec_get(dev))
				continue;

			iova = sg_dma_address(sgt->sgl);

			if (region_buf) {
				sec_handle =
					(dma_addr_t)dmabuf_to_secure_handle(
						dmabuf);
				len = scnprintf(tmp_str, 39, "sec_handle:%#llx",
						sec_handle);
				if (len >= 0)
					tmp_str[len] =
						'\0'; /* No need memset */
			}
			dmabuf_dump(
				s,
				"\t\tbuf_priv: tab:%-2u dom:%-2u map:%d iova:%#-12lx %s attr:%#-4lx dir:%-2d dev:%s\n",
				i, j, mapped, (unsigned long)iova,
				region_buf ? tmp_str : "",
				cache_data->dev_info[j].map_attrs,
				cache_data->dev_info[j].direction,
				dev_name(cache_data->dev_info[j].dev));
		}
	}

	return 0;
}

/**
 * return none-zero value means dump fail.
 *       maybe the input dmabuf isn't this heap buffer, no need dump
 *
 * return 0 means dump pass
 */
static int sec_heap_buf_priv_dump(const struct dma_buf *dmabuf,
				  struct dma_heap *heap, void *priv)
{
	struct mtk_sec_heap_buffer *buf = dmabuf->priv;
	struct seq_file *s = priv;

	if (!is_mtk_sec_heap_dmabuf(dmabuf) || heap != buf->heap)
		return -EINVAL;

	if (buf->show)
		return buf->show(dmabuf, s);

	return -EINVAL;
}

static struct mtk_heap_priv_info mtk_sec_heap_priv = {
	.buf_priv_dump = sec_heap_buf_priv_dump,
};

int is_mtk_sec_heap_dmabuf(const struct dma_buf *dmabuf)
{
	if (IS_ERR_OR_NULL(dmabuf))
		return 0;

	if (dmabuf->ops == &sec_buf_page_ops ||
	    dmabuf->ops == &sec_buf_region_ops)
		return 1;

	return 0;
}
EXPORT_SYMBOL_GPL(is_mtk_sec_heap_dmabuf);

int is_support_secure_handle(const struct dma_buf *dmabuf)
{
	if (IS_ERR_OR_NULL(dmabuf))
		return 0;

	return is_region_base_dmabuf(dmabuf);
}
EXPORT_SYMBOL_GPL(is_support_secure_handle);

/* return 0 means error */
u64 dmabuf_to_secure_handle(const struct dma_buf *dmabuf)
{
	int heap_base;
	struct mtk_sec_heap_buffer *buffer;

	if (!is_mtk_sec_heap_dmabuf(dmabuf)) {
		pr_err("%s err, dmabuf is not secure\n", __func__);
		return 0;
	}
	buffer = dmabuf->priv;
	heap_base = get_heap_base_type(buffer->heap);
	if (heap_base != REGION_BASE && heap_base != PAGE_BASE) {
		pr_warn("%s failed, heap(%s) not support sec_handle\n",
			__func__, dma_heap_get_name(buffer->heap));
		return 0;
	}

	return buffer->sec_handle;
}
EXPORT_SYMBOL_GPL(dmabuf_to_secure_handle);

#if (!(IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)))

int dmabuf_to_sec_id(const struct dma_buf *dmabuf, u32 *sec_hdl)
{
	struct mtk_sec_heap_buffer *buffer = NULL;
	struct secure_heap_region *sec_heap = NULL;

	if (!is_region_base_dmabuf(dmabuf)) {
		pr_err("%s err, dmabuf is not region base\n", __func__);
		return -1;
	}

	*sec_hdl = dmabuf_to_secure_handle(dmabuf);

	buffer = dmabuf->priv;
	sec_heap = sec_heap_region_get(buffer->heap);
	if (!sec_heap) {
		pr_err("%s, sec_heap_region_get(%s) failed!!\n", __func__,
		       buffer->heap ? dma_heap_get_name(buffer->heap) :
		       "null ptr");
		return -1;
	}

	return tmem_type2sec_id(sec_heap->tmem_type);
}
EXPORT_SYMBOL_GPL(dmabuf_to_sec_id);

int dmabuf_to_tmem_type(const struct dma_buf *dmabuf, u32 *sec_hdl)
{
	struct mtk_sec_heap_buffer *buffer = NULL;
	struct secure_heap_region *sec_heap = NULL;

	if (!is_region_base_dmabuf(dmabuf)) {
		pr_err("%s err, dmabuf is not region base\n", __func__);
		return -1;
	}

	*sec_hdl = dmabuf_to_secure_handle(dmabuf);

	buffer = dmabuf->priv;
	sec_heap = sec_heap_region_get(buffer->heap);
	if (!sec_heap) {
		pr_err("%s, sec_heap_region_get(%s) failed!!\n", __func__,
		       buffer->heap ? dma_heap_get_name(buffer->heap) :
		       "null ptr");
		return -1;
	}

	return sec_heap->tmem_type;
}
EXPORT_SYMBOL_GPL(dmabuf_to_tmem_type);
#endif

static int mtk_region_heap_create(void)
{
	struct dma_heap_export_info exp_info = {0};
	int i = 0, j, ret;

	/* region base & page base use same heap show */
	exp_info.priv = (void *)&mtk_sec_heap_priv;

	/* No need pagepool for secure heap */
	exp_info.ops = &sec_heap_region_ops;

	for (i = 0; i < mtk_sec_region_count; i++) {
		for (j = REGION_HEAP_NORMAL; j < REGION_TYPE_NUM; j++) {
			if (!mtk_sec_heap_region[i].heap_name[j])
				continue;
			exp_info.name = mtk_sec_heap_region[i].heap_name[j];
			mtk_sec_heap_region[i].heap[j] = dma_heap_add(&exp_info);
			if (IS_ERR(mtk_sec_heap_region[i].heap[j]))
				return PTR_ERR(mtk_sec_heap_region[i].heap[j]);

			ret = dma_set_mask_and_coherent(
				mtk_sec_heap_region[i].heap_dev,
				DMA_BIT_MASK(34));
			if (ret) {
				dev_info(
					mtk_sec_heap_region[i].heap_dev,
					"dma_set_mask_and_coherent failed: %d\n",
					ret);
				return ret;
			}
			mutex_init(&mtk_sec_heap_region[i].heap_lock);

			pr_info("%s add heap[%s][%d] dev:%s, success\n",
				__func__, exp_info.name,
				mtk_sec_heap_region[i].tmem_type,
				dev_name(mtk_sec_heap_region[i].heap_dev));
		}
	}

	return 0;
}

static struct list_head secure_heap_config_data;
static int mtk_dma_heap_config_probe(struct platform_device *pdev)
{
	struct mtk_dma_heap_config *heap_config;
	int ret;

	heap_config = kzalloc(sizeof(*heap_config), GFP_KERNEL);
	if (!heap_config)
		return -ENOMEM;

	ret = mtk_dma_heap_config_parse(&pdev->dev, heap_config);
	if (ret)
		return ret;

	list_add(&heap_config->list_node, &secure_heap_config_data);
	return 0;
}

static const struct mtk_dma_heap_match_data dmaheap_data_mtk_region = {
	.dmaheap_type = DMA_HEAP_MTK_SEC_REGION,
};

static const struct mtk_dma_heap_match_data dmaheap_data_mtk_page = {
	.dmaheap_type = DMA_HEAP_MTK_SEC_PAGE,
};

static const struct of_device_id mtk_dma_heap_match_table[] = {
	{.compatible = "mediatek,dmaheap-mtk-sec-region", .data = &dmaheap_data_mtk_region},
	{.compatible = "mediatek,dmaheap-mtk-sec-page", .data = &dmaheap_data_mtk_page},
	{},
};

static struct platform_driver mtk_dma_heap_config_driver = {
	.probe = mtk_dma_heap_config_probe,
	.driver = {
		.name = "mtk-dma-heap-secure",
		.of_match_table = mtk_dma_heap_match_table,
	},
};

static int set_heap_dev_dma(struct device *heap_dev)
{
	if (!heap_dev)
		return -EINVAL;

	dma_coerce_mask_and_coherent(heap_dev, DMA_BIT_MASK(64));

	if (!heap_dev->dma_parms) {
		heap_dev->dma_parms = devm_kzalloc(
			heap_dev, sizeof(*heap_dev->dma_parms), GFP_KERNEL);
		if (!heap_dev->dma_parms)
			return -ENOMEM;

		dma_set_max_seg_size(heap_dev,
					   (unsigned int)DMA_BIT_MASK(64));
	}

	return 0;
}

static int mtk_dma_heap_config_alloc(void)
{
	struct mtk_dma_heap_config *heap_config, *temp_config;
	struct secure_heap_region *heap_region;
	struct secure_heap_page *heap_page;
	int i = 0, j = 0;

	list_for_each_entry(heap_config, &secure_heap_config_data, list_node) {
		if (heap_config->heap_type == DMA_HEAP_MTK_SEC_PAGE)
			mtk_sec_page_count++;
		else if (heap_config->heap_type == DMA_HEAP_MTK_SEC_REGION)
			mtk_sec_region_count++;
	}

	pr_info("%s page count:%d,region count:%d\n", __func__,
		mtk_sec_page_count, mtk_sec_region_count);

	mtk_sec_heap_page = kcalloc(mtk_sec_page_count, sizeof(*mtk_sec_heap_page), GFP_KERNEL);
	if (!mtk_sec_heap_page)
		goto alloc_page_fail;

	mtk_sec_heap_region = kcalloc(mtk_sec_region_count, sizeof(*mtk_sec_heap_region),
				      GFP_KERNEL);
	if (!mtk_sec_heap_region)
		goto alloc_region_fail;

	memset(mtk_sec_heap_page, 0, sizeof(*mtk_sec_heap_page) * mtk_sec_page_count);
	memset(mtk_sec_heap_region, 0, sizeof(*mtk_sec_heap_region) * mtk_sec_region_count);

	list_for_each_entry_safe(heap_config, temp_config, &secure_heap_config_data, list_node) {
		if (heap_config->heap_type == DMA_HEAP_MTK_SEC_PAGE && i < mtk_sec_page_count) {
			heap_page = &mtk_sec_heap_page[i];
			heap_page->heap_type = PAGE_BASE;
			heap_page->tmem_type = heap_config->trusted_mem_type;
			heap_page->heap_dev = heap_config->dev;
			heap_page->heap_name = heap_config->heap_name;
			i++;
		} else if (heap_config->heap_type == DMA_HEAP_MTK_SEC_REGION &&
			   j < mtk_sec_region_count) {
			heap_region = &mtk_sec_heap_region[j];
			heap_region->heap_type = REGION_BASE;
			heap_region->tmem_type = heap_config->trusted_mem_type;
			heap_region->heap_dev = heap_config->dev;
			heap_region->iommu_dev = mtk_smmu_get_shared_device(heap_config->dev);
			heap_region->heap_name[REGION_HEAP_NORMAL] = heap_config->heap_name;
			heap_region->heap_name[REGION_HEAP_ALIGN] =
							heap_config->region_heap_align_name;
			j++;
		}

		list_del(&heap_config->list_node);
		kfree(heap_config);
	}

	return 0;

alloc_region_fail:
	kfree(mtk_sec_heap_page);
	mtk_sec_heap_page = NULL;

alloc_page_fail:
	mtk_sec_page_count = 0;
	mtk_sec_region_count = 0;

	list_for_each_entry_safe(heap_config, temp_config, &secure_heap_config_data, list_node) {
		list_del(&heap_config->list_node);
		kfree(heap_config);
	}
	return -ENOMEM;
}

static int mtk_page_heap_create(void)
{
	struct dma_heap_export_info exp_info = {0};
	int i, j;
	int err = 0;

	/* page pool */
	for (i = 0; i < SEC_NUM_ORDERS; i++) {
		sec_pools[i] = dmabuf_page_pool_create(sec_order_flags[i],
				sec_orders[i]);
		if (IS_ERR(sec_pools[i])) {
			pr_err("%s: page pool creation failed!\n", __func__);
			for (j = 0; j < i; j++)
				dmabuf_page_pool_destroy(sec_pools[j]);
			return PTR_ERR(sec_pools[i]);
		}
	}

	exp_info.ops = &sec_heap_page_ops;
	for (i = 0; i < mtk_sec_page_count; i++) {
		/* param check */
		if (mtk_sec_heap_page[i].heap_type != PAGE_BASE) {
			pr_info("invalid heap param, %s, %d\n",
				mtk_sec_heap_page[i].heap_name,
				mtk_sec_heap_page[i].heap_type);
			continue;
		}

		exp_info.name = mtk_sec_heap_page[i].heap_name;

		mtk_sec_heap_page[i].heap = dma_heap_add(&exp_info);
		if (IS_ERR(mtk_sec_heap_page[i].heap)) {
			pr_err("%s error, dma_heap_add failed, heap:%s\n",
			       __func__, mtk_sec_heap_page[i].heap_name);
			return PTR_ERR(mtk_sec_heap_page[i].heap);
		}
		err = set_heap_dev_dma(
			dma_heap_get_dev(mtk_sec_heap_page[i].heap));
		if (err) {
			pr_err("%s add heap[%s][%d] failed\n", __func__,
			       exp_info.name, mtk_sec_heap_page[i].tmem_type);
			return err;
		}
		mtk_sec_heap_page[i].bitmap = alloc_page(GFP_KERNEL | __GFP_ZERO);
		pr_info("%s add heap[%s][%d] success\n", __func__,
			exp_info.name, mtk_sec_heap_page[i].tmem_type);
	}
	return 0;
}

static int __init mtk_sec_heap_init(void)
{
	int ret;

	pr_info("%s+\n", __func__);

	smmu_v3_enable = smmu_v3_enabled();

	INIT_LIST_HEAD(&secure_heap_config_data);
	ret = platform_driver_register(&mtk_dma_heap_config_driver);
	if (ret < 0) {
		pr_info("%s fail to register dma heap: %d\n", __func__, ret);
		return ret;
	}

	ret = mtk_dma_heap_config_alloc();
	if (ret) {
		pr_info("%s secure heap alloc failed\n", __func__);
		goto err;
	}

	ret = mtk_page_heap_create();
	if (ret) {
		pr_info("%s page_base_heap_create failed\n", __func__);
		goto err;
	}

	ret = mtk_region_heap_create();
	if (ret) {
		pr_info("%s region_base_heap_create failed\n", __func__);
		goto err;
	}

	pr_info("%s-\n", __func__);

	return 0;

err:
	platform_driver_unregister(&mtk_dma_heap_config_driver);

	return ret;

}

static void __exit mtk_sec_heap_exit(void)
{
	kfree(mtk_sec_heap_page);
	kfree(mtk_sec_heap_region);

	platform_driver_unregister(&mtk_dma_heap_config_driver);
}

MODULE_SOFTDEP("pre: apusys");
module_init(mtk_sec_heap_init);
module_exit(mtk_sec_heap_exit);
MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL v2");
