// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <asm/div64.h>
#include <trace/hooks/sched.h>
#include <trace/events/sched.h>
#include "pelt_hint_sysfs.h"
#include "pelt_hint_usedext.h"
#include "pelt_hint.h"

#ifndef CREATE_TRACE_POINTS
#define CREATE_TRACE_POINTS
#endif
#include "pelt_hint_trace_event.h"

#define UTIL_EST_MARGIN (SCHED_CAPACITY_SCALE / 100)

static int gai_cpu_boost_enable;
static int gai_cpu_boost_ratio = UTIL_EST_WEIGHT_SHIFT;
static int detect_fork_tgid;
static struct kobject *pelt_hint_kobj;
static HLIST_HEAD(gai_thread_info_list);
static DEFINE_MUTEX(gai_thread_info_lock);

void pelt_hint_main_trace(const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;

	if (!trace_pelt_hint_trace_enabled())
		return;

	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);

	if (unlikely(len == 256))
		log[255] = '\0';
	va_end(args);
	trace_pelt_hint_trace(log);
}

/* clone from common kernel fair.c */
static inline unsigned long task_util(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.util_avg);
}

/* clone from common kernel fair.c */
static inline unsigned long task_runnable(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.runnable_avg);
}

///* clone from common kernel fair.c */
//static inline unsigned long _task_util_est(struct task_struct *p)
//{
//	return READ_ONCE(p->se.avg.util_est) & ~UTIL_AVG_UNCHANGED;
//}

///* clone from common kernel fair.c */
//static inline unsigned long task_util_est(struct task_struct *p)
//{
//	return max(task_util(p), _task_util_est(p));
//}

void mtk_hook_util_est_update(void *data, struct cfs_rq *cfs_rq,
	struct task_struct *p, bool task_sleep, int *ret)
{
	unsigned int ewma, dequeued, last_ewma_diff;
	unsigned int final_ratio;
	struct pelt_hint_oem_task_data *pelt_hint_tsk = NULL;

	if (!gai_cpu_boost_enable)
		return;

	pelt_hint_tsk = (struct pelt_hint_oem_task_data *) p->android_oem_data1;
	pelt_hint_main_trace("[PELT_HINT] pid:%d gai_task_flag:%d gai_cpu_boost_enable:%d",
		p->pid, pelt_hint_tsk->gai_task_flag, gai_cpu_boost_enable);
	if (gai_cpu_boost_enable == 2 && !pelt_hint_tsk->gai_task_flag) {
		if (p->tgid == detect_fork_tgid) {
			pelt_hint_tsk->gai_task_flag = 1;
			pelt_hint_main_trace("[PELT_HINT] pid:%d is new task, gai_task_flag:%d",
				p->pid, pelt_hint_tsk->gai_task_flag);
		} else
			return;
	}

	if (!sched_feat(UTIL_EST))
		return;

	if (!task_sleep)
		return;

	ewma = READ_ONCE(p->se.avg.util_est);

	if (ewma & UTIL_AVG_UNCHANGED) {
		pelt_hint_main_trace("[PELT_HINT] UTIL_AVG_UNCHANGED mask not reset pid:%d ewma:%u", p->pid, ewma);
		return;
	}

	dequeued = task_util(p);
	pelt_hint_main_trace("[PELT_HINT] pid:%d util:%u util_est:%u runnable_avg:%lu cpu:%d(%d)",
		p->pid, dequeued, ewma, task_runnable(p),
		cpu_of(rq_of(cfs_rq)), arch_scale_cpu_capacity(cpu_of(rq_of(cfs_rq))));

	if (ewma <= dequeued) {
		ewma = dequeued;
		goto done;
	}

	last_ewma_diff = ewma - dequeued;
	if (last_ewma_diff < UTIL_EST_MARGIN)
		goto done;

	if (dequeued > arch_scale_cpu_capacity(cpu_of(rq_of(cfs_rq))))
		return;

	if ((dequeued + UTIL_EST_MARGIN) < task_runnable(p))
		goto done;

	if (gai_cpu_boost_ratio > 0)
		final_ratio = gai_cpu_boost_ratio;
	else
		final_ratio = UTIL_EST_WEIGHT_SHIFT;

