// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h> /* for SMC ID table */
#define MODULE_NAME	 "[atf-efuse]"
#define DEV_NAME	 "atf-efuse"
#define FIRST_MINOR 0
#define MINOR_CNT 1
#define ATF_EFUSE_IOCTL_READ 0
#define ATF_EFUSE_IOCTL_WRITE 1
#define EFUSE_BUFFER_SIZE 256

/******************************************************************************
 *
 *  eFuse SMC Control
 *
 *****************************************************************************/
enum mtk_efuse_kernel_ctrl_cmd {
	MTK_EFUSE_KERNEL_CTRL_GET_LEN = 0,
	MTK_EFUSE_KERNEL_CTRL_READ = 1,
	MTK_EFUSE_KERNEL_CTRL_GET_DATA = 2,
	MTK_EFUSE_KERNEL_CTRL_WRITE = 3,
	MTK_EFUSE_KERNEL_CTRL_SEND_DATA = 4,
};

/******************************************************************************
 *
 *  Returned status of eFuse SMC
 *
 *****************************************************************************/
#define MTK_EFUSE_SUCCESS					0x00000000
#define MTK_EFUSE_ERROR_INVALID_LEN				0x00000001
#define MTK_EFUSE_ERROR_INVALID_PARAM				0x00000002

/* use to offset efuse r/w api error code */
#define MTK_EFUSE_ERROR_CODE_OFFSET				0x0000000A

/******************************************************************************
 *
 *  Returned status of eFuse ioctl
 *
 *****************************************************************************/
#define ATF_EFUSE_SUCCESS					0x00000000
#define ATF_EFUSE_LEN_MISMATCH					0x00000001

struct atf_efuse_data {
	u32 fuse;
	u8 data[EFUSE_BUFFER_SIZE];
	size_t len;
	u32 ret;
};

static int major_number;
dev_t atf_efuse_dev;
static struct class *atf_efuse_class;
struct cdev atf_efuse_cdev;

static int atf_efuse_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int atf_efuse_release(struct inode *inode, struct file *file)
{
	return 0;
}


static long atf_efuse_ioctl(struct file *file, unsigned int ioctl_num,
				unsigned long ioctl_param)
{
	struct atf_efuse_data *ptr = (struct atf_efuse_data *)ioctl_param;
	struct atf_efuse_data data_in = {0};
	struct atf_efuse_data data_out = {0};
	struct arm_smccc_res res;
	u32 fuse = 0x0;
	u32 len = 0x0;
	u32 send_data = 0x0;
	long ret = 0;
	int offset = 0;

	if (copy_from_user(&data_in, ptr, sizeof(struct atf_efuse_data)) != 0)
		return -EFAULT;

	switch (ioctl_num) {
	case ATF_EFUSE_IOCTL_READ:
		fuse = data_in.fuse;
		len = data_in.len;
		arm_smccc_smc(MTK_SIP_KERNEL_EFUSE_CONTROL, MTK_EFUSE_KERNEL_CTRL_GET_LEN,
			fuse, len, 0x0, 0x0, 0x0, 0x0, &res);
		if (res.a0 != MTK_EFUSE_SUCCESS) {
			ret = -1;
			data_out.ret = res.a0;
			goto out;
		}

		arm_smccc_smc(MTK_SIP_KERNEL_EFUSE_CONTROL, MTK_EFUSE_KERNEL_CTRL_READ,
			fuse, len, 0x0, 0x0, 0x0, 0x0, &res);
		if (res.a0 != MTK_EFUSE_SUCCESS) {
			ret = -1;
			data_out.ret = res.a0;
			goto out;
		}

		if(len > EFUSE_BUFFER_SIZE) {
			ret = -1;
			goto out;
		}

		// len and offset are both in bytes
		for (offset = 0; offset < len; offset += 4) {
			arm_smccc_smc(MTK_SIP_KERNEL_EFUSE_CONTROL,
				MTK_EFUSE_KERNEL_CTRL_GET_DATA,
				fuse, offset, 0x0, 0x0, 0x0, 0x0, &res);
			if (res.a0 != MTK_EFUSE_SUCCESS) {
				ret = -1;
				data_out.ret = res.a0;
				goto out;
			} else
				memcpy(&(data_out.data[offset]), &(res.a1), 4);
		}
		break;
	case ATF_EFUSE_IOCTL_WRITE:
		fuse = data_in.fuse;
		len = data_in.len;
		arm_smccc_smc(MTK_SIP_KERNEL_EFUSE_CONTROL, MTK_EFUSE_KERNEL_CTRL_GET_LEN,
			fuse, len, 0x0, 0x0, 0x0, 0x0, &res);
		if (res.a0 != MTK_EFUSE_SUCCESS) {
			ret = -1;
			data_out.ret = res.a0;
			goto out;
		}

		// len and offset are both in bytes
		for (offset = 0; offset < len; offset += 4) {
			memcpy(&send_data, &(data_in.data[offset]), 4);
			arm_smccc_smc(MTK_SIP_KERNEL_EFUSE_CONTROL,
				MTK_EFUSE_KERNEL_CTRL_SEND_DATA,
				fuse, offset, send_data, 0x0, 0x0, 0x0, &res);
			if (res.a0 != MTK_EFUSE_SUCCESS) {
				ret = -1;
				data_out.ret = res.a0;
				goto out;
			}
		}

		arm_smccc_smc(MTK_SIP_KERNEL_EFUSE_CONTROL, MTK_EFUSE_KERNEL_CTRL_WRITE,
			fuse, len, 0x0, 0x0, 0x0, 0x0, &res);
		if (res.a0 != MTK_EFUSE_SUCCESS) {
			ret = -1;
			data_out.ret = res.a0;
			goto out;
		}
		break;
	default:
		return -ENOTTY;
	}

out:
	if (copy_to_user(ptr, &data_out, sizeof(struct atf_efuse_data)) != 0)
		return -EFAULT;

	return ret;
}

static const struct file_operations atf_efuse_fops = {
	.unlocked_ioctl = atf_efuse_ioctl,
	.open = atf_efuse_open,
	.release = atf_efuse_release,
};

static int __init atf_efuse_init(void)
{
	int ret = 0;
	struct device *device;

	if (alloc_chrdev_region(&atf_efuse_dev, FIRST_MINOR, MINOR_CNT, DEV_NAME) < 0) {
		ret = 0;
		goto out;
	}
	atf_efuse_class = class_create(DEV_NAME);
	if (IS_ERR(atf_efuse_class)) {
		ret = PTR_ERR(atf_efuse_class);
		unregister_chrdev_region(atf_efuse_dev, 1);
		goto out;
	}
	/* initialize the device structure and register the device  */
	cdev_init(&atf_efuse_cdev, &atf_efuse_fops);
	atf_efuse_cdev.owner = THIS_MODULE;

	ret = cdev_add(&atf_efuse_cdev, atf_efuse_dev, 1);
	if (ret < 0) {
		class_destroy(atf_efuse_class);
		unregister_chrdev_region(atf_efuse_dev, 1);
		goto out;
	}
	/*create device*/
	device = device_create(atf_efuse_class, NULL, atf_efuse_dev, NULL,
			"atf-efuse");
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		cdev_del(&atf_efuse_cdev);
		class_destroy(atf_efuse_class);
		unregister_chrdev_region(atf_efuse_dev, 1);
		goto out;
	}
out:
	return ret;
}

static void __exit atf_efuse_exit(void)
{
	unregister_chrdev(major_number, DEV_NAME);
}

module_init(atf_efuse_init);
module_exit(atf_efuse_exit);
MODULE_LICENSE("GPL");
