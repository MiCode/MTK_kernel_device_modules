
/* Copyright (c) 2022 The Linux Foundation. All rights reserved.
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
/* this driver is compatible for southchip wireless charge ic */
#include <linux/i2c.h>
#include <linux/alarmtimer.h>
#include <linux/ktime.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <asm/unaligned.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/regulator/consumer.h>
#include <asm/uaccess.h>
#include <linux/wait.h>
#include <linux/list.h>
#include "../charger_class.h"
#include "../mtk_charger.h"
#include "../bq28z610.h"
#include "sc96281.h"
#include "sc96281_mtp_program.h"
#include "sc96281_firmware.h"
//#include "sc96281_pgm.h"
#include <linux/mca/common/mca_log.h>
#include <linux/mca/common/mca_event.h>
#include <linux/mca/common/mca_charge_mievent.h>

#ifndef MCA_LOG_TAG
#define MCA_LOG_TAG "mca_wls_sc96281_info"
#endif

static int log_level = 1;
#define sc96281_log_err(fmt, ...)							\
do {										\
	if (log_level >= 0)							\
		printk(KERN_ERR "[WLS_SC96281] " fmt, ##__VA_ARGS__);	\
} while (0)
#define sc96281_log_info(fmt, ...)							\
do {										\
	if (log_level >= 1)							\
		printk(KERN_ERR "[WLS_SC96281] " fmt, ##__VA_ARGS__);	\
} while (0)
#define sc96281_log_dbg(fmt, ...)							\
do {										\
	if (log_level >= 2)							\
		printk(KERN_ERR "[WLS_SC96281] " fmt, ##__VA_ARGS__);	\
} while (0)

static struct sc96281_chg *g_chip;
static struct wls_fw_parameters g_wls_fw_data = {0};
static struct regmap_config sc96281_regmap_config = {
    .reg_bits = 16,
    .val_bits = 8,
    .max_register = 0xFFFF,
};

static struct int_map_t rx_irq_map[] = {
    DECL_INTERRUPT_MAP(WP_IRQ_RX_POWER_ON, RX_INT_POWER_ON),
    DECL_INTERRUPT_MAP(WP_IRQ_RX_LDO_ON, RX_INT_LDO_ON),
    DECL_INTERRUPT_MAP(WP_IRQ_RX_AUTH, RX_INT_AUTHEN_FINISH),
    DECL_INTERRUPT_MAP(WP_IRQ_RX_RENEG_SUCCESS, RX_INT_RENEGO_DONE),
    DECL_INTERRUPT_MAP(WP_IRQ_RX_FAST_CHARGE_SUCCESS, RX_INT_FAST_CHARGE),
    DECL_INTERRUPT_MAP(WP_IRQ_OTP, RX_INT_OCP_OTP_ALARM),
    DECL_INTERRUPT_MAP(WP_IRQ_OTP_110, RX_INT_OCP_OTP_ALARM),
    DECL_INTERRUPT_MAP(WP_IRQ_PKT_RECV, RX_INT_TRANSPARENT_SUCCESS),
    DECL_INTERRUPT_MAP(WP_IRQ_PPP_TIMEOUT, RX_INT_TRANSPARENT_FAIL),
    DECL_INTERRUPT_MAP(WP_IRQ_RX_FACTORY_TEST, RX_INT_FACTORY_TEST),
    DECL_INTERRUPT_MAP(WP_IRQ_SLEEP, RX_INT_POWER_OFF),
    DECL_INTERRUPT_MAP(WP_IRQ_ERROR_CODE, RX_INT_ERR_CODE),
    //DECL_INTERRUPT_MAP(WP_IRQ_RX_RENEG_FAIL, RX_INT_RENEGO_FAIL),
    //DECL_INTERRUPT_MAP(xx, RX_INT_ALARM_SUCCESS),
    //DECL_INTERRUPT_MAP(xx, RX_INT_ALARM_FAIL),
    //DECL_INTERRUPT_MAP(xx, RX_INT_OOB_GOOD),
    //DECL_INTERRUPT_MAP(xx, RX_INT_RPP),
};

static struct int_map_t tx_irq_map[] = {
    DECL_INTERRUPT_MAP(WP_IRQ_TX_PING, RTX_INT_PING),
    DECL_INTERRUPT_MAP(WP_IRQ_TX_DET_RX, RTX_INT_GET_RX),
    DECL_INTERRUPT_MAP(WP_IRQ_TX_CEP_TIMEOUT, RTX_INT_CEP_TIMEOUT),
    DECL_INTERRUPT_MAP(WP_IRQ_TX_EPT, RTX_INT_EPT),
    //DECL_INTERRUPT_MAP(xx, RTX_INT_PROTECTION),
    DECL_INTERRUPT_MAP(WP_IRQ_TX_DET_TX, RTX_INT_GET_TX),
    //DECL_INTERRUPT_MAP(xx, RTX_INT_REVERSE_TEST_READY),
    //DECL_INTERRUPT_MAP(xx, RTX_INT_REVERSE_TEST_DONE),
    DECL_INTERRUPT_MAP(WP_IRQ_TX_FOD, RTX_INT_FOD),
    DECL_INTERRUPT_MAP(WP_IRQ_PKT_RECV, RTX_INT_EPT_PKT),
    DECL_INTERRUPT_MAP(WP_IRQ_ERROR_CODE, RTX_INT_ERR_CODE),
};

static int sc96281_check_i2c(struct sc96281_chg *chip);
static u8 sc96281_no_charging_get_fw_version(struct sc96281_chg *chip);
static int sc96281_firmware_update_func(struct sc96281_chg *chip, u8 cmd);

//--------------------------IIC API-----------------------------
static int __sc96281_read_block(struct sc96281_chg *chip, uint16_t reg, uint8_t *data, uint8_t length)
{
    int ret;

    ret = regmap_raw_read(chip->regmap, reg, data, length);
    if (ret < 0) {
        sc96281_log_err("i2c read fail: can't read from reg 0x%04X\n", reg);
    }

    return ret;
}

static int __sc96281_write_block(struct sc96281_chg *chip, uint16_t reg, uint8_t *data, uint8_t length)
{
    int ret;

    ret = regmap_raw_write(chip->regmap, reg, data, length);
    if (ret < 0) {
        sc96281_log_err("i2c write fail: can't write 0x%04X: %d\n", reg, ret);
    }

    return ret;
}

static int sc96281_read_block(struct sc96281_chg *chip, uint16_t reg, uint8_t *data, uint8_t len)
{
    int ret;
    uint16_t alignAddr = reg & 0XFFFFFFFC;
    uint32_t alignOffs = reg % 4;
    uint8_t ext = (reg + len) % 4;
    uint16_t length = len;
    uint8_t *pbuf = NULL;

    if (chip->fw_program) {
        sc96281_log_err("firmware programming\n");
        return -1;
    }

    mutex_lock(&chip->i2c_rw_lock);
    length += alignOffs;
    if (ext != 0)
        length += (4 - ext);

    pbuf = kzalloc(length, GFP_KERNEL);

    ret = __sc96281_read_block(chip, alignAddr, pbuf, length);
    if (ret)
        sc96281_log_err("read fail\n");
    else
        memcpy(data, pbuf + alignOffs, len);

    kfree(pbuf);
    mutex_unlock(&chip->i2c_rw_lock);

    return ret;
}

static int __sc96281_write_read_block(struct sc96281_chg *chip, uint16_t reg, uint8_t *data, uint8_t len)
{
    int ret;
    uint16_t alignAddr = reg & 0XFFFFFFFC;
    uint32_t alignOffs = reg % 4;
    uint8_t ext = (reg + len) % 4;
    uint16_t length = len;
    uint8_t *pbuf = NULL;

    length += alignOffs;
    if (ext != 0)
        length += (4 - ext);

    pbuf = kzalloc(length, GFP_KERNEL);

    ret = __sc96281_read_block(chip, alignAddr, pbuf, length);
    if (ret)
        sc96281_log_err("read fail\n");
    else
        memcpy(data, pbuf + alignOffs, len);

    kfree(pbuf);

    return ret;
}

static int sc96281_write_block(struct sc96281_chg *chip, uint16_t reg, uint8_t *data, uint8_t len)
{
    int ret;
    uint16_t alignAddr = reg & 0XFFFFFFFC;
    uint32_t alignOffs = reg % 4;
    uint8_t ext = (reg + len) % 4;
    uint16_t length = len;
    uint8_t *pbuf = NULL;

    if (chip->fw_program) {
        sc96281_log_err("firmware programming\n");
        return -1;
    }

    mutex_lock(&chip->i2c_rw_lock);
    pbuf = kzalloc(len + 8, GFP_KERNEL);
    if (alignOffs != 0) {
        ret = __sc96281_write_read_block(chip, alignAddr, pbuf, len + 8);
        if (ret) {
            sc96281_log_err("sc96281 block read failed: reg=0x%04X len=%d ret=%d\n", alignAddr, alignOffs, ret);
            goto write_fail;
        }
        memcpy(pbuf + alignOffs, data, length);
        length += alignOffs;
    } else {
        memcpy(pbuf, data, length);
    }

    if (ext != 0) {
        ret = __sc96281_write_read_block(chip, alignAddr + length, pbuf + length, 4 - ext);
        if (ret) {
            sc96281_log_err("sc96281 block read failed: reg=0x%04X len=%d ret=%d\n", alignAddr + length, 4 - ext, ret);
            goto write_fail;
        }
        length += (4 - ext);
    }

    ret = __sc96281_write_block(chip, alignAddr, pbuf, length);

write_fail:
    kfree(pbuf);
    mutex_unlock(&chip->i2c_rw_lock);

    return ret;
}


//-------------------sc96281 system interface-------------------
#define read_cust(sc, member, p) \
    sc96281_read_block(sc, (uint64_t)(&(member)), (uint8_t *)(p), (uint8_t)sizeof(member))
#define write_cust(sc, member, p) \
    sc96281_write_block(sc, (uint64_t)(&(member)), (uint8_t *)(p), (uint8_t)sizeof(member))

static uint8_t private_sizeof(uint8_t header, uint8_t cmd) {
    if (header == 0x2F && cmd == 0x62) return 4;
    else return 0;
}

static uint8_t sizeof_msg(uint8_t header) {
    uint8_t len;
    if (header < 0x20)
        len = 1;
    else if (header < 0x80)
        len = 2 + ((header - 0x20) >> 4);
    else if (header < 0xe0)
        len = 8 + ((header - 0x80) >> 3);
    else len = 20 + ((header - 0xe0) >> 2);
    return len;
}


//-------------------sc96281 external setting-------------------
static bool sc96281_check_votable(struct sc96281_chg *chip)
{
    if (!chip->fcc_votable)
        chip->fcc_votable = find_votable("CHARGER_FCC");
    if (!chip->fcc_votable) {
        sc96281_log_err("failed to get fcc_votable\n");
        return false;
    }
    if (!chip->icl_votable)
        chip->icl_votable = find_votable("CHARGER_ICL");
    if (!chip->icl_votable) {
        sc96281_log_err("failed to get icl_votable\n");
        return false;
    }
    return true;
}

static void sc96281_set_pmic_icl(struct sc96281_chg *chip, int mA)
{
    if (chip->icl_votable)
        vote(chip->icl_votable, WLS_CHG_VOTER, true, mA);
    else
        sc96281_log_err("no icl votable, don't set icl\n");
    sc96281_log_info("wls icl setting, %d-%d %s\n", mA, get_effective_result(chip->icl_votable), get_effective_client(chip->icl_votable));
    return;
}

static void sc96281_stepper_pmic_icl(struct sc96281_chg *chip, int start_icl, int end_icl, int step_ma, int ms)
{
    int temp_icl = start_icl;
    sc96281_set_pmic_icl(chip, temp_icl);
    if (start_icl < end_icl) {
        while (temp_icl < end_icl) {
            sc96281_set_pmic_icl(chip, temp_icl);
            temp_icl += step_ma;
            msleep(ms);
        }
    } else {
        while (temp_icl > end_icl) {
            sc96281_set_pmic_icl(chip, temp_icl);
            if (temp_icl > step_ma)
                temp_icl -= step_ma;
            else
                temp_icl = 0;
            msleep(ms);
        }
    }
    sc96281_set_pmic_icl(chip, end_icl);
    return;
}

static int sc96281_get_icl(struct sc96281_chg *chip)
{
    int effective_icl = RX_PMIC_ICL_DEFAULT_MA;

    if (chip->icl_votable)
        effective_icl = get_effective_result(chip->icl_votable);

    sc96281_log_info("wls get icl: %d\n", effective_icl);
    return effective_icl;
}

static void sc96281_set_pmic_fcc(struct sc96281_chg *chip,int mA)
{
    if (chip->fcc_votable)
        vote(chip->fcc_votable, WLS_CHG_VOTER, true, mA);
    else
        sc96281_log_err("no fcc votable, don't set fcc\n");
    sc96281_log_info("wls set fcc: %d\n", mA);
    return;
}

static int sc96281_get_fcc(struct sc96281_chg *chip)
{
    int effective_fcc = RX_PMIC_FCC_DEFAULT_MA;

    if (chip->fcc_votable)
        effective_fcc = get_effective_result(chip->fcc_votable);

    sc96281_log_info("wls get fcc: %d\n", effective_fcc);
    return effective_fcc;
}

static int sc96281_enable_power_path(struct sc96281_chg *chip, bool en)
{
    sc96281_log_info("%s: %d\n", __func__, en);
    return charger_dev_enable_powerpath(chip->chg_dev, true);
}

static int sc96281_enable_bc12(struct sc96281_chg *chip, bool attach)
{
    int ret = 0;
    union power_supply_propval prop;
    static struct power_supply *bc12_psy;

    bc12_psy = power_supply_get_by_name("primary_chg");
    if (IS_ERR_OR_NULL(bc12_psy)) {
        sc96281_log_err("%s Couldn't get bc12_psy\n", __func__);
        return ret;
    } else {
        prop.intval = attach;
        return power_supply_set_property(bc12_psy, POWER_SUPPLY_PROP_ONLINE, &prop);
    }
}

static void sc96281_add_trans_task_to_queue(struct sc96281_chg *chip, TRANS_DATA_FLAG data_flag, int value)
{
    struct trans_data_lis_node *node = NULL;

    node = kmalloc(sizeof(struct trans_data_lis_node), GFP_ATOMIC);
    if (!node) {
        sc96281_log_err("create node error, return\n");
        return;
    }

    sc96281_log_info("add: data flag: 0x%02x, value: %d\n", data_flag, value);

    spin_lock(&chip->list_lock);
    node->data_flag = data_flag;
    node->value = value;
    list_add_tail(&node->lnode, &chip->header);
    chip->head_cnt++;
    spin_unlock(&chip->list_lock);

    wake_up_interruptible(&chip->wait_que);
}

//-------------------sc96281 TX interface-------------------
static int sc96281_tx_set_cmd(struct sc96281_chg *chip, uint32_t cmd)
{
    int ret;

    ret = write_cust(chip, cust_tx.cmd, &cmd);
    if (ret)
        sc96281_log_err("sc96281 tx set cmd fail\n");

    return ret;
}

static int sc96281_start_tx_function(struct sc96281_chg *chip, bool en)
{
    int ret = 0;

    if (en)
        ret = sc96281_tx_set_cmd(chip, AP_CMD_TX_ENABLE);
    else
        ret = sc96281_tx_set_cmd(chip, AP_CMD_TX_DISABLE);

    sc96281_log_info("[%s] en:%d ret:%d\n", __func__, en, ret);
    return ret;
}

static void sc96281_tx_power_off_err(struct sc96281_chg *chip)
{
    return;
}

static int rx_set_reverse_boost_enable_gpio(struct sc96281_chg *chip, int enable)
{
   int ret = 0;
   if (gpio_is_valid(chip->reverse_boost_gpio)) {
       ret = gpio_request(chip->reverse_boost_gpio, "reverse-boost-enable-gpio");
       if (ret) {
           sc96281_log_err( "%s: unable to reverse_boost_enable_gpio [%d]\n",
               __func__, chip->reverse_boost_gpio);
       }
       ret = gpio_direction_output(chip->reverse_boost_gpio, !!enable);
       if (ret) {
           sc96281_log_err("%s: cannot set direction for reverse_boost_enable_gpio  gpio [%d]\n",
               __func__, chip->reverse_boost_gpio);
       }
       gpio_free(chip->reverse_boost_gpio);
   } else
       sc96281_log_err("%s: unable to set reverse_boost_enable_gpio\n", __func__);
    return ret;
}

static int sc96281_set_reverse_gpio(struct sc96281_chg *chip, int enable)
{
    int ret = 0;
    if (gpio_is_valid(chip->tx_on_gpio)) {
        if (enable) {
            rx_set_reverse_boost_enable_gpio(chip, enable);
            msleep(100);
        }
        ret = gpio_request(chip->tx_on_gpio, "tx-on-gpio");
        if (ret)
            sc96281_log_err("%s: unable to request tx_on gpio\n", __func__);

        ret = gpio_direction_output(chip->tx_on_gpio, enable);
        if (ret)
            sc96281_log_err("%s: cannot set direction for tx_on gpio\n", __func__);

        ret = gpio_get_value(chip->tx_on_gpio);
        sc96281_log_info("txon gpio: %d\n", ret);
        gpio_free(chip->tx_on_gpio);
    
        if (!enable) {
            msleep(100);
            rx_set_reverse_boost_enable_gpio(chip, enable);
        }
    } else
        sc96281_log_err("%s: unable to set tx_on gpio\n", __func__);
    return ret;
}

static int sc96281_set_reverse_pmic_boost(struct sc96281_chg *chip, int purpose, int enable)
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
        sc96281_log_err("%s failed to get master_cp_dev\n", __func__);
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
            ret = sc96281_check_i2c(chip);
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

static int sc96281_enable_reverse_boost(struct sc96281_chg *chip, int purpose, int enable)
{
    int ret = 0;

    if (chip->power_good_flag && enable) {
        mca_log_err("wls online, cannot enable rev boost\n");
        return -1;
    }

    chip->is_reverse_boosting = true;
    switch (chip->reverse_boost_src) {
    case PMIC_REV_BOOST:
        ret = sc96281_set_reverse_pmic_boost(chip, purpose, enable);
        break;
    case EXTERNAL_BOOST:
        ret = sc96281_set_reverse_gpio(chip, enable);
        break;
    default:
        ret = -1;
        mca_log_err("reverse boost source is invalid\n");
        break;
    }
    chip->is_reverse_boosting = false;

    return ret;
}

static int sc96281_set_reverse_chg_mode(struct sc96281_chg *chip, bool enable)
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
            sc96281_start_tx_function(chip, false);
            sc96281_enable_reverse_boost(chip, BOOST_FOR_REVCHG, false);
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

__maybe_unused static int sc96281_get_trx_cep(struct sc96281_chg *chip, int *trx_cep)
{
    int ret = 0;
    int8_t cep_value = 0;

    ret = read_cust(chip, cust_tx.cep_value, &cep_value);
    if (ret) {
        sc96281_log_err("get trx cep fail\n");
        cep_value = 0;
        return ret;
    }

    *trx_cep = (int)cep_value;
    sc96281_log_info("get trx cep: %d\n", *trx_cep);

    return ret;
}

static int sc96281_get_trx_iout(struct sc96281_chg *chip, int *iout)
{
    int ret = 0;
    uint16_t cur = 0;

    ret = read_cust(chip, cust_tx.iout, &cur);
    if (ret) {
        sc96281_log_err("get trx isense fail\n");
        *iout = 0;
        return ret;
    }

    *iout = (int)cur;
    sc96281_log_info("get trx isense: %d\n", *iout);

    return ret;
}

static int sc96281_get_trx_vrect(struct sc96281_chg *chip, int *vrect)
{
    int ret = 0;
    uint16_t rect = 0;

    ret = read_cust(chip, cust_tx.vrect, &vrect);
    if (ret) {
        sc96281_log_err("get trx vrect fail\n");
        *vrect = 0;
        return ret;
    }

    *vrect = (int)rect;
    sc96281_log_info("get trx vrect: %d\n", *vrect);

    return ret;
}

static int sc96281_get_trx_err_code(struct sc96281_chg *chip, uint32_t *err_code, uint32_t *dfx_code)
{
    int ret = 0;
    uint32_t code = 0;

    ret = read_cust(chip, cust_tx.err_code, &code);
    if (ret) {
        sc96281_log_err("get trx error code fail\n");
        *err_code = 0;
        return ret;
    }

    switch (code) {
        case ERROR_CODE_TX_PING_OVP: sc96281_log_info("tx error PING_OVP\n"); break;
        case ERROR_CODE_TX_PING_OCP: sc96281_log_info("tx error PING_OCP\n"); break;
        case ERROR_CODE_TX_OVP: sc96281_log_info("tx error OVP\n"); break;
        case ERROR_CODE_TX_OCP: sc96281_log_info("tx error OCP\n"); *dfx_code = CHARGE_DFX_WLS_TRX_OCP; break;
        case ERROR_CODE_TX_BRIDGE_OCP: sc96281_log_info("tx error BRIDGE_OCP\n"); break;
        case ERROR_CODE_TX_CLAMP_OVP: sc96281_log_info("tx error CLAMP_OVP\n"); break;
        case ERROR_CODE_TX_LVP: sc96281_log_info("tx error LVP\n"); break;
        case ERROR_CODE_TX_OTP: sc96281_log_info("tx error OTP\n"); break;
        case ERROR_CODE_TX_OTP_HARD: sc96281_log_info("tx error OTP_HARD\n"); break;
        case ERROR_CODE_TX_PRE_FOD: sc96281_log_info("tx error PRE_FOD\n"); break;
        case ERROR_CODE_TX_FOD: sc96281_log_info("tx error FOD\n"); break;
        case ERROR_CODE_TX_CE_TIMEOUT: sc96281_log_info("tx error CE_TIMEOUT\n"); break;
        case ERROR_CODE_TX_RP_TIMEOUT: sc96281_log_info("tx error RP_TIMEOUT\n"); break;
        case ERROR_CODE_TX_NOT_SS: sc96281_log_info("tx error NOT_SS\n"); break;
        case ERROR_CODE_TX_NOT_ID: sc96281_log_info("tx error NOT_ID\n"); break;
        case ERROR_CODE_TX_NOT_XID: sc96281_log_info("tx error NOT_XID\n"); break;
        case ERROR_CODE_TX_NOT_CFG: sc96281_log_info("tx error NOT_CFG\n"); break;
        case ERROR_CODE_TX_SS_TIMEOUT: sc96281_log_info("tx error SS_TIMEOUT\n"); break;
        case ERROR_CODE_TX_ID_TIMEOUT: sc96281_log_info("tx error ID_TIMEOUT\n"); break;
        case ERROR_CODE_TX_XID_TIMEOUT: sc96281_log_info("tx error XID_TIMEOUT\n"); break;
        case ERROR_CODE_TX_CFG_TIMEOUT: sc96281_log_info("tx error CFG_TIMEOUT\n"); break;
        case ERROR_CODE_TX_NEG_TIMEOUT: sc96281_log_info("tx error NEG_TIMEOUT\n"); break;
        case ERROR_CODE_TX_CAL_TIMEOUT: sc96281_log_info("tx error CAL_TIMEOUT\n"); break;
        case ERROR_CODE_TX_CFG_COUNT: sc96281_log_info("tx error CFG_COUNT\n"); break;
        case ERROR_CODE_TX_PCH_VALUE: sc96281_log_info("tx error PCH_VALUE\n"); break;
        case ERROR_CODE_TX_EPT_PKT: sc96281_log_info("tx error EPT_PKT\n"); break;
        case ERROR_CODE_TX_ILLEGAL_PKT: sc96281_log_info("tx error ILLEGAL_PKT\n"); break;
        case ERROR_CODE_TX_AC_DET: sc96281_log_info("tx error AC_DET\n"); break;
        case ERROR_CODE_TX_CHG_FULL: sc96281_log_info("tx error CHG_FULL\n"); break;
        case ERROR_CODE_TX_SS_ID: sc96281_log_info("tx error SS_ID\n"); break;
        case ERROR_CODE_TX_AP_CMD: sc96281_log_info("tx error AP_CMD\n"); break;
        default: sc96281_log_info("tx error code unknow\n"); break;
    }

    *err_code = code;
    return ret;
}

int sc96281_tx_get_int(struct sc96281_chg *chip, uint32_t *txint)
{
    int ret;

    ret = read_cust(chip, cust_tx.irq_flag, txint);
    if (ret) {
        sc96281_log_err("sc96281 get tx int fail\n");
    }

    return ret;
}

int sc96281_tx_clr_int(struct sc96281_chg *chip, uint32_t txint)
{
    int ret;

    mutex_lock(&chip->data_transfer_lock);
    ret = write_cust(chip, cust_tx.irq_clr, &txint);
    if (ret) {
        sc96281_log_err("sc96281 clear tx int fail\n");
    }
    mutex_unlock(&chip->data_transfer_lock);

    return ret;
}

uint16_t sc96281_tx_remap_int(struct sc96281_chg *chip, uint32_t txint)
{
    int i = 0;
    uint16_t val = 0;

    for (i = 0; i < ARRAY_SIZE(tx_irq_map); i++) {
        if (tx_irq_map[i].irq_regval & txint) {
            val |= tx_irq_map[i].irq_flag;
        }
    }

    return val;
}

static void sc96281_reverse_chg_config_work(struct work_struct *work)
{
    struct sc96281_chg *chip = container_of(work, struct sc96281_chg,
            reverse_chg_config_work.work);
    int ret = 0;
    int real_type = XMUSB350_TYPE_UNKNOW;
    int len;
    char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
    struct mca_event_notify_data event_data = { 0 };

    if (!chip->reverse_chg_en) {
        sc96281_log_info("revchg has been close\n");
        return;
    }

    ret = usb_get_property(USB_PROP_REAL_TYPE, &real_type);
    if ((ret >= 0) && (real_type == XMUSB350_TYPE_SDP || real_type == XMUSB350_TYPE_CDP))
        chip->bc12_reverse_chg = true;
    else
        chip->bc12_reverse_chg = false;

    if (chip->bc12_reverse_chg) {
        sc96281_log_err("BC1.2 cannot revchg\n");
        sc96281_set_reverse_chg_mode(chip, false);
        chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
        len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
        event_data.event = event;
        event_data.event_len = len;
        mca_event_report_uevent(&event_data);
        return;
    }

    ret = sc96281_enable_reverse_boost(chip, BOOST_FOR_REVCHG, true);

    if (!chip->user_reverse_chg) {
        sc96281_log_info("user close revchg\n");
        sc96281_set_reverse_chg_mode(chip, false);
        return;
    }

    if (ret) {
        sc96281_log_err("revchg boost fail\n");
        sc96281_set_reverse_chg_mode(chip, false);
        chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
        len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
        event_data.event = event;
        event_data.event_len = len;
        mca_event_report_uevent(&event_data);
        return;
    }

    ret = sc96281_start_tx_function(chip, true);
    sc96281_log_err("reverse charge done, ret=%d\n", ret);
}

static void sc96281_reverse_chg_monitor_work(struct work_struct *work)
{
    struct sc96281_chg *chip = container_of(work, struct sc96281_chg,
            reverse_chg_monitor_work.work);
    int iout = 0, vrect = 0;

    if (!chip->is_reverse_boosting && chip->is_reverse_chg == REVERSE_STATE_TRANSFER) {
        sc96281_get_trx_iout(chip, &iout);
        sc96281_get_trx_vrect(chip, &vrect);
        sc96281_log_info("wireless revchg: [iout:%d], [vrect:%d]\n", iout, vrect);
        schedule_delayed_work(&chip->reverse_chg_monitor_work, msecs_to_jiffies(5000));
    }
}

static void sc96281_reverse_transfer_timeout_work(struct work_struct *work)
{
    struct sc96281_chg *chip = container_of(work, struct sc96281_chg,
            reverse_transfer_timeout_work.work);
    int ret;
    int len;
    char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
    struct mca_event_notify_data event_data = { 0 };

    chip->tx_timeout_flag = true;
    ret = sc96281_set_reverse_chg_mode(chip, false);
    chip->is_reverse_chg = REVERSE_STATE_TIMEOUT;
    len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_TIMEOUT);
    event_data.event = event;
    event_data.event_len = len;
    mca_event_report_uevent(&event_data);
    sc96281_log_info("reverse chg transfer timeout");
    chip->tx_timeout_flag = false;
}

static void sc96281_reverse_ping_timeout_work(struct work_struct *work)
{
    struct sc96281_chg *chip = container_of(work, struct sc96281_chg,
            reverse_ping_timeout_work.work);
    int ret;
    int len;
    char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
    struct mca_event_notify_data event_data = { 0 };

    chip->tx_timeout_flag = true;
    ret = sc96281_set_reverse_chg_mode(chip, false);
    chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
    len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_REVERSE_CHG_STATE=%d", REVERSE_STATE_ENDTRANS);
    event_data.event = event;
    event_data.event_len = len;
    mca_event_report_uevent(&event_data);
    sc96281_log_info("reverse chg ping timeout");
    chip->tx_timeout_flag = false;
}

static int sc96281_reverse_enable_fod(struct sc96281_chg *chip, bool enable)
{
    int ret = 0;

    if (enable) {
        ret = sc96281_tx_set_cmd(chip, AP_CMD_TX_FOD_ENABLE);
        if (ret) {
            sc96281_log_err("enable tx fod fail\n");
        } else {
            sc96281_log_info("enable tx fod success\n");
        }
    } else {
        ret = sc96281_tx_set_cmd(chip, AP_CMD_TX_FOD_DISABLE);
        if (ret) {
            sc96281_log_err("disable tx fod fail\n");
        } else {
            sc96281_log_info("disable tx fod success\n");
        }
    }

    return ret;
}

static void sc96281_reverse_chg_handler(struct sc96281_chg *chip, u16 int_flag)
{
    uint32_t err_code = 0, dfx_code = 0;
    bool need_report = false;
    int len;
    char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
    struct mca_event_notify_data event_data = { 0 };

    if (int_flag & RTX_INT_PING) {
        mca_log_err("RTX_INT_PING!\n");
        if (!chip->tx_timeout_flag)
            cancel_delayed_work_sync(&chip->reverse_ping_timeout_work);
        schedule_delayed_work(&chip->reverse_transfer_timeout_work, msecs_to_jiffies(REVERSE_TRANSFER_TIMEOUT_TIMER));

    } else if (int_flag & RTX_INT_GET_RX) {
        mca_log_err("RTX_INT_GET_RX!\n");
        sc96281_reverse_enable_fod(chip, true);
        chip->is_reverse_chg = REVERSE_STATE_TRANSFER;
        need_report = true;
        if (!chip->tx_timeout_flag)
            cancel_delayed_work_sync(&chip->reverse_transfer_timeout_work);

    } else if (int_flag & RTX_INT_CEP_TIMEOUT) {
        mca_log_err("RTX_INT_CEP_TIMEOUT!\n");
        chip->is_reverse_chg = REVERSE_STATE_WAITPING;
        need_report = true;
        schedule_delayed_work(&chip->reverse_ping_timeout_work, msecs_to_jiffies(REVERSE_PING_TIMEOUT_TIMER));

    } else if (int_flag & RTX_INT_EPT) {
        mca_log_err("RTX_INT_EPT!\n");
        schedule_delayed_work(&chip->reverse_ping_timeout_work, msecs_to_jiffies(REVERSE_PING_TIMEOUT_TIMER));

    } else if (int_flag & RTX_INT_PROTECTION) {
        if (!chip->is_reverse_closing) {
            mca_log_err("RTX_INT_PROTECTION!\n");
            sc96281_tx_power_off_err(chip);
            sc96281_set_reverse_chg_mode(chip, false);
            chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
            need_report = true;
        }

    } else if (int_flag & RTX_INT_GET_TX) {
        if (!chip->is_reverse_closing) {
            mca_log_err("RTX_INT_GET_TX!\n");
            sc96281_set_reverse_chg_mode(chip, false);
            chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
            need_report = true;
        }

    } else if (int_flag & RTX_INT_REVERSE_TEST_READY) {
        mca_log_err("RTX_INT_REVERSE_TEST_READY!\n");
        chip->revchg_test_status = REVERSE_TEST_READY;
        if (!chip->tx_timeout_flag)
            cancel_delayed_work(&chip->factory_reverse_stop_work);

    } else if (int_flag & RTX_INT_REVERSE_TEST_DONE) {
        if (!chip->is_reverse_closing) {
            mca_log_err("RTX_INT_REVERSE_TEST_DONE!\n");
            sc96281_set_reverse_chg_mode(chip, false);
            chip->revchg_test_status = REVERSE_TEST_DONE;
        }

    } else if (int_flag & RTX_INT_FOD) {
        if (!chip->is_reverse_closing) {
            mca_log_err("RTX_INT_FOD!\n");
            sc96281_set_reverse_chg_mode(chip, false);
            chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
            need_report = true;
            mca_charge_mievent_report(CHARGE_DFX_WLS_TRX_FOD, NULL, 0);
        }

    } else if (int_flag & RTX_INT_EPT_PKT) {
        mca_log_err("RTX_INT_EPT_PKT!\n");
        schedule_delayed_work(&chip->reverse_transfer_timeout_work, msecs_to_jiffies(REVERSE_TRANSFER_TIMEOUT_TIMER));

    } else if (int_flag & RTX_INT_ERR_CODE) {
        sc96281_get_trx_err_code(chip, &err_code, &dfx_code);
        mca_log_err("RTX_INT_ERR_CODE! code=[%d %d]!\n", err_code, dfx_code);
        switch (dfx_code) {
        case CHARGE_DFX_WLS_TRX_OCP:
            mca_charge_mievent_report(CHARGE_DFX_WLS_TRX_OCP, NULL, 0);
            break;
        default:
            break;
        }
    } else {
        sc96281_log_info("tx mode unknown interrupt: 0x%x\n", int_flag);
    }

    if (need_report) {
        len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_REVERSE_CHG_STATE=%d", chip->is_reverse_chg);
        event_data.event = event;
        event_data.event_len = len;
        mca_event_report_uevent(&event_data);
    }

    return;
}


//-------------------sc96281 RX interface-----------------------
static int sc96281_rx_set_cmd(struct sc96281_chg *chip, uint32_t cmd)
{
    int ret;

    ret = write_cust(chip, cust_rx.cmd, &cmd);
    if (ret)
        sc96281_log_err("sc96281 rx set cmd %d fail\n", cmd);

    return ret;
}

static int sc96281_rx_set_cust_cmd(struct sc96281_chg *chip, uint32_t cmd)
{
    int ret = 0;

    ret = write_cust(chip, cust_rx.mi_ctx.cmd, &cmd);
    if (ret)
        sc96281_log_err("sc96281 rx set cust cmd %d fail\n", cmd);

    return ret;
}

static int sc96281_rx_recv_fsk_pkt(struct sc96281_chg *chip, fsk_pkt_t *fsk)
{
    int ret;

    ret = read_cust(chip, cust_rx.fsk_pkt, fsk);
    if (ret) {
        sc96281_log_err("sc96281 read recv fsk pkt fail\n");
    }

    return ret;
}

static int sc96281_rx_send_ask_pkt(struct sc96281_chg *chip, ask_pkt_t *ask, bool is_resp)
{
    int ret = 0;

    ret = write_cust(chip, cust_rx.ask_pkt, ask);
    if (ret) {
        sc96281_log_err("sc96281 write send ask pkt 0x%x fail\n", ask->header);
    }

    if (is_resp)
        ret = sc96281_rx_set_cust_cmd(chip, CUST_CMD_RX_TP_SEND);
    else
        ret = sc96281_rx_set_cmd(chip, AP_CMD_SEND_PPP);

    if (ret) {
        sc96281_log_err("sc96281 set cmd fail\n");
    } else {
        sc96281_log_info("sc96281 set cmd ask 0x%X %X %X %X %X %X %X %X",
            ask->header, ask->msg[0], ask->msg[1], ask->msg[2],
            ask->msg[3], ask->msg[4], ask->msg[5], ask->msg[6]);
    }

    return ret;
}

static void sc96281_rx_power_good_update(struct sc96281_chg *chip)
{
    int ret = 0;

    if (gpio_is_valid(chip->power_good_gpio)) {
        ret = gpio_get_value(chip->power_good_gpio);
        if (ret) {
            chip->power_good_flag = 1;
        } else {
            chip->power_good_flag = 0;
        }
        sc96281_log_info("power good flag update to %d\n", chip->power_good_flag);
    }
}

static void sc96281_rx_power_off_err(struct sc96281_chg *chip)
{
    return;
}

static int sc96281_set_enable_mode(struct sc96281_chg *chip, bool enable)
{
    int ret = 0;
    int gpio_val = 0;
    int sleep = !!enable;

    if (gpio_is_valid(chip->rx_sleep_gpio)) {
        ret = gpio_request(chip->rx_sleep_gpio, "rx-sleep-gpio");
        if (ret) {
            mca_log_err("unable to request rx_sleep_gpio [%d]\n", chip->rx_sleep_gpio);
        }
        ret = gpio_direction_output(chip->rx_sleep_gpio, !sleep);
        if (ret) {
            mca_log_err("cannot set direction for rx_sleep_gpio [%d]\n", chip->rx_sleep_gpio);
        }
        gpio_val = gpio_get_value(chip->rx_sleep_gpio);
        chip->enable_flag = (gpio_val == 0) ? 1 : 0;
        mca_log_err("rx_sleep_gpio val is :%d\n", gpio_val);
        gpio_free(chip->rx_sleep_gpio);
    }

    return ret;
}

static void sc96281_set_fod(struct sc96281_chg *chip, struct fod_params_t *params_base)
{
    int ret = 0;

    ret = write_cust(chip, cust_rx.bpp_fod, params_base->params);
    ret |= write_cust(chip, cust_rx.epp_fod, (uint8_t *)params_base->params + sizeof(cust_rx.bpp_fod));
    ret |= sc96281_rx_set_cust_cmd(chip, CUST_CMD_FOD_16_SEGMENT);
    if (ret)
        sc96281_log_err("sc96281 set 16-segment fod fail\n");
    else
        sc96281_log_info("sc96281 set 16-segment fod success\n");

    return;
}

static void sc96281_set_fod_params(struct sc96281_chg *chip)
{
    int uuid = 0, i = 0, j = 0;
    bool found = true;

    uuid |= chip->uuid[0] << 24;
    uuid |= chip->uuid[1] << 16;
    uuid |= chip->uuid[2] << 8;
    uuid |= chip->uuid[3];
    sc96281_log_info("uuid: 0x%x,0x%x,0x%x,0x%x\n", chip->uuid[0], chip->uuid[1], chip->uuid[2], chip->uuid[3]);

    for (i = 0; i < chip->fod_params_size; i++) {
        found = true;
        if (uuid != chip->fod_params[i].uuid) {
            found = false;
            continue;
        }
        /* found fod by uuid */
        if (found) {
            sc96281_log_info("found fod params\n");
            for(j = 0; j < chip->fod_params[i].length; j++)
                sc96281_log_info("fod gain:%d offset:%d\n", chip->fod_params[i].params[j].gain, chip->fod_params[i].params[j].offset);
            sc96281_set_fod(chip, &chip->fod_params[i]);
            return;
        }
    }

    if (!found) {
        sc96281_log_info("can not found fod params, use default fod\n");
        sc96281_set_fod(chip, &chip->fod_params_default);
    }

    return;
}

static int sc96281_set_debug_fod_params(struct sc96281_chg *chip)
{
    struct fod_params_t fod_params;
    int index = chip->wls_debug_one_fod_index;
    int i = 0, k = 0;
    int uuid = 0;
    bool found = false;

    if (!chip->power_good_flag) {
        sc96281_log_err("power good flag is false\n");
        return -1;
    }

    if (chip->wls_debug_set_fod_type == WLS_DEBUG_EPP_FOD_ALL_DIRECTLY) {
        fod_params.type = chip->wls_debug_all_fod_params->type;
        fod_params.length = chip->wls_debug_all_fod_params->length;
        for (i = 0; i < fod_params.length; i++) {
            fod_params.params[i].gain = chip->wls_debug_all_fod_params->params[i].gain;
            fod_params.params[i].offset = chip->wls_debug_all_fod_params->params[i].offset;
        }
        sc96281_set_fod(chip, &fod_params);
        return 0;
    }

    uuid |= chip->uuid[0] << 24;
    uuid |= chip->uuid[1] << 16;
    uuid |= chip->uuid[2] << 8;
    uuid |= chip->uuid[3];
    //check all params
    for (i = 0; i < chip->fod_params_size; i++) {
        found = true;
        // uuid checking
        if (uuid != chip->fod_params[i].uuid) {
            found = false;
            continue;
        }
        // found
        if (found) {
            sc96281_log_info("uuid: 0x%x,0x%x,0x%x,0x%x\n", chip->uuid[0], chip->uuid[1], chip->uuid[2], chip->uuid[3]);
            if (chip->wls_debug_set_fod_type == WLS_DEBUG_EPP_FOD_ALL &&
                    chip->fod_params[i].length == chip->wls_debug_all_fod_params->length) {
                for (k = 0; k < chip->fod_params[i].length; k++) {
                    chip->fod_params[i].params[k].gain = chip->wls_debug_all_fod_params->params[k].gain;
                    chip->fod_params[i].params[k].offset = chip->wls_debug_all_fod_params->params[k].offset;
                }
            } else if (chip->wls_debug_set_fod_type == WLS_DEBUG_EPP_FOD_SINGLE &&
                    index < chip->fod_params[i].length) {
                chip->fod_params[i].params[index].gain = chip->wls_debug_one_fod_param.gain;
                chip->fod_params[i].params[index].offset = chip->wls_debug_one_fod_param.offset;
            }
            sc96281_set_fod(chip, &chip->fod_params[i]);
            return 0;
        }
    }

    if (chip->adapter_type >= ADAPTER_XIAOMI_QC3) {
        found = true;
        sc96281_log_info("fod epp+ default\n");
        if (chip->wls_debug_set_fod_type == WLS_DEBUG_EPP_FOD_ALL &&
                chip->fod_params_default.length == chip->wls_debug_all_fod_params->length) {
            for (k = 0; k < chip->fod_params_default.length; k++) {
                chip->fod_params_default.params[k].gain = chip->wls_debug_all_fod_params->params[k].gain;
                chip->fod_params_default.params[k].offset = chip->wls_debug_all_fod_params->params[k].offset;
            }
        } else if (chip->wls_debug_set_fod_type == WLS_DEBUG_EPP_FOD_SINGLE &&
                    index < chip->fod_params_default.length) {
            chip->fod_params_default.params[index].gain = chip->wls_debug_one_fod_param.gain;
            chip->fod_params_default.params[index].offset = chip->wls_debug_one_fod_param.offset;
        }
        sc96281_set_fod(chip, &chip->fod_params_default);
    }

    if (!found)
        sc96281_log_info("can not found fod params, uuid: 0x%x,0x%x,0x%x,0x%x\n", chip->uuid[0], chip->uuid[1], chip->uuid[2], chip->uuid[3]);

    return 0;
}

static int sc96281_set_debug_fod(struct sc96281_chg *chip, int *args, int count)
{
    uint8_t index = 0;

    switch (args[0]) {
    case WLS_DEBUG_EPP_FOD_SINGLE:
        sc96281_log_info("set one epp fod\n");
        chip->wls_debug_set_fod_type = WLS_DEBUG_EPP_FOD_SINGLE;
        index = args[1];
        chip->wls_debug_one_fod_index = index;
        chip->wls_debug_one_fod_param.gain = args[2];
        chip->wls_debug_one_fod_param.offset = args[3];
        sc96281_set_debug_fod_params(chip);
        break;
    case WLS_DEBUG_EPP_FOD_ALL:
        sc96281_log_info("set all epp fod\n");
        if (chip->wls_debug_all_fod_params == NULL)
            chip->wls_debug_all_fod_params = kmalloc(sizeof(struct fod_params_t), GFP_KERNEL);
        chip->wls_debug_set_fod_type = WLS_DEBUG_EPP_FOD_ALL;
        chip->wls_debug_all_fod_params->length = count / 2;
        for (index = 0; index < chip->wls_debug_all_fod_params->length; ++index) {
            chip->wls_debug_all_fod_params->params[index].gain = args[index * 2 + 1];
            chip->wls_debug_all_fod_params->params[index].offset = args[index * 2 + 2];
        }
        sc96281_set_debug_fod_params(chip);
        break;
    case WLS_DEBUG_EPP_FOD_ALL_DIRECTLY:
        sc96281_log_info("wls debug set all fod\n");
        if (chip->wls_debug_all_fod_params == NULL)
            chip->wls_debug_all_fod_params = kmalloc(sizeof(struct fod_params_t), GFP_KERNEL);
        chip->wls_debug_set_fod_type = WLS_DEBUG_EPP_FOD_ALL_DIRECTLY;
        chip->wls_debug_all_fod_params->type = args[1];
        chip->wls_debug_all_fod_params->length = count / 2 - 1;
        for (index = 0; index < chip->wls_debug_all_fod_params->length; ++index) {
            chip->wls_debug_all_fod_params->params[index].gain = args[index * 2 + 2];
            chip->wls_debug_all_fod_params->params[index].offset = args[index * 2 + 3];
        }
        sc96281_set_debug_fod_params(chip);
        break;
    default:
        sc96281_log_err("not support debug_fod_type, return\n");
        break;
    }

    return 0;
}

static int sc96281_set_adapter_voltage(struct sc96281_chg *chip, int voltage)
{
    int ret = 0;

    if ((voltage < ADAPTER_VOL_MIN_MV) || (voltage > ADAPTER_VOL_MAX_MV))
        voltage = ADAPTER_VOL_DEFAULT_MV;

    ret = write_cust(chip, cust_rx.mi_ctx.fc_volt, &voltage);
    if (ret) {
        sc96281_log_err("wls_set_adapter_voltage to %dmV fail\n", voltage);
        return ret;
    }
    ret = sc96281_rx_set_cust_cmd(chip, CUST_CMD_RX_FAST_CHARGE);

    sc96281_log_info("wls_set_adapter_voltage to %dmV success\n", voltage);

    return ret;
}

static int sc96281_get_cep(struct sc96281_chg * chip, int *cep)
{
    int ret = 0;
    int8_t cep_value = 0;

    ret = read_cust(chip, cust_rx.cep_value, &cep_value);
    if (ret) {
        sc96281_log_err("sc96281 get rx value fail\n");
        cep_value = 0;
    }

    *cep = (int)cep_value;
    sc96281_log_info("get rx cep: %d\n", *cep);

    return ret;
}

static int sc96281_set_vout(struct sc96281_chg * chip, int vout)
{
    int ret = 0;
    int cep = 0;
    uint16_t vout_value = 0;

    if (!chip->power_good_flag) {
        sc96281_log_info("power good off, don't set vout\n");
        return -1;
    }

    mutex_lock(&chip->data_transfer_lock);
    msleep(20);

    if (chip->parallel_charge) {
        ret = sc96281_get_cep(chip, &cep);
        if (ret < 0) {
            sc96281_log_info("get cep failed : %d\n", ret);
            goto exit_set_vout;
        } else if (ABS(cep) > ABS_CEP_VALUE && chip->vout_setted <= vout) {
            sc96281_log_info("[%s] vout: %d, cep: %d, not set vout\n", __func__, vout, cep);
            ret = -1;
            goto exit_set_vout;
        }
    }

    vout = (vout < VOUT_SET_MIN_MV)? VOUT_SET_DEFAULT_MV : vout;
    vout = (vout > VOUT_SET_MAX_MV)? VOUT_SET_MAX_MV : vout;
    vout_value = (uint16_t)vout;
    ret = write_cust(chip, cust_rx.target_vout, &vout_value);
    if (ret) {
        sc96281_log_err("sc96281 set vout fail\n");
        goto exit_set_vout;
    }
    ret = sc96281_rx_set_cmd(chip, AP_CMD_RX_VOUT_CHANGE);
    if (ret) {
        sc96281_log_err("sc96281 set cmd fail\n");
        goto exit_set_vout;
    }

    chip->vout_setted = vout;
    sc96281_log_info("set rx vout: %d cep=%d parallel_charge=%d\n", vout, cep, chip->parallel_charge);

exit_set_vout:
    mutex_unlock(&chip->data_transfer_lock);
    return ret;
}

static int sc96281_get_vrect(struct sc96281_chg * chip, int *vrect)
{
    int ret = 0;
    uint16_t temp_val = 0;

    if (!chip->power_good_flag) {
        sc96281_log_info("power good off, cannot get vrect\n");
        *vrect = 0;
        return ret;
    }

    ret = read_cust(chip, cust_rx.vrect, &temp_val);
    if (ret) {
        sc96281_log_err("sc96281 get vrect fail\n");
    } else {
        sc96281_log_info("sc96281 get vrect: %d\n", temp_val);
        *vrect = (int)temp_val;
    }

    return ret;
}

static int sc96281_get_vout(struct sc96281_chg * chip, int *vout)
{
    int ret = 0;
    uint16_t temp_val = 0;

    if (!chip->power_good_flag) {
        sc96281_log_info("power good off, cannot get vout\n");
        *vout = 0;
        return ret;
    }

    ret = read_cust(chip, cust_rx.vout, &temp_val);
    if (ret) {
        sc96281_log_err("sc96281 get vout fail\n");
    } else {
        sc96281_log_info("sc96281 get vout: %d\n", temp_val);
        *vout = (int)temp_val;
    }

    return ret;
}

static int sc96281_get_iout(struct sc96281_chg * chip, int *iout)
{
    int ret = 0;
    uint16_t temp_val = 0;

    if (!chip->power_good_flag) {
        sc96281_log_info("power good off, cannot get iout\n");
        *iout = 0;
        return ret;
    }

    ret = read_cust(chip, cust_rx.iout, &temp_val);
    if (ret) {
        sc96281_log_err("sc96281 get iout fail\n");
    } else {
        sc96281_log_info("sc96281 get iout: %d\n", temp_val);
        *iout = (int)temp_val;
    }

    return ret;
}

static int sc96281_get_temp(struct sc96281_chg * chip, int *temp)
{
    int ret = 0;
    uint16_t temp_val = 0;

    if (!chip->power_good_flag) {
        sc96281_log_info("power good off, cannot get temp\n");
        *temp = 0;
        return ret;
    }

    ret = read_cust(chip, cust_rx.t_die, &temp_val);
    if (ret) {
        sc96281_log_err("sc96281 get temp fail\n");
    } else {
        sc96281_log_info("sc96281 get temp: %d\n", temp_val);
        *temp = (int)temp_val / 10;
    }

    return ret;
}

static int sc96281_get_rx_err_code(struct sc96281_chg *chip, uint32_t *err_code, uint32_t *dfx_code)
{
    int ret = 0;
    uint32_t code = 0;

    ret = read_cust(chip, cust_rx.err_code, &code);
    if (ret) {
        sc96281_log_err("get rx error code fail\n");
        *err_code = 0;
        return ret;
    }

    switch (code) {
        case ERROR_CODE_RX_AP_CMD: sc96281_log_info("rx error AP_CMD\n"); break;
        case ERROR_CODE_RX_AC_LOSS: sc96281_log_info("rx error AC_LOSS\n"); break;
        case ERROR_CODE_RX_SS_OVP: sc96281_log_info("rx error SS_OVP\n"); break;
        case ERROR_CODE_RX_VOUT_OVP: sc96281_log_info("rx error VOUT_OVP\n"); *dfx_code = CHARGE_DFX_WLS_RX_OVP; break;
        case ERROR_CODE_RX_OVP_SUSTAIN: sc96281_log_info("rx error OVP_SUSTAIN\n"); break;
        case ERROR_CODE_RX_OCP_ADC: sc96281_log_info("rx error OCP_ADC\n"); break;
        case ERROR_CODE_RX_OCP_HARD: sc96281_log_info("rx error OCP_HARD\n"); *dfx_code = CHARGE_DFX_WLS_RX_OCP; break;
        case ERROR_CODE_RX_SCP: sc96281_log_info("rx error SCP\n"); break;
        case ERROR_CODE_RX_OTP_HARD: sc96281_log_info("rx error OTP_HARD\n"); *dfx_code = CHARGE_DFX_WLS_RX_OTP; break;
        case ERROR_CODE_RX_OTP_110: sc96281_log_info("rx error OTP_110\n"); break;
        case ERROR_CODE_RX_NGATE_OVP: sc96281_log_info("rx error NGATE_OVP\n"); break;
        case ERROR_CODE_RX_LDO_OPP: sc96281_log_info("rx error LDO_OPP\n"); break;
        case ERROR_CODE_RX_SLEEP: sc96281_log_info("rx error SLEEP\n"); break;
        case ERROR_CODE_RX_HOP1: sc96281_log_info("rx error HOP1\n"); break;
        case ERROR_CODE_RX_HOP2: sc96281_log_info("rx error HOP2\n"); break;
        case ERROR_CODE_RX_HOP3: sc96281_log_info("rx error HOP3\n"); break;
        case ERROR_CODE_RX_VRECT_OVP: sc96281_log_info("rx error Vrect OVP\n"); break;
        default: sc96281_log_info("rx error code unknow\n"); break;
    }

    *err_code = code;
    return ret;
}

static int sc96281_set_cp_status(struct sc96281_chg * chip, int status)
{
    return 0;
}

static int sc96281_send_transparent_data(struct sc96281_chg *chip, uint8_t *send_data, uint8_t length)
{
    int ret = 0;
    ask_pkt_t ask;
    bool is_resp = false;

    sc96281_log_info("data[0]=0x%x, data[1]=0x%x, data[2]=0x%x, length=%d\n", send_data[0], send_data[1], send_data[2], length);

    if (send_data[0] == 0 || send_data[0] == 1) is_resp = true;
    memcpy(ask.buff, &send_data[1], length - 1); //compatible to existed code,ignore data[0]

    mutex_lock(&chip->data_transfer_lock);

    ret = sc96281_rx_send_ask_pkt(chip, &ask, is_resp);
    if (ret) {
        sc96281_log_err("send transparent data %d failed\n", length);
    } else {
        sc96281_log_info("send transparent data %d success\n", length);
        memcpy(chip->sent_pri_packet.buff, &send_data[1], length - 1);
    }

    mutex_unlock(&chip->data_transfer_lock);

    return ret;
}

static void sc96281_rcv_transparent_data(struct sc96281_chg *chip, u8 *rcv_value, u8 buff_len, u8 *rcv_len)
{
    int ret = 0;
    fsk_pkt_t fsk;
    int i = 0;
    int sent_len = 0, data_len = 0;
    int rcv_index = 0;
    bool is_resp = false;

    if (!chip->power_good_flag)
        return;

    ret = sc96281_rx_recv_fsk_pkt(chip, &fsk);
    if (ret) {
        sc96281_log_err("[%s] receive power reduce cmd failed\n", __func__);
        return;
    }

    sc96281_log_info("receive_transparent_data fsk=%X %X %X %X %X %X %X %X\n",
                 fsk.buff[0], fsk.buff[1], fsk.buff[2], fsk.buff[3],
                 fsk.buff[4], fsk.buff[5], fsk.buff[6], fsk.buff[7]);

    if ((chip->sent_pri_packet.header == 0x05 && fsk.header >= 0xF0 && fsk.header <= 0xF4)
        || (fsk.header == 0 || fsk.header == 0x33 || fsk.header == 0x55 || fsk.header == 0xFF)) {
        is_resp  = true;
        data_len = 1;
    } else {
        data_len = private_sizeof(fsk.header, fsk.msg[0]);
    }
    if (data_len == 0) data_len = sizeof_msg(fsk.header);
    sent_len = sizeof_msg(chip->sent_pri_packet.header) + 1;

    sc96281_log_info("receive_transparent_data data_length=%d\n", data_len);
    if (data_len > buff_len) {
        sc96281_log_err("receive_transparent_data buffer overflow\n");
        data_len = 0;
        return;
    }

    *rcv_len = (sent_len << 4) | (data_len);
    memcpy(&rcv_value[rcv_index], chip->sent_pri_packet.buff, sent_len);
    rcv_index += sent_len;

    if (is_resp) {
        rcv_value[rcv_index] = fsk.header;
        sc96281_log_info("receive_transparent_data fsk.header=0x%x\n", fsk.header);
    } else if (fsk.header == 0x2F && fsk.msg[0] == 0x62) {
        for (i = 0; i < data_len; i++) {
            rcv_value[rcv_index + i] = fsk.buff[i];
            sc96281_log_info("receive_transparent_data data[%d]=0x%x\n", rcv_index + i, rcv_value[rcv_index + i]);
        }
    } else {
        for (i = 0; i < data_len; i++) {
            rcv_value[rcv_index + i] = fsk.msg[i];
            sc96281_log_info("receive_transparent_data data[%d]=0x%x\n", rcv_index + i, rcv_value[rcv_index + i]);
        }
    }
    sc96281_log_info("receive_transparent_data rcv_len=0x%x\n", *rcv_len);
    return;
}

static int sc96281_update_soc_to_tx(struct sc96281_chg * chip, int soc)
{
#ifdef CONFIG_FACTORY_BUILD
    return 0;
#endif
    int ret = 0;
    uint8_t data[3] = {0};

    if (!chip->power_good_flag) {
        sc96281_log_info("power good off, don't update soc\n");
        return ret;
    }

    if (chip->adapter_type < ADAPTER_XIAOMI_QC3) {
        sc96281_log_info("adapter type is %d, don't update soc\n", chip->adapter_type);
        return ret;
    }

    chip->mutex_lock_sts = true;
    data[0] = 0x00;
    data[1] = 0x05;
    data[2] = (u8)(soc & 0xFF);
    ret = sc96281_send_transparent_data(chip, data, ARRAY_SIZE(data));
    if (ret) {
        sc96281_log_err("update soc to tx fail\n");
    } else {
        chip->current_trans_packet_type = WLS_SOC_PACKET;
        sc96281_log_info("update soc: %d\n", soc);
    }

    schedule_delayed_work(&chip->mutex_unlock_work, msecs_to_jiffies(1200));
    return ret;
}

static int sc96281_update_q_value_strategy_to_tx(struct sc96281_chg * chip, uint8_t q_value)
{
    int ret = 0;
    uint8_t send_value[4] = {0x01, 0x28, 0x62, 0};

    if (!chip->power_good_flag) {
        sc96281_log_info("power good off, don't update soc\n");
        return ret;
    }

    chip->mutex_lock_sts = true;
    send_value[3] = (uint8_t)(q_value & 0xFF);
    ret = sc96281_send_transparent_data(chip, send_value, ARRAY_SIZE(send_value));
    if (ret)
        sc96281_log_err("update q_value_strategy to tx fail\n");
    else {
        chip->current_trans_packet_type = WLS_Q_STARTEGY_PACKET;
        sc96281_log_info("%s: {0x%02x, 0x%02x, 0x%02x, 0x%02x}\n",
                __func__, send_value[0], send_value[1], send_value[2], send_value[3]);
    }

    schedule_delayed_work(&chip->mutex_unlock_work, msecs_to_jiffies(1500));
    return ret;
}

static int sc96281_send_frequency_to_tx(struct sc96281_chg *chip, int freq_khz)
{
    int ret = 0;
    uint8_t send_value[4] = {0x00, 0x28, 0xd3, 0};

    if (!chip->power_good_flag) {
        sc96281_log_err("%s power good off\n", __func__);
        return -1;
    }

    if (freq_khz < SUPER_TX_FREQUENCY_MIN_KHZ || freq_khz > SUPER_TX_FREQUENCY_MAX_KHZ) {
        sc96281_log_err("%s freq %d invalid.\n", __func__, freq_khz);
        return -1;
    }

    chip->mutex_lock_sts = true;
    send_value[3] = (uint8_t)(freq_khz & 0xFF);
    ret = sc96281_send_transparent_data(chip, send_value, ARRAY_SIZE(send_value));
    if (ret) {
        sc96281_log_err("%s fail\n", __func__);
    } else {
        sc96281_log_info("%s %d success\n", __func__, freq_khz);
        chip->current_trans_packet_type = WLS_FREQUENCE_PACKET;
    }

    schedule_delayed_work(&chip->mutex_unlock_work, msecs_to_jiffies(1200));
    return ret;
}

static int sc96281_send_vout_range_to_tx(struct sc96281_chg *chip, int max_volt_mv)
{
    int ret = 0;
    uint8_t send_value[4] = {0x00, 0x28, 0xd6, 0};

    if (!chip->power_good_flag) {
        sc96281_log_err("%s power good off\n", __func__);
        return -1;
    }

    if (max_volt_mv < SUPER_TX_VOUT_MIN_MV || max_volt_mv > SUPER_TX_VOUT_MAX_MV) {
        sc96281_log_err("%s max_volt %d invalid.\n", __func__, max_volt_mv);
        return -1;
    }

    chip->mutex_lock_sts = true;
    send_value[3] = (uint8_t)((max_volt_mv - SUPER_TX_VOUT_MIN_MV) / 500);
    ret = sc96281_send_transparent_data(chip, send_value, ARRAY_SIZE(send_value));
    if (ret) {
        sc96281_log_err("%s fail\n", __func__);
    } else {
        sc96281_log_info("%s %d success\n", __func__, max_volt_mv);
        chip->current_trans_packet_type = WLS_VOUT_RANGE_PACKET;
    }

    schedule_delayed_work(&chip->mutex_unlock_work, msecs_to_jiffies(1200));
    return ret;
}

static int sc96281_send_fan_speed_to_tx(struct sc96281_chg *chip, int value)
{
    int ret = 0;
    ask_pkt_t ask;

    if (!chip->power_good_flag) {
        sc96281_log_err("%s power good off\n", __func__);
        return -1;
    }

    if (value < WLS_TX_FAN_SPEED_MIN || value > WLS_TX_FAN_SPEED_MAX) {
        mca_log_err("fan speed %d invalid.\n", value);
        return -1;
    }

    chip->mutex_lock_sts = true;
    chip->tx_speed = value;
    ask.header = 0x38;
    ask.msg[0] = PRIVATE_CMD_CTRL_PRI_PKT;
    ask.msg[1] = value;
    ask.msg[2] = 0;

    mutex_lock(&chip->data_transfer_lock);
    ret = sc96281_rx_send_ask_pkt(chip, &ask, false);
    if (ret) {
        mca_log_err("set fan speed %d failed\n", value);
    } else {
        mca_log_err("set fan speed %d success\n", value);
    }
    mutex_unlock(&chip->data_transfer_lock);

    schedule_delayed_work(&chip->mutex_unlock_work, msecs_to_jiffies(1500));
    return ret;
}

static int sc96281_check_i2c(struct sc96281_chg *chip)
{
    int ret = 0;
    uint8_t data = 0x55;

    ret = write_cust(chip, cust_rx.iic_check, &data);
    if (ret) {
        sc96281_log_err("sc96281 check i2c write fail\n");
        return -1;
    }

    msleep(20);

    ret = read_cust(chip, cust_rx.iic_check, &data);
    if (ret < 0) {
        sc96281_log_err("sc96281 check i2c read fail\n");
        return -1;
    }

    if (data == 0x55) {
        sc96281_log_info("i2c check ok!\n");
    } else {
        sc96281_log_err("i2c check failed!\n");
        return -1;
    }

    return 1;
}

int sc96281_rx_get_int(struct sc96281_chg *chip, uint32_t *rxint)
{
    int ret;

    ret = read_cust(chip, cust_rx.irq_flag, rxint);
    if (ret) {
        sc96281_log_err("sc96281 clear rx int fail\n");
    }

    return ret;
}

int sc96281_rx_clr_int(struct sc96281_chg *chip, uint32_t rxint)
{
    int ret;

    mutex_lock(&chip->data_transfer_lock);
    ret = write_cust(chip, cust_rx.irq_clr, &rxint);
    if (ret) {
        sc96281_log_err("sc96281 clear rx int fail\n");
    }
    mutex_unlock(&chip->data_transfer_lock);

    return ret;
}

uint16_t sc96281_rx_remap_int(struct sc96281_chg *chip, uint32_t rxint)
{
    int i = 0;
    uint16_t val = 0;

    for (i = 0; i < ARRAY_SIZE(rx_irq_map); i++) {
        if (rx_irq_map[i].irq_regval & rxint) {
            val |= rx_irq_map[i].irq_flag;
        }
    }

    return val;
}

static void sc96281_epp_uuid_func(struct sc96281_chg *chip)
{
    u8 vendor = 0, module = 0, version = 0, power = 0;
    vendor  = chip->uuid[0];
    module  = chip->uuid[1];
    version = chip->uuid[2];
    power   = chip->uuid[3];
    sc96281_log_info("epp uuid: vendor:0x%x, module:0x%x, version:0x%x, power:0x%x", vendor, module, version, power);

    if ((vendor == 0x9) && (module == 0x8) && (version == 0x6) && (power == 0x7))
        chip->is_music_tx = true;
    if (((vendor == 0x9) && (module == 0x1) && (version == 0x5) && (power == 0x6))
        || ((vendor == 0xc) && (module == 0x9) && (version == 0x9) && (power == 0x8))
        || ((vendor == 0xc) && (module == 0x9) && (version == 0x9) && (power == 0x6)))
        chip->is_plate_tx = true;
    if (((vendor == 0x6) && (module == 0x2) && (version == 0x8) && (power == 0x1))
        || ((vendor == 0x1) && (module == 0x8) && (version == 0x2) && (power == 0x5))
        || ((vendor == 0x9) && (module == 0x8) && (version == 0x2) && (power == 0x7))
        || ((vendor == 0x9) && (module == 0xC) && (version == 0x2) && (power == 0x1)))
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
        sc96281_log_info("[TODO] is car tx");
        //TODO set wls car adapter node to 1
    }
    if (chip->is_music_tx)
        chip->adapter_type = ADAPTER_VOICE_BOX;
    return;
}

static void sc96281_process_factory_cmd(struct sc96281_chg *chip, u8 cmd)
{
    int ret = 0;
    u8 send_data[8] = {0};
    u8 data_h = 0, data_l = 0;
    u8 index = 0;
    int rx_iout = 0, rx_vout = 0;

    switch (cmd) {
    case FACTORY_TEST_CMD_RX_IOUT:
        ret = sc96281_get_iout(chip, &rx_iout);
        if (ret >= 0) {
            data_h = (rx_iout & 0x00ff);
            data_l = (rx_iout & 0xff00) >> 8;
        }
        index = 0;
        send_data[index++] = TX_ACTION_NO_REPLY;
        send_data[index++] = TRANS_DATA_LENGTH_3BYTE;
        send_data[index++] = cmd;
        send_data[index++] = data_h;
        send_data[index++] = data_l;
        ret = sc96281_send_transparent_data(chip, send_data, index);
        if (ret < 0)
            mca_log_err("send transparent data failed\n");
        mca_log_err("rx_iout: 0x%x, 0x%x, iout = %d\n", data_h, data_l, rx_iout);
        break;
    case FACTORY_TEST_CMD_RX_VOUT:
        ret = sc96281_get_vout(chip, &rx_vout);
        if (ret >= 0) {
            data_h = (rx_vout & 0x00ff);
            data_l = (rx_vout & 0xff00) >> 8;
        }
        index = 0;
        send_data[index++] = TX_ACTION_NO_REPLY;
        send_data[index++] = TRANS_DATA_LENGTH_3BYTE;
        send_data[index++] = cmd;
        send_data[index++] = data_h;
        send_data[index++] = data_l;
        ret = sc96281_send_transparent_data(chip, send_data, index);
        if (ret < 0)
            mca_log_err("send transparent data failed\n");
        mca_log_err("rx_vout: 0x%x, 0x%x, iout = %d\n", data_h, data_l, rx_vout);
        break;
    case FACTORY_TEST_CMD_RX_FW_ID:
        index = 0;
        send_data[index++] = TX_ACTION_NO_REPLY;
        send_data[index++] = TRANS_DATA_LENGTH_5BYTE;
        send_data[index++] = cmd;
        send_data[index++] = 0x0;
        send_data[index++] = 0x0;
        send_data[index++] = g_wls_fw_data.fw_rx_id;
        send_data[index++] = g_wls_fw_data.fw_tx_id;
        ret = sc96281_send_transparent_data(chip, send_data, index);
        if (ret < 0)
            mca_log_err("send transparent data failed\n", __func__);
        mca_log_err("fw_version: 0x%x0x%x\n", g_wls_fw_data.fw_rx_id, g_wls_fw_data.fw_tx_id);
        break;
    case FACTORY_TEST_CMD_RX_CHIP_ID:
        index = 0;
        send_data[index++] = TX_ACTION_NO_REPLY;
        send_data[index++] = TRANS_DATA_LENGTH_3BYTE;
        send_data[index++] = cmd;
        send_data[index++] = 16;
        send_data[index++] = 51;
        ret = sc96281_send_transparent_data(chip, send_data, index);
        if (ret < 0)
            mca_log_err("send transparent data failed\n");
        mca_log_err("chip id: 0x%x0x%x\n", g_wls_fw_data.hw_id_h, g_wls_fw_data.hw_id_l);
        break;
    case FACTORY_TEST_CMD_ADAPTER_TYPE:
        index = 0;
        send_data[index++] = TX_ACTION_NO_REPLY;
        send_data[index++] = TRANS_DATA_LENGTH_2BYTE;
        send_data[index++] = cmd;
        send_data[index++] = chip->adapter_type;
        ret = sc96281_send_transparent_data(chip, send_data, index);
        if (ret < 0)
            mca_log_err("send transparent data failed\n");
        mca_log_err("adapter type: %d\n", chip->adapter_type);
        break;
    case FACTORY_TEST_CMD_REVERSE_REQ:
        index = 0;
        send_data[index++] = TX_ACTION_NO_REPLY;
        send_data[index++] = TRANS_DATA_LENGTH_1BYTE;
        send_data[index++] = cmd;
        ret = sc96281_send_transparent_data(chip, send_data, index);
        if (ret < 0)
            mca_log_err("send transparent data failed\n");
        chip->revchg_test_status = REVERSE_TEST_SCHEDULE;
        mca_log_err("reverse charge start\n");
        break;
    default:
        mca_log_err("unknown cmd: %d\n", cmd);
        break;
    }
    return;
}

static void sc96281_rcv_factory_test_cmd(struct sc96281_chg *chip, u8 *rev_data, u8 *length)
{
    int ret = 0;
    uint8_t i;
    fsk_pkt_t fsk;

    if (!chip->power_good_flag)
        return;

    ret = sc96281_rx_recv_fsk_pkt(chip, &fsk);
    if (ret) {
        sc96281_log_err("[%s] receive factory test cmd failed\n", __func__);
        return;
    }

    sc96281_log_info("receive_transparent_data fsk=%X %X %X %X %X %X %X %X\n",
            fsk.buff[0], fsk.buff[1], fsk.buff[2], fsk.buff[3],
            fsk.buff[4], fsk.buff[5], fsk.buff[6], fsk.buff[7]);

    if (fsk.header == 0 || fsk.header == 0x33 || fsk.header == 0x55 || fsk.header == 0xFF)
        *length = 1;
    else if(fsk.header < 0x20)
        *length = 3;
    else if (fsk.header < 0x80)
        *length = 4 + ((fsk.header - 0x20) >> 4);
    else if (fsk.header < 0xe0)
        *length = 10 + ((fsk.header - 0x80) >> 3);
    else
        *length = 22 + ((fsk.header - 0xe0) >> 2);

    sc96281_log_info("data_length=%d\n", *length);

    for (i = 0; i < *length; i++) {
        rev_data[i] = fsk.buff[i];
        sc96281_log_info("receive_product_test_cmd i=%d, data[i]=0x%x\n", i, rev_data[i]);
    }

    return;
}

static int sc96281_get_rx_rtx_mode(struct sc96281_chg *chip, u8 *mode)
{
    int ret = 0;
    uint32_t rx_mode;

    ret = read_cust(chip, cust_rx.mode, &rx_mode);
    if (ret) {
        sc96281_log_err("wls_get_ex_rtx_mode fail\n");
        *mode = RX_MODE;
        return ret;
    } else {
        if (rx_mode & WORK_MODE_TX)
            *mode = RTX_MODE;
        else
            *mode = RX_MODE;
    }

    sc96281_log_info("get_rx_rtx_mode %d\n", *mode);
    return ret;
}

static u8 sc96281_get_rx_power_mode(struct sc96281_chg *chip)
{
    int ret = 0;
    uint32_t rx_mode;
    uint8_t pwr_mode = BPP_MODE;

    ret = read_cust(chip, cust_rx.mode, &rx_mode);
    if (ret) {
        sc96281_log_err("wls_get_rx_pwrmode fail\n");
        return pwr_mode;
    } else {
        if (rx_mode & WORK_MODE_EPP)
            pwr_mode = EPP_MODE;
        else
            pwr_mode = BPP_MODE;
    }

    sc96281_log_info(" get rx pwrmode %d\n", pwr_mode);
    return pwr_mode;
}

static u8 sc96281_get_fastchg_result(struct sc96281_chg *chip)
{
    int ret = 0;
    uint32_t func_flag = 0;
    uint8_t fastchg_result = 0;

    ret = read_cust(chip, cust_rx.mi_ctx.fun_flag, &func_flag);
    if (ret) {
        sc96281_log_err("wls_get_fastchg_result fail\n");
        return fastchg_result;
    } else {
        if (func_flag & PROJECT_FLAG_FAST_CHARGE)
            fastchg_result = 1;
        else
            fastchg_result = 0;
    }

    sc96281_log_info(" get fastchg_result %d\n", fastchg_result);
    return fastchg_result;
}

static u8 sc96281_get_auth_value(struct sc96281_chg *chip)
{
    int ret = 0;
    uint32_t auth_data = 0, uuid = 0;
    uint8_t adapter_type = ADAPTER_NONE;

    if (!chip->power_good_flag)
        return 0;

    ret = read_cust(chip, cust_rx.mi_ctx.fun_flag, &auth_data);
    if (ret) {
        sc96281_log_err("fail to get fun_flag, ret = %d\n", ret);
        return 0;
    } else {
        sc96281_log_info("auth_data = 0x%x\n", auth_data);
    }

    ret = read_cust(chip, cust_rx.mi_ctx.adapter_type, &adapter_type);
    if (!ret)
        chip->adapter_type = (adapter_type != ADAPTER_NONE)? adapter_type : ADAPTER_AUTH_FAILED;

    if (auth_data & PROJECT_FLAG_UUID) {
        ret = read_cust(chip, cust_rx.mi_ctx.tx_uuid, &uuid);
        if (!ret) {
            chip->uuid[0] = (uuid >> 24) & 0xFF;
            chip->uuid[1] = (uuid >> 16) & 0xFF;
            chip->uuid[2] = (uuid >> 8) & 0xFF;
            chip->uuid[3] = uuid & 0xFF;
        }
    }

    sc96281_log_info("adapter type: %d\n", chip->adapter_type);
    sc96281_log_info("uuid: 0x%x, 0x%x, 0x%x, 0x%x\n", chip->uuid[0], chip->uuid[1], chip->uuid[2], chip->uuid[3]);

    return (uint8_t)auth_data;
}

static void sc96281_do_renego(struct sc96281_chg *chip, u8 max_power)
{
    int ret = 0;

    sc96281_log_info("max_power = %d\n", max_power);

    if (!chip->power_good_flag)
        return;

    max_power *= 2;
    ret = write_cust(chip, cust_rx.mi_ctx.reneg_param, &max_power);
    if (ret) {
        sc96281_log_err("renegotiate write parameter failed\n");
        return;
    }

    ret = sc96281_rx_set_cust_cmd(chip, CUST_CMD_RX_RENEG);
    if (ret) {
        sc96281_log_err("renegotiate cmd failed\n");
        return;
    }

    return;
}

static void sc96281_start_renego(struct sc96281_chg *chip)
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
        sc96281_do_renego(chip, max_power);
    return;
}

