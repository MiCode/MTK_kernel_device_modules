/*
 * WL28681C2 ON Semiconductor LDO PMIC Driver.
 *
 * Copyright (c) 2020 On XiaoMi.
 * liuqinhong@xiaomi.com
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/cdev.h>
#include <linux/fs.h>

#include <linux/module.h>
#include <linux/param.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/of_device.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/of_gpio.h>


#define wl28681c2_err(message, ...) \
        pr_err("[%s] : "message, __func__, ##__VA_ARGS__)
#define wl28681c2_debug(message, ...) \
        pr_err("[%s] : "message, __func__, ##__VA_ARGS__)

#define WL28681C2_SLAVE_ADDR      0x6A

#define WL28681C2_REG_ENABLE      0x03
#define WL28681C2_REG_DISCHARGE   0x10
#define WL28681C2_REG_RESET       0X11
#define WL28681C2_REG_LDO0        0x04

#define LDO_VSET_REG(offset) ((offset) + WL28681C2_REG_LDO0)

#define MAX_REG_NAME     20
#define WL28681C2_MAX_LDO  7

#define WL28681C2_CLASS_NAME       "camera_ldo2"
#define WL28681C2_NAME_FMT         "wl28681c2%u"
#define WL28681C2_NAME_STR_LEN_MAX 60
#define WL28681C2_MAX_NUMBER       5
#define MV_PER_V               1000

#define volt2regval(wldo_volt_addr, volt)                  \
		(wldo_volt_addr < 2 ?                            \
		(volt - 496 * MV_PER_V) / (8 * MV_PER_V) + 61      \
		: (volt - 1372 * MV_PER_V) / (8 * MV_PER_V))       \

#define regval2volt(wldo_volt_addr, regval)                \
		(wldo_volt_addr < 2 ?                            \
		496 * MV_PER_V + 8 * MV_PER_V * (regval - 61)      \
		: 1372 * MV_PER_V + 8 * MV_PER_V * regval)         \

struct wl28681c2_char_dev {
	dev_t dev_no;
	struct cdev *pcdev;
	struct device *pdevice;
};

static struct class *pwl28681c2_class = NULL;
static struct wl28681c2_char_dev wl28681c2_dev_list[WL28681C2_MAX_NUMBER];

struct wl28681c2_regulator{
	struct device    *dev;
	struct regmap    *regmap;
	struct regulator_desc rdesc;
	struct regulator_dev  *rdev;
	struct regulator      *parent_supply;
	struct regulator      *en_supply;
	struct device_node    *of_node;
	u16         offset;
	int         min_dropout_uv;
	int         iout_ua;
	int         index;
};

struct regulator_data {
	char *name;
	char *supply_name;
	int   default_uv;
	int   min_dropout_uv;
	int   iout_ua;
};

static struct regulator_data reg_data[] = {
	{ "wl28681c2-l1", "wl28681c2-l1", 1200000, 80000, 800000},
	{ "wl28681c2-l2", "wl28681c2-l2", 1200000, 80000, 800000},
	{ "wl28681c2-l3", "wl28681c2-l3", 2800000, 80000, 800000},
	{ "wl28681c2-l4", "wl28681c2-l4", 2800000, 80000, 800000},
	{ "wl28681c2-l5", "wl28681c2-l5", 2800000, 80000, 800000},
	{ "wl28681c2-l6", "wl28681c2-l6", 2800000, 80000, 800000},
	{ "wl28681c2-l7", "wl28681c2-l7", 2800000, 80000, 800000},
};

static const struct regmap_config wl28681c2_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

/*common functions*/
static int wl28681c2_read(struct regmap *regmap, u16 reg, u8 *val, int count)
{
	int rc;

	rc = regmap_bulk_read(regmap, reg, val, count);
	if (rc < 0)
		wl28681c2_err("failed to read 0x%04x\n", reg);
	return rc;
}

static int wl28681c2_write(struct regmap *regmap, u16 reg, u8*val, int count)
{
	int rc;

	wl28681c2_debug("Writing 0x%02x to 0x%02x\n", *val, reg);
	rc = regmap_bulk_write(regmap, reg, val, count);
	if (rc < 0)
		wl28681c2_err("failed to write 0x%04x\n", reg);

	return rc;
}

