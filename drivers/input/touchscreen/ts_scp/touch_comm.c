// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "touch_comm " fmt

#include <linux/err.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/atomic.h>
#include <linux/string.h>
#include <linux/timekeeping.h>
#include <linux/mutex.h>

#include "tiny_crc8.h"
#include "ts_scp_core.h"

struct touch_notify_handle {
	void (*handler)(struct touch_comm_notify *n, void *private_data);
	void *private_data;
};

static DEFINE_MUTEX(touch_comm_lock);
static uint8_t touch_comm_sequence;
struct touch_notify_handle touch_notify_handle[MAX_TOUCH_COMM_NOTIFY_CMD];

static void touch_comm_notify_handler(int id, void *data, unsigned int len)
{
	uint8_t crc = 0;
	struct touch_comm_notify *notify = data;
	struct touch_notify_handle *handle = NULL;

	crc = tiny_crc8((uint8_t *)notify, offsetof(typeof(*notify), crc8));
	if (notify->crc8 != crc) {
		pr_info("unrecognized packet %u %u %u %u %u\n",
			notify->sequence, notify->command,
			notify->length, notify->crc8, crc);
		return;
	}

	if (sizeof(*notify) < len) {
		pr_info("sizeof(*notify) < len");
		return;
	}

	if (notify->command >= MAX_TOUCH_COMM_NOTIFY_CMD) {
		pr_info("notify->command >= MAX_TOUCH_COMM_NOTIFY_CMD");
		return;
	}
	handle = &touch_notify_handle[notify->command];
	if (!handle->handler) {
		pr_info("!handle->handler");
		return;
	}
	handle->handler(notify, handle->private_data);
}

static int touch_comm_ctrl_seq_send(struct touch_comm_ctrl *ctrl,
		unsigned int size)
{
	int ret = 0;
	uint8_t crc = 0;
	struct touch_comm_ack ack;

	memset(&ack, 0, sizeof(ack));
	ctrl->sequence = touch_comm_sequence;
	ctrl->crc8 = tiny_crc8((uint8_t *)ctrl, offsetof(typeof(*ctrl), crc8));
	ret = ipi_comm_sync(get_ctrl_id(), (unsigned char *)ctrl, size,
		(unsigned char *)&ack, sizeof(ack));
	if (ret < 0) {
		if (ret != -EIO)
			touch_comm_sequence++;
		pr_info_ratelimited("ipi_comm_sync error %d\n", ret);
		return ret;
	}
	touch_comm_sequence++;
	crc = tiny_crc8((uint8_t *)&ack, offsetof(typeof(ack), crc8));

	if (ack.crc8 != crc) {
		pr_info("unrecognized packet %u %u %u %u %u\n",
			ack.sequence, ack.command,
			ack.ret_val, ack.crc8, crc);
		return -EBADMSG;
	}
	if (ctrl->sequence != ack.sequence)
		return -EILSEQ;
	if (ctrl->command != ack.command)
		return -EPROTO;
	if (ack.ret_val != TS_ACK_PASS) {
		pr_info("ack.ret_val fail ret=%d\n", ack.ret_val);
		return -EREMOTEIO;
	}

	return 0;
}

static int touch_comm_ctrl_send_locked(struct touch_comm_ctrl *ctrl,
		unsigned int size)
{
	int retry = 0, ret = 0;
	const int max_retry = 10;
	const int64_t timeout = 10000000000LL;
	int64_t start_time = 0, duration = 0;

	start_time = ktime_get_boottime_ns();
	do {
		ret = touch_comm_ctrl_seq_send(ctrl, size);
	} while (retry++ < max_retry && ret < 0 && ret != -EBADMSG
		&& ret != -EILSEQ && ret != -EPROTO && ret != -EREMOTEIO);

	duration = ktime_get_boottime_ns() - start_time;
	if (duration > timeout)
		pr_notice("running time %lld, cmd %u, retries %d\n",
			duration, ctrl->command, retry);
	return ret;
}

static int touch_comm_ctrl_send(struct touch_comm_ctrl *ctrl, unsigned int size)
{
	int ret = 0;

	mutex_lock(&touch_comm_lock);
	ret = touch_comm_ctrl_send_locked(ctrl, size);
	mutex_unlock(&touch_comm_lock);
	return ret;
}

