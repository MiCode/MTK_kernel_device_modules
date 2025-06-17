// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include <linux/module.h>
#include "loom.h"
#include "loom_base.h"
#include "game_sysfs.h"
#include "game.h"
#include "fpsgo_frame_info.h"
#include "loom_loading_ctrl.h"

#define DEFAULT_LOOM_UPDATE_LIST_PERIOD NSEC_PER_SEC

static int fi_cb_is_registerd;
static int loom_select_is_set;
static int loom_flt_is_set;
static int update_active_list_period;
static struct kobject *loom_kobj;

/* print struct loom_attr_info related hlist */
#define MAX_PID_DIGIT 7
#define MAIN_LOG_SIZE (256)
#define MAX_HLIST_LENGTH 25
static void print_hlist(const char *tag, int tgid, struct hlist_head *head)
{
	char *hlist_str = NULL;
	char temp[MAX_PID_DIGIT] = {"\0"};
	struct loom_attr_info *iter;
	int cnt = 0, ret = 0;

	hlist_str = loom_calloc(MAX_HLIST_LENGTH, MAX_PID_DIGIT * sizeof(char));
	if (!hlist_str)
		return;

	hlist_for_each_entry(iter, head, hlist) {
		if (cnt >= MAX_HLIST_LENGTH) {
			loom_main_trace("[loom][%s] list too long, exceeded max length.", __func__);
			break;
		}
		if (strlen(hlist_str) == 0)
			ret = snprintf(temp, sizeof(temp), "%d", iter->pid);
		else
			ret = snprintf(temp, sizeof(temp), ",%d", iter->pid);
		if (ret > 0 && (strlen(hlist_str) + strlen(temp) < MAIN_LOG_SIZE))
			strncat(hlist_str, temp, strlen(temp));
		cnt++;
	}
	loom_main_trace("[loom][%s][%d] hlist size=%d, hlist:%s", tag, tgid, cnt, hlist_str);
}

int loom_task_control(struct loom_attr_info *iter)
{
	int ret = 0;

	if (!iter)
		return -EINVAL;

	if (iter->prio > 0) {
		int throttle_time = iter->prio >= 2 ? 1000 : 12;

		set_task_priority_based_vip_and_throttle(iter->pid,
			iter->prio, throttle_time);
		iter->vip_set = 1;
	} else if (iter->vip_set && iter->prio <= 0) {
		unset_task_priority_based_vip(iter->pid);
		iter->vip_set = 0;
	}

	if (iter->cpu_mask != LOOM_DEFAULT_VALUE) {
		ret = loom_sched_setaffinity(iter->pid, iter->cpu_mask);
		if (ret >= 0)
			iter->cmask_set = 1;
	} else if (iter->cmask_set && iter->cpu_mask == LOOM_DEFAULT_VALUE) {
		ret = loom_sched_setaffinity(iter->pid, 255);
		if (ret >= 0)
			iter->cmask_set = 0;
	}

	return ret;
}

void loom_reset_task_setting(struct loom_attr_info *info)
{
	int ret = 0;

	if (!info || info->pid <= 0)
		return;

	if (info->vip_set) {
		unset_task_priority_based_vip(info->pid);
		info->vip_set = 0;
	}
	if (info->cmask_set) {
		ret = loom_sched_setaffinity(info->pid, 255);
		if (ret >= 0)
			info->cmask_set = 0;
	}
}

static void list_a_except_b(struct hlist_head *list_a,
	struct hlist_head *list_b, struct hlist_head *list_remove)
{
	struct hlist_node *ptr_a, *ptr_b;
	struct loom_attr_info *info_a, *info_b;

	if (!list_a || !list_b || !list_remove)
		return;

	ptr_a = list_a->first;
	ptr_b = list_b->first;

