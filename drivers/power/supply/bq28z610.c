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
#include "mtk_battery.h"

enum product_name {
	PRODUCT_NO,
	DUCHAMP_CN,
	DUCHAMP_GL,
};

static int log_level = 1;
static int product_name = PRODUCT_NO;
static ktime_t time_init = -1;

#define fg_err(fmt, ...)					\
do {								\
	if (log_level >= 0)					\
			printk(KERN_ERR "" fmt, ##__VA_ARGS__);	\
} while (0)

#define fg_info(fmt, ...)					\
do {								\
	if (log_level >= 1)					\
			printk(KERN_ERR "" fmt, ##__VA_ARGS__);	\
} while (0)

#define fg_dbg(fmt, ...)					\
do {								\
	if (log_level >= 2)					\
			printk(KERN_ERR "" fmt, ##__VA_ARGS__);	\
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

	if (reg == bq->regs[BQ_FG_REG_TEMP]) {
		fg_err("%s: reg=0x%02x, val=0x%02x,0x%02x\n", __func__,
			reg, data[1], data[0]);
	}

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
		// fg_err("%s:len=%d,sum=%d,data[%d]=%d\n", __func__, len, sum, i, data[i]);
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
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, 2);
	if (ret < 0)
		return ret;

	msleep(4);

	ret = fg_read_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, 36);
	if (ret < 0)
		return ret;

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

	if (len > 32)
		return -1;

	t_buf[0] = (u8)cmd;
	t_buf[1] = (u8)(cmd >> 8);
	for (i = 0; i < len; i++)
		t_buf[i+2] = data[i];

	/*write command/addr, data*/
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, len + 2);
	if (ret < 0) {
		fg_err("%s failed to write block\n", bq->log_tag);
		return ret;
	}

	cksum = fg_checksum(data, len + 2);
	t_buf[0] = cksum;
	t_buf[1] = len + 4; /*buf length, cmd, CRC and len byte itself*/
	/*write checksum and length*/
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_MAC_CHKSUM], t_buf, 2);

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

//#ifndef CONFIG_FACTORY_BUILD
	/*
	 * in very low rate BatteryTemperature read from fg is very large above 100 degree
	 * use workaround here to avoid high temp shutdown by mistaken
	 */
	if (tbat >= 3730) {
		tbat = 3180;
		fg_err("%s FG TBAT abnormal force 45C \n", bq->log_tag);
	}
//#endif

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

static void fg_read_volt(struct bq_fg_chip *bq)
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
#ifdef CONFIG_TARGET_PRODUCT_PEARL
			fcc = 5160;
#else
			fcc = pre_fcc;
#endif
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
	static u16 pre_rm =2580;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_RM], &rm);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
#ifdef CONFIG_TARGET_PRODUCT_PEARL
			rm = 718;
#else
			rm = 708;
#endif
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
			cc = 0;
			if(bq->i2c_error_count < 10)
				bq->i2c_error_count++;
		}
	}
	else
	{
		if(bq->i2c_error_count > 0)
			bq->i2c_error_count = 0;
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
		ret = fg_mac_read_block(bq, FG_MAC_CMD_QMAX, data, 14);
		if (ret < 0)
			fg_err("%s failed to read MAC\n", bq->log_tag);
	} else if (bq->device_name == BQ_FG_BQ28Z610) {
		ret = fg_mac_read_block(bq, FG_MAC_CMD_QMAX, data, 20);
		if (ret < 0)
			fg_err("%s failed to read MAC\n", bq->log_tag);
	} else {
		fg_err("%s not support device name\n", bq->log_tag);
	}

	bq->qmax[0] = (data[1] << 8) | data[0];
	bq->qmax[1] = (data[3] << 8) | data[2];
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
		if (ret) {
			fg_err("%s failed to write fastcharge = %d\n", bq->log_tag, ret);
			return ret;
		}
	} else {
		ret = fg_mac_write_block(bq, FG_MAC_CMD_FASTCHARGE_DIS, data, 2);
		if (ret) {
			fg_err("%s failed to write fastcharge = %d\n", bq->log_tag, ret);
			return ret;
		}
	}

	return ret;
}

static int fg_set_charge_eoc(struct bq_fg_chip *bq, bool enable)
{
	int ret = 0;
	u8 t_buf[40];

	bq->fast_chg = enable;

	t_buf[0] = 0x31;
	t_buf[1] = 0x00;
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, 2);
	if (ret < 0)
		return ret;

	return ret;
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

#define BATT_HIGH_AVG_CURRENT		1000
#define NORMAL_TEMP_CHARGING_DELTA	10000
#define NORMAL_DISTEMP_CHARGING_DELTA	60000
#define LOW_TEMP_CHARGING_DELTA		5000
#define LOW_TEMP_DISCHARGING_DELTA	20000
#define FFC_SMOOTH_LEN			4
#define FG_RAW_SOC_FULL			10000
#define FG_REPORT_FULL_SOC		9100
#define FG_OPTIMIZ_FULL_TIME		80000

