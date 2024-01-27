// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include "mdw_ext.h"
#include "mdw_ext_ioctl.h"

#define MDWEXT_DEV_NAME "apuext"

struct mdw_ext_device *mdw_ext_dev;
u32 g_mdwext_klog = 15;

static int mdw_ext_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int mdw_ext_close(struct inode *inode, struct file *filp)
{
	return 0;
}

static const struct file_operations mdw_ext_fops = {
	.owner = THIS_MODULE,
	.open = mdw_ext_open,
	.release = mdw_ext_close,
	.unlocked_ioctl = mdw_ext_ioctl,
	.compat_ioctl = mdw_ext_ioctl,
};

static struct miscdevice mdw_ext_misc_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MDWEXT_DEV_NAME,
	.fops = &mdw_ext_fops,
};

//----------------------------------------
int mdw_ext_init(void)
{
	int ret = 0;

	pr_info("%s register misc...\n", __func__);
	ret = misc_register(&mdw_ext_misc_dev);
	if (ret) {
		pr_info("failed to register apu ext misc driver\n");
		goto out;
	}

	/* alloc ext device */
	mdw_ext_dev = kzalloc(sizeof(*mdw_ext_dev), GFP_KERNEL);
	if (mdw_ext_dev == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	mdw_ext_dev->misc_dev = &mdw_ext_misc_dev;
	pr_info("%s register misc done\n", __func__);

out:
	return ret;
}

void mdw_ext_deinit(void)
{
	misc_deregister(&mdw_ext_misc_dev);
	if (mdw_ext_dev != NULL)
		kfree(mdw_ext_dev);
}
