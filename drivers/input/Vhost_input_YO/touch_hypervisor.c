// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2024 MediaTek Inc.
 */


#define DEBUG

#define pr_fmt(fmt) "vhost-touch: " fmt

#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/vhost.h>
#include <linux/mutex.h>
#include <linux/file.h>
#include <linux/kthread.h>
#include <linux/llist.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/mailbox_controller.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>

#include <linux/sched.h>
#include <linux/sched/types.h>
#include <uapi/linux/sched/types.h>
#include <asm/current.h>

#include <linux/atomic.h>
#include "vhost.h"

#include "touch_hypervisor.h"

/* Guest driver can echo "touchworld" message to device. */
#define VIRTIO_TOUCH_F_ECHO_MSG 0

/* Max number of bytes transferred before requeueing the job.
 * Using this limit prevents one virtqueue from starving others.
 */
#define VHOST_TOUCH_WEIGHT 0x80000

/* Max number of packets transferred before requeueing the job.
 * Using this limit prevents one virtqueue from starving others with small
 * pkts.
 */
#define VHOST_TOUCH_PKT_WEIGHT 256

#define ZX_OK (0)
#define ZX_ERR_NOT_SUPPORTED (-2)


bool un_init_flag = true;

enum {
	VHOST_TOUCH_FEATURES = (1ULL << VIRTIO_RING_F_INDIRECT_DESC)
			| (1ULL << VIRTIO_RING_F_EVENT_IDX)
			| (1ULL << VIRTIO_TOUCH_F_ECHO_MSG),
};

enum {
	VIRTIO_TOUCH_Q_COMMAND = 0,
	VIRTIO_TOUCH_Q_EVENT = 1,
	VIRTIO_TOUCH_Q_COUNT = 2,
};

enum {
	VIRTIO_TOUCH_CMD_ECHO_SYNC = 0,
	VIRTIO_TOUCH_CMD_ECHO_ASYNC = 1,
	VIRTIO_TOUCH_CMD_START_EVENT = 2,
};

enum {
	VIRTIO_TOUCH_EVENT_TYPE_NORMAL = 0,
	VIRTIO_TOUCH_EVENT_TYPE_CB = 1,
};

/**
 * Request, Response, and Event protocol for virtio-touch device.
 */
struct virtio_touch_req {
	uint32_t id;
	uint32_t cmd;
	uint8_t data[MAX_VIRTIO_SEND_BYTE];
};

struct virtio_touch_rsp {
	uint32_t rc;
	uint8_t data[MAX_VIRTIO_SEND_BYTE];
};

struct virtio_touch_event {
	uint32_t id;
	uint32_t type;
	uint32_t cmd_id;
	union {
		uint8_t data[MAX_VIRTIO_SEND_BYTE];
		struct virtio_touch_rsp rsp;
	};
};

struct mutex touch_using_mutex;

struct event_buffer_entry {
	struct virtio_touch_event event;
	struct llist_node llnode;
};

struct vhost_touch {
	struct vhost_virtqueue vq[VIRTIO_TOUCH_Q_COUNT];
	struct vhost_dev dev;
	struct vhost_work work;
	struct vhost_work prio_work;
	spinlock_t evt_lock;
	unsigned int evt_nr;
	struct event_buffer_entry *evt_buf;
	struct llist_head evt_pool;
	struct llist_head evt_queue;
};


struct vhost_touch *gtouch;

static int vhost_touch_report_evt(struct vhost_virtqueue *vq, const char *data)
{
	//struct vhost_touch *touch;
	struct event_buffer_entry *evt;
	struct llist_node *node = NULL;
	ssize_t length = 0;

	vq=&gtouch->vq[VIRTIO_TOUCH_Q_EVENT];
	if (un_init_flag == false) {
		if (gtouch == NULL) {
			WARN_ON_ONCE(gtouch == NULL);
			return -EINVAL;
		}
		spin_lock(&gtouch->evt_lock);
		node = llist_del_first(&gtouch->evt_pool);
		if (node == NULL) {
			WARN_ON_ONCE(node == NULL);
			spin_unlock(&gtouch->evt_lock);
			return -EINVAL;
		}
		llist_add(node, &gtouch->evt_queue);
		spin_unlock(&gtouch->evt_lock);
		evt = container_of(node, typeof(*evt), llnode);
		if (evt == NULL) {
			WARN_ON_ONCE(evt == NULL);
			return -EINVAL;
		}
		evt->event.type = VIRTIO_TOUCH_EVENT_TYPE_NORMAL;
		length = strscpy(evt->event.data, data, sizeof(evt->event.data));
		if (length <= 0 || length > sizeof(evt->event.data)) {
			pr_info("%s ERROR: strscpy failed\n", __func__);
			return -EINVAL;
		}
		vhost_vq_work_queue(vq, &gtouch->work);
		return 0;
	} else
		return -EINVAL;
}

