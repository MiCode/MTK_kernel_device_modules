// SPDX-License-Identifier: GPL-2.0+
/*
 * mi_locking will show kernel lock wait/hold time.
 *
 * Copyright (C) 2024 Xiaomi Ltd.
 */
#ifndef __HOLDLOCK_H
#define __HOLDLOCK_H

#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>

#include "waitlock.h"

#define LOCK_STATS_DIRNAME                      "lock_stats"
#define MAX_TRACE_ENTRIES                       (512)
#define OVER_FLOW_LEN                           32
#define MAX_STACE_TRACE_ENTRIES                 (32)
#define TASK_COMM_LEN                           16
#define BACKTRACE_DEPTH                         30
#define MONI_LOCK_TYPES                          2

#define DEFINE_PROC_ATTRIBUTE(name, __write)				\
	static int name##_open(struct inode *inode, struct file *file)	\
	{								\
		return single_open(file, name##_show, inode->i_private/*PDE_DATA(inode)*/);	\
	}							\
									\
	static const struct proc_ops name##_fops = {		\
		.proc_open		= name##_open,	   			\
		.proc_write		= name##_write,				\
		.proc_read		= seq_read,				\
		.proc_lseek		= seq_lseek,				\
		.proc_release	= single_release,			\
	}

#define DEFINE_PROC_ATTRIBUTE_RW(name)					\
	static ssize_t name##_write(struct file *file,			\
				    const char __user *buf,		\
				    size_t count, loff_t *ppos)		\
	{								\
		return name##_store(file_inode(file)->i_private/*PDE_DATA(file_inode(file))*/, buf,	\
				    count);				\
	}								\
	DEFINE_PROC_ATTRIBUTE(name, name##_write)

#define DEFINE_PROC_ATTRIBUTE_RO(name)	\
	DEFINE_PROC_ATTRIBUTE(name, NULL)

struct stack_entry {
	unsigned int nr_entries;
	unsigned long *entries;
};


struct stack_entry_task {
	unsigned int nr_entries;
	unsigned long entries[BACKTRACE_DEPTH];
};


struct xm_stack_trace {
	u64 last_timestamp;
	struct task_struct *skip;

	unsigned int nr_stack_entries;
	unsigned int nr_entries;
	struct stack_entry stack_entries[MAX_STACE_TRACE_ENTRIES];
	unsigned long entries[MAX_TRACE_ENTRIES + OVER_FLOW_LEN];

	char curr_comms[MAX_STACE_TRACE_ENTRIES][TASK_COMM_LEN];
	char parent_comms[MAX_STACE_TRACE_ENTRIES][TASK_COMM_LEN];
	pid_t pids[MAX_STACE_TRACE_ENTRIES];
	int duration[MAX_STACE_TRACE_ENTRIES];
	int prio[MAX_STACE_TRACE_ENTRIES];
	bool flag[MAX_STACE_TRACE_ENTRIES];
	u64 timestamp[MAX_STACE_TRACE_ENTRIES];
};

struct xm_stack_trace_task {
	unsigned int nr_tasks;
	struct stack_entry_task stack_entries[MAX_STACE_TRACE_ENTRIES];

	char curr_comms[MAX_STACE_TRACE_ENTRIES][TASK_COMM_LEN];
	char parent_comms[MAX_STACE_TRACE_ENTRIES][TASK_COMM_LEN];
	pid_t pids[MAX_STACE_TRACE_ENTRIES];
	int duration[MAX_STACE_TRACE_ENTRIES];
	int prio[MAX_STACE_TRACE_ENTRIES];
	bool flag[MAX_STACE_TRACE_ENTRIES];
	u64 timestamp[MAX_STACE_TRACE_ENTRIES];
};

void holdlock_exit(void);
int holdlock_init(void);
int holdlock_proc_init(void);
void android_vh_record_mutex_lock_starttime_handler(void *nouse, struct mutex *lock, unsigned long settime_jiffies);
void android_vh_record_rwsem_lock_starttime_handler(void *ignore, struct rw_semaphore *sem, unsigned long settime_jiffies);
void android_vh_rwsem_write_finished_handler(void *ignore, struct rw_semaphore *sem);
void stack_trace_show(struct seq_file *m, struct xm_stack_trace __percpu *p_stack_trace);
inline bool stack_trace_record(struct xm_stack_trace *stack_trace, int duration, bool flag);

extern struct xm_stack_trace __percpu *p_mutex_stack_trace;
extern struct xm_stack_trace __percpu *p_rwsem_stack_trace;
extern struct lock_stats monitor_info[GRP_TYPES];

#endif /* __HOLDLOCK_H */