/*
 * bq28z610 fuel gauge driver
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

#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/regmap.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include "bq28z610.h"
#include "pmic_voter.h"
#include "mtk_charger.h"
#include "charger_partition.h"
#include "xm_chg_uevent.h"

enum product_name {
	XAGA_NO,
	XAGA,
	XAGAPRO,
	DAUMIER,
};

static int log_level = 1;
static int product_name = XAGA_NO;
static ktime_t time_init = -1;

#define fg_err(fmt, ...)					\
do {								\
	if (log_level >= 0)					\
			printk(KERN_ERR "[HQ_FG]" fmt, ##__VA_ARGS__);	\
} while (0)

#define fg_info(fmt, ...)					\
do {								\
	if (log_level >= 1)					\
			printk(KERN_ERR "[HQ_FG]" fmt, ##__VA_ARGS__);	\
} while (0)

#define fg_dbg(fmt, ...)					\
do {								\
	if (log_level >= 2)					\
			printk(KERN_ERR "[HQ_FG]" fmt, ##__VA_ARGS__);	\
} while (0)

static struct regmap_config fg_regmap_config = {
	.reg_bits  = 8,
	.val_bits  = 8,
	.max_register  = 0xFF,
};

static int __fg_read_byte(struct i2c_client *client, u8 reg, u8 *val)
{
	int ret = 0;

	ret =  i2c_smbus_read_byte_data(client, reg);
	if(ret < 0)
	{
		fg_info("i2c read byte failed: can't read from reg 0x%02X faild\n", reg);
		return ret;
	}

	*val = (u8)ret;

	return 0;
}

static int fg_read_byte(struct bq_fg_chip *bq, u8 reg, u8 *val)
{
	int ret;
	mutex_lock(&bq->i2c_rw_lock);
	ret = __fg_read_byte(bq->client, reg, val);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static int fg_read_word(struct bq_fg_chip *bq, u8 reg, u16 *val)
{
	u8 data[2] = {0, 0};
	int ret = 0;

	if(atomic_read(&bq->fg_in_sleep))
	{
		fg_err("%s in sleep\n", __func__);
		return -EINVAL;
	}

	ret = regmap_raw_read(bq->regmap, reg, data, 2);
	if (ret) {
		fg_info("%s I2C failed to read 0x%02x\n", bq->log_tag, reg);
		return ret;
	}

	*val = (data[1] << 8) | data[0];
	return ret;
}

static int fg_read_block(struct bq_fg_chip *bq, u8 reg, u8 *buf, u8 len)
{
	int ret = 0, i = 0;
	unsigned int data = 0;

	if(atomic_read(&bq->fg_in_sleep))
	{
		fg_err("%s in sleep\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < len; i++) {
		ret = regmap_read(bq->regmap, reg + i, &data);
		if (ret) {
			fg_info("%s I2C failed to read 0x%02x\n", bq->log_tag, reg + i);
			return ret;
		}
		buf[i] = data;
	}

	return ret;
}

static int fg_write_block(struct bq_fg_chip *bq, u8 reg, u8 *data, u8 len)
{
	int ret = 0, i = 0;

	if(atomic_read(&bq->fg_in_sleep))
	{
		fg_err("%s in sleep\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < len; i++) {
		ret = regmap_write(bq->regmap, reg + i, (unsigned int)data[i]);
		if (ret) {
			fg_err("%s I2C failed to write 0x%02x\n", bq->log_tag, reg + i);
			return ret;
		}
	}

	return ret;
}

static u8 fg_checksum(u8 *data, u8 len)
{
	u8 i;
	u16 sum = 0;

	for (i = 0; i < len; i++) {
		sum += data[i];
	}

	sum &= 0xFF;

	return 0xFF - sum;
}

static int fg_mac_read_block(struct bq_fg_chip *bq, u16 cmd, u8 *buf, u8 len)
{
	int ret;
	u8 cksum_calc, cksum;
	u8 t_buf[40];
	u8 t_len;
	int i;

	memset(t_buf, 0, sizeof(t_buf));

	t_buf[0] = (u8)cmd;
	t_buf[1] = (u8)(cmd >> 8);
	mutex_lock(&bq->i2c_rw_lock);
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, 2);
	if (ret < 0) {
		mutex_unlock(&bq->i2c_rw_lock);
		return ret;
	}

	//msleep(4);
	usleep_range(4000, 4100);

	ret = fg_read_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, 36);
	if (ret < 0) {
		mutex_unlock(&bq->i2c_rw_lock);
		return ret;
	}
	mutex_unlock(&bq->i2c_rw_lock);

	cksum = t_buf[34];
	t_len = t_buf[35];

	if (t_len > 41)
		t_len = 41;

	if (t_len < 2)
		t_len = 2;

	cksum_calc = fg_checksum(t_buf, t_len - 2);
	if (cksum_calc != cksum) {
		fg_err("%s failed to checksum\n", bq->log_tag);
		return 1;
	}

	for (i = 0; i < len; i++)
		buf[i] = t_buf[i+2];

	return 0;
}

static int fg_mac_write_block(struct bq_fg_chip *bq, u16 cmd, u8 *data, u8 len)
{
	int ret;
	u8 cksum;
	u8 t_buf[40];
	int i;
	bool secure_delay = false;

	if (len > 32)
		return -1;

	if (cmd > 0x4000 || cmd == 0x007B)
		secure_delay = true;

	t_buf[0] = (u8)cmd;
	t_buf[1] = (u8)(cmd >> 8);
	for (i = 0; i < len; i++)
		t_buf[i+2] = data[i];

	mutex_lock(&bq->i2c_rw_lock);
	/*write command/addr, data*/
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, len + 2);
	if (ret < 0) {
		mutex_unlock(&bq->i2c_rw_lock);
		fg_err("%s failed to write block\n", bq->log_tag);
		return ret;
	}

	cksum = fg_checksum(t_buf, len + 2);
	t_buf[0] = cksum;
	t_buf[1] = len + 4; /*buf length, cmd, CRC and len byte itself*/
	/*write checksum and length*/
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_MAC_CHKSUM], t_buf, 2);
	if (secure_delay)
		msleep(100);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static int fg_sha256_auth(struct bq_fg_chip *bq, u8 *challenge, int length)
{
	int ret = 0;
	u8 cksum_calc = 0, data[2] = {0};

	/*
	1. The host writes 0x00 to 0x3E.
	2. The host writes 0x00 to 0x3F
	*/
	data[0] = 0x00;
	data[1] = 0x00;
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], data, 2);
	if (ret < 0)
		return ret;
	/*
	3. Write the random challenge should be written in a 32-byte block to address 0x40-0x5F
	*/
	msleep(2);

	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_MAC_DATA], challenge, length);
	if (ret < 0)
		return ret;

	/*4. Write the checksum (2’s complement sum of (1), (2), and (3)) to address 0x60.*/
	cksum_calc = fg_checksum(challenge, length);
	ret = regmap_write(bq->regmap, bq->regs[BQ_FG_REG_MAC_CHKSUM], cksum_calc);
	if (ret < 0)
		return ret;

	/*5. Write the length to address 0x61.*/
	ret = regmap_write(bq->regmap, bq->regs[BQ_FG_REG_MAC_DATA_LEN], length + 4);
	if (ret < 0)
		return ret;

	msleep(300);

	ret = fg_read_block(bq, bq->regs[BQ_FG_REG_MAC_DATA], bq->digest, length);
	if (ret < 0)
		return ret;

	return 0;
}

static int fg_read_status(struct bq_fg_chip *bq)
{
	u16 flags = 0;
	int ret = 0;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_BATT_STATUS], &flags);
	if (ret < 0)
		return ret;

	bq->batt_fc = !!(flags & BIT(5));

	return 0;
}

static int fg_read_rsoc(struct bq_fg_chip *bq)
{
	u16 soc = 0;
	bool retry = false;
	int ret = 0;
	static int pre_soc = 50;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_SOC], &soc);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read RSOC\n", bq->log_tag);
			soc = pre_soc;
			if(bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	}
	else
	{
		if(bq->i2c_error_count > 0)
			bq->i2c_error_count = 0;
	}
	pre_soc = soc;
	return soc;
}

static int fg_read_temperature(struct bq_fg_chip *bq)
{
	u16 tbat = 0;
	bool retry = false;
	int ret = 0;

	if (bq->fake_tbat)
		return bq->fake_tbat;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_TEMP], &tbat);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read TBAT\n", bq->log_tag);
			tbat = 2980;
			if(bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	}
	else
	{
		if(bq->i2c_error_count > 0)
			bq->i2c_error_count = 0;
	}
	fg_err("%s read FG TBAT = %d\n", bq->log_tag, tbat);
	if (!tbat)
		tbat = 2980;

	return tbat - 2730;
}

static void fg_read_cell_voltage(struct bq_fg_chip *bq)
{
	u8 data[64] = {0};
	bool retry = false;
	int ret = 0;

retry:
	ret = fg_mac_read_block(bq, FG_MAC_CMD_DASTATUS1, data, 32);
	if (ret) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read cell voltage\n", bq->log_tag);
			bq->cell_voltage[0] = 4000;
			bq->cell_voltage[1] = 4000;
			if(bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	} else {
		bq->cell_voltage[0] = (data[1] << 8) | data[0];
		bq->cell_voltage[1] = (data[3] << 8) | data[2];
		if(bq->i2c_error_count > 0)
			bq->i2c_error_count = 0;
	}

	bq->cell_voltage[2] = 2 * max(bq->cell_voltage[0], bq->cell_voltage[1]);
}

static int fg_read_volt(struct bq_fg_chip *bq)
{
	u16 vbat = 0;
	bool retry = false;
	int ret = 0;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_VOLT], &vbat);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read VBAT\n", bq->log_tag);
			vbat = 4000;
			if(bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	}
	else
	{
		if(bq->i2c_error_count > 0)
			bq->i2c_error_count = 0;
	}

	bq->vbat = (int)vbat;

	if (bq->device_name == BQ_FG_BQ28Z610)
		fg_read_cell_voltage(bq);
	else
		bq->cell_voltage[0] = bq->cell_voltage[1] = bq->cell_voltage[2] = bq->vbat;

	return ret;
}

static int fuelguage_check_fg_status(struct fuel_gauge_dev *fuel_gauge)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);
	int ret = 0;

	/*bit 0:batterysecret, bit 1:i2c status, bit 2~3:isc status, bit 4 charge_watt_err, bit 5 authenticate done*/
	ret = bq->batt_auth_done << 5 |bq->charge_watt_err << 4 | bq->isc_status << 2 | bq->i2c_err_flag << 1 | !(!!bq->authenticate);

	return ret;
}

static int fuelguage_get_batt_auth(struct fuel_gauge_dev *fuel_gauge)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);
	int ret = 0;

	if (bq)
		ret = bq->authenticate;
	else
		ret = 0;

	return ret;
}

static int fuelguage_report_fg_soc100(struct fuel_gauge_dev *fuel_gauge)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);
	u8 data[5] = {0};
	int ret = -1;

	data[0] = 1;
	if (bq) {
		ret =fg_mac_write_block(bq, FG_MAC_CMD_REPORT_SOC100, data,1);
		if (ret < 0)
			fg_err("set report aoc 100 err");
	}

	return ret;
}

static int fuelguage_get_rsoc(struct fuel_gauge_dev *fuel_gauge)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);

	if (bq)
		return bq->rsoc;
	else
		return -1;
}

static int fuelguage_get_raw_soc(struct fuel_gauge_dev *fuel_gauge)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);

	return bq->raw_soc;
}

static int fg_read_avg_current(struct bq_fg_chip *bq)
{
	s16 avg_ibat = 0;
	bool retry = false;
	int ret = 0;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_AI], (u16 *)&avg_ibat);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read AVG_IBAT\n", bq->log_tag);
			avg_ibat = 0;
			if(bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	}
	else
	{
		if(bq->i2c_error_count > 0)
			bq->i2c_error_count = 0;
	}
	avg_ibat = -1 * avg_ibat;

	return avg_ibat;
}

static int fg_read_current(struct bq_fg_chip *bq)
{
	s16 ibat = 0;
	bool retry = false;
	int ret = 0;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CN], (u16 *)&ibat);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read IBAT\n", bq->log_tag);
			ibat = 0;
			if(bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	}
	else
	{
		if(bq->i2c_error_count > 0)
			bq->i2c_error_count = 0;
	}
	if (ibat == 0 && bq->bq_charging_status == POWER_SUPPLY_STATUS_DISCHARGING)
		ibat = -1 * bq->ibat;
	ibat = -1 * ibat;
	return ibat;
}

static int fg_read_fcc(struct bq_fg_chip *bq)
{
	u16 fcc = 0;
	bool retry = false;
	int ret = 0;
	static int pre_fcc = 5160;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_FCC], &fcc);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fcc = pre_fcc;
			fg_err("%s failed to read FCC,FCC=%d\n", bq->log_tag, fcc);
			if(bq-> i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	}
	else
	{
		if(bq->i2c_error_count > 0)
			bq->i2c_error_count = 0;
	}
	pre_fcc = fcc;
	bq->fcc = fcc;
	return fcc;
}

static int fg_read_rm(struct bq_fg_chip *bq)
{
	u16 rm = 0;
	bool retry = false;
	int ret = 0;
	static int pre_rm =2580;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_RM], &rm);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			rm = 708;
			fg_err("%s failed to read RM,RM=%d\n", bq->log_tag, rm);
			if(bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	}
	else
	{
		if(bq->i2c_error_count > 0)
			bq->i2c_error_count = 0;
	}
	pre_rm = rm;
	bq->rm = rm;
	return rm;
}

static int fg_read_dc(struct bq_fg_chip *bq)
{
	u16 dc = 0;
	bool retry = false;
	int ret = 0;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_DC], &dc);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read DC\n", bq->log_tag);
			dc = 5160;
			if(bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	}
	else
	{
		if(bq->i2c_error_count > 0)
			bq->i2c_error_count = 0;
	}

	return dc;
}

static int fg_read_soh(struct bq_fg_chip *bq)
{
	u16 soh = 0;
	bool retry = false;
	int ret = 0;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_SOH], &soh);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read SOH\n", bq->log_tag);
			soh = 50;
			if(bq-> i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	}
	else
	{
		if(bq->i2c_error_count > 0)
			bq->i2c_error_count = 0;
	}

	return soh;
}

#if 0
static int fg_read_cv(struct bq_fg_chip *bq)
{
	u16 cv = 0;
	bool retry = false;
	int ret = 0;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CHG_VOL], &cv);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read CV\n", bq->log_tag);
			cv = 4480;
		}
	}

	return cv;
}

static int fg_read_cc(struct bq_fg_chip *bq)
{
	u16 cc = 0;
	bool retry = false;
	int ret = 0;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CHG_CUR], &cc);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read CC\n", bq->log_tag);
			cc = 11000;
		}
	}

	return cc;
}
#endif

static int fg_read_cyclecount(struct bq_fg_chip *bq)
{
	u16 cc = 0;
	bool retry = false;
	int ret = 0;
	static u16 pre_cc = 0;

	if(bq->fake_cycle_count > 0)
	{
		return bq->fake_cycle_count;
	}

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CC], &cc);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			fg_err("%s failed to read CC\n", bq->log_tag);
			cc = pre_cc;
			if(bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	}
	else
	{
		if(bq->i2c_error_count > 0)
			bq->i2c_error_count = 0;
		pre_cc = cc;
	}

	return cc;
}

static int fg_get_raw_soc(struct bq_fg_chip *bq)
{
	int raw_soc = 0;

	bq->rm = fg_read_rm(bq);
	bq->fcc = fg_read_fcc(bq);

	raw_soc = bq->rm * 10000 / bq->fcc;

	return raw_soc;
}

static int fuel_guage_get_soc_decimal_rate(struct fuel_gauge_dev *fuel_gauge)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);
	int soc, i;

	if (bq->dec_rate_len <= 0)
		return 0;

	soc = fg_read_rsoc(bq);

	for (i = 0; i < bq->dec_rate_len; i += 2) {
		if (soc < bq->dec_rate_seq[i]) {
			return bq->dec_rate_seq[i - 1];
		}
	}

	return bq->dec_rate_seq[bq->dec_rate_len - 1];
}

static int fuel_guage_get_soc_decimal(struct fuel_gauge_dev *fuel_gauge)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);
	int rsoc, raw_soc;

	if (!bq)
		return 0;

	rsoc = fg_read_rsoc(bq);
	raw_soc = fg_get_raw_soc(bq);

	if (bq->ui_soc > rsoc)
		return 0;

	return raw_soc % 100;
}

static int fg_get_soc_decimal_rate(struct bq_fg_chip *bq)
{
	int soc, i;
	if (bq->dec_rate_len <= 0)
		return 0;
	soc = fg_read_rsoc(bq);
	for (i = 0; i < bq->dec_rate_len; i += 2) {
		if (soc < bq->dec_rate_seq[i]) {
			return bq->dec_rate_seq[i - 1];
		}
	}
	return bq->dec_rate_seq[bq->dec_rate_len - 1];
}

