// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_panel.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_encoder.h>
#include <drm/drm_crtc.h>
#include <drm/drm_vblank.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_drv.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/component.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include "mtk_log.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"
#include "mtk_panel_ext.h"
#include "mtk_dp_api.h"
#include "mtk_dsi.h"
#include "mtk_drm_drv.h"
#include "mtk_dump.h"
#include "mtk_drm_plane.h"
#include "mtk_virtio_disp.h"
#include "mtk_drm_ddp_comp_auto.h"
#include "mtk_drm_crtc_auto.h"
#include "mtk_drm_crtc_auto_guest.h"
#include "mtk_fence.h"
#include "mtk_sync.h"


/*=======================================GUEST===================================================*/
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
void mtk_drm_crtc_auto_init_guest (struct mtk_drm_crtc *mtk_crtc,
			    const struct mtk_crtc_path_data *path_data,
			    int pipe)
{
		unsigned int possible_crtcs = 0;
		struct mtk_ddp_comp *output_comp;

		output_comp = mtk_ddp_comp_request_output(mtk_crtc);
		if (output_comp) {
			if (output_comp->id == DDP_COMPONENT_DSI0 ||
				output_comp->id == DDP_COMPONENT_DSI0_VIRTUAL) {
				mtk_crtc->emi_req = true;
				DDPMSG("%s CRTC-%d emi_req %d\n", __func__, pipe, mtk_crtc->emi_req);
			}
			mtk_ddp_comp_io_cmd(output_comp, NULL, GET_DEVICE_TYPE,
				&(mtk_crtc->is_shared_device));
			possible_crtcs = 1 << pipe;
			mtk_ddp_comp_io_cmd(output_comp, NULL, SET_CRTC_ID,
						&possible_crtcs);

			if (output_comp->id == DDP_COMPONENT_DSI2_VIRTUAL) {
				mtk_crtc->virtual_path = true;
				if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params)
					mtk_crtc->offset_x = mtk_crtc->panel_ext->params->crop_width[0];
				else
					DDPMSG("%s CRTC%d panel is not connect\n", __func__, pipe);
			}
		}

		mtk_crtc->is_virtio_path = true;

#ifdef MTK_VIRT_WITH_HOTPLUG
		if (drm_crtc_index(&mtk_crtc->base) == 1)
			mtk_set_hotplug_status(1);
#endif
}

/* restore ovl layer config and set dal layer if any */
static void mtk_crtc_restore_plane_setting_virt(struct mtk_drm_crtc *mtk_crtc)
{
	unsigned int i;
	struct drm_crtc *crtc = &mtk_crtc->base;
	unsigned int plane_index = 0;
	struct mtk_plane_state *plane_state;
	struct drm_plane *plane;
	struct mtk_ddp_comp *comp;

	DDPMSG("%s %d drm crtc%d\n", __func__, __LINE__, drm_crtc_index(crtc));

	for (i = 0; i < mtk_crtc->layer_nr; i++) {
		plane = &mtk_crtc->planes[i].base;

		plane_state = to_mtk_plane_state(plane->state);
		if (plane_state->base.crtc == NULL && plane_state->pending.enable) {
			DDPMSG("%s CRTC NULL but plane%d pending, skip restore\n", __func__, i);
			break;
		}

		plane_index = to_crtc_plane_index(plane_state->base.plane);
		comp = mtk_crtc_get_comp_with_index(mtk_crtc, plane_index);
		if (plane_state->comp_state.comp_id == 0 && comp) {
			plane_state->comp_state.comp_id = comp->id;

			if (comp->id)
				DDPMSG("%s %d CRTC%d plane %d %d comp %s\n",
					__func__, __LINE__,
					drm_crtc_index(crtc),
					plane_state->base.plane->index,
					plane_index,
					mtk_dump_comp_str_id(plane_state->comp_state.comp_id));
		}
	}
}

