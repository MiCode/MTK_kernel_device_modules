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
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/mca/common/mca_log.h>

#include "mpc8011b.h"
#include "mtk_battery.h"
#include "fuelgauge_class.h"

#ifndef MCA_LOG_TAG
#define MCA_LOG_TAG "fuelgauge_8011b"
#endif


enum product_name {
	PRODUCT_NO,
	RODIN_CN,
	RODIN_GL,
	RODIN_IN,
};

static int log_level = 1;
static int product_name = PRODUCT_NO;
static ktime_t probe_time = -1;

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

static int get_median(int data);

static int __fg_read_byte(struct i2c_client *client, u8 reg, u8 *val)
{
	int ret = 0;

	ret =  i2c_smbus_read_byte_data(client, reg);
	if(ret < 0)
	{
		mca_log_info("i2c read byte failed: can't read from reg 0x%02X faild\n", reg);
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

	ret = regmap_raw_read(bq->regmap, reg, data, 2);
	if (ret) {
		mca_log_err("%s I2C failed to read 0x%02x\n", bq->log_tag, reg);
		return ret;
	}

	*val = (data[1] << 8) | data[0];

	if (reg == bq->regs[BQ_FG_REG_TEMP]) {
		mca_log_err("%s: reg=0x%02x, val=0x%02x,0x%02x\n", bq->log_tag,
			reg, data[1], data[0]);
	}

	return ret;
}

static int fg_read_block(struct bq_fg_chip *bq, u8 reg, u8 *buf, u8 len)
{
	int ret = 0, i = 0;
	unsigned int data = 0;

	for (i = 0; i < len; i++) {
		ret = regmap_read(bq->regmap, reg + i, &data);
		if (ret) {
			mca_log_info("%s I2C failed to read 0x%02x\n", bq->log_tag, reg + i);
			return ret;
		}
		buf[i] = data;
	}

	return ret;
}

static int fg_write_block(struct bq_fg_chip *bq, u8 reg, u8 *data, u8 len)
{
	int ret = 0, i = 0;

	for (i = 0; i < len; i++) {
		ret = regmap_write(bq->regmap, reg + i, (unsigned int)data[i]);
		if (ret) {
			mca_log_err("%s I2C failed to write 0x%02x\n", bq->log_tag, reg + i);
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
		// mca_log_err("%s:len=%d,sum=%d,data[%d]=%d\n", bq->log_tag, len, sum, i, data[i]);
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
		mca_log_err("%s failed to checksum\n", bq->log_tag);
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
		mca_log_err("%s failed to write block\n", bq->log_tag);
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
			mca_log_err("%s failed to read RSOC\n", bq->log_tag);
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
	bq->rsoc = soc;
	return soc;
}

static int fg_read_real_temp(struct bq_fg_chip *bq)
{
	u16 tbat = 0;
	u8 data[64] = {0};
	int ret = 0;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_REAL_TEMP, data, 20);
	if (ret < 0)
		mca_log_err("%s failed to read REAL_TEMP\n", bq->log_tag);
	tbat = (data[1] << 8) | data[0];

	mca_log_err("%s read FG real TBAT = %d\n", bq->log_tag, tbat);

	return tbat - 2730;
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
			mca_log_err("%s failed to read TBAT\n", bq->log_tag);
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
	if (!tbat)
		tbat = 2980;

//#ifndef CONFIG_FACTORY_BUILD
	/*
	 * in very low rate BatteryTemperature read from fg is very large above 100 degree
	 * use workaround here to avoid high temp shutdown by mistaken
	 */
	if (tbat >= 3730) {
		tbat = 3180;
		mca_log_err("%s FG TBAT abnormal force 45C \n", bq->log_tag);
	}
//#endif
	bq->tbat = get_median(tbat - 2730);
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
			mca_log_err("%s failed to read cell voltage\n", bq->log_tag);
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
	static int pre_vbat = 0;
	int temp_vbat = 0;

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_VOLT], &vbat);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			mca_log_err("%s failed to read VBAT\n", bq->log_tag);
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

	temp_vbat = (int)vbat;
	if (temp_vbat >= 5000) {
		mca_log_err("%s VBAT=%d abnormal using pre value\n", bq->log_tag, temp_vbat);
		bq->vbat = pre_vbat;
	} else {
		bq->vbat = temp_vbat;
		pre_vbat = bq->vbat;
	}

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
			mca_log_err("%s failed to read AVG_IBAT\n", bq->log_tag);
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
			mca_log_err("%s failed to read IBAT\n", bq->log_tag);
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
	bq->ibat = ibat;
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
                  	mca_log_err("%s failed to read FCC,FCC=%d\n", bq->log_tag, fcc);
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
            mca_log_err("%s failed to read RM,RM=%d\n", bq->log_tag, rm);
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
			mca_log_err("%s failed to read DC\n", bq->log_tag);
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

	if (bq->fake_soh != 0) {
		soh = bq->fake_soh;
		return soh;
	}

retry:
	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_SOH], &soh);
	if (ret < 0) {
		if (!retry) {
			retry = true;
			msleep(10);
			goto retry;
		} else {
			mca_log_err("%s failed to read SOH\n", bq->log_tag);
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
	bq->soh = soh;
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
			mca_log_err("%s failed to read CV\n", bq->log_tag);
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
			mca_log_err("%s failed to read CC\n", bq->log_tag);
			cc = 11000;
		}
	}

	return cc;
}
#endif

void fg_aged_prediction(struct bq_fg_chip *bq,
	u32 cyclecount)
{
    static u32 soh_threashold = 92;
    static u32 cls_threashlod = 400;
    static u32 cls_step       = 50;
    if (cyclecount > cls_threashlod) {
        soh_threashold -= 1;
        cls_threashlod += cls_step;
        bms_set_property(BMS_PROP_AGED_IN_ADVANCE, false);
    }

    if (bq->ui_soh < soh_threashold) {
        bms_set_property(BMS_PROP_AGED_IN_ADVANCE, true);
        mca_log_info("%s: use 800 scheme in advance, ui_soh:%d, soh_threashold:%d, cycle_count:%d",
           bq->log_tag, bq->ui_soh, soh_threashold, cyclecount);
    }
}

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
			mca_log_err("%s failed to read CC\n", bq->log_tag);
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

	fg_aged_prediction(bq, cc);
	bq->cycle_count = cc;
	return cc;
}

static int fg_get_raw_soc(struct bq_fg_chip *bq)
{
	int raw_soc = 0;

	bq->rm = fg_read_rm(bq);
	bq->fcc = fg_read_fcc(bq);

	raw_soc = bq->rm * 10000 / bq->fcc;
	bq->raw_soc = raw_soc;

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

	return raw_soc % 100;
}

static void fg_read_qmax(struct bq_fg_chip *bq)
{
	u8 data[64] = {0};
	int ret = 0;

	if (bq->device_name == BQ_FG_BQ27Z561 || bq->device_name == BQ_FG_NFG1000A || bq->device_name == BQ_FG_NFG1000B) {
		ret = fg_mac_read_block(bq, FG_MAC_CMD_QMAX, data, 14);
		if (ret < 0)
			mca_log_err("%s failed to read MAC\n", bq->log_tag);
	} else if (bq->device_name == BQ_FG_BQ28Z610) {
		ret = fg_mac_read_block(bq, FG_MAC_CMD_QMAX, data, 20);
		if (ret < 0)
			mca_log_err("%s failed to read MAC\n", bq->log_tag);
	} else {
		mca_log_err("%s not support device name\n", bq->log_tag);
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
			mca_log_err("%s failed to write fastcharge = %d\n", bq->log_tag, ret);
			return ret;
		}
	} else {
		ret = fg_mac_write_block(bq, FG_MAC_CMD_FASTCHARGE_DIS, data, 2);
		if (ret) {
			mca_log_err("%s failed to write fastcharge = %d\n", bq->log_tag, ret);
			return ret;
		}
	}

	return ret;
}

static int fg_set_charge_eoc(struct bq_fg_chip *bq, bool enable)
{
	int ret = 0;
	u8 t_buf[40];

	bq->charge_eoc = enable;

	t_buf[0] = 0x31;
	t_buf[1] = 0x00;
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, 2);
	if (ret < 0)
		return ret;

	return ret;
}

__maybe_unused
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

// 67W
#define SOC_PROPORTION_67W 97
#define SOC_PROPORTION_67W_C 98
// 120W
#define SOC_PROPORTION_120W 94
#define SOC_PROPORTION_120W_C 95

#define SOC_PROPORTION_EEA    100
#define SOC_PROPORTION_EEA_C  101

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

static int fg_set_shutdown_mode(struct bq_fg_chip *bq)
{
	int ret = 0;
	u8 data[5] = {0};

	mca_log_info("%s fg_set_shutdown_mode\n", bq->log_tag);
	bq->shutdown_mode = true;

	data[0] = 1;

	ret = fg_mac_write_block(bq, FG_MAC_CMD_SHUTDOWN, data, 2);
	if (ret)
		mca_log_err("%s failed to send shutdown cmd 0\n", bq->log_tag);

	ret = fg_mac_write_block(bq, FG_MAC_CMD_SHUTDOWN, data, 2);
	if (ret)
		mca_log_err("%s failed to send shutdown cmd 1\n", bq->log_tag);

	return ret;
}

static bool battery_get_psy(struct bq_fg_chip *bq)
{
	bq->batt_psy = power_supply_get_by_name("battery");
	if (!bq->batt_psy) {
		mca_log_err("%s failed to get batt_psy", bq->log_tag);
		return false;
	}
	return true;
}