static int fg_get_soc_decimal(struct bq_fg_chip *bq)
{
	int rsoc, raw_soc;
	if (!bq)
		return 0;
	rsoc = fg_read_rsoc(bq);
	raw_soc = fg_get_raw_soc(bq);
	if (bq->ui_soc > rsoc)
		return 0;
	return raw_soc % 100;
}

static void fg_read_qmax(struct bq_fg_chip *bq)
{
	u8 data[64] = {0};
	int ret = 0;

	if (bq->device_name == BQ_FG_BQ27Z561 || bq->device_name == BQ_FG_NFG1000A || bq->device_name == BQ_FG_NFG1000B) {
		ret = fg_mac_read_block(bq, FG_MAC_CMD_DASTATUS1, data, 14);
		if (ret < 0)
			fg_err("%s failed to read MAC\n", bq->log_tag);
	} else if (bq->device_name == BQ_FG_BQ28Z610 ||  bq->device_name == MPC_FG_MPC8011B) {
		ret = fg_mac_read_block(bq, FG_MAC_CMD_DASTATUS1, data, 20);
		if (ret < 0)
			fg_err("%s failed to read MAC\n", bq->log_tag);
	} else {
		fg_err("%s not support device name\n", bq->log_tag);
	}

	bq->qmax[0] = (data[5] << 8) | data[4];
	bq->qmax[1] = (data[5] << 8) | data[4];
}

static int fg_set_fastcharge_mode(struct bq_fg_chip *bq, bool enable)
{
	u8 data[5] = {0};
	int ret = 0;

	if (bq->fast_chg == enable)
		return ret;
	else
		data[0] = bq->fast_chg = enable;

	if (bq->device_name == BQ_FG_BQ28Z610)
		return ret;

	if (enable) {
		ret = fg_mac_write_block(bq, FG_MAC_CMD_FASTCHARGE_EN, data, 2);
		fg_info("%s write 3e fastcharge = %d success\n", bq->log_tag, ret);
		if (ret) {
			fg_err("%s failed to write fastcharge = %d\n", bq->log_tag, ret);
			return ret;
		}
	} else {
		ret = fg_mac_write_block(bq, FG_MAC_CMD_FASTCHARGE_DIS, data, 2);
		fg_info("%s write 3f fastcharge = %d success\n", bq->log_tag, ret);
		if (ret) {
			fg_err("%s failed to write fastcharge = %d\n", bq->log_tag, ret);
			return ret;
		}
	}

	return ret;
}

static int fuelguage_set_fastcharge_mode(struct fuel_gauge_dev *fuel_gauge, bool en)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);
	int ret = 0;

	ret = fg_set_fastcharge_mode(bq, en);
	return ret;
}

static int fuelguage_get_fastcharge_mode(struct fuel_gauge_dev *fuel_gauge)
{
	struct bq_fg_chip *bq = fuel_gauge_get_private(fuel_gauge);

	return bq->fast_chg;
}

static int calc_delta_time(ktime_t time_last, int *delta_time)
{
	ktime_t time_now;

	time_now = ktime_get();

	*delta_time = ktime_ms_delta(time_now, time_last);
	if (*delta_time < 0)
		*delta_time = 0;

	fg_dbg("now:%lld, last:%lld, delta:%d\n", time_now, time_last, *delta_time);

	return 0;
}

struct ffc_smooth {
	int curr_lim;
	int time;
};

struct ffc_smooth ffc_dischg_smooth[FFC_SMOOTH_LEN] = {
	{0,    150000},
	{300,  100000},
	{600,   72000},
	{1000,  50000},
};

