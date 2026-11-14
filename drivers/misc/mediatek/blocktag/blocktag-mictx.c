// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 * Authors:
 *	Perry Hsu <perry.hsu@mediatek.com>
 *	Stanley Chu <stanley.chu@mediatek.com>
 */

#define DEBUG 1

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[blocktag][mictx]" fmt

#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/spinlock.h>
#include <linux/math64.h>
#define CREATE_TRACE_POINTS
#include "blocktag-internal.h"
#include "blocktag-trace.h"
#include "mtk_blocktag.h"

static struct mtk_btag_mictx *mictx_find(struct mtk_blocktag *btag, __u32 id)
{
	return xa_load(&btag->ctx.mictx_xa, id);
}

void mtk_btag_mictx_reset(struct mtk_btag_mictx_id *mictx_id)
{
	struct mtk_blocktag *btag;
	struct mtk_btag_mictx *mictx;
	unsigned long flags;
	int qid;

	if (!mictx_id || !mictx_id->id || !mictx_id->btag_id)
		return;

	guard(rcu)();

	btag = mtk_btag_find_by_id(mictx_id->btag_id);
	if (!btag)
		return;

	mictx = mictx_find(btag, mictx_id->id);
	if (!mictx)
		return;

	/* clear throughput, request data */
	for (qid = 0; qid < mictx->queue_nr; qid++) {
		struct mtk_btag_mictx_queue *q = &mictx->q[qid];

		spin_lock_irqsave(&q->lock, flags);
		q->rq_size[BTAG_IO_READ] = 0;
		q->rq_size[BTAG_IO_WRITE] = 0;
		q->rq_cnt[BTAG_IO_READ] = 0;
		q->rq_cnt[BTAG_IO_WRITE] = 0;
		q->top_len = 0;
		q->tp_size[BTAG_IO_READ] = 0;
		q->tp_size[BTAG_IO_WRITE] = 0;
		q->tp_time[BTAG_IO_READ] = 0;
		q->tp_time[BTAG_IO_WRITE] = 0;
		spin_unlock_irqrestore(&q->lock, flags);
	}

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_DEBUG_BUILD)
	/* clear average queue depth */
	spin_lock_irqsave(&mictx->avg_qd.lock, flags);
	mictx->avg_qd.latency = 0;
	mictx->avg_qd.last_depth_chg = sched_clock();
	spin_unlock_irqrestore(&mictx->avg_qd.lock, flags);
#endif

	/* clear workload */
	spin_lock_irqsave(&mictx->wl.lock, flags);
	mictx->wl.idle_total = 0;
	mictx->wl.window_begin = sched_clock();
	if (mictx->wl.idle_begin)
		mictx->wl.idle_begin = mictx->wl.window_begin;
	spin_unlock_irqrestore(&mictx->wl.lock, flags);

	/* clear top app io information */
	spin_lock_irqsave(&mictx->top.lock, flags);
	mictx->top.pages[BTAG_IO_READ] = 0;
	mictx->top.pages[BTAG_IO_WRITE] = 0;
	mictx->top.rnd_cnt = 0;
	spin_unlock_irqrestore(&mictx->top.lock, flags);
}

void mtk_btag_mictx_get_top_rw(struct mtk_btag_mictx_id *mictx_id,
			       __u32 *top_pages_r, __u32 *top_pages_w)
{
	struct mtk_blocktag *btag;
	struct mtk_btag_mictx *mictx;
	unsigned long flags;

	if (!mictx_id || !mictx_id->id || !mictx_id->btag_id)
		return;

	if (!top_pages_r || !top_pages_w)
		return;

	guard(rcu)();

	btag = mtk_btag_find_by_id(mictx_id->btag_id);
	if (!btag)
		return;

	mictx = mictx_find(btag, mictx_id->id);
	if (!mictx)
		return;

	spin_lock_irqsave(&mictx->top.lock, flags);
	*top_pages_r = mictx->top.pages[BTAG_IO_READ];
	*top_pages_w = mictx->top.pages[BTAG_IO_WRITE];
	spin_unlock_irqrestore(&mictx->top.lock, flags);
}

