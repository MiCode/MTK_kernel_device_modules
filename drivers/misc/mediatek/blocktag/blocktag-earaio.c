// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 * Authors:
 *	Chiayu Ku <chiayu.ku@mediatek.com>
 *	Stanley Chu <stanley.chu@mediatek.com>
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[blocktag][earaio]" fmt

#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "blocktag-internal.h"
#include "blocktag-fuse-trace.h"

#define EARA_IO_SYS_MAX_SIZE 27
typedef struct _EARA_IO_SYS_PACKAGE {
	union {
		__s32 cmd;
		__s32 data[EARA_IO_SYS_MAX_SIZE];
	};
} EARA_IO_SYS_PACKAGE;

#define EARA_IO_GETINDEX             _IOW('g', 1, EARA_IO_SYS_PACKAGE)
#define EARA_IO_COLLECT              _IOW('g', 2, EARA_IO_SYS_PACKAGE)
#define EARA_IO_GETINDEX2            _IOW('g', 3, EARA_IO_SYS_PACKAGE)
#define EARA_IO_SET_PARAM            _IOW('g', 4, EARA_IO_SYS_PACKAGE)

#define PARAM_RANDOM_THRESHOLD       0
#define PARAM_SEQ_R_THRESHOLD        1
#define PARAM_SEQ_W_THRESHOLD        2
#define PARAM_FUSE_THRESHOLD         3
#define PARAM_FUSE_UNLINK_THRESHOLD  4

#define ACCEL_NO                     0
#define ACCEL_NORMAL                 1
#define ACCEL_RAND                   3
#define ACCEL_SEQ                    4
#define ACCEL_FUSE                   5

#define MAX_IO_INFO_NR 2
struct earaio_iostat {
	struct _io {
		int wl;
		int top;
		int reqc_r;
		int reqc_w;
		int q_dept;
		int reqsz_top_r;
		int reqsz_top_w;
		int reqc_rand;
	} io[MAX_IO_INFO_NR];
	char io_name[MAX_IO_INFO_NR][BTAG_NAME_LEN];
	struct _fuse {
		int req_cnt;
		int unlink_cnt;
		unsigned short hot_pid;
		unsigned short hot_tgid;
	} fuse;
};

static_assert(sizeof(struct earaio_iostat) <= sizeof(EARA_IO_SYS_PACKAGE),
	      "earaio_iostat is large then earaio ioctl package");

/* mini context for major embedded storage only */
#define MICTX_PROC_CMD_BUF_SIZE (16)
#define PWD_WIDTH_NS 100000000 /* 100ms */

static DEFINE_MUTEX(eara_ioctl_lock);
static struct mtk_btag_earaio_control earaio_ctrl;

#define for_each_valid_mictx_id(mictx_id, index)			\
	for (index = 0, mictx_id = &earaio_ctrl.io_info[index].mictx_id;\
	     index < MAX_IO_INFO_NR && mictx_id->id;			\
	     mictx_id = ++index < MAX_IO_INFO_NR ?			\
	     &earaio_ctrl.io_info[index].mictx_id : NULL)

static int earaio_boost_open(struct inode *inode, struct file *file)
{
	unsigned long flags;

	spin_lock_irqsave(&earaio_ctrl.lock, flags);
	earaio_ctrl.msg_open_cnt++;
	if (earaio_ctrl.msg_open_cnt == 1)
		earaio_ctrl.pwd_begin = sched_clock();

	spin_unlock_irqrestore(&earaio_ctrl.lock, flags);
	return 0;
}

static int earaio_boost_release(struct inode *inode, struct file *file)
{
	unsigned long flags;

	spin_lock_irqsave(&earaio_ctrl.lock, flags);
	earaio_ctrl.msg_open_cnt--;

	spin_unlock_irqrestore(&earaio_ctrl.lock, flags);
	return 0;
}