static int wl28681c2_masked_write(struct regmap *regmap, u16 reg, u8 mask, u8 val)
{
	int rc;
	wl28681c2_debug("Writing 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);
	rc = regmap_update_bits(regmap, reg, mask, val);
	if (rc < 0)
		wl28681c2_err("failed to write 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);
	return rc;
}

static int wl28681c2_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct wl28681c2_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;
	u8 reg;

	rc = wl28681c2_read(fan_reg->regmap,
		WL28681C2_REG_ENABLE, &reg, 1);
	if (rc < 0) {
		wl28681c2_err("[%s] failed to read enable reg rc = %d\n", fan_reg->rdesc.name, rc);
		return rc;
	}
	return !!(reg & (1u << fan_reg->offset));
}

static int wl28681c2_regulator_enable(struct regulator_dev *rdev)
{
	struct wl28681c2_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	if (fan_reg->parent_supply) {
		rc = regulator_enable(fan_reg->parent_supply);
		if (rc < 0) {
			wl28681c2_err("[%s] failed to enable parent rc=%d\n", fan_reg->rdesc.name, rc);
			return rc;
		}
	}
	wl28681c2_err("fan_reg->offset is(begin)  == %d\n" ,fan_reg->offset);
	rc = wl28681c2_masked_write(fan_reg->regmap,
		WL28681C2_REG_ENABLE,
		1u << fan_reg->offset, 1u << fan_reg->offset);
	wl28681c2_err("fan_reg->offset is(end)  == %d\n" ,fan_reg->offset);
	if (rc < 0) {
		wl28681c2_err("[%s] failed to enable regulator rc=%d\n", fan_reg->rdesc.name, rc);
		goto remove_vote;
	}
	wl28681c2_debug("[%s][%d] regulator enable\n", fan_reg->rdesc.name, fan_reg->index);
	return 0;

remove_vote:
	// if (fan_reg->parent_supply)
	// 	rc = regulator_disable(fan_reg->parent_supply);
	// if (rc < 0)
	// 	wl28681c2_err("[%s] failed to disable parent regulator rc=%d\n", fan_reg->rdesc.name, rc);
	return -ETIME;
}

static int wl28681c2_regulator_disable(struct regulator_dev *rdev)
{
	struct wl28681c2_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	rc = wl28681c2_masked_write(fan_reg->regmap,
		WL28681C2_REG_ENABLE,
		1u << fan_reg->offset, 0);

	if (rc < 0) {
		wl28681c2_err("[%s] failed to disable regulator rc=%d\n", fan_reg->rdesc.name,rc);
		return rc;
	}

	/*remove voltage vot from parent regulator */
	// if (fan_reg->parent_supply) {
	// 	rc = regulator_set_voltage(fan_reg->parent_supply, 0, INT_MAX);
	// 	if (rc < 0) {
	// 		wl28681c2_err("[%s] failed to remove parent voltage rc=%d\n", fan_reg->rdesc.name,rc);
	// 		return rc;
	// 	}
	// 	rc = regulator_disable(fan_reg->parent_supply);
	// 	if (rc < 0) {
	// 		wl28681c2_err("[%s] failed to disable parent rc=%d\n", fan_reg->rdesc.name, rc);
	// 		return rc;
	// 	}
	// }

	wl28681c2_debug("[%s][%d] regulator disabled\n", fan_reg->rdesc.name, fan_reg->index);
	return 0;
}

static int wl28681c2_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct wl28681c2_regulator *fan_reg = rdev_get_drvdata(rdev);
	u8  vset = 0;
	int rc   = 0;
	int uv   = 0;

	rc = wl28681c2_read(fan_reg->regmap, LDO_VSET_REG(fan_reg->offset),
	&vset, 1);
	if (rc < 0) {
		wl28681c2_err("[%s] failed to read regulator voltage rc = %d\n", fan_reg->rdesc.name,rc);
		return rc;
	}

	if (vset == 0) {
		uv = reg_data[fan_reg->offset].default_uv;
	} else {
		wl28681c2_debug("[%s][%d] voltage read [%x]\n", fan_reg->rdesc.name, fan_reg->index, vset);

		uv = regval2volt(fan_reg->offset, vset);
	}
	return uv;
}

