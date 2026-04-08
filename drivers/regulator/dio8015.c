/*
 * dio8015 ON Semiconductor LDO PMIC Driver.
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


#define dio8015_err(message, ...) \
        pr_err("[%s] : "message, __func__, ##__VA_ARGS__)
#define dio8015_debug(message, ...) \
        pr_err("[%s] : "message, __func__, ##__VA_ARGS__)

#define dio8015_REG_ENABLE      0x0E
#define dio8015_REG_DISCHARGE   0x02
#define dio8015_REG_LDO0        0x03

#define LDO_VSET_REG(offset) ((offset) + dio8015_REG_LDO0)

#define VSET_DVDD_BASE_UV   600000
#define VSET_DVDD_STEP_UV   6000

#define VSET_AVDD_BASE_UV   1200000
#define VSET_AVDD_STEP_UV   12500

#define MAX_REG_NAME     20
#define dio8015_MAX_LDO  4

#define dio8015_CLASS_NAME       "camera_ldo"
#define dio8015_NAME_FMT         "dio8015%u"
#define dio8015_NAME_STR_LEN_MAX 50
#define dio8015_MAX_NUMBER       5

struct dio8015_char_dev {
	dev_t dev_no;
	struct cdev *pcdev;
	struct device *pdevice;
};

static struct class *pdio8015_class = NULL;
static struct dio8015_char_dev dio8015_dev_list[dio8015_MAX_NUMBER];

struct dio8015_regulator{
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
	{ "dvdd1", "none", 1200000, 600000, 500000},
	{ "dvdd2", "none", 1200000, 600000, 500000},
	{ "avdd1", "none", 2800000, 1200000, 300000},
	{ "avdd2", "none", 2800000, 1200000, 300000},
};

static const struct regmap_config dio8015_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

/*common functions*/
static int dio8015_read(struct regmap *regmap, u16 reg, u8*val, int count)
{
	int rc;

	rc = regmap_bulk_read(regmap, reg, val, count);
	if (rc < 0)
		dio8015_err("failed to read 0x%04x\n", reg);
	return rc;
}

static int dio8015_write(struct regmap *regmap, u16 reg, u8*val, int count)
{
	int rc;
	u8 temp;

	dio8015_err("Writing 0x%02x to 0x%02x\n", *val, reg);
	rc = regmap_bulk_write(regmap, reg, val, count);
	if (rc < 0)
		dio8015_err("failed to write 0x%04x\n", reg);

	rc = dio8015_read(regmap, reg, &temp, count);

	dio8015_err("to write 0x%04x  exp 0x%x now 0x%x \n", reg, *val, temp);

	return rc;
}

