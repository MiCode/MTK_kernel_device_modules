// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright(c) 2024 X-Ring technologies Inc.
 */

#define pr_fmt(fmt) "mctrl: " fmt

#include <linux/spinlock.h>
#include <linux/cgroup.h>

#include <trace/hooks/mm.h>

#include "internal.h"
#include "mctrl.h"
#include "zgroup.h"

static LIST_HEAD(score_head);
static DEFINE_RWLOCK(mctrl_lock);

#ifdef CONFIG_XRING_SMART_CACHE
static LIST_HEAD(level_head);
static struct mem_cgroup *cache_memcgs[MEMCG_LEVEL_MAX];
#endif

static struct mctrl_group_param mctrl_params[MCTRL_GROUP_MAX];

/* must call get_next_memcg_break when exiting with non-empty mem_cgroup */
struct mem_cgroup *get_next_memcg(struct mem_cgroup *prev, bool level)
{
	struct memcg_ctrl *mctrl;
	struct mem_cgroup *cur = NULL;
	struct mem_cgroup *begin = prev;
	struct list_head *pos;
	struct list_head *head = &score_head;
	unsigned long flags;

#ifdef CONFIG_XRING_SMART_CACHE
	if (level)
		head = &level_head;
#endif

	read_lock_irqsave(&mctrl_lock, flags);
next:
	if (unlikely(!prev))
		pos = head;
	else
		pos = &MEMCG_OEM_DATA(prev)->list;

	if (list_empty(pos))
		goto unlock;

	if (pos->next == head)
		goto unlock;

	mctrl = list_entry(pos->next, struct memcg_ctrl, list);
	cur = mctrl->memcg;

	if (!mem_cgroup_tryget(cur)) {
		prev = cur;
		cur = NULL;
		goto next;
	}

unlock:
	read_unlock_irqrestore(&mctrl_lock, flags);
	mem_cgroup_put(begin);

	return cur;
}

void get_next_memcg_break(struct mem_cgroup *memcg)
{
	if (memcg)
		css_put(&memcg->css);
}

static void mctrl_score_update(struct memcg_ctrl *mctrl)
{
	struct memcg_ctrl *cur_mctrl;
	struct list_head *cur;
	unsigned long flags;

	write_lock_irqsave(&mctrl_lock, flags);

	list_for_each(cur, &score_head) {
		cur_mctrl = list_entry(cur, struct memcg_ctrl, list);

		if (mctrl->score > cur_mctrl->score)
			break;
	}
	list_move_tail(&mctrl->list, cur);
#ifdef CONFIG_XRING_SMART_CACHE
	if (mctrl->level != 0) {
		cache_memcgs[mctrl->level - 1] = 0;
		mctrl->level = 0;
	}
#endif

	write_unlock_irqrestore(&mctrl_lock, flags);
}

static int memcg_ctrl_alloc(struct mem_cgroup *memcg, bool atomic)
{
	struct memcg_ctrl *mctrl;
	atomic64_t *ori_mctrl;

	mctrl = kzalloc(sizeof(struct memcg_ctrl), atomic ? GFP_ATOMIC :
							    GFP_KERNEL);
	if (!mctrl)
		return -ENOMEM;

	INIT_LIST_HEAD(&mctrl->list);
	mctrl->memcg = memcg;
	/* percentage */
	mctrl->refault_ratio = 50;

	ori_mctrl = (atomic64_t *)&memcg->android_oem_data1[1];
	if (atomic64_cmpxchg(ori_mctrl, 0, (u64)mctrl) != 0)
		kfree(mctrl);

	return 0;
}

static void memcg_ctrl_free(struct mem_cgroup *memcg)
{
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);

	if (!mctrl)
		return;

	memcg->android_oem_data1[1] = 0;
	kfree(mctrl);
}

static void vh_mem_cgroup_alloc(void *data, struct mem_cgroup *memcg)
{
	int ret;

	ret = memcg_ctrl_alloc(memcg, true);
	if (ret)
		pr_notice("alloc memcg_ctrl fail(%d) in atomic, try later", ret);
}

static void vh_mem_cgroup_free(void *data, struct mem_cgroup *memcg)
{
	memcg_ctrl_free(memcg);
}

static void vh_mem_cgroup_css_online(void *data,
				     struct cgroup_subsys_state *css,
				     struct mem_cgroup *memcg)
{
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);

	if (!mctrl)
		return;

	mctrl_score_update(mctrl);
}

static void mctrl_list_del_init(struct memcg_ctrl *mctrl)
{
	unsigned long flags;

	write_lock_irqsave(&mctrl_lock, flags);

#ifdef CONFIG_XRING_SMART_CACHE
	if (mctrl->level != 0) {
		cache_memcgs[mctrl->level - 1] = 0;
		mctrl->level = 0;
	}
#endif
	list_del_init(&mctrl->list);

	write_unlock_irqrestore(&mctrl_lock, flags);
}

