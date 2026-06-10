// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "virtio-tpd: " fmt

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
#include <linux/ktime.h>
#include <linux/timekeeping.h>

#if IS_ENABLED(CONFIG_TOUCHSCREEN_VHOST_TOUCH_ENABLE)
#include "vhost_mbrain.h"
#endif

#include "tpd_hyp_virtio.h"
#include "tpd.h"

/* Guest driver can echo "tpdworld" message to device. */
#define VIRTIO_TPD_F_ECHO_MSG  0
#define VIRTIO_ID_TPD           56

#if IS_ENABLED(CONFIG_TOUCHSCREEN_VHOST_TOUCH_ENABLE)
static int last_mbrain_index = 1;
static int last_mbrain_index_err_cnt = 1;
static int last_mbrain_index_cnt = 100;
#endif

static unsigned int features[] = {
	VIRTIO_TPD_F_ECHO_MSG,
};

#define VIRTIO_SUBSYSTEM_ID_MTK_TOUCH     106
#define PCI_SUBVENDOR_ID_MTK             0xf0ff
static struct virtio_device_id id_table[] = {
	{ VIRTIO_SUBSYSTEM_ID_MTK_TOUCH, PCI_SUBVENDOR_ID_MTK },
	{ 0 },
};

char test_buff[MAX_VIRTIO_SEND_BYTE_TMP] = {0};
enum {
	VIRTIO_TPD_Q_COMMAND = 0,
	VIRTIO_TPD_Q_EVENT = 1,
	VIRTIO_TPD_Q_COUNT = 2,
};

enum {
	VIRTIO_TPD_CMD_ECHO_SYNC = 0,
	VIRTIO_TPD_CMD_ECHO_ASYNC = 1,
	VIRTIO_TPD_CMD_START_EVENT = 2,
};

enum {
	VIRTIO_TPD_EVENT_TYPE_NORMAL = 0,
	VIRTIO_TPD_EVENT_TYPE_CB = 1,
};

/**
 * Request, Response, and Event protocol for virtio-tpd device.
 */
struct virtio_tpd_req {
	uint32_t id;
	uint32_t cmd;
	uint8_t data[MAX_VIRTIO_SEND_BYTE];
};
struct virtio_tpd_rsp {
	uint32_t rc;
	uint8_t data[MAX_VIRTIO_SEND_BYTE];
};
struct virtio_tpd_event {
	uint32_t id;
	uint32_t type;
	uint32_t cmd_id;
	union {
		uint8_t data[MAX_VIRTIO_SEND_BYTE];
		struct virtio_tpd_rsp rsp;
	};
};

#if IS_ENABLED(CONFIG_TOUCHSCREEN_VHOST_TOUCH_ENABLE)
struct touch_mbrain_work_data {
	struct work_struct work;
	unsigned long flag;
	unsigned long latency;
};

static struct touch_mbrain_work_data touch_mbrain_work;
#endif

/**
 * virtio-tpd device.
 */
struct virtio_tpd {
	struct virtio_device *vdev;
	struct virtqueue *vqs[VIRTIO_TPD_Q_COUNT];
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
typedef void tpd_callback_t(struct virtio_tpd_rsp *rsp);

/**
 * Command for virtio-tpd device.
 */
struct virtio_tpd_cmd {
	struct virtio_tpd_req req;
	struct virtio_tpd_rsp rsp;
	struct completion done;
	struct rb_node node;
	tpd_callback_t *cb;
};

#if IS_ENABLED(CONFIG_TOUCHSCREEN_VHOST_TOUCH_ENABLE)
static BLOCKING_NOTIFIER_HEAD(touch_mb_notifier_list);

int touch_mb_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&touch_mb_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(touch_mb_register);

int touch_mb_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&touch_mb_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(touch_mb_unregister);

int touch_mb_event_trigger(unsigned long event_type, void *latency)
{
	return blocking_notifier_call_chain(&touch_mb_notifier_list, event_type, latency);
}

static void handle_touch_mbarin(struct work_struct *work)
{
	struct touch_mbrain_work_data *handle_work_data = container_of(work,
						struct touch_mbrain_work_data, work);
	touch_mb_event_trigger(handle_work_data->flag, (void *)handle_work_data->latency);
}
#endif

static bool cmd_less(struct rb_node *node, const struct rb_node *parent)
{
	struct virtio_tpd_cmd *cmd = container_of(node, struct virtio_tpd_cmd,
			node);
	struct virtio_tpd_cmd *cmd_parent = container_of(node,
			struct virtio_tpd_cmd, node);
	return cmd->req.id < cmd_parent->req.id;
}

static int cmd_cmp(const void *key, const struct rb_node *node)
{
	uint32_t id = *(uint32_t *)key;
	struct virtio_tpd_cmd *cmd = container_of(node, struct virtio_tpd_cmd,
				node);
	if (id < cmd->req.id)
		return -1;
	else if (id > cmd->req.id)
		return 1;
	else
		return 0;
}

static void handle_cmd_callbak(struct virtio_tpd *tpd,
		struct virtio_tpd_event *evt)
{
	struct rb_node *node;
	struct virtio_tpd_cmd *cmd;
	unsigned long flags;

