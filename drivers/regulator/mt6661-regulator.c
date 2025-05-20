// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2024 MediaTek Inc.

#include <linux/interrupt.h>
#include <linux/math.h>
#include <linux/mfd/mt6661/registers.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/mt6661-regulator.h>
#include <linux/regulator/of_regulator.h>

#define MT6661_HWCID0_E1_CODE	0x10
#define MT6661_HWCID0_E2_CODE	0x20

#define SET_OFFSET		0x1
#define CLR_OFFSET		0x2
#define HW_NORMAL_OP_EN		0x2
#define STRGINGIFY(x)	#x
#define CONCAT_AND_STRGINGIFY(x, y) STRGINGIFY(x##_##y)

#define MT6661_REGULATOR_MODE_NORMAL	0
#define MT6661_REGULATOR_MODE_FCCM	1
#define MT6661_REGULATOR_MODE_LP	2
#define MT6661_REGULATOR_MODE_ULP	3

#define DEFAULT_DELAY_MS		10
#define BUF_SIZE			20

/*
 * MT6661 regulator lock register
 */
#define MT6661_TMA_UNLOCK_VALUE		0x999E
#define MT6661_BUCK_TOP_UNLOCK_VALUE	0x5543

/*
 * MT6661 regulators' information
 *
 * @desc: standard fields of regulator description.
 * @lp_mode_reg: for operating NORMAL/IDLE mode register.
 * @lp_mode_mask: MASK for operating lp_mode register.
 * @hw_lp_mode_reg: hardware NORMAL/IDLE mode status register.
 * @hw_lp_mode_mask: MASK for hardware NORMAL/IDLE mode status register.
 * @modeset_reg: for operating AUTO/PWM mode register.
 * @modeset_mask: MASK for operating modeset register.
 * @vocal_reg: Calibrates output voltage register.
 * @vocal_mask: MASK of Calibrates output voltage register.
 * @lp_imax_uA: Maximum load current in Low power mode.
 * @hw_op_en_reg: for HW control operating mode register.
 */
struct mt6661_regulator_info {
	int irq;
	int lp_irq;
	int oc_irq_enable_delay_ms;
	struct delayed_work oc_work;
	struct regulator_desc desc;
	u32 max_uV;
	u32 lp_mode_reg;
	u32 lp_mode_mask;
	u32 modeset_reg;
	u32 modeset_mask;
	u32 vocal_reg;
	u32 vocal_mask;
	u32 vocal_unit;
	u32 da_en_reg;
	u32 da_en_mask;
	u32 da_lp_mask;
	u32 eint_en_reg;
	u32 eint_en_mask;
	u32 pol_mask;
	int lp_imax_uA;
	u32 hw_op_en_reg;
};

#define MT6661_BUCK(_name, _min, _max, _step, _volt_ranges,	\
		    _enable_reg, _en_bit, _vsel_reg, _vsel_mask,\
		    _lp_mode_reg, _lp_bit,			\
		    _modeset_reg, modeset_bit)			\
[MT6661_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(#_name),		\
		.of_parse_cb = mt6661_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &mt6661_buck_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6661_ID_##_name,			\
		.min_uV = _min,					\
		.uV_step = _step,				\
		.owner = THIS_MODULE,				\
		.n_voltages = ((_max) - (_min)) / (_step) + 1,	\
		.linear_ranges = _volt_ranges,			\
		.n_linear_ranges = ARRAY_SIZE(_volt_ranges),	\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(_en_bit),			\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
		.of_map_mode = mt6661_map_mode,			\
	},							\
	.max_uV = _max,						\
	.lp_mode_reg = _lp_mode_reg,				\
	.lp_mode_mask = BIT(_lp_bit),				\
	.da_en_reg = MT6661_DA_##_name##_EN_ADDR,		\
	.da_en_mask = BIT(0),					\
	.da_lp_mask = 0xc,					\
	.modeset_reg = _modeset_reg,				\
	.modeset_mask = BIT(modeset_bit),			\
	.lp_imax_uA = 3000000,					\
	.hw_op_en_reg = MT6661_##_name##_OP_EN_1,		\
}

#define MT6661_SSHUB(_name, _min, _max, _step, _volt_ranges,	\
		     _enable_reg, _vsel_reg, _vsel_mask)	\
[MT6661_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(#_name),		\
		.of_parse_cb = mt6661_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &mt6661_sshub_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6661_ID_##_name,			\
		.owner = THIS_MODULE,				\
		.n_voltages = ((_max) - (_min)) / (_step) + 1,	\
		.linear_ranges = _volt_ranges,			\
		.n_linear_ranges = ARRAY_SIZE(_volt_ranges),	\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(0),				\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
	},							\
}

#define MT6661_LDO_LINEAR(_name, _min, _max, _step, _volt_ranges, \
			  _enable_reg, _en_bit, _vsel_reg,	\
			  _vsel_mask, _vocal_reg, _vocal_mask,	\
			  _lp_mode_reg, _lp_bit)		\
[MT6661_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(#_name),		\
		.of_parse_cb = mt6661_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &mt6661_volt_range_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6661_ID_##_name,			\
		.min_uV = _min,					\
		.uV_step = _step,				\
		.owner = THIS_MODULE,				\
		.n_voltages = ((_max) - (_min)) / (_step) + 1,	\
		.linear_ranges = _volt_ranges,			\
		.n_linear_ranges = ARRAY_SIZE(_volt_ranges),	\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(_en_bit),			\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
		.of_map_mode = mt6661_map_mode,			\
	},							\
	.max_uV = _max,						\
	.vocal_reg = _vocal_reg,				\
	.vocal_mask = _vocal_mask,				\
	.vocal_unit = 10000,					\
	.da_en_reg = MT6661_DA_##_name##_B_EN_ADDR,		\
	.da_en_mask = 0x3,					\
	.da_lp_mask = 0x7,					\
	.lp_mode_reg = _lp_mode_reg,				\
	.lp_mode_mask = BIT(_lp_bit),				\
	.lp_imax_uA = 100000,					\
	.hw_op_en_reg = MT6661_LDO_##_name##_HW_OP_EN0,		\
}

