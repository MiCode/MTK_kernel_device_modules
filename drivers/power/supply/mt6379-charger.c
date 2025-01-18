// SPDX-License-Identifier: GPL-2.0-only
/*
 * mt6379-charger.c -- Mediatek MT6379 Charger Driver
 *
 * Copyright (c) 2023 MediaTek Inc.
 *
 * Author: SHIH CHIA CHANG <jeff_chang@richtek.com>
 */

#include <dt-bindings/power/mtk-charger.h>
#include <linux/bitfield.h>
#include <linux/devm-helpers.h>
#include <linux/init.h>
#include <linux/linear_range.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/regmap.h>

#include "mt6379-charger.h"
#include "charger_class.h"
#include <ufcs_class.h>

#define DEFAULT_PMIC_UVLO_MV	2000
#define DPDM_OV_THRESHOLD_MV	3850

enum {
	CHG_STAT_SLEEP,
	CHG_STAT_VBUS_RDY,
	CHG_STAT_TRICKLE,
	CHG_STAT_PRE,
	CHG_STAT_FAST,
	CHG_STAT_EOC,
	CHG_STAT_BKGND,
	CHG_STAT_DONE,
	CHG_STAT_FAULT,
	CHG_STAT_OTG = 15,
	CHG_STAT_MAX,
};

struct mt6379_charger_field {
	const char *name;
	const struct linear_range *range;
	struct reg_field field;
	bool inited;
};

enum mt6379_charger_dtprop_type {
	DTPROP_U32,
	DTPROP_BOOL,
};

struct mt6379_charger_dtprop {
	const char *name;
	size_t offset;
	enum mt6379_charger_reg_field field;
	enum mt6379_charger_dtprop_type type;
};

#define MT6379_CHG_DTPROP(_name, _member, _field, _type)		  \
{									  \
	.name = _name,							  \
	.field = _field,						  \
	.type = _type,							  \
	.offset = offsetof(struct mt6379_charger_platform_data, _member),\
}

#define MT6379_CHARGER_FIELD(_fd, _reg, _lsb, _msb)			\
[_fd] = {								\
	.name = #_fd,							\
	.range = NULL,							\
	.field = REG_FIELD(_reg, _lsb, _msb),				\
	.inited = true,							\
}

