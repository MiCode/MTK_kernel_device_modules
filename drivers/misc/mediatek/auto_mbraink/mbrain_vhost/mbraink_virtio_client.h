/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MBRAINK_VIRTIO_H
#define MBRAINK_VIRTIO_H

#define MAX_VIRTIO_BUFFER_BYTE 4096
#define MAX_VIRTIO_SEND_BYTE (MAX_VIRTIO_BUFFER_BYTE - 28)

int mbraink_hyp_virtio_init(void);
void mbraink_hyp_virtio_deinit(void);
int mbraink_hyp_sendMsg(u32 msg_length, void *msg_buffer);

extern int mbraink_netlink_send_msg(const char *msg); //EXPORT_SYMBOL_GPL

#endif /*end of MBRAINK_VIRTIO_H*/