void mtk_btag_mictx_send_command(struct mtk_blocktag *btag, __u64 start_t,
				 enum mtk_btag_io_type io_type, __u64 tot_len,
				 __u64 top_len, __u32 tid, __u16 qid)
{
	struct mtk_btag_mictx *mictx;
	__u32 top_pages_r, top_pages_w, top_rnd_cnt;
	unsigned long id;

	if (!btag || tid >= BTAG_MAX_TAG || io_type == BTAG_IO_UNKNOWN)
		return;

	rcu_read_lock();
	xa_for_each(&btag->ctx.mictx_xa, id, mictx) {
		struct mtk_btag_mictx_queue *q = &mictx->q[qid];
		unsigned long flags;
#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_DEBUG_BUILD)
		__u64 time;
#endif

		/* workload */
		spin_lock_irqsave(&mictx->wl.lock, flags);
		if (!mictx->wl.depth++) {
			mictx->wl.idle_total += start_t - mictx->wl.idle_begin;
			mictx->wl.idle_begin = 0;
		}
		spin_unlock_irqrestore(&mictx->wl.lock, flags);

		/* top app io information */
		spin_lock_irqsave(&mictx->top.lock, flags);
		mictx->top.pages[io_type] += (__u32)(top_len >> PAGE_SHIFT);
		if (top_len == (1 << PAGE_SHIFT))
			mictx->top.rnd_cnt++;
		top_pages_r = mictx->top.pages[BTAG_IO_READ];
		top_pages_w = mictx->top.pages[BTAG_IO_WRITE];
		top_rnd_cnt = mictx->top.rnd_cnt;
		spin_unlock_irqrestore(&mictx->top.lock, flags);

		/* request size & count */
		spin_lock_irqsave(&q->lock, flags);
		q->rq_cnt[io_type]++;
		q->rq_size[io_type] += tot_len;
		q->top_len += top_len;
		spin_unlock_irqrestore(&q->lock, flags);

		/* tags */
		mictx->tags[tid].start_t = start_t;

		/* throughput */
		if (mictx->full_logging) {
			mictx->tags[tid].io_type = io_type;
			mictx->tags[tid].len = tot_len;
		}

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_DEBUG_BUILD)
		/* average queue depth
		 * NOTE: see the calculation in mictx_evaluate_avg_qd()
		 */
		spin_lock_irqsave(&mictx->avg_qd.lock, flags);
		time = sched_clock();
		if (mictx->full_logging) {
			mictx->avg_qd.latency += mictx->avg_qd.depth *
					(time - mictx->avg_qd.last_depth_chg);
		}
		mictx->avg_qd.last_depth_chg = time;
		mictx->avg_qd.depth++;
		spin_unlock_irqrestore(&mictx->avg_qd.lock, flags);
#endif

		if (top_len && mictx->vops && mictx->vops->top_io_notify)
			mictx->vops->top_io_notify(io_type, top_pages_r,
						   top_pages_w, top_rnd_cnt);
	}
	rcu_read_unlock();
}

void mtk_btag_mictx_complete_command(struct mtk_blocktag *btag, __u64 end_t,
				     __u32 tid, __u16 qid)
{
	struct mtk_btag_mictx *mictx;
	unsigned long id;

	if (!btag || tid >= BTAG_MAX_TAG)
		return;

	rcu_read_lock();
	xa_for_each(&btag->ctx.mictx_xa, id, mictx) {
		struct mtk_btag_mictx_queue *q = &mictx->q[qid];
		unsigned long flags;
#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_DEBUG_BUILD)
		__u64 time;
#endif

		if (!mictx->tags[tid].start_t)
			continue;

		/* workload */
		spin_lock_irqsave(&mictx->wl.lock, flags);
		if (!--mictx->wl.depth)
			mictx->wl.idle_begin = end_t;
		spin_unlock_irqrestore(&mictx->wl.lock, flags);

		/* throughput */
		if (mictx->full_logging) {
			spin_lock_irqsave(&q->lock, flags);
			q->tp_size[mictx->tags[tid].io_type] +=
				mictx->tags[tid].len;
			q->tp_time[mictx->tags[tid].io_type] +=
				end_t - mictx->tags[tid].start_t;
			spin_unlock_irqrestore(&q->lock, flags);
		}

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_DEBUG_BUILD)
		/* average queue depth
		 * NOTE: see the calculation in mictx_evaluate_avg_qd()
		 */
		spin_lock_irqsave(&mictx->avg_qd.lock, flags);
		time = sched_clock();
		if (mictx->full_logging) {
			mictx->avg_qd.latency += mictx->avg_qd.depth *
					(time - mictx->avg_qd.last_depth_chg);
		}
		mictx->avg_qd.last_depth_chg = time;
		mictx->avg_qd.depth--;
		spin_unlock_irqrestore(&mictx->avg_qd.lock, flags);
#endif

		/* clear tags */
		mictx->tags[tid].start_t = 0;
		mictx->tags[tid].len = 0;
	}
	rcu_read_unlock();
}

