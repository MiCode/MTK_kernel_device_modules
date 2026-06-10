
/* Copyright (c) 2025 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/* this driver is compatible for nuvolta wireless charge ic */
#include <linux/i2c.h>
#include <linux/alarmtimer.h>
#include <linux/ktime.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/regulator/consumer.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include "charger_class.h"
#include "mtk_charger.h"
#include "nuvolta_1671.h"
#include "nuvolta_1671_fw.h"
#include "bq28z610.h"
#include <linux/mca/common/mca_log.h>
#include <linux/mca/common/mca_event.h>
#include <linux/mca/common/mca_charge_mievent.h>

#ifndef MCA_LOG_TAG
#define MCA_LOG_TAG "mca_wls_nu1671"
#endif

static struct nuvolta_1671_chg *g_chip;
static int nuvolta_1671_set_enable_mode(struct nuvolta_1671_chg *chip, bool enable);
static int nu1671_enable_reverse_boost(struct nuvolta_1671_chg *chip, int purpose, int enable);
static int nuvolta_1671_firmware_update_func(struct nuvolta_1671_chg *chip, u8 cmd);

static struct regmap_config nuvolta_1671_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = 0xFFFF,
};
static struct  wls_bin_parameters g_msg;

static int rx1671_read(struct nuvolta_1671_chg *chip, u8 *val, u16 addr)
{
	int i, ret = 0;
	unsigned int temp;

	mutex_lock(&chip->i2c_lock);
	if (chip->shutdown_flag) {
		mca_log_err("failed-read, device is shutdown\n");
		goto out;
	}
	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = regmap_read(chip->regmap, addr, &temp);
		if (IS_ERR_VALUE((unsigned long)ret)) {
			mca_log_err("failed-read, reg(0x%02X), ret(%d)\n", addr, ret);
		} else {
			*val = (u8)temp;
			break;
		}
	}

out:
	mutex_unlock(&chip->i2c_lock);
	return ret;
}

static int rx1671_read_buffer(struct nuvolta_1671_chg *chip, u8 *buf, u16 addr, int size)
{
	int ret = 0;

	mutex_lock(&chip->i2c_lock);
	if (chip->shutdown_flag) {
		mca_log_err("failed-bulk-read, device is shutdown\n");
		goto out;
	}
	ret = regmap_bulk_read(chip->regmap, addr, buf, size);

out:
	mutex_unlock(&chip->i2c_lock);
	if (IS_ERR_VALUE((unsigned long)ret))
		mca_log_err("failed-bulk-read, reg(0x%02X), size(%d), ret(%d)\n", addr, size, ret);

	return ret;
}

static int rx1671_write(struct nuvolta_1671_chg *chip, u8 val, u16 addr)
{
	int i, ret = 0;

	mutex_lock(&chip->i2c_lock);
	if (chip->shutdown_flag) {
		mca_log_err("failed-write, device is shutdown\n");
		goto out;
	}
	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = regmap_write(chip->regmap, addr, val);
		if (IS_ERR_VALUE((unsigned long)ret))
			mca_log_err("failed-write, reg(0x%02X), ret(%d)\n", addr, ret);
		else
			break;
	}

out:
	mutex_unlock(&chip->i2c_lock);
	return ret;
}

static int rx1671_write_buffer(struct nuvolta_1671_chg *chip, u8 *buf, u16 addr, int size)
{
	int ret = 0;

	mutex_lock(&chip->i2c_lock);
	if (chip->shutdown_flag) {
		mca_log_err("failed-bulk-write, device is shutdown\n");
		goto out;
	}
	ret = regmap_bulk_write(chip->regmap, addr, buf, size);

out:
	mutex_unlock(&chip->i2c_lock);
	if (IS_ERR_VALUE((unsigned long)ret))
		mca_log_err("failed-bulk-write, reg(0x%02X), size(%d), ret(%d)\n", addr, size, ret);

	return ret;
}

static int nuvolta_1671_start_tx_function(struct nuvolta_1671_chg *chip, bool enable)
{
	int ret = 0;

	if (enable)
		ret = rx1671_write(chip, RX_CMD_ENABLE_TX, REG_RX_INT_3);
	else
		ret = rx1671_write(chip, RX_CMD_DISABLE_TX, REG_RX_INT_3);

	mca_log_err("enable_reverse_chg: %d\n", enable);
	return ret;
}

static int nuvolta_1671_check_i2c(struct nuvolta_1671_chg *chip)
{
	int ret = 0;
	u8 data = 0;
	if(chip == NULL)
		return -EINVAL;
	mutex_lock(&chip->data_transfer_lock);
	ret = rx1671_write(chip, RX_CMD_I2C_CHECK_MASK, I2C_CMD_CHECK_ADDR);
	if (ret < 0)
		goto exit;

	msleep(20);

	ret = rx1671_read(chip, &data, I2C_CMD_CHECK_ADDR);
	if (ret < 0)
		goto exit;

	if (data == RX_CMD_I2C_CHECK_MASK)
		mca_log_err("i2c check ok!\n");
	else {
		mca_log_err("i2c check failed!\n");
		ret = -1;
	}

exit:
	mutex_unlock(&chip->data_transfer_lock);
	return ret;
}

static int nu1671_set_reverse_pmic_boost(struct nuvolta_1671_chg *chip, int purpose, int enable)
{
	 struct regulator *vbus = chip->pmic_boost;
    int ret = 0;
    int otg_enable = 0, vbus_vol_now = 0;
    int retry_cnt = 0;
    int voltage = 5000, target_voltage = 5000;

    mca_log_err("purpose=%d, en=%d\n", purpose, enable);

    if (!chip->master_cp_dev)
        chip->master_cp_dev = get_charger_by_name("cp_master");
    if (!chip->master_cp_dev) {
        mca_log_err("%s failed to get master_cp_dev\n", __func__);
    }
	if(vbus == NULL) {
		mca_log_err("pmic boost is NULL\n");
		return -EINVAL;
	}

    usb_get_property(USB_PROP_OTG_ENABLE, &otg_enable);

    if (enable) {
        target_voltage = (purpose == BOOST_FOR_FWUPDATE) ? chip->fwupdate_boost_vol :
            ((purpose == BOOST_FOR_REVCHG) ? chip->revchg_boost_vol : target_voltage);
        usb_set_property(USB_PROP_INPUT_SUSPEND, 1);
        charger_dev_enable_acdrv_manual(chip->master_cp_dev, true);
        charger_dev_enable_cp_usb_gate(chip->master_cp_dev, false);
        msleep(100);
        ret = regulator_set_voltage(vbus, voltage*1000, INT_MAX);
        ret |= regulator_set_current_limit(vbus, 1800000, 1800000);
        ret |= regulator_enable(vbus);
        if (ret) {
            mca_log_err("vbus regulator failed\n");
            charger_dev_enable_cp_usb_gate(chip->master_cp_dev, true);
            if (!otg_enable)
                charger_dev_enable_acdrv_manual(chip->master_cp_dev, false);
            return ret;
        }

        while (voltage < target_voltage) {
            voltage += 500;
            msleep(20);
            if (chip->power_good_flag)
                break;
            if (voltage < target_voltage) {
                mca_log_err("vbus regulator set to %dmV\n", voltage);
                ret = regulator_set_voltage(vbus, voltage*1000, INT_MAX);
            } else {
                mca_log_err("vbus regulator set to %dmV\n", target_voltage);
                ret = regulator_set_voltage(vbus, target_voltage*1000, INT_MAX);
            }
        }

        if (chip->power_good_flag)
            return -1;

        msleep(20);
        while (retry_cnt < 3) {
            ret = nuvolta_1671_check_i2c(chip);
            if (ret >= 0) {
                ret = 0;
                break;
            }
            msleep(20);
            mca_log_err("trx i2c check fail!");
            ++retry_cnt;
        }
        if (retry_cnt < 3) {
            mca_log_err("vbus regulator enable\n");
        } else {
            mca_log_err("vbus regulator enable fail\n");
            mca_charge_mievent_report(CHARGE_DFX_WLS_TRX_IIC_ERR, NULL, 0);
        }
    } else {
        regulator_set_voltage(vbus, 5050*1000, INT_MAX);
        msleep(20);
        ret = regulator_disable(vbus);
        if (ret)
            mca_log_err("vbus regulator disable fail, ret=%d\n", ret);
        msleep(20);
        if (!otg_enable)
            usb_set_property(USB_PROP_INPUT_SUSPEND, 0);
        usb_get_property(USB_PROP_PMIC_VBUS, &vbus_vol_now);
        mca_log_err("vbus regulator disable, vbus=%dmV, state=%d\n", vbus_vol_now, regulator_is_enabled(vbus));
        msleep(100);

        if (!chip->fw_version_reflash) {
            charger_dev_enable_cp_usb_gate(chip->master_cp_dev, true);
            if (!otg_enable)
                charger_dev_enable_acdrv_manual(chip->master_cp_dev, false);
        }
    }
    return ret;
}

static int nuvolta_1671_set_reverse_chg_mode(struct nuvolta_1671_chg *chip, int enable)
{
	int otg_enable = 0, vusb_insert = 0;
    char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
    struct mca_event_notify_data event_data = { 0 };
    int len;

    if (chip->fw_update) {
        chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
        mca_log_err("fw updating, don't enable reverse charge\n");
        goto exit_to_report_state;
    }

    if (enable) {
        chip->is_reverse_chg = REVERSE_STATE_OPEN;
        len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_REVERSE_CHG_MODE=%d", REVERSE_CHARGE_OPEN);
        event_data.event = event;
        event_data.event_len = len;
        mca_event_report_uevent(&event_data);
        len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_OPEN);
        event_data.event = event;
        event_data.event_len = len;
        mca_event_report_uevent(&event_data);

        if (chip->power_good_flag) {
            chip->is_reverse_chg = REVERSE_STATE_FORWARD;
            mca_log_err("wireless charging, don't enable reverse charge\n");
            goto exit_to_report_state;
        }
        usb_get_property(USB_PROP_OTG_ENABLE, &otg_enable);
        wls_get_property(WLS_PROP_VUSB_INSERT, &vusb_insert);
        if (otg_enable || vusb_insert) {
            chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
            mca_log_err("wirechg or otg online, don't enable reverse charge\n");
            goto exit_to_report_state;
        }

        mca_log_err("start reverse charge\n");
        pm_stay_awake(chip->dev);
        chip->reverse_chg_en = true;
        schedule_delayed_work(&chip->reverse_chg_config_work, msecs_to_jiffies(500));
        schedule_delayed_work(&chip->reverse_chg_monitor_work, msecs_to_jiffies(700));
        schedule_delayed_work(&chip->reverse_ping_timeout_work, msecs_to_jiffies(REVERSE_PING_TIMEOUT_TIMER + 300));

    } else {
        if (!chip->is_reverse_closing && !chip->is_reverse_boosting) {
            mca_log_err("close reverse charge\n");
            chip->is_reverse_closing = true;
            chip->reverse_chg_en = false;
            nuvolta_1671_start_tx_function(chip, false);
            nu1671_enable_reverse_boost(chip, BOOST_FOR_REVCHG, false);
            msleep(100);
            len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_REVERSE_CHG_MODE=%d", REVERSE_CHARGE_CLOSE);
            event_data.event = event;
            event_data.event_len = len;
            mca_event_report_uevent(&event_data);
            mca_charge_mievent_set_state(MIEVENT_STATE_PLUG, 0);
            cancel_delayed_work(&chip->reverse_chg_monitor_work);
            if (!chip->tx_timeout_flag) {
                cancel_delayed_work(&chip->reverse_transfer_timeout_work);
                cancel_delayed_work(&chip->reverse_ping_timeout_work);
            }
            if (!chip->reverse_chg_en)
                pm_relax(chip->dev);
            chip->is_reverse_closing = false;
        } else
            mca_log_err("close reverse charge failed, closing:%d, boosting:%d\n", chip->is_reverse_closing, chip->is_reverse_boosting);
    }

    return 0;

exit_to_report_state:
    if (!chip->reverse_chg_en) {
        msleep(1000);
        len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_REVERSE_CHG_MODE=%d", REVERSE_CHARGE_CLOSE);
        event_data.event = event;
        event_data.event_len = len;
        mca_event_report_uevent(&event_data);
        len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_REVERSE_CHG_STATE=%d", chip->is_reverse_chg);
        event_data.event = event;
        event_data.event_len = len;
        mca_event_report_uevent(&event_data);
    }
    return -1;
}

static void nuvolta_1671_set_fod(struct nuvolta_1671_chg *chip, struct fod_params_t *params_base)
{
	u8 params_offset;
	u8 count = 0;
	int buffer_length;
	struct params_t params_buffer[FOD_PARAMS_MAX_LENGTH];
	int i = 0;
	int ret = 0;
	struct params_t *params_ptr = params_base->params;

	mutex_lock(&chip->data_transfer_lock);

	for (i = 0; i < params_base->length; ++i)
		mca_log_err("params[%d]: %d, %d\n", i, params_ptr[i].gain, params_ptr[i].offset);

	ret = rx1671_write(chip, RX_CMD_FOD_SET, REG_RX_SENT_CMD); //cmd 0x98
	if (ret < 0)
		goto exit;

	mca_log_err("set fod type: %d\n", params_base->type);
	ret = rx1671_write(chip, params_base->type, REG_RX_SENT_DATA2); //params type
	if (ret < 0)
		goto exit;

	params_offset = 0;
	while (params_offset < params_base->length) {
		count = params_offset + 5 <= params_base->length ? 5 : params_base->length - params_offset;
		buffer_length = sizeof(struct params_t) * count;

		ret = rx1671_write(chip, buffer_length + 3, REG_RX_SENT_DATA1); // cmd length
		if (ret < 0)
			goto exit;

		ret = rx1671_write(chip, params_offset, REG_RX_SENT_DATA3); // params offset
		if (ret < 0)
			goto exit;

		ret = rx1671_write(chip, buffer_length, REG_RX_SENT_DATA4); // buffer_length
		if (ret < 0)
			goto exit;

		memcpy((void *)params_buffer, (const void *)&params_ptr[params_offset], buffer_length);

		ret = rx1671_write_buffer(chip, (u8 *)params_buffer, REG_RX_SENT_DATA5, buffer_length); //params length:max 10 one time
		if (ret < 0)
			goto exit;

		ret = rx1671_write(chip, buffer_length + 5, REG_RX_INT_0); // triger int to rx
		if (ret < 0)
			goto exit;

		msleep(20); // wait rx save fod params
		params_offset += count;
	}

exit:
	mutex_unlock(&chip->data_transfer_lock);
}

static void nuvolta_1671_set_bpp_plus_fod(struct nuvolta_1671_chg *chip, struct fod_params_t *params_base)
{
	u8 params_offset;
	int buffer_length;
	struct params_t params_buffer[FOD_PARAMS_MAX_LENGTH];
	int ret = 0;
	int i = 0;

	for (i = 0; i < params_base->length; ++i)
		mca_log_err("params[%d]: %d, %d\n", i, params_base->params[i].gain, params_base->params[i].offset);

	mutex_lock(&chip->data_transfer_lock);
	params_offset = 0;
	buffer_length = sizeof(struct params_t) * FOD_PARAMS_MAX_LENGTH;

	ret = rx1671_write(chip, RX_CMD_FOD_SET, REG_RX_SENT_CMD); //cmd 0x98
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, BPP_PLUS_FOD_SET_CMD_LENGTH, REG_RX_SENT_DATA1); //cmd length:13
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, params_base->type, REG_RX_SENT_DATA2); //params type
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, params_offset, REG_RX_SENT_DATA3); //params offset:0
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, buffer_length, REG_RX_SENT_DATA4); //params length:max 10 one time
	if (ret < 0)
		goto exit;

	memcpy((void *)params_buffer, (const void *)&params_base[params_offset], buffer_length);

	ret = rx1671_write_buffer(chip, (u8 *)params_buffer, REG_RX_SENT_DATA5, buffer_length); //params length:max 10 one time
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, FOD_SET_TRIGGER_RX, REG_RX_INT_0); //triger int to rx
	if (ret < 0)
		goto exit;

	mca_log_err("set bpp plus fod type: %d\n", params_base->type);
	msleep(20); //wait rx save for params

exit:
	mutex_unlock(&chip->data_transfer_lock);
}

static void nuvolta_1671_set_fod_params(struct nuvolta_1671_chg *chip)
{
	int uuid = 0, i = 0;
	bool found = true;

	uuid |= chip->uuid[0] << 24;
    uuid |= chip->uuid[1] << 16;
    uuid |= chip->uuid[2] << 8;
    uuid |= chip->uuid[3];
    mca_log_info("uuid: 0x%x,0x%x,0x%x,0x%x\n", chip->uuid[0], chip->uuid[1], chip->uuid[2], chip->uuid[3]);

	for (i = 0; i < chip->fod_params_size_2_1; i++) {
			found = true;
			if (uuid != chip->fod_params_2_1[i].uuid) {
			found = false;
			continue;
		}
		/* found fod by uuid */
		if (found) {
			mca_log_info("found 2_1 fod params uuid: 0x%x,0x%x,0x%x,0x%x\n", chip->uuid[0],
				chip->uuid[1], chip->uuid[2], chip->uuid[3]);
			nuvolta_1671_set_fod(chip, &chip->fod_params_2_1[i]);
		}
	}
	i = 0;
	found = true;
	for (i = 0; i < chip->fod_params_size; i++) {
			found = true;
			if (uuid != chip->fod_params[i].uuid) {
			found = false;
			continue;
		}
		/* found fod by uuid */
		if (found) {
			mca_log_info("found fod params uuid: 0x%x,0x%x,0x%x,0x%x\n", chip->uuid[0],
				chip->uuid[1], chip->uuid[2], chip->uuid[3]);
			nuvolta_1671_set_fod(chip, &chip->fod_params[i]);
			return;
		}
	}
	if (((chip->adapter_type == ADAPTER_QC3) || (chip->adapter_type == ADAPTER_PD))
		&& (!chip->epp)) {
		found = true;
		mca_log_info("bpp plus\n");
		nuvolta_1671_set_bpp_plus_fod(chip, &chip->fod_params_bpp_plus);
	} else if (chip->adapter_type >= ADAPTER_XIAOMI_QC3) {
		found = true;
		mca_log_info("epp+ default\n");
		nuvolta_1671_set_fod(chip, &chip->fod_params_default);
	}
	if (!found)
		mca_log_info("can not found fod params, uuid: 0x%x,0x%x,0x%x,0x%x\n",
			chip->uuid[0], chip->uuid[1], chip->uuid[2], chip->uuid[3]);
	return;
}

static int nuvolta_1671_set_adapter_voltage(struct nuvolta_1671_chg *chip, int voltage)
{
	int ret = 0;
	u8 vol_h = 0, vol_l = 0;

	mutex_lock(&chip->data_transfer_lock);

	if ((voltage < ADAPTER_VOL_MIN_MV) || (voltage > ADAPTER_VOL_MAX_MV))
		voltage = ADAPTER_VOL_DEFAULT_MV;

	vol_h = (u8)(voltage >> 8);
	vol_l = (u8)(voltage & 0xFF);

	ret = rx1671_write(chip, RX_CMD_TRANSMIT_PACKET, REG_RX_SENT_CMD);
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, ADAPTER_VOL_PACKET_LENGTH, REG_RX_SENT_DATA1);
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, ADAPTER_VOL_SET_TYPE, REG_RX_SENT_DATA2);
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, TRANS_DATA_LENGTH_3BYTE, REG_RX_SENT_DATA3);
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, RX_CMD_FASTCHG_SET_TX_VOLTAGE, REG_RX_SENT_DATA4);
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, vol_l, REG_RX_SENT_DATA5);
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, vol_h, REG_RX_SENT_DATA6);
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, ADAPTER_VOL_TRIGGER_RX, REG_RX_INT_0);
	if (ret < 0)
		goto exit;

	mca_log_err("adapter voltage setted: %d\n", voltage);

exit:
	mutex_unlock(&chip->data_transfer_lock);
	return ret;
}

static int nuvolta_1671_get_cep_value(struct nuvolta_1671_chg * chip, u8 *cep)
{
	int ret = 0;
	u8 read_buf[128];

	ret = rx1671_read(chip, &read_buf[0], (0x2320+0x10));
	if (ret < 0)
		return ret;

	*cep = read_buf[0];
	mca_log_err("cep: %d\n", *cep);

	return ret;
}

static int nuvolta_1671_set_vout(struct nuvolta_1671_chg * chip, int vout)
{
	int ret = 0;
	u8 vout_h, vout_l;
	u8 cep;
	int max_vol = VOUT_SET_MAX_MV;
	if (!chip->power_good_flag) {
		mca_log_info("power good disonline, don't set vout\n");
		return -1;
	}
	mutex_lock(&chip->data_transfer_lock);
	msleep(20);

	if (chip->parallel_charge == true) {
		ret = nuvolta_1671_get_cep_value(chip, &cep);
		if (ABS(cep) > ABS_CEP_VALUE) {
			mca_log_err("vol:%d ,cep %d, not set\n", vout, cep);
			goto exit;
		}
	}

	if (vout < VOUT_SET_MIN_MV)
		vout = VOUT_SET_DEFAULT_MV;
	else if (vout > max_vol)
		vout = max_vol;

	vout_h = (u8)(vout >> 8);
	vout_l = (u8)(vout & 0xFF);

	ret = rx1671_write(chip, RX_CMD_SET_RX_VOUT, REG_RX_SENT_CMD);
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, VOUT_SET_PACKET_LENGTH, REG_RX_SENT_DATA1);
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, vout_l, REG_RX_SENT_DATA2);
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, vout_h, REG_RX_SENT_DATA3);
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, VOUT_SET_TRIGGER_RX, REG_RX_INT_0);
	if (ret < 0)
		goto exit;

	chip->vout_setted = vout;
	mca_log_err("set rx vout: %d cep=%d parallel_charge=%d\n", vout, cep, chip->parallel_charge);

exit:
	mutex_unlock(&chip->data_transfer_lock);
	return ret;
}

static int nuvolta_1671_get_vrect(struct nuvolta_1671_chg * chip, int *vrect)
{
	int ret = 0;
	u8 read_buf[128];
	if (!chip->power_good_flag) {
		*vrect = 0;
		return ret;
	}
	ret = rx1671_read(chip, &read_buf[0], (0x2320+0x38));
	if (ret < 0)
		return ret;

	ret = rx1671_read(chip, &read_buf[1], (0x2320+0x39));
	if (ret < 0)
		return ret;

	*vrect = read_buf[1] * 256 + read_buf[0];

	mca_log_err("wls_get_vrect: %d\n", *vrect);

	return ret;
}

static int nuvolta_1671_get_vout(struct nuvolta_1671_chg * chip, int *vout)
{
	int ret = 0;
	u8 read_buf[128];
	u8 cep = 0;
	if (!chip->power_good_flag) {
		*vout = 0;
		return ret;
	}
	ret = rx1671_read(chip, &read_buf[0], (0x2320+0x34));
	if (ret < 0)
		return ret;

	ret = rx1671_read(chip, &read_buf[1], (0x2320+0x35));
	if (ret < 0)
		return ret;

	*vout = read_buf[1] * 256 + read_buf[0];

	ret = rx1671_read(chip, &read_buf[0], (0x2320+0x10));
	if (ret < 0)
		return ret;

	cep = read_buf[0];

	mca_log_err("wls_get_vout: %d, cep = %d\n", *vout, cep);

	return ret;
}

