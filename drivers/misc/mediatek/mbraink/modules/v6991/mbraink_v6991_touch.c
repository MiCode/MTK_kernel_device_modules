// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <mbraink_modules_ops_def.h>
#include <ghost_touch.h>

#include "mbraink_v6991_touch.h"

void touch_ghost_notify(void)
{
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	int pos = 0;

	n = snprintf(netlink_buf + pos,
			NETLINK_EVENT_MESSAGE_SIZE - pos,
			"%s",
			NETLINK_EVENT_TOUCH_GHOST_NOTIFY);

	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
		return;
	mbraink_netlink_send_msg(netlink_buf);
}

int mbraink_v6991_get_touch_ghost_info(struct mbraink_touch_ghost_info *touch_ghost_info)
{
	int ret = 0;
	size_t buffer_size = 0;
	struct ghost_touch_data *buffer = NULL;
	ssize_t result = 0;
	int count = 0;

	buffer_size = get_ghost_buffer_size();
	buffer = kzalloc(buffer_size, GFP_KERNEL);

	if (buffer == NULL)
		return -ENOMEM;

	result = get_ghost_touch_data(buffer, buffer_size);
	if (result < 0) {
		pr_notice("[%s] Failed to get ghost touch data\n", __func__);
		kfree(buffer);
		ret = -EINVAL;
	} else {
		memset(touch_ghost_info, 0, sizeof(struct mbraink_touch_ghost_info));
		count = buffer_size / sizeof(struct ghost_touch_data);
		count = count > MAX_TOUCH_GHOST_SZ ? MAX_TOUCH_GHOST_SZ : count;
		for (int i = 0; i < count; i++) {
			struct ghost_touch_data *current_touch_data = &buffer[i];

			touch_ghost_info->data[i].kernel_time = current_touch_data->timestamp;
			touch_ghost_info->data[i].unix_time = current_touch_data->unixtimestamp;
			touch_ghost_info->data[i].touch_id = current_touch_data->touch_id;
			touch_ghost_info->data[i].x = current_touch_data->x;
			touch_ghost_info->data[i].y = current_touch_data->y;
			touch_ghost_info->count += 1;
		}

		kfree(buffer);
	}

	return ret;
}

static struct mbraink_touch_ops mbraink_v6991_touch_ops = {
	.get_touch_ghost_info = mbraink_v6991_get_touch_ghost_info,
};

int mbraink_v6991_touch_init(void)
{
	int ret = 0;

	ret = register_mbraink_touch_ops(&mbraink_v6991_touch_ops);
	ret = register_kfifo_callback(touch_ghost_notify);
	return ret;
}

int mbraink_v6991_touch_deinit(void)
{
	int ret = 0;

	ret = unregister_mbraink_touch_ops();
	ret = unregister_kfifo_callback(touch_ghost_notify);
	return ret;
}
