/* SPDX-License-Identifier: GPL-2.0 */
/* hypmmu-mtk_iommu-pagepool.h
 *
 * Hypmmu PAGE pool for mtk_iommu.
 *
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef _MTK_IOMMU_PAGEPOOL_H
#define _MTK_IOMMU_PAGEPOOL_H

#define HYPPMM_SET_CMA_REGION (0XBB00FFA8)
#define IOMMU_DRIVER_MEM_PFN_MAX (100U)

struct fmpt {
	u64 *smpt;
	u64 mem_order;
};

struct mpt {
	/* Memory used by IOMMU driver */
	struct fmpt fmpt[IOMMU_DRIVER_MEM_PFN_MAX];
	u32 mem_block_num;
};

unsigned long alloc_iommu_pgtbl_page(void);
bool is_iommu_pgtbl_page_memory(void);

#endif /* _MTK_IOMMU_PAGEPOOL_H */