#define MT6661_LDO(_name, _volt_table, _enable_reg, _en_bit,	\
		   _vsel_reg, _vsel_mask, _vocal_reg,		\
		   _vocal_mask, _lp_mode_reg, _lp_bit)		\
[MT6661_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(#_name),		\
		.of_parse_cb = mt6661_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &mt6661_volt_table_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6661_ID_##_name,			\
		.owner = THIS_MODULE,				\
		.n_voltages = ARRAY_SIZE(_volt_table),		\
		.volt_table = _volt_table,			\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(_en_bit),			\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
		.of_map_mode = mt6661_map_mode,			\
	},							\
	.vocal_reg = _vocal_reg,				\
	.vocal_mask = _vocal_mask,				\
	.da_en_reg = MT6661_DA_##_name##_B_EN_ADDR,		\
	.da_en_mask = 0x3,					\
	.da_lp_mask = 0x7,					\
	.lp_mode_reg = _lp_mode_reg,				\
	.lp_mode_mask = BIT(_lp_bit),				\
	.da_lp_mask = 0x4,					\
	.lp_imax_uA = 100000,					\
	.hw_op_en_reg = MT6661_LDO_##_name##_HW_OP_EN0,		\
}

#define MT6661_EINT(_name, _eint_pol, _volt_table,		\
		    _enable_reg, _en_bit, _vsel_reg, _vsel_mask,\
		    _vocal_reg, _vocal_mask, _lp_mode_reg, _lp_bit, \
		    _eint_en_reg, _eint_en_bit, _pol_bit)	\
[MT6661_ID_##_name##_##_eint_pol] = {				\
	.desc = {						\
		.name = CONCAT_AND_STRGINGIFY(_name, _eint_pol),\
		.of_match = of_match_ptr(CONCAT_AND_STRGINGIFY(_name, _eint_pol)), \
		.of_parse_cb = mt6661_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &mt6661_eint_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6661_ID_##_name##_##_eint_pol,		\
		.owner = THIS_MODULE,				\
		.n_voltages = ARRAY_SIZE(_volt_table),		\
		.volt_table = _volt_table,			\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(_en_bit),			\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
		.of_map_mode = mt6661_map_mode,			\
	},							\
	.vocal_reg = _vocal_reg,				\
	.vocal_mask = _vocal_mask,				\
	.lp_mode_reg = _lp_mode_reg,				\
	.lp_mode_mask = BIT(_lp_bit),				\
	.eint_en_reg = _eint_en_reg,				\
	.eint_en_mask = BIT(_eint_en_bit),			\
	.pol_mask = BIT(_pol_bit),				\
}

static const struct linear_range mt_volt_range0[] = {
	REGULATOR_LINEAR_RANGE(0, 0, 240, 5000),
};

static const struct linear_range mt_volt_range1[] = {
	REGULATOR_LINEAR_RANGE(500000, 0, 15, 100000),
};

static const unsigned int ldo_volt_table0[] = {
	1200000, 1500000, 1800000, 2000000, 2400000, 2500000, 2600000, 2700000,
	2800000, 2900000, 3000000, 3100000, 3200000, 3300000, 3400000, 3500000,
};

static int mt6661_buck_enable(struct regulator_dev *rdev)
{
	return regmap_write(rdev->regmap, rdev->desc->enable_reg + SET_OFFSET,
			    rdev->desc->enable_mask);
}

static int mt6661_buck_disable(struct regulator_dev *rdev)
{
	return regmap_write(rdev->regmap, rdev->desc->enable_reg + CLR_OFFSET,
			    rdev->desc->enable_mask);
}

static int mt6661_regulator_set_voltage(struct regulator_dev *rdev, int min_uV, int max_uV, unsigned int *selector)
{
	struct mt6661_regulator_info *info = rdev_get_drvdata(rdev);
	struct regulator_desc *desc;
	int volt = min_uV, ret = 0;
	u32 vosel = 0, sn_vocal = 0;

	if (!info)
		return -ENODEV;

	desc = &info->desc;
	if ((desc->min_uV && volt < desc->min_uV) ||
	    (info->max_uV && volt > info->max_uV)) {
		dev_err(&rdev->dev,
			"Failed to set %s voltage(%d)\n", info->desc.name, volt);
		return -ERANGE;
	}

	if (info->vocal_reg) {
		/* SN-LDO */
		vosel = (volt - desc->min_uV) / desc->uV_step;
		sn_vocal = DIV_ROUND_UP((volt - desc->min_uV) % desc->uV_step,
						info->vocal_unit);
		/* SN-LDO max_vocal: +90mV */
		if (sn_vocal > 9) {
			sn_vocal = 0;
			vosel++;
		}
	} else {
		vosel = DIV_ROUND_UP(volt - desc->min_uV, desc->uV_step);
	}
	if (info->vocal_reg)
		vosel = (vosel << 4) | sn_vocal;
	if (selector)
		*selector = vosel;
	ret = regmap_write(rdev->regmap, desc->vsel_reg, vosel);

	return ret;
}

static int mt6661_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct mt6661_regulator_info *info = rdev_get_drvdata(rdev);
	struct regulator_desc *desc;
	int vosel = 0, volt = 0, ret;

	if (!info)
		return -ENODEV;

	desc = &info->desc;
	ret = regmap_read(rdev->regmap, desc->vsel_reg, &vosel);
	if (ret)
		dev_err(&rdev->dev,
			"Failed to get %s voltage, ret=%d\n", info->desc.name, ret);

	if (info->vocal_reg)
		volt = desc->min_uV + ((vosel >> 4) & 0xf) * desc->uV_step +
		       (vosel & 0xf) * info->vocal_unit;
	else
		volt = desc->min_uV + vosel * desc->uV_step;

	return volt;
}

