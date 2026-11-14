/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef MBRAINK_PMU_H
#define MBRAINK_PMU_H

#include <mbraink_ioctl_struct_def.h>

int mbraink_pmu_init(void);
int mbraink_pmu_deinit(void);

int mbraink_set_pmu_enable(bool enable);
int mbraink_get_pmu_info(struct mbraink_pmu_info *pmuInfo);

#endif /*end of MBRAINK_PMU_H*/
