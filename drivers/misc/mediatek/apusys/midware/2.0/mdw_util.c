// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mdw_ioctl.h"
#include "mdw_cmn.h"

static int mdw_util_info(struct mdw_fpriv *mpriv, uint32_t type, uint64_t val)
{
	int ret = 0;
	struct mdw_device *mdev = mpriv->mdev;

	switch (type) {
	case MDW_UTIL_INFO_POWERPOLICY:
		/* only performance mode can be set by user */
		if (val == MDW_POWERPOLICY_PERFORMANCE)
			ret = mdev->dev_funcs->pb_get(MDW_POWERPOLICY_PERFORMANCE, MDW_PB_DEBOUNCE_MS);
		break;

	default:
		break;
	}

	return ret;
}

int mdw_util_ioctl(struct mdw_fpriv *mpriv, void *data)
{
	union mdw_util_args *args = (union mdw_util_args *)data;
	struct mdw_util_in *in = (struct mdw_util_in *)args;
	struct mdw_device *mdev = mpriv->mdev;
	void *mem_ucmd = NULL;
	int ret = 0;

	mdw_flw_debug("s(0x%llx) op::%d\n", (uint64_t)mpriv, args->in.op);

	switch (args->in.op) {
	case MDW_UTIL_IOCTL_INFO:
		ret = mdw_util_info(mpriv, in->info.type, in->info.value);
		if (ret) {
			mdw_drv_err("setup info(%u/0x%llx) fail(%d)\n",
				in->info.type, in->info.value, ret);
		}
		break;

	case MDW_UTIL_IOCTL_SETPOWER:
		ret = mdev->dev_funcs->set_power(mdev, in->power.dev_type,
			in->power.core_idx, in->power.boost);
		break;

	case MDW_UTIL_IOCTL_UCMD:
		if (!in->ucmd.size || !in->ucmd.handle) {
			mdw_drv_err("invalid ucmd(%u/0x%llx) param\n",
				in->ucmd.size, in->ucmd.handle);
			ret = -EINVAL;
			break;
		}

		if (args->in.ucmd.size > MDW_UTIL_CMD_MAX_SIZE) {
			mdw_drv_err("util cmd over size(%u/%u)\n",
				args->in.ucmd.size, MDW_UTIL_CMD_MAX_SIZE);
			ret = -EINVAL;
			break;
		}

		mem_ucmd = vzalloc(args->in.ucmd.size);
		if (!mem_ucmd) {
			ret = -ENOMEM;
			break;
		}

		if (copy_from_user(mem_ucmd,
			(void __user *)in->ucmd.handle,
			in->ucmd.size)) {
			ret = -EFAULT;
			goto free_ucmd;
		}
		ret = mdev->dev_funcs->ucmd(mdev, in->ucmd.dev_type,
			mem_ucmd, in->ucmd.size);
		if (ret) {
			mdw_drv_err("dev(%d) ucmd fail\n", in->ucmd.dev_type);
			goto free_ucmd;
		}

		if (copy_to_user((void __user *)in->ucmd.handle,
				mem_ucmd, in->ucmd.size))
			ret = -EFAULT;

free_ucmd:
		vfree(mem_ucmd);
		break;

	default:
		mdw_drv_err("invalid util op code(%d)\n", args->in.op);
		ret = -EINVAL;
		break;
	}

	return ret;
}