static int wl28681c2_write_voltage(struct wl28681c2_regulator* fan_reg, int min_uv,
	int max_uv)
{
	int rc   = 0;
	u8  vset = 0;

	wl28681c2_err("write_min_uv ==  %d, write_max_uv == %d\n", min_uv, max_uv);

	if (min_uv > max_uv) {
		wl28681c2_err("[%s] requestd voltage above maximum limit\n", fan_reg->rdesc.name);
		return -EINVAL;
	}

	vset = volt2regval(fan_reg->offset, min_uv);

	wl28681c2_err("fan_reg->offset ==  %d, min_uv == %d, VSET=[0x%2x]\n", fan_reg->offset, min_uv, vset);

	rc = wl28681c2_write(fan_reg->regmap, LDO_VSET_REG(fan_reg->offset),
		&vset, 1);
	if (rc < 0) {
		wl28681c2_err("[%s] failed to write voltage rc = %d\n", fan_reg->rdesc.name,rc);
		return rc;
	}

	wl28681c2_debug("[%s][%d] VSET=[0x%2x]\n", fan_reg->rdesc.name, fan_reg->index, vset);
	return 0;
}

static int wl28681c2_regulator_set_voltage(struct regulator_dev *rdev,
	int min_uv, int max_uv, unsigned int* selector)
{
	struct wl28681c2_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	// if (fan_reg->parent_supply) {
	// 	rc = regulator_set_voltage(fan_reg->parent_supply,
	// 		fan_reg->min_dropout_uv + min_uv,
	// 		INT_MAX);
	// 	if (rc < 0) {
	// 		wl28681c2_err("[%s] failed to request parent supply voltage rc=%d\n", fan_reg->rdesc.name,rc);
	// 		return rc;
	// 	}
	// }

	rc = wl28681c2_write_voltage(fan_reg, min_uv, max_uv);
	// if (rc < 0) {
	// 	/* remove parentn's voltage vote */
	// 	if (fan_reg->parent_supply)
	// 		regulator_set_voltage(fan_reg->parent_supply, 0, INT_MAX);
	// }
	wl28681c2_debug("[%s][%d] voltage set to %d\n", fan_reg->rdesc.name, fan_reg->index, min_uv);
	return rc;
}

static struct regulator_ops wl28681c2_regulator_ops = {
	.enable      = wl28681c2_regulator_enable,
	.disable     = wl28681c2_regulator_disable,
	.is_enabled  = wl28681c2_regulator_is_enabled,
	.set_voltage = wl28681c2_regulator_set_voltage,
	.get_voltage = wl28681c2_regulator_get_voltage,
};

