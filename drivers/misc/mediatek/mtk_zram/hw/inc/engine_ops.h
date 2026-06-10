/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef _ENGINE_OPS_H_
#define _ENGINE_OPS_H_

#include <inc/engine_fifo.h>
#include <hwcomp_bridge_type.h>

struct engine_operations_struct {

	/* Post-process one compression cmd */
	void (*comp_process_completed_cmd)(struct hwfifo *fifo, uint32_t entry, bool silence);

	/* Prepare CMD for the compression of one page */
	bool (*fill_compression_info)(struct hwfifo *fifo, uint32_t entry,
				struct page *page, struct comp_pp_info *from, bool ref_inc);

	/* Pairs for allocate & release compression fifo DST buffers*/
	int (*fill_comp_fifo_dst_buffers)(struct hwfifo *fifo, int id);
	void (*release_comp_fifo_dst_buffers)(struct hwfifo *fifo);

	/* Post-process one decompression cmd */
	void (*dcomp_process_completed_cmd)(struct hwfifo *fifo, uint32_t entry, bool async);

	/* Prepare CMD for the decompression of one page (called by hwcomp_decompress_page) */
	bool (*fill_decompression_info)(struct hwfifo *fifo, uint32_t entry,
				void *src, unsigned int slen, struct page *page,
				struct dcomp_pp_info *from, bool async,
				zspool_to_hwcomp_buffer_fn zspool_to_hwcomp_buffer);

	/* Pairs for allocate & release decompression fifo SRC buffers */
	int (*fill_dcomp_fifo_src_buffers)(struct hwfifo *fifo, int id);
	void (*release_dcomp_fifo_src_buffers)(struct hwfifo *fifo);
};

extern const struct engine_operations_struct engine_dc_ops;
#if IS_ENABLED(CONFIG_HWCOMP_SUPPORT_NO_DST_COPY)
extern const struct engine_operations_struct engine_ndc_ops;
#endif

/* Post-process callback for compression */
extern compress_pp_fn hwcomp_compress_post_process_dc;
#if IS_ENABLED(CONFIG_HWCOMP_SUPPORT_NO_DST_COPY)
extern compress_pp_fn hwcomp_compress_post_process_ndc;
#endif

/* Post-process callback for decompression */
extern decompress_pp_fn hwcomp_decompress_post_process;

#endif /* _ENGINE_OPS_H_ */
