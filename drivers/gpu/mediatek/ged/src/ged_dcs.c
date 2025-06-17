// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

#include <ged_base.h>
#include <ged_dcs.h>
#include <ged_dvfs.h>
#include <ged_log.h>
#include "ged_tracepoint.h"
#include "ged_eb.h"

#if defined(CONFIG_MTK_GPUFREQ_V2)
#include <ged_gpufreq_v2.h>
#include <gpufreq_v2.h>
#else
#include <ged_gpufreq_v1.h>
#endif /* CONFIG_MTK_GPUFREQ_V2 */

static unsigned int g_dcs_enable;
static unsigned int g_dcs_support;
static unsigned int g_dcs_opp_setting;
static struct mutex g_DCS_lock;

int g_cur_core_num;
int g_max_core_num;
int g_avail_mask_num;
int g_virtual_opp_num;
static int g_dcs_stress;
unsigned int g_fix_core_num;
unsigned int g_fix_core_mask;
bool g_setting_dirty;

int g_lowpwr_mode;

// adjust dcs_performance
static unsigned int g_adjust_dcs_support;
static unsigned int g_adjust_dcs_ratio_th; //freq max/min threshold (ex:20(2 times))
static unsigned int g_adjust_dcs_fr_cnt;   // store frame cnt
static unsigned int g_adjust_dcs_non_dcs_th; // none dcs threshold (ex: 20(20%))

// major_min_core
unsigned int g_major_min_core;
unsigned int g_major_option;

// gov core mask
unsigned int g_has_gov_support;
unsigned int g_gov_enable;
unsigned int g_gov_src;


struct gpufreq_core_mask_info *g_core_mask_table;
struct gpufreq_core_mask_info *g_avail_mask_table;

/* Function Pointer hooked by DDK to scale cores */
int (*ged_dvfs_set_gpu_core_mask_fp)(u64 core_mask) = NULL;
EXPORT_SYMBOL(ged_dvfs_set_gpu_core_mask_fp);

static void _dcs_init_core_mask_table(void)
{
	int i = 0;
	struct gpufreq_core_mask_info *mask_table;

	/* init core mask table */
	g_max_core_num = gpufreq_get_core_num();
	g_cur_core_num = g_max_core_num;
	mask_table = gpufreq_get_core_mask_table();
	g_core_mask_table = kcalloc(g_max_core_num,
		sizeof(struct gpufreq_core_mask_info), GFP_KERNEL);

	if (!g_core_mask_table || !mask_table) {
		GED_LOGE("Failed to query core mask from gpufreq");
		g_dcs_enable = 0;
		return;
	}

	for (i = 0; i < g_max_core_num; i++)
		*(g_core_mask_table + i) = *(mask_table + i);


	for (i = 0; i < g_max_core_num; i++) {
		GED_LOGD("[%02d*] MC0%d : 0x%llX",
			i, g_core_mask_table[i].num, g_core_mask_table[i].mask);
	}


	// return mask_table;
}

GED_ERROR ged_dcs_init_platform_info(void)
{
	struct device_node *dcs_node = NULL;
	int opp_setting = 0;
	int ret = GED_OK;
	g_fix_core_num = 0;
	g_fix_core_mask = 0;
	g_setting_dirty = false;

	g_adjust_dcs_ratio_th = 20;
	g_adjust_dcs_fr_cnt = 20;
	g_adjust_dcs_non_dcs_th = 20;

	g_lowpwr_mode = 0;

	mutex_init(&g_DCS_lock);

	dcs_node = of_find_compatible_node(NULL, NULL, "mediatek,gpu_dcs");
	if (unlikely(!dcs_node)) {
		GED_LOGE("Failed to find gpu_dcs node");
		return ret;
	}

	of_property_read_u32(dcs_node, "dcs-policy-support", &g_dcs_support);
	of_property_read_u32(dcs_node, "virtual-opp-support", &g_dcs_opp_setting);

	opp_setting = g_dcs_opp_setting;

	if (!g_dcs_support) {
		GED_LOGE("DCS policy not support");
		return ret;
	}

	if(g_dcs_opp_setting == 0) {
		g_max_core_num = gpufreq_get_core_num();
		g_dcs_opp_setting = 1 << (g_max_core_num - 1);
		opp_setting = g_dcs_opp_setting;
		GED_LOGI("DCS repair opp setting %x", g_dcs_opp_setting);
	}

	while (opp_setting) {
		g_avail_mask_num += opp_setting & 1;
		opp_setting >>= 1;
	}
	g_dcs_enable = 1;

	GED_LOGI("g_dcs_enable: %u,  g_dcs_opp_setting: 0x%X",
			g_dcs_enable, g_dcs_opp_setting);

	_dcs_init_core_mask_table();

	g_adjust_dcs_support = 1;
	dcs_init_dts_with_eb();
	dcs_set_major_min(6, 0);

	return ret;
}

