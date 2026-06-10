// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#include "mbraink_v8668.h"

static ssize_t mbraink_platform_info_show(struct device *dev,
								struct device_attribute *attr,
								char *buf)
{
	return snprintf(buf, PAGE_SIZE, "show mbraink v8668 information...\n");
}

static ssize_t mbraink_platform_info_store(struct device *dev,
								struct device_attribute *attr,
								const char *buf,
								size_t count)
{
	unsigned int command;
	unsigned long long value;
	int retSize = 0;

	retSize = sscanf(buf, "%d %llu", &command, &value);
	if (retSize == -1)
		return 0;

	pr_info("%s: Get Command (%d), Value (%llu) size(%d)\n",
			__func__,
			command,
			value,
			retSize);

	if (command == 1)
		mbraink_v8668_gpu_setQ2QTimeoutInNS(value);
	if (command == 2)
		mbraink_v8668_gpu_setPerfIdxTimeoutInNS(value);
	if (command == 3)
		mbraink_v8668_gpu_setPerfIdxLimit(value);
	if (command == 4)
		mbraink_v8668_gpu_dumpPerfIdxList();
	if (command == 5)
		mbraink_v8668_gpu_fpsgoSetGameMode(value);
	return count;
}
static DEVICE_ATTR_RW(mbraink_platform_info);

static int mbraink_v8668_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *mbraink_v8668_device = &pdev->dev;

	ret = device_create_file(mbraink_v8668_device, &dev_attr_mbraink_platform_info);
	pr_info("[MBK_v8668] %s: device create file mbraink info ret = %d\n", __func__, ret);

	ret = mbraink_v8668_hypervisor_init();
	if (ret)
		pr_notice("[MBK_v8668] mbraink 8668 hypervisor init failed.\n");

	ret = mbraink_v8668_memory_init(mbraink_v8668_device);
	if (ret)
		pr_notice("[MBK_v8668] mbraink v8668 memory init failed.\n");

	ret = mbraink_v8668_audio_init();
	if (ret)
		pr_notice("[MBK_v8668] mbraink v8668 audio init failed.\n");

	ret = mbraink_v8668_battery_init(mbraink_v8668_device);
	if (ret)
		pr_notice("[MBK_v8668] mbraink v8668 battery init failed.\n");

	ret = mbraink_v8668_power_init(mbraink_v8668_device);
	if (ret)
		pr_notice("[MBK_v8668] mbraink v8668 power init failed.\n");

	ret = mbraink_v8668_gpu_init();
	if (ret)
		pr_notice("[MBK_v8668] mbraink v8668 gpu init failed.\n");

	ret = mbraink_v8668_gps_init();
	if (ret)
		pr_notice("[MBK_v8668] mbraink v8668 gps init failed.\n");

	ret = mbraink_v8668_wifi_init();
	if (ret)
		pr_notice("[MBK_v8668] mbraink v8668 wifi init failed.\n");

	ret = mbraink_v8668_camera_init();
	if (ret)
		pr_notice("[MBK_v8668] mbraink v8668 camera init failed.\n");

	ret = mbraink_v8668_pmu_init();
	if (ret)
		pr_notice("[MBK_v8668] mbraink v8668 pmu init failed.\n");

	ret = mbraink_v8668_touch_init();
	if (ret)
		pr_notice("[MBK_v8668] mbraink v8668 touch init failed.\n");

	ret = mbraink_v8668_hrt_init(mbraink_v8668_device);
	if (ret)
		pr_notice("[MBK_v8668] mbraink v8668 hrt init failed.\n");

	ret = mbraink_v8668_systeminfo_init(mbraink_v8668_device);
	if (ret)
		pr_notice("[MBK_v8668] mbraink v8668 systeminfo init failed.\n");

	ret = mbraink_v8668_telephony_init();
	if (ret)
		pr_notice("mbraink telephony init failed.\n");
	ret = mbraink_v8668_cmdq_init();
	if (ret)
		pr_notice("mbraink cmdq init failed.\n");

	return ret;
}

static void mbraink_v8668_remove(struct platform_device *pdev)
{
	struct device *mbraink_v8668_device = &pdev->dev;

	device_remove_file(mbraink_v8668_device, &dev_attr_mbraink_platform_info);

	mbraink_v8668_hypervisor_deinit();
	mbraink_v8668_memory_deinit(mbraink_v8668_device);
	mbraink_v8668_audio_deinit();
	mbraink_v8668_battery_deinit();
	mbraink_v8668_power_deinit();
	mbraink_v8668_gpu_deinit();
	mbraink_v8668_gps_deinit();
	mbraink_v8668_wifi_deinit();
	mbraink_v8668_camera_deinit();
	mbraink_v8668_pmu_deinit();
	mbraink_v8668_touch_deinit();
	mbraink_v8668_hrt_deinit(mbraink_v8668_device);
	mbraink_v8668_systeminfo_deinit(mbraink_v8668_device);
	mbraink_v8668_telephony_deinit();
	mbraink_v8668_cmdq_deinit();
}

static const struct of_device_id mtk_mbraink_v8668_of_ids[] = {
	{ .compatible = "mediatek,mbraink-v8668" },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_mbraink_v8668_of_ids);

static struct platform_driver mtk_mbraink_v8668_driver = {
	.probe = mbraink_v8668_probe,
	.remove = mbraink_v8668_remove,
	.driver = {
		.name = "mtk_mbraink_v8668",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = mtk_mbraink_v8668_of_ids,
		.pm = NULL,
	},
};
module_platform_driver(mtk_mbraink_v8668_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("<Wei-chin.Tsai@mediatek.com>");
MODULE_DESCRIPTION("MBrainK v8668 Linux Device Driver");
MODULE_VERSION("1.0");
