// SPDX-License-Identifier: GPL-2.0
/*
 * aw836xx.c  aw836xx pa module
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

#include <linux/i2c.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/irq.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/gameport.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/vmalloc.h>
#include <uapi/sound/asound.h>
#include <sound/control.h>
#include <sound/soc.h>
#include "aw836xx.h"

#define AW836XX_I2C_NAME "aw836xx_pa"
#define AW836XX_DRIVER_VERSION "v0.1.0"

static LIST_HEAD(g_aw836xx_list);
static DEFINE_MUTEX(g_aw836xx_mutex_lock);
unsigned int g_aw836xx_dev_cnt;

int aw836xx_i2c_write_byte(struct aw_device *aw_dev, uint8_t addr, uint8_t data)
{
	int ret = -1;
	uint8_t cnt = 0;

	while (cnt < 3) {
		ret = i2c_smbus_write_byte_data(aw_dev->i2c, addr, data);
		if (ret < 0) {
			AW_DEV_LOGE(aw_dev->dev, "i2c_write cnt=%d error=%d", cnt, ret);
			cnt++;
			usleep_range(2000, 2500);
			continue;
		}

		break;
	}

	return ret;
}

static int aw836xx_i2c_read_byte(struct aw_device *aw_dev, uint8_t addr, uint8_t *data)
{
	int ret = -1;
	uint8_t cnt = 0;

	while (cnt < 3) {
		ret = i2c_smbus_read_byte_data(aw_dev->i2c, addr);
		if (ret < 0) {
			AW_DEV_LOGE(aw_dev->dev, "i2c_read cnt=%d error=%d", cnt, ret);
			cnt++;
			usleep_range(2000, 2500);
			continue;
		}

		*data = ret;
		break;
	}

	return ret;
}

static int aw836xx_i2c_read_msg(struct aw_device *aw_dev, uint8_t addr, uint8_t *data, uint32_t len)
{
	int ret = -1;

	struct i2c_msg msg[] = {
	[0] = {
		.addr = aw_dev->i2c_addr,
		.flags = 0,
		.len = sizeof(uint8_t),
		.buf = &addr,
		},
	[1] = {
		.addr = aw_dev->i2c_addr,
		.flags = I2C_M_RD,
		.len = len,
		.buf = data,
		},
	};

	ret = i2c_transfer(aw_dev->i2c->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0) {
		AW_DEV_LOGE(aw_dev->dev, "transfer failed");
		return ret;
	} else if (ret != AW_I2C_READ_MSG_NUM) {
		AW_DEV_LOGE(aw_dev->dev, "transfer failed(size error)");
		return -ENXIO;
	}

	return 0;
}

static void aw836xx_ctrl_power(struct aw_device *aw_dev, bool enable)
{
	gpio_set_value_cansleep(aw_dev->sdz_gpio, enable ? AW_GPIO_HIGHT_LEVEL : AW_GPIO_LOW_LEVEL);
}

static void aw836xx_ctrl_mute(struct aw_device *aw_dev, bool enable)
{
	gpio_set_value_cansleep(aw_dev->mute_gpio, enable ? AW_GPIO_HIGHT_LEVEL : AW_GPIO_LOW_LEVEL);
}

static int aw836xx_diagnose(struct aw_device *aw_dev, uint8_t count)
{
	int ret = -1;
	uint8_t data = 0;

	while (count--) {
		aw836xx_ctrl_power(aw_dev, true);
		mdelay(400);

		aw836xx_i2c_read_byte(aw_dev, aw_dev->fault.addr, &data);
		if ((data & aw_dev->fault.mask) != aw_dev->fault.check_val) {
			AW_DEV_LOGE(aw_dev->dev, "fault check error: %d", ret);
			aw836xx_ctrl_power(aw_dev, false);
			continue;
		}

		aw836xx_i2c_read_byte(aw_dev, aw_dev->status.addr, &data);
		if ((data & aw_dev->status.mask) != aw_dev->status.check_val) {
			AW_DEV_LOGE(aw_dev->dev, "status check error: %d", ret);
			aw836xx_ctrl_power(aw_dev, false);
			continue;
		}

		ret = 0;
		break;
	}

	return ret;
}

static int aw836xx_power_down(struct aw836xx *aw836xx)
{
	int ret = 0;

	aw836xx_ctrl_mute(&aw836xx->aw_dev, true);
	aw836xx_ctrl_power(&aw836xx->aw_dev, false);

	return ret;
}

static int aw836xx_power_on(struct aw836xx *aw836xx)
{
	int ret = 0;

	ret = aw836xx_diagnose(&aw836xx->aw_dev, 1);
	if (ret < 0) {
		AW_DEV_LOGE(aw836xx->dev, "load diagnosis failed");
		goto EXIT;
	}

	aw836xx_ctrl_mute(&aw836xx->aw_dev, false);

EXIT:
	return ret;
}

int aw836xx_set_mute(int dev_index, bool mute)
{
	int ret = 0;
	struct list_head *pos = NULL;
	struct aw836xx *aw836xx = NULL;

	list_for_each(pos, &g_aw836xx_list) {
		aw836xx = list_entry(pos, struct aw836xx, list);
		if ((dev_index < 0) || (aw836xx->dev_index == dev_index)) {
			if (mute)
				aw836xx_power_down(aw836xx);
			else
				aw836xx_power_on(aw836xx);
		}
	}

	return ret;
}
EXPORT_SYMBOL(aw836xx_set_mute);

/****************************************************************************
 *
 *aw836xx attribute node
 *
 ****************************************************************************/