void mtk_drm_crtc_enable_virtio(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned int crtc_id = drm_crtc_index(crtc);
	struct cmdq_pkt *cmdq_handle;
#ifndef DRM_CMDQ_DISABLE
	struct cmdq_client *client;
#endif
#if IS_ENABLED(CONFIG_MTK_VIRTIO_DISP)
	struct mtk_ddp_comp *output_comp;
	struct virtio_disp_cmd *cmd;
#endif

	if (mtk_crtc->enabled) {
		DDPINFO("crtc%d skip %s\n", crtc_id, __func__);
		return;
	}

	DDPMSG("%s + drm crtc%d layer_nr %d\n", __func__, crtc_id, mtk_crtc->layer_nr);

#if IS_ENABLED(CONFIG_MTK_VIRTIO_DISP)
	if (!mtk_crtc->is_shared_device) {
		DDPMSG("%s + drm crtc%d is not shared device.\n", __func__, crtc_id);
		output_comp = mtk_ddp_comp_request_output(mtk_crtc);
		if (output_comp) {
			cmd = virtio_disp_cmd_create();
			cmd->req.cmd = VIRTIO_DISP_CRTC_ENABLE;
			cmd->req.param.crtc.output_comp_id = mtk_ddp_comp_get_phy_output_comp(output_comp->id);
			cmd->req.param.crtc.enable = 1;
			virtio_disp_cmd_submit(cmd);
			virtio_disp_cmd_destroy(cmd);
		}
	} else
		DDPMSG("%s + drm crtc%d is shared device, skip enable.\n", __func__, crtc_id);
#endif

	mtk_crtc_attach_ddp_comp(crtc, mtk_crtc->ddp_mode, true);

	/* 1. power on mtcmos */
	mtk_drm_top_clk_prepare_enable(crtc);

	/*2 enable all module clk. enable smi clk ref cnt, otherwise android smi can't set ostd*/
	mtk_crtc_ddp_prepare(mtk_crtc);

#ifndef DRM_CMDQ_DISABLE
	/* 3. power on cmdq client */
	client = mtk_crtc->gce_obj.client[CLIENT_CFG];
	cmdq_mbox_enable(client->chan);
#endif
	mtk_crtc_vdisp_ao_config(crtc);

	if (mtk_ddp_comp_get_type(mtk_crtc->first_exdma->id) == MTK_OVL_EXDMA) {
		mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
			mtk_crtc->gce_obj.client[CLIENT_CFG]);

		if (cmdq_handle) {
			mtk_ddp_comp_io_cmd(mtk_crtc->first_exdma, cmdq_handle,
				IRQ_LEVEL_ALL, NULL);
			cmdq_pkt_flush(cmdq_handle);
			cmdq_pkt_destroy(cmdq_handle);
		}
	}

	/* 4. restore OVL setting */
	mtk_crtc_restore_plane_setting_virt(mtk_crtc);

	drm_crtc_vblank_on(crtc);
#if IS_ENABLED(CONFIG_MTK_VIRTIO_DISP)
	if (crtc_id == 1) {
		cmd = virtio_disp_cmd_create();
		cmd->req.cmd = VIRTIO_DISP_CMD_HOTPLUG_STATUS;
		cmd->req.param.event = 0;
		(void)virtio_disp_cmd_submit(cmd);
		virtio_disp_cmd_destroy(cmd);
	}
#endif

	mtk_crtc_set_status(crtc, true);

#ifdef MTK_VIRT_WITH_HOTPLUG
	if (crtc_id == 1)
		mtk_set_hotplug_status(0);
#endif

	DDPINFO("%s - drm crtc%d\n", __func__, crtc_id);
}

