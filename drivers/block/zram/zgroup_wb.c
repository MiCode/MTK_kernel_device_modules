// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright(c) 2024 X-Ring technologies Inc.
 */

#define pr_fmt(fmt) "zgroup: " fmt

#include <linux/blkdev.h>
#include <linux/fscrypt.h>
#include <linux/math.h>
#include <linux/memory.h>
#include <linux/key.h>
#include <linux/key-type.h>
#include <linux/scatterlist.h>
#include <linux/blk-crypto.h>
#include <crypto/skcipher.h>

#include "ilist.h"
#include "internal.h"
#include "zram_drv.h"
#include "zgroup.h"

/* MAX_KEYS = pow(ZGROUP_CHILD_PER_NODE, DEPTH - 1) * ZGROUP_KEYS_PER_NODE */
#define ZGROUP_NODE_SIZE	L1_CACHE_BYTES
#define ZGROUP_KEYS_PER_NODE	(ZGROUP_NODE_SIZE / sizeof(sector_t))
#define ZGROUP_CHILD_PER_NODE	(ZGROUP_KEYS_PER_NODE + 1)

/* Limit the backing device size to 64G */
#define ZGROUP_WB_MAX_TARGETS	32768

#define ZGROUP_SHRINK_NR	32

#define ZGROUP_POOL_NR		64

/* HACK: default segment size, calculated by f2fs DEFAULT_BLOCKS_PER_SEGMENT */
#define ZGROUP_DEF_SEG_SIZE	(_AC(512, UL) << PAGE_SHIFT)

#define ZGROUP_BITMAP_SIZE(nr)	(BITS_TO_LONGS(nr) * sizeof(unsigned long))

static struct workqueue_struct *zgroup_read_wq __read_mostly;
static struct workqueue_struct *zgroup_write_wq __read_mostly;
wait_queue_head_t zgroup_fault_wq;

static void zgroup_stats_wb_size_inc(struct zram_group *zgroup,
				     unsigned short gid, size_t size)
{
	atomic_inc(&zgroup->stats[gid].wb_pages);
	atomic_inc(&zgroup->stats[0].wb_pages);
	atomic64_add(size, &zgroup->stats[gid].wb_size);
	atomic64_add(size, &zgroup->stats[0].wb_size);
}

void zgroup_stats_wb_size_dec(struct zram_group *zgroup,
			      unsigned short gid, size_t size)
{
	atomic_dec(&zgroup->stats[gid].wb_pages);
	atomic_dec(&zgroup->stats[0].wb_pages);
	atomic64_sub(size, &zgroup->stats[gid].wb_size);
	atomic64_sub(size, &zgroup->stats[0].wb_size);
}

static void zgroup_stats_wb_ext_inc(struct zram_group *zgroup,
				    unsigned short gid)
{
	atomic_inc(&zgroup->stats[gid].wb_exts);
	atomic_inc(&zgroup->stats[0].wb_exts);
}

void zgroup_stats_wb_ext_dec(struct zram_group *zgroup, unsigned short gid)
{
	atomic_dec(&zgroup->stats[gid].wb_exts);
	atomic_dec(&zgroup->stats[0].wb_exts);
}

static void zgroup_stats_wb_fault_inc(struct zram_group *zgroup,
				       unsigned short gid)
{
	atomic64_inc(&zgroup->stats[gid].wb_fault);
	atomic64_inc(&zgroup->stats[0].wb_fault);
}

/* move obj to ext */
static void zgroup_wb_add_obj(struct zram_group *zgroup, u32 idx, u32 eid)
{
	u32 hidx;

	hidx = eid + zgroup->nr_objs + zgroup->nr_grps;
	ilist_add(hidx, idx, zgroup->obj_tab);
	pr_debug("add wb obj idx %u hidx %u", idx, hidx);
}

/* move obj from ext to the isolated */
void zgroup_wb_del_obj(struct zram_group *zgroup, u32 idx, u32 eid)
{
	u32 hidx;

	hidx = eid + zgroup->nr_objs + zgroup->nr_grps;
	ilist_del(hidx, idx, zgroup->obj_tab);
	pr_debug("del wb obj idx %u hidx %u", idx, hidx);
}

/* move ext to group */
static void zgroup_wb_add_ext(struct zram_group *zgroup, u32 idx, u16 gid)
{
	u32 hidx;

	hidx = gid + zgroup->nr_exts;
	ilist_add(hidx, idx, zgroup->ext_tab);
	pr_debug("add wb ext idx %u hidx %u", idx, hidx);
}

/* move ext from group to the isolated */
void zgroup_wb_del_ext_nolock(struct zram_group *zgroup, u32 idx, u16 gid)
{
	u32 hidx;

	hidx = gid + zgroup->nr_exts;
	ilist_del_nolock(hidx, idx, zgroup->ext_tab);
	ilist_clear_priv_nolock(idx, zgroup->ext_tab);
	pr_debug("del wb ext idx %u hidx %u", idx, hidx);
}

static void zgroup_free_pool(struct kref *kref)
{
	struct zgroup_pool *pool = container_of(kref, struct zgroup_pool, kref);
	struct page *page;

	spin_lock(&pool->lock);
	while (!list_empty(&pool->head)) {
		page = list_first_entry(&pool->head, struct page, lru);
		list_del_init(&page->lru);
		__free_page(page);
	}
	spin_unlock(&pool->lock);

	kfree(pool);
}

static void zgroup_get_pool(struct zgroup_pool *pool)
{
	kref_get(&pool->kref);
}

void zgroup_put_pool(struct zgroup_pool *pool)
{
	kref_put(&pool->kref, zgroup_free_pool);
}

int zgroup_init_pool(struct zgroup_plug *plug)
{
	struct zgroup_pool *pool;

	DBG_BUGON(plug->pool);

	pool = kzalloc(sizeof(struct zgroup_pool), GFP_NOIO);
	if (!pool)
		return -ENOMEM;

	kref_init(&pool->kref);
	spin_lock_init(&pool->lock);
	INIT_LIST_HEAD(&pool->head);
	plug->pool = pool;

	return 0;
}

static void zgroup_free_page_array(struct zgroup_pool *pool,
				   struct page **page_array,
				   unsigned int nr_pages)
{
	int i;

	if (pool && pool->nr_pages < ZGROUP_POOL_NR) {
		spin_lock(&pool->lock);
		for (i = 0; i < nr_pages; i++)
			if (page_array[i]) {
				list_add(&page_array[i]->lru, &pool->head);
				pool->nr_pages++;
				page_array[i] = NULL;
			}
		spin_unlock(&pool->lock);

		return;
	}

	for (i = 0; i < nr_pages; i++)
		if (page_array[i]) {
			__free_page(page_array[i]);
			page_array[i] = NULL;
		}
}

static int zgroup_alloc_page_array(struct zgroup_pool *pool,
				   struct page **page_array,
				   unsigned int nr_pages, gfp_t gfp)
{
	struct page *page;
	unsigned int allocated = 0;
	unsigned int last;
	int i;

	if (pool && pool->nr_pages) {
		spin_lock(&pool->lock);
		while (!list_empty(&pool->head) && allocated < nr_pages) {
			page = list_first_entry(&pool->head, struct page, lru);
			list_del(&page->lru);
			pool->nr_pages--;
			page_array[allocated++] = page;
		}
		spin_unlock(&pool->lock);
	}

	while (allocated < nr_pages) {
		last = allocated;

		allocated = alloc_pages_bulk_array(gfp, nr_pages,
						   page_array);
		if (unlikely(allocated == last)) {
			/* No progress, fail and do cleanup. */
			for (i = 0; i < allocated; i++) {
				__free_page(page_array[i]);
				page_array[i] = NULL;
			}
			return -ENOMEM;
		}
	}

	return 0;
}

struct page *zgroup_alloc_page(struct zgroup_pool *pool, gfp_t gfp)
{
	struct page *page = NULL;

	if (pool && pool->nr_pages) {
		spin_lock(&pool->lock);
		if (!list_empty(&pool->head)) {
			page = list_first_entry(&pool->head, struct page, lru);
			list_del(&page->lru);
			pool->nr_pages--;
		}
		spin_unlock(&pool->lock);
	}

	return page ?: alloc_page(gfp);
}