static ssize_t earaio_boost_read(struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	int len = 0;

	if (earaio_ctrl.msg_buf_used_entry == 0) {
		/* buf empty */
		return 0;
	}

	if (access_ok(buf, count)) {
		len = (count < EARAIO_BOOST_ENTRY_LEN) ?
			count : EARAIO_BOOST_ENTRY_LEN;
		(void)__copy_to_user(buf,
			&earaio_ctrl.msg_buf[earaio_ctrl.msg_buf_start_idx],
			len);
		memset(&earaio_ctrl.msg_buf[earaio_ctrl.msg_buf_start_idx], 0,
			EARAIO_BOOST_ENTRY_LEN);
		earaio_ctrl.msg_buf_start_idx += EARAIO_BOOST_ENTRY_LEN;
		if (earaio_ctrl.msg_buf_start_idx == EARAIO_BOOST_BUF_SZ)
			earaio_ctrl.msg_buf_start_idx = 0;
		earaio_ctrl.msg_buf_used_entry--;
	}

	return len;
}

static unsigned int earaio_boost_pool(struct file *file, poll_table *pt)
{
	if (earaio_ctrl.msg_buf_used_entry)
		return (POLLIN | POLLRDNORM);

	poll_wait(file, &earaio_ctrl.msg_readable, pt);

	if (earaio_ctrl.msg_buf_used_entry)
		return (POLLIN | POLLRDNORM);

	return 0;
}

static const struct proc_ops earaio_boost_fops = {
	.proc_open = earaio_boost_open,
	.proc_release = earaio_boost_release,
	.proc_read = earaio_boost_read,
	.proc_poll = earaio_boost_pool,
};

static void mtk_btag_earaio_boost_fill(int boost)
{
	if (earaio_ctrl.msg_buf_used_entry == EARAIO_BOOST_ENTRY_NUM) {
		/* buf full */
		return;
	}

	earaio_ctrl.earaio_boost_state = boost;

	snprintf(&earaio_ctrl.msg_buf[earaio_ctrl.msg_buf_start_idx],
		 EARAIO_BOOST_ENTRY_LEN, "boost=%d", boost);
	earaio_ctrl.msg_buf_used_entry++;

	if (boost)
		wake_up(&earaio_ctrl.msg_readable);
}

#if IS_ENABLED(CONFIG_MTK_FUSE_TRACER)
static inline void earaio_get_fuse_count(u64 *total, u64 *unlink)
{
#if IS_ENABLED(CONFIG_CGROUP_SCHED)
	*total = mtk_btag_fuse_get_req_cnt_top(0);
	*unlink = mtk_btag_fuse_get_req_cnt_top(FUSE_UNLINK);
#else
	*total = mtk_btag_fuse_get_req_cnt(0);
	*unlink = mtk_btag_fuse_get_req_cnt(FUSE_UNLINK);
#endif
}

static void earaio_set_fuse_data(struct earaio_iostat *data)
{
	unsigned long flags;
	u64 total, unlink;

	earaio_get_fuse_count(&total, &unlink);
	spin_lock_irqsave(&earaio_ctrl.lock, flags);
	data->fuse.req_cnt = (int)(total - earaio_ctrl.fuse_total_prev);
	data->fuse.unlink_cnt = (int)(unlink - earaio_ctrl.fuse_unlink_prev);
	earaio_ctrl.fuse_total_prev = total;
	earaio_ctrl.fuse_unlink_prev = unlink;
	spin_unlock_irqrestore(&earaio_ctrl.lock, flags);
	mtk_btag_fuse_get_hot_pid(&data->fuse.hot_pid, &data->fuse.hot_tgid);
}
#endif

static void earaio_reset_data(void)
{
	struct mtk_btag_mictx_id *mictx_id;
	unsigned long flags;
	int index;

	for_each_valid_mictx_id(mictx_id, index)
		mtk_btag_mictx_reset(mictx_id);
	spin_lock_irqsave(&earaio_ctrl.lock, flags);
#if IS_ENABLED(CONFIG_MTK_FUSE_TRACER)
	earaio_get_fuse_count(&earaio_ctrl.fuse_total_prev,
			      &earaio_ctrl.fuse_unlink_prev);
#endif
	earaio_ctrl.pwd_begin = sched_clock();
	spin_unlock_irqrestore(&earaio_ctrl.lock, flags);
}

