// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2024 Mediatek Inc.
// Copyright (C) 2024 Richtek Technology Corp.
// Author: ChiYuan Huang <cy_huang@richtek.com>

#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/reboot.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/spmi.h>
#include <linux/unaligned.h>

#define MT6688_REG_TOP_INFO1		0x00
#define MT6688_REG_TOP_CTRL1		0x04
#define MT6688_REG_TOP_STAT3		0x22
#define MT6688_REG_LDO_STAT1		0x23
#define MT6688_REG_BST_STAT1		0x25
#define MT6688_REG_TOP_ANA_2		0x51

#define MT6688_MASK_VENID		GENMASK(7, 4)
#define MT6688_MASK_REVISION		GENMASK(3, 0)
#define MT6688_MASK_WDTRSTB		BIT(2)
#define MT6688_MASK_OTP_ALM		BIT(2)
#define MT6688_MASK_OTP			BIT(1)
#define MT6688_MASK_LDO_OCPG(_rid)	BIT(8 - (_rid))
#define MT6688_MASK_DIG18_OV		BIT(3)
#define MT6688_MASK_VBST_OVP		BIT(6)
#define MT6688_MASK_LPMODE		BIT(3)
#define MT6688_MASK_VOSEL		GENMASK(7, 4)
#define MT6688_MASK_VOCAL		GENMASK(3, 0)
#define MT6688_MASK_EBITS		GENMASK(3, 0)
#define MT6688_MASK_VDIG18_VOBITS	GENMASK(7, 4)

#define MT6688_VENDOR_ID		0x80
#define MT6688_VOCAL_MAX		10
#define MT6688_CHIP_REV_E1		0
#define MT6688_CHIP_REV_E2		2
#define MT6688_CHIP_REV_E3		3

enum {
	MT6688_REGULATOR_VBST,
	MT6688_REGULATOR_AUX18,
	MT6688_REGULATOR_VCK12,
	MT6688_REGULATOR_VCK18,
	MT6688_REGULATOR_VDIG18,
	MT6688_REGULATOR_VRTC28,
	MT6688_MAX_REGULATOR
};

static int mt6688_spmi_read(void *sdev, const void *reg_buf, size_t reg_len, void *val_buf,
			    size_t val_len)
{
	WARN_ON_ONCE(reg_len != 2);
	return spmi_ext_register_readl(sdev, get_unaligned_be16(reg_buf), val_buf, val_len);
}

static int mt6688_spmi_write(void *sdev, const void *val_buf, size_t val_len)
{
	WARN_ON_ONCE(val_len < 2);
	return spmi_ext_register_writel(sdev, get_unaligned_be16(val_buf), val_buf + 2,
					val_len - 2);
}

static int mt6688_spmi_gather_write(void *sdev, const void *reg_buf, size_t reg_len,
				    const void *val_buf, size_t val_len)
{
	WARN_ON_ONCE(reg_len != 2);
	return spmi_ext_register_writel(sdev, get_unaligned_be16(reg_buf), val_buf, val_len);
}

static const struct regmap_bus mt6688_spmi_bus = {
	.read = mt6688_spmi_read,
	.write = mt6688_spmi_write,
	.gather_write = mt6688_spmi_gather_write,
	.max_raw_read = 2,
	.max_raw_write = 2,
	.fast_io = true,
};

static const struct regmap_config mt6688_regmap_config = {
	.name = "mt6688",
	.reg_bits = 16,
	.val_bits = 8,
	.reg_format_endian = REGMAP_ENDIAN_BIG,
	.max_register = 0x8ff,
};

static int mt6688_vbst_get_error_flags(struct regulator_dev *rdev, unsigned int *flags)
{
	struct regmap *regmap = rdev_get_regmap(rdev);
	unsigned int top_stat, bst_stat, rpt_flags = 0;
	int ret;

	ret = regmap_read(regmap, MT6688_REG_TOP_STAT3, &top_stat);
	ret |= regmap_read(regmap, MT6688_REG_BST_STAT1, &bst_stat);
	if (ret)
		return ret;

	if (top_stat & MT6688_MASK_OTP_ALM)
		rpt_flags |= REGULATOR_ERROR_OVER_TEMP_WARN;

	if (top_stat & MT6688_MASK_OTP)
		rpt_flags |= REGULATOR_ERROR_OVER_TEMP;

	if (bst_stat & MT6688_MASK_VBST_OVP)
		rpt_flags |= REGULATOR_ERROR_REGULATION_OUT;

	*flags = rpt_flags;

	return 0;
}

