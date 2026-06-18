// SPDX-License-Identifier: GPL-2.0
/*
 * sy6541.c
 *
 * charge-pump ic driver
 *
 * Copyright (c) 2023-2023 Xiaomi Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */
#define pr_fmt(fmt)	"[sy6541] %s: " fmt, __func__

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
#include <linux/mca/common/mca_log.h>
#include <linux/mca/common/mca_event.h>
#include <linux/mca/common/mca_charge_mievent.h>

#include "mtk_charger.h"
#include "charger_class.h"
#include "sy6541.h"
#include "sy6541_reg.h"

#ifndef MCA_LOG_TAG
#define MCA_LOG_TAG "cp_sy6541"
#endif
#define MAX_LENGTH_BYTE 600
#define MAX_REG_COUNT 0x42
#define MCA_SET_OVPGATE_COUNT 10
static int cp_get_adc_data(struct sy6541_device *bq, int channel,  u32 *result);

/********i2c basic read/write interface***********/
static int cp_read_word(struct i2c_client *client, u8 reg, u16 *val)
{
	s32 ret;
	ret = i2c_smbus_read_word_data(client, reg);

	if (ret < 0) {
		mca_log_err("i2c read word fail: can't read from reg 0x%02X, errcode=%d\n", reg, ret);
		return ret;
	}
	*val = (u16)ret;

	return 0;
}

#ifdef DEBUG_CODE
static int cp_write_word(struct i2c_client *client, u8 reg, u16 val)
{
	s32 ret;
	ret = i2c_smbus_write_word_data(client, reg, val);

	if (ret < 0) {
		mca_log_err("i2c write word fail: can't write to reg 0x%02X\n", reg);
		return ret;
	}

	return 0;
}
#endif

static int cp_read_byte(struct i2c_client *client, u8 reg, u8 *val)
{
	s32 ret;
	ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0) {
		mca_log_err("i2c read byte fail: can't read from reg 0x%02X, errcode=%d\n", reg, ret);
		return ret;
	}
	*val = (u8)ret;

	return 0;
}

static int cp_write_byte(struct i2c_client *client, u8 reg, u8 val)
{
	s32 ret;
	ret = i2c_smbus_write_byte_data(client, reg, val);

	if (ret < 0) {
		mca_log_err("i2c write byte fail: can't write to reg 0x%02X, errcode=%d\n", reg, ret);
		return ret;
	}

	return 0;
}

#ifdef DEBUG_CODE
static int cp_read_block(struct i2c_client *client, u8 reg, u8 *buf, int len)
{
	int ret;
	int i;

	for (i = 0; i < len; i++) {
		ret = i2c_smbus_read_byte_data(client, reg + i);
		if (ret < 0) {
			mca_log_err("i2c read reg 0x%02X faild\n", reg + i);
			return ret;
		}
		buf[i] = ret;
	}

	return 0;
}

static int cp_write_block(struct i2c_client *client, u8 reg, u8 *buf, int len)
{
	int ret;
	int i;

	for (i = 0; i < len; i++) {
		ret = i2c_smbus_write_byte_data(client, reg + i, buf[i]);
		if (ret < 0) {
			mca_log_err("i2c write reg 0x%02X faild\n", reg + i);
			return ret;
		}
	}

	return 0;
}
#endif

static int cp_read_i2c_block_data(struct i2c_client *client, u8 reg, unsigned short len, u8 *buf)
{
	int ret;

	if (!client || !buf) {
		mca_log_err("null pointer read\n");
		return -EINVAL;
	}

	ret = i2c_smbus_read_i2c_block_data(client, reg, len, buf);
	if (ret < 0) {
		mca_log_err("I2C SMBus read failed, ret=%d\n", ret);
		return ret;
	}

	return ret;
}

static int cp_update_bits(struct i2c_client *client, u8 reg, u8 mask, u8 val)
{
	u8 tmp;

	cp_read_byte(client, reg, &tmp);
	tmp &= ~mask;
	tmp |= val & mask;
	cp_write_byte(client, reg, tmp);

	return 0;
}

static int sy6541_init_protection(struct sy6541_device *cp, int forward_work_mode);
static int sy6541_init_device(struct sy6541_device *cp);
/* SY6541_ADC */
#ifdef DEBUG_CODE
static int sy6541_check_adc_enabled(struct sy6541_device *bq, bool *enabled)
{
	int ret = 0;
	unsigned int val;
	ret = cp_read_byte(bq->client, SY6541_REG_15, &val);
	if (!ret)
		*enabled = !!(val & SY6541_ADC_EN_MASK);
	mca_log_err("%s enable adc %x, is %d\n", bq->log_tag, val, *enabled);
	return ret;
}
#endif

static int sy6541_enable_adc(struct sy6541_device *bq, bool enable)
{
	int ret = 0;
	u8 val;

	val = enable ? SY6541_ADC_ENABLE : SY6541_ADC_DISABLE;
	val <<= SY6541_ADC_EN_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_15,
		SY6541_ADC_EN_MASK, val);

	return ret;
}

/* SY6541_CHG_EN */
static int sy6541_enable_charge(struct sy6541_device *bq, bool enable)
{
	int ret = 0;
	u8 val;

	ret = cp_read_byte(bq->client, SY6541_REG_F6, &val);
	if (ret < 0)
		return ret;
	if (val != 0x20) {
		mca_log_err( "sy6541_enable_charge: val[%02x] is not 0x20\n",
			bq->log_tag, val);
		return sy6541_init_device(bq);
	}

	val = enable ? SY6541_CHG_ENABLE : SY6541_CHG_DISABLE;
	val <<= SY6541_CHG_EN_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_0B,
				SY6541_CHG_EN_MASK, val);
	mca_log_err("%s enable cp: %d\n", bq->log_tag, enable);

	return ret;
}

static int sy6541_check_charge_enabled(struct sy6541_device *bq, bool *enabled)
{
	int ret = 0;
	u8 val;

	ret = cp_read_byte(bq->client, SY6541_REG_0A, &val);
	if (!ret)
		*enabled = !!(val & SY6541_CP_SWITCHING_STAT_MASK);

	return ret;
}

/* SY6541_BAT_OVP */
static int sy6541_enable_batovp(struct sy6541_device *bq, bool enable)
{
	int ret = 0;
	u8 val;

	val = enable ? SY6541_BAT_OVP_ENABLE : SY6541_BAT_OVP_DISABLE;
	val <<= SY6541_BAT_OVP_DIS_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_01,
			SY6541_BAT_OVP_DIS_MASK, val);

	return ret;
}

static int sy6541_set_batovp_th(struct sy6541_device *bq, int threshold)
{
	int ret = 0;
	u8 val;

	if (threshold < SY6541_BAT_OVP_BASE)
		threshold = SY6541_BAT_OVP_BASE;
	val = (threshold - SY6541_BAT_OVP_BASE) / SY6541_BAT_OVP_LSB;
	val <<= SY6541_BAT_OVP_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_01,
				SY6541_BAT_OVP_MASK, val);

	return ret;
}

/* SY6541_USB_OVP */
static int sy6541_set_usbovp_th(struct sy6541_device *bq, int threshold)
{
	int ret = 0;
	u8 val;

    if (threshold == 7500) {
        val = SY6541_USB_OVP_7PV5;
	} else {
        val = (threshold - SY6541_USB_OVP_BASE) / SY6541_USB_OVP_LSB;
	}
	val <<= SY6541_USB_OVP_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_03,
				SY6541_USB_OVP_MASK, val);

	return ret;
}

/* SY6541_OVPGATE_ON_DG_SET */
static int sy6541_set_ovpgate_on_dg_set(struct sy6541_device *bq, int data)
{
	int ret = 0;
	u8 val;

	val = data ? SY6541_OVPGATE_ON_DG_10MS : SY6541_OVPGATE_ON_DG_40MS;
	val <<= SY6541_OVPGATE_ON_DG_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_03,
				SY6541_OVPGATE_ON_DG_MASK, val);

	return ret;
}

