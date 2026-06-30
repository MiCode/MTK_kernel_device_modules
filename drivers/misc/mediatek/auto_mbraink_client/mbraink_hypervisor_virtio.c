// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "virtio-mbraink: " fmt

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/spinlock.h>
#include <linux/virtio.h>
#include <linux/vringh.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_ring.h>
#include <linux/dma-mapping.h>
#include <linux/virtio_config.h>
#include <linux/completion.h>
#include <linux/mailbox_controller.h>
#include <linux/debugfs.h>
#include <linux/workqueue.h>
#include <linux/rbtree.h>

#include <linux/clk.h>

#include "mbraink_hypervisor_virtio.h"
#include "mbraink_ioctl_struct_def.h"

/* Guest driver can echo "mbrainkworld" message to device. */
#define VIRTIO_MBRAINK_F_ECHO_MSG  0
#define VIRTIO_ID_MBRAINK          57

static unsigned int features[] = {
	VIRTIO_MBRAINK_F_ECHO_MSG,
};

static struct virtio_device_id id_table[] = {
	{ VIRTIO_ID_MBRAINK, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

char test_buff[512] = {0};
enum {
	VIRTIO_MBRAINK_Q_COMMAND = 0,
	VIRTIO_MBRAINK_Q_EVENT = 1,
	VIRTIO_MBRAINK_Q_COUNT = 2,
};

enum {
	VIRTIO_MBRAINK_CMD_ECHO_SYNC = 0,
	VIRTIO_MBRAINK_CMD_ECHO_ASYNC = 1,
	VIRTIO_MBRAINK_CMD_START_EVENT = 2,
};

enum {
	VIRTIO_MBRAINK_EVENT_TYPE_NORMAL = 0,
	VIRTIO_MBRAINK_EVENT_TYPE_CB = 1,
};

/**
 * Request, Response, and Event protocol for virtio-mbraink device.
 */
struct virtio_mbraink_req {
	uint32_t id;
	uint32_t cmd;
	uint8_t data[MAX_VIRTIO_BUFFER_BYTE];
};
struct virtio_mbraink_rsp {
	uint32_t rc;
	uint8_t data[MAX_VIRTIO_BUFFER_BYTE];
};
struct virtio_mbraink_event {
	uint32_t id;
	uint32_t type;
	uint32_t cmd_id;
	union {
		uint8_t data[MAX_VIRTIO_BUFFER_BYTE];
		struct virtio_mbraink_rsp rsp;
	};
};

/**
 * virtio-mbraink device.
 */
struct virtio_mbraink {
	struct virtio_device *vdev;
	struct virtqueue *vqs[VIRTIO_MBRAINK_Q_COUNT];
	spinlock_t cmd_lock;
	spinlock_t evt_lock;
	uint32_t cmd_count;
	uint32_t event_count;
	struct dentry *dbg_file;
	struct rb_root cmd_tree;
};

/**
 * Callback for asynchronous command call.
 */
typedef void mbraink_callback_t(struct virtio_mbraink_rsp *rsp);

/**
 * Command for virtio-mbraink device.
 */
struct virtio_mbraink_cmd {
	struct virtio_mbraink_req req;
	struct virtio_mbraink_rsp rsp;
	struct completion done;
	struct rb_node node;
	mbraink_callback_t *cb;
};

static bool cmd_less(struct rb_node *node, const struct rb_node *parent)
{
	struct virtio_mbraink_cmd *cmd = container_of(node, struct virtio_mbraink_cmd,
			node);
	struct virtio_mbraink_cmd *cmd_parent = container_of(node,
			struct virtio_mbraink_cmd, node);
	return cmd->req.id < cmd_parent->req.id;
}

static int cmd_cmp(const void *key, const struct rb_node *node)
{
	uint32_t id = *(uint32_t *)key;
	struct virtio_mbraink_cmd *cmd = container_of(node, struct virtio_mbraink_cmd,
				node);
	if (id < cmd->req.id)
		return -1;
	else if (id > cmd->req.id)
		return 1;
	else
		return 0;
}

static void handle_cmd_callbak(struct virtio_mbraink *mbraink,
		struct virtio_mbraink_event *evt)
{
	struct rb_node *node;
	struct virtio_mbraink_cmd *cmd;
	unsigned long flags;