	spin_lock_irqsave(&tpd->cmd_lock, flags);
	node = rb_find(&evt->cmd_id, &tpd->cmd_tree, cmd_cmp);
	if (node)
		rb_erase(node, &tpd->cmd_tree);

	spin_unlock_irqrestore(&tpd->cmd_lock, flags);


	if (node) {
		cmd = container_of(node, struct virtio_tpd_cmd, node);
		if (cmd->cb)
			cmd->cb(&evt->rsp);

		kfree(cmd);
	}
}

static void handle_touch_event(int flag, int X,int Y, int H, int W, int id,int touch_type )
{
	if( flag == 0 )
		touch_virtul_release(touch_type,id);
	else
		touch_virtul(flag,X,Y,H,W,id,touch_type);

}

#if IS_ENABLED(CONFIG_TOUCHSCREEN_VHOST_TOUCH_ENABLE)
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
#endif

static void handle_tpd_event(struct virtio_tpd *tpd,
		struct virtio_tpd_event *evt, unsigned int len)
{
	int x = 0, y = 0, flag = 0, h =0, w =0,id =0,touch_type=0, mbrain_index = 0;
	int yocto_time = 0;
	int ret;
	unsigned int actuall_len = 0;
#if IS_ENABLED(CONFIG_TOUCHSCREEN_VHOST_TOUCH_ENABLE)
	int latency =0;
#endif
	if (evt == NULL) {
		pr_info("failed the evt is NULL .\n");
		return;
	}

	actuall_len = sizeof(evt->data);
	switch (evt->type) {
	case VIRTIO_TPD_EVENT_TYPE_NORMAL:
		if (evt->data[actuall_len-1] != '\0') {
			pr_info("event error data.\n");
			return;
		}
		ret = sscanf(evt->data, "%d %d %d %d %d %d %d %d %d",&flag, &x,
				&y,&h,&w,&id, &touch_type,&mbrain_index,&yocto_time);
		if (ret != 9) {
			pr_info("failed to parse all arguments.\n");
			return;
		}
#if IS_ENABLED(CONFIG_TOUCHSCREEN_VHOST_TOUCH_ENABLE)
		/* when latency > 5 or drop 3 points within 100 points triger mbrain */
		latency = get_real_time() - yocto_time;
		if(latency > 5) {
			touch_mbrain_work.flag = 1;
			touch_mbrain_work.latency = latency;
			schedule_work(&touch_mbrain_work.work);
		}
		last_mbrain_index_cnt--;
		if((mbrain_index - last_mbrain_index) > 1 &&
				(last_mbrain_index -mbrain_index ) != 9) {
			last_mbrain_index_err_cnt++;
			if (last_mbrain_index_err_cnt > 3) {
				touch_mbrain_work.flag = 2;
				touch_mbrain_work.latency = 0;
				schedule_work(&touch_mbrain_work.work);
			}
		}
		if(last_mbrain_index_cnt <= 0) {
			last_mbrain_index_cnt = 100;
			last_mbrain_index_err_cnt = 1;
		}
		last_mbrain_index = mbrain_index;
#endif
		handle_touch_event(flag, x, y,h,w,id,touch_type);
		break;
	case VIRTIO_TPD_EVENT_TYPE_CB:
		handle_cmd_callbak(tpd, evt);
		break;
	default:
		pr_info(" not support type.\n");
	}
}

/* Called from virtio device, in IRQ context */
static void tpd_cmd_done(struct virtqueue *vq)
{
	struct virtio_tpd *tpd = vq->vdev->priv;
	unsigned long flags;
	unsigned int len;
	struct virtio_tpd_cmd *cmd;

	spin_lock_irqsave(&tpd->cmd_lock, flags);
	do {
		virtqueue_disable_cb(vq);
		while ((cmd = virtqueue_get_buf(vq, &len)) != NULL)
			complete(&cmd->done);

		if (unlikely(virtqueue_is_broken(vq)))
			break;
	} while (!virtqueue_enable_cb(vq));
	virtqueue_kick(vq);
	spin_unlock_irqrestore(&tpd->cmd_lock, flags);

}

static void virtio_tpd_queue_evt_buffer(struct virtio_tpd *tpd,
		struct virtio_tpd_event *evt)
{
	if (!tpd || !evt)
		return;