/* SY6541_WPC_OVP */
static int sy6541_set_wpcovp_th(struct sy6541_device *bq, int threshold)
{
	int ret = 0;
	u8 val;

    if (threshold == 7500) {
        val = SY6541_WPC_OVP_7PV5;
	} else {
        val = (threshold - SY6541_WPC_OVP_BASE) / SY6541_WPC_OVP_LSB;
	}
	val <<= SY6541_WPC_OVP_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_04,
				SY6541_WPC_OVP_MASK, val);

	return ret;
}
/* SY6541_BUS_OVP */
#define RANGE_MIN_MAX 2
static int sy6541_set_busovp_th(struct sy6541_device *bq, int threshold)
{
	int ret = 0;
	u8 val;
	int ovp_th_range[CP_MODE_DIV_MAX][RANGE_MIN_MAX];
	int base[CP_MODE_DIV_MAX];
	int lsb[CP_MODE_DIV_MAX];
	int ovp_mask;

	ovp_th_range[CP_MODE_DIV4][0] = 15000;
	ovp_th_range[CP_MODE_DIV4][1] = 27000;
	ovp_th_range[CP_MODE_DIV2][0] = 7500;
	ovp_th_range[CP_MODE_DIV2][1] = 13500;
	ovp_th_range[CP_MODE_DIV1][0] = 3750;
	ovp_th_range[CP_MODE_DIV1][1] = 6750;
	mca_log_err("%s threshold= %d, min = %d, max =%d\n",
		bq->log_tag, threshold, ovp_th_range[bq->work_mode][0],
		ovp_th_range[bq->work_mode][1]);

	if (threshold < ovp_th_range[bq->work_mode][0])
		threshold = ovp_th_range[bq->work_mode][0];
	else if (threshold > ovp_th_range[bq->work_mode][1])
		threshold = ovp_th_range[bq->work_mode][1];

	base[CP_MODE_DIV4] = SY6541_BUS_OVP_41MODE_BASE;
	lsb[CP_MODE_DIV4] = SY6541_BUS_OVP_41MODE_LSB;
	base[CP_MODE_DIV2] = SY6541_BUS_OVP_21MODE_BASE;
	lsb[CP_MODE_DIV2] = SY6541_BUS_OVP_21MODE_LSB;
	base[CP_MODE_DIV1] = SY6541_BUS_OVP_11MODE_BASE;
	lsb[CP_MODE_DIV1] = SY6541_BUS_OVP_11MODE_LSB;

	ovp_mask = SY6541_BUS_OVP_MASK;
	val = (threshold - base[bq->work_mode]) / lsb[bq->work_mode];
	mca_log_err("%s bus_ovpth= %d, val = %d\n", bq->log_tag, threshold, val);
	val <<= SY6541_BUS_OVP_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_05,
			ovp_mask, val);

	return ret;
}

/* SY6541_OUT_OVP */
static int sy6541_set_outovp_th(struct sy6541_device *bq, int threshold)
{
	int ret = 0;
	u8 val;

	if (threshold < 4800)
		threshold = 4800;
	val = (threshold - SY6541_OUT_OVP_BASE) / SY6541_OUT_OVP_LSB;
	val <<= SY6541_OUT_OVP_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_05,
		SY6541_OUT_OVP_MASK, val);

	return ret;
}

/* SY6541_BUS_OCP */
static int sy6541_enable_busocp(struct sy6541_device *bq, bool enable)
{
	int ret = 0;
	u8 val;

	val = enable ? SY6541_BUS_OCP_ENABLE : SY6541_BUS_OCP_DISABLE;
	val <<= SY6541_BUS_OCP_DIS_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_06,
		SY6541_BUS_OCP_DIS_MASK, val);

	return ret;
}

static int sy6541_set_busocp_th(struct sy6541_device *bq, int threshold)
{
	int ret = 0;
	u8 val;

	if (threshold < SY6541_BUS_OCP_BASE)
		threshold = SY6541_BUS_OCP_BASE;
	val = (threshold - SY6541_BUS_OCP_BASE) / SY6541_BUS_OCP_LSB;
	val <<= SY6541_BUS_OCP_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_06,
	SY6541_BUS_OCP_MASK, val);

	return ret;
}

/* SY6541_BUS_UCP */
static int sy6541_enable_busucp(struct sy6541_device *bq, bool enable)
{
	int ret = 0;
	u8 val;

	val = enable ? SY6541_BUS_UCP_ENABLE : SY6541_BUS_UCP_DISABLE;
	val <<= SY6541_BUS_UCP_DIS_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_07,
		SY6541_BUS_UCP_DIS_MASK, val);
	val = enable ? SY6541_BUS_UCP_RISE_NOT_MASK : SY6541_BUS_UCP_RISE_IS_MASK;
	val <<= SY6541_BUS_UCP_FALL_MASK_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_07,
		SY6541_BUS_UCP_FALL_MASK_MASK, val);
	val = enable ? SY6541_BUS_UCP_FALL_NOT_MASK : SY6541_BUS_UCP_FALL_IS_MASK;
	val <<= SY6541_BUS_UCP_RISE_MASK_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_07,
		SY6541_BUS_UCP_RISE_MASK_MASK, val);

	return ret;
}

static int sy6541_set_adc_scanrate(struct sy6541_device *bq, bool oneshot)
{
	int ret = 0;
	u8 val;

	val = oneshot ? SY6541_ADC_RATE_ONESHOT : SY6541_ADC_RATE_CONTINOUS;
	val <<= SY6541_ADC_RATE_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_15,
		SY6541_ADC_RATE_MASK, val);
	return ret;
}

#define SY6541_ADC_REG_BASE SY6541_REG_17
static int sy6541_get_adc_data(struct sy6541_device *bq, int channel, u32 *result)
{
	int ret = 0;
	unsigned int val_h;
	u16 val;
	u16 tmp;
	u8 temp_adc;

	if (channel >= ADC_MAX_NUM)
		return -1;
	mca_log_err("adc channel = %d", channel);
	cp_read_byte(bq->client, SY6541_REG_15, &temp_adc);
	mca_log_err("SY6541_REG_15 = %d", temp_adc);
	cp_read_byte(bq->client, SY6541_REG_16, &temp_adc);
	mca_log_err("SY6541_REG_16 = %d", temp_adc);

	ret = cp_read_word(bq->client, SY6541_ADC_REG_BASE + (channel << 1), &tmp);
	if (ret < 0) {
		mca_log_err("adc read error");
		return ret;
	}
	val_h = ((tmp & 0xFF) << 8);
	val = val_h | ((tmp >> 8) & 0xFF);
	switch (channel) {
	case ADC_IBUS:
		val = val * SY6541_IBUS_ADC_LSB;
		break;
	case ADC_VBUS:
		val = val * SY6541_VBUS_ADC_LSB;
		break;
	case ADC_VUSB:
		val = val * SY6541_VUSB_ADC_LSB;
		break;
	case ADC_VWPC:
		val = val * SY6541_VWPC_ADC_LSB;
		break;
	case ADC_VOUT:
		val = val * SY6541_VOUT_ADC_LSB;
		break;
	case ADC_VBAT:
		val = val * SY6541_VBAT_ADC_LSB;
		break;
	case ADC_TDIE:
		val = val * SY6541_TDIE_ADC_LSB - 40;
		break;
	default:
		break;
	}
	*result = val;
	mca_log_debug("%s channel %d adc %x, result: %d\n", bq->log_tag, channel, tmp, *result);

	return ret;
}

static int sy6541_set_adc_scan(struct sy6541_device *bq, int channel, bool enable)
{
	int ret = 0;
	u8 reg;
	u8 mask;
	u8 shift;
	u8 val;

	if (channel > ADC_MAX_NUM)
		return -1;
	if (channel == ADC_IBUS) {
		reg = SY6541_REG_15;
		shift = SY6541_IBUS_ADC_DIS_SHIFT;
		mask = SY6541_IBUS_ADC_DIS_MASK;
	} else {
		reg = SY6541_REG_16;
		shift = 8 - channel;
		mask = 1 << shift;
	}
	val = enable ?  (0 << shift) : (1 << shift);
	ret = cp_update_bits(bq->client, reg, mask, val);

	return ret;
}

