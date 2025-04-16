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

static DECLARE_WAIT_QUEUE_HEAD(controller_wq);
static atomic_t controller_wq_ready;

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
static int set_uclamp_min_ta;
//static int set_cpu_freq_min;
static int set_gpu_freq_min;
static int target_fps_count[MAX_CODEC_TYPE] = {0};
static int alive_count[MAX_CODEC_TYPE] = {0};
static int isTranscoding;
//static struct task_struct *kvideogo_active;
//static bool videogo_enable;


static void videogo_vcodec_send_fn(int iotype, void *data)
{
	struct data_entry *entry;

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return;

	entry->type = iotype;

	switch(iotype) {
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
		pr_info("type: %d not support\n", iotype);
		kfree(entry);
		return;
	}

	mutex_lock(&passive_workqueue_mutex);
	list_add_tail(&entry->list, &passive_workqueue);
	mutex_unlock(&passive_workqueue_mutex);

	wake_up_interruptible(&passive_wq);
}

static int is_transcoding(struct inst_node *venc_info)
{
	struct inst_node *vdec_info, *tmp;
	int ret = 0;

	// Check has the same caller_pid of venc from vdec list
	// One VENC can be paired with multiple VDECs
	list_for_each_entry_safe(vdec_info, tmp, &inst_list[VDEC], list) {
		if (vdec_info->caller_pid == venc_info->caller_pid) {
			// abnormal oprate, it means best effort mode
			if (vdec_info->oprate > 960 || venc_info->oprate > 960)
				ret = 1;

			// oprate is 0, it means best effort mode
			// Need to avoid Vilte Scenario
			if (vdec_info->oprate == 0 && venc_info->oprate == 0) {
				ret = 1;

				// Could be Vilte Scenario
				if (venc_info->width * venc_info->height <= FHD_SIZE &&
					venc_info->oprate_avdvfs < 35)
					ret = 0;
			}

			if ((vdec_info->oprate != 0 || venc_info->oprate != 0) &&
				((abs(vdec_info->oprate - vdec_info->oprate_avdvfs) > 10) ||
				(abs(venc_info->oprate - venc_info->oprate_avdvfs) > 10)))
				ret = 1;
		}

		if (ret)
			break;
	}

	return ret;
}

static int videogo_process_data(int iotype, void *data)
{
	struct inst_node *info0, *tmp;
	int ret = VGO_IDLE;

	if (iotype == VGO_RECV_INSTANCE_INC) {
		struct inst_init_data *init_data = (struct inst_init_data *)data;
		int type = init_data->inst_type;

		info0 = kmalloc(sizeof(*info0), GFP_KERNEL);
		if (!info0)
			return -ENOMEM;

		info0->inst_type = type;
		info0->ctx_id = init_data->ctx_id;
		info0->caller_pid = init_data->caller_pid;
		info0->fourcc = init_data->fourcc;
		info0->oprate = init_data->oprate;
		info0->oprate_vgo = 0;

		if ((init_data->width > 0 && init_data->width < INT_MAX) &&
			(init_data->height > 0 && init_data->height < INT_MAX)) {
			info0->width = init_data->width;
			info0->height = init_data->height;
		} else {
			kfree(info0);
			return -ENOMEM;
		}

		memset(info0->hw_proc_time, 0, sizeof(info0->hw_proc_time));
		info0->post_proc_time = 0;
		INIT_LIST_HEAD(&info0->list);

		mutex_lock(&inst_list_mutex[type]);
		list_add(&info0->list, &inst_list[type]);

		alive_count[type]++;
		if (info0->oprate <= TARGET_FPS)
			target_fps_count[type]++;
		mutex_unlock(&inst_list_mutex[type]);

		pr_info("[vgo] INC inst_type: %d, ctx_id: %d, caller_pid: %d, fourcc: %d, oprate: %d, width: %d, height: %d\n",
			info0->inst_type, info0->ctx_id, info0->caller_pid,
			info0->fourcc, info0->oprate, info0->width, info0->height);
	} else if (iotype == VGO_RECV_INSTANCE_DEC) {
		struct inst_init_data *inst_data = (struct inst_init_data *)data;
		int type = inst_data->inst_type;

		mutex_lock(&inst_list_mutex[type]);
		alive_count[type]--;
		if (inst_data->oprate <= TARGET_FPS)
			target_fps_count[type]--;

		list_for_each_entry_safe(info0, tmp, &inst_list[type], list) {
			if (info0->ctx_id == inst_data->ctx_id &&
				info0->inst_type == type) {

				pr_info("[vgo] DEC inst_type: %d, ctx_id: %d\n",
						info0->inst_type, info0->ctx_id);
				list_del(&info0->list);
				kfree(info0);
				break;
			}
		}
		mutex_unlock(&inst_list_mutex[type]);

		isTranscoding = alive_count[VENC] == 0 ? 0 : isTranscoding;

	} else if (iotype == VGO_RECV_RUNNING_UPDATE) {
		struct inst_data *run_data = (struct inst_data *)data;
		int type = run_data->inst_type;

		mutex_lock(&inst_list_mutex[type]);
		list_for_each_entry_safe(info0, tmp, &inst_list[type], list) {
			if (info0->ctx_id == run_data->ctx_id &&
				info0->inst_type == run_data->inst_type) {
				info0->oprate_avdvfs = run_data->oprate;
				memcpy(info0->hw_proc_time, run_data->hw_proc_time,
					   sizeof(info0->hw_proc_time));
				info0->updated = 1;

				break;
			}
		}
		mutex_unlock(&inst_list_mutex[type]);

		//struct oprate_data oprate_vgo;
		//oprate_vgo.inst_type = type;
		//oprate_vgo.ctx_id = info0->ctx_id;
		//oprate_vgo.oprate = info0->oprate_avdvfs;
		//mtk_vcodec_vgo_send(VGO_SEND_OPRATE, videogo_vcodec_send_fn);

		if (!isTranscoding)
			isTranscoding = is_transcoding(info0);
		//if (!isTranscoding && info0->oprate_avdvfs > 45 &&
		//	type == VENC && alive_count[VDEC] > 0)
		//	if (info0->oprate_avdvfs >= (info0->oprate * 11 + 9) / 10)
		//		isTranscoding = 1;

		pr_info("[vgo] oprate_avdvfs=%d, oprate=%d, type=%d, isTrans=%d\n",
				info0->oprate_avdvfs, info0->oprate, type, isTranscoding);
	}

	return ret;
}

