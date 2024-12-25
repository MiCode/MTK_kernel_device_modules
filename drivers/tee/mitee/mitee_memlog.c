/*
 * Copyright (C) 2015 Google, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "mitee_memlog.h"

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/panic_notifier.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/syscalls.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include <tee_drv.h>
#include "optee_ffa.h"
#include "optee_private.h"
#include "optee_smc.h"

#define SET_KEY_FILE "/data/vendor/mitee/key.log"
static bool read_b_buf = false;

static int log_read_line(struct mitee_memlog_state *s, int put, int get)
{
	struct log_rb *log = s->log;
	int i;
	char c = '\0';
	size_t max_to_read =
		min((size_t)(put - get), sizeof(s->line_buffer) - 1);
	size_t mask = log->sz - 1;

	for (i = 0; i < max_to_read && c != '\n';)
		s->line_buffer[i++] = c = log->data[get++ & mask];
	s->line_buffer[i] = '\0';

	return i;
}

static int log_read_b_line(struct mitee_memlog_state *s, int put, int get)
{
	struct log_rb *log = s->log;
	int i;
	char c = '\0';
	size_t max_to_read =
		min((size_t)(put - get), sizeof(s->line_buffer) - 1);
	size_t mask = log->b_sz - 1;

	for (i = 0; i < max_to_read && c != '\n';)
		s->line_buffer[i++] = c = log->data[log->sz + (get++ & mask)];
	s->line_buffer[i] = '\0';
	return i;
}
#if 0
static void mitee_dump_logs(struct mitee_memlog_state *s)
{
	struct log_rb *log = s->log;
	uint32_t get, put, alloc;
	int read_chars;

	BUG_ON(!is_power_of_2(log->sz));

	/*
	 * For this ring buffer, at any given point, alloc >= put >= get.
	 * The producer side of the buffer is not locked, so the put and alloc
	 * pointers must be read in a defined order (put before alloc) so
	 * that the above condition is maintained. A read barrier is needed
	 * to make sure the hardware and compiler keep the reads ordered.
	 */
	get = s->get;
	while ((put = log->put) != get) {
		/* Make sure that the read of put occurs before the read of log data */
		rmb();

		/* Read a line from the log */
		read_chars = log_read_line(s, put, get);

		/* Force the loads from log_read_line to complete. */
		rmb();
		alloc = log->alloc;

		/*
		 * Discard the line that was just read if the data could
		 * have been corrupted by the producer.
		 */
		if (alloc - get > log->sz) {
			pr_err("mitee: log overflow.");
			get = alloc - log->sz;
			continue;
		}
		pr_info("mitee: %s", s->line_buffer);
		get += read_chars;
	}
	s->get = get;
}
#endif

static int mitee_memlog_call_notify(struct notifier_block *nb,
				    unsigned long action, void *data)
{
	struct mitee_memlog_state *s;
	// unsigned long flags;

	if (action != MITEE_CALL_RETURNED)
		return NOTIFY_DONE;

	s = container_of(nb, struct mitee_memlog_state, call_notifier);
	// pr_err("ststest: start dump log\n");
	// spin_lock_irqsave(&s->lock, flags);
	// mitee_dump_logs(s);
	// spin_unlock_irqrestore(&s->lock, flags);
	atomic_inc(&s->mitee_log_event_count);
	wake_up_interruptible(&s->mitee_log_wq);
	// pr_err("ststest: end dump log\n");
	return NOTIFY_OK;
}

static int mitee_memlog_panic_notify(struct notifier_block *nb,
				     unsigned long action, void *data)
{
	struct mitee_memlog_state *s;

	/*
	 * Don't grab the spin lock to hold up the panic notifier, even
	 * though this is racy.
	 */
	s = container_of(nb, struct mitee_memlog_state, panic_notifier);
	pr_info("mitee-log panic notifier\n");
	// mitee_dump_logs(s);
	atomic_inc(&s->mitee_log_event_count);
	wake_up_interruptible(&s->mitee_log_wq);
	return NOTIFY_OK;
}