static const struct regulator_ops mt6688_regulator_vbst_ctrl = {
	.list_voltage = regulator_list_voltage_linear,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.get_error_flags = mt6688_vbst_get_error_flags,
};

static int mt6688_ldo_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	unsigned int mask = MT6688_MASK_LPMODE, val;

	switch (mode) {
	case REGULATOR_MODE_NORMAL:
		val = 0;
		break;
	case REGULATOR_MODE_IDLE:
		val = mask;
		break;
	default:
		return -EINVAL;
	}

	return regmap_update_bits(rdev_get_regmap(rdev), rdev->desc->enable_reg, mask, val);
}

static unsigned int mt6688_ldo_get_mode(struct regulator_dev *rdev)
{
	unsigned int val;
	int ret;

	ret = regmap_read(rdev_get_regmap(rdev), rdev->desc->enable_reg, &val);
	if (ret)
		return REGULATOR_MODE_INVALID;

	return val & MT6688_MASK_LPMODE ? REGULATOR_MODE_IDLE : REGULATOR_MODE_NORMAL;
}

static int mt6688_ldo_get_error_flags(struct regulator_dev *rdev, unsigned int *flags)
{
	struct regmap *regmap = rdev_get_regmap(rdev);
	unsigned int top_stat, ldo_stat[2], rpt_flags = 0;
	unsigned int pg_mask, oc_mask;
	int rid = rdev_get_id(rdev), ret;

	switch (rid) {
	case MT6688_REGULATOR_AUX18 ... MT6688_REGULATOR_VDIG18:
		pg_mask = oc_mask = MT6688_MASK_LDO_OCPG(rid);
		break;
	case MT6688_REGULATOR_VRTC28:
		pg_mask = MT6688_MASK_LDO_OCPG(rid);
		/* There's no OC event for VRTC28 */
		oc_mask = 0;
		break;
	default:
		return -EINVAL;
	}

	ret = regmap_read(regmap, MT6688_REG_TOP_STAT3, &top_stat);
	ret |= regmap_raw_read(regmap, MT6688_REG_BST_STAT1, ldo_stat, ARRAY_SIZE(ldo_stat));
	if (ret)
		return ret;

	if (top_stat & MT6688_MASK_OTP_ALM)
		rpt_flags |= REGULATOR_ERROR_OVER_TEMP_WARN;

	if (top_stat & MT6688_MASK_OTP)
		rpt_flags |= REGULATOR_ERROR_OVER_TEMP;

	if (ldo_stat[0] & pg_mask)
		rpt_flags |= REGULATOR_ERROR_FAIL;

	if (ldo_stat[1] & oc_mask)
		rpt_flags |= REGULATOR_ERROR_OVER_CURRENT;

	if (rid == MT6688_REGULATOR_VDIG18 && ldo_stat[1] & MT6688_MASK_DIG18_OV)
		rpt_flags |= REGULATOR_ERROR_REGULATION_OUT;

	*flags = rpt_flags;

	return 0;
}

static const struct regulator_ops mt6688_regulator_vck12_ctrl = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6688_ldo_set_mode,
	.get_mode = mt6688_ldo_get_mode,
	.get_error_flags = mt6688_ldo_get_error_flags,
};

static int mt6688_vck18_aux18_list_voltage(struct regulator_dev *rdev, unsigned int selector)
{
	unsigned int vocal, vosel;

	vocal = FIELD_GET(MT6688_MASK_VOCAL, selector);
	vosel = FIELD_GET(MT6688_MASK_VOSEL, selector);

	if (vocal > MT6688_VOCAL_MAX)
		vocal = MT6688_VOCAL_MAX;

	return rdev->desc->volt_table[vosel] + vocal * 10000;
}

static int mt6688_vck18_aux18_set_voltage_sel(struct regulator_dev *rdev, unsigned int selector)
{
	u8 volt_set[2];

	volt_set[0] = FIELD_GET(MT6688_MASK_VOCAL, selector);
	volt_set[1] = FIELD_GET(MT6688_MASK_VOSEL, selector);

	return regmap_raw_write(rdev_get_regmap(rdev), rdev->desc->vsel_reg, volt_set,
				ARRAY_SIZE(volt_set));
}

static int mt6688_vck18_aux18_get_voltage_sel(struct regulator_dev *rdev)
{
	u8 volt_get[2];
	int ret;

	ret = regmap_raw_read(rdev_get_regmap(rdev), rdev->desc->vsel_reg, volt_get,
			      ARRAY_SIZE(volt_get));
	if (ret)
		return ret;

	return ((volt_get[1] & MT6688_MASK_EBITS) << 4) | (volt_get[0] & MT6688_MASK_EBITS);
}

