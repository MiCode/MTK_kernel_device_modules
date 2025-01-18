/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_VDISP_COMMON_H__
#define __MTK_VDISP_COMMON_H__

enum mtk_vdisp_version {
	VDISP_VER1 = 1,
	VDISP_VER2,
	VDISP_VER_CNT,
};

struct mtk_vdisp_funcs {
	void (*genpd_put)(void);
	void (*vlp_disp_vote)(u32 user, bool set);
	s32 (*poll_power_cnt)(s32 val);
	void (*sent_aod_scp_sema)(void __iomem *_SPM_SEMA_AP);
};

#endif
