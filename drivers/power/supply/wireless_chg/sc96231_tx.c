// SPDX-License-Identifier: GPL-2.0
/*
 * sc96231_tx.c
 *
 * wireless TRX ic driver
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

#define pr_fmt(fmt)    "[sc96230] %s: " fmt, __func__

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
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
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/semaphore.h>
#include <linux/sched/clock.h>
#include <uapi/linux/sched/types.h>
#include <linux/mca/common/mca_log.h>
#include <linux/mca/common/mca_event.h>

#include "sc96231_reg.h"
#include "sc96231_mtp_program.h"
#include "sc96231_fw.h"
#include "wireless_charger_class.h"

// #ifndef MCA_LOG_TAG
// #define MCA_LOG_TAG "sc96231_tx"
// #endif
static int log_level = 2;

#ifndef MCA_LOG_TAG
#define MCA_LOG_TAG "sc96231_tx"
#endif
#define sc_log_err(fmt, ...)					\
do {								\
	if (log_level >= 0)					\
			printk(KERN_ERR "[SC96231]" fmt, ##__VA_ARGS__);	\
} while (0)

#define sc_log_info(fmt, ...)					\
do {								\
	if (log_level >= 1)					\
			printk(KERN_ERR "[SC96231]" fmt, ##__VA_ARGS__);	\
} while (0)

#define sc_log_dbg(fmt, ...)					\
do {								\
	if (log_level >= 2)					\
			printk(KERN_ERR "[SC96231]" fmt, ##__VA_ARGS__);	\
} while (0)


 #define SC96231_DRV_VERSION     "1.0.0_G"

static ATOMIC_NOTIFIER_HEAD(pen_charge_state_notifier);

//--------------------------IIC API-----------------------------
static const struct regmap_config sc96231_regmap_config = {
    .reg_bits = 16,
    .val_bits = 8,
    .max_register = 0xFFFF,
};

static int __sc96231_read_block(struct sc96231 *sc, uint16_t reg, 
        uint8_t *data, uint8_t length)
{
    int ret;

    ret = regmap_raw_read(sc->regmap, reg, data, length);
    if (ret) {
        mca_log_err("i2c read fail: can't read from reg 0x%04X\n", reg);
        return -EIO;
    }

    return 0;
}

static int __sc96231_write_block(struct sc96231 *sc, uint16_t reg, 
        uint8_t *data, uint8_t length)
{
    int ret;

    ret = regmap_raw_write(sc->regmap, reg, data, length);
    if (ret) {
        mca_log_err("i2c write fail: can't write 0x%04X: %x\n", reg, ret);
        return -EIO;
    }

    return 0;
}

static int sc96231_read_block(struct sc96231 *sc, uint32_t reg, 
        uint8_t *data, uint8_t len)
{
    int ret;
    uint8_t *reg_data;
    uint8_t reg_len;
    uint16_t reg_start;

    if (sc->fw_program) {
        mca_log_err("firmware programming\n");
        return -EBUSY;
    }

    reg_start = (reg - (reg % 4));
    reg_len = (reg + len - reg_start) % 4 == 0 ? 
        (reg + len - reg_start) : ((((reg + len - reg_start) / 4) + 1) * 4);
    
    reg_data = (uint8_t*)kmalloc(reg_len, GFP_KERNEL);
    if (!reg_data) {
        mca_log_err("The Pointer is NULL\n");
        return -ENOMEM;
    }

    //mca_log_err("Enter %s, Reg: %04x, len:%x, reg_start:%04x, reg_len:%x.\n",
    //     __func__, reg, len, reg_start, reg_len);

    mutex_lock(&sc->i2c_rw_lock);
    ret = __sc96231_read_block(sc, reg_start, reg_data, reg_len);
    mutex_unlock(&sc->i2c_rw_lock);

    if (ret >= 0) {
        memcpy(data, reg_data + (reg % 4), len);
    }

    kfree(reg_data);
    return ret;
}

static int sc96231_write_block(struct sc96231 *sc, uint32_t reg,
        uint8_t *data, uint8_t len)
{
    int ret = 0;
    uint8_t *reg_data;
    uint8_t reg_len;
    int reg_start;

    if (sc->fw_program) {
        mca_log_err("firmware programming\n");
        return -EBUSY;
    }

    reg_start = (reg - (reg % 4));
    reg_len = (reg + len - reg_start) % 4 == 0 ? 
        (reg + len - reg_start) : ((((reg + len - reg_start) / 4) + 1) * 4);

    reg_data = (uint8_t*)kmalloc(reg_len, GFP_KERNEL);
    if (!reg_data) {
        mca_log_err("The Pointer is NULL\n");
        return -ENOMEM;
    }

    mutex_lock(&sc->i2c_rw_lock);
    if (reg_start != reg || reg_len != len) {
        ret = __sc96231_read_block(sc, reg_start, reg_data, reg_len);
    }
    memcpy(reg_data + (reg % 4), data, len);
    ret |= __sc96231_write_block(sc, reg_start, reg_data, reg_len);
    mutex_unlock(&sc->i2c_rw_lock);

    kfree(reg_data);
    return ret;
}
//-------------------------------------------------------------

//-------------------sc96231 system interface-------------------
__maybe_unused
int sc96231_get_chipid(struct sc96231 *sc, uint32_t *chip_id)
{
    int ret;

    ret = readCust(sc, cust_tx.ChipID, chip_id);
    if (ret < 0) {
        mca_log_err("sc96231 get chip id fail\n");
    }
    mca_log_info(" -> %x\n", *chip_id);
    return ret;
}

__maybe_unused
int sc96231_tx_get_int(struct sc96231 *sc, uint32_t *txint)
{
	int ret = 0;
	ret = readCust(sc, cust_tx.IRQ_Flag, txint);
	if (ret < 0)
	{
		mca_log_err("sc962x get Tx interrupt fail\n");
	} else {
		mca_log_info("sc962x Tx interrupt=0x%X\n", *txint);
	}
	return ret;
}
__maybe_unused
static int sc96231_tx_set_cmd(struct sc96231 *sc, TX_CMD cmd)
{
    int ret = 0;
    ret = writeCust(sc, cust_tx.Cmd, &cmd);
    if (ret < 0) {
        mca_log_err("tx set cmd=0x%X failed\n", cmd);
        return ret;
    }
    return ret;
}
__maybe_unused
int sc96231_tx_clr_int(struct sc96231 *sc, uint32_t txint)
{
	int ret = 0;
	ret = writeCust(sc, cust_tx.IRQ_Clr, &txint);
	if (ret < 0)
	{
		mca_log_err("sc96231 clear tx interrupt 0x%X fail\n", txint);
		return ret;
	}
	return ret;
}
__maybe_unused
static int sc96231_check_i2c(struct sc96231 *sc)
{
    int ret = 0;
    uint8_t tmp = 0x55;

    ret = writeCust(sc, cust_tx.IIC_Check, &tmp);
    if (ret < 0) {
        mca_log_err("sc96231 write iic_check fail\n");
        return ret;
    }

    mdelay(20);

    ret = readCust(sc, cust_tx.IIC_Check, &tmp);
    if (ret < 0) {
        mca_log_err("sc96231 read iic_check fail\n");
        return ret;
    }

    if (tmp == 0x55) {
        mca_log_info("i2c check ok!\n");
    } else {
        mca_log_err("i2c check failed!\n");
        return -1;
    }

    return ret;
}
__maybe_unused
static int sc96231_tx_enable(struct sc96231 *sc, bool enable)
{
	int ret = 0;

    if (enable) {
        ret = sc96231_tx_set_cmd(sc, AP_CMD_TX_ENABLE);
        if (ret) {
            mca_log_err("enter TX mode fail\n");
        } else {
            mca_log_info("enter TX mode success\n");
        }
    } else {
        ret = sc96231_tx_set_cmd(sc, AP_CMD_TX_DISABLE);
        if (ret) {
            mca_log_err("exit TX mode fail\n");
        } else {
            mca_log_info("exit TX mode success\n");
        }
    }

	return ret;
}
__maybe_unused
static int sc96231_get_ss_reg_value(struct sc96231 *sc, int *ss_value)
{
	int ret = 0;
    uint8_t ss = 0;

	ret = readCust(sc, cust_tx.SigStrength, &ss);
    if (ret < 0) {
        mca_log_err("tx get ss failed\n");
        *ss_value = 0;
        return ret;
    }

    *ss_value = ss;
    sc->reverse_ss = *ss_value;
    mca_log_info("tx get ss :%d\n", sc->reverse_ss);
	return ret;
}
__maybe_unused
static int sc96231_tx_get_voltage(struct sc96231 *sc, uint16_t *volt)
{
	int ret = 0;

	ret = readCust(sc, cust_tx.VOut, volt);
    if (ret < 0) {
        mca_log_err("tx get voltage fail\n");
    } else {
        mca_log_info("tx get voltage :%d\n", *volt);
    }

	return ret;
}
__maybe_unused
static int sc96231_tx_get_current(struct sc96231 *sc, uint16_t *curr)
{
	int ret = 0;

	ret = readCust(sc, cust_tx.IOut, curr);
    if (ret < 0) {
        mca_log_err("tx get current fail\n");
    } else {
        mca_log_info("tx get current :%d\n", *curr);
    }

	return ret;
}
__maybe_unused
static int sc96231_tx_get_tdie(struct sc96231 *sc, uint16_t *tdie)
{
	int ret = 0;

	ret = readCust(sc, cust_tx.T_Die, tdie);
    if (ret < 0) {
        mca_log_err("tx get tdie fail\n");
    } else {
        mca_log_info("tx get tdie :%d\n", *tdie);
    }

	return ret;
}
__maybe_unused
static int sc96231_tx_get_pen_soc(struct sc96231 *sc)
{
    int ret = 0;
    uint8_t battery_level = 0;

    ret = readCust(sc, cust_tx.battery_level, &battery_level);
    if (ret < 0) {
        mca_log_err("sc96231 get data of pen battery level fail\n");
        sc->reverse_pen_soc = 255;
        sc->reverse_pen_full = 0;
        return ret;
    } else {
        mca_log_info("sc96231 get data of pen battery level: 0x%02x\n", battery_level);
    }

    sc->reverse_pen_soc = battery_level & 0x7F;
    sc->reverse_pen_full = (battery_level & 0x80) >> 7;
    mca_log_info("sc96231 get pen soc :%d, pen full: %d\n", sc->reverse_pen_soc, sc->reverse_pen_full);

    if ((sc->reverse_pen_soc > 100) || (sc->reverse_pen_soc < 0))
        sc->reverse_pen_soc = 255;

    return ret;
}
__maybe_unused
static int sc96231_tx_get_pen_ble_mac(struct sc96231 *sc)
{
    int ret = 0;
    uint8_t mac_buf1[2] = { 0 };
    uint8_t mac_buf2[4] = { 0 };

    ret = readCust(sc, cust_tx.mac_byte0_1, mac_buf1);
    if (ret < 0) {
        mca_log_err("sc96231 get data of pen mac1 fail\n");
        return ret;
    }

    ret = readCust(sc, cust_tx.mac_byte2_5, mac_buf2);
    if (ret < 0) {
        mca_log_err("sc96231 get data of pen mac2 fail\n");
        return ret;
    }

    mca_log_info("sc96231 get raw data of pen mac: 0x%02x:0x%02x:0x%02x:0x%02x:0x%02x:0x%02x\n",
        mac_buf1[0], mac_buf1[1], mac_buf2[0], mac_buf2[1], mac_buf2[2], mac_buf2[3]);

    if (mac_buf2[0] == 0 && mac_buf2[1] == 0 && mac_buf2[2] == 0 && mac_buf2[3] == 0) {
        mca_log_err("sc96231 get ble mac fail\n");
        return -1;
    }

    //get pen mac
    sc->pen_mac_data[0] = mac_buf1[0];
    sc->pen_mac_data[1] = mac_buf1[1];
    sc->pen_mac_data[2] = mac_buf2[0];
    sc->pen_mac_data[3] = mac_buf2[1];
    sc->pen_mac_data[4] = mac_buf2[2];
    sc->pen_mac_data[5] = mac_buf2[3];
    mca_log_info("sc96231 get pen mac addr: 0x%02x:0x%02x:0x%02x 0x%02x:0x%02x:0x%02x\n",
        sc->pen_mac_data[0], sc->pen_mac_data[1], sc->pen_mac_data[2],
        sc->pen_mac_data[3], sc->pen_mac_data[4], sc->pen_mac_data[5]);

    return ret;
}
__maybe_unused
static int sc96231_get_fwver(struct sc96231 *sc, uint32_t *fw_ver)
{
    int ret;

    ret = readCust(sc, cust_tx.FirmwareVer, fw_ver);
    if (ret) {
        mca_log_err("sc96231 get fw ver fail\n");
    }

    return ret;
}
__maybe_unused
static int sc96231_get_image_fwver(const unsigned char *firmware, const uint32_t len, uint32_t *image_ver)
{
    if (len < 0x200) {
        mca_log_err("Firmware image length is too short\n");
        return -1;
    }

    *image_ver = (uint32_t)firmware[0X100 + 4] & 0x00FF;
    *image_ver |= ((uint32_t)firmware[0X100 + 5] & 0x00FF) << 8;
    *image_ver |= ((uint32_t)firmware[0X100 + 6] & 0x00FF) << 16;
    *image_ver |= ((uint32_t)firmware[0X100 + 7] & 0x00FF) << 24;

    return 0;
}

//--------------------------------------------------------------

//--------------------------ops functions------------------------
__maybe_unused
static int sc96231_check_firmware_state(bool *update, void *data)
{
    int ret = 0;
    struct sc96231 *chip = (struct sc96231 *)data;
    uint32_t fw_ver = 0, image_ver = 0;

    if (sc96231_check_i2c(chip)) {
        ret = -1;
        goto err;
    }

    ret = sc96231_get_fwver(chip, &fw_ver);
    if (ret)
        goto err;

    ret = sc96231_get_image_fwver(chip->fw_data_ptr, chip->fw_data_size, &image_ver);
    if (ret < 0)
        goto err;

    chip->wls_fw_data->fw_boot_id = fw_ver >> 16;
    chip->wls_fw_data->fw_rx_id = fw_ver >> 8;
    chip->wls_fw_data->fw_tx_id = fw_ver;

    mca_log_info("ic fw version: %02x.%02x.%02x, img version: 0x%x\n",
        chip->wls_fw_data->fw_boot_id, chip->wls_fw_data->fw_rx_id, chip->wls_fw_data->fw_tx_id, image_ver);

    if (fw_ver == 0 || fw_ver < image_ver) {
        mca_log_info("need update\n");
        *update = true;
    } else {
        mca_log_info("no need update\n");
        *update = false;
    }

    return ret;

err:
    mca_log_err("i2c error!\n");
    *update = false;
    return ret;
}
__maybe_unused
static int sc96231_get_fw_version_check(uint8_t *check_result, void *data)
{
    int ret = 0;
    struct sc96231 *sc = (struct sc96231 *)data;
    uint32_t fw_check = 0, fw_ver = 0, image_ver = 0;

    ret = readCust(sc, cust_tx.FirmwareCheck, &fw_check);
    ret |= sc96231_get_fwver(sc, &fw_ver);
    if (ret) {
        mca_log_err("sc96231 get fw check failed\n");
        return ret;
    }

    sc->wls_fw_data->fw_boot_id = fw_ver >> 16;
    sc->wls_fw_data->fw_rx_id = fw_ver >> 8;
    sc->wls_fw_data->fw_tx_id = fw_ver;

    ret = sc96231_get_image_fwver(sc->fw_data_ptr, sc->fw_data_size, &image_ver);

    if (fw_check == fw_ver && fw_ver >= image_ver)
        *check_result = BOOT_CHECK_SUCCESS | RX_CHECK_SUCCESS | TX_CHECK_SUCCESS;

    mca_log_info("sc96231 get fw check success, ver:0x%x, check:0x%x, res:0x%x\n",
        fw_ver, fw_check, *check_result);

    return ret;
}
__maybe_unused
static int sc96231_download_fw(void *data)
{
	int ret = 0;
    struct sc96231 *sc = (struct sc96231 *)data;
	uint32_t image_ver = 0;
	uint32_t cur_ver = 0x00;

    ret = sc96231_get_fwver(sc, &cur_ver);
	if (ret < 0) {
		mca_log_err("sc962x get firmware version before mtp download failed\n");
		return ret;
	}

	ret = sc96231_get_image_fwver(sc->fw_data_ptr, sc->fw_data_size, &image_ver);
	mca_log_info("sc96231 current firmware version=%x, new firmware version=%x\n", cur_ver, image_ver);

    mca_log_info("sc96231 start download mtp\n");
    //need check
    //ret = sc96231_mtp_program(sc, sc96231_fw[chip->fw_version_index], sizeof(sc96231_fw[chip->fw_version_index]));
    ret = sc96231_mtp_program(sc, sc->fw_data_ptr, sc->fw_data_size);
    if (ret < 0) {
        sc->wls_fw_data->fw_boot_id = 0xfe;
        sc->wls_fw_data->fw_rx_id = 0xfe;
        sc->wls_fw_data->fw_tx_id = 0xfe;
        mca_log_err("sc96231_mtp_program:fail, update fw version: 0xfe.\n");
    } 
    mca_log_info("sc96231 download mtp successfully\n");

	return ret;
}
__maybe_unused
static int sc96231_get_firmware_version(char *buf, void *data)
{
    struct sc96231 *chip = (struct sc96231 *)data;

    scnprintf(buf, 10, "%02x.%02x.%02x",
                chip->wls_fw_data->fw_boot_id, chip->wls_fw_data->fw_rx_id, chip->wls_fw_data->fw_tx_id);

    mca_log_info("rx firmware version: boot = 0x%x, rx = 0x%x, tx = 0x%x\n",
        chip->wls_fw_data->fw_boot_id, chip->wls_fw_data->fw_rx_id, chip->wls_fw_data->fw_tx_id);

    return 0;
}
__maybe_unused
static int sc96231_enable_reverse_chg(bool enable, void *data)
{
    struct sc96231 *chip = (struct sc96231 *)data;

    return sc96231_tx_enable(chip, enable);
}
__maybe_unused
static int sc96231_ops_check_i2c(void *data)
{
    int ret = 0;
    struct sc96231 *chip = (struct sc96231 *)data;

    ret = sc96231_check_i2c(chip);

    return ret;
}
__maybe_unused
static int sc96231_set_external_boost_enable(bool enable, void *data)
{
	int ret = 0;
	int en = !!enable;
	int gpio_enable_val = 0;
    struct sc96231 *chip = (struct sc96231 *)data;

	mca_log_info("set external boost enable:%d\n", enable);

	if (enable) {
		if (gpio_is_valid(chip->reverse_txon_gpio)) {
			ret = gpio_request(chip->reverse_txon_gpio, "reverse-txon-gpio");
			if (ret)
				mca_log_err("request txon gpio [%d] failed\n", chip->reverse_txon_gpio);
			ret = gpio_direction_output(chip->reverse_txon_gpio, en);
			if (ret)
				mca_log_err("set txon gpio [%d] output failed\n", chip->reverse_txon_gpio);
			gpio_enable_val = gpio_get_value(chip->reverse_txon_gpio);
			mca_log_err("reverse txon gpio is %d\n", gpio_enable_val);
			gpio_free(chip->reverse_txon_gpio);
		}

		if (gpio_is_valid(chip->reverse_boost_gpio)) {
			ret = gpio_request(chip->reverse_boost_gpio, "reverse-boost-gpio");
			if (ret)
				mca_log_err("request boost gpio [%d] failed\n", chip->reverse_boost_gpio);
			ret = gpio_direction_output(chip->reverse_boost_gpio, en);
			if (ret)
				mca_log_err("set boost gpio [%d] output failed\n", chip->reverse_boost_gpio);
			gpio_free(chip->reverse_boost_gpio);
		}
	} else {
		if (gpio_is_valid(chip->reverse_boost_gpio)) {
			ret = gpio_request(chip->reverse_boost_gpio, "reverse-boost-gpio");
			if (ret)
				mca_log_err("request boost gpio [%d] failed\n", chip->reverse_boost_gpio);
			ret = gpio_direction_output(chip->reverse_boost_gpio, en);
			if (ret)
				mca_log_err("set boost gpio [%d] output failed\n", chip->reverse_boost_gpio);
			gpio_free(chip->reverse_boost_gpio);
		}

		if (gpio_is_valid(chip->reverse_txon_gpio)) {
			ret = gpio_request(chip->reverse_txon_gpio, "reverse-txon-gpio");
			if (ret)
				mca_log_err("request txon gpio [%d] failed\n", chip->reverse_txon_gpio);
			ret = gpio_direction_output(chip->reverse_txon_gpio, en);
			if (ret)
				mca_log_err("set txon gpio [%d] output failed\n", chip->reverse_txon_gpio);
			gpio_enable_val = gpio_get_value(chip->reverse_txon_gpio);
			mca_log_err("reverse txon gpio is %d\n", gpio_enable_val);
			gpio_free(chip->reverse_txon_gpio);
		}
	}
	return ret;
}
__maybe_unused
static int sc96231_get_tx_ss(int *ss, void *data)
{
    struct sc96231 *chip = (struct sc96231 *)data;
    int ret = 0;

    if (!chip->hall3_online && !chip->hall4_online)
        *ss = 0;
    else {
        ret = sc96231_get_ss_reg_value(chip, ss);
        if (ret < 0) {
            mca_log_err("get ss reg value failed\n");
            *ss = 0;
        }
    }

    return 0;
}
__maybe_unused
static int sc96231_get_pen_hall3(int *value, void *data)
{
    struct sc96231 *chip = (struct sc96231 *)data;

    if (chip->hall3_online)
        *value = 0;
    else
        *value = 1;

    return 0;
}
__maybe_unused
static int sc96231_get_pen_hall4(int *value, void *data)
{
    struct sc96231 *chip = (struct sc96231 *)data;

    if (chip->hall4_online)
        *value = 0;
    else
        *value = 1;

    return 0;
}
__maybe_unused
static int sc96231_get_pen_hall3_s(int *value, void *data)
{
    struct sc96231 *chip = (struct sc96231 *)data;

    *value = chip->hall3_s_val;

    return 0;
}
__maybe_unused
static int sc96231_get_pen_hall4_s(int *value, void *data)
{
    struct sc96231 *chip = (struct sc96231 *)data;

    *value = chip->hall4_s_val;

    return 0;
}
__maybe_unused
static int sc96231_get_pen_hall_ppe_n(int *value, void *data)
{
    struct sc96231 *chip = (struct sc96231 *)data;

    *value = chip->hall_ppe_n_val;

    return 0;
}
__maybe_unused
static int sc96231_get_pen_hall_ppe_s(int *value, void *data)
{
    struct sc96231 *chip = (struct sc96231 *)data;

    *value = chip->hall_ppe_s_val;

    return 0;
}
__maybe_unused
static int sc96231_get_pen_place_err(int *value, void *data)
{
    struct sc96231 *chip = (struct sc96231 *)data;

    *value = chip->pen_place_err;

    return 0;
}
__maybe_unused
static int sc96231_set_pen_place_err(int err, void *data)
{
    struct sc96231 *chip = (struct sc96231 *)data;

    chip->pen_place_err = err;

    return 0;
}

__maybe_unused
static int sc96231_get_tx_vout(int *value, void *data)
{
    struct sc96231 *chip = (struct sc96231 *)data;
    int ret = 0;
    u16 tx_vout = 0;


    if (!chip->hall3_online && !chip->hall4_online)
        *value = 0;
    else {
        ret = sc96231_tx_get_voltage(chip, &tx_vout);
        if (ret < 0) {
            mca_log_err("get tx vout failed\n");
            *value  = 0;
        } else {
            *value = tx_vout;
        }
    }
    return 0;
}
__maybe_unused
static int sc96231_get_tx_iout(int *value, void *data)
{
    struct sc96231 *chip = (struct sc96231 *)data;
    int ret = 0;
    u16 tx_iout = 0;


    if (!chip->hall3_online && !chip->hall4_online)
        *value = 0;
    else {
        ret = sc96231_tx_get_current(chip, &tx_iout);
        if (ret < 0) {
            mca_log_err("get tx iout failed\n");
            *value  = 0;
        } else {
            *value = tx_iout;
        }
    }
    return 0;
}
__maybe_unused
static int sc96231_get_tx_tdie(int *temp, void *data)
{
    struct sc96231 *chip = (struct sc96231 *)data;
    int ret = 0;
    u16 tx_tdie = 0;


    if (!chip->hall3_online && !chip->hall4_online)
        *temp = 0;
    else {
        ret = sc96231_tx_get_tdie(chip, &tx_tdie);
        if (ret < 0) {
            mca_log_err("get tx tdie failed\n");
            *temp  = 0;
        } else {
            *temp = tx_tdie;
        }
    }
    return 0;
}

__maybe_unused
static int sc96231_get_pen_soc(int *soc, void *data)
{
    struct sc96231 *chip = (struct sc96231 *)data;
    int ret = 0;

    if (!chip->hall3_online && !chip->hall4_online)
        *soc = 255;
    else {
        ret = sc96231_tx_get_pen_soc(chip);
        if (ret < 0) {
            mca_log_err("get pen soc failed\n");
            *soc  = 255;
        } else {
            *soc = chip->reverse_pen_soc;
        }
    }
    return 0;
}

__maybe_unused
static int sc96231_get_pen_mac(u64 *mac, void *data)
{
    struct sc96231 *sc = (struct sc96231 *)data;
    int ret = 0;
    u64 mac_l = 0;
    u64 mac_h = 0;

    if (!sc->hall3_online && !sc->hall4_online)
        *mac = 0;
    else {
        ret = sc96231_tx_get_pen_ble_mac(sc);
        if (ret < 0) {
            mca_log_err("get pen mac failed\n");
            *mac  = 0;
        } else {
            mac_l = (u64)(sc->pen_mac_data[0] | sc->pen_mac_data[1] << 8 | sc->pen_mac_data[2] << 16 |
            sc->pen_mac_data[3] << 24);
            mac_h = (u64)(sc->pen_mac_data[4] | sc->pen_mac_data[5] << 8);
            // set high 32bit to zero
            mac_l <<= 32;
            mac_l >>= 32;
            *mac = (u64)(mac_h << 32 | mac_l);
        }
    }
    return 0;
}

__maybe_unused
static int sc96231_get_pen_full_flag(int *pen_full, void *data)
{
    struct sc96231 *chip = (struct sc96231 *)data;
    int ret = 0;

    if (!chip->hall3_online && !chip->hall4_online)
        *pen_full = 0;
    else {
        ret = sc96231_tx_get_pen_soc(chip);
        if (ret < 0) {
            mca_log_err("get pen full flag failed\n");
            *pen_full  = 0;
        } else {
            *pen_full = chip->reverse_pen_full;
        }
    }
    return 0;
}

static int sc96231_wls_is_wireless_present(struct wireless_charger_device *chg_dev, bool *present)
{
	*present = false;
	return 0;
}

static int sc96231_wls_get_pen_soc(struct wireless_charger_device *chg_dev, int *soc)
{
	struct sc96231 *chip = dev_get_drvdata(&chg_dev->dev);
	int pen_soc =0;
	sc96231_get_pen_soc(&pen_soc, chip);
	*soc = pen_soc;
	mca_log_err("get pen soc:%d\n", pen_soc);
	return 0;
}

static const struct wireless_charger_properties sc96231_chg_props = {
	.alias_name = "sc96231_wireless_tx_chg",
};
 static const struct wireless_charger_ops sc96231_wls_ops = {
//     .wls_enable_reverse_chg = sc96231_enable_reverse_chg,
//     .wls_check_i2c_is_ok = sc96231_ops_check_i2c,
//     .wls_enable_rev_fod = NULL,
//     .wls_get_fw_version = sc96231_get_firmware_version,
//     .wls_download_fw = sc96231_download_fw,
//     .wls_get_fw_version_check = sc96231_get_fw_version_check,
//     .wls_check_firmware_state = sc96231_check_firmware_state,
//     .wls_set_fw_bin = NULL,
//     .wls_download_fw_from_bin = NULL,
//     .wls_set_external_boost_enable = sc96231_set_external_boost_enable,
//     .wls_get_poweroff_err_code = NULL,
//     .wls_get_tx_vout = sc96231_get_tx_vout,
//     .wls_get_tx_iout = sc96231_get_tx_iout,
//     .wls_get_tx_tdie = sc96231_get_tx_tdie,
//     .wls_get_tx_ss = sc96231_get_tx_ss,
//     .wls_get_pen_mac = sc96231_get_pen_mac,
	.wls_get_pen_soc = sc96231_wls_get_pen_soc,
	.wls_is_wireless_present = sc96231_wls_is_wireless_present,
//     .wls_get_pen_full_flag = sc96231_get_pen_full_flag,
//     .wls_get_pen_hall3 = sc96231_get_pen_hall3,
//     .wls_get_pen_hall4 = sc96231_get_pen_hall4,
//     .wls_get_pen_hall3_s = sc96231_get_pen_hall3_s,
//     .wls_get_pen_hall4_s = sc96231_get_pen_hall4_s,
//     .wls_get_pen_hall_ppe_n = sc96231_get_pen_hall_ppe_n,
//     .wls_get_pen_hall_ppe_s = sc96231_get_pen_hall_ppe_s,
//     .wls_get_pen_place_err = sc96231_get_pen_place_err,
//     .wls_set_pen_place_err = sc96231_set_pen_place_err,
};


static int sc96231_parse_fw_data(struct sc96231 *chip)
{
    int ret = 0;

    switch (chip->project_vendor) {
    case WLS_CHIP_VENDOR_SC96231:
        chip->fw_data_ptr = sc96231_fw[chip->fw_version_index];
        chip->fw_data_size = sizeof(sc96231_fw[chip->fw_version_index]);
        break;
    default:
        chip->fw_data_ptr = sc96231_fw[chip->fw_version_index];
        chip->fw_data_size = sizeof(sc96231_fw[chip->fw_version_index]);
        break;
    }

    mca_log_info("cur fw_data is:[0]:%02x; [1]:%02x; [2]:%02x\n",
                chip->fw_data_ptr[0], chip->fw_data_ptr[1], chip->fw_data_ptr[2]);
    return ret;
}

int mca_wireless_rev_enable_reverse_charge(struct sc96231 *info, bool enable);

static void mca_wireless_reverse_chg_handler(struct sc96231 *info)
{
	// int ret = 0;
	int len;
	char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
	struct mca_event_notify_data event_data = { 0 };
	u8 err_code = 0;
	int ss = 0;
	int pen_soc = 255;
	u64 pen_mac = 0;

	mca_log_info("reverse chg handler, int_index = %d\n", info->proc_data.int_flag);

	switch (info->proc_data.int_flag) {
	case RTX_INT_PING:
		if (!info->tx_timeout_flag)
			cancel_delayed_work_sync(&info->tx_ping_timeout_work);
		schedule_delayed_work(&info->tx_transfer_timeout_work, msecs_to_jiffies(REVERSE_TRANSFER_TIMEOUT_TIMER));
		mca_log_info("RTX_INT_PING");
		break;
	case RTX_INT_GET_RX:
		// ret = platform_class_wireless_enable_rev_fod(WIRELESS_ROLE_MASTER, true);
		// if (ret)
		// 	mca_log_info("RTX_INT_GET_RX: set rev fod failed in GET_RX!\n");
		info->proc_data.reverse_chg_sts = REVERSE_STATE_TRANSFER;
		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
			"POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_TRANSFER);
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);

		if (info->support_tx_only) {
			len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
				"POWER_SUPPLY_REVERSE_PEN_CHG_STATE=%d", REVERSE_STATE_TRANSFER);
			event_data.event = event;
			event_data.event_len = len;
			mca_event_report_uevent(&event_data);
		}
		if (!info->tx_timeout_flag)
			cancel_delayed_work_sync(&info->tx_transfer_timeout_work);
		mca_log_info("RTX_INT_GET_RX: get rx!\n");
		break;
	case RTX_INT_CEP_TIMEOUT:
		info->proc_data.reverse_chg_sts = REVERSE_STATE_WAITPING;
		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
			"POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_WAITPING);
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);

		if (info->support_tx_only) {
			len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
				"POWER_SUPPLY_REVERSE_PEN_CHG_STATE=%d", REVERSE_STATE_WAITPING);
			event_data.event = event;
			event_data.event_len = len;
			mca_event_report_uevent(&event_data);
		}

		schedule_delayed_work(&info->tx_ping_timeout_work, msecs_to_jiffies(REVERSE_PING_TIMEOUT_TIMER));
		mca_log_info("RTX_INT_CEP_TIMEOUT: tx ping timeout!\n");
		break;
	case RTX_INT_EPT:
		schedule_delayed_work(&info->tx_ping_timeout_work, msecs_to_jiffies(REVERSE_PING_TIMEOUT_TIMER));
		mca_log_info("RTX_INT_EPT: end power transfer!\n");
		break;
	case RTX_INT_PROTECTION:
		/*01-tsd  02-tx_oc 03-PK_OC 04-UVLO 05-other*/
		// if (info->proc_data.wireless_reverse_closing == false) {
		// 	(void)platform_class_wireless_get_poweroff_err_code(WIRELESS_ROLE_MASTER, &err_code);
		// 	if (err_code == 4) {
		// 		schedule_delayed_work(&info->disable_tx_work, msecs_to_jiffies(700));
		// 		mca_charge_mievent_report(CHARGE_DFX_WLS_TRX_UVLO, NULL, 0);
		// 	} else if (err_code != 0) {
		// 		mca_wireless_rev_enable_reverse_charge(info, false);
		// 		info->proc_data.reverse_chg_sts = REVERSE_STATE_ENDTRANS;
		// 		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
		// 			"POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
		// 		event_data.event = event;
		// 		event_data.event_len = len;
		// 		mca_event_report_uevent(&event_data);

		// 		if (info->support_tx_only) {
		// 			len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
		// 				"POWER_SUPPLY_REVERSE_PEN_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
		// 			event_data.event = event;
		// 			event_data.event_len = len;
		// 			mca_event_report_uevent(&event_data);
		// 		}

		// 		if (err_code == 2)
		// 			mca_charge_mievent_report(CHARGE_DFX_WLS_TRX_OCP, NULL, 0);
		// 	}
			mca_log_info("RTX_INT_PROTECTION: protection, %d", err_code);
		// } else
		// 	mca_log_info("RTX_INT_PROTECTION: invalid protection");
		break;
	case RTX_INT_GET_TX:
		if (!info->proc_data.wireless_reverse_closing) {
			mca_wireless_rev_enable_reverse_charge(info, false);
			info->proc_data.reverse_chg_sts = REVERSE_STATE_ENDTRANS;
			len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
				"POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
			event_data.event = event;
			event_data.event_len = len;
			mca_event_report_uevent(&event_data);

			if (info->support_tx_only) {
				len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
					"POWER_SUPPLY_REVERSE_PEN_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
				event_data.event = event;
				event_data.event_len = len;
				mca_event_report_uevent(&event_data);
			}

			mca_log_info("RTX_INT_GET_TX: get tx!!");
		}
		break;
	case RTX_INT_REVERSE_TEST_READY:
		mca_log_info("RTX_INT_REVERSE_TEST_READY: receiver reverse test ready, cancel timer");
		// if (!info->tx_timeout_flag)
		// 	cancel_delayed_work_sync(&info->reverse_test_stop_work);
		break;
	case RTX_INT_REVERSE_TEST_DONE:
		mca_wireless_rev_enable_reverse_charge(info, false);
		mca_log_info("RTX_INT_REVERSE_TEST_DONE: reverse test done");
		break;
	case RTX_INT_FOD:
		mca_wireless_rev_enable_reverse_charge(info, false);
		info->proc_data.reverse_chg_sts = REVERSE_STATE_ENDTRANS;
		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
			"POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);

		if (info->support_tx_only) {
			len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
				"POWER_SUPPLY_REVERSE_PEN_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
			event_data.event = event;
			event_data.event_len = len;
			mca_event_report_uevent(&event_data);
		}

		// mca_charge_mievent_report(CHARGE_DFX_WLS_TRX_FOD, NULL, 0);
		mca_log_info("RTX_INT_FOD: fod!!");
		break;
	case RTX_INT_EPT_PKT:
		schedule_delayed_work(&info->tx_transfer_timeout_work, msecs_to_jiffies(REVERSE_TRANSFER_TIMEOUT_TIMER));
		mca_log_info("RTX_INT_EPT_PKT: ept receive!!");
		break;
	case RTX_INT_ERR_CODE:
		// (void)platform_class_wireless_get_tx_err_code(WIRELESS_ROLE_MASTER, &err_code);
		mca_log_err("RTX_INT_ERR_CODE, err_code:0x%02x\n", err_code);
		break;
	case RTX_INT_TX_DET_RX:
		mca_log_info("RTX_INT_TX_DET_RX trigger!");
		cancel_delayed_work_sync(&info->pen_place_err_check_work);
		sc96231_get_tx_ss(&ss, info);
		if (ss < 100) {
			mca_log_info("tx get ss_reg value: %d, pen place err: ss\n", ss);
			mca_wireless_rev_enable_reverse_charge(info, false);
			cancel_delayed_work_sync(&info->pen_data_handle_work);
			sc96231_set_pen_place_err(1, info);
			len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
				"POWER_SUPPLY_PEN_PLACE_ERR=%d", 1);
			event_data.event = event;
			event_data.event_len = len;
			mca_event_report_uevent(&event_data);

			info->proc_data.reverse_chg_sts = REVERSE_STATE_ENDTRANS;
			len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
				"POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
			event_data.event = event;
			event_data.event_len = len;
			mca_event_report_uevent(&event_data);

			if (info->support_tx_only) {
				len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
					"POWER_SUPPLY_REVERSE_PEN_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
				event_data.event = event;
				event_data.event_len = len;
				mca_event_report_uevent(&event_data);
			}
		}
		break;
	case RTX_INT_TX_CONFIG:
		mca_log_info("RTX_INT_TX_CONFIG trigger!");
		schedule_delayed_work(&info->pen_data_handle_work, msecs_to_jiffies(REVERSE_PEN_DELAY_TIMER));
		break;
	case RTX_INT_TX_CHS_UPDATE:
		mca_log_info("RTX_INT_TX_CHS_UPDATE trigger!");
		sc96231_get_pen_soc(&pen_soc, info);
		sc96231_get_pen_mac(&pen_mac, info);
        mca_log_info("rev wireless chg monitor pen_soc:%d pen_mac:%llu", pen_soc, pen_mac);
		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
				"POWER_SUPPLY_REVERSE_PEN_SOC=%d", pen_soc);
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);

		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
				"POWER_SUPPLY_PEN_MAC=%llx", pen_mac);
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);
		break;
	case RTX_INT_TX_BLE_CONNECT:
		mca_log_info("RTX_INT_TX_BLE_CONNECT trigger!");
		sc96231_get_pen_soc(&pen_soc, info);
		sc96231_get_pen_mac(&pen_mac, info);
        mca_log_info("monitor pen_soc:%d pen_mac:%llu", pen_soc, pen_mac);
		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
				"POWER_SUPPLY_REVERSE_PEN_SOC=%d", pen_soc);
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);

		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
				"POWER_SUPPLY_PEN_MAC=%llx", pen_mac);
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);
		break;
	default:
		break;
	}
	info->proc_data.int_flag = 0;
}