static int zgroup_crypt_page(struct crypto_skcipher *tfm,
			     struct page *sp, struct page *dp,
			     sector_t sector, bool encrypt)
{
	union zgroup_iv {
		__le64 index;
		u8 raw[16];	/* AES-256-XTS takes a 16-byte IV */
	} iv;
	const size_t datasize = PAGE_SIZE;
	struct skcipher_request *req;
	struct scatterlist src, dst;
	DECLARE_CRYPTO_WAIT(wait);
	int ret;

	memset(&iv, 0, sizeof(iv));
	iv.index = cpu_to_le64(sp->index);

	req = skcipher_request_alloc(tfm, GFP_NOIO);
	if (!req) {
		ret = -ENOMEM;
		goto out;
	}

	sg_init_table(&src, 1);
	sg_init_table(&dst, 1);
	sg_set_page(&src, sp, datasize, 0);
	sg_set_page(&dst, dp, datasize, 0);
	skcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG |
					   CRYPTO_TFM_REQ_MAY_SLEEP,
				      crypto_req_done, &wait);
	skcipher_request_set_crypt(req, &src, &dst, datasize, &iv);
	if (encrypt)
		ret = crypto_wait_req(crypto_skcipher_encrypt(req), &wait);
	else
		ret = crypto_wait_req(crypto_skcipher_decrypt(req), &wait);
	if (ret) {
		pr_err("Error encrypting data: %d\n", ret);
		goto out;
	}

	pr_debug("Encryption was successful\n");
out:
	skcipher_request_free(req);
	return ret;
}

static inline bool zgroup_io_crypt(struct zgroup_io *zio)
{
	struct zram_group *zgroup = zio->zgroup;
	struct zgroup_bdev *d = zgroup->bdev;
	struct zgroup_crypt_info *ci = &d->ci;

	if (!ci->crypt || ci->inline_crypt)
		return false;

	return true;
}

static int zgroup_decrypt_pages(struct zgroup_io *zio)
{
	struct zram_group *zgroup = zio->zgroup;
	struct zgroup_bdev *d = zgroup->bdev;
	struct zgroup_crypt_info *ci = &d->ci;
	int i, ret;
	sector_t sector;

	if (!zgroup_io_crypt(zio))
		return 0;

	sector = (sector_t)zio->eid * ZGROUP_SEC_PER_EXT;
	for (i = 0; i < ZGROUP_PAGE_PER_EXT; i++) {
		ret = zgroup_crypt_page(ci->tfm,
					zio->crypt_pages[i], zio->pages[i],
					sector, false);
		if (ret)
			return ret;

		sector += PAGE_SECTORS;
	}

	return 0;
}

static void zgroup_put_crypt_pages(struct zgroup_io *zio)
{
	int i;

	if (!zgroup_io_crypt(zio))
		return;

	if (zio->crypt_pages) {
		for (i = 0; i < ZGROUP_PAGE_PER_EXT; i++)
			if (zio->crypt_pages[i])
				__free_page(zio->crypt_pages[i]);

		kfree(zio->crypt_pages);
		zio->crypt_pages = NULL;
	}
}

static struct page **zgroup_get_crypt_pages(struct zgroup_io *zio)
{
	struct zram_group *zgroup = zio->zgroup;
	struct zgroup_bdev *d = zgroup->bdev;
	struct zgroup_crypt_info *ci = &d->ci;
	sector_t sector;
	int i, ret;

	if (!zgroup_io_crypt(zio))
		return zio->pages;

	zio->crypt_pages = kcalloc(ZGROUP_PAGE_PER_EXT, sizeof(struct page *),
				   GFP_NOIO);
	if (!zio->crypt_pages)
		return ERR_PTR(-ENOMEM);

	sector = (sector_t)zio->eid * ZGROUP_SEC_PER_EXT;
	for (i = 0; i < ZGROUP_PAGE_PER_EXT; i++) {
		zio->crypt_pages[i] = alloc_page(GFP_NOIO);
		if (!zio->crypt_pages[i]) {
			ret = -ENOMEM;
			goto out;
		}

		if (!op_is_write(zio->op))
			continue;

		ret = zgroup_crypt_page(ci->tfm,
					zio->pages[i], zio->crypt_pages[i],
					sector, true);
		if (ret)
			goto out;

		sector += PAGE_SECTORS;
	}

	return zio->crypt_pages;
out:
	zgroup_put_crypt_pages(zio);
	return ERR_PTR(ret);
}

static void zgroup_crypt_set_ctx(struct zgroup_io *zio, struct bio *bio)
{
	struct zram_group *zgroup = zio->zgroup;
	struct zgroup_bdev *d = zgroup->bdev;
	struct zgroup_crypt_info *ci = &d->ci;
	u64 dun[BLK_CRYPTO_DUN_ARRAY_SIZE] = { 0 };

	if (!ci->crypt || !ci->inline_crypt)
		return;

	dun[0] = le64_to_cpu(bio->bi_iter.bi_sector >> PAGE_SECTORS_SHIFT);
	bio_crypt_set_ctx(bio, ci->blk_key, dun, GFP_NOIO | __GFP_NOFAIL);
}

static void zgroup_copy_obj(struct zgroup_io *zio, unsigned long iofs,
			    char *obj, size_t size, bool swapin)
{
	unsigned long ofs, end, len;
	unsigned long pg_idx, pg_ofs;
	char *src, *dst, *pg_cur, *obj_cur;

	ofs = 0;
	end = ofs + size;
	pg_idx = iofs / PAGE_SIZE;
	pg_ofs = iofs % PAGE_SIZE;

	while (ofs < end) {
		pg_cur = page_to_virt(zio->pages[pg_idx]) + pg_ofs;
		obj_cur = obj + ofs;
		/* swapin means copy ext to obj */
		src = swapin ? pg_cur : obj_cur;
		dst = swapin ? obj_cur : pg_cur;
		len = min(end - ofs, PAGE_SIZE - pg_ofs);
		memcpy(dst, src, len);

		ofs += len;
		/* pages may only have offset in the first copy. */
		pg_ofs += len;
		if (pg_ofs >= PAGE_SIZE) {
			pg_ofs -= PAGE_SIZE;
			++pg_idx;
		}
	}
}

static void zgroup_ext_to_obj(struct zgroup_io *zio, u32 idx, bool free)
{
	size_t sz;
	unsigned long handle = -ENOMEM;
	unsigned long lofs, ofs;
	char *obj;
	struct zram_group *zgroup = zio->zgroup;
	struct zram *zram = zgroup->zram;
	int pg_idx;
repeat:
	zram_slot_lock(zram, idx);

	if (!zram_test_flag(zram, idx, ZRAM_GROUP_WB)) {
		zram_slot_unlock(zram, idx);
		zs_free(zram->mem_pool, handle);
		return;
	}

	lofs = zram_get_handle(zram, idx);
	sz = zram_get_obj_size(zram, idx);
	DBG_BUGON(lofs / ZGROUP_EXT_SIZE != zio->eid);

	if (IS_ERR_VALUE(handle))
		handle = zs_malloc_mi(zio->pool, zram->mem_pool, sz,
				   __GFP_KSWAPD_RECLAIM |
				   __GFP_NOWARN |
				   __GFP_HIGHMEM |
				   __GFP_MOVABLE);

	if (IS_ERR_VALUE(handle)) {
		zram_slot_unlock(zram, idx);
		/* how to handle the situation without __GFP_NOFAIL? */
		handle = zs_malloc_mi(zio->pool, zram->mem_pool, sz,
				   GFP_NOIO |
				   __GFP_HIGHMEM |
				   __GFP_MOVABLE |
				   __GFP_NOFAIL);
		goto repeat;
	}

	ofs = lofs % ZGROUP_EXT_SIZE;
	obj = zs_map_object(zram->mem_pool, handle, ZS_MM_WO);
	zgroup_copy_obj(zio, ofs, obj, sz, true);
	zs_unmap_object(zram->mem_pool, handle);

	/* try to free the last page */
	pg_idx = ofs / PAGE_SIZE - 1;
	if (free && pg_idx >= 0 && zio->pages[pg_idx])
		zgroup_free_page_array(zio->pool, &zio->pages[pg_idx], 1);

	zram_set_handle(zram, idx, handle);
	zram_clear_flag(zram, idx, ZRAM_GROUP_WB);

	zgroup_wb_del_obj(zgroup, idx, zio->eid);
	zgroup_add_obj(zgroup, idx, zio->gid);
	zgroup_stats_wb_size_dec(zgroup, zio->gid, sz);
	zgroup_stats_obj_size_inc(zgroup, zio->gid, sz);

	zram_slot_unlock(zram, idx);
}