static const struct regulator_ops mt6688_regulator_vck18_aux18_ctrl = {
	.list_voltage = mt6688_vck18_aux18_list_voltage,
	.set_voltage_sel = mt6688_vck18_aux18_set_voltage_sel,
	.get_voltage_sel = mt6688_vck18_aux18_get_voltage_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6688_ldo_set_mode,
	.get_mode = mt6688_ldo_get_mode,
	.get_error_flags = mt6688_ldo_get_error_flags,
};

static int mt6688_vdig18_list_voltage(struct regulator_dev *rdev, unsigned int selector)
{
	unsigned int vosel, vocal;

	vosel = FIELD_GET(MT6688_MASK_VOSEL, selector);
	vocal = FIELD_GET(MT6688_MASK_VOCAL, selector);

	if (vocal > MT6688_VOCAL_MAX)
		vocal = MT6688_VOCAL_MAX;

	return rdev->desc->volt_table[vosel] + vocal * 10000;
}

static int mt6688_vdig18_set_voltage_sel(struct regulator_dev *rdev, unsigned int selector)
{
	int shift = ffs(MT6688_MASK_VDIG18_VOBITS) - 1, ret;
	struct regmap *regmap = rdev_get_regmap(rdev);
	u8 volt_set[2];

	ret = regmap_raw_read(regmap, rdev->desc->vsel_reg, volt_set, ARRAY_SIZE(volt_set));
	if (ret)
		return ret;

	/* Bytes [0] b'7:4 -> VOSEL, [1] b'7:4 -> VOCAL */
	volt_set[0] &= ~MT6688_MASK_VDIG18_VOBITS;
	volt_set[0] |= (FIELD_GET(MT6688_MASK_VOSEL, selector) << shift);
	volt_set[1] &= ~MT6688_MASK_VDIG18_VOBITS;
	volt_set[1] |= (FIELD_GET(MT6688_MASK_VOCAL, selector) << shift);

	return regmap_raw_write(regmap, rdev->desc->vsel_reg, volt_set, ARRAY_SIZE(volt_set));
}

static int mt6688_vdig18_get_voltage_sel(struct regulator_dev *rdev)
{
	unsigned int selector;
	u8 volt_get[2];
	int ret;

	ret = regmap_raw_read(rdev_get_regmap(rdev), rdev->desc->vsel_reg, volt_get,
			      ARRAY_SIZE(volt_get));
	if (ret)
		return ret;

	selector = volt_get[0] & MT6688_MASK_VDIG18_VOBITS;
	selector += FIELD_GET(MT6688_MASK_VDIG18_VOBITS, volt_get[1]);

	return selector;
}

static int mt6688_vdig18_e2_set_voltage_sel(struct regulator_dev *rdev, unsigned int selector)
{
	return regmap_write(rdev_get_regmap(rdev), rdev->desc->vsel_reg, selector);
}

static int mt6688_vdig18_e2_get_voltage_sel(struct regulator_dev *rdev)
{
	unsigned int selector;
	int ret;

	ret = regmap_read(rdev_get_regmap(rdev), rdev->desc->vsel_reg, &selector);
	if (ret)
		return ret;

	return selector;
}

static const struct regulator_ops mt6688_regulator_vdig18_ctrl = {
	.list_voltage = mt6688_vdig18_list_voltage,
	.set_voltage_sel = mt6688_vdig18_set_voltage_sel,
	.get_voltage_sel = mt6688_vdig18_get_voltage_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.get_error_flags = mt6688_ldo_get_error_flags,
};

static const struct regulator_ops mt6688_regulator_vdig18_e2_ctrl = {
	.list_voltage = mt6688_vdig18_list_voltage,
	.set_voltage_sel = mt6688_vdig18_e2_set_voltage_sel,
	.get_voltage_sel = mt6688_vdig18_e2_get_voltage_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.get_error_flags = mt6688_ldo_get_error_flags,
};

static const struct regulator_ops mt6688_regulator_vrtc28_ctrl = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.get_error_flags = mt6688_ldo_get_error_flags,
};

