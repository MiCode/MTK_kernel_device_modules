// SPDX-License-Identifier: GPL-2.0
/*
 * MTK USB Offload IPI
 * *
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Yu-chen.Liu <yu-chen.liu@mediatek.com>
 */

#include "usb_offload.h"
#include "audio_task_usb_msg_id.h"
#include "audio_task_manager.h"

static bool register_ipi_receiver;

static void usb_offload_ipi_recv(struct ipi_msg_t *ipi_msg)
{
	if (!ipi_msg) {
		USB_OFFLOAD_ERR("%s null ipi_msg\n", __func__);
		return;
	}

	USB_OFFLOAD_DBG("msg_id:0x%x\n", ipi_msg->msg_id);

	switch (ipi_msg->msg_id) {
	case AUD_USB_MSG_D2A_XHCI_IRQ:
	case AUD_USB_MSG_D2A_XHCI_EP_INFO:
		usb_offload_ipi_hid_handler((void *)ipi_msg);
		break;
	case AUD_USB_MSG_D2A_TRACE_URB:
		usb_offload_ipi_trace_handler((void *)ipi_msg);
		break;
	default:
		USB_OFFLOAD_ERR("unknown msg_id:0x%x\n", ipi_msg->msg_id);
		break;
	}
}

void usb_offload_register_ipi_recv(void)
{
	if (!register_ipi_receiver) {
		audio_task_register_callback(
			TASK_SCENE_USB_DL, usb_offload_ipi_recv);
		register_ipi_receiver = true;
	}
}

static int send_multi_scene(uint8_t data_type, uint8_t ack_type,
	uint16_t msg_id, uint32_t param1, uint32_t param2, void *data_buffer)
{
	struct ipi_msg_t ipi_msg;
	uint8_t scene = 0;
	int send_result = 0;

	for (scene = TASK_SCENE_USB_DL; scene <= TASK_SCENE_USB_UL; scene++) {
		send_result = audio_send_ipi_msg(
			&ipi_msg, scene, AUDIO_IPI_LAYER_TO_DSP, data_type,
			ack_type, msg_id, param1, param2, data_buffer);

		if (send_result)
			break;
	}
	return send_result;
}

static int send_single_scene(uint8_t scene, uint8_t data_type, uint8_t ack_type,
	uint16_t msg_id, uint32_t param1, uint32_t param2, void *data_buffer)
{
	struct ipi_msg_t ipi_msg;

	return audio_send_ipi_msg(
			&ipi_msg, scene, AUDIO_IPI_LAYER_TO_DSP, data_type,
			ack_type, msg_id, param1, param2, data_buffer);
}

char *msg_string[UOI_NUM] = {"INIT_ADSP", "DEINIT_ADSP", "ENABLE_STREAM",
	"DISABLE_STREAM", "ENABLE_HID", "DISABLE_HID", "ENABLE_TRACE", "DISABLE_TRACE"};

int usb_offload_send_ipi_msg(enum usb_offload_ipi_msg msg_type, void *data, size_t size)
{
	uint8_t scene = TASK_SCENE_INVALID;
	uint8_t data_type;
	uint8_t ack_type;
	uint16_t msg_id;
	uint32_t param1 = size;
	int ret = 0;
	size_t desire;
	bool ignore_timeout = true;

	if (msg_type >= UOI_NUM) {
		USB_OFFLOAD_ERR("invalid msg_type:%d\n", msg_type);
		return -EINVAL;
	}

	switch (msg_type) {
	case UOI_INIT_ADSP:
		desire = sizeof(struct mem_info_xhci);
		if (param1 != desire || data == NULL)
			goto sz_fail;
		data_type = AUDIO_IPI_PAYLOAD;
		ack_type = AUDIO_IPI_MSG_NEED_ACK;
		msg_id = AUD_USB_MSG_A2D_INIT_ADSP;
		break;
	case UOI_DEINIT_ADSP:
		desire = 0;
		if (param1 != desire || data != NULL)
			goto sz_fail;
		data_type = AUDIO_IPI_MSG_ONLY;
		ack_type = AUDIO_IPI_MSG_NEED_ACK;
		msg_id = AUD_USB_MSG_A2D_DISCONNECT;
		break;
	case UOI_ENABLE_STREAM:
	case UOI_DISABLE_STREAM:
	{
		struct usb_audio_stream_msg *uas_msg = (struct usb_audio_stream_msg *)data;

		desire = sizeof(struct usb_audio_stream_msg);
		if (param1 != desire || data == NULL)
			goto sz_fail;
		scene = uas_msg->uainfo.direction == 0 ? TASK_SCENE_USB_DL : TASK_SCENE_USB_UL;
		data_type = AUDIO_IPI_DMA;
		ack_type = AUDIO_IPI_MSG_NEED_ACK;
		msg_id = AUD_USB_MSG_A2D_ENABLE_STREAM;
		break;
	}
	case UOI_ENABLE_HID:
	case UOI_DISABLE_HID:
		desire = sizeof(struct usb_offload_urb_msg);
		if (param1 != desire || data == NULL)
			goto sz_fail;
		scene = TASK_SCENE_USB_DL;
		data_type = AUDIO_IPI_DMA;
		ack_type = AUDIO_IPI_MSG_BYPASS_ACK;
		msg_id = AUD_USB_MSG_A2D_ENABLE_HID;
		break;
	case UOI_ENABLE_TRACE:
	case UOI_DISABLE_TRACE:
		desire = sizeof(struct usb_trace_msg);
		if (param1 != desire || data == NULL)
			goto sz_fail;
		scene = TASK_SCENE_USB_DL;
		data_type = AUDIO_IPI_PAYLOAD;
		ack_type = AUDIO_IPI_MSG_NEED_ACK;
		msg_id = AUD_USB_MSG_A2D_ENABLE_TRACE_URB;
		break;
	default:
		break;
	}

	if (scene == TASK_SCENE_INVALID)
		ret = send_multi_scene(data_type, ack_type,	msg_id, param1, 0, data);
	else
		ret = send_single_scene(scene, data_type, ack_type, msg_id, param1, 0, data);

	if (ret == -ETIMEDOUT) {
		USB_OFFLOAD_ERR("<IPI:%s> timeout, ret:%d, ignore_timeout:%d\n",
			msg_string[msg_type], ret, ignore_timeout);

		/* although it took too long, it still succeed */
		if (ignore_timeout)
			ret = 0;
	} else if (ret == 0)
		USB_OFFLOAD_INFO("<IPI:%s> success to send\n", msg_string[msg_type]);
	else
		USB_OFFLOAD_ERR("<IPI:%s> fail to send, ret:%d\n", msg_string[msg_type], ret);

	return ret;

sz_fail:
	USB_OFFLOAD_ERR("<IPI:%s> sz_fail, size:%zu desire:%zu\n", msg_string[msg_type], size, desire);
	return -EINVAL;
}