	while (ptr_a) {
		info_a = hlist_entry(ptr_a, struct loom_attr_info, hlist);

		while (ptr_b) {
			info_b = hlist_entry(ptr_b, struct loom_attr_info, hlist);
			if (info_b->pid >= info_a->pid)
				break;
			ptr_b = ptr_b->next;
		}

		// new active list has same element with old one, copy the setting record
		if (ptr_b && info_b->pid == info_a->pid) {
			info_b->vip_set = info_a->vip_set;
			info_b->cmask_set = info_a->cmask_set;
		}

		if (!ptr_b || info_b->pid != info_a->pid) {
			struct loom_attr_info *remove_iter = NULL;

			remove_iter = loom_search_add_task_cfg(list_remove, MATCH_PID,
				info_a->proc_name,info_a->thread_name, info_a->pid, 1);

			loom_assign_task_cfg(remove_iter, info_a->mode, info_a->matching_num,
				info_a->prio, info_a->cpu_mask,info_a->set_exclusive, info_a->loading_ub,
				info_a->loading_lb, info_a->bhr, info_a->limit_min_freq, info_a->limit_max_freq,
				info_a->set_rescue, info_a->rescue_f_opp, info_a->rescue_c_freq, info_a->rescue_time);
			// copy setting record to the remove list
			if (remove_iter) {
				remove_iter->vip_set = info_a->vip_set;
				remove_iter->cmask_set = info_a->cmask_set;
			}
		}
		ptr_a = ptr_a->next;
	}
}

static void loom_find_new_active_list(struct hlist_head *head, int tgid)
{
	struct task_struct *gtsk, *sib, *task_samename;
	struct loom_attr_info *iter, *find_iter;
	int tlen = 0;

	rcu_read_lock();
	gtsk = find_task_by_vpid(tgid);
	if (!gtsk)
		goto done;

	loom_cfg_lock();
	get_task_struct(gtsk);
	for_each_thread(gtsk, sib) {
		get_task_struct(sib);
		hlist_for_each_entry(iter, loom_get_cfg_list(), hlist) {
			// TODO : if we need pass_proc_name_check
			/* PID Matching Mode */
			if (iter->mode == MATCH_PID) {
				if (iter->pid != sib->pid)
					continue;
				find_iter = loom_search_add_task_cfg(head, iter->mode,
					gtsk->comm, sib->comm, sib->pid, 0);
				if (!find_iter)
					find_iter = loom_add_task_cfg_pid_sorted(head, gtsk->comm,
						sib->comm, sib->pid);
			} else {    /* Name Matcing Mode */
				if (iter->tgid != LOOM_DEFAULT_VALUE && iter->tgid != sib->tgid)
					continue;
				else if (iter->tgid == LOOM_DEFAULT_VALUE &&
						strncmp(gtsk->comm, iter->proc_name, LOOM_MAX_NAME_LENGTH))
					continue;

				tlen = strlen(iter->thread_name);
				if (strncmp(sib->comm, iter->thread_name, tlen))
					continue;

				if (iter->mode == MATCH_NAME_EXACT && tlen != strlen(sib->comm))
					continue;

				find_iter = loom_search_add_task_cfg(head, iter->mode,
					iter->proc_name, iter->thread_name, sib->pid, 0);

				if (!find_iter || (iter->matching_num != 1 && find_iter->pid != sib->pid)) {
					find_iter = loom_add_task_cfg_pid_sorted(head, iter->proc_name,
						iter->thread_name, sib->pid);
				} else if (iter->matching_num == 1 && find_iter->pid != sib->pid) {
					task_samename = find_task_by_vpid(find_iter->pid);
					if (!task_samename)
						continue;

					get_task_struct(task_samename);
					if (task_samename->se.sum_exec_runtime > sib->se.sum_exec_runtime) {
						put_task_struct(task_samename);
						continue;
					}
					find_iter->pid = sib->pid;
					put_task_struct(task_samename);
				}
			}
			// transfer cfg value from loom_task_cfg
			loom_assign_task_cfg(find_iter, iter->mode, iter->matching_num, iter->prio,
				iter->cpu_mask,iter->set_exclusive, iter->loading_ub, iter->loading_lb,
				iter->bhr, iter->limit_min_freq, iter->limit_max_freq,
				iter->set_rescue, iter->rescue_f_opp, iter->rescue_c_freq, iter->rescue_time);
		}
		put_task_struct(sib);
	}
	put_task_struct(gtsk);
	loom_cfg_unlock();

done:
	rcu_read_unlock();
}

