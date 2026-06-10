/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef CPUQOS_V3_H
#define CPUQOS_V3_H
#include <linux/ioctl.h>
#include "common.h"

struct _CPUQOS_V3_PACKAGE {
	__u32 mode;
	__u32 pid;
	__u32 set_task;
	__u32 group_id;
	__u32 set_group;
	__u32 user_pid;
	__u32 bitmask;
	__u32 set_user_group;
};

#define CPUQOS_V3_SET_CPUQOS_MODE		_IOW('g', 14, struct _CPUQOS_V3_PACKAGE)
#define CPUQOS_V3_SET_CT_TASK			_IOW('g', 15, struct _CPUQOS_V3_PACKAGE)
#define CPUQOS_V3_SET_CT_GROUP			_IOW('g', 16, struct _CPUQOS_V3_PACKAGE)
#define CPUQOS_V3_SET_TASK_TO_USER_GROUP	_IOW('g', 17, struct _CPUQOS_V3_PACKAGE)
#define CPUQOS_V3_SET_CCL_TO_USER_GROUP		_IOW('g', 18, struct _CPUQOS_V3_PACKAGE)

#define USER_GROUP_START_IDX 9

#endif

