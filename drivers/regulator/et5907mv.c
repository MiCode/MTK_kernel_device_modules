/*
 * ET5907MV ON Semiconductor LDO PMIC Driver.
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


#define et5907mv_err(message, ...) \
        pr_err("[%s] : "message, __func__, ##__VA_ARGS__)
#define et5907mv_debug(message, ...) \
        pr_err("[%s] : "message, __func__, ##__VA_ARGS__)

#define ET5907MV_SLAVE_ADDR      0x6A

#define ET5907MV_REG_ENABLE      0x03
#define ET5907MV_REG_DISCHARGE   0x10
#define ET5907MV_REG_LDO0        0x04

#define LDO_VSET_REG(offset) ((offset) + ET5907MV_REG_LDO0)

#define MAX_REG_NAME     20
#define ET5907MV_MAX_LDO  7

#define ET5907MV_CLASS_NAME       "camera_ldo"
#define ET5907MV_NAME_FMT         "et5907mv%u"
#define ET5907MV_NAME_STR_LEN_MAX 50
#define ET5907MV_MAX_NUMBER       5
#define MV_PER_V               1000

#define volt2regval(wldo_volt_addr, volt)                  \
		(wldo_volt_addr < 2 ?                            \
		(volt - 600 * MV_PER_V) / (6 * MV_PER_V)      \
		: (volt - 1200 * MV_PER_V) / (10 * MV_PER_V))       \

#define regval2volt(wldo_volt_addr, regval)                \
		(wldo_volt_addr < 2 ?                            \
		600 * MV_PER_V + 6 * MV_PER_V * regval      \
		: 1200 * MV_PER_V + 10 * MV_PER_V * regval)         \

struct et5907mv_char_dev {
	dev_t dev_no;
	struct cdev *pcdev;
	struct device *pdevice;
};

static struct class *pet5907mv_class = NULL;
static struct et5907mv_char_dev et5907mv_dev_list[ET5907MV_MAX_NUMBER];

struct et5907mv_regulator{
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
	{ "et5907-l1", "none", 1200000, 80000, 800000},
	{ "et5907-l2", "none", 1200000, 80000, 800000},
	{ "et5907-l3", "none", 2800000, 80000, 800000},
	{ "et5907-l4", "none", 2800000, 80000, 800000},
	{ "et5907-l5", "none", 2800000, 80000, 800000},
	{ "et5907-l6", "none", 2800000, 80000, 800000},
	{ "et5907-l7", "none", 2800000, 80000, 800000},
};

static const struct regmap_config et5907mv_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

/*common functions*/
static int et5907mv_read(struct regmap *regmap, u16 reg, u8 *val, int count)
{
	int rc;

	rc = regmap_bulk_read(regmap, reg, val, count);
	if (rc < 0)
		et5907mv_err("failed to read 0x%04x\n", reg);
	return rc;
}

static int et5907mv_write(struct regmap *regmap, u16 reg, u8*val, int count)
{
	int rc;

	et5907mv_debug("Writing 0x%02x to 0x%02x\n", *val, reg);
	rc = regmap_bulk_write(regmap, reg, val, count);
	if (rc < 0)
		et5907mv_err("failed to write 0x%04x\n", reg);

	return rc;
}