static void run_shutdown_temp_state_machine(struct bq_fg_chip *bq)
{
	if (bq->tbat < -60) {
		bq->temp_state = BAT_COLD;
	} else if (bq->tbat < 0) {
		if (bq->temp_state <= BAT_COLD && bq->tbat < -40) {
		} else {
			bq->temp_state = BAT_LITTLE_COLD;
		}
	} else if (bq->tbat < 100) {
		if (bq->temp_state <= BAT_LITTLE_COLD && bq->tbat < 20) {
			bq->temp_state = BAT_LITTLE_COLD;
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

static int fg_count_shutdown_vol(struct bq_fg_chip *bq, int count)
{
	int count_vol = 0;

	switch(bq->temp_state) {
		case BAT_COLD:
			if (count < 121) {
				count_vol = 2800;
			} else if (count >= 121 && count < 383) {
				count_vol = 2800;
			} else if (count >= 383 && count < 557) {
				count_vol = 2900;
			} else {
				count_vol = 2950;
			}
			break;
		case BAT_LITTLE_COLD:
			if (count < 121) {
				count_vol = 2800;
			} else if (count >= 121 && count < 383) {
				count_vol = 2800;
			} else if (count >= 383 && count < 557) {
				count_vol = 2950;
			} else {
				count_vol = 3050;
			}
			break;
		case BAT_COOL:
			if (count < 121) {
				count_vol = 3000;
			} else if (count >= 121 && count < 383) {
				count_vol = 3000;
			} else if (count >= 383 && count < 557) {
				count_vol = 3100;
			} else {
				count_vol = 3200;
			}
			break;
		case BAT_NORMAL:
			if (count < 121) {
				count_vol = 3000;
			} else if (count >= 121 && count < 383) {
				count_vol = 3100;
			} else if (count >= 383 && count < 557) {
				count_vol = 3200;
			} else {
				count_vol = 3300;
			}
			break;
		default:
			count_vol = 3200;
			break;
	}

	return count_vol;
}

static int fg_cycle_shutdown_vol(struct bq_fg_chip *bq)
{
	int cycle_vol = 0;
	int cycle = 0;

	mutex_lock(&bq->data_lock);
	cycle = fg_read_cyclecount(bq);
	mutex_unlock(&bq->data_lock);

	switch(bq->temp_state) {
		case BAT_COLD:
		case BAT_LITTLE_COLD:
			if (cycle < 200) {
				cycle_vol = 2800;
			} else if (cycle >= 200 && cycle < 600) {
				cycle_vol = 2800;
			} else if (cycle >= 600 && cycle < 1200) {
				cycle_vol = 3050;
			} else {
				cycle_vol = 3150;
			}
			break;
		case BAT_COOL:
		case BAT_NORMAL:
			if (cycle < 200) {
				cycle_vol = 3000;
			} else if (cycle >= 200 && cycle < 600) {
				cycle_vol = 3000;
			} else if (cycle >= 600 && cycle < 1200) {
				cycle_vol = 3200;
			} else {
				cycle_vol = 3300;
			}
			break;
		default:
			cycle_vol = 3200;
			break;
	}

	return cycle_vol;
}

static void battery_shutdown_vol_update(struct bq_fg_chip *bq)
{
	int ret = 0;
	int shutdown_vol = 3200;
	int count_vol = 0;
	int cycle_vol = 0;
	int term_vol = 0;
	int tbat_temp = 0;
	u8 t_buf[32];
	u8 data[2] = {0xff, 0xff};
	int count_3v0 = 0, count_3v2 = 0, count = 0, count_3v4 = 0;

	mutex_lock(&bq->data_lock);
	tbat_temp = fg_read_temperature(bq);
	mutex_unlock(&bq->data_lock);
	bq->tbat = get_median(tbat_temp);
	if (product_name == RODIN_CN || product_name == RODIN_IN) {
		if (bq->fake_dod_count) {
			count = bq->fake_dod_count;
			mca_log_info("%s: fake_dod_count=%d \n", bq->log_tag, bq->fake_dod_count);
		} else {
			/* read count */
			ret = fg_mac_read_block(bq, FG_MAC_CMD_MIXDATA1, t_buf, 32);
			if (ret) {
				mca_log_err("failed to get FG_MAC_CMD_MIXDATA1\n");
			} else {
				mca_log_info("%s: FG_MAC_CMD_MIXDATA1: %s\n", bq->log_tag, t_buf);
			}
			count_3v0 = t_buf[9] << 8 | t_buf[8];
			count_3v2 = t_buf[11] << 8 | t_buf[10];
			count_3v4 = t_buf[13] << 8 | t_buf[12];
			count = count_3v0*30/10 + count_3v2*21/10 + count_3v4*14/10;
		}

		/* support dynamic shutdown voltage */
		run_shutdown_temp_state_machine(bq);
		count_vol = fg_count_shutdown_vol(bq, count);
		cycle_vol = fg_cycle_shutdown_vol(bq);
		shutdown_vol = max(count_vol, cycle_vol);
		mca_log_info("%s: count=%d,%d,%d,%d vol=%d,%d,%d\n", bq->log_tag,
				count, count_3v0, count_3v2, count_3v4,
				shutdown_vol, count_vol, cycle_vol);

		/* set shutdown_vol */
		if (shutdown_vol != bq->shutdown_voltage) {
			if (bq->tbat < 0) {
				term_vol = shutdown_vol + 100;
			} else {
				term_vol = shutdown_vol + 50;
			}
			if (shutdown_vol >= 3300) {
				term_vol = shutdown_vol + 40;
			}
			data[0] = term_vol & 0xFF;
			data[1] = (term_vol >> 8) & 0xFF;
			ret = fg_mac_write_block(bq, FG_MAC_CMD_MIX_TERM_VOLT, data, 2);
			if (ret) {
				mca_log_err("%s:dynamic_shutdown_vol failed to write FG_MAC_CMD_MIX_TERM_VOLT\n", bq->log_tag);
			}

			ret = fg_mac_read_block(bq, FG_MAC_CMD_MIX_TERM_VOLT, t_buf, 2);
			mca_log_info("%s:dynamic_shutdown_vol t_buf[0]=0x%02x t_buf[1]=0x%02x FG_MAC_CMD_MIX_TERM_VOLT=%d\n", bq->log_tag,
					t_buf[0], t_buf[1], t_buf[1] << 8 | t_buf[0]);
		}
	} else {
		shutdown_vol = 3200;
	}

	bq->shutdown_voltage = shutdown_vol;
	bq->normal_shutdown_vbat = bq->shutdown_voltage;
	if (shutdown_vol >= 3300) {
		bq->critical_shutdown_vbat = bq->shutdown_voltage + 40;
	} else {
		bq->critical_shutdown_vbat = bq->shutdown_voltage + 50;
	}
	bq->cool_critical_shutdown_vbat = bq->shutdown_voltage + 50;
	bq->old_critical_shutdown_vbat = bq->shutdown_voltage + 50;

	mca_log_info("%s: shutdown voltage=%d,%d,%d,%d,%d state=%d,%d-%d\n", bq->log_tag, bq->shutdown_voltage, bq->normal_shutdown_vbat,
		bq->critical_shutdown_vbat, bq->cool_critical_shutdown_vbat, bq->old_critical_shutdown_vbat, bq->tbat, bq->temp_state, bq->cycle_count);
}

static int fg_unseal_send_key_for_nvt(struct bq_fg_chip *bq)
{
	int ret = 0;
	u8 key1[] = {0x0B,0x02};
	u8 key2[] = {0x02,0x55};

	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], key1, 2);
	if (ret < 0) {
		mca_log_err("step 1 first word write fail\n");
		return ret;
	}

	msleep(300);

	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], key2, 2);
	if (ret < 0) {
		mca_log_err(" step 2 second word write fail");
		return ret;
	}

	msleep(300);
	mca_log_err("%s fg unseal success\n", bq->log_tag);
	return ret;
}

#define SEAL_STATUS_MASK 0x03
static int fg_get_seal(struct bq_fg_chip *bq, int *value)
{
	int ret;
	u8 t_buf[64] = {0};

	ret = fg_mac_read_block(bq, FG_MAC_CMD_SEAL_STATE, t_buf, 32);
	if (ret < 0)
		return ret;

	*value = t_buf[1] & SEAL_STATUS_MASK;

	return ret;

}

#define UNSEAL_COUNT_MAX 3
static int fg_set_seal(struct bq_fg_chip *bq, int value)
{
	int ret;
	int seal_status = 0;
	int count = 0;
	u8 t_buf[] = {0x30,0x00};

	if (value) {
		ret = fg_get_seal(bq, &seal_status);
		if (seal_status == SEAL_STATE_FA || seal_status == SEAL_STATE_UNSEALED) {
			mca_log_err("%s: FG is unsealed", bq->log_tag);
			return 0;
		}
		while (count++ < UNSEAL_COUNT_MAX) {
			ret = fg_unseal_send_key_for_nvt(bq);
			ret = fg_get_seal(bq, &seal_status);
			if (seal_status == SEAL_STATE_FA || seal_status == SEAL_STATE_UNSEALED) {
				mca_log_err("%s FG is unsealed", bq->log_tag);
				break;
			}
			msleep(100);
		}
	} else {
		if (seal_status == SEAL_STATE_SEALED) {
			mca_log_err("%s FG is sealed", bq->log_tag);
			return 0;
		}
		while (count++ < UNSEAL_COUNT_MAX) {
			ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, 2);
			if (ret < 0)
				mca_log_err("%s Failed to send seal command", bq->log_tag);
			ret = fg_get_seal(bq, &seal_status);
			if (seal_status == SEAL_STATE_SEALED) {
				mca_log_err("%s FG is sealed", bq->log_tag);
				break;
			}
			msleep(100);
		}
	}


	return ret;
}

