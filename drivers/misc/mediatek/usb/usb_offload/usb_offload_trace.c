// SPDX-License-Identifier: GPL-2.0
/*
 * MTK USB Offload Trace Support
 * *
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Yu-chen.Liu <yu-chen.liu@mediatek.com>
 */

#define CREATE_TRACE_POINTS

#include <linux/platform_device.h>
#include <linux/of_device.h>

#include "../usb_xhci/xhci.h"
#include "audio_task_manager.h"
#include "audio_task_usb_msg_id.h"
#include "usb_offload.h"
#include "usb_offload_trace.h"

unsigned int enable_trace;
module_param(enable_trace, uint, 0644);
MODULE_PARM_DESC(enable_trace, "Enable/Disable Audio Data Trace");

/* export for u_logger to register trace point */
EXPORT_TRACEPOINT_SYMBOL_GPL(usb_offload_trace_trigger);

#define TRACE_SUPPORT_OFT  (0)
#define TRACE_START_OFT    (1)
#define TRACE_OUT_INIT_OFT (2)
#define TRACE_IN_INIT_OFT  (3)
#define TRACE_SUPPORT      (1 << 0)
#define TRACE_START        (1 << 1)
#define TRACE_OUT_INIT     (1 << 2)
#define TRACE_IN_INIT      (1 << 3)

struct stream_info {
	int direction;
	u16 slot;
	u16 ep;
	struct uo_buffer *payload;
	struct usb_endpoint_descriptor desc;
	void *transfer_buffer;
	int actual_length;
};

struct trace_manager {
	struct device *dev;
	struct stream_info stream_in;
	struct stream_info stream_out;
	u8 flag;
} manager;

#define chk_flag(mag, bit)   ((mag)->flag & bit)
#define set_flag(mag, bit)   ((mag)->flag |= bit)
#define clr_flag(mag, bit)   ((mag)->flag &= ~(bit))

static void stop_trace_stream(struct stream_info *stream);
static int usb_offload_trace_trigger(struct trace_manager *mag,
		struct stream_info *stream);

static inline void stream_init(struct stream_info *stream, bool is_in)
{
	stream->direction = is_in ? 1 : 0;
	stream->payload = NULL;
	stream->transfer_buffer = NULL;
	stream->actual_length = 0;
	memset(&stream->desc, 0, sizeof(struct usb_endpoint_descriptor));
}

static inline struct stream_info *get_stream(struct trace_manager *mag, bool is_in)
{
	return is_in ? &mag->stream_in : &mag->stream_out;
}

static inline void dump_manager_flag(struct trace_manager *mag)
{
	USB_OFFLOAD_DBG("support:%d start:%d in:%d out:%d\n",
		chk_flag(mag, TRACE_SUPPORT) >> TRACE_SUPPORT_OFT,
		chk_flag(mag, TRACE_START) >> TRACE_START_OFT,
		chk_flag(mag, TRACE_IN_INIT) >> TRACE_IN_INIT_OFT,
		chk_flag(mag, TRACE_OUT_INIT) >> TRACE_OUT_INIT_OFT);
}

int usb_offload_debug_init(struct usb_offload_dev *udev)
{
	struct trace_manager *mag = &manager;

	mag->dev = udev->dev;
	mag->flag = 0;

	stream_init(&mag->stream_in, true);
	stream_init(&mag->stream_out, false);
	set_flag(mag, TRACE_SUPPORT);
	clr_flag(mag, TRACE_START);
	clr_flag(mag, TRACE_IN_INIT);
	clr_flag(mag, TRACE_OUT_INIT);

	dump_manager_flag(mag);
	USB_OFFLOAD_INFO("debug init success\n");

	return 0;
}

static int stream_start(struct uo_buffer *trace_buffer,
	u16 slot, u16 ep, bool is_in, struct usb_endpoint_descriptor *desc)
{
	struct trace_manager *mag = &manager;
	struct stream_info *stream;

	if (!chk_flag(mag, TRACE_SUPPORT))
		return -EOPNOTSUPP;

	if (!desc || !trace_buffer || !trace_buffer->allocated || !trace_buffer->phys) {
		USB_OFFLOAD_ERR("stream init fail, dir:%d is_init:%d\n",
			is_in, chk_flag(mag, TRACE_IN_INIT));
		return -EINVAL;
	}

	stream = get_stream(mag, is_in);
	stream->payload = trace_buffer;
	stream->slot = slot;
	stream->ep = ep;
	memcpy(&stream->desc, desc, sizeof(struct usb_endpoint_descriptor));

	USB_OFFLOAD_INFO("is_in:%d ep_type:%d trace_buffer:0x%llx\n",
		is_in, usb_endpoint_type(desc), trace_buffer->phys);

	if (is_in)
		set_flag(mag, TRACE_IN_INIT);
	else
		set_flag(mag, TRACE_OUT_INIT);

	if (!chk_flag(mag, TRACE_START))
		set_flag(mag, TRACE_START);

	dump_manager_flag(mag);
	return 0;
}