	struct virtqueue *vq = tpd->vqs[VIRTIO_TPD_Q_EVENT];
	struct scatterlist sg[1];

	sg_init_one(sg, evt, sizeof(*evt));
	evt->id = tpd->event_count++;
	virtqueue_add_inbuf(vq, sg, 1, evt, GFP_ATOMIC);
}

/* Called from virtio device, in IRQ context */
static void tpd_evt_done(struct virtqueue *vq)
{
	struct virtio_tpd *tpd = vq->vdev->priv;
	struct virtio_tpd_event *evt;
	unsigned long flags;
	unsigned int len;

	spin_lock_irqsave(&tpd->evt_lock, flags);
	do {
		virtqueue_disable_cb(vq);
		while ((evt = virtqueue_get_buf(vq, &len)) != NULL) {
			handle_tpd_event(tpd, evt, len);
			virtio_tpd_queue_evt_buffer(tpd, evt);
		}
		if (unlikely(virtqueue_is_broken(vq)))
			break;
	} while (!virtqueue_enable_cb(vq));
	virtqueue_kick(vq);
	spin_unlock_irqrestore(&tpd->evt_lock, flags);

}

static int fill_tpd_evt(struct virtio_tpd *tpd)
{
	int i;
	struct virtio_tpd_event *evts;
	struct virtqueue *vq = tpd->vqs[VIRTIO_TPD_Q_EVENT];
	size_t nr_evts = vq->num_free;
	unsigned long flags;

	evts = kmalloc_array(nr_evts, sizeof(*evts), GFP_KERNEL);
	if (!evts)
		return -ENOMEM;

	spin_lock_irqsave(&tpd->evt_lock, flags);
	for (i = 0; i < nr_evts; i++)
		virtio_tpd_queue_evt_buffer(tpd, &evts[i]);

	virtqueue_kick(vq);
	spin_unlock_irqrestore(&tpd->evt_lock, flags);
	//kfree(evts);
	return 0;
}

static int virtio_tpd_queue_cmd_sync(struct virtio_tpd *tpd,
		struct virtio_tpd_cmd *cmd)
{
	int err;
	struct virtqueue *vq = tpd->vqs[VIRTIO_TPD_Q_COMMAND];
	unsigned long flags;
	struct scatterlist req;
	struct scatterlist rsp;
	struct scatterlist *sgs[2];

	sg_init_one(&req, &cmd->req, sizeof(cmd->req));
	sg_init_one(&rsp, &cmd->rsp, sizeof(cmd->rsp));
	sgs[0] = &req;
	sgs[1] = &rsp;

	spin_lock_irqsave(&tpd->cmd_lock, flags);
	cmd->req.id = tpd->cmd_count++;
	err = virtqueue_add_sgs(vq, sgs, 1, 1, cmd, GFP_ATOMIC);
	if (err)
		goto failed;

	virtqueue_kick(vq);
	spin_unlock_irqrestore(&tpd->cmd_lock, flags);