static ssize_t reg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	int ret = 0;
	unsigned int i = 0;
	unsigned char reg_val = 0;
	struct aw836xx *aw836xx = dev_get_drvdata(dev);
	struct aw_device *aw_dev = &aw836xx->aw_dev;

	mutex_lock(&aw836xx->reg_lock);
	for (i = 0; i <= aw_dev->reg_max_addr; i++) {
		ret = aw836xx_i2c_read_byte(&aw836xx->aw_dev, i, &reg_val);
		if (ret < 0) {
			len += snprintf(buf + len, PAGE_SIZE - len,
					"read reg [0x%x] failed\n", i);
			AW_DEV_LOGE(aw836xx->dev, "read reg [0x%x] failed", i);
		} else {
			len += snprintf(buf + len, PAGE_SIZE - len,
					"reg:0x%02X=0x%02X\n", i, reg_val);
			AW_DEV_LOGD(aw836xx->dev, "reg:0x%02X=0x%02X",
					i, reg_val);
		}
	}
	mutex_unlock(&aw836xx->reg_lock);

	return len;
}

static ssize_t reg_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t len)
{
	unsigned int databuf[2] = { 0 };
	int ret = 0;
	struct aw836xx *aw836xx = dev_get_drvdata(dev);

	mutex_lock(&aw836xx->reg_lock);
	if (sscanf(buf, "0x%x 0x%x", &databuf[0], &databuf[1]) == 2) {
		if (databuf[0] > aw836xx->aw_dev.reg_max_addr) {
			AW_DEV_LOGE(aw836xx->dev, "set reg[0x%x] error,is out of reg_addr_max[0x%x]",
				databuf[0], aw836xx->aw_dev.reg_max_addr);
			mutex_unlock(&aw836xx->reg_lock);
			return -EINVAL;
		}

		ret = aw836xx_i2c_write_byte(&aw836xx->aw_dev, databuf[0], databuf[1]);
		if (ret < 0)
			AW_DEV_LOGE(aw836xx->dev, "set [0x%x]=0x%x failed",
				databuf[0], databuf[1]);
		else
			AW_DEV_LOGD(aw836xx->dev, "set [0x%x]=0x%x succeed",
				databuf[0], databuf[1]);
	} else {
		AW_DEV_LOGE(aw836xx->dev, "i2c write cmd input error");
	}
	mutex_unlock(&aw836xx->reg_lock);

	return len;
}

