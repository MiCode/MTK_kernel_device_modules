// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>

#include <mtk_gpu_utility.h>
#include "gpu_bm.h"

static int _mgq_proc_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t
_mgq_proc_write(struct file *file, const char __user *buffer,
					size_t count, loff_t *f_pos)
{
	return 0;
}

static const struct proc_ops _mgq_proc_fops = {
	.proc_open = _mgq_proc_open,
	.proc_read = seq_read,
	.proc_write = _mgq_proc_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int _MTKGPUQoS_initDebugFS(void)
{
	struct proc_dir_entry *dir = NULL;

	dir = proc_mkdir("mgq", NULL);
	if (!dir) {
		pr_debug("@%s: create /proc/mgq failed\n", __func__);
		return -ENOMEM;
	}

	if (!proc_create("job_status", 0664, dir, &_mgq_proc_fops))
		pr_debug("@%s: create /proc/mgq/job_status failed\n", __func__);

	return 0;
}

static void _MTKGPUQoS_setupFW(phys_addr_t phyaddr, size_t size)
{
	;
}

void MTKGPUQoS_mode(int seg_flag)
{
	;
}
EXPORT_SYMBOL(MTKGPUQoS_mode);

void MTKGPUQoS_mode_ratio(int mode)
{
	;
}
EXPORT_SYMBOL(MTKGPUQoS_mode_ratio);

static void bw_v1_gpu_power_change_notify(int power_on)
{
	;
}

static void _MTKGPUQoS_init(void)
{
	;
}

void MTKGPUQoS_setup(struct v1_data *v1, phys_addr_t phyaddr, size_t size)
{
	_MTKGPUQoS_init();
	_MTKGPUQoS_initDebugFS();
	_MTKGPUQoS_setupFW(phyaddr, size);

	mtk_register_gpu_power_change("qpu_qos", bw_v1_gpu_power_change_notify);
}
EXPORT_SYMBOL(MTKGPUQoS_setup);

int MTKGPUQoS_is_inited(void)
{
	return 0;
}
EXPORT_SYMBOL(MTKGPUQoS_is_inited);

uint32_t MTKGPUQoS_getBW(uint32_t offset)
{
	return 0;
}
EXPORT_SYMBOL(MTKGPUQoS_getBW);

static int __init mtk_gpu_qos_init(void)
{
	/*Do Nothing*/
	return 0;
}

static void __exit mtk_gpu_qos_exit(void)
{
	/*Do Nothing*/
	;
}

arch_initcall(mtk_gpu_qos_init);
module_exit(mtk_gpu_qos_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek GPU QOS");
MODULE_AUTHOR("MediaTek Inc.");
