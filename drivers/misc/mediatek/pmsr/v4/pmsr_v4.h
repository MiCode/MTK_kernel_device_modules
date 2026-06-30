/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
//#include <linux/proc_fs.h>
//#include <linux/uaccess.h>

#ifndef __PMSR_H__
#define __PMSR_H__

#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#define MONTYPE_RISING			(0)
#define MONTYPE_FALLING			(1)
#define MONTYPE_HIGH_LEVEL		(2)
#define MONITPE_LOW_LEVEL		(3)

#define SPEED_MODE_NORMAL		(0x0)
#define SPEED_MODE_SPEED		(0x1)

#define DEFAULT_SELTYPE			(0)
#define DEFAULT_MONTYPE			MONTYPE_HIGH_LEVEL
#define DEFAULT_SPEED_MODE		SPEED_MODE_NORMAL

#define PMSR_PERIOD_MS           (1000)
#define WINDOW_LEN_SPEED         (PMSR_PERIOD_MS * 0x65B8)
#define WINDOW_LEN_NORMAL        (PMSR_PERIOD_MS * 0xD)
#define GET_EVENT_RATIO_SPEED(x)	((x)/(WINDOW_LEN_SPEED/1000))
#define GET_EVENT_RATIO_NORMAL(x)	((x)/(WINDOW_LEN_NORMAL/1000))

/* the max length of a signal name */
#define PMSR_SIGNAL_NAME_MAX_LEN (70)

/* the max signal numbers that user can select */
#define PMSR_MAX_SIG_CH          (400)
#define PMSR_DEFAULT_OUTPUT_LIMIT     (PMSR_MAX_SIG_CH)

/* define 2 buffer size for receiving message from the user space */
#define PMSR_BUF_WRITESZ         (512)
#define PMSR_BUF_WRITESZ_LARGE   (8192 - 1)

/* log header size for the runtime log */
#define PMSR_DEFAULT_RT_LOGBUF_HEADER (80)

/* The real rt log length that we can print
 * - The print buffer max length is 1024
 * - The print log header length is 48
 */
#define PMSR_RT_LOGBUF_LIMIT (1024 - 48)

/*
 * Reserve 7 characters for a UNIT in rt log.
 * A UNIT means "a data + 1 space + 1 reserved".
 * The data length =  5 if window length is 2s, tclk 32K,
 *                    the max tick = 64000  => needs 5 characters
 */
#define PMSR_RT_LOGBUF_UNIT (7)

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
enum APMCU_PMSR_SCMI_UUID {
	APMCU_SCMI_UUID_PMSR = 0,

	APMCU_SCMI_UUID_NUM,
};

enum PMSR_TOOL_ACT {
	PMSR_TOOL_ACT_ENABLE = 1,
	PMSR_TOOL_ACT_DISABLE,
	PMSR_TOOL_ACT_WINDOW,
	PMSR_TOOL_ACT_MONTYPE,
	PMSR_TOOL_ACT_SIGNUM,
	PMSR_TOOL_ACT_EN,
	PMSR_TOOL_ACT_TEST,
	PMSR_TOOL_ACT_SELTYPE,
	PMSR_TOOL_ACT_GET_SRAM,
	PMSR_TOOL_ACT_PROF_CNT_EN,
	PMSR_TOOL_ACT_GET_MBUF_LEN,
};

enum PMSR_MODE_FOR_MET {
	PMSR_MODE_FOR_MET_FR = 0,
	PMSR_MODE_FOR_MET_TO,
	PMSR_MODE_FOR_MET_CTS,
};
#endif

struct pmsr_dpmsr_cfg {
	unsigned int seltype;   /* dpmsr select type */
	unsigned int montype;	/* 2 bits, monitor type */
	unsigned int signum;
	unsigned int en;
};

struct pmsr_tool_mon_results {
	unsigned int timestamp_l;
	unsigned int timestamp_h;
	unsigned int winlen;
};

struct pmsr_tool_mon_results_addr {
	unsigned int data_addr;
	unsigned int data_num;
	unsigned int mon_res_addr;
};

struct pmsr_tool_results {
	unsigned int oldest_idx;
};

struct pmsr_cfg {
	struct pmsr_dpmsr_cfg *dpmsr;
	const char **signal_name;
	unsigned int pmsr_speed_mode;		/* 0: normal, 1: high speed */
	unsigned int pmsr_window_len;
	unsigned int pmsr_sample_rate;
	unsigned int dpmsr_count;
	unsigned int sig_count;
	unsigned int sig_limit;
	unsigned int met_cts_mode; /* custimized timestamp mode */
	unsigned int err;
	unsigned int test;
	unsigned int prof_cnt;
	unsigned int acc_sig_name_len;
	unsigned int mbuf_data_limit;
	unsigned int output_limit;
	unsigned int rt_logbuf_size;
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct pmsr_tool_mon_results_addr *share_buf;
	unsigned int pmsr_tool_buffer_max_space;
	struct pmsr_tool_results *pmsr_tool_share_results;
#endif
	bool enable;
	bool enable_hrtimer;
	/* pmsr_window_len
	 *	0: will be automatically updated for current speed mode
	 * for normal speed mode, set to WINDOW_LEN_NORMAL.
	 * for high speed mode, set to WINDOW_LEN_SPEED.
	 */
};
#endif /* __PMSR_H__ */

