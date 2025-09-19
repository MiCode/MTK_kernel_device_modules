/*
 * sp0821.c  sp0821 yuv module
 *
 * Author: Bruce <sunchengwei@longcheer.com>
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
#include <linux/regulator/consumer.h>
#include "sp_sp0821_iii.h"
#include "../imgsensor.h"
#include "../imgsensor_proc.h"
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/consumer.h>

extern void ISP_MCLK3_EN (bool En);

/*****************************************************************
* sp0821 marco
******************************************************************/
#define SP0821_DRIVER_VERSION	"V2.0"
#define SP0821_PRODUCT_NUM		4
#define SP0821_PRODUCT_NAME_LEN	8
#define SP0821_SENSOR_ID   0x9c
#define SP0821_MCLK_ON   "sp0821_mclk_on"
#define SP0821_MCLK_OFF   "sp0821_mclk_off"
struct pinctrl_state *sp0821_mclk_on;
struct pinctrl_state *sp0821_mclk_off;
/*****************************************************************
* sp0821 global global variable
******************************************************************/
static unsigned char read_reg_id = 0;
static unsigned char read_reg_value = 0;
static int read_reg_flag = 0;
static int driver_flag = 0;
struct sp0821 *g_sp0821 = NULL;
//bool qvga_probe = false;
/**********************************************************
* i2c write and read
**********************************************************/
static int sp0821_i2c_write(struct sp0821 *sp0821,
			     unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < SP0821_I2C_RETRIES) {
		ret = i2c_smbus_write_byte_data(sp0821->i2c_client,
						reg_addr, reg_data);
		if (ret < 0) {
			qvga_dev_err(sp0821->dev,
				   "%s: i2c_write cnt=%d error=%d\n", __func__,
				   cnt, ret);
		} else {
			break;
		}
		cnt++;
		msleep(SP0821_I2C_RETRY_DELAY);
	}

	return ret;
}

static int sp0821_i2c_read(struct sp0821 *sp0821,
			    unsigned char reg_addr, unsigned char *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < SP0821_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(sp0821->i2c_client, reg_addr);
		if (ret < 0) {
			qvga_dev_err(sp0821->dev,
				   "%s: i2c_read cnt=%d error=%d\n", __func__,
				   cnt, ret);
		} else {
			*reg_data = ret;
			break;
		}
		cnt++;
		msleep(SP0821_I2C_RETRY_DELAY);
	}

	return ret;
}

static struct sp0821 *sp0821_malloc_init(struct i2c_client *client)
{
	struct sp0821 *sp0821 =
	    devm_kzalloc(&client->dev, sizeof(struct sp0821), GFP_KERNEL);
	if (sp0821 == NULL) {
		dev_err(&client->dev, "%s: devm_kzalloc failed.\n", __func__);
		return NULL;
	}

	sp0821->i2c_client = client;

	pr_info("%s enter , client_addr = 0x%02x\n", __func__,
		sp0821->i2c_client->addr);

