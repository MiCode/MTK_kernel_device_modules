// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/vmalloc.h>
#include "loom_base.h"
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include "../kernel_core_ctrl/kernel_core_ctrl.h"
#include <linux/cpumask.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/sched/clock.h>

static int loom_task_cfg_length;
static int loom_render_num;
static HLIST_HEAD(loom_render_list);
static HLIST_HEAD(loom_task_cfg);
static DEFINE_MUTEX(render_lock);
static DEFINE_MUTEX(cfg_lock);
//static DEFINE_MUTEX(loom_cb_lock);  need or not??

void *loom_alloc(int size)
{
	void *pvBuf = NULL;

	if (size <= PAGE_SIZE)
		pvBuf = kzalloc(size, GFP_ATOMIC);
	else
		pvBuf = vzalloc(size);
	return pvBuf;
}

void *loom_calloc(int num, int size)
{
	void *pvBuf = NULL;

	pvBuf = kcalloc(num, size, GFP_KERNEL);

	return pvBuf;
}

void loom_free(void *pvBuf)
{
	kvfree(pvBuf);
}

unsigned long long loom_get_time(void)
{
	unsigned long long ret;

	preempt_disable();
	ret = cpu_clock(smp_processor_id());
	preempt_enable();

	return ret;
}

static struct cpumask loom_generate_user_cpu_mask(int mask_int)
{
	unsigned long cpumask_ulval = mask_int;
	struct cpumask cpumask_setting;
	int cpu;

	cpumask_clear(&cpumask_setting);
	for_each_possible_cpu(cpu) {
		if (cpumask_ulval & (1 << cpu))
			cpumask_set_cpu(cpu, &cpumask_setting);
	}
	return cpumask_setting;
}

long loom_sched_setaffinity(int pid, int user_mask)
{
	struct task_struct *p;
	struct cpumask cpu_mask;
	int retval;

	rcu_read_lock();

	p = find_task_by_vpid(pid);
	if (!p) {
		rcu_read_unlock();
		return -ESRCH;
	}

	/* Prevent p going away */
	get_task_struct(p);
	rcu_read_unlock();

	if (p->flags & PF_NO_SETAFFINITY) {
		retval = -EINVAL;
		goto out_put_task;
	}

	cpu_mask = loom_generate_user_cpu_mask(user_mask);
	retval = set_cpus_allowed_ptr_by_kernel(p, &cpu_mask);
out_put_task:
	put_task_struct(p);
	return retval;
}

/* use for delete all loom active list or loom task cfg*/
void loom_clear_loom_attr(struct hlist_head *head)
{
	struct loom_attr_info *attr_iter;
	struct hlist_node *tmp;

	if (!head)
		return;
	hlist_for_each_entry_safe(attr_iter, tmp, head, hlist) {
		hlist_del(&attr_iter->hlist);
		loom_free(attr_iter);
		if (head == &loom_task_cfg)
			loom_task_cfg_length--;
	}
	INIT_HLIST_HEAD(head);
}

/* use for delete loom lc active list */
static void loom_clear_loading_ctrl_list(struct list_head *head)
{
	//struct loom_loading_ctrl *iter;
	//struct loom_loading_ctrl *tmp;

	if (!head)
		return;
	/*
	 *hlist_for_each_entry_safe(iter, tmp, head, list) {
	 *	hlist_del(&iter->hlist);
	 *	loom_free(iter);
	 *}
	 */
	INIT_LIST_HEAD(head);
}

/* use for delete specific loom_attr_info */
void loom_delete_task_cfg(struct loom_attr_info *iter, struct hlist_head *head)
{
	if (!iter)
		return;
	hlist_del(&iter->hlist);
	if (head == &loom_task_cfg)
		loom_task_cfg_length--;
	loom_free(iter);
}

struct loom_attr_info *loom_add_task_cfg_pid_sorted(struct hlist_head *head, char *proc_name,
	char *thread_name, int pid)
{
	struct loom_attr_info *iter;
	struct loom_attr_info *new_info;
	struct hlist_node *last = NULL;

	new_info = loom_alloc(sizeof(struct loom_attr_info));
	if (!new_info)
		return NULL;

	new_info->pid = pid;
	if (!strscpy(new_info->proc_name, proc_name, 16)) {
		loom_free(new_info);
		return NULL;
	}
	new_info->proc_name[15] = '\0';
	if (!strscpy(new_info->thread_name, thread_name, 16)) {
		loom_free(new_info);
		return NULL;
	}
	new_info->thread_name[15] = '\0';
	hlist_for_each_entry(iter, head, hlist) {
		if (iter->pid > pid) {
			hlist_add_before(&new_info->hlist, &iter->hlist);
			return new_info;
		}
		last = &iter->hlist;
	}
	if (last)
		hlist_add_behind(&new_info->hlist, last);
	else
		hlist_add_head(&new_info->hlist, head);
	return new_info;
}