static void vhost_touch_report_cb(struct vhost_virtqueue *vq, uint32_t cmd_id,
		const char *data)
{
	struct vhost_touch *touch;
	struct event_buffer_entry *evt;
	struct llist_node *node;

	vq=&touch->vq[VIRTIO_TOUCH_Q_EVENT];
	touch = container_of(vq->dev, struct vhost_touch, dev);
	spin_lock(&touch->evt_lock);
	node = llist_del_first(&touch->evt_pool);
	WARN_ON_ONCE(node == NULL);
	llist_add(node, &touch->evt_queue);
	spin_unlock(&touch->evt_lock);

	evt = container_of(node, typeof(*evt), llnode);
	evt->event.cmd_id = cmd_id;
	evt->event.type = VIRTIO_TOUCH_EVENT_TYPE_CB;
	evt->event.rsp.rc = 0;
	strscpy(evt->event.rsp.data, data, sizeof(evt->event.rsp.data));

	vhost_vq_work_queue(vq, &touch->work);

}

/* Host kick us for I/O completion */
static void vhost_touch_handle_host_kick(struct vhost_work *work)
{
	struct vhost_virtqueue *vq;
	struct event_buffer_entry *evt;
	struct llist_node *llnode;
	struct llist_head reclaim_head;
	struct vhost_touch *touch;
	bool added;
	unsigned long flags;
	size_t copy_len = sizeof(struct virtio_touch_event);
	int head, in, out;

	init_llist_head(&reclaim_head);
	touch = container_of(work, struct vhost_touch, work);
	vq = &touch->vq[VIRTIO_TOUCH_Q_EVENT];
	spin_lock_irqsave(&touch->evt_lock, flags);
	while ((llnode = llist_del_first(&touch->evt_queue)) != NULL)
		llist_add(llnode, &reclaim_head);
	spin_unlock_irqrestore(&touch->evt_lock, flags);
	added = false;
	llnode = reclaim_head.first;
	vhost_disable_notify(&touch->dev, vq);
	while (llnode) {
		struct iov_iter iter;

		evt = llist_entry(llnode, struct event_buffer_entry, llnode);
		llnode = llist_next(llnode);
		head = vhost_get_vq_desc(vq, vq->iov, ARRAY_SIZE(vq->iov), &out,
					 &in, NULL, NULL);
		if (unlikely(head < 0)) {
			vq_err(vq, "failed to get vring desc: %d\n", head);
			continue;
		}
		if (unlikely(head == vq->num)) {
			if (unlikely(vhost_enable_notify(&touch->dev, vq)))
				vhost_disable_notify(&touch->dev, vq);
			continue;
		}
		iov_iter_init(&iter, ITER_DEST, &vq->iov[0], 1, copy_len);
		if (copy_to_iter(&evt->event, copy_len, &iter) != copy_len) {
			vq_err(vq, "Failed to write event\n");
			continue;
		}
		vhost_add_used(vq, head, copy_len);
		added = true;
	}
	if (likely(added))
		vhost_signal(&touch->dev, &touch->vq[VIRTIO_TOUCH_Q_EVENT]);
	spin_lock_irqsave(&touch->evt_lock, flags);
	while ((llnode = llist_del_first(&reclaim_head)) != NULL)
		llist_add(llnode, &touch->evt_pool);
	spin_unlock_irqrestore(&touch->evt_lock, flags);
}

struct test_arg {
	struct vhost_touch *touch;
	struct virtio_touch_req req;
	int count;
};

