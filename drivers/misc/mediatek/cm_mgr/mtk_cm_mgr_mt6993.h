/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_CM_MGR_PLATFORM_H__
#define __MTK_CM_MGR_PLATFORM_H__

#define CREATE_TRACE_POINTS
#include "mtk_cm_mgr_events_mt6993.h"

#if IS_ENABLED(CONFIG_MTK_DRAMC)
#include <soc/mediatek/dramc.h>
#endif /* CONFIG_MTK_DRAMC */

enum {
	CM_MGR_LP5 = 0,
	CM_MGR_LP5X = 1,
	CM_MGR_MAX,
};

enum cm_mgr_cpu_cluster {
	CM_MGR_L = 0,
	CM_MGR_B,
	CM_MGR_BB,
	CM_MGR_CPU_CLUSTER,
};

struct tag_chipid {
	u32 size;
	u32 hw_code;
	u32 hw_subcode;
	u32 hw_ver;
	u32 sw_ver;
};

extern void check_cm_mgr_status_mt6993(unsigned int cluster, unsigned int freq,
					   unsigned int idx);
enum cm_wrap_emibm_type {
	CM_WRAP_EMIBM_TOTAL,
	CM_WRAP_EMIBM_CPU,
	CM_WRAP_EMIBM_GPU,
	CM_WRAP_EMIBM_APU,
	CM_WRAP_EMIBM_MM,
	CM_WRAP_EMIBM_MD,
	CM_WRAP_EMIBM_WB,
	CM_WRAP_EMI_MAX,
};

struct cm_profile_info {
	unsigned long long info[CM_WRAP_EMI_MAX];
	unsigned long update_cnt;
};

extern int cm_profile_get_bw(struct cm_profile_info *info_ptr);
#endif /* __MTK_CM_MGR_PLATFORM_H__ */