static int aw836xx_awrw_write(struct aw836xx *aw836xx,
			const char *buf, size_t count)
{
	int i = 0, ret = -1;
	char *data_buf = NULL;
	int buf_len = 0;
	unsigned int temp_data = 0;
	int data_str_size = 0;
	char *reg_data;
	struct aw_i2c_packet *packet = &aw836xx->i2c_packet;

	AW_DEV_LOGD(aw836xx->dev, "enter");
	/* one addr or one data string Composition of Contains two bytes of symbol(0X)*/
	/* and two byte of hexadecimal data*/
	data_str_size = 2 + 2 * AWRW_DATA_BYTES;

	/* The buf includes the first address of the register to be written and all data */
	buf_len = AWRW_ADDR_BYTES + packet->reg_num * AWRW_DATA_BYTES;
	AW_DEV_LOGI(aw836xx->dev, "buf_len = %d,reg_num = %d", buf_len, packet->reg_num);
	data_buf = vmalloc(buf_len);
	if (data_buf == NULL) {
		AW_DEV_LOGE(aw836xx->dev, "alloc memory failed");
		return -ENOMEM;
	}
	memset(data_buf, 0, buf_len);

	data_buf[0] = packet->reg_addr;
	reg_data = data_buf + 1;

	AW_DEV_LOGD(aw836xx->dev, "reg_addr: 0x%02x", data_buf[0]);

	/*ag:0x00 0x01 0x01 0x01 0x01 0x00\x0a*/
	for (i = 0; i < packet->reg_num; i++) {
		ret = sscanf(buf + AWRW_HDR_LEN + 1 + i * (data_str_size + 1),
			"0x%x", &temp_data);
		if (ret != 1) {
			AW_DEV_LOGE(aw836xx->dev, "sscanf failed,ret=%d", ret);
			vfree(data_buf);
			data_buf = NULL;
			return ret;
		}
		reg_data[i] = temp_data;
		AW_DEV_LOGD(aw836xx->dev, "[%d] : 0x%02x", i, reg_data[i]);
	}

	mutex_lock(&aw836xx->reg_lock);
	ret = i2c_master_send(aw836xx->aw_dev.i2c, data_buf, buf_len);
	if (ret < 0) {
		AW_DEV_LOGE(aw836xx->dev, "write failed");
		vfree(data_buf);
		data_buf = NULL;
		return -EFAULT;
	}
	mutex_unlock(&aw836xx->reg_lock);

	vfree(data_buf);
	data_buf = NULL;

	AW_DEV_LOGD(aw836xx->dev, "down");
	return 0;
}

static int aw836xx_awrw_data_check(struct aw836xx *aw836xx,
			int *data, size_t count)
{
	struct aw_i2c_packet *packet = &aw836xx->i2c_packet;
	int req_data_len = 0;
	int act_data_len = 0;
	int data_str_size = 0;

	if ((data[AWRW_HDR_ADDR_BYTES] != AWRW_ADDR_BYTES) ||
		(data[AWRW_HDR_DATA_BYTES] != AWRW_DATA_BYTES)) {
		AW_DEV_LOGE(aw836xx->dev, "addr_bytes [%d] or data_bytes [%d] unsupport",
			data[AWRW_HDR_ADDR_BYTES], data[AWRW_HDR_DATA_BYTES]);
		return -EINVAL;
	}

	/* one data string Composition of Contains two bytes of symbol(0x)*/
	/* and two byte of hexadecimal data*/
	data_str_size = 2 + 2 * AWRW_DATA_BYTES;
	act_data_len = count - AWRW_HDR_LEN - 1;

	/* There is a comma(,) or space between each piece of data */
	if (data[AWRW_HDR_WR_FLAG] == AWRW_FLAG_WRITE) {
		/*ag:0x00 0x01 0x01 0x01 0x01 0x00\x0a*/
		req_data_len = (data_str_size + 1) * packet->reg_num;
		if (req_data_len > act_data_len) {
			AW_DEV_LOGE(aw836xx->dev, "data_len checkfailed,requeset data_len [%d],actaul data_len [%d]",
				req_data_len, act_data_len);
			return -EINVAL;
		}
	}

	return 0;
}

/* flag addr_bytes data_bytes reg_num reg_addr*/
static int aw836xx_awrw_parse_buf(struct aw836xx *aw836xx,
			const char *buf, size_t count, int *wr_status)
{
	unsigned int data[AWRW_HDR_MAX] = {0};
	struct aw_i2c_packet *packet = &aw836xx->i2c_packet;
	int ret = -1;

	if (sscanf(buf, "0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",
		&data[AWRW_HDR_WR_FLAG], &data[AWRW_HDR_ADDR_BYTES],
		&data[AWRW_HDR_DATA_BYTES], &data[AWRW_HDR_REG_NUM],
		&data[AWRW_HDR_REG_ADDR]) == 5) {

		packet->reg_addr = data[AWRW_HDR_REG_ADDR];
		packet->reg_num = data[AWRW_HDR_REG_NUM];
		*wr_status = data[AWRW_HDR_WR_FLAG];
		ret = aw836xx_awrw_data_check(aw836xx, data, count);
		if (ret < 0)
			return ret;

		return 0;
	}

	return -EINVAL;
}

