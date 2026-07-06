/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifndef __VPUBUF_DMA_CONTIG_H__
#define __VPUBUF_DMA_CONTIG_H__

#include "vpubuf-core.h"
#include <linux/dma-mapping.h>

void *vpu_dma_contig_init_ctx(struct device *dev);
void vpu_dma_contig_cleanup_ctx(void *alloc_ctx);

extern const struct vpu_mem_ops vpu_dma_contig_memops;

#endif
