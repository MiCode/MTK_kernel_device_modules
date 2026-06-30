/*
 * WARHOL_WL2868C ON Semiconductor LDO PMIC Driver.
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


#define warhol_wl2868c_err(message, ...) \
        pr_err("[%s] : "message, __func__, ##__VA_ARGS__)
#define warhol_wl2868c_debug(message, ...) \
        pr_err("[%s] : "message, __func__, ##__VA_ARGS__)



#define WARHOL_WL2868C_SLAVE_ADDR      0x5E

#define WARHOL_WL2868C_REG_ENABLE      0x0E
#define WARHOL_WL2868C_REG_LDO0        0x03

#define LDO_VSET_REG(offset) ((offset) + WARHOL_WL2868C_REG_LDO0)

#define MAX_REG_NAME     20
#define WARHOL_WL2868C_MAX_LDO  7

#define WARHOL_WL2868C_CLASS_NAME       "camera_ldo_warhol_wl2868c"
#define WARHOL_WL2868C_NAME_FMT         "wl2868c%u"
#define WARHOL_WL2868C_NAME_STR_LEN_MAX 50
#define WARHOL_WL2868C_MAX_NUMBER       5
#define MV_PER_V               1000

#define volt2regval(wldo_volt_addr, volt)                  \
		(wldo_volt_addr < 2 ?                              \
		(volt - 496 * MV_PER_V) / (8 * MV_PER_V)           \
		: (volt - 1504 * MV_PER_V) / (8 * MV_PER_V))       \

#define regval2volt(wldo_volt_addr, regval)                \
		(wldo_volt_addr < 2 ?                              \
		496 * MV_PER_V + 8 * MV_PER_V * regval             \
		: 1504 * MV_PER_V + 8 * MV_PER_V * regval)         \

struct warhol_wl2868c_char_dev {
	dev_t dev_no;
	struct cdev *pcdev;
	struct device *pdevice;
};

static struct class *pwarhol_wl2868c_class = NULL;
static struct warhol_wl2868c_char_dev warhol_wl2868c_dev_list[WARHOL_WL2868C_MAX_NUMBER];

struct warhol_wl2868c_regulator{
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
	{ "warhol_wl2868c-l1", "warhol_wl2868c-l1", 1200000, 80000, 800000},
	{ "warhol_wl2868c-l2", "warhol_wl2868c-l2", 1200000, 80000, 800000},
	{ "warhol_wl2868c-l3", "warhol_wl2868c-l3", 2800000, 80000, 800000},
	{ "warhol_wl2868c-l4", "warhol_wl2868c-l4", 2800000, 80000, 800000},
	{ "warhol_wl2868c-l5", "warhol_wl2868c-l5", 2800000, 80000, 800000},
	{ "warhol_wl2868c-l6", "warhol_wl2868c-l6", 2800000, 80000, 800000},
	{ "warhol_wl2868c-l7", "warhol_wl2868c-l7", 2800000, 80000, 800000},
};

static const struct regmap_config warhol_wl2868c_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

/*common functions*/
static int warhol_wl2868c_read(struct regmap *regmap, u16 reg, u8 *val, int count)
{
	int rc;

	rc = regmap_bulk_read(regmap, reg, val, count);
	if (rc < 0)
		warhol_wl2868c_err("failed to read 0x%04x\n", reg);
	return rc;
}

static int warhol_wl2868c_write(struct regmap *regmap, u16 reg, u8*val, int count)
{
	int rc;

	warhol_wl2868c_debug("Writing 0x%02x to 0x%02x\n", *val, reg);
	rc = regmap_bulk_write(regmap, reg, val, count);
	if (rc < 0)
		warhol_wl2868c_err("failed to write 0x%04x\n", reg);

	return rc;
}

