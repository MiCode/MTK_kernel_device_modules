// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
#include "../mtk_drm_crtc.h"
#include "../mtk_drm_ddp_comp.h"
#include "../mtk_drm_drv.h"
#include "../mtk_dump.h"
#include "../mtk_log.h"

#include "mtk_drm_ddp_comp_auto.h"
#include "mtk_drm_drv_auto.h"

#include "mtk_drm_crtc_auto.h"


/* CRTC0 DSI0 */
#else
static const enum mtk_ddp_comp_id mt6991_mtk_ovlsys_main_bringup[] = {
	DDP_COMPONENT_OVL_EXDMA3,
	DDP_COMPONENT_OVL0_BLENDER1,
	DDP_COMPONENT_OVL_EXDMA4,
	DDP_COMPONENT_OVL0_BLENDER2,
	DDP_COMPONENT_OVL_EXDMA5,
	DDP_COMPONENT_OVL0_BLENDER3,
	DDP_COMPONENT_OVL_EXDMA6,
	DDP_COMPONENT_OVL0_BLENDER4,
	DDP_COMPONENT_OVL_EXDMA7,
	DDP_COMPONENT_OVL0_BLENDER5,
	DDP_COMPONENT_OVL0_OUTPROC0,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC5,
};

static const enum mtk_ddp_comp_id mt6991_mtk_ddp_main_bringup[] = {
	DDP_COMPONENT_DLI_ASYNC0,
	DDP_COMPONENT_PQ0_IN_CB0,
#ifdef DRM_BYPASS_PQ
	DDP_COMPONENT_PQ0_OUT_CB6,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB1,
	DDP_COMPONENT_DLO_ASYNC1, DDP_COMPONENT_DLI_ASYNC21,
	DDP_COMPONENT_SPLITTER0_IN_CB1,
	DDP_COMPONENT_SPLITTER0_OUT_CB9,
#else
	DDP_COMPONENT_MDP_RSZ0,		DDP_COMPONENT_TDSHP0,
	DDP_COMPONENT_CCORR0,		DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_C3D0,		DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D1,		DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,		DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0,	DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_PQ0_OUT_CB0,
	DDP_COMPONENT_SPR0,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB0,
	DDP_COMPONENT_DLO_ASYNC0,	DDP_COMPONENT_DLI_ASYNC20,
	DDP_COMPONENT_ODDMR0,		DDP_COMPONENT_DITHER2,
	DDP_COMPONENT_POSTALIGN0,
	DDP_COMPONENT_SPLITTER0_OUT_CB9,
#endif
	DDP_COMPONENT_COMP0_OUT_CB6,
	DDP_COMPONENT_MERGE0_OUT_CB0,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_VDISP_AO,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_CHIST0,	DDP_COMPONENT_CHIST1,
#endif
};

static const enum mtk_ddp_comp_id mt6991_mtk_ddp_mem_dp_wo_tdshp[] = {
	DDP_COMPONENT_OVL1_EXDMA6,
	DDP_COMPONENT_OVL1_BLENDER4,
	DDP_COMPONENT_OVL1_OUTPROC3,
	DDP_COMPONENT_OVLSYS_WDMA2,
};