static int bq_battery_soc_smooth_tracking_sencond(struct bq_fg_chip *bq,
	int raw_soc, int batt_soc, int soc)
{
	static ktime_t changed_time = -1;
	int unit_time = 0, delta_time = 0;
	int change_delta = 0;
	int soc_changed = 0;

	if (bq->tbat < 150) {
		bq->monitor_delay = FG_MONITOR_DELAY_5S;
	}
	if (raw_soc > bq->report_full_rsoc) {
		if (raw_soc == 10000 && bq->last_soc < 99) {
			unit_time = 20000;
			calc_delta_time(changed_time, &change_delta);
			if (delta_time < 0) {
				changed_time = ktime_get();
				delta_time = 0;
			}
			delta_time = change_delta / unit_time;
			soc_changed = min(1, delta_time);
			if (soc_changed) {
				soc = bq->last_soc + soc_changed;
				fg_info("%s soc increase changed = %d\n", bq->log_tag, soc_changed);
			} else {
				soc = bq->last_soc;
			}
		} else {
			soc = 100;
		}
	} else if (raw_soc > 990) {
		soc += bq->soc_gap;
		if (soc > 99)
			soc = 99;
	} else {
		if (raw_soc == 0 && bq->last_soc > 1) {
			bq->ffc_smooth = false;
			unit_time = 5000;
			calc_delta_time(changed_time, &change_delta);
			delta_time = change_delta / unit_time;
			if (delta_time < 0) {
				changed_time = ktime_get();
				delta_time = 0;
			}
			soc_changed = min(1, delta_time);
			if (soc_changed) {
				fg_info("%s soc reduce changed = %d\n", bq->log_tag, soc_changed);
				soc = bq->last_soc - soc_changed;
			} else
				soc = bq->last_soc;
		} else {
			soc = (raw_soc + 89) / 90;
		}
	}

	if (soc >= 100)
		soc = 100;
	if (soc < 0)
		soc = batt_soc;

	if (bq->last_soc <= 0)
		bq->last_soc = soc;
	if (bq->last_soc != soc) {
		if(abs(soc - bq->last_soc) > 1){
			union power_supply_propval pval = {0, };
			int status,rc;

			rc = power_supply_get_property(bq->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
			status = pval.intval;

			calc_delta_time(changed_time, &change_delta);
			delta_time = change_delta / LOW_TEMP_CHARGING_DELTA;
			if (delta_time < 0) {
				changed_time = ktime_get();
				delta_time = 0;
			}
			soc_changed = min(1, delta_time);
			if(soc_changed){
				changed_time = ktime_get();
			}

			fg_info("avoid jump soc = %d last = %d soc_change = %d state = %d ,delta_time = %d\n",
					soc,bq->last_soc ,soc_changed,status,change_delta);

			if(status == POWER_SUPPLY_STATUS_CHARGING){
				if(soc > bq->last_soc){
					soc = bq->last_soc + soc_changed;
					bq->last_soc = soc;
				}else{
					fg_info("Do not smooth waiting real soc increase here\n");
					soc = bq->last_soc;
				}
			} else if(status != POWER_SUPPLY_STATUS_FULL){
				if(soc < bq->last_soc){
					soc = bq->last_soc - soc_changed;
					bq->last_soc = soc;
				}else{
					fg_info("Do not smooth waiting real soc decrease here\n");
					soc = bq->last_soc;
				}
			}
		}else{
			changed_time = ktime_get();
			bq->last_soc = soc;
		}
	}
	return soc;
}

static int bq_battery_soc_smooth_tracking(struct bq_fg_chip *bq,
		int raw_soc, int batt_soc, int batt_temp, int batt_ma)
{
	static int last_batt_soc = -1, system_soc, cold_smooth;
	static int last_status;
	int change_delta = 0, rc;
	int optimiz_delta = 0, status;
	static ktime_t last_change_time;
	static ktime_t last_optimiz_time;
	int unit_time = 0;
	int soc_changed = 0, delta_time = 0;
	static int optimiz_soc, last_raw_soc;
	union power_supply_propval pval = {0, };
	int batt_ma_avg, i;

	if (bq->optimiz_soc > 0) {
		bq->ffc_smooth = true;
		last_batt_soc = bq->optimiz_soc;
		system_soc = bq->optimiz_soc;
		last_change_time = ktime_get();
		bq->optimiz_soc = 0;
	}

	if (last_batt_soc < 0)
		last_batt_soc = batt_soc;

	if (raw_soc == FG_RAW_SOC_FULL)
		bq->ffc_smooth = false;

	if (bq->ffc_smooth) {
		rc = power_supply_get_property(bq->batt_psy,
				POWER_SUPPLY_PROP_STATUS, &pval);
		if (rc < 0) {
			fg_info("failed get batt staus\n");
			return -EINVAL;
		}
		status = pval.intval;
		if (batt_soc == system_soc) {
			bq->ffc_smooth = false;
			return batt_soc;
		}
		if (status != last_status) {
			if (last_status == POWER_SUPPLY_STATUS_CHARGING
					&& status == POWER_SUPPLY_STATUS_DISCHARGING)
				last_change_time = ktime_get();
			last_status = status;
		}
	}

	if (bq->fast_chg && raw_soc >= bq->report_full_rsoc && raw_soc != FG_RAW_SOC_FULL) {
		if (last_optimiz_time == 0)
			last_optimiz_time = ktime_get();
		calc_delta_time(last_optimiz_time, &optimiz_delta);
		delta_time = optimiz_delta / FG_OPTIMIZ_FULL_TIME;
		soc_changed = min(1, delta_time);
		if (raw_soc > last_raw_soc && soc_changed) {
			last_raw_soc = raw_soc;
			optimiz_soc += soc_changed;
			last_optimiz_time = ktime_get();
			fg_info("optimiz_soc:%d, last_optimiz_time%lld\n",
					optimiz_soc, last_optimiz_time);
			if (optimiz_soc > 100)
				optimiz_soc = 100;
			bq->ffc_smooth = true;
		}
		if (batt_soc > optimiz_soc) {
			optimiz_soc = batt_soc;
			last_optimiz_time = ktime_get();
		}
		if (bq->ffc_smooth)
			batt_soc = optimiz_soc;
		last_change_time = ktime_get();
	} else {
		optimiz_soc = batt_soc + 1;
		last_raw_soc = raw_soc;
		last_optimiz_time = ktime_get();
	}

	calc_delta_time(last_change_time, &change_delta);
	batt_ma_avg = fg_read_avg_current(bq);
	if (batt_temp > 150/* BATT_COOL_THRESHOLD */ && !cold_smooth && batt_soc != 0) {
		if (bq->ffc_smooth && (status == POWER_SUPPLY_STATUS_DISCHARGING ||
					status == POWER_SUPPLY_STATUS_NOT_CHARGING ||
					batt_ma_avg > 50)) {
			for (i = 1; i < FFC_SMOOTH_LEN; i++) {
				if (batt_ma_avg < ffc_dischg_smooth[i].curr_lim) {
					unit_time = ffc_dischg_smooth[i-1].time;
					break;
				}
			}
			if (i == FFC_SMOOTH_LEN) {
				unit_time = ffc_dischg_smooth[FFC_SMOOTH_LEN-1].time;
			}
		}
	} else {
		/* Calculated average current > 1000mA */
		if (batt_ma_avg > BATT_HIGH_AVG_CURRENT)
			/* Heavy loading current, ignore battery soc limit*/
			unit_time = LOW_TEMP_CHARGING_DELTA;
		else
			unit_time = LOW_TEMP_DISCHARGING_DELTA;
		if (batt_soc != last_batt_soc)
			cold_smooth = true;
		else
			cold_smooth = false;
	}
	if (unit_time > 0) {
		delta_time = change_delta / unit_time;
		soc_changed = min(1, delta_time);
	} else {
		if (!bq->ffc_smooth)
			bq->update_now = true;
	}

	fg_info("batt_ma_avg:%d, batt_ma:%d, cold_smooth:%d, optimiz_soc:%d",
			batt_ma_avg, batt_ma, cold_smooth, optimiz_soc);
	fg_info("delta_time:%d, change_delta:%d, unit_time:%d"
			" soc_changed:%d, bq->update_now:%d, bq->ffc_smooth:%d,bq->fast_chg:%d",
			delta_time, change_delta, unit_time,
			soc_changed, bq->update_now, bq->ffc_smooth,bq->fast_chg);

	if (last_batt_soc < batt_soc && batt_ma < 0)
		/* Battery in charging status
		 * update the soc when resuming device
		 */
		last_batt_soc = bq->update_now ?
			batt_soc : last_batt_soc + soc_changed;
	else if (last_batt_soc > batt_soc && batt_ma > 0) {
		/* Battery in discharging status
		 * update the soc when resuming device
		 */
		last_batt_soc = bq->update_now ?
			batt_soc : last_batt_soc - soc_changed;
	}
	bq->update_now = false;

	if (system_soc != last_batt_soc) {
		system_soc = last_batt_soc;
		last_change_time = ktime_get();
	}

	fg_info("raw_soc:%d batt_soc:%d,last_batt_soc:%d,system_soc:%d",
			raw_soc, batt_soc, last_batt_soc, system_soc);

	return system_soc;
}

struct mtk_charger* get_mtk_charger_info(void)
{
	struct power_supply *chg_psy = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		fg_err("%s Couldn't get chg_psy\n", __func__);
		return NULL;
	}

	return (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
}

static int bq_battery_soc_smooth_tracking_new(struct bq_fg_chip *bq, int raw_soc, int batt_soc, int batt_ma)
{
	static int system_soc, last_system_soc;
	int soc_changed = 0, unit_time = 10000, delta_time = 0, soc_delta = 0;
	static ktime_t last_change_time = -1;
	int change_delta = 0;
	int  rc, charging_status, i=0, batt_ma_avg = 0;
	union power_supply_propval pval = {0, };
	static int ibat_pos_count = 0;
	struct timespec64 time;
	ktime_t tmp_time = 0;
	struct mtk_charger *mtk_chg_info = NULL;

	tmp_time = ktime_get_boottime();
	time = ktime_to_timespec64(tmp_time);

	if((batt_ma > 0) && (ibat_pos_count < 10))
		ibat_pos_count++;
	else if(batt_ma <= 0)
		ibat_pos_count = 0;

	rc = power_supply_get_property(bq->batt_psy,
				POWER_SUPPLY_PROP_STATUS, &pval);
	if (rc < 0) {
		fg_info("failed get batt staus\n");
		return -EINVAL;
	}

	mtk_chg_info = get_mtk_charger_info();
	if (mtk_chg_info == NULL) {
		fg_info("failed get mtk charger info\n");
		return -EINVAL;
	}

	charging_status = pval.intval;
	bq->bq_charging_status = pval.intval;

	if (bq->tbat < 150 || !raw_soc) {
		bq->monitor_delay = FG_MONITOR_DELAY_3S;
	} else {
		bq->monitor_delay = FG_MONITOR_DELAY_10S;
	}

	/*Map system_soc value according to raw_soc */
	if(raw_soc >= bq->report_full_rsoc)
		system_soc = 100;
	else if (bq->max_chg_power_120w || product_name == XAGAPRO) {
		system_soc = ((raw_soc + 94) / 95);
		if(system_soc > 99)
			system_soc = 99;
	} else {
		system_soc = ((raw_soc + 97) / 98);
		if(system_soc > 99)
			system_soc = 99;
    }
	fg_info("%s smooth_new: fisrt step, system_soc = %d\n", __func__, system_soc);

	/*Get the initial value for the first time */
	if(last_change_time == -1) {
		last_change_time = ktime_get();
		if(system_soc != 0)
			last_system_soc = system_soc;
		else
			last_system_soc = batt_soc;
	}

	if ((charging_status == POWER_SUPPLY_STATUS_DISCHARGING ||
		charging_status == POWER_SUPPLY_STATUS_NOT_CHARGING ) &&
		!bq->rm && (bq->tbat < 150) && (last_system_soc >= 1)) {
		batt_ma_avg = fg_read_avg_current(bq);
		for (i = FFC_SMOOTH_LEN-1; i >= 0; i--) {
			if (batt_ma_avg > ffc_dischg_smooth[i].curr_lim) {
				unit_time = ffc_dischg_smooth[i].time;
				break;
			}
		}

		if (bq->vbat < bq->shutdown_delay_vol)
			unit_time = 10000;

		fg_info("enter low temperature smooth unit_time=%d batt_ma_avg=%d\n", unit_time, batt_ma_avg);
	}

	/*If the soc jump, will smooth one cap every 10S */
	soc_delta = abs(system_soc - last_system_soc);
	if(soc_delta > 1 || (bq->vbat < 3400 && system_soc > 0) || (unit_time != 10000 && soc_delta == 1) || system_soc == 100){
		//unit_time != 10000 && soc_delta == 1 fix low temperature 2% jump to 0%
		calc_delta_time(last_change_time, &change_delta);

		delta_time = change_delta / unit_time;
		if (delta_time < 0) {
			last_change_time = ktime_get();
			delta_time = 0;
		}

		soc_changed = min(1, delta_time);

		if (soc_changed) {
			if((charging_status == POWER_SUPPLY_STATUS_CHARGING || charging_status == POWER_SUPPLY_STATUS_FULL) && system_soc > last_system_soc)
				system_soc = last_system_soc + soc_changed;
			else if(charging_status == POWER_SUPPLY_STATUS_DISCHARGING && system_soc < last_system_soc)
				system_soc = last_system_soc - soc_changed;
		} else
			system_soc = last_system_soc;
		fg_info("fg jump smooth soc_changed=%d\n", soc_changed);
	}
	if(system_soc < last_system_soc)
		system_soc = last_system_soc - 1;
	/*Avoid mismatches between charging status and soc changes  */
	if (((charging_status == POWER_SUPPLY_STATUS_DISCHARGING) && (system_soc > last_system_soc)) ||
		((charging_status == POWER_SUPPLY_STATUS_NOT_CHARGING) && (system_soc > last_system_soc)) ||
		((charging_status == POWER_SUPPLY_STATUS_CHARGING) && (system_soc < last_system_soc) && (ibat_pos_count < 3) && ((time.tv_sec > 10))))
		system_soc = last_system_soc;
	fg_info("smooth_new:sys_soc:%d last_sys_soc:%d soc_delta:%d charging_status:%d unit_time:%d batt_ma_avg=%d\n" ,
		system_soc, last_system_soc, soc_delta, charging_status, unit_time, batt_ma_avg);

	if(system_soc != last_system_soc){
		last_change_time = ktime_get();
		last_system_soc = system_soc;
	}

	if(system_soc > 100)
		system_soc = 100;

	if (system_soc <= 0) {
		if (((bq->vbat >= bq->shutdown_vol) || (time.tv_sec <= 10) || bq->shutdown_delay ||
			(charging_status == POWER_SUPPLY_STATUS_CHARGING && time.tv_sec <= 40)) &&
			!bq->shutdown_flag) {
			system_soc = 1;
			fg_err("uisoc::hold 1 when volt > shutdown_vol. \n");
		} else {
			system_soc = 0;
			bq->shutdown_flag = true;
			fg_err("soc = 0, triggle shutdown\n");
		}
	}

	if (mtk_chg_info->product_name_index == EEA) {
		if ((bq->last_soc == 100) && (system_soc == 100) &&
			(charging_status == POWER_SUPPLY_STATUS_CHARGING)) {
			fg_info("when cable plug in ,uisoc was 100, need disable charge\n");
			mtk_chg_info->plug_in_soc100_flag = true;
		} else if ((system_soc == 100) && (charging_status == POWER_SUPPLY_STATUS_CHARGING)) {
			fg_info("hold uisoc = 99 until charger status is full\n");
			system_soc = 99;
		}
	}

	if(bq->last_soc != system_soc){
		bq->last_soc = system_soc;
	}

	return system_soc;
}

static int fg_set_shutdown_mode(struct bq_fg_chip *bq)
{
	int ret = 0;
	u8 data[5] = {0};

	fg_info("%s fg_set_shutdown_mode\n", bq->log_tag);
	bq->shutdown_mode = true;

	data[0] = 1;

	ret = fg_mac_write_block(bq, FG_MAC_CMD_SHUTDOWN, data, 2);
	if (ret)
		fg_err("%s failed to send shutdown cmd 0\n", bq->log_tag);

	ret = fg_mac_write_block(bq, FG_MAC_CMD_SHUTDOWN, data, 2);
	if (ret)
		fg_err("%s failed to send shutdown cmd 1\n", bq->log_tag);

	return ret;
}

static bool battery_get_psy(struct bq_fg_chip *bq)
{
	bq->batt_psy = power_supply_get_by_name("battery");
	if (!bq->batt_psy) {
		fg_err("%s failed to get batt_psy", bq->log_tag);
		return false;
	}
	return true;
}

/**************************support dynamic shutdown voltage start**************************/
static void run_shutdown_temp_state_machine(struct bq_fg_chip *bq)
{
	if (bq->tbat < -60) {
		if (bq->temp_state > BAT_COLD && bq->tbat >= -80) {
			bq->temp_state = BAT_LITTLE_COLD;
		} else {
			bq->temp_state = BAT_COLD;
                }
	} else if (bq->tbat < 0) {
		if (bq->temp_state <= BAT_COLD && bq->tbat < -40) {
			bq->temp_state = BAT_COLD;
		} else if (bq->temp_state > BAT_LITTLE_COLD && bq->tbat >= -20) {
			bq->temp_state = BAT_COOL;
		} else {
			bq->temp_state = BAT_LITTLE_COLD;
		}
	} else if (bq->tbat < 100) {
		if (bq->temp_state <= BAT_LITTLE_COLD && bq->tbat < 20) {
			bq->temp_state = BAT_LITTLE_COLD;
		} else if (bq->temp_state > BAT_COOL && bq->tbat >= 80) {
			bq->temp_state = BAT_NORMAL;
		} else {
			bq->temp_state = BAT_COOL;
		}
	} else {
		if (bq->temp_state <= BAT_COOL && bq->tbat < 120) {
			bq->temp_state = BAT_COOL;
		} else {
			bq->temp_state = BAT_NORMAL;
		}
	}
}

static void fg_get_count_shutdown_vol(struct bq_fg_chip *bq, int *count_vol)
{
	int count = bq->dod_count;

	switch(bq->temp_state) {
		case BAT_COLD:
			if (count <= 80) {
				count_vol[0] = 2800;
				count_vol[1] = 2850;
				count_vol[2] = 2900;
			} else if (count > 80 && count <= 199) {
				count_vol[0] = 2850;
				count_vol[1] = 2900;
				count_vol[2] = 2950;
			} else if (count > 199 && count <= 599) {
				count_vol[0] = 2900;
				count_vol[1] = 2950;
				count_vol[2] = 3000;
			} else {
				count_vol[0] = 2950;
				count_vol[1] = 3000;
				count_vol[2] = 3050;
			}
			break;
		case BAT_LITTLE_COLD:
			if (count <= 80) {
				count_vol[0] = 2800;
				count_vol[1] = 2850;
				count_vol[2] = 2900;
			} else if (count > 80 && count <= 199) {
				count_vol[0] = 2950;
				count_vol[1] = 3000;
				count_vol[2] = 3050;
			} else if (count > 199 && count <= 599) {
				count_vol[0] = 3000;
				count_vol[1] = 3050;
				count_vol[2] = 3100;
			} else {
				count_vol[0] = 3050;
				count_vol[1] = 3100;
				count_vol[2] = 3150;
			}
			break;
		case BAT_COOL:
			if (count <= 80) {
				count_vol[0] = 3000;
				count_vol[1] = 3050;
				count_vol[2] = 3050;
			} else if (count > 80 && count <= 199) {
				count_vol[0] = 3050;
				count_vol[1] = 3100;
				count_vol[2] = 3100;
			} else if (count > 199 && count <= 599) {
				count_vol[0] = 3100;
				count_vol[1] = 3150;
				count_vol[2] = 3150;
			} else {
				count_vol[0] = 3200;
				count_vol[1] = 3250;
				count_vol[2] = 3250;
			}
			break;
		case BAT_NORMAL:
			if (count <= 80) {
				count_vol[0] = 3000;
				count_vol[1] = 3050;
				count_vol[2] = 3050;
			} else if (count > 80 && count <= 199) {
				count_vol[0] = 3100;
				count_vol[1] = 3150;
				count_vol[2] = 3150;
			} else if (count > 199 && count <= 599) {
				count_vol[0] = 3200;
				count_vol[1] = 3250;
				count_vol[2] = 3250;
			} else {
				count_vol[0] = 3300;
				count_vol[1] = 3340;
				count_vol[2] = 3340;
			}
			break;
		default:
			count_vol[0] = 3400;
			count_vol[1] = 3400;
			count_vol[2] = 3400;
			break;
	}
}

static void fg_get_cycle_shutdown_vol(struct bq_fg_chip *bq, int *cycle_vol)
{
	int cycle = bq->cycle_count;

	switch(bq->temp_state) {
		case BAT_COLD:
		case BAT_LITTLE_COLD:
			if (cycle <= 600) {
				cycle_vol[0] = 2800;
				cycle_vol[1] = 2850;
				cycle_vol[2] = 2900;
			} else if (cycle > 600 && cycle <= 1200) {
				cycle_vol[0] = 3000;
				cycle_vol[1] = 3050;
				cycle_vol[2] = 3100;
			} else {
				cycle_vol[0] = 3050;
				cycle_vol[1] = 3100;
				cycle_vol[2] = 3150;
			}
			break;
		case BAT_COOL:
		case BAT_NORMAL:
			if (cycle <= 600) {
				cycle_vol[0] = 3000;
				cycle_vol[1] = 3050;
				cycle_vol[2] = 3050;
			} else if (cycle > 600 && cycle <= 1200) {
				cycle_vol[0] = 3200;
				cycle_vol[1] = 3250;
				cycle_vol[2] = 3250;
			} else {
				cycle_vol[0] = 3300;
				cycle_vol[1] = 3340;
				cycle_vol[2] = 3340;
			}
			break;
		default:
			cycle_vol[0] = 3400;
			cycle_vol[1] = 3400;
			cycle_vol[2] = 3400;
			break;
	}
}

static void battery_shutdown_vol_update(struct bq_fg_chip *bq)
{
	int ret = 0;
	int count_vol[3] = {0};
	int cycle_vol[3] = {0};
	int shutdown_vol, shutdown_delay_vol, shutdown_fg_vol = 3400;
	u8 t_buf[32] = {0};
	u8 data[2] = {0xff, 0xff};
	int count_2v8, count_3v1, count_3v3 = 0;

	if (bq->fake_dod_count) {
		bq->dod_count = bq->fake_dod_count;
		fg_info("%s: fake_dod_count=%d \n", __func__, bq->fake_dod_count);
	} else {
		/* read count */
		ret = fg_mac_read_block(bq, FG_MAC_CMD_MIXDATA1, t_buf, 32);
		if (ret) {
			fg_err("failed to get FG_MAC_CMD_MIXDATA1\n");
			return;
		}

		count_2v8 = t_buf[9] << 8 | t_buf[8];
		count_3v1 = t_buf[11] << 8 | t_buf[10];
		count_3v3 = t_buf[13] << 8 | t_buf[12];
		bq->dod_count = count_2v8 * 20/10 + count_3v1 * 15/10 + count_3v3 * 13/10;
	}

	/* support dynamic shutdown voltage */
	run_shutdown_temp_state_machine(bq);
	fg_get_count_shutdown_vol(bq, count_vol);
	fg_get_cycle_shutdown_vol(bq, cycle_vol);
	shutdown_vol = max(count_vol[0], cycle_vol[0]);
	shutdown_delay_vol = max(count_vol[1], cycle_vol[1]);
	shutdown_fg_vol = max(count_vol[2], cycle_vol[2]);
	fg_info("%s: count=%d,%d,%d,%d count_vol=%d,%d,%d cycle_vol=%d,%d,%d\n", __func__,
			bq->dod_count, count_2v8, count_3v1, count_3v3,
			count_vol[0], count_vol[1], count_vol[2],
			cycle_vol[0], cycle_vol[1], cycle_vol[2]);

	/* set shutdown_fg_vol */
	if (shutdown_fg_vol != bq->shutdown_fg_vol) {
		data[0] = shutdown_fg_vol & 0xFF;
		data[1] = (shutdown_fg_vol >> 8) & 0xFF;
		ret = fg_mac_write_block(bq, FG_MAC_CMD_MIX_TERM_VOLT, data, 2);
		if (ret) {
			fg_info("%s:failed to write FG_MAC_CMD_MIX_TERM_VOLT\n", __func__);
		}

		ret = fg_mac_read_block(bq, FG_MAC_CMD_MIX_TERM_VOLT, t_buf, 2);
		fg_info("%s:t_buf[0]=0x%02x t_buf[1]=0x%02x FG_MAC_CMD_MIX_TERM_VOLT=%d\n",
				__func__, t_buf[0], t_buf[1], t_buf[1] << 8 | t_buf[0]);
	}

	bq->shutdown_vol = shutdown_vol;
	bq->shutdown_delay_vol = shutdown_delay_vol;
	bq->shutdown_fg_vol = shutdown_fg_vol;

	fg_info("%s: shutdown voltage=%d,%d,%d state=%d,%d cycle=%d count=%d\n", __func__,
				bq->shutdown_vol, bq->shutdown_delay_vol, bq->shutdown_fg_vol,
				bq->tbat, bq->temp_state, bq->cycle_count, bq->dod_count);
}
/**************************support dynamic shutdown voltage end**************************/

static void fg_update_status(struct bq_fg_chip *bq)
{
	int temp_soc = 0;
	static int last_soc = 0;
	ktime_t time_now = -1;
	#if 0
        struct mtk_battery *gm;
	#endif

	mutex_lock(&bq->data_lock);
	bq->cycle_count = fg_read_cyclecount(bq);
	bq->rsoc = fg_read_rsoc(bq);
	bq->soh = fg_read_soh(bq);
	bq->raw_soc = fg_get_raw_soc(bq);
	bq->ibat = fg_read_current(bq);
	bq->tbat = fg_read_temperature(bq);
	bq->i2c_err_flag = (bq->i2c_error_count >= 10) ? true : false;
	fg_read_status(bq);
	bms_get_property(BMS_PROP_ISC_ALERT_LEVEL, &bq->isc_status);
	mutex_unlock(&bq->data_lock);
	fg_info("%s fg_update rsoc=%d, raw_soc=%d, vbat=%d, cycle_count=%d\n", __func__, bq->rsoc, bq->raw_soc, bq->vbat, bq->cycle_count);

	if (!battery_get_psy(bq)) {
		fg_err("%s fg_update failed to get battery psy\n", bq->log_tag);
		bq->ui_soc = bq->rsoc;
		return;
	} else {
		#if 0
                gm = (struct mtk_battery *)power_supply_get_drvdata(bq->batt_psy);
		#endif
		time_now = ktime_get();
		if (time_init != -1 && (time_now - time_init < 10000 ))
		{
			bq->ui_soc = bq->rsoc;
			goto out;
		}
		bq->ui_soc = bq_battery_soc_smooth_tracking_new(bq, bq->raw_soc, bq->rsoc, bq->ibat);
		if(bq->night_charging && bq->ui_soc > 80 && bq->ui_soc > last_soc){
			bq->ui_soc = last_soc;
			fg_err("%s last_soc = %d, night_charging = %d,\n", __func__, last_soc, bq->night_charging);
		}

		goto out;
		temp_soc = bq_battery_soc_smooth_tracking(bq, bq->raw_soc, bq->rsoc, bq->tbat, bq->ibat);
		bq->ui_soc = bq_battery_soc_smooth_tracking_sencond(bq, bq->raw_soc, bq->rsoc, temp_soc);

out:
		fg_info("%s [FG_STATUS] [UISOC RSOC RAWSOC TEMP_SOC SOH] = [%d %d %d %d %d], [VBAT CELL0 CELL1 IBAT TBAT FC FAST_MODE] = [%d %d %d %d %d %d %d]\n", bq->log_tag,
			bq->ui_soc, bq->rsoc, bq->raw_soc, temp_soc, bq->soh, bq->vbat, bq->cell_voltage[0], bq->cell_voltage[1], bq->ibat, bq->tbat, bq->batt_fc, bq->fast_chg);

		last_soc = bq->ui_soc;
		
		if (bq->batt_psy) {
			power_supply_changed(bq->batt_psy);
		}
	}
}

__maybe_unused
static void low_vbat_power_off(struct bq_fg_chip *bq)
{
	static int count = 0;
	int rc = 0;
	union power_supply_propval pval = {0,};

	if (!bq->batt_psy)
		battery_get_psy(bq);
	if (!bq->batt_psy) {
		fg_err("get battery is null\n");
		return;
	}
	rc = power_supply_get_property(bq->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (rc < 0)
		fg_err("get battery volt error\n");
	else
		bq->vbat = pval.intval / 1000;

	fg_info("vbat:%d, shutdown_vol:%d, count:%d\n", bq->vbat, bq->shutdown_vol, count);
	if (bq->vbat < bq->shutdown_vol) {
		if (count < 3) {
			count++;
		} else {
			fg_info("vbat:%d under shutdown_vol:%d, poweroff\n", bq->vbat, bq->shutdown_vol);
			kernel_power_off();
		}
	} else {
		count = 0;
	}
}

static void power_off_check_work(struct bq_fg_chip *bq)
{
	static int count = 0;
	static int ibat_over_count = 0;

	battery_shutdown_vol_update(bq);
	//low_vbat_power_off(bq);
	if (!bq->usb_psy) {
		bq->usb_psy = power_supply_get_by_name("usb");
		if (!bq->usb_psy) {
			fg_err("failed to usb psy\n");
			return;
		}
	}

	if (bq->ui_soc <= 1) {
		if (bq->vbat <= bq->shutdown_delay_vol && bq->vbat >= bq->shutdown_vol &&
				bq->bat_status != POWER_SUPPLY_STATUS_CHARGING) {
			if (count < 2)
				count++;
			else
				bq->shutdown_delay = true;
		} else if (bq->ibat > 1000) {
			ibat_over_count++;
			if (ibat_over_count > 14)
				bq->shutdown_delay = true;
		} else if (bq->bat_status == POWER_SUPPLY_STATUS_CHARGING && bq->shutdown_delay) {
			count = 0;
			ibat_over_count = 0;
			bq->shutdown_delay = false;
		}
	} else {
		count = 0;
		ibat_over_count = 0;
		bq->shutdown_delay = false;
	}
	fg_info("uisoc:%d, tbat:%d, ibat:%d, bat_status:%d, shutdown_delay:%d, last_shutdown_delay:%d, count:%d, ibat_over_count:%d\n",
		bq->ui_soc, bq->tbat, bq->ibat, bq->bat_status, bq->shutdown_delay, bq->last_shutdown_delay, count, ibat_over_count);
	if (bq->last_shutdown_delay != bq->shutdown_delay) {
		bq->last_shutdown_delay = bq->shutdown_delay;
		power_supply_changed(bq->usb_psy);
		power_supply_changed(bq->batt_psy);

		xm_charge_uevent_report(CHG_UEVENT_SHUTDOWN_DELAY, bq->shutdown_delay);
	}
}

static void fg_monitor_workfunc(struct work_struct *work)
{
	struct bq_fg_chip *bq = container_of(work, struct bq_fg_chip, monitor_work.work);

	if (!bq->mtk_charger)
		bq->mtk_charger = get_charger_by_name("mtk_charger");

	fg_update_status(bq);

	power_off_check_work(bq);

	if (bq->ui_soc == 1 || bq->vbat <= 3200)
		schedule_delayed_work(&bq->monitor_work, msecs_to_jiffies(1000));
	else
		schedule_delayed_work(&bq->monitor_work, msecs_to_jiffies(bq->monitor_delay));

	if (bq->bms_wakelock->active)
		__pm_relax(bq->bms_wakelock);
}

static int dod_count_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->fake_dod_count;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int dod_count_set(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int val)
{
	if (bq)
		bq->fake_dod_count = val;
	fg_err("%s %d\n", __func__, val);
	return 0;
}

static int fastcharge_mode_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->fast_chg;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int fastcharge_mode_set(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int val)
{
	if (bq)
		fg_set_fastcharge_mode(bq, !!val);
	fg_err("%s %d\n", __func__, val);
	return 0;
}

static int input_suspend_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	bool value = false;
	int ret = 0;

	ret = charger_dev_input_suspend_get_flag(bq->mtk_charger, &value);
	if (ret)
		fg_err("ERROR:%s fail, ret:%d\n", __func__, ret);
	*val = value;

	return 0;
}

static int input_suspend_set(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int val)
{
	bool value = false;
	int ret = 0;

	value = !!val;
	ret = charger_dev_input_suspend_set_flag(bq->mtk_charger, value);
	if (ret)
		fg_err("ERROR:%s fail, ret:%d\n", __func__, ret);

	return 0;
}

//是否支持该功能，如果不支持UI开关不会进行显示
static int otg_ui_support_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	*val = 1;
	fg_err("[%s] *val: %d\n", __func__, *val);
	return 0;
}

