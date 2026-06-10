// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#include <inc/fifo_dst_copy.h>
#include <inc/engine_ops.h>
#include <inc/helpers.h>
#include "../hwcomp_bridge.h"

/* Structure and macro for current fifo implementation (backup of word_2_value during decompression) */
typedef struct { uint64_t val; } dcomp_src_t;
#define DCOMP_PRIV(fifo, entry)	(fifo->priv + entry * sizeof(dcomp_src_t))

/***************************************
 ** Main implementation for DST Copy ***
 ***************************************/

/*
 * Post-process one decompression cmd
 */
static void __dcomp_process_completed_cmd(struct hwfifo *fifo, uint32_t entry, bool async)
{
	struct decompress_cmd *cmdp = DCOMP_CMD(fifo, entry);
	struct dcomp_pp_info *pp_info = DCOMP_CMPL(fifo, entry);
	dcomp_src_t *hw_priv = DCOMP_PRIV(fifo, entry);
	unsigned int dcomp_status;
	int err = 0;

	if (engine_coherence_disabled())
		invalidate_dcache((unsigned long)cmdp, (unsigned long)cmdp + ENGINE_DCOMP_CMD_SIZE);

	dcomp_status = ((unsigned int)READ_ONCE(cmdp->word_0_value)) & DCOMP_CMD_STATUS_MASK;

	switch (dcomp_status) {
	/* Successfully decompressed by HW */
	case DCOMP_CMD_DECOMPRESSED:
		break;

	/* This CMD is not processed correctly during HW decompression */
	case DCOMP_CMD_ERROR:
		/* Dump CMD */
		dump_dcomp_cmd(cmdp);
		pr_info("%s: fail - decompressed status (0x%x).\n", __func__, dcomp_status);
		err = -EIO;
		break;

	/* Successfully decompressed by HW, but the HASH verification is failed */
	case DCOMP_CMD_HASH_FAIL:
		err = -EIO;
		break;

	default:
		/* Dump CMD */
		dump_dcomp_cmd(cmdp);
#ifdef ZRAM_ENGINE_DEBUG
		pr_info("%s: fifo_va(%llx) fifo_pa(%llx)\n", __func__, (uint64_t)fifo->buf, (uint64_t)fifo->buf_pa);
#endif
		/* It's unexpected to be here. dump registers... (TODO) */
		pr_info("%s: unexpected decompressed status (0x%x).\n", __func__, dcomp_status);
		err = -EIO;
		break;
	}

	/* Callback for remaining works */
	if (async)
		hwcomp_decompress_post_process(err, pp_info);

	/* Restore fixed buffer */
	cmdp->word_2_value = hw_priv->val;

	/* Post-process is finished. */
	reset_cmd_after_decompression(cmdp);
}

/*
 * Routine for post-processing compression cmd with COMP_CMD_COMPRESSED state
 */
static void *process_cmd_after_compression(struct compress_cmd *cmdp, int *err)
{
	uint32_t *dst_addr;
	uint64_t compressed_addr;
	phys_addr_t phys_addr;

	dst_addr = (uint32_t *)&cmdp->word_7_value;	/* 4096 bytes */
	compressed_addr = *dst_addr;
	phys_addr = DST_ADDR_TO_PHYS(compressed_addr);

	/* No error */
	*err = 0;

	return phys_to_virt(phys_addr);
}

static void invalidate_comp_dst_buffers(struct compress_cmd *cmdp)
{
	uint32_t *dst_addr;
	unsigned long from_addr;
	unsigned int copy_size = ENGINE_MAX_BUF_SIZES;	/* 4096 bytes */

	dst_addr = (uint32_t *)&cmdp->word_7_value;
	from_addr = (unsigned long)cmd_buf_addr_to_va(dst_addr);
	invalidate_dcache((unsigned long)from_addr, (unsigned long)from_addr + copy_size);
}

/*
 * Post-process one compression cmd
 */
