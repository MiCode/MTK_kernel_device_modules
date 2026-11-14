// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[blocktag][fuse]" fmt

#include "blocktag-fuse-trace.h"
#include <linux/tracepoint.h>
#include <linux/smp.h>
#include <fuse_i.h>

static struct {
	const char *name;
} fuse_ops[] = {
	[FUSE_LOOKUP]		= { "LOOKUP" },
	[FUSE_FORGET]		= { "FORGET" },
	[FUSE_GETATTR]		= { "GETATTR" },
	[FUSE_SETATTR]		= { "SETATTR" },
	[FUSE_READLINK]		= { "READLINK" },
	[FUSE_SYMLINK]		= { "SYMLINK" },
	[FUSE_MKNOD]		= { "MKNOD" },
	[FUSE_MKDIR]		= { "MKDIR" },
	[FUSE_UNLINK]		= { "UNLINK" },
	[FUSE_RMDIR]		= { "RMDIR" },
	[FUSE_RENAME]		= { "RENAME" },
	[FUSE_LINK]		= { "LINK" },
	[FUSE_OPEN]		= { "OPEN" },
	[FUSE_READ]		= { "READ" },
	[FUSE_WRITE]		= { "WRITE" },
	[FUSE_STATFS]		= { "STATFS" },
	[FUSE_RELEASE]		= { "RELEASE" },
	[FUSE_FSYNC]		= { "FSYNC" },
	[FUSE_SETXATTR]		= { "SETXATTR" },
	[FUSE_GETXATTR]		= { "GETXATTR" },
	[FUSE_LISTXATTR]	= { "LISTXATTR" },
	[FUSE_REMOVEXATTR]	= { "REMOVEXATTR" },
	[FUSE_FLUSH]		= { "FLUSH" },
	[FUSE_INIT]		= { "INIT" },
	[FUSE_OPENDIR]		= { "OPENDIR" },
	[FUSE_READDIR]		= { "READDIR" },
	[FUSE_RELEASEDIR]	= { "RELEASEDIR" },
	[FUSE_FSYNCDIR]		= { "FSYNCDIR" },
	[FUSE_GETLK]		= { "GETLK" },
	[FUSE_SETLK]		= { "SETLK" },
	[FUSE_SETLKW]		= { "SETLKW" },
	[FUSE_ACCESS]		= { "ACCESS" },
	[FUSE_CREATE]		= { "CREATE" },
	[FUSE_INTERRUPT]	= { "INTERRUPT" },
	[FUSE_BMAP]		= { "BMAP" },
	[FUSE_DESTROY]		= { "DESTROY" },
	[FUSE_IOCTL]		= { "IOCTL" },
	[FUSE_POLL]		= { "POLL" },
	[FUSE_NOTIFY_REPLY]	= { "NOTIFY_REPLY" },
	[FUSE_BATCH_FORGET]	= { "BATCH_FORGET" },
	[FUSE_FALLOCATE]	= { "FALLOCATE" },
	[FUSE_READDIRPLUS]	= { "READDIRPLUS" },
	[FUSE_RENAME2]		= { "RENAME2" },
	[FUSE_LSEEK]		= { "LSEEK" },
	[FUSE_COPY_FILE_RANGE]	= { "COPY_FILE_RANGE" },
	[FUSE_SETUPMAPPING]	= { "SETUPMAPPING" },
	[FUSE_REMOVEMAPPING]	= { "REMOVEMAPPING" },
	[FUSE_SYNCFS]		= { "SYNCFS" },
	[FUSE_TMPFILE]		= { "TMPFILE" },
	[FUSE_STATX]		= { "STATX" },
	[FUSE_CANONICAL_PATH]	= { "CANONICAL_PATH" },
	[CUSE_INIT]		= { "CUSE_INIT" },
};

#define FUSE_MAXOP ARRAY_SIZE(fuse_ops)

static const char *opname(enum fuse_opcode opcode)
{
	if (opcode >= FUSE_MAXOP || !fuse_ops[opcode].name)
		return NULL;
	else
		return fuse_ops[opcode].name;
}

