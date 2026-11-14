// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/slab.h>

#include "mbraink_modules_ops_def.h"
#include "mbraink_touch.h"


static struct mbraink_touch_ops _mbraink_touch_ops;

int mbraink_touch_init(void)
{
	_mbraink_touch_ops.get_touch_ghost_info = NULL;
	return 0;
}

int mbraink_touch_deinit(void)
{
	_mbraink_touch_ops.get_touch_ghost_info = NULL;
	return 0;
}

int register_mbraink_touch_ops(struct mbraink_touch_ops *ops)
{
	if (!ops)
		return -1;

	pr_info("%s: register.\n", __func__);

	_mbraink_touch_ops.get_touch_ghost_info = ops->get_touch_ghost_info;

	return 0;
}
EXPORT_SYMBOL(register_mbraink_touch_ops);

int unregister_mbraink_touch_ops(void)
{
	pr_info("%s: unregister.\n", __func__);

	_mbraink_touch_ops.get_touch_ghost_info = NULL;
	return 0;
}
EXPORT_SYMBOL(unregister_mbraink_touch_ops);


int mbraink_get_touch_ghost_info(struct mbraink_touch_ghost_info *touch_ghost_info)
{
	int ret = 0;

	if (_mbraink_touch_ops.get_touch_ghost_info)
		ret = _mbraink_touch_ops.get_touch_ghost_info(touch_ghost_info);
	else
		pr_info("%s: Do not support touch ghost info query.\n", __func__);
	return ret;
}
