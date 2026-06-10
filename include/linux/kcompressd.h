/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 MediaTek Inc.
 */

#ifndef _KCOMPRESSD_H_
#define _KCOMPRESSD_H_

#include <linux/rwsem.h>
#include <linux/kfifo.h>
#include <linux/atomic.h>

#define INIT_KFIFO_SIZE		4096
#define DEFAULT_NR_KCOMPRESSD	3

typedef void (*compress_callback)(void *mem, struct bio *bio);

struct kcompress {
	struct task_struct *kcompressd;
	wait_queue_head_t kcompressd_wait;
	struct kfifo write_fifo;
	atomic_t running;
};

int schedule_bio_write(void *, struct bio *, compress_callback);
#endif