static const char *filtername(int filter)
{
	switch (filter) {
	case 0:
		return "NONE";
#ifdef FUSE_OPCODE_FILTER
	case FUSE_PREFILTER:
		return "FUSE_PREFILTER";
	case FUSE_POSTFILTER:
		return "FUSE_POSTFILTER";
	case FUSE_PREFILTER | FUSE_POSTFILTER:
		return "FUSE_PREFILTER | FUSE_POSTFILTER";
#endif
	default:
		return NULL;
	}
}

static const char *eventname(int event)
{
	switch (event) {
	case BTAG_FUSE_REQ:
		return "FUSE_REQ";
	default:
		return NULL;
	}
}

/*
 * Fuse Periodic statistics
 */
#define PERIODIC_STAT_MS 100
static struct fuse_periodic_stat pstat;
struct hrtimer pstat_timer;

static void fuse_pstat_inc(void)
{
	guard(spinlock_irqsave)(&pstat.lock);

	pstat.accumulator++;
	if (!hrtimer_active(&pstat_timer))
		hrtimer_start(&pstat_timer,
			      ms_to_ktime(PERIODIC_STAT_MS),
			      HRTIMER_MODE_REL);
}

static void clean_up_pstat(void)
{
	guard(spinlock_irqsave)(&pstat.lock);
	memset(&pstat, 0, sizeof(pstat));
}

u64 btag_fuse_pstat_get_last(void)
{
	guard(spinlock_irqsave)(&pstat.lock);
	return pstat.last_cnt;
}

u64 btag_fuse_pstat_get_max(void)
{
	guard(spinlock_irqsave)(&pstat.lock);
	return pstat.max_cnt;
}

u64 btag_fuse_pstat_get_distribution(u32 idx)
{
	if (idx >= ARRAY_SIZE(pstat.distribution))
		return 0;

	guard(spinlock_irqsave)(&pstat.lock);
	return pstat.distribution[idx];
}

static enum hrtimer_restart pstat_timer_fn(struct hrtimer *timer)
{
	scoped_guard(spinlock_irqsave, &pstat.lock) {
		pstat.last_cnt = pstat.accumulator;
		pstat.distribution[fls(pstat.last_cnt)]++;
		if (pstat.accumulator) {
			pstat.accumulator = 0;
			if (pstat.last_cnt > pstat.max_cnt)
				pstat.max_cnt = pstat.last_cnt;
		} else {
			return HRTIMER_NORESTART;
		}
	}

	hrtimer_forward_now(timer, ms_to_ktime(PERIODIC_STAT_MS));
	return HRTIMER_RESTART;
}

/*
 * Fuse tracer core
 */
#define TEMP_PID_CNT 32768
struct pid_fuse_stat_entry {
	u64 req_cnt;
	unsigned short tgid;
} *pid_stat;
static unsigned short hot_pid;
static struct btag_fuse_req_hist fuse_log;
static struct btag_fuse_req_stat stat[FUSE_MAXOP];
static u64 total_req_cnt;
#if IS_ENABLED(CONFIG_CGROUP_SCHED)
static struct btag_fuse_req_stat top_stat[FUSE_MAXOP];
static u64 total_req_cnt_top;
#endif
static DEFINE_SPINLOCK(stat_lock);
static DEFINE_SPINLOCK(top_stat_lock);
static DEFINE_SPINLOCK(pid_stat_lock);

