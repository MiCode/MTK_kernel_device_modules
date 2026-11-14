// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <mbraink_modules_ops_def.h>

#include "mbraink_systeminfo.h"

static struct mbraink_systeminfo_ops _mbraink_systeminfo_ops;

int mbraink_systeminfo_init(void)
{
	_mbraink_systeminfo_ops.get_chipid_info = NULL;
	return 0;
}

int mbraink_systeminfo_deinit(void)
{
	_mbraink_systeminfo_ops.get_chipid_info = NULL;
	return 0;
}

int register_mbraink_systeminfo_ops(struct mbraink_systeminfo_ops *ops)
{
	if (!ops)
		return -1;

	pr_info("%s: register.\n", __func__);

	_mbraink_systeminfo_ops.get_chipid_info = ops->get_chipid_info;

	return 0;
}
EXPORT_SYMBOL(register_mbraink_systeminfo_ops);

int unregister_mbraink_systeminfo_ops(void)
{
	pr_info("%s: unregister.\n", __func__);

	_mbraink_systeminfo_ops.get_chipid_info = NULL;

	return 0;
}
EXPORT_SYMBOL(unregister_mbraink_systeminfo_ops);

int mbraink_get_chipid_info(struct mbraink_chipid_info *chipid_info)
{
	int ret = 0;

	if (_mbraink_systeminfo_ops.get_chipid_info)
		ret = _mbraink_systeminfo_ops.get_chipid_info(chipid_info);
	else
		pr_info("%s: Do not support system info query.\n", __func__);
	return ret;
}