static void sc96281_adapter_handle(struct sc96281_chg *chip)
{
    mca_log_err("adapter:%d, epp:%d\n", chip->adapter_type, chip->epp);

    if (!chip->fc_flag) {
        switch (chip->adapter_type) {
        case ADAPTER_SDP:
        case ADAPTER_CDP:
        case ADAPTER_DCP:
        case ADAPTER_QC2:
            sc96281_log_info("SDP/CDP/DCP/QC2 adapter set iwls 750mA\n");
            chip->pre_curr = 750;
            sc96281_set_pmic_icl(chip, 750);
            sc96281_set_pmic_fcc(chip, 1000);
            break;
        case ADAPTER_QC3:
        case ADAPTER_PD:
        case ADAPTER_AUTH_FAILED:
            if (chip->epp) {
                sc96281_log_info("QC3/PD/FAIL EPP adapter set iwls 850mA\n");
                chip->pre_curr = 850;
                sc96281_set_pmic_icl(chip, 850);
                sc96281_set_pmic_fcc(chip, 2000);
            } else {
                sc96281_log_info("QC3/PD/FAIL BPP adapter set iwls 750mA\n");
                chip->pre_curr = 750;
                chip->pre_vol = BPP_DEFAULT_VOUT;
                sc96281_set_pmic_icl(chip, 750);
                sc96281_set_pmic_fcc(chip, 1000);
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
            sc96281_log_info("XM adapter set iwls 850mA\n");
            chip->pre_curr = 850;
            chip->pre_vol = EPP_DEFAULT_VOUT;
            sc96281_set_pmic_icl(chip, 850);
            sc96281_set_pmic_fcc(chip, 2000);
            break;
        default:
            sc96281_log_info("other adapter type\n");
            break;
        }
    } else {
        switch (chip->adapter_type) {
        case ADAPTER_QC3:
        case ADAPTER_PD:
            msleep(2000);
            sc96281_log_info("QC3/PD FC BPP set iwls 1100mA\n");
            sc96281_stepper_pmic_icl(chip, 800, 1100, 100, 20);
            chip->pre_curr = 1100;
            chip->pre_vol = BPP_PLUS_VOUT;
            sc96281_set_pmic_fcc(chip, 2000);
            break;
        case ADAPTER_XIAOMI_QC3:
        case ADAPTER_XIAOMI_PD:
        case ADAPTER_ZIMI_CAR_POWER:
            sc96281_log_info("20W adapter set fcc 3800mA\n");
            sc96281_set_pmic_fcc(chip, 3800);
            chip->pre_vol = EPP_PLUS_VOUT;
            chip->qc_enable = true;
            chip->is_vout_range_set_done = true;
            break;
        case ADAPTER_XIAOMI_PD_40W:
        case ADAPTER_VOICE_BOX:
            sc96281_log_info("30W adapter set fcc 5400mA\n");
            sc96281_set_pmic_fcc(chip, 5400);
            chip->pre_vol = EPP_PLUS_VOUT;
            chip->qc_enable = true;
            chip->is_vout_range_set_done = true;
            break;
        case ADAPTER_XIAOMI_PD_50W:
        case ADAPTER_XIAOMI_PD_60W:
        case ADAPTER_XIAOMI_PD_100W:
            if (chip->is_sailboat_tx) {
                sc96281_log_info("sailboat 50/60/100W adapters set fcc 8800mA\n");
                sc96281_set_pmic_fcc(chip, 8800);
            } else {
                sc96281_log_info("50/60/100W adapters set fcc 9200mA\n");
                sc96281_set_pmic_fcc(chip, 9200);
            }
            chip->pre_vol = EPP_PLUS_VOUT;
            chip->qc_enable = true;
            break;
        default:
            sc96281_log_info("other adapter type\n");
            break;
        }
    }

    if (chip->wireless_psy)
        power_supply_changed(chip->wireless_psy);

    schedule_delayed_work(&chip->chg_monitor_work, msecs_to_jiffies(1000));
}

static void sc96281_set_fastchg_adapter_v(struct sc96281_chg *chip)
{
    int ret = 0;
    switch (chip->adapter_type) {
    case ADAPTER_QC3:
    case ADAPTER_PD:
        if (!chip->epp) {
            sc96281_log_info("bpp+ set adapter voltage to 9V\n");
            ret = sc96281_set_adapter_voltage(chip, BPP_PLUS_VOUT);
            if (ret < 0)
                sc96281_log_info("bpp+ set adapter voltage failed!!!\n");
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
        sc96281_log_info("EPP+ set adapter voltage to 15V\n");
        ret = sc96281_set_adapter_voltage(chip, EPP_PLUS_VOUT);
        if (ret < 0)
            sc96281_log_info("epp+ set adapter voltage failed!!!\n");
        break;
    default:
        sc96281_log_info("other adapter, don't set adapter voltage\n");
        break;
    }
    return;
}

static void sc96281_update_qucikchg_type(struct sc96281_chg *chip)
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

static void sc96281_process_power_reduce_cmd(struct sc96281_chg *chip, u8 cmd)
{
    if (cmd == chip->current_for_adapter_cmd || cmd < ADAPTER_CMD_TYPE_F0 || cmd > ADAPTER_CMD_TYPE_F4)
        return;

    if (sc96281_check_votable(chip) == false)
        return;

    chip->current_for_adapter_cmd  = cmd;
    sc96281_log_info("[%s] ADAPTER_CMD_TYPE_%d trigger\n", __func__, chip->current_for_adapter_cmd);

    switch (cmd) {
    case ADAPTER_CMD_TYPE_F0:
        vote(chip->fcc_votable, WLS_POWER_REDUCE_VOTER, false, 0);
        break;
    case ADAPTER_CMD_TYPE_F1:
        vote(chip->fcc_votable, WLS_POWER_REDUCE_VOTER, true, 6000);
        break;
    case ADAPTER_CMD_TYPE_F2:
        vote(chip->fcc_votable, WLS_POWER_REDUCE_VOTER, true, 4000);
        break;
    case ADAPTER_CMD_TYPE_F3:
        vote(chip->fcc_votable, WLS_POWER_REDUCE_VOTER, true, 2000);
        vote(chip->icl_votable, WLS_POWER_REDUCE_VOTER, true, 850);
        break;
    case ADAPTER_CMD_TYPE_F4:
        vote(chip->fcc_votable, WLS_POWER_REDUCE_VOTER, true, 1000);
        vote(chip->icl_votable, WLS_POWER_REDUCE_VOTER, true, 400);
        break;
    default:
        sc96281_log_info("[%s] RX_INT_POWER_REDUCE default\n", __func__);
        break;
    }
}

static void sc96281_process_q_value_strategy(struct sc96281_chg *chip, bool limt_en)
{
    if (limt_en) {
        vote(chip->fcc_votable, WLS_Q_VALUE_STRATEGY_VOTER, true, WLS_CHG_TX_QLIMIT_FCC_5W);
        vote(chip->icl_votable, WLS_Q_VALUE_STRATEGY_VOTER, true, WLS_CHG_TX_QLIMIT_ICL_5W);
    } else {
        vote(chip->fcc_votable, WLS_Q_VALUE_STRATEGY_VOTER, false, 0);
        vote(chip->icl_votable, WLS_Q_VALUE_STRATEGY_VOTER, false, 0);
    }
    sc96281_log_info("[%s] limt_en=%d\n", __func__, limt_en);
}

static void sc96281_chg_handler(struct sc96281_chg *chip, u16 int_flag)
{
    u8 auth_status = 0;
    u8 rcv_value[128] = {0};
    u8 val_length = 0, l_len = 0;
    uint32_t err_code = 0, dfx_code = 0;
    int tx_speed = 0;
    u8 tx_q2 = WLS_DEFAULT_TX_Q2;
    int dfx_data[2] = { 0 };
    int effective_fcc = 0;
    int len = 0;
    char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
    struct mca_event_notify_data event_data;

    if (int_flag & RX_INT_POWER_ON) {
        chip->epp = sc96281_get_rx_power_mode(chip);
        mca_log_err("RX_INT_POWER_ON epp: %d\n", chip->epp);

    } else if (int_flag & RX_INT_LDO_ON) {
        mca_log_err("RX_INT_LDO_ON!\n");
        chip->epp = sc96281_get_rx_power_mode(chip);
        sc96281_enable_bc12(chip, true);
        sc96281_enable_power_path(chip, true);
        if (chip->epp) {
            sc96281_set_pmic_fcc(chip, 3000);
            msleep(20);
            effective_fcc = sc96281_get_fcc(chip);
            if (effective_fcc < 1500)
                sc96281_stepper_pmic_icl(chip, 200, 400, 100, 20);
            else
                sc96281_stepper_pmic_icl(chip, 200, 800, 100, 20);
        } else {
            sc96281_set_pmic_fcc(chip, 1500);
            msleep(20);
            effective_fcc = sc96281_get_fcc(chip);
            if (effective_fcc < 900)
                sc96281_stepper_pmic_icl(chip, 250, 500, 100, 20);
            else
                sc96281_stepper_pmic_icl(chip, 250, 750, 100, 20);
        }

    } else if (int_flag & RX_INT_AUTHEN_FINISH) {
        auth_status = sc96281_get_auth_value(chip);
        mca_log_err("RX_INT_AUTHEN_FINISH! auth_data:%d\n", auth_status);
        if (auth_status != AUTH_STATUS_FAILED) {
            if (auth_status >= AUTH_STATUS_UUID_OK)
                sc96281_set_fod_params(chip);
            if (chip->epp)
                sc96281_epp_uuid_func(chip);
            sc96281_update_qucikchg_type(chip);
            if (chip->adapter_type >= ADAPTER_XIAOMI_PD_50W) {
                if (chip->is_car_tx) {
                    len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_WLS_CAR_ADAPTER=%d", 1);
                    event_data.event = event;
                    event_data.event_len = len;
                    mca_event_report_uevent(&event_data);
                }
                if(chip->q_value_supprot) {
                    if (chip->low_inductance_50w_tx)
                        sc96281_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_QVALUE, chip->tx_q1[ADAPTER_LOW_INDUCTANCE_TX_50W]);
                    else if (chip->low_inductance_80w_tx)
                        sc96281_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_QVALUE, chip->tx_q1[ADAPTER_LOW_INDUCTANCE_TX_80W]);
                    usleep_range(100*1000, 150*1000);
                }
                schedule_delayed_work(&chip->renegociation_work, msecs_to_jiffies(1500));
                if (chip->is_support_fan_tx) {
                    tx_speed = (chip->quiet_sts)? WLS_TX_FAN_SPEED_QUIET : WLS_TX_FAN_SPEED_NORMAL;
                    sc96281_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_FAN_SPEED, tx_speed);
                }
            } else if(chip->adapter_type >= ADAPTER_XIAOMI_QC3) {
                if (chip->is_support_fan_tx) {
                    tx_speed = (chip->quiet_sts)? WLS_TX_FAN_SPEED_QUIET : WLS_TX_FAN_SPEED_NORMAL;
                    sc96281_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_FAN_SPEED, tx_speed);
                }
                if (chip->is_car_tx) {
                    len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_WLS_CAR_ADAPTER=%d", 1);
                    event_data.event = event;
                    event_data.event_len = len;
                    mca_event_report_uevent(&event_data);
                }
                sc96281_adapter_handle(chip);
                sc96281_set_fastchg_adapter_v(chip);
            } else {
                sc96281_adapter_handle(chip);
                sc96281_set_fastchg_adapter_v(chip);
            }
        } else {
            sc96281_log_info("[%s] authen failed!\n", __func__);
            sc96281_adapter_handle(chip);
        }

    } else if (int_flag & RX_INT_RENEGO_DONE) {
        mca_log_err("RX_INT_RENEGO_DONE!\n");
        cancel_delayed_work_sync(&chip->renegociation_work);
        sc96281_set_fastchg_adapter_v(chip);

    } else if (int_flag & RX_INT_FAST_CHARGE) {
        mca_log_err("RX_INT_FAST_CHARGE!\n");
        chip->fc_flag = sc96281_get_fastchg_result(chip);
        if (chip->fc_flag) {
            if (chip->adapter_type < ADAPTER_XIAOMI_PD_50W || chip->is_car_tx) {
                sc96281_adapter_handle(chip);
                sc96281_set_vout(chip, 8000);
                //TODO: update wls thermal
            } else {
                sc96281_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_FREQUENCE, SUPER_TX_FREQUENCY_DEFAULT_KHZ);
            }
            sc96281_adapter_handle(chip);
            //TODO
            //set tx fan speed tx_speed
            //if chip->epp, enable vdd
        } else if (chip->set_fastcharge_vout_cnt++ < 3) {
            sc96281_log_info("set fastchg vol failed, retry %d\n", chip->set_fastcharge_vout_cnt);
            msleep(2000);
            sc96281_set_fastchg_adapter_v(chip);
        } else {
            sc96281_log_info("set fastchg vol failed finally\n");
            sc96281_adapter_handle(chip);
            mca_charge_mievent_report(CHARGE_DFX_WLS_FASTCHG_FAIL, NULL, 0);
            //if (chip->adapter_type >= ADAPTER_XIAOMI_QC3)
            //    charger_dev_enable_pmic_ovp(chip->cp_master_dev, true);
        }

    } else if (int_flag & RX_INT_TRANSPARENT_SUCCESS) {
        sc96281_rcv_transparent_data(chip, rcv_value, ARRAY_SIZE(rcv_value), &val_length);
        l_len = val_length & 0x0F;
        mca_log_err("RX_INT_TRANSPARENT_SUCCESS, curr_cmd=%d, val_len=%d %d\n",
                chip->current_trans_packet_type, val_length, l_len);
        if (l_len == 1) {
            if (rcv_value[0] == 0x28) {
                if ((rcv_value[3] & 0x3C) == 0x3C) {
                    chip->set_tx_voltage_cnt = 0;
                    if (chip->current_trans_packet_type == WLS_FREQUENCE_PACKET) {
                        if (chip->is_car_tx || chip->is_sailboat_tx)
                            sc96281_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_VOUT_RANGE, SUPER_TX_VOUT_MIN_MV);
                        else
                            sc96281_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_VOUT_RANGE, SUPER_TX_VOUT_PLAN_A_MV);
                    } else if (chip->current_trans_packet_type == WLS_VOUT_RANGE_PACKET) {
                        chip->current_trans_packet_type = UNKNOWN_PACKET;
                        chip->is_vout_range_set_done = true;
                        sc96281_adapter_handle(chip);
                        //TODO: limit cp_work_mode
                    }
                } else {
                    if (chip->set_tx_voltage_cnt++ < 3) {
                        if (chip->current_trans_packet_type == WLS_FREQUENCE_PACKET) {
                            sc96281_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_FREQUENCE, SUPER_TX_FREQUENCY_DEFAULT_KHZ);
                        } else if (chip->current_trans_packet_type == WLS_VOUT_RANGE_PACKET) {
                            if (chip->adapter_type > ADAPTER_XIAOMI_PD_50W)
                                sc96281_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_VOUT_RANGE, SUPER_TX_VOUT_PLAN_B_MV);
                            else
                                sc96281_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_VOUT_RANGE, SUPER_TX_VOUT_PLAN_A_MV);
                        }
                    } else {
                        sc96281_log_err("set tx voltage failed finally\n");
                        chip->current_trans_packet_type = UNKNOWN_PACKET;
                        chip->is_vout_range_set_done = true;
                        chip->force_cp_2_1_mode = true;
                        sc96281_set_vout(chip, 8000);
                        sc96281_adapter_handle(chip);
                    }
                }
            } else if (rcv_value[0] == 0x05) {
                sc96281_process_power_reduce_cmd(chip, rcv_value[2]);
                chip->current_trans_packet_type = UNKNOWN_PACKET;
            }
        } else if(chip->current_trans_packet_type == WLS_Q_STARTEGY_PACKET || l_len == 4) {
            if (chip->low_inductance_50w_tx)
                tx_q2 = chip->tx_q2[ADAPTER_LOW_INDUCTANCE_TX_50W];
            else if (chip->low_inductance_80w_tx)
                tx_q2 = chip->tx_q2[ADAPTER_LOW_INDUCTANCE_TX_80W];
            else
                tx_q2 = rcv_value[2];
            sc96281_log_info("[%s] RX_INT_TRANSPARENT_SUCCESS tx_q=%d, tx_q2=%d\n", __func__, rcv_value[5], tx_q2);
            sc96281_process_q_value_strategy(chip, rcv_value[5] <= tx_q2);
            if (rcv_value[5] <= tx_q2) {
                dfx_data[0] = tx_q2;
                dfx_data[1] = rcv_value[5];
                mca_charge_mievent_report(CHARGE_DFX_WLS_FOD_LOW_POWER, dfx_data, 2);
            }
            chip->current_trans_packet_type = UNKNOWN_PACKET;
        }

    } else if (int_flag & RX_INT_TRANSPARENT_FAIL) {
        mca_log_err("RX_INT_TRANSPARENT_FAIL!\n");
        sc96281_rcv_transparent_data(chip, rcv_value, ARRAY_SIZE(rcv_value), &val_length);
        if (rcv_value[0] == 0x28 && chip->current_trans_packet_type != UNKNOWN_PACKET) {
            if (chip->set_tx_voltage_cnt++ < 3) {
                if (chip->current_trans_packet_type == WLS_FREQUENCE_PACKET) {
                    sc96281_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_FREQUENCE, SUPER_TX_FREQUENCY_DEFAULT_KHZ);
                } else if (chip->current_trans_packet_type == WLS_VOUT_RANGE_PACKET) {
                    if (chip->adapter_type > ADAPTER_XIAOMI_PD_50W)
                        sc96281_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_VOUT_RANGE, SUPER_TX_VOUT_PLAN_B_MV);
                    else
                        sc96281_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_VOUT_RANGE, SUPER_TX_VOUT_PLAN_A_MV);
                }
            } else {
                chip->current_trans_packet_type = UNKNOWN_PACKET;
                chip->is_vout_range_set_done = true;
                chip->force_cp_2_1_mode = true;
                sc96281_set_vout(chip, 8000);
                sc96281_adapter_handle(chip);
            }
        }

    } else if (int_flag & RX_INT_OCP_OTP_ALARM) {
        mca_log_err("RX_INT_OCP_OTP_ALARM!, scheduled_flag=%d\n", chip->oxp_scheduled_flag);
        if (!chip->oxp_scheduled_flag) {
            chip->oxp_scheduled_flag = true;
            schedule_delayed_work(&chip->rx_alarm_work, msecs_to_jiffies(300));
        }

    } else if (int_flag & RX_INT_SLEEP) {
        mca_log_err("RX_INT_SLEEP!\n");
        sc96281_rx_power_good_update(chip);

    } else if (int_flag & RX_INT_POWER_OFF) {
        mca_log_err("RX_INT_POWER_OFF!\n");
        sc96281_rx_power_off_err(chip);
        sc96281_rx_power_good_update(chip);
        if (chip->wls_wakelock->active)
            __pm_relax(chip->wls_wakelock);

    } else if (int_flag & RX_INT_FACTORY_TEST) {
        mca_log_err("RX_INT_FACTORY_TEST!\n");
        sc96281_rcv_factory_test_cmd(chip, rcv_value, &val_length);
        mca_log_err("factory test: 0x%x, 0x%x, 0x%x\n", rcv_value[0], rcv_value[1],rcv_value[2]);
        if (rcv_value[0] == FACTORY_TEST_CMD)
            sc96281_process_factory_cmd(chip, rcv_value[1]);

    } else if (int_flag & RX_INT_ERR_CODE) {
        sc96281_get_rx_err_code(chip, &err_code, &dfx_code);
        mca_log_err("RX_INT_ERR_CODE! code=[%d %d]!\n", err_code, dfx_code);
        switch (dfx_code) {
        case CHARGE_DFX_WLS_RX_OTP:
            mca_charge_mievent_report(CHARGE_DFX_WLS_RX_OTP, &chip->rx_temp, 1);
            break;
        case CHARGE_DFX_WLS_RX_OVP:
            mca_charge_mievent_report(CHARGE_DFX_WLS_RX_OVP, NULL, 0);
            break;
        case CHARGE_DFX_WLS_RX_OCP:
            mca_charge_mievent_report(CHARGE_DFX_WLS_RX_OCP, NULL, 0);
            break;
        default:
            break;
        }
    }

    return;
}

