// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include "mtk_disp_ovl_outproc.h"

static int mtk_disp_ovl_outproc_probe(struct platform_device *pdev)
{
	return 0;
}

static int mtk_disp_ovl_outproc_remove(struct platform_device *pdev)
{

	return 0;
}

static const struct mtk_disp_ovl_outproc_data mt6991_ovl_driver_data = {
	//
};

static const struct of_device_id mtk_disp_ovl_outproc_driver_dt_match[] = {
	{.compatible = "mediatek,mt6991-disp-ovl-outproc",
	 .data = &mt6991_ovl_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_ovl_outproc_driver_dt_match);

struct platform_driver mtk_disp_ovl_outproc_driver = {
	.probe = mtk_disp_ovl_outproc_probe,
	.remove = mtk_disp_ovl_outproc_remove,
	.driver = {

			.name = "mediatek-disp-ovl-outproc",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_ovl_outproc_driver_dt_match,
		},
};
