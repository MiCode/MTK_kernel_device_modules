// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#include <ged_type.h>
#include <ged_base.h>
#include <ged_gpu_memsys.h>
#if !IS_ENABLED(CONFIG_MTK_GPU_LEGACY) /* MTK_GPU_EB_SUPPORT */
#include <gpufreq_v2.h>
#endif /* MTK_GPU_EB_SUPPORT */

#if defined(MTK_GPU_MEMSYS_UTIL)
static phys_addr_t g_counter_pa, g_counter_va;
static unsigned int g_counter_size;
static struct gpu_memsys_stat *g_memsys_stat;
#if !IS_ENABLED(CONFIG_MTK_GPU_LEGACY) /* MTK_GPU_EB_SUPPORT */
static bool g_hrt_debug_enabled;
#endif /* MTK_GPU_EB_SUPPORT */

static GED_ERROR gpu_memsys_sysram_init(void)
{
	struct resource res = {};
	struct device_node *node = NULL;
	phys_addr_t counter_pa = 0, counter_va = 0, counter_size = 0;

	/* get GPU SLC pre-defined device node from dts */
	node = of_find_compatible_node(NULL, NULL, "mediatek,gpu_memsys");
	if (unlikely(!node)) {
		GED_LOGE("[GPU_MEMSYS]%s Cannot find gpu memsys dts node", __func__);
		return GED_ERROR_FAIL;
	}

	/* get sysram address from "reg" property then translate into a resource */
	if (unlikely(of_address_to_resource(node, 1, &res))) {
		GED_LOGE("[GPU_MEMSYS]%s Cannot get physical memory addr", __func__);
		return GED_ERROR_FAIL;
	}

	counter_pa = res.start;
	counter_size = resource_size(&res);
	/* Transfer physical addr to virtual addr */
	counter_va = (phys_addr_t)(size_t)ioremap_wc(counter_pa, counter_size);
	if (!counter_va) {
		GED_LOGE("[GPU_MEMSYS]%s Fail to ioremap\n", __func__);
		return GED_ERROR_FAIL;
	}

	g_counter_pa = counter_pa;
	g_counter_va = counter_va;
	g_counter_size = counter_size;
	g_memsys_stat = (struct gpu_memsys_stat *)(uintptr_t)g_counter_va;

	/* release reference count for node using */
	of_node_put(node);

	/* clear the buffer*/
	memset((void *)g_counter_va, 0, g_counter_size);

	GED_LOGI("[GPU_MEMSYS]%s memsys sysram usage from %pa ~ %pa size: %x",
		__func__, &(res.start), &(res.end), (unsigned int)resource_size(&res));

	return GED_OK;
}


struct gpu_memsys_stat *get_gpu_memsys_stat(void)
{
	if (likely(g_memsys_stat))
		return g_memsys_stat;
	GED_LOGI("[GPU_MEMSYS]%s null g_memsys_stat\n", __func__);
	return NULL;
}

GED_ERROR ged_gpu_memsys_init(void)
{
	int ret = 0;

	/* init sysram usage */
	ret = gpu_memsys_sysram_init();

	if (unlikely(ret)) {
		GED_LOGE("[GPU_MEMSYS]%s gpu_memsys_sysram_init failed.", __func__);
		return GED_ERROR_FAIL;
	}

	if (g_memsys_stat) {
		g_memsys_stat->features = 0;
		g_memsys_stat->wla_ctrl_1 = 0;
		g_memsys_stat->wla_ctrl_2 = 0;
		GED_LOGI("[GPU_MEMSYS] %s get memsys stat successfully.",  __func__);
		GED_LOGI("[GPU_MEMSYS] feature status: %d\n", g_memsys_stat->features);
	} else {
		GED_LOGE("[GPU_MEMSYS] %s null memsys stat", __func__);
	}

	return GED_OK;
}

void ged_gpu_memsys_feature_enable(unsigned int idx)
{
	if (!g_memsys_stat) {
		GED_LOGE("[GPU_MEMSYS]%s null memsys stat", __func__);
		return;
	}

	g_memsys_stat->features = (idx & 0xFF);
	GED_LOGE("[GPU_MEMSYS]%s memsys feature = 0x%x", __func__, g_memsys_stat->features);
}

EXPORT_SYMBOL_GPL(ged_gpu_memsys_feature_enable);

GED_ERROR ged_gpu_memsys_exit(void)
{
	/*Do Nothing*/
	return GED_OK;
}

#if !IS_ENABLED(CONFIG_MTK_GPU_LEGACY) /* MTK_GPU_EB_SUPPORT */
GED_ERROR ged_gpu_aximon_init(void)
{
	struct device_node *chosen_node;
	const char *name = NULL;
	int ret;

	/* get mtk_fabric_hrt_debug device node from dts */
	chosen_node = of_find_node_by_path("/chosen");
	if (chosen_node) {
		ret = of_property_read_string_index(chosen_node, "mtk_fabric_hrt_debug", 0, &name);
		if (!ret && (!strncmp("on", name, sizeof("on"))))
			g_hrt_debug_enabled = true;
		else
			g_hrt_debug_enabled = false;
	}

	if (g_hrt_debug_enabled)
		gpufreq_set_mfgsys_config(CONFIG_AXI_MON, FEAT_ENABLE);

	GED_LOGI("[GPU_DEBUG_UTIL]%s aximonitor init status : %u\n",
		__func__, g_hrt_debug_enabled);

	return GED_OK;
}

GED_ERROR ged_gpu_aximon_exit(void)
{
	/*Do Nothing*/
	return GED_OK;
}
#endif /* MTK_GPU_EB_SUPPORT */
#endif /* MTK_GPU_MEMSYS_UTIL */

