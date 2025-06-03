// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_reserved_mem.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <asm/kvm_pkvm_module.h>
#include <asm/kvm_host.h>
#include <crypto/sha2.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <pkvm_mgmt/pkvm_mgmt.h>
#include "pkvm_hypmmu_host.h"

#if defined(MTK_IOMMU_CMAPOOL) && (MTK_IOMMU_CMAPOOL)
#include "mtk_iommu-cmapool.h"
#endif /* defined(MTK_IOMMU_CMAPOOL) && (MTK_IOMMU_CMAPOOL) */

#undef pr_fmt
#define pr_fmt(fmt) "[PKVM_HYPMMU]: " fmt

static int __init hypmmu_nvhe_init(void)
{
	int ret = -EPERM;

#if defined(MTK_IOMMU_CMAPOOL) && (MTK_IOMMU_CMAPOOL)
	/* Allocate mtk-iommu pgtables from CMA */
	ret = platform_driver_register(&mtk_iommu_cmapool_driver);
	if (unlikely(ret != 0)) {
		pr_info("failed to register hw driver: %s\n",
				mtk_iommu_cmapool_driver.driver.name);
		ret = 0;
		goto final;
	}
#endif /* defined(MTK_IOMMU_CMAPOOL) && (MTK_IOMMU_CMAPOOL) */

	if (!is_protected_kvm_enabled()) {
		pr_info("Skip pKVM hypmmu init, cause pKVM is not enabled\n");
		ret = 0;
		goto final;
	}

	/* Allocate hypmmu page ownership table */

	/*  */
#if 0
	if (has_mtkiommu())
		init_mtkiommu();
	if (has_gpumpu())
		init_gpumpu();
	if (has_inframpu())
		init_inframpu();
#endif /* 0 */

	ret = 0;

final:
	return ret;
}

module_init(hypmmu_nvhe_init);
MODULE_LICENSE("GPL");
