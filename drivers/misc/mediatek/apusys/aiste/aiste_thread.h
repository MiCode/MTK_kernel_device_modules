/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __AISTE_THREAD_H__
#define __AISTE_THREAD_H__

#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <linux/pid.h>

struct RecordEntry {
	pid_t tid;
	uint16_t cpu_boost;
};

struct CpuBoostConfig {
	int threshold;
	int uclamp_min;
	int uclamp_max;
};

void aiste_thread_init(void);
void aiste_thread_deinit(void);
void aiste_thread_update(pid_t thread_id, uint16_t cpu_boost_value);

uint16_t aiste_thread_get_cpu_boost(pid_t tid);
void aiste_thread_update_record(pid_t tid, uint16_t cpu_boost, bool is_aiste_supported);
#endif /* __AISTE_THREAD_H__ */
