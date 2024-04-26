// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2022 MediaTek Inc.
 * Copyright (c) 2022 BayLibre
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define EDP_PHYD_BASE_ADDR		0x130A0000


#define PHYD_OFFSET			0x0000
#define PHYD_DIG_LAN0_OFFSET	0x1000
#define PHYD_DIG_LAN1_OFFSET	0x1100
#define PHYD_DIG_LAN2_OFFSET	0x1200
#define PHYD_DIG_LAN3_OFFSET	0x1300
#define PHYD_DIG_GLB_OFFSET	    0x1400


#define IPMUX_CONTROL			(PHYD_DIG_GLB_OFFSET + 0x98)
#define EDPTX_DSI_PHYD_SEL_FLDMASK		0x1
#define EDPTX_DSI_PHYD_SEL_FLDMASK_POS		0

#define DP_PHY_DIG_TX_CTL_0		(PHYD_DIG_GLB_OFFSET + 0x74)
#define TX_LN_EN_FLDMASK				0xf


#define MTK_DP_PHY_DIG_PLL_CTL_1	(PHYD_DIG_GLB_OFFSET + 0x14)
#define TPLL_SSC_EN			BIT(8)

#define MTK_DP_PHY_DIG_BIT_RATE		(PHYD_DIG_GLB_OFFSET + 0x3C)
#define BIT_RATE_RBR			0x1
#define BIT_RATE_HBR			0x4
#define BIT_RATE_HBR2			0x7
#define BIT_RATE_HBR3			0x9

#define MTK_DP_PHY_DIG_SW_RST		(PHYD_DIG_GLB_OFFSET + 0x38)
#define DP_GLB_SW_RST_PHYD		BIT(0)
#define DP_GLB_SW_RST_PHYD_MASK		BIT(0)

#define DRIVING_FORCE 0x30
#define EDP_TX_LN_VOLT_SWING_VAL_FLDMASK                                0x6
#define EDP_TX_LN_VOLT_SWING_VAL_FLDMASK_POS                            1
#define EDP_TX_LN_PRE_EMPH_VAL_FLDMASK                                  0x18
#define EDP_TX_LN_PRE_EMPH_VAL_FLDMASK_POS                              3



#define MTK_DP_LANE0_DRIVING_PARAM_3		(PHYD_OFFSET + 0x138)
#define MTK_DP_LANE1_DRIVING_PARAM_3		(PHYD_OFFSET + 0x238)
#define MTK_DP_LANE2_DRIVING_PARAM_3		(PHYD_OFFSET + 0x338)
#define MTK_DP_LANE3_DRIVING_PARAM_3		(PHYD_OFFSET + 0x438)
#define XTP_LN_TX_LCTXC0_SW0_PRE0_DEFAULT	BIT(4)
#define XTP_LN_TX_LCTXC0_SW0_PRE1_DEFAULT	(BIT(10) | BIT(12))
#define XTP_LN_TX_LCTXC0_SW0_PRE2_DEFAULT	GENMASK(20, 19)
#define XTP_LN_TX_LCTXC0_SW0_PRE3_DEFAULT	GENMASK(29, 29)
#define DRIVING_PARAM_3_DEFAULT	(XTP_LN_TX_LCTXC0_SW0_PRE0_DEFAULT | \
				 XTP_LN_TX_LCTXC0_SW0_PRE1_DEFAULT | \
				 XTP_LN_TX_LCTXC0_SW0_PRE2_DEFAULT | \
				 XTP_LN_TX_LCTXC0_SW0_PRE3_DEFAULT)

#define XTP_LN_TX_LCTXC0_SW1_PRE0_DEFAULT	GENMASK(4, 3)
#define XTP_LN_TX_LCTXC0_SW1_PRE1_DEFAULT	GENMASK(12, 9)
#define XTP_LN_TX_LCTXC0_SW1_PRE2_DEFAULT	(BIT(18) | BIT(21))
#define XTP_LN_TX_LCTXC0_SW2_PRE0_DEFAULT	GENMASK(29, 29)
#define DRIVING_PARAM_4_DEFAULT	(XTP_LN_TX_LCTXC0_SW1_PRE0_DEFAULT | \
				 XTP_LN_TX_LCTXC0_SW1_PRE1_DEFAULT | \
				 XTP_LN_TX_LCTXC0_SW1_PRE2_DEFAULT | \
				 XTP_LN_TX_LCTXC0_SW2_PRE0_DEFAULT)

