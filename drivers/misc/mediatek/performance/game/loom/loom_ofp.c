// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include <linux/workqueue.h>
#include <linux/sched/cputime.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include "loom_base.h"
#include "loom.h"
#include "loom_ofp.h"
#include "game_sysfs.h"
#include "game.h"
static struct kobject *loom_kobj;
static int ofp_enable;
static int ofp_polling_ms;
static int ofp_overload_th;
static int ofp_light_th;
static int over_trigger_cnt;
static int light_trigger_cnt;
static struct workqueue_struct *loom_ofp_WorkQueue;
static struct hrtimer loom_ofp_hrt;
static void notify_cpu_loading_update(struct work_struct *work);
static DECLARE_WORK(loom_ofp_work,
	(void *)notify_cpu_loading_update);

static int ofp_over_cnt;
static int ofp_light_cnt;
static int ofp_cur_loading;
int ofp_is_overload;				/* CPUs which are not isolated are overloaded */

static int ws_loading;				/* Whole system CPUs loading */
struct cpu_info *cpu_load_info;		/* CPU workload tracking info */
int loom_cpu_num;

void loom_notify_dedicated(int cpu_id, int set)
{
	if (!cpu_load_info || !loom_cpu_num) {
		loom_main_trace("[%s] load_info not initialized.", __func__);
		return;
	}
	cpu_load_info[cpu_id].is_isolated = set;
	loom_systrace_c(-100, 0, set, "cpu[%d]_isolation", cpu_id);
}

static void loom_check_loading_status(int loading, int overload_th, int light_th,
	int over_trigger, int light_trigger, int *overload_cnt, int *light_cnt,
	int *is_overload, const char *tag)
{
	if (loading >= overload_th) {
		*light_cnt = 0;
		*overload_cnt += 1;
		if (*overload_cnt >= over_trigger) {
			*is_overload = 1;
			loom_main_trace("[%s] %s overload=%d", __func__, tag, 1);
		}
	} else if (loading <= light_th) {
		*overload_cnt = 0;
		*light_cnt += 1;
		if (*light_cnt >= light_trigger) {
			*is_overload = 0;
			loom_main_trace("[%s] %s overload=%d", __func__, tag, 0);
		}
	} else {
		*overload_cnt = 0;
		*light_cnt = 0;
	}
}

static int loom_update_cpu_loading(void)
{
	int i;
	u64 total_wall = 0, total_idle = 0;
	u64 ofp_wall = 0, ofp_idle = 0;

	if (!ofp_enable)
		return -EINVAL;

	if (!cpu_load_info) {
		loom_main_trace("[%s]update failed. cpu_load_info NULL.", __func__);
		return -EINVAL;
	}

	/* update per-cpu load info */
	for_each_possible_cpu(i) {
		u64 wall_time = 0, idle_time = 0;

		cpu_load_info[i].prev_wall_time = cpu_load_info[i].cur_wall_time;
		cpu_load_info[i].prev_idle_time = cpu_load_info[i].cur_idle_time;
		/* note: idle time include iowait */
		cpu_load_info[i].cur_idle_time = get_cpu_idle_time(i,
			&cpu_load_info[i].cur_wall_time, 1);

		if (cpu_active(i)) {  //!!!!!!!!!!!!!!!!! vip_engine only use cpu_active , need verify
			wall_time = cpu_load_info[i].cur_wall_time - cpu_load_info[i].prev_wall_time;
			idle_time = cpu_load_info[i].cur_idle_time - cpu_load_info[i].prev_idle_time;
			total_wall += wall_time;
			total_idle += idle_time;
			//loom_systrace_c(-100, 0, wall_time, "ofp_wall_time[%d]", i); //debug use
			//loom_systrace_c(-100, 0, idle_time, "ofp_idle_time[%d]", i); //debug use

			if (!cpu_load_info[i].is_isolated) {
				ofp_wall += wall_time;
				ofp_idle += idle_time;
				//loom_systrace_c(-100, 0, 1, "not_isolated[%d]", i); //debug use
			}

			if (wall_time > 0 && wall_time > idle_time) {
				cpu_load_info[i].cur_loading = (int)(div_u64(100 *
					(wall_time - idle_time), wall_time));
				loom_systrace_c(-100, 0, cpu_load_info[i].cur_loading, "ofp_loading[%d]", i);
			}
		}
	}

	/* update whole system load info */
	if (total_wall > 0 && total_wall > total_idle) {
		ws_loading = (int)(div_u64(100 * (total_wall - total_idle), total_wall));
		loom_systrace_c(-100, 0, ws_loading, "whole_system_loading");
	}
	/* update ofp load info */
	if (ofp_wall > 0 && ofp_wall > ofp_idle) {
		ofp_cur_loading = (int)(div_u64(100 * (ofp_wall - ofp_idle), ofp_wall));
		loom_systrace_c(-100, 0, ofp_cur_loading, "ofp_loading");
		loom_check_loading_status(ofp_cur_loading, ofp_overload_th, ofp_light_th,
			over_trigger_cnt, light_trigger_cnt, &ofp_over_cnt,
				&ofp_light_cnt, &ofp_is_overload, "ofp");
	}

	if (ofp_is_overload) {
		loom_systrace_c(-100, 0, 1, "loom_overload");
		loom_systrace_c(-100, 0, 0, "loom_overload");
	}
	return 0;
}

