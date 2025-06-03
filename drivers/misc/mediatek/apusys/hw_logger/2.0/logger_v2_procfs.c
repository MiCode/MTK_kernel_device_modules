// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/poll.h>
#include <linux/workqueue.h>

#include "hw_logger.h"
#include "logger_v2_ipi.h"
#include "logger_v2_procfs.h"
#include "logger_v2.h"

static unsigned int g_log_level;
static struct proc_dir_entry *proc_root;
static struct proc_dir_entry *proc_log_lv;
static struct proc_dir_entry *proc_debug_attr;
static struct proc_dir_entry *proc_seq_block;
static struct proc_dir_entry *proc_seq_nonblock;
static struct proc_dir_entry *proc_raw_np_log;
static struct delayed_work apusys_mblog_delayed_work;
static struct workqueue_struct *apusys_mblog_wq;
static wait_queue_head_t apusys_mblog_wait_queue;

enum e_seq_mode {
	SEQ_MODE_NONBLOCK,
	SEQ_MODE_BLOCK,
	SEQ_MODE_MB_LOG,
};

struct seq_local_data
{
	char *buf_base;
	unsigned int buf_size;
	unsigned int r_ofs;
	unsigned int padding_to_flush;
	loff_t write_pos;
	enum e_seq_mode seq_mode;
};

struct seq_global_data
{
	bool init;
	loff_t pos;
	unsigned int r_ofs;
	unsigned int buf_from_irq;
};

static struct seq_global_data prev_block_session = {
	.init = false,
	.pos = 0,
	.r_ofs = 0,
	.buf_from_irq = 0,
};
static struct seq_global_data prev_mb_log_session = {
	.init = false,
	.pos = 0,
	.r_ofs = 0,
	.buf_from_irq = 0,
};
static DEFINE_SPINLOCK(prev_block_session_lock);
static DEFINE_SPINLOCK(prev_mb_log_session_lock);


static void notify_mblog_work(struct work_struct *work)
{
	HWLOGR_DBG("+");
	wake_up_all(&apusys_mblog_wait_queue);
	HWLOGR_DBG("-");
}

void logger_v2_notify_mblog(unsigned int ms)
{
	HWLOGR_DBG("+");

	if (ms == 0)
		wake_up_all(&apusys_mblog_wait_queue);
	else {
		queue_delayed_work(apusys_mblog_wq,
			&apusys_mblog_delayed_work,
			msecs_to_jiffies(ms));
	}
	HWLOGR_DBG("-");
}

static ssize_t proc_clear_buffer(struct file *flip,
	const char __user *buffer, size_t count, loff_t *f_pos,
	enum LOG_BUFF_TYPE buff_type)
{
	int ret;
	char meta_buf[PROC_WRITE_BUFSIZE];

	if (count + 1 > PROC_WRITE_BUFSIZE)
		return -EFAULT;

	ret = copy_from_user(meta_buf, buffer, count);
	if (ret) {
		HWLOGR_ERR("copy_from_user failed (%d)\n", ret);
		goto out;
	}

	meta_buf[count] = '\0';
	HWLOGR_INFO("cmd = %s\n", meta_buf);
	if (!strncmp(meta_buf, CLEAR_LOG_CMD, strlen(CLEAR_LOG_CMD)))
		logger_v2_clear_buf(buff_type);

out:
	return count;
}

static ssize_t proc_clear_write_np(struct file *flip,
	const char __user *buffer, size_t count, loff_t *f_pos)
{
	return proc_clear_buffer(flip, buffer, count, f_pos, LOG_BUFF_NP);
}

static int proc_raw_np_log_seq_show(struct seq_file *s, void *v)
{
	char *buf_base = NULL;
	unsigned int buf_size = 0;

	logger_v2_get_buf_info(LOG_BUFF_NP, &buf_base, &buf_size);
	logger_v2_buf_invalidate(LOG_BUFF_NP);
	seq_write(s, buf_base, buf_size);
	return 0;
}

static int proc_raw_np_log_sqopen(struct inode *inode, struct file *file)
{
	return single_open(file, proc_raw_np_log_seq_show, NULL);
}

