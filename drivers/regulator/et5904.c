/*
 * ET5904 ON Semiconductor LDO PMIC Driver.
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
#include <linux/gpio.h>

#define et5904_err(message, ...) \
        pr_err("[%s] : "message, __func__, ##__VA_ARGS__)
#define et5904_debug(message, ...) \
        pr_err("[%s] : "message, __func__, ##__VA_ARGS__)

#define ET5904_SLAVE_ADDR      0x50

#define ET5904_REG_ENABLE      0x0E
#define ET5904_REG_DISCHARGE   0x02
#define ET5904_REG_LDO0        0x03

#define LDO_VSET_REG(offset) ((offset) + ET5904_REG_LDO0)

#define VSET_DVDD_BASE_UV   600000
#define VSET_DVDD_STEP_UV   6000

#define VSET_AVDD_BASE_UV   1200000
#define VSET_AVDD_STEP_UV   12500

#define MAX_REG_NAME     20
#define ET5904_MAX_LDO  4

#define ET5904_CLASS_NAME       "camera_ldo"
#define ET5904_NAME_FMT         "et5904%u"
#define ET5904_NAME_STR_LEN_MAX 50
#define ET5904_MAX_NUMBER       5

struct et5904_char_dev {
	dev_t dev_no;
	struct cdev *pcdev;
	struct device *pdevice;
};

static struct class *pet5904_class = NULL;
static struct et5904_char_dev et5904_dev_list[ET5904_MAX_NUMBER];

struct et5904_regulator{
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
	{ "et5904-l1", "none", 1200000, 105000, 800000},
	{ "et5904-l2", "none", 1200000, 105000, 800000},
	{ "et5904-l3", "none", 2800000, 90000, 300000},
	{ "et5904-l4", "none", 2800000, 90000, 300000},
};

static const struct regmap_config et5904_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};
static int hwLevel = 3;
static int et5904_i2c_write(struct i2c_client *i2c_client, u8 reg, u8 val)
{
	int ret;
	u8 buf[2];
	struct i2c_msg msg;
	if (i2c_client == NULL)
		return -ENODEV;
	buf[0] = reg & 0xff;
	buf[1] = val;
	msg.addr = i2c_client->addr;
	msg.flags = i2c_client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);
	ret = i2c_transfer(i2c_client->adapter, &msg, 1);
	if (ret < 0){
		dev_info(&i2c_client->dev, "i2c transfer failed (%d)\n", ret);
	}
	return ret;
}
static int et5904_i2c_read(struct i2c_client *i2c_client, u8 reg, u8 *val)
{
	int ret;
	u8 buf[1];
	struct i2c_msg msg[2];
	if (i2c_client == NULL)
		return -ENODEV;
	buf[0] = reg & 0xff;
	msg[0].addr = i2c_client->addr;
	msg[0].flags = i2c_client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);
	msg[1].addr = i2c_client->addr;
	msg[1].flags = i2c_client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 1;
	ret = i2c_transfer(i2c_client->adapter, msg, 2);
	if (ret < 0) {
		dev_info(&i2c_client->dev, "i2c transfer failed (%d)\n", ret);
		return ret;
	}
	*val = buf[0];
	return 0;
}
static int et5904_lod3_enable_by_i2c(struct i2c_client *client)
{
	//
	u8 vset = 0, ldoen = 0;
	et5904_i2c_write(client, ET5904_REG_DISCHARGE, 0x8f);
	//ldo3 enable 3.1v
	vset = DIV_ROUND_UP(3100000 - VSET_AVDD_BASE_UV, VSET_AVDD_STEP_UV);
	et5904_i2c_write(client, 0x05, vset);
	et5904_i2c_read(client, ET5904_REG_ENABLE, &ldoen);
	ldoen = ldoen | 0x04;
	et5904_i2c_write(client, ET5904_REG_ENABLE, ldoen);
	return 0;
}
static int et5904_lod3_disable_by_i2c(struct i2c_client *client)
{
	u8 ldoen = 0;
	et5904_i2c_read(client, ET5904_REG_ENABLE, &ldoen);
	ldoen = ldoen & 0x0b;
	et5904_i2c_write(client, ET5904_REG_ENABLE, ldoen);
	return 0;
}
/*common functions*/
static int et5904_read(struct regmap *regmap, u16 reg, u8 *val, int count)
{
	int rc;

	rc = regmap_bulk_read(regmap, reg, val, count);
	if (rc < 0)
		et5904_err("failed to read 0x%04x\n", reg);
	return rc;
}