#define XTP_LN_TX_LCTXC0_SW2_PRE1_DEFAULT	(BIT(3) | BIT(5))
#define XTP_LN_TX_LCTXC0_SW3_PRE0_DEFAULT	GENMASK(13, 12)
#define DRIVING_PARAM_5_DEFAULT	(XTP_LN_TX_LCTXC0_SW2_PRE1_DEFAULT | \
				 XTP_LN_TX_LCTXC0_SW3_PRE0_DEFAULT)

#define XTP_LN_TX_LCTXCP1_SW0_PRE0_DEFAULT	0
#define XTP_LN_TX_LCTXCP1_SW0_PRE1_DEFAULT	GENMASK(10, 10)
#define XTP_LN_TX_LCTXCP1_SW0_PRE2_DEFAULT	GENMASK(19, 19)
#define XTP_LN_TX_LCTXCP1_SW0_PRE3_DEFAULT	GENMASK(28, 28)
#define DRIVING_PARAM_6_DEFAULT	(XTP_LN_TX_LCTXCP1_SW0_PRE0_DEFAULT | \
				 XTP_LN_TX_LCTXCP1_SW0_PRE1_DEFAULT | \
				 XTP_LN_TX_LCTXCP1_SW0_PRE2_DEFAULT | \
				 XTP_LN_TX_LCTXCP1_SW0_PRE3_DEFAULT)

#define XTP_LN_TX_LCTXCP1_SW1_PRE0_DEFAULT	0
#define XTP_LN_TX_LCTXCP1_SW1_PRE1_DEFAULT	GENMASK(10, 9)
#define XTP_LN_TX_LCTXCP1_SW1_PRE2_DEFAULT	GENMASK(19, 18)
#define XTP_LN_TX_LCTXCP1_SW2_PRE0_DEFAULT	0
#define DRIVING_PARAM_7_DEFAULT	(XTP_LN_TX_LCTXCP1_SW1_PRE0_DEFAULT | \
				 XTP_LN_TX_LCTXCP1_SW1_PRE1_DEFAULT | \
				 XTP_LN_TX_LCTXCP1_SW1_PRE2_DEFAULT | \
				 XTP_LN_TX_LCTXCP1_SW2_PRE0_DEFAULT)

#define XTP_LN_TX_LCTXCP1_SW2_PRE1_DEFAULT	GENMASK(3, 3)
#define XTP_LN_TX_LCTXCP1_SW3_PRE0_DEFAULT	0
#define DRIVING_PARAM_8_DEFAULT	(XTP_LN_TX_LCTXCP1_SW2_PRE1_DEFAULT | \
				 XTP_LN_TX_LCTXCP1_SW3_PRE0_DEFAULT)

struct mtk_dp_phy {
	struct regmap *regs;
};

static const struct regmap_config edp_phyd_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

enum {
	DPTX_SWING0	= 0x00,
	DPTX_SWING1	= 0x01,
	DPTX_SWING2	= 0x02,
	DPTX_SWING3	= 0x03,
};

enum {
	DPTX_PREEMPHASIS0	= 0x00,
	DPTX_PREEMPHASIS1	= 0x01,
	DPTX_PREEMPHASIS2	= 0x02,
	DPTX_PREEMPHASIS3	= 0x03,
};

enum {
	DPTX_LANE0 = 0x0,
	DPTX_LANE1 = 0x1,
	DPTX_LANE2 = 0x2,
	DPTX_LANE3 = 0x3,
	DPTX_LANE_MAX,
};

enum {
	DPTX_LANE_COUNT1 = 0x1,
	DPTX_LANE_COUNT2 = 0x2,
	DPTX_LANE_COUNT4 = 0x4,
};