static void comp_process_completed_cmd(struct hwfifo *fifo, uint32_t entry, bool silence)
{
	struct compress_cmd *cmdp = COMP_CMD(fifo, entry);
	struct comp_pp_info *pp_info = COMP_CMPL(fifo, entry);
	unsigned int comp_status;
	unsigned int comp_size = 0;
	uint64_t repeat_pattern;
	void *buffer = NULL;
	enum hwcomp_flags flag = HWCOMP_INVAL;
	int err = 0;				/* Error for HW processed result */
	int pp_err = 0;				/* Error for SW post-processed result */

	if (engine_coherence_disabled())
		invalidate_dcache((unsigned long)cmdp, (unsigned long)cmdp + ENGINE_COMP_CMD_SIZE);

	comp_status = get_comp_cmd_status(cmdp);

	switch (comp_status) {
	/* Contains repeated pattern */
	case COMP_CMD_REPEATED:
		/* Transform byte to uint64_t */
		repeat_pattern = (uint64_t) cmdp->repeat_pattern | (cmdp->repeat_pattern << 8);
		repeat_pattern = repeat_pattern | (repeat_pattern << 16);
		repeat_pattern = repeat_pattern | (repeat_pattern << 32);
		/* Update post-process info */
		pp_info->repeat_pattern = repeat_pattern;	/* override original page pointer */
		flag = HWCOMP_SAME;
		/* Update comp_size to sizeof(uint64_t) */
		comp_size = sizeof(uint64_t);
		break;

	/* Compressed size larger than ENGINE_MAX_COMPRESSIBLE_SIZE */
	case COMP_CMD_INCOMPRESSIBLE:
		/* Update post-process info */
		flag = HWCOMP_HUGE;
		/* Update comp_size to max compressible size */
		comp_size = ENGINE_MAX_COMPRESSIBLE_SIZE;
		break;

	/* Successfully compressed by HW */
	case COMP_CMD_COMPRESSED:
		/* Acquire compressed information from cmd */
		buffer = process_cmd_after_compression(cmdp, &pp_err);
		/* Update post-process info */
		flag = HWCOMP_NORMAL;
		/* Update comp_size to actual compressed size */
		comp_size = cmdp->compr_size;
		/* Invalidate dcache for dst */
		if (engine_coherence_disabled())
			invalidate_comp_dst_buffers(cmdp);
		break;

	/* This CMD is not processed correctly during HW compression */
	case COMP_CMD_ERROR:
		/* Dump CMD */
		if (!silence)
			dump_comp_cmd(cmdp);
		/* No valid HW processed result */
		err = -EIO;
		break;

	default:
		/* It's unexpected to be here. dump more information... */
		pr_info("%s: unexpected compressed status (0x%x).\n", __func__, comp_status);
		/* Dump CMD */
		if (!silence)
			dump_comp_cmd(cmdp);
		/* No valid HW processed result */
		err = -EIO;
		break;
	}

	/* Callback for remaining works */
	hwcomp_compress_post_process_dc(err, buffer, comp_size, pp_info, flag);

	/* Post-process is finished */

	/* Reset the cmd according to pp_err */
	if (!pp_err)
		reset_cmd_after_compression(cmdp, fifo->id);
	else
		;/* Never comes here! */

#ifdef ZRAM_ENGINE_DEBUG
	/* DEBUG */
	dump_comp_cmd(cmdp);
#endif
}

/* HW decompressed buffer has the requirement of end alignment to 16bytes. */
#define SW_TO_HW_COPY_ALIGNED	(16)

/*
 * Prepare for the decompression of one page (called by hwcomp_decompress_page).
 * Unlike No-DST copy mode, "dcomp_pp_info *from" is necessary no matter whether
 * async is false or true.
 */
