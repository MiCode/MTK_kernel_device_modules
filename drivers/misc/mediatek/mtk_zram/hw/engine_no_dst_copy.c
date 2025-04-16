// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#include <inc/fifo_no_dst_copy.h>
#include <inc/engine_ops.h>
#include <inc/helpers.h>
#include "../hwcomp_bridge.h"

/***************************************
 * Main implementation for No-DST Copy *
 ***************************************/

/* val: ENGINE_MIN_BUF_VAL ~ ENGINE_MAX_BUF_VAL */
#define buf_x_to_allocator_idx(val)	(ENGINE_MAX_BUF_VAL - val)

/*
 * Read compressed data from src, and store it to dst by comp_len.
 * It will be called under preemption and page faults disabled.
 */
int hwcomp_buf_read(void *dst, struct hwcomp_buf_t *src, unsigned int comp_len)
{
	uint64_t buf_x;
	uint64_t buf_x_val;
	int buf_selected = 0;
	uint32_t *src_addr;
	void *to_addr;
	void *from_addr = NULL;
	unsigned int remaining;
	unsigned int copy_size, prev_copy_size = ENGINE_MAX_BUF_SIZES + 1;
	int err = 0;
#ifdef ZRAM_ENGINE_DEBUG
	int index;
#endif

	/* Sanity check */
	if (dst == NULL || src == NULL || comp_len == 0) {
		pr_info("%s: dst:0x%llx src:0x%llx, comp_len:%u\n", __func__,
				(unsigned long long)dst, (unsigned long long)src, comp_len);
		return -EINVAL;
	}

	buf_x = (src->meta_info & BUF_X_START_POS_MASK) >> BUF_X_START_POS_SHIFT;
	buf_x_val = buf_x & BUF_X_BITS_MASK;
	src_addr = (uint32_t *)&src->buf_set_0;
	to_addr = dst;
	remaining = comp_len;

	/* buf_x is placed in order according to the size and stop at empty value (0x0) */
	while (buf_x_val >= ENGINE_MIN_BUF_VAL && buf_x_val <= ENGINE_MAX_BUF_VAL) {
		/* Calculate copy_size */
		copy_size = ENGINE_MAX_BUF_SIZES >> (ENGINE_MAX_BUF_VAL - buf_x_val);

		/* Don't exceed remaining */
		if (remaining < copy_size) {
#ifdef ZRAM_ENGINE_DEBUG
			pr_info("%s: copy_size:%u is larger than remaining:%u\n", __func__, copy_size, remaining);
#endif
			copy_size = remaining;
		}

		/* buf_x is placed in order according to the size */
		if (copy_size >= prev_copy_size) {
			pr_info("%s: invalid copy_size:%u, not smaller than:%u\n", __func__, copy_size, prev_copy_size);
			err = -1;
			goto exit;
		}

		/* src_addr should not be 0 */
		if (*src_addr == 0) {
			pr_info("%s: src_addr is 0\n", __func__);
			err = -2;
			goto exit;
		}

		/* Move compressed data */
		from_addr = cmd_buf_addr_to_va(src_addr++);

		/* Invalidate buffers for compressed data before memory copy (!!!) */
		invalidate_dcache((unsigned long)from_addr, (unsigned long)from_addr + copy_size);

		memcpy(to_addr, from_addr, copy_size);
		to_addr += copy_size;
		remaining -= copy_size;
		prev_copy_size = copy_size;

#ifdef ZRAM_ENGINE_DEBUG
		index = buf_x_to_allocator_idx(buf_x_val);
		pr_info("%s: index:%d from_addr:0x%llx\n", __func__, index, (unsigned long long)from_addr);
#endif
		/* Next buf_x */
		buf_x >>= BUF_X_BITS;
		buf_x_val = buf_x & BUF_X_BITS_MASK;

		/* Break if all selected buffers are found */
		if (++buf_selected == ENGINE_MAX_BUF_SELECTED)
			break;
	}

	/* remaining should be 0 here */
	if (remaining != 0) {
		pr_info("%s: remaining is not 0:%u\n", __func__, remaining);
		err = -3;
	}

exit:
	return err;
}
EXPORT_SYMBOL(hwcomp_buf_read);

