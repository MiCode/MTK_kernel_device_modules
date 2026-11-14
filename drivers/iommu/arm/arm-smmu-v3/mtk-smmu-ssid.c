// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Ning Li <ning.li@mediatek.com>
 *         Yunfei Wang <yf.wang@mediatek.com>
 */

#include <linux/dma-map-ops.h>
#include <linux/dma-mapping.h>
#include <linux/iova.h>
#include <linux/scatterlist.h>
#include <trace/hooks/iommu.h>

#include "iommu_debug.h"
#include "mtk-smmu-v3.h"
#include "mtk-smmu-ssid.h"

static const char *IOMMU_DMA_RANGE_PROP_NAME = "mtk,iommu-dma-range";
static const char *SMMU_SSIDS_PROP_NAME = "mtk,smmu-ssids";
static const char *SMMU_SSID_RANGE_NAME = "mtk,smmu-ssid-range";

static inline
struct iommu_domain *to_iommu_domain(struct arm_smmu_ssid_domain *ssid_domain)
{
	if (unlikely(!ssid_domain || !ssid_domain->domain))
		return NULL;

	return &ssid_domain->domain->domain;
}

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

enum iommu_dma_cookie_type {
	IOMMU_DMA_IOVA_COOKIE,
	IOMMU_DMA_MSI_COOKIE,
};

enum iommu_dma_queue_type {
	IOMMU_DMA_OPTS_PER_CPU_QUEUE,
	IOMMU_DMA_OPTS_SINGLE_QUEUE,
};

struct iommu_dma_options {
	enum iommu_dma_queue_type qt;
	size_t		fq_size;
	unsigned int	fq_timeout;
};

struct iommu_dma_cookie {
	enum iommu_dma_cookie_type	type;
	union {
		/* Full allocator for IOMMU_DMA_IOVA_COOKIE */
		struct {
			struct iova_domain	iovad;
			/* Flush queue */
			union {
				struct iova_fq	*single_fq;
				struct iova_fq	__percpu *percpu_fq;
			};
			/* Number of TLB flushes that have been started */
			atomic64_t		fq_flush_start_cnt;
			/* Number of TLB flushes that have been finished */
			atomic64_t		fq_flush_finish_cnt;
			/* Timer to regularily empty the flush queues */
			struct timer_list	fq_timer;
			/* 1 when timer is active, 0 when not */
			atomic_t		fq_timer_on;
		};
		/* Trivial linear page allocator for IOMMU_DMA_MSI_COOKIE */
		dma_addr_t		msi_iova;
	};
	struct list_head		msi_page_list;

	/* Domain for flush queue callback; NULL if flush queue not in use */
	struct iommu_domain		*fq_domain;
	/* Options for dma-iommu use */
	struct iommu_dma_options	options;
	struct mutex			mutex;
};

/* Flush queue entry for deferred flushing */
struct iova_fq_entry {
	unsigned long iova_pfn;
	unsigned long pages;
	struct list_head freelist;
	u64 counter; /* Flush counter when this entry was added */
};

/* Per-CPU flush queue structure */
struct iova_fq {
	spinlock_t lock;
	unsigned int head, tail;
	unsigned int mod_mask;
	struct iova_fq_entry entries[];
};

static struct iommu_dma_cookie *cookie_alloc(enum iommu_dma_cookie_type type)
{
	struct iommu_dma_cookie *cookie;

	cookie = kzalloc(sizeof(*cookie), GFP_KERNEL);
	if (cookie) {
		INIT_LIST_HEAD(&cookie->msi_page_list);
		cookie->type = type;
	}
	return cookie;
}

static int iommu_get_dma_cookie(struct iommu_domain *domain)
{
	if (domain->iova_cookie)
		return -EEXIST;

	domain->iova_cookie = cookie_alloc(IOMMU_DMA_IOVA_COOKIE);
	if (!domain->iova_cookie)
		return -ENOMEM;

	mutex_init(&domain->iova_cookie->mutex);
	return 0;
}

static void iommu_get_resv_regions_for_dev_ssid(struct device *dev, u32 ssid,
						struct list_head *head)
{
	struct arm_smmu_master *master = dev_iommu_priv_get(dev);
	struct arm_smmu_device *smmu;
	struct iommu_resv_region *region;
	int prot = IOMMU_WRITE | IOMMU_NOEXEC | IOMMU_MMIO;

	if (!master)
		return;

	smmu = master->smmu;
	region = iommu_alloc_resv_region(MSI_IOVA_BASE, MSI_IOVA_LENGTH,
					 prot, IOMMU_RESV_SW_MSI, GFP_KERNEL);
	if (!region) {
		dev_info(dev, "[%s] region is NULL\n", __func__);
		return;
	}

	list_add_tail(&region->list, head);

	if (smmu->impl && smmu->impl->get_resv_regions_ssid)
		smmu->impl->get_resv_regions_ssid(dev, ssid, head);
}

static int iova_reserve_iommu_regions(struct device *dev, u32 ssid,
				      struct iommu_domain *domain)
{
	struct iommu_dma_cookie *cookie = domain->iova_cookie;
	struct iova_domain *iovad = &cookie->iovad;
	struct iommu_resv_region *region;
	LIST_HEAD(resv_regions);
	struct iova *iova;

	iommu_get_resv_regions_for_dev_ssid(dev, ssid, &resv_regions);
	list_for_each_entry(region, &resv_regions, list) {
		unsigned long lo, hi;

		/* We ARE the software that manages these! */
		if (region->type == IOMMU_RESV_SW_MSI)
			continue;

		lo = iova_pfn(iovad, region->start);
		hi = iova_pfn(iovad, region->start + region->length - 1);
		iova = reserve_iova(iovad, lo, hi);

		dev_dbg(dev, "[%s] region[0x%lx, 0x%lx], iova[0x%lx, 0x%lx]\n",
			__func__, hi, lo,
			(iova ? iova->pfn_hi : 0),
			(iova ? iova->pfn_lo : 0));
	}
	iommu_put_resv_regions(dev, &resv_regions);