static void mca_wireless_rev_process_int_change(struct sc96231 *chip, int value)
{
	chip->proc_data.int_flag = value;
	mca_wireless_reverse_chg_handler(chip);
}

static void xm_wireless_rev_tx_transfer_timeout_work(struct work_struct *work)
{
	struct sc96231 *info = container_of(work,
		struct sc96231, tx_transfer_timeout_work.work);
	int len;
	char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
	struct mca_event_notify_data event_data = { 0 };

	info->tx_timeout_flag = true;
	mca_wireless_rev_enable_reverse_charge(info, false);
	info->proc_data.reverse_chg_sts = REVERSE_STATE_TIMEOUT;
	len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
			"POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_TIMEOUT);
	event_data.event = event;
	event_data.event_len = len;
	mca_event_report_uevent(&event_data);

	if (info->support_tx_only) {
		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
			"POWER_SUPPLY_REVERSE_PEN_CHG_STATE=%d", REVERSE_STATE_TIMEOUT);
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);
	}

	mca_log_info("reverse chg transfer timeout");
	info->tx_timeout_flag = false;
}

// static void xm_wireless_rev_monitor_work(struct work_struct *work)
// {
// 	struct sc96231 *info = container_of(work,
// 		struct sc96231, monitor_work.work);
// 	int isense = 0, vrect = 0, power_good = 0;