static int dio8015_masked_write(struct regmap *regmap, u16 reg, u8 mask, u8 val)
{
	int rc;
	u8 temp = 0,new = 0;
	dio8015_debug("Writing 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);

	rc = dio8015_read(regmap, reg, &temp, 1);
	temp = 	temp & ~mask;
	temp |=  val & mask ;

	rc = regmap_update_bits(regmap, reg, mask, val);
	if (rc < 0)
		dio8015_err("failed to write 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);

	rc = dio8015_read(regmap, reg, &new, 1);

	dio8015_err("write 0x%04x  exp 0x%x now 0x%x \n", reg,temp,new);

	return rc;
}

void dio8015_show_reg_info(struct regmap *regmap)
{
	int rc;
	unsigned int val      = 0xFF;
	int i = 0;

	if(regmap != NULL){
		for (i = 0; i <= 0x0F; i++) {
			rc = regmap_read(regmap, i, &val);
			if (rc < 0) {
				dio8015_err("dio8015 failed to get value \n");
			}
			else {
				dio8015_err("dio8015 dump reg: addr 0x%x value 0x%x \n",i,val);
			}
		}
		dio8015_err("dump dio8015 success!");
	}else{
		dio8015_err("regmap == NULL ");
	}
}
EXPORT_SYMBOL(dio8015_show_reg_info);


static int dio8015_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct dio8015_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;
	u8 reg;

	rc = dio8015_read(fan_reg->regmap,
		dio8015_REG_ENABLE, &reg, 1);
	if (rc < 0) {
		dio8015_err("[%s] failed to read enable reg rc = %d\n", fan_reg->rdesc.name, rc);
		return rc;
	}
	return !!(reg & (1u << fan_reg->offset));
}

static int dio8015_regulator_enable(struct regulator_dev *rdev)
{
	struct dio8015_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	if (fan_reg->parent_supply) {
		rc = regulator_enable(fan_reg->parent_supply);
		if (rc < 0) {
			dio8015_err("[%s] failed to enable parent rc=%d\n", fan_reg->rdesc.name, rc);
			return rc;
		}
	}

	rc = dio8015_masked_write(fan_reg->regmap,
		dio8015_REG_ENABLE,
		1u << fan_reg->offset, 1u << fan_reg->offset);
	if (rc < 0) {
		dio8015_err("[%s] failed to enable regulator rc=%d\n", fan_reg->rdesc.name, rc);
		goto remove_vote;
	}
	dio8015_debug("[%s][%d] regulator enable\n", fan_reg->rdesc.name, fan_reg->index);
	return 0;

remove_vote:
	if (fan_reg->parent_supply)
		rc = regulator_disable(fan_reg->parent_supply);
	if (rc < 0)
		dio8015_err("[%s] failed to disable parent regulator rc=%d\n", fan_reg->rdesc.name, rc);
	return -ETIME;
}

static int dio8015_regulator_disable(struct regulator_dev *rdev)
{
	struct dio8015_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	rc = dio8015_masked_write(fan_reg->regmap,
		dio8015_REG_ENABLE,
		1u << fan_reg->offset, 0);

	if (rc < 0) {
		dio8015_err("[%s] failed to disable regulator rc=%d\n", fan_reg->rdesc.name,rc);
		return rc;
	}

	/*remove voltage vot from parent regulator */
	if (fan_reg->parent_supply) {
		rc = regulator_set_voltage(fan_reg->parent_supply, 0, INT_MAX);
		if (rc < 0) {
			dio8015_err("[%s] failed to remove parent voltage rc=%d\n", fan_reg->rdesc.name,rc);
			return rc;
		}
		rc = regulator_disable(fan_reg->parent_supply);
		if (rc < 0) {
			dio8015_err("[%s] failed to disable parent rc=%d\n", fan_reg->rdesc.name, rc);
			return rc;
		}
	}

	dio8015_debug("[%s][%d] regulator disabled\n", fan_reg->rdesc.name, fan_reg->index);
	return 0;
}

static int dio8015_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct dio8015_regulator *fan_reg = rdev_get_drvdata(rdev);
	u8  vset = 0;
	int rc   = 0;
	int uv   = 0;

	rc = dio8015_read(fan_reg->regmap, LDO_VSET_REG(fan_reg->offset),
	&vset, 1);
	if (rc < 0) {
		dio8015_err("[%s] failed to read regulator voltage rc = %d\n", fan_reg->rdesc.name,rc);
		return rc;
	}

	if (vset == 0) {
		uv = reg_data[fan_reg->offset].default_uv;
	} else {
		dio8015_debug("[%s][%d] voltage read [%x]\n", fan_reg->rdesc.name, fan_reg->index, vset);
		if (fan_reg->offset == 0 || fan_reg->offset == 1)
			uv = VSET_DVDD_BASE_UV + vset * VSET_DVDD_STEP_UV; //DVDD
		else
			uv = VSET_AVDD_BASE_UV + vset * VSET_AVDD_STEP_UV; //AVDD
	}
	return uv;
}

