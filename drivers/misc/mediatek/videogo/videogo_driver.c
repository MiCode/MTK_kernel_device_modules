// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>

#include "videogo_driver.h"
#include "videogo_public.h"


static DECLARE_KFIFO(service_fifo, struct vgo_powerhal_info, 16);

static DECLARE_WAIT_QUEUE_HEAD(passive_wq);
static int passive_wq_ready;

static DECLARE_WAIT_QUEUE_HEAD(controller_wq);
static int controller_wq_ready;

static DECLARE_WAIT_QUEUE_HEAD(service_wq);

static struct task_struct *active_thread;

static LIST_HEAD(passive_workqueue);
//static struct list_head inst_list[MAX_CODEC_TYPE];
static struct list_head inst_list[MAX_CODEC_TYPE] = {
	LIST_HEAD_INIT(inst_list[0]),
	LIST_HEAD_INIT(inst_list[1])
};
//static LIST_HEAD(transcoding_list);
static DEFINE_MUTEX(passive_workqueue_mutex);
static DEFINE_MUTEX(service_mutex);
struct mutex inst_list_mutex[MAX_CODEC_TYPE] = {
		__MUTEX_INITIALIZER(inst_list_mutex[VDEC]),
		__MUTEX_INITIALIZER(inst_list_mutex[VENC]) };
//static int inst_list_length[MAX_CODEC_TYPE] = {0};

static int major_num;
static struct class *videogo_class;
static struct device *videogo_device;
static struct cdev videogo_cdev;

// RUNNABLE_BOOST/MARGIN_CONTROL
#define TARGET_FPS 30
static int set_runnable_boost_disable;
static int set_margin_control;
static int target_fps_count[2] = {0};
static int alive_count[2] = {0};

//static struct task_struct *kvideogo_active;
//static bool videogo_enable;


static void videogo_vcodec_send_fn(int type, void *data)
{
	struct data_entry *entry;

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return;

	entry->type = type;

	switch(type) {
	case VGO_RECV_INSTANCE_INC:
	case VGO_RECV_INSTANCE_DEC:
		entry->data = kmalloc(sizeof(struct inst_init_data), GFP_KERNEL);
		if (!entry->data) {
			kfree(entry);
			return;
		}
		memcpy(entry->data, data, sizeof(struct inst_init_data));
		break;
	case VGO_RECV_RUNNING_UPDATE:
		entry->data = kmalloc(sizeof(struct inst_data), GFP_KERNEL);
		if (!entry->data) {
			kfree(entry);
			return;
		}
		memcpy(entry->data, data, sizeof(struct inst_data));
		break;
	default:
		pr_info("type: %d not support\n", type);
		return;
	}

	mutex_lock(&passive_workqueue_mutex);
	list_add_tail(&entry->list, &passive_workqueue);
	passive_wq_ready = 1;
	mutex_unlock(&passive_workqueue_mutex);

	wake_up_interruptible(&passive_wq);
}

static int videogo_process_data(int type, void *data)
{
	struct inst_node *info, *tmp;
	//struct transcoding_pair *pair, *pair_tmp;
	int ret = VGO_IDLE;

	if (type == VGO_RECV_INSTANCE_INC) {
		struct inst_init_data *init_data = (struct inst_init_data *)data;

		info = kmalloc(sizeof(*info), GFP_KERNEL);
		if (!info)
			return -ENOMEM;

		info->inst_type = init_data->inst_type;
		info->ctx_id = init_data->ctx_id;
		info->caller_pid = init_data->caller_pid;
		info->fourcc = init_data->fourcc;
		info->oprate = init_data->oprate;
		info->oprate_vgo = 0;

		if ((init_data->width > 0 && init_data->width < INT_MAX) ||
			(init_data->height > 0 && init_data->height < INT_MAX)) {
			info->width = init_data->width;
			info->height = init_data->height;
		} else
			return -ENOMEM;

		memset(info->hw_proc_time, 0, sizeof(info->hw_proc_time));
		info->post_proc_time = 0;
		INIT_LIST_HEAD(&info->list);

		mutex_lock(&inst_list_mutex[info->inst_type]);
		list_add(&info->list, &inst_list[info->inst_type]);

		alive_count[info->inst_type]++;
		if (info->oprate > TARGET_FPS)
			target_fps_count[info->inst_type]++;
		mutex_unlock(&inst_list_mutex[info->inst_type]);

		pr_info("[vgo] INC inst_type: %d, ctx_id: %d, caller_pid: %d, fourcc: %d, oprate: %d, width: %d, height: %d\n",
			info->inst_type, info->ctx_id, info->caller_pid,
			info->fourcc, info->oprate, info->width, info->height);
	} else if (type == VGO_RECV_INSTANCE_DEC) {
		struct inst_init_data *inst_data = (struct inst_init_data *)data;

		mutex_lock(&inst_list_mutex[inst_data->inst_type]);
		alive_count[inst_data->inst_type]--;
		if (inst_data->oprate > TARGET_FPS)
			target_fps_count[inst_data->inst_type]--;

		list_for_each_entry_safe(info, tmp, &inst_list[inst_data->inst_type], list) {
			if (info->ctx_id == inst_data->ctx_id &&
				info->inst_type == inst_data->inst_type) {

				pr_info("[vgo] DEC inst_type: %d, ctx_id: %d\n", info->inst_type, info->ctx_id);
				list_del(&info->list);
				kfree(info);
				break;
			}
		}
		mutex_unlock(&inst_list_mutex[inst_data->inst_type]);

	} else if (type == VGO_RECV_RUNNING_UPDATE) {
		struct inst_data *run_data = (struct inst_data *)data;

		mutex_lock(&inst_list_mutex[run_data->inst_type]);
		list_for_each_entry_safe(info, tmp, &inst_list[run_data->inst_type], list) {
			if (info->ctx_id == run_data->ctx_id &&
				info->inst_type == run_data->inst_type) {
				info->oprate_avdvfs = run_data->oprate;
				memcpy(info->hw_proc_time, run_data->hw_proc_time,
					   sizeof(info->hw_proc_time));
				info->updated = 1;

				break;
			}
		}
		mutex_unlock(&inst_list_mutex[run_data->inst_type]);
	}

	return ret;
}