	return sp0821;
}
#if 1
void sp0821_Init(struct sp0821 *sp0821)
{
    /*SYS*/
	sp0821_i2c_write(sp0821, 0x30, 0x01);
	sp0821_i2c_write(sp0821, 0x32, 0x00);
	sp0821_i2c_write(sp0821, 0x03, 0x00);
	sp0821_i2c_write(sp0821, 0x04, 0x96);
	sp0821_i2c_write(sp0821, 0x24, 0x13);
	sp0821_i2c_write(sp0821, 0x9b, 0x32);
	sp0821_i2c_write(sp0821, 0xd7, 0x00);
	sp0821_i2c_write(sp0821, 0xc5, 0xc7);
	sp0821_i2c_write(sp0821, 0xc6, 0xe2);
	sp0821_i2c_write(sp0821, 0xe7, 0x03);
	sp0821_i2c_write(sp0821, 0x32, 0x00);
	sp0821_i2c_write(sp0821, 0x32, 0x01);
	sp0821_i2c_write(sp0821, 0x32, 0x00);
	sp0821_i2c_write(sp0821, 0xbf, 0x0f);
	sp0821_i2c_write(sp0821, 0xba, 0x5a);
	sp0821_i2c_write(sp0821, 0xbb, 0x69);
	sp0821_i2c_write(sp0821, 0xe7, 0x00);
	sp0821_i2c_write(sp0821, 0x32, 0x07);
	sp0821_i2c_write(sp0821, 0x31, 0x03);
	sp0821_i2c_write(sp0821, 0x2c, 0x0f);
	sp0821_i2c_write(sp0821, 0x2e, 0x3c);
	sp0821_i2c_write(sp0821, 0x30, 0x01);
	sp0821_i2c_write(sp0821, 0x28, 0x2e);
	sp0821_i2c_write(sp0821, 0x29, 0x1f);
	sp0821_i2c_write(sp0821, 0x0f, 0x30);
	sp0821_i2c_write(sp0821, 0x14, 0xb0);
	sp0821_i2c_write(sp0821, 0x38, 0x50);
	sp0821_i2c_write(sp0821, 0x39, 0x52);
	sp0821_i2c_write(sp0821, 0x3a, 0x60);
	sp0821_i2c_write(sp0821, 0x3b, 0x10);
	sp0821_i2c_write(sp0821, 0x3c, 0xe0);
	sp0821_i2c_write(sp0821, 0x85, 0x01);
	sp0821_i2c_write(sp0821, 0xe0, 0x02);
	sp0821_i2c_write(sp0821, 0xe5, 0x60);
	sp0821_i2c_write(sp0821, 0xf5, 0x02);
	sp0821_i2c_write(sp0821, 0xf1, 0x03);
	sp0821_i2c_write(sp0821, 0xf3, 0x40);
	sp0821_i2c_write(sp0821, 0x41, 0x00);
	sp0821_i2c_write(sp0821, 0x05, 0x00);
	sp0821_i2c_write(sp0821, 0x06, 0x00);
	sp0821_i2c_write(sp0821, 0x07, 0x00);
	sp0821_i2c_write(sp0821, 0x08, 0x00);
	sp0821_i2c_write(sp0821, 0x09, 0x00);
	sp0821_i2c_write(sp0821, 0x0a, 0x34);
	sp0821_i2c_write(sp0821, 0x0D, 0x01);
	sp0821_i2c_write(sp0821, 0xc8, 0x10);
	sp0821_i2c_write(sp0821, 0x29, 0x1e);
	sp0821_i2c_write(sp0821, 0xa2, 0x32);
	sp0821_i2c_write(sp0821, 0xa3, 0x00);
	sp0821_i2c_write(sp0821, 0xa4, 0x32);
	sp0821_i2c_write(sp0821, 0xa5, 0x00);
	sp0821_i2c_write(sp0821, 0xa8, 0x32);
	sp0821_i2c_write(sp0821, 0xa9, 0x00);
	sp0821_i2c_write(sp0821, 0xaa, 0x01);
	sp0821_i2c_write(sp0821, 0xab, 0x00);
	sp0821_i2c_write(sp0821, 0x4c, 0x80);
	sp0821_i2c_write(sp0821, 0x4d, 0x80);
	sp0821_i2c_write(sp0821, 0xa6, 0x30);
	sp0821_i2c_write(sp0821, 0xa7, 0x20);
	sp0821_i2c_write(sp0821, 0xac, 0x30);
	sp0821_i2c_write(sp0821, 0xad, 0x20);
	sp0821_i2c_write(sp0821, 0x8a, 0x3e);
	sp0821_i2c_write(sp0821, 0x8b, 0x30);
	sp0821_i2c_write(sp0821, 0x8c, 0x2a);
	sp0821_i2c_write(sp0821, 0x8d, 0x26);
	sp0821_i2c_write(sp0821, 0x8e, 0x26);
	sp0821_i2c_write(sp0821, 0x8f, 0x24);
	sp0821_i2c_write(sp0821, 0x90, 0x24);
	sp0821_i2c_write(sp0821, 0x91, 0x22);
	sp0821_i2c_write(sp0821, 0x92, 0x22);
	sp0821_i2c_write(sp0821, 0x93, 0x22);
	sp0821_i2c_write(sp0821, 0x94, 0x20);
	sp0821_i2c_write(sp0821, 0x95, 0x20);
	sp0821_i2c_write(sp0821, 0x96, 0x20);
	sp0821_i2c_write(sp0821, 0x17, 0x88);
	sp0821_i2c_write(sp0821, 0x18, 0x80);
	sp0821_i2c_write(sp0821, 0x4e, 0x78);
	sp0821_i2c_write(sp0821, 0x4f, 0x78);
	sp0821_i2c_write(sp0821, 0x58, 0x8a);
	sp0821_i2c_write(sp0821, 0x59, 0xa8);
	sp0821_i2c_write(sp0821, 0x5a, 0x80);
	sp0821_i2c_write(sp0821, 0xca, 0x00);
	sp0821_i2c_write(sp0821, 0x86, 0x08);
	sp0821_i2c_write(sp0821, 0x87, 0x0f);
	sp0821_i2c_write(sp0821, 0x88, 0x30);
	sp0821_i2c_write(sp0821, 0x89, 0x45);
	sp0821_i2c_write(sp0821, 0x9e, 0x94);
	sp0821_i2c_write(sp0821, 0x9f, 0x88);
	sp0821_i2c_write(sp0821, 0x97, 0x84);
	sp0821_i2c_write(sp0821, 0x98, 0x88);
	sp0821_i2c_write(sp0821, 0x99, 0x74);
	sp0821_i2c_write(sp0821, 0x9a, 0x84);
	sp0821_i2c_write(sp0821, 0xa0, 0x7c);
	sp0821_i2c_write(sp0821, 0xa1, 0x78);
	sp0821_i2c_write(sp0821, 0x9d, 0x09);
	sp0821_i2c_write(sp0821, 0xB1, 0x04);
	sp0821_i2c_write(sp0821, 0xb3, 0x00);
	sp0821_i2c_write(sp0821, 0x47, 0x40);
	sp0821_i2c_write(sp0821, 0xb8, 0x04);
	sp0821_i2c_write(sp0821, 0xb9, 0x28);
	sp0821_i2c_write(sp0821, 0x3f, 0x18);
	sp0821_i2c_write(sp0821, 0xc1, 0xff);
	sp0821_i2c_write(sp0821, 0xc2, 0x40);
	sp0821_i2c_write(sp0821, 0xc3, 0xff);
	sp0821_i2c_write(sp0821, 0xc4, 0x40);
	sp0821_i2c_write(sp0821, 0xc5, 0xc7);
	sp0821_i2c_write(sp0821, 0xc6, 0xe2);
	sp0821_i2c_write(sp0821, 0xc7, 0xef);
	sp0821_i2c_write(sp0821, 0xc8, 0x10);
	sp0821_i2c_write(sp0821, 0x50, 0x2a);
	sp0821_i2c_write(sp0821, 0x51, 0x2a);
	sp0821_i2c_write(sp0821, 0x52, 0x2f);
	sp0821_i2c_write(sp0821, 0x53, 0xcf);
	sp0821_i2c_write(sp0821, 0x54, 0xd0);
	sp0821_i2c_write(sp0821, 0x5c, 0x1e);
	sp0821_i2c_write(sp0821, 0x5d, 0x21);
	sp0821_i2c_write(sp0821, 0x5e, 0x1a);
	sp0821_i2c_write(sp0821, 0x5f, 0xe9);
	sp0821_i2c_write(sp0821, 0x60, 0x98);
	sp0821_i2c_write(sp0821, 0xcb, 0x3f);
	sp0821_i2c_write(sp0821, 0xcc, 0x3f);
	sp0821_i2c_write(sp0821, 0xcd, 0x3f);
	sp0821_i2c_write(sp0821, 0xce, 0x85);
	sp0821_i2c_write(sp0821, 0xcf, 0xff);
	sp0821_i2c_write(sp0821, 0x79, 0x5a);
	sp0821_i2c_write(sp0821, 0x7a, 0xDC);
	sp0821_i2c_write(sp0821, 0x7b, 0x0A);
	sp0821_i2c_write(sp0821, 0x7c, 0xFD);
	sp0821_i2c_write(sp0821, 0x7d, 0x46);
	sp0821_i2c_write(sp0821, 0x7e, 0xFD);
	sp0821_i2c_write(sp0821, 0x7f, 0xFD);
	sp0821_i2c_write(sp0821, 0x80, 0xEF);
	sp0821_i2c_write(sp0821, 0x81, 0x54);
	sp0821_i2c_write(sp0821, 0x1b, 0x0a);
	sp0821_i2c_write(sp0821, 0x1c, 0x0f);
	sp0821_i2c_write(sp0821, 0x1d, 0x15);
	sp0821_i2c_write(sp0821, 0x1e, 0x15);
	sp0821_i2c_write(sp0821, 0x1f, 0x15);
	sp0821_i2c_write(sp0821, 0x20, 0x1f);
	sp0821_i2c_write(sp0821, 0x21, 0x2a);
	sp0821_i2c_write(sp0821, 0x22, 0x2a);
	sp0821_i2c_write(sp0821, 0x56, 0x49);
	sp0821_i2c_write(sp0821, 0x1a, 0x14);
	sp0821_i2c_write(sp0821, 0x34, 0x1f);
	sp0821_i2c_write(sp0821, 0x82, 0x10);
	sp0821_i2c_write(sp0821, 0x83, 0x00);
	sp0821_i2c_write(sp0821, 0x84, 0xff);
	sp0821_i2c_write(sp0821, 0xd7, 0x50);
	sp0821_i2c_write(sp0821, 0xd8, 0x1a);
	sp0821_i2c_write(sp0821, 0xd9, 0x20);
	sp0821_i2c_write(sp0821, 0xc9, 0x1f);
	sp0821_i2c_write(sp0821, 0xbf, 0x33);
	sp0821_i2c_write(sp0821, 0xba, 0x37);
	sp0821_i2c_write(sp0821, 0xbb, 0x38);

}   /*    sensor_init  */
#endif

