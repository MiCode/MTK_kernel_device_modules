/*
 * oca72xxx_device.c  oca72xxx pa module
 *
 * Copyright (c) 2021 OCS Technology CO., LTD
 *
 * Author: Wall <Wall@orient-chip.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/timer.h>
#include "oca72xxx.h"
#include "oca72xxx_device.h"
#include "oca72xxx_log.h"
#include "oca72xxx_pid_9b_reg.h"
#include "oca72xxx_pid_18_reg.h"
#include "oca72xxx_pid_39_reg.h"
#include "oca72xxx_pid_59_3x9_reg.h"
#include "oca72xxx_pid_59_5x9_reg.h"
#include "oca72xxx_pid_5a_reg.h"
#include "oca72xxx_pid_76_reg.h"
#include "oca72xxx_pid_60_reg.h"

/*************************************************************************
 * oca72xxx variable
 ************************************************************************/
const char *g_oca_pid_9b_product[] = {
	"oca72319",
};
const char *g_oca_pid_18_product[] = {
	"oca72358",
};

const char *g_oca_pid_39_product[] = {
	"oca72329",
	"oca72339",
	"oca72349",
};

const char *g_oca_pid_59_3x9_product[] = {
	"oca72359",
	"oca72389",
};

const char *g_oca_pid_59_5x9_product[] = {
	"oca72509",
	"oca72519",
	"oca72529",
	"oca72539",
};

const char *g_oca_pid_5a_product[] = {
	"oca72549",
	"oca72559",
	"oca72569",
	"oca72579",
	"oca71509",
	"oca72579G",
};

const char *g_oca_pid_76_product[] = {
	"oca72390",
	"oca72320",
	"oca72401",
	"oca72360",
	"aw87390",
};

const char *g_oca_pid_60_product[] = {
	"oca72560",
	"oca72561",
	"oca72562",
	"oca72501",
	"oca72550",
};

static int oca72xxx_dev_get_chipid(struct oca_device *oca_dev);

/***************************************************************************
 *
 * reading and writing of I2C bus
 *
 ***************************************************************************/
int oca72xxx_dev_i2c_write_byte(struct oca_device *oca_dev,
			uint8_t reg_addr, uint8_t reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < OCA_I2C_RETRIES) {
		ret = i2c_smbus_write_byte_data(oca_dev->i2c, reg_addr, reg_data);
		if (ret < 0)
			OCA_DEV_LOGE(oca_dev->dev, "i2c_write cnt=%d error=%d",
				cnt, ret);
		else
			break;

		cnt++;
		msleep(OCA_I2C_RETRY_DELAY);
	}

	return ret;
}

int oca72xxx_dev_i2c_read_byte(struct oca_device *oca_dev,
			uint8_t reg_addr, uint8_t *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < OCA_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(oca_dev->i2c, reg_addr);
		if (ret < 0) {
			OCA_DEV_LOGE(oca_dev->dev, "i2c_read cnt=%d error=%d",
				cnt, ret);
		} else {
			*reg_data = ret;
			break;
		}
		cnt++;
		msleep(OCA_I2C_RETRY_DELAY);
	}

	return ret;
}

int oca72xxx_dev_i2c_read_msg(struct oca_device *oca_dev,
	uint8_t reg_addr, uint8_t *data_buf, uint32_t data_len)
{
	int ret = -1;

	struct i2c_msg msg[] = {
	[0] = {
		.addr = oca_dev->i2c_addr,
		.flags = 0,
		.len = sizeof(uint8_t),
		.buf = &reg_addr,
		},
	[1] = {
		.addr = oca_dev->i2c_addr,
		.flags = I2C_M_RD,
		.len = data_len,
		.buf = data_buf,
		},
	};

	ret = i2c_transfer(oca_dev->i2c->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0) {
		OCA_DEV_LOGE(oca_dev->dev, "transfer failed");
		return ret;
	} else if (ret != OCA_I2C_READ_MSG_NUM) {
		OCA_DEV_LOGE(oca_dev->dev, "transfer failed(size error)");
		return -ENXIO;
	}

	return 0;
}

int oca72xxx_dev_i2c_write_bits(struct oca_device *oca_dev,
	uint8_t reg_addr, uint8_t mask, uint8_t reg_data)
{
	int ret = -1;
	unsigned char reg_val = 0;

	ret = oca72xxx_dev_i2c_read_byte(oca_dev, reg_addr, &reg_val);
	if (ret < 0) {
		OCA_DEV_LOGE(oca_dev->dev, "i2c read error, ret=%d", ret);
		return ret;
	}
	reg_val &= mask;
	reg_val |= (reg_data & (~mask));
	ret = oca72xxx_dev_i2c_write_byte(oca_dev, reg_addr, reg_val);
	if (ret < 0) {
		OCA_DEV_LOGE(oca_dev->dev, "i2c write error, ret=%d", ret);
		return ret;
	}

	return 0;
}

/************************************************************************
 *
 * oca72xxx device update profile data to registers
 *
 ************************************************************************/
static int oca72xxx_dev_reg_update(struct oca_device *oca_dev,
			struct oca_data_container *profile_data)
{
	int i = 0;
	int ret = -1;

	if (profile_data == NULL)
		return -EINVAL;