#define MT6379_CHARGER_FIELD_RANGE(_fd, _reg, _lsb, _msb)		\
[_fd] = {								\
	.name = #_fd,							\
	.range = &mt6379_charger_ranges[MT6379_RANGE_##_fd],		\
	.field = REG_FIELD(_reg, _lsb, _msb),				\
	.inited = true,							\
}

enum {
	PORT_STAT_NOINFO,
	PORT_STAT_APPLE_10W = 8,
	PORT_STAT_SS_TA,
	PORT_STAT_APPLE_5W,
	PORT_STAT_APPLE_12W,
	PORT_STAT_UNKNOWN_TA,
	PORT_STAT_SDP,
	PORT_STAT_CDP,
	PORT_STAT_DCP,
};

enum {
	MT6379_RANGE_F_BATINT = 0,
	MT6379_RANGE_F_IBUS_AICR,
	MT6379_RANGE_F_WLIN_AICR,
	MT6379_RANGE_F_VBUS_MIVR,
	MT6379_RANGE_F_WLIN_MIVR,
	MT6379_RANGE_F_VREC,
	MT6379_RANGE_F_CV,
	MT6379_RANGE_F_CC,
	MT6379_RANGE_F_CHG_TMR,
	MT6379_RANGE_F_IEOC,
	MT6379_RANGE_F_EOC_TIME,
	MT6379_RANGE_F_VSYSOV,
	MT6379_RANGE_F_VSYSMIN,
	MT6379_RANGE_F_PE20_CODE,
	MT6379_RANGE_F_IPREC,
	MT6379_RANGE_F_AICC_RPT,
	MT6379_RANGE_F_OTG_LBP,
	MT6379_RANGE_F_OTG_OCP,
	MT6379_RANGE_F_OTG_CC,
	MT6379_RANGE_F_IRCMP_R,
	MT6379_RANGE_F_IRCMP_V,
	MT6379_RANGE_F_MAX,
};

enum {
	MT6379_DP_LDO_VSEL_600MV,
	MT6379_DP_LDO_VSEL_650MV,
	MT6379_DP_LDO_VSEL_700MV,
	MT6379_DP_LDO_VSEL_750MV,
	MT6379_DP_LDO_VSEL_1800MV,
	MT6379_DP_LDO_VSEL_2800MV,
	MT6379_DP_LDO_VSEL_3300MV,
};

enum {
	MT6379_DP_PULL_RSEL_1_2_K,
	MT6379_DP_PULL_RSEL_10_K,
	MT6379_DP_PULL_RSEL_15_K,
};

enum {
	MT6379_CHGIN_OV_4_7_V,
	MT6379_CHGIN_OV_5_8_V,
	MT6379_CHGIN_OV_6_5_V,
	MT6379_CHGIN_OV_11_V,
	MT6379_CHGIN_OV_14_5_V,
	MT6379_CHGIN_OV_18_V,
	MT6379_CHGIN_OV_22_5_V,
};

static const struct linear_range mt6379_charger_ranges[MT6379_RANGE_F_MAX] = {
	LINEAR_RANGE_IDX(MT6379_RANGE_F_BATINT, 3900000, 0x0, 0x51, 10000),
	LINEAR_RANGE_IDX(MT6379_RANGE_F_IBUS_AICR, 100000, 0x0, 0xA7, 25000),
	LINEAR_RANGE_IDX(MT6379_RANGE_F_WLIN_AICR, 100000, 0x0, 0x7F, 25000),
	LINEAR_RANGE_IDX(MT6379_RANGE_F_VBUS_MIVR, 3900000, 0x0, 0xB5, 100000),
	LINEAR_RANGE_IDX(MT6379_RANGE_F_WLIN_MIVR, 3900000, 0x0, 0xB5, 100000),
	LINEAR_RANGE_IDX(MT6379_RANGE_F_VREC, 100000, 0x0, 0x1, 100000),
	LINEAR_RANGE_IDX(MT6379_RANGE_F_CV, 3900000, 0x0, 0x51, 10000),
	LINEAR_RANGE_IDX(MT6379_RANGE_F_CC, 300000, 0x6, 0x50, 50000),
	LINEAR_RANGE_IDX(MT6379_RANGE_F_CHG_TMR, 5, 0x0, 0x3, 5),
	LINEAR_RANGE_IDX(MT6379_RANGE_F_IEOC, 100000, 0x0, 0x3A, 50000),
	LINEAR_RANGE_IDX(MT6379_RANGE_F_EOC_TIME, 0, 0x0, 0x3, 15),
	LINEAR_RANGE_IDX(MT6379_RANGE_F_VSYSOV, 4600000, 0, 7, 100000),
	LINEAR_RANGE_IDX(MT6379_RANGE_F_VSYSMIN, 3200000, 0, 0xF, 100000),
	LINEAR_RANGE_IDX(MT6379_RANGE_F_PE20_CODE, 5500000, 0, 0x1D, 500000),
	LINEAR_RANGE_IDX(MT6379_RANGE_F_IPREC, 50000, 0, 0x27, 50000),
	LINEAR_RANGE_IDX(MT6379_RANGE_F_AICC_RPT, 100000, 0x0, 0xA7, 25000), /* same as aicr */
	LINEAR_RANGE_IDX(MT6379_RANGE_F_OTG_LBP, 2700000, 0x0, 0x7, 100000),
	LINEAR_RANGE_IDX(MT6379_RANGE_F_OTG_OCP, 3500000, 0x0, 0x3, 1000000),
	LINEAR_RANGE_IDX(MT6379_RANGE_F_OTG_CC, 500000, 0x0, 0x6, 3000000),
	LINEAR_RANGE_IDX(MT6379_RANGE_F_IRCMP_R, 0, 0x0, 0xA, 5),
	LINEAR_RANGE_IDX(MT6379_RANGE_F_IRCMP_V, 0, 0x0, 0x14, 10),
};


static const struct mt6379_charger_field mt6379_charger_fields[F_MAX] = {
	MT6379_CHARGER_FIELD(F_SHIP_RST_DIS, MT6379_REG_CORE_CTRL2, 0, 0),
	MT6379_CHARGER_FIELD(F_PD_MDEN, MT6379_REG_CORE_CTRL2, 1, 1),
	MT6379_CHARGER_FIELD(F_ST_PWR_RDY, MT6379_REG_CHG_STAT0, 0, 0),
	MT6379_CHARGER_FIELD(F_ST_MIVR, MT6379_REG_CHG_STAT1, 7, 7),
	MT6379_CHARGER_FIELD(F_ST_AICC_DONE, MT6379_REG_CHG_STAT2, 2, 2),
	MT6379_CHARGER_FIELD(F_ST_USBID, MT6379_REG_USBID_STAT, 0, 0),
	MT6379_CHARGER_FIELD_RANGE(F_BATINT, MT6379_REG_CHG_BATPRO, 0, 6),
	MT6379_CHARGER_FIELD(F_BATPROTECT_EN, MT6379_REG_CHG_BATPRO, 7, 7),
	MT6379_CHARGER_FIELD(F_PP_PG_FLAG, MT6379_REG_CHG_TOP1, 7, 7),
	MT6379_CHARGER_FIELD(F_BATFET_DIS, MT6379_REG_CHG_TOP1, 6, 6),
	MT6379_CHARGER_FIELD(F_BATFET_DISDLY, MT6379_REG_CHG_COMP2, 5, 7),
	MT6379_CHARGER_FIELD(F_QON_RST_EN, MT6379_REG_CHG_TOP1, 3, 3),
	MT6379_CHARGER_FIELD(F_UUG_FULLON, MT6379_REG_CHG_TOP2, 3, 3),
	MT6379_CHARGER_FIELD(F_CHG_BYPASS, MT6379_REG_CHG_TOP2, 1, 1),
	MT6379_CHARGER_FIELD_RANGE(F_IBUS_AICR, MT6379_REG_CHG_IBUS_AICR, 0, 7),
	MT6379_CHARGER_FIELD(F_ILIM_EN, MT6379_REG_CHG_WLIN_AICR, 7, 7),
	MT6379_CHARGER_FIELD_RANGE(F_WLIN_AICR, MT6379_REG_CHG_WLIN_AICR, 0, 6),
	MT6379_CHARGER_FIELD_RANGE(F_VBUS_MIVR, MT6379_REG_CHG_VBUS_MIVR, 0, 7),
	MT6379_CHARGER_FIELD_RANGE(F_WLIN_MIVR, MT6379_REG_CHG_WLIN_MIVR, 0, 7),
	MT6379_CHARGER_FIELD_RANGE(F_VREC, MT6379_REG_CHG_VCHG, 7, 7),
	MT6379_CHARGER_FIELD_RANGE(F_CV, MT6379_REG_CHG_VCHG, 0, 6),
	MT6379_CHARGER_FIELD_RANGE(F_CC, MT6379_REG_CHG_ICHG, 0, 6),
	MT6379_CHARGER_FIELD(F_CHG_TMR_EN, MT6379_REG_CHG_TMR, 7, 7),
	MT6379_CHARGER_FIELD(F_CHG_TMR_2XT, MT6379_REG_CHG_TMR, 6, 6),
	MT6379_CHARGER_FIELD_RANGE(F_CHG_TMR, MT6379_REG_CHG_TMR, 4, 5),
	MT6379_CHARGER_FIELD_RANGE(F_IEOC, MT6379_REG_CHG_EOC1, 0, 5),
	MT6379_CHARGER_FIELD(F_WLIN_FST, MT6379_REG_CHG_EOC2, 7, 7),
	MT6379_CHARGER_FIELD(F_CHGIN_OV, MT6379_REG_CHG_EOC2, 4, 6),
	MT6379_CHARGER_FIELD_RANGE(F_EOC_TIME, MT6379_REG_CHG_EOC2, 2, 3),
	MT6379_CHARGER_FIELD(F_TE, MT6379_REG_CHG_EOC2, 1, 1),
	MT6379_CHARGER_FIELD(F_EOC_RST, MT6379_REG_CHG_EOC2, 0, 0),
	MT6379_CHARGER_FIELD(F_DISCHARGE_EN, MT6379_REG_CHG_VSYS, 7, 7),
	MT6379_CHARGER_FIELD_RANGE(F_VSYSOV, MT6379_REG_CHG_VSYS, 4, 6),
	MT6379_CHARGER_FIELD_RANGE(F_VSYSMIN, MT6379_REG_CHG_VSYS, 0, 3),
	MT6379_CHARGER_FIELD(F_HZ, MT6379_REG_CHG_WDT, 6, 6),
	MT6379_CHARGER_FIELD(F_BUCK_EN, MT6379_REG_CHG_WDT, 5, 5),
	MT6379_CHARGER_FIELD(F_CHG_EN, MT6379_REG_CHG_WDT, 4, 4),
	MT6379_CHARGER_FIELD(F_WDT_EN, MT6379_REG_CHG_WDT, 3, 3),
	MT6379_CHARGER_FIELD(F_WDT_RST, MT6379_REG_CHG_WDT, 2, 2),
	MT6379_CHARGER_FIELD(F_WDT_TIME, MT6379_REG_CHG_WDT, 0, 1),
	MT6379_CHARGER_FIELD(F_PE_EN, MT6379_REG_CHG_PUMPX, 7, 7),
	MT6379_CHARGER_FIELD(F_PE_SEL, MT6379_REG_CHG_PUMPX, 6, 6),
	MT6379_CHARGER_FIELD(F_PE10_INC, MT6379_REG_CHG_PUMPX, 5, 5),
	MT6379_CHARGER_FIELD_RANGE(F_PE20_CODE, MT6379_REG_CHG_PUMPX, 0, 4),
	MT6379_CHARGER_FIELD(F_AICC_EN, MT6379_REG_CHG_AICC, 7, 7),
	MT6379_CHARGER_FIELD(F_AICC_ONESHOT, MT6379_REG_CHG_AICC, 6, 6),
	MT6379_CHARGER_FIELD_RANGE(F_IPREC, MT6379_REG_CHG_IPREC, 0, 5),
	MT6379_CHARGER_FIELD_RANGE(F_AICC_RPT, MT6379_REG_CHG_AICC_RPT, 0, 7),
	MT6379_CHARGER_FIELD_RANGE(F_OTG_LBP, MT6379_REG_CHG_OTG_LBP, 0, 2),
	MT6379_CHARGER_FIELD(F_SEAMLESS_OTG, MT6379_REG_CHG_OTG_C, 7, 7),
	MT6379_CHARGER_FIELD(F_OTG_THERMAL_EN, MT6379_REG_CHG_OTG_C, 6, 6),
	MT6379_CHARGER_FIELD_RANGE(F_OTG_OCP, MT6379_REG_CHG_OTG_C, 4, 5),
	MT6379_CHARGER_FIELD(F_OTG_WLS, MT6379_REG_CHG_OTG_C, 3, 3),
	MT6379_CHARGER_FIELD_RANGE(F_OTG_CC, MT6379_REG_CHG_OTG_C, 0, 2),
	MT6379_CHARGER_FIELD(F_IRCMP_EN, MT6379_REG_CHG_COMP1, 7, 7),
	MT6379_CHARGER_FIELD_RANGE(F_IRCMP_R, MT6379_REG_CHG_COMP1, 3, 6),
	MT6379_CHARGER_FIELD_RANGE(F_IRCMP_V, MT6379_REG_CHG_COMP2, 0, 4),
	MT6379_CHARGER_FIELD(F_IC_STAT, MT6379_REG_CHG_STAT, 0, 3),
	MT6379_CHARGER_FIELD(F_FORCE_VBUS_SINK, MT6379_REG_CHG_HD_TOP1, 5, 5),
	MT6379_CHARGER_FIELD(F_IS_TDET, MT6379_REG_USBID_CTRL1, 2, 4),
	MT6379_CHARGER_FIELD(F_ID_RUPSEL, MT6379_REG_USBID_CTRL1, 5, 6),
	MT6379_CHARGER_FIELD(F_USBID_EN, MT6379_REG_USBID_CTRL1, 7, 7),
	MT6379_CHARGER_FIELD(F_USBID_FLOATING, MT6379_REG_USBID_CTRL2, 1, 1),
	MT6379_CHARGER_FIELD(F_BC12_EN, MT6379_REG_BC12_FUNC, 7, 7),
	MT6379_CHARGER_FIELD(F_PORT_STAT, MT6379_REG_BC12_STAT, 0, 3),
	MT6379_CHARGER_FIELD(F_MANUAL_MODE, MT6379_REG_DPDM_CTRL1, 7, 7),
	MT6379_CHARGER_FIELD(F_DPDM_SW_VCP_EN, MT6379_REG_DPDM_CTRL1, 5, 5),
	MT6379_CHARGER_FIELD(F_DP_DET_EN, MT6379_REG_DPDM_CTRL1, 1, 1),
	MT6379_CHARGER_FIELD(F_DM_DET_EN, MT6379_REG_DPDM_CTRL1, 0, 0),
	MT6379_CHARGER_FIELD(F_DP_LDO_EN, MT6379_REG_DPDM_CTRL2, 7, 7),
	MT6379_CHARGER_FIELD(F_DP_LDO_VSEL, MT6379_REG_DPDM_CTRL2, 4, 6),
	MT6379_CHARGER_FIELD(F_DP_PULL_REN, MT6379_REG_DPDM_CTRL4, 7, 7),
	MT6379_CHARGER_FIELD(F_DP_PULL_RSEL, MT6379_REG_DPDM_CTRL4, 5, 6),
};

static int mt6379_charger_init_rmap_fields(struct mt6379_charger_data *cdata)
{
	int i = 0;
	const struct mt6379_charger_field *fds = mt6379_charger_fields;

	for (i = 0; i < F_MAX; i++) {
		cdata->rmap_fields[i] = devm_regmap_field_alloc(cdata->dev,
							       cdata->rmap,
							       fds[i].field);
		if (IS_ERR(cdata->rmap_fields[i]))
			return dev_err_probe(cdata->dev,
					     PTR_ERR(cdata->rmap_fields[i]),
					     "Failed to allocate regmap fields[%s]\n",
					     fds[i].name);
	}
	return 0;
}

int mt6379_charger_field_get(struct mt6379_charger_data *cdata,
			     enum mt6379_charger_reg_field fd, u32 *val)
{
	int ret = 0;
	u32 regval = 0;
	u32 idx = fd;

	if (!mt6379_charger_fields[idx].inited)
		return -EOPNOTSUPP;

	ret = regmap_field_read(cdata->rmap_fields[idx], &regval);
	if (ret < 0)
		return ret;

	if (mt6379_charger_fields[idx].range)
		return linear_range_get_value(mt6379_charger_fields[idx].range,
					      regval, val);

	*val = regval;
	return 0;
}

int mt6379_charger_field_set(struct mt6379_charger_data *cdata,
			     enum mt6379_charger_reg_field fd, unsigned int val)
{
	int ret = 0;
	bool f;
	const struct linear_range *r;
	u32 idx = fd;

	if (!mt6379_charger_fields[idx].inited)
		return -EOPNOTSUPP;

	if (mt6379_charger_fields[idx].range) {
		r = mt6379_charger_fields[idx].range;

		/* MIVR should get high selector */
		if (idx == F_VBUS_MIVR || idx == F_WLIN_MIVR) {
			ret = linear_range_get_selector_high(r, val, &val, &f);
			if (ret)
				val = r->max_sel;
		} else
			linear_range_get_selector_within(r, val, &val);
	}

	return regmap_field_write(cdata->rmap_fields[idx], val);
}

static const struct mt6379_charger_platform_data mt6379_charger_pdata_def = {
	.aicr = 3225,
	.mivr = 4400,
	.ichg = 2000,
	.ieoc = 150,
	.cv = 4200,
	.wdt_time = 40000,
	.vbus_ov = 14500,
	.vrec = 100,
	.ircmp_v = 0,
	.ircmp_r = 0,
	.chg_tmr = 10,
	.nr_port = 1,
	.wdt_en = false,
	.te_en = true,
	.chg_tmr_en = true,
	.chgdev_name = "primary_chg",
	.usb_killer_detect = false,
};


static const u32 mt6379_otg_cc_ma[] = {
	500000, 800000, 1100000, 1400000, 1700000, 2000000, 2300000,
};

static int mt6379_set_voltage_sel(struct regulator_dev *rdev, unsigned int sel)
{
	const struct regulator_desc *desc = rdev->desc;
	struct regmap *rmap = rdev_get_regmap(rdev);

	sel <<= ffs(desc->vsel_mask) - 1;
	sel = cpu_to_be16(sel);

	return regmap_bulk_write(rmap, desc->vsel_reg, &sel, 2);
}

static int mt6379_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *desc = rdev->desc;
	struct regmap *rmap = rdev_get_regmap(rdev);
	unsigned int val = 0;
	int ret = 0;

	ret = regmap_bulk_read(rmap, desc->vsel_reg, &val, 2);
	if (ret)
		return ret;

	val = be16_to_cpu(val);
	val &= desc->vsel_mask;
	val >>= ffs(desc->vsel_mask) - 1;
	return val;
}

