/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __GED_GPU_MEMSYS_H__
#define __GED_GPU_MEMSYS_H__
#include <linux/types.h>
#include "ged_type.h"


struct gpu_memsys_stat {
	unsigned int features;
	unsigned int wla_ctrl_1;
	unsigned int wla_ctrl_2;
};

GED_ERROR ged_gpu_memsys_init(void);
struct gpu_memsys_stat *get_gpu_memsys_stat(void);
void ged_gpu_memsys_feature_enable(unsigned int idx);

GED_ERROR ged_gpu_memsys_exit(void);

#if !IS_ENABLED(CONFIG_MTK_GPU_LEGACY) /* MTK_GPU_EB_SUPPORT */
GED_ERROR ged_gpu_aximon_init(void);
GED_ERROR ged_gpu_aximon_exit(void);
#endif /* MTK_GPU_EB_SUPPORT */

#endif /* __GED_GPU_MEMSYS_H__ */