static bool zgroup_test_obj_wb(struct zram *zram, u32 idx, u16 gid)
{
	DBG_BUGON(zram_test_flag(zram, idx, ZRAM_SAME));

	if (zram_test_flag(zram, idx, ZRAM_GROUP_FAULT))
		return false;

	if (zram_test_flag(zram, idx, ZRAM_GROUP_WB))
		return false;

	/* released page */
	if (ilist_is_isolated(idx, zram->zgroup->obj_tab))
		return false;

	/* overwrited page */
	if (zram_get_memcg_id(zram, idx) != gid)
		return false;

	return true;
}

static bool zgroup_obj_to_ext(struct zgroup_io *zio, u32 idx,
			      unsigned long *ofs)
{
	size_t sz;
	unsigned long handle, lofs;
	char *obj;
	struct zram_group *zgroup = zio->zgroup;
	struct zram *zram = zgroup->zram;

	zram_slot_lock(zram, idx);
	handle = zram_get_handle(zram, idx);
	if (!handle || !zgroup_test_obj_wb(zram, idx, zio->gid)) {
		zram_slot_unlock(zram, idx);
		return false;
	}

	sz = zram_get_obj_size(zram, idx);
	if (*ofs + sz > ZGROUP_EXT_SIZE) {
		zram_slot_unlock(zram, idx);
		return true;
	}

	obj = zs_map_object(zram->mem_pool, handle, ZS_MM_RO);
	zgroup_copy_obj(zio, *ofs, obj, sz, false);
	zs_unmap_object(zram->mem_pool, handle);
	zs_free(zram->mem_pool, handle);

	lofs = (unsigned long)zio->eid * ZGROUP_EXT_SIZE + *ofs;
	zram_set_handle(zram, idx, lofs);
	zram_set_flag(zram, idx, ZRAM_GROUP_WB);

	zgroup_del_obj(zgroup, idx, zio->gid);
	zgroup_wb_add_obj(zgroup, idx, zio->eid);
	zgroup_stats_obj_size_dec(zgroup, zio->gid, sz);
	zgroup_stats_wb_size_inc(zgroup, zio->gid, sz);

	zram_slot_unlock(zram, idx);
	*ofs += sz;

	return false;
}

bool zgroup_shrink_idxs(struct ilist_table *tab, u32 hidx, u32 *idxs, int *cnt)
{
	u32 idx;
	int max_cnt = *cnt;

	*cnt = 0;
	ilist_lock(hidx, tab);

	ilist_for_each_idx_reverse(idx, hidx, tab) {
		if (ilist_test_priv_nolock(idx, tab))
			continue;

		idxs[*cnt] = idx;
		if (++*cnt >= max_cnt)
			break;
	}

	ilist_unlock(hidx, tab);

	return idx == hidx;
}

static void zgroup_free_empty_ext(struct zgroup_io *zio)
{
	u32 hidx;
	struct zram_group *zgroup = zio->zgroup;

	hidx = zio->gid + zgroup->nr_exts;
	ilist_lock(hidx, zgroup->ext_tab);

	/* avoid race */
	if (!ilist_is_isolated(zio->eid, zgroup->ext_tab)) {
		zgroup_wb_del_ext_nolock(zgroup, zio->eid, zio->gid);
		zgroup_stats_wb_ext_dec(zgroup, zio->gid);
		zio->free_ext = true;
	}

	ilist_unlock(hidx, zgroup->ext_tab);
}

static void zgroup_swapin_obj(struct zgroup_io *zio, u32 idx)
{
	u32 hidx;
	struct zram_group *zgroup = zio->zgroup;

	zgroup_ext_to_obj(zio, idx, false);

	hidx = zio->eid + zgroup->nr_objs + zgroup->nr_grps;
	/* avoid race */
	if (ilist_is_isolated(hidx, zgroup->obj_tab))
		zgroup_free_empty_ext(zio);
}

static void zgroup_swapin_objs(struct zgroup_io *zio)
{
	bool last;
	u32 idxs[ZGROUP_SHRINK_NR];
	u32 hidx;
	int cnt, i;
	struct zram_group *zgroup = zio->zgroup;

	hidx = zio->eid + zgroup->nr_objs + zgroup->nr_grps;
repeat:
	cnt = ZGROUP_SHRINK_NR;
	last = zgroup_shrink_idxs(zgroup->obj_tab, hidx, idxs, &cnt);
	if (cnt == 0 && last)
		goto out;

	for (i = 0; i < cnt; i++)
		zgroup_ext_to_obj(zio, idxs[i], true);
	if (!last)
		goto repeat;
out:
	/* avoid race */
	zgroup_free_empty_ext(zio);
}

static unsigned long zgroup_swapout_objs(struct zgroup_io *zio)
{
	bool last, full;
	u32 idxs[ZGROUP_SHRINK_NR];
	u32 hidx;
	int cnt, i;
	unsigned long last_out_sz;
	unsigned long out_sz = 0;
	struct zram_group *zgroup = zio->zgroup;

	hidx = zio->gid + zgroup->nr_objs;
repeat:
	cnt = ZGROUP_SHRINK_NR;
	last = zgroup_shrink_idxs(zgroup->obj_tab, hidx, idxs, &cnt);
	if (cnt == 0 && last)
		goto out;

	/* avoid infinite loops */
	last_out_sz = out_sz;
	for (i = 0; i < cnt; i++) {
		full = zgroup_obj_to_ext(zio, idxs[i], &out_sz);
		if (full)
			break;
	}
	if (!full && !last && out_sz != last_out_sz)
		goto repeat;
out:
	/* ensure that we actually swap out objs */
	if (out_sz) {
		zgroup_wb_add_ext(zgroup, zio->eid, zio->gid);
		zgroup_stats_wb_ext_inc(zgroup, zio->gid);
	}

	return out_sz;
}

static inline
unsigned long find_next_zero_bit_wrap(const unsigned long *addr,
				      unsigned long size, unsigned long offset)
{
	unsigned long bit = find_next_zero_bit(addr, size, offset);

	if (bit < size)
		return bit;

	bit = find_first_zero_bit(addr, offset);
	return bit < offset ? bit : size;
}

static int zgroup_alloc_ext(struct zgroup_bdev *d)
{
	unsigned long eid, last_ext;

repeat:
	last_ext = atomic_read(&d->last_ext);
	eid = find_next_zero_bit_wrap(d->ext_map, d->nr_exts, last_ext);
	if (eid == d->nr_exts)
		return -ENOSPC;

	if (test_and_set_bit(eid, d->ext_map))
		goto repeat;

	atomic_set(&d->last_ext, eid);

	return eid;
}

static inline void zgroup_free_ext(struct zgroup_bdev *d, u32 eid)
{
	DBG_BUGON(!test_and_clear_bit(eid, d->ext_map));
}

static inline bool zgroup_test_ext(struct zgroup_bdev *d, u32 eid)
{
	return test_bit(eid, d->ext_map);
}

static inline void zgroup_io_step_set(struct zgroup_io *zio,
				      enum zgroup_io_step step)
{
	return atomic_set(&zio->step, step);
}

static inline enum zgroup_io_step zgroup_io_step_get(struct zgroup_io *zio)
{
	return atomic_read(&zio->step);
}

static inline bool zgroup_io_step_chg(struct zgroup_io *zio,
				      enum zgroup_io_step from,
				      enum zgroup_io_step to)
{
	return atomic_cmpxchg(&zio->step, from, to) == from;
}

static void zgroup_free_io(struct zgroup_io *zio)
{
	zgroup_put_crypt_pages(zio);
	zgroup_free_page_array(zio->pool, zio->pages, ZGROUP_PAGE_PER_EXT);
	if (zio->pool)
		zgroup_put_pool(zio->pool);

	kfree(zio->pages);
	kfree_rcu(zio, rcu);
}