int sp0821_GetSensorID(struct sp0821 *sp0821)
{
	int retry = 5;
	unsigned char reg_data = 0x00;

	//check if sensor ID correct
	do {
		sp0821_i2c_read(sp0821, 0x02, &reg_data);
		qvga_dev_err(sp0821->dev, "drv-%s: Read MSB Sensor ID sucess = 0x%02x\n", __func__, reg_data);
		if (reg_data == SP0821_SENSOR_ID) {
			qvga_dev_err(sp0821->dev, "drv-%s: Read Sensor ID sucess = 0x%02x\n", __func__, reg_data);
			driver_flag = 1;
			return 0;
		} else {
			qvga_dev_err(sp0821->dev, "rv-%s: Read Sensor ID Fail = 0x%02x\n", __func__, reg_data);
			driver_flag = 0;
		}
		mdelay(10);
		retry--;
		pr_err("%s sp0821 get sensorid retry %d time\n", __func__, retry);
	} while (retry > 0);

	return -1;
}

#if 1
static void sp0821_avdd_control(struct sp0821 *sp0821, bool flag)
{
	int ret;
	qvga_dev_info(sp0821->dev, "%s enter\n", __func__);
	if (IS_ERR(sp0821->vcama)) {
		qvga_dev_err(sp0821->dev, "%s AVDD get regulator failed\n", __func__);
		regulator_put(sp0821->vcama);
		return;
	}

	if (flag) {
		regulator_set_voltage(sp0821->vcama, 2800000, 2800000);
		ret = regulator_enable(sp0821->vcama);
	} else {
		ret = regulator_disable(sp0821->vcama);
	}
	if(ret)
		pr_err("%s regulator enable %d failed\n", __func__, flag);
	return;
}
// define IOVDD
static void sp0821_iovdd_control(struct sp0821 *sp0821, bool flag)
{
    int ret;
    qvga_dev_info(sp0821->dev, "%s enter\n", __func__);
    if (IS_ERR(sp0821->vcamio)) {
        qvga_dev_err(sp0821->dev, "%s  IOVDD get regulator failed\n", __func__);
        regulator_put(sp0821->vcamio);
        return;
    }
    if (flag) {
        regulator_set_voltage(sp0821->vcamio, 1800000, 1800000);
        ret = regulator_enable(sp0821->vcamio);
    } else {
        ret = regulator_disable(sp0821->vcamio);
    }
    if(ret)
	pr_err("%s regulator enable %d failed\n", __func__, flag);
    return;
}
#endif
#if 0
static void sp0821_vcam_control(struct sp0821 *sp0821, bool flag)
{
    struct regulator *vcama;
    struct regulator *vcamio;
    struct regulator *vcamd;

    qvga_dev_info(sp0821->dev, "%s enter\n", __func__);

    if(flag)
    {
        vcamd = regulator_get(sp0821->dev,"vcamd");
        if (IS_ERR(vcamd)) {
            qvga_dev_err(sp0821->dev, "%s get regulator vcamd failed\n", __func__);
            regulator_put(vcamd);
            return;
        }
        regulator_set_voltage(vcamd, 1200000, 1200000);
        regulator_enable(vcamd);

        vcamio = regulator_get(sp0821->dev,"vcamio");
        if (IS_ERR(vcamio)) {
            qvga_dev_err(sp0821->dev, "%s get regulator vcamio failed\n", __func__);
            regulator_put(vcamio);
            return;
        }
        regulator_set_voltage(vcamio, 1800000, 1800000);
        regulator_enable(vcamio);

        vcama = regulator_get(sp0821->dev,"vcama");
        if (IS_ERR(vcama)) {
            qvga_dev_err(sp0821->dev, "%s get regulator vcama failed\n", __func__);
            regulator_put(vcama);
            return;
        }
        regulator_set_voltage(vcama, 2800000, 2800000);
        regulator_enable(vcama);
    }
    else
    {
        vcama = regulator_get(sp0821->dev,"vcama");
        if (IS_ERR(vcama)) {
            qvga_dev_err(sp0821->dev, "%s get regulator vcama failed\n", __func__);
            regulator_put(vcama);
            return;
        }
        regulator_disable(vcama);

        vcamio = regulator_get(sp0821->dev,"vcamio");
        if (IS_ERR(vcamio)) {
            qvga_dev_err(sp0821->dev, "%s get regulator vcamio failed\n", __func__);
            regulator_put(vcamio);
            return;
        }
        regulator_disable(vcamio);

        vcamd = regulator_get(sp0821->dev,"vcamd");
        if (IS_ERR(vcamd)) {
            qvga_dev_err(sp0821->dev, "%s get regulator vcamd failed\n", __func__);
            regulator_put(vcamd);
            return;
       }
       regulator_disable(vcamd);
    }
    return;
}
#endif
static void sp0821_hw_on_reset(struct sp0821 *sp0821)
{
	qvga_dev_info(sp0821->dev, "%s enter\n", __func__);

	if (gpio_is_valid(sp0821->reset_gpio)) {
		gpio_set_value_cansleep(sp0821->reset_gpio, 0);
		udelay(50);
		gpio_set_value_cansleep(sp0821->reset_gpio, 1);
		mdelay(1);
		gpio_set_value_cansleep(sp0821->reset_gpio, 0);
                mdelay(1);
	}
}
static void sp0821_hw_on_reset1(struct sp0821 *sp0821)
{
	qvga_dev_info(sp0821->dev, "%s enter\n", __func__);

	if (gpio_is_valid(sp0821->reset_gpio1)) {
		gpio_set_value_cansleep(sp0821->reset_gpio1, 1);
	}
}
static void sp0821_hw_off_reset(struct sp0821 *sp0821)
{
	qvga_dev_info(sp0821->dev, "%s enter\n", __func__);

	if (gpio_is_valid(sp0821->reset_gpio)) {
		gpio_set_value_cansleep(sp0821->reset_gpio, 0);
		udelay(50);
		gpio_set_value_cansleep(sp0821->reset_gpio, 1);
		udelay(50);
		gpio_set_value_cansleep(sp0821->reset_gpio, 0);
	}
}
static void sp0821_hw_off_reset1(struct sp0821 *sp0821)
{
	qvga_dev_info(sp0821->dev, "%s enter\n", __func__);

	if (gpio_is_valid(sp0821->reset_gpio1)) {
		gpio_set_value_cansleep(sp0821->reset_gpio1, 0);
	}
}
static void sp0821_hw_on(struct sp0821 *sp0821)
{
#if 1
	sp0821_iovdd_control(sp0821, true);
	udelay(10);
	sp0821_avdd_control(sp0821, true);
#endif
	udelay(20);
    sp0821_hw_on_reset1(sp0821);
	sp0821_hw_on_reset(sp0821);
	udelay(4000);
	sp0821_Init(sp0821);

	sp0821->hwen_flag = 1;
}
static void sp0821_hw_off(struct sp0821 *sp0821)