static ssize_t awrw_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct aw836xx *aw836xx = dev_get_drvdata(dev);
	struct aw_i2c_packet *packet = &aw836xx->i2c_packet;
	int wr_status = 0;
	int ret = -1;

	if (count < AWRW_HDR_LEN) {
		AW_DEV_LOGE(aw836xx->dev, "data count too smaller, please check write format");
		AW_DEV_LOGE(aw836xx->dev, "string %s,count=%ld",
			buf, (u_long)count);
		return -EINVAL;
	}

	AW_DEV_LOGI(aw836xx->dev, "string:[%s],count=%ld", buf, (u_long)count);
	ret = aw836xx_awrw_parse_buf(aw836xx, buf, count, &wr_status);
	if (ret < 0) {
		AW_DEV_LOGE(aw836xx->dev, "can not parse string");
		return ret;
	}

	if (wr_status == AWRW_FLAG_WRITE) {
		ret = aw836xx_awrw_write(aw836xx, buf, count);
		if (ret < 0)
			return ret;
	} else if (wr_status == AWRW_FLAG_READ) {
		packet->status = AWRW_I2C_ST_READ;
		AW_DEV_LOGI(aw836xx->dev, "read_cmd:reg_addr[0x%02x], reg_num[%d]",
			packet->reg_addr, packet->reg_num);
	} else {
		AW_DEV_LOGE(aw836xx->dev, "please check str format, unsupport read_write_status: %d",
			wr_status);
		return -EINVAL;
	}

	return count;
}

static ssize_t awrw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aw836xx *aw836xx = dev_get_drvdata(dev);
	struct aw_i2c_packet *packet = &aw836xx->i2c_packet;
	int data_len = 0;
	size_t len = 0;
	int ret = -1, i = 0;
	char *reg_data = NULL;

	if (packet->status != AWRW_I2C_ST_READ) {
		AW_DEV_LOGE(aw836xx->dev, "please write read cmd first");
		return -EINVAL;
	}

	data_len = AWRW_DATA_BYTES * packet->reg_num;
	reg_data = vmalloc(data_len);
	if (reg_data == NULL) {
		AW_DEV_LOGE(aw836xx->dev, "memory alloc failed");
		ret = -EINVAL;
		goto exit;
	}

	mutex_lock(&aw836xx->reg_lock);
	ret = aw836xx_i2c_read_msg(&aw836xx->aw_dev, packet->reg_addr,
				(char *)reg_data, data_len);
	if (ret < 0) {
		ret = -EFAULT;
		mutex_unlock(&aw836xx->reg_lock);
		goto exit;
	}
	mutex_unlock(&aw836xx->reg_lock);

	AW_DEV_LOGI(aw836xx->dev, "reg_addr 0x%02x, reg_num %d",
		packet->reg_addr, packet->reg_num);

	for (i = 0; i < data_len; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len,
			"0x%02x,", reg_data[i]);
		AW_DEV_LOGI(aw836xx->dev, "0x%02x", reg_data[i]);
	}

	ret = len;

exit:
	if (reg_data) {
		vfree(reg_data);
		reg_data = NULL;
	}
	packet->status = AWRW_I2C_ST_NONE;
	return ret;
}

static ssize_t drv_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
		"driver_ver: %s\n", AW836XX_DRIVER_VERSION);

	return len;
}

static DEVICE_ATTR_RW(reg);
static DEVICE_ATTR_RW(awrw);
static DEVICE_ATTR_RO(drv_ver);

static struct attribute *aw836xx_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_awrw.attr,
	&dev_attr_drv_ver.attr,
	NULL
};

static struct attribute_group aw836xx_attribute_group = {
	.attrs = aw836xx_attributes
};

