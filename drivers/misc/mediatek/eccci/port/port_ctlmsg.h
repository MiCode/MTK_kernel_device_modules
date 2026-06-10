/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __PORT_CTLMSG_H__
#define __PORT_CTLMSG_H__

#include "ccci_core.h"
#include "port_t.h"

/* used to determine ccci_ctrl_header->control_msg, where a value of 11
 * indicates  that it is a message for unified port configuration.
 */
#define CCCI_CTRL_MSG_PORT_UNIFIED_CFG	11

enum unified_port_config_type {
	CCCI_RPC_PORT_UNIFIED_CFG = 1,
	CCCI_FS_PORT_UNIFIED_CFG,
	CCCI_SYSMSG_PORT_UNIFIED_CFG,
	CCCI_CHANNEL_PORT_UNIFIED_CFG,
	CCCI_HIF_PORT_UNIFIED_CFG,
};

#define CCCI_CTRL_HDR_LEN		sizeof(struct ccci_ctrl_header)
struct ccci_ctrl_header {
	u32 control_msg;
	u8 version;
	u8 send_again:1;
	u8 reserve:7;
	u8 reserve2[2];
	u32 length;
} __packed;

#define PORT_RUNTIME_CFG_HDR_LEN	sizeof(struct port_runtime_config_header)
#define PORT_CFG_MSG_TOTAL_HDR_LEN	(CCCI_HEADER_LEN + CCCI_CTRL_HDR_LEN + PORT_RUNTIME_CFG_HDR_LEN)

/* struct port_runtime_config_header - Message from MD to unified ports configuration */
struct port_runtime_config_header {
	u16 config_len;  // total length of runtime in this packet
	u8 config_type;  // 1:RPC / 2:FS / 3:SYSMSG / 4:Channel / 5:hif.
	u8 msg_type;     // 1:request(MD->AP) / 2:response(AP->MD).
	u8 is_enable;    // 0:disable / 1:enable.
	u8 version;      // defatult 1.
	u8 reserve[2];   // reserve bytes.
} __packed;

#define PORT_RUNTIME_CFG_INFO_LEN	sizeof(struct port_runtime_config_info)
struct port_runtime_config_info {
	u16 dl_ch_id;
	u16 ul_ch_id;
	u8 dl_hw_queue_id;
	u8 ul_hw_queue_id;
	u8 reserved[2];
	u8 node_type;
	u8 peer_id;
	u8 flag;
	u8 port_name_len;
	char port_name[CCCI_PORT_NAME_LEN];
} __packed;


/****************************************************************************/
/* External API Region called by port ctl object */
/****************************************************************************/
void runtime_port_device_init_work(struct work_struct *work);

/* max support port_t members: 2^7 = 128 */
#define RUNTIME_CFG_PORT_CFG_HASH_TABLE_BITS 7
extern DECLARE_HASHTABLE(unified_port_cfg_hash_tbl, RUNTIME_CFG_PORT_CFG_HASH_TABLE_BITS);
extern struct work_struct runtime_port_config_work;

#endif	/* __PORT_CTLMSG_H__ */