/*
 * no need check scp_status due to send ready to scp.
 */
static int touch_comm_notify_status_bypass_locked(struct touch_comm_notify_status *notify)
{
	int ret = 0;

	notify->sequence = touch_comm_sequence;
	notify->crc8 =
		tiny_crc8((uint8_t *)notify, offsetof(typeof(*notify), crc8));
	ret = ipi_comm_noack(get_notify_id(), (unsigned char *)notify,
		sizeof(*notify));
	if (ret < 0)
		return ret;
	touch_comm_sequence++;
	return ret;
}

static int touch_comm_notify_status_bypass(struct touch_comm_notify_status *notify)
{
	int ret = 0;

	mutex_lock(&touch_comm_lock);
	ret = touch_comm_notify_status_bypass_locked(notify);
	mutex_unlock(&touch_comm_lock);
	return ret;
}
void touch_comm_data_notify(u_int8_t touch_type, u_int8_t touch_id, int cmd, void *data, uint8_t length)
{
	int ret = 0;
	struct touch_comm_notify_status *ts_scp_notify_status = NULL;
	uint32_t notify_size = 0;

	notify_size = ipi_comm_size(sizeof(*ts_scp_notify_status));
	ts_scp_notify_status = kzalloc(notify_size, GFP_KERNEL);

	ts_scp_notify_status->touch_id = touch_id;
	ts_scp_notify_status->touch_type = touch_type;
	ts_scp_notify_status->command = cmd;
	ts_scp_notify_status->length = length;
	if (length <= sizeof(ts_scp_notify_status->value)) {
        memcpy(ts_scp_notify_status->value, data, length);
	} else {
		ts_scp_err("the data length of cmd=%u to send too long", cmd);
		ret = -EIO;
	    goto error;
	}
	ret = touch_comm_notify_status_bypass(ts_scp_notify_status);
	if (ret < 0)
		ts_scp_err("notify ready to scp fail %d", ret);
	else
		ts_scp_info("need scp touch probe");
error:
	kfree(ts_scp_notify_status);
}

int touch_comm_cmd_send(uint8_t touch_type, uint8_t cmd, void *data, uint8_t length)
{
	int ret = 0;
	struct touch_comm_ctrl *ctrl = NULL;
	uint32_t ctrl_size = 0;

	ctrl_size = ipi_comm_size(sizeof(*ctrl));
	ctrl = kzalloc(ctrl_size, GFP_KERNEL);
	ctrl->touch_type = touch_type;
	ctrl->command = cmd;
	ctrl->length = length;
	ts_scp_info("ts_scp_comm_with ctrl_size=%d, cmd=0x%02x, length=%d",
		ctrl_size, cmd, length);
	if ((length <= sizeof(ctrl->data)) && (data != NULL)) {
		memcpy(ctrl->data, data, length);
	} else {
		ts_scp_err("the data length of cmd=%u to send too long", cmd);
		ret = -EIO;
	    goto error;
	}
	ret = touch_comm_ctrl_send(ctrl, ctrl_size);
error:
	kfree(ctrl);

	return ret;
}
void touch_comm_notify_handler_register(uint8_t cmd,
		void (*f)(struct touch_comm_notify *n, void *private_data),
		void *private_data)
{
	if (cmd >= MAX_TOUCH_COMM_NOTIFY_CMD)
		return;

	touch_notify_handle[cmd].private_data = private_data;
	touch_notify_handle[cmd].handler = f;
}

void touch_comm_notify_handler_unregister(uint8_t cmd)
{
	if (cmd >= MAX_TOUCH_COMM_NOTIFY_CMD)
		return;

	touch_notify_handle[cmd].handler = NULL;
	touch_notify_handle[cmd].private_data = NULL;
}

int touch_comm_init(void)
{
	touch_comm_sequence = 0;
	ipi_comm_init();
	ipi_comm_notify_handler_register(touch_comm_notify_handler);

	return 0;
}

void touch_comm_exit(void)
{
	ipi_comm_notify_handler_unregister();
	ipi_comm_exit();
}