static void mtk_btag_eara_get_data(struct earaio_iostat *data)
{
	struct mtk_btag_mictx_iostat_struct iostat = {0};
	struct mtk_btag_mictx_id *mictx_id;
	unsigned long flags;
	int index;

	WARN_ON(!mutex_is_locked(&eara_ioctl_lock));

	for_each_valid_mictx_id(mictx_id, index) {
		mtk_btag_mictx_get_data(mictx_id, &iostat);
		data->io[index].wl = iostat.wl;
		data->io[index].top = iostat.top;
		data->io[index].reqc_r = iostat.reqcnt_r;
		data->io[index].reqc_w = iostat.reqcnt_w;
		data->io[index].q_dept = iostat.q_depth;
		data->io[index].reqc_rand = iostat.top_rnd_cnt;
		data->io[index].reqsz_top_r = iostat.top_pages_r << PAGE_SHIFT;
		data->io[index].reqsz_top_w = iostat.top_pages_w << PAGE_SHIFT;
		strscpy(data->io_name[index], earaio_ctrl.io_info[index].name,
			BTAG_NAME_LEN);
	}
#if IS_ENABLED(CONFIG_MTK_FUSE_TRACER)
	earaio_set_fuse_data(data);
#endif
	spin_lock_irqsave(&earaio_ctrl.lock, flags);
	earaio_ctrl.pwd_begin = sched_clock();
	spin_unlock_irqrestore(&earaio_ctrl.lock, flags);
}

#define EARAIO_BOOST_EVAL_THRESHOLD_PAGES ((32 * 1024 * 1024) >> PAGE_SHIFT)

/**
 * earaio_try_boost - try to send an ACCEL_NORMAL boost message if needed
 * @boost:	Whether to boost or not
 *
 * This function attempts to send an ACCEL_NORMAL message based on the @boost.
 *
 * If @boost is false, this always returns 0 indicating a successful unboost.
 * If @boost is true, this function returns:
 *   - 0: ACCEL_NORMAL message is prepared for the earaio service.
 *   - 1: no need to send ACCEL_NORMAL, threshold cannot be reached
 *   - 2: no need to send ACCEL_NORMAL, disable or no user
 *   - 3: no need to send ACCEL_NORMAL, already in boosted
 */