static int nuvolta_1671_get_iout(struct nuvolta_1671_chg * chip, int *iout)
{
	int ret = 0;
	u8 read_buf[128];
	if (!chip->power_good_flag) {
		*iout = 0;
		return ret;
	}
	ret = rx1671_read(chip, &read_buf[0], (0x2320+0x36));
	if (ret < 0)
		return ret;

	ret = rx1671_read(chip, &read_buf[1], (0x2320+0x37));
	if (ret < 0)
		return ret;

	*iout = read_buf[1] * 256 + read_buf[0];

	mca_log_err("wls_get_iout: %d\n", *iout);

	return ret;
}

static int nuvolta_1671_get_temp(struct nuvolta_1671_chg * chip, int *temp)
{
	int ret = 0;
	u8 read_buf[128];
	if (!chip->power_good_flag) {
		*temp = 0;
		return ret;
	}
	ret = rx1671_read(chip, &read_buf[0], (0x2320+0x3e));
	if (ret < 0)
		return ret;

	ret = rx1671_read(chip, &read_buf[1], (0x2320+0x3f));
	if (ret < 0)
		return ret;

	*temp = read_buf[1] * 256 + read_buf[0];

	mca_log_err("wls_get_rx_temp: %d\n", *temp);

	return ret;
}

static int nu1671_get_trx_iout(struct nuvolta_1671_chg * chip, int *iout)
{
   	int ret = 0;
	u8 read_buf[128];

	ret = rx1671_read(chip, &read_buf[0], (0x2320+0x36));
	if (ret < 0)
		return ret;

	ret = rx1671_read(chip, &read_buf[1], (0x2320+0x37));
	if (ret < 0)
		return ret;

	*iout = read_buf[1] * 256 + read_buf[0];

	mca_log_info("trx isense: %d\n", *iout);

	return ret;
}

static int nu1671_get_trx_vrect(struct nuvolta_1671_chg * chip, int *vrect)
{
	int ret = 0;
	u8 read_buf[128];

	ret = rx1671_read(chip, &read_buf[0], (0x2320+0x38));
	if (ret < 0)
		return ret;

	ret = rx1671_read(chip, &read_buf[1], (0x2320+0x39));
	if (ret < 0)
		return ret;

	*vrect = read_buf[1] * 256 + read_buf[0];

	mca_log_info("trx vrect: %d\n", *vrect);

	return ret;
}

static void nuvolta_1671_set_pmic_icl(struct nuvolta_1671_chg * chip, int mA)
{
	if (chip->icl_votable)
		vote(chip->icl_votable, WLS_CHG_VOTER, true, mA);
	else
		mca_log_err("no icl votable, don't set icl\n");
	mca_log_info("wls set pmic icl: %d\n", mA);
	return;
}

static void nuvolta_1671_set_pmic_fcc(struct nuvolta_1671_chg *chip,int mA)
{
    if (chip->fcc_votable)
        vote(chip->fcc_votable, WLS_CHG_VOTER, true, mA);
    else
        mca_log_err("no fcc votable, don't set fcc\n");
    mca_log_info("wls set fcc: %d\n", mA);
    return;
}

static void nuvolta_1671_stepper_pmic_icl(struct nuvolta_1671_chg * chip,
	int start_icl, int end_icl, int step_ma, int ms)
{
	int temp_icl = start_icl;
	nuvolta_1671_set_pmic_icl(chip, temp_icl);
	if (start_icl < end_icl) {
		while (temp_icl < end_icl) {
			nuvolta_1671_set_pmic_icl(chip, temp_icl);
			temp_icl += step_ma;
			msleep(ms);
		}
	} else {
		while (temp_icl > end_icl) {
			nuvolta_1671_set_pmic_icl(chip, temp_icl);
			if (temp_icl > step_ma)
				temp_icl -= step_ma;
			else
				temp_icl = 0;
			msleep(ms);
		}
	}
	nuvolta_1671_set_pmic_icl(chip, end_icl);
	return;
}

static void nuvolta_1671_set_pmic_ichg(struct nuvolta_1671_chg * chip,int mA)
{
	if (chip->fcc_votable)
		vote(chip->fcc_votable, WLS_CHG_VOTER, true, mA);
	else
		mca_log_err("no fcc votable, don't set fcc\n");
	mca_log_info("wls set fcc: %d\n", mA);
	return;
}

static int nuvolta_1671_get_fcc(struct nuvolta_1671_chg * chip)
{
	int effective_fcc = 0;
	effective_fcc = get_effective_result(chip->fcc_votable);
	mca_log_info("wls get fcc: %d\n", effective_fcc);
	return effective_fcc;
}

static void nuvolta_1671_epp_uuid_func(struct nuvolta_1671_chg *chip)
{
	u8 vendor = 0, module = 0, version = 0, power = 0;
	vendor  = chip->uuid[0];
	module  = chip->uuid[1];
	version = chip->uuid[2];
	power   = chip->uuid[3];
	mca_log_info("epp uuid: vendor:0x%x, module:0x%x, version:0x%x, power:0x%x",
		vendor, module, version, power);
	if ((vendor == 0x9) && (module == 0x8) && (version == 0x6) && (power == 0x7))
		chip->is_music_tx = true;
	if (((vendor == 0x9) && (module == 0x1) && (version == 0x5) && (power == 0x6))
		|| ((vendor == 0xc) && (module == 0x9) && (version == 0x9) && (power == 0x8))
		|| ((vendor == 0xc) && (module == 0x9) && (version == 0x9) && (power == 0x6)))
		chip->is_plate_tx = true;
	if (((vendor == 0x6) && (module == 0x2) && (version == 0x8) && (power == 0x1))
		|| ((vendor == 0x1) && (module == 0x8) && (version == 0x2) && (power == 0x5)))
		chip->is_car_tx = true;
	if ((vendor == 0x1) && (module == 0x1) && (version == 0xE) && (power == 0x1))
		chip->is_train_tx = true;
	if ((vendor == 0x9) && (module == 0x1) && (version == 0x9) && (power == 0x1))
		chip->is_standard_tx = true;
	if (((vendor == 0x9) && (module == 0x1) && (version == 0x9) && (power == 0x4))
			|| ((vendor == 0x9) && (module == 0x1) && (version == 0x9) && (power == 0xc)))
		chip->is_sailboat_tx = true;
	if (((vendor == 0x9) && (module == 0x1) && (version == 0xa) && (power == 0x1))
        || ((vendor == 0x9) && (module == 0x1) && (version == 0x3) && (power == 0x5))
        || ((vendor == 0x9) && (module == 0x1) && (version == 0x9) && (power == 0x4))
        || ((vendor == 0x9) && (module == 0x1) && (version == 0x9) && (power == 0xc))
        || ((vendor == 0x9) && (module == 0x1) && (version == 0x9) && (power == 0x7)))
        chip->is_support_fan_tx = true;
	if ((vendor == 0x9) && (module == 0x1) && (version == 0xa) && (power == 0x1))
		chip->low_inductance_50w_tx = true;
	if ((vendor == 0x9) && (module == 0x1) && (version == 0x3) && (power == 0x5))
		chip->low_inductance_80w_tx = true;

	if ((chip->is_car_tx) && (chip->adapter_type >= ADAPTER_XIAOMI_QC3)) {
		mca_log_info("[TODO] is car tx");
		//TODO set wls car adapter node to 1
	}
	if (chip->is_music_tx)
		chip->adapter_type = ADAPTER_VOICE_BOX;
	return;
}

static int nuvolta_1671_send_transparent_data(struct nuvolta_1671_chg *chip,
	u8 *send_data, u8 length)
{
	u8 i;
	int ret = 0;

	mutex_lock(&chip->data_transfer_lock);

	if(length >= 3)
		mca_log_err("data[0] = 0x%x data[1] = 0x%x data[2] = 0x%x, length=%d\n",
			send_data[0], send_data[1], send_data[2], length);

	msleep(20);

	ret = rx1671_write(chip, RX_CMD_TRANSMIT_PACKET, REG_RX_SENT_CMD);//(0x0000, 0x69);
	if (ret < 0) {
		mca_log_err("FAILED!\n");
		goto exit;
	}

	ret = rx1671_write(chip, length, REG_RX_SENT_DATA1);
	if (ret < 0) {
		mca_log_err("write lenth FAILED!\n");
		goto exit;
	}

	for (i = 0; i < length; i++) {
		ret = rx1671_write(chip, send_data[i], (REG_RX_SENT_DATA2 + i));
		if (ret < 0) {
			mca_log_err("write data FAILED!\n");
			goto exit;
		}

		mca_log_err("i=%d, send_data[0+i]=0x%x, (0x0002+i)=0x%x\n",
						i, send_data[i], (REG_RX_SENT_DATA2 + i));
	}

	ret = rx1671_write(chip, (length + 2), REG_RX_INT_0);
	if (ret < 0) {
		mca_log_err("write trigger FAILED!\n");
		goto exit;
	}

exit:
	mutex_unlock(&chip->data_transfer_lock);
	return ret;
}

static int nuvolta_1671_process_factory_cmd(struct nuvolta_1671_chg *chip, u8 cmd)
{
	int ret = 0;
	u8 send_data[8];
	u8 data_h;
	u8 data_l;
	int rx_iout;
	int rx_vout;
	u8 index = 0;

	switch (cmd) {
	case FACTORY_TEST_CMD_RX_IOUT:
		ret = nuvolta_1671_get_iout(chip, &rx_iout);
		data_h = ((uint32_t)rx_iout & 0x00ff);
		data_l = ((uint32_t)rx_iout & 0xff00) >> 8;
		index = 0;
		send_data[index++] = TX_ACTION_NO_REPLY;
		send_data[index++] = TRANS_DATA_LENGTH_3BYTE;
		send_data[index++] = FACTORY_TEST_CMD_RX_IOUT;
		send_data[index++] = data_h;
		send_data[index++] = data_l;
		mca_log_err("nuvolta_1671_factory_test --rx_iout--0x%x,0x%x iout=%d\n", data_h, data_l, rx_iout);
		break;
	case FACTORY_TEST_CMD_RX_VOUT:
		ret = nuvolta_1671_get_vout(chip, &rx_vout);
		data_h = ((uint32_t)rx_vout & 0x00ff);
		data_l = ((uint32_t)rx_vout & 0xff00) >> 8;
		index = 0;
		send_data[index++] = TX_ACTION_NO_REPLY;
		send_data[index++] = TRANS_DATA_LENGTH_3BYTE;
		send_data[index++] = FACTORY_TEST_CMD_RX_VOUT;
		send_data[index++] = data_h;
		send_data[index++] = data_l;
		mca_log_err("nuvolta_1671_factory_test --rx_vout--0x%x,0x%x vout=%d\n", data_h, data_l, rx_vout);
		break;
	case FACTORY_TEST_CMD_RX_FW_ID:
		index = 0;
		send_data[index++] = TX_ACTION_NO_REPLY;
		send_data[index++] = TRANS_DATA_LENGTH_5BYTE;
		send_data[index++] = FACTORY_TEST_CMD_RX_FW_ID;
		send_data[index++] = 0x0;
		send_data[index++] = 0x0;
		send_data[index++] = chip->wls_fw_data->fw_version;
		send_data[index++] = chip->wls_fw_data->fw_version;
		mca_log_err("nuvolta_1671_factory_test --fw_version--0x%x 0x%x\n", chip->wls_fw_data->fw_version, chip->wls_fw_data->fw_version);
		break;
	case FACTORY_TEST_CMD_RX_CHIP_ID:
		index = 0;
		send_data[index++] = TX_ACTION_NO_REPLY;
		send_data[index++] = TRANS_DATA_LENGTH_3BYTE;
		send_data[index++] = FACTORY_TEST_CMD_RX_CHIP_ID;
		send_data[index++] = 0x16;
		send_data[index++] = 0x71;
		mca_log_err("nuvolta_1671_factory_test --rx_chip_id--0x%x0x%x\n", chip->wls_fw_data->hw_id_h, chip->wls_fw_data->hw_id_l);
		break;
	case FACTORY_TEST_CMD_ADAPTER_TYPE:
		index = 0;
		send_data[index++] = TX_ACTION_NO_REPLY;
		send_data[index++] = TRANS_DATA_LENGTH_2BYTE;
		send_data[index++] = FACTORY_TEST_CMD_ADAPTER_TYPE;
		send_data[index++] = chip->adapter_type;
		mca_log_err("nuvolta_1671_factory_test --usb_type--%d\n", chip->adapter_type);
		break;
	case FACTORY_TEST_CMD_REVERSE_REQ:
		index = 0;
		send_data[index++] = TX_ACTION_NO_REPLY;
		send_data[index++] = TRANS_DATA_LENGTH_1BYTE;
		send_data[index++] = FACTORY_TEST_CMD_REVERSE_REQ;
		chip->revchg_test_status = REVERSE_TEST_SCHEDULE;
		mca_log_err("[ factory reverse test] receive request\n");
		break;
	default:
		return -1;
	}
	ret = nuvolta_1671_send_transparent_data(chip, send_data, index);
	return ret;
}

static int nuvolta_1671_rcv_factory_test_cmd(struct nuvolta_1671_chg *chip,
	u8 *rcv_data, u8 *length)
{
	int ret = 0;
	u8 read_buf[16];
	u8 data_length;
	u8 i;
	const u8 max_data_length = 15;

	ret = rx1671_write(chip, 0x88, 0x0062);
	if (ret < 0)
		return ret;

	ret = rx1671_read_buffer(chip, read_buf, (0x2320+0x74), 16);
	if (ret < 0)
		return ret;

	data_length = read_buf[0];
	data_length = data_length > max_data_length ? max_data_length : data_length;
	*length = data_length;

	mca_log_info("data_length=%d\n", data_length);
	for (i = 0; i < data_length; i++) {
		rcv_data[i] = read_buf[i + 1];
		mca_log_info("i=%d, data[i]=0x%x\n", i, rcv_data[i]);
	}

	return ret;
}

static u8 nuvolta_1671_get_rx_power_mode(struct nuvolta_1671_chg *chip)
{
	u8 power_mode = 0;
	u8 read_buf[5] = {0};
	int ret = 0;

	ret = rx1671_read(chip, &read_buf[0], (0x2320+0x0a));
	if (ret < 0)
		return ret;

	power_mode = read_buf[0];

	mca_log_err("rx power mode: %d\n", power_mode);
	mca_log_err("boot: 0x%x, rx: 0x%x, tx: 0x%x\n",
				read_buf[0], read_buf[1], read_buf[2]);
	return power_mode;
}

static u8 nuvolta_1671_get_fastchg_result(struct nuvolta_1671_chg *chip, u8 *fc_flag)
{
	u8 fastchg_result = 0;
	int ret = 0;

	ret = rx1671_read(chip, &fastchg_result, (0x2320+0xa2));
	if (ret < 0)
			return ret;

	*fc_flag = fastchg_result;
	mca_log_err("fastch result: %d\n", *fc_flag);

	return ret;
}

static u8 nuvolta_1671_get_auth_value(struct nuvolta_1671_chg *chip, u8 *value)
{
	u8 read_buf[RX_AUTH_DATA_LENGTH];
	u8 auth_data = 0;
	int ret = 0;

	ret = rx1671_read(chip, &read_buf[0], (0x2320+0xa0));
	if (ret < 0)
		return ret;

	auth_data = read_buf[0];
	mca_log_err("auth_data = 0x%x\n", auth_data);

	if (auth_data > AUTH_STATUS_FAILED) {

		ret = rx1671_read(chip, &read_buf[0], (0x2320+0x0e));
		if (ret < 0)
			return ret;

		ret = rx1671_read(chip, &read_buf[1], (0x2320+0x0f));
		if (ret < 0)
			return ret;

		chip->epp_tx_id_l = read_buf[0];
		chip->epp_tx_id_h = read_buf[1];
	}

	ret = rx1671_read(chip, &read_buf[0], (0x2320+0xa3));
	if (ret < 0)
		return ret;

	chip->adapter_type = read_buf[0];

	ret = rx1671_read(chip, &read_buf[0], (0x2320+0x19));
	if (ret < 0)
		return ret;

	// chip->tx_type = read_buf[0]; TODO

	if (chip->adapter_type == ADAPTER_NONE)
		chip->adapter_type = ADAPTER_AUTH_FAILED;

	if (auth_data > AUTH_STATUS_UUID_OK) {
		ret = rx1671_read(chip, &read_buf[0], (0x2320+0xa4));
		if (ret < 0)
			return ret;
		ret = rx1671_read(chip, &read_buf[1], (0x2320+0xa5));
		if (ret < 0)
			return ret;

		ret = rx1671_read(chip, &read_buf[2], (0x2320+0xa6));
		if (ret < 0)
			return ret;

		ret = rx1671_read(chip, &read_buf[3], (0x2320+0xa7));
		if (ret < 0)
			return ret;

		chip->uuid[0] = read_buf[0];
		chip->uuid[1] = read_buf[1];
		chip->uuid[2] = read_buf[2];
		chip->uuid[3] = read_buf[3];
	}

	*value = auth_data;

	mca_log_err("tx_id_l: 0x%x, tx_id_h: 0x%x\n",
		chip->epp_tx_id_l, chip->epp_tx_id_h);
	mca_log_err("adapter type: %d\n", chip->adapter_type);
	mca_log_err("uuid: 0x%x, 0x%x, 0x%x, 0x%x\n",
		chip->uuid[0], chip->uuid[1], chip->uuid[2], chip->uuid[3]);

	return 0;
}

static int nuvolta_1671_get_rx_rtx_mode(struct nuvolta_1671_chg *chip, u8 *mode)
{
	int ret = 0;
	u8 temp = 0;

	ret = rx1671_read(chip, &temp, RX_RTX_MODE);
	mca_log_err("data = %d\n", temp);
	if (temp == TRX_MODE_STATUS)
		*mode = RTX_MODE;
	else
		*mode = RX_MODE;
	return ret;
}

static int nuvolta_1671_power_off_err(struct nuvolta_1671_chg *chip, u8 *err_code)
{
	int ret = 0;
	int i;
	u8 read_buf[16];
	u16 base_addr = 0x2320;
	int total = 0;
	char str[512] = {0};

	/* dump registers */
	ret = rx1671_read_buffer(chip, read_buf, (0x2320+0x30), 16);
	if (ret < 0) {
		mca_log_err("fail to read poweroff related registers\n");
		return ret;
	}

	for (i = 0; i < 8; i++)
		total += snprintf(str + total, sizeof(str) - total, "[0x%02X]=0x%02X,",
			base_addr + i, read_buf[i]);
	mca_log_info("dump: %s\n", str);

	total = 0;
	memset(str, 0, sizeof(str));
	for (i = 8; i < 16; i++)
		total += snprintf(str + total, sizeof(str) - total, "[0x%02X]=0x%02X,",
			base_addr + i, read_buf[i]);
	mca_log_info("dump: %s\n", str);

	/* poweroff error code */
	ret = rx1671_read(chip, err_code, RX_POWER_OFF_ERR);
	if (ret < 0) {
		mca_log_err("fail to read poweroff error code\n");
		return ret;
	}

	return ret;
}

static int nuvolta_1671_do_renego(struct nuvolta_1671_chg *chip, u8 max_power)
{
	int ret = 0;

	mutex_lock(&chip->data_transfer_lock);

	mca_log_err("max_power = %d\n", max_power);

	ret = rx1671_write(chip, RX_CMD_RENEGO_SET, REG_RX_SENT_CMD);
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, RENEGO_LENGTH, REG_RX_SENT_DATA1);
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, max_power, REG_RX_SENT_DATA2);
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, RENEGO_TRIGGER_RX, REG_RX_INT_0);
	if (ret < 0)
		goto exit;

exit:
	mutex_unlock(&chip->data_transfer_lock);
	return ret;
}

static void nuvolta_1671_adapter_handle(struct nuvolta_1671_chg *chip)
{
	mca_log_info("adapter: %d, epp: %d\n",
		chip->adapter_type, chip->epp);
	if (!chip->fc_flag) {
		switch (chip->adapter_type) {
		case ADAPTER_SDP:
		case ADAPTER_CDP:
		case ADAPTER_DCP:
		case ADAPTER_QC2:
			mca_log_info("set icl for SDP/CDP/DCP/QC2 adapter\n");
			nuvolta_1671_set_pmic_icl(chip, 750);
			nuvolta_1671_set_pmic_ichg(chip, 1000);
			chip->pre_curr = 750;
			break;
		case ADAPTER_QC3:
		case ADAPTER_PD:
		case ADAPTER_AUTH_FAILED:
			if (chip->epp) {
				mca_log_info("set icl in EPP for QC3/PD/FAIL adapter\n");
				nuvolta_1671_set_pmic_icl(chip, 850);
				nuvolta_1671_set_pmic_ichg(chip, 2000);
				chip->pre_curr = 850;
			} else {
				mca_log_info("set icl in BPP for QC3/PD/FAIL adapter\n");
				nuvolta_1671_set_pmic_icl(chip, 750);
				nuvolta_1671_set_pmic_ichg(chip, 1000);
				chip->pre_curr = 750;
				chip->pre_vol = BPP_DEFAULT_VOUT;
			}
			break;
		case ADAPTER_XIAOMI_QC3:
		case ADAPTER_XIAOMI_PD:
		case ADAPTER_ZIMI_CAR_POWER:
		case ADAPTER_XIAOMI_PD_40W:
		case ADAPTER_VOICE_BOX:
		case ADAPTER_XIAOMI_PD_50W:
		case ADAPTER_XIAOMI_PD_60W:
		case ADAPTER_XIAOMI_PD_100W:
			mca_log_info("set icl for adapter more than 9\n");
			nuvolta_1671_set_pmic_icl(chip, 850);
			nuvolta_1671_set_pmic_ichg(chip, 2000);
			chip->pre_curr = 850;
			chip->pre_vol = EPP_DEFAULT_VOUT;
			break;
		default:
			mca_log_info("other adapter type\n");
			break;
		}
	} else {
		switch (chip->adapter_type) {
		case ADAPTER_QC3:
		case ADAPTER_PD:
			msleep(2000);
			mca_log_info("set icl for QC3/PD in FC\n");
			nuvolta_1671_stepper_pmic_icl(chip, 800, 1100, 100, 20);
			nuvolta_1671_set_pmic_ichg(chip, 2000);
			chip->pre_curr = 1100;
			chip->pre_vol = BPP_PLUS_VOUT;
			break;
		case ADAPTER_XIAOMI_QC3:
		case ADAPTER_XIAOMI_PD:
		case ADAPTER_ZIMI_CAR_POWER:
			mca_log_info("set fcc for 20W adapter\n");
			nuvolta_1671_set_pmic_ichg(chip, 4000);
			chip->pre_vol = EPP_DEFAULT_VOUT;
			chip->qc_enable = true;
			chip->is_vout_range_set_done = true;
			break;
		case ADAPTER_XIAOMI_PD_40W:
		case ADAPTER_VOICE_BOX:
			mca_log_info("set fcc for 30W adapter\n");
			nuvolta_1671_set_pmic_ichg(chip, 6000);
			chip->pre_vol = EPP_DEFAULT_VOUT;
			chip->qc_enable = true;
			chip->is_vout_range_set_done = true;
			break;
		case ADAPTER_XIAOMI_PD_50W:
		case ADAPTER_XIAOMI_PD_60W:
		case ADAPTER_XIAOMI_PD_100W:
			mca_log_info("set fcc for adapters of more than 50W\n");
			nuvolta_1671_set_pmic_ichg(chip, 9200);
			chip->pre_vol = EPP_DEFAULT_VOUT;
			chip->qc_enable = true;
			if (chip->is_car_tx)
				chip->is_vout_range_set_done = true;
			break;
		default:
			mca_log_info("other adapter type, break\n");
			break;
		}
	}
	if (chip->wireless_psy)
		power_supply_changed(chip->wireless_psy);
	schedule_delayed_work(&chip->chg_monitor_work,
			msecs_to_jiffies(1000));
	return;
}