static unsigned int mt6688_ldo_of_map_mode(unsigned int mode)
{
	switch (mode) {
	case 0:
		return REGULATOR_MODE_NORMAL;
	case 1:
		return REGULATOR_MODE_IDLE;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

static const unsigned int mt6688_vck18_aux18_e1_volt_table[] = {
	1800000, 1900000, 2000000, 2100000, 2200000, 2300000, 2400000, 2500000,
	2600000, 2700000, 2800000, 2900000, 3000000, 3100000, 3200000, 3300000,
};

static const unsigned int mt6688_vdig18_e1_volt_table[] = {
	1200000, 1300000, 1500000, 1700000, 1800000, 2000000, 2100000, 2200000,
	2700000, 2800000, 2900000, 3000000, 3100000, 3300000, 3400000, 3500000,
};

static const unsigned int mt6688_vck18_aux18_e2_volt_table[] = {
	1800000, 1900000, 1900000, 1900000, 1900000, 1900000, 1900000, 1900000,
	1900000, 1900000, 1900000, 1900000, 1900000, 1900000, 1900000, 1900000,
};

static const unsigned int mt6688_vdig18_e2_volt_table[] = {
	1700000, 1700000, 1700000, 1700000, 1800000, 1800000, 1800000, 1800000,
	1800000, 1800000, 1800000, 1800000, 1800000, 1800000, 1800000, 1800000,
};

#define MT6688_ALL_REGULATOR_DESC(_vck18_aux18_table, _vdig18_table, _vdig18_regulator_ops) \
{\
	{\
		.name = "mt6688-vbst",\
		.of_match = "vbst",\
		.supply_name = "vbb",\
		.regulators_node = "regulators",\
		.id = MT6688_REGULATOR_VBST,\
		.ops = &mt6688_regulator_vbst_ctrl,\
		.type = REGULATOR_VOLTAGE,\
		.owner = THIS_MODULE,\
		.n_voltages = 12,\
		.linear_min_sel = 1,\
		.min_uV = 2900000,\
		.uV_step = 100000,\
		.vsel_reg = 0x501,\
		.vsel_mask = GENMASK(7, 4),\
		.enable_reg = 0x500,\
		.enable_mask = BIT(7),\
	},\
	{\
		.name = "mt6688-aux18",\
		.of_match = "aux18",\
		.supply_name = "vbb",\
		.regulators_node = "regulators",\
		.id = MT6688_REGULATOR_AUX18,\
		.ops = &mt6688_regulator_vck18_aux18_ctrl,\
		.type = REGULATOR_VOLTAGE,\
		.owner = THIS_MODULE,\
		.n_voltages = 256,\
		.volt_table = _vck18_aux18_table,\
		.vsel_reg = 0x654,\
		.enable_reg = 0x640,\
		.enable_mask = BIT(2),\
		.of_map_mode = mt6688_ldo_of_map_mode,\
	},\
	{\
		.name = "mt6688-vck12",\
		.of_match = "vck12",\
		.supply_name = "vs2",\
		.regulators_node = "regulators",\
		.id = MT6688_REGULATOR_VCK12,\
		.ops = &mt6688_regulator_vck12_ctrl,\
		.type = REGULATOR_VOLTAGE,\
		.owner = THIS_MODULE,\
		.n_voltages = 1,\
		.fixed_uV = 1200000,\
		.enable_reg = 0x600,\
		.enable_mask = BIT(2),\
		.of_map_mode = mt6688_ldo_of_map_mode,\
	},\
	{\
		.name = "mt6688-vck18",\
		.of_match = "vck18",\
		.supply_name = "vbb",\
		.regulators_node = "regulators",\
		.id = MT6688_REGULATOR_VCK18,\
		.ops = &mt6688_regulator_vck18_aux18_ctrl,\
		.type = REGULATOR_VOLTAGE,\
		.owner = THIS_MODULE,\
		.n_voltages = 256,\
		.volt_table = _vck18_aux18_table,\
		.vsel_reg = 0x634,\
		.enable_reg = 0x620,\
		.enable_mask = BIT(2),\
		.of_map_mode = mt6688_ldo_of_map_mode,\
	},\
	{\
		.name = "mt6688-vdig18",\
		.of_match = "vdig18",\
		.supply_name = "vbb",\
		.regulators_node = "regulators",\
		.id = MT6688_REGULATOR_VDIG18,\
		.ops = &_vdig18_regulator_ops,\
		.type = REGULATOR_VOLTAGE,\
		.owner = THIS_MODULE,\
		.n_voltages = 256,\
		.volt_table = _vdig18_table,\
		.vsel_reg = 0x661,\
		.enable_reg = 0x663,\
		.enable_mask = BIT(3),\
	},\
	{\
		.name = "mt6688-vrtc28",\
		.of_match = "vrtc28",\
		.regulators_node = "regulators",\
		.id = MT6688_REGULATOR_VRTC28,\
		.ops = &mt6688_regulator_vrtc28_ctrl,\
		.type = REGULATOR_VOLTAGE,\
		.owner = THIS_MODULE,\
		.n_voltages = 1,\
		.fixed_uV = 2800000,\
		.enable_reg = 0x670,\
		.enable_mask = BIT(1),\
	},\
}

static const struct regulator_desc mt6688_e1_regulator_desc[MT6688_MAX_REGULATOR] =
	MT6688_ALL_REGULATOR_DESC(mt6688_vck18_aux18_e1_volt_table, mt6688_vdig18_e1_volt_table,
				  mt6688_regulator_vdig18_ctrl);

static const struct regulator_desc mt6688_e2_regulator_desc[MT6688_MAX_REGULATOR] =
	MT6688_ALL_REGULATOR_DESC(mt6688_vck18_aux18_e2_volt_table, mt6688_vdig18_e2_volt_table,
				  mt6688_regulator_vdig18_e2_ctrl);

static ssize_t ecid_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct regmap *regmap = dev_get_regmap(dev, NULL);
	static bool already_got;
	static u8 val[3];
	int ret = 0;

	if (!regmap)
		return -EINVAL;

	if (already_got)
		goto out;

	ret = regmap_bulk_read(regmap, MT6688_REG_TOP_ANA_2, &val, 3);
	if (ret) {
		dev_info(dev, "%s, Failed to get ecid data (ret:%d)\n", __func__, ret);
		return sysfs_emit(buf, "Failed_To_Get_Ecid_ret_%d\n", ret);
	}

	already_got = true;

out:
	dev_info(dev, "%s, MT6688_ECID=0x%02X,0x%02X,0x%02X\n", __func__, val[0], val[1], val[2]);

	return sysfs_emit(buf, "MT6688_ECID_0x%02X_0x%02X_0x%02X\n", val[0], val[1], val[2]);
}
static DEVICE_ATTR_RO(ecid);

static void mt6688_destroy_ecid_attr(void *data)
{
	struct device *dev = data;

	device_remove_file(dev, &dev_attr_ecid);
}

static int mt6688_probe(struct spmi_device *sdev)
{
	const struct regulator_desc *desc;
	struct device *dev = &sdev->dev;
	struct regulator_config cfg = { .dev = dev };
	struct regulator_dev *rdev;
	struct regmap *regmap;
	unsigned int venid;
	int i, ret;

	regmap = devm_regmap_init(dev, &mt6688_spmi_bus, sdev, &mt6688_regmap_config);
	if (IS_ERR(regmap))
		return dev_err_probe(dev, PTR_ERR(regmap), "Failed to init regmap\n");

	ret = regmap_read(regmap, MT6688_REG_TOP_INFO1, &venid);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to read vendor information\n");

	if ((venid & MT6688_MASK_VENID) != MT6688_VENDOR_ID)
		return dev_err_probe(dev, -ENODEV, "Incorrect vendor id (0x%02x)\n", venid);

	dev_info(dev, "VENID -> 0x%02x\n", venid);

	switch (FIELD_GET(MT6688_MASK_REVISION, venid)) {
	case MT6688_CHIP_REV_E1:
		desc = mt6688_e1_regulator_desc;
		break;
	default:
		desc = mt6688_e2_regulator_desc;
		break;
	}

	for (i = 0; i < MT6688_MAX_REGULATOR; i++) {
		rdev = devm_regulator_register(dev, desc + i, &cfg);
		if (IS_ERR(rdev)) {
			ret = PTR_ERR(rdev);
			return dev_err_probe(dev, ret, "Failed to register (%d) regulator\n", i);
		}
	}

	ret = device_create_file(dev, &dev_attr_ecid);
	if (ret)
		dev_info(dev, "Failed to create ECID device attr (ret:%d)\n", ret);

	ret = devm_add_action_or_reset(dev, mt6688_destroy_ecid_attr, dev);
	if (ret)
		dev_info(dev, "Failed to add ECID device attr to devm list (ret:%d)\n", ret);

	/* Used for SPAR & connsys device to register */
	return devm_of_platform_populate(dev);
}

static const struct of_device_id mt6688_dev_match_table[] = {
	{ .compatible = "mediatek,mt6688" },
	{}
};
MODULE_DEVICE_TABLE(of, mt6688_dev_match_table);

static struct spmi_driver mt6688_driver = {
	.driver = {
		.name = "mt6688",
		.of_match_table = mt6688_dev_match_table,
	},
	.probe = mt6688_probe,
};
module_spmi_driver(mt6688_driver);

MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("Mediatek MT6688 voltage regulator driver");
MODULE_LICENSE("GPL");
