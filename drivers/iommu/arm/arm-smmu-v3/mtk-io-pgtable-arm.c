// SPDX-License-Identifier: GPL-2.0-only
/*
 * CPU-agnostic ARM page table allocator.
 * Host-specific functions. The rest is in io-pgtable-arm-common.c.
 *
 * Copyright (C) 2014 ARM Limited
 * Copyright (c) 2025 MediaTek Inc.
 *
 * Author: Will Deacon <will.deacon@arm.com>
 */

#define pr_fmt(fmt)	"arm-lpae io-pgtable: " fmt

#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/io-pgtable-arm.h>
#include <linux/kernel.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/dma-mapping.h>

#include "iommu-pages.h"

#include <asm/barrier.h>

#include "mtk-io-pgtable-arm.h"
#include "mtk-smmu-v3.h"

static bool selftest_running;

int arm_lpae_map_exists(void)
{
	WARN_ON(!selftest_running);
	return -EEXIST;
}

void arm_lpae_unmap_empty(void)
{
	WARN_ON(!selftest_running);
}

static dma_addr_t __arm_lpae_dma_addr(void *pages)
{
	return (dma_addr_t)virt_to_phys(pages);
}

static void *mtk_iommu_alloc_pages(size_t size, gfp_t gfp,
				   struct io_pgtable_cfg *cfg,
				   void *cookie)
{
	struct device *dev = cfg->iommu_dev;
	int order = get_order(size);
	int retry_count = 0;
	int retry_max = 8;
	void *pages;

	pages = iommu_alloc_pages_node(dev_to_node(dev), gfp, order);
	if (pages)
		return pages;

	if ((gfp & GFP_ATOMIC) == 0)
		return NULL;

	/* Retry if atomic alloc memory fail */
	while (!pages && retry_count < retry_max) {
		bool atomic_ctx = in_atomic() || irqs_disabled() || in_interrupt();
		gfp_t gfp_flags = gfp;

		if (!atomic_ctx) {
			/* If not in atomic ctx, wait memory reclaim */
			gfp_flags = (gfp & ~GFP_ATOMIC) | GFP_KERNEL;
		}

		pages = iommu_alloc_pages_node(dev_to_node(dev), gfp_flags, order);
		dev_info_ratelimited(dev,
				     "[%s] retry:%d size:0x%zx gfp:0x%x->0x%x ret:%d\n",
				     __func__, retry_count + 1, size, gfp,
				     gfp_flags, (pages != NULL));
		if (!pages) {
			retry_count++;
			if (atomic_ctx) {
				/* most wait 4ms at atomic */
				udelay(500);
			} else {
				usleep_range(8000, 10*1000);
			}
		}
	}

	if (!pages) {
		dev_info(dev, "[%s] retry:%d size:0x%zx gfp:0x%x failed\n",
			 __func__, retry_count, size, gfp);
	}

	return pages;
}

void *__arm_lpae_alloc_pages(size_t size, gfp_t gfp,
			     struct io_pgtable_cfg *cfg,
			     void *cookie)
{
	struct device *dev = cfg->iommu_dev;
	int order = get_order(size);
	dma_addr_t dma;
	void *pages;

	if (gfp & __GFP_HIGHMEM) {
		gfp &= ~__GFP_HIGHMEM;
		WARN_ON_ONCE(1);
	}
	if (cfg->alloc)
		pages = cfg->alloc(cookie, size, gfp);
	else
		pages = iommu_alloc_pages_node(dev_to_node(dev), gfp, order);

	if (!pages) {
		/* Retry if alloc pages fail */
		pages = mtk_iommu_alloc_pages(size, gfp, cfg, cookie);
		if (!pages)
			return NULL;
	}

	if (!cfg->coherent_walk) {
		dma = dma_map_single(dev, pages, size, DMA_TO_DEVICE);
		if (dma_mapping_error(dev, dma))
			goto out_free;
		/*
		 * We depend on the IOMMU being able to work with any physical
		 * address directly, so if the DMA layer suggests otherwise by
		 * translating or truncating them, that bodes very badly...
		 */
		if (dma != virt_to_phys(pages))
			goto out_unmap;
	}

	return pages;

out_unmap:
	dev_err(dev, "Cannot accommodate DMA translation for IOMMU page tables\n");
	dma_unmap_single(dev, dma, size, DMA_TO_DEVICE);

out_free:
	if (cfg->free)
		cfg->free(cookie, pages, size);
	else
		iommu_free_pages(pages, order);

	return NULL;
}

