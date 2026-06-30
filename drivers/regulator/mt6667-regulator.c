// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2024 MediaTek Inc.

#include <linux/interrupt.h>
#include <linux/math.h>
#include <linux/mfd/mt6667/registers.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/mt6667-regulator.h>
#include <linux/regulator/of_regulator.h>

#define SET_OFFSET		0x1
#define CLR_OFFSET		0x2
#define HW_NORMAL_OP_EN		0x2
#define STRGINGIFY(x)	#x
#define CONCAT_AND_STRGINGIFY(x, y) STRGINGIFY(x##_##y)

#define MT6667_REGULATOR_MODE_NORMAL	0
#define MT6667_REGULATOR_MODE_FCCM	1
#define MT6667_REGULATOR_MODE_LP	2
#define MT6667_REGULATOR_MODE_ULP	3

#define DEFAULT_DELAY_MS		10

/*
 * MT6667 regulator lock register
 */
#define MT6667_TMA_UNLOCK_VALUE		0x9998
#define MT6667_BUCK_TOP_UNLOCK_VALUE	0x5543

/*
 * MT6667 regulators' information
 *
 * @desc: standard fields of regulator description.
 * @lp_mode_reg: for operating NORMAL/IDLE mode register.
 * @lp_mode_mask: MASK for operating lp_mode register.
 * @hw_lp_mode_reg: hardware NORMAL/IDLE mode status register.
 * @hw_lp_mode_mask: MASK for hardware NORMAL/IDLE mode status register.
 * @modeset_reg: for operating AUTO/PWM mode register.
 * @modeset_mask: MASK for operating modeset register.
 * @lp_imax_uA: Maximum load current in Low power mode.
 * @hw_op_en_reg: for HW control operating mode register.
 */
struct mt6667_regulator_info {
	int irq;
	int oc_irq_enable_delay_ms;
	struct delayed_work oc_work;
	struct regulator_desc desc;
	u32 max_uV;
	u32 lp_mode_reg;
	u32 lp_mode_mask;
	u32 modeset_reg;
	u32 modeset_mask;
	u32 da_en_reg;
	u32 da_en_mask;
	u32 da_lp_mask;
	int lp_imax_uA;
	u32 hw_op_en_reg;
};

#define MT6667_BUCK(_name, _min, _max, _step, _volt_ranges,	\
		    _enable_reg, _en_bit, _vsel_reg, _vsel_mask,\
		    _lp_mode_reg, _lp_bit,			\
		    _modeset_reg, modeset_bit)			\
[MT6667_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(#_name),		\
		.of_parse_cb = mt6667_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &mt6667_buck_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6667_ID_##_name,			\
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
		.of_map_mode = mt6667_map_mode,			\
	},							\
	.max_uV = _max,						\
	.lp_mode_reg = _lp_mode_reg,				\
	.lp_mode_mask = BIT(_lp_bit),				\
	.da_en_reg = MT6667_PMIC_DA_##_name##_EN_ADDR,		\
	.da_en_mask = BIT(0),					\
	.da_lp_mask = 0xc,					\
	.modeset_reg = _modeset_reg,				\
	.modeset_mask = BIT(modeset_bit),			\
	.lp_imax_uA = 3000000,					\
	.hw_op_en_reg = MT6667_##_name##_OP_EN_1,		\
}

static const struct linear_range mt_volt_range0[] = {
	REGULATOR_LINEAR_RANGE(0, 0, 240, 5000),
};

static int mt6667_buck_enable(struct regulator_dev *rdev)
{
	return regmap_write(rdev->regmap, rdev->desc->enable_reg + SET_OFFSET,
			    rdev->desc->enable_mask);
}

static int mt6667_buck_disable(struct regulator_dev *rdev)
{
	return regmap_write(rdev->regmap, rdev->desc->enable_reg + CLR_OFFSET,
			    rdev->desc->enable_mask);
}