static int videogo_passive_fn(void *arg)
{
	struct data_entry *entry;
	int ret = 0;

	while(!kthread_should_stop()) {
		ret = wait_event_interruptible(passive_wq, !list_empty(&passive_workqueue) ||
				kthread_should_stop());

		if (ret < 0) {
			pr_info("[vgo] wait event return=%d\n", ret);
			continue;
		}

		if (kthread_should_stop())
			break;

		mutex_lock(&passive_workqueue_mutex);
		while(!list_empty(&passive_workqueue)) {

			entry = list_first_entry(&passive_workqueue, struct data_entry, list);
			videogo_process_data(entry->type, entry->data);

			list_del(&entry->list);
			kfree(entry);
		}
		mutex_unlock(&passive_workqueue_mutex);

		// if need to notify controller thread
		atomic_inc(&controller_wq_ready);
		wake_up_interruptible(&controller_wq);
	}

	return 0;
}

static void send_service_info(const char *log_msg, int service_type,
								int data0, int data1, int data2)
{
	struct vgo_powerhal_info service_info;

	pr_info("[vgo] %s\n", log_msg);
	service_info.type = service_type;
	service_info.data[0] = data0;
	service_info.data[1] = data1;
	service_info.data[2] = data2;
	mutex_lock(&service_mutex);
	kfifo_in(&service_fifo, &service_info, 1);
	mutex_unlock(&service_mutex);
}

static int videogo_controller_fn(void *arg)
{
	int ret = 0;

	while(!kthread_should_stop()) {
		ret = wait_event_interruptible(controller_wq,
				 atomic_read(&controller_wq_ready) > 0 || kthread_should_stop());

		if (ret < 0) {
			pr_info("[vgo] wait event return=%d\n", ret);
			continue;
		}

		if (kthread_should_stop())
			break;
		atomic_dec(&controller_wq_ready);

		int total_vdec_30fps = target_fps_count[VDEC];
		int total_vdec = alive_count[VDEC];
		int total_venc = alive_count[VENC];

		if (total_vdec_30fps > 0 && total_vdec_30fps <= 2 &&
			!total_venc && total_vdec_30fps == total_vdec) {
			if (!set_runnable_boost_disable) {
				send_service_info("ack Runnable_boost_disable",
								VGO_RUNNABLE_BOOST_DISABLE, 1, 0, 0);
				set_runnable_boost_disable = 1;
			}
			if (!set_margin_control) {
				send_service_info("ack margin_control",
								VGO_MARGIN_CONTROL_0, 1000, 20, 0);
				set_margin_control = 1;
			}
		} else {
			if (set_runnable_boost_disable) {
				send_service_info("rel Runnable_boost_disable",
								VGO_RUNNABLE_BOOST_DISABLE, -1, 0, 0);
				set_runnable_boost_disable = 0;
			}
			if (set_margin_control) {
				send_service_info("rel margin_control",
								VGO_MARGIN_CONTROL_0, -1, 0, 0);
				set_margin_control = 0;
			}
		}

		if (isTranscoding) {
			if (!set_uclamp_min_ta) {
				send_service_info("ack uclamp_min_ta",
								VGO_UCLAMP_MIN_TA, 100, 0, 0);
				set_uclamp_min_ta = 1;
			}
			if (!set_gpu_freq_min) {
				send_service_info("ack gpu_freq_min",
								VGO_GPU_FREQ_MIN, 7, 0, 0);
				set_gpu_freq_min = 1;
			}
		} else {
			if (set_uclamp_min_ta) {
				send_service_info("rel uclamp_min_ta",
								VGO_UCLAMP_MIN_TA, -1, 0, 0);
				set_uclamp_min_ta = 0;
			}
			if (set_gpu_freq_min) {
				send_service_info("rel gpu_freq_min",
								VGO_GPU_FREQ_MIN, -1, 0, 0);
				set_gpu_freq_min = 0;
			}
		}

		if (!kfifo_is_empty(&service_fifo))
			wake_up_interruptible(&service_wq);

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
