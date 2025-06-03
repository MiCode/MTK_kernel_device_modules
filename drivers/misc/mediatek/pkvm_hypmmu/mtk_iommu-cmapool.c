// SPDX-License-Identifier: GPL-2.0
/* mtk_iommu-cmapool.c
 *
 * Hypmmu CMA pool for mtk_iommu.
 *
 * Copyright (c) 2025 MediaTek Inc.
 */

#if defined(MTK_IOMMU_CMAPOOL) && (MTK_IOMMU_CMAPOOL)

#define _MTK_IOMMU_CMAPOOL_C
#include "mtk_iommu-cmapool.h"

static int mtk_iommu_cmapool_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *rmem_node = NULL;

	pr_info("%s: start cmapool probe\n", __func__);

	do {
		struct device **cmapool_dev = &mtk_iommu_cmapool_info.dev;
		struct reserved_mem **cmapool_rmem =
			&mtk_iommu_cmapool_info.rmem;
		struct page **cmapool_rmem_page =
			&mtk_iommu_cmapool_info.rmem_page;

		/* Setup dev struct */
		*cmapool_dev = &pdev->dev;
		if (IS_ERR_OR_NULL((const void *)*cmapool_dev)) {
			pr_err("%s: failed to get device, err=%d\n",
					__func__, PTR_ERR_OR_ZERO(
						(const void *)*cmapool_dev));
			ret = -ENODEV;
			break;
		}
		ret = dma_coerce_mask_and_coherent(*cmapool_dev,
				DMA_BIT_MASK(64));
		if (unlikely(ret != 0)) {
			pr_err("%s: failed to set dma coherent mask, err=%d\n",
					__func__, ret);
			break;
		}

		/* Setup rmem for dev */
		ret = of_reserved_mem_device_init_by_idx(*cmapool_dev,
				(*cmapool_dev)->of_node, 0);
		if (unlikely(ret != 0)) {
			pr_err("%s: failed to init reserved memory device, err=%d\n",
					__func__, ret);
			ret = -EBUSY;
			break;
		}

		/* Setup rmem struct */
		rmem_node = of_parse_phandle((*cmapool_dev)->of_node,
				"memory-region", 0);
		if (IS_ERR_OR_NULL((const void *)rmem_node)) {
			pr_err("%s: failed to parse phandle for memory-region, err=%d\n",
					__func__, PTR_ERR_OR_ZERO(
						(const void *)rmem_node));
			ret = -ENODEV;
			break;
		}
		*cmapool_rmem = of_reserved_mem_lookup(rmem_node);
		if (IS_ERR_OR_NULL((const void *)*cmapool_rmem)) {
			pr_err("%s: failed to lookup reserved memory, err=%d\n",
					__func__, PTR_ERR_OR_ZERO(
						(const void *)*cmapool_rmem));
			ret = -EINVAL;
			break;
		}

		/* Alloc rmem */
		pr_info("%s: alloc reserved memory, start=%#llx, num_pages=%#llx\n",
				__func__, (u64)(*cmapool_rmem)->base,
				(u64)(*cmapool_rmem)->size >> PAGE_SHIFT);
		*cmapool_rmem_page = cma_alloc((*cmapool_dev)->cma_area,
				(ulong)(*cmapool_rmem)->size >> PAGE_SHIFT,
				0, false);
		if (IS_ERR_OR_NULL((const void *)*cmapool_rmem_page)) {
			pr_err("%s: failed to alloc reserved memory, err=%d\n",
					__func__, PTR_ERR_OR_ZERO(
						(const void *)*cmapool_rmem_page));
			ret = -ENOMEM;
			break;
		}
		if (unlikely((phys_addr_t)page_to_phys(*cmapool_rmem_page) !=
				(phys_addr_t)(*cmapool_rmem)->base)) {
			pr_err("%s: bad paddr of reserved memory, alloc=%llx, expected=%llx\n",
					__func__,
					(u64)page_to_phys(*cmapool_rmem_page),
					(u64)(*cmapool_rmem)->base);
			ret = -EFAULT;
			break;
		}
		(void)kmemleak_ignore_phys((*cmapool_rmem)->base);

		ret = 0;
	} while (false);

	if (unlikely(ret != 0))
		pr_err("%s: cmapool probe failed, err=%d\n", __func__, ret);

	/* Cleanup rmem node */
	do {
		if (IS_ERR_OR_NULL((const void *)rmem_node))
			break;

		(void)of_node_put(rmem_node);
	} while (false);
	rmem_node = NULL;

	pr_info("%s: done cmapool probe\n", __func__);

	return ret;
}

static void mtk_iommu_cmapool_remove(struct platform_device *pdev)
{
	int ret = 0;

	pr_info("%s: start cmapool remove\n", __func__);

	do {
		bool cma_ret = true;
		void *ptr_ret = NULL;
		struct device **cmapool_dev = &mtk_iommu_cmapool_info.dev;
		struct reserved_mem **cmapool_rmem =
			&mtk_iommu_cmapool_info.rmem;
		struct page **cmapool_rmem_page =
			&mtk_iommu_cmapool_info.rmem_page;

		/* Release rmem */
		pr_info("%s: release reserved memory, num_pages=%#lx\n",
				__func__,
				(ulong)(*cmapool_rmem)->size >> PAGE_SHIFT);
		cma_ret = cma_release((*cmapool_dev)->cma_area,
				*cmapool_rmem_page,
				(ulong)(*cmapool_rmem)->size >> PAGE_SHIFT);
		if (unlikely(!cma_ret)) {
			pr_err("%s: failed to release reserved memory, err=%d\n",
					__func__, ret);
			ret = -ENOMEM;
			break;
		}

		/* Clear out cmapool info */
		ptr_ret = memset((void *)&mtk_iommu_cmapool_info, 0,
				sizeof(mtk_iommu_cmapool_info));
		if (IS_ERR_OR_NULL((const void *)ptr_ret)) {
			pr_err("%s: failed to cleanup cmapool info, err=%d\n",
					__func__, PTR_ERR_OR_ZERO(
						(const void *)ptr_ret));
			ret = -EINVAL;
			break;
		}

		ret = 0;
	} while (false);

	if (unlikely(ret != 0))
		pr_err("%s: cmapool remove failed, err=%d\n", __func__, ret);

	pr_info("%s: done cmapool remove\n", __func__);
}

struct platform_driver mtk_iommu_cmapool_driver = {
	.probe = mtk_iommu_cmapool_probe,
	.remove = mtk_iommu_cmapool_remove,
	.driver = {
		.name = "hypmmu-mtk_iommu-cmapool",
		.of_match_table = mtk_iommu_cmapool_of_match_table,
	},
};

#endif /* defined(MTK_IOMMU_CMAPOOL) && (MTK_IOMMU_CMAPOOL) */

