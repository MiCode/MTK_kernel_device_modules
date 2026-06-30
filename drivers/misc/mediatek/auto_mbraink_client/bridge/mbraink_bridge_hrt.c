// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/mutex.h>

#include "mbraink_bridge_hrt.h"

static struct mbraink2hrt_ops g_bridge2platform_ops;
static DEFINE_MUTEX(bridge_hrt_lock);

void mbraink_bridge_hrt_init(void)
{

	mutex_lock(&bridge_hrt_lock);

	g_bridge2platform_ops.isp_hrt_notify = NULL;

	mutex_unlock(&bridge_hrt_lock);
}

void mbraink_bridge_hrt_deinit(void)
{
	mutex_lock(&bridge_hrt_lock);

	g_bridge2platform_ops.isp_hrt_notify = NULL;

	mutex_unlock(&bridge_hrt_lock);
}

/* register callback from mbraink platform driver */
int mtk_mbrain2isp_register_ops(struct mbraink2hrt_ops *ops)
{
	if (!ops)
		return -1;

	pr_info("%s successfully\n", __func__);

	mutex_lock(&bridge_hrt_lock);

	g_bridge2platform_ops.isp_hrt_notify = ops->isp_hrt_notify;

	mutex_unlock(&bridge_hrt_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_mbrain2isp_register_ops);

int mtk_mbrain2isp_unregister_ops(void)
{
	pr_info("%s successfully\n", __func__);

	mutex_lock(&bridge_hrt_lock);

	g_bridge2platform_ops.isp_hrt_notify = NULL;

	mutex_unlock(&bridge_hrt_lock);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_mbrain2isp_unregister_ops);

void mtk_mbrain2isp_hrt_cb(int threshold)
{
	mutex_lock(&bridge_hrt_lock);

	if (!g_bridge2platform_ops.isp_hrt_notify)
		pr_info("%s:isp_hrt_notify is NULL\n", __func__);
	else
		g_bridge2platform_ops.isp_hrt_notify(threshold);

	mutex_unlock(&bridge_hrt_lock);
}
EXPORT_SYMBOL_GPL(mtk_mbrain2isp_hrt_cb);