static void sc96281_wireless_int_work(struct work_struct *work)
{
    int ret = 0;
    struct sc96281_chg *chip = container_of(work, struct sc96281_chg, wireless_int_work.work);
    uint8_t int_rtx_mode = RX_MODE;
    uint32_t int_value = 0;
    uint16_t int_flag = 0;

    sc96281_log_info("%s enter\n", __func__);
    mutex_lock(&chip->wireless_chg_int_lock);

    ret = sc96281_get_rx_rtx_mode(chip, &int_rtx_mode);
    if (ret < 0) {
        sc96281_log_err("get rtx mode fail\n");
        goto exit;
    }

    if (int_rtx_mode == RTX_MODE) {
        ret = sc96281_tx_get_int(chip, &int_value);
        if (ret < 0) {
            sc96281_log_err("get tx int flag fail\n");
            goto exit;
        }
        int_flag = sc96281_tx_remap_int(chip, int_value);
        mca_log_err("get tx int: 0x%x -> 0x%x\n", int_value, int_flag);
        sc96281_reverse_chg_handler(chip, int_flag);
        sc96281_tx_clr_int(chip, int_value);
    } else {
        ret = sc96281_rx_get_int(chip, &int_value);
        if (ret < 0) {
            sc96281_log_err("get rx int flag fail\n");
            goto exit;
        }
        int_flag = sc96281_rx_remap_int(chip, int_value);
        mca_log_err("get rx int: 0x%x -> 0x%x\n", int_value, int_flag);
        sc96281_chg_handler(chip, int_flag);
        sc96281_rx_clr_int(chip, int_value);
    }

exit:
    mutex_unlock(&chip->wireless_chg_int_lock);
    return;
}