//用户是否可操作该开关，有设备插入时不可操作
static int cid_status_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	bool value = false;
	int ret = 0;

	ret = charger_dev_manual_get_cid_status(bq->mtk_charger, &value);
	if (ret)
		fg_err("ERROR:%s fail, ret:%d\n", __func__, ret);

	*val = value;
	fg_err("[%s] *val: %d\n", __func__, *val);

	return 0;
}

//UI开关状态：打开 or 关闭
static int cc_toggle_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	bool value = false;
	int ret = 0;

	ret = charger_dev_manual_get_cc_toggle(bq->mtk_charger, &value);
	if (ret)
		fg_err("ERROR:%s fail, ret:%d\n", __func__, ret);
	*val = value;	
	fg_err("[%s] *val: %d\n", __func__, *val);

	return 0;
}

static int cc_toggle_set(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int val)
{
	bool value = false;
	int ret = 0;

	value = !!val;
	bq->control_cc_toggle = value;
	ret = charger_dev_manual_set_cc_toggle(bq->mtk_charger, value);
	if (ret)
		fg_err("ERROR:%s fail, ret:%d\n", __func__, ret);
	fg_err("[%s] *val: %d\n", __func__, bq->control_cc_toggle);

	return 0;
}

#if IS_ENABLED(CONFIG_RUST_DETECTION)
static int moisture_detection_en_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	*val = 1;
	fg_err("[%s] lpd_enable: %d\n", __func__, *val);

	return 0;
}

static int moisture_detection_status_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	struct mtk_charger *mtk_chg_info = NULL;

	mtk_chg_info = get_mtk_charger_info();
	if (!mtk_chg_info)
		return 0;
	*val = mtk_chg_info->lpd_flag;
	fg_err("[%s] lpd_status: %d\n", __func__, *val);

	return 0;
}

static int lpd_charging_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	struct mtk_charger *mtk_chg_info = NULL;

	mtk_chg_info = get_mtk_charger_info();
	if (!mtk_chg_info)
		return 0;
	*val = mtk_chg_info->lpd_charging_limit;
	fg_err("[%s] lpd_status: %d\n", __func__, *val);

	return 0;
}

static int lpd_charging_set(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int val)
{
	struct mtk_charger *mtk_chg_info = NULL;

	mtk_chg_info = get_mtk_charger_info();
	if (!mtk_chg_info)
		return 0;
	mtk_chg_info->lpd_charging_limit = val;
	fg_err("[%s] lpd_charging_limit: %d\n", __func__, val);

	return 0;
}
#endif

static int reverse_quick_charge_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	bool value = false;
	int ret = 0;

	ret = charger_dev_reverse_quick_charge_get_flag(bq->mtk_charger, &value);
	if (ret)
		fg_err("ERROR:%s fail, ret:%d\n", __func__, ret);
	*val = value;
	fg_err("[%s] *val: %d\n", __func__, *val);

	return 0;
}

static int reverse_quick_charge_set(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int val)
{
	bool value = false;
	int ret = 0;

	value = !!val;
	ret = charger_dev_reverse_quick_charge_set_flag(bq->mtk_charger, value);
	if (ret)
		fg_err("ERROR:%s fail, ret:%d\n", __func__, ret);
	fg_err("[%s] [REVCHG] reverse_quick_charge_set_flag\n", __func__);

	return 0;
}

static int revchg_bcl_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	bool value = false;
	int ret = 0;

	ret = charger_dev_revchg_bcl_get_flag(bq->mtk_charger, &value);
	if (ret)
		fg_err("ERROR:%s fail, ret:%d\n", __func__, ret);
	*val = value;
	fg_err("[%s] *val: %d\n", __func__, *val);

	return 0;
}

static int revchg_bcl_set(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int val)
{
	bool value = false;
	int ret = 0;

	value = !!val;
	ret = charger_dev_revchg_bcl_set_flag(bq->mtk_charger, value);
	if (ret)
		fg_err("ERROR:%s fail, ret:%d\n", __func__, ret);
	fg_err("[%s] [REVCHG]revchg_bcl_set_flag\n", __func__);

	return 0;
}

static int shipmode_count_reset_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	struct mtk_charger *mtk_chg_info = NULL;

	mtk_chg_info = get_mtk_charger_info();
	if (mtk_chg_info == NULL) {
		*val = 0;
		fg_info("failed get mtk charger info\n");
		return -EINVAL;
	}

	*val = mtk_chg_info->shipmode_flag;

	fg_err("[%s] *val: %d\n", __func__, *val);
	return 0;
}

static int shipmode_count_reset_set(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int val)
{
	struct mtk_charger *mtk_chg_info = NULL;

	mtk_chg_info = get_mtk_charger_info();
	if (mtk_chg_info == NULL) {
		fg_info("failed get mtk charger info\n");
		return -EINVAL;
	}

	mtk_chg_info->shipmode_flag = !!val;

	fg_err("[%s] shipmode_set_flag\n", __func__);

	return 0;
}

static int monitor_delay_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->monitor_delay;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int monitor_delay_set(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int val)
{
	if (bq)
		bq->monitor_delay = val;
	fg_err("%s %d\n", __func__, val);
	return 0;
}

static int fcc_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->fcc;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int rm_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->rm;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int rsoc_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->rsoc;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int shutdown_delay_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->shutdown_delay;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int shutdown_delay_set(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int val)
{
	if (bq)
		bq->fake_shutdown_delay_enable = val;
	fg_err("%s %d\n", __func__, val);
	return 0;
}

static int capacity_raw_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->raw_soc;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int soc_decimal_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = fg_get_soc_decimal(bq);
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int av_current_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = fg_read_avg_current(bq);
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int soc_decimal_rate_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = fg_get_soc_decimal_rate(bq);
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int resistance_id_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = 100000;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int resistance_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = 100000;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int authentic_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->authenticate;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int authentic_set(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int val)
{
	if (bq) {
		bq->authenticate = !!val;
		bq->batt_auth_done = true;
	}
	fg_err("%s %d\n", __func__, val);

	return 0;
}

static int shutdown_mode_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->authenticate;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int shutdown_mode_set(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int val)
{
	if (bq)
		fg_set_shutdown_mode(bq);
	fg_err("%s %d\n", __func__, val);
	return 0;
}

static int chip_ok_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->chip_ok;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int batt_id_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->cell_supplier;
	else
		*val = 0;

	return 0;
}

static int fg_vendor_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->device_name;
	else
		*val = 0;

	return 0;
}

static int charge_done_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->batt_fc;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int soh_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->soh;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int bms_slave_connect_error_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	*val = gpio_get_value(bq->slave_connect_gpio);
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int cell_supplier_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->cell_supplier;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int i2c_error_count_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if(bq && bq->fake_i2c_error_count > 0)
	{
		*val = bq->fake_i2c_error_count;
		return 0;
	}
	if (bq)
		*val = bq->i2c_error_count;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int i2c_error_count_set(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int val)
{
	if (bq)
		bq->fake_i2c_error_count = val;
	fg_err("%s %d\n", __func__, val);
	return 0;
}

static ssize_t bms_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy;
	struct bq_fg_chip *bq;
	struct bq_bms_sysfs_field_info *usb_attr;
	int val;
	ssize_t ret;

	ret = kstrtos32(buf, 0, &val);
	if (ret < 0)
		return ret;

	psy = dev_get_drvdata(dev);
	bq = (struct bq_fg_chip *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct bq_bms_sysfs_field_info, attr);
	if (usb_attr->set != NULL)
		usb_attr->set(bq, usb_attr, val);

	return count;
}

static ssize_t bms_sysfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct bq_fg_chip *bq;
	struct bq_bms_sysfs_field_info *usb_attr;
	int val = 0;
	int val_attr[32] = {0};
	ssize_t count = 0;
	int pos = 0;
	int remaining;

	psy = dev_get_drvdata(dev);
	bq = (struct bq_fg_chip *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct bq_bms_sysfs_field_info, attr);

	if (usb_attr->get != NULL) {
		if (usb_attr->prop == BMS_PROP_BAT_USE_ENVIRONMENT) {
			usb_attr->get(bq, usb_attr, val_attr);
			remaining = PAGE_SIZE;
			for (int i = 0; i < 32; i++) {
				const char *format = (i == 31) ? "%d\n" : "%d ";
				int written;

				written = scnprintf(buf + pos, remaining, format, val_attr[i]);
				if (written < 0) {
					break;
				}

				if (written >= remaining) {
					pos += remaining - 1;
					break;
				}

				pos += written;
				remaining -= written;
			}
			count = pos;
		} else {
			usb_attr->get(bq, usb_attr, &val);
			count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
		}
	}

	return count;
}

static int temp_max_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	char data_limetime1[32];
	int ret = 0;

	memset(data_limetime1, 0, sizeof(data_limetime1));

	ret = fg_mac_read_block(bq, FG_MAC_CMD_LIFETIME1, data_limetime1, sizeof(data_limetime1));
	if (ret)
		fg_err("failed to get FG_MAC_CMD_LIFETIME1\n");
	*val = data_limetime1[6];

	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int time_ot_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	char data_limetime3[32];
	char data[32];
	int ret = 0;

	memset(data_limetime3, 0, sizeof(data_limetime3));
	memset(data, 0, sizeof(data));

	ret = fg_mac_read_block(bq, FG_MAC_CMD_LIFETIME3, data_limetime3, sizeof(data_limetime3));
	if (ret)
		fg_err("failed to get FG_MAC_CMD_LIFETIME3\n");

	ret = fg_mac_read_block(bq, FG_MAC_CMD_MANU_NAME, data, sizeof(data));
	if (ret)
		fg_err("failed to get FG_MAC_CMD_MANU_NAME\n");

	if (data[2] == 'C') //TI
	{
		ret = fg_mac_read_block(bq, FG_MAC_CMD_FW_VER, data, sizeof(data));
		if (ret)
			fg_err("failed to get FG_MAC_CMD_FW_VER\n");

		if ((data[3] == 0x0) && (data[4] == 0x1)) //R0 FW
			*val = ((data_limetime3[15] << 8) | (data_limetime3[14] << 0)) << 2;
		else if ((data[3] == 0x1) && (data[4] == 0x2)) //R1 FW
			*val = ((data_limetime3[9] << 8) | (data_limetime3[8] << 0)) << 2;
	}
	else if (data[2] == '4') //NVT
		*val = (data_limetime3[15] << 8) | (data_limetime3[14] << 0);

	fg_err("%s %d\n", __func__, *val);
	return 0;
}

int isc_alert_level_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u8 isc_alert_level = 0;

	if(bq->device_name != BQ_FG_NFG1000A && bq->device_name != BQ_FG_NFG1000B && bq->device_name != MPC_FG_MPC8011B)
	{
		fg_err("%s: this Bq_Fg is not support this function.\n", __func__);
		return -1;
	}

	ret = fg_read_byte(bq, bq->regs[NVT_FG_REG_ISC], &isc_alert_level);

	if(ret < 0)
	{
		fg_err("%s: read isc_alert_level occur error.\n", __func__);
		return ret;
	}
	*val = isc_alert_level;
	fg_err("%s:now isc:%d.\n", __func__, *val);
	return ret;
}

int soa_alert_level_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u8 soa_alert_level = 0;

	if(bq->device_name != BQ_FG_NFG1000A && bq->device_name != BQ_FG_NFG1000B)
	{
		fg_err("%s: this Bq_Fg is not support this function.\n", __func__);
		return -1;
	}

	ret = fg_read_byte(bq,bq->regs[NVT_FG_REG_SOA_L], &soa_alert_level);

	if(ret < 0)
	{
		fg_err("%s: read soa_alert_level occur error.\n", __func__);
		return ret;
	}
	*val = soa_alert_level;
	fg_err("%s:now soa:%d.\n", __func__, *val);
	return ret;
}

int charging_state_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u16 charging_state = 0;

	ret = fg_read_word(bq, FG_MAC_CMD_CHARGING_STATUS, &charging_state);
		if (ret < 0)
			fg_err("%s failed to read MAC\n", __func__);

	*val = !!(charging_state & BIT(3));
	fg_err("%s:charging_state:%d.\n", __func__, *val);
	return ret;
}

static bool is_all_digits(const char *str) {
    while (*str) {
        if (!isdigit((unsigned char)*str)) {
            return false;
        }
        str++;
    }
    return true;
}

#define DEFAULT_FIRST_USAGE_DATE "00000000"
#define ERROE_FIRST_USAGE_DATE "99999999"
#define DEST_INIT_STRING "000000"
static ssize_t first_usage_date_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct bq_fg_chip *bq;
	psy = dev_get_drvdata(dev);
	bq = (struct bq_fg_chip *)power_supply_get_drvdata(psy);
	char first_usage_date[9] = {'0','0','0','0','0','0','0','0','\0'};
	u8 t_buf[64] = { 0 };
	int ret = 0;

	//memset(buf, 0, sizeof(buf));
	fg_info("first_usage_date_show\n");
	ret = fg_mac_read_block(bq, FG_MAC_CMD_FIRST_USAGE_DATE, t_buf, 32);
	if (ret < 0) {// read error, show 9999999
		strcpy(first_usage_date, ERROE_FIRST_USAGE_DATE);
		fg_err("read failed %s\n", first_usage_date);
		goto out;
	}

	first_usage_date[0] = '2'; /* Fixed header */
	first_usage_date[1] = '0'; /* Fixed header */
	first_usage_date[2] = '0' + t_buf[11] / 10; /* Second last digit of year */
	first_usage_date[3] = '0' + t_buf[11] % 10; /* Last digit of year */
	first_usage_date[4] = '0' + t_buf[12] / 10; /* First digit of month */
	first_usage_date[5] = '0' + t_buf[12] % 10; /* Second digit of month */
	first_usage_date[6] = '0' + t_buf[13] / 10; /* First digit of day */
	first_usage_date[7] = '0' + t_buf[13] % 10; /* Second digit of day */
	first_usage_date[8] = '\0';

	fg_err("first_usage_date read t_buf[11] %d t_buf[12] %d t_buf[13] %d\n", t_buf[11], t_buf[12], t_buf[13]);

	if (!strncmp(&first_usage_date[2], DEST_INIT_STRING, strlen(DEST_INIT_STRING))) {
		strcpy(first_usage_date, DEFAULT_FIRST_USAGE_DATE);
		fg_err(" usage_date init %s\n", first_usage_date);
		goto out;
	}

	if (!is_all_digits(first_usage_date)) {
		fg_err("usage data has invalid char [%s]\n", first_usage_date);
		//strcpy(buf, ERROE_FIRST_USAGE_DATE);
		goto out;
	}

	fg_err("read first_usage_date = %s\n", first_usage_date);