void ged_dcs_exit(void)
{
	mutex_destroy(&g_DCS_lock);

	kfree(g_core_mask_table);
	kfree(g_avail_mask_table);
}

struct gpufreq_core_mask_info *dcs_get_avail_mask_table(void)
{
	int i, j = 0;
	u32 iter = 0;

	if (!g_dcs_opp_setting)
		return g_core_mask_table;

	if (g_avail_mask_table)
		return g_avail_mask_table;

	/* mapping selected core mask */
	g_avail_mask_table = kcalloc(g_avail_mask_num,
		sizeof(struct gpufreq_core_mask_info), GFP_KERNEL);

	iter = 1 << (g_max_core_num - 1);

	for (i = 0; i < g_max_core_num; i++) {
		if (g_dcs_opp_setting & iter) {
			*(g_avail_mask_table + j) = *(g_core_mask_table + i);
			j++;
		}
		iter >>= 1;
	}

	for (i = 0; i < g_avail_mask_num; i++) {
		GED_LOGD("[%02d*] MC0%d : 0x%llX",
			i, g_avail_mask_table[i].num, g_avail_mask_table[i].mask);
	}

	return g_avail_mask_table;
}

void dcs_init_dts_with_eb(void)
{
	static bool has_init = false;

	if (!has_init)
	{
		struct fdvfs_ipi_data ipi_data = {0};
		int ret = 0;

		ipi_data.u.set_para.arg[0] = GPUFDVFS_IPI_SET_DTS_INIT_DCS;
		ipi_data.u.set_para.arg[1] = g_dcs_support;
		ipi_data.u.set_para.arg[2] = g_dcs_opp_setting;
		ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_CONFIG, &ipi_data);

		if (ret)
			GED_LOGD("%s err:%d\n", __func__, ret);
		else {
			g_has_gov_support = ipi_data.u.set_para.arg[0];
			g_gov_enable = ipi_data.u.set_para.arg[1];
			has_init = true;
		}
	}
}

int dcs_get_dcs_opp_setting(void)
{
	return g_dcs_opp_setting;
}

int dcs_get_cur_core_num(void)
{
	if (is_fdvfs_enable() & POLICY_MODE_V2) {
		if (dcs_get_gov_enable()) {
			unsigned int gov_mask_num = mtk_gpueb_sysram_read(fdvfs_v2_table[DCS_GOV_CORE_NUM].addr);

			if (gov_mask_num > 0)
				return gov_mask_num;
		}
	}
	return g_cur_core_num;
}

int dcs_get_max_core_num(void)
{
	return g_max_core_num;
}

int dcs_get_avail_mask_num(void)
{
	return g_avail_mask_num;
}

