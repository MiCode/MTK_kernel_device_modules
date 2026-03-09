/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2024, X-Ring technologies Inc., All rights reserved.
 */
#ifndef __XSWAPD_H
#define __XSWAPD_H

#include <linux/cpuhotplug.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

enum xswapd_stop_ctrl {
	XSWAPD_STOP_RECLAIM = 0,
	XSWAPD_STOP_WB,
};
struct xswapd_data {
	struct task_struct *xswapd;

	wait_queue_head_t wait;
	atomic_t wait_flag;
	unsigned long wake_time;
	unsigned int wake_interval;
	bool dry_run;

	unsigned long shrink_time;
	unsigned long shrink_nr;

	unsigned long last_fault;
	unsigned long fault_time;
	struct delayed_work dwork;
};

#define PGDAT_OEM_DATA(pgdat) \
	((struct xswapd_data *)(pgdat)->android_oem_data1)

enum xswapd_pressure_state {
	XSWAPD_PS_MIN = 0,
	XSWAPD_PS_LOW,
	XSWAPD_PS_HIGH,
	NR_XSWAPD_PRESS_STATE
};

/*
 * memory related unit:	MB
 * time related unit:	ms
 * nr related unit:	page numbers
 * refault_ratio unit:	pages per second
 * wb limit unit:	byte
 */
struct xswapd_ctrl {
	atomic_t mem_press_watermark;
	atomic_t mem_high_watermark;
	atomic_t mem_low_watermark;
	atomic_t zram_press_watermark;
	atomic_t reclaim_size;

	atomic_t wake_interval;
	atomic_t dry_run_wake_interval;
	atomic_t dry_run_reclaim_nr;

	atomic_t memcg_shrink_interval;
	atomic_t shrink_interval;
	atomic_t shrink_nr;

	atomic_t fault_interval;
	atomic_t refault_ratio;

	atomic64_t wb_limit;

	struct eventfd_ctx *press_eventfd[NR_XSWAPD_PRESS_STATE];
	enum cpuhp_state xswapd_cpuhp_state;
	unsigned int stop_swap;
	bool enable;
	bool umrenable;
};

#define PAGES_PER_MB_SHIFT		(20 - PAGE_SHIFT)

#define DEF_MEM_PRESS_WATERMARK		3000
#define DEF_MEM_HIGH_WATERMARK		3000
#define DEF_MEM_LOW_WATERMARK		2500
#define DEF_ZRAM_PRESS_WATERMARK	75
#define DEF_RECLAIM_SIZE		100

#define DEF_WAKE_INTERVAL		1000
#define DEF_DRY_RUN_WAKE_INTERVAL	20
#define DEF_DRY_RUN_RECLAIM_NR		10

#define DEF_MEMCG_SHRINK_INTERVAL	10
#define DEF_SHRINK_INTERVAL		10000
#define DEF_SHRINK_NR			(1024 << PAGES_PER_MB_SHIFT)

#define DEF_FAULT_INTERVAL		200
#define DEF_REFAULT_RATIO		22000

#define DEF_WB_LIMIT			5368709120

enum xswapd_event_item {
	XSWAPD_WAKEUP,
	XSWAPD_SHRINK_NODE,
	XSWAPD_SHRINK_ANON,
	XSWAPD_SHRINK_WB,
	XSWAPD_UPDATE_REFAULT,
	XSWAPD_REFAULT_SKIP,
	XSWAPD_MEMCG_REFAULT_SKIP,
	XSWAPD_MEMCG_CMP_SKIP,
	XSWAPD_MEMCG_WB_SKIP,
	XSWAPD_SHRINK_SKIP,
	XSWAPD_RECLAIMED,
	XSWAPD_WRITEBACK,
	XSWAPD_MIN_PRESS,
	XSWAPD_LOW_PRESS,
	XSWAPD_HIGH_PRESS,
	XSWAPD_LOW_MEM_SKIP,
	XSWAPD_INTERVAL_SKIP,
	XSWAPD_DRY_RUN,
	NR_XSWAPD_ITEMS
};

struct xswapd_event_state {
	unsigned long event[NR_XSWAPD_ITEMS];
};

/* HACK: use process hole flag */
#define PF_RECLAIM_TASK		PF__HOLE__02000000

/* HACK: copy from vmscan.c */
enum scan_balance {
	SCAN_EQUAL,
	SCAN_FRACT,
	SCAN_ANON,
	SCAN_FILE,
};

bool xswapd_stop_wb(void);

#endif
