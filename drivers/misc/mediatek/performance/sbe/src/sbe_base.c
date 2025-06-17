// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/preempt.h>
#include <linux/smp.h>
#include <linux/trace_events.h>
#include <linux/sched/clock.h>
#include <linux/stdarg.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/vmalloc.h>
#include "fpsgo_frame_info.h"
#include "sbe_base.h"
#include "sbe_cpu_ctrl.h"
#include "sbe_sysfs.h"

#ifndef CREATE_TRACE_POINTS
#define CREATE_TRACE_POINTS
#endif
#include "sbe_trace_event.h"

static int display_rate;
static int total_sbe_spid_loading_num;
static struct kobject *sbe_base_kobj;
static struct rb_root sbe_info_tree;
static struct rb_root sbe_render_info_tree;
static struct rb_root sbe_spid_loading_tree;
static DEFINE_MUTEX(sbe_tree_lock);

void sbe_trace(const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;

	if (!trace_sbe_trace_enabled())
		return;

	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);

	if (unlikely(len == 256))
		log[255] = '\0';
	va_end(args);
	trace_sbe_trace(log);
}

void sbe_systrace_c(pid_t pid, unsigned long long bufID,
	int val, const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;
	char buf[256];

	if (!trace_sbe_systrace_enabled())
		return;

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		log[255] = '\0';

	if (!bufID) {
		len = snprintf(buf, sizeof(buf), "C|%d|%s|%d\n", pid, log, val);
	} else {
		len = snprintf(buf, sizeof(buf), "C|%d|%s|%d|0x%llx\n",
			pid, log, val, bufID);
	}
	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf[255] = '\0';

	trace_sbe_systrace(buf);
}

void sbe_get_tree_lock(const char *tag)
{
	mutex_lock(&sbe_tree_lock);
}

void sbe_put_tree_lock(const char *tag)
{
	mutex_unlock(&sbe_tree_lock);
}

unsigned long long sbe_get_time(void)
{
	unsigned long long temp;

	preempt_disable();
	temp = cpu_clock(smp_processor_id());
	preempt_enable();

	return temp;
}

void sbe_set_display_rate(int fps)
{
	display_rate = fps;
}

int sbe_get_display_rate(void)
{
	return display_rate;
}

int sbe_get_tgid(int pid)
{
	struct task_struct *tsk;
	int tgid = 0;

	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (tsk)
		get_task_struct(tsk);
	rcu_read_unlock();

	if (!tsk)
		return 0;

	tgid = tsk->tgid;
	put_task_struct(tsk);

	return tgid;
}

int sbe_arch_nr_clusters(void)
{
	int cpu, num = 0;
	struct cpufreq_policy *policy;

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy) {
			num = 0;
			break;
		}

		num++;
		cpu = cpumask_last(policy->related_cpus);
		cpufreq_cpu_put(policy);
	}

	return num;
}

int sbe_arch_nr_get_opp_cpu(int cpu)
{
	struct cpufreq_policy *curr_policy = NULL;
	struct cpufreq_frequency_table *pos, *table;
	int idx;
	int nr_opp = 0;
	int ret = 0;

	curr_policy = kzalloc(sizeof(struct cpufreq_policy), GFP_KERNEL);
	if (!curr_policy)
		goto out;

	ret = cpufreq_get_policy(curr_policy, cpu);
	if (ret == 0) {
		table = curr_policy->freq_table;
		if (table) {
			cpufreq_for_each_valid_entry_idx(pos, table, idx) {
				nr_opp++;
			}
		}
	}

out:
	kfree(curr_policy);
	return nr_opp;
}

int sbe_arch_nr_max_opp_cpu(void)
{
	int num_opp = 0, max_opp = 0;
	int cpu;
	int ret = 0;
	struct cpufreq_policy *curr_policy = NULL;

	curr_policy = kzalloc(sizeof(struct cpufreq_policy), GFP_KERNEL);
	if (!curr_policy)
		goto out;

	for_each_possible_cpu(cpu) {
		ret = cpufreq_get_policy(curr_policy, cpu);
		if (ret != 0)
			continue;

		num_opp = sbe_arch_nr_get_opp_cpu(cpu);
		cpu = cpumask_last(curr_policy->related_cpus);

		if (max_opp < num_opp)
			max_opp = num_opp;
	}

out:
	kfree(curr_policy);
	return max_opp;
}