int dcs_set_core_mask(unsigned int core_mask, unsigned int core_num, int commit_type)
{
	int ret = GED_OK;

	if (is_fdvfs_enable() & POLICY_MODE_V2) {
		mtk_gpueb_sysram_write(fdvfs_v2_table[GPU_DEBUG].addr, g_fix_core_num);
		mtk_gpueb_sysram_write(fdvfs_v2_table[GPU_LOWPWR_ENABLE].addr, g_lowpwr_mode);
		if (dcs_get_gov_enable())
			return ret;
	}

	mutex_lock(&g_DCS_lock);

	if ((!g_dcs_enable || g_cur_core_num == core_num) && !g_setting_dirty)
		goto done_unlock;

	if (!g_core_mask_table) {
		ret = GED_ERROR_FAIL;
		GED_LOGE("null core mask table");
		goto done_unlock;
	}

	if (g_fix_core_num > 0 && commit_type != GED_DVFS_LOWPWR_COMMIT)
		ged_dvfs_set_gpu_core_mask_fp(g_fix_core_mask);
	else
		ged_dvfs_set_gpu_core_mask_fp(core_mask);

	g_setting_dirty = g_dcs_stress;
	g_cur_core_num = core_num;

	trace_GPU_DVFS__Policy__DCS(g_max_core_num, g_cur_core_num, g_fix_core_num, g_lowpwr_mode);
	trace_GPU_DVFS__Policy__DCS__Detail(core_mask);
	/* TODO: set return error */
	if (ret) {
		GED_LOGE("Failed to set core_mask: 0x%x, core_num: %u", core_mask, core_num);
		goto done_unlock;
	}

done_unlock:
	mutex_unlock(&g_DCS_lock);
	return ret;
}

int dcs_set_fix_core_mask(gov_mask_config_t config, unsigned int core_mask)
{
	int ret = GED_OK;

	if (dcs_get_gov_enable() && (is_fdvfs_enable() & POLICY_MODE_V2))
		return ret;

	mutex_lock(&g_DCS_lock);

	if (ged_dvfs_set_gpu_core_mask_fp != NULL)
		ged_dvfs_set_gpu_core_mask_fp(core_mask);
	else
		ret = GED_ERROR_FAIL;

	g_setting_dirty = true;

	trace_GPU_DVFS__Policy__DCS(config, g_cur_core_num, g_fix_core_num, g_lowpwr_mode);
	trace_GPU_DVFS__Policy__DCS__Detail(core_mask);
	/* TODO: set return error */
	if (ret) {
		GED_LOGE("Failed to set core_mask: 0x%x %d", core_mask, config);
		goto done_unlock;
	}

done_unlock:
	mutex_unlock(&g_DCS_lock);
	return ret;
}


int dcs_set_fix_num(unsigned int core_num)
{
	int ret = GED_OK;
	int i = 0;

	mutex_lock(&g_DCS_lock);
	if (g_fix_core_num != core_num) {
		g_setting_dirty = true;

		if (!g_core_mask_table)
			_dcs_init_core_mask_table();

		if (!g_core_mask_table) {
			ret = GED_ERROR_FAIL;
			pr_info("init core mask table fail");
			goto done_unlock;
		}

		if (core_num > 0 &&
			core_num <= g_max_core_num &&
			g_max_core_num == DCS_DEBUG_MAX_CORE) {
			g_fix_core_num = core_num;
			for (i = 0; i < g_max_core_num; i++) {
				if (g_fix_core_num == g_core_mask_table[i].num) {
					g_fix_core_mask = g_core_mask_table[i].mask;
					pr_info("g_debug setting %X", g_fix_core_num);
				}
			}
		} else {
			g_fix_core_num =  0;
			g_fix_core_mask = 0;
			pr_info("g_debug reset %X", g_fix_core_num);
		}
		if (is_fdvfs_enable() & POLICY_MODE_V2) {
			mtk_gpueb_sysram_write(fdvfs_v2_table[GPU_DEBUG].addr, g_fix_core_num);
		}
	}

done_unlock:
	mutex_unlock(&g_DCS_lock);
	return ret;
}

void dcs_fix_reset(void)
{
	g_setting_dirty = true;
	g_fix_core_num =  0;
	g_fix_core_mask = 0;
	pr_info("g_debug timer reset  %X", g_fix_core_num);

	if (is_fdvfs_enable() & POLICY_MODE_V2) {
		mtk_gpueb_sysram_write(fdvfs_v2_table[GPU_DEBUG].addr, g_fix_core_num);
	}
}

