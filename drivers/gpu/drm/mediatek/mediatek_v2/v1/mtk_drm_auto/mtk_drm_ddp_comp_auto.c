// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"
#include "mtk_dump.h"
#include "mtk_drm_ddp.h"
#include "mtk_drm_ddp_comp_auto.h"
#include <dt-bindings/disp/ddp_comp.h>


static u32 mtk_ddp_comp_map[DDP_COMP_NUM] = {

	[DDP_COMP_DISP1_DP_INTF0] = DDP_COMPONENT_DP_INTF0,
	[DDP_COMP_DISP1_DP_INTF1] = DDP_COMPONENT_DP_INTF1,

	[DDP_COMP_DISP1_DSC0] = DDP_COMPONENT_DSC0,
	[DDP_COMP_DISP1_DSC1] = DDP_COMPONENT_DSC1,
	[DDP_COMP_DISP1_DSC2] = DDP_COMPONENT_DSC2,
	[DDP_COMP_DISP1_DSC3] = DDP_COMPONENT_DSC3,

	[DDP_COMP_DISP1_DSI0] = DDP_COMPONENT_DSI0,
	[DDP_COMP_DISP1_DSI1] = DDP_COMPONENT_DSI1,
	[DDP_COMP_DISP1_DSI2] = DDP_COMPONENT_DSI2,
	[DDP_COMP_DISP1_DVO] = DDP_COMPONENT_DISP_DVO,

	[DDP_COMP_DISP1_DSI0_VIRT] = DDP_COMPONENT_DSI0_VIRTUAL,
	[DDP_COMP_DISP1_DSI1_VIRT] = DDP_COMPONENT_DSI1_VIRTUAL,
	[DDP_COMP_DISP1_DSI2_VIRT] = DDP_COMPONENT_DSI2_VIRTUAL,
	[DDP_COMP_DISP1_DPI0_VIRT] = DDP_COMPONENT_DPI0_VIRTUAL,
	[DDP_COMP_DISP1_DPI1_VIRT] = DDP_COMPONENT_DPI1_VIRTUAL,
	[DDP_COMP_DISP1_DVO0_VIRT] = DDP_COMPONENT_DVO_VIRTUAL,

//	[DDP_COMP_DISP1_DSI2_1_VIRT] = DDP_COMPONENT_DSI2_1_VIRTUAL,

	[DDP_COMP_OVL0_EXDMA2] = DDP_COMPONENT_OVL_EXDMA2,
	[DDP_COMP_OVL0_EXDMA3] = DDP_COMPONENT_OVL_EXDMA3,
	[DDP_COMP_OVL0_EXDMA4] = DDP_COMPONENT_OVL_EXDMA4,
	[DDP_COMP_OVL0_EXDMA5] = DDP_COMPONENT_OVL_EXDMA5,
	[DDP_COMP_OVL0_EXDMA6] = DDP_COMPONENT_OVL_EXDMA6,
	[DDP_COMP_OVL0_EXDMA7] = DDP_COMPONENT_OVL_EXDMA7,
	[DDP_COMP_OVL0_EXDMA8] = DDP_COMPONENT_OVL_EXDMA8,
	[DDP_COMP_OVL0_EXDMA9] = DDP_COMPONENT_OVL_EXDMA9,

	[DDP_COMP_OVL0_BLENDER0] = DDP_COMPONENT_OVL0_BLENDER0,
	[DDP_COMP_OVL0_BLENDER1] = DDP_COMPONENT_OVL0_BLENDER1,
	[DDP_COMP_OVL0_BLENDER2] = DDP_COMPONENT_OVL0_BLENDER2,
	[DDP_COMP_OVL0_BLENDER3] = DDP_COMPONENT_OVL0_BLENDER3,
	[DDP_COMP_OVL0_BLENDER4] = DDP_COMPONENT_OVL0_BLENDER4,
	[DDP_COMP_OVL0_BLENDER5] = DDP_COMPONENT_OVL0_BLENDER5,
	[DDP_COMP_OVL0_BLENDER6] = DDP_COMPONENT_OVL0_BLENDER6,
	[DDP_COMP_OVL0_BLENDER7] = DDP_COMPONENT_OVL0_BLENDER7,
	[DDP_COMP_OVL0_BLENDER8] = DDP_COMPONENT_OVL0_BLENDER8,
	[DDP_COMP_OVL0_BLENDER9] = DDP_COMPONENT_OVL0_BLENDER9,

	[DDP_COMP_OVL0_OUTPROC0] = DDP_COMPONENT_OVL0_OUTPROC0,
	[DDP_COMP_OVL0_OUTPROC1] = DDP_COMPONENT_OVL0_OUTPROC1,
	[DDP_COMP_OVL0_OUTPROC2] = DDP_COMPONENT_OVL0_OUTPROC2,
	[DDP_COMP_OVL0_OUTPROC3] = DDP_COMPONENT_OVL0_OUTPROC3,
	[DDP_COMP_OVL0_OUTPROC4] = DDP_COMPONENT_OVL0_OUTPROC4,
	[DDP_COMP_OVL0_OUTPROC5] = DDP_COMPONENT_OVL0_OUTPROC5,

	[DDP_COMP_OVL0_DLI_ASYNC0] = DDP_COMPONENT_OVLSYS_DLI_ASYNC0,
	[DDP_COMP_OVL0_DLI_ASYNC1] = DDP_COMPONENT_OVLSYS_DLI_ASYNC1,
	[DDP_COMP_OVL0_DLI_ASYNC2] = DDP_COMPONENT_OVLSYS_DLI_ASYNC2,
	[DDP_COMP_OVL0_DLI_ASYNC3] = DDP_COMPONENT_OVLSYS_DLI_ASYNC3,
	[DDP_COMP_OVL0_DLI_ASYNC4] = DDP_COMPONENT_OVLSYS_DLI_ASYNC4,
	[DDP_COMP_OVL0_DLI_ASYNC5] = DDP_COMPONENT_OVLSYS_DLI_ASYNC5,
	[DDP_COMP_OVL0_DLI_ASYNC6] = DDP_COMPONENT_OVLSYS_DLI_ASYNC6,
	[DDP_COMP_OVL0_DLI_ASYNC7] = DDP_COMPONENT_OVLSYS_DLI_ASYNC7,
	[DDP_COMP_OVL0_DLI_ASYNC8] = DDP_COMPONENT_OVLSYS_DLI_ASYNC8,

	[DDP_COMP_OVL0_DLO_ASYNC0] = DDP_COMPONENT_OVLSYS_DLO_ASYNC0,
	[DDP_COMP_OVL0_DLO_ASYNC1] = DDP_COMPONENT_OVLSYS_DLO_ASYNC1,
	[DDP_COMP_OVL0_DLO_ASYNC2] = DDP_COMPONENT_OVLSYS_DLO_ASYNC2,
	[DDP_COMP_OVL0_DLO_ASYNC3] = DDP_COMPONENT_OVLSYS_DLO_ASYNC3,
	[DDP_COMP_OVL0_DLO_ASYNC4] = DDP_COMPONENT_OVLSYS_DLO_ASYNC4,
	[DDP_COMP_OVL0_DLO_ASYNC5] = DDP_COMPONENT_OVLSYS_DLO_ASYNC5,
	[DDP_COMP_OVL0_DLO_ASYNC6] = DDP_COMPONENT_OVLSYS_DLO_ASYNC6,
	[DDP_COMP_OVL0_DLO_ASYNC7] = DDP_COMPONENT_OVLSYS_DLO_ASYNC7,
	[DDP_COMP_OVL0_DLO_ASYNC8] = DDP_COMPONENT_OVLSYS_DLO_ASYNC8,
	[DDP_COMP_OVL0_DLO_ASYNC9] = DDP_COMPONENT_OVLSYS_DLO_ASYNC9,
	[DDP_COMP_OVL0_DLO_ASYNC10] = DDP_COMPONENT_OVLSYS_DLO_ASYNC10,
	[DDP_COMP_OVL0_DLO_ASYNC11] = DDP_COMPONENT_OVLSYS_DLO_ASYNC11,
	[DDP_COMP_OVL0_DLO_ASYNC12] = DDP_COMPONENT_OVLSYS_DLO_ASYNC12,

	[DDP_COMP_OVL1_EXDMA2] = DDP_COMPONENT_OVL1_EXDMA2,
	[DDP_COMP_OVL1_EXDMA3] = DDP_COMPONENT_OVL1_EXDMA3,
	[DDP_COMP_OVL1_EXDMA4] = DDP_COMPONENT_OVL1_EXDMA4,
	[DDP_COMP_OVL1_EXDMA5] = DDP_COMPONENT_OVL1_EXDMA5,
	[DDP_COMP_OVL1_EXDMA6] = DDP_COMPONENT_OVL1_EXDMA6,
	[DDP_COMP_OVL1_EXDMA7] = DDP_COMPONENT_OVL1_EXDMA7,
	[DDP_COMP_OVL1_EXDMA8] = DDP_COMPONENT_OVL1_EXDMA8,
	[DDP_COMP_OVL1_EXDMA9] = DDP_COMPONENT_OVL1_EXDMA9,

	[DDP_COMP_OVL1_BLENDER0] = DDP_COMPONENT_OVL1_BLENDER0,
	[DDP_COMP_OVL1_BLENDER1] = DDP_COMPONENT_OVL1_BLENDER1,
	[DDP_COMP_OVL1_BLENDER2] = DDP_COMPONENT_OVL1_BLENDER2,
	[DDP_COMP_OVL1_BLENDER3] = DDP_COMPONENT_OVL1_BLENDER3,
	[DDP_COMP_OVL1_BLENDER4] = DDP_COMPONENT_OVL1_BLENDER4,
	[DDP_COMP_OVL1_BLENDER5] = DDP_COMPONENT_OVL1_BLENDER5,
	[DDP_COMP_OVL1_BLENDER6] = DDP_COMPONENT_OVL1_BLENDER6,
	[DDP_COMP_OVL1_BLENDER7] = DDP_COMPONENT_OVL1_BLENDER7,
	[DDP_COMP_OVL1_BLENDER8] = DDP_COMPONENT_OVL1_BLENDER8,
	[DDP_COMP_OVL1_BLENDER9] = DDP_COMPONENT_OVL1_BLENDER9,
	[DDP_COMP_OVL1_OUTPROC0] = DDP_COMPONENT_OVL1_OUTPROC0,

	[DDP_COMP_OVL1_OUTPROC1] = DDP_COMPONENT_OVL1_OUTPROC1,
	[DDP_COMP_OVL1_OUTPROC2] = DDP_COMPONENT_OVL1_OUTPROC2,
	[DDP_COMP_OVL1_OUTPROC3] = DDP_COMPONENT_OVL1_OUTPROC3,
	[DDP_COMP_OVL1_OUTPROC4] = DDP_COMPONENT_OVL1_OUTPROC4,
	[DDP_COMP_OVL1_OUTPROC5] = DDP_COMPONENT_OVL1_OUTPROC5,

	[DDP_COMP_OVL1_DLI_ASYNC0] = DDP_COMPONENT_OVLSYS1_DLI_ASYNC0,
	[DDP_COMP_OVL1_DLI_ASYNC1] = DDP_COMPONENT_OVLSYS1_DLI_ASYNC1,
	[DDP_COMP_OVL1_DLI_ASYNC2] = DDP_COMPONENT_OVLSYS1_DLI_ASYNC2,
	[DDP_COMP_OVL1_DLI_ASYNC3] = DDP_COMPONENT_OVLSYS1_DLI_ASYNC3,
	[DDP_COMP_OVL1_DLI_ASYNC4] = DDP_COMPONENT_OVLSYS1_DLI_ASYNC4,
	[DDP_COMP_OVL1_DLI_ASYNC5] = DDP_COMPONENT_OVLSYS1_DLI_ASYNC5,
	[DDP_COMP_OVL1_DLI_ASYNC6] = DDP_COMPONENT_OVLSYS1_DLI_ASYNC6,
	[DDP_COMP_OVL1_DLI_ASYNC7] = DDP_COMPONENT_OVLSYS1_DLI_ASYNC7,
	[DDP_COMP_OVL1_DLI_ASYNC8] = DDP_COMPONENT_OVLSYS1_DLI_ASYNC8,

	[DDP_COMP_OVL1_DLO_ASYNC0] = DDP_COMPONENT_OVLSYS1_DLO_ASYNC0,
	[DDP_COMP_OVL1_DLO_ASYNC1] = DDP_COMPONENT_OVLSYS1_DLO_ASYNC1,
	[DDP_COMP_OVL1_DLO_ASYNC2] = DDP_COMPONENT_OVLSYS1_DLO_ASYNC2,
	[DDP_COMP_OVL1_DLO_ASYNC3] = DDP_COMPONENT_OVLSYS1_DLO_ASYNC3,
	[DDP_COMP_OVL1_DLO_ASYNC4] = DDP_COMPONENT_OVLSYS1_DLO_ASYNC4,
	[DDP_COMP_OVL1_DLO_ASYNC5] = DDP_COMPONENT_OVLSYS1_DLO_ASYNC5,
	[DDP_COMP_OVL1_DLO_ASYNC6] = DDP_COMPONENT_OVLSYS1_DLO_ASYNC6,
	[DDP_COMP_OVL1_DLO_ASYNC7] = DDP_COMPONENT_OVLSYS1_DLO_ASYNC7,
	[DDP_COMP_OVL1_DLO_ASYNC8] = DDP_COMPONENT_OVLSYS1_DLO_ASYNC8,
	[DDP_COMP_OVL1_DLO_ASYNC9] = DDP_COMPONENT_OVLSYS1_DLO_ASYNC9,
	[DDP_COMP_OVL1_DLO_ASYNC10] = DDP_COMPONENT_OVLSYS1_DLO_ASYNC10,
	[DDP_COMP_OVL1_DLO_ASYNC11] = DDP_COMPONENT_OVLSYS1_DLO_ASYNC11,
	[DDP_COMP_OVL1_DLO_ASYNC12] = DDP_COMPONENT_OVLSYS1_DLO_ASYNC12,
};