// 	platform_class_wireless_get_trx_isense(WIRELESS_ROLE_MASTER, &isense);
// 	platform_class_wireless_get_trx_vrect(WIRELESS_ROLE_MASTER, &vrect);

// 	mca_log_info("wireless revchg: [isense:%d], [vrect:%d]\n", isense, vrect);
// 	(void)platform_class_wireless_is_present(WIRELESS_ROLE_MASTER, &power_good);
// 	if (!power_good)
// 		schedule_delayed_work(&info->monitor_work, msecs_to_jiffies(1000));
// }

static void xm_wireless_rev_tx_ping_timeout_work(struct work_struct *work)
{
	struct sc96231 *info = container_of(work,
		struct sc96231, tx_ping_timeout_work.work);
	int len;
	char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
	struct mca_event_notify_data event_data = { 0 };

	info->tx_timeout_flag = true;
	mca_wireless_rev_enable_reverse_charge(info, false);
	info->proc_data.reverse_chg_sts = REVERSE_STATE_ENDTRANS;
	len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
			"POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
	event_data.event = event;
	event_data.event_len = len;
	mca_event_report_uevent(&event_data);

	if (info->support_tx_only) {
		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
			"POWER_SUPPLY_REVERSE_PEN_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);
	}

	mca_log_info("reverse chg ping timeout");
	info->tx_timeout_flag = false;
}