static int test_cmd_callback(void *data)
{
	struct test_arg *arg = data;

	ssleep(1);
	vhost_touch_report_cb(&arg->touch->vq[VIRTIO_TOUCH_Q_EVENT], arg->req.id,
			"OK");

	kfree(arg);
	return 0;
}

static int test_event(void *data)
{
	struct test_arg *arg = data;

	for (int i = 0; i < arg->count; i++) {
		vhost_touch_report_evt(&arg->touch->vq[VIRTIO_TOUCH_Q_EVENT],
				"Hello World!");
		ssleep(1);
	}

	kfree(arg);
	return 0;
}

static void handle_touch_request(struct vhost_touch *touch,
		struct vhost_virtqueue *vq, struct virtio_touch_req *req,
		struct virtio_touch_rsp *rsp)
{
	switch (req->cmd) {
	case VIRTIO_TOUCH_CMD_ECHO_SYNC: {
		pr_info("SYNC[BE]TOUCH device received command(id:%d cmd:%d data:%s length:%d)\n",
				req->id, req->cmd, req->data, strlen(req->data));

		strscpy((char *) rsp->data, "OK", sizeof(rsp->data));
		rsp->rc = ZX_OK;
		break;
	}
	case VIRTIO_TOUCH_CMD_ECHO_ASYNC: {
		struct test_arg *arg = kvzalloc(sizeof(*arg), GFP_KERNEL);

		arg->touch = touch;
		arg->req = *req;

		pr_info("ASYNC[BE]TOUCH device received command(id:%d cmd:%d data:%s)\n",
				req->id, req->cmd, req->data);

		kthread_run(test_cmd_callback, arg, "test_cmd_callback");
		break;
	}
	case VIRTIO_TOUCH_CMD_START_EVENT: {
		int count = 10;
		struct test_arg *arg = kvzalloc(sizeof(*arg), GFP_KERNEL);

		arg->touch = touch;
		arg->count = count;

		pr_info("[BE]TOUCH device start event thread last for %d seconds.\n",
				count);

		kthread_run(test_event, arg, "test_event");
		break;
	}
	}
}

static void vhost_touch_handle_guest_cmd_kick(struct vhost_work *work)
{
	struct virtio_touch_req req;
	struct virtio_touch_rsp resp;
	struct vhost_virtqueue *vq;
	struct vhost_touch *touch;
	int head;
	bool added = false;

	vq = container_of(work, struct vhost_virtqueue, poll.work);
	touch = container_of(vq->dev, struct vhost_touch, dev);
	vhost_disable_notify(&touch->dev, vq);
	for (;;) {
		int in, out, ret, used = 0;
		size_t out_size, in_size;
		struct iovec *out_iov, *in_iov;
		struct iov_iter out_iter, in_iter;

		head = vhost_get_vq_desc(vq, vq->iov, ARRAY_SIZE(vq->iov), &out,
					 &in, NULL, NULL);
		if (unlikely(head < 0)) {
			vq_err(vq, "failed to get vring desc: %d\n", head);
			break;
		}
		if (unlikely(head == vq->num)) {
			if (unlikely(vhost_enable_notify(&touch->dev, vq))) {
				vhost_disable_notify(&touch->dev, vq);
				continue;
			}
			break;
		}
		out_iov = vq->iov;
		out_size = iov_length(out_iov, out);
		iov_iter_init(&out_iter, ITER_SOURCE, out_iov, out, out_size);
		ret = copy_from_iter(&req, sizeof(req), &out_iter);
		if (ret != sizeof(req)) {
			vq_err(vq, "Failed to copy request, ret=%d\n", ret);
			vhost_discard_vq_desc(vq, 1);
			break;
		}
		handle_touch_request(touch, vq, &req, &resp);
		if (in > 0) {
			in_iov = &vq->iov[out];
			in_size = iov_length(in_iov, in);

			iov_iter_init(&in_iter, ITER_DEST, in_iov, in, in_size);
			ret = copy_to_iter(&resp, sizeof(resp), &in_iter);
			if (ret != sizeof(resp)) {
				vq_err(vq, "Failed to copy result, ret=%d\n", ret);
				vhost_discard_vq_desc(vq, 1);
				break;
			}
			used += ret;
		}
		vhost_add_used(vq, head, used);
		added = true;
	}
		if (added)
			vhost_signal(&touch->dev, vq);
}

