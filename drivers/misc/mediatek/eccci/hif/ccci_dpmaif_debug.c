// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>

#include "ccci_dpmaif_debug.h"
#include "ccci_dpmaif_com.h"
#include "net_pool.h"
#include "ccci_port.h"
#include "ccci_hif.h"

#define TAG "dbg"





#define DEBUG_BUFFER_LEN   (163840)
#define DEBUG_MIN_READ_LEN (4000)


struct dpmaif_debug_buffer {
	atomic_t          dbg_user_cnt;
	wait_queue_head_t dbg_wq;
	spinlock_t        dbg_lock;
	atomic_t      status; //0:CLOSE, 1:OPEN
	char         *data;
	unsigned int  rd;
	unsigned int  wr;
	unsigned int  buf_len;
	int           pre_call_wq;
};


static struct dpmaif_debug_buffer g_debug_buf;
static atomic_t                   g_debug_buf_len;
unsigned int                      g_debug_flags;


#ifdef ENABLE_DPMAIF_ISR_LOG

#define ISR_LOG_DATA_LEN 10

static void ccci_dpmaif_print_irq_log(struct dpmaif_isr_count *isr_log)
{
	int i, j;
	char string[300];
	int len = 0, pos = 0;
	u64 tss1, tss2, tse1, tse2;

	if (isr_log == NULL)
		return;

	CCCI_BUF_LOG_TAG(0, CCCI_DUMP_DPMAIF, TAG,
		"dump dpmaif isr log: L2TISAR0(0~31) L2RISAR0(32~63)\n");

	for (i = 0; i < ISR_LOG_DATA_LEN; i++) {
		if (isr_log[i].ts_start == 0)
			continue;

		tss1 = isr_log[i].ts_start;
		tss2 = do_div(tss1, 1000000000);

		tse1 = isr_log[i].ts_end;
		tse2 = do_div(tse1, 1000000000);

		len = snprintf(string+pos, 300-pos, "%d|%lu.%06lu~%lu.%06lu->",
						i, (unsigned long)tss1, (unsigned long)(tss2/1000),
						(unsigned long)tse1, (unsigned long)(tse2/1000));
		if ((len <= 0) || (len >= 300-pos))
			break;

		pos += len;

		for (j = 0; j < 64; j++) {
			if (isr_log[i].irq_cnt[j] == 0)
				continue;

			len = snprintf(string+pos, 300-pos, " %u-%u", j, isr_log[i].irq_cnt[j]);
			if ((len <= 0) || (len >= 300-pos))
				break;

			pos += len;
		}

		pos = 0;

		CCCI_BUF_LOG_TAG(0, CCCI_DUMP_DPMAIF, TAG, "%s\n", string);
	}
}

void ccci_dpmaif_show_irq_log(void)
{
	unsigned int i;
	struct dpmaif_rx_queue *rxq;

	for (i = 0; i < g_dpmaif_ctrl->real_rxq_num; i++) {
		rxq = &g_dpmaif_ctrl->rxq[i];
		ccci_dpmaif_print_irq_log(rxq->isr_cnt_each_rxq);
	}
}

inline int ccci_dpmaif_record_isr_cnt(unsigned long long ts,
		struct dpmaif_rx_queue *rxq,
		unsigned int L2TISAR0, unsigned int L2RISAR0)
{
	unsigned int i;

	if (rxq->isr_cnt_each_rxq == NULL) {
		CCCI_ERROR_LOG(-1, TAG, "%s:%d isr_cnt_each_rxq is null\n", __func__, __LINE__);
		return 0;
	}

	if ((ts - rxq->isr_pre_time) >= 1000000000) {  // > 1s
		rxq->isr_log_idx++;
		if (rxq->isr_log_idx >= ISR_LOG_DATA_LEN)
			rxq->isr_log_idx = 0;

		memset(rxq->isr_cnt_each_rxq[rxq->isr_log_idx].irq_cnt, 0,
			sizeof(rxq->isr_cnt_each_rxq[rxq->isr_log_idx].irq_cnt));
		rxq->isr_cnt_each_rxq[rxq->isr_log_idx].ts_start = ts;
		rxq->isr_pre_time = ts;
	}

	rxq->isr_cnt_each_rxq[rxq->isr_log_idx].ts_end = ts;

	for (i = 0; i < 32; i++) {
		if (L2TISAR0 == 0)
			break;

		if (L2TISAR0 & (1<<i)) {
			L2TISAR0 &= (~(1<<i));
			rxq->isr_cnt_each_rxq[rxq->isr_log_idx].irq_cnt[i]++;
			if (rxq->isr_cnt_each_rxq[rxq->isr_log_idx].irq_cnt[i] > 50000)
				return -1;
		}
	}

	for (i = 0; i < 32; i++) {
		if (L2RISAR0 == 0)
			break;

		if (L2RISAR0 & (1<<i)) {
			L2RISAR0 &= (~(1<<i));
			rxq->isr_cnt_each_rxq[rxq->isr_log_idx].irq_cnt[i+32]++;
			if (rxq->isr_cnt_each_rxq[rxq->isr_log_idx].irq_cnt[i+32] > 50000)
				return -1;
		}
	}

	return 0;
}

