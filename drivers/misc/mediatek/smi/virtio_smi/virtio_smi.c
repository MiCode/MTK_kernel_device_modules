// SPDX-License-Identifier: GPL-2.0
/*
 * Virtio driver for the paravirtualized smi
 *
 * Copyright (C) 2019 Arm Limited
 * Copyright (c) 2024 MediaTek Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/dma-map-ops.h>
#include <linux/freezer.h>
#include <linux/interval_tree.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/pci.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <linux/wait.h>
#include <uapi/linux/virtio_ids.h>

enum log_level {
	log_debug = 0,
};

#define DBG_LOG(msg, args...)							\
	do {									\
		if (log_level & BIT(log_debug))					\
			pr_info("[%s:%d]" msg, __func__, __LINE__, ##args);	\
	} while (0)

static struct task_struct *virtio_smi_thread_handle;
static u32 log_level;

#define VIRTIO_SMI_S_OK			0x00
#define VIRTIO_SMI_S_IOERR		0x01
#define VIRTIO_SMI_S_UNSUPP		0x02
#define VIRTIO_SMI_S_DEVERR		0x03
#define VIRTIO_SMI_S_INVAL		0x04
#define VIRTIO_SMI_S_RANGE		0x05
#define VIRTIO_SMI_S_NOENT		0x06
#define VIRTIO_SMI_S_FAULT		0x07
#define VIRTIO_SMI_S_NOMEM		0x08

#define VSMI_REQUEST_VQ			0
#define VSMI_NR_VQS			1

struct vsmi_dev {
	struct device		*dev;
	struct virtio_device	*vdev;
	struct virtqueue	*vqs[VSMI_NR_VQS];
	spinlock_t		request_lock;
};

struct virtio_smi_req_tail {
	__u8	status;
	__u8	reserved[3];
};

enum smi_type {
	SMI_LARB = 1,
	SMI_COMMON = 2,
};

enum power_op {
	PM_GET = 1,
	PM_PUT = 2,
};

struct virtio_smi_req_packet {
	enum smi_type			type;
	enum power_op			op;
	unsigned int			smi_id;
	struct virtio_smi_req_tail	tail;
};

struct vsmi_request {
	void		*writeback;
	unsigned int	write_offset;
	unsigned int	len;
	char		buf[];
};

struct vsmi_dev *g_vsmi;

static int vsmi_get_req_errno(void *buf, size_t len)
{
	struct virtio_smi_req_tail *tail = buf + len - sizeof(*tail);

	switch (tail->status) {
	case VIRTIO_SMI_S_OK:
		return 0;
	case VIRTIO_SMI_S_UNSUPP:
		return -EACCES;
	case VIRTIO_SMI_S_INVAL:
		return -EINVAL;
	case VIRTIO_SMI_S_NOENT:
		return -ENOENT;
	case VIRTIO_SMI_S_FAULT:
		return -EFAULT;
	case VIRTIO_SMI_S_NOMEM:
		return -ENOMEM;
	case VIRTIO_SMI_S_IOERR:
	case VIRTIO_SMI_S_DEVERR:
	default:
		return -EIO;
	}
}

static void vsmi_set_req_status(void *buf, size_t len, int status)
{
	struct virtio_smi_req_tail *tail = buf + len - sizeof(*tail);

	tail->status = status;
	DBG_LOG("status = %d\n", status);
}

static off_t vsmi_get_write_desc_offset(struct vsmi_dev *vsmi, size_t len)
{
	size_t tail_size = sizeof(struct virtio_smi_req_tail);

	return len - tail_size;
}

/*
 * __vsmi_sync_req - Complete all in-flight requests
 */