static bool mt6379_charger_is_usb_killer(struct mt6379_charger_data *cdata)
{
	int i = 0, ret = 0, vdp = 0, vdm = 0;
	bool killer = false;
	static const u32 vdiff = 200;
	struct mt6379_charger_platform_data *pdata = dev_get_platdata(cdata->dev);
	static const struct {
		enum mt6379_charger_reg_field fd;
		u32 val;
	} settings[] = {
		{ F_MANUAL_MODE, 1 },
		{ F_DPDM_SW_VCP_EN, 1 },
		{ F_DP_DET_EN, 1 },
		{ F_DM_DET_EN, 1 },
		{ F_DP_LDO_VSEL, MT6379_DP_LDO_VSEL_1800MV },
		{ F_DP_LDO_EN, 1 },
		{ F_DP_PULL_RSEL, MT6379_DP_PULL_RSEL_1_2_K },
		{ F_DP_PULL_REN, 1 },
	};

	if (!pdata->usb_killer_detect) {
		dev_info(cdata->dev, "usb killer is not set\n");
		return false;
	}

	/* Turn on usb dp 1.8V */
	for (i = 0; i < ARRAY_SIZE(settings); i++) {
		ret = mt6379_charger_field_set(cdata, settings[i].fd, settings[i].val);
		if (ret < 0)
			goto recover;
	}
	--i;

	/* check usb DPDM */
	ret = iio_read_channel_processed(&cdata->iio_adcs[ADC_CHAN_USBDP], &vdp);
	if (ret < 0) {
		dev_err(cdata->dev, "Failed to read usb DP voltage\n");
		goto recover;
	}
	ret = iio_read_channel_processed(&cdata->iio_adcs[ADC_CHAN_USBDM], &vdm);
	if (ret < 0) {
		dev_err(cdata->dev, "Failed to read usb DM voltage\n");
		goto recover;
	}
	vdp = U_TO_M(vdp);
	vdm = U_TO_M(vdm);
	dev_info(cdata->dev, "dp = %dmV, dm = %dmV, vdiff = %dmV\n", vdp, vdm,
		 abs(vdp - vdm));
	if (abs(vdp - vdm) < vdiff) {
		dev_info(cdata->dev, "suspect usb killer\n");
		killer = true;
	}
recover:
	for (; i >= 0; i--) /* set to default value */
		mt6379_charger_field_set(cdata, settings[i].fd, 0);

	return killer;
}

static int mt6379_otg_regulator_enable(struct regulator_dev *rdev)
{
	struct mt6379_charger_data *cdata = rdev->reg_data;
	int ret = 0;

	if (mt6379_charger_is_usb_killer(cdata))
		return -EIO;

	/* disable PP_CV_FLOW_IDLE */
	ret = mt6379_enable_hm(cdata, true);
	ret = regmap_update_bits(cdata->rmap, MT6379_REG_CHG_HD_PP7, BIT(5), 0);
	if (ret) {
		ret = mt6379_enable_hm(cdata, false);
		return -EINVAL;
	}
	ret = mt6379_enable_hm(cdata, false);

	dev_info(cdata->dev, "%s, ret = %d\n", __func__, ret);
	return regulator_enable_regmap(rdev);
}

static int mt6379_otg_regulator_disable(struct regulator_dev *rdev)
{
	struct mt6379_charger_data *cdata = rdev->reg_data;
	int ret = 0;

	/* enable PP_CV_FLOW_IDLE */
	ret = mt6379_enable_hm(cdata, true);
	ret = regmap_update_bits(cdata->rmap, MT6379_REG_CHG_HD_PP7, BIT(5), BIT(5));
	if (ret) {
		ret = mt6379_enable_hm(cdata, false);
		return -EINVAL;
	}
	ret = mt6379_enable_hm(cdata, false);
	dev_info(cdata->dev, "%s, ret = %d\n", __func__, ret);
	return regulator_disable_regmap(rdev);
}

static int mt6379_otg_set_current_limit(struct regulator_dev *rdev,
					int min_uA, int max_uA)
{
	struct mt6379_charger_data *cdata = rdev->reg_data;
	const struct regulator_desc *desc = rdev->desc;
	int i, shift = ffs(desc->csel_mask) - 1;

	for (i = 0; i < ARRAY_SIZE(mt6379_otg_cc_ma); i++) {
		if (min_uA <= mt6379_otg_cc_ma[i])
			break;
	}
	if (i == ARRAY_SIZE(mt6379_otg_cc_ma)) {
		dev_notice(cdata->dev, "%s: out of current range\n", __func__);
		return -EINVAL;
	}
	dev_info(cdata->dev, "%s: select otg_cc = %d\n", __func__,
		 mt6379_otg_cc_ma[i]);
	return regmap_update_bits(cdata->rmap, desc->csel_reg, desc->csel_mask,
				  i << shift);
}

static const struct regulator_ops mt6379_otg_regulator_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.enable = mt6379_otg_regulator_enable,
	.disable = mt6379_otg_regulator_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.set_voltage_sel = mt6379_set_voltage_sel,
	.get_voltage_sel = mt6379_get_voltage_sel,
	.set_current_limit = mt6379_otg_set_current_limit,
	.get_current_limit = regulator_get_current_limit_regmap,
};

static const struct regulator_desc mt6379_charger_otg_rdesc = {
	.of_match = "usb-otg-vbus-regulator",
	.name = "mt6379-usb-otg-vbus",
	.ops = &mt6379_otg_regulator_ops,
	.owner = THIS_MODULE,
	.type = REGULATOR_VOLTAGE,
	.min_uV = 4850000,
	.uV_step = 25000,
	.n_voltages = 287,
	.linear_min_sel = 20,
	.vsel_reg = MT6379_REG_CHG_OTG_CV_MSB,
	.vsel_mask = 0x1FF,
	.enable_reg = MT6379_REG_CHG_TOP2,
	.enable_mask = BIT(0),
	.curr_table = mt6379_otg_cc_ma,
	.n_current_limits = ARRAY_SIZE(mt6379_otg_cc_ma),
	.csel_reg = MT6379_REG_CHG_OTG_C,
	.csel_mask = GENMASK(2, 0),
};

static int mt6379_init_otg_regulator(struct mt6379_charger_data *cdata)
{
	struct regulator_config config = {
		.dev = cdata->dev,
		.regmap = cdata->rmap,
		.driver_data = cdata,
	};

	cdata->rdev = devm_regulator_register(cdata->dev, &mt6379_charger_otg_rdesc,
					      &config);
	return PTR_ERR_OR_ZERO(cdata->rdev);
}

static char *mt6379_psy_supplied_to[] = {
	"battery",
	"mtk-master-charger",
};

static enum power_supply_usb_type mt6379_charger_psy_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_DCP,
};

static enum power_supply_property mt6379_charger_properties[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
	POWER_SUPPLY_PROP_PRECHARGE_CURRENT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CALIBRATE,
	POWER_SUPPLY_PROP_ENERGY_EMPTY,
	POWER_SUPPLY_PROP_TYPE,
};

static const char *const mt6379_attach_trig_names[] = {
	"ignore", "pwr_rdy", "typec",
};

static void __maybe_unused mt6379_charger_check_dpdm_ov(struct mt6379_charger_data *cdata, int attach)
{
	struct chgdev_notify *mtk_chg_noti = &(cdata->chgdev->noti);
	int ret = 0, vdp = 0, vdm = 0;

	if (attach == ATTACH_TYPE_NONE)
		return;

	/* Check if USB DPDM is Over Voltage */
	ret = iio_read_channel_processed(&cdata->iio_adcs[ADC_CHAN_USBDP], &vdp);
	if (ret < 0)
		dev_notice(cdata->dev, "%s: Failed to read USB DP voltage\n", __func__);

	ret = iio_read_channel_processed(&cdata->iio_adcs[ADC_CHAN_USBDM], &vdm);
	if (ret < 0)
		dev_notice(cdata->dev, "%s: Failed to read USB DM voltage\n", __func__);

	vdp = U_TO_M(vdp);
	vdm = U_TO_M(vdm);
	if (vdp >= DPDM_OV_THRESHOLD_MV || vdm >= DPDM_OV_THRESHOLD_MV) {
		dev_notice(cdata->dev, "%s: USB DPDM OV! valid: %dmV vdp: %dmV,vdm: %dmV\n",
			   __func__, DPDM_OV_THRESHOLD_MV, vdp, vdm);
		mtk_chg_noti->dpdmov_stat = true;
		charger_dev_notify(cdata->chgdev, CHARGER_DEV_NOTIFY_DPDM_OVP);
	}
}