static int videogo_passive_fn(void *arg)
{
	struct data_entry *entry;

	while(!kthread_should_stop()) {
		wait_event_interruptible(passive_wq, passive_wq_ready ||
		kthread_should_stop());

		if (kthread_should_stop())
			break;

		mutex_lock(&passive_workqueue_mutex);
		while(!list_empty(&passive_workqueue)) {

			entry = list_first_entry(&passive_workqueue, struct data_entry, list);
			videogo_process_data(entry->type, entry->data);

			list_del(&entry->list);
			kfree(entry);
		}
		passive_wq_ready = 0;
		mutex_unlock(&passive_workqueue_mutex);

		// if need to notify controller thread
		controller_wq_ready = 1;
		wake_up_interruptible(&controller_wq);
	}

	return 0;
}

static int videogo_controller_fn(void *arg)
{
//	struct transcoding_pair *pair, *pair_tmp;
	struct vgo_powerhal_info service_info;

	while(!kthread_should_stop()) {
		wait_event_interruptible(controller_wq, controller_wq_ready);

		int total_vdec_30fps = target_fps_count[VDEC];
		int total_vdec = alive_count[VDEC];
		int total_venc = alive_count[VENC];

		if (total_vdec_30fps > 0 && total_vdec_30fps <= 2 &&
			!total_venc && total_vdec_30fps == total_vdec) {
			if (!set_runnable_boost_disable) {
				pr_info("[vgo] ack disable Runnable_boost\n");
				service_info.type = VGO_RUNNABLE_BOOST_DISABLE;
				service_info.data[0] = 1;

				mutex_lock(&service_mutex);
				kfifo_in(&service_fifo, &service_info, 1);
				mutex_unlock(&service_mutex);

				set_runnable_boost_disable = 1;

			}
			if (!set_margin_control) {
				pr_info("[vgo] ack margin control\n");
				service_info.type = VGO_MARGIN_CONTROL_0;
				service_info.data[0] = 1000;
				service_info.data[1] = 20;
				service_info.data[2] = 0;

				mutex_lock(&service_mutex);
				kfifo_in(&service_fifo, &service_info, 1);
				mutex_unlock(&service_mutex);

				set_margin_control = 1;
			}
		} else {
			if (set_runnable_boost_disable) {
				pr_info("[vgo] rel disable Runnable_boost\n");
				service_info.type = VGO_RUNNABLE_BOOST_DISABLE;
				service_info.data[0] = 0;

				mutex_lock(&service_mutex);
				kfifo_in(&service_fifo, &service_info, 1);
				mutex_unlock(&service_mutex);
				set_runnable_boost_disable = 0;
			}
			if (set_margin_control) {
				pr_info("[vgo] rel margin control\n");
				service_info.type = VGO_MARGIN_CONTROL_0;
				service_info.data[0] = 0;

				mutex_lock(&service_mutex);
				kfifo_in(&service_fifo, &service_info, 1);
				mutex_unlock(&service_mutex);
				set_margin_control = 0;
			}
		}

		if (!kfifo_is_empty(&service_fifo)) {
			pr_info("[vgo] Hints service_wq\n");
			wake_up_interruptible(&service_wq);
		}
		controller_wq_ready = 0;
	}
	return 0;
}

static int videogo_open(struct inode *inode, struct file *file)
{
	pr_info("videogo: Device opened\n");
	return 0;
}

static int videogo_release(struct inode *inode, struct file *file)
{
	pr_info("videogo: Device closed\n");
	return 0;
}