static int earaio_try_boost(bool boost)
{
	struct mtk_btag_mictx_id *mictx_id;
	unsigned long flags;
	__u32 top_r_sum = 0, top_w_sum = 0;
	__u32 top_r, top_w;
	int index, ret = 1;
#if IS_ENABLED(CONFIG_MTK_FUSE_TRACER)
	u64 fuse_total, fuse_unlink;
	__u32 total_top_rw = 0;
#endif

	spin_lock_irqsave(&earaio_ctrl.lock, flags);

	if (!boost) {
		/*
		 * It does not harm to set start_collect as false without
		 * checking original start_collect
		 */
		earaio_ctrl.start_collect = false;
		earaio_ctrl.earaio_boost_state = ACCEL_NO;
		earaio_ctrl.boosted = boost;
		ret = 0;
		goto end;
	}

	if (!earaio_ctrl.enabled || !earaio_ctrl.earaio_boost_entry ||
	    !earaio_ctrl.msg_open_cnt) {
		ret = 2;
		goto end;
	}

	/* TODO: we need to check earaio_boost_state also ? */
	if (earaio_ctrl.boosted || earaio_ctrl.earaio_boost_state) {
		ret = 3;
		goto end;
	}

	/* Establish threshold for top app read, write */
	for_each_valid_mictx_id(mictx_id, index) {
		top_r = 0;
		top_w = 0;
		mtk_btag_mictx_get_top_rw(mictx_id, &top_r, &top_w);
#if IS_ENABLED(CONFIG_MTK_FUSE_TRACER)
		total_top_rw += (top_r + top_w);
#endif
		top_r_sum += top_r;
		top_w_sum += top_w;
	}
	if (top_r_sum >= EARAIO_BOOST_EVAL_THRESHOLD_PAGES ||
	    top_w_sum >= EARAIO_BOOST_EVAL_THRESHOLD_PAGES)
		goto need_boost;

#if IS_ENABLED(CONFIG_MTK_FUSE_TRACER)
	/* Establish threshold for top app fuse request count */
	earaio_get_fuse_count(&fuse_total, &fuse_unlink);
	if ((fuse_total - earaio_ctrl.fuse_total_prev >
	      earaio_ctrl.fuse_threshold) ||
	    (fuse_unlink - earaio_ctrl.fuse_unlink_prev >
	     earaio_ctrl.fuse_unlink_threshold))
		goto need_boost;
#endif

end:
	spin_unlock_irqrestore(&earaio_ctrl.lock, flags);
	return ret;

need_boost:
	mtk_btag_earaio_boost_fill(ACCEL_NORMAL);
	earaio_ctrl.boosted = boost;
	spin_unlock_irqrestore(&earaio_ctrl.lock, flags);
	return 0;
}

static void mtk_btag_eara_start_collect(void)
{
	unsigned long flags;

	spin_lock_irqsave(&earaio_ctrl.lock, flags);
	WARN_ON(!mutex_is_locked(&eara_ioctl_lock));
	if (!earaio_ctrl.start_collect)
		earaio_ctrl.start_collect = true;
	spin_unlock_irqrestore(&earaio_ctrl.lock, flags);

	earaio_reset_data();
}

static void mtk_btag_eara_stop_collect(void)
{
	/*
	 * Set earaio_ctrl.start_collect as false in earaio_try_boost()
	 * to avoid get earaio_ctrl.lock twice
	 */
	earaio_try_boost(false);

	WARN_ON(!mutex_is_locked(&eara_ioctl_lock));
}

static void mtk_btag_eara_switch_collect(int cmd)
{
	mutex_lock(&eara_ioctl_lock);

	if (cmd)
		mtk_btag_eara_start_collect();
	else
		mtk_btag_eara_stop_collect();

	mutex_unlock(&eara_ioctl_lock);
}

static void mtk_btag_eara_transfer_data(__s32 *data, __s32 input_size)
{
	struct earaio_iostat earaio_data = {0};
	int limit_size;

	mutex_lock(&eara_ioctl_lock);
	mtk_btag_eara_get_data(&earaio_data);
	mutex_unlock(&eara_ioctl_lock);

	limit_size = MIN(input_size, sizeof(struct earaio_iostat));
	memcpy(data, &earaio_data, limit_size);
}

static unsigned long eara_ioctl_copy_from_user(void *pvTo,
		const void __user *pvFrom, unsigned long ulBytes)
{
	if (access_ok(pvFrom, ulBytes))
		return __copy_from_user(pvTo, pvFrom, ulBytes);

	return ulBytes;
}

static void mtk_btag_eara_set_param(int cmd)
{
	unsigned int param_idx, param_val;

	param_idx = ((unsigned int)cmd) >> 24;
	param_val = cmd & 0xffffff;

	switch (param_idx) {
	case PARAM_RANDOM_THRESHOLD:
		earaio_ctrl.rand_rw_threshold = param_val;
		break;
	case PARAM_SEQ_R_THRESHOLD:
		/* << 8 to convert mb as pages*/
		earaio_ctrl.seq_r_threshold = param_val << 8;
		break;
	case PARAM_SEQ_W_THRESHOLD:
		/* << 8 to convert mb as pages*/
		earaio_ctrl.seq_w_threshold = param_val << 8;
		break;
	case PARAM_FUSE_THRESHOLD:
		earaio_ctrl.fuse_threshold = param_val;
		break;
	}
}

