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

#define KMSG_COMPONENT "zram"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bio.h>
#include <linux/bitops.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/device.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/backing-dev.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/err.h>
#include <linux/idr.h>
#include <linux/sysfs.h>
#include <linux/debugfs.h>
#include <linux/cpuhotplug.h>
#include <linux/part_stat.h>
#include <linux/kcompressd.h>
#include <linux/tracepoint.h>

#include "hwcomp_bridge.h"

#include <trace/hooks/mm.h>

static DEFINE_IDR(zram_index_idr);
/* idr index must be protected */
static DEFINE_MUTEX(zram_index_mutex);

static int zram_major;
static const char *default_compressor = "lz4";
#if PAGE_SIZE == 4096
static const char *default_mode = "hwonly";
#else
static const char *default_mode = "swonly";
#endif

static DEFINE_STATIC_KEY_FALSE(kcompressd_enabled);
static inline bool is_kcompressd_enabled(void)
{
	return static_branch_unlikely(&kcompressd_enabled);
}

/* Set a value to stand for fifo depth */
#define HWZRAM_IS_BUSY	(1 << CONFIG_ZRAM_ENGINE_COMP_FIFO_BITS)
static atomic_t hwzram_busy = ATOMIC_INIT(0);
static inline bool is_hwzram_busy(void)
{
	return (atomic_read(&hwzram_busy) != 0);
}

static inline void mark_hwzram_busy(void)
{
	if(is_kcompressd_enabled())
		atomic_set(&hwzram_busy, HWZRAM_IS_BUSY);
}

static inline void mark_hwzram_not_busy(void)
{
	if(is_kcompressd_enabled())
		atomic_dec_if_positive(&hwzram_busy);
}

/* Module params (documentation at end) */
static unsigned int num_devices = 1;
/*
 * Pages that compress to sizes equals or greater than this are stored
 * uncompressed in memory.
 */
static size_t huge_class_size;

static const struct block_device_operations zram_devops;

static void zram_free_page(struct zram *zram, size_t index);
static int zram_read_page(struct zram *zram, struct page *page, u32 index,
			  struct bio *parent);

static int zram_slot_trylock(struct zram *zram, u32 index)
{
	return bit_spin_trylock(ZRAM_LOCK, &(ZRAM_TE(zram, index))->flags);
}

static void zram_slot_lock(struct zram *zram, u32 index)
{
	bit_spin_lock(ZRAM_LOCK, &(ZRAM_TE(zram, index))->flags);
}

static void zram_slot_unlock(struct zram *zram, u32 index)
{
	bit_spin_unlock(ZRAM_LOCK, &(ZRAM_TE(zram, index))->flags);
}

static inline bool init_done(struct zram *zram)
{
	return zram->disksize;
}

static inline struct zram *dev_to_zram(struct device *dev)
{
	return (struct zram *)dev_to_disk(dev)->private_data;
}

static unsigned long zram_get_handle(struct zram *zram, u32 index)
{
	return (ZRAM_TE(zram, index))->handle;
}

static void zram_set_handle(struct zram *zram, u32 index, unsigned long handle)
{
	(ZRAM_TE(zram, index))->handle = handle;
}

/* flag operations require table entry bit_spin_lock() being held */
static bool zram_test_flag(struct zram *zram, u32 index,
			enum zram_pageflags flag)
{
	return (ZRAM_TE(zram, index))->flags & BIT(flag);
}

static void zram_set_flag(struct zram *zram, u32 index,
			enum zram_pageflags flag)
{
	(ZRAM_TE(zram, index))->flags |= BIT(flag);
}

static void zram_clear_flag(struct zram *zram, u32 index,
			enum zram_pageflags flag)
{
	(ZRAM_TE(zram, index))->flags &= ~BIT(flag);
}

static inline void zram_set_element(struct zram *zram, u32 index,
			unsigned long element)
{
	(ZRAM_TE(zram, index))->element = element;
}

static unsigned long zram_get_element(struct zram *zram, u32 index)
{
	return (ZRAM_TE(zram, index))->element;
}

static size_t zram_get_obj_size(struct zram *zram, u32 index)
{
	return (ZRAM_TE(zram, index))->flags & (BIT(ZRAM_FLAG_SHIFT) - 1);
}

static void zram_set_obj_size(struct zram *zram,
					u32 index, size_t size)
{
	unsigned long flags = (ZRAM_TE(zram, index))->flags >> ZRAM_FLAG_SHIFT;

	(ZRAM_TE(zram, index))->flags = (flags << ZRAM_FLAG_SHIFT) | size;
}

static inline bool zram_allocated(struct zram *zram, u32 index)
{
	return zram_get_obj_size(zram, index) ||
			zram_test_flag(zram, index, ZRAM_SAME) ||
			zram_test_flag(zram, index, ZRAM_WB);
}

static inline bool zram_hw_compress(struct zram *zram, u32 index)
{
	return zram_test_flag(zram, index, ZRAM_HW_COMPRESS);
}

static inline bool zram_in_hw_processing(struct zram *zram, u32 index)
{
	return zram_test_flag(zram, index, ZRAM_IN_HW_PROCESSING);
}

#if PAGE_SIZE != 4096
static inline bool is_partial_io(struct bio_vec *bvec)
{
	return bvec->bv_len != PAGE_SIZE;
}
#define ZRAM_PARTIAL_IO		1
#else
static inline bool is_partial_io(struct bio_vec *bvec)
{
	return false;
}
#endif

static inline void zram_set_priority(struct zram *zram, u32 index, u32 prio)
{
	prio &= ZRAM_COMP_PRIORITY_MASK;
	/*
	 * Clear previous priority value first, in case if we recompress
	 * further an already recompressed page
	 */
	(ZRAM_TE(zram, index))->flags &= ~(ZRAM_COMP_PRIORITY_MASK <<
				      ZRAM_COMP_PRIORITY_BIT1);
	(ZRAM_TE(zram, index))->flags |= (prio << ZRAM_COMP_PRIORITY_BIT1);
}

static inline u32 zram_get_priority(struct zram *zram, u32 index)
{
	u32 prio = (ZRAM_TE(zram, index))->flags >> ZRAM_COMP_PRIORITY_BIT1;

	return prio & ZRAM_COMP_PRIORITY_MASK;
}

static void zram_accessed(struct zram *zram, u32 index)
{
	zram_clear_flag(zram, index, ZRAM_IDLE);
#ifdef CONFIG_ZRAM_TRACK_ENTRY_ACTIME
	(ZRAM_TE(zram, index))->ac_time = ktime_get_boottime();
#endif
}

static inline void update_used_max(struct zram *zram,
					const unsigned long pages)
{
	unsigned long cur_max = atomic_long_read(&zram->stats.max_used_pages);

	do {
		if (cur_max >= pages)
			return;
	} while (!atomic_long_try_cmpxchg(&zram->stats.max_used_pages,
					  &cur_max, pages));
}

static inline void zram_fill_page(void *ptr, unsigned long len,
					unsigned long value)
{
	WARN_ON_ONCE(!IS_ALIGNED(len, sizeof(unsigned long)));
	memset_l(ptr, value, len / sizeof(unsigned long));
}

static bool page_same_filled(void *ptr, unsigned long *element)
{
	unsigned long *page;
	unsigned long val;
	unsigned int pos, last_pos = PAGE_SIZE / sizeof(*page) - 1;

	page = (unsigned long *)ptr;
	val = page[0];

	if (val != page[last_pos])
		return false;

	for (pos = 1; pos < last_pos; pos++) {
		if (val != page[pos])
			return false;
	}

	*element = val;

	return true;
}

static ssize_t initstate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u32 val;
	struct zram *zram = dev_to_zram(dev);

	down_read(&zram->init_lock);
	val = init_done(zram);
	up_read(&zram->init_lock);

	return scnprintf(buf, PAGE_SIZE, "%u\n", val);
}

static ssize_t disksize_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zram *zram = dev_to_zram(dev);

	return scnprintf(buf, PAGE_SIZE, "%llu\n", zram->disksize);
}

static ssize_t mem_limit_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	u64 limit;
	char *tmp;
	struct zram *zram = dev_to_zram(dev);

	limit = memparse(buf, &tmp);
	if (buf == tmp) /* no chars parsed, invalid input */
		return -EINVAL;

	down_write(&zram->init_lock);
	zram->limit_pages = PAGE_ALIGN(limit) >> PAGE_SHIFT;
	up_write(&zram->init_lock);

	return len;
}

static ssize_t mem_used_max_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int err;
	unsigned long val;
	struct zram *zram = dev_to_zram(dev);

	err = kstrtoul(buf, 10, &val);
	if (err || val != 0)
		return -EINVAL;

	down_read(&zram->init_lock);
	if (init_done(zram)) {
		atomic_long_set(&zram->stats.max_used_pages,
				zs_get_total_pages(zram->mem_pool));
	}
	up_read(&zram->init_lock);

	return len;
}

/*
 * Mark all pages which are older than or equal to cutoff as IDLE.
 * Callers should hold the zram init lock in read mode
 */
static void mark_idle(struct zram *zram, ktime_t cutoff)
{
	int is_idle = 1;
	unsigned long nr_pages = zram->disksize >> PAGE_SHIFT;
	int index;

	for (index = 0; index < nr_pages; index++) {
		/*
		 * Do not mark ZRAM_UNDER_WB slot as ZRAM_IDLE to close race.
		 * See the comment in writeback_store.
		 */
		zram_slot_lock(zram, index);
		if (zram_allocated(zram, index) &&
				!zram_test_flag(zram, index, ZRAM_UNDER_WB)) {
#ifdef CONFIG_ZRAM_TRACK_ENTRY_ACTIME
			is_idle = !cutoff || ktime_after(cutoff,
							 (ZRAM_TE(zram, index))->ac_time);
#endif
			if (is_idle)
				zram_set_flag(zram, index, ZRAM_IDLE);
		}
		zram_slot_unlock(zram, index);
	}
}

static ssize_t idle_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct zram *zram = dev_to_zram(dev);
	ktime_t cutoff_time = 0;
	ssize_t rv = -EINVAL;

	if (!sysfs_streq(buf, "all")) {
		/*
		 * If it did not parse as 'all' try to treat it as an integer
		 * when we have memory tracking enabled.
		 */
		u64 age_sec;

		if (IS_ENABLED(CONFIG_ZRAM_TRACK_ENTRY_ACTIME) && !kstrtoull(buf, 0, &age_sec))
			cutoff_time = ktime_sub(ktime_get_boottime(),
					ns_to_ktime(age_sec * NSEC_PER_SEC));
		else
			goto out;
	}

	down_read(&zram->init_lock);
	if (!init_done(zram))
		goto out_unlock;

	/*
	 * A cutoff_time of 0 marks everything as idle, this is the
	 * "all" behavior.
	 */
	mark_idle(zram, cutoff_time);
	rv = len;

out_unlock:
	up_read(&zram->init_lock);
out:
	return rv;
}

#ifdef CONFIG_ZRAM_WRITEBACK
static ssize_t writeback_limit_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct zram *zram = dev_to_zram(dev);
	u64 val;
	ssize_t ret = -EINVAL;

	if (kstrtoull(buf, 10, &val))
		return ret;

	down_read(&zram->init_lock);
	spin_lock(&zram->wb_limit_lock);
	zram->wb_limit_enable = val;
	spin_unlock(&zram->wb_limit_lock);
	up_read(&zram->init_lock);
	ret = len;

	return ret;
}

static ssize_t writeback_limit_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	bool val;
	struct zram *zram = dev_to_zram(dev);

	down_read(&zram->init_lock);
	spin_lock(&zram->wb_limit_lock);
	val = zram->wb_limit_enable;
	spin_unlock(&zram->wb_limit_lock);
	up_read(&zram->init_lock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t writeback_limit_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct zram *zram = dev_to_zram(dev);
	u64 val;
	ssize_t ret = -EINVAL;

	if (kstrtoull(buf, 10, &val))
		return ret;

	down_read(&zram->init_lock);
	spin_lock(&zram->wb_limit_lock);
	zram->bd_wb_limit = val;
	spin_unlock(&zram->wb_limit_lock);
	up_read(&zram->init_lock);
	ret = len;

	return ret;
}

static ssize_t writeback_limit_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u64 val;
	struct zram *zram = dev_to_zram(dev);

	down_read(&zram->init_lock);
	spin_lock(&zram->wb_limit_lock);
	val = zram->bd_wb_limit;
	spin_unlock(&zram->wb_limit_lock);
	up_read(&zram->init_lock);

	return scnprintf(buf, PAGE_SIZE, "%llu\n", val);
}

static void reset_bdev(struct zram *zram)
{
	if (!zram->backing_dev)
		return;

	/* hope filp_close flush all of IO */
	filp_close(zram->backing_dev, NULL);
	zram->backing_dev = NULL;
	zram->bdev = NULL;
	zram->disk->fops = &zram_devops;
	kvfree(zram->bitmap);
	zram->bitmap = NULL;
}

static ssize_t backing_dev_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct file *file;
	struct zram *zram = dev_to_zram(dev);
	char *p;
	ssize_t ret;

	down_read(&zram->init_lock);
	file = zram->backing_dev;
	if (!file) {
		memcpy(buf, "none\n", 5);
		up_read(&zram->init_lock);
		return 5;
	}

	p = file_path(file, buf, PAGE_SIZE - 1);
	if (IS_ERR(p)) {
		ret = PTR_ERR(p);
		goto out;
	}

	ret = strlen(p);
	memmove(buf, p, ret);
	buf[ret++] = '\n';
out:
	up_read(&zram->init_lock);
	return ret;
}

static ssize_t backing_dev_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	char *file_name;
	size_t sz;
	struct file *backing_dev = NULL;
	struct inode *inode;
	unsigned int bitmap_sz;
	unsigned long nr_pages, *bitmap = NULL;
	int err;
	struct zram *zram = dev_to_zram(dev);

	file_name = kmalloc(PATH_MAX, GFP_KERNEL);
	if (!file_name)
		return -ENOMEM;

	down_write(&zram->init_lock);
	if (init_done(zram)) {
		pr_info("Can't setup backing device for initialized device\n");
		err = -EBUSY;
		goto out;
	}

	strscpy(file_name, buf, PATH_MAX);
	/* ignore trailing newline */
	sz = strlen(file_name);
	if (sz > 0 && file_name[sz - 1] == '\n')
		file_name[sz - 1] = 0x00;

	backing_dev = filp_open_block(file_name, O_RDWR | O_LARGEFILE | O_EXCL, 0);
	if (IS_ERR(backing_dev)) {
		err = PTR_ERR(backing_dev);
		backing_dev = NULL;
		goto out;
	}

	inode = backing_dev->f_mapping->host;

	/* Support only block device in this moment */
	if (!S_ISBLK(inode->i_mode)) {
		err = -ENOTBLK;
		goto out;
	}

	nr_pages = i_size_read(inode) >> PAGE_SHIFT;
	bitmap_sz = BITS_TO_LONGS(nr_pages) * sizeof(long);
	bitmap = kvzalloc(bitmap_sz, GFP_KERNEL);
	if (!bitmap) {
		err = -ENOMEM;
		goto out;
	}

	reset_bdev(zram);

	zram->bdev = I_BDEV(inode);
	zram->backing_dev = backing_dev;
	zram->bitmap = bitmap;
	zram->nr_pages = nr_pages;
	up_write(&zram->init_lock);

	pr_info("setup backing device %s\n", file_name);
	kfree(file_name);

	return len;
out:
	kvfree(bitmap);

	if (backing_dev)
		filp_close(backing_dev, NULL);

	up_write(&zram->init_lock);

	kfree(file_name);

	return err;
}

