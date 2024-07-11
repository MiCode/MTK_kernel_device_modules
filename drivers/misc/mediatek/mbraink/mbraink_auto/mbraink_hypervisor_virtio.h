/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MBRAINK_HYPERVISOR_VIRTIO_H
#define MBRAINK_HYPERVISOR_VIRTIO_H

#define MAX_VIRTIO_SEND_BYTE 1024

int vhost_mbraink_init(void);
void vhost_mbraink_deinit(void);

extern int mbraink_netlink_send_msg(const char *msg);

#endif /*end of MBRAINK_HYPERVISOR_VIRTIO_H*/