	spin_lock_irqsave(&mbraink->cmd_lock, flags);
	node = rb_find(&evt->cmd_id, &mbraink->cmd_tree, cmd_cmp);
	if (node)
		rb_erase(node, &mbraink->cmd_tree);
	spin_unlock_irqrestore(&mbraink->cmd_lock, flags);

	if (node) {
		cmd = container_of(node, struct virtio_mbraink_cmd, node);
		if (cmd->cb)
			cmd->cb(&evt->rsp);
		kfree(cmd);
	}
}

static void handle_mbraink_event(struct virtio_mbraink *mbraink,
		struct virtio_mbraink_event *evt)
{
	unsigned int sptr = 0;
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};

	switch (evt->type) {
	case VIRTIO_MBRAINK_EVENT_TYPE_NORMAL:
		sptr = snprintf(netlink_buf,
					NETLINK_EVENT_MESSAGE_SIZE, "%s %s",
					NETLINK_EVENT_HOST2CLEINT,
					evt->data);
		if (sptr < 0 || sptr >= NETLINK_EVENT_MESSAGE_SIZE) {
			pr_info("%s: snprintf error...\n", __func__);
		} else {
			pr_info("%s: send msg to client: %s\n", __func__, netlink_buf);
			mbraink_netlink_send_msg(netlink_buf);
		}
		break;
	case VIRTIO_MBRAINK_EVENT_TYPE_CB:
		handle_cmd_callbak(mbraink, evt);
		break;
	default:
		pr_info("[FE]Received unknown mbraink device event(type:%d)\n",
				evt->type);
	}
}

/* Called from virtio device, in IRQ context */
static void mbraink_cmd_done(struct virtqueue *vq)
{
	struct virtio_mbraink *mbraink = vq->vdev->priv;
	unsigned long flags;
	unsigned int len;
	struct virtio_mbraink_cmd *cmd;

	spin_lock_irqsave(&mbraink->cmd_lock, flags);
	do {
		virtqueue_disable_cb(vq);
		while ((cmd = virtqueue_get_buf(vq, &len)) != NULL)
			complete(&cmd->done);
		if (unlikely(virtqueue_is_broken(vq)))
			break;
	} while (!virtqueue_enable_cb(vq));
	virtqueue_kick(vq);
	spin_unlock_irqrestore(&mbraink->cmd_lock, flags);
}

static void virtio_mbraink_queue_evt_buffer(struct virtio_mbraink *mbraink,
		struct virtio_mbraink_event *evt)
{
	struct virtqueue *vq = mbraink->vqs[VIRTIO_MBRAINK_Q_EVENT];
	struct scatterlist sg[1];