// N11A_CN
#define SOC_HY 2
#define SOC_PROPORTION 975
#define SOC_PROPORTION_C 965
#define SOC_EXTRE 1
// 67W
#define SOC_PROPORTION_67W 97
#define SOC_PROPORTION_67W_C 98
// 120W
#define SOC_PROPORTION_120W 94
#define SOC_PROPORTION_120W_C 95

#define SOC_EXTRE_VBAT_LOW_THRESHOLD 3430
#define SOC_EXTRE_VBAT_LOW_THRESHOLD_COOL 3350
#define SOC_EXTRE_VBAT_LOW_THRESHOLD_COLD 3280
#define BATT_COOL_THRESHOLD 15
#define BATT_COLD_THRESHOLD 0


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

static int bq_battery_soc_smooth_tracking_new(struct bq_fg_chip *bq, int raw_soc, int batt_soc, int batt_ma)
{
	static int system_soc, last_system_soc, last_extreme_soc, low_voltage_cnt, low_voltage_threshold;
	int soc_changed = 0, unit_time = 10000, delta_time = 0, soc_delta = 0;
	static ktime_t last_change_time = -1;
	int change_delta = 0;
	int  rc, charging_status, i=0, batt_ma_avg = 0;
	union power_supply_propval pval = {0, };
	static int ibat_pos_count = 0;
	struct timespec64 time;
	ktime_t tmp_time = 0;
	bool smooth_en = false;

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
	charging_status = pval.intval;
	if (bq->tbat < 150) {
		bq->monitor_delay = FG_MONITOR_DELAY_3S;
	}
	if (!raw_soc) {
		bq->monitor_delay = FG_MONITOR_DELAY_10S;
	}
	/*Map system_soc value according to raw_soc */
	if(raw_soc >= bq->report_full_rsoc)
		system_soc = 100;
	else if(product_name == DUCHAMP_CN){
		raw_soc = (int)(raw_soc * 10 - SOC_PROPORTION * SOC_EXTRE) > 0 ? (raw_soc * 10 - SOC_PROPORTION * SOC_EXTRE) : 0 ;
		raw_soc = (int)(raw_soc / 10);
		system_soc = ((raw_soc * 10 + SOC_PROPORTION_C - SOC_EXTRE * 10) / (SOC_PROPORTION - SOC_EXTRE * 10));
		if (system_soc > 99)
			system_soc = 99;
	}else if (bq->max_chg_power_120w) {
		system_soc = ((raw_soc + SOC_PROPORTION_120W) / SOC_PROPORTION_120W_C);
		if(system_soc > 99)
			system_soc = 99;
	} else {
		system_soc = ((raw_soc + SOC_PROPORTION_67W) / SOC_PROPORTION_67W_C);
		if(system_soc > 99)
			system_soc = 99;
    }

	if ((product_name == DUCHAMP_CN) && (system_soc == 0) && (batt_soc <= SOC_EXTRE + 1))
		batt_soc = 0;
	/*Get the initial value for the first time */
	if(last_change_time == -1){
		last_change_time = ktime_get();
		if(system_soc != 0)
			last_system_soc = system_soc;
		else
			last_system_soc = batt_soc;
	}

	if ((charging_status == POWER_SUPPLY_STATUS_DISCHARGING ||
		charging_status == POWER_SUPPLY_STATUS_NOT_CHARGING ) && 
		!bq->rm && bq->tbat < 150 && last_system_soc >= 1) 
	{
		batt_ma_avg = fg_read_avg_current(bq);
		for (i = FFC_SMOOTH_LEN-1; i >= 0; i--) {
		if (batt_ma_avg > ffc_dischg_smooth[i].curr_lim) {
			unit_time = ffc_dischg_smooth[i].time;
			break;
		}
		}
		fg_info("enter low temperature smooth unit_time=%d batt_ma_avg=%d\n", unit_time, batt_ma_avg);
	}

	if (bq->tbat < BATT_COLD_THRESHOLD)
	{
		low_voltage_threshold = SOC_EXTRE_VBAT_LOW_THRESHOLD_COLD;
	} else if (bq->tbat < BATT_COOL_THRESHOLD){
		low_voltage_threshold = SOC_EXTRE_VBAT_LOW_THRESHOLD_COOL;
	} else{
		low_voltage_threshold = SOC_EXTRE_VBAT_LOW_THRESHOLD;
	}
	// soc extreme
	if (product_name == DUCHAMP_CN &&  bq->vbat < low_voltage_threshold && raw_soc > 200 && raw_soc < 600 &&
		(bq->ibat > 0 || charging_status == POWER_SUPPLY_STATUS_DISCHARGING))
	{
		if (++low_voltage_cnt >= 5 && last_extreme_soc)
		{
			system_soc = last_extreme_soc - 1;
			smooth_en = 1;
			low_voltage_cnt = 0;
		}else
		{
			system_soc = last_extreme_soc;
		}
	}else{
		low_voltage_cnt = 0;
		smooth_en = 0;
		if (SOC_EXTRE && system_soc > last_extreme_soc &&
			(bq->ibat > 0 || charging_status == POWER_SUPPLY_STATUS_DISCHARGING))
			system_soc = last_extreme_soc;
	}
	fg_info("bq_battery_soc_smooth_tracking: system_soc: %d, last_extreme_soc: %d, last_system_soc: %d, low_voltage_cnt: %d, batt_volt: %d",
			  system_soc, last_extreme_soc, last_system_soc, low_voltage_cnt, bq->vbat);
	if (smooth_en)
		unit_time = 30000;
	else
		unit_time = 10000;
	/*If the soc jump, will smooth one cap every 10S */
	soc_delta = abs(system_soc - last_system_soc);
	if(soc_delta > 1 || (bq->vbat < 3400 && system_soc > 0) || (unit_time != 10000 && soc_delta == 1) || smooth_en){
		//unit_time != 10000 && soc_delta == 1 fix low temperature 2% jump to 0%
		calc_delta_time(last_change_time, &change_delta);
		delta_time = change_delta / unit_time;
		if (delta_time < 0) {
			last_change_time = ktime_get();
			delta_time = 0;
		}
		soc_changed = min(1, delta_time);
		if (soc_changed) {
			if(charging_status == POWER_SUPPLY_STATUS_CHARGING && system_soc > last_system_soc)
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
	if (((charging_status == POWER_SUPPLY_STATUS_DISCHARGING) && (system_soc > last_system_soc)) || ((charging_status == POWER_SUPPLY_STATUS_CHARGING) && (system_soc < last_system_soc) && (ibat_pos_count < 3) && ((time.tv_sec > 10))))
		system_soc = last_system_soc;
	fg_info("smooth_new:sys_soc:%d last_sys_soc:%d soc_delta:%d charging_status:%d unit_time:%d batt_ma_avg=%d\n" ,
		system_soc, last_system_soc, soc_delta, charging_status, unit_time, batt_ma_avg);

	if(system_soc != last_system_soc){
		last_change_time = ktime_get();
		last_system_soc = system_soc;
	}
	if(system_soc > 100)
		system_soc =100;
	if(system_soc < 0)
		system_soc =0;

	if (bq->cycle_count >= 400 && bq->shutdown_voltage == 3200) {
		bq->normal_shutdown_vbat = bq->shutdown_voltage + 30;
	}

	if ((system_soc == 0) && ((bq->vbat >= bq->normal_shutdown_vbat) || ((time.tv_sec <= 10)))) {
		system_soc = 1;
		fg_err("uisoc::hold 1 when volt > %dmV. \n", bq->normal_shutdown_vbat);
	}

	if(bq->last_soc != system_soc){
		bq->last_soc = system_soc;
		last_extreme_soc = system_soc;
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

static void fg_update_status(struct bq_fg_chip *bq)
{
	int temp_soc = 0,  delta_temp = 0;
	static int last_soc = 0, last_temp = 0;
	ktime_t time_now = -1;
        struct mtk_battery *gm;

	mutex_lock(&bq->data_lock);
	bq->cycle_count = fg_read_cyclecount(bq);
	bq->rsoc = fg_read_rsoc(bq);
	bq->soh = fg_read_soh(bq);
	bq->raw_soc = fg_get_raw_soc(bq);
	bq->ibat = fg_read_current(bq);
	bq->tbat = fg_read_temperature(bq);
	fg_read_volt(bq);
	fg_read_status(bq);
	mutex_unlock(&bq->data_lock);
	fg_err("%s fg_update rsoc=%d, raw_soc=%d, vbat=%d, cycle_count=%d\n", __func__, bq->rsoc, bq->raw_soc, bq->vbat, bq->cycle_count);

	if (!battery_get_psy(bq)) {
		fg_err("%s fg_update failed to get battery psy\n", bq->log_tag);
		bq->ui_soc = bq->rsoc;
		return;
	} else {
                gm = (struct mtk_battery *)power_supply_get_drvdata(bq->batt_psy);
		time_now = ktime_get();
		if (time_init != -1 && (time_now - time_init < 10000 ))
		{
			bq->ui_soc = bq->rsoc;
			goto out;
		}
		bq->ui_soc = bq_battery_soc_smooth_tracking_new(bq, bq->raw_soc, bq->rsoc, bq->ibat);
		if(gm->night_charging && bq->ui_soc > 80 && bq->ui_soc > last_soc){
                bq->ui_soc = last_soc;
                fg_err("%s last_soc = %d, night_charging = %d,\n", __func__, last_soc, gm->night_charging);
         }

out:
		fg_info("%s [FG_STATUS] [UISOC RSOC RAWSOC TEMP_SOC SOH] = [%d %d %d %d %d], [VBAT CELL0 CELL1 IBAT TBAT FC FAST_MODE] = [%d %d %d %d %d %d %d]\n", bq->log_tag,
			bq->ui_soc, bq->rsoc, bq->raw_soc, temp_soc, bq->soh, bq->vbat, bq->cell_voltage[0], bq->cell_voltage[1], bq->ibat, bq->tbat, bq->batt_fc, bq->fast_chg);

		delta_temp = abs(last_temp - bq->tbat);
		if (bq->batt_psy && (last_soc != bq->ui_soc || delta_temp > 5 || bq->ui_soc == 0 || bq->rsoc == 0)) {
			fg_err("%s last_soc = %d, last_temp = %d, delta_temp = %d\n", __func__, last_soc, last_temp, delta_temp);
			power_supply_changed(bq->batt_psy);
		}

		last_soc = bq->ui_soc;
		if (delta_temp > 5)
			last_temp = bq->tbat;
	}
}

static void fg_monitor_workfunc(struct work_struct *work)
{
	struct bq_fg_chip *bq = container_of(work, struct bq_fg_chip, monitor_work.work);

	fg_update_status(bq);

	schedule_delayed_work(&bq->monitor_work, msecs_to_jiffies(bq->monitor_delay));
	if (bq->bms_wakelock->active)
		__pm_relax(bq->bms_wakelock);
}

static int charge_eoc_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->charge_eoc;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int charge_eoc_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		fg_set_charge_eoc(gm, !!val);
	fg_err("%s %d\n", __func__, val);
	return 0;
}

static int fastcharge_mode_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->fast_chg;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int fastcharge_mode_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		fg_set_fastcharge_mode(gm, !!val);
	fg_err("%s %d\n", __func__, val);
	return 0;
}

static int monitor_delay_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->monitor_delay;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int monitor_delay_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->monitor_delay = val;
	fg_err("%s %d\n", __func__, val);
	return 0;
}

static int fcc_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->fcc;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int rm_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->rm;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int rsoc_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->rsoc;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int shutdown_delay_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->shutdown_delay;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int shutdown_delay_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->fake_shutdown_delay_enable = val;
	fg_err("%s %d\n", __func__, val);
	return 0;
}