static enum mtk_ddp_comp_id mtk_ddp_virt_comp_map[] = {
	DDP_COMPONENT_DSI0_VIRTUAL, DDP_COMPONENT_DSI0,
	DDP_COMPONENT_DSI1_VIRTUAL, DDP_COMPONENT_DSI1,
	DDP_COMPONENT_DSI2_VIRTUAL, DDP_COMPONENT_DSI2,
	DDP_COMPONENT_DSI2_1_VIRTUAL, DDP_COMPONENT_DSI2_VIRTUAL,
	DDP_COMPONENT_DVO_VIRTUAL, DDP_COMPONENT_DISP_DVO,
	DDP_COMPONENT_DPI0_VIRTUAL, DDP_COMPONENT_DP_INTF0,
	DDP_COMPONENT_DPI1_VIRTUAL, DDP_COMPONENT_DP_INTF1,
};

bool mtk_ddp_comp_check_output_comp(enum mtk_ddp_comp_id virt_id,
				    enum mtk_ddp_comp_id phy_id)
{
	int i = 0;
	int map_size = ARRAY_SIZE(mtk_ddp_virt_comp_map);
	enum mtk_ddp_comp_id *comp_map = mtk_ddp_virt_comp_map;

	for (i = 0; i < map_size; i += 2) {
		if ((virt_id == comp_map[i]) && (phy_id == comp_map[i + 1]))
			return true;
	}