static unsigned long eara_ioctl_copy_to_user(void __user *pvTo,
		const void *pvFrom, unsigned long ulBytes)
{
	if (access_ok(pvTo, ulBytes))
		return __copy_to_user(pvTo, pvFrom, ulBytes);

	return ulBytes;
}

static long earaio_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	ssize_t ret = 0;
	EARA_IO_SYS_PACKAGE *msgKM = NULL;
	EARA_IO_SYS_PACKAGE *msgUM = (EARA_IO_SYS_PACKAGE *)arg;
	EARA_IO_SYS_PACKAGE smsgKM = {0};
	unsigned long flags;

	msgKM = &smsgKM;

	switch (cmd) {
	case EARA_IO_GETINDEX:
		spin_lock_irqsave(&earaio_ctrl.lock, flags);
		if (earaio_ctrl.earaio_boost_state != ACCEL_NORMAL)
			//fix me: determine shall be 1 or 0
			earaio_ctrl.earaio_boost_state = ACCEL_NORMAL;
		spin_unlock_irqrestore(&earaio_ctrl.lock, flags);
		mtk_btag_eara_transfer_data(smsgKM.data,
				sizeof(EARA_IO_SYS_PACKAGE));
		eara_ioctl_copy_to_user(msgUM, msgKM,
				sizeof(EARA_IO_SYS_PACKAGE));
		break;
	case EARA_IO_COLLECT:
		if (eara_ioctl_copy_from_user(msgKM, msgUM,
				sizeof(EARA_IO_SYS_PACKAGE))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}
		mtk_btag_eara_switch_collect(msgKM->cmd);
		break;
	case EARA_IO_GETINDEX2:
		mtk_btag_eara_transfer_data(smsgKM.data,
				sizeof(EARA_IO_SYS_PACKAGE));
		eara_ioctl_copy_to_user(msgUM, msgKM,
				sizeof(EARA_IO_SYS_PACKAGE));
		break;
	case EARA_IO_SET_PARAM:
		if (eara_ioctl_copy_from_user(msgKM, msgUM,
				sizeof(EARA_IO_SYS_PACKAGE))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}
		mtk_btag_eara_set_param(msgKM->cmd);
		break;
	default:
		pr_debug("proc_ioctl: unknown cmd %x\n", cmd);
		ret = -EINVAL;
		goto ret_ioctl;
	}

ret_ioctl:
	return ret;
}

static int earaio_ioctl_show(struct seq_file *m, void *v)
{
	return 0;
}

static int earaio_ioctl_open(struct inode *inode, struct file *file)
{
	return single_open(file, earaio_ioctl_show, inode->i_private);
}

