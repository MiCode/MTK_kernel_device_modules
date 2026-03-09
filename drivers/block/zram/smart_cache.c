// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright(c) 2024 X-Ring technologies Inc.
 */

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/memcontrol.h>
#include <linux/mm_inline.h>
#include <linux/mmzone.h>
#include <linux/page-flags.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/writeback.h>
#include <linux/shmem_fs.h>
#include <linux/pagemap.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/swap.h>
#include <linux/dax.h>
#include <linux/mm.h>
#include <linux/vmstat.h>
#include <asm-generic/bitsperlong.h>
#include <linux/xarray.h>
#include <linux/numa.h>
#include <linux/kernfs.h>
#include <linux/page_ref.h>
#include <linux/pagevec.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <trace/hooks/mm.h>
#include <trace/hooks/vmscan.h>

#include <uapi/linux/magic.h>

#include "mctrl.h"

/* redefine pr_fmt "smart_cache: " */
#undef pr_fmt
#define pr_fmt(fmt) "smart_cache: " fmt

/* HACK: need to avoid conflict with i_state flag in fs.h */
#define I_CACHE_LEVEL_SHIFT	50
#define I_CACHE_LEVEL_MASK	(((1UL << MEMCG_LEVEL_MAX) - 1) << I_CACHE_LEVEL_SHIFT)

/* HACK: should be defined in page-flags.h */
#define PF_HEAD(page, enforce)	PF_POISONED_CHECK(compound_head(page))
PAGEFLAG(OemReserved4, oem_reserved_4, PF_HEAD)
#undef PF_HEAD

#define FILE_SHRINK_TIMEOUT (5 * HZ)

struct smart_cache_info {
	bool enable;

	unsigned int ratio;
	unsigned long isolate_lru_fail_cnt;
	unsigned long isolate_lru_clear_fail_cnt;
	unsigned long memcg_move_fail_cnt;
};

static struct smart_cache_info sci;

/* HACK: copy from memcontrol.c */
void folio_putback_lru(struct folio *folio)
{
	folio_add_lru(folio);
	folio_put(folio);		/* drop ref from isolate */
}

static bool sc_isolate_folio(struct lruvec *lruvec, struct folio *folio)
{
	if (!folio_test_lru(folio)) {
		sci.isolate_lru_fail_cnt++;
		pr_debug("SC: isolate_lru_fail_cnt=%lu\n", sci.isolate_lru_fail_cnt);
		return false;
	}

	if (unlikely(!folio_try_get(folio)))
		return false;

	if (!folio_test_clear_lru(folio)) {
		sci.isolate_lru_clear_fail_cnt++;
		pr_debug("SC: isolate_lru_clear_fail_cnt=%lu\n", sci.isolate_lru_clear_fail_cnt);
		/* Another thread is already isolating this folio */
		folio_put(folio);
		return false;
	}

	lruvec_del_folio(lruvec, folio);
	return true;
}

/* TODO: need lru_add_drain ? */
static unsigned long sc_shrink_lruvec(struct mem_cgroup *memcg,
				      struct lruvec *lruvec,
				      unsigned long nr_to_scan)
{
	enum lru_list lru;
	struct folio *folio;
	struct list_head *src;
	LIST_HEAD(dst);
	unsigned long nr_scan = 0;
	unsigned long nr_move = 0;

	spin_lock_irq(&lruvec->lru_lock);
	for (lru = LRU_INACTIVE_FILE; lru <= LRU_ACTIVE_FILE; lru++) {
		src = &lruvec->lists[lru];

		while (nr_scan < nr_to_scan && !list_empty(src)) {
			struct folio *folio;

			folio = lru_to_folio(src);
			nr_scan += folio_nr_pages(folio);

			if (sc_isolate_folio(lruvec, folio))
				list_add(&folio->lru, &dst);
			else
				list_move(&folio->lru, src);
		}
	}
	spin_unlock_irq(&lruvec->lru_lock);

	while (!list_empty(&dst)) {
		folio = lru_to_folio(&dst);
		list_del(&folio->lru);

		if (!folio_trylock(folio))
			goto put;

		if (!mem_cgroup_move_account(folio, true,
					     memcg, root_mem_cgroup))
			nr_move += folio_nr_pages(folio);

		folio_unlock(folio);
put:
		folio_putback_lru(folio);
	}

	pr_debug("scan %lu move %lu", nr_scan, nr_move);

	return nr_move;
}