	return 0;

failed:
	spin_unlock_irqrestore(&tpd->cmd_lock, flags);
	return err;
}

static int virtio_tpd_echo_sync_test(struct virtio_tpd *tpd)
{
	int err;
	struct virtio_tpd_cmd *cmd = kzalloc(sizeof(struct virtio_tpd_cmd),
			GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	cmd->req.cmd = VIRTIO_TPD_CMD_ECHO_SYNC;
	strscpy((char *)cmd->req.data, "Hello world!", sizeof(cmd->req.data));
	init_completion(&cmd->done);

	err = virtio_tpd_queue_cmd_sync(tpd, cmd);
	if (err)
		goto failed;

	wait_for_completion(&cmd->done);
	kfree(cmd);

	return 0;

failed:
	kfree(cmd);
	return err;
}

static int virtio_tpd_start_event_test(struct virtio_tpd *tpd)
{
	int err;

	struct virtio_tpd_cmd *cmd = kzalloc(sizeof(struct virtio_tpd_cmd),
			GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	cmd->req.cmd = VIRTIO_TPD_CMD_START_EVENT;
	init_completion(&cmd->done);
	err = virtio_tpd_queue_cmd_sync(tpd, cmd);
	if (err)
		goto failed;

	wait_for_completion(&cmd->done);
	kfree(cmd);
	return 0;

failed:
	kfree(cmd);
	return err;
}

void virtio_tpd_async_callback(struct virtio_tpd_rsp *rsp)
{
	pr_info("[FE]virtio-tpd callback received response(rc:%d data:%s)\n", rsp->rc, rsp->data);
}

static int virtio_tpd_echo_async_test(struct virtio_tpd *tpd,
		tpd_callback_t *cb)
{
	int err;
	struct virtqueue *vq = tpd->vqs[VIRTIO_TPD_Q_COMMAND];
	unsigned long flags;
	struct scatterlist req;
	struct scatterlist *sgs[1];
	struct virtio_tpd_cmd *cmd;

	cmd = kzalloc(sizeof(struct virtio_tpd_cmd), GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	sg_init_one(&req, &cmd->req, sizeof(cmd->req));
	sgs[0] = &req;

	cmd->req.cmd = VIRTIO_TPD_CMD_ECHO_ASYNC;
	strscpy(test_buff, "Hello world, test for tpd send data!", sizeof(test_buff));
	strscpy((char *)cmd->req.data, test_buff, sizeof(cmd->req.data));
	init_completion(&cmd->done);
	cmd->cb = cb;

	spin_lock_irqsave(&tpd->cmd_lock, flags);
	cmd->req.id = tpd->cmd_count++;
	err = virtqueue_add_sgs(vq, sgs, 1, 0, cmd, GFP_ATOMIC);
	if (err)
		goto failed;

	pr_info("ghq_client: %s: %d\n", __func__, __LINE__);
	rb_add(&cmd->node, &tpd->cmd_tree, cmd_less);
	virtqueue_kick(vq);
	spin_unlock_irqrestore(&tpd->cmd_lock, flags);

	return 0;

failed:
	spin_unlock_irqrestore(&tpd->cmd_lock, flags);
	kfree(cmd);
	return err;
}

static int tpd_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, NULL, inode->i_private);
}

static ssize_t tpd_debugfs_write(struct file *file,
					 const char __user *usr_buf,
					 size_t size, loff_t *ppos)
{
	struct virtio_tpd *priv;
	int rv, val;
	struct seq_file *sfile;

	if (*ppos != 0)
		return -EINVAL;

	rv = kstrtoint_from_user(usr_buf, size, 0, &val);
	if (rv)
		return rv;

	pr_info("ghq_client: %s: %d\n", __func__, __LINE__);
	sfile = file->private_data;
	priv = sfile->private;

	switch(val) {
	case VIRTIO_TPD_CMD_ECHO_SYNC:
		rv = virtio_tpd_echo_sync_test(priv);
		break;
	case VIRTIO_TPD_CMD_ECHO_ASYNC:
		rv = virtio_tpd_echo_async_test(priv, virtio_tpd_async_callback);
		break;
	case VIRTIO_TPD_CMD_START_EVENT:
		rv = virtio_tpd_start_event_test(priv);
		break;
	default:
		rv = -EINVAL;
	}

	if (rv)
		return rv;

	return size;
}

static const struct file_operations tpd_debugfs_ops = {
	.owner = THIS_MODULE,
	.open = tpd_debugfs_open,
	.write = tpd_debugfs_write,
};

struct virtio_tpd *g_tpd;
static int virtio_tpd_probe(struct virtio_device *vdev)
{
	struct virtqueue_info vq_cbs[VIRTIO_TPD_Q_COUNT] = {
		{
			.callback = tpd_cmd_done,
			.name = "command",
			.ctx = false,
		},
		{
			.callback = tpd_evt_done,
			.name = "event",
			.ctx = false,
		},
	};
	int err = -EINVAL;

	g_tpd = kmalloc(sizeof(struct virtio_tpd), GFP_KERNEL);
	if (!g_tpd)
		return -ENOMEM;

	g_tpd->vdev = vdev;
	vdev->priv = g_tpd;
	spin_lock_init(&g_tpd->cmd_lock);
	spin_lock_init(&g_tpd->evt_lock);
	g_tpd->event_count = 0;
	g_tpd->cmd_tree = RB_ROOT;

	err = virtio_find_vqs(vdev, VIRTIO_TPD_Q_COUNT, g_tpd->vqs, vq_cbs, NULL);
	if (err)
		goto err;

	virtio_device_ready(vdev);
	fill_tpd_evt(g_tpd);

	g_tpd->dbg_file = debugfs_create_file("virtio-touch", 0220, NULL, g_tpd,
					    &tpd_debugfs_ops);

	dev_info(&vdev->dev, "virtio-tpd initialized\n");
	return 0;
err:
	dev_info(&vdev->dev, "virtio-tpd probe failed:%d\n", err);

	if (g_tpd->vdev)
		vdev->config->del_vqs(g_tpd->vdev);
	kfree(g_tpd);
	return err;
}

static void virtio_tpd_remove(struct virtio_device *vdev)
{
	struct virtio_tpd *tpd = vdev->priv;
	struct rb_node *n;
	struct virtio_tpd_cmd *cmd;
	unsigned long flags;

	vdev->config->reset(vdev);
	vdev->vringh_config->del_vrhs(tpd->vdev);
	vdev->config->del_vqs(tpd->vdev);
	debugfs_remove_recursive(tpd->dbg_file);
	spin_lock_irqsave(&tpd->cmd_lock, flags);
	while((n = rb_first(&tpd->cmd_tree)) != NULL) {
		cmd = container_of(n, struct virtio_tpd_cmd, node);
		rb_erase(n, &tpd->cmd_tree);
		kfree(cmd);
	}
	spin_unlock_irqrestore(&tpd->cmd_lock, flags);

	kfree(tpd);
}

static struct virtio_driver virtio_tpd_driver = {
	.feature_table = features,
	.feature_table_size = ARRAY_SIZE(features),
	.driver.name = KBUILD_MODNAME,
	.driver.owner = THIS_MODULE,
	.id_table = id_table,
	.probe = virtio_tpd_probe,
	.remove = virtio_tpd_remove,
};

int virtio_tpd_init(void)
{
	int err;

#if IS_ENABLED(CONFIG_TOUCHSCREEN_VHOST_TOUCH_ENABLE)
	INIT_WORK(&touch_mbrain_work.work, handle_touch_mbarin);
#endif
	err = register_virtio_driver(&virtio_tpd_driver);
	if (err < 0) {
		pr_info("Error %d registering virtio tpd driver\n",
		       err);
		goto unregister;
	}

	return 0;
unregister:
	unregister_virtio_driver(&virtio_tpd_driver);
	return err;
}

void virtio_tpd_deinit(void)
{
#if IS_ENABLED(CONFIG_TOUCHSCREEN_VHOST_TOUCH_ENABLE)
	cancel_work_sync(&touch_mbrain_work.work);
#endif
	unregister_virtio_driver(&virtio_tpd_driver);
}

static int virtio_tpd_echo_sync(struct virtio_tpd *tpd, char *virtio_send_buffer)
{
	int err;

	struct virtio_tpd_cmd *cmd = kzalloc(sizeof(struct virtio_tpd_cmd),
			GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	cmd->req.cmd = VIRTIO_TPD_CMD_ECHO_SYNC;
	// strncpy((char *)cmd->req.data, virtio_send_buffer, MAX_VIRTIO_SEND_BYTE);
	strscpy((char *)cmd->req.data, virtio_send_buffer, MAX_VIRTIO_SEND_BYTE);
	init_completion(&cmd->done);

	err = virtio_tpd_queue_cmd_sync(tpd, cmd);
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

	virtio_tpd_echo_sync(g_tpd, virtio_send_buffer);
	return ret;
}

long send_string_to_host_test (void)
{
	long ret = 0;

	virtio_tpd_echo_sync_test(g_tpd);
	return ret;
}



//module_init(virtio_tpd_init);
//module_exit(virtio_tpd_fini);

//MODULE_DESCRIPTION("VIRTIO Sample Driver");
//MODULE_LICENSE("GPL v2");