static int mca_wireless_rev_external_boost_config(struct sc96231 *info, int enable)
{
	if (!info)
		return -1;

	if (enable) {
        if (gpio_is_valid(info->reverse_txon_gpio)){
			gpio_set_value(info->reverse_txon_gpio, !!enable);
			mca_log_info("reverse_txon_gpio enable\n");
		}
        msleep(5);
        if (gpio_is_valid(info->reverse_boost_gpio)){
			gpio_set_value(info->reverse_boost_gpio, !!enable);
			mca_log_info("reverse_boost_gpio enable\n");
		}
        mca_log_info("boost enable\n");
        msleep(15);
	} else {
         if (gpio_is_valid(info->reverse_boost_gpio)){
			gpio_set_value(info->reverse_boost_gpio, !!enable);
			mca_log_info("reverse_boost_gpio disable\n");
		}
         msleep(5);
		if (gpio_is_valid(info->reverse_txon_gpio)){
			gpio_set_value(info->reverse_txon_gpio, !!enable);
			mca_log_info("reverse_txon_gpio disable\n");
		}
        mca_log_info("boost disable\n");
	}

	mca_log_err("set reverse boost done!!!");
	return 0;
}

static int mca_wireless_rev_charge_config(struct sc96231 *info, int enable)
{
	int ret = 0;
	// int power_good = 0;

	if (!info)
		return -1;

	mca_log_err("wireless_reverse_charge_config\n");

	// if (info->proc_data.wls_sleep_fw_update) {
	// 	if (info->rev_boost_default == PMIC_REV_BOOST) {
	// 		ret = mca_wireless_rev_firmware_update_boost_config(enable);
	// 		mca_log_err("fw update start boost config!!!");
	// 		return ret;
	// 	} else if (info->rev_boost_default == EXTERNAL_BOOST) {
	// 		ret = mca_wireless_rev_external_boost_config(enable);
	// 		mca_log_info("fw update start external boost config!!!");
	// 		return ret;
	// 	}
	// }
	ret = mca_wireless_rev_external_boost_config(info, enable);
    mca_log_err("end boost config!!!");

	return ret;
}

static void xm_wireless_rev_charge_config_work(struct work_struct *work)
{
	struct sc96231 *info = container_of(work,
		struct sc96231, reverse_charge_config_work.work);
	int ret = 0;
	// int usb_real_type = 0;
	int len;
	char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
	struct mca_event_notify_data event_data = { 0 };

	if (!info->proc_data.reverse_chg_en) {
		mca_log_info("reverse_chg_en has been close, return\n");
		return;
	}

	mca_log_err("start reverse_chg_config!!!\n");
	ret = mca_wireless_rev_charge_config(info, true);
	mca_log_err("stop reverse_chg_config!!!ret = %d", ret);

	if (!info->proc_data.user_reverse_chg) {
		mca_log_err("user close reverse charge!!!");
		mca_wireless_rev_enable_reverse_charge(info, false);
		return;
	}

	if (ret) {
		mca_log_err("reverse charge fail!!!");
		mca_wireless_rev_enable_reverse_charge(info, false);
		mca_log_err("reverse charge fail!!!end");
		info->proc_data.reverse_chg_sts = REVERSE_STATE_ENDTRANS;
		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
			"POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);

		if (info->support_tx_only) {
			len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
				"POWER_SUPPLY_REVERSE_PEN_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
			event_data.event = event;
			event_data.event_len = len;
			mca_event_report_uevent(&event_data);
		}

		return;
	}

	ret = sc96231_enable_reverse_chg(true, info);
	mca_log_err("reverse charge success = %d!!!", ret);
}


int mca_wireless_rev_enable_reverse_charge(struct sc96231 *info, bool enable)
{
	// int power_good = 0;
	int pen_hall3;
	int pen_hall4;
	int ret = 0;
    struct mca_event_notify_data event_data = { 0 };
	char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
	int len;

	if (!info)
		return -1;

	if (info->proc_data.fw_updating) {
		info->proc_data.reverse_chg_sts = REVERSE_STATE_ENDTRANS;
		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
			"POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);

		if (info->support_tx_only) {
			len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
				"POWER_SUPPLY_REVERSE_PEN_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
			event_data.event = event;
			event_data.event_len = len;
			mca_event_report_uevent(&event_data);
		}

		mca_log_info("fw updating, don't enable reverse charge\n");
		return -1;
	}

	if (enable) {
		if (info->support_tx_only) {
			sc96231_get_pen_hall3(&pen_hall3, info);
			sc96231_get_pen_hall4(&pen_hall4, info);
			if (pen_hall3 && pen_hall4) {
				mca_log_info("pen is not attached, don't enable reverse charge\n");
				return -1;
			}
		}

		pm_stay_awake(info->dev);
		info->proc_data.reverse_chg_en = true;
		info->proc_data.reverse_chg_sts = REVERSE_STATE_OPEN;
		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
			"POWER_SUPPLY_REVERSE_CHG_MODE=%d", REVERSE_CHARGE_OPEN);
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);

		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
			"POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_OPEN);
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);

		if (info->support_tx_only) {
			len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
				"POWER_SUPPLY_REVERSE_PEN_CHG_STATE=%d", REVERSE_STATE_OPEN);
			event_data.event = event;
			event_data.event_len = len;
			mca_event_report_uevent(&event_data);
		}

		mca_log_err("start reverse_charge_config_work\n");
		if (info->support_tx_only)
			schedule_delayed_work(&info->reverse_charge_config_work, msecs_to_jiffies(0));
		else
			schedule_delayed_work(&info->reverse_charge_config_work, msecs_to_jiffies(200));
		schedule_delayed_work(&info->tx_ping_timeout_work, msecs_to_jiffies(REVERSE_PING_TIMEOUT_TIMER));
		//schedule_delayed_work(&info->monitor_work, msecs_to_jiffies(300));
		if (info->support_tx_only)
			schedule_delayed_work(&info->pen_place_err_check_work, msecs_to_jiffies(REVERSE_PPE_TIMEOUT_TIMER));
	} else {
		mca_log_err("close");
		if (!info->proc_data.wireless_reverse_closing) {
			info->proc_data.wireless_reverse_closing = true;
			// mca_charge_mievent_set_state(MIEVENT_STATE_PLUG, 0);
			//cancel_delayed_work_sync(&info->monitor_work);
			ret |= sc96231_enable_reverse_chg(false, info);;
			mca_log_err("start reverse_charge_config\n");
			ret = mca_wireless_rev_charge_config(info, false);
			mca_log_err("stop reverse_charge_config\n");
			info->proc_data.reverse_chg_en = false;
			len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
				"POWER_SUPPLY_REVERSE_CHG_MODE=%d", REVERSE_CHARGE_CLOSE);
			event_data.event = event;
			event_data.event_len = len;
			mca_event_report_uevent(&event_data);
			if (!info->tx_timeout_flag) {
				cancel_delayed_work_sync(&info->tx_ping_timeout_work);
				cancel_delayed_work_sync(&info->tx_transfer_timeout_work);
			}
			cancel_delayed_work_sync(&info->pen_place_err_check_work);
			if (!info->proc_data.reverse_chg_en)
				pm_relax(info->dev);
			info->proc_data.wireless_reverse_closing = false;
		}
		mca_log_err("close end");
	}

	return ret;
}
EXPORT_SYMBOL(mca_wireless_rev_enable_reverse_charge);