static int loom_update_active_list(struct loom_render_info *info)
{
	unsigned long long ts = loom_get_time();
	long long tdiff;
	int ret = 0;
	struct hlist_head new_active_list = HLIST_HEAD_INIT;
	struct hlist_head remove_list = HLIST_HEAD_INIT;
	struct loom_attr_info *iter;
	struct loom_loading_ctrl *lc_iter;

	if (!info)
		return -EINVAL;

	tdiff = (long long)ts - (long long)info->last_update_ts;
	if (tdiff < 0LL || tdiff < update_active_list_period)
		return -EINVAL;


	print_hlist("original_hlist", info->tgid, &info->active_list);
	// find new active list
	loom_find_new_active_list(&new_active_list, info->tgid);

	print_hlist("new_active_list", info->tgid, &new_active_list);
	// find removed list
	list_a_except_b(&info->active_list, &new_active_list, &remove_list);

	print_hlist("remove_list", info->tgid, &remove_list);
	// for all removed tasks, reset all their loom setting and remove list
	hlist_for_each_entry(iter, &remove_list, hlist) {
		loom_reset_task_setting(iter);
		lc_iter = loom_search_and_add_loading_ctrl_info(&info->lc_active_list, info->pid,
			info->tgid, 0);
		if(lc_iter)
			loom_delete_loading_ctrl_info(lc_iter);
	}
	loom_clear_loom_attr(&remove_list);

	// apply new active list to info->active_list and remove the old active lsit
	remove_list.first = info->active_list.first;
	info->active_list.first = new_active_list.first;
	loom_clear_loom_attr(&remove_list);

	info->last_update_ts = ts;
	return ret;
}

void loom_reset_operation(struct loom_render_info *info)
{
	struct loom_attr_info *iter = NULL;
	struct loom_loading_ctrl *lc_iter = NULL, *tmp = NULL;

	if (!info)
		return;

	//loom_task setting reset
	hlist_for_each_entry(iter, &info->active_list, hlist) {
		loom_reset_task_setting(iter);
	}
	// loading ctrl reset
	list_for_each_entry_safe(lc_iter, tmp, &info->lc_active_list, hlist) {
		loom_delete_loading_ctrl_info(lc_iter);
	}
}

static void cpumask_to_cpu_cluster(int cpu_mask, int *cpuid, int *cpu_cluster)
{
	int cpu = 0;

	*cpuid = -1;
	*cpu_cluster = -1;

	if (cpu_mask & 128) {  // big cluster
		*cpuid = 7;
		*cpu_cluster = 2;
	} else if (cpu_mask & 112) {  // middle cluster
		*cpu_cluster = 1;
		for(cpu = 4; cpu < 7; cpu++) {
			if (cpu_mask & (1 << cpu)) {
				*cpuid = cpu;
				break;
			}
		}
	} else if (cpu_mask & 15) {  // little cluster
		*cpu_cluster = 0;
		for(cpu = 0; cpu < 4; cpu++) {
			if (cpu_mask & (1 << cpu)) {
				*cpuid = cpu;
				break;
			}
		}
	}
	game_main_trace("[%s] cpumask=%d, cpuid=%d, cluster=%d", cpu_mask, *cpuid, *cpu_cluster);
}

