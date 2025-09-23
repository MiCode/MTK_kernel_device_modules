// SPDX-License-Identifier: GPL-2.0
/*
* sc7601 driver
*
* Copyright (C) 2017 Texas Instruments Incorporated - http://www.ti.com/
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed "as is" WITHOUT ANY WARRANTY of any
* kind, whether express or implied; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/regmap.h>
#include <linux/pinctrl/consumer.h>
#include "charger_class.h"
#include "sc7601.h"
#include "sc7601_reg.h"

#define MAX_LENGTH_BYTE 600
#define MAX_REG_COUNT 0x20

/********start i2c basic read/write interface for load_switch**********/
#if 0
 static int ls_read_word(struct i2c_client *client, u8 reg, u16 *val)
{
	s32 ret;

	ret = i2c_smbus_read_word_data(client, reg);
	if (ret < 0) {
		dev_err("i2c read word fail: can't read from reg 0x%02X, errcode=%d\n", reg, ret);
		return ret;
	}

	*val = (u16)ret;
	return 0;
}

static int ls_write_word(struct i2c_client *client, u8 reg, u16 val)
{
	s32 ret;

	ret = i2c_smbus_write_word_data(client, reg, val);
	if (ret < 0) {
		dev_err("i2c write word fail: can't write to reg 0x%02X\n", reg);
		return ret;
	}
	return 0;
}

static int ls_read_block(struct i2c_client *client, u8 reg, u8 *buf, int len)
{
	int ret;
	int i;

	for (i = 0; i < len; i++) {
		ret = i2c_smbus_read_byte_data(client, reg + i);
		if (ret < 0) {
			dev_err("i2c read reg 0x%02X faild\n", reg + i);
			return ret;
		}
		buf[i] = ret;
	}
	return 0;
}

 static int ls_write_block(struct i2c_client *client, u8 reg, u8 *buf, int len)
{
	int ret;
	int i;

	for (i = 0; i < len; i++) {
		ret = i2c_smbus_write_byte_data(client, reg + i, buf[i]);
		if (ret < 0) {
			dev_err("i2c write reg 0x%02X faild\n", reg + i);
			return ret;
		}
	}

	return 0;
}
#endif

static int ls_read_byte(struct i2c_client *client, u8 reg, u8 *val)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		pr_err("i2c read word fail: can't read from reg 0x%02X, errcode=%d\n", reg, ret);
		return ret;
	}

	*val = (u8)ret;
	return 0;

}

static int ls_write_byte(struct i2c_client *client, u8 reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(client, reg, val);
	if (ret < 0) {
		pr_err("i2c write word fail: can't write to reg 0x%02X, errcode=%d\n", reg, ret);
		return ret;
	}

	return 0;
}

static int ls_update_bits(struct i2c_client *client, u8 reg, u8 mask, u8 val)
{
	u8 tmp;

	ls_read_byte(client, reg, &tmp);
	tmp &= ~mask;
	tmp |= val & mask;
	ls_write_byte(client, reg, tmp);
	return 0;
}
/***********************end i2c basic read/write interface**********************/

#if 0

enum ls_attr_list {
	LS_DEBUG_PROP_ADDRESS,
	LS_DEBUG_PROP_COUNT,
	LS_DEBUG_PROP_DATA,
};

static struct reg_context {
			int address;
			int count;
			int data;
} reg_info;