	return 0;
}

static int iommu_dma_init_domain(struct iommu_domain *domain,
				 struct device *dev, u32 ssid,
				 unsigned long granule,
				 unsigned long start_pfn)
{
	struct iommu_dma_cookie *cookie = domain->iova_cookie;
	struct iova_domain *iovad;
	int ret;

	if (!cookie || cookie->type != IOMMU_DMA_IOVA_COOKIE)
		return -EINVAL;

	iovad = &cookie->iovad;

	/* start_pfn is always nonzero for an already-initialised domain */
	mutex_lock(&cookie->mutex);
	if (iovad->start_pfn) {
		if (granule != iovad->granule ||
		    start_pfn != iovad->start_pfn) {
			pr_info("Incompatible range for DMA domain\n");
			ret = -EFAULT;
			goto done_unlock;
		}

		ret = 0;
		goto done_unlock;
	}

	init_iova_domain(iovad, granule, start_pfn);
	ret = iova_domain_init_rcaches(iovad);
	if (ret)
		goto done_unlock;

	ret = iova_reserve_iommu_regions(dev, ssid, domain);

done_unlock:
	mutex_unlock(&cookie->mutex);
	if (ret)
		dev_info(dev, "[%s] ret:%d\n", __func__, ret);
	return ret;
}

static bool dev_has_iommu(struct device *dev)
{
	return dev->iommu && dev->iommu->iommu_dev;
}

static struct iommu_domain *__iommu_domain_alloc(const struct iommu_ops *ops,
						 struct device *dev,
						 unsigned int type)
{
	struct iommu_domain *domain;
	unsigned int alloc_type = type & IOMMU_DOMAIN_ALLOC_FLAGS;

	if (alloc_type == IOMMU_DOMAIN_IDENTITY && ops->identity_domain)
		return ops->identity_domain;
	else if (alloc_type == IOMMU_DOMAIN_BLOCKED && ops->blocked_domain)
		return ops->blocked_domain;
	else if (type & __IOMMU_DOMAIN_PAGING && ops->domain_alloc_paging)
		domain = ops->domain_alloc_paging(dev);
	else if (ops->domain_alloc)
		domain = ops->domain_alloc(alloc_type);
	else
		return ERR_PTR(-EOPNOTSUPP);

	if (IS_ERR(domain))
		return domain;
	if (!domain)
		return ERR_PTR(-ENOMEM);

	domain->type = type;
	domain->owner = ops;

	if (!domain->pgsize_bitmap)
		domain->pgsize_bitmap = ops->pgsize_bitmap;

	if (!domain->ops)
		domain->ops = ops->default_domain_ops;

	if (iommu_is_dma_domain(domain)) {
		int rc;

		rc = iommu_get_dma_cookie(domain);
		if (rc) {
			dev_info(dev, "[%s] iommu_get_dma_cookie, ret:%d\n", __func__, rc);
			iommu_domain_free(domain);
			return ERR_PTR(rc);
		}
	} else {
		dev_info(dev, "[%s] NOT IOMMU_DOMAIN_DMA_API\n", __func__);
	}
	return domain;
}

static struct iommu_domain *iommu_domain_alloc_paging(struct device *dev,
						      unsigned int type)
{
	if (!dev_has_iommu(dev))
		return ERR_PTR(-ENODEV);

	return __iommu_domain_alloc(dev_iommu_ops(dev), dev, type);
}

static dma_addr_t iommu_dma_alloc_iova(struct arm_smmu_ssid_domain *ssid_domain,
				       size_t size, u64 dma_limit,
				       struct device *dev)
{
	struct iommu_domain *domain = &ssid_domain->domain->domain;
	struct iommu_dma_cookie *cookie = domain->iova_cookie;
	struct iova_domain *iovad = &cookie->iovad;
	unsigned long shift, iova_len, iova;
	dma_addr_t iova_addr;

	shift = iova_shift(iovad);
	iova_len = size >> shift;

	dma_limit = min_not_zero(dma_limit, dev->bus_dma_limit);

	if (domain->geometry.force_aperture)
		dma_limit = min(dma_limit, (u64)domain->geometry.aperture_end);

	iova = alloc_iova_fast(iovad, iova_len, dma_limit >> shift, true);
	iova_addr = (dma_addr_t)(iova << shift);

	/* trace_android_vh_iommu_iovad_alloc_iova */
	mtk_iova_alloc(dev, ssid_domain->ssid, iovad, iova_addr, size);
	dev_dbg(dev, "[%s] iova:0x%llx, size:0x%zx, shift:%lu\n",
		__func__, iova_addr, size, shift);

	return iova_addr;
}

static void iommu_dma_free_iova(struct arm_smmu_ssid_domain *ssid_domain,
				dma_addr_t iova, size_t size,
				struct iommu_iotlb_gather *gather,
				struct device *dev)
{
	struct iommu_domain *domain = &ssid_domain->domain->domain;
	struct iommu_dma_cookie *cookie = domain->iova_cookie;
	struct iova_domain *iovad = &cookie->iovad;

	free_iova_fast(iovad, iova_pfn(iovad, iova), size >> iova_shift(iovad));

	/* trace_android_vh_iommu_iovad_free_iova */
	mtk_iova_free(dev, ssid_domain->ssid, iovad, iova, size);
	dev_dbg(dev, "[%s] iova:0x%llx, size:0x%zx\n", __func__, iova, size);
}

