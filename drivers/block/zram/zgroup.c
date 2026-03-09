// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright(c) 2024 X-Ring technologies Inc.
 */

#define pr_fmt(fmt) "zgroup: " fmt

#include <linux/printk.h>
#include <linux/blkdev.h>
#include <linux/vmalloc.h>

#include "ilist.h"
#include "internal.h"
#include "zram_drv.h"
#include "zgroup.h"
#include "xswapd.h"

static DECLARE_RWSEM(zgroup_rwsem);
static LIST_HEAD(zgroup_head);

void zgroup_stats_obj_size_inc(struct zram_group *zgroup,
			       unsigned short gid, size_t size)
{
	atomic_inc(&zgroup->stats[gid].obj_pages);
	atomic_inc(&zgroup->stats[0].obj_pages);
	atomic64_add(size, &zgroup->stats[gid].obj_size);
	atomic64_add(size, &zgroup->stats[0].obj_size);
}

void zgroup_stats_obj_size_dec(struct zram_group *zgroup,
			       unsigned short gid, size_t size)
{
	atomic_dec(&zgroup->stats[gid].obj_pages);
	atomic_dec(&zgroup->stats[0].obj_pages);
	atomic64_sub(size, &zgroup->stats[gid].obj_size);
	atomic64_sub(size, &zgroup->stats[0].obj_size);
}

static void zgroup_stats_obj_fault_inc(struct zram_group *zgroup,
				       unsigned short gid)
{
	atomic64_inc(&zgroup->stats[gid].obj_fault);
	atomic64_inc(&zgroup->stats[0].obj_fault);
}

static void zgroup_stats_obj_drop_inc(struct zram_group *zgroup,
				      unsigned short gid, size_t size)
{
	atomic64_add(size, &zgroup->stats[gid].obj_drop);
	atomic64_add(size, &zgroup->stats[0].obj_drop);
}

#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
static void zgroup_stats_wb_drop_inc(struct zram_group *zgroup,
				     unsigned short gid, size_t size)
{
	atomic64_add(size, &zgroup->stats[gid].wb_drop);
	atomic64_add(size, &zgroup->stats[0].wb_drop);
}
#endif

static unsigned long __zgroup_get_memcg_stat(struct zram_group *zgroup,
					     unsigned short gid,
					     enum zgroup_stat_item item)
{
	switch (item) {
	case ZGROUP_OBJ_PAGES:
		return atomic_read(&zgroup->stats[gid].obj_pages);
	case ZGROUP_OBJ_SIZE:
		return atomic64_read(&zgroup->stats[gid].obj_size);
	case ZGROUP_OBJ_FAULT:
		return atomic64_read(&zgroup->stats[gid].obj_fault);
	case ZGROUP_OBJ_DROP:
		return atomic64_read(&zgroup->stats[gid].obj_drop);
#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
	case ZGROUP_WB_EXTS:
		return atomic_read(&zgroup->stats[gid].wb_exts);
	case ZGROUP_WB_PAGES:
		return atomic_read(&zgroup->stats[gid].wb_pages);
	case ZGROUP_WB_SIZE:
		return atomic64_read(&zgroup->stats[gid].wb_size);
	case ZGROUP_WB_FAULT:
		return atomic64_read(&zgroup->stats[gid].wb_fault);
	case ZGROUP_WB_DROP:
		return atomic64_read(&zgroup->stats[gid].wb_drop);
#endif
	default:
		pr_warn_ratelimited("unknown stat item %u", item);
		break;
	}

	return 0;
}

unsigned long zgroup_get_memcg_stat(struct mem_cgroup *memcg,
				    enum zgroup_stat_item item)
{
	struct zram_group *zgroup;
	unsigned long size = 0;
	unsigned short gid;

	gid = memcg ? mem_cgroup_id(memcg) : 0;

	down_read(&zgroup_rwsem);
	list_for_each_entry(zgroup, &zgroup_head, list)
		size += __zgroup_get_memcg_stat(zgroup, gid, item);
	up_read(&zgroup_rwsem);

	return size;
}