out:
	return sprintf(buf, "%s\n", first_usage_date);
}

static void remove_whitespace(const char *src, char *dest) {
	int j = 0;

	while (*src != '\0') {
		if (!isspace((unsigned char)*src)) {
			dest[j++] = *src;
		}
		src++;
	}
	dest[j] = '\0';
	return;
}

#define RETRY_UPDATE_COUNT 3
static ssize_t first_usage_date_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy;
	struct bq_fg_chip *bq;
	psy = dev_get_drvdata(dev);
	bq = (struct bq_fg_chip *)power_supply_get_drvdata(psy);
	u8 data[32] = {0};
	u8 data_temp[32] = {0};
	int retry = 0;
	int ret = 0;
	bool check_status = false;
	char *tmp_buf = NULL;

	tmp_buf = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp_buf)
		return -EINVAL;

	remove_whitespace(buf, tmp_buf);

	if (!is_all_digits(tmp_buf) || !count) {
		fg_err("input is not all digits! errorstr = %s count = [%zu]\n", tmp_buf, count);
		kfree(tmp_buf);
		return -EINVAL;
	}

	ret = fg_mac_read_block(bq, FG_MAC_CMD_FIRST_USAGE_DATE, data, 32);
	if (ret < 0) {
		fg_err("failed to get first_usage\n");
		kfree(tmp_buf);
		return -EINVAL;
	}

	fg_err("data[11] %d  data[12] %d data[13] %d\n", data[11], data[12], data[13]);

	if ((data[11] == 0x00 && data[12] == 0x00 && data[13] == 0x00) ||
		(data[11] == 0xFF && data[12] == 0xFF && data[13] == 0xFF)) {
		fg_err(" init status must update\n");
	} else {
		fg_err("first_usage_date has been updated [%d][%d][%d]\n",
			data[11], data[12], data[13]);
	}
	fg_err("tmp_buf[2] - [7] %c %c %c %c %c %c \n", tmp_buf[2], tmp_buf[3], tmp_buf[4], tmp_buf[5], tmp_buf[6], tmp_buf[7]);
	/*example,20241220,2024(0x34)1(0x31)2(0x32)2(0x32)0(0x30)*/
	data[11] = (tmp_buf[2] - '0') * 10 + (tmp_buf[3] - '0');  /* Last 2 digits of year (YY) */
	data[12] = (tmp_buf[4] - '0') * 10 + (tmp_buf[5] - '0');  /* Month (1-12) */
	data[13] = (tmp_buf[6] - '0') * 10 + (tmp_buf[7] - '0');  /* Day of month (1-31) */

	while (retry++ < RETRY_UPDATE_COUNT) {
		ret = fg_mac_write_block(bq, FG_MAC_CMD_FIRST_USAGE_DATE, data, 32);
		if (ret < 0) {
			msleep(100);
			continue;
		}

		msleep(100);
		memset(data_temp, 0, sizeof(data_temp));
		ret = fg_mac_read_block(bq, FG_MAC_CMD_FIRST_USAGE_DATE, data_temp, 32);
		if (ret < 0) {
			fg_err("failed to read first_usage_date\n");
			continue;
		}

		fg_err("data[11] %d  data[12] %d data[13] %d\n", data[11], data[12], data[13]);

		if (data[11] == data_temp[11] &&
			data[12] == data_temp[12] &&
			data[13] == data_temp[13]) {
			fg_err("write first_usage_date success\n");
			check_status = true;
			break;
		} else {
			fg_err("write fail Updata[%d][%d][%d] = Reg[%d][%d][%d]\n",
				data[11],data[12], data[13], data_temp[11], data_temp[12], data_temp[13]);
			continue;
		}
	}

	if (!check_status) {
		data[11] = 00;
		data[12] = 00;
		data[13] = 00;
		retry = 0;

		while (retry++ < RETRY_UPDATE_COUNT) {
			ret = fg_mac_write_block(bq, FG_MAC_CMD_FIRST_USAGE_DATE, data, sizeof(data));
			if (ret < 0) {
				msleep(100);
				continue;
			}

			msleep(100);
			memset(data_temp, 0, sizeof(data_temp));
			ret = fg_mac_read_block(bq, FG_MAC_CMD_FIRST_USAGE_DATE, data_temp, sizeof(data_temp));
			if (!data_temp[11] && !data_temp[12] && !data_temp[13]) {
				fg_err("reset success\n");
				check_status = true;
				break;
			} else {
				fg_err("reset fail Reg[%d][%d][%d]\n",
				data_temp[11], data_temp[12], data_temp[13]);
				continue;
			}
		}

	}

	kfree(tmp_buf);

	return count;
}

static struct device_attribute first_usage_date_attr =
	__ATTR(first_usage_date, 0644, first_usage_date_show, first_usage_date_store);


static ssize_t manufacturing_date_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct bq_fg_chip *bq;
	psy = dev_get_drvdata(dev);
	bq = (struct bq_fg_chip *)power_supply_get_drvdata(psy);
	u8 t_buf[64] = {0};
	int ret, len;
	char manufacturing_date[9] = {'2', '0', '2', '0', '0', '0', '0', '0', '\0'};
	fg_err("manufacturing_date_show\n");
	ret = fg_mac_read_block(bq, FG_MAC_CMD_BATT_SN, t_buf, 32);
	if (ret < 0) {
		fg_err("failed to get BATT_SN\n");
		return ret;
	}
	fg_err("read manu t_buf %s\n",t_buf);
	manufacturing_date[3] = '0' + (t_buf[6] & 0xF);
	manufacturing_date[6] = '0' + (t_buf[8] & 0xF);
	manufacturing_date[7] = '0' + (t_buf[9] & 0xF);

	switch (t_buf[7]) {
	case 10:
		manufacturing_date[4] = '1';
		manufacturing_date[5] = '0';
		break;
	case 11:
		manufacturing_date[4] = '1';
		manufacturing_date[5] = '1';
		break;
	case 12:
		manufacturing_date[4] = '1';
		manufacturing_date[5] = '2';
		break;
	default:
		manufacturing_date[5] = '0' + (t_buf[7] & 0xF);
		break;
	}
	len = ARRAY_SIZE(manufacturing_date);

	fg_err("read manufacturing_date=%c,%c,%c,%c,%c,len=%d\n",
	manufacturing_date[3], manufacturing_date[4], manufacturing_date[5], manufacturing_date[6], manufacturing_date[7], len);

	fg_err("read manu %s\n",manufacturing_date);

	return sprintf(buf, "%s\n", manufacturing_date);
}

static struct device_attribute manufacturing_date_attr =
	__ATTR(manufacturing_date, 0444, manufacturing_date_show, NULL);


static ssize_t fg1_cycle_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct bq_fg_chip *bq;
	psy = dev_get_drvdata(dev);
	bq = (struct bq_fg_chip *)power_supply_get_drvdata(psy);

	bq->cycle_count = fg_read_cyclecount(bq);
	fg_err("fg1_cycle = %d\n",bq->cycle_count);

	return sprintf(buf, "%d\n", bq->cycle_count);
}
static struct device_attribute fg1_cycle_attr =
	__ATTR(fg1_cycle, 0444, fg1_cycle_show, NULL);

static ssize_t soh_sn_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct bq_fg_chip *bq;
	psy = dev_get_drvdata(dev);
	bq = (struct bq_fg_chip *)power_supply_get_drvdata(psy);
	u8 t_buf[64] = {0};
	int ret;
	fg_err("soh_sn_show\n");
	ret = fg_mac_read_block(bq, FG_MAC_CMD_BATT_SN, t_buf, 32);
	if (ret < 0) {
		fg_err("failed to get BATT_SN\n");
		return sprintf(buf, "unknown\n");
	}

	return sprintf(buf, "%s\n", t_buf);
}

static struct device_attribute soh_sn_attr =
	__ATTR(soh_sn, 0444, soh_sn_show, NULL);


static inline unsigned long fg_convert_u8_to_u16(unsigned char lsb, unsigned char hsb)
{
	return ((hsb << 8) | lsb);
}

static ssize_t calc_rvalue_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct bq_fg_chip *bq;
	psy = dev_get_drvdata(dev);
	bq = (struct bq_fg_chip *)power_supply_get_drvdata(psy);

	int ret = 0 ;
	unsigned long k1 = 45;
	unsigned long k2 = 45;
	unsigned long k3 = 70;
	unsigned long k4 = 45;
	unsigned long k5 = 100;
	unsigned long k6 = 70;
	unsigned long k7 = 45;
	unsigned long CCcc, FFff, IIii, JJjj, MMmm, NNnn, OOoo;
	unsigned char data[32] = { 0 };
	unsigned long rvalue;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_CALC_RVALUE, data, 32);
	fg_err("calc_rvalue %s", data);
	if (ret) {
		fg_err("read calc_rvalue error retry\n");
		mdelay(150);
		ret = fg_mac_read_block(bq, FG_MAC_CMD_CALC_RVALUE, data, 32);
		if (ret) {
			fg_err("read calc_rvalue error done\n");
			return -EINVAL;
		}
	}

	CCcc = fg_convert_u8_to_u16(data[4], data[5]);
	FFff = fg_convert_u8_to_u16(data[10], data[11]);
	IIii = fg_convert_u8_to_u16(data[16], data[17]);
	JJjj = fg_convert_u8_to_u16(data[18], data[19]);
	MMmm = fg_convert_u8_to_u16(data[24], data[25]);
	NNnn = fg_convert_u8_to_u16(data[26], data[27]);
	OOoo = fg_convert_u8_to_u16(data[28], data[29]);

	rvalue = (CCcc * k1 + FFff * k2 + IIii * k3 + JJjj * k4 +
		MMmm * k5 + NNnn * k6 + OOoo * k7) / 10;
	fg_err("calr_rvalue=%lu\n", rvalue);

	return sprintf(buf, "%lu\n", rvalue);
}

static struct device_attribute calc_rvalue_attr =
	__ATTR(calc_rvalue, 0444, calc_rvalue_show, NULL);

static ssize_t batt_sn_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct bq_fg_chip *bq;
	psy = dev_get_drvdata(dev);
	bq = (struct bq_fg_chip *)power_supply_get_drvdata(psy);
	u8 t_buf[64] = {0};
	int ret;

	fg_err("batt_sn_show\n");
	ret = fg_mac_read_block(bq, FG_MAC_CMD_BATT_SN, t_buf, 32);
	if (ret < 0) {
		fg_err("failed to get BATT_SN\n");
		return sprintf(buf, "unknown\n");
	}

	return sprintf(buf, "%s\n", t_buf);
}

static struct device_attribute batt_sn_attr =
	__ATTR(batt_sn, 0444, batt_sn_show, NULL);


//charger partition rw node
static int charger_partition_test_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	charger_partition_info_1 *info_1 = NULL;

	ret = charger_partition_alloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		fg_err("[charger] %s failed to alloc\n", __func__);
		return -1;
	}

	info_1 = (charger_partition_info_1 *)charger_partition_read(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(!info_1) {
		fg_err("[charger] %s failed to read\n", __func__);
		ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
		if(ret < 0) {
			fg_err("[charger] %s failed to dealloc\n", __func__);
			return -1;
		}
		return -1;
	}
	fg_err("[charger] %s ret: %d, info_1->test: %u\n", __func__, ret, info_1->test);
	*val = info_1->test;

	ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		fg_err("[charger] %s failed to dealloc\n", __func__);
		return -1;
	}
	return 0;
}

static int charger_partition_test_set(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int val)
{
	int ret = 0;
	charger_partition_info_1 info_1 = {.power_off_mode = 2, .zero_speed_mode = 2, .test = 2, .reserved = 0};

	ret = charger_partition_alloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		fg_err("[charger] %s failed to alloc\n", __func__);
		return -1;
	}

	info_1.test = val;
	ret = charger_partition_write(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, (void *)&info_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		fg_err("[charger] %s failed to write\n", __func__);
		ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
		if(ret < 0) {
			fg_err("[charger] %s failed to dealloc\n", __func__);
			return -1;
		}
		return -1;
	}
	fg_err("[charger] %s ret: %d, info_1.test: %u\n", __func__, ret, info_1.test);

	ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		fg_err("[charger] %s failed to dealloc\n", __func__);
		return -1;
	}
	return 0;
}

static int charger_partition_poweroffmode_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	charger_partition_info_1 *info_1 = NULL;

	ret = charger_partition_alloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		fg_err("[charger] %s failed to alloc\n", __func__);
		return -1;
	}

	info_1 = (charger_partition_info_1 *)charger_partition_read(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(!info_1) {
		fg_err("[charger] %s failed to read\n", __func__);
		ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
		if(ret < 0) {
			fg_err("[charger] %s failed to dealloc\n", __func__);
			return -1;
		}
		return -1;
	}
	fg_err("[charger] %s ret: %d, info_1->power_off_mode: %u\n", __func__, ret, info_1->power_off_mode);
	*val = info_1->power_off_mode;

	ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		fg_err("[charger] %s failed to dealloc\n", __func__);
		return -1;
	}
	return 0;
}

static int charger_partition_poweroffmode_set(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int val)
{
	int ret = 0;
	charger_partition_info_1 info_1 = {.power_off_mode = 2, .zero_speed_mode = 2, .test = 0x34567890, .reserved = 0};

	ret = charger_partition_alloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		fg_err("[charger] %s failed to alloc\n", __func__);
		return -1;
	}

	info_1.power_off_mode = val;
	ret = charger_partition_write(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, (void *)&info_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		fg_err("[charger] %s failed to write\n", __func__);
		ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
		if(ret < 0) {
			fg_err("[charger] %s failed to dealloc\n", __func__);
			return -1;
		}
		return -1;
	}
	fg_err("[charger] %s ret: %d, info_1.power_off_mode: %u, info_1.test: 0x%0x\n", __func__, ret, info_1.power_off_mode, info_1.test);

	ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
	if(ret < 0) {
		fg_err("[charger] %s failed to dealloc\n", __func__);
		return -1;
	}
	return 0;
}

static int count_level1_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u8 data[64] = {0};

	ret = fg_mac_read_block(bq, FG_MAC_CMD_MIXDATA1, data, 32);

	if(ret < 0)
	{
		fg_err("%s: read count_level1_get  error.\n", __func__);
		return -EINVAL;
	}

	*val = (data[9] << 8) | (data[8] );
	fg_err("%s:now count level1:%d.\n", __func__, *val);
	return ret;
}

static int count_level2_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u8 data[64] = {0};

	ret = fg_mac_read_block(bq, FG_MAC_CMD_MIXDATA1, data, 32);

	if(ret < 0)
	{
		fg_err("%s: read count_level2_get  error.\n", __func__);
		return -EINVAL;
	}

	*val = (data[11] << 8) | (data[10] );
	fg_err("%s:now count level2:%d.\n", __func__, *val);
	return ret;
}

static int count_level3_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u8 data[64] = {0};

	ret = fg_mac_read_block(bq, FG_MAC_CMD_MIXDATA1, data, 32);

	if(ret < 0)
	{
		fg_err("%s: read count_level3_get  error.\n", __func__);
		return -EINVAL;
	}

	*val = (data[13] << 8) | (data[12] );
	fg_err("%s:now count level3:%d.\n", __func__, *val);
	return ret;
}

static int over_vol_duration_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u8 data[64] = {0};

	ret = fg_mac_read_block(bq, FG_MAC_CMD_OVER_VOL_DURATION, data, 32);

	if(ret < 0)
	{
		fg_err("%s: read over_vol_duration_get  error.\n", __func__);
		return -EINVAL;
	}

	*val = (data[11] << 24) | (data[10] << 16) | (data[9] << 8) | (data[8]);
	fg_err("%s:now over_vol_duration_get:%d.\n", __func__, *val);
	return ret;
}

static int min_life_temp_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u8 data[64] = {0};

	ret = fg_mac_read_block(bq, FG_MAC_CMD_LIFETIME1, data, 32);

	if(ret < 0)
	{
		fg_err("%s: read min_life_temp  error.\n", __func__);
		return -EINVAL;
	}

	*val = data[7];
	fg_err("%s:now min_life_temp:%d.\n", __func__, *val);
	return ret;
}