	return false;
}

enum mtk_ddp_comp_id mtk_ddp_comp_get_virt_output_comp(enum mtk_ddp_comp_id phy_id)
{
	int i = 0;
	int map_size = ARRAY_SIZE(mtk_ddp_virt_comp_map);
	enum mtk_ddp_comp_id *comp_map = mtk_ddp_virt_comp_map;

	for (i = 0; i < map_size; i += 2) {
		if (phy_id == comp_map[i + 1])
			return comp_map[i];
	}

	return 0;
}

enum mtk_ddp_comp_id mtk_ddp_comp_get_phy_output_comp(enum mtk_ddp_comp_id virt_id)
{
	int i = 0;
	int map_size = ARRAY_SIZE(mtk_ddp_virt_comp_map);
	enum mtk_ddp_comp_id *comp_map = mtk_ddp_virt_comp_map;

	for (i = 0; i < map_size; i += 2) {
		if (virt_id == comp_map[i])
			return comp_map[i + 1];
	}

	return 0;
}

bool mtk_ddp_comp_is_rdma(struct mtk_ddp_comp *comp)
{
	if (mtk_ddp_comp_get_type(comp->id) == MTK_OVL_EXDMA)
		return true;

	return false;
}

bool mtk_ddp_comp_is_rdma_by_id(enum mtk_ddp_comp_id id)
{
	if (mtk_ddp_comp_get_type(id) == MTK_OVL_EXDMA)
		return true;

	return false;
}