void mtk_drm_crtc_disable_virtio(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned int crtc_id = drm_crtc_index(&mtk_crtc->base);
#ifndef DRM_CMDQ_DISABLE
	struct cmdq_client *client;
#endif
	int i = 0, j = 0;
	struct mtk_ddp_comp *comp = NULL;
	struct cmdq_pkt *cmdq_handle = NULL;
#if IS_ENABLED(CONFIG_MTK_VIRTIO_DISP)
	struct mtk_ddp_comp *output_comp;
	struct virtio_disp_cmd *cmd;
#endif

	if (!mtk_crtc->enabled) {
		DDPINFO("drm crtc%d skip %s\n", crtc_id, __func__);
		return;
	}

	mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
		mtk_crtc->gce_obj.client[CLIENT_CFG]);

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		if (mtk_ddp_comp_get_type(comp->id) == MTK_OVL_EXDMA)
			mtk_ddp_comp_stop(comp, cmdq_handle);
	}

	if (cmdq_handle) {
		cmdq_pkt_flush(cmdq_handle);
		cmdq_pkt_destroy(cmdq_handle);
	}

	DDPINFO("%s:%d drm crtc%d+\n", __func__, __LINE__, crtc_id);
	drm_crtc_vblank_off(crtc);
#ifndef DRM_CMDQ_DISABLE
	/* 8. power off cmdq client */
	client = mtk_crtc->gce_obj.client[CLIENT_CFG];
	cmdq_mbox_disable(client->chan);
#endif

	mtk_crtc->qos_ctx->last_hrt_req = 0;
	mtk_crtc->qos_ctx->last_larb_hrt_req = 0;

	/* 9. power off all modules in this CRTC */
	mtk_crtc_ddp_unprepare(mtk_crtc);

	mtk_drm_top_clk_disable_unprepare(crtc);
	mtk_crtc_set_status(crtc, false);

#if IS_ENABLED(CONFIG_MTK_VIRTIO_DISP)
	if (!mtk_crtc->is_shared_device) {
		DDPMSG("%s + drm crtc%d is not shared device.\n", __func__, crtc_id);
		output_comp = mtk_ddp_comp_request_output(mtk_crtc);
		if (output_comp) {
			cmd = virtio_disp_cmd_create();
			cmd->req.cmd = VIRTIO_DISP_CRTC_ENABLE;
			cmd->req.param.crtc.output_comp_id = mtk_ddp_comp_get_phy_output_comp(output_comp->id);
			cmd->req.param.crtc.enable = 0;
			virtio_disp_cmd_submit(cmd);
			virtio_disp_cmd_destroy(cmd);
		}
	} else
		DDPMSG("%s + drm crtc%d is shared device, skip enable. last_larb_hrt_req %d\n",
		       __func__, crtc_id,
		       mtk_crtc->qos_ctx->last_larb_hrt_req);
#endif

#ifdef MTK_VIRT_WITH_HOTPLUG
	if (crtc_id == 1)
		mtk_set_hotplug_status(1);
#endif

	DDPINFO("drm crtc%d %s:%d -\n", crtc_id, __func__, __LINE__);
}

void mtk_drm_crtc_register_irq(struct mtk_drm_private *priv,
			       struct mtk_drm_crtc *mtk_crtc, int pipe)
{
	struct device *dev = NULL;
	struct device_node *node = NULL;
	struct platform_device *pdev = NULL;
	int irq = 0, num_irqs = 0, ret = 0;
	struct mtk_ddp_comp *first_exdma = NULL;

	DDPMSG("%s crtc-%d register irq+\n", __func__, pipe);

	if (!priv) {
		DDPMSG("%s crtc-%d invalid private data\n", __func__, pipe);
		return;
	}

	dev = priv->mmsys_dev;
	if (!dev) {
		DDPMSG("%s crtc-%d invalid mmsys dev\n", __func__, pipe);
		return;
	}

	node = dev->of_node;
	if (!node) {
		DDPMSG("%s crtc-%d invalid mmsys node\n", __func__, pipe);
		return;
	}

	first_exdma = mtk_crtc->first_exdma;
	if (!first_exdma) {
		DDPMSG("%s crtc-%d invalid first_exdma\n", __func__, pipe);
		return;
	}