static irqreturn_t sc96281_interrupt_handler(int irq, void *dev_id)
{
    struct sc96281_chg *chip = dev_id;
    sc96281_log_dbg("[%s]\n", __func__);
    schedule_delayed_work(&chip->wireless_int_work, 0);
    return IRQ_HANDLED;
}

static void sc96281_reset_parameters(struct sc96281_chg *chip)
{
    sc96281_log_info("%s\n", __func__);
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
    chip->is_sailboat_tx = false;
    chip->is_standard_tx = false;
    chip->is_support_fan_tx = false;
    chip->low_inductance_50w_tx = false;
    chip->low_inductance_80w_tx = false;
    chip->parallel_charge = false;
    chip->force_cp_2_1_mode = false;
    //chip->reverse_chg_en = false;
    chip->i2c_ok_flag = false;
    chip->oxp_scheduled_flag = false;
    chip->mutex_lock_sts = false;
    chip->is_vout_range_set_done = false;
    chip->current_trans_packet_type = UNKNOWN_PACKET;
    chip->set_tx_voltage_cnt = 0;
    chip->qc_type = QUICK_CHARGE_NORMAL;
}

static void sc96281_pg_det_work(struct work_struct *work)
{
    struct sc96281_chg *chip = container_of(work, struct sc96281_chg, wireless_pg_det_work.work);
    int ret = 0, wls_switch_usb = 0;
    int len = 0;
    char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
    struct mca_event_notify_data event_data = { 0 };

    sc96281_log_info("%s:enter\n", __func__);
    if (!chip->wireless_psy) {
        chip->wireless_psy = power_supply_get_by_name("wireless");
        if (!chip->wireless_psy)
            sc96281_log_err("failed to get wireless psy\n");
    }
    if (gpio_is_valid(chip->power_good_gpio)) {
        ret = gpio_get_value(chip->power_good_gpio);
        mca_log_err("power_good_gpio ret=%d\n", ret);
        if (ret) {
            mca_log_err("power_good high, wireless attached\n");
            if (!chip->wls_wakelock->active)
                __pm_stay_awake(chip->wls_wakelock);
            chip->power_good_flag = 1;
            chip->adapter_type = ADAPTER_SDP;
            chip->current_for_adapter_cmd = ADAPTER_CMD_TYPE_NONE;
            sc96281_set_pmic_icl(chip, 0);
            if (chip->icl_votable) {
                vote(chip->icl_votable, WLS_PARACHG_VOTER, false, 0);
                vote(chip->icl_votable, ICL_VOTER, false, 0);
            }
            cancel_delayed_work_sync(&chip->renegociation_work);
            schedule_delayed_work(&chip->i2c_check_work, msecs_to_jiffies(500));
            schedule_delayed_work(&chip->trans_data_work, msecs_to_jiffies(0));
            //TODO: set usb suspend
            //TODO: high soc > 70%, limit fcc to 7A
            //TODO: auto judge firmware update
        } else {
            mca_log_err("power_good low, wireless detached\n");
            sc96281_reset_parameters(chip);
            wake_up_interruptible(&chip->wait_que);
            mca_charge_mievent_set_state(MIEVENT_STATE_PLUG, 0);
            cancel_delayed_work(&chip->chg_monitor_work);
            cancel_delayed_work(&chip->rx_alarm_work);
            cancel_delayed_work(&chip->i2c_check_work);
            cancel_delayed_work_sync(&chip->renegociation_work);
            cancel_delayed_work_sync(&chip->trans_data_work);
            sc96281_set_pmic_icl(chip, 0);
            if (chip->icl_votable) {
                vote(chip->icl_votable, WLS_CHG_VOTER, false, 0);
                vote(chip->icl_votable, WLS_POWER_REDUCE_VOTER, false, 0);
                vote(chip->icl_votable, WLS_PARACHG_VOTER, false, 0);
            }
            if (chip->fcc_votable) {
                vote(chip->fcc_votable, WLS_CHG_VOTER, false, 0);
                vote(chip->fcc_votable, WLS_POWER_REDUCE_VOTER, false, 0);
                vote(chip->fcc_votable, WLS_Q_VALUE_STRATEGY_VOTER, false, 0);
                vote(chip->fcc_votable, WLS_OCP_PROTECT_VOTER, false, 0);
                vote(chip->fcc_votable, WLS_OTP_PROTECT_VOTER, false, 0);
            }

            len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_WLS_CAR_ADAPTER=%d", 0);
            event_data.event = event;
            event_data.event_len = len;
            mca_event_report_uevent(&event_data);

            wls_get_property(WLS_PROP_SWITCH_USB, &wls_switch_usb);
            sc96281_log_info("wireless switch to usb: %d\n", wls_switch_usb);
            if (!wls_switch_usb) {
                sc96281_enable_bc12(chip, false);
                sc96281_enable_power_path(chip, true);
            }
            if (chip->revchg_test_status == REVERSE_TEST_SCHEDULE) {
                sc96281_log_info("factory reverse charge start\n");
                schedule_delayed_work(&chip->factory_reverse_start_work, msecs_to_jiffies(2000));
            }
            if (chip->wls_wakelock->active)
                __pm_relax(chip->wls_wakelock);
            //TODO: disable vdd
            //TODO: enable aicl
        }
        if (chip->wireless_psy) {
            power_supply_changed(chip->wireless_psy);
        }
    }
}