#endif

void ccci_dpmaif_debug_add(void *data, int len)
{
	unsigned long flags = 0;
	unsigned int free_cnt = 0;

	spin_lock_irqsave(&g_debug_buf.dbg_lock, flags);

	if (g_debug_buf.data == NULL)
		goto _func_exit_;

	free_cnt = get_ringbuf_free_cnt(g_debug_buf.buf_len, g_debug_buf.rd, g_debug_buf.wr);
	if (len <= free_cnt) {
		if ((g_debug_buf.wr + len) > g_debug_buf.buf_len) {
			int s = g_debug_buf.buf_len - g_debug_buf.wr;

			memcpy(g_debug_buf.data + g_debug_buf.wr, data, s);
			memcpy(g_debug_buf.data, data + s, len - s);
		} else
			memcpy(g_debug_buf.data + g_debug_buf.wr, data, len);

		/* for cpu exec. */
		smp_wmb();

		g_debug_buf.wr = get_ringbuf_next_idx(g_debug_buf.buf_len, g_debug_buf.wr, len);

		len += (g_debug_buf.buf_len - free_cnt);
		if (len > g_debug_buf.pre_call_wq) {
			if ((len - g_debug_buf.pre_call_wq) > DEBUG_MIN_READ_LEN) {
				g_debug_buf.pre_call_wq += DEBUG_MIN_READ_LEN;
				wake_up_all(&g_debug_buf.dbg_wq);
			}
		} else
			g_debug_buf.pre_call_wq = len - 1;
	}

_func_exit_:
	spin_unlock_irqrestore(&g_debug_buf.dbg_lock, flags);
}

static ssize_t dpmaif_debug_read(struct file *file, char __user *buf,
		size_t size, loff_t *ppos)
{
	unsigned int read_len = 0;
	int ret = 0, len;
	unsigned long flags = 0;

	spin_lock_irqsave(&g_debug_buf.dbg_lock, flags);
	if (g_debug_buf.data == NULL)
		goto _func_exit_;

	read_len = get_ringbuf_used_cnt(g_debug_buf.buf_len, g_debug_buf.rd, g_debug_buf.wr);
	if (read_len == 0)
		goto _func_exit_;

	if (read_len > size)
		read_len = size;

	if ((g_debug_buf.rd + read_len) > g_debug_buf.buf_len) {
		len = g_debug_buf.buf_len - g_debug_buf.rd;

		ret = copy_to_user(buf, g_debug_buf.data + g_debug_buf.rd, len);
		if (ret)
			goto _func_exit_;

		ret = copy_to_user(buf + len, g_debug_buf.data, read_len - len);

	} else
		ret = copy_to_user(buf, g_debug_buf.data + g_debug_buf.rd, read_len);

	if (ret)
		goto _func_exit_;

	g_debug_buf.rd = get_ringbuf_next_idx(g_debug_buf.buf_len, g_debug_buf.rd, read_len);
	spin_unlock_irqrestore(&g_debug_buf.dbg_lock, flags);
	return read_len;

_func_exit_:
	spin_unlock_irqrestore(&g_debug_buf.dbg_lock, flags);
	if (ret)
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: copy_to_user() fail; read_len: %d(%d)\n",
			__func__, read_len, ret);
	return 0;
}

