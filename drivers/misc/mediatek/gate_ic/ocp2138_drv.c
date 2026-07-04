// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/gpio/consumer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include "ocp2138_drv.h"

#if IS_ENABLED(CONFIG_LEDS_MTK)
#define LEDS_BRIGHTNESS_CHANGED
#include <linux/leds-mtk.h>
#endif

/*****************************************************************************
 * Define
 *****************************************************************************/
#define GATE_I2C_ID_NAME "gate_ic_i2c_ocp2138"

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME " %s(%d) :[GATE][I2C] " fmt, __func__, __LINE__

/*****************************************************************************
 * Define Register
 *****************************************************************************/
#define LCM_BIAS			0x03
#define VPOS_BIAS			0x00
#define VNEG_BIAS			0x01

//extern char *saved_command_line;
int bias_id = 2;
/*****************************************************************************
 * GLobal Variable
 *****************************************************************************/
static const struct of_device_id _gate_ic_i2c_of_match[] = {
	{
		.compatible = "mediatek,lcd_bias",
	},
	{}
};

static struct i2c_client *_gate_ic_i2c_client;

struct gate_ic_client {
	struct i2c_client *i2c_client;
	struct device *dev;
	atomic_t gate_ic_power_status;
};

/*****************************************************************************
 * Driver Functions
 *****************************************************************************/
int _gate_ic_read_bytes(unsigned char addr, unsigned char *returnData)
{
	char cmd_buf[2] = { 0x00, 0x00 };
	char readData = 0;
	int ret = 0;
	struct i2c_client *client = _gate_ic_i2c_client;

	if (client == NULL) {
		pr_info("ERROR!! _gate_ic_i2c_client is null\n");
		return 0;
	}

	cmd_buf[0] = addr;
	ret = i2c_master_send(client, &cmd_buf[0], 1);
	ret = i2c_master_recv(client, &cmd_buf[1], 1);
	if (ret < 0)
		pr_info("ERROR %d!! i2c read data 0x%0x fail !!\n", ret, addr);

	readData = cmd_buf[1];
	*returnData = readData;

	return ret;
}
EXPORT_SYMBOL_GPL(_gate_ic_read_bytes);

int _gate_ic_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = _gate_ic_i2c_client;
	char write_data[2] = { 0 };

	if (client == NULL) {
		pr_info("ERROR!! _gate_ic_i2c_client is null\n");
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_info("ERROR %d!! i2c write data fail 0x%0x, 0x%0x !!\n",
				ret, addr, value);

	return ret;
}
EXPORT_SYMBOL_GPL(_gate_ic_write_bytes);

void _gate_ic_panel_bias_enable(unsigned int power_status)
{
	pr_err("%s+\n", __func__);

	if (power_status) {
		/* set bias to 6.0v */
		_gate_ic_write_bytes(VPOS_BIAS, 0x14);
		_gate_ic_write_bytes(VNEG_BIAS, 0x14);

		/* bias enable */
		_gate_ic_write_bytes(LCM_BIAS, 0x33);
	} else {
		_gate_ic_write_bytes(LCM_BIAS, 0x30);
	}
}
EXPORT_SYMBOL_GPL(_gate_ic_panel_bias_enable);

/*****************************************************************************
 * Function
 *****************************************************************************/

static int _gate_ic_i2c_probe(struct i2c_client *client)
{

//	char *bkl_ptr = NULL;
	struct gate_ic_client *gate_client;
	//struct device *dev = &client->dev;


	pr_info("%s+: client name=%s addr=0x%x\n", __func__, client->name, client->addr);

#if 0
	bkl_ptr = (char *)strnstr(saved_command_line, "lcd_bias_ic=", strlen(saved_command_line));
	bkl_ptr += strlen("lcd_bias_ic=");
	bias_id = simple_strtol(bkl_ptr, NULL, 10);
	pr_info("[%s]:----bias_id = %d-----\n", __func__, bias_id);

#endif 

	//if (bias_id != 02) {
		pr_info("bias ic is not ocp2138");
		return -ENODEV;
	//}

	gate_client = devm_kzalloc(&client->dev, sizeof(struct gate_ic_client), GFP_KERNEL);

	if (!gate_client)
		return -ENOMEM;

	gate_client->dev = &client->dev;
	gate_client->i2c_client = client;

	i2c_set_clientdata(client, gate_client);
	_gate_ic_i2c_client = client;

	pr_info("%s-: ", __func__);

	return 0;
}

static void _gate_ic_i2c_remove(struct i2c_client *client)
{
	struct gate_ic_client *gate_client;

	pr_info("%s+\n", __func__);

	gate_client = i2c_get_clientdata(client);

	i2c_unregister_device(client);

	kfree(gate_client);
	gate_client = NULL;
	_gate_ic_i2c_client = NULL;
	i2c_unregister_device(client);
}

/*****************************************************************************
 * Data Structure
 *****************************************************************************/

static const struct i2c_device_id _gate_ic_i2c_id[] = {
	{GATE_I2C_ID_NAME, 0},
	{}
};

static struct i2c_driver _gate_ic_i2c_driver = {
	.id_table = _gate_ic_i2c_id,
	.probe = _gate_ic_i2c_probe,
	.remove = _gate_ic_i2c_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = GATE_I2C_ID_NAME,
		   .of_match_table = _gate_ic_i2c_of_match,
		   },
};

module_i2c_driver(_gate_ic_i2c_driver);

MODULE_AUTHOR("Mediatek Corporation");
MODULE_DESCRIPTION("MTK OCP2131 I2C Driver");
MODULE_LICENSE("GPL");