static enum power_supply_type mt6379_charger_get_psy_type(
	struct mt6379_charger_data *cdata, int idx)
{
	return POWER_SUPPLY_TYPE_USB;
}

static int mt6379_charger_set_online(struct mt6379_charger_data *cdata,
				     enum mt6379_attach_trigger trig,
				     int attach)
{
	struct mt6379_charger_platform_data *pdata = dev_get_platdata(cdata->dev);
	int i = 0, idx = ONLINE_GET_IDX(attach);
	int active_idx = 0, pre_active_idx = 0;

	dev_info(cdata->dev, "trig=%s,attach=0x%x\n",
		 mt6379_attach_trig_names[trig], attach);
	/* if attach trigger is not match, ignore it */
	if (pdata->attach_trig != trig) {
		dev_info(cdata->dev, "trig=%s ignored\n",
			 mt6379_attach_trig_names[trig]);
		return 0;
	}
	attach = ONLINE_GET_ATTACH(attach);

	mutex_lock(&cdata->attach_lock);
	if (attach == ATTACH_TYPE_NONE)
		cdata->bc12_dn[idx] = false;
	if (!cdata->bc12_dn[idx])
		atomic_set(&cdata->attach[idx], attach);

	active_idx = cdata->active_idx;
	pre_active_idx = active_idx;
	for (i = 0; i < pdata->nr_port; i++) {
		if (atomic_read(&cdata->attach[i]) > ATTACH_TYPE_NONE) {
			active_idx = i;
			break;
		}
	}

	if (pdata->nr_port > 1 && attach == ATTACH_TYPE_TYPEC &&
	    !cdata->bc12_dn[idx]) {
		cdata->psy_type[idx] = mt6379_charger_get_psy_type(cdata, idx);
		cdata->bc12_dn[idx] = true;
		switch (cdata->psy_type[idx]) {
		case POWER_SUPPLY_TYPE_USB:
			cdata->psy_usb_type[idx] = POWER_SUPPLY_USB_TYPE_SDP;
			break;
		case POWER_SUPPLY_TYPE_USB_DCP:
		case POWER_SUPPLY_TYPE_APPLE_BRICK_ID:
			cdata->psy_type[idx] = POWER_SUPPLY_TYPE_USB_DCP;
			cdata->psy_usb_type[idx] = POWER_SUPPLY_USB_TYPE_DCP;
			break;
		case POWER_SUPPLY_TYPE_USB_CDP:
			cdata->psy_usb_type[idx] = POWER_SUPPLY_USB_TYPE_CDP;
			break;
		default:
			cdata->psy_type[idx] = POWER_SUPPLY_TYPE_USB;
			cdata->psy_usb_type[idx] = POWER_SUPPLY_USB_TYPE_DCP;
			break;
		}
	}
	cdata->psy_desc.type = cdata->psy_type[active_idx];

	cdata->active_idx = active_idx;

	if ((attach > ATTACH_TYPE_PD && cdata->bc12_dn[idx]) ||
	    (active_idx == pre_active_idx && idx != active_idx)) {
		mutex_unlock(&cdata->attach_lock);
		return 0;
	}
	mutex_unlock(&cdata->attach_lock);

	if (!queue_work(cdata->wq, &cdata->bc12_work))
		dev_notice(cdata->dev, "%s bc12 work already queued\n", __func__);
	return 0;
}

static int mt6379_charger_get_online(struct mt6379_charger_data *cdata,
				     union power_supply_propval *val)
{
	val->intval = atomic_read(&cdata->attach[cdata->active_idx]);
	return 0;
}

static int mt6379_get_charger_status(struct mt6379_charger_data *cdata)
{
	int ret = 0;
	u32 stat = 0, chg_en = 0;
	union power_supply_propval online;

	ret = mt6379_charger_get_online(cdata, &online);
	if (ret) {
		dev_err(cdata->dev, "Failed to get online status\n");
		return ret;
	}

	if (!online.intval)
		return POWER_SUPPLY_STATUS_DISCHARGING;

	ret = mt6379_charger_field_get(cdata, F_CHG_EN, &chg_en);
	if (ret < 0)
		return ret;
	ret = mt6379_charger_field_get(cdata, F_IC_STAT, &stat);
	if (ret < 0)
		return ret;
	switch (stat) {
	case CHG_STAT_OTG:
		return POWER_SUPPLY_STATUS_DISCHARGING;
	case CHG_STAT_VBUS_RDY...CHG_STAT_BKGND:
		if (chg_en)
			return POWER_SUPPLY_STATUS_CHARGING;
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	case CHG_STAT_DONE:
		return POWER_SUPPLY_STATUS_FULL;
	case CHG_STAT_FAULT:
	case CHG_STAT_SLEEP:
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	default:
		return POWER_SUPPLY_STATUS_UNKNOWN;
	};
	return POWER_SUPPLY_STATUS_UNKNOWN;
}

static int mt6379_get_vbat_monitor(struct mt6379_charger_data *cdata,
				   union power_supply_propval *val)
{
	u32 vbat_mon = 0;
	int ret = 0;

	ret = iio_read_channel_processed(&cdata->iio_adcs[ADC_CHAN_VBATMON], &vbat_mon);
	if (ret)
		dev_info(cdata->dev, "Failed to read VBATMON(ret:%d)\n", ret);

	val->intval = vbat_mon;

	return ret;
}

static int mt6379_charger_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct mt6379_charger_data *cdata = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!cdata)
		return -ENODEV;

	switch (psp) {
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = "Mediatek";
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		ret = mt6379_charger_get_online(cdata, val);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		ret = mt6379_get_charger_status(cdata);
		if (ret < 0)
			return ret;
		val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = mt6379_charger_field_get(cdata, F_CC, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = linear_range_get_max_value(
			&mt6379_charger_ranges[MT6379_RANGE_F_CC]);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = mt6379_charger_field_get(cdata, F_CV, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		val->intval = linear_range_get_max_value(
			&mt6379_charger_ranges[MT6379_RANGE_F_CV]);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = mt6379_charger_field_get(cdata, F_IBUS_AICR, &val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = mt6379_charger_field_get(cdata, F_VBUS_MIVR, &val->intval);
		break;
	case POWER_SUPPLY_PROP_PRECHARGE_CURRENT:
		ret = mt6379_charger_field_get(cdata, F_IPREC, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		ret = mt6379_charger_field_get(cdata, F_IEOC, &val->intval);
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = cdata->psy_usb_type[cdata->active_idx];
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (cdata->psy_usb_type[cdata->active_idx] == POWER_SUPPLY_USB_TYPE_SDP)
			val->intval = 500000;
		else if (cdata->psy_usb_type[cdata->active_idx] == POWER_SUPPLY_USB_TYPE_DCP)
			val->intval = 3225000;
		else if (cdata->psy_usb_type[cdata->active_idx] == POWER_SUPPLY_USB_TYPE_CDP)
			val->intval = 1500000;
		else
			val->intval = 500000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (cdata->psy_usb_type[cdata->active_idx] == POWER_SUPPLY_USB_TYPE_DCP)
			val->intval = 22000000;
		else
			val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = cdata->psy_desc.type;
		break;
	case POWER_SUPPLY_PROP_CALIBRATE: /* for Gauge */
		ret = mt6379_get_vbat_monitor(cdata, val);
		break;
	case POWER_SUPPLY_PROP_ENERGY_EMPTY:
		val->intval = cdata->vbat0_flag;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int mt6379_charger_set_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       const union power_supply_propval *val)
{
	int ret = 0;
	struct mt6379_charger_data *cdata = power_supply_get_drvdata(psy);

	if (!cdata)
		return -ENODEV;

	dev_info(cdata->dev, "psp=%d, val = %d\n", psp, val->intval);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		return mt6379_charger_set_online(cdata, ATTACH_TRIG_TYPEC, val->intval);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		return mt6379_charger_field_set(cdata, F_CC, val->intval);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		return mt6379_charger_field_set(cdata, F_CV, val->intval);
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (cdata->bypass_mode_entered)
			return 0;
		return mt6379_charger_field_set(cdata, F_IBUS_AICR, val->intval);
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		if (cdata->bypass_mode_entered)
			return 0;
		return mt6379_charger_field_set(cdata, F_VBUS_MIVR, val->intval);
	case POWER_SUPPLY_PROP_PRECHARGE_CURRENT:
		return mt6379_charger_field_set(cdata, F_IPREC, val->intval);
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		return mt6379_charger_field_set(cdata, F_IEOC, val->intval);
	case POWER_SUPPLY_PROP_ENERGY_EMPTY:
		cdata->vbat0_flag = val->intval;
		break;
	default:
		ret = -EINVAL;
		break;
	};
	return ret;
}

static int mt6379_charger_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
	case POWER_SUPPLY_PROP_PRECHARGE_CURRENT:
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_ENERGY_EMPTY:
		return 1;
	default:
		return 0;
	};
	return 0;
}

static const struct power_supply_desc mt6379_charger_psy_desc = {
	.name = "mt6379-charger",
	.type = POWER_SUPPLY_TYPE_USB,
	.usb_types = mt6379_charger_psy_usb_types,
	.num_usb_types = ARRAY_SIZE(mt6379_charger_psy_usb_types),
	.properties = mt6379_charger_properties,
	.num_properties = ARRAY_SIZE(mt6379_charger_properties),
	.get_property = mt6379_charger_get_property,
	.set_property = mt6379_charger_set_property,
	.property_is_writeable = mt6379_charger_property_is_writeable,
};

static int mt6379_set_shipping_mode(struct mt6379_charger_data *cdata)
{
	int ret = 0;

	ret = mt6379_charger_field_set(cdata, F_SHIP_RST_DIS, 1);
	if (ret < 0) {
		dev_err(cdata->dev, "Failed to disable ship reset\n");
		return ret;
	}

	ret = mt6379_charger_field_set(cdata, F_BATFET_DISDLY, 0);
	if (ret < 0) {
		dev_err(cdata->dev, "Failed to disable ship mode delay\n");
		return ret;
	}

	/* To shutdown system even with TA, disable buck_en here */
	ret = mt6379_charger_field_set(cdata, F_BUCK_EN, 0);
	if (ret < 0) {
		dev_notice(cdata->dev, "Failed to disable chg buck en\n");
		return ret;
	}

	return mt6379_charger_field_set(cdata, F_BATFET_DIS, 1);
}

static ssize_t shipping_mode_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct mt6379_charger_data *cdata = power_supply_get_drvdata(to_power_supply(dev));
	unsigned long magic = 0;
	int ret = 0;

	ret = kstrtoul(buf, 0, &magic);
	if (ret < 0) {
		dev_warn(dev, "parsing number fail\n");
		return ret;
	}
	if (magic != 5526789)
		return -EINVAL;

	ret = mt6379_set_shipping_mode(cdata);
	return ret < 0 ? ret : count;

}
static DEVICE_ATTR_WO(shipping_mode);