static int et5904_write(struct regmap *regmap, u16 reg, u8*val, int count)
{
	int rc;

	et5904_debug("Writing 0x%02x to 0x%02x\n", *val, reg);
	rc = regmap_bulk_write(regmap, reg, val, count);
	if (rc < 0)
		et5904_err("failed to write 0x%04x\n", reg);

	return rc;
}

static int et5904_masked_write(struct regmap *regmap, u16 reg, u8 mask, u8 val)
{
	int rc;
	et5904_debug("Writing 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);
	rc = regmap_update_bits(regmap, reg, mask, val);
	if (rc < 0)
		et5904_err("failed to write 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);
	return rc;
}

static int et5904_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct et5904_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;
	u8 reg;

	rc = et5904_read(fan_reg->regmap,
		ET5904_REG_ENABLE, &reg, 1);
	if (rc < 0) {
		et5904_err("[%s] failed to read enable reg rc = %d\n", fan_reg->rdesc.name, rc);
		return rc;
	}
	return !!(reg & (1u << fan_reg->offset));
}

static int et5904_regulator_enable(struct regulator_dev *rdev)
{
	struct et5904_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	if (fan_reg->parent_supply) {
		rc = regulator_enable(fan_reg->parent_supply);
		if (rc < 0) {
			et5904_err("[%s] failed to enable parent rc=%d\n", fan_reg->rdesc.name, rc);
			return rc;
		}
	}

	rc = et5904_masked_write(fan_reg->regmap,
		ET5904_REG_ENABLE,
		1u << fan_reg->offset, 1u << fan_reg->offset);
	if (rc < 0) {
		et5904_err("[%s] failed to enable regulator rc=%d\n", fan_reg->rdesc.name, rc);
		goto remove_vote;
	}
	et5904_debug("[%s][%d] regulator enable\n", fan_reg->rdesc.name, fan_reg->index);
	return 0;

remove_vote:
	if (fan_reg->parent_supply)
		rc = regulator_disable(fan_reg->parent_supply);
	if (rc < 0)
		et5904_err("[%s] failed to disable parent regulator rc=%d\n", fan_reg->rdesc.name, rc);
	return -ETIME;
}

static int et5904_regulator_disable(struct regulator_dev *rdev)
{
	struct et5904_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	rc = et5904_masked_write(fan_reg->regmap,
		ET5904_REG_ENABLE,
		1u << fan_reg->offset, 0);

	if (rc < 0) {
		et5904_err("[%s] failed to disable regulator rc=%d\n", fan_reg->rdesc.name,rc);
		return rc;
	}

	/*remove voltage vot from parent regulator */
	if (fan_reg->parent_supply) {
		rc = regulator_set_voltage(fan_reg->parent_supply, 0, INT_MAX);
		if (rc < 0) {
			et5904_err("[%s] failed to remove parent voltage rc=%d\n", fan_reg->rdesc.name,rc);
			return rc;
		}
		rc = regulator_disable(fan_reg->parent_supply);
		if (rc < 0) {
			et5904_err("[%s] failed to disable parent rc=%d\n", fan_reg->rdesc.name, rc);
			return rc;
		}
	}

	et5904_debug("[%s][%d] regulator disabled\n", fan_reg->rdesc.name, fan_reg->index);
	return 0;
}

static int et5904_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct et5904_regulator *fan_reg = rdev_get_drvdata(rdev);
	u8  vset = 0;
	int rc   = 0;
	int uv   = 0;

	rc = et5904_read(fan_reg->regmap, LDO_VSET_REG(fan_reg->offset),
	&vset, 1);
	if (rc < 0) {
		et5904_err("[%s] failed to read regulator voltage rc = %d\n", fan_reg->rdesc.name,rc);
		return rc;
	}

	if (vset == 0) {
		uv = reg_data[fan_reg->offset].default_uv;
	} else {
		et5904_debug("[%s][%d] voltage read [%x]\n", fan_reg->rdesc.name, fan_reg->index, vset);
		if (fan_reg->offset == 0 || fan_reg->offset == 1)
			uv = VSET_DVDD_BASE_UV + vset * VSET_DVDD_STEP_UV; //DVDD
		else
			uv = VSET_AVDD_BASE_UV + vset * VSET_AVDD_STEP_UV; //AVDD
	}
	et5904_err("et5904_regulator_get_voltage uv:%d\n",uv);
	return uv;
}

