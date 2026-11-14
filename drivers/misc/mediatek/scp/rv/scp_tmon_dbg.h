/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __SCP_TMON_DBG__
#define __SCP_TMON_DBG__

#include <linux/types.h>

#define SCP_TMON_DATA_MODULE_ID      9
#define SCP_TMON_DATA_VERSION        0
#define configMAX_TASK_NAME_LEN     16
#define HEADER_SIZE					8
#define MAX_TASKS					255

struct task_monitor_info {
    char name[configMAX_TASK_NAME_LEN];
    int loading;
    uint16_t watermark;
};

int scp_sys_tmon_mbrain_plat_init (void);

#endif
