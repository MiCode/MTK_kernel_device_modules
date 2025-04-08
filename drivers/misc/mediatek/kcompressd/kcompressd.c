// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 MediaTek Inc.
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bio.h>
#include <linux/bitops.h>
#include <linux/freezer.h>
#include <linux/kernel.h>
#include <linux/psi.h>
#include <linux/kfifo.h>
#include <linux/swap.h>
#include <linux/delay.h>
#include <linux/kcompressd.h>

static atomic_t enable_kcompressd;
static unsigned int nr_kcompressd;
static atomic_t current_fifo = ATOMIC_INIT(0);
static unsigned int kfifo_size_per_kcompressd;
static struct kcompress *kcompress;

struct kcompressd_para {
	wait_queue_head_t *kcompressd_wait;
	struct kfifo *write_fifo;
	atomic_t *running;
};

static struct kcompressd_para *kcompressd_para;

struct write_work {
	void *mem;
	struct bio *bio;
	compress_callback cb;
};

static int kcompressd(void *para)
{
	struct task_struct *tsk = current;
	struct kcompressd_para *p = (struct kcompressd_para *)para;
	//unsigned long pflags;

	tsk->flags |= PF_MEMALLOC | PF_KSWAPD;
	set_freezable();

	while (!kthread_should_stop()) {
		while (!kfifo_is_empty(p->write_fifo)) {
			struct write_work entry;

			// psi_memstall_enter(&pflags);
			if (sizeof(struct write_work) == kfifo_out(p->write_fifo,
						&entry, sizeof(struct write_work))) {
				entry.cb(entry.mem, entry.bio);
				bio_put(entry.bio);
			}
			// psi_memstall_leave(&pflags);
		}

		usleep_range(1000, 2000);
		if (kfifo_is_empty(p->write_fifo))
			break;
	}

	tsk->flags &= ~(PF_MEMALLOC | PF_KSWAPD);
	atomic_set(p->running, 0);
	return 0;
}

static int init_write_fifos(void)
{
	int i;

	for (i = 0; i < nr_kcompressd; i++) {
		if (kfifo_alloc(&kcompress[i].write_fifo,
					kfifo_size_per_kcompressd * sizeof(struct write_work), GFP_KERNEL)) {
			pr_info("Failed to alloc kfifo %d\n", i);
			return -ENOMEM;
		}
	}
	return 0;
}

static void cleanup_kfifos(int idx)
{
	struct write_work entry;

	while (sizeof(struct write_work) == kfifo_out(&kcompress[idx].write_fifo,
				&entry, sizeof(struct write_work))) {
		bio_put(entry.bio);
		entry.cb(entry.mem, entry.bio);
	}
	kfifo_free(&kcompress[idx].write_fifo);
}

static int kcompress_update(void)
{
	int i;
	int ret;

	kcompress = kvmalloc_array(nr_kcompressd, sizeof(struct kcompress), GFP_KERNEL);
	if (!kcompress)
		return -ENOMEM;

	kcompressd_para = kvmalloc_array(nr_kcompressd, sizeof(struct kcompressd_para), GFP_KERNEL);
	if (!kcompressd_para)
		return -ENOMEM;

	ret = init_write_fifos();
	if (ret) {
		pr_info("Init write fifos failed!\n");
		return ret;
	}

	for (i = 0; i < nr_kcompressd; i++) {
		kcompressd_para[i].kcompressd_wait = &kcompress[i].kcompressd_wait;
		kcompressd_para[i].write_fifo = &kcompress[i].write_fifo;
		kcompressd_para[i].running = &kcompress[i].running;
	}

	return 0;
}

static void stop_all_kcompressd_thread(void)
{
	int i;

	for (i = 0; i < nr_kcompressd; i++) {
		kthread_stop(kcompress[i].kcompressd);
		kcompress[i].kcompressd = NULL;
		cleanup_kfifos(i);
	}
}

static int do_nr_kcompressd_handler(const char *val,
		const struct kernel_param *kp)
{
	int ret;

	atomic_set(&enable_kcompressd, false);

	stop_all_kcompressd_thread();

	ret = param_set_int(val, kp);
	if (!ret) {
		pr_info("Invalid number of kcompressd.\n");
		return -EINVAL;
	}

	ret = init_write_fifos();
	if (ret) {
		pr_info("Init write fifos failed!\n");
		return ret;
	}

	atomic_set(&enable_kcompressd, true);

	return 0;
}

