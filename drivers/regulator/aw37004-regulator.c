// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2023 awinic. All Rights Reserved.
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


#define aw37004_err(message, ...) \
        pr_err("[%s] : "message, __func__, ##__VA_ARGS__)
#define aw37004_debug(message, ...) \
        pr_err("[%s] : "message, __func__, ##__VA_ARGS__)

#define AW37004_REG_ENABLE      0x0E
#define AW37004_REG_DISCHARGE   0x02
#define AW37004_REG_LDO0        0x03

#define LDO_VSET_REG(offset) ((offset) + AW37004_REG_LDO0)

#define VSET_DVDD_BASE_UV   600000
#define VSET_DVDD_STEP_UV   6000

#define VSET_AVDD_BASE_UV   1200000
#define VSET_AVDD_STEP_UV   12500

#define MAX_REG_NAME     20
#define AW37004_MAX_LDO  4

#define AW37004_CLASS_NAME       "camera_ldo"
#define AW37004_NAME_FMT         "aw37004%u"
#define AW37004_NAME_STR_LEN_MAX 50
#define AW37004_MAX_NUMBER       5

struct aw37004_char_dev {
	dev_t dev_no;
	struct cdev *pcdev;
	struct device *pdevice;
};

static struct class *paw37004_class = NULL;
static struct aw37004_char_dev aw37004_dev_list[AW37004_MAX_NUMBER];

struct aw37004_regulator{
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
	{ "dvdd1", "none", 1200000, 80000, 500000},
	{ "dvdd2", "none", 1200000, 80000, 500000},
	{ "avdd1", "none", 2800000, 90000, 300000},
	{ "avdd2", "none", 2800000, 90000, 300000},
};

static const struct regmap_config aw37004_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

/*common functions*/
static int aw37004_read(struct regmap *regmap, u16 reg, u8*val, int count)
{
	int rc;

	rc = regmap_bulk_read(regmap, reg, val, count);
	if (rc < 0)
		aw37004_err("failed to read 0x%04x\n", reg);
	return rc;
}

static int aw37004_write(struct regmap *regmap, u16 reg, u8*val, int count)
{
	int rc;
	u8 temp;

	aw37004_err("Writing 0x%02x to 0x%02x\n", *val, reg);
	rc = regmap_bulk_write(regmap, reg, val, count);
	if (rc < 0)
		aw37004_err("failed to write 0x%04x\n", reg);

	rc = aw37004_read(regmap, reg, &temp, count);

	aw37004_err("to write 0x%04x  exp 0x%x now 0x%x \n", reg,*val,temp);

	return rc;
}

static int aw37004_masked_write(struct regmap *regmap, u16 reg, u8 mask, u8 val)
{
	int rc;
	u8 temp = 0,new = 0;
	aw37004_debug("Writing 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);

	rc = aw37004_read(regmap, reg, &temp, 1);
	temp = 	temp & ~mask;
	temp |=  val & mask ;

	rc = regmap_update_bits(regmap, reg, mask, val);
	if (rc < 0)
		aw37004_err("failed to write 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);

	rc = aw37004_read(regmap, reg, &new, 1);

	aw37004_err("write 0x%04x  exp 0x%x now 0x%x \n", reg,temp,new);

	return rc;
}

void aw37004_show_reg_info(struct regmap *regmap)
{
	int rc;
	unsigned int val      = 0xFF;
	int i = 0;

	if(regmap != NULL){
		for (i = 0; i <= 0x0F; i++) {
			rc = regmap_read(regmap, i, &val);
			if (rc < 0) {
				aw37004_err("aw37004 failed to get value \n");
			}
			else {
				aw37004_err("aw37004 dump reg: addr 0x%x value 0x%x \n",i,val);
			}
		}
		aw37004_err("dump aw37004 success!");
	}else{
		aw37004_err("regmap == NULL ");
	}
}
EXPORT_SYMBOL(aw37004_show_reg_info);


static int aw37004_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct aw37004_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;
	u8 reg;

	rc = aw37004_read(fan_reg->regmap,
		AW37004_REG_ENABLE, &reg, 1);
	if (rc < 0) {
		aw37004_err("[%s] failed to read enable reg rc = %d\n", fan_reg->rdesc.name, rc);
		return rc;
	}
	return !!(reg & (1u << fan_reg->offset));
}

static int aw37004_regulator_enable(struct regulator_dev *rdev)
{
	struct aw37004_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	if (fan_reg->parent_supply) {
		rc = regulator_enable(fan_reg->parent_supply);
		if (rc < 0) {
			aw37004_err("[%s] failed to enable parent rc=%d\n", fan_reg->rdesc.name, rc);
			return rc;
		}
	}

	rc = aw37004_masked_write(fan_reg->regmap,
		AW37004_REG_ENABLE,
		1u << fan_reg->offset, 1u << fan_reg->offset);
	if (rc < 0) {
		aw37004_err("[%s] failed to enable regulator rc=%d\n", fan_reg->rdesc.name, rc);
		goto remove_vote;
	}
	aw37004_debug("[%s][%d] regulator enable\n", fan_reg->rdesc.name, fan_reg->index);
	return 0;

remove_vote:
	if (fan_reg->parent_supply)
		rc = regulator_disable(fan_reg->parent_supply);
	if (rc < 0)
		aw37004_err("[%s] failed to disable parent regulator rc=%d\n", fan_reg->rdesc.name, rc);
	return -ETIME;
}

static int aw37004_regulator_disable(struct regulator_dev *rdev)
{
	struct aw37004_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	rc = aw37004_masked_write(fan_reg->regmap,
		AW37004_REG_ENABLE,
		1u << fan_reg->offset, 0);

	if (rc < 0) {
		aw37004_err("[%s] failed to disable regulator rc=%d\n", fan_reg->rdesc.name,rc);
		return rc;
	}

	/*remove voltage vot from parent regulator */
	if (fan_reg->parent_supply) {
		rc = regulator_set_voltage(fan_reg->parent_supply, 0, INT_MAX);
		if (rc < 0) {
			aw37004_err("[%s] failed to remove parent voltage rc=%d\n", fan_reg->rdesc.name,rc);
			return rc;
		}
		rc = regulator_disable(fan_reg->parent_supply);
		if (rc < 0) {
			aw37004_err("[%s] failed to disable parent rc=%d\n", fan_reg->rdesc.name, rc);
			return rc;
		}
	}

	aw37004_debug("[%s][%d] regulator disabled\n", fan_reg->rdesc.name, fan_reg->index);
	return 0;
}

static int aw37004_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct aw37004_regulator *fan_reg = rdev_get_drvdata(rdev);
	u8  vset = 0;
	int rc   = 0;
	int uv   = 0;

	rc = aw37004_read(fan_reg->regmap, LDO_VSET_REG(fan_reg->offset),
	&vset, 1);
	if (rc < 0) {
		aw37004_err("[%s] failed to read regulator voltage rc = %d\n", fan_reg->rdesc.name,rc);
		return rc;
	}

	if (vset == 0) {
		uv = reg_data[fan_reg->offset].default_uv;
	} else {
		aw37004_debug("[%s][%d] voltage read [%x]\n", fan_reg->rdesc.name, fan_reg->index, vset);
		if (fan_reg->offset == 0 || fan_reg->offset == 1)
			uv = VSET_DVDD_BASE_UV + vset * VSET_DVDD_STEP_UV; //DVDD
		else
			uv = VSET_AVDD_BASE_UV + vset * VSET_AVDD_STEP_UV; //AVDD
	}
	return uv;
}

static int aw37004_write_voltage(struct aw37004_regulator* fan_reg, int min_uv,
	int max_uv)
{
	int rc   = 0;
	u8  vset = 0;

	if (min_uv > max_uv) {
		aw37004_err("[%s] requestd voltage above maximum limit\n", fan_reg->rdesc.name);
		return -EINVAL;
	}

	if (fan_reg->offset == 0 || fan_reg->offset == 1)
		vset = DIV_ROUND_UP(min_uv - VSET_DVDD_BASE_UV, VSET_DVDD_STEP_UV); //DVDD
	else
		vset = DIV_ROUND_UP(min_uv - VSET_AVDD_BASE_UV, VSET_AVDD_STEP_UV); //AVDD

	rc = aw37004_write(fan_reg->regmap, LDO_VSET_REG(fan_reg->offset),
		&vset, 1);
	if (rc < 0) {
		aw37004_err("[%s] failed to write voltage rc = %d\n", fan_reg->rdesc.name,rc);
		return rc;
	}

	aw37004_debug("[%s][%d] VSET=[0x%2x]\n", fan_reg->rdesc.name, fan_reg->index, vset);
	return 0;
}

static int aw37004_regulator_set_voltage(struct regulator_dev *rdev,
	int min_uv, int max_uv, unsigned int* selector)
{
	struct aw37004_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	if (fan_reg->parent_supply) {
		rc = regulator_set_voltage(fan_reg->parent_supply,
			fan_reg->min_dropout_uv + min_uv,
			INT_MAX);
		if (rc < 0) {
			aw37004_err("[%s] failed to request parent supply voltage rc=%d\n", fan_reg->rdesc.name,rc);
			return rc;
		}
	}

	rc = aw37004_write_voltage(fan_reg, min_uv, max_uv);
	if (rc < 0) {
		/* remove parentn's voltage vote */
		if (fan_reg->parent_supply)
			regulator_set_voltage(fan_reg->parent_supply, 0, INT_MAX);
	}
	aw37004_debug("[%s][%d] voltage set to %d\n", fan_reg->rdesc.name, fan_reg->index, min_uv);
	return rc;
}

static struct regulator_ops aw37004_regulator_ops = {
	.enable      = aw37004_regulator_enable,
	.disable     = aw37004_regulator_disable,
	.is_enabled  = aw37004_regulator_is_enabled,
	.set_voltage = aw37004_regulator_set_voltage,
	.get_voltage = aw37004_regulator_get_voltage,
};

static int aw37004_register_ldo(struct aw37004_regulator *aw37004_reg,
	const char *name)
{
	struct regulator_config reg_config = {};
	struct regulator_init_data *init_data;

	struct device_node *reg_node = aw37004_reg->of_node;
	struct device *dev           = aw37004_reg->dev;
	int rc, i, init_voltage;
	char buff[MAX_REG_NAME];

	/* try to find ldo pre-defined in the regulator table */
	for (i = 0; i< AW37004_MAX_LDO; i++) {
		if (!strcmp(reg_data[i].name, name))
			break;
	}

	if (i == AW37004_MAX_LDO) {
		aw37004_err("Invalid regulator name %s\n", name);
		return -EINVAL;
	}

	rc = of_property_read_u16(reg_node, "offset", &aw37004_reg->offset);
	if (rc < 0) {
		aw37004_err("%s:failed to get regulator offset rc = %d\n", name, rc);
		return rc;
	}

	//assign default value defined in code.
	aw37004_reg->min_dropout_uv = reg_data[i].min_dropout_uv;
	of_property_read_u32(reg_node, "min-dropout-voltage",
		&aw37004_reg->min_dropout_uv);

	aw37004_reg->iout_ua = reg_data[i].iout_ua;
	of_property_read_u32(reg_node, "iout_ua",
		&aw37004_reg->iout_ua);

	init_voltage = -EINVAL;
	of_property_read_u32(reg_node, "init-voltage", &init_voltage);

	scnprintf(buff, MAX_REG_NAME, "%s-supply", reg_data[i].supply_name);
	if (of_find_property(dev->of_node, buff, NULL)) {
		aw37004_reg->parent_supply = devm_regulator_get(dev,
			reg_data[i].supply_name);
		if (IS_ERR(aw37004_reg->parent_supply)) {
			rc = PTR_ERR(aw37004_reg->parent_supply);
			if (rc != EPROBE_DEFER)
				aw37004_err("%s: failed to get parent regulator rc = %d\n",
					name, rc);
				return rc;
		}
	}

	init_data = of_get_regulator_init_data(dev, reg_node, &aw37004_reg->rdesc);
	if (init_data == NULL) {
		aw37004_err("%s: failed to get regulator data\n", name);
		return -ENODATA;
	}


	if (!init_data->constraints.name) {
		aw37004_err("%s: regulator name missing\n", name);
		return -EINVAL;
	}

	/* configure the initial voltage for the regulator */
	if (init_voltage > 0) {
		rc = aw37004_write_voltage(aw37004_reg, init_voltage,
			init_data->constraints.max_uV);
		if (rc < 0)
			aw37004_err("%s:failed to set initial voltage rc = %d\n", name, rc);
	}

	init_data->constraints.input_uV = init_data->constraints.max_uV;
	init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_STATUS
		| REGULATOR_CHANGE_VOLTAGE;

	reg_config.dev         = dev;
	reg_config.init_data   = init_data;
	reg_config.driver_data = aw37004_reg;
	reg_config.of_node     = reg_node;

	aw37004_reg->rdesc.owner      = THIS_MODULE;
	aw37004_reg->rdesc.type       = REGULATOR_VOLTAGE;
	aw37004_reg->rdesc.ops        = &aw37004_regulator_ops;
	aw37004_reg->rdesc.name       = init_data->constraints.name;
	aw37004_reg->rdesc.n_voltages = 1;

	aw37004_debug("try to register ldo %s\n", name);
	aw37004_reg->rdev = devm_regulator_register(dev, &aw37004_reg->rdesc,
		&reg_config);
	if (IS_ERR(aw37004_reg->rdev)) {
		rc = PTR_ERR(aw37004_reg->rdev);
		aw37004_err("%s: failed to register regulator rc =%d\n",
		aw37004_reg->rdesc.name, rc);
		return rc;
	}

	aw37004_debug("%s regulator register done\n", name);
	return 0;
}

static int aw37004_parse_regulator(struct regmap *regmap, struct device *dev)
{
	int rc           = 0;
	int index        = 0;
	const char *name = NULL;
	struct device_node *child             = NULL;
	struct aw37004_regulator *aw37004_reg = NULL;

	of_property_read_u32(dev->of_node, "index", &index);

	/* parse each regulator */
	for_each_available_child_of_node(dev->of_node, child) {
		aw37004_reg = devm_kzalloc(dev, sizeof(*aw37004_reg), GFP_KERNEL);
		if (!aw37004_reg)
			return -ENOMEM;

		aw37004_reg->regmap  = regmap;
		aw37004_reg->of_node = child;
		aw37004_reg->dev     = dev;
		aw37004_reg->index   = index;
		aw37004_reg->parent_supply = NULL;

		rc = of_property_read_string(child, "regulator-name", &name);
		if (rc)
			continue;

		rc = aw37004_register_ldo(aw37004_reg, name);
		if (rc <0 ) {
			aw37004_err("failed to register regulator %s rc = %d\n", name, rc);
			return rc;
		}
	}
	return 0;
}

static int aw37004_open(struct inode *a_inode, struct file *a_file)
{
	return 0;
}

static int aw37004_release(struct inode *a_inode, struct file *a_file)
{
	return 0;
}

static long aw37004_ioctl(struct file *a_file, unsigned int a_cmd,
			 unsigned long a_param)
{
	return 0;
}

static const struct file_operations aw37004_file_operations = {
	.owner          = THIS_MODULE,
	.open           = aw37004_open,
	.release        = aw37004_release,
	.unlocked_ioctl = aw37004_ioctl,
};

static ssize_t aw37004_show_status(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct regmap *regmap = (struct regmap *)dev->driver_data;
	int rc;
	unsigned int val      = 0xFF;
	unsigned int len      = 0;

	rc = regmap_read(regmap, 0x00, &val);
	if (rc < 0) {
		aw37004_err("AW37004 failed to get PID\n");
		len = sprintf(buf, "fail\n");
	}
	else {
		aw37004_debug("AW37004 get Product ID: [%02x]\n", val);
		len = sprintf(buf, "success\n");
	}

	return len;
}

static ssize_t aw37004_show_info(struct device *dev,
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


static ssize_t aw37004_show_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct regmap *regmap = (struct regmap *)dev->driver_data;
	int rc;
	unsigned int val      = 0xFF;
	unsigned int len      = 0;

	rc = regmap_read(regmap, AW37004_REG_ENABLE, &val);
	if (rc < 0) {
		len = sprintf(buf, "read 0x0E ==> fail\n");
	}
	else {
		len = sprintf(buf, "read 0x0E ==> 0x%x\n", val);
	}

	return len;
}

static ssize_t aw37004_set_enable(struct device *dev,
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
	aw37004_write(regmap, AW37004_REG_ENABLE, &val, 1);

	return len;
}

static ssize_t aw37004_set_reset(struct device *dev,
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

	aw37004_err("aw37004 set_val = 0x%02x \n",val);
	if(val == 1){
		aw37004_show_reg_info(regmap);
		for (i = 0; i <= 0x0F; i++) {
			rc = aw37004_write(regmap, i, &rest_val, 1);
			if (rc < 0) {
				aw37004_err("aw37004 write addr 0x%x failed \n",i);
			}
		}
		aw37004_show_reg_info(regmap);
	}
	return len;
}


static DEVICE_ATTR(status, S_IWUSR|S_IRUSR, aw37004_show_status, NULL);
static DEVICE_ATTR(info, S_IWUSR|S_IRUSR, aw37004_show_info, NULL);
static DEVICE_ATTR(enable, S_IWUSR|S_IRUSR, aw37004_show_enable, aw37004_set_enable);
static DEVICE_ATTR(reset, S_IWUSR|S_IRUSR, NULL, aw37004_set_reset);


static int aw37004_driver_register(int index, struct regmap *regmap)
{
	u8 rest_val= 0;
	int i = 0, rc = 0;
	char device_drv_name[AW37004_NAME_STR_LEN_MAX] = { 0 };
	struct aw37004_char_dev aw37004_dev = aw37004_dev_list[index];
	snprintf(device_drv_name, AW37004_NAME_STR_LEN_MAX - 1,
		AW37004_NAME_FMT, index);

	aw37004_err("This is aw37004[%d] \n",index);
	aw37004_show_reg_info(regmap);
	for (i = 0; i <= 0x0F; i++) {
		rc = aw37004_write(regmap, i, &rest_val, 1);
		if (rc < 0) {
			aw37004_err("aw37004 write addr 0x%x failed \n",i);
		}
	}
	aw37004_show_reg_info(regmap);


	/* Register char driver */
	if (alloc_chrdev_region(&(aw37004_dev.dev_no), 0, 1,
			device_drv_name)) {
		aw37004_debug("[AW37004] Allocate device no failed\n");
		return -EAGAIN;
	}

	/* Allocate driver */
	aw37004_dev.pcdev = cdev_alloc();
	if (aw37004_dev.pcdev == NULL) {
		unregister_chrdev_region(aw37004_dev.dev_no, 1);
		aw37004_debug("[AW37004] Allocate mem for kobject failed\n");
		return -ENOMEM;
	}

	/* Attatch file operation. */
	cdev_init(aw37004_dev.pcdev, &aw37004_file_operations);
	aw37004_dev.pcdev->owner = THIS_MODULE;

	/* Add to system */
	if (cdev_add(aw37004_dev.pcdev, aw37004_dev.dev_no, 1)) {
		aw37004_debug("Attatch file operation failed\n");
		unregister_chrdev_region(aw37004_dev.dev_no, 1);
		return -EAGAIN;
	}

	if (paw37004_class == NULL) {
		paw37004_class = class_create(AW37004_CLASS_NAME);
		if (IS_ERR(paw37004_class)) {
			int ret = PTR_ERR(paw37004_class);
			aw37004_debug("Unable to create class, err = %d\n", ret);
			return ret;
		}
	}

	aw37004_dev.pdevice = device_create(paw37004_class, NULL,
			aw37004_dev.dev_no, NULL, device_drv_name);
	if (aw37004_dev.pdevice == NULL) {
		aw37004_debug("[AW37004] Allocate device_create for kobject failed\n");
		return -ENOMEM;
	}

	aw37004_dev.pdevice->driver_data = regmap;
	rc = sysfs_create_file(&(aw37004_dev.pdevice->kobj), &dev_attr_status.attr);
	rc = sysfs_create_file(&(aw37004_dev.pdevice->kobj), &dev_attr_info.attr);
	rc = sysfs_create_file(&(aw37004_dev.pdevice->kobj), &dev_attr_enable.attr);
	rc = sysfs_create_file(&(aw37004_dev.pdevice->kobj), &dev_attr_reset.attr);

	return 0;
}

static int aw37004_regulator_probe(struct i2c_client *client)
{
	int rc                = 0;
	unsigned int val      = 0xFF;
	struct regmap *regmap = NULL;
	int index = 0;

	rc = of_property_read_u32(client->dev.of_node, "index", &index);
	if (rc) {
		aw37004_err("failed to read index");
		return rc;
	}

	regmap = devm_regmap_init_i2c(client, &aw37004_regmap_config);
	if (IS_ERR(regmap)) {
		aw37004_err("AW37004 failed to allocate regmap\n");
		return PTR_ERR(regmap);
	}

	rc = regmap_read(regmap, 0x00, &val);
	if (rc < 0) {
		aw37004_err("AW37004 failed to get PID\n");
		return 0;
	}
	else
		aw37004_debug("AW37004 get Product ID: [%02x]\n", val);

	if (val != 0x00) {
		aw37004_debug("AW37004 get Product ID: [%02x],probe fail\n", val);
		return -ENODEV;
	}

	aw37004_driver_register(index, regmap);

	val = 0xFF;
	aw37004_write(regmap, AW37004_REG_DISCHARGE, (u8*)&val, 1);

	rc = aw37004_parse_regulator(regmap, &client->dev);
	if (rc < 0) {
		aw37004_err("AW37004 failed to parse device tree rc=%d\n", rc);
		return 0;
	}
	return 0;
}

static const struct of_device_id aw37004_dt_ids[] = {
	{
		.compatible = "willsemi,aw37004",
	},
	{ }
};
MODULE_DEVICE_TABLE(of, aw37004_dt_ids);

static const struct i2c_device_id aw37004_id[] = {
	{
		.name = "aw37004-regulator",
		.driver_data = 0,
	},
	{ },
};
MODULE_DEVICE_TABLE(i2c, aw37004_id);

static struct i2c_driver aw37004_regulator_driver = {
	.driver = {
		.name = "aw37004-regulator",
		.of_match_table = of_match_ptr(aw37004_dt_ids),
	},
	.probe = aw37004_regulator_probe,
	.id_table = aw37004_id,
};

module_i2c_driver(aw37004_regulator_driver);
MODULE_LICENSE("GPL v2");