struct sbe_info *sbe_get_info(int pid, int force)
{
	struct rb_node **p = &sbe_info_tree.rb_node;
	struct rb_node *parent = NULL;
	struct sbe_info *tmp = NULL;

	while (*p) {
		parent = *p;
		tmp = rb_entry(parent, struct sbe_info, entry);

		if (pid < tmp->pid)
			p = &(*p)->rb_left;
		else if (pid > tmp->pid)
			p = &(*p)->rb_right;
		else
			return tmp;
	}

	if (!force)
		return NULL;

	tmp = kzalloc(sizeof(struct sbe_info), GFP_KERNEL);
	if (!tmp)
		return NULL;

	tmp->pid = pid;
	tmp->ux_crtl_type = 0;
	tmp->ux_scrolling = 0;

	rb_link_node(&tmp->entry, parent, p);
	rb_insert_color(&tmp->entry, &sbe_info_tree);

	return tmp;
}

void sbe_delete_info(int pid)
{
	struct sbe_info *data;

	data = sbe_get_info(pid, 0);
	if (data) {
		rb_erase(&data->entry, &sbe_info_tree);
		kfree(data);
	}
}

int sbe_check_info_status(void)
{
	int count = 0;
	struct sbe_info *iter;
	struct rb_node *rbn;

	rbn = rb_first(&sbe_info_tree);
	while (rbn) {
		iter = rb_entry(rbn, struct sbe_info, entry);
		if (!sbe_get_tgid(iter->pid)) {
			rb_erase(&iter->entry, &sbe_info_tree);
			kfree(iter);
			rbn = rb_first(&sbe_info_tree);
		} else {
			rbn = rb_next(rbn);
			count++;
		}
	}

	return count;
}

struct sbe_render_info *sbe_get_render_info_by_thread_name(int tgid, char *thread_name)
{
	struct sbe_render_info *iter, *temp_info = NULL;
	struct rb_node *rbn;
	struct task_struct *tsk;

	rbn = rb_first(&sbe_render_info_tree);
	while (rbn) {
		iter = rb_entry(rbn, struct sbe_render_info, entry);
		if (iter->tgid == tgid) {
			rcu_read_lock();
			tsk = find_task_by_vpid(iter->pid);
			if (tsk) {
				get_task_struct(tsk);
				if (!strncmp(tsk->comm, thread_name, 16))
					temp_info = iter;
				put_task_struct(tsk);
			}
			rcu_read_unlock();
		}

		if (temp_info)
			return temp_info;

		rbn = rb_next(rbn);
	}

	return temp_info;
}

int sbe_get_render_tid_by_render_pid(int tgid, int pid,
	int *out_tid_arr, unsigned long long *out_bufID_arr,
	int *out_tid_num, int out_tid_max_num)
{
	int i;
	int index = 0;
	struct render_fw_info *tmp_arr = NULL;
	struct task_struct *tsk;

	if (tgid <= 0 || pid <= 0 ||
		!out_tid_arr || !out_bufID_arr ||
		!out_tid_num || out_tid_max_num <= 0)
		return -EINVAL;

	tmp_arr = kcalloc(out_tid_max_num, sizeof(struct render_fw_info), GFP_KERNEL);
	if (!tmp_arr)
		return -ENOMEM;

	fpsgo_other2comp_get_render_fw_info(0, out_tid_max_num, out_tid_num, tmp_arr);
	for (i = 0; i < *out_tid_num; i++) {
		if (tmp_arr[i].producer_tgid != tgid)
			continue;

		rcu_read_lock();
		tsk = find_task_by_vpid(tmp_arr[i].producer_pid);
		if (tsk) {
			get_task_struct(tsk);
			if (tmp_arr[i].producer_pid == pid && index < out_tid_max_num) {
				out_tid_arr[index] = tmp_arr[i].producer_pid;
				out_bufID_arr[index] = tmp_arr[i].buffer_id;
				index++;
			}
			put_task_struct(tsk);
		}
		rcu_read_unlock();
	}
	*out_tid_num = index;

	kfree(tmp_arr);

	return 0;
}

