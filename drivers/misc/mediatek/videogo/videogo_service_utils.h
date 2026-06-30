/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#ifndef VIDEOGO_SERVICE_UTILS_H
#define VIDEOGO_SERVICE_UTILS_H

#include <linux/wait.h>

#include "videogo_driver.h"
#include "videogo_utils.h"

static DEFINE_MUTEX(service_mutex);
static DECLARE_KFIFO(service_fifo, struct vgo_powerhal_info, 16);
static DECLARE_WAIT_QUEUE_HEAD(service_wq);

static void send_service_info(const char *log_msg, int service_type,
								int data0, int data1, int data2)
{
	struct vgo_powerhal_info service_info;

	mtk_vgo_debug("%s: %d %d %d", log_msg, data0, data1, data2);
	service_info.type = service_type;
	service_info.data[0] = data0;
	service_info.data[1] = data1;
	service_info.data[2] = data2;
	mutex_lock(&service_mutex);
	kfifo_in(&service_fifo, &service_info, 1);
	mutex_unlock(&service_mutex);

	wake_up_interruptible(&service_wq);
}

#endif // VIDEOGO_SERVICE_UTILS_H