static int et5904_write_voltage(struct et5904_regulator* fan_reg, int min_uv,
	int max_uv)
{
	int rc   = 0;
	u8  vset = 0;

	if (min_uv > max_uv) {
		et5904_err("[%s] requestd voltage above maximum limit\n", fan_reg->rdesc.name);
		return -EINVAL;
	}

	if (fan_reg->offset == 0 || fan_reg->offset == 1)
		vset = DIV_ROUND_UP(min_uv - VSET_DVDD_BASE_UV, VSET_DVDD_STEP_UV); //DVDD
	else
		vset = DIV_ROUND_UP(min_uv - VSET_AVDD_BASE_UV, VSET_AVDD_STEP_UV); //AVDD

	rc = et5904_write(fan_reg->regmap, LDO_VSET_REG(fan_reg->offset),
		&vset, 1);
	if (rc < 0) {
		et5904_err("[%s] failed to write voltage rc = %d\n", fan_reg->rdesc.name,rc);
		return rc;
	}

	et5904_debug("[%s][%d] VSET=[0x%2x]\n", fan_reg->rdesc.name, fan_reg->index, vset);
	return 0;
}

static int et5904_regulator_set_voltage(struct regulator_dev *rdev,
	int min_uv, int max_uv, unsigned int* selector)
{
	struct et5904_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	if (fan_reg->parent_supply) {
		rc = regulator_set_voltage(fan_reg->parent_supply,
			fan_reg->min_dropout_uv + min_uv,
			INT_MAX);
		if (rc < 0) {
			et5904_err("[%s] failed to request parent supply voltage rc=%d\n", fan_reg->rdesc.name,rc);
			return rc;
		}
	}

	rc = et5904_write_voltage(fan_reg, min_uv, max_uv);
	if (rc < 0) {
		/* remove parentn's voltage vote */
		if (fan_reg->parent_supply)
			regulator_set_voltage(fan_reg->parent_supply, 0, INT_MAX);
	}
	et5904_debug("[%s][%d] voltage set to %d\n", fan_reg->rdesc.name, fan_reg->index, min_uv);
	return rc;
}

static struct regulator_ops et5904_regulator_ops = {
	.enable      = et5904_regulator_enable,
	.disable     = et5904_regulator_disable,
	.is_enabled  = et5904_regulator_is_enabled,
	.set_voltage = et5904_regulator_set_voltage,
	.get_voltage = et5904_regulator_get_voltage,
};