static const struct proc_ops earaio_ioctl_fops = {
	.proc_ioctl = earaio_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.proc_compat_ioctl = earaio_ioctl,
#endif
	.proc_open = earaio_ioctl_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static ssize_t earaio_control_write(struct file *file, const char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	struct mtk_btag_mictx_id *mictx_id;
	int index, ret;
	char cmd[MICTX_PROC_CMD_BUF_SIZE] = {0};

	if (!count)
		goto err;

	if (count > MICTX_PROC_CMD_BUF_SIZE) {
		pr_info("proc_write: command too long\n");
		goto err;
	}

	ret = copy_from_user(cmd, ubuf, count);
	if (ret < 0)
		goto err;

	/* remove line feed */
	cmd[count - 1] = 0;

	if (!strcmp(cmd, "0")) {
		earaio_try_boost(false);
		earaio_ctrl.enabled = false;
		pr_info("EARA-IO QoS Disable\n");
	} else if (!strcmp(cmd, "1")) {
		earaio_ctrl.enabled = true;
		pr_info("EARA-IO QoS Enable\n");
	} else if (!strcmp(cmd, "2")) {
		for_each_valid_mictx_id(mictx_id, index)
			mtk_btag_mictx_set_full_logging(mictx_id, false);
		pr_info("EARA-IO Full Logging Disable\n");
	} else if (!strcmp(cmd, "3")) {
		for_each_valid_mictx_id(mictx_id, index)
			mtk_btag_mictx_set_full_logging(mictx_id, true);
		pr_info("EARA-IO Full Logging Enable\n");
	} else {
		pr_info("proc_write: invalid cmd %s\n", cmd);
		goto err;
	}

	return count;
err:
	return -1;
}

static int earaio_control_show(struct seq_file *s, void *data)
{
	struct mtk_btag_mictx_id *mictx_id;
	int index;

	seq_puts(s, "<MTK EARA-IO Control Unit>\n");

	seq_printf(s, "Monitor Storage:");
	for_each_valid_mictx_id(mictx_id, index)
		seq_printf(s, " [%s]", earaio_ctrl.io_info[index].name);
	seq_printf(s, "\n");

	seq_puts(s, "Status:\n");
	seq_printf(s, "  EARA-IO Control     : %s\n",
		   earaio_ctrl.enabled ? "Enable" : "Disable");

	seq_printf(s, "  EARA-IO Full Loging :");
	rcu_read_lock();
	for_each_valid_mictx_id(mictx_id, index) {
		seq_printf(s, " [%s]", mtk_btag_mictx_full_logging(mictx_id) ?
			   "Enable" : "Disable");
	}
	rcu_read_unlock();
	seq_printf(s, "\n");

	seq_puts(s, "Commands: echo n > earaio_ctrl, n presents\n");
	seq_puts(s, "  Disable EARA-IO QoS  : 0\n");
	seq_puts(s, "  Enable EARA-IO QoS   : 1\n");
	seq_puts(s, "  Disable Full Logging : 2\n");
	seq_puts(s, "  Enable Full Logging  : 3\n");
	return 0;
}

static const struct seq_operations earaio_control_seq_ops = {
	.start  = mtk_btag_seq_debug_start,
	.next   = mtk_btag_seq_debug_next,
	.stop   = mtk_btag_seq_debug_stop,
	.show   = earaio_control_show,
};

static int earaio_control_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &earaio_control_seq_ops);
}

static const struct proc_ops earaio_control_fops = {
	.proc_open		= earaio_control_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= seq_release,
	.proc_write		= earaio_control_write,
};

void mtk_btag_earaio_check_window(void)
{
	if ((sched_clock() - earaio_ctrl.pwd_begin) < PWD_WIDTH_NS)
		return;

	/* only reset data when the threshold has not been reached */
	if (earaio_try_boost(true) == 1)
		earaio_reset_data();
}

static void earaio_top_io_notify(enum mtk_btag_io_type type, __u32 top_pages_r,
				 __u32 top_pages_w, __u32 top_rnd_cnt)
{
	int early_notification = 0;
	unsigned long flags;

	spin_lock_irqsave(&earaio_ctrl.lock, flags);

	if (earaio_ctrl.earaio_boost_state != ACCEL_RAND) {
		if (top_rnd_cnt > earaio_ctrl.rand_rw_threshold) {
			early_notification = ACCEL_RAND;
			mtk_btag_earaio_boost_fill(ACCEL_RAND);
			if (!earaio_ctrl.start_collect) {
				earaio_ctrl.start_collect = true;
				earaio_ctrl.boosted = true;
			}
		}
	}

	if (earaio_ctrl.earaio_boost_state != ACCEL_SEQ &&
	    earaio_ctrl.earaio_boost_state != ACCEL_RAND) {
		if (type == BTAG_IO_READ &&
		    top_pages_r > earaio_ctrl.seq_r_threshold) {
			early_notification = ACCEL_SEQ;
			mtk_btag_earaio_boost_fill(ACCEL_SEQ);
			if (!earaio_ctrl.start_collect) {
				earaio_ctrl.start_collect = true;
				earaio_ctrl.boosted = true;
			}
		} else if (type == BTAG_IO_WRITE && top_pages_r == 0 &&
			   top_pages_w > earaio_ctrl.seq_w_threshold) {
			early_notification = ACCEL_SEQ;
			mtk_btag_earaio_boost_fill(ACCEL_SEQ);
			if (!earaio_ctrl.start_collect) {
				earaio_ctrl.start_collect = true;
				earaio_ctrl.boosted = true;
			}
		}
	}

	spin_unlock_irqrestore(&earaio_ctrl.lock, flags);

	if (!early_notification)
		mtk_btag_earaio_check_window();
}