	if (oca_dev->hwen_status == OCA_DEV_HWEN_OFF) {
		OCA_DEV_LOGE(oca_dev->dev, "dev is pwr_off,can not update reg");
		return -EINVAL;
	}

	for (i = 0; i < profile_data->len; i = i + 2) {
		OCA_DEV_LOGI(oca_dev->dev, "reg=0x%02x, val = 0x%02x",
			profile_data->data[i], profile_data->data[i + 1]);

		//delay ms
		if (profile_data->data[i] == OCA72XXX_DELAY_REG_ADDR) {
			OCA_DEV_LOGI(oca_dev->dev, "delay %d ms", profile_data->data[i + 1]);
			usleep_range(profile_data->data[i + 1] * OCA72XXX_REG_DELAY_TIME, \
				profile_data->data[i + 1] * OCA72XXX_REG_DELAY_TIME + 10);
			continue;
		}

		ret = oca72xxx_dev_i2c_write_byte(oca_dev, profile_data->data[i],
				profile_data->data[i + 1]);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static void oca72xxx_dev_reg_mute_bits_set(struct oca_device *oca_dev,
				uint8_t *reg_val, bool enable)
{
	if (enable) {
		*reg_val &= oca_dev->mute_desc.mask;
		*reg_val |= oca_dev->mute_desc.enable;
	} else {
		*reg_val &= oca_dev->mute_desc.mask;
		*reg_val |= oca_dev->mute_desc.disable;
	}
}

static int oca72xxx_dev_reg_update_mute(struct oca_device *oca_dev,
			struct oca_data_container *profile_data)
{
	int i = 0;
	int ret = -1;
	uint8_t reg_val = 0;

	if (profile_data == NULL)
		return -EINVAL;

	if (oca_dev->hwen_status == OCA_DEV_HWEN_OFF) {
		OCA_DEV_LOGE(oca_dev->dev, "hwen is off,can not update reg");
		return -EINVAL;
	}

	if (oca_dev->mute_desc.mask == OCA_DEV_REG_INVALID_MASK) {
		OCA_DEV_LOGE(oca_dev->dev, "mute ctrl mask invalid");
		return -EINVAL;
	}

	for (i = 0; i < profile_data->len; i = i + 2) {
		OCA_DEV_LOGI(oca_dev->dev, "reg=0x%02x, val = 0x%02x",
			profile_data->data[i], profile_data->data[i + 1]);
		//delay ms
		if (profile_data->data[i] == OCA72XXX_DELAY_REG_ADDR) {
			OCA_DEV_LOGI(oca_dev->dev, "delay %d ms", profile_data->data[i + 1]);
			usleep_range(profile_data->data[i + 1] * OCA72XXX_REG_DELAY_TIME, \
				profile_data->data[i + 1] * OCA72XXX_REG_DELAY_TIME + 10);
			continue;
		}

		reg_val = profile_data->data[i + 1];
		if (profile_data->data[i] == oca_dev->mute_desc.addr) {
			oca72xxx_dev_reg_mute_bits_set(oca_dev, &reg_val, true);
			OCA_DEV_LOGD(oca_dev->dev, "change mute_mask, val = 0x%02x",
				reg_val);
		}

		ret = oca72xxx_dev_i2c_write_byte(oca_dev, profile_data->data[i], reg_val);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/************************************************************************
 *
 * oca72xxx device hadware and soft contols
 *
 ************************************************************************/
static bool oca72xxx_dev_gpio_is_valid(struct oca_device *oca_dev)
{
	if (gpio_is_valid(oca_dev->rst_gpio))
		return true;
	else
		return false;
}

void oca72xxx_dev_hw_pwr_ctrl(struct oca_device *oca_dev, bool enable)
{
	if (oca_dev->hwen_status == OCA_DEV_HWEN_INVALID) {
		OCA_DEV_LOGD(oca_dev->dev, "product not have reset-pin,hardware pwd control invalid");
		return;
	}
	if (enable) {
		if (oca72xxx_dev_gpio_is_valid(oca_dev)) {
			gpio_set_value_cansleep(oca_dev->rst_gpio, OCA_GPIO_LOW_LEVEL);
			mdelay(2);
			gpio_set_value_cansleep(oca_dev->rst_gpio, OCA_GPIO_HIGHT_LEVEL);
			mdelay(2);
			oca_dev->hwen_status = OCA_DEV_HWEN_ON;
			OCA_DEV_LOGI(oca_dev->dev, "hw power on");
		} else {
			OCA_DEV_LOGI(oca_dev->dev, "hw already power on");
		}
	} else {
		if (oca72xxx_dev_gpio_is_valid(oca_dev)) {
			gpio_set_value_cansleep(oca_dev->rst_gpio, OCA_GPIO_LOW_LEVEL);
			mdelay(2);
			oca_dev->hwen_status = OCA_DEV_HWEN_OFF;
			OCA_DEV_LOGI(oca_dev->dev, "hw power off");
		} else {
			OCA_DEV_LOGI(oca_dev->dev, "hw already power off");
		}
	}
}

static int oca72xxx_dev_mute_ctrl(struct oca_device *oca_dev, bool enable)
{
	int ret = 0;

	if (enable) {
		ret = oca72xxx_dev_i2c_write_bits(oca_dev, oca_dev->mute_desc.addr,
				oca_dev->mute_desc.mask, oca_dev->mute_desc.enable);
		if (ret < 0)
			return ret;
		OCA_DEV_LOGI(oca_dev->dev, "set mute down");
	} else {
		ret = oca72xxx_dev_i2c_write_bits(oca_dev, oca_dev->mute_desc.addr,
				oca_dev->mute_desc.mask, oca_dev->mute_desc.disable);
		if (ret < 0)
			return ret;
		OCA_DEV_LOGI(oca_dev->dev, "close mute down");
	}

	return 0;
}

void oca72xxx_dev_soft_reset(struct oca_device *oca_dev)
{
	int i = 0;
	int ret = -1;
	struct oca_soft_rst_desc *soft_rst = &oca_dev->soft_rst_desc;

	OCA_DEV_LOGD(oca_dev->dev, "enter");

	if (oca_dev->hwen_status == OCA_DEV_HWEN_OFF) {
		OCA_DEV_LOGE(oca_dev->dev, "hw is off,can not softrst");
		return;
	}

	if (oca_dev->soft_rst_enable == OCA_DEV_SOFT_RST_DISENABLE) {
		OCA_DEV_LOGD(oca_dev->dev, "softrst is disenable");
		return;
	}

	if (soft_rst->access == NULL || soft_rst->len == 0) {
		OCA_DEV_LOGE(oca_dev->dev, "softrst_info not init");
		return;
	}

	if (soft_rst->len % 2) {
		OCA_DEV_LOGE(oca_dev->dev, "softrst data_len[%d] is odd number,data not available",
			oca_dev->soft_rst_desc.len);
		return;
	}

	for (i = 0; i < soft_rst->len; i += 2) {
		OCA_DEV_LOGD(oca_dev->dev, "softrst_reg=0x%02x, val = 0x%02x",
			soft_rst->access[i], soft_rst->access[i + 1]);

		ret = oca72xxx_dev_i2c_write_byte(oca_dev, soft_rst->access[i],
				soft_rst->access[i + 1]);
		if (ret < 0) {
			OCA_DEV_LOGE(oca_dev->dev, "write failed,ret = %d,cnt=%d",
				ret, i);
			return;
		}
	}
	OCA_DEV_LOGD(oca_dev->dev, "down");
}


int oca72xxx_dev_default_pwr_off(struct oca_device *oca_dev,
		struct oca_data_container *profile_data)
{
	int ret = 0;

	OCA_DEV_LOGD(oca_dev->dev, "enter");
	if (oca_dev->hwen_status == OCA_DEV_HWEN_OFF) {
		OCA_DEV_LOGE(oca_dev->dev, "hwen is already off");
		return 0;
	}

	if (oca_dev->soft_off_enable && profile_data) {
		ret = oca72xxx_dev_reg_update(oca_dev, profile_data);
		if (ret < 0) {
			OCA_DEV_LOGE(oca_dev->dev, "update profile[Off] fw config failed");
			goto reg_off_update_failed;
		}
	}

	oca72xxx_dev_hw_pwr_ctrl(oca_dev, false);
	OCA_DEV_LOGD(oca_dev->dev, "down");
	return 0;

reg_off_update_failed:
	oca72xxx_dev_hw_pwr_ctrl(oca_dev, false);
	return ret;
}


/************************************************************************
 *
 * oca72xxx device power on process function
 *
 ************************************************************************/

int oca72xxx_dev_default_pwr_on(struct oca_device *oca_dev,
			struct oca_data_container *profile_data)
{
	int ret = 0;

	/*hw power on*/
	oca72xxx_dev_hw_pwr_ctrl(oca_dev, true);

	ret = oca72xxx_dev_reg_update(oca_dev, profile_data);
	if (ret < 0)
		return ret;

	return 0;
}

/****************************************************************************
 *
 * oca72xxx chip esd status check
 *
 ****************************************************************************/
int oca72xxx_dev_esd_reg_status_check(struct oca_device *oca_dev)
{
	int ret;
	unsigned char reg_val = 0;
	struct oca_esd_check_desc *esd_desc = &oca_dev->esd_desc;

	OCA_DEV_LOGD(oca_dev->dev, "enter");

	if (!esd_desc->first_update_reg_addr) {
		OCA_DEV_LOGE(oca_dev->dev, "esd check info if not init,please check");
		return -EINVAL;
	}

	ret = oca72xxx_dev_i2c_read_byte(oca_dev, esd_desc->first_update_reg_addr,
			&reg_val);
	if (ret < 0) {
		OCA_DEV_LOGE(oca_dev->dev, "read reg 0x%02x failed",
			esd_desc->first_update_reg_addr);
		return ret;
	}

	OCA_DEV_LOGD(oca_dev->dev, "0x%02x:default val=0x%02x real val=0x%02x",
		esd_desc->first_update_reg_addr,
		esd_desc->first_update_reg_val, reg_val);

	if (reg_val == esd_desc->first_update_reg_val) {
		OCA_DEV_LOGE(oca_dev->dev, "reg status check failed");
		return -EINVAL;
	}
	return 0;
}

int oca72xxx_dev_check_reg_is_rec_mode(struct oca_device *oca_dev)
{
	int ret;
	unsigned char reg_val = 0;
	struct oca_rec_mode_desc *rec_desc = &oca_dev->rec_desc;

	if (!rec_desc->addr) {
		OCA_DEV_LOGE(oca_dev->dev, "rec check info if not init,please check");
		return -EINVAL;
	}

	ret = oca72xxx_dev_i2c_read_byte(oca_dev, rec_desc->addr, &reg_val);
	if (ret < 0) {
		OCA_DEV_LOGE(oca_dev->dev, "read reg 0x%02x failed",
			rec_desc->addr);
		return ret;
	}

	if (rec_desc->enable) {
		if (reg_val & ~(rec_desc->mask)) {
			OCA_DEV_LOGI(oca_dev->dev, "reg status is receiver mode");
			oca_dev->is_rec_mode = OCA_IS_REC_MODE;
		} else {
			oca_dev->is_rec_mode = OCA_NOT_REC_MODE;
		}
	} else {
		if (!(reg_val & ~(rec_desc->mask))) {
			OCA_DEV_LOGI(oca_dev->dev, "reg status is receiver mode");
			oca_dev->is_rec_mode = OCA_IS_REC_MODE;
		} else {
			oca_dev->is_rec_mode = OCA_NOT_REC_MODE;
		}
	}
	return 0;
}


/****************************************************************************
 *
 * oca72xxx product attributes init info
 *
 ****************************************************************************/

/********************** oca72xxx_pid_9A attributes ***************************/

static int oca_dev_pid_9b_reg_update(struct oca_device *oca_dev,
			struct oca_data_container *profile_data)
{
	int i = 0;
	int ret = -1;
	uint8_t reg_val = 0;

	if (profile_data == NULL)
		return -EINVAL;

	if (oca_dev->hwen_status == OCA_DEV_HWEN_OFF) {
		OCA_DEV_LOGE(oca_dev->dev, "dev is pwr_off,can not update reg");
		return -EINVAL;
	}

	if (profile_data->len != OCA_PID_9B_BIN_REG_CFG_COUNT) {
		OCA_DEV_LOGE(oca_dev->dev, "reg_config count of bin is error,can not update reg");
		return -EINVAL;
	}
	ret = oca72xxx_dev_i2c_write_byte(oca_dev, OCA72XXX_PID_9B_ENCRYPTION_REG,
		OCA72XXX_PID_9B_ENCRYPTION_BOOST_OUTPUT_SET);
	if (ret < 0)
		return ret;

	for (i = 1; i < OCA_PID_9B_BIN_REG_CFG_COUNT; i++) {
		OCA_DEV_LOGI(oca_dev->dev, "reg=0x%02x, val = 0x%02x",
			i, profile_data->data[i]);
		//delay ms
		if (profile_data->data[i] == OCA72XXX_DELAY_REG_ADDR) {
			OCA_DEV_LOGI(oca_dev->dev, "delay %d ms", profile_data->data[i + 1]);
			usleep_range(profile_data->data[i + 1] * OCA72XXX_REG_DELAY_TIME, \
				profile_data->data[i + 1] * OCA72XXX_REG_DELAY_TIME + 10);
			continue;
		}

		reg_val = profile_data->data[i];
		if (i == OCA72XXX_PID_9B_SYSCTRL_REG) {
			oca72xxx_dev_reg_mute_bits_set(oca_dev, &reg_val, true);
			OCA_DEV_LOGD(oca_dev->dev, "change mute_mask, val = 0x%02x",
				reg_val);
		}

		ret = oca72xxx_dev_i2c_write_byte(oca_dev, i, reg_val);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int oca_dev_pid_9b_pwr_on(struct oca_device *oca_dev, struct oca_data_container *data)
{
	int ret = 0;

	/*hw power on*/
	oca72xxx_dev_hw_pwr_ctrl(oca_dev, true);

	/* open the mute */
	ret = oca72xxx_dev_mute_ctrl(oca_dev, true);
	if (ret < 0)
		return ret;

	/* Update scene parameters in mute mode */
	ret = oca_dev_pid_9b_reg_update(oca_dev, data);
	if (ret < 0)
		return ret;

	/* close the mute */
	ret = oca72xxx_dev_mute_ctrl(oca_dev, false);
	if (ret < 0)
		return ret;

	return 0;
}

static void oca_dev_pid_9b_init(struct oca_device *oca_dev)
{
	/* Product register permission info */
	oca_dev->reg_max_addr = OCA72XXX_PID_9B_REG_MAX;
	oca_dev->reg_access = oca72xxx_pid_9b_reg_access;

	oca_dev->mute_desc.addr = OCA72XXX_PID_9B_SYSCTRL_REG;
	oca_dev->mute_desc.mask = OCA72XXX_PID_9B_REG_EN_SW_MASK;
	oca_dev->mute_desc.enable = OCA72XXX_PID_9B_REG_EN_SW_DISABLE_VALUE;
	oca_dev->mute_desc.disable = OCA72XXX_PID_9B_REG_EN_SW_ENABLE_VALUE;
	oca_dev->ops.pwr_on_func = oca_dev_pid_9b_pwr_on;

	/* software reset control info */
	oca_dev->soft_rst_desc.len = sizeof(oca72xxx_pid_9b_softrst_access);
	oca_dev->soft_rst_desc.access = oca72xxx_pid_9b_softrst_access;
	oca_dev->soft_rst_enable = OCA_DEV_SOFT_RST_ENABLE;

	/* Whether to allow register operation to power off */
	oca_dev->soft_off_enable = OCA_DEV_SOFT_OFF_DISENABLE;

	oca_dev->product_tab = g_oca_pid_9b_product;
	oca_dev->product_cnt = OCA72XXX_PID_9B_PRODUCT_MAX;

	oca_dev->rec_desc.addr = OCA72XXX_PID_9B_SYSCTRL_REG;
	oca_dev->rec_desc.disable = OCA72XXX_PID_9B_SPK_MODE_ENABLE;
	oca_dev->rec_desc.enable = OCA72XXX_PID_9B_SPK_MODE_DISABLE;
	oca_dev->rec_desc.mask = OCA72XXX_PID_9B_SPK_MODE_MASK;

	/* esd reg info */
	oca_dev->esd_desc.first_update_reg_addr = OCA72XXX_PID_9B_SYSCTRL_REG;
	oca_dev->esd_desc.first_update_reg_val = OCA72XXX_PID_9B_SYSCTRL_DEFAULT;

	oca_dev->vol_desc.addr = OCA_REG_NONE;
}

static int oca_dev_pid_9a_init(struct oca_device *oca_dev)
{
	int ret = 0;

	ret = oca72xxx_dev_i2c_write_byte(oca_dev, OCA72XXX_PID_9B_ENCRYPTION_REG,
		OCA72XXX_PID_9B_ENCRYPTION_BOOST_OUTPUT_SET);
	if (ret < 0) {
		OCA_DEV_LOGE(oca_dev->dev, "write 0x64=0x2C error");
		return -EINVAL;
	}

	ret = oca72xxx_dev_get_chipid(oca_dev);
	if (ret < 0) {
		OCA_DEV_LOGE(oca_dev->dev, "read chipid is failed,ret=%d", ret);
		return ret;
	}

	if (oca_dev->chipid == OCA_DEV_CHIPID_9B) {
		OCA_DEV_LOGI(oca_dev->dev, "product is pid_9B class");
		oca_dev_pid_9b_init(oca_dev);
	} else {
		OCA_DEV_LOGE(oca_dev->dev, "product is not pid_9B class，not support");
		return -EINVAL;
	}

	return 0;
}

/********************** oca72xxx_pid_9b attributes end ***********************/

/********************** oca72xxx_pid_18 attributes ***************************/
static int oca_dev_pid_18_pwr_on(struct oca_device *oca_dev, struct oca_data_container *data)
{
	int ret = 0;

	/*hw power on*/
	oca72xxx_dev_hw_pwr_ctrl(oca_dev, true);

	/* open the mute */
	ret = oca72xxx_dev_mute_ctrl(oca_dev, true);
	if (ret < 0)
		return ret;

	/* Update scene parameters in mute mode */
	ret = oca72xxx_dev_reg_update_mute(oca_dev, data);
	if (ret < 0)
		return ret;

	/* close the mute */
	ret = oca72xxx_dev_mute_ctrl(oca_dev, false);
	if (ret < 0)
		return ret;

	return 0;
}

static void oca_dev_chipid_18_init(struct oca_device *oca_dev)
{
	/* Product register permission info */
	oca_dev->reg_max_addr = OCA72XXX_PID_18_REG_MAX;
	oca_dev->reg_access = oca72xxx_pid_18_reg_access;

	oca_dev->mute_desc.addr = OCA72XXX_PID_18_SYSCTRL_REG;
	oca_dev->mute_desc.mask = OCA72XXX_PID_18_REG_EN_SW_MASK;
	oca_dev->mute_desc.enable = OCA72XXX_PID_18_REG_EN_SW_DISABLE_VALUE;
	oca_dev->mute_desc.disable = OCA72XXX_PID_18_REG_EN_SW_ENABLE_VALUE;
	oca_dev->ops.pwr_on_func = oca_dev_pid_18_pwr_on;

	/* software reset control info */
	oca_dev->soft_rst_desc.len = sizeof(oca72xxx_pid_18_softrst_access);
	oca_dev->soft_rst_desc.access = oca72xxx_pid_18_softrst_access;
	oca_dev->soft_rst_enable = OCA_DEV_SOFT_RST_ENABLE;

	/* Whether to allow register operation to power off */
	oca_dev->soft_off_enable = OCA_DEV_SOFT_OFF_ENABLE;

	oca_dev->product_tab = g_oca_pid_18_product;
	oca_dev->product_cnt = OCA72XXX_PID_18_PRODUCT_MAX;

	oca_dev->rec_desc.addr = OCA72XXX_PID_18_SYSCTRL_REG;
	oca_dev->rec_desc.disable = OCA72XXX_PID_18_REG_REC_MODE_DISABLE;
	oca_dev->rec_desc.enable = OCA72XXX_PID_18_REG_REC_MODE_ENABLE;
	oca_dev->rec_desc.mask = OCA72XXX_PID_18_REG_REC_MODE_MASK;

	/* esd reg info */
	oca_dev->esd_desc.first_update_reg_addr = OCA72XXX_PID_18_CLASSD_REG;
	oca_dev->esd_desc.first_update_reg_val = OCA72XXX_PID_18_CLASSD_DEFAULT;

	oca_dev->vol_desc.addr = OCA72XXX_PID_18_CPOC_REG;
}
/********************** oca72xxx_pid_18 attributes end ***********************/

/********************** oca72xxx_pid_39 attributes ***************************/
static void oca_dev_chipid_39_init(struct oca_device *oca_dev)
{
	/* Product register permission info */
	oca_dev->reg_max_addr = OCA72XXX_PID_39_REG_MAX;
	oca_dev->reg_access = oca72xxx_pid_39_reg_access;

	/* software reset control info */
	oca_dev->soft_rst_desc.len = sizeof(oca72xxx_pid_39_softrst_access);
	oca_dev->soft_rst_desc.access = oca72xxx_pid_39_softrst_access;
	oca_dev->soft_rst_enable = OCA_DEV_SOFT_RST_ENABLE;

	/* Whether to allow register operation to power off */
	oca_dev->soft_off_enable = OCA_DEV_SOFT_OFF_ENABLE;

	oca_dev->product_tab = g_oca_pid_39_product;
	oca_dev->product_cnt = OCA72XXX_PID_39_PRODUCT_MAX;

	oca_dev->rec_desc.addr = OCA72XXX_PID_39_REG_MODECTRL;
	oca_dev->rec_desc.disable = OCA72XXX_PID_39_REC_MODE_DISABLE;
	oca_dev->rec_desc.enable = OCA72XXX_PID_39_REC_MODE_ENABLE;
	oca_dev->rec_desc.mask = OCA72XXX_PID_39_REC_MODE_MASK;

	/* esd reg info */
	oca_dev->esd_desc.first_update_reg_addr = OCA72XXX_PID_39_REG_MODECTRL;
	oca_dev->esd_desc.first_update_reg_val = OCA72XXX_PID_39_MODECTRL_DEFAULT;

	oca_dev->vol_desc.addr = OCA72XXX_PID_39_REG_CPOVP;
}
/********************* oca72xxx_pid_39 attributes end *************************/


/********************* oca72xxx_pid_59_5x9 attributes *************************/
static void oca_dev_chipid_59_5x9_init(struct oca_device *oca_dev)
{
	/* Product register permission info */
	oca_dev->reg_max_addr = OCA72XXX_PID_59_5X9_REG_MAX;
	oca_dev->reg_access = oca72xxx_pid_59_5x9_reg_access;

	/* software reset control info */
	oca_dev->soft_rst_desc.len = sizeof(oca72xxx_pid_59_5x9_softrst_access);
	oca_dev->soft_rst_desc.access = oca72xxx_pid_59_5x9_softrst_access;
	oca_dev->soft_rst_enable = OCA_DEV_SOFT_RST_ENABLE;

	/* Whether to allow register operation to power off */
	oca_dev->soft_off_enable = OCA_DEV_SOFT_OFF_ENABLE;

	oca_dev->product_tab = g_oca_pid_59_5x9_product;
	oca_dev->product_cnt = OCA72XXX_PID_59_5X9_PRODUCT_MAX;

	oca_dev->rec_desc.addr = OCA72XXX_PID_59_5X9_REG_SYSCTRL;
	oca_dev->rec_desc.disable = OCA72XXX_PID_59_5X9_REC_MODE_DISABLE;
	oca_dev->rec_desc.enable = OCA72XXX_PID_59_5X9_REC_MODE_ENABLE;
	oca_dev->rec_desc.mask = OCA72XXX_PID_59_5X9_REC_MODE_MASK;

	/* esd reg info */
	oca_dev->esd_desc.first_update_reg_addr = OCA72XXX_PID_59_5X9_REG_ENCR;
	oca_dev->esd_desc.first_update_reg_val = OCA72XXX_PID_59_5X9_ENCRY_DEFAULT;

	oca_dev->vol_desc.addr = OCA_REG_NONE;
}
/******************* oca72xxx_pid_59_5x9 attributes end ***********************/

/********************* oca72xxx_pid_59_3x9 attributes *************************/
static void oca_dev_chipid_59_3x9_init(struct oca_device *oca_dev)
{
	/* Product register permission info */
	oca_dev->reg_max_addr = OCA72XXX_PID_59_3X9_REG_MAX;
	oca_dev->reg_access = oca72xxx_pid_59_3x9_reg_access;

	/* software reset control info */
	oca_dev->soft_rst_desc.len = sizeof(oca72xxx_pid_59_3x9_softrst_access);
	oca_dev->soft_rst_desc.access = oca72xxx_pid_59_3x9_softrst_access;
	oca_dev->soft_rst_enable = OCA_DEV_SOFT_RST_ENABLE;

	/* Whether to allow register operation to power off */
	oca_dev->soft_off_enable = OCA_DEV_SOFT_OFF_ENABLE;

	oca_dev->product_tab = g_oca_pid_59_3x9_product;
	oca_dev->product_cnt = OCA72XXX_PID_59_3X9_PRODUCT_MAX;

	oca_dev->rec_desc.addr = OCA72XXX_PID_59_3X9_REG_MDCRTL;
	oca_dev->rec_desc.disable = OCA72XXX_PID_59_3X9_SPK_MODE_ENABLE;
	oca_dev->rec_desc.enable = OCA72XXX_PID_59_3X9_SPK_MODE_DISABLE;
	oca_dev->rec_desc.mask = OCA72XXX_PID_59_3X9_SPK_MODE_MASK;

	/* esd reg info */
	oca_dev->esd_desc.first_update_reg_addr = OCA72XXX_PID_59_3X9_REG_ENCR;
	oca_dev->esd_desc.first_update_reg_val = OCA72XXX_PID_59_3X9_ENCR_DEFAULT;

	oca_dev->vol_desc.addr = OCA72XXX_PID_59_3X9_REG_CPOVP;
}
/******************* oca72xxx_pid_59_3x9 attributes end ***********************/

/********************** oca72xxx_pid_5a attributes ****************************/
static void oca_dev_chipid_5a_init(struct oca_device *oca_dev)
{
	/* Product register permission info */
	oca_dev->reg_max_addr = OCA72XXX_PID_5A_REG_MAX;
	oca_dev->reg_access = oca72xxx_pid_5a_reg_access;

	/* software reset control info */
	oca_dev->soft_rst_desc.len = sizeof(oca72xxx_pid_5a_softrst_access);
	oca_dev->soft_rst_desc.access = oca72xxx_pid_5a_softrst_access;
	oca_dev->soft_rst_enable = OCA_DEV_SOFT_RST_ENABLE;

	/* Whether to allow register operation to power off */
	oca_dev->soft_off_enable = OCA_DEV_SOFT_OFF_ENABLE;

	oca_dev->product_tab = g_oca_pid_5a_product;
	oca_dev->product_cnt = OCA72XXX_PID_5A_PRODUCT_MAX;

	oca_dev->rec_desc.addr = OCA72XXX_PID_5A_REG_SYSCTRL_REG;
	oca_dev->rec_desc.disable = OCA72XXX_PID_5A_REG_RCV_MODE_DISABLE;
	oca_dev->rec_desc.enable = OCA72XXX_PID_5A_REG_RCV_MODE_ENABLE;
	oca_dev->rec_desc.mask = OCA72XXX_PID_5A_REG_RCV_MODE_MASK;

	/* esd reg info */
	oca_dev->esd_desc.first_update_reg_addr = OCA72XXX_PID_5A_REG_DFT3R_REG;
	oca_dev->esd_desc.first_update_reg_val = OCA72XXX_PID_5A_DFT3R_DEFAULT;

	oca_dev->vol_desc.addr = OCA_REG_NONE;
}
/********************** oca72xxx_pid_5a attributes end ************************/

/********************** oca72xxx_pid_76 attributes ****************************/
static void oca_dev_chipid_76_init(struct oca_device *oca_dev)
{
	/* Product register permission info */
	oca_dev->reg_max_addr = OCA72XXX_PID_76_REG_MAX;
	oca_dev->reg_access = oca72xxx_pid_76_reg_access;

	/* software reset control info */
	oca_dev->soft_rst_desc.len = sizeof(oca72xxx_pid_76_softrst_access);
	oca_dev->soft_rst_desc.access = oca72xxx_pid_76_softrst_access;
	oca_dev->soft_rst_enable = OCA_DEV_SOFT_RST_ENABLE;

	/* software power off control info */
	oca_dev->soft_off_enable = OCA_DEV_SOFT_OFF_ENABLE;

	oca_dev->product_tab = g_oca_pid_76_product;
	oca_dev->product_cnt = OCA72XXX_PID_76_PROFUCT_MAX;

	oca_dev->rec_desc.addr = OCA72XXX_PID_76_MDCTRL_REG;
	oca_dev->rec_desc.disable = OCA72XXX_PID_76_EN_SPK_ENABLE;
	oca_dev->rec_desc.enable = OCA72XXX_PID_76_EN_SPK_DISABLE;
	oca_dev->rec_desc.mask = OCA72XXX_PID_76_EN_SPK_MASK;

	/* esd reg info */
	oca_dev->esd_desc.first_update_reg_addr = OCA72XXX_PID_76_DFT_ADP1_REG;
	oca_dev->esd_desc.first_update_reg_val = OCA72XXX_PID_76_DFT_ADP1_CHECK;

	oca_dev->vol_desc.addr = OCA72XXX_PID_76_CPOVP_REG;
}
/********************** oca72xxx_pid_76 attributes end ************************/

/********************** oca72xxx_pid_60 attributes ****************************/
static void oca_dev_chipid_60_init(struct oca_device *oca_dev)
{
	/* Product register permission info */
	oca_dev->reg_max_addr = OCA72XXX_PID_60_REG_MAX;
	oca_dev->reg_access = oca72xxx_pid_60_reg_access;

	/* software reset control info */
	oca_dev->soft_rst_desc.len = sizeof(oca72xxx_pid_60_softrst_access);
	oca_dev->soft_rst_desc.access = oca72xxx_pid_60_softrst_access;
	oca_dev->soft_rst_enable = OCA_DEV_SOFT_RST_ENABLE;

	/* software power off control info */
	oca_dev->soft_off_enable = OCA_DEV_SOFT_OFF_ENABLE;

	oca_dev->product_tab = g_oca_pid_60_product;
	oca_dev->product_cnt = OCA72XXX_PID_60_PROFUCT_MAX;

	oca_dev->rec_desc.addr = OCA72XXX_PID_60_SYSCTRL_REG;
	oca_dev->rec_desc.disable = OCA72XXX_PID_60_RCV_MODE_DISABLE;
	oca_dev->rec_desc.enable = OCA72XXX_PID_60_RCV_MODE_ENABLE;
	oca_dev->rec_desc.mask = OCA72XXX_PID_60_RCV_MODE_MASK;

	/* esd reg info */
	oca_dev->esd_desc.first_update_reg_addr = OCA72XXX_PID_60_NG3_REG;
	oca_dev->esd_desc.first_update_reg_val = OCA72XXX_PID_60_ESD_REG_VAL;

	oca_dev->vol_desc.addr = OCA_REG_NONE;
}
/********************** oca72xxx_pid_60 attributes end ************************/

#if IS_ENABLED(CONFIG_LCT_AUDIO_INFO)
extern int lct_audio_info_set_pa_name(const char *pa_name, int count);
#endif
#define OCA_DEV_NAME "oca72xx"

static int oca_dev_chip_init(struct oca_device *oca_dev)
{
	int ret  = 0;

	/*get info by chipid*/
	switch (oca_dev->chipid) {
	case OCA_DEV_CHIPID_9A:
		ret = oca_dev_pid_9a_init(oca_dev);
		if (ret < 0)
			OCA_DEV_LOGE(oca_dev->dev, "product is pid_9B init failed");
		break;
	case OCA_DEV_CHIPID_9B:
		oca_dev_pid_9b_init(oca_dev);
		OCA_DEV_LOGI(oca_dev->dev, "product is pid_9B class");
		break;
	case OCA_DEV_CHIPID_18:
		oca_dev_chipid_18_init(oca_dev);
		OCA_DEV_LOGI(oca_dev->dev, "product is pid_18 class");
		break;
	case OCA_DEV_CHIPID_39:
		oca_dev_chipid_39_init(oca_dev);
		OCA_DEV_LOGI(oca_dev->dev, "product is pid_39 class");
		break;
	case OCA_DEV_CHIPID_59:
		if (oca72xxx_dev_gpio_is_valid(oca_dev)) {
			oca_dev_chipid_59_5x9_init(oca_dev);
			OCA_DEV_LOGI(oca_dev->dev, "product is pid_59_5x9 class");
		} else {
			oca_dev_chipid_59_3x9_init(oca_dev);
			OCA_DEV_LOGI(oca_dev->dev, "product is pid_59_3x9 class");
		}
		break;
	case OCA_DEV_CHIPID_5A:
		oca_dev_chipid_5a_init(oca_dev);
		#if IS_ENABLED(CONFIG_LCT_AUDIO_INFO)
		lct_audio_info_set_pa_name(OCA_DEV_NAME, strlen(OCA_DEV_NAME));
		#endif
		OCA_DEV_LOGI(oca_dev->dev, "product is pid_5A class");
		break;
	case OCA_DEV_CHIPID_76:
		oca_dev_chipid_76_init(oca_dev);
		OCA_DEV_LOGI(oca_dev->dev, "product is pid_76 class");
		break;
	case OCA_DEV_CHIPID_60:
		oca_dev_chipid_60_init(oca_dev);
		OCA_DEV_LOGI(oca_dev->dev, "product is pid_60 class");
		break;
	default:
		OCA_DEV_LOGE(oca_dev->dev, "unsupported device revision [0x%x]",
			oca_dev->chipid);
		return -EINVAL;
	}

	return 0;
}

static int oca72xxx_dev_get_chipid(struct oca_device *oca_dev)
{
	int ret = -1;
	unsigned int cnt = 0;
	unsigned char reg_val = 0;

	for (cnt = 0; cnt < OCA_READ_CHIPID_RETRIES; cnt++) {
		ret = oca72xxx_dev_i2c_read_byte(oca_dev, OCA_DEV_REG_CHIPID, &reg_val);
		if (ret < 0) {
			OCA_DEV_LOGE(oca_dev->dev, "[%d] read chip is failed, ret=%d",
				cnt, ret);
			continue;
		}
		break;
	}


	if (cnt == OCA_READ_CHIPID_RETRIES) {
		OCA_DEV_LOGE(oca_dev->dev, "read chip is failed,cnt=%d", cnt);
		return -EINVAL;
	}

	OCA_DEV_LOGI(oca_dev->dev, "read chipid[0x%x] succeed", reg_val);
	oca_dev->chipid = reg_val;

	return 0;
}

int oca72xxx_dev_init(struct oca_device *oca_dev)
{
	int ret = -1;

	ret = oca72xxx_dev_get_chipid(oca_dev);
	if (ret < 0) {
		OCA_DEV_LOGE(oca_dev->dev, "read chipid is failed,ret=%d", ret);
		return ret;
	}

	ret = oca_dev_chip_init(oca_dev);

	return ret;
}