static int stream_stop(struct stream_info *stream)
{
	struct trace_manager *mag = &manager;
	bool is_in;

	if (!stream)
		return -EINVAL;

	is_in = stream->direction;

	USB_OFFLOAD_INFO("is_in:%d ep_type:%d\n", is_in, usb_endpoint_type(&stream->desc));

	if (mtk_offload_free_mem(stream->payload))
		USB_OFFLOAD_ERR("fail to free trace_buffer, dir:%d", stream->direction);

	stream->payload = NULL;
	stream->actual_length = 0;
	memset(&stream->desc, 0, sizeof(struct usb_endpoint_descriptor));

	if (is_in)
		clr_flag(mag, TRACE_IN_INIT);
	else
		clr_flag(mag, TRACE_OUT_INIT);

	if (!chk_flag(mag, TRACE_IN_INIT) && !chk_flag(mag, TRACE_OUT_INIT))
		clr_flag(mag, TRACE_START);

	dump_manager_flag(mag);
	return 0;
}

static int usb_offload_trace_trigger(struct trace_manager *mag,
		struct stream_info *stream)
{
	bool is_in;

	if (!mag || !stream)
		return -EINVAL;

	is_in = (stream->direction == 1);

	if (!chk_flag(mag, TRACE_SUPPORT))
		return -EOPNOTSUPP;

	if ((is_in && !chk_flag(mag, TRACE_IN_INIT)) ||
		(!is_in && !chk_flag(mag, TRACE_OUT_INIT)))
		return -EINVAL;

	USB_OFFLOAD_DBG("stream:%p buffer:%p len:%d des:%p is_in:%d\n",
		stream, stream->transfer_buffer,
		stream->actual_length, &stream->desc, is_in);

	trace_usb_offload_trace_trigger((void *)stream,
		stream->transfer_buffer, stream->actual_length,
		stream->slot, stream->ep, &stream->desc);

	return 0;
}

void usb_offload_ipi_trace_handler(void *param)
{
	struct ipi_msg_t *ipi_msg = NULL;
	struct trace_manager *mag = &manager;
	struct stream_info *stream;
	u32 length;
	bool direction;

	ipi_msg = (struct ipi_msg_t *)param;
	if (!ipi_msg) {
		USB_OFFLOAD_ERR("null ipi_msg\n");
		return;
	}

	if (!chk_flag(mag, TRACE_START)) {
		USB_OFFLOAD_ERR("trace doesn't start\n");
		return;
	}

	switch (ipi_msg->msg_id) {
	case AUD_USB_MSG_D2A_TRACE_URB:
		/* message only
		 * @param1: direction
		 * @param2: payload length
		 */
		direction = (bool)ipi_msg->param1;
		length = (u32)ipi_msg->param2;

		stream = direction ? &mag->stream_in : &mag->stream_out;
		stream->transfer_buffer = (void *)stream->payload->virt;
		stream->actual_length = length;

		if (stream->transfer_buffer)
			usb_offload_trace_trigger(mag, stream);

		break;
	default:
		break;
	}

}