struct sbe_render_info *sbe_get_render_info(int pid,
	unsigned long long buffer_id, int force)
{
	struct rb_node **p = &sbe_render_info_tree.rb_node;
	struct rb_node *parent = NULL;
	struct sbe_render_info *tmp = NULL;

	while (*p) {
		parent = *p;
		tmp = rb_entry(parent, struct sbe_render_info, entry);

		if (pid < tmp->pid)
			p = &(*p)->rb_left;
		else if (pid > tmp->pid)
			p = &(*p)->rb_right;
		else {
			if (buffer_id < tmp->buffer_id)
				p = &(*p)->rb_left;
			else if (buffer_id > tmp->buffer_id)
				p = &(*p)->rb_right;
			return tmp;
		}
	}

	if (!force)
		return NULL;

	tmp = vzalloc(sizeof(struct sbe_render_info));
	if (!tmp)
		return NULL;

	tmp->pid = pid;
	tmp->tgid = sbe_get_tgid(pid);
	tmp->buffer_count_filter = 0;
	tmp->rescue_more_count = 0;
	tmp->frame_count = 0;
	tmp->affinity_task_mask = 0;
	tmp->ux_affinity_task_basic_cap = 0;
	tmp->critical_basic_cap = 0;
	tmp->sbe_rescuing_frame_id = -1;
	tmp->hwui_arr_idx = 0;
	tmp->target_fps = sbe_get_display_rate();
	tmp->target_time = div_u64(NSEC_PER_SEC, sbe_get_display_rate());
	tmp->buffer_id = buffer_id;
	tmp->rescue_start_time = 0;
	tmp->latest_use_ts = sbe_get_time();
	tmp->ux_frame_info_tree = RB_ROOT;
	tmp->dpt_policy_enable = 0;
	tmp->dpt_policy_force_disable = 0;
	tmp->affinity_task_mask_cnt = 0;
	tmp->calculate_dy_enhance_idx = 0;
	tmp->core_ctl_ignore_vip_task = 0;
	memset(tmp->aff_dep_arr, 0, sizeof(int) * MAX_TASK_NUM);

	INIT_LIST_HEAD(&(tmp->scroll_list));
	mutex_init(&tmp->ux_mlock);

	rb_link_node(&tmp->entry, parent, p);
	rb_insert_color(&tmp->entry, &sbe_render_info_tree);

	return tmp;
}

void sbe_delete_render_info(struct sbe_render_info *iter)
{
	int i;
	struct xgf_policy_cmd xgf_attr_iter;
	struct fpsgo_boost_attr attr_iter;

	if (!iter)
		return;

	// clean thread setting, e.g., uclamp, priority, affinity, ...
	if (iter->dep_self_ctrl) {
		for (i = 0; i < iter->dep_num; i++) {
			iter->ux_blc_next = 0;
			iter->ux_blc_cur = 0;
			iter->sbe_enhance = 0;
			__sbe_set_per_task_cap(iter, 0, 100);
			fpsgo_other2comp_set_no_boost_info(1, iter->dep_arr[i], 0);
		}
		sbe_set_deplist_policy(iter, 0);
	}

	switch_fpsgo_control(1, iter->pid, 0, iter->buffer_id);
	fpsgo_other2fstb_set_target(1, iter->pid, 0, 0, 0, 0, iter->buffer_id);
	memset(&xgf_attr_iter, -1, sizeof(struct xgf_policy_cmd));
	xgf_attr_iter.mode = 1;
	xgf_attr_iter.pid = iter->pid;
	xgf_attr_iter.bufid = iter->buffer_id;
	fpsgo_other2xgf_set_attr(0, &xgf_attr_iter);
	set_fpsgo_attr(1, iter->pid, 0, &attr_iter);

	sbe_del_ux(iter);
	rb_erase(&iter->entry, &sbe_render_info_tree);
	vfree(iter);
}

int sbe_forece_reset_fpsgo_critical_tasks(void)
{
	struct sbe_render_info *iter;
	struct rb_node *rbn;

	rbn = rb_first(&sbe_render_info_tree);
	while (rbn) {
		iter = rb_entry(rbn, struct sbe_render_info, entry);
		if (iter->fpsgo_critical_flag) {
			fpsgo_other2xgf_set_critical_tasks(iter->pid, iter->buffer_id, NULL, 0, 0);
			iter->fpsgo_critical_flag = 0;
		}
		rbn = rb_next(rbn);
	}

	return 0;
}