void mca_event_report_multi_uevent(struct sc96231 *info, const struct mca_event_notify_data *n_data1,
                                    const struct mca_event_notify_data *n_data2)
{
	char uevent_buf1[MCA_EVENT_NOTIFY_SIZE] = { 0 };
	char uevent_buf2[MCA_EVENT_NOTIFY_SIZE] = { 0 };
	char *envp[3] = { uevent_buf1, uevent_buf2, NULL };
	int ret = 0;

	if (!info) {
		sc_log_err("%s: mtk_charger dev is null\n", __func__);
		return;
	}

	if (!n_data1 || !n_data1->event || !n_data2 || !n_data2->event) {
		sc_log_err("%s: n_data or event is null\n", __func__);
		return;
	}

	if (n_data1->event_len >= MCA_EVENT_NOTIFY_SIZE || n_data2->event_len >= MCA_EVENT_NOTIFY_SIZE) {
		sc_log_err("%s: event_len is invalid\n", __func__);
		return;
	}

	memcpy(uevent_buf1, n_data1->event, n_data1->event_len);
	memcpy(uevent_buf2, n_data2->event, n_data2->event_len);
	sc_log_info("%s: receive uevent_buf %d, %s\n", __func__, n_data1->event_len, uevent_buf1);
	sc_log_info("%s: receive uevent_buf %d, %s\n", __func__, n_data2->event_len, uevent_buf2);

	ret = kobject_uevent_env(&info->dev->kobj, KOBJ_CHANGE, envp);
	if (ret < 0) {
		sc_log_info("%s: notify uevent fail, ret=%d\n", __func__, ret);
	}
}

static void mca_wireless_rev_process_hall_change(struct sc96231 *chip, int value)
{
	bool is_pen_attached = !!value;
	bool pen_hall3_gpio_value = !(value & (1 << 3));
	bool pen_hall4_gpio_value = !(value & (1 << 4));
	char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
	char event2[MCA_EVENT_NOTIFY_SIZE] = { 0 };
	struct mca_event_notify_data event_data = { 0 };
	struct mca_event_notify_data event_data2 = { 0 };
	int len, len2;
	u8 check_result = 0;

	mca_log_info("pen hall change, set reverse charge: %d\n", is_pen_attached);
	if (!chip->fw_program)
		mca_wireless_rev_enable_reverse_charge(chip, is_pen_attached);

	if (!is_pen_attached) {
		cancel_delayed_work_sync(&chip->pen_data_handle_work);
		cancel_delayed_work_sync(&chip->pen_place_err_check_work);
		chip->proc_data.reverse_chg_sts = REVERSE_STATE_ENDTRANS;
		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
			"POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);

		if (chip->support_tx_only) {
			len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
				"POWER_SUPPLY_REVERSE_PEN_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
			event_data.event = event;
			event_data.event_len = len;
			mca_event_report_uevent(&event_data);

			len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
				"POWER_SUPPLY_PEN_PLACE_ERR=%d", 0);
			event_data.event = event;
			event_data.event_len = len;
			mca_event_report_uevent(&event_data);
		}
	}

	atomic_notifier_call_chain(&pen_charge_state_notifier, is_pen_attached, NULL);
	mca_log_info("pen_charge_state_notifier: %d\n", is_pen_attached);

	len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_PEN_HALL3=%d", pen_hall3_gpio_value);
	event_data.event = event;
	event_data.event_len = len;
	len2 = snprintf(event2, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_PEN_HALL4=%d", pen_hall4_gpio_value);
	event_data2.event = event2;
	event_data2.event_len = len2;
	mca_event_report_multi_uevent(chip, &event_data, &event_data2);
	sc96231_get_fw_version_check(&check_result, chip);
	mca_log_info("rx firmware version: boot = 0x%x, rx = 0x%x, tx = 0x%x, check_result = %d\n",
								chip->wls_fw_data->fw_boot_id, chip->wls_fw_data->fw_rx_id, chip->wls_fw_data->fw_tx_id, check_result);
}

static void xm_wireless_rev_process_ppe_hall_change(struct sc96231 *info)
{
	char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
	struct mca_event_notify_data event_data = { 0 };
	int len;

	sc_log_err("pen ppe hall change, place err: %d\n", info->pen_place_err);

	if (info->pen_place_err) {
		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
			"POWER_SUPPLY_PEN_PLACE_ERR=%d", info->pen_place_err);
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);

		if (info->proc_data.reverse_chg_en && info->proc_data.reverse_chg_sts == REVERSE_STATE_TRANSFER) {
			mca_wireless_rev_enable_reverse_charge(info, false);
			cancel_delayed_work_sync(&info->pen_data_handle_work);
			info->proc_data.reverse_chg_sts = REVERSE_STATE_ENDTRANS;
			len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
				"POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
			event_data.event = event;
			event_data.event_len = len;
			mca_event_report_uevent(&event_data);

			if (info->support_tx_only) {
				len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
					"POWER_SUPPLY_REVERSE_PEN_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
				event_data.event = event;
				event_data.event_len = len;
				mca_event_report_uevent(&event_data);
			}
		}
	} else {
		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
			"POWER_SUPPLY_PEN_PLACE_ERR=%d", 0);
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);
	}
}

static int sc96231_parse_dt(struct sc96231 *chip)
{
    struct device_node *node = chip->dev->of_node;

    if (!node) {
        mca_log_err("No DT data Failing Probe\n");
        return -EINVAL;
    }

    (void)of_property_read_u32(node, "project_vendor", &chip->project_vendor);
    (void)of_property_read_u32(node, "rx_role", &chip->rx_role);
    (void)of_property_read_u32(node, "fw_version_index", &chip->fw_version_index);

    mca_log_err("project_vendor=%d, rx_role=%d, fw_version_index=%d", 
        chip->project_vendor, chip->rx_role, chip->fw_version_index);

    sc96231_parse_fw_data(chip);

    chip->irq_gpio = of_get_named_gpio(node, "rx-int", 0);
    if (!gpio_is_valid(chip->irq_gpio)) {
        mca_log_err("fail_irq_gpio %d\n", chip->irq_gpio);
        return -EINVAL;
    }

    chip->hall3_gpio = of_get_named_gpio(node, "hall-int3", 0);
    if (!gpio_is_valid(chip->hall3_gpio)) {
        mca_log_err("fail_hall3_gpio %d\n", chip->hall3_gpio);
        return -EINVAL;
    }

    chip->hall4_gpio = of_get_named_gpio(node, "hall-int4", 0);
    if (!gpio_is_valid(chip->hall4_gpio)) {
        mca_log_err("fail_hall4_gpio %d\n", chip->hall4_gpio);
        return -EINVAL;
    }

    chip->hall3_s_gpio = of_get_named_gpio(node, "hall-int3-s", 0);
    if (!gpio_is_valid(chip->hall3_s_gpio)) {
        mca_log_err("fail_hall3_s_gpio %d\n", chip->hall3_s_gpio);
        return -EINVAL;
    }

    chip->hall4_s_gpio = of_get_named_gpio(node, "hall-int4-s", 0);
    if (!gpio_is_valid(chip->hall4_s_gpio)) {
        mca_log_err("fail_hall4_s_gpio %d\n", chip->hall4_s_gpio);
        return -EINVAL;
    }

    chip->hall_ppe_n_gpio = of_get_named_gpio(node, "hall-ppe-n", 0);
    if (!gpio_is_valid(chip->hall_ppe_n_gpio)) {
        mca_log_err("fail_hall_ppe_n_gpio %d\n", chip->hall_ppe_n_gpio);
        // return -EINVAL;
    }

    chip->hall_ppe_s_gpio = of_get_named_gpio(node, "hall-ppe-s", 0);
    if (!gpio_is_valid(chip->hall_ppe_s_gpio)) {
        mca_log_err("fail_hall_ppe_s_gpio %d\n", chip->hall_ppe_s_gpio);
        // return -EINVAL;
    }

    chip->reverse_txon_gpio = of_get_named_gpio(node, "reverse-txon-gpio", 0);
    if ((!gpio_is_valid(chip->reverse_txon_gpio))) {
        mca_log_err("fail_get_reverse_txon_gpio %d\n", chip->reverse_txon_gpio);
        return -EINVAL;
    }

    chip->reverse_boost_gpio = of_get_named_gpio(node, "reverse-boost-gpio", 0);
    if ((!gpio_is_valid(chip->reverse_boost_gpio))) {
        mca_log_err("fail_get_reverse_boost_gpio %d\n", chip->reverse_boost_gpio);
        return -EINVAL;
    }

    return 0;
}


static int_map_t tx_irq_map[] = {
    DECL_INTERRUPT_MAP(WP_IRQ_TX_PING, RTX_INT_PING),
    DECL_INTERRUPT_MAP(WP_IRQ_TX_POWER_TRANSFER, RTX_INT_GET_RX),
    DECL_INTERRUPT_MAP(WP_IRQ_TX_CEP_TIMEOUT, RTX_INT_CEP_TIMEOUT),
    DECL_INTERRUPT_MAP(WP_IRQ_TX_REMOVE_POWER , RTX_INT_EPT),
    DECL_INTERRUPT_MAP(WP_IRQ_TX_DET_TX, RTX_INT_GET_TX),
    DECL_INTERRUPT_MAP(WP_IRQ_TX_FOD, RTX_INT_FOD),
    DECL_INTERRUPT_MAP(WP_IRQ_TX_DET_RX, RTX_INT_TX_DET_RX),
    DECL_INTERRUPT_MAP(WP_IRQ_TX_CFG_PKT, RTX_INT_TX_CONFIG),
    DECL_INTERRUPT_MAP(WP_IRQ_TX_CHS_UPDATE  , RTX_INT_TX_CHS_UPDATE),
    DECL_INTERRUPT_MAP(WP_IRQ_TX_BLE_CONNECT, RTX_INT_TX_BLE_CONNECT),
};

static void sc96231_ppe_hall_interrupt_work(struct work_struct *work)
{
    struct sc96231 *chip = container_of(work, struct sc96231, ppe_hall_irq_work.work);
    int hall3_s_gpio_val = 1;
    int hall4_s_gpio_val = 1;
    int hall_ppe_n_gpio_val = 1;
    int hall_ppe_s_gpio_val = 1;

    if (gpio_is_valid(chip->hall3_s_gpio)) {
        hall3_s_gpio_val = gpio_get_value(chip->hall3_s_gpio);
        chip->hall3_s_val = hall3_s_gpio_val;
        mca_log_info("hall3_s gpio is %d \n", hall3_s_gpio_val);
    } else {
        mca_log_err("hall3_s irq gpio not provided\n");
        return;
    }

    if (gpio_is_valid(chip->hall4_s_gpio)) {
        hall4_s_gpio_val = gpio_get_value(chip->hall4_s_gpio);
        chip->hall4_s_val = hall4_s_gpio_val;
        mca_log_info("hall4_s gpio is %d \n", hall4_s_gpio_val);
    } else {
        mca_log_err("hall4_s irq gpio not provided\n");
        return;
    }

    if (gpio_is_valid(chip->hall_ppe_n_gpio)) {
        hall_ppe_n_gpio_val = gpio_get_value(chip->hall_ppe_n_gpio);
        chip->hall_ppe_n_val = hall_ppe_n_gpio_val;
        mca_log_info("hall_ppe_n gpio is %d \n", hall_ppe_n_gpio_val);
    } else {
        mca_log_err("hall_ppe_n irq gpio not provided\n");
        // return;
    }

    if (gpio_is_valid(chip->hall_ppe_s_gpio)) {
        hall_ppe_s_gpio_val = gpio_get_value(chip->hall_ppe_s_gpio);
        chip->hall_ppe_s_val = hall_ppe_s_gpio_val;
        mca_log_info("hall_ppe_s gpio is %d \n", hall_ppe_s_gpio_val);
    } else {
        mca_log_err("hall_ppe_s irq gpio not provided\n");
        // return;
    }

    if (!hall3_s_gpio_val || !hall4_s_gpio_val) { //|| !hall_ppe_n_gpio_val || !hall_ppe_s_gpio_val) {
        mca_log_info("ppe hall triger, pen place error\n");
        chip->pen_place_err = PPE_HALL;
    } else {
		mca_log_info("ppe hall triger, pen place accurate\n");
        chip->pen_place_err = PPE_NONE;
    }
    xm_wireless_rev_process_ppe_hall_change(chip);

    return;
}