#define THE_10_BYTE 0xB2
#define THE_11_BYTE 0x0C
#define THE_12_BYTE 0x66
#define THE_13_BYTE 0x0D
#define VERSION_NUMBER 0x43
#define DF_CHECK_FIRST 0X5f
#define DF_CHECK_SECOND 0X80
#define MAX_RETRY_FOR_UPDATE_FW 0x03
static int fg_update_record_voltage_level(struct bq_fg_chip *bq)
{
	int ret = 0;
	u8 t_buf[36] = {0};
	int seal_status = 0;
	int count = 0;
	int address = 10;
	bool check_status = 0;
	/* count1:3.05v count2:3.25v count3:3.43v */
	u8 record_voltage[] = {0xB2,0x0C,0x66,0x0D,0x88,0x13};
	int byte_length = 6;

	if (product_name == RODIN_CN || product_name == RODIN_IN) {
		mca_log_err("%s product_name=%d start work\n", bq->log_tag, product_name);
	} else {
		mca_log_err("%s product_name=%d do nothing\n", bq->log_tag, product_name);
		return 0;
	}

	/*First if updated*/
	ret = fg_mac_read_block(bq, FG_MAC_CMD_DEVICE_CHEM, t_buf, 6);
	if (t_buf[3] >= VERSION_NUMBER) {
		mca_log_err("%s it has been upteded version_number[0x%x]\n", bq->log_tag, t_buf[3]);
		return 0;
	}
	/*Second Update Fw*/
	ret = fg_set_seal(bq, true);
	ret |= fg_get_seal(bq, &seal_status);
	if (seal_status != SEAL_STATE_UNSEALED && seal_status != SEAL_STATE_FA) {
		mca_log_err("%s Fg Unseal Failed\n", bq->log_tag);
		return ret;
	}

	/*Modify Vcutoff*/
	while (count++ < MAX_RETRY_FOR_UPDATE_FW) {
		memset(t_buf, 0, sizeof(t_buf));
		check_status = false;
		ret = fg_mac_read_block(bq, FG_MAC_CMD_RECORD_VOLTAGE_LEVEL, t_buf, 32);
		if (ret < 0) {
			mca_log_err("%s could not read record_voltage_level, ret=%d\n", bq->log_tag, ret);
			msleep(100);
			continue;
		}
		for (int i = 0; i < byte_length; i++) {
			t_buf[address + i] = record_voltage[i];
			mca_log_err("%s Reg:t_buf[%d]=[0x%x]\n", bq->log_tag, address + i, t_buf[address + i]);
		}

		ret = fg_mac_write_block(bq, FG_MAC_CMD_RECORD_VOLTAGE_LEVEL, t_buf, 32);
		if (ret < 0) {
			mca_log_err("%s could not write record_voltage_level, ret=%d\n", bq->log_tag, ret);
			msleep(100);
			continue;
		}

		memset(t_buf, 0, sizeof(t_buf));
		ret = fg_mac_read_block(bq, FG_MAC_CMD_RECORD_VOLTAGE_LEVEL, t_buf, 32);
		if (ret < 0) {
			mca_log_err("%s could not read record_voltage_level, ret=%d\n", bq->log_tag, ret);
			msleep(100);
			continue;
		}

		for (int i = 0; i < byte_length; i++) {
			if (t_buf[address + i] != record_voltage[i]) {
				check_status = true;
				break;
			}
			mca_log_err("%s Update:t_buf[%d]=[0x%x]\n", bq->log_tag, address + i, t_buf[address + i]);
		}

		if (check_status) {
			mca_log_err("%s Modify record_voltage_level Error\n", bq->log_tag);
			msleep(100);
			continue;
		} else {
			mca_log_err("%s Modify record_voltage_level Suceess\n", bq->log_tag);
			break;
		}
	}

	if (count >= MAX_RETRY_FOR_UPDATE_FW) {
		goto error;
	}

	count = 0;
	/*Modify Version Number*/
	while (count++ < MAX_RETRY_FOR_UPDATE_FW) {
		memset(t_buf, 0, sizeof(t_buf));
		ret = fg_mac_read_block(bq, FG_MAC_CMD_UPDATE_VERSION, t_buf, 32);
		t_buf[3] = VERSION_NUMBER;
		ret = fg_mac_write_block(bq, FG_MAC_CMD_UPDATE_VERSION, t_buf, 32);
		if (ret < 0) {
			mca_log_err("%s could not write update_version, ret=%d\n", bq->log_tag, ret);
			msleep(100);
			continue;
		}
		memset(t_buf, 0, sizeof(t_buf));
		ret = fg_mac_read_block(bq, FG_MAC_CMD_UPDATE_VERSION, t_buf, 32);
		if (ret < 0) {
			mca_log_err("%s could not read update_version, ret=%d\n", bq->log_tag,  ret);
			msleep(100);
			continue;
		}

		if (t_buf[3] != VERSION_NUMBER) {
			mca_log_err("%s Modify update_version Error\n", bq->log_tag);
			msleep(100);
			continue;
		} else {
			mca_log_err("%s Modify update_version Sucess\n", bq->log_tag);
			break;
		}
	}

	if (count >= MAX_RETRY_FOR_UPDATE_FW) {
		goto error;
	}

error:
	fg_set_seal(bq, false);
	return ret;
}

#define SIZE 3
static int get_median(int data) {
	static int queue[SIZE] = {25,25,25};
	static bool init_temp = true;
	int arr[SIZE] = {25,25,25};
	int i = 0, j = 0;
	int size = SIZE;

	if (init_temp) {
		for(j = 0; j < size; j++) {
			queue[j] = data;
		}
		init_temp = false;
		return data;
	}

	for(j = 0; j < size-1; j++) {
		queue[j] = queue[j+1];
	}
	queue[size-1] = data;

	for (i=0;i<size;i++) {
		arr[i] = queue[i];
	}

	for (int i = 0; i < size - 1; i++) {
		for (int j = 0; j < size - 1 - i; j++) {
			if (arr[j] > arr[j + 1]) {
				int temp = arr[j];
				arr[j] = arr[j + 1];
				arr[j + 1] = temp;
			}
		}
	}
	mca_log_info("middle_temp=%d \n", arr[size / 2]);
	return arr[size / 2];
}

static int charge_eoc_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->charge_eoc;
	else
		*val = 0;
	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int charge_eoc_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		fg_set_charge_eoc(gm, !!val);
	mca_log_err("%s %d\n", gm->log_tag, val);
	return 0;
}

static int charging_done_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->charging_done;
	else
		*val = 0;
	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int charging_done_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm) {
		gm->charging_done = !!val;
		gm->en_smooth_full = gm->charging_done;
	}
	mca_log_err("%s %d\n", gm->log_tag, val);
	return 0;
}

static int dod_count_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->fake_dod_count;
	else
		*val = 0;
	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int dod_count_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->fake_dod_count = val;
	mca_log_err("%s %d\n", gm->log_tag, val);
	return 0;
}

static int bms_get_soh_sn(struct bq_fg_chip *gm, char *buf)
{
	static u16 addr = 0x0070;
	u8 soh_sn_data[32] = {0};
	int ret = 0, i = 0;
	if(IS_ERR_OR_NULL(gm) || buf == NULL)
		return -1;
	ret = fg_mac_read_block(gm, addr, soh_sn_data, 32);
	if(ret < 0)
	{
		mca_log_err("failed to read soh_sn_data \n");
		return ret;
	}

	for (i = 0; i < sizeof(soh_sn_data); i++)
	{
		buf[i] = (char)soh_sn_data[i];
		mca_log_err("%s: buf[%d] = %d \n", gm->log_tag, i, soh_sn_data[i]);
	}
	return ret;
}

u16 fg_8bit_convert_to_16bit(u8 LSB, u8 HSB)
{
	u16 sum = 0;

	sum |= HSB;
	sum = (sum << 8) | LSB;

	return sum;
}

static int fg_show_calc_rvalue(struct bq_fg_chip *bq)
{
	static int K1 = 4500;
	static int K2 = 4500;
	static int K3 = 7000;
	static int K4 = 4500;
	static int K5 = 1000;
	static int K6 = 7000;
	static int K7 = 4500;
	static u16 addr = 0x0066;

	u16 CCcc = 0;
	u16 FFff = 0;
	u16 IIii = 0;
	u16 JJjj = 0;
	u16 MMmm = 0;
	u16 NNnn = 0;
	u16 OOoo = 0;

	int Rvalue = 0;
	u8 data_buf[40] = {0,};
	int ret = 0;

	ret = fg_mac_read_block(bq, addr, data_buf, 32);
	if (ret < 0)
	{
		mca_log_err("%s: failed to get R value raw data 0x0066\n", bq->log_tag);
		return ret;
	}

	CCcc = fg_8bit_convert_to_16bit(data_buf[4], data_buf[5]);
	FFff = fg_8bit_convert_to_16bit(data_buf[10], data_buf[11]);
	IIii = fg_8bit_convert_to_16bit(data_buf[16], data_buf[17]);
	JJjj = fg_8bit_convert_to_16bit(data_buf[18], data_buf[19]);
	MMmm = fg_8bit_convert_to_16bit(data_buf[24], data_buf[25]);
	NNnn = fg_8bit_convert_to_16bit(data_buf[26], data_buf[27]);
	OOoo = fg_8bit_convert_to_16bit(data_buf[28], data_buf[29]);

	mca_log_err("%s: cal_rvalue:CCcc %d FFff %d IIii %d JJjj %d MMmm %d NNnn %d OOoo %d \n", bq->log_tag,
											CCcc, FFff, IIii, JJjj, MMmm, NNnn, OOoo);

	Rvalue = (CCcc * K1 + FFff * K2 + IIii * K3 + JJjj * K4 + MMmm * K5 + NNnn * K6 + OOoo * K7)/1000;
	return Rvalue;
}

static int calc_rvalue_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = (u32)fg_show_calc_rvalue(gm);
	else
		*val = 0;
	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int real_temp_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = (u32)fg_read_real_temp(gm);
	else
		*val = 0;
	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int aged_in_advance_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->aged_in_advance;
	else
		*val = 0;
	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int aged_in_advance_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		 gm->aged_in_advance = val;
	mca_log_err("%s %d\n", gm->log_tag, val);
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
	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int fastcharge_mode_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		fg_set_fastcharge_mode(gm, !!val);
	mca_log_err("%s %d\n", gm->log_tag, val);
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
	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int monitor_delay_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->monitor_delay = val;
	mca_log_err("%s %d\n", gm->log_tag, val);
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
	mca_log_err("%s %d\n", gm->log_tag, *val);
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
	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int rsoc_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm){
		mutex_lock(&gm->data_lock);
		gm->rsoc = fg_read_rsoc(gm);
		mutex_unlock(&gm->data_lock);
		*val = gm->rsoc;
	} else
		*val = 0;
	mca_log_err("%s %d\n", gm->log_tag, *val);
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
	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int shutdown_delay_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->fake_shutdown_delay_enable = val;
	mca_log_err("%s %d\n", gm->log_tag, val);
	return 0;
}

static int capacity_raw_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm){
		mutex_lock(&gm->data_lock);
		gm->raw_soc = fg_get_raw_soc(gm);
		mutex_unlock(&gm->data_lock);
		*val = gm->raw_soc;
	} else
		*val = 0;
	mca_log_err("%s %d\n", gm->log_tag, *val);
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
	mca_log_err("%s %d\n", gm->log_tag, *val);
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
	mca_log_err("%s %d\n", gm->log_tag, *val);
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
	mca_log_err("%s %d\n", gm->log_tag, *val);
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
	mca_log_err("%s %d\n", gm->log_tag, *val);
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
	mca_log_err("%s %d\n", gm->log_tag, *val);
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
	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int authentic_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->authenticate = !!val;
	mca_log_err("%s %d\n", gm->log_tag, val);
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
	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int shutdown_mode_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		fg_set_shutdown_mode(gm);
	mca_log_err("%s %d\n", gm->log_tag, val);
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
	mca_log_err("%s %d\n", gm->log_tag, *val);
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
	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int charge_done_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm){
		mutex_lock(&gm->data_lock);
		fg_read_status(gm);
		mutex_unlock(&gm->data_lock);
		*val = gm->batt_fc;
	}else
		*val = 0;
	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int soh_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm){
		mutex_lock(&gm->data_lock);
		gm->soh = fg_read_soh(gm);
		mutex_unlock(&gm->data_lock);
		*val = gm->soh;
	}
	else
		*val = 0;
	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int soh_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->fake_soh = val;
	mca_log_err("%s %d\n", gm->log_tag, val);
	return 0;
}

static int ui_soh_get(struct bq_fg_chip *gm)
{
	static u16 addr = 0x007B;
	u8 ui_soh_data[70] = {0};
	int ret = 0;

	if (!gm)
		return 0;

	ret = fg_mac_read_block(gm, addr, ui_soh_data, 32);
	if (ret < 0)
	{
		mca_log_err("failed to read ui_soh_data \n");
		return ret;
	}
	mca_log_info("%s %d", gm->log_tag, ui_soh_data[0]);
	if (ui_soh_data[0] > 100 || ui_soh_data[0] < 0)
	{
		mca_log_err("abnormal battery force ui_soh = 0");
		gm->ui_soh = 0;
	} else if (ui_soh_data[0] > 0)
	{
		gm->ui_soh = ui_soh_data[0];
	} else
	{
		mca_log_err("new battery force ui_soh = 100");
		gm->ui_soh = 100;
	}

	return gm->ui_soh;
}

