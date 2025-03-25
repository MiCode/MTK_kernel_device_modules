/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */
/*
 * Compressed RAM block device
 *
 * Copyright (C) 2008, 2009, 2010  Nitin Gupta
 *               2012, 2013 Minchan Kim
 *
 * This code is released using a dual license strategy: BSD/GPL
 * You can choose the licence that better fits your requirements.
 *
 * Released under the terms of 3-clause BSD License
 * Released under the terms of GNU General Public License Version 2.0
 *
 */

#ifndef _MTK_ZRAM_DRV_H_
#define _MTK_ZRAM_DRV_H_

#include <linux/rwsem.h>
#include <linux/zsmalloc.h>
#include <linux/crypto.h>

#include "zcomp.h"

#define SECTORS_PER_PAGE_SHIFT	(PAGE_SHIFT - SECTOR_SHIFT)
#define SECTORS_PER_PAGE	(1 << SECTORS_PER_PAGE_SHIFT)
#define ZRAM_LOGICAL_BLOCK_SHIFT 12
#define ZRAM_LOGICAL_BLOCK_SIZE	(1 << ZRAM_LOGICAL_BLOCK_SHIFT)
#define ZRAM_SECTOR_PER_LOGICAL_BLOCK	\
	(1 << (ZRAM_LOGICAL_BLOCK_SHIFT - SECTOR_SHIFT))


/*
 * ZRAM is mainly used for memory efficiency so we want to keep memory
 * footprint small and thus squeeze size and zram pageflags into a flags
 * member. The lower ZRAM_FLAG_SHIFT bits is for object size (excluding
 * header), which cannot be larger than PAGE_SIZE (requiring PAGE_SHIFT
 * bits), the higher bits are for zram_pageflags.
 *
 * We use BUILD_BUG_ON() to make sure that zram pageflags don't overflow.
 */
#define ZRAM_FLAG_SHIFT (PAGE_SHIFT + 1)

/* Only 2 bits are allowed for comp priority index */
#define ZRAM_COMP_PRIORITY_MASK	0x3

/* Flags for zram pages (table[page_no].flags) */
enum zram_pageflags {
	/* zram slot is locked */
	ZRAM_LOCK = ZRAM_FLAG_SHIFT,
	ZRAM_SAME,	/* Page consists the same element */
	ZRAM_WB,	/* page is stored on backing_device */
	ZRAM_UNDER_WB,	/* page is under writeback */
	ZRAM_HUGE,	/* Incompressible page */
	ZRAM_IDLE,	/* not accessed page since last idle marking */
	ZRAM_INCOMPRESSIBLE, /* none of the algorithms could compress it */

	ZRAM_COMP_PRIORITY_BIT1, /* First bit of comp priority index */
	ZRAM_COMP_PRIORITY_BIT2, /* Second bit of comp priority index */

	/*
	 * This is used to stand for HWCOMP_NORMAL with compressible size.
	 * No dst copy: Table entries involve hwcomp_buf_t.
	 * w/ dst copy: HW compressed. (Not HUGE or SAME)
	 */
	ZRAM_HW_COMPRESS,

	ZRAM_IN_HW_PROCESSING,	/* Under HW processing(especially for HW decompression).
				   Never allow SW flow to touch it. */

	__NR_ZRAM_PAGEFLAGS,
};

/*-- Data structures */

#define ZRAM_MAX_COMP_MODE_NAME	(32)

/* Support variant operation modes */
struct zram_mode_operations {
	void (*bio_read)(struct zram *zram, struct bio *bio);
	int (*hw_bvec_read)(struct zram *zram, struct bio_vec *bvec,	/* Should not be NULL
									   if HW operation is possible */
			u32 index, int offset, struct bio *bio, bool wait);
	void (*bio_write)(struct zram *zram, struct bio *bio);
	int (*hw_bvec_write)(struct zram *zram, struct bio_vec *bvec,	/* Should not be NULL
									   if HW operation is possible */
			u32 index, int offset, struct bio *bio, bool wait);
	/* Convert hwcomp_buf_t to zspool buffer and release hwcomp_buf_t one(s). */
	int (*to_zspool)(struct zram *zram, u32 index);
	/* Called under bit_spin_lock on zram slot to release corresponding buffer. */
	void (*free_entry)(struct zram *zram, size_t index);
	void (*wait_for_hw)(struct zram *zram);