static int capacity_raw_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->raw_soc;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int soc_decimal_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = fg_get_soc_decimal(gm);
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int av_current_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = fg_read_avg_current(gm);
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int soc_decimal_rate_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = fg_get_soc_decimal_rate(gm);
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int resistance_id_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = 100000;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int resistance_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = 100000;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int authentic_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->authenticate;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int authentic_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->authenticate = !!val;
	fg_err("%s %d\n", __func__, val);
	return 0;
}

static int shutdown_mode_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->authenticate;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int shutdown_mode_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		fg_set_shutdown_mode(gm);
	fg_err("%s %d\n", __func__, val);
	return 0;
}

static int chip_ok_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->chip_ok;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int shutdown_voltage_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->shutdown_voltage;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int charge_done_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->batt_fc;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int soh_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->soh;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int bms_slave_connect_error_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	*val = gpio_get_value(gm->slave_connect_gpio);
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int cell_supplier_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->cell_supplier;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int i2c_error_count_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if(gm->fake_i2c_error_count > 0)
	{
		*val = gm->fake_i2c_error_count;
		return 0;
	}
	if (gm)
		*val = gm->i2c_error_count;
	else
		*val = 0;
	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int i2c_error_count_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->fake_i2c_error_count = val;
	fg_err("%s %d\n", __func__, val);
	return 0;
}