bool mtk_ddp_comp_is_share_comp(struct mtk_ddp_comp *comp)
{
	if (comp->id < 0 || comp->id >= DDP_COMPONENT_ID_MAX)
		return false;

	return (comp->is_virt_comp == SHARE_EXDMA) ? 1 : 0;
}

bool mtk_ddp_comp_is_virt(struct mtk_ddp_comp *comp)
{
	if (comp->id < 0 || comp->id >= DDP_COMPONENT_ID_MAX)
		return false;

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
	return false;
#endif

	return (comp->is_virt_comp == GUEST_EXDMA) ? 1 : 0;
}

int mtk_ddp_comp_is_layer_on(struct mtk_ddp_comp *comp)
{
	if (comp && comp->funcs && comp->funcs->is_layer_on)
		return comp->funcs->is_layer_on(comp);
	else
		return 0;
}

bool mtk_ddp_comp_is_virt_by_id(struct mtk_drm_private *private,
	enum mtk_ddp_comp_id id)
{
	if (id >= DDP_COMPONENT_ID_MAX)
		return false;

	if (private->ddp_comp[id])
		return (private->ddp_comp[id]->is_virt_comp == GUEST_EXDMA) ? 1 : 0;
	else
		return false;
}

enum mtk_ddp_comp_id mtk_ddp_comp_get_map_id(u32 comp_id)
{
	enum mtk_ddp_comp_id ddp_comp_id;

