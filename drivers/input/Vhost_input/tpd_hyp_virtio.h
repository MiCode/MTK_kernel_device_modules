/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef TPD_VIRTIO_H
#define TPD_VIRTIO_H

#define MAX_VIRTIO_SEND_BYTE 1056
#define MAX_VIRTIO_SEND_BYTE_TMP 200

int virtio_tpd_init(void);
void virtio_tpd_deinit(void);
long send_string_to_host(char *virtio_send_buffer);
long send_string_to_host_test (void);

#endif /*end of TPD_VIRTIO_H*/
