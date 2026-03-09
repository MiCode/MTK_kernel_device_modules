/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2024, X-Ring technologies Inc., All rights reserved.
 */
#ifndef __ZGROUP_H
#define __ZGROUP_H

#include <linux/blk-crypto.h>
#include <linux/memcontrol.h>

/* HACK: copy from memcontrol.c */
#define ZMEM_CGROUP_ID_MAX	((1UL << MEM_CGROUP_ID_SHIFT) - 1)

#ifdef CONFIG_XRING_ZRAM_MEMCG_DEBUG
#define DBG_BUGON		BUG_ON
#else
#define DBG_BUGON(x)		((void)(x))
#endif

struct zram_group_stats {
	atomic_t obj_pages;
	atomic64_t obj_size;
	atomic64_t obj_fault;
	atomic64_t obj_drop;
#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
	atomic_t wb_exts;
	atomic_t wb_pages;
	atomic64_t wb_size;
	atomic64_t wb_fault;
	atomic64_t wb_drop;
#endif
};

enum zgroup_stat_item {
	ZGROUP_OBJ_PAGES,
	ZGROUP_OBJ_SIZE,
	ZGROUP_OBJ_FAULT,
	ZGROUP_OBJ_DROP,
#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
	ZGROUP_WB_EXTS,
	ZGROUP_WB_PAGES,
	ZGROUP_WB_SIZE,
	ZGROUP_WB_FAULT,
	ZGROUP_WB_DROP,
#endif
	NR_ZGROUP_ITEMS
};

struct zram_group {
	struct zram *zram;
	struct list_head list;

	unsigned long nr_objs;
	unsigned long nr_grps;

	struct ilist_node *obj_list;
	struct ilist_node *grp_obj_list;
	struct ilist_table *obj_tab;

#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
	unsigned long nr_exts;

	struct ilist_node *ext_obj_list;
	struct ilist_node *ext_list;
	struct ilist_node *grp_ext_list;
	struct ilist_table *ext_tab;

	struct zgroup_bdev *bdev;
#endif

	struct zram_group_stats *stats;
};

#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
/* TODO: Should consider adapting to page size, ensure ext > page */
#define ZGROUP_EXT_SHIFT	15
#define ZGROUP_EXT_SIZE		(_AC(1, UL) << ZGROUP_EXT_SHIFT)
#define ZGROUP_PAGE_PER_EXT	(ZGROUP_EXT_SIZE >> PAGE_SHIFT)
#define ZGROUP_SEC_PER_EXT	(ZGROUP_EXT_SIZE >> SECTOR_SHIFT)

/* HACK: should be defined in uapi */
#define ZRAM_IOC_SET_BDEV _IOW(0x5C, 1, struct zgroup_bdev_info)
#define ZRAM_IOC_GET_BDEV _IOWR(0x5C, 2, struct zgroup_bdev_info)
#define ZGROUP_PATH_MAX 128

struct zgroup_target_info {
	__u64 physical;
	__u64 length;
};

struct zgroup_bdev_info {
	__u32 dev_id;
	__u32 reserved1;

	__u32 crypto_info;
	__u32 key_id;

	char bdev_path[ZGROUP_PATH_MAX];
	__u64 bdev_size;

	__u32 reserved2;
	__u32 nr_targets;
	struct zgroup_target_info targets[];
};

struct zgroup_crypt_info {
	bool crypt;
	bool inline_crypt;

	struct crypto_skcipher *tfm;
	struct blk_crypto_key *blk_key;
};

struct zgroup_target {
	sector_t lofs;
	sector_t pofs;
	sector_t len;
};

struct zgroup_table {
	/* btree table */
#define ZGROUP_WB_MAX_DEPTH	5
	unsigned int depth;
	unsigned int nr_index[ZGROUP_WB_MAX_DEPTH];
	sector_t *index[ZGROUP_WB_MAX_DEPTH];

	unsigned int nr_targets;
	unsigned int max_targets;
	sector_t *keys;
	struct zgroup_target *targets;
};