static unsigned long sc_shrink_mglruvec(struct mem_cgroup *memcg, struct lruvec *lruvec, unsigned long nr_to_scan)
{
	int gen, zone;
	struct list_head *src;
	struct folio *folio;
	LIST_HEAD(dst);
	int nr_move = 0;
	unsigned long nr_scan = 0;
	unsigned long seq;

	spin_lock_irq(&lruvec->lru_lock);
	for (seq = lruvec->lrugen.min_seq[LRU_GEN_FILE]; seq <= lruvec->lrugen.max_seq; seq++) {
		gen = seq % MAX_NR_GENS;
		for (zone = MAX_NR_ZONES - 1; zone >= 0; zone--) {
			src = &lruvec->lrugen.folios[gen][LRU_GEN_FILE][zone];
			while (nr_scan < nr_to_scan && !list_empty(src)) {
				folio = lru_to_folio(src);
				nr_scan += folio_nr_pages(folio);

				if (sc_isolate_folio(lruvec, folio))
					list_add(&folio->lru, &dst);
				else {
					pr_debug("SC: sc_isolate_folio fail, nr_move=%d\n", nr_move);

					list_move(&folio->lru, src);
				}
			}
		}
	}
	spin_unlock_irq(&lruvec->lru_lock);

	while (!list_empty(&dst)) {
		folio = lru_to_folio(&dst);
		list_del(&folio->lru);

		if (!folio_trylock(folio)) {
			pr_debug("SC: folio_trylock fail, nr_move=%d\n", nr_move);

			goto put;
		}

		if (!mem_cgroup_move_account(folio, true, memcg, root_mem_cgroup))
			nr_move += folio_nr_pages(folio);

		folio_unlock(folio);

put:
		folio_putback_lru(folio);
	}

	pr_debug("SC: nr_move=%d\n", nr_move);

	return nr_move;
}

static unsigned long sc_shrink_memcg(pg_data_t *pgdat,
				     struct mem_cgroup *memcg,
				     unsigned long nr_to_reclaim)
{
	struct lruvec *lruvec;

	lruvec = mem_cgroup_lruvec(memcg, pgdat);

	pr_debug("SC: lrugen=%d\n", lruvec->lrugen.enabled);

	if (!lruvec->lrugen.enabled)
		return sc_shrink_lruvec(memcg, lruvec, nr_to_reclaim);
	else
		return sc_shrink_mglruvec(memcg, lruvec, nr_to_reclaim);

	return 0;
}

static unsigned long memcg_file_pages(pg_data_t *pgdat,
				      struct mem_cgroup *memcg)
{
	struct lruvec *lruvec;

	if (!memcg)
		return 0;

	lruvec = mem_cgroup_lruvec(memcg, pgdat);

	return lruvec_page_state(lruvec, NR_ACTIVE_FILE) +
	       lruvec_page_state(lruvec, NR_INACTIVE_FILE);
}

static void sc_shrink_memcg_by_max(pg_data_t *pgdat)
{
	struct mem_cgroup *memcg = NULL;
	struct memcg_ctrl *mctrl;
	unsigned long nr_file;

	while ((memcg = get_next_memcg(memcg, true))) {
		mctrl = MEMCG_OEM_DATA(memcg);
		nr_file = memcg_file_pages(pgdat, memcg);

		pr_debug("SC: nr_file=%lu(max=%lu)\n", nr_file, mctrl->max);

		if (nr_file > mctrl->max)
			sc_shrink_memcg(pgdat, memcg, nr_file - mctrl->max);
	}
}

