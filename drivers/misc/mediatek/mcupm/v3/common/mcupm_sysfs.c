// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

/* MCUPM LOGGER */
#include <linux/io.h>
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/timer.h>
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/poll.h>


#include "mcupm_ipi_id.h"
#include "../include/mcupm_internal_driver.h"
#include "../include/mcupm_sysfs.h"

static int mcupm_log_if_open(struct inode *inode, struct file *file);
static int mcupm_log_if_open(struct inode *inode, struct file *file)
{
	/* pr_debug("[MCUPM] mcupm_log_if_open\n"); */
	return nonseekable_open(inode, file);
}

static ssize_t mcupm_log_if_read(struct file *file, char __user *data,
				 size_t len, loff_t *ppos);
static ssize_t mcupm_log_if_read(struct file *file, char __user *data,
				 size_t len, loff_t *ppos)
{
	ssize_t ret;

	/* pr_debug("[MCUPM] mcupm_log_if_read\n"); */

	ret = 0;

	return ret;
}
static unsigned int mcupm_log_if_poll(struct file *file, poll_table *wait);
static unsigned int mcupm_log_if_poll(struct file *file, poll_table *wait)
{
	unsigned int ret = 0;

	/* pr_debug("[MCUPM] mcupm_log_if_poll\n"); */


	return ret;
}

static const struct file_operations mcupm_misc_file_ops = {
	.owner = THIS_MODULE,
	.read = mcupm_log_if_read,
	.open = mcupm_log_if_open,
	.poll = mcupm_log_if_poll,
};

static struct miscdevice mcupm_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mcupm",
	.fops = &mcupm_misc_file_ops
};

unsigned int mcupm_target;
static ssize_t mcupm_alive_show(struct device *kobj,
				 struct device_attribute *attr, char *buf)
{

	struct mcupm_ipi_data_s ipi_data;
	int ret = 0;

	ipi_data.cmd = 0xDEAD;

	multi_mcupm_plt_ackdata[mcupm_target] = 0;
	ret = mtk_ipi_send_compl(GET_MCUPM_IPIDEV(mcupm_target), CHAN_PLATFORM, IPI_SEND_POLLING,
			&ipi_data,
			sizeof(struct mcupm_ipi_data_s) / MCUPM_MBOX_SLOT_SIZE,
			2000);
	return snprintf(buf, PAGE_SIZE, "Target MCUPM[%d] Channel(%d) is %s ret=%d\n",
			mcupm_target, CHAN_PLATFORM, multi_mcupm_plt_ackdata[mcupm_target] ? "Alive" : "Dead", ret);

	return -ENODEV;

}
static ssize_t mcupm_alive_store(struct device *dev,
											struct device_attribute *attr,
											const char *buf,
											size_t size)
{
	int rc;
	unsigned int val = 0;

	rc = kstrtouint(buf, 10, &val);
	if (rc < 0 || val >= get_mcupms_ipidev_number()) {
		pr_info( "%s: invalid(out-of-range) val=%d rc=%d\n", __func__, val, rc);
		return -EINVAL;
	}
	mcupm_target = val;

	return size;
}

DEVICE_ATTR_RW(mcupm_alive);

int mcupm_sysfs_create_file(struct device_attribute *attr)
{
	return device_create_file(mcupm_misc_device.this_device, attr);
}

int mcupm_sysfs_create_mcupm_alive(void)
{
	return mcupm_sysfs_create_file(&dev_attr_mcupm_alive);
}

static int mcupm_sysfs_init(void)
{
	int ret;

	ret = misc_register(&mcupm_misc_device);

	if (unlikely(ret != 0))
		return ret;

	return 0;
}
int mcupms_sysfs_misc_init(void)
{
	u32 ret;

	ret = mcupm_sysfs_init();
	if (ret) {
		pr_info("[MCUPM] Sysfs misc init Failed\n");
		return ret;
	}

	return 0;
}