static int et5907mv_masked_write(struct regmap *regmap, u16 reg, u8 mask, u8 val)
{
	int rc;
	et5907mv_debug("Writing 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);
	rc = regmap_update_bits(regmap, reg, mask, val);
	if (rc < 0)
		et5907mv_err("failed to write 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);
	return rc;
}

static int et5907mv_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct et5907mv_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;
	u8 reg;

	rc = et5907mv_read(fan_reg->regmap,
		ET5907MV_REG_ENABLE, &reg, 1);
	if (rc < 0) {
		et5907mv_err("[%s] failed to read enable reg rc = %d\n", fan_reg->rdesc.name, rc);
		return rc;
	}
	return !!(reg & (1u << fan_reg->offset));
}

static int et5907mv_regulator_enable(struct regulator_dev *rdev)
{
	struct et5907mv_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	if (fan_reg->parent_supply) {
		rc = regulator_enable(fan_reg->parent_supply);
		if (rc < 0) {
			et5907mv_err("[%s] failed to enable parent rc=%d\n", fan_reg->rdesc.name, rc);
			return rc;
		}
	}
	et5907mv_err("fan_reg->offset is(begin)  == %d\n" ,fan_reg->offset);
	rc = et5907mv_masked_write(fan_reg->regmap,
		ET5907MV_REG_ENABLE,
		1u << fan_reg->offset, 1u << fan_reg->offset);
	et5907mv_err("fan_reg->offset is(end)  == %d\n" ,fan_reg->offset);
	if (rc < 0) {
		et5907mv_err("[%s] failed to enable regulator rc=%d\n", fan_reg->rdesc.name, rc);
		goto remove_vote;
	}
	et5907mv_debug("[%s][%d] regulator enable\n", fan_reg->rdesc.name, fan_reg->index);
	return 0;

remove_vote:
	if (fan_reg->parent_supply)
		rc = regulator_disable(fan_reg->parent_supply);
	if (rc < 0)
		et5907mv_err("[%s] failed to disable parent regulator rc=%d\n", fan_reg->rdesc.name, rc);
	return -ETIME;
}

static int et5907mv_regulator_disable(struct regulator_dev *rdev)
{
	struct et5907mv_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	rc = et5907mv_masked_write(fan_reg->regmap,
		ET5907MV_REG_ENABLE,
		1u << fan_reg->offset, 0);

	if (rc < 0) {
		et5907mv_err("[%s] failed to disable regulator rc=%d\n", fan_reg->rdesc.name,rc);
		return rc;
	}

	/*remove voltage vot from parent regulator */
	if (fan_reg->parent_supply) {
		rc = regulator_set_voltage(fan_reg->parent_supply, 0, INT_MAX);
		if (rc < 0) {
			et5907mv_err("[%s] failed to remove parent voltage rc=%d\n", fan_reg->rdesc.name,rc);
			return rc;
		}
		rc = regulator_disable(fan_reg->parent_supply);
		if (rc < 0) {
			et5907mv_err("[%s] failed to disable parent rc=%d\n", fan_reg->rdesc.name, rc);
			return rc;
		}
	}

	et5907mv_debug("[%s][%d] regulator disabled\n", fan_reg->rdesc.name, fan_reg->index);
	return 0;
}

static int et5907mv_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct et5907mv_regulator *fan_reg = rdev_get_drvdata(rdev);
	u8  vset = 0;
	int rc   = 0;
	int uv   = 0;

	rc = et5907mv_read(fan_reg->regmap, LDO_VSET_REG(fan_reg->offset),
	&vset, 1);
	if (rc < 0) {
		et5907mv_err("[%s] failed to read regulator voltage rc = %d\n", fan_reg->rdesc.name,rc);
		return rc;
	}

	if (vset == 0) {
		uv = reg_data[fan_reg->offset].default_uv;
	} else {
		et5907mv_debug("[%s][%d] voltage read [%x]\n", fan_reg->rdesc.name, fan_reg->index, vset);

		uv = regval2volt(fan_reg->offset, vset);
	}
	return uv;
}

static int et5907mv_write_voltage(struct et5907mv_regulator* fan_reg, int min_uv,
	int max_uv)
{
	int rc   = 0;
	u8  vset = 0;

	et5907mv_err("write_min_uv ==  %d, write_max_uv == %d\n", min_uv, max_uv);

	if (min_uv > max_uv) {
		et5907mv_err("[%s] requestd voltage above maximum limit\n", fan_reg->rdesc.name);
		return -EINVAL;
	}

	vset = volt2regval(fan_reg->offset, min_uv);

	et5907mv_err("fan_reg->offset ==  %d, min_uv == %d, VSET=[0x%2x]\n", fan_reg->offset, min_uv, vset);

	rc = et5907mv_write(fan_reg->regmap, LDO_VSET_REG(fan_reg->offset),
		&vset, 1);
	if (rc < 0) {
		et5907mv_err("[%s] failed to write voltage rc = %d\n", fan_reg->rdesc.name,rc);
		return rc;
	}

	et5907mv_debug("[%s][%d] VSET=[0x%2x]\n", fan_reg->rdesc.name, fan_reg->index, vset);
	return 0;
}