static unsigned long alloc_block_bdev(struct zram *zram)
{
	unsigned long blk_idx = 1;
retry:
	/* skip 0 bit to confuse zram.handle = 0 */
	blk_idx = find_next_zero_bit(zram->bitmap, zram->nr_pages, blk_idx);
	if (blk_idx == zram->nr_pages)
		return 0;

	if (test_and_set_bit(blk_idx, zram->bitmap))
		goto retry;

	atomic64_inc(&zram->stats.bd_count);
	return blk_idx;
}

static void free_block_bdev(struct zram *zram, unsigned long blk_idx)
{
	int was_set;

	was_set = test_and_clear_bit(blk_idx, zram->bitmap);
	WARN_ON_ONCE(!was_set);
	atomic64_dec(&zram->stats.bd_count);
}

static void read_from_bdev_async(struct zram *zram, struct page *page,
			unsigned long entry, struct bio *parent)
{
	struct bio *bio;

	bio = bio_alloc(zram->bdev, 1, parent->bi_opf, GFP_NOIO);
	bio->bi_iter.bi_sector = entry * (PAGE_SIZE >> 9);
	__bio_add_page(bio, page, PAGE_SIZE, 0);
	bio_chain(bio, parent);
	submit_bio(bio);
}

#define PAGE_WB_SIG "page_index="

#define PAGE_WRITEBACK			0
#define HUGE_WRITEBACK			(1<<0)
#define IDLE_WRITEBACK			(1<<1)
#define INCOMPRESSIBLE_WRITEBACK	(1<<2)

static ssize_t writeback_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct zram *zram = dev_to_zram(dev);
	unsigned long nr_pages = zram->disksize >> PAGE_SHIFT;
	unsigned long index = 0;
	struct bio bio;
	struct bio_vec bio_vec;
	struct page *page;
	ssize_t ret = len;
	int mode, err;
	unsigned long blk_idx = 0;

	if (sysfs_streq(buf, "idle"))
		mode = IDLE_WRITEBACK;
	else if (sysfs_streq(buf, "huge"))
		mode = HUGE_WRITEBACK;
	else if (sysfs_streq(buf, "huge_idle"))
		mode = IDLE_WRITEBACK | HUGE_WRITEBACK;
	else if (sysfs_streq(buf, "incompressible"))
		mode = INCOMPRESSIBLE_WRITEBACK;
	else {
		if (strncmp(buf, PAGE_WB_SIG, sizeof(PAGE_WB_SIG) - 1))
			return -EINVAL;

		if (kstrtol(buf + sizeof(PAGE_WB_SIG) - 1, 10, &index) ||
				index >= nr_pages)
			return -EINVAL;

		nr_pages = 1;
		mode = PAGE_WRITEBACK;
	}

	down_read(&zram->init_lock);
	if (!init_done(zram)) {
		ret = -EINVAL;
		goto release_init_lock;
	}

	if (!zram->backing_dev) {
		ret = -ENODEV;
		goto release_init_lock;
	}

	page = alloc_page(GFP_KERNEL);
	if (!page) {
		ret = -ENOMEM;
		goto release_init_lock;
	}

	for (; nr_pages != 0; index++, nr_pages--) {
		spin_lock(&zram->wb_limit_lock);
		if (zram->wb_limit_enable && !zram->bd_wb_limit) {
			spin_unlock(&zram->wb_limit_lock);
			ret = -EIO;
			break;
		}
		spin_unlock(&zram->wb_limit_lock);

		if (!blk_idx) {
			blk_idx = alloc_block_bdev(zram);
			if (!blk_idx) {
				ret = -ENOSPC;
				break;
			}
		}

		zram_slot_lock(zram, index);
		if (!zram_allocated(zram, index))
			goto next;

		if (zram_in_hw_processing(zram, index))
			goto next;

		if (zram_test_flag(zram, index, ZRAM_WB) ||
				zram_test_flag(zram, index, ZRAM_SAME) ||
				zram_test_flag(zram, index, ZRAM_UNDER_WB))
			goto next;

		if (mode & IDLE_WRITEBACK &&
		    !zram_test_flag(zram, index, ZRAM_IDLE))
			goto next;
		if (mode & HUGE_WRITEBACK &&
		    !zram_test_flag(zram, index, ZRAM_HUGE))
			goto next;
		if (mode & INCOMPRESSIBLE_WRITEBACK &&
		    !zram_test_flag(zram, index, ZRAM_INCOMPRESSIBLE))
			goto next;

		/*
		 * Clearing ZRAM_UNDER_WB is duty of caller.
		 * IOW, zram_free_page never clear it.
		 */
		zram_set_flag(zram, index, ZRAM_UNDER_WB);
		/* Need for hugepage writeback racing */
		zram_set_flag(zram, index, ZRAM_IDLE);
		zram_slot_unlock(zram, index);

		/* Convert HW compressed buf to zspool if necessary */
		if (zram->ops->to_zspool)
			zram->ops->to_zspool(zram, index);

		if (zram_read_page(zram, page, index, NULL)) {
			zram_slot_lock(zram, index);
			zram_clear_flag(zram, index, ZRAM_UNDER_WB);
			zram_clear_flag(zram, index, ZRAM_IDLE);
			zram_slot_unlock(zram, index);
			continue;
		}

		bio_init(&bio, zram->bdev, &bio_vec, 1,
			 REQ_OP_WRITE | REQ_SYNC);
		bio.bi_iter.bi_sector = blk_idx * (PAGE_SIZE >> 9);
		__bio_add_page(&bio, page, PAGE_SIZE, 0);

		/*
		 * XXX: A single page IO would be inefficient for write
		 * but it would be not bad as starter.
		 */
		err = submit_bio_wait(&bio);
		if (err) {
			zram_slot_lock(zram, index);
			zram_clear_flag(zram, index, ZRAM_UNDER_WB);
			zram_clear_flag(zram, index, ZRAM_IDLE);
			zram_slot_unlock(zram, index);
			/*
			 * BIO errors are not fatal, we continue and simply
			 * attempt to writeback the remaining objects (pages).
			 * At the same time we need to signal user-space that
			 * some writes (at least one, but also could be all of
			 * them) were not successful and we do so by returning
			 * the most recent BIO error.
			 */
			ret = err;
			continue;
		}

		atomic64_inc(&zram->stats.bd_writes);
		/*
		 * We released zram_slot_lock so need to check if the slot was
		 * changed. If there is freeing for the slot, we can catch it
		 * easily by zram_allocated.
		 * A subtle case is the slot is freed/reallocated/marked as
		 * ZRAM_IDLE again. To close the race, idle_store doesn't
		 * mark ZRAM_IDLE once it found the slot was ZRAM_UNDER_WB.
		 * Thus, we could close the race by checking ZRAM_IDLE bit.
		 */
		zram_slot_lock(zram, index);
		if (!zram_allocated(zram, index) ||
			  !zram_test_flag(zram, index, ZRAM_IDLE)) {
			zram_clear_flag(zram, index, ZRAM_UNDER_WB);
			zram_clear_flag(zram, index, ZRAM_IDLE);
			goto next;
		}

		zram_free_page(zram, index);
		zram_clear_flag(zram, index, ZRAM_UNDER_WB);
		zram_set_flag(zram, index, ZRAM_WB);
		zram_set_element(zram, index, blk_idx);
		blk_idx = 0;
		atomic64_inc(&zram->stats.pages_stored);
		spin_lock(&zram->wb_limit_lock);
		if (zram->wb_limit_enable && zram->bd_wb_limit > 0)
			zram->bd_wb_limit -=  1UL << (PAGE_SHIFT - 12);
		spin_unlock(&zram->wb_limit_lock);
next:
		zram_slot_unlock(zram, index);
	}

	if (blk_idx)
		free_block_bdev(zram, blk_idx);
	__free_page(page);
release_init_lock:
	up_read(&zram->init_lock);

	return ret;
}

struct zram_work {
	struct work_struct work;
	struct zram *zram;
	unsigned long entry;
	struct page *page;
	int error;
};

static void zram_sync_read(struct work_struct *work)
{
	struct zram_work *zw = container_of(work, struct zram_work, work);
	struct bio_vec bv;
	struct bio bio;

	bio_init(&bio, zw->zram->bdev, &bv, 1, REQ_OP_READ);
	bio.bi_iter.bi_sector = zw->entry * (PAGE_SIZE >> 9);
	__bio_add_page(&bio, zw->page, PAGE_SIZE, 0);
	zw->error = submit_bio_wait(&bio);
}

/*
 * Block layer want one ->submit_bio to be active at a time, so if we use
 * chained IO with parent IO in same context, it's a deadlock. To avoid that,
 * use a worker thread context.
 */
static int read_from_bdev_sync(struct zram *zram, struct page *page,
				unsigned long entry)
{
	struct zram_work work;

	work.page = page;
	work.zram = zram;
	work.entry = entry;

	INIT_WORK_ONSTACK(&work.work, zram_sync_read);
	queue_work(system_unbound_wq, &work.work);
	flush_work(&work.work);
	destroy_work_on_stack(&work.work);

	return work.error;
}

static int read_from_bdev(struct zram *zram, struct page *page,
			unsigned long entry, struct bio *parent)
{
	atomic64_inc(&zram->stats.bd_reads);
	if (!parent) {
		if (WARN_ON_ONCE(!IS_ENABLED(ZRAM_PARTIAL_IO)))
			return -EIO;
		return read_from_bdev_sync(zram, page, entry);
	}
	read_from_bdev_async(zram, page, entry, parent);
	return 0;
}
#else
static inline void reset_bdev(struct zram *zram) {};
static int read_from_bdev(struct zram *zram, struct page *page,
			unsigned long entry, struct bio *parent)
{
	return -EIO;
}

static void free_block_bdev(struct zram *zram, unsigned long blk_idx) {};
#endif

#ifdef CONFIG_ZRAM_MEMORY_TRACKING

static struct dentry *zram_debugfs_root;

static void zram_debugfs_create(void)
{
	zram_debugfs_root = debugfs_create_dir("zram", NULL);
}

static void zram_debugfs_destroy(void)
{
	debugfs_remove_recursive(zram_debugfs_root);
}

static ssize_t read_block_state(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	char *kbuf;
	ssize_t index, written = 0;
	struct zram *zram = file->private_data;
	unsigned long nr_pages = zram->disksize >> PAGE_SHIFT;
	struct timespec64 ts;

	kbuf = kvmalloc(count, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	down_read(&zram->init_lock);
	if (!init_done(zram)) {
		up_read(&zram->init_lock);
		kvfree(kbuf);
		return -EINVAL;
	}

	for (index = *ppos; index < nr_pages; index++) {
		int copied;

		zram_slot_lock(zram, index);
		if (!zram_allocated(zram, index))
			goto next;

		ts = ktime_to_timespec64((ZRAM_TE(zram, index))->ac_time);
		copied = snprintf(kbuf + written, count,
			"%12zd %12lld.%06lu %c%c%c%c%c%c\n",
			index, (s64)ts.tv_sec,
			ts.tv_nsec / NSEC_PER_USEC,
			zram_test_flag(zram, index, ZRAM_SAME) ? 's' : '.',
			zram_test_flag(zram, index, ZRAM_WB) ? 'w' : '.',
			zram_test_flag(zram, index, ZRAM_HUGE) ? 'h' : '.',
			zram_test_flag(zram, index, ZRAM_IDLE) ? 'i' : '.',
			zram_get_priority(zram, index) ? 'r' : '.',
			zram_test_flag(zram, index,
				       ZRAM_INCOMPRESSIBLE) ? 'n' : '.');

		if (count <= copied) {
			zram_slot_unlock(zram, index);
			break;
		}
		written += copied;
		count -= copied;
next:
		zram_slot_unlock(zram, index);
		*ppos += 1;
	}

	up_read(&zram->init_lock);
	if (copy_to_user(buf, kbuf, written))
		written = -EFAULT;
	kvfree(kbuf);

	return written;
}

static const struct file_operations proc_zram_block_state_op = {
	.open = simple_open,
	.read = read_block_state,
	.llseek = default_llseek,
};

static void zram_debugfs_register(struct zram *zram)
{
	if (!zram_debugfs_root)
		return;

	zram->debugfs_dir = debugfs_create_dir(zram->disk->disk_name,
						zram_debugfs_root);
	debugfs_create_file("block_state", 0400, zram->debugfs_dir,
				zram, &proc_zram_block_state_op);
}

static void zram_debugfs_unregister(struct zram *zram)
{
	debugfs_remove_recursive(zram->debugfs_dir);
}
#else
static void zram_debugfs_create(void) {};
static void zram_debugfs_destroy(void) {};
static void zram_debugfs_register(struct zram *zram) {};
static void zram_debugfs_unregister(struct zram *zram) {};
#endif

/*
 * We switched to per-cpu streams and this attr is not needed anymore.
 * However, we will keep it around for some time, because:
 * a) we may revert per-cpu streams in the future
 * b) it's visible to user space and we need to follow our 2 years
 *    retirement rule; but we already have a number of 'soon to be
 *    altered' attrs, so max_comp_streams need to wait for the next
 *    layoff cycle.
 */
static ssize_t max_comp_streams_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", num_online_cpus());
}

static ssize_t max_comp_streams_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	return len;
}

static void comp_algorithm_set(struct zram *zram, u32 prio, const char *alg)
{
	/* Do not free statically defined compression algorithms */
	if (zram->comp_algs[prio] != default_compressor)
		kfree(zram->comp_algs[prio]);

	zram->comp_algs[prio] = alg;
}

static ssize_t __comp_algorithm_show(struct zram *zram, u32 prio, char *buf)
{
	ssize_t sz;

	down_read(&zram->init_lock);
	sz = zcomp_available_show(zram->comp_algs[prio], buf);
	up_read(&zram->init_lock);

	return sz;
}

static int __comp_algorithm_store(struct zram *zram, u32 prio, const char *buf)
{
	char *compressor;
	size_t sz;

	sz = strlen(buf);
	if (sz >= CRYPTO_MAX_ALG_NAME)
		return -E2BIG;

	compressor = kstrdup(buf, GFP_KERNEL);
	if (!compressor)
		return -ENOMEM;

	/* ignore trailing newline */
	if (sz > 0 && compressor[sz - 1] == '\n')
		compressor[sz - 1] = 0x00;

	if (!zcomp_available_algorithm(compressor)) {
		kfree(compressor);
		return -EINVAL;
	}

	down_write(&zram->init_lock);
	if (init_done(zram)) {
		up_write(&zram->init_lock);
		kfree(compressor);
		pr_info("Can't change algorithm for initialized device\n");
		return -EBUSY;
	}

	comp_algorithm_set(zram, prio, compressor);
	up_write(&zram->init_lock);
	return 0;
}

static ssize_t comp_algorithm_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct zram *zram = dev_to_zram(dev);

	return __comp_algorithm_show(zram, ZRAM_PRIMARY_COMP, buf);
}

static ssize_t comp_algorithm_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf,
				    size_t len)
{
	struct zram *zram = dev_to_zram(dev);
	int ret;

	ret = __comp_algorithm_store(zram, ZRAM_PRIMARY_COMP, buf);
	return ret ? ret : len;
}

#ifdef CONFIG_ZRAM_MULTI_COMP
static ssize_t recomp_algorithm_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct zram *zram = dev_to_zram(dev);
	ssize_t sz = 0;
	u32 prio;

	for (prio = ZRAM_SECONDARY_COMP; prio < ZRAM_MAX_COMPS; prio++) {
		if (!zram->comp_algs[prio])
			continue;

		sz += scnprintf(buf + sz, PAGE_SIZE - sz - 2, "#%d: ", prio);
		sz += __comp_algorithm_show(zram, prio, buf + sz);
	}

	return sz;
}