static struct zgroup_io *zgroup_alloc_io(struct zgroup_plug *plug, u32 eid)
{
	struct zgroup_io *zio;
	int ret;

	zio = kzalloc(sizeof(struct zgroup_io), GFP_NOIO);
	if (!zio)
		return ERR_PTR(-ENOMEM);

	zio->gid = plug->gid;
	zio->eid = eid;
	zio->zgroup = plug->zgroup;

	zio->op = plug->op;
	zio->pages = kcalloc(ZGROUP_PAGE_PER_EXT, sizeof(struct page *),
			     GFP_NOIO);
	if (!zio->pages)
		goto err;

	ret = zgroup_alloc_page_array(plug->pool, zio->pages,
				      ZGROUP_PAGE_PER_EXT, GFP_NOIO);
	if (ret)
		goto err;

	zio->free_ext = false;
	zgroup_io_step_set(zio, ZIO_STARTED);
	kref_init(&zio->kref);
	init_completion(&zio->wait);

	return zio;
err:
	zgroup_free_io(zio);
	return ERR_PTR(-ENOMEM);
}

static struct zgroup_io *zgroup_find_io(struct zgroup_bdev *d, u32 eid)
{
	struct zgroup_io *zio;

	rcu_read_lock();
	zio = xa_load(&d->managed_ios, eid);
	if (zio) {
		if (!kref_get_unless_zero(&zio->kref))
			zio = NULL;
		else
			DBG_BUGON(eid != zio->eid);
	}
	rcu_read_unlock();

	return zio;
}

static struct zgroup_io *zgroup_find_io_lock(struct zgroup_bdev *d, u32 eid)
{
	struct zgroup_io *zio;

	xa_lock(&d->managed_ios);
	zio = xa_load(&d->managed_ios, eid);
	if (zio) {
		if (!kref_get_unless_zero(&zio->kref))
			zio = NULL;
		else
			DBG_BUGON(eid != zio->eid);
	}
	xa_unlock(&d->managed_ios);

	return zio;
}

static struct zgroup_io *zgroup_insert_io(struct zgroup_io *zio)
{
	struct zgroup_io *pre;
	struct zram_group *zgroup = zio->zgroup;
	struct zgroup_bdev *d = zio->zgroup->bdev;

	DBG_BUGON(kref_read(&zio->kref) < 1);
repeat:
	xa_lock(&d->managed_ios);
	if (!op_is_write(zio->op) &&
	    ilist_is_isolated(zio->eid, zgroup->ext_tab)) {
		zio = NULL;
		goto out;
	}
	pre = __xa_cmpxchg(&d->managed_ios, zio->eid, NULL, zio, GFP_NOIO);
	if (pre) {
		if (xa_is_err(pre)) {
			pre = ERR_PTR(xa_err(pre));
		} else if (!kref_get_unless_zero(&pre->kref)) {
			/* try to legitimize the current in-tree one */
			xa_unlock(&d->managed_ios);
			cond_resched();
			goto repeat;
		}
		zio = pre;
	}
out:
	xa_unlock(&d->managed_ios);

	return zio;
}

static struct zgroup_io *zgroup_get_io(struct zgroup_plug *plug, u32 eid)
{
	struct zgroup_io *zio, *new_zio;
	struct zram_group *zgroup = plug->zgroup;
	struct zgroup_bdev *d = zgroup->bdev;

	zio = zgroup_find_io(d, eid);
	if (zio)
		return zio;

	new_zio = zgroup_alloc_io(plug, eid);
	if (IS_ERR(new_zio))
		return new_zio;

	zio = zgroup_insert_io(new_zio);
	if (zio != new_zio)
		zgroup_free_io(new_zio);

	DBG_BUGON(!IS_ERR_OR_NULL(zio) && !zgroup_test_ext(d, zio->eid));

	return zio;
}

static void zgroup_release_io(struct kref *kref)
{
	struct zgroup_io *zio = container_of(kref, struct zgroup_io, kref);
	struct zgroup_bdev *d = zio->zgroup->bdev;

	xa_erase(&d->managed_ios, zio->eid);
	if (zio->free_ext)
		zgroup_free_ext(d, zio->eid);

	zgroup_free_io(zio);
}

static inline void zgroup_put_io(struct zgroup_io *zio)
{
	kref_put(&zio->kref, zgroup_release_io);
}

static inline void zgroup_complete_io(struct zgroup_io *zio)
{
	complete_all(&zio->wait);
}

void zgroup_try_to_free_ext(struct zgroup_bdev *d, u32 eid)
{
	struct zgroup_io *zio;

	zio = zgroup_find_io_lock(d, eid);
	if (!zio) {
		zgroup_free_ext(d, eid);
		return;
	}

	zio->free_ext = true;
	zgroup_put_io(zio);
}

static inline void zgroup_free_req(struct zgroup_req *req)
{
	zgroup_put_pool(req->pool);
	kfree(req);
}

static void zgroup_post_read_work(struct work_struct *work)
{
	struct zgroup_req *req = container_of(work, struct zgroup_req, work);
	struct zgroup_io *zio, *next;

	zio_for_each_safe(zio, next, req) {
		if (!zio->status) {
			zio->status = zgroup_decrypt_pages(zio);
			if (!zio->status)
				zgroup_swapin_objs(zio);
		}

		ilist_clear_priv(zio->eid, zio->zgroup->ext_tab);

		if (zgroup_io_crypt(zio))
			zgroup_complete_io(zio);
		zgroup_put_io(zio);
	}

	zgroup_free_req(req);
}

static void zgroup_post_write_work(struct work_struct *work)
{
	struct zgroup_req *req = container_of(work, struct zgroup_req, work);
	struct zgroup_io *zio, *next;

	zio_for_each_safe(zio, next, req) {
		if (zio->status)
			zgroup_swapin_objs(zio);

		zgroup_put_io(zio);
	}

	zgroup_free_req(req);
}

static inline void zgroup_complete_req(struct zgroup_req *req)
{
	struct zgroup_io *zio, *next;

	zio_for_each_safe(zio, next, req) {
		if (req->status)
			zio->status = req->status;
		DBG_BUGON(!zgroup_io_step_chg(zio, ZIO_INFLIGHT, ZIO_COMPLETE));

		if (!op_is_write(zio->op) && !zgroup_io_crypt(zio))
			zgroup_complete_io(zio);
	}
}

static void zgroup_end_req(struct bio *bio)
{
	struct zgroup_req *req = bio->bi_private;
	struct zgroup_io *zio = req->zio;

	req->status = bio->bi_status;
	zgroup_complete_req(req);

	INIT_WORK(&req->work, op_is_write(zio->op) ? zgroup_post_write_work :
						     zgroup_post_read_work);
	queue_work(op_is_write(zio->op) ? zgroup_write_wq : zgroup_read_wq,
		   &req->work);

	bio_put(bio);
}

/* get the index of n'th node's k'th key in the next level */
static inline unsigned int zgroup_get_child(unsigned int n, unsigned int k)
{
	return n * ZGROUP_CHILD_PER_NODE + k;
}

/* get the n'th node from level l */
static inline sector_t *zgroup_get_node(struct zgroup_table *t,
					unsigned int l, unsigned int n)
{
	return t->index[l] + n * ZGROUP_KEYS_PER_NODE;
}

static sector_t zgroup_map_sector(struct zgroup_table *t, sector_t sector)
{
	unsigned int l, n = 0, k = 0;
	sector_t *node;
	struct zgroup_target *ti;

	for (l = 0; l < t->depth; l++) {
		n = zgroup_get_child(n, k);
		node = zgroup_get_node(t, l, n);

		for (k = 0; k < ZGROUP_KEYS_PER_NODE; k++)
			if (node[k] >= sector)
				break;
	}

	ti = &t->targets[ZGROUP_KEYS_PER_NODE * n + k];

	return ti->pofs + (sector - ti->lofs);
}

static void zgroup_free_req_io(struct zgroup_req *req, int status)
{
	struct zgroup_io *zio, *next;

	zio_for_each_safe(zio, next, req) {
		zio->status = status;
		if (op_is_write(zio->op))
			zgroup_swapin_objs(zio);

		zgroup_complete_io(zio);
		zgroup_put_io(zio);
	}
}

