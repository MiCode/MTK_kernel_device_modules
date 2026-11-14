/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 MediaTek Inc.
 */


#ifndef __CCCI_MBRAIN_H__
#define __CCCI_MBRAIN_H__

//  the header file is for M-brain, do not move/change the content

enum CCCI_MBRAIN_EVENT_TYPE {
	CCCI_MBRAIN_EVENT_INVALID = 0,
	CCCI_MBRAIN_EVENT_FSM_POLL,
	CCCI_MBRAIN_EVENT_MAX,
};

// for CCCI_MBRAIN_EVENT_FSM_POLL
struct fsm_poll_data {
	int version;                    // if fsm_poll_data adds new information, you can identify by this filed
	char key_info[256];             // record some character information that you want to save
	unsigned long long time_stamp;  // send heartbeat packet time
	int cost_time;                  // how long does the heartbeat packet take to respond
};


typedef int (*ccci_mbrain_event_notify_func_t)(enum CCCI_MBRAIN_EVENT_TYPE event_type, void *ccci_mbrain_data);

int ccci_mbrain_register(ccci_mbrain_event_notify_func_t callback);
int ccci_mbrain_unregister(void);

#endif // __CCCI_MBRAIN_H__