static bool fill_decompression_info(struct hwfifo *fifo, uint32_t entry,
		void *src, unsigned int slen, struct page *page,
		struct dcomp_pp_info *from, bool async,
		zspool_to_hwcomp_buffer_fn zspool_to_hwcomp_buffer)
{
	struct decompress_cmd *cmdp = DCOMP_CMD(fifo, entry);
	struct dcomp_pp_info *pp_info = DCOMP_CMPL(fifo, entry);
	void *src_va;
	uint32_t hash_value = 0;
	int ret;

	/* Set cmd as DCOMP_CMD_IDLE. If it's invalid, and then bypass it. */
	if (dcomp_cmd_check_invalid(cmdp))
		return false;

#ifdef ZRAM_ENGINE_DEBUG
	dump_dcomp_cmd(cmdp);
#endif

	/* Setup src buffers for decompression (src_va is 4096 aligned) */
	src_va = cmd_buf_addr_to_va((uint32_t *)&cmdp->word_2_value);
	ret = zspool_to_hwcomp_buffer(from, src_va, (unsigned long)src,
			slen, SW_TO_HW_COPY_ALIGNED);
	if (ret) {
		pr_info("%s: failed to setup src buffers.\n", __func__);
		return false;
	}

	/* Initialize decompress cmd */
	update_cmd_before_decompression(cmdp, page, slen, hash_value, false, false, async);

	/* Decompression info is initialized. */
	if (async) {
		memcpy(pp_info, from, sizeof(struct dcomp_pp_info));

		/* Increase the reference count for async decompression */
		refcount_inc_for_async_dcomp(from);
	}

#ifdef ZRAM_ENGINE_DEBUG
	dump_dcomp_cmd(cmdp);
#endif

	/* Flush dcache for src buffer, fifo cmd & dst page */
	if (engine_coherence_disabled()) {
		flush_dcache((unsigned long)src_va, (unsigned long)src_va + slen);
		flush_dcache((unsigned long)cmdp, (unsigned long)cmdp + ENGINE_DCOMP_CMD_SIZE);
		flush_dcache((unsigned long)phys_to_virt((page_to_phys(page))),
				(unsigned long)phys_to_virt((page_to_phys(page))) + PAGE_SIZE);
	}

	return true;
}

/*
 * Pairs for allocate & release decompression fifo SRC buffers
 */
static void release_dcomp_fifo_src_buffers(struct hwfifo *fifo)
{
	struct decompress_cmd *cmdp;
	uint32_t entry;
	uint32_t *src_addr;
	int bit = ENGINE_MAX_BUF_IDX;

	if (!fifo) {
		pr_info("%s: empty fifo.\n", __func__);
		return;
	}

	for (entry = 0; entry < fifo->size; entry++) {
		cmdp = DCOMP_CMD(fifo, entry);
		src_addr = (uint32_t *)&cmdp->word_2_value;

		/* Free the corresponding buffer */
		free_entry_buffer(src_addr, bit);
	}

	/* Release priv */
	kfree(fifo->priv);
}

static int fill_dcomp_fifo_src_buffers(struct hwfifo *fifo, int id)
{
	struct decompress_cmd *cmdp;
	uint32_t entry;
	uint32_t *src_addr;
	int bit = ENGINE_MAX_BUF_IDX;
	dcomp_src_t *hw_priv;

	if (!fifo)
		return -EINVAL;

	for (entry = 0; entry < fifo->size; entry++) {
		cmdp = DCOMP_CMD(fifo, entry);

		src_addr = (uint32_t *)&cmdp->word_2_value;

		/* Fill SRC buffer */
		if (refill_entry_buffer(src_addr, bit, false)) {
			pr_info("%s: failed to fill src buffer for (%u)(%d).\n", __func__, entry, bit);
			goto error;
		}

		/* Expected to be 7 */
		cmdp->buf_a = ENGINE_MAX_BUF_VAL;
		cmdp->fifo = id;
	}

	/* Setup priv for current fifo implementation (backup of word_2_value during decompression) */
	fifo->priv = kzalloc(fifo->size * sizeof(dcomp_src_t), GFP_KERNEL);
	if (fifo->priv == NULL) {
		pr_info("%s: failed to allocate private buffer.\n", __func__);
		goto error;
	}

	/*
	 * Offset: 0x10 ~ 0x18 will be cleared to 0 after HW processes the DCMD.
	 * We use fixed buffer indicated by SRC_ADDR_a to store compressed data,
	 * and we don't want to miss it after the decompression. So keep it here.
	 */
	for (entry = 0; entry < fifo->size; entry++) {
		cmdp = DCOMP_CMD(fifo, entry);
		hw_priv = DCOMP_PRIV(fifo, entry);
		hw_priv->val = cmdp->word_2_value;
	}

#ifdef ZRAM_ENGINE_DEBUG
	/* DEBUG */
	dump_all_dcomp_cmd(fifo);
#endif

	return 0;

error:
	/* Error occurs. Release all allocated buffers and return error status. */
	release_dcomp_fifo_src_buffers(fifo);
	return -ENOMEM;
}