int sbe_check_render_info_status(void)
{
	int count = 0;
	unsigned long long cur_ts = sbe_get_time();
	struct sbe_render_info *iter;
	struct rb_node *rbn;

	rbn = rb_first(&sbe_render_info_tree);
	while (rbn) {
		iter = rb_entry(rbn, struct sbe_render_info, entry);

		if (((cur_ts - iter->latest_use_ts > 60*10*NSEC_PER_SEC)
			&& (!iter->scroll_status)) ||
			!sbe_get_tgid(iter->pid)) {
			sbe_delete_render_info(iter);
			rbn = rb_first(&sbe_render_info_tree);
		} else {
			rbn = rb_next(rbn);
			count++;
		}
	}

	return count;
}

int sbe_check_spid_loading_status(void)
{
	int local_tgid = 0;
	struct sbe_spid_loading *iter = NULL;
	struct rb_node *rbn = NULL;

	rbn = rb_first(&sbe_spid_loading_tree);
	while (rbn) {
		iter = rb_entry(rbn, struct sbe_spid_loading, rb_node);
		local_tgid = sbe_get_tgid(iter->tgid);
		if (local_tgid)
			rbn = rb_next(rbn);
		else {
			rb_erase(rbn, &sbe_spid_loading_tree);
			kfree(iter);
			total_sbe_spid_loading_num--;
			rbn = rb_first(&sbe_spid_loading_tree);
		}
	}

	return total_sbe_spid_loading_num;
}

static void sbe_delete_oldest_spid_loading(void)
{
	unsigned long long min_ts = ULLONG_MAX;
	struct sbe_spid_loading *iter = NULL, *min_iter = NULL;
	struct rb_node *rbn = NULL;

	for (rbn = rb_first(&sbe_spid_loading_tree); rbn; rbn = rb_next(rbn)) {
		iter = rb_entry(rbn, struct sbe_spid_loading, rb_node);
		if (iter->ts < min_ts) {
			min_ts = iter->ts;
			min_iter = iter;
		}
	}

	if (min_iter) {
		rb_erase(&min_iter->rb_node, &sbe_spid_loading_tree);
		kfree(min_iter);
		total_sbe_spid_loading_num--;
	}
}

static struct sbe_spid_loading *sbe_get_spid_loading(int tgid, int create)
{
	struct rb_node **p = &sbe_spid_loading_tree.rb_node;
	struct rb_node *parent = NULL;
	struct sbe_spid_loading *iter = NULL;

	while (*p) {
		parent = *p;
		iter = rb_entry(parent, struct sbe_spid_loading, rb_node);

		if (tgid < iter->tgid)
			p = &(*p)->rb_left;
		else if (tgid > iter->tgid)
			p = &(*p)->rb_right;
		else
			return iter;
	}

	if (!create)
		return NULL;

	iter = kzalloc(sizeof(struct sbe_spid_loading), GFP_KERNEL);
	if (!iter)
		return NULL;

	iter->tgid = tgid;
	iter->spid_num = 0;
	iter->ts = sbe_get_time();

	rb_link_node(&iter->rb_node, parent, p);
	rb_insert_color(&iter->rb_node, &sbe_spid_loading_tree);
	total_sbe_spid_loading_num++;

	if (total_sbe_spid_loading_num >= MAX_SBE_SPID_LOADING_SIZE)
		sbe_delete_oldest_spid_loading();

	return iter;
}

int sbe_delete_spid_loading(int tgid)
{
	int ret = 0;
	struct sbe_spid_loading *iter = NULL;

	sbe_get_tree_lock(__func__);
	iter = sbe_get_spid_loading(tgid, 0);
	if (iter) {
		rb_erase(&iter->rb_node, &sbe_spid_loading_tree);
		kfree(iter);
		total_sbe_spid_loading_num--;
		ret = 1;
	}
	sbe_put_tree_lock(__func__);

	return ret;
}

