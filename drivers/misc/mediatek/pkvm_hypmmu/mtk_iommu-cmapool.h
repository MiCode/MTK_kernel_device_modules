/* SPDX-License-Identifier: GPL-2.0 */
/* hypmmu-mtk_iommu-cmapool.h
 *
 * Hypmmu CMA pool for mtk_iommu.
 *
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef _MTK_IOMMU_CMAPOOL_H
#define _MTK_IOMMU_CMAPOOL_H

#if defined(MTK_IOMMU_CMAPOOL) && (MTK_IOMMU_CMAPOOL)

#include <linux/compiler.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/types.h>

#ifdef _MTK_IOMMU_CMAPOOL_C

#include <linux/cma.h>
#include <linux/dma-mapping.h>
#include <linux/kmemleak.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/printk.h>
#include <linux/string.h>

__maybe_unused static struct {
	struct device *dev;
	struct reserved_mem *rmem;
	struct page *rmem_page;
} mtk_iommu_cmapool_info;

__maybe_unused const struct of_device_id mtk_iommu_cmapool_of_match_table[] = {
	{ .compatible = "mediatek,hypmmu-mtk_iommu-cma" },
	{},
};

static int mtk_iommu_cmapool_probe(struct platform_device *pdev);

static void mtk_iommu_cmapool_remove(struct platform_device *pdev);

#endif /* _MTK_IOMMU_CMAPOOL_C */

extern struct platform_driver mtk_iommu_cmapool_driver;

#endif /* defined(MTK_IOMMU_CMAPOOL) && (MTK_IOMMU_CMAPOOL) */

#endif /* _MTK_IOMMU_CMAPOOL_H */

