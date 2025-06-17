/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __AISTE_QOS_H__
#define __AISTE_QOS_H__

#include <linux/types.h> // for pid_t

#define MAX_THREAD_NUM_PER_QOS 32

struct qos_entry {
	uint16_t ddr_boost_value;
	uint16_t cpu_boost_value;
	uint16_t thread_count; // Input: number of threads
	pid_t thread_set[MAX_THREAD_NUM_PER_QOS];
	bool used;
	struct list_head list;
};

enum aiste_qos_reset_reason {
	AISTE_QOS_RESET_REASON_CREATE,
	AISTE_QOS_RESET_REASON_DELETE
};

enum aiste_thread_update_reason {
	AISTE_THREAD_UPDATE_REASON_ADD,
	AISTE_THREAD_UPDATE_REASON_DELETE,
	AISTE_THREAD_UPDATE_REASON_EXIST
};

int aiste_qos_init(void);
int aiste_qos_deinit(void);
uint64_t aiste_create_qos(void);
int aiste_delete_qos(struct qos_entry *entry);
int aiste_request_qos(struct qos_entry *entry, uint16_t ddr_boost, uint16_t cpu_boost,
						uint16_t thread_count, pid_t *thread_set);
struct qos_entry *validate_and_get_qos_entry(uint64_t qos_id);

extern bool is_aiste_supported;

#endif /* __AISTE_QOS_H__ */