/* SY6541_REG_RST */
static int sy6541_set_reg_reset(struct sy6541_device *bq)
{
	int ret = 0;
	u8 val;

	val = SY6541_REG_RESET;
	val <<= SY6541_REG_RST_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_0E,
			SY6541_REG_RST_MASK, val);
	return ret;
}

/* SY6541_MODE */
static int sy6541_set_operation_mode(struct sy6541_device *bq, int operation_mode)
{
	int ret = 0;
	u8 val;

	switch (operation_mode) {
	case SY6541_FORWARD_4_1_CHARGER_MODE:
	case SY6541_REVERSE_1_4_CONVERTER_MODE:
		val = operation_mode;
		bq->work_mode = CP_MODE_DIV4;
		break;
	case SY6541_FORWARD_2_1_CHARGER_MODE:
	case SY6541_REVERSE_1_2_CONVERTER_MODE:
		val = operation_mode;
		bq->work_mode = CP_MODE_DIV2;
		break;
	case SY6541_FORWARD_1_1_CHARGER_MODE:
	case SY6541_REVERSE_1_1_CONVERTER_MODE:
		val = operation_mode;
		bq->work_mode = CP_MODE_DIV1;
		break;
	case SY6541_FORWARD_1_1_CHARGER_MODE1:
		val = SY6541_FORWARD_1_1_CHARGER_MODE;
		bq->work_mode = CP_MODE_DIV1;
		break;
	case SY6541_REVERSE_1_1_CONVERTER_MODE1:
		val = SY6541_REVERSE_1_1_CONVERTER_MODE;
		bq->work_mode = CP_MODE_DIV1;
		break;
	default:
		mca_log_err("%s operation mode error%d\n", bq->log_tag, operation_mode);
		return -1;
	    break;
	}

	bq->operation_mode = val;
	ret = sy6541_init_protection(bq, bq->work_mode);
	mca_log_err("%s set operation mode %d reg %d work_mode %d\n",
		bq->log_tag, operation_mode, val, bq->work_mode);
	val <<= SY6541_MODE_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_0E, SY6541_MODE_MASK, val);

	return ret;
}

static int sy6541_get_operation_mode(struct sy6541_device *bq, int *operation_mode)
{
	int ret = 0;
	u8 val;

	ret = cp_read_byte(bq->client, SY6541_REG_0E, &val);
	if (ret) {
		mca_log_err("%s get operation mode fail\n", bq->log_tag);
		return ret;
	}
	*operation_mode = (val & SY6541_MODE_MASK);

	return ret;
}

/*static int sy6541_get_int_stat(struct sy6541_device *bq, int channel, bool *enable)
{
	int ret = 0;
	u8 val = 0;

	ret = cp_read_byte(bq->client, SY6541_REG_10, &val);
	switch (channel) {
	case VOUT_OK_REV_STAT:
		*enable = !!(val & SY6541_VOUT_OK_REV_STAT_MASK);
		break;
	case VOUT_OK_CHG_STAT:
		*enable = !!(val & SY6541_VOUT_OK_CHG_STAT_MASK);
		break;
	case VOUT_INSERT_STAT:
		*enable = !!(val & SY6541_VOUT_INSERT_STAT_MASK);
		break;
	case VBUS_PRESENT_STAT:
		*enable = !!(val & SY6541_VBUS_PRESENT_STAT_MASK);
		break;
	case VWPC_PRESENT_STAT:
		*enable = !!(val & SY6541_VWPC_INSERT_STAT_MASK);
		break;
	case VUSB_PRESENT_STAT:
		*enable = !!(val & SY6541_VUSB_INSERT_STAT_MASK);
		break;
	default:
		*enable = 0;
		break;
	}
	return ret;
}*/

/* SY6541_ACDRV_MANUAL_EN */
static int sy6541_enable_acdrv_manual(struct sy6541_device *bq, bool enable)
{
	int ret = 0;
	u8 val = 0;

	val = enable ? SY6541_ACDRV_MANUAL_MODE : SY6541_ACDRV_AUTO_MODE;
	val <<= SY6541_ACDRV_MANUAL_EN_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_0B,
			SY6541_ACDRV_MANUAL_EN_MASK, val);
	return ret;
}

/* SY65418581_OVPGATE_EN */
static int sy6541_enable_ovpgate(struct sy6541_device *bq, bool enable)
{
	int ret = 0;
	u8 val;
	int i;
	bool penable = true;
	u8 value;

	bq->ovpgate_en = enable;
	if (!bq->i2c_is_working)
		msleep(30);

	val = enable ? SY6541_OVPGATE_ENABLE : SY6541_OVPGATE_DISABLE;
	val <<= SY6541_OVPGATE_EN_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_0B,
		SY6541_OVPGATE_EN_MASK, val);
	for (i = 0; i < MCA_SET_OVPGATE_COUNT; i++) {
		ret = cp_read_byte(bq->client, SY6541_REG_0B, &value);
		if (!ret) {
			penable = !!(value & SY6541_OVPGATE_EN_MASK);
			if (bq->ovpgate_en == penable)
				break;
		}
		mca_log_err("%s count %d, ovpgate_en %d, penable %d\n",
			bq->log_tag, i, bq->ovpgate_en, penable);
		ret = cp_update_bits(bq->client, SY6541_REG_0B,
			SY6541_OVPGATE_EN_MASK, val);
		msleep(10);
	}
	mca_log_err("%s enable %d\n", bq->log_tag, enable);

	return ret;
}

/* SY6541_IBUS_UCP_TIMEOUT */
static int sy6541_set_ibus_ucp_timeout(struct sy6541_device *bq, u8 val)
{
	val = val > SY6541_IBUS_UCP_TIMEOUT_81920MS ? SY6541_IBUS_UCP_TIMEOUT_DISABLE : val;
	val <<= SY6541_IBUS_UCP_TIMEOUT_SET_SHIFT;
	return cp_update_bits(bq->client, SY6541_REG_0D,
		SY6541_IBUS_UCP_TIMEOUT_SET_MASK, val);
}

/* SY6541_WD_TIMEOUT_SET */
#ifdef DEBUG_CODE
static int sy6541_set_wdt(struct sy6541_device *bq, int ms)
{
	int ret = 0;
	u8 val;

	switch (ms) {
	case 0:
		val = SY6541_WD_TIMEOUT_DISABLE;
		break;
	case 200:
		val = SY6541_WD_TIMEOUT_0P2S;
		break;
	case 500:
		val = SY6541_WD_TIMEOUT_0P5S;
		break;
	case 1000:
		val = SY6541_WD_TIMEOUT_1S;
		break;
	case 5000:
		val = SY6541_WD_TIMEOUT_5S;
		break;
	case 30000:
		val = SY6541_WD_TIMEOUT_30S;
		break;
	default:
		val = SY6541_WD_TIMEOUT_DISABLE;
		break;
	}
	val <<= SY6541_WD_TIMEOUT_SET_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_0D,
			SY6541_WD_TIMEOUT_SET_MASK, val);
	return ret;
}
#endif

static int sy6541_set_ucp_fall_dg(struct sy6541_device *bq, u8 date)
{
	int ret = 0;
	u8 val;

	val = date;
	val <<= SY6541_BUS_UCP_FALL_DG_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_07,
				SY6541_BUS_UCP_FALL_DG_MASK, val);
	return ret;
}

static int sy6541_set_switch_freq(struct sy6541_device *bq, u8 data)
{
	int ret = 0;
	u8 val;

	val = data;
	val <<= SY6541_FSW_SET_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_28,
			SY6541_DEAD_TIME_SET_MASK, 0x03);
	ret |= cp_update_bits(bq->client, SY6541_REG_0C,
			SY6541_FSW_SET_MASK, val);
	ret |= cp_update_bits(bq->client, SY6541_REG_28,
			SY6541_DEAD_TIME_SET_MASK, 0x01);

	return ret;
}