static ssize_t ls_debugfs_show(void *priv_data, char *buf)
{
	struct mca_debugfs_attr_data *attr_data = (struct mca_debugfs_attr_data *)priv_data;
	struct mca_debugfs_attr_info *attr_info = attr_data->attr_info;
	struct sc7601_device *dev_data = (struct sc7601_device *)attr_data->private;
	u8 val = 0;
	ssize_t count = 0;
	int ret = 0;
	char read_buf[MAX_LENGTH_BYTE] = {'\0'};

	if (!dev_data || !attr_info) {
		dev_err("null pointer show\n");
		return count;
	}

	switch (attr_info->debugfs_attr_name) {
	case LS_DEBUG_PROP_ADDRESS:
		count = scnprintf(buf, PAGE_SIZE, "%02x\n", reg_info.address);
		break;
	case LS_DEBUG_PROP_COUNT:
		count = scnprintf(buf, PAGE_SIZE, "%x\n", reg_info.count);
		break;
	case LS_DEBUG_PROP_DATA:
		for (int i = 0; i < reg_info.count; i++) {
			ret = ls_read_byte(dev_data->client, (reg_info.address + i), &val);
			count += scnprintf(read_buf, MAX_LENGTH_BYTE, "%02x: %02x\n",
				reg_info.address + i, val);
			strcat(buf, read_buf);
		}
		break;
	default:
		break;
	}
	return count;
}

static ssize_t ls_debugfs_store(void *priv_data, const char *buf, size_t count)
{
	struct mca_debugfs_attr_data *attr_data = (struct mca_debugfs_attr_data *)priv_data;
	struct mca_debugfs_attr_info *attr_info = attr_data->attr_info;
	struct sc7601_device *dev_data = (struct sc7601_device *)attr_data->private;
	int val = 0;
	int ret = 0;

	if (!dev_data || !attr_info) {
		dev_err("null pointer store\n");
		return count;
	}

	if (kstrtoint(buf, 16, &val))
		return -EINVAL;

	switch (attr_info->debugfs_attr_name) {
	case LS_DEBUG_PROP_ADDRESS:
		reg_info.address = val;
		break;
	case LS_DEBUG_PROP_COUNT:
		if (val > MAX_REG_COUNT)
			reg_info.count = MAX_REG_COUNT;
		else if (reg_info.count < 1)
			reg_info.count = 1;
		else
			reg_info.count = val;
		break;
	case LS_DEBUG_PROP_DATA:
		ret = ls_write_byte(dev_data->client, reg_info.address, val);
		break;
	default:
		break;
	}

	return count;
}

struct mca_debugfs_attr_info ls_debugfs_field_tbl[] = {
	mca_debugfs_attr(ls_debugfs, 0664, LS_DEBUG_PROP_ADDRESS, address),
	mca_debugfs_attr(ls_debugfs, 0664, LS_DEBUG_PROP_COUNT, count),
	mca_debugfs_attr(ls_debugfs, 0600, LS_DEBUG_PROP_DATA, data),
};

#define LS_DEBUGFS_ATTRS_SIZE			ARRAY_SIZE(ls_debugfs_field_tbl)

#endif
/*******end debugfs******************************/

static int sc7601x_set_reg_reset(struct sc7601_device *sc760x)
{
	int ret = 0;
	u8 val;

	val = SC760X_REG_RESET;
	ret = ls_update_bits(sc760x->client, SC7601_REG_04,
		SC760X_REG_RST_MASK, val << SC760X_REG_RESET_SHIFT);

	return ret;
}

static int sc760x_set_ibat_limit(struct sc7601_device *sc760x, int limit)
{
	int val;
	int ret = 0;

	if (limit < SC760X_IBAT_CHG_LIM_BASE) {
		limit = SC760X_IBAT_CHG_LIM_BASE;
	}
	val = (limit - SC760X_IBAT_CHG_LIM_BASE) / SC760X_IBAT_CHG_LIM_LSB;
	ret = ls_write_byte(sc760x->client, SC7601_REG_01, val);

	return ret;
}

 static int sc760x_get_ibat_limit(struct sc7601_device *sc760x, int *limit)
{
	int ret = 0;
	u8 data;

	ret = ls_read_byte(sc760x->client, SC7601_REG_01, &data);
	*limit = ((data * SC760X_IBAT_CHG_LIM_LSB) + SC760X_IBAT_CHG_LIM_BASE);

	return ret;
}

static int sc760x_get_work_mode(struct sc7601_device *sc760x, int *mode)
{
	int ret = 0;
	u8 data;

	ret = ls_read_byte(sc760x->client, SC7601_REG_0F, &data);
	*mode = data;

	return ret;
}