static int et5907mv_regulator_set_voltage(struct regulator_dev *rdev,
	int min_uv, int max_uv, unsigned int* selector)
{
	struct et5907mv_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	if (fan_reg->parent_supply) {
		rc = regulator_set_voltage(fan_reg->parent_supply,
			fan_reg->min_dropout_uv + min_uv,
			INT_MAX);
		if (rc < 0) {
			et5907mv_err("[%s] failed to request parent supply voltage rc=%d\n", fan_reg->rdesc.name,rc);
			return rc;
		}
	}

	rc = et5907mv_write_voltage(fan_reg, min_uv, max_uv);
	if (rc < 0) {
		/* remove parentn's voltage vote */
		if (fan_reg->parent_supply)
			regulator_set_voltage(fan_reg->parent_supply, 0, INT_MAX);
	}
	et5907mv_debug("[%s][%d] voltage set to %d\n", fan_reg->rdesc.name, fan_reg->index, min_uv);
	return rc;
}

static struct regulator_ops et5907mv_regulator_ops = {
	.enable      = et5907mv_regulator_enable,
	.disable     = et5907mv_regulator_disable,
	.is_enabled  = et5907mv_regulator_is_enabled,
	.set_voltage = et5907mv_regulator_set_voltage,
	.get_voltage = et5907mv_regulator_get_voltage,
};

static int et5907mv_register_ldo(struct et5907mv_regulator *et5907mv_reg,
	const char *name)
{
	struct regulator_config reg_config = {};
	struct regulator_init_data *init_data;

	struct device_node *reg_node = et5907mv_reg->of_node;
	struct device *dev           = et5907mv_reg->dev;
	int rc, i, init_voltage;
	char buff[MAX_REG_NAME];

	/* try to find ldo pre-defined in the regulator table */
	for (i = 0; i< ET5907MV_MAX_LDO; i++) {
		if (!strcmp(reg_data[i].name, name))
			break;
	}

	if (i == ET5907MV_MAX_LDO) {
		et5907mv_err("Invalid regulator name %s\n", name);
		return -EINVAL;
	}

	rc = of_property_read_u16(reg_node, "offset", &et5907mv_reg->offset);
	if (rc < 0) {
		et5907mv_err("%s:failed to get regulator offset rc = %d\n", name, rc);
		return rc;
	}

	//assign default value defined in code.
	et5907mv_reg->min_dropout_uv = reg_data[i].min_dropout_uv;
	of_property_read_u32(reg_node, "min-dropout-voltage",
		&et5907mv_reg->min_dropout_uv);

	et5907mv_reg->iout_ua = reg_data[i].iout_ua;
	of_property_read_u32(reg_node, "iout_ua",
		&et5907mv_reg->iout_ua);

	init_voltage = -EINVAL;
	of_property_read_u32(reg_node, "init-voltage", &init_voltage);

	scnprintf(buff, MAX_REG_NAME, "%s-supply", reg_data[i].supply_name);
	if (of_find_property(dev->of_node, buff, NULL)) {
		et5907mv_reg->parent_supply = devm_regulator_get(dev,
			reg_data[i].supply_name);
		if (IS_ERR(et5907mv_reg->parent_supply)) {
			rc = PTR_ERR(et5907mv_reg->parent_supply);
			if (rc != EPROBE_DEFER)
				et5907mv_err("%s: failed to get parent regulator rc = %d\n",
					name, rc);
				return rc;
		}
	}

	init_data = of_get_regulator_init_data(dev, reg_node, &et5907mv_reg->rdesc);
	if (init_data == NULL) {
		et5907mv_err("%s: failed to get regulator data\n", name);
		return -ENODATA;
	}


	if (!init_data->constraints.name) {
		et5907mv_err("%s: regulator name missing\n", name);
		return -EINVAL;
	}

	/* configure the initial voltage for the regulator */
	if (init_voltage > 0) {
		rc = et5907mv_write_voltage(et5907mv_reg, init_voltage,
			init_data->constraints.max_uV);
		if (rc < 0)
			et5907mv_err("%s:failed to set initial voltage rc = %d\n", name, rc);
	}

	init_data->constraints.input_uV = init_data->constraints.max_uV;
	init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_STATUS
		| REGULATOR_CHANGE_VOLTAGE;

	reg_config.dev         = dev;
	reg_config.init_data   = init_data;
	reg_config.driver_data = et5907mv_reg;
	reg_config.of_node     = reg_node;

	et5907mv_reg->rdesc.owner      = THIS_MODULE;
	et5907mv_reg->rdesc.type       = REGULATOR_VOLTAGE;
	et5907mv_reg->rdesc.ops        = &et5907mv_regulator_ops;
	et5907mv_reg->rdesc.name       = init_data->constraints.name;
	et5907mv_reg->rdesc.n_voltages = 1;

	et5907mv_debug("try to register ldo %s\n", name);

	et5907mv_reg->rdev = devm_regulator_register(dev, &et5907mv_reg->rdesc,
		&reg_config);
	if (IS_ERR(et5907mv_reg->rdev)) {
		rc = PTR_ERR(et5907mv_reg->rdev);
		et5907mv_err("%s: failed to register regulator rc =%d\n",
		et5907mv_reg->rdesc.name, rc);
		return rc;
	}

	et5907mv_debug("%s regulator register done\n", name);
	return 0;
}