static void vhost_touch_handle_guest_evt_kick(struct vhost_work *work)
{
}

static void vhost_touch_handle_prio_work(struct vhost_work *prio_work)
{
	struct task_struct *task = current;
	struct sched_param param;
	int ret = 0;
	int prio = 20;

	param.sched_priority = prio;
	if (task != NULL) {
		ret = sched_setscheduler(task, SCHED_FIFO, &param);
		pr_info("[touch]%s() task:%p prio:%d ret:%d\n", __func__, task, prio, ret);
	} else
		pr_info("[touch]%s() task is null\n", __func__);
}

static int vhost_touch_open(struct inode *inode, struct file *file)
{
	//struct vhost_touch *touch;
	struct vhost_virtqueue **vqs;
	int ret = 0;

	mutex_lock(&touch_using_mutex);
	gtouch = kvzalloc(sizeof(*gtouch), GFP_KERNEL);
	if (!gtouch) {
		ret = -ENOMEM;
		goto out;
	}

	vqs = kcalloc(VIRTIO_TOUCH_Q_COUNT, sizeof(*vqs), GFP_KERNEL);
	if (!vqs) {
		ret = -ENOMEM;
		goto out_touch;
	}

	spin_lock_init(&gtouch->evt_lock);
	un_init_flag = false;
	init_llist_head(&gtouch->evt_pool);
	init_llist_head(&gtouch->evt_queue);

	gtouch->vq[VIRTIO_TOUCH_Q_COMMAND].handle_kick = vhost_touch_handle_guest_cmd_kick;
	gtouch->vq[VIRTIO_TOUCH_Q_EVENT].handle_kick = vhost_touch_handle_guest_evt_kick;

	vqs[VIRTIO_TOUCH_Q_COMMAND] = &gtouch->vq[VIRTIO_TOUCH_Q_COMMAND];
	vqs[VIRTIO_TOUCH_Q_EVENT] = &gtouch->vq[VIRTIO_TOUCH_Q_EVENT];
	vhost_work_init(&gtouch->work, vhost_touch_handle_host_kick);
	vhost_work_init(&gtouch->prio_work, vhost_touch_handle_prio_work);
	vhost_dev_init(&gtouch->dev, vqs, VIRTIO_TOUCH_Q_COUNT, UIO_MAXIOV,
			VHOST_TOUCH_PKT_WEIGHT, VHOST_TOUCH_WEIGHT, true, NULL);
	file->private_data = gtouch;
	mutex_unlock(&touch_using_mutex);
	return ret;
out_touch:
	kvfree(gtouch);
out:
	mutex_unlock(&touch_using_mutex);
	return ret;
}

static int vhost_touch_release(struct inode *inode, struct file *f)
{
	struct vhost_touch *touch = f->private_data;

	mutex_lock(&touch_using_mutex);
	vhost_dev_stop(&touch->dev);
	vhost_dev_cleanup(&touch->dev);
	kfree(touch->dev.vqs);
	kfree(touch->evt_buf);
	kvfree(touch);
	gtouch = NULL;
	mutex_unlock(&touch_using_mutex);
	return 0;
}

static int vhost_touch_set_features(struct vhost_touch *touch, u64 features)
{
	struct vhost_virtqueue *vq;
	int i;

	mutex_lock(&touch->dev.mutex);
	for (i = 0; i < VIRTIO_TOUCH_Q_COUNT; i++) {
		vq = &touch->vq[i];
		mutex_lock(&vq->mutex);
		vq->acked_features = features;
		mutex_unlock(&vq->mutex);
	}
	mutex_unlock(&touch->dev.mutex);

	return 0;
}

static long vhost_touch_reset_owner(struct vhost_touch *touch)
{
	struct vhost_iotlb *umem;
	int err;

	mutex_lock(&touch->dev.mutex);
	err = vhost_dev_check_owner(&touch->dev);
	if (err)
		goto done;

	umem = vhost_dev_reset_owner_prepare();
	if (!umem) {
		err = -ENOMEM;
		goto done;
	}

	vhost_dev_reset_owner(&touch->dev, umem);

done:
	mutex_unlock(&touch->dev.mutex);
	return err;
}