#if 0
 static int sc760x_set_power_limit_en(struct sc7601_device *sc760x, bool enable)
{
	int ret = 0;

	return ls_update_bits(sc760x->client, SC7601_REG_02, SC760X_POW_LIM_DIS_MASK,
			enable ? SC760X_POW_LIM_DIS_ENABLE : SC760X_POW_LIM_DISABLE);
	return ret;
}

static int sc760x_enable_current_limit(struct sc7601_device *sc760x, bool enable)
{
	int ret = 0;
	ret = ls_update_bits(sc760x->client, SC7601_REG_03, SC760X_ILIM_DIS_MASK,
				enable ? SC760X_ILIM_ENABLE : SC760X_ILIM_DISABLE);
	return ret;
}

static int sc760x_enable_volt_diff_check(struct sc7601_device *sc760x, bool enable)
{
	int ret = 0;
	ret = ls_update_bits(sc760x->client, SC7601_REG_03, SC760X_VDIFF_CHECK_DIS_MASK,
				enable ? SC760X_VDIFF_CHECK_DIS_ENABLE : SC760X_VDIFF_CHECK_DIS_DISABLE);
	return ret;
}
#endif

static int sc760x_set_lowpower_mode(struct sc7601_device *sc760x, bool enable)
{
	int ret = 0;
	u8 data, val;

	val = enable ? SC760X_LOWPOWER_MODE : SC760X_NO_ACTION;
	ret = ls_update_bits(sc760x->client, SC7601_REG_04,
		SC760X_EN_LOWPOWER_MASK, val << SC760X_REG_LOWPOWER_SHIFT);
	ret = ls_read_byte(sc760x->client, SC7601_REG_04, &data);

	pr_err("%s read reg: 0x%02x, data 0x%02x\n", sc760x->log_tag, SC7601_REG_04, data);
	return ret;
}

static int sc760x_get_lowpower_mode(struct sc7601_device *sc760x, bool *enable)
{
	int ret = 0;
	u8 data;

	ret = ls_read_byte(sc760x->client, SC7601_REG_04, &data);
	*enable = (data & SC760X_EN_LOWPOWER_MASK) ? true : false;

	return ret;
}

static int sc760x_set_iprechg(struct sc7601_device *sc760x, u16 preChgCurr)
{
	u16 val;
	int ret = 0;

	if (preChgCurr < SC760X_IPRECHG_BASE) {
		preChgCurr = SC760X_IPRECHG_BASE;
	}
	val = (preChgCurr - SC760X_IPRECHG_BASE) / SC760X_IPRECHG_LSB;
	ret = ls_update_bits(sc760x->client, SC7601_REG_06,
		SC760X_IPRECHG_MASK, val << SC760X_IPRECHG_SHIFT);
	return ret;

}

static int sc760x_set_vfcchg(struct sc7601_device *sc760x, u16 vfcChgVolt)
{
	u16 val;
	int ret = 0;

	if (vfcChgVolt < SC760X_VFCCHG_LSB) {
		vfcChgVolt = SC760X_VFCCHG_LSB;
	}
	val = (vfcChgVolt - SC760X_VFCCHG_BASE) / SC760X_VFCCHG_LSB;
	ret = ls_update_bits(sc760x->client, SC7601_REG_06,
		SC760X_VFCCHG_MASK, val << SC760X_VFCCHG_SHIFT);
	return ret;
}

static int sc760x_set_batovp(struct sc7601_device *sc760x, u16 ovpBatVolt)
{
	u16 val;
	int ret = 0;

	if (ovpBatVolt < SC760X_BAT_OVP_BASE) {
		ovpBatVolt = SC760X_BAT_OVP_BASE;
	}
	val = (ovpBatVolt - SC760X_BAT_OVP_BASE) / SC760X_BAT_OVP_LSB;
	ret = ls_update_bits(sc760x->client, SC7601_REG_08,
		SC760X_BAT_OVP_MASK, val << SC760X_BAT_OVP_SHIFT);
	return ret;
}