static void mictx_evaluate_workload(struct mtk_btag_mictx *mictx,
				    struct mtk_btag_mictx_iostat_struct *iostat)
{
	unsigned long flags;
	__u64 cur_time, idle_total, window_begin, idle_begin = 0;

	spin_lock_irqsave(&mictx->wl.lock, flags);
	cur_time = sched_clock();
	idle_total = mictx->wl.idle_total;
	window_begin = mictx->wl.window_begin;
	mictx->wl.idle_total = 0;
	mictx->wl.window_begin = cur_time;
	if (mictx->wl.idle_begin) {
		idle_begin = mictx->wl.idle_begin;
		mictx->wl.idle_begin = cur_time;
	}
	spin_unlock_irqrestore(&mictx->wl.lock, flags);

	if (idle_begin)
		idle_total += cur_time - idle_begin;
	iostat->wl = (__u16)(100 - div64_u64(idle_total * 100,
					     cur_time - window_begin));
	iostat->duration = cur_time - window_begin;
}

static __u32 calculate_throughput(__u64 bytes, __u64 time)
{
	__u64 tp;

	if (!bytes)
		return 0;

	do_div(time, 1000000);       /* convert ns to ms */
	if (!time)
		return 0;

	tp = div64_u64(bytes, time); /* byte/ms */
	tp = (tp * 1000) >> 10;      /* KB/s */

	return (__u32)tp;
}

static void mictx_evaluate_top(struct mtk_btag_mictx *mictx,
			       struct mtk_btag_mictx_iostat_struct *iostat)
{
	unsigned long flags;

	spin_lock_irqsave(&mictx->top.lock, flags);
	iostat->top_pages_r = mictx->top.pages[BTAG_IO_READ];
	iostat->top_pages_w = mictx->top.pages[BTAG_IO_WRITE];
	iostat->top_rnd_cnt = mictx->top.rnd_cnt;
	mictx->top.pages[BTAG_IO_READ] = 0;
	mictx->top.pages[BTAG_IO_WRITE] = 0;
	mictx->top.rnd_cnt = 0;
	spin_unlock_irqrestore(&mictx->top.lock, flags);
}

static void mictx_evaluate_queue(struct mtk_btag_mictx *mictx,
				 struct mtk_btag_mictx_iostat_struct *iostat)
{
	__u64 tp_size[BTAG_IO_TYPE_NR] = {0};
	__u64 tp_time[BTAG_IO_TYPE_NR] = {0};
	__u64 tot_len, top_len = 0;
	unsigned long flags;
	int qid;

	/* get and clear mictx queue data */
	for (qid = 0; qid < mictx->queue_nr; qid++) {
		struct mtk_btag_mictx_queue tmp, *q = &mictx->q[qid];
		int io_type;

		spin_lock_irqsave(&q->lock, flags);
		tmp = *q;
		for (io_type = 0; io_type < BTAG_IO_TYPE_NR; io_type++) {
			q->rq_size[io_type] = 0;
			q->rq_cnt[io_type] = 0;
			q->tp_size[io_type] = 0;
			q->tp_time[io_type] = 0;
		}
		q->top_len = 0;
		spin_unlock_irqrestore(&q->lock, flags);

		iostat->reqsize_r += tmp.rq_size[BTAG_IO_READ];
		iostat->reqsize_w += tmp.rq_size[BTAG_IO_WRITE];
		iostat->reqcnt_r += tmp.rq_cnt[BTAG_IO_READ];
		iostat->reqcnt_w += tmp.rq_cnt[BTAG_IO_WRITE];
		top_len += tmp.top_len;

		if (!mictx->full_logging)
			continue;

		tp_size[BTAG_IO_READ] += tmp.tp_size[BTAG_IO_READ];
		tp_size[BTAG_IO_WRITE] += tmp.tp_size[BTAG_IO_WRITE];
		tp_time[BTAG_IO_READ] += tmp.tp_time[BTAG_IO_READ];
		tp_time[BTAG_IO_WRITE] += tmp.tp_time[BTAG_IO_WRITE];
	}

