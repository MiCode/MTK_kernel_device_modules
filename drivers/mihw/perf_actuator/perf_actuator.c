// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2025-2025 Technologies Co., Ltd.
 */

#define pr_fmt(fmt) "perf_actuator: " fmt

#include <linux/module.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include "../include/perf_actuator.h"

struct perf_unit {
	unsigned int cmd;
	PERF_ACTUATOR_FUNC func;
};

struct perf_unit g_perf_actuator_table[PERF_ACTUATOR_MAX_NR];

static DECLARE_RWSEM(g_table_rwsem);

int register_perf_actuator(unsigned int cmd, PERF_ACTUATOR_FUNC func)
{
	unsigned int idx = _IOC_NR(cmd);

	if (idx >= PERF_ACTUATOR_MAX_NR ||
	    func == NULL)
		return -EINVAL;

	down_write(&g_table_rwsem);

	if (g_perf_actuator_table[idx].func)
		pr_err("perf_actuator: register %u conflict\n", cmd);

	g_perf_actuator_table[idx].cmd = cmd;
	g_perf_actuator_table[idx].func = func;

	up_write(&g_table_rwsem);

	return 0;
}
EXPORT_SYMBOL_GPL(register_perf_actuator);

int unregister_perf_actuator(unsigned int cmd)
{
	unsigned int idx = _IOC_NR(cmd);

	if (idx >= PERF_ACTUATOR_MAX_NR)
		return -EINVAL;

	down_write(&g_table_rwsem);

	if (g_perf_actuator_table[idx].func == NULL ||
	    g_perf_actuator_table[idx].cmd != cmd)
		pr_err("perf_actuator: unregister %u conflict\n", cmd);

	g_perf_actuator_table[idx].cmd = 0;
	g_perf_actuator_table[idx].func = NULL;

	up_write(&g_table_rwsem);

	return 0;
}
EXPORT_SYMBOL_GPL(unregister_perf_actuator);

static long perf_actuator_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	void __user *uarg = (void __user *)(uintptr_t)arg;
	unsigned int idx = _IOC_NR(cmd);
	int ret;

	if (uarg == NULL) {
		pr_err("perf_actuator: invalid user uarg\n");
		return -EINVAL;
	}

	if (_IOC_TYPE(cmd) != PERF_ACTUATOR_MAGIC) {
		pr_err("perf_actuator: invalid magic %u\n", _IOC_TYPE(cmd));
		return -EINVAL;
	}

	if (idx >= PERF_ACTUATOR_MAX_NR) {
		pr_err("perf_actuator: invalid cmd, %u > %d\n",
		       idx, PERF_ACTUATOR_MAX_NR);
		return -ERANGE;
	}

	down_read(&g_table_rwsem);
	if (g_perf_actuator_table[idx].func == NULL ||
		g_perf_actuator_table[idx].cmd != cmd) {
		pr_err("perf_actuator: idx %d, cmd %d not match or not registered!\n", idx, cmd);
		ret = -ENOENT;
	} else {
		ret = g_perf_actuator_table[idx].func(uarg);
		pr_debug("perf_actuator: cur %d, cmd %d done!\n", current->pid, idx);
	}

	up_read(&g_table_rwsem);
	return ret;
}

static int perf_actuator_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int perf_actuator_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations perf_actuator_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = perf_actuator_ioctl,
	.open = perf_actuator_open,
	.release = perf_actuator_release,
};

static struct miscdevice perf_actuator_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "xr_perf_actuator",
	.fops = &perf_actuator_fops,
};

static int __init perf_actuator_dev_init(void)
{
	int err;

	err = misc_register(&perf_actuator_device);
	if (err != 0) {
		pr_err("register perf_actuator_device failed, err:%d", err);
		return err;
	}

	return 0;
}

static void __exit perf_actuator_dev_exit(void)
{
	misc_deregister(&perf_actuator_device);
}

module_init(perf_actuator_dev_init);
module_exit(perf_actuator_dev_exit);
MODULE_LICENSE("GPL v2");