static void vh_mem_cgroup_css_offline(void *data,
				      struct cgroup_subsys_state *css,
				      struct mem_cgroup *memcg)
{
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);

	if (!mctrl)
		return;

	mctrl_list_del_init(mctrl);
}

static void mctrl_clean_memcg(struct memcg_ctrl *mctrl)
{
	struct mem_cgroup *memcg = mctrl->memcg;

	if (!mem_cgroup_tryget(memcg))
		return;

	mctrl_list_del_init(mctrl);
	memcg_ctrl_free(memcg);

	mem_cgroup_put(memcg);
}

static void mctrl_clean_memcgs(void)
{
	struct memcg_ctrl *mctrl, *n;

	list_for_each_entry_safe(mctrl, n, &score_head, list)
		mctrl_clean_memcg(mctrl);

#ifdef CONFIG_XRING_SMART_CACHE
	list_for_each_entry_safe(mctrl, n, &level_head, list)
		mctrl_clean_memcg(mctrl);
#endif
}

static int mctrl_register_hooks(void)
{
	int ret;

	ret = register_android_vh(mem_cgroup_alloc);
	if (ret)
		goto err;

	ret = register_android_vh(mem_cgroup_free);
	if (ret)
		goto err_free;

	ret = register_android_vh(mem_cgroup_css_online);
	if (ret)
		goto err_online;

	ret = register_android_vh(mem_cgroup_css_offline);
	if (ret)
		goto err_offline;

	return 0;

err_offline:
	unregister_android_vh(mem_cgroup_css_online);
err_online:
	unregister_android_vh(mem_cgroup_free);
err_free:
	unregister_android_vh(mem_cgroup_alloc);
err:
	return ret;
}

static void mctrl_unregister_hooks(void)
{
	unregister_android_vh(mem_cgroup_css_offline);
	unregister_android_vh(mem_cgroup_css_online);
	unregister_android_vh(mem_cgroup_free);
	unregister_android_vh(mem_cgroup_alloc);
}

static int mctrl_score_write(struct cgroup_subsys_state *css,
			     struct cftype *cft, u64 val)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);
	int ret;

	if (val > MEMCG_SCORE_MAX)
		return -EINVAL;

	if (!mctrl) {
		ret = memcg_ctrl_alloc(memcg, false);
		if (ret)
			return ret;
		mctrl = MEMCG_OEM_DATA(memcg);
	}

	mctrl->score = val;
	mctrl_score_update(mctrl);

	return 0;
}

static u64 mctrl_score_read(struct cgroup_subsys_state *css,
			    struct cftype *cft)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);

	if (!mctrl)
		return -EPERM;

	return mctrl->score;
}

#ifdef CONFIG_XRING_SMART_CACHE
struct mem_cgroup *get_cache_memcg(unsigned int level)
{
	if (level > MEMCG_LEVEL_MAX)
		return NULL;

	return cache_memcgs[level - 1];
}

static int mctrl_level_update(struct mem_cgroup *memcg, unsigned int level)
{
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);
	struct memcg_ctrl *cur_mctrl;
	struct list_head *cur;
	unsigned long flags;
	int ret = 0;

	write_lock_irqsave(&mctrl_lock, flags);

	if (cache_memcgs[level - 1]) {
		ret = -EBUSY;
		goto unlock;
	}

	list_for_each(cur, &level_head) {
		cur_mctrl = list_entry(cur, struct memcg_ctrl, list);

		if (level > cur_mctrl->level)
			break;
	}
	list_move_tail(&mctrl->list, cur);
	cache_memcgs[level - 1] = memcg;
	mctrl->level = level;
unlock:
	write_unlock_irqrestore(&mctrl_lock, flags);

	return ret;
}

static int mctrl_level_write(struct cgroup_subsys_state *css,
			     struct cftype *cft, u64 val)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);
	int ret;

	if (val > MEMCG_LEVEL_MAX)
		return -EINVAL;

	if (!mctrl) {
		ret = memcg_ctrl_alloc(memcg, false);
		if (ret)
			return ret;
		mctrl = MEMCG_OEM_DATA(memcg);
	}

	/* Non-cache memcg */
	if (val == 0)
		mctrl_score_update(mctrl);
	else
		mctrl_level_update(memcg, val);

	return 0;
}

static u64 mctrl_level_read(struct cgroup_subsys_state *css,
			    struct cftype *cft)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);

	if (!mctrl)
		return -EPERM;

	return mctrl->level;
}