static void nuvolta_1671_start_renego(struct nuvolta_1671_chg *chip)
{
	u8 max_power = 0;
	switch (chip->adapter_type) {
	case ADAPTER_XIAOMI_PD_40W:
    case ADAPTER_VOICE_BOX:
        max_power = 20;
        break;
    case ADAPTER_XIAOMI_PD_50W:
        if ((chip->uuid[0] == 0x9) && (chip->uuid[1] == 0x1) && (chip->uuid[2] == 0xa) && (chip->uuid[3] == 0x1))
            max_power = 25;
        else
            max_power = 20;
        break;
	case ADAPTER_XIAOMI_PD_60W:
	case ADAPTER_XIAOMI_PD_100W:
		max_power = 25;
		break;
	default:
		break;
	}
	if (max_power > 0)
		nuvolta_1671_do_renego(chip, max_power);
	return;
}

static void nuvolta_1671_set_fastchg_adapter_v(struct nuvolta_1671_chg *chip)
{
	int ret = 0;
	switch (chip->adapter_type) {
	case ADAPTER_QC3:
	case ADAPTER_PD:
		if (!chip->epp) {
			mca_log_info("bpp+ set adapter voltage to 9V\n");
			ret = nuvolta_1671_set_adapter_voltage(chip, BPP_PLUS_VOUT);
			if (ret < 0)
				mca_log_info("bpp+ set adapter voltage failed!!!\n");
		}
		break;
	case ADAPTER_XIAOMI_QC3:
	case ADAPTER_XIAOMI_PD:
	case ADAPTER_ZIMI_CAR_POWER:
	case ADAPTER_XIAOMI_PD_40W:
	case ADAPTER_VOICE_BOX:
	case ADAPTER_XIAOMI_PD_50W:
	case ADAPTER_XIAOMI_PD_60W:
	case ADAPTER_XIAOMI_PD_100W:
		/* close pmic ovp trigger befor raise voltage to 15V */
		ret = nuvolta_1671_set_adapter_voltage(chip, EPP_PLUS_VOUT);
		if (ret < 0)
			mca_log_info("epp+ set adapter voltage failed!!!\n");
		else
			mca_log_info("EPP+ set adapter voltage to 15V\n");
		break;
	default:
		mca_log_info("other adapter, don't set adapter voltage\n");
		break;
	}
	return;
}

static int nuvolta_1671_enable_power_path(struct nuvolta_1671_chg *chip, bool en)
{
	mca_log_info("%d\n", en);
	return charger_dev_enable_powerpath(chip->chg_dev, true);
	return 0;
}

static int nuvolta_1671_enable_bc12(struct nuvolta_1671_chg *chip, bool attach)
{
	union power_supply_propval prop;
	static struct power_supply *bc12_psy;
	bc12_psy = power_supply_get_by_name("primary_chg");
	if (IS_ERR_OR_NULL(bc12_psy)) {
		mca_log_err("Couldn't get bc12_psy\n");
		return -EINVAL;
	} else {
		prop.intval = attach;
		return power_supply_set_property(bc12_psy,
					 POWER_SUPPLY_PROP_ONLINE, &prop);
	}
}

static void nuvolta_1671_clear_int(struct nuvolta_1671_chg *chip)
{
	int ret = 0;

	mutex_lock(&chip->data_transfer_lock);

	ret = rx1671_write(chip, RX_CMD_CLEAR_INT, REG_RX_SENT_CMD);//(0x0000, 0x68)
	ret = rx1671_write(chip, RX_CLEAR_INT_LENGTH, REG_RX_SENT_DATA1);//(0x0001, 0x02)
	ret = rx1671_write(chip, 0xff, REG_RX_SENT_DATA2);//(0x0002, 0xff);
	ret = rx1671_write(chip, 0xff, REG_RX_SENT_DATA3);//(0x0003, 0xff);
	ret = rx1671_write(chip, RX_CLEAR_INT_TRIGGER_RX, REG_RX_INT_0);//(0x0060, 0x04);

	mca_log_err("clear int ret: %d\n", ret);

	mutex_unlock(&chip->data_transfer_lock);
}

static int nuvolta_1671_reverse_enable_fod(struct nuvolta_1671_chg *chip, bool enable)
{
	int ret = 0;
	u8 gain = 94;
	u8 offset = 0;

	mutex_lock(&chip->data_transfer_lock);

	if (enable) {
		ret = rx1671_write(chip, RX_CMD_ENABLE_REVERSE_FOD, REG_RX_SENT_CMD);
		if (ret < 0)
			goto exit;
		ret = rx1671_write(chip, REVERSE_FOD_EN, REG_RX_SENT_DATA1);
		if (ret < 0)
			goto exit;
		ret = rx1671_write(chip, gain, REG_RX_SENT_DATA2);
		if (ret < 0)
			goto exit;
		ret = rx1671_write(chip, offset, REG_RX_SENT_DATA3);
		if (ret < 0)
			goto exit;
		ret = rx1671_write(chip, REVERSE_FOD_TRIGGER_RX, REG_RX_INT_0);
		if (ret < 0)
			goto exit;
		mca_log_info("gain: %d, offset:%d\n", gain, offset);
	} else {
		ret = rx1671_write(chip, RX_CMD_ENABLE_REVERSE_FOD, REG_RX_SENT_CMD);
		if (ret < 0)
			goto exit;
		ret = rx1671_write(chip, REVERSE_FOD_DIS, REG_RX_SENT_DATA1);
		if (ret < 0)
			goto exit;
		ret = rx1671_write(chip, DISABLE_REVERSE_FOD_TRIGGER_RX, REG_RX_INT_0);
		if (ret < 0)
			goto exit;
		mca_log_info("disable reverse fod\n");
	}

exit:
	mutex_unlock(&chip->data_transfer_lock);
	return ret;
}

static void nuvolta_1671_reverse_chg_handler(struct nuvolta_1671_chg *chip, u16 int_flag)
{
	uint32_t err_code = 0, dfx_code = 0;
    bool need_report = false;
    int len;
    char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
    struct mca_event_notify_data event_data = { 0 };
	switch (int_flag) {
	case RTX_INT_PING:
		mca_log_err("RTX_INT_PING!\n");
        if (!chip->tx_timeout_flag)
            cancel_delayed_work_sync(&chip->reverse_ping_timeout_work);
        schedule_delayed_work(&chip->reverse_transfer_timeout_work, msecs_to_jiffies(REVERSE_TRANSFER_TIMEOUT_TIMER));
		break;
	case RTX_INT_GET_RX:
		mca_log_err("RTX_INT_GET_RX!\n");
        nuvolta_1671_reverse_enable_fod(chip, true);
        chip->is_reverse_chg = REVERSE_STATE_TRANSFER;
        need_report = true;
        if (!chip->tx_timeout_flag)
            cancel_delayed_work_sync(&chip->reverse_transfer_timeout_work);
		break;
	case RTX_INT_CEP_TIMEOUT:
		mca_log_err("RTX_INT_CEP_TIMEOUT!\n");
        chip->is_reverse_chg = REVERSE_STATE_WAITPING;
        need_report = true;
        schedule_delayed_work(&chip->reverse_ping_timeout_work, msecs_to_jiffies(REVERSE_PING_TIMEOUT_TIMER));
		break;
	case RTX_INT_EPT:
		mca_log_err("RTX_INT_EPT!\n");
        schedule_delayed_work(&chip->reverse_ping_timeout_work, msecs_to_jiffies(REVERSE_PING_TIMEOUT_TIMER));
		break;
	case RTX_INT_PROTECTION:
		 mca_log_err("RTX_INT_PROTECTION!\n");
		if (!chip->is_reverse_closing) {
            nuvolta_1671_set_reverse_chg_mode(chip, false);
            chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
            need_report = true;
        }
		break;
	case RTX_INT_GET_TX:
		mca_log_err("RTX_INT_GET_TX!\n");
		if (!chip->is_reverse_closing) {
            nuvolta_1671_set_reverse_chg_mode(chip, false);
            chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
            need_report = true;
        }
		break;
	case RTX_INT_REVERSE_TEST_READY:
		mca_log_err("RTX_INT_REVERSE_TEST_READY!\n");
        chip->revchg_test_status = REVERSE_TEST_READY;
        if (!chip->tx_timeout_flag)
            cancel_delayed_work(&chip->factory_reverse_stop_work);
		break;
	case RTX_INT_REVERSE_TEST_DONE:
		if (!chip->is_reverse_closing) {
            mca_log_err("RTX_INT_REVERSE_TEST_DONE!\n");
            nuvolta_1671_set_reverse_chg_mode(chip, false);
            chip->revchg_test_status = REVERSE_TEST_DONE;
        }
		break;
	case RTX_INT_FOD:
		mca_log_err("RTX_INT_FOD!\n");
		 if (!chip->is_reverse_closing) {
            nuvolta_1671_set_reverse_chg_mode(chip, false);
            chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
            need_report = true;
            mca_charge_mievent_report(CHARGE_DFX_WLS_TRX_FOD, NULL, 0);
        }
		break;
	case RTX_INT_EPT_PKT:
		mca_log_err("RTX_INT_EPT_PKT!\n");
        schedule_delayed_work(&chip->reverse_transfer_timeout_work, msecs_to_jiffies(REVERSE_TRANSFER_TIMEOUT_TIMER));
		break;
	case RTX_INT_ERR_CODE:
        mca_log_err("RTX_INT_ERR_CODE! code=[%d %d]!\n", err_code, dfx_code);
		break;
	default:
		mca_log_info("tx mode unknown interrupt: 0x%x\n", int_flag);
		break;
	}
	if (need_report) {
        len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_REVERSE_CHG_STATE=%d", chip->is_reverse_chg);
        event_data.event = event;
        event_data.event_len = len;
        mca_event_report_uevent(&event_data);
    }
	if (chip->wireless_psy)
		power_supply_changed(chip->wireless_psy); //TODO delete power supply
	return;
}

static void nuvolta_1671_process_power_reduce_cmd(struct nuvolta_1671_chg *chip, u8 cmd)
{
	switch(cmd){
	case RX_INT_POWER_REDUCE_F0:
		mca_log_info("RX_INT_POWER_REDUCE_F0 trigger\n");
		if(chip->current_for_adapter_cmd != ADAPTER_CMD_TYPE_F0){
			vote(chip->fcc_votable, WLS_POWER_REDUCE_VOTER, false, 0);
		}
		chip->current_for_adapter_cmd = ADAPTER_CMD_TYPE_F0;
		break;
	case RX_INT_POWER_REDUCE_F1:
		mca_log_info("RX_INT_POWER_REDUCE_F1 trigger\n");
		if(chip->current_for_adapter_cmd != ADAPTER_CMD_TYPE_F1){
			vote(chip->fcc_votable, WLS_POWER_REDUCE_VOTER, true, 6000);
		}
		chip->current_for_adapter_cmd = ADAPTER_CMD_TYPE_F1;
		break;
	case RX_INT_POWER_REDUCE_F2:
		mca_log_info("RX_INT_POWER_REDUCE_F2 trigger\n");
		if(chip->current_for_adapter_cmd != ADAPTER_CMD_TYPE_F2){
			vote(chip->fcc_votable, WLS_POWER_REDUCE_VOTER, true, 4000);
		}
		chip->current_for_adapter_cmd = ADAPTER_CMD_TYPE_F2;
		break;
	case RX_INT_POWER_REDUCE_F3:
		mca_log_info("RX_INT_POWER_REDUCE_F3 trigger\n");
		if(chip->current_for_adapter_cmd != ADAPTER_CMD_TYPE_F3){
			vote(chip->fcc_votable, WLS_POWER_REDUCE_VOTER, true, 2000);
		}
		chip->current_for_adapter_cmd = ADAPTER_CMD_TYPE_F3;
		break;
	case RX_INT_POWER_REDUCE_F4:
		mca_log_info("RX_INT_POWER_REDUCE_F4 trigger\n");
		if(chip->current_for_adapter_cmd != ADAPTER_CMD_TYPE_F4){
			vote(chip->fcc_votable, WLS_POWER_REDUCE_VOTER, true, 1000);
		}
		chip->current_for_adapter_cmd = ADAPTER_CMD_TYPE_F4;
		break;
	default:
		mca_log_info("RX_INT_POWER_REDUCE default\n");
		break;
	}
}

static void nuvolta_1671_process_q_value_strategy(struct nuvolta_1671_chg *chip, bool limt_en)
{
	if (limt_en) {
        vote(chip->fcc_votable, WLS_Q_VALUE_STRATEGY_VOTER, true, WLS_CHG_TX_QLIMIT_FCC_5W);
        vote(chip->icl_votable, WLS_Q_VALUE_STRATEGY_VOTER, true, WLS_CHG_TX_QLIMIT_ICL_5W);
    } else {
        vote(chip->fcc_votable, WLS_Q_VALUE_STRATEGY_VOTER, false, 0);
        vote(chip->icl_votable, WLS_Q_VALUE_STRATEGY_VOTER, false, 0);
    }
    mca_log_info("limt_en=%d\n", limt_en);
}

static int nuvolta_1671_update_soc_to_tx(struct nuvolta_1671_chg * chip, int soc)
{
#ifdef CONFIG_FACTORY_BUILD
    return 0;
#endif
    int ret = 0;
    uint8_t data[3] = {0};

    if (!chip->power_good_flag) {
        mca_log_info("power good off, don't update soc\n");
        return ret;
    }

    if (chip->adapter_type < ADAPTER_XIAOMI_QC3) {
        mca_log_info("adapter type is %d, don't update soc\n", chip->adapter_type);
        return ret;
    }

    chip->mutex_lock_sts = true;
    data[0] = 0x00;
    data[1] = 0x05;
    data[2] = (u8)(soc & 0xFF);
    ret = nuvolta_1671_send_transparent_data(chip, data, ARRAY_SIZE(data));
    if (ret) {
        mca_log_err("update soc to tx fail\n");
    } else {
        chip->current_trans_packet_type = WLS_SOC_PACKET;
        mca_log_info("update soc: %d\n", soc);
    }

    schedule_delayed_work(&chip->mutex_unlock_work, msecs_to_jiffies(1200));
    return ret;
}

static int nuvolta_1671_update_q_value_strategy_to_tx(struct nuvolta_1671_chg * chip, uint8_t q_value)
{
    int ret = 0;
    uint8_t send_value[4] = {0x01, 0x28, 0x62, 0};

    if (!chip->power_good_flag) {
        mca_log_info("power good off, don't update soc\n");
        return ret;
    }

    chip->mutex_lock_sts = true;
    send_value[3] = (uint8_t)(q_value & 0xFF);
    ret = nuvolta_1671_send_transparent_data(chip, send_value, ARRAY_SIZE(send_value));
    if (ret)
        mca_log_err("update q_value_strategy to tx fail\n");
    else {
        chip->current_trans_packet_type = WLS_Q_STARTEGY_PACKET;
        mca_log_info("{0x%02x, 0x%02x, 0x%02x, 0x%02x}\n",
                	send_value[0], send_value[1], send_value[2], send_value[3]);
    }

    schedule_delayed_work(&chip->mutex_unlock_work, msecs_to_jiffies(1500));
    return ret;
}

static int nuvolta_1671_send_frequency_to_tx(struct nuvolta_1671_chg *chip, int freq_khz)
{
    int ret = 0;
    uint8_t send_value[4] = {0x00, 0x28, 0xd3, 0};

    if (!chip->power_good_flag) {
        mca_log_err("%s power good off\n", __func__);
        return -1;
    }

    if (freq_khz < SUPER_TX_FREQUENCY_MIN_KHZ || freq_khz > SUPER_TX_FREQUENCY_MAX_KHZ) {
        mca_log_err("freq %d invalid.\n", freq_khz);
        return -1;
    }

    chip->mutex_lock_sts = true;
    send_value[3] = (uint8_t)(freq_khz & 0xFF);
    ret = nuvolta_1671_send_transparent_data(chip, send_value, ARRAY_SIZE(send_value));
    if (ret) {
        mca_log_err("fail\n");
    } else {
        mca_log_info("%d success\n", freq_khz);
        chip->current_trans_packet_type = WLS_FREQUENCE_PACKET;
    }

    schedule_delayed_work(&chip->mutex_unlock_work, msecs_to_jiffies(1200));
    return ret;
}

static int nuvolta_1671_send_vout_range_to_tx(struct nuvolta_1671_chg *chip, int max_volt_mv)
{
    int ret = 0;
    uint8_t send_value[4] = {0x00, 0x28, 0xd6, 0};

    if (!chip->power_good_flag) {
        mca_log_err("power good off\n");
        return -1;
    }

    if (max_volt_mv < SUPER_TX_VOUT_MIN_MV || max_volt_mv > SUPER_TX_VOUT_MAX_MV) {
        mca_log_err(" max_volt %d invalid.\n", max_volt_mv);
        return -1;
    }

    chip->mutex_lock_sts = true;
    send_value[3] = (uint8_t)((max_volt_mv - SUPER_TX_VOUT_MIN_MV) / 500);
    ret = nuvolta_1671_send_transparent_data(chip, send_value, ARRAY_SIZE(send_value));
    if (ret) {
        mca_log_err("fail\n");
    } else {
        mca_log_info("%d success\n", max_volt_mv);
        chip->current_trans_packet_type = WLS_VOUT_RANGE_PACKET;
    }

    schedule_delayed_work(&chip->mutex_unlock_work, msecs_to_jiffies(1200));
    return ret;
}

static int nuvolta_1671_send_fan_speed_to_tx(struct nuvolta_1671_chg *chip, int value)
{
	int ret = 0;
	u8 send_value[5] = {0x00, 0x38, 0x63, 0, 0};

	if (!chip->power_good_flag)
		return -1;

	if (value < SUPER_TX_FAN_SPEED_MIN_PERCENT || value > SUPER_TX_FAN_SPEED_MAX_PERCENT)
		return -1;

	if (chip->tx_speed == value)
		return ret;

	chip->mutex_lock_sts = true;
	send_value[3] = (u8)value;
	chip->tx_speed = value;

	ret = nuvolta_1671_send_transparent_data(chip, send_value, ARRAY_SIZE(send_value));
	if (ret) {
        mca_log_err("fail\n");
    } else {
        mca_log_info("%d success\n", chip->tx_speed);
        chip->current_trans_packet_type = WLS_FAN_SPEED_PACKET;
    }
	schedule_delayed_work(&chip->mutex_unlock_work, msecs_to_jiffies(1500));
	return ret;
}

static void nuvolta_1671_process_trans_func(struct nuvolta_1671_chg *chip, struct trans_data_lis_node *node)
{
    switch (node->data_flag) {
    case TRANS_DATA_FLAG_SOC:
        nuvolta_1671_update_soc_to_tx(chip, node->value);
        break;
    case TRANS_DATA_FLAG_QVALUE:
        nuvolta_1671_update_q_value_strategy_to_tx(chip, node->value);
        break;
    case TRANS_DATA_FLAG_FAN_SPEED:
        nuvolta_1671_send_fan_speed_to_tx(chip, node->value);
        break;
    case TRANS_DATA_FLAG_VOUT_RANGE:
        nuvolta_1671_send_vout_range_to_tx(chip, node->value);
        break;
    case TRANS_DATA_FLAG_FREQUENCE:
        nuvolta_1671_send_frequency_to_tx(chip, node->value);
        break;
    default:
        mca_log_err("not support this type\n");
        break;
    }
}

static int nuvolta_1671_process_trans(struct nuvolta_1671_chg *chip)
{
    struct trans_data_lis_node *cur_node, *temp_node;

    while (!list_empty(&chip->header) && !chip->mutex_lock_sts) {
        spin_lock(&chip->list_lock);
        list_for_each_entry_safe(cur_node, temp_node, &chip->header, lnode) {
            if (chip->mutex_lock_sts)
                break;
            list_del(&cur_node->lnode);
            spin_unlock(&chip->list_lock);

            mca_log_info("cur_node: data_flag: %d, value: %d\n", cur_node->data_flag, cur_node->value);
            nuvolta_1671_process_trans_func(chip, cur_node);

            spin_lock(&chip->list_lock);
            kfree(cur_node);
            chip->head_cnt--;
        }
        spin_unlock(&chip->list_lock);
    }

    if (!chip->power_good_flag)
        return 1;

    return 0;
}

static void nuvolta_1671_trans_data_work(struct work_struct *work)
{
    struct nuvolta_1671_chg *chip = container_of(work, struct nuvolta_1671_chg,
            trans_data_work.work);

    while (chip->power_good_flag)
        wait_event_interruptible(chip->wait_que, (nuvolta_1671_process_trans(chip)));
}

static void nuvolta_1671_mutex_unlock_work(struct work_struct *work)
{
    struct nuvolta_1671_chg *chip = container_of(work, struct nuvolta_1671_chg,
            mutex_unlock_work.work);

    if (chip->mutex_lock_sts) {
        chip->mutex_lock_sts = false;
        wake_up_interruptible(&chip->wait_que);
    }
}

static void nuvolta_1671_add_trans_task_to_queue(struct nuvolta_1671_chg *chip, TRANS_DATA_FLAG data_flag, int value)
{
    struct trans_data_lis_node *node = NULL;

    node = kmalloc(sizeof(struct trans_data_lis_node), GFP_ATOMIC);
    if (!node) {
        mca_log_err("create node error, return\n");
        return;
    }

    mca_log_err("add: data flag: 0x%02x, value: %d\n", data_flag, value);

    spin_lock(&chip->list_lock);
    node->data_flag = data_flag;
    node->value = value;
    list_add_tail(&node->lnode, &chip->header);
    chip->head_cnt++;
    spin_unlock(&chip->list_lock);

    wake_up_interruptible(&chip->wait_que);
}

static int nuvolta_1671_rcv_transparent_data(struct nuvolta_1671_chg *chip, u8 *rcv_value, 
					int buff_len, u8 *rcv_len)
{
	u8 read_buf[128];
	int ret = 0;
	int i;
	u8 rsp_len = 0;

	ret = rx1671_read(chip, &read_buf[0], (0x2320+0x5b));
	if (ret < 0)
			return ret;
	*rcv_len = read_buf[0];
	mca_log_err("receive_transparent_data data_length=%d\n", *rcv_len);

	rsp_len = read_buf[0];
	rsp_len = (rsp_len >> 4) + (rsp_len & 0x0F);
	if (rsp_len > buff_len) {
		mca_log_err("receive_transparent_data buffer overflow\n");
		*rcv_len = 0;
		return -1;
	}

	for (i = 0; i < rsp_len; i++) {
		ret = rx1671_read(chip, &rcv_value[i], (0x2320+0x5c+i));
		if (ret < 0)
			return ret;
		mca_log_err("receive_transparent_data i=%d, data[i]=0x%x\n", i, rcv_value[i]);
	}

	return ret;
}