static ssize_t recomp_algorithm_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t len)
{
	struct zram *zram = dev_to_zram(dev);
	int prio = ZRAM_SECONDARY_COMP;
	char *args, *param, *val;
	char *alg = NULL;
	int ret;

	args = skip_spaces(buf);
	while (*args) {
		args = next_arg(args, &param, &val);

		if (!val || !*val)
			return -EINVAL;

		if (!strcmp(param, "algo")) {
			alg = val;
			continue;
		}

		if (!strcmp(param, "priority")) {
			ret = kstrtoint(val, 10, &prio);
			if (ret)
				return ret;
			continue;
		}
	}

	if (!alg)
		return -EINVAL;

	if (prio < ZRAM_SECONDARY_COMP || prio >= ZRAM_MAX_COMPS)
		return -EINVAL;

	ret = __comp_algorithm_store(zram, prio, alg);
	return ret ? ret : len;
}
#endif

static ssize_t compact_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct zram *zram = dev_to_zram(dev);

	down_read(&zram->init_lock);
	if (!init_done(zram)) {
		up_read(&zram->init_lock);
		return -EINVAL;
	}

	zs_compact(zram->mem_pool);
	up_read(&zram->init_lock);

	return len;
}

static ssize_t io_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zram *zram = dev_to_zram(dev);
	ssize_t ret;

	down_read(&zram->init_lock);
	ret = scnprintf(buf, PAGE_SIZE,
			"%8llu %8llu 0 %8llu %8llu %8llu %8llu %8llu %8llu %8llu %8llu\n",
			(u64)atomic64_read(&zram->stats.failed_reads),
			(u64)atomic64_read(&zram->stats.failed_writes),
			(u64)atomic64_read(&zram->stats.notify_free),
			(u64)atomic64_read(&zram->stats.hw_failed_reads),
			(u64)atomic64_read(&zram->stats.hw_failed_writes),
			(u64)atomic64_read(&zram->stats.hw_inuse),
			(u64)atomic64_read(&zram->stats.hw_busy),
			(u64)atomic64_read(&zram->stats.hw_busy_wait),
			(u64)atomic64_read(&zram->stats.hw_dec_busy),
			(u64)atomic64_read(&zram->stats.hw_dec_busy_wait));
	up_read(&zram->init_lock);

	return ret;
}

static ssize_t mm_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zram *zram = dev_to_zram(dev);
	struct zs_pool_stats pool_stats;
	u64 orig_size, mem_used = 0;
	long max_used;
	ssize_t ret;

	memset(&pool_stats, 0x00, sizeof(struct zs_pool_stats));

	down_read(&zram->init_lock);
	if (init_done(zram)) {
		mem_used = zs_get_total_pages(zram->mem_pool);
		zs_pool_stats(zram->mem_pool, &pool_stats);
	}

	orig_size = atomic64_read(&zram->stats.pages_stored);
	max_used = atomic_long_read(&zram->stats.max_used_pages);

	ret = scnprintf(buf, PAGE_SIZE,
			"%8llu %8llu %8llu %8lu %8ld %8llu %8lu %8llu %8llu %8llu\n",
			orig_size << PAGE_SHIFT,
			(u64)atomic64_read(&zram->stats.compr_data_size),
			mem_used << PAGE_SHIFT,
			zram->limit_pages << PAGE_SHIFT,
			max_used << PAGE_SHIFT,
			(u64)atomic64_read(&zram->stats.same_pages),
			atomic_long_read(&pool_stats.pages_compacted),
			(u64)atomic64_read(&zram->stats.huge_pages),
			(u64)atomic64_read(&zram->stats.huge_pages_since),
			(u64)atomic64_read(&zram->stats.hw_huge_pages_since));
	up_read(&zram->init_lock);

	return ret;
}

#ifdef CONFIG_ZRAM_WRITEBACK
#define FOUR_K(x) ((x) * (1 << (PAGE_SHIFT - 12)))
static ssize_t bd_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zram *zram = dev_to_zram(dev);
	ssize_t ret;

	down_read(&zram->init_lock);
	ret = scnprintf(buf, PAGE_SIZE,
		"%8llu %8llu %8llu\n",
			FOUR_K((u64)atomic64_read(&zram->stats.bd_count)),
			FOUR_K((u64)atomic64_read(&zram->stats.bd_reads)),
			FOUR_K((u64)atomic64_read(&zram->stats.bd_writes)));
	up_read(&zram->init_lock);

	return ret;
}
#endif

static ssize_t debug_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int version = 1;
	struct zram *zram = dev_to_zram(dev);
	ssize_t ret;

	down_read(&zram->init_lock);
	ret = scnprintf(buf, PAGE_SIZE,
			"version: %d\n%8llu %8llu\n",
			version,
			(u64)atomic64_read(&zram->stats.writestall),
			(u64)atomic64_read(&zram->stats.miss_free));
	up_read(&zram->init_lock);

	return ret;
}

static DEVICE_ATTR_RO(io_stat);
static DEVICE_ATTR_RO(mm_stat);
#ifdef CONFIG_ZRAM_WRITEBACK
static DEVICE_ATTR_RO(bd_stat);
#endif
static DEVICE_ATTR_RO(debug_stat);

static void zram_meta_free(struct zram *zram, u64 disksize)
{
	size_t num_pages = disksize >> PAGE_SHIFT;
	size_t index;

	if (zram->ops->wait_for_hw)
		zram->ops->wait_for_hw(zram);

	/* Free all pages that are still in this zram device */
	for (index = 0; index < num_pages; index++)
		zram_free_page(zram, index);

	zs_destroy_pool(zram->mem_pool);
	vfree(zram->table);
}

static bool zram_meta_alloc(struct zram *zram, u64 disksize)
{
	size_t num_pages;

	num_pages = disksize >> PAGE_SHIFT;
	zram->table = vzalloc(array_size(num_pages, zram->ops->table_entry_sz));
	if (!zram->table)
		return false;

	zram->mem_pool = zs_create_pool(zram->disk->disk_name);
	if (!zram->mem_pool) {
		vfree(zram->table);
		return false;
	}

	if (!huge_class_size)
		huge_class_size = zs_huge_class_size(zram->mem_pool);

	pr_info("%s: %llu\n", __func__, (unsigned long long)huge_class_size);

	return true;
}

/*
 * To protect concurrent access to the same index entry,
 * caller should hold this table index entry's bit_spinlock to
 * indicate this index entry is accessing.
 */
static void zram_free_page(struct zram *zram, size_t index)
{
	unsigned long handle;

#ifdef CONFIG_ZRAM_TRACK_ENTRY_ACTIME
	(ZRAM_TE(zram, index))->ac_time = 0;
#endif
	if (zram_test_flag(zram, index, ZRAM_IDLE))
		zram_clear_flag(zram, index, ZRAM_IDLE);

	if (zram_test_flag(zram, index, ZRAM_HUGE)) {
		zram_clear_flag(zram, index, ZRAM_HUGE);
		atomic64_dec(&zram->stats.huge_pages);
	}

	if (zram_test_flag(zram, index, ZRAM_INCOMPRESSIBLE))
		zram_clear_flag(zram, index, ZRAM_INCOMPRESSIBLE);

	zram_set_priority(zram, index, 0);

	if (zram_test_flag(zram, index, ZRAM_WB)) {
		zram_clear_flag(zram, index, ZRAM_WB);
		free_block_bdev(zram, zram_get_element(zram, index));
		goto out;
	}

	/*
	 * No memory is allocated for same element filled pages.
	 * Simply clear same page flag.
	 */
	if (zram_test_flag(zram, index, ZRAM_SAME)) {
		zram_clear_flag(zram, index, ZRAM_SAME);
		atomic64_dec(&zram->stats.same_pages);
		goto out;
	}

	/* If handle is 0 here, it means no compressed memory for this entry */
	handle = zram_get_handle(zram, index);
	if (!handle)
		return;

	/* Release corresponding buffers */
	if (zram->ops->free_entry)
		zram->ops->free_entry(zram, index);
	else
		zs_free(zram->mem_pool, handle);

	atomic64_sub(zram_get_obj_size(zram, index),
			&zram->stats.compr_data_size);
out:
	atomic64_dec(&zram->stats.pages_stored);
	zram_set_handle(zram, index, 0);
	zram_set_obj_size(zram, index, 0);
	WARN_ON_ONCE((ZRAM_TE(zram, index))->flags &
		~(1UL << ZRAM_LOCK | 1UL << ZRAM_UNDER_WB));
}

/*
 * Reads (decompresses if needed) a page from zspool (zsmalloc).
 * Corresponding ZRAM slot should be locked.
 */
static int zram_read_from_zspool(struct zram *zram, struct page *page,
				 u32 index)
{
	struct zcomp_strm *zstrm;
	unsigned long handle;
	unsigned int size;
	void *src, *dst;
	u32 prio;
	int ret;

	handle = zram_get_handle(zram, index);
	if (!handle || zram_test_flag(zram, index, ZRAM_SAME)) {
		unsigned long value;
		void *mem;

		value = handle ? zram_get_element(zram, index) : 0;
		mem = kmap_local_page(page);
		zram_fill_page(mem, PAGE_SIZE, value);
		kunmap_local(mem);
		return 0;
	}

	size = zram_get_obj_size(zram, index);

	if (size != PAGE_SIZE) {
		prio = zram_get_priority(zram, index);
		zstrm = zcomp_stream_get(zram->comps[prio]);
	}

	src = zs_map_object(zram->mem_pool, handle, ZS_MM_RO);
	if (size == PAGE_SIZE) {
		dst = kmap_local_page(page);
		copy_page(dst, src);
		kunmap_local(dst);
		ret = 0;
	} else {
		dst = kmap_local_page(page);
		ret = zcomp_decompress(zstrm, src, size, dst);
		kunmap_local(dst);
		zcomp_stream_put(zram->comps[prio]);
	}
	zs_unmap_object(zram->mem_pool, handle);
	return ret;
}

static int zram_read_page(struct zram *zram, struct page *page, u32 index,
			  struct bio *parent)
{
	int ret;

	zram_slot_lock(zram, index);
	if (!zram_test_flag(zram, index, ZRAM_WB)) {
		/* Slot should be locked through out the function call */
		ret = zram_read_from_zspool(zram, page, index);
		zram_slot_unlock(zram, index);
	} else {
		/*
		 * The slot should be unlocked before reading from the backing
		 * device.
		 */
		zram_slot_unlock(zram, index);

		ret = read_from_bdev(zram, page, zram_get_element(zram, index),
				     parent);
	}

	/* Should NEVER happen. Return bio error if it does. */
	if (WARN_ON(ret < 0))
		pr_err("Decompression failed! err=%d, page=%u\n", ret, index);

	return ret;
}

/*
 * Use a temporary buffer to decompress the page, as the decompressor
 * always expects a full page for the output.
 */
static int zram_bvec_read_partial(struct zram *zram, struct bio_vec *bvec,
				  u32 index, int offset)
{
	struct page *page = alloc_page(GFP_NOIO);
	int ret;

	if (!page)
		return -ENOMEM;
	ret = zram_read_page(zram, page, index, NULL);
	if (likely(!ret))
		memcpy_to_bvec(bvec, page_address(page) + offset);
	__free_page(page);
	return ret;
}

static int zram_bvec_read(struct zram *zram, struct bio_vec *bvec,
			  u32 index, int offset, struct bio *bio)
{
	if (is_partial_io(bvec))
		return zram_bvec_read_partial(zram, bvec, index, offset);
	return zram_read_page(zram, bvec->bv_page, index, bio);
}

static int zram_write_page(struct zram *zram, struct page *page, u32 index)
{
	int ret = 0;
	unsigned long alloced_pages;
	unsigned long handle = -ENOMEM;
	unsigned int comp_len = 0;
	void *src, *dst, *mem;
	struct zcomp_strm *zstrm;
	unsigned long element = 0;
	enum zram_pageflags flags = 0;

	mem = kmap_local_page(page);
	if (page_same_filled(mem, &element)) {
		kunmap_local(mem);
		/* Free memory associated with this sector now. */
		flags = ZRAM_SAME;
		atomic64_inc(&zram->stats.same_pages);
		goto out;
	}
	kunmap_local(mem);

compress_again:
	zstrm = zcomp_stream_get(zram->comps[ZRAM_PRIMARY_COMP]);
	src = kmap_local_page(page);
	ret = zcomp_compress(zstrm, src, &comp_len);
	kunmap_local(src);

	if (unlikely(ret)) {
		zcomp_stream_put(zram->comps[ZRAM_PRIMARY_COMP]);
		pr_err("Compression failed! err=%d\n", ret);
		zs_free(zram->mem_pool, handle);
		return ret;
	}

	if (comp_len >= huge_class_size)
		comp_len = PAGE_SIZE;
	/*
	 * handle allocation has 2 paths:
	 * a) fast path is executed with preemption disabled (for
	 *  per-cpu streams) and has __GFP_DIRECT_RECLAIM bit clear,
	 *  since we can't sleep;
	 * b) slow path enables preemption and attempts to allocate
	 *  the page with __GFP_DIRECT_RECLAIM bit set. we have to
	 *  put per-cpu compression stream and, thus, to re-do
	 *  the compression once handle is allocated.
	 *
	 * if we have a 'non-null' handle here then we are coming
	 * from the slow path and handle has already been allocated.
	 */
	if (IS_ERR_VALUE(handle))
		handle = zs_malloc(zram->mem_pool, comp_len,
				__GFP_KSWAPD_RECLAIM |
				__GFP_NOWARN |
				__GFP_HIGHMEM |
				__GFP_MOVABLE);
	if (IS_ERR_VALUE(handle)) {
		zcomp_stream_put(zram->comps[ZRAM_PRIMARY_COMP]);
		atomic64_inc(&zram->stats.writestall);
		handle = zs_malloc(zram->mem_pool, comp_len,
				GFP_NOIO | __GFP_HIGHMEM |
				__GFP_MOVABLE);
		if (IS_ERR_VALUE(handle))
			return PTR_ERR((void *)handle);

		if (comp_len != PAGE_SIZE)
			goto compress_again;
		/*
		 * If the page is not compressible, you need to acquire the
		 * lock and execute the code below. The zcomp_stream_get()
		 * call is needed to disable the cpu hotplug and grab the
		 * zstrm buffer back. It is necessary that the dereferencing
		 * of the zstrm variable below occurs correctly.
		 */
		zstrm = zcomp_stream_get(zram->comps[ZRAM_PRIMARY_COMP]);
	}

	alloced_pages = zs_get_total_pages(zram->mem_pool);
	update_used_max(zram, alloced_pages);

	if (zram->limit_pages && alloced_pages > zram->limit_pages) {
		zcomp_stream_put(zram->comps[ZRAM_PRIMARY_COMP]);
		zs_free(zram->mem_pool, handle);
		return -ENOMEM;
	}

	dst = zs_map_object(zram->mem_pool, handle, ZS_MM_WO);

	src = zstrm->buffer;
	if (comp_len == PAGE_SIZE)
		src = kmap_local_page(page);
	memcpy(dst, src, comp_len);
	if (comp_len == PAGE_SIZE)
		kunmap_local(src);

	zcomp_stream_put(zram->comps[ZRAM_PRIMARY_COMP]);
	zs_unmap_object(zram->mem_pool, handle);
	atomic64_add(comp_len, &zram->stats.compr_data_size);
out:
	/*
	 * Free memory associated with this sector
	 * before overwriting unused sectors.
	 */
	zram_slot_lock(zram, index);
	zram_free_page(zram, index);

	if (comp_len == PAGE_SIZE) {
		zram_set_flag(zram, index, ZRAM_HUGE);
		atomic64_inc(&zram->stats.huge_pages);
		atomic64_inc(&zram->stats.huge_pages_since);
	}

	if (flags) {
		zram_set_flag(zram, index, flags);
		zram_set_element(zram, index, element);
	}  else {
		zram_set_handle(zram, index, handle);
		zram_set_obj_size(zram, index, comp_len);
	}
	zram_slot_unlock(zram, index);

	/* Update stats */
	atomic64_inc(&zram->stats.pages_stored);
	return ret;
}

