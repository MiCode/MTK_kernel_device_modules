// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <mbraink_modules_ops_def.h>

#include "mbraink_v6993_systeminfo.h"

#define CHIP_VER_E1 0x0
#define CHIP_VER_E2 0x1

struct tag_chipid {
	u32 size;
	u32 hw_code;
	u32 hw_subcode;
	u32 hw_ver;
	u32 sw_ver;
};

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

static struct mbraink_systeminfo_ops mbraink_v6993_systeminfo_ops = {
	.get_chipid_info = mbraink_v6993_get_chipid_info,
};

int mbraink_v6993_systeminfo_init(void)
{
	int ret = 0;

	ret = register_mbraink_systeminfo_ops(&mbraink_v6993_systeminfo_ops);

	return ret;
}

int mbraink_v6993_systeminfo_deinit(void)
{
	int ret = 0;

	ret = unregister_mbraink_systeminfo_ops();

	return ret;
}
