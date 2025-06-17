/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __AISTE_IOCTL_H__
#define __AISTE_IOCTL_H__

#include <linux/fs.h>
#include <linux/ioctl.h> // for _IOWR
#include <linux/types.h> // for pid_t
#include <linux/uaccess.h> // for copy_from_user and copy_to_user
#include <linux/miscdevice.h>

#define AISTE_MAGIC 'Q'
#define AISTE_NAME "aiste"
#define MAX_THREAD_NUM_PER_QOS 32

struct aiste_ioctl_qos {
	uint16_t ddr_boost_value; // Input: 0-100
	uint16_t cpu_boost_value; // Input: 0-100
	uint16_t thread_count; // Input: number of threads
	pid_t thread_set[MAX_THREAD_NUM_PER_QOS];
};

struct aiste_ioctl_id {
	uint64_t qos_id; // Unique ID of the qos
};

struct aiste_ioctl_request {
	uint64_t qos_id; // Unique ID of the qos
	struct aiste_ioctl_qos qos;
};

#define AISTE_IOCTL_CREATE_QOS _IOWR(AISTE_MAGIC, 0, struct aiste_ioctl_id)
#define AISTE_IOCTL_REQUEST_QOS _IOWR(AISTE_MAGIC, 1, struct aiste_ioctl_request)
#define AISTE_IOCTL_DELETE_QOS _IOWR(AISTE_MAGIC, 2, struct aiste_ioctl_id)

struct miscdevice *aiste_get_misc_dev(void);

#endif /* __AISTE_IOCTL_H__ */
