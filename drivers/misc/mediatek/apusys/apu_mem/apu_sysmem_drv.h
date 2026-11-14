/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_APU_SYSMEM_DRV_H__
#define __MTK_APU_SYSMEM_DRV_H__

#include "apu_sysmem.h"

/* log definition */

extern uint32_t g_apu_sysmem_klog;

enum {
	APU_SYSMEM_ERR = 0x1,
	APU_SYSMEM_WRN = 0x2,
	APU_SYSMEM_INFO = 0x4,
	APU_SYSMEM_DBG = 0x8,
	APU_SYSMEM_VERBO = 0x10,
};

static inline int apu_sysmem_log_level_check(int log_level)
{
	return g_apu_sysmem_klog & log_level;
}

#define apu_sysmem_err(x, args...) \
	do { \
		if (apu_sysmem_log_level_check(APU_SYSMEM_ERR)) \
			pr_info("[error][%s/%d]" x, __func__, __LINE__, ##args); \
	} while (0)
#define apu_sysmem_warn(x, args...) \
	do { \
		if (apu_sysmem_log_level_check(APU_SYSMEM_WRN)) \
			pr_info("[warn][%s/%d]" x, __func__, __LINE__, ##args); \
	} while (0)
#define apu_sysmem_info(x, args...) \
	do { \
		if (apu_sysmem_log_level_check(APU_SYSMEM_INFO)) \
			pr_info("[info][%s/%d]" x, __func__, __LINE__, ##args); \
	} while (0)
#define apu_sysmem_debug(x, args...) \
	do { \
		if (apu_sysmem_log_level_check(APU_SYSMEM_DBG)) \
			pr_info("[debug][%s/%d]" x, __func__, __LINE__, ##args); \
	} while (0)

/* support allocation type */
struct dma_buf *apu_sysmem_dmaheap_alloc(uint64_t size, uint64_t flags);
int apu_sysmem_dmaheap_free(struct dma_buf *dbuf);

struct dma_buf *apu_sysmem_pages_alloc(uint64_t size, uint64_t flags);
int apu_sysmem_pages_free(struct dma_buf *dbuf);

struct dma_buf *apu_sysmem_aram_alloc(uint64_t session_id, enum apu_sysmem_mem_type type, uint64_t size, uint64_t flags);
int apu_sysmem_aram_free(struct dma_buf *dbuf);

#endif