static void btag_fuse_request_send(void *data, const struct fuse_req *rq)
{
#ifdef FUSE_OPCODE_FILTER
	u32 opcode = rq->in.h.opcode & FUSE_OPCODE_FILTER;
	u32 filter = rq->in.h.opcode & ~FUSE_OPCODE_FILTER;
#else
	u32 opcode = rq->in.h.opcode;
	u32 filter = 0;
#endif

	struct btag_fuse_entry *e;
	unsigned long flags;
	unsigned int idx;
#if IS_ENABLED(CONFIG_CGROUP_SCHED)
	struct cgroup *grp;
#endif

	spin_lock_irqsave(&fuse_log.lock, flags);
	if (fuse_log.enable) {
		idx = fuse_log.next++;
		if (fuse_log.next == MAX_FUSE_REQ_HIST_CNT)
			fuse_log.next = 0;
		e = fuse_log.req_hist + idx;
		e->flags = rq->flags;
		e->time = local_clock();
		e->unique = rq->in.h.unique;
		e->nodeid = rq->in.h.nodeid;
		e->uid = rq->in.h.uid;
		e->gid = rq->in.h.gid;
		e->pid = rq->in.h.pid;
		e->opcode = opcode;
		e->filter = filter;
		e->event = BTAG_FUSE_REQ;
		e->cpu = smp_processor_id();
	}
	spin_unlock_irqrestore(&fuse_log.lock, flags);

	fuse_pstat_inc();

	if (opcode && opcode < FUSE_MAXOP) {
		spin_lock_irqsave(&stat_lock, flags);
		stat[opcode].count++;
#ifdef FUSE_OPCODE_FILTER
		if (filter & FUSE_PREFILTER)
			stat[opcode].prefilter++;
		if (filter & FUSE_POSTFILTER)
			stat[opcode].postfilter++;
#endif
		total_req_cnt++;
		spin_unlock_irqrestore(&stat_lock, flags);

#if IS_ENABLED(CONFIG_CGROUP_SCHED)
		rcu_read_lock();
		grp = task_cgroup(current, cpuset_cgrp_id);
		if (grp->kn->name && !strcmp("top-app", grp->kn->name)) {
			spin_lock_irqsave(&top_stat_lock, flags);
			top_stat[opcode].count++;
#ifdef FUSE_OPCODE_FILTER
			if (filter & FUSE_PREFILTER)
				top_stat[opcode].prefilter++;
			if (filter & FUSE_POSTFILTER)
				top_stat[opcode].postfilter++;
#endif
			total_req_cnt_top++;
			spin_unlock_irqrestore(&top_stat_lock, flags);
		}
		rcu_read_unlock();
#endif

		spin_lock_irqsave(&pid_stat_lock, flags);
		pid_stat[current->pid].req_cnt++;
		pid_stat[current->pid].tgid = current->tgid;
		if (current->pid != hot_pid &&
		    pid_stat[current->pid].req_cnt > pid_stat[hot_pid].req_cnt)
			hot_pid = current->pid;
		spin_unlock_irqrestore(&pid_stat_lock, flags);

		mtk_btag_earaio_check_window();
	}

	if (!opname(opcode))
		pr_err("unknown opcode, rq->in.h.opcode=%u\n", rq->in.h.opcode);

	if (!filtername(filter))
		pr_err("unknown filter, rq->in.h.opcode=%u\n", rq->in.h.opcode);
}

/**
 * mtk_btag_fuse_get_req_cnt - Get the cumulatvie number of fuse requests
 * @opcode: Specifies to get the count for, 0 for total request count
 *
 * Return the cumulative request count. 0 if the opcode is invalid.
 */
u64 mtk_btag_fuse_get_req_cnt(u32 opcode)
{
	unsigned long flags;
	u64 ret;

	if (opcode >= FUSE_MAXOP)
		return 0;

	spin_lock_irqsave(&stat_lock, flags);
	ret = opcode ? stat[opcode].count : total_req_cnt;
	spin_unlock_irqrestore(&stat_lock, flags);

	return ret;
}

#if IS_ENABLED(CONFIG_CGROUP_SCHED)
/**
 * This function is similar to mtk_btag_fuse_get_req_cnt but retrieves the
 * request count from the top app.
 */
u64 mtk_btag_fuse_get_req_cnt_top(u32 opcode)
{
	unsigned long flags;
	u64 ret = 0;

	if (opcode < FUSE_MAXOP) {
		spin_lock_irqsave(&top_stat_lock, flags);
		ret = opcode ? top_stat[opcode].count : total_req_cnt_top;
		spin_unlock_irqrestore(&top_stat_lock, flags);
	}

	return ret;
}
#endif

