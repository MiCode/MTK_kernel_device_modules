// SPDX-License-Identifier: GPL-2.0+
/*
 * mi_locking will show kernel lock wait/hold time.
 *
 * Copyright (C) 2024 Xiaomi Ltd.
 */

#define MI_LOCK_LOG_TAG       "waitlock"
#include <linux/init.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/rwsem.h>
#include <linux/mutex.h>
#include <trace/hooks/dtask.h>

#include "waitlock.h"
#include "holdlock.h"

/*********************************commom***********************************/
struct xm_stack_trace __percpu *p_wait_mutex_stack_trace;
struct xm_stack_trace __percpu *p_wait_rwsemw_stack_trace;

static void android_vh_rwsem_write_wait_finish_handler(void *unused, struct rw_semaphore *sem);
static void android_vh_rwsem_write_wait_start_handler(void *unused, struct rw_semaphore *sem);
static void android_vh_mutex_wait_start_handler(void *unused, struct mutex *lock);
static void android_vh_mutex_wait_finish_handler(void *unused, struct mutex *lock);

static void clear_stats(void);

#define TO_LIMIT_GRP_IDX(x)		(x <= GRP_OT ? x : GRP_OT)
#define PDE_DATA(inode) pde_data(inode)

char *group_str[GRP_TYPES] = {"GRP_UX", "GRP_RT", "GRP_OT",};

static char *lock_str[LOCK_TYPES] = {"mutex", "rwsem_write",};

int get_task_grp_idx(void)
{
	struct mi_task_struct *xts;
	int ret = GRP_OT;

	if (current->prio < MAX_RT_PRIO)
		return GRP_RT;

	xts = get_mi_task_struct(current);

	if (!IS_ERR_OR_NULL(xts) && (is_xm_ux_task(current)))	{
		return GRP_UX;
	}

	return ret;
}

static  u64 lockstat_clock(void)
{
	return local_clock();
}

/*******************************wait enable********************************/
static int wait_enable_show(struct seq_file *m, void *ptr)
{
	seq_printf(m,"%s\n",  (g_opt_enable & WAIT_LK_ENABLE)?"Enabled":"Disabled");
	return 0;
}

static ssize_t wait_enable_store(void *priv, const char __user *buf, size_t count)
{
	int val;
	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	if (val) {
		g_opt_enable |= WAIT_LK_ENABLE;
		register_trace_android_vh_rwsem_write_wait_finish(android_vh_rwsem_write_wait_finish_handler, NULL);
		register_trace_android_vh_rwsem_write_wait_start(android_vh_rwsem_write_wait_start_handler, NULL);
		register_trace_android_vh_mutex_wait_finish(android_vh_mutex_wait_finish_handler, NULL);
		register_trace_android_vh_mutex_wait_start(android_vh_mutex_wait_start_handler, NULL);
	}
	else {
		g_opt_enable ^= WAIT_LK_ENABLE;
		unregister_trace_android_vh_rwsem_write_wait_finish(android_vh_rwsem_write_wait_finish_handler, NULL);
		unregister_trace_android_vh_rwsem_write_wait_start(android_vh_rwsem_write_wait_start_handler, NULL);
		unregister_trace_android_vh_mutex_wait_finish(android_vh_mutex_wait_finish_handler, NULL);
		unregister_trace_android_vh_mutex_wait_start(android_vh_mutex_wait_start_handler, NULL);

		clear_stats();
	}
	return count;
}
DEFINE_PROC_ATTRIBUTE_RW(wait_enable);
/*******************************stack trace info********************************/
static int mutex_stack_trace_show(struct seq_file *m, void *ptr)
{
	stack_trace_show(m,p_wait_mutex_stack_trace);
	seq_printf(m, "[wait lock] mutex stack_trace_show end\n");
	return 0;
}

static ssize_t mutex_stack_trace_store(void *priv, const char __user *buf,
				 size_t count)
{
	pr_info("[wait lock] mutex stack_trace_store\n");
	return count;
}
DEFINE_PROC_ATTRIBUTE_RW(mutex_stack_trace);

static int rwsemw_stack_trace_show(struct seq_file *m, void *ptr)
{
	stack_trace_show(m,p_wait_rwsemw_stack_trace);
	seq_printf(m, "[wait lock] rwsemw stack_trace_show end\n");
	return 0;
}

