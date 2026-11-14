/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 *
 */

#ifndef _MTK_TMEM_INTEROP_H
#define _MTK_TMEM_INTEROP_H

#if !defined(TMEM_PRIV)
#	define TMEM_PRIV extern
#endif /* !defined(TMEM_PRIV) */

#include "mtk_iommu.h"
#include "mtk_sec_heap.h"
#include "mtk_sec_heap_priv.h"
#include "mtk-smmu-v3.h"
#include "mtk_page_pool.h"
#include <linux/arm-smccc.h>
#include <linux/highmem.h>
#include <linux/list_sort.h>
#include <linux/types.h>
#include <public/trusted_mem_api.h>

#if IS_ENABLED(CONFIG_MTK_PKVM_SMMU)
#include <asm/kvm_pkvm_module.h>
#include <pkvm_mgmt/pkvm_mgmt.h>
#endif /* IS_ENABLED(CONFIG_MTK_PKVM_SMMU) */

#if IS_ENABLED(CONFIG_MTK_PKVM_TMEM)
#include <asm/kvm_pkvm_module.h>
#include <pkvm_mgmt/pkvm_mgmt.h>
#endif /* IS_ENABLED(CONFIG_MTK_PKVM_TMEM) */

#define BASE_SEC_HEAP_SZ (PAGE_SIZE << 3)

#define ENABLE_PKVM_PMM 1

/* TMEM common functions */

TMEM_PRIV int pmm_common_buffer_v2(struct ssheap_buf_info *ssheap,
		u8 pmm_attr, int lock);

TMEM_PRIV int pmm_assign_buffer_v2(struct ssheap_buf_info *ssheap,
		u8 pmm_attr);

TMEM_PRIV int pmm_unassign_buffer_v2(struct ssheap_buf_info *ssheap,
		u8 pmm_attr);

TMEM_PRIV int paddr_cmp(void *priv, const struct list_head *a,
		const struct list_head *b);

//TMEM_PRIV int pkvm_smmu_mapping(struct page *pmm_page, u8 pmm_attr,
//		u32 tmp_count, int lock);

#if (ENABLE_PKVM_PMM == 0)
TMEM_PRIV void pkvm_smmu_merge_ptable(void);
#endif

TMEM_PRIV inline void pmm_set_msg_entry(u32 *pmm_msg, u32 index,
		struct page *page);

TMEM_PRIV struct page *pmm_alloc_msg_v2(struct sg_table *table,
		struct ssheap_buf_info *ssheap, uint max_order);

TMEM_PRIV struct ssheap_buf_info *ssheap_create_by_sgtable(
		struct sg_table *table, ulong req_sz);

int tmem_api_ver(void);

/* TMEM page-based functions */

TMEM_PRIV struct page *page_alloc_largest_available(ulong size,
		uint max_order);

TMEM_PRIV int page_alloc(struct secure_heap_page *sec_heap,
		struct mtk_sec_heap_buffer *buffer, ulong req_sz);

TMEM_PRIV int page_alloc_v2(struct secure_heap_page *sec_heap,
		struct mtk_sec_heap_buffer *buffer, ulong req_sz);

struct dma_buf *tmem_page_alloc(struct dma_heap *heap, ulong len, u32 fd_flags,
		u64 heap_flags);

TMEM_PRIV int page_free(struct secure_heap_page *sec_heap,
		struct mtk_sec_heap_buffer *buffer);

TMEM_PRIV int page_free_v2(struct secure_heap_page *sec_heap,
		struct mtk_sec_heap_buffer *buffer);

void tmem_page_free(struct dma_buf *dmabuf);

/* TMEM region-based functions */

TMEM_PRIV int region_alloc(struct secure_heap_region *sec_heap,
		struct mtk_sec_heap_buffer *buffer, ulong req_sz,
		bool aligned);

struct dma_buf *tmem_region_alloc(struct dma_heap *heap, ulong len,
		u32 fd_flags, u64 heap_flags);

TMEM_PRIV int region_free(struct secure_heap_region *sec_heap,
		struct mtk_sec_heap_buffer *buffer);

void tmem_region_free(struct dma_buf *dmabuf);

#endif /* _MTK_TMEM_INTEROP_H */