void mtk_btag_fuse_get_hot_pid(unsigned short *pid, unsigned short *tgid)
{
	unsigned long flags;

	spin_lock_irqsave(&top_stat_lock, flags);
	*pid = hot_pid;
	*tgid = pid_stat[hot_pid].tgid;
	memset(pid_stat, 0, sizeof(struct pid_fuse_stat_entry) * TEMP_PID_CNT);
	hot_pid = 0;
	spin_unlock_irqrestore(&top_stat_lock, flags);
}

void mtk_btag_fuse_req_hist_show(char **buff, unsigned long *size,
				 struct seq_file *seq)
{
	struct btag_fuse_entry *e;
	struct timespec64 time;
	unsigned long flags;
	int idx;

	spin_lock_irqsave(&fuse_log.lock, flags);

	if (!fuse_log.enable) {
		BTAG_PRINTF(NULL, NULL, seq, "req_hist is disabled\n");
		goto unlock;
	}

	idx = fuse_log.next;

	while (true) {
		idx = idx ? idx - 1 : MAX_FUSE_REQ_HIST_CNT - 1;

		if (idx == fuse_log.next)
			break;

		e = fuse_log.req_hist + idx;
		if (!e->time)
			continue;

		time = ns_to_timespec64(e->time);
		BTAG_PRINTF(buff, size, seq,
			    "%llu.%09lu,%s(%u),flags=0x%03lx,unique=%llu,nodeid=0x%012llx,uid=%u,gid=%u,pid=%u,bpf=%s,opcode=%u(%s)\n",
			    time.tv_sec, time.tv_nsec,
			    eventname(e->event) ?: "???",
			    e->cpu,
			    e->flags,
			    e->unique,
			    e->nodeid,
			    e->uid,
			    e->gid,
			    e->pid,
			    filtername(e->filter) ?: "???",
			    e->opcode,
			    opname(e->opcode) ?: "???");
	}
unlock:
	spin_unlock_irqrestore(&fuse_log.lock, flags);
}

static void disable_req_hist(void)
{
	unsigned long flags;

	spin_lock_irqsave(&fuse_log.lock, flags);
	if (fuse_log.enable) {
		memset(fuse_log.req_hist, 0, sizeof(fuse_log.req_hist));
		fuse_log.next = 0;
		fuse_log.enable = 0;
	}
	spin_unlock_irqrestore(&fuse_log.lock, flags);
}

static void enable_req_hist(void)
{
	unsigned long flags;

	spin_lock_irqsave(&fuse_log.lock, flags);
	if (!fuse_log.enable)
		fuse_log.enable = 1;
	spin_unlock_irqrestore(&fuse_log.lock, flags);
}

static void clean_up_stat(void)
{
	unsigned long flags;

	spin_lock_irqsave(&stat_lock, flags);
	memset(stat, 0, sizeof(stat));
	total_req_cnt = 0;
	spin_unlock_irqrestore(&stat_lock, flags);
#if IS_ENABLED(CONFIG_CGROUP_SCHED)
	spin_lock_irqsave(&top_stat_lock, flags);
	memset(top_stat, 0, sizeof(top_stat));
	total_req_cnt_top = 0;
	spin_unlock_irqrestore(&top_stat_lock, flags);
#endif
}

/*
 * Fuse Tracepoint Register & Unregister
 */
#define FOR_EACH_INTEREST(i) for (i = 0; i < ARRAY_SIZE(interests); i++)

struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool init;
};

static struct tracepoints_table interests[] = {
	{
		.name = "fuse_request_send",
		.func = btag_fuse_request_send
	},
};

static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(interests[i].name, tp->name) == 0)
			interests[i].tp = tp;
	}
}

static void uninstall_tracepoints(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (interests[i].init) {
			tracepoint_probe_unregister(interests[i].tp,
						    interests[i].func,
						    NULL);
			interests[i].init = NULL;
		}
	}
}