static ssize_t proc_log_level_write(struct file *flip,
	const char __user *buffer, size_t count, loff_t *f_pos)
{
	int ret;
	char meta_buf[PROC_WRITE_BUFSIZE] = {0};
	unsigned int log_level = 0;

	if (count + 1 >= PROC_WRITE_BUFSIZE)
		return -ENOMEM;

	ret = copy_from_user(meta_buf, buffer, count);
	if (ret) {
		HWLOGR_ERR("copy_from_user failed (%d)\n", ret);
		goto out;
	}

	meta_buf[count] = '\0';
	HWLOGR_INFO("cmd = %s\n", meta_buf);
	ret = kstrtouint(meta_buf, 0, &log_level);
	if (ret) {
		HWLOGR_ERR("kstrtouint failed (%d)\n", ret);
		goto out;
	}
	HWLOGR_INFO("log_level = %u\n", log_level);
	set_log_level(log_level);
	g_log_level = log_level;

out:
	return count;
}

static int proc_log_level_seq_show(struct seq_file *s, void *v)
{
	seq_printf(s, "hw logger log level: %d\n", g_log_level);
	return 0;
}

static int proc_log_level_sqopen(struct inode *inode, struct file *file)
{
	return single_open(file, proc_log_level_seq_show, NULL);
}

static int proc_debug_attr_seq_show(struct seq_file *s, void *v)
{
	unsigned long flags;

	spin_lock_irqsave(&prev_block_session_lock, flags);
	seq_printf(s, "prev_block_session_r_ofs: 0x%08x \n "
				"prev_block_session_pos: %lld \n",
				prev_block_session.r_ofs, prev_block_session.pos);
	spin_unlock_irqrestore(&prev_block_session_lock, flags);

	spin_lock_irqsave(&prev_mb_log_session_lock, flags);
	seq_printf(s, "prev_mb_log_session_r_ofs: 0x%08x \n "
				"prev_mb_log_session_pos: %lld \n",
				prev_mb_log_session.r_ofs, prev_mb_log_session.pos);
	spin_unlock_irqrestore(&prev_mb_log_session_lock, flags);
	return 0;
}

static ssize_t proc_debug_attr_write(struct file *flip,
	const char __user *buffer, size_t count, loff_t *f_pos)
{
	int ret;
	char meta_buf[PROC_WRITE_BUFSIZE] = {0};
	unsigned int val;

	if (count + 1 >= PROC_WRITE_BUFSIZE)
		return -ENOMEM;

	ret = copy_from_user(meta_buf, buffer, count);
	if (ret) {
		HWLOGR_ERR("copy_from_user failed (%d)\n", ret);
		goto out;
	}

	meta_buf[count] = '\0';
	ret = kstrtouint(meta_buf, PROC_WRITE_BUFSIZE, &val);
	if (ret) {
		HWLOGR_ERR("kstrtouint failed (%d)\n", ret);
		goto out;
	}

	if (val < DBG_LOG_ERR || val > DBG_LOG_DEBUG) {
		HWLOGR_ERR("set g_hw_logger_log_lv failed (%u)\n", val);
	} else {
		g_hw_logger_log_lv = val;
		HWLOGR_INFO("g_hw_logger_log_lv: %u\n", g_hw_logger_log_lv);
	}

out:
	return count;
}

static int proc_debug_attr_sqopen(struct inode *inode, struct file *file)
{
	return single_open(file, proc_debug_attr_seq_show, NULL);
}

static void seq_write_wrapper(struct seq_file *s, char *data, size_t len)
{
	unsigned int bin_size, bin_start, offset = 0, prev_is_bin = 0;

	HWLOGR_DBG("+");

	while (offset < len) {
		bin_start = offset;
		bin_size = 0;

		// get non-binary data offset
		while (offset < len &&
			data[offset] == HWLOG_BINARY_HEADER &&
			data[offset+1] == HWLOG_BINARY_HEADER) {
			offset += HWLOG_LINE_SIZE;
			bin_size += HWLOG_LINE_SIZE;
			prev_is_bin = 1;
		}

		// flush binary data
		if (bin_size) {
			seq_write(s, data + bin_start, bin_size);
			HWLOGR_DBG("seq_write s->count: %lu src: %p size: %u\n",
				s->count, data + bin_start, bin_size);

			// if eof, break
			if (offset >= len)
				break;
		}

		// flush string data
		if (data[offset] != '\0') {
			if (prev_is_bin)
				seq_putc(s, '\n');

			// force null-terminated and new line
			if (data[offset + HWLOG_LINE_SIZE - 1] != '\0') {
				data[offset + HWLOG_LINE_SIZE - 1] = '\0';
			}
			seq_puts(s, data + offset);
			HWLOGR_DBG("seq_puts s->count: %lu src: %p\n",
				s->count, data + offset);
			prev_is_bin = 0;
		}
		offset += HWLOG_LINE_SIZE;
	}
	HWLOGR_DBG("-");
}

