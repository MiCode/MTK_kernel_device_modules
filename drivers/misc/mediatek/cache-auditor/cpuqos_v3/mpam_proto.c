// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cgroup.h>
#include <linux/slab.h>
#include <linux/task_work.h>
#include <linux/sched.h>
#include <linux/percpu-defs.h>
#include <trace/events/task.h>
#include <linux/platform_device.h>

#include <trace/hooks/fpsimd.h>
#include <trace/hooks/cgroup.h>
#include <sched/sched.h>
#include "cpuqos_v3.h"
#include "cpuqos_sys_common.h"

#if IS_ENABLED(CONFIG_MTK_SLBC)
#include <mtk_slbc_sram.h>
#else
#define SLC_SYSRAM_BASE         0x00113E00
static void __iomem *sram_base_addr;
#endif

#define CREATE_TRACE_POINTS
#include <cpuqos_v3_trace.h>
#undef CREATE_TRACE_POINTS
#undef TRACE_INCLUDE_PATH

#define CREATE_TRACE_POINTS
#include <met_mpam_events.h>
#undef CREATE_TRACE_POINTS
#undef TRACE_INCLUDE_PATH

#define CPUQOS_L3CTL_M_OFS      0x84
#define SLC_CPU_DEBUG0_R_OFS    0x88
#define SLC_CPU_DEBUG1_R_OFS    0x8C
#define SLC_SRAM_SIZE           0x100

#define TAG "cpuqos"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jing-Ting Wu");

static int cpuqos_subsys_id = cpu_cgrp_id;
static struct device_node *node;
static int plat_enable;
static int q_pid = -1;
static int boot_complete;
/* For ftrace */
static void cpu_qos_handler(struct work_struct *work);
static int cpu_qos_track_enable;
static int cpu_qos_delay;
static struct workqueue_struct *cpuqos_workq;
static DECLARE_DELAYED_WORK(cpu_qos_tracker, cpu_qos_handler);

/*
 * cgroup path -> PARTID map
 *
 * Could be made better with cgroupv2: we could have some files that userspace
 * could write into which would give us a {path, PARTID} pair. We could then
 * translate the path to a cgroup with cgroup_get_from_path(), and save the
 * css->id mapping then.
 */
static char *mpam_path_partid_map[] = {
	"/",
	"/foreground",
	"/background",
	"/top-app",
	"/rt",
	"/system",
	"/system-background"
};

/*
 * cgroup css->id -> PARTID cache
 *
 * Not sure how stable those IDs are supposed to be. If we are supposed to
 * support cgroups being deleted, we may need more hooks to cache that.
 */
static int mpam_css_partid_map[50] = { [0 ... 49] = -1 };

/*
 * group number by mpam_path_partid_map -> css->id
 *
 */
static int mpam_group_css_map[50] = { [0 ... 49] = -1 };

/* The MPAM0_EL1.PARTID_D in use by a given CPU */
static DEFINE_PER_CPU(int, mpam_local_partid);

enum perf_mode {
	AGGRESSIVE,
	BALANCE,
	CONSERVATIVE,
	DISABLE
};

enum partid_grp {
	DEF_PARTID,
	SECURE_PARTID,
	CT_PARTID,
	NCT_PARTID
};

enum partid_rank {
	GROUP_RANK,
	TASK_RANK
};

static enum perf_mode cpuqos_perf_mode = DISABLE;

int task_curr_clone(const struct task_struct *p)
{
	return cpu_curr(task_cpu(p)) == p;
}

