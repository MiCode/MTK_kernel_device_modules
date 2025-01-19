// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>

#include "apu_sysmem_drv.h"

#define APU_SYSMEM_DMAHEAP_CACHED_NAME "mtk_mm"
#define APU_SYSMEM_DMAHEAP_UNCACHED_NAME "mtk_mm-uncached"

struct dma_buf *apu_sysmem_dmaheap_alloc(uint64_t size, uint64_t flags)
{
	struct dma_heap *dma_heap = NULL;
	struct dma_buf *dbuf = NULL;

	if (flags & F_APU_SYSMEM_FLAG_CACHEABLE)
		dma_heap = dma_heap_find(APU_SYSMEM_DMAHEAP_CACHED_NAME);
	else
		dma_heap = dma_heap_find(APU_SYSMEM_DMAHEAP_UNCACHED_NAME);

	if (dma_heap == NULL) {
		apu_sysmem_err("can't find available dma heap, flags(0x%llx)\n", flags);
		goto out;
	}

	/* allocate buffer from dma heap */
	dbuf = dma_heap_buffer_alloc(dma_heap, size, O_RDWR | O_CLOEXEC, 0);
	if (IS_ERR_OR_NULL(dbuf)) {
		apu_sysmem_err("allocate buffer(%llu/0x%llx) from dma heap failed\n", size, flags);
		dbuf = NULL;
	}
	dma_heap_put(dma_heap);

out:
	if (dbuf) {
		apu_sysmem_debug("heap(%s) dbuf(0x%llx) size(%llu) flags(0x%llx)\n",
		dbuf->exp_name, (uint64_t)dbuf, size, flags);
	}

	return dbuf;
}

int apu_sysmem_dmaheap_free(struct dma_buf *dbuf)
{
	apu_sysmem_debug("dbuf(0x%llx)\n", (uint64_t)dbuf);

	if (IS_ERR_OR_NULL(dbuf))
		return -EINVAL;

	dma_heap_buffer_free(dbuf);

	return 0;
}