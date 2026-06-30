// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/dma-direct.h>
#include <linux/rpmsg.h>

#include "apusys_core.h"
#include "hds.h"

static struct apusys_core_info *g_info;
/* global */
struct apu_hds_device *g_hdev;
uint32_t g_hds_klog;

static int apu_hds_rpmsg_probe(struct rpmsg_device *rpdev)
{
	int ret = 0;

	pr_info("%s +\n", __func__);

	g_hds_klog = APU_HDS_LOG_LV_INFO;

	g_hdev = devm_kzalloc(&rpdev->dev, sizeof(*g_hdev), GFP_KERNEL);
	if (IS_ERR_OR_NULL(g_hdev)) {
		ret = IS_ERR_OR_NULL(g_hdev);
		goto out;
	}

	g_hdev->rpdev = rpdev;
	ret = apu_hds_dev_init(g_hdev);
	if (ret) {
		apu_hds_err("init apu hds failed(%d)\n", ret);
		goto free_hdev;
	}

	ret = hds_cmd_init();
	if (ret) {
		apu_hds_err("init hds cmd failed(%d)\n", ret);
		goto deinit_dev;
	}

	ret = apu_hds_sysfs_init();
	if (ret) {
		apu_hds_err("init hds sysfs failed(%d)\n", ret);
		goto deinit_dev;
	}

	goto out;

deinit_dev:
	apu_hds_dev_deinit(g_hdev);
free_hdev:
	 devm_kfree(&rpdev->dev, g_hdev);
out:
	pr_info("%s -\n", __func__);

	return ret;
}

static void apu_hds_rpmsg_remove(struct rpmsg_device *rpdev)
{
	pr_info("%s +\n", __func__);
	apu_hds_sysfs_deinit();
	apu_hds_dev_deinit(g_hdev);
	devm_kfree(&rpdev->dev, g_hdev);
	pr_info("%s -\n", __func__);
}

static const struct of_device_id apu_hds_rpmsg_of_match[] = {
	{ .compatible = "mediatek,apu-hds-tx-rpmsg"},
	{ },
};

static struct rpmsg_driver apu_hds_rpmsg_driver = {
	.drv = {
		.name = "apu-hds-rpmsg",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(apu_hds_rpmsg_of_match),
	},
	.probe = apu_hds_rpmsg_probe,
	.remove = apu_hds_rpmsg_remove,
};

//----------------------------------------
int apu_hds_init(struct apusys_core_info *info)
{
	int ret = 0;

	g_info = info;

	pr_info("%s register rpmsg...\n", __func__);

	ret = register_rpmsg_driver(&apu_hds_rpmsg_driver);
	if (ret)
		pr_info("failed to register apu hds rpmsg driver\n");

	pr_info("%s register rpmsg done(%d)\n", __func__, ret);

	return ret;
}

void apu_hds_exit(void)
{
	unregister_rpmsg_driver(&apu_hds_rpmsg_driver);
}