static ssize_t bms_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy;
	struct bq_fg_chip *gm;
	struct mtk_bms_sysfs_field_info *usb_attr;
	int val;
	ssize_t ret;

	ret = kstrtos32(buf, 0, &val);
	if (ret < 0)
		return ret;

	psy = dev_get_drvdata(dev);
	gm = (struct bq_fg_chip *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct mtk_bms_sysfs_field_info, attr);
	if (usb_attr->set != NULL)
		usb_attr->set(gm, usb_attr, val);

	return count;
}

static ssize_t bms_sysfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct bq_fg_chip *gm;
	struct mtk_bms_sysfs_field_info *usb_attr;
	int val = 0;
	ssize_t count;

	psy = dev_get_drvdata(dev);
	gm = (struct bq_fg_chip *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct mtk_bms_sysfs_field_info, attr);
	if (usb_attr->get != NULL)
		usb_attr->get(gm, usb_attr, &val);

	count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
	return count;
}

static int temp_max_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	char data_limetime1[32];
	int ret = 0;

	memset(data_limetime1, 0, sizeof(data_limetime1));

	ret = fg_mac_read_block(gm, FG_MAC_CMD_LIFETIME1, data_limetime1, sizeof(data_limetime1));
	if (ret)
		fg_err("failed to get FG_MAC_CMD_LIFETIME1\n");
	*val = data_limetime1[6];

	fg_err("%s %d\n", __func__, *val);
	return 0;
}