enum sc760x_reg_idx{
	IBAT_CHG_LIM,
	POWER_LIM,
	CONTROL1,
	CONTROL2,
	TRICKLE_CHG,
	PRE_CHG,
	CHG_OVP,
	BAT_OVP,
	CHG_OCP,
	DSG_OCP,
	TDIE_FLT,
	TDIE_ALM,
	DEGLITCH1,
	DEGLITCH2,
	STAT1,
	STAT2,
	ADC_CTRL1,
};
static unsigned int sc760x_reg_list[] = {
	0x01,// IBAT_CHG_LIM
	0x02,// POWER_LIM
	0x03,// CONTROL1
	0x04,// CONTROL2
	0x05,// TRICKLE_CHG
	0x06,// PRE_CHG
	0x07,// CHG_OVP
	0x08,// BAT_OVP
	0x09,// CHG_OCP
	0x0A,// DSG_OCP
	0x0B,// TDIE_FLT
	0x0C,// TDIE_ALM
	0x0D,// DEGLITCH1
	0x0E,// DEGLITCH2
	0x0F,// STAT1
	0x10,// STAT2
	0x15,// ADC_CTRL1
};
#define SC760X_MAX_REG_NUM			ARRAY_SIZE(sc760x_reg_list)

static int sc760x_dev_dump(struct sc7601_device *sc760x)
{
	u8 reg[SC760X_MAX_REG_NUM] = { 0 };
	int i = 0;
	int len = 0, idx = 0;
	char buf_tmp[256] = {0,};

	for (i = 0; i < SC760X_MAX_REG_NUM; i++) {
		ls_read_byte(sc760x->client, sc760x_reg_list[i], &reg[i]);
		len = scnprintf(buf_tmp + strlen(buf_tmp), PAGE_SIZE - idx,
			"[0x%02X]=0x%02X,", sc760x_reg_list[i], reg[i]);
		idx += len;

		if (((i + 1) % 8 == 0) || ((i + 1) == SC760X_MAX_REG_NUM)) {
			pr_err("%s %s\n", sc760x->log_tag, buf_tmp);
		}
	}

	return 0;
}


static int sc7601_init_device(struct sc7601_device *sc760x)
{
	int ret = 0;

	ret = sc7601x_set_reg_reset(sc760x);
	ret = sc760x_set_ibat_limit(sc760x, sc760x->ibat_limit);
	ret = ls_write_byte(sc760x->client, SC7601_REG_02, 0x0B);
	ret = ls_write_byte(sc760x->client, SC7601_REG_03, 0x90);
	ret = sc760x_set_lowpower_mode(sc760x, false);
	ret = sc760x_set_batovp(sc760x, sc760x->bat_ovp_th);
	ret = sc760x_set_iprechg(sc760x, sc760x->pre_chg_curr);
	ret = sc760x_set_vfcchg(sc760x, sc760x->vfc_chg_volt);

	if (ret != 0) {
		pr_err("fail to init load switch device\n");
	} else {
		pr_err("%s success to init load switch device\n", sc760x->log_tag);
	}
	sc760x_dev_dump(sc760x);
	return ret;
}

static int ops_loadsw_get_present(struct charger_device *chg_dev, bool *present)
{
	struct sc7601_device *sc760x = charger_get_data(chg_dev);
	int ret = 0, times = 0;
	u8 data;

	while (times < SC760X_I2C_MAX_CHECK_TIMES) {
		ret = ls_read_byte(sc760x->client, SC7601_REG_03, &data);
		if (ret != 0) {
			sc760x->chip_ok = false;
			times++;
		} else {
			sc760x->chip_ok = true;
			break;
		}
	}
	pr_err("[SC7601] %s:%d\n", __func__, sc760x->chip_ok);
	*present  = sc760x->chip_ok;
	return ret;
}

