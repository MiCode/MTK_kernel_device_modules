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

struct DISP_TDSHP_REG_LEGACY {
	/* GROUP : MDP_TDSHP_00 (0x0000) */
	__u32 tdshp_softcoring_gain;         //[MSB:LSB] = [07:00]
	__u32 tdshp_gain_high;               //[MSB:LSB] = [15:08]
	__u32 tdshp_gain_mid;                //[MSB:LSB] = [23:16]
	__u32 tdshp_ink_sel;                 //[MSB:LSB] = [26:24]
	__u32 tdshp_bypass_high;             //[MSB:LSB] = [29:29]
	__u32 tdshp_bypass_mid;              //[MSB:LSB] = [30:30]
	__u32 tdshp_en;                      //[MSB:LSB] = [31:31]
	/* GROUP : MDP_TDSHP_01 (0x0004) */
	__u32 tdshp_limit_ratio;             //[MSB:LSB] = [03:00]
	__u32 tdshp_gain;                    //[MSB:LSB] = [11:04]
	__u32 tdshp_coring_zero;             //[MSB:LSB] = [23:16]
	__u32 tdshp_coring_thr;              //[MSB:LSB] = [31:24]
	/* GROUP : MDP_TDSHP_02 (0x0008) */
	__u32 tdshp_coring_value;            //[MSB:LSB] = [15:08]
	__u32 tdshp_bound;                   //[MSB:LSB] = [23:16]
	__u32 tdshp_limit;                   //[MSB:LSB] = [31:24]
	/* GROUP : MDP_TDSHP_03 (0x000c) */
	__u32 tdshp_sat_proc;                //[MSB:LSB] = [05:00]
	__u32 tdshp_ac_lpf_coe;              //[MSB:LSB] = [11:08]
	__u32 tdshp_clip_thr;                //[MSB:LSB] = [23:16]
	__u32 tdshp_clip_ratio;              //[MSB:LSB] = [28:24]
	__u32 tdshp_clip_en;                 //[MSB:LSB] = [31:31]
	/* GROUP : MDP_TDSHP_05 (0x0014) */
	__u32 tdshp_ylev_p048;               //[MSB:LSB] = [07:00]
	__u32 tdshp_ylev_p032;               //[MSB:LSB] = [15:08]
	__u32 tdshp_ylev_p016;               //[MSB:LSB] = [23:16]
	__u32 tdshp_ylev_p000;               //[MSB:LSB] = [31:24]
	/* GROUP : MDP_TDSHP_06 (0x0018) */
	__u32 tdshp_ylev_p112;               //[MSB:LSB] = [07:00]
	__u32 tdshp_ylev_p096;               //[MSB:LSB] = [15:08]
	__u32 tdshp_ylev_p080;               //[MSB:LSB] = [23:16]
	__u32 tdshp_ylev_p064;               //[MSB:LSB] = [31:24]
	/* GROUP : MDP_TDSHP_07 (0x001c) */
	__u32 tdshp_ylev_p176;               //[MSB:LSB] = [07:00]
	__u32 tdshp_ylev_p160;               //[MSB:LSB] = [15:08]
	__u32 tdshp_ylev_p144;               //[MSB:LSB] = [23:16]
	__u32 tdshp_ylev_p128;               //[MSB:LSB] = [31:24]
	/* GROUP : MDP_TDSHP_08 (0x0020) */
	__u32 tdshp_ylev_p240;               //[MSB:LSB] = [07:00]
	__u32 tdshp_ylev_p224;               //[MSB:LSB] = [15:08]
	__u32 tdshp_ylev_p208;               //[MSB:LSB] = [23:16]
	__u32 tdshp_ylev_p192;               //[MSB:LSB] = [31:24]
	/* GROUP : MDP_TDSHP_09 (0x0024) */
	__u32 tdshp_ylev_en;                 //[MSB:LSB] = [14:14]
	__u32 tdshp_ylev_alpha;              //[MSB:LSB] = [21:16]
	__u32 tdshp_ylev_256;                //[MSB:LSB] = [31:24]
	/* GROUP : MDP_PBC_00 (0x0040) */
	__u32 pbc1_radius_r;                 //[MSB:LSB] = [05:00]
	__u32 pbc1_theta_r;                  //[MSB:LSB] = [11:06]
	__u32 pbc1_rslope_1;                 //[MSB:LSB] = [21:12]
	__u32 pbc1_gain;                     //[MSB:LSB] = [29:22]
	__u32 pbc1_lpf_en;                   //[MSB:LSB] = [30:30]
	__u32 pbc1_en;                       //[MSB:LSB] = [31:31]
	/* GROUP : MDP_PBC_01 (0x0044) */
	__u32 pbc1_lpf_gain;                 //[MSB:LSB] = [05:00]
	__u32 pbc1_tslope;                   //[MSB:LSB] = [15:06]
	__u32 pbc1_radius_c;                 //[MSB:LSB] = [23:16]
	__u32 pbc1_theta_c;                  //[MSB:LSB] = [31:24]
	/* GROUP : MDP_PBC_02 (0x0048) */
	__u32 pbc1_edge_slope;               //[MSB:LSB] = [05:00]
	__u32 pbc1_edge_thr;                 //[MSB:LSB] = [13:08]
	__u32 pbc1_edge_en;                  //[MSB:LSB] = [14:14]
	__u32 pbc1_conf_gain;                //[MSB:LSB] = [19:16]
	__u32 pbc1_rslope;                   //[MSB:LSB] = [31:22]
	/* GROUP : MDP_PBC_03 (0x004c) */
	__u32 pbc2_radius_r;                 //[MSB:LSB] = [05:00]
	__u32 pbc2_theta_r;                  //[MSB:LSB] = [11:06]
	__u32 pbc2_rslope_1;                 //[MSB:LSB] = [21:12]
	__u32 pbc2_gain;                     //[MSB:LSB] = [29:22]
	__u32 pbc2_lpf_en;                   //[MSB:LSB] = [30:30]
	__u32 pbc2_en;                       //[MSB:LSB] = [31:31]
	/* GROUP : MDP_PBC_04 (0x0050) */
	__u32 pbc2_lpf_gain;                 //[MSB:LSB] = [05:00]
	__u32 pbc2_tslope;                   //[MSB:LSB] = [15:06]
	__u32 pbc2_radius_c;                 //[MSB:LSB] = [23:16]
	__u32 pbc2_theta_c;                  //[MSB:LSB] = [31:24]
	/* GROUP : MDP_PBC_05 (0x0054) */
	__u32 pbc2_edge_slope;               //[MSB:LSB] = [05:00]
	__u32 pbc2_edge_thr;                 //[MSB:LSB] = [13:08]
	__u32 pbc2_edge_en;                  //[MSB:LSB] = [14:14]
	__u32 pbc2_conf_gain;                //[MSB:LSB] = [19:16]
	__u32 pbc2_rslope;                   //[MSB:LSB] = [31:22]
	/* GROUP : MDP_PBC_06 (0x0058) */
	__u32 pbc3_radius_r;                 //[MSB:LSB] = [05:00]
	__u32 pbc3_theta_r;                  //[MSB:LSB] = [11:06]
	__u32 pbc3_rslope_1;                 //[MSB:LSB] = [21:12]
	__u32 pbc3_gain;                     //[MSB:LSB] = [29:22]
	__u32 pbc3_lpf_en;                   //[MSB:LSB] = [30:30]
	__u32 pbc3_en;                       //[MSB:LSB] = [31:31]
	/* GROUP : MDP_PBC_07 (0x005c) */
	__u32 pbc3_lpf_gain;                 //[MSB:LSB] = [05:00]
	__u32 pbc3_tslope;                   //[MSB:LSB] = [15:06]
	__u32 pbc3_radius_c;                 //[MSB:LSB] = [23:16]
	__u32 pbc3_theta_c;                  //[MSB:LSB] = [31:24]
	/* GROUP : MDP_PBC_08 (0x0060) */
	__u32 pbc3_edge_slope;               //[MSB:LSB] = [05:00]
	__u32 pbc3_edge_thr;                 //[MSB:LSB] = [13:08]
	__u32 pbc3_edge_en;                  //[MSB:LSB] = [14:14]
	__u32 pbc3_conf_gain;                //[MSB:LSB] = [19:16]
	__u32 pbc3_rslope;                   //[MSB:LSB] = [31:22]
	/* GROUP : MDP_TDSHP_10 (0x0320) */
	__u32  tdshp_mid_softlimit_ratio;    //[MSB:LSB] = [03:00]
	__u32  tdshp_mid_coring_zero;        //[MSB:LSB] = [23:16]
	__u32  tdshp_mid_coring_thr;         //[MSB:LSB] = [31:24]
	/* GROUP : MDP_TDSHP_11 (0x0324) */
	__u32  tdshp_mid_softcoring_gain;    //[MSB:LSB] = [07:00]
	__u32  tdshp_mid_coring_value;       //[MSB:LSB] = [15:08]
	__u32  tdshp_mid_bound;              //[MSB:LSB] = [13:16]
	__u32  tdshp_mid_limit;              //[MSB:LSB] = [31:24]
	/* GROUP : MDP_TDSHP_12 (0x0328) */
	__u32  tdshp_high_softlimit_ratio;   //[MSB:LSB] = [03:00]
	__u32  tdshp_high_coring_zero;       //[MSB:LSB] = [23:16]
	__u32  tdshp_high_coring_thr;        //[MSB:LSB] = [31:24]
	/* GROUP : MDP_TDSHP_13 (0x032c) */
	__u32  tdshp_high_softcoring_gain;   //[MSB:LSB] = [07:00]
	__u32  tdshp_high_coring_value;      //[MSB:LSB] = [15:08]
	__u32  tdshp_high_bound;             //[MSB:LSB] = [13:16]
	__u32  tdshp_high_limit;             //[MSB:LSB] = [31:24]
	/* GROUP : MDP_EDF_GAIN_00 (0x0300) */
	__u32  edf_clip_ratio_inc;           //[MSB:LSB] = [02:00]
	__u32  edf_edge_gain;                //[MSB:LSB] = [12:08]
	__u32  edf_detail_gain;              //[MSB:LSB] = [23:16]
	__u32  edf_flat_gain;                //[MSB:LSB] = [28:24]
	__u32  edf_gain_en;                  //[MSB:LSB] = [31:31]
	/* GROUP : MDP_EDF_GAIN_01 (0x0304) */
	__u32  edf_edge_th;                  //[MSB:LSB] = [08:00]
	__u32  edf_detail_fall_th;           //[MSB:LSB] = [17:09]
	__u32  edf_detail_rise_th;           //[MSB:LSB] = [23:18]
	__u32  edf_flat_th;                  //[MSB:LSB] = [30:25]
	/* GROUP : MDP_EDF_GAIN_02 (0x0308) */
	__u32  edf_edge_slope;               //[MSB:LSB] = [04:00]
	__u32  edf_detail_fall_slope;        //[MSB:LSB] = [12:08]
	__u32  edf_detail_rise_slope;        //[MSB:LSB] = [18:16]
	__u32  edf_flat_slope;               //[MSB:LSB] = [26:24]
	/* GROUP : MDP_EDF_GAIN_03 (0x030c) */
	__u32  edf_edge_mono_slope;          //[MSB:LSB] = [03:00]
	__u32  edf_edge_mono_th;             //[MSB:LSB] = [14:08]
	__u32  edf_edge_mag_slope;           //[MSB:LSB] = [20:16]
	__u32  edf_edge_mag_th;              //[MSB:LSB] = [28:24]
	/* GROUP : MDP_EDF_GAIN_05 (0x0314) */
	__u32  edf_edge_trend_flat_mag;
	__u32  edf_edge_trend_slope;
	__u32  edf_edge_trend_th;
	__u32  edf_bld_wgt_mag;              //[MSB:LSB] = [07:00]
	__u32  edf_bld_wgt_mono;             //[MSB:LSB] = [15:08]
	__u32  edf_bld_wgt_trend;
	/* GROUP : MDP_C_BOOST_MAIN (0x00e0) */
	__u32  tdshp_cboost_lmt_u;           //[MSB:LSB] = [31:24]
	__u32  tdshp_cboost_lmt_l;           //[MSB:LSB] = [23:16]
	__u32  tdshp_cboost_en;              //[MSB:LSB] = [13:13]
	__u32  tdshp_cboost_gain;            //[MSB:LSB] = [07:00]
	/* GROUP : MDP_C_BOOST_MAIN_2 (0x00e4) */
	__u32  tdshp_cboost_yconst;          //[MSB:LSB] = [31:24]
	__u32  tdshp_cboost_yoffset_sel;     //[MSB:LSB] = [17:16]
	__u32  tdshp_cboost_yoffset;         //[MSB:LSB] = [06:00]
	/* GROUP : MDP_POST_YLEV_00 (0x0480) */
	__u32 tdshp_post_ylev_p048;          //[MSB:LSB] = [07:00]
	__u32 tdshp_post_ylev_p032;          //[MSB:LSB] = [13:13]
	__u32 tdshp_post_ylev_p016;          //[MSB:LSB] = [23:16]
	__u32 tdshp_post_ylev_p000;          //[MSB:LSB] = [31:24]
	/* GROUP : MDP_POST_YLEV_01 (0x0484) */
	__u32 tdshp_post_ylev_p112;          //[MSB:LSB] = [07:00]
	__u32 tdshp_post_ylev_p096;          //[MSB:LSB] = [13:13]
	__u32 tdshp_post_ylev_p080;          //[MSB:LSB] = [23:16]
	__u32 tdshp_post_ylev_p064;          //[MSB:LSB] = [31:24]
	/* GROUP : MDP_POST_YLEV_02 (0x0488) */
	__u32 tdshp_post_ylev_p176;          //[MSB:LSB] = [07:00]
	__u32 tdshp_post_ylev_p160;          //[MSB:LSB] = [13:13]
	__u32 tdshp_post_ylev_p144;          //[MSB:LSB] = [23:16]
	__u32 tdshp_post_ylev_p128;          //[MSB:LSB] = [31:24]
	/* GROUP : MDP_POST_YLEV_03 (0x048c) */
	__u32 tdshp_post_ylev_p240;          //[MSB:LSB] = [07:00]
	__u32 tdshp_post_ylev_p224;          //[MSB:LSB] = [13:13]
	__u32 tdshp_post_ylev_p208;          //[MSB:LSB] = [23:16]
	__u32 tdshp_post_ylev_p192;          //[MSB:LSB] = [31:24]
	/* GROUP : MDP_POST_YLEV_04 (0x0490) */
	__u32 tdshp_post_ylev_en;            //[MSB:LSB] = [14:14]
	__u32 tdshp_post_ylev_alpha;         //[MSB:LSB] = [21:16]
	__u32 tdshp_post_ylev_256;           //[MSB:LSB] = [31:24]
};

#define DISP_TDSHP_REG                  DISP_TDSHP_REG_LEGACY

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
	unsigned int overhead_v;
	unsigned int comp_overhead_v;
};

struct mtk_disp_tdshp_primary {
	wait_queue_head_t size_wq;
	bool get_size_available;
	struct DISP_TDSHP_DISPLAY_SIZE tdshp_size;
	struct mutex data_lock;
	struct DISP_TDSHP_REG *tdshp_regs;
	int tdshp_reg_valid;
	int *aal_clarity_support;
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