	if (!first_exdma->funcs || !first_exdma->funcs->irq_handle) {
		DDPMSG("%s crtc-%d invalid irq_handle", __func__, pipe);
		return;
	}

	pdev = of_find_device_by_node(node);
	if (!pdev) {
		DDPMSG("%s crtc-%d invalid pdev %s\n", __func__, pipe, node->full_name);
		of_node_put(node);
		return;
	}

	num_irqs = platform_irq_count(pdev);
	if (!num_irqs) {
		DDPMSG("%s crtc-%d invalid num_irqs %d\n", __func__, pipe, num_irqs);
		return;
	}

	irq = platform_get_irq(pdev, pipe);
	if (irq < 0) {
		DDPMSG("%s crtc-%d get irq fail\n", __func__, pipe);
		return;
	}

	ret = devm_request_irq(first_exdma->dev, irq, first_exdma->funcs->irq_handle,
			   IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev),
			   first_exdma);
	if (ret) {
		DDPMSG("%s crtc-%d devm_request_irq fail: %d\n", __func__, pipe, ret);
		return;
	}

	DDPMSG("%s crtc-%d register irq %d-\n", __func__, pipe, irq);
}

int mtk_drm_get_host_crtc_obj_id(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	unsigned int crtc_obj_id = 0;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct drm_crtc *crtc;
	int ret = 0;
	struct virtio_disp_cmd *cmd;
	struct mtk_ddp_comp *out_comp;

	if (!data) {
		DDPMSG("[E]%s invalid data pointer", __func__);
		return -EINVAL;
	}
	crtc_obj_id = *(unsigned int *)data;

	drm_for_each_crtc(crtc, dev) {
		if (IS_ERR_OR_NULL(crtc))
			continue;
		if (crtc->base.id == crtc_obj_id) {
			mtk_crtc = to_mtk_crtc(crtc);
			break;
		}
	}

	if (!mtk_crtc) {
		DDPMSG("[E]%s there is no crtc-%d\n", __func__, crtc_obj_id);
		*(unsigned int *)data = 0;
		return -EINVAL;
	}

#if IS_ENABLED(CONFIG_MTK_VIRTIO_DISP)
	cmd = virtio_disp_cmd_create();
	if (!cmd) {
		DDPMSG("[E]%s failed to create virtio_disp_cmd\n", __func__);
		return -ENOMEM;
	}

	cmd->req.cmd = VIRTIO_DISP_CMD_CHECK_INDEX;

	out_comp = mtk_ddp_comp_request_output(mtk_crtc);

	if (!out_comp) {
		DDPMSG("[E]%s crtc-%d invalid out comp\n", __func__, crtc_obj_id);
		*(unsigned int *)data = 0;
		virtio_disp_cmd_destroy(cmd);
		return -EINVAL;
	}

	cmd->req.param.panel.output_comp_id = mtk_ddp_comp_get_phy_output_comp(out_comp->id);
	ret = virtio_disp_cmd_submit(cmd);
	if (ret < 0) {
		DDPMSG("[E]%s there is no host for crtc-%d obj_id %d\n",
			__func__, drm_crtc_index(&mtk_crtc->base), crtc_obj_id);
		*(unsigned int *)data = 0;
		virtio_disp_cmd_destroy(cmd);
		return -EINVAL;
	}

	DDPMSG("%s crtc-%d obj_id %d out_comp %d %s get host crtc_id-%d obj_id %d\n",
		__func__, drm_crtc_index(&mtk_crtc->base), crtc_obj_id,
		out_comp->id, mtk_dump_comp_str(out_comp),
		cmd->rsp.param.crtc.crtc_id,
		cmd->rsp.param.crtc.crtc_obj_id);

	*(unsigned int *)data = cmd->rsp.param.crtc.crtc_obj_id;

	virtio_disp_cmd_destroy(cmd);
#endif

	return 0;
}