static int max_life_temp_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u8 data[64] = {0};

	ret = fg_mac_read_block(bq, FG_MAC_CMD_LIFETIME1, data, 32);

	if(ret < 0)
	{
		fg_err("%s: read max_life_temp  error.\n", __func__);
		return -EINVAL;
	}

	*val = data[6];
	fg_err("%s:now max_life_temp:%d.\n", __func__, *val);
	return ret;
}

static int min_life_vol_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u8 data[64] = {0};

	ret = fg_mac_read_block(bq, FG_MAC_CMD_LIFETIME3, data, 32);

	if(ret < 0)
	{
		fg_err("%s: read min_life_vol  error.\n", __func__);
		return -EINVAL;
	}

	*val = (data[19] << 8) | (data[18] );
	fg_err("%s:now min_life_vol:%d.\n", __func__, *val);
	return ret;
}

static int max_life_vol_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u8 data[64] = {0};

	ret = fg_mac_read_block(bq, FG_MAC_CMD_LIFETIME1, data, 32);

	if(ret < 0)
	{
		fg_err("%s: read max_life_vol_get  error.\n", __func__);
		return -EINVAL;
	}

	*val = (data[1] << 8) | (data[0] );
	fg_err("%s:now max_life_vol_get:%d.\n", __func__, *val);
	return ret;
}

static int rel_soh_cyclecount_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u8 data[64] = {0};

	ret = fg_mac_read_block(bq, FG_MAC_CMD_SOH_CALIBRATION_VAL, data, 32);

	if(ret < 0)
	{
		fg_err("%s: read rel_soh_cyclecount_get  error.\n", __func__);
		return -EINVAL;
	}

	*val = (data[3] << 8) | (data[2] );
	fg_err("%s:now rel_soh_cyclecount_get:%d.\n", __func__, *val);
	return ret;
}

static int rel_soh_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u8 data[64] = {0};

	ret = fg_mac_read_block(bq, FG_MAC_CMD_SOH_CALIBRATION_VAL, data, 32);

	if(ret < 0)
	{
		fg_err("%s: read rel_soh_get  error.\n", __func__);
		return -EINVAL;
	}

	*val = (data[1] << 8) | (data[0] );
	fg_err("%s:now rel_soh_get:%d.\n", __func__, *val);
	return ret;
}

static int qmax_cyclecount_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u8 data[64] = {0};

	ret = fg_mac_read_block(bq, FG_MAC_CMD_DASTATUS1, data, 32);

	if(ret < 0)
	{
		fg_err("%s: read qmax_cyclecount_get  error.\n", __func__);
		return -EINVAL;
	}

	*val = (data[11] << 8) | (data[10] );
	fg_err("%s:now qmax_cyclecount_get:%d.\n", __func__, *val);
	return ret;
}

static int qmax_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u8 data[64] = {0};

	ret = fg_mac_read_block(bq, FG_MAC_CMD_DASTATUS1, data, 32);

	if(ret < 0)
	{
		fg_err("%s: read qmax_get  error.\n", __func__);
		return -EINVAL;
	}

	*val = (data[5] << 8) | (data[4] );
	fg_err("%s:now qmax_get:%d.\n", __func__, *val);
	return ret;
}

static int eis_soh_cyclecount_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u8 data[64] = {0};

	ret = fg_mac_read_block(bq, FG_MAC_CMD_EIS, data, 32);

	if(ret < 0)
	{
		fg_err("%s: read eis_soh_cyclecount_get  error.\n", __func__);
		return -EINVAL;
	}

	*val = (data[3] << 8) | (data[2] );
	fg_err("%s:now eis_soh_cyclecount_get:%d.\n", __func__, *val);
	return ret;
}

static int eis_soh_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u8 data[64] = {0};

	ret = fg_mac_read_block(bq, FG_MAC_CMD_EIS, data, 32);

	if(ret < 0)
	{
		fg_err("%s: read eis_soh_get  error.\n", __func__);
		return -EINVAL;
	}

	*val = (data[1] << 8) | (data[0] );
	fg_err("%s:now eis_soh_get:%d.\n", __func__, *val);
	return ret;
}

static int batt_use_environment_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u8 data_rvalue[64] = {0};
	u8 data_over_vol[64] = {0};
	int i;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_CALC_RVALUE, data_rvalue, 32);
	if (ret < 0) {
		fg_err("%s: Read FG_MAC_CMD_CALC_RVALUE (0x0066) failed, ret=%d\n", __func__, ret);
		return ret;
	}

	ret = fg_mac_read_block(bq, FG_MAC_CMD_OVER_VOL_DURATION, data_over_vol, 32);
	if (ret < 0) {
		fg_err("%s: Read FG_MAC_CMD_OVER_VOL_DURATION (0x0067) failed, ret=%d\n", __func__, ret);
		return ret;
	}

	for (i = 0; i < 16; i++) {
		val[i] = (data_rvalue[2 * i + 1] << 8) | data_rvalue[2 * i];
	}

	for (i = 0; i < 3; i++) {
		val[16 + i] = (data_over_vol[2 * i + 1] << 8) | data_over_vol[2 * i];
	}

	fg_info("%s: Successfully read t1~t19. t1=%d, t16=%d, t19=%d\n",
			__func__, val[0], val[15], val[18]);
	return 0;
}

static int charge_absoc_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	*val = bq->rsoc;
	fg_err("%s:now charge_absoc_get:%d.\n", __func__, *val);

	return 0;
}

static int soh_new_get(struct bq_fg_chip *bq,
	struct bq_bms_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->ui_soh;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);

	return 0;
}

/* Must be in the same order as BMS_PROP_* */
static struct bq_bms_sysfs_field_info bms_sysfs_field_tbl[] = {
	BMS_SYSFS_FIELD_RW(fastcharge_mode, BMS_PROP_FASTCHARGE_MODE),
	BMS_SYSFS_FIELD_RW(monitor_delay, BMS_PROP_MONITOR_DELAY),
	BMS_SYSFS_FIELD_RO(fcc, BMS_PROP_FCC),
	BMS_SYSFS_FIELD_RO(rm, BMS_PROP_RM),
	BMS_SYSFS_FIELD_RO(rsoc, BMS_PROP_RSOC),
	BMS_SYSFS_FIELD_RW(shutdown_delay, BMS_PROP_SHUTDOWN_DELAY),
	BMS_SYSFS_FIELD_RO(capacity_raw, BMS_PROP_CAPACITY_RAW),
	BMS_SYSFS_FIELD_RO(soc_decimal, BMS_PROP_SOC_DECIMAL),
	BMS_SYSFS_FIELD_RO(soc_decimal_rate, BMS_PROP_SOC_DECIMAL_RATE),
	BMS_SYSFS_FIELD_RO(resistance_id, BMS_PROP_RESISTANCE_ID),
	BMS_SYSFS_FIELD_RW(authentic, BMS_PROP_AUTHENTIC),
	BMS_SYSFS_FIELD_RW(shutdown_mode, BMS_PROP_SHUTDOWN_MODE),
	BMS_SYSFS_FIELD_RO(chip_ok, BMS_PROP_CHIP_OK),
	BMS_SYSFS_FIELD_RO(charge_done, BMS_PROP_CHARGE_DONE),
	BMS_SYSFS_FIELD_RO(soh, BMS_PROP_SOH),
	BMS_SYSFS_FIELD_RO(resistance, BMS_PROP_RESISTANCE),
	BMS_SYSFS_FIELD_RW(i2c_error_count, BMS_PROP_I2C_ERROR_COUNT),
	BMS_SYSFS_FIELD_RO(av_current, BMS_PROP_AV_CURRENT),
	BMS_SYSFS_FIELD_RO(temp_max, BMS_PROP_TEMP_MAX),
	BMS_SYSFS_FIELD_RO(time_ot, BMS_PROP_TIME_OT),
	BMS_SYSFS_FIELD_RO(bms_slave_connect_error, BMS_PROP_BMS_SLAVE_CONNECT_ERROR),
	BMS_SYSFS_FIELD_RO(cell_supplier, BMS_PROP_CELL_SUPPLIER),
	BMS_SYSFS_FIELD_RO(isc_alert_level, BMS_PROP_ISC_ALERT_LEVEL),
	BMS_SYSFS_FIELD_RO(soa_alert_level, BMS_PROP_SOA_ALERT_LEVEL),
	BMS_SYSFS_FIELD_RO(charging_state, BMS_PROP_CHARGING_STATE),
	BMS_SYSFS_FIELD_RW(input_suspend, BMS_PROP_INPUT_SUSPEND),
	BMS_SYSFS_FIELD_RO(otg_ui_support, BMS_PROP_OTG_UI_SUPPORT),
	BMS_SYSFS_FIELD_RO(cid_status, BMS_PROP_CID_STATUS),
	BMS_SYSFS_FIELD_RW(cc_toggle, BMS_PROP_CC_TOGGLE),
	BMS_SYSFS_FIELD_RW(charger_partition_poweroffmode, BMS_PROP_CHARGER_PARTITION_POWEROFFMODE),
	BMS_SYSFS_FIELD_RW(charger_partition_test, BMS_PROP_CHARGER_PARTITION_TEST),
	BMS_SYSFS_FIELD_RO(batt_id, BMS_PROP_BATT_ID),
	BMS_SYSFS_FIELD_RO(fg_vendor, BMS_PROP_FG_VENDOR),
	BMS_SYSFS_FIELD_RW(dod_count, BMS_PROP_DOD_COUNT),
	BMS_SYSFS_FIELD_RW(reverse_quick_charge, BMS_PROP_REVERSE_QUICK_CHARGE),
	BMS_SYSFS_FIELD_RW(revchg_bcl, BMS_PROP_REVCHG_BCL),
	BMS_SYSFS_FIELD_RW(shipmode_count_reset, BMS_PROP_SHIPMODE),
	BMS_SYSFS_FIELD_RO(count_level1, BMS_PROP_COUNT_LEVLE1),
	BMS_SYSFS_FIELD_RO(count_level2, BMS_PROP_COUNT_LEVLE2),
	BMS_SYSFS_FIELD_RO(count_level3, BMS_PROP_COUNT_LEVLE3),
	BMS_SYSFS_FIELD_RO(over_vol_duration, BMS_PROP_OVER_VOL_DURATION),
	BMS_SYSFS_FIELD_RO(min_life_temp, BMS_PROP_MIN_LIFE_TIME),
	BMS_SYSFS_FIELD_RO(max_life_temp, BMS_PROP_MAX_LIFE_TIME),
	BMS_SYSFS_FIELD_RO(min_life_vol, BMS_PROP_MIN_LIFE_VOL),
	BMS_SYSFS_FIELD_RO(max_life_vol, BMS_PROP_MAX_LIFE_VOL),
	BMS_SYSFS_FIELD_RO(rel_soh_cyclecount, BMS_PROP_REL_SOH_CYCLECOUNT),
	BMS_SYSFS_FIELD_RO(rel_soh, BMS_PROP_REL_SOH),
	BMS_SYSFS_FIELD_RO(qmax_cyclecount, BMS_PROP_QMAX_CYCLECOUNT),
	BMS_SYSFS_FIELD_RO(qmax, BMS_PROP_QMAX),
	BMS_SYSFS_FIELD_RO(eis_soh_cyclecount, BMS_PROP_EIS_SOH_CYCLECOUNT),
	BMS_SYSFS_FIELD_RO(eis_soh, BMS_PROP_EIS_SOH),
	BMS_SYSFS_FIELD_RO(batt_use_environment, BMS_PROP_BAT_USE_ENVIRONMENT),
	BMS_SYSFS_FIELD_RO(charge_absoc, BMS_PROP_CHARGE_ABSOC),
	#if IS_ENABLED(CONFIG_RUST_DETECTION)
	BMS_SYSFS_FIELD_RO(moisture_detection_en, BAT_PROP_MOISTURE_DETECTION_EN),
	BMS_SYSFS_FIELD_RO(moisture_detection_status, BAT_PROP_MOISTURE_DETECTION_STATUS),
	BMS_SYSFS_FIELD_RW(lpd_charging, BMS_PROP_LPD_CHARGING),
	#endif
	BMS_SYSFS_FIELD_RO(soh_new, BMS_PROP_SOH_NEW),
};

int bms_get_property(enum bms_property bp,
			    int *val)
{
	struct bq_fg_chip *bq;
	struct power_supply *psy;

	psy = power_supply_get_by_name("battery");
	if (psy == NULL)
		return -ENODEV;

	bq = (struct bq_fg_chip *)power_supply_get_drvdata(psy);
	if (bms_sysfs_field_tbl[bp].prop == bp)
		bms_sysfs_field_tbl[bp].get(bq,
			&bms_sysfs_field_tbl[bp], val);
	else {
		fg_err("%s usb bp:%d idx error\n", __func__, bp);
		return -ENOTSUPP;
	}

	return 0;
}
EXPORT_SYMBOL(bms_get_property);

int bms_set_property(enum bms_property bp,
			    int val)
{
	struct bq_fg_chip *bq;
	struct power_supply *psy;

	psy = power_supply_get_by_name("battery");
	if (psy == NULL)
		return -ENODEV;

	bq = (struct bq_fg_chip *)power_supply_get_drvdata(psy);

	if (bms_sysfs_field_tbl[bp].prop == bp)
		bms_sysfs_field_tbl[bp].set(bq,
			&bms_sysfs_field_tbl[bp], val);
	else {
		fg_err("%s usb bp:%d idx error\n", __func__, bp);
		return -ENOTSUPP;
	}
	return 0;
}
EXPORT_SYMBOL(bms_set_property);

static struct attribute *
	bms_sysfs_attrs[ARRAY_SIZE(bms_sysfs_field_tbl) + 7];

static const struct attribute_group bms_sysfs_attr_group = {
	.attrs = bms_sysfs_attrs,
};

static void bms_sysfs_init_attrs(void)
{
	int i, limit = ARRAY_SIZE(bms_sysfs_field_tbl);

	for (i = 0; i < limit; i++)
		bms_sysfs_attrs[i] = &bms_sysfs_field_tbl[i].attr.attr;

	bms_sysfs_attrs[i] = &first_usage_date_attr.attr;
	bms_sysfs_attrs[i+1] = &manufacturing_date_attr.attr;
	bms_sysfs_attrs[i+2] = &fg1_cycle_attr.attr;
	bms_sysfs_attrs[i+3] = &soh_sn_attr.attr;
	bms_sysfs_attrs[i+4] = &calc_rvalue_attr.attr;
	bms_sysfs_attrs[i+5] = &batt_sn_attr.attr;

	bms_sysfs_attrs[i+6] = NULL; /* Has additional entry for this */
}

static int battery_sysfs_create_group(struct power_supply *psy)
{
	bms_sysfs_init_attrs();

	return sysfs_create_group(&psy->dev.kobj,
			&bms_sysfs_attr_group);
}

int check_cap_level(int uisoc)
{
	if (uisoc >= 100)
		return POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	else if (uisoc >= 80 && uisoc < 100)
		return POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
	else if (uisoc >= 20 && uisoc < 80)
		return POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	else if (uisoc > 0 && uisoc < 20)
		return POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	else if (uisoc == 0)
		return POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	else
		return POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;
}

static enum power_supply_property fg_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