static void sc_shrink_memcg_by_ratio(pg_data_t *pgdat)
{
	struct mem_cgroup *memcg = NULL;
	unsigned long nr_total, nr_can_cache, nr_to_recalim;
	unsigned long nr_cache = 0;
	unsigned long nr_reclaimed = 0;

	nr_total = node_page_state_pages(pgdat, NR_ACTIVE_FILE) +
			node_page_state_pages(pgdat, NR_INACTIVE_FILE);
	while ((memcg = get_next_memcg(memcg, true)))
		nr_cache += memcg_file_pages(pgdat, memcg);

	nr_can_cache = nr_total * sci.ratio / 100;
	nr_to_recalim = nr_cache > nr_can_cache ?
			nr_cache - nr_can_cache : 0;

	pr_debug("SC: nr_total=%lu, ratio=%u, nr_can_cache=%lu, nr_cache=%lu, nr_to_recalim=%lu\n", nr_total, sci.ratio, nr_can_cache, nr_cache, nr_to_recalim);

	if (!nr_to_recalim)
		return;

	while ((memcg = get_next_memcg(memcg, true))) {
		pr_debug("SC: nr_to_recalim=%lu(nr_can_cache=%lu)\n", nr_to_recalim, nr_can_cache);
		nr_reclaimed += sc_shrink_memcg(pgdat, memcg,
						nr_to_recalim - nr_reclaimed);

		if (nr_reclaimed >= nr_to_recalim) {
			get_next_memcg_break(memcg);
			break;
		}
	}
}

static bool sc_skip_reclaim(struct mem_cgroup *memcg)
{
	struct memcg_ctrl *mctrl;

	pr_debug("SC: enable=%d\n", sci.enable);

	if (!sci.enable)
		return true;

	if (!memcg || mem_cgroup_is_root(memcg))
		return false;

	mctrl = MEMCG_OEM_DATA(memcg);

	pr_debug("SC: level=%u\n", mctrl->level);

	if (mctrl && mctrl->level)
		return false;

	return true;
}

static void sc_should_memcg_bypass(struct mem_cgroup *memcg, bool *bypass)
{
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);

	if (!sci.enable)
		return;

	if (!mctrl)
		return;

	if (mctrl->level)
		*bypass = true;
}

static void vh_filemap_add_folio(void *data, struct address_space *mapping,
				 struct folio *folio, pgoff_t index)
{
	if (!sci.enable)
		return;

	if (!folio || !mapping || !mapping->host)
		return;

	if (folio_test_hugetlb(folio))
		return;

	if (mem_cgroup_disabled())
		return;

	if (!(mapping->host->i_state & I_CACHE_LEVEL_MASK))
		return;

	/* mark cache folio */
	folio_set_oem_reserved_4(folio);
	/* HACK: set mapping to pass parameter temporarily */
	folio->private = mapping;
}

static void vh_mem_cgroup_charge(void *data, struct folio *folio,
				 struct mem_cgroup **memcg)
{
	unsigned long level_mask;
	unsigned int level;
	struct mem_cgroup *cache_memcg;
	struct address_space *mapping;

	if (!folio_test_oem_reserved_4(folio))
		return;

	mapping = folio->private;
	level_mask = (mapping->host->i_state & I_CACHE_LEVEL_MASK);
	for (level = 1; level < MEMCG_LEVEL_MAX + 1; level++) {
		if (mapping->host->i_state &
		    1UL << (I_CACHE_LEVEL_SHIFT + level - 1))
			break;
	}
	if (level == MEMCG_LEVEL_MAX + 1)
		goto out;

	cache_memcg = get_cache_memcg(level);
	rcu_read_lock();
	if (cache_memcg && css_tryget_online(&cache_memcg->css)) {
		css_put(&(*memcg)->css);
		*memcg = cache_memcg;
	}
	rcu_read_unlock();

out:
	folio_clear_oem_reserved_4(folio);
	folio->private = NULL;
}

/* TODO: need hook mem_cgroup_soft_limit_reclaim ? */
static void vh_shrink_node_memcgs(void *data, struct mem_cgroup *memcg,
				  bool *skip)
{
	sc_should_memcg_bypass(memcg, skip);
}