/*
 * This is a partial IO. Read the full page before writing the changes.
 */
static int zram_bvec_write_partial(struct zram *zram, struct bio_vec *bvec,
				   u32 index, int offset, struct bio *bio)
{
	struct page *page = alloc_page(GFP_NOIO);
	int ret;

	if (!page)
		return -ENOMEM;

	ret = zram_read_page(zram, page, index, bio);
	if (!ret) {
		memcpy_from_bvec(page_address(page) + offset, bvec);
		ret = zram_write_page(zram, page, index);
	}
	__free_page(page);
	return ret;
}

static int zram_bvec_write(struct zram *zram, struct bio_vec *bvec,
			   u32 index, int offset, struct bio *bio)
{
	if (is_partial_io(bvec))
		return zram_bvec_write_partial(zram, bvec, index, offset, bio);
	return zram_write_page(zram, bvec->bv_page, index);
}

#ifdef CONFIG_ZRAM_MULTI_COMP
/*
 * This function will decompress (unless it's ZRAM_HUGE) the page and then
 * attempt to compress it using provided compression algorithm priority
 * (which is potentially more effective).
 *
 * Corresponding ZRAM slot should be locked.
 */
static int zram_recompress(struct zram *zram, u32 index, struct page *page,
			   u64 *num_recomp_pages, u32 threshold, u32 prio,
			   u32 prio_max)
{
	struct zcomp_strm *zstrm = NULL;
	unsigned long handle_old;
	unsigned long handle_new;
	unsigned int comp_len_old;
	unsigned int comp_len_new;
	unsigned int class_index_old;
	unsigned int class_index_new;
	u32 num_recomps = 0;
	void *src, *dst;
	int ret;

	handle_old = zram_get_handle(zram, index);
	if (!handle_old)
		return -EINVAL;

	comp_len_old = zram_get_obj_size(zram, index);
	/*
	 * Do not recompress objects that are already "small enough".
	 */
	if (comp_len_old < threshold)
		return 0;

	ret = zram_read_from_zspool(zram, page, index);
	if (ret)
		return ret;

	class_index_old = zs_lookup_class_index(zram->mem_pool, comp_len_old);
	/*
	 * Iterate the secondary comp algorithms list (in order of priority)
	 * and try to recompress the page.
	 */
	for (; prio < prio_max; prio++) {
		if (!zram->comps[prio])
			continue;

		/*
		 * Skip if the object is already re-compressed with a higher
		 * priority algorithm (or same algorithm).
		 */
		if (prio <= zram_get_priority(zram, index))
			continue;

		num_recomps++;
		zstrm = zcomp_stream_get(zram->comps[prio]);
		src = kmap_local_page(page);
		ret = zcomp_compress(zstrm, src, &comp_len_new);
		kunmap_local(src);

		if (ret) {
			zcomp_stream_put(zram->comps[prio]);
			return ret;
		}

		class_index_new = zs_lookup_class_index(zram->mem_pool,
							comp_len_new);

		/* Continue until we make progress */
		if (class_index_new >= class_index_old ||
		    (threshold && comp_len_new >= threshold)) {
			zcomp_stream_put(zram->comps[prio]);
			continue;
		}

		/* Recompression was successful so break out */
		break;
	}

	/*
	 * We did not try to recompress, e.g. when we have only one
	 * secondary algorithm and the page is already recompressed
	 * using that algorithm
	 */
	if (!zstrm)
		return 0;

	/*
	 * Decrement the limit (if set) on pages we can recompress, even
	 * when current recompression was unsuccessful or did not compress
	 * the page below the threshold, because we still spent resources
	 * on it.
	 */
	if (*num_recomp_pages)
		*num_recomp_pages -= 1;

	if (class_index_new >= class_index_old) {
		/*
		 * Secondary algorithms failed to re-compress the page
		 * in a way that would save memory, mark the object as
		 * incompressible so that we will not try to compress
		 * it again.
		 *
		 * We need to make sure that all secondary algorithms have
		 * failed, so we test if the number of recompressions matches
		 * the number of active secondary algorithms.
		 */
		if (num_recomps == zram->num_active_comps - 1)
			zram_set_flag(zram, index, ZRAM_INCOMPRESSIBLE);
		return 0;
	}

	/* Successful recompression but above threshold */
	if (threshold && comp_len_new >= threshold)
		return 0;

	/*
	 * No direct reclaim (slow path) for handle allocation and no
	 * re-compression attempt (unlike in zram_write_bvec()) since
	 * we already have stored that object in zsmalloc. If we cannot
	 * alloc memory for recompressed object then we bail out and
	 * simply keep the old (existing) object in zsmalloc.
	 */
	handle_new = zs_malloc(zram->mem_pool, comp_len_new,
			       __GFP_KSWAPD_RECLAIM |
			       __GFP_NOWARN |
			       __GFP_HIGHMEM |
			       __GFP_MOVABLE);
	if (IS_ERR_VALUE(handle_new)) {
		zcomp_stream_put(zram->comps[prio]);
		return PTR_ERR((void *)handle_new);
	}

	dst = zs_map_object(zram->mem_pool, handle_new, ZS_MM_WO);
	memcpy(dst, zstrm->buffer, comp_len_new);
	zcomp_stream_put(zram->comps[prio]);

	zs_unmap_object(zram->mem_pool, handle_new);

	zram_free_page(zram, index);
	zram_set_handle(zram, index, handle_new);
	zram_set_obj_size(zram, index, comp_len_new);
	zram_set_priority(zram, index, prio);

	atomic64_add(comp_len_new, &zram->stats.compr_data_size);
	atomic64_inc(&zram->stats.pages_stored);

	return 0;
}

#define RECOMPRESS_IDLE		(1 << 0)
#define RECOMPRESS_HUGE		(1 << 1)

static ssize_t recompress_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	u32 prio = ZRAM_SECONDARY_COMP, prio_max = ZRAM_MAX_COMPS;
	struct zram *zram = dev_to_zram(dev);
	unsigned long nr_pages = zram->disksize >> PAGE_SHIFT;
	char *args, *param, *val, *algo = NULL;
	u64 num_recomp_pages = ULLONG_MAX;
	u32 mode = 0, threshold = 0;
	unsigned long index;
	struct page *page;
	ssize_t ret;

	args = skip_spaces(buf);
	while (*args) {
		args = next_arg(args, &param, &val);

		if (!val || !*val)
			return -EINVAL;

		if (!strcmp(param, "type")) {
			if (!strcmp(val, "idle"))
				mode = RECOMPRESS_IDLE;
			if (!strcmp(val, "huge"))
				mode = RECOMPRESS_HUGE;
			if (!strcmp(val, "huge_idle"))
				mode = RECOMPRESS_IDLE | RECOMPRESS_HUGE;
			continue;
		}

		if (!strcmp(param, "max_pages")) {
			/*
			 * Limit the number of entries (pages) we attempt to
			 * recompress.
			 */
			ret = kstrtoull(val, 10, &num_recomp_pages);
			if (ret)
				return ret;
			continue;
		}

		if (!strcmp(param, "threshold")) {
			/*
			 * We will re-compress only idle objects equal or
			 * greater in size than watermark.
			 */
			ret = kstrtouint(val, 10, &threshold);
			if (ret)
				return ret;
			continue;
		}

		if (!strcmp(param, "algo")) {
			algo = val;
			continue;
		}
	}

	if (threshold >= huge_class_size)
		return -EINVAL;

	down_read(&zram->init_lock);
	if (!init_done(zram)) {
		ret = -EINVAL;
		goto release_init_lock;
	}

	if (algo) {
		bool found = false;

		for (; prio < ZRAM_MAX_COMPS; prio++) {
			if (!zram->comp_algs[prio])
				continue;

			if (!strcmp(zram->comp_algs[prio], algo)) {
				prio_max = min(prio + 1, ZRAM_MAX_COMPS);
				found = true;
				break;
			}
		}

		if (!found) {
			ret = -EINVAL;
			goto release_init_lock;
		}
	}

	page = alloc_page(GFP_KERNEL);
	if (!page) {
		ret = -ENOMEM;
		goto release_init_lock;
	}

	ret = len;
	for (index = 0; index < nr_pages; index++) {
		int err = 0;

		if (!num_recomp_pages)
			break;

		zram_slot_lock(zram, index);

		if (!zram_allocated(zram, index))
			goto next;

		if (mode & RECOMPRESS_IDLE &&
		    !zram_test_flag(zram, index, ZRAM_IDLE))
			goto next;

		if (mode & RECOMPRESS_HUGE &&
		    !zram_test_flag(zram, index, ZRAM_HUGE))
			goto next;

		if (zram_test_flag(zram, index, ZRAM_WB) ||
		    zram_test_flag(zram, index, ZRAM_UNDER_WB) ||
		    zram_test_flag(zram, index, ZRAM_SAME) ||
		    zram_test_flag(zram, index, ZRAM_INCOMPRESSIBLE))
			goto next;

		err = zram_recompress(zram, index, page, &num_recomp_pages,
				      threshold, prio, prio_max);
next:
		zram_slot_unlock(zram, index);
		if (err) {
			ret = err;
			break;
		}

		cond_resched();
	}

	__free_page(page);

release_init_lock:
	up_read(&zram->init_lock);
	return ret;
}
#endif

static void zram_bio_discard(struct zram *zram, struct bio *bio)
{
	size_t n = bio->bi_iter.bi_size;
	u32 index = bio->bi_iter.bi_sector >> SECTORS_PER_PAGE_SHIFT;
	u32 offset = (bio->bi_iter.bi_sector & (SECTORS_PER_PAGE - 1)) <<
			SECTOR_SHIFT;

	/*
	 * zram manages data in physical block size units. Because logical block
	 * size isn't identical with physical block size on some arch, we
	 * could get a discard request pointing to a specific offset within a
	 * certain physical block.  Although we can handle this request by
	 * reading that physiclal block and decompressing and partially zeroing
	 * and re-compressing and then re-storing it, this isn't reasonable
	 * because our intent with a discard request is to save memory.  So
	 * skipping this logical block is appropriate here.
	 */
	if (offset) {
		if (n <= (PAGE_SIZE - offset))
			return;

		n -= (PAGE_SIZE - offset);
		index++;
	}

	while (n >= PAGE_SIZE) {
		zram_slot_lock(zram, index);
		zram_free_page(zram, index);
		zram_slot_unlock(zram, index);
		atomic64_inc(&zram->stats.notify_free);
		index++;
		n -= PAGE_SIZE;
	}

	bio_endio(bio);
}

static void zram_bio_read(struct zram *zram, struct bio *bio)
{
	unsigned long start_time = bio_start_io_acct(bio);
	struct bvec_iter iter = bio->bi_iter;

	do {
		u32 index = iter.bi_sector >> SECTORS_PER_PAGE_SHIFT;
		u32 offset = (iter.bi_sector & (SECTORS_PER_PAGE - 1)) <<
				SECTOR_SHIFT;
		struct bio_vec bv = bio_iter_iovec(bio, iter);

		bv.bv_len = min_t(u32, bv.bv_len, PAGE_SIZE - offset);

		if (zram_bvec_read(zram, &bv, index, offset, bio) < 0) {
			atomic64_inc(&zram->stats.failed_reads);
			bio->bi_status = BLK_STS_IOERR;
			break;
		}
		flush_dcache_page(bv.bv_page);

		zram_slot_lock(zram, index);
		zram_accessed(zram, index);
		zram_slot_unlock(zram, index);

		bio_advance_iter_single(bio, &iter, bv.bv_len);
	} while (iter.bi_size);

	bio_end_io_acct(bio, start_time);
	bio_endio(bio);
}

static void zram_bio_write(struct zram *zram, struct bio *bio)
{
	unsigned long start_time = bio_start_io_acct(bio);
	struct bvec_iter iter = bio->bi_iter;

	do {
		u32 index = iter.bi_sector >> SECTORS_PER_PAGE_SHIFT;
		u32 offset = (iter.bi_sector & (SECTORS_PER_PAGE - 1)) <<
				SECTOR_SHIFT;
		struct bio_vec bv = bio_iter_iovec(bio, iter);

		bv.bv_len = min_t(u32, bv.bv_len, PAGE_SIZE - offset);

		if (zram_bvec_write(zram, &bv, index, offset, bio) < 0) {
			atomic64_inc(&zram->stats.failed_writes);
			bio->bi_status = BLK_STS_IOERR;
			break;
		}

		zram_slot_lock(zram, index);
		zram_accessed(zram, index);
		zram_slot_unlock(zram, index);

		bio_advance_iter_single(bio, &iter, bv.bv_len);
	} while (iter.bi_size);

	bio_end_io_acct(bio, start_time);
	bio_endio(bio);
}

/***********************************************************
 *** Common Functions both for DST and No-DST copy modes ***
 ***********************************************************/

/* Allocate buffer from zspool */
static unsigned long alloc_zspool_memory(struct zram *zram, unsigned int comp_len)
{
	unsigned long alloced_pages;
	unsigned long handle = -ENOMEM;

	preempt_disable();

	/*
	 * handle allocation has 2 paths:
	 * a) fast path is executed with preemption disabled and
	 *    has __GFP_DIRECT_RECLAIM bit clear, since we can't sleep;
	 * b) slow path enables preemption and attempts to allocate
	 *    the page with __GFP_DIRECT_RECLAIM bit set.
	 */
	handle = zs_malloc(zram->mem_pool, comp_len,
			__GFP_KSWAPD_RECLAIM |
			__GFP_NOWARN |
			__GFP_HIGHMEM |
			__GFP_MOVABLE);
	if (IS_ERR_VALUE(handle)) {
		preempt_enable();
		atomic64_inc(&zram->stats.writestall);
		handle = zs_malloc(zram->mem_pool, comp_len,
				GFP_NOIO | __GFP_HIGHMEM |
				__GFP_MOVABLE);
		if (IS_ERR_VALUE(handle))
			return PTR_ERR((void *)handle);
		preempt_disable();
	}

	alloced_pages = zs_get_total_pages(zram->mem_pool);
	update_used_max(zram, alloced_pages);

	if (zram->limit_pages && alloced_pages > zram->limit_pages) {
		preempt_enable();
		zs_free(zram->mem_pool, handle);
		return -ENOMEM;
	}

	preempt_enable();
	return handle;
}

/* Use zsmalloc to store compressed data */
static unsigned long hwcomp_copy_to_zspool(struct zram *zram, void *buffer,
					struct page *page, unsigned int comp_len)
{
	unsigned long handle = -ENOMEM;
	void *src, *dst;

	/* Sanity check */
	if (buffer == NULL && comp_len != PAGE_SIZE) {
		/* Unexpected to be here... */
		pr_info("%s: unexpected buffer & comp_len.\n", __func__);
		return -EINVAL;
	}

	handle = alloc_zspool_memory(zram, comp_len);
	if (IS_ERR_VALUE(handle))
		return -ENOMEM;

	dst = zs_map_object(zram->mem_pool, handle, ZS_MM_WO);
	if (comp_len == PAGE_SIZE) {
		src = kmap_local_page(page);
		copy_page(dst, src);
		kunmap_local(src);
	} else {
		memcpy(dst, buffer, comp_len);
	}
	zs_unmap_object(zram->mem_pool, handle);

	return handle;
}

/* Wait for the completion of HW operations */
static void zram_wait_for_hw(struct zram *zram)
{
	/*
	 * When swapoff is called, zram resource will be released through zram_meta_free.
	 * swapoff operation will make zram wait for the completion of pending write I/Os, no read ones.
	 * (sync_blockdev)
	 * The design is to only make sure the data consistency between RAM and storage.
	 *
	 * When introducing asynchronous write/read I/Os through HW, SWP_SYNCHRONOUS_IO needs to be
	 * removed, but this will make race condition happen between swapoff operation and read I/Os,
	 *
	 * CPU#0: swapoff -> try_to_unuse -> swap_readpage (asynchronous)
	 * CPU#1: (thread for post-processing read I/Os)
	 *
	 * To avoid the case, we need to wait for the completion of pending read I/Os when users want to
	 * release zram resource.
	 */

	wait_event_interruptible(zram->hw_wait, atomic64_read(&zram->stats.hw_inuse) == 0);
}