static void sc96231_hall_interrupt_work(struct work_struct *work)
{
    struct sc96231 *chip = container_of(work, struct sc96231, hall_irq_work.work);
    int hall3_gpio_val = 1;
    int hall4_gpio_val = 1;

    if (gpio_is_valid(chip->hall3_gpio)) {
        hall3_gpio_val = gpio_get_value(chip->hall3_gpio);
        mca_log_info("hall3 gpio is %d \n", hall3_gpio_val);
    } else {
        mca_log_err("hall3 irq gpio not provided\n");
        return;
    }

    if (gpio_is_valid(chip->hall4_gpio)) {
        hall4_gpio_val = gpio_get_value(chip->hall4_gpio);
        mca_log_info("hall4 gpio is %d \n", hall4_gpio_val);
    } else {
        mca_log_err("hall4 irq gpio not provided\n");
        return;
    }

    if (!hall3_gpio_val || !hall4_gpio_val) {
        if (chip->hall3_online == 1 || chip->hall4_online == 1) {
            mca_log_info("hall3 or hall4 is already online, return\n");
            return;
        }
        mca_log_info("pen attached!\n");

        if (!hall3_gpio_val)
            chip->hall3_online = 1;
        if (!hall4_gpio_val)
            chip->hall4_online = 1;

        mca_log_info("hall3 online: %d, hall4 online: %d\n", chip->hall3_online, chip->hall4_online);
        //TODO
        // mca_strategy_func_process(STRATEGY_FUNC_TYPE_REV_WIRELESS,
        //     MCA_EVENT_WIRELESS_PEN_HALL_CHANGE, chip->hall3_online << 3 | chip->hall4_online << 4);
        mca_wireless_rev_process_hall_change(chip, chip->hall3_online << 3 | chip->hall4_online << 4);

    } else {
        if (chip->hall3_online == 0 && chip->hall4_online == 0) {
            mca_log_info("hall3 and hall4 are already offline, return \n");
            return;
        }
        chip->hall3_online = 0;
        chip->hall4_online = 0;
        mca_log_info("pen dettached!\n");
        mca_wireless_rev_process_hall_change(chip, 0);
    }

    return;
}


static void sc96231_interrupt_work(struct work_struct *work)
{
    struct sc96231 *chip = container_of(work, struct sc96231, interrupt_work.work);
    int ret = 0;
    uint32_t regval;
    int i = 0;

    mutex_lock(&chip->wireless_chg_int_lock);

    ret = sc96231_tx_get_int(chip, &regval);
    if (ret < 0) {
        mca_log_err("get tx int flag fail\n");
        goto exit;
    }

    // clear intflag first
    sc96231_tx_clr_int(chip, regval);

    for (i = 0; i < ARRAY_SIZE(tx_irq_map); i++) {
        if (tx_irq_map[i].irq_regval & regval) {
            chip->proc_data.int_flag = tx_irq_map[i].irq_flag;
            mca_log_info("process irq flag: %d\n", chip->proc_data.int_flag);
            mca_wireless_rev_process_int_change(chip, chip->proc_data.int_flag);
        }
    }

exit:
    chip->proc_data.int_flag = 0;
    mutex_unlock(&chip->wireless_chg_int_lock);
    mca_log_info("already clear int and unlock mutex\n");
}

static irqreturn_t sc96231_interrupt_handler(int irq, void *dev_id)
{
    struct sc96231 *chip = dev_id;
    schedule_delayed_work(&chip->interrupt_work, msecs_to_jiffies(0));
    return IRQ_HANDLED;
}

static irqreturn_t sc96231_hall3_interrupt_handler(int irq, void *dev_id)
{
    struct sc96231 *chip = dev_id;
    mca_log_err("hall3 irq\n");
    schedule_delayed_work(&chip->hall_irq_work, msecs_to_jiffies(0));
    return IRQ_HANDLED;
}

static irqreturn_t sc96231_hall4_interrupt_handler(int irq, void *dev_id)
{
    struct sc96231 *chip = dev_id;
    mca_log_err("hall4 irq\n");
    schedule_delayed_work(&chip->hall_irq_work, msecs_to_jiffies(0));
    return IRQ_HANDLED;
}

static irqreturn_t sc96231_hall3_s_interrupt_handler(int irq, void *dev_id)
{
    struct sc96231 *chip = dev_id;
    mca_log_err("hall3_s irq\n");
    schedule_delayed_work(&chip->ppe_hall_irq_work, msecs_to_jiffies(0));
    return IRQ_HANDLED;
}

static irqreturn_t sc96231_hall4_s_interrupt_handler(int irq, void *dev_id)
{
    struct sc96231 *chip = dev_id;
    mca_log_err("hall4_s irq\n");
    schedule_delayed_work(&chip->ppe_hall_irq_work, msecs_to_jiffies(0));
    return IRQ_HANDLED;
}

static irqreturn_t sc96231_hall_ppe_n_interrupt_handler(int irq, void *dev_id)
{
    struct sc96231 *chip = dev_id;
    schedule_delayed_work(&chip->ppe_hall_irq_work, msecs_to_jiffies(0));
    return IRQ_HANDLED;
}

static irqreturn_t sc96231_hall_ppe_s_interrupt_handler(int irq, void *dev_id)
{
    struct sc96231 *chip = dev_id;
    schedule_delayed_work(&chip->ppe_hall_irq_work, msecs_to_jiffies(0));
    return IRQ_HANDLED;
}

static int sc96231_irq_init(struct sc96231 *chip)
{
    int ret = 0;

    if (gpio_is_valid(chip->irq_gpio)) {
        chip->irq = gpio_to_irq(chip->irq_gpio);
        if (chip->irq < 0) {
            mca_log_err("gpio_to_irq Fail!\n");
            goto fail_irq_gpio;
        }
    } else {
        mca_log_err("irq gpio not provided\n");
        goto fail_irq_gpio;
    }
    if (chip->irq) {
        ret = devm_request_threaded_irq(&chip->client->dev, chip->irq, NULL,
                sc96231_interrupt_handler,
                (IRQF_TRIGGER_FALLING | IRQF_ONESHOT),
                "sc96231_chg_stat_irq", chip);
        if (ret)
            mca_log_err("Failed irq = %d ret = %d\n", chip->irq, ret);
    }
    enable_irq_wake(chip->irq);

    if (gpio_is_valid(chip->hall3_gpio)) {
        chip->hall3_irq = gpio_to_irq(chip->hall3_gpio);
        if (chip->hall3_irq < 0) {
            mca_log_err("hall3 gpio_to_irq Fail!\n");
            goto fail_hall3_irq_gpio;
        }
    } else {
        mca_log_err("hall3 irq gpio not provided\n");
        goto fail_hall3_irq_gpio;
    }
    if (chip->hall3_irq) {
        ret = devm_request_threaded_irq(&chip->client->dev, chip->hall3_irq, NULL,
                sc96231_hall3_interrupt_handler,
                (IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT),
                "sc96231_hall3_irq", chip);
        if (ret)
            mca_log_err("Failed hall3 irq = %d ret = %d\n", chip->hall3_irq, ret);
    }
    enable_irq_wake(chip->hall3_irq);

    if (gpio_is_valid(chip->hall4_gpio)) {
        chip->hall4_irq = gpio_to_irq(chip->hall4_gpio);
        if (chip->hall4_irq < 0) {
            mca_log_err("hall4 gpio_to_irq Fail!\n");
            goto fail_hall4_irq_gpio;
        }
    } else {
        mca_log_err("hall4 irq gpio not provided\n");
        goto fail_hall4_irq_gpio;
    }
    if (chip->hall4_irq) {
        ret = devm_request_threaded_irq(&chip->client->dev, chip->hall4_irq, NULL,
                sc96231_hall4_interrupt_handler,
                (IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT),
                "sc96231_hall4_irq", chip);
        if (ret)
            mca_log_err("Failed hall4 irq = %d ret = %d\n", chip->hall4_irq, ret);
    }
    enable_irq_wake(chip->hall4_irq);
 
    if (gpio_is_valid(chip->hall3_s_gpio)) {
        chip->hall3_s_irq = gpio_to_irq(chip->hall3_s_gpio);
        if (chip->hall3_s_irq < 0) {
            mca_log_err("hall3_s gpio_to_irq Fail!\n");
            goto fail_hall3_s_irq_gpio;
        }
    } else {
        mca_log_err("hall3_s irq gpio not provided\n");
        goto fail_hall3_s_irq_gpio;
    }
    if (chip->hall3_s_irq) {
        ret = devm_request_threaded_irq(&chip->client->dev, chip->hall3_s_irq, NULL,
                sc96231_hall3_s_interrupt_handler,
                (IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT),
                "sc96231_hall3_s_irq", chip);
        if (ret)
            mca_log_err("Failed hall3_s irq = %d ret = %d\n", chip->hall3_s_irq, ret);
    }
    enable_irq_wake(chip->hall3_s_irq);

    if (gpio_is_valid(chip->hall4_s_gpio)) {
        chip->hall4_s_irq = gpio_to_irq(chip->hall4_s_gpio);
        if (chip->hall4_s_irq < 0) {
            mca_log_err("hall4_s gpio_to_irq Fail!\n");
            goto fail_hall4_s_irq_gpio;
        }
    } else {
        mca_log_err("hall4_s irq gpio not provided\n");
        goto fail_hall4_s_irq_gpio;
    }
    if (chip->hall4_s_irq) {
        ret = devm_request_threaded_irq(&chip->client->dev, chip->hall4_s_irq, NULL,
                sc96231_hall4_s_interrupt_handler,
                (IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT),
                "sc96231_hall4_s_irq", chip);
        if (ret)
            mca_log_err("Failed hall4_s irq = %d ret = %d\n", chip->hall4_s_irq, ret);
    }
    enable_irq_wake(chip->hall4_s_irq);

    if (gpio_is_valid(chip->hall_ppe_n_gpio)) {
        chip->hall_ppe_n_irq = gpio_to_irq(chip->hall_ppe_n_gpio);
        if (chip->hall_ppe_n_irq < 0) {
            mca_log_err("hall_ppe_n gpio_to_irq Fail!\n");
            goto fail_hall_ppe_n_irq_gpio;
        }
    } else {
        mca_log_err("hall_ppe_n irq gpio not provided\n");
        goto fail_hall_ppe_n_irq_gpio;
    }
    if (chip->hall_ppe_n_irq) {
        ret = devm_request_threaded_irq(&chip->client->dev, chip->hall_ppe_n_irq, NULL,
                sc96231_hall_ppe_n_interrupt_handler,
                (IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT),
                "sc96231_hall_ppe_n_irq", chip);
        if (ret)
            mca_log_err("Failed hall_ppe_n irq = %d ret = %d\n", chip->hall_ppe_n_irq, ret);
    }
    enable_irq_wake(chip->hall_ppe_n_irq);

    if (gpio_is_valid(chip->hall_ppe_s_gpio)) {
        chip->hall_ppe_s_irq = gpio_to_irq(chip->hall_ppe_s_gpio);
        if (chip->hall_ppe_s_irq < 0) {
            mca_log_err("hall_ppe_s gpio_to_irq Fail!\n");
            goto fail_hall_ppe_s_irq_gpio;
        }
    } else {
        mca_log_err("hall_ppe_s irq gpio not provided\n");
        goto fail_hall_ppe_s_irq_gpio;
    }
    if (chip->hall_ppe_s_irq) {
        ret = devm_request_threaded_irq(&chip->client->dev, chip->hall_ppe_s_irq, NULL,
                sc96231_hall_ppe_s_interrupt_handler,
                (IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT),
                "sc96231_hall_ppe_s_irq", chip);
        if (ret)
            mca_log_err("Failed hall_ppe_s irq = %d ret = %d\n", chip->hall_ppe_s_irq, ret);
    }
    enable_irq_wake(chip->hall_ppe_s_irq);

    return ret;

fail_hall_ppe_s_irq_gpio:
    gpio_free(chip->hall_ppe_s_gpio);
fail_hall_ppe_n_irq_gpio:
    gpio_free(chip->hall_ppe_n_gpio);
fail_hall4_s_irq_gpio:
    gpio_free(chip->hall4_s_gpio);
fail_hall3_s_irq_gpio:
    gpio_free(chip->hall3_s_gpio);
fail_hall4_irq_gpio:
    gpio_free(chip->hall4_gpio);
fail_hall3_irq_gpio:
    gpio_free(chip->hall3_gpio);
fail_irq_gpio:
    gpio_free(chip->irq_gpio);

    return ret;
}

