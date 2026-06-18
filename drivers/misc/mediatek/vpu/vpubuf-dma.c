// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
/**
 * @file vpubuf-dma.c
 * Handle about VPU memory management.
 */
#include <linux/of.h>
#include <linux/highmem.h>

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/firmware.h>
#include <linux/iommu.h>

#include "vpubuf-core.h"
#include "vpubuf-dma-contig.h"
#include "vpubuf-dma.h"
#include "vpu_cmn.h"
#include "vpu_hw.h"

#if (defined(CONFIG_MTK_M4U) || defined(CONFIG_MTK_PSEUDO_M4U))
#include "pseudo_m4u.h"
#endif
static int vpu_map_kva_to_sgt(
	const char *buf, size_t len, struct sg_table *sgt);

static dma_addr_t vpu_map_sg_to_iova(
	struct vpu_device *vpu_device, struct scatterlist *sg,
	unsigned int nents, size_t len, dma_addr_t given_iova);

static void apu_bmap_free(struct apu_bmap *ab, uint32_t addr, unsigned int size);
static struct vpu_kernel_buf *vbuf_dma_kmap(struct vpu_device *vpu_device,
					uint32_t usage, uint64_t phy_addr,
					uint64_t kva_addr,
					uint32_t iova_addr,
					uint32_t size)
{
	struct vpu_kernel_buf *vkbuf = NULL;
	struct sg_table *sg = NULL;
	int ret = 0;

	LOG_INF("%s, kmap phy_addr=0x%llx\n", __func__, phy_addr);
	//int pg_size;
	vkbuf = kzalloc(sizeof(*vkbuf), GFP_KERNEL);
	if (!vkbuf)
		return NULL;
	vkbuf->iova_addr = 0;
	LOG_INF("%s usage = %d, size = %x\n", __func__, usage, size);
	LOG_INF("%s, kva_addr=0x%llx\n", __func__, kva_addr);
	size = round_up(size, PAGE_SIZE);
	LOG_INF("%s, iova_addr=0x%x, size = %x\n", __func__, iova_addr, size);

	switch (usage) {
	case VKBUF_MAP_FPHY_FIOVA:
	{
		sg = &(vkbuf->sg);
		ret = vpu_map_kva_to_sgt((void *)(kva_addr), size, sg);
		if (ret)
			break;
		vkbuf->iova_addr = vpu_map_sg_to_iova(vpu_device, sg->sgl,
			sg->nents, size, iova_addr);
		vkbuf->handle = 0;
		vkbuf->usage = usage;
		vkbuf->kva = (void *)(uintptr_t)kva_addr;
		/* vkbuf->sg = sg; */
		vkbuf->phy_addr = phy_addr;
		vkbuf->iova_addr = (uint32_t)iova_addr;
		vkbuf->size = size;
		break;
	}

	case VKBUF_MAP_FPHY_DIOVA:
	{
		if (iova_addr == 0 || iova_addr >= VPU_MVA_RESERVED_END)
			iova_addr = VPU_MVA_RESERVED_END;
		sg = &(vkbuf->sg);
		ret = vpu_map_kva_to_sgt((void *)kva_addr, size, sg);
		if (ret)
			break;
		vkbuf->iova_addr = vpu_map_sg_to_iova(vpu_device, sg->sgl, sg->nents,
			size, iova_addr);
		vkbuf->handle = 0;
		vkbuf->kva = (void *)(uintptr_t)kva_addr;
		vkbuf->usage = usage;
		vkbuf->phy_addr = phy_addr;
		vkbuf->size = size;
		break;
	}
	case VKBUF_MAP_DPHY_FIOVA:
	{
		//void *kva =  kvmalloc(size, GFP_KERNEL);
		void *kva =  vmalloc(size);

		vkbuf->kva = kva;
		if (iova_addr == 0 || iova_addr >= VPU_MVA_RESERVED_END)
			iova_addr = VPU_MVA_RESERVED_END;
		if (vkbuf->kva == NULL) {
			LOG_ERR("fail to VKBUF_MAP_DPHY_FIOVA\n");
			kfree(vkbuf);
			return NULL;
		}
		sg = &(vkbuf->sg);
		LOG_INF("%s, kva_addr=0x%lx\n", __func__, (unsigned long)kva_addr);
		ret = vpu_map_kva_to_sgt(kva, size, sg);
		if (ret)
			break;
		vkbuf->iova_addr = vpu_map_sg_to_iova(vpu_device, sg->sgl, sg->nents,
			size, iova_addr);
		vkbuf->handle = 0;
		vkbuf->kva = kva;
		vkbuf->usage = usage;
		vkbuf->phy_addr = 0;
		vkbuf->size = size;
		break;
	}
	case VKBUF_MAP_DPHY_DIOVA:
	{
		void *kva =  vmalloc(size);

		vkbuf->kva = kva;
		if (iova_addr == 0 || iova_addr >= VPU_MVA_RESERVED_END)
			iova_addr = VPU_MVA_RESERVED_END;

		if (vkbuf->kva == NULL) {
			LOG_ERR("fail to VKBUF_MAP_DPHY_DIOVA\n");
			kfree(vkbuf);
			return NULL;
		}
		sg = &(vkbuf->sg);
		ret = vpu_map_kva_to_sgt(kva, size, sg);
		if (ret)
			break;
		vkbuf->iova_addr = vpu_map_sg_to_iova(vpu_device, sg->sgl, sg->nents,
			size, iova_addr);
		vkbuf->kva = kva;
		vkbuf->handle = 0;
		vkbuf->usage = usage;
		vkbuf->phy_addr = 0;
		vkbuf->size = size;
		break;
	}
	default:
	{
		break;
	}
	}
	LOG_INF("%s end, kva_addr=0x%llx\n", __func__, kva_addr);
	LOG_INF("%s , iova=0x%x,size = %x\n", __func__, vkbuf->iova_addr, size);
	return vkbuf;
}