	sg_init_one(sg, evt, sizeof(*evt));
	evt->id = mbraink->event_count++;
	virtqueue_add_inbuf(vq, sg, 1, evt, GFP_ATOMIC);
}

/* Called from virtio device, in IRQ context */
static void mbraink_evt_done(struct virtqueue *vq)
{
	struct virtio_mbraink *mbraink = vq->vdev->priv;
	struct virtio_mbraink_event *evt;
	unsigned long flags;
	unsigned int len;

	spin_lock_irqsave(&mbraink->evt_lock, flags);
	do {
		virtqueue_disable_cb(vq);
		while ((evt = virtqueue_get_buf(vq, &len)) != NULL) {
			handle_mbraink_event(mbraink, evt);
			virtio_mbraink_queue_evt_buffer(mbraink, evt);
		}
		if (unlikely(virtqueue_is_broken(vq)))
			break;
	} while (!virtqueue_enable_cb(vq));
	virtqueue_kick(vq);
	spin_unlock_irqrestore(&mbraink->evt_lock, flags);
}

static int fill_mbraink_evt(struct virtio_mbraink *mbraink)
{
	int i;
	struct virtio_mbraink_event *evts;
	struct virtqueue *vq = mbraink->vqs[VIRTIO_MBRAINK_Q_EVENT];
	size_t nr_evts = vq->num_free;
	unsigned long flags;

	evts = kmalloc_array(nr_evts, sizeof(*evts), GFP_KERNEL);
	if (!evts)
		return -ENOMEM;

	spin_lock_irqsave(&mbraink->evt_lock, flags);
	for (i = 0; i < nr_evts; i++)
		virtio_mbraink_queue_evt_buffer(mbraink, &evts[i]);
	virtqueue_kick(vq);
	spin_unlock_irqrestore(&mbraink->evt_lock, flags);

	return 0;
}

static int virtio_mbraink_queue_cmd_sync(struct virtio_mbraink *mbraink,
		struct virtio_mbraink_cmd *cmd)
{
	int err;
	struct virtqueue *vq = mbraink->vqs[VIRTIO_MBRAINK_Q_COMMAND];
	unsigned long flags;
	struct scatterlist req;
	struct scatterlist rsp;
	struct scatterlist *sgs[2];

	sg_init_one(&req, &cmd->req, sizeof(cmd->req));
	sg_init_one(&rsp, &cmd->rsp, sizeof(cmd->rsp));
	sgs[0] = &req;
	sgs[1] = &rsp;

	spin_lock_irqsave(&mbraink->cmd_lock, flags);
	cmd->req.id = mbraink->cmd_count++;
	err = virtqueue_add_sgs(vq, sgs, 1, 1, cmd, GFP_ATOMIC);
	if (err)
		goto failed;
	virtqueue_kick(vq);
	spin_unlock_irqrestore(&mbraink->cmd_lock, flags);