	ewma <<= final_ratio;
	ewma  -= last_ewma_diff;
	ewma >>= final_ratio;
	ewma++;
	pelt_hint_main_trace("[PELT_HINT] pid:%d final_ratio:%u util_est:%u", p->pid, final_ratio, ewma);
done:
	ewma |= UTIL_AVG_UNCHANGED;
	WRITE_ONCE(p->se.avg.util_est, ewma);

	*ret = 1;
}

static struct gai_thread_info *pelt_hint_get_gai_thread_info(int pid)
{
	struct gai_thread_info *iter = NULL;
	struct hlist_node *h = NULL;

	hlist_for_each_entry_safe(iter, h, &gai_thread_info_list, hlist) {
		if (iter->pid == pid)
			break;
	}

	return iter;
}

static int pelt_hint_add_gai_thread_info(int pid, struct task_struct *tsk)
{
	struct gai_thread_info *iter = NULL;
	struct pelt_hint_oem_task_data *pelt_hint_tsk = NULL;
	struct task_struct *tmp_tsk = NULL;

	iter = kzalloc(sizeof(struct gai_thread_info), GFP_KERNEL);
	if (!iter) {
		/*pr_debug("[pelt_hint] %s memory alloc fail\n", __func__);*/
		return -ENOMEM;
	}

	iter->pid = pid;
	hlist_add_head(&iter->hlist, &gai_thread_info_list);
	pr_debug("[pelt_hint] %s add pid:%d\n", __func__, pid);

	pr_debug("[pelt_hint] %s add 1\n", __func__);
	if (tsk) {
		pelt_hint_tsk = (struct pelt_hint_oem_task_data *) tsk->android_oem_data1;
		pelt_hint_tsk->gai_task_flag = 1;
		pr_debug("[pelt_hint] %s add 2\n", __func__);
	} else {
		rcu_read_lock();
		tmp_tsk = find_task_by_vpid(pid);
		if (tmp_tsk) {
			get_task_struct(tmp_tsk);
			pelt_hint_tsk = (struct pelt_hint_oem_task_data *) tmp_tsk->android_oem_data1;
			pelt_hint_tsk->gai_task_flag = 1;
			put_task_struct(tmp_tsk);
			pr_debug("[pelt_hint] %s add 2\n", __func__);
		}
		rcu_read_unlock();
	}
	pr_debug("[pelt_hint] %s add 3\n", __func__);

	return 0;
}

static int pelt_hint_delete_gai_thread_info(int pid, struct task_struct *tsk)
{
	struct gai_thread_info *iter = NULL;
	struct pelt_hint_oem_task_data *pelt_hint_tsk = NULL;
	struct task_struct *tmp_tsk = NULL;

	iter = pelt_hint_get_gai_thread_info(pid);
	if (!iter)
		return -EINVAL;
	hlist_del(&iter->hlist);
	kfree(iter);
	pr_debug("[pelt_hint] %s del pid:%d\n", __func__, pid);

	pr_debug("[pelt_hint] %s del 1\n", __func__);
	if (tsk) {
		pelt_hint_tsk = (struct pelt_hint_oem_task_data *) tsk->android_oem_data1;
		pelt_hint_tsk->gai_task_flag = 0;
		pr_debug("[pelt_hint] %s del 2\n", __func__);
	} else {
		rcu_read_lock();
		tmp_tsk = find_task_by_vpid(pid);
		if (tmp_tsk) {
			get_task_struct(tmp_tsk);
			pelt_hint_tsk = (struct pelt_hint_oem_task_data *) tmp_tsk->android_oem_data1;
			pelt_hint_tsk->gai_task_flag = 0;
			put_task_struct(tmp_tsk);
			pr_debug("[pelt_hint] %s del 2\n", __func__);
		}
		rcu_read_unlock();
	}
	pr_debug("[pelt_hint] %s del 3\n", __func__);

	return 0;
}