static ssize_t rwsemw_stack_trace_store(void *priv, const char __user *buf,
				 size_t count)
{
	pr_info("[wait lock] rwsemw stack_trace_store\n");
	return count;
}
DEFINE_PROC_ATTRIBUTE_RW(rwsemw_stack_trace);


static u32 proc_type[LOCK_TYPES];
static struct lock_stats stats_info[GRP_TYPES];

int waittime_thres_exceed_type(int grp_idx, int type, u64 time)
{
	int real_idx;

	real_idx = TO_LIMIT_GRP_IDX(grp_idx);
	if (time > thres_array[real_idx][type][FATAL_THRES]) {
		return FATAL_THRES;
	} else if (time > thres_array[real_idx][type][HIGH_THRES]) {
		return HIGH_THRES;
	} else if (time > thres_array[real_idx][type][LOW_THRES]) {
		return LOW_THRES;
	} else {
		return -1;
	}
}

static int track_stat_update(int grp_idx, int type, struct track_stat *ts, u64 time)
{
	int thres_type;

	if (NULL == ts)
		return -1;

	if (type >= LOCK_TYPES) {
		ml_err("pass error type param, type = %d\n", type);
		return -1;
	}

	thres_type = waittime_thres_exceed_type(grp_idx, type, time);
	if (thres_type < 0)
		return thres_type;

	atomic_inc(&ts->level[thres_type]);

// #ifdef CONFIG_XM_INTERNAL_VERSION
// 	atomic64_add(time, &ts->exp_total_time);
// #endif
	cond_trace_printk(locking_opt_debug(LK_DEBUG_PRINTK),
		"[%s]: add exceed low thres item, grp = %s, type = %s, time = %llu\n",
		__func__, group_str[grp_idx], lock_str[type], time);
	return thres_type;
}


static int lock_stats_update(int grp_idx, int type, u64 time)
{
	struct lock_stats *pcs;
	int ret;
	/*
	 * Get percpu data ptr.
	 * Prevent process from switching to another CPU, disable preempt.
	 * There only is a atomic_inc operation(and/or a trace printk)
	 * during the preemtion off, no particularly time-consuming operations.
	 */
	pcs = &stats_info[grp_idx];
	ret = track_stat_update(grp_idx, type, &pcs->per_type_stat[type], time);
// #ifdef CONFIG_XM_INTERNAL_VERSION
// 	/*Total nr++, Whether or not the minimum threshold is exceeded*/
// 	atomic_inc(&pcs->per_type_stat[type].total_nr);
// 	atomic64_add(time, &pcs->per_type_stat[type].total_time);
// #endif
	return ret;
}

/**
 * handle_wait_stats
 * @param1: mutex or rwsemw
 * @param2: wait lock time， ns
 *
 * handle wait lock time, get cnt to show or
 * save stack trace if UX/RT in fatal
 *
 */
__always_inline void handle_wait_stats(int type, u64 time)
{
	int grp_idx;
	int ret;

	grp_idx = get_task_grp_idx();

	ret = lock_stats_update(grp_idx, type, time);

	if (g_opt_stack && ret >= 0) {
		/*
		 * Only record UX/RT's fatal info, We don't care other groups.
		 */
		if ((ret == FATAL_THRES) && (grp_idx <= GRP_RT)) {
			// if (gen_insert_fatal_info(type, time))
			// 	ml_err("[waitlock]:Failed to generate&insert fatal info \n");
			cond_trace_printk(locking_opt_debug(LK_DEBUG_PRINTK),
				"[%s]: type=%d, curr->pid=%d, curr->comm=%s, comm->name=%s, holdtime_ms=%llu\n",
				__func__, type, current->pid, current->comm, current->group_leader->comm, time/1000);

			if (type == MUTEX) {
				struct xm_stack_trace *stack_trace = this_cpu_ptr(p_wait_mutex_stack_trace);
				stack_trace_record(stack_trace, time/NSEC_PER_MSEC, 0);
			}
			else if (type == RWSEM_WRITE) {
				struct xm_stack_trace *stack_trace = this_cpu_ptr(p_wait_rwsemw_stack_trace);
				stack_trace_record(stack_trace, time/NSEC_PER_MSEC, 0);
			}
		}
	}
}