unsigned int dcs_get_fix_num(void)
{
	return g_fix_core_num;
}
unsigned int dcs_get_fix_mask(void)
{
	return g_fix_core_mask;
}

void dcs_set_setting_dirty(void)
{
	g_setting_dirty = true;
}

bool dcs_get_setting_dirty(void)
{
	return g_setting_dirty;
}

int dcs_restore_max_core_mask(void)
{
	int ret = GED_OK;

	if (is_fdvfs_enable() & POLICY_MODE_V2) {
		mtk_gpueb_sysram_write(fdvfs_v2_table[GPU_DEBUG].addr, g_fix_core_num);
		mtk_gpueb_sysram_write(fdvfs_v2_table[GPU_LOWPWR_ENABLE].addr, g_lowpwr_mode);
		if (dcs_get_gov_enable())
			return ret;
	}

	mutex_lock(&g_DCS_lock);

	if ((!g_dcs_enable || g_cur_core_num == g_max_core_num) && !g_setting_dirty)
		goto done_unlock;

	if (g_core_mask_table == NULL) {
		ret = GED_ERROR_FAIL;
		GED_LOGE("null core mask table");
		goto done_unlock;
	}

	if (g_fix_core_num > 0)
		ged_dvfs_set_gpu_core_mask_fp(g_fix_core_mask);
	else
		ged_dvfs_set_gpu_core_mask_fp(g_core_mask_table[0].mask);

	g_cur_core_num = g_max_core_num;
	trace_GPU_DVFS__Policy__DCS(g_max_core_num, g_cur_core_num, g_fix_core_num, g_lowpwr_mode);
	trace_GPU_DVFS__Policy__DCS__Detail(g_core_mask_table[0].mask);

done_unlock:
	mutex_unlock(&g_DCS_lock);
	return ret;
}

int is_dcs_enable(void)
{
	return g_dcs_enable;
}

void dcs_enable(int enable)
{
	if (g_core_mask_table == NULL)
		return;

	mutex_lock(&g_DCS_lock);

	g_setting_dirty = true;
	if (enable) {
		g_dcs_enable = enable;
		if (dcs_get_gov_enable()) {
			ged_eb_dvfs_task(EB_DCS_CORE_NUM, g_max_core_num);

			ged_dvfs_set_gpu_core_mask_fp(g_core_mask_table[0].mask);
			g_cur_core_num = g_max_core_num;
			trace_GPU_DVFS__Policy__DCS(g_max_core_num, g_cur_core_num, g_fix_core_num, g_lowpwr_mode);
			trace_GPU_DVFS__Policy__DCS__Detail(g_core_mask_table[0].mask);
		}
	} else {
		if (g_fix_core_num > 0)
			ged_dvfs_set_gpu_core_mask_fp(g_fix_core_mask);
		else
			ged_dvfs_set_gpu_core_mask_fp(g_core_mask_table[0].mask);
		g_cur_core_num = g_max_core_num;
		g_dcs_enable = 0;
		trace_GPU_DVFS__Policy__DCS(g_max_core_num, g_cur_core_num, g_fix_core_num, g_lowpwr_mode);
		trace_GPU_DVFS__Policy__DCS__Detail(g_core_mask_table[0].mask);
	}

	// write dcs related data to sysram for EB dvfs
	ged_eb_dvfs_task(EB_DCS_ENABLE, ged_gpufreq_get_dcs_sysram());
	ged_eb_dvfs_task(EB_REINIT, 0);

	mutex_unlock(&g_DCS_lock);
}
EXPORT_SYMBOL(dcs_enable);

int dcs_get_dcs_stress(void)
{
	return g_dcs_stress;
}

void dcs_set_dcs_stress(int enable)
{
	mutex_lock(&g_DCS_lock);
	g_dcs_stress = enable;
	g_setting_dirty = true;

	if (is_fdvfs_enable() & POLICY_MODE_V2) {
		struct fdvfs_ipi_data ipi_data = {0};
		int ret = 0;

		ipi_data.u.set_para.arg[0] = GPUFDVFS_IPI_SET_DCS_STRESS;
		ipi_data.u.set_para.arg[1] = enable;
		ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_CONFIG, &ipi_data);

		if (ret)
			GED_LOGD("%s err:%d\n", __func__, ret);
	}
	mutex_unlock(&g_DCS_lock);
}