void usb_offload_trace_start(struct usb_audio_stream_msg *msg)
{
	struct usb_trace_msg trace_msg = {0};
	struct trace_manager *mag = &manager;
	struct usb_endpoint_descriptor *desc;
	struct stream_info *stream;
	struct uo_buffer *trace_buffer;
	u16 slot, ep;
	int dir;

	if (!chk_flag(mag, TRACE_SUPPORT)) {
		USB_OFFLOAD_INFO("trace was unsupported\n");
		goto error;
	}

	if (!enable_trace) {
		USB_OFFLOAD_INFO("trace was disabled\n");
		goto error;
	}

	if (!msg)
		goto error;

	dir = msg->direction;
	stream = get_stream(mag, dir);
	if (unlikely(!stream))
		goto error;

	if ((dir && chk_flag(mag, TRACE_IN_INIT)) ||
		(!dir && chk_flag(mag, TRACE_OUT_INIT))) {
		USB_OFFLOAD_ERR("stream was already enabled, dir:%d\n", stream->direction);
		goto error;
	}

	desc = &msg->data_ep_info.desc;
	slot = msg->slot_id;
	ep = xhci_get_endpoint_index_(desc);

	USB_OFFLOAD_INFO("slot:%d ep:%d direction:%d\n", slot, ep, dir);

	trace_buffer = uob_get_empty(UO_STRUCT_URB);
	if (!trace_buffer) {
		USB_OFFLOAD_ERR("insufficent on %s array\n", uo_struct_name(UO_STRUCT_URB));
		goto error;
	}

	/* allocate trace buffer */
	if (mtk_offload_alloc_mem(trace_buffer, msg->data_ep_info.urb_size, 64,
			UO_PROV_DRAM, UO_STRUCT_URB, true)) {
		USB_OFFLOAD_ERR("fail to allocate trace_buffer, dir:%d\n", dir);
		goto error;
	}

	/* init trace_stream */
	if (stream_start(trace_buffer, slot, ep, dir, desc)) {
		USB_OFFLOAD_ERR("init stream fail\n");
		if (mtk_offload_free_mem(trace_buffer))
			USB_OFFLOAD_ERR("fail to free trace_buffer\n");
		goto error;
	}

	/* prepare trace_msg */
	trace_msg.enable = 1;
	trace_msg.disable_all = 0;
	trace_msg.direction = dir;
	trace_msg.buffer = (unsigned long long)trace_buffer->phys;
	trace_msg.size = (unsigned int)trace_buffer->size;

	/* send IPI_MSG(enable_trace:1) */
	if (usb_offload_send_ipi_msg(UOI_ENABLE_TRACE, &trace_msg, sizeof(struct usb_trace_msg)))
		stop_trace_stream(stream);
error:
	return;
}

void usb_offload_trace_stop(int dir, bool skip_ipi)
{
	struct usb_trace_msg trace_msg = {
		.direction = dir,
	};
	struct trace_manager *mag = &manager;
	struct stream_info *stream;
	int retval = 0;

	if (!chk_flag(mag, TRACE_SUPPORT)) {
		USB_OFFLOAD_INFO("trace was unsupported\n");
		goto error;
	}

	if (!enable_trace) {
		USB_OFFLOAD_INFO("trace was disabled\n");
		goto error;
	}

	stream = get_stream(mag, dir);
	if (unlikely(!stream))
		goto error;

	if ((stream->direction && !chk_flag(mag, TRACE_IN_INIT)) ||
		(!stream->direction && !chk_flag(mag, TRACE_OUT_INIT))) {
		USB_OFFLOAD_ERR("stream was already disabled, dir:%d\n", stream->direction);
		goto error;
	}

	if (!skip_ipi)
		/* send IPI_MSG(enable_trace:0) */
		retval = usb_offload_send_ipi_msg(UOI_DISABLE_TRACE, &trace_msg, sizeof(struct usb_trace_msg));

	if (!retval)
		stop_trace_stream(stream);

error:
	return;
}

void usb_offload_trace_stop_all(void)
{
	struct usb_trace_msg trace_msg = {0};
	struct trace_manager *mag = &manager;

	if (!chk_flag(mag, TRACE_SUPPORT)) {
		USB_OFFLOAD_INFO("trace was unsupported\n");
		goto error;
	}

	if (!enable_trace) {
		USB_OFFLOAD_INFO("trace was disabled\n");
		goto error;
	}

	if (!chk_flag(mag, TRACE_IN_INIT) && !chk_flag(mag, TRACE_OUT_INIT)) {
		USB_OFFLOAD_ERR("all stream was disabled\n");
		goto error;
	}

	trace_msg.enable = 0;
	trace_msg.disable_all = 1;
	trace_msg.direction = 0;
	trace_msg.buffer = 0;
	trace_msg.size = 0;

	if (usb_offload_send_ipi_msg(UOI_DISABLE_TRACE, &trace_msg, sizeof(struct usb_trace_msg)))
		goto error;
	stop_trace_stream(&mag->stream_in);
	stop_trace_stream(&mag->stream_out);
error:
	return;
}

static void stop_trace_stream(struct stream_info *stream)
{
	if (!stream)
		return;

	if (stream_stop(stream))
		USB_OFFLOAD_ERR("fail to deinit trace_stream, dir:%d", stream->direction);
}