static void sc96281_init_detect_work(struct work_struct *work)
{
    struct sc96281_chg *chip = container_of(work, struct sc96281_chg,
        init_detect_work.work);
    int ret = 0;

    if (gpio_is_valid(chip->power_good_gpio)) {
        ret = gpio_get_value(chip->power_good_gpio);
        sc96281_log_info("init power good: %d\n", ret);
        if (ret) {
            sc96281_set_enable_mode(chip, false);
            usleep_range(20000, 25000);
            sc96281_set_enable_mode(chip, true);
        }
    }
    return;
}

static void sc96281_init_fw_check_work(struct work_struct *work)
{
    struct sc96281_chg *chip = container_of(work, struct sc96281_chg,
        init_fw_check_work.work);
    int ret = 0;
    int otg_enable = 0, vusb_insert = 0;

    if (chip->power_good_flag || chip->reverse_chg_en || chip->fw_update) {
        sc96281_log_info("%s: wls chg or wls revchg or fw updating, no need to check fw\n", __func__);
        return;
    }

    usb_get_property(USB_PROP_OTG_ENABLE, &otg_enable);
    wls_get_property(WLS_PROP_VUSB_INSERT, &vusb_insert);
    if (otg_enable || vusb_insert) {
        sc96281_log_info("%s: wirechg or otg online, no need to check fw\n", __func__);
        return;
    }

    if (sc96281_no_charging_get_fw_version(chip) != (BOOT_CHECK_SUCCESS | RX_CHECK_SUCCESS | TX_CHECK_SUCCESS)) {
        ret = sc96281_firmware_update_func(chip, FW_UPDATE_FORCE);
        if (ret < 0) {
            sc96281_log_err("%s: fw update failed\n", __func__);
        } else {
            sc96281_log_info("%s: fw update success\n", __func__);
        }
    }

    return;
}

