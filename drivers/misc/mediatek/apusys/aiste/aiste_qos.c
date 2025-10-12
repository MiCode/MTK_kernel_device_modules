// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/slab.h>     // for kmalloc and kfree
#include <linux/list.h>     // for list_head
#include <linux/uaccess.h>  // for copy_from_user
#include <linux/string.h>   // for memcmp
#include <linux/random.h>
#include <linux/jiffies.h>

#include "aiste_qos.h"
#include "aiste_scmi.h"
#include "aiste_debug.h"
#include "aiste_thread.h"

#define MAX_BOOST_VALUE 100
#define DEFAULT_QOS_POOL_SIZE 32
#define INT32_MAX 2147483647
#define ENTRY_USED(entry) ((entry)->used)

bool is_aiste_supported;

/* global variables */
static uint16_t system_ddr_boost;
static LIST_HEAD(qos_list);
struct mutex qos_list_lock;

// Note that this function is not protected by a lock. Caller shall handle concurrency.
static void reset_qos_entry(struct qos_entry *entry, enum aiste_qos_reset_reason reason)
{
	int i;
	entry->used = (reason != AISTE_QOS_RESET_REASON_DELETE);
	entry->ddr_boost_value = 0;
	entry->cpu_boost_value = 0;
	entry->thread_count = 0;
	for (i = 0; i < MAX_THREAD_NUM_PER_QOS; i++)
		entry->thread_set[i] = -1;
}

static void set_system_ddr_boost(bool delete, uint16_t old_ddr_boost)
{
	uint16_t max_ddr_boost = 0;
	struct list_head *pos = NULL;
	struct qos_entry *entry = NULL;

	// the processed entry's DDR boost is not dominant, skip
	if (delete && (old_ddr_boost < system_ddr_boost))
		return;

	list_for_each(pos, &qos_list) {
		entry = list_entry(pos, struct qos_entry, list);
		if (entry->ddr_boost_value > max_ddr_boost)
			max_ddr_boost  = entry->ddr_boost_value;
	}

	if (max_ddr_boost != system_ddr_boost) {
		system_ddr_boost = max_ddr_boost ;
		aiste_scmi_set(system_ddr_boost);
	}
}

static void set_cpu_boost_for_thread(pid_t tid)
{
	uint16_t max_cpu_boost = 0;
	uint16_t cur_cpu_boost = aiste_thread_get_cpu_boost(tid);
	struct qos_entry *entry = NULL;
	int i;

	list_for_each_entry(entry, &qos_list, list) {
		if (!ENTRY_USED(entry))
			continue;

		for (i = 0; i < entry->thread_count; i++) {
			if (entry->thread_set[i] == tid) {
				if (entry->cpu_boost_value > max_cpu_boost)
					max_cpu_boost = entry->cpu_boost_value;
				break;
			}
		}
	}

	aiste_thr_debug("%s: tid: %d (cpu_boost: %d->%d)\n",
		__func__, tid, cur_cpu_boost, max_cpu_boost);

	if (max_cpu_boost != cur_cpu_boost)
		aiste_thread_update_record(tid, max_cpu_boost);
}

struct qos_entry *validate_and_get_qos_entry(uint64_t qos_id)
{
	struct qos_entry *entry = NULL;

	mutex_lock(&qos_list_lock);
	list_for_each_entry(entry, &qos_list, list) {
		if (ENTRY_USED(entry) && entry->id == qos_id) {
			mutex_unlock(&qos_list_lock);
			return entry;
		}
	}
	mutex_unlock(&qos_list_lock);
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
	mutex_init(&qos_list_lock);
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
	mutex_destroy(&qos_list_lock);
	aiste_thread_deinit();
	return 0;
}

static uint64_t generate_unique_id(void)
{
	uint64_t random_part = get_random_u64();
	uint64_t time_part = (uint64_t)jiffies;

	return (random_part ^ time_part);
}

/* return the id of the newly created qos entry. */
uint64_t aiste_create_qos(void)
{
	struct qos_entry *entry = NULL;

	if(!is_aiste_supported) {
		aiste_err("AISTE is not supported on this platform\n");
		return 0;
	}

	mutex_lock(&qos_list_lock);

	/* Find an unused entry in the pool */
	list_for_each_entry(entry, &qos_list, list) {
		if (!ENTRY_USED(entry)) {
			reset_qos_entry(entry, AISTE_QOS_RESET_REASON_CREATE);
			goto FIND_ENTRY;
		}
	}

	/* Expand the pool if necessary */
	entry = kmalloc(sizeof(struct qos_entry), GFP_KERNEL);
	if (!entry) {
		mutex_unlock(&qos_list_lock);
		return 0;
	}

	INIT_LIST_HEAD(&entry->list);
	reset_qos_entry(entry, AISTE_QOS_RESET_REASON_CREATE);
	list_add_tail(&entry->list, &qos_list);

FIND_ENTRY:
	entry->id = generate_unique_id();
	mutex_unlock(&qos_list_lock);
	aiste_qos_debug("%s: Create QoS (0x%llx)\n", __func__, entry->id);
	return entry->id;
}