static ssize_t bypass_iq_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct mt6379_charger_data *cdata = power_supply_get_drvdata(to_power_supply(dev));
	unsigned int val = 0;
	int ret = 0;

	ret = regmap_read(cdata->rmap, MT6379_REG_CHG_BYPASS_IQ, &val);
	if (ret)
		return ret;

	dev_info(dev, "%s val = 0x%02x\n", __func__, val);
	return sysfs_emit(buf, "%d uA\n", 20 * (1 + val));
}
static DEVICE_ATTR_RO(bypass_iq);

static ssize_t bypass_mode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mt6379_charger_data *cdata = power_supply_get_drvdata(to_power_supply(dev));

	return sysfs_emit(buf, "%s\n", cdata->bypass_mode_entered ? "Y" : "N");
}

static ssize_t bypass_mode_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct mt6379_charger_data *cdata = power_supply_get_drvdata(to_power_supply(dev));
	int ret = 0;
	bool enter = false;

	ret = kstrtobool(buf, &enter);
	if (ret)
		return ret;

	if (enter == cdata->bypass_mode_entered)
		return count;

	if (enter) {
		ret = mt6379_charger_field_set(cdata, F_VBUS_MIVR, 3900000);
		if (ret)
			return ret;
		ret = mt6379_charger_field_set(cdata, F_IBUS_AICR, 3000000);
		ret = mt6379_charger_field_set(cdata, F_CHG_BYPASS, 1);
		if (ret)
			return ret;
		cdata->bypass_mode_entered = true;
	} else {
		ret = mt6379_charger_field_set(cdata, F_CHG_BYPASS, 0);
		if (ret)
			return ret;
	}
	cdata->bypass_mode_entered = enter;
	return count;
}
static DEVICE_ATTR_RW(bypass_mode);

static struct attribute *mt6379_charger_psy_sysfs_attrs[] = {
	&dev_attr_bypass_mode.attr,
	&dev_attr_shipping_mode.attr,
	&dev_attr_bypass_iq.attr,
	NULL
};
ATTRIBUTE_GROUPS(mt6379_charger_psy_sysfs); /* mt6379_charger_psy_sysfs_groups */

static int mt6379_charger_init_psy(struct mt6379_charger_data *cdata)
{
	struct mt6379_charger_platform_data *pdata = dev_get_platdata(cdata->dev);
	struct power_supply_config config = {
		.drv_data = cdata,
		.of_node = dev_of_node(cdata->dev),
		.supplied_to = mt6379_psy_supplied_to,
		.num_supplicants = ARRAY_SIZE(mt6379_psy_supplied_to),
		.attr_grp = mt6379_charger_psy_sysfs_groups,
	};

	memcpy(&cdata->psy_desc, &mt6379_charger_psy_desc, sizeof(cdata->psy_desc));
	cdata->psy_desc.name = pdata->chgdev_name;
	cdata->psy = devm_power_supply_register(cdata->dev, &cdata->psy_desc, &config);

	return PTR_ERR_OR_ZERO(cdata->psy);
}

static const char *const mt6379_port_stat_names[] = {
	[PORT_STAT_NOINFO] = "No Info",
	[PORT_STAT_APPLE_10W] = "Apple 10W",
	[PORT_STAT_SS_TA] = "SS",
	[PORT_STAT_APPLE_5W] = "Apple 5W",
	[PORT_STAT_APPLE_12W] = "Apple 12W",
	[PORT_STAT_UNKNOWN_TA] = "Unknown TA",
	[PORT_STAT_SDP] = "SDP",
	[PORT_STAT_CDP] = "CDP",
	[PORT_STAT_DCP] = "DCP",
};

enum mt6379_usbsw {
	USBSW_CHG = 0,
	USBSW_USB,
};

#define PHY_MODE_BC11_SET 1
#define PHY_MODE_BC11_CLR 2
static int mt6379_charger_set_usbsw(struct mt6379_charger_data *cdata,
				    enum mt6379_usbsw usbsw)
{
	struct phy *phy;
	int ret = 0, mode = (usbsw == USBSW_CHG) ? PHY_MODE_BC11_SET :
						   PHY_MODE_BC11_CLR;

	dev_info(cdata->dev, "usbsw=%d\n", usbsw);
	phy = phy_get(cdata->dev, "usb2-phy");
	if (IS_ERR_OR_NULL(phy)) {
		dev_err(cdata->dev, "failed to get usb2-phy\n");
		return -ENODEV;
	}
	ret = phy_set_mode_ext(phy, PHY_MODE_USB_DEVICE, mode);
	if (ret)
		dev_err(cdata->dev, "failed to set phy ext mode\n");
	phy_put(cdata->dev, phy);
	return ret;
}

static bool is_usb_rdy(struct device *dev)
{
	bool ready = true;
	struct device_node *node;

	node = of_parse_phandle(dev->of_node, "usb", 0);
	if (node) {
		ready = !of_property_read_bool(node, "cdp-block");
		dev_info(dev, "usb ready = %d\n", ready);
	} else
		dev_warn(dev, "usb node missing or invalid\n");
	return ready;
}

static int mt6379_charger_enable_bc12(struct mt6379_charger_data *cdata, bool en)
{
	int i = 0, ret = 0, attach = 0;
	static const int max_wait_cnt = 250;

	dev_info(cdata->dev, "en=%d\n", en);
	if (en) {
		/* CDP port specific process */
		dev_info(cdata->dev, "check CDP block\n");
		for (i = 0; i < max_wait_cnt; i++) {
			if (is_usb_rdy(cdata->dev))
				break;
			attach = atomic_read(&cdata->attach[0]);
			if (attach == ATTACH_TYPE_PWR_RDY ||
				attach == ATTACH_TYPE_TYPEC)
				msleep(100);
			else {
				dev_notice(cdata->dev, "%s: change attach:%d, disable bc12\n",
					   __func__, attach);
				en = false;
				break;
			}
		}
		if (i == max_wait_cnt)
			dev_notice(cdata->dev, "CDP timeout\n");
		else
			dev_info(cdata->dev, "CDP free\n");
	}
	ret = mt6379_charger_set_usbsw(cdata, en ? USBSW_CHG : USBSW_USB);
	if (ret)
		return ret;
	return mt6379_charger_field_set(cdata, F_BC12_EN, en);
}

static int mt6379_charger_toggle_bc12(struct mt6379_charger_data *cdata)
{
	int ret;

	ret = mt6379_charger_enable_bc12(cdata, false);
	if (ret)
		dev_notice(cdata->dev, "%s: failed to disable bc12(%d)\n",
			   __func__, ret);
	return mt6379_charger_enable_bc12(cdata, true);
}

