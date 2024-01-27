/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_DRM_DDP_ADDON_H
#define MTK_DRM_DDP_ADDON_H

#include <drm/drm_crtc.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_rect.h"
#include "mtk_layering_rule.h"

#include "../mml/mtk-mml.h"
#include "../mml/mtk-mml-drm.h"

#define DISP_PIPE_NUM 2

enum addon_scenario {
	NONE,
	ONE_SCALING,
	TWO_SCALING,
	WDMA_WRITE_BACK,
	WDMA_WRITE_BACK_OVL,
	GAME_PQ,
	VP_PQ,
	TRIPLE_DISP,
	MML_WITH_PQ,
	MML_RSZ,       /* MML Inline Rotate and MML RSZ */
	MML_DL,	       /* MML Direct Link */
	MML_SRAM_ONLY, /* MML Inline Rotate */
	DSC_COMP, /* describe which DSC module would apply in this CRTC */
	ADDON_SCN_NR,
};

enum addon_module {
	DISP_RSZ,
	DISP_RSZ_v2,
	DISP_RSZ_v3,
	DISP_RSZ_v4,
	DISP_RSZ_v5,
	DISP_RSZ_v6,
	OVL_RSZ,         /* OVL_2L pq out, pq in OVL_2L */
	OVL_RSZ_1,
	DISP_WDMA0,
	DISP_WDMA0_v2,
	DISP_WDMA0_v3,
	DISP_WDMA0_v4,
	DISP_WDMA0_v5,
	DISP_WDMA1,
	DISP_WDMA1_v2,
	DISP_WDMA2,
	DISP_OVLSYS_WDMA0,
	DISP_OVLSYS_WDMA2,
	DISP_OVLSYS_WDMA0_v2,
	DISP_WDMA2_v2,
	DMDP_PQ_WITH_RDMA,
	DISP_MML_DL,     /* pq in OVL_2L */
	DISP_MML_DL_1,
	DISP_MML_IR_PQ,    /* OVL_2L blend out, ufod in OVL_4L */
	DISP_MML_IR_PQ_1,
	DISP_MML_IR_PQ_v2, /* OVL_2L pq out, pq in OVL_2L */
	DISP_MML_IR_PQ_v2_1,
	DISP_MML_SRAM_ONLY,
	DISP_MML_SRAM_ONLY_1,
	DISP_MML_IR_PQ_v3,
	DSC_0,
	DSC_1,
	ADDON_MODULE_NUM,
};

enum addon_type {
	ADDON_BETWEEN,
	ADDON_BEFORE,
	ADDON_AFTER,
	ADDON_CONNECT,
	ADDON_DISCONNECT,
	ADDON_EMBED,
};

struct mtk_lye_ddp_state {
	enum addon_scenario scn[HRT_DISP_TYPE_NUM];
	uint8_t lc_tgt_layer;
	u32 rpo_lye;
	u32 mml_ir_lye;
	u32 mml_dl_lye;
	bool need_repaint;
};

struct mtk_addon_path_data {
	const unsigned int *path;
	unsigned int path_len;
};

struct mtk_addon_module_data {
	enum addon_module module;
	enum addon_type type;
	unsigned int attach_comp;
};

struct mtk_addon_scenario_data {
	unsigned int module_num;
	const struct mtk_addon_module_data *module_data;
	enum HRT_TB_TYPE hrt_type;
};

struct mtk_addon_config_type {
	enum addon_module module;
	enum addon_type type;
	uint8_t tgt_layer;
	int tgt_comp;
};

struct mtk_rsz_param {
	u32 in_x;
	u32 out_x;
	u32 step;
	u32 int_offset;
	u32 sub_offset;
	u32 in_len;
	u32 out_len;
};

struct mtk_addon_rsz_config {
	struct mtk_addon_config_type config_type;
	struct mtk_rect rsz_src_roi;
	struct mtk_rect rsz_dst_roi;
	struct mtk_rsz_param rsz_param;
	uint8_t lc_tgt_layer;
};

struct mtk_addon_wdma_config {
	struct mtk_addon_config_type config_type;
	struct mtk_rect wdma_src_roi;
	struct mtk_rect wdma_dst_roi;
	int pitch;
	dma_addr_t addr;
	struct drm_framebuffer *fb;
	struct golden_setting_context *p_golden_setting_context;
};