static int et5907mv_parse_regulator(struct regmap *regmap, struct device *dev)
{
	int rc           = 0;
	int index        = 0;
	const char *name = NULL;
	struct device_node *child             = NULL;
	struct et5907mv_regulator *et5907mv_reg = NULL;

	of_property_read_u32(dev->of_node, "index", &index);

	/* parse each regulator */
	for_each_available_child_of_node(dev->of_node, child) {
		et5907mv_reg = devm_kzalloc(dev, sizeof(*et5907mv_reg), GFP_KERNEL);
		if (!et5907mv_reg)
			return -ENOMEM;

		et5907mv_reg->regmap  = regmap;
		et5907mv_reg->of_node = child;
		et5907mv_reg->dev     = dev;
		et5907mv_reg->index   = index;
		et5907mv_reg->parent_supply = NULL;

		rc = of_property_read_string(child, "regulator-name", &name);
		if (rc)
			continue;

		rc = et5907mv_register_ldo(et5907mv_reg, name);
		if (rc <0 ) {
			et5907mv_err("failed to register regulator %s rc = %d\n", name, rc);
			return rc;
		}
	}
	return 0;
}

static int et5907mv_open(struct inode *a_inode, struct file *a_file)
{
	return 0;
}

static int et5907mv_release(struct inode *a_inode, struct file *a_file)
{
	return 0;
}

static long et5907mv_ioctl(struct file *a_file, unsigned int a_cmd,
			 unsigned long a_param)
{
	return 0;
}

static const struct file_operations et5907mv_file_operations = {
	.owner          = THIS_MODULE,
	.open           = et5907mv_open,
	.release        = et5907mv_release,
	.unlocked_ioctl = et5907mv_ioctl,
};

static ssize_t et5907mv_show_status(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct regmap *regmap = (struct regmap *)dev->driver_data;
	int rc;
	unsigned int val      = 0xFF;
	unsigned int len      = 0;

	rc = regmap_read(regmap, 0x00, &val);
	if (rc < 0) {
		et5907mv_err("ET5907MV failed to get PID\n");
		len = sprintf(buf, "fail\n");
	}
	else {
		et5907mv_debug("ET5907MV get Product ID: [%02x]\n", val);
		len = sprintf(buf, "success\n");
	}

	return len;
}

static ssize_t et5907mv_show_info(struct device *dev,
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

static ssize_t et5907mv_show_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct regmap *regmap = (struct regmap *)dev->driver_data;
	int rc;
	unsigned int val      = 0xFF;
	unsigned int len      = 0;

	rc = regmap_read(regmap, ET5907MV_REG_ENABLE, &val);
	if (rc < 0) {
		len = sprintf(buf, "read 0x03 ==> fail\n");
	}
	else {
		len = sprintf(buf, "read 0x03 ==> 0x%x\n", val);
	}

	return len;
}

static ssize_t et5907mv_set_enable(struct device *dev,
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
	et5907mv_write(regmap, ET5907MV_REG_ENABLE, &val, 1);

	return len;
}

static DEVICE_ATTR(status, S_IWUSR|S_IRUSR, et5907mv_show_status, NULL);
static DEVICE_ATTR(info, S_IWUSR|S_IRUSR, et5907mv_show_info, NULL);
static DEVICE_ATTR(enable, S_IWUSR|S_IRUSR, et5907mv_show_enable, et5907mv_set_enable);

