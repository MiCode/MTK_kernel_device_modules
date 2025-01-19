// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef _MTK_SMMU_SSID_H_
#define _MTK_SMMU_SSID_H_

#include <linux/iommu.h>
#include <linux/rbtree.h>

#include "mtk-smmu-v3.h"

static inline int smmu_ssid_rb_cmp(const void *k, const struct rb_node *node)
{
	const struct arm_smmu_ssid_domain *ref = rb_entry(node,
			struct arm_smmu_ssid_domain, node);
	const struct arm_smmu_ssid_domain *key = (void *)k;

	if (ref->ssid > key->ssid)
		return -1;
	else if (ref->ssid < key->ssid)
		return 1;

	return 0;
}

static inline bool smmu_ssid_rb_less(struct rb_node *node,
				     const struct rb_node *parent)
{
	const struct arm_smmu_ssid_domain *e = rb_entry(node,
			struct arm_smmu_ssid_domain, node);

	return smmu_ssid_rb_cmp(e, parent) < 0;
}

u64 mtk_smmu_get_tab_id_ssid(struct device *dev, u32 ssid);
struct device_node *mtk_parse_dma_region_ssid(struct device *dev, u32 ssid);

int mtk_enable_smmu_ssid(struct device *dev, u32 ssid);
int mtk_disable_smmu_ssid(struct device *dev, u32 ssid);
int mtk_release_smmu_ssids(struct device *dev);
dma_addr_t mtk_smmu_map_pages_ssid(struct device *dev, struct page *page,
				   unsigned long offset, size_t size,
				   enum dma_data_direction dir,
				   unsigned long attrs, u32 ssid);
void mtk_smmu_unmap_pages_ssid(struct device *dev, dma_addr_t dma_addr,
			       size_t size, enum dma_data_direction dir,
			       unsigned long attrs, u32 ssid);
int mtk_smmu_map_sg_ssid(struct device *dev, struct scatterlist *sg, int nents,
			 enum dma_data_direction dir, unsigned long attrs,
			 u32 ssid);
void mtk_smmu_unmap_sg_ssid(struct device *dev, struct scatterlist *sg,
			    int nents, enum dma_data_direction dir,
			    unsigned long attrs, u32 ssid);
phys_addr_t mtk_smmu_iova_to_phys_ssid(struct device *dev, dma_addr_t iova,
				       u32 ssid);
u64 mtk_smmu_iova_to_iopte_ssid(struct device *dev, dma_addr_t iova, u32 ssid);

#endif /* _MTK_SMMU_SSID_H_ */