static int is_buf_empty(struct mitee_memlog_state *s)
{
	struct log_rb *log = s->log;
	uint32_t get, put;

	get = s->get;
	put = log->put;
	return (get == put);
}

static int is_b_buf_empty(struct mitee_memlog_state *s)
{
	struct log_rb *log = s->log;
	uint32_t get, put;

	get = s->b_get;
	put = log->b_put;
	return (get == put);
}

static int do_mitee_memlog_read(struct mitee_memlog_state *s, char __user *buf,
				size_t size)
{
	struct log_rb *log = s->log;
	uint32_t get, put, alloc;
	int read_chars = 0, copy_chars = 0, tbuf_size, outbuf_size;
	char *psrc = NULL;

	WARN_ON(!is_power_of_2(log->sz));

	if (!read_b_buf) {
		/*
		* For this ring buffer, at any given point, alloc >= put >= get.
		* The producer side of the buffer is not locked, so the put and alloc
		* pointers must be read in a defined order (put before alloc) so
		* that the above condition is maintained. A read barrier is needed
		* to make sure the hardware and compiler keep the reads ordered.
		*/
		get = s->b_get;
		put = log->b_put;
		/* make sure the hardware and compiler reads the correct put & alloc*/
		rmb();
		alloc = log->b_alloc;

		if (alloc - get > log->b_sz) {
			pr_notice("mitee: log overflow, lose some msg.");
			get = alloc - log->b_sz;
		}

		if (get > put)
			return -EFAULT;

		if (is_b_buf_empty(s)) {
			read_b_buf = true;
			return 0;
		}

		outbuf_size = min((int)(put - get), (int)size);

		tbuf_size = (outbuf_size / MITEE_LINE_BUFFER_SIZE + 1) *
			    MITEE_LINE_BUFFER_SIZE;

		/* tbuf_size >= outbuf_size >= size */
		psrc = kzalloc(tbuf_size, GFP_KERNEL);

		if (!psrc)
			return -ENOMEM;

		while (get != put) {
			read_chars = log_read_b_line(s, put, get);
			/* Force the loads from log_read_line to complete. */
			rmb();
			if (copy_chars + read_chars > outbuf_size)
				break;
			memcpy(psrc + copy_chars, s->line_buffer, read_chars);
			get += read_chars;
			copy_chars += read_chars;
		}

		if (copy_to_user(buf, psrc, copy_chars)) {
			kfree(psrc);
			return -EFAULT;
		}
		kfree(psrc);

		s->b_get = get;

		return copy_chars;
	} else {
		/*
		* For this ring buffer, at any given point, alloc >= put >= get.
		* The producer side of the buffer is not locked, so the put and alloc
		* pointers must be read in a defined order (put before alloc) so
		* that the above condition is maintained. A read barrier is needed
		* to make sure the hardware and compiler keep the reads ordered.
		*/
		get = s->get;
		put = log->put;
		/* make sure the hardware and compiler reads the correct put & alloc*/
		rmb();
		alloc = log->alloc;

		if (alloc - get > log->sz) {
			pr_notice("mitee: log overflow, lose some msg.");
			get = alloc - log->sz;
		}

		if (get > put)
			return -EFAULT;

		if (is_buf_empty(s))
			return 0;

		outbuf_size = min((int)(put - get), (int)size);

		tbuf_size = (outbuf_size / MITEE_LINE_BUFFER_SIZE + 1) *
			    MITEE_LINE_BUFFER_SIZE;

		/* tbuf_size >= outbuf_size >= size */
		psrc = kzalloc(tbuf_size, GFP_KERNEL);

		if (!psrc)
			return -ENOMEM;

		while (get != put) {
			read_chars = log_read_line(s, put, get);
			/* Force the loads from log_read_line to complete. */
			rmb();
			if (copy_chars + read_chars > outbuf_size)
				break;
			memcpy(psrc + copy_chars, s->line_buffer, read_chars);
			get += read_chars;
			copy_chars += read_chars;
		}

		if (copy_to_user(buf, psrc, copy_chars)) {
			kfree(psrc);
			return -EFAULT;
		}
		kfree(psrc);

		s->get = get;

		return copy_chars;
	}
}

