/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#ifndef __DRV_CLK_MTK_PLL_H
#define __DRV_CLK_MTK_PLL_H

#include <linux/clk-provider.h>

#define REG_CON0		                0
#define REG_CON1		                4

#define CON0_BASE_EN		                BIT(0)
#define CON0_PWR_ON		                BIT(0)
#define CON0_ISO_EN		                BIT(1)
#define PCW_CHG_MASK		                BIT(31)

#define AUDPLL_TUNER_EN		                BIT(31)

#define POSTDIV_MASK		                0x7

/* default 7 bits integer, can be overridden with pcwibits. */
#define INTEGER_BITS		                7

#define MTK_WAIT_HWV_PLL_PREPARE_CNT	        500
#define MTK_WAIT_HWV_PLL_PREPARE_US		1
#define MTK_WAIT_HWV_PLL_VOTE_CNT		100
#define MTK_WAIT_HWV_PLL_LONG_VOTE_CNT		2500
#define MTK_WAIT_HWV_PLL_VOTE_US		2
#define MTK_WAIT_HWV_PLL_DONE_CNT		100000
#define MTK_WAIT_HWV_PLL_DONE_US		1

#define MTK_WAIT_HWV_RES_PREPARE_CNT	        500
#define MTK_WAIT_HWV_RES_PREPARE_US		1
#define MTK_WAIT_HWV_RES_VOTE_CNT		1000
#define MTK_WAIT_HWV_RES_VOTE_US		2
#define MTK_WAIT_HWV_RES_DONE_CNT		100000
#define MTK_WAIT_HWV_RES_DONE_US		1

#define PLL_EN_TYPE				0
#define PLL_RSTB_TYPE				1

#define PLL_MMINFRA_VOTE_BIT		        26

struct mtk_clk_pll {
	struct clk_hw	hw;
	void __iomem	*base_addr;
	void __iomem	*pd_addr;
	void __iomem	*pwr_addr;
	void __iomem	*tuner_addr;
	void __iomem	*tuner_en_addr;
	void __iomem	*pcw_addr;
	void __iomem	*pcw_chg_addr;
	void __iomem	*en_addr;
	void __iomem	*en_set_addr;
	void __iomem	*en_clr_addr;
	void __iomem	*rstb_addr;
	void __iomem	*rstb_set_addr;
	void __iomem	*rstb_clr_addr;
	void __iomem	*fenc_addr;
	const struct mtk_pll_data *data;
	struct regmap	*hwv_regmap;
	unsigned int	en_msk;
	unsigned int	rstb_msk;
	unsigned int	fenc_msk;
	unsigned int	flags;
	unsigned int	onoff_cnt;
};

extern const struct clk_ops mtk_pll_ops;
extern const struct clk_ops mtk_pll_fenc_ops;
extern const struct clk_ops mtk_pll_setclr_ops;
extern const struct clk_ops mtk_hwv_pll_ops;
#endif  //__DRV_CLK_MTK_PLL_H