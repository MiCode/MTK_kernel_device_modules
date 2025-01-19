/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef _TS_SCP_H_
#define _TS_SCP_H_

#include <linux/input.h>
#include "touch_comm.h"

#define MAX_CRASH_NUM                  (10)
#define MAX_CRASH_NUM_ONE_DURATION     (1)
#define TS_AFTER_SCP_REST_TIME         (10000000000)

enum ts_scp_touch_uniq_id {
    TOUCH_ID_INVALID = 0,
    TOUCH_ID_GT9895,
    TOUCH_ID_GT9916,
    TOUCH_ID_GT9896S,
    MAX_TOUCH_ID,
};

enum ts_scp_touch_type {
    TS_TYPE_INVALID = 0,
    TS_TYPE_PRIMARY,
    TS_TYPE_SECONDARY,
    MAX_TS_TOUCH_TYPE,
};

enum touch_comm_ctrl_cmd {
    TOUCH_COMM_CTRL_SCP_HANDLE_CMD, //SCP handle touch event
    TOUCH_COMM_CTRL_AP_HANDLE_CMD, //AP handle touch event
    TOUCH_COMM_CTRL_QUERY_SCP_STATUS_CMD, //query scp is ready for touch or not
    TOUCH_COMM_CTRL_SUSPEND_CMD, //notify suspend
    TOUCH_COMM_CTRL_RESUME_CMD, //notify resume
    TOUCH_COMM_CTRL_REINIT_CMD, //notify reinit
    TOUCH_COMM_CTRL_CHANGE_REPORT_RATE_CMD, //change report rate

    TOUCH_COMM_CTRL_STATUS_UPDATE,
    MAX_TOUCH_COMM_CTRL_CMD,
};

enum touch_comm_notify_cmd {
    TOUCH_COMM_NOTIFY_DATA_CMD,
    TOUCH_COMM_NOTIFY_READY_CMD,
    MAX_TOUCH_COMM_NOTIFY_CMD,
};

/*State Mask*/
#define STAT_NONE                       (0)
/*SCP Touch state bit 0-15*/
#define STAT_SCP_TP_INIT_READY          (1 << 0)
#define STAT_SCP_TP_WORK_STATE          (1 << 1)
#define STAT_TP_WORK_MODE               (1 << 2)
#define STAT_AP_TP_READY                (1 << 3)

/*SCP state bit 0-15*/
#define STAT_SCP_STATE_READY            (1 << 0)
#define STAT_SCP_STATE_CRASH_DURATION   (1 << 2)
#define STAT_SCP_STATE_CRASH_TOO_MUCH   (1 << 3)
/*State Mask*/

struct ts_scp_data {
    uint8_t touch_type;
    int32_t data[TOUCH_COMM_DATA_MAX];
};

struct ts_scp_cmd {
    uint8_t touch_type;
    uint8_t command;
    uint8_t data[TOUCH_COMM_CTRL_DATA_MAX];
};

struct tp_offload_scp {
    int (*offload_scp)(struct tp_offload_scp *tp, bool enable);
    void (*report_func)(void *data);

    char *name;
    uint8_t touch_type;
    uint8_t touch_id;
    void *private_data;
};

/* log macro */
extern bool debug_log_flag;
#define ts_scp_info(fmt, arg...) \
		pr_info("[TS-INF][%s:%d] "fmt"\n", __func__, __LINE__, ##arg)
#define	ts_scp_err(fmt, arg...) \
		pr_info("[TS-ERR][%s:%d] "fmt"\n", __func__, __LINE__, ##arg)
#define ts_scp_debug(fmt, arg...) \
		{if (debug_log_flag) \
		pr_info("[TS-DBG][%s:%d] "fmt"\n", __func__, __LINE__, ##arg);}

struct ts_scp_node_debug {
    bool debug_on;
};

struct ts_scp_node_ctrl {
    uint8_t touch_type;
    uint8_t cmd;
};

#define TS_SCP_NODE_DEBUG           _IOWR('a', 1, struct ts_scp_node_debug)
#define TS_SCP_NODE_CTRL            _IOWR('a', 2, struct ts_scp_node_ctrl)

int ts_scp_request_offload(struct tp_offload_scp *tp);
void ts_scp_release_offload(struct tp_offload_scp *tp);
int ts_scp_cmd_handler_sync(struct ts_scp_cmd *option_cmd);
void ts_scp_cmd_handler_async(struct ts_scp_cmd *option_cmd);
int ts_scp_offload_check_status(uint8_t touch_type);
int64_t ts_scp_generate_timestamp(int64_t scp_timestamp, int64_t scp_archcounter);
#endif
