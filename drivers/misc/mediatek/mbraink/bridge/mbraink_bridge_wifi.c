// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/mutex.h>

#include "mbraink_bridge_wifi.h"

static struct mbraink2wifi_ops g_mbraink2wifi_ops;
static struct wifi2mbraink_set_ops g_wifi2mbraink_ops;
static DEFINE_MUTEX(bridge_wifi_lock);
static DEFINE_MUTEX(bridge_wifi_set_lock);

void mbraink_bridge_wifi_init(void)
{

	mutex_lock(&bridge_wifi_lock);

	g_mbraink2wifi_ops.get_data = NULL;
	g_mbraink2wifi_ops.priv = NULL;
	g_wifi2mbraink_ops.set_data = NULL;

	mutex_unlock(&bridge_wifi_lock);
}

void mbraink_bridge_wifi_deinit(void)
{
	mutex_lock(&bridge_wifi_lock);

	g_mbraink2wifi_ops.get_data = NULL;
	g_mbraink2wifi_ops.priv = NULL;
	g_wifi2mbraink_ops.set_data = NULL;

	mutex_unlock(&bridge_wifi_lock);
}

/*register callback from wifi driver*/
void register_wifi2mbraink_ops(struct mbraink2wifi_ops *ops)
{
	if (!ops)
		return;

	pr_info("%s successfully\n", __func__);

	mutex_lock(&bridge_wifi_lock);

	g_mbraink2wifi_ops.get_data = ops->get_data;
	g_mbraink2wifi_ops.priv = ops->priv;

	mutex_unlock(&bridge_wifi_lock);
}
EXPORT_SYMBOL_GPL(register_wifi2mbraink_ops);

void unregister_wifi2mbraink_ops(void)
{
	pr_info("%s successfully\n", __func__);

	mutex_lock(&bridge_wifi_lock);

	g_mbraink2wifi_ops.get_data = NULL;
	g_mbraink2wifi_ops.priv = NULL;

	mutex_unlock(&bridge_wifi_lock);
}
EXPORT_SYMBOL_GPL(unregister_wifi2mbraink_ops);

int register_platform_to_bridge_ops(struct wifi2mbraink_set_ops *ops)
{
	if (!ops || !ops->set_data)
		return -EINVAL;

	mutex_lock(&bridge_wifi_set_lock);
	g_wifi2mbraink_ops = *ops;
	mutex_unlock(&bridge_wifi_set_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(register_platform_to_bridge_ops);

void unregister_platform_to_bridge_ops(void)
{
	mutex_lock(&bridge_wifi_set_lock);
	memset(&g_wifi2mbraink_ops, 0, sizeof(g_wifi2mbraink_ops));
	mutex_unlock(&bridge_wifi_set_lock);
}
EXPORT_SYMBOL_GPL(unregister_platform_to_bridge_ops);

enum wifi2mbr_status
mbraink_bridge_wifi_get_data(enum mbr2wifi_reason reason,
			enum wifi2mbr_tag tag,
			void *data,
			unsigned short *real_len)
{
	enum wifi2mbr_status ret = WIFI2MBR_NO_OPS;

	mutex_lock(&bridge_wifi_lock);

	if (!g_mbraink2wifi_ops.get_data || !g_mbraink2wifi_ops.priv)
		pr_info("%s: get_data || priv is NULL\n", __func__);
	else
		ret = g_mbraink2wifi_ops.get_data(g_mbraink2wifi_ops.priv,
						reason, tag, data, real_len);

	mutex_unlock(&bridge_wifi_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(mbraink_bridge_wifi_get_data);

/* Notify MBrain with WiFi data
 * This function is called by WiFi driver to send data to MBrain
 */
void wifi2mbrain_notify(enum wifi2mbr_tag tag,
			void *data,
			unsigned short len,
			unsigned short count)
{
	struct wifi2mbr_data wifi_data;

	if (!data || len == 0) {
		pr_info("%s: Invalid data or length\n", __func__);
		return;
	}

	mutex_lock(&bridge_wifi_set_lock);

	if (!g_wifi2mbraink_ops.set_data)
		pr_info("%s: set_data is NULL\n", __func__);
	else {
		wifi_data.tag = tag;
		wifi_data.data = data;
		wifi_data.len = len;
		wifi_data.count = count;
		g_wifi2mbraink_ops.set_data(&wifi_data);
	}

	mutex_unlock(&bridge_wifi_set_lock);
}
EXPORT_SYMBOL_GPL(wifi2mbrain_notify);