struct mtk_addon_mml_config {
	struct mtk_addon_config_type config_type;
	void *ctx;
	struct mml_mutex_ctl mutex;		    /* [IN] display mode and output port */
	struct mml_submit submit;		    /* [IN] mml_drm_split_info submit_pq */
	bool dual;				    /* [IN] set true if display uses dual pipe */
	struct mml_task *task;			    /* [OUT] task and config for mml */
	struct mtk_rect mml_src_roi[DISP_PIPE_NUM]; /* [OUT] src roi for OVL */
	struct mtk_rect mml_dst_roi[DISP_PIPE_NUM]; /* [OUT] dst roi for OVL */
	bool is_yuv;				    /* [OUT] src format */
	bool y2r_en;				    /* [OUT] enable y2r */
	bool is_entering;			    /* [OUT] state of entering or leaving */
	u8 pipe;				    /* [OUT] pipe indicator 0:left 1:right*/
};

union mtk_addon_config {
	struct mtk_addon_config_type config_type;
	struct mtk_addon_rsz_config addon_rsz_config;
	struct mtk_addon_wdma_config addon_wdma_config;
	struct mtk_addon_mml_config addon_mml_config;
};

const struct mtk_addon_path_data *
mtk_addon_module_get_path(enum addon_module module);
const struct mtk_addon_scenario_data *
mtk_addon_get_scenario_data(const char *source, struct drm_crtc *crtc,
			    enum addon_scenario scn);
const struct mtk_addon_scenario_data *
mtk_addon_get_scenario_data_dual(const char *source, struct drm_crtc *crtc,
			    enum addon_scenario scn);
bool mtk_addon_scenario_support(struct drm_crtc *crtc, enum addon_scenario scn);
void mtk_addon_path_config(struct drm_crtc *crtc,
			const struct mtk_addon_module_data *module_data,
			union mtk_addon_config *addon_config,
			struct cmdq_pkt *cmdq_handle);

void mtk_addon_connect_between(struct drm_crtc *crtc, unsigned int ddp_mode,
			       const struct mtk_addon_module_data *module_data,
			       union mtk_addon_config *addon_config,
			       struct cmdq_pkt *cmdq_handle);
void mtk_addon_disconnect_between(
	struct drm_crtc *crtc, unsigned int ddp_mode,
	const struct mtk_addon_module_data *module_data,
	union mtk_addon_config *addon_config, struct cmdq_pkt *cmdq_handle);
void mtk_addon_connect_before(struct drm_crtc *crtc, unsigned int ddp_mode,
			      const struct mtk_addon_module_data *module_data,
			      union mtk_addon_config *addon_config,
			      struct cmdq_pkt *cmdq_handle);
void mtk_addon_disconnect_before(
	struct drm_crtc *crtc, unsigned int ddp_mode,
	const struct mtk_addon_module_data *module_data,
	union mtk_addon_config *addon_config, struct cmdq_pkt *cmdq_handle);
void mtk_addon_connect_after(struct drm_crtc *crtc, unsigned int ddp_mode,
			      const struct mtk_addon_module_data *module_data,
			      union mtk_addon_config *addon_config,
			      struct cmdq_pkt *cmdq_handle);
void mtk_addon_disconnect_after(
	struct drm_crtc *crtc, unsigned int ddp_mode,
	const struct mtk_addon_module_data *module_data,
	union mtk_addon_config *addon_config, struct cmdq_pkt *cmdq_handle);

void mtk_addon_connect_external(struct drm_crtc *crtc, unsigned int ddp_mode,
			      const struct mtk_addon_module_data *module_data,
			      union mtk_addon_config *addon_config,
			      struct cmdq_pkt *cmdq_handle);
void mtk_addon_disconnect_external(
	struct drm_crtc *crtc, unsigned int ddp_mode,
	const struct mtk_addon_module_data *module_data,
	union mtk_addon_config *addon_config, struct cmdq_pkt *cmdq_handle);

void mtk_addon_connect_embed(struct drm_crtc *crtc, unsigned int ddp_mode,
			     const struct mtk_addon_module_data *module_data,
			     union mtk_addon_config *addon_config, struct cmdq_pkt *cmdq_handle);
void mtk_addon_disconnect_embed(struct drm_crtc *crtc, unsigned int ddp_mode,
				const struct mtk_addon_module_data *module_data,
				union mtk_addon_config *addon_config, struct cmdq_pkt *cmdq_handle);

#endif