static int wl28681c2_register_ldo(struct wl28681c2_regulator *wl28681c2_reg,
	const char *name)
{
	struct regulator_config reg_config = {};
	struct regulator_init_data *init_data;

	struct device_node *reg_node = wl28681c2_reg->of_node;
	struct device *dev           = wl28681c2_reg->dev;
	int rc, i, init_voltage;
	char buff[MAX_REG_NAME];

	/* try to find ldo pre-defined in the regulator table */
	for (i = 0; i< WL28681C2_MAX_LDO; i++) {
		if (!strcmp(reg_data[i].name, name))
			break;
	}

	if (i == WL28681C2_MAX_LDO) {
		wl28681c2_err("Invalid regulator name %s\n", name);
		return -EINVAL;
	}

	rc = of_property_read_u16(reg_node, "offset", &wl28681c2_reg->offset);
	if (rc < 0) {
		wl28681c2_err("%s:failed to get regulator offset rc = %d\n", name, rc);
		return rc;
	}

	//assign default value defined in code.
	wl28681c2_reg->min_dropout_uv = reg_data[i].min_dropout_uv;
	of_property_read_u32(reg_node, "min-dropout-voltage",
		&wl28681c2_reg->min_dropout_uv);

	wl28681c2_reg->iout_ua = reg_data[i].iout_ua;
	of_property_read_u32(reg_node, "iout_ua",
		&wl28681c2_reg->iout_ua);

	init_voltage = -EINVAL;
	of_property_read_u32(reg_node, "init-voltage", &init_voltage);

	scnprintf(buff, MAX_REG_NAME, "%s-supply", reg_data[i].supply_name);
	if (of_find_property(dev->of_node, buff, NULL)) {
		wl28681c2_reg->parent_supply = devm_regulator_get(dev,
			reg_data[i].supply_name);
		if (IS_ERR(wl28681c2_reg->parent_supply)) {
			rc = PTR_ERR(wl28681c2_reg->parent_supply);
			if (rc != EPROBE_DEFER)
				wl28681c2_err("%s: failed to get parent regulator rc = %d\n",
					name, rc);
				return rc;
		}
	}

	init_data = of_get_regulator_init_data(dev, reg_node, &wl28681c2_reg->rdesc);
	if (init_data == NULL) {
		wl28681c2_err("%s: failed to get regulator data\n", name);
		return -ENODATA;
	}


	if (!init_data->constraints.name) {
		wl28681c2_err("%s: regulator name missing\n", name);
		return -EINVAL;
	}

	/* configure the initial voltage for the regulator */
	if (init_voltage > 0) {
		rc = wl28681c2_write_voltage(wl28681c2_reg, init_voltage,
			init_data->constraints.max_uV);
		if (rc < 0)
			wl28681c2_err("%s:failed to set initial voltage rc = %d\n", name, rc);
	}

	init_data->constraints.input_uV = init_data->constraints.max_uV;
	init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_STATUS
		| REGULATOR_CHANGE_VOLTAGE;

	reg_config.dev         = dev;
	reg_config.init_data   = init_data;
	reg_config.driver_data = wl28681c2_reg;
	reg_config.of_node     = reg_node;

	wl28681c2_reg->rdesc.owner      = THIS_MODULE;
	wl28681c2_reg->rdesc.type       = REGULATOR_VOLTAGE;
	wl28681c2_reg->rdesc.ops        = &wl28681c2_regulator_ops;
	wl28681c2_reg->rdesc.name       = init_data->constraints.name;
	wl28681c2_reg->rdesc.n_voltages = 1;

	wl28681c2_debug("try to register ldo %s\n", name);

	wl28681c2_reg->rdev = devm_regulator_register(dev, &wl28681c2_reg->rdesc,
		&reg_config);
	if (IS_ERR(wl28681c2_reg->rdev)) {
		rc = PTR_ERR(wl28681c2_reg->rdev);
		wl28681c2_err("%s: failed to register regulator rc =%d\n",
		wl28681c2_reg->rdesc.name, rc);
		return rc;
	}

	wl28681c2_debug("%s regulator register done\n", name);
	return 0;
}

static int wl28681c2_parse_regulator(struct regmap *regmap, struct device *dev)
{
	int rc           = 0;
	int index        = 0;
	const char *name = NULL;
	struct device_node *child             = NULL;
	struct wl28681c2_regulator *wl28681c2_reg = NULL;

	of_property_read_u32(dev->of_node, "index", &index);

	/* parse each regulator */
	for_each_available_child_of_node(dev->of_node, child) {
		wl28681c2_reg = devm_kzalloc(dev, sizeof(*wl28681c2_reg), GFP_KERNEL);
		if (!wl28681c2_reg)
			return -ENOMEM;

		wl28681c2_reg->regmap  = regmap;
		wl28681c2_reg->of_node = child;
		wl28681c2_reg->dev     = dev;
		wl28681c2_reg->index   = index;
		wl28681c2_reg->parent_supply = NULL;

		rc = of_property_read_string(child, "regulator-name", &name);
		if (rc)
			continue;

		rc = wl28681c2_register_ldo(wl28681c2_reg, name);
		if (rc <0 ) {
			wl28681c2_err("failed to register regulator %s rc = %d\n", name, rc);
			return rc;
		}
	}
	return 0;
}

static int wl28681c2_open(struct inode *a_inode, struct file *a_file)
{
	return 0;
}

static int wl28681c2_release(struct inode *a_inode, struct file *a_file)
{
	return 0;
}

static long wl28681c2_ioctl(struct file *a_file, unsigned int a_cmd,
			 unsigned long a_param)
{
	return 0;
}

static const struct file_operations wl28681c2_file_operations = {
	.owner          = THIS_MODULE,
	.open           = wl28681c2_open,
	.release        = wl28681c2_release,
	.unlocked_ioctl = wl28681c2_ioctl,
};