void write_mpam_partid(int partid)
{
	switch (partid) {
	case DEF_PARTID:
		asm volatile (
			"mrs x0, s3_0_c10_c5_1\n\t"
			"bic x0, x0, #(0xffffffffffff)\n\t"
			"msr s3_0_c10_c5_1, x0\n\t"
			"mrs x0, s3_0_c10_c5_0\n\t"
			"bic x0, x0, #(0xffffffffffff)\n\t"
			"msr s3_0_c10_c5_0, x0\n\t"
			: : : "memory");
		break;
	case SECURE_PARTID:
		asm volatile (
			"mrs x0, s3_0_c10_c5_1\n\t"
			"bic x0, x0, #(0xffffffffffff)\n\t"
			"orr x0, x0, #(0x1<<16)\n\t"
			"orr x0, x0, #(0x2)\n\t"
			"msr s3_0_c10_c5_1, x0\n\t"
			"mrs x0, s3_0_c10_c5_0\n\t"
			"bic x0, x0, #(0xffffffffffff)\n\t"
			"orr x0, x0, #(0x1<<16)\n\t"
			"orr x0, x0, #(0x2)\n\t"
			"msr s3_0_c10_c5_0, x0\n\t"
			: : : "memory");
		break;
	case CT_PARTID:
		asm volatile (
			"mrs x0, s3_0_c10_c5_1\n\t"
			"bic x0, x0, #(0xffffffffffff)\n\t"
			"orr x0, x0, #(0x2<<16)\n\t"
			"orr x0, x0, #(0x2)\n\t"
			"msr s3_0_c10_c5_1, x0\n\t"
			"mrs x0, s3_0_c10_c5_0\n\t"
			"bic x0, x0, #(0xffffffffffff)\n\t"
			"orr x0, x0, #(0x2<<16)\n\t"
			"orr x0, x0, #(0x2)\n\t"
			"msr s3_0_c10_c5_0, x0\n\t"
			: : : "memory");
		break;
	case NCT_PARTID:
		asm volatile (
			"mrs x0, s3_0_c10_c5_1\n\t"
			"bic x0, x0, #(0xffffffffffff)\n\t"
			"orr x0, x0, #(0x3<<16)\n\t"
			"orr x0, x0, #(0x2)\n\t"
			"msr s3_0_c10_c5_1, x0\n\t"
			"mrs x0, s3_0_c10_c5_0\n\t"
			"bic x0, x0, #(0xffffffffffff)\n\t"
			"orr x0, x0, #(0x3<<16)\n\t"
			"orr x0, x0, #(0x2)\n\t"
			"msr s3_0_c10_c5_0, x0\n\t"
			: : : "memory");
		break;
	}
	return;

}

unsigned int get_task_partid(struct task_struct *p)
{
	unsigned int partid;

	partid = (unsigned int)p->android_vendor_data1[1];

	return partid;
}

unsigned int get_task_rank(struct task_struct *p)
{
	unsigned int rank;

	rank = (unsigned int)p->android_vendor_data1[2];

	return rank;
}

/* Get the css:partid mapping */
static int mpam_map_css_partid(struct cgroup_subsys_state *css)
{
	int partid;

	if (!css)
		goto no_match;

	/*
	 * The map is stable post init so no risk of two concurrent tasks
	 * cobbling each other
	 */
	partid = READ_ONCE(mpam_css_partid_map[css->id]);
	if (partid >= 0)
		return partid;

no_match:
	/* No match, use sensible default */
	partid = NCT_PARTID;

	return partid;
}

/*
 * Get the task_struct:partid mapping
 * This is the place to add special logic for a task-specific (rather than
 * cgroup-wide) PARTID.
 */
static int mpam_map_task_partid(struct task_struct *p)
{
	struct cgroup_subsys_state *css;
	int partid;
	int rank = get_task_rank(p);

	if (cpuqos_perf_mode == DISABLE) {
		/* disable mode */
		partid = DEF_PARTID;
		goto out;
	}

	if (rank == TASK_RANK) {
		/* task rank */
		partid = get_task_partid(p);
		goto out;
	} else {
		/* group rank */
		rcu_read_lock();
		css = task_css(p, cpuqos_subsys_id);
		partid = mpam_map_css_partid(css);
		rcu_read_unlock();
		goto out;
	}
out:
	return partid;
}

/*
 * Write the PARTID to use on the local CPU.
 */
static void mpam_write_partid(int partid)
{
	this_cpu_write(mpam_local_partid, partid);

	/* Write to e.g. MPAM0_EL1.PARTID_D here */
	write_mpam_partid(this_cpu_read(mpam_local_partid));
}

/*
 * Sync @p's associated PARTID with this CPU's register.
 */