static void loom_set_operation(struct loom_render_info *info)
{
	struct loom_attr_info *iter;
	struct loom_loading_ctrl *lc_iter;
	int cpu = -1, cluster = -1;

	loom_update_active_list(info);

	print_hlist("active_list", info->tgid, &info->active_list);
	hlist_for_each_entry(iter, &info->active_list, hlist) {
		loom_task_control(iter);
		if ((iter->set_exclusive > 0) && (iter->loading_ub > 0 || iter->loading_lb > 0)) {
			cpumask_to_cpu_cluster(iter->cpu_mask, &cpu, &cluster);
			lc_iter = loom_search_and_add_loading_ctrl_info(&info->lc_active_list, iter->pid,
				info->tgid, 1);
			if(lc_iter) {
				lc_iter->loading_thr_up_bound = iter->loading_ub;
				lc_iter->loading_thr_low_bound = iter->loading_lb;
				lc_iter->cpu = cpu;
				lc_iter->cluster = cluster;
				lc_iter->bhr = iter->bhr;
				lc_iter->limit_min_freq = iter->limit_min_freq;
				lc_iter->limit_max_freq = iter->limit_max_freq;
				lc_iter->set_rescue = iter->set_rescue;
				lc_iter->rescue_f_opp = iter->rescue_f_opp;
				lc_iter->rescue_c_freq = iter->rescue_c_freq;
				lc_iter->rescue_time = iter->rescue_time;
			}
		} else {
			lc_iter = loom_search_and_add_loading_ctrl_info(&info->lc_active_list, iter->pid,
				info->tgid, 0);
			if(lc_iter)
				loom_delete_loading_ctrl_info(lc_iter);
		}
	}

	// do we need to iterate lc_active_list ??
	list_for_each_entry(lc_iter, &info->lc_active_list, hlist) {
		loom_loading_ctrl_operation(lc_iter, info->queue_end_ts, lc_iter->cluster, lc_iter->cpu);
	}
}

void fpsgo_loom_frame_info_cb(unsigned long cmd, struct render_frame_info *iter)
{
	struct loom_render_info *info = NULL;
	int pid;

	loom_render_lock();
	pid = iter->tgid;


	info = loom_search_add_render_info(pid, 0);
	if (!info)
		goto out;

	info->queue_end_ts = game_get_time();
	loom_set_operation(info);
out:
	loom_render_unlock();
}

static int loom_register_frame_info_cb(int set, fpsgo_frame_info_callback cb)
{
	int ret = 0;
	unsigned long cb_mask = 1 << GET_FPSGO_QUEUE_END;

	if (set && !fi_cb_is_registerd) {
		ret = register_fpsgo_frame_info_callback(cb_mask, cb);
		if (ret >= 0) {
			fi_cb_is_registerd = 1;
			loom_main_trace("[loom][%s] fpsgo frame info callback, register=%d", __func__, 1);
		}
	} else if (!set && fi_cb_is_registerd) {
		ret = unregister_fpsgo_frame_info_callback(cb);
		if (ret >= 0) {
			fi_cb_is_registerd = 0;
			loom_main_trace("[loom][%s] fpsgo frame info callback, register=%d", __func__, 0);
		}
	}
	return ret;
}

static void loom_select_cfg_apply(int set)
{
	int ret = 0;

	if (set && !loom_select_is_set) {
		ret = vip_loom_select_cfg_apply(1, 1);
		if (!ret) {
			loom_select_is_set = 1;
			loom_main_trace("[loom][%s] loom select cfg, set=%d", __func__, 1);
		}
	} else if (!set && loom_select_is_set) {
		ret = vip_loom_select_cfg_apply(0, 1);
		if (!ret) {
			loom_select_is_set = 0;
			loom_main_trace("[loom][%s] loom select cfg, set=%d", __func__, 0);
		}
	}
}