static int sy6541_set_fsw(struct sy6541_device *cp, int fsw)
{
	int ret;
	int fsw_min = cp->fsw_cfg.min;
	int fsw_max = cp->fsw_cfg.max;
	int step_val = cp->fsw_cfg.step;
	int val;

	fsw = fsw > fsw_max ? fsw_max : (fsw < fsw_min ? fsw_min : fsw);
	val = (fsw - fsw_min) / step_val;
	mca_log_err("%s fsw: %d, val: %d\n", cp->log_tag, fsw, val);
	ret = sy6541_set_switch_freq(cp, val);

	return ret;
}

static int sy6541_get_tdie(struct sy6541_device *cp, int *tdie)
{
	int ret = 0;
	int val = 0;
	u8 byte_h = 0;
	u8 byte_l = 0;

	ret = cp_read_byte(cp->client, SY6541_REG_23, &byte_h);
	ret |= cp_read_byte(cp->client, SY6541_REG_24, &byte_l);
	val = (byte_h & SY6541_TDIE_POL_H_MASK << 8) |
			(byte_l & SY6541_TDIE_POL_L_MASK);
	*tdie = val * SY6541_TDIE_ADC_LSB;
	mca_log_err("%s byte_h: %02x, byte_l: %02x, val: %d, tdie: %d\n",
		cp->log_tag, byte_h, byte_l, val, *tdie);

	return ret;
}

static int sy6541_set_vbus_errorhi_rf(struct sy6541_device *bq, int rf)
{
	int ret = 0;
	u8 val;

	switch (rf) {
	case 10:
		val = SY6541_VBUS_ERRORHI_RF_0P1_VOUT;
		break;
	case 15:
		val = SY6541_VBUS_ERRORHI_RF_0P15_VOUT;
		break;
	case 20:
		val = SY6541_VBUS_ERRORHI_RF_0P2_VOUT;
		break;
	case 25:
		val = SY6541_VBUS_ERRORHI_RF_0P25_VOUT;
		break;
	default:
		val = SY6541_VBUS_ERRORHI_RF_0P15_VOUT;
		break;
	}
	val <<= SY6541_VBUS_ERRORHI_RF_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_02, SY6541_VBUS_ERRORHI_RF_MASK, val);
	return ret;
}

static int sy6541_set_vbus_errorlo_rf(struct sy6541_device *bq, int rf)
{
	int ret = 0;
	u8 val;

	switch (rf) {
	case 0:
		val = SY6541_VBUS_ERRORLO_RF_0_VOUT;
		break;
	case 1:
		val = SY6541_VBUS_ERRORLO_RF_0P01_VOUT;
		break;
	case 2:
		val = SY6541_VBUS_ERRORLO_RF_0P02_VOUT;
		break;
	case 3:
		val = SY6541_VBUS_ERRORLO_RF_0P03_VOUT;
		break;
	default:
		val = SY6541_VBUS_ERRORLO_RF_0P01_VOUT;
		break;
	}
	val <<= SY6541_VBUS_ERRORLO_RF_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_02, SY6541_VBUS_ERRORLO_RF_MASK, val);
	return ret;
}

static int sy6541_enable_dt_bit(struct sy6541_device *bq, bool enable)
{
	int ret = 0;
	u8 val;

	val = enable ? SY6541_DT_BIT_ENABLE : SY6541_DT_BIT_DISABLE;
	val <<= SY6541_DT_BIT_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_F3, SY6541_DT_BIT_MASK, val);
	return ret;
}

enum cp_reg_idx {
	VBAT_OVP_REG = 0,
	IBAT_OCP_REG,
	VUSB_OVP_REG,
	VWPC_OVP_REG,
	VOUT_VBUS_OVP_REG,
	IBUS_OCP_REG,
	IBUS_UCP_REG,
	PMID2OUT_OVP_REG,
	PMID2OUT_UVP_REG,
	CONVERTER_STATE_REG,
	CTRL1_REG,
	CTRL2_REG,
	CTRL3_REG,
	CTRL4_REG,
	CTRL5_REG,
	INT_STAT_REG,
	INT_FLAG_REG,
	FLT_FLAG_REG,
	DEVICE_ID_REG,
	FAULT_STATUS_REG,
	CP_REG_MAX,
};

static void sy6541_abnormal_charging_judge(struct sy6541_device *bq, u8 *data)
{
	int val[2] = {0};

	if (!data)
		return;
	if ((data[INT_STAT_REG] & 0x08) == 0) {
		mca_log_err("VOUT UVLO\n");
	}
	if ((data[INT_STAT_REG] & 0x3F) != 0x3F)
		mca_log_err("%s VIN have problem\n", bq->log_tag);
	if (data[CONVERTER_STATE_REG] & 0x08)
		mca_log_err("%s VBUS_ERRORHI_STAT\n", bq->log_tag);
	if (data[CONVERTER_STATE_REG] & 0x10)
		mca_log_err("%sVBUS_ERRORLO_STAT\n", bq->log_tag);
	if (data[CONVERTER_STATE_REG] & 0x01) {
		mca_log_err("%s CBOOT SHORT/OPEN 111\n", bq->log_tag);
	}
	if (data[VBAT_OVP_REG] & 0x20) {
		mca_log_err("%s VBAT_OVP\n", bq->log_tag);
	}
	if (data[VUSB_OVP_REG] & 0x20) {
		mca_log_err("%s VUSB_OVP\n", bq->log_tag);
	}
	if (data[VWPC_OVP_REG] & 0x20) {
		mca_log_err("%s VWPC_OVP\n", bq->log_tag);
	}
	if (data[IBUS_OCP_REG] & 0x20) {
		mca_log_err("%s IBUS_OCP\n", bq->log_tag);
	}
	if (data[IBUS_UCP_REG] & 0x01) {
		mca_log_err("%s IBUS_UCP\n", bq->log_tag);
	}
	if (data[CONVERTER_STATE_REG] & 0x80) {
		mca_log_err("%s POR_FLAG\n", bq->log_tag);
	}
	if (data[FLT_FLAG_REG] & SY6541_TDIE_OTP_FLAG_MASK) {
		mca_log_err("%s TSHUT\n", bq->log_tag);
		cp_get_adc_data(bq, ADC_TDIE, &val[0]);
	}
	if (data[FLT_FLAG_REG] & SY6541_VBUS_OVP_FLAG_MASK) {
		mca_log_err("%s vbus ovp\n", bq->log_tag);
	}
}