#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
static unsigned long __zgroup_memcg_wb(struct zram_group *zgroup, u16 gid,
				       unsigned long sz_to_wb,
				       unsigned long *io_wb, bool sync)
{
	unsigned long sz_wb = 0;
	long ret;
	struct zgroup_plug plug = {
		.zgroup = zgroup,
		.gid = gid,
		.op = REQ_OP_WRITE,
	};

	if (!sync) {
		ret = zgroup_init_pool(&plug);
		if (ret)
			goto out;

		ret = zgroup_start_plug(&plug, NULL);
		if (ret)
			goto out_put;
	}

	while (sz_wb < sz_to_wb && !xswapd_stop_wb()) {
		if (!sync)
			ret = zgroup_write_ext(&plug);
		else
			ret = zgroup_write_ext_sync(&plug);
		if (ret <= 0)
			break;
		sz_wb += ret;
		*io_wb += ZGROUP_EXT_SIZE;
	}

	if (!sync) {
		ret = zgroup_finish_plug(&plug);
out_put:
		zgroup_put_pool(plug.pool);
	}
out:
	if (ret < 0)
		pr_err("failed to write ext from group %u ret %ld", gid, ret);
	return sz_wb;
}

unsigned long zgroup_memcg_wb(struct mem_cgroup *memcg, unsigned long sz_to_wb,
			      unsigned long *io_wb, bool sync)
{
	struct zram_group *zgroup;
	unsigned long sz_wb = 0;
	unsigned long io_per_wb;
	unsigned short gid;

	gid = memcg ? mem_cgroup_id(memcg) : 0;

	down_read(&zgroup_rwsem);
	list_for_each_entry(zgroup, &zgroup_head, list) {
		if (!zgroup->bdev)
			continue;

		io_per_wb = 0;
		sz_wb += __zgroup_memcg_wb(zgroup, gid,
					   sz_to_wb - sz_wb, &io_per_wb, sync);
		*io_wb += io_per_wb;
		if (sz_wb >= sz_to_wb)
			break;
	}
	up_read(&zgroup_rwsem);

	return sz_wb;
}

static unsigned long __zgroup_memcg_rb(struct zram_group *zgroup, u16 gid,
				       unsigned long sz_to_rb)
{
	bool last;
	u32 eid, hidx;
	int cnt;
	long ret;
	unsigned long sz_rb = 0;
	struct zgroup_plug plug = {
		.zgroup = zgroup,
		.gid = gid,
		.op = REQ_OP_READ,
	};

	ret = zgroup_init_pool(&plug);
	if (ret)
		goto out;

	ret = zgroup_start_plug(&plug, NULL);
	if (ret)
		goto out_put;

	hidx = gid + zgroup->nr_exts;
	while (sz_rb < sz_to_rb) {
		cnt = 1;
		last = zgroup_shrink_idxs(zgroup->ext_tab, hidx, &eid, &cnt);
		if (cnt == 0 && last)
			break;

		ret = zgroup_read_ext(&plug, eid);
		if (ret < 0)
			break;
		sz_rb += ret;
	}

	ret = zgroup_finish_plug(&plug);
	if (ret)
		goto out_put;

out_put:
	zgroup_put_pool(plug.pool);
out:
	if (ret)
		pr_err("failed to read ext from group %u ret %ld", gid, ret);
	return sz_rb;
}

unsigned long zgroup_memcg_rb(struct mem_cgroup *memcg, unsigned long sz_to_rb)
{
	struct zram_group *zgroup;
	unsigned long sz_rb = 0;
	unsigned short gid;

	gid = memcg ? mem_cgroup_id(memcg) : 0;

	down_read(&zgroup_rwsem);
	list_for_each_entry(zgroup, &zgroup_head, list) {
		if (!zgroup->bdev)
			continue;

		sz_rb += __zgroup_memcg_rb(zgroup, gid, sz_to_rb - sz_rb);
		if (sz_rb >= sz_to_rb)
			break;
	}
	up_read(&zgroup_rwsem);

	return sz_rb;
}
#endif