static int et5904_register_ldo(struct et5904_regulator *et5904_reg,
	const char *name)
{
	struct regulator_config reg_config = {};
	struct regulator_init_data *init_data;

	struct device_node *reg_node = et5904_reg->of_node;
	struct device *dev           = et5904_reg->dev;
	int rc, i, init_voltage;
	char buff[MAX_REG_NAME];

	/* try to find ldo pre-defined in the regulator table */
	for (i = 0; i< ET5904_MAX_LDO; i++) {
		if (!strcmp(reg_data[i].name, name))
			break;
	}

	if (i == ET5904_MAX_LDO) {
		et5904_err("Invalid regulator name %s\n", name);
		return -EINVAL;
	}

	rc = of_property_read_u16(reg_node, "offset", &et5904_reg->offset);
	if (rc < 0) {
		et5904_err("%s:failed to get regulator offset rc = %d\n", name, rc);
		return rc;
	}

	//assign default value defined in code.
	et5904_reg->min_dropout_uv = reg_data[i].min_dropout_uv;
	of_property_read_u32(reg_node, "min-dropout-voltage",
		&et5904_reg->min_dropout_uv);

	et5904_reg->iout_ua = reg_data[i].iout_ua;
	of_property_read_u32(reg_node, "iout_ua",
		&et5904_reg->iout_ua);

	init_voltage = -EINVAL;
	of_property_read_u32(reg_node, "init-voltage", &init_voltage);

	scnprintf(buff, MAX_REG_NAME, "%s-supply", reg_data[i].supply_name);
	if (of_find_property(dev->of_node, buff, NULL)) {
		et5904_reg->parent_supply = devm_regulator_get(dev,
			reg_data[i].supply_name);
		if (IS_ERR(et5904_reg->parent_supply)) {
			rc = PTR_ERR(et5904_reg->parent_supply);
			if (rc != EPROBE_DEFER)
				et5904_err("%s: failed to get parent regulator rc = %d\n",
					name, rc);
				return rc;
		}
	}

	init_data = of_get_regulator_init_data(dev, reg_node, &et5904_reg->rdesc);
	if (init_data == NULL) {
		et5904_err("%s: failed to get regulator data\n", name);
		return -ENODATA;
	}


	if (!init_data->constraints.name) {
		et5904_err("%s: regulator name missing\n", name);
		return -EINVAL;
	}

	/* configure the initial voltage for the regulator */
	if (init_voltage > 0) {
		rc = et5904_write_voltage(et5904_reg, init_voltage,
			init_data->constraints.max_uV);
		if (rc < 0)
			et5904_err("%s:failed to set initial voltage rc = %d\n", name, rc);
	}

	init_data->constraints.input_uV = init_data->constraints.max_uV;
	init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_STATUS
		| REGULATOR_CHANGE_VOLTAGE;

	reg_config.dev         = dev;
	reg_config.init_data   = init_data;
	reg_config.driver_data = et5904_reg;
	reg_config.of_node     = reg_node;

	et5904_reg->rdesc.owner      = THIS_MODULE;
	et5904_reg->rdesc.type       = REGULATOR_VOLTAGE;
	et5904_reg->rdesc.ops        = &et5904_regulator_ops;
	et5904_reg->rdesc.name       = init_data->constraints.name;
	et5904_reg->rdesc.n_voltages = 1;

	et5904_debug("try to register ldo %s\n", name);
	et5904_reg->rdev = devm_regulator_register(dev, &et5904_reg->rdesc,
		&reg_config);
	if (IS_ERR(et5904_reg->rdev)) {
		rc = PTR_ERR(et5904_reg->rdev);
		et5904_err("%s: failed to register regulator rc =%d\n",
		et5904_reg->rdesc.name, rc);
		return rc;
	}

	et5904_debug("%s regulator register done\n", name);
	return 0;
}

static int et5904_parse_regulator(struct regmap *regmap, struct device *dev)
{
	int rc           = 0;
	int index        = 0;
	const char *name = NULL;
	struct device_node *child             = NULL;
	struct et5904_regulator *et5904_reg = NULL;

	of_property_read_u32(dev->of_node, "index", &index);

	/* parse each regulator */
	for_each_available_child_of_node(dev->of_node, child) {
		et5904_reg = devm_kzalloc(dev, sizeof(*et5904_reg), GFP_KERNEL);
		if (!et5904_reg)
			return -ENOMEM;

		et5904_reg->regmap  = regmap;
		et5904_reg->of_node = child;
		et5904_reg->dev     = dev;
		et5904_reg->index   = index;
		et5904_reg->parent_supply = NULL;

		rc = of_property_read_string(child, "regulator-name", &name);
		if (rc)
			continue;
		if((hwLevel  < 3) && (strcmp(name, "et5904-l3") == 0)) {
			et5904_err("P01 dont register ldo %s", name);
			continue;
		}
		rc = et5904_register_ldo(et5904_reg, name);
		if (rc <0 ) {
			et5904_err("failed to register regulator %s rc = %d\n", name, rc);
			return rc;
		}
	}
	return 0;
}

static int et5904_open(struct inode *a_inode, struct file *a_file)
{
	return 0;
}

static int et5904_release(struct inode *a_inode, struct file *a_file)
{
	return 0;
}

static long et5904_ioctl(struct file *a_file, unsigned int a_cmd,
			 unsigned long a_param)
{
	return 0;
}

