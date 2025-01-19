/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _TOUCH_COMM_H_
#define _TOUCH_COMM_H_

#include "touch_ipi_comm.h"

#define TOUCH_COMM_DATA_MAX  (56)
#define TOUCH_COMM_CTRL_DATA_MAX  (3)

enum ack_ret_value {
	TS_ACK_FAIL = 0,
	TS_ACK_PASS,
	MAX_ACK_VALUE,
};

struct touch_comm_ctrl {
	uint8_t sequence;
	uint8_t touch_type;
	uint8_t command;
	uint8_t length;
	uint8_t crc8;
	uint8_t data[TOUCH_COMM_CTRL_DATA_MAX];
} __packed __aligned(4);

struct touch_comm_ack {
	uint8_t sequence;
	uint8_t touch_type;;
	uint8_t command;
	int8_t ret_val;
	uint8_t crc8;
} __packed __aligned(4);

struct touch_comm_notify {
	uint8_t sequence;
	uint8_t touch_type;
	uint8_t command;
	uint8_t crc8;
	uint32_t length;
	int32_t value[TOUCH_COMM_DATA_MAX] __aligned(4);
} __packed __aligned(4);

struct touch_comm_notify_status {
	uint8_t sequence;
	uint8_t touch_type;
	uint8_t touch_id;
	uint8_t command;
	uint8_t crc8;
	uint8_t length;
	int8_t value[2];
} __packed __aligned(4);

int touch_comm_cmd_send(uint8_t touch_type, uint8_t cmd, void *data, uint8_t length);
void touch_comm_data_notify(u_int8_t touch_type, u_int8_t touch_id, int cmd, void *data, uint8_t length);
void touch_comm_notify_handler_register(uint8_t cmd,
		void (*f)(struct touch_comm_notify *n, void *private_data),
		void *private_data);
void touch_comm_notify_handler_unregister(uint8_t cmd);
int touch_comm_init(void);
void touch_comm_exit(void);

#endif
