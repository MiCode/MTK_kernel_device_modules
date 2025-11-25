// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2014 MediaTek Inc.
 */

#define pr_fmt(fmt) "virtio-disp: " fmt

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

#include "mtk_virtio_disp.h"

#define VIRTIO_ID_DISP         52 /* virtio for display */

enum {
	VIRTIO_DISP_Q_COMMAND = 0,
	VIRTIO_DISP_Q_EVENT = 1,
	VIRTIO_DISP_Q_COUNT = 2,
};

/**
 * virtio-disp device.
 */
struct virtio_disp {
	struct virtio_device *vdev;
	struct virtqueue *vqs[VIRTIO_DISP_Q_COUNT];
	spinlock_t cmd_lock;
	spinlock_t evt_lock;
	uint32_t cmd_count;
};

struct virtio_disp *g_virt_disp_dev;
mtk_virt_hotplug_cb g_dp_otplug_cb;

#if IS_ENABLED(CONFIG_VIRTIO_CMDQ_DMA_MAP)
dma_addr_t mtk_drm_vbuffer_map(struct sg_table *sgt, uint32_t *idr)
{
	int ret, i;
	struct virtio_disp_cmd *cmd;
	struct scatterlist *sg;
	unsigned int num_entries;
	struct virtio_drm_mem_entry *ents;
	dma_addr_t dma_addr;

	/*set sg into nents*/
	ents = kcalloc(sgt->nents, sizeof(*ents), GFP_KERNEL);
	if (!ents) {
		ret = -ENOMEM;
		return ret;
	}

	/*copy sgt info*/
	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		ents[i].addr = cpu_to_le64(sg_phys(sg));
		ents[i].length = cpu_to_le32(sg->length);
	}

	num_entries = sgt->nents;
	/*submit vbuf to vm*/
	cmd = virtio_disp_cmd_create();
	cmd->req.cmd = VIRTIO_DRM_VBUF_MAP;
	cmd->req.param.vbuf.buf_entries = ents;
	cmd->req.param.vbuf.buf_size = sizeof(*ents) * num_entries;
	cmd->req.param.vbuf.num_ents = cpu_to_le32(num_entries);
	virtio_disp_cmd_submit(cmd);

	/*set sg index to mml obj*/
	*idr = cmd->rsp.param.vbuf.idr;
	dma_addr = (dma_addr_t)cmd->rsp.param.vbuf.dma_addr;

	/*return vbuf*/
	kfree(ents);
	virtio_disp_cmd_destroy(cmd);

	return (dma_addr_t)dma_addr;
}
EXPORT_SYMBOL(mtk_drm_vbuffer_map);

void mtk_drm_vbuffer_unmap(uint32_t idr)
{
	struct virtio_disp_cmd *cmd;

	/*submit vbuf to vm*/
	cmd = virtio_disp_cmd_create();
	cmd->req.cmd = VIRTIO_DRM_VBUF_UNMAP;
	cmd->req.param.vbuf.id = cpu_to_le32(idr);

	virtio_disp_cmd_submit(cmd);
	virtio_disp_cmd_destroy(cmd);
}
EXPORT_SYMBOL(mtk_drm_vbuffer_unmap);
#endif

static void handle_disp_event(struct virtio_disp *disp,
		struct virtio_disp_event *evt)
{
	pr_info("[FE]Received test device event\n");

	if (g_dp_otplug_cb)
		g_dp_otplug_cb(evt->event);
}

/* Called from virtio device, in IRQ context */
static void disp_cmd_done(struct virtqueue *vq)
{
	struct virtio_disp *disp = vq->vdev->priv;
	unsigned long flags;
	unsigned int len;
	struct virtio_disp_cmd *cmd;

	spin_lock_irqsave(&disp->cmd_lock, flags);
	do {
		virtqueue_disable_cb(vq);
		while ((cmd = virtqueue_get_buf(vq, &len)) != NULL)
			complete(&cmd->done);

		if (unlikely(virtqueue_is_broken(vq)))
			break;
	} while (!virtqueue_enable_cb(vq));
	virtqueue_kick(vq);
	spin_unlock_irqrestore(&disp->cmd_lock, flags);

}

static int virtio_disp_queue_evt_buffer(struct virtio_disp *disp,
		struct virtio_disp_event *evt)
{
	int ret;
	struct scatterlist sg;
	struct virtqueue *vq = disp->vqs[VIRTIO_DISP_Q_EVENT];

