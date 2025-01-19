/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 *
 */

#ifndef _MTK_SEC_HEAP_PRIV_H
#define _MTK_SEC_HEAP_PRIV_H

#include "mtk_heap_priv.h"
#include <public/trusted_mem_api.h>

extern bool smmu_v3_enable;

extern const struct dma_buf_ops sec_buf_region_ops;
extern const struct dma_buf_ops sec_buf_page_ops;

extern const struct dma_heap_ops sec_heap_page_ops;
extern const struct dma_heap_ops sec_heap_region_ops;

#define LOW_ORDER_GFP (GFP_HIGHUSER | __GFP_ZERO | __GFP_COMP)
#define MID_ORDER_GFP (LOW_ORDER_GFP | __GFP_NOWARN)
#define HIGH_ORDER_GFP \
	(((GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN | __GFP_NORETRY) & \
		~__GFP_RECLAIM) | __GFP_COMP)

/*
 * 4KB page granule: 4KB
 * 16KB page granule: 16KB
 */
static int orders[1] = { 0 };
#define NUM_ORDERS ARRAY_SIZE(orders)

extern struct dmabuf_page_pool *pools[NUM_ORDERS];

enum HEAP_BASE_TYPE {
	HEAP_TYPE_INVALID = 0,
	REGION_BASE,
	PAGE_BASE,
	HEAP_BASE_NUM
};

enum REGION_TYPE {
	REGION_HEAP_NORMAL,
	REGION_HEAP_ALIGN,
	REGION_TYPE_NUM,
};

struct mtk_sec_heap_buffer {
	struct dma_heap *heap;
	struct list_head attachments;
	struct mutex lock;
	unsigned long len;
	struct sg_table sg_table;
	int vmap_cnt;
	void *vaddr;
	bool uncached;
	/* helper function */
	int (*show)(const struct dma_buf *dmabuf, struct seq_file *s);

	/* secure heap will not strore sgtable here */
	struct list_head iova_caches;
	struct mutex map_lock; /* map iova lock */
	pid_t pid;
	pid_t tid;
	char pid_name[TASK_COMM_LEN];
	char tid_name[TASK_COMM_LEN];
	unsigned long long ts; /* us */

	int gid; /* slc */

	/* private part for secure heap */
	u64 sec_handle; /* keep same type with tmem */
	struct ssheap_buf_info *ssheap; /* for page base */
};

struct secure_heap_region {
	bool heap_mapped; /* indicate whole region if it is mapped */
	bool heap_filled; /* indicate whole region if it is filled */
	struct mutex heap_lock;
	const char *heap_name[REGION_TYPE_NUM];
	atomic64_t total_size;
	phys_addr_t region_pa;
	u32 region_size;
	struct sg_table *region_table;
	struct dma_heap *heap[REGION_TYPE_NUM];
	struct device *heap_dev;
	struct device *iommu_dev; /* smmu uses shared dev */
	enum TRUSTED_MEM_REQ_TYPE tmem_type;
	enum HEAP_BASE_TYPE heap_type;
};

struct secure_heap_page {
	const char *heap_name;
	atomic64_t total_size;
	struct dma_heap *heap;
	struct device *heap_dev;
	struct page *bitmap;
	enum TRUSTED_MEM_REQ_TYPE tmem_type;
	enum HEAP_BASE_TYPE heap_type;
};

bool region_heap_is_aligned(struct dma_heap *heap);

void init_buffer_info(struct dma_heap *heap,
		struct mtk_sec_heap_buffer *buffer);

struct dma_buf *alloc_dmabuf(struct dma_heap *heap,
		struct mtk_sec_heap_buffer *buffer,
		const struct dma_buf_ops *ops, u32 fd_flags);

struct secure_heap_page *sec_heap_page_get(const struct dma_heap *heap);

struct secure_heap_region *sec_heap_region_get(const struct dma_heap *heap);

int region_base_alloc(struct secure_heap_region *sec_heap,
		struct mtk_sec_heap_buffer *buffer, unsigned long req_sz,
		bool aligned);

int region_base_free(struct secure_heap_region *sec_heap,
		struct mtk_sec_heap_buffer *buffer);

int sec_buf_priv_dump(const struct dma_buf *dmabuf, struct seq_file *s);

int copy_sec_sg_table(struct sg_table *source, struct sg_table *dest);

int fill_heap_sgtable(struct secure_heap_region *sec_heap,
		struct mtk_sec_heap_buffer *buffer);

#endif /* _MTK_SEC_HEAP_PRIV_H */