static void mt6379_charger_bc12_work_func(struct work_struct *work)
{
	struct mt6379_charger_data *cdata = container_of(work,
							 struct mt6379_charger_data,
							 bc12_work);
	struct mt6379_charger_platform_data *pdata = dev_get_platdata(cdata->dev);
	bool bc12_ctrl = !(pdata->nr_port > 1), bc12_en = false, rpt_psy = true;
	int ret = 0, attach = ATTACH_TYPE_NONE, active_idx = 0;
	u32 val = 0;

	mutex_lock(&cdata->attach_lock);
	active_idx = cdata->active_idx;
	attach = atomic_read(&cdata->attach[cdata->active_idx]);
	dev_info(cdata->dev, "attach=%d\n", attach);

	if (attach > ATTACH_TYPE_NONE && pdata->boot_mode == 5) {
		/* skip bc12 to speed up ADVMETA_BOOT */
		dev_notice(cdata->dev, "force SDP in meta mode\n");
		cdata->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		cdata->psy_type[active_idx] = POWER_SUPPLY_TYPE_USB;
		cdata->psy_usb_type[active_idx] = POWER_SUPPLY_USB_TYPE_SDP;
		goto out;
	}

	switch (attach) {
	case ATTACH_TYPE_NONE:
		/* Put UFCS detach event */
		if (active_idx == 0 && cdata->psy_desc.type == POWER_SUPPLY_TYPE_USB_DCP) {
			cdata->wait_for_ufcs_attach = false;
			ufcs_attach_change(cdata->ufcs, false);
		}

		cdata->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		cdata->psy_type[active_idx] = POWER_SUPPLY_TYPE_USB;
		cdata->psy_usb_type[active_idx] = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		goto out;
	case ATTACH_TYPE_TYPEC:
		if (pdata->nr_port > 1)
			goto out;
		fallthrough;
	case ATTACH_TYPE_PWR_RDY:
		if (!cdata->bc12_dn[active_idx]) {
			bc12_en = true;
			rpt_psy = false;
			goto out;
		}
		ret = mt6379_charger_field_get(cdata, F_PORT_STAT, &val);
		if (ret < 0) {
			dev_err(cdata->dev, "failed to get port stat\n");
			rpt_psy = false;
			goto out;
		}
		break;
	case ATTACH_TYPE_PD_SDP:
		val = PORT_STAT_SDP;
		break;
	case ATTACH_TYPE_PD_DCP:
		val = PORT_STAT_DCP;
		break;
	case ATTACH_TYPE_PD_NONSTD:
		val = PORT_STAT_UNKNOWN_TA;
		break;
	default:
		dev_info(cdata->dev,
			 "%s: using tradtional bc12 flow!\n", __func__);
		break;
	}

	switch (val) {
	case PORT_STAT_NOINFO:
		bc12_ctrl = false;
		rpt_psy = false;
		dev_info(cdata->dev, "%s no info\n", __func__);
		goto out;
	case PORT_STAT_APPLE_5W:
	case PORT_STAT_APPLE_10W:
	case PORT_STAT_APPLE_12W:
	case PORT_STAT_SS_TA:
	case PORT_STAT_DCP:
		cdata->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		cdata->psy_type[active_idx] = POWER_SUPPLY_TYPE_USB_DCP;
		cdata->psy_usb_type[active_idx] = POWER_SUPPLY_USB_TYPE_DCP;

		/* Put UFCS attach event */
		if (active_idx == 0 && val == PORT_STAT_DCP) {
			cdata->wait_for_ufcs_attach = true;
			ufcs_attach_change(cdata->ufcs, true);
		}
		break;
	case PORT_STAT_SDP:
		cdata->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		cdata->psy_type[active_idx] = POWER_SUPPLY_TYPE_USB;
		cdata->psy_usb_type[active_idx] = POWER_SUPPLY_USB_TYPE_SDP;
		break;
	case PORT_STAT_CDP:
		cdata->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
		cdata->psy_type[active_idx] = POWER_SUPPLY_TYPE_USB_CDP;
		cdata->psy_usb_type[active_idx] = POWER_SUPPLY_USB_TYPE_CDP;
		break;
	case PORT_STAT_UNKNOWN_TA:
		cdata->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		cdata->psy_type[active_idx] = POWER_SUPPLY_TYPE_USB;
		cdata->psy_usb_type[active_idx] = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	default:
		bc12_ctrl = false;
		rpt_psy = false;
		dev_info(cdata->dev, "Unknown port stat %d\n", val);
		goto out;
	}
	dev_info(cdata->dev, "port stat = %s\n", mt6379_port_stat_names[val]);
out:
	mutex_unlock(&cdata->attach_lock);
	if (bc12_ctrl) {
		//mt6379_charger_check_dpdm_ov(cdata, attach);
		if (mt6379_charger_enable_bc12(cdata, bc12_en) < 0)
			dev_notice(cdata->dev, "failed to set bc12 = %d\n", bc12_en);
	}
	if (rpt_psy)
		power_supply_changed(cdata->psy);
}

const struct mt6379_charger_dtprop mt6379_charger_dtprops[] = {
	MT6379_CHG_DTPROP("chg-tmr", chg_tmr, F_CHG_TMR, DTPROP_U32),
	MT6379_CHG_DTPROP("chg-tmr-en", chg_tmr_en, F_CHG_TMR_EN, DTPROP_BOOL),
	MT6379_CHG_DTPROP("ircmp-v", ircmp_v, F_IRCMP_V, DTPROP_U32),
	MT6379_CHG_DTPROP("ircmp-r", ircmp_r, F_IRCMP_R, DTPROP_U32),
	MT6379_CHG_DTPROP("wdt-time", wdt_time, F_WDT_TIME, DTPROP_U32),
	MT6379_CHG_DTPROP("wdt-en", wdt_en, F_WDT_EN, DTPROP_BOOL),
	MT6379_CHG_DTPROP("te-en", te_en, F_TE, DTPROP_BOOL),
	MT6379_CHG_DTPROP("mivr", mivr, F_VBUS_MIVR, DTPROP_U32),
	MT6379_CHG_DTPROP("aicr", aicr, F_IBUS_AICR, DTPROP_U32),
	MT6379_CHG_DTPROP("ichg", ichg, F_CC, DTPROP_U32),
	MT6379_CHG_DTPROP("ieoc", ieoc, F_IEOC, DTPROP_U32),
	MT6379_CHG_DTPROP("cv", cv, F_CV, DTPROP_U32),
	MT6379_CHG_DTPROP("vrec", vrec, F_VREC, DTPROP_U32),
	MT6379_CHG_DTPROP("chgin-ov", chgin_ov, F_CHGIN_OV, DTPROP_U32),
	MT6379_CHG_DTPROP("nr-port", nr_port, F_MAX, DTPROP_U32),
};

void mt6379_charger_parse_dt_helper(struct device *dev, void *pdata,
				    const struct mt6379_charger_dtprop *dp)
{
	int ret = 0;
	void *val = pdata + dp->offset;

	if (dp->type == DTPROP_BOOL)
		*((bool *)val) = device_property_read_bool(dev, dp->name);
	else {
		ret = device_property_read_u32(dev, dp->name, val);
		if (ret < 0)
			dev_info(dev, "property %s not found\n", dp->name);
	}
}

static int mt6379_charger_get_pdata(struct device *dev)
{
	int i = 0;
	u32 val = 0;
	struct device_node *np = dev->of_node, *boot_np, *pmic_uvlo_np;
	struct mt6379_charger_platform_data *pdata = dev_get_platdata(dev);
	const struct {
		u32 size;
		u32 tag;
		u32 boot_mode;
		u32 boot_type;
	} *tag;

	if (np) {
		pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		memcpy(pdata, &mt6379_charger_pdata_def, sizeof(*pdata));
		for (i = 0; i < ARRAY_SIZE(mt6379_charger_dtprops); i++)
			mt6379_charger_parse_dt_helper(dev, pdata,
							&mt6379_charger_dtprops[i]);
		pdata->usb_killer_detect =
			device_property_read_bool(dev, "usb-killer-detect");

		if (of_property_read_string(np, "chgdev-name", &pdata->chgdev_name))
			dev_notice(dev, "Failed to get chgdev_name\n");

		boot_np = of_parse_phandle(np, "boot-mode", 0);
		if (!boot_np) {
			dev_err(dev, "Failed to get boot-mode phandle\n");
			return -ENODEV;
		}
		tag = of_get_property(boot_np, "atag,boot", NULL);
		if (!tag) {
			dev_err(dev, "Failed to get atag,boot\n");
			return -EINVAL;
		}
		dev_info(dev, "sz:0x%x tag:0x%x mode:0x%x type:0x%x\n",
			 tag->size, tag->tag, tag->boot_mode, tag->boot_type);
		pdata->boot_mode = tag->boot_mode;
		pdata->boot_type = tag->boot_type;

		if (of_property_read_u32(np, "bc12-sel", &val) < 0 &&
				of_property_read_u32(np, "bc12_sel", &val) < 0) {
			dev_err(dev, "property bc12_sel not found\n");
			return -EINVAL;
		}

		if (val != MTK_CTD_BY_SUBPMIC &&
				val != MTK_CTD_BY_SUBPMIC_PWR_RDY)
			pdata->attach_trig = ATTACH_TRIG_IGNORE;
		else if (IS_ENABLED(CONFIG_TCPC_CLASS) &&
				val == MTK_CTD_BY_SUBPMIC)
			pdata->attach_trig = ATTACH_TRIG_TYPEC;
		else
			pdata->attach_trig = ATTACH_TRIG_PWR_RDY;

		pmic_uvlo_np = of_parse_phandle(np, "pmic-uvlo", 0);
		if (!pmic_uvlo_np)
			dev_notice(dev, "Failed to get pmic-uvlo phandle\n");

		if (of_property_read_u32(pmic_uvlo_np, "uvlo-level", &val) < 0)
			dev_notice(dev, "property uvlo-level not found, use default: %d mv\n",
					DEFAULT_PMIC_UVLO_MV);

		if (val != 0)
			pdata->pmic_uvlo = val;
		else
			pdata->pmic_uvlo = DEFAULT_PMIC_UVLO_MV;

		dev->platform_data = pdata;
	}
	return pdata ? 0 : -ENODEV;
}

static u32 pdata_get_val(void *pdata, const struct mt6379_charger_dtprop *dp)
{
	if (dp->type == DTPROP_BOOL)
		return *((bool *)(pdata + dp->offset));
	return *((u32 *)(pdata + dp->offset));
}

static int mt6379_charger_apply_pdata(struct mt6379_charger_data *cdata)
{
	int i = 0, ret = 0;
	u32 val = 0;
	const struct mt6379_charger_dtprop *dp;

	for (i = 0; i < ARRAY_SIZE(mt6379_charger_dtprops); i++) {
		dp = &mt6379_charger_dtprops[i];
		if (dp->field >= F_MAX)
			continue;
		val = pdata_get_val(dev_get_platdata(cdata->dev), dp);
		dev_info(cdata->dev, "dp-name = %s, val = %d\n", dp->name, val);
		ret = mt6379_charger_field_set(cdata, dp->field, val);
		if (ret == -EOPNOTSUPP)
			continue;
		else if (ret < 0) {
			dev_err(cdata->dev, "Failed to apply pdata %s\n", dp->name);
			return ret;
		}
	}
	return 0;
}