static ssize_t lock_thres_ctrl_write(struct file *file,
			       const char __user * user_buffer, size_t count,
			       loff_t * offset)
{
	char *lvl_str[] = {"low", "high", "fatal"};
	char kbuf[64];
	char grp[10], lock[20], level[10];
	int ret, i, j, k;
	u32 val;

	if (count < 15 || count >= 64) {
		ml_err("Invailid argument \n");
		return -EINVAL;
	}

	if (copy_from_user(kbuf, user_buffer, count)) {
		ml_err("Copy from user failed\n");
		return -EFAULT;
	}

	kbuf[count] = '\0';

	ret = sscanf(kbuf, "%9s %19s %9s %10d", grp, lock, level, &val);
	if (ret != 4) {
		ml_err("Failed to execute sscanf\n");
		return -EFAULT;
	}

	for (i = 0; i < LIMIT_GRP_TYPES; i++) {
		if (0 == strcmp(grp, group_str[i]))
			break;
	}
	if (LIMIT_GRP_TYPES == i) {
		ml_err("Invalid Group\n");
		return -EFAULT;
	}

	for (j = 0; j < LOCK_TYPES; j++) {
		if (0 == strcmp(lock, lock_str[j]))
			break;
	}
	if (LOCK_TYPES == j) {
		ml_err("Invalid lock name\n");
		return -EFAULT;
	}

	for (k = 0; k < 3; k++) {
		if (0 == strcmp(level, lvl_str[k]))
			break;
	}
	if (3 == k) {
		ml_err("Invalid level name\n");
		return -EFAULT;
	}

	WRITE_ONCE(thres_array[i][j][k], val * 1000);

	return count;
}

static int lock_thres_ctrl_show(struct seq_file *m, void *v)
{
	char *buf;
	int i, j, idx = 0;
	int ret;

	/* PAGE_SIZE is big enough, noneed to use snprintf. */
	buf = (char *)kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = sprintf(&buf[idx], "/*************************************************/\n"
			"Set Usage: \necho group lock_type level value > THIS_FILE \n"
			"Example: echo GRP_UX spinlock low 100000 > ${THIS_FILE} \n\n"
			"Note:Please refer to the output below for the group name and lock name,\n"
			"which are case-sensitive and strictly follow the format, "
			"otherwise the setting will fail."
			"\n/*************************************************/\n\n");

	if ((ret < 0) || (ret >= PAGE_SIZE - idx))
		goto err;
	idx += ret;


	ret = sprintf(&buf[idx], "%-20s%-15s%-15s%-15s\n", "lock-type",
			"low(us)", "high(us)", "fatal(us)");
	if ((ret < 0) || (ret >= PAGE_SIZE - idx))
		goto err;

	idx += ret;

	for (i = 0; i < LIMIT_GRP_TYPES; i++) {
		ret = sprintf(&buf[idx], "\n%s:\n", group_str[i]);
		if ((ret < 0) || (ret >= PAGE_SIZE - idx))
			goto err;
		idx += ret;

		for (j = 0; j < LOCK_TYPES; j++) {
			ret = sprintf(&buf[idx], "%-20s%-15u%-15u%-15u\n", lock_str[j],
						thres_array[i][j][LOW_THRES] / 1000,
						thres_array[i][j][HIGH_THRES] / 1000,
						thres_array[i][j][FATAL_THRES] / 1000);
			if ((ret < 0) || (ret >= PAGE_SIZE - idx))
				goto err;
			idx += ret;
		}
	}
	buf[idx] = '\n';
	seq_printf(m, "%s\n", buf);
	kfree(buf);
	return 0;

err:
	kfree(buf);
	return -EFAULT;
}

static int lock_thres_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, lock_thres_ctrl_show, PDE_DATA(inode));
}

static const struct proc_ops lock_thres_ctrl_fops = {
	.proc_open		= lock_thres_ctrl_open,
	.proc_write		= lock_thres_ctrl_write,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};


