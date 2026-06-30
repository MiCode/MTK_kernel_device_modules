// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

//#include "mtk-scp-ultra.h"
#include <linux/sched.h>
#include <linux/types.h>
#include <uapi/linux/sched/types.h>
#include <linux/mm_types.h>
#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/sysfs.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by kmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/sysfs.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/sched/types.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/ioport.h>
#include <linux/kthread.h>
#include <linux/io.h>
#include <linux/wait.h>
#include <linux/time.h>

#include "audio_ultra_msg_id.h"
#include "mtk-scp-ultra-common.h"
#include "mtk-scp-ultra.h"
#include "mtk-scp-ultra_dump.h"

#define DUMP_ULTRA_PCM_DATA_PATH "/data/vendor/audiohal/audio_dump"
#define FRAME_BUF_SIZE (8192)
#define ULTRA_IPIMSG_TIMEOUT 200

enum ultra_dump_type {
	DUMP_PCM_IN = 0,
	DUMP_PCM_OUT = 1,
};

struct dump_package_t {
	uint8_t dump_data_type;
	uint32_t rw_idx;
	uint32_t data_size;
};

struct dump_queue_t {
	struct dump_package_t dump_package[256];
	uint8_t idx_r;
	uint8_t idx_w;
};

struct scp_ultra_reserved_mem_t {
	char *start_phy;
	char *start_virt;
	uint32_t size;
};

struct ultra_dump_info {
	const char *name;
	struct proc_dir_entry *file;
};

struct pcm_dump_t {
	char decode_pcm[FRAME_BUF_SIZE];
};

static struct ultra_dump_info dump_outinfo;
static struct ultra_dump_info dump_ininfo;
static struct dump_queue_t *indump_queue;
static struct dump_queue_t *outdump_queue;
static wait_queue_head_t wq_indump_pcm;
static wait_queue_head_t wq_outdump_pcm;
static bool dump_mem_not_valid;
static bool indump_dev_open, outdump_dev_open;

static struct scp_ultra_reserved_mem_t ultra_dump_mem;
static struct wakeup_source *wakelock_ultra_dump_lock;

void ultra_dump_message(void *msg_data)
{
	uint32_t *temp_payload = (uint32_t *)(msg_data);
	enum ultra_dump_type dump_type;
	uint8_t dump_idx = 0;

	if (temp_payload == NULL) {
		pr_info("%s err\n", __func__);
		return;
	}

	dump_type = (enum ultra_dump_type)(*temp_payload);
	if(dump_type == DUMP_PCM_OUT && outdump_queue != NULL) {
		dump_idx = outdump_queue->idx_w;
		outdump_queue->dump_package[dump_idx].dump_data_type =
			DUMP_PCM_OUT;
		outdump_queue->dump_package[dump_idx].data_size =
			*(temp_payload + 1);
		outdump_queue->dump_package[dump_idx].rw_idx =
			*(temp_payload + 2);
		outdump_queue->idx_w++;
		wake_up_interruptible(&wq_outdump_pcm);
	} else if (dump_type == DUMP_PCM_IN && indump_queue != NULL) {
		dump_idx = indump_queue->idx_w;
		indump_queue->dump_package[dump_idx].dump_data_type =
			DUMP_PCM_IN;
		indump_queue->dump_package[dump_idx].data_size =
			*(temp_payload + 1);
		indump_queue->dump_package[dump_idx].rw_idx =
			*(temp_payload + 2);
		indump_queue->idx_w++;
		wake_up_interruptible(&wq_indump_pcm);
	} else {
		pr_info("%s(), data fill fail, dump_type=%d!\n",
			__func__, dump_type);
	}
}

static int ultra_dump_file_open(struct inode *inode, struct file *file)
{
	enum ultra_dump_type type = (enum ultra_dump_type)pde_data(file_inode(file));

	file->private_data = (void *)type;
	if ((type == DUMP_PCM_IN && indump_dev_open) ||
	    (type == DUMP_PCM_OUT && outdump_dev_open)) {
		pr_info("%s dev file has been open\n", __func__);
		return -EBUSY;
	}

	if (type == DUMP_PCM_IN)
		indump_dev_open = true;
	else
		outdump_dev_open = true;

	return 0;
}

static int ultra_dump_file_release(struct inode *inode, struct file *file)
{
	enum ultra_dump_type type = (enum ultra_dump_type)pde_data(file_inode(file));

	if (type == DUMP_PCM_IN)
		indump_dev_open = false;
	else
		outdump_dev_open = false;

	return 0;
}

static unsigned int ultra_dump_file_poll(struct file *file, poll_table *wait)
{
	enum ultra_dump_type type;
	static wait_queue_head_t *wait_queue;
	struct dump_queue_t *queue = NULL;

	type = (enum ultra_dump_type)pde_data(file_inode(file));
	if (type == DUMP_PCM_IN) {
		wait_queue = &wq_indump_pcm;
		queue = indump_queue;
	} else {
		wait_queue = &wq_outdump_pcm;
		queue = outdump_queue;
	}

	poll_wait(file, wait_queue, wait);

	if (queue && queue->idx_r != queue->idx_w)
		return POLLIN | POLLRDNORM;

	return 0;
}

