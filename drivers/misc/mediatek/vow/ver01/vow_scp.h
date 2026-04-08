/* SPDX-License-Identifier: GPL-2.0 */
/*
 * vow_scp.h  --  VoW SCP definition
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

#ifndef __VOW_SCP_H__
#define __VOW_SCP_H__

#include <linux/types.h>

/* if IPI expand, need to modify maximum data length(unit: int) */

#define VOW_IPI_HEADER_LENGTH         (2)  /* 2 * 4byte = 8 */
#define VOW_IPI_SEND_BUFFER_LENGTH    (9)  /* 9 * 4byte = 36 */
#define VOW_IPI_RECEIVE_LENGTH        (20) /* 20 * 4byte = 80 */
#define VOW_IPI_ACK_LENGTH            (2)  /* 2 * 4byte = 8 */

#define VOW_IPI_WAIT_ACK_TIMEOUT      (10)
#define VOW_IPI_RESEND_TIMES          (2)

#define VOW_IPI_MAGIC_NUM             (0x98)
#define VOW_HEADER_LENGTH             (8)


enum {
	VOW_IPI_BYPASS_ACK = 0,
	VOW_IPI_NEED_ACK,
	VOW_IPI_ACK_BACK
};

enum {
	IPI_SCP_DIE = -1,
	IPI_SCP_SEND_FAIL = -2,
	IPI_SCP_NO_SUPPORT = -3,
	IPI_SCP_SEND_PASS = 0,
	IPI_SCP_RECOVERING = 1
};

/* AP -> SCP ipi structure */
struct vow_ipi_send_info {
	uint8_t msg_id;
	uint8_t msg_need_ack;
	uint8_t param1;
	uint8_t param2;
	uint32_t msg_length;
	uint32_t payload[VOW_IPI_SEND_BUFFER_LENGTH];
};

/* SCP -> AP ipi structure */
struct vow_ipi_receive_info {
	uint8_t msg_id;
	uint8_t msg_need_ack;
	uint8_t param1;
	uint8_t param2;
	uint32_t msg_length;
	uint32_t msg_data[VOW_IPI_RECEIVE_LENGTH];
};

/* SCP -> AP ipi ack structure */
struct vow_ipi_ack_info {
	uint8_t msg_id;
	uint8_t msg_need_ack;
	uint8_t param1;
	uint8_t param2;
	uint32_t msg_data;
};

unsigned int vow_check_scp_status(void);
void vow_ipi_register(void (*ipi_rx_call)(unsigned int, void *));
int vow_ipi_send(unsigned int msg_id,
		 unsigned int payload_len,
		 unsigned int *payload,
		 unsigned int need_ack);

#endif /*__VOW_SCP_H__ */