{
	sp0821_hw_off_reset(sp0821);

#if 1
	sp0821_avdd_control(sp0821, false);
	sp0821_iovdd_control(sp0821, false);
#endif
	sp0821->hwen_flag = 0;
}

static ssize_t sp0821_get_reg(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	if (read_reg_flag) {
		len += snprintf(buf + len, PAGE_SIZE - len, "The reg 0x%02X value is 0x%02X\n",
				read_reg_id, read_reg_value);
		read_reg_flag = 0;
		read_reg_id = 0;
		read_reg_value = 0;
	} else {
		len += snprintf(buf + len, PAGE_SIZE - len, "Please echo reg id into reg\n");
	}

	return len;
}

static ssize_t sp0821_set_reg(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t len)
{
	unsigned int databuf[2] = { 0 };
	unsigned char reg_data = 0x00;
	//int length;
	//struct sp0821 *sp0821 = dev_get_drvdata(dev);

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		sp0821_i2c_write(g_sp0821, databuf[0], databuf[1]);
	}
	else if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 1) {
		sp0821_i2c_read(g_sp0821, databuf[0], &reg_data);
		read_reg_id = databuf[0];
		read_reg_value = reg_data;
		read_reg_flag = 1;
	}

	return len;
}

static ssize_t sp0821_get_name(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	if (driver_flag) {
		len += snprintf(buf + len, PAGE_SIZE - len, "%s\n",
				"sp_sp0821_iii");
	} else {
		len += snprintf(buf + len, PAGE_SIZE - len, "%s\n",
				"none");
	}

	return len;
}