static void mtk_dptx_phyd_reset_swing_pre(struct mtk_dp_phy *dp_phy)
{
	regmap_update_bits(dp_phy->regs, PHYD_DIG_LAN0_OFFSET + DRIVING_FORCE,
			 EDP_TX_LN_VOLT_SWING_VAL_FLDMASK | EDP_TX_LN_PRE_EMPH_VAL_FLDMASK, 0x0);
	regmap_update_bits(dp_phy->regs, PHYD_DIG_LAN1_OFFSET + DRIVING_FORCE,
			 EDP_TX_LN_VOLT_SWING_VAL_FLDMASK | EDP_TX_LN_PRE_EMPH_VAL_FLDMASK, 0x0);
	regmap_update_bits(dp_phy->regs, PHYD_DIG_LAN2_OFFSET + DRIVING_FORCE,
			 EDP_TX_LN_VOLT_SWING_VAL_FLDMASK | EDP_TX_LN_PRE_EMPH_VAL_FLDMASK, 0x0);
	regmap_update_bits(dp_phy->regs, PHYD_DIG_LAN3_OFFSET + DRIVING_FORCE,
			 EDP_TX_LN_VOLT_SWING_VAL_FLDMASK | EDP_TX_LN_PRE_EMPH_VAL_FLDMASK, 0x0);
}


static int mtk_dp_phy_init(struct phy *phy)
{
	struct mtk_dp_phy *dp_phy = phy_get_drvdata(phy);

	/* Set IPMUX_CONTROL to eDP Mode */
	regmap_update_bits(dp_phy->regs, IPMUX_CONTROL,
						0 << EDPTX_DSI_PHYD_SEL_FLDMASK_POS,
						EDPTX_DSI_PHYD_SEL_FLDMASK);

	regmap_update_bits(dp_phy->regs, PHYD_DIG_GLB_OFFSET + 0x10,
						BIT(0) | BIT(1) | BIT(2),
						BIT(0) | BIT(1) | BIT(2));

	return 0;
}

static int mtk_dp_phy_configure(struct phy *phy, union phy_configure_opts *opts)
{
	struct mtk_dp_phy *dp_phy = phy_get_drvdata(phy);
	u32 val;

	if (opts->dp.set_rate) {
		switch (opts->dp.link_rate) {
		default:
			dev_info(&phy->dev,
				"Implementation error, unknown linkrate %x\n",
				opts->dp.link_rate);
			return -EINVAL;
		case 1620:
			val = BIT_RATE_RBR;
			break;
		case 2700:
			val = BIT_RATE_HBR;
			break;
		case 5400:
			val = BIT_RATE_HBR2;
			break;
		case 8100:
			val = BIT_RATE_HBR3;
			break;
		}
		regmap_write(dp_phy->regs, MTK_DP_PHY_DIG_BIT_RATE, val);
	}

	if (opts->dp.set_lanes) {
		for (val = 0; val < 4; val++) {
			regmap_update_bits(dp_phy->regs, DP_PHY_DIG_TX_CTL_0,
				((1 << (val + 1)) - 1), TX_LN_EN_FLDMASK);
		}
	}

	/* set swing and pre */
	if (opts->dp.set_voltages) {
		if (opts->dp.lanes >= DPTX_LANE_COUNT1) {
			regmap_update_bits(dp_phy->regs,
				PHYD_DIG_LAN0_OFFSET + DRIVING_FORCE,
				EDP_TX_LN_VOLT_SWING_VAL_FLDMASK |
				EDP_TX_LN_PRE_EMPH_VAL_FLDMASK,
				opts->dp.voltage[DPTX_LANE0] << 1 |
				opts->dp.pre[DPTX_LANE0] << 3);

			if (opts->dp.lanes >= DPTX_LANE_COUNT2) {
				regmap_update_bits(dp_phy->regs,
					PHYD_DIG_LAN1_OFFSET + DRIVING_FORCE,
					EDP_TX_LN_VOLT_SWING_VAL_FLDMASK |
					EDP_TX_LN_PRE_EMPH_VAL_FLDMASK,
					opts->dp.voltage[DPTX_LANE1] << 1 |
					opts->dp.pre[DPTX_LANE1] << 3);

				if (opts->dp.lanes == DPTX_LANE_COUNT4) {
					regmap_update_bits(dp_phy->regs,
						PHYD_DIG_LAN2_OFFSET + DRIVING_FORCE,
						EDP_TX_LN_VOLT_SWING_VAL_FLDMASK |
						EDP_TX_LN_PRE_EMPH_VAL_FLDMASK,
						opts->dp.voltage[DPTX_LANE2] << 1 |
						opts->dp.pre[DPTX_LANE2] << 3);

					regmap_update_bits(dp_phy->regs,
						PHYD_DIG_LAN3_OFFSET + DRIVING_FORCE,
						EDP_TX_LN_VOLT_SWING_VAL_FLDMASK |
						EDP_TX_LN_PRE_EMPH_VAL_FLDMASK,
						opts->dp.voltage[DPTX_LANE3] << 1 |
						opts->dp.pre[DPTX_LANE3] << 3);
				}
			}
		}
	}

	regmap_update_bits(dp_phy->regs, MTK_DP_PHY_DIG_PLL_CTL_1,
			   TPLL_SSC_EN, opts->dp.ssc ? 0 : TPLL_SSC_EN);

	return 0;
}

