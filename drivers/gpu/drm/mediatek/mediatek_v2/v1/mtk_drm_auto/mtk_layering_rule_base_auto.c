// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/string.h>
#include <linux/mm.h>
//#include <mt-plat/mtk_chip.h>
#if IS_ENABLED(CONFIG_COMPAT)
#include <linux/compat.h>
#include <drm/drm_file.h>
#include <drm/drm_ioctl.h>
#endif
#include <drm/drm_modes.h>
#include <drm/drm_property.h>
#ifdef CONFIG_MTK_DCS
#include <mt-plat/mtk_meminfo.h>
#endif
#include "mtk_layering_rule.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_plane.h"
#include "mtk_drm_assert.h"
#include "mtk_log.h"
#include "mtk_drm_mmp.h"
#define CREATE_TRACE_POINTS
#include "mtk_drm_gem.h"
#include "mtk_dsi.h"

#include "../mml/mtk-mml.h"
#include "../mml/mtk-mml-color.h"
#include "../mml/mtk-mml-drm-adaptor.h"
#include "mtk_disp_oddmr/mtk_disp_oddmr.h"
#include "mtk_disp_dbi_count.h"

#include <linux/module.h>

static struct layering_rule_info_t *l_rule_info;
static struct layering_rule_ops *l_rule_ops;

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
#include "mtk_drm_crtc_auto.h"
#define DISP_EXDMA_LAYER_LIMIT 7

int mtk_lye_get_exdma_comp_id_auto(int disp_idx, int layer_idx,
			     struct drm_device *drm_dev, int fun_lye, int rsz_lye)
{
	struct mtk_drm_private *priv = drm_dev->dev_private;
	struct drm_crtc *crtc = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_ddp_comp *exdma_comp;
	int exdma_comp_id = 0;
	u32 local_index = 0;

	if (disp_idx < 0 || disp_idx >= MAX_CRTC) {
		DDPMSG("%s: invalid disp idx %d\n", __func__, disp_idx);
		return -EINVAL;
	}

	crtc = priv->crtc[disp_idx];
	mtk_crtc = to_mtk_crtc(crtc);

	if (layer_idx < (DISP_EXDMA_LAYER_LIMIT + fun_lye))
		local_index = layer_idx - fun_lye;
	else
		local_index = layer_idx - DISP_EXDMA_LAYER_LIMIT - fun_lye;

	exdma_comp = mtk_crtc_get_comp_with_index(mtk_crtc, local_index);
	if (exdma_comp)
		exdma_comp_id = exdma_comp->id;
	else
		exdma_comp_id = mtk_crtc->first_exdma->id;

	DDPINFO("%s  %d CRTC %d layer_idx %d fun_lye %d local_index %d exdma_comp %s\n",
		__func__, __LINE__, disp_idx, layer_idx, fun_lye, local_index,
		mtk_dump_comp_str_id(exdma_comp_id));

	return exdma_comp_id;
}

void mtk_register_layering_rule_ops_for_auto(struct layering_rule_ops *ops,
				    struct layering_rule_info_t *info)
{
	l_rule_ops = ops;
	l_rule_info = info;
}

/*when android only have 2 layer, video layer need goto mml for pq*/
/*other layer goes to gpu*/
void clear_layer_for_two_android_layer(struct drm_mtk_layering_info *disp_info,
			struct drm_device *drm_dev)
{
	int i = 0;
	uint16_t l_tb;
	int disp_idx;
	int ovl_num_limit;
	struct drm_mtk_layer_config *c;
	int top = -1;

	if (get_layering_opt(LYE_OPT_SPHRT))
		disp_idx = disp_info->disp_idx;
	else
		disp_idx = 0;

	l_tb = l_rule_ops->get_mapping_table(drm_dev, disp_idx, disp_info->disp_list, DISP_HW_LAYER_TB,
					     MAX_PHY_OVL_CNT);
	ovl_num_limit = mtk_get_phy_layer_limit(l_tb);

	if (ovl_num_limit == 1)
		return;

	if (disp_info->layer_num[0] <= 1)
		return;

	for (i = disp_info->layer_num[0] - 1; i >= 0; i--) {
		c = &disp_info->input_config[0][i];
		if (mtk_has_layer_cap(c, MTK_MML_DISP_DECOUPLE_LAYER) ||
		    mtk_has_layer_cap(c, MTK_MML_DISP_DECOUPLE2_LAYER)) {
			top = i;
			break;
		}
	}

	if (top == -1)
		return;

	if (!mtk_is_gles_layer(disp_info, 0, top))
		return;

	//roll back all to gpu
	disp_info->gles_head[0] = 0;
	disp_info->gles_tail[0] = disp_info->layer_num[0] - 1;

	c = &disp_info->input_config[0][top];
	if (top == disp_info->gles_head[0])
		disp_info->gles_head[0]++;
	else if (top == disp_info->gles_tail[0])
		disp_info->gles_tail[0]--;
	else
		c->layer_caps |= MTK_DISP_CLIENT_CLEAR_LAYER;
}

#endif