static void vh_should_memcg_bypass(void *data, struct mem_cgroup *memcg,
				   int priority, bool *bypass)
{
	sc_should_memcg_bypass(memcg, bypass);
}

static void vh_shrink_node(void *data, pg_data_t *pgdat,
			   struct mem_cgroup *memcg)
{
	if (sc_skip_reclaim(memcg))
		return;

	sc_shrink_memcg_by_max(pgdat);
	sc_shrink_memcg_by_ratio(pgdat);
}

static int sc_register_hooks(void)
{
	int ret = 0;

	ret = register_android_vh(filemap_add_folio);
	if (ret)
		goto err;

	ret = register_android_vh(mem_cgroup_charge);
	if (ret)
		goto err_charge;

	ret = register_android_vh(shrink_node_memcgs);
	if (ret)
		goto err_memcgs;

	ret = register_android_vh(should_memcg_bypass);
	if (ret)
		goto err_bypass;

	ret = register_android_vh(shrink_node);
	if (ret)
		goto err_node;

	return 0;
err_node:
	unregister_android_vh(should_memcg_bypass);
err_bypass:
	unregister_android_vh(shrink_node_memcgs);
err_memcgs:
	unregister_android_vh(mem_cgroup_charge);
err_charge:
	unregister_android_vh(filemap_add_folio);
err:
	return ret;
}

static void sc_unregister_hooks(void)
{
	unregister_android_vh(shrink_node);
	unregister_android_vh(should_memcg_bypass);
	unregister_android_vh(shrink_node_memcgs);
	unregister_android_vh(mem_cgroup_charge);
	unregister_android_vh(filemap_add_folio);
}

static struct inode *sc_get_inode(struct file *filp)
{
	struct inode *inode = file_inode(filp);

	while (inode->i_sb->s_magic == OVERLAYFS_SUPER_MAGIC) {
		filp = filp->private_data;
		inode = file_inode(filp);
	}

	return inode;
}

static inline struct folio *sc_find_get_entry(struct xa_state *xas, pgoff_t max, xa_mark_t mark)
{
	struct folio *folio;

retry:
	if (mark == XA_PRESENT)
		folio = xas_find(xas, max);
	else
		folio = xas_find_marked(xas, max, mark);

	if (xas_retry(xas, folio))
		goto retry;
	/*
	 * A shadow entry of a recently evicted page, a swap
	 * entry from shmem/tmpfs or a DAX entry.  Return it
	 * without attempting to raise page count.
	 */
	if (!folio || xa_is_value(folio))
		return folio;

	if (!folio_try_get(folio))
		goto reset;

	if (unlikely(folio != xas_reload(xas))) {
		folio_put(folio);
		goto reset;
	}

	return folio;
reset:
	xas_reset(xas);
	goto retry;
}

static unsigned int sc_find_get_entries(struct address_space *mapping, pgoff_t start, pgoff_t end,
					struct folio_batch *fbatch, pgoff_t *indices)
{
	XA_STATE(xas, &mapping->i_pages, start);
	struct folio *folio;

	rcu_read_lock();
	while ((folio = sc_find_get_entry(&xas, end, XA_PRESENT)) != NULL) {
		indices[fbatch->nr] = xas.xa_index;
		if (!folio_batch_add(fbatch, folio))
			break;
	}
	rcu_read_unlock();

	return folio_batch_count(fbatch);
}