void loom_assign_task_cfg(struct loom_attr_info *info, int mode,
	int match_num, int prio, int cpu_mask, int set_exclusive,
	int loading_ub, int loading_lb, int bhr,int set_rescue, int rescue_f)
{
	if (!info)
		return;
	if (mode != LOOM_DEFAULT_VALUE)
		info->mode = mode;
	if (match_num != LOOM_DEFAULT_VALUE)
		info->matching_num = match_num;
	if (prio != LOOM_DEFAULT_VALUE)
		info->prio = prio;
	if (cpu_mask != LOOM_DEFAULT_VALUE)
		info->cpu_mask = cpu_mask;
	if (set_exclusive != LOOM_DEFAULT_VALUE)
		info->set_exclusive = set_exclusive;
	if (loading_ub != LOOM_DEFAULT_VALUE)
		info->loading_ub = loading_ub;
	if (loading_lb != LOOM_DEFAULT_VALUE)
		info->loading_lb = loading_lb;
	if (bhr != LOOM_DEFAULT_VALUE)
		info->bhr = bhr;
	if (set_rescue != LOOM_DEFAULT_VALUE)
		info->set_rescue = set_rescue;
	if (rescue_f != LOOM_DEFAULT_VALUE)
		info->rescue_f = rescue_f;
}


/* add new loom setting to loom_attr_info hlist head, if there is an existed loom setting just return it */
/*@head: loom_attr_info hlist for search and add */
/*@mode: 0==> match by name, 1==> match by pid */
/*@add: if not find, add new one to head */
/* TODO: if pid control and name control both exist, use the same loom_attr_info to store? */
struct loom_attr_info *loom_search_add_task_cfg(struct hlist_head *head, int mode,
	char *proc_name, char *thread_name, int pid, int add)
{
	struct loom_attr_info *iter = NULL;
	struct hlist_node *tmp = NULL;

	hlist_for_each_entry_safe(iter, tmp, head, hlist) { // may not use this if we need two unitymain config??
		if (mode == MATCH_PID) {
			if (iter->pid != pid)
				continue;
		}else {
			if (strlen(iter->proc_name) != strlen(proc_name) ||
				strncmp(iter->proc_name, proc_name, strlen(proc_name)))
				continue;
			if (strlen(iter->thread_name) != strlen(thread_name) ||
				strncmp(iter->thread_name, thread_name, strlen(thread_name)))
				continue;
		}
		//find same name same pid, just use this one
		break;
	}

	if (iter || !add || (head == &loom_task_cfg && loom_task_cfg_length
			>= LOOM_MAX_LOOM_CFG_NUM))
		goto out;

	iter = loom_alloc(sizeof(struct loom_attr_info));
	if (!iter)
		goto out;
	if (head == &loom_task_cfg)
		loom_task_cfg_length++;
	hlist_add_head(&iter->hlist, head);
	if (mode == MATCH_PID) {
		iter->pid = pid;
	}else {
		if (!strscpy(iter->proc_name, proc_name, 16)) {
			loom_delete_task_cfg(iter, head);
			iter = NULL;
			goto out;
		}
		iter->proc_name[15] = '\0';
		if (!strscpy(iter->thread_name, thread_name, 16)) {
			loom_delete_task_cfg(iter, head);
			iter = NULL;
			goto out;
		}
		iter->thread_name[15] = '\0';
	}
	iter->mode = LOOM_DEFAULT_VALUE;
	iter->matching_num = LOOM_DEFAULT_VALUE;
	iter->prio = LOOM_DEFAULT_VALUE;
	iter->cpu_mask = LOOM_DEFAULT_VALUE;
	iter->set_exclusive = LOOM_DEFAULT_VALUE;
	iter->loading_ub = LOOM_DEFAULT_VALUE;
	iter->loading_lb = LOOM_DEFAULT_VALUE;
	iter->bhr = LOOM_DEFAULT_VALUE;
	iter->set_rescue = LOOM_DEFAULT_VALUE;
	iter->rescue_f = LOOM_DEFAULT_VALUE;
out:
	return iter;
}

void loom_delete_render_info(struct loom_render_info *iter)
{
	// fpsgo_com2xgf_delete_render_info referenced
	hlist_del(&iter->render_hlist);
	loom_clear_loom_attr(&iter->active_list);
	loom_clear_loading_ctrl_list(&iter->lc_active_list);
	loom_free(iter);
	loom_render_num--;
}

struct loom_render_info *loom_search_add_render_info(int tgid, int add)
{
	//xgf_get_render_if
	//search or create loom_render in hlist
	struct loom_render_info *iter = NULL;
	struct hlist_node *h = NULL;

	hlist_for_each_entry_safe(iter, h, &loom_render_list, render_hlist) {
		if (iter->tgid == tgid)
			break;
	}
	if (iter || !add || loom_render_num >= LOOM_MAX_RENDER_NUM)
		goto out;
	iter = loom_alloc(sizeof(struct loom_render_info));
	if (!iter)
		goto out;
	iter->tgid = tgid;
	iter->pid = 0; // do we need??
	iter->buffer_id = 0; // do we need??
	iter->target_fps = 0;
	iter->last_update_ts = 0;
	INIT_HLIST_HEAD(&iter->active_list);
	INIT_LIST_HEAD(&iter->lc_active_list);

	loom_render_num++;
	hlist_add_head(&iter->render_hlist, &loom_render_list);
out:
	return iter;
}

struct hlist_head *loom_get_render_list(void)
{
	return &loom_render_list;
}

struct hlist_head *loom_get_cfg_list(void)
{
	return &loom_task_cfg;
}

int loom_get_render_num(void)
{
	return loom_render_num;
}

int loom_get_cfg_length(void)
{
	return loom_task_cfg_length;
}

void loom_render_lock(void)
{
	mutex_lock(&render_lock);
}

void loom_render_unlock(void)
{
	mutex_unlock(&render_lock);
}

void loom_cfg_lock(void)
{
	mutex_lock(&cfg_lock);
}

void loom_cfg_unlock(void)
{
	mutex_unlock(&cfg_lock);
}
