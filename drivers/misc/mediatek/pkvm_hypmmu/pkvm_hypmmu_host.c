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
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <pkvm_mgmt/pkvm_mgmt.h>
#include "debug_pgtbl.h"
#include "pkvm_hypmmu_host.h"

#if defined(MTK_IOMMU_CMAPOOL) && (MTK_IOMMU_CMAPOOL)
#include "mtk_iommu-cmapool.h"
#endif /* defined(MTK_IOMMU_CMAPOOL) && (MTK_IOMMU_CMAPOOL) */

#undef pr_fmt
#define pr_fmt(fmt) "[PKVM_HYPMMU]: " fmt

#define ksym_ref_addr_nvhe(x) \
	((typeof(kvm_nvhe_sym(x)) *)(pkvm_el2_mod_va(&kvm_nvhe_sym(x), mod_token)))

unsigned long mod_token;
int iommu_finalise_hvc;

static u32 query_hal(const char *name)
{
	struct device_node *node = NULL;
	u32 ret = 0;

	node = of_find_node_by_name(NULL, "pkvm");
	if (!node)
		goto out;

	node = of_find_node_by_name(node, "hypmmu");
	if (!node)
		goto out;

	of_property_read_u32(node, name, &ret);
	pr_info("query pkvm/hypmmu/%s=%u\n", name, ret);
out:
	return ret;
}

static bool use_mtkiommu(void)
{
	return query_hal("use-mtkiommu") > 0 ? true : false;
}

static u32 use_gpumpu(void)
{
	return query_hal("use-gpumpu") > 0 ? true : false;
}

static u32 use_inframpu(void)
{
	return query_hal("use-inframpu") > 0 ? true : false;
}

static u32 hal_nums(void)
{
	u32 ret = 0;

	if (use_mtkiommu())
		ret++;
	if (use_gpumpu())
		ret++;
	if (use_inframpu())
		ret++;

	return ret;
}

static int init_each_hals(void)
{
	int ret;

	if (use_mtkiommu()) {
		ret = init_mtkiommu();
		if (ret) {
			pr_err("init_mtkiommu failed\n");
			return ret;
		}
	}

	if (use_gpumpu()) {
		ret = init_gpumpu();
		if (ret) {
			pr_err("init_gpumpu failed\n");
			return ret;
		}
	}

	if (use_inframpu()) {
		ret = init_inframpu();
		if (ret) {
			pr_err("init_inframpu failed\n");
			return ret;
		}
	}

	return 0;
}

static int load_hypmmu_el2_mod(void)
{
	return pkvm_load_el2_module(kvm_nvhe_sym(pkvm_hypmmu_load_init), &mod_token);
}

static int kvm_init_driver(void)
{
	int ret;
	struct kvm_hyp_memcache atomic_mc = {0};

	ret = kvm_iommu_init_hyp(ksym_ref_addr_nvhe(hypmmu_ops), &atomic_mc);
	if (ret) {
		pr_info("kvm_iommu_init_hyp ret=%d\n", ret);
		return ret;
	}

	ret = pkvm_el2_mod_call(iommu_finalise_hvc);
	if (ret)
		pr_info("iommu_finalise_handler ret=%d\n", ret);

	return ret;
}

pkvm_handle_t kvm_get_iommu_id_by_of(struct device_node *np)
{
	return 0;
}

struct kvm_iommu_driver kvm_iommu_ops = {
	.init_driver = kvm_init_driver,
	.get_iommu_id_by_of  = kvm_get_iommu_id_by_of,
};

static int register_kvm_iommu(void)
{
	return kvm_iommu_register_driver(&kvm_iommu_ops );
}

static int setup_hvc_call(void)
{
	iommu_finalise_hvc = pkvm_register_el2_mod_call(
		kvm_nvhe_sym(iommu_finalise), mod_token);

	return 0;
}

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
		pr_info("Skip pKVM hypmmu init due to pKVM is not enabled\n");
		return 0;
	}

	if (hal_nums() == 0) {
		pr_info("no any hals, ignore loading el2 module\n");
		return 0;
	}

	/* load hypmmu el2 module */
	ret = load_hypmmu_el2_mod();
	if (ret) {
		pr_err("load hypmmu el2 mod failed\n");
		return ret;
	}

	/* Init for each HAL form host side */
	ret = init_each_hals();
	if (ret)
		return ret;

	setup_hvc_call();

	ret = register_kvm_iommu();
	if (ret) {
		pr_err("register_kvm_iommu failed\n");
		return ret;
	}

	ret = debug_pgtbl_init();
	if (ret) {
		pr_err("debug_pgtbl_init failed\n");
		return ret;
	}
final:
	return ret;
}

module_init(hypmmu_nvhe_init);
MODULE_LICENSE("GPL");