static int zgroup_submit_req(struct zgroup_req *req, bool sync)
{
	struct zgroup_io *zio = req->zio;
	struct zgroup_bdev *d;
	struct bio *bio;
	struct page **pages;
	sector_t sector;
	int i;
	int ret = 0;

	d = zio->zgroup->bdev;
	bio = bio_alloc(d->blk_dev, BIO_MAX_VECS, zio->op, GFP_NOIO);
	if (!bio) {
		pr_err("bio alloc failed!\n");
		ret = -ENOMEM;
		goto out_free;
	}

	sector = (sector_t)zio->eid * ZGROUP_SEC_PER_EXT;
	bio->bi_iter.bi_sector = zgroup_map_sector(&d->table, sector);
	zgroup_crypt_set_ctx(zio, bio);

	zio_for_each(zio, req) {
		pages = zgroup_get_crypt_pages(zio);
		if (IS_ERR(pages)) {
			ret = -ENOMEM;
			goto out;
		}

		for (i = 0; i < ZGROUP_PAGE_PER_EXT; i++) {
			if (!bio_add_page(bio, pages[i], PAGE_SIZE, 0)) {
				ret = -EIO;
				goto out;
			}
		}
#ifdef CONFIG_XRING_ZRAM_MEMCG_DEBUG
		zio->bio = bio;
#endif

	}

	if (sync) {
		ret = submit_bio_wait(bio);
		bio_put(bio);
		DBG_BUGON(!zgroup_io_step_chg(req->zio,
					      ZIO_INFLIGHT, ZIO_COMPLETE));
		if (ret)
			goto out_free;
	} else {
		bio->bi_private = req;
		bio->bi_end_io = zgroup_end_req;
		submit_bio(bio);
	}

	return ret;
out:
	bio_put(bio);
out_free:
	zgroup_free_req_io(req, ret);
	return ret;
}

int zgroup_start_plug(struct zgroup_plug *plug, struct zgroup_io *zio)
{
	struct zgroup_req *req;

	DBG_BUGON(plug->req);

	req = kzalloc(sizeof(struct zgroup_req), GFP_NOIO);
	if (!req) {
		zgroup_put_pool(plug->pool);
		return -ENOMEM;
	}

	if (zio) {
		req->zio = zio;
		req->ziotail = zio;
		req->nr_vecs = ZGROUP_PAGE_PER_EXT;
	}

	zgroup_get_pool(plug->pool);
	req->pool = plug->pool;
	plug->req = req;

	return 0;
}

int zgroup_finish_plug(struct zgroup_plug *plug)
{
	int ret = 0;

	DBG_BUGON(!plug->req);

	if (!plug->req->zio) {
		zgroup_free_req(plug->req);
		goto out;
	}

	ret = zgroup_submit_req(plug->req, false);
	if (ret)
		zgroup_free_req(plug->req);

out:
	plug->req = NULL;
	return ret;
}

static bool zgroup_io_mergable(struct zgroup_io *prev, struct zgroup_io *next)
{
	struct zgroup_table *t = &prev->zgroup->bdev->table;
	sector_t psec, nsec;

	if (prev->eid + 1 != next->eid)
		return false;

	psec = zgroup_map_sector(t, (sector_t)prev->eid * ZGROUP_SEC_PER_EXT);
	nsec = zgroup_map_sector(t, (sector_t)next->eid * ZGROUP_SEC_PER_EXT);

	return psec + ZGROUP_SEC_PER_EXT == nsec;
}

static bool zgroup_try_merge(struct zgroup_req *req, struct zgroup_io *zio)
{
	if (req->nr_vecs + ZGROUP_PAGE_PER_EXT > BIO_MAX_VECS)
		return false;

	if (!req->zio) {
		req->zio = zio;
		req->ziotail = zio;
		req->nr_vecs = ZGROUP_PAGE_PER_EXT;
		return true;
	}

	if (zgroup_io_mergable(req->ziotail, zio)) {
		req->ziotail->next = zio;
		req->ziotail = zio;
		req->nr_vecs += ZGROUP_PAGE_PER_EXT;
		return true;
	}

	if (zgroup_io_mergable(zio, req->zio)) {
		zio->next = req->zio;
		req->zio = zio;
		req->nr_vecs += ZGROUP_PAGE_PER_EXT;
		return true;
	}

	return false;
}

static int zgroup_submit_zio(struct zgroup_plug *plug, struct zgroup_io *zio)
{
	int ret;

	if (!zgroup_io_step_chg(zio, ZIO_STARTED, ZIO_INFLIGHT)) {
		DBG_BUGON(op_is_write(zio->op));
		zgroup_put_io(zio);
		return -EEXIST;
	}
	/* mark inflight ext */
	if (!op_is_write(zio->op))
		ilist_set_priv(zio->eid, plug->zgroup->ext_tab);

	zgroup_get_pool(plug->pool);
	zio->pool = plug->pool;

	if (zgroup_try_merge(plug->req, zio))
		return 0;

	ret = zgroup_finish_plug(plug);
	if (ret)
		return ret;

	ret = zgroup_start_plug(plug, zio);
	if (ret) {
		zgroup_put_io(zio);
		return ret;
	}

	return 0;
}

static void zgroup_submit_zio_work(struct work_struct *work)
{
	struct zgroup_req *req;

	req = container_of(work, struct zgroup_req, work);
	req->status = zgroup_submit_req(req, true);
}

/*
 * Block layer want one ->submit_bio to be active at a time, so if we use
 * chained IO with parent IO in same context, it's a deadlock. To avoid that,
 * use a worker thread context.
 */
static int zgroup_submit_zio_async(struct zgroup_io *zio,
				   struct zram_group *zgroup)
{
	struct zgroup_req req = {
		.zio = zio,
	};

	if (!zgroup_io_step_chg(zio, ZIO_STARTED, ZIO_INFLIGHT)) {
		wait_for_completion(&zio->wait);
		if (zio->status) {
			zgroup_put_io(zio);
		}
		return zio->status;
	}
	/* mark inflight ext */
	if (!op_is_write(zio->op))
		ilist_set_priv(zio->eid, zgroup->ext_tab);

#if 1
	INIT_WORK_ONSTACK(&req.work, zgroup_submit_zio_work);
	queue_work(zgroup_read_wq, &req.work);
	flush_work(&req.work);
	destroy_work_on_stack(&req.work);
#else
	req.status = zgroup_submit_req(&req, true);
#endif

	return req.status ?: zgroup_decrypt_pages(zio);
}

/* Must be called with ZRAM_LOCK held */
int zgroup_read_obj(struct zram *zram, u32 index)
{
	struct zgroup_io *zio;
	u32 eid;
	u16 gid;
	int ret = 0;
	struct zram_group *zgroup = zram->zgroup;
	struct zgroup_plug plug = {
		.zgroup = zgroup,
		.op = REQ_OP_READ,
	};

repeat:
	if (!zram_test_flag(zram, index, ZRAM_GROUP_WB))
		return ret;

	/* TODO: need wait ? */
	if (zram_test_flag(zram, index, ZRAM_GROUP_FAULT)) {
		zram_slot_unlock(zram, index);
		wait_event(zgroup_fault_wq,
			   !zram_test_flag(zram, index, ZRAM_GROUP_FAULT));
		zram_slot_lock(zram, index);
		goto repeat;
	}

	zram_set_flag(zram, index, ZRAM_GROUP_FAULT);
	gid = zram_get_memcg_id(zram, index);
	eid = zram_get_handle(zram, index) / ZGROUP_EXT_SIZE;
	zgroup_stats_wb_fault_inc(zgroup, gid);

	zram_slot_unlock(zram, index);

	plug.gid = gid;
	zio = zgroup_get_io(&plug, eid);
	if (IS_ERR_OR_NULL(zio)) {
		ret = PTR_ERR_OR_ZERO(zio);
		goto out_lock;
	}

#ifdef CONFIG_XRING_ZRAM_MEMCG_DEBUG
	zio->owner = zio->owner ?: current;
#endif

	/* No need to wait, since obj is stored in zio->pages */
	if (op_is_write(zio->op)) {
		zgroup_swapin_obj(zio, index);
		goto out;
	}

	ret = zgroup_submit_zio_async(zio, zgroup);
	if (ret)
		goto out_lock;

	zgroup_swapin_objs(zio);
out:
	zgroup_complete_io(zio);
	zgroup_put_io(zio);
out_lock:
	zram_slot_lock(zram, index);
	zram_clear_flag(zram, index, ZRAM_GROUP_FAULT);
	wake_up(&zgroup_fault_wq);

	if (ret)
		pr_err("failed to read obj %u from ext %u group %u ret %d",
		       index, eid, gid, ret);
	else
		DBG_BUGON(zram_test_flag(zram, index, ZRAM_GROUP_WB));

	return ret;
}