static ssize_t mitee_memlog_read(struct file *file, char __user *buf,
				 size_t size, loff_t *ppos)
{
	struct mitee_memlog_state *s = pde_data(file_inode(file));
	int ret = 0;

	if (atomic_xchg(&s->readable, 0)) {
		ret = do_mitee_memlog_read(s, buf, size);
		s->poll_event = atomic_read(&s->mitee_log_event_count);
		atomic_set(&s->readable, 1);
	}
	return ret;
}

static int mitee_memlog_open(struct inode *inode, struct file *file)
{
	struct mitee_memlog_state *s = pde_data(inode);
	int ret;

	ret = nonseekable_open(inode, file);
	if (unlikely(ret))
		return ret;
	s->poll_event = atomic_read(&s->mitee_log_event_count);
	return 0;
}

static int mitee_memlog_release(struct inode *inode, struct file *file)
{
	return 0;
}

static unsigned int mitee_memlog_poll(struct file *file,
				      struct poll_table_struct *wait)
{
	struct mitee_memlog_state *s = pde_data(file_inode(file));
	int mask = 0;

	if (!is_buf_empty(s))
		return POLLIN | POLLRDNORM;

	poll_wait(file, &s->mitee_log_wq, wait);

	if (s->poll_event != atomic_read(&s->mitee_log_event_count))
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

static const struct proc_ops mitee_memlog_fops = {
	.proc_open = mitee_memlog_open,
	.proc_read = mitee_memlog_read,
	.proc_release = mitee_memlog_release,
	.proc_poll = mitee_memlog_poll,
};

static ssize_t mitee_memlog_key_read(struct file *file, char __user *buf,
				     size_t size, loff_t *ppos)
{
	struct mitee_memlog_state *s = pde_data(file_inode(file));
	int ret = 0;
	char *psrc = NULL;
	psrc = kzalloc(KEY_LENGTH, GFP_KERNEL);

	if (!psrc)
		return -ENOMEM;

	memcpy(psrc, (const void *)s->log->aeskey, KEY_LENGTH);
	if (copy_to_user(buf, psrc, KEY_LENGTH)) {
		kfree(psrc);
		pr_err("error miteelog mitee_memlog_key_read\n");
		return -EFAULT;
	}
	kfree(psrc);
	ret = KEY_LENGTH;
	return ret;
}

static int mitee_memlog_key_open(struct inode *inode, struct file *file)
{
	int ret;
	ret = nonseekable_open(inode, file);
	if (unlikely(ret))
		return ret;
	return 0;
}

static int mitee_memlog_key_release(struct inode *inode, struct file *file)
{
	return 0;
}

static unsigned int mitee_memlog_key_poll(struct file *file,
					  struct poll_table_struct *wait)
{
	return 0;
}

static const struct proc_ops mitee_memlog_key_fops = {
	.proc_open = mitee_memlog_key_open,
	.proc_read = mitee_memlog_key_read,
	.proc_release = mitee_memlog_key_release,
	.proc_poll = mitee_memlog_key_poll,
};

static int mitee_call_notifier_register(struct notifier_block *n)
{
	struct optee *optee = get_optee_drv_state();

	if (!optee) {
		pr_err("mitee_driver_state is NULL\n");
		return -EFAULT;
	}

	return atomic_notifier_chain_register(&optee->notifier, n);
}

static int mitee_call_notifier_unregister(struct notifier_block *n)
{
	struct optee *optee = get_optee_drv_state();

	if (!optee) {
		pr_err("mitee_driver_state is NULL\n");
		return -EFAULT;
	}

	return atomic_notifier_chain_unregister(&optee->notifier, n);
}

int mitee_memlog_probe(struct ffa_device *ffa_dev, const struct ffa_ops *ops,
		       struct platform_device *pdev)
{
	struct mitee_memlog_state *s;
	int result = 0;
	phys_addr_t pa;
	phys_addr_t begin;
	phys_addr_t end;
	size_t size;
	void *va;
	struct ffa_send_direct_data data = { OPTEE_FFA_GET_MITEE_LOG_BUFFER };
	int rc = 0;