static int __vsmi_sync_req(struct vsmi_dev *vsmi)
{
	unsigned int len = 0;
	size_t write_len;
	struct vsmi_request *req;
	struct virtqueue *vq = vsmi->vqs[VSMI_REQUEST_VQ];

	assert_spin_locked(&vsmi->request_lock);

	virtqueue_kick(vq);

	while (true) {
		req = virtqueue_get_buf(vq, &len);
		if (!req)
			continue;
		if (!len)
			vsmi_set_req_status(req->buf, req->len, VIRTIO_SMI_S_IOERR);
		write_len = req->len - req->write_offset;
		if (req->writeback && len == write_len)
			memcpy(req->writeback, req->buf + req->write_offset,
			       write_len);
		kfree(req);

		break;
	}

	DBG_LOG("geted sync req\n");

	return 0;
}

/*
 * __vsmi_add_request - Add one request to the queue
 * @buf: pointer to the request buffer
 * @len: length of the request buffer
 * @writeback: copy data back to the buffer when the request completes.
 *
 * Add a request to the queue. Only synchronize the queue if it's already full.
 * Otherwise don't kick the queue nor wait for requests to complete.
 *
 * When @writeback is true, data written by the device, including the request
 * status, is copied into @buf after the request completes. This is unsafe if
 * the caller allocates @buf on stack and drops the lock between add_req() and
 * sync_req().
 *
 * Return 0 if the request was successfully added to the queue.
 */
static int __vsmi_add_req(struct vsmi_dev *vsmi, void *buf, size_t len,
				bool writeback)
{
	int ret;
	off_t write_offset;
	struct vsmi_request *req;
	struct scatterlist top_sg, bottom_sg;
	struct scatterlist *sg[2] = { &top_sg, &bottom_sg };
	struct virtqueue *vq = vsmi->vqs[VSMI_REQUEST_VQ];

	assert_spin_locked(&vsmi->request_lock);

	write_offset = vsmi_get_write_desc_offset(vsmi, len);
	if (write_offset <= 0)
		return -EINVAL;

	req = kzalloc(sizeof(*req) + len, GFP_ATOMIC);
	if (!req)
		return -ENOMEM;

	req->len = len;
	if (writeback) {
		req->writeback = buf + write_offset;
		req->write_offset = write_offset;
	}
	memcpy(&req->buf, buf, write_offset);

	sg_init_one(&top_sg, req->buf, write_offset);
	sg_init_one(&bottom_sg, req->buf + write_offset, len - write_offset);

	ret = virtqueue_add_sgs(vq, sg, 1, 1, req, GFP_ATOMIC);
	if (ret) {
		pr_notice("virtqueue_add_sgs add smi packet fail,ret=%d\n", ret);
		goto err_free;
	}

	return 0;

err_free:
	kfree(req);
	return ret;
}

/*
 * Send a request and wait for it to complete. Return the request status (as an
 * errno)
 */