/* Free corresponding buffers listed in hwcomp_buf_t */
void hwcomp_buf_destroy(struct hwcomp_buf_t *entry)
{
	uint64_t buf_x;
	uint64_t buf_x_val;
	int buf_selected = 0;
	uint32_t *src_addr;
	int index;
#ifdef ZRAM_ENGINE_DEBUG
	void *free_addr = NULL;
#endif

	/* Sanity check */
	if (entry == NULL) {
		pr_info("%s: entry:0x%llx\n", __func__, (unsigned long long)entry);
		return;
	}

#ifdef ZRAM_ENGINE_DEBUG
	dump_cmd(entry, sizeof(*entry));
#endif

	buf_x = (entry->meta_info & BUF_X_START_POS_MASK) >> BUF_X_START_POS_SHIFT;
	buf_x_val = buf_x & BUF_X_BITS_MASK;
	src_addr = (uint32_t *)&entry->buf_set_0;

#ifdef ZRAM_ENGINE_DEBUG
	pr_info("%s: (%lx)(%lx)(%lx)\n",
		__func__, (unsigned long)buf_x, (unsigned long)buf_x_val, (unsigned long)src_addr);
#endif

	/* buf_x is placed in order according to the size and stop at empty value (0x0) */
	while (buf_x_val >= ENGINE_MIN_BUF_VAL && buf_x_val <= ENGINE_MAX_BUF_VAL) {

		/* Release buf_x: tag is KASAN_TAG_KERNEL(0xff), so no tag match will be executed. */
		index = buf_x_to_allocator_idx(buf_x_val);
		free_entry_buffer(src_addr, index);

#ifdef ZRAM_ENGINE_DEBUG
		/* VA to be freed */
		free_addr = cmd_buf_addr_to_va(src_addr);
		pr_info("%s: index:%d from_addr:0x%llx\n", __func__, index, (unsigned long long)free_addr);
#endif

		/* Next buf_x */
		src_addr++;
		buf_x >>= BUF_X_BITS;
		buf_x_val = buf_x & BUF_X_BITS_MASK;

		/* Break if all selected buffers are found */
		if (++buf_selected == ENGINE_MAX_BUF_SELECTED)
			break;
	}
}
EXPORT_SYMBOL(hwcomp_buf_destroy);

/*
 * Post-process one decompression cmd
 */
static void __dcomp_process_completed_cmd(struct hwfifo *fifo, uint32_t entry, bool async)
{
	struct decompress_cmd *cmdp = DCOMP_CMD(fifo, entry);
	struct dcomp_pp_info *pp_info = DCOMP_CMPL(fifo, entry);
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
		/* It's unexpected to be here. dump registers... (TODO) */
		pr_info("%s: unexpected decompressed status (0x%x).\n", __func__, dcomp_status);
		err = -EIO;
		break;
	}

#ifdef MORE_DEBUG
	if (err && async) {
		// CHECK DEC ERR
		if (simulate_ndc_decompress(fifo, entry)) {
			pr_info("(HWZRAM)(HW) %s: decompress fail: (entry) = (0x%x) for index(%u)\n",
				__func__, entry, pp_info->index);
		} else {
			pr_info("(HWZRAM)(HW) %s: decompress success: (entry) = (0x%x) for index(%u)\n",
				__func__, entry, pp_info->index);
			err = 0;
		}
	}
#endif

	/* Callback for remaining works */
	if (async)
		hwcomp_decompress_post_process(err, pp_info);

	/* Post-process is finished. */
	reset_cmd_after_decompression(cmdp);
}

/*
 * Routine for post-processing compression cmd with COMP_CMD_COMPRESSED state
 */