	dev_dbg(&pdev->dev, "%s\n", __func__);

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s) {
		result = -ENOMEM;
		goto error_alloc_state;
	}

	spin_lock_init(&s->lock);
	s->dev = &pdev->dev;
	s->mitee_dev = s->dev->parent;
	s->get = 0;
	s->b_get = 0;
	read_b_buf = false;

	rc = ops->msg_ops->sync_send_receive(ffa_dev, &data);
	if (rc) {
		pr_err("miteelog service not available %d\n", rc);
		result = -EIO;
		goto error_alloc_log;
	}
	//pr_err("miteelog start=%x, size=%zu, setting=%x\n", (uint64_t)res.result.start, res.result.size,res.result.settings);
	begin = roundup(data.data0, PAGE_SIZE);
	end = rounddown(data.data0 + data.data1, PAGE_SIZE);
	pa = begin;
	size = end - begin;

	pr_err("miteelog paddr=%#llx, size=%zu\n", (uint64_t)pa, size);
	va = memremap(pa, size, MEMREMAP_WB);
	if (!va) {
		pr_err("miteelog ioremap failed\n");
		result = -EFAULT;
		goto error_alloc_log;
	}
	s->log = va;

	s->call_notifier.notifier_call = mitee_memlog_call_notify;
	result = mitee_call_notifier_register(&s->call_notifier);
	if (result < 0) {
		dev_err(&pdev->dev, "failed to register mitee call notifier\n");
		goto error_call_notifier;
	}

	s->panic_notifier.notifier_call = mitee_memlog_panic_notify;
	result = atomic_notifier_chain_register(&panic_notifier_list,
						&s->panic_notifier);
	if (result < 0) {
		dev_err(&pdev->dev,
			"failed to register mitee panic notifier\n");
		goto error_panic_notifier;
	}
	init_waitqueue_head(&s->mitee_log_wq);
	atomic_set(&s->mitee_log_event_count, 0);
	atomic_set(&s->readable, 1);
	platform_set_drvdata(pdev, s);

	/* create /proc/mitee_log */
	s->proc = proc_create_data("mitee_log", 0444, NULL, &mitee_memlog_fops,
				   s);
	if (!s->proc) {
		pr_info("mitee_log proc_create failed!\n");
		return -ENOMEM;
	}

	/* create /proc/mitee_log_key */
	s->proc_key = proc_create_data("mitee_log_key", 0444, NULL,
				       &mitee_memlog_key_fops, s);
	if (!s->proc_key) {
		pr_info("mitee_log proc_key_create failed!\n");
		return -ENOMEM;
	}

	return 0;

error_panic_notifier:
	mitee_call_notifier_unregister(&s->call_notifier);
error_call_notifier:
//TODO notify tee we failed register notifier
// trusty_std_call32(s->trusty_dev, SMC_SC_SHARED_LOG_RM,
// (u32)pa, (u32)((u64)pa >> 32), 0);
// error_std_call:
// __free_pages(s->log_pages, get_order(TRUSTY_LOG_SIZE));
error_alloc_log:
	kfree(s);
	s = NULL;
error_alloc_state:
	return result;
}

int mitee_memlog_remove(struct platform_device *pdev)
{
	// int result;
	struct mitee_memlog_state *s = platform_get_drvdata(pdev);
	// phys_addr_t pa = page_to_phys(s->log_pages);
	if (!s)
		return 0;

	dev_dbg(&pdev->dev, "%s\n", __func__);
	proc_remove(s->proc);
	proc_remove(s->proc_key);
	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &s->panic_notifier);
	mitee_call_notifier_unregister(&s->call_notifier);
	//TODO notify tee we failed register notifier
	// result = trusty_std_call32(s->trusty_dev, SMC_SC_SHARED_LOG_RM,
	// (u32)pa, (u32)((u64)pa >> 32), 0);
	// if (result) {
	// 	pr_err("trusty std call (SMC_SC_SHARED_LOG_RM) failed: %d\n", result);
	// }
	// __free_pages(s->log_pages, get_order(TRUSTY_LOG_SIZE));
	kfree(s);

	return 0;
}