static int seq_show(struct seq_file *s, void *v)
{
	struct seq_local_data *session = (struct seq_local_data *)v;
	size_t start_count;

	HWLOGR_DBG("+");
	HWLOGR_DBG("buf_base: %p buf_size: 0x%08x seq_mode: %d s->size: %lu\n",
				session->buf_base, session->buf_size, session->seq_mode,
				s->size);

	if (!s->buf) {
		HWLOGR_ERR("no seq buffer\n");
		return -ENOMEM;
	}

	logger_v2_buf_invalidate(LOG_BUFF_NP);
	start_count = s->count;

	if (session->r_ofs + session->padding_to_flush <= session->buf_size) {
		HWLOGR_DBG("seq_write_wrapper s->count: %lu src: %p size: %u\n",
			s->count,
			session->buf_base + session->r_ofs,
			session->padding_to_flush);
		seq_write_wrapper(s,
			session->buf_base + session->r_ofs,
			session->padding_to_flush);
	} else {
		HWLOGR_DBG("seq_write_wrapper s->count: %lu src: %p size: %u\n",
			s->count,
			session->buf_base + session->r_ofs,
			session->buf_size - session->r_ofs);
		seq_write_wrapper(s,
			session->buf_base + session->r_ofs,
			session->buf_size - session->r_ofs);
		HWLOGR_DBG("seq_write_wrapper s->count: %lu src: %p size: %u\n",
			s->count,
			session->buf_base,
			session->padding_to_flush + session->r_ofs - session->buf_size);
		seq_write_wrapper(s,
			session->buf_base,
			session->padding_to_flush + session->r_ofs - session->buf_size);
	}

	if (session->seq_mode == SEQ_MODE_NONBLOCK)
		logger_v2_debug_info_dump(s);

	session->write_pos = s->count - start_count;

	HWLOGR_DBG("s->size %ld", s->size);
	HWLOGR_DBG("s->from %ld", s->from);
	HWLOGR_DBG("s->count %ld", s->count);
	HWLOGR_DBG("s->pad_until %ld", s->pad_until);
	HWLOGR_DBG("s->index %lld", s->index);
	HWLOGR_DBG("s->read_pos %lld", s->read_pos);
	HWLOGR_DBG("-");
	return 0;
}

static void *seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct seq_local_data *session = (struct seq_local_data *)v;
	unsigned long flags;

	HWLOGR_DBG("+");
	HWLOGR_DBG("pos: %lld buf_base: 0x%p buf_size: 0x%08x "
				"padding_to_flush: 0x%08x r_ofs: 0x%08x seq_mode: %d\n",
				*pos, session->buf_base, session->buf_size,
				session->padding_to_flush, session->r_ofs,
				session->seq_mode);

	*pos = *pos + session->write_pos;
	session->r_ofs += session->padding_to_flush;

	if (session->r_ofs >= session->buf_size)
		session->r_ofs -= session->buf_size;

	/* only pos can pass to next seq_start */
	/* so we backup current r_ofs and pos in global var */
	switch (session->seq_mode) {
	case SEQ_MODE_BLOCK:
		spin_lock_irqsave(&prev_block_session_lock, flags);
		prev_block_session.r_ofs = session->r_ofs;
		prev_block_session.pos = *pos;
		spin_unlock_irqrestore(&prev_block_session_lock, flags);
		break;
	case SEQ_MODE_MB_LOG:
		spin_lock_irqsave(&prev_mb_log_session_lock, flags);
		prev_mb_log_session.r_ofs = session->r_ofs;
		prev_mb_log_session.pos = *pos;
		spin_unlock_irqrestore(&prev_mb_log_session_lock, flags);
		break;
	default:
		break;
	}

	HWLOGR_DBG("s->size %ld", s->size);
	HWLOGR_DBG("s->from %ld", s->from);
	HWLOGR_DBG("s->count %ld", s->count);
	HWLOGR_DBG("s->pad_until %ld", s->pad_until);
	HWLOGR_DBG("s->index %lld", s->index);
	HWLOGR_DBG("s->read_pos %lld", s->read_pos);
	HWLOGR_DBG("-");
	return NULL;
}

