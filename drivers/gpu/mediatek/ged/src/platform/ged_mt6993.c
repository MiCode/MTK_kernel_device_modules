// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/ioport.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#include "ged_mt6993.h"
#include "ged_log.h"
#include "ged_base.h"
#include "ged_eb.h"
#include "ged_global.h"

static int ged_platform_pdrv_probe(struct platform_device *pdev);
static void ged_platform_pdrv_remove(struct platform_device *pdev);

#define GED_DRIVER_DEVICE_NAME "ged_platform"

static const struct of_device_id g_ged_platform_of_match[] = {
	{ .compatible = "mediatek,gpu_reduce_mips" },  // dts
	{ /* sentinel */ }
};
static struct platform_driver g_ged_platform_pdrv = {
	.probe = ged_platform_pdrv_probe,
	.remove = ged_platform_pdrv_remove,
	.driver = {
		.name = "ged_platform",
		.owner = THIS_MODULE,
		.of_match_table = g_ged_platform_of_match,
	},
};

static struct ged_platform_fp platform_fp = {
	.get_sysram = ged_platform_get_sysram,
	.get_ts_rb_num = ged_platform_get_ts_rb_num,
	.get_mbrain_max_num = ged_platform_get_mbrain_max_num,
};

unsigned int ged_platform_get_ts_rb_num(void)
{
	return SRAM_TS_RB_NUM;
}

unsigned int ged_platform_get_mbrain_max_num(void)
{
	return MBRAIN_MAX_OPP_NUM;
}

unsigned int ged_platform_get_sysram(int virtual_offset)
{
	unsigned int index = 0;
	unsigned int ret = 0;

	// Order: legacy data => Debug RB => TS RB => Mbrain => Normal data

	// legacy sysram defined in ged_eb.h
	if (virtual_offset < FDVFS_LEGACY_DATA_SIZE)
		ret = virtual_offset;

	// Debug RB range
	if (virtual_offset >= FDVFS_LEGACY_DATA_SIZE &&
		virtual_offset < FDVFS_TS_VIRTUAL_DATA_START) {
		index = virtual_offset - FDVFS_LEGACY_DATA_SIZE;
		ret = FDVFS_LEGACY_DATA_SIZE + index * SYSRAM_LOG_SIZE;
	}
	// TS RB range
	if (virtual_offset >= FDVFS_TS_VIRTUAL_DATA_START &&
		virtual_offset < FDVFS_MBRAIN_VIRTUAL_DATA_START) {
		index = virtual_offset - FDVFS_TS_VIRTUAL_DATA_START;
		ret = FDVFS_TS_REAL_DATA_START + index * SYSRAM_LOG_SIZE;
	}
	// Mbrain range
	if (virtual_offset >= FDVFS_MBRAIN_VIRTUAL_DATA_START &&
		virtual_offset < FDVFS_NORMAL_VIRTUAL_DATA_START) {
		index = virtual_offset - FDVFS_MBRAIN_VIRTUAL_DATA_START;
		ret = FDVFS_MBRAIN_REAL_DATA_START + index * SYSRAM_LOG_SIZE;
	}
	// Normal data range
	if (virtual_offset >= FDVFS_NORMAL_VIRTUAL_DATA_START) {
		index = virtual_offset - FDVFS_NORMAL_VIRTUAL_DATA_START;
		ret = FDVFS_NORMAL_REAL_DATA_START + index * SYSRAM_LOG_SIZE;
	}

	return ret;
}

static int ged_init_platform_info(void)
{
	GED_ERROR err = GED_OK;
	// do something
	return err;
}

/*
 * ged driver probe
 */
static int ged_platform_pdrv_probe(struct platform_device *pdev)
{
	GED_ERROR err = GED_OK;

	GED_LOGI("@%s: start to probe ged_platform driver\n", __func__);

	/* defer probe when ged wrapper isn't ready */
	if (!ged_driver_done()) {
		GED_LOGE("ged has not been probed, defer ged platform probe");
		err = -EPROBE_DEFER;
		return err;
	}

	err = ged_init_platform_info();
	if (err != GED_OK)
		GED_LOGI("@%s: Fail to init platfor info\n", __func__);

	/*
	 * register differnet platform fp to wrapper
	 */
	ged_register_platform_fp(&platform_fp);

	/*
	 * do platform related init in ged_eb
	 */
	ged_do_platform_related_init();

	GED_LOGI("@%s: ged_platform driver probe done\n", __func__);

	return err;
}

/*
 * ged driver remove
 */
static void ged_platform_pdrv_remove(struct platform_device *pdev)
{
	// remove_proc_entry(GED_DRIVER_DEVICE_NAME, NULL);
}

/*
 * unregister the ged_platform driver
 */
static void ged_platform_exit(void)
{
	platform_driver_unregister(&g_ged_platform_pdrv);
}

/*
 * register the ged_platform driver
 */
static int ged_platform_init(void)
{
	GED_ERROR err = GED_OK;

	GED_LOGI("@%s: start to init ged_platform driver\n", __func__);

	/* register platform driver */
	err = platform_driver_register(&g_ged_platform_pdrv);
	if (err) {
		GED_LOGE("@%s: failed to register ged_platform driver\n", __func__);
		goto ERROR;
	}

	GED_LOGI("@%s: ged_platform driver init done\n", __func__);

ERROR:
	return err;
}

module_init(ged_platform_init);
module_exit(ged_platform_exit);

MODULE_DEVICE_TABLE(of, g_ged_platform_of_match);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek GED Platform Driver");
MODULE_AUTHOR("MediaTek Inc.");