static dma_addr_t iommu_dma_map(struct arm_smmu_ssid_domain *ssid_domain,
				phys_addr_t phys, size_t size, int prot,
				u64 dma_mask, struct device *dev)
{
	struct iommu_domain *domain = &ssid_domain->domain->domain;
	struct iommu_dma_cookie *cookie = domain->iova_cookie;
	struct iova_domain *iovad = &cookie->iovad;
	size_t iova_off = iova_offset(iovad, phys);
	dma_addr_t iova;
	int ret;

	size = iova_align(iovad, size + iova_off);

	iova = iommu_dma_alloc_iova(ssid_domain, size, dma_mask, dev);
	if (!iova) {
		dev_info(dev, "[%s] alloc iova fail\n", __func__);
		return DMA_MAPPING_ERROR;
	}

	ret = iommu_map(domain, iova, phys - iova_off, size, prot, GFP_ATOMIC);
	if (ret) {
		dev_info(dev, "[%s] map iova fail, iova:0x%llx, ret:%d\n",
			 __func__, iova, ret);
		iommu_dma_free_iova(ssid_domain, iova, size, NULL, dev);
		return DMA_MAPPING_ERROR;
	}

	dev_dbg(dev, "[%s] iova:0x%llx, phys:0x%llx, size:0x%zx\n",
		__func__, (iova + iova_off), phys, size);

	return iova + iova_off;
}

static void iommu_dma_unmap(struct arm_smmu_ssid_domain *ssid_domain,
			    dma_addr_t dma_addr, size_t size,
			    struct device *dev)
{
	struct iommu_domain *domain = &ssid_domain->domain->domain;
	struct iommu_dma_cookie *cookie = domain->iova_cookie;
	struct iova_domain *iovad = &cookie->iovad;
	size_t iova_off = iova_offset(iovad, dma_addr);
	struct iommu_iotlb_gather iotlb_gather;
	size_t unmapped;

	dma_addr -= iova_off;
	size = iova_align(iovad, size + iova_off);
	iommu_iotlb_gather_init(&iotlb_gather);

	unmapped = iommu_unmap_fast(domain, dma_addr, size, &iotlb_gather);
	WARN_ON(unmapped != size);

	dev_dbg(dev, "[%s] iova:0x%llx, size:0x%zx\n", __func__, dma_addr, size);

	iommu_iotlb_sync(domain, &iotlb_gather);

	iommu_dma_free_iova(ssid_domain, dma_addr, size, &iotlb_gather, dev);
}

static int dma_info_to_prot(enum dma_data_direction dir, bool coherent,
			    unsigned long attrs)
{
	int prot = coherent ? IOMMU_CACHE : 0;

	if (attrs & DMA_ATTR_PRIVILEGED)
		prot |= IOMMU_PRIV;

	trace_android_rvh_iommu_dma_info_to_prot(attrs, &prot);

	switch (dir) {
	case DMA_BIDIRECTIONAL:
		return prot | IOMMU_READ | IOMMU_WRITE;
	case DMA_TO_DEVICE:
		return prot | IOMMU_READ;
	case DMA_FROM_DEVICE:
		return prot | IOMMU_WRITE;
	default:
		return 0;
	}
}

static int __finalise_sg(struct device *dev, struct scatterlist *sg, int nents,
			 dma_addr_t dma_addr)
{
	struct scatterlist *s, *cur = sg;
	unsigned long seg_mask = dma_get_seg_boundary(dev);
	unsigned int cur_len = 0, max_len = dma_get_max_seg_size(dev);
	int i, count = 0;

	for_each_sg(sg, s, nents, i) {
		/* Restore this segment's original unaligned fields first */
		dma_addr_t s_dma_addr = sg_dma_address(s);
		unsigned int s_iova_off = sg_dma_address(s);
		unsigned int s_length = sg_dma_len(s);
		unsigned int s_iova_len = s->length;

		sg_dma_address(s) = DMA_MAPPING_ERROR;
		sg_dma_len(s) = 0;

		if (sg_dma_is_bus_address(s)) {
			if (i > 0)
				cur = sg_next(cur);

			sg_dma_unmark_bus_address(s);
			sg_dma_address(cur) = s_dma_addr;
			sg_dma_len(cur) = s_length;
			sg_dma_mark_bus_address(cur);
			count++;
			cur_len = 0;
			continue;
		}

		s->offset += s_iova_off;
		s->length = s_length;

		/*
		 * Now fill in the real DMA data. If...
		 * - there is a valid output segment to append to
		 * - and this segment starts on an IOVA page boundary
		 * - but doesn't fall at a segment boundary
		 * - and wouldn't make the resulting output segment too long
		 */
		if (cur_len && !s_iova_off && (dma_addr & seg_mask) &&
		    (max_len - cur_len >= s_length)) {
			/* ...then concatenate it with the previous one */
			cur_len += s_length;
		} else {
			/* Otherwise start the next output segment */
			if (i > 0)
				cur = sg_next(cur);
			cur_len = s_length;
			count++;

			sg_dma_address(cur) = dma_addr + s_iova_off;
		}

		sg_dma_len(cur) = cur_len;
		dma_addr += s_iova_len;

		if (s_length + s_iova_off < s_iova_len)
			cur_len = 0;
	}
	return count;
}

static void __invalidate_sg(struct scatterlist *sg, int nents)
{
	struct scatterlist *s;
	int i;

	for_each_sg(sg, s, nents, i) {
		if (sg_dma_is_bus_address(s)) {
			sg_dma_unmark_bus_address(s);
		} else {
			if (sg_dma_address(s) != DMA_MAPPING_ERROR)
				s->offset += sg_dma_address(s);
			if (sg_dma_len(s))
				s->length = sg_dma_len(s);
		}
		sg_dma_address(s) = DMA_MAPPING_ERROR;
		sg_dma_len(s) = 0;
	}
}