static void sc96281_get_charge_phase(struct sc96281_chg *chip, int *chg_phase)
{
    switch (*chg_phase) {
    case NORMAL_MODE:
        if (chip->batt_soc == 100) {
            *chg_phase = TAPER_MODE;
            sc96281_log_info("change normal mode to tapter mode");
        }
        break;
    case TAPER_MODE:
        if ((chip->batt_soc == 100) && (chip->chg_status == POWER_SUPPLY_STATUS_FULL)) {
            *chg_phase = FULL_MODE;
            sc96281_log_info("change taper mode to full mode");
        } else if (chip->batt_soc < 99) {
            *chg_phase = NORMAL_MODE;
            sc96281_log_info("change taper mode to normal mode");
        }
        break;
    case FULL_MODE:
        if (chip->chg_status == POWER_SUPPLY_STATUS_CHARGING) {
            *chg_phase = RECHG_MODE;
            sc96281_log_info("change full mode to recharge mode");
        }
        break;
    case RECHG_MODE:
        if (chip->chg_status == POWER_SUPPLY_STATUS_FULL) {
            *chg_phase = FULL_MODE;
            sc96281_log_info("change recharge mode to full mode");
        }
        break;
    default:
        break;
    }
    return;
}

static void sc96281_get_adapter_current(struct sc96281_chg *chip, u8 adapter)
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

static void sc96281_get_charging_info(struct sc96281_chg *chip)
{
    int vout = 0, iout = 0, vrect = 0;
    union power_supply_propval val = {0,};
    int ret = 0;

    if (!chip || !chip->fcc_votable || !chip->icl_votable)
        return;

    if (!chip->batt_psy)
        chip->batt_psy = power_supply_get_by_name("battery");
    if (!chip->batt_psy)
        sc96281_log_err("failed to get batt_psy\n");
    else {
        power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
        if(chip->batt_soc < val.intval){
            sc96281_log_err("update soc to tx soc=%d\n", val.intval);
            sc96281_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_SOC, val.intval);
        }
        chip->batt_soc = val.intval;
        power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_TEMP, &val);
        chip->batt_temp = val.intval;
        power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_STATUS, &val);
        chip->chg_status = val.intval;
        sc96281_get_charge_phase(chip, &chip->chg_phase);
    }

    ret = sc96281_get_iout(chip, &iout);
    if (ret < 0) {
        sc96281_log_err("get iout failed\n");
        iout = 0;
    }
    ret = sc96281_get_vout(chip, &vout);
    if (ret < 0) {
        sc96281_log_err("get vout failed\n");
        vout = 0;
    }
    ret = sc96281_get_vrect(chip, &vrect);
    if (ret < 0) {
        sc96281_log_err("get vrect failed\n");
        vrect = 0;
    }
    ret = sc96281_get_temp(chip, &chip->rx_temp);
    if (ret < 0) {
        sc96281_log_err("get temp failed\n");
    }

    chip->rx_fcc = sc96281_get_fcc(chip);
    chip->rx_icl = sc96281_get_icl(chip);

    mca_log_err("rx[v:%d %d i:%d t:%d], st[soc:%d tabt:%d st:%d ph:%d], vote[fcc:%d-%d %s icl:%d-%d %s]\n",
            vrect, vout, iout, chip->rx_temp, chip->batt_soc, chip->batt_temp, chip->chg_status, chip->chg_phase,
            get_client_vote(chip->fcc_votable, WLS_CHG_VOTER), chip->rx_fcc, get_effective_client(chip->fcc_votable),
            get_client_vote(chip->icl_votable, WLS_CHG_VOTER), chip->rx_icl, get_effective_client(chip->icl_votable));
}

static void sc96281_update_target_vol_curr(struct sc96281_chg *chip)
{
    int fcc = 0;

    sc96281_get_adapter_current(chip, chip->adapter_type);

    if (chip->is_plate_tx || chip->is_train_tx) {
        chip->target_vol = (chip->chg_phase == FULL_MODE || chip->chg_phase == RECHG_MODE)? EPP_DEFAULT_VOUT : chip->target_vol;
        chip->target_curr = (chip->chg_phase == FULL_MODE)? 800 : ((chip->chg_phase == RECHG_MODE)? 1000 : chip->target_curr);
    } else if (chip->is_music_tx) {
        chip->target_vol = (chip->chg_phase == FULL_MODE || chip->chg_phase == RECHG_MODE || chip->batt_temp >= 390)? EPP_DEFAULT_VOUT : chip->target_vol;
        chip->target_curr = (chip->chg_phase == FULL_MODE)? 800 : ((chip->chg_phase == RECHG_MODE)? 1000 : chip->target_curr);
        chip->fc_flag = (chip->batt_temp >= 39)? 0 : chip->fc_flag;
        fcc = (chip->batt_temp < 360)? 6000 : (((chip->batt_temp <= 430)? (2500 - (chip->batt_temp - 360) * 20): 1000));
        mca_log_err("set misic tx fcc:%d\n", fcc);
        sc96281_set_pmic_fcc(chip, fcc);
    }

    if (chip->target_vol != chip->pre_vol && !chip->parallel_charge) {
        mca_log_err("set new vout:%d, pre vout:%d\n", chip->target_vol, chip->pre_vol);
        sc96281_set_vout(chip, chip->target_vol);
        chip->pre_vol = chip->target_vol;
    }
    if (chip->target_curr != chip->pre_curr && !chip->parallel_charge) {
        mca_log_err("set new icl:%d, pre icl:%d\n", chip->target_curr, chip->pre_curr);
        sc96281_set_pmic_icl(chip, chip->target_curr);
        chip->pre_curr = chip->target_curr;
    }
}

static void sc96281_monitor_work(struct work_struct *work)
{
    struct sc96281_chg *chip = container_of(work, struct sc96281_chg,
            chg_monitor_work.work);

    sc96281_get_charging_info(chip);
    sc96281_update_target_vol_curr(chip);
    schedule_delayed_work(&chip->chg_monitor_work, msecs_to_jiffies(5000));
}

static void sc96281_factory_reverse_start_work(struct work_struct *work)
{
    struct sc96281_chg *chip = container_of(work, struct sc96281_chg,
            factory_reverse_start_work.work);

    chip->user_reverse_chg = true;
    sc96281_set_reverse_chg_mode(chip, true);
    chip->revchg_test_status = REVERSE_TEST_PROCESSING;
    sc96281_log_info("factory reverse test, processing\n");
    schedule_delayed_work(&chip->factory_reverse_stop_work, msecs_to_jiffies(12000));
}

static void sc96281_factory_reverse_stop_work(struct work_struct *work)
{
    struct sc96281_chg *chip = container_of(work, struct sc96281_chg,
            factory_reverse_stop_work.work);
    int len;
    char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
    struct mca_event_notify_data event_data = { 0 };

    chip->tx_timeout_flag = true;
    sc96281_set_reverse_chg_mode(chip, false);
    chip->revchg_test_status = REVERSE_TEST_DONE;
    chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
    len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_REVERSE_CHG_STATE=%d", chip->is_reverse_chg);
    event_data.event = event;
    event_data.event_len = len;
    mca_event_report_uevent(&event_data);
    sc96281_log_info("factory reverse test, stop\n");
    chip->tx_timeout_flag = false;
}

static void sc96281_check_rx_alarm(struct sc96281_chg *chip, int *ocp_flag, int *otp_flag)
{
    int iout = 0, temp = 0;
    int ret = 0;

    ret = sc96281_get_iout(chip, &iout);
    if (ret < 0)
        *ocp_flag = 0;
    else {
        if (iout >= RX_MAX_IOUT_TRIG)
            *ocp_flag = 1;
        else if (iout >= RX_MAX_IOUT_CLR)
            *ocp_flag = 0;
        else
            *ocp_flag = -1;
    }

    ret = sc96281_get_temp(chip, &temp);
    if (ret < 0)
        *otp_flag = 0;
    else {
        if (temp >= RX_MAX_TEMP_TRIG)
            *otp_flag = 1;
        else if (temp >= RX_MAX_TEMP_CLR)
            *otp_flag = 0;
        else
            *otp_flag = -1;
    }

    return;
}

static void sc96281_rx_alarm_work(struct work_struct *work)
{
    struct sc96281_chg *chip = container_of(work, struct sc96281_chg,
            rx_alarm_work.work);
    int ocp_flag = 0, otp_flag = 0;
    int fcc_setted = 0, ocp_fcc_setted = 0, otp_fcc_setted = 0;

    if (!chip->fcc_votable)
        chip->fcc_votable = find_votable("CHARGER_FCC");
    if (!chip->fcc_votable) {
        chip->oxp_scheduled_flag = false;
        return;
    }

    sc96281_check_rx_alarm(chip, &ocp_flag, &otp_flag);
    fcc_setted = sc96281_get_fcc(chip);
    ocp_fcc_setted = get_client_vote(chip->fcc_votable, WLS_OCP_PROTECT_VOTER);
    otp_fcc_setted = get_client_vote(chip->fcc_votable, WLS_OTP_PROTECT_VOTER);
    sc96281_log_info("rx_alarm_work: ocp[%d %d], otp[%d %d] fcc[%d %s]\n",
            ocp_flag, ocp_fcc_setted, otp_flag, otp_fcc_setted,
            fcc_setted, get_effective_client(chip->fcc_votable));

    if ((ocp_flag == 0) && (otp_flag == 0))
        goto exit;

    if (ocp_flag == 1) {
        if (fcc_setted - 100 > 1000) {
            sc96281_log_info("soft ocp trigger, reduce fcc 100mA\n");
            vote(chip->fcc_votable, WLS_OCP_PROTECT_VOTER, true, fcc_setted-100);
        }
    } else if (ocp_flag == -1) {
        if (ocp_fcc_setted < 0)
            ocp_fcc_setted = fcc_setted;
        if (ocp_fcc_setted <= fcc_setted) {
            sc96281_log_info("soft ocp clear, increase fcc 100mA\n");
            vote(chip->fcc_votable, WLS_OCP_PROTECT_VOTER, true, ocp_fcc_setted+100);
        }
    }

    if (otp_flag == 1) {
        if (fcc_setted - 500 > 1000) {
            sc96281_log_info("soft otp trigger, reduce fcc 500mA\n");
            vote(chip->fcc_votable, WLS_OTP_PROTECT_VOTER, true, fcc_setted-500);
        }
    } else if (otp_flag == -1) {
        if (otp_fcc_setted < 0)
            otp_fcc_setted = fcc_setted;
        if (otp_fcc_setted <= fcc_setted) {
            sc96281_log_info("soft otp clear, increase fcc 500mA\n");
            vote(chip->fcc_votable, WLS_OTP_PROTECT_VOTER, true, otp_fcc_setted+500);
        }
    }

exit:
    schedule_delayed_work(&chip->rx_alarm_work, msecs_to_jiffies(5000));
    chip->oxp_scheduled_flag = false;
}

static void sc96281_i2c_check_work(struct work_struct *work)
{
    struct sc96281_chg *chip = container_of(work, struct sc96281_chg,
            i2c_check_work.work);
    int ret = 0;
    int retry_cnt = 0;

    while (retry_cnt < 3) {
        ret = sc96281_check_i2c(chip);
        if (ret >= 0)
            break;
        msleep(20);
        retry_cnt++;
    }

    if (retry_cnt >= 3) {
        mca_charge_mievent_report(CHARGE_DFX_WLS_RX_IIC_ERR, NULL, 0);
        chip->i2c_ok_flag = false;
    } else {
        chip->i2c_ok_flag = true;
    }

    return;
}

static void sc96281_wireless_renegociation_work(struct work_struct *work)
{
    struct sc96281_chg *chip = container_of(work, struct sc96281_chg,
            renegociation_work.work);

    sc96281_start_renego(chip);
    sc96281_log_err("%s:start renego work\n", __func__);
}

static void sc96281_process_trans_func(struct sc96281_chg *chip, struct trans_data_lis_node *node)
{
    switch (node->data_flag) {
    case TRANS_DATA_FLAG_SOC:
        sc96281_update_soc_to_tx(chip, node->value);
        break;
    case TRANS_DATA_FLAG_QVALUE:
        sc96281_update_q_value_strategy_to_tx(chip, node->value);
        break;
    case TRANS_DATA_FLAG_FAN_SPEED:
        sc96281_send_fan_speed_to_tx(chip, node->value);
        break;
    case TRANS_DATA_FLAG_VOUT_RANGE:
        sc96281_send_vout_range_to_tx(chip, node->value);
        break;
    case TRANS_DATA_FLAG_FREQUENCE:
        sc96281_send_frequency_to_tx(chip, node->value);
        break;
    default:
        sc96281_log_err("not support this type\n");
        break;
    }
}