static void loom_flt_cfg_apply(int set)
{
#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
	int ret = 0;

	if (set && !loom_flt_is_set) {
		ret = vip_loom_flt_cfg_apply(1, 1);
		if (!ret) {
			loom_flt_is_set = 1;
			loom_main_trace("[loom][%s] loom flt cfg, set=%d", __func__, 1);
		}
	} else if (!set && loom_flt_is_set) {
		ret = vip_loom_flt_cfg_apply(0, 1);
		if (!ret) {
			loom_flt_is_set = 0;
			loom_main_trace("[loom][%s] loom flt cfg, set=%d", __func__, 0);
		}
	}
#else
	loom_flt_is_set = 0;
#endif  // IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
}

int loom_activate(int pid)
{
	struct loom_render_info *iter = NULL;
	int ret = 0;

	loom_render_lock();
	iter = loom_search_add_render_info(pid, 1);
	if (!iter) {
		loom_render_unlock();
		return -ENOMEM;
	}
	//switch_fpsgo_control(0, pid, 0, 0);
	fbt_set_magt_workaround_passive_mode(1);
	ret = loom_register_frame_info_cb(1, &fpsgo_loom_frame_info_cb);
	loom_select_cfg_apply(1);
	loom_flt_cfg_apply(1);
	loom_render_unlock();
	return ret;
}

int loom_deactivate(int pid)
{
	struct loom_render_info *iter = NULL;
	int ret = 0;

	loom_render_lock();
	iter= loom_search_add_render_info(pid, 0);
	if (!iter) {
		loom_render_unlock();
		return -EINVAL;
	}
	loom_reset_operation(iter);
	loom_delete_render_info(iter);
	//switch_fpsgo_control(0, pid, 1, 0);
	fbt_set_magt_workaround_passive_mode(0);
	if (!loom_get_render_num()) {
		ret = loom_register_frame_info_cb(0, &fpsgo_loom_frame_info_cb);
		loom_select_cfg_apply(0);
		loom_flt_cfg_apply(0);
	}
	loom_render_unlock();
	return ret;
}

void loom_enable(int pid, int enable)
{
	if (enable)
		loom_activate(pid);
	else
		loom_deactivate(pid);
}

int loom_set_task_cfg(char *proc_name, char *thread_name,
	int pid, int mode, int matching_num, int prio, int cpu_mask,
	int set_exclusive, int loading_ub, int loading_lb, int bhr,
	int limit_min_freq, int limit_max_freq,
	int set_rescue, int rescue_f_opp, int rescue_c_freq, int rescue_time)
{
	struct loom_attr_info *task_attr;
	int ret = -1;

	loom_cfg_lock();
	if (strncmp("0", proc_name, 1) && !strncmp("0", thread_name, 1)) {
		loom_clear_loom_attr(loom_get_cfg_list());
		goto out;
	}

	task_attr = loom_search_add_task_cfg(loom_get_cfg_list(), mode, proc_name, thread_name, pid, 1);
	if (!task_attr)
		goto out;
	//assign value
	loom_assign_task_cfg(task_attr, mode, matching_num, prio, cpu_mask, set_exclusive,
		loading_ub, loading_lb, bhr, limit_min_freq, limit_max_freq,
		set_rescue, rescue_f_opp, rescue_c_freq, rescue_time);
	ret = 0;
	print_hlist("loom_task_cfg", -1, loom_get_cfg_list());
out:
	loom_cfg_unlock();
	return ret;
}
EXPORT_SYMBOL(loom_set_task_cfg);

int loom_reset_task_cfg(char *proc_name, char *thread_name, int pid)
{
	int ret = -1;
	struct loom_attr_info *task_attr;
	int mode;

	loom_cfg_lock();
	if ((!proc_name || !thread_name) && pid <= 0)
		goto out;

	mode = pid > 0 ? MATCH_PID : 0;
	task_attr = loom_search_add_task_cfg(loom_get_cfg_list(), mode, proc_name, thread_name, pid, 0);
	if (!task_attr)
		goto out;

	loom_delete_task_cfg(task_attr, loom_get_cfg_list());
	ret = 0;
	print_hlist("post delete loom_task_cfg", -1, loom_get_cfg_list());
out:
	loom_cfg_unlock();
	return ret;
}
EXPORT_SYMBOL(loom_reset_task_cfg);

