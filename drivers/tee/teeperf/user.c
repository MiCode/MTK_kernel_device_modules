// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/cdev.h>
#include <linux/cpufreq.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
//#include "group.h"
#include "user.h"

#define TEEPERF_MAX_GROUP_SIZE 1024
static int group_list[TEEPERF_MAX_GROUP_SIZE];
static unsigned int group_list_size;

static void teeperf_set_cpu_to_high_freq(int target_cpu, u32 high_freq,
	unsigned int freq_level_index)
{
	struct cpufreq_policy *policy;
	unsigned int index, max_index, min_index;

	if (target_cpu >= 8) {
		pr_info(PFX "invalid target cpu (%d)\n", target_cpu);
		return;
	}

	policy = cpufreq_cpu_get(target_cpu);
	if (policy == NULL) {
		pr_info(PFX "invalid policy, target cpu (%d)\n", target_cpu);
		return;
	}

	down_write(&policy->rwsem);
	max_index = 0;
	min_index = cpufreq_table_find_index_dl(policy, 0, false);
	if (high_freq) {
		/* set min_freq to selected freq */
		index = max_index + freq_level_index;
		if (index > min_index)
			index = min_index;
	} else {
		/* set min_freq to min freq */
		index = min_index;
	}
	policy->cpuinfo.min_freq = policy->freq_table[index].frequency;
	up_write(&policy->rwsem);
	cpufreq_cpu_put(policy);
	cpufreq_update_limits(target_cpu);
}

static void teeperf_set_cpu_group_to_high_freq(enum teeperf_cpu_group group,
	u32 high_freq)
{
	enum teeperf_cpu_map map = cpu_map;
	unsigned int freq_level_index = 0;
	int cpu;

	if (group == CPU_SUPER_GROUP) {
		freq_level_index = SUPER_CPU_FREQ_LEVEL_INDEX;
		if (map == CPU_4_3_1_MAP) {
			teeperf_set_cpu_to_high_freq(7, high_freq, freq_level_index);
		} else {
			for (cpu = 0; cpu < 8; cpu++)
				teeperf_set_cpu_to_high_freq(cpu, high_freq, 0);
		}
	} else if (group == CPU_BIG_GROUP) {
		freq_level_index = BIG_CPU_FREQ_LEVEL_INDEX;
		if (map == CPU_4_3_1_MAP) {
			teeperf_set_cpu_to_high_freq(4, high_freq, freq_level_index);
			teeperf_set_cpu_to_high_freq(5, high_freq, freq_level_index);
			teeperf_set_cpu_to_high_freq(6, high_freq, freq_level_index);
		} else if (map == CPU_6_2_MAP) {
			teeperf_set_cpu_to_high_freq(6, high_freq, freq_level_index);
			teeperf_set_cpu_to_high_freq(7, high_freq, freq_level_index);
		} else {
			for (cpu = 0; cpu < 8; cpu++)
				teeperf_set_cpu_to_high_freq(cpu, high_freq, 0);
		}
	} else if (group == CPU_LITTLE_GROUP) {
		freq_level_index = LITTLE_CPU_FREQ_LEVEL_INDEX;
		if (map == CPU_4_3_1_MAP) {
			teeperf_set_cpu_to_high_freq(0, high_freq, freq_level_index);
			teeperf_set_cpu_to_high_freq(1, high_freq, freq_level_index);
			teeperf_set_cpu_to_high_freq(2, high_freq, freq_level_index);
			teeperf_set_cpu_to_high_freq(3, high_freq, freq_level_index);
		} else if (map == CPU_6_2_MAP) {
			teeperf_set_cpu_to_high_freq(0, high_freq, freq_level_index);
			teeperf_set_cpu_to_high_freq(1, high_freq, freq_level_index);
			teeperf_set_cpu_to_high_freq(2, high_freq, freq_level_index);
			teeperf_set_cpu_to_high_freq(3, high_freq, freq_level_index);
			teeperf_set_cpu_to_high_freq(4, high_freq, freq_level_index);
			teeperf_set_cpu_to_high_freq(5, high_freq, freq_level_index);
		} else {
			for (cpu = 0; cpu < 8; cpu++)
				teeperf_set_cpu_to_high_freq(cpu, high_freq, 0);
		}
	} else {
		for (cpu = 0; cpu < 8; cpu++)
			teeperf_set_cpu_to_high_freq(cpu, high_freq, 0);
	}
}

static void teeperf_high_freq(enum teeperf_cpu_type type, u32 high_freq)
{
	teeperf_set_cpu_to_high_freq(TEE_CPU, high_freq, BIG_CPU_FREQ_LEVEL_INDEX);
	teeperf_set_cpu_group_to_high_freq(CPU_LITTLE_GROUP, high_freq);

	if (type == CPU_V9_TYPE)
		teeperf_set_cpu_group_to_high_freq(CPU_BIG_GROUP, high_freq);
	else if (type == CPU_V8_TYPE)
		teeperf_set_cpu_group_to_high_freq(CPU_LITTLE_GROUP, high_freq);
	else
		teeperf_set_cpu_group_to_high_freq(CPU_LITTLE_GROUP, high_freq);
}

static int teeperf_user_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int teeperf_user_release(struct inode *inode, struct file *file)
{
	return 0;
}