bool zgroup_get_enable(void)
{
	bool enable;

	down_read(&zgroup_rwsem);
	enable = !list_empty(&zgroup_head);
	up_read(&zgroup_rwsem);

	return enable;
}

/* move obj to group */
void zgroup_add_obj(struct zram_group *zgroup, u32 idx, u16 gid)
{
	u32 hidx;

	hidx = gid + zgroup->nr_objs;
	ilist_add(hidx, idx, zgroup->obj_tab);
	pr_debug("add obj idx %u hidx %u", idx, hidx);
}

/* move obj from group to the isolated */
void zgroup_del_obj(struct zram_group *zgroup, u32 idx, u16 gid)
{
	u32 hidx;

	hidx = gid + zgroup->nr_objs;
	ilist_del(hidx, idx, zgroup->obj_tab);
	pr_debug("del obj idx %u hidx %u", idx, hidx);
}

int zgroup_track_obj_fault(struct zram *zram, u32 index)
{
	unsigned short gid;

	if (!zram->zgroup)
		return 0;

	gid = zram_get_memcg_id(zram, index);
	if (!gid)
		return 0;

	zgroup_stats_obj_fault_inc(zram->zgroup, gid);

#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
	return zgroup_read_obj(zram, index);
#else
	return 0;
#endif
}

/*
 * Currently we don't track recompress or partial objs.
 * Must be called with ZRAM_LOCK held.
 */
void zgroup_track_obj(struct zram *zram, u32 index, struct mem_cgroup *memcg)
{
	struct zram_group *zgroup = zram->zgroup;
	unsigned short gid;
	size_t size;

	if (!zgroup)
		return;

	if (!memcg)
		return;

	gid = mem_cgroup_id(memcg);
	if (!gid)
		return;
	size = zram_get_obj_size(zram, index);

	zram_set_memcg_id(zram, index, gid);
	zgroup_add_obj(zgroup, index, gid);
	zgroup_stats_obj_size_inc(zgroup, gid, size);
	if (zram_test_flag(zram, index, ZRAM_SAME))
		ilist_set_priv(index, zgroup->obj_tab);
}

/* Must be called with ZRAM_LOCK held */
void zgroup_untrack_obj(struct zram *zram, u32 index)
{
	struct zram_group *zgroup = zram->zgroup;
	unsigned short gid;
	size_t size;
#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
	u32 eid, hidx;
#endif
	if (!zgroup)
		return;

#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
repeat:
	if (zram_test_flag(zram, index, ZRAM_GROUP_FAULT)) {
		zram_slot_unlock(zram, index);
		wait_event(zgroup_fault_wq,
			   !zram_test_flag(zram, index, ZRAM_GROUP_FAULT));
		zram_slot_lock(zram, index);
		goto repeat;
	}
#endif

	gid = zram_get_memcg_id(zram, index);
	if (!gid)
		return;
	size = zram_get_obj_size(zram, index);

#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
	if (zram_test_flag(zram, index, ZRAM_GROUP_WB)) {
		eid = zram_get_handle(zram, index) / ZGROUP_EXT_SIZE;

		zgroup_wb_del_obj(zgroup, index, eid);
		zgroup_stats_wb_size_dec(zgroup, gid, size);
		zgroup_stats_wb_drop_inc(zgroup, gid, size);

		hidx = eid + zgroup->nr_objs + zgroup->nr_grps;
		if (ilist_is_isolated(hidx, zgroup->obj_tab)) {
			hidx = gid + zgroup->nr_exts;
			ilist_lock(hidx, zgroup->ext_tab);

			if (!ilist_is_isolated(eid, zgroup->ext_tab)) {
				zgroup_wb_del_ext_nolock(zgroup, eid, gid);
				zgroup_stats_wb_ext_dec(zgroup, gid);
				zgroup_try_to_free_ext(zgroup->bdev, eid);
			}

			ilist_unlock(hidx, zgroup->ext_tab);
		}

		zram_clear_flag(zram, index, ZRAM_GROUP_WB);
		zram_set_memcg_id(zram, index, 0);

		/* see zram_free_page */
		atomic64_sub(zram_get_obj_size(zram, index),
				&zram->stats.compr_data_size);
		atomic64_dec(&zram->stats.pages_stored);
		zram_set_handle(zram, index, 0);
		zram_set_obj_size(zram, index, 0);
		return;
	}
#endif

	zgroup_del_obj(zgroup, index, gid);
	zram_set_memcg_id(zram, index, 0);
	zgroup_stats_obj_size_dec(zgroup, gid, size);
	zgroup_stats_obj_drop_inc(zgroup, gid, size);
	if (zram_test_flag(zram, index, ZRAM_SAME))
		ilist_clear_priv(index, zgroup->obj_tab);
}

