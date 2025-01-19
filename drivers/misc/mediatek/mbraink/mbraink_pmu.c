// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <mbraink_modules_ops_def.h>

#include "mbraink_pmu.h"

static struct mbraink_pmu_ops _mbraink_pmu_ops;

int mbraink_pmu_init(void)
{
	_mbraink_pmu_ops.set_pmu_enable = NULL;
	_mbraink_pmu_ops.get_pmu_info = NULL;
	return 0;
}

int mbraink_pmu_deinit(void)
{
	_mbraink_pmu_ops.set_pmu_enable = NULL;
	_mbraink_pmu_ops.get_pmu_info = NULL;
	return 0;
}

int register_mbraink_pmu_ops(struct mbraink_pmu_ops *ops)
{
	if (!ops)
		return -1;

	pr_info("%s: register.\n", __func__);

	_mbraink_pmu_ops.set_pmu_enable = ops->set_pmu_enable;
	_mbraink_pmu_ops.get_pmu_info = ops->get_pmu_info;

	return 0;
}
EXPORT_SYMBOL(register_mbraink_pmu_ops);

int unregister_mbraink_pmu_ops(void)
{
	pr_info("%s: unregister.\n", __func__);

	_mbraink_pmu_ops.set_pmu_enable = NULL;
	_mbraink_pmu_ops.get_pmu_info = NULL;
	return 0;
}
EXPORT_SYMBOL(unregister_mbraink_pmu_ops);

int mbraink_set_pmu_enable(bool enable)
{
	int ret = 0;

	if (_mbraink_pmu_ops.set_pmu_enable)
		ret = _mbraink_pmu_ops.set_pmu_enable(enable);
	else
		pr_info("%s: Do not support pmu info feature.\n", __func__);

	return ret;
}

int mbraink_get_pmu_info(struct mbraink_pmu_info *pmuInfo)
{
	int ret = 0;

	if (_mbraink_pmu_ops.get_pmu_info)
		ret = _mbraink_pmu_ops.get_pmu_info(pmuInfo);
	else
		pr_info("%s: Do not support pmu info query.\n", __func__);
	return ret;
}