/* Update huge flag & statistics in post-processing */
static inline void zram_set_huge_for_pp(struct zram *zram, u32 index)
{
	/* Set ZRAM_HUGE */
	zram_set_flag(zram, index, ZRAM_HUGE);

	/* Update statistics */
	atomic64_inc(&zram->stats.huge_pages);
	atomic64_inc(&zram->stats.huge_pages_since);
}

#if IS_ENABLED(CONFIG_HWCOMP_SUPPORT_NO_DST_COPY)
static int __hwcomp_buf_to_zspool(struct zram *zram, u32 index);
#endif

/* Post process for asynchronous hw decompression */
static void hwcomp_decompress_post_process(int err, struct dcomp_pp_info *pp_info)
{
	struct zram *zram = pp_info->zram;
	u32 index = pp_info->index;
	struct bio *bio = pp_info->bio;

	if (unlikely(!zram)) {
		pr_info("%s: Empty ZRAM for index (%u) at (%d)\n", __func__, index, err);
		return;
	}

	if (unlikely(!bio)) {
		pr_info("%s: Empty BIO for index (%u) at (%d)\n", __func__, index, err);
		return;
	}

	/* bio has been acquired, it's ok to reset the field to null here */
	pp_info->bio = NULL;

	zram_slot_lock(zram, index);

	/* Clear ZRAM_IN_HW_PROCESSING whatever the err is */
	zram_clear_flag(zram, index, ZRAM_IN_HW_PROCESSING);
	atomic64_dec(&zram->stats.hw_inuse);

	zram_slot_unlock(zram, index);

	if (err) {
		pr_info("%s: error (%d) occurs for index (%u)\n", __func__, err, index);
		atomic64_inc(&zram->stats.hw_failed_reads);

#if IS_ENABLED(CONFIG_HWCOMP_SUPPORT_NO_DST_COPY)
		/* Copy HW compressed data to zspool if necessary */
		if (zram->ops->table_entry_sz == sizeof(struct zram_table_entry_ndc)) {
			if (__hwcomp_buf_to_zspool(zram, index) < 0) {
				pr_info("%s: translation for SW decompression fail\n", __func__);
				goto out;
			}
		}
#endif
		/* Fallback to SW decompression */
		if (zram_read_page(zram, pp_info->page, index, NULL)) {
			pr_info("%s: fallback to SW decompression fail\n", __func__);
			goto out;
		}
	}

	flush_dcache_page(pp_info->page);

	/* If there is someone in wait, try to wake it up */
	if (wq_has_sleeper(&zram->hw_wait) && (atomic64_read(&zram->stats.hw_inuse) == 0))
		wake_up(&zram->hw_wait);

	/* Call bio_endio to decrease one reference to bio from bio_inc_remaining */
	bio_endio(bio);
	return;

out:
	/* Return as bio error */
	atomic64_inc(&zram->stats.failed_reads);
	bio_io_error(bio);
}

/* Used to detect same page */
static inline bool zram_detect_samepage(struct page *page, unsigned long *element)
{
	void *mem;
	bool samepage = false;

	mem = kmap_local_page(page);
	if (page_same_filled(mem, element))
		samepage = true;

	kunmap_local(mem);

	return samepage;
}

#if IS_ENABLED(CONFIG_HWCOMP_SUPPORT_NO_DST_COPY)

/**************************************
 *** Functions for No-DST copy mode ***
 **************************************/

static void zram_set_hw_compressed(struct zram *zram, u32 index, struct hwcomp_buf_t *entry)
{
	memcpy(&(ZRAM_TE_NDC(zram, index))->compressed, entry, sizeof(struct hwcomp_buf_t));
}

static void zram_get_hw_compressed(struct zram *zram, u32 index, struct hwcomp_buf_t *entry)
{
	memcpy(entry, &(ZRAM_TE_NDC(zram, index))->compressed,	sizeof(struct hwcomp_buf_t));
}

/* Convert hwcomp_buf_t to zspool buffer and release hwcomp_buf_t one(s) */
static int __hwcomp_buf_to_zspool(struct zram *zram, u32 index)
{
	unsigned int comp_len;
	struct hwcomp_buf_t src;
	unsigned long handle = -ENOMEM;
	void *dst;
	int ret = 0;

#ifdef ZRAM_ENGINE_DEBUG
	pr_info("%s\n", __func__);
#endif
	zram_slot_lock(zram, index);
	comp_len = zram_get_obj_size(zram, index);
	zram_slot_unlock(zram, index);

	/* Allocate zspool memory */
	handle = alloc_zspool_memory(zram, comp_len);
	if (IS_ERR_VALUE(handle)) {
		pr_info("%s: failed to allocate zspool memory.\n", __func__);
		return -ENOMEM;
	}

	zram_slot_lock(zram, index);

	/* Confirm whether it involves hwcomp_buf_t */
	if (!zram_hw_compress(zram, index)) {
		ret = 0;
		goto free_handle;
	}

	/* Start converting */
	comp_len = zram_get_obj_size(zram, index);
	zram_get_hw_compressed(zram, index, &src);
	dst = zs_map_object(zram->mem_pool, handle, ZS_MM_WO);
	ret = hwcomp_buf_read(dst, &src, comp_len);
	zs_unmap_object(zram->mem_pool, handle);
	if (ret) {
		pr_info("%s: failed to convert buffer: %d.\n", __func__, ret);
		ret = -EINVAL;
		goto free_handle;
	}

	/* Not involving hwcomp_buf_t now */
	zram_clear_flag(zram, index, ZRAM_HW_COMPRESS);

	/* Update handle */
	zram_set_handle(zram, index, handle);
	zram_slot_unlock(zram, index);

	/* Release hwcomp_buf */
	hwcomp_buf_destroy(&src);
	return 0;

free_handle:

	zs_free(zram->mem_pool, handle);
	zram_slot_unlock(zram, index);
	return ret;
}

static int hwcomp_buf_to_zspool(struct zram *zram, u32 index)
{
	bool hw_compress;
	int ret = 0;

	zram_slot_lock(zram, index);
	hw_compress = zram_hw_compress(zram, index);
	zram_slot_unlock(zram, index);

	if (hw_compress)
		ret = __hwcomp_buf_to_zspool(zram, index);

	return ret;
}

/*
 * (No-DST-Copy) There are different usages on "buffer" -
 *
 * a. "buffer" stands for the entry of "struct hwcomp_buf_t *" if pp_info->flag is HWCOMP_NORMAL
 * b. "buffer" is NULL if pp_info->flag is not HWCOMP_NORMAL
 */
static void hwcomp_compress_post_process_ndc(int err, void *buffer, unsigned int comp_len,
				struct comp_pp_info *pp_info, enum hwcomp_flags flag)
{
	struct zram *zram = pp_info->zram;
	u32 index = pp_info->index;
	struct bio *bio = pp_info->bio;
	unsigned long handle = -ENOMEM;

	/* zram should not be NULL here */
	if (unlikely(!zram)) {
		pr_info("%s: Empty ZRAM for index (%u) at (%d)\n", __func__, index, err);
		return;
	}

	/* Check bio */
	if (unlikely(!bio)) {
		pr_info("%s: Empty BIO for index (%u) at (%d)\n", __func__, index, err);
		return;
	}

	/* bio has been acquired, it's ok to reset the field to null here */
	pp_info->bio = NULL;

	if (err) {
		atomic64_inc(&zram->stats.hw_failed_writes);

		/* Fallback to SW compression */
		if (zram_write_page(zram, pp_info->page, index)) {
			pr_info("%s: fallback to SW compression fail\n", __func__);
			goto out;
		}

		goto fallback_success;
	}

	/* Copy incompressible page to zspool */
	if (flag == HWCOMP_HUGE) {

		/* HW views it as huge page */
		atomic64_inc(&zram->stats.hw_huge_pages_since);

		/* Make sure comp_len is PAGE_SIZE for HWCOMP_HUGE */
		comp_len = PAGE_SIZE;
		handle = hwcomp_copy_to_zspool(zram, buffer, pp_info->page, comp_len);
		if (IS_ERR_VALUE(handle))
			goto out;
	}

	/*
	 * Free memory associated with this sector
	 * before overwriting unused sectors.
	 */
	zram_slot_lock(zram, index);
	zram_free_page(zram, index);

	/* Update zram table entry */
	if (flag == HWCOMP_NORMAL || flag == HWCOMP_HUGE) {

		/* Set compressed info to zram table */
		if (flag == HWCOMP_NORMAL) {
			zram_set_hw_compressed(zram, index, (struct hwcomp_buf_t *)buffer);
			zram_set_flag(zram, index, ZRAM_HW_COMPRESS);
		} else {
			zram_set_huge_for_pp(zram, index);

			/* Set handle from pool */
			zram_set_handle(zram, index, handle);
		}

		/* Common setting */
		zram_set_obj_size(zram, index, comp_len);
		atomic64_add(comp_len, &zram->stats.compr_data_size);

	} else if (flag == HWCOMP_SAME) {

		/* Set ZRAM_SAME */
		zram_set_flag(zram, index, ZRAM_SAME);

		/* Increase the count of same pages */
		atomic64_inc(&zram->stats.same_pages);

		/* Set repeat_pattern */
		zram_set_element(zram, index, pp_info->repeat_pattern);

	} else {

		/* Invalid case */
		zram_slot_unlock(zram, index);
		WARN_ON_ONCE(1);
		goto out;
	}

	zram_accessed(zram, index);
	zram_slot_unlock(zram, index);

	/* Update stats */
	atomic64_inc(&zram->stats.pages_stored);

fallback_success:

	/* Call bio_endio to decrease one reference to bio from bio_inc_remaining */
	bio_endio(bio);
	return;

out:
	/* Return as bio error */
	atomic64_inc(&zram->stats.failed_writes);
	bio_io_error(bio);
}

static int zram_hw_bvec_read_ndc(struct zram *zram, struct bio_vec *bvec,
			  u32 index, int offset, struct bio *bio, bool wait)
{
	/*
	 * This is used to pass the information of compressed buffers to HW.
	 * Not allowed to be referenced during whole HW decompression.
	 * It should be safely released after return from hwcomp_decompress_page.
	 */
	struct hwcomp_buf_t src;
	unsigned int comp_len;
	int ret;
	struct dcomp_pp_info pp_info = {
		/* .pp_cb = hwcomp_decompress_post_process, */
		.zram = zram,
		.index = index,
		.page = bvec->bv_page,	/* dst page */
		.bio = bio,
	};

	if (is_partial_io(bvec)) {
		pr_info("%s: Partial IO is not supported.\n", __func__);
		return -EINVAL;
	}

	zram_slot_lock(zram, index);

	/* Confirm whether it is HW compress */
	if (!zram_hw_compress(zram, index)) {
		zram_slot_unlock(zram, index);
		return -EINVAL;
	}

	comp_len = zram_get_obj_size(zram, index);
	zram_get_hw_compressed(zram, index, &src);

	/* Tell SW it's under HW processing now */
	zram_set_flag(zram, index, ZRAM_IN_HW_PROCESSING);
	atomic64_inc(&zram->stats.hw_inuse);

	zram_slot_unlock(zram, index);

again:
	/* Asynchronous. Check whether HW can accept this request. */
	ret = hwcomp_decompress_page(zram->priv, &src, comp_len, bvec->bv_page, &pp_info, NULL);
	if (ret == 0)
		return 0;

	/* Wait for HW available */
	if (ret == -EBUSY) {
		if (wait) {
#ifdef ZRAM_ENGINE_DEBUG
			pr_info_ratelimited("%s: HW is busy, waiting for available.\n", __func__);
#endif
			atomic64_inc(&zram->stats.hw_dec_busy_wait);

			/* HW is busy. Relinquish CPU to take a breath. */
			WAIT_FOR_HWDCOMP();

			goto again;
		}

		atomic64_inc(&zram->stats.hw_dec_busy);
	}

	/* Unfortunately, HW can't handle this request properly. Clear related status */
	zram_slot_lock(zram, index);
	zram_clear_flag(zram, index, ZRAM_IN_HW_PROCESSING);
	atomic64_dec(&zram->stats.hw_inuse);
	zram_slot_unlock(zram, index);
	return ret;
}

/* When "wait" is true, the request will wait until HW is available */
static int zram_hw_bvec_write_ndc(struct zram *zram, struct bio_vec *bvec,
			   u32 index, int offset, struct bio *bio, bool wait)
{
	int ret;
	struct comp_pp_info pp_info = {
		/* .pp_cb = hwcomp_compress_post_process_ndc, */
		.zram = zram,
		.index = index,
		.page = bvec->bv_page,
		.bio = bio,
	};
	unsigned long element = 0;

	if (is_partial_io(bvec)) {
		pr_info("%s: Partial IO is not supported.\n", __func__);
		return -EINVAL;
	}

	/* Same page detection */
	if (zram_detect_samepage(bvec->bv_page, &element)) {
		atomic64_inc(&zram->stats.same_pages);
		zram_slot_lock(zram, index);
		zram_free_page(zram, index);
		zram_set_flag(zram, index, ZRAM_SAME);
		zram_set_element(zram, index, element);
		zram_slot_unlock(zram, index);
		atomic64_inc(&zram->stats.pages_stored);
		return 0;
	}

again:
	/* Asynchronous. Check whether HW can accept this request. */
	ret = hwcomp_compress_page(zram->priv, bvec->bv_page, &pp_info);
	if (ret == 0)
		return 0;

	/* Wait for HW available */
	if (ret == -EBUSY) {
		if (wait) {
#ifdef ZRAM_ENGINE_DEBUG
			pr_info_ratelimited("%s: HW is busy, waiting for available.\n", __func__);
#endif
			/* Don't block current task if it is exiting */
			if (current->flags & PF_EXITING)
				return ret;

			atomic64_inc(&zram->stats.hw_busy_wait);

			/* HW is busy. Relinquish CPU to take a breath. */
			WAIT_FOR_HWCOMP();

			goto again;
		}

		atomic64_inc(&zram->stats.hw_busy);
	}

	return ret;
}

static void zram_hybrid_bio_read_ndc(struct zram *zram, struct bio *bio)
{
	unsigned long start_time = bio_start_io_acct(bio);
	struct bvec_iter iter = bio->bi_iter;
	bool hw_compress;

	do {
		u32 index = iter.bi_sector >> SECTORS_PER_PAGE_SHIFT;
		u32 offset = (iter.bi_sector & (SECTORS_PER_PAGE - 1)) <<
				SECTOR_SHIFT;
		struct bio_vec bv = bio_iter_iovec(bio, iter);

		bv.bv_len = min_t(u32, bv.bv_len, PAGE_SIZE - offset);

		/* Check whether it should go to SW flow */
		zram_slot_lock(zram, index);
		hw_compress = zram_hw_compress(zram, index);
		zram_slot_unlock(zram, index);

		/* Go to HW decompression - asynchronous */
		if (!hw_compress ||
				zram_hw_bvec_read_ndc(zram, &bv, index, offset, bio, false) < 0) {

			/* Copy HW compressed data to zspool if needed */
			if (hw_compress && __hwcomp_buf_to_zspool(zram, index) < 0) {
				atomic64_inc(&zram->stats.failed_reads);
				bio->bi_status = BLK_STS_IOERR;
				break;
			}

			/* Fallback to SW flow */
			if (zram_bvec_read(zram, &bv, index, offset, bio) < 0) {
				atomic64_inc(&zram->stats.failed_reads);
				bio->bi_status = BLK_STS_IOERR;
				break;
			}
			flush_dcache_page(bv.bv_page);

			zram_slot_lock(zram, index);
			zram_accessed(zram, index);
			zram_slot_unlock(zram, index);
		}

		bio_advance_iter_single(bio, &iter, bv.bv_len);
	} while (iter.bi_size);

	bio_end_io_acct(bio, start_time);
	bio_endio(bio);
}

