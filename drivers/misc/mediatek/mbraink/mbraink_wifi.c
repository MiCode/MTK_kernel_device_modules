// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/module.h>

#include "mbraink_modules_ops_def.h"

static struct mbraink_wifi_ops _mbraink_wifi_ops;

int mbraink_wifi_init(void)
{
	_mbraink_wifi_ops.get_wifi_data = NULL;
	return 0;
}

int mbraink_wifi_deinit(void)
{
	_mbraink_wifi_ops.get_wifi_data = NULL;
	return 0;
}

int register_mbraink_wifi_ops(struct mbraink_wifi_ops *ops)
{
	if (!ops)
		return -1;

	pr_info("%s: register.\n", __func__);

	_mbraink_wifi_ops.get_wifi_data = ops->get_wifi_data;

	return 0;
}
EXPORT_SYMBOL(register_mbraink_wifi_ops);

int unregister_mbraink_wifi_ops(void)
{
	pr_info("%s: unregister.\n", __func__);

	_mbraink_wifi_ops.get_wifi_data = NULL;
	return 0;
}
EXPORT_SYMBOL(unregister_mbraink_wifi_ops);

void mbraink_get_wifi_data(unsigned int reason)
{

	if (_mbraink_wifi_ops.get_wifi_data)
		_mbraink_wifi_ops.get_wifi_data(reason);
	else
		pr_info("%s: Do not support ioctl get_wifi_data.\n", __func__);
}