static inline unsigned int mt6661_map_mode(unsigned int mode)
{
	switch (mode) {
	case MT6661_REGULATOR_MODE_NORMAL:
		return REGULATOR_MODE_NORMAL;
	case MT6661_REGULATOR_MODE_FCCM:
		return REGULATOR_MODE_FAST;
	case MT6661_REGULATOR_MODE_LP:
		return REGULATOR_MODE_IDLE;
	case MT6661_REGULATOR_MODE_ULP:
		return REGULATOR_MODE_STANDBY;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

static int mt6661_buck_unlock(struct regmap *map, bool unlock)
{
	u16 buf = unlock ? MT6661_BUCK_TOP_UNLOCK_VALUE : 0;

	return regmap_bulk_write(map, MT6661_BUCK_TOP_KEY_PROT_LO, &buf, 2);
}

static unsigned int mt6661_regulator_get_mode(struct regulator_dev *rdev)
{
	struct mt6661_regulator_info *info = rdev_get_drvdata(rdev);
	unsigned int val = 0;
	int ret;

	ret = regmap_read(rdev->regmap, info->modeset_reg, &val);
	if (ret) {
		dev_err(&rdev->dev, "Failed to get mt6661 mode: %d\n", ret);
		return ret;
	}

	if (val & info->modeset_mask)
		return REGULATOR_MODE_FAST;

	if (info->da_en_reg) {
		ret = regmap_read(rdev->regmap, info->da_en_reg, &val);
		val &= info->da_lp_mask;
	} else {
		ret = regmap_read(rdev->regmap, info->lp_mode_reg, &val);
		val &= info->lp_mode_mask;
	}
	if (ret) {
		dev_err(&rdev->dev,
			"Failed to get mt6661 lp mode: %d\n", ret);
		return ret;
	}

	if (val)
		return REGULATOR_MODE_IDLE;
	else
		return REGULATOR_MODE_NORMAL;
}

static int mt6661_regulator_set_mode(struct regulator_dev *rdev,
				     unsigned int mode)
{
	struct mt6661_regulator_info *info = rdev_get_drvdata(rdev);
	int ret = 0;
	int curr_mode;

	curr_mode = mt6661_regulator_get_mode(rdev);
	switch (mode) {
	case REGULATOR_MODE_FAST:
		ret = mt6661_buck_unlock(rdev->regmap, true);
		if (ret)
			return ret;
		ret = regmap_update_bits(rdev->regmap,
					 info->modeset_reg,
					 info->modeset_mask,
					 info->modeset_mask);
		ret |= mt6661_buck_unlock(rdev->regmap, false);
		break;
	case REGULATOR_MODE_NORMAL:
		if (curr_mode == REGULATOR_MODE_FAST) {
			ret = mt6661_buck_unlock(rdev->regmap, true);
			if (ret)
				return ret;
			ret = regmap_update_bits(rdev->regmap,
						 info->modeset_reg,
						 info->modeset_mask,
						 0);
			ret |= mt6661_buck_unlock(rdev->regmap, false);
			break;
		} else if (curr_mode == REGULATOR_MODE_IDLE) {
			ret = regmap_update_bits(rdev->regmap,
						 info->lp_mode_reg,
						 info->lp_mode_mask,
						 0);
			udelay(100);
		}
		break;
	case REGULATOR_MODE_IDLE:
		ret = regmap_update_bits(rdev->regmap,
					 info->lp_mode_reg,
					 info->lp_mode_mask,
					 info->lp_mode_mask);
		break;
	default:
		return -EINVAL;
	}

	if (ret) {
		dev_err(&rdev->dev,
			"Failed to set mt6661 mode(%d): %d\n", mode, ret);
	}
	return ret;
}

static int mt6661_regulator_set_load(struct regulator_dev *rdev, int load_uA)
{
	struct mt6661_regulator_info *info = rdev_get_drvdata(rdev);
	int ret = 0;

	/* not support */
	if (!info->lp_imax_uA)
		return 0;

	if (load_uA >= info->lp_imax_uA) {
		ret = mt6661_regulator_set_mode(rdev, REGULATOR_MODE_NORMAL);
		if (ret)
			return ret;
		/* enable HW1_OP_EN (HW1 default high) */
		ret = regmap_update_bits(rdev->regmap,
					 info->hw_op_en_reg,
					 HW_NORMAL_OP_EN, HW_NORMAL_OP_EN);
		dev_info(&rdev->dev,
			 "[%s] %s force normal mode\n", __func__, info->desc.name);
	} else {
		/* disable HW1_OP_EN */
		ret = regmap_update_bits(rdev->regmap,
					 info->hw_op_en_reg,
					 HW_NORMAL_OP_EN, 0);
		dev_info(&rdev->dev,
			 "[%s] %s set to original setting\n", __func__, info->desc.name);
	}
	return ret;
}

static int mt6661_eint_enable(struct regulator_dev *rdev)
{
	struct mt6661_regulator_info *info = rdev_get_drvdata(rdev);
	struct regulator_desc *desc;
	unsigned int val;
	int ret;

	if (!info)
		return -ENODEV;
	desc = &info->desc;

	if (desc->id >= MT6661_ID_LD20_1_EINT_HIGH &&
	    desc->id <= MT6661_ID_LDC0_4_EINT_HIGH)
		val = info->pol_mask;
	else
		val = 0;
	ret = regmap_update_bits(rdev->regmap, info->eint_en_reg,
				 info->pol_mask, val);
	if (ret)
		return ret;

	ret = regmap_update_bits(rdev->regmap, desc->enable_reg,
				 desc->enable_mask, desc->enable_mask);
	if (ret)
		return ret;

	ret = regmap_update_bits(rdev->regmap, info->eint_en_reg,
				 info->eint_en_mask, info->eint_en_mask);
	return ret;
}

static int mt6661_eint_disable(struct regulator_dev *rdev)
{
	struct mt6661_regulator_info *info = rdev_get_drvdata(rdev);
	struct regulator_desc *desc;
	int ret;

	if (!info)
		return -ENODEV;
	desc = &info->desc;

	ret = regmap_update_bits(rdev->regmap, desc->enable_reg,
				 desc->enable_mask, 0);
	if (ret)
		return ret;

	udelay(1500); /* Must delay for LDO discharging */
	ret = regmap_update_bits(rdev->regmap, info->eint_en_reg,
				 info->eint_en_mask, 0);
	return ret;
}

static const struct regulator_ops mt6661_buck_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = mt6661_buck_enable,
	.disable = mt6661_buck_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6661_regulator_set_mode,
	.get_mode = mt6661_regulator_get_mode,
	.set_load = mt6661_regulator_set_load,
};

static const struct regulator_ops mt6661_volt_range_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage = mt6661_regulator_set_voltage,
	.get_voltage = mt6661_regulator_get_voltage,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = mt6661_buck_enable,
	.disable = mt6661_buck_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6661_regulator_set_mode,
	.get_mode = mt6661_regulator_get_mode,
	.set_load = mt6661_regulator_set_load,
};