static void nu1671_update_qucikchg_type(struct nuvolta_1671_chg *chip)
{
    int qc_type = QUICK_CHARGE_NORMAL;
    int len = 0;
    char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
    struct mca_event_notify_data event_data;

    switch (chip->adapter_type) {
    case ADAPTER_SDP:
    case ADAPTER_CDP:
    case ADAPTER_DCP:
    case ADAPTER_QC2:
    case ADAPTER_QC3:
    case ADAPTER_PD:
    case ADAPTER_AUTH_FAILED:
        qc_type = QUICK_CHARGE_NORMAL;
        break;
    case ADAPTER_XIAOMI_QC3:
    case ADAPTER_XIAOMI_PD:
    case ADAPTER_ZIMI_CAR_POWER:
        qc_type = QUICK_CHARGE_FLASH;
        break;
    case ADAPTER_XIAOMI_PD_40W:
    case ADAPTER_VOICE_BOX:
    case ADAPTER_XIAOMI_PD_50W:
    case ADAPTER_XIAOMI_PD_60W:
    case ADAPTER_XIAOMI_PD_100W:
        qc_type = QUICK_CHARGE_SUPER;
        break;
    default:
        break;
    }

    if (chip->qc_type != qc_type) {
        chip->qc_type = qc_type;
        len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_QUICK_CHARGE_TYPE=%d", chip->qc_type);
        event_data.event = event;
        event_data.event_len = len;
        mca_event_report_uevent(&event_data);
    }

    return;
}

static void nu1671_pre_authen_process(struct nuvolta_1671_chg *chip)
{
	u8 read_buf[RX_AUTH_DATA_LENGTH];
    int ret = 0;
    uint8_t adapter_type = ADAPTER_NONE;
    int len = 0;
    char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
    struct mca_event_notify_data event_data = { 0 };

    if (!chip->power_good_flag)
        return;

    ret = rx1671_read(chip, &read_buf[0], (0x2320+0xa3));
	if (ret < 0)
		return;
	adapter_type = read_buf[0];

    if (adapter_type) {
        mca_log_err("pre authen get adapter type:%d\n", adapter_type);
        chip->adapter_type_first = adapter_type;
        chip->adapter_type = adapter_type;
    } else {
        mca_log_err("pre authen use default adapter type\n");
        chip->adapter_type = ADAPTER_SDP;
    }

    len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_TX_ADAPTER=%d", chip->adapter_type);
    event_data.event = event;
    event_data.event_len = len;
    mca_event_report_uevent(&event_data);

    if (chip->adapter_type == ADAPTER_SDP)
        return;

    nu1671_update_qucikchg_type(chip);
}

static void nuvolta_1671_chg_handler(struct nuvolta_1671_chg *chip, u16 int_flag)
{
	int len = 0;
	char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
	struct mca_event_notify_data event_data = { 0 };
	u8 auth_status = 0, err_code;
	u8 rcv_value[128] = {0};
	u8 val_length = 0, l_len = 0;
	int tx_q2 = 0;
	int tx_speed = WLS_TX_FAN_SPEED_NORMAL;
	mca_log_info("enter\n");
	switch (int_flag) {
	case RX_INT_POWER_ON:
		chip->epp = nuvolta_1671_get_rx_power_mode(chip);
		mca_log_info("RX_INT_POWER_ON epp: %d\n", chip->epp);
		break;
	case RX_INT_LDO_ON:
		mca_log_info("RX_INT_LDO_ON!\n");
		chip->epp = nuvolta_1671_get_rx_power_mode(chip);
		chip->wls_fw_data->hw_id_h = 0x16;
		chip->wls_fw_data->hw_id_l = 0x19;
		/* enable xmusb350 apsd */
		nuvolta_1671_enable_bc12(chip, true);
		nuvolta_1671_enable_power_path(chip, true);
		nu1671_pre_authen_process(chip);
		if (chip->epp)
			nuvolta_1671_stepper_pmic_icl(chip, 250, 850, 100, 20);
		else
			nuvolta_1671_stepper_pmic_icl(chip, 250, 750, 100, 20);
		if(chip->epp)
			nuvolta_1671_set_pmic_ichg(chip, 3000);
		else
			nuvolta_1671_set_pmic_ichg(chip, 1500);
		break;
	case RX_INT_AUTHEN_FINISH:
		nuvolta_1671_get_auth_value(chip, &auth_status);
		mca_log_info("RX_INT_AUTHEN_FINISH! auth data:%d\n", auth_status);
		if (auth_status != AUTH_STATUS_FAILED) {
			if (auth_status >= AUTH_STATUS_UUID_OK)
				nuvolta_1671_set_fod_params(chip);
			if (chip->epp)
				nuvolta_1671_epp_uuid_func(chip);
			if (chip->adapter_type_first != chip->adapter_type) {
					len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_TX_ADAPTER=%d", chip->adapter_type);
					event_data.event = event;
					event_data.event_len = len;
					mca_event_report_uevent(&event_data);
					nu1671_update_qucikchg_type(chip);
			}
			if (chip->adapter_type >= ADAPTER_XIAOMI_PD_50W){
	#ifndef CONFIG_FACTORY_BUILD
				if(chip->q_value_supprot) {
					if (chip->low_inductance_50w_tx)
						nuvolta_1671_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_QVALUE, chip->tx_q1[ADAPTER_LOW_INDUCTANCE_TX_50W]);
					else if (chip->low_inductance_80w_tx)
						nuvolta_1671_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_QVALUE, chip->tx_q1[ADAPTER_LOW_INDUCTANCE_TX_80W]);
					usleep_range(100*1000, 150*1000);
				}
	#endif
				schedule_delayed_work(&chip->renegociation_work, msecs_to_jiffies(1500));
			}else if(chip->adapter_type >= ADAPTER_XIAOMI_QC3){
				nuvolta_1671_adapter_handle(chip);
				nuvolta_1671_set_fastchg_adapter_v(chip);
			}else{
				nuvolta_1671_adapter_handle(chip);
				nuvolta_1671_set_fastchg_adapter_v(chip);
			}
		} else {
			mca_log_info("authen failed!\n");
			nuvolta_1671_adapter_handle(chip);
		}
		break;
	case RX_INT_RENEGO_DONE:
		mca_log_info("RX_INT_RENEGO_DONE!\n");
		cancel_delayed_work_sync(&chip->renegociation_work);
		nuvolta_1671_set_fastchg_adapter_v(chip);
		break;
	case RX_INT_FAST_CHARGE:
		mca_log_info("RX_INT_FAST_CHARGE!\n");
		if(chip->adapter_type >= ADAPTER_XIAOMI_QC3){
			if (chip->is_support_fan_tx) {
				tx_speed = (chip->quiet_sts)? WLS_TX_FAN_SPEED_QUIET : WLS_TX_FAN_SPEED_NORMAL;
				nuvolta_1671_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_FAN_SPEED, tx_speed);
			}
		}
		nuvolta_1671_get_fastchg_result(chip, &chip->fc_flag);
		if (chip->fc_flag) {
			if (chip->adapter_type < ADAPTER_XIAOMI_PD_50W || chip->is_car_tx) {
                nuvolta_1671_adapter_handle(chip);
                nuvolta_1671_set_vout(chip, 8000);
            } else {
                nuvolta_1671_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_FREQUENCE, SUPER_TX_FREQUENCY_DEFAULT_KHZ);
				nuvolta_1671_adapter_handle(chip);
            }
		} else if (chip->set_fastcharge_vout_cnt++ < 3) {
			mca_log_info("set fastchg vol failed, retry %d\n",
				chip->set_fastcharge_vout_cnt);
			msleep(2000);
			nuvolta_1671_set_fastchg_adapter_v(chip);
		} else {
			mca_log_info("set fastchg vol failed finally\n");
			nuvolta_1671_adapter_handle(chip);
		}
		break;
	case RX_INT_OCP_OTP_ALARM:
		mca_log_info("OCP OR OTP trigger\n");
#ifndef CONFIG_FACTORY_BUILD
		schedule_delayed_work(&chip->rx_alarm_work,
					msecs_to_jiffies(500));
#endif
		break;
	case RX_INT_POWER_OFF:
		mca_log_info("POWER OFF INT trigger\n");
		nuvolta_1671_power_off_err(chip, &err_code);
		if (chip->wls_wakelock->active)
			__pm_relax(chip->wls_wakelock);
		break;
	case RX_INT_TRANSPARENT_SUCCESS:
		nuvolta_1671_rcv_transparent_data(chip, rcv_value, ARRAY_SIZE(rcv_value), &val_length);
        l_len = val_length & 0x0F;
        mca_log_err("RX_INT_TRANSPARENT_SUCCESS, curr_cmd=%d, val_len=%d %d\n",
                chip->current_trans_packet_type, val_length, l_len);
        if (l_len == 1) {
            if (rcv_value[0] == 0x28) {
                if ((rcv_value[3] & 0x3C) == 0x3C) {
                    chip->set_tx_voltage_cnt = 0;
                    if (chip->current_trans_packet_type == WLS_FREQUENCE_PACKET) {
                        if (chip->is_car_tx || chip->is_sailboat_tx)
                            nuvolta_1671_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_VOUT_RANGE, SUPER_TX_VOUT_MIN_MV);
                        else
                            nuvolta_1671_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_VOUT_RANGE, SUPER_TX_VOUT_PLAN_A_MV);
                    } else if (chip->current_trans_packet_type == WLS_VOUT_RANGE_PACKET) {
                        chip->current_trans_packet_type = UNKNOWN_PACKET;
                        chip->is_vout_range_set_done = true;
                        nuvolta_1671_adapter_handle(chip);
                    }
                } else {
                    if (chip->set_tx_voltage_cnt++ < 3) {
                        if (chip->current_trans_packet_type == WLS_FREQUENCE_PACKET) {
                            nuvolta_1671_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_FREQUENCE, SUPER_TX_FREQUENCY_DEFAULT_KHZ);
                        } else if (chip->current_trans_packet_type == WLS_VOUT_RANGE_PACKET) {
                            if (chip->adapter_type > ADAPTER_XIAOMI_PD_50W)
                                nuvolta_1671_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_VOUT_RANGE, SUPER_TX_VOUT_PLAN_B_MV);
                            else
                                nuvolta_1671_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_VOUT_RANGE, SUPER_TX_VOUT_PLAN_A_MV);
                        }
                    } else {
                        mca_log_err("set tx voltage failed finally\n");
                        chip->current_trans_packet_type = UNKNOWN_PACKET;
                        chip->is_vout_range_set_done = true;
                        chip->force_cp_2_1_mode = true;
                        nuvolta_1671_set_vout(chip, 8000);
                        nuvolta_1671_adapter_handle(chip);
                    }
                }
            } else if (rcv_value[0] == 0x05) {
                nuvolta_1671_process_power_reduce_cmd(chip, rcv_value[2]);
                chip->current_trans_packet_type = UNKNOWN_PACKET;
            }
        } else if(chip->current_trans_packet_type == WLS_Q_STARTEGY_PACKET || l_len == 4) {
            if (chip->low_inductance_50w_tx)
                tx_q2 = chip->tx_q2[ADAPTER_LOW_INDUCTANCE_TX_50W];
            else if (chip->low_inductance_80w_tx)
                tx_q2 = chip->tx_q2[ADAPTER_LOW_INDUCTANCE_TX_80W];
            else
                tx_q2 = rcv_value[2];
            mca_log_info("RX_INT_TRANSPARENT_SUCCESS tx_q=%d, tx_q2=%d\n", rcv_value[5], tx_q2);
            nuvolta_1671_process_q_value_strategy(chip, rcv_value[5] <= tx_q2);
            chip->current_trans_packet_type = UNKNOWN_PACKET;
        }
		break;
	case RX_INT_TRANSPARENT_FAIL:
		 mca_log_err("RX_INT_TRANSPARENT_FAIL!\n");
        nuvolta_1671_rcv_transparent_data(chip, rcv_value, ARRAY_SIZE(rcv_value), &val_length);
        if (rcv_value[0] == 0x28 && chip->current_trans_packet_type != UNKNOWN_PACKET) {
            if (chip->set_tx_voltage_cnt++ < 3) {
                if (chip->current_trans_packet_type == WLS_FREQUENCE_PACKET) {
                    nuvolta_1671_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_FREQUENCE, SUPER_TX_FREQUENCY_DEFAULT_KHZ);
                } else if (chip->current_trans_packet_type == WLS_VOUT_RANGE_PACKET) {
                    if (chip->adapter_type > ADAPTER_XIAOMI_PD_50W)
                        nuvolta_1671_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_VOUT_RANGE, SUPER_TX_VOUT_PLAN_B_MV);
                    else
                        nuvolta_1671_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_VOUT_RANGE, SUPER_TX_VOUT_PLAN_A_MV);
                }
            } else {
                chip->current_trans_packet_type = UNKNOWN_PACKET;
                chip->is_vout_range_set_done = true;
                chip->force_cp_2_1_mode = true;
                nuvolta_1671_set_vout(chip, 8000);
                nuvolta_1671_adapter_handle(chip);
            }
        }
		break;
	case RX_INT_FACTORY_TEST:
		mca_log_info("factory test\n");
		nuvolta_1671_rcv_factory_test_cmd(chip, rcv_value, &val_length);
		mca_log_info("factory test: 0x%x, 0x%x, 0x%x\n",
			rcv_value[0], rcv_value[1],rcv_value[2]);
		if (rcv_value[0] == FACTORY_TEST_CMD)
			nuvolta_1671_process_factory_cmd(chip, rcv_value[1]);
		break;
	default:
		break;
	}
	return;
}

static void nuvolta_1671_wireless_int_work(struct work_struct *work)
{
	u16 int_flag = 0;
	u8 int_l = 0, int_h = 0;
	u8 int_trx_mode = RX_MODE;
	int ret = 0;
	struct nuvolta_1671_chg *chip = container_of(work,
						struct nuvolta_1671_chg, wireless_int_work.work);
	mutex_lock(&chip->wireless_chg_int_lock);
	ret = rx1671_read(chip, &int_l, REG_RX_REV_CMD); //0x0020
	if (ret < 0) {
		mca_log_err("read int 0x20 error\n");
		goto exit;
	}
	ret = rx1671_read(chip, &int_h, REG_RX_REV_DATA1); //0x0021
	if (ret < 0) {
		mca_log_err("read int 0x21 error\n");
		goto exit;
	}
	int_flag = (int_h << 8) | int_l;
	nuvolta_1671_get_rx_rtx_mode(chip, &int_trx_mode);
	mca_log_info("int_flag: 0x%x ic work on %s mode\n", int_flag,
						int_trx_mode == RTX_MODE ? "rtx" : "rx");

	if (int_trx_mode == RTX_MODE) {
		nuvolta_1671_reverse_chg_handler(chip, int_flag);
	} else {
		nuvolta_1671_chg_handler(chip, int_flag);
	}
	nuvolta_1671_clear_int(chip);
exit:
	mutex_unlock(&chip->wireless_chg_int_lock);
	return;
}

static irqreturn_t nuvolta_1671_interrupt_handler(int irq, void *dev_id)
{
	struct nuvolta_1671_chg *chip = dev_id;
	schedule_delayed_work(&chip->wireless_int_work, 0);
	return IRQ_HANDLED;
}

static void nuvolta_1671_reset_parameters(struct nuvolta_1671_chg *chip)
{
	mca_log_info("enter\n");
	chip->power_good_flag = 0;
	chip->ss = 2;
	chip->epp = 0;
	chip->qc_enable = false;
	chip->chg_phase = NORMAL_MODE;
	chip->adapter_type = 0;
	chip->fc_flag = 0;
	chip->set_fastcharge_vout_cnt = 0;
	chip->is_car_tx = false;
	chip->is_music_tx = false;
	chip->is_plate_tx = false;
	chip->is_train_tx = false;
	chip->is_standard_tx = false;
	chip->parallel_charge = false;
	chip->reverse_chg_en = false;
	chip->alarm_flag = false;
	chip->i2c_ok_flag = false;
	chip->set_tx_voltage_cnt = 0;
	chip->is_sailboat_tx = false;
	chip->is_vout_range_set_done = false;
	chip->force_cp_2_1_mode = false;
	chip->low_inductance_50w_tx = false;
	chip->low_inductance_80w_tx = false;
	chip->mutex_lock_sts = false;
	chip->is_support_fan_tx = false;
	chip->adapter_type_first = 0;
	chip->qc_type = QUICK_CHARGE_NORMAL;
	return;
}

static u8 nuvolta_1671_get_image_fw_version(struct nuvolta_1671_chg *chip)
{
	unsigned int  fw_size = chip->fw_data_size;
	if(fw_size >= 9)
		return ((~chip->fw_data_ptr[0x7ff7]) & 0xFF);
	else
		return 0;
}

static int nuvolta_1671_check_rx_fw_version(struct nuvolta_1671_chg *chip)
{
	int ret = 0;
	int check_result = BOOT_CHECK_SUCCESS | RX_CHECK_SUCCESS | TX_CHECK_SUCCESS;
	u8 image_fw_version = CRC_CHECK_ERR_VER;
	u8 read_buf[FW_VERSION_BUF_LENGTH] = {0};
	int i = 0;

	if (chip->power_good_flag) {
		mca_log_err("exit when wls chg\n");
		return check_result;
	}
	image_fw_version = nuvolta_1671_get_image_fw_version(chip);
	ret = rx1671_write(chip, RX_CMD_ENABLE_CRC, REG_RX_INT_3);
	if (ret < 0) {
		mca_log_err("fail to write : 0x%x\n", REG_RX_INT_3);
		return check_result;
	}
	msleep(100);
	ret = rx1671_read_buffer(chip, read_buf, REG_RX_REV_DATA8, FW_VERSION_BUF_LENGTH);
	if (ret < 0) {
		mca_log_err("fail to read : 0x%x\n", 0x0028);
		return check_result;
	}
	for(i = 0; i < FW_VERSION_BUF_LENGTH; i++){
		mca_log_err("read_data[%d] = %x\n", i, read_buf[i]);
	}
	mca_log_err("fw crc check: 0x%x, 0x%x\n", read_buf[0], read_buf[4]);
	if (read_buf[0] == CRC_CHECK_SUCCESS && read_buf[4] >= image_fw_version && read_buf[4] != 0xfe) {
		chip->wls_fw_data->fw_version = read_buf[4];
		check_result = BOOT_CHECK_SUCCESS | RX_CHECK_SUCCESS | TX_CHECK_SUCCESS;
	} else {
		chip->wls_fw_data->fw_version = read_buf[4];
		check_result = 0;
	}
	mca_log_err("image fw version: 0x%x\n", image_fw_version);
	mca_log_err("ic fw version: 0x%x, check_result: 0x%x\n",
			chip->wls_fw_data->fw_version, check_result);

	return check_result;
}

static u8 nu1671_no_charging_get_fw_version(struct nuvolta_1671_chg *chip)
{
	u8 fw_update_status = 7;

	if (chip->power_good_flag)
		return fw_update_status;

	if (chip->fw_update) {
		mca_log_err("fw update going, can not get fw version\n");
		return fw_update_status;
	}

	chip->fw_update = true;
	nu1671_enable_reverse_boost(chip, BOOST_FOR_FWUPDATE, true);
	msleep(100);
	fw_update_status = nuvolta_1671_check_rx_fw_version(chip);
	nu1671_enable_reverse_boost(chip, BOOST_FOR_FWUPDATE, false);
	chip->fw_update = false;
	mca_log_err("boot = 0x%x, rx = 0x%x, tx = 0x%x\n",
			g_chip->wls_fw_data->fw_version, g_chip->wls_fw_data->fw_version, g_chip->wls_fw_data->fw_version);
	return fw_update_status;
}

static void nuvolta_1671_pg_det_work(struct work_struct *work)
{
	struct nuvolta_1671_chg *chip = container_of(work, struct nuvolta_1671_chg, wireless_pg_det_work.work);
	int ret = 0, wls_switch_usb = 0;
	int len = 0;
	char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
	struct mca_event_notify_data event_data = { 0 };
	static int pg_low_cnt = 0;
	mca_log_err("enter\n");
	if (!chip->wireless_psy) {
		chip->wireless_psy = power_supply_get_by_name("wireless");
		if (!chip->wireless_psy)
			mca_log_err("failed to get wireless psy\n");
	}
	if (gpio_is_valid(chip->power_good_gpio)) {
		ret = gpio_get_value(chip->power_good_gpio);
		mca_log_err("power_good_gpio ret=%d\n", ret);
		if (ret) {
			mca_log_info("power_good high, wireless attached\n");
			pg_low_cnt = 0;
			chip->pg_low_debounce = false;
			if (!chip->wls_wakelock->active)
				__pm_stay_awake(chip->wls_wakelock);
			chip->power_good_flag = 1;
			chip->adapter_type = ADAPTER_SDP;
			chip->current_for_adapter_cmd = UNKNOWN;
			if (chip->power_good_flag && !chip->is_reverse_closing && chip->reverse_chg_en) {
				mca_log_info("close wls rev chg\n");
				nuvolta_1671_set_reverse_chg_mode(chip, false);
				chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
				len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_REVERSE_CHG_STATE=%d", chip->is_reverse_chg);
				event_data.event = event;
				event_data.event_len = len;
				mca_event_report_uevent(&event_data);
			}
			nuvolta_1671_set_pmic_icl(chip, 0);
			if (chip->icl_votable) {
				vote(chip->icl_votable, WLS_PARACHG_VOTER, false, 0);
				vote(chip->icl_votable, ICL_VOTER, false, 0);
			}
			cancel_delayed_work_sync(&chip->renegociation_work);
			schedule_delayed_work(&chip->i2c_check_work, msecs_to_jiffies(500));
			schedule_delayed_work(&chip->trans_data_work, msecs_to_jiffies(0));
		} else {
			mca_log_info("power_good low, wireless detached\n");
			if (pg_low_cnt++ < 1) {
				mca_log_err("power_good become low, start debounce.\n");
				chip->pg_low_debounce = true;
				if (chip->wireless_psy)
					power_supply_changed(chip->wireless_psy);
				cancel_delayed_work(&chip->chg_monitor_work);
				schedule_delayed_work(&chip->wireless_pg_det_work, msecs_to_jiffies(2500));
				return;
			}
			pg_low_cnt = 0;
			chip->pg_low_debounce = false;
			chip->current_trans_packet_type = UNKNOWN_PACKET;
			nuvolta_1671_reset_parameters(chip);
			wake_up_interruptible(&chip->wait_que);
			cancel_delayed_work(&chip->chg_monitor_work);
			cancel_delayed_work(&chip->rx_alarm_work);
			cancel_delayed_work(&chip->i2c_check_work);
			cancel_delayed_work_sync(&chip->renegociation_work);
			cancel_delayed_work_sync(&chip->trans_data_work);
			nuvolta_1671_set_pmic_icl(chip, 0);
			if (chip->icl_votable){
				vote(chip->icl_votable, WLS_PARACHG_VOTER, false, 0);
				vote(chip->icl_votable, WLS_CHG_VOTER, false, 0);
			}
			if (chip->fcc_votable){
				vote(chip->fcc_votable, WLS_POWER_REDUCE_VOTER, false, 0);
				vote(chip->fcc_votable, WLS_Q_VALUE_STRATEGY_VOTER, false, 0);
				vote(chip->fcc_votable, WLS_CHG_VOTER, false, 0);
			}

			len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_TX_ADAPTER=%d", 0);
			event_data.event = event;
			event_data.event_len = len;
			mca_event_report_uevent(&event_data);
			len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_QUICK_CHARGE_TYPE=%d", 0);
			event_data.event = event;
			event_data.event_len = len;
			mca_event_report_uevent(&event_data);
			len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_WLS_CAR_ADAPTER=%d", 0);
			event_data.event = event;
			event_data.event_len = len;
			mca_event_report_uevent(&event_data);

			wls_get_property(WLS_PROP_SWITCH_USB, &wls_switch_usb);
			mca_log_info("wireless switch to usb: %d\n", wls_switch_usb);
			if (!wls_switch_usb) {
				nuvolta_1671_enable_bc12(chip, false);
				nuvolta_1671_enable_power_path(chip, true);
			}
			if (chip->revchg_test_status == REVERSE_TEST_SCHEDULE) {
				mca_log_info("factory reverse charge start\n");
				schedule_delayed_work(&chip->factory_reverse_start_work, msecs_to_jiffies(2000));
			}
			if (chip->wls_wakelock->active)
				__pm_relax(chip->wls_wakelock);
		}
		if (chip->wireless_psy)
			power_supply_changed(chip->wireless_psy);
	}
}