static void vbuf_dma_kunmap(struct vpu_device *vpu_device,
				struct vpu_kernel_buf *vkbuf)
{
	struct iommu_domain *domain;
	size_t ret = 0;
	size_t size = vkbuf->size;

	domain = iommu_get_domain_for_dev(vpu_device->dev[0]);
	if (vkbuf->sg.sgl) {
		if (domain != NULL)
			ret = iommu_unmap(domain, vkbuf->iova_addr, size);

		if (ret != size)
			LOG_ERR("%s: iommu_unmap iova: %llx, returned: %zx, expected: %zx\n",
				__func__, (u64)vkbuf->iova_addr, ret, size);
		sg_free_table(&vkbuf->sg);
	}
	apu_bmap_free(&vpu_device->ab, vkbuf->iova_addr, size);
	vfree(vkbuf->kva);
	kfree(vkbuf);
}

static void vbuf_dma_init(struct vpu_device *vpu_device)
{
	vbuf_std_init(vpu_device);
}

static void vbuf_dma_deinit(struct vpu_device *vpu_device)
{
	vbuf_std_deinit(vpu_device);
}

static uint64_t vbuf_dma_import_handle(struct vpu_device *vpu_device, int fd)
{
	return 0;
}

static void vbuf_dma_free_handle(struct vpu_device *vpu_device, uint64_t id)
{
}

int apu_bmap_init(struct apu_bmap *ab, const char *name)
{
	if (!ab || ab->start > ab->end)
		return -EINVAL;

	memset(ab->name, 0, APU_BMAP_NAME_LEN);
	strscpy(ab->name, name, APU_BMAP_NAME_LEN - 1);

	/* size must be multiple of allocation unit */
	ab->size = ab->end - ab->start;
	LOG_INF("%s: %s: start: 0x%x, end: 0x%x, size: 0x%x, au: 0x%x\n",
		__func__, ab->name, ab->start, ab->end, ab->size, ab->au);
	if (!is_au_align(ab, ab->size)) {
		LOG_INF("%s: %s: size 0x%x is un-aligned to AU 0x%x\n",
			__func__, ab->name, ab->size, ab->au);
		return -EINVAL;
	}

	ab->nbits = ab->size / ab->au;
	ab->b = bitmap_zalloc(ab->nbits, GFP_KERNEL);
	spin_lock_init(&ab->lock);

	return 0;
}