static int install_tracepoints(void)
{
	int i;

	/* Install the tracepoints */
	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	FOR_EACH_INTEREST(i) {
		if (interests[i].tp == NULL) {
			pr_info("tracepoints %s not found\n",
				interests[i].name);
			/* Unload previously loaded */
			uninstall_tracepoints();
			return -EINVAL;
		}

		tracepoint_probe_register(interests[i].tp,
					  interests[i].func,
					  NULL);
		interests[i].init = true;
	}

	return 0;
}

/*
 * Fuse Proc FS Operations
 */
static struct proc_dir_entry *fuse_root;
static struct proc_dir_entry *req_hist_e;
static struct proc_dir_entry *stat_e;
static struct proc_dir_entry *pstat_e;
static struct proc_dir_entry *control_e;

static int req_hist_proc_show(struct seq_file *m, void *v)
{
	mtk_btag_fuse_req_hist_show(NULL, NULL, m);
	return 0;
}

static int req_hist_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, req_hist_proc_show, inode->i_private);
}

static const struct proc_ops btag_fuse_req_hist_fops = {
	.proc_open		= req_hist_proc_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};

static int stat_proc_show(struct seq_file *seq, void *v)
{
	unsigned long flags;
	int i;

	spin_lock_irqsave(&stat_lock, flags);
	BTAG_PRINTF(NULL, NULL, seq,
		    "Total Request Count: %llu\n", total_req_cnt);
	spin_unlock_irqrestore(&stat_lock, flags);

	BTAG_PRINTF(NULL, NULL, seq,
		    "opcode,prefilter,postfilter,count,name\n");

	for (i = 0; i < FUSE_MAXOP; i++) {
		if (!opname(i))
			continue;

		spin_lock_irqsave(&stat_lock, flags);
		BTAG_PRINTF(NULL, NULL, seq, "%d,%llu,%llu,%llu,%s\n",
			    i, stat[i].prefilter, stat[i].postfilter,
			    stat[i].count, opname(i));
		spin_unlock_irqrestore(&stat_lock, flags);
	}

	return 0;
}

static int stat_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, stat_proc_show, inode->i_private);
}

static const struct proc_ops btag_fuse_stat_fops = {
	.proc_open		= stat_proc_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};

static int pstat_proc_show(struct seq_file *seq, void *v)
{
	int i;

	BTAG_PRINTF(NULL, NULL, seq, "last: %llu\n",
			btag_fuse_pstat_get_last());
	BTAG_PRINTF(NULL, NULL, seq, "max: %llu\n",
			btag_fuse_pstat_get_max());

	BTAG_PRINTF(NULL, NULL, seq, "[0, 0]: %llu\n",
			btag_fuse_pstat_get_distribution(0));

	for (i = 1; i < ARRAY_SIZE(pstat.distribution); i++)
		BTAG_PRINTF(NULL, NULL, seq, "[2^%d, 2^%d): %llu\n", i - 1, i,
				btag_fuse_pstat_get_distribution(i));

	return 0;
}

static int pstat_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, pstat_proc_show, inode->i_private);
}

static const struct proc_ops btag_fuse_pstat_fops = {
	.proc_open		= pstat_proc_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};

static int control_proc_show(struct seq_file *seq, void *v)
{
	unsigned long flags;
	u8 hist_enable;

	spin_lock_irqsave(&fuse_log.lock, flags);
	hist_enable = fuse_log.enable;
	spin_unlock_irqrestore(&fuse_log.lock, flags);

	BTAG_PRINTF(NULL, NULL, seq, "req_hist status: %s\n",
		    hist_enable ? "enable" : "disable");
	BTAG_PRINTF(NULL, NULL, seq, "=== control command ===\n");
	BTAG_PRINTF(NULL, NULL, seq, "echo n > /proc/blocktag/fuse/control\n");
	BTAG_PRINTF(NULL, NULL, seq, "0: disable req_hist\n");
	BTAG_PRINTF(NULL, NULL, seq, "1: enable req_hist\n");
	BTAG_PRINTF(NULL, NULL, seq, "2: clear stat\n");
	BTAG_PRINTF(NULL, NULL, seq, "3: clear periodic_stat\n");

	return 0;
}

static int control_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, control_proc_show, inode->i_private);
}