/* for sshub */
static const struct regulator_ops mt6661_sshub_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
};

static const struct regulator_ops mt6661_volt_table_ops = {
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_iterate,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = mt6661_buck_enable,
	.disable = mt6661_buck_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6661_regulator_set_mode,
	.get_mode = mt6661_regulator_get_mode,
	.set_load = mt6661_regulator_set_load,
};

static const struct regulator_ops mt6661_eint_ops = {
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_iterate,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = mt6661_eint_enable,
	.disable = mt6661_eint_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6661_regulator_set_mode,
	.get_mode = mt6661_regulator_get_mode,
};

static int mt6661_of_parse_cb(struct device_node *np,
			      const struct regulator_desc *desc,
			      struct regulator_config *config);

/* The array is indexed by id(MT6661_ID_XXX) */
static struct mt6661_regulator_info mt6661_regulators[] = {
	MT6661_BUCK(BUCK1, 0, 1200000, 5000, mt_volt_range0,
		    MT6661_RG_BUCK1_EN_ADDR,
		    MT6661_RG_BUCK1_EN_SHIFT,
		    MT6661_RG_BUCK1_VOSEL_ADDR,
		    MT6661_RG_BUCK1_VOSEL_MASK,
		    MT6661_RG_BUCK1_LP_ADDR,
		    MT6661_RG_BUCK1_LP_SHIFT,
		    MT6661_RG_FCCM_PH1_ADDR,
		    MT6661_RG_FCCM_PH1_SHIFT),
	MT6661_BUCK(BUCK2, 0, 1200000, 5000, mt_volt_range0,
		    MT6661_RG_BUCK2_EN_ADDR,
		    MT6661_RG_BUCK2_EN_SHIFT,
		    MT6661_RG_BUCK2_VOSEL_ADDR,
		    MT6661_RG_BUCK2_VOSEL_MASK,
		    MT6661_RG_BUCK2_LP_ADDR,
		    MT6661_RG_BUCK2_LP_SHIFT,
		    MT6661_RG_FCCM_PH2_ADDR,
		    MT6661_RG_FCCM_PH2_SHIFT),
	MT6661_BUCK(BUCK3, 0, 1200000, 5000, mt_volt_range0,
		    MT6661_RG_BUCK3_EN_ADDR,
		    MT6661_RG_BUCK3_EN_SHIFT,
		    MT6661_RG_BUCK3_VOSEL_ADDR,
		    MT6661_RG_BUCK3_VOSEL_MASK,
		    MT6661_RG_BUCK3_LP_ADDR,
		    MT6661_RG_BUCK3_LP_SHIFT,
		    MT6661_RG_FCCM_PH3_ADDR,
		    MT6661_RG_FCCM_PH3_SHIFT),
	MT6661_BUCK(BUCK4, 0, 1200000, 5000, mt_volt_range0,
		    MT6661_RG_BUCK4_EN_ADDR,
		    MT6661_RG_BUCK4_EN_SHIFT,
		    MT6661_RG_BUCK4_VOSEL_ADDR,
		    MT6661_RG_BUCK4_VOSEL_MASK,
		    MT6661_RG_BUCK4_LP_ADDR,
		    MT6661_RG_BUCK4_LP_SHIFT,
		    MT6661_RG_FCCM_PH4_ADDR,
		    MT6661_RG_FCCM_PH4_SHIFT),
	MT6661_BUCK(BUCK5, 0, 1200000, 5000, mt_volt_range0,
		    MT6661_RG_BUCK5_EN_ADDR,
		    MT6661_RG_BUCK5_EN_SHIFT,
		    MT6661_RG_BUCK5_VOSEL_ADDR,
		    MT6661_RG_BUCK5_VOSEL_MASK,
		    MT6661_RG_BUCK5_LP_ADDR,
		    MT6661_RG_BUCK5_LP_SHIFT,
		    MT6661_RG_FCCM_PH5_ADDR,
		    MT6661_RG_FCCM_PH5_SHIFT),
	MT6661_BUCK(BUCK6, 0, 1200000, 5000, mt_volt_range0,
		    MT6661_RG_BUCK6_EN_ADDR,
		    MT6661_RG_BUCK6_EN_SHIFT,
		    MT6661_RG_BUCK6_VOSEL_ADDR,
		    MT6661_RG_BUCK6_VOSEL_MASK,
		    MT6661_RG_BUCK6_LP_ADDR,
		    MT6661_RG_BUCK6_LP_SHIFT,
		    MT6661_RG_FCCM_PH6_ADDR,
		    MT6661_RG_FCCM_PH6_SHIFT),
	MT6661_SSHUB(BUCK1_SSHUB, 0, 1200000, 5000, mt_volt_range0,
		     MT6661_RG_BUCK1_SSHUB_EN_ADDR,
		     MT6661_RG_BUCK1_SSHUB_VOSEL_ADDR,
		     MT6661_RG_BUCK1_SSHUB_VOSEL_MASK),
	MT6661_SSHUB(BUCK6_SSHUB, 0, 1200000, 5000, mt_volt_range0,
		     MT6661_RG_BUCK6_SSHUB_EN_ADDR,
		     MT6661_RG_BUCK6_SSHUB_VOSEL_ADDR,
		     MT6661_RG_BUCK6_SSHUB_VOSEL_MASK),
	MT6661_LDO(LD20_1, ldo_volt_table0,
		   MT6661_RG_LDO_LD20_1_EN_ADDR,
		   MT6661_RG_LDO_LD20_1_EN_SHIFT,
		   MT6661_RG_LDO_LD20_1_VOSEL_ADDR,
		   MT6661_RG_LDO_LD20_1_VOSEL_MASK,
		   MT6661_RG_LDO_LD20_1_VOCAL_ADDR,
		   MT6661_RG_LDO_LD20_1_VOCAL_MASK,
		   MT6661_RG_LDO_LD20_1_LP_ADDR,
		   MT6661_RG_LDO_LD20_1_LP_SHIFT),
	MT6661_LDO(LD20_2, ldo_volt_table0,
		   MT6661_RG_LDO_LD20_2_EN_ADDR,
		   MT6661_RG_LDO_LD20_2_EN_SHIFT,
		   MT6661_RG_LDO_LD20_2_VOSEL_ADDR,
		   MT6661_RG_LDO_LD20_2_VOSEL_MASK,
		   MT6661_RG_LDO_LD20_2_VOCAL_ADDR,
		   MT6661_RG_LDO_LD20_2_VOCAL_MASK,
		   MT6661_RG_LDO_LD20_2_LP_ADDR,
		   MT6661_RG_LDO_LD20_2_LP_SHIFT),
	MT6661_LDO(LDC0_3, ldo_volt_table0,
		   MT6661_RG_LDO_LDC0_3_EN_ADDR,
		   MT6661_RG_LDO_LDC0_3_EN_SHIFT,
		   MT6661_RG_LDO_LDC0_3_VOSEL_ADDR,
		   MT6661_RG_LDO_LDC0_3_VOSEL_MASK,
		   MT6661_RG_LDO_LDC0_3_VOCAL_ADDR,
		   MT6661_RG_LDO_LDC0_3_VOCAL_MASK,
		   MT6661_RG_LDO_LDC0_3_LP_ADDR,
		   MT6661_RG_LDO_LDC0_3_LP_SHIFT),
	MT6661_LDO(LDC0_4, ldo_volt_table0,
		   MT6661_RG_LDO_LDC0_4_EN_ADDR,
		   MT6661_RG_LDO_LDC0_4_EN_SHIFT,
		   MT6661_RG_LDO_LDC0_4_VOSEL_ADDR,
		   MT6661_RG_LDO_LDC0_4_VOSEL_MASK,
		   MT6661_RG_LDO_LDC0_4_VOCAL_ADDR,
		   MT6661_RG_LDO_LDC0_4_VOCAL_MASK,
		   MT6661_RG_LDO_LDC0_4_LP_ADDR,
		   MT6661_RG_LDO_LDC0_4_LP_SHIFT),
	MT6661_EINT(LD20_1, EINT_HIGH, ldo_volt_table0,
		   MT6661_RG_LDO_LD20_1_EN_ADDR,
		   MT6661_RG_LDO_LD20_1_EN_SHIFT,
		   MT6661_RG_LDO_LD20_1_VOSEL_ADDR,
		   MT6661_RG_LDO_LD20_1_VOSEL_MASK,
		   MT6661_RG_LDO_LD20_1_VOCAL_ADDR,
		   MT6661_RG_LDO_LD20_1_VOCAL_MASK,
		   MT6661_RG_LDO_LD20_1_LP_ADDR,
		   MT6661_RG_LDO_LD20_1_LP_SHIFT,
		   MT6661_RG_LDO_LD20_1_EINT_EN_ADDR,
		   MT6661_RG_LDO_LD20_1_EINT_EN_SHIFT,
		   MT6661_RG_LDO_LD20_1_EINT_POL_SHIFT),
	MT6661_EINT(LD20_1, EINT_LOW, ldo_volt_table0,
		   MT6661_RG_LDO_LD20_1_EN_ADDR,
		   MT6661_RG_LDO_LD20_1_EN_SHIFT,
		   MT6661_RG_LDO_LD20_1_VOSEL_ADDR,
		   MT6661_RG_LDO_LD20_1_VOSEL_MASK,
		   MT6661_RG_LDO_LD20_1_VOCAL_ADDR,
		   MT6661_RG_LDO_LD20_1_VOCAL_MASK,
		   MT6661_RG_LDO_LD20_1_LP_ADDR,
		   MT6661_RG_LDO_LD20_1_LP_SHIFT,
		   MT6661_RG_LDO_LD20_1_EINT_EN_ADDR,
		   MT6661_RG_LDO_LD20_1_EINT_EN_SHIFT,
		   MT6661_RG_LDO_LD20_1_EINT_POL_SHIFT),
	MT6661_EINT(LD20_2, EINT_HIGH, ldo_volt_table0,
		   MT6661_RG_LDO_LD20_2_EN_ADDR,
		   MT6661_RG_LDO_LD20_2_EN_SHIFT,
		   MT6661_RG_LDO_LD20_2_VOSEL_ADDR,
		   MT6661_RG_LDO_LD20_2_VOSEL_MASK,
		   MT6661_RG_LDO_LD20_2_VOCAL_ADDR,
		   MT6661_RG_LDO_LD20_2_VOCAL_MASK,
		   MT6661_RG_LDO_LD20_2_LP_ADDR,
		   MT6661_RG_LDO_LD20_2_LP_SHIFT,
		   MT6661_RG_LDO_LD20_2_EINT_EN_ADDR,
		   MT6661_RG_LDO_LD20_2_EINT_EN_SHIFT,
		   MT6661_RG_LDO_LD20_2_EINT_POL_SHIFT),
	MT6661_EINT(LD20_2, EINT_LOW, ldo_volt_table0,
		   MT6661_RG_LDO_LD20_2_EN_ADDR,
		   MT6661_RG_LDO_LD20_2_EN_SHIFT,
		   MT6661_RG_LDO_LD20_2_VOSEL_ADDR,
		   MT6661_RG_LDO_LD20_2_VOSEL_MASK,
		   MT6661_RG_LDO_LD20_2_VOCAL_ADDR,
		   MT6661_RG_LDO_LD20_2_VOCAL_MASK,
		   MT6661_RG_LDO_LD20_2_LP_ADDR,
		   MT6661_RG_LDO_LD20_2_LP_SHIFT,
		   MT6661_RG_LDO_LD20_2_EINT_EN_ADDR,
		   MT6661_RG_LDO_LD20_2_EINT_EN_SHIFT,
		   MT6661_RG_LDO_LD20_2_EINT_POL_SHIFT),
	MT6661_EINT(LDC0_3, EINT_HIGH, ldo_volt_table0,
		   MT6661_RG_LDO_LDC0_3_EN_ADDR,
		   MT6661_RG_LDO_LDC0_3_EN_SHIFT,
		   MT6661_RG_LDO_LDC0_3_VOSEL_ADDR,
		   MT6661_RG_LDO_LDC0_3_VOSEL_MASK,
		   MT6661_RG_LDO_LDC0_3_VOCAL_ADDR,
		   MT6661_RG_LDO_LDC0_3_VOCAL_MASK,
		   MT6661_RG_LDO_LDC0_3_LP_ADDR,
		   MT6661_RG_LDO_LDC0_3_LP_SHIFT,
		   MT6661_RG_LDO_LDC0_3_EINT_EN_ADDR,
		   MT6661_RG_LDO_LDC0_3_EINT_EN_SHIFT,
		   MT6661_RG_LDO_LDC0_3_EINT_POL_SHIFT),
	MT6661_EINT(LDC0_3, EINT_LOW, ldo_volt_table0,
		   MT6661_RG_LDO_LDC0_3_EN_ADDR,
		   MT6661_RG_LDO_LDC0_3_EN_SHIFT,
		   MT6661_RG_LDO_LDC0_3_VOSEL_ADDR,
		   MT6661_RG_LDO_LDC0_3_VOSEL_MASK,
		   MT6661_RG_LDO_LDC0_3_VOCAL_ADDR,
		   MT6661_RG_LDO_LDC0_3_VOCAL_MASK,
		   MT6661_RG_LDO_LDC0_3_LP_ADDR,
		   MT6661_RG_LDO_LDC0_3_LP_SHIFT,
		   MT6661_RG_LDO_LDC0_3_EINT_EN_ADDR,
		   MT6661_RG_LDO_LDC0_3_EINT_EN_SHIFT,
		   MT6661_RG_LDO_LDC0_3_EINT_POL_SHIFT),
	MT6661_EINT(LDC0_4, EINT_HIGH, ldo_volt_table0,
		   MT6661_RG_LDO_LDC0_4_EN_ADDR,
		   MT6661_RG_LDO_LDC0_4_EN_SHIFT,
		   MT6661_RG_LDO_LDC0_4_VOSEL_ADDR,
		   MT6661_RG_LDO_LDC0_4_VOSEL_MASK,
		   MT6661_RG_LDO_LDC0_4_VOCAL_ADDR,
		   MT6661_RG_LDO_LDC0_4_VOCAL_MASK,
		   MT6661_RG_LDO_LDC0_4_LP_ADDR,
		   MT6661_RG_LDO_LDC0_4_LP_SHIFT,
		   MT6661_RG_LDO_LDC0_4_EINT_EN_ADDR,
		   MT6661_RG_LDO_LDC0_4_EINT_EN_SHIFT,
		   MT6661_RG_LDO_LDC0_4_EINT_POL_SHIFT),
	MT6661_EINT(LDC0_4, EINT_LOW, ldo_volt_table0,
		   MT6661_RG_LDO_LDC0_4_EN_ADDR,
		   MT6661_RG_LDO_LDC0_4_EN_SHIFT,
		   MT6661_RG_LDO_LDC0_4_VOSEL_ADDR,
		   MT6661_RG_LDO_LDC0_4_VOSEL_MASK,
		   MT6661_RG_LDO_LDC0_4_VOCAL_ADDR,
		   MT6661_RG_LDO_LDC0_4_VOCAL_MASK,
		   MT6661_RG_LDO_LDC0_4_LP_ADDR,
		   MT6661_RG_LDO_LDC0_4_LP_SHIFT,
		   MT6661_RG_LDO_LDC0_4_EINT_EN_ADDR,
		   MT6661_RG_LDO_LDC0_4_EINT_EN_SHIFT,
		   MT6661_RG_LDO_LDC0_4_EINT_POL_SHIFT),
	MT6661_LDO_LINEAR(LN60_5, 500000, 2090000, 100000, mt_volt_range1,
			  MT6661_RG_LDO_LN60_5_EN_ADDR,
			  MT6661_RG_LDO_LN60_5_EN_SHIFT,
			  MT6661_RG_LN60_5_VOSEL_ADDR,
			  MT6661_RG_LN60_5_VOSEL_MASK,
			  MT6661_RG_LN60_5_VOCAL_ADDR,
			  MT6661_RG_LN60_5_VOCAL_MASK,
			  MT6661_RG_LDO_LN60_5_LP_ADDR,
			  MT6661_RG_LDO_LN60_5_LP_SHIFT),
	MT6661_LDO_LINEAR(LN60_6, 500000, 2090000, 100000, mt_volt_range1,
			  MT6661_RG_LDO_LN60_6_EN_ADDR,
			  MT6661_RG_LDO_LN60_6_EN_SHIFT,
			  MT6661_RG_LN60_6_VOSEL_ADDR,
			  MT6661_RG_LN60_6_VOSEL_MASK,
			  MT6661_RG_LN60_6_VOCAL_ADDR,
			  MT6661_RG_LN60_6_VOCAL_MASK,
			  MT6661_RG_LDO_LN60_6_LP_ADDR,
			  MT6661_RG_LDO_LN60_6_LP_SHIFT),
	MT6661_LDO_LINEAR(LNC0_10, 500000, 2090000, 100000, mt_volt_range1,
			  MT6661_RG_LDO_LNC0_10_EN_ADDR,
			  MT6661_RG_LDO_LNC0_10_EN_SHIFT,
			  MT6661_RG_LNC0_10_VOSEL_ADDR,
			  MT6661_RG_LNC0_10_VOSEL_MASK,
			  MT6661_RG_LNC0_10_VOCAL_ADDR,
			  MT6661_RG_LNC0_10_VOCAL_MASK,
			  MT6661_RG_LDO_LNC0_10_LP_ADDR,
			  MT6661_RG_LDO_LNC0_10_LP_SHIFT),
	MT6661_LDO_LINEAR(LN60_7, 500000, 2090000, 100000, mt_volt_range1,
			  MT6661_RG_LDO_LN60_7_EN_ADDR,
			  MT6661_RG_LDO_LN60_7_EN_SHIFT,
			  MT6661_RG_LN60_7_VOSEL_ADDR,
			  MT6661_RG_LN60_7_VOSEL_MASK,
			  MT6661_RG_LN60_7_VOCAL_ADDR,
			  MT6661_RG_LN60_7_VOCAL_MASK,
			  MT6661_RG_LDO_LN60_7_LP_ADDR,
			  MT6661_RG_LDO_LN60_7_LP_SHIFT),
	MT6661_LDO_LINEAR(LNC0_8, 500000, 2090000, 100000, mt_volt_range1,
			  MT6661_RG_LDO_LNC0_8_EN_ADDR,
			  MT6661_RG_LDO_LNC0_8_EN_SHIFT,
			  MT6661_RG_LNC0_8_VOSEL_ADDR,
			  MT6661_RG_LNC0_8_VOSEL_MASK,
			  MT6661_RG_LNC0_8_VOCAL_ADDR,
			  MT6661_RG_LNC0_8_VOCAL_MASK,
			  MT6661_RG_LDO_LNC0_8_LP_ADDR,
			  MT6661_RG_LDO_LNC0_8_LP_SHIFT),
	MT6661_LDO_LINEAR(LNC0_9, 500000, 2090000, 100000, mt_volt_range1,
			  MT6661_RG_LDO_LNC0_9_EN_ADDR,
			  MT6661_RG_LDO_LNC0_9_EN_SHIFT,
			  MT6661_RG_LNC0_9_VOSEL_ADDR,
			  MT6661_RG_LNC0_9_VOSEL_MASK,
			  MT6661_RG_LNC0_9_VOCAL_ADDR,
			  MT6661_RG_LNC0_9_VOCAL_MASK,
			  MT6661_RG_LDO_LNC0_9_LP_ADDR,
			  MT6661_RG_LDO_LNC0_9_LP_SHIFT),
	MT6661_LDO_LINEAR(LNC0_S_11, 500000, 2090000, 100000, mt_volt_range1,
			  MT6661_RG_LDO_LNC0_S_11_EN_ADDR,
			  MT6661_RG_LDO_LNC0_S_11_EN_SHIFT,
			  MT6661_RG_LNC0_S_11_VOSEL_ADDR,
			  MT6661_RG_LNC0_S_11_VOSEL_MASK,
			  MT6661_RG_LNC0_S_11_VOCAL_ADDR,
			  MT6661_RG_LNC0_S_11_VOCAL_MASK,
			  MT6661_RG_LDO_LNC0_S_11_LP_ADDR,
			  MT6661_RG_LDO_LNC0_S_11_LP_SHIFT),
	MT6661_LDO_LINEAR(LNC0_S_12, 500000, 2090000, 100000, mt_volt_range1,
			  MT6661_RG_LDO_LNC0_S_12_EN_ADDR,
			  MT6661_RG_LDO_LNC0_S_12_EN_SHIFT,
			  MT6661_RG_LNC0_S_12_VOSEL_ADDR,
			  MT6661_RG_LNC0_S_12_VOSEL_MASK,
			  MT6661_RG_LNC0_S_12_VOCAL_ADDR,
			  MT6661_RG_LNC0_S_12_VOCAL_MASK,
			  MT6661_RG_LDO_LNC0_S_12_LP_ADDR,
			  MT6661_RG_LDO_LNC0_S_12_LP_SHIFT),
};