static int dio8015_write_voltage(struct dio8015_regulator* fan_reg, int min_uv,
	int max_uv)
{
	int rc   = 0;
	u8  vset = 0;

	if (min_uv > max_uv) {
		dio8015_err("[%s] requestd voltage above maximum limit\n", fan_reg->rdesc.name);
		return -EINVAL;
	}

	if (fan_reg->offset == 0 || fan_reg->offset == 1)
		vset = DIV_ROUND_UP(min_uv - VSET_DVDD_BASE_UV, VSET_DVDD_STEP_UV); //DVDD
	else
		vset = DIV_ROUND_UP(min_uv - VSET_AVDD_BASE_UV, VSET_AVDD_STEP_UV); //AVDD

	rc = dio8015_write(fan_reg->regmap, LDO_VSET_REG(fan_reg->offset),
		&vset, 1);
	if (rc < 0) {
		dio8015_err("[%s] failed to write voltage rc = %d\n", fan_reg->rdesc.name,rc);
		return rc;
	}

	dio8015_debug("[%s][%d] VSET=[0x%2x]\n", fan_reg->rdesc.name, fan_reg->index, vset);
	return 0;
}

static int dio8015_regulator_set_voltage(struct regulator_dev *rdev,
	int min_uv, int max_uv, unsigned int* selector)
{
	struct dio8015_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	if (fan_reg->parent_supply) {
		rc = regulator_set_voltage(fan_reg->parent_supply,
			fan_reg->min_dropout_uv + min_uv,
			INT_MAX);
		if (rc < 0) {
			dio8015_err("[%s] failed to request parent supply voltage rc=%d\n", fan_reg->rdesc.name,rc);
			return rc;
		}
	}

	rc = dio8015_write_voltage(fan_reg, min_uv, max_uv);
	if (rc < 0) {
		/* remove parentn's voltage vote */
		if (fan_reg->parent_supply)
			regulator_set_voltage(fan_reg->parent_supply, 0, INT_MAX);
	}
	dio8015_debug("[%s][%d] voltage set to %d\n", fan_reg->rdesc.name, fan_reg->index, min_uv);
	return rc;
}

static struct regulator_ops dio8015_regulator_ops = {
	.enable      = dio8015_regulator_enable,
	.disable     = dio8015_regulator_disable,
	.is_enabled  = dio8015_regulator_is_enabled,
	.set_voltage = dio8015_regulator_set_voltage,
	.get_voltage = dio8015_regulator_get_voltage,
};

