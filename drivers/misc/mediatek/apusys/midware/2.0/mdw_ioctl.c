// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/uaccess.h>

#include "mdw_ioctl.h"
#include "mdw_cmn.h"

static int mdw_ioctl_cmd(struct mdw_fpriv *mpriv, void *kdata)
{
	int ret = 0;

	if (mpriv->mdev->uapi_ver == 2)
		ret = mdw_cmd_ioctl_v2(mpriv, kdata);
	else if (mpriv->mdev->uapi_ver == 3)
		ret = mdw_cmd_ioctl_v3(mpriv, kdata);
	else if (mpriv->mdev->uapi_ver >= 4)
		ret = mdw_cmd_ioctl_v4(mpriv, kdata);
	else
		ret = -EINVAL;

	return ret;
}

long mdw_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned int usize = 0, nr = _IOC_NR(cmd);
	void *kdata = NULL;
	struct mdw_fpriv *mpriv = filp->private_data;

	/* check nr before any actions */
	if (nr < APU_MDW_IOCTL_START || nr > APU_MDW_IOCTL_END) {
		mdw_drv_debug("not support nr(%u)\n", nr);
		return -ENOTTY;
	}

	/* allocate for user data */
	usize = _IOC_SIZE(cmd);
	/* check kzalloc return */
	kdata = kzalloc(usize, GFP_KERNEL);
	if (unlikely(ZERO_OR_NULL_PTR(kdata)))
		return -ENOMEM;
	/* copy from user data */
	if (cmd & IOC_IN) {
		if (copy_from_user(kdata, (void __user *)arg, usize)) {
			ret = -EFAULT;
			goto out;
		}
	}

	switch (cmd) {
	case APU_MDW_IOCTL_HANDSHAKE:
		ret = mdw_hs_ioctl(mpriv, kdata);
		break;
	case APU_MDW_IOCTL_MEM:
		ret = mdw_mem_ioctl(mpriv, kdata);
		break;
	case APU_MDW_IOCTL_CMD:
		ret = mdw_ioctl_cmd(mpriv, kdata);
		break;
	case APU_MDW_IOCTL_UTIL:
		ret = mdw_util_ioctl(mpriv, kdata);
		break;
	default:
		ret = -EFAULT;
		goto out;
	}

	/* copy to user data */
	if (cmd & IOC_OUT) {
		if (copy_to_user((void __user *)arg, kdata, usize)) {
			ret = -EFAULT;
			goto out;
		}
	}

out:
	kfree(kdata);

	return ret;
}