	/* top rate */
	tot_len = iostat->reqsize_r + iostat->reqsize_w;
	iostat->top = tot_len ? (__u32)div64_u64(top_len * 100, tot_len) : 0;

	if (!mictx->full_logging)
		return;

	/* throughput (per-request) */
	iostat->tp_req_r = calculate_throughput(tp_size[BTAG_IO_READ],
						tp_time[BTAG_IO_READ]);
	iostat->tp_req_w = calculate_throughput(tp_size[BTAG_IO_WRITE],
						tp_time[BTAG_IO_WRITE]);

	/* throughput (overlapped, not 100% precise) */
	iostat->tp_all_r = calculate_throughput(tp_size[BTAG_IO_READ],
						iostat->duration);
	iostat->tp_all_w = calculate_throughput(tp_size[BTAG_IO_WRITE],
						iostat->duration);
}

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_DEBUG_BUILD)
/* Average Queue Depth
 * NOTE:
 *                 |<----------------- Window Time (WT) ----------------->|
 *                 |<---------- cmd1 ---------->                          |
 *                 |                 <---------- cmd2 ---------->         |
 *    time          t0               t1        t2               t3
 *    Depth Before  0                1         2                1
 *    Depth After   1                2         1                0
 *
 *    The Average Queue Depth can be calculate as:
 *      = Sum of CMD Latency / Window Time
 *      = [ (t1-t0)*1 + (t2-t1)*2 + (t3-t2)*1 ] / WT
 *      = Sum{[Depth_Change_Time(n) - Depth_Change_Time(n-1)] * Depth(n-1)} / WT
 */
static void mictx_evaluate_avg_qd(struct mtk_btag_mictx *mictx,
				  struct mtk_btag_mictx_iostat_struct *iostat)
{
	unsigned long flags;
	__u64 cur_time, latency, last_depth_chg, depth;

	spin_lock_irqsave(&mictx->avg_qd.lock, flags);
	cur_time = sched_clock();
	latency = mictx->avg_qd.latency;
	last_depth_chg = mictx->avg_qd.last_depth_chg;
	depth = mictx->avg_qd.depth;
	mictx->avg_qd.latency = 0;
	mictx->avg_qd.last_depth_chg = cur_time;
	spin_unlock_irqrestore(&mictx->avg_qd.lock, flags);

	latency += depth * (cur_time - last_depth_chg);
	iostat->q_depth = (__u16)div64_u64(latency, iostat->duration);
}
#endif

int mtk_btag_mictx_get_data(struct mtk_btag_mictx_id *mictx_id,
			    struct mtk_btag_mictx_iostat_struct *iostat)
{
	struct mtk_blocktag *btag;
	struct mtk_btag_mictx *mictx;

	if (!iostat || !mictx_id || !mictx_id->id || !mictx_id->btag_id)
		return -EINVAL;

	guard(rcu)();

	btag = mtk_btag_find_by_id(mictx_id->btag_id);
	if (!btag)
		return -ENODEV;

	mictx = mictx_find(btag, mictx_id->id);
	if (!mictx)
		return -ENOENT;

	memset(iostat, 0, sizeof(struct mtk_btag_mictx_iostat_struct));
	mictx_evaluate_workload(mictx, iostat);
	mictx_evaluate_top(mictx, iostat);
	mictx_evaluate_queue(mictx, iostat);
#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_DEBUG_BUILD)
	if (mictx->full_logging)
		mictx_evaluate_avg_qd(mictx, iostat);
#endif
	trace_blocktag_mictx_get_data(mictx->name, iostat);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_btag_mictx_get_data);

void mtk_btag_mictx_set_full_logging(struct mtk_btag_mictx_id *mictx_id,
				     bool enable)
{
	struct mtk_blocktag *btag;
	struct mtk_btag_mictx *mictx;

	if (!mictx_id || !mictx_id->id || !mictx_id->btag_id)
		return;

	guard(rcu)();

	btag = mtk_btag_find_by_id(mictx_id->btag_id);
	if (!btag)
		return;

	mictx = mictx_find(btag, mictx_id->id);
	if (!mictx)
		return;
	mictx->full_logging = enable;
}

int mtk_btag_mictx_full_logging(struct mtk_btag_mictx_id *mictx_id)
{
	struct mtk_blocktag *btag;
	struct mtk_btag_mictx *mictx;

	if (!mictx_id || !mictx_id->id || !mictx_id->btag_id)
		return -1;

	guard(rcu)();

	btag = mtk_btag_find_by_id(mictx_id->id);
	if (!btag)
		return -1;

	mictx = mictx_find(btag, mictx_id->id);
	if (!mictx)
		return -1;
	return mictx->full_logging;
}

