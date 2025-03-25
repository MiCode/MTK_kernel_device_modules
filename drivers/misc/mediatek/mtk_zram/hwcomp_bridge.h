/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef _HWCOMP_BRIDGE_H_
#define _HWCOMP_BRIDGE_H_

#include <linux/bio.h>
#include <hwcomp_bridge_type.h>

/* Include declarations from other files */
struct zram;
struct bio;

/**********************************************************
 * Necessary information for HW compression/decompression *
 **********************************************************/

enum hwcomp_flags {
	HWCOMP_INVAL = 0,
	HWCOMP_SAME,	/* Similar to ZRAM_SAME */
	HWCOMP_HUGE,	/* Similar to ZRAM_HUGE */
	HWCOMP_NORMAL,
};

struct comp_pp_info {
	struct zram *zram;
	u32 index;
	union {
		struct page *page;		/* Initialized to src_page and valid for "flag != HWCOMP_SAME"
						   after HW compression */
		unsigned long repeat_pattern;	/* Valid for "flag == HWCOMP_SAME" */
	};
	struct bio *bio;
};

struct dcomp_pp_info {
	struct zram *zram;
	u32 index;
	struct page *page;			/* dst page */
	struct bio *bio;
};

#if IS_ENABLED(CONFIG_HWCOMP_SUPPORT_NO_DST_COPY)
/* Structure for HW compression w/o SW copy */
struct hwcomp_buf_t {
	uint64_t buf_set_0;
	uint64_t buf_set_1;
	union {
		struct {
			uint64_t hash_value:32;
			uint64_t :16;
			uint64_t buf_a:4;	/* Align *** Decompression FIFO CMD Format *** in engine_fifo.h */
			uint64_t buf_b:4;
			uint64_t buf_c:4;
			uint64_t buf_d:4;
		};
		uint64_t meta_info;
	};
};
#endif

/* Allocated for each disk page (note: move it to here because its layout will change according to configurations) */
struct zram_table_entry {
	unsigned long flags;
#ifdef CONFIG_ZRAM_TRACK_ENTRY_ACTIME
	ktime_t ac_time;
#endif
	union {
		unsigned long handle;
		unsigned long element;
	};
};

#if IS_ENABLED(CONFIG_HWCOMP_SUPPORT_NO_DST_COPY)
struct zram_table_entry_ndc {
	unsigned long flags;
#ifdef CONFIG_ZRAM_TRACK_ENTRY_ACTIME
	ktime_t ac_time;
#endif
	union {
		unsigned long handle;
		unsigned long element;
		/*
		 * HW compression w/o SW copy -
		 * should be placed in the same union with handle and at the end of this structure.
		 */
		struct hwcomp_buf_t compressed;
	};
};
#endif

/* Increase the reference count by 1 for async operations (should be called before post-process starts) */
static inline void refcount_inc_for_comp(struct comp_pp_info *obj)
{
	/* Increase the remaining count to bio for compression */
	bio_inc_remaining(obj->bio);
}

static inline void refcount_inc_for_async_dcomp(struct dcomp_pp_info *obj)
{
	/* Increase the remaining count to bio for async decompression */
	bio_inc_remaining(obj->bio);
}

/*
 * HW-impl APIs - create & destroy an entity to HW engine
 */

#define NO_DST_COPY_MODE	(0x0)
#define DST_COPY_MODE		(0x1)

void *hwcomp_create(int, compress_pp_fn, decompress_pp_fn);
void hwcomp_destroy(void *);

/*
 * HW-impl APIs - compression & decompression entries
 */
int hwcomp_compress_page(void *hw, struct page *src_page, struct comp_pp_info *pp_info);
int hwcomp_decompress_page(void *hw, void *src, unsigned int slen, struct page *dst_page,
		struct dcomp_pp_info *pp_info, zspool_to_hwcomp_buffer_fn zspool_to_hwcomp_buffer);
int hwcomp_decompress_page_sync(void *hw, void *src, unsigned int slen, struct page *dst_page,
		struct dcomp_pp_info *pp_info, zspool_to_hwcomp_buffer_fn zspool_to_hwcomp_buffer);

/*
 * HW-impl APIs - special operations for NO_DST_COPY mode
 */
#if IS_ENABLED(CONFIG_HWCOMP_SUPPORT_NO_DST_COPY)
int hwcomp_buf_read(void *dst, struct hwcomp_buf_t *src, unsigned int comp_len);
void hwcomp_buf_destroy(struct hwcomp_buf_t *entry);
#endif

#include <linux/swap.h>
#include <linux/delay.h>

#define WAIT_FOR_HWCOMP()			\
	do {					\
		usleep_range(100, 200);		\
	} while (0)				\

#define WAIT_FOR_HWDCOMP()			\
	do {					\
		usleep_range(10, 50);		\
	} while (0)				\

#include "mtk_zram_drv.h"

static inline struct zram_table_entry *get_zram_table_entry(struct zram *zram, u32 index)
{
	return zram->table + zram->ops->table_entry_sz * index;
}

#define ZRAM_TE(zram, index)		(get_zram_table_entry(zram, index))
#define ZRAM_TE_NDC(zram, index)	((struct zram_table_entry_ndc *)(get_zram_table_entry(zram, index)))

#endif /* _HWCOMP_BRIDGE_H_ */