/*
 * Corresponding ZRAM slot should be locked.
 * (Slot with ZRAM_SAME won't be here)
 */
static void zram_free_hw_entry_ndc(struct zram *zram, size_t index)
{
	struct hwcomp_buf_t entry;
	unsigned long handle;

	/* Call zs_free if no hwcomp_buf_t involved */
	if (!zram_hw_compress(zram, index)) {
		handle = zram_get_handle(zram, index);
		if (!handle) {
			WARN_ON_ONCE(1);
			return;
		}

		zs_free(zram->mem_pool, handle);
		return;
	}

	/* Copy it to into stack buf with boundary for safety (directly accessed by HW impl is not allowed) */
	zram_get_hw_compressed(zram, index, &entry);
	hwcomp_buf_destroy(&entry);

	/* Clear HW_COMPRESS flag */
	zram_clear_flag(zram, index, ZRAM_HW_COMPRESS);
}

#endif /* CONFIG_HWCOMP_SUPPORT_NO_DST_COPY */

/***********************************
 *** Functions for DST copy mode ***
 ***********************************/

/* Copy compressed data from zspool to hwcomp buffer */
static int zspool_to_hwcomp_buffer(struct dcomp_pp_info *obj, void *buffer, unsigned long handle,
		unsigned int slen, unsigned int copysz_aligned)
{
	struct zram *zram = obj->zram;
	void *src;
	unsigned int new_slen = ALIGN(slen, copysz_aligned);

	if (unlikely(!zram))
		return -1;

	if (new_slen > PAGE_SIZE)
		return -1;

	src = zs_map_object(zram->mem_pool, handle, ZS_MM_RO);
	memcpy(buffer, src, slen);
	zs_unmap_object(zram->mem_pool, handle);

	/* Padding buffer with 0 to copysz_aligned */
	if (new_slen > slen)
		memset(buffer + slen, 0x0, new_slen - slen);

	return 0;
}

/*
 * (DST-Copy) There are different usages on "buffer" -
 *
 * a. "buffer" stands for the DST buffer containing compressed data if pp_info->flag is HWCOMP_NORMAL
 * b. "buffer" is NULL if pp_info->flag is not HWCOMP_NORMAL
 */
static void hwcomp_compress_post_process_dc(int err, void *buffer, unsigned int comp_len,
				struct comp_pp_info *pp_info, enum hwcomp_flags flag)
{
	struct zram *zram = pp_info->zram;
	u32 index = pp_info->index;
	struct bio *bio = pp_info->bio;
	unsigned long handle = -ENOMEM;

	/* zram should not be NULL here */
	if (unlikely(!zram)) {
		pr_info("%s: Empty ZRAM for index (%u) at (%d)\n", __func__, index, err);
		return;
	}

	/* Check bio */
	if (unlikely(!bio)) {
		pr_info("%s: Empty BIO for index (%u) at (%d)\n", __func__, index, err);
		return;
	}

	/* bio has been acquired, it's ok to reset the field to null here */
	pp_info->bio = NULL;

	if (err) {
		atomic64_inc(&zram->stats.hw_failed_writes);

		/* Fallback to SW compression */
		if (zram_write_page(zram, pp_info->page, index)) {
			pr_info_ratelimited("%s: fallback to SW compression fail\n", __func__);
			goto out;
		}

		goto fallback_success;
	}

	/* Copy compressed/incompressible page to zspool */
	if (flag != HWCOMP_SAME) {

		/* HW views it as huge page */
		if (flag == HWCOMP_HUGE)
			atomic64_inc(&zram->stats.hw_huge_pages_since);

		/* Make sure comp_len is PAGE_SIZE for HWCOMP_HUGE or if comp_len >= huge_class_size */
		if (comp_len >= huge_class_size || flag == HWCOMP_HUGE)
			comp_len = PAGE_SIZE;
		handle = hwcomp_copy_to_zspool(zram, buffer, pp_info->page, comp_len);
		if (IS_ERR_VALUE(handle))
			goto out;
	}

	/*
	 * Free memory associated with this sector
	 * before overwriting unused sectors.
	 */
	zram_slot_lock(zram, index);
	zram_free_page(zram, index);

	/* Update zram table entry */
	if (flag == HWCOMP_NORMAL || flag == HWCOMP_HUGE) {

		/* Check whether it is huge page (>= huge_class_size or HWCOMP_HUGE) */
		if (comp_len == PAGE_SIZE)
			zram_set_huge_for_pp(zram, index);
		else
			zram_set_flag(zram, index, ZRAM_HW_COMPRESS);

		/* Set handle from pool */
		zram_set_handle(zram, index, handle);

		/* Common setting */
		zram_set_obj_size(zram, index, comp_len);
		atomic64_add(comp_len, &zram->stats.compr_data_size);

	} else if (flag == HWCOMP_SAME) {

		/* Set ZRAM_SAME */
		zram_set_flag(zram, index, ZRAM_SAME);

		/* Increase the count of same pages */
		atomic64_inc(&zram->stats.same_pages);

		/* Set repeat_pattern */
		zram_set_element(zram, index, pp_info->repeat_pattern);

	} else {

		/* Invalid case */
		zram_slot_unlock(zram, index);
		WARN_ON_ONCE(1);
		goto out;
	}

	zram_accessed(zram, index);
	zram_slot_unlock(zram, index);

	/* Mark hwzram not busy if necessary */
	mark_hwzram_not_busy();

	/* Update stats */
	atomic64_inc(&zram->stats.pages_stored);

fallback_success:

	/* Call bio_endio to decrease one reference to bio from bio_inc_remaining */
	bio_endio(bio);
	return;

out:
	/* Return as bio error */
	atomic64_inc(&zram->stats.failed_writes);
	bio_io_error(bio);
}

static int zram_hw_bvec_read_dc(struct zram *zram, struct bio_vec *bvec,
			  u32 index, int offset, struct bio *bio, bool wait)
{
	unsigned long handle;
	unsigned int comp_len;
	int ret;
	struct dcomp_pp_info pp_info = {
		/* .pp_cb = hwcomp_decompress_post_process, */
		.zram = zram,
		.index = index,
		.page = bvec->bv_page,	/* dst page */
		.bio = bio,
	};

	if (is_partial_io(bvec)) {
		pr_info("%s: Partial IO is not supported.\n", __func__);
		return -EINVAL;
	}

	zram_slot_lock(zram, index);

	/* Confirm whether it is HW compress */
	if (!zram_hw_compress(zram, index)) {
		zram_slot_unlock(zram, index);
		return -EINVAL;
	}

	comp_len = zram_get_obj_size(zram, index);
	/* handle is valid (not 0 or error) here when ZRAM_HW_COMPRESS is set */
	handle = zram_get_handle(zram, index);

	/* Tell SW it's under HW processing now */
	zram_set_flag(zram, index, ZRAM_IN_HW_PROCESSING);
	atomic64_inc(&zram->stats.hw_inuse);

	zram_slot_unlock(zram, index);

again:
	/* Asynchronous. Check whether HW can accept this request. */
	ret = hwcomp_decompress_page(zram->priv, (void *)handle, comp_len, bvec->bv_page,
				&pp_info, zspool_to_hwcomp_buffer);
	if (ret == 0)
		return 0;

	/* Wait for HW available */
	if (ret == -EBUSY) {
		if (wait) {
#ifdef ZRAM_ENGINE_DEBUG
			pr_info_ratelimited("%s: HW is busy, waiting for available.\n", __func__);
#endif
			atomic64_inc(&zram->stats.hw_dec_busy_wait);

			/* HW is busy. Relinquish CPU to take a breath. */
			WAIT_FOR_HWDCOMP();

			goto again;
		}

		atomic64_inc(&zram->stats.hw_dec_busy);
	}

	/* Unfortunately, HW can't handle this request properly. Clear related status */
	zram_slot_lock(zram, index);
	zram_clear_flag(zram, index, ZRAM_IN_HW_PROCESSING);
	atomic64_dec(&zram->stats.hw_inuse);
	zram_slot_unlock(zram, index);

	/*
	 * Compressed data is copied to HW buffer after return from hwcomp_decompress_page.
	 */

	return ret;
}

/* When "wait" is true, the request will wait until HW is available */
static int zram_hw_bvec_write_dc(struct zram *zram, struct bio_vec *bvec,
			   u32 index, int offset, struct bio *bio, bool wait)
{
	int ret;
	struct comp_pp_info pp_info = {
		/* .pp_cb = hwcomp_compress_post_process_dc, */
		.zram = zram,
		.index = index,
		.page = bvec->bv_page,
		.bio = bio,
	};
	unsigned long element = 0;

	if (is_partial_io(bvec)) {
		pr_info("%s: Partial IO is not supported.\n", __func__);
		return -EINVAL;
	}

	/* Same page detection */
	if (zram_detect_samepage(bvec->bv_page, &element)) {
		atomic64_inc(&zram->stats.same_pages);
		zram_slot_lock(zram, index);
		zram_free_page(zram, index);
		zram_set_flag(zram, index, ZRAM_SAME);
		zram_set_element(zram, index, element);
		zram_slot_unlock(zram, index);
		atomic64_inc(&zram->stats.pages_stored);
		return 0;
	}

again:
	/* Asynchronous. Check whether HW can accept this request. */
	ret = hwcomp_compress_page(zram->priv, bvec->bv_page, &pp_info);
	if (ret == 0)
		return 0;

	/* Wait for HW available */
	if (ret == -EBUSY) {

		/* Mark hwzram busy if necessary */
		mark_hwzram_busy();

		if (wait) {
#ifdef ZRAM_ENGINE_DEBUG
			pr_info_ratelimited("%s: HW is busy, waiting for available.\n", __func__);
#endif
			/* Don't block current task if it is exiting */
			if (current->flags & PF_EXITING)
				return ret;

			atomic64_inc(&zram->stats.hw_busy_wait);

			/* HW is busy. Relinquish CPU to take a breath. */
			WAIT_FOR_HWCOMP();

			goto again;
		}

		atomic64_inc(&zram->stats.hw_busy);
	}

	return ret;
}

#if 0
static void zram_hybrid_bio_read_dc(struct zram *zram, struct bio *bio)
{
	unsigned long start_time = bio_start_io_acct(bio);
	struct bvec_iter iter = bio->bi_iter;
	bool hw_compress;

	do {
		u32 index = iter.bi_sector >> SECTORS_PER_PAGE_SHIFT;
		u32 offset = (iter.bi_sector & (SECTORS_PER_PAGE - 1)) <<
				SECTOR_SHIFT;
		struct bio_vec bv = bio_iter_iovec(bio, iter);

		bv.bv_len = min_t(u32, bv.bv_len, PAGE_SIZE - offset);

		/* Check whether it should go to SW flow */
		zram_slot_lock(zram, index);
		hw_compress = zram_hw_compress(zram, index);
		zram_slot_unlock(zram, index);

		/* Go to HW decompression - asynchronous */
		if (!hw_compress ||
				zram_hw_bvec_read_dc(zram, &bv, index, offset, bio, false) < 0) {

			/* Fallback to SW flow */
			if (zram_bvec_read(zram, &bv, index, offset, bio) < 0) {
				atomic64_inc(&zram->stats.failed_reads);
				bio->bi_status = BLK_STS_IOERR;
				break;
			}
			flush_dcache_page(bv.bv_page);

			zram_slot_lock(zram, index);
			zram_accessed(zram, index);
			zram_slot_unlock(zram, index);
		}

		bio_advance_iter_single(bio, &iter, bv.bv_len);
	} while (iter.bi_size);

	bio_end_io_acct(bio, start_time);
	bio_endio(bio);
}
#endif

/*
 * Corresponding ZRAM slot should be locked.
 * (Slot with ZRAM_SAME won't be here)
 */
static void zram_free_hw_entry_dc(struct zram *zram, size_t index)
{
	unsigned long handle;

#ifdef ZRAM_ENGINE_DEBUG
	pr_info("%s\n", __func__);
#endif
	/* Clear HW_COMPRESS flag */
	if (zram_hw_compress(zram, index))
		zram_clear_flag(zram, index, ZRAM_HW_COMPRESS);

	handle = zram_get_handle(zram, index);
	if (!handle) {
		WARN_ON_ONCE(1);
		return;
	}

	zs_free(zram->mem_pool, handle);
}

/*********************************************************************
 *** Common Functions both for DST and No-DST copy modes (Part-II) ***
 *********************************************************************/

static void zram_hwonly_bio_read(struct zram *zram, struct bio *bio)
{
	unsigned long start_time = bio_start_io_acct(bio);
	struct bvec_iter iter = bio->bi_iter;
	bool hw_compress;
	const struct zram_mode_operations *ops = zram->ops;

	do {
		u32 index = iter.bi_sector >> SECTORS_PER_PAGE_SHIFT;
		u32 offset = (iter.bi_sector & (SECTORS_PER_PAGE - 1)) <<
				SECTOR_SHIFT;
		struct bio_vec bv = bio_iter_iovec(bio, iter);

		bv.bv_len = min_t(u32, bv.bv_len, PAGE_SIZE - offset);

		/* Check whether it should go to SW flow */
		zram_slot_lock(zram, index);
		hw_compress = zram_hw_compress(zram, index);
		zram_slot_unlock(zram, index);

		if (hw_compress) {
			/* Go to HW decompression - asynchronous */
			if (ops->hw_bvec_read(zram, &bv, index, offset, bio, true) < 0) {
				atomic64_inc(&zram->stats.failed_reads);
				bio->bi_status = BLK_STS_IOERR;
				break;
			}
		} else {
			/* Go to SW flow */
			if (zram_bvec_read(zram, &bv, index, offset, bio) < 0) {
				atomic64_inc(&zram->stats.failed_reads);
				bio->bi_status = BLK_STS_IOERR;
				break;
			}
			flush_dcache_page(bv.bv_page);
		}

		zram_slot_lock(zram, index);
		zram_accessed(zram, index);
		zram_slot_unlock(zram, index);

		bio_advance_iter_single(bio, &iter, bv.bv_len);
	} while (iter.bi_size);

	bio_end_io_acct(bio, start_time);
	bio_endio(bio);
}

static void zram_hybrid_bio_write(struct zram *zram, struct bio *bio)
{
	unsigned long start_time = bio_start_io_acct(bio);
	struct bvec_iter iter = bio->bi_iter;
	const struct zram_mode_operations *ops = zram->ops;

	do {
		u32 index = iter.bi_sector >> SECTORS_PER_PAGE_SHIFT;
		u32 offset = (iter.bi_sector & (SECTORS_PER_PAGE - 1)) <<
				SECTOR_SHIFT;
		struct bio_vec bv = bio_iter_iovec(bio, iter);

		bv.bv_len = min_t(u32, bv.bv_len, PAGE_SIZE - offset);

		/* HW compression - asynchronous */
		if (ops->hw_bvec_write(zram, &bv, index, offset, bio, false) < 0) {

			/* Failed to add request to HW, fallback to SW compression */
			if (zram_bvec_write(zram, &bv, index, offset, bio) < 0) {
				atomic64_inc(&zram->stats.failed_writes);
				bio->bi_status = BLK_STS_IOERR;
				break;
			}

			zram_slot_lock(zram, index);
			zram_accessed(zram, index);
			zram_slot_unlock(zram, index);
		}

		bio_advance_iter_single(bio, &iter, bv.bv_len);
	} while (iter.bi_size);

	bio_end_io_acct(bio, start_time);
	bio_endio(bio);
}