int mca_wireless_rev_set_user_reverse_chg(struct sc96231 *chip, bool user_reverse_chg)
{
	if (!chip)
		return -1;

	if (user_reverse_chg)
		chip->proc_data.user_reverse_chg = true;
	else
		chip->proc_data.user_reverse_chg = false;

	return 0;
}

static int nuvolta_1652_firmware_update_func(struct sc96231 *chip, u8 cmd)
  {
  	int ret = 0;
  	//TODO1 disable reverse charge if it run
  	switch (cmd) {
  	case FW_UPDATE_CHECK:
  		schedule_delayed_work(&chip->tx_firmware_update, msecs_to_jiffies(0));
  		break;
  	case FW_UPDATE_FORCE:
  		schedule_delayed_work(&chip->tx_firmware_update, msecs_to_jiffies(0));
  		break;
  	case FW_UPDATE_FROM_BIN:
  		// ret = nuvolta_1652_download_fw_bin(chip, false, true);
  		// if (ret < 0) {
  		// 	mca_log_info("[%s] fw download failed! cmd: %d\n", __func__, cmd);
  		// 	goto exit;
  		// }
  		break;
  	case FW_UPDATE_ERASE:
  		// nuvolta_1652_erase_fw(chip, 32768);
  		// if (ret < 0) {
  		// 	mca_log_info("[%s] fw download failed! cmd: %d\n", __func__, cmd);
  		// 	goto exit;
  		// }
  		// break;
  	case FW_UPDATE_AUTO:
  		// ret = nuvolta_1652_download_fw(chip, true, false);
  		// if (ret < 0) {
  		// 	mca_log_info("[%s] fw download failed! cmd: %d\n", __func__, cmd);
  		// 	goto exit;
  		// }
  		break;
  	default:
  		mca_log_info("unknown cmd: %d\n", cmd);
  		break;
  	}
  	return ret;
}

static void xm_wireless_rev_tx_firmware_update_work(struct work_struct *work)
{
    int ret = 0;
    u8 check_result = 0;
	struct sc96231 *chip = container_of(work, 
                    struct sc96231, tx_firmware_update.work);

    chip->fw_update = true;
    chip->proc_data.fw_updating = true;
  	//TODO2 sleep rx before start download and resume after
  	ret = mca_wireless_rev_charge_config(chip, true);
  	msleep(20);
	sc96231_download_fw(chip);
    if (ret < 0) {
  		mca_log_info("fw download failed!\n");
  		goto exit;
  	}
	sc96231_get_fw_version_check(&check_result, chip);
  	if (check_result == (RX_CHECK_SUCCESS | TX_CHECK_SUCCESS | BOOT_CHECK_SUCCESS)) {
  		if (ret < 0)
  			goto exit;
  		mca_log_info("download firmware success!\n");
  	} else {
  		ret = -1;
  		mca_log_info("download firmware failed!\n");
  	}
  exit:
  	//nuvolta_1652_set_reverse_gpio(chip, false);
  	mca_wireless_rev_charge_config(chip, false);
    chip->fw_update = false;
    chip->proc_data.fw_updating = false;
}

static void xm_wireless_rev_pen_data_handle_work(struct work_struct *work)
{
	struct sc96231  *info =  container_of(work,
		struct sc96231, pen_data_handle_work.work);
	int pen_soc = 255, tx_vout = 0, tx_iout = 0, tx_tdie = 0;
	int pen_full_flag = 0;
	static int full_soc_count = 0;
	u64 pen_mac = 0;
	int len;
	char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
	struct mca_event_notify_data event_data = { 0 };

	// get pen data
	sc96231_get_pen_soc(&pen_soc, info);
	sc96231_get_tx_vout(&tx_vout, info);
	sc96231_get_tx_iout(&tx_iout, info);
	sc96231_get_tx_tdie(&tx_tdie, info);
	sc96231_get_pen_full_flag(&pen_full_flag, info);
	sc96231_get_pen_mac(&pen_mac, info);

	len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
			"POWER_SUPPLY_PEN_PLACE_ERR=%d", 0);
	event_data.event = event;
	event_data.event_len = len;
	mca_event_report_uevent(&event_data);

	len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
			"POWER_SUPPLY_REVERSE_PEN_SOC=%d", pen_soc);
	event_data.event = event;
	event_data.event_len = len;
	mca_event_report_uevent(&event_data);

	len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
			"POWER_SUPPLY_PEN_MAC=%llx", pen_mac);
	event_data.event = event;
	event_data.event_len = len;
	mca_event_report_uevent(&event_data);

	if (pen_full_flag) {
		mca_log_info("pens battery is full, disable reverse chg\n");
		mca_wireless_rev_enable_reverse_charge(info, false);
	} else if ((pen_soc == 100) && (full_soc_count < PEN_SOC_FULL_COUNT)) {
		full_soc_count++;
		mca_log_info("pens soc is 100 count: %d\n", full_soc_count);
	} else {
		full_soc_count = 0;
	}

	if (full_soc_count == PEN_SOC_FULL_COUNT) {
		mca_log_info("pens soc is 100 exceed 18 ,disable reverse chg\n");
		full_soc_count = 0;
		pen_full_flag = 1;
		mca_wireless_rev_enable_reverse_charge(info, false);
	}

	if (pen_full_flag) {
		info->proc_data.reverse_chg_sts = REVERSE_STATE_ENDTRANS;
		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
			"POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);

		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
			"POWER_SUPPLY_REVERSE_PEN_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);
	}

	if (info->proc_data.reverse_chg_sts == REVERSE_STATE_TRANSFER) {
		schedule_delayed_work(&info->pen_data_handle_work, msecs_to_jiffies(REVERSE_PEN_DELAY_TIMER));
	}

	mca_log_info("loop pen data handle work\n");
	return;
}

static void xm_wireless_rev_pen_place_err_check_work(struct work_struct *work)
{
	struct sc96231  *info =  container_of(work,
		struct sc96231, pen_place_err_check_work.work);
    int ss = 0;
	int len;
	char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
	struct mca_event_notify_data event_data = { 0 };

	sc96231_get_tx_ss(&ss, info);
	if (!ss) {
		sc_log_info("pen place err check timeout, pen place err: hall\n");
		sc96231_set_pen_place_err(2, info);

		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE,
				"POWER_SUPPLY_PEN_PLACE_ERR=%d", 2);
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);
	}

	sc_log_info("reverse charge status: %d\n", info->proc_data.reverse_chg_sts);

	return;
}

static ssize_t chip_firmware_update_store(struct device *dev,
  		struct device_attribute *attr,
  		const char *buf,
  		size_t count)
  {
  	int cmd = 0, ret = 0;
    struct sc96231 *chip = dev_get_drvdata(dev);
  	if (chip->fw_update){
  		mca_log_info("Firmware Update is on going!\n");
  		return count;
  	}
  	cmd = (int)simple_strtoul(buf, NULL, 10);
  	mca_log_info("value %d\n", cmd);
  	if ((cmd > FW_UPDATE_NONE) && (cmd < FW_UPDATE_MAX)) {
  		ret = nuvolta_1652_firmware_update_func(chip, cmd);
  		if (ret < 0) {
  			mca_log_info("Firmware Update:failed!\n");
  			return count;
  		} else {
  			mca_log_info("Firmware Update:Success!\n");
  			return count;
  		}
  	} else {
  		mca_log_info("Firmware Update:invalid cmd\n");
  	}
  	return count;
}

static ssize_t chip_version_show(struct device *dev,
  		struct device_attribute *attr,
  		char *buf)
{
  	u8 check_result = 0;
    struct sc96231 *chip = dev_get_drvdata(dev);
  	if (chip->fw_update) {
  		mca_log_info("fw update going, can not show version\n");
  		return scnprintf(buf, PAGE_SIZE, "updating\n");
  	} else {
  		chip->fw_update = true;
  		mca_wireless_rev_charge_config(chip, true);
  		msleep(20);
  		sc96231_get_fw_version_check(&check_result, chip);
  		chip->fw_update = false;
  		mca_wireless_rev_charge_config(chip, false);
  		return scnprintf(buf, PAGE_SIZE, "%02x.%02x.%02x\n",
  				chip->wls_fw_data->fw_boot_id, chip->wls_fw_data->fw_rx_id, chip->wls_fw_data->fw_tx_id);
  	}
}

static ssize_t pen_hall3_online_show(struct device *dev,
  		struct device_attribute *attr,
  		char *buf)
{
    struct sc96231 *chip = dev_get_drvdata(dev);
    mca_log_info("%d\n", chip->hall3_online);
  	return scnprintf(buf, PAGE_SIZE, "%d\n", !chip->hall3_online);
}

static ssize_t pen_hall4_online_show(struct device *dev,
  		struct device_attribute *attr,
  		char *buf)
{
    struct sc96231 *chip = dev_get_drvdata(dev);
    mca_log_info("%d\n", chip->hall4_online);
  	return scnprintf(buf, PAGE_SIZE, "%d\n", !chip->hall4_online);
}

static ssize_t pen_soc_show(struct device *dev,
  		struct device_attribute *attr,
  		char *buf)
{
    struct sc96231 *chip = dev_get_drvdata(dev);
    int pen_soc = 0;
    sc96231_get_pen_soc(&pen_soc, chip);
    mca_log_info("%d\n", pen_soc);
  	return scnprintf(buf, PAGE_SIZE, "%d\n", pen_soc);
}

int pen_charge_state_notifier_register_client(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&pen_charge_state_notifier, nb);
}
EXPORT_SYMBOL(pen_charge_state_notifier_register_client);

int pen_charge_state_notifier_unregister_client(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&pen_charge_state_notifier, nb);
}
EXPORT_SYMBOL(pen_charge_state_notifier_unregister_client);

static ssize_t hall3_gpio_show(struct device *dev,
  		struct device_attribute *attr,
  		char *buf)
{
    struct sc96231 *chip = dev_get_drvdata(dev);
    int hall3_gpio_val = 1;
    if (gpio_is_valid(chip->hall3_gpio)) {
        hall3_gpio_val = gpio_get_value(chip->hall3_gpio);
        mca_log_info("hall3 gpio is %d \n", hall3_gpio_val);
    } else {
        mca_log_err("hall3 irq gpio not provided\n");
        return scnprintf(buf, PAGE_SIZE, "%d\n", hall3_gpio_val);
    }
  	return scnprintf(buf, PAGE_SIZE, "%d\n", hall3_gpio_val);
}

static ssize_t hall4_gpio_show(struct device *dev,
  		struct device_attribute *attr,
  		char *buf)
{
    struct sc96231 *chip = dev_get_drvdata(dev);
    int hall4_gpio_val = 1;
    if (gpio_is_valid(chip->hall4_gpio)) {
        hall4_gpio_val = gpio_get_value(chip->hall4_gpio);
        mca_log_info("hall3 gpio is %d \n", hall4_gpio_val);
    } else {
        mca_log_err("hall3 irq gpio not provided\n");
        return scnprintf(buf, PAGE_SIZE, "%d\n", hall4_gpio_val);
    }
  	return scnprintf(buf, PAGE_SIZE, "%d\n", hall4_gpio_val);
}

static ssize_t hall3_s_gpio_show(struct device *dev,
  		struct device_attribute *attr,
  		char *buf)
{
    struct sc96231 *chip = dev_get_drvdata(dev);
    int hall3_s_gpio_val = 1;
    if (gpio_is_valid(chip->hall3_s_gpio)) {
        hall3_s_gpio_val = gpio_get_value(chip->hall3_s_gpio);
        mca_log_info("hall3 gpio is %d \n", hall3_s_gpio_val);
    } else {
        mca_log_err("hall3 irq gpio not provided\n");
        return scnprintf(buf, PAGE_SIZE, "%d\n", hall3_s_gpio_val);
    }
  	return scnprintf(buf, PAGE_SIZE, "%d\n", hall3_s_gpio_val);
}

