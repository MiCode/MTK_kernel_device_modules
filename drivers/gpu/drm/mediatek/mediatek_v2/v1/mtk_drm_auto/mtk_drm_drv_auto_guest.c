// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <drm/drm_crtc.h>
#include <drm/drm_vblank.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_plane.h>
#include <linux/videodev2.h>

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO)
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_log.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_plane.h"
#include "mtk_drm_gem.h"

#include "mtk_drm_ddp_comp_auto.h"
#include "mtk_drm_drv_auto.h"
#include "mtk_drm_crtc_auto.h"
#include "mtk_drm_mmp.h"
#include "mtk_virtio_disp.h"

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
static atomic_t top_isr_ref; /* irq power status protection */
static atomic_t top_clk_ref; /* top clk status protection*/
static spinlock_t top_clk_lock; /* power status protection*/

int mtk_drm_pm_ctl_auto_guest(struct mtk_drm_private *priv, enum disp_pm_action action)
{
	int ret = 0;

	DDPINFO("pm ctrl :action: %d\n", action);
	return ret;
}

void mtk_drm_get_pwr_clk_auto_guest(struct mtk_drm_private *priv)
{
	struct device *dev = priv->mmsys_dev;
	struct device_node *pwr_node;
	struct pwr_clk_map *clk_map;
	int i;

	pwr_node = of_parse_phandle(dev->of_node, "pwr-handle", 0);
	if (!pwr_node) {
		DDPMSG("No pwr-handle node\n");
		priv->pwr_node = NULL;
		return;
	}

	priv->pwr_node = pwr_node;
	clk_map = priv->data->pwr_clk_map;
	if (!clk_map) {
		DDPMSG("%s No clk_map\n", __func__);
		return;
	}
	// Get clocks from pwr_dev
	for (i = 0; clk_map[i].name != NULL; i++) {
		priv->pwr_clks[clk_map[i].id] = of_clk_get_by_name(pwr_node, clk_map[i].name);
		if (IS_ERR(priv->pwr_clks[clk_map[i].id])) {
			DDPMSG("No %s clock in pwr dev\n", clk_map[i].name);
			priv->pwr_clks[clk_map[i].id] = NULL;
		} else
			DDPMSG("get %s id: %d\n", clk_map[i].name, clk_map[i].id);
	}
}

void mtk_drm_get_top_clk_auto_guest(struct mtk_drm_private *priv)
{
	struct device *dev = priv->mmsys_dev;
	struct device_node *node = dev->of_node;
	int clk_num;

	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL) {
		spin_lock_init(&top_clk_lock);
		/* TODO: check display enable from lk */
		atomic_set(&top_isr_ref, 0);
		atomic_set(&top_clk_ref, 1);
		priv->power_state = true;
	}

	clk_num = of_count_phandle_with_args(node, "clocks", "#clock-cells");
	if (clk_num == -ENOENT) {
		priv->top_clk_num = -1;
		priv->top_clk = NULL;
		return;
	}
	priv->top_clk_num = clk_num;

	DDPFUNC("first pm_get\n");
	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL) {
		spin_lock_init(&top_clk_lock);
		/* TODO: check display enable from lk */
		atomic_set(&top_isr_ref, 0);
		atomic_set(&top_clk_ref, 1);
		priv->power_state = true;
	}
}

void mtk_drm_top_clk_prepare_enable_auto_guest(struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	bool en = 1;
	unsigned long flags = 0;

	if (priv->top_clk_num <= 0)
		return;

	DDPMSG("%s top_clk_num %d\n", __func__, priv->top_clk_num);

	spin_lock_irqsave(&top_clk_lock, flags);
	atomic_inc(&top_clk_ref);
	if (atomic_read(&top_clk_ref) == 1) {
		DDPFENCE("%s:%d power_state = true\n", __func__, __LINE__);
		priv->power_state = true;
	}

	spin_unlock_irqrestore(&top_clk_lock, flags);

	DRM_MMP_MARK(top_clk, atomic_read(&top_clk_ref),
			atomic_read(&top_isr_ref));
	if (priv->data->sodi_config)
		priv->data->sodi_config(crtc->dev, DDP_COMPONENT_ID_MAX, NULL, &en);

	if (priv->data->disable_merge_irq)
		priv->data->disable_merge_irq(crtc->dev);
}