static void pelt_hint_check_gai_thread_info(void)
{
	struct gai_thread_info *iter = NULL;
	struct hlist_node *h = NULL;

	hlist_for_each_entry_safe(iter, h, &gai_thread_info_list, hlist) {
		rcu_read_lock();
		if (!find_task_by_vpid(iter->pid)) {
			pr_debug("[pelt_hint] %s pid:%d not exist, need to delete\n", __func__, iter->pid);
			hlist_del(&iter->hlist);
			kfree(iter);
		}
		rcu_read_unlock();
	}
}

static void pelt_hint_sched_process_fork_tracer(void *ignore,
	struct task_struct *parent, struct task_struct *pelt_hintld)
{
	// do something
	pelt_hint_main_trace("%s in %d fork %d cpu:%d ...",
		__func__, parent->pid, pelt_hintld->pid, smp_processor_id());
}

struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool registered;
};

static struct tracepoints_table pelt_hint_tracepoints[] = {
	{.name = "sched_process_fork", .func = pelt_hint_sched_process_fork_tracer},
};

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(pelt_hint_tracepoints) / sizeof(struct tracepoints_table); i++)

static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(pelt_hint_tracepoints[i].name, tp->name) == 0)
			pelt_hint_tracepoints[i].tp = tp;
	}
}

static void pelt_hint_clear_tracepoints(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (pelt_hint_tracepoints[i].registered) {
			tracepoint_probe_unregister(pelt_hint_tracepoints[i].tp, pelt_hint_tracepoints[i].func, NULL);
			pelt_hint_tracepoints[i].registered = false;
		}
	}
}

static void pelt_hint_register_trace_event(void)
{
	int i, ret = 0;

	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	FOR_EACH_INTEREST(i) {
		if (pelt_hint_tracepoints[i].tp == NULL) {
			pr_debug("[PELT_HINT] %s tracepoint not found\n", pelt_hint_tracepoints[i].name);
			pelt_hint_clear_tracepoints();
			return;
		}
	}

	ret = tracepoint_probe_register(pelt_hint_tracepoints[0].tp, pelt_hint_tracepoints[0].func, NULL);
	if (ret) {
		pr_debug("[PELT_HINT] Couldn't activate tracepoint probe of sched_process_fork\n");
		return;
	}
	pelt_hint_tracepoints[0].registered = true;
}

CHI_SYSFS_READ(gai_cpu_boost_enable, 1, gai_cpu_boost_enable);
CHI_SYSFS_WRITE_VALUE(gai_cpu_boost_enable, gai_cpu_boost_enable, 0, 2);
static KOBJ_ATTR_RW(gai_cpu_boost_enable);

CHI_SYSFS_READ(gai_cpu_boost_ratio, 1, gai_cpu_boost_ratio);
CHI_SYSFS_WRITE_VALUE(gai_cpu_boost_ratio, gai_cpu_boost_ratio, 0, 30);
static KOBJ_ATTR_RW(gai_cpu_boost_ratio);

static ssize_t set_gai_thread_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int pos = 0;
	int length = 0;
	struct gai_thread_info *iter = NULL;
	struct hlist_node *h = NULL;

	temp = kcalloc(PELT_HINT_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	length = scnprintf(temp + pos, PELT_HINT_SYSFS_MAX_BUFF_SIZE - pos,
			"detect_fork_tgid: %d\n", detect_fork_tgid);
	pos += length;

	length = scnprintf(temp + pos, PELT_HINT_SYSFS_MAX_BUFF_SIZE - pos,
			"GAI CPU thread: ");
	pos += length;

	mutex_lock(&gai_thread_info_lock);
	hlist_for_each_entry_safe(iter, h, &gai_thread_info_list, hlist) {
		length = scnprintf(temp + pos, PELT_HINT_SYSFS_MAX_BUFF_SIZE - pos,
			"%d ", iter->pid);
		pos += length;
	}
	mutex_unlock(&gai_thread_info_lock);

	length = scnprintf(temp + pos, PELT_HINT_SYSFS_MAX_BUFF_SIZE - pos, "\n");
	pos += length;

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}
/*
 * mode: 0(by process), 1(by thread)
 * pid: +pid(add) -pid(delete)
 */