static void process_cmd_after_compression(struct compress_cmd *cmdp, struct hwcomp_buf_t *compressed, int *err)
{
	unsigned int bufsel;
	uint32_t *dst_addr, *store_dst_addr;
	int bit;
	uint64_t buf_x = 0x0;
	int buf_x_shift = BUF_X_START_POS_SHIFT;

	/* default status */
	*err = 0;

	/* Initialize the iterating */
	bufsel = cmdp->bufsel;
	dst_addr = (uint32_t *)&cmdp->word_4_value;
	store_dst_addr = (uint32_t *)&compressed->buf_set_0;

	/* Iterating DST buffers from the biggest to the smallest */
	for (bit = ENGINE_MAX_BUF_IDX; bit <= ENGINE_MIN_BUF_IDX; bit++) {

		/* Check bufsel bitwisely & compose buf_x */
		if (bufsel & (0x1 << bit)) {

			/* No need to invalidate $ for compressed data (No coherence both for ENC DST & DST SRC) */

			/* Store dst addr */
			*store_dst_addr = *dst_addr & BUF_ADDR_BITS_MASK;
			store_dst_addr++;

			/*
			 * Clear dst_addr to 0 after copying it to compressed
			 * to align the initial setting at fifo buffer creation.
			 */
			*dst_addr = 0;

			/* Mark its corresponding size */
			buf_x = buf_x | ((uint64_t)((int)ENGINE_MAX_BUF_VAL - bit) << buf_x_shift);
			buf_x_shift += BUF_X_BITS;

			/* Refill DST buffer */
			if (refill_entry_buffer(dst_addr, bit, true)) {
				pr_info("%s: failed to refill dst buffer for (%d).\n", __func__, bit);
				*err = -1;
			}
		}

		/* Move to the position of next DST buffer */
		dst_addr++;
	}

	compressed->meta_info = (buf_x & BUF_X_START_POS_MASK) | cmdp->hash_result;

#ifdef ZRAM_ENGINE_DEBUG
	/* DEBUG */
	dump_comp_cmd(cmdp);
#endif
}

/*
 * Helper function to allocate dst buffers for compression cmd -
 * err is set to 0 when the function runs successfully.
 */
static void refill_comp_dst_buffers(struct compress_cmd *cmdp, int *err)
{
	uint32_t *dst_addr;
	int bit;

	/* default status */
	*err = 0;

	/* Initializing & iterating for filling buffers */
	dst_addr = (uint32_t *)&cmdp->word_4_value;
	for (bit = ENGINE_MAX_BUF_IDX; bit <= ENGINE_MIN_BUF_IDX; bit++) {

		/* Fill DST buffer if necessary */
		if ((*dst_addr == 0) && refill_entry_buffer(dst_addr, bit, true)) {
			pr_info("%s: failed to refill dst buffer for (%d).\n", __func__, bit);
			*err = -1;
		}

		/* Move to the position of next DST buffer */
		dst_addr++;
	}
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
	struct hwcomp_buf_t compressed_info;	/* in-stack cookie */
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
		process_cmd_after_compression(cmdp, &compressed_info, &pp_err);
		buffer = &compressed_info;
		/* Update post-process info */
		flag = HWCOMP_NORMAL;
		/* Update comp_size to actual compressed size */
		comp_size = cmdp->compr_size;
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

	/*
	 * Callback for remaining works -
	 * No memory copy from dst buffers for compressed data (!!!)
	 */
	hwcomp_compress_post_process_ndc(err, buffer, comp_size, pp_info, flag);

	/* Post-process is finished */

	/*
	 * If the comp_status is COMP_CMD_IDLE, it means this cmd
	 * may not have sufficient dst buffers and HW bypasses it.
	 * Give it one more chance to allocate sufficient dst ones.
	 */
	if (comp_status == COMP_CMD_IDLE)
		refill_comp_dst_buffers(cmdp, &pp_err);

	/* Reset the cmd according to pp_err */
	if (!pp_err)
		reset_cmd_after_compression(cmdp, fifo->id);
	else
		reset_cmd_after_compression_pp_err(cmdp);

#ifdef ZRAM_ENGINE_DEBUG
	/* DEBUG */
	dump_comp_cmd(cmdp);
#endif
}

/*
 * Prepare for the decompression of one page (called by hwcomp_decompress_page).
 * "dcomp_pp_info *from" is unnecessary when async is false.
 */