static int show_stats(char *buf, u32 blen)
{
	int i, j;
	int idx = 0;
	int ret;
	/*
	 * groups -> locks -> low/high/fatal.
	 * Cause the stack size limit(2048), We can't just define a static array
	 * with 3 columns. The last column of memory get from kmalloc.
	 */
	struct track_stat *lock_count_info;

	for (i = 0; i < GRP_TYPES; i++) {
		for (j = 0; j < LOCK_TYPES; j++) {
			if (0 == j) {
        			ret = snprintf(&buf[idx], (blen - idx), "%s-{", group_str[i]);
				if ((ret < 0) || (ret >= blen - idx))
					goto err;
				idx += ret;
			}

			lock_count_info = &stats_info[i].per_type_stat[j];
// #ifdef CONFIG_XM_INTERNAL_VERSION
// 			ret = snprintf(&buf[idx], (blen - idx), "%s:[%d,%d,%d,%d,%lld,%lld]",
// 					lock_str[j], atomic_read(&lock_count_info->level[0]),
// 					atomic_read(&lock_count_info->level[1]),
// 					atomic_read(&lock_count_info->level[2]),
// 					atomic_read(&lock_count_info->total_nr),
// 					atomic64_read(&lock_count_info->total_time),
// 					atomic64_read(&lock_count_info->exp_total_time));
// #else
			ret = snprintf(&buf[idx], (blen - idx), "%s:[%u,%u,%u]",
					lock_str[j], atomic_read(&lock_count_info->level[0]),
					atomic_read(&lock_count_info->level[1]),
					atomic_read(&lock_count_info->level[2]));
//#endif
			if ((ret < 0) || (ret >= blen - idx))
				goto err;
			idx += ret;

			if ((LOCK_TYPES - 1) != j) {
				ret = snprintf(&buf[idx], (blen - idx), ",");
				if ((ret < 0) || (ret >= blen - idx))
					goto err;
				idx += ret;
			}
		}
		ret = snprintf(&buf[idx], (blen - idx), "}\n");
		if ((ret < 0) || (ret >= blen - idx))
			goto err;
		idx += ret;
	}

	buf[idx] = '\0';

	return 0;

err:
	return -EFAULT;
}


static void clear_stats(void)
{
	int i, j;

	for (i = 0; i < GRP_TYPES; i++) {
		for (j = 0; j < LOCK_TYPES; j++) {
			atomic_set(&stats_info[i].per_type_stat[j].level[0], 0);
			atomic_set(&stats_info[i].per_type_stat[j].level[1], 0);
			atomic_set(&stats_info[i].per_type_stat[j].level[2], 0);
// #ifdef CONFIG_XM_INTERNAL_VERSION
// 			atomic_set(&stats_info[i].per_type_stat[j].total_nr, 0);
// 			atomic64_set(&stats_info[i].per_type_stat[j].total_time, 0);
// 			atomic64_set(&stats_info[i].per_type_stat[j].exp_total_time, 0);
// #endif
		}
	}
}

static void read_clear_stats(void)
{
	clear_stats();
}

static int lock_stats_rclear_show(struct seq_file *m, void *v)
{
	char *buf;
	int ret;

	buf = kmalloc(SHOW_STAT_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = show_stats(buf, SHOW_STAT_BUF_SIZE);
	if (ret < 0) {
		kfree(buf);
		return ret;
	}

	read_clear_stats();

	seq_printf(m, "%s\n", buf);
	kfree(buf);

	return 0;
}

static int lock_stats_rclear_open(struct inode *inode, struct file *file)
{
	return single_open(file, lock_stats_rclear_show, inode);
}


static const struct proc_ops lock_stat_rclear_fops = {
	.proc_open		= lock_stats_rclear_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release,
};


static int lock_stats_show(struct seq_file *m, void *v)
{
	char *buf;
	int ret;

	buf = kmalloc(SHOW_STAT_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = show_stats(buf, SHOW_STAT_BUF_SIZE);
	if (ret < 0) {
		kfree(buf);
		return -EFAULT;
	}

	seq_printf(m, "%s\n", buf);
	kfree(buf);

	return 0;
}

static int lock_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, lock_stats_show, inode);
}


static const struct proc_ops lock_stat_fops = {
	.proc_open		= lock_stats_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release,
};