static int sc_file_shrink_folio(struct folio *folio, struct list_head *list)
{
	struct lruvec *lruvec = folio_lruvec(folio);
	struct mem_cgroup *memcg = folio_memcg(folio);

	pr_debug("SC: shrink folio\n");

	if (list) {
		spin_lock_irq(&lruvec->lru_lock);
		if (sc_isolate_folio(lruvec, folio) == false) {
			spin_unlock_irq(&lruvec->lru_lock);
			pr_debug("SC: sc_isolate_folio fail, skip\n");
			return 0;
		} else {
			/* folio get by sc_isolate_folio */
			list_add(&folio->lru, list);
			spin_unlock_irq(&lruvec->lru_lock);
		}
	}

	if (!folio_trylock(folio)) {
		pr_debug("SC: folio_trylock fail, try after\n");
		return -1;
	}

	if (mem_cgroup_move_account(folio, true, memcg, root_mem_cgroup)) {
		sci.memcg_move_fail_cnt++;
		pr_debug("SC: mem_cgroup_move_account fail, try after, memcg_move_fail_cnt=%lu\n", sci.memcg_move_fail_cnt);
		folio_unlock(folio);
		return -1;
	}

	/* isolate and lock and move all success, del folio from list first(must first for folio_putback_lru->folio_add_lru) */
	list_del(&folio->lru);
	/* isolate and lock and move all success, unlock folio */
	folio_unlock(folio);
	/* isolate and lock and move all success, add lru and put folio(folio_try_get if sc_isolate_folio success) last */
	folio_putback_lru(folio);
	return 0;
}

static void sc_folio_batch_remove_exceptionals(struct folio_batch *fbatch)
{
	unsigned int i, j;

	for (i = 0, j = 0; i < folio_batch_count(fbatch); i++) {
		struct folio *folio = fbatch->folios[i];

		if (!xa_is_value(folio))
			fbatch->folios[j++] = folio;
	}
	fbatch->nr = j;
}

static void sc_file_shrink(struct inode *inode, pgoff_t start, pgoff_t end)
{
	struct address_space *mapping = inode->i_mapping;
	struct folio *folio;
	struct folio_batch fbatch;
	pgoff_t index;
	pgoff_t indices[PAGEVEC_SIZE];
	int i = 0;
	LIST_HEAD(list);
	int ret = 0;
	struct mem_cgroup *memcg = NULL;
	struct memcg_ctrl *mctrl = NULL;
	unsigned long start_jiffies;

	pr_debug("SC: file shrink\n");

	if (mapping_empty(mapping)) {
		pr_debug("SC: mapping_empty\n");
		return;
	}

	folio_batch_init(&fbatch);
	index = start;
	/* folio gets */
	while (sc_find_get_entries(mapping, index, end, &fbatch, indices)) {
		for (i = 0; i < folio_batch_count(&fbatch); i++) {
			folio = fbatch.folios[i];
			index = indices[i];
			if (xa_is_value(folio))
				continue;
			memcg = folio_memcg(folio);
			mctrl = MEMCG_OEM_DATA(memcg);
			if (!mctrl) {
				pr_debug("SC: mctrl is null\n");
				continue;
			} else if (mctrl->level >= MEMCG_LEVEL_MAX || mctrl->level <= 0) {
				pr_debug("SC: level=%u\n", mctrl->level);
				continue;
			} else
				sc_file_shrink_folio(folio, &list);
		}
		sc_folio_batch_remove_exceptionals(&fbatch);
		/* folio puts */
		folio_batch_release(&fbatch);
		index++;
	}

	pr_debug("SC: try shrink list\n");

	start_jiffies = jiffies;
	while (!list_empty(&list)) {
		if (time_after(jiffies, start_jiffies + FILE_SHRINK_TIMEOUT)) {
			pr_debug("SC: file shrink timeout\n");
			break;
		}
		/* The lru_to_folio function takes the node off the tail of the LRU list */
		folio = lru_to_folio(&list);
		ret = sc_file_shrink_folio(folio, NULL);
		if (ret)
			/* delete from one list and add as another's head */
			list_move(&folio->lru, &list);
		cond_resched();
	}

	pr_debug("SC: clear list\n");

	while (!list_empty(&list)) {
		folio = lru_to_folio(&list);
		list_del(&folio->lru);
		folio_putback_lru(folio);
	}
}