/**
 * Release bitmap
 * @param[in] ab The apu bitmap object to be released
 */
void apu_bmap_exit(struct apu_bmap *ab)
{
	if (!ab || !ab->b)
		return;

	bitmap_free(ab->b);
	ab->b = NULL;
}

/**
 * Allocate addresses from bitmap
 * @param[in] ab The apu bitmap object to be allocated from.
 * @param[in] size Desired allocation size in bytes.
 * @param[in] given_addr Search begin from the given address,
 *			  0 = Searches from ab->start
 * @return 0: Allocation failed.<br>
 *	  Others: Allocated address.<br>
 * @remark: Searches free addresses from begin (ab->start).<br>
 *	  You have to check the returned address for static iova mapping
 */
uint32_t apu_bmap_alloc(struct apu_bmap *ab, unsigned int size,
	uint32_t given_addr)
{
	uint32_t addr = 0;
	unsigned int nr;
	unsigned long offset;
	unsigned long start = 0;
	unsigned long flags;

	if (!ab)
		return 0;

	if (given_addr) {
		start = given_addr - ab->start;
		if (!is_au_align(ab, start)) {
			LOG_INF("%s: %s: size: 0x%x, given addr: 0x%x, start 0x%lx is to AU 0x%x\n",
				__func__, ab->name, size, addr, start, ab->au);
			return 0;
		}
		start = start / ab->au;
	}

	spin_lock_irqsave(&ab->lock, flags);
	nr = round_up(size, ab->au) / ab->au;

	offset = bitmap_find_next_zero_area(ab->b, ab->nbits, start,
		nr, ab->align_mask);

	if (offset >= ab->nbits) {
		LOG_INF("%s: %s : size: 0x%x, given addr: 0x%x, offset: %ld, nbits: %ld\n",
			__func__, ab->name, size, addr, offset, ab->nbits);
		goto out;
	}

	addr = offset * ab->au + ab->start;
	__bitmap_set(ab->b, offset, nr);

out:
	spin_unlock_irqrestore(&ab->lock, flags);

	if (addr)
		LOG_INF("%s: %s: size: 0x%x, given_addr: 0x%x, allocated addr: 0x%x\n",
		__func__, ab->name, size, given_addr, addr);

	return addr;
}

/**
 * Free occupied addresses from bitmap
 * @param[in] ab The apu bitmap object.
 * @param[in] addr Allocated start address returned by apu_bmap_alloc().
 * @param[in] size Allocated size.
 */
void apu_bmap_free(struct apu_bmap *ab, uint32_t addr, unsigned int size)
{
	unsigned int nr;
	unsigned long offset;
	unsigned long flags;

	if (!ab || addr < ab->start || (addr + size) > ab->end)
		return;

	nr = round_up(size, ab->au) / ab->au;
	offset = addr - ab->start;

	LOG_INF("%s: %s: addr: 0x%x, size: 0x%x, nr_bits: %d\n",
		__func__, ab->name, addr, size, nr);

	if (!is_au_align(ab, offset)) {
		LOG_INF("%s: %s: addr 0x%x, offset %ld is un-aligned to AU 0x%x\n",
			__func__, ab->name, addr, offset, ab->au);
		return;
	}

	offset = offset / ab->au;
	if (offset >= ab->nbits) {
		LOG_INF("%s: %s: addr 0x%x, offset-bit %ld is out of limit %ld\n",
			__func__, ab->name, addr, offset, ab->nbits);
		return;
	}

	spin_lock_irqsave(&ab->lock, flags);
	__bitmap_clear(ab->b, offset, nr);
	spin_unlock_irqrestore(&ab->lock, flags);
}