static int vhost_touch_setup(struct vhost_touch *touch)
{
	int i;
	struct event_buffer_entry *evt;

	touch->evt_nr = touch->vq[VIRTIO_TOUCH_Q_EVENT].num;

	touch->evt_buf = kmalloc_array(touch->evt_nr,
		sizeof(struct event_buffer_entry), GFP_KERNEL);
	if (!touch->evt_buf)
		return -ENOMEM;

	for (i = 0; i < touch->evt_nr; i++) {
		evt = &touch->evt_buf[i];
		llist_add(&evt->llnode, &touch->evt_pool);
	}

	return 0;
}

static long vhost_touch_ioctl(struct file *f, unsigned int ioctl,
			    unsigned long arg)
{
	struct vhost_touch *touch = f->private_data;
	void __user *argp = (void __user *)arg;
	u64 __user *featurep = argp;
	u64 features;
	int ret;

	switch (ioctl) {
	case VHOST_GET_FEATURES:
		features = VHOST_TOUCH_FEATURES;
		if (copy_to_user(featurep, &features, sizeof(features)))
			return -EFAULT;
		return 0;
	case VHOST_SET_FEATURES:
		if (copy_from_user(&features, featurep, sizeof(features)))
			return -EFAULT;
		if (features & ~VHOST_TOUCH_FEATURES)
			return -EOPNOTSUPP;
		return vhost_touch_set_features(touch, features);
	case VHOST_RESET_OWNER:
		return vhost_touch_reset_owner(touch);
	default:
		mutex_lock(&touch->dev.mutex);
		ret = vhost_dev_ioctl(&touch->dev, ioctl, argp);
		if (ret == -ENOIOCTLCMD)
			ret = vhost_vring_ioctl(&touch->dev, ioctl, argp);
		if (!ret && ioctl == VHOST_SET_VRING_NUM)
			ret = vhost_touch_setup(touch);
		if (ioctl == VHOST_SET_OWNER)
			vhost_vq_work_queue(&touch->vq[VIRTIO_TOUCH_Q_EVENT], &touch->prio_work);
		mutex_unlock(&touch->dev.mutex);
		return ret;
	}
}

static int get_real_time(void)
{
	struct timespec64 ts;
	long long ms;
	int current_time;

	ktime_get_real_ts64(&ts);
	ms = ts.tv_sec * 1000LL + ts.tv_nsec / 1000000L;
	current_time = ms % 1000;

	return current_time;
}

static atomic_t mbrain_index = ATOMIC_INIT(1);

int touch_event(char *tpd_info)
{
	struct test_arg *arg = kvzalloc(sizeof(*arg), GFP_KERNEL);
	char *str = tpd_info;
	int ret = 0;
	int index;
	int cur_time = 0;

	mutex_lock(&touch_using_mutex);

	index = atomic_inc_return(&mbrain_index);
	if (index > 10) {
		atomic_set(&mbrain_index, 1);
		index = 1;
	}
	cur_time = get_real_time();
	sprintf(str + strlen(str), " %d %d", index, cur_time);
	arg->touch = gtouch;
	vhost_touch_report_evt(&arg->touch->vq[VIRTIO_TOUCH_Q_EVENT], str);
	mutex_unlock(&touch_using_mutex);

	kfree(arg);
	return 0;
}
EXPORT_SYMBOL_GPL(touch_event);

static const struct file_operations vhost_touch_fops = {
	.owner          = THIS_MODULE,
	.open           = vhost_touch_open,
	.release        = vhost_touch_release,
	.llseek         = noop_llseek,
	.unlocked_ioctl = vhost_touch_ioctl,
};

static struct miscdevice vhost_touch_misc = {
	MISC_DYNAMIC_MINOR,
	"vhost-touch",
	&vhost_touch_fops,
};

int vhost_touch_init(void)
{
	int ret;

	mutex_init(&touch_using_mutex);
	gtouch = NULL;
	ret = misc_register(&vhost_touch_misc);
	return ret;
}

void vhost_touch_deinit(void)
{

	misc_deregister(&vhost_touch_misc);
}

module_init(vhost_touch_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek touch panel driver");
MODULE_AUTHOR("Pavel Xu<Pavel.Xu@mediatek.com>");