void mtk_get_fence_from_timeline(unsigned int session_id, unsigned int timeline_id,
	unsigned int index, struct dma_fence **fence)
{
	struct mtk_fence_info *layer_info = NULL;

	layer_info = _disp_sync_get_sync_info(session_id, timeline_id);

	if (layer_info == NULL) {
		DDPPR_ERR("%s:%d layer_info is null\n", __func__, __LINE__);
		return;
	}

	if (layer_info->timeline == NULL)
		return;

	mtk_sync_timeline_to_fence(layer_info->timeline, index, fence);
}

void mtk_crtc_set_rel_fence_to_monitor(struct mtk_drm_private *priv, unsigned int session_id,
	unsigned int timeline_id, unsigned int fence_idx)
{
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_crtc_fence_mon_node *fence_node = NULL;
	uint32_t idx = 0;

	if ((MTK_SESSION_TYPE(session_id) >= MTK_SESSION_PRIMARY) &&
		(MTK_SESSION_TYPE(session_id) < MTK_SESSION_MAX))
		idx = MTK_SESSION_TYPE(session_id) - 1;
	else
		DDPMSG("invalid session id:%d\n", session_id);

	if (idx < MAX_CRTC) {
		mtk_crtc = to_mtk_crtc(priv->crtc[idx]);

		if (!mtk_crtc->enabled) {
			mtk_crtc = to_mtk_crtc(priv->crtc[0]);
			DDPMSG("crtc[%d] not enable\n", idx);
		}
	} else {
		mtk_crtc = to_mtk_crtc(priv->crtc[0]);
		DDPMSG("session id over max crtc\n");
	}

	fence_node = kmalloc(sizeof(struct mtk_crtc_fence_mon_node), GFP_KERNEL);
	if (!fence_node) {
		DDPMSG("%s alloc fence node fail\n", __func__);
		return;
	}

	fence_node->fence_idx = fence_idx;
	fence_node->session_id = session_id;
	fence_node->timeline_id = timeline_id;

	/*dma_fence_get*/
	mtk_get_fence_from_timeline(session_id, timeline_id,
					fence_idx, &fence_node->fence);
	if (fence_node->fence) {
		INIT_LIST_HEAD(&fence_node->link);
		mtk_sync_fence_get(fence_node->fence);
		mutex_lock(&mtk_crtc->layer_fence_lock);
		list_add_tail(&fence_node->link, &mtk_crtc->fence_monitor_head);
		mtk_crtc->layer_f_count++;
		mutex_unlock(&mtk_crtc->layer_fence_lock);

		atomic_set(&mtk_crtc->monitor_event, 1);
		wake_up_interruptible(&mtk_crtc->fence_monitor_wq);
	} else
		kfree(fence_node);
}

void mtk_crtc_set_pf_fence_to_monitor(struct drm_crtc *crtc, unsigned int fence_idx)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned int index = drm_crtc_index(crtc);
	struct mtk_crtc_fence_mon_node *fence_node = NULL;

	fence_node = kmalloc(sizeof(struct mtk_crtc_fence_mon_node), GFP_KERNEL);
	if (!fence_node) {
		DDPMSG("%s alloc fence node fail\n", __func__);
		return;
	}

	fence_node->crtc_id = index;
	fence_node->fence_idx = fence_idx;
	fence_node->session_id = mtk_get_session_id(crtc);
	fence_node->timeline_id =
		mtk_fence_get_present_timeline_id(fence_node->session_id);

	/*dma_fence_get*/
	mtk_get_fence_from_timeline(fence_node->session_id, fence_node->timeline_id,
					fence_idx, &fence_node->fence);
	if (fence_node->fence) {
		INIT_LIST_HEAD(&fence_node->link);
		mtk_sync_fence_get(fence_node->fence);
		mutex_lock(&mtk_crtc->pf_fence_lock);
		list_add_tail(&fence_node->link, &mtk_crtc->pf_fence_monitor_head);
		mtk_crtc->pre_f_count++;
		mutex_unlock(&mtk_crtc->pf_fence_lock);

		atomic_set(&mtk_crtc->pf_monitor_event, 1);
		wake_up_interruptible(&mtk_crtc->pf_fence_monitor_wq);
	} else
		kfree(fence_node);
}