static int warhol_wl2868c_masked_write(struct regmap *regmap, u16 reg, u8 mask, u8 val)
{
	int rc;
	warhol_wl2868c_debug("Writing 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);
	rc = regmap_update_bits(regmap, reg, mask, val);
	if (rc < 0)
		warhol_wl2868c_err("failed to write 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);
	return rc;
}

static int warhol_wl2868c_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct warhol_wl2868c_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;
	u8 reg;

	rc = warhol_wl2868c_read(fan_reg->regmap,
		WARHOL_WL2868C_REG_ENABLE, &reg, 1);
	if (rc < 0) {
		warhol_wl2868c_err("[%s] failed to read enable reg rc = %d\n", fan_reg->rdesc.name, rc);
		return rc;
	}
	return !!(reg & (1u << fan_reg->offset));
}

static int warhol_wl2868c_regulator_enable(struct regulator_dev *rdev)
{
	struct warhol_wl2868c_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	if (fan_reg->parent_supply) {
		rc = regulator_enable(fan_reg->parent_supply);
		if (rc < 0) {
			warhol_wl2868c_err("[%s] failed to enable parent rc=%d\n", fan_reg->rdesc.name, rc);
			return rc;
		}
	}
	warhol_wl2868c_err("fan_reg->offset is(begin)  == %d\n" ,fan_reg->offset);
	rc = warhol_wl2868c_masked_write(fan_reg->regmap,
		WARHOL_WL2868C_REG_ENABLE,
		1u << fan_reg->offset, 1u << fan_reg->offset);
	warhol_wl2868c_err("fan_reg->offset is(end)  == %d\n" ,fan_reg->offset);
	if (rc < 0) {
		warhol_wl2868c_err("[%s] failed to enable regulator rc=%d\n", fan_reg->rdesc.name, rc);
		goto remove_vote;
	}
	warhol_wl2868c_debug("[%s][%d] regulator enable\n", fan_reg->rdesc.name, fan_reg->index);
	return 0;

remove_vote:
	// if (fan_reg->parent_supply)
	// 	rc = regulator_disable(fan_reg->parent_supply);
	// if (rc < 0)
	// 	wl2868c_err("[%s] failed to disable parent regulator rc=%d\n", fan_reg->rdesc.name, rc);
	return -ETIME;
}

static int warhol_wl2868c_regulator_disable(struct regulator_dev *rdev)
{
	struct warhol_wl2868c_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	rc = warhol_wl2868c_masked_write(fan_reg->regmap,
		WARHOL_WL2868C_REG_ENABLE,
		1u << fan_reg->offset, 0);

	if (rc < 0) {
		warhol_wl2868c_err("[%s] failed to disable regulator rc=%d\n", fan_reg->rdesc.name,rc);
		return rc;
	}

//	/*remove voltage vot from parent regulator */
//	if (fan_reg->parent_supply) {
//		rc = regulator_set_voltage(fan_reg->parent_supply, 0, INT_MAX);
//		if (rc < 0) {
//			warhol_wl2868c_err("[%s] failed to remove parent voltage rc=%d\n", fan_reg->rdesc.name,rc);
//			return rc;
//		}
//		rc = regulator_disable(fan_reg->parent_supply);
//		if (rc < 0) {
//			warhol_wl2868c_err("[%s] failed to disable parent rc=%d\n", fan_reg->rdesc.name, rc);
//			return rc;
//		}
//	}

	warhol_wl2868c_debug("[%s][%d] regulator disabled\n", fan_reg->rdesc.name, fan_reg->index);
	return 0;
}

static int warhol_wl2868c_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct warhol_wl2868c_regulator *fan_reg = rdev_get_drvdata(rdev);
	u8  vset = 0;
	int rc   = 0;
	int uv   = 0;

	rc = warhol_wl2868c_read(fan_reg->regmap, LDO_VSET_REG(fan_reg->offset),
	&vset, 1);
	if (rc < 0) {
		warhol_wl2868c_err("[%s] failed to read regulator voltage rc = %d\n", fan_reg->rdesc.name,rc);
		return rc;
	}

	if (vset == 0) {
		uv = reg_data[fan_reg->offset].default_uv;
	} else {
		warhol_wl2868c_debug("[%s][%d] voltage read [%x]\n", fan_reg->rdesc.name, fan_reg->index, vset);

		uv = regval2volt(fan_reg->offset, vset);
	}
	return uv;
}