	return 0;

failed:
	spin_unlock_irqrestore(&mbraink->cmd_lock, flags);
	return err;
}

static int virtio_mbraink_echo_sync_test(struct virtio_mbraink *mbraink)
{
	int err;

	struct virtio_mbraink_cmd *cmd = kzalloc(sizeof(struct virtio_mbraink_cmd),
			GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	cmd->req.cmd = VIRTIO_MBRAINK_CMD_ECHO_SYNC;
	strscpy((char *)cmd->req.data, "Hello world!", sizeof(cmd->req.data));
	init_completion(&cmd->done);

	err = virtio_mbraink_queue_cmd_sync(mbraink, cmd);
	if (err)
		goto failed;

	pr_info("[FE]virtio-mbraink send command(id:%d cmd:%d data:%s)\n", cmd->req.id,
			cmd->req.cmd, cmd->req.data);

	wait_for_completion(&cmd->done);

	pr_info("[FE]virtio-mbraink received response(rc:%d data:%s)\n", cmd->rsp.rc,
			cmd->rsp.data);

	kfree(cmd);

	return 0;

failed:
	kfree(cmd);
	return err;
}

static int virtio_mbraink_start_event_test(struct virtio_mbraink *mbraink)
{
	int err;

	struct virtio_mbraink_cmd *cmd = kzalloc(sizeof(struct virtio_mbraink_cmd),
			GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	cmd->req.cmd = VIRTIO_MBRAINK_CMD_START_EVENT;
	init_completion(&cmd->done);

	err = virtio_mbraink_queue_cmd_sync(mbraink, cmd);
	if (err)
		goto failed;

	pr_info("[FE]virtio-mbraink send command(id:%d cmd:%d)\n", cmd->req.id,
			cmd->req.cmd);

	wait_for_completion(&cmd->done);

	kfree(cmd);
	return 0;

failed:
	kfree(cmd);
	return err;
}

void virtio_mbraink_async_callback(struct virtio_mbraink_rsp *rsp)
{
	pr_info("[FE]virtio-mbraink callback received response(rc:%d data:%s)\n",
			rsp->rc, rsp->data);
}

static int virtio_mbraink_echo_async_test(struct virtio_mbraink *mbraink,
		mbraink_callback_t *cb)
{
	int err;
	struct virtqueue *vq = mbraink->vqs[VIRTIO_MBRAINK_Q_COMMAND];
	unsigned long flags;
	struct scatterlist req;
	struct scatterlist *sgs[1];
	struct virtio_mbraink_cmd *cmd;

	cmd = kzalloc(sizeof(struct virtio_mbraink_cmd), GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	sg_init_one(&req, &cmd->req, sizeof(cmd->req));
	sgs[0] = &req;

	cmd->req.cmd = VIRTIO_MBRAINK_CMD_ECHO_ASYNC;
	strscpy(test_buff, "Hello world, test for mbraink send data!", sizeof(test_buff));
	strscpy((char *)cmd->req.data, test_buff, sizeof(cmd->req.data));
	init_completion(&cmd->done);
	cmd->cb = cb;

	spin_lock_irqsave(&mbraink->cmd_lock, flags);
	cmd->req.id = mbraink->cmd_count++;
	err = virtqueue_add_sgs(vq, sgs, 1, 0, cmd, GFP_ATOMIC);
	if (err)
		goto failed;
	rb_add(&cmd->node, &mbraink->cmd_tree, cmd_less);
	virtqueue_kick(vq);
	spin_unlock_irqrestore(&mbraink->cmd_lock, flags);

	pr_info("[FE]virtio-mbraink send command(id:%d cmd:%d data:%s)\n", cmd->req.id,
			cmd->req.cmd, cmd->req.data);

	return 0;

failed:
	spin_unlock_irqrestore(&mbraink->cmd_lock, flags);
	kfree(cmd);
	return err;
}

static int mbraink_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, NULL, inode->i_private);
}

static ssize_t mbraink_debugfs_write(struct file *file,
					 const char __user *usr_buf,
					 size_t size, loff_t *ppos)
{
	struct virtio_mbraink *priv;
	int rv, val;
	struct seq_file *sfile;

	if (*ppos != 0)
		return -EINVAL;

	rv = kstrtoint_from_user(usr_buf, size, 0, &val);
	if (rv)
		return rv;

	sfile = file->private_data;
	priv = sfile->private;

	switch(val) {
	case VIRTIO_MBRAINK_CMD_ECHO_SYNC:
		rv = virtio_mbraink_echo_sync_test(priv);
		break;
	case VIRTIO_MBRAINK_CMD_ECHO_ASYNC:
		rv = virtio_mbraink_echo_async_test(priv, virtio_mbraink_async_callback);
		break;
	case VIRTIO_MBRAINK_CMD_START_EVENT:
		rv = virtio_mbraink_start_event_test(priv);
		break;
	default:
		rv = -EINVAL;
	}

	if (rv)
		return rv;

	return size;
}

static const struct file_operations mbraink_debugfs_ops = {
	.owner = THIS_MODULE,
	.open = mbraink_debugfs_open,
	.write = mbraink_debugfs_write,
};

struct virtio_mbraink *g_mbraink;
static int virtio_mbraink_probe(struct virtio_device *vdev)
{
	vq_callback_t *vq_cbs[VIRTIO_MBRAINK_Q_COUNT] = {
			mbraink_cmd_done,
			mbraink_evt_done
	};
	const char *names[VIRTIO_MBRAINK_Q_COUNT] = { "command", "event" };
	//struct virtio_mbraink *mbraink;
	int err = -EINVAL;

	g_mbraink = kzalloc(sizeof(struct virtio_mbraink), GFP_KERNEL);
	if (!g_mbraink)
		return -ENOMEM;

	g_mbraink->vdev = vdev;
	vdev->priv = g_mbraink;
	spin_lock_init(&g_mbraink->cmd_lock);
	spin_lock_init(&g_mbraink->evt_lock);
	g_mbraink->event_count = 0;
	g_mbraink->cmd_tree = RB_ROOT;

	err = virtio_find_vqs(vdev, VIRTIO_MBRAINK_Q_COUNT, g_mbraink->vqs, vq_cbs, names,
			      NULL);
	if (err)
		goto err;

	virtio_device_ready(vdev);
	fill_mbraink_evt(g_mbraink);

	g_mbraink->dbg_file = debugfs_create_file("virtio-mbrain", 0644, NULL, g_mbraink,
					    &mbraink_debugfs_ops);

	dev_info(&vdev->dev, "virtio-mbraink initialized\n");

	return 0;

err:
	dev_info(&vdev->dev, "virtio-mbraink probe failed:%d\n", err);

	if (g_mbraink->vdev)
		vdev->config->del_vqs(g_mbraink->vdev);
	kfree(g_mbraink);
	return err;
}

static void virtio_mbraink_remove(struct virtio_device *vdev)
{
	struct virtio_mbraink *mbraink = vdev->priv;
	struct rb_node *n;
	struct virtio_mbraink_cmd *cmd;
	unsigned long flags;

	vdev->config->reset(vdev);
	vdev->vringh_config->del_vrhs(mbraink->vdev);
	vdev->config->del_vqs(mbraink->vdev);
	debugfs_remove_recursive(mbraink->dbg_file);

	spin_lock_irqsave(&mbraink->cmd_lock, flags);
	while((n = rb_first(&mbraink->cmd_tree)) != NULL) {
		cmd = container_of(n, struct virtio_mbraink_cmd, node);
		rb_erase(n, &mbraink->cmd_tree);
		kfree(cmd);
	}
	spin_unlock_irqrestore(&mbraink->cmd_lock, flags);

	kfree(mbraink);
}

static struct virtio_driver virtio_mbraink_driver = {
	.feature_table = features,
	.feature_table_size = ARRAY_SIZE(features),
	.driver.name = KBUILD_MODNAME,
	.driver.owner = THIS_MODULE,
	.id_table = id_table,
	.probe = virtio_mbraink_probe,
	.remove = virtio_mbraink_remove,
};

int virtio_mbraink_init(void)
{
	int err;

	err = register_virtio_driver(&virtio_mbraink_driver);
	if (err < 0) {
		pr_info("Error %d registering virtio mbraink driver\n",
		       err);
		goto unregister;
	}

	return 0;

unregister:
	unregister_virtio_driver(&virtio_mbraink_driver);
	return err;
}

void virtio_mbraink_deinit(void)
{
	unregister_virtio_driver(&virtio_mbraink_driver);
	//kfree(g_mbraink);
}

static int virtio_mbraink_echo_sync(struct virtio_mbraink *mbraink, char *virtio_send_buffer)
{
	int err;

	struct virtio_mbraink_cmd *cmd = kzalloc(sizeof(struct virtio_mbraink_cmd),
			GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	cmd->req.cmd = VIRTIO_MBRAINK_CMD_ECHO_SYNC;
	strscpy((char *)cmd->req.data, virtio_send_buffer, sizeof(cmd->req.data));
	init_completion(&cmd->done);

	err = virtio_mbraink_queue_cmd_sync(mbraink, cmd);
	if (err)
		goto failed;

	wait_for_completion(&cmd->done);

	kfree(cmd);

	return 0;

failed:
	kfree(cmd);
	return err;
}

long send_string_to_host(char *virtio_send_buffer)
{
	long ret = 0;
	int rv = 0;

	rv = virtio_mbraink_echo_sync(g_mbraink, virtio_send_buffer);

	return ret;
}
