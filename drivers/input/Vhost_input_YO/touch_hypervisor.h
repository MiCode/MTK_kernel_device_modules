/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef TOUCH_VIRTIO_H
#define TOUCH_VIRTIO_H

#define MAX_VIRTIO_SEND_BYTE 256

int touch_event(char *tpd_info);
#endif /*end of TOUCH_VIRTIO_H*/
