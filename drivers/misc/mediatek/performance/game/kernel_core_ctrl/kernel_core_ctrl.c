// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <trace/events/sched.h>
#include <trace/events/task.h>
#include <trace/hooks/sched.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include "common.h"
#include "eas/eas_plus.h"
#include "kernel_core_ctrl.h"
#include "game_sysfs.h"
#include "game.h"

# define FORCE_MODE_ON 1
# define FORCE_MODE_OFF 0

static int kernel_core_ctrl_force_mode;
static DEFINE_MUTEX(kernel_core_ctrl_mutex);

static struct kobject *kernel_core_ctrl_kobj;

static ssize_t kernel_core_ctrl_force_mode_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int arg = -1;

	mutex_lock(&kernel_core_ctrl_mutex);
	arg = kernel_core_ctrl_force_mode;
	mutex_unlock(&kernel_core_ctrl_mutex);

	return scnprintf(buf, PAGE_SIZE, "%d\n", arg);
}

static ssize_t kernel_core_ctrl_force_mode_store(struct kobject *kobj,
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

			if (arg == 0 || arg == 1) {
				if (arg == 0) {
					mutex_lock(&kernel_core_ctrl_mutex);
					kernel_core_ctrl_force_mode = FORCE_MODE_OFF;
					mutex_unlock(&kernel_core_ctrl_mutex);
				} else {
					mutex_lock(&kernel_core_ctrl_mutex);
					kernel_core_ctrl_force_mode = FORCE_MODE_ON;
					mutex_unlock(&kernel_core_ctrl_mutex);
				}
			}
		}
	}
out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(kernel_core_ctrl_force_mode);

int set_cpus_allowed_ptr_by_kernel(struct task_struct *p, const struct cpumask *new_mask)
{
	struct cpumask *kernel_allowed_mask;
	int ret;

	if (!p)
		return -EINVAL;
	kernel_allowed_mask = &((struct mtk_task *) android_task_vendor_data(p))->kernel_allowed_mask;
	cpumask_copy(kernel_allowed_mask, new_mask);
	ret = set_cpus_allowed_ptr(p, new_mask);
	return ret;
}

static void mtk_set_cpus_allowed_ptr(void *data, struct task_struct *p,
	struct affinity_context *ctx, bool *skip_user_ptr)
{
	struct cpumask *kernel_allowed_mask = &((struct mtk_task *) android_task_vendor_data(p))->kernel_allowed_mask;
	struct rq_flags rf;
	struct rq *rq = task_rq_lock(p, &rf);
	cpumask_t user_mask;

	// not set or invalid cpu mask
	if (cpumask_empty(kernel_allowed_mask))
		goto out;

	cpumask_copy(&user_mask, ctx->new_mask);

	// if force mode is set, SCA_USER would be ignored
	unsigned int condition_mask = (kernel_core_ctrl_force_mode == FORCE_MODE_ON)
		? (SCA_MIGRATE_ENABLE | SCA_MIGRATE_DISABLE)
		: (SCA_USER | SCA_MIGRATE_ENABLE | SCA_MIGRATE_DISABLE);

	if (p->user_cpus_ptr &&
		!(ctx->flags & (condition_mask))) {
		*skip_user_ptr = true;
		cpumask_copy(rq->scratch_mask, kernel_allowed_mask);
		ctx->new_mask = rq->scratch_mask;
	}
	if (p->user_cpus_ptr && !cpumask_empty(kernel_allowed_mask)){
		game_print_trace(
		"kernel_core_ctrl: pid = %d, skip_user = %d, user_mask = 0x%x, kernel_allowed_mask = 0x%x, new_mask = 0x%x",
		p->pid,
		*skip_user_ptr,
		cpumask_bits(&user_mask)[0],
		cpumask_bits(kernel_allowed_mask)[0],
		cpumask_bits(ctx->new_mask)[0]);
	}

out:
	task_rq_unlock(rq, p, &rf);
}

void kernel_core_ctrl_exit(void)
{
	set_cpus_allowed_ptr_by_kernel_fp = NULL;
	loom_set_cpus_allowed_ptr_by_kernel_fp = NULL;

	mutex_lock(&kernel_core_ctrl_mutex);
	kernel_core_ctrl_force_mode = FORCE_MODE_OFF;
	mutex_unlock(&kernel_core_ctrl_mutex);

	game_sysfs_remove_file(kernel_core_ctrl_kobj, &kobj_attr_kernel_core_ctrl_force_mode);
}

int kernel_core_ctrl_init(void)
{
	int ret = 0;

	ret = register_trace_android_rvh_set_cpus_allowed_ptr(mtk_set_cpus_allowed_ptr, NULL);
	if (ret)
		pr_info("register mtk_set_cpus_allowed_ptr hooks failed, returned %d\n", ret);

	set_cpus_allowed_ptr_by_kernel_fp = &set_cpus_allowed_ptr_by_kernel;
	loom_set_cpus_allowed_ptr_by_kernel_fp = &set_cpus_allowed_ptr_by_kernel;

	if (!game_get_sysfs_dir(&kernel_core_ctrl_kobj))
		game_sysfs_create_file(kernel_core_ctrl_kobj, &kobj_attr_kernel_core_ctrl_force_mode);

	return ret;
}