static unsigned int block_and_wait_log(
	unsigned int timeout, struct seq_local_data *session)
{
	unsigned int raw_w_ofs = 0, cnt = 0;

	do {
		raw_w_ofs = logger_v2_get_w_ofs();

		if (session->r_ofs != raw_w_ofs) {
			if (raw_w_ofs > session->r_ofs)
				return raw_w_ofs - session->r_ofs;
			else
				return raw_w_ofs + session->buf_size - session->r_ofs;
		}

		/* sleep and check if ctrl-c input */
		usleep_range(WAIT_LOG_INTERVAL_MIN, WAIT_LOG_INTERVAL_MAX);
	} while(!signal_pending(current) && cnt++ < timeout);

	return 0;
}

static void *seq_start(struct seq_file *s, loff_t *pos, bool block_mode)
{
	struct seq_local_data *session;
	unsigned int raw_w_ofs = 0, raw_buf_size = 0;
	unsigned long flags;
	char *raw_buf_base, *new_seq_buf;
	enum e_seq_mode seq_mode;

	if (block_mode) {
		if (s->file && s->file->f_flags & O_NONBLOCK) {
			/* mobilelog user cat seq_logl */
			seq_mode = SEQ_MODE_MB_LOG;
		} else {
			/* normal user cat seq_logl */
			seq_mode = SEQ_MODE_BLOCK;
		}
	} else {
		/* normal user cat seq_log */
		seq_mode = SEQ_MODE_NONBLOCK;
	}

	HWLOGR_DBG("seq_mode: %d pos: %lld\n", seq_mode, *pos);
	HWLOGR_DBG("s->size %ld", s->size);
	HWLOGR_DBG("s->from %ld", s->from);
	HWLOGR_DBG("s->count %ld", s->count);
	HWLOGR_DBG("s->pad_until %ld", s->pad_until);
	HWLOGR_DBG("s->index %lld", s->index);
	HWLOGR_DBG("s->read_pos %lld", s->read_pos);

	if (*pos > s->read_pos) {
		HWLOGR_DBG("flush only");
		return NULL;
	}
	/* pos will be zero if no previous session.
		only iterate one time */
	if (seq_mode == SEQ_MODE_NONBLOCK && *pos > 0) {
		HWLOGR_DBG("eof");
		return NULL;
	}

	logger_v2_get_buf_info(LOG_BUFF_NP, &raw_buf_base, &raw_buf_size);

	session = kzalloc(sizeof(struct seq_local_data), GFP_KERNEL);
	if (!session)
		return NULL;

	session->seq_mode = seq_mode;
	session->buf_base = raw_buf_base;
	session->buf_size = raw_buf_size;

	switch(seq_mode) {
	case SEQ_MODE_NONBLOCK:
		raw_w_ofs = logger_v2_get_w_ofs();
		session->r_ofs = raw_w_ofs;
		session->padding_to_flush = raw_buf_size;
		break;
	case SEQ_MODE_MB_LOG:
		spin_lock_irqsave(&prev_mb_log_session_lock, flags);
		if (!prev_mb_log_session.init) {
			prev_mb_log_session.init = true;
			spin_unlock_irqrestore(&prev_mb_log_session_lock, flags);
			/* show whole raw buffer */
			raw_w_ofs = logger_v2_get_w_ofs();
			session->r_ofs = raw_w_ofs;
			session->padding_to_flush = raw_buf_size;
		} else {
			session->r_ofs = prev_mb_log_session.r_ofs;
			spin_unlock_irqrestore(&prev_mb_log_session_lock, flags);
			session->padding_to_flush =\
				block_and_wait_log(MB_LOG_WAIT_TIMEOUT, session);
			if (session->padding_to_flush == 0) {
				kfree(session);
				return NULL;
			}
		}
		break;
	case SEQ_MODE_BLOCK:
		spin_lock_irqsave(&prev_block_session_lock, flags);
		if (*pos != 0 && *pos == prev_block_session.pos) {
			session->r_ofs = prev_block_session.r_ofs;
			spin_unlock_irqrestore(&prev_block_session_lock, flags);

			session->padding_to_flush =\
				block_and_wait_log(BLOCK_LOG_WAIT_TIMEOUT, session);
			if (session->padding_to_flush == 0) {
				kfree(session);
				return NULL;
			}
		} else {
			if (*pos != 0)
				HWLOGR_INFO("get prev session fail current pos %lld,"
					" previous pos %lld\n", *pos, prev_block_session.pos);
			spin_unlock_irqrestore(&prev_block_session_lock, flags);
			/* show whole raw buffer */
			raw_w_ofs = logger_v2_get_w_ofs();
			session->r_ofs = raw_w_ofs;
			session->padding_to_flush = raw_buf_size;
		}
		break;
	}

	HWLOGR_DBG("s->size: %lu s->count: %lu "
				"padding_to_flush: 0x%08x r_ofs: 0x%08x\n",
				s->size, s->count,
				session->padding_to_flush, session->r_ofs);

	/* prepare enough seq buffer */
	if ((session->padding_to_flush + MAX_RV_INFO_SIZE) > (s->size - s->count)) {
		if (*pos > 0) {
			HWLOGR_INFO("alloc seq_buf %lu pos %lld\n", s->size, *pos);
			new_seq_buf = kvmalloc(s->size <<= 1, GFP_KERNEL_ACCOUNT);
			if (new_seq_buf) {
				memcpy(new_seq_buf, s->buf, s->count);
				kvfree(s->buf);
				s->buf = new_seq_buf;
			} else {
				HWLOGR_ERR("alloc seq_buf failed s->size: %lu\n", s->size);
				kfree(session);
				return NULL;
			}
		} else {
			HWLOGR_DBG("alloc seq_buf %lu pos %lld\n", s->size, *pos);
			new_seq_buf = kvmalloc(DEFAULT_SEQ_BUF_SIZE, GFP_KERNEL_ACCOUNT);
			if (new_seq_buf) {
				kvfree(s->buf);
				s->buf = new_seq_buf;
				s->size = DEFAULT_SEQ_BUF_SIZE;
			}
		}
	}

	return session;
}