static void loom_cpu_ofp(void)
{
	struct loom_render_info *render_iter = NULL;
	struct loom_attr_info *iter = NULL;

	if(!ofp_enable)
		return;
	if (!ofp_is_overload)
		return;

		// iterate all loom tasks
	hlist_for_each_entry(render_iter, loom_get_render_list(), render_hlist) {
		hlist_for_each_entry(iter, &render_iter->active_list, hlist) {
#if IS_ENABLED(CONFIG_MTK_TASK_TURBO)
			int cpuid;
			int ret = 0;

			if (!iter->cmask_set || !iter->is_exclusive)
				continue;

			// reset cpu dedicated
			ret = loom_ctask_cpu_dedicated(iter->pid, -1);
			if (ret < 0)
				continue;
			cpuid = cpumask_to_cpu_id(iter->cmask_set);
			loom_notify_dedicated(cpuid, 0);
			iter->is_exclusive = 0;
			iter->cmask_set = 0;
			// degenerate to normal affinity
			if (iter->cpu_mask != LOOM_DEFAULT_VALUE) {
				ret = loom_sched_setaffinity(iter->pid, iter->cpu_mask);
				if (ret >= 0)
					iter->cmask_set = iter->cpu_mask;
			}
#endif
		}
	}
}

static void enable_cpu_loading_timer(void)
{
	ktime_t ktime;

	if (!ofp_enable)
		return;
	ktime = ktime_set(0, ofp_polling_ms * 1000000);
	hrtimer_start(&loom_ofp_hrt, ktime, HRTIMER_MODE_REL);
}

static void disable_cpu_loading_timer(void)
{
	hrtimer_cancel(&loom_ofp_hrt);
}

static void notify_cpu_loading_update(struct work_struct *work)
{
	loom_render_lock();
	loom_update_cpu_loading();
	loom_cpu_ofp();
	loom_render_unlock();
	enable_cpu_loading_timer();
}

static enum hrtimer_restart loom_ofp_hrt_timeout(struct hrtimer *timer)
{
	if (loom_ofp_WorkQueue)
		queue_work(loom_ofp_WorkQueue, &loom_ofp_work);
	return HRTIMER_NORESTART;
}

static void loom_cpuload_init(struct cpu_info *cpu_load_info, int num)
{
	int i;

	if (!cpu_load_info)
		return;
	for (i = 0; i < num; i++) {
		cpu_load_info[i].prev_wall_time = 0;
		cpu_load_info[i].prev_idle_time = 0;
		cpu_load_info[i].cur_wall_time = 0;
		cpu_load_info[i].cur_idle_time = 0;
		cpu_load_info[i].cur_loading = 0;
		cpu_load_info[i].is_isolated = 0;
	}
}

#define OFP_SYSFS_READ(name, variable); \
static ssize_t name##_show(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		char *buf) \
{ \
	int arg = -1; \
\
	loom_render_lock(); \
	arg = (variable); \
	loom_render_unlock(); \
	return scnprintf(buf, PAGE_SIZE, "%d\n", arg); \
}

#define OFP_SYSFS_WRITE_VALUE(name, variable, min, max); \
static ssize_t name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		const char *buf, size_t count) \
{ \
	char *acBuffer = NULL; \
	int arg; \
\
	acBuffer = kcalloc(FI_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL); \
	if (!acBuffer) \
		goto out; \
\
	if ((count > 0) && (count < FI_SYSFS_MAX_BUFF_SIZE)) { \
		if (scnprintf(acBuffer, FI_SYSFS_MAX_BUFF_SIZE, "%s", buf)) { \
			if (kstrtoint(acBuffer, 0, &arg) == 0) { \
				if (arg >= (min) && arg <= (max)) { \
					loom_render_lock(); \
					(variable) = arg; \
					loom_render_unlock(); \
				} \
			} \
		} \
	} \
\
out: \
	kfree(acBuffer); \
	return count; \
}