static ssize_t sp0821_get_light(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	unsigned char reg_data = 0x00;
	// int length;
	//struct sp0821 *sp0821 = dev_get_drvdata(dev);

	sp0821_i2c_read(g_sp0821, 0xb0, &reg_data);
	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n",
			reg_data);

	return len;
}

static ssize_t sp0821_set_light(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t len)
{
	ssize_t ret;
	unsigned int state;
	//struct sp0821 *sp0821 = dev_get_drvdata(dev);

	ret = kstrtouint(buf, 10, &state);
	if (ret) {
		qvga_dev_err(g_sp0821->dev, "%s: fail to change str to int\n",
			   __func__);
		return ret;
	}
	if (state == 0)
		sp0821_hw_off(g_sp0821); /*OFF*/
	else
		sp0821_hw_on(g_sp0821); /*ON*/
	return len;
}

static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO,
		sp0821_get_reg, sp0821_set_reg);
static DEVICE_ATTR(cam_name, S_IWUSR | S_IRUGO,
		sp0821_get_name, NULL);
static DEVICE_ATTR(light, S_IWUSR | S_IRUGO,
		sp0821_get_light, sp0821_set_light);

static struct attribute *sp0821_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_cam_name.attr,
	&dev_attr_light.attr,
	NULL
};

