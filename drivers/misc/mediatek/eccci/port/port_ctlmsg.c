// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/hashtable.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <mt-plat/mtk_ccci_common.h>
#include <linux/string.h>

#include "ccci_bm.h"
#include "port_cfg.h"
#include "ccci_config.h"
#include "ccci_common_config.h"
#include "ccci_core.h"
#include "ccci_fsm.h"
#include "ccci_hif.h"
#include "port_ctlmsg.h"
#include "port_proxy.h"


#define MAX_QUEUE_LENGTH 16

int add_new_runtime_cfg_port_t(struct port_runtime_config_info *port_info)
{
	struct port_t *new_port;
	static unsigned int dev_minor_id = 50;

	new_port = kzalloc(sizeof(struct port_t), GFP_KERNEL);
	if (!new_port) {
		CCCI_ERROR_LOG(0, PORT, "%s, Failed to kzalloc for new port\n", __func__);
		return -ENOMEM;
	}
	new_port->name = kzalloc(CCCI_PORT_NAME_LEN, GFP_KERNEL);
	if (!new_port->name) {
		CCCI_ERROR_LOG(0, PORT, "%s, Failed to kzalloc for new port name\n", __func__);
		kfree(new_port);
		return -ENOMEM;
	}

	new_port->tx_ch = port_info->ul_ch_id;
	new_port->rx_ch = port_info->dl_ch_id;
	new_port->txq_index = port_info->ul_hw_queue_id;
	new_port->rxq_index = port_info->dl_hw_queue_id;

	//for char port use 0xff exception queue id
	new_port->txq_exp_index = 0xff;
	new_port->rxq_exp_index = 0xff;
	new_port->hif_id = MD1_NORMAL_HIF;
	new_port->flags = port_info->flag | PORT_F_WITH_CHAR_NODE;
	new_port->ops = &char_port_ops;

	dev_minor_id++;
	if (dev_minor_id > MAX_PORT_DEV_MINOR) {
		CCCI_ERROR_LOG(0, PORT, "%s, exceed max dev minor id\n", __func__);
		kfree(new_port->name);
		kfree(new_port);
		return -ESPIPE;
	}
	new_port->minor = dev_minor_id;

	if (port_info->port_name_len + 1 > CCCI_PORT_NAME_LEN) {
		CCCI_ERROR_LOG(0, PORT, "%s, error port name len = %d\n",
			__func__, port_info->port_name_len);
		kfree(new_port->name);
		kfree(new_port);
		return -EINVAL;
	}
	scnprintf(new_port->name, port_info->port_name_len + 1, "%s", port_info->port_name);

	new_port->create_port_dev_flag = 1;

	CCCI_NORMAL_LOG(0, PORT, "%s, tx_ch=%u, rx_ch=%u, txq_index=%u, rxq_index=%u, node_type=%u, peer_id=%u, flag=%u, port_name_len=%u, name=%s\n",
			__func__, port_info->ul_ch_id, port_info->dl_ch_id, port_info->ul_hw_queue_id,
			port_info->dl_hw_queue_id, port_info->node_type, port_info->peer_id, port_info->flag,
			port_info->port_name_len, port_info->port_name);

	hash_add(unified_port_cfg_hash_tbl, &new_port->runtime_cfg_port_hnode, new_port->tx_ch);

	return 0;
}

/* When parsing a ul_ch info, use a hash table to quickly find the
 * corresponding port_t item and update its create_port_dev_flag.
 */
void update_or_add_port_t(struct port_runtime_config_info *port_info)
{
	struct port_t *port;
	bool port_find_in_legacy_tbl = false;

	hash_for_each_possible(unified_port_cfg_hash_tbl, port, runtime_cfg_port_hnode, port_info->ul_ch_id) {
		CCCI_NORMAL_LOG(0, PORT, "%s, finding port by tx_ch: %s\n", __func__, port_info->port_name);
		if (port_info->ul_ch_id == port->tx_ch) {
			port->create_port_dev_flag = 1;
			port_find_in_legacy_tbl = true;
			break;
			CCCI_NORMAL_LOG(0, PORT, "%s, find port: %s\n", __func__, port->name);
		}
	}

	/* for new port, if it's not found in legacy table,
	 * then add it in the unified_port_cfg_hash_tbl table.
	 */
	if (!port_find_in_legacy_tbl)
		add_new_runtime_cfg_port_t(port_info);
}

void runtime_port_device_init_work(struct work_struct *work)
{
	struct port_t *port;
	unsigned int bkt;

	hash_for_each(unified_port_cfg_hash_tbl, bkt, port, runtime_cfg_port_hnode) {
		if (port->create_port_dev_flag && port->ops && port->ops->init && port->name) {
			CCCI_NORMAL_LOG(0, PORT, "%s, Creating device for port: %s\n",
				__func__, port->name);
			port->ops->init(port);
		}
	}
}

/* unified port config handler
 * @param skb: the packet received
 * @return: 0 success, others failed
 */