// dcs adjust reference
void dcs_set_adjust_support(unsigned int val)
{
	mutex_lock(&g_DCS_lock);
	g_adjust_dcs_support = val;
	mutex_unlock(&g_DCS_lock);
}
EXPORT_SYMBOL(dcs_set_adjust_support);

void dcs_set_adjust_ratio_th(unsigned int val)
{
	mutex_lock(&g_DCS_lock);
	g_adjust_dcs_ratio_th = val;
	mutex_unlock(&g_DCS_lock);
}
EXPORT_SYMBOL(dcs_set_adjust_ratio_th);

void dcs_set_adjust_fr_cnt(unsigned int val)
{
	mutex_lock(&g_DCS_lock);
	g_adjust_dcs_fr_cnt = val;
	mutex_unlock(&g_DCS_lock);
}
EXPORT_SYMBOL(dcs_set_adjust_fr_cnt);

void dcs_set_adjust_non_dcs_th(unsigned int val)
{
	mutex_lock(&g_DCS_lock);
	g_adjust_dcs_non_dcs_th = val;
	mutex_unlock(&g_DCS_lock);
}
EXPORT_SYMBOL( dcs_set_adjust_non_dcs_th);

unsigned int dcs_get_adjust_support(void)
{
	return g_adjust_dcs_support;
}
EXPORT_SYMBOL(dcs_get_adjust_support);

unsigned int dcs_get_adjust_ratio_th(void)
{
	return g_adjust_dcs_ratio_th;
}
EXPORT_SYMBOL(dcs_get_adjust_ratio_th);

unsigned int dcs_get_adjust_fr_cnt(void)
{
	return g_adjust_dcs_fr_cnt;
}
EXPORT_SYMBOL(dcs_get_adjust_fr_cnt);

unsigned int dcs_get_adjust_non_dcs_th(void)
{
	return g_adjust_dcs_non_dcs_th;
}
EXPORT_SYMBOL(dcs_get_adjust_non_dcs_th);

unsigned int dcs_get_major_min(void) {
	return g_major_min_core;
}

void dcs_set_major_min(unsigned int num, unsigned int option){
	struct fdvfs_ipi_data ipi_data = {0};
	int ret = 0;

	if (num <= g_max_core_num) {
		g_major_min_core = num;
		g_major_option = option;

		ipi_data.u.set_para.arg[0] = GPUFDVFS_IPI_SET_MAJOR_MIN_CORE;
		ipi_data.u.set_para.arg[1] = num;
		ipi_data.u.set_para.arg[2] = option;
		ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_CONFIG, &ipi_data);

		if (ret)
			GED_LOGD("%s err:%d\n", __func__, ret);
	}
}

ssize_t get_get_major_min_dump(char *buf, int sz, ssize_t pos)
{
	int length;

	length = scnprintf(buf + pos, sz - pos, "min:%u opt:%u", g_major_min_core, g_major_option);
	pos += length;

	return pos;
}

unsigned int dcs_get_gov_support(void) {
	return g_has_gov_support;
}

unsigned int dcs_get_gov_enable(void) {
	if (g_dcs_enable)
		return g_gov_enable;
	else
		return 0;
}