long zgroup_read_ext(struct zgroup_plug *plug, u32 eid)
{
	struct zgroup_io *zio;
	int ret;

	zio = zgroup_get_io(plug, eid);
	if (IS_ERR_OR_NULL(zio)) {
		ret = PTR_ERR_OR_ZERO(zio);
		goto out;
	}

	/* skip this ext to avoid refalult */
	if (op_is_write(zio->op)) {
		zgroup_put_io(zio);
		return 0;
	}

	ret = zgroup_submit_zio(plug, zio);
	if (ret) {
		/* inflight io */
		if (ret == -EEXIST)
			return 0;
		goto out;
	}

	return ZGROUP_EXT_SIZE;
out:
	if (ret)
		pr_err("failed to read ext %u from group %u ret %d",
	       eid, plug->gid, ret);
	return ret;
}

long zgroup_write_ext(struct zgroup_plug *plug)
{
	struct zram_group *zgroup = plug->zgroup;
	struct zgroup_io *zio;
	unsigned long sz;
	int eid;
	int ret = 0;

	eid = zgroup_alloc_ext(zgroup->bdev);
	if (eid < 0) {
		ret = eid;
		goto out;
	}

	zio = zgroup_get_io(plug, eid);
	if (IS_ERR(zio)) {
		ret = PTR_ERR(zio);
		goto get_out;
	}
	DBG_BUGON(!op_is_write(zio->op));

	sz = zgroup_swapout_objs(zio);
	if (!sz)
		goto swap_out;

	ret = zgroup_submit_zio(plug, zio);
	if (ret)
		goto out;

	return sz;
swap_out:
	zgroup_put_io(zio);
get_out:
	zgroup_free_ext(zgroup->bdev, eid);
out:
	if (ret)
		pr_err("failed to write async ext from group %u ret %d",
		       plug->gid, ret);
	return ret;
}

long zgroup_write_ext_sync(struct zgroup_plug *plug)
{
	struct zram_group *zgroup = plug->zgroup;
	struct zgroup_req req = { };
	struct zgroup_io *zio;
	unsigned long sz;
	int eid;
	long ret = 0;

	eid = zgroup_alloc_ext(zgroup->bdev);
	if (eid < 0) {
		ret = eid;
		goto out;
	}

	zio = zgroup_get_io(plug, eid);
	if (IS_ERR(zio)) {
		ret = PTR_ERR(zio);
		goto get_out;
	}
	DBG_BUGON(!op_is_write(zio->op));

	if (!zgroup_io_step_chg(zio, ZIO_STARTED, ZIO_INFLIGHT)) {
		DBG_BUGON(op_is_write(zio->op));
		ret = -EEXIST;
		goto swap_out;
	}

	sz = zgroup_swapout_objs(zio);
	if (!sz)
		goto swap_out;

	req.zio = zio;
	zio->status = zgroup_submit_req(&req, true);
	if (zio->status){
		zgroup_swapin_objs(zio);
		ret = zio->status;
	}

	zgroup_put_crypt_pages(zio);
swap_out:
	zgroup_put_io(zio);
get_out:
	if (ret) {
		if(ret != -EEXIST)
			zgroup_free_ext(zgroup->bdev, eid);
out:
		pr_err("failed to write sync ext from group %u ret %ld",
		       plug->gid, ret);
		return ret;
	}

	if (!sz)
		zgroup_free_ext(zgroup->bdev, eid);

	return sz;
}


static unsigned int zgroup_int_log(unsigned int n, unsigned int base)
{
	int result = 0;

	while (n > 1) {
		n = DIV_ROUND_UP(n, base);
		result++;
	}

	return result;
}

/* return the highest key in the child */
static sector_t zgroup_get_key(struct zgroup_table *t,
			       unsigned int l, unsigned int n)
{
	for (; l < t->depth - 1; l++)
		n = zgroup_get_child(n, ZGROUP_CHILD_PER_NODE - 1);

	/* set to maximum value */
	if (n >= t->nr_index[l])
		return (sector_t) -1;

	return zgroup_get_node(t, l, n)[ZGROUP_KEYS_PER_NODE - 1];
}

static int zgroup_setup_btree_index(struct zgroup_table *t, unsigned int l)
{
	unsigned int n, k, c;
	sector_t *node;

	for (n = 0; n < t->nr_index[l]; n++) {
		node = zgroup_get_node(t, l, n);

		for (k = 0; k < ZGROUP_KEYS_PER_NODE; k++) {
			c = zgroup_get_child(n, k);
			node[k] = zgroup_get_key(t, l + 1, c);
		}
	}

	return 0;
}

static int zgroup_setup_index(struct zgroup_table *t)
{
	int i;
	unsigned int total = 0;
	sector_t *index;

	/* only one layer, no need to setup index */
	if (t->depth < 2)
		return 0;

	for (i = t->depth - 2; i >= 0; i--) {
		t->nr_index[i] = DIV_ROUND_UP(t->nr_index[i + 1],
					      ZGROUP_CHILD_PER_NODE);
		total += t->nr_index[i];
	}

	index = kvcalloc(total, ZGROUP_NODE_SIZE, GFP_KERNEL);
	if (!index)
		return -ENOMEM;

	for (i = t->depth - 2; i >= 0; i--) {
		t->index[i] = index;
		index += t->nr_index[i] * ZGROUP_KEYS_PER_NODE;
		zgroup_setup_btree_index(t, i);
	}

	return 0;
}

static int zgroup_build_index(struct zgroup_table *t)
{
	unsigned int leafs;

	if (t->nr_targets == 0)
		return -EINVAL;

	leafs = DIV_ROUND_UP(t->nr_targets, ZGROUP_KEYS_PER_NODE);
	t->depth = 1 + zgroup_int_log(leafs, ZGROUP_CHILD_PER_NODE);
	if (t->depth >= ZGROUP_WB_MAX_DEPTH)
		return -EINVAL;

	/* set up leaf layer */
	t->nr_index[t->depth - 1] = leafs;
	t->index[t->depth - 1] = t->keys;

	return zgroup_setup_index(t);
}

static int zgroup_target_add(struct zgroup_table *t,
			     sector_t start, sector_t len)
{
	struct zgroup_target *ti, *prev;

	if (t->nr_targets >= t->max_targets) {
		pr_warn("unmatched target number %u %u",
			t->nr_targets, t->max_targets);
		return -EINVAL;
	}

	ti = t->targets + t->nr_targets;
	memset(ti, 0, sizeof(struct zgroup_target));

	if (t->nr_targets != 0) {
		prev = &t->targets[t->nr_targets - 1];
		ti->lofs = prev->lofs + prev->len;
	}

	ti->pofs = start;
	ti->len = len;

	t->keys[t->nr_targets++] = ti->lofs + ti->len - 1;

	return 0;
}

static int zgroup_targets_add(struct zgroup_table *t,
			      struct zgroup_bdev_info *info,
			      struct zgroup_target_info __user *targets)
{
	int ret, i;
	unsigned long long dev_sz;
	struct zgroup_target_info __user *ti_ptr = targets;
	struct zgroup_target_info ti;
	struct zgroup_target *target;
	struct zgroup_bdev *d;