static const struct file_operations et5904_file_operations = {
	.owner          = THIS_MODULE,
	.open           = et5904_open,
	.release        = et5904_release,
	.unlocked_ioctl = et5904_ioctl,
};

static ssize_t et5904_show_status(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct regmap *regmap = (struct regmap *)dev->driver_data;
	int rc;
	unsigned int val      = 0xFF;
	unsigned int len      = 0;

	rc = regmap_read(regmap, 0x00, &val);
	if (rc < 0) {
		et5904_err("ET5904 failed to get PID\n");
		len = sprintf(buf, "fail\n");
	}
	else {
		et5904_debug("ET5904 get Product ID: [%02x]\n", val);
		len = sprintf(buf, "success\n");
	}

	return len;
}

static ssize_t et5904_show_info(struct device *dev,
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


static ssize_t et5904_show_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct regmap *regmap = (struct regmap *)dev->driver_data;
	int rc;
	unsigned int val      = 0xFF;
	unsigned int len      = 0;

	rc = regmap_read(regmap, ET5904_REG_ENABLE, &val);
	if (rc < 0) {
		len = sprintf(buf, "read 0x0E ==> fail\n");
	}
	else {
		len = sprintf(buf, "read 0x0E ==> 0x%x\n", val);
	}

	return len;
}

static ssize_t et5904_set_enable(struct device *dev,
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
	et5904_write(regmap, ET5904_REG_ENABLE, &val, 1);

	return len;
}


static DEVICE_ATTR(status, S_IWUSR|S_IRUSR, et5904_show_status, NULL);
static DEVICE_ATTR(info, S_IWUSR|S_IRUSR, et5904_show_info, NULL);
static DEVICE_ATTR(enable, S_IWUSR|S_IRUSR, et5904_show_enable, et5904_set_enable);




static int et5904_driver_register(int index, struct regmap *regmap)
{
	unsigned long ret;
	char device_drv_name[ET5904_NAME_STR_LEN_MAX] = { 0 };
	struct et5904_char_dev et5904_dev = et5904_dev_list[index];

	snprintf(device_drv_name, ET5904_NAME_STR_LEN_MAX - 1,
		ET5904_NAME_FMT, index);

	/* Register char driver */
	if (alloc_chrdev_region(&(et5904_dev.dev_no), 0, 1,
			device_drv_name)) {
		et5904_debug("[ET5904] Allocate device no failed\n");
		return -EAGAIN;
	}

	/* Allocate driver */
	et5904_dev.pcdev = cdev_alloc();
	if (et5904_dev.pcdev == NULL) {
		unregister_chrdev_region(et5904_dev.dev_no, 1);
		et5904_debug("[ET5904] Allocate mem for kobject failed\n");
		return -ENOMEM;
	}

	/* Attatch file operation. */
	cdev_init(et5904_dev.pcdev, &et5904_file_operations);
	et5904_dev.pcdev->owner = THIS_MODULE;

	/* Add to system */
	if (cdev_add(et5904_dev.pcdev, et5904_dev.dev_no, 1)) {
		et5904_debug("Attatch file operation failed\n");
		unregister_chrdev_region(et5904_dev.dev_no, 1);
		return -EAGAIN;
	}

	if (pet5904_class == NULL) {
		pet5904_class = class_create(THIS_MODULE, ET5904_CLASS_NAME);
		if (IS_ERR(pet5904_class)) {
			int ret = PTR_ERR(pet5904_class);
			et5904_debug("Unable to create class, err = %d\n", ret);
			return ret;
		}
	}

	et5904_dev.pdevice = device_create(pet5904_class, NULL,
			et5904_dev.dev_no, NULL, device_drv_name);
	if (et5904_dev.pdevice == NULL) {
		et5904_debug("[ET5904] Allocate device_create for kobject failed\n");
		return -ENOMEM;
	}

	et5904_dev.pdevice->driver_data = regmap;
	ret = sysfs_create_file(&(et5904_dev.pdevice->kobj), &dev_attr_status.attr);
	ret = sysfs_create_file(&(et5904_dev.pdevice->kobj), &dev_attr_info.attr);
	ret = sysfs_create_file(&(et5904_dev.pdevice->kobj), &dev_attr_enable.attr);

	return 0;
}