	if (comp_id >= DDP_COMP_NUM) {
		DDPMSG("%s comp_id %d\n", __func__, comp_id);
		return DDP_COMPONENT_ID_MAX;
	}

	ddp_comp_id = mtk_ddp_comp_map[comp_id];

	DDPMSG("%s comp_id %d %d %s\n",
		__func__, comp_id, ddp_comp_id, mtk_dump_comp_str_id(ddp_comp_id));

	return ddp_comp_id;
}

bool mtk_ddp_comp_is_comp_out_cb_by_id(enum mtk_ddp_comp_id id)
{
	if (id >= DDP_COMPONENT_ID_MAX)
		return false;

	if ((id == DDP_COMPONENT_COMP0_OUT_CB6) ||
	    (id == DDP_COMPONENT_COMP0_OUT_CB7) ||
	    (id == DDP_COMPONENT_COMP0_OUT_CB8) ||
	    (id == DDP_COMPONENT_COMP0_OUT_CB9) ||
	    (id == DDP_COMPONENT_COMP0_OUT_CB10) ||
	    (id == DDP_COMPONENT_COMP0_OUT_CB11) ||
	    (id == DDP_COMPONENT_COMP0_OUT_CB12) ||
	    (id == DDP_COMPONENT_COMP0_OUT_CB13) ||
	    (id == DDP_COMPONENT_COMP0_OUT_CB14))
		return true;
	else
		return false;
}

void mtk_ddp_comp_init_type(struct mtk_drm_private *private,
	enum mtk_ddp_comp_id id, int comp_type)
{
	if (id >= DDP_COMPONENT_ID_MAX)
		return;