static int soh_new_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = ui_soh_get(gm);
	else
		*val = 0;
	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int bms_slave_connect_error_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if(gpio_is_valid(gm->slave_connect_gpio))
		*val = gpio_get_value(gm->slave_connect_gpio);
	else
		*val = 0;
	mca_log_err("%s %d\n", gm->log_tag, *val);
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
	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int adapting_power_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->adapting_power;
	else
		*val = 0;
	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int control_batt_chg_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
	{
		 if (gm->dev_role == FUELGAUGE_MASTER && gpio_is_valid(gm->batt1_control_gpio))
    	{
        	gpio_direction_output(gm->batt1_control_gpio, !!val);
        	gpio_set_value(gm->batt1_control_gpio, !!val); 
        	mca_log_err("success to reset batt1_control_gpio %d\n", !!val);
    	}
    	if (gm->dev_role == FUELGAUGE_SLAVE && gpio_is_valid(gm->batt2_control_gpio))
    	{
        	gpio_direction_output(gm->batt2_control_gpio, !!val);
        	gpio_set_value(gm->batt2_control_gpio, !!val); 
        	mca_log_err("success to reset batt2_control_gpio %d\n", !!val);
    	}
	}
	mca_log_err("%s %d\n", gm->log_tag, val);
	return 0;
}

static int control_batt_chg_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm){
		 if (gm->dev_role == FUELGAUGE_MASTER && gpio_is_valid(gm->batt1_control_gpio))
    	{
        	*val = gpio_get_value(gm->batt1_control_gpio); 
        	mca_log_err("get batt1_control_gpio val:%d\n", *val);
    	}
    	if (gm->dev_role == FUELGAUGE_SLAVE && gpio_is_valid(gm->batt2_control_gpio))
    	{
        	*val = gpio_get_value(gm->batt2_control_gpio); 
        	mca_log_err("get batt2_control_gpio val:%d\n", *val);
    	}
	}else
		*val = 0;
	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int cutoff_vol_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	u8 t_buf[32];
	int ret = 0;

	if (gm) {
		ret = fg_mac_read_block(gm, FG_MAC_CMD_MIX_TERM_VOLT, t_buf, 2);
		if (ret) {
			fg_err("failed to read mix_term_volt\n");
			return 0;
		}
		fg_info("%s:t_buf[0]=0x%02x t_buf[1]=0x%02x FG_MAC_CMD_MIX_TERM_VOLT=%d\n", gm->log_tag,
			t_buf[0], t_buf[1], t_buf[1] << 8 | t_buf[0]);
		*val = t_buf[1] << 8 | t_buf[0];
	} else
		*val = 0;
	fg_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int cutoff_vol_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	u8 t_buf[32];
	int ret = 0;
	int term_vol = val;
	u8 data[2] = {0xff, 0xff};

	if (gm) {
		data[0] = term_vol & 0xFF;
		data[1] = (term_vol >> 8) & 0xFF;
		ret = fg_mac_write_block(gm, FG_MAC_CMD_MIX_TERM_VOLT, data, 2);
		if (ret) {
			mca_log_err("%s:dynamic_shutdown_vol failed to write FG_MAC_CMD_MIX_TERM_VOLT\n", gm->log_tag);
		}
		ret = fg_mac_read_block(gm, FG_MAC_CMD_MIX_TERM_VOLT, t_buf, 2);
		mca_log_info("%s:dynamic_shutdown_vol t_buf[0]=0x%02x t_buf[1]=0x%02x FG_MAC_CMD_MIX_TERM_VOLT=%d\n", gm->log_tag,
						t_buf[0], t_buf[1], t_buf[1] << 8 | t_buf[0]);
	}
	mca_log_info("%s %d\n", gm->log_tag, val);
	return 0;
}

static int vendor_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->battery_vendor;
	else
		*val = 0;
	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int pack_vendor_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->pack_vendor;
	else
		*val = 0;
mca_log_err("%s %d\n", gm->log_tag, *val);
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
	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int i2c_error_count_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->fake_i2c_error_count = val;
	mca_log_err("%s %d\n", gm->log_tag, val);
	return 0;
}

static int eea_chg_support_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;

	*val = gm->is_eea_model;
	mca_log_err("%s is_eea_model=%d,%d\n", gm->log_tag, gm->is_eea_model, *val);

	return ret;
#if 0
	static u16 addr = 0x007B;
	u8 eea_chg_data[32] = {0};
	int ret = 0;

	ret = fg_mac_read_block(gm, addr, eea_chg_data, 32);
	if (ret < 0)
	{
		mca_log_err("failed to read eea_chg_support data\n");
		return ret;
	}
	*val = eea_chg_data[31];
	for (int i = 0; i < sizeof(eea_chg_data); i++)
		mca_log_err("%s: data[%d] = %d \n", bq->log_tag, i, eea_chg_data[i]);
	return ret;
#endif
}

static int eea_chg_support_set(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int val)
{
	int ret = 0;

	gm->is_eea_model = !!val;
	mca_log_err("%s is_eea_model=%d,%d\n", gm->log_tag, gm->is_eea_model, val);

	return ret;
#if 0
	static u16 addr = 0x007B;
	u8 eea_chg_data[32] = {0};
	int ret = 0;

	ret = fg_mac_read_block(gm, addr, eea_chg_data, 32);
	if (ret < 0)
	{
		mca_log_err("failed to read eea_chg_support data\n");
		return ret;
	}
	for (int i = 0; i < sizeof(eea_chg_data); i++)
		mca_log_err("%s: data[%d] = %d \n", bq->log_tag, i, eea_chg_data[i]);

	eea_chg_data[31] = val;
	mca_log_err("%s write eea_chg_support data = %d\n", bq->log_tag, val);

	ret = fg_mac_write_block(gm, addr, eea_chg_data, 32);
	if (ret < 0)
	{
		mca_log_err("failed to write eea_chg_support data\n");
		return ret;
	}
	return ret;
#endif
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

static int voltage_max_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	char data_limetime1[32];
	int ret = 0;

	memset(data_limetime1, 0, sizeof(data_limetime1));

	ret = fg_mac_read_block(gm, FG_MAC_CMD_LIFETIME1, data_limetime1, sizeof(data_limetime1));
	if (ret)
		mca_log_err("%s failed to get FG_MAC_CMD_LIFETIME1\n", gm->log_tag);
	*val = ((data_limetime1[1] << 8) | (data_limetime1[0] << 0));

	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
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
		mca_log_err("%s failed to get FG_MAC_CMD_LIFETIME1\n", gm->log_tag);
	*val = data_limetime1[6];

	mca_log_err("%s %d\n", gm->log_tag, *val);
	return 0;
}

static int temp_min_get(struct bq_fg_chip *gm,
	struct mtk_bms_sysfs_field_info *attr,
	int *val)
{
	char data_limetime1[32];
	int ret = 0;

	memset(data_limetime1, 0, sizeof(data_limetime1));

	ret = fg_mac_read_block(gm, FG_MAC_CMD_LIFETIME1, data_limetime1, sizeof(data_limetime1));
	if (ret)
		mca_log_err("failed to get FG_MAC_CMD_LIFETIME1\n");
	*val = data_limetime1[7];

	mca_log_err("%s %d\n", gm->log_tag, *val);
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
		mca_log_err("failed to get FG_MAC_CMD_LIFETIME3\n");

	ret = fg_mac_read_block(gm, FG_MAC_CMD_MANU_NAME, data, sizeof(data));
	if (ret)
		mca_log_err("failed to get FG_MAC_CMD_MANU_NAME\n");

	if (data[2] == 'C') //TI
	{
		ret = fg_mac_read_block(gm, FG_MAC_CMD_FW_VER, data, sizeof(data));
		if (ret)
			mca_log_err("failed to get FG_MAC_CMD_FW_VER\n");

		if ((data[3] == 0x0) && (data[4] == 0x1)) //R0 FW
			*val = ((data_limetime3[15] << 8) | (data_limetime3[14] << 0)) << 2;
		else if ((data[3] == 0x1) && (data[4] == 0x2)) //R1 FW
			*val = ((data_limetime3[9] << 8) | (data_limetime3[8] << 0)) << 2;
	}
	else if (data[2] == '4') //NVT
		*val = (data_limetime3[15] << 8) | (data_limetime3[14] << 0);

	mca_log_err("%s %d\n", gm->log_tag, *val);
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
		mca_log_err("%s: this Bq_Fg is not support this function.\n", gm->log_tag);
		return -1;
	}

	ret = fg_read_byte(gm, gm->regs[NVT_FG_REG_ISC], &isc_alert_level);

	if(ret < 0)
	{
		mca_log_err("%s: read isc_alert_level occur error.\n", gm->log_tag);
		return ret;
	}
	*val = isc_alert_level;
	mca_log_err("%s:now isc:%d.\n", gm->log_tag, *val);
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
		mca_log_err("%s: this Bq_Fg is not support this function.\n", gm->log_tag);
		return -1;
	}

	ret = fg_read_byte(gm,gm->regs[NVT_FG_REG_SOA_L], &soa_alert_level);

	if(ret < 0)
	{
		mca_log_err("%s: read soa_alert_level occur error.\n", gm->log_tag);
		return ret;
	}
	*val = soa_alert_level;
	mca_log_err("%s:now soa:%d.\n", gm->log_tag, *val);
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
	BMS_SYSFS_FIELD_RW(soh, BMS_PROP_SOH),
	BMS_SYSFS_FIELD_RO(soh_new, BMS_PROP_SOH_NEW),
	BMS_SYSFS_FIELD_RO(resistance, BMS_PROP_RESISTANCE),
	BMS_SYSFS_FIELD_RW(i2c_error_count, BMS_PROP_I2C_ERROR_COUNT),
	BMS_SYSFS_FIELD_RO(av_current, BMS_PROP_AV_CURRENT),
	BMS_SYSFS_FIELD_RO(voltage_max, BMS_PROP_VOLTAGE_MAX),
	BMS_SYSFS_FIELD_RO(temp_max, BMS_PROP_TEMP_MAX),
	BMS_SYSFS_FIELD_RO(temp_min, BMS_PROP_TEMP_MIN),
	BMS_SYSFS_FIELD_RO(time_ot, BMS_PROP_TIME_OT),
	BMS_SYSFS_FIELD_RO(bms_slave_connect_error, BMS_PROP_BMS_SLAVE_CONNECT_ERROR),
	BMS_SYSFS_FIELD_RO(cell_supplier, BMS_PROP_CELL_SUPPLIER),
	BMS_SYSFS_FIELD_RO(isc_alert_level, BMS_PROP_ISC_ALERT_LEVEL),
	BMS_SYSFS_FIELD_RO(soa_alert_level, BMS_PROP_SOA_ALERT_LEVEL),
	BMS_SYSFS_FIELD_RO(shutdown_voltage, BMS_PROP_SHUTDOWN_VOL),
	BMS_SYSFS_FIELD_RW(charge_eoc, BMS_PROP_CHARGE_EOC),
	BMS_SYSFS_FIELD_RW(charging_done, BMS_PROP_CHARGING_DONE),
	BMS_SYSFS_FIELD_RO(calc_rvalue, BMS_PROP_CALC_RVALUE),
	BMS_SYSFS_FIELD_RW(aged_in_advance, BMS_PROP_AGED_IN_ADVANCE),
	BMS_SYSFS_FIELD_RW(eea_chg_support, BMS_PROP_EEA_CHG_SUPPORT),
	BMS_SYSFS_FIELD_RO(real_temp, BMS_PROP_REAL_TEMP),
	BMS_SYSFS_FIELD_RO(vendor, BMS_PROP_BATTERY_VENDOR),
	BMS_SYSFS_FIELD_RO(pack_vendor, BMS_PROP_BATTERY_PACK_VENDOR),
	BMS_SYSFS_FIELD_RW(dod_count, BMS_PROP_DOD_COUNT),
	BMS_SYSFS_FIELD_RO(adapting_power, BMS_PROP_ADAP_POWER),
	BMS_SYSFS_FIELD_RW(control_batt_chg, BMS_PROP_CONTROL_BATT_CHG),
	BMS_SYSFS_FIELD_RW(cutoff_vol, BMS_PROP_CUTOFF_VOL),
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
		mca_log_err("%s usb bp:%d idx error\n", gm->log_tag, bp);
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
		mca_log_err("%s usb bp:%d idx error\n", gm->log_tag, bp);
		return -ENOTSUPP;
	}
	return 0;
}
EXPORT_SYMBOL(bms_set_property);