static int unified_port_cfg_handler(struct port_t *ccci_ctrl_port, struct sk_buff *skb)
{
	int ret = 0;
	unsigned int i;
	struct ccci_header *ccci_h = (struct ccci_header *)skb->data;
	struct ccci_ctrl_header *ccci_ctl_h = NULL;
	struct port_runtime_config_header *runtime_port_cfg_h = NULL;
	struct port_runtime_config_info *runtime_port_cfg_info = NULL;
	unsigned int port_info_cnt = 0;

	/* MD->AP: ccci_h->channel should be 0x0. */
	if (ccci_h->channel != CCCI_CONTROL_RX) {
		CCCI_ERROR_LOG(0, PORT, "%s: err channel(%d)\n", __func__, ccci_h->channel);
		goto invalid_param; // write chl hw queue id == 0xff
	}

	/* ccci_h->data[1] == CCCI_HEADER_LEN + CCCI_CTRL_HEADER_LEN +
	 * PORT_RUNTIME_CONFIG_HEADER_LEN + length of multiple port runtime config info.
	 */
	if (ccci_h->data[1] < PORT_CFG_MSG_TOTAL_HDR_LEN) {
		CCCI_ERROR_LOG(0, PORT, "%s: invalid port cfg msg len(%d)\n",
			__func__, ccci_h->data[1]);
		goto invalid_param; // write chl hw queue id == 0xff
	}

	ccci_ctl_h = (struct ccci_ctrl_header *)((char *)ccci_h + CCCI_HEADER_LEN);
	runtime_port_cfg_h = (struct port_runtime_config_header *)((char *)ccci_ctl_h + CCCI_CTRL_HDR_LEN);
	runtime_port_cfg_info = (struct port_runtime_config_info *)((char *)runtime_port_cfg_h +
		PORT_RUNTIME_CFG_HDR_LEN);

	/* for port unified cfg msg, ccci_ctl_h->control_msg should be 11 */
	if (ccci_ctl_h->control_msg != CCCI_CTRL_MSG_PORT_UNIFIED_CFG) {
		CCCI_ERROR_LOG(0, PORT, "%s: invalid control message type %d\n",
			__func__, ccci_ctl_h->control_msg);
		goto invalid_param; // write chl hw queue id == 0xff
	}

	/* Check if the length carried in each header is correct：
	 * ccci_h->data[1] == sizeof(ccci_h) + sizeof(ccci_ctl_h) +
	 *     sizeof(port_runtime_cfg_hdr) + cnt*sizeof(port_runtime_cfg_info)
	 * ccci_ctl_h->length == sizeof(port_runtime_cfg_hdr) + cnt*sizeof(port_runtime_cfg_info)
	 * runtime_port_cfg_h->config_len == cnt * sizeof(port_runtime_cfg_info)
	 */
	if ((((ccci_h->data[1] - CCCI_HEADER_LEN - CCCI_CTRL_HDR_LEN) != ccci_ctl_h->length)) &&
		((ccci_ctl_h->length - PORT_RUNTIME_CFG_HDR_LEN) != runtime_port_cfg_h->config_len)) {
		CCCI_ERROR_LOG(0, PORT, "%s: invalid data length, %d -- %d -- %d\n",
			__func__, ccci_h->data[1], ccci_ctl_h->length, runtime_port_cfg_h->config_len);
		goto invalid_param;
	}

	/* one control msg include multi port info */
	port_info_cnt = runtime_port_cfg_h->config_len / PORT_RUNTIME_CFG_INFO_LEN;

	switch (runtime_port_cfg_h->config_type) {
	case CCCI_CHANNEL_PORT_UNIFIED_CFG:
		for (i = 0; i < port_info_cnt; i++) {
			runtime_port_cfg_info = (struct port_runtime_config_info *)((char *)runtime_port_cfg_info
				+ i * PORT_RUNTIME_CFG_INFO_LEN);
			update_or_add_port_t(runtime_port_cfg_info);
		}
		break;
	default:
		CCCI_ERROR_LOG(0, PORT, "%s: unknown unified port config type %d\n",
			__func__, runtime_port_cfg_h->config_type);
	}

	// send response to MD
	ccci_h->channel = 0x1; //AP->MD: ccci_h->channel should be 0x1
	runtime_port_cfg_h->msg_type = 0x2; //AP->MD: msg_type should be 0x2
	runtime_port_cfg_h->is_enable = 0x1;

	ret = ccci_hif_send_skb(ccci_ctrl_port->hif_id, ccci_ctrl_port->txq_index, skb, 0, 1);
	if (ret) {
		CCCI_ERROR_LOG(0, PORT, "%s: send skb fail %d\n",__func__, ret);
		goto invalid_param;
	}

	/* when ccci_ctl_h->send_again == 1, we need to create char dev for ports*/
	if (!ccci_ctl_h->send_again)
		schedule_work(&runtime_port_config_work);

	return 0;

invalid_param:
	return -CCCI_ERR_INVALID_PARAM;
}

static void control_msg_handler(struct port_t *port, struct sk_buff *skb)
{
	int ret = 0;
	struct ccci_header *ccci_h = (struct ccci_header *)skb->data;

	/* when ccci_h->data[0]==0, should be port unified config msg */
	if (ccci_h->data[0] == 0)
		ret = unified_port_cfg_handler(port, skb);
	else
		ret = ccci_fsm_recv_control_packet(skb);
	if (ret)
		CCCI_ERROR_LOG(0, PORT, "%s control msg gotten error: %d\n",
			port->name, ret);
}

static int port_ctl_init(struct port_t *port)
{
	CCCI_DEBUG_LOG(0, PORT,
		"kernel port %s is initializing\n", port->name);
	port->skb_handler = &control_msg_handler;
	port->private_data = kthread_run(port_kthread_handler,
		port, "%s", port->name);
	port->rx_length_th = MAX_QUEUE_LENGTH;
	port->skb_from_pool = 1;
	return 0;
}

struct port_ops ctl_port_ops = {
	.init = &port_ctl_init,
	.recv_skb = &port_recv_skb,
};