static long videogo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct inst_data_user data;
	struct vgo_powerhal_info service_info;

	switch (cmd) {
	case VGO_IOCTL_SET_PROCTIME:
		if (copy_from_user(&data, (struct inst_data_user __user *)arg, sizeof(data))) {
			pr_err("VGO_IOCTL_SET_PROCTIME: copy_from_user is fail\n");
			return -EFAULT;
		}
		pr_info("Recv data: ctx_id=%d, avg=%d, max=%d, min=%d, count=%d\n",
				data.ctx_id, data.avg_proc_time, data.max_proc_time,
				data.min_proc_time, data.count);
		break;
	case VGO_IOCTL_GET:
		wait_event_interruptible(service_wq, !kfifo_is_empty(&service_fifo));
		mutex_lock(&service_mutex);
		ret = kfifo_out(&service_fifo, &service_info, 1);
		mutex_unlock(&service_mutex);
		if (ret != 1) {
			pr_info("kfifo_out is abnormal: %d\n", ret);
			return -EFAULT;
		}
		if (copy_to_user((struct vgo_powerhal_info __user *)arg,
					 &service_info, sizeof(service_info))) {
			return -EFAULT;
		}
		break;
	default:
		return -EFAULT;
	}

	return 0;
}

static const struct file_operations fops = {
	.open = videogo_open,
	.release = videogo_release,
	.unlocked_ioctl = videogo_ioctl,
};

/*
 * driver initialization entry point
 */
static struct task_struct *passive_thread;
static struct task_struct *controller_thread;

static int __init videogo_init(void)
{
	int ret;
	dev_t dev;

	INIT_KFIFO(service_fifo);
	init_waitqueue_head(&passive_wq);
	init_waitqueue_head(&controller_wq);
	mutex_init(&passive_workqueue_mutex);
	mutex_init(&service_mutex);

	pr_info("videogo: _init\n");

	ret = register_chrdev(0, DEVICE_NAME, &fops);
	if (ret < 0) {
		pr_err("videogo: Failed to register character device:%s\n", DEVICE_NAME);
		return ret;
	}
	major_num = ret;
	pr_info("videogo: Registered with major number %d\n", major_num);

	cdev_init(&videogo_cdev, &fops);
	videogo_cdev.owner = THIS_MODULE;

	dev = MKDEV(major_num, 0);
	ret = cdev_add(&videogo_cdev, dev, 1);
	if (ret <0) {
		unregister_chrdev(major_num, DEVICE_NAME);
		pr_err("videogo: Adding cdev failed with:%d\n", ret);
		return ret;
	}

	videogo_class = class_create(CLASS_NAME);
	if (IS_ERR(videogo_class)) {
		unregister_chrdev(major_num, DEVICE_NAME);
		pr_err("videogo: Failed to register device class:%s\n", CLASS_NAME);
		return PTR_ERR(videogo_class);
	}
	pr_info("videogo: Device class registered\n");

	videogo_device = device_create(videogo_class, NULL,
						 MKDEV(major_num, 0), NULL, DEVICE_NAME);
	if (IS_ERR(videogo_device)) {
		class_destroy(videogo_class);
		unregister_chrdev(major_num, DEVICE_NAME);
		pr_err("videogo: Failed to create the device:%s\n", DEVICE_NAME);
		return PTR_ERR(videogo_device);
	}
	pr_info("videogo: Device class created\n");

	passive_thread = kthread_run(videogo_passive_fn, NULL, "videogo_passive");
	if (IS_ERR(passive_thread)) {
		pr_err("videogo: Failed to create videogo_passive thread\n");
		return PTR_ERR(passive_thread);
	}

	controller_thread = kthread_run(videogo_controller_fn, NULL, "videogo_controller");
	if (IS_ERR(controller_thread)) {
		pr_err("videogo: Failed to create videogo_controller thread\n");
		kthread_stop(passive_thread);
		return PTR_ERR(controller_thread);
	}

	mtk_vcodec_vgo_send(VGO_SEND_UPDATE_FN, videogo_vcodec_send_fn);

	pr_info("videogo: module loaded\n");

	return 0;
}
module_init(videogo_init);

/*
 * driver exit point
 */
static void videogo_exit(void)
{
	struct inst_node *info, *tmp;
	int i;

	kthread_stop(passive_thread);
	kthread_stop(controller_thread);

	if (active_thread != NULL)
		kthread_stop(active_thread);

    // Clean instance list
	for (i = 0; i < MAX_CODEC_TYPE; i++) {
		mutex_lock(&inst_list_mutex[i]);
		list_for_each_entry_safe(info, tmp, &inst_list[i], list) {
			list_del(&info->list);
			kfree(info);
		}
		mutex_unlock(&inst_list_mutex[i]);
	}

	// Clean passive_workqueue
	mutex_lock(&passive_workqueue_mutex);
	while(!list_empty(&passive_workqueue)) {
		struct data_entry *entry;

		entry = list_first_entry(&passive_workqueue, struct data_entry, list);

		list_del(&entry->list);
		kfree(entry);
	}
	mutex_unlock(&passive_workqueue_mutex);

	device_destroy(videogo_class, MKDEV(major_num, 0));
	class_unregister(videogo_class);
	class_destroy(videogo_class);
	cdev_del(&videogo_cdev);
	unregister_chrdev(major_num, DEVICE_NAME);
}
module_exit(videogo_exit);

MODULE_DESCRIPTION("MEDIATEK Module VIDEOGO driver");
MODULE_AUTHOR("Mediatek");
MODULE_LICENSE("GPL");