static int aw836xx_malloc_init(struct i2c_client *client, struct aw836xx **aw836xx)
{
	int ret = 0;

	*aw836xx = devm_kzalloc(&client->dev, sizeof(struct aw836xx), GFP_KERNEL);
	if (*aw836xx == NULL) {
		AW_DEV_LOGE(&client->dev, "failed to devm_kzalloc aw836xx");
		ret = -ENOMEM;
		goto EXIT;
	}

	(*aw836xx)->dev = &client->dev;
	(*aw836xx)->aw_dev.dev = &client->dev;
	(*aw836xx)->aw_dev.i2c_bus = client->adapter->nr;
	(*aw836xx)->aw_dev.i2c_addr = client->addr;
	(*aw836xx)->aw_dev.i2c = client;

	mutex_init(&((*aw836xx)->reg_lock));

	AW_DEV_LOGI(&client->dev, "struct aw836xx devm_kzalloc and init down");

EXIT:
	return ret;
}

static int aw836xx_dtsi_parse(struct aw836xx *aw836xx, struct device_node *dev_node)
{
	int ret = 0;

	ret = of_get_named_gpio(dev_node, "sdz-gpio", 0);
	if (ret < 0) {
		AW_DEV_LOGI(aw836xx->dev, "no sdz gpio provided");
		goto EXIT;
	}
	aw836xx->aw_dev.sdz_gpio = ret;
	AW_DEV_LOGI(aw836xx->dev, "sdz gpio[%d] parse succeed", ret);

	ret = of_get_named_gpio(dev_node, "mute-gpio", 0);
	if (ret < 0) {
		AW_DEV_LOGI(aw836xx->dev, "no mute gpio provided");
		goto EXIT;
	}
	aw836xx->aw_dev.mute_gpio = ret;
	AW_DEV_LOGI(aw836xx->dev, "mute gpio[%d] parse succeed", ret);

	if (gpio_is_valid(aw836xx->aw_dev.sdz_gpio)) {
		ret = devm_gpio_request_one(aw836xx->dev, aw836xx->aw_dev.sdz_gpio,
					GPIOF_OUT_INIT_LOW, "aw836xx_sdz");
		if (ret < 0) {
			AW_DEV_LOGE(aw836xx->dev, "sdz gpio request failed");
			goto EXIT;
		}
	}

	if (gpio_is_valid(aw836xx->aw_dev.mute_gpio)) {
		ret = devm_gpio_request_one(aw836xx->dev, aw836xx->aw_dev.mute_gpio,
					GPIOF_OUT_INIT_HIGH, "aw836xx_mute");
		if (ret < 0) {
			AW_DEV_LOGE(aw836xx->dev, "mute gpio request failed");
			goto EXIT;
		}
	}

EXIT:
	return ret;
}

static int aw836xx_get_chipid(struct aw_device *aw_dev)
{
	int ret = -1;
	unsigned int cnt = 0;
	unsigned char reg_val_l = 0;
	unsigned char reg_val_h = 0;

	for (cnt = 0; cnt < AW_READ_CHIPID_RETRIES; cnt++) {
		ret = aw836xx_i2c_read_byte(aw_dev, AW836XX_CHIPIDL_REG, &reg_val_l);
		if (ret < 0) {
			AW_DEV_LOGE(aw_dev->dev, "[%d] read low id is failed, ret=%d",
				cnt, ret);
			continue;
		}
		break;
	}
	if (cnt == AW_READ_CHIPID_RETRIES) {
		AW_DEV_LOGE(aw_dev->dev, "read low id is failed");
		return -EINVAL;
	}

	for (cnt = 0; cnt < AW_READ_CHIPID_RETRIES; cnt++) {
		ret = aw836xx_i2c_read_byte(aw_dev, AW836XX_CHIPIDH_REG, &reg_val_h);
		if (ret < 0) {
			AW_DEV_LOGE(aw_dev->dev, "[%d] read high id is failed, ret=%d",
				cnt, ret);
			continue;
		}
		break;
	}
	if (cnt == AW_READ_CHIPID_RETRIES) {
		AW_DEV_LOGE(aw_dev->dev, "read high id is failed");
		return -EINVAL;
	}

	aw_dev->chipid = (int)reg_val_l | (int)reg_val_h << 8;
	AW_DEV_LOGI(aw_dev->dev, "read chipid (0x%x) succeed", aw_dev->chipid);

	return 0;
}

