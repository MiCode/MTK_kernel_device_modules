// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>

#define I2C_ID_NAME "ocp2131"

static const struct of_device_id i2c_lcm_of_match[] = {
		{.compatible = "mediatek,i2c-lcd-bias"},
		{},
};

struct i2c_client *ocp2131_i2c_client;


static int ocp2131_probe(struct i2c_client *client);
static void ocp2131_remove(struct i2c_client *client);

struct ocp2131_dev {
	struct i2c_client *client;

};

static const struct i2c_device_id ocp2131_id[] = {
	{I2C_ID_NAME, 0},
	{}
};

static struct i2c_driver ocp2131_iic_driver = {
	.id_table = ocp2131_id,
	.probe = ocp2131_probe,
	.remove = ocp2131_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = I2C_ID_NAME,
			.of_match_table = i2c_lcm_of_match,
		   },
};

static DEFINE_MUTEX(ocp2131_i2c_access);

static int ocp2131_probe(struct i2c_client *client)
{
	pr_info("i2c: name=%s addr=0x%x\n", client->name, client->addr);
	ocp2131_i2c_client = client;
	return 0;
}

static void ocp2131_remove(struct i2c_client *client)
{
	ocp2131_i2c_client = NULL;
	i2c_unregister_device(client);
}

int ocp2131_i2c_read_bytes(unsigned char addr, unsigned char *returnData)
{
	char cmd_buf[2] = { 0x00, 0x00 };
	int ret = 0;
	struct i2c_client *client = ocp2131_i2c_client;

	if (client == NULL) {
		pr_info("%s: ERROR!! client is null\n", __func__);
		return 0;
	}

	mutex_lock(&ocp2131_i2c_access);

	cmd_buf[0] = addr;
	ret = i2c_master_send(client, &cmd_buf[0], 1);
	ret = i2c_master_recv(client, &cmd_buf[1], 1);
	if (ret < 0) {
		mutex_unlock(&ocp2131_i2c_access);
		pr_info("%s: ERROR read 0x%x fail %d\n", __func__, addr, ret);
		return ret;
	}

	*returnData = cmd_buf[1];
	mutex_unlock(&ocp2131_i2c_access);

	return ret;
}
EXPORT_SYMBOL(ocp2131_i2c_read_bytes);

int ocp2131_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = ocp2131_i2c_client;
	char write_data[2] = { 0 };

	if (client == NULL) {
		pr_info("%s: ERROR!! client is null\n", __func__);
		return 0;
	}

	mutex_lock(&ocp2131_i2c_access);

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0) {
		mutex_unlock(&ocp2131_i2c_access);
		pr_info("%s: ERROR write 0x%x with 0x%x fail %d\n",
				__func__, addr, value, ret);
		return ret;
	}
	mutex_unlock(&ocp2131_i2c_access);

	return ret;
}
EXPORT_SYMBOL(ocp2131_i2c_write_bytes);

static int __init ocp2131_iic_init(void)
{
	i2c_add_driver(&ocp2131_iic_driver);
	pr_info("%s success\n", __func__);
	return 0;
}

static void __exit ocp2131_iic_exit(void)
{
	i2c_del_driver(&ocp2131_iic_driver);
}


module_init(ocp2131_iic_init);
module_exit(ocp2131_iic_exit);

MODULE_AUTHOR("Hongjiang He <hongjiang.he@mediatek.com>");
MODULE_DESCRIPTION("MTK ocp2131 I2C Driver");
MODULE_LICENSE("GPL");