static int mctrl_max_write(struct cgroup_subsys_state *css,
			   struct cftype *cft, u64 val)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);

	if (!mctrl)
		return -EPERM;

	if (val & ~PAGE_MASK)
		return -EINVAL;

	/* bytes to nr */
	mctrl->max = val >> PAGE_SHIFT;

	return 0;
}

static u64 mctrl_max_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);

	if (!mctrl)
		return -EPERM;

	return mctrl->max << PAGE_SHIFT;
}
#endif

static ssize_t mctrl_name_write(struct kernfs_open_file *of, char *buf,
				size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);
	int len;

	if (!mctrl)
		return -EPERM;

	buf = strstrip(buf);
	len = strlen(buf) + 1;
	if (len > MEMCG_NAME_LEN_MAX)
		len = MEMCG_NAME_LEN_MAX;

	snprintf(mctrl->name, len, "%s", buf);

	return nbytes;
}

static int mctrl_name_show(struct seq_file *m, void *v)
{
	struct mem_cgroup *memcg = mem_cgroup_from_seq(m);
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);

	if (!mctrl)
		return -EPERM;

	seq_printf(m, "%s\n", mctrl->name);

	return 0;
}

static int mctrl_comp_ratio_write(struct cgroup_subsys_state *css,
				  struct cftype *cft, u64 val)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);

	/* percentage */
	if (val > 100)
		return -EINVAL;

	if (!mctrl)
		return -EPERM;

	mctrl->comp_ratio = val;

	return 0;
}

static u64 mctrl_comp_ratio_read(struct cgroup_subsys_state *css,
				 struct cftype *cft)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);

	if (!mctrl)
		return -EPERM;

	return mctrl->comp_ratio;
}

static int mctrl_wb_ratio_write(struct cgroup_subsys_state *css,
				struct cftype *cft, u64 val)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);

	/* percentage */
	if (val > 100)
		return -EINVAL;

	if (!mctrl)
		return -EPERM;

	mctrl->wb_ratio = val;

	return 0;
}

static u64 mctrl_wb_ratio_read(struct cgroup_subsys_state *css,
			       struct cftype *cft)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);

	if (!mctrl)
		return -EPERM;

	return mctrl->wb_ratio;
}

static int mctrl_refault_ratio_write(struct cgroup_subsys_state *css,
				     struct cftype *cft, u64 val)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);

	/* percentage */
	if (val > 100)
		return -EINVAL;

	if (!mctrl)
		return -EPERM;

	mctrl->refault_ratio = val;

	return 0;
}

static u64 mctrl_refault_ratio_read(struct cgroup_subsys_state *css,
				    struct cftype *cft)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);
	struct memcg_ctrl *mctrl = MEMCG_OEM_DATA(memcg);

	if (!mctrl)
		return -EPERM;

	return mctrl->refault_ratio;
}

static void mctrl_group_param_update(unsigned int level)
{
	struct mem_cgroup *memcg = NULL;
	struct memcg_ctrl *mctrl;

	while ((memcg = get_next_memcg(memcg, false))) {
		mctrl = MEMCG_OEM_DATA(memcg);

		if (mctrl->score > mctrl_params[level].max_score)
			continue;

		if (mctrl->score < mctrl_params[level].min_score) {
			get_next_memcg_break(memcg);
			break;
		}

		mctrl->comp_ratio = mctrl_params[level].comp_ratio;
		mctrl->wb_ratio = mctrl_params[level].wb_ratio;
		mctrl->refault_ratio = mctrl_params[level].refault_ratio;
	}
}

static ssize_t mctrl_group_param_write(struct kernfs_open_file *of, char *buf,
				       size_t nbytes, loff_t off)
{
	unsigned int level, min_score, max_score;
	unsigned int comp_ratio, wb_ratio, refault_ratio;

	buf = strstrip(buf);

	if (sscanf(buf, "%u %u %u %u %u %u", &level, &min_score, &max_score,
					     &comp_ratio, &wb_ratio,
					     &refault_ratio) != 6)
		return -EINVAL;

	if (level >= MCTRL_GROUP_MAX)
		return -EINVAL;

	if (min_score > MEMCG_SCORE_MAX || max_score > MEMCG_SCORE_MAX ||
	    min_score > max_score)
		return -EINVAL;

	mctrl_params[level].min_score = min_score;
	mctrl_params[level].max_score = max_score;
	mctrl_params[level].comp_ratio = comp_ratio;
	mctrl_params[level].wb_ratio = wb_ratio;
	mctrl_params[level].refault_ratio = refault_ratio;

	mctrl_group_param_update(level);

	return nbytes;
}