struct zgroup_bdev {
	struct file *blk_file;
	struct block_device *blk_dev;

	struct zgroup_crypt_info ci;

	u64 dev_sz;
	unsigned int nr_exts;
	unsigned long *ext_map;
	atomic_t last_ext;
	struct zgroup_table table;

	struct xarray managed_ios;
};

enum zgroup_io_step {
	ZIO_STARTED,
	ZIO_INFLIGHT,
	ZIO_COMPLETE,
};

struct zgroup_io {
	union {
		struct {
			struct zgroup_io *next;
			struct zgroup_pool *pool;
		};
		struct rcu_head rcu;
	};
	struct zram_group *zgroup;
	u32 eid;
	u16 gid;
	bool free_ext;

	int status;
	enum req_op op;
	struct page **pages;
	struct page **crypt_pages;

	atomic_t step;
	struct kref kref;
	struct completion wait;

#ifdef CONFIG_XRING_ZRAM_MEMCG_DEBUG
	struct task_struct *owner;
	struct bio *bio;
#endif
};

struct zgroup_req {
	struct zgroup_pool *pool;

	struct zgroup_io *zio;
	struct zgroup_io *ziotail;
	unsigned short nr_vecs;

	int status;
	struct work_struct work;
};

struct zgroup_pool {
	struct kref kref;
	spinlock_t lock;
	unsigned int nr_pages;
	struct list_head head;
};

struct zgroup_plug {
	struct zgroup_pool *pool;
	struct zgroup_req *req;
	struct zram_group *zgroup;
	u16 gid;
	enum req_op op;
};

extern wait_queue_head_t zgroup_fault_wq;

#define zio_for_each(pos, req) \
	for (pos = (req)->zio; pos; pos = pos->next)

#define zio_for_each_safe(pos, n, req) \
	for (pos = (req)->zio, n = pos->next; \
	     pos; pos = n, n = pos->next)

void zgroup_stats_wb_size_dec(struct zram_group *zgroup,
			      unsigned short gid, size_t size);
void zgroup_stats_wb_ext_dec(struct zram_group *zgroup, unsigned short gid);
void zgroup_wb_del_obj(struct zram_group *zgroup, u32 idx, u32 eid);
void zgroup_wb_del_ext_nolock(struct zram_group *zgroup, u32 idx, u16 gid);
bool zgroup_shrink_idxs(struct ilist_table *tab, u32 hidx, u32 *idxs, int *cnt);
void zgroup_try_to_free_ext(struct zgroup_bdev *d, u32 eid);
long zgroup_write_ext(struct zgroup_plug *plug);
long zgroup_write_ext_sync(struct zgroup_plug *plug);
long zgroup_read_ext(struct zgroup_plug *plug, u32 eid);
int zgroup_init_pool(struct zgroup_plug *plug);
void zgroup_put_pool(struct zgroup_pool *pool);
struct page *zgroup_alloc_page(struct zgroup_pool *pool, gfp_t gfp);
int zgroup_start_plug(struct zgroup_plug *plug, struct zgroup_io *zio);
int zgroup_finish_plug(struct zgroup_plug *plug);
unsigned long zgroup_memcg_wb(struct mem_cgroup *memcg, unsigned long sz_to_wb,
			      unsigned long *io_wb, bool sync);
unsigned long zgroup_memcg_rb(struct mem_cgroup *memcg, unsigned long sz_to_rb);
#endif

void zgroup_stats_obj_size_inc(struct zram_group *zgroup,
			       unsigned short gid, size_t size);
void zgroup_stats_obj_size_dec(struct zram_group *zgroup,
			       unsigned short gid, size_t size);
unsigned long zgroup_get_memcg_stat(struct mem_cgroup *memcg,
				    enum zgroup_stat_item item);
bool zgroup_get_enable(void);
void zgroup_add_obj(struct zram_group *zgroup, u32 idx, u16 gid);
void zgroup_del_obj(struct zram_group *zgroup, u32 idx, u16 gid);
#endif