static int sc96281_process_trans(struct sc96281_chg *chip)
{
    struct trans_data_lis_node *cur_node, *temp_node;

    while (!list_empty(&chip->header) && !chip->mutex_lock_sts) {
        spin_lock(&chip->list_lock);
        list_for_each_entry_safe(cur_node, temp_node, &chip->header, lnode) {
            if (chip->mutex_lock_sts)
                break;
            list_del(&cur_node->lnode);
            spin_unlock(&chip->list_lock);

            sc96281_log_info("cur_node: data_flag: %d, value: %d\n", cur_node->data_flag, cur_node->value);
            sc96281_process_trans_func(chip, cur_node);

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

static void sc96281_trans_data_work(struct work_struct *work)
{
    struct sc96281_chg *chip = container_of(work, struct sc96281_chg,
            trans_data_work.work);

    while (chip->power_good_flag)
        wait_event_interruptible(chip->wait_que, (sc96281_process_trans(chip)));
}

static void sc96281_mutex_unlock_work(struct work_struct *work)
{
    struct sc96281_chg *chip = container_of(work, struct sc96281_chg,
            mutex_unlock_work.work);

    if (chip->mutex_lock_sts) {
        chip->mutex_lock_sts = false;
        wake_up_interruptible(&chip->wait_que);
    }
}

static irqreturn_t sc96281_power_good_handler(int irq, void *dev_id)
{
    struct sc96281_chg *chip = dev_id;

    sc96281_log_err("power_good irq trigger\n");
    if (chip->fw_update) {
        sc96281_log_err("fw_update exit\n");
        return IRQ_HANDLED;
    }

    schedule_delayed_work(&chip->wireless_pg_det_work, msecs_to_jiffies(0));
    return IRQ_HANDLED;
}

static int sc96281_parse_fod_subparams(struct device_node *node, const char *name, struct params_t *params)
{
    int i, j;
    int len;
    u8 *idata = NULL;

    if (strcmp(name, "null") == 0) {
        sc96281_log_info("no need parse params\n");
        return -1;
    }

    len = of_property_count_u8_elems(node, name);
    if (len <= 0 || ((unsigned int)len % PARAMS_T_MAX != 0) || ((unsigned int)len > DEFAULT_FOD_PARAM_LEN * PARAMS_T_MAX)) {
        sc96281_log_err("parse %s failed\n", name);
        return -1;
    }

    idata = kcalloc(len, sizeof(u8), GFP_KERNEL);
    if (!idata) {
        sc96281_log_err("malloc failed\n");
        return -1;
    }
    if (of_property_read_u8_array(node, name, idata, len)) {
        sc96281_log_err("prop %s read fail, array len %d\n", name, len);
        kfree(idata);
        idata = NULL;
        return -1;
    }
    for (i = 0; i < len / 2; i++) {
        j = 2 * i;
        params[i].gain = idata[j];
        params[i].offset = idata[j + 1];
        sc96281_log_dbg("[%d]params: gain:%d, offset:%d\n", i, params[i].gain, params[i].offset);
    }

    kfree(idata);
    idata = NULL;
    return 0;
}

static int sc96281_parse_fod_params(struct device_node *node, struct sc96281_chg *info)
{
    int array_len, row, col, i;
    const char *tmp_string = NULL;

    array_len = of_property_count_strings(node, "fod_params");
    if (array_len <= 0 || ((unsigned int)array_len % FOD_PARA_MAX != 0) || ((unsigned int)array_len > FOD_PARA_MAX_GROUP * FOD_PARA_MAX)) {
        sc96281_log_err("parse fod_params failed\n");
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
            if (sc96281_parse_fod_subparams(node, tmp_string, info->fod_params[row].params))
                return -1;
            break;
        default:
            break;
        }
    }

    return 0;
}

static int sc96281_parse_fod_params_default(struct device_node *node, struct sc96281_chg *info)
{
    int array_len, row, col, i;
    const char *tmp_string = NULL;

    array_len = of_property_count_strings(node, "fod_params_default");
    if (array_len <= 0 || ((unsigned int)array_len % FOD_PARA_MAX != 0) || ((unsigned int)array_len > FOD_PARA_MAX_GROUP * FOD_PARA_MAX)) {
        sc96281_log_err("parse fod_params_default failed\n");
        return -1;
    }

    for (i = 0; i < array_len; i++) {
        if (of_property_read_string_index(node, "fod_params_default", i, &tmp_string))
            return -1;

        row = i / FOD_PARA_MAX;
        col = i % FOD_PARA_MAX;
        sc96281_log_dbg("[%d]fod params default %s\n", i, tmp_string);
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
            if (sc96281_parse_fod_subparams(node, tmp_string, info->fod_params_default.params))
                return -1;
            break;
        default:
            break;
        }
    }

    return 0;
}

static int sc96281_parse_fw_fod_data(struct sc96281_chg *chip)
{
    int ret = 0;
    struct device_node *node = NULL;

    chip->fw_version_index = chip->fw_version_index_default;
    node = of_find_node_by_name(NULL, "wls_sc96281_fod_data");

    chip->fw_data_ptr = SC96281_FIRMWARE[chip->fw_version_index];
    chip->fw_data_size = sizeof(SC96281_FIRMWARE[chip->fw_version_index]);
    mca_log_err("fw_version_index %d, fw_data [%02x %02x %02x]\n",
            chip->fw_version_index, chip->fw_data_ptr[0], chip->fw_data_ptr[1], chip->fw_data_ptr[2]);

    ret = sc96281_parse_fod_params(node, chip);
    ret = sc96281_parse_fod_params_default(node, chip);

    return ret;
}

static int sc96281_parse_dt(struct sc96281_chg *chip)
{
    struct device_node *node = chip->dev->of_node;
    int ret = 0;
    u8 idata_u8[ADAPTER_LOW_INDUCTANCE_TX_TYPE_MAX] = { 0 };

    if (!node) {
        mca_log_err("No DT data Failing Probe\n");
        return -EINVAL;
    }

    if (of_property_read_u32(node, "fw_version_index_default", &chip->fw_version_index_default) < 0)
        chip->fw_version_index_default = 0;
    if (of_property_read_u32(node, "fw_version_index_jp", &chip->fw_version_index_jp) < 0)
        chip->fw_version_index_jp = 0;
    sc96281_parse_fw_fod_data(chip);

    chip->rx_sleep_gpio = of_get_named_gpio(node, "rx_sleep_gpio", 0);
    if ((!gpio_is_valid(chip->rx_sleep_gpio))) {
        mca_log_err("fail rx_sleep_gpio %d\n", chip->rx_sleep_gpio);
        return -EINVAL;
    }
    chip->irq_gpio = of_get_named_gpio(node, "rx_irq_gpio", 0);
    if (!gpio_is_valid(chip->irq_gpio)) {
        mca_log_err("fail irq_gpio %d\n", chip->irq_gpio);
        return -EINVAL;
    }
    chip->power_good_gpio = of_get_named_gpio(node, "pwr_det_gpio", 0);
    if (!gpio_is_valid(chip->power_good_gpio)) {
        mca_log_err("fail power_good_gpio %d\n", chip->power_good_gpio);
        return -EINVAL;
    }

    of_property_read_u32(node, "reverse_boost_src", &chip->reverse_boost_src);
    chip->reverse_boost_src = (chip->reverse_boost_src >= BOOST_SRC_MAX)? PMIC_REV_BOOST : chip->reverse_boost_src;
    if (chip->reverse_boost_src == EXTERNAL_BOOST) {
        chip->tx_on_gpio = of_get_named_gpio(node, "reverse_chg_ovp_gpio", 0);
        if (!gpio_is_valid(chip->tx_on_gpio)) {
            mca_log_err("fail tx_on_gpio %d\n", chip->tx_on_gpio);
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

static int sc96281_gpio_init(struct sc96281_chg *chip)
{
    int ret = 0;

    if (gpio_is_valid(chip->irq_gpio)) {
        chip->client->irq = gpio_to_irq(chip->irq_gpio);
        if (chip->client->irq < 0) {
            sc96281_log_err("[%s] gpio_to_irq Fail! \n", __func__);
            goto fail_irq_gpio;
        }
    } else {
        sc96281_log_err("%s: irq gpio not provided\n", __func__);
        goto fail_irq_gpio;
    }

    if (gpio_is_valid(chip->power_good_gpio)) {
        chip->power_good_irq = gpio_to_irq(chip->power_good_gpio);
        if (chip->power_good_irq < 0) {
            sc96281_log_err("[%s] gpio_to_irq Fail! \n", __func__);
            goto fail_power_good_gpio;
        }
    } else {
        sc96281_log_err("%s: power good gpio not provided\n", __func__);
        goto fail_power_good_gpio;
    }

    return ret;

fail_irq_gpio:
    gpio_free(chip->irq_gpio);
fail_power_good_gpio:
    gpio_free(chip->power_good_gpio);
    return ret;
}

static int sc96281_get_image_fwver(const unsigned char *firmware, const uint32_t len, uint32_t *image_ver)
{
    if (len < 0x200) {
        sc96281_log_err("Firmware image length is too short\n");
        return -1;
    }

    *image_ver = (uint32_t)firmware[0X100 + 4] & 0x00FF;
    *image_ver |= ((uint32_t)firmware[0X100 + 5] & 0x00FF) << 8;
    *image_ver |= ((uint32_t)firmware[0X100 + 6] & 0x00FF) << 16;
    *image_ver |= ((uint32_t)firmware[0X100 + 7] & 0x00FF) << 24;

    return 0;
}

static uint8_t sc96281_get_fw_version(struct sc96281_chg *chip)
{
    int ret = 0;
    uint32_t fw_ver = 0, image_ver = 0, fw_check = 0;
    uint8_t check_result = 0;

    ret = read_cust(chip, cust_rx.firmware_check, &fw_check);
    ret |= read_cust(chip, cust_rx.firmware_ver, &fw_ver);
    if (ret) {
        sc96281_log_err("sc96281 get fw_check or fw_ver fail\n");
        return check_result;
    }

    if ((fw_ver >> 16) == 0x5663 && (fw_check == 0)) {
        fw_check = fw_ver;
        g_wls_fw_data.fw_boot_id = 0;
    } else {
        g_wls_fw_data.fw_boot_id = (fw_ver >> 16) & 0xFF;
    }
    g_wls_fw_data.fw_rx_id = (fw_ver >> 8) & 0xFF;
    g_wls_fw_data.fw_tx_id = fw_ver & 0xFF;

    sc96281_get_image_fwver(chip->fw_data_ptr, chip->fw_data_size, &image_ver);
    if(fw_check == fw_ver && fw_ver >= image_ver)
        check_result = BOOT_CHECK_SUCCESS | RX_CHECK_SUCCESS | TX_CHECK_SUCCESS;
    sc96281_log_info("sc96281 get fw check success, fw_ver:0x%x, img_ver:0x%x, check:0x%x, res:0x%x\n",
        fw_ver, image_ver, fw_check, check_result);

    return check_result;
}

static u8 sc96281_in_charging_get_fw_version(struct sc96281_chg *chip)
{
    u8 fw_update_status = 7;

    if (!chip->power_good_flag)
        return fw_update_status;

    fw_update_status = sc96281_get_fw_version(chip);
    sc96281_log_info("%s: boot = 0x%x, rx = 0x%x, tx = 0x%x\n",
        __func__, g_wls_fw_data.fw_boot_id, g_wls_fw_data.fw_rx_id, g_wls_fw_data.fw_tx_id);

    return fw_update_status;
}

static u8 sc96281_no_charging_get_fw_version(struct sc96281_chg *chip)
{
    u8 fw_update_status = 7;

    if (chip->power_good_flag)
        return fw_update_status;

    if (chip->fw_update) {
        sc96281_log_info("%s: fw update going, can not get fw version\n", __func__);
        return fw_update_status;
    }

    chip->fw_update = true;
    sc96281_enable_reverse_boost(chip, BOOST_FOR_FWUPDATE, true);
    msleep(100);
    fw_update_status = sc96281_get_fw_version(chip);
    sc96281_enable_reverse_boost(chip, BOOST_FOR_FWUPDATE, false);
    chip->fw_update = false;
    sc96281_log_info("%s: boot = 0x%x, rx = 0x%x, tx = 0x%x\n",
        __func__, g_wls_fw_data.fw_boot_id, g_wls_fw_data.fw_rx_id, g_wls_fw_data.fw_tx_id);

    return fw_update_status;
}

static int sc96281_download_fw(struct sc96281_chg *chip)
{
    int ret = 0;

    sc96281_rx_set_cmd(chip, AP_CMD_WAIT_FOR_UPDATE);
    ret = mtp_program(chip, false);
    if (ret) {
        sc96281_log_err("download firmware fail\n");
        chip->fw_upgrade_fail_info = "MtpProgramFail";
    } else
        sc96281_log_err("download firmware success\n");

    return ret;
}

static int sc96281_download_fw_from_bin(struct sc96281_chg *chip)
{
    int ret = 0;

    ret = sc96281_check_i2c(chip);
    if (ret < 0) {
        sc96281_log_err("check i2c fail, quit download firmware from bin.\n");
        return ret;
    }

    sc96281_rx_set_cmd(chip, AP_CMD_WAIT_FOR_UPDATE);
    ret = mtp_program(chip, true);
    if (ret)
        sc96281_log_err("download firmware from bin fail\n");
    else
        sc96281_log_info("download firmware from bin success\n");

    chip->fw_bin_length = 0;

    return ret;
}

static int sc96281_firmware_update_func(struct sc96281_chg *chip, u8 cmd)
{
    int ret = 0;
    u8 check_result = 0;
    int otg_enable = 0, vusb_insert = 0;

    //TODO1 disable reverse charge if it run
    chip->fw_update = true;
    sc96281_set_enable_mode(chip, false);
    sc96281_enable_reverse_boost(chip, BOOST_FOR_FWUPDATE, true);
    msleep(100);
    ret = sc96281_check_i2c(chip);
    if (ret < 0)
        goto exit;

    //start down firmware
    cmd = (cmd >= FW_UPDATE_MAX)? FW_UPDATE_FORCE : cmd;
    switch (cmd) {
    case FW_UPDATE_USER:
        check_result = sc96281_get_fw_version(chip);
        if (check_result == (RX_CHECK_SUCCESS | TX_CHECK_SUCCESS | BOOT_CHECK_SUCCESS)) {
            mca_log_err("fw no need update\n");
            ret = 0;
            goto exit;
        }
        ret = sc96281_download_fw(chip);
        if (ret < 0) {
            mca_log_err("fw download failed! cmd: %d\n", cmd);
            goto exit;
        }
        msleep(100);
        break;
    case FW_UPDATE_FORCE:
        ret = sc96281_download_fw(chip);
        if (ret < 0) {
            mca_log_err("fw download failed! cmd: %d\n", cmd);
            goto exit;
        }
        msleep(100);
        break;
    case FW_UPDATE_FROM_BIN:
        ret = sc96281_download_fw_from_bin(chip);
        if (ret < 0) {
            mca_log_err("fw download failed! cmd: %d\n", cmd);
            goto exit;
        }
        msleep(100);
        break;
    default:
        mca_log_err("unknown cmd: %d\n", cmd);
        break;
    }

    if (cmd != FW_UPDATE_CHECK) {
        chip->fw_version_reflash = true;
        sc96281_enable_reverse_boost(chip, BOOST_FOR_FWUPDATE, false);
        msleep(1000);
        sc96281_enable_reverse_boost(chip, BOOST_FOR_FWUPDATE, true);
        chip->fw_version_reflash = false;
        msleep(100);
    }

    check_result = sc96281_get_fw_version(chip);
    if (check_result == (RX_CHECK_SUCCESS | TX_CHECK_SUCCESS | BOOT_CHECK_SUCCESS)) {
        mca_log_err("download firmware success!\n");
    } else {
        ret = -1;
        mca_log_err("download firmware failed!\n");
    }

exit:
    chip->fw_update = false;
    sc96281_enable_reverse_boost(chip, BOOST_FOR_FWUPDATE, false);
    usb_get_property(USB_PROP_OTG_ENABLE, &otg_enable);
    wls_get_property(WLS_PROP_VUSB_INSERT, &vusb_insert);
    if (!otg_enable && !vusb_insert)
        sc96281_set_enable_mode(chip, true);

    return ret;
}


//-------------------------- node ops-----------------------------
static ssize_t chip_vrect_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    int vrect = 0, ret = 0;
    ret = sc96281_get_vrect(g_chip, &vrect);
    if (ret < 0 ) {
        sc96281_log_err("get vrect failed\n");
        vrect = 0;
    }
    return scnprintf(buf, PAGE_SIZE, "sc96281 Vrect : %d mV\n", vrect);
}

static ssize_t chip_iout_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    int iout = 0, ret = 0;
    ret = sc96281_get_iout(g_chip, &iout);
    if (ret < 0 ) {
        sc96281_log_err("get iout failed\n");
        iout = 0;
    }
    return scnprintf(buf, PAGE_SIZE, "%d\n", iout);
}

static ssize_t chip_temp_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    int temp = 0;

    if (g_chip == NULL)
        temp = 0;
    else
        temp = g_chip->rx_temp;

    return scnprintf(buf, PAGE_SIZE, "%d\n", temp);
}

static ssize_t chip_vout_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    int vout = 0, ret = 0;
    ret = sc96281_get_vout(g_chip, &vout);
    if (ret < 0 ) {
        sc96281_log_err("get vout failed\n");
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
    sc96281_log_info("[%s] --Store output_voltage = %d\n",
                            __func__, index);
    if ((index < 4000) || (index > 21000)) {
        sc96281_log_err("[%s] Store Voltage %s is invalid\n",
                            __func__, buf);
        sc96281_set_vout(g_chip, 0);
        return count;
    }
    sc96281_set_vout(g_chip, index);
    return count;
}

static ssize_t wls_debug_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    int vout = 0, vrect=0, iout=0, ret = 0;
    sc96281_log_err("[WLS_DEBUG] enter %s\n", __func__);
    ret = sc96281_get_vout(g_chip, &vout);
    if (ret < 0 ) {
        sc96281_log_err("[WLS_DEBUG] get vout failed\n");
        vout = 0;
    }
    ret = sc96281_get_vrect(g_chip, &vrect);
    if (ret < 0)
    {
        sc96281_log_err("[WLS_DEBUG] get vrect failed\n");
        vrect = 0;
    }
    ret = sc96281_get_iout(g_chip, &iout);
    if (ret < 0)
    {
        sc96281_log_err("[WLS_DEBUG] get iout failed\n");
        iout = 0;
    }
    sc96281_log_err("[WLS_DEBUG] vout vret iout = [%d %d %d]\n", vout, vrect, iout);
    return scnprintf(buf, PAGE_SIZE, "vout=%d, vrect=%d, iout=%d\n", vout, vrect, iout);
}

static const char *const wls_debug_text[] = {
    "none", "set_fcc", "set_icl", "set_epp_fod_single", "set_epp_fod_all", "set_epp_fod_more_five", "too_bigger"};

static ssize_t wls_debug_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    int num, i = 0;
    char *token = NULL;
    int data[30] = {0};
    char dest[100];
    char *rest = NULL;

    strcpy(dest, buf);
    rest = dest;

    sc96281_log_err("[WLS_DEBUG] [%s] %s\n", __func__, buf);
    sc96281_log_err("[WLS_DEBUG] rest %s\n", rest);

    while ((token = strsep(&rest, " ")) != NULL) {
        num = simple_strtol(token, NULL, 10);;
        if(i < 30)
            data[i]  = num;
        else
            break;
        sc96281_log_err("[WLS_DEBUG] num=%d",num);
        i++;
    }

    switch(data[0])
    {
        if(data[0] <= WLS_DEBUG_MAX)
            sc96281_log_err("[WLS_DEBUG] enter %s\n", wls_debug_text[data[0]]);
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
        if (count == 4)
            sc96281_set_debug_fod(g_chip, data, count);
        break;
    case WLS_DEBUG_EPP_FOD_ALL:
        if (count == 33)
            sc96281_set_debug_fod(g_chip, data, count);
        break;
    case WLS_DEBUG_EPP_FOD_ALL_DIRECTLY:
        if (count % 2 == 0 && count >= 12)
            sc96281_set_debug_fod(g_chip, data, count);
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
    static uint16_t total_length = 0;
    static uint8_t fw_area = 0;
    static uint8_t serial_number = 0;

    if (g_chip == NULL)
        return -EINVAL;

    if ( strncmp("length:", buf, 7 ) == 0 ) {
        if (kstrtou16( buf+7, 10, &total_length))
              return -EINVAL;
        g_chip->fw_bin_length = total_length;
        serial_number = 0;
        sc96281_log_info("[WLS_BIN] total_length:%d, serial_number:%d\n", total_length, serial_number);
    } else if ( strncmp("area:", buf, 5 ) == 0 ) {
        if (kstrtou8( buf+5, 10, &fw_area))
              return -EINVAL;
        sc96281_log_info("[WLS_BIN] area:%d\n", fw_area);
    } else {
        memcpy((g_chip->fw_bin + serial_number * count), buf, count);
        serial_number++;
        sc96281_log_info("[WLS_BIN] serial_number:%d, count:%zu\n", serial_number, count);
    }

    return count;
}

static ssize_t chip_firmware_update_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    int cmd = 0, ret = 0;
    if (g_chip->fw_update){
        sc96281_log_info("[%s] Firmware Update is on going!\n", __func__);
        return count;
    }
    cmd = (int)simple_strtoul(buf, NULL, 10);
    sc96281_log_info("[%s] value %d\n", __func__, cmd);
    if ((cmd >= FW_UPDATE_ERASE) && (cmd < FW_UPDATE_MAX)) {
        ret = sc96281_firmware_update_func(g_chip, cmd);
        if (ret < 0) {
            sc96281_log_err("[%s] Firmware Update:failed!\n", __func__);
            return count;
        } else {
            sc96281_log_info("[%s] Firmware Update:Success!\n", __func__);
            return count;
        }
    } else {
        sc96281_log_err("[%s] Firmware Update:invalid cmd\n", __func__);
    }
    return count;
}

static ssize_t chip_version_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    u8 check_result = 0;
    if (g_chip->fw_update) {
        sc96281_log_info("[%s] fw update going, can not show version\n", __func__);
        return scnprintf(buf, PAGE_SIZE, "updating\n");
    } else {
        g_chip->fw_update = true;
        sc96281_set_enable_mode(g_chip, false);
        sc96281_enable_reverse_boost(g_chip, BOOST_FOR_FWUPDATE, true);
        msleep(100);
        check_result = sc96281_get_fw_version(g_chip);
        g_chip->fw_update = false;
        sc96281_enable_reverse_boost(g_chip, BOOST_FOR_FWUPDATE, false);
        sc96281_set_enable_mode(g_chip, true);
        return scnprintf(buf, PAGE_SIZE, "%02x.%02x.%02x\n",
                g_wls_fw_data.fw_boot_id, g_wls_fw_data.fw_rx_id, g_wls_fw_data.fw_tx_id);
    }
}

static ssize_t tx_speed_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    return scnprintf(buf, PAGE_SIZE, "%d\n", g_chip->tx_speed);
}

static ssize_t tx_speed_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    int tx_speed = 0;

    tx_speed = (int)simple_strtoul(buf, NULL, 10);
    if (tx_speed < WLS_TX_FAN_SPEED_MIN || tx_speed > WLS_TX_FAN_SPEED_MAX) {
        sc96281_log_err("%s: %d is invalid\n", __func__, tx_speed);
        return count;
    }

    sc96281_add_trans_task_to_queue(g_chip, TRANS_DATA_FLAG_FAN_SPEED, tx_speed);
    return count;
}

static DEVICE_ATTR(chip_vrect, S_IRUGO, chip_vrect_show, NULL);
static DEVICE_ATTR(chip_firmware_update, S_IWUSR, NULL, chip_firmware_update_store);
static DEVICE_ATTR(chip_version, S_IRUGO, chip_version_show, NULL);
static DEVICE_ATTR(chip_vout, S_IWUSR | S_IRUGO, chip_vout_show, chip_vout_store);
static DEVICE_ATTR(chip_iout, S_IRUGO, chip_iout_show, NULL);
static DEVICE_ATTR(chip_temp, S_IRUGO, chip_temp_show, NULL);
static DEVICE_ATTR(wls_debug, S_IWUSR | S_IRUGO, wls_debug_show, wls_debug_store);
static DEVICE_ATTR(wls_bin, S_IWUSR, NULL, wls_bin_store);
static DEVICE_ATTR(tx_speed, S_IWUSR | S_IRUGO, tx_speed_show, tx_speed_store);

static struct attribute *sc96281_sysfs_attrs[] = {
    &dev_attr_chip_vrect.attr,
    &dev_attr_chip_version.attr,
    &dev_attr_chip_vout.attr,
    &dev_attr_chip_iout.attr,
    &dev_attr_chip_temp.attr,
    &dev_attr_chip_firmware_update.attr,
    &dev_attr_wls_debug.attr,
    &dev_attr_wls_bin.attr,
    &dev_attr_tx_speed.attr,
    NULL,
};
static const struct attribute_group sc96281_sysfs_group_attrs = {
    .attrs = sc96281_sysfs_attrs,
};


//------------------------ wls class ops---------------------------
static int wls_is_wireless_present(struct wireless_charger_device *chg_dev, bool *present)
{
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);
    if (chip->power_good_flag)
        *present = true;
    else
        *present = false;
    return 0;
}

static int wls_is_i2c_ok(struct wireless_charger_device *chg_dev, bool *i2c_ok)
{
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);

    if (!chip->power_good_flag)
        *i2c_ok = false;
    else
        *i2c_ok = chip->i2c_ok_flag;
    return 0;
}

static int wls_is_qc_enable(struct wireless_charger_device *chg_dev, bool *enable)
{
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);
    if (chip->qc_enable)
        *enable = true;
    else
        *enable = false;
    return 0;
}

static int wls_is_firmware_update(struct wireless_charger_device *chg_dev, bool *update)
{
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);
    if (chip->fw_update)
        *update = true;
    else
        *update = false;
    return 0;
}

