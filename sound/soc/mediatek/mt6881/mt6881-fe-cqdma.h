/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#ifndef _MT6881_FE_CQDMA_H_
#define _MT6881_FE_CQDMA_H_

#include "mt6881-afe-common.h"

#define CQDMA_SOURCE_0 0
#define CQDMA_SOURCE_1 0x40000000

#define CQDMA_BE_FIFO_SIZE 0x40 /* 64 bytes */
#define CQDMA_FE_MAX_CHUNK_SIZE (CQDMA_BE_FIFO_SIZE * 8)
#define CQDMA_FE_MIN_CHUNK_SIZE (CQDMA_BE_FIFO_SIZE)

#define CQDMA_PERIOD_BYTES_MIN 64
#define CQDMA_PERIOD_BYTES_MAX (256 * 1024)
#define CQDMA_PERIODS_MIN 2
#define CQDMA_PERIODS_MAX 256
#define CQDMA_BUFFER_BYTES_MAX (256 * 2 * 1024)

#define REGMAP_READ_POLL_MIN 1000 /* usec */
#define REGMAP_READ_POLL_MAX (10 * 1000) /* usec */

#define TO_DMA_ADDR(msb, lsb) \
	(((u64)(msb) << 32) | (lsb))

enum {
	DAI_CQDMA0,
	DAI_CQDMA1,
	DAI_CQDMA_NUM,
};

enum cqdma_rwsize {
	CQDMA_RWSIZE_1BYTE,
	CQDMA_RWSIZE_2BYTE,
	CQDMA_RWSIZE_4BYTE,
	CQDMA_RWSIZE_8BYTE,
};

enum cqdma_burst_len {
	CQDMA_BURST_LEN_0_8,
	CQDMA_BURST_LEN_1_8,
	CQDMA_BURST_LEN_2_8,
	CQDMA_BURST_LEN_3_8,
	CQDMA_BURST_LEN_4_8,
	CQDMA_BURST_LEN_5_8,
	CQDMA_BURST_LEN_6_8,
	CQDMA_BURST_LEN_7_8,
};

struct mtk_afe_cqdma_priv {
	dma_addr_t dma_start_addr;
	unsigned int chunk_size; /* transfer length */
	unsigned int buffer_chunk_cnt; /* number of chunks for whole buffer */
	unsigned int irq_step; /* number of chunks per update */
	unsigned int total_buffer_bytes;
};
#endif // _MT6881_FE_CQDMA_H_