static void clear_all_loom_render_info(void)
{
	struct loom_render_info *iter;
	struct hlist_node *tmp;

	hlist_for_each_entry_safe(iter, tmp, loom_get_render_list(), render_hlist) {
		loom_reset_operation(iter);
		switch_fpsgo_control(0, iter->pid, 1, 0);
		loom_delete_render_info(iter);
	}
}

static ssize_t loom_enable_by_process_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	struct loom_render_info *iter = NULL;
	char *temp = NULL;
	int pos = 0;
	int length = 0;

	temp = loom_calloc(FI_SYSFS_MAX_BUFF_SIZE, sizeof(char));
	if (!temp)
		goto out;

	loom_render_lock();
	length = scnprintf(temp + pos, FI_SYSFS_MAX_BUFF_SIZE - pos,
		"loom_render_info:%d\n", loom_get_render_num());
	pos += length;

	length = scnprintf(temp + pos, FI_SYSFS_MAX_BUFF_SIZE - pos,
		"%s\n", "tgid");
	pos += length;

	hlist_for_each_entry(iter, loom_get_render_list(), render_hlist) {
		length = scnprintf(temp + pos, FI_SYSFS_MAX_BUFF_SIZE - pos,
			"%d\n", iter->tgid);
		pos += length;
	}
	loom_render_unlock();

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);
out:
	kfree(temp);
	return length;
}


static ssize_t loom_enable_by_process_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int tgid;
	int arg;

	acBuffer = loom_calloc(FI_SYSFS_MAX_BUFF_SIZE, sizeof(char));
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FI_SYSFS_MAX_BUFF_SIZE) &&
		scnprintf(acBuffer,FI_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
		acBuffer[count] = '\0';
		if (sscanf(acBuffer, "%d %d", &tgid, &arg) == 2) {
			if (arg >= 0 && arg <= 1)
				loom_enable(tgid, arg);
		}
	}
out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(loom_enable_by_process);

static ssize_t loom_task_cfg_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	struct loom_attr_info *iter = NULL;
	char *temp = NULL;
	int pos = 0;
	int length = 0;

	temp = loom_calloc(FI_SYSFS_MAX_BUFF_SIZE, sizeof(char));
	if (!temp)
		goto out;

	loom_cfg_lock();
	length = scnprintf(temp + pos, FI_SYSFS_MAX_BUFF_SIZE - pos,
		"loom_task_cfg_list_length:%d\n", loom_get_cfg_length());
	pos += length;

	length = scnprintf(temp + pos, FI_SYSFS_MAX_BUFF_SIZE - pos,
	"%-16s %-8s %-16s %-8s %-8s %-16s %-8s %-10s %-16s %-10s %-10s %-8s %-16s %-16s %-10s %-16s %-16s %-16s\n",
		"process_name",
		"tgid",
		"thread_name",
		"pid",
		"mode",
		"matching_num",
		"prio",
		"cpu_mask",
		"set_exclusive",
		"loading_ub",
		"loading_lb",
		"bhr",
		"limit_min_freq",
		"limit_max_freq",
		"set_rescue",
		"rescue_f_opp",
		"rescue_c_freq",
		"rescue_time");
	pos += length;

	hlist_for_each_entry(iter, loom_get_cfg_list(), hlist) {
		length = scnprintf(temp + pos, FI_SYSFS_MAX_BUFF_SIZE - pos,
		"%-16s %-8d %-16s %-8d %-8d %-16d %-8d %-10d %-16d %-10d %-10d %-8d %-16d %-16d %-10d %-16d %-16d %-16d\n",
			iter->proc_name,
			iter->tgid,
			iter->thread_name,
			iter->pid,
			iter->mode,
			iter->matching_num,
			iter->prio,
			iter->cpu_mask,
			iter->set_exclusive,
			iter->loading_ub,
			iter->loading_lb,
			iter->bhr,
			iter->limit_min_freq,
			iter->limit_max_freq,
			iter->set_rescue,
			iter->rescue_f_opp,
			iter->rescue_c_freq,
			iter->rescue_time);
		pos += length;
	}
	loom_cfg_unlock();

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);
out:
	kfree(temp);
	return length;
}