static int wls_is_car_adapter(struct wireless_charger_device *chg_dev, bool *enable)
{
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);
    if (chip->is_car_tx)
        *enable = true;
    else
        *enable = false;
    return 0;
}

static int wls_get_reverse_charge(struct wireless_charger_device *chg_dev, bool *enable)
{
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);
    if (chip->reverse_chg_en)
        *enable = true;
    else
        *enable = false;
    return 0;
}

static int wls_notify_cp_status(struct wireless_charger_device *chg_dev, int status)
{
    int ret = 0;
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);
    if (chip->power_good_flag)
        ret = sc96281_set_cp_status(chip, status);
    else
        sc96281_log_info("power good off, can't set status\n");
    sc96281_log_info("wls_notify_cp_status: %d\n", status);
    return ret;
}

static int wls_get_chip_version(struct wireless_charger_device *chg_dev, char *buf)
{
    int ret = 0;
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);

    if (chip->fw_update) {
        ret = scnprintf(buf, PAGE_SIZE, "updating\n");
        sc96281_log_info("[%s] fw update going, can not show version\n", __func__);
    } else {
        ret = scnprintf(buf, PAGE_SIZE, "%02x.%02x.%02x\n",
                g_wls_fw_data.fw_boot_id, g_wls_fw_data.fw_rx_id, g_wls_fw_data.fw_tx_id);
        sc96281_log_info("%s is %s\n", __func__, buf);
    }

    return ret;
}

static int wls_firmware_update(struct wireless_charger_device *chg_dev, int cmd)
{
    int ret = 0;
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);

    if (chip->fw_update){
        mca_log_err("Firmware Update is on going!\n");
        return -1;
    }

    if ((cmd < FW_UPDATE_ERASE) || (cmd >= FW_UPDATE_MAX)) {
        mca_log_err("Firmware Update:invalid cmd\n");
        return -1;
    }

    mca_log_err("value %d\n", cmd);
    ret = sc96281_firmware_update_func(chip, cmd);
    if (ret < 0)
        mca_log_err("Firmware Update:failed!\n");
    else
        mca_log_err("Firmware Update:Success!\n");

    return ret;
}

static int wls_check_fw_version(struct wireless_charger_device *chg_dev)
{
    int ret = 0;
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);
    if (chip->fw_update){
        sc96281_log_info("[%s] Firmware Update is on going!\n", __func__);
        return ret;
    }
    sc96281_log_info("[%s] power good=%d\n", __func__, chip->power_good_flag);
    if(chip->power_good_flag)
        ret = sc96281_in_charging_get_fw_version(chip);
    else
        ret = sc96281_no_charging_get_fw_version(chip);
    return ret;
}

static int wls_enable_reverse_chg(struct wireless_charger_device *chg_dev, bool enable)
{
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);
    int ret = 0;

    ret = sc96281_set_reverse_chg_mode(chip, enable);
    chip->user_reverse_chg = enable;

    return ret;
}

static int wls_set_vout(struct wireless_charger_device *chg_dev, int vout)
{
    int ret = 0;
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);
    if (chip->power_good_flag)
        ret = sc96281_set_vout(chip, vout);
    else
        sc96281_log_info("power good off, can't set vout\n");
    sc96281_log_info("wls_set_vout: %d\n", vout);
    return ret;
}

static int wls_get_vout(struct wireless_charger_device *chg_dev, int *vout)
{
    int ret = 0;
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);
    if (chip->power_good_flag) {
        ret = sc96281_get_vout(chip, vout);
        if (ret < 0)
            *vout = 0;
        sc96281_log_info("wls_get_vout: %d\n", *vout);
    } else
        *vout = 0;

    return 0;
}

static int wls_get_iout(struct wireless_charger_device *chg_dev, int *iout)
{
    int ret = 0;
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);
    if (chip->power_good_flag) {
        ret = sc96281_get_iout(chip, iout);
        if (ret < 0)
            *iout = 0;
        sc96281_log_info("wls_get_iout: %d\n", *iout);
    } else
        *iout = 0;

    return 0;
}

static int wls_get_vrect(struct wireless_charger_device *chg_dev, int *vrect)
{
    int ret = 0;
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);
    if (chip->power_good_flag) {
        ret = sc96281_get_vrect(chip, vrect);
        if (ret < 0)
            *vrect = 0;
    } else
        *vrect = 0;
    sc96281_log_info("wls_get_vrect: %d\n", *vrect);
    return 0;
}

static int wls_get_tx_adapter(struct wireless_charger_device *chg_dev, int *adapter)
{
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);
    if (chip->power_good_flag) {
        sc96281_log_info("wls_get_adapter: %d\n", chip->adapter_type);
        *adapter = chip->adapter_type;
    } else
        *adapter = 0;

    return 0;
}

static int wls_get_tx_uuid(struct wireless_charger_device *chg_dev, char *buf)
{
    int ret = 0;
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);

    if (!chip->power_good_flag) {
        ret = scnprintf(buf, PAGE_SIZE, "00.00.00.00");
        return ret;
    }

    ret = scnprintf(buf, PAGE_SIZE, "%02x.%02x.%02x.%02x",
            chip->uuid[0], chip->uuid[1], chip->uuid[2], chip->uuid[3]);
    mca_log_err("%s\n", buf);

    return ret;
}

static int wls_get_reverse_chg_state(struct wireless_charger_device *chg_dev, int *state)
{
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);
    *state = chip->is_reverse_chg;
    sc96281_log_info("wls_get_reverse_chg_state: %d\n", chip->is_reverse_chg);
    return 0;
}

static int wls_is_enable(struct wireless_charger_device *chg_dev, bool *enable)
{
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);

    *enable = (chip->enable_flag == 0)? false : true;
    return 0;
}

static int wls_enable_chg(struct wireless_charger_device *chg_dev, bool enable)
{
    int ret = 0;
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);
    int len;
    char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
    struct mca_event_notify_data event_data = { 0 };

    if (!chip->power_good_flag && !enable) {
        if (chip->icl_votable) {
            vote(chip->icl_votable, WLS_CHG_VOTER, false, 0);
            vote(chip->icl_votable, WLS_POWER_REDUCE_VOTER, false, 0);
        }
        if (chip->fcc_votable) {
            vote(chip->fcc_votable, WLS_CHG_VOTER, false, 0);
            vote(chip->fcc_votable, WLS_POWER_REDUCE_VOTER, false, 0);
            vote(chip->fcc_votable, WLS_Q_VALUE_STRATEGY_VOTER, false, 0);
            vote(chip->fcc_votable, WLS_OCP_PROTECT_VOTER, false, 0);
            vote(chip->fcc_votable, WLS_OTP_PROTECT_VOTER, false, 0);
        }
    }

    ret = sc96281_set_enable_mode(chip, enable);

    if (!enable && chip->reverse_boost_src == PMIC_REV_BOOST && chip->reverse_chg_en) {
        sc96281_log_info("wls revchg state is %d, close it\n", chip->is_reverse_chg);
        sc96281_set_reverse_chg_mode(chip, false);
        chip->is_reverse_chg = REVERSE_STATE_ENDTRANS;
        len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "POWER_SUPPLY_REVERSE_CHG_STATE=%d", chip->is_reverse_chg);
        event_data.event = event;
        event_data.event_len = len;
        mca_event_report_uevent(&event_data);
    }

    return ret;
}

static int wls_set_rx_sleep_mode(struct wireless_charger_device *chg_dev, int sleep_for_dam)
{
    int ret = 0;
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);

    if(sleep_for_dam)
        ret = sc96281_rx_set_cust_cmd(chip, CUST_CMD_ULPM);
    sc96281_log_info("%s :%d\n", __func__, sleep_for_dam);

    return ret;
}

static int wls_set_quiet_sts(struct wireless_charger_device *chg_dev, int quiet_sts)
{
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);
    int tx_speed = WLS_TX_FAN_SPEED_NORMAL;

    chip->quiet_sts = quiet_sts;

    if(!chip->power_good_flag) {
        sc96281_log_info("%s: wls is not online, keep sts %d wait for next chg\n", __func__, quiet_sts);
    } else {
        tx_speed = quiet_sts? WLS_TX_FAN_SPEED_QUIET : WLS_TX_FAN_SPEED_NORMAL;
        if (chip->tx_speed != tx_speed)
            sc96281_add_trans_task_to_queue(chip, TRANS_DATA_FLAG_FAN_SPEED, tx_speed);
        else
            sc96281_log_info("%s: tx speed is already %d, no need to set\n", __func__, tx_speed);
    }

    return 0;
}

static int wls_set_parallel_charge(struct wireless_charger_device *chg_dev, bool parachg)
{
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);

    chip->parallel_charge = parachg;
    mca_log_info("%d\n", parachg);

    return 0;
}

static int wls_is_vout_range_set_done(struct wireless_charger_device *chg_dev, bool *is_done)
{
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);

    if (chip->power_good_flag)
        *is_done = chip->is_vout_range_set_done;
    else
        *is_done = false;

    return 0;
}

static int wls_get_adapter_chg_mode(struct wireless_charger_device *chg_dev, int *cp_chg_mode)
{
    struct sc96281_chg *chip = dev_get_drvdata(&chg_dev->dev);

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
        if (chip->force_cp_2_1_mode)
            *cp_chg_mode = FORWARD_2_1_CHARGER_MODE;
        else if (chip->is_car_tx || chip->is_sailboat_tx)
            *cp_chg_mode = FORWARD_2_1_CHARGER_MODE;
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

static const struct wireless_charger_properties sc96281_chg_props = {
    .alias_name = "sc96281_wireless_chg",
};
static const struct wireless_charger_ops sc96281_chg_ops = {
    .wls_enable_reverse_chg = wls_enable_reverse_chg,
    .wls_is_wireless_present = wls_is_wireless_present,
    .wls_is_i2c_ok = wls_is_i2c_ok,
    .wls_is_qc_enable = wls_is_qc_enable,
    .wls_is_firmware_update = wls_is_firmware_update,
    .wls_set_vout = wls_set_vout,
    .wls_get_vout = wls_get_vout,
    .wls_get_iout = wls_get_iout,
    .wls_get_vrect = wls_get_vrect,
    .wls_get_tx_adapter = wls_get_tx_adapter,
    .wls_get_tx_uuid = wls_get_tx_uuid,
    .wls_get_reverse_chg_state = wls_get_reverse_chg_state,
    .wls_is_enable = wls_is_enable,
    .wls_enable_chg = wls_enable_chg,
    .wls_is_car_adapter = wls_is_car_adapter,
    .wls_get_reverse_chg = wls_get_reverse_charge,
    .wls_notify_cp_status = wls_notify_cp_status,
    .wls_get_chip_version = wls_get_chip_version,
    .wls_firmware_update = wls_firmware_update,
    .wls_check_fw_version = wls_check_fw_version,
    .wls_set_rx_sleep_mode = wls_set_rx_sleep_mode,
    .wls_set_quiet_sts = wls_set_quiet_sts,
    .wls_set_parallel_charge = wls_set_parallel_charge,
    .wls_is_vout_range_set_done = wls_is_vout_range_set_done,
    .wls_get_adapter_chg_mode = wls_get_adapter_chg_mode,
};
static int sc96281_chg_init_chgdev(struct sc96281_chg *chip)
{
    sc96281_log_info("enter %s\n", __func__);
    chip->wlschgdev = wireless_charger_device_register(chip->wlsdev_name, chip->dev,
                        chip, &sc96281_chg_ops,
                        &sc96281_chg_props);
    return IS_ERR(chip->wlschgdev) ? PTR_ERR(chip->wlschgdev) : 0;
}

static int sc96281_probe(struct i2c_client *client)
{
    int ret = 0;
    bool temp = false;
    struct sc96281_chg *chip;
    char *name = NULL;
    sc96281_log_info("enter sc96281 probe\n");
    chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
    if (!chip) {
        sc96281_log_err("Failed to allocate memory\n");
        return -ENOMEM;
    }
    chip->regmap = devm_regmap_init_i2c(client, &sc96281_regmap_config);
    if (IS_ERR(chip->regmap)) {
        sc96281_log_err("failed to allocate register map\n");
        return PTR_ERR(chip->regmap);
    }
    chip->client = client;
    chip->dev = &client->dev;
    chip->fw_update = false;
    chip->fw_version_reflash = false;
    chip->ss = 2;
    chip->wlsdev_name = SC96281_DRIVER_NAME;
    chip->chg_phase = NORMAL_MODE;
    chip->user_reverse_chg = true;
    chip->revchg_test_status = REVERSE_TEST_NONE;
    chip->enable_flag = 1;
    chip->qc_type = QUICK_CHARGE_NORMAL;
    g_chip = chip;
    device_init_wakeup(&client->dev, true);
    i2c_set_clientdata(client, chip);
    mutex_init(&chip->wireless_chg_int_lock);
    mutex_init(&chip->data_transfer_lock);
    mutex_init(&chip->i2c_rw_lock);
    name = devm_kasprintf(chip->dev, GFP_KERNEL, "%s", "wireless_chg suspend wakelock");
    chip->wls_wakelock = wakeup_source_register(NULL, name);

    INIT_LIST_HEAD(&chip->header);
    spin_lock_init(&chip->list_lock);
    init_waitqueue_head(&chip->wait_que);

    INIT_DELAYED_WORK(&chip->wireless_int_work, sc96281_wireless_int_work);
    INIT_DELAYED_WORK(&chip->wireless_pg_det_work, sc96281_pg_det_work);
    INIT_DELAYED_WORK(&chip->chg_monitor_work, sc96281_monitor_work);
    INIT_DELAYED_WORK(&chip->init_detect_work, sc96281_init_detect_work);
    INIT_DELAYED_WORK(&chip->init_fw_check_work, sc96281_init_fw_check_work);
    INIT_DELAYED_WORK(&chip->rx_alarm_work, sc96281_rx_alarm_work);
    INIT_DELAYED_WORK(&chip->i2c_check_work, sc96281_i2c_check_work);
    INIT_DELAYED_WORK(&chip->renegociation_work, sc96281_wireless_renegociation_work);
    INIT_DELAYED_WORK(&chip->reverse_chg_config_work, sc96281_reverse_chg_config_work);
    INIT_DELAYED_WORK(&chip->reverse_chg_monitor_work, sc96281_reverse_chg_monitor_work);
    INIT_DELAYED_WORK(&chip->reverse_transfer_timeout_work, sc96281_reverse_transfer_timeout_work);
    INIT_DELAYED_WORK(&chip->reverse_ping_timeout_work, sc96281_reverse_ping_timeout_work);
    INIT_DELAYED_WORK(&chip->factory_reverse_start_work, sc96281_factory_reverse_start_work);
    INIT_DELAYED_WORK(&chip->factory_reverse_stop_work, sc96281_factory_reverse_stop_work);
    INIT_DELAYED_WORK(&chip->trans_data_work, sc96281_trans_data_work);
    INIT_DELAYED_WORK(&chip->mutex_unlock_work, sc96281_mutex_unlock_work);

    sc96281_parse_dt(chip);
    sc96281_gpio_init(chip);
    if(chip->client->irq) {
        ret = devm_request_threaded_irq(&chip->client->dev, chip->client->irq, NULL,
                sc96281_interrupt_handler,
                (IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_SHARED),
                "sc96281_chg_stat_irq", chip);
        if (ret)
            sc96281_log_err("Failed irq = %d ret = %d\n", chip->client->irq, ret);
    }
    enable_irq_wake(chip->client->irq);
    if (chip->power_good_irq) {
        ret = devm_request_threaded_irq(&chip->client->dev, chip->power_good_irq, NULL,
                sc96281_power_good_handler,
                (IRQF_TRIGGER_FALLING |  IRQF_TRIGGER_RISING | IRQF_ONESHOT),
                "sc96281_power_good_irq", chip);
        if (ret) {
            sc96281_log_err("Failed irq = %d ret = %d\n", chip->power_good_irq, ret);
        }
    }
    enable_irq_wake(chip->power_good_irq);

    /* register charger device for wireless */
    ret = sc96281_chg_init_chgdev(chip);
    if (ret < 0) {
        sc96281_log_err("failed to register wireless chgdev %d\n", ret);
        return -ENODEV;
    }
    // get cp master charger device
    if (!chip->cp_master_dev)
        chip->cp_master_dev = get_charger_by_name("cp_master");
    /* check vote for icl and ichg */
    temp = sc96281_check_votable(chip);
    if (!temp)
        sc96281_log_err("failed to check vote %d\n", temp);
    ret = sysfs_create_group(&chip->dev->kobj, &sc96281_sysfs_group_attrs);
    if (ret < 0)
    {
        sc96281_log_err("sysfs_create_group fail %d\n", ret);
        goto error_sysfs;
    }
    sc96281_log_info("probe node 001\n");
    /* pmic boost  */
    chip->pmic_boost = devm_regulator_get(chip->dev, "pmic_vbus");
    if (IS_ERR(chip->pmic_boost)) {
        sc96281_log_err("failed to get pmic vbus\n");
        goto error_sysfs;
    }
    /* get master cp dev  */
    if (!chip->master_cp_dev)
        chip->master_cp_dev = get_charger_by_name("cp_master");
    if (!chip->master_cp_dev) {
        sc96281_log_err("failed to get master_cp_dev\n");
        //goto error_sysfs;
    }
        /* get master cp dev  */
    if (!chip->chg_dev)
        chip->chg_dev = get_charger_by_name("primary_chg");
    if (!chip->chg_dev) {
        sc96281_log_err("failed to get chg_dev\n");
        //goto error_sysfs;
    }
    /* reset wls charge when power good online */
    schedule_delayed_work(&chip->init_detect_work, msecs_to_jiffies(1000));
    schedule_delayed_work(&chip->init_fw_check_work, msecs_to_jiffies(20 * 1000));
    sc96281_log_info("[%s] success! \n", __func__);
    return 0;
error_sysfs:
    sysfs_remove_group(&chip->dev->kobj, &sc96281_sysfs_group_attrs);
    if (chip->irq_gpio > 0)
        gpio_free(chip->irq_gpio);
    if (chip->power_good_gpio > 0)
        gpio_free(chip->power_good_gpio);
    return 0;
}

static void sc96281_remove(struct i2c_client *client)
{
    struct sc96281_chg *chip = i2c_get_clientdata(client);
    mutex_destroy(&chip->data_transfer_lock);
    mutex_destroy(&chip->i2c_rw_lock);
    mutex_destroy(&chip->wireless_chg_int_lock);

    if (chip->irq_gpio > 0)
        gpio_free(chip->irq_gpio);
    if (chip->power_good_gpio > 0)
        gpio_free(chip->power_good_gpio);

    return;
}

static int sc96281_suspend(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct sc96281_chg *chip = i2c_get_clientdata(client);
    sc96281_log_err("%s in sleep\n", __func__);
    return enable_irq_wake(chip->client->irq);
}

static int sc96281_resume(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct sc96281_chg *chip = i2c_get_clientdata(client);
    sc96281_log_err("%s in sleep\n", __func__);
    return disable_irq_wake(chip->client->irq);
}

static const struct dev_pm_ops sc96281_pm_ops = {
    .suspend    = sc96281_suspend,
    .resume        = sc96281_resume,
};

static void sc96281_shutdown(struct i2c_client *client)
{
    struct sc96281_chg *chip = i2c_get_clientdata(client);
    if (chip->power_good_flag) {
        sc96281_set_enable_mode(chip, false);
        usleep_range(20000, 25000);
        sc96281_set_enable_mode(chip, true);
    }
    sc96281_log_info("%s: shutdown: %s\n", __func__, chip->wlsdev_name);
    return;
}

static const struct of_device_id sc96281_charger_match_table[] = {
    { .compatible = "sc,sc96281-wireless-charger",},
    { },
};

static const struct i2c_device_id sc96281_id[] = {
    {"sc96281", 0},
    {},
};
MODULE_DEVICE_TABLE(i2c, sc96281_id);

static struct i2c_driver sc96281_wireless_charger_driver = {
    .driver        = {
        .name        = "sc96281",
        .owner        = THIS_MODULE,
        .of_match_table    = sc96281_charger_match_table,
        .pm        = &sc96281_pm_ops,
    },
    .probe        = sc96281_probe,
    .remove        = sc96281_remove,
    .shutdown    = sc96281_shutdown,
    .id_table    = sc96281_id,
};
module_i2c_driver(sc96281_wireless_charger_driver);

MODULE_DESCRIPTION("SC SC96281 Wireless Charge Driver");
MODULE_AUTHOR("linjiashuo@xiaomi.com");
MODULE_LICENSE("GPL v2");
