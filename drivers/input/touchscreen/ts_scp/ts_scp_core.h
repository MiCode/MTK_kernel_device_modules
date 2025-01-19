/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef _TS_SCP_H_
#define _TS_SCP_H_

#include <linux/input.h>
#include "touch_comm.h"
#include "interface.h"
#include "scenario_handle.h"
#include "scp_rv.h"
#include "timesync.h"

#define MAX_SCP_FINGER_NUM	10
#define MAX_CRASH_NUM		10

enum ts_scp_point_status {
	TS_SCP_NONE = 0,
	TS_SCP_RELEASE,
	TS_SCP_TOUCH,
};

enum ts_scp_touch_uniq_id {
    TOUCH_ID_INVALID = 0,
    TOUCH_ID_GT9895,
    TOUCH_ID_GT9916,
    TOUCH_ID_GT9896S,
    MAX_TOUCH_ID,
};

enum ts_scp_touch_type {
	TS_UNIQ_NONE = 0,
	TS_UNIQ_ONE,
	TS_UNIQ_TWO,
	TS_UNIQ_THREE,
	MAX_TOUCH_UNIQ_ID,
};

enum ts_scp_ack_ret_value {
	TS_ACK_FAIL = 0,
	TS_ACK_PASS,
	MAX_ACK_VALUE,
};

enum ts_scp_finger_on_mode {
	TS_FINIGER_NONE = 0,
	TS_FINIGER_ENABLE_CMD,
	TS_FINIGER_READY_CMD,
	MAX_FINGER_MODE,
};

/* scp touch event data */
struct ts_scp_tp_data{
	int32_t finger_status;
	uint32_t position_x;
	uint32_t position_y;
	uint32_t major;
	uint32_t pressure;
};

struct ts_scp_tp_multi_data{
    int touch_num;
	int64_t scp_timestamp;
	int64_t scp_archcounter;
    struct ts_scp_tp_data coords[MAX_SCP_FINGER_NUM];
};
/*
struct ts_scp_tp_scenario{
	uint32_t scenario_flag;
	uint16_t scenario_prio;
	uint32_t scenario_status;
};
*/
struct ts_scp_data {
	struct ts_scp_tp_multi_data multi_data;
};

struct ts_scp_device {
	struct ts_scp_data *ts_data;
	struct ts_scp_ctrl *ctrl;
	struct task_struct *task;
	struct task_struct *task_ready;
	struct task_struct *task_scene;
	struct input_dev *input_dev;
	struct timesync_filter filter;

	int ts_scp_major;
	struct class *ts_scp_class;
	struct device *device;
	bool ts_scp_common_enable;
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
	uint8_t cmd;
};

#define TS_SCP_NODE_DEBUG           _IOWR('a', 1, struct ts_scp_node_debug)
#define TS_SCP_NODE_CTRL            _IOWR('a', 2, struct ts_scp_node_ctrl)

extern struct ts_scp_device ts_scp_dev;
extern struct touch_comm_notify_status ts_scp_notify_status;
extern void (*generic_report_func)(struct ts_scp_device *dev);
extern struct completion ts_scp_scene_done;

void connect_report_func(void (*report_func)(struct ts_scp_device *dev));
int generic_power_on_reinit(void);
void ts_scp_is_scp_touch_need_probe(void);
void ts_scp_set_touch_type_id(uint8_t type, uint8_t id);
int64_t ts_scp_timesync(struct ts_scp_device *device);

#endif