static void nuvolta_1671_init_detect_work(struct work_struct *work)
{
	struct nuvolta_1671_chg *chip =
		container_of(work, struct nuvolta_1671_chg,
		init_detect_work.work);
	int ret = 0;
	if(chip == NULL){
		mca_log_err("chip poniter is null\n");
		return;
	}
	if (gpio_is_valid(chip->power_good_gpio)) {
		ret = gpio_get_value(chip->power_good_gpio);
		mca_log_info("init power good: %d\n", ret);
		if (ret) {
			nuvolta_1671_set_enable_mode(chip, false);
			usleep_range(20000, 25000);
			nuvolta_1671_set_enable_mode(chip, true);
		}
	}
	return;
}

static void nu1671_init_fw_check_work(struct work_struct *work)
{
	struct nuvolta_1671_chg *chip = container_of(work, struct nuvolta_1671_chg,
				init_fw_check_work.work);
	int ret = 0;
	int otg_enable = 0, vusb_insert = 0;

	if (chip->power_good_flag || chip->reverse_chg_en || chip->fw_update) {
		mca_log_err("wls chg or wls revchg or fw updating, no need to check fw\n");
		return;
	}

	usb_get_property(USB_PROP_OTG_ENABLE, &otg_enable);
	wls_get_property(WLS_PROP_VUSB_INSERT, &vusb_insert);
	if (otg_enable || vusb_insert) {
		mca_log_err("wirechg or otg online, no need to check fw\n");
		return;
	}
	if (nu1671_no_charging_get_fw_version(chip) != (BOOT_CHECK_SUCCESS | RX_CHECK_SUCCESS | TX_CHECK_SUCCESS)) {
		ret = nuvolta_1671_firmware_update_func(chip, FW_UPDATE_FORCE);
		if (ret < 0) {
			mca_log_err("fw update failed\n");
		} else {
			mca_log_err("fw update success\n");
		}
	}
    return;
}

static void nuvolta_1671_hall_interrupt_work(struct work_struct *work)
{
	struct nuvolta_1671_chg *chip = container_of(work,
		struct nuvolta_1671_chg, hall_interrupt_work.work);
	bool new_case_status = false;

	if (gpio_is_valid(chip->hall_n_int_gpio))
		chip->hall_n_gpio_status = !!gpio_get_value(chip->hall_n_int_gpio);
	if (gpio_is_valid(chip->hall_s_int_gpio))
		chip->hall_s_gpio_status = !!gpio_get_value(chip->hall_s_int_gpio);
	mca_log_err("hall interrupt, n: %d, s: %d\n",
		chip->hall_n_gpio_status, chip->hall_s_gpio_status);

	if (!chip->hall_s_gpio_status || !chip->hall_n_gpio_status)
		new_case_status = true;
	if (chip->magnetic_case_flag == new_case_status) {
		mca_log_err("magnetic case already %s\n", new_case_status ? "attached" : "detached");
		return;
	}

	chip->magnetic_case_flag = new_case_status;
	mca_log_err("magnetic case %s\n", new_case_status ? "attached" : "detached");
	// mca_strategy_func_process(STRATEGY_FUNC_TYPE_BASIC_WIRELESS,
	// 	MCA_EVENT_WIRELESS_MAGNETIC_CASE_STATUS, chip->magnetic_case_flag); TODO
}

static void nuvolta_1671_get_charge_phase(struct nuvolta_1671_chg *chip, int *chg_phase)
{
	switch (*chg_phase) {
	case NORMAL_MODE:
		if (chip->batt_soc == 100) {
			*chg_phase = TAPER_MODE;
			mca_log_info("change normal mode to tapter mode");
		}
		break;
	case TAPER_MODE:
		if ((chip->batt_soc == 100) && (chip->chg_status == POWER_SUPPLY_STATUS_FULL)) {
			*chg_phase = FULL_MODE;
			mca_log_info("change taper mode to full mode");
		} else if (chip->batt_soc < 99) {
			*chg_phase = NORMAL_MODE;
			mca_log_info("change taper mode to normal mode");
		}
		break;
	case FULL_MODE:
		if (chip->chg_status == POWER_SUPPLY_STATUS_CHARGING) {
			*chg_phase = RECHG_MODE;
			mca_log_info("change full mode to recharge mode");
		}
		break;
	case RECHG_MODE:
		if (chip->chg_status == POWER_SUPPLY_STATUS_FULL) {
			*chg_phase = FULL_MODE;
			mca_log_info("change recharge mode to full mode");
		}
		break;
	default:
		break;
	}
	return;
}

static void nuvolta_1671_get_adapter_current(struct nuvolta_1671_chg *chip, u8 adapter)
{
	int input_suspend = 0;

	switch (adapter) {
	case ADAPTER_QC2:
		chip->target_vol = BPP_QC2_VOUT;
		chip->target_curr = (chip->chg_phase == FULL_MODE)? 200 : ((chip->batt_soc >= 99)? 350 : ((chip->batt_soc >= 95)? 500 : 750));
		break;
	case ADAPTER_QC3:
	case ADAPTER_PD:
		if (chip->epp) {
			chip->target_vol = EPP_DEFAULT_VOUT;
			chip->target_curr = (chip->chg_phase == FULL_MODE)? 250 : ((chip->chg_phase == RECHG_MODE)? 550 : 850);
		} else if (chip->fc_flag) {
			chip->target_vol = (chip->batt_soc >= 95)? BPP_DEFAULT_VOUT: BPP_PLUS_VOUT;
			chip->target_curr = (chip->chg_phase == FULL_MODE)? 250 : ((chip->batt_soc >= 95)? 750 : 1100);
		} else {
			chip->target_vol = BPP_DEFAULT_VOUT;
			chip->target_curr = (chip->chg_phase == FULL_MODE)? 250 : 750;
		}
		break;
	case ADAPTER_AUTH_FAILED:
		if (chip->epp) {
			chip->target_vol = EPP_DEFAULT_VOUT;
			chip->target_curr = (chip->chg_phase == FULL_MODE)? 250 : ((chip->chg_phase == RECHG_MODE)? 550 : 850);
		} else {
			chip->target_vol = BPP_DEFAULT_VOUT;
			chip->target_curr = (chip->batt_soc >= 99)? 350  : 750;
		}
		break;
	case ADAPTER_XIAOMI_QC3:
	case ADAPTER_XIAOMI_PD:
	case ADAPTER_ZIMI_CAR_POWER:
	case ADAPTER_VOICE_BOX:
	case ADAPTER_XIAOMI_PD_40W:
	case ADAPTER_XIAOMI_PD_50W:
	case ADAPTER_XIAOMI_PD_60W:
	case ADAPTER_XIAOMI_PD_100W:
		if (chip->fc_flag) {
			chip->target_vol = (chip->chg_phase == FULL_MODE)? BPP_DEFAULT_VOUT : EPP_DEFAULT_VOUT;
			chip->target_curr = (chip->chg_phase == FULL_MODE)? 300 : ((chip->chg_phase == RECHG_MODE)? 550 : 1300);
		} else {
			chip->target_vol = (chip->chg_phase == FULL_MODE)? BPP_DEFAULT_VOUT : EPP_DEFAULT_VOUT;
			chip->target_curr = (chip->chg_phase == FULL_MODE)? 200 : ((chip->chg_phase == RECHG_MODE)? 550 : 850);
		}
		break;
	default:
		if (chip->epp) {
			chip->target_vol = EPP_DEFAULT_VOUT;
			chip->target_curr = 850;
		} else {
			chip->target_vol = BPP_DEFAULT_VOUT;
			chip->target_curr = 750;
		}
		break;
	}

    usb_get_property(USB_PROP_OTG_ENABLE, &input_suspend);
	if (input_suspend)
		chip->target_vol = BPP_DEFAULT_VOUT;

	mca_log_err("adapter:%d, target_vout:%d, target_icl:%d, input_suspend:%d", adapter, chip->target_vol, chip->target_curr, input_suspend);
}

static void nuvolta_1671_get_charging_info(struct nuvolta_1671_chg *chip)
{
	int vout, iout, vrect, raw_soc;
	union power_supply_propval val = {0,};
	int ret = 0;
	if (!chip)
		return;
	if (!chip->batt_psy)
		chip->batt_psy = power_supply_get_by_name("battery");
	if (!chip->batt_psy)
		mca_log_err("failed to get batt_psy\n");
	else {
		power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
		if(chip->batt_soc < val.intval){
			mca_log_err("update soc to tx soc=%d\n", val.intval);
			nuvolta_1671_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_SOC, val.intval);
		}
		chip->batt_soc = val.intval;
		power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_TEMP, &val);
        chip->batt_temp = val.intval;
		power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_STATUS, &val);
		chip->chg_status = val.intval;
		nuvolta_1671_get_charge_phase(chip, &chip->chg_phase);
	}
	ret = nuvolta_1671_get_iout(chip, &iout);
	if (ret < 0 ) {
		mca_log_err("get iout failed\n");
		iout = 0;
	}
	ret = nuvolta_1671_get_vout(chip, &vout);
	if (ret < 0 ) {
		mca_log_err("get vout failed\n");
		vout = 0;
	}
	ret = nuvolta_1671_get_vrect(chip, &vrect);
	if (ret < 0 ) {
		mca_log_err("get vrect failed\n");
		vrect = 0;
	}
	bms_get_property(BMS_PROP_CAPACITY_RAW, &raw_soc);
	chip->raw_soc = raw_soc;

	mca_log_info("Vout:%d, Iout:%d, Vrect:%d, soc: %d, status: %d, temp:%d, chg_phase: %d\n",
				vout, iout, vrect, chip->batt_soc, chip->chg_status, chip->batt_temp, chip->chg_phase);
}

static void nuvolta_1671_charging_loop(struct nuvolta_1671_chg *chip)
{
	int fcc = 0;
	nuvolta_1671_get_adapter_current(chip, chip->adapter_type);
	if (chip->is_plate_tx || chip->is_train_tx) {
		chip->target_vol = (chip->chg_phase == FULL_MODE || chip->chg_phase == RECHG_MODE)? EPP_DEFAULT_VOUT : chip->target_vol;
		chip->target_curr = (chip->chg_phase == FULL_MODE)? 800 : ((chip->chg_phase == RECHG_MODE)? 1000 : chip->target_curr);
	} else if (chip->is_music_tx) {
		chip->target_vol = (chip->chg_phase == FULL_MODE || chip->chg_phase == RECHG_MODE || chip->batt_temp >= 390)? EPP_DEFAULT_VOUT : chip->target_vol;
		chip->target_curr = (chip->chg_phase == FULL_MODE)? 800 : ((chip->chg_phase == RECHG_MODE)? 1000 : chip->target_curr);
		chip->fc_flag = (chip->batt_temp >= 39)? 0 : chip->fc_flag;
		fcc = (chip->batt_temp < 360)? 6000 : (((chip->batt_temp <= 430)? (2500 - (chip->batt_temp - 360) * 20): 1000));
		mca_log_err("set misic tx fcc:%d\n", fcc);
		nuvolta_1671_set_pmic_fcc(chip, fcc);
	}

	if (chip->target_vol != chip->pre_vol && !chip->parallel_charge) {
		mca_log_err("set new vout:%d, pre vout:%d\n", chip->target_vol, chip->pre_vol);
		nuvolta_1671_set_vout(chip, chip->target_vol);
		chip->pre_vol = chip->target_vol;
	}
	if (chip->target_curr != chip->pre_curr && !chip->parallel_charge) {
		mca_log_err("set new icl:%d, pre icl:%d\n", chip->target_curr, chip->pre_curr);
		nuvolta_1671_set_pmic_icl(chip, chip->target_curr);
		chip->pre_curr = chip->target_curr;
	}
}

static void nuvolta_1671_monitor_work(struct work_struct *work)
{
	struct nuvolta_1671_chg *chip =
		 container_of(work, struct nuvolta_1671_chg,
					chg_monitor_work.work);
	nuvolta_1671_get_charging_info(chip);
	nuvolta_1671_charging_loop(chip);
	schedule_delayed_work(&chip->chg_monitor_work,
			msecs_to_jiffies(5000));
}

static void nuvolta_1671_factory_reverse_start_work(struct work_struct *work)
{
	struct nuvolta_1671_chg *chip = container_of(work, struct nuvolta_1671_chg,
					factory_reverse_start_work.work);
	chip->user_reverse_chg = true;
	nuvolta_1671_set_reverse_chg_mode(chip, true);
	chip->revchg_test_status = REVERSE_TEST_PROCESSING;
	mca_log_err("factory reverse test, processing\n");
	schedule_delayed_work(&chip->factory_reverse_stop_work, msecs_to_jiffies(12000));
	return;
}

static void nuvolta_1671_factory_reverse_stop_work(struct work_struct *work)
{
	struct nuvolta_1671_chg *chip = container_of(work, struct nuvolta_1671_chg,
					factory_reverse_stop_work.work);
	int len;
	char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
	struct mca_event_notify_data event_data = { 0 };

	chip->tx_timeout_flag = true;
	nuvolta_1671_set_reverse_chg_mode(chip, false);
	chip->revchg_test_status = REVERSE_TEST_DONE;
	chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
	len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_REVERSE_CHG_STATE=%d", chip->is_reverse_chg);
	event_data.event = event;
	event_data.event_len = len;
	mca_event_report_uevent(&event_data);
	mca_log_err("factory reverse test, stop\n");
	chip->tx_timeout_flag = false;
	return;
}

static int nu1671_enable_reverse_boost(struct nuvolta_1671_chg *chip, int purpose, int enable)
{
    int ret = 0;

    if (chip->power_good_flag && enable) {
        mca_log_err("wls online, cannot enable rev boost\n");
        return -1;
    }

    chip->is_reverse_boosting = true;
    switch (chip->reverse_boost_src) {
    case PMIC_REV_BOOST:
        ret = nu1671_set_reverse_pmic_boost(chip, purpose, enable);
        break;
    case EXTERNAL_BOOST:
        //add if need
        break;
    default:
        ret = -1;
        mca_log_err("reverse boost source is invalid\n");
        break;
    }
    chip->is_reverse_boosting = false;

    return ret;
}

static void nu1671_reverse_chg_config_work(struct work_struct *work)
{
    struct nuvolta_1671_chg *chip = container_of(work, struct nuvolta_1671_chg,
            reverse_chg_config_work.work);
    int ret = 0;
    int real_type = XMUSB350_TYPE_UNKNOW;
    int len;
    char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
    struct mca_event_notify_data event_data = { 0 };

    if (!chip->reverse_chg_en) {
        mca_log_err("revchg has been close\n");
        return;
    }

    ret = usb_get_property(USB_PROP_REAL_TYPE, &real_type);
    if ((ret >= 0) && (real_type == XMUSB350_TYPE_SDP || real_type == XMUSB350_TYPE_CDP))
        chip->bc12_reverse_chg = true;
    else
        chip->bc12_reverse_chg = false;

    if (chip->bc12_reverse_chg) {
        mca_log_err("BC1.2 cannot revchg\n");
        nuvolta_1671_set_reverse_chg_mode(chip, false);
        chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
        len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
        event_data.event = event;
        event_data.event_len = len;
        mca_event_report_uevent(&event_data);
        return;
    }

    ret = nu1671_enable_reverse_boost(chip, BOOST_FOR_REVCHG, true);

    if (!chip->user_reverse_chg) {
        mca_log_err("user close revchg\n");
        nuvolta_1671_set_reverse_chg_mode(chip, false);
        return;
    }

    if (ret) {
        mca_log_err("revchg boost fail\n");
        nuvolta_1671_set_reverse_chg_mode(chip, false);
        chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
        len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
        event_data.event = event;
        event_data.event_len = len;
        mca_event_report_uevent(&event_data);
        return;
    }

    ret = nuvolta_1671_start_tx_function(chip, true);
    mca_log_info("reverse charge done, ret=%d\n", ret);
}

static void nu1671_reverse_chg_monitor_work(struct work_struct *work)
{
    struct nuvolta_1671_chg *chip = container_of(work, struct nuvolta_1671_chg,
            reverse_chg_monitor_work.work);
    int iout = 0, vrect = 0;

    if (!chip->is_reverse_boosting && chip->is_reverse_chg == REVERSE_STATE_TRANSFER) {
        nu1671_get_trx_iout(chip, &iout);
        nu1671_get_trx_vrect(chip, &vrect);
        mca_log_err("wireless revchg: [iout:%d], [vrect:%d]\n", iout, vrect);
        schedule_delayed_work(&chip->reverse_chg_monitor_work, msecs_to_jiffies(5000));
    }
}

static void nu1671_reverse_transfer_timeout_work(struct work_struct *work)
{
    struct nuvolta_1671_chg *chip = container_of(work, struct nuvolta_1671_chg,
            reverse_transfer_timeout_work.work);
    int ret;
    int len;
    char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
    struct mca_event_notify_data event_data = { 0 };

    chip->tx_timeout_flag = true;
    ret = nuvolta_1671_set_reverse_chg_mode(chip, false);
    chip->is_reverse_chg = REVERSE_STATE_TIMEOUT;
    len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_TIMEOUT);
    event_data.event = event;
    event_data.event_len = len;
    mca_event_report_uevent(&event_data);
    mca_log_err("reverse chg transfer timeout");
    chip->tx_timeout_flag = false;
}

static void nu1671_reverse_ping_timeout_work(struct work_struct *work)
{
    struct nuvolta_1671_chg *chip = container_of(work, struct nuvolta_1671_chg,
            reverse_ping_timeout_work.work);
    int ret;
    int len;
    char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
    struct mca_event_notify_data event_data = { 0 };

    chip->tx_timeout_flag = true;
    ret = nuvolta_1671_set_reverse_chg_mode(chip, false);
    chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
    len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
    event_data.event = event;
    event_data.event_len = len;
    mca_event_report_uevent(&event_data);
    mca_log_err("reverse chg ping timeout");
    chip->tx_timeout_flag = false;
}

static void nuvolta_1671_check_rx_alarm(struct nuvolta_1671_chg *chip,
	bool *ocp_flag, bool *otp_flag)
{
	int iout = 0, temp = 0;
	int ret = 0;
	ret = nuvolta_1671_get_iout(chip, &iout);
	if (ret < 0)
		*ocp_flag = false;
	else
		*ocp_flag = (iout >= RX_MAX_IOUT);
	ret = nuvolta_1671_get_temp(chip, &temp);
	if (ret < 0)
		*otp_flag = false;
	else
		*otp_flag = (temp >= RX_MAX_TEMP);
	return;
}

static void nuvolta_1671_rx_alarm_work(struct work_struct *work)
{
	struct nuvolta_1671_chg *chip =
		container_of(work, struct nuvolta_1671_chg,
		rx_alarm_work.work);
	bool ocp_flag = false, otp_flag = false;
	int fcc_setted = 0;
	nuvolta_1671_check_rx_alarm(chip, &ocp_flag, &otp_flag);
	if ((!ocp_flag) && (!otp_flag))
		return;
	fcc_setted = nuvolta_1671_get_fcc(chip);
	if (ocp_flag) {
		mca_log_info("soft ocp, reduce fcc 100mA\n");
		if (fcc_setted - 100 > 0)
			nuvolta_1671_set_pmic_ichg(chip, fcc_setted - 100);
	}
	if (otp_flag) {
		mca_log_info("soft otp, reduce fcc 500mA\n");
		if (fcc_setted - 500 > 0)
			nuvolta_1671_set_pmic_ichg(chip, fcc_setted - 500);
	}
	schedule_delayed_work(&chip->rx_alarm_work,
				msecs_to_jiffies(4000));
	return;
}

static void nuvolta_1671_i2c_check_work(struct work_struct *work)
{
	struct nuvolta_1671_chg *chip =
		container_of(work, struct nuvolta_1671_chg,
		i2c_check_work.work);
	int ret = 0;

	ret = nuvolta_1671_check_i2c(chip);
	if (ret < 0)
		chip->i2c_ok_flag = false;
	else
		chip->i2c_ok_flag = true;
	return;
}

static void nuvolta_wireless_renegociation_work(struct work_struct *work)
{
	struct nuvolta_1671_chg *chip =
		container_of(work, struct nuvolta_1671_chg,
		renegociation_work.work);

	nuvolta_1671_start_renego(chip);
	mca_log_err("start renego work\n");
}

static irqreturn_t nuvolta_1671_power_good_handler(int irq, void *dev_id)
{
	struct nuvolta_1671_chg *chip = dev_id;
	mca_log_err("power_good irq trigger\n");
	if (chip->fw_update)
	{
		mca_log_err("fw_update exit\n");
		return IRQ_HANDLED;
	}
	mca_log_err("enter wireless_pg_det_work\n");
	cancel_delayed_work(&chip->wireless_pg_det_work);
	schedule_delayed_work(&chip->wireless_pg_det_work, msecs_to_jiffies(0));
	return IRQ_HANDLED;
}

static irqreturn_t nuvolta_1671_hall_interrupt_handler(int irq, void *dev_id)
{
	struct nuvolta_1671_chg *chip = dev_id;

	mca_log_err("hall_int detected\n");

	schedule_delayed_work(&chip->hall_interrupt_work, msecs_to_jiffies(0));

	return IRQ_HANDLED;
}

static int nuvolta_1671_parse_fod_subparams(struct device_node *node, const char *name, struct params_t *params)
{
    int i, j;
    int len;
    u8 *idata = NULL;

    if (strcmp(name, "null") == 0) {
        mca_log_err("no need parse params\n");
        return -1;
    }

    len = of_property_count_u8_elems(node, name);
    if (len <= 0 || ((unsigned int)len % PARAMS_T_MAX != 0) || ((unsigned int)len > DEFAULT_FOD_PARAM_LEN * PARAMS_T_MAX)) {
        mca_log_err("parse %s failed\n", name);
        return -1;
    }

    idata = kcalloc(len, sizeof(u8), GFP_KERNEL);
    if (!idata) {
        mca_log_err("malloc failed\n");
        return -1;
    }
    if (of_property_read_u8_array(node, name, idata, len)) {
        mca_log_err("prop %s read fail, array len %d\n", name, len);
        kfree(idata);
        idata = NULL;
        return -1;
    }
    for (i = 0; i < len / 2; i++) {
        j = 2 * i;
        params[i].gain = idata[j];
        params[i].offset = idata[j + 1];
    }

    kfree(idata);
    idata = NULL;
    return 0;
}