static inline int teeperf_ioctl_check_pointer(unsigned int cmd, int __user *uarg)
{
	int err = 0;

	err = !access_ok(uarg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	return 0;
}

static void teeperf_set_uclamp(bool enable)
{
	struct task_struct *task, *task_child;
	struct sched_attr attr = {};
	uint i;
	int ret = -1;

	attr.sched_policy = -1;
	attr.sched_flags = SCHED_FLAG_KEEP_ALL | SCHED_FLAG_UTIL_CLAMP | SCHED_FLAG_RESET_ON_FORK;
	attr.sched_util_max = -1;

	if (enable)
		attr.sched_util_min = cpu_uclamp_min;
	else
		attr.sched_util_min = -1;

	rcu_read_lock();
	for_each_process(task) {
		if (task != NULL && (
				!strcmp(task->comm, "c2@1.2-mediatek") ||
				!strcmp(task->comm, "mtk-vcodec-dec") ||
				!strcmp(task->comm, "vdec_ipi_recv") ||
				!strcmp(task->comm, "mtk-vcodec-enc") ||
				!strcmp(task->comm, "venc_ipi_recv") ||
				!strcmp(task->comm, "android.hardwar"))) {
			for_each_thread(task, task_child) {
				if (task_child != NULL) {
					if (group_list_size < TEEPERF_MAX_GROUP_SIZE) {
						group_list[group_list_size] = task_child->pid;
						group_list_size++;
						pr_info(PFX"put pid %d\n", task_child->pid);
					} else
						pr_info(PFX"put tid %d fail", task_child->pid);
				}
			}
		}
	}

	for(i = 0; i < group_list_size; i++) {
		struct task_struct *p;

		p = find_task_by_vpid(group_list[i]);
		if (likely(p)) {
			get_task_struct(p);
			attr.sched_policy = p->policy;
			if (p->policy == SCHED_FIFO || p->policy == SCHED_RR)
				attr.sched_priority = p->rt_priority;
			ret = sched_setattr_nocheck(p, &attr);
			for_each_thread(p, task_child) {
				if (task_child) {
					get_task_struct(task_child);
					if (try_get_task_stack(task_child))
						ret = sched_setattr_nocheck(task_child, &attr);
					put_task_struct(task_child);
				}
			}
			put_task_struct(p);
			if (ret != 0)
				pr_info(PFX "set uclamp failed, pid %d ret %d\n", group_list[i], ret);
		}
	}

	rcu_read_unlock();
}

static long teeperf_user_ioctl(struct file *file, unsigned int id, unsigned long arg)
{
	int __user *uarg = (int __user *)arg;
	int ret = -EINVAL;

	pr_info(PFX "%u from %s\n", _IOC_NR(id), current->comm);

	if (teeperf_ioctl_check_pointer(id, uarg))
		return -EFAULT;

	switch (id) {
	case TEEPERF_IO_HIGH_FREQ: {
		enum teeperf_cpu_type type = cpu_type;
		enum teeperf_cpu_hint_mode mode = cpu_hint_mode;
		u32 high_freq;

		if (copy_from_user(&high_freq, uarg, sizeof(high_freq))) {
			ret = -EFAULT;
			break;
		}
		pr_info(PFX"mode %d\n", mode);
		if (mode ==  CPU_GRP_AWARE_MODE || mode == CPU_UCLAMP_MODE)
			teeperf_set_uclamp(high_freq);
		else
			teeperf_high_freq(type, high_freq);

		ret = 0;
		break;
	}
	default:
		ret = -ENOIOCTLCMD;
		pr_info(PFX "unsupported command, id %d\n", id);
	}

	return ret;
}

ssize_t teeperf_dbg_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{
	enum teeperf_cpu_type type = cpu_type;
	char *pinput, *cmd_str, *parm_str;
	char input[32] = {0};
	long param;
	size_t len;
	int err;
	u32 high_freq;

	len = (count < (sizeof(input) - 1)) ? count : (sizeof(input) - 1);
	if (copy_from_user(input, buffer, len)) {
		pr_info(PFX "copy from user failed\n");
		return -EFAULT;
	}

	input[len] = '\0';
	pinput = input;

	cmd_str = strsep(&pinput, " ");
	if (!cmd_str)
		return -EINVAL;

	parm_str = strsep(&pinput, " ");
	if (!parm_str)
		return -EINVAL;

	err = kstrtol(parm_str, 10, &param);
	if (err)
		return err;

	if (!strncmp(cmd_str, "teeperf_ut", sizeof("teeperf_ut"))) {
		if (param != 0)
			high_freq = 1;
		else
			high_freq = 0;

		teeperf_high_freq(type, high_freq);
	} else {
		return -EINVAL;
	}

	return count;
}

static const struct file_operations teeperf_user_fops = {
	.owner = THIS_MODULE,
	.open = teeperf_user_open,
	.release = teeperf_user_release,
	.unlocked_ioctl = teeperf_user_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = teeperf_user_ioctl,
#endif
};

static const struct proc_ops teeperf_dbg_fops = {
	.proc_write = teeperf_dbg_write,
};

int teeperf_user_init(struct cdev *cdev)
{
	cdev_init(cdev, &teeperf_user_fops);
	proc_create("teeperf_dbg", 0660, NULL, &teeperf_dbg_fops);
	return 0;
}