#define SHUTDOWN_DELAY_VOL	3300
#define SHUTDOWN_VOL	3400
static int fg_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val)
{
	struct bq_fg_chip *bq = power_supply_get_drvdata(psy);
	struct charger_device *chg_dev = get_charger_by_name("primary_chg");
	struct votable	*fv_votable = find_votable("CHARGER_FV");
	int vbat_min = 0, vbat_max = 0;
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bq->bat_status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (bq->tbat <= -100)
			bq->bat_health = POWER_SUPPLY_HEALTH_COLD;
		else if (bq->tbat <= 150)
			bq->bat_health = POWER_SUPPLY_HEALTH_COOL;
		else if (bq->tbat <= 480)
			bq->bat_health = POWER_SUPPLY_HEALTH_GOOD;
		else if (bq->tbat <= 520)
			bq->bat_health = POWER_SUPPLY_HEALTH_WARM;
		else if (bq->tbat < 600)
			bq->bat_health = POWER_SUPPLY_HEALTH_HOT;
		else
			bq->bat_health = POWER_SUPPLY_HEALTH_OVERHEAT;
		val->intval = bq->bat_health;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		if (bq->fake_soc != 0xff)
			val->intval = check_cap_level(bq->fake_soc);
		else
			val->intval = check_cap_level(bq->ui_soc);
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = bq->model_name;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = bq->batt_info;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (bq->i2c_err_flag) {
			if (!IS_ERR_OR_NULL(chg_dev)) {
				charger_dev_get_adc(chg_dev, ADC_CHANNEL_VBAT, &vbat_min, &vbat_max);
				val->intval = vbat_min;
				break;
			}
		}
		mutex_lock(&bq->data_lock);
		fg_read_volt(bq);
		val->intval = bq->cell_voltage[2] * 1000;
		mutex_unlock(&bq->data_lock);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		mutex_lock(&bq->data_lock);
		bq->ibat = fg_read_current(bq);
		if (bq->i2c_err_flag)
			val->intval = -500000;
		else
			val->intval = bq->ibat * 1000;
		mutex_unlock(&bq->data_lock);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = fg_read_avg_current(bq);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (bq->fake_soc != 0xff) {
			val->intval = bq->fake_soc;
			break;
		}
		if (bq->i2c_err_flag)
			val->intval = 15;
		else
			val->intval = bq->ui_soc <= 0 ? 0 : bq->ui_soc;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (bq->fake_tbat) {
			val->intval = bq->fake_tbat;
			break;
		}
		if (bq->i2c_err_flag)
			val->intval = 250;
		else
			val->intval = bq->tbat;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (bq->device_name == BQ_FG_BQ27Z561 || bq->device_name == BQ_FG_NFG1000A || bq->device_name == BQ_FG_NFG1000B || bq->device_name == MPC_FG_MPC8011B)
			val->intval = bq->fcc;
		else if (bq->device_name == BQ_FG_BQ28Z610)
			val->intval = bq->fcc * 2;
		else
			val->intval = 4500;
		val->intval *= 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		if (bq->device_name == BQ_FG_BQ27Z561 || bq->device_name == BQ_FG_NFG1000A || bq->device_name == BQ_FG_NFG1000B || bq->device_name == MPC_FG_MPC8011B)
			val->intval = bq->dc;
		else if (bq->device_name == BQ_FG_BQ28Z610)
			val->intval = bq->dc * 2;
		else
			val->intval = 4500;
		val->intval *= 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = bq->rm;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		if (bq->fake_cycle_count > 0) {
			val->intval = bq->fake_cycle_count;
		} else {
			val->intval = bq->cycle_count;
		}
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		if (IS_ERR_OR_NULL(bq->chg_psy)) {
			bq->chg_psy = devm_power_supply_get_by_phandle(bq->dev, "charger");
			fg_err("%s retry to get chg_psy\n", __func__);
		}
		if (IS_ERR_OR_NULL(bq->chg_psy)) {
			fg_err("%s Couldn't get chg_psy\n", __func__);
			ret = 4350;
		} else {
			ret = power_supply_get_property(bq->chg_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, val);
			if (ret < 0)
				fg_err("get CV property fail\n");
		}
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (IS_ERR_OR_NULL(fv_votable)) {
			fg_err("get VOLTAGE_MAX failed \n");
			val->intval = 0;
			break;
		}
		val->intval = get_effective_result(fv_votable);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = bq->thermal_level;
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static int fg_set_property(struct power_supply *psy, enum power_supply_property prop, const union power_supply_propval *val)
{
	struct bq_fg_chip *bq = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
		bq->fake_tbat = val->intval;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		bq->fake_soc = val->intval;
		power_supply_changed(bq->fg_psy);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		bq->fake_cycle_count = val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		bq->thermal_level = val->intval;
		fg_info("set battery thermal level = %d\n", bq->thermal_level);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int fg_prop_is_writeable(struct power_supply *psy, enum power_supply_property prop)
{
	int ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static void bq_battery_external_power_changed(struct power_supply *psy)
{
	struct bq_fg_chip *bq;
	union power_supply_propval online = {0}, status = {0}, vbat0 = {0};
	union power_supply_propval prop_type = {0};
	int cur_chr_type = 0;
	bool charge_full = false;
	bool warm_term = false;
	struct power_supply *dv2_chg_psy = NULL;
	struct power_supply *usb_psy = NULL;
	struct mtk_charger *mtk_chg_info = NULL;
	struct charger_device *chg_dev = NULL;
	int ret = 0, temp = 0;
	int fg_status = 0x00;

	bq = psy->drv_data;

	mtk_chg_info = get_mtk_charger_info();
	if (mtk_chg_info == NULL)
		fg_info("failed get mtk charger info\n");

	chg_dev = get_charger_by_name("primary_chg");
	if (chg_dev == NULL)
		fg_info("failed get mt6375 charger\n");

	if(!bq->fuel_gauge)
		fg_status = fuelguage_check_fg_status(bq->fuel_gauge);

	if (!bq->mtk_charger)
		bq->mtk_charger = get_charger_by_name("mtk_charger");

	usb_psy = power_supply_get_by_name("usb");
	if (!IS_ERR_OR_NULL(usb_psy)) {
		ret = charger_dev_usb_get_property(bq->mtk_charger, USB_PROP_CHARGE_FULL, &temp);
		if (ret)
			charge_full = false;
		else
			charge_full = temp;

		ret = charger_dev_usb_get_property(bq->mtk_charger, USB_PROP_WARM_TERM, &temp);
		if (ret)
			warm_term = false;
		else
			warm_term = temp;
	}

	if (IS_ERR_OR_NULL(bq->chg_psy)) {
		bq->chg_psy = devm_power_supply_get_by_phandle(bq->dev, "charger");
		fg_err("%s retry to get chg_psy\n", __func__);
		if (IS_ERR_OR_NULL(bq->chg_psy))
			fg_err("%s fail to get chg_psy\n", __func__);
	} else {
		ret = power_supply_get_property(bq->chg_psy,
			POWER_SUPPLY_PROP_ONLINE, &online);
		ret = power_supply_get_property(bq->chg_psy,
			POWER_SUPPLY_PROP_STATUS, &status);
		ret = power_supply_get_property(bq->chg_psy,
			POWER_SUPPLY_PROP_ENERGY_EMPTY, &vbat0);
		fg_err("%s status.intval=%d online.intval=%d", __func__, status.intval, online.intval);

		if (!online.intval || (chg_dev != NULL && chg_dev->noti.vbusbad_stat &&
				mtk_chg_info != NULL && mtk_chg_info->vbus_check && bq->ibat >= 0)) {
			bq->bat_status = POWER_SUPPLY_STATUS_DISCHARGING;
		} else {
			if (status.intval == POWER_SUPPLY_STATUS_NOT_CHARGING) {
				if (bq->ibat > 0)
					bq->bat_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
				dv2_chg_psy = power_supply_get_by_name("mtk-mst-div-chg");
				if (!IS_ERR_OR_NULL(dv2_chg_psy)) {
					ret = power_supply_get_property(dv2_chg_psy,
						POWER_SUPPLY_PROP_ONLINE, &online);
					if (online.intval) {
						bq->bat_status =
							POWER_SUPPLY_STATUS_CHARGING;
						status.intval =
							POWER_SUPPLY_STATUS_CHARGING;
					}
				}
				if (!IS_ERR_OR_NULL(usb_psy)) {
					ret = power_supply_get_property(usb_psy,
						POWER_SUPPLY_PROP_STATUS, &status);
					fg_err("get battery status = %d\n", status.intval);
					if (status.intval == POWER_SUPPLY_STATUS_CHARGING) {
						bq->bat_status = POWER_SUPPLY_STATUS_CHARGING;
					} else if (status.intval == POWER_SUPPLY_STATUS_DISCHARGING) {
						bq->bat_status = POWER_SUPPLY_STATUS_DISCHARGING;
					}
				}
			} else {
				if (charge_full) {
					fg_err("POWER_SUPPLY_STATUS_FULL, EOC\n");
					if ( warm_term || (fg_status & FG_ERR_MASK)) {
						bq->bat_status = POWER_SUPPLY_STATUS_CHARGING;
						fg_err("The charging status is %d\n", bq->bat_status);
					} else {
						bq->bat_status = POWER_SUPPLY_STATUS_FULL;
					}
				} else {
					bq->bat_status = POWER_SUPPLY_STATUS_CHARGING;
					fg_err("charge_status = %d\n", bq->bat_status);
				}

			}
			//fg_sw_bat_cycle_accu(bq);
		}


		if (mtk_chg_info != NULL) {
			if(mtk_chg_info->plug_in_soc100_flag == true)
				bq->bat_status = POWER_SUPPLY_STATUS_FULL;

			if(mtk_chg_info->input_suspend ||
					(online.intval && (mtk_chg_info->thermal.sm == BAT_TEMP_LOW ||
							mtk_chg_info->thermal.sm == BAT_TEMP_HIGH)))
				bq->bat_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		}

		fg_err("%s final event, name:%s online:%d, status:%d\n", __func__, psy->desc->name, online.intval, status.intval);

		/* check charger type */
		ret = power_supply_get_property(bq->chg_psy,
			POWER_SUPPLY_PROP_USB_TYPE, &prop_type);
		/* plug in out */
		cur_chr_type = prop_type.intval;
		if (cur_chr_type == POWER_SUPPLY_TYPE_UNKNOWN) {
			if (bq->chr_type != POWER_SUPPLY_TYPE_UNKNOWN)
				fg_err("%s chr plug out\n", __func__);
		}

		fg_update_status(bq);
	}

	bq->chr_type = cur_chr_type;
}

static int fg_init_psy(struct bq_fg_chip *bq)
{
	struct power_supply_config fg_psy_cfg = {};

	bq->fg_psy_d.name = "battery";
	bq->fg_psy_d.type = POWER_SUPPLY_TYPE_BATTERY;
	bq->fg_psy_d.properties = fg_props;
	bq->fg_psy_d.num_properties = ARRAY_SIZE(fg_props);
	bq->fg_psy_d.get_property = fg_get_property;
	bq->fg_psy_d.set_property = fg_set_property;
	bq->fg_psy_d.property_is_writeable = fg_prop_is_writeable;
	bq->fg_psy_d.external_power_changed = bq_battery_external_power_changed;
	fg_psy_cfg.drv_data = bq;

	bq->fg_psy = devm_power_supply_register(bq->dev, &bq->fg_psy_d, &fg_psy_cfg);
	if (IS_ERR(bq->fg_psy)) {
		fg_err("%s failed to register fg_psy", bq->log_tag);
		return PTR_ERR(bq->fg_psy);
	} else
		battery_sysfs_create_group(bq->fg_psy);

	return 0;
}


/****************************************************/
/*               add bms node                       */
/****************************************************/


static ssize_t soh_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct bq_fg_chip *bq;
	struct power_supply *psy;

	psy = dev_get_drvdata(dev);
	bq = (struct bq_fg_chip *)power_supply_get_drvdata(psy);

	if (bq) {
		bq->soh = fg_read_soh(bq);
		fg_err("%s soh = %d", __func__, bq->soh);
	}
	return sprintf(buf, "%d\n", bq->soh);
}

static struct device_attribute soh_attr = __ATTR(soh, 0444, soh_show, NULL);

static ssize_t rsoc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bq_fg_chip *bq;
	struct power_supply *psy;

	psy = dev_get_drvdata(dev);
	bq = (struct bq_fg_chip *)power_supply_get_drvdata(psy);

	if (bq) {
		bq->rsoc = fg_read_rsoc(bq);
		fg_err("%s rsoc = %d", __func__, bq->rsoc);
	}

	return sprintf(buf, "%d\n", bq->rsoc);
}
static struct device_attribute rsoc_attr = __ATTR(rsoc, 0444, rsoc_show, NULL);

static struct attribute *bms_file_psy_attr[] = {
	&soh_attr.attr,
	&rsoc_attr.attr,
	NULL,
};

static const struct attribute_group bms_psy_attrs_group = {
	.attrs = bms_file_psy_attr,
};

static enum power_supply_property bms_file_props[] = {
	POWER_SUPPLY_PROP_AUTHENTIC,

};

static int bms_file_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val)
{
	struct bq_fg_chip *bq = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_AUTHENTIC:
		val->intval = bq->authenticate;
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static int bms_file_set_property(struct power_supply *psy, enum power_supply_property prop, const union power_supply_propval *val)
{
	struct bq_fg_chip *bq = power_supply_get_drvdata(psy);
	switch (prop) {
	case POWER_SUPPLY_PROP_AUTHENTIC:
		bq->authenticate = val->intval;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int bms_file_prop_is_writeable(struct power_supply *psy, enum power_supply_property prop)
{
	int ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_AUTHENTIC:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static void bms_file_external_power_changed(struct power_supply *psy)
{

}

static int bms_file_init_psy(struct bq_fg_chip *bq)
{
	struct power_supply_config bms_file_psy_cfg = {};
	int ret;

	bq->bms_psy_d.name = "bms";
	bq->bms_psy_d.type = POWER_SUPPLY_TYPE_UNKNOWN;
	bq->bms_psy_d.properties = bms_file_props;
	bq->bms_psy_d.num_properties = ARRAY_SIZE(bms_file_props);
	bq->bms_psy_d.get_property = bms_file_get_property;
	bq->bms_psy_d.set_property = bms_file_set_property;
	bq->bms_psy_d.property_is_writeable = bms_file_prop_is_writeable;
	bq->bms_psy_d.external_power_changed = bms_file_external_power_changed;
	bms_file_psy_cfg.drv_data = bq;
	bq->bms_file_psy = devm_power_supply_register(bq->dev, &bq->bms_psy_d, &bms_file_psy_cfg);
	if (IS_ERR(bq->bms_file_psy)) {
		fg_err("%s failed to register fg_psy", bq->log_tag);
		return PTR_ERR(bq->bms_file_psy);
	} else {
		ret = sysfs_create_group(&bq->bms_file_psy->dev.kobj,&bms_psy_attrs_group);
		if (ret) {
			fg_err("failed create_group bms file");
			return ret;
		}
	}

	return 0;
}

static ssize_t fg_show_qmax0(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret = 0;

	mutex_lock(&bq->data_lock);
	fg_read_qmax(bq);
	mutex_unlock(&bq->data_lock);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", bq->qmax[0]);

	return ret;
}

static ssize_t fg_show_qmax1(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", bq->qmax[1]);

	return ret;
}

static ssize_t fg_show_cell0_voltage(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", bq->cell_voltage[0]);

	return ret;
}

static ssize_t fg_show_cell1_voltage(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", bq->cell_voltage[1]);

	return ret;
}

static ssize_t fg_show_rsoc(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int rsoc = 0, ret = 0;

	mutex_lock(&bq->data_lock);
	rsoc = fg_read_rsoc(bq);
	mutex_unlock(&bq->data_lock);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", rsoc);

	return ret;
}

static ssize_t fg_show_fcc(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int fcc = 0, ret = 0;

	mutex_lock(&bq->data_lock);
	fcc = fg_read_fcc(bq);
	mutex_unlock(&bq->data_lock);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", fcc);

	return ret;
}

static ssize_t fg_show_rm(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int rm = 0, ret = 0;

	mutex_lock(&bq->data_lock);
	rm = fg_read_rm(bq);
	mutex_unlock(&bq->data_lock);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", rm);

	return ret;
}

int fg_stringtohex(char *str, unsigned char *out, unsigned int *outlen)
{
	char *p = str;
	char high = 0, low = 0;
	int tmplen = strlen(p), cnt = 0;
	tmplen = strlen(p);
	while(cnt < (tmplen / 2))
	{
		high = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;
		low = (*(++ p) > '9' && ((*p <= 'F') || (*p <= 'f'))) ? *(p) - 48 - 7 : *(p) - 48;
		out[cnt] = ((high & 0x0f) << 4 | (low & 0x0f));
		p ++;
		cnt ++;
	}
	if(tmplen % 2 != 0) out[cnt] = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;

	if(outlen != NULL) *outlen = tmplen / 2 + tmplen % 2;

	return tmplen / 2 + tmplen % 2;
}

static ssize_t fg_verify_digest_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	u8 digest_buf[4] = {0};
	int len = 0, i = 0;

	if (bq->device_name == BQ_FG_BQ27Z561 || bq->device_name == BQ_FG_NFG1000A || bq->device_name == BQ_FG_NFG1000B || bq->device_name == MPC_FG_MPC8011B) {
		for (i = 0; i < RANDOM_CHALLENGE_LEN_BQ27Z561; i++) {
			memset(digest_buf, 0, sizeof(digest_buf));
			snprintf(digest_buf, sizeof(digest_buf) - 1, "%02x", bq->digest[i]);
			strlcat(buf, digest_buf, RANDOM_CHALLENGE_LEN_BQ27Z561 * 2 + 1);
		}
	} else if (bq->device_name == BQ_FG_BQ28Z610) {
		for (i = 0; i < RANDOM_CHALLENGE_LEN_BQ28Z610; i++) {
			memset(digest_buf, 0, sizeof(digest_buf));
			snprintf(digest_buf, sizeof(digest_buf) - 1, "%02x", bq->digest[i]);
			strlcat(buf, digest_buf, RANDOM_CHALLENGE_LEN_BQ28Z610 * 2 + 1);
		}
	} else {
		fg_err("%s not support device name\n", bq->log_tag);
	}

	len = strlen(buf);
	buf[len] = '\0';

	return strlen(buf) + 1;
}

ssize_t fg_verify_digest_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int i = 0;
	u8 random[RANDOM_CHALLENGE_LEN_MAX] = {0};
	char kbuf[70] = {0};

	memset(kbuf, 0, sizeof(kbuf));
	strncpy(kbuf, buf, count - 1);
	fg_stringtohex(kbuf, random, &i);
	if (bq->device_name == BQ_FG_BQ27Z561 || bq->device_name == BQ_FG_NFG1000A || bq->device_name == BQ_FG_NFG1000B || bq->device_name == MPC_FG_MPC8011B)
		fg_sha256_auth(bq, random, RANDOM_CHALLENGE_LEN_BQ27Z561);
	else if (bq->device_name == BQ_FG_BQ28Z610)
		fg_sha256_auth(bq, random, RANDOM_CHALLENGE_LEN_BQ28Z610);

	return count;
}

static ssize_t fg_show_log_level(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", log_level);
	fg_info("%s show log_level = %d\n", bq->log_tag, log_level);

	return ret;
}

static ssize_t fg_store_log_level(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret = 0;

	ret = sscanf(buf, "%d", &log_level);
	fg_info("%s store log_level = %d\n", bq->log_tag, log_level);

	return count;
}

static ssize_t bms_device_ui_soh_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret = 0;
	unsigned char data[64] = { 0 };

	if (fg_mac_read_block(bq, FG_MAC_CMD_UI_SOH, data, 11))
		return -EINVAL;
	ret = scnprintf(buf, PAGE_SIZE,
		"%u %u %u %u %u %u %u %u %u %u %u\n",
		data[0], data[1], data[2], data[3], data[4], data[5],
		data[6], data[7], data[8], data[9], data[10]);

	return ret;
}

static ssize_t bms_device_ui_soh_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	char *pchar = NULL;
	int size = 0, i = 0, ret;
	unsigned char data_uisoh[32] = { 0 };
	unsigned char data[32] = { 0 };
	unsigned int temp = 0;
	char *tmp_buf = NULL;

	tmp_buf = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp_buf)
		return -EINVAL;

	strscpy(tmp_buf, buf, count + 1);
	fg_err("%s\n", tmp_buf);
	while ((pchar = strsep(&tmp_buf, " ")) != NULL) {
		if (kstrtouint(pchar, 0, &temp)) {
			fg_err("pchar cover to int fail\n");
			goto write_reg;
		}
		data_uisoh[size] = (unsigned char)temp;
		fg_err("data_uisoh[%d]=%d\n", size, data_uisoh[size]);
		++size;
		if (size >= 11)
			break;
	}

