/* SPDX-License-Identifier: GPL-2.0 */
/*
* sc2201.h
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
#ifndef __SC2201_H__
#define __SC2201_H__

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

#define SC2201_DRV_VERSION         "1.0.0_G"
#define SC2201_DEVICE_ID0           0x01
#define SC2201_DEVICE_ID1           0x22
#define SC2201_REG_HVDCP_EN         0x30F5
#define sc2201_err(fmt, ...)        pr_err("SC2201_ERR:" fmt, ##__VA_ARGS__)
#define sc2201_info(fmt, ...)       pr_info("SC2201_INFO:" fmt, ##__VA_ARGS__)
#define CONFIG_ENABLE_SYSFS_DEBUG

#define SC2201_REGNUM               0x10
#define ONLINE_GET_ATTACH(online)	(online & 0xf)

enum sc2201_bc12_type {
	BC12_TYPE_NO_INPUT = 0,
	BC12_TYPE_SDP,
	BC12_TYPE_CDP,
	BC12_TYPE_DCP,
	BC12_TYPE_HVDCP,
	BC12_TYPE_UNKNOWN,
	BC12_TYPE_NONSTAND,
};

enum {
	SC2201_REG_DEVICE_ID0,
	SC2201_REG_DEVICE_ID1,
	SC2201_REG_GENERAL_CTRL,
	SC2201_REG_GENERAL_INT_FLAG,
	SC2201_REG_GENERAL_INT_MASK,
	SC2201_REG_BC1P2_CTRL,
	SC2201_REG_DPDM_INT_FLAG,
	SC2201_REG_DPDM_INT_MASK,
	SC2201_REG_UFCS_CTRL0,
	SC2201_REG_UFCS_CTRL1,
	SC2201_REG_UFCS_INT_FLAG0,
	SC2201_REG_UFCS_INT_FLAG1,
	SC2201_REG_UFCS_INT_FLAG2,
	SC2201_REG_UFCS_INT_MASK0,
	SC2201_REG_UFCS_INT_MASK1,
	SC2201_REG_UFCS_INT_MASK2,
};

enum sc2201_fields {
	DEVICE_ID0, /*reg00*/
	DEVICE_ID1, /*reg01*/
	WD_TIMEOUT, TSD_EN, REG_RST, DPDM_EN,  /*reg02*/
	TSD_STAT, UFCS_FLAG, DPDM_FLAG, WD_TIMEOUT_FLAG, TSD_FLAG, /*reg03*/
	WD_TIMEOUT_MSK, TSD_MSK,  /*reg04*/
	EN_BC1P2, FORCE_BC1P2, /*reg05*/
	DET_DONE_FLAG, BC1P2_TYPE0, DP_PV_FLAG, DM_OV_FLAG, /*reg06*/
	DET_DONE_MASK, DP_OV_MASK, DM_OV_MASK, /*reg07*/
	UFCS_EN, UFCS_HANDSHAKE_EN, BAUD_RATE, SND_CMD, CABLE_HARDRESET, SOURCE_HARDRESET, /*reg08*/
	TX_BUFFER_CLR, RX_BUFFER_CLR, ACK_DISCARD1, ACK_CABLE, ACK_DISCARD2, EN_DM_HIZ, /*reg09*/
};

static enum power_supply_usb_type sc2201_charger_psy_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_DCP,
};

struct sc_reg_field {
	uint32_t reg;
	uint32_t lsb;
	uint32_t msb;
	bool force_write;
};

#define SC_REG_FIELD(_reg, _lsb, _msb) {           \
					.reg = _reg,                \
					.lsb = _lsb,                \
					.msb = _msb,                \
					}

#define SC_REG_FIELD_FORCE_WRITE(_reg, _lsb, _msb) {           \
					.reg = _reg,                \
					.lsb = _lsb,                \
					.msb = _msb,                \
					.force_write = true,        \
					}

