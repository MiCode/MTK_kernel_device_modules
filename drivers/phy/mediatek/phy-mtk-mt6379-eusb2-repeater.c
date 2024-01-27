// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek eUSB2 MT6379 Repeater Driver
 *
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/phy/phy.h>
#include <linux/delay.h>
#include <linux/bits.h>

/* MT6379 eUSB2 control registers */
#define RG_USB_EN_SRC_SEL		(1 << 5)
#define RG_USB20_BC11_SW_EN		(1 << 0)
#define RG_USB20_REV_A_5		(1 << 5)
#define RG_USB20_DISCTH		(1 << 7)
#define RG_USB20_HSRX_BIAS_EN_SEL	(1 << 2)
#define RG_FM_DUMMY			(1 << 4)

#define PHYA_COM_CR0_3			0x3
#define PHYA_U2_CR0_3			0x12
#define PHYD_COM_CR2_3			0x8b
#define PHYA_U2_CR2_0			0x0018
#define PHYA_U2_CR2_1			0x0019
#define PHYD_FM_CR0_1			0x00A5

/* MT6379 VID */
#define MT6379_REG_DEV_INFO	0x70
#define MT6379_VENID_MASK	GENMASK(7, 4)

struct eusb2_repeater {
	struct device *dev;
	struct regmap *regmap;
	struct phy *phy;
	u16 base;
	/* tuning parameter */
	int eusb2_squelch;
	int eusb2_equalization;
	int eusb2_output_swing;
	int eusb2_deemphasis;
	int squelch;
	int equalization;
	int ouput_swing;
	int deemphasis;
	int hs_termination;
	int discth;
	int fs_rise_fall;
	int hs_rise_fall;
	enum phy_mode mode;
};

static void eusb2_rptr_prop_parse(struct eusb2_repeater *rptr)
{

}

static void eusb2_rptr_prop_set(struct eusb2_repeater *rptr)
{

}

static int eusb2_repeater_init(struct phy *phy)
{
	struct eusb2_repeater *rptr = phy_get_drvdata(phy);
	int ret = 0;

	dev_info(rptr->dev, "MTK MT6379 eusb2 repeater init done\n");

	return ret;
}

static int eusb2_repeater_exit(struct phy *phy)
{
	struct eusb2_repeater *rptr = phy_get_drvdata(phy);

	dev_info(rptr->dev, "MTK MT6379 eusb2 repeater exit done\n");

	return 0;
}

static int eusb2_repeater_power_on(struct phy *phy)
{
	struct eusb2_repeater *rptr = phy_get_drvdata(phy);

	dev_info(rptr->dev, "eusb2 repeater power on\n");

	regmap_update_bits(rptr->regmap, rptr->base + 0x92, 0x7, 0x7);

	/* off */
	/* RG_USB_EN_SRC_SEL = 0x1 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_3, RG_USB_EN_SRC_SEL, RG_USB_EN_SRC_SEL);
	/* RG_USB20_BC11_SW_EN = 0x1 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3, RG_USB20_BC11_SW_EN, RG_USB20_BC11_SW_EN);
	/* RG_USB20_REV_A[5] = 0x0 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_3, RG_USB20_REV_A_5, 0);

	/* on */
	/* RG_USB_EN_SRC_SEL = 0x0 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_3, RG_USB_EN_SRC_SEL, 0);
	/* RG_USB20_BC11_SW_EN = 0x0 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3, RG_USB20_BC11_SW_EN, 0);
	/* RG_USB20_REV_A[5] = 0x1 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_3, RG_USB20_REV_A_5, RG_USB20_REV_A_5);

	/* The default value of HS disconnect threshold is too high, adjust HS DISC TH */
	/* (default 0x0D18 [7:4]= 4b'1110 to 0x0D18 [7:4]= 4b'0110) */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_0, RG_USB20_DISCTH, 0);

	/* FS enumeration fail, change the decoupling cap value in bias circuit */
	/* (default 0x0D18 [10:9]= 2b'10 to [10:9]= 2b'00, 0x0DA4 [15:12]= 4b'0000 to 0x0DA4 [15:12]= 4b'0001) */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR2_1, RG_USB20_HSRX_BIAS_EN_SEL, 0);
	regmap_update_bits(rptr->regmap, rptr->base + PHYD_FM_CR0_1, RG_FM_DUMMY, RG_FM_DUMMY);

	/* Set phy tuning */
	eusb2_rptr_prop_set(rptr);

	return 0;
}