static int smmu_rpm_get(struct arm_smmu_device *smmu)
{
	if (smmu && smmu->impl && smmu->impl->smmu_power_get)
		return smmu->impl->smmu_power_get(smmu);

	return 0;
}

static int smmu_rpm_put(struct arm_smmu_device *smmu)
{
	if (smmu && smmu->impl && smmu->impl->smmu_power_put)
		return smmu->impl->smmu_power_put(smmu);

	return 0;
}

static struct arm_smmu_ssid_domain *
smmu_ssid_domain_find(struct arm_smmu_domain *smmu_domain, u32 ssid)
{
	struct arm_smmu_ssid_domain *ssid_domain = NULL;
	struct arm_smmu_ssid_domain key = {
		.ssid = ssid,
	};
	struct rb_node *node;
	unsigned long flags;

	if (!smmu_domain || ssid == SMMU_NO_SSID)
		return NULL;

	spin_lock_irqsave(&smmu_domain->ssid_domains_lock, flags);
	node = rb_find(&key, &smmu_domain->ssid_domains, smmu_ssid_rb_cmp);
	if (node)
		ssid_domain = rb_entry(node, struct arm_smmu_ssid_domain, node);
	spin_unlock_irqrestore(&smmu_domain->ssid_domains_lock, flags);

	return ssid_domain;
}

static int smmu_ssid_domain_add(struct arm_smmu_domain *smmu_domain,
				struct arm_smmu_ssid_domain *ssid_domain)
{
	unsigned long flags;

	if (!smmu_domain || !ssid_domain)
		return -EINVAL;

	spin_lock_irqsave(&smmu_domain->ssid_domains_lock, flags);
	rb_add(&ssid_domain->node, &smmu_domain->ssid_domains, smmu_ssid_rb_less);
	spin_unlock_irqrestore(&smmu_domain->ssid_domains_lock, flags);

	return 0;
}

static int smmu_ssid_domain_delete(struct arm_smmu_domain *smmu_domain,
				   struct arm_smmu_ssid_domain *ssid_domain)
{
	unsigned long flags;

	if (!smmu_domain || !ssid_domain)
		return -EINVAL;

	spin_lock_irqsave(&smmu_domain->ssid_domains_lock, flags);
	rb_erase(&ssid_domain->node, &smmu_domain->ssid_domains);
	spin_unlock_irqrestore(&smmu_domain->ssid_domains_lock, flags);

	return 0;
}

static void smmu_ssid_domain_release(struct arm_smmu_master *master,
				     struct arm_smmu_ssid_domain *ssid_domain)
{
	struct iommu_domain *domain = to_iommu_domain(ssid_domain);

	if (!domain)
		return;

	arm_smmu_remove_dev_ssid(master->dev, ssid_domain->ssid, domain);
	iommu_domain_free(domain);
	kfree(ssid_domain);
}

static struct arm_smmu_ssid_domain *
smmu_ssid_domain_for_dev_ssid(struct device *dev, u32 ssid)
{
	struct arm_smmu_domain *smmu_domain;
	struct iommu_domain *domain;

	if (ssid == SMMU_NO_SSID)
		return NULL;

	domain = iommu_get_domain_for_dev(dev);
	if (WARN_ON(!domain || IS_ERR(domain)))
		return NULL;

	smmu_domain = to_smmu_domain(domain);

	return smmu_ssid_domain_find(smmu_domain, ssid);
}

static struct iommu_domain *
iommu_get_domain_for_dev_ssid(struct device *dev, u32 ssid)
{
	struct arm_smmu_ssid_domain *ssid_domain;
	struct iommu_domain *domain;

	if (ssid != SMMU_NO_SSID) {
		ssid_domain = smmu_ssid_domain_for_dev_ssid(dev, ssid);
		domain = to_iommu_domain(ssid_domain);
	} else {
		domain = iommu_get_domain_for_dev(dev);
	}

	return domain;
}

static struct io_pgtable_ops *get_smmu_pgtable_ops(struct device *dev, u32 ssid)
{
	struct iommu_domain *domain;
	struct io_pgtable_ops *ops;

	domain = iommu_get_domain_for_dev_ssid(dev, ssid);
	if (!domain) {
		dev_info(dev, "[%s] no ssid domain, ssid:%u\n", __func__, ssid);
		return NULL;
	}

	ops = to_smmu_domain(domain)->pgtbl_ops;
	if (!ops) {
		dev_info(dev, "[%s] no pgtable ops, ssid:%u\n", __func__, ssid);
		return NULL;
	}

	return ops;
}

static u64 get_smmu_tab_id_by_ssid_domain(struct arm_smmu_ssid_domain *ssid_domain)
{
	struct mtk_smmu_data *data;
	u64 ssid, asid;
	u64 smmu_id;

	if (!ssid_domain || IS_ERR(ssid_domain))
		return SMMU_TAB_ID_INVALID;

	data = to_mtk_smmu_data(ssid_domain->domain->smmu);
	smmu_id = data->plat_data->smmu_type;
	ssid = ssid_domain->ssid;
	asid = ssid_domain->asid;

	return (smmu_id << SMMU_ID_SHIFT) | (ssid << SMMU_SSID_SHIFT) | asid;
}

u64 mtk_smmu_get_tab_id_ssid(struct device *dev, u32 ssid)
{
	struct arm_smmu_ssid_domain *ssid_domain;
	struct iommu_domain *domain;

	if (ssid != SMMU_NO_SSID) {
		ssid_domain = smmu_ssid_domain_for_dev_ssid(dev, ssid);
		domain = to_iommu_domain(ssid_domain);
		if (!domain) {
			dev_info(dev, "[%s] no ssid domain, ssid:%u\n", __func__, ssid);
			return SMMU_TAB_ID_INVALID;
		}

		return get_smmu_tab_id_by_ssid_domain(ssid_domain);
	} else {
		return get_smmu_tab_id(dev);
	}
}