static bool fill_decompression_info(struct hwfifo *fifo, uint32_t entry,
		void *__src, unsigned int slen, struct page *page,
		struct dcomp_pp_info *from, bool async,
		zspool_to_hwcomp_buffer_fn zspool_to_hwcomp_buffer)
{
	struct hwcomp_buf_t *src = (struct hwcomp_buf_t *)__src;
	struct decompress_cmd *cmdp = DCOMP_CMD(fifo, entry);
	struct dcomp_pp_info *pp_info = DCOMP_CMPL(fifo, entry);
	uint32_t hash_value = 0;

	/* Set cmd as DCOMP_CMD_IDLE. If it's invalid, and then bypass it. */
	if (dcomp_cmd_check_invalid(cmdp))
		return false;

	/* Setup src buffers for decompression */
	cmdp->word_0_value = (src->meta_info & BUF_X_START_POS_MASK) | (cmdp->word_0_value & ~BUF_X_START_POS_MASK);
	cmdp->word_2_value = src->buf_set_0;
	cmdp->word_3_value = src->buf_set_1;
	hash_value = src->hash_value;

	/* Initialize decompress cmd (Set hash_verify to true (?)) */
	update_cmd_before_decompression(cmdp, page, slen, hash_value, false, false, async);

	/* Decompression info is initialized. */
	if (async) {
		memcpy(pp_info, from, sizeof(struct dcomp_pp_info));

		/* Increase the reference count for async decompression */
		refcount_inc_for_async_dcomp(from);
	}

	/* Invalidate dcache for fifo cmd & dst page */
	if (engine_coherence_disabled()) {
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
	/* Do nothing */
}

static int fill_dcomp_fifo_src_buffers(struct hwfifo *fifo, int id)
{
	struct decompress_cmd *cmdp;
	uint32_t entry;

	if (!fifo)
		return -EINVAL;

	for (entry = 0; entry < fifo->size; entry++) {
		cmdp = DCOMP_CMD(fifo, entry);

		cmdp->fifo = id;
	}

#ifdef ZRAM_ENGINE_DEBUG
	/* DEBUG */
	dump_all_dcomp_cmd(fifo);
#endif

	return 0;
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

	/* Flush dcache for fifo cmd & src page */
	if (engine_coherence_disabled()) {
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
	int bit;

	if (!fifo) {
		pr_info("%s: empty fifo.\n", __func__);
		return;
	}

	for (entry = 0; entry < fifo->size; entry++) {
		cmdp = COMP_CMD(fifo, entry);

		dst_addr = (uint32_t *)&cmdp->word_4_value;
		for (bit = ENGINE_MAX_BUF_IDX; bit <= ENGINE_MIN_BUF_IDX; bit++) {
			/* Free the corresponding buffer */
			free_entry_buffer(dst_addr, bit);
			/* Move to the position of next DST buffer */
			dst_addr++;
		}
	}
}

static int fill_comp_fifo_dst_buffers(struct hwfifo *fifo, int id)
{
	struct compress_cmd *cmdp;
	uint32_t entry;
	int err;

	if (!fifo)
		return -EINVAL;

	for (entry = 0; entry < fifo->size; entry++) {
		cmdp = COMP_CMD(fifo, entry);
		err = 0;
		refill_comp_dst_buffers(cmdp, &err);
		if (err) {
			pr_info("%s: failed to fill dst buffer for (%u).\n", __func__, entry);
			goto error;
		}

		/* Set invariant information */
		cmdp->buf_enable = ENGINE_BUF_ENABLE;
		cmdp->fifo = id;
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

/* Engine operations for No-DST copy mode */
const struct engine_operations_struct engine_ndc_ops = {
	.comp_process_completed_cmd	= comp_process_completed_cmd,
	.fill_compression_info		= fill_compression_info,
	.fill_comp_fifo_dst_buffers	= fill_comp_fifo_dst_buffers,
	.release_comp_fifo_dst_buffers	= release_comp_fifo_dst_buffers,
	.dcomp_process_completed_cmd	= __dcomp_process_completed_cmd,
	.fill_decompression_info	= fill_decompression_info,
	.fill_dcomp_fifo_src_buffers	= fill_dcomp_fifo_src_buffers,
	.release_dcomp_fifo_src_buffers	= release_dcomp_fifo_src_buffers,
};