static ssize_t control_proc_write(struct file *file, const char *buf,
				  size_t count, loff_t *data)
{
	unsigned int val;
	int err = kstrtouint_from_user(buf, count, 0, &val);

	if (err)
		return err;

	switch (val) {
	case 0:
		disable_req_hist();
		break;
	case 1:
		enable_req_hist();
		break;
	case 2:
		clean_up_stat();
		break;
	case 3:
		clean_up_pstat();
		break;
	default:
		pr_err("unknown command %u\n", val);
		return -EINVAL;
	}

	return count;
}

static const struct proc_ops btag_fuse_control_fops = {
	.proc_open		= control_proc_open,
	.proc_write		= control_proc_write,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};

/*
 * MTK Blocktag Fuse Trace Init & Exit
 */
void mtk_btag_fuse_init(struct proc_dir_entry *btag_root)
{
	int ret;

	if (!btag_root) {
		pr_err("Invalid btag_root\n");
		return;
	}

	spin_lock_init(&fuse_log.lock);
	spin_lock_init(&pstat.lock);
#ifdef MTK_BLOCK_IO_DEBUG_BUILD
	fuse_log.enable = 1;
#endif

	pid_stat = kzalloc(sizeof(struct pid_fuse_stat_entry) * TEMP_PID_CNT,
			   GFP_KERNEL);

	ret = install_tracepoints();
	if (ret) {
		pr_err("install tracepoints failed %d\n", ret);
		return;
	}

	hrtimer_init(&pstat_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pstat_timer.function = pstat_timer_fn;
	hrtimer_start(&pstat_timer, ms_to_ktime(PERIODIC_STAT_MS),
			HRTIMER_MODE_REL);

	/* proc_create for fuse/req_hist, fuse/stat, fuse/control */
	fuse_root = proc_mkdir("fuse", btag_root);
	if (IS_ERR(fuse_root)) {
		ret = PTR_ERR(fuse_root);
		pr_err("proc create failed for fuse (%d)\n", ret);
		goto uninstall_tp;
	}
	req_hist_e = proc_create("req_hist", S_IFREG | 0444, fuse_root,
				 &btag_fuse_req_hist_fops);
	if (IS_ERR(req_hist_e)) {
		ret = PTR_ERR(req_hist_e);
		pr_err("proc create failed for fuse/req_hist (%d)\n", ret);
		goto free_fuse_root;
	}
	stat_e = proc_create("stat", S_IFREG | 0444, fuse_root,
				 &btag_fuse_stat_fops);
	if (IS_ERR(stat_e)) {
		ret = PTR_ERR(stat_e);
		pr_err("proc create failed for fuse/stat (%d)\n", ret);
		goto free_req_hist_e;
	}
	pstat_e = proc_create("periodic_stat", S_IFREG | 0444, fuse_root,
				 &btag_fuse_pstat_fops);
	if (IS_ERR(pstat_e)) {
		ret = PTR_ERR(pstat_e);
		pr_err("proc create failed for fuse/periodic_stat (%d)\n", ret);
		goto free_stat_e;
	}
	control_e = proc_create("control", S_IFREG | 0444, fuse_root,
				 &btag_fuse_control_fops);
	if (IS_ERR(control_e)) {
		ret = PTR_ERR(control_e);
		pr_err("proc create failed for fuse/control (%d)\n", ret);
		goto free_pstat_e;
	}

	return;

free_pstat_e:
	proc_remove(pstat_e);
free_stat_e:
	proc_remove(stat_e);
free_req_hist_e:
	proc_remove(req_hist_e);
free_fuse_root:
	proc_remove(fuse_root);
uninstall_tp:
	uninstall_tracepoints();
	hrtimer_cancel(&pstat_timer);
}

void mtk_btag_fuse_exit(void)
{
	hrtimer_cancel(&pstat_timer);
	proc_remove(control_e);
	proc_remove(stat_e);
	proc_remove(req_hist_e);
	proc_remove(fuse_root);
	uninstall_tracepoints();
	kfree(pid_stat);
}