static int eusb2_repeater_power_off(struct phy *phy)
{
	struct eusb2_repeater *rptr = phy_get_drvdata(phy);

	dev_info(rptr->dev, "eusb2 repeater power off\n");

	/* off */
	/* RG_USB_EN_SRC_SEL = 0x1 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYD_COM_CR2_3, RG_USB_EN_SRC_SEL, RG_USB_EN_SRC_SEL);
	/* RG_USB20_BC11_SW_EN = 0x1 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_COM_CR0_3, RG_USB20_BC11_SW_EN, RG_USB20_BC11_SW_EN);
	/* RG_USB20_REV_A[5] = 0x0 */
	regmap_update_bits(rptr->regmap, rptr->base + PHYA_U2_CR0_3, RG_USB20_REV_A_5, 0);

	return 0;
}

static int eusb2_repeater_set_mode(struct phy *phy,
				   enum phy_mode mode, int submode)
{
	/* struct eusb2_repeater *rptr = phy_get_drvdata(phy); */

	switch (mode) {
	case PHY_MODE_USB_HOST:
		break;
	case PHY_MODE_USB_DEVICE:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct phy_ops eusb2_repeater_ops = {
	.init		= eusb2_repeater_init,
	.exit		= eusb2_repeater_exit,
	.power_on	= eusb2_repeater_power_on,
	.power_off	= eusb2_repeater_power_off,
	.set_mode	= eusb2_repeater_set_mode,
	.owner		= THIS_MODULE,
};

static int eusb2_repeater_probe(struct platform_device *pdev)
{
	struct eusb2_repeater *rptr;
	struct device *dev = &pdev->dev;
	struct phy_provider *phy_provider;
	struct device_node *np = dev->of_node;
	u32 res;
	int ret;

	rptr = devm_kzalloc(dev, sizeof(*rptr), GFP_KERNEL);
	if (!rptr)
		return -ENOMEM;

	rptr->dev = dev;
	dev_set_drvdata(dev, rptr);

	rptr->regmap = dev_get_regmap(dev->parent, NULL);
	if (!rptr->regmap)
		return -ENODEV;

	ret = of_property_read_u32(np, "reg", &res);
	if (ret < 0)
		return ret;

	rptr->base = res;

	eusb2_rptr_prop_parse(rptr);

	rptr->phy = devm_phy_create(dev, np, &eusb2_repeater_ops);
	if (IS_ERR(rptr->phy)) {
		dev_info(dev, "failed to create PHY: %d\n", ret);
		return PTR_ERR(rptr->phy);
	}

	phy_set_drvdata(rptr->phy, rptr);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider))
		return PTR_ERR(phy_provider);

	dev_info(dev, "MTK MT6379 eusb2 repeater probe done\n");

	return 0;
}

static int eusb2_repeater_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id eusb2_repeater_of_match_table[] = {
	{.compatible = "mtk,mt6379-eusb2-repeater",},
	{},
};
MODULE_DEVICE_TABLE(of, eusb2_repeater_of_match_table);

static struct platform_driver eusb2_repeater_driver = {
	.driver = {
		.name = "mt6379-eusb2-repeater",
		.of_match_table = eusb2_repeater_of_match_table,
	},
	.probe = eusb2_repeater_probe,
	.remove = eusb2_repeater_remove,
};

module_platform_driver(eusb2_repeater_driver);

MODULE_DESCRIPTION("MediaTek eUSB2 MT6379 Repeater Driver");
MODULE_LICENSE("GPL");