static void mt6661_oc_irq_enable_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct mt6661_regulator_info *info
		= container_of(dwork, struct mt6661_regulator_info, oc_work);

	enable_irq(info->irq);
	enable_irq(info->lp_irq);
}

static irqreturn_t mt6661_oc_irq(int irq, void *data)
{
	struct regulator_dev *rdev = (struct regulator_dev *)data;
	struct mt6661_regulator_info *info = rdev_get_drvdata(rdev);

	disable_irq_nosync(info->irq);
	disable_irq_nosync(info->lp_irq);
	if (!regulator_is_enabled_regmap(rdev))
		goto delayed_enable;
	regulator_notifier_call_chain(rdev, REGULATOR_EVENT_OVER_CURRENT,
				      NULL);
delayed_enable:
	schedule_delayed_work(&info->oc_work,
			      msecs_to_jiffies(info->oc_irq_enable_delay_ms));
	return IRQ_HANDLED;
}

static int mt6661_of_parse_cb(struct device_node *np,
			      const struct regulator_desc *desc,
			      struct regulator_config *config)
{
	struct mt6661_regulator_info *info = config->driver_data;
	int ret;

	if ((desc->id >= MT6661_ID_LD20_1 && info->lp_irq > 0) ||
	    (desc->id < MT6661_ID_LD20_1 && info->irq > 0)) {
		ret = of_property_read_u32(np, "mediatek,oc-irq-enable-delay-ms",
					   &info->oc_irq_enable_delay_ms);
		if (ret || !info->oc_irq_enable_delay_ms)
			info->oc_irq_enable_delay_ms = DEFAULT_DELAY_MS;
		INIT_DELAYED_WORK(&info->oc_work, mt6661_oc_irq_enable_work);
	}
	return 0;
}

