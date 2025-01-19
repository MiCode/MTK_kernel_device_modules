/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _TOUCH_COMM_H_
#define _TOUCH_COMM_H_

#include "touch_ipi_comm.h"

enum touch_comm_ctrl_cmd {
	TOUCH_COMM_CTRL_SCP_HANDLE_CMD, //SCP handle touch event
	TOUCH_COMM_CTRL_AP_HANDLE_CMD, //AP handle touch event
	TOUCH_COMM_CTRL_QUERY_SCP_STATUS_CMD, //query scp is ready for touch or not
	TOUCH_COMM_CTRL_SUSPEND_CMD, //notify suspend
	TOUCH_COMM_CTRL_RESUME_CMD, //notify resume
	TOUCH_COMM_CTRL_REINIT_CMD, //notify reinit
	TOUCH_COMM_CTRL_CHANGE_REPORT_RATE_CMD, //change report rate

	MAX_TOUCH_COMM_CTRL_CMD,
};

enum touch_comm_notify_cmd {
	TOUCH_COMM_NOTIFY_DATA_CMD,
	TOUCH_COMM_NOTIFY_READY_CMD,

	MAX_TOUCH_COMM_NOTIFY_CMD,
};

struct touch_comm_batch {
	int64_t delay;
	int64_t latency;
} __packed __aligned(4);

struct touch_comm_timesync {
	int64_t host_timestamp;
	int64_t host_archcounter;
	int64_t sched_clock;
	int32_t usecond;
	int32_t second;
	int32_t minute;
	int32_t hour;
	int32_t day;
	int32_t month;
} __packed __aligned(4);

struct touch_comm_share_mem {
	uint8_t available_num;
	struct {
		uint8_t payload_type;
		uint32_t payload_base;
	} __aligned(4) base_info[4];
} __packed __aligned(4);

struct touch_comm_ctrl {
	uint8_t sequence;
	uint8_t touch_id;
	uint8_t command;
	uint8_t length;
	uint8_t crc8;
	uint8_t data[3];
} __packed __aligned(4);

struct touch_comm_ack {
	uint8_t sequence;
	uint8_t touch_id;;
	uint8_t command;
	int8_t ret_val;
	uint8_t crc8;
} __packed __aligned(4);

struct data_notify {
	int32_t write_position;
	int64_t scp_timestamp;
	int64_t scp_archcounter;
} __packed __aligned(4);

struct touch_comm_notify {
	uint8_t sequence;
	uint8_t touch_id;
	uint8_t command;
	uint8_t crc8;
	uint32_t length;
	int32_t value[56] __aligned(4);
} __packed __aligned(4);

struct touch_comm_notify_status {
	uint8_t sequence;
	uint8_t touch_id;
	uint8_t touch_type;
	uint8_t command;
	uint8_t crc8;
	uint8_t length;
	int8_t value[2];
} __packed __aligned(4);

int touch_comm_ctrl_send(struct touch_comm_ctrl *ctrl, unsigned int size);
int touch_comm_notify(struct touch_comm_notify *notify);
int touch_comm_notify_status_bypass(struct touch_comm_notify_status *notify);
void touch_comm_notify_handler_register(uint8_t cmd,
		void (*f)(struct touch_comm_notify *n, void *private_data),
		void *private_data);
void touch_comm_notify_handler_unregister(uint8_t cmd);
int touch_comm_init(void);
void touch_comm_exit(void);

#endif