write_reg:
	bq->ui_soh = data_uisoh[0];

	ret = fg_mac_read_block(bq, FG_MAC_CMD_UI_SOH, data, 32);
	if (ret < 0) {
		fg_err("failed to get first_usage\n");
		return -EINVAL;
	}
	fg_err("read %s\n", data);
	for (i = 0; i < 11; i++)
		data[i] = data_uisoh[i];

	if (fg_mac_write_block(bq, FG_MAC_CMD_UI_SOH, data, 32))
		fg_err("write ui_soh failed\n");
	fg_err("write %s\n", data);

	return count;
}

static ssize_t model_show_status(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret = 0;

	if(!IS_ERR_OR_NULL(bq))
		ret = scnprintf(buf, PAGE_SIZE, "%d\n", bq->model_status);
	else
		ret = scnprintf(buf, PAGE_SIZE, "fail\n");

	return ret;
}

static ssize_t fg_show_vterm(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int vterm = 0, ret = 0;
	u8 data[2] = {0};

	mutex_lock(&bq->data_lock);
	ret = fg_mac_read_block(bq, FG_MAC_CMD_MIX_TERM_VOLT, data, 2);
	mutex_unlock(&bq->data_lock);
	if(ret < 0)
		vterm = 0;
	else
		vterm = data[1] << 8 | data[0];

	ret = snprintf(buf, PAGE_SIZE, "%d\n", vterm);

	return ret;
}

static DEVICE_ATTR(fcc, S_IRUGO, fg_show_fcc, NULL);
static DEVICE_ATTR(rm, S_IRUGO, fg_show_rm, NULL);
static DEVICE_ATTR(rsoc, S_IRUGO, fg_show_rsoc, NULL);
static DEVICE_ATTR(cell0_voltage, S_IRUGO, fg_show_cell0_voltage, NULL);
static DEVICE_ATTR(cell1_voltage, S_IRUGO, fg_show_cell1_voltage, NULL);
static DEVICE_ATTR(qmax0, S_IRUGO, fg_show_qmax0, NULL);
static DEVICE_ATTR(qmax1, S_IRUGO, fg_show_qmax1, NULL);
static DEVICE_ATTR(model_status, S_IRUGO, model_show_status, NULL);
static DEVICE_ATTR(verify_digest, S_IRUGO | S_IWUSR, fg_verify_digest_show, fg_verify_digest_store);
static DEVICE_ATTR(log_level, S_IRUGO | S_IWUSR, fg_show_log_level, fg_store_log_level);
static DEVICE_ATTR(ui_soh, S_IRUGO | S_IWUSR, bms_device_ui_soh_show, bms_device_ui_soh_store);
static DEVICE_ATTR(vterm, S_IRUGO, fg_show_vterm, NULL);

static struct attribute *fg_attributes[] = {
	&dev_attr_rm.attr,
	&dev_attr_fcc.attr,
	&dev_attr_rsoc.attr,
	&dev_attr_cell0_voltage.attr,
	&dev_attr_cell1_voltage.attr,
	&dev_attr_qmax0.attr,
	&dev_attr_qmax1.attr,
	&dev_attr_model_status.attr,
	&dev_attr_verify_digest.attr,
	&dev_attr_log_level.attr,
	&dev_attr_ui_soh.attr,
	&dev_attr_vterm.attr,
	NULL,
};

static const struct attribute_group fg_attr_group = {
	.attrs = fg_attributes,
};


static int fg_parse_dt(struct bq_fg_chip *bq)
{
	struct device_node *node = bq->dev->of_node;
	int ret = 0, size = 0;

	bq->max_chg_power_120w = of_property_read_bool(node, "max_chg_power_120w");
	bq->enable_shutdown_delay = of_property_read_bool(node, "enable_shutdown_delay");

	ret = of_property_read_u32(node, "normal_shutdown_vbat_1s", &bq->normal_shutdown_vbat);
	if (ret)
		fg_err("%s failed to parse normal_shutdown_vbat_1s\n", bq->log_tag);

	ret = of_property_read_u32(node, "critical_shutdown_vbat_1s", &bq->critical_shutdown_vbat);
	if (ret)
		fg_err("%s failed to parse critical_shutdown_vbat_1s\n", bq->log_tag);

	ret = of_property_read_u32(node, "cool_critical_shutdown_vbat_1s", &bq->cool_critical_shutdown_vbat);
	if (ret)
		fg_err("%s failed to parse cool_critical_shutdown_vbat_1s\n", bq->log_tag);

	ret = of_property_read_u32(node, "old_critical_shutdown_vbat_1s", &bq->old_critical_shutdown_vbat);
	if (ret)
		fg_err("%s failed to parse old_critical_shutdown_vbat_1s\n", bq->log_tag);

	bq->report_full_rsoc = 9650;

	ret = of_property_read_u32(node, "soc_gap_1s", &bq->soc_gap);
	if (ret)
		fg_err("%s failed to parse soc_gap_1s\n", bq->log_tag);

	of_get_property(node, "soc_decimal_rate", &size);
	if (size) {
		bq->dec_rate_seq = devm_kzalloc(bq->dev,
				size, GFP_KERNEL);
		if (bq->dec_rate_seq) {
			bq->dec_rate_len =
				(size / sizeof(*bq->dec_rate_seq));
			if (bq->dec_rate_len % 2) {
				fg_err("%s invalid soc decimal rate seq\n", bq->log_tag);
				return -EINVAL;
			}
			of_property_read_u32_array(node,
					"soc_decimal_rate",
					bq->dec_rate_seq,
					bq->dec_rate_len);
		} else {
			fg_err("%s error allocating memory for dec_rate_seq\n", bq->log_tag);
		}
	}

	return ret;
}

static int fg_check_device(struct bq_fg_chip *bq)
{
	u8 data[32];
	int ret = 0;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_DEVICE_NAME, data, sizeof(data));
	if (ret)
		fg_err("failed to get FG_MAC_CMD_DEVICE_NAME\n");
	else
		fg_err("FG_MAC_CMD_DEVICE_NAME : %s\n", data);

	if (strstr(data, "BN71") != NULL) {
		bq->device_name = MPC_FG_MPC8011B;
		strcpy(bq->area_name, "mpc8011b_cn");
		strcpy(bq->log_tag, "[HQCHG_MPC8011B_CN]");
	} else if (strstr(data, "BN69") != NULL) {
		bq->device_name = MPC_FG_MPC8011B;
		strcpy(bq->area_name, "mpc8011b_gl");
		strcpy(bq->log_tag, "[HQCHG_MPC8011B_GL]");
	} else if (strstr(data, "BN6A") != NULL) {
		bq->device_name = MPC_FG_MPC8011B;
		strcpy(bq->area_name, "mpc8011b_jn");
		strcpy(bq->log_tag, "[HQCHG_MPC8011B_JN]");
	} else {
		bq->device_name = BQ_FG_UNKNOWN;
		strcpy(bq->area_name, "UNKNOWN");
		strcpy(bq->log_tag, "[HQCHG_UNKNOWN_FG]");
	}

	ret = fg_mac_read_block(bq, FG_MAC_CMD_MANU_NAME, data, 32);
	if (ret)
		fg_info("failed to get FG_MAC_CMD_MANU_NAME\n");
	else
		fg_err("FG_MAC_CMD_MANU_NAME : %s\n", data);

	if (!strncmp(data, "MI", 2) && bq->device_name != BQ_FG_UNKNOWN)
		bq->chip_ok = true;
	else
		bq->chip_ok = false;

	if(strncmp(&data[4], "6", 1) == 0)
		bq->charge_watt_err = false;
	else
		bq->charge_watt_err = true;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_DEVICE_CHEM, data, 32);
	if (ret)
		fg_info("failed to get FG_MAC_CMD_DEVICE_CHEM\n");
	else
		fg_err("FG_MAC_CMD_DEVICE_CHEM : %s\n", data);

	if(strncmp(&data[3], "5", 1) >= 0)
		bq->model_status = 1;
	else
		bq->model_status = 0;

	if (!strncmp(&data[2], "S", 1) && bq->device_name != BQ_FG_UNKNOWN) {
		bq->cell_supplier = BMS_CELL_SWD;
		strcpy(bq->device_chem, "swd");
	} else if(!strncmp(&data[2], "N", 1) && bq->device_name != BQ_FG_UNKNOWN) {
		bq->cell_supplier = BMS_CELL_NVT;
		strcpy(bq->device_chem, "nvt");
	} else if(!strncmp(&data[2], "C", 1) && bq->device_name != BQ_FG_UNKNOWN) {
		bq->cell_supplier = BMS_CELL_COS;
		strcpy(bq->device_chem, "cos");
	} else {
		bq->cell_supplier = BMS_CELL_UNKNOWN;
		strcpy(bq->device_chem, "UNKNOWN");
	}
	snprintf(bq->batt_info, BATT_INFO_LEN_MAX, "%s_%s", bq->area_name, bq->device_chem);
	snprintf(bq->model_name, MODEL_NAME_LEN_MAX, "P16_%dmah_45w", bq->dc);

	return ret;
}

static struct fuel_gauge_ops fuel_gauge_ops = {
	.get_soc_decimal = fuel_guage_get_soc_decimal,
	.get_soc_decimal_rate = fuel_guage_get_soc_decimal_rate,
	.set_fastcharge_mode = fuelguage_set_fastcharge_mode,
	.get_fastcharge_mode = fuelguage_get_fastcharge_mode,
	.check_fg_status = fuelguage_check_fg_status,
	.get_batt_auth = fuelguage_get_batt_auth,
	.report_fg_soc100 = fuelguage_report_fg_soc100,
	.get_rsoc = fuelguage_get_rsoc,
	.get_raw_soc = fuelguage_get_raw_soc,
};

static int fg_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct bq_fg_chip *bq;
	int ret = 0;
	char *name = NULL;
	u8 data[5] = {0};

	fg_info("FG probe enter\n");
	bq = devm_kzalloc(&client->dev, sizeof(*bq), GFP_DMA);
	if (!bq)
		return -ENOMEM;

	bq->dev = &client->dev;
	bq->client = client;
	bq->monitor_delay = FG_MONITOR_DELAY_10S;
	bq->authenticate = 0;
	bq->batt_auth_done = 0;

	memcpy(bq->regs, bq_fg_regs, NUM_REGS);

	i2c_set_clientdata(client, bq);
	name = devm_kasprintf(bq->dev, GFP_KERNEL, "%s",
		"bms suspend wakelock");
	bq->bms_wakelock = wakeup_source_register(NULL, name);
	bq->shutdown_mode = false;
	bq->shutdown_flag = false;
	bq->fake_cycle_count = 0;
	bq->raw_soc = -ENODATA;
	bq->last_soc = -EINVAL;
	bq->i2c_error_count = 0;
	mutex_init(&bq->i2c_rw_lock);
	mutex_init(&bq->data_lock);
	atomic_set(&bq->fg_in_sleep, 0);

	bq->regmap = devm_regmap_init_i2c(client, &fg_regmap_config);
	if (IS_ERR(bq->regmap)) {
		fg_err("failed to allocate regmap\n");
		return PTR_ERR(bq->regmap);
	}

	bq->dc = fg_read_dc(bq);
	fg_check_device(bq);

	ret = fg_parse_dt(bq);
	if (ret) {
		fg_err("%s failed to parse DTS\n", bq->log_tag);
		return ret;
	}

	ret = fg_init_psy(bq);
	if (ret) {
		fg_err("%s failed to init psy\n", bq->log_tag);
		return ret;
	}

	ret = bms_file_init_psy(bq);
	if (ret) {
		fg_err("%s failed to init psy\n", bq->log_tag);
		return ret;
	}

	time_init = ktime_get();
	fg_update_status(bq);

	ret = sysfs_create_group(&bq->dev->kobj, &fg_attr_group);
	if (ret) {
		fg_err("%s failed to register sysfs\n", bq->log_tag);
		return ret;
	}

	bq->update_now = true;
	INIT_DELAYED_WORK(&bq->monitor_work, fg_monitor_workfunc);
	schedule_delayed_work(&bq->monitor_work, msecs_to_jiffies(5000));

	/* init fast charge mode */
	data[0] = 0;
	fg_info("-fastcharge init-\n");
	ret = fg_mac_write_block(bq, FG_MAC_CMD_FASTCHARGE_DIS, data, 2);
	if (ret) {
		fg_err("%s failed to write fastcharge = %d\n", bq->log_tag, ret);
	}

	bq->fuel_gauge = fuel_gauge_register("fuel_gauge",
						bq->dev, &fuel_gauge_ops, bq);
	if (!bq->fuel_gauge) {
		ret = PTR_ERR(bq->fuel_gauge);
		fg_err("%s failed to register fuel_gauge\n", bq->log_tag);
		return ret;
	}

	bq->fake_soc = 0xff;

	charger_partition_init();

	fg_info("%s FG probe success\n", bq->log_tag);

	return 0;
}

static int fg_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	atomic_set(&bq->fg_in_sleep, 1);
	fg_err("%s in sleep\n", __func__);

	cancel_delayed_work_sync(&bq->monitor_work);

	return 0;
}

static int fg_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	atomic_set(&bq->fg_in_sleep, 0);
	fg_err("%s resume in sleep\n", __func__);
	if (!bq->bms_wakelock->active)
		__pm_stay_awake(bq->bms_wakelock);
	schedule_delayed_work(&bq->monitor_work, 0);

	return 0;
}

static void fg_remove(struct i2c_client *client)
{
	struct bq_fg_chip *bq = i2c_get_clientdata(client);

	power_supply_unregister(bq->fg_psy);
	mutex_destroy(&bq->data_lock);
	mutex_destroy(&bq->i2c_rw_lock);
	sysfs_remove_group(&bq->dev->kobj, &fg_attr_group);
	charger_partition_exit();
}

static void fg_shutdown(struct i2c_client *client)
{
	struct bq_fg_chip *bq = i2c_get_clientdata(client);

	fg_info("%s bq fuel gauge driver shutdown!\n", bq->log_tag);
}

static struct of_device_id fg_match_table[] = {
	{.compatible = "bq28z610",},
	{},
};
MODULE_DEVICE_TABLE(of, fg_match_table);

static const struct i2c_device_id fg_id[] = {
	{ "bq28z610", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, fg_id);

static const struct dev_pm_ops fg_pm_ops = {
	.resume		= fg_resume,
	.suspend	= fg_suspend,
};

static struct i2c_driver fg_driver = {
	.driver	= {
		.name   = "bq28z610",
		.owner  = THIS_MODULE,
		.of_match_table = fg_match_table,
		.pm     = &fg_pm_ops,
	},
	.id_table       = fg_id,

	.probe          = fg_probe,
	.remove		= fg_remove,
	.shutdown	= fg_shutdown,
};

module_i2c_driver(fg_driver);

MODULE_DESCRIPTION("TI GAUGE Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Texas Instruments");