#define CP_DUMP_REG_LEN 16
static int sy6541_dump_important_regs(struct sy6541_device *bq)
{
	int ret = 0;
	u8 data[CP_DUMP_REG_LEN] = { 0 };
	u8 reg[CP_REG_MAX] = { 0 };
	u8 base_addr;
	int i = 0;

	base_addr = SY6541_REG_00;
	ret = cp_read_i2c_block_data(bq->client, base_addr, CP_DUMP_REG_LEN, data);
	if (ret < 0) {
		mca_log_err("dump registers failed, base_addr: 0x%02X\n", base_addr);
	} else {
		memcpy(reg, data + 1, 15);
		for (i = 0; i < CP_DUMP_REG_LEN; i += 8) {
			mca_log_err("%s: [0x%02X]=0x%02X, [0x%02X]=0x%02X, [0x%02X]=0x%02X, [0x%02X]=0x%02X, "
				"[0x%02X]=0x%02X, [0x%02X]=0x%02X, [0x%02X]=0x%02X, [0x%02X]=0x%02X",
				bq->log_tag, base_addr + i, data[i], base_addr + i + 1, data[i + 1],
				base_addr + i + 2, data[i + 2], base_addr + i + 3, data[i + 3],
				base_addr + i + 4, data[i + 4], base_addr + i + 5, data[i + 5],
				base_addr + i + 6, data[i + 6], base_addr + i + 7, data[i + 7]);
		}
	}

	if (!!(data[11] & SY6541_OVPGATE_EN_MASK) != bq->ovpgate_en) {
		mca_log_err("set cp ovpgate not effective, repeat set ovpgate\n");
		sy6541_enable_ovpgate(bq, bq->ovpgate_en);
	}

	base_addr = SY6541_REG_10;
	ret = cp_read_i2c_block_data(bq->client, base_addr, CP_DUMP_REG_LEN, data);
	if (ret < 0) {
		mca_log_err("dump registers failed, base_addr: 0x%02X\n", base_addr);
	} else {
		reg[INT_STAT_REG] = data[SY6541_REG_10 - base_addr];
		reg[INT_FLAG_REG] = data[SY6541_REG_11 - base_addr];
		reg[FLT_FLAG_REG] = data[SY6541_REG_13 - base_addr];
		for (i = 0; i < CP_DUMP_REG_LEN; i += 8) {
			mca_log_err("%s: [0x%02X]=0x%02X, [0x%02X]=0x%02X, [0x%02X]=0x%02X, [0x%02X]=0x%02X, "
				"[0x%02X]=0x%02X, [0x%02X]=0x%02X, [0x%02X]=0x%02X, [0x%02X]=0x%02X",
				bq->log_tag, base_addr + i, data[i], base_addr + i + 1, data[i + 1],
				base_addr + i + 2, data[i + 2], base_addr + i + 3, data[i + 3],
				base_addr + i + 4, data[i + 4], base_addr + i + 5, data[i + 5],
				base_addr + i + 6, data[i + 6], base_addr + i + 7, data[i + 7]);
		}
	}

	base_addr = SY6541_REG_6E;
	ret = cp_read_i2c_block_data(bq->client, base_addr, 1, data);
	if (ret < 0) {
		mca_log_err("dump registers failed, base_addr: 0x%02X\n", base_addr);
	} else {
		reg[DEVICE_ID_REG] = data[SY6541_REG_6E - base_addr];
		mca_log_err("%s: [0x%02X]=0x%02X", bq->log_tag, base_addr, data[0]);
	}

	// CONVERTER STATE Register (Address=0Ah)
	if ((reg[CONVERTER_STATE_REG] & SY6541_CP_SWITCHING_STAT_MASK) == 0) {
		mca_log_err("%s cp switching stop, enter abnormal charging judge\n", bq->log_tag);
		sy6541_abnormal_charging_judge(bq, reg);
	}

	return 0;
}

static int sy6541_init_protection(struct sy6541_device *cp, int work_mode)
{
	int ret = 0;

	ret = sy6541_enable_batovp(cp, true);
	ret |= sy6541_enable_busocp(cp, true);
	ret |= sy6541_enable_busucp(cp, true);
	ret |= sy6541_set_vbus_errorlo_rf(cp, 0);
	ret |= sy6541_set_vbus_errorhi_rf(cp, 15);
	ret |= sy6541_set_batovp_th(cp, cp->cfg.bat_ovp_th);
	ret |= sy6541_set_busovp_th(cp, cp->cfg.bus_ovp_th[work_mode]);
	ret |= sy6541_set_usbovp_th(cp, cp->cfg.usb_ovp_th[work_mode]);
	ret |= sy6541_set_busocp_th(cp, cp->cfg.bus_ocp_th[work_mode]);
	ret |= sy6541_set_wpcovp_th(cp, cp->cfg.wpc_ovp_th);
	ret |= sy6541_set_outovp_th(cp, cp->cfg.out_ovp_th);
	if (cp->operation_mode == CP_MODE_FORWARD_4_1) {
		ret |= sy6541_enable_dt_bit(cp, FALSE);
	} else if (cp->operation_mode == CP_MODE_FORWARD_2_1
		|| cp->operation_mode == CP_MODE_FORWARD_1_1) {
		ret |= sy6541_enable_dt_bit(cp, TRUE);
	}

	return ret;
}

static int sy6541_init_adc(struct sy6541_device *cp)
{
	sy6541_set_adc_scanrate(cp, false);
	sy6541_set_adc_scan(cp, ADC_IBUS, true);
	sy6541_set_adc_scan(cp, ADC_VBUS, true);
	sy6541_set_adc_scan(cp, ADC_VUSB, true);
	sy6541_set_adc_scan(cp, ADC_VWPC, true);
	sy6541_set_adc_scan(cp, ADC_VOUT, true);
	sy6541_set_adc_scan(cp, ADC_VBAT, true);
	sy6541_set_adc_scan(cp, ADC_TDIE, true);
	sy6541_enable_adc(cp, false);

	return 0;
}

static int sy6541_init_device(struct sy6541_device *cp)
{
	int ret = 0;
	int retry_cnt = 0;

	sy6541_set_reg_reset(cp);
	while (retry_cnt < ERROR_RECOVERY_COUNT)	{
		ret |= sy6541_set_ibus_ucp_timeout(cp, SY6541_IBUS_UCP_TIMEOUT_5120MS);
		ret |= sy6541_set_ucp_fall_dg(cp, SY6541_BUS_UCP_FALL_DG_4MS);
		ret |= sy6541_set_ovpgate_on_dg_set(cp, SY6541_OVPGATE_ON_DG_10MS);
		ret |= sy6541_init_adc(cp);
		ret |= sy6541_set_operation_mode(cp, CP_MODE_FORWARD_2_1);
		cp->fsw_cfg.min = SY6541_FSW_MIN;
		cp->fsw_cfg.max = SY6541_FSW_MAX;
		cp->fsw_cfg.step = SY6541_FSW_STEP;
		sy6541_set_fsw(cp, CP_DEFAULT_FSW);
		ret = cp_update_bits(cp->client, SY6541_REG_F6, SY6541_DT_SET_MASK, 0x20);
		if (ret < 0) {
			retry_cnt++;
		} else {
			mca_log_err("%s success to init CP device\n", cp->log_tag);
			break;
		}
	}

	return ret;
}

static int cp_get_adc_data(struct sy6541_device *bq, int channel,  u32 *result)
{
	int ret = 0;

	ret = sy6541_get_adc_data(bq, channel, result);
	if (ret)
		mca_log_err(" %s failed get ADC value\n", bq->log_tag);
	return ret;
}

static int cp_enable_adc(struct sy6541_device *bq, bool enable)
{
	int ret = 0;

	ret = sy6541_enable_adc(bq, enable);
	if (ret)
		mca_log_err("%s failed to enable/disable ADC\n", bq->log_tag);
	return ret;
}

static int ops_cp_dump_register(struct charger_device *chg_dev)
{
	struct sy6541_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = sy6541_dump_important_regs(bq);
	if (ret)
		mca_log_err("%s failed dump registers ret=%d\n", bq->log_tag, ret);
	return ret;
}

static int ops_cp_enable_charge(struct charger_device *chg_dev, bool enable)
{
	struct sy6541_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = sy6541_enable_charge(bq, enable);
	if (ret)
		mca_log_err("%s failed enable cp charge\n", bq->log_tag);
	return ret;
}

static int ops_cp_get_charge_enabled(struct charger_device *chg_dev, bool *enabled)
{
	struct sy6541_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = sy6541_check_charge_enabled(bq, enabled);
	if (ret)
		mca_log_err("%s failed get enable cp charge status ret =%d\n", bq->log_tag, ret);
	return ret;
}

static int ops_cp_get_vbus(struct charger_device *chg_dev, u32 *val)
{
	struct sy6541_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = cp_get_adc_data(bq, ADC_VBUS, val);
	return ret;
}

static int ops_cp_get_ibus(struct charger_device *chg_dev, u32 *val)
{
	struct sy6541_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = cp_get_adc_data(bq, ADC_IBUS, val);
	return ret;
}