void __arm_lpae_free_pages(void *pages, size_t size,
			   struct io_pgtable_cfg *cfg,
			   void *cookie)
{
	if (!cfg->coherent_walk)
		dma_unmap_single(cfg->iommu_dev, __arm_lpae_dma_addr(pages),
				 size, DMA_TO_DEVICE);

	if (cfg->free)
		cfg->free(cookie, pages, size);
	else
		iommu_free_pages(pages, get_order(size));
}

void __arm_lpae_sync_pte(arm_lpae_iopte *ptep, int num_entries,
			 struct io_pgtable_cfg *cfg)
{
	dma_sync_single_for_device(cfg->iommu_dev, __arm_lpae_dma_addr(ptep),
				   sizeof(*ptep) * num_entries, DMA_TO_DEVICE);
}

static void arm_lpae_free_pgtable(struct io_pgtable *iop)
{
	struct arm_lpae_io_pgtable *data = io_pgtable_to_data(iop);
	struct arm_lpae_io_pgtable_ext *data_ext = io_pgtable_to_data_ext(data);

	__arm_lpae_free_pgtable(data, data->start_level, data->pgd);
	kfree(data_ext);
}

static int visit_dirty(struct io_pgtable_walk_data *walk_data, int lvl,
		       arm_lpae_iopte *ptep, size_t size)
{
	struct io_pgtable_walk_common *walker = walk_data->data;
	struct iommu_dirty_bitmap *dirty = walker->data;

	if (!iopte_leaf(*ptep, lvl, walk_data->iop->fmt))
		return 0;

	if (iopte_writeable_dirty(*ptep)) {
		iommu_dirty_bitmap_record(dirty, walk_data->addr, size);
		if (!(walk_data->flags & IOMMU_DIRTY_NO_CLEAR))
			iopte_set_writeable_clean(ptep);
	}

	return 0;
}

static int arm_lpae_read_and_clear_dirty(struct io_pgtable_ops *ops,
					 unsigned long iova, size_t size,
					 unsigned long flags,
					 struct iommu_dirty_bitmap *dirty)
{
	struct arm_lpae_io_pgtable *data = io_pgtable_ops_to_data(ops);
	struct io_pgtable_cfg *cfg = &data->iop.cfg;
	struct io_pgtable_walk_common walker = {
		.data = dirty,
	};
	struct io_pgtable_walk_data walk_data = {
		.iop = &data->iop,
		.data = &walker,
		.visit = visit_dirty,
		.flags = flags,
		.addr = iova,
		.end = iova + size,
	};
	arm_lpae_iopte *ptep = data->pgd;
	int lvl = data->start_level;

	if (WARN_ON(!size))
		return -EINVAL;
	if (WARN_ON((iova + size - 1) & ~(BIT(cfg->ias) - 1)))
		return -EINVAL;
	if (data->iop.fmt != ARM_64_LPAE_S1)
		return -EINVAL;

	return __arm_lpae_iopte_walk(data, &walk_data, ptep, lvl);
}

static struct io_pgtable *
arm_64_lpae_alloc_pgtable_s1(struct io_pgtable_cfg *cfg, void *cookie)
{
	struct arm_lpae_io_pgtable_ext *data_ext;
	struct arm_lpae_io_pgtable *data;

	data_ext = kzalloc(sizeof(*data_ext), GFP_KERNEL);
	if (!data_ext)
		return NULL;

	data = &data_ext->data;

	if (arm_lpae_init_pgtable_s1(cfg, data))
		goto out_free_data;

	data->iop.ops.read_and_clear_dirty = arm_lpae_read_and_clear_dirty;
	/* Looking good; allocate a pgd */
	data->pgd = __arm_lpae_alloc_pages(ARM_LPAE_PGD_SIZE(data),
					   GFP_KERNEL, cfg, cookie);
	if (!data->pgd)
		goto out_free_data;

	/* Ensure the empty pgd is visible before any actual TTBR write */
	wmb();

	/* TTBR */
	cfg->arm_lpae_s1_cfg.ttbr = virt_to_phys(data->pgd);

	/* Initialize the spinlock */
	spin_lock_init(&data_ext->split_lock);

	return &data->iop;

out_free_data:
	kfree(data_ext);
	return NULL;
}

static int arm_64_lpae_configure_s1(struct io_pgtable_cfg *cfg)
{
	struct arm_lpae_io_pgtable data = {};

	return arm_lpae_init_pgtable_s1(cfg, &data);
}

struct io_pgtable_init_fns mtk_io_pgtable_arm_64_lpae_s1_contig_fns = {
	.caps	= IO_PGTABLE_CAP_CUSTOM_ALLOCATOR,
	.alloc	= arm_64_lpae_alloc_pgtable_s1,
	.free	= arm_lpae_free_pgtable,
	.configure	= arm_64_lpae_configure_s1,
};