static void mpam_sync_task(struct task_struct *p)
{
	struct cgroup_subsys_state *css;
	int old_partid = this_cpu_read(mpam_local_partid);
	u64 v1, v2;

	rcu_read_lock();
	css = task_css(p, cpuqos_subsys_id);
	rcu_read_unlock();

	mpam_write_partid(mpam_map_task_partid(p));

	__asm__ volatile ("mrs %0, s3_0_c10_c5_1" : "=r" (v1));
	__asm__ volatile ("mrs %0, s3_0_c10_c5_0" : "=r" (v2));

	trace_cpuqos_cpu_partid(smp_processor_id(), p->pid,
				css->id, old_partid,
				this_cpu_read(mpam_local_partid),
				v1, v2,
				get_task_rank(p),
				cpuqos_perf_mode);

}

/*
 * Same as mpam_sync_task(), with a pre-filter for the current task.
 */
static void mpam_sync_current(void *task)
{
	int prev_partid;
	int next_partid;

	if (task && task != current)
		return;

	if (trace_MPAM_CT_task_enter_enabled() && trace_MPAM_CT_task_leave_enabled()) {
		prev_partid = this_cpu_read(mpam_local_partid);
		next_partid = mpam_map_task_partid(current);

		if ((prev_partid == CT_PARTID) && (next_partid == NCT_PARTID))
			trace_MPAM_CT_task_leave(current->pid, current->comm);

		if (next_partid == CT_PARTID)
			trace_MPAM_CT_task_enter(current->pid, current->comm);
	}

	mpam_sync_task(current);
}

/*
 * Same as mpam_sync_current(), with an explicit mb for partid mapping changes.
 * Note: technically not required for arm64+GIC since we get explicit barriers
 * when raising and handling an IPI. See:
 * f86c4fbd930f ("irqchip/gic: Ensure ordering between read of INTACK and shared data")
 */
static void mpam_sync_current_mb(void *task)
{
	if (task && task != current)
		return;

	/* Pairs with smp_wmb() following mpam_cgroup_partid_map[] updates */
	smp_rmb();
	mpam_sync_task(current);
}

static void mpam_kick_task(struct task_struct *p, int partid)
{
	if (partid >= 0)
		p->android_vendor_data1[1] = partid;

	/*
	 * If @p is no longer on the task_cpu(p) we see here when the smp_call
	 * actually runs, then it had a context switch, so it doesn't need the
	 * explicit update - no need to chase after it.
	 */
	if (task_curr_clone(p))
		smp_call_function_single(task_cpu(p), mpam_sync_current, p, 1);
}

/*
 * Set group is critical task(CT)/non-critical task(NCT)
 * group_id: depend on mpam_path_partid_map list
 *           0: "/",
 *           1: "/foreground"
 *           2: "/background"
 *	     3: "/top-app"
 *           4: "/rt",
 *           5: "/system",
 *           6: "/system-background"
 * set: if true, set group is CT;
 *      if false, set group is NCT.
 * Return: 0: success,
 *        -1: perf mode is disable / group_id is not exist.
 */