	sg_init_one(&sg, evt, sizeof(struct virtio_disp_event));

	ret = virtqueue_add_inbuf(vq, &sg, 1, evt, GFP_ATOMIC);

	if (ret)
		return ret;

	virtqueue_kick(vq);

	return 0;
}

/* Called from virtio device, in IRQ context */
static void disp_evt_done(struct virtqueue *vq)
{
	struct virtio_disp *disp = vq->vdev->priv;
	struct virtio_disp_event *evt;
	unsigned long flags;
	unsigned int len;

	spin_lock_irqsave(&disp->evt_lock, flags);
	do {
		virtqueue_disable_cb(vq);
		while ((evt = virtqueue_get_buf(vq, &len)) != NULL) {
			handle_disp_event(disp, evt);
			virtio_disp_queue_evt_buffer(disp, evt);
		}
		if (unlikely(virtqueue_is_broken(vq)))
			break;
	} while (!virtqueue_enable_cb(vq));
	virtqueue_kick(vq);
	spin_unlock_irqrestore(&disp->evt_lock, flags);

}

static int fill_disp_evt(struct virtio_disp *disp)
{
	int i;
	struct virtio_disp_event *evts;
	struct virtqueue *vq = disp->vqs[VIRTIO_DISP_Q_EVENT];
	size_t nr_evts = vq->num_free;
	unsigned long flags;

	evts = kmalloc_array(nr_evts, sizeof(*evts), GFP_KERNEL);
	if (!evts)
		return -ENOMEM;

	spin_lock_irqsave(&disp->evt_lock, flags);
	for (i = 0; i < nr_evts; i++)
		virtio_disp_queue_evt_buffer(disp, &evts[i]);

	virtqueue_kick(vq);
	spin_unlock_irqrestore(&disp->evt_lock, flags);

	return 0;
}

struct virtio_disp_cmd *virtio_disp_cmd_create(void)
{
	struct virtio_disp_cmd *cmd;

	cmd = kzalloc(sizeof(struct virtio_disp_cmd), GFP_KERNEL);
	if (!cmd)
		return ERR_PTR(-ENOMEM);
	return cmd;
}
EXPORT_SYMBOL(virtio_disp_cmd_create);

void virtio_disp_cmd_destroy(struct virtio_disp_cmd *cmd)
{
	kfree(cmd);
}
EXPORT_SYMBOL(virtio_disp_cmd_destroy);