/* CRTC1 DP0 */
static const enum mtk_ddp_comp_id mt6991_mtk_ddp_ext_dp[] = {
	DDP_COMPONENT_OVL1_EXDMA3,
	DDP_COMPONENT_OVL1_BLENDER1,
	DDP_COMPONENT_OVL1_EXDMA4,
	DDP_COMPONENT_OVL1_BLENDER2,
	DDP_COMPONENT_OVL1_EXDMA5,
	DDP_COMPONENT_OVL1_BLENDER3,
	DDP_COMPONENT_OVL1_EXDMA6,
	DDP_COMPONENT_OVL1_BLENDER4,
	DDP_COMPONENT_OVL1_OUTPROC0,
	DDP_COMPONENT_OVLSYS1_DLO_ASYNC5,
	DDP_COMPONENT_DLI_ASYNC8,
	DDP_COMPONENT_PQ0_IN_CB8,
#ifdef DRM_BYPASS_PQ
	DDP_COMPONENT_PQ0_OUT_CB7,
#else
	DDP_COMPONENT_MDP_RSZ1,		DDP_COMPONENT_TDSHP1,
	DDP_COMPONENT_CCORR2,		DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_C3D2,		DDP_COMPONENT_CCORR3,
	DDP_COMPONENT_C3D3,		DDP_COMPONENT_DMDP_AAL1,
	DDP_COMPONENT_AAL1,		DDP_COMPONENT_GAMMA1,
	DDP_COMPONENT_POSTMASK1,	DDP_COMPONENT_DITHER1,
	DDP_COMPONENT_PQ0_OUT_CB3,
#endif
	DDP_COMPONENT_PANEL0_COMP_OUT_CB2,
	DDP_COMPONENT_DLO_ASYNC2,
	DDP_COMPONENT_DLI_ASYNC22,
	DDP_COMPONENT_SPLITTER0_IN_CB2,
	DDP_COMPONENT_SPLITTER0_OUT_CB10,
	DDP_COMPONENT_COMP0_OUT_CB7,
	DDP_COMPONENT_MERGE0_OUT_CB1,
	DDP_COMPONENT_DP_INTF0,
};

/* CRTC3 eDP */
static const enum mtk_ddp_comp_id mt6991_mtk_ddp_secondary[] = {
	DDP_COMPONENT_OVL_EXDMA9,
	DDP_COMPONENT_OVL0_BLENDER7,
	DDP_COMPONENT_OVL0_OUTPROC2,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC7,
	DDP_COMPONENT_DLI_ASYNC2,
	DDP_COMPONENT_PQ0_IN_CB2,
	DDP_COMPONENT_PQ0_OUT_CB8,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB3,
	DDP_COMPONENT_DLO_ASYNC3,
	DDP_COMPONENT_DLI_ASYNC23,
	DDP_COMPONENT_SPLITTER0_IN_CB3,
	DDP_COMPONENT_SPLITTER0_OUT_CB11,
	DDP_COMPONENT_COMP0_OUT_CB8,
	DDP_COMPONENT_MERGE0_OUT_CB2,
	DDP_COMPONENT_DISP_DVO,
};

static const enum mtk_ddp_comp_id mt6991_mtk_ddp_discrete_chip[] = {

};

/* CRTC4 DSI1 */
static const enum mtk_ddp_comp_id mt6991_mtk_ddp_fifth_path[] = {
	DDP_COMPONENT_OVL1_EXDMA8,
	DDP_COMPONENT_OVL1_BLENDER6,
	DDP_COMPONENT_OVL1_OUTPROC1,
	DDP_COMPONENT_OVLSYS1_DLO_ASYNC6,
	DDP_COMPONENT_DLI_ASYNC9,
	DDP_COMPONENT_PQ0_IN_CB9,
	DDP_COMPONENT_PQ0_OUT_CB9,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB4,
	DDP_COMPONENT_DLO_ASYNC4,
	DDP_COMPONENT_DLI_ASYNC24,
	DDP_COMPONENT_SPLITTER0_IN_CB4,
	DDP_COMPONENT_SPLITTER0_OUT_CB12,
	DDP_COMPONENT_COMP0_OUT_CB9,
	DDP_COMPONENT_MERGE0_OUT_CB3,
	DDP_COMPONENT_DSI1,
};