static struct proc_dir_entry *mictx_ioctl_entry;
static DEFINE_MUTEX(entry_lock);

struct mictx_ioctl_info {
	union {
		struct mictx_register_info {
			char btag_name[BTAG_NAME_LEN];
			char mictx_name[BTAG_NAME_LEN];
		} register_info;
		struct mtk_btag_mictx_id mictx_id;
		struct mtk_btag_mictx_iostat_struct iostat;
	};
};

#define MICTX_IOC_MAGIC 'm'
#define MICTX_ENABLE	_IOW(MICTX_IOC_MAGIC, 1, struct mictx_ioctl_info)
#define MICTX_DISABLE	_IOW(MICTX_IOC_MAGIC, 2, struct mictx_ioctl_info)
#define MICTX_GET_DATA	_IOW(MICTX_IOC_MAGIC, 3, struct mictx_ioctl_info)

static long mictx_proc_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long __arg)
{
	struct mictx_ioctl_info input, output;
	const char *btag_name, *mictx_name;
	void __user *arg = (void __user *)__arg;
	int ret = 0;

	switch (cmd) {
	case MICTX_ENABLE:
		if (copy_from_user(&input, arg,
				   sizeof(struct mictx_ioctl_info))) {
			pr_notice("%s: copy from user failed for cmd %u, address %p",
				  __func__, cmd, arg);
			ret = -EFAULT;
			goto ret_ioctl;
		}
		btag_name = input.register_info.btag_name;
		mictx_name = input.register_info.mictx_name;

		ret = mtk_btag_mictx_register(&output.mictx_id, btag_name,
					      mictx_name, NULL);
		if (ret) {
			pr_notice("%s: mictx enable failed %d\n",
				  __func__, ret);
			goto ret_ioctl;
		}

		if (copy_to_user(arg, &output,
				 sizeof(struct mictx_ioctl_info))) {
			pr_notice("%s: copy to user failed for cmd %u, address %p",
				  __func__, cmd, arg);
			ret = -EFAULT;
			goto ret_ioctl;
		}
		break;
	case MICTX_DISABLE:
		if (copy_from_user(&input, arg,
				   sizeof(struct mictx_ioctl_info))) {
			pr_notice("%s: copy from user failed for cmd %u, address %p",
				  __func__, cmd, arg);
			ret = -EFAULT;
			goto ret_ioctl;
		}

		mtk_btag_mictx_unregister(&input.mictx_id);
		break;
	case MICTX_GET_DATA:
		if (copy_from_user(&input, arg,
				   sizeof(struct mictx_ioctl_info))) {
			pr_notice("%s: copy from user failed for cmd %u, address %p",
				  __func__, cmd, arg);
			ret = -EFAULT;
			goto ret_ioctl;
		}

		ret = mtk_btag_mictx_get_data(&input.mictx_id, &output.iostat);
		if (ret) {
			pr_notice("%s: mictx get data failed %d\n",
				  __func__, ret);
			goto ret_ioctl;
		}

		if (copy_to_user(arg, &output,
				 sizeof(struct mictx_ioctl_info))) {
			pr_notice("%s: copy to user failed for cmd %u, address %p",
				  __func__, cmd, arg);
			ret = -EFAULT;
			goto ret_ioctl;
		}
		break;
	default:
		pr_notice("%s: unknown cmd %u\n", __func__, cmd);
		ret = -EFAULT;
		goto ret_ioctl;
	}

ret_ioctl:
	return ret;
}

static int mictx_ioctl_seq_show(struct seq_file *m, void *v)
{
	return 0;
}

static int mictx_ioctl_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mictx_ioctl_seq_show, inode->i_private);
}