OFP_SYSFS_READ(ofp_overload_th, ofp_overload_th);
OFP_SYSFS_WRITE_VALUE(ofp_overload_th, ofp_overload_th, 0, 100);
static KOBJ_ATTR_RW(ofp_overload_th);

OFP_SYSFS_READ(ofp_light_th, ofp_light_th);
OFP_SYSFS_WRITE_VALUE(ofp_light_th, ofp_light_th, 0, 100);
static KOBJ_ATTR_RW(ofp_light_th);

OFP_SYSFS_READ(over_trigger_cnt, over_trigger_cnt);
OFP_SYSFS_WRITE_VALUE(over_trigger_cnt, over_trigger_cnt, 1, 10);
static KOBJ_ATTR_RW(over_trigger_cnt);

OFP_SYSFS_READ(light_trigger_cnt, light_trigger_cnt);
OFP_SYSFS_WRITE_VALUE(light_trigger_cnt, over_trigger_cnt, 1, 10);
static KOBJ_ATTR_RW(light_trigger_cnt);

/* OFP currently use loom_render_lock to protect all data */
static ssize_t ofp_enable_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int arg = -1;

	loom_render_lock();
	arg = ofp_enable;
	loom_render_unlock();
	return scnprintf(buf, PAGE_SIZE, "%d\n", arg);
}

static ssize_t ofp_enable_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FI_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;
	if ((count > 0) && (count < FI_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FI_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) != 0)
				goto out;
			loom_render_lock();
			if (arg == 0 && ofp_enable == 1) { // ofp switch off
				ofp_enable = 0;
				ofp_is_overload = 0;
				ofp_over_cnt = 0;
				ofp_light_cnt = 0;
				loom_cpuload_init(cpu_load_info, loom_cpu_num);
				disable_cpu_loading_timer();
			} else if (arg == 1 && ofp_enable == 0) {	// ofp switch_on
				ofp_enable = 1;
				enable_cpu_loading_timer();
			}
			loom_render_unlock();
		}
	}
out:
	kfree(acBuffer);
	return count;
}
static KOBJ_ATTR_RW(ofp_enable);

void loom_ofp_exit(void)
{
	loom_free(cpu_load_info);
	// need destroy workqueue?
	destroy_workqueue(loom_ofp_WorkQueue);
	game_sysfs_remove_file(loom_kobj, &kobj_attr_ofp_enable);
	game_sysfs_remove_file(loom_kobj, &kobj_attr_ofp_overload_th);
	game_sysfs_remove_file(loom_kobj, &kobj_attr_ofp_light_th);
	game_sysfs_remove_file(loom_kobj, &kobj_attr_over_trigger_cnt);
	game_sysfs_remove_file(loom_kobj, &kobj_attr_light_trigger_cnt);
}

void loom_ofp_init(void)
{
	loom_ofp_WorkQueue = create_singlethread_workqueue("loom_ofp_wq");
	if (!loom_ofp_WorkQueue)
		return;
	hrtimer_init(&loom_ofp_hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	loom_ofp_hrt.function = loom_ofp_hrt_timeout;
	loom_cpu_num = num_possible_cpus();
	if (loom_cpu_num <= 0)
		goto FAIL;
	cpu_load_info = loom_calloc(loom_cpu_num, sizeof(struct cpu_info));
	if (!cpu_load_info)
		goto FAIL;
	loom_cpuload_init(cpu_load_info, loom_cpu_num);
	ofp_polling_ms = 4;
	ofp_overload_th = 95;
	ofp_light_th = 85;
	over_trigger_cnt = 1;
	light_trigger_cnt = 2;
	if (!game_get_sysfs_dir(&loom_kobj)) {
		game_sysfs_create_file(loom_kobj, &kobj_attr_ofp_enable);
		game_sysfs_create_file(loom_kobj, &kobj_attr_ofp_overload_th);
		game_sysfs_create_file(loom_kobj, &kobj_attr_ofp_light_th);
		game_sysfs_create_file(loom_kobj, &kobj_attr_over_trigger_cnt);
		game_sysfs_create_file(loom_kobj, &kobj_attr_light_trigger_cnt);
	}
	enable_cpu_loading_timer();
	return;
FAIL:
	destroy_workqueue(loom_ofp_WorkQueue);
}