static int aw836xx_common_init(struct aw_device *aw_dev,
	const struct aw_dev_property *property)
{
	int ret = 0;

	aw_dev->chipid = property->id;
	aw_dev->reg_max_addr = property->max_addr;

	aw_dev->fault.addr = AW836XX_FAULT_REG;
	aw_dev->fault.mask = AW836XX_FAULT_CHECK_MASK;
	aw_dev->fault.check_val = AW836XX_FAULT_CHECK_VAL;

	aw_dev->status.addr = AW836XX_STATUS_REG;
	aw_dev->status.mask = AW836XX_STATUS_CHECK_MASK;
	aw_dev->status.check_val = AW836XX_STATUS_CHECK_VAL;

	return ret;
}

static int aw836xx_pid_11_init(struct aw_device *aw_dev)
{
	int ret = 0;

	aw_dev->reg_max_addr = AW836XX_PID_11_REG_MAX;

	aw_dev->fault.addr = AW836XX_PID_11_FAULT_REG;
	aw_dev->fault.mask = AW836XX_FAULT_CHECK_MASK;
	aw_dev->fault.check_val = AW836XX_FAULT_CHECK_VAL;

	aw_dev->status.addr = AW836XX_PID_11_STATUS_REG;
	aw_dev->status.mask = AW836XX_STATUS_CHECK_MASK;
	aw_dev->status.check_val = AW836XX_STATUS_CHECK_VAL;

	return ret;
}

const struct aw_dev_property g_aw_dev_property_registry[] = {
	{
		.id = 0x11,
		.dev_init_func = aw836xx_pid_11_init,
	}
};

static int aw836xx_check_chip_model(struct aw_device *aw_dev)
{
	int ret  = -EINVAL;
	int i = 0;

	for (i = 0; i < sizeof(g_aw_dev_property_registry) / sizeof(struct aw_dev_property); i++) {
		if (aw_dev->chipid == g_aw_dev_property_registry[i].id) {
			if (g_aw_dev_property_registry[i].dev_init_func != NULL)
				ret = g_aw_dev_property_registry[i].dev_init_func(aw_dev);
			else
				ret = aw836xx_common_init(aw_dev, &g_aw_dev_property_registry[i]);

			if (ret < 0)
				AW_DEV_LOGE(aw_dev->dev, "product is pid_%x init failed",
						g_aw_dev_property_registry[i].id);
			else
				AW_DEV_LOGI(aw_dev->dev, "product is pid_%x class",
						g_aw_dev_property_registry[i].id);

			ret = 0;
			break;
		}
	}

	return ret;
}

static int aw836xx_chip_init(struct aw_device *aw_dev)
{
	int ret = 0;

	ret = aw836xx_check_chip_model(aw_dev);
	if (ret == 0)
		goto EXIT;

	aw_dev->chipid &= 0xFF;
	ret = aw836xx_check_chip_model(aw_dev);
	if (ret < 0) {
		AW_DEV_LOGE(aw_dev->dev, "unsupported device revision [0x%x]", aw_dev->chipid);
		goto EXIT;
	}

EXIT:
	return ret;
}

static int aw836xx_dev_init(struct aw_device *aw_dev)
{
	int ret = 0;

	aw836xx_ctrl_power(aw_dev, true);
	mdelay(400);

	ret = aw836xx_get_chipid(aw_dev);
	if (ret < 0)
		goto EXIT;

	ret = aw836xx_chip_init(aw_dev);
	if (ret < 0)
		goto EXIT;

	ret = aw836xx_diagnose(aw_dev, 3);
	if (ret < 0) {
		AW_DEV_LOGE(aw_dev->dev, "load diagnosis failed");
		goto EXIT;
	}
	aw836xx_ctrl_power(aw_dev, false);

EXIT:
	return ret;
}

