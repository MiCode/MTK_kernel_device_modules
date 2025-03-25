/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef _FIFO_DST_COPY_H_
#define _FIFO_DST_COPY_H_

#include <inc/engine_fifo.h>

/* CMD bucket buffers */

/* 4096 */
#define ENGINE_MAX_BUF_SELECTED		(1)
#define ENGINE_MIN_BUF_VAL		(7)
#define ENGINE_MIN_BUF_IDX		(6)	/* Index for buffer allocator */
#define ENGINE_MAX_BUF_VAL		(7)
#define ENGINE_MAX_BUF_IDX		(6)	/* Index for buffer allocator */
#define ENGINE_MIN_BUF_SIZES		(4096)
#define ENGINE_MAX_BUF_SIZES		(4096)
#define ENGINE_MAX_COMPRESSIBLE_SIZE	(4096)
#define ENGINE_BUF_ENABLE		(0x40)	/* 1000000b */

/* Compression CMD format relatives */

#define COMP_CMD_OFFSET_0_RESET_MASK		(0x4000000000000100UL)	/* buf_enable & fifo are invariant
									   after initialization */

/*
 * 1. Clear src_addr, I bit and set status to COMP_CMD_IDLE (0x0).
 * 2. Set word_1_value to 0x0.
 */
static inline void reset_cmd_after_compression(struct compress_cmd *cmd, uint32_t id)
{
	/* After the completion of HW compression, CMD should be post-processed before doing reset */

	cmd->word_0_value = (cmd->word_0_value & COMP_CMD_OFFSET_0_RESET_MASK);
	cmd->word_1_value = 0x0;

	/* CMD is invalid, try to restore it */
	if (unlikely(cmd->buf_enable != ENGINE_BUF_ENABLE)) {
		cmd->buf_enable = ENGINE_BUF_ENABLE;
		cmd->fifo = id;
	}
}

/*
 * 1. Clear src_addr, I bit to 0x0.
 * 2. Set word_1_value to 0x0.
 * 3. Set cmd status to COMP_CMD_PP_ERROR.
 */
static inline void reset_cmd_after_compression_pp_err(struct compress_cmd *cmd)
{
	/* After the completion of HW compression, CMD should be post-processed before doing reset */

	cmd->word_0_value = (cmd->word_0_value & COMP_CMD_OFFSET_0_RESET_MASK);
	cmd->word_1_value = 0x0;

	/* Error occurs during post-processing */
	cmd->status = COMP_CMD_PP_ERROR;
}

/* Decompression CMD format relatives */

#define DCOMP_CMD_OFFSET_0_RESET_MASK		(0x000F000000000700UL)	/* BUF_a & fifo are invariant */

static inline void reset_cmd_after_decompression(struct decompress_cmd *cmd)
{
	/* After the completion of HW decompression, CMD should be post-processed before doing reset */

	cmd->word_0_value = (cmd->word_0_value & DCOMP_CMD_OFFSET_0_RESET_MASK);
}

#endif /* _FIFO_DST_COPY_H_ */
