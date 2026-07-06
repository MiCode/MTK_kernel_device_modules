/* SPDX-License-Identifier: GPL-2.0 */
/*
* sc7601.h
*
* fuelgauge reg
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
#ifndef __SC7601_H__
#define __SC7601_H__

#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/regmap.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/time.h>


#define BAT_OVP_TH    5500
#define PRE_CHG_CURR  100
#define VFC_CHG_VOLT  3100
#define LIMIT_CHG_CURR  7000
#define SC760X_I2C_MAX_CHECK_TIMES 3

enum sc760x_role {
    SC760X_MASTER,
    SC760X_SLAVE,
};

// typedef enum {
//     SC760_ADC_IBAT = 0,
//     SC760_ADC_VBAT,
//     SC760_ADC_VCHG,
//     SC760_ADC_TBAT,
//     SC760_ADC_TDIE,
// }SC760_ADC_CH;


// typedef enum {
//     WORK_FULLY_OFF = 0,
//     WORK_TRICKLE_CHARGE,
//     WORK_PRE_CHARGE,
//     WORK_FULLY_ON,
//     WORK_CURRENT_REGULATION,
//     WORK_POWER_REGULATION,
// }WORK_MODE;

struct sc7601_device {
    struct i2c_client *client;
    struct device *dev;
    struct regmap *regmap;
    int bat_ovp_th;
    int pre_chg_curr;
    int vfc_chg_volt;
    int ibat_limit;
    bool chip_ok;
    int loadsw_role;
    char log_tag[25];
    struct charger_device *chg_dev;
    struct charger_properties chg_props;
};


#endif /* __SC7601_H__ */