#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
static struct ilist_node *zgroup_ext_get_node(u32 idx, void *priv)
{
	struct zram_group *zgroup = priv;

	if (idx < zgroup->nr_exts)
		return &zgroup->ext_list[idx];

	idx -= zgroup->nr_exts;
	if (idx < zgroup->nr_grps)
		return &zgroup->grp_ext_list[idx];

	return NULL;
}

static void zgroup_wb_meta_free(struct zram_group *zgroup)
{
	ilist_table_free(zgroup->ext_tab);
	vfree(zgroup->grp_ext_list);
	vfree(zgroup->ext_list);
	vfree(zgroup->ext_obj_list);
	zgroup->ext_tab = NULL;
	zgroup->grp_ext_list = NULL;
	zgroup->ext_list = NULL;
	zgroup->ext_obj_list = NULL;
}

static int zgroup_wb_meta_alloc(struct zram_group *zgroup)
{
	int ret, i;
	unsigned long nr_objs, nr_grps, nr_exts;
	struct zram *zram = zgroup->zram;

	if (!zram->zgroup_bdev)
		return 0;
	zgroup->bdev = zram->zgroup_bdev;

	nr_objs = zgroup->nr_objs;
	nr_grps = zgroup->nr_grps;
	nr_exts = zram->zgroup_bdev->nr_exts;

	/* index starts from 0 */
	if (nr_objs + nr_grps + nr_exts - 1 > ILIST_IDX_MAX)
		return -EINVAL;

	zgroup->nr_exts = nr_exts;

	zgroup->ext_obj_list = vzalloc(sizeof(struct ilist_node) * nr_exts);
	if (!zgroup->ext_obj_list) {
		ret = -ENOMEM;
		goto err;
	}

	zgroup->ext_list = vzalloc(sizeof(struct ilist_node) * nr_exts);
	if (!zgroup->obj_list) {
		ret = -ENOMEM;
		goto err;
	}
	zgroup->grp_ext_list = vzalloc(sizeof(struct ilist_node) * nr_grps);
	if (!zgroup->grp_ext_list) {
		ret = -ENOMEM;
		goto err;
	}

	zgroup->ext_tab = ilist_table_alloc(zgroup_ext_get_node, zgroup);
	if (!zgroup->ext_tab) {
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < nr_exts; i++)
		ilist_node_init(i + nr_objs + nr_grps, zgroup->obj_tab);
	for (i = 0; i < nr_exts; i++)
		ilist_node_init(i, zgroup->ext_tab);
	for (i = 0; i < nr_grps; i++)
		ilist_node_init(i + zgroup->nr_exts, zgroup->ext_tab);

	return 0;

err:
	zgroup_wb_meta_free(zgroup);
	return ret;
}
#else
static inline void zgroup_wb_meta_free(struct zram_group *zgroup) {}
static inline int zgroup_wb_meta_alloc(struct zram_group *zgroup) { return 0; }
#endif

static struct ilist_node *zgroup_obj_get_node(u32 idx, void *priv)
{
	struct zram_group *zgroup = priv;

	if (idx < zgroup->nr_objs)
		return &zgroup->obj_list[idx];

	idx -= zgroup->nr_objs;
	if (idx < zgroup->nr_grps)
		return &zgroup->grp_obj_list[idx];

#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
	idx -= zgroup->nr_grps;
	if (idx < zgroup->nr_exts)
		return &zgroup->ext_obj_list[idx];
#endif

