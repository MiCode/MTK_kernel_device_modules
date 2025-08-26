// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/spmi.h>

#include <mbraink_modules_ops_def.h>
#include "mbraink_v6993_systeminfo.h"

#define CHIP_VER_E1 0x0
#define CHIP_VER_E2 0x1

#define MT6661_RESERVED_REG 0xA10

struct tag_chipid {
	u32 size;
	u32 hw_code;
	u32 hw_subcode;
	u32 hw_ver;
	u32 sw_ver;
};

static struct device_node *mbraink_pmic_np;
static struct regmap *mbraink_pmic_regmap;
static struct platform_device *mbraink_pmic_pdev;
static int mbraink_pmic_magic;

static int mbraink_v6993_set_sw_count_mode(int command);

static ssize_t mbraink_platform_system_info_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	int ret = 0;
	int local_mbraink_pmic_magic = 0;

	ret = regmap_read(mbraink_pmic_regmap, MT6661_RESERVED_REG, &local_mbraink_pmic_magic);

	if (ret)
		pr_err("%s: Failed to read reserved reg: %d\n", __func__, ret);

	return snprintf(buf, PAGE_SIZE, "update reg = 0x%x, current reg = 0x%x\n",
			local_mbraink_pmic_magic, mbraink_pmic_magic);
}

static ssize_t mbraink_platform_system_info_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf,
						size_t count)
{
	int command = 0;
	int retSize = 0;

	retSize = kstrtoint(buf, 10, &command);
	if (retSize == -1) {
		pr_info("%s: read command sscanf failed\n", __func__);
		return 0;
	}

	mbraink_v6993_set_sw_count_mode(command);

	return count;
}
static DEVICE_ATTR_RW(mbraink_platform_system_info);

static int mbraink_v6993_get_chipid_info(struct mbraink_chipid_info *chipid_info)
{
	struct device_node *node;
	struct tag_chipid *chip_id;
	int len;

	node = of_find_node_by_path("/chosen");
	if (!node)
		node = of_find_node_by_path("/chosen@0");

	if (node) {
		chip_id = (struct tag_chipid *)of_get_property(node, "atag,chipid", &len);
		if (!chip_id) {
			pr_info("%s: could not found atag,chipid in chosen\n", __func__);
			return -ENODEV;
		}
	} else {
		pr_info("%s: chosen node not found in device tree\n", __func__);
		return -ENODEV;
	}

	if (chip_id->sw_ver == CHIP_VER_E1)
		chipid_info->sw_ver = 1;
	else
		chipid_info->sw_ver = 2;

	return 0;
}

static int mbraink_v6993_set_sw_count_mode(int val)
{
	int ret = 0;

	if (!mbraink_pmic_regmap) {
		pr_info("%s: mbraink pmic init failed\n", __func__);
		return -ENODEV;
	}

	if (val == 1) {
		ret = regmap_write(mbraink_pmic_regmap, MT6661_RESERVED_REG, MT6661_RESERVED_VAL);
		if (ret)
			pr_err("%s: Failed to write PMIC reg 0x%02x: %d\n",
				__func__, MT6661_RESERVED_REG, ret);
		else
			pr_info("%s: PMIC reg 0x%02x write: 0x%02x\n",
				__func__, MT6661_RESERVED_REG, MT6661_RESERVED_VAL);
	} else {
		ret = regmap_write(mbraink_pmic_regmap, MT6661_RESERVED_REG, 0);
		if (ret)
			pr_err("%s: Failed to write PMIC reg 0x%02x: %d\n",
				__func__, MT6661_RESERVED_REG, ret);
		else
			pr_info("%s: PMIC reg 0x%02x write: 0x0\n",
				__func__, MT6661_RESERVED_REG);
	}

	return ret;
}

int mbraink_v6993_pmic_read(void)
{
	return mbraink_pmic_magic;
}
EXPORT_SYMBOL_GPL(mbraink_v6993_pmic_read);

int mbraink_v6993_pmic_interface_get(struct device *dev)
{
	int ret = 0;

	mbraink_pmic_np = of_parse_phandle(dev->of_node, "pmic", 0);
	if (!mbraink_pmic_np) {
		pr_err("%s: Failed to get PMIC node from DT\n", __func__);
		return -ENODEV;
	}

	mbraink_pmic_pdev = of_find_device_by_node(mbraink_pmic_np->child);

	if (!mbraink_pmic_pdev) {
		pr_err("%s: Failed to find platform device for pmic\n", __func__);
		of_node_put(mbraink_pmic_np);
		mbraink_pmic_np = NULL;
		return -ENODEV;
	}

	mbraink_pmic_regmap = dev_get_regmap(mbraink_pmic_pdev->dev.parent, NULL);

	if (!mbraink_pmic_regmap) {
		pr_err("%s: Failed to get PMIC regmap\n", __func__);
		of_node_put(mbraink_pmic_np);
		mbraink_pmic_np = NULL;
		put_device(&mbraink_pmic_pdev->dev);
		mbraink_pmic_pdev = NULL;
		return -ENODEV;
	}

	ret = regmap_read(mbraink_pmic_regmap, MT6661_RESERVED_REG, &mbraink_pmic_magic);
	if (ret) {
		pr_err("%s: Failed to read reserved reg: %d\n", __func__, ret);
		return ret;
	}
	pr_info("%s: Reserved reg[0x%02x] = 0x%02x\n",
		__func__, MT6661_RESERVED_REG, mbraink_pmic_magic);

	return ret;
}

int mbraink_v6993_pmic_interface_put(struct device *dev)
{
	if (mbraink_pmic_np)
		of_node_put(mbraink_pmic_np);
	if (mbraink_pmic_pdev)
		put_device(&mbraink_pmic_pdev->dev);
	return 0;
}

static struct mbraink_systeminfo_ops mbraink_v6993_systeminfo_ops = {
	.get_chipid_info = mbraink_v6993_get_chipid_info,
	.set_sw_count_mode = mbraink_v6993_set_sw_count_mode,
};

int mbraink_v6993_systeminfo_init(struct device *dev)
{
	int ret = 0;

	ret = device_create_file(dev, &dev_attr_mbraink_platform_system_info);
	if (ret) {
		pr_err("%s: device_create_file failed\n", __func__);
		goto End;
	}

	ret = register_mbraink_systeminfo_ops(&mbraink_v6993_systeminfo_ops);
	if (ret) {
		pr_err("%s: register_mbraink_systeminfo_ops failed\n", __func__);
		goto End;
	}

	ret = mbraink_v6993_pmic_interface_get(dev);
	if (ret)
		pr_info("%s: register mbraink pmic interface failed\n", __func__);

End:
	return ret;
}

int mbraink_v6993_systeminfo_deinit(struct device *dev)
{
	int ret = 0;

	device_remove_file(dev, &dev_attr_mbraink_platform_system_info);

	ret = unregister_mbraink_systeminfo_ops();
	if (ret) {
		pr_err("%s: unregister_mbraink_systeminfo_ops failed\n", __func__);
		goto Deinit;
	}

	ret = mbraink_v6993_pmic_interface_put(dev);
Deinit:
	return ret;
}