/* TODO: avoid reclaim cache inode */
static int sc_file_add(unsigned int fd, unsigned int level)
{
	struct fd f;
	struct inode *inode;
	int ret = 0;

	if (!sci.enable)
		return -EPERM;

	if (level > MEMCG_LEVEL_MAX)
		return -EINVAL;

	if (level && !get_cache_memcg(level))
		return -EINVAL;

	f = fdget(fd);
	if (!f.file)
		return -EBADF;

	inode = sc_get_inode(f.file);
	if (!inode) {
		ret = -EBADF;
		goto out;
	}

	spin_lock(&inode->i_lock);
	inode->i_state &= ~I_CACHE_LEVEL_MASK;
	if (level) {
		inode->i_state |= 1UL << (I_CACHE_LEVEL_SHIFT + level - 1);
		spin_unlock(&inode->i_lock);
	} else {
		spin_unlock(&inode->i_lock);
		sc_file_shrink(inode, 0, -1);
	}

	pr_debug("SC: add file %pd(inode=%p, i_state=%lu) level %u", f.file->f_path.dentry, inode, inode->i_state, level);
out:
	fdput(f);

	return ret;
}

static ssize_t file_store(struct kobject *kobj, struct kobj_attribute *attr,
			  const char *buf, size_t len)
{
	int ret = 0;
	char *ori_opts, *opts;
	char *opt, *left, *right;
	unsigned int fd, level;

	opts = kstrdup(buf, GFP_KERNEL);
	if (!opts)
		return -ENOMEM;
	ori_opts = opts;

	while ((opt = strsep(&opts, ",")) != NULL) {
		if (!*opt)
			continue;

		right = opt;
		left = strsep(&right, "-");
		if (!left || !right) {
			ret = -EINVAL;
			goto out;
		}

		ret = kstrtouint(left, 10, &fd);
		if (ret)
			goto out;

		ret = kstrtouint(right, 10, &level);
		if (ret)
			goto out;

		ret = sc_file_add(fd, level);
		if (ret)
			goto out;
	}

out:
	kfree(ori_opts);

	return ret == 0 ? len : ret;
}

static ssize_t ratio_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	return sysfs_emit(buf, "%u\n", sci.ratio);
}

static ssize_t ratio_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t len)
{
	unsigned int val;
	int ret = 0;

	ret = kstrtouint(buf, 10, &val);
	if (ret)
		return ret;

	/* percentage */
	if (val > 100)
		return -EINVAL;

	sci.ratio = val;

	return len;
}

static ssize_t enable_show(struct kobject *kobj, struct kobj_attribute *attr,
			   char *buf)
{
	return sysfs_emit(buf, "%d\n", sci.enable);
}

static ssize_t enable_store(struct kobject *kobj, struct kobj_attribute *attr,
			    const char *buf, size_t len)
{
	int ret = 0;

	ret = kstrtobool(buf, &sci.enable);

	return ret ? ret : len;
}

#define SMART_CACHE_ATTR_RO(_name) \
	struct kobj_attribute smart_cache_##_name##_attr = __ATTR_RO(_name)

#define SMART_CACHE_ATTR_WO(_name) \
	struct kobj_attribute smart_cache_##_name##_attr = __ATTR_WO(_name)

#define SMART_CACHE_ATTR(_name) \
	struct kobj_attribute smart_cache_##_name##_attr = __ATTR_RW(_name)

static SMART_CACHE_ATTR_WO(file);
static SMART_CACHE_ATTR(ratio);
static SMART_CACHE_ATTR(enable);

static struct attribute *smart_cache_attrs[] = {
	&smart_cache_file_attr.attr,
	&smart_cache_ratio_attr.attr,
	&smart_cache_enable_attr.attr,
	NULL
};

static const struct attribute_group smart_cache_attr_group = {
	.name = "smart_cache",
	.attrs = smart_cache_attrs,
};

int smart_cache_init(void)
{
	int ret = 0;

	ret = sysfs_create_group(kernel_kobj, &smart_cache_attr_group);
	if (ret)
		pr_err("add smart_cache sysfs fail %d\n", ret);

	ret = sc_register_hooks();
	if (ret) {
		sysfs_remove_group(kernel_kobj, &smart_cache_attr_group);
		pr_err("register smart_cache hooks fail: %d", ret);
		return ret;
	}

	pr_info("smart_cache init success");

	return ret;
}

void smart_cache_exit(void)
{
	sc_unregister_hooks();
	sysfs_remove_group(kernel_kobj, &smart_cache_attr_group);
}