void mtk_unsignaled_fence_info(struct drm_crtc *crtc,
		struct mtk_crtc_fence_mon_node *fence_node, bool is_pf)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_fence_mon_node *node = NULL, *next = NULL;

	if (mtk_crtc && is_pf) {
		unsigned int fence_idx = readl(mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_PRESENT_FENCE(drm_crtc_index(crtc))));
		DDPMSG("hw last rel fence index:%d\n", fence_idx);

		mutex_lock(&mtk_crtc->pf_fence_lock);
		if (!list_empty(&mtk_crtc->pf_fence_monitor_head)) {
			node = list_last_entry(&mtk_crtc->pf_fence_monitor_head,
					struct mtk_crtc_fence_mon_node, link);
		}
		mutex_unlock(&mtk_crtc->pf_fence_lock);
		if (node)
			DDPMSG("latest fence index:crtc[%d], %d, %d\n", node->crtc_id, node->fence_idx,
				node->timeline_id);

		DDPMSG("current fence index:%d\n", fence_node->fence_idx);
	} else if (mtk_crtc) {
		unsigned int last_fence;
		int i;

		for (i = 0; i < mtk_crtc->layer_nr; i++) {
			last_fence = *(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
			   DISP_SLOT_CUR_CONFIG_FENCE(mtk_get_plane_slot_idx(mtk_crtc, i)));
			DDPMSG("hw last rel fence index:[%d]-[%d]\n", i, last_fence);
		}

		mutex_lock(&mtk_crtc->layer_fence_lock);
		if (!list_empty(&mtk_crtc->fence_monitor_head)) {
			list_for_each_entry_safe(node, next, &mtk_crtc->fence_monitor_head, link) {
				if (node)
					DDPMSG("latest fence index: %d, %d, %d\n", node->session_id,
						node->timeline_id, node->fence_idx);
			}
		}
		mutex_unlock(&mtk_crtc->layer_fence_lock);
		DDPMSG("current fence index:%d\n", fence_node->fence_idx);
	}
}

void mtk_fence_timeout_handle(struct drm_crtc *crtc,
	struct mtk_crtc_fence_mon_node *fence_node, unsigned long time, bool is_pf)
{
	int ret = 0;

	ret = mtk_sync_fence_wait_timeout(fence_node->fence, time);
	if (ret < 0) {
		DDPMSG("%s %d release fence\n", __func__, __LINE__);

		/*should release crtc fence & all release fence*/
		if (is_pf) {
			//get last fence info
			mtk_unsignaled_fence_info(crtc, fence_node, true);

			//mtk_release_present_fence(fence_node->session_id, fence_node->fence_idx, ktime_get());
			//mtk_drm_crtc_release_fence(crtc);
		} else {
			mtk_unsignaled_fence_info(crtc, fence_node, false);
			//mtk_drm_crtc_release_fence(crtc);
		}
	#ifdef IF_ZERO /* printf log only */
		/*trigger aee*/
		DDPAEE_FATAL("aee disp fence timeout");
	#endif
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_MBRAIN)
		char key_str[KEY_STR_LENGTH];

		generate_key_str(key_str);

		DDPMSG("%s[%d]Generated key:%s\n", __func__, __LINE__, key_str);

		struct mb_disp_data mb_data = { 0 };