static void et5904_getBoardId(struct device *dev)
{
	int ret;
	unsigned int gpio1, gpio2, gpio3;

	//get gpio
	gpio1 = of_get_named_gpio(dev->of_node, "BoardId_gpio1", 0);//105
	if ((!gpio_is_valid(gpio1))) {
		return;
	}

	gpio2 = of_get_named_gpio(dev->of_node, "BoardId_gpio2", 0);//106
	if ((!gpio_is_valid(gpio2))) {
		return;
	}

	gpio3 = of_get_named_gpio(dev->of_node, "BoardId_gpio3", 0);//113
	if ((!gpio_is_valid(gpio3))) {
		return;
	}

	//request gpio
	ret = gpio_request_one(gpio1, GPIOF_DIR_IN, "et5904_gpio1");
	if (ret) {
		goto err_gpio1;
	}

	ret = gpio_request_one(gpio2, GPIOF_DIR_IN, "et5904_gpio2");
	if (ret) {
		goto err_gpio2;
	}

	ret = gpio_request_one(gpio3, GPIOF_DIR_IN, "et5904_gpio3");
	if (ret) {
		goto err_gpio3;
	}

	//getHwlevel
	hwLevel = (gpio_get_value(gpio1) << 0) | (gpio_get_value(gpio2) << 1) | (gpio_get_value(gpio3) << 2) ;
	et5904_debug("%s get  hwLevel:%d", __func__, hwLevel);

err_gpio3:
	gpio_free(gpio3);
err_gpio2:
	gpio_free(gpio2);
err_gpio1:
	gpio_free(gpio1);

	return;
}
static void et5904_regulator_shutdown(struct i2c_client *client)
{
	if(hwLevel < 3) {
		et5904_debug("ET5904 before P1 i2c disen ldo3 hwLevel[%d]\n", hwLevel);
		et5904_lod3_disable_by_i2c(client);
	}
}
static int et5904_regulator_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc                = 0;
	unsigned int val      = 0xFF;
	struct regmap *regmap = NULL;
	int index = 0;

	client->addr =  (ET5904_SLAVE_ADDR >> 1);

	et5904_getBoardId(&client->dev);
	rc = of_property_read_u32(client->dev.of_node, "index", &index);
	if (rc) {
		et5904_err("failed to read index");
		return rc;
	}

	regmap = devm_regmap_init_i2c(client, &et5904_regmap_config);
	if (IS_ERR(regmap)) {
		et5904_err("ET5904 failed to allocate regmap\n");
		return PTR_ERR(regmap);
	}

	et5904_driver_register(index, regmap);

	rc = regmap_read(regmap, 0x00, &val);
	if (rc < 0) {
		et5904_err("ET5904 failed to get PID\n");
		return -ENODEV;
	}
	else
		et5904_debug("ET5904 get Product ID: [%02x]\n", val);


	rc = et5904_parse_regulator(regmap, &client->dev);
	if (rc < 0) {
		et5904_err("ET5904 failed to parse device tree rc=%d\n", rc);
		return -ENODEV;
	}
	if(hwLevel < 3) {
		et5904_debug("ET5904 before P1 i2c en ldo3 hwLevel[%d]\n", hwLevel);
		et5904_lod3_enable_by_i2c(client);
	}
	
	return 0;
}

static const struct of_device_id et5904_dt_ids[] = {
	{
		.compatible = "etek,et5904",
	},
	{
		.compatible = "awinic,aw37004",
	},
	{ }
};
MODULE_DEVICE_TABLE(of, et5904_dt_ids);

static const struct i2c_device_id et5904_id[] = {
	{
		.name = "et5904-regulator",
		.driver_data = 0,
	},
	{ },
};
MODULE_DEVICE_TABLE(i2c, et5904_id);

static struct i2c_driver et5904_regulator_driver = {
	.driver = {
		.name = "et5904-regulator",
		.of_match_table = of_match_ptr(et5904_dt_ids),
	},
	.probe = et5904_regulator_probe,
	.shutdown = et5904_regulator_shutdown,
	.id_table = et5904_id,
};

module_i2c_driver(et5904_regulator_driver);
MODULE_LICENSE("GPL v2");