static ssize_t wl28681c2_show_status(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct regmap *regmap = (struct regmap *)dev->driver_data;
	int rc;
	unsigned int val      = 0xFF;
	unsigned int len      = 0;

	rc = regmap_read(regmap, 0x00, &val);
	if (rc < 0) {
		wl28681c2_err("WL28681C2 failed to get PID\n");
		len = sprintf(buf, "fail\n");
	}
	else {
		wl28681c2_debug("WL28681C2 get Product ID: [%02x]\n", val);
		len = sprintf(buf, "success\n");
	}

	return len;
}

static ssize_t wl28681c2_show_info(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct regmap *regmap = (struct regmap *)dev->driver_data;
	int rc;
	unsigned int val      = 0xFF;
	unsigned int len      = 0;
	int i = 0;

	for (i = 0; i <= 0x24; i++) {
		rc = regmap_read(regmap, i, &val);
		if (rc < 0) {
			len += sprintf(buf+len, "read 0x%x ==> fail\n", i);
		}
		else {
			len += sprintf(buf+len, "read 0x%x ==> 0x%x\n",i, val);
		}
	}

	return len;
}

static ssize_t wl28681c2_show_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct regmap *regmap = (struct regmap *)dev->driver_data;
	int rc;
	unsigned int val      = 0xFF;
	unsigned int len      = 0;

	rc = regmap_read(regmap, WL28681C2_REG_ENABLE, &val);
	if (rc < 0) {
		len = sprintf(buf, "read 0x03 ==> fail\n");
	}
	else {
		len = sprintf(buf, "read 0x03 ==> 0x%x\n", val);
	}

	return len;
}

static ssize_t wl28681c2_set_enable(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t len)
{
	u8 val = 0;
	struct regmap *regmap = (struct regmap *)dev->driver_data;

	if (buf[0] == '0' && buf[1] == 'x') {
		val = (u8)simple_strtoul(buf, NULL, 16);
	} else {
		val = (u8)simple_strtoul(buf, NULL, 10);
	}
	wl28681c2_write(regmap, WL28681C2_REG_ENABLE, &val, 1);

	return len;
}

static DEVICE_ATTR(status, S_IWUSR|S_IRUSR, wl28681c2_show_status, NULL);
static DEVICE_ATTR(info, S_IWUSR|S_IRUSR, wl28681c2_show_info, NULL);
static DEVICE_ATTR(enable, S_IWUSR|S_IRUSR, wl28681c2_show_enable, wl28681c2_set_enable);