static int warhol_wl2868c_write_voltage(struct warhol_wl2868c_regulator* fan_reg, int min_uv,
	int max_uv)
{
	int rc   = 0;
	u8  vset = 0;

	warhol_wl2868c_err("write_min_uv ==  %d, write_max_uv == %d\n", min_uv, max_uv);

	if (min_uv > max_uv) {
		warhol_wl2868c_err("[%s] requestd voltage above maximum limit\n", fan_reg->rdesc.name);
		return -EINVAL;
	}

	vset = volt2regval(fan_reg->offset, min_uv);

	warhol_wl2868c_err("fan_reg->offset ==  %d, min_uv == %d, VSET=[0x%2x]\n", fan_reg->offset, min_uv, vset);

	rc = warhol_wl2868c_write(fan_reg->regmap, LDO_VSET_REG(fan_reg->offset),
		&vset, 1);
	if (rc < 0) {
		warhol_wl2868c_err("[%s] failed to write voltage rc = %d\n", fan_reg->rdesc.name,rc);
		return rc;
	}

	warhol_wl2868c_debug("[%s][%d] VSET=[0x%2x]\n", fan_reg->rdesc.name, fan_reg->index, vset);
	return 0;
}

static int warhol_wl2868c_regulator_set_voltage(struct regulator_dev *rdev,
	int min_uv, int max_uv, unsigned int* selector)
{
	struct warhol_wl2868c_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

//	if (fan_reg->parent_supply) {
//		rc = regulator_set_voltage(fan_reg->parent_supply,
//			fan_reg->min_dropout_uv + min_uv,
//			INT_MAX);
//		if (rc < 0) {
//			warhol_wl2868c_err("[%s] failed to request parent supply voltage rc=%d\n", fan_reg->rdesc.name,rc);
//			return rc;
//		}
//	}

	rc = warhol_wl2868c_write_voltage(fan_reg, min_uv, max_uv);
//	if (rc < 0) {
//		/* remove parentn's voltage vote */
//		if (fan_reg->parent_supply)
//			regulator_set_voltage(fan_reg->parent_supply, 0, INT_MAX);
//	}
	warhol_wl2868c_debug("[%s][%d] voltage set to %d\n", fan_reg->rdesc.name, fan_reg->index, min_uv);
	return rc;
}

static struct regulator_ops warhol_wl2868c_regulator_ops = {
	.enable      = warhol_wl2868c_regulator_enable,
	.disable     = warhol_wl2868c_regulator_disable,
	.is_enabled  = warhol_wl2868c_regulator_is_enabled,
	.set_voltage = warhol_wl2868c_regulator_set_voltage,
	.get_voltage = warhol_wl2868c_regulator_get_voltage,
};