struct device_node *of_find_smmu_ssid_node(struct device *dev, u32 ssid)
{
	struct device_node *ssids_node;
	struct device_node *child;
	u32 ssid_val;

	if (!dev->of_node || ssid == SMMU_NO_SSID)
		return NULL;

	ssids_node = of_get_child_by_name(dev->of_node, SMMU_SSIDS_PROP_NAME);
	if (!ssids_node) {
		dev_dbg(dev, "[%s] no %s node found\n",
			__func__, SMMU_SSIDS_PROP_NAME);
		return NULL;
	}

	for_each_child_of_node(ssids_node, child) {
		if (of_property_read_u32(child, "ssid", &ssid_val) == 0) {
			if (ssid_val == ssid) {
				dev_dbg(dev, "[%s] find ssid:%u node:%s\n",
					__func__, ssid, child->full_name);
				of_node_put(ssids_node);
				return child;
			}
		}
	}

	of_node_put(ssids_node);
	return NULL;
}

struct device_node *mtk_parse_dma_region_ssid(struct device *dev, u32 ssid)
{
	struct device_node *np = of_find_smmu_ssid_node(dev, ssid);
	int len, i = 0;

	/* If no ssid node, find mtk,smmu-ssids node */
	if (!np) {
		np = of_get_child_by_name(dev->of_node, SMMU_SSIDS_PROP_NAME);
		if (!np)
			return NULL;

		if (of_get_property(np, IOMMU_DMA_RANGE_PROP_NAME, &len)) {
			dev_dbg(dev, "[%s] ssid:%u use node:%s\n",
				__func__, ssid, np->full_name);
			return np;
		}

		of_node_put(np);
		return NULL;
	}

	/* If No iommu-dma-range property use the parent node */
	for (; np && i < 2; np = np->parent, i++) {
		if (of_get_property(np, IOMMU_DMA_RANGE_PROP_NAME, &len)) {
			dev_dbg(dev, "[%s] ssid:%u node:%s\n",
				__func__, ssid, np->full_name);
			return np;
		}
	}

	return NULL;
}

int of_find_smmu_ssid_range(struct arm_smmu_master *master, u32 *ssid_range)
{
	u32 ssid_max = (1 << master->ssid_bits) - 1;
	struct device *dev = master->dev;
	struct device_node *ssids_node;
	int ret = 0;

	if (!dev || !dev->of_node)
		return -EINVAL;

	ssids_node = of_get_child_by_name(dev->of_node, SMMU_SSIDS_PROP_NAME);
	if (!ssids_node) {
		dev_dbg(dev, "[%s] no %s node found\n",
			 __func__, SMMU_SSIDS_PROP_NAME);
		return -ENODEV;
	}

	ret = of_property_read_u32_array(ssids_node, SMMU_SSID_RANGE_NAME,
					 ssid_range, 2);
	if (ret) {
		dev_info(dev, "[%s] fail to read ssid-range\n", __func__);
		goto out_put;
	}

	if (ssid_range[0] == 0 ||
	    ssid_range[0] > ssid_range[1] ||
	    ssid_range[1] > ssid_max) {
		dev_info(dev, "[%s] invalid ssid-range:%u-%u\n",
			 __func__, ssid_range[0], ssid_range[1]);
		ret = -EINVAL;
	}

out_put:
	of_node_put(ssids_node);
	return ret;
}

int smmu_ssid_is_valid(struct arm_smmu_master *master, u32 ssid)
{
	u32 ssid_max = (1 << master->ssid_bits) - 1;
	u32 ssid_range[2] = {};
	int ret = 0;

	if (ssid == SMMU_NO_SSID || ssid > ssid_max)
		return -EINVAL;

	ret = of_find_smmu_ssid_range(master, ssid_range);
	if (ret)
		return ret;

	if (ssid < ssid_range[0] || ssid > ssid_range[1])
		return -EINVAL;

	return 0;
}

bool smmu_ssid_supported(struct arm_smmu_master *master, u32 ssid)
{
	if (!(master->smmu->features & ARM_SMMU_FEAT_SSID))
		return false;

	if (master->ssid_bits == 0)
		return false;

	return smmu_ssid_is_valid(master, ssid) == 0;
}

void iommu_domain_check(struct iommu_domain *def_domain, struct iommu_domain *domain)
{
	struct iommu_domain_geometry *def_geometry = &def_domain->geometry;
	struct iommu_domain_geometry *geometry = &domain->geometry;
	bool diff;

	diff = domain->type != def_domain->type ||
	       domain->ops != def_domain->ops ||
	       domain->owner != def_domain->owner ||
	       domain->pgsize_bitmap != def_domain->pgsize_bitmap ||
	       geometry->aperture_start != def_geometry->aperture_start ||
	       geometry->aperture_end != def_geometry->aperture_end ||
	       geometry->force_aperture != def_geometry->force_aperture;
	if(diff) {
		pr_info("[%s] def_dom[type:%u pgsize_bitmap:0x%lx geometry[0x%llx 0x%llx %d]]\n",
			__func__,
			def_domain->type,
			def_domain->pgsize_bitmap,
			def_geometry->aperture_start,
			def_geometry->aperture_end,
			def_geometry->force_aperture);

		pr_info("[%s] new_dom[type:%u pgsize_bitmap:0x%lx geometry[0x%llx 0x%llx %d]]\n",
			__func__,
			domain->type,
			domain->pgsize_bitmap,
			geometry->aperture_start,
			geometry->aperture_end,
			geometry->force_aperture);
	}
}

int ssid_domain_finalise(struct arm_smmu_master *master,
			 struct arm_smmu_ssid_domain *ssid_domain)
{
	struct iommu_domain *def_domain = &ssid_domain->default_domain->domain;
	struct iommu_domain *domain = &ssid_domain->domain->domain;
	struct device *dev = master->dev;
	int ret = 0;