static int wl28681c2_driver_register(int index, struct regmap *regmap)
{
	unsigned long ret;
	char device_drv_name[WL28681C2_NAME_STR_LEN_MAX] = { 0 };
	struct wl28681c2_char_dev wl28681c2_dev = wl28681c2_dev_list[index];

	snprintf(device_drv_name, WL28681C2_NAME_STR_LEN_MAX - 1,
		WL28681C2_NAME_FMT, index);

	/* Register char driver */
	if (alloc_chrdev_region(&(wl28681c2_dev.dev_no), 0, 1,
			device_drv_name)) {
		wl28681c2_debug("[WL28681C2] Allocate device no failed\n");
		return -EAGAIN;
	}

	/* Allocate driver */
	wl28681c2_dev.pcdev = cdev_alloc();
	if (wl28681c2_dev.pcdev == NULL) {
		unregister_chrdev_region(wl28681c2_dev.dev_no, 1);
		wl28681c2_debug("[WL28681C2] Allocate mem for kobject failed\n");
		return -ENOMEM;
	}

	/* Attatch file operation. */
	cdev_init(wl28681c2_dev.pcdev, &wl28681c2_file_operations);
	wl28681c2_dev.pcdev->owner = THIS_MODULE;

	/* Add to system */
	if (cdev_add(wl28681c2_dev.pcdev, wl28681c2_dev.dev_no, 1)) {
		wl28681c2_debug("Attatch file operation failed\n");
		unregister_chrdev_region(wl28681c2_dev.dev_no, 1);
		return -EAGAIN;
	}

	if (pwl28681c2_class == NULL) {
		pwl28681c2_class = class_create(WL28681C2_CLASS_NAME);
		if (IS_ERR(pwl28681c2_class)) {
			int ret = PTR_ERR(pwl28681c2_class);
			wl28681c2_debug("Unable to create class, err = %d\n", ret);
			return ret;
		}
	}

	wl28681c2_dev.pdevice = device_create(pwl28681c2_class, NULL,
			wl28681c2_dev.dev_no, NULL, device_drv_name);
	if (wl28681c2_dev.pdevice == NULL) {
		wl28681c2_debug("[WL28681C2] Allocate device_create for kobject failed\n");
		return -ENOMEM;
	}

	wl28681c2_dev.pdevice->driver_data = regmap;
	ret = sysfs_create_file(&(wl28681c2_dev.pdevice->kobj), &dev_attr_status.attr);
	ret = sysfs_create_file(&(wl28681c2_dev.pdevice->kobj), &dev_attr_info.attr);
	ret = sysfs_create_file(&(wl28681c2_dev.pdevice->kobj), &dev_attr_enable.attr);

	return 0;
}
static int wl28681c2_regulator_probe(struct i2c_client *client)
{
	int rc                = 0;
	unsigned int val      = 0xFF;
	struct regmap *regmap = NULL;
	int index = 0;
	u8 reg_reset = 7;
	int reset_gpio = 0;

	client->addr =  (WL28681C2_SLAVE_ADDR >> 1);

	rc = of_property_read_u32(client->dev.of_node, "index", &index);
	if (rc) {
		wl28681c2_err("failed to read index");
		return rc;
	}

	reset_gpio = of_get_named_gpio(client->dev.of_node, "wl28681c,reset-gpio", 0);
	if (reset_gpio < 0) {
		wl28681c2_err("invalid reset-gpio in dt: %d", reset_gpio);
		return -EINVAL;
	}

	rc = devm_gpio_request_one(&client->dev,reset_gpio,GPIOF_OUT_INIT_HIGH, "reset_gpio");
	if (rc < 0) {
		wl28681c2_err("Failed to request reset gpio, r:%d", rc);
		return rc;
	}

	regmap = devm_regmap_init_i2c(client, &wl28681c2_regmap_config);
	if (IS_ERR(regmap)) {
		wl28681c2_err("WL28681C2 failed to allocate regmap\n");
		return PTR_ERR(regmap);
	}

	wl28681c2_driver_register(index, regmap);

	rc = regmap_read(regmap, 0x00, &val);
	if (rc < 0) {
		wl28681c2_err("WL28681C2 failed to get PID\n");
		return -ENODEV;
	}
	else
		wl28681c2_debug("WL28681C2 get Product ID: [%02x]\n", val);

	//close Under Voltage Protection(UVP) function
	rc = wl28681c2_write(regmap, WL28681C2_REG_RESET, &reg_reset, 1);
	if (rc < 0) {
		wl28681c2_err("[%x] failed to write reset reg rc = %d\n", WL28681C2_REG_RESET, rc);
		return rc;
	}

	rc = wl28681c2_parse_regulator(regmap, &client->dev);
	if (rc < 0) {
		wl28681c2_err("WL28681C2 failed to parse device tree rc=%d\n", rc);
		return -ENODEV;
	}

	return 0;
}

static const struct of_device_id wl28681c2_dt_ids[] = {
	{
		.compatible = "onsemi,fan53870",
	},
	{
		.compatible = "willsemi,wl28681c2",
	},
	{ }
};
MODULE_DEVICE_TABLE(of, wl28681c2_dt_ids);

static const struct i2c_device_id wl28681c2_id[] = {
	{
		.name = "wl28681c2-regulator",
		.driver_data = 0,
	},
	{ },
};
MODULE_DEVICE_TABLE(i2c, wl28681c2_id);

static struct i2c_driver wl28681c2_regulator_driver = {
	.driver = {
		.name = "wl28681c2-regulator",
		.of_match_table = of_match_ptr(wl28681c2_dt_ids),
	},
	.probe = wl28681c2_regulator_probe,
	.id_table = wl28681c2_id,
};

module_i2c_driver(wl28681c2_regulator_driver);
MODULE_LICENSE("GPL v2");