	for (i = 0; i < info->nr_targets; i++) {
		if (copy_from_user(&ti, ti_ptr,
				   sizeof(struct zgroup_target_info)))
			return -EFAULT;
		ti_ptr++;

		/* warn out if the segment size doesn't match the addr */
		if (ti.physical % ZGROUP_DEF_SEG_SIZE ||
		    ti.length % ZGROUP_DEF_SEG_SIZE) {
			pr_warn("unaligned addr %llu-%llu with segment %lu",
				ti.physical, ti.length, ZGROUP_DEF_SEG_SIZE);
			return -EINVAL;
		}

		/* bytes to sector */
		ret = zgroup_target_add(t,
					ti.physical >> SECTOR_SHIFT,
					ti.length >> SECTOR_SHIFT);
		if (ret)
			return ret;
	}

	if (t->nr_targets != t->max_targets) {
		pr_warn("unmatched target number %u %u",
			t->nr_targets, t->max_targets);
		return -EINVAL;
	}

	d = container_of(t, struct zgroup_bdev, table);
	target = t->targets + (t->nr_targets - 1);
	dev_sz = (target->lofs + target->len) << SECTOR_SHIFT;
	if (dev_sz != d->dev_sz) {
		pr_warn("unmatched dev size %llu %llu",
			dev_sz, d->dev_sz);
		return -EINVAL;
	}

	return 0;
}

static int zgroup_targets_init(struct zgroup_table *t, unsigned int nr_targets)
{
	sector_t *keys;
	struct zgroup_target *targets;
	unsigned int align_targets;

	align_targets = roundup(nr_targets, ZGROUP_KEYS_PER_NODE);

	/* Allocate both the key array and target array at once. */
	keys = kvcalloc(align_targets,
			sizeof(sector_t) + sizeof(struct zgroup_target),
			GFP_KERNEL);
	if (!keys)
		return -ENOMEM;

	/* set to maximum value */
	memset(keys, -1, sizeof(sector_t) * align_targets);
	targets = (struct zgroup_target *) (keys + align_targets);

	t->max_targets = nr_targets;
	t->keys = keys;
	t->targets = targets;

	return 0;
}

static int zgroup_table_init(struct zgroup_table *t,
			     struct zgroup_bdev_info *info,
			     struct zgroup_target_info __user *targets)
{
	int ret;

	if (!info->nr_targets || info->nr_targets > ZGROUP_WB_MAX_TARGETS) {
		pr_warn("illegal target number %u, max %d",
			info->nr_targets, ZGROUP_WB_MAX_TARGETS);
		return -EINVAL;
	}

	ret = zgroup_targets_init(t, info->nr_targets);
	if (ret)
		return ret;

	ret = zgroup_targets_add(t, info, targets);
	if (ret)
		return ret;

	ret = zgroup_build_index(t);
	if (ret)
		return ret;

	return 0;
}

static int zgroup_blk_dev_init(struct zgroup_bdev *d,
			       struct zgroup_bdev_info *info)
{
	int ret = 0;
	struct file *blk_file = NULL;
	struct block_device *blk_dev = NULL;
	struct inode *inode;
	struct address_space *mapping;

	blk_file = filp_open_block(info->bdev_path, O_RDWR|O_LARGEFILE, 0);
	if (IS_ERR(blk_file)) {
		ret = PTR_ERR(blk_file);
		blk_file = NULL;
		goto out;
	}

	mapping = blk_file->f_mapping;
	inode = mapping->host;

	/* support only block device for now */
	if (!S_ISBLK(inode->i_mode)) {
		ret = -ENOTBLK;
		goto out;
	}

	blk_dev = blkdev_get_by_dev(inode->i_rdev,
				    BLK_OPEN_READ | BLK_OPEN_WRITE,
				    NULL, NULL);
	if (IS_ERR(blk_dev)) {
		ret = PTR_ERR(blk_dev);
		blk_dev = NULL;
		goto out;
	}

	/* warn out if the segment size doesn't match the dev size */
	if (info->bdev_size % ZGROUP_DEF_SEG_SIZE) {
		pr_warn("unaligned dev size %llu with segment size %lu",
			info->bdev_size, ZGROUP_DEF_SEG_SIZE);
		ret = -EINVAL;
		goto out;
	}

	d->blk_file = blk_file;
	d->blk_dev = blk_dev;
	d->dev_sz = info->bdev_size;
	d->nr_exts = info->bdev_size >> ZGROUP_EXT_SHIFT;
	d->ext_map = kvzalloc(ZGROUP_BITMAP_SIZE(d->nr_exts), GFP_KERNEL);
	if (!d->ext_map)
		ret = -ENOMEM;
	xa_init(&d->managed_ios);

out:
	if (ret) {
		if (blk_dev)
			blkdev_put(blk_dev, NULL);
		if (blk_file)
			filp_close(blk_file, NULL);
	}

	return ret;
}

static void zgroup_bdev_free(struct zgroup_bdev *d)
{
	struct zgroup_crypt_info *ci;
	struct zgroup_table *t;

	if (!d)
		return;
	t = &d->table;
	ci = &d->ci;

	if (ci->crypt) {
		if (!ci->inline_crypt) {
			crypto_free_skcipher(ci->tfm);
		} else if (ci->blk_key) {
			blk_crypto_evict_key(d->blk_dev, ci->blk_key);
			kfree_sensitive(ci->blk_key);
		}
	}

	kvfree(t->keys);

	if (t->depth >= 2)
		kvfree(t->index[t->depth - 2]);

	kvfree(d->ext_map);

	if (d->blk_dev) {
		blkdev_put(d->blk_dev, NULL);
		d->blk_dev = NULL;
	}

	if (d->blk_file) {
		/* hope filp_close flush all of IO */
		filp_close(d->blk_file, NULL);
		d->blk_file = NULL;
	}

	kfree(d);
}

static struct crypto_skcipher *zgroup_alloc_tfm(void)
{
	struct crypto_skcipher *tfm;
	/* AES-256-XTS takes a 64-byte key */
	u8 key[64];
	int ret = 0;

	tfm = crypto_alloc_skcipher("xts(aes)", 0, 0);
	if (IS_ERR(tfm)) {
		pr_err("Error allocating handle: %ld\n", PTR_ERR(tfm));
		tfm = NULL;
		goto out;
	}

	get_random_bytes(key, sizeof(key));
	ret = crypto_skcipher_setkey(tfm, key, sizeof(key));
	if (ret) {
		pr_err("Error setting key: %d\n", ret);
		goto out;
	}

out:
	if (ret && tfm)
		crypto_free_skcipher(tfm);

	memzero_explicit(key, sizeof(key));

	return tfm;
}

/* HACK: reference to get_keyring_key in kering.c */
static int zgroup_get_keyring_key(u32 key_id, u8 *raw_key, size_t *raw_key_sz)
{
	key_ref_t ref;
	struct key *key;
	const struct fscrypt_provisioning_key_payload *payload;
	int ret;

	ref = lookup_user_key(key_id, 0, KEY_NEED_SEARCH);
	if (IS_ERR(ref))
		return PTR_ERR(ref);
	key = key_ref_to_ptr(ref);

	ret = strncmp(key->type->name, "fscrypt-provisioning", 20);
	if (ret)
		goto bad_key;
	payload = key->payload.data[0];

	/* Don't allow fscrypt v1 keys to be used as v2 keys and vice versa. */
	if (payload->type != FSCRYPT_KEY_SPEC_TYPE_IDENTIFIER)
		goto bad_key;

	*raw_key_sz = key->datalen - sizeof(*payload);
	memcpy(raw_key, payload->raw, *raw_key_sz);
	ret = 0;
	goto out_put;

bad_key:
	ret = -EKEYREJECTED;
out_put:
	key_ref_put(ref);
	return ret;
}

static int zgroup_init_key(struct zgroup_bdev *d,
			   enum blk_crypto_mode_num crypto_mode,
			   enum blk_crypto_key_type key_type,
			   key_serial_t key_id)
{
	struct zgroup_crypt_info *ci = &d->ci;
	size_t raw_key_sz;
	u8 raw_key[BLK_CRYPTO_MAX_ANY_KEY_SIZE];
	int ret;

	if (key_id == 0)
		return -EINVAL;

	memset(raw_key, 0, sizeof(raw_key));
	ret = zgroup_get_keyring_key(key_id, raw_key, &raw_key_sz);
	if (ret)
		goto out;

	ci->blk_key = kzalloc(sizeof(struct blk_crypto_key), GFP_KERNEL);
	if (!ci->blk_key) {
		ret = -ENOMEM;
		goto out;
	}

	/* UFS only supports 8 bytes for any DUN */
	ret = blk_crypto_init_key(ci->blk_key, raw_key, raw_key_sz,
				  key_type, crypto_mode,
				  8, PAGE_SIZE);
	if (ret)
		goto out;

	ret = blk_crypto_start_using_key(d->blk_dev, ci->blk_key);
	if (ret)
		goto out;

out:
	if (ret && ci->blk_key)
		kfree_sensitive(ci->blk_key);

	memzero_explicit(raw_key, sizeof(raw_key));

	return ret;
}