static int warhol_wl2868c_register_ldo(struct warhol_wl2868c_regulator *warhol_wl2868c_reg,
	const char *name)
{
	struct regulator_config reg_config = {};
	struct regulator_init_data *init_data;

	struct device_node *reg_node = warhol_wl2868c_reg->of_node;
	struct device *dev           = warhol_wl2868c_reg->dev;
	int rc, i, init_voltage;
	char buff[MAX_REG_NAME];

	/* try to find ldo pre-defined in the regulator table */
	for (i = 0; i< WARHOL_WL2868C_MAX_LDO; i++) {
		if (!strcmp(reg_data[i].name, name))
			break;
	}

	if (i == WARHOL_WL2868C_MAX_LDO) {
		warhol_wl2868c_err("Invalid regulator name %s\n", name);
		return -EINVAL;
	}

	rc = of_property_read_u16(reg_node, "offset", &warhol_wl2868c_reg->offset);
	if (rc < 0) {
		warhol_wl2868c_err("%s:failed to get regulator offset rc = %d\n", name, rc);
		return rc;
	}

	//assign default value defined in code.
	warhol_wl2868c_reg->min_dropout_uv = reg_data[i].min_dropout_uv;
	of_property_read_u32(reg_node, "min-dropout-voltage",
		&warhol_wl2868c_reg->min_dropout_uv);

	warhol_wl2868c_reg->iout_ua = reg_data[i].iout_ua;
	of_property_read_u32(reg_node, "iout_ua",
		&warhol_wl2868c_reg->iout_ua);

	init_voltage = -EINVAL;
	of_property_read_u32(reg_node, "init-voltage", &init_voltage);

	scnprintf(buff, MAX_REG_NAME, "%s-supply", reg_data[i].supply_name);
	if (of_find_property(dev->of_node, buff, NULL)) {
		warhol_wl2868c_reg->parent_supply = devm_regulator_get(dev,
			reg_data[i].supply_name);
		if (IS_ERR(warhol_wl2868c_reg->parent_supply)) {
			rc = PTR_ERR(warhol_wl2868c_reg->parent_supply);
			if (rc != EPROBE_DEFER)
				warhol_wl2868c_err("%s: failed to get parent regulator rc = %d\n",
					name, rc);
				return rc;
		}
	}

	init_data = of_get_regulator_init_data(dev, reg_node, &warhol_wl2868c_reg->rdesc);
	if (init_data == NULL) {
		warhol_wl2868c_err("%s: failed to get regulator data\n", name);
		return -ENODATA;
	}


	if (!init_data->constraints.name) {
		warhol_wl2868c_err("%s: regulator name missing\n", name);
		return -EINVAL;
	}

	/* configure the initial voltage for the regulator */
	if (init_voltage > 0) {
		rc = warhol_wl2868c_write_voltage(warhol_wl2868c_reg, init_voltage,
			init_data->constraints.max_uV);
		if (rc < 0)
			warhol_wl2868c_err("%s:failed to set initial voltage rc = %d\n", name, rc);
	}

	init_data->constraints.input_uV = init_data->constraints.max_uV;
	init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_STATUS
		| REGULATOR_CHANGE_VOLTAGE;

	reg_config.dev         = dev;
	reg_config.init_data   = init_data;
	reg_config.driver_data = warhol_wl2868c_reg;
	reg_config.of_node     = reg_node;

	warhol_wl2868c_reg->rdesc.owner      = THIS_MODULE;
	warhol_wl2868c_reg->rdesc.type       = REGULATOR_VOLTAGE;
	warhol_wl2868c_reg->rdesc.ops        = &warhol_wl2868c_regulator_ops;
	warhol_wl2868c_reg->rdesc.name       = init_data->constraints.name;
	warhol_wl2868c_reg->rdesc.n_voltages = 1;

	warhol_wl2868c_debug("try to register ldo %s\n", name);

	warhol_wl2868c_reg->rdev = devm_regulator_register(dev, &warhol_wl2868c_reg->rdesc,
		&reg_config);
	if (IS_ERR(warhol_wl2868c_reg->rdev)) {
		rc = PTR_ERR(warhol_wl2868c_reg->rdev);
		warhol_wl2868c_err("%s: failed to register regulator rc =%d\n",
		warhol_wl2868c_reg->rdesc.name, rc);
		return rc;
	}

	warhol_wl2868c_debug("%s regulator register done\n", name);
	return 0;
}

static int warhol_wl2868c_parse_regulator(struct regmap *regmap, struct device *dev)
{
	int rc           = 0;
	int index        = 0;
	const char *name = NULL;
	struct device_node *child             = NULL;
	struct warhol_wl2868c_regulator *warhol_wl2868c_reg = NULL;

	of_property_read_u32(dev->of_node, "index", &index);

	/* parse each regulator */
	for_each_available_child_of_node(dev->of_node, child) {
		warhol_wl2868c_reg = devm_kzalloc(dev, sizeof(*warhol_wl2868c_reg), GFP_KERNEL);
		if (!warhol_wl2868c_reg)
			return -ENOMEM;

		warhol_wl2868c_reg->regmap  = regmap;
		warhol_wl2868c_reg->of_node = child;
		warhol_wl2868c_reg->dev     = dev;
		warhol_wl2868c_reg->index   = index;
		warhol_wl2868c_reg->parent_supply = NULL;

		rc = of_property_read_string(child, "regulator-name", &name);
		if (rc)
			continue;

		rc = warhol_wl2868c_register_ldo(warhol_wl2868c_reg, name);
		if (rc <0 ) {
			warhol_wl2868c_err("failed to register regulator %s rc = %d\n", name, rc);
			return rc;
		}
	}
	return 0;
}