int slave_bms_get_property(enum bms_property bp,
			    int *val)
{
	struct bq_fg_chip *gm;
	struct power_supply *psy;

	psy = power_supply_get_by_name("bms_slave");
	if (psy == NULL)
		return -ENODEV;

	gm = (struct bq_fg_chip *)power_supply_get_drvdata(psy);
	if (bms_sysfs_field_tbl[bp].prop == bp)
		bms_sysfs_field_tbl[bp].get(gm,
			&bms_sysfs_field_tbl[bp], val);
	else {
		mca_log_err("%s usb bp:%d idx error\n", gm->log_tag, bp);
		return -ENOTSUPP;
	}

	return 0;
}
EXPORT_SYMBOL(slave_bms_get_property);

int slave_bms_set_property(enum bms_property bp,
			    int val)
{
	struct bq_fg_chip *gm;
	struct power_supply *psy;

	psy = power_supply_get_by_name("bms_slave");
	if (psy == NULL)
		return -ENODEV;

	gm = (struct bq_fg_chip *)power_supply_get_drvdata(psy);

	if (bms_sysfs_field_tbl[bp].prop == bp)
		bms_sysfs_field_tbl[bp].set(gm,
			&bms_sysfs_field_tbl[bp], val);
	else {
		mca_log_err("%s usb bp:%d idx error\n", gm->log_tag, bp);
		return -ENOTSUPP;
	}
	return 0;
}
EXPORT_SYMBOL(slave_bms_set_property);

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
	int tem = 0, tbat_temp = 0;;
	struct mtk_battery_manager *bm;
	struct mtk_battery *gm;

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
		mutex_lock(&bq->data_lock);
		bq->rsoc = fg_read_rsoc(bq);
		mutex_unlock(&bq->data_lock);
		val->intval = bq->rsoc;
		//add shutdown delay feature
		if (bq->enable_shutdown_delay) {
			if (val->intval <= 1 && !bq->vbatt_empty) {
				mutex_lock(&bq->data_lock);
				tbat_temp = fg_read_temperature(bq);
				fg_read_volt(bq);
				mutex_unlock(&bq->data_lock);
				bq->tbat = get_median(tbat_temp);
				tem = bq->tbat;
				if (!battery_get_psy(bq)) {
					mca_log_err("%s get capacity failed to get battery psy\n", bq->log_tag);
					break;
				} else
					power_supply_get_property(bq->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
				if (pval.intval != POWER_SUPPLY_STATUS_CHARGING) {
					if(bq->shutdown_delay == true) {
						val->intval = 1;
					} else if ((bq->cell_voltage[2] <= bq->critical_shutdown_vbat) &&
							bq->shutdown_flag == false) {
						bq->shutdown_delay = true;
						val->intval = 1;
					} else {
						bq->shutdown_delay = false;
					}
					mca_log_err("%s last_shutdown= %d. shutdown= %d, soc =%d, voltage =%d\n", bq->log_tag, last_shutdown_delay, bq->shutdown_delay, val->intval, bq->cell_voltage[2]);
				} else {
					bq->shutdown_delay = false;
					if ((bq->cell_voltage[2] >= (bq->critical_shutdown_vbat - 60)) &&
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
			}
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (bq->fake_tbat) {
			val->intval = bq->fake_tbat;
			break;
		}
		mutex_lock(&bq->data_lock);
		tbat_temp = fg_read_temperature(bq);
		mutex_unlock(&bq->data_lock);
		bq->tbat = get_median(tbat_temp);
		if (!battery_get_psy(bq)) {
			mca_log_err("%s fg_update failed to get battery psy\n", bq->log_tag);
		} else {
			bm = (struct mtk_battery_manager *)power_supply_get_drvdata(bq->batt_psy);
			gm = bm->gm1;
			if(gm != NULL){
				if (gm->extreme_cold_chg_flag) {
					if (bq->tbat > 50)
						bq->extreme_cold_temp = 60;
					else
						bq->extreme_cold_temp = 50;
				} else
					bq->extreme_cold_temp = 0;
			}
		}
		// mca_log_info("%s update tbat=%d, extreme_cold_temp = %d\n", bq->log_tag, bq->tbat, bq->extreme_cold_temp);
		val->intval = bq->tbat - bq->extreme_cold_temp;
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
		mutex_lock(&bq->data_lock);
		bq->cycle_count = fg_read_cyclecount(bq);
		mutex_unlock(&bq->data_lock);
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
		power_supply_changed(bq->fg_psy);
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

	if(bq->dev_role == FUELGAUGE_MASTER)
		bq->fg_psy_d.name = "bms";
	else if(bq->dev_role == FUELGAUGE_SLAVE)
		bq->fg_psy_d.name = "bms_slave";
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
		mca_log_err("%s failed to register fg_psy", bq->log_tag);
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
	mutex_lock(&bq->data_lock);
	fg_read_volt(bq);
	mutex_unlock(&bq->data_lock);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", bq->cell_voltage[0]);

	return ret;
}

static ssize_t fg_show_cell1_voltage(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret = 0;
	mutex_lock(&bq->data_lock);
	fg_read_volt(bq);
	mutex_unlock(&bq->data_lock);
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
		mca_log_err("%s not support device name\n", bq->log_tag);
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
	mca_log_info("%s show log_level = %d\n", bq->log_tag, log_level);

	return ret;
}

static ssize_t fg_store_log_level(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret = 0;

	ret = sscanf(buf, "%d", &log_level);
	mca_log_info("%s store log_level = %d\n", bq->log_tag, log_level);

	return count;
}

static ssize_t fg_show_manufacturing_date(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	u8 t_buf[64] = { 0 };
	int ret = 0;
	char manufacturing_date[] = {'2', '0', '2', '0', '0', '0', '0', '0', '\0'};

	ret = fg_mac_read_block(bq, FG_MAC_CMD_BATT_SN, t_buf, 32);
	if (ret < 0) {
		mca_log_err("failed to get BATT_SN\n");
		return ret;
	}

	manufacturing_date[3] = '0' + (t_buf[6] & 0xF);
	manufacturing_date[6] = '0' + (t_buf[8] & 0xF);
	manufacturing_date[7] = '0' + (t_buf[9] & 0xF);

	switch (t_buf[7]) {
	case 0x41:
		manufacturing_date[4] = '1';
		manufacturing_date[5] = '0';
		break;
	case 0x42:
		manufacturing_date[4] = '1';
		manufacturing_date[5] = '1';
		break;
	case 0x43:
		manufacturing_date[4] = '1';
		manufacturing_date[5] = '2';
		break;
	default:
		manufacturing_date[5] = '0' + (t_buf[7] & 0xF);
		break;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%s", manufacturing_date);
	mca_log_err("%s:%s\n", bq->log_tag, buf);

	return ret;
}

static ssize_t fg_show_batt_sn(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	u8 t_buf[64] = { 0 };
	int ret = 0;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_BATT_SN, t_buf, 32);
	if (ret < 0) {
		mca_log_err("failed to get BATT_SN\n");
		return ret;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%s", t_buf);
	mca_log_err("%s:%s\n", bq->log_tag, buf);

	return ret;
}

#define DEST_INIT_STRING "000000"
#define DEFAULT_FIRST_USAGE_DATE "00000000"
#define ERROE_FIRST_USAGE_DATE "99999999"
#define FW_VERSION_0913 "2052"
static bool is_all_digits(const char *str) {
    while (*str) {
        if (!isdigit((unsigned char)*str)) {
            return false;
        }
        str++;
    }
    return true;
}

static ssize_t fg_show_first_usage_date(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	u8 t_buf[64] = { 0 };
	int ret;
	char first_usage_date[] = {'2', '0', '0', '0', '0', '0', '0', '0', '\0'};
	char data[64];

	ret = fg_mac_read_block(bq, FG_MAC_CMD_UI_SOH, t_buf, 32);
	if (ret < 0) {// read error, show 9999999
		memcpy(buf,"99999999",8);
		mca_log_err("failed to get first_usage_date\n");
		return ret;
	}

	first_usage_date[0] = '2';/*固定头*/
	first_usage_date[1] = '0';/*固定头*/
	first_usage_date[2] = '0' + t_buf[11] / 10;/*年份的倒数第二位数字*/
	first_usage_date[3] = '0' + t_buf[11] % 10;/*年份的倒数第一位数字*/
	first_usage_date[4] = '0' + t_buf[12] / 10;/*月份第一位数字*/
	first_usage_date[5] = '0' + t_buf[12] % 10;/*月份第二位数字*/
	first_usage_date[6] = '0' + t_buf[13] / 10;/*日第一位数字*/
	first_usage_date[7] = '0' + t_buf[13] % 10;/*日第二位数字*/

	ret = scnprintf(data, PAGE_SIZE, "%s", first_usage_date);

	if (!strncmp(&data[2], DEST_INIT_STRING, strlen(DEST_INIT_STRING))) {
		strcpy(data, DEFAULT_FIRST_USAGE_DATE);
		mca_log_err(" usage_date init %s\n", data);
	} else if (!strncmp(&data[0], FW_VERSION_0913, strlen(FW_VERSION_0913))) {
		data[2] = '2'; /*年份的倒数第二位数字*/
		data[3] = '4';/*年份的倒数第一位数字*/
		data[4] = '1';/*月份第一位数字*/
		data[5] = '0';/*月份第二位数字*/
		data[6] = '0';/*日第一位数字*/
		data[7] = '1';/*日第二位数字*/
		mca_log_err("09-13 version compatiable\n");
	}

	if (!is_all_digits(data)) {
		mca_log_err("usage data has invalid char [%s]\n", data);
		strcpy(data, ERROE_FIRST_USAGE_DATE);
	}

	mca_log_err("%s:%s\n", bq->log_tag, data);
	ret = scnprintf(buf, PAGE_SIZE, "%s", data);

	return ret;
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
static ssize_t fg_store_first_usage_date(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret, len;
	u8 data[32] = { 0 };
	u8 data_temp[32] = { 0 };
	char *tmp_buf = NULL;
	int retry = 0;
	bool check_status = false;

	tmp_buf = kzalloc(size + 1, GFP_KERNEL);
	if (!tmp_buf)
		return 0;

	len = strlen(tmp_buf);
	remove_whitespace(buf, tmp_buf);
	mca_log_err("write first_usage_date=%s,len=%d\n", tmp_buf,len);

	if (!is_all_digits(tmp_buf) || !size) {
		mca_log_err("input is not all digits! errorstr = %s size = [%ld]\n", tmp_buf, size);
		kfree(tmp_buf);
		return 0;
	}

	ret = fg_mac_read_block(bq, FG_MAC_CMD_UI_SOH, data, 32);
	if (ret < 0) {
		mca_log_err("failed to get first_usage\n");
		kfree(tmp_buf);
		return 0;
	}

	/*example,20241220,2024(0x34)1(0x31)2(0x32)2(0x32)0(0x30)*/
	data[11] = (tmp_buf[2] - '0') * 10 + (tmp_buf[3] - '0'); /*记录年份的最后2位数字*/
	data[12] = (tmp_buf[4] - '0') * 10 + (tmp_buf[5] - '0'); /*记录月份的数字*/
	data[13] = (tmp_buf[6] - '0') * 10 + (tmp_buf[7] - '0'); /*记录日期的数字*/

	while (retry++ < RETRY_UPDATE_COUNT) {
		ret = fg_mac_write_block(bq, FG_MAC_CMD_UI_SOH, data, 32);
		if (ret < 0) {
			msleep(100);
			continue;
		}

		msleep(100);
		memset(data_temp, 0, sizeof(data_temp));
		ret = fg_mac_read_block(bq, FG_MAC_CMD_UI_SOH, data_temp, 32);
		if (ret < 0) {
			mca_log_err("failed to read first_usage_date\n");
			continue;
		}

		if (data[11] == data_temp[11] &&
			data[12] == data_temp[12] &&
			data[13] == data_temp[13]) {
			mca_log_err("write first_usage_date success\n");
			check_status = true;
			break;
		} else {
			mca_log_err("write fail Updata[%d][%d][%d] = Reg[%d][%d][%d]\n",
				data[11],data[12], data[13], data_temp[11], data_temp[12], data_temp[13]);
			continue;
		}
	}

	if (!check_status) {
		data[11] = 0x00;
		data[12] = 0x00;
		data[13] = 0x00;
		retry = 0;

		while (retry++ < RETRY_UPDATE_COUNT) {
			ret = fg_mac_write_block(bq, FG_MAC_CMD_UI_SOH, data, 32);
			if (ret < 0) {
				msleep(100);
				continue;
			}

			msleep(100);
			memset(data_temp, 0, sizeof(data_temp));
			ret = fg_mac_read_block(bq, FG_MAC_CMD_UI_SOH, data_temp, 32);
			if (!data_temp[11] && !data_temp[12] && !data_temp[13]) {
				mca_log_err("reset success\n");
				check_status = true;
				break;
			} else {
				mca_log_err("reset fail Reg[%d][%d][%d]\n",
				data_temp[11], data_temp[12], data_temp[13]);
				continue;
			}
		}

	}
	kfree(tmp_buf);

	return size;
}

static ssize_t fg_show_ui_soh(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	static u16 addr = 0x007B;
	u8 ui_soh_data[70] = {0};
	int ret = 0;

	ret = fg_mac_read_block(bq, addr, ui_soh_data, 32);
	if (ret < 0)
	{
		mca_log_err("failed to read ui_soh_data \n");
		return ret;
	}
	ret = snprintf(buf, PAGE_SIZE, "%d %d %d %d %d %d %d %d %d %d %d \n",
		ui_soh_data[0],ui_soh_data[1],ui_soh_data[2],ui_soh_data[3],ui_soh_data[4],ui_soh_data[5],
		ui_soh_data[6],ui_soh_data[7],ui_soh_data[8],ui_soh_data[9],ui_soh_data[10]);
	mca_log_err("%s: latest_ui_soh = %d \n", bq->log_tag, ui_soh_data[0]);
	return ret;
}

static ssize_t fg_store_ui_soh(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	static u16 addr = 0x007B;
	char t_data[70] = {0};
	char *pchar = NULL, *qchar = NULL;
	u8 ui_soh_data[40] = {0,};
	u8 data[40] = {0,};
	int ret = 0, i = 0;
	u8 val = 0;

	// mca_log_err("%s raw data : %s \n", bq->log_tag, buf);
	memset(t_data, 0, sizeof(t_data));
	strncpy(t_data, buf, count);
	mca_log_err("%s t_data : %s\n", bq->log_tag, t_data);

	qchar = t_data;

	while ((pchar = strsep(&qchar, " ")))
	{
		ret = kstrtou8(pchar, 10, &val);
		if (ret < 0) {
			mca_log_err("kstrtou8 error return %d \n", ret);
			return count;
		}
		ui_soh_data[i] = val;
		val = 0;
		mca_log_err("%s ui_soh_data[%d]: %d \n", bq->log_tag ,i, ui_soh_data[i]);
		i++;
	}

	bq->ui_soh = ui_soh_data[0];
	mca_log_err("%s: bq->ui_soh = %d \n",bq->log_tag, bq->ui_soh);

	ret = fg_mac_read_block(bq, FG_MAC_CMD_UI_SOH, data, 32);
	if (ret < 0) {
		mca_log_err("failed to get first_usage\n");
		return ret;
	}
	for (i = 0; i < 11; i++)
		data[i] = ui_soh_data[i];

	ret = fg_mac_write_block(bq, addr, data, 32);
	if (ret < 0)
	{
		mca_log_err("failed to write data \n");
		return count;
	}

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
static DEVICE_ATTR(ui_soh, S_IRUGO | S_IWUSR, fg_show_ui_soh, fg_store_ui_soh);
static DEVICE_ATTR(manufacturing_date, S_IRUGO, fg_show_manufacturing_date, NULL);
static DEVICE_ATTR(first_usage_date, S_IRUGO | S_IWUSR, fg_show_first_usage_date, fg_store_first_usage_date);
static DEVICE_ATTR(batt_sn, S_IRUGO, fg_show_batt_sn, NULL);

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
	&dev_attr_ui_soh.attr,
	&dev_attr_manufacturing_date.attr,
	&dev_attr_first_usage_date.attr,
	&dev_attr_batt_sn.attr,
	NULL,
};

static const struct attribute_group fg_attr_group = {
	.attrs = fg_attributes,
};

static int fg_parse_dt(struct bq_fg_chip *bq)
{
	struct device_node *node = bq->dev->of_node;
	int ret = 0, size = 0;
	struct pinctrl *bq_pinctrl;
	struct pinctrl_state *slave_connect_cfg;

	bq->max_chg_power_120w = of_property_read_bool(node, "max_chg_power_120w");
	bq->enable_shutdown_delay = of_property_read_bool(node, "enable_shutdown_delay");

	bq->slave_connect_gpio = of_get_named_gpio(node, "slave_connect_gpio", 0);
	//mca_log_err("%s slave_connect_gpio = %d \n", bq->log_tag, bq->slave_connect_gpio );
	if (!gpio_is_valid(bq->slave_connect_gpio)) {
		mca_log_info("failed to parse slave_connect_gpio\n");
		//  return -1;
	}
	bq->batt1_control_gpio = of_get_named_gpio(node, "batt1_ctrl_gpio", 0);
	bq->batt2_control_gpio = of_get_named_gpio(node, "batt2_ctrl_gpio", 0);
	if (!gpio_is_valid(bq->batt1_control_gpio) && !gpio_is_valid(bq->batt2_control_gpio)) {
		mca_log_info("failed to parse batt_control_gpio\n");
		// return -1;
	}
	bq_pinctrl = devm_pinctrl_get(bq->dev);
	if (! IS_ERR_OR_NULL(bq_pinctrl)) {
		slave_connect_cfg = pinctrl_lookup_state(bq_pinctrl, "slave_connect_cfg");
		if (! IS_ERR_OR_NULL(slave_connect_cfg)) {
			pinctrl_select_state(bq_pinctrl, slave_connect_cfg);
			mca_log_err("success to config slave_connect_cfg\n");
		} else {
			mca_log_err("failed to parse slave_connect_cfg\n");
		}
	} else {
		mca_log_err("failed to get pinctrl\n");
	}

	ret = of_property_read_u32(node, "normal_shutdown_vbat_1s", &bq->normal_shutdown_vbat);
	if (ret)
		mca_log_err("%s failed to parse normal_shutdown_vbat_1s\n", bq->log_tag);

	ret = of_property_read_u32(node, "critical_shutdown_vbat_1s", &bq->critical_shutdown_vbat);
	if (ret)
		mca_log_err("%s failed to parse critical_shutdown_vbat_1s\n", bq->log_tag);

	ret = of_property_read_u32(node, "cool_critical_shutdown_vbat_1s", &bq->cool_critical_shutdown_vbat);
	if (ret)
		mca_log_err("%s failed to parse cool_critical_shutdown_vbat_1s\n", bq->log_tag);

	ret = of_property_read_u32(node, "old_critical_shutdown_vbat_1s", &bq->old_critical_shutdown_vbat);
	if (ret)
		mca_log_err("%s failed to parse old_critical_shutdown_vbat_1s\n", bq->log_tag);

	ret = of_property_read_u32(node, "report_full_rsoc_1s", &bq->report_full_rsoc);
	if (ret)
		mca_log_err("%s failed to parse report_full_rsoc_1s\n", bq->log_tag);
	ret = of_property_read_u32(node, "soc_gap_1s", &bq->soc_gap);
	if (ret)
		mca_log_err("%s failed to parse soc_gap_1s\n", bq->log_tag);
	ret = of_property_read_u32(node, "ic_role", &bq->dev_role);
	if (ret)
		mca_log_err("%s failed to parse ic_role\n", bq->log_tag);

	of_get_property(node, "soc_decimal_rate", &size);
	if (size) {
		bq->dec_rate_seq = devm_kzalloc(bq->dev,
				size, GFP_KERNEL);
		if (bq->dec_rate_seq) {
			bq->dec_rate_len =
				(size / sizeof(*bq->dec_rate_seq));
			if (bq->dec_rate_len % 2) {
				mca_log_err("%s invalid soc decimal rate seq\n", bq->log_tag);
				return -EINVAL;
			}
			of_property_read_u32_array(node,
					"soc_decimal_rate",
					bq->dec_rate_seq,
					bq->dec_rate_len);
		} else {
			mca_log_err("%s error allocating memory for dec_rate_seq\n", bq->log_tag);
		}
	}

	return ret;
}

static int fg_get_adapt_power(struct bq_fg_chip *bq , char ch)
{
	switch (ch) {
		case '0':
			bq->adapting_power = 10;
			break;
		case '1':
			bq->adapting_power = 15;
			break;
		case '2':
			bq->adapting_power = 18;
			break;
		case '3':
			bq->adapting_power = 25;
			break;
		case '4':
			bq->adapting_power = 33;
			break;
		case '5':
			bq->adapting_power = 35;
			break;
		case '6':
			bq->adapting_power = 40;
			break;
		case '7':
			bq->adapting_power = 55;
			break;
		case '8':
			bq->adapting_power = 60;
			break;
		case '9':
			bq->adapting_power = 67;
			break;
		case 'A':
			bq->adapting_power = 80;
			break;
		case 'B':
			bq->adapting_power = 90;
			break;
		case 'C':
			bq->adapting_power = 100;
			break;
		case 'D':
			bq->adapting_power = 120;
			break;
		case 'E':
			bq->adapting_power = 140;
			break;
		case 'F':
			bq->adapting_power = 160;
			break;
		case 'G':
			bq->adapting_power = 180;
			break;
		case 'H':
			bq->adapting_power = 200;
			break;
		case 'I':
			bq->adapting_power = 220;
			break;
		case 'J':
			bq->adapting_power = 240;
			break;
		default:
			bq->adapting_power = 0;
			break;
	}
	mca_log_err("%s: data=%c, adapting_power=%d\n", bq->log_tag, ch, bq->adapting_power);

	return 0;
}

static int fg_check_device(struct bq_fg_chip *bq)
{
	u8 data[32];
	int ret = 0;
	static char ch = '\0';

	/* FG EEPROM Coding Rule V0.04 Update */
	ret = fg_mac_read_block(bq, FG_MAC_CMD_MANU_NAME, data, 32);
	if (ret) {
		mca_log_info("%s: failed to get FG_MAC_CMD_MANU_NAME, ret=%d\n", bq->log_tag,ret);
		mca_log_err("%s: FG_MAC_CMD_MANU_NAME: %s\n", bq->log_tag, data);
	} else {
		mca_log_err("%s: FG_MAC_CMD_MANU_NAME: %s\n", bq->log_tag, data);
	}

	if (!strncmp(data, "MI", 2)) {
		bq->chip_ok = true;

		if (!strncmp(&data[2], "4", 1)) {
			bq->device_name = BQ_FG_NFG1000B;
			strcpy(bq->model_name, "nfg1000b");
			bq->battery_vendor = BATTERY_PACK_VENDOR_NFG1000B;
			strcpy(bq->log_tag, "[XMCHG_NFG1000B]");
		} else if (!strncmp(&data[2], "5", 1) || !strncmp(&data[2], "6", 1)) {
			bq->device_name = BQ_FG_NFG1000A;
			strcpy(bq->model_name, "mpc8011b");
			bq->battery_vendor = BATTERY_PACK_VENDOR_MPC8011B;
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
			mca_log_info("%s: failed to get MI fg.\n", bq->log_tag);
		}

		if (bq->chip_ok) {
			ch = data[4];
			fg_get_adapt_power(bq, ch);
		}
	} else {
		bq->chip_ok = false;
		strcpy(bq->log_tag, "[OTHERS_UNKNOWN_FG]");
		mca_log_info("failed to get MI fg.\n");
	}

	ret = fg_mac_read_block(bq, FG_MAC_CMD_DEVICE_CHEM, data, 32);
	if (ret) {
		mca_log_info("failed to get FG_MAC_CMD_DEVICE_CHEM\n");
		mca_log_err("%s: FG_MAC_CMD_DEVICE_CHEM: %s\n", bq->log_tag, data);
	} else {
		mca_log_err("%s: FG_MAC_CMD_DEVICE_CHEM: %s\n", bq->log_tag, data);
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

	switch (data[2]) {
		case 'B':
			bq->pack_vendor = PACK_SUPPLIER_BYD;
			break;
		case 'C':
			bq->pack_vendor = PACK_SUPPLIER_COSLIGHT;
			break;
		case 'S':
			bq->pack_vendor = PACK_SUPPLIER_SUNWODA;
			break;
		case 'N':
			bq->pack_vendor = PACK_SUPPLIER_NVT;
			break;
		case 'U':
			bq->pack_vendor = PACK_SUPPLIER_SCUD;
			break;
		case 'T':
			bq->pack_vendor = PACK_SUPPLIER_TWS;
			break;
		case 'I':
			bq->pack_vendor = PACK_SUPPLIER_LISHEN;
			break;
		case 'K':
			bq->pack_vendor = PACK_SUPPLIER_DESAY;
			break;
		default:
			bq->pack_vendor = PACK_SUPPLIER_NVT;
			break;
	}

	ret = fg_mac_read_block(bq, FG_MAC_CMD_DEVICE_NAME, data, 32);
	if (ret) {
		mca_log_err("failed to get FG_MAC_CMD_DEVICE_NAME\n");
		mca_log_err("%s: FG_MAC_CMD_DEVICE_NAME: %s\n", bq->log_tag, data);
	} else {
		mca_log_err("%s: FG_MAC_CMD_DEVICE_NAME: %s\n", bq->log_tag, data);
	}

	if(bq->dev_role == FUELGAUGE_MASTER){
		snprintf(bq->log_tag, PAGE_SIZE, "%s[FG_MASTER]", bq->log_tag);
	}else{
		snprintf(bq->log_tag, PAGE_SIZE, "%s[FG_SLAVE]", bq->log_tag);
	}

	return ret;
}

static int fg_reset_batt_control_gpio(struct bq_fg_chip *bq)
{
	if(bq == NULL)
		return -1;
	if (gpio_is_valid(bq->batt1_control_gpio))
	{
		gpio_direction_output(bq->batt1_control_gpio, 0);
		gpio_set_value(bq->batt1_control_gpio, 0); 
		mca_log_err("success to reset batt1_control_gpio\n");
	}
	if (gpio_is_valid(bq->batt2_control_gpio))
	{
		gpio_direction_output(bq->batt2_control_gpio, 0);
		gpio_set_value(bq->batt2_control_gpio, 0); 
		mca_log_err("success to reset batt2_control_gpio\n");
	}
	return 0;
}

static int ops_fg_read_cyclecount(struct fg_device *fg_dev, int *cyclecount)
{
	struct bq_fg_chip *bq = fuelgauge_get_data(fg_dev);
	int ret = 0;
	mutex_lock(&bq->data_lock);
	*cyclecount = fg_read_cyclecount(bq);
	mutex_unlock(&bq->data_lock);

	return ret;
}

static int ops_fg_read_rsoc(struct fg_device *fg_dev, int *rsoc)
{
	struct bq_fg_chip *bq = fuelgauge_get_data(fg_dev);
	int ret = 0;
	mutex_lock(&bq->data_lock);
	*rsoc = fg_read_rsoc(bq);
	mutex_unlock(&bq->data_lock);
	mca_log_err("%s rsoc=%d\n", bq->log_tag, *rsoc);
	return ret;
}

static int ops_fg_read_soh(struct fg_device *fg_dev, int *soh)
{
	struct bq_fg_chip *bq = fuelgauge_get_data(fg_dev);
	int ret = 0;
	mutex_lock(&bq->data_lock);
	*soh = fg_read_soh(bq);
	mutex_unlock(&bq->data_lock);

	return ret;
}

static int ops_fg_get_raw_soc(struct fg_device *fg_dev, int *raw_soc)
{
	struct bq_fg_chip *bq = fuelgauge_get_data(fg_dev);
	int ret = 0;
	mutex_lock(&bq->data_lock);
	*raw_soc = fg_get_raw_soc(bq);
	mutex_unlock(&bq->data_lock);

	return ret;
}

static int ops_fg_read_current(struct fg_device *fg_dev, int *ibat)
{
	struct bq_fg_chip *bq = fuelgauge_get_data(fg_dev);
	int ret = 0;
	mutex_lock(&bq->data_lock);
	*ibat = fg_read_current(bq);
	mutex_unlock(&bq->data_lock);
	mca_log_err("%s ibat=%d\n", bq->log_tag, *ibat);
	return ret;
}

static int ops_fg_read_temperature(struct fg_device *fg_dev, int *temp)
{
	struct bq_fg_chip *bq = fuelgauge_get_data(fg_dev);
	int ret = 0;
	mutex_lock(&bq->data_lock);
	*temp = fg_read_temperature(bq);
	mutex_unlock(&bq->data_lock);
	return ret;
}

static int ops_fg_read_volt(struct fg_device *fg_dev, int *vbat)
{
	struct bq_fg_chip *bq = fuelgauge_get_data(fg_dev);
	int ret = 0;
	mutex_lock(&bq->data_lock);
	fg_read_volt(bq);
	*vbat = bq->vbat;
	mutex_unlock(&bq->data_lock);
	mca_log_err("%s vbat=%d\n", bq->log_tag, *vbat);
	return ret;
}

static int ops_fg_read_status(struct fg_device *fg_dev, bool *batt_fc)
{
	struct bq_fg_chip *bq = fuelgauge_get_data(fg_dev);
	int ret = 0;
	mutex_lock(&bq->data_lock);
	fg_read_status(bq);
	*batt_fc = bq->batt_fc;;
	mutex_unlock(&bq->data_lock);

	return ret;
}

static int ops_fg_update_battery_shutdown_vol(struct fg_device *fg_dev, int *shutdown_vbat)
{
	struct bq_fg_chip *bq = fuelgauge_get_data(fg_dev);
	int ret = 0;

	battery_shutdown_vol_update(bq);
	*shutdown_vbat = bq->normal_shutdown_vbat;

	return ret;
}

static int ops_fg_update_monitor_delay(struct fg_device *fg_dev, int *monitor_delay)
{
	struct bq_fg_chip *bq = fuelgauge_get_data(fg_dev);
	int ret = 0;
	*monitor_delay = bq->monitor_delay;

	return ret;
}

static int ops_fg_update_eea_chg_support(struct fg_device *fg_dev, bool *eea_support)
{
	struct bq_fg_chip *bq = fuelgauge_get_data(fg_dev);
	int ret = 0;
	*eea_support = bq->is_eea_model;

	return ret;
}

static int ops_fg_get_charging_done_status(struct fg_device *fg_dev, bool *charging_done)
{
	struct bq_fg_chip *bq = fuelgauge_get_data(fg_dev);
	int ret = 0;
	*charging_done = bq->charging_done;

	return ret;
}

static int ops_fg_get_en_smooth_full_status(struct fg_device *fg_dev, bool *en_smooth_full)
{
	struct bq_fg_chip *bq = fuelgauge_get_data(fg_dev);
	int ret = 0;
	*en_smooth_full = bq->en_smooth_full;

	return ret;
}

static int ops_fg_get_rm(struct fg_device *fg_dev, int *rm)
{
	struct bq_fg_chip *bq = fuelgauge_get_data(fg_dev);
	int ret = 0;
	*rm = bq->rm;

	return ret;
}

static int ops_fg_read_avg_current(struct fg_device *fg_dev, int *avg_current)
{
	struct bq_fg_chip *bq = fuelgauge_get_data(fg_dev);
	int ret = 0;
	*avg_current = fg_read_avg_current(bq);

	return ret;
}

static int ops_fg_get_i2c_error_count(struct fg_device *fg_dev, int *count)
{
	struct bq_fg_chip *bq = fuelgauge_get_data(fg_dev);
	if(bq == NULL)
		return -1;
	*count = bq->i2c_error_count;

	return 0;
}

static int ops_fg_read_fcc(struct fg_device *fg_dev, int *fcc)
{
	struct bq_fg_chip *bq = fuelgauge_get_data(fg_dev);
	if(bq == NULL)
		return -1;
	*fcc = fg_read_fcc(bq);
	bq->fcc = *fcc;

	return 0;
}

static int ops_fg_read_design_capacity(struct fg_device *fg_dev, int *dc)
{
	struct bq_fg_chip *bq = fuelgauge_get_data(fg_dev);
	if(bq == NULL)
		return -1;
	*dc = bq->dc;

	return 0;
}

static int ops_fg_get_fg_chip_ok(struct fg_device *fg_dev, int *chip_ok)
{
	struct bq_fg_chip *bq = fuelgauge_get_data(fg_dev);
	if(bq == NULL)
		return -1;
	*chip_ok = bq->chip_ok;

	return 0;
}

static int ops_fg_get_ui_soh(struct fg_device *fg_dev, char *buf)
{
	struct bq_fg_chip *bq = fuelgauge_get_data(fg_dev);
	static u16 addr = 0x007B;
	u8 ui_soh_data[70] = {0};
	int ret = 0, i = 0;
	if(bq == NULL)
		return -1;
	mutex_lock(&bq->data_lock);
	ret = fg_mac_read_block(bq, addr, ui_soh_data, 32);
	if (ret < 0)
	{
		mca_log_err("failed to read ui_soh_data \n");
		mutex_unlock(&bq->data_lock);
		return ret;
	}
	mutex_unlock(&bq->data_lock);
	for (i = 0; i < sizeof(ui_soh_data); i++)
	{
		buf[i] = (char)ui_soh_data[i];
		mca_log_err("%s: buf[%d] = %d \n", bq->log_tag, i, ui_soh_data[i]);
	}
	mca_log_err("%s: latest_ui_soh = %d \n", bq->log_tag, ui_soh_data[0]);
	return ret;
}

static int ops_fg_set_ui_soh(struct fg_device *fg_dev, char *buf)
{
	struct bq_fg_chip *bq = fuelgauge_get_data(fg_dev);
	static u16 addr = 0x007B;
	char t_data[70] = {0};
	char *pchar = NULL, *qchar = NULL;
	u8 ui_soh_data[40] = {0,};
	u8 data[40] = {0,};
	int ret = 0, i = 0;
	u8 val = 0;
	if(bq == NULL)
		return -1;

	memset(t_data, 0, sizeof(t_data));
	strncpy(t_data, buf, 70);
	qchar = t_data;

	while ((pchar = strsep(&qchar, " ")))
	{
		ret = kstrtou8(pchar, 10, &val);
		if (ret < 0) {
			mca_log_err("kstrtou8 error return %d \n", ret);
			return ret;
		}
		ui_soh_data[i] = val;
		val = 0;
		mca_log_err("%s ui_soh_data[%d]: %d \n", bq->log_tag ,i, ui_soh_data[i]);
		i++;
	}

	bq->ui_soh = ui_soh_data[0];
	mca_log_err("%s: bq->ui_soh = %d \n",bq->log_tag, bq->ui_soh);

	mutex_lock(&bq->data_lock);
	ret = fg_mac_read_block(bq, FG_MAC_CMD_UI_SOH, data, 32);
	if (ret < 0) {
		mutex_unlock(&bq->data_lock);
		mca_log_err("failed to get first_usage\n");
		return ret;
	}
	for (i = 0; i < 11; i++)
		data[i] = ui_soh_data[i];

	ret = fg_mac_write_block(bq, addr, data, 32);
	if (ret < 0)
	{
		mutex_unlock(&bq->data_lock);
		mca_log_err("failed to write data \n");
		return ret;
	}
	mutex_unlock(&bq->data_lock);

	return ret;
}

static int ops_fg_get_soh_sn(struct fg_device *fg_dev, char *buf)
{
	struct bq_fg_chip *bq = fuelgauge_get_data(fg_dev);
	mutex_lock(&bq->data_lock);
	bms_get_soh_sn(bq, buf);
	mutex_unlock(&bq->data_lock);
	return 0;
}

static const struct fg_ops fuelgauge_dev_ops = {
	.read_cyclecount = ops_fg_read_cyclecount,
	.read_rsoc = ops_fg_read_rsoc,
	.read_soh = ops_fg_read_soh,
	.get_raw_soc = ops_fg_get_raw_soc,
	.read_current = ops_fg_read_current,
	.read_temperature = ops_fg_read_temperature,
	.read_volt = ops_fg_read_volt,
	.read_status = ops_fg_read_status,
	.update_battery_shutdown_vol = ops_fg_update_battery_shutdown_vol,
	.update_monitor_delay = ops_fg_update_monitor_delay,
	.update_eea_chg_support = ops_fg_update_eea_chg_support,
	.get_charging_done_status = ops_fg_get_charging_done_status,
	.get_en_smooth_full_status = ops_fg_get_en_smooth_full_status,
	.get_rm = ops_fg_get_rm,
	.read_avg_current = ops_fg_read_avg_current,
	.read_i2c_error_count = ops_fg_get_i2c_error_count,
	.read_fcc = ops_fg_read_fcc,
	.read_design_capacity = ops_fg_read_design_capacity,
	.get_fg_chip_ok = ops_fg_get_fg_chip_ok,
	.fg_get_ui_soh = ops_fg_get_ui_soh,
	.fg_set_ui_soh = ops_fg_set_ui_soh,
	.fg_get_soh_sn = ops_fg_get_soh_sn,
};

static int fuelgauge_device_register(struct bq_fg_chip *sc, int work_mode)
{
	switch (work_mode) {
	case FUELGAUGE_SLAVE:
		sc->fg_dev = fg_device_register("fg_slave", sc->dev, sc, &fuelgauge_dev_ops, &sc->fg_props);
		break;
	case FUELGAUGE_MASTER:
		sc->fg_dev = fg_device_register("fg_master", sc->dev, sc, &fuelgauge_dev_ops, &sc->fg_props);
		break;
	default:
		dev_err(sc->dev, "not support work_mode\n");
		return -EINVAL;
	}

	return 0;
}

static int fg_probe(struct i2c_client *client)
{
	struct bq_fg_chip *bq;
	int ret = 0;
	u8 data[5] = {0};

	product_name = RODIN_CN;
	mca_log_info("FG probe enter\n");
	bq = devm_kzalloc(&client->dev, sizeof(*bq), GFP_DMA);
	if (!bq)
		return -ENOMEM;

	bq->dev = &client->dev;
	bq->client = client;
	bq->monitor_delay = FG_MONITOR_DELAY_30S;

	memcpy(bq->regs, bq_fg_regs, NUM_REGS);

	i2c_set_clientdata(client, bq);
	bq->shutdown_mode = false;
	bq->shutdown_flag = false;
	bq->fake_cycle_count = 0;
	bq->extreme_cold_temp = 0;
	bq->raw_soc = -ENODATA;
	bq->last_soc = -EINVAL;
	bq->i2c_error_count = 0;
	bq->ui_soh = 100;
	bq->temp_state = BAT_NORMAL;
	mutex_init(&bq->i2c_rw_lock);
	mutex_init(&bq->data_lock);
	bq->mt6373_reg_vio28 = devm_regulator_get_optional(bq->dev, "fgvio28");
	if(IS_ERR(bq->mt6373_reg_vio28)){
		ret = PTR_ERR(bq->mt6373_reg_vio28);
		if((ret != -ENODEV) && bq->dev->of_node){
			mca_log_err("failed to get vio28 regulator\n");
		}
		mca_log_err("unable to get vio28 regulator\n");
	}else{
		regulator_set_voltage(bq->mt6373_reg_vio28, 3300000, 3300000);
		mca_log_err("success to set vio28 as 3300mv\n");
	}

	bq->regmap = devm_regmap_init_i2c(client, &fg_regmap_config);
	if (IS_ERR(bq->regmap)) {
		mca_log_err("failed to allocate regmap\n");
		return PTR_ERR(bq->regmap);
	}

	ret = fg_parse_dt(bq);
	if (ret) {
		mca_log_err("%s failed to parse DTS\n", bq->log_tag);
		return ret;
	}

	fg_reset_batt_control_gpio(bq);
	fg_check_device(bq);

	battery_shutdown_vol_update(bq);
	probe_time = ktime_get();

	ret = fg_init_psy(bq);
	if (ret) {
		mca_log_err("%s failed to init psy\n", bq->log_tag);
		return ret;
	}

	ret = sysfs_create_group(&bq->dev->kobj, &fg_attr_group);
	if (ret) {
		mca_log_err("%s failed to register sysfs\n", bq->log_tag);
		return ret;
	}
	fuelgauge_device_register(bq, bq->dev_role);
	bq->update_now = true;

	bq->dc = fg_read_dc(bq);

	/* init fast charge mode */
	data[0] = 0;
	mca_log_err("-fastcharge init-\n");
	ret = fg_mac_write_block(bq, FG_MAC_CMD_FASTCHARGE_DIS, data, 2);
	if (ret) {
		mca_log_err("%s failed to write fastcharge = %d\n", bq->log_tag, ret);
	}

	/* update shutdown count thres */
	fg_update_record_voltage_level(bq);
	mca_log_info("%s FG probe success\n", bq->log_tag);

	return 0;
}

static int fg_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	mca_log_err("%s in sleep\n", bq->log_tag);

	return 0;
}

static int fg_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	mca_log_err("%s resume in sleep\n", bq->log_tag);

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

	mca_log_info("%s bq fuel gauge driver shutdown!\n", bq->log_tag);
}

static struct of_device_id fg_match_table[] = {
	{.compatible = "bq28z610",},
	{.compatible = "bq28z610-master",},
	{.compatible = "bq28z610-slave",},
	{},
};
MODULE_DEVICE_TABLE(of, fg_match_table);

static const struct i2c_device_id fg_id[] = {
	{ "bq28z610", 0 },
	{ "bq28z610-master", 1 },
	{ "bq28z610-slave", 2 },
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
