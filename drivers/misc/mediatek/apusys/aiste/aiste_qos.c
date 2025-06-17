// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/slab.h> // for kmalloc and kfree
#include <linux/list.h> // for list_head
#include <linux/uaccess.h> // for copy_from_user
#include <linux/string.h> // for memcmp

#include "aiste_qos.h"
#include "aiste_scmi.h"
#include "aiste_debug.h"
#include "aiste_thread.h"

#define MAX_BOOST_VALUE 100
#define DEFAULT_QOS_POOL_SIZE 32

bool is_aiste_supported;

/* global variables */
static uint16_t system_ddr_boost_value;
static LIST_HEAD(qos_list);

static void reset_qos_entry(struct qos_entry *entry, enum aiste_qos_reset_reason reason)
{
	int i;

	entry->used = (reason != AISTE_QOS_RESET_REASON_DELETE);
	entry->ddr_boost_value = 0;
	entry->cpu_boost_value = 0;
	entry->thread_count = 0;
	for (i=0; i < MAX_THREAD_NUM_PER_QOS; i++)
		entry->thread_set[i] = -1;
}

/* Keep track of max DDR boost during entry addition/removal */
static void update_system_ddr_boost(bool delete, uint16_t oldDdrBoostValue)
{
	uint16_t maxDdrBoost = 0;
	struct list_head *pos = NULL;
	struct qos_entry *entry = NULL;

	// the processed entry's DDR boost is not dominant, skip
	if (delete && (oldDdrBoostValue < system_ddr_boost_value))
		return;

	list_for_each(pos, &qos_list) {
		entry = list_entry(pos, struct qos_entry, list);
		if (entry->ddr_boost_value > maxDdrBoost)
			maxDdrBoost = entry->ddr_boost_value;
	}

	if (maxDdrBoost != system_ddr_boost_value) {
		system_ddr_boost_value = maxDdrBoost;
		if (is_aiste_supported)
			aiste_scmi_set(system_ddr_boost_value);
		else
			aiste_err("AISTE is not supported on this platform\n");
	}
}

static void update_thread_cpu_boost(bool delete, pid_t tid, uint16_t oldCpuBoost)
{
	uint16_t maxCpuBoost = 0, curCpuBoost = 0;
	struct list_head *pos = NULL;
	struct qos_entry *entry = NULL;
	int i;

	// the processed entry's CPU boost is not dominant
	curCpuBoost = aiste_thread_get_cpu_boost(tid);
	if (delete && oldCpuBoost < curCpuBoost) {
		aiste_qos_debug("%s: current entry's cpu boost is not dominant (%d/%d)\n",
			__func__, oldCpuBoost, curCpuBoost);
		return;
	}

	list_for_each(pos, &qos_list) {
		entry = list_entry(pos, struct qos_entry, list);
		if (!entry->used)
			continue;
		for (i = 0; i < entry->thread_count; i++) {
			if (entry->thread_set[i] == tid)
				break;
		}
		if (i < entry->thread_count && entry->cpu_boost_value > maxCpuBoost)
			maxCpuBoost = entry->cpu_boost_value;
	}

	// aiste_qos_debug("%s: maxCpuBoost=%d, curCpuBoost=%d\n", __func__, maxCpuBoost, curCpuBoost);
	if (maxCpuBoost != curCpuBoost)
		aiste_thread_update_record(tid, maxCpuBoost, is_aiste_supported);
}

struct qos_entry *validate_and_get_qos_entry(uint64_t qos_id)
{
	struct qos_entry *entry = NULL;

	list_for_each_entry(entry, &qos_list, list) {
		if (entry->used && (uint64_t)entry == qos_id)
			return entry;
	}
	aiste_err("%s: invalid qos_id: %llx\n", __func__, qos_id);
	return NULL;
}

int aiste_qos_init(void)
{
	struct qos_entry *entry;

	for (size_t i = 0; i < DEFAULT_QOS_POOL_SIZE; i++) {
		entry = kmalloc(sizeof(struct qos_entry), GFP_KERNEL);
		if (!entry)
			return -ENOMEM;
		INIT_LIST_HEAD(&entry->list);
		entry->used = false;
		list_add_tail(&entry->list, &qos_list);
	}
	aiste_thread_init();
	return 0;
}

int aiste_qos_deinit(void)
{
	struct list_head *pos, *q;
	struct qos_entry *entry;

	list_for_each_safe(pos, q, &qos_list) {
		entry = list_entry(pos, struct qos_entry, list);
		list_del(pos);
		kfree(entry);
	}
	aiste_thread_deinit();
	return 0;
}

/* return the id of the newly created qos entry. */
uint64_t aiste_create_qos(void)
{
	struct qos_entry *entry = NULL;

	/* Find an unused entry in the pool */
	list_for_each_entry(entry, &qos_list, list) {
		if (!entry->used) {
			reset_qos_entry(entry, AISTE_QOS_RESET_REASON_CREATE);
			goto FIND_ENTRY;
		}
	}

	/* Expland the pool if necessary */
	entry = kmalloc(sizeof(struct qos_entry), GFP_KERNEL);
	if (!entry)
		return 0;
	INIT_LIST_HEAD(&entry->list);
	reset_qos_entry(entry, AISTE_QOS_RESET_REASON_CREATE);
	list_add_tail(&entry->list, &qos_list);

FIND_ENTRY:
	aiste_qos_debug("%s: qos_id=%llx\n", __func__, (uint64_t)entry);
	return (uint64_t)entry;
}

int aiste_delete_qos(struct qos_entry *entry)
{
	int i;
	pid_t thread_set[MAX_THREAD_NUM_PER_QOS];
	uint16_t old_ddr_boost = 0, old_cpu_boost = 0;
	uint16_t thread_count = 0;

	if (!entry)
		return -EINVAL;

	aiste_qos_debug("%s: qos_id=%llx\n", __func__, (uint64_t)entry);

	old_ddr_boost = entry->ddr_boost_value;
	old_cpu_boost = entry->cpu_boost_value;
	thread_count = entry->thread_count;
	memcpy(thread_set, entry->thread_set, thread_count * sizeof(pid_t));
	reset_qos_entry(entry, AISTE_QOS_RESET_REASON_DELETE);
	update_system_ddr_boost(true, old_ddr_boost);
	for (i = 0; i < thread_count; i++)
		update_thread_cpu_boost(true, thread_set[i], old_cpu_boost);
	return 0;
}

int aiste_request_qos(struct qos_entry *entry, uint16_t ddr_boost, uint16_t cpu_boost,
						uint16_t thread_count, pid_t *thread_set)
{
	uint16_t old_ddr_boost = 0, old_cpu_boost = 0;
	bool update_cpu_boost = false, update_thread_set = false;
	pid_t old_thread_set[MAX_THREAD_NUM_PER_QOS];
	uint16_t old_thread_count = 0;
	int i = 0, j = 0;

	if (!entry)
		return -EINVAL;

	// Initialize old thread to avoid coverity warnings
	memset(old_thread_set, -1, sizeof(old_thread_set));
	old_thread_count = entry->thread_count;

	// Update DDR boost value
	if (entry->ddr_boost_value != ddr_boost) {
		old_ddr_boost = entry->ddr_boost_value;
		entry->ddr_boost_value = ddr_boost;
		update_system_ddr_boost(false, old_ddr_boost);
	}

	// Update CPU boost value
	if (thread_count == 0) {
		aiste_qos_debug("%s: thread number = 0. Skip CPU boost.\n", __func__);
		return 0;
	}

	// Backup and update cpu_boost, thread_count, and thread_set
	if (entry->cpu_boost_value != cpu_boost) {
		update_cpu_boost = true;
		old_cpu_boost = entry->cpu_boost_value;
		entry->cpu_boost_value = cpu_boost;
	}
	if (entry->thread_count != thread_count ||
		memcmp(entry->thread_set, thread_set, sizeof(pid_t) * entry->thread_count) != 0) {
		update_thread_set = true;
		entry->thread_count = thread_count;
		memcpy(old_thread_set, entry->thread_set, entry->thread_count * sizeof(pid_t));
		memcpy(entry->thread_set, thread_set, thread_count * sizeof(pid_t));
	}

	if (update_thread_set || update_cpu_boost) {
		i = 0;
		j = 0;
		while (i < thread_count && j < old_thread_count) {
			if (thread_set[i] < old_thread_set[j]) {
				// handle new added threads
				update_thread_cpu_boost(false, thread_set[i], old_cpu_boost);
				i++;
			} else if (thread_set[i] > old_thread_set[j]) {
				// handle deleted threads
				update_thread_cpu_boost(true, old_thread_set[j], old_cpu_boost);
				j++;
			} else {
				// handle existing threads
				if (update_cpu_boost)
					update_thread_cpu_boost(false, thread_set[i], old_cpu_boost);
				i++;
				j++;
			}
		}
		// handle remaining new added threads
		while (i < thread_count) {
			update_thread_cpu_boost(false, thread_set[i], old_cpu_boost);
			i++;
		}

		// handle remaining threads to be delete
		while (j < old_thread_count) {
			update_thread_cpu_boost(true, old_thread_set[j], old_cpu_boost);
			j++;
		}
	}

	return 0;
}