/* Clear UVLO info from LK2 */
static void pmic_clear_uvlo_info(struct regulator_dev *rdev)
{
	int ret = 0;
	unsigned int val = 0;

	ret |= regmap_write(rdev->regmap, MT6661_VRC_CON2, val);
	ret |= regmap_write(rdev->regmap, MT6661_VRC_CON3, val);
	if (ret)
		pr_info("%s: failed to access register.\n", __func__);
}

static int mt6661_regulator_probe(struct platform_device *pdev)
{
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	struct mt6661_regulator_info *info;
	const char *compatible;
	unsigned int slvid = 0, hwcid = 0;
	int i = 0, j = -1, ret;
	char **ldo_name;

	dev_info(&pdev->dev, "%s\n", __func__);
	config.dev = pdev->dev.parent;
	config.regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!config.regmap)
		return -ENODEV;

	ret = regmap_read(config.regmap, MT6661_HWCID0, &hwcid);
	if (ret)
		dev_err(&pdev->dev, "Failed to get mt6661 hwcid: ret=%d\n", ret);
	ret = regmap_read(config.regmap, MT6661_RG_SLV_ID_ADDR, &slvid);
	if (ret)
		dev_err(&pdev->dev, "Failed to get mt6661 slvid: ret=%d\n", ret);
	slvid &= 0xF;

	ldo_name = devm_kzalloc(&pdev->dev, (MT6661_ID_LDC0_4_EINT_LOW - MT6661_ID_LD20_1 + 1) *
				sizeof(char *), GFP_KERNEL);
	if (!ldo_name)
		return -ENOMEM;
	for (i = 0; i < MT6661_MAX_REGULATOR; i++) {
		info = &mt6661_regulators[i];
		info->irq = platform_get_irq_byname_optional(pdev, info->desc.name);
		/* For LDO LP INT */
		if (i >= MT6661_ID_LD20_1) {
			++j;
			ldo_name[j] = devm_kzalloc(&pdev->dev, BUF_SIZE, GFP_KERNEL);
			if (!ldo_name[j])
				return -ENOMEM;
			snprintf(ldo_name[j], BUF_SIZE, "%s_LP", info->desc.name);
			info->lp_irq = platform_get_irq_byname_optional(pdev, ldo_name[j]);
		}
		config.driver_data = info;

		/* skip registering MT6661-S5 LNC0_9 in MT6993 A0(MT6661-E1) to avoid OC */
		if (hwcid == MT6661_HWCID0_E1_CODE &&
		    slvid == 5 && info->desc.id == MT6661_ID_LNC0_9) {
			dev_info(&pdev->dev, "Skip registering %s\n", info->desc.name);
			continue;
		}
		rdev = devm_regulator_register(&pdev->dev, &info->desc, &config);
		if (IS_ERR(rdev)) {
			ret = PTR_ERR(rdev);
			dev_err(&pdev->dev, "Failed to register %s, ret=%d\n",
				info->desc.name, ret);
			continue;
		}
		if (info->irq <= 0) {
			dev_err(&pdev->dev, "Failed to get IRQ for %s\n", info->desc.name);
			continue;
		}
		ret = devm_request_threaded_irq(&pdev->dev, info->irq, NULL,
						mt6661_oc_irq,
						IRQF_TRIGGER_HIGH,
						info->desc.name,
						rdev);
		/* For LDO LP INT */
		if (i >= MT6661_ID_LD20_1) {
			if (info->lp_irq <= 0) {
				dev_err(&pdev->dev, "Failed to get IRQ for %s\n", ldo_name[j]);
				continue;
			}
			ret |= devm_request_threaded_irq(&pdev->dev, info->lp_irq, NULL,
						mt6661_oc_irq,
						IRQF_TRIGGER_HIGH,
						ldo_name[j],
						rdev);
		}

		if (ret) {
			dev_err(&pdev->dev, "Failed to request IRQ:%s, ret=%d",
				info->desc.name, ret);
			continue;
		}
	}
	ret = of_property_read_string(pdev->dev.of_node, "compatible", &compatible);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get compatible property, ret=%d\n", ret);
		return ret;
	}
	if (!strcmp(compatible, "mediatek,mt6661-4-regulator"))
		pmic_clear_uvlo_info(rdev);

	return 0;
}

static const struct platform_device_id mt6661_regulator_ids[] = {
	{ "mt6661-4-regulator", 4},
	{ "mt6661-3-regulator", 3},
	{ "mt6661-5-regulator", 5},
	{ "mt6661-6-regulator", 6},
	{ "mt6661-7-regulator", 7},
	{ "mt6661-8-regulator", 8},
	{ "mt6661-12-regulator", 12},
	{ "mt6661-13-regulator", 13},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(platform, mt6661_regulator_ids);

static struct platform_driver mt6661_regulator_driver = {
	.driver = {
		.name = "mt6661-regulator",
	},
	.probe = mt6661_regulator_probe,
	.id_table = mt6661_regulator_ids,
};
module_platform_driver(mt6661_regulator_driver);

MODULE_AUTHOR("Ying-Ren.Chen <ying-ren.chen@mediatek.com>");
MODULE_DESCRIPTION("Regulator Driver for MediaTek MT6661 PMIC");
MODULE_LICENSE("GPL v2");