static void *seq_start_block(struct seq_file *s, loff_t *pos)
{
	return seq_start(s, pos, true);
}

static void *seq_start_nonblock(struct seq_file *s, loff_t *pos)
{
	return seq_start(s, pos, false);
}

static void seq_stop(struct seq_file *s, void *v)
{
	HWLOGR_DBG("+");
	HWLOGR_DBG("s->size %ld", s->size);
	HWLOGR_DBG("s->from %ld", s->from);
	HWLOGR_DBG("s->count %ld", s->count);
	HWLOGR_DBG("s->pad_until %ld", s->pad_until);
	HWLOGR_DBG("s->index %lld", s->index);
	HWLOGR_DBG("s->read_pos %lld", s->read_pos);

	kfree(v);
}

static unsigned int seq_poll(struct file *file, poll_table *wait)
{
	unsigned int ret = 0, raw_w_ofs = 0;
	unsigned long flags;
	HWLOGR_DBG("+");

	if (!(file->f_mode & FMODE_READ))
		return ret;

	poll_wait(file, &apusys_mblog_wait_queue, wait);

	raw_w_ofs = logger_v2_get_w_ofs();

	spin_lock_irqsave(&prev_mb_log_session_lock, flags);
	if (!prev_mb_log_session.init)
		ret = POLLIN | POLLRDNORM;
	else {
		if (prev_mb_log_session.r_ofs != raw_w_ofs)
			ret = POLLIN | POLLRDNORM;
	}
	spin_unlock_irqrestore(&prev_mb_log_session_lock, flags);

	HWLOGR_DBG("-");
	return ret;
}

static const struct seq_operations log_buf_seq_block_ops = {
	.start = seq_start_block,
	.next  = seq_next,
	.stop  = seq_stop,
	.show  = seq_show
};

