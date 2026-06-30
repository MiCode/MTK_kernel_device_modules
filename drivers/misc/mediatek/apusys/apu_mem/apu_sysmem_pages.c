// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/highmem.h>
#include <linux/dma-heap.h>

#include "apu_sysmem_drv.h"

struct dma_buf *apu_sysmem_pages_alloc(uint64_t size, uint64_t flags)
{
	return NULL;
}

int apu_sysmem_pages_free(struct dma_buf *dbuf)
{
	return -EINVAL;
}