// SPDX-License-Identifier: GPL-2.0
//
// Copyright (C) 2025 MediaTek Inc.
//
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/stddef.h>
#include <linux/mutex.h>

#include "ccci_debug.h"
#include "mt-plat/mtk_ccci_common.h"
#include "ccci_mbrain.h"

#define TAG "ccci_mbrain"

static DEFINE_MUTEX(ccci_mbrain_lock);
static ccci_mbrain_event_notify_func_t ccci_mbrain_event_notify_fp;


ccci_mbrain_event_notify_func_t ccci_mbrain_get_event_notify_fp(void)
{
	ccci_mbrain_event_notify_func_t callback;

	mutex_lock(&ccci_mbrain_lock);
	callback = ccci_mbrain_event_notify_fp;
	mutex_unlock(&ccci_mbrain_lock);

	return callback;
}


// mbrain call ccci_mbrain_register function
int ccci_mbrain_register(ccci_mbrain_event_notify_func_t callback)
{
	if (!callback) {
		CCCI_ERROR_LOG(-1, TAG, "Parameter is null\n");
		return -1;
	}

	mutex_lock(&ccci_mbrain_lock);
	if (ccci_mbrain_event_notify_fp) {
		CCCI_ERROR_LOG(-1, TAG, "callback already registered\n");
		mutex_unlock(&ccci_mbrain_lock);
		return -1;
	}

	ccci_mbrain_event_notify_fp = callback;
	mutex_unlock(&ccci_mbrain_lock);

	return 0;
}
EXPORT_SYMBOL(ccci_mbrain_register);


// mbrain call ccci_mbrain_unregister function
int ccci_mbrain_unregister(void)
{
	mutex_lock(&ccci_mbrain_lock);
	ccci_mbrain_event_notify_fp = NULL;
	mutex_unlock(&ccci_mbrain_lock);

	return 0;
}
EXPORT_SYMBOL(ccci_mbrain_unregister);