static int mt6379_charger_init_setting(struct mt6379_charger_data *cdata)
{
	struct mt6379_charger_platform_data *pdata = dev_get_platdata(cdata->dev);
	int ret = 0;

	/* enable pre-UV function */
	ret |= mt6379_enable_hm(cdata, true);
	ret |= regmap_update_bits(cdata->rmap, MT6379_REG_CHG_HD_DIG2, BIT(1), BIT(1));
	ret |= mt6379_enable_hm(cdata, false);
	if (ret) {
		dev_err(cdata->dev, "Failed to set pre-UV function\n");
		return ret;
	}

	ret = mt6379_charger_field_set(cdata, F_AICC_ONESHOT, 1);
	if (ret) {
		dev_err(cdata->dev, "Failed to set aicc oneshot\n");
		return ret;
	}

	/* Disable BC12 */
	ret = mt6379_charger_field_set(cdata, F_BC12_EN, 0);
	if (ret) {
		dev_err(cdata->dev, "Failed to disable bc12\n");
		return ret;
	}

	/* OTG LBP set 2.8 V */
	ret = mt6379_charger_field_set(cdata, F_OTG_LBP, 2800000);
	if (ret) {
		dev_err(cdata->dev, "Failed to set otb lbp 2.8V\n");
		return ret;
	}

	/* set aicr = 200mA in 1:META_BOOT 5:ADVMETA_BOOT */
	if (pdata->boot_mode == 1 || pdata->boot_mode == 5) {
		ret = mt6379_charger_field_set(cdata, F_IBUS_AICR, 200000);
		if (ret < 0) {
			dev_err(cdata->dev, "Failed to set aicr 200mA\n");
			return ret;
		}
	}

	ret = mt6379_charger_apply_pdata(cdata);
	if (ret) {
		dev_err(cdata->dev, "Failed to apply charger pdata\n");
		return ret;
	}
	ret = mt6379_charger_field_get(cdata, F_CV, &cdata->cv);
	if (ret < 0) {
		dev_err(cdata->dev, "Failed to get CV after apply pdata\n");
		cdata->cv = 0;
	}

	/* Disable input current limit */
	ret = mt6379_charger_field_set(cdata, F_ILIM_EN, 0);
	if (ret) {
		dev_err(cdata->dev, "Failed to disable input current limit\n");
		return ret;
	}

	/*
	 * disable WDT to save 1mA power consumption
	 * it will be turned back on later
	 * if it is enabled int dt property ant TA attached
	 */
	ret = mt6379_charger_field_set(cdata, F_WDT_EN, 0);
	if (ret < 0)
		dev_err(cdata->dev, "Failed to disable WDT\n");
	return ret;
}

static void mt6379_charger_check_pwr_rdy(struct mt6379_charger_data *cdata)
{
	union power_supply_propval val;
	u32 value = 0;
	int ret = 0;

	ret = mt6379_charger_field_get(cdata, F_ST_PWR_RDY, &value);
	if (ret < 0)
		return;
	val.intval = value ? ATTACH_TYPE_PWR_RDY : ATTACH_TYPE_NONE;
	if (val.intval == ATTACH_TYPE_NONE && !cdata->fsw_control) {
		ret = mt6379_enable_hm(cdata, true);
		ret = regmap_update_bits(cdata->rmap, MT6379_REG_CHG_HD_BOBU5,
					 MT6379_CHG_RAMPUP_COMP_MSK,
					 3 << MT6379_CHG_RAMPUP_COMP_SFT);
		ret = regmap_update_bits(cdata->rmap, MT6379_REG_CHG_HD_TRIM6,
					 MT6379_CHG_IEOC_FLOW_RB_MSK, 0);
		ret = mt6379_enable_hm(cdata, false);
		cdata->fsw_control = true;
	}
	mt6379_charger_set_online(cdata, ATTACH_TRIG_PWR_RDY, val.intval);
}