/* CRTC5 DSI2 */
static const enum mtk_ddp_comp_id mt6991_mtk_ddp_sixth_path[] = {
	DDP_COMPONENT_OVL1_EXDMA9,
	DDP_COMPONENT_OVL1_BLENDER7,
	DDP_COMPONENT_OVL1_OUTPROC2,
	DDP_COMPONENT_OVLSYS1_DLO_ASYNC7,
	DDP_COMPONENT_DLI_ASYNC10,
	DDP_COMPONENT_PQ0_IN_CB10,
	DDP_COMPONENT_PQ0_OUT_CB10,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB5,
	DDP_COMPONENT_DLO_ASYNC5,
	DDP_COMPONENT_DLI_ASYNC25,
	DDP_COMPONENT_SPLITTER0_IN_CB5,
	DDP_COMPONENT_SPLITTER0_OUT_CB13,
	DDP_COMPONENT_COMP0_OUT_CB10,
	DDP_COMPONENT_MERGE0_OUT_CB4,
	DDP_COMPONENT_DSI2,
};

/* CRTC6 DP1 */
static const enum mtk_ddp_comp_id mt6991_mtk_ddp_seventh_path[] = {
	DDP_COMPONENT_OVL1_EXDMA7,
	DDP_COMPONENT_OVL1_BLENDER5,
	DDP_COMPONENT_OVL1_OUTPROC3,
	DDP_COMPONENT_OVLSYS1_DLO_ASYNC8,
	DDP_COMPONENT_DLI_ASYNC11,
	DDP_COMPONENT_PQ0_IN_CB11,
	DDP_COMPONENT_PQ0_OUT_CB11,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB6,
	DDP_COMPONENT_DLO_ASYNC6,
	DDP_COMPONENT_DLI_ASYNC26,
	DDP_COMPONENT_SPLITTER0_IN_CB6,
	DDP_COMPONENT_SPLITTER0_OUT_CB14,
	DDP_COMPONENT_COMP0_OUT_CB11,
	DDP_COMPONENT_MERGE0_OUT_CB5,
	DDP_COMPONENT_DP_INTF1,
};

const struct mtk_crtc_path_data mt6991_mtk_main_path_data = {
	.ovl_path[DDP_MAJOR][0] = mt6991_mtk_ovlsys_main_bringup,
	.ovl_path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ovlsys_main_bringup),
	.path[DDP_MAJOR][0] = mt6991_mtk_ddp_main_bringup,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ddp_main_bringup),
	.path_req_hrt[DDP_MAJOR][0] = true,
//	.ovl_path[DDP_MINOR][0] = mt6991_mtk_ovlsys_main_bringup,
//	.ovl_path_len[DDP_MINOR][0] = ARRAY_SIZE(mt6991_mtk_ovlsys_main_bringup),
//	.path[DDP_MINOR][0] = mt6991_mtk_ddp_main_bringup_minor,
//	.path_len[DDP_MINOR][0] = ARRAY_SIZE(mt6989_mtk_ddp_main_bringup_minor),
//	.path_req_hrt[DDP_MINOR][0] = true,
//	.dual_ovl_path[0] = mt6989_mtk_ovlsys_dual_main_bringup,
//	.dual_ovl_path_len[0] = ARRAY_SIZE(mt6989_mtk_ovlsys_dual_main_bringup),
//	.dual_path[0] = mt6989_mtk_ddp_dual_main_bringup,
//	.dual_path_len[0] = ARRAY_SIZE(mt6989_mtk_ddp_dual_main_bringup),
//	.wb_path[DDP_MAJOR] = mt6983_mtk_ddp_main_wb_path,
//	.wb_path_len[DDP_MAJOR] = ARRAY_SIZE(mt6983_mtk_ddp_main_wb_path),
//	.addon_data = mt6991_addon_main,
//	.addon_data_dual = mt6989_addon_main_dual,
//	.scaling_data = mt6991_scaling_main,
//	.scaling_data_dual = mt6989_scaling_main_dual,
};