static const struct seq_operations log_buf_seq_nonblock_ops = {
	.start = seq_start_nonblock,
	.next  = seq_next,
	.stop  = seq_stop,
	.show  = seq_show
};

static int proc_log_buf_block_sqopen(struct inode *inode, struct file *file)
{
	return seq_open(file, &log_buf_seq_block_ops);
}

static int proc_log_buf_nonblock_sqopen(struct inode *inode, struct file *file)
{
	return seq_open(file, &log_buf_seq_nonblock_ops);
}

static const struct proc_ops proc_raw_np_log_ops = {
	.proc_open		= proc_raw_np_log_sqopen,
	.proc_read		= seq_read,
	.proc_write		= proc_clear_write_np,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release
};

static const struct proc_ops proc_log_level_ops = {
	.proc_open		= proc_log_level_sqopen,
	.proc_write		= proc_log_level_write,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release
};

static const struct proc_ops proc_debug_attr_ops = {
	.proc_open		= proc_debug_attr_sqopen,
	.proc_write		= proc_debug_attr_write,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release
};

static const struct proc_ops proc_nonblock_ops = {
	.proc_open		= proc_log_buf_nonblock_sqopen,
	.proc_read		= seq_read,
	.proc_write		= proc_clear_write_np,
	.proc_lseek		= seq_lseek,
	.proc_release	= seq_release
};

static const struct proc_ops proc_block_ops = {
	.proc_open		= proc_log_buf_block_sqopen,
	.proc_poll		= seq_poll,
	.proc_read		= seq_read,
	.proc_write		= proc_clear_write_np,
	.proc_lseek		= seq_lseek,
	.proc_release	= seq_release
};

int logger_v2_create_procfs(struct platform_device *pdev)
{
	int ret = 0;

	proc_root = proc_mkdir(APUSYS_HWLOGR_DIR, NULL);
	ret = IS_ERR_OR_NULL(proc_root);
	if (ret) {
		HWLOGR_ERR("create dir fail (%d)\n", ret);
		goto out;
	}

	proc_log_lv = proc_create("log", 0440,  proc_root, &proc_log_level_ops);
	ret = IS_ERR_OR_NULL(proc_log_lv);
	if (ret) {
		HWLOGR_ERR("create log fail (%d)\n", ret);
		goto out;
	}

	proc_debug_attr = proc_create("attr", 0440,  proc_root, &proc_debug_attr_ops);
	ret = IS_ERR_OR_NULL(proc_debug_attr);
	if (ret) {
		HWLOGR_ERR("create attr fail (%d)\n", ret);
		goto out;
	}

	proc_seq_nonblock = proc_create("seq_log", 0440, proc_root, &proc_nonblock_ops);
	ret = IS_ERR_OR_NULL(proc_seq_nonblock);
	if (ret) {
		HWLOGR_ERR("create seqlog fail (%d)\n", ret);
		goto out;
	}

	proc_seq_block = proc_create("seq_logl", 0440, proc_root, &proc_block_ops);
	ret = IS_ERR_OR_NULL(proc_seq_block);
	if (ret) {
		HWLOGR_ERR("create seq_logl fail (%d)\n", ret);
		goto out;
	}

	proc_raw_np_log = proc_create("raw_log", 0440, proc_root, &proc_raw_np_log_ops);
	ret = IS_ERR_OR_NULL(proc_raw_np_log);
	if (ret) {
		HWLOGR_ERR("create raw_log fail (%d)\n", ret);
		goto out;
	}

	init_waitqueue_head(&apusys_mblog_wait_queue);
	apusys_mblog_wq = create_workqueue(APUSYS_HWLOG_WQ_NAME);
	INIT_DELAYED_WORK(&apusys_mblog_delayed_work, notify_mblog_work);

out:
	return ret;
}

int logger_v2_remove_procfs(struct platform_device *pdev)
{
	flush_workqueue(apusys_mblog_wq);
	destroy_workqueue(apusys_mblog_wq);

	remove_proc_entry("log", proc_root);
	remove_proc_entry("attr", proc_root);
	remove_proc_entry("seq_log", proc_root);
	remove_proc_entry("seq_logl", proc_root);
	remove_proc_entry("raw_log", proc_root);
	remove_proc_entry(APUSYS_HWLOGR_DIR, NULL);
	return 0;
}