static struct attribute_group sp0821_attribute_group = {
	.attrs = sp0821_attributes
};

static void sp0821_parse_gpio_dt(struct sp0821 *sp0821,
					struct device_node *np)
{
	qvga_dev_info(sp0821->dev, "%s enter, dev_i2c%d@0x%02X\n", __func__,
			sp0821->i2c_seq, sp0821->i2c_addr);

	sp0821->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (sp0821->reset_gpio < 0) {
		qvga_dev_err(sp0821->dev,
			   "%s: no reset gpio provided, hardware reset unavailable\n",
			__func__);
		sp0821->reset_gpio = -1;
	} else {
		qvga_dev_info(sp0821->dev, "%s: reset gpio provided ok\n",
			 __func__);
	}

	sp0821->reset_gpio1 = of_get_named_gpio(np, "reset-gpio1", 0);
	if (sp0821->reset_gpio1 < 0) {
		qvga_dev_err(sp0821->dev,
			   "%s: no reset gpio1 provided, hardware reset unavailable\n",
			__func__);
		sp0821->reset_gpio1 = -1;
	} else {
		qvga_dev_info(sp0821->dev, "%s: reset gpio1 provided ok\n",
			 __func__);
	}
}


static void sp0821_parse_dt(struct sp0821 *sp0821, struct device_node *np)
{
	qvga_dev_info(sp0821->dev, "%s enter, dev_i2c%d@0x%02X\n", __func__,
		    sp0821->i2c_seq, sp0821->i2c_addr);
	sp0821_parse_gpio_dt(sp0821, np);
}