static int nuvolta_1671_parse_fod_params(struct device_node *node, struct nuvolta_1671_chg *info)
{
    int array_len, row, col, i;
    const char *tmp_string = NULL;

    array_len = of_property_count_strings(node, "fod_params");
    if (array_len <= 0 || ((unsigned int)array_len % FOD_PARA_MAX != 0) || ((unsigned int)array_len > FOD_PARA_MAX_GROUP * FOD_PARA_MAX)) {
        mca_log_err("parse fod_params failed\n");
        return -1;
    }

    info->fod_params_size = array_len / FOD_PARA_MAX;

    for (i = 0; i < array_len; i++) {
        if (of_property_read_string_index(node, "fod_params", i, &tmp_string))
            return -1;

        row = i / FOD_PARA_MAX;
        col = i % FOD_PARA_MAX;
        switch (col) {
        case FOD_PARA_TYPE:
            if (kstrtou8(tmp_string, 10, &info->fod_params[row].type))
                return -1;
            break;
        case FOD_PARA_LENGTH:
            if (kstrtou8(tmp_string, 10, &info->fod_params[row].length))
                return -1;
            break;
        case FOD_PARA_UUID:
            if (kstrtoint(tmp_string, 16, &info->fod_params[row].uuid))
                return -1;
            break;
        case FOD_PARA_PARAMS:
            if (nuvolta_1671_parse_fod_subparams(node, tmp_string, info->fod_params[row].params))
                return -1;
            break;
        default:
            break;
        }
    }

    return 0;
}

static int nuvolta_1671_parse_fod_params_2_1(struct device_node *node, struct nuvolta_1671_chg *info)
{
    int array_len, row, col, i;
    const char *tmp_string = NULL;

    array_len = of_property_count_strings(node, "fod_params_2_1");
    if (array_len <= 0 || ((unsigned int)array_len % FOD_PARA_MAX != 0) || ((unsigned int)array_len > FOD_PARA_MAX_GROUP * FOD_PARA_MAX)) {
        mca_log_err("parse fod_params failed\n");
        return -1;
    }

    info->fod_params_size_2_1 = array_len / FOD_PARA_MAX;

    for (i = 0; i < array_len; i++) {
        if (of_property_read_string_index(node, "fod_params_2_1", i, &tmp_string))
            return -1;

        row = i / FOD_PARA_MAX;
        col = i % FOD_PARA_MAX;
        switch (col) {
        case FOD_PARA_TYPE:
            if (kstrtou8(tmp_string, 10, &info->fod_params_2_1[row].type))
                return -1;
            break;
        case FOD_PARA_LENGTH:
            if (kstrtou8(tmp_string, 10, &info->fod_params_2_1[row].length))
                return -1;
            break;
        case FOD_PARA_UUID:
            if (kstrtoint(tmp_string, 16, &info->fod_params_2_1[row].uuid))
                return -1;
            break;
        case FOD_PARA_PARAMS:
            if (nuvolta_1671_parse_fod_subparams(node, tmp_string, info->fod_params_2_1[row].params))
                return -1;
            break;
        default:
            break;
        }
    }

    return 0;
}

static int nuvolta_1671_parse_fod_params_default(struct device_node *node, struct nuvolta_1671_chg *info)
{
    int array_len, row, col, i;
    const char *tmp_string = NULL;

    array_len = of_property_count_strings(node, "fod_params_default");
    if (array_len <= 0 || ((unsigned int)array_len % FOD_PARA_MAX != 0) || ((unsigned int)array_len > FOD_PARA_MAX_GROUP * FOD_PARA_MAX)) {
        mca_log_err("parse fod_params_default failed\n");
        return -1;
    }

    for (i = 0; i < array_len; i++) {
        if (of_property_read_string_index(node, "fod_params_default", i, &tmp_string))
            return -1;

        row = i / FOD_PARA_MAX;
        col = i % FOD_PARA_MAX;
        mca_log_err("[%d]fod params default %s\n", i, tmp_string);
        switch (col) {
        case FOD_PARA_TYPE:
            if (kstrtou8(tmp_string, 10, &info->fod_params_default.type))
                return -1;
            break;
        case FOD_PARA_LENGTH:
            if (kstrtou8(tmp_string, 10, &info->fod_params_default.length))
                return -1;
            break;
        case FOD_PARA_UUID:
            if (kstrtoint(tmp_string, 16, &info->fod_params_default.uuid))
                return -1;
            break;
        case FOD_PARA_PARAMS:
            if (nuvolta_1671_parse_fod_subparams(node, tmp_string, info->fod_params_default.params))
                return -1;
            break;
        default:
            break;
        }
    }

    return 0;
}

static int nuvolta_1671_parse_fod_params_bpp_plus(struct device_node *node, struct nuvolta_1671_chg *info)
{
    int array_len, row, col, i;
    const char *tmp_string = NULL;

    array_len = of_property_count_strings(node, "fod_params_bpp_plus");
    if (array_len <= 0 || ((unsigned int)array_len % FOD_PARA_MAX != 0) || ((unsigned int)array_len > FOD_PARA_MAX_GROUP * FOD_PARA_MAX)) {
        mca_log_err("parse fod_params_bpp_plus failed\n");
        return -1;
    }

    for (i = 0; i < array_len; i++) {
        if (of_property_read_string_index(node, "fod_params_bpp_plus", i, &tmp_string))
            return -1;

        row = i / FOD_PARA_MAX;
        col = i % FOD_PARA_MAX;
        mca_log_err("[%d]fod params default %s\n", i, tmp_string);
        switch (col) {
        case FOD_PARA_TYPE:
            if (kstrtou8(tmp_string, 10, &info->fod_params_bpp_plus.type))
                return -1;
            break;
        case FOD_PARA_LENGTH:
            if (kstrtou8(tmp_string, 10, &info->fod_params_bpp_plus.length))
                return -1;
            break;
        case FOD_PARA_UUID:
            if (kstrtoint(tmp_string, 16, &info->fod_params_bpp_plus.uuid))
                return -1;
            break;
        case FOD_PARA_PARAMS:
            if (nuvolta_1671_parse_fod_subparams(node, tmp_string, info->fod_params_bpp_plus.params))
                return -1;
            break;
        default:
            break;
        }
    }

    return 0;
}

static int nuvolta_1671_parse_fw_fod_data(struct nuvolta_1671_chg *chip)
{
	int ret = 0;
	struct device_node *node = NULL;

	chip->fw_version_index = chip->fw_version_index_default;
	node = of_find_node_by_name(NULL, "wls_nuvolta_1671_fod_data");

	chip->fw_data_ptr = fw_data_1671[chip->fw_version_index];
	chip->fw_data_size = sizeof(fw_data_1671[chip->fw_version_index]);
	mca_log_err("fw_version_index %d, fw_data [%02x %02x %02x]\n",
			chip->fw_version_index, chip->fw_data_ptr[0], chip->fw_data_ptr[1], chip->fw_data_ptr[2]);
	ret = nuvolta_1671_parse_fod_params(node, chip);
	ret |= nuvolta_1671_parse_fod_params_2_1(node, chip);
	ret |= nuvolta_1671_parse_fod_params_default(node, chip);
	ret |= nuvolta_1671_parse_fod_params_bpp_plus(node, chip);

	return ret;
}

static int nuvolta_1671_parse_dt(struct nuvolta_1671_chg *chip)
{
	struct device_node *node = chip->dev->of_node;
	u8 idata_u8[ADAPTER_LOW_INDUCTANCE_TX_TYPE_MAX] = { 0 };
	int ret = 0;
	if (!node) {
		mca_log_err("No DT data Failing Probe\n");
		return -EINVAL;
	}

	of_property_read_u32(node, "support-hall", &chip->support_hall);
	if (of_property_read_u32(node, "fw_version_index_default", &chip->fw_version_index_default) < 0)
		chip->fw_version_index_default = 0;
	if (of_property_read_u32(node, "fw_version_index_jp", &chip->fw_version_index_jp) < 0)
		chip->fw_version_index_jp = 0;
	nuvolta_1671_parse_fw_fod_data(chip);
	chip->enable_gpio = of_get_named_gpio(node, "rx_sleep_gpio", 0);
	if ((!gpio_is_valid(chip->enable_gpio))) {
		return -EINVAL;
	} else {
		ret = devm_gpio_request(chip->dev, chip->enable_gpio, "rx_sleep_gpio");
		if (ret) {
			mca_log_err("Failed to request rx_sleep_gpio: %d\n", ret);
		}
	}

	chip->irq_gpio = of_get_named_gpio(node, "rx_irq_gpio", 0);
	if (!gpio_is_valid(chip->irq_gpio)) {
		mca_log_err("fail_irq_gpio %d\n", chip->irq_gpio);
		return -EINVAL;
	} else {
		ret = devm_gpio_request(chip->dev, chip->irq_gpio, "rx_irq_gpio");
		if (ret) {
			mca_log_err("Failed to request rx_irq_gpio: %d\n", ret);
		}
	}

	chip->power_good_gpio = of_get_named_gpio(node, "pwr_det_gpio", 0);
	if (!gpio_is_valid(chip->power_good_gpio)) {
		mca_log_err("fail_power_good_gpio %d\n", chip->power_good_gpio);
		return -EINVAL;
	} else {
		ret = devm_gpio_request(chip->dev, chip->power_good_gpio, "pwr_det_gpio");
		if (ret) {
			mca_log_err("Failed to request pwr_det_gpio: %d\n", ret);
		}
	}

	if (chip->support_hall) {
		chip->hall_n_int_gpio = of_get_named_gpio(node, "hall-n-int", 0);
		if (!gpio_is_valid(chip->hall_n_int_gpio)) {
			mca_log_err("fail_hall-n-int %d\n", chip->hall_n_int_gpio);
			return -EINVAL;
		} else {
			ret = devm_gpio_request(chip->dev, chip->hall_n_int_gpio, "hall-n-int");
			if (ret) {
				mca_log_err("Failed to request all-n-int: %d\n", ret);
			}
		}

		chip->hall_s_int_gpio = of_get_named_gpio(node, "hall-s-int", 0);
		if (!gpio_is_valid(chip->hall_s_int_gpio)) {
			mca_log_err("fail_hall-s-int %d\n", chip->hall_s_int_gpio);
			return -EINVAL;
		} else {
			ret = devm_gpio_request(chip->dev, chip->hall_s_int_gpio, "hall-s-int");
			if (ret) {
				mca_log_err("Failed to request hall-s-int: %d\n", ret);
			}
		}
	}

	of_property_read_u32(node, "reverse_boost_src", &chip->reverse_boost_src);
	chip->reverse_boost_src = (chip->reverse_boost_src >= BOOST_SRC_MAX)? PMIC_REV_BOOST : chip->reverse_boost_src;
	if (chip->reverse_boost_src == EXTERNAL_BOOST) {
		chip->tx_on_gpio = of_get_named_gpio(node, "reverse_chg_ovp_gpio", 0);
		if (!gpio_is_valid(chip->tx_on_gpio)) {
			mca_log_err("fail_tx_on gpio %d\n", chip->tx_on_gpio);
			return -EINVAL;
		}
		chip->reverse_boost_gpio = of_get_named_gpio(node, "reverse_boost_gpio", 0);
		if (!gpio_is_valid(chip->reverse_boost_gpio)) {
			mca_log_err("fail reverse_boost_gpio %d\n", chip->reverse_boost_gpio);
			return -EINVAL;
		}
	}
	of_property_read_u32(node, "revchg_boost_vol", &chip->revchg_boost_vol);
	of_property_read_u32(node, "fwupdate_boost_vol", &chip->fwupdate_boost_vol);

	chip->q_value_supprot = of_property_read_bool(node, "q_value_supprot");
	if (chip->q_value_supprot) {
		ret = of_property_read_u8_array(node, "tx_q1", idata_u8, ADAPTER_LOW_INDUCTANCE_TX_TYPE_MAX);
		 if (ret) {
            chip->tx_q1[ADAPTER_LOW_INDUCTANCE_TX_50W] = WLS_DEFAULT_TX_Q1;
            chip->tx_q1[ADAPTER_LOW_INDUCTANCE_TX_80W] = WLS_DEFAULT_TX_Q1;
        } else {
            memcpy(chip->tx_q1, idata_u8, sizeof(idata_u8));
        }

		ret = of_property_read_u8_array(node, "tx_q2", idata_u8, ADAPTER_LOW_INDUCTANCE_TX_TYPE_MAX);
        if (ret) {
            chip->tx_q2[ADAPTER_LOW_INDUCTANCE_TX_50W] = WLS_DEFAULT_TX_Q2;
            chip->tx_q2[ADAPTER_LOW_INDUCTANCE_TX_80W] = WLS_DEFAULT_TX_Q2;
        } else {
            memcpy(chip->tx_q2, idata_u8, sizeof(idata_u8));
        }

        mca_log_err("tx_q1[0x%02x 0x%02x], tx_q2[0x%02x 0x%02x]\n",
                chip->tx_q1[ADAPTER_LOW_INDUCTANCE_TX_50W], chip->tx_q1[ADAPTER_LOW_INDUCTANCE_TX_80W],
                chip->tx_q2[ADAPTER_LOW_INDUCTANCE_TX_50W], chip->tx_q2[ADAPTER_LOW_INDUCTANCE_TX_80W]);
	}
	return 0;
}

static int nuvolta_rx1671_gpio_init(struct nuvolta_1671_chg *chip)
{
	int ret = 0;
	if (gpio_is_valid(chip->irq_gpio)) {
		chip->client->irq = gpio_to_irq(chip->irq_gpio);
		if (chip->client->irq < 0) {
			mca_log_err("gpio_to_irq Fail! \n");
			goto fail_irq_gpio;
		}
	} else {
		mca_log_err("irq gpio not provided\n");
		goto fail_irq_gpio;
	}
	if (gpio_is_valid(chip->power_good_gpio)) {
		chip->power_good_irq = gpio_to_irq(chip->power_good_gpio);
		if (chip->power_good_irq < 0) {
			mca_log_err("gpio_to_irq Fail! \n");
			goto fail_power_good_gpio;
		}
	} else {
		mca_log_err("power good gpio not provided\n");
		goto fail_power_good_gpio;
	}

	if (chip->support_hall) {
		if (gpio_is_valid(chip->hall_n_int_gpio)) {
			chip->hall_n_int_irq = gpio_to_irq(chip->hall_n_int_gpio);
			if (chip->hall_n_int_irq < 0) {
				mca_log_err("gpio_to_hall_n_int Fail!\n");
				goto fail_hall_n_int_gpio;
			}
		} else {
			mca_log_err("hall int gpio not provided\n");
			goto fail_hall_n_int_gpio;
		}

		if (gpio_is_valid(chip->hall_s_int_gpio)) {
			chip->hall_s_int_irq = gpio_to_irq(chip->hall_s_int_gpio);
			if (chip->hall_s_int_irq < 0) {
				mca_log_err("gpio_to_hall_s_int Fail!\n");
				goto fail_hall_s_int_gpio;
			}
		} else {
			mca_log_err("hall int gpio not provided\n");
			goto fail_hall_s_int_gpio;
		}
	}
	return ret;
fail_hall_s_int_gpio:
	gpio_free(chip->hall_s_int_gpio);
fail_hall_n_int_gpio:
	gpio_free(chip->hall_n_int_gpio);
fail_irq_gpio:
	gpio_free(chip->irq_gpio);
fail_power_good_gpio:
	gpio_free(chip->power_good_gpio);
	return ret;
}

static ssize_t chip_vrect_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int vrect = 0, ret = 0;
	ret = nuvolta_1671_get_vrect(g_chip, &vrect);
	if (ret < 0 ) {
		mca_log_err("get vrect failed\n");
		vrect = 0;
	}
	return scnprintf(buf, PAGE_SIZE, "rx1671 Vrect : %d mV\n", vrect);
}
static ssize_t chip_iout_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int iout = 0, ret = 0;
	ret = nuvolta_1671_get_iout(g_chip, &iout);
	if (ret < 0 ) {
		mca_log_err("get iout failed\n");
		iout = 0;
	}
	return scnprintf(buf, PAGE_SIZE, "%d\n", iout);
}

static ssize_t chip_temp_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int temp = 0, ret = 0;
	ret = nuvolta_1671_get_temp(g_chip, &temp);
	if (ret < 0 ) {
		mca_log_err("get temp failed\n");
		temp = 0;
	}
	return scnprintf(buf, PAGE_SIZE, "%d\n", temp);
}

static ssize_t chip_vout_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int vout = 0, ret = 0;
	ret = nuvolta_1671_get_vout(g_chip, &vout);
	if (ret < 0 ) {
		mca_log_err("get vout failed\n");
		vout = 0;
	}
	return scnprintf(buf, PAGE_SIZE, "%d\n", vout);
}

static ssize_t chip_vout_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int index;
	index = (int)simple_strtoul(buf, NULL, 10);
	mca_log_info("[rx1671] --Store output_voltage = %d\n", index);
	if ((index < 4000) || (index > 21000)) {
		mca_log_err("Store Voltage %s is invalid\n", buf);
		nuvolta_1671_set_vout(g_chip, 0);
		return count;
	}
	nuvolta_1671_set_vout(g_chip, index);
	return count;
}

static ssize_t wls_debug_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int vout = 0, vrect=0, iout=0, ret = 0;
	mca_log_err("[WLS_DEBUG] enter\n");
	ret = nuvolta_1671_get_vout(g_chip, &vout);
	if (ret < 0 ) {
		mca_log_err("[WLS_DEBUG] get vout failed\n");
		vout = 0;
	}
	ret = nuvolta_1671_get_vrect(g_chip, &vrect);
	if (ret < 0)
	{
		mca_log_err("[WLS_DEBUG] get vrect failed\n");
		vrect = 0;
	}
	ret = nuvolta_1671_get_iout(g_chip, &iout);
	if (ret < 0)
	{
		mca_log_err("[WLS_DEBUG] get iout failed\n");
		iout = 0;
	}
	mca_log_err("[WLS_DEBUG] vout vret iout = [%d %d %d]\n", vout, vrect, iout);
	return scnprintf(buf, PAGE_SIZE, "vout=%d, vrect=%d, iout=%d\n", vout, vrect, iout);
}

enum WLS_DEBUG_CMD {
	WLS_DEBUG_FCC = 1,
	WLS_DEBUG_ICL,
	WLS_DEBUG_EPP_FOD_SINGLE,
	WLS_DEBUG_EPP_FOD_ALL,
	WLS_DEBUG_EPP_FOD_MORE_FIVE,
	WLS_DEBUG_MAX,
};

static const char *const wls_debug_text[] = {
	"none", "set_fcc", "set_icl", "set_epp_fod_single", "set_epp_fod_all", "set_epp_fod_more_five", "too_bigger"};

static ssize_t wls_debug_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int num, i = 0, j = 0;
	char *token = NULL;
	int data[30] = {0};
	char dest[100];
	char *rest = NULL;
	strcpy(dest, buf);
	rest = dest;
	mca_log_err("[WLS_DEBUG] %s\n", buf);
	mca_log_err("[WLS_DEBUG] rest %s\n", rest);
	while ((token = strsep(&rest, " ")) != NULL) {
    	num = simple_strtol(token, NULL, 10);;
		if(i < 30)
			data[i]  = num;
		else
			break;
    	mca_log_err("[WLS_DEBUG] num=%d",num);
		i++;
    }
	mca_log_err("[WLS_DEBUG] \n");
	switch(data[0])
	{
		if(data[0] <= WLS_DEBUG_MAX)
			mca_log_err("[WLS_DEBUG] enter %s\n", wls_debug_text[data[0]]);
		case WLS_DEBUG_FCC:
		if(data[1] > 0 && data[1] <= 10000)
			vote(g_chip->fcc_votable, WLS_DEBUG_CHG_VOTER, true, data[1]);
		else if(data[1] == 0)
			vote(g_chip->fcc_votable, WLS_DEBUG_CHG_VOTER, false, 0);
		break;
		case WLS_DEBUG_ICL:
		if(data[1] > 0 && data[1] <= 2500)
			vote(g_chip->icl_votable, WLS_DEBUG_CHG_VOTER, true, data[1]);
		else if(data[1] == 0)
			vote(g_chip->icl_votable, WLS_DEBUG_CHG_VOTER, false, 0);
		break;
		case WLS_DEBUG_EPP_FOD_SINGLE:
		memset(g_chip->wls_debug_all_fod_params.params, 0, sizeof(g_chip->wls_debug_all_fod_params.params));
		if(data[1] >= 0 && data[1] < 12)
		{
			g_chip->wls_debug_all_fod_params.params[data[1]].gain = data[2];
			g_chip->wls_debug_all_fod_params.params[data[1]].offset = data[3];
			mca_log_err("[WLS_DEBUG] index = %d, gain=%d, offset=%d\n", data[1],
						g_chip->wls_debug_all_fod_params.params[data[1]].gain, g_chip->wls_debug_all_fod_params.params[data[1]].offset);
		}
		nuvolta_1671_set_fod(g_chip, &g_chip->wls_debug_all_fod_params);
		break;
		case WLS_DEBUG_EPP_FOD_ALL:
		memset(g_chip->wls_debug_all_fod_params.params, 0, sizeof(g_chip->wls_debug_all_fod_params.params));
		for(j=0; j < 11; j++)
		{
			g_chip->wls_debug_all_fod_params.params[j].gain = data[j*2+1];
			g_chip->wls_debug_all_fod_params.params[j].offset = data[j*2+2];
		}
		for(j=0; j < 12; j++)
		{
			mca_log_err("[WLS_DEBUG] gain=%d, offset=%d\n", g_chip->wls_debug_all_fod_params.params[j].gain,
								g_chip->wls_debug_all_fod_params.params[j].offset);
		}
		nuvolta_1671_set_fod(g_chip, &g_chip->wls_debug_all_fod_params);
		break;
		case WLS_DEBUG_EPP_FOD_MORE_FIVE:
		//TODO
		break;
		default:
		break;
	}
	return count;
}

static ssize_t wls_bin_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	static u16 total_length = 0;
	static u8 fw_area = 0;

	if( strncmp("length:", buf, 7 ) == 0 ) {
		if (kstrtou16( buf+7, 10, &total_length))
		      return -EINVAL;
		g_msg.total_length = total_length;
		g_chip->fw_bin_length = total_length;
		g_msg.block_num = 0;
		mca_log_err("[WLS_DEBUG] total_length:%d\n", total_length);
	} else if( strncmp("area:", buf, 5 ) == 0 ) {
		if (kstrtou8( buf+5, 10, &fw_area))
		      return -EINVAL;
		g_msg.fw_area = fw_area;
		g_msg.block_num = 0;
		mca_log_err("[WLS_DEBUG] area:%d\n", fw_area);
	}else {
		mca_log_err("[WLS_DEBUG] enter set wls_bin\n");
		memcpy(g_msg.wls_bin+(g_msg.block_num*512), buf, 512);
		g_msg.block_num ++;
	}
	return count;
}