	return NULL;
}

static void zgroup_comp_meta_free(struct zram_group *zgroup)
{
	vfree(zgroup->stats);
	ilist_table_free(zgroup->obj_tab);
	vfree(zgroup->grp_obj_list);
	vfree(zgroup->obj_list);
	zgroup->stats = NULL;
	zgroup->obj_tab = NULL;
	zgroup->grp_obj_list = NULL;
	zgroup->obj_list = NULL;
}

static int zgroup_comp_meta_alloc(struct zram_group *zgroup,
				  unsigned long nr_objs, unsigned long nr_grps)
{
	int ret, i;

	if (nr_objs + nr_grps - 1 > ILIST_IDX_MAX)
		return -EINVAL;

	zgroup->nr_objs = nr_objs;
	zgroup->nr_grps = nr_grps;

	zgroup->obj_list = vzalloc(sizeof(struct ilist_node) * nr_objs);
	if (!zgroup->obj_list) {
		ret = -ENOMEM;
		goto err;
	}

	zgroup->grp_obj_list = vzalloc(sizeof(struct ilist_node) * nr_grps);
	if (!zgroup->grp_obj_list) {
		ret = -ENOMEM;
		goto err;
	}

	zgroup->obj_tab = ilist_table_alloc(zgroup_obj_get_node, zgroup);
	if (!zgroup->obj_tab) {
		ret = -ENOMEM;
		goto err;
	}

	zgroup->stats = vzalloc(sizeof(struct zram_group_stats) * nr_grps);
	if (!zgroup->stats) {
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < nr_objs; i++)
		ilist_node_init(i, zgroup->obj_tab);
	for (i = 0; i < nr_grps; i++)
		ilist_node_init(i + zgroup->nr_objs, zgroup->obj_tab);

	return 0;
err:
	zgroup_comp_meta_free(zgroup);
	return ret;
}

static void zgroup_add(struct zram_group *zgroup)
{
	down_write(&zgroup_rwsem);
	list_add(&zgroup->list, &zgroup_head);
	up_write(&zgroup_rwsem);
}

static void zgroup_remove(struct zram_group *zgroup)
{
	down_write(&zgroup_rwsem);
	list_del(&zgroup->list);
	up_write(&zgroup_rwsem);
}

int zgroup_alloc(struct zram *zram, size_t nr_objs)
{
	int ret;
	struct zram_group *zgroup;

	if (!zram->zgroup_enable)
		return 0;

	zgroup = kzalloc(sizeof(struct zram_group), GFP_KERNEL);
	if (!zgroup)
		return -ENOMEM;
	zgroup->zram = zram;

	/*
	 * According to mem_cgroup_alloc, the range of mem_cgroup_id is from 1
	 * to MEM_CGROUP_ID_MAX, so we should reserve 0.
	 */
	ret = zgroup_comp_meta_alloc(zgroup, nr_objs, ZMEM_CGROUP_ID_MAX + 1);
	if (ret)
		goto err;

	ret = zgroup_wb_meta_alloc(zgroup);
	if (ret)
		goto err;

	zgroup_add(zgroup);
	zram->zgroup = zgroup;

#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
	pr_info("zram%d zgroup init success, obj %lu, ext %lu",
		zram->disk->first_minor, zgroup->nr_objs, zgroup->nr_exts);
#else
	pr_info("zram%d zgroup init success, obj %lu",
		zram->disk->first_minor, zgroup->nr_objs);
#endif

err:
	if (ret) {
		zgroup_wb_meta_free(zgroup);
		zgroup_comp_meta_free(zgroup);
		kfree(zgroup);
	}

	return 0;
}

void zgroup_free(struct zram *zram)
{
	struct zram_group *zgroup = zram->zgroup;

	if (!zgroup)
		return;

	zram->zgroup = NULL;

	zgroup_remove(zgroup);
	zgroup_wb_meta_free(zgroup);
	zgroup_comp_meta_free(zgroup);
	kfree(zgroup);
}