static int aw836xx_i2c_probe(struct i2c_client *client)
{
	struct device_node *dev_node = client->dev.of_node;
	struct aw836xx *aw836xx = NULL;
	int ret = -1;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		AW_DEV_LOGE(&client->dev, "check_functionality failed");
		ret = -ENODEV;
		goto EXIT;
	}

	ret = aw836xx_malloc_init(client, &aw836xx);
	if (ret < 0)
		goto EXIT;

	i2c_set_clientdata(client, aw836xx);

	ret = aw836xx_dtsi_parse(aw836xx, dev_node);
	if (ret < 0)
		goto EXIT;

	ret = aw836xx_dev_init(&aw836xx->aw_dev);
	if (ret < 0)
		goto EXIT;

	ret = sysfs_create_group(&aw836xx->dev->kobj, &aw836xx_attribute_group);
	if (ret < 0)
		ret = 0;
		AW_DEV_LOGE(aw836xx->dev, "failed to create sysfs nodes, will not allowed to use");

	/* enable wake source */
	device_init_wakeup(aw836xx->dev, true);

	/*add device to total list */
	mutex_lock(&g_aw836xx_mutex_lock);
	aw836xx->dev_index = g_aw836xx_dev_cnt;
	g_aw836xx_dev_cnt++;
	list_add(&aw836xx->list, &g_aw836xx_list);
	mutex_unlock(&g_aw836xx_mutex_lock);

	AW_DEV_LOGI(aw836xx->dev, "succeed, dev_index=[%d], g_aw836xx_dev_cnt= [%d]",
			aw836xx->dev_index, g_aw836xx_dev_cnt);

EXIT:
	if ((ret < 0) && (aw836xx != NULL)) {
		AW_DEV_LOGE(aw836xx->dev, "pa init failed");
		aw836xx_ctrl_power(&aw836xx->aw_dev, false);
		devm_kfree(&client->dev, aw836xx);
	}

	return ret;
}

#ifdef AW_KERNEL_VER_OVER_6_1_0
static void aw836xx_i2c_remove(struct i2c_client *client)
#else
static int aw836xx_i2c_remove(struct i2c_client *client)
#endif
{
	struct aw836xx *aw836xx = i2c_get_clientdata(client);

	sysfs_remove_group(&aw836xx->dev->kobj, &aw836xx_attribute_group);

	mutex_lock(&g_aw836xx_mutex_lock);
	g_aw836xx_dev_cnt--;
	list_del(&aw836xx->list);
	mutex_unlock(&g_aw836xx_mutex_lock);

#ifdef AW_KERNEL_VER_OVER_6_1_0
#else
	return 0;
#endif
}

static const struct i2c_device_id aw836xx_i2c_id[] = {
	{AW836XX_I2C_NAME, 0},
	{},
};

static const struct of_device_id extpa_of_match[] = {
	{.compatible = "awinic,aw836xx_pa"},
	{},
};

#ifdef CONFIG_PM
static int aw836xx_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct aw836xx *aw836xx = i2c_get_clientdata(client);

	AW_DEV_LOGI(&client->dev, "enter");
	aw836xx->is_suspend = false;

	return 0;
}

static int aw836xx_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct aw836xx *aw836xx = i2c_get_clientdata(client);

	AW_DEV_LOGI(&client->dev, "enter");
	aw836xx->is_suspend = true;

	return 0;
}

static const struct dev_pm_ops aw836xx_dev_pm_ops = {
	.suspend = aw836xx_i2c_suspend,
	.resume  = aw836xx_i2c_resume,
};
#endif

static struct i2c_driver aw836xx_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = AW836XX_I2C_NAME,
		.of_match_table = extpa_of_match,
#ifdef CONFIG_PM
		.pm = &aw836xx_dev_pm_ops,
#endif
		},
	.probe = aw836xx_i2c_probe,
	.remove = aw836xx_i2c_remove,
	.id_table = aw836xx_i2c_id,
};

static int __init aw836xx_pa_init(void)
{
	int ret;

	AW_LOGI("driver version: %s", AW836XX_DRIVER_VERSION);

	ret = i2c_add_driver(&aw836xx_i2c_driver);
	if (ret < 0) {
		AW_LOGE("Unable to register driver, ret= %d", ret);
		return ret;
	}
	return 0;
}

static void __exit aw836xx_pa_exit(void)
{
	AW_LOGI("enter");
	i2c_del_driver(&aw836xx_i2c_driver);
}

module_init(aw836xx_pa_init);
module_exit(aw836xx_pa_exit);

MODULE_AUTHOR("<wangpeng@awinic.com>");
MODULE_DESCRIPTION("awinic aw836xx pa driver");
MODULE_LICENSE("GPL");