void set_gai_thread(int mode, int pid)
{
	int local_pid = 0;

	local_pid = pid > 0 ? pid : -pid;
	pr_debug("[pelt_hint] %s mode:%d pid:%d local_pid:%d\n", __func__, mode, pid, local_pid);

	mutex_lock(&gai_thread_info_lock);
	if (mode == 0) {
		struct task_struct *gtsk, *sib;

		rcu_read_lock();
		gtsk = find_task_by_vpid(local_pid);
		if (gtsk) {
			get_task_struct(gtsk);
			for_each_thread(gtsk, sib) {
				get_task_struct(sib);
				if (pid > 0 && !pelt_hint_get_gai_thread_info(sib->pid))
					pelt_hint_add_gai_thread_info(sib->pid, sib);
				else
					pelt_hint_delete_gai_thread_info(sib->pid, sib);
				put_task_struct(sib);
			}
			put_task_struct(gtsk);
		}
		rcu_read_unlock();

		if (pid > 0)
			detect_fork_tgid = local_pid;
		else if (detect_fork_tgid == local_pid)
			detect_fork_tgid = 0;
	} else if (mode == 1) {
		if (pid > 0 && !pelt_hint_get_gai_thread_info(local_pid))
			pelt_hint_add_gai_thread_info(local_pid, NULL);
		else
			pelt_hint_delete_gai_thread_info(local_pid, NULL);
	}
	pelt_hint_check_gai_thread_info();
	mutex_unlock(&gai_thread_info_lock);
}

static ssize_t set_gai_thread_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int arg = 0;
	int mode = -1;
	//int local_pid = 0;

	acBuffer = kcalloc(PELT_HINT_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < PELT_HINT_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, PELT_HINT_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (sscanf(acBuffer, "%d %d", &mode, &arg) != 2)
				goto out;
			set_gai_thread(mode, arg);
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(set_gai_thread);
int magt2pelt_notify_pelt_hint_boost(int enable, int pid_mode, int pid, int ratio)
{

	if (enable <= 2 && enable > 0 && ratio > 1) {
		gai_cpu_boost_enable = enable;
		gai_cpu_boost_ratio = ratio;
		set_gai_thread(pid_mode, pid);
	} else {
		gai_cpu_boost_enable = 0;
		gai_cpu_boost_ratio = UTIL_EST_WEIGHT_SHIFT;
		set_gai_thread(pid_mode, -1);
	}

	return 0;
}

static void __exit pelt_hint_test_main_exit(void)
{
	pelt_hint_sysfs_remove_file(pelt_hint_kobj, &kobj_attr_gai_cpu_boost_enable);
	pelt_hint_sysfs_remove_file(pelt_hint_kobj, &kobj_attr_gai_cpu_boost_ratio);
	pelt_hint_sysfs_remove_file(pelt_hint_kobj, &kobj_attr_set_gai_thread);
	pelt_hint_sysfs_remove_dir(&pelt_hint_kobj);
	pelt_hint_sysfs_exit();
}

static int __init pelt_hint_test_main_init(void)
{
	int ret = 0;

	pelt_hint_sysfs_init();
	if (!pelt_hint_sysfs_create_dir(NULL, "common", &pelt_hint_kobj)) {
		pelt_hint_sysfs_create_file(pelt_hint_kobj, &kobj_attr_gai_cpu_boost_enable);
		pelt_hint_sysfs_create_file(pelt_hint_kobj, &kobj_attr_gai_cpu_boost_ratio);
		pelt_hint_sysfs_create_file(pelt_hint_kobj, &kobj_attr_set_gai_thread);
	}

	pelt_hint_register_trace_event();
	magt2pelt_notify_pelt_hint_boost_fp = magt2pelt_notify_pelt_hint_boost;

	ret = register_trace_android_rvh_util_est_update(mtk_hook_util_est_update, NULL);
	if (ret)
		pr_debug("[PELT_HINT] register_trace_android_rvh_util_est_update fail ret:%d\n", ret);

	return 0;
}

module_init(pelt_hint_test_main_init);
module_exit(pelt_hint_test_main_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek PELT_HINTA-PELT_HINT EXPERIMENT");
MODULE_AUTHOR("MediaTek PELT_HINTA-PELT_HINT");