static int per_lock_stats_show(struct seq_file *m, void *v)
{
	char *buf;
	u32 *ptr;
	int type;
	int i, ret, idx = 0;
// #ifdef CONFIG_XM_INTERNAL_VERSION
// 	char time[64], exp_time[64];
// #endif
	struct track_stat *lock_count_info;

	ptr = (u32*)m->private;
	if (NULL == ptr) {
		return -EFAULT;
	}
	type = *ptr;

	buf = (char*)kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

// #ifdef CONFIG_XM_INTERNAL_VERSION
// 	ret = snprintf(&buf[idx], (PAGE_SIZE - idx), "%-12s%-12s%-12s%-12s%-12s%-12s%-12s\n",
// 			" ", "low", "high", "fatal", "total_nr", "total_time", "exp_total_time");
// #else
	ret = snprintf(&buf[idx], (PAGE_SIZE - idx), "%-12s%-12s%-12s%-12s\n",
			" ", "low", "high", "fatal");
//#endif
	if ((ret < 0) || (ret >= PAGE_SIZE - idx))
		goto err;
	idx += ret;

	for (i = 0; i < GRP_TYPES; i++) {
		lock_count_info = &stats_info[i].per_type_stat[type];
// #ifdef CONFIG_XM_INTERNAL_VERSION
// 		print_time(atomic64_read(&lock_count_info->total_time), time, 64);
// 		print_time(atomic64_read(&lock_count_info->exp_total_time), exp_time, 64);
// 		ret = snprintf(&buf[idx], (PAGE_SIZE - idx), "%-12s%-12u%-12u%-12u%-12u%-12s%-12s\n",
// 				group_str[i], atomic_read(&lock_count_info->level[0]),
// 				atomic_read(&lock_count_info->level[1]),
// 				atomic_read(&lock_count_info->level[2]),
// 				atomic_read(&lock_count_info->total_nr),
// 				time, exp_time);
// #else
		ret = snprintf(&buf[idx], (PAGE_SIZE - idx), "%-12s%-12u%-12u%-12u\n",
				group_str[i], atomic_read(&lock_count_info->level[0]),
				atomic_read(&lock_count_info->level[1]),
				atomic_read(&lock_count_info->level[2]));
//#endif
		if ((ret < 0) || (ret >= PAGE_SIZE - idx))
			goto err;
		idx += ret;
	}

	buf[idx] = '\0';
	seq_printf(m, "%s\n", buf);
	kfree(buf);

	return 0;

err:
	kfree(buf);
	return -EFAULT;
}

static int per_lock_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, per_lock_stats_show, PDE_DATA(inode));
}

static const struct proc_ops per_lock_stat_fops = {
	.proc_open		= per_lock_stats_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release,
};


#define LOCK_STATS_DIRNAME "lock_stats"
#define WAIT_OVERVIEW_DIRNAME "overview"

//extern struct proc_dir_entry *d_xm_locking;
extern struct proc_dir_entry *d_xm_locking_wait;
struct proc_dir_entry *d_lock_stats;
struct proc_dir_entry *d_wait_overview;