int set_ct_group(int group_id, bool set)
{
	int css_id = -1;
	int old_partid;
	int new_partid;

	if ((group_id >= ARRAY_SIZE(mpam_path_partid_map)) || (group_id < 0) ||
		(cpuqos_perf_mode == DISABLE) || (plat_enable == 0))
		return -1;

	css_id = mpam_group_css_map[group_id];
	if (css_id < 0)
		return -1;

	old_partid = mpam_css_partid_map[css_id];

	if (set)
		new_partid = CT_PARTID;
	else
		new_partid = NCT_PARTID;

	trace_cpuqos_set_ct_group(group_id, css_id, set,
				old_partid, new_partid,
				cpuqos_perf_mode);

	if (new_partid != old_partid) {
		mpam_css_partid_map[css_id] = new_partid;

		/*
		 * Ensure the partid map update is visible before kicking the CPUs.
		 * Pairs with smp_rmb() in mpam_sync_current_mb().
		 */
		smp_wmb();
		smp_call_function(mpam_sync_current_mb, NULL, 1);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(set_ct_group);

/*
 * Set task is critical task(CT) or use its group partid
 * pid: task pid
 * set: if true, set task is CT(ignore group setting);
 *      if false, set task use its group partid.
 * Return: 0: success,
	   -1: perf mode is disable / p is not exist.
 */
int set_ct_task(int pid, bool set)
{
	struct task_struct *p;
	struct cgroup_subsys_state *css;
	int old_partid;
	int new_partid;

	if (cpuqos_perf_mode == DISABLE || (plat_enable == 0))
		return -1;

	rcu_read_lock();
	p = find_task_by_vpid(pid);

	if (p)
		get_task_struct(p);
	rcu_read_unlock();

	if (!p)
		return -1;

	rcu_read_lock();
	css = task_css(p, cpuqos_subsys_id);
	rcu_read_unlock();

	old_partid = mpam_map_task_partid(p); /* before rank change */

	if (set) { /* set task is critical task */
		p->android_vendor_data1[2] = TASK_RANK;
		if (mpam_map_task_partid(p) != CT_PARTID)
			new_partid = CT_PARTID;
		else
			new_partid = old_partid;
	} else { /* reset to group setting */
		p->android_vendor_data1[2] = GROUP_RANK;
		new_partid = mpam_map_task_partid(p); /* after rank change */
	}

	trace_cpuqos_set_ct_task(p->pid, css->id, set,
				old_partid, new_partid,
				get_task_rank(p), cpuqos_perf_mode);

	if (new_partid != old_partid)
		mpam_kick_task(p, new_partid);

	put_task_struct(p);

	return 0;
}
EXPORT_SYMBOL_GPL(set_ct_task);

int set_cpuqos_mode(int mode)
{
	if (mode > DISABLE || mode < AGGRESSIVE || (plat_enable == 0))
		return -1;

	switch (mode) {
	case AGGRESSIVE:
		cpuqos_perf_mode = AGGRESSIVE;
		break;
	case BALANCE:
		cpuqos_perf_mode = BALANCE;
		break;
	case CONSERVATIVE:
		cpuqos_perf_mode = CONSERVATIVE;
		break;
	case DISABLE:
		cpuqos_perf_mode = DISABLE;
		break;
	}

	trace_cpuqos_set_cpuqos_mode(cpuqos_perf_mode);
#if IS_ENABLED(CONFIG_MTK_SLBC)
	slbc_sram_write(CPUQOS_MODE, cpuqos_perf_mode);
#else
	pr_info("Set to SLBC fail: config is disable\n");
	sram_base_addr = ioremap(SLC_SYSRAM_BASE, SLC_SRAM_SIZE);
	iowrite32(cpuqos_perf_mode, (sram_base_addr + CPUQOS_MODE));
#endif

	/*
	 * Ensure the partid map update is visible before kicking the CPUs.
	 * Pairs with smp_rmb() in mpam_sync_current_mb().
	 */
	smp_wmb();
	smp_call_function(mpam_sync_current_mb, NULL, 1);

	return 0;
}
EXPORT_SYMBOL_GPL(set_cpuqos_mode);

static void cpuqos_tracer(void)
{
	unsigned int csize = 0;

#if IS_ENABLED(CONFIG_MTK_SLBC)
	csize = slbc_sram_read(SLC_CPU_DEBUG1_R_OFS);
#else
	sram_base_addr = ioremap(SLC_SYSRAM_BASE, SLC_SRAM_SIZE);

	if (!sram_base_addr) {
		pr_info("Remap SLC SYSRAM failed\n");
		return -EIO;
	}

	csize = ioread32(sram_base_addr + SLC_CPU_DEBUG1_R_OFS);
#endif
	csize &= 0xf;

	if (!boot_complete) {
		pr_info("cpuqos_mode=%d, slices=%u, cache way mode=%u",
			cpuqos_perf_mode,
			(csize & 0xC)>>2, csize & 0x3);
	}

	trace_cpuqos_debug_info(cpuqos_perf_mode,
			(csize & 0xC)>>2, csize & 0x3);
}

static void cpu_qos_handler(struct work_struct *work)
{
	if (cpu_qos_track_enable) {
		cpuqos_tracer();
		queue_delayed_work(cpuqos_workq, &cpu_qos_tracker,
				msecs_to_jiffies(cpu_qos_delay));
	}
}

static ssize_t show_cpuqos_status(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;
	unsigned int csize = 0, ctnct = 0;

#if IS_ENABLED(CONFIG_MTK_SLBC)
	csize = slbc_sram_read(SLC_CPU_DEBUG1_R_OFS);
	ctnct = slbc_sram_read(SLC_CPU_DEBUG0_R_OFS);
#else
	sram_base_addr = ioremap(SLC_SYSRAM_BASE, SLC_SRAM_SIZE);

	if (!sram_base_addr) {
		pr_info("Remap SLC SYSRAM failed\n");
		return -EIO;
	}

	csize = ioread32(sram_base_addr + SLC_CPU_DEBUG1_R_OFS);
	ctnct = ioread32(sram_base_addr + SLC_CPU_DEBUG0_R_OFS);
#endif
	csize &= 0xf;
	ctnct &= 0xfff;

	len += snprintf(buf+len, max_len-len,
			"CPUQoS CPPD setting = %d/%d, L3CC total/ct/nct = %d/%d/%d\n",
			(csize & 0xC)>>2, csize & 0x3, (ctnct & 0xf00)>>8,
			(ctnct & 0xf0)>>4, (ctnct & 0xf));

	return len;
}

static ssize_t set_cache_size(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *ubuf,
		size_t cnt)
{
	unsigned int data = 0, mode = 0, slice = 0, portion = 0;

#if !IS_ENABLED(CONFIG_MTK_SLBC)
	sram_base_addr = ioremap(SLC_SYSRAM_BASE, SLC_SRAM_SIZE);

	if (!sram_base_addr) {
		pr_info("Remap SLC SYSRAM failed\n");
		return -EIO;
	}
#endif

	if (sscanf(ubuf, "%d:%d:%d", &mode, &slice, &portion) == 3) {
		data = (mode << 4) | (slice << 2) | (portion);
#if IS_ENABLED(CONFIG_MTK_SLBC)
		slbc_sram_write(CPUQOS_L3CTL_M_OFS, data);
#else
		iowrite32(data, (sram_base_addr + CPUQOS_L3CTL_M_OFS));
#endif
	}

	return cnt;
}

static ssize_t show_cache_size(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;
	unsigned int data = 0, mode = 0, slice = 0, portion = 0;

#if IS_ENABLED(CONFIG_MTK_SLBC)
	data = slbc_sram_read(CPUQOS_L3CTL_M_OFS);
#else
	sram_base_addr = ioremap(SLC_SYSRAM_BASE, SLC_SRAM_SIZE);

	if (!sram_base_addr) {
		pr_info("Remap SLC SYSRAM failed\n");
		return -EIO;
	}
	data = ioread32(sram_base_addr + CPUQOS_L3CTL_M_OFS);
#endif
	mode = (data & 0x10) >> 4;
	slice = (data & 0xc) >> 2;
	portion = data & 0x3;

	len += snprintf(buf+len, max_len-len,
			"raw = %x, mode = %d, slice = %d, portion = %d\n",
			data, mode, slice, portion);

	return len;
}

static ssize_t set_trace_enable(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *ubuf,
		size_t cnt)
{
	int enable = 0;

	if (!kstrtoint(ubuf, 10, &enable)) {
		if (enable) {
			cpu_qos_track_enable = 1;
			queue_delayed_work(cpuqos_workq, &cpu_qos_tracker,
					msecs_to_jiffies(cpu_qos_delay));
		} else
			cpu_qos_track_enable = 0;
	}
	return cnt;
}

static ssize_t show_trace_enable(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len += snprintf(buf+len, max_len-len,
			"CPU QoS trace enable:%d\n",
			cpu_qos_track_enable);

	return len;
}

static ssize_t set_boot_complete(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *ubuf,
		size_t cnt)
{
	int enable = 0;

	if (!kstrtoint(ubuf, 10, &enable)) {
		if (!boot_complete && (enable == 1)) {
			set_cpuqos_mode(BALANCE);
			boot_complete = 1;
			pr_info("cpuqos working!\n");
		}
	}

	return cnt;
}

struct kobj_attribute show_cpuqos_status_attr =
__ATTR(cpuqos_status_info, 0400, show_cpuqos_status, NULL);

struct kobj_attribute set_cache_size_attr =
__ATTR(cpuqos_set_cache_size, 0600, show_cache_size, set_cache_size);

struct kobj_attribute trace_enable_attr =
__ATTR(cpuqos_trace_enable, 0600, show_trace_enable, set_trace_enable);

struct kobj_attribute boot_complete_attr =
__ATTR(cpuqos_boot_complete, 0600, NULL, set_boot_complete);

static void mpam_hook_attach(void __always_unused *data,
			     struct cgroup_subsys *ss, struct cgroup_taskset *tset)
{
	struct cgroup_subsys_state *css;
	struct task_struct *p;

	if (ss->id != cpuqos_subsys_id)
		return;

	cgroup_taskset_first(tset, &css);
	cgroup_taskset_for_each(p, css, tset)
		mpam_kick_task(p, -1);
}

static void mpam_hook_switch(void __always_unused *data,
			     struct task_struct *prev, struct task_struct *next)
{
	int prev_partid;
	int next_partid;

	if (trace_MPAM_CT_task_enter_enabled() && trace_MPAM_CT_task_leave_enabled()) {
		prev_partid = this_cpu_read(mpam_local_partid);
		next_partid = mpam_map_task_partid(next);

		if (prev && (prev_partid == CT_PARTID))
			trace_MPAM_CT_task_leave(prev->pid, prev->comm);

		if (next && (next_partid == CT_PARTID))
			trace_MPAM_CT_task_enter(next->pid, next->comm);
	}

	mpam_sync_task(next);
}

static void mpam_task_newtask(void __always_unused *data,
				struct task_struct *p, unsigned long clone_flags)
{
	p->android_vendor_data1[1] = NCT_PARTID; /* partid */
	p->android_vendor_data1[2] = GROUP_RANK; /* rank */
}

/* Check if css' path matches any in mpam_path_partid_map and cache that */
static void __init __map_css_partid(struct cgroup_subsys_state *css, char *tmp, int pathlen)
{
	int i;

	cgroup_path(css->cgroup, tmp, pathlen);

	for (i = 0; i < ARRAY_SIZE(mpam_path_partid_map); i++) {
		if (!strcmp(mpam_path_partid_map[i], tmp)) {
			WRITE_ONCE(mpam_group_css_map[i], css->id);

			if (cpuqos_perf_mode == DISABLE)
				WRITE_ONCE(mpam_css_partid_map[css->id], DEF_PARTID);

			/* init group_partid */
			if (!strcmp(mpam_path_partid_map[i], "/top-app"))
				WRITE_ONCE(mpam_css_partid_map[css->id], CT_PARTID);
			else
				WRITE_ONCE(mpam_css_partid_map[css->id], NCT_PARTID);

			pr_info("group_id=%d, path=%s, mpam_path=%s, css_id=%d, group_css=%d, partid_map=%d\n",
				i, tmp, mpam_path_partid_map[i], css->id, mpam_group_css_map[i],
				mpam_css_partid_map[css->id]);
		}
	}
}

/* Recursive DFS */
static void __init __map_css_children(struct cgroup_subsys_state *css, char *tmp, int pathlen)
{
	struct cgroup_subsys_state *child;

	list_for_each_entry_rcu(child, &css->children, sibling) {
		if (!child || !child->cgroup)
			continue;

		__map_css_partid(child, tmp, pathlen);
		__map_css_children(child, tmp, pathlen);
	}
}

static int __init mpam_init_cgroup_partid_map(void)
{
	struct cgroup_subsys_state *css;
	struct cgroup *cgroup;
	char buf[50];
	int ret = 0;

	rcu_read_lock();
	/*
	 * cgroup_get_from_path() would be much cleaner, but that seems to be v2
	 * only. Getting current's cgroup is only a means to get a cgroup handle,
	 * use that to get to the root. Clearly doesn't work if several roots
	 * are involved.
	 */
	cgroup = task_cgroup(current, cpuqos_subsys_id);
	if (IS_ERR(cgroup)) {
		ret = PTR_ERR(cgroup);
		goto out_unlock;
	}

	cgroup = &cgroup->root->cgrp;
	css = rcu_dereference(cgroup->subsys[cpuqos_subsys_id]);
	if (IS_ERR_OR_NULL(css)) {
		ret = -ENOENT;
		goto out_unlock;
	}

	__map_css_partid(css, buf, 50);
	__map_css_children(css, buf, 50);

out_unlock:
	rcu_read_unlock();
	return ret;
}

static int platform_cpuqos_v3_probe(struct platform_device *pdev)
{
	int ret = 0, retval = 0;

	node = pdev->dev.of_node;

	ret = of_property_read_u32(node,
			"enable", &retval);
	if (!ret)
		plat_enable = retval;
	else
		pr_info("%s unable to get plat_enable\n", __func__);

	pr_info("cpuqos_v3 plat_enable=%d\n", plat_enable);

	return 0;
}

static const struct of_device_id platform_cpuqos_v3_of_match[] = {
	{ .compatible = "mediatek,cpuqos-v3", },
	{},
};

static const struct platform_device_id platform_cpuqos_v3_id_table[] = {
	{ "cpuqos-v3", 0},
	{ },
};

static ssize_t show_l3m_status(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;
	int css_id = -1;
	struct task_struct *p;
	int i;

	len += snprintf(buf+len, max_len-len,
			"L3 manage perf mode = %d, CT task group = ", cpuqos_perf_mode);

	for (i = 0; i < ARRAY_SIZE(mpam_path_partid_map); i++) {
		css_id = mpam_group_css_map[i];
		if (css_id < 0)
			continue;
		if (mpam_css_partid_map[css_id] == CT_PARTID)
			len += snprintf(buf+len, max_len-len, "%s ", mpam_path_partid_map[i]);
	}

	if (q_pid > -1) {
		rcu_read_lock();
		p = find_task_by_vpid(q_pid);
		if (!p) {
			rcu_read_unlock();
			goto out;
		}

		get_task_struct(p);
		rcu_read_unlock();
		len += snprintf(buf+len, max_len-len, ", pid %d is %s",
				q_pid, (mpam_map_task_partid(p) ==
					CT_PARTID?"CT":"NCT"));
		put_task_struct(p);
	}

out:

	len += snprintf(buf+len, max_len-len, "\n");
	return len;
}

static ssize_t set_l3m_query_pid(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *ubuf,
		size_t cnt)
{

	int query_pid = 0;

	if (!kstrtoint(ubuf, 10, &query_pid))
		q_pid = query_pid;

	return cnt;
}

struct kobj_attribute show_L3m_status_attr =
__ATTR(l3m_status_info, 0600, show_l3m_status, set_l3m_query_pid);

static struct platform_driver mtk_platform_cpuqos_v3_driver = {
	.probe = platform_cpuqos_v3_probe,
	.driver = {
		.name = "cpuqos_v3",
		.owner = THIS_MODULE,
		.of_match_table = platform_cpuqos_v3_of_match,
	},
	.id_table = platform_cpuqos_v3_id_table,
};

void init_cpuqos_v3_platform(void)
{
	platform_driver_register(&mtk_platform_cpuqos_v3_driver);
}

void exit_cpuqos_v3_platform(void)
{
	plat_enable = 0;
	platform_driver_unregister(&mtk_platform_cpuqos_v3_driver);
}

static unsigned long cpuqos_ctl_copy_from_user(void *pvTo,
		const void __user *pvFrom, unsigned long ulBytes)
{
	if (access_ok(pvFrom, ulBytes))
		return __copy_from_user(pvTo, pvFrom, ulBytes);

	return ulBytes;
}

static int cpuqos_ctl_show(struct seq_file *m, void *v)
{
	return 0;
}

static int cpuqos_ctl_open(struct inode *inode, struct file *file)
{
	return single_open(file, cpuqos_ctl_show, inode->i_private);
}

static long cpuqos_ctl_ioctl_impl(struct file *filp,
		unsigned int cmd, unsigned long arg, void *pKM)
{
	int ret = 0;
	void __user *ubuf_cpuqos = (struct _CPUQOS_V3_PACKAGE *)arg;
	struct _CPUQOS_V3_PACKAGE msgKM_cpuqos = {0};

	switch (cmd) {
	case CPUQOS_V3_SET_CPUQOS_MODE:
		if (cpuqos_ctl_copy_from_user(&msgKM_cpuqos, ubuf_cpuqos,
				sizeof(struct _CPUQOS_V3_PACKAGE)))
			return -1;

		ret = set_cpuqos_mode(msgKM_cpuqos.mode);
		break;
	case CPUQOS_V3_SET_CT_TASK:
		if (cpuqos_ctl_copy_from_user(&msgKM_cpuqos, ubuf_cpuqos,
				sizeof(struct _CPUQOS_V3_PACKAGE)))
			return -1;

		ret = set_ct_task(msgKM_cpuqos.pid, msgKM_cpuqos.set_task);
		break;
	case CPUQOS_V3_SET_CT_GROUP:
		if (cpuqos_ctl_copy_from_user(&msgKM_cpuqos, ubuf_cpuqos,
				sizeof(struct _CPUQOS_V3_PACKAGE)))
			return -1;

		ret = set_ct_group(msgKM_cpuqos.group_id, msgKM_cpuqos.set_group);
		break;
	default:
		pr_info("%s: %s %d: unknown cmd %x\n",
			TAG, __FILE__, __LINE__, cmd);
		ret = -1;
		goto ret_ioctl;
	}

ret_ioctl:
	return ret;
}

static long cpuqos_ctl_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	return cpuqos_ctl_ioctl_impl(filp, cmd, arg, NULL);
}

static const struct proc_ops cpuqos_ctl_Fops = {
	.proc_ioctl = cpuqos_ctl_ioctl,
	.proc_compat_ioctl = cpuqos_ctl_ioctl,
	.proc_open = cpuqos_ctl_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int __init mpam_proto_init(void)
{
	int ret = 0;
	struct proc_dir_entry *pe, *parent;

	/* boot configuration */
	boot_complete = cpu_qos_track_enable = 0;
	cpu_qos_delay = 25;
	cpuqos_workq = NULL;

	init_cpuqos_v3_platform();
	if (!plat_enable) {
		pr_info("cpuqos_v3 is disable at this platform\n");
		goto out;
	}

	ret = init_cpuqos_common_sysfs();
	if (ret) {
		pr_info("init cpuqos sysfs failed\n");
		goto out;
	}

	/* Set DISABLE mode to initial mode */
	cpuqos_perf_mode = DISABLE;

	ret = mpam_init_cgroup_partid_map();
	if (ret) {
		pr_info("init cpuqos failed\n");
		goto out;
	}

	ret = register_trace_android_vh_cgroup_attach(mpam_hook_attach, NULL);
	if (ret) {
		pr_info("register android_vh_cgroup_attach failed\n");
		goto out;
	}

	ret = register_trace_android_vh_is_fpsimd_save(mpam_hook_switch, NULL);
	if (ret) {
		pr_info("register android_vh_is_fpsimd_save failed\n");
		goto out_attach;
	}

	ret = register_trace_task_newtask(mpam_task_newtask, NULL);
	if (ret) {
		pr_info("register trace_task_newtask failed\n");
		goto out_attach;
	}

	/*
	 * Ensure the cpuqos mode/partid map update is visible
	 * before kicking the CPUs.
	 */
	pr_info("init cpuqos mode = %d\n", cpuqos_perf_mode);
	ret = set_cpuqos_mode(cpuqos_perf_mode);
	if (ret) {
		pr_info("init set cpuqos mode failed\n");
		goto out_attach;
	}
	cpuqos_tracer();
	cpuqos_workq = create_workqueue("cpuqos_wq");

	/* init cpuqos ioctl */
	pr_info("%s: start to init cpuqos_ioctl driver\n", TAG);
	parent = proc_mkdir("cpuqosmgr", NULL);
	pe = proc_create("cpuqos_ioctl", 0664, parent, &cpuqos_ctl_Fops);
	if (!pe) {
		pr_info("%s: %s failed with %d\n", TAG,
				"Creating file node ", ret);
		ret = -ENOMEM;
		goto out;
	}

	return 0;

out_attach:
	unregister_trace_android_vh_cgroup_attach(mpam_hook_attach, NULL);
out:
	return ret;
}

static void mpam_reset_partid(void __always_unused *info)
{
	mpam_write_partid(DEF_PARTID);
}

static void __init mpam_proto_exit(void)
{
	if (cpuqos_workq)
		destroy_workqueue(cpuqos_workq);

	unregister_trace_android_vh_is_fpsimd_save(mpam_hook_switch, NULL);
	unregister_trace_android_vh_cgroup_attach(mpam_hook_attach, NULL);
	unregister_trace_task_newtask(mpam_task_newtask, NULL);

	smp_call_function(mpam_reset_partid, NULL, 1);
	cleanup_cpuqos_common_sysfs();
	exit_cpuqos_v3_platform();
}

module_init(mpam_proto_init);
module_exit(mpam_proto_exit);