const struct mtk_crtc_path_data mt6991_mtk_main_full_set_data = {
//	.ovl_path[DDP_MAJOR][0] = mt6989_mtk_ovlsys_main_full_set,
//	.ovl_path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6989_mtk_ovlsys_main_full_set),
//	.path[DDP_MAJOR][0] = mt6991_mtk_ddp_main_bringup,
//	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ddp_main_bringup),
//	.path_req_hrt[DDP_MAJOR][0] = true,
//	.addon_data = mt6989_addon_main,
//	.addon_data_dual = mt6989_addon_main_dual,
//	.scaling_data = mt6989_scaling_main,
//	.scaling_data_dual = mt6989_scaling_main_dual,
};

const struct mtk_crtc_path_data mt6991_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt6991_mtk_ddp_ext_dp,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ddp_ext_dp),
	.path_req_hrt[DDP_MAJOR][0] = true,
//	.addon_data = mt6991_addon_ext,
#if !IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
	.is_exdma_dual_layer = true,
#endif
};

const struct mtk_crtc_path_data mt6991_mtk_dp_w_tdshp_path_data = {
//	.path[DDP_MAJOR][0] = mt6989_mtk_ddp_mem_dp_w_tdshp,
//	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6989_mtk_ddp_mem_dp_w_tdshp),
//	.addon_data = mt6983_addon_dp_w_tdshp,
};

const struct mtk_crtc_path_data mt6991_mtk_dp_wo_tdshp_path_data = {
	.path[DDP_MAJOR][0] = mt6991_mtk_ddp_mem_dp_wo_tdshp,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ddp_mem_dp_wo_tdshp),
//	.addon_data = mt6989_addon_dp_wo_tdshp,
	.is_exdma_dual_layer = true,
};

const struct mtk_crtc_path_data mt6991_mtk_secondary_path_data = {
	.path[DDP_MAJOR][0] = mt6991_mtk_ddp_secondary,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ddp_secondary),
	.path_req_hrt[DDP_MAJOR][0] = true,
//	.dual_path[0] = mt6989_mtk_ddp_dual_secondary_dp,
//	.dual_path_len[0] = ARRAY_SIZE(mt6989_mtk_ddp_dual_secondary_dp),
//	.addon_data = mt6989_addon_secondary_path,
//	.addon_data_dual = mt6989_addon_secondary_path_dual,
};

const struct mtk_crtc_path_data mt6991_mtk_discrete_path_data = {
	.path[DDP_MAJOR][0] = mt6991_mtk_ddp_discrete_chip,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ddp_discrete_chip),
	.path_req_hrt[DDP_MAJOR][0] = true,
//	.dual_path[0] = mt6989_mtk_ddp_dual_discrete_chip,
//	.dual_path_len[0] = ARRAY_SIZE(mt6989_mtk_ddp_dual_discrete_chip),
//	.addon_data = mt6989_addon_discrete_path,
//	.addon_data_dual = mt6989_addon_discrete_path_dual,
	.is_discrete_path = true,
};

const struct mtk_crtc_path_data mt6991_mtk_fifth_path_data = {
	.path[DDP_MAJOR][0] = mt6991_mtk_ddp_fifth_path,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ddp_fifth_path),
	.path_req_hrt[DDP_MAJOR][0] = true,
};

const struct mtk_crtc_path_data mt6991_mtk_sixth_path_data = {
	.path[DDP_MAJOR][0] = mt6991_mtk_ddp_sixth_path,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ddp_sixth_path),
	.path_req_hrt[DDP_MAJOR][0] = true,
};

const struct mtk_crtc_path_data mt6991_mtk_seventh_path_data = {
	.path[DDP_MAJOR][0] = mt6991_mtk_ddp_seventh_path,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ddp_seventh_path),
	.path_req_hrt[DDP_MAJOR][0] = true,
};

int mtk_drm_pm_notifier(struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	struct mtk_drm_kernel_pm *kernel_pm = container_of(notifier, typeof(*kernel_pm), nb);

	DDPMSG("%s pm_event %d set pm status(%d)\n",
	       __func__, pm_event, atomic_read(&kernel_pm->status));

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		wake_up_interruptible(&kernel_pm->wq);
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		wake_up_interruptible(&kernel_pm->wq);
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}
#endif