void mtk_drm_top_clk_disable_unprepare_auto_guest(struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	int cnt = 0;
	unsigned long flags = 0;

	if (priv->top_clk_num <= 0)
		return;

	//set_swpm_disp_active(false);
	spin_lock_irqsave(&top_clk_lock, flags);
	atomic_dec(&top_clk_ref);
	if (atomic_read(&top_clk_ref) == 0) {
		while (atomic_read(&top_isr_ref) > 0 &&
		       cnt++ < 10) {
			spin_unlock_irqrestore(&top_clk_lock, flags);
			pr_notice("%s waiting for isr job, %d\n",
				  __func__, cnt);
			usleep_range(20, 40);
			spin_lock_irqsave(&top_clk_lock, flags);
		}
		priv->power_state = false;
	}
	spin_unlock_irqrestore(&top_clk_lock, flags);

	DDPMSG("%s top_clk_num %d\n", __func__, priv->top_clk_num);

	DRM_MMP_MARK(top_clk, atomic_read(&top_clk_ref),
			atomic_read(&top_isr_ref));
}

bool mtk_drm_top_clk_isr_get_auto_guest(struct mtk_ddp_comp *comp)
{
	unsigned long flags = 0;

	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL) {
		spin_lock_irqsave(&top_clk_lock, flags);
		if (atomic_read(&top_clk_ref) <= 0) {
			DDPMSG("%s, top clk off at %s\n",
				  __func__, mtk_dump_comp_str(comp));
			spin_unlock_irqrestore(&top_clk_lock, flags);
			return false;
		}
		atomic_inc(&top_isr_ref);
		spin_unlock_irqrestore(&top_clk_lock, flags);
	}
	return true;
}

void mtk_drm_top_clk_isr_put_auto_guest(struct mtk_ddp_comp *comp)
{
	unsigned long flags = 0;

	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL) {

		spin_lock_irqsave(&top_clk_lock, flags);

		/* when timeout of polling isr ref in unpreare top clk*/
		if (atomic_read(&top_clk_ref) <= 0) {
			DDPMSG("%s, top clk off at %s\n",
				  __func__,  mtk_dump_comp_str(comp));
			spin_unlock_irqrestore(&top_clk_lock, flags);
			return;
		}
		atomic_dec(&top_isr_ref);
		spin_unlock_irqrestore(&top_clk_lock, flags);
	}
}

#if IS_ENABLED(CONFIG_DRM_PATH_CONFIG_FROM_DTS)
static void mtk_drm_path_update_output_comp(u32 crtc_id, struct mtk_crtc_path_data *crtc_path_data,
					struct virtio_disp_an_crtc_path_data *an_crtc_path_data)
{
	int i = 0, ddp_mode = DDP_MAJOR, ddp_path = DDP_FIRST_PATH;
	enum mtk_ddp_comp_id comp_id;

	enum mtk_ddp_comp_id output_comp = an_crtc_path_data->output_comp;

	if (!mtk_ddp_comp_is_output_by_id(output_comp)) {
		DDPMSG("[E]%s crtc-%d invalid output comp %s\n",
		       __func__, crtc_id, mtk_dump_comp_str_id(output_comp));
		return;
	}

	DDPMSG("%s crtc-%d output_comp %d %s\n", __func__, crtc_id, output_comp,
		mtk_dump_comp_str_id(output_comp));

	for_each_comp_id_target_mode_path_in_disp_path_data(comp_id, crtc_path_data, i, ddp_mode,
							    ddp_path) {
		if (mtk_ddp_comp_is_output_by_id(comp_id)) {
			DDPMSG("%s crtc-%d update output comp %s -> %s.\n",
			       __func__, crtc_id, mtk_dump_comp_str_id(comp_id),
			       mtk_dump_comp_str_id(output_comp));
			crtc_path_data->path[ddp_mode][ddp_path][i] = output_comp;
		}
	}
}