/****************************************************************************
* sp0821 i2c driver
*****************************************************************************/
static int sp0821_i2c_probe(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	//struct pinctrl *sp0821_pinctrl = NULL;
	struct pinctrl_state *set_state = NULL;
	//struct pinctrl_state *sp0821_mclk_on = NULL;
	//struct pinctrl_state *sp0821_mclk_off = NULL;
	struct sp0821 *sp0821 = NULL;
	struct class *qvga_class;
	struct device *dev;
	int ret = -1;

	pr_err("%s enter , %d@0x%02x\n", __func__,client->adapter->nr, client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		qvga_dev_err(&client->dev, "%s: check_functionality failed\n",
			   __func__);
		ret = -ENODEV;
		goto exit_check_functionality_failed;
	}

	sp0821 = sp0821_malloc_init(client);
	g_sp0821 = sp0821;
	sp0821->i2c_seq = sp0821->i2c_client->adapter->nr;
	sp0821->i2c_addr = sp0821->i2c_client->addr;
	if (sp0821 == NULL) {
		dev_err(&client->dev, "%s: failed to parse device tree node\n",
			__func__);
		ret = -ENOMEM;
		goto exit_devm_kzalloc_failed;
	}

	sp0821->dev = &client->dev;
	i2c_set_clientdata(client, sp0821);
	sp0821_parse_dt(sp0821, np);

	if (gpio_is_valid(sp0821->reset_gpio)) {
		ret = devm_gpio_request_one(&client->dev,
					    sp0821->reset_gpio,
					    GPIOF_OUT_INIT_LOW, "sp0821_rst");//sp
		if (ret) {
			qvga_dev_err(&client->dev,
				   "%s: rst request failed\n", __func__);
			goto exit_gpio_request_failed;
		}
	}
	if (gpio_is_valid(sp0821->reset_gpio1)) {
		ret = devm_gpio_request_one(&client->dev,
					    sp0821->reset_gpio1,
					    GPIOF_OUT_INIT_LOW, "sp0821_rst");//sp
		if (ret) {
			qvga_dev_err(&client->dev,
				   "%s: rst request failed\n", __func__);
			goto exit_gpio_request_failed;
		}
	}

        sp0821->sp0821_pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(sp0821->sp0821_pinctrl)) {
		qvga_dev_err(&client->dev, "%s: sp0821_pinctrl not defined\n", __func__);
	} else {
		set_state = pinctrl_lookup_state(sp0821->sp0821_pinctrl, SP0821_MCLK_ON);
		if (IS_ERR_OR_NULL(set_state)) {
			qvga_dev_err(&client->dev, "%s: sp0821_pinctrl lookup failed for mclk on\n", __func__);
		} else {
			sp0821_mclk_on = set_state;
		}
		set_state = pinctrl_lookup_state(sp0821->sp0821_pinctrl, SP0821_MCLK_OFF);
		if (IS_ERR_OR_NULL(set_state)) {
			qvga_dev_err(&client->dev, "%s: sp0821_pinctrl lookup failed for mclk off\n", __func__);
		} else {
			sp0821_mclk_off = set_state;
		}
		ret = pinctrl_select_state(sp0821->sp0821_pinctrl, sp0821_mclk_off);
		if (ret < 0) {
			qvga_dev_err(&client->dev, "%s: sp0821_pinctrl select failed for mclk off\n", __func__);
		}
	}

	//power on camera
        sp0821_hw_off_reset1(sp0821);
        mdelay(5);