static int warhol_wl2868c_open(struct inode *a_inode, struct file *a_file)
{
	return 0;
}

static int warhol_wl2868c_release(struct inode *a_inode, struct file *a_file)
{
	return 0;
}

static long warhol_wl2868c_ioctl(struct file *a_file, unsigned int a_cmd,
			 unsigned long a_param)
{
	return 0;
}

static const struct file_operations warhol_wl2868c_file_operations = {
	.owner          = THIS_MODULE,
	.open           = warhol_wl2868c_open,
	.release        = warhol_wl2868c_release,
	.unlocked_ioctl = warhol_wl2868c_ioctl,
};

static ssize_t warhol_wl2868c_show_status(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct regmap *regmap = (struct regmap *)dev->driver_data;
	int rc;
	unsigned int val      = 0xFF;
	unsigned int len      = 0;

	rc = regmap_read(regmap, 0x00, &val);
	if (rc < 0) {
		warhol_wl2868c_err("WARHOL_WL2868C failed to get PID\n");
		len = sprintf(buf, "fail\n");
	}
	else {
		warhol_wl2868c_debug("WARHOL_WL2868C get Product ID: [%02x]\n", val);
		len = sprintf(buf, "success\n");
	}

	return len;
}

static ssize_t warhol_wl2868c_show_info(struct device *dev,
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

static ssize_t warhol_wl2868c_show_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct regmap *regmap = (struct regmap *)dev->driver_data;
	int rc;
	unsigned int val      = 0xFF;
	unsigned int len      = 0;

	rc = regmap_read(regmap, WARHOL_WL2868C_REG_ENABLE, &val);
	if (rc < 0) {
		len = sprintf(buf, "read 0x03 ==> fail\n");
	}
	else {
		len = sprintf(buf, "read 0x03 ==> 0x%x\n", val);
	}

	return len;
}

static ssize_t warhol_wl2868c_set_enable(struct device *dev,
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
	warhol_wl2868c_write(regmap, WARHOL_WL2868C_REG_ENABLE, &val, 1);

	return len;
}

static DEVICE_ATTR(status, S_IWUSR|S_IRUSR, warhol_wl2868c_show_status, NULL);
static DEVICE_ATTR(info, S_IWUSR|S_IRUSR, warhol_wl2868c_show_info, NULL);
static DEVICE_ATTR(enable, S_IWUSR|S_IRUSR, warhol_wl2868c_show_enable, warhol_wl2868c_set_enable);