static int time_ot_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	char data_limetime3[32];
	char data[32];
	int ret = 0;

	memset(data_limetime3, 0, sizeof(data_limetime3));
	memset(data, 0, sizeof(data));

	ret = fg_mac_read_block(gm, FG_MAC_CMD_LIFETIME3, data_limetime3, sizeof(data_limetime3));
	if (ret)
		fg_err("failed to get FG_MAC_CMD_LIFETIME3\n");

	ret = fg_mac_read_block(gm, FG_MAC_CMD_MANU_NAME, data, sizeof(data));
	if (ret)
		fg_err("failed to get FG_MAC_CMD_MANU_NAME\n");

	if (data[2] == 'C') //TI
	{
		ret = fg_mac_read_block(gm, FG_MAC_CMD_FW_VER, data, sizeof(data));
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

int isc_alert_level_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u8 isc_alert_level = 0;

	if(gm->device_name != BQ_FG_NFG1000A && gm->device_name != BQ_FG_NFG1000B)
	{
		fg_err("%s: this Bq_Fg is not support this function.\n", __func__);
		return -1;
	}

	ret = fg_read_byte(gm, gm->regs[NVT_FG_REG_ISC], &isc_alert_level);

	if(ret < 0)
	{
		fg_err("%s: read isc_alert_level occur error.\n", __func__);
		return ret;
	}
	*val = isc_alert_level;
	fg_err("%s:now isc:%d.\n", __func__, *val);
	return ret;
}

int soa_alert_level_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u8 soa_alert_level = 0;

	if(gm->device_name != BQ_FG_NFG1000A && gm->device_name != BQ_FG_NFG1000B)
	{
		fg_err("%s: this Bq_Fg is not support this function.\n", __func__);
		return -1;
	}

	ret = fg_read_byte(gm,gm->regs[NVT_FG_REG_SOA_L], &soa_alert_level);

	if(ret < 0)
	{
		fg_err("%s: read soa_alert_level occur error.\n", __func__);
		return ret;
	}
	*val = soa_alert_level;
	fg_err("%s:now soa:%d.\n", __func__, *val);
	return ret;
}

/* Must be in the same order as BMS_PROP_* */
static struct mtk_bms_sysfs_field_info bms_sysfs_field_tbl[] = {
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
	BMS_SYSFS_FIELD_RO(shutdown_voltage, BMS_PROP_SHUTDOWN_VOL),
	BMS_SYSFS_FIELD_RW(charge_eoc, BMS_PROP_CHARGE_EOC),
};

int bms_get_property(enum bms_property bp,
			    int *val)
{
	struct bq_fg_chip *gm;
	struct power_supply *psy;

	psy = power_supply_get_by_name("bms");
	if (psy == NULL)
		return -ENODEV;

	gm = (struct bq_fg_chip *)power_supply_get_drvdata(psy);
	if (bms_sysfs_field_tbl[bp].prop == bp)
		bms_sysfs_field_tbl[bp].get(gm,
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
	struct bq_fg_chip *gm;
	struct power_supply *psy;

	psy = power_supply_get_by_name("bms");
	if (psy == NULL)
		return -ENODEV;

	gm = (struct bq_fg_chip *)power_supply_get_drvdata(psy);

	if (bms_sysfs_field_tbl[bp].prop == bp)
		bms_sysfs_field_tbl[bp].set(gm,
			&bms_sysfs_field_tbl[bp], val);
	else {
		fg_err("%s usb bp:%d idx error\n", __func__, bp);
		return -ENOTSUPP;
	}
	return 0;
}
EXPORT_SYMBOL(bms_set_property);

static struct attribute *
	bms_sysfs_attrs[ARRAY_SIZE(bms_sysfs_field_tbl) + 1];

static const struct attribute_group bms_sysfs_attr_group = {
	.attrs = bms_sysfs_attrs,
};

static void bms_sysfs_init_attrs(void)
{
	int i, limit = ARRAY_SIZE(bms_sysfs_field_tbl);

	for (i = 0; i < limit; i++)
		bms_sysfs_attrs[i] = &bms_sysfs_field_tbl[i].attr.attr;

	bms_sysfs_attrs[limit] = NULL; /* Has additional entry for this */
}

static int bms_sysfs_create_group(struct power_supply *psy)
{
	bms_sysfs_init_attrs();

	return sysfs_create_group(&psy->dev.kobj,
			&bms_sysfs_attr_group);
}

static enum power_supply_property fg_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
};

