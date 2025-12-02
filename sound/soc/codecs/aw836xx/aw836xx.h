/* SPDX-License-Identifier: GPL-2.0
 *
 * aw836xx.h  aw836xx pa module
 *
 * Copyright (c) 2024 AWINIC Technology CO., LTD
 *
 * Author: Leon <wangpeng@awinic.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#ifndef __AW836XX_H__
#define __AW836XX_H__

#include <linux/version.h>
#include <linux/kernel.h>
#include <sound/control.h>
#include <sound/soc.h>

#define AW_KERNEL_VER_OVER_6_1_0

#define AW_LOGI(fmt, ...)\
	pr_info("[Awinic] %s:" fmt "\n", __func__, ##__VA_ARGS__)

#define AW_LOGD(fmt, ...)\
	pr_info("[Awinic] %s:" fmt "\n", __func__, ##__VA_ARGS__)

#define AW_LOGE(fmt, ...)\
	pr_info("[Awinic] %s:" fmt "\n", __func__, ##__VA_ARGS__)

#define AW_DEV_LOGI(dev, fmt, ...)\
	pr_info("[Awinic] [%s]%s: " fmt "\n", dev_name(dev), __func__, ##__VA_ARGS__)

#define AW_DEV_LOGD(dev, fmt, ...)\
	pr_info("[Awinic] [%s]%s: " fmt "\n", dev_name(dev), __func__, ##__VA_ARGS__)

#define AW_DEV_LOGE(dev, fmt, ...)\
	pr_info("[Awinic] [%s]%s: " fmt "\n", dev_name(dev), __func__, ##__VA_ARGS__)

#define AW_I2C_READ_MSG_NUM (2)
#define AW_DEV_REG_RD_ACCESS (1 << 0)
#define AW_DEV_REG_WR_ACCESS (1 << 1)
#define AWRW_ADDR_BYTES (1)
#define AWRW_DATA_BYTES (1)
#define AWRW_HDR_LEN (24)
#define AW_GPIO_HIGHT_LEVEL (1)
#define AW_GPIO_LOW_LEVEL (0)
#define AW_READ_CHIPID_RETRIES (3)

#define AW836XX_CHIPIDL_REG (0x00)
#define AW836XX_CHIPIDH_REG (0x01)
#define AW836XX_FAULT_REG (0x02)
#define AW836XX_OC_START (3)
#define AW836XX_OC_LEN (1)
#define AW836XX_OC_MASK (((1 << AW836XX_OC_LEN) - 1) << AW836XX_OC_START)
#define AW836XX_OC_DISABLE (0)
#define AW836XX_OC_DISABLE_VAL (AW836XX_OC_DISABLE << AW836XX_OC_START)
#define AW836XX_UVP_START (4)
#define AW836XX_UVP_LEN (1)
#define AW836XX_UVP_MASK (((1 << AW836XX_UVP_LEN) - 1) << AW836XX_UVP_START)
#define AW836XX_UVP_DISABLE (0)
#define AW836XX_UVP_DISABLE_VAL (AW836XX_UVP_DISABLE << AW836XX_UVP_START)
#define AW836XX_OVP_START (5)
#define AW836XX_OVP_LEN (1)
#define AW836XX_OVP_MASK (((1 << AW836XX_OVP_LEN) - 1) << AW836XX_OVP_START)
#define AW836XX_OVP_DISABLE (0)
#define AW836XX_OVP_DISABLE_VAL (AW836XX_OVP_DISABLE << AW836XX_OVP_START)
#define AW836XX_DC_START (6)
#define AW836XX_DC_LEN (1)
#define AW836XX_DC_MASK (((1 << AW836XX_DC_LEN) - 1) << AW836XX_DC_START)
#define AW836XX_DC_DISABLE (0)
#define AW836XX_DC_DISABLE_VAL (AW836XX_DC_DISABLE << AW836XX_DC_START)
#define AW836XX_OT_START (7)
#define AW836XX_OT_LEN (1)
#define AW836XX_OT_MASK (((1 << AW836XX_OT_LEN) - 1) << AW836XX_OT_START)
#define AW836XX_OT_DISABLE (0)
#define AW836XX_OT_DISABLE_VAL (AW836XX_OT_DISABLE << AW836XX_OT_START)
#define AW836XX_FAULT_CHECK_MASK \
	(AW836XX_OC_MASK | AW836XX_UVP_MASK | AW836XX_OVP_MASK | AW836XX_DC_MASK | AW836XX_OT_MASK)
#define AW836XX_FAULT_CHECK_VAL \
	(AW836XX_OC_DISABLE_VAL \
	| AW836XX_UVP_DISABLE_VAL \
	| AW836XX_OVP_DISABLE_VAL \
	| AW836XX_DC_DISABLE_VAL \
	| AW836XX_OT_DISABLE_VAL)
#define AW836XX_STATUS_REG (0x03)
#define AW836XX_S2P_START (0)
#define AW836XX_S2P_LEN (1)
#define AW836XX_S2P_MASK (((1 << AW836XX_S2P_LEN) - 1) << AW836XX_S2P_START)
#define AW836XX_S2P_DISABLE (0)
#define AW836XX_S2P_DISABLE_VAL (AW836XX_S2P_DISABLE << AW836XX_S2P_START)
#define AW836XX_S2G_START (1)
#define AW836XX_S2G_LEN (1)
#define AW836XX_S2G_MASK (((1 << AW836XX_S2G_LEN) - 1) << AW836XX_S2G_START)
#define AW836XX_S2G_DISABLE (0)
#define AW836XX_S2G_DISABLE_VAL (AW836XX_S2G_DISABLE << AW836XX_S2G_START)
#define AW836XX_OL_START (2)
#define AW836XX_OL_LEN (1)
#define AW836XX_OL_MASK (((1 << AW836XX_OL_LEN) - 1) << AW836XX_OL_START)
#define AW836XX_OL_DISABLE (0)
#define AW836XX_OL_DISABLE_VAL (AW836XX_OL_DISABLE << AW836XX_OL_START)
#define AW836XX_SL_START (3)
#define AW836XX_SL_LEN (1)
#define AW836XX_SL_MASK (((1 << AW836XX_SL_LEN) - 1) << AW836XX_SL_START)
#define AW836XX_SL_DISABLE (0)
#define AW836XX_SL_DISABLE_VAL (AW836XX_SL_DISABLE << AW836XX_SL_START)
#define AW836XX_FAULT_START (4)
#define AW836XX_FAULT_LEN (1)
#define AW836XX_FAULT_MASK (((1 << AW836XX_FAULT_LEN) - 1) << AW836XX_FAULT_START)
#define AW836XX_FAULT_DISABLE (1)
#define AW836XX_FAULT_DISABLE_VAL (AW836XX_FAULT_DISABLE << AW836XX_FAULT_START)
#define AW836XX_STATUS_CHECK_MASK \
	(AW836XX_S2P_MASK | AW836XX_S2G_MASK | AW836XX_OL_MASK | AW836XX_SL_MASK | AW836XX_FAULT_MASK)
#define AW836XX_STATUS_CHECK_VAL \
	(AW836XX_S2P_DISABLE_VAL \
	| AW836XX_S2G_DISABLE_VAL \
	| AW836XX_OL_DISABLE_VAL \
	| AW836XX_SL_DISABLE_VAL \
	| AW836XX_FAULT_DISABLE_VAL)

#define AW836XX_PID_11_REG_MAX (0x7F)
#define AW836XX_PID_11_FAULT_REG (0x01)
#define AW836XX_PID_11_STATUS_REG (0x02)

enum {
	AWRW_FLAG_WRITE = 0,
	AWRW_FLAG_READ,
};

enum {
	AWRW_I2C_ST_NONE = 0,
	AWRW_I2C_ST_READ,
	AWRW_I2C_ST_WRITE,
};

enum {
	AWRW_HDR_WR_FLAG = 0,
	AWRW_HDR_ADDR_BYTES,
	AWRW_HDR_DATA_BYTES,
	AWRW_HDR_REG_NUM,
	AWRW_HDR_REG_ADDR,
	AWRW_HDR_MAX,
};

struct aw_i2c_packet {
	char status;
	unsigned int reg_num;
	unsigned int reg_addr;
	char *reg_data;
};

struct aw_addr_check_desc {
	uint8_t addr;
	uint8_t mask;
	uint8_t check_val;
};

struct aw_device {
	uint8_t i2c_addr;
	int chipid;
	int i2c_bus;
	int sdz_gpio;
	int mute_gpio;
	int reg_max_addr;

	struct device *dev;
	struct i2c_client *i2c;
	struct aw_addr_check_desc fault;
	struct aw_addr_check_desc status;
};

typedef int (*dev_init_func)(struct aw_device *aw_dev);

struct aw_dev_property {
	uint8_t max_addr;
	int id;
	dev_init_func dev_init_func;
};

struct aw836xx {
	bool is_suspend;
	uint8_t dev_index;
	struct device *dev;
	struct mutex reg_lock;
	struct aw_device aw_dev;
	struct aw_i2c_packet i2c_packet;
	struct list_head list;
};

#endif
