/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef MTK_DISP_SWPM_H
#define MTK_DISP_SWPM_H

/* only record display0 behavior so far */
#define SWPM_DISP_NUM 1

enum disp_cmd_action {
	DISP_GET_SWPM_ADDR = 0,
	DISP_SWPM_SET_POWER_STA = 1,
	DISP_SWPM_SET_IDLE_STA = 2,
};

enum disp_pq_enum {
	DISP_PQ_AAL = 0,
	DISP_PQ_CCORR,
	DISP_PQ_C3D_17,
	DISP_PQ_GAMMA,
	DISP_PQ_COLOR,
	DISP_PQ_TDSHP,
	DISP_PQ_DITHER,
	DMDP_AAL,
	DISP_PQ_C3D_9,
	DISP_PQ_MAX,		/* ALWAYS keep at the end*/
};

/* MUST align struct with that in sspm */
struct disp_swpm_data {
	/* DISP PQ */
	unsigned int disp_aal;
	unsigned int disp_ccorr;
	unsigned int disp_c3d;
	unsigned int disp_gamma;
	unsigned int disp_color;
	unsigned int disp_tdshp;
	unsigned int disp_dither;
	unsigned int disp_chist;
	/* DSI */
	unsigned int dsi_lane_num;
	unsigned int dsi_phy_type;
	unsigned int dsi_data_rate;
	/* DISP others */
	unsigned int dsc_num;
	unsigned int oddmr_num;
	unsigned int ovl_num;
	unsigned int rsz_num;
};

extern int mtk_disp_get_pq_data(unsigned int info_idx);
extern int mtk_disp_get_dsi_data_rate(unsigned int info_idx);
extern int mtk_disp_is_panel_need_dsc(void *data);
extern int mtk_disp_get_wrking_exdma_num(void *data);
extern int mtk_disp_is_ovl_mdp_rsz_en(void *data);
extern int mtk_disp_is_disp_scaling_en(void *data);
extern int mtk_disp_get_oddmr_enable(int oddmr_idx);
extern int swpm_disp_v6993_init(void);
extern void swpm_disp_v6993_exit(void);
#endif