//REGISTER
static const struct sc_reg_field sc2201_reg_fields[] = {
	/*reg00*/
	[DEVICE_ID0]            = SC_REG_FIELD(SC2201_REG_DEVICE_ID0, 0, 7),
	/*reg01*/
	[DEVICE_ID1]            = SC_REG_FIELD(SC2201_REG_DEVICE_ID1, 0, 7),
	/*reg02*/
	[WD_TIMEOUT]            = SC_REG_FIELD(SC2201_REG_GENERAL_CTRL, 5, 7),
	[TSD_EN]                = SC_REG_FIELD(SC2201_REG_GENERAL_CTRL, 3, 3),
	[REG_RST]               = SC_REG_FIELD(SC2201_REG_GENERAL_CTRL, 1, 1),
	[DPDM_EN]               = SC_REG_FIELD(SC2201_REG_GENERAL_CTRL, 0, 0),
	/*reg03*/
	[TSD_STAT]              = SC_REG_FIELD(SC2201_REG_GENERAL_INT_FLAG, 7, 7),
	[UFCS_FLAG]             = SC_REG_FIELD(SC2201_REG_GENERAL_INT_FLAG, 3, 3),
	[DPDM_FLAG]             = SC_REG_FIELD(SC2201_REG_GENERAL_INT_FLAG, 2, 2),
	[WD_TIMEOUT_FLAG]       = SC_REG_FIELD(SC2201_REG_GENERAL_INT_FLAG, 1, 1),
	[TSD_FLAG]              = SC_REG_FIELD(SC2201_REG_GENERAL_INT_FLAG, 0, 0),
	/*reg04*/
	[WD_TIMEOUT_MSK]        = SC_REG_FIELD(SC2201_REG_GENERAL_INT_MASK, 1, 1),
	[TSD_MSK]               = SC_REG_FIELD(SC2201_REG_GENERAL_INT_MASK, 0, 0),
	/*reg05*/
	[EN_BC1P2]              = SC_REG_FIELD(SC2201_REG_BC1P2_CTRL, 1, 1),
	[FORCE_BC1P2]           = SC_REG_FIELD_FORCE_WRITE(SC2201_REG_BC1P2_CTRL, 0, 0),
	/*reg06*/
	[DET_DONE_FLAG]         = SC_REG_FIELD(SC2201_REG_DPDM_INT_FLAG, 7, 7),
	[BC1P2_TYPE0]           = SC_REG_FIELD(SC2201_REG_DPDM_INT_FLAG, 4, 6),
	[DP_PV_FLAG]            = SC_REG_FIELD(SC2201_REG_DPDM_INT_FLAG, 2, 2),
	[DM_OV_FLAG]            = SC_REG_FIELD(SC2201_REG_DPDM_INT_FLAG, 1, 1),
	/*reg07*/
	[DET_DONE_MASK]         = SC_REG_FIELD(SC2201_REG_DPDM_INT_MASK, 7, 7),
	[DP_OV_MASK]            = SC_REG_FIELD(SC2201_REG_DPDM_INT_MASK, 2, 2),
	[DM_OV_MASK]            = SC_REG_FIELD(SC2201_REG_DPDM_INT_MASK, 1, 1),
	/*reg08*/
	[UFCS_EN]               = SC_REG_FIELD(SC2201_REG_UFCS_CTRL0, 7, 7),
	[UFCS_HANDSHAKE_EN]     = SC_REG_FIELD(SC2201_REG_UFCS_CTRL0, 5, 5),
	[BAUD_RATE]             = SC_REG_FIELD(SC2201_REG_UFCS_CTRL0, 3, 4),
	[SND_CMD]               = SC_REG_FIELD(SC2201_REG_UFCS_CTRL0, 2, 2),
	[CABLE_HARDRESET]       = SC_REG_FIELD(SC2201_REG_UFCS_CTRL0, 1, 1),
	[SOURCE_HARDRESET]      = SC_REG_FIELD(SC2201_REG_UFCS_CTRL0, 0, 0),
	/*reg09*/
	[TX_BUFFER_CLR]         = SC_REG_FIELD(SC2201_REG_UFCS_CTRL1, 5, 5),
	[RX_BUFFER_CLR]         = SC_REG_FIELD(SC2201_REG_UFCS_CTRL1, 4, 4),
	[ACK_DISCARD1]          = SC_REG_FIELD(SC2201_REG_UFCS_CTRL1, 3, 3),
	[ACK_CABLE]             = SC_REG_FIELD(SC2201_REG_UFCS_CTRL1, 2, 2),
	[ACK_DISCARD2]          = SC_REG_FIELD(SC2201_REG_UFCS_CTRL1, 1, 1),
	[EN_DM_HIZ]             = SC_REG_FIELD(SC2201_REG_UFCS_CTRL1, 0, 0),
};

struct sc2201_param_e {
	int wd_timeout;
	int dpdm_en;
};

struct sc2201_chip {
	struct device *dev;
	struct i2c_client *client;
	struct sc2201_param_e sc2201_param;

	int bc12_type;
	int psy_usb_type;
	int psy_desc_type;

	struct delayed_work force_detect_dwork;
	int force_detect_count;
	struct delayed_work bc12_timeout_dwork;
	bool bc12_detect;
	bool bc12_detect_done;
	struct mutex bc_detect_lock;
	struct power_supply		*xmusb350_psy;
	struct power_supply		*usb_psy;
	struct power_supply_desc xmusb350_psy_desc;

	int irq_gpio;
	int irq;
	int typec_online;
};

extern int sc2201_force_dpdm(struct sc2201_chip *sc);
extern int xm_get_chg_type(struct sc2201_chip *sc);
extern int sc2201_get_charger_type(struct sc2201_chip *sc);
extern void sc2201_removed_reset(struct sc2201_chip *sc);
#endif /* __SC2201_H__ */