static int ops_loadsw_get_ibat_limit(struct charger_device *chg_dev, int *ibat_limit)
{
	struct sc7601_device *sc760x = charger_get_data(chg_dev);
	int ret = 0;

	ret = sc760x_get_ibat_limit(sc760x, ibat_limit);
	pr_err("[SC7601] %s:%d\n", __func__, *ibat_limit);
	return ret;
}

static int ops_loadsw_set_ibat_limit(struct charger_device *chg_dev, int val)
{
	struct sc7601_device *sc760x = charger_get_data(chg_dev);
	int ret = 0;

	ret = sc760x_set_ibat_limit(sc760x, val);

	if (ret)
		pr_err("%s failed set ibat limit\n", sc760x->log_tag);
	pr_err("[SC7601] %s:%d\n", __func__, val);
	return ret;
}

static int ops_loadsw_set_lowpower_mode(struct charger_device *chg_dev, bool mode)
{
	struct sc7601_device *sc760x = charger_get_data(chg_dev);
	int ret = 0;

	ret = sc760x_set_lowpower_mode(sc760x, mode);

	if (ret)
		pr_err("%s failed set lowpower mode\n", sc760x->log_tag);
	pr_err("[SC7601] %s:%d\n", __func__, mode);
	return ret;
}

static int ops_loadsw_get_lowpower_mode(struct charger_device *chg_dev, bool *mode)
{
	struct sc7601_device *sc760x = charger_get_data(chg_dev);
	int ret = 0;

	ret = sc760x_get_lowpower_mode(sc760x, mode);
	pr_err("[SC7601] %s:%d\n", __func__, *mode);
	return ret;
}

static int ops_loadsw_get_work_mode(struct charger_device *chg_dev, int *mode)
{
	struct sc7601_device *sc760x = charger_get_data(chg_dev);
	int ret = 0;

	ret = sc760x_get_work_mode(sc760x, mode);
	pr_err("[SC7601] %s:%d\n", __func__, *mode);
	return ret;
}

static const struct charger_ops sc760x_chg_ops = {
	.loadsw_get_present = ops_loadsw_get_present,
	.loadsw_get_ibat_limit = ops_loadsw_get_ibat_limit,
	.loadsw_set_ibat_limit = ops_loadsw_set_ibat_limit,
	.loadsw_set_lowpower_mode = ops_loadsw_set_lowpower_mode,
	.loadsw_get_lowpower_mode = ops_loadsw_get_lowpower_mode,
	.loadsw_get_work_mode = ops_loadsw_get_work_mode,
};

static int sc7601_register_platform(struct sc7601_device *sc760x)
{
	sc7601x_set_reg_reset(sc760x);
    sc760x->chg_dev = charger_device_register("load_switch",  sc760x->dev, sc760x, &sc760x_chg_ops, &sc760x->chg_props);

	return 0;
}

static int sc7601_parse_dt(struct sc7601_device *sc760x)
{
	struct device_node *np = sc760x->dev->of_node;
	int ret = 0;
	struct pinctrl *load_switch_pinctrl;
	struct pinctrl_state *load_switch_cfg;
	dev_err(sc760x->dev, "enter sc7601_parse_dt\n");
	if (!np) {
		dev_err(sc760x->dev, "device tree info missing\n");
		return -1;
	}

	load_switch_pinctrl = devm_pinctrl_get(sc760x->dev);
	if (! IS_ERR_OR_NULL(load_switch_pinctrl)) {
		load_switch_cfg = pinctrl_lookup_state(load_switch_pinctrl, "charger_load_switch");
		if (! IS_ERR_OR_NULL(load_switch_cfg)) {
			pinctrl_select_state(load_switch_pinctrl, load_switch_cfg);
			dev_err(sc760x->dev, "success to config load_switch_cfg\n");
		} else {
			dev_err(sc760x->dev, "failed to parse load_switch_cfg\n");
		}
	} else {
		dev_err(sc760x->dev, "sc601 failed to get pinctrl\n");
	}
	of_property_read_u32(np, "ic_role", &sc760x->loadsw_role);

	if (sc760x->loadsw_role == SC760X_SLAVE)
		strscpy(sc760x->log_tag, "[1]", sizeof("[1]"));
	else
		strscpy(sc760x->log_tag, "[0]", sizeof("[0]"));

	of_property_read_u32(np, "bat-ovp-threshold", &sc760x->bat_ovp_th);
	of_property_read_u32(np, "pre-chg-curr", &sc760x->pre_chg_curr);
	of_property_read_u32(np, "vfc-chg-volt", &sc760x->vfc_chg_volt);
	of_property_read_u32(np, "limit-chg-curr", &sc760x->ibat_limit);
	dev_err(sc760x->dev, "%s bat-ovp:%d, prechg:%d,vfcchg%d]\n", sc760x->log_tag,
		sc760x->bat_ovp_th, sc760x->pre_chg_curr, sc760x->vfc_chg_volt);

	return ret;
}