static ssize_t hall4_s_gpio_show(struct device *dev,
  		struct device_attribute *attr,
  		char *buf)
{
    struct sc96231 *chip = dev_get_drvdata(dev);
    int hall4_s_gpio_val = 1;
    if (gpio_is_valid(chip->hall4_s_gpio)) {
        hall4_s_gpio_val = gpio_get_value(chip->hall4_s_gpio);
        mca_log_info("hall3 gpio is %d \n", hall4_s_gpio_val);
    } else {
        mca_log_err("hall3 irq gpio not provided\n");
        return scnprintf(buf, PAGE_SIZE, "%d\n", hall4_s_gpio_val);
    }
  	return scnprintf(buf, PAGE_SIZE, "%d\n", hall4_s_gpio_val);
}

static ssize_t reverse_chg_mode_show(struct device *dev,
  		struct device_attribute *attr,
  		char *buf)
{
    struct sc96231 *chip = dev_get_drvdata(dev);

  	return scnprintf(buf, PAGE_SIZE, "%d\n", chip->proc_data.reverse_chg_en);
}

static ssize_t reverse_chg_mode_store(struct device *dev,
  		struct device_attribute *attr,
  		const char *buf,
  		size_t count)
  {
  	int val = 0, rc = 0;
    struct sc96231 *chip = dev_get_drvdata(dev);
  	if (kstrtoint(buf, 10, &val))
			return -EINVAL;
	mca_log_info("store reverse_chg_mode = %d\n", val);

	if (!val || chip->proc_data.reverse_chg_sts != REVERSE_STATE_TRANSFER) {
		// enabling reverse charge in REVERSE_STATE_TRANSFER status will cause the transfer to
		// terminate, this is usually not expected behavior, so only enable it when we are not
		// in that status
		rc = mca_wireless_rev_enable_reverse_charge(chip, !!val);
        if (rc < 0){
            return rc;
        }
        if (!val)
		    chip->proc_data.reverse_chg_sts = REVERSE_STATE_ENDTRANS;
	} else {
		mca_log_info("store reverse_chg_mode no operation\n");
	}

	rc = mca_wireless_rev_set_user_reverse_chg(chip, !!val);
	if (rc < 0)
		return rc;
  	return count;
}

static ssize_t reverse_chg_state_show(struct device *dev,
  		struct device_attribute *attr,
  		char *buf)
{
    struct sc96231 *chip = dev_get_drvdata(dev);
    mca_log_info("reverse_chg_state_show = %d\n", chip->proc_data.reverse_chg_sts);
  	return scnprintf(buf, PAGE_SIZE, "%d\n", chip->proc_data.reverse_chg_sts);
}

static ssize_t tx_tdie_show(struct device *dev,
  		struct device_attribute *attr,
  		char *buf)
{
    struct sc96231 *chip = dev_get_drvdata(dev);
    int tx_tdie = 0;
    sc96231_get_tx_tdie(&tx_tdie, chip);
    mca_log_info("%d\n", tx_tdie);
  	return scnprintf(buf, PAGE_SIZE, "%d\n", tx_tdie);
}

static ssize_t tx_vout_show(struct device *dev,
  		struct device_attribute *attr,
  		char *buf)
{
    struct sc96231 *chip = dev_get_drvdata(dev);
    int tx_vout = 0;
    sc96231_get_tx_vout(&tx_vout, chip);
    mca_log_info("%d\n", tx_vout);
  	return scnprintf(buf, PAGE_SIZE, "%d\n", tx_vout);
}

static ssize_t tx_iout_show(struct device *dev,
  		struct device_attribute *attr,
  		char *buf)
{
    struct sc96231 *chip = dev_get_drvdata(dev);
    int tx_iout = 0;
    sc96231_get_tx_iout(&tx_iout, chip);
    mca_log_info("%d\n", tx_iout);
  	return scnprintf(buf, PAGE_SIZE, "%d\n", tx_iout);
}

static DEVICE_ATTR(chip_firmware_update, S_IWUSR, NULL, chip_firmware_update_store);
static DEVICE_ATTR(chip_version, S_IRUGO, chip_version_show, NULL);
static DEVICE_ATTR(pen_hall3_online, S_IRUGO, pen_hall3_online_show, NULL);
static DEVICE_ATTR(pen_hall4_online, S_IRUGO, pen_hall4_online_show, NULL);
static DEVICE_ATTR(pen_soc, S_IRUGO, pen_soc_show, NULL);
static DEVICE_ATTR(hall3_gpio, S_IRUGO, hall3_gpio_show, NULL);
static DEVICE_ATTR(hall4_gpio, S_IRUGO, hall4_gpio_show, NULL);
static DEVICE_ATTR(hall3_s_gpio, S_IRUGO, hall3_s_gpio_show, NULL);
static DEVICE_ATTR(hall4_s_gpio, S_IRUGO, hall4_s_gpio_show, NULL);
static DEVICE_ATTR(reverse_chg_mode, S_IWUSR | S_IRUGO, reverse_chg_mode_show, reverse_chg_mode_store);
static DEVICE_ATTR(reverse_chg_state, S_IRUGO, reverse_chg_state_show, NULL);
static DEVICE_ATTR(tx_tdie, S_IRUGO, tx_tdie_show, NULL);
static DEVICE_ATTR(tx_vout, S_IRUGO, tx_vout_show, NULL);
static DEVICE_ATTR(tx_iout, S_IRUGO, tx_iout_show, NULL);

static struct attribute *sc96231_sysfs_attrs[] = {
    &dev_attr_chip_firmware_update.attr,
    &dev_attr_chip_version.attr,
    &dev_attr_pen_hall3_online.attr,
    &dev_attr_pen_hall4_online.attr,
    &dev_attr_pen_soc.attr,
    &dev_attr_hall3_gpio.attr,
    &dev_attr_hall4_gpio.attr,
    &dev_attr_hall3_s_gpio.attr,
    &dev_attr_hall4_s_gpio.attr,
    &dev_attr_reverse_chg_mode.attr,
    &dev_attr_reverse_chg_state.attr,
    &dev_attr_tx_tdie.attr,
    &dev_attr_tx_vout.attr,
    &dev_attr_tx_iout.attr,
    NULL,
};

static const struct attribute_group sc96231_sysfs_group_attrs = {
    .attrs = sc96231_sysfs_attrs,
};

static int sc96231_charger_probe(struct i2c_client *client)
{
    int ret = 0;
    struct sc96231 *chip;

    mca_log_info("sc96231 %s probe start!\n", SC96231_DRV_VERSION);

    chip = devm_kzalloc(&client->dev, sizeof(struct sc96231), GFP_KERNEL);
    if (!chip)
        return -ENOMEM;

    chip->wls_fw_data = devm_kzalloc(&client->dev, sizeof(struct wls_fw_parameters), GFP_KERNEL);
    if (!chip->wls_fw_data)
        return -ENOMEM;

    chip->regmap = devm_regmap_init_i2c(client, &sc96231_regmap_config);
    if (IS_ERR(chip->regmap)) {
        mca_log_err("failed to allocate register map\n");
        return PTR_ERR(chip->regmap);
    }

    chip->client = client;
    chip->dev = &client->dev;
    chip->support_tx_only = true;
    chip->proc_data.user_reverse_chg = true;
    device_init_wakeup(&client->dev, true);
    i2c_set_clientdata(client, chip);

    mutex_init(&chip->i2c_rw_lock);
    mutex_init(&chip->wireless_chg_int_lock);

    sc96231_parse_dt(chip);
    sc96231_irq_init(chip);

    if (!gpio_is_valid(chip->reverse_boost_gpio)){
		mca_log_err("failed to parse reverse_boost_gpio\n");
	}else{
		gpio_direction_output(chip->reverse_boost_gpio, 0);
	}
    if (!gpio_is_valid(chip->reverse_txon_gpio)){
		mca_log_err("failed to parse reverse_txon_gpio\n");
	}else{
		gpio_direction_output(chip->reverse_txon_gpio, 0);
	}

    INIT_DELAYED_WORK(&chip->interrupt_work, sc96231_interrupt_work);
    INIT_DELAYED_WORK(&chip->hall_irq_work, sc96231_hall_interrupt_work);
    INIT_DELAYED_WORK(&chip->ppe_hall_irq_work, sc96231_ppe_hall_interrupt_work);
    //XM add
    INIT_DELAYED_WORK(&chip->reverse_charge_config_work, xm_wireless_rev_charge_config_work);
    INIT_DELAYED_WORK(&chip->tx_ping_timeout_work, xm_wireless_rev_tx_ping_timeout_work);
	INIT_DELAYED_WORK(&chip->tx_transfer_timeout_work, xm_wireless_rev_tx_transfer_timeout_work);
    INIT_DELAYED_WORK(&chip->tx_firmware_update, xm_wireless_rev_tx_firmware_update_work);
    INIT_DELAYED_WORK(&chip->pen_data_handle_work, xm_wireless_rev_pen_data_handle_work);
    INIT_DELAYED_WORK(&chip->pen_place_err_check_work, xm_wireless_rev_pen_place_err_check_work);

	chip->chg_dev = wireless_charger_device_register("sc96231_tx", chip->dev, chip, &sc96231_wls_ops, &sc96231_chg_props);
	if (chip->chg_dev == NULL) {
		mca_log_err("register ops fail\n");
		goto err_dev;
    }

    mca_log_err("check pen attachment after 14s\n");
    schedule_delayed_work(&chip->hall_irq_work, msecs_to_jiffies(14000));
    schedule_delayed_work(&chip->ppe_hall_irq_work, msecs_to_jiffies(0));

    // device_create_file(chip->dev, &dev_attr_test);
    ret = sysfs_create_group(&chip->dev->kobj, &sc96231_sysfs_group_attrs);
    if (ret < 0)
    {
        mca_log_info("sysfs_create_group fail %d\n", ret);
        goto error_sysfs;
    }
    mca_log_err("sc96231 probe success!\n");
    return ret;

error_sysfs:
    sysfs_remove_group(&chip->dev->kobj, &sc96231_sysfs_group_attrs);
err_dev:
    mca_log_err("sc96231 probe failed!\n");
    if(!IS_ERR_OR_NULL(chip->chg_dev))
        wireless_charger_device_unregister(chip->chg_dev);

    return 0;
}

static int sc96231_suspend(struct device *dev)
{
    struct sc96231 *chip = dev_get_drvdata(dev);

    mca_log_info("Suspend successfully!");
    if (device_may_wakeup(dev))
        enable_irq_wake(chip->irq);

    return 0;
}

static int sc96231_resume(struct device *dev)
{
    struct sc96231 *chip = dev_get_drvdata(dev);

    mca_log_info("Resume successfully!");
    if (device_may_wakeup(dev))
        disable_irq_wake(chip->irq);

    return 0;
}

static const struct dev_pm_ops sc96231_pm_ops = {
    .resume     = sc96231_resume,
    .suspend    = sc96231_suspend,
};

static void sc96231_charger_remove(struct i2c_client *client)
{
    struct sc96231 *chip = i2c_get_clientdata(client);

    mutex_destroy(&chip->i2c_rw_lock);
    mutex_destroy(&chip->wireless_chg_int_lock);

    if (chip->irq_gpio > 0)
        gpio_free(chip->irq_gpio);
    return;
}

static void sc96231_charger_shutdown(struct i2c_client *client)
{
    return;
}

static struct of_device_id sc96231_charger_match_table[] = {
    {
        .compatible = "sc,sc96231-wireless-charger",
        .data = NULL,
    },
    {},
};
MODULE_DEVICE_TABLE(of, sc96231_charger_match_table);

static const struct i2c_device_id sc96231_id[] = {
    {"sc96231", 0},
    {},
};

static struct i2c_driver sc96231_wireless_charger_driver = {
    .driver     = {
        .name   = "sc-wireless-charger",
        .owner  = THIS_MODULE,
        .of_match_table = of_match_ptr(sc96231_charger_match_table),
        .pm     = &sc96231_pm_ops,
    },
    .probe      = sc96231_charger_probe,
    .remove     = sc96231_charger_remove,
    .shutdown   = sc96231_charger_shutdown,
    .id_table	= sc96231_id,
};

module_i2c_driver(sc96231_wireless_charger_driver);

MODULE_DESCRIPTION("SC SC96231 Wireless Charge Driver");
MODULE_AUTHOR("Aiden-yu@southchip.com");
MODULE_LICENSE("GPL v2");