	iommu_domain_check(def_domain, domain);

	/* init iova domain and update iova reserve region */
	ret = iommu_dma_init_domain(domain, dev, ssid_domain->ssid,
				    def_domain->iova_cookie->iovad.granule,
				    def_domain->iova_cookie->iovad.start_pfn);
	if (ret)
		goto out_err;

	dev_dbg(dev, "[%s] cookie[type:%d granule:0x%lx start_pfn:0x%lx]]\n",
		__func__, domain->iova_cookie->type,
		domain->iova_cookie->iovad.granule,
		domain->iova_cookie->iovad.start_pfn);

out_err:
	return ret;
}

int iommu_attach_device_ssid(struct iommu_domain *domain,
			     struct device *dev, u32 ssid)
{
	int ret;

	mutex_lock(&dev->iommu_group->mutex);
	ret = arm_smmu_set_dev_ssid(domain, dev, ssid);
	mutex_unlock(&dev->iommu_group->mutex);

	return ret;
}

struct arm_smmu_ssid_domain *
smmu_ssid_domain_alloc(struct arm_smmu_master *master, u32 ssid)
{
	struct arm_smmu_ssid_domain *ssid_domain;
	struct iommu_domain *domain = NULL;
	struct device *dev = master->dev;
	unsigned int def_domain_type;
	int ret = 0;

	ssid_domain = kzalloc(sizeof(*ssid_domain), GFP_KERNEL);
	if (!ssid_domain) {
		ret = -ENOMEM;
		goto out_free_ssid_domain;
	}

	def_domain_type = master->domain->domain.type;
	domain = iommu_domain_alloc_paging(dev, def_domain_type);
	if (!domain) {
		ret = -ENOMEM;
		goto out_free_ssid_domain;
	}
	if (IS_ERR(domain)) {
		ret = PTR_ERR(domain);
		goto out_free_ssid_domain;
	}

	ssid_domain->domain = to_smmu_domain(domain);
	ssid_domain->default_domain = master->domain;
	ssid_domain->ssid = ssid;
	ssid_domain->asid = ssid_domain->domain->cd.asid;

	ret = ssid_domain_finalise(master, ssid_domain);
	if (ret) {
		dev_info(dev, "[%s] ssid_domain_finalise, ret:%d\n", __func__, ret);
		goto out_free_ssid_domain;
	}

	ret = iommu_attach_device_ssid(domain, dev, ssid_domain->ssid);
	if (ret) {
		dev_info(dev, "[%s] attach_device_ssid, ret:%d\n", __func__, ret);
		goto out_free_ssid_domain;
	}

	return ssid_domain;

out_free_ssid_domain:
	if (domain && !IS_ERR(domain))
		iommu_domain_free(domain);
	if (ssid_domain)
		kfree(ssid_domain);
	if (ret)
		dev_info(dev, "[%s] ssid:%u, ret:%d\n", __func__, ssid, ret);
	return NULL;
}

int mtk_enable_smmu_ssid(struct device *dev, u32 ssid)
{
	struct arm_smmu_master *master = dev_iommu_priv_get(dev);
	struct arm_smmu_ssid_domain *ssid_domain;
	struct arm_smmu_domain *smmu_domain;
	struct arm_smmu_device *smmu;
	int nr_masters;
	int ret = 0;
	u32 sid;

	if (!master || !master->smmu || !master->domain || !master->streams)
		return -EINVAL;

	if (!smmu_ssid_supported(master, ssid)) {
		dev_info(dev, "[%s] ssid:%u is not supported\n", __func__, ssid);
		return -ENODEV;
	}

	sid = master->streams[0].id;
	smmu = master->smmu;
	smmu_domain = master->domain;
	mutex_lock(&smmu_domain->ssid_mutex);
	ssid_domain = smmu_ssid_domain_find(smmu_domain, ssid);
	if (ssid_domain) {
		nr_masters = atomic_inc_return(&ssid_domain->nr_ssid_masters);
		dev_dbg(dev, "[%s] find exist domain:%p, sid:%u, ssid:%u, nr:%d\n",
			__func__, ssid_domain, sid, ssid, nr_masters);
		mutex_unlock(&smmu_domain->ssid_mutex);
		return 0;
	}

	ret = smmu_rpm_get(smmu);
	if (ret) {
		dev_info(smmu->dev, "[%s] power_status:%d, dev:%s\n",
			 __func__, ret, dev_name(dev));
		mutex_unlock(&smmu_domain->ssid_mutex);
		return ret;
	}

	ssid_domain = smmu_ssid_domain_alloc(master, ssid);
	if (!ssid_domain) {
		ret = -ENOMEM;
		goto out;
	}

	ret = smmu_ssid_domain_add(smmu_domain, ssid_domain);
	if (!ret) {
		nr_masters = atomic_inc_return(&ssid_domain->nr_ssid_masters);

		/* only fist and last ssid print debug log */
		if (master->cd_table.used_ssids == 1 ||
		    master->cd_table.used_ssids == ((1U << master->ssid_bits) - 1)) {
			dev_info(dev,
				 "[%s] domain:%p, sid:%u, ssid:%u, asid:%u, nr:%d, ssids[%u,%u]\n",
				 __func__, ssid_domain, sid, ssid, ssid_domain->asid,
				 nr_masters, master->ssid_bits, master->cd_table.used_ssids);
		}
	}

out:
	smmu_rpm_put(smmu);
	mutex_unlock(&smmu_domain->ssid_mutex);
	if (ret)
		dev_info(dev, "[%s] ssid:%u, ret:%d\n", __func__, ssid, ret);
	return ret;
}

