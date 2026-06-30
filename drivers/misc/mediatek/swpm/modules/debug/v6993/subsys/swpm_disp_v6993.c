// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/notifier.h>
#include "swpm_module.h"
#include "sspm_reservedmem.h"

#include "swpm_disp_v6993.h"

static struct disp_swpm_data *disp_swpm_data_ptr;

static void update_disp_info(void)
{
	if (!disp_swpm_data_ptr)
		return;

	/* DISP PQ */
	disp_swpm_data_ptr->disp_aal = mtk_disp_get_pq_data(DMDP_AAL);
	disp_swpm_data_ptr->disp_ccorr = mtk_disp_get_pq_data(DISP_PQ_CCORR);
	disp_swpm_data_ptr->disp_c3d =
			mtk_disp_get_pq_data(DISP_PQ_C3D_17) + mtk_disp_get_pq_data(DISP_PQ_C3D_9);
	disp_swpm_data_ptr->disp_gamma = mtk_disp_get_pq_data(DISP_PQ_GAMMA);
	disp_swpm_data_ptr->disp_color = mtk_disp_get_pq_data(DISP_PQ_COLOR);
	disp_swpm_data_ptr->disp_tdshp = mtk_disp_get_pq_data(DISP_PQ_TDSHP);
	disp_swpm_data_ptr->disp_dither = mtk_disp_get_pq_data(DISP_PQ_DITHER);

	/* DSI */
	disp_swpm_data_ptr->dsi_lane_num = 4;
	disp_swpm_data_ptr->dsi_data_rate = mtk_disp_get_dsi_data_rate(0);
	disp_swpm_data_ptr->dsi_phy_type = mtk_disp_get_dsi_data_rate(1);

	/* DISP others */
	// use panel need dsc as surrogate for dsc is enabled
	disp_swpm_data_ptr->dsc_num = mtk_disp_is_panel_need_dsc(NULL);
	disp_swpm_data_ptr->ovl_num = mtk_disp_get_wrking_exdma_num(NULL);
	// disp_mdp_rsz use crtc request as surrogate for whether mdp_rsz is enabled
	disp_swpm_data_ptr->rsz_num = mtk_disp_is_ovl_mdp_rsz_en(NULL)
		+ mtk_disp_is_disp_scaling_en(NULL);
	disp_swpm_data_ptr->oddmr_num = (mtk_disp_get_oddmr_enable(0) != 0)
		|| (mtk_disp_get_oddmr_enable(1) != 0)
		|| (mtk_disp_get_oddmr_enable(2) != 0);
}

static int disp_swpm_event(struct notifier_block *nb,
			unsigned long event, void *v)
{
	switch (event) {
	case SWPM_LOG_DATA_NOTIFY:
		update_disp_info();
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block disp_swpm_notifier = {
	.notifier_call = disp_swpm_event,
};

int swpm_disp_v6993_init(void)
{
	unsigned int offset;

	offset = swpm_set_and_get_cmd(0, 0, DISP_GET_SWPM_ADDR, DISP_CMD_TYPE);

	disp_swpm_data_ptr = (struct disp_swpm_data *)
		sspm_sbuf_get(offset);

	/* exception control for illegal sbuf request */
	if (!disp_swpm_data_ptr)
		return -1;

	swpm_register_event_notifier(&disp_swpm_notifier);

	return 0;
}

void swpm_disp_v6993_exit(void)
{
	swpm_unregister_event_notifier(&disp_swpm_notifier);
}