static unsigned char fw_arrange_data[32768];
static int nuvolta_1671_mtp_program(struct nuvolta_1671_chg *chip,
	unsigned char *fw_data, int fw_data_length)
{
	int ret = 0;
	u8 read_data = 0, wrfail = 0, busy = 0;
	int i = 0, j = 0;
	int index = 0;

	ret = rx1671_write(chip, 0x00, 0x1f23);
	ret |= rx1671_write(chip, 0x2d, 0x1f23);
	ret |= rx1671_write(chip, 0xd2, 0x1f23);
	ret |= rx1671_write(chip, 0x22, 0x1f23);
	ret |= rx1671_write(chip, 0xdd, 0x1f23);
	if (ret)
		goto exit;

	ret = rx1671_read(chip, &read_data, 0x1f23);
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, 0x00, 0x1f26);
	ret |= rx1671_write(chip, 0x4b, 0x1f26);
	ret |= rx1671_write(chip, 0xb4, 0x1f26);
	ret |= rx1671_write(chip, 0x44, 0x1f26);
	ret |= rx1671_write(chip, 0xbb, 0x1f26);
	if (ret)
		goto exit;

	ret = rx1671_read(chip, &read_data, 0x1f26);
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, 0xC8, 0x1002);
	if (ret < 0)
		goto exit;

	ret = rx1671_read(chip, &read_data, 0x008c);
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, 0x82, 0x008c);
	ret |= rx1671_write(chip, 0x00, 0x1f26);
	if (ret < 0)
		goto exit;

	ret = rx1671_read(chip, &read_data, 0x008c);
	if (ret < 0)
		goto exit;

	ret = rx1671_write(chip, 0x40, 0x0090);
	ret |= rx1671_write(chip, 0x01, 0x4a28);
	ret |= rx1671_write(chip, 0x08, 0x4a43);
	ret |= rx1671_write(chip, 0x44, 0x4a32);
	ret |= rx1671_write(chip, 0x1b, 0x4a33);
	ret |= rx1671_write(chip, 0x21, 0x4a30);
	ret |= rx1671_write(chip, 0x80, 0x4918);
	if (ret)
		goto exit;

	ret = rx1671_read(chip, &read_data, 0x008c);
	if (ret < 0)
		goto exit;
	ret = rx1671_write(chip, 0x10, 0x0018);//0x00--2Kbyte, 0x10--64byte, 0x20--128byte
	ret |= rx1671_write(chip, 0x00, 0x0019);
	ret |= rx1671_write(chip, 0x0f, 0x0012);
	if (ret)
		goto exit;

	for (i = 0; i < 32*1024; i+=4) {
		fw_arrange_data[i+0] = (fw_data == NULL ? 0xFF : fw_data[i+3]);
		fw_arrange_data[i+1] = (fw_data == NULL ? 0xFF : fw_data[i+2]);
		fw_arrange_data[i+2] = (fw_data == NULL ? 0xFF : fw_data[i+1]);
		fw_arrange_data[i+3] = (fw_data == NULL ? 0xFF : fw_data[i+0]);
	}

	/************************write the first 4 bytes*************************/
	ret = rx1671_write(chip, 0x00, 0x0010);
	ret |= rx1671_write(chip, 0x00, 0x0011);
	if (ret)
		goto exit;

	mca_log_err("fw_arrange_data[0]:0x%x\n", fw_arrange_data[0]);
	mca_log_err("fw_arrange_data[1]:0x%x\n", fw_arrange_data[1]);
	mca_log_err("fw_arrange_data[2]:0x%x\n", fw_arrange_data[2]);
	mca_log_err("fw_arrange_data[3]:0x%x\n", fw_arrange_data[3]);

	ret = rx1671_write(chip, fw_arrange_data[0], 0x001c);
	ret |= rx1671_write(chip, fw_arrange_data[1], 0x001d);
	ret |= rx1671_write(chip, fw_arrange_data[2], 0x001e);
	ret |= rx1671_write(chip, fw_arrange_data[3], 0x001f);
	if (ret)
		goto exit;

	ret = rx1671_write(chip, 0x01, 0x0017);
	ret |= rx1671_write(chip, 0x01, 0x0019);
	ret |= rx1671_write(chip, 0x00, 0x0019);
	if (ret)
		goto exit;

	for (j = 0; j < RX_FW_WRITE_CHECK_COUNT; j++) {//250
		ret = rx1671_read(chip, &read_data, REG_MTP_STATUS);//0x001b
		if (ret < 0)
			goto exit;

		busy = (read_data & MTP_STATUS_MASK) >> MTP_STATUS_SHIFT;
		if (busy == 0) {
			wrfail = (read_data & MTP_WRITE_RESULT_MASK) >> MTP_WRITE_RESULT_SHIFT;
			if (wrfail == 1) {
				mca_log_err("wrfail, MTP error\n");
				ret = -1;
				goto exit;
			} else
				break;
		}
		usleep_range(100, 110);//100us
	}
	/************************write the first 4 bytes*************************/

	ret = rx1671_write(chip, 0x6A, 0x001A);
	if (ret)
		goto exit;
	mca_log_err("start write FW data\n");

	/************************write data*************************/
	for (index = 0; index < 511; index+=2) {
		ret = rx1671_write_buffer(chip, &fw_arrange_data[64 * index], 0x2800, 64);//write 64 bytes for each times
		if (ret < 0)
			goto exit;

		ret = rx1671_write(chip, 0x02, 0x0019);
		ret |= rx1671_write(chip, 0x06, 0x0019);
		if (ret)
			goto exit;

		for (j = 0; j < 50; j++) {//the maximum delay time is 150ms
			ret = rx1671_read(chip, &read_data, 0x00a4);
			if (ret < 0)
				goto exit;
			if (read_data == 0)
				break;
			msleep(3);
		}

		ret = rx1671_write_buffer(chip, &fw_arrange_data[64 * (index+1)], 0x3000, 64);
		ret |= rx1671_write(chip, 0x02, 0x0019);
		ret |= rx1671_write(chip, 0x0a, 0x0019);
		if (ret)
			goto exit;

		for (j = 0; j < 50; j++) {//the maximum delay time is 150ms
			ret = rx1671_read(chip, &read_data, 0x00a4);
			if (ret < 0)
				goto exit;
			if (read_data == 0)
				break;
			msleep(3);
		}
	}
	/************************write data*************************/

	ret = rx1671_read(chip, &read_data, 0x00a4);
	if (ret < 0)
		goto exit;
	mca_log_err("reg0x00a4: 0x%x \n",read_data);

	ret = rx1671_write(chip, 0x00, 0x0017);
	ret |= rx1671_write(chip, 0x00, 0x001a);
	ret |= rx1671_write(chip, 0x00, 0x4918);

	ret |= rx1671_write(chip, 0x00, 0x4a30);
	ret |= rx1671_write(chip, 0x08, 0x1002);
	ret |= rx1671_write(chip, 0x01, 0x5000);
	ret |= rx1671_write(chip, 0x03, 0x5008);

	ret |= rx1671_write(chip, 0x00, 0x0090);
	ret |= rx1671_write(chip, 0x00, 0x1002);
	ret |= rx1671_write(chip, 0x00, 0x1f23);
	if (ret)
		goto exit;

	mca_log_err("MTP download finished! \n");
	msleep(100);

	return ret;
exit:
	mca_log_err("wrfail, MTP error\n");
	msleep(100);

	return ret;
}

static int nuvolta_1671_download_fw_from_image(struct nuvolta_1671_chg *chip)
{
	int ret = 0;
	unsigned char *fw_data = NULL;
	int fw_data_length = 0;

	fw_data = chip->fw_data_ptr;
	fw_data_length = chip->fw_data_size;
	mca_log_err("start downloading firmware from image\n");
	ret = nuvolta_1671_mtp_program(chip, NULL, RX_FW_DATA_DEFAULT_LENGTH);
	if (ret) {
		mca_log_err("fail to erase before downloading firmware\n");
		return ret;
	}
	msleep(100);
	ret = nuvolta_1671_mtp_program(chip, fw_data, fw_data_length);
	msleep(200);
	return ret;
}

static int nuvolta_1671_download_fw_from_bin(struct nuvolta_1671_chg *chip)
{
	int ret = 0, i = 0;
	unsigned char *fw_data = NULL;
	int fw_data_length = 0;

	fw_data = g_msg.wls_bin;
	fw_data_length = sizeof(g_msg.wls_bin);
	mca_log_info("[WLS_DEBUG] fw data length: %d\n", fw_data_length);
	for (i = 0; i < fw_data_length; ++i) {
		fw_data[i] = ~fw_data[i];
	}

	mca_log_err("start downloading firmware from bin\n");

	ret = nuvolta_1671_mtp_program(chip, NULL, RX_FW_DATA_DEFAULT_LENGTH);
	if (ret) {
		mca_log_err("fail to erase before downloading firmware\n");
		return ret;
	}
	msleep(100);
	ret = nuvolta_1671_mtp_program(chip, fw_data, fw_data_length);
	msleep(200);
	chip->fw_bin_length = 0;
	return ret;
}

static int nuvolta_1671_firmware_update_func(struct nuvolta_1671_chg *chip, u8 cmd)
{
	int ret = 0;
	u8 check_result = 0;
	int otg_enable = 0, vusb_insert = 0;

	chip->fw_update = true;
	nuvolta_1671_set_enable_mode(chip, false);
	nu1671_enable_reverse_boost(chip, BOOST_FOR_FWUPDATE, true);
	msleep(100);
	ret = nuvolta_1671_check_i2c(chip);
	if (ret < 0){
		mca_log_err("ic enter sleep, cmd: %d\n", cmd);
	}

	 cmd = (cmd >= FW_UPDATE_MAX)? FW_UPDATE_FORCE : cmd;
	switch (cmd) {
	case FW_UPDATE_USER:
        check_result = nuvolta_1671_check_rx_fw_version(chip);
        if (check_result == (RX_CHECK_SUCCESS | TX_CHECK_SUCCESS | BOOT_CHECK_SUCCESS)) {
            mca_log_err("fw no need update\n");
            ret = 0;
            goto exit;
        }
        ret = nuvolta_1671_download_fw_from_image(chip);
        if (ret < 0) {
            mca_log_err("fw download failed! cmd: %d\n", cmd);
            goto exit;
        }
        msleep(100);
        break;
	case FW_UPDATE_FORCE:
		ret = nuvolta_1671_download_fw_from_image(chip);
		if (ret < 0) {
			mca_log_err("fw download failed! cmd: %d\n", cmd);
			goto exit;
		}
		msleep(100);
		break;
	case FW_UPDATE_FROM_BIN:
		ret = nuvolta_1671_download_fw_from_bin(chip);
		if (ret < 0) {
			mca_log_err("fw download failed! cmd: %d\n", cmd);
			goto exit;
		}
		msleep(100);
		break;
	case FW_UPDATE_ERASE:
		ret = nuvolta_1671_mtp_program(chip, NULL, RX_FW_DATA_DEFAULT_LENGTH);
		if (ret < 0) {
			mca_log_err("fw erase failed! cmd: %d\n", cmd);
			goto exit;
		}
		break;
	default:
		mca_log_err("unknown cmd: %d\n", cmd);
		break;
	}

	if (cmd != FW_UPDATE_CHECK) {
        chip->fw_version_reflash = true;
        nu1671_enable_reverse_boost(chip, BOOST_FOR_FWUPDATE, false);
        msleep(1000);
        nu1671_enable_reverse_boost(chip, BOOST_FOR_FWUPDATE, true);
        chip->fw_version_reflash = false;
        msleep(100);
    }
	check_result = nuvolta_1671_check_rx_fw_version(chip);
	if (check_result == (RX_CHECK_SUCCESS | TX_CHECK_SUCCESS | BOOT_CHECK_SUCCESS)) {
		mca_log_info("download firmware success!\n");
	} else {
		ret = -1;
		mca_log_info("download firmware failed!\n");
	}
exit:
	chip->fw_update = false;
	nu1671_enable_reverse_boost(chip, BOOST_FOR_FWUPDATE, false);
	usb_get_property(USB_PROP_OTG_ENABLE, &otg_enable);
	wls_get_property(WLS_PROP_VUSB_INSERT, &vusb_insert);
	if (!otg_enable && !vusb_insert)
		nuvolta_1671_set_enable_mode(chip, true);
	return ret;
}
static ssize_t chip_firmware_update_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int cmd = 0, ret = 0;
	if (g_chip->fw_update){
		mca_log_info("Firmware Update is on going!\n");
		return count;
	}
	cmd = (int)simple_strtoul(buf, NULL, 10);
	mca_log_info("value %d\n", cmd);
	if ((cmd > FW_UPDATE_NONE) && (cmd < FW_UPDATE_MAX)) {
		ret = nuvolta_1671_firmware_update_func(g_chip, cmd);
		if (ret < 0) {
			mca_log_err("Firmware Update:failed!\n");
			return count;
		} else {
			mca_log_info("Firmware Update:Success!\n");
			return count;
		}
	} else {
		mca_log_err("Firmware Update:invalid cmd\n");
	}
	return count;
}

static ssize_t chip_version_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	u8 check_result = 0;
	if (g_chip->fw_update) {
		mca_log_info("fw update going, can not show version\n");
		return scnprintf(buf, PAGE_SIZE, "updating\n");
	} else {
		nuvolta_1671_set_enable_mode(g_chip, false);
		g_chip->fw_update = true;
		nu1671_enable_reverse_boost(g_chip, BOOST_FOR_FWUPDATE, true);
		msleep(100);
		check_result = nuvolta_1671_check_rx_fw_version(g_chip);
		g_chip->fw_update = false;
		nuvolta_1671_set_enable_mode(g_chip, true);
		nu1671_enable_reverse_boost(g_chip, BOOST_FOR_FWUPDATE, false);
		return scnprintf(buf, PAGE_SIZE, "%02x.%02x.%02x.%x%x\n",
				g_chip->wls_fw_data->fw_version, g_chip->wls_fw_data->fw_version, g_chip->wls_fw_data->fw_version,
				g_chip->wls_fw_data->hw_id_h, g_chip->wls_fw_data->hw_id_l);
	}
}

static ssize_t hall_n_gpio_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	// struct sc96231 *chip = dev_get_drvdata(dev); TODO:delete g_chip
    int hall3_n_gpio_val =  !!g_chip->hall_n_gpio_status;
    return scnprintf(buf, PAGE_SIZE, "%d\n", hall3_n_gpio_val);
}

static ssize_t hall_s_gpio_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
    int hall3_s_gpio_val =  !!g_chip->hall_s_gpio_status;
    return scnprintf(buf, PAGE_SIZE, "%d\n", hall3_s_gpio_val);
}

static DEVICE_ATTR(chip_vrect, S_IRUGO, chip_vrect_show, NULL);
static DEVICE_ATTR(chip_firmware_update, S_IWUSR, NULL, chip_firmware_update_store);
static DEVICE_ATTR(chip_version, S_IRUGO, chip_version_show, NULL);
static DEVICE_ATTR(chip_vout, S_IWUSR | S_IRUGO, chip_vout_show, chip_vout_store);
static DEVICE_ATTR(chip_iout, S_IRUGO, chip_iout_show, NULL);
static DEVICE_ATTR(chip_temp, S_IRUGO, chip_temp_show, NULL);
static DEVICE_ATTR(wls_debug, S_IWUSR | S_IRUGO, wls_debug_show, wls_debug_store);
static DEVICE_ATTR(wls_bin, S_IWUSR, NULL, wls_bin_store);
static DEVICE_ATTR(hall_n_gpio, S_IRUGO, hall_n_gpio_show, NULL);
static DEVICE_ATTR(hall_s_gpio, S_IRUGO, hall_s_gpio_show, NULL);

static struct attribute *rx1671_sysfs_attrs[] = {
	&dev_attr_chip_vrect.attr,
	&dev_attr_chip_version.attr,
	&dev_attr_chip_vout.attr,
	&dev_attr_chip_iout.attr,
	&dev_attr_chip_temp.attr,
	&dev_attr_chip_firmware_update.attr,
	&dev_attr_wls_debug.attr,
	&dev_attr_wls_bin.attr,
	&dev_attr_hall_s_gpio.attr,
	&dev_attr_hall_n_gpio.attr,
	NULL,
};
static const struct attribute_group rx1671_sysfs_group_attrs = {
	.attrs = rx1671_sysfs_attrs,
};

static int nu1671_wls_is_wireless_present(struct wireless_charger_device *chg_dev, bool *present)
{
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);
	if (chip->power_good_flag)
		*present = true;
	else
		*present = false;
	return 0;
}

static int nu1671_wls_is_i2c_ok(struct wireless_charger_device *chg_dev, bool *i2c_ok)
{
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);

	if (!chip->power_good_flag)
		*i2c_ok = false;
	else
		*i2c_ok = chip->i2c_ok_flag;
	return 0;
}

static int nu1671_wls_is_qc_enable(struct wireless_charger_device *chg_dev, bool *enable)
{
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);
	if (chip->qc_enable)
		*enable = true;
	else
		*enable = false;
	return 0;
}

static int nu1671_wls_is_firmware_update(struct wireless_charger_device *chg_dev, bool *update)
{
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);
	if (chip->fw_update)
		*update = true;
	else
		*update = false;
	return 0;
}

static int nu1671_wls_is_car_adapter(struct wireless_charger_device *chg_dev, bool *enable)
{
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);
	if (chip->is_car_tx)
		*enable = true;
	else
		*enable = false;
	return 0;
}

static int nu1671_wls_get_reverse_charge(struct wireless_charger_device *chg_dev, bool *enable)
{
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);
	if (chip->reverse_chg_en)
		*enable = true;
	else
		*enable = false;
	return 0;
}

static int nu1671_wls_get_saved_fw_version(struct wireless_charger_device *chg_dev, char *buf)
{
	int ret = 0;
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);
	if (chip->fw_update) {
		mca_log_info("fw update going, can not show version\n");
		return scnprintf(buf, PAGE_SIZE, "updating\n");
	} else {
		ret = scnprintf(buf, 10, "%02x.%02x.%02x", chip->wls_fw_data->fw_version,
		chip->wls_fw_data->fw_version, chip->wls_fw_data->fw_version);
		mca_log_info("is %s\n", buf);
	}
	return ret;
}

static int nu1671_wls_firmware_update(struct wireless_charger_device *chg_dev, int cmd)
{
	int ret = 0;
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);
	if (chip->fw_update){
		mca_log_info("Firmware Update is on going!\n");
		return ret;
	}
	mca_log_info("value %d\n", cmd);
	if ((cmd > FW_UPDATE_NONE) && (cmd < FW_UPDATE_MAX)) {
		ret = nuvolta_1671_firmware_update_func(chip, cmd);
		if (ret < 0) {
			mca_log_err("Firmware Update:failed!\n");
			return ret;
		} else {
			mca_log_info("Firmware Update:Success!\n");
			return ret;
		}
	} else {
		mca_log_err("Firmware Update:invalid cmd\n");
	}
	return ret;
}

static int nuvolta_1671_check_rx_fw_version_inchg(struct nuvolta_1671_chg *chip)
{
	int ret = 0;
	int check_result = BOOT_CHECK_SUCCESS | RX_CHECK_SUCCESS | TX_CHECK_SUCCESS;
	u8 image_fw_version = CRC_CHECK_ERR_VER;
	u8 ic_fw_version = CRC_CHECK_ERR_VER;
	u8 read_buf[FW_VERSION_BUF_LENGTH] = {0};

	image_fw_version = nuvolta_1671_get_image_fw_version(chip);
	ret = rx1671_write(chip, RX_CMD_START_READ, REG_RX_INT_2);
	if (ret < 0) {
		mca_log_err("fail to write : 0x%x\n", REG_RX_INT_2);
		return check_result;
	}
	msleep(20);
	ret = rx1671_read(chip, &ic_fw_version, (0x2320+0x04));
	if (ret < 0) {
		mca_log_err("fail to read : 0x%x\n", 0x2324);
		return check_result;
	}

	ret = rx1671_write(chip, RX_CMD_ENABLE_CRC, REG_RX_INT_3);
	if (ret < 0) {
		mca_log_err("fail to write : 0x%x\n", REG_RX_INT_3);
		return check_result;
	}
	msleep(100);
	ret = rx1671_read_buffer(chip, read_buf, REG_RX_REV_DATA8, FW_VERSION_BUF_LENGTH);
	if (ret < 0) {
		mca_log_err("fail to read : 0x%x\n", 0x0028);
		return check_result;
	}
	mca_log_err("fw crc check: 0x%x\n", read_buf[0]);
	if (read_buf[0] == CRC_CHECK_SUCCESS && ic_fw_version >= image_fw_version && ic_fw_version != 0xfe) {
		chip->wls_fw_data->fw_version = ic_fw_version;
		check_result = BOOT_CHECK_SUCCESS | RX_CHECK_SUCCESS | TX_CHECK_SUCCESS;
	} else {
		chip->wls_fw_data->fw_version = ic_fw_version;
		check_result = 0;
	}
	mca_log_err("image fw version: 0x%x\n", image_fw_version);
	mca_log_err("ic fw version: 0x%x, check_result: 0x%x\n",
			chip->wls_fw_data->fw_version, check_result);

	return check_result;
}

static int nu1671_wls_check_ic_fw_version(struct wireless_charger_device *chg_dev)
{
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);
	int check_result = 0;

	if (chip->fw_update) {
		mca_log_err("fw update going, can not check fw version\n");
		return check_result;
	}

	if (!chip->power_good_flag) {
		chip->fw_update = true;
		nu1671_enable_reverse_boost(chip, BOOST_FOR_FWUPDATE, true);
		msleep(100);
		check_result = nuvolta_1671_check_rx_fw_version(chip);
		nu1671_enable_reverse_boost(chip, BOOST_FOR_FWUPDATE, false);
		chip->fw_update = false;
	} else {
		check_result = nuvolta_1671_check_rx_fw_version_inchg(chip);
	}
	mca_log_info("check_result : 0x%x\n", check_result);
	return check_result;
}

static int nu1671_wls_set_rx_sleep_mode(struct wireless_charger_device *chg_dev, int sleep_for_dam)
{
	int ret = 0;
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);

    if (sleep_for_dam)
		ret = rx1671_write(chip, 0x03, 0x0063);
	mca_log_err("%d\n", sleep_for_dam);
    return ret;
}

static int nu1671_wls_set_quiet_sts(struct wireless_charger_device *chg_dev, int quiet_sts)
{
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);
    int tx_speed = WLS_TX_FAN_SPEED_NORMAL;

    chip->quiet_sts = quiet_sts;

    if(!chip->power_good_flag) {
        mca_log_err("wls is not online, keep sts %d wait for next chg\n",quiet_sts);
    } else {
        tx_speed = quiet_sts? WLS_TX_FAN_SPEED_QUIET : WLS_TX_FAN_SPEED_NORMAL;
        if (chip->tx_speed != tx_speed)
            nuvolta_1671_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_FAN_SPEED, tx_speed);
        else
            mca_log_err("tx speed is already %d, no need to set\n", tx_speed);
    }

    return 0;
}

static int nu1671_wls_set_parallel_charge(struct wireless_charger_device *chg_dev, bool parachg)
{
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);

    chip->parallel_charge = parachg;
    mca_log_err("%d\n", parachg);

    return 0;
}

static int nu1671_wls_is_vout_range_set_done(struct wireless_charger_device *chg_dev, bool *is_done)
{
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);

    if (chip->power_good_flag)
        *is_done = chip->is_vout_range_set_done;
    else
        *is_done = false;

    return 0;
}