static int zgroup_crypt_init(struct zgroup_bdev *d, struct zgroup_bdev_info *info)
{
	int ret = 0;
	u8 crypto_mode, key_type;
	struct zgroup_crypt_info *ci = &d->ci;

	/* bit 1: whether to encrypt */
	ci->crypt = info->crypto_info & 0x1;
	/* bit 2: whether to inline encrypt */
	ci->inline_crypt = info->crypto_info & 0x2;

	if (!ci->crypt && ci->inline_crypt)
		return -EINVAL;

	if (!ci->crypt)
		return ret;

	if (!ci->inline_crypt) {
		ci->tfm = zgroup_alloc_tfm();
		if (!ci->tfm)
			return -EINVAL;
	} else {
		/* bit 16-23: crypto key type */
		key_type = (info->crypto_info >> 16) & 0xff;
		if (key_type != BLK_CRYPTO_KEY_TYPE_HW_WRAPPED
			&& key_type != BLK_CRYPTO_KEY_TYPE_STANDARD)
			return -EINVAL;

		/* bit 24-31: crypto mode */
		crypto_mode = (info->crypto_info >> 24) & 0xff;
		if (crypto_mode != BLK_ENCRYPTION_MODE_AES_256_XTS)
			return -EINVAL;

		ret = zgroup_init_key(d, crypto_mode, key_type, info->key_id);
		if (ret)
			return ret;
	}

	return ret;
}

void zgroup_reset_bdev(struct zram *zram)
{
	if (!zram->zgroup_bdev)
		return;

	zgroup_bdev_free(zram->zgroup_bdev);
	zram->zgroup_bdev = NULL;
}

static int zgroup_set_bdev(struct zram *zram, struct zgroup_bdev_info *info,
			   struct zgroup_target_info __user *targets)
{
	int ret;
	struct zgroup_bdev *d;

	d = kzalloc(sizeof(struct zgroup_bdev), GFP_KERNEL);
	if (!d) {
		ret = -ENOMEM;
		goto out;
	}

	/* backing dev path and size */
	ret = zgroup_blk_dev_init(d, info);
	if (ret) {
		pr_warn("Can't init blk dev\n");
		goto out;
	}

	/* configure crypt */
	ret = zgroup_crypt_init(d, info);
	if (ret) {
		pr_warn("Can't init crypt\n");
		goto out;
	}

	/* target number and range */
	ret = zgroup_table_init(&d->table, info, targets);
	if (ret) {
		pr_warn("Can't init table\n");
		goto out;
	}

	down_write(&zram->init_lock);

	if (init_done(zram)) {
		pr_warn("Can't setup backing device for initialized device\n");
		ret = -EBUSY;
		goto out_unlock;
	}

	if (!zram->zgroup_enable) {
		ret = -EPERM;
		goto out_unlock;
	}

	zgroup_reset_bdev(zram);
	zram->zgroup_bdev = d;
out_unlock:
	up_write(&zram->init_lock);
out:
	if (ret)
		zgroup_bdev_free(d);

	return ret;
}

static int zram_ioctl_set_bdev(struct zgroup_bdev_info __user *ptr)
{
	struct zram *zram;
	struct zgroup_bdev_info info;
	int ret;

	if (copy_from_user(&info, ptr, sizeof(struct zgroup_bdev_info)))
		return -EFAULT;

	mutex_lock(&zram_index_mutex);

	zram = idr_find(&zram_index_idr, info.dev_id);
	if (!zram) {
		ret = -ENODEV;
		goto out;
	}

	ret = zgroup_set_bdev(zram, &info, ptr->targets);
out:
	mutex_unlock(&zram_index_mutex);

	return ret;
}

static int zgroup_get_bdev(struct zram *zram, struct zgroup_bdev_info *info,
			   struct zgroup_target_info __user *targets)
{
	int i;
	int ret = 0;
	struct zgroup_bdev *d;
	struct zgroup_crypt_info *ci;
	struct zgroup_table *t;
	struct zgroup_target *target;
	struct zgroup_target_info ti;
	struct zgroup_target_info __user *ti_ptr = targets;
	dev_t dev;

	down_read(&zram->init_lock);
	if (!zram->zgroup_bdev) {
		ret = -EINVAL;
		goto out;
	}
	d = zram->zgroup_bdev;

	ci = &d->ci;
	dev = d->blk_dev->bd_dev;
	t = &d->table;

	if (info->nr_targets > t->nr_targets) {
		pr_warn("illegal target number %u, max %d",
			info->nr_targets, t->nr_targets);
		ret = -EINVAL;
		goto out;
	}

	memset(info->bdev_path, 0, ZGROUP_PATH_MAX);
	scnprintf(info->bdev_path, ZGROUP_PATH_MAX,
		  "dev=%u:%u", MAJOR(dev), MINOR(dev));
	info->crypto_info = (__u32)ci->crypt | ((__u32)ci->inline_crypt << 1);

	for (i = 0; i < info->nr_targets; i++) {
		target = t->targets + i;
		ti.physical = target->pofs << SECTOR_SHIFT;
		ti.length = target->len << SECTOR_SHIFT;

		if (copy_to_user(ti_ptr, &ti,
				 sizeof(struct zgroup_target_info))) {
			ret = -EFAULT;
			goto out;
		}
		ti_ptr++;
	}
	info->nr_targets = t->nr_targets;
out:
	up_read(&zram->init_lock);

	return ret;
}

static int zram_ioctl_get_bdev(struct zgroup_bdev_info __user *ptr)
{
	struct zram *zram;
	struct zgroup_bdev_info info;
	int ret;

	if (copy_from_user(&info, ptr, sizeof(struct zgroup_bdev_info)))
		return -EFAULT;

	mutex_lock(&zram_index_mutex);

	zram = idr_find(&zram_index_idr, info.dev_id);
	if (!zram) {
		ret = -ENODEV;
		goto out;
	}

	ret = zgroup_get_bdev(zram, &info, ptr->targets);
out:
	mutex_unlock(&zram_index_mutex);

	if (ret)
		return ret;

	if (copy_to_user(ptr, &info, sizeof(struct zgroup_bdev_info)))
		return -EFAULT;

	return ret;
}


long zram_control_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *ptr = (void __user *)arg;
	int ret = -ENOIOCTLCMD;

	switch (cmd) {
	case ZRAM_IOC_SET_BDEV:
		ret = zram_ioctl_set_bdev(ptr);
		break;
	case ZRAM_IOC_GET_BDEV:
		ret = zram_ioctl_get_bdev(ptr);
		break;
	default:
		break;
	}

	return ret;
}

static void zgroup_wb_destroy_wq(void)
{
	if (zgroup_read_wq) {
		destroy_workqueue(zgroup_read_wq);
		zgroup_read_wq = NULL;
	}

	if (zgroup_write_wq) {
		destroy_workqueue(zgroup_write_wq);
		zgroup_write_wq = NULL;
	}
}

int zgroup_wb_init(void)
{
	int ret = 0;

	init_waitqueue_head(&zgroup_fault_wq);

	zgroup_read_wq = alloc_workqueue("zgroup_read",
					 WQ_HIGHPRI | WQ_UNBOUND, 0);
	if (!zgroup_read_wq) {
		ret = -ENOMEM;
		goto out;
	}

	zgroup_write_wq = alloc_workqueue("zgroup_write", 0, 0);
	if (!zgroup_write_wq) {
		ret = -ENOMEM;
		goto out;
	}

	pr_info("zgroup_wb init success");
out:
	if (ret)
		zgroup_wb_destroy_wq();

	return ret;
}

void zgroup_wb_exit(void)
{
	zgroup_wb_destroy_wq();
}