	/* Used to translate an index to get the table entry */
	size_t table_entry_sz;
	int (*hwinit)(struct zram *zram);
	void (*hwfini)(struct zram *zram);

	/* If fallback to SW is possible, then set it as true */
	bool default_swalg;
	const char *name;
};

struct zram_stats {
	atomic64_t compr_data_size;	/* compressed size of pages stored */
	atomic64_t failed_reads;	/* can happen when memory is too low */
	atomic64_t failed_writes;	/* can happen when memory is too low */
	atomic64_t notify_free;	/* no. of swap slot free notifications */
	atomic64_t same_pages;		/* no. of same element filled pages */
	atomic64_t huge_pages;		/* no. of huge pages */
	atomic64_t huge_pages_since;	/* no. of huge pages since zram set up */
	atomic64_t pages_stored;	/* no. of pages currently stored */
	atomic_long_t max_used_pages;	/* no. of maximum pages stored */
	atomic64_t writestall;		/* no. of write slow paths */
	atomic64_t miss_free;		/* no. of missed free */
#ifdef	CONFIG_ZRAM_WRITEBACK
	atomic64_t bd_count;		/* no. of pages in backing device */
	atomic64_t bd_reads;		/* no. of reads from backing device */
	atomic64_t bd_writes;		/* no. of writes from backing device */
#endif
	atomic64_t hw_failed_reads;	/* Error occurs when using HW for reads */
	atomic64_t hw_failed_writes;	/* Error occurs when using HW for writes */
	atomic64_t hw_inuse;		/* no. of requests using HW (now only for read) */
	atomic64_t hw_busy;		/* HW compression is busy (may be FIFO full) */
	atomic64_t hw_busy_wait;	/* HW compression is busy and wait for available */
	atomic64_t hw_dec_busy;		/* HW decompression is busy (may be FIFO full) */
	atomic64_t hw_dec_busy_wait;	/* HW decompression is busy and wait for available */
	atomic64_t hw_huge_pages_since;	/* no. of huge pages (from hw view) since zram set up */
};

#ifdef CONFIG_ZRAM_MULTI_COMP
#define ZRAM_PRIMARY_COMP	0U
#define ZRAM_SECONDARY_COMP	1U
#define ZRAM_MAX_COMPS	4U
#else
#define ZRAM_PRIMARY_COMP	0U
#define ZRAM_SECONDARY_COMP	0U
#define ZRAM_MAX_COMPS	1U
#endif

struct zram {
	void *table;
	struct zs_pool *mem_pool;
	struct zcomp *comps[ZRAM_MAX_COMPS];
	struct gendisk *disk;

	/* For HW compression engine or other underlying implementations */
	void *priv;

	/* zram_mode_xxx hooks */
	const struct zram_mode_operations *ops;

	/* Prevent concurrent execution of device init */
	struct rw_semaphore init_lock;
	/*
	 * the number of pages zram can consume for storing compressed data
	 */
	unsigned long limit_pages;

	struct zram_stats stats;
	/*
	 * This is the limit on amount of *uncompressed* worth of data
	 * we can store in a disk.
	 */
	u64 disksize;	/* bytes */
	const char *comp_algs[ZRAM_MAX_COMPS];
	s8 num_active_comps;
	/*
	 * zram is claimed so open request will be failed
	 */
	bool claim; /* Protected by disk->open_mutex */
#ifdef CONFIG_ZRAM_WRITEBACK
	struct file *backing_dev;
	spinlock_t wb_limit_lock;
	bool wb_limit_enable;
	u64 bd_wb_limit;
	struct block_device *bdev;
	unsigned long *bitmap;
	unsigned long nr_pages;
#endif
#ifdef CONFIG_ZRAM_MEMORY_TRACKING
	struct dentry *debugfs_dir;
#endif
	wait_queue_head_t hw_wait;
};

#endif