static int ops_cp_set_mode(struct charger_device *chg_dev, int value)
{
	struct sy6541_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = sy6541_set_operation_mode(bq, value);
	if (ret)
		mca_log_err("%s failed set cp charge mode\n", bq->log_tag);
	return ret;
}

static int ops_cp_is_bypass_enabled(struct charger_device *chg_dev, bool *enabled)
{
	struct sy6541_device *bq = charger_get_data(chg_dev);
	int ret = 0;
	int cp_mode;

	ret = sy6541_get_operation_mode(bq, &cp_mode);
	if (ret)
		mca_log_err("%s failed to get div_mode\n", bq->log_tag);
	if (cp_mode == CP_FORWARD_1_TO_1) {
		*enabled = true;
	} else {
		*enabled = false;
	}

	return ret;
}

static int ops_cp_device_init(struct charger_device *chg_dev, int value)
{
	struct sy6541_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	switch (value) {
		case SY6541_FORWARD_4_1_CHARGER_MODE:
		case SY6541_REVERSE_1_4_CONVERTER_MODE:
			bq->work_mode = CP_MODE_DIV4;
			break;
		case SY6541_FORWARD_2_1_CHARGER_MODE:
		case SY6541_REVERSE_1_2_CONVERTER_MODE:
			bq->work_mode = CP_MODE_DIV2;
			break;
		case SY6541_FORWARD_1_1_CHARGER_MODE:
		case SY6541_REVERSE_1_1_CONVERTER_MODE:
			bq->work_mode = CP_MODE_DIV1;
			break;
		case SY6541_FORWARD_1_1_CHARGER_MODE1:
			bq->work_mode = CP_MODE_DIV1;
			break;
		case SY6541_REVERSE_1_1_CONVERTER_MODE1:
			bq->work_mode = CP_MODE_DIV1;
			break;
		default:
			mca_log_err("%s operation mode error%d\n", bq->log_tag, value);
			return -1;
			break;
	}
	// ret = sy6541_init_device(bq);
	ret = sy6541_init_protection(bq, bq->work_mode);
	if (ret)
		mca_log_err("%s failed init cp init protection\n", bq->log_tag);
	return ret;
}

static int ops_cp_enable_adc(struct charger_device *chg_dev, bool enable)
{
	struct sy6541_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = cp_enable_adc(bq, enable);
	return ret;
}

static int ops_cp_get_bypass_support(struct charger_device *chg_dev, bool *enabled)
{
	*enabled = true;
	mca_log_err("%s %d\n", __func__, *enabled);
	return 0;
}

static int ops_enable_acdrv_manual(struct charger_device *chg_dev, bool enable)
{
	struct sy6541_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = sy6541_enable_acdrv_manual(bq, enable);
	if (ret)
		mca_log_err("%s failed enable cp acdrv manual\n", bq->log_tag);
	return ret;
}

static int ops_cp_set_usb_gate_en(struct charger_device *chg_dev, bool enable)
{
	struct sy6541_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = sy6541_enable_ovpgate(bq, enable);
	if (ret)
		mca_log_err("%s failed enable cp ovpgate\n", bq->log_tag);
	return ret;
}

static int ops_cp_get_chip_ok(struct charger_device *chg_dev, int *val)
{
	struct sy6541_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	*val  = bq->chip_ok;
	return ret;
}

/* SY6541_OTG_EN */
static int sy6541_enable_otg(struct sy6541_device *bq, bool enable)
{
	int ret = 0;
	u8 val;

	val = enable ? SY6541_OTG_ENABLE : SY6541_OTG_DISABLE;
	val <<= SY6541_OTG_EN_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_0B,
				SY6541_OTG_EN_MASK, val);
	return ret;
}

static int sy6541_get_ovpgate_enable(struct sy6541_device *bq, bool *enable)
{
	int ret = 0;
	u8 val = 0;

	ret = cp_read_byte(bq->client, SY6541_REG_0B, &val);
	mca_log_info("ovpgate_status SY6541_REG_0B=0x%x\n", val);
	if (ret) {
		mca_log_info("get ovpgate enable fail\n");
		return ret;
	}
	*enable = !!(val & SY6541_OVPGATE_EN_MASK);
	return ret;
}

static int sy6541_set_revchg(struct sy6541_device *bq, bool enable)
{
	int cp_mode;
	bool ovpgate_enable = false, charging_enable = false;

	if (enable) {
		sy6541_enable_otg(bq, true);
		sy6541_set_operation_mode(bq, SY6541_REVERSE_1_2_CONVERTER_MODE);
		sy6541_enable_ovpgate(bq, true);
		sy6541_init_protection(bq, 1);

		sy6541_get_ovpgate_enable(bq, &ovpgate_enable);
		sy6541_get_operation_mode(bq, &cp_mode);
		mca_log_info("cp mode: %d, ovpgate_enable status: %d", cp_mode, ovpgate_enable);
		if ((cp_mode != SY6541_REVERSE_1_2_CONVERTER_MODE) || (!ovpgate_enable))
			return -1;
	} else {
		sy6541_enable_otg(bq, false);
		sy6541_enable_charge(bq, false);
		sy6541_enable_acdrv_manual(bq, false);
		sy6541_set_operation_mode(bq, SY6541_FORWARD_2_1_CHARGER_MODE);

		sy6541_get_operation_mode(bq, &cp_mode);
		sy6541_check_charge_enabled(bq, &charging_enable);
		mca_log_info("cp mode: %d, charging_enable: %d", cp_mode, charging_enable);
		if ((cp_mode != SY6541_FORWARD_2_1_CHARGER_MODE) || charging_enable)
			return -1;
	}

	return 0;
}

#define CP_SET_REVCHG_RETRY 10
static int ops_cp_set_revchg(struct charger_device *chg_dev, bool enable, int cp_mode)
{
	struct sy6541_device *bq = charger_get_data(chg_dev);
	int ret = 0;
	int i = 0;

	for (i = 0; i < CP_SET_REVCHG_RETRY; i++) {
		ret = sy6541_set_revchg(bq, enable);
		if (ret == 0)
			break;
		mca_log_info("failed set revchg, retry: %d\n", i);
		mdelay(20);
	}

	return ret;
}

/* SY6541_WPCGATE_EN */
static int sy6541_enable_wpcgate(struct sy6541_device *bq, bool enable)
{
	int ret = 0;
	u8 val;

	val = enable ? SY6541_WPCGATE_ENABLE : SY6541_WPCGATE_DISABLE;
	val <<= SY6541_WPCGATE_EN_SHIFT;
	ret = cp_update_bits(bq->client, SY6541_REG_0B,
				SY6541_WPCGATE_EN_MASK, val);
	return ret;
}

static int ops_cp_enable_wpcgate(struct charger_device *chg_dev, bool enable)
{
	struct sy6541_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = sy6541_enable_wpcgate(bq, enable);
	if (ret)
		mca_log_info("failed enable cp wpcgate\n");
	return ret;
}

#define SY6541_UCPWD_TIMEOUT_MASK 0x3F
static int sy6541_set_adjustadble_timeout(struct sy6541_device *bq, u8 val)
{
	return cp_update_bits(bq->client, SY6541_REG_0D,
			SY6541_UCPWD_TIMEOUT_MASK, val);
}

static int ops_cp_set_adjustadble_timeout(struct charger_device *chg_dev, int value)
{
	struct sy6541_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = sy6541_set_adjustadble_timeout(bq, value);
	if (ret)
		mca_log_info("failed set cp 0D\n");

	return ret;
}

static int ops_cp_get_tdie(struct charger_device *chg_dev, u32 *val)
{
	struct sy6541_device *bq = charger_get_data(chg_dev);
	return sy6541_get_tdie(bq, val);
}