static void mtk_drm_path_update_ovlsys_data_impl(u32 crtc_id,
					struct mtk_crtc_path_data *crtc_path_data,
					struct virtio_disp_an_crtc_path_data *an_crtc_path_data,
					bool dual_ovl)
{
	enum mtk_ddp_comp_id *ovl_path = NULL;
	int i = 0;
	u32 *an_ovl_path;
	u32 an_ovl_path_len;

	if (dual_ovl) {
		an_ovl_path = an_crtc_path_data->dual_ovl_path;
		an_ovl_path_len = an_crtc_path_data->dual_ovl_path_len;
	} else {
		an_ovl_path = an_crtc_path_data->ovl_path;
		an_ovl_path_len = an_crtc_path_data->ovl_path_len;
	}

	if (!an_ovl_path || !an_ovl_path_len) {
		DDPMSG("%s crtc-%d dual_ovl %d invalid an_ovl_path %p or an_ovl_path_len %d\n",
			__func__, crtc_id, dual_ovl, an_ovl_path, an_ovl_path_len);
		return;
	}

	DDPMSG("%s crtc-%d dual_ovl %d ovl_path_len %d\n",
		__func__, crtc_id, dual_ovl, an_ovl_path_len);

	ovl_path = kcalloc(an_ovl_path_len, sizeof(enum mtk_ddp_comp_id), GFP_KERNEL);
	if (!ovl_path) {
		DDPMSG("%s data path kcalloc fail, crtc-%d\n", __func__, crtc_id);
		return;
	}

	for (i = 0; i < an_ovl_path_len; i++) {
		ovl_path[i] = an_ovl_path[i];
		DDPMSG("%s crtc-%d %s\n", __func__, crtc_id, mtk_dump_comp_str_id(ovl_path[i]));
	}

	if (dual_ovl) {
		crtc_path_data->dual_ovl_path[0] = ovl_path;
		crtc_path_data->dual_ovl_path_len[0] = an_ovl_path_len;
	} else {
		crtc_path_data->ovl_path[0][0] = ovl_path;
		crtc_path_data->ovl_path_len[0][0] = an_ovl_path_len;
	}
}

static void mtk_drm_path_update_ovlsys_data(u32 crtc_id,
					struct mtk_crtc_path_data *crtc_path_data,
					struct virtio_disp_an_crtc_path_data *an_crtc_path_data)
{
	mtk_drm_path_update_ovlsys_data_impl(crtc_id, crtc_path_data, an_crtc_path_data, false);

	if (an_crtc_path_data->dual_ovl_enable)
		mtk_drm_path_update_ovlsys_data_impl(crtc_id, crtc_path_data, an_crtc_path_data,
						     true);
}

static void mtk_drm_path_update_crtc_prop(u32 crtc_id,
					struct mtk_crtc_path_data *crtc_path_data,
					struct virtio_disp_an_crtc_path_data *an_crtc_path_data)
{
	if (an_crtc_path_data->is_shared_device)
		crtc_path_data->is_shared_device = true;

	crtc_path_data->host_crtc_id = an_crtc_path_data->host_crtc_id;

	if (an_crtc_path_data->dual_ovl_enable)
		crtc_path_data->is_dual_ovl = true;

	DDPMSG("%s crtc-%d host_crtc_id %d is_shared_device %d dual_ovl %d\n",
		__func__, crtc_id, crtc_path_data->host_crtc_id,
		crtc_path_data->is_shared_device,
		crtc_path_data->is_dual_ovl);
}
#endif