int mtk_release_smmu_ssids(struct device *dev)
{
	struct arm_smmu_master *master = dev_iommu_priv_get(dev);
	struct arm_smmu_ssid_domain *ssid_domain, *tmp;
	struct arm_smmu_domain *smmu_domain;
	struct arm_smmu_device *smmu;
	struct rb_root *root;
	int ret = 0;

	if (!master || !master->smmu || !master->domain)
		return -EINVAL;

	if (!(master->smmu->features & ARM_SMMU_FEAT_SSID))
		return 0;

	smmu = master->smmu;
	smmu_domain = master->domain;
	mutex_lock(&smmu_domain->ssid_mutex);
	ret = smmu_rpm_get(smmu);
	if (ret) {
		dev_info(smmu->dev, "[%s] power_status:%d, dev:%s\n",
			 __func__, ret, dev_name(dev));
		mutex_unlock(&smmu_domain->ssid_mutex);
		return ret;
	}

	root = &smmu_domain->ssid_domains;
	rbtree_postorder_for_each_entry_safe(ssid_domain, tmp, root, node) {
		int nr_masters = atomic_read(&ssid_domain->nr_ssid_masters);

		dev_info(dev, "[%s] domain:%p, ssid:%u, asid:%u, nr:%d\n",
			 __func__, ssid_domain, ssid_domain->ssid,
			 ssid_domain->asid, nr_masters);
		WARN_ON(nr_masters != 0);

		smmu_ssid_domain_delete(smmu_domain, ssid_domain);
		smmu_ssid_domain_release(master, ssid_domain);
	}

	smmu_rpm_put(smmu);
	mutex_unlock(&smmu_domain->ssid_mutex);
	return ret;
}

int mtk_disable_smmu_ssid(struct device *dev, u32 ssid)
{
	struct arm_smmu_master *master = dev_iommu_priv_get(dev);
	struct arm_smmu_ssid_domain *ssid_domain;
	struct arm_smmu_domain *smmu_domain;
	struct arm_smmu_device *smmu;
	int nr_masters;
	int ret = 0;
	u32 sid;

	if (!master || !master->smmu || !master->domain || !master->streams)
		return -EINVAL;

	if (!smmu_ssid_supported(master, ssid)) {
		dev_info(dev, "[%s] ssid:%u is not supported\n", __func__, ssid);
		return -ENODEV;
	}

	sid = master->streams[0].id;
	smmu = master->smmu;
	smmu_domain = master->domain;
	mutex_lock(&smmu_domain->ssid_mutex);
	ssid_domain = smmu_ssid_domain_find(smmu_domain, ssid);
	if (!ssid_domain) {
		dev_info(dev, "[%s] no ssid domain, sid:%u, ssid:%u\n", __func__, sid, ssid);
		mutex_unlock(&smmu_domain->ssid_mutex);
		return -EINVAL;
	}

	ret = smmu_rpm_get(smmu);
	if (ret) {
		dev_info(smmu->dev, "[%s] power_status:%d, dev:%s\n", __func__,
			 ret, dev_name(dev));
		mutex_unlock(&smmu_domain->ssid_mutex);
		return ret;
	}

	nr_masters = atomic_dec_return(&ssid_domain->nr_ssid_masters);
	dev_info(dev, "[%s] domain:%p, sid:%u, ssid:%u, asid:%u, nr:%d, ssids[%u, %u]\n",
		 __func__, ssid_domain, sid, ssid_domain->ssid, ssid_domain->asid,
		 nr_masters, master->ssid_bits, master->cd_table.used_ssids);

	if (nr_masters == 0) {
		smmu_ssid_domain_delete(smmu_domain, ssid_domain);
		smmu_ssid_domain_release(master, ssid_domain);
	}

	smmu_rpm_put(smmu);
	mutex_unlock(&smmu_domain->ssid_mutex);
	return ret;
}

dma_addr_t mtk_smmu_map_pages_ssid(struct device *dev, struct page *page,
				   unsigned long offset, size_t size,
				   enum dma_data_direction dir,
				   unsigned long attrs, u32 ssid)
{
	phys_addr_t phys = page_to_phys(page) + offset;
	bool coherent = dev_is_dma_coherent(dev);
	int prot = dma_info_to_prot(dir, coherent, attrs);
	dma_addr_t iova, dma_mask = dma_get_mask(dev);
	struct arm_smmu_ssid_domain *ssid_domain;
	struct iommu_domain *domain;

	ssid_domain = smmu_ssid_domain_for_dev_ssid(dev, ssid);
	domain = to_iommu_domain(ssid_domain);
	if (!domain) {
		dev_info(dev, "[%s] no ssid domain, ssid:%u\n", __func__, ssid);
		return DMA_MAPPING_ERROR;
	}

	if (!coherent && !(attrs & DMA_ATTR_SKIP_CPU_SYNC))
		mtk_smmu_dma_sync_for_device(dev, phys, size, dir);

	iova = iommu_dma_map(ssid_domain, phys, size, prot, dma_mask, dev);
	if (iova == DMA_MAPPING_ERROR)
		dev_info(dev, "[%s] map iova fail, ssid:%u\n", __func__, ssid);

	return iova;
}

void mtk_smmu_unmap_pages_ssid(struct device *dev, dma_addr_t dma_addr,
			       size_t size, enum dma_data_direction dir,
			       unsigned long attrs, u32 ssid)
{
	struct arm_smmu_ssid_domain *ssid_domain;
	struct iommu_domain *domain;
	phys_addr_t phys;

	ssid_domain = smmu_ssid_domain_for_dev_ssid(dev, ssid);
	domain = to_iommu_domain(ssid_domain);
	if (!domain) {
		dev_info(dev, "[%s] no ssid domain, ssid:%u\n", __func__, ssid);
		return;
	}

	phys = iommu_iova_to_phys(domain, dma_addr);
	if (WARN_ON(!phys))
		return;

	if (!(attrs & DMA_ATTR_SKIP_CPU_SYNC) && !dev_is_dma_coherent(dev))
		mtk_smmu_dma_sync_for_cpu(dev, phys, size, dir);

	iommu_dma_unmap(ssid_domain, dma_addr, size, dev);
}