static int mctrl_group_param_show(struct seq_file *m, void *v)
{
	int i;

	seq_puts(m, "[level] min_score max_score comp_ratio wb_ratio refault_ratio\n");

	for (i = 0; i < MCTRL_GROUP_MAX; ++i)
		seq_printf(m, "[%d] %u %u %u %u %u\n", i,
			   mctrl_params[i].min_score,
			   mctrl_params[i].max_score,
			   mctrl_params[i].comp_ratio,
			   mctrl_params[i].wb_ratio,
			   mctrl_params[i].refault_ratio);

	return 0;
}

static int mctrl_stat_show(struct seq_file *m, void *v)
{
	unsigned long nr_comp, sz_comp, fault_comp, drop_comp;
#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
	unsigned long nr_ext, nr_wb, sz_wb, fault_wb, drop_wb;
#endif
	struct mem_cgroup *memcg = mem_cgroup_from_seq(m);

	nr_comp = zgroup_get_memcg_stat(memcg, ZGROUP_OBJ_PAGES);
	sz_comp = zgroup_get_memcg_stat(memcg, ZGROUP_OBJ_SIZE);
	fault_comp = zgroup_get_memcg_stat(memcg, ZGROUP_OBJ_FAULT);
	drop_comp = zgroup_get_memcg_stat(memcg, ZGROUP_OBJ_DROP);

	seq_printf(m, "comp_pages: %lu\n", nr_comp);
	seq_printf(m, "comp_size:  %lu\n", sz_comp);
	seq_printf(m, "comp_fault: %lu\n", fault_comp);
	seq_printf(m, "comp_drop:  %lu\n", drop_comp);

#ifdef CONFIG_XRING_ZRAM_MEMCG_WRITEBACK
	nr_ext = zgroup_get_memcg_stat(memcg, ZGROUP_WB_EXTS);
	nr_wb = zgroup_get_memcg_stat(memcg, ZGROUP_WB_PAGES);
	sz_wb = zgroup_get_memcg_stat(memcg, ZGROUP_WB_SIZE);
	fault_wb = zgroup_get_memcg_stat(memcg, ZGROUP_WB_FAULT);
	drop_wb = zgroup_get_memcg_stat(memcg, ZGROUP_WB_DROP);

	seq_printf(m, "wb_exts:    %lu\n", nr_ext);
	seq_printf(m, "wb_pages:   %lu\n", nr_wb);
	seq_printf(m, "wb_size:    %lu\n", sz_wb);
	seq_printf(m, "wb_fault:   %lu\n", fault_wb);
	seq_printf(m, "wb_drop:    %lu\n", drop_wb);
#endif

	return 0;
}

static struct cftype mctrl_files[] = {
	{
		.name = "mctrl.score",
		.write_u64 = mctrl_score_write,
		.read_u64 = mctrl_score_read,
	},
#ifdef CONFIG_XRING_SMART_CACHE
	{
		.name = "mctrl.level",
		.write_u64 = mctrl_level_write,
		.read_u64 = mctrl_level_read,
	},
	{
		.name = "mctrl.max",
		.write_u64 = mctrl_max_write,
		.read_u64 = mctrl_max_read,
	},
#endif
	{
		.name = "mctrl.name",
		.write = mctrl_name_write,
		.seq_show = mctrl_name_show,
	},
	{
		.name = "mctrl.comp_ratio",
		.write_u64 = mctrl_comp_ratio_write,
		.read_u64 = mctrl_comp_ratio_read,
	},
	{
		.name = "mctrl.wb_ratio",
		.write_u64 = mctrl_wb_ratio_write,
		.read_u64 = mctrl_wb_ratio_read,
	},
	{
		.name = "mctrl.refault_ratio",
		.write_u64 = mctrl_refault_ratio_write,
		.read_u64 = mctrl_refault_ratio_read,
	},
	{
		.name = "mctrl.group_param",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.write = mctrl_group_param_write,
		.seq_show = mctrl_group_param_show,
	},
	{
		.name = "mctrl.stat",
		.seq_show = mctrl_stat_show,
	},
	{ }, /* terminate */
};

int mctrl_init(void)
{
	int ret;

	ret = cgroup_add_legacy_cftypes(&memory_cgrp_subsys, mctrl_files);
	if (ret) {
		pr_err("add mctrl_files fail %d\n", ret);
		return ret;
	}

	ret = mctrl_register_hooks();
	if (ret) {
		cgroup_rm_cftypes(mctrl_files);
		pr_err("register memcg hooks fail: %d", ret);
		return ret;
	}

	pr_info("mctrl init success");

	return ret;
}

void mctrl_exit(void)
{
	/*
	 * HACK: mctrl can't be freed released after unregister_hooks,
	 * thus causing a memory leak.
	 */
	mctrl_clean_memcgs();
	mctrl_unregister_hooks();
	cgroup_rm_cftypes(mctrl_files);
}