static const struct charger_ops sy6541_chg_ops = {
	.enable = ops_cp_enable_charge,
	.is_enabled = ops_cp_get_charge_enabled,
	.get_vbus_adc = ops_cp_get_vbus,
	.get_ibus_adc = ops_cp_get_ibus,

	//.cp_get_vbatt = ops_cp_get_vbatt,
	.get_ibat_adc = NULL,
	.cp_set_mode = ops_cp_set_mode,
	.is_bypass_enabled = ops_cp_is_bypass_enabled,
	.cp_device_init = ops_cp_device_init,
	.cp_enable_adc = ops_cp_enable_adc,
	.cp_get_bypass_support = ops_cp_get_bypass_support,
	.cp_dump_register = ops_cp_dump_register,
	.enable_acdrv_manual = ops_enable_acdrv_manual,
	.set_pmic_ovp_en = NULL, //ops_cp_set_pmic_ovp_en,
	.set_ibus_ucp_en = NULL, //ops_cp_set_ibus_ucp_en,
	.cp_chip_ok = ops_cp_get_chip_ok,
	.cp_get_tdie = ops_cp_get_tdie,
	.cp_get_fault_type = NULL, //ops_cp_get_fault_type,
	.cp_clear_fault_type = NULL, //ops_cp_clear_fault_type,
	.set_usb_gate_en = ops_cp_set_usb_gate_en,
	.cp_enable_wpcgate = ops_cp_enable_wpcgate,
	.cp_set_adjustadble_timeout = ops_cp_set_adjustadble_timeout,
	.cp_enable_ovpgate = ops_cp_set_usb_gate_en,
	.cp_set_revchg = ops_cp_set_revchg,
	.cp_set_qb = NULL,
	.cp_set_pmid2outuvp_th = NULL,
	// .set_wpc_gate_en = ops_cp_set_wpc_gate_en,
	.cp_get_en_fail_status = NULL, //ops_cp_get_en_fail_status,
	.cp_set_en_fail_status = NULL, //ops_cp_set_en_fail_status,
};

static void sy6541_irq_handler(struct work_struct *work)
{
	struct sy6541_device *bq = container_of(work, struct sy6541_device, irq_handle_work.work);

	mca_log_err("%s handler\n", bq->log_tag);
	if (bq->i2c_is_working)
		sy6541_dump_important_regs(bq);
}

static irqreturn_t sy6541_int_isr(int irq, void *private)
{
	struct sy6541_device *bq = private;

	mca_log_err("%s %s\n", bq->log_tag, __func__);
	pm_wakeup_dev_event(bq->dev, 500, true);
	schedule_delayed_work(&bq->irq_handle_work, 0);

	return IRQ_HANDLED;
}

static int sy6541_parse_dt(struct sy6541_device *bq)
{
	struct device_node *np = bq->dev->of_node;
	int ret = 0;

	if (!np) {
		mca_log_err("device tree info missing\n");
		return -1;
	}

    bq->irq_gpio = of_get_named_gpio(np, "sc,sc858x,irq-gpio", 0);
    if (!gpio_is_valid(bq->irq_gpio)) {
        dev_err(bq->dev,"%s fail to valid gpio : %d\n", bq->log_tag, bq->irq_gpio);
        return -EINVAL;
    }

	bq->nlpm_gpio = of_get_named_gpio(np, "cp-nlpm-gpio", 0);
	if (!gpio_is_valid(bq->nlpm_gpio)) {
		mca_log_err("%s failed to parse  sc858_nlpm_gpio\n", bq->log_tag);
		//return -1;
	}

	of_property_read_u32(np, "ic_role", &bq->cp_role);
	if (bq->cp_role == SC8581_SLAVE) {
		strscpy(bq->log_tag, "[1]", sizeof("[1]"));
	} else {
		strscpy(bq->log_tag, "[0]", sizeof("[0]"));
	}
	of_property_read_u32(np, "bat-ovp-threshold", &bq->cfg.bat_ovp_th);
	of_property_read_u32_array(np, "bus-ovp-threshold", bq->cfg.bus_ovp_th, CP_MODE_DIV_MAX);
	mca_log_err("%s bus_ovp [%d,%d,%d]\n", bq->log_tag, bq->cfg.bus_ovp_th[0], bq->cfg.bus_ovp_th[1], bq->cfg.bus_ovp_th[2]);
	of_property_read_u32_array(np, "usb-ovp-threshold", bq->cfg.usb_ovp_th, CP_MODE_DIV_MAX);
	mca_log_err("%s usb_ovp [%d,%d,%d]\n", bq->log_tag, bq->cfg.usb_ovp_th[0], bq->cfg.usb_ovp_th[1], bq->cfg.usb_ovp_th[2]);
	of_property_read_u32_array(np, "bus-ocp-threshold", bq->cfg.bus_ocp_th, CP_MODE_DIV_MAX);
	mca_log_err("%s bus_ocp [%d,%d,%d]\n", bq->log_tag, bq->cfg.bus_ocp_th[0], bq->cfg.bus_ocp_th[1], bq->cfg.bus_ocp_th[2]);
	of_property_read_u32(np, "wpc-ovp-threshold", &bq->cfg.wpc_ovp_th);
	of_property_read_u32(np, "out-ovp-threshold", &bq->cfg.out_ovp_th);

	return ret;
}

static int sy6541_register_irq(struct sy6541_device *bq)
{
	int ret = 0;

	ret = devm_gpio_request(bq->dev, bq->irq_gpio, dev_name(bq->dev));
	if (ret < 0) {
		mca_log_err(" %s failed to master request gpio\n", bq->log_tag);
		return -1;
	}

	bq->irq = gpio_to_irq(bq->irq_gpio);
	if (bq->irq < 0) {
		mca_log_err("%s failed to master get gpio_irq\n", bq->log_tag);
		return -1;
	}

	ret = request_irq(bq->irq, sy6541_int_isr,
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		dev_name(bq->dev), bq);
	if (ret < 0) {
		mca_log_err("%s failed to master request irq\n", bq->log_tag);
		return -1;
	}
	enable_irq_wake(bq->irq);

	return 0;
}

static int sy6541_init_gpio(struct sy6541_device *bq)
{
	int ret = 0;

	if (bq->cp_role != SC8581_MASTER)
		return ret;

	ret = devm_gpio_request(bq->dev, bq->nlpm_gpio, dev_name(bq->dev));
	if (ret)
		mca_log_err("%s unable to request nlpm gpio [%d]\n", bq->log_tag, bq->nlpm_gpio);
	else {
		ret = gpio_direction_output(bq->nlpm_gpio, 1);
		if (ret)
			mca_log_err("%s unable to set direction for nlpm gpio[%d]\n", bq->log_tag, bq->nlpm_gpio);
		msleep(400);
	}

	return ret;
}

/*static int is_sy6541_cp(struct sy6541_device *bq)
{
	int ret = 0;
	u8 data;
	int retry_cnt = 0;

	while (retry_cnt < ERROR_RECOVERY_COUNT) {
		ret = cp_read_byte(bq->client, SY6541_REG_6E, &data);
		if (ret < 0) {
			retry_cnt++;
			msleep(100);
			mca_log_err(" %s failed to read sy6541 device id, retry count = %d\n",
				bq->log_tag, retry_cnt);
		} else
			break;
	}

	if (retry_cnt == ERROR_RECOVERY_COUNT && ret < 0) {
		mca_log_err("%s failed to detect sy6541 device. retry %d times\n",
			bq->log_tag, retry_cnt);
		return ret;
	}

	return data;
}*/

static int sy6541_register_charger(struct sy6541_device *bq, int work_mode)
{
	switch (work_mode) {
	case SC8581_MASTER:
		bq->chg_dev = charger_device_register("cp_master", bq->dev, bq, &sy6541_chg_ops, &bq->chg_props);
		break;
	case SC8581_SLAVE:
		bq->chg_dev = charger_device_register("cp_slave", bq->dev, bq, &sy6541_chg_ops, &bq->chg_props);
		break;
	default:
		dev_err(bq->dev, "not support work_mode\n");
		return -EINVAL;
	}

	return 0;
}