static ssize_t loom_task_cfg_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char *acBuffer = NULL;
	char proc_name[16], thread_name[16];
	int pid, mode, matching_num;
	int prio;
	int cpu_mask, set_exclusive;
	int loading_ub, loading_lb;
	int bhr, set_rescue, rescue_f_opp, rescue_c_freq, rescue_time;
	int limit_min_freq, limit_max_freq;

	acBuffer = loom_calloc(FI_SYSFS_MAX_BUFF_SIZE, sizeof(char));
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FI_SYSFS_MAX_BUFF_SIZE) &&
		scnprintf(acBuffer,FI_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
		acBuffer[count] = '\0';
		if (sscanf(acBuffer,
			"%15s %15s %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
			proc_name, thread_name, &pid, &mode, &matching_num,
			&prio, &cpu_mask, &set_exclusive, &loading_ub,
			&loading_lb, &bhr, &limit_min_freq, &limit_max_freq,
			&set_rescue, &rescue_f_opp, &rescue_c_freq, &rescue_time) != 17)
			goto out;
		if (mode == LOOM_DEFAULT_VALUE &&
			matching_num == LOOM_DEFAULT_VALUE &&
			prio == LOOM_DEFAULT_VALUE &&
			cpu_mask == LOOM_DEFAULT_VALUE &&
			set_exclusive == LOOM_DEFAULT_VALUE &&
			loading_ub == LOOM_DEFAULT_VALUE &&
			loading_lb == LOOM_DEFAULT_VALUE &&
			bhr == LOOM_DEFAULT_VALUE &&
			limit_min_freq == LOOM_DEFAULT_VALUE &&
			limit_max_freq == LOOM_DEFAULT_VALUE &&
			set_rescue == LOOM_DEFAULT_VALUE &&
			rescue_f_opp == LOOM_DEFAULT_VALUE &&
			rescue_c_freq == LOOM_DEFAULT_VALUE &&
			rescue_time == LOOM_DEFAULT_VALUE)
			loom_reset_task_cfg(proc_name, thread_name, pid);
		else
			loom_set_task_cfg(proc_name, thread_name, pid, mode,
				matching_num, prio, cpu_mask, set_exclusive, loading_ub,
				loading_lb, bhr, limit_min_freq, limit_max_freq,
				set_rescue, rescue_f_opp, rescue_c_freq, rescue_time);
	}
out:
	loom_free(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(loom_task_cfg);

/* TODO */
void loom_exit(void)
{
	loom_render_lock();
	clear_all_loom_render_info();
	loom_register_frame_info_cb(0, &fpsgo_loom_frame_info_cb);
	exit_loom_loading_ctrl();
	loom_render_unlock();

	loom_cfg_lock();
	loom_clear_loom_attr(loom_get_cfg_list());
	loom_cfg_lock();

	game_sysfs_remove_file(loom_kobj, &kobj_attr_loom_enable_by_process);
	game_sysfs_remove_file(loom_kobj, &kobj_attr_loom_task_cfg);
}

/* TODO */
int loom_init(void)
{
	update_active_list_period = DEFAULT_LOOM_UPDATE_LIST_PERIOD;
	if (!game_get_sysfs_dir(&loom_kobj)) {
		game_sysfs_create_file(loom_kobj, &kobj_attr_loom_enable_by_process);
		game_sysfs_create_file(loom_kobj, &kobj_attr_loom_task_cfg);
	}
	init_loom_loading_ctrl();
	// Todo: create file node
	return 0;
}