int sbe_update_spid_loading(int *cur_pid_arr, int cur_pid_num, int tgid)
{
	int ret = 0;
	int i;
	unsigned long long local_runtime;
	struct sbe_spid_loading *iter = NULL;
	struct task_struct *tsk = NULL;

	if (!cur_pid_arr || cur_pid_num <= 0) {
		ret = -EINVAL;
		return ret;
	}

	sbe_get_tree_lock(__func__);
	iter = sbe_get_spid_loading(tgid, 1);
	if (!iter) {
		ret = -ENOMEM;
		goto out;
	}

	iter->ts = sbe_get_time();

	memset(iter->spid_arr, 0, MAX_TASK_NUM * sizeof(int));
	memset(iter->spid_latest_runtime, 0,
		MAX_TASK_NUM * sizeof(unsigned long long));

	for (i = 0; i < cur_pid_num; i++) {
		local_runtime = 0;
		rcu_read_lock();
		tsk = find_task_by_vpid(cur_pid_arr[i]);
		if (tsk) {
			get_task_struct(tsk);
			local_runtime = tsk->se.sum_exec_runtime;
			put_task_struct(tsk);
		}
		rcu_read_unlock();

		iter->spid_arr[i] = cur_pid_arr[i];
		iter->spid_latest_runtime[i] = local_runtime;
		sbe_trace("[SBE] %s %dth tgid:%d update spid:%d runtime:%llu",
			__func__, i+1, tgid, cur_pid_arr[i], local_runtime);

		if (i == MAX_TASK_NUM - 1)
			break;
	}
	iter->spid_num = cur_pid_num <= MAX_TASK_NUM ? cur_pid_num : MAX_TASK_NUM;

out:
	sbe_put_tree_lock(__func__);
	return ret;
}

int sbe_query_spid_loading(void)
{
	int ret = 0;
	int i;
	unsigned long long local_runtime;
	struct sbe_spid_loading *iter = NULL;
	struct rb_node *rbn = NULL;
	struct task_struct *tsk = NULL;

	sbe_get_tree_lock(__func__);
	for (rbn = rb_first(&sbe_spid_loading_tree); rbn; rbn = rb_next(rbn)) {
		iter = rb_entry(rbn, struct sbe_spid_loading, rb_node);
		if (!iter || !sbe_get_tgid(iter->tgid))
			continue;

		for (i = 0; i < iter->spid_num; i++) {
			local_runtime = 0;
			rcu_read_lock();
			tsk = find_task_by_vpid(iter->spid_arr[i]);
			if (tsk) {
				get_task_struct(tsk);
				local_runtime = tsk->se.sum_exec_runtime;
				put_task_struct(tsk);
			}
			rcu_read_unlock();

			sbe_trace("[SBE] %s %dth tgid:%d query spid:%d runtime:%llu->%llu",
				__func__, i+1, iter->tgid, iter->spid_arr[i], iter->spid_latest_runtime[i], local_runtime);

			if (local_runtime > 0 &&
				local_runtime > iter->spid_latest_runtime[i]) {
				ret = 1;
				break;
			}
		}

		if (ret)
			break;
	}
	sbe_put_tree_lock(__func__);

	return ret;
}

int sbe_split_task_name(int tgid, char *dep_name, int dep_num, int *out_tid_arr, const char *caller)
{
	char *thread_name = NULL, *remain_str = NULL;
	char local_thread_name[16];
	int i;
	int index = 0;
	struct task_struct *tg = NULL, *sib = NULL;

	if (!dep_name || dep_num <= 0 || !out_tid_arr)
		return index;

	remain_str = dep_name;
	for (i = 0; i < dep_num; i++) {
		thread_name = strsep(&remain_str, ",");
		if (!thread_name ||
			!strncpy(local_thread_name, thread_name, 16))
			break;
		local_thread_name[15] = '\0';
		sbe_trace("[SBE] %s split task name: %s", caller, local_thread_name);

		rcu_read_lock();
		tg = find_task_by_vpid(tgid);
		if (tg) {
			get_task_struct(tg);
			for_each_thread(tg, sib) {
				get_task_struct(sib);
				if (!strncmp(sib->comm, local_thread_name, 16)) {
					if (index < dep_num) {
						out_tid_arr[index] = sib->pid;
						index++;
						sbe_trace("[SBE] %s split task tid: %d",
							caller, sib->pid);
					}
				}
				put_task_struct(sib);
			}
			put_task_struct(tg);
		}
		rcu_read_unlock();
	}

	return index;
}


int sbe_split_task_tid(char *dep_name, int dep_num, int *out_tid_arr, const char *caller)
{
	char *thread_name = NULL, *remain_str = NULL;
	char local_thread_name[16];
	int i;
	int index = 0;
	int tid;

	if (!dep_name || dep_num <= 0 || !out_tid_arr)
		return index;

	remain_str = dep_name;
	for (i = 0; i < dep_num; i++) {
		thread_name = strsep(&remain_str, ",");
		if (!thread_name || !memcpy(local_thread_name, thread_name, 16))
			break;
		local_thread_name[15] = '\0';
		sbe_trace("[SBE] %s split task name: %s", caller, local_thread_name);

		if (kstrtoint(local_thread_name, 10, &tid))
			continue;

		out_tid_arr[index] = tid;
		index++;
		sbe_trace("[SBE] %s split task tid: %d", caller, tid);
	}

	return index;
}