int virtio_disp_cmd_submit(struct virtio_disp_cmd *cmd)
{
	int err;
	struct virtqueue *vq;
	unsigned long flags;
	struct scatterlist req_sgl, res_sgl;
	int outcnt = 0, intcnt = 0;
#if IS_ENABLED(CONFIG_VIRTIO_CMDQ_DMA_MAP)
	struct scatterlist buf_sgl;
	struct scatterlist *sgs[3];
#else
	struct scatterlist *sgs[2];
#endif

	if (g_virt_disp_dev != NULL) {
		vq = g_virt_disp_dev->vqs[VIRTIO_DISP_Q_COMMAND];
	} else {
		pr_info("virtio display probe fail.\n");
		return -EINVAL;
	}
	if (cmd->cb)
		g_dp_otplug_cb = cmd->cb;
	sg_init_one(&req_sgl, &cmd->req, sizeof(cmd->req));
	sgs[outcnt + intcnt] = &req_sgl;
	outcnt++;

#if IS_ENABLED(CONFIG_VIRTIO_CMDQ_DMA_MAP)
	if (cmd->req.cmd == VIRTIO_DRM_VBUF_MAP) {
		sg_init_one(&buf_sgl, cmd->req.param.vbuf.buf_entries, cmd->req.param.vbuf.buf_size);
		sgs[outcnt + intcnt] = &buf_sgl;
		outcnt++;
	}
#endif

	sg_init_one(&res_sgl, &cmd->rsp, sizeof(cmd->rsp));
	sgs[outcnt + intcnt] = &res_sgl;
	intcnt++;

	init_completion(&cmd->done);

	spin_lock_irqsave(&g_virt_disp_dev->cmd_lock, flags);
	cmd->req.id = g_virt_disp_dev->cmd_count++;
	err = virtqueue_add_sgs(vq, sgs, outcnt, intcnt, cmd, GFP_ATOMIC);
	if (err) {
		spin_unlock_irqrestore(&g_virt_disp_dev->cmd_lock, flags);
		return err;
	}
	virtqueue_kick(vq);
	spin_unlock_irqrestore(&g_virt_disp_dev->cmd_lock, flags);

#if IS_ENABLED(CONFIG_VIRTIO_CMDQ_DMA_MAP)
	if (!(cmd->req.cmd == VIRTIO_DRM_VBUF_MAP ||
		cmd->req.cmd == VIRTIO_DRM_VBUF_UNMAP))
		pr_info("[FE]virtio-disp send command(id:%d cmd:%d)\n", cmd->req.id,
				cmd->req.cmd);
#else
	pr_info("%s [FE]virtio-disp send command(id:%d cmd:%d) sizeof rsp.param %lu\n",
		__func__,
		cmd->req.id,
		cmd->req.cmd,
		sizeof(cmd->rsp.param));
#endif

	wait_for_completion(&cmd->done);

	switch (cmd->req.cmd) {
	case VIRTIO_DISP_CMD_GET_PANEL:
		pr_info("%s [FE]virtio-disp received wxh(%dx%d)\n",
			__func__,
			cmd->rsp.param.panel.width,
			cmd->rsp.param.panel.height);
		break;
	case VIRTIO_DISP_CRTC_ENABLE:
		pr_info("%s [FE]virtio-disp crtc%d enable done\n",
			__func__,
			cmd->rsp.param.crtc.crtc_id);
		break;
	case VIRTIO_DISP_CMD_GET_CRTC_INFO:
		pr_info("%s [FE]virtio-disp an crtc_nr %d\n",
			__func__,
			cmd->rsp.param.path_info.crtc_nr);
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL(virtio_disp_cmd_submit);

static int virtio_disp_probe(struct virtio_device *vdev)
{
	struct virtio_disp *disp;
	int err = -EINVAL;
	struct virtqueue_info vq_config[VIRTIO_DISP_Q_COUNT] = {
		{
			.callback = disp_cmd_done,
			.name = "command",
			.ctx = false,
		},
		{
			.callback = disp_evt_done,
			.name = "event",
			.ctx = false,
		},
	};

	disp = kzalloc(sizeof(*disp), GFP_KERNEL);
	if (!disp)
		return -ENOMEM;

	disp->vdev = vdev;
	vdev->priv = disp;
	spin_lock_init(&disp->cmd_lock);
	spin_lock_init(&disp->evt_lock);

	err = virtio_find_vqs(vdev, VIRTIO_DISP_Q_COUNT, disp->vqs, vq_config, NULL);
	if (err)
		goto err;

	virtio_device_ready(vdev);
	fill_disp_evt(disp);

	g_virt_disp_dev = disp;
	dev_info(&vdev->dev, "virtio-disp initialized\n");

	return 0;
err:
	dev_info(&vdev->dev, "virtio-disp probe failed:%d\n", err);

	if (disp->vdev)
		vdev->config->del_vqs(disp->vdev);
	kfree(disp);
	return err;
}

static void virtio_disp_remove(struct virtio_device *vdev)
{
	struct virtio_disp *disp = vdev->priv;

	vdev->config->reset(vdev);
	vdev->vringh_config->del_vrhs(disp->vdev);
	vdev->config->del_vqs(disp->vdev);

	kfree(disp);
}

static struct virtio_device_id id_table[] = {
	{ VIRTIO_ID_DISP, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static unsigned int features[] = {};

static struct virtio_driver virtio_disp_driver = {
	.feature_table = features,
	.feature_table_size = ARRAY_SIZE(features),
	.driver.name = KBUILD_MODNAME,
	.driver.owner = THIS_MODULE,
	.id_table = id_table,
	.probe = virtio_disp_probe,
	.remove = virtio_disp_remove,
};

static int virtio_disp_init(void)
{
	int ret = 0;

	ret = register_virtio_driver(&virtio_disp_driver);

	return ret;
}

subsys_initcall(virtio_disp_init);

MODULE_DEVICE_TABLE(virtio, id_table);
MODULE_DESCRIPTION("virtio disp driver");
MODULE_LICENSE("GPL");