	if (private->ddp_comp[id])
		private->ddp_comp[id]->is_virt_comp = comp_type;
}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_HOST) || IS_ENABLED(MTK_DRM_MEDIATEK_AUTO_AN_ONLY)
static u32 mtk_ddp_exdma_ultra_sel_map[] = {
	[DDP_COMPONENT_OVL_EXDMA2] =  OVL_EXDMA2_SEL,
	[DDP_COMPONENT_OVL1_EXDMA2] = OVL_EXDMA2_SEL,
	[DDP_COMPONENT_OVL_EXDMA3] =  OVL_EXDMA3_SEL,
	[DDP_COMPONENT_OVL1_EXDMA3] = OVL_EXDMA3_SEL,
	[DDP_COMPONENT_OVL_EXDMA4] =  OVL_EXDMA4_SEL,
	[DDP_COMPONENT_OVL1_EXDMA4] = OVL_EXDMA4_SEL,
	[DDP_COMPONENT_OVL_EXDMA5] =  OVL_EXDMA5_SEL,
	[DDP_COMPONENT_OVL1_EXDMA5] = OVL_EXDMA5_SEL,
	[DDP_COMPONENT_OVL_EXDMA6] =  OVL_EXDMA6_SEL,
	[DDP_COMPONENT_OVL1_EXDMA6] = OVL_EXDMA6_SEL,
	[DDP_COMPONENT_OVL_EXDMA7] =  OVL_EXDMA7_SEL,
	[DDP_COMPONENT_OVL1_EXDMA7] = OVL_EXDMA7_SEL,
	[DDP_COMPONENT_OVL_EXDMA8] =  OVL_EXDMA8_SEL,
	[DDP_COMPONENT_OVL1_EXDMA8] = OVL_EXDMA8_SEL,
	[DDP_COMPONENT_OVL_EXDMA9] =  OVL_EXDMA9_SEL,
	[DDP_COMPONENT_OVL1_EXDMA9] = OVL_EXDMA9_SEL,
};

static u32 mtk_ddp_output_ultra_sel_map[] = {
	[DDP_COMPONENT_DSI0] =  DSI0_MAC0_ULTRA,
	[DDP_COMPONENT_DSI1] =  DSI1_MAC0_ULTRA,
	[DDP_COMPONENT_DSI2] =  DSI2_MAC0_ULTRA,
	[DDP_COMPONENT_DP_INTF0] = DP_INTF0_ULTRA,
	[DDP_COMPONENT_DP_INTF1] = DP_INTF1_ULTRA,
	[DDP_COMPONENT_DISP_DVO] = DVO0_ULTRA,
};

enum mtk_ddp_lk_comp_id mtk_ddp_comp_map_lk_id(enum mtk_ddp_comp_id id)
{
	enum mtk_ddp_lk_comp_id lk_id = DDP_LK_DSI0;

	if (id >= DDP_COMPONENT_ID_MAX)
		return DDP_LK_DSI0;

	switch (id) {
	case DDP_COMPONENT_DSI0:
		lk_id = DDP_LK_DSI0;
		break;
	case DDP_COMPONENT_DISP_DVO:
		lk_id = DDP_LK_EDP;
		break;
	case DDP_COMPONENT_DP_INTF0:
		lk_id = DDP_LK_DP0;
		break;
	case DDP_COMPONENT_DSI1:
		lk_id = DDP_LK_DSI1;
		break;
	case DDP_COMPONENT_DSI2:
		lk_id = DDP_LK_DSI2;
		break;
	case DDP_COMPONENT_DSI2_VIRTUAL:
		lk_id = DDP_LK_DSI2_1;
		break;
	case DDP_COMPONENT_DP_INTF1:
		lk_id = DDP_LK_DP1;
		break;
	default:
		lk_id = DDP_LK_MAX;
		break;
	}
	return lk_id;
}

bool mtk_ddp_comp_is_enable_from_lk(enum mtk_ddp_comp_id id)
{
	enum mtk_ddp_lk_comp_id lk_id = DDP_LK_DSI0;

	if (id >= DDP_COMPONENT_ID_MAX)
		return false;

	lk_id = mtk_ddp_comp_map_lk_id(id);

	DDPMSG("%s comp %s from_atag 0x%X lk_id %d 0x%X\n",
		__func__,
		mtk_dump_comp_str_id(id),
		mtk_disp_num_from_atag(), lk_id, BIT(lk_id));

	if (mtk_disp_num_from_atag() & BIT(lk_id))
		return true;
	else
		return false;
}