static ssize_t sbe_render_info_status_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct sbe_render_info *iter1;
	struct sbe_info *iter2;
	struct sbe_spid_loading *iter3;
	struct rb_node *rbn;
	char *temp = NULL;
	int pos = 0;
	int length = 0;
	int i;

	temp = kcalloc(SBE_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	sbe_get_tree_lock(__func__);

	length = scnprintf(temp + pos, SBE_SYSFS_MAX_BUFF_SIZE - pos,
		"sbe_render_info:\n");
	pos += length;
	for (rbn = rb_first(&sbe_render_info_tree); rbn; rbn = rb_next(rbn)) {
		iter1 = rb_entry(rbn, struct sbe_render_info, entry);
		length = scnprintf(temp + pos, SBE_SYSFS_MAX_BUFF_SIZE - pos,
				"\ttgid:%d pid:%d id:0x%llx target:%d %llu cpu_time:%llu %llu blc:%d %d\n",
				iter1->tgid, iter1->pid, iter1->buffer_id,
				iter1->target_fps, iter1->target_time,
				iter1->raw_running_time, iter1->ema_running_time,
				iter1->ux_blc_cur, iter1->ux_blc_next);
		pos += length;
	}

	length = scnprintf(temp + pos, SBE_SYSFS_MAX_BUFF_SIZE - pos,
		"sbe_info:\n");
	pos += length;
	for (rbn = rb_first(&sbe_info_tree); rbn; rbn = rb_next(rbn)) {
		iter2 = rb_entry(rbn, struct sbe_info, entry);
		length = scnprintf(temp + pos, SBE_SYSFS_MAX_BUFF_SIZE - pos,
				"\tpid:%d ux_crtl_type:%d ux_scrolling:%d\n",
				iter2->pid, iter2->ux_crtl_type, iter2->ux_scrolling);
		pos += length;
	}

	length = scnprintf(temp + pos, SBE_SYSFS_MAX_BUFF_SIZE - pos,
		"sbe_spid_loading:\n");
	pos += length;
	for (rbn = rb_first(&sbe_spid_loading_tree); rbn; rbn = rb_next(rbn)) {
		iter3 = rb_entry(rbn, struct sbe_spid_loading, rb_node);
		length = scnprintf(temp + pos, SBE_SYSFS_MAX_BUFF_SIZE - pos,
			"\ttgid:%d ts:%llu\n", iter3->tgid, iter3->ts);
		pos += length;
		for (i = 0; i < iter3->spid_num; i++) {
			length = scnprintf(temp + pos, SBE_SYSFS_MAX_BUFF_SIZE - pos,
				"\t\tspid_arr[%d]:%d spid_latest_runtime[%d]:%llu\n",
				i, iter3->spid_arr[i], i, iter3->spid_latest_runtime[i]);
			pos += length;
		}
	}
	sbe_put_tree_lock(__func__);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}
static KOBJ_ATTR_RO(sbe_render_info_status);

int sbe_base_init(void)
{
	sbe_info_tree = RB_ROOT;
	sbe_render_info_tree = RB_ROOT;
	sbe_spid_loading_tree = RB_ROOT;

	if (!sbe_sysfs_create_dir(NULL, "base", &sbe_base_kobj)) {
		sbe_sysfs_create_file(sbe_base_kobj, &kobj_attr_sbe_render_info_status);
	}

	return 0;
}

void sbe_base_exit(void)
{
	sbe_sysfs_remove_file(sbe_base_kobj, &kobj_attr_sbe_render_info_status);

	sbe_sysfs_remove_dir(&sbe_base_kobj);
}

void sbe_get_proc_name(int tgid, char *name)
{
	struct task_struct *gtsk = NULL;

	if (!name)
		return;

	rcu_read_lock();
	gtsk = find_task_by_vpid(tgid);
	if (gtsk) {
		get_task_struct(gtsk);
		strscpy(name, gtsk->comm, 16);
		put_task_struct(gtsk);
		name[15] = '\0';
	} else {
		name[0]= '\0';
	}
	rcu_read_unlock();
}
