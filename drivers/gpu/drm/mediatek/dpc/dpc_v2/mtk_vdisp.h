/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_VDISP_H__
#define __MTK_VDISP_H__

#include "mtk_dpc_v2.h"
#include "mtk_vdisp_common.h"
#include "mtk_vdisp_avs.h"

/* This id is only for disp internal use */
enum disp_pd_id {
	DISP_PD_DISP_VCORE,
	DISP_PD_DISP1,
	DISP_PD_DISP0,
	DISP_PD_OVL1,
	DISP_PD_OVL0,
	DISP_PD_MML1,
	DISP_PD_MML0,
	DISP_PD_EDP,
	DISP_PD_DPTX,
	DISP_PD_NUM,
};

void mtk_vdisp_dpc_register(const struct dpc_funcs *funcs);

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

struct mtk_vdisp_data {
	const struct mtk_vdisp_avs_data *avs;
};

#endif
