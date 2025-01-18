// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/kdev_t.h>

#include "mbraink_bridge.h"

struct mbraink_bridge_data mbraink_bridge_priv;

static int mbraink_bridge_open(struct inode *inode, struct file *filp)
{
	pr_info("[MBK_BDG_INFO] %s\n", __func__);

	return 0;
}

static int mbraink_bridge_release(struct inode *inode, struct file *filp)
{
	pr_info("[MBK_BDG_INFO] %s\n", __func__);

	return 0;
}

static long mbraink_bridge_ioctl(struct file *filp,
				unsigned int cmd,
				unsigned long arg)
{
	pr_info("[MBK_BDG_INFO] %s\n", __func__);
	return 0;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long mbraink_bridge_compat_ioctl(struct file *filp,
				unsigned int cmd, unsigned long arg)
{
	return mbraink_bridge_ioctl(filp, cmd, (unsigned long) compat_ptr(arg));
}
#endif

static const struct file_operations mbraink_fops = {
	.owner		= THIS_MODULE,
	.open		= mbraink_bridge_open,
	.release        = mbraink_bridge_release,
	.unlocked_ioctl = mbraink_bridge_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl   = mbraink_bridge_compat_ioctl,
#endif
};

static void class_create_release(const struct class *cls)
{
	/*do nothing because the mbraink class is not from malloc*/
}

static struct class mbraink_bridge_class = {
	.name		= "mbraink_bridge_host",
	.class_release	= class_create_release,
	.pm		= NULL,
};

static void device_create_release(struct device *dev)
{
	/*do nothing because the mbraink device is not from malloc*/
}

static struct device mbraink_bridge_device = {
	.init_name	= "mbraink_bridge",
	.release	= device_create_release,
	.parent		= NULL,
	.driver_data	= NULL,
	.class		= NULL,
	.devt		= 0,
};

static ssize_t mbraink_bridge_info_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "show the mbraink bridge information...\n");
}

static ssize_t mbraink_bridge_info_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	pr_info("[MBK_BDG_INFO] %s\n", __func__);
	return count;
}
static DEVICE_ATTR_RW(mbraink_bridge_info);

static int mbraink_bridge_dev_init(void)
{
	dev_t mbraink_dev_no = 0;
	int attr_ret = 0;

	/*Allocating Major number*/
	if ((alloc_chrdev_region(&mbraink_dev_no, 0, 1, CHRDEV_NAME)) < 0) {
		pr_notice("[MBK_BDG_INFO] Cannot allocate major number %u\n",
				mbraink_dev_no);
		return -EBADF;
	}
	pr_info("[MBK_BDG_INFO] %s: Major = %u Minor = %u\n",
			__func__, MAJOR(mbraink_dev_no),
			MINOR(mbraink_dev_no));

	/*Initialize cdev structure*/
	cdev_init(&mbraink_bridge_priv.mbraink_bridge_cdev, &mbraink_fops);

	/*Adding character device to the system*/
	if ((cdev_add(&mbraink_bridge_priv.mbraink_bridge_cdev, mbraink_dev_no, 1)) < 0) {
		pr_notice("[MBK_BDG_INFO] Cannot add the device to the system\n");
		goto r_class;
	}

	/*Register mbraink class*/
	if (class_register(&mbraink_bridge_class)) {
		pr_notice("[MBK_BDG_INFO] Cannot register the mbraink class %s\n",
			mbraink_bridge_class.name);
		goto r_class;
	}

	/*add mbraink device into mbraink_bridge_class host,
	 *and assign the character device id to mbraink device
	 */

	mbraink_bridge_device.devt = mbraink_dev_no;
	mbraink_bridge_device.class = &mbraink_bridge_class;

	/*Register mbraink device*/
	if (device_register(&mbraink_bridge_device)) {
		pr_notice("[MBK_BDG_INFO] Cannot register the Device %s\n",
			mbraink_bridge_device.init_name);
		goto r_device;
	}
	pr_info("[MBK_BDG_INFO] %s: Mbraink device init done.\n", __func__);

	attr_ret = device_create_file(&mbraink_bridge_device, &dev_attr_mbraink_bridge_info);
	pr_info("[MBK_BDG_INFO] %s: device create file mbraink info ret = %d\n",
			__func__, attr_ret);

	return 0;

r_device:
	class_unregister(&mbraink_bridge_class);
r_class:
	unregister_chrdev_region(mbraink_dev_no, 1);

	return -EPERM;
}

static int mbraink_bridge_init(void)
{
	int ret = 0;

	ret = mbraink_bridge_dev_init();
	if (ret)
		pr_notice("[MBK_BDG_INFO] mbraink device init failed.\n");

	mbraink_bridge_gps_init();

	return ret;
}

static void mbraink_bridge_dev_exit(void)
{
	device_remove_file(&mbraink_bridge_device, &dev_attr_mbraink_bridge_info);

	device_unregister(&mbraink_bridge_device);
	mbraink_bridge_device.class = NULL;

	class_unregister(&mbraink_bridge_class);
	cdev_del(&mbraink_bridge_priv.mbraink_bridge_cdev);
	unregister_chrdev_region(mbraink_bridge_device.devt, 1);

	pr_info("[MBK_BDG_INFO] %s: MBraink device exit done, major:minor %u:%u\n",
			__func__,
			MAJOR(mbraink_bridge_device.devt),
			MINOR(mbraink_bridge_device.devt));
}

static void mbraink_bridge_exit(void)
{
	mbraink_bridge_gps_deinit();
	mbraink_bridge_dev_exit();
}

module_init(mbraink_bridge_init);
module_exit(mbraink_bridge_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("<Wei-chin.Tsai@mediatek.com>");
MODULE_DESCRIPTION("MBrainK Bridge Linux Device Driver");
MODULE_VERSION("1.0");