static irqreturn_t mt6379_fl_pwr_rdy_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	mt6379_charger_check_pwr_rdy(cdata);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_detach_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	mt6379_charger_check_pwr_rdy(cdata);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_rechg_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_chg_done_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_bk_chg_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_ieoc_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_bus_chg_rdy_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_wlin_chg_rdy_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_vbus_ov_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	charger_dev_notify(cdata->chgdev, CHARGER_DEV_NOTIFY_VBUS_OVP);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_chg_batov_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_chg_sysov_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_chg_tout_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	charger_dev_notify(cdata->chgdev, CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_chg_busuv_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_chg_threg_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_chg_aicr_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_chg_mivr_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_aicc_done_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_pe_done_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_pp_pgb_evt_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_wdt_handler(int irq, void *data)
{
	int ret = 0;
	struct mt6379_charger_data *cdata = data;

	ret = mt6379_charger_field_set(cdata, F_WDT_RST, 1);
	if (ret)
		dev_notice(cdata->dev, "Failed to kick wdt\n");
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_otg_fault_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_otg_lbp_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_otg_cc_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_batpro_done_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;
	int ret = 0;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	ret = charger_dev_enable_6pin_battery_charging(cdata->chgdev, false);
	if (ret)
		dev_err(cdata->dev, "%s enable 6pin failed\n", __func__);
	charger_dev_notify(cdata->chgdev, CHARGER_DEV_NOTIFY_BATPRO_DONE);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_otg_clear_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_dcd_done_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_bc12_hvdcp_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;

	dev_info(cdata->dev, "%s, irq = %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_fl_bc12_dn_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;
	int attach = 0;
	bool toggle_by_ufcs = false;

	dev_info(cdata->dev, "%s ++\n", __func__);
	mutex_lock(&cdata->attach_lock);
	attach = atomic_read(&cdata->attach[0]);
	if (attach == ATTACH_TYPE_NONE) {
		cdata->bc12_dn[0] = false;
		mutex_unlock(&cdata->attach_lock);
		dev_notice(cdata->dev, "%s attach=%d\n", __func__, attach);
		return IRQ_HANDLED;
	}

	/* If UFCS detect fail, BC12 will be retoggled to support HVDCP */
	if (attach < ATTACH_TYPE_PD &&
	    cdata->psy_desc.type == POWER_SUPPLY_TYPE_USB_DCP)
		toggle_by_ufcs = true;

	cdata->bc12_dn[0] = true;
	mutex_unlock(&cdata->attach_lock);

	if (!toggle_by_ufcs && attach < ATTACH_TYPE_PD) {
		if (!queue_work(cdata->wq, &cdata->bc12_work))
			dev_notice(cdata->dev, "%s bc12 work already queued\n",
				   __func__);
	}
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_adc_vbat_mon_ov_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;
	int ret = 0;
	union power_supply_propval val;

	ret = power_supply_get_property(cdata->psy,
		POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &val);
	if (ret < 0)
		return IRQ_HANDLED;
	dev_info(cdata->dev, "cv = %dmV\n", U_TO_M(val.intval));
	return IRQ_HANDLED;
}

static irqreturn_t mt6379_usbid_evt_handler(int irq, void *data)
{
	struct mt6379_charger_data *cdata = data;
	int ret = 0;
	u32 val = 0;

	ret = mt6379_charger_field_get(cdata,  F_ST_USBID, &val);
	if (ret)
		dev_err(cdata->dev, "get F_ST_USBID failed\n");
	dev_info(cdata->dev, "%s, usbid_stat = %d\n", __func__, val);
	return IRQ_HANDLED;
}

#define DDATA_DEVM_KCALLOC(member)					\
	(cdata->member = devm_kcalloc(cdata->dev, pdata->nr_port,	\
				      sizeof(*cdata->member), GFP_KERNEL))\

static int mt6379_chg_init_multi_ports(struct mt6379_charger_data *cdata)
{
	struct mt6379_charger_platform_data *pdata = dev_get_platdata(cdata->dev);
	int i = 0;

	if (pdata->nr_port < 1)
		return -EINVAL;

	if (pdata->nr_port > 1 && pdata->attach_trig != ATTACH_TRIG_TYPEC)
		return -EPERM;

	DDATA_DEVM_KCALLOC(psy_type);
	DDATA_DEVM_KCALLOC(psy_usb_type);
	DDATA_DEVM_KCALLOC(attach);
	DDATA_DEVM_KCALLOC(bc12_dn);
	if (!cdata->psy_type || !cdata->psy_usb_type || !cdata->attach || !cdata->bc12_dn)
		return -ENOMEM;

	cdata->active_idx = 0;
	for (i = 0; i < pdata->nr_port; i++) {
		cdata->psy_type[i] = POWER_SUPPLY_TYPE_USB;
		cdata->psy_usb_type[i] = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		atomic_set(&cdata->attach[i], ATTACH_TYPE_NONE);
		cdata->bc12_dn[i] = false;
	}

	return 0;
}

#define MT6379_CHARGER_IRQ(_name)					\
{									\
	.name = #_name,							\
	.handler = mt6379_##_name##_handler,				\
}

static int mt6379_charger_init_irq(struct mt6379_charger_data *cdata)
{
	int i = 0, ret = 0;
	const struct {
		char *name;
		irq_handler_t handler;
	} mt6379_charger_irqs[] = {
		MT6379_CHARGER_IRQ(fl_pwr_rdy),
		MT6379_CHARGER_IRQ(fl_detach),
		MT6379_CHARGER_IRQ(fl_rechg),
		MT6379_CHARGER_IRQ(fl_chg_done),
		MT6379_CHARGER_IRQ(fl_bk_chg),
		MT6379_CHARGER_IRQ(fl_ieoc),
		MT6379_CHARGER_IRQ(fl_bus_chg_rdy),
		MT6379_CHARGER_IRQ(fl_wlin_chg_rdy),
		MT6379_CHARGER_IRQ(fl_vbus_ov),
		MT6379_CHARGER_IRQ(fl_chg_batov),
		MT6379_CHARGER_IRQ(fl_chg_sysov),
		MT6379_CHARGER_IRQ(fl_chg_tout),
		MT6379_CHARGER_IRQ(fl_chg_busuv),
		MT6379_CHARGER_IRQ(fl_chg_threg),
		MT6379_CHARGER_IRQ(fl_chg_aicr),
		MT6379_CHARGER_IRQ(fl_chg_mivr),
		MT6379_CHARGER_IRQ(fl_aicc_done),
		MT6379_CHARGER_IRQ(fl_pe_done),
		MT6379_CHARGER_IRQ(pp_pgb_evt),
		MT6379_CHARGER_IRQ(fl_wdt),
		MT6379_CHARGER_IRQ(fl_otg_fault),
		MT6379_CHARGER_IRQ(fl_otg_lbp),
		MT6379_CHARGER_IRQ(fl_otg_cc),
		MT6379_CHARGER_IRQ(fl_batpro_done),
		MT6379_CHARGER_IRQ(fl_otg_clear),
		MT6379_CHARGER_IRQ(fl_dcd_done),
		MT6379_CHARGER_IRQ(fl_bc12_hvdcp),
		MT6379_CHARGER_IRQ(fl_bc12_dn),
		MT6379_CHARGER_IRQ(adc_vbat_mon_ov),
		MT6379_CHARGER_IRQ(usbid_evt),
	};

	if (ARRAY_SIZE(mt6379_charger_irqs) > MT6379_IRQ_MAX) {
		dev_err(cdata->dev, "irq number out of MT6379_IRQ_MAX\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(mt6379_charger_irqs); i++) {
		ret = platform_get_irq_byname(to_platform_device(cdata->dev),
					      mt6379_charger_irqs[i].name);
		if (ret < 0) /* not declare in dts file */
			continue;
		dev_info(cdata->dev, "request %s, ret = %d\n", mt6379_charger_irqs[i].name, ret);
		cdata->irq_nums[i] = ret;
		ret = devm_request_threaded_irq(cdata->dev, ret, NULL,
						mt6379_charger_irqs[i].handler,
						IRQF_TRIGGER_FALLING,
						mt6379_charger_irqs[i].name,
						cdata);
		if (ret)
			return dev_err_probe(cdata->dev, ret,
					     "Failed to request irq %s\n",
					     mt6379_charger_irqs[i].name);
	}
	return 0;
}

static void mt6379_charger_destroy_wq(void *data)
{
	struct workqueue_struct *wq = data;

	flush_workqueue(wq);
	destroy_workqueue(wq);
}

static void mt6379_charger_destroy_attach_lock(void *data)
{
	struct mutex *attach_lock = data;

	mutex_destroy(attach_lock);
}

static void mt6379_charger_destroy_cv_lock(void *data)
{
	struct mutex *cv_lock = data;

	mutex_destroy(cv_lock);
}

static void mt6379_charger_destroy_hm_lock(void *data)
{
	struct mutex *hm_lock = data;

	mutex_destroy(hm_lock);
}

static void mt6379_charger_destroy_pe_lock(void *data)
{
	struct mutex *pe_lock = data;

	mutex_destroy(pe_lock);
}

static int mt6379_charger_init_mutex(struct mt6379_charger_data *cdata)
{
	int ret = 0;
	struct device *dev = cdata->dev;

	/* init mutex */
	mutex_init(&cdata->attach_lock);
	ret = devm_add_action_or_reset(cdata->dev,
				       mt6379_charger_destroy_attach_lock,
				       &cdata->attach_lock);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init attach lock\n");

	mutex_init(&cdata->cv_lock);
	ret = devm_add_action_or_reset(cdata->dev,
				       mt6379_charger_destroy_cv_lock,
				       &cdata->cv_lock);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init cv lock\n");

	mutex_init(&cdata->hm_lock);
	ret = devm_add_action_or_reset(cdata->dev,
				       mt6379_charger_destroy_hm_lock,
				       &cdata->hm_lock);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init hm lock\n");

	mutex_init(&cdata->pe_lock);
	ret = devm_add_action_or_reset(cdata->dev,
				       mt6379_charger_destroy_pe_lock,
				       &cdata->pe_lock);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init pe lock\n");

	/* init atomic */
	atomic_set(&cdata->eoc_cnt, 0);
	atomic_set(&cdata->no_6pin_used, 0);
	atomic_set(&cdata->tchg, 0);

	return 0;
}

static int ufcs_port_notifier_call(struct notifier_block *nb,
				   unsigned long action, void *data)
{
	struct mt6379_charger_data *cdata = container_of(nb, struct mt6379_charger_data,
							 ufcs_noti);
	int attach;

	if (!cdata->wait_for_ufcs_attach)
		return NOTIFY_DONE;

	cdata->wait_for_ufcs_attach = false;

	mutex_lock(&cdata->attach_lock);

	attach = atomic_read(&cdata->attach[0]);

	if (action == UFCS_NOTIFY_ATTACH_FAIL &&
	    (attach == ATTACH_TYPE_TYPEC || attach == ATTACH_TYPE_PWR_RDY))
		mt6379_charger_toggle_bc12(cdata);

	mutex_unlock(&cdata->attach_lock);

	return NOTIFY_DONE;
}

static void mt6379_release_ufcs_port(void *d)
{
	struct mt6379_charger_data *cdata = d;

	unregister_ufcs_dev_notifier(cdata->ufcs, &cdata->ufcs_noti);
	ufcs_port_put(cdata->ufcs);
}

static int mt6379_charger_get_iio_adc(struct mt6379_charger_data *cdata)
{
	int ret = 0;

	cdata->iio_adcs = devm_iio_channel_get_all(cdata->dev);
	if (IS_ERR(cdata->iio_adcs))
		return PTR_ERR(cdata->iio_adcs);

	ret = iio_read_channel_processed(&cdata->iio_adcs[ADC_CHAN_ZCV], &cdata->zcv);
	if (ret)
		dev_err(cdata->dev, "%s, Failed to read ZCV voltage\n", __func__);

	dev_info(cdata->dev, "%s, zcv = %d mV\n", __func__, cdata->zcv);
	return 0;
}

static int mt6379_charger_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mt6379_charger_data *cdata;
	int ret = 0;

	cdata = devm_kzalloc(dev, sizeof(*cdata), GFP_KERNEL);
	if (!cdata)
		return -ENOMEM;

	cdata->rmap = dev_get_regmap(dev->parent, NULL);
	if (!cdata->rmap)
		return dev_err_probe(dev, -ENODEV, "Failed to get regmap\n");

	cdata->dev = dev;

	ret = mt6379_charger_init_rmap_fields(cdata);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init regmap field\n");

	ret = mt6379_charger_get_pdata(dev);
	if (ret < 0)
		dev_err_probe(dev, ret, "Failed to get platform data\n");


	ret = mt6379_charger_init_mutex(cdata);
	if (ret < 0)
		return ret;

	cdata->wq = create_singlethread_workqueue(dev_name(cdata->dev));
	if (!cdata->wq)
		return dev_err_probe(dev, -ENOMEM, "Failed to create WQ\n");

	ret = devm_add_action_or_reset(dev, mt6379_charger_destroy_wq, cdata->wq);
	if (ret)
		dev_err_probe(dev, ret, "Failed to init WQ\n");

	ret = devm_work_autocancel(dev, &cdata->bc12_work,
				   mt6379_charger_bc12_work_func);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init bc12 work\n");

	platform_set_drvdata(pdev, cdata);

	ret = mt6379_charger_init_setting(cdata);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init mt6379 charger\n");

	ret = mt6379_charger_get_iio_adc(cdata);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get iio adc\n");

	ret = mt6379_chg_init_multi_ports(cdata);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init multi ports\n");

	ret = mt6379_charger_init_psy(cdata);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init psy\n");

	ret = mt6379_init_otg_regulator(cdata);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init OTG regulator\n");

	ret = mt6379_charger_init_chgdev(cdata);
	if (ret < 0)
		return dev_err_probe(dev, ret, "Failed to init chgdev\n");

	ret = mt6379_charger_init_irq(cdata);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init irq\n");

	cdata->ufcs = ufcs_port_get_by_name("port.0");
	if (!cdata->ufcs)
		return dev_err_probe(dev, -ENODEV, "Failed to get ufcs port\n");

	cdata->ufcs_noti.notifier_call = ufcs_port_notifier_call;
	register_ufcs_dev_notifier(cdata->ufcs, &cdata->ufcs_noti);

	ret = devm_add_action_or_reset(dev, mt6379_release_ufcs_port, cdata);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to add ufcs action\n");

	mt6379_charger_check_pwr_rdy(cdata);
	return 0;
}

static int mt6379_charger_remove(struct platform_device *pdev)
{
	struct mt6379_charger_data *cdata = platform_get_drvdata(pdev);

	charger_device_unregister(cdata->chgdev);
	return 0;
}

static const struct of_device_id mt6379_charger_of_match[] = {
	{ .compatible = "mediatek,mt6379-charger", },
	{ },
};
MODULE_DEVICE_TABLE(of, mt6379_charger_of_match);

static struct platform_driver mt6379_charger_driver = {
	.probe = mt6379_charger_probe,
	.remove = mt6379_charger_remove,
	.driver = {
		.name = "mt6379-charger",
		.of_match_table = mt6379_charger_of_match,
	},
};
module_platform_driver(mt6379_charger_driver);

MODULE_AUTHOR("SHIH CHIA CHANG <jeff_chang@richtek.com>");
MODULE_DESCRIPTION("MT6379 Charger Driver");
MODULE_LICENSE("GPL");
