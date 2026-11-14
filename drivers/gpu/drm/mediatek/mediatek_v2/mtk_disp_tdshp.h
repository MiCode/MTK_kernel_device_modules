/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_TDSHP_H__
#define __MTK_DISP_TDSHP_H__

#include <linux/uaccess.h>
#include <uapi/drm/mediatek_drm.h>

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/module.h>
#include <linux/workqueue.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_lowpower.h"
#include "mtk_log.h"
#include "mtk_dump.h"
#include "mtk_disp_pq_device.h"

/* TDSHP Clarity */
#define MDP_TDSHP_00                            (0x000)
#define MDP_TDSHP_CFG                           (0x110)
#define MDP_HIST_CFG_00                         (0x064)
#define MDP_HIST_CFG_01                         (0x068)
#define MDP_LUMA_HIST_00                        (0x06C)
#define MDP_LUMA_SUM                            (0x0B4)
#define MDP_TDSHP_SRAM_1XN_OUTPUT_CNT           (0x0B8)
#define MDP_Y_FTN_1_0_MAIN                      (0x0BC)
#define MDP_TDSHP_STATUS_00                     (0x644)
#define MIDBAND_COEF_V_CUST_FLT1_00             (0x584)
#define MIDBAND_COEF_V_CUST_FLT1_01             (0x588)
#define MIDBAND_COEF_V_CUST_FLT1_02             (0x58C)
#define MIDBAND_COEF_V_CUST_FLT1_03             (0x590)
#define MIDBAND_COEF_H_CUST_FLT1_00             (0x594)
#define MIDBAND_COEF_H_CUST_FLT1_01             (0x598)
#define MIDBAND_COEF_H_CUST_FLT1_02             (0x59C)
#define MIDBAND_COEF_H_CUST_FLT1_03             (0x600)

#define HIGHBAND_COEF_V_CUST_FLT1_00            (0x604)
#define HIGHBAND_COEF_V_CUST_FLT1_01            (0x608)
#define HIGHBAND_COEF_V_CUST_FLT1_02            (0x60C)
#define HIGHBAND_COEF_V_CUST_FLT1_03            (0x610)
#define HIGHBAND_COEF_H_CUST_FLT1_00            (0x614)
#define HIGHBAND_COEF_H_CUST_FLT1_01            (0x618)
#define HIGHBAND_COEF_H_CUST_FLT1_02            (0x61C)
#define HIGHBAND_COEF_H_CUST_FLT1_03            (0x620)
#define HIGHBAND_COEF_RD_CUST_FLT1_00           (0x624)
#define HIGHBAND_COEF_RD_CUST_FLT1_01           (0x628)
#define HIGHBAND_COEF_RD_CUST_FLT1_02           (0x62C)
#define HIGHBAND_COEF_RD_CUST_FLT1_03           (0x630)
#define HIGHBAND_COEF_LD_CUST_FLT1_00           (0x634)
#define HIGHBAND_COEF_LD_CUST_FLT1_01           (0x638)
#define HIGHBAND_COEF_LD_CUST_FLT1_02           (0x63C)
#define HIGHBAND_COEF_LD_CUST_FLT1_03           (0x640)
#define MDP_TDSHP_SIZE_PARA                     (0x674)
#define MDP_TDSHP_FREQUENCY_WEIGHTING           (0x678)
#define MDP_TDSHP_FREQUENCY_WEIGHTING_FINAL     (0x67C)
#define SIZE_PARAMETER_MODE_SEGMENTATION_LENGTH (0x680)
#define FINAL_SIZE_ADAPTIVE_WEIGHT_HUGE         (0x684)
#define FINAL_SIZE_ADAPTIVE_WEIGHT_BIG          (0x688)
#define FINAL_SIZE_ADAPTIVE_WEIGHT_MEDIUM       (0x68C)
#define FINAL_SIZE_ADAPTIVE_WEIGHT_SMALL        (0x690)
#define ACTIVE_PARA_FREQ_M                      (0x694)
#define ACTIVE_PARA_FREQ_H                      (0x698)
#define ACTIVE_PARA_FREQ_D                      (0x69C)
#define ACTIVE_PARA_FREQ_L                      (0x700)
#define ACTIVE_PARA                             (0x704)
#define CLASS_0_2_GAIN                          (0x708)
#define CLASS_3_5_GAIN                          (0x70C)
#define CLASS_6_8_GAIN                          (0x710)
#define LUMA_CHROMA_PARAMETER                   (0x714)
#define MDP_TDSHP_STATUS_ROI_X                  (0x718)
#define MDP_TDSHP_STATUS_ROI_Y                  (0x71C)
#define FRAME_WIDTH_HIGHT                       (0x720)
#define MDP_TDSHP_SHADOW_CTRL                   (0x724)

struct mtk_disp_tdshp_data {
	bool support_shadow;
	bool need_bypass_shadow;
};

struct mtk_disp_tdshp_tile_overhead {
	unsigned int in_width;
	unsigned int overhead;
	unsigned int comp_overhead;
};

struct mtk_disp_tdshp_tile_overhead_v {
	unsigned int top_overhead_v;
	unsigned int bot_overhead_v;
	unsigned int comp_overhead_v;
};

struct mtk_disp_tdshp_primary {
	wait_queue_head_t size_wq;
	bool get_size_available;
	struct DISP_TDSHP_DISPLAY_SIZE tdshp_size;
	struct mutex data_lock;
	struct TDSHP_CLARITY_REG *tdshp_regs;
	uint32_t tdshp_reg_valid;
	int tdshp_clarity_support;
	unsigned int relay_state;
};

struct mtk_disp_tdshp {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_disp_tdshp_data *data;
	bool is_right_pipe;
	int path_order;
	struct mtk_ddp_comp *companion;
	struct mtk_disp_tdshp_primary *primary_data;
	struct mtk_disp_tdshp_tile_overhead tile_overhead;
	struct mtk_disp_tdshp_tile_overhead_v tile_overhead_v;
	unsigned int set_partial_update;
	unsigned int roi_height;
};

void disp_tdshp_regdump(struct mtk_ddp_comp *comp);
// for displayPQ update to swpm tppa
unsigned int disp_tdshp_bypass_info(struct mtk_drm_crtc *mtk_crtc);

#endif