static struct mtk_btag_mictx_vops mictx_earaio_vops = {
	.top_io_notify = earaio_top_io_notify,
};

void mtk_btag_earaio_register(const char *btag_name)
{
	struct mtk_btag_mictx_id *mictx_id;
	unsigned long flags;
	int i, ret;

	spin_lock_irqsave(&earaio_ctrl.lock, flags);

	if (!earaio_ctrl.enabled) {
		earaio_ctrl.enabled = true;
		earaio_ctrl.pwd_begin = sched_clock();
	}

	for (i = 0; i < MAX_IO_INFO_NR; i++)
		if (!earaio_ctrl.io_info[i].mictx_id.id)
			break;
	if (i == MAX_IO_INFO_NR)
		goto unlock;

	mictx_id = &earaio_ctrl.io_info[i].mictx_id;
	snprintf(earaio_ctrl.io_info[i].name, BTAG_NAME_LEN,
		 "earaio_%s", btag_name);
	ret = mtk_btag_mictx_register(mictx_id, btag_name,
				      earaio_ctrl.io_info[i].name,
				      &mictx_earaio_vops);
	if (ret) {
		mictx_id->id = 0;
		pr_notice("earaio mictx enable failed: %d\n", ret);
		goto unlock;
	}

	/* Disable Full Logging for earaio by default */
	mtk_btag_mictx_set_full_logging(mictx_id, false);

unlock:
	spin_unlock_irqrestore(&earaio_ctrl.lock, flags);

	if (i == MAX_IO_INFO_NR)
		pr_notice("register %s failed, no empty mictx_id\n", btag_name);
}

void mtk_btag_earaio_init(struct proc_dir_entry *root)
{
	struct proc_dir_entry *proc_root, *proc_entry;

	spin_lock_init(&earaio_ctrl.lock);
	earaio_ctrl.rand_rw_threshold = THRESHOLD_MAX;
	earaio_ctrl.seq_r_threshold = THRESHOLD_MAX;
	earaio_ctrl.seq_w_threshold = THRESHOLD_MAX;
	earaio_ctrl.fuse_threshold = THRESHOLD_MAX;

	proc_root = proc_mkdir("earaio", root);
	if (IS_ERR(proc_root)) {
		pr_err("proc_mkdir earaio failed: %ld\n",
		       PTR_ERR(proc_root));
		return;
	}

	proc_entry = proc_create("control", S_IFREG | 0444, proc_root,
				 &earaio_control_fops);
	if (IS_ERR(proc_entry)) {
		pr_err("proc_create control failed: %ld\n",
		       PTR_ERR(proc_entry));
		return;
	}

	proc_entry = proc_create("ioctl", 0664, proc_root,
				 &earaio_ioctl_fops);
	if (IS_ERR(proc_entry)) {
		pr_err("proc_create ioctl failed: %ld\n",
		       PTR_ERR(proc_entry));
		return;
	}

	proc_entry = proc_create("boost", 0440, proc_root,
				 &earaio_boost_fops);
	if (IS_ERR(proc_entry)) {
		pr_err("proc_create boost failed: %ld\n",
		       PTR_ERR(proc_entry));
		return;
	}

	earaio_ctrl.earaio_boost_entry = proc_entry;
	init_waitqueue_head(&earaio_ctrl.msg_readable);
}