static int mtk_dp_phy_reset(struct phy *phy)
{
	struct mtk_dp_phy *dp_phy = phy_get_drvdata(phy);
	unsigned int val;

	regmap_update_bits(dp_phy->regs, MTK_DP_PHY_DIG_SW_RST,
			   0, DP_GLB_SW_RST_PHYD_MASK);
	usleep_range(50, 200);
	regmap_update_bits(dp_phy->regs, MTK_DP_PHY_DIG_SW_RST,
			   DP_GLB_SW_RST_PHYD, DP_GLB_SW_RST_PHYD_MASK);

	regmap_read(dp_phy->regs,DP_PHY_DIG_TX_CTL_0, &val);
	val = val & TX_LN_EN_FLDMASK;
	pr_info("[eDPTX] Current lane power %x\n", val);

	while (val > 0) {
		val >>= 1;
		regmap_update_bits(dp_phy->regs, DP_PHY_DIG_TX_CTL_0,
			val, TX_LN_EN_FLDMASK);
	}

	mtk_dptx_phyd_reset_swing_pre(dp_phy);

	return 0;
}

static const struct phy_ops mtk_dp_phy_dev_ops = {
	.init = mtk_dp_phy_init,
	.configure = mtk_dp_phy_configure,
	.reset = mtk_dp_phy_reset,
	.owner = THIS_MODULE,
};

static int mtk_dp_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_dp_phy *dp_phy;
	struct phy *phy;
	struct regmap *regs;
	void __iomem *regs_base;

	regs_base = ioremap(EDP_PHYD_BASE_ADDR, 0x1500);
	if (!regs_base) {
		pr_info("[eDPTX] Unable to map register.\n");
		return -ENOMEM;
	}

	regs = devm_regmap_init_mmio(dev, regs_base, &edp_phyd_regmap_config);
	if (IS_ERR(regs)) {
		pr_info("[eDPTX] No data passed, requires struct regmap\n");
		return -EINVAL;
	}

	dp_phy = devm_kzalloc(dev, sizeof(*dp_phy), GFP_KERNEL);
	if (!dp_phy)
		return -ENOMEM;

	dp_phy->regs = regs;
	phy = devm_phy_create(dev, NULL, &mtk_dp_phy_dev_ops);
	if (IS_ERR(phy)) {
		pr_info("Failed to create DP PHY\n");
		return PTR_ERR(phy);
	}

	phy_set_drvdata(phy, dp_phy);
	if (!dev->of_node)
		phy_create_lookup(phy, "edp", dev_name(dev));

	return 0;
}

struct platform_driver mtk_dp_phy_driver = {
	.probe = mtk_dp_phy_probe,
	.driver = {
		.name = "mediatek-edp-phy",
	},
};

MODULE_AUTHOR("Markus Schneider-Pargmann <msp@baylibre.com>");
MODULE_DESCRIPTION("MediaTek DP PHY Driver");
MODULE_LICENSE("GPL");