/*
 * Prepare for the compression of one page
 */
static bool fill_compression_info(struct hwfifo *fifo, uint32_t entry,
		struct page *page, struct comp_pp_info *from, bool ref_inc)
{
	struct compress_cmd *cmdp = COMP_CMD(fifo, entry);
	struct comp_pp_info *pp_info = COMP_CMPL(fifo, entry);

	/* Set cmd as COMP_CMD_IDLE. If it's invalid, and then bypass it. */
	if (comp_cmd_check_invalid(cmdp, entry))
		return false;

	/* Initialize compress cmd */
	update_cmd_before_compression(cmdp, page, false);

	/* Initialize structure for post-processing */
	memcpy(pp_info, from, sizeof(struct comp_pp_info));

	/* Increase the reference count for compression */
	if (ref_inc)
		refcount_inc_for_comp(from);

#ifdef ZRAM_ENGINE_DEBUG
	dump_comp_cmd(cmdp);
#endif

	/* Invalidate dcache for dst. Flush dcache for fifo cmd & src page */
	if (engine_coherence_disabled()) {
		invalidate_comp_dst_buffers(cmdp);
		flush_dcache((unsigned long)cmdp, (unsigned long)cmdp + ENGINE_COMP_CMD_SIZE);
		flush_dcache((unsigned long)phys_to_virt((page_to_phys(page))),
				(unsigned long)phys_to_virt((page_to_phys(page))) + PAGE_SIZE);
	}

	return true;
}

/*
 * Pairs for allocate & release compression fifo DST buffers
 */
static void release_comp_fifo_dst_buffers(struct hwfifo *fifo)
{
	struct compress_cmd *cmdp;
	uint32_t entry;
	uint32_t *dst_addr;
	int bit = ENGINE_MAX_BUF_IDX;

	if (!fifo) {
		pr_info("%s: empty fifo.\n", __func__);
		return;
	}

	for (entry = 0; entry < fifo->size; entry++) {
		cmdp = COMP_CMD(fifo, entry);

		dst_addr = (uint32_t *)&cmdp->word_7_value;

		/* Free the corresponding buffer */
		free_entry_buffer(dst_addr, bit);
	}
}

static int fill_comp_fifo_dst_buffers(struct hwfifo *fifo, int id)
{
	struct compress_cmd *cmdp;
	uint32_t entry;
	uint32_t *dst_addr;
	int bit = ENGINE_MAX_BUF_IDX;

	if (!fifo)
		return -EINVAL;

	for (entry = 0; entry < fifo->size; entry++) {
		cmdp = COMP_CMD(fifo, entry);

		dst_addr = (uint32_t *)&cmdp->word_7_value;

		/* Fill DST buffer */
		if (refill_entry_buffer(dst_addr, bit, false)) {
			pr_info("%s: failed to fill dst buffer for (%u)(%d).\n", __func__, entry, bit);
			goto error;
		}

		/* Set invariant information */
		cmdp->buf_enable = ENGINE_BUF_ENABLE;
		cmdp->fifo = id;

		/* Copy cmdp word_0 to word_4 and word_6 */
		cmdp->word_4_value = cmdp->word_0_value;
		cmdp->word_6_value = cmdp->word_0_value;
	}

#ifdef ZRAM_ENGINE_DEBUG
	/* DEBUG */
	dump_all_comp_cmd(fifo);
#endif

	return 0;

error:
	/* Error occurs. Release all allocated buffers and return error status. */
	release_comp_fifo_dst_buffers(fifo);
	return -ENOMEM;
}

/* Engine operations for DST copy mode */
const struct engine_operations_struct engine_dc_ops = {
	.comp_process_completed_cmd	= comp_process_completed_cmd,
	.fill_compression_info		= fill_compression_info,
	.fill_comp_fifo_dst_buffers	= fill_comp_fifo_dst_buffers,
	.release_comp_fifo_dst_buffers	= release_comp_fifo_dst_buffers,
	.dcomp_process_completed_cmd	= __dcomp_process_completed_cmd,
	.fill_decompression_info	= fill_decompression_info,
	.fill_dcomp_fifo_src_buffers	= fill_dcomp_fifo_src_buffers,
	.release_dcomp_fifo_src_buffers	= release_dcomp_fifo_src_buffers,
};