static void mtk_ddp_comp_config_exdma_ultra_sel(enum mtk_ddp_comp_id id,
						enum mtk_ddp_comp_id out_id,
						void __iomem *ovlsys_regs)
{
	u32 val = 0;
	u32 val_mask = 0;

	u32 output_ultral_sel = 0;
	u32 exdma_ultral_fld = 0;

	u32 ultra_sel_reg_off = OVLSYS_EXRDMA_ULTRA_SEL0;
	u32 preultra_sel_reg_off = OVLSYS_EXRDMA_PREULTRA_SEL0;

	if (id >= ARRAY_SIZE(mtk_ddp_exdma_ultra_sel_map) ||
	    out_id >= ARRAY_SIZE(mtk_ddp_output_ultra_sel_map)) {
		DDPMSG("[E]%s invalid comp %s or output comp %s\n",
		       __func__, mtk_dump_comp_str_id(id), mtk_dump_comp_str_id(out_id));

		return;
	}

	exdma_ultral_fld = mtk_ddp_exdma_ultra_sel_map[id];
	output_ultral_sel = mtk_ddp_output_ultra_sel_map[out_id];

	if (exdma_ultral_fld == OVL_EXDMA8_SEL || exdma_ultral_fld == OVL_EXDMA9_SEL) {
		ultra_sel_reg_off = OVLSYS_EXRDMA_ULTRA_SEL1;
		preultra_sel_reg_off = OVLSYS_EXRDMA_PREULTRA_SEL1;
	}

	val = readl_relaxed(ovlsys_regs + ultra_sel_reg_off);

	SET_VAL_MASK(val, val_mask, output_ultral_sel, exdma_ultral_fld);

	writel_relaxed(val, ovlsys_regs + ultra_sel_reg_off);
	writel_relaxed(val, ovlsys_regs + preultra_sel_reg_off);

	DDPMSG("%s comp %s ultra_shift %d output comp %s sel %d val 0x%X mask 0x%X\n",
	       __func__,
	       mtk_dump_comp_str_id(id), REG_FLD_SHIFT(exdma_ultral_fld),
	       mtk_dump_comp_str_id(out_id), output_ultral_sel,
	       val, val_mask);
}

void mtk_ddp_comp_exdma_ultra_sel_config(struct mtk_drm_crtc *mtk_crtc)
{

	struct mtk_disp_mutex *mutex = NULL;
	struct mtk_ddp *ddp = NULL;
	void __iomem *ovlsys_regs = NULL;

	unsigned int i, j;
	struct mtk_ddp_comp *comp, *output_comp;

	if (!mtk_crtc) {
		DDPMSG("%s invalid CRTC\n", __func__);
		return;
	}

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (!output_comp) {
		DDPMSG("%s CRTC %d invalid output comp\n",
		       __func__, drm_crtc_index(&mtk_crtc->base));
		return;
	}

	mutex = mtk_crtc->mutex[0];
	if (!mutex) {
		DDPMSG("%s invalid mutex\n", __func__);
		return;
	}

	ddp = container_of(mutex, struct mtk_ddp, mutex[mutex->id]);
	if (!ddp) {
		DDPMSG("%s invalid ddp\n", __func__);
		return;
	}

	DDPMSG("%s CRTC %d output comp %s\n",
	       __func__, drm_crtc_index(&mtk_crtc->base), mtk_dump_comp_str(output_comp));

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		if (comp && mtk_ddp_comp_is_rdma(comp)) {
			if (ddp->data->dispsys_map[comp->id] == OVLSYS1)
				ovlsys_regs = mtk_crtc->ovlsys1_regs;
			else
				ovlsys_regs = mtk_crtc->ovlsys0_regs;

			mtk_ddp_comp_config_exdma_ultra_sel(comp->id, output_comp->id, ovlsys_regs);
		}
	}
}
#endif
