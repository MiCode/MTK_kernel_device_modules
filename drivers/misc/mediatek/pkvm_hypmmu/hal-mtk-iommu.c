// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_reserved_mem.h>
#include <asm/kvm_pkvm_module.h>

#include <mtk-iommu-defines.h>
#include "pkvm_hypmmu_host.h"
#include "mtk_iommu-pagepool.h"

#if defined(MTK_IOMMU_CMAPOOL) && (MTK_IOMMU_CMAPOOL)
#include "mtk_iommu-cmapool.h"
#endif

#undef pr_fmt
#define pr_fmt(fmt) "[PKVM_HYPMMU_MTKIOMMU]: " fmt

#define PROT_PGD_NAME "mediatek,platform_mtksmmu_protpgd"

static int init_hvc;
static int add_iommu_device_hvc;

static phys_addr_t rmem_base;
static phys_addr_t rmem_size;

static phys_addr_t pmm_pfn;
static phys_addr_t pmm_size;

static phys_addr_t page_pool_base;
static phys_addr_t page_pool_size;

static struct page *ac_table_page;
static u32 ac_table_order;

static int setup_hvc_call(void)
{
	init_hvc = pkvm_register_el2_mod_call(
		kvm_nvhe_sym(mtk_iommu_hyp_init), mod_token);
	add_iommu_device_hvc = pkvm_register_el2_mod_call(
		kvm_nvhe_sym(mtk_iommu_add_device), mod_token);

	return 0;
}

static int alloc_ac_table(void)
{
	u32 size = AC_TABLE_SIZE;
	u32 order = get_order(size);

	pr_info("%s: size=%u order=%u\n", __func__, size, order);
	ac_table_page = alloc_pages(GFP_KERNEL, order);

	if (!ac_table_page) {
		pr_info("allocate ac table failed. size=%u order=%u\n",
			size, order);
		return -ENOMEM;
	}

	ac_table_order = order;

	return 0;
}

static int query_rmem(void)
{
	struct device_node *node;
	struct reserved_mem *rmem;

	node = of_find_compatible_node(NULL, NULL, PROT_PGD_NAME);
	if (!node) {
		pr_info("%s not found\n", PROT_PGD_NAME);
		return -EINVAL;
	}

	rmem = of_reserved_mem_lookup(node);
	if (!rmem) {
		pr_info("mtksmmu_protpgd reserved mem not found\n");
		return -EINVAL;
	}

	rmem_base = rmem->base;
	rmem_size = rmem->size;

	pr_info("%s: rmem_base=%llx rmem_size=%llx\n", __func__, rmem_base, rmem_size);

	return 0;
}

static int query_page_pool(void)
{
	if (is_iommu_pgtbl_page_memory()) {
		/* Allocate memory from body system for mpool*/
		pmm_pfn = alloc_iommu_pgtbl_page();
		pmm_size = PAGE_SIZE;
	} else {
		/* Allocate memory from CMA/Reserved memory */
		pmm_pfn = 0;
		pmm_size = 0;
	}

#if defined(MTK_IOMMU_CMAPOOL) && (MTK_IOMMU_CMAPOOL)
	page_pool_base = (phys_addr_t)cma_pool_base;
	page_pool_size = (phys_addr_t)cma_pool_size;

	pr_info("%s: page_pool_base=%llx page_pool_size=%llx\n", __func__,
		page_pool_base , page_pool_size);
#endif
	return 0;
}

static int setup_memory_pool(void)
{
	int ret;

	/* Allocate SWS2 AC table */
	ret = alloc_ac_table();
	if (ret)
		return ret;

	/* Query reserved memory */
	ret = query_rmem();
	if (ret)
		return ret;

	ret = query_page_pool();
	if (ret)
		return ret;

	ret = pkvm_el2_mod_call(init_hvc, page_to_phys(ac_table_page),
		ac_table_order, rmem_base, rmem_size + page_pool_size,
		pmm_pfn, pmm_size);

	return ret;
}

struct device_node *get_dev_node_by_alias(struct device_node *alias_node, unsigned int i)
{
	char name[64] = "";
	const char *path = NULL;
	struct device_node *node = NULL;

	if (snprintf(name, 64, "mtksmmu%d", i) < 0)
		return node;

	if(!of_property_read_string(alias_node, name, &path)) {
		node = of_find_node_by_path(path);
		pr_info("%s: found :%s\n", __func__, name);
	}

	return node;
}

static bool read_reg(struct device_node *node, unsigned int *reg_start, unsigned int *reg_size)
{
	int array_size = 0;
	unsigned int reg[8] = { 0U };

	array_size = of_property_count_u32_elems(node, "reg");
	if (array_size < 0) {
		pr_info("wrong reg array size\n");
		return false;
	}

	if (of_property_read_u32_array(node, "reg", reg, array_size)) {
		pr_info("%s: read reg value fail\n", __func__);
		return false;
	}

	*reg_start = reg[1];
	*reg_size = reg[3];

	return true;
}

static void read_table_id(struct device_node *node, unsigned int *table_id)
{
	/* new: table-id */
	if (of_property_read_u32(node, "table-id", table_id) == 0)
		return;

	/* legacy: table_id */
	if (of_property_read_u32(node, "table_id", table_id) == 0)
		return;

	*table_id = 0;
}

static int probe_devices(void)
{
	struct device_node *node = NULL, *alias_node = NULL;
	unsigned int reg_start = 0U, reg_size = 0U;
	unsigned int table_id = 0;
	int found = 0;
	int ret;

	alias_node = of_find_node_by_path("/aliases");

	if (!alias_node) {
		pr_info("%s: search alias fail\n", __func__);
		return -EINVAL;
	}

	for (unsigned int i = 0; i < MAX_IOMMU_DEVICES; i++) {
		node = get_dev_node_by_alias(alias_node, i);
		if (!node)
			continue;

		if (!read_reg(node, &reg_start, &reg_size))
			continue;

		read_table_id(node, &table_id);

		ret = pkvm_el2_mod_call(add_iommu_device_hvc, reg_start, reg_size, table_id);
		if (ret)
			break;
		found++;
	}

	if (!found) {
		pr_info("WARN: no any mkt-iommu devices found in aliases\n");
		return -EINVAL;
	}

	return 0;
}

int init_mtkiommu(void)
{
	int ret;

	/* Setup hvc call for mtkiommu */
	ret = setup_hvc_call();
	if (ret) {
		pr_info("mtkiommu: setup hvc call failed %d\n", ret);
		return ret;
	}

	/* Setup memory pool */
	ret = setup_memory_pool();
	if (ret) {
		pr_info("mtkiommu: setup memory pool failed %d\n", ret);
		return ret;
	}

	ret = probe_devices();
	if (ret) {
		pr_info("mtkiommu: probe devices failed %d\n", ret);
		return ret;
	}

	return 0;
}