static int warhol_wl2868c_driver_register(int index, struct regmap *regmap)
{
	unsigned long ret;
	char device_drv_name[WARHOL_WL2868C_NAME_STR_LEN_MAX] = { 0 };
	struct warhol_wl2868c_char_dev warhol_wl2868c_dev = warhol_wl2868c_dev_list[index];

	snprintf(device_drv_name, WARHOL_WL2868C_NAME_STR_LEN_MAX - 1,
		WARHOL_WL2868C_NAME_FMT, index);

	/* Register char driver */
	if (alloc_chrdev_region(&(warhol_wl2868c_dev.dev_no), 0, 1,
			device_drv_name)) {
		warhol_wl2868c_debug("[WARHOL_WL2868C] Allocate device no failed\n");
		return -EAGAIN;
	}

	/* Allocate driver */
	warhol_wl2868c_dev.pcdev = cdev_alloc();
	if (warhol_wl2868c_dev.pcdev == NULL) {
		unregister_chrdev_region(warhol_wl2868c_dev.dev_no, 1);
		warhol_wl2868c_debug("[WARHOL_WL2868C] Allocate mem for kobject failed\n");
		return -ENOMEM;
	}

	/* Attatch file operation. */
	cdev_init(warhol_wl2868c_dev.pcdev, &warhol_wl2868c_file_operations);
	warhol_wl2868c_dev.pcdev->owner = THIS_MODULE;

	/* Add to system */
	if (cdev_add(warhol_wl2868c_dev.pcdev, warhol_wl2868c_dev.dev_no, 1)) {
		warhol_wl2868c_debug("Attatch file operation failed\n");
		unregister_chrdev_region(warhol_wl2868c_dev.dev_no, 1);
		return -EAGAIN;
	}

	if (pwarhol_wl2868c_class == NULL) {
		pwarhol_wl2868c_class = class_create(WARHOL_WL2868C_CLASS_NAME);
		if (IS_ERR(pwarhol_wl2868c_class)) {
			int ret = PTR_ERR(pwarhol_wl2868c_class);
			warhol_wl2868c_debug("Unable to create class, err = %d\n", ret);
			return ret;
		}
	}

	warhol_wl2868c_dev.pdevice = device_create(pwarhol_wl2868c_class, NULL,
			warhol_wl2868c_dev.dev_no, NULL, device_drv_name);
	if (warhol_wl2868c_dev.pdevice == NULL) {
		warhol_wl2868c_debug("[WARHOL_WL2868C] Allocate device_create for kobject failed\n");
		return -ENOMEM;
	}

	warhol_wl2868c_dev.pdevice->driver_data = regmap;
	ret = sysfs_create_file(&(warhol_wl2868c_dev.pdevice->kobj), &dev_attr_status.attr);
	ret = sysfs_create_file(&(warhol_wl2868c_dev.pdevice->kobj), &dev_attr_info.attr);
	ret = sysfs_create_file(&(warhol_wl2868c_dev.pdevice->kobj), &dev_attr_enable.attr);

	return 0;
}
static int warhol_wl2868c_regulator_probe(struct i2c_client *client)
{
	int rc                = 0;
	unsigned int val      = 0xFF;
	struct regmap *regmap = NULL;
	int index = 0;

	client->addr =  (WARHOL_WL2868C_SLAVE_ADDR >> 1);

	rc = of_property_read_u32(client->dev.of_node, "index", &index);
	if (rc) {
		warhol_wl2868c_err("failed to read index");
		return rc;
	}

	warhol_wl2868c_driver_register(index, regmap);

	regmap = devm_regmap_init_i2c(client, &warhol_wl2868c_regmap_config);
	if (IS_ERR(regmap)) {
		warhol_wl2868c_err("WARHOL_WL2868C failed to allocate regmap\n");
		return PTR_ERR(regmap);
	}

	rc = regmap_read(regmap, 0x00, &val);
	if (rc < 0) {
		warhol_wl2868c_err("WARHOL_WL2868C failed to get PID\n");
		return -ENODEV;
	}
	else
	warhol_wl2868c_debug("WARHOL_WL2868C get Product ID: [%02x]\n", val);

	rc = warhol_wl2868c_parse_regulator(regmap, &client->dev);
	if (rc < 0) {
		warhol_wl2868c_err("WARHOL_WL2868C failed to parse device tree rc=%d\n", rc);
		return -ENODEV;
	}

	return 0;
}

static const struct of_device_id warhol_wl2868c_dt_ids[] = {
	{
		.compatible = "willsemi,warhol_wl2868c",
	},
	{
		.compatible = "sgm,sgm38120",
	},
	{ }
};
MODULE_DEVICE_TABLE(of, warhol_wl2868c_dt_ids);

static const struct i2c_device_id warhol_wl2868c_id[] = {
	{
		.name = "wl2868c-regulator",
		.driver_data = 0,
	},
	{ },
};
MODULE_DEVICE_TABLE(i2c, warhol_wl2868c_id);

static struct i2c_driver warhol_wl2868c_regulator_driver = {
	.driver = {
		.name = "wl2868c-regulator",
		.of_match_table = of_match_ptr(warhol_wl2868c_dt_ids),
	},
	.probe = warhol_wl2868c_regulator_probe,
	.id_table = warhol_wl2868c_id,
};

module_i2c_driver(warhol_wl2868c_regulator_driver);
MODULE_LICENSE("GPL v2");

