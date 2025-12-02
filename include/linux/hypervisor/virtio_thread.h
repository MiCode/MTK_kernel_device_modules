/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#include <linux/virtio.h>
#include <linux/spinlock.h>
#include <linux/virtio_config.h>
#include <linux/virtio_anchor.h>
#include <linux/module.h>
#include <linux/idr.h>
#include <linux/of.h>
#include <linux/kthread.h>
#include <linux/sched.h>

int virtio_init_kthread(void *data)
{
	struct virtio_driver *driver = (struct virtio_driver *)data;
	/* Catch this early. */
	return register_virtio_driver(driver);
}

int register_virtio_driver_thread(struct virtio_driver *driver)
{
	struct task_struct *virtio_init_task = NULL;

	virtio_init_task = kthread_run(virtio_init_kthread, (void *)driver,
		"virtio_init_kthread");

	return 0;
}

#define module_virtio_driver_thread(__virtio_driver) \
	module_driver(__virtio_driver, register_virtio_driver_thread, \
			unregister_virtio_driver)