static void dpmaif_sysfs_parse(char *buf, int size)
{
	char *psub = NULL, *pname = NULL, *pvalue = NULL;
	unsigned int debug_buf_len = 0, wake_up_flag = 0;
	unsigned long flags = 0;

	if (!buf || size <= 0)
		return;

	pname = buf;
	while (1) {
		psub = strchr(pname, '|');

		if (psub) {
			*psub = '\0';
			psub += 1;
		}

		pvalue = strchr(pname, '=');
		if (pvalue) {
			*pvalue = '\0';
			pvalue += 1;
		}

		if (strstr(pname, "debug_flags")) {
			if (pvalue && *pvalue)
				if (kstrtouint(pvalue, 16, &g_debug_flags))
					return;
		} else if (strstr(pname, "debug_buf_len")) {
			if (pvalue && *pvalue)
				if (kstrtouint(pvalue, 10, &debug_buf_len))
					return;
		} else if (strstr(pname, "run_wq")) {
			if (pvalue && *pvalue)
				if (kstrtouint(pvalue, 10, &wake_up_flag))
					return;
			if (wake_up_flag)
				wake_up_all(&g_debug_buf.dbg_wq);
		}

		if (!psub)
			break;

		pname = psub;
	}

	spin_lock_irqsave(&g_debug_buf.dbg_lock, flags);
	if ((debug_buf_len > 0) && (atomic_read(&g_debug_buf_len) != debug_buf_len))
		atomic_set(&g_debug_buf_len, debug_buf_len);
	spin_unlock_irqrestore(&g_debug_buf.dbg_lock, flags);
}

#define MAX_WRITE_LEN 300
static ssize_t dpmaif_debug_write(struct file *fp, const char __user *buf,
	size_t size, loff_t *ppos)
{
	char str[MAX_WRITE_LEN] = {0};
	int ret;

	if (size >= MAX_WRITE_LEN)
		return 0;

	ret = copy_from_user(str, buf, size);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: copy_from_user() fail; size: %lu(%d)\n",
			__func__, size, ret);
		return 0;
	}

	str[MAX_WRITE_LEN-1] = '\0';
	dpmaif_sysfs_parse(str, size);

	return size;
}

static unsigned int dpmaif_debug_poll(struct file *fp, struct poll_table_struct *poll)
{
	unsigned long flags = 0;

	poll_wait(fp, &g_debug_buf.dbg_wq, poll);

	spin_lock_irqsave(&g_debug_buf.dbg_lock, flags);
	if (get_ringbuf_used_cnt(g_debug_buf.buf_len, g_debug_buf.rd, g_debug_buf.wr)) {
		spin_unlock_irqrestore(&g_debug_buf.dbg_lock, flags);
		return (POLLIN | POLLRDNORM);
	}
	spin_unlock_irqrestore(&g_debug_buf.dbg_lock, flags);
	return 0;
}

static int dpmaif_debug_open(struct inode *inode, struct file *file)
{
	unsigned long flags = 0;
	char *pdata = NULL, *pfreedata = NULL;
	unsigned int plen = 0;
	bool need_realloc = false;

	spin_lock_irqsave(&g_debug_buf.dbg_lock, flags);
	if (atomic_read(&g_debug_buf.dbg_user_cnt)) {
		atomic_set(&g_debug_buf.status, 1);
		spin_unlock_irqrestore(&g_debug_buf.dbg_lock, flags);
		return -EBUSY;
	}

	atomic_inc(&g_debug_buf.dbg_user_cnt);
	plen = atomic_read(&g_debug_buf_len);

	if (!g_debug_buf.data || plen != g_debug_buf.buf_len) {
		need_realloc = true;
		pfreedata = g_debug_buf.data;
		if (pfreedata) {
			g_debug_buf.data = NULL;
			g_debug_buf.rd  = 0;
			g_debug_buf.wr  = 0;
			g_debug_buf.pre_call_wq = 0;
		}
	}
	spin_unlock_irqrestore(&g_debug_buf.dbg_lock, flags);

	if (need_realloc) {
		if (pfreedata)
			vfree(pfreedata);
		pdata = vmalloc(plen);
		if (!pdata)
			return -ENOMEM;
		CCCI_NORMAL_LOG(-1, TAG, "[%s] vmalloc(%u):%p\n",
			__func__, plen, pdata);
	}

	spin_lock_irqsave(&g_debug_buf.dbg_lock, flags);
	if (need_realloc) {
		g_debug_buf.buf_len = plen;
		g_debug_buf.data = pdata;
	}
	g_debug_flags = 0xFFFFFFFF;

	atomic_set(&g_debug_buf.status, 1);
	spin_unlock_irqrestore(&g_debug_buf.dbg_lock, flags);
	CCCI_NORMAL_LOG(-1, TAG, "[%s] name: %s:%p\n", __func__, current->comm, g_debug_buf.data);
	return 0;
}

