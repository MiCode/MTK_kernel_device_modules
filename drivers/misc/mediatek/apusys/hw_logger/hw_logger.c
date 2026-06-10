// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/platform_device.h>
#include <linux/of_device.h>

#include "hw_logger.h"

/* debug log level */
static struct platform_device *g_pdev;
static struct mtk_apu_logger_platdata *g_platdata;
unsigned char g_hw_logger_log_lv = DBG_LOG_INFO;


int hw_logger_power_on(void)
{
	HWLOGR_DBG("+");

	if (!g_platdata || !g_platdata->ops.v1_ops)
		return -EINVAL;

	return g_platdata->ops.v1_ops->power_on();
}

int hw_logger_deep_idle_enter_pre(void)
{
	HWLOGR_DBG("+");

	if (!g_platdata || !g_platdata->ops.v1_ops)
		return -EINVAL;

	return g_platdata->ops.v1_ops->deep_idle_enter_pre();
}

int hw_logger_deep_idle_enter_post(void)
{
	HWLOGR_DBG("+");

	if (!g_platdata || !g_platdata->ops.v1_ops)
		return -EINVAL;

	return g_platdata->ops.v1_ops->deep_idle_enter_post();
}

int hw_logger_deep_idle_leave(void)
{
	HWLOGR_DBG("+");

	if (!g_platdata || !g_platdata->ops.v1_ops)
		return -EINVAL;

	return g_platdata->ops.v1_ops->deep_idle_leave();
}

int hw_logger_dump_tcm_log(void)
{
	HWLOGR_DBG("+");

	if (!g_platdata || !g_platdata->ops.v1_ops)
		return -EINVAL;

	return g_platdata->ops.v1_ops->dump_tcm_log();
}

int hw_logger_config_init(struct mtk_apu *apu)
{
	HWLOGR_INFO("+");

	if (!g_platdata)
		return -EINVAL;

	return g_platdata->ops.config_init(apu);
}

int hw_logger_ipi_init(struct mtk_apu *apu)
{
	HWLOGR_INFO("+");

	if (!g_platdata)
		return -EINVAL;

	return g_platdata->ops.ipi_init(apu);
}

void hw_logger_ipi_remove(struct mtk_apu *apu)
{
	HWLOGR_INFO("+");

	if (!g_platdata)
		return;

	g_platdata->ops.ipi_remove(apu);
}


static int hw_logger_probe(struct platform_device *pdev)
{
	struct mtk_apu_logger_platdata *platdata;

	HWLOGR_INFO("+");
	platdata = (struct mtk_apu_logger_platdata *)
		of_device_get_match_data(&pdev->dev);

	if (!platdata) {
		HWLOGR_ERR("of_device_get_match_data fail\n");
		return -EINVAL;
	}

	g_pdev = pdev;
	g_platdata = platdata;

	HWLOGR_INFO("-");
	return g_platdata->ops.probe(g_pdev);
}

static void hw_logger_remove(struct platform_device *pdev)
{
	HWLOGR_INFO("+");
	g_platdata->ops.remove(pdev);
	g_platdata = NULL;
	g_pdev = NULL;
}

static void hw_logger_shutdown(struct platform_device *pdev)
{
	HWLOGR_INFO("+");
	g_platdata->ops.shutdown(pdev);
}

#ifndef LOGGER_V1_PLAT_DATA
const struct mtk_apu_logger_platdata logger_v1_platdata;
#endif
#ifndef LOGGER_V2_PLAT_DATA
const struct mtk_apu_logger_platdata logger_v2_platdata;
#endif

static const struct of_device_id apusys_hw_logger_of_match[] = {
	{ .compatible = "mediatek,apusys_hw_logger", .data = &logger_v1_platdata},
	{ .compatible = "mediatek,mt6993-apusys_hw_logger", .data = &logger_v2_platdata},
	{},
};

static struct platform_driver hw_logger_driver = {
	.probe = hw_logger_probe,
	.remove = hw_logger_remove,
	.shutdown = hw_logger_shutdown,
	.driver = {
		.name = HWLOGR_DEV_NAME,
		.of_match_table = of_match_ptr(apusys_hw_logger_of_match),
	}
};

int hw_logger_init(struct apusys_core_info *info)
{
	int ret = 0;

	HWLOGR_INFO("+");
	allow_signal(SIGKILL);

	ret = platform_driver_register(&hw_logger_driver);
	if (ret != 0) {
		HWLOGR_ERR("failed to register hw_logger driver");
		return -ENODEV;
	}
	return ret;
}

void hw_logger_exit(void)
{
	HWLOGR_INFO("+");
	disallow_signal(SIGKILL);
	platform_driver_unregister(&hw_logger_driver);
}
