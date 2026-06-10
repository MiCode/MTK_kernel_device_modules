// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Mingyuan Ma <mingyuan.ma@mediatek.com>
 * This driver adds support for SMMU performance monitor via ELA.
 */

#ifndef _MTK_SMMU_ELA_
#define _MTK_SMMU_ELA_

#include <linux/seq_file.h>

#if IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)
int mtk_smmu_enable_ela(u32 smmu_type);
int mtk_smmu_disable_ela(u32 smmu_type);
bool mtk_smmu_ela_enabled(u32 smmu_type);
int mtk_smmu_ela_init(u32 smmu_type);
void mtk_smmu_ela_dump(struct seq_file *s, u32 smmu_type);
#else /* CONFIG_DEVICE_MODULES_ARM_SMMU_V3 */
static inline int mtk_smmu_enable_ela(u32 smmu_type)
{
	return -1;
}

static inline int mtk_smmu_disable_ela(u32 smmu_type)
{
	return -1;
}

static inline bool mtk_smmu_ela_enabled(u32 smmu_type)
{
	return false;
}

static inline int mtk_smmu_ela_init(u32 smmu_type)
{
	return -1;
}

static inline void mtk_smmu_ela_dump(struct seq_file *s, u32 smmu_type)
{
}
#endif /* CONFIG_DEVICE_MODULES_ARM_SMMU_V3 */
#endif /* _MTK_SMMU_ELA_ */