static int et5907mv_driver_register(int index, struct regmap *regmap)
{
	unsigned long ret;
	char device_drv_name[ET5907MV_NAME_STR_LEN_MAX] = { 0 };
	struct et5907mv_char_dev et5907mv_dev = et5907mv_dev_list[index];

	snprintf(device_drv_name, ET5907MV_NAME_STR_LEN_MAX - 1,
		ET5907MV_NAME_FMT, index);

	/* Register char driver */
	if (alloc_chrdev_region(&(et5907mv_dev.dev_no), 0, 1,
			device_drv_name)) {
		et5907mv_debug("[ET5907MV] Allocate device no failed\n");
		return -EAGAIN;
	}

	/* Allocate driver */
	et5907mv_dev.pcdev = cdev_alloc();
	if (et5907mv_dev.pcdev == NULL) {
		unregister_chrdev_region(et5907mv_dev.dev_no, 1);
		et5907mv_debug("[ET5907MV] Allocate mem for kobject failed\n");
		return -ENOMEM;
	}

	/* Attatch file operation. */
	cdev_init(et5907mv_dev.pcdev, &et5907mv_file_operations);
	et5907mv_dev.pcdev->owner = THIS_MODULE;

	/* Add to system */
	if (cdev_add(et5907mv_dev.pcdev, et5907mv_dev.dev_no, 1)) {
		et5907mv_debug("Attatch file operation failed\n");
		unregister_chrdev_region(et5907mv_dev.dev_no, 1);
		return -EAGAIN;
	}

	if (pet5907mv_class == NULL) {
		pet5907mv_class = class_create(ET5907MV_CLASS_NAME);
		if (IS_ERR(pet5907mv_class)) {
			int ret = PTR_ERR(pet5907mv_class);
			et5907mv_debug("Unable to create class, err = %d\n", ret);
			return ret;
		}
	}

	et5907mv_dev.pdevice = device_create(pet5907mv_class, NULL,
			et5907mv_dev.dev_no, NULL, device_drv_name);
	if (et5907mv_dev.pdevice == NULL) {
		et5907mv_debug("[ET5907MV] Allocate device_create for kobject failed\n");
		return -ENOMEM;
	}

	et5907mv_dev.pdevice->driver_data = regmap;
	ret = sysfs_create_file(&(et5907mv_dev.pdevice->kobj), &dev_attr_status.attr);
	ret = sysfs_create_file(&(et5907mv_dev.pdevice->kobj), &dev_attr_info.attr);
	ret = sysfs_create_file(&(et5907mv_dev.pdevice->kobj), &dev_attr_enable.attr);

	return 0;
}
static int et5907mv_regulator_probe(struct i2c_client *client)
{
	int rc                = 0;
	unsigned int val      = 0xFF;
	struct regmap *regmap = NULL;
	int index = 0;

	client->addr =  (ET5907MV_SLAVE_ADDR >> 1);

	rc = of_property_read_u32(client->dev.of_node, "index", &index);
	if (rc) {
		et5907mv_err("failed to read index");
		return rc;
	}

	regmap = devm_regmap_init_i2c(client, &et5907mv_regmap_config);
	if (IS_ERR(regmap)) {
		et5907mv_err("ET5907MV failed to allocate regmap\n");
		return PTR_ERR(regmap);
	}

	et5907mv_driver_register(index, regmap);

	rc = regmap_read(regmap, 0x00, &val);
	if (rc < 0) {
		et5907mv_err("ET5907MV failed to get PID\n");
		return -ENODEV;
	}
	else
		et5907mv_debug("ET5907MV get Product ID: [%02x]\n", val);


	rc = et5907mv_parse_regulator(regmap, &client->dev);
	if (rc < 0) {
		et5907mv_err("ET5907MV failed to parse device tree rc=%d\n", rc);
		return -ENODEV;
	}

	return 0;
}

static const struct of_device_id et5907mv_dt_ids[] = {
	{
		.compatible = "etek,et5907",
	},
	{ }
};
MODULE_DEVICE_TABLE(of, et5907mv_dt_ids);

static const struct i2c_device_id et5907mv_id[] = {
	{
		.name = "et5907mv-regulator",
		.driver_data = 0,
	},
	{ },
};
MODULE_DEVICE_TABLE(i2c, et5907mv_id);

static struct i2c_driver et5907mv_regulator_driver = {
	.driver = {
		.name = "et5907mv-regulator",
		.of_match_table = of_match_ptr(et5907mv_dt_ids),
	},
	.probe = et5907mv_regulator_probe,
	.id_table = et5907mv_id,
};

module_i2c_driver(et5907mv_regulator_driver);
MODULE_LICENSE("GPL v2");