void dcs_set_gov_enable(unsigned int enable, unsigned int src)
{
	struct fdvfs_ipi_data ipi_data = {0};
	int ret = 0;

	ipi_data.u.set_para.arg[0] = GPUFDVFS_IPI_SET_GOV_ENABLE;
	ipi_data.u.set_para.arg[1] = enable;
	ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_CONFIG, &ipi_data);

	if (ret) {
		GED_LOGD("%s err:%d\n", __func__, ret);
		return;
	}

	g_has_gov_support = ipi_data.u.set_para.arg[0];
	g_gov_enable = ipi_data.u.set_para.arg[1];
	g_setting_dirty = true;
	g_gov_src = src;

	if (dcs_get_gov_enable()) {

		if (g_core_mask_table == NULL)
			return;

		mutex_lock(&g_DCS_lock);

		ged_dvfs_set_gpu_core_mask_fp(g_core_mask_table[0].mask);
		g_cur_core_num = g_max_core_num;
		trace_GPU_DVFS__Policy__DCS(g_max_core_num, g_cur_core_num, g_fix_core_num, g_lowpwr_mode);
		trace_GPU_DVFS__Policy__DCS__Detail(g_core_mask_table[0].mask);
		ged_eb_dvfs_task(EB_DCS_CORE_NUM, g_max_core_num);

		mutex_unlock(&g_DCS_lock);
	}
}

unsigned int dcs_get_desire_mask(void)
{
	if (is_fdvfs_enable() & POLICY_MODE_V2)
		return mtk_gpueb_sysram_read(fdvfs_v2_table[GOV_DESIRE_MASK].addr);

	return 0;
}

ssize_t get_get_gov_support_dump(char *buf, int sz, ssize_t pos)
{
	int length;

	length = scnprintf(buf + pos, sz - pos,
			"support:%u enable:%u(%u)(%u) (enable will restore to 0 if platform not support)\n",
			dcs_get_gov_support(), dcs_get_gov_enable(), g_gov_enable, ged_dvfs_get_gov_mask_enable());

	pos += length;


	if (is_fdvfs_enable() & POLICY_MODE_V2) {
		length = scnprintf(buf + pos, sz - pos,
			"%u", mtk_gpueb_sysram_read(fdvfs_v2_table[DCS_GOV_CORE_NUM].addr));
		pos += length;
	}

	length = scnprintf(buf + pos, sz - pos, "(%u), ap(%u) last_src(%u)\n",
		mtk_gpueb_sysram_read(SYSRAM_GPU_EB_DESIRE_FREQ_ID),
		mtk_gpueb_sysram_read(SYSRAM_GPU_EB_DCS_CORE_NUM), g_gov_src);

	pos += length;

	return pos;
}

int dcs_get_lowpwr(void)
{
	return g_lowpwr_mode;
}

void dcs_set_lowpwr(int enable)
{
	if (g_core_mask_table == NULL)
		return;

	if (g_avail_mask_table == NULL)
		return;

	mutex_lock(&g_DCS_lock);
	g_lowpwr_mode = enable;

	if (is_fdvfs_enable() & POLICY_MODE_V2) {
		mtk_gpueb_sysram_write(fdvfs_v2_table[GPU_LOWPWR_ENABLE].addr, g_lowpwr_mode);
		if (dcs_get_gov_enable())
			goto done_unlock;
	}

	if(!g_dcs_enable)
		goto done_unlock;

	if (enable) {
		int	is_fix_dvfs = ged_is_fix_dvfs();
		if (is_fix_dvfs <= 1) {
			if (ged_check_ceil_in_min_working_opp()) {
				int max_avail_idx = g_avail_mask_num - 1;
				if (max_avail_idx >= 0) {
					ged_dvfs_set_gpu_core_mask_fp(g_avail_mask_table[max_avail_idx].mask);
					g_cur_core_num = g_avail_mask_table[max_avail_idx].num;
					trace_GPU_DVFS__Policy__DCS(g_max_core_num, g_cur_core_num,
												g_fix_core_num, g_lowpwr_mode);
					trace_GPU_DVFS__Policy__DCS__Detail(g_avail_mask_table[max_avail_idx].mask);
					trace_tracing_mark_write(5566, "silence", g_lowpwr_mode);
				}
			} else
				trace_tracing_mark_write(5566, "silence", 2);
		} else
			trace_tracing_mark_write(5566, "silence", 3);
	} else {
		trace_tracing_mark_write(5566, "silence", g_lowpwr_mode);
		g_setting_dirty = true;
	}

done_unlock:
	mutex_unlock(&g_DCS_lock);
}