static const enum power_supply_property sy6541_charger_props[] = {
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static int sy6541_charger_get_property(struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{
    struct sy6541_device *bq = power_supply_get_drvdata(psy);
	u32 vbat = 0;

    switch (psp) {
    case POWER_SUPPLY_PROP_STATUS:
        val->intval = 0;
        break;
    case POWER_SUPPLY_PROP_PRESENT:
        val->intval = bq->usb_present;
        break;
    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		cp_get_adc_data(bq, ADC_VBAT, &vbat);
		val->intval = vbat;
		break;
    default:
        return -EINVAL;
    }

    return 0;
}

static int sy6541_charger_set_property(struct power_supply *psy,
                    enum power_supply_property prop,
                    const union power_supply_propval *val)
{
    struct sy6541_device *sc = power_supply_get_drvdata(psy);

    switch (prop) {
    case POWER_SUPPLY_PROP_PRESENT:
        sc->usb_present = !!val->intval;
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

static int sy6541_charger_is_writeable(struct power_supply *psy,
                    enum power_supply_property prop)
{
    int ret;

    switch (prop) {
    case POWER_SUPPLY_PROP_PRESENT:
        ret = 1;
        break;
    default:
        ret = 0;
        break;
    }
    return ret;
}

static int sy6541_psy_register(struct sy6541_device *bq)
{
    bq->psy_cfg.drv_data = bq;
    bq->psy_cfg.of_node = bq->dev->of_node;

    bq->psy_desc.name = sy6541_psy_name[bq->cp_role];

    bq->psy_desc.type = POWER_SUPPLY_TYPE_MAINS;
    bq->psy_desc.properties = sy6541_charger_props;
    bq->psy_desc.num_properties = ARRAY_SIZE(sy6541_charger_props);
    bq->psy_desc.get_property = sy6541_charger_get_property;
    bq->psy_desc.set_property = sy6541_charger_set_property;
    bq->psy_desc.property_is_writeable = sy6541_charger_is_writeable;


    bq->fc2_psy = devm_power_supply_register(bq->dev,
            &bq->psy_desc, &bq->psy_cfg);
    if (IS_ERR(bq->fc2_psy)) {
        pr_err("failed to register fc2_psy\n");
        return PTR_ERR(bq->fc2_psy);
    }

    mca_log_err("%s power supply register successfully\n", bq->psy_desc.name);

    return 0;
}

static int cp_vbus_get(struct sy6541_device *sc,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u32 data = 0;

	if (sc) {
		ret = cp_get_adc_data(sc, ADC_VBUS, &data);
		*val = data;
	} else {
		*val = 0;
	}
	mca_log_err("%s cp_vbus=%d\n",  sc->log_tag, *val);
	return 0;
}

static int cp_ibus_get(struct sy6541_device *sc,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u32 data = 0;

	if (sc) {
		ret = cp_get_adc_data(sc, ADC_IBUS, &data);
		*val = data;
	} else {
		*val = 0;
	}
	mca_log_err("%s cp_ibus=%d\n", sc->log_tag, *val);
	return 0;
}

static int cp_tdie_get(struct sy6541_device *sc,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u32 data = 0;

	if (sc) {
		ret = cp_get_adc_data(sc, ADC_TDIE, &data);
		*val = data;
	} else
		*val = 0;
	//ln_err("%s %d\n", __func__, *val);
	return 0;
}

static int chip_ok_get(struct sy6541_device *sc,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	if (sc)
		*val = sc->chip_ok;
	else
		*val = 0;
	//ln_err("%s %d\n", __func__, *val);
	return 0;
}

static ssize_t cp_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy;
	struct sy6541_device *sc;
	struct mtk_cp_sysfs_field_info *usb_attr;
	int val;
	ssize_t ret;

	ret = kstrtos32(buf, 0, &val);
	if (ret < 0)
		return ret;

	psy = dev_get_drvdata(dev);
	sc = (struct sy6541_device *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct mtk_cp_sysfs_field_info, attr);
	if (usb_attr->set != NULL)
		usb_attr->set(sc, usb_attr, val);

	return count;
}

static ssize_t cp_sysfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct sy6541_device *sc;
	struct mtk_cp_sysfs_field_info *usb_attr;
	int val = 0;
	ssize_t count;

	psy = dev_get_drvdata(dev);
	sc = (struct sy6541_device *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct mtk_cp_sysfs_field_info, attr);
	if (usb_attr->get != NULL)
		usb_attr->get(sc, usb_attr, &val);

	count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
	return count;
}

static struct mtk_cp_sysfs_field_info cp_sysfs_field_tbl[] = {
	CP_SYSFS_FIELD_RO(cp_vbus, CP_PROP_VBUS),
	CP_SYSFS_FIELD_RO(cp_ibus, CP_PROP_IBUS),
	CP_SYSFS_FIELD_RO(cp_tdie, CP_PROP_TDIE),
	CP_SYSFS_FIELD_RO(chip_ok, CP_PROP_CHIP_OK),
};

static struct attribute *
	cp_sysfs_attrs[ARRAY_SIZE(cp_sysfs_field_tbl) + 1];

static const struct attribute_group cp_sysfs_attr_group = {
	.attrs = cp_sysfs_attrs,
};

static void cp_sysfs_init_attrs(void)
{
	int i, limit = ARRAY_SIZE(cp_sysfs_field_tbl);

	for (i = 0; i < limit; i++)
		cp_sysfs_attrs[i] = &cp_sysfs_field_tbl[i].attr.attr;

	cp_sysfs_attrs[limit] = NULL; /* Has additional entry for this */
}

static int cp_sysfs_create_group(struct power_supply *psy)
{
	cp_sysfs_init_attrs();

	return sysfs_create_group(&psy->dev.kobj,
			&cp_sysfs_attr_group);
}

int sy6541_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct sy6541_device *bq;
	int ret = 0;

	bq = devm_kzalloc(dev, sizeof(*bq), GFP_KERNEL);
	if (!bq)
		return -ENOMEM;

	bq->client = client;
	bq->dev = dev;
	//i2c_set_clientdata(client, bq);

	ret = sy6541_parse_dt(bq);
	if (ret) {
		mca_log_err(" %s failed to parse DTS\n", bq->log_tag);
		return ret;
	}

	ret = sy6541_init_gpio(bq);
	if (ret) {
		mca_log_err(" %s failed to init gpio\n", bq->log_tag);
		return ret;
	}

	ret = sy6541_register_irq(bq);
	if (ret) {
		mca_log_err("%s failed to int irq\n", bq->log_tag);
		return ret;
	}

	ret = sy6541_register_charger(bq, bq->cp_role);
    if (ret) {
	    pr_err("failed to register charger\n");
	    return ret;
    }

	ret = sy6541_init_device(bq);
	if (ret) {
		mca_log_err("%s failed to init sy6541\n", bq->log_tag);
		return ret;
	}

	ret = sy6541_psy_register(bq);
	if (ret) {
		mca_log_err("%s psy register failed %d\n", bq->log_tag, ret);
		return ret;
	}

	ret = cp_sysfs_create_group(bq->fc2_psy);
	if (ret) {
		mca_log_err("%s cp_sysfs_create_group failed %d\n", bq->log_tag, ret);
		return ret;
	}

	INIT_DELAYED_WORK(&bq->irq_handle_work, sy6541_irq_handler);
	schedule_delayed_work(&bq->irq_handle_work, 0);

	bq->chip_ok = true;
	bq->ovpgate_en = true;
	bq->i2c_is_working = true;

	ret = sy6541_dump_important_regs(bq);
	if (ret)
		mca_log_err("%s failed dump registers ret=%d\n", bq->log_tag, ret);

	device_init_wakeup(bq->dev, true);
	mca_log_err("%s probe success %d\n", bq->log_tag, ret);

	return 0;
}
EXPORT_SYMBOL(sy6541_probe);

MODULE_AUTHOR("weijun <weijun@xiaomi.com>");
MODULE_DESCRIPTION("sy6541 driver");
MODULE_LICENSE("GPL v2");