static ssize_t ultra_dump_file_read(struct file *file, char __user *data,
				size_t count, loff_t *ppos)
{
	enum ultra_dump_type type;
	struct dump_package_t *dump_package = NULL;
	static wait_queue_head_t *wait_queue;
	struct dump_queue_t *queue = NULL;
	uint8_t current_idx = 0;
	struct pcm_dump_t *pcm_dump = NULL;
	int ret = 0, size = 0;

	type = (enum ultra_dump_type)pde_data(file_inode(file));
	if (type == DUMP_PCM_IN) {
		wait_queue = &wq_indump_pcm;
		queue = indump_queue;
	} else {
		wait_queue = &wq_outdump_pcm;
		queue = outdump_queue;
	}

	if (queue == NULL || dump_mem_not_valid) {
		pr_info("queue is NULL or dump_mem_not_valid\n");
		return -EFAULT;
	}

	if (queue->idx_r != queue->idx_w) {
		current_idx = queue->idx_r;
		queue->idx_r++;
	} else {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

		ret = wait_event_interruptible_timeout(*wait_queue,
						       (queue->idx_r != queue->idx_w),
						       msecs_to_jiffies(ULTRA_IPIMSG_TIMEOUT));
		if (ret == -ERESTARTSYS) {
			pr_info("%s error -ERESTARTSYS, type:%d\n", __func__, type);
			return -EINTR;
		}
		current_idx = queue->idx_r;
		queue->idx_r++;
	}

	dump_package = &queue->dump_package[current_idx];
	if (dump_package == NULL) {
		pr_info("%s(), dump_package == null", __func__);
		return -EFAULT;
	}

	switch (dump_package->dump_data_type) {
	case DUMP_PCM_IN:
	case DUMP_PCM_OUT: {
		size = dump_package->data_size;
		pcm_dump = (struct pcm_dump_t *)
			(ultra_dump_mem.start_virt + dump_package->rw_idx);
		if ((char __user *)pcm_dump->decode_pcm == NULL) {
			pr_info("%s(), null, break\n", __func__);
			return -EFAULT;
		}
		if (copy_to_user(data, pcm_dump->decode_pcm, size)) {
			pr_info("%s(), copy to user fail, size=%d.\n",
			__func__, size);
			return -EFAULT;
		}
		break;
	}
	default:
		pr_debug("%s(), err type, cidx=%d,idx_r=%d,idx_w=%d,type=%d\n",
			__func__,
			current_idx,
			queue->idx_r,
			queue->idx_w,
			dump_package->dump_data_type);
		break;
	}

	return size;
}

static const struct proc_ops ultra_dump_fops = {
	.proc_lseek     = generic_file_llseek,
	.proc_read      = ultra_dump_file_read,
	.proc_poll      = ultra_dump_file_poll,
	.proc_open      = ultra_dump_file_open,
	.proc_release   = ultra_dump_file_release,
};

void ultra_dump_init(void)
{
	struct mtk_base_scp_ultra *scp_ultra = get_scp_ultra_base();
	struct audio_ultra_dram *dump_resv_mem;

	dump_resv_mem = &scp_ultra->ultra_dump.dump_resv_mem;
	ultra_dump_mem.start_virt = dump_resv_mem->vir_addr;
	ultra_dump_mem.size = dump_resv_mem->size;
	if (ultra_dump_mem.start_virt == NULL) {
		dump_mem_not_valid = true;
		pr_info("%s(), dump mem error, start_virt:%p\n",
			__func__,
			ultra_dump_mem.start_virt);
	}
	wakelock_ultra_dump_lock = aud_wake_lock_init(NULL, "ultradump lock");

	init_waitqueue_head(&wq_indump_pcm);
	init_waitqueue_head(&wq_outdump_pcm);

	ultra_dump_register_file();

	if (indump_queue == NULL) {
		indump_queue = kmalloc(sizeof(struct dump_queue_t), GFP_KERNEL);
		if (indump_queue != NULL)
			memset_io(indump_queue, 0, sizeof(struct dump_queue_t));
	}
	if (outdump_queue == NULL) {
		outdump_queue = kmalloc(sizeof(struct dump_queue_t), GFP_KERNEL);
		if (outdump_queue != NULL)
			memset_io(outdump_queue, 0, sizeof(struct dump_queue_t));
	}

	indump_dev_open = false;
	outdump_dev_open = false;
}

void ultra_dump_deinit(void)
{
	aud_wake_lock_destroy(wakelock_ultra_dump_lock);
	kfree(indump_queue);
	kfree(outdump_queue);
}

int ultra_start_dump(void)
{
	aud_wake_lock(wakelock_ultra_dump_lock);
	return 0;
}

void ultra_stop_dump(void)
{
	aud_wake_unlock(wakelock_ultra_dump_lock);
}

void ultra_dump_register_file(void)
{
	static struct proc_dir_entry *dir_dump;
	struct ultra_dump_info *outinfo = NULL;
	struct ultra_dump_info *ininfo = NULL;

	memset(&dump_outinfo, 0, sizeof(dump_outinfo));
	memset(&dump_ininfo, 0, sizeof(dump_ininfo));
	outinfo = &dump_outinfo;
	ininfo = &dump_ininfo;

	if (!dir_dump)
		dir_dump = proc_mkdir("ultra_debug", NULL);

	outinfo->name = "outpcm_dump";
	outinfo->file = proc_create_data(outinfo->name, 0444, dir_dump,
					 &ultra_dump_fops,
					 (void *)DUMP_PCM_OUT);

	ininfo->name = "inpcm_dump";
	ininfo->file = proc_create_data(ininfo->name, 0444, dir_dump,
					&ultra_dump_fops,
					(void *)DUMP_PCM_IN);
}
