/* SPDX-License-Identifier: GPL-2.0 */

/*
 *
 * Copyright (c) 2025 MediaTek Inc.
 *
 */

#ifndef GHOST_TOUCH_H
#define GHOST_TOUCH_H
#define GOODIX_TOUCH_GHOST

#ifdef GOODIX_TOUCH_GHOST
#define GHOST_TOUCH_FIFO_SIZE 64

struct ghost_touch_data {
	int touch_id;
	int64_t timestamp;
	int64_t unixtimestamp;
	int x;
	int y;
};

typedef void (*kfifo_data_ready_callback)(void);

ssize_t get_ghost_buffer_size(void);
ssize_t get_ghost_touch_data(struct ghost_touch_data *buffer, size_t buffer_size);
int register_kfifo_callback(kfifo_data_ready_callback callback);
int unregister_kfifo_callback(kfifo_data_ready_callback callback);
#endif
#endif
