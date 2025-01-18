// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/mutex.h>

#include "mbraink_v6991.h"
#include "mbraink_v6991_memory.h"
#include "mbraink_v6991_audio.h"
#include "mbraink_v6991_battery.h"
#include "mbraink_v6991_power.h"
#include "mbraink_v6991_gpu.h"
#include "mbraink_v6991_gps.h"

//static DEFINE_MUTEX(power_lock);
//static DEFINE_MUTEX(pmu_lock);
struct mbraink_v6991_data mbraink_v6991_priv;

static int mbraink_v6991_open(struct inode *inode, struct file *filp)
{
	pr_info("[MBK_v6991] %s\n", __func__);

	return 0;
}

static int mbraink_v6991_release(struct inode *inode, struct file *filp)
{
	pr_info("[MBK_v6991] %s\n", __func__);

	return 0;
}

static long mbraink_v6991_ioctl(struct file *filp,
							unsigned int cmd,
							unsigned long arg)
{
	pr_info("[MBK_v6991] %s\n", __func__);

	return 0;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long mbraink_v6991_compat_ioctl(struct file *filp,
				unsigned int cmd, unsigned long arg)
{
	return mbraink_v6991_ioctl(filp, cmd, (unsigned long) compat_ptr(arg));
}
#endif

static const struct file_operations mbraink_v6991_fops = {
	.owner		= THIS_MODULE,
	.open		= mbraink_v6991_open,
	.release        = mbraink_v6991_release,
	.unlocked_ioctl = mbraink_v6991_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl   = mbraink_v6991_compat_ioctl,
#endif
};

static void class_create_release(const struct class *cls)
{
	/*do nothing because the mbraink class is not from malloc*/
}

static struct class mbraink_v6991_class = {
	.name		= "mbraink_platform_host",
	.class_release	= class_create_release,
	.pm		= NULL,
};

static void device_create_release(struct device *dev)
{
	/*do nothing because the mbraink device is not from malloc*/
}

static struct device mbraink_v6991_device = {
	.init_name	= "mbraink_platform",
	.release		= device_create_release,
	.parent		= NULL,
	.driver_data	= NULL,
	.class		= NULL,
	.devt		= 0,
};

static ssize_t mbraink_platform_info_show(struct device *dev,
								struct device_attribute *attr,
								char *buf)
{
	return sprintf(buf, "show mbraink v6991 information...\n");
}

static ssize_t mbraink_platform_info_store(struct device *dev,
								struct device_attribute *attr,
								const char *buf,
								size_t count)
{
	unsigned int command;
	unsigned long long value;
	int retSize = 0;

	retSize = sscanf(buf, "%d %llu", &command, &value);
	if (retSize == -1)
		return 0;

	pr_info("%s: Get Command (%d), Value (%llu) size(%d)\n",
			__func__,
			command,
			value,
			retSize);

	if (command == 1)
		mbraink_v6991_gpu_setQ2QTimeoutInNS(value);
	if (command == 2)
		mbraink_v6991_gpu_setPerfIdxTimeoutInNS(value);
	if (command == 3)
		mbraink_v6991_gpu_setPerfIdxLimit(value);
	if (command == 4)
		mbraink_v6991_gpu_dumpPerfIdxList();
	return count;
}

static DEVICE_ATTR_RW(mbraink_platform_info);

static int mbraink_v6991_dev_init(void)
{
	dev_t mbraink_v6991_dev_no = 0;
	int attr_ret = 0;

	/*Allocating Major number*/
	if ((alloc_chrdev_region(&mbraink_v6991_dev_no, 0, 1, CHRDEV_NAME)) < 0) {
		pr_notice("Cannot allocate major number %u\n",
				mbraink_v6991_dev_no);
		return -EBADF;
	}
	pr_info("[MBK_v6991] %s: Major = %u Minor = %u\n",
			__func__, MAJOR(mbraink_v6991_dev_no),
			MINOR(mbraink_v6991_dev_no));

	/*Initialize cdev structure*/
	cdev_init(&mbraink_v6991_priv.mbraink_v6991_cdev, &mbraink_v6991_fops);

	/*Adding character device to the system*/
	if ((cdev_add(&mbraink_v6991_priv.mbraink_v6991_cdev, mbraink_v6991_dev_no, 1)) < 0) {
		pr_notice("[MBK_v6991] Cannot add the device to the system\n");
		goto r_class;
	}

	/*Register mbraink v6991 class*/
	if (class_register(&mbraink_v6991_class)) {
		pr_notice("[MBK_v6991] Cannot register the mbraink class %s\n",
			mbraink_v6991_class.name);
		goto r_class;
	}

	/*add mbraink v6991 device into mbraink_v6991_class host,
	 *and assign the character device id to mbraink v6991 device
	 */

	mbraink_v6991_device.devt = mbraink_v6991_dev_no;
	mbraink_v6991_device.class = &mbraink_v6991_class;

	/*Register mbraink v6991 device*/
	if (device_register(&mbraink_v6991_device)) {
		pr_notice("[MBK_v6991] Cannot register the Device %s\n",
			mbraink_v6991_device.init_name);
		goto r_device;
	}
	pr_info("[MBK_v6991] %s: Mbraink v6991 device init done.\n", __func__);

	attr_ret = device_create_file(&mbraink_v6991_device, &dev_attr_mbraink_platform_info);
	pr_info("[MBK_v6991] %s: device create file mbraink info ret = %d\n", __func__, attr_ret);

	return 0;

r_device:
	class_unregister(&mbraink_v6991_class);
r_class:
	unregister_chrdev_region(mbraink_v6991_dev_no, 1);

	return -EPERM;
}

static int mbraink_v6991_init(void)
{
	int ret = 0;

	ret = mbraink_v6991_dev_init();
	if (ret)
		pr_notice("[MBK_v6991] mbraink v6991 device init failed.\n");

	ret = mbraink_v6991_memory_init();
	if (ret)
		pr_notice("[MBK_v6991] mbraink v6991 memory init failed.\n");

	ret = mbraink_v6991_audio_init();
	if (ret)
		pr_notice("[MBK_v6991] mbraink v6991 audio init failed.\n");

	ret = mbraink_v6991_battery_init();
	if (ret)
		pr_notice("[MBK_v6991] mbraink v6991 battery init failed.\n");

	ret = mbraink_v6991_power_init();
	if (ret)
		pr_notice("[MBK_v6991] mbraink v6991 power init failed.\n");

	ret = mbraink_v6991_gpu_init();
	if (ret)
		pr_notice("[MBK_v6991] mbraink v6991 gpu init failed.\n");

	ret = mbraink_v6991_gps_init();
	if (ret)
		pr_notice("[MBK_v6991] mbraink v6991 gps init failed.\n");
	return ret;
}

static void mbraink_v6991_dev_exit(void)
{
	device_remove_file(&mbraink_v6991_device, &dev_attr_mbraink_platform_info);

	device_unregister(&mbraink_v6991_device);
	mbraink_v6991_device.class = NULL;

	class_unregister(&mbraink_v6991_class);
	cdev_del(&mbraink_v6991_priv.mbraink_v6991_cdev);
	unregister_chrdev_region(mbraink_v6991_device.devt, 1);

	pr_info("[MBK_v6991] %s: MBraink v6991 device exit done, major:minor %u:%u\n",
			__func__,
			MAJOR(mbraink_v6991_device.devt),
			MINOR(mbraink_v6991_device.devt));
}

static void mbraink_v6991_exit(void)
{
	mbraink_v6991_dev_exit();
	mbraink_v6991_memory_deinit();
	mbraink_v6991_audio_deinit();
	mbraink_v6991_battery_deinit();
	mbraink_v6991_power_deinit();
	mbraink_v6991_gpu_deinit();
	mbraink_v6991_gps_deinit();
}

module_init(mbraink_v6991_init);
module_exit(mbraink_v6991_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("<Wei-chin.Tsai@mediatek.com>");
MODULE_DESCRIPTION("MBrainK v6991 Linux Device Driver");
MODULE_VERSION("1.0");