static void zram_hwonly_bio_write(struct zram *zram, struct bio *bio)
{
	unsigned long start_time = bio_start_io_acct(bio);
	struct bvec_iter iter = bio->bi_iter;
	const struct zram_mode_operations *ops = zram->ops;

	do {
		u32 index = iter.bi_sector >> SECTORS_PER_PAGE_SHIFT;
		u32 offset = (iter.bi_sector & (SECTORS_PER_PAGE - 1)) <<
				SECTOR_SHIFT;
		struct bio_vec bv = bio_iter_iovec(bio, iter);

		bv.bv_len = min_t(u32, bv.bv_len, PAGE_SIZE - offset);

		/* HW compression - asynchronous */
		if (ops->hw_bvec_write(zram, &bv, index, offset, bio, true) < 0) {

			/* Failed to add request to HW, fallback to SW compression */
			if (zram_bvec_write(zram, &bv, index, offset, bio) < 0) {
				atomic64_inc(&zram->stats.failed_writes);
				bio->bi_status = BLK_STS_IOERR;
				break;
			}

			zram_slot_lock(zram, index);
			zram_accessed(zram, index);
			zram_slot_unlock(zram, index);
		}

		bio_advance_iter_single(bio, &iter, bv.bv_len);
	} while (iter.bi_size);

	bio_end_io_acct(bio, start_time);
	bio_endio(bio);
}

static int zram_init_hw_engine(struct zram *zram)
{
	void *hw_engine;
	int mode;
	compress_pp_fn comp_pp_cb = NULL;
	decompress_pp_fn dcomp_pp_cb = hwcomp_decompress_post_process;

	pr_info("%s\n", __func__);

	/* Switch to proper HW mode */
	if (!strncmp(zram->ops->name, "ndc", 3)) {
		mode = NO_DST_COPY_MODE;
#if IS_ENABLED(CONFIG_HWCOMP_SUPPORT_NO_DST_COPY)
		comp_pp_cb = hwcomp_compress_post_process_ndc;
#endif
	} else {
		mode = DST_COPY_MODE;
		comp_pp_cb = hwcomp_compress_post_process_dc;
	}

	hw_engine = hwcomp_create(mode, comp_pp_cb, dcomp_pp_cb);
	if (IS_ERR_OR_NULL(hw_engine))
		return -ENODEV;

	zram->priv = hw_engine;
	return 0;
}

static void zram_fini_hw_engine(struct zram *zram)
{
	void *hw_engine = zram->priv;

	pr_info("%s\n", __func__);

	zram->priv = NULL;

	hwcomp_destroy(hw_engine);
}

/* Callback for kcompressd */
static void zram_bio_write_callback(void *mem, struct bio *bio)
{
	struct zram *zram = (struct zram *)mem;

	zram_bio_write(zram, bio);
}

static void zram_kcompressd_hw_bio_write(struct zram *zram, struct bio *bio)
{
	if(is_hwzram_busy()) {
		/*
		 * When hw is busy, trying to send requests to kcompressd.
		 * If kcompressd is unable to receive the request, fallback
		 * to SW bio write.
		 */
		if(schedule_bio_write(zram, bio, zram_bio_write_callback))
			zram_bio_write(zram, bio);
	} else {
		/*
		 * Call hybrid_bio_write to make sure we have the chance to
		 * sending requests to kcompressd without busy waiting on HW.
		 */
		zram_hybrid_bio_write(zram, bio);
	}
}

static void zram_kcompressd_bio_write(struct zram *zram, struct bio *bio)
{
	/*
	 * Trying to send requests to kcompressd firstly. If kcompressd
	 * is unable to receive the request, fallback to SW bio write.
	 */
	if(schedule_bio_write(zram, bio, zram_bio_write_callback))
		zram_bio_write(zram, bio);
}

/*
 * The order is corresponding to the one in the comp_mode_series.
 */
const struct zram_mode_operations mode_ops[] = {

	/* Mode operations for DST-Copy mode (_dc). */
	[0] = {
		.name		= "hybrid",
		//.bio_read	= zram_hybrid_bio_read_dc,
		.bio_read	= zram_bio_read,
		.hw_bvec_read	= zram_hw_bvec_read_dc,
		.bio_write	= zram_hybrid_bio_write,
		.hw_bvec_write	= zram_hw_bvec_write_dc,
		.to_zspool	= NULL,
		.free_entry	= zram_free_hw_entry_dc,
		.wait_for_hw	= zram_wait_for_hw,
		.hwinit		= zram_init_hw_engine,
		.hwfini		= zram_fini_hw_engine,
		.default_swalg	= true,
		.table_entry_sz = sizeof(struct zram_table_entry),
	},

	[1] = {
		.name		= "hwonly",
		//.bio_read	= zram_hwonly_bio_read,
		.bio_read	= zram_bio_read,
		.hw_bvec_read	= zram_hw_bvec_read_dc,
		.bio_write	= zram_hwonly_bio_write,
		.hw_bvec_write	= zram_hw_bvec_write_dc,
		.to_zspool	= NULL,
		.free_entry	= zram_free_hw_entry_dc,
		.wait_for_hw	= zram_wait_for_hw,
		.hwinit		= zram_init_hw_engine,
		.hwfini		= zram_fini_hw_engine,
		.default_swalg	= true,
		.table_entry_sz = sizeof(struct zram_table_entry),
	},

	/* SW only */
	[2] = {
		.name		= "swonly",
		.bio_read	= zram_bio_read,
		.hw_bvec_read	= NULL,
		.bio_write	= zram_bio_write,
		.hw_bvec_write	= NULL,
		.to_zspool	= NULL,
		.free_entry	= NULL,
		.wait_for_hw	= NULL,
		.hwinit		= NULL,
		.hwfini		= NULL,
		.default_swalg	= false,
		.table_entry_sz = sizeof(struct zram_table_entry),
	},

	/* kcompressd:hw (similar to hybrid) */
	[3] = {
		.name		= "kcompressd:hw",
		//.bio_read	= zram_hybrid_bio_read_dc,
		.bio_read	= zram_bio_read,
		.hw_bvec_read	= zram_hw_bvec_read_dc,
		.bio_write	= zram_kcompressd_hw_bio_write,
		.hw_bvec_write	= zram_hw_bvec_write_dc,
		.to_zspool	= NULL,
		.free_entry	= zram_free_hw_entry_dc,
		.wait_for_hw	= zram_wait_for_hw,
		.hwinit		= zram_init_hw_engine,
		.hwfini		= zram_fini_hw_engine,
		.default_swalg	= true,
		.table_entry_sz = sizeof(struct zram_table_entry),
	},

	/* kcompressd (similar to swonly) */
	[4] = {
		.name		= "kcompressd",
		.bio_read	= zram_bio_read,
		.hw_bvec_read	= NULL,
		.bio_write	= zram_kcompressd_bio_write,
		.hw_bvec_write	= NULL,
		.to_zspool	= NULL,
		.free_entry	= NULL,
		.wait_for_hw	= NULL,
		.hwinit		= NULL,
		.hwfini		= NULL,
		.default_swalg	= false,
		.table_entry_sz = sizeof(struct zram_table_entry),
	},

#if IS_ENABLED(CONFIG_HWCOMP_SUPPORT_NO_DST_COPY)
	/* Mode operations for No-DST-Copy mode (_ndc). */
	[5] = {
		/* default mode */
		.name		= "ndc:hybrid",
		.bio_read	= zram_hybrid_bio_read_ndc,
		.hw_bvec_read	= zram_hw_bvec_read_ndc,
		.bio_write	= zram_hybrid_bio_write,
		.hw_bvec_write	= zram_hw_bvec_write_ndc,
		.to_zspool	= hwcomp_buf_to_zspool,
		.free_entry	= zram_free_hw_entry_ndc,
		.wait_for_hw	= zram_wait_for_hw,
		.hwinit		= zram_init_hw_engine,
		.hwfini		= zram_fini_hw_engine,
		.default_swalg	= true,
		.table_entry_sz = sizeof(struct zram_table_entry_ndc),
	},

	[6] = {
		.name		= "ndc:hwonly",
		.bio_read	= zram_hwonly_bio_read,
		.hw_bvec_read	= zram_hw_bvec_read_ndc,
		.bio_write	= zram_hwonly_bio_write,
		.hw_bvec_write	= zram_hw_bvec_write_ndc,
		.to_zspool	= hwcomp_buf_to_zspool,
		.free_entry	= zram_free_hw_entry_ndc,
		.wait_for_hw	= zram_wait_for_hw,
		.hwinit		= zram_init_hw_engine,
		.hwfini		= zram_fini_hw_engine,
		.default_swalg	= true,
		.table_entry_sz = sizeof(struct zram_table_entry_ndc),
	},
#endif
};

/*
 * Compression mode -
 * hybrid: HW compression first, fallback to SW compression if necessary.
 * hwonly: No fallback to SW except !zram_hw_compress.
 * swonly: Only SW flow.
 * kcompressd:hw: HW compression with kcompressd extension
 * kcompressd: SW compression with kcompressd extension
 * ndc:hybrid: hybrid with no dst copy
 * ndc:hwonly: hwonly with no dst copy
 */
static const char * const comp_mode_series[] = {
	"hybrid",
	"hwonly",
	"swonly",
	"kcompressd:hw",
	"kcompressd",
#if IS_ENABLED(CONFIG_HWCOMP_SUPPORT_NO_DST_COPY)
	"ndc:hybrid",
	"ndc:hwonly",
#endif
};

static ssize_t comp_mode_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct zram *zram = dev_to_zram(dev);
	ssize_t sz = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(comp_mode_series); i++) {
		if (!strcmp(zram->ops->name, comp_mode_series[i])) {
			sz += scnprintf(buf + sz, PAGE_SIZE - sz - 2,
					"[%s] ", comp_mode_series[i]);
		} else {
			sz += scnprintf(buf + sz, PAGE_SIZE - sz - 2,
					"%s ", comp_mode_series[i]);
		}
	}

	sz += scnprintf(buf + sz, PAGE_SIZE - sz, "\n");
	return sz;
}

static void comp_mode_set_algorithm(struct zram *zram)
{
	/* Override SW alg to default if necessary */
	if (zram->ops->default_swalg)
		comp_algorithm_set(zram, ZRAM_PRIMARY_COMP, default_compressor);
}

static void comp_mode_set(struct zram *zram, const char *mode)
{
	int i, ret = -1;

retry:
	for (i = 0; i < ARRAY_SIZE(comp_mode_series); i++) {
		if (!strcmp(mode, comp_mode_series[i])) {
			ret = 0;
			break;
		}
	}

	/* default setting */
	if (ret != 0) {
		pr_info("%s: use default mode: %s\n", __func__, default_mode);
		mode = default_mode;
		goto retry;
	}

	/* Determine operations */
	zram->ops = &mode_ops[i];

	/* Support BLK_FEAT_SYNCHRONOUS when it's swonly */
	if (!strcmp(mode, "swonly"))
		zram->disk->queue->limits.features |= BLK_FEAT_SYNCHRONOUS;

	/* Turn on kcompressd_enabled if necessary */
	if (!strcmp(mode, "kcompressd:hw") || !strcmp(mode, "kcompressd"))
		static_branch_enable(&kcompressd_enabled);

	/* Override SW algorithm if necessary before creating zcomp instance */
	comp_mode_set_algorithm(zram);
}

static ssize_t comp_mode_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf,
				    size_t len)
{
	struct zram *zram = dev_to_zram(dev);
	char *mode;
	size_t sz;

	sz = strlen(buf);
	if (sz >= ZRAM_MAX_COMP_MODE_NAME)
		return -E2BIG;

	mode = kstrdup(buf, GFP_KERNEL);
	if (!mode)
		return -ENOMEM;

	/* ignore trailing newline */
	if (sz > 0 && mode[sz - 1] == '\n')
		mode[sz - 1] = 0x00;

	down_write(&zram->init_lock);
	if (init_done(zram)) {
		up_write(&zram->init_lock);
		kfree(mode);
		pr_info("Can't change mode for initialized device\n");
		return -EBUSY;
	}

	comp_mode_set(zram, mode);
	up_write(&zram->init_lock);
	kfree(mode);
	return len;
}

/* For probing interesting tracepoints */
struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
};

#define ZRAM_SWP_READ_SYNCHRONOUS_IO	(1 << 12)	/* __SWP_READ_SYNCHRONOUS_IO */
static void probe_android_vh_adjust_swap_info_flags(void *ignore, unsigned long *flags)
{
	*flags |= ZRAM_SWP_READ_SYNCHRONOUS_IO;
	pr_info("%s: New swap info flags: %lx\n", __func__, *flags);
}

#define MAX_ALLOWABLE_BATCH_ALLOCATION_SIZE	(189)
static void probe_android_vh_rmqueue_pcplist_override_batch(void *ignore, int *batch)
{
	if (*batch > MAX_ALLOWABLE_BATCH_ALLOCATION_SIZE)
		*batch = MAX_ALLOWABLE_BATCH_ALLOCATION_SIZE;
}

static struct tracepoints_table zram_tracepoints[] = {
{.name = "android_vh_adjust_swap_info_flags", .func = probe_android_vh_adjust_swap_info_flags, .tp = NULL},
{.name = "android_vh_rmqueue_pcplist_override_batch", .func = probe_android_vh_rmqueue_pcplist_override_batch,
	.tp = NULL},
};

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(zram_tracepoints) / sizeof(struct tracepoints_table); i++)

static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(zram_tracepoints[i].name, tp->name) == 0)
			zram_tracepoints[i].tp = tp;
	}
}

/* Find out interesting tracepoints and try to register them. */
static void __init zram_hookup_tracepoints(void)
{
	int i, ret;

	/* Find out interesting tracepoints */
	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	/* Probing found tracepoints */
	FOR_EACH_INTEREST(i) {
		if (zram_tracepoints[i].tp == NULL) {
			pr_info("Error: %s not found\n", zram_tracepoints[i].name);
			continue;
		}

		ret = tracepoint_probe_register(zram_tracepoints[i].tp,	zram_tracepoints[i].func, NULL);
		if (ret) {
			pr_info("Failed to register %s\n", zram_tracepoints[i].name);

			/* Set tp as NULL to tell unhook function bypassing this tracepoint */
			zram_tracepoints[i].tp = NULL;
		}
	}
}

/* Unhook probed tracepoints and try to unregister them. */
static void __exit zram_unhook_tracepoints(void)
{
	int i, ret;

	/* Unhooking probed tracepoints */
	FOR_EACH_INTEREST(i) {
		if (zram_tracepoints[i].tp == NULL) {
			pr_info("%s is not probed\n", zram_tracepoints[i].name);
			continue;
		}

		ret = tracepoint_probe_unregister(zram_tracepoints[i].tp, zram_tracepoints[i].func, NULL);
		if (ret)
			pr_info("Failed to unregister %s\n", zram_tracepoints[i].name);

		/* Always set tp as NULL after unregister */
		zram_tracepoints[i].tp = NULL;
	}
}

/*
 * Handler function for all zram I/O requests.
 */
static void zram_submit_bio(struct bio *bio)
{
	struct zram *zram = bio->bi_bdev->bd_disk->private_data;

	switch (bio_op(bio)) {
	case REQ_OP_READ:
		zram->ops->bio_read(zram, bio);
		break;
	case REQ_OP_WRITE:
		zram->ops->bio_write(zram, bio);
		break;
	case REQ_OP_DISCARD:
	case REQ_OP_WRITE_ZEROES:
		zram_bio_discard(zram, bio);
		break;
	default:
		WARN_ON_ONCE(1);
		bio_endio(bio);
	}
}