static dma_addr_t vpu_map_sg_to_iova(
	struct vpu_device *vpu_device, struct scatterlist *sg,
	unsigned int nents, size_t len, dma_addr_t given_iova)
{
	struct iommu_domain *domain;
	dma_addr_t iova = 0;
	size_t size = 0;
	int prot = IOMMU_READ | IOMMU_WRITE;
	u64 bank = VPU_MVA_BANK;
	u32 iova_end = VPU_MVA_RESERVED_END;
	u32 iova_heap = VPU_MVA_RESERVED_BANK;
	//struct vpu_device *vd = dev_get_drvdata(&pdev->dev);
	if (sg == NULL) {
		LOG_INF("%s: fail because of sg is null\n", __func__);
		goto err;
	}

	domain = iommu_get_domain_for_dev(vpu_device->dev[0]);
	if (domain == NULL) {
		LOG_INF("%s: fail because of domain is null\n", __func__);
		goto err;
	}

	LOG_INF("%s: len: %zx, given_iova: %llx (%s alloc)\n",
		__func__, len, (u64)given_iova,
		(given_iova < iova_end) ? "static" : "dynamic");

	if (given_iova < iova_end) {  /* Static IOVA allocation */
		iova = apu_bmap_alloc(&vpu_device->ab, len, given_iova);
		if (!iova)
			goto err;
		/* Static: must be allocated on the given address */
		if (iova != given_iova) {
			LOG_INF("%s: given iova: %llx, apu_bmap_alloc returned: %llx\n",
				__func__, (u64)given_iova, (u64)iova);
			goto err;
		}
	} else {  /* Dynamic IOVA allocation */
		/* Dynamic: Allocate from heap first */
		iova = apu_bmap_alloc(&vpu_device->ab, len, iova_heap);
		if (!iova) {
			/* Dynamic: Try to allocate again from iova start */
			iova = apu_bmap_alloc(&vpu_device->ab, len, 0);
			if (!iova)
				goto err;
		}
	}

	iova = iova | bank;
	LOG_INF("%s: len: %zx, iova: %llx\n",
		__func__, len, (u64)iova);

	size = iommu_map_sg(domain, iova, sg, nents, prot, GFP_KERNEL);

	if (size == 0) {
		LOG_INF("%s: iommu_map_sg: len: %zx, iova: %llx, failed\n",
			__func__, len, (u64)iova);
		goto err;
	} else if (size != len) {
		LOG_INF("%s: iommu_map_sg: len: %zx, iova: %llx, mismatch with mapped size: %zx\n",
			__func__, len, (u64)iova, size);
		goto err;
	}

	return iova;

err:
	if (iova)
		apu_bmap_free(&vpu_device->ab, len, iova);

	return 0;
}

static int vpu_map_kva_to_sgt(const char *buf, size_t len, struct sg_table *sgt)
{
	struct page **pages = NULL;
	unsigned int nr_pages;
	unsigned int index;
	const char *p;
	int ret;

	LOG_INF("%s: buf: 0x%lx, len: 0x%zx, sgt: 0x%p\n",
		__func__, (unsigned long)buf, len, sgt);

	nr_pages = DIV_ROUND_UP((unsigned long)buf + len, PAGE_SIZE)
		- ((unsigned long)buf / PAGE_SIZE);
	pages = kmalloc_array(nr_pages, sizeof(struct page *), GFP_KERNEL);

	if (!pages)
		return -ENOMEM;

	p = buf - offset_in_page(buf);

	for (index = 0; index < nr_pages; index++) {
		if (is_vmalloc_addr(p))
			pages[index] = vmalloc_to_page(p);
		else
			pages[index] = kmap_to_page((void *)p);
		if (!pages[index]) {
			LOG_INF("%s: map failed\n", __func__);
			ret = -EFAULT;
			goto out;
		}
		p += PAGE_SIZE;
	}

	LOG_INF("%s: nr_pages: %d\n", __func__, nr_pages);

	ret = sg_alloc_table_from_pages(sgt, pages, index,
		offset_in_page(buf), len, GFP_KERNEL);

	if (ret) {
		LOG_INF("%s: sg_alloc_table_from_pages: %d\n",
			__func__, ret);
		goto out;
	}

out:
	kfree(pages);
	return ret;
}



struct vpu_map_ops vpu_dma_mapops = {
	.init_phy_iova = vbuf_dma_init,
	.deinit_phy_iova = vbuf_dma_deinit,
	.kmap_phy_iova = vbuf_dma_kmap,
	.kunmap_phy_iova = vbuf_dma_kunmap,
	.import_handle = vbuf_dma_import_handle,
	.free_handle = vbuf_dma_free_handle,
};
