/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 *
 * Author: Chunfeng Yun <chunfeng.yun@mediatek.com>
 */

#ifndef __PHY_MTK_H__
#define __PHY_MTK_H__

#include <linux/bitfield.h>
#include <linux/io.h>

#define PHY_MODE_PROPERTY_MAX 15

enum mtk_phy_submode {
	PHY_MODE_BC11_SW_SET = 1,
	PHY_MODE_BC11_SW_CLR,
	PHY_MODE_DPDMPULLDOWN_SET,
	PHY_MODE_DPDMPULLDOWN_CLR,
	PHY_MODE_DPPULLUP_SET,
	PHY_MODE_DPPULLUP_CLR,
	PHY_MODE_NORMAL,
	PHY_MODE_FLIP,
	PHY_MODE_SUSPEND_DEV,
	PHY_MODE_SUSPEND_NO_DEV,
	PHY_MODE_DIS_PRE_EMP,
	/* reserves for USB driving properpt switch use */
	PHY_MODE_PROPERTY_SET = 0xfff0,
	PHY_MODE_PROPERTY_SET_END = PHY_MODE_PROPERTY_SET + PHY_MODE_PROPERTY_MAX,
};

static inline void mtk_phy_clear_bits(void __iomem *reg, u32 bits)
{
	u32 tmp = readl(reg);

	tmp &= ~bits;
	writel(tmp, reg);
}

static inline void mtk_phy_set_bits(void __iomem *reg, u32 bits)
{
	u32 tmp = readl(reg);

	tmp |= bits;
	writel(tmp, reg);
}

static inline void mtk_phy_update_bits(void __iomem *reg, u32 mask, u32 val)
{
	u32 tmp = readl(reg);

	tmp &= ~mask;
	tmp |= val & mask;
	writel(tmp, reg);
}

/* field @mask shall be constant and continuous */
#define mtk_phy_update_field(reg, mask, val) \
({ \
	typeof(mask) mask_ = (mask);	\
	mtk_phy_update_bits(reg, mask_, FIELD_PREP(mask_, val)); \
})

static inline int mtk_phy_mode_property_to_index(int submode)
{
	int index = -EINVAL;

	if (submode >= PHY_MODE_PROPERTY_SET && submode <= PHY_MODE_PROPERTY_SET_END)
		index = submode - PHY_MODE_PROPERTY_SET;

	return index;
}

static inline int mtk_phy_index_to_mode_property(int index)
{
	int submode = -EINVAL;

	if (index >= 0 && index < PHY_MODE_PROPERTY_MAX)
		submode = PHY_MODE_PROPERTY_SET + index;

	return submode;
}

#endif