static inline unsigned int mt6667_map_mode(unsigned int mode)
{
	switch (mode) {
	case MT6667_REGULATOR_MODE_NORMAL:
		return REGULATOR_MODE_NORMAL;
	case MT6667_REGULATOR_MODE_FCCM:
		return REGULATOR_MODE_FAST;
	case MT6667_REGULATOR_MODE_LP:
		return REGULATOR_MODE_IDLE;
	case MT6667_REGULATOR_MODE_ULP:
		return REGULATOR_MODE_STANDBY;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

static int mt6667_buck_unlock(struct regmap *map, bool unlock)
{
	u16 buf = unlock ? MT6667_BUCK_TOP_UNLOCK_VALUE : 0;

	return regmap_bulk_write(map, MT6667_BUCK_TOP_KEY_PROT_LO, &buf, 2);
}

static unsigned int mt6667_regulator_get_mode(struct regulator_dev *rdev)
{
	struct mt6667_regulator_info *info = rdev_get_drvdata(rdev);
	unsigned int val = 0;
	int ret;

	ret = regmap_read(rdev->regmap, info->modeset_reg, &val);
	if (ret) {
		dev_err(&rdev->dev, "Failed to get mt6667 mode: %d\n", ret);
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
			"Failed to get mt6667 lp mode: %d\n", ret);
		return ret;
	}

	if (val)
		return REGULATOR_MODE_IDLE;
	else
		return REGULATOR_MODE_NORMAL;
}

static int mt6667_regulator_set_mode(struct regulator_dev *rdev,
				     unsigned int mode)
{
	struct mt6667_regulator_info *info = rdev_get_drvdata(rdev);
	int ret = 0;
	int curr_mode;

	curr_mode = mt6667_regulator_get_mode(rdev);
	switch (mode) {
	case REGULATOR_MODE_FAST:
		ret = mt6667_buck_unlock(rdev->regmap, true);
		if (ret)
			return ret;
		ret = regmap_update_bits(rdev->regmap,
					 info->modeset_reg,
					 info->modeset_mask,
					 info->modeset_mask);
		ret |= mt6667_buck_unlock(rdev->regmap, false);
		break;
	case REGULATOR_MODE_NORMAL:
		if (curr_mode == REGULATOR_MODE_FAST) {
			ret = mt6667_buck_unlock(rdev->regmap, true);
			if (ret)
				return ret;
			ret = regmap_update_bits(rdev->regmap,
						 info->modeset_reg,
						 info->modeset_mask,
						 0);
			ret |= mt6667_buck_unlock(rdev->regmap, false);
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
			"Failed to set mt6667 mode(%d): %d\n", mode, ret);
	}
	return ret;
}

static int mt6667_regulator_set_load(struct regulator_dev *rdev, int load_uA)
{
	struct mt6667_regulator_info *info = rdev_get_drvdata(rdev);
	int ret = 0;

	/* not support */
	if (!info->lp_imax_uA)
		return 0;

	if (load_uA >= info->lp_imax_uA) {
		ret = mt6667_regulator_set_mode(rdev, REGULATOR_MODE_NORMAL);
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
			 "[%s] %s restore to original setting\n", __func__, info->desc.name);
	}
	return ret;
}

static const struct regulator_ops mt6667_buck_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = mt6667_buck_enable,
	.disable = mt6667_buck_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6667_regulator_set_mode,
	.get_mode = mt6667_regulator_get_mode,
	.set_load = mt6667_regulator_set_load,
};

static int mt6667_of_parse_cb(struct device_node *np,
			      const struct regulator_desc *desc,
			      struct regulator_config *config);

/* The array is indexed by id(MT6667_ID_XXX) */
static struct mt6667_regulator_info mt6667_regulators[] = {
	MT6667_BUCK(BUCK1, 0, 1200000, 5000, mt_volt_range0,
		    MT6667_PMIC_RG_BUCK1_EN_ADDR,
		    MT6667_PMIC_RG_BUCK1_EN_SHIFT,
		    MT6667_PMIC_RG_BUCK1_VOSEL_ADDR,
		    MT6667_PMIC_RG_BUCK1_VOSEL_MASK,
		    MT6667_PMIC_RG_BUCK1_LP_ADDR,
		    MT6667_PMIC_RG_BUCK1_LP_SHIFT,
		    MT6667_PMIC_RG_FCCM_PH1_ADDR,
		    MT6667_PMIC_RG_FCCM_PH1_SHIFT),
	MT6667_BUCK(BUCK2, 0, 1200000, 5000, mt_volt_range0,
		    MT6667_PMIC_RG_BUCK2_EN_ADDR,
		    MT6667_PMIC_RG_BUCK2_EN_SHIFT,
		    MT6667_PMIC_RG_BUCK2_VOSEL_ADDR,
		    MT6667_PMIC_RG_BUCK2_VOSEL_MASK,
		    MT6667_PMIC_RG_BUCK2_LP_ADDR,
		    MT6667_PMIC_RG_BUCK2_LP_SHIFT,
		    MT6667_PMIC_RG_FCCM_PH2_ADDR,
		    MT6667_PMIC_RG_FCCM_PH2_SHIFT),
	MT6667_BUCK(BUCK3, 0, 1200000, 5000, mt_volt_range0,
		    MT6667_PMIC_RG_BUCK3_EN_ADDR,
		    MT6667_PMIC_RG_BUCK3_EN_SHIFT,
		    MT6667_PMIC_RG_BUCK3_VOSEL_ADDR,
		    MT6667_PMIC_RG_BUCK3_VOSEL_MASK,
		    MT6667_PMIC_RG_BUCK3_LP_ADDR,
		    MT6667_PMIC_RG_BUCK3_LP_SHIFT,
		    MT6667_PMIC_RG_FCCM_PH3_ADDR,
		    MT6667_PMIC_RG_FCCM_PH3_SHIFT),
	MT6667_BUCK(BUCK4, 0, 1200000, 5000, mt_volt_range0,
		    MT6667_PMIC_RG_BUCK4_EN_ADDR,
		    MT6667_PMIC_RG_BUCK4_EN_SHIFT,
		    MT6667_PMIC_RG_BUCK4_VOSEL_ADDR,
		    MT6667_PMIC_RG_BUCK4_VOSEL_MASK,
		    MT6667_PMIC_RG_BUCK4_LP_ADDR,
		    MT6667_PMIC_RG_BUCK4_LP_SHIFT,
		    MT6667_PMIC_RG_FCCM_PH4_ADDR,
		    MT6667_PMIC_RG_FCCM_PH4_SHIFT),
};