static int dpmaif_debug_close(struct inode *inode, struct file *file)
{
	unsigned long flags = 0;

	/* wait until vmalloc is finished */
	while (!atomic_read(&g_debug_buf.status))
		udelay(500);
	spin_lock_irqsave(&g_debug_buf.dbg_lock, flags);
	atomic_dec(&g_debug_buf.dbg_user_cnt);
	g_debug_flags = 0;
	atomic_set(&g_debug_buf.status, 0);
	spin_unlock_irqrestore(&g_debug_buf.dbg_lock, flags);
	return 0;
}

static void dpmaif_md_ee_cb(void)
{
	wake_up_all(&g_debug_buf.dbg_wq);
}

static const struct proc_ops g_dpmaif_debug_fops = {
	.proc_read    = dpmaif_debug_read,
	.proc_write   = dpmaif_debug_write,
	.proc_poll    = dpmaif_debug_poll,
	.proc_open    = dpmaif_debug_open,
	.proc_release = dpmaif_debug_close,
};

void ccci_dpmaif_isr_record_init(void)
{
#ifdef ENABLE_DPMAIF_ISR_LOG
	unsigned int i;
	struct dpmaif_rx_queue *rxq;
	struct dpmaif_isr_count *isr_log;

	isr_log = kzalloc(sizeof(struct dpmaif_isr_count) * ISR_LOG_DATA_LEN *
			g_dpmaif_ctrl->real_rxq_num, GFP_KERNEL);
	if (!isr_log) {
		CCCI_ERROR_LOG(-1, TAG, "[%s] error: alloc isr_log fail\n", __func__);
		return;
	}

	for (i = 0; i < g_dpmaif_ctrl->real_rxq_num; i++) {
		rxq = &g_dpmaif_ctrl->rxq[i];
		rxq->isr_cnt_each_rxq = isr_log + ISR_LOG_DATA_LEN * i;
		CCCI_NORMAL_LOG(-1, TAG, "%s:rxq%d addr=%p\n",
			__func__, i, rxq->isr_cnt_each_rxq);
	}

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	mrdump_mini_add_extra_file((unsigned long)isr_log, __pa_nodebug(isr_log),
		(sizeof(struct dpmaif_isr_count) * ISR_LOG_DATA_LEN *
			g_dpmaif_ctrl->real_rxq_num), "DPMAIF_ISR");
#endif

#else
	CCCI_NORMAL_LOG(-1, TAG, "DPMAIF_ISR_LOG not enable\n");
#endif

}

void ccci_dpmaif_debug_init(void)
{
	struct proc_dir_entry *dpmaif_debug_proc;

	g_debug_buf.buf_len = 0;
	g_debug_flags   = 0;
	atomic_set(&g_debug_buf_len, 1024 * 1024);
	atomic_set(&g_debug_buf.dbg_user_cnt, 0);
	atomic_set(&g_debug_buf.status, 0);
	g_debug_buf.data = NULL;
	g_debug_buf.rd   = 0;
	g_debug_buf.wr   = 0;
	g_debug_buf.pre_call_wq = 0;

	dpmaif_debug_proc = proc_create("dpmaif_debug", 0644, NULL, &g_dpmaif_debug_fops);
	if (dpmaif_debug_proc == NULL) {
		CCCI_ERROR_LOG(-1, TAG, "[%s] error: proc_create fail.\n", __func__);
		return;
	}

	spin_lock_init(&g_debug_buf.dbg_lock);
	init_waitqueue_head(&g_debug_buf.dbg_wq);

	ccci_set_dpmaif_debug_cb(&dpmaif_md_ee_cb);

	ccci_dpmaif_isr_record_init();
}