static const struct proc_ops mictx_ioctl_fops = {
	.proc_ioctl = mictx_proc_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.proc_compat_ioctl = mictx_proc_ioctl,
#endif
	.proc_open = mictx_ioctl_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

void mtk_btag_mictx_ioctl_create(struct proc_dir_entry *btag_root)
{
	if (!btag_root)
		return;

	mutex_lock(&entry_lock);

	if (mictx_ioctl_entry) {
		pr_notice("%s: mictx_ioctl exists\n", __func__);
		goto unlock;
	}

	mictx_ioctl_entry = proc_create("mictx_ioctl", S_IFREG | 0444,
					btag_root, &mictx_ioctl_fops);
	if (IS_ERR(mictx_ioctl_entry)) {
		pr_notice("%s: mictx_ioctl create failed %ld",
			  __func__, PTR_ERR(mictx_ioctl_entry));
		mictx_ioctl_entry = NULL;
	}

unlock:
	mutex_unlock(&entry_lock);
}

void mtk_btag_mictx_ioctl_remove(void)
{
	mutex_lock(&entry_lock);

	if (!mictx_ioctl_entry) {
		pr_notice("%s: mictx_ioctl doesn't exist\n", __func__);
		goto unlock;
	}
	proc_remove(mictx_ioctl_entry);
	mictx_ioctl_entry = NULL;

unlock:
	mutex_unlock(&entry_lock);
}

static int mictx_alloc(struct mtk_blocktag *btag, unsigned long *id,
		       const char *name, struct mtk_btag_mictx_vops *vops)
{
	struct mtk_btag_mictx *mictx;
	__u64 cur_time = sched_clock();
	int qid, ret;

	mictx = kzalloc(struct_size(mictx, q, btag->ctx.count), GFP_ATOMIC);
	if (!mictx)
		return -ENOMEM;

	mictx->queue_nr = btag->ctx.count;
	spin_lock_init(&mictx->wl.lock);
	mictx->wl.window_begin = cur_time;
	mictx->wl.idle_begin = cur_time;
	spin_lock_init(&mictx->top.lock);
#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_DEBUG_BUILD)
	spin_lock_init(&mictx->avg_qd.lock);
	mictx->avg_qd.last_depth_chg = cur_time;
#endif
	for (qid = 0; qid < btag->ctx.count; qid++)
		spin_lock_init(&mictx->q[qid].lock);
	mictx->vops = vops;
	mictx->full_logging = true;
	strscpy(mictx->name, name, BTAG_NAME_LEN);

	ret = xa_alloc(&btag->ctx.mictx_xa, &mictx->id, mictx, xa_limit_32b,
		       GFP_ATOMIC);
	if (ret < 0) {
		kfree(mictx);
		return ret;
	}
	*id = mictx->id;

	return 0;
}

static void mictx_free(struct mtk_blocktag *btag, __u16 id)
{
	struct mtk_btag_mictx *mictx;

	mictx = xa_erase(&btag->ctx.mictx_xa, id);
	if (!mictx)
		return;

	kfree_rcu(mictx, rcu);
}

void mtk_btag_mictx_free_all(struct mtk_blocktag *btag)
{
	struct mtk_btag_mictx *mictx;
	unsigned long id;

	xa_for_each(&btag->ctx.mictx_xa, id, mictx) {
		xa_erase(&btag->ctx.mictx_xa, id);
		kfree_rcu(mictx, rcu);
	}
}

int mtk_btag_mictx_register(struct mtk_btag_mictx_id *mictx_id,
			    const char *btag_name, const char *mictx_name,
			    struct mtk_btag_mictx_vops *vops)
{
	struct mtk_blocktag *btag;
	int ret;

	if (!mictx_id)
		return -EINVAL;

	mictx_id->id = 0;
	mictx_id->btag_id = 0;

	guard(rcu)();

	btag = mtk_btag_find_by_name(btag_name);
	if (!btag)
		return -ENOENT;

	ret = mictx_alloc(btag, &mictx_id->id, mictx_name, vops);
	if (ret < 0) {
		pr_notice("mictx alloc fail %d\n", ret);
		return ret;
	}
	mictx_id->btag_id = btag->id;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_btag_mictx_register);

void mtk_btag_mictx_unregister(struct mtk_btag_mictx_id *mictx_id)
{
	struct mtk_blocktag *btag;

	if (!mictx_id || !mictx_id->id || !mictx_id->btag_id)
		return;

	guard(rcu)();

	btag = mtk_btag_find_by_id(mictx_id->btag_id);
	if (!btag)
		return;

	mictx_free(btag, mictx_id->id);
}
EXPORT_SYMBOL_GPL(mtk_btag_mictx_unregister);

void mtk_btag_mictx_init(struct mtk_blocktag *btag)
{
	xa_init_flags(&btag->ctx.mictx_xa, XA_FLAGS_ALLOC1);
}