static int create_stats_procs(void)
{
	struct proc_dir_entry *p;
	int i;

	if (!d_xm_locking_wait)
		return -ENOMEM;

	d_lock_stats = proc_mkdir(LOCK_STATS_DIRNAME, d_xm_locking_wait);
	if (NULL == d_lock_stats)
		goto err;

	d_wait_overview = proc_mkdir(WAIT_OVERVIEW_DIRNAME, d_xm_locking_wait);
	if (NULL == d_wait_overview)
		goto err;
	ml_err("WAIT_OVERVIEW_DIRNAME init success!!\n");

	//create "mutex" "rwsem_write"
	for (i = 0; i < LOCK_TYPES; i++) {
		proc_type[i] = i;
		p = proc_create_data(lock_str[i], S_IRUGO | S_IWUGO,
					d_lock_stats, &per_lock_stat_fops, &proc_type[i]);
		if (NULL == p)
			goto err1;
	}

	/*For error condtion, to clean up. */
	i -= 1;

	p = proc_create("kern_lock_stats", S_IRUGO | S_IWUGO, d_xm_locking_wait, &lock_stat_fops);
	if (NULL == p)
		goto err2;

	p = proc_create("kern_lock_stats_rclear", S_IRUGO | S_IWUGO, d_xm_locking_wait, &lock_stat_rclear_fops);
	if (NULL == p)
		goto err3;

	p = proc_create("lock_thres_ctrl", S_IRUGO | S_IWUGO, d_xm_locking_wait, &lock_thres_ctrl_fops);
	if (NULL == p)
		goto err5;

	p = proc_create("mutex_stack_trace", S_IRUGO | S_IWUGO, d_xm_locking_wait, &mutex_stack_trace_fops);
	if (NULL == p)
		goto err6;

	p = proc_create("rwsemw_stack_trace", S_IRUGO | S_IWUGO, d_xm_locking_wait, &rwsemw_stack_trace_fops);
	if (NULL == p)
		goto err7;

	p = proc_create("wait_enable", S_IRUGO | S_IWUGO, d_xm_locking_wait, &wait_enable_fops);
	if (NULL == p)
		goto err8;

	if (NULL == (proc_create_data("mutex_max", S_IRUGO, d_wait_overview, &lock_stat_overview_fops, &mutex_max_ctx)) ||
		NULL == (proc_create_data("mutex_total", S_IRUGO, d_wait_overview, &lock_stat_overview_fops, &mutex_total_ctx)) ||
		NULL == (proc_create_data("mutex_avg", S_IRUGO, d_wait_overview, &lock_stat_overview_fops, &mutex_avg_ctx)) ||
		NULL == (proc_create_data("mutex_cnt", S_IRUGO, d_wait_overview, &lock_stat_overview_fops,	&mutex_cnt_ctx)) ||
		NULL == (proc_create_data("rwsemw_max", S_IRUGO, d_wait_overview, &lock_stat_overview_fops,&rwsemw_max_ctx)) ||
		NULL == (proc_create_data("rwsemw_total", S_IRUGO, d_wait_overview, &lock_stat_overview_fops, &rwsemw_total_ctx)) ||
		NULL == (proc_create_data("rwsemw_avg", S_IRUGO, d_wait_overview, &lock_stat_overview_fops, &rwsemw_avg_ctx)) ||
		NULL == (proc_create_data("rwsemw_cnt", S_IRUGO, d_wait_overview, &lock_stat_overview_fops, &rwsemw_cnt_ctx)))
		goto err9;

	return 0;

err9:
	remove_proc_entry(WAIT_OVERVIEW_DIRNAME, d_xm_locking_wait);

err8:
	remove_proc_entry("wait_enable", d_xm_locking_wait);

err7:
 	remove_proc_entry("rwsemw_stack_trace", d_xm_locking_wait);

err6:
 	remove_proc_entry("mutex_stack_trace", d_xm_locking_wait);

err5:
	remove_proc_entry("lock_thres_ctrl", d_xm_locking_wait);

err3:
	remove_proc_entry("kern_lock_stats_rclear", d_xm_locking_wait);

err2:
	remove_proc_entry("kern_lock_stats", d_xm_locking_wait);

err1:
	for (; i >= 0; i--) {
		remove_proc_entry(lock_str[i], d_lock_stats);
	}
	remove_proc_entry(LOCK_STATS_DIRNAME, d_xm_locking_wait);

err:
	return -ENOMEM;
}

static void remove_stats_procs(void)
{
	//int i;
	remove_proc_entry("kern_lock_stats", d_xm_locking_wait);
    remove_proc_entry("kern_lock_stats_rclear", d_xm_locking_wait);
	remove_proc_entry("fatal_lock_stats", d_xm_locking_wait);
    remove_proc_entry("lock_thres_ctrl", d_xm_locking_wait);

	remove_proc_entry("wait_enable", d_xm_locking_wait);
	remove_proc_entry("mutex_stack_trace", d_xm_locking_wait);
	remove_proc_entry("rwsemw_stack_trace", d_xm_locking_wait);

	remove_proc_subtree(LOCK_STATS_DIRNAME, d_xm_locking_wait);
	remove_proc_subtree(WAIT_OVERVIEW_DIRNAME, d_xm_locking_wait);

	// for (i = 0; i < LOCK_TYPES; i++) {
	// 	remove_proc_entry(lock_str[i], d_xm_locking_wait);
	// }

	remove_proc_entry(LOCK_STATS_DIRNAME, d_xm_locking_wait);
	remove_proc_entry(WAIT_OVERVIEW_DIRNAME, d_xm_locking_wait);
}


/****************************hooks operations*****************************/
static __always_inline void lk_contended(int type)
{
	//struct xm_task_struct *xts;
	struct mi_task_struct *xts;

	/* xts is bound to task_struct.
	 * The curr process will not be released during the operation.
	 * So, there is noneed to use rcu lock.
	 */
	xts = get_mi_task_struct(current);

	if (unlikely(IS_ERR_OR_NULL(xts))) {
		ml_err("error start, but xts == NULL !\n");
		return;
	}

	if (xts->lkinfo.waittime_stamp) {
		ml_err("error start,times_stamp not 0,type = %d !\n", type);
		return;
	}

	xts->lkinfo.waittime_stamp = lockstat_clock();
}