int mtk_drm_path_data_update_from_host(struct mtk_drm_private *private)
{
	struct virtio_disp_cmd *cmd;
	int i, ret = 0;
	struct mtk_crtc_path_data *crtc_path_data;
	struct virtio_disp_rsp_crtc_path_info *path_info;
	struct virtio_disp_an_crtc_path_data *an_crtc_path_data;

#if IS_ENABLED(CONFIG_DRM_PATH_CONFIG_FROM_DTS)
	cmd = virtio_disp_cmd_create();
	if (IS_ERR(cmd)) {
		DDPMSG("[E] %s vitio cmd create fail %ld!\n", __func__, PTR_ERR(cmd));
		return PTR_ERR(cmd);
	}

	cmd->req.cmd = VIRTIO_DISP_CMD_GET_CRTC_INFO;
	ret = virtio_disp_cmd_submit(cmd);
	if (ret) {
		DDPMSG("[E] %s get crtc path data info fail.!\n", __func__);
		virtio_disp_cmd_destroy(cmd);
		return ret;
	}

	path_info = &cmd->rsp.param.path_info;
	if (!path_info->crtc_nr || path_info->crtc_nr > MAX_CRTC) {
		DDPMSG("[E] %s crtc_nr %d MAX_CRTC %d invalid.!\n",
			__func__, path_info->crtc_nr, MAX_CRTC);
		virtio_disp_cmd_destroy(cmd);
		return -EINVAL;
	}

	for (i = 0; i < path_info->crtc_nr && i < MAX_CRTC; i++) {
		crtc_path_data = mtk_disp_crtc_path_data[i];
		an_crtc_path_data = &path_info->crtc_path_data[i];

		if (!crtc_path_data)
			continue;

		if (i == 2) {
			/* writeback */
			crtc_path_data->is_path_enable = true;
			crtc_path_data->host_crtc_id = 2;
			continue;
		}

		if (!an_crtc_path_data->ovl_path_len) {
			crtc_path_data->is_path_enable = false;
			continue;
		}

		crtc_path_data->is_path_enable = true;

		mtk_drm_path_update_crtc_prop(i, crtc_path_data, an_crtc_path_data);

		mtk_drm_path_update_ovlsys_data(i, crtc_path_data, an_crtc_path_data);

		mtk_drm_path_update_output_comp(i, crtc_path_data, an_crtc_path_data);
	}

	virtio_disp_cmd_destroy(cmd);
#else
	for (i = 0; i < MAX_CRTC; i++) {
		switch (i) {
		case 0:
			mtk_disp_crtc_path_data[i] =
				(struct mtk_crtc_path_data *)private->data->main_path_data;
			break;
		case 1:
			mtk_disp_crtc_path_data[i] =
				(struct mtk_crtc_path_data *)private->data->ext_path_data;
			break;
		case 2:
			mtk_disp_crtc_path_data[i] =
				(struct mtk_crtc_path_data *)private->data->third_path_data;
			break;
		case 3:
			mtk_disp_crtc_path_data[i] =
				(struct mtk_crtc_path_data *)private->data->fourth_path_data_discrete;
			break;
		case 4:
			mtk_disp_crtc_path_data[i] =
				(struct mtk_crtc_path_data *)private->data->fifth_path_data;
			break;
		case 5:
			mtk_disp_crtc_path_data[i] =
				(struct mtk_crtc_path_data *)private->data->sixth_path_data;
			break;
		case 6:
			mtk_disp_crtc_path_data[i] =
				(struct mtk_crtc_path_data *)private->data->seventh_path_data;
			break;
		default:
			break;
		}
	}
#endif
	return ret;
}

int mtk_drm_path_find_crtc(struct mtk_ddp_comp *comp)
{
	struct mtk_crtc_path_data *crtc_path_data;
	int i;

	for (i = 0; i < MAX_CRTC; i++) {
		crtc_path_data = mtk_disp_crtc_path_data[i];

		if (!crtc_path_data)
			continue;

		if (!crtc_path_data->is_path_enable)
			continue;

		if (mtk_drm_find_comp(comp, crtc_path_data)) {
			DDPMSG("%s crtc_nr %d comp %s!\n",
				__func__, i, mtk_dump_comp_str_id(comp->id));
			return i;
		}
	}

	return -EINVAL;
}

bool mtk_drm_path_is_shared_device(struct mtk_ddp_comp *comp)
{
	struct mtk_crtc_path_data *crtc_path_data;
	int i;

	for (i = 0; i < MAX_CRTC; i++) {
		crtc_path_data = mtk_disp_crtc_path_data[i];

		if (!crtc_path_data || !crtc_path_data->is_path_enable)
			continue;

		if (mtk_drm_find_comp(comp, crtc_path_data)) {
			DDPMSG("%s crtc_nr %d comp %s is_shared_device %d!\n",
				__func__, i, mtk_dump_comp_str_id(comp->id),
				crtc_path_data->is_shared_device);
			return crtc_path_data->is_shared_device;
		}
	}

	return false;
}
#endif

#endif
