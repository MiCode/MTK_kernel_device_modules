/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef _FIFO_NO_DST_COPY_H_
#define _FIFO_NO_DST_COPY_H_

#include <inc/engine_fifo.h>

/* CMD bucket buffers */

/* 64, 128, 256, 512, 1024, 2048 */
#define ENGINE_MAX_BUF_SELECTED		(4)
#define ENGINE_MIN_BUF_VAL		(1)
#define ENGINE_MIN_BUF_IDX		(5)	/* Index for buffer allocator */
#define ENGINE_MAX_BUF_VAL		(6)
#define ENGINE_MAX_BUF_IDX		(0)	/* Index for buffer allocator */
#define ENGINE_MIN_BUF_SIZES		(64)
#define ENGINE_MAX_BUF_SIZES		(2048)
#define ENGINE_MAX_COMPRESSIBLE_SIZE	(3840)
#define ENGINE_BUF_ENABLE		(0x3f)	/* 0111111b */

/* Compression CMD format relatives */

#define COMP_CMD_OFFSET_0_RESET_MASK		(0x3f00000000000100UL)	/* buf_enable & fifo are invariant
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

#define DCOMP_CMD_OFFSET_0_RESET_MASK		(0x0000000000000700UL)	/* fifo are invariant */

static inline void reset_cmd_after_decompression(struct decompress_cmd *cmd)
{
	/* After the completion of HW decompression, CMD should be post-processed before doing reset */

	cmd->word_0_value = (cmd->word_0_value & DCOMP_CMD_OFFSET_0_RESET_MASK);
}

/* CMD format relatives */

#define BUF_X_START_POS_SHIFT	(48)
#define BUF_X_START_POS_MASK	(~((1UL << BUF_X_START_POS_SHIFT) - 1))
#define BUF_X_BITS		(4)
#define BUF_X_BITS_MASK		((1UL << BUF_X_BITS) - 1)

#endif /* _FIFO_NO_DST_COPY_H_ */

