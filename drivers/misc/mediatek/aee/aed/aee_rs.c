// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/string.h>

//#if IS_ENABLED(CONFIG_DEVICE_MODULES_DRM_MEDIATEK)
//#include <mtk_drm_assert_ext.h>
//#endif

#include "aed.h"


static int aed_rs_open(struct inode *inode, struct file *filp)
{
	if (strncmp(current->comm, "aee_aed", 7))
		return -1;
	pr_debug("%s:%d:%d\n", __func__, MAJOR(inode->i_rdev),
						MINOR(inode->i_rdev));
	return 0;
}

static int aed_rs_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static unsigned int aed_rs_poll(struct file *file,
					struct poll_table_struct *ptable)
{
	return 0;
}

static ssize_t aed_rs_read(struct file *filp, char __user *buf,
						size_t count, loff_t *f_pos)
{
	return 0;
}

static ssize_t aed_rs_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	return 0;
}

DEFINE_SEMAPHORE(aed_rs_sem,1);
static long aedrs_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	if (aee_get_mode() >= AEE_MODE_CUSTOMER_ENG) {
		pr_info("cmd(%d) not allowed (mode %d)\n",
				cmd, aee_get_mode());
		return -EFAULT;
	}

	if (down_interruptible(&aed_rs_sem) < 0)
		return -ERESTARTSYS;

	switch (cmd) {
	case AEEIOCTL_DAL_SHOW:
		goto EXIT;
	case AEEIOCTL_DAL_CLEAN:
		goto EXIT;
	case AEEIOCTL_SETCOLOR:
		goto EXIT;
	default:
		ret = -EINVAL;
	}

 EXIT:
	up(&aed_rs_sem);
	return ret;
}

/******************************************************************************
 * Module related
 *****************************************************************************/
static const struct file_operations aed_rs_fops = {
	.owner = THIS_MODULE,
	.open = aed_rs_open,
	.release = aed_rs_release,
	.poll = aed_rs_poll,
	.read = aed_rs_read,
	.write = aed_rs_write,
	.unlocked_ioctl = aedrs_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = aedrs_ioctl,
#endif
};

static struct miscdevice aed_rs_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "aed2",
	.fops = &aed_rs_fops,
};

static int __init aedrs_init(void)
{
	int err;

	err = misc_register(&aed_rs_dev);
	if (unlikely(err))
		pr_err("aee: failed to register aed2 device!\n");

	return err;
}

static void __exit aedrs_exit(void)
{
	misc_deregister(&aed_rs_dev);
}
module_init(aedrs_init);
module_exit(aedrs_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek AED Driver");
MODULE_AUTHOR("MediaTek Inc.");