static void mt6667_oc_irq_enable_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct mt6667_regulator_info *info
		= container_of(dwork, struct mt6667_regulator_info, oc_work);

	enable_irq(info->irq);
}

static irqreturn_t mt6667_oc_irq(int irq, void *data)
{
	struct regulator_dev *rdev = (struct regulator_dev *)data;
	struct mt6667_regulator_info *info = rdev_get_drvdata(rdev);

	disable_irq_nosync(info->irq);
	if (!regulator_is_enabled_regmap(rdev))
		goto delayed_enable;
	regulator_notifier_call_chain(rdev, REGULATOR_EVENT_OVER_CURRENT,
				      NULL);
delayed_enable:
	schedule_delayed_work(&info->oc_work,
			      msecs_to_jiffies(info->oc_irq_enable_delay_ms));
	return IRQ_HANDLED;
}

static int mt6667_of_parse_cb(struct device_node *np,
			      const struct regulator_desc *desc,
			      struct regulator_config *config)
{
	struct mt6667_regulator_info *info = config->driver_data;
	int ret;

	if (info->irq > 0) {
		ret = of_property_read_u32(np, "mediatek,oc-irq-enable-delay-ms",
					   &info->oc_irq_enable_delay_ms);
		if (ret || !info->oc_irq_enable_delay_ms)
			info->oc_irq_enable_delay_ms = DEFAULT_DELAY_MS;
		INIT_DELAYED_WORK(&info->oc_work, mt6667_oc_irq_enable_work);
	}
	return 0;
}

static int mt6667_regulator_probe(struct platform_device *pdev)
{
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	struct mt6667_regulator_info *info;
	int i, ret;

	config.dev = pdev->dev.parent;
	config.regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!config.regmap)
		return -ENODEV;

	for (i = 0; i < MT6667_MAX_REGULATOR; i++) {
		info = &mt6667_regulators[i];
		info->irq = platform_get_irq_byname_optional(pdev, info->desc.name);
		config.driver_data = info;

		rdev = devm_regulator_register(&pdev->dev, &info->desc, &config);
		if (IS_ERR(rdev)) {
			ret = PTR_ERR(rdev);
			dev_err(&pdev->dev, "failed to register %s, ret=%d\n",
				info->desc.name, ret);
			continue;
		}

		if (info->irq <= 0)
			continue;
		ret = devm_request_threaded_irq(&pdev->dev, info->irq, NULL,
						mt6667_oc_irq,
						IRQF_TRIGGER_HIGH,
						info->desc.name,
						rdev);
		if (ret) {
			dev_err(&pdev->dev, "Failed to request IRQ:%s, ret=%d",
				info->desc.name, ret);
			continue;
		}
	}

	return 0;
}

static const struct platform_device_id mt6667_regulator_ids[] = {
	{ "mt6667-7-regulator", 7},
	{ "mt6667-8-regulator", 8},
	{ "mt6667-12-regulator", 12},
	{ "mt6667-13-regulator", 13},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(platform, mt6667_regulator_ids);

static struct platform_driver mt6667_regulator_driver = {
	.driver = {
		.name = "mt6667-regulator",
	},
	.probe = mt6667_regulator_probe,
	.id_table = mt6667_regulator_ids,
};
module_platform_driver(mt6667_regulator_driver);

MODULE_AUTHOR("Ying-Ren.Chen <ying-ren.chen@mediatek.com>");
MODULE_DESCRIPTION("Regulator Driver for MediaTek MT6667 PMIC");
MODULE_LICENSE("GPL v2");
