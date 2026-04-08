// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 *       Tiffany Lin <tiffany.lin@mediatek.com>
 */
#include <mtk_vcodec_pm_codec.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <soc/mediatek/smi.h>
#include <linux/pm_runtime.h>
int mtk_vcodec_init_pm(struct mtk_vcodec_dev *mtkdev)
{
	struct platform_device *pdev;
	struct platform_device *tmpdev;
	struct mtk_vcodec_pm *pm;
	int ret = 0;
	struct device_node *node;

	pdev = mtkdev->plat_dev;
	tmpdev = mtkdev->plat_dev;
	pm = &mtkdev->pm;
	memset(pm, 0, sizeof(struct mtk_vcodec_pm));
	pm->mtkdev = mtkdev;
	pm->dev = &pdev->dev;
    pr_debug("+%s,%d\n", __func__, __LINE__);

	node = of_find_compatible_node(NULL, NULL, "mediatek,smi_larb1");
	if (!node) {
	    pr_info("[error]+%s,%d.can not get larb1\n", __func__, __LINE__);
		return -1;
	}
	pdev = of_find_device_by_node(node);
	if (pdev == NULL) {
	    pr_info("[error]SMI LARB1 is not ready yet\n");
		of_node_put(node);
		return -1;
	}
	if (!strcmp(node->name, "smi_larb1")) {
		pm->larbvdecs[0] = &pdev->dev;
		if (!device_link_add(&tmpdev->dev, pm->larbvdecs[0],
				DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS)) {
			pr_info("%s larbvdecs(0) device link fail\n", __func__);
			return -1;
		}
		pr_debug("+%s,larbvdecs[0] = %p", __func__,  pm->larbvdecs[0]);
	}

    node = of_find_compatible_node(NULL, NULL, "mediatek,smi_larb4");
	if (!node) {
		pr_info("[error]+%s,%d.can not get larb4\n", __func__, __LINE__);
		return -1;
	}
	pdev = of_find_device_by_node(node);
	if (pdev == NULL) {
	    pr_info("[error]SMI LARB4 is not ready yet\n");
		of_node_put(node);
		return -1;
	}
	if (!strcmp(node->name, "smi_larb4")) {
		pm->larbvencs[0] = &pdev->dev;
		if (!device_link_add(&tmpdev->dev, pm->larbvencs[0],
				DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS)) {
			pr_info("%s larbvencs(0) device link fail\n", __func__);
			return -1;
	}
	    pr_debug("+%s,larbvencs[0] = %p", __func__, pm->larbvencs[0]);
	}

	pr_debug("+%s,%d Done\n", __func__, __LINE__);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_init_pm);

MODULE_LICENSE("GPL");