static int dio8015_register_ldo(struct dio8015_regulator *dio8015_reg,
	const char *name)
{
	struct regulator_config reg_config = {};
	struct regulator_init_data *init_data;

	struct device_node *reg_node = dio8015_reg->of_node;
	struct device *dev           = dio8015_reg->dev;
	int rc, i, init_voltage;
	char buff[MAX_REG_NAME];

	/* try to find ldo pre-defined in the regulator table */
	for (i = 0; i< dio8015_MAX_LDO; i++) {
		if (!strcmp(reg_data[i].name, name))
			break;
	}

	if (i == dio8015_MAX_LDO) {
		dio8015_err("Invalid regulator name %s\n", name);
		return -EINVAL;
	}

	rc = of_property_read_u16(reg_node, "offset", &dio8015_reg->offset);
	if (rc < 0) {
		dio8015_err("%s:failed to get regulator offset rc = %d\n", name, rc);
		return rc;
	}

	//assign default value defined in code.
	dio8015_reg->min_dropout_uv = reg_data[i].min_dropout_uv;
	of_property_read_u32(reg_node, "min-dropout-voltage",
		&dio8015_reg->min_dropout_uv);

	dio8015_reg->iout_ua = reg_data[i].iout_ua;
	of_property_read_u32(reg_node, "iout_ua",
		&dio8015_reg->iout_ua);

	init_voltage = -EINVAL;
	of_property_read_u32(reg_node, "init-voltage", &init_voltage);

	scnprintf(buff, MAX_REG_NAME, "%s-supply", reg_data[i].supply_name);
	if (of_find_property(dev->of_node, buff, NULL)) {
		dio8015_reg->parent_supply = devm_regulator_get(dev,
			reg_data[i].supply_name);
		if (IS_ERR(dio8015_reg->parent_supply)) {
			rc = PTR_ERR(dio8015_reg->parent_supply);
			if (rc != EPROBE_DEFER)
				dio8015_err("%s: failed to get parent regulator rc = %d\n",
					name, rc);
				return rc;
		}
	}

	init_data = of_get_regulator_init_data(dev, reg_node, &dio8015_reg->rdesc);
	if (init_data == NULL) {
		dio8015_err("%s: failed to get regulator data\n", name);
		return -ENODATA;
	}


	if (!init_data->constraints.name) {
		dio8015_err("%s: regulator name missing\n", name);
		return -EINVAL;
	}

	/* configure the initial voltage for the regulator */
	if (init_voltage > 0) {
		rc = dio8015_write_voltage(dio8015_reg, init_voltage,
			init_data->constraints.max_uV);
		if (rc < 0)
			dio8015_err("%s:failed to set initial voltage rc = %d\n", name, rc);
	}

	init_data->constraints.input_uV = init_data->constraints.max_uV;
	init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_STATUS
		| REGULATOR_CHANGE_VOLTAGE;

	reg_config.dev         = dev;
	reg_config.init_data   = init_data;
	reg_config.driver_data = dio8015_reg;
	reg_config.of_node     = reg_node;

	dio8015_reg->rdesc.owner      = THIS_MODULE;
	dio8015_reg->rdesc.type       = REGULATOR_VOLTAGE;
	dio8015_reg->rdesc.ops        = &dio8015_regulator_ops;
	dio8015_reg->rdesc.name       = init_data->constraints.name;
	dio8015_reg->rdesc.n_voltages = 1;

	dio8015_debug("try to register ldo %s\n", name);
	dio8015_reg->rdev = devm_regulator_register(dev, &dio8015_reg->rdesc,
		&reg_config);
	if (IS_ERR(dio8015_reg->rdev)) {
		rc = PTR_ERR(dio8015_reg->rdev);
		dio8015_err("%s: failed to register regulator rc =%d\n",
		dio8015_reg->rdesc.name, rc);
		return rc;
	}

	dio8015_debug("%s regulator register done\n", name);
	return 0;
}

static int dio8015_parse_regulator(struct regmap *regmap, struct device *dev)
{
	int rc           = 0;
	int index        = 0;
	const char *name = NULL;
	struct device_node *child             = NULL;
	struct dio8015_regulator *dio8015_reg = NULL;

	of_property_read_u32(dev->of_node, "index", &index);

	/* parse each regulator */
	for_each_available_child_of_node(dev->of_node, child) {
		dio8015_reg = devm_kzalloc(dev, sizeof(*dio8015_reg), GFP_KERNEL);
		if (!dio8015_reg)
			return -ENOMEM;

		dio8015_reg->regmap  = regmap;
		dio8015_reg->of_node = child;
		dio8015_reg->dev     = dev;
		dio8015_reg->index   = index;
		dio8015_reg->parent_supply = NULL;

		rc = of_property_read_string(child, "regulator-name", &name);
		if (rc)
			continue;

		rc = dio8015_register_ldo(dio8015_reg, name);
		if (rc <0 ) {
			dio8015_err("failed to register regulator %s rc = %d\n", name, rc);
			return rc;
		}
	}
	return 0;
}

static int dio8015_open(struct inode *a_inode, struct file *a_file)
{
	return 0;
}

static int dio8015_release(struct inode *a_inode, struct file *a_file)
{
	return 0;
}

static long dio8015_ioctl(struct file *a_file, unsigned int a_cmd,
			 unsigned long a_param)
{
	return 0;
}

static const struct file_operations dio8015_file_operations = {
	.owner          = THIS_MODULE,
	.open           = dio8015_open,
	.release        = dio8015_release,
	.unlocked_ioctl = dio8015_ioctl,
};

