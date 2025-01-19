/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_APU_SYSMEM_DRV_H__
#define __MTK_APU_SYSMEM_DRV_H__

#include "apu_sysmem.h"

/* log definition */
#define apu_sysmem_err(x, args...) \
	pr_info("[error][%s/%d]" x, __func__, __LINE__, ##args)
#define apu_sysmem_warn(x, args...) \
	pr_info("[warn][%s/%d]" x, __func__, __LINE__, ##args)
#define apu_sysmem_info(x, args...) \
	pr_info("[info][%s/%d]" x, __func__, __LINE__, ##args)
#define apu_sysmem_debug(x, args...) \
	pr_info("[debug][%s/%d]" x, __func__, __LINE__, ##args)

/* support allocation type */
struct dma_buf *apu_sysmem_dmaheap_alloc(uint64_t size, uint64_t flags);
int apu_sysmem_dmaheap_free(struct dma_buf *dbuf);

struct dma_buf *apu_sysmem_pages_alloc(uint64_t size, uint64_t flags);
int apu_sysmem_pages_free(struct dma_buf *dbuf);

struct dma_buf *apu_sysmem_aram_alloc(uint64_t session_id, enum apu_sysmem_mem_type type, uint64_t size, uint64_t flags);
int apu_sysmem_aram_free(struct dma_buf *dbuf);

#endif