int mtk_smmu_map_sg_ssid(struct device *dev, struct scatterlist *sg, int nents,
			 enum dma_data_direction dir, unsigned long attrs,
			 u32 ssid)
{
	struct arm_smmu_ssid_domain *ssid_domain;
	struct iommu_domain *domain;
	struct iova_domain *iovad;
	struct scatterlist *s, *prev = NULL;
	int prot = dma_info_to_prot(dir, dev_is_dma_coherent(dev), attrs);
	dma_addr_t iova;
	size_t iova_len = 0;
	unsigned long mask = dma_get_seg_boundary(dev);
	ssize_t ret;
	int i;

	ssid_domain = smmu_ssid_domain_for_dev_ssid(dev, ssid);
	domain = to_iommu_domain(ssid_domain);
	if (!domain) {
		dev_info(dev, "[%s] no ssid domain, ssid:%u\n", __func__, ssid);
		return -EINVAL;
	}

	iovad = &domain->iova_cookie->iovad;

	if (!(attrs & DMA_ATTR_SKIP_CPU_SYNC))
		dma_sync_sg_for_device(dev, sg, nents, dir);

	for_each_sg(sg, s, nents, i) {
		size_t s_iova_off = iova_offset(iovad, s->offset);
		size_t s_length = s->length;
		size_t pad_len = (mask - iova_len + 1) & mask;

		sg_dma_address(s) = s_iova_off;
		sg_dma_len(s) = s_length;
		s->offset -= s_iova_off;
		s_length = iova_align(iovad, s_length + s_iova_off);
		s->length = s_length;

		if (pad_len && pad_len < s_length - 1) {
			prev->length += pad_len;
			iova_len += pad_len;
		}

		iova_len += s_length;
		prev = s;
	}

	if (!iova_len)
		return __finalise_sg(dev, sg, nents, 0);

	iova = iommu_dma_alloc_iova(ssid_domain, iova_len, dma_get_mask(dev), dev);
	if (!iova) {
		dev_info(dev, "[%s] alloc iova fail, ssid:%u\n", __func__, ssid);
		ret = -ENOMEM;
		goto out_restore_sg;
	}

	ret = iommu_map_sg(domain, iova, sg, nents, prot, GFP_ATOMIC);
	if (ret < 0 || ret < iova_len) {
		dev_info(dev, "[%s] map iova fail, iova:0x%llx, ret:%ld\n",
			 __func__, iova, ret);
		goto out_free_iova;
	}

	dev_dbg(dev, "[%s] iova:0x%llx, size:0x%zx\n", __func__, iova, iova_len);

	return __finalise_sg(dev, sg, nents, iova);

out_free_iova:
	iommu_dma_free_iova(ssid_domain, iova, iova_len, NULL, dev);
out_restore_sg:
	__invalidate_sg(sg, nents);

	if (ret != -ENOMEM && ret != -EREMOTEIO)
		return -EINVAL;
	return ret;
}

void mtk_smmu_unmap_sg_ssid(struct device *dev, struct scatterlist *sg,
			    int nents, enum dma_data_direction dir,
			    unsigned long attrs, u32 ssid)
{
	struct arm_smmu_ssid_domain *ssid_domain;
	struct iommu_domain *domain;
	dma_addr_t end = 0, start = 0;
	struct scatterlist *tmp;
	int i;

	ssid_domain = smmu_ssid_domain_for_dev_ssid(dev, ssid);
	domain = to_iommu_domain(ssid_domain);
	if (!domain) {
		/* dma-heap deferred unmap when release dma-buf */
		dev_dbg(dev, "[%s] no ssid domain, ssid:%u\n", __func__, ssid);
		return;
	}

	if (!(attrs & DMA_ATTR_SKIP_CPU_SYNC))
		dma_sync_sg_for_cpu(dev, sg, nents, dir);

	/*
	 * The scatterlist segments are mapped into a single
	 * contiguous IOVA allocation, the start and end points
	 * just have to be determined.
	 */
	for_each_sg(sg, tmp, nents, i) {
		if (sg_dma_is_bus_address(tmp)) {
			sg_dma_unmark_bus_address(tmp);
			continue;
		}

		if (sg_dma_len(tmp) == 0)
			break;

		start = sg_dma_address(tmp);
		break;
	}

	nents -= i;
	for_each_sg(tmp, tmp, nents, i) {
		if (sg_dma_is_bus_address(tmp)) {
			sg_dma_unmark_bus_address(tmp);
			continue;
		}

		if (sg_dma_len(tmp) == 0)
			break;

		end = sg_dma_address(tmp) + sg_dma_len(tmp);
	}

	if (end == 0) {
		dev_info(dev, "[%s] iova end incorrect, start:0x%llx, ssid:%u\n",
			 __func__, start, ssid);
		return;
	}

	iommu_dma_unmap(ssid_domain, start, end - start, dev);
}

phys_addr_t mtk_smmu_iova_to_phys_ssid(struct device *dev, dma_addr_t iova,
				       u32 ssid)
{
	struct io_pgtable_ops *ops;

	ops = get_smmu_pgtable_ops(dev, ssid);
	if (!ops)
		return 0;

	return ops->iova_to_phys(ops, iova);
}

u64 mtk_smmu_iova_to_iopte_ssid(struct device *dev, dma_addr_t iova, u32 ssid)
{
	struct io_pgtable_ops *ops;

	ops = get_smmu_pgtable_ops(dev, ssid);
	if (!ops)
		return 0;

	return mtk_smmu_iova_to_iopte(ops, iova);
}