static int vsmi_send_req_sync(struct vsmi_dev *vsmi, void *buf,
			      size_t len)
{
	int ret;
	unsigned long flags;

	if (!vsmi) {
		pr_notice("Invalid vsmi parameter\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&vsmi->request_lock, flags);

	ret = __vsmi_add_req(vsmi, buf, len, true);
	if (ret) {
		dev_notice(vsmi->dev, "could not add request (%d)\n", ret);
		goto out_unlock;
	}

	ret = __vsmi_sync_req(vsmi);
	if (ret)
		dev_notice(vsmi->dev, "could not sync requests (%d)\n", ret);

	ret = vsmi_get_req_errno(buf, len);

	DBG_LOG("ret = %d\n", ret);

out_unlock:
	spin_unlock_irqrestore(&vsmi->request_lock, flags);
	return ret;
}

int vsmi_larb_power_on(unsigned int id)
{
	struct virtio_smi_req_packet smi_pkt;
	int ret = 0;

	smi_pkt.type = SMI_LARB;
	smi_pkt.op = PM_GET;
	smi_pkt.smi_id = id;

	DBG_LOG("larb_power_on\n");
	ret = vsmi_send_req_sync(g_vsmi, &smi_pkt, sizeof(smi_pkt));
	if (ret)
		pr_notice("send SMI req LARB%d power on failed, ret = %d\n",
			   id, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(vsmi_larb_power_on);

int vsmi_larb_power_off(unsigned int id)
{
	struct virtio_smi_req_packet smi_pkt;
	int ret = 0;

	smi_pkt.type = SMI_LARB;
	smi_pkt.op = PM_PUT;
	smi_pkt.smi_id = id;

	DBG_LOG("larb_power_off\n");
	ret = vsmi_send_req_sync(g_vsmi, &smi_pkt, sizeof(smi_pkt));
	if (ret)
		pr_notice("send SMI req LARB%d power off failed, ret = %d\n",
			   id, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(vsmi_larb_power_off);

int vsmi_common_power_on(unsigned int id)
{
	struct virtio_smi_req_packet smi_pkt;
	int ret = 0;

	smi_pkt.type = SMI_COMMON;
	smi_pkt.op = PM_GET;
	smi_pkt.smi_id = id;

	DBG_LOG("common_power_on\n");
	ret = vsmi_send_req_sync(g_vsmi, &smi_pkt, sizeof(smi_pkt));
	if (ret)
		pr_notice("send SMI req COMMON%d power on failed, ret = %d\n",
			   id, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(vsmi_common_power_on);

int vsmi_common_power_off(unsigned int id)
{
	struct virtio_smi_req_packet smi_pkt;
	int ret = 0;

	smi_pkt.type = SMI_COMMON;
	smi_pkt.op = PM_PUT;
	smi_pkt.smi_id = id;

	DBG_LOG("common_power_off\n");
	ret = vsmi_send_req_sync(g_vsmi, &smi_pkt, sizeof(smi_pkt));
	if (ret)
		pr_notice("send SMI req COMMON%d power off failed, ret = %d\n",
			   id, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(vsmi_common_power_off);

#ifdef VIRTIO_SMI_DEBUG
int vsmi_power_op(unsigned int id, bool is_larb, bool is_on)
{
	struct virtio_smi_req_packet smi_pkt;
	int ret;

	if (is_larb && is_on) {
		smi_pkt.type = SMI_LARB;
		smi_pkt.op = PM_GET;
	} else if (is_larb && !is_on) {
		smi_pkt.type = SMI_LARB;
		smi_pkt.op = PM_PUT;
	} else if (!is_larb && is_on) {
		smi_pkt.type = SMI_COMMON;
		smi_pkt.op = PM_GET;
	} else if (!is_larb && !is_on) {
		smi_pkt.type = SMI_COMMON;
		smi_pkt.op = PM_PUT;
	} else {
		pr_notice("Invalid request type:%u and op:%d\n",
			smi_pkt.type, smi_pkt.op);
		return -EINVAL;
	}
	smi_pkt.smi_id = id;

	ret = vsmi_send_req_sync(g_vsmi, &smi_pkt, sizeof(smi_pkt));
	if (ret) {
		pr_notice("send SMI request %s%d %s failed, ret = %d\n",
			is_larb? "LARB": "COMMON", id,
			is_on? "ON": "OFF", ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(vsmi_power_op);

static ssize_t run_test_write(struct file *filp, const char __user *buf,
			      size_t count, loff_t *ppos)
{
	DBG_LOG("vsmi_power_op op start\n");
	vsmi_larb_power_on(11);
	vsmi_larb_power_off(11);
	vsmi_common_power_on(7);
	vsmi_common_power_off(7);
	vsmi_power_op(10, true, true);
	DBG_LOG("vsmi_power_op op done\n");

	return count;
}

static int run_test_show(struct seq_file *m, void *v)
{
	return 0;
}

static int run_test_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, run_test_show, inode->i_private);
}

static const struct file_operations run_test_fops = {
	.open = run_test_open,
	.release = single_release,
	.read = seq_read,
	.write = run_test_write,
	.owner = THIS_MODULE,
};
#endif

static void vsmi_request_handler(struct virtqueue *vq)
{
	struct vsmi_dev *vsmi;

	DBG_LOG("enter\n");
	vsmi = vq->vdev->priv;
}

static int vsmi_init_vqs(struct vsmi_dev *vsmi)
{
	struct virtio_device *vdev = dev_to_virtio(vsmi->dev);
	struct virtqueue_info vqs_info[VSMI_NR_VQS] = {
		[0] = { .name = "request",
			.callback = vsmi_request_handler,
			.ctx = false,
		},
	};

	return virtio_find_vqs(vdev, VSMI_NR_VQS, vsmi->vqs, vqs_info, NULL);
}

static int vsmi_probe(struct virtio_device *vdev)
{
	struct vsmi_dev *vsmi = NULL;
	struct device *dev = &vdev->dev;
	int ret;
#ifdef VIRTIO_SMI_DEBUG
	struct dentry *debugfs_root;
#endif

	if (!virtio_has_feature(vdev, VIRTIO_F_VERSION_1)) {
		pr_notice("%s no VIRTIO_F_VERSION_1\n", __func__);
		return -ENODEV;
	}

	vsmi = devm_kzalloc(dev, sizeof(*vsmi), GFP_KERNEL);
	if (!vsmi)
		return -ENOMEM;

	pr_info("[F/E virtio-smi]: vsmi probing!\n");

	spin_lock_init(&vsmi->request_lock);
	vsmi->dev = dev;
	vsmi->vdev = vdev;

	ret = vsmi_init_vqs(vsmi);
	if (ret) {
		kfree(vsmi);
		return ret;
	}
	virtio_device_ready(vdev);

	vdev->priv = vsmi;
	g_vsmi = vsmi;

#ifdef VIRTIO_SMI_DEBUG
	debugfs_root = debugfs_create_dir("virtio_smi", NULL);
	if (IS_ERR(debugfs_root)) {
		pr_notice("debugfs_create_dir failed: %ld\n", PTR_ERR(debugfs_root));
		ret = PTR_ERR(debugfs_root);
		goto err;
	}

	debugfs_create_file("run_test", 0600, debugfs_root, NULL,
			     &run_test_fops);
#endif

	return 0;

#ifdef VIRTIO_SMI_DEBUG
err:
	kfree(vsmi);
	virtio_break_device(vsmi->vdev);
	vsmi->vdev->config->del_vqs(vsmi->vdev);
	return ret;
#endif
}

static void vsmi_remove(struct virtio_device *vdev)
{
	/* Stop all virtqueues */
	virtio_reset_device(vdev);
	vdev->config->del_vqs(vdev);

	dev_info(&vdev->dev, "device removed\n");
}

static unsigned int features[] = {
};

static struct virtio_device_id id_table[] = {
	{ VIRTIO_SUBSYSTEM_ID_MTK_SMI, VIRTIO_DEV_ANY_ID },
	{ 0 },
};
MODULE_DEVICE_TABLE(virtio, id_table);

static struct virtio_driver virtio_smi_drv = {
	.driver.name		= KBUILD_MODNAME,
	.driver.owner		= THIS_MODULE,
	.id_table		= id_table,
	.feature_table		= features,
	.feature_table_size	= ARRAY_SIZE(features),
	.probe			= vsmi_probe,
	.remove			= vsmi_remove,
};

static int virtio_smi_init_func(void *data)
{
	register_virtio_driver(&virtio_smi_drv);
	return 0;
}

static int virtio_smi_init(void)
{
	int ret = 0;

	virtio_smi_thread_handle = kthread_run(virtio_smi_init_func,
					NULL, "virtio_smi_init_thread");
	if (IS_ERR(virtio_smi_thread_handle)) {
		ret = PTR_ERR(virtio_smi_thread_handle);
		pr_info("virtio_smi_init_thread run fail, err = %d\n", ret);
	}

	return ret;
}

module_init(virtio_smi_init);

module_param(log_level, uint, 0644);
MODULE_PARM_DESC(log_level, "virt smi log level");

MODULE_DESCRIPTION("Virtio SMI driver");
MODULE_AUTHOR("zhengnan.chen <zhengnan.chen@mediatek.com>");
MODULE_LICENSE("GPL");
