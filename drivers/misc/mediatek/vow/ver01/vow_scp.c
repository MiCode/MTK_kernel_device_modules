// SPDX-License-Identifier: GPL-2.0
/*
 *  vow_scp.c  --  VoW SCP
 *
 *  Copyright (c) 2020 MediaTek Inc.
 *  Author: Michael HSiao <michael.hsiao@mediatek.com>
 */

/*****************************************************************************
 * Header Files
 *****************************************************************************/
#include <linux/delay.h>
#include "vow_scp.h"
#include "vow.h"
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
#include "scp_ipi.h"
#endif

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
static void vow_ipi_recv_handler(int id,
				void *data,
				unsigned int len);
#endif

unsigned int ipi_ack_return;
unsigned int ipi_ack_id;
unsigned int ipi_ack_data;

void (*ipi_rx_handle)(unsigned int a, void *b);

/*****************************************************************************
 * Function
 ****************************************************************************/
unsigned int vow_check_scp_status(void)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	return is_scp_ready(SCP_A_ID);
#else
	return 0;
#endif
}

void vow_ipi_register(void (*ipi_rx_call)(unsigned int, void *))
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	scp_ipi_registration(IPI_VOW, vow_ipi_recv_handler, "VOW_driver_device");
#endif
	ipi_rx_handle = ipi_rx_call;
}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
static void vow_ipi_recv_handler( int id,
				void *data,
				unsigned int len)
{
	struct vow_ipi_receive_info *ipi_info =
		(struct vow_ipi_receive_info *)data;

	struct vow_ipi_ack_info *ipi_ack_info;

	/* check magic num */
	if (ipi_info->param2 != VOW_IPI_MAGIC_NUM)
		VOWDRV_DEBUG("IPI Magic num no correct=%d\n", ipi_info->param2);

	VOWDRV_DEBUG("Recv IPI id=%d, need_ack=%d, msg_len=%d\n",
		     ipi_info->msg_id, ipi_info->msg_need_ack, ipi_info->msg_length);

	if (ipi_info->msg_need_ack == VOW_IPI_ACK_BACK) {
		// receive ack ipi from SCP
		ipi_ack_info = (struct vow_ipi_ack_info *)data;
		ipi_ack_return = ipi_ack_info->msg_need_ack;
		ipi_ack_id = ipi_ack_info->msg_id;
		ipi_ack_data = ipi_ack_info->msg_data;
	} else {
		// receive ipi from SCP
		ipi_rx_handle(ipi_info->msg_id, (void *)ipi_info->msg_data);
	}
}
#endif

int vow_ipi_send(unsigned int msg_id,
		 unsigned int payload_len,
		 unsigned int *payload,
		 unsigned int need_ack)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	int ret = IPI_SCP_SEND_FAIL;
	int ipi_result = -1;
	unsigned int retry_time = VOW_IPI_SEND_CNT_TIMEOUT;
	unsigned int retry_cnt = 0;
	unsigned int ack_time = VOW_IPI_WAIT_ACK_TIMEOUT;
	unsigned int ack_cnt = 0;
	unsigned int msg_need_ack = 0;
	unsigned int resend_cnt = 0;
	struct vow_ipi_send_info ipi_data;

	if (!vow_check_scp_status()) {
		VOWDRV_DEBUG("SCP is off, bypass send ipi id(%d)\n", msg_id);
		return IPI_SCP_DIE;
	}
	if (vow_service_GetScpRecoverStatus() == true) {
		VOWDRV_DEBUG("scp is recovering, then break, ipi id(%d)\n", msg_id);
		return IPI_SCP_RECOVERING;
	}

	/* clear send buffer */
	memset(&ipi_data.payload[0], 0,
		sizeof(unsigned int) * VOW_IPI_SEND_BUFFER_LENGTH);

	resend_cnt = 0;
	msg_need_ack = need_ack;
	ipi_data.msg_id = msg_id;
	ipi_data.msg_need_ack = msg_need_ack;
	ipi_data.param1 = 0;
	ipi_data.param2 = VOW_IPI_MAGIC_NUM;
	ipi_data.msg_length = payload_len;
	if (payload > 0) {
		/* have payload */
		memcpy(&ipi_data.payload[0], payload,
		       sizeof(unsigned int) * payload_len);
	}

RESEND_IPI:
	if (resend_cnt == VOW_IPI_RESEND_TIMES) {
		VOWDRV_DEBUG("%s(), resend over time, drop ipi id:%d\n",
			__func__, msg_id);
		return IPI_SCP_SEND_FAIL;
	}
	/* ipi ack reset */
	ipi_ack_return = 0;
	ipi_ack_id = 0xFF;
	ipi_ack_data = 0;

	for (retry_cnt = 0; retry_cnt <= retry_time; retry_cnt++) {
		if (!vow_check_scp_status()) {
			VOWDRV_DEBUG("SCP is off, bypass send ipi id(%d)\n", msg_id);
			return IPI_SCP_DIE;
		}
		ipi_result = scp_ipi_send(IPI_VOW,&ipi_data,
			sizeof(unsigned int) * payload_len + VOW_HEADER_LENGTH , 0 ,SCP_A_ID);
		if (ipi_result == SCP_IPI_DONE) {
			if (retry_cnt != 0) {
				VOWDRV_DEBUG("%s(), ipi_id(%d) succeed after retry cnt =%d\n",
						 __func__,
						 msg_id,
						 retry_cnt);
			}
			break;
		}
		if (retry_cnt == retry_time) { // already retry max times
			VOWDRV_DEBUG("%s() ERROR, ipi_id(%d) Fail %d after retry cnt =%d\n",
				__func__,
				msg_id,
				ipi_result,
				retry_cnt);
		}
		if (vow_service_GetScpRecoverStatus() == true) {
			VOWDRV_DEBUG("scp is recovering, then break, ipi id(%d)\n", msg_id);
			ret = IPI_SCP_RECOVERING;
			break;
		}
		msleep(VOW_WAITCHECK_INTERVAL_MS);
	}

	if (ipi_result == IPI_ACTION_DONE) {
		if (need_ack == VOW_IPI_NEED_ACK) {
			for (ack_cnt = 0; ack_cnt <= ack_time; ack_cnt++) {
				if ((ipi_ack_return == VOW_IPI_ACK_BACK) &&
				    (ipi_ack_id == msg_id)) {
					/* ack back */
					break;
				}
				if (ack_cnt >= ack_time) {
					/* no ack */
					VOWDRV_DEBUG("%s(), no ack\n",
						__func__);
					resend_cnt++;
					goto RESEND_IPI;
				}
				msleep(VOW_WAITCHECK_INTERVAL_MS);
			}
		}
		VOWDRV_DEBUG("%s(), ipi_id(%d) pass\n", __func__, msg_id);
		ret = IPI_SCP_SEND_PASS;
	} else {
		// IPI fail log
		VOWDRV_DEBUG("%s(), ipi_id(%d) fail, ipi_res=%d, res=%d\n",
			     __func__, msg_id, ipi_result, ret);
	}
	return ret;
#else
	(void) msg_id;
	(void) payload_len;
	(void) payload;
	(void) need_ack;
	VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
	return IPI_SCP_NO_SUPPORT;
#endif
}
EXPORT_SYMBOL_GPL(vow_ipi_send);