#if 1
	sp0821->vcamio = regulator_get(sp0821->dev,"vcamio");
	sp0821_iovdd_control(sp0821, true);
	udelay(10);
	sp0821->vcama = regulator_get(sp0821->dev,"vcama");
	sp0821_avdd_control(sp0821, true);
#else
	sp0821_vcam_control(sp0821, true);
#endif
	mdelay(1);
	ret = pinctrl_select_state(sp0821->sp0821_pinctrl, sp0821_mclk_on);
	if (ret < 0) {
		qvga_dev_err(&client->dev, "%s: sp0821_pinctrl select failed for mclk on\n", __func__);
	}
	mdelay(5);
        sp0821_hw_on_reset1(sp0821);
	sp0821_hw_on_reset(sp0821);

//	sp0821->hwen_flag = 1;

	/* sp0821 sensor id */
	ret = sp0821_GetSensorID(sp0821);
	if (ret < 0) {
		qvga_dev_err(&client->dev,
			   "%s: sp0821read_sensorid failed ret=%d\n", __func__,
			   ret);
		goto exit_i2c_check_id_failed;
	}

        //power off camera
        sp0821_hw_off_reset1(sp0821);
#if 1
	sp0821_avdd_control(sp0821, false);
	sp0821_iovdd_control(sp0821, false);
#else
	sp0821_vcam_control(sp0821, false);
#endif
//	sp0821_Init(sp0821);

	//qvga_class = class_create(THIS_MODULE, "qvga_cam");
	qvga_class = class_create("qvga_cam");
	dev = device_create(qvga_class, NULL, client->dev.devt, NULL, "qvga_depth");

	ret = sysfs_create_group(&dev->kobj, &sp0821_attribute_group);
	if (ret < 0) {
		qvga_dev_err(&client->dev,
			    "%s failed to create sysfs nodes\n", __func__);
	}

	return 0;
 exit_i2c_check_id_failed:
        sp0821_hw_off_reset1(sp0821);
#if 1
	sp0821_avdd_control(sp0821, false);
	sp0821_iovdd_control(sp0821, false);
#else
	sp0821_vcam_control(sp0821, false);
#endif
    if (gpio_is_valid(sp0821->reset_gpio))
		gpio_free(sp0821->reset_gpio);
 exit_gpio_request_failed:
	devm_kfree(&client->dev, sp0821);
	sp0821 = NULL;
 exit_devm_kzalloc_failed:
 exit_check_functionality_failed:
	return ret;
}

static void sp0821_i2c_remove(struct i2c_client *client)
{
	struct sp0821 *sp0821 = i2c_get_clientdata(client);
	if (gpio_is_valid(sp0821->reset_gpio))
		gpio_free(sp0821->reset_gpio);
	if (gpio_is_valid(sp0821->reset_gpio1))
		gpio_free(sp0821->reset_gpio1);
	devm_kfree(&client->dev, sp0821);
	sp0821 = NULL;
}

static const struct of_device_id sp0821_of_match[] = {
	{.compatible = "sp,sp0821_yuv"},
	{},
};

static struct i2c_driver sp0821_i2c_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name =  "sp0821_yuv",
		   .of_match_table = sp0821_of_match,
		   },
	.probe = sp0821_i2c_probe,
	.remove = sp0821_i2c_remove,
};

static int __init sp0821_yuv_init(void)
{
	int ret;

	pr_info("%s: driver version: %s\n", __func__,
				SP0821_DRIVER_VERSION);

	ret = i2c_add_driver(&sp0821_i2c_driver);
	if (ret) {
		pr_info("****[%s] Unable to register driver (%d)\n",
			__func__, ret);
		return ret;
	}
	return 0;
}

static void __exit sp0821_yuv_exit(void)
{
	pr_info("%s enter\n", __func__);
	i2c_del_driver(&sp0821_i2c_driver);
}

module_init(sp0821_yuv_init);
module_exit(sp0821_yuv_exit);

MODULE_AUTHOR("wangyong4@longcheer.com>");
MODULE_DESCRIPTION("sp0821 yuv driver");
MODULE_LICENSE("GPL v2");