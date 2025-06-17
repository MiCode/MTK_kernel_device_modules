// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include "aiste_debug.h"
#include "aiste_thread.h"

#define MAX_THREAD_NUM 32

static int record_size = MAX_THREAD_NUM;
static int record_count;
struct RecordEntry *thread_record;
struct mutex thread_record_lock;

//TODO: Implement different CPU configurations for different platforms.
const struct CpuBoostConfig cpuBoostConfigs[] = {
	{.threshold = 0,   .uclamp_min = 0,   .uclamp_max = 1024}, // Default CPU boost
	{.threshold = 25,  .uclamp_min = 0,   .uclamp_max = 512},  // Sustained CPU boost
	{.threshold = 50,  .uclamp_min = 256, .uclamp_max = 768},  // Moderate CPU boost
	{.threshold = 75,  .uclamp_min = 512, .uclamp_max = 1024}  // Performance CPU boost
};

static void aiste_thread_set_uclamp(pid_t tid, uint16_t cpu_boost)
{
	//TODO: get current uclamp.min, uclamp.max and check if needed to update
	struct task_struct *p = find_task_by_vpid(tid);
	struct sched_attr attr = {};

	if (!p) {
		aiste_err("%s: task of tid %d not found\n", __func__, tid);
		return;
	}

	attr.sched_policy = -1;
	attr.sched_flags =
		SCHED_FLAG_KEEP_ALL |
		SCHED_FLAG_UTIL_CLAMP |
		SCHED_FLAG_RESET_ON_FORK;

	/* Set priority for real-time policies (SCHED_FIFO or SCHED_RR) */
	if (p->policy == SCHED_FIFO || p->policy == SCHED_RR)
		attr.sched_priority = p->rt_priority;

	/* Find matching configuration based on cpu_boost level */
	for (int i = ARRAY_SIZE(cpuBoostConfigs) - 1; i >= 0; --i) {
		if (cpu_boost >= cpuBoostConfigs[i].threshold) {
			attr.sched_util_min = cpuBoostConfigs[i].uclamp_min;
			attr.sched_util_max = cpuBoostConfigs[i].uclamp_max;
			break;
		}
	}

	if (sched_setattr_nocheck(p, &attr) != 0)
		aiste_err("%s: set %d uclamp fail\n", __func__, p->pid);
	aiste_qos_debug("%s: tid=%d cur_min=%d cur_max=%d\n",
		__func__, p->pid, attr.sched_util_min, attr.sched_util_max);
}

void aiste_thread_init(void)
{
	thread_record = kmalloc_array(MAX_THREAD_NUM, sizeof(struct RecordEntry), GFP_KERNEL);
	if (!thread_record)
		return;
	memset(thread_record, 0, record_size * sizeof(struct RecordEntry));
	mutex_init(&thread_record_lock);
}

void aiste_thread_deinit(void)
{
	kfree(thread_record);
	mutex_destroy(&thread_record_lock);
}

// Note that this function is not protected by a lock. Caller shall handle concurrency.
static int aiste_thread_get_record(pid_t tid)
{
	int i = 0;

	for (i=0; i < record_count; i++) {
		if (thread_record[i].tid == tid)
			return i;
	}
	aiste_qos_debug("%s: No record for tid %d\n", __func__, tid);
	return -1;
}

static void aiste_thread_delete_reord(pid_t tid)
{
	int index_to_delete = aiste_thread_get_record(tid);
	int i = 0;

	aiste_qos_debug("%s: tid=%d\n", __func__, tid);
	if (index_to_delete < 0 || index_to_delete >= record_count) {
		aiste_err("%s: thread record not found.\n", __func__);
		return;
	}

	for (i = index_to_delete; i < record_count-1 ; i++)
		thread_record[i] = thread_record[i + 1];

	record_count--;
}

uint16_t aiste_thread_get_cpu_boost(pid_t tid)
{
	int i = -1;
	uint16_t cpu_boost = 0;

	mutex_lock(&thread_record_lock);
	i = aiste_thread_get_record(tid);
	if (i >= 0 && i <= record_count)
		cpu_boost = thread_record[i].cpu_boost;
	mutex_unlock(&thread_record_lock);
	return cpu_boost;
}

void aiste_thread_update_record(pid_t tid, uint16_t cpu_boost, bool is_aiste_supported)
{
	int index_to_update = -1;
	struct RecordEntry *temp = NULL;

	mutex_lock(&thread_record_lock);

	index_to_update = aiste_thread_get_record(tid);
	if (index_to_update != -1 && index_to_update < record_count) {
		/* Update the thread record if the thread is already in the record */
		if (thread_record[index_to_update].cpu_boost == cpu_boost) {
			aiste_qos_debug("%s: thread %d: old cpu_boost=new cpu_boost, skip.\n", __func__, tid);
			goto UNLOCK;
		} else {
			thread_record[index_to_update].cpu_boost = cpu_boost;
			aiste_qos_debug("%s: set thread_record[%d]=(%d, %d)\n",
				__func__, index_to_update, tid, cpu_boost);
		}
	} else {
		/* Add new record if thread is not found */
		if (record_count == record_size) {
			/* Double the record size if thread record is full */
			record_size *= 2;
			temp = kmalloc_array(record_size, sizeof(struct RecordEntry), GFP_KERNEL);
			if (!temp)
				goto UNLOCK;
			memcpy(temp, thread_record, record_count * sizeof(struct RecordEntry));
			kfree(thread_record);
			thread_record = temp;
			memset(&thread_record[record_count], 0 ,
				(record_size - record_count) * sizeof(struct RecordEntry));
			aiste_qos_debug("%s: expanded thread record size to %d entries\n", __func__, record_size);
		}

		thread_record[record_count].tid = tid;
		thread_record[record_count].cpu_boost = cpu_boost;
		aiste_qos_debug("%s: new record. set thread_record[%d]=(%d, %d)\n",
			__func__, record_count, tid, cpu_boost);
		record_count++;
	}
	if (is_aiste_supported)
		aiste_thread_set_uclamp(tid, cpu_boost);
	else
		aiste_err("AISTE is not supported on this platform\n");

	if (cpu_boost == 0)
		aiste_thread_delete_reord(tid);
UNLOCK:
	mutex_unlock(&thread_record_lock);
}