static void zram_slot_free_notify(struct block_device *bdev,
				unsigned long index)
{
	struct zram *zram;

	zram = bdev->bd_disk->private_data;

	atomic64_inc(&zram->stats.notify_free);
	if (!zram_slot_trylock(zram, index)) {
		atomic64_inc(&zram->stats.miss_free);
		return;
	}

	/*
	 * If zram is processed by HW in the meantime, just bypass it.
	 * This slot may be used in the future compression, and it can
	 * be overwritten in that request.
	 */
	if (zram_in_hw_processing(zram, index)) {
		zram_slot_unlock(zram, index);
		atomic64_inc(&zram->stats.miss_free);
		return;
	}

	zram_free_page(zram, index);
	zram_slot_unlock(zram, index);
}

static void zram_destroy_comps(struct zram *zram)
{
	u32 prio;

	for (prio = 0; prio < ZRAM_MAX_COMPS; prio++) {
		struct zcomp *comp = zram->comps[prio];

		zram->comps[prio] = NULL;
		if (!comp)
			continue;
		zcomp_destroy(comp);
		zram->num_active_comps--;
	}
}

static void zram_reset_device(struct zram *zram)
{
	down_write(&zram->init_lock);

	zram->limit_pages = 0;

	if (!init_done(zram)) {
		up_write(&zram->init_lock);
		return;
	}

	set_capacity_and_notify(zram->disk, 0);
	part_stat_set_all(zram->disk->part0, 0);

	/* I/O operation under all of CPU are done so let's free */
	zram_meta_free(zram, zram->disksize);
	zram->disksize = 0;
	zram_destroy_comps(zram);
	memset(&zram->stats, 0, sizeof(zram->stats));
	reset_bdev(zram);

	comp_algorithm_set(zram, ZRAM_PRIMARY_COMP, default_compressor);

	/* Destroy HW instance if necessary */
	if (zram->ops->hwfini)
		zram->ops->hwfini(zram);

	comp_mode_set(zram, default_mode);
	up_write(&zram->init_lock);
}

static ssize_t disksize_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	u64 disksize;
	struct zcomp *comp;
	struct zram *zram = dev_to_zram(dev);
	int err;
	u32 prio;

	disksize = memparse(buf, NULL);
	if (!disksize)
		return -EINVAL;

	down_write(&zram->init_lock);
	if (init_done(zram)) {
		pr_info("Cannot change disksize for initialized device\n");
		err = -EBUSY;
		goto out_unlock;
	}

	disksize = PAGE_ALIGN(disksize);
	if (!zram_meta_alloc(zram, disksize)) {
		err = -ENOMEM;
		goto out_unlock;
	}

	/* Override SW algorithm if necessary before creating zcomp instance */
	comp_mode_set_algorithm(zram);

	for (prio = 0; prio < ZRAM_MAX_COMPS; prio++) {
		if (!zram->comp_algs[prio])
			continue;

		comp = zcomp_create(zram->comp_algs[prio]);
		if (IS_ERR(comp)) {
			pr_err("Cannot initialise %s compressing backend\n",
			       zram->comp_algs[prio]);
			err = PTR_ERR(comp);
			goto out_free_comps;
		}

		zram->comps[prio] = comp;
		zram->num_active_comps++;
	}

#if PAGE_SIZE != 4096
	/* Switch to swonly if not 4K page */
	comp_mode_set(zram, "swonly");
#endif

	/* Create HW instance if necessary */
	if (zram->ops->hwinit) {
		err = zram->ops->hwinit(zram);
		if (err) {
			pr_info("%s: failed to do hwinit (%d).\n", __func__, err);

			/* Switch to swonly */
			comp_mode_set(zram, "swonly");
		}
	}

	zram->disksize = disksize;
	set_capacity_and_notify(zram->disk, zram->disksize >> SECTOR_SHIFT);
	up_write(&zram->init_lock);

	return len;

out_free_comps:
	zram_destroy_comps(zram);
	zram_meta_free(zram, disksize);
out_unlock:
	up_write(&zram->init_lock);
	return err;
}

static ssize_t reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int ret;
	unsigned short do_reset;
	struct zram *zram;
	struct gendisk *disk;

	ret = kstrtou16(buf, 10, &do_reset);
	if (ret)
		return ret;

	if (!do_reset)
		return -EINVAL;

	zram = dev_to_zram(dev);
	disk = zram->disk;

	mutex_lock(&disk->open_mutex);
	/* Do not reset an active device or claimed device */
	if (disk_openers(disk) || zram->claim) {
		mutex_unlock(&disk->open_mutex);
		return -EBUSY;
	}

	/* From now on, anyone can't open /dev/zram[0-9] */
	zram->claim = true;
	mutex_unlock(&disk->open_mutex);

	/* Make sure all the pending I/O are finished */
	sync_blockdev(disk->part0);
	zram_reset_device(zram);

	mutex_lock(&disk->open_mutex);
	zram->claim = false;
	mutex_unlock(&disk->open_mutex);

	return len;
}

static int zram_open(struct gendisk *disk, blk_mode_t mode)
{
	struct zram *zram = disk->private_data;

	WARN_ON(!mutex_is_locked(&disk->open_mutex));

	/* zram was claimed to reset so open request fails */
	if (zram->claim)
		return -EBUSY;
	return 0;
}

static const struct block_device_operations zram_devops = {
	.open = zram_open,
	.submit_bio = zram_submit_bio,
	.swap_slot_free_notify = zram_slot_free_notify,
	.owner = THIS_MODULE
};

static DEVICE_ATTR_WO(compact);
static DEVICE_ATTR_RW(disksize);
static DEVICE_ATTR_RO(initstate);
static DEVICE_ATTR_WO(reset);
static DEVICE_ATTR_WO(mem_limit);
static DEVICE_ATTR_WO(mem_used_max);
static DEVICE_ATTR_WO(idle);
static DEVICE_ATTR_RW(max_comp_streams);
static DEVICE_ATTR_RW(comp_algorithm);
#ifdef CONFIG_ZRAM_WRITEBACK
static DEVICE_ATTR_RW(backing_dev);
static DEVICE_ATTR_WO(writeback);
static DEVICE_ATTR_RW(writeback_limit);
static DEVICE_ATTR_RW(writeback_limit_enable);
#endif
#ifdef CONFIG_ZRAM_MULTI_COMP
static DEVICE_ATTR_RW(recomp_algorithm);
static DEVICE_ATTR_WO(recompress);
#endif
static DEVICE_ATTR_RW(comp_mode);

static struct attribute *zram_disk_attrs[] = {
	&dev_attr_disksize.attr,
	&dev_attr_initstate.attr,
	&dev_attr_reset.attr,
	&dev_attr_compact.attr,
	&dev_attr_mem_limit.attr,
	&dev_attr_mem_used_max.attr,
	&dev_attr_idle.attr,
	&dev_attr_max_comp_streams.attr,
	&dev_attr_comp_algorithm.attr,
#ifdef CONFIG_ZRAM_WRITEBACK
	&dev_attr_backing_dev.attr,
	&dev_attr_writeback.attr,
	&dev_attr_writeback_limit.attr,
	&dev_attr_writeback_limit_enable.attr,
#endif
	&dev_attr_io_stat.attr,
	&dev_attr_mm_stat.attr,
#ifdef CONFIG_ZRAM_WRITEBACK
	&dev_attr_bd_stat.attr,
#endif
	&dev_attr_debug_stat.attr,
#ifdef CONFIG_ZRAM_MULTI_COMP
	&dev_attr_recomp_algorithm.attr,
	&dev_attr_recompress.attr,
#endif
	&dev_attr_comp_mode.attr,
	NULL,
};

ATTRIBUTE_GROUPS(zram_disk);

/*
 * Allocate and initialize new zram device. the function returns
 * '>= 0' device_id upon success, and negative value otherwise.
 */
static int zram_add(void)
{
	struct queue_limits lim = {
		.logical_block_size		= ZRAM_LOGICAL_BLOCK_SIZE,
		/*
		 * To ensure that we always get PAGE_SIZE aligned and
		 * n*PAGE_SIZED sized I/O requests.
		 */
		.physical_block_size		= PAGE_SIZE,
		.io_min				= PAGE_SIZE,
		.io_opt				= PAGE_SIZE,
		.max_hw_discard_sectors		= UINT_MAX,
		/*
		 * zram_bio_discard() will clear all logical blocks if logical
		 * block size is identical with physical block size(PAGE_SIZE).
		 * But if it is different, we will skip discarding some parts of
		 * logical blocks in the part of the request range which isn't
		 * aligned to physical block size.  So we can't ensure that all
		 * discarded logical blocks are zeroed.
		 */
#if ZRAM_LOGICAL_BLOCK_SIZE == PAGE_SIZE
		.max_write_zeroes_sectors	= UINT_MAX,
#endif
		.features			= BLK_FEAT_STABLE_WRITES,
	};
	struct zram *zram;
	int ret, device_id;

	zram = kzalloc(sizeof(struct zram), GFP_KERNEL);
	if (!zram)
		return -ENOMEM;

	ret = idr_alloc(&zram_index_idr, zram, 0, 0, GFP_KERNEL);
	if (ret < 0)
		goto out_free_dev;
	device_id = ret;

	init_rwsem(&zram->init_lock);
#ifdef CONFIG_ZRAM_WRITEBACK
	spin_lock_init(&zram->wb_limit_lock);
#endif

	/* gendisk structure */
	zram->disk = blk_alloc_disk(&lim, NUMA_NO_NODE);
	if (IS_ERR(zram->disk)) {
		pr_err("Error allocating disk structure for device %d\n",
			device_id);
		ret = PTR_ERR(zram->disk);
		goto out_free_idr;
	}

	zram->disk->major = zram_major;
	zram->disk->first_minor = device_id;
	zram->disk->minors = 1;
	zram->disk->flags |= GENHD_FL_NO_PART;
	zram->disk->fops = &zram_devops;
	zram->disk->private_data = zram;
	snprintf(zram->disk->disk_name, 16, "zram%d", device_id);

	/* Actual capacity set using sysfs (/sys/block/zram<id>/disksize */
	set_capacity(zram->disk, 0);
	ret = device_add_disk(NULL, zram->disk, zram_disk_groups);
	if (ret)
		goto out_cleanup_disk;

	comp_algorithm_set(zram, ZRAM_PRIMARY_COMP, default_compressor);
	comp_mode_set(zram, default_mode);

	init_waitqueue_head(&zram->hw_wait);

	zram_debugfs_register(zram);
	pr_info("Added device: %s\n", zram->disk->disk_name);
	return device_id;

out_cleanup_disk:
	put_disk(zram->disk);
out_free_idr:
	idr_remove(&zram_index_idr, device_id);
out_free_dev:
	kfree(zram);
	return ret;
}

static int zram_remove(struct zram *zram)
{
	bool claimed;

	mutex_lock(&zram->disk->open_mutex);
	if (disk_openers(zram->disk)) {
		mutex_unlock(&zram->disk->open_mutex);
		return -EBUSY;
	}

	claimed = zram->claim;
	if (!claimed)
		zram->claim = true;
	mutex_unlock(&zram->disk->open_mutex);

	zram_debugfs_unregister(zram);

	if (claimed) {
		/*
		 * If we were claimed by reset_store(), del_gendisk() will
		 * wait until reset_store() is done, so nothing need to do.
		 */
		;
	} else {
		/* Make sure all the pending I/O are finished */
		sync_blockdev(zram->disk->part0);
		zram_reset_device(zram);
	}

	pr_info("Removed device: %s\n", zram->disk->disk_name);

	del_gendisk(zram->disk);

	/* del_gendisk drains pending reset_store */
	WARN_ON_ONCE(claimed && zram->claim);

	/*
	 * disksize_store() may be called in between zram_reset_device()
	 * and del_gendisk(), so run the last reset to avoid leaking
	 * anything allocated with disksize_store()
	 */
	zram_reset_device(zram);

	put_disk(zram->disk);
	kfree(zram);
	return 0;
}

/* zram-control sysfs attributes */

/*
 * NOTE: hot_add attribute is not the usual read-only sysfs attribute. In a
 * sense that reading from this file does alter the state of your system -- it
 * creates a new un-initialized zram device and returns back this device's
 * device_id (or an error code if it fails to create a new device).
 */
static ssize_t hot_add_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	int ret;

	mutex_lock(&zram_index_mutex);
	ret = zram_add();
	mutex_unlock(&zram_index_mutex);

	if (ret < 0)
		return ret;
	return scnprintf(buf, PAGE_SIZE, "%d\n", ret);
}
/* This attribute must be set to 0400, so CLASS_ATTR_RO() can not be used */
static struct class_attribute class_attr_hot_add =
	__ATTR(hot_add, 0400, hot_add_show, NULL);

static ssize_t hot_remove_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	struct zram *zram;
	int ret, dev_id;

	/* dev_id is gendisk->first_minor, which is `int' */
	ret = kstrtoint(buf, 10, &dev_id);
	if (ret)
		return ret;
	if (dev_id < 0)
		return -EINVAL;

	mutex_lock(&zram_index_mutex);

	zram = idr_find(&zram_index_idr, dev_id);
	if (zram) {
		ret = zram_remove(zram);
		if (!ret)
			idr_remove(&zram_index_idr, dev_id);
	} else {
		ret = -ENODEV;
	}

	mutex_unlock(&zram_index_mutex);
	return ret ? ret : count;
}
static CLASS_ATTR_WO(hot_remove);

static struct attribute *zram_control_class_attrs[] = {
	&class_attr_hot_add.attr,
	&class_attr_hot_remove.attr,
	NULL,
};
ATTRIBUTE_GROUPS(zram_control_class);

static struct class zram_control_class = {
	.name		= "zram-control",
	.class_groups	= zram_control_class_groups,
};

static int zram_remove_cb(int id, void *ptr, void *data)
{
	WARN_ON_ONCE(zram_remove(ptr));
	return 0;
}

static void destroy_devices(void)
{
	class_unregister(&zram_control_class);
	idr_for_each(&zram_index_idr, &zram_remove_cb, NULL);
	zram_debugfs_destroy();
	idr_destroy(&zram_index_idr);
	unregister_blkdev(zram_major, "zram");
	cpuhp_remove_multi_state(CPUHP_ZCOMP_PREPARE);
}

static int __init zram_init(void)
{
	int ret;

	BUILD_BUG_ON(__NR_ZRAM_PAGEFLAGS > BITS_PER_LONG);

	ret = cpuhp_setup_state_multi(CPUHP_ZCOMP_PREPARE, "block/zram:prepare",
				      zcomp_cpu_up_prepare, zcomp_cpu_dead);
	if (ret < 0)
		return ret;

	ret = class_register(&zram_control_class);
	if (ret) {
		pr_err("Unable to register zram-control class\n");
		cpuhp_remove_multi_state(CPUHP_ZCOMP_PREPARE);
		return ret;
	}

	zram_debugfs_create();
	zram_major = register_blkdev(0, "zram");
	if (zram_major <= 0) {
		pr_err("Unable to get major number\n");
		class_unregister(&zram_control_class);
		cpuhp_remove_multi_state(CPUHP_ZCOMP_PREPARE);
		return -EBUSY;
	}

	while (num_devices != 0) {
		mutex_lock(&zram_index_mutex);
		ret = zram_add();
		mutex_unlock(&zram_index_mutex);
		if (ret < 0)
			goto out_error;
		num_devices--;
	}

	/* Hook up related tracepoints */
	zram_hookup_tracepoints();

	return 0;

out_error:
	destroy_devices();
	return ret;
}

static void __exit zram_exit(void)
{
	/* Unhook probed tracepoints */
	zram_unhook_tracepoints();

	destroy_devices();
}

module_init(zram_init);
module_exit(zram_exit);

module_param(num_devices, uint, 0);
MODULE_PARM_DESC(num_devices, "Number of pre-created zram devices");

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Nitin Gupta <ngupta@vflare.org>");
MODULE_DESCRIPTION("Compressed RAM Block Device");