static int nu1671_wls_get_adapter_chg_mode(struct wireless_charger_device *chg_dev, int *cp_chg_mode)
{
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);
	int uuid = 0, i = 0, found = true;

	uuid |= chip->uuid[0] << 24;
	uuid |= chip->uuid[1] << 16;
	uuid |= chip->uuid[2] << 8;
	uuid |= chip->uuid[3];
    mca_log_info("uuid: 0x%x,0x%x,0x%x,0x%x\n", chip->uuid[0], chip->uuid[1], chip->uuid[2], chip->uuid[3]);

    switch (chip->adapter_type) {
    case ADAPTER_XIAOMI_QC3:
    case ADAPTER_XIAOMI_PD:
    case ADAPTER_ZIMI_CAR_POWER:
    case ADAPTER_XIAOMI_PD_40W:
    case ADAPTER_VOICE_BOX:
        *cp_chg_mode = FORWARD_2_1_CHARGER_MODE;
        break;
    case ADAPTER_XIAOMI_PD_50W:
    case ADAPTER_XIAOMI_PD_60W:
    case ADAPTER_XIAOMI_PD_100W:
		for (i = 0; i < chip->fod_params_size_2_1; i++) {
			found = true;
			if (uuid != chip->fod_params_2_1[i].uuid) {
				found = false;
				continue;
			}
			/* found fod by uuid */
			if (found) {
				mca_log_info("uuid: 0x%x,0x%x,0x%x,0x%x\n", chip->uuid[0],
				chip->uuid[1], chip->uuid[2], chip->uuid[3]);
			}
		}
        if (chip->force_cp_2_1_mode)
            *cp_chg_mode = FORWARD_2_1_CHARGER_MODE;
        else if (chip->is_car_tx || chip->is_sailboat_tx)
            *cp_chg_mode = FORWARD_2_1_CHARGER_MODE;
        else if (found)
            *cp_chg_mode = FORWARD_4_1_CHARGER_MODE | FORWARD_2_1_CHARGER_MODE;
		else
			*cp_chg_mode = FORWARD_4_1_CHARGER_MODE;
        break;
    default:
        *cp_chg_mode = FORWARD_4_1_CHARGER_MODE;
        break;
    }

    chip->cp_chg_mode = *cp_chg_mode;
    mca_log_err("cp_chg_mode:%d\n", chip->cp_chg_mode);
    return 0;
}

static int nuvolta_1671_get_hall_n_gpio_status(struct wireless_charger_device *chg_dev, bool *hall_n_gpio_status)
{
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);

	if(chip == NULL)
		return -EINVAL;
	*hall_n_gpio_status =  !!chip->hall_n_gpio_status;
	mca_log_err(" hall_n = %d\n", *hall_n_gpio_status);
	return 0;
}

static int nuvolta_1671_get_hall_s_gpio_status(struct wireless_charger_device *chg_dev, bool *hall_s_gpio_status)
{
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);

	if(chip == NULL)
		return -EINVAL;
	*hall_s_gpio_status =  !!chip->hall_s_gpio_status;
	mca_log_err(" hall_s = %d\n", *hall_s_gpio_status);
	return 0;
}

static int nuvolta_1671_get_power_good_low_debounce(struct wireless_charger_device *chg_dev, bool *pg_low_debounce)
{
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);

	if(chip == NULL)
		return -EINVAL;
	*pg_low_debounce =  !!chip->pg_low_debounce;
	mca_log_err(" pg_low_debounce = %d\n", *pg_low_debounce);
	return 0;
}

static int nu1671_wls_enable_reverse_chg(struct wireless_charger_device *chg_dev, bool enable)
{
	int ret = 0;
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);

    ret = nuvolta_1671_set_reverse_chg_mode(chip, enable);
    chip->user_reverse_chg = enable;

    return ret;
}
static int nu1671_wls_set_vout(struct wireless_charger_device *chg_dev, int vout)
{
	int ret = 0;
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);
	if (chip->power_good_flag)
		ret = nuvolta_1671_set_vout(chip, vout);
	else
		mca_log_info("power good off, can't set vout\n");
	mca_log_err("wls_set_vout: %d\n", vout);
	return ret;
}

static int nu1671_wls_get_vout(struct wireless_charger_device *chg_dev, int *vout)
{
	int ret = 0;
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);
	if (chip->power_good_flag) {
		ret = nuvolta_1671_get_vout(chip, vout);
		if (ret < 0)
			*vout = 0;
	} else
		*vout = 0;
	mca_log_err("wls_get_vout: %d\n", *vout);
	return 0;
}

static int nu1671_wls_get_iout(struct wireless_charger_device *chg_dev, int *iout)
{
	int ret = 0;
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);
	if (chip->power_good_flag) {
		ret = nuvolta_1671_get_iout(chip, iout);
		if (ret < 0)
			*iout = 0;
	} else
		*iout = 0;
	mca_log_err("wls_get_iout: %d\n", *iout);
	return 0;
}

static int nu1671_wls_get_vrect(struct wireless_charger_device *chg_dev, int *vrect)
{
	int ret = 0;
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);
	if (chip->power_good_flag) {
		ret = nuvolta_1671_get_vrect(chip, vrect);
		if (ret < 0)
			*vrect = 0;
	} else
		*vrect = 0;
	mca_log_err("wls_get_vrect: %d\n", *vrect);
	return 0;
}

static int nu1671_wls_get_tx_adapter(struct wireless_charger_device *chg_dev, int *adapter)
{
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);
	if (chip->power_good_flag)
		*adapter = chip->adapter_type;
	else
		*adapter = ADAPTER_NONE;
	return 0;
}

static int nu1671_wls_get_tx_uuid(struct wireless_charger_device *chg_dev, char *buf)
{
	int ret = 0;
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);
	if (!chip->power_good_flag) {
        ret = scnprintf(buf, PAGE_SIZE, "00.00.00.00");
        return ret;
    }

    ret = scnprintf(buf, PAGE_SIZE, "%02x.%02x.%02x.%02x",
            chip->uuid[0], chip->uuid[1], chip->uuid[2], chip->uuid[3]);
    mca_log_err("%s\n", buf);

    return ret;
}

static int nu1671_wls_get_reverse_chg_state(struct wireless_charger_device *chg_dev, int *state)
{
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);
	*state = chip->is_reverse_chg;
	mca_log_err(" is_reverse_chg:%d\n", chip->is_reverse_chg);
	return 0;
}

static int nu1671_wls_is_enable(struct wireless_charger_device *chg_dev, bool *enable)
{
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);
	*enable = (chip->enable_flag == 0)? false : true;
    return 0;
}

static int nu1671_wls_set_enable_mode(struct wireless_charger_device *chg_dev, bool enable)
{
	int ret = 0;
	struct nuvolta_1671_chg *chip = dev_get_drvdata(&chg_dev->dev);
	int len;
	char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
	struct mca_event_notify_data event_data = { 0 };

	if (!chip->power_good_flag && !enable) {
        if (chip->icl_votable) {
            vote(chip->icl_votable, WLS_CHG_VOTER, false, 0);
			vote(chip->icl_votable, WLS_DEBUG_CHG_VOTER, false, 0);
			vote(chip->icl_votable, WLS_PARACHG_VOTER, false, 0);
			vote(chip->icl_votable, WLS_POWER_REDUCE_VOTER, false, 0);
        }
        if (chip->fcc_votable) {
            vote(chip->fcc_votable, WLS_CHG_VOTER, false, 0);
            vote(chip->fcc_votable, WLS_POWER_REDUCE_VOTER, false, 0);
            vote(chip->fcc_votable, WLS_Q_VALUE_STRATEGY_VOTER, false, 0);
            vote(chip->fcc_votable, WLS_DEBUG_CHG_VOTER, false, 0);
        }
    }
	ret = nuvolta_1671_set_enable_mode(chip, enable);
	if(ret){
		mca_log_err("set enable mode failed\n");
	}else{
		mca_log_err("set enable mode success\n");
	}
	if (!enable && chip->reverse_boost_src == PMIC_REV_BOOST && chip->reverse_chg_en) {
        mca_log_info("wls revchg state is %d, close it\n", chip->is_reverse_chg);
        nuvolta_1671_set_reverse_chg_mode(chip, false);
        chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
        len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_REVERSE_CHG_STATE=%d", chip->is_reverse_chg);
        event_data.event = event;
        event_data.event_len = len;
        mca_event_report_uevent(&event_data);
    }
	return 0;
}

static int nuvolta_1671_set_enable_mode(struct nuvolta_1671_chg *chip, bool enable)
{
	int ret = 0;
	int gpio_enable_val = 0;
	int en = !!enable;
	if(chip == NULL){
		mca_log_err("chip poniter null\n");
		return -EINVAL;
	}
	if (gpio_is_valid(chip->enable_gpio)) {
		ret = gpio_direction_output(chip->enable_gpio, !en);
		if (ret) {
			mca_log_err("cannot set direction for idt enable gpio [%d]\n", chip->enable_gpio);
		}
		gpio_enable_val = gpio_get_value(chip->enable_gpio);
		chip->enable_flag = (gpio_enable_val == 0) ? 1 : 0;
		mca_log_err("nuvolta enable gpio val is :%d\n", gpio_enable_val);
	}
	return ret;
}

static const struct wireless_charger_properties nuvolta_1671_chg_props = {
	.alias_name = "nuvolta_wireless_chg",
};
static const struct wireless_charger_ops nuvolta_1671_chg_ops = {
	.wls_enable_reverse_chg = nu1671_wls_enable_reverse_chg,
	.wls_is_wireless_present = nu1671_wls_is_wireless_present,
	.wls_is_i2c_ok = nu1671_wls_is_i2c_ok,
	.wls_is_qc_enable = nu1671_wls_is_qc_enable,
	.wls_is_firmware_update = nu1671_wls_is_firmware_update,
	.wls_set_vout = nu1671_wls_set_vout,
	.wls_get_vout = nu1671_wls_get_vout,
	.wls_get_iout = nu1671_wls_get_iout,
	.wls_get_vrect = nu1671_wls_get_vrect,
	.wls_get_tx_adapter = nu1671_wls_get_tx_adapter,
	.wls_get_tx_uuid = nu1671_wls_get_tx_uuid,
	.wls_get_reverse_chg_state = nu1671_wls_get_reverse_chg_state,
	.wls_is_enable = nu1671_wls_is_enable,
	.wls_enable_chg = nu1671_wls_set_enable_mode,
	.wls_is_car_adapter = nu1671_wls_is_car_adapter,
	.wls_get_reverse_chg = nu1671_wls_get_reverse_charge,
	.wls_get_chip_version = nu1671_wls_get_saved_fw_version,
	.wls_firmware_update = nu1671_wls_firmware_update,
	.wls_check_fw_version = nu1671_wls_check_ic_fw_version,
	.wls_set_rx_sleep_mode = nu1671_wls_set_rx_sleep_mode,
	.wls_set_quiet_sts = nu1671_wls_set_quiet_sts,
	.wls_set_parallel_charge = nu1671_wls_set_parallel_charge,
	.wls_is_vout_range_set_done = nu1671_wls_is_vout_range_set_done,
	.wls_get_adapter_chg_mode = nu1671_wls_get_adapter_chg_mode,
	.wls_get_hall_n_gpio_status = nuvolta_1671_get_hall_n_gpio_status,
	.wls_get_hall_s_gpio_status = nuvolta_1671_get_hall_s_gpio_status,
	.wls_get_power_good_low_debounce = nuvolta_1671_get_power_good_low_debounce,
};

static int nuvolta_1671_chg_init_chgdev(struct nuvolta_1671_chg *chip)
{
	mca_log_info("enter\n");
	chip->wlschgdev = wireless_charger_device_register(chip->wlsdev_name, chip->dev,
						chip, &nuvolta_1671_chg_ops,
						&nuvolta_1671_chg_props);
	return IS_ERR(chip->wlschgdev) ? PTR_ERR(chip->wlschgdev) : 0;
}

static bool nuvolta_1671_check_votable(struct nuvolta_1671_chg *chip)
{
	if (!chip->fcc_votable)
		chip->fcc_votable = find_votable("CHARGER_FCC");
	if (!chip->fcc_votable) {
		mca_log_err("failed to get fcc_votable\n");
		return false;
	}
	if (!chip->icl_votable)
		chip->icl_votable = find_votable("CHARGER_ICL");
	if (!chip->icl_votable) {
		mca_log_err("failed to get icl_votable\n");
		return false;
	}
	return true;
}

static int nuvolta_1671_probe(struct i2c_client *client)
{
	int ret = 0;
	bool temp = false;
	struct nuvolta_1671_chg *chip;
	char *name = NULL;
	mca_log_info("enter nuvolta 1671 probe V2.0\n");
	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		mca_log_err("Failed to allocate memory\n");
		return -ENOMEM;
	}
	chip->wls_fw_data = devm_kzalloc(&client->dev, sizeof(*chip->wls_fw_data), GFP_KERNEL);
	if (!chip->wls_fw_data)
		return -ENOMEM;
	chip->regmap = devm_regmap_init_i2c(client, &nuvolta_1671_regmap_config);
	if (IS_ERR(chip->regmap)) {
		mca_log_err("failed to allocate register map\n");
		return PTR_ERR(chip->regmap);
	}
	chip->client = client;
	chip->dev = &client->dev;
	chip->fw_update = false;
	chip->ss = 2;
	chip->wlsdev_name = NUVOLTA_1671_DRIVER_NAME;
	chip->chg_phase = NORMAL_MODE;
	chip->hall_n_gpio_status = true;
	chip->hall_s_gpio_status = true;
	chip->enable_flag = 1;
	chip->user_reverse_chg = true;
	chip->fw_version_reflash = false;
	chip->revchg_test_status = REVERSE_TEST_NONE;
	chip->qc_type = QUICK_CHARGE_NORMAL;
	chip->pg_low_debounce = false;
	g_chip = chip;
	device_init_wakeup(&client->dev, true);
	i2c_set_clientdata(client, chip);
	mutex_init(&chip->wireless_chg_int_lock);
	mutex_init(&chip->data_transfer_lock);
	mutex_init(&chip->i2c_lock);
	name = devm_kasprintf(chip->dev, GFP_KERNEL, "%s",
		"wireless_chg suspend wakelock");
	chip->wls_wakelock = wakeup_source_register(NULL, name);

	INIT_LIST_HEAD(&chip->header);
	spin_lock_init(&chip->list_lock);
	init_waitqueue_head(&chip->wait_que);

	INIT_DELAYED_WORK(&chip->wireless_int_work, nuvolta_1671_wireless_int_work);
	INIT_DELAYED_WORK(&chip->wireless_pg_det_work, nuvolta_1671_pg_det_work);
	INIT_DELAYED_WORK(&chip->chg_monitor_work, nuvolta_1671_monitor_work);
	INIT_DELAYED_WORK(&chip->init_detect_work, nuvolta_1671_init_detect_work);
	INIT_DELAYED_WORK(&chip->init_fw_check_work, nu1671_init_fw_check_work);
	INIT_DELAYED_WORK(&chip->rx_alarm_work, nuvolta_1671_rx_alarm_work);
	INIT_DELAYED_WORK(&chip->i2c_check_work, nuvolta_1671_i2c_check_work);
	INIT_DELAYED_WORK(&chip->renegociation_work, nuvolta_wireless_renegociation_work);
	INIT_DELAYED_WORK(&chip->hall_interrupt_work, nuvolta_1671_hall_interrupt_work);
	INIT_DELAYED_WORK(&chip->trans_data_work, nuvolta_1671_trans_data_work);
	INIT_DELAYED_WORK(&chip->mutex_unlock_work, nuvolta_1671_mutex_unlock_work);
	INIT_DELAYED_WORK(&chip->factory_reverse_start_work, nuvolta_1671_factory_reverse_start_work);
	INIT_DELAYED_WORK(&chip->factory_reverse_stop_work, nuvolta_1671_factory_reverse_stop_work);
	INIT_DELAYED_WORK(&chip->reverse_chg_config_work, nu1671_reverse_chg_config_work);
	INIT_DELAYED_WORK(&chip->reverse_chg_monitor_work, nu1671_reverse_chg_monitor_work);
	INIT_DELAYED_WORK(&chip->reverse_transfer_timeout_work, nu1671_reverse_transfer_timeout_work);
	INIT_DELAYED_WORK(&chip->reverse_ping_timeout_work, nu1671_reverse_ping_timeout_work);
	nuvolta_1671_parse_dt(chip);
	nuvolta_rx1671_gpio_init(chip);
	if(chip->client->irq) {
		ret = devm_request_threaded_irq(&chip->client->dev, chip->client->irq, NULL,
				nuvolta_1671_interrupt_handler,
				(IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_SHARED),
				"nuvolta_1671_chg_stat_irq", chip);
		if (ret)
			mca_log_err("Failed irq = %d ret = %d\n", chip->client->irq, ret);
	}
	enable_irq_wake(chip->client->irq);
	if (chip->power_good_irq) {
		ret = devm_request_threaded_irq(&chip->client->dev, chip->power_good_irq, NULL,
				nuvolta_1671_power_good_handler,
				(IRQF_TRIGGER_FALLING |  IRQF_TRIGGER_RISING | IRQF_ONESHOT),
				"nuvolta_1671_power_good_irq", chip);
		if (ret) {
			mca_log_err("Failed irq = %d ret = %d\n", chip->power_good_irq, ret);
		}
	}
	enable_irq_wake(chip->power_good_irq);

	if (chip->support_hall) {
		if (chip->hall_n_int_irq) {
			ret = devm_request_threaded_irq(&chip->client->dev, chip->hall_n_int_irq, NULL,
					nuvolta_1671_hall_interrupt_handler,
					(IRQF_TRIGGER_FALLING |  IRQF_TRIGGER_RISING | IRQF_ONESHOT),
					"nuvolta_1671_hall_n_int_irq", chip);
			if (ret)
				mca_log_err("Failed hall n irq = %d ret = %d\n", chip->hall_n_int_irq, ret);
			enable_irq_wake(chip->hall_n_int_irq);
		}

		if (chip->hall_s_int_irq) {
			ret = devm_request_threaded_irq(&chip->client->dev, chip->hall_s_int_irq, NULL,
					nuvolta_1671_hall_interrupt_handler,
					(IRQF_TRIGGER_FALLING |  IRQF_TRIGGER_RISING | IRQF_ONESHOT),
					"nuvolta_1671_hall_s_int_irq", chip);
			if (ret)
				mca_log_err("Failed hall s irq = %d ret = %d\n", chip->hall_s_int_irq, ret);
			enable_irq_wake(chip->hall_s_int_irq);
		}
	}

	/* register charger device for wireless */
	ret = nuvolta_1671_chg_init_chgdev(chip);
	if (ret < 0) {
		mca_log_err("failed to register wireless chgdev %d\n", ret);
		return -ENODEV;
	}

	temp = nuvolta_1671_check_votable(chip);
	if (!temp)
		mca_log_err("failed to check vote %d\n", temp);
	ret = sysfs_create_group(&chip->dev->kobj, &rx1671_sysfs_group_attrs);
	if (ret < 0)
	{
		mca_log_err("sysfs_create_group fail %d\n", ret);
		goto error_sysfs;
	}
	/* pmic boost  */
	chip->pmic_boost = devm_regulator_get(chip->dev, "pmic_vbus");
	if (IS_ERR(chip->pmic_boost)) {
		mca_log_err("failed to get pmic vbus\n");
		goto error_sysfs;
	}
	/* get master cp dev  */
	if (!chip->master_cp_dev)
		chip->master_cp_dev = get_charger_by_name("cp_master");
	if (!chip->master_cp_dev) {
		mca_log_err("failed to get master_cp_dev\n");
		//goto error_sysfs;
	}
		/* get master cp dev  */
	if (!chip->chg_dev)
		chip->chg_dev = get_charger_by_name("primary_chg");
	if (!chip->chg_dev) {
		mca_log_err("failed to get chg_dev\n");
		//goto error_sysfs; TODO why can not get
	}
	chip->shutdown_flag = false;
	/* reset wls charge when power good online */
	schedule_delayed_work(&chip->i2c_check_work, msecs_to_jiffies(0));
	schedule_delayed_work(&chip->init_detect_work, msecs_to_jiffies(1000));
	schedule_delayed_work(&chip->init_fw_check_work, msecs_to_jiffies(20 * 1000));
	if (chip->support_hall)
		schedule_delayed_work(&chip->hall_interrupt_work, msecs_to_jiffies(1200));
	mca_log_info("success! \n");
	return 0;
error_sysfs:
	sysfs_remove_group(&chip->dev->kobj, &rx1671_sysfs_group_attrs);
	if (chip->irq_gpio > 0)
		gpio_free(chip->irq_gpio);
	if (chip->power_good_gpio > 0)
		gpio_free(chip->power_good_gpio);
	return 0;
}

static void nuvolta_1671_remove(struct i2c_client *client)
{
	struct nuvolta_1671_chg *chip = i2c_get_clientdata(client);
	mutex_destroy(&chip->i2c_lock);
	mutex_destroy(&chip->wireless_chg_int_lock);
	mutex_destroy(&chip->data_transfer_lock);
	if (chip->irq_gpio > 0)
		gpio_free(chip->irq_gpio);
	if (chip->power_good_gpio > 0)
		gpio_free(chip->power_good_gpio);
	if (chip->support_hall) {
		if (chip->hall_n_int_gpio > 0)
			gpio_free(chip->hall_n_int_gpio);
		if (chip->hall_s_int_gpio > 0)
			gpio_free(chip->hall_s_int_gpio);
	}
	return;
}

static int nuvolta_1671_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nuvolta_1671_chg *chip = i2c_get_clientdata(client);
	mca_log_err("in sleep\n");
	return enable_irq_wake(chip->client->irq);
}

static int nuvolta_1671_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nuvolta_1671_chg *chip = i2c_get_clientdata(client);
	mca_log_err("in sleep\n");
	return disable_irq_wake(chip->client->irq);
}

static const struct dev_pm_ops nuvolta_1671_pm_ops = {
	.suspend	= nuvolta_1671_suspend,
	.resume		= nuvolta_1671_resume,
};

static void nuvolta_1671_shutdown(struct i2c_client *client)
{
	struct nuvolta_1671_chg *chip = i2c_get_clientdata(client);
	if (chip->power_good_flag) {
		nuvolta_1671_set_enable_mode(chip, false);
		usleep_range(20000, 25000);
		nuvolta_1671_set_enable_mode(chip, true);
	}
	mutex_lock(&chip->i2c_lock);
	chip->shutdown_flag = true;
	mutex_unlock(&chip->i2c_lock);
	mca_log_info("shutdown: %s\n", chip->wlsdev_name);
	return;
}
static const struct of_device_id nuvolta_1671_match_table[] = {
	{ .compatible = "nuvolta,rx1671",},
	{ },
};
static const struct i2c_device_id nuvolta_1671_id[] = {
	{"nuvolta_1671", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, nuvolta_1671_id);
static struct i2c_driver nuvolta_1671_driver = {
	.driver		= {
		.name		= "nuvolta_1671",
		.owner		= THIS_MODULE,
		.of_match_table	= nuvolta_1671_match_table,
		.pm		= &nuvolta_1671_pm_ops,
	},
	.probe		= nuvolta_1671_probe,
	.remove		= nuvolta_1671_remove,
	.id_table	= nuvolta_1671_id,
	.shutdown	= nuvolta_1671_shutdown,
};
module_i2c_driver(nuvolta_1671_driver);
MODULE_AUTHOR("xiezhichang <xiezhichang@xiaomi.com>");
MODULE_DESCRIPTION("nuvolta wireless charge driver");
MODULE_LICENSE("GPL");