		mb_data.mtk_crtc_id = drm_crtc_index(crtc);
		mb_data.event = DISP_FENCE_TIMEOUT;
		mb_data.req_type = DISP_SCRAP_YOCTO_ANDROID_HYP_TRACE;
		int len = snprintf(mb_data.identity_info, sizeof(mb_data.identity_info),
			"AN-CRTC%d-DISP_FENCE_TIMEOUT-key_%s",
			mb_data.mtk_crtc_id, key_str);
		if (len < 0 || len >=  sizeof(mb_data.identity_info))
			DDPPR_ERR("%s[%d]: snprintf return error", __func__, __LINE__);
		mb_data.identity_info[sizeof(mb_data.identity_info) - 1] = '\0';

		disp_mb_event_trigger(&mb_data);
#endif
	}

	mtk_sync_fence_put(fence_node->fence);
}

int mtk_drm_monitor_pf_fence_thread(void *data)
{
	struct sched_param param = {.sched_priority = 87};
	struct mtk_drm_crtc *mtk_crtc = (struct mtk_drm_crtc *)data;
	struct mtk_crtc_fence_mon_node *node = NULL;
	unsigned long timeout = msecs_to_jiffies(500);

	sched_setscheduler(current, SCHED_RR, &param);

	while (!kthread_should_stop()) {
		if (!mtk_crtc) {
			DDPMSG("[%s] mtk_crtc is null\n", __func__);
			continue;
		}
		wait_event_interruptible(mtk_crtc->pf_fence_monitor_wq,
					 atomic_read(&mtk_crtc->pf_monitor_event));
		atomic_set(&mtk_crtc->pf_monitor_event, 0);

		while (mtk_crtc->pre_f_count > 0) {

			/*start wait fence*/
			mutex_lock(&mtk_crtc->pf_fence_lock);
			mtk_crtc->pre_f_count--;
			if (!list_empty(&mtk_crtc->pf_fence_monitor_head)) {
				node = list_first_entry(&mtk_crtc->pf_fence_monitor_head,
						struct mtk_crtc_fence_mon_node, link);
				if (node)
					list_del(&node->link);
				else
					node = NULL;
			}
			mutex_unlock(&mtk_crtc->pf_fence_lock);

			if (node) {
				DDPDBG("[%s] crtc[%d] wait fence %llu\n", __func__, node->crtc_id, node->fence->seqno);
				mtk_fence_timeout_handle(&mtk_crtc->base, node, timeout, true);
				kfree(node);
				node = NULL;
			}
		}
	}
	return 0;
}

int mtk_drm_monitor_rel_fence_thread(void *data)
{
	struct mtk_crtc_fence_mon_node *node = NULL;
	struct sched_param param = {.sched_priority = 87};
	struct mtk_drm_crtc *mtk_crtc = (struct mtk_drm_crtc *)data;
	unsigned long timeout = msecs_to_jiffies(500);

	sched_setscheduler(current, SCHED_RR, &param);

	while (!kthread_should_stop()) {
		if (!mtk_crtc) {
			DDPMSG("[%s] mtk_crtc is null\n", __func__);
			continue;
		}

		wait_event_interruptible(mtk_crtc->fence_monitor_wq,
					 atomic_read(&mtk_crtc->monitor_event));
		atomic_set(&mtk_crtc->monitor_event, 0);

		while (mtk_crtc->layer_f_count > 0) {

			/*start wait fence*/
			mutex_lock(&mtk_crtc->layer_fence_lock);
			mtk_crtc->layer_f_count--;
			if (!list_empty(&mtk_crtc->fence_monitor_head)) {
				node = list_first_entry(&mtk_crtc->fence_monitor_head,
						struct mtk_crtc_fence_mon_node, link);
				if (node)
					list_del(&node->link);
				else
					node = NULL;
			}
			mutex_unlock(&mtk_crtc->layer_fence_lock);

			if (node) {
				DDPDBG("[%s] s[%d] t[%d] wait fence:%llu\n", __func__, node->session_id,
						node->timeline_id, node->fence->seqno);
				mtk_fence_timeout_handle(&mtk_crtc->base, node, timeout, false);
				kfree(node);
				node = NULL;
			}
		}
	}

	return 0;
}

#endif