static int sc7601_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct sc7601_device *sc760x;
	int ret = 0;
	
	dev_err(&client->dev, "[SC7601]  %s enter\n", __func__);
	sc760x = devm_kzalloc(dev, sizeof(struct sc7601_device), GFP_KERNEL);
	if (!sc760x){
		dev_err(&client->dev, "[SC7601]  failed to allocate memory\n");
		return -ENOMEM;
	}	

	sc760x->client = client;
	sc760x->dev = dev;
	i2c_set_clientdata(client, sc760x);
	dev_err(sc760x->dev, "%s [SC7601] probe start\n", __func__);
	ret = sc7601_parse_dt(sc760x);
	if (ret) {
		dev_err(sc760x->dev, "[SC7601] failed to parse DTS\n");
		return ret;
	}

	ret = sc7601_register_platform(sc760x);
	dev_err(sc760x->dev, "%s [SC7601] sc7601_register_platform\n", __func__);
	ret = sc7601_init_device(sc760x);
	if (ret) {
		dev_err(sc760x->dev, "[SC7601] failed to init sc7601\n");
		return ret;
	}

	sc760x->chip_ok = true;
// #ifdef CONFIG_DEBUG_FS
// 	reg_info.address = SC7601_REG_00;
// 	reg_info.count = 1;

// 	mca_debugfs_create_group("sc760x", ls_debugfs_field_tbl, LS_DEBUGFS_ATTRS_SIZE, sc760x);
// #endif
	dev_err(sc760x->dev, "%s [SC7601] probe success %d\n", sc760x->log_tag, ret);
	return 0;
}

static int sc_loadswitch_suspend(struct device *dev)
{
	pr_err("suspend\n");

	return 0;
}

static int sc_loadswitch_resume(struct device *dev)
{
	pr_err("resume\n");

	return 0;
}

static void sc_loadswitch_remove(struct i2c_client *client)
{
	struct sc7601_device *sc760x = i2c_get_clientdata(client);;
	sc760x_set_lowpower_mode(sc760x, true);
	pr_err("sc load switch driver remove!\n");
}

static void sc_loadswitch_shutdown(struct i2c_client *client)
{
	struct sc7601_device *sc760x = i2c_get_clientdata(client);;
	sc760x_set_lowpower_mode(sc760x, true);
	pr_err("sc load switch driver shutdown!\n");
}


static const struct dev_pm_ops sc_loadswitch_pm_ops = {
	.resume		= sc_loadswitch_resume,
	.suspend	= sc_loadswitch_suspend,
};

static const struct of_device_id sc7601_of_match[] = {
	{.compatible = "sc7601"},
	{},
};

static struct i2c_driver sc_loadswitch_driver = {
	.driver	= {
		.name   = "sc7601",
		.owner  = THIS_MODULE,
		.of_match_table = sc7601_of_match,
		.pm	 = &sc_loadswitch_pm_ops,
	},

	.probe		= sc7601_probe,
	.remove		= sc_loadswitch_remove,
	.shutdown	= sc_loadswitch_shutdown,
};

module_i2c_driver(sc_loadswitch_driver);

MODULE_DESCRIPTION("SC sc7601 Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("xiezhichang <xiezhichang@xiaomi.com>");