int aiste_delete_qos(struct qos_entry *entry)
{
	int i;
	pid_t thread_set[MAX_THREAD_NUM_PER_QOS];
	uint16_t thread_count = 0;
	uint16_t ddr_boost = 0;

	if (!entry)
		return -EINVAL;

	ddr_boost = entry->ddr_boost_value;
	thread_count = entry->thread_count;
	memcpy(thread_set, entry->thread_set, thread_count * sizeof(pid_t));

	mutex_lock(&qos_list_lock);
	reset_qos_entry(entry, AISTE_QOS_RESET_REASON_DELETE);
	set_system_ddr_boost(true, ddr_boost);
	for (i = 0; i < thread_count; i++)
		set_cpu_boost_for_thread(thread_set[i]);
	mutex_unlock(&qos_list_lock);
	aiste_qos_debug("%s: Delete QoS (0x%llx)\n", __func__, entry->id);
	return 0;
}

static void process_threads(uint64_t qos_id, bool is_cpu_boost_updated,
							pid_t *new_thread_set, uint16_t new_thread_count,
							pid_t *old_thread_set, uint16_t old_thread_count)
{
	int i = 0, j = 0;

	while (i < new_thread_count || j < old_thread_count) {
		pid_t new_thread = (i < new_thread_count) ? new_thread_set[i] : INT32_MAX;
		pid_t old_thread = (j < old_thread_count) ? old_thread_set[j] : INT32_MAX;

		if (new_thread < old_thread) {
			// New thread added
			set_cpu_boost_for_thread(new_thread);
			aiste_thr_debug("%s: Thread %d added to QoS %llx\n", __func__, new_thread, qos_id);
			i++;
		} else if (old_thread < new_thread) {
			// Old thread removed
			set_cpu_boost_for_thread(old_thread);
			aiste_thr_debug("%s: Thread %d removed from QoS %llx\n", __func__, old_thread, qos_id);
			j++;
		} else {
			// Thread unchanged, but apply CPU boost if needed
			if (is_cpu_boost_updated)
				set_cpu_boost_for_thread(new_thread);
			i++;
			j++;
		}
	}
}

int aiste_request_qos(struct qos_entry *entry, uint16_t new_ddr_boost, uint16_t new_cpu_boost,
						uint16_t new_thread_count, pid_t *new_thread_set)
{
	bool is_cpu_boost_updated = false;
	bool is_thread_set_updated = false;
	uint16_t old_ddr_boost = 0, old_cpu_boost = 0;
	uint16_t old_thread_count = 0;
	pid_t old_thread_set[MAX_THREAD_NUM_PER_QOS] = {-1};
	char tids_buf[512] = {0};
	char tid_str[8];
	int ret;

	if (!entry || new_thread_count > MAX_THREAD_NUM_PER_QOS)
		return -EINVAL;

	old_ddr_boost = entry->ddr_boost_value;
	old_cpu_boost = entry->cpu_boost_value;
	old_thread_count = entry->thread_count;

	mutex_lock(&qos_list_lock);
	// Update DDR boost value
	if (new_ddr_boost != old_ddr_boost ) {
		entry->ddr_boost_value = new_ddr_boost;
		set_system_ddr_boost(false, old_ddr_boost);
	}

	// If no threads provided, skip CPU boost and exit
	if (new_thread_count == 0) {
		aiste_qos_debug("%s: thread number = 0. Skip CPU boost.\n", __func__);
		goto UNLOCK;
	}

	// Check and update CPU boost value
	if (new_cpu_boost != old_cpu_boost) {
		is_cpu_boost_updated = true;
		entry->cpu_boost_value = new_cpu_boost;
	}
	// Check and update thread set only if changes occur
	if (old_thread_count != new_thread_count ||
		memcmp(entry->thread_set, new_thread_set, sizeof(pid_t) * entry->thread_count) != 0) {
		is_thread_set_updated = true;
		memcpy(old_thread_set, entry->thread_set, entry->thread_count * sizeof(pid_t));
		memcpy(entry->thread_set, new_thread_set, new_thread_count * sizeof(pid_t));
		entry->thread_count = new_thread_count;
	}
	if (!is_cpu_boost_updated && !is_thread_set_updated)
		goto UNLOCK;

	// Process adding/removing threads and applying CPU boosts as necessary
	process_threads(entry->id, is_cpu_boost_updated, new_thread_set, new_thread_count,
		old_thread_set, old_thread_count);

UNLOCK:
	mutex_unlock(&qos_list_lock);
	for (int i = 0; i < new_thread_count; i++) {
		if (i > 0)
			strlcat(tids_buf, ",", sizeof(tids_buf));
		ret = snprintf(tid_str, sizeof(tid_str), "%d", new_thread_set[i]);
		if (ret >= sizeof(tid_str) || ret < 0)
			return 0;
		strlcat(tids_buf, tid_str, sizeof(tids_buf));
	}
	aiste_qos_debug("%s: Request QoS(0x%llx), DDR(%d), CPU(%d), TIDs(#%d:%s)\n", __func__,
		entry->id, new_ddr_boost, new_cpu_boost, new_thread_count, tids_buf);
	return 0;
}