static const struct kernel_param_ops param_ops_change_nr_kcompressd = {
	.set = &do_nr_kcompressd_handler,
	.get = &param_get_uint,
	.free = NULL,
};

module_param_cb(nr_kcompressd, &param_ops_change_nr_kcompressd,
		&nr_kcompressd, 0644);
MODULE_PARM_DESC(nr_kcompressd, "Number of pre-created daemon for page compression");

static int do_kfifo_size_per_kcompressd_handler(const char *val,
		const struct kernel_param *kp)
{
	int ret;

	atomic_set(&enable_kcompressd, false);

	stop_all_kcompressd_thread();

	ret = param_set_int(val, kp);
	if (!ret) {
		pr_info("Invalid size of kfifo for kcompressd.\n");
		return -EINVAL;
	}

	ret = init_write_fifos();
	if (ret) {
		pr_info("Init write fifos failed!\n");
		return ret;
	}

	pr_info("Size of kfifo for kcompressd was changed: %d\n", kfifo_size_per_kcompressd);

	atomic_set(&enable_kcompressd, true);
	return 0;
}

static const struct kernel_param_ops param_ops_change_kfifo_size_per_kcompressd = {
	.set = &do_kfifo_size_per_kcompressd_handler,
	.get = &param_get_uint,
	.free = NULL,
};

module_param_cb(kfifo_size_per_kcompressd, &param_ops_change_kfifo_size_per_kcompressd,
		&kfifo_size_per_kcompressd, 0644);
MODULE_PARM_DESC(kfifo_size_per_kcompressd,
		"Size of kfifo for kcompressd");

int schedule_bio_write(void *mem, struct bio *bio, compress_callback cb)
{
	int i;
	bool submit_success = false;

	struct write_work entry = {
		.mem = mem,
		.bio = bio,
		.cb = cb
	};

	if (unlikely(!atomic_read(&enable_kcompressd)))
		return -EBUSY;

	if (!nr_kcompressd || !current_is_kswapd())
		return -EBUSY;

	bio_get(bio);

	for (i = 0; i < nr_kcompressd; i++) {
		submit_success =
			(kfifo_avail(&kcompress[i].write_fifo) >= sizeof(struct write_work)) &&
			(sizeof(struct write_work) == kfifo_in(&kcompress[i].write_fifo,
					 &entry, sizeof(struct write_work)));

		if (submit_success)
			break;
	}

	if (submit_success) {
		if (!atomic_read(&kcompress[i].running)) {
			atomic_set(&kcompress[i].running , 1);
			kcompress[i].kcompressd = kthread_run(kcompressd, &kcompressd_para[i], "kcompressd:%d", i);
			if (IS_ERR(kcompress[i].kcompressd)) {
				atomic_set(&kcompress[i].running , 0);
				pr_info("Failed to start kcompressd:%d in %s\n", i, __func__);
				cleanup_kfifos(i);
				return -ENOMEM;
			}
		}
	} else {
		bio_put(bio);
		return -EBUSY;
	}

	return 0;
}
EXPORT_SYMBOL(schedule_bio_write);

static int __init kcompressd_init(void)
{
	int i;
	int ret;

	nr_kcompressd = DEFAULT_NR_KCOMPRESSD;
	kfifo_size_per_kcompressd = INIT_KFIFO_SIZE;

	ret = kcompress_update();
	if (ret) {
		pr_info("Init kcompressd failed!\n");
		return ret;
	}

	atomic_set(&enable_kcompressd, true);
	return 0;
}

static void __exit kcompressd_exit(void)
{
	stop_all_kcompressd_thread();

	kvfree(kcompress);
	kvfree(kcompressd_para);
}

module_init(kcompressd_init);
module_exit(kcompressd_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Qun-Wei Lin <qun-wei.lin@mediatek.com>");
MODULE_DESCRIPTION("Separate the page compression from the memory reclaiming");

#pragma GCC diagnostic pop