#define SHUTDOWN_DELAY_VOL	3300
#define SHUTDOWN_VOL	3400
static int fg_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val)
{
	struct bq_fg_chip *bq = power_supply_get_drvdata(psy);
	static bool last_shutdown_delay = false;
	union power_supply_propval pval = {0, };
	int tem;

	switch (psp) {
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = bq->model_name;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		mutex_lock(&bq->data_lock);
		fg_read_volt(bq);
		val->intval = bq->cell_voltage[2] * 1000;
		mutex_unlock(&bq->data_lock);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		mutex_lock(&bq->data_lock);
		bq->ibat = fg_read_current(bq);
		val->intval = bq->ibat * 1000;
		mutex_unlock(&bq->data_lock);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (bq->fake_soc) {
			val->intval = bq->fake_soc;
			break;
		}

		if (bq->i2c_error_count >= 1) {
			val->intval = 15;
			break;
		}

		val->intval = bq->ui_soc;
		//add shutdown delay feature
		if (bq->enable_shutdown_delay) {
			if (val->intval == 0) {
				tem = bq->tbat;
				if (!battery_get_psy(bq)) {
					fg_err("%s get capacity failed to get battery psy\n", bq->log_tag);
					break;
				} else
					power_supply_get_property(bq->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
				if (pval.intval != POWER_SUPPLY_STATUS_CHARGING) {
					if(bq->shutdown_delay == true) {
						val->intval = 1;
					} else if (((tem > 0 && bq->cell_voltage[2] >= bq->critical_shutdown_vbat)
						|| (tem <= 0 && bq->cell_voltage[2] >= bq->cool_critical_shutdown_vbat)) &&
							bq->shutdown_flag == false) {
						bq->shutdown_delay = true;
						val->intval = 1;
					} else {
						bq->shutdown_delay = false;
					}
					fg_err("%s last_shutdown= %d. shutdown= %d, soc =%d, voltage =%d\n", bq->log_tag, last_shutdown_delay, bq->shutdown_delay, val->intval, bq->cell_voltage[2]);
				} else {
					bq->shutdown_delay = false;
					if ((((tem > 0) && (bq->cell_voltage[2] >= (bq->critical_shutdown_vbat - 40)))
						|| ((tem < 0) && (bq->cell_voltage[2] >= (bq->cool_critical_shutdown_vbat - 40)))
						|| ((bq->cycle_count > 200) && (bq->cell_voltage[2] >= (bq->old_critical_shutdown_vbat - 40)))) &&
							bq->shutdown_flag == false) {
						val->intval = 1;
					}
				}
			} else {
				bq->shutdown_delay = false;
			}

			if (val->intval <= 0)
				bq->shutdown_flag = true;
			else
				bq->shutdown_flag = false;

			if (bq->shutdown_flag)
				val->intval = 0;

			if (last_shutdown_delay != bq->shutdown_delay || val->intval == 0) {
				last_shutdown_delay = bq->shutdown_delay;
				if (bq->fg_psy)
					power_supply_changed(bq->fg_psy);
				fg_err("%s power_supply_changed\n", bq->log_tag);
			}
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (bq->fake_tbat) {
			val->intval = bq->fake_tbat;
			break;
		}
		val->intval = bq->tbat;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (bq->device_name == BQ_FG_BQ27Z561 || bq->device_name == BQ_FG_NFG1000A || bq->device_name == BQ_FG_NFG1000B)
			val->intval = bq->fcc;
		else if (bq->device_name == BQ_FG_BQ28Z610)
			val->intval = bq->fcc * 2;
		else
			val->intval = 4500;
		val->intval *= 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		if (bq->device_name == BQ_FG_BQ27Z561 || bq->device_name == BQ_FG_NFG1000A || bq->device_name == BQ_FG_NFG1000B)
			val->intval = bq->dc;
		else if (bq->device_name == BQ_FG_BQ28Z610)
			val->intval = bq->dc * 2;
		else
			val->intval = 4500;
		val->intval *= 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = bq->rm * 1000;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = bq->cycle_count;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
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
	default:
		return -EINVAL;
	}

	return 0;
}

static char *mtk_bms_supplied_to[] = {
        "battery",
        "usb",
};

static int fg_prop_is_writeable(struct power_supply *psy, enum power_supply_property prop)
{
	int ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static int fg_init_psy(struct bq_fg_chip *bq)
{
	struct power_supply_config fg_psy_cfg = {};

	bq->fg_psy_d.name = "bms";
	bq->fg_psy_d.type = POWER_SUPPLY_TYPE_BATTERY;
	bq->fg_psy_d.properties = fg_props;
	bq->fg_psy_d.num_properties = ARRAY_SIZE(fg_props);
	bq->fg_psy_d.get_property = fg_get_property;
	bq->fg_psy_d.set_property = fg_set_property;
	bq->fg_psy_d.property_is_writeable = fg_prop_is_writeable;
	fg_psy_cfg.supplied_to = mtk_bms_supplied_to;
	fg_psy_cfg.num_supplicants = ARRAY_SIZE(mtk_bms_supplied_to);
	fg_psy_cfg.drv_data = bq;

	bq->fg_psy = devm_power_supply_register(bq->dev, &bq->fg_psy_d, &fg_psy_cfg);
	if (IS_ERR(bq->fg_psy)) {
		fg_err("%s failed to register fg_psy", bq->log_tag);
		return PTR_ERR(bq->fg_psy);
	} else
	    bms_sysfs_create_group(bq->fg_psy);

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

	if (bq->device_name == BQ_FG_BQ27Z561 || bq->device_name == BQ_FG_NFG1000A || bq->device_name == BQ_FG_NFG1000B) {
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
	if (bq->device_name == BQ_FG_BQ27Z561 || bq->device_name == BQ_FG_NFG1000A || bq->device_name == BQ_FG_NFG1000B)
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

static DEVICE_ATTR(fcc, S_IRUGO, fg_show_fcc, NULL);
static DEVICE_ATTR(rm, S_IRUGO, fg_show_rm, NULL);
static DEVICE_ATTR(rsoc, S_IRUGO, fg_show_rsoc, NULL);
static DEVICE_ATTR(cell0_voltage, S_IRUGO, fg_show_cell0_voltage, NULL);
static DEVICE_ATTR(cell1_voltage, S_IRUGO, fg_show_cell1_voltage, NULL);
static DEVICE_ATTR(qmax0, S_IRUGO, fg_show_qmax0, NULL);
static DEVICE_ATTR(qmax1, S_IRUGO, fg_show_qmax1, NULL);
static DEVICE_ATTR(verify_digest, S_IRUGO | S_IWUSR, fg_verify_digest_show, fg_verify_digest_store);
static DEVICE_ATTR(log_level, S_IRUGO | S_IWUSR, fg_show_log_level, fg_store_log_level);

static struct attribute *fg_attributes[] = {
	&dev_attr_rm.attr,
	&dev_attr_fcc.attr,
	&dev_attr_rsoc.attr,
	&dev_attr_cell0_voltage.attr,
	&dev_attr_cell1_voltage.attr,
	&dev_attr_qmax0.attr,
	&dev_attr_qmax1.attr,
	&dev_attr_verify_digest.attr,
	&dev_attr_log_level.attr,
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

        bq->slave_connect_gpio = of_get_named_gpio(node, "slave_connect_gpio", 0);
          fg_err("%s slave_connect_gpio = %d \n", bq->log_tag, bq->slave_connect_gpio );    
          if (!gpio_is_valid(bq->slave_connect_gpio)) {
                 fg_info("failed to parse slave_connect_gpio\n");
                 return -1;
          }

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

	if (product_name == DUCHAMP_CN || product_name == DUCHAMP_GL) {
		bq->normal_shutdown_vbat = bq->shutdown_voltage;
		bq->critical_shutdown_vbat = bq->shutdown_voltage - 60;
		bq->cool_critical_shutdown_vbat = bq->shutdown_voltage - 60;
		bq->old_critical_shutdown_vbat = bq->shutdown_voltage - 60;
	}

	ret = of_property_read_u32(node, "report_full_rsoc_1s", &bq->report_full_rsoc);
	if (ret)
		fg_err("%s failed to parse report_full_rsoc_1s\n", bq->log_tag);
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
	int ret = 0, i = 0;

	static int Adapting_Power[20] = {10, 15, 18, 25, 33, 35, 40,
		55, 60, 67, 80, 90, 100, 120, 140, 160, 180, 200, 220, 240};

	/* FG EEPROM Coding Rule V0.04 Update */
	ret = fg_mac_read_block(bq, FG_MAC_CMD_MANU_NAME, data, 32);
	if (ret) {
		fg_info("%s: failed to get FG_MAC_CMD_MANU_NAME, ret=%d\n", __func__,ret);
		fg_err("%s: FG_MAC_CMD_MANU_NAME: %s\n", __func__, data);
	} else {
		fg_err("%s: FG_MAC_CMD_MANU_NAME: %s\n", __func__, data);
	}

	if (!strncmp(data, "MI", 2)) {
		bq->chip_ok = true;

		if (!strncmp(&data[2], "4", 1)) {
			bq->device_name = BQ_FG_NFG1000B;
			strcpy(bq->model_name, "nfg1000b");
			strcpy(bq->log_tag, "[XMCHG_NFG1000B]");
		} else if (!strncmp(&data[2], "5", 1)) {
			bq->device_name = BQ_FG_NFG1000A;
			strcpy(bq->model_name, "nfg1000a");
			strcpy(bq->log_tag, "[XMCHG_NFG1000A]");
		} else if (!strncmp(&data[2], "C", 1)) {
			bq->device_name = BQ_FG_BQ27Z561;
			strcpy(bq->model_name, "bq27z561");
			strcpy(bq->log_tag, "[XMCHG_BQ27Z561]");
		} else if (!strncmp(&data[2], "D", 1)) {
			bq->device_name = BQ_FG_BQ30Z55;
			strcpy(bq->model_name, "bq30z55");
			strcpy(bq->log_tag, "[XMCHG_BQ30Z55]");
		} else if (!strncmp(&data[2], "E", 1)) {
			bq->device_name = BQ_FG_BQ40Z50;
			strcpy(bq->model_name, "bq40z50");
			strcpy(bq->log_tag, "[XMCHG_BQ40Z50]");
		} else if (!strncmp(&data[2], "F", 1)) {
			bq->device_name = BQ_FG_BQ27Z746;
			strcpy(bq->model_name, "bq27z746");
			strcpy(bq->log_tag, "[XMCHG_BQ27Z746]");
		} else if (!strncmp(&data[2], "G", 1)) {
			bq->device_name = BQ_FG_BQ28Z610;
			strcpy(bq->model_name, "bq28z610");
			strcpy(bq->log_tag, "[XMCHG_BQ28Z610]");
		} else if (!strncmp(&data[2], "H", 1)) {
			bq->device_name = BQ_FG_MAX1789;
			strcpy(bq->model_name, "max1789");
			strcpy(bq->log_tag, "[XMCHG_MAX1789]");
		} else if (!strncmp(&data[2], "I", 1)) {
			bq->device_name = BQ_FG_RAA241200;
			strcpy(bq->model_name, "raa241200");
			strcpy(bq->log_tag, "[XMCHG_RAA241200]");
		} else {
			bq->device_name = BQ_FG_UNKNOWN;
			strcpy(bq->model_name, "UNKNOWN");
			strcpy(bq->log_tag, "[XMCHG_UNKNOWN_FG]");
			bq->chip_ok = false;
			fg_info("%s: failed to get MI fg.\n", __func__);
		}

		if(!strncmp(&data[14], "I", 1))
		{
			bq->shutdown_voltage = 3400;
		}else if(!strncmp(&data[14], "E", 1))
		{
			bq->shutdown_voltage = 3200;
		}else{
			bq->shutdown_voltage = 3400;
		}
		fg_info("%s: get shutdown voltage=%d.\n", __func__, bq->shutdown_voltage);
		if (bq->chip_ok) {
			i = data[4];
			if ((i >= 0) && (i <= 19)) {
				bq->adapting_power = Adapting_Power[i];
				fg_info("%s: adapting_power:%d.\n", __func__, bq->adapting_power);
			}
		}
	} else {
		bq->chip_ok = false;
		fg_info("failed to get MI fg.\n");
	}

	ret = fg_mac_read_block(bq, FG_MAC_CMD_DEVICE_CHEM, data, 32);
	if (ret) {
		fg_info("failed to get FG_MAC_CMD_DEVICE_CHEM\n");
		fg_err("%s: FG_MAC_CMD_DEVICE_CHEM: %s\n", __func__, data);
	} else {
		fg_err("%s: FG_MAC_CMD_DEVICE_CHEM: %s\n", __func__, data);
	}

	if (!strncmp(&data[1], "L", 1) && bq->device_name != BQ_FG_UNKNOWN) {
		bq->cell_supplier = BMS_CELL_LWN;
		strcpy(bq->device_chem, "LWN");
	} else if(!strncmp(&data[1], "F", 1) && bq->device_name != BQ_FG_UNKNOWN) {
		bq->cell_supplier = BMS_CELL_ATL;
		strcpy(bq->device_chem, "ATL");
	} else if(!strncmp(&data[1], "J", 1) && bq->device_name != BQ_FG_UNKNOWN) {
		bq->cell_supplier = BMS_CELL_COS;
		strcpy(bq->device_chem, "COS");
	} else {
		bq->cell_supplier = BMS_CELL_UNKNOWN;
		strcpy(bq->device_chem, "UNKNOWN");
	}

	ret = fg_mac_read_block(bq, FG_MAC_CMD_DEVICE_NAME, data, 32);
	if (ret) {
		fg_err("failed to get FG_MAC_CMD_DEVICE_NAME\n");
		fg_err("%s: FG_MAC_CMD_DEVICE_NAME: %s\n", __func__, data);
	} else {
		fg_err("%s: FG_MAC_CMD_DEVICE_NAME: %s\n", __func__, data);
	}

	return ret;
}

static int fg_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct bq_fg_chip *bq;
	int ret = 0;
	char *name = NULL;
	u8 data[5] = {0};

	product_name = DUCHAMP_CN;
	fg_info("%s: product_name=%d\n", __func__, product_name);

	fg_info("FG probe enter\n");
	bq = devm_kzalloc(&client->dev, sizeof(*bq), GFP_DMA);
	if (!bq)
		return -ENOMEM;

	bq->dev = &client->dev;
	bq->client = client;
	bq->monitor_delay = FG_MONITOR_DELAY_30S;

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

	fg_check_device(bq);

	ret = fg_parse_dt(bq);
	if (ret) {
		fg_err("%s failed to parse DTS\n", bq->log_tag);
		return ret;
	}

	time_init = ktime_get();
	fg_update_status(bq);

	ret = fg_init_psy(bq);
	if (ret) {
		fg_err("%s failed to init psy\n", bq->log_tag);
		return ret;
	}

	ret = sysfs_create_group(&bq->dev->kobj, &fg_attr_group);
	if (ret) {
		fg_err("%s failed to register sysfs\n", bq->log_tag);
		return ret;
	}

	bq->update_now = true;
	INIT_DELAYED_WORK(&bq->monitor_work, fg_monitor_workfunc);
	schedule_delayed_work(&bq->monitor_work, msecs_to_jiffies(5000));

	bq->dc = fg_read_dc(bq);

	/* init fast charge mode */
	data[0] = 0;
	fg_err("-fastcharge init-\n");
	ret = fg_mac_write_block(bq, FG_MAC_CMD_FASTCHARGE_DIS, data, 2);
	if (ret) {
		fg_err("%s failed to write fastcharge = %d\n", bq->log_tag, ret);
	}

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