static __always_inline void lk_acquired(int type, const char *name)
{
	//struct xm_task_struct *xts;
	struct mi_task_struct *xts;
	u64 now, delta = 0;

	xts = get_mi_task_struct(current);

	if (unlikely(IS_ERR_OR_NULL(xts))) {
		ml_err("error end, but xts == NULL !\n");
		return;
	}

	if (xts->lkinfo.waittime_stamp) {
		now = lockstat_clock();
		delta = now - xts->lkinfo.waittime_stamp;
		xts->lkinfo.waittime_stamp = 0;
		handle_wait_stats(type, delta);

		if (g_opt_sort) {
			if(type == MUTEX || type == RWSEM_WRITE)
				handle_wait_delta(name, delta, type);
		}
	} else {
		ml_err("error end," "no start recorded, type = %s !\n", lock_str[type]);
		return;
	}
}


static void android_vh_mutex_wait_start_handler(void *unused, struct mutex *lock)
{
	lk_contended(MUTEX);
}

static void android_vh_mutex_wait_finish_handler(void *unused, struct mutex *lock)
{
	const char *name = (const char *)lock->android_oem_data1[1];
	lk_acquired(MUTEX, name);
}

static void android_vh_rwsem_write_wait_start_handler(void *unused, struct rw_semaphore *sem)
{
	lk_contended(RWSEM_WRITE);
}

static void android_vh_rwsem_write_wait_finish_handler(void *unused, struct rw_semaphore *sem)
{
	const char *name = (const char *)sem->android_oem_data1[1];
	lk_acquired(RWSEM_WRITE, name);
}

/****************************hooks operations*****************************/

int kern_lstat_init(void)
{
	int ret;

	if (g_opt_enable & WAIT_LK_ENABLE) {
		REGISTER_HOOKS_HANDLE_RET(register_trace_android_vh_mutex_wait_start,
					android_vh_mutex_wait_start_handler, NULL, err3);
		REGISTER_HOOKS_HANDLE_RET(register_trace_android_vh_mutex_wait_finish,
					android_vh_mutex_wait_finish_handler, NULL, err4);
		REGISTER_HOOKS_HANDLE_RET(register_trace_android_vh_rwsem_write_wait_start,
					android_vh_rwsem_write_wait_start_handler, NULL, err7);
		REGISTER_HOOKS_HANDLE_RET(register_trace_android_vh_rwsem_write_wait_finish,
					android_vh_rwsem_write_wait_finish_handler, NULL, err8);
	}

	ret = create_stats_procs();
	if (ret < 0)
		goto err9;

	p_wait_mutex_stack_trace = alloc_percpu(struct xm_stack_trace);
	p_wait_rwsemw_stack_trace = alloc_percpu(struct xm_stack_trace);

	if (!p_wait_mutex_stack_trace || !p_wait_rwsemw_stack_trace) {
		ml_err("alloc_percpu failed!");
		goto free_buf;
	}

	return 0;

free_buf:
	free_percpu(p_wait_mutex_stack_trace);
	free_percpu(p_wait_rwsemw_stack_trace);
err9:
	remove_stats_procs();
err8:
	unregister_trace_android_vh_rwsem_write_wait_finish(
			android_vh_rwsem_write_wait_finish_handler, NULL);
err7:
	unregister_trace_android_vh_rwsem_write_wait_start(
			android_vh_rwsem_write_wait_start_handler, NULL);
err4:
	unregister_trace_android_vh_mutex_wait_finish(
			android_vh_mutex_wait_finish_handler, NULL);
err3:
	unregister_trace_android_vh_mutex_wait_start(
			android_vh_mutex_wait_start_handler, NULL);

 	return ret;
}
//EXPORT_SYMBOL(kern_lstat_init);

void  kern_lstat_exit(void)
{
	unregister_trace_android_vh_mutex_wait_start(
			android_vh_mutex_wait_start_handler, NULL);
	unregister_trace_android_vh_mutex_wait_finish(
			android_vh_mutex_wait_finish_handler, NULL);
	unregister_trace_android_vh_rwsem_write_wait_start(
			android_vh_rwsem_write_wait_start_handler, NULL);
	unregister_trace_android_vh_rwsem_write_wait_finish(
			android_vh_rwsem_write_wait_finish_handler, NULL);

	remove_stats_procs();

}
//EXPORT_SYMBOL(kern_lstat_exit);