static ssize_t dio8015_show_status(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct regmap *regmap = (struct regmap *)dev->driver_data;
	int rc;
	unsigned int val      = 0xFF;
	unsigned int len      = 0;

	rc = regmap_read(regmap, 0x00, &val);
	if (rc < 0) {
		dio8015_err("dio8015 failed to get PID\n");
		len = sprintf(buf, "fail\n");
	} else {
		dio8015_debug("dio8015 get Product ID: [%02x]\n", val);
		len = sprintf(buf, "success\n");
	}

	return len;
}

static ssize_t dio8015_show_info(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct regmap *regmap = (struct regmap *)dev->driver_data;
	int rc;
	unsigned int val      = 0xFF;
	unsigned int len      = 0;
	int i = 0;

	for (i = 0; i <= 0x0F; i++) {
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


static ssize_t dio8015_show_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct regmap *regmap = (struct regmap *)dev->driver_data;
	int rc;
	unsigned int val      = 0xFF;
	unsigned int len      = 0;

	rc = regmap_read(regmap, dio8015_REG_ENABLE, &val);
	if (rc < 0) {
		len = sprintf(buf, "read 0x0E ==> fail\n");
	}
	else {
		len = sprintf(buf, "read 0x0E ==> 0x%x\n", val);
	}

	return len;
}

static ssize_t dio8015_set_enable(struct device *dev,
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
	dio8015_write(regmap, dio8015_REG_ENABLE, &val, 1);

	return len;
}

static ssize_t dio8015_set_reset(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t len)
{
	u8 val = 0, rest_val = 0;
	int rc;
	int i = 0;
	struct regmap *regmap = (struct regmap *)dev->driver_data;

	if (buf[0] == '0' && buf[1] == 'x') {
		val = (u8)simple_strtoul(buf, NULL, 16);
	} else {
		val = (u8)simple_strtoul(buf, NULL, 10);
	}

	dio8015_err("dio8015 set_val = 0x%02x \n",val);
	if(val == 1){
		dio8015_show_reg_info(regmap);
		for (i = 0; i <= 0x0F; i++) {
			rc = dio8015_write(regmap, i, &rest_val, 1);
			if (rc < 0) {
				dio8015_err("dio8015 write addr 0x%x failed \n",i);
			}
		}
		dio8015_show_reg_info(regmap);
	}
	return len;
}


static DEVICE_ATTR(status, S_IWUSR|S_IRUSR, dio8015_show_status, NULL);
static DEVICE_ATTR(info, S_IWUSR|S_IRUSR, dio8015_show_info, NULL);
static DEVICE_ATTR(enable, S_IWUSR|S_IRUSR, dio8015_show_enable, dio8015_set_enable);
static DEVICE_ATTR(reset, S_IWUSR|S_IRUSR, NULL, dio8015_set_reset);


static int dio8015_driver_register(int index, struct regmap *regmap)
{
	int rc = 0;
	char device_drv_name[dio8015_NAME_STR_LEN_MAX] = { 0 };
	struct dio8015_char_dev dio8015_dev = dio8015_dev_list[index];
	snprintf(device_drv_name, dio8015_NAME_STR_LEN_MAX - 1,
		dio8015_NAME_FMT, index);

	dio8015_err("This is dio8015[%d] \n",index);
	dio8015_show_reg_info(regmap);


	/* Register char driver */
	if (alloc_chrdev_region(&(dio8015_dev.dev_no), 0, 1,
			device_drv_name)) {
		dio8015_debug("[dio8015] Allocate device no failed\n");
		return -EAGAIN;
	}

	/* Allocate driver */
	dio8015_dev.pcdev = cdev_alloc();
	if (dio8015_dev.pcdev == NULL) {
		unregister_chrdev_region(dio8015_dev.dev_no, 1);
		dio8015_debug("[dio8015] Allocate mem for kobject failed\n");
		return -ENOMEM;
	}

	/* Attatch file operation. */
	cdev_init(dio8015_dev.pcdev, &dio8015_file_operations);
	dio8015_dev.pcdev->owner = THIS_MODULE;

	/* Add to system */
	if (cdev_add(dio8015_dev.pcdev, dio8015_dev.dev_no, 1)) {
		dio8015_debug("Attatch file operation failed\n");
		unregister_chrdev_region(dio8015_dev.dev_no, 1);
		return -EAGAIN;
	}

	if (pdio8015_class == NULL) {
		pdio8015_class = class_create(dio8015_CLASS_NAME);
		if (IS_ERR(pdio8015_class)) {
			int ret = PTR_ERR(pdio8015_class);
			dio8015_debug("Unable to create class, err = %d\n", ret);
			return ret;
		}
	}

	dio8015_dev.pdevice = device_create(pdio8015_class, NULL,
			dio8015_dev.dev_no, NULL, device_drv_name);
	if (dio8015_dev.pdevice == NULL) {
		dio8015_debug("[dio8015] Allocate device_create for kobject failed\n");
		return -ENOMEM;
	}

	dio8015_dev.pdevice->driver_data = regmap;
	rc = sysfs_create_file(&(dio8015_dev.pdevice->kobj), &dev_attr_status.attr);
	rc = sysfs_create_file(&(dio8015_dev.pdevice->kobj), &dev_attr_info.attr);
	rc = sysfs_create_file(&(dio8015_dev.pdevice->kobj), &dev_attr_enable.attr);
	rc = sysfs_create_file(&(dio8015_dev.pdevice->kobj), &dev_attr_reset.attr);

	return 0;
}

static int dio8015_regulator_probe(struct i2c_client *client)
{
	int rc                = 0;
	unsigned int val      = 0xFF;
	struct regmap *regmap = NULL;
	int index = 0;

	rc = of_property_read_u32(client->dev.of_node, "index", &index);
	if (rc) {
		dio8015_err("failed to read index");
		return rc;
	}

	regmap = devm_regmap_init_i2c(client, &dio8015_regmap_config);
	if (IS_ERR(regmap)) {
		dio8015_err("dio8015 failed to allocate regmap\n");
		return PTR_ERR(regmap);
	}

	rc = regmap_read(regmap, 0x00, &val);
	if (rc < 0) {
		dio8015_err("dio8015 failed to get PID\n");
		return 0;
	}
	else
		dio8015_debug("dio8015 get Product ID: [%02x]\n", val);

	if (val != 0x04) {
		dio8015_debug("dio8015 get Product ID: [%02x],probe fail\n", val);
		return -ENODEV;
	}

	dio8015_driver_register(index, regmap);

	val = 0x0F;
	dio8015_write(regmap, dio8015_REG_DISCHARGE, (u8*)&val, 1);

	rc = dio8015_parse_regulator(regmap, &client->dev);
	if (rc < 0) {
		dio8015_err("dio8015 failed to parse device tree rc=%d\n", rc);
		return 0;
	}
	return 0;
}

static const struct of_device_id dio8015_dt_ids[] = {
	{
		.compatible = "dio,dio8015",
	},
	{
		.compatible = "willsemi,wl2866d",
	},
	{ }
};
MODULE_DEVICE_TABLE(of, dio8015_dt_ids);

static const struct i2c_device_id dio8015_id[] = {
	{
		.name = "dio8015-regulator",
		.driver_data = 0,
	},
	{ },
};
MODULE_DEVICE_TABLE(i2c, dio8015_id);

static struct i2c_driver dio8015_regulator_driver = {
	.driver = {
		.name = "dio8015-regulator",
		.of_match_table = of_match_ptr(dio8015_dt_ids),
	},
	.probe = dio8015_regulator_probe,
	.id_table = dio8015_id,
};

module_i2c_driver(dio8015_regulator_driver);
MODULE_LICENSE("GPL v2");

