// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/version.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/genalloc.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <mtk_gpu_utility.h>
#if defined(CONFIG_MTK_GPUFREQ_V2)
#include <ged_gpufreq_v2.h>
#include <gpufreq_v2.h>
#endif
#include "ged_base.h"
#include "ged_hal.h"
#include "ged_sysfs.h"

#include "ged_dvfs.h"

#include "ged_notify_sw_vsync.h"
#include "ged_kpi.h"
#include "ged_global.h"
#include "ged_dcs.h"
#include "ged_eb.h"
#if defined(MTK_GPU_SLC_POLICY)
#include "ged_gpu_slc.h"
#endif /* MTK_GPU_SLC_POLICY */
#if defined(MTK_GPU_MEMSYS_UTIL)
#include "ged_gpu_memsys.h"
#endif /* MTK_GPU_MEMSYS_UTIL */

static struct kobject *hal_kobj;
extern int stat_mcu_store[][30];
#if defined(MTK_GPU_MEMSYS_UTIL)
static struct kobject *memsys_kobj;
#endif /* MTK_GPU_MEMSYS_UTIL */


int tokenizer(char *pcSrc, int i32len, int *pi32IndexArray, int i32NumToken)
{
	int i = 0;
	int j = 0;
	int head = -1;

	for ( ; i < i32len; i++) {
		if (pcSrc[i] != ' ') {
			if (head == -1)
				head = i;
		} else {
			if (head != -1) {
				pi32IndexArray[j] = head;
				j++;
				if (j == i32NumToken)
					return j;
				head = -1;
			}
			pcSrc[i] = 0;
		}
	}

	if (head != -1) {
		pi32IndexArray[j] = head;
		j++;
		return j;
	}

	return -1;
}

/* MBrain */
static ssize_t opp_logs_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int len = 0, i = 0, cur_idx = 0;
	unsigned int ui32FqCount = g_real_oppfreq_num;
	struct GED_DVFS_OPP_STAT *report = NULL;
	u64 last_ts = 0;

	if (ui32FqCount)
		report = vmalloc(sizeof(struct GED_DVFS_OPP_STAT) * ui32FqCount);

	if ((report != NULL) &&
		ged_dvfs_query_opp_cost(report, ui32FqCount, false, &last_ts) == 0) {
		cur_idx = gpufreq_get_cur_oppidx(TARGET_DEFAULT);
		cur_idx = (cur_idx > g_real_minfreq_idx)? g_real_minfreq_idx: cur_idx;
		len = scnprintf(buf, PAGE_SIZE, "Last TS: %llu\n", last_ts);
		len += scnprintf(buf + len, PAGE_SIZE - len, "GPU Freq(MHz)  Time(ms)\n");

		for (i = 0; i < ui32FqCount; i++) {
			if (i == cur_idx)
				len += scnprintf(buf + len, PAGE_SIZE - len, "*");
			else
				len += scnprintf(buf + len, PAGE_SIZE - len, " ");

			len += scnprintf(buf + len, PAGE_SIZE - len, "%4u    ",
					gpufreq_get_freq_by_idx(TARGET_DEFAULT, i) / 1000);

			/* truncate to ms */
			len += scnprintf(buf + len, PAGE_SIZE - len, "%llu\n", report[i].ui64Active);
		}
	} else {
		len = scnprintf(buf, PAGE_SIZE - len, "Not Supported.\n");
		if (len < 0)
			GED_LOGE("scnprintf failed to write to buffer!\n");
	}

	if (report != NULL)
		vfree(report);

	return len;
}
static KOBJ_ATTR_RO(opp_logs);

static ssize_t gpu_sum_loading_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	u64 sum_loading = 0, sum_delta_time = 0;

	ged_dvfs_query_loading(&sum_loading, &sum_delta_time);

	return scnprintf(buf, PAGE_SIZE, "%llu %llu\n", sum_loading, sum_delta_time);
}
static KOBJ_ATTR_RO(gpu_sum_loading);

static ssize_t gpu_power_state_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	u64 off_time = 0, idle_time = 0, on_time = 0;
	u64 last_ts = 0;

	ged_dvfs_query_power_state_time(&off_time, &idle_time, &on_time, &last_ts);

	return scnprintf(buf, PAGE_SIZE, "last_ts:%llu off: %llu idle: %llu on: %llu\n",
			last_ts, off_time, idle_time, on_time);
}
static KOBJ_ATTR_RO(gpu_power_state);
/* MBrain end */
//-----------------------------------------------------------------------------
static ssize_t total_gpu_freq_level_count_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int ui32FreqLevelCount;

	if (false == mtk_custom_get_gpu_freq_level_count(&ui32FreqLevelCount))
		ui32FreqLevelCount = 0;

	return scnprintf(buf, PAGE_SIZE, "%u\n", ui32FreqLevelCount);
}

static KOBJ_ATTR_RO(total_gpu_freq_level_count);
//-----------------------------------------------------------------------------
static ssize_t custom_boost_gpu_freq_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int ui32BoostGpuFreqLevel = 0;
	char debug_buf[GED_SYSFS_MAX_BUFF_SIZE];

	ui32BoostGpuFreqLevel = ged_dvfs_get_custom_boost_gpu_freq();
	ged_dvfs_get_custom_boost_gpu_freq_info_str(debug_buf, sizeof(debug_buf), 0);

	return scnprintf(buf, PAGE_SIZE, "%u\n%s\n", ui32BoostGpuFreqLevel, debug_buf);
}

static ssize_t custom_boost_gpu_freq_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				mtk_custom_boost_gpu_freq(i32Value);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(custom_boost_gpu_freq);
//-----------------------------------------------------------------------------
extern unsigned int fb_rsf_policy_enable;

static ssize_t fb_rsf_policy_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int pos = 0;

	if (fb_rsf_policy_enable){
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"fb_rsf_policy_enable (%d)\n",fb_rsf_policy_enable);
	}else
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,"fb_rsf_policy is disabled\n");

	return pos;
}

static ssize_t fb_rsf_policy_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{

	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	unsigned int u32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtouint(acBuffer, 0, &u32Value) == 0) {
				if (u32Value > 0) {
					fb_rsf_policy_enable = u32Value;
					ged_eb_dvfs_task(EB_FB_RSF_POLICY_ENABLE, u32Value);
				} else {
					fb_rsf_policy_enable = 0;
					ged_eb_dvfs_task(EB_FB_RSF_POLICY_ENABLE, 0);
				}
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(fb_rsf_policy);
//-----------------------------------------------------------------------------
extern unsigned int ged_npu_hint_enable;

static ssize_t ged_npu_hint_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int pos = 0;

	if (ged_npu_hint_enable){
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"ged_npu_hint_enable (%d)\n",ged_npu_hint_enable);
	}else
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,"ged_npu_hint is disabled\n");

	return pos;
}

static ssize_t ged_npu_hint_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{

	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	unsigned int u32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtouint(acBuffer, 0, &u32Value) == 0) {
				if (u32Value > 0) {
					if (ged_npu_hint_enable != u32Value) {
						ged_npu_hint_enable = u32Value;
						mtk_gpueb_sysram_write(fdvfs_v2_table[GPU_FB_NPU_HINT_MS].addr,u32Value);
						pr_info("SR NPU enable");
					}
				} else {
					ged_npu_hint_enable = 0;
					mtk_gpueb_sysram_write(fdvfs_v2_table[GPU_FB_NPU_HINT_MS].addr,0);
					pr_info("SR NPU disable");
				}
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(ged_npu_hint);
//-----------------------------------------------------------------------------

static ssize_t custom_upbound_gpu_freq_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int ui32UpboundGpuFreqLevel = 0;
	char debug_buf[GED_SYSFS_MAX_BUFF_SIZE];

	ui32UpboundGpuFreqLevel = ged_dvfs_get_custom_ceiling_gpu_freq();
	ged_dvfs_get_custom_ceiling_gpu_freq_info_str(debug_buf, sizeof(debug_buf), 0);

	return scnprintf(buf, PAGE_SIZE, "%u\n%s\n", ui32UpboundGpuFreqLevel, debug_buf);
}

static ssize_t custom_upbound_gpu_freq_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				mtk_custom_upbound_gpu_freq(i32Value);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(custom_upbound_gpu_freq);
//-----------------------------------------------------------------------------
static ssize_t current_freqency_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct GED_DVFS_FREQ_DATA sFreqInfo;

	ged_dvfs_get_gpu_cur_freq(&sFreqInfo);

	return scnprintf(buf, PAGE_SIZE, "%u %lu\n",
		sFreqInfo.ui32Idx, sFreqInfo.ulFreq);
}

static KOBJ_ATTR_RO(current_freqency);
//-----------------------------------------------------------------------------
static ssize_t previous_freqency_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct GED_DVFS_FREQ_DATA sFreqInfo;

	ged_dvfs_get_gpu_pre_freq(&sFreqInfo);

	return scnprintf(buf, PAGE_SIZE, "%u %lu\n",
		sFreqInfo.ui32Idx, sFreqInfo.ulFreq);
}

static KOBJ_ATTR_RO(previous_freqency);
//-----------------------------------------------------------------------------
static ssize_t gpu_utilization_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int loading = 0;
	unsigned int block = 0;
	unsigned int idle = 0;

	mtk_get_gpu_loading(&loading);
	mtk_get_gpu_block(&block);
	mtk_get_gpu_idle(&idle);

	return scnprintf(buf, PAGE_SIZE, "%u %u %u\n", loading, block, idle);
}

static KOBJ_ATTR_RO(gpu_utilization);
//-----------------------------------------------------------------------------
static int32_t _boost_level = -1;
#define MAX_BOOST_DIGITS 10
static ssize_t gpu_boost_level_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", _boost_level);
}

static ssize_t gpu_boost_level_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char str_num[MAX_BOOST_DIGITS];
	long val;

	if (count > 0 && count < MAX_BOOST_DIGITS) {
		if (scnprintf(str_num, MAX_BOOST_DIGITS, "%s", buf)) {
			if (kstrtol(str_num, 10, &val) == 0)
				_boost_level = (int32_t)val;
		}
	}

	return count;
}

static KOBJ_ATTR_RW(gpu_boost_level);
//-----------------------------------------------------------------------------
int ged_dvfs_boost_value(void)
{
	return _boost_level;
}

//-----------------------------------------------------------------------------
#ifdef MTK_GED_KPI
static ssize_t ged_kpi_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	unsigned int fps;
	unsigned int cpu_time;
	unsigned int gpu_time;
	unsigned int response_time;
	unsigned int gpu_remained_time;
	unsigned int cpu_remained_time;
	unsigned int gpu_freq;
	union combineData tmp_multi = {0};

	if (!(is_fdvfs_enable() & POLICY_MODE_V2)) {
		fps = ged_kpi_get_cur_fps();
		cpu_time = ged_kpi_get_cur_avg_cpu_time();
		gpu_time = ged_kpi_get_cur_avg_gpu_time();
		response_time = ged_kpi_get_cur_avg_response_time();
		cpu_remained_time = ged_kpi_get_cur_avg_cpu_remained_time();
		gpu_remained_time = ged_kpi_get_cur_avg_gpu_remained_time();
		gpu_freq = ged_kpi_get_cur_avg_gpu_freq();
	} else {
		tmp_multi = mtk_gpueb_sysram_multi_read(SYSRAM_GPU_KPI_FPS_FREQ);
		fps = tmp_multi.twoVar.var1;
		gpu_freq = tmp_multi.twoVar.var2;
		cpu_time = mtk_gpueb_sysram_read(SYSRAM_GPU_KPI_CPU_TIME);
		gpu_time = mtk_gpueb_sysram_read(SYSRAM_GPU_KPI_GPU_TIME);
		response_time = 0;
		cpu_remained_time = 0;
		gpu_remained_time = 0;
	}


	return scnprintf(buf, PAGE_SIZE, "%u,%u,%u,%u,%u,%u,%u\n",
			fps, cpu_time, gpu_time, response_time,
			cpu_remained_time, gpu_remained_time, gpu_freq);
}

static KOBJ_ATTR_RO(ged_kpi);
#endif /* MTK_GED_KPI */
//-------------------------------------------------------------------------
extern unsigned int early_force_fallback_enable;
static ssize_t early_force_fallback_policy_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{

	int pos = 0;

	if (early_force_fallback_enable)
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
			"early_force_fallback_policy is enabled\n");
	else
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
			"early_force_fallback_policy is disabled\n");

	return pos;
}
static ssize_t early_force_fallback_policy_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value > 0 && i32Value < 256)
					early_force_fallback_enable = 1;
				else
					early_force_fallback_enable = 0;
			}
		}
	}

	return count;
}
static KOBJ_ATTR_RW(early_force_fallback_policy);

//-----------------------------------------------------------------------------
static ssize_t dvfs_margin_value_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int i32DvfsMarginValue;
	int pos = 0;
	int length;

	if (false == mtk_get_dvfs_margin_value(&i32DvfsMarginValue)) {
		i32DvfsMarginValue = 0;
		length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"call mtk_get_dvfs_margin_value false\n");
		pos += length;
	}
	length = scnprintf(buf + pos, PAGE_SIZE - pos,
			"%d\n", i32DvfsMarginValue);
	pos += length;

	return pos;
}

static ssize_t dvfs_margin_value_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				mtk_dvfs_margin_value(i32Value);

			ged_eb_dvfs_task(EB_DBG_CMD, 0);
			ged_eb_dvfs_task(EB_REINIT, 0);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(dvfs_margin_value);
//-----------------------------------------------------------------------------
static ssize_t loading_base_dvfs_step_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int i32StepValue;
	int pos = 0;
	int length;

	if (false == mtk_get_loading_base_dvfs_step(&i32StepValue)) {
		i32StepValue = 0;
		length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"call mtk_get_loading_base_dvfs_step false\n");
		pos += length;
	}
	length = scnprintf(buf + pos, PAGE_SIZE - pos,
			"%x\n", i32StepValue);
	pos += length;

	return pos;
}

static ssize_t loading_base_dvfs_step_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				mtk_loading_base_dvfs_step(i32Value);

			ged_eb_dvfs_task(EB_DBG_CMD, 0);
			ged_eb_dvfs_task(EB_REINIT, 0);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(loading_base_dvfs_step);
//-----------------------------------------------------------------------------
static ssize_t timer_base_dvfs_margin_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int i32DvfsMarginValue;
	int pos = 0;
	int length;

	if (false == mtk_get_timer_base_dvfs_margin(&i32DvfsMarginValue)) {
		i32DvfsMarginValue = 0;
		length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"call mtk_get_timer_base_dvfs_margin false\n");
		pos += length;
	}
	length = scnprintf(buf + pos, PAGE_SIZE - pos,
			"%d\n", i32DvfsMarginValue);
	pos += length;

	return pos;
}

static ssize_t timer_base_dvfs_margin_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				mtk_timer_base_dvfs_margin(i32Value);

			ged_eb_dvfs_task(EB_DBG_CMD, 0);
			ged_eb_dvfs_task(EB_REINIT, 0);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(timer_base_dvfs_margin);


static ssize_t dvfs_loading_mode_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int ui32DvfsLoadingMode;
	int pos = 0;
	int length;

	if (false == mtk_get_dvfs_loading_mode(&ui32DvfsLoadingMode)) {
		ui32DvfsLoadingMode = 0;
		length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"call mtk_get_dvfs_loading_mode false\n");
		pos += length;
	}
	length = scnprintf(buf + pos, PAGE_SIZE - pos,
			"%d\n", ui32DvfsLoadingMode);
	pos += length;

	return pos;
}

static ssize_t dvfs_loading_mode_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				mtk_dvfs_loading_mode(i32Value);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(dvfs_loading_mode);

static ssize_t dvfs_workload_mode_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int ui32DvfsWorkloadMode;
	int pos = 0;
	int length;

	if (false == mtk_get_dvfs_workload_mode(&ui32DvfsWorkloadMode)) {
		ui32DvfsWorkloadMode = 0;
		length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"call mtk_get_dvfs_workload_mode false\n");
		pos += length;
	}

	if (is_fdvfs_enable() & POLICY_MODE_V2)
		ui32DvfsWorkloadMode = mtk_gpueb_sysram_read(fdvfs_v2_table[GPU_EB_WORKLOAD_MODE].addr);

	length = scnprintf(buf + pos, PAGE_SIZE - pos,
			"%d\n", ui32DvfsWorkloadMode);
	pos += length;

	return pos;
}

static ssize_t dvfs_workload_mode_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				mtk_dvfs_workload_mode(i32Value);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(dvfs_workload_mode);

// -----------------------------------------------------------------------------
static ssize_t target_fps_vsync_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct GED_BRIDGE_OUT_HINT_FRAME_INFO infoOut;
	int pos = 0;
	int length;

	ged_kpi_hint_frame_info(&infoOut);
	length = scnprintf(buf + pos, PAGE_SIZE - pos,
			"main_head BQ_ID:%llu FPS_V:%d FPS_gpu:%d  (%d, %d)\n",
			(unsigned long long)infoOut.mainHead_BQ_ID,
			infoOut.mainHead_fps_v, infoOut.mainHead_fps_gpu,
			prom_enable, g_target_fps_vsync);
	pos += length;

	return pos;
}

static ssize_t target_fps_vsync_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	struct GED_BRIDGE_OUT_HINT_FRAME_INFO infoOut;
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;
	int SET_MAIN_HEAD_MASK = 0x1 << 8; // 256

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value & SET_MAIN_HEAD_MASK) {
					ged_kpi_hint_frame_info(&infoOut);
					ged_kpi_set_target_FPS_api(infoOut.mainHead_BQ_ID,
						i32Value & 0xFF, 0);
				} else if (i32Value > 0) {
					ged_kpi_set_target_FPS_api(0, i32Value, 0);
				}
			}
		}
	}

	return count;
}


static KOBJ_ATTR_RW(target_fps_vsync);
//-----------------------------------------------------------------------------

static ssize_t eb_dvfs_policy_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int eb_policy_mode;
	int pos = 0;
	bool ret = true;
	struct fdvfs_ipi_data *ipi_data;

	ipi_data = (struct fdvfs_ipi_data *)ged_alloc_atomic(
		sizeof(struct fdvfs_ipi_data));

	if (!ipi_data) {
		GED_LOGE("ged_alloc_atomic fail!\n");
		return pos;
	}

	memset(ipi_data, 0, sizeof(struct fdvfs_ipi_data));

	eb_policy_mode = is_fdvfs_enable();
	if (eb_policy_dts_flag > 0 && eb_policy_mode & POLICY_MODE_V2)
		pos += scnprintf(buf + pos, PAGE_SIZE - pos, "EB DVFS V2 enable,\n");
	else if (eb_policy_dts_flag > 0 && eb_policy_mode > 0)
		pos += scnprintf(buf + pos, PAGE_SIZE - pos, "EB DVFS enable,\n");
	else
		pos += scnprintf(buf + pos, PAGE_SIZE - pos, "EB DVFS disable,\n");

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"dts (%d), EB_policy_mode(%d),\n\n",
			eb_policy_dts_flag, eb_policy_mode);


	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"================EB Parameter=====================\n");
	ipi_data->cmd = GPUFDVFS_IPI_GET_DEFAULT_POLICY_MODE;
	ret = mtk_get_fastdvfs_mode((void *)ipi_data);

	if (ret) {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"EB_default_flag: %u, ",ipi_data->u.set_para.arg[0]);
	}

	/* 0:eb_flag, 1:dcs, 2:async, 3:real min opp, 4:virtual min opp*/
	ipi_data->cmd = GPUFDVFS_IPI_GET_MODE;
	ret = mtk_get_fastdvfs_mode((void *)ipi_data);

	if (ret) {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"EB_Flag: %u\n",ipi_data->u.set_para.arg[0]);
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"DCS: %u  Async: %u\n",
				ipi_data->u.set_para.arg[1],
				ipi_data->u.set_para.arg[2]);
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"OPP-- Real min: %u  Virtual min: %u\n",
				ipi_data->u.set_para.arg[3],
				ipi_data->u.set_para.arg[4]);
	}

	/* 0:loading mode, 1:workload mode*/
	ipi_data->cmd = GPUFDVFS_IPI_GET_LOADING_MODE;
	ret = mtk_get_fastdvfs_mode((void *)ipi_data);
	if (ret) {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"laoding mode: %u\n", ipi_data->u.set_para.arg[0]);
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"workload mode: %u\n",ipi_data->u.set_para.arg[1]);
	}

	/* 0:avg loading, 1:uhigh bound, 2:ulow bound, 3:high bound, 4:low bound*/
	ipi_data->cmd = GPUFDVFS_IPI_GET_BOUND;
	ret = mtk_get_fastdvfs_mode((void *)ipi_data);

	if (ret) {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"Avg loading: %u\n",ipi_data->u.set_para.arg[0]);
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"Bound-- u_high: %u  high: %u  low: %u  u_low: %u\n",
				ipi_data->u.set_para.arg[1],
				ipi_data->u.set_para.arg[3],
				ipi_data->u.set_para.arg[4],
				ipi_data->u.set_para.arg[2]);
	}

	/* 0:cur margin, 1:margin ceil, 2:margin floor, 3:preserve, 4:debug count*/
	ipi_data->cmd = GPUFDVFS_IPI_GET_MARGIM;
	ret = mtk_get_fastdvfs_mode((void *)ipi_data);

	if (ret) {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"Margin-- ceil: %u  cur: %u  floor: %u\n",
				ipi_data->u.set_para.arg[1],
				ipi_data->u.set_para.arg[0],
				ipi_data->u.set_para.arg[2]);
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"Preserve: %u\n",ipi_data->u.set_para.arg[3]);
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"Debug count: %u\n",ipi_data->u.set_para.arg[4]);
	}

	/* 0:fallback_time, 1:fallback_mode, 2:fallback_win_size*/
	ipi_data->cmd = GPUFDVFS_IPI_GET_FB_TUNE_PARAM;
	ret = mtk_get_fastdvfs_mode((void *)ipi_data);

	if (ret) {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"TUNE_PARAM-- fallback_time: %u  fallback_mode: %u  fallback_win_size: %u\n",
				ipi_data->u.set_para.arg[0],
				ipi_data->u.set_para.arg[1],
				ipi_data->u.set_para.arg[2]);
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"Ignore notify: %u\n",ipi_data->u.set_para.arg[3]);
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"method3 cnt: %u\n",ipi_data->u.set_para.arg[4]);
	}

	/* 0:ultra_low_step_size, 1:ultra_high_step_size, 2:lb_target_mode,
	 * 3:lb_stride_size, 4:loading_win_size_cmd
	 */
	ipi_data->cmd = GPUFDVFS_IPI_GET_LB_TUNE_PARAM;
	ret = mtk_get_fastdvfs_mode((void *)ipi_data);

	if (ret) {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"TUNE_PARAM-- ultra_low_step_size: %u  ultra_high_step_size: %u\n",
				ipi_data->u.set_para.arg[0],
				ipi_data->u.set_para.arg[1]);
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"TUNE_PARAM-- lb_target_mode: %u  lb_stride_size: %u  loading_win_size_cmd: %u\n",
				ipi_data->u.set_para.arg[2],
				ipi_data->u.set_para.arg[3],
				ipi_data->u.set_para.arg[4]);
	}
	if (ipi_data)
		ged_free(ipi_data, sizeof(struct fdvfs_ipi_data));

	return pos;
}

static ssize_t eb_dvfs_policy_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	unsigned int u32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtouint(acBuffer, 0, &u32Value) == 0) {
				if (u32Value > 0) {
					if (u32Value & POLICY_MODE_V2)
						eb_policy_dts_flag = 2;
					else
						eb_policy_dts_flag = 1;
					mtk_set_fastdvfs_mode(u32Value);
				} else {
					eb_policy_dts_flag = 0;
					mtk_set_fastdvfs_mode(POLICY_DISABLE);
				}
			}
		}
	}

	return count;
}
static KOBJ_ATTR_RW(eb_dvfs_policy);

//-----------------------------------------------------------------------------

static ssize_t eb_dvfs_kpi_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int pos = 0, i = 0, j = 0;
	bool ret = true;
	struct fdvfs_ipi_data *ipi_data;
	unsigned int head_num = 0, kpi_num = 0;
	ipi_data = (struct fdvfs_ipi_data *)ged_alloc_atomic(
		sizeof(struct fdvfs_ipi_data));

	if (!ipi_data) {
		GED_LOGE("ged_alloc_atomic fail!\n");
		return pos;
	}

	memset(ipi_data, 0, sizeof(struct fdvfs_ipi_data));

	/*
	  1 : get number of head, kpi
      2 : show head data
      3 : show kpi data
	*/
	ipi_data->cmd = GPUFDVFS_IPI_GET_KPI_DATA;
	ipi_data->u.set_para.arg[0] = GPUFDVFS_KPI_GET_NUM;
	ret = mtk_get_fastdvfs_mode((void *)ipi_data);

	if (ret) {
		head_num = ipi_data->u.set_para.arg[0];
		kpi_num = ipi_data->u.set_para.arg[1];
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"HEAD num: %u, KPI num: %u\n",
					head_num,
					kpi_num);
	}

	if (head_num > 0) {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"-------------------- KPI information ----------------------\n");
		for (i = 0; i < head_num; i++) {
			ipi_data->cmd = GPUFDVFS_IPI_GET_KPI_DATA;
			ipi_data->u.set_para.arg[0] = GPUFDVFS_KPI_GET_HEAD;
			ipi_data->u.set_para.arg[1] = i;
			ret = mtk_get_fastdvfs_mode((void *)ipi_data);
			if (ret) {
				unsigned int kpi_tmp_num = ipi_data->u.set_para.arg[1];
				pos += scnprintf(buf + pos, PAGE_SIZE - pos,
						"HEAD[%d] pid: %u KPI num: %u bqid: %u_%u target FPS: %d\n",
							i,
							ipi_data->u.set_para.arg[0],
							ipi_data->u.set_para.arg[1],
							ipi_data->u.set_para.arg[2],
							ipi_data->u.set_para.arg[3],
							ipi_data->u.set_para.arg[4]);
				if (kpi_tmp_num > 0) {
					for (j = 0; j < kpi_tmp_num; j++) {
						ipi_data->cmd = GPUFDVFS_IPI_GET_KPI_DATA;
						ipi_data->u.set_para.arg[0] = GPUFDVFS_KPI_GET_KPI_FROM_HEAD;
						ipi_data->u.set_para.arg[1] = i;
						ipi_data->u.set_para.arg[2] = j;
						ret = mtk_get_fastdvfs_mode((void *)ipi_data);
						if (ret) {
							pos += scnprintf(buf + pos, PAGE_SIZE - pos,
									"->Frame[%d] pid: %u Frame_id: %u q2done: %u gpu_time: %u ulMask: 0x%X\n",
										j,
										ipi_data->u.set_para.arg[0],
										ipi_data->u.set_para.arg[1],
										ipi_data->u.set_para.arg[2],
										ipi_data->u.set_para.arg[3],
										ipi_data->u.set_para.arg[4]);
						}

					}
				}
				scnprintf(buf + pos, PAGE_SIZE - pos, "\n");
			}
			ipi_data->cmd = GPUFDVFS_IPI_GET_KPI_DATA;
			ipi_data->u.set_para.arg[0] = GPUFDVFS_KPI_GET_HEAD_FPS;
			ipi_data->u.set_para.arg[1] = i;
			ret = mtk_get_fastdvfs_mode((void *)ipi_data);
			if (ret) {
				pos += scnprintf(buf + pos, PAGE_SIZE - pos,
						"HEAD_addtion [%d] fps: %u use: %u candi: %u gpu_target: %u t_last_fps: %d\n",
							i,
							ipi_data->u.set_para.arg[0],
							ipi_data->u.set_para.arg[1],
							ipi_data->u.set_para.arg[2],
							ipi_data->u.set_para.arg[3],
							ipi_data->u.set_para.arg[4]);
				scnprintf(buf + pos, PAGE_SIZE - pos, "\n");
			}

		}
	}

	if (ipi_data)
		ged_free(ipi_data, sizeof(struct fdvfs_ipi_data));

	return pos;
}

static ssize_t eb_dvfs_kpi_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	unsigned int u32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtouint(acBuffer, 0, &u32Value) == 0) {
				GED_LOGE("eb_dvfs_kpi_store %d", u32Value);
			}
		}
	}

	return count;
}
static KOBJ_ATTR_RW(eb_dvfs_kpi);

//-----------------------------------------------------------------------------

static ssize_t v_table_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int pos = 0, i = 0;
	bool ret = true;
	struct fdvfs_ipi_data *ipi_data;
	unsigned int s_num = 0, s_init = 0, rs_num = 0, t_init = 0, ver = 0;
	ipi_data = (struct fdvfs_ipi_data *)ged_alloc_atomic(
		sizeof(struct fdvfs_ipi_data));

	if (!ipi_data) {
		GED_LOGE("ged_alloc_atomic fail!\n");
		return pos;
	}

	memset(ipi_data, 0, sizeof(struct fdvfs_ipi_data));

	ipi_data->cmd = GPUFDVFS_IPI_GET_TABLE_DATA;
	ipi_data->u.set_para.arg[0] = GPUFDVFS_TABLE_GET_NUM;
	ret = mtk_get_fastdvfs_mode((void *)ipi_data);

	if (ret) {
		ver = ipi_data->u.set_para.arg[0];
		s_num = ipi_data->u.set_para.arg[1];
		s_init = ipi_data->u.set_para.arg[2];
		rs_num = ipi_data->u.set_para.arg[3];
		t_init = ipi_data->u.set_para.arg[4];
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"ver: %u, s_num: %u s_init:%d rs_num: %u t_init:%u\n",
					ver, s_num, s_init, rs_num, t_init);
	}

	if (s_num > 0) {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"\n%-3s %-5s %-5s %-5s %-5s %-5s %-5s %-5s %-5s %-5s %-5s\n",
				   "idx", "t_f", "vs_f", "rs_f", "r_opp", "c_num",
				          "add_1", "add_2", "add_3", "add_4", "add_5");
		for (i = 0; i < s_num; i++) {
			ipi_data->cmd = GPUFDVFS_IPI_GET_TABLE_DATA;
			ipi_data->u.set_para.arg[0] = GPUFDVFS_TABLE_GET_COL_1;
			ipi_data->u.set_para.arg[1] = i;
			ret = mtk_get_fastdvfs_mode((void *)ipi_data);
			if (ret) {
				pos += scnprintf(buf + pos, PAGE_SIZE - pos,
						"%-3d %-5u %-5u %-5u %-5u %-5u",
							i,
							ipi_data->u.set_para.arg[0] / 1000,
							ipi_data->u.set_para.arg[1] / 1000,
							ipi_data->u.set_para.arg[2] / 1000,
							ipi_data->u.set_para.arg[3],
							ipi_data->u.set_para.arg[4]);
			}
			ipi_data->cmd = GPUFDVFS_IPI_GET_TABLE_DATA;
			ipi_data->u.set_para.arg[0] = GPUFDVFS_TABLE_GET_COL_2;
			ipi_data->u.set_para.arg[1] = i;
			ret = mtk_get_fastdvfs_mode((void *)ipi_data);
			if (ret) {
				pos += scnprintf(buf + pos, PAGE_SIZE - pos,
						" %-5u %-5u %-5u %-5u %-5u\n",
							ipi_data->u.set_para.arg[0],
							ipi_data->u.set_para.arg[1],
							ipi_data->u.set_para.arg[2],
							ipi_data->u.set_para.arg[3],
							ipi_data->u.set_para.arg[4]);
				scnprintf(buf + pos, PAGE_SIZE - pos, "\n");
			}
		}
	}

	if (ipi_data)
		ged_free(ipi_data, sizeof(struct fdvfs_ipi_data));

	return pos;
}

static ssize_t v_table_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	unsigned int u32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtouint(acBuffer, 0, &u32Value) == 0) {
				GED_LOGE("v_table %d", u32Value);
			}
		}
	}

	return count;
}
static KOBJ_ATTR_RW(v_table);

//-----------------------------------------------------------------------------


static ssize_t frame_base_optimize_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int pos = 0;
	int length;

	length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"%d\n", g_ged_frame_base_optimize);

	pos += length;

	return pos;
}

static ssize_t frame_base_optimize_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				g_ged_frame_base_optimize = i32Value;
		}
	}

	return count;

}
static KOBJ_ATTR_RW(frame_base_optimize);

//-----------------------------------------------------------------------------

static ssize_t pre_fence_chk_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int pos = 0;
	int length;

	length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"%d\n", g_ged_pre_fence_chk);

	pos += length;

	return pos;
}

static ssize_t pre_fence_chk_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				g_ged_pre_fence_chk = i32Value;
		}
	}

	return count;

}
static KOBJ_ATTR_RW(pre_fence_chk);

//-----------------------------------------------------------------------------

static struct notifier_block ged_fb_notifier;

static int ged_fb_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int blank;

	/* If we aren't interested in this event, skip it immediately ... */
	if (event != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_UNBLANK:
		g_ui32EventStatus |= GED_EVENT_LCD;
		ged_dvfs_probe_signal(GED_GAS_SIGNAL_EVENT);
		break;
	case FB_BLANK_POWERDOWN:
		g_ui32EventStatus &= ~GED_EVENT_LCD;
		ged_dvfs_probe_signal(GED_GAS_SIGNAL_EVENT);
		break;
	default:
		break;
	}

	return 0;
}

struct ged_event_change_entry_t {
	ged_event_change_fp callback;
	void *private_data;
	char name[128];
	struct list_head sList;
};

static struct {
	struct mutex lock;
	struct list_head listen;
} g_ged_event_change = {
	.lock     = __MUTEX_INITIALIZER(g_ged_event_change.lock),
	.listen   = LIST_HEAD_INIT(g_ged_event_change.listen),
};

bool mtk_register_ged_event_change(const char *name,
	ged_event_change_fp callback, void *private_data)
{
	struct ged_event_change_entry_t *entry = NULL;

	entry = kmalloc(sizeof(struct ged_event_change_entry_t), GFP_KERNEL);
	if (entry == NULL)
		return false;

	entry->callback = callback;
	entry->private_data = private_data;
	strncpy(entry->name, name, sizeof(entry->name) - 1);
	entry->name[sizeof(entry->name) - 1] = 0;
	INIT_LIST_HEAD(&entry->sList);

	mutex_lock(&g_ged_event_change.lock);

	list_add(&entry->sList, &g_ged_event_change.listen);

	mutex_unlock(&g_ged_event_change.lock);

	return true;
}

bool mtk_unregister_ged_event_change(const char *name)
{
	struct list_head *pos, *head;
	struct ged_event_change_entry_t *entry = NULL;

	mutex_lock(&g_ged_event_change.lock);

	head = &g_ged_event_change.listen;
	list_for_each(pos, head) {
		entry = list_entry(pos, struct ged_event_change_entry_t, sList);
		if (strncmp(entry->name, name, sizeof(entry->name) - 1) == 0)
			break;
		entry = NULL;
	}

	if (entry) {
		list_del(&entry->sList);
		kfree(entry);
	}

	mutex_unlock(&g_ged_event_change.lock);

	return true;
}

void mtk_ged_event_notify(int events)
{
	struct list_head *pos, *head;
	struct ged_event_change_entry_t *entry = NULL;

	mutex_lock(&g_ged_event_change.lock);

	head = &g_ged_event_change.listen;
	list_for_each(pos, head) {
		entry = list_entry(pos, struct ged_event_change_entry_t, sList);
		entry->callback(entry->private_data, events);
	}

	mutex_unlock(&g_ged_event_change.lock);
}
//------------------------------------------------------------------------------
extern unsigned int force_loading_based_enable;
static ssize_t force_loading_base_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{

	int pos = 0;
	int margin = 0;
	int force_loading_base = 0;

	if (force_loading_based_enable) {
		force_loading_base = force_loading_based_enable >> 8;
		if (force_loading_base == 1)
			pos += scnprintf(buf + pos, PAGE_SIZE - pos,"force_loading_base is enabled\n");
		else if (force_loading_base == 2){
			margin =  force_loading_based_enable&0xFF;
			pos += scnprintf(buf + pos, PAGE_SIZE - pos,"MFRC dvfs policy is enabled , MFRC dvfs margin %d\n",margin);
		}
	}else
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,"force_loading_base & MFRC dvfs policy is disabled\n");

	return pos;
}
static ssize_t force_loading_base_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value > 0 && i32Value < 1000) {
					force_loading_based_enable = i32Value;
					ged_eb_dvfs_task(EB_FB_MFRC_POLICY_ENABLE, i32Value);
				} else {
					force_loading_based_enable = 0;
					ged_eb_dvfs_task(EB_FB_MFRC_POLICY_ENABLE, i32Value);
				}
			}
		}
	}

	return count;
}
static KOBJ_ATTR_RW(force_loading_base);
//------------------------------------------------------------------------------

static ssize_t gpu_fps_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{

	int pos = 0;

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,"%d\n", dump_gpu_fps_table());

	return pos;
}
static ssize_t gpu_fps_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	unsigned int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				update_gpu_fps_table(i32Value);
		}
	}
	return count;
}
static KOBJ_ATTR_RW(gpu_fps);

static ssize_t default_fps_margin_support_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", ignore_fpsgo_enable);
}

static ssize_t default_fps_margin_support_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;
	struct fdvfs_ipi_data ipi_data = {0};
	int ret = 0;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value > 0 && i32Value < 256)
					ignore_fpsgo_enable = 1;
				else
					ignore_fpsgo_enable = 0;

				ipi_data.u.set_para.arg[0] = GPUFDVFS_IPI_SET_USE_DEFAULT_MAGIN_ENABLE;
				ipi_data.u.set_para.arg[1] = ignore_fpsgo_enable;

				ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_CONFIG, &ipi_data);
				if (ret)
					GED_LOGD("%s err:%d\n", __func__, ret);
			}
		}
	}
	return count;
}
static KOBJ_ATTR_RW(default_fps_margin_support);

//-----------------------------------------------------------------------------
#if IS_ENABLED(CONFIG_MTK_GPUFREQ_V2) /* GED_DCS_POLICY */
static ssize_t dcs_mode_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct gpufreq_core_mask_info *avail_mask_table;
	int avail_mask_num = 0;
	int dcs_enable = 0;
	int mode = 0;
	int pos = 0;
	int i = 0;

	dcs_enable = is_dcs_enable();

	avail_mask_table = dcs_get_avail_mask_table();
	avail_mask_num = dcs_get_avail_mask_num();

	if (dcs_enable) {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"DCS Policy is enable\n");
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"Current in use core num: %d\n", dcs_get_cur_core_num());
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"Available max core num: %d\n",	dcs_get_max_core_num());
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"T enable: %d (0:disable 1:enable 2:enable with log)\n",
					dcs_get_dcs_stress());
		dcs_debug();
	} else {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"DCS Policy is disabled\n");
	}
	/* User Defined DCS Core num */
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
		"====================================\n");
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
		"Enable DCS with user-defined mode with min Available Freq Code:\n");
	for (i = 1; i < avail_mask_num; i++) {
		mode += (1 << (avail_mask_table[i].num));
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[%d] Achieve %d/%d Freq --> %d\n", i, avail_mask_table[i].num,
				avail_mask_table[0].num, mode);
	}
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
		"====================================\n");
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
		"Please echo [the min Code you want] for enable\n");
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
		"(Echo 0 for disable)\n");

	return pos;
}

static ssize_t dcs_mode_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;
	int mode = 0, i = 0;
	unsigned int ud_mask_bit = 0;
	struct gpufreq_core_mask_info *avail_mask_table;
	int avail_mask_num = 0;

	avail_mask_table = dcs_get_avail_mask_table();
	avail_mask_num = dcs_get_avail_mask_num();

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value <= 0) {
					dcs_enable(0);
					return count;
				}
				ud_mask_bit = (i32Value >> 1);
				for (i = 1; i < avail_mask_num; i++) {
					mode += (1 << (avail_mask_table[i].num));
					if (i32Value == mode) {
						ged_set_ud_mask_bit(ud_mask_bit);
						break;
					}
				}
				dcs_enable(1);
			}
		}
	}
	return count;
}

static KOBJ_ATTR_RW(dcs_mode);

static ssize_t dcs_major_min_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char debug_buf[GED_SYSFS_MAX_BUFF_SIZE];

	get_get_major_min_dump(debug_buf, sizeof(debug_buf), 0);

	return scnprintf(buf, PAGE_SIZE, "%s\n", debug_buf);
}

static ssize_t dcs_major_min_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		int ret;
		int major, option;

		ret = sscanf(buf, "%2d %2d", &major, &option);
		if (ret == 2) {
			if (major < 0 || option < 0)
				return count;
		} else {
			ret = sscanf(buf, "%d", &major);
			if (ret == 1) {
				if (major < 0)
					return count;
				option = 0;
			} else
				return count;
		}
		dcs_set_major_min(major, option);

	}

	return count;
}
static KOBJ_ATTR_RW(dcs_major_min);

static ssize_t dcs_stress_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", dcs_get_dcs_stress());
}

static ssize_t dcs_stress_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value <= 0)
					dcs_set_dcs_stress(0);
				else
					dcs_set_dcs_stress(i32Value);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(dcs_stress);

static ssize_t dcs_adjust_support_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "en:%d, Mm:%d, fr:%d, nondcs:%d\n",
						  dcs_get_adjust_support(),
						  dcs_get_adjust_ratio_th(),
						  dcs_get_adjust_fr_cnt(),
						  dcs_get_adjust_non_dcs_th());
}

static ssize_t dcs_adjust_support_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value <= 0)
					dcs_set_adjust_support(0);
				else
					dcs_set_adjust_support(i32Value);
			}
		}
	}

	return count;
}
static KOBJ_ATTR_RW(dcs_adjust_support);

static ssize_t dcs_adjust_fr_cnt_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", dcs_get_adjust_fr_cnt());
}

static ssize_t dcs_adjust_fr_cnt_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value <= 0)
					dcs_set_adjust_fr_cnt(0);
				else
					dcs_set_adjust_fr_cnt(i32Value);
			}
		}
	}

	return count;
}
static KOBJ_ATTR_RW(dcs_adjust_fr_cnt);

static ssize_t gov_mask_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char debug_buf[GED_SYSFS_MAX_BUFF_SIZE];

	get_get_gov_support_dump(debug_buf, sizeof(debug_buf), 0);

	return scnprintf(buf, PAGE_SIZE, "%s\n", debug_buf);
}

static ssize_t gov_mask_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value <= 0)
					dcs_set_gov_enable(0, 0);
				else
					dcs_set_gov_enable(i32Value, 0);
			}
		}
	}


	return count;
}
static KOBJ_ATTR_RW(gov_mask);

static ssize_t lowpwr_mode_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int pos = 0;

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"%d\n", dcs_get_lowpwr());
	if (is_fdvfs_enable() & POLICY_MODE_V2) {
		union combineData tmp_multi = {0};

		tmp_multi = mtk_gpueb_sysram_multi_read(fdvfs_v2_table[GPU_LOWPWR_TRACE].addr);

		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"v2: %d\n",	 mtk_gpueb_sysram_read(fdvfs_v2_table[GPU_LOWPWR_ENABLE].addr));
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"(%u)s:%u d:%u fix:%u l:%u\n",
					tmp_multi.value, tmp_multi.fourVar.var1, tmp_multi.fourVar.var2,
					tmp_multi.fourVar.var3, tmp_multi.fourVar.var4);
	}

	return pos;
}

static ssize_t lowpwr_mode_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value <= 0)
					dcs_set_lowpwr(0);
				else
					dcs_set_lowpwr(i32Value);
			}
		}
	}

	return count;
}
static KOBJ_ATTR_RW(lowpwr_mode);

#endif /* CONFIG_MTK_GPUFREQ_V2 */
//-----------------------------------------------------------------------------

#if IS_ENABLED(CONFIG_MTK_GPU_FW_IDLE)
static ssize_t fw_idle_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int ui32FwIdle = 0;
	int fw_idle_enable = 0;
	int pos = 0;

	ui32FwIdle = ged_kpi_get_fw_idle_mode();
	fw_idle_enable = ged_kpi_is_fw_idle_policy_enable();

	if (fw_idle_enable > 0) {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"FW idle policy is enable\n");
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"Policy mode : %d\n", ui32FwIdle);
	} else {
		pos = scnprintf(buf + pos, PAGE_SIZE - pos,
					"FW idle policy is disabled\n");
	}

	return pos;
}

static ssize_t fw_idle_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	u32 i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				switch (i32Value) {
				case 0xF0:
					ged_kpi_enable_fw_idle_policy(1);
					break;
				case 0xFF:
					ged_kpi_enable_fw_idle_policy(0);
					break;
				default:
					if (ged_kpi_is_fw_idle_policy_enable())
						ged_kpi_set_fw_idle_mode(i32Value);
				}
			}
		}
	}

	return count;
}
static KOBJ_ATTR_RW(fw_idle);
#endif /* MTK_GPU_FW_IDLE */

static ssize_t whitebox_power_support_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int support_flag = 0;
	int pos = 0;

	support_flag = ged_get_whitebox_power_test_support();

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"whitebox_power_support = %u\n", support_flag);

	return pos;
}


static ssize_t whitebox_power_support_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	u32 i32Value = 0;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				ged_gpu_whitebox_power_test_support((int)i32Value);
		}
	}

	return count;
}
static KOBJ_ATTR_RW(whitebox_power_support);


static ssize_t whitebox_power_force_state_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int force_state = 0;
	int pos = 0;
	int state6_3 = 0;
	int state7_3 = 0;
	int state24_3 = 0;

	force_state = ged_get_whitebox_power_test_case();
	state6_3 = stat_mcu_store[6][3];
	state7_3 = stat_mcu_store[7][3];
	state24_3 = stat_mcu_store[24][3];

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"stat_mcu_store[6][3]=%d, stat_mcu_store[7][3]=%d, stat_mcu_store[24][3]=%d, force_state=%d\n",
					state6_3, state7_3, state24_3, force_state);

	return pos;
}

static ssize_t whitebox_power_force_state_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	unsigned int u32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtouint(acBuffer, 0, &u32Value) == 0)
				ged_gpu_whitebox_power_test_case((int)u32Value);
		}
	}

	return count;
}
static KOBJ_ATTR_RW(whitebox_power_force_state);

#if IS_ENABLED(CONFIG_MTK_GPU_POWER_ON_OFF_TEST)
unsigned int g_ged_power_stress_test_support;

static ssize_t gpu_power_on_off_test_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int pos = 0;

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
			"gpu_power_on_off_test support: %d\n", g_ged_power_stress_test_support);

	return pos;
}

static ssize_t gpu_power_on_off_test_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	u32 i32Value = 0;

	if (count > 0 && count < GED_SYSFS_MAX_BUFF_SIZE) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf))
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				g_ged_power_stress_test_support = i32Value;
	}

	return count;
}

static KOBJ_ATTR_RW(gpu_power_on_off_test);
#endif /* MTK_GPU_POWER_ON_OFF_TEST */

#if IS_ENABLED(CONFIG_MTK_GPU_APO_SUPPORT)
static ssize_t apo_thr_us_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned long long apo_thr_us = 0;
	int pos = 0;

	apo_thr_us = ged_get_apo_thr_ns()/1000;

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[APO_Thr]: %llu us\n", apo_thr_us);

	return pos;
}

static ssize_t apo_thr_us_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	u32 i32Value = 0;

	if (count > 0 && count < GED_SYSFS_MAX_BUFF_SIZE) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				if (i32Value > 0)
					ged_set_apo_thr_ns((unsigned long long)i32Value*1000);
		}
	}

	return count;
}
static KOBJ_ATTR_RW(apo_thr_us);

static ssize_t apo_wakeup_us_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned long long apo_wakeup_us = 0;
	int pos = 0;

	apo_wakeup_us = ged_get_apo_wakeup_ns()/1000;

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[APO_Wakeup]: %llu us\n", apo_wakeup_us);

	return pos;
}

static ssize_t apo_wakeup_us_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	u32 i32Value = 0;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				if (i32Value > 0)
					ged_set_apo_wakeup_ns((unsigned long long)i32Value*1000);
		}
	}

	return count;
}
static KOBJ_ATTR_RW(apo_wakeup_us);

static ssize_t apo_lp_thr_us_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned long long apo_lp_thr_us = 0;
	int pos = 0;

	apo_lp_thr_us = ged_get_apo_lp_thr_ns()/1000;

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[APO_LP_Thr]: %llu us\n", apo_lp_thr_us);

	return pos;
}

static ssize_t apo_lp_thr_us_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	u32 i32Value = 0;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf))
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				ged_set_apo_lp_thr_ns((unsigned long long)i32Value*1000);
	}

	return count;
}
static KOBJ_ATTR_RW(apo_lp_thr_us);

static ssize_t apo_api_boost_thr_us_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned long long apo_api_boost_thr_us = 0;
	int pos = 0;

	apo_api_boost_thr_us = ged_get_apo_api_boost_thr_ns()/1000;

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[APO_API_Boost_Thr]: %llu us\n", apo_api_boost_thr_us);

	return pos;
}

static ssize_t apo_api_boost_thr_us_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	u32 i32Value = 0;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf))
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				ged_set_apo_api_boost_thr_ns((unsigned long long)i32Value*1000);
	}

	return count;
}
static KOBJ_ATTR_RW(apo_api_boost_thr_us);

static ssize_t apo_force_hint_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int apo_force_hint = -1;
	int pos = 0;

	apo_force_hint = ged_get_apo_force_hint();

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[APO_Force_Hint]: %d\n", apo_force_hint);

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[Help]:\n0: APO_NORMAL_HINT, 1: APO_LP_HINT, Others: No Operation (APO_INVALID_HINT)\n");

	return pos;
}

static ssize_t apo_force_hint_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	u32 i32Value = 0;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf))
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				ged_set_apo_force_hint((int)i32Value);
	}

	return count;
}
static KOBJ_ATTR_RW(apo_force_hint);

static ssize_t apo_autosuspend_delay_ms_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned long long apo_autosuspend_delay_ms = 0;
	int pos = 0;

	apo_autosuspend_delay_ms = ged_get_apo_autosuspend_delay_ms();

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[APO_Autosuspend_Delay]: %llu us\n", apo_autosuspend_delay_ms);

	return pos;
}

static ssize_t apo_autosuspend_delay_ms_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value = 0;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value >= 0) {
					ged_set_apo_autosuspend_delay_ms((unsigned int)i32Value);
					ged_set_apo_autosuspend_delay_ctrl(1);
				} else
					ged_set_apo_autosuspend_delay_ctrl(0);
			}
		}
	}

	return count;
}
static KOBJ_ATTR_RW(apo_autosuspend_delay_ms);

static ssize_t apo_autosuspend_delay_target_ref_count_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int apo_autosuspend_delay_target_ref_count = 0;
	int pos = 0;

	apo_autosuspend_delay_target_ref_count = ged_get_apo_autosuspend_delay_target_ref_count();

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[APO_Autosuspend_Delay_Target_Ref_Count]: %d\n",
				apo_autosuspend_delay_target_ref_count);

	return pos;
}

static ssize_t apo_autosuspend_delay_target_ref_count_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	u32 i32Value = 0;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				ged_set_apo_autosuspend_delay_target_ref_count((int)i32Value);
		}
	}

	return count;
}
static KOBJ_ATTR_RW(apo_autosuspend_delay_target_ref_count);

static ssize_t apo_status_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	bool bGPUAPO;
	bool bGPUPredictAPO;
	unsigned long long ns_gpu_off_duration;
	unsigned long long ns_gpu_predict_off_duration;
	int apo_hint;
	int apo_autosuspend_delay_ref_count;
	int apo_autosuspend_delay_ctrl;
	int pos = 0;

	bGPUAPO = ged_gpu_apo_notify();
	bGPUPredictAPO = ged_gpu_predict_apo_notify();
	ns_gpu_off_duration = ged_get_power_duration();
	ns_gpu_predict_off_duration = ged_get_predict_power_duration();
	apo_hint = ged_get_apo_hint();
	apo_autosuspend_delay_ref_count = ged_get_apo_autosuspend_delay_ref_count();
	apo_autosuspend_delay_ctrl = ged_get_apo_autosuspend_delay_ctrl();

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[APO VERSION]: %d\n", g_ged_apo_support);

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[APO]: %d, [Predict-APO]: %d\n", bGPUAPO, bGPUPredictAPO);

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[Dur]: %lld, [Predict_Dur]: %lld\n",
				ns_gpu_off_duration, ns_gpu_predict_off_duration);

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[APO_Hint]: %d\n",
				apo_hint);

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[Autosuspend_Delay_Ref_Count]: %d\n",
				apo_autosuspend_delay_ref_count);

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[Autosuspend_Delay_Ctrl]: %d\n",
				apo_autosuspend_delay_ctrl);

	return pos;
}

static ssize_t apo_status_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	u32 i32Value = 0;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf))
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				ged_set_apo_status((int)i32Value);
	}

	return count;
}
static KOBJ_ATTR_RW(apo_status);

static ssize_t apo_legacy_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	enum ged_apo_legacy apo_legacy;
	int pos = 0;

	apo_legacy = ged_get_apo_legacy();

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[APO Legacy]: %d\n", (int)apo_legacy);

	return pos;
}

static ssize_t apo_legacy_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	u32 i32Value = 0;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf))
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				ged_set_apo_legacy((int)i32Value);
	}

	return count;
}
static KOBJ_ATTR_RW(apo_legacy);

static ssize_t apo_api_sync_status_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int pos = 0;

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[APO API_Sync]: %d\n", g_ged_apo_api_sync_support);

	return pos;
}

static ssize_t apo_api_sync_status_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	u32 i32Value = 0;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf))
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				ged_set_apo_api_sync_status((int)i32Value);
	}

	return count;
}
static KOBJ_ATTR_RW(apo_api_sync_status);

#endif /* CONFIG_MTK_GPU_APO_SUPPORT */

static ssize_t autosuspend_stress_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", ged_get_autosuspend_stress());
}

static ssize_t autosuspend_stress_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value <= 0)
					ged_set_autosuspend_stress(0);
				else
					ged_set_autosuspend_stress(i32Value);
			}
		}
	}

	return count;
}
static KOBJ_ATTR_RW(autosuspend_stress);

//-----------------------------------------------------------------------------

unsigned int g_loading_stride_size = GED_DEFAULT_SLIDE_STRIDE_SIZE;
unsigned int g_loading_target_mode;

static ssize_t loading_stride_size_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",
		g_loading_target_mode * 100 + g_loading_stride_size);
}

static ssize_t loading_stride_size_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value < 100) {
					g_loading_target_mode = 0;
					g_loading_stride_size = i32Value;
				} else if (i32Value < 200 && i32Value > 100) {
					g_loading_target_mode = 1;
					g_loading_stride_size = i32Value % 100;
				} else if (i32Value < 300 && i32Value > 200) {
					g_loading_target_mode = 2;
					g_loading_stride_size = i32Value % 100;
				} else if (i32Value < 400 && i32Value > 300) {
					g_loading_target_mode = 3;
					g_loading_stride_size = i32Value % 100;
				} else if (i32Value < 500 && i32Value > 400) {
					g_loading_target_mode = 4;
					g_loading_stride_size = i32Value % 100;
				} else {
					g_loading_target_mode = 0;
					g_loading_stride_size = GED_DEFAULT_SLIDE_STRIDE_SIZE;
				}

				ged_eb_dvfs_task(EB_DBG_CMD, 0);
				ged_eb_dvfs_task(EB_REINIT, 0);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(loading_stride_size);

//-----------------------------------------------------------------------------

unsigned int g_frame_target_mode = GED_DEFAULT_FRAME_TARGET_MODE;
unsigned int g_frame_target_time = GED_DEFAULT_FRAME_TARGET_TIME;

static ssize_t fallback_timing_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	if (is_fdvfs_enable() & POLICY_MODE_V2)
		return scnprintf(buf, PAGE_SIZE, "%u\n", mtk_gpueb_sysram_read(SYSRAM_GPU_FB_TARGET_HD));
	else
		return scnprintf(buf, PAGE_SIZE, "%u\n", g_frame_target_mode * 100 + g_frame_target_time);
}

static ssize_t fallback_timing_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value < 300 && i32Value >= 0) {
					if (i32Value < 100) {
						g_frame_target_mode = 0;
						g_frame_target_time = i32Value;
					} else if (i32Value < 200 && i32Value > 100) {
						g_frame_target_mode = 1;
						g_frame_target_time =  i32Value % 100;
					} else if (i32Value < 300 && i32Value > 200) {
						g_frame_target_mode = 2;
						g_frame_target_time =  i32Value % 100;
					}
				} else {
					g_frame_target_mode = GED_DEFAULT_FRAME_TARGET_MODE;
					g_frame_target_time = GED_DEFAULT_FRAME_TARGET_TIME;
				}
				mtk_gpueb_sysram_write(SYSRAM_GPU_FB_TARGET_HD, i32Value);
				ged_eb_dvfs_task(EB_DBG_CMD, 0);
				ged_eb_dvfs_task(EB_REINIT, 0);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(fallback_timing);

//-----------------------------------------------------------------------------

unsigned int g_fallback_mode = GED_DEFAULT_FALLBACK_MODE;
unsigned int g_fallback_time = GED_DEFAULT_FALLBACK_TIME;

static ssize_t fallback_interval_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", g_fallback_mode * 100 + g_fallback_time);
}

static ssize_t fallback_interval_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value >= 0 && i32Value < 400) {
					if (i32Value < 100) {
						g_fallback_mode = 0;
						g_fallback_time = i32Value;
					}
					if (i32Value > 100 && i32Value < 200) {
						g_fallback_mode = 1;
						g_fallback_time = i32Value%100;
					}
					if (i32Value > 200 && i32Value < 300) {
						g_fallback_mode = 2;
						g_fallback_time = i32Value%100;
					}
					if (i32Value > 300 && i32Value < 400) {
						g_fallback_mode = 3;
						g_fallback_time = i32Value%100;
					}
				} else {
					g_fallback_mode = GED_DEFAULT_FALLBACK_MODE;
					g_fallback_time = GED_DEFAULT_FALLBACK_TIME;
				}
				ged_eb_dvfs_task(EB_DBG_CMD, 0);
				ged_eb_dvfs_task(EB_REINIT, 0);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(fallback_interval);

//-----------------------------------------------------------------------------
unsigned int g_fallback_window_size = GED_DEFAULT_FALLBACK_WINDOW_SIZE;

static ssize_t fallback_window_size_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", g_fallback_window_size);
}

static ssize_t fallback_window_size_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value > 0 && i32Value < 65)
					g_fallback_window_size = i32Value;
				else
					g_fallback_window_size = GED_DEFAULT_FALLBACK_WINDOW_SIZE;

				ged_eb_dvfs_task(EB_DBG_CMD, 0);
				ged_eb_dvfs_task(EB_REINIT, 0);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(fallback_window_size);

//-----------------------------------------------------------------------------
unsigned int g_fallback_frequency_adjust = 1;

static ssize_t fallback_frequency_adjust_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", g_fallback_frequency_adjust);
}

static ssize_t fallback_frequency_adjust_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	unsigned int u32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtouint(acBuffer, 0, &u32Value) == 0) {
				if (u32Value <= 1)
					g_fallback_frequency_adjust = u32Value;

				/* Temporary solution: if fallback_frequency_adjust enabled,
				 * use previous dvfs_loading_mode policy(LOADING_MAX_ITERMCU)
				 */
				if (g_fallback_frequency_adjust == 0) {
					mtk_dvfs_loading_mode(LOADING_MAX_ITERMCU);
					mtk_dvfs_workload_mode(WORKLOAD_MAX_ITERMCU);
				} else {
					mtk_dvfs_loading_mode(LOADING_ACTIVE);
					mtk_dvfs_workload_mode(WORKLOAD_ACTIVE);
				}
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(fallback_frequency_adjust);
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static ssize_t dvfs_async_ratio_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int pos = 0;

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[dvfs_async]: force stack opp(%d), force top opp(%d)\n",
				ged_dvfs_get_stack_oppidx(), ged_dvfs_get_top_oppidx());
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[dvfs_async]: enable log(%d), enable async(%d)\n",
				ged_dvfs_get_async_log_level(), ged_dvfs_get_async_ratio_support());
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[dvfs_async]: perf model ver(%d)\n",
				ged_dvfs_get_async_perf_model());
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[dvfs_async]: oppnum_eachmask(%d),async_oppnum(%d),async_id_threshold(%d)\n",
				ged_dvfs_get_async_oppnum_eachmask(),ged_dvfs_get_async_oppnum(),
						ged_dvfs_get_async_id_threshold());
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				"[dvfs_lb_async]: enable LB async(%d), perf_diff_th(%d)\n",
				ged_dvfs_get_lb_async_ratio_support(), ged_dvfs_get_lb_async_perf_diff());

	return pos;
}

static ssize_t dvfs_async_ratio_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;
	int SUB_MASK = (0x1 << 6) - 1; // 0011 1111 (63)
	int CLEAR_FORCE_OPP_MASK = 0x1 << 6; // 64
	int FORCE_TOP_OPP_MASK = 0x1 << 7; // 128
	int ENABLE_ASYNC_RATIO_MASK = 0x1 << 8; // 256
	int ASYNC_LOG_LEVEL_MASK = 0x1 << 9; // 512
	int ASYNC_PERF_MODEL_MASK = 0x1 << 10; // 1024
	int ENABLE_LB_ASYNC_RATIO_MASK = 0x1 << 11; // 2048

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value & FORCE_TOP_OPP_MASK) {
					ged_dvfs_force_top_oppidx(i32Value & SUB_MASK);
				} else if (i32Value & ENABLE_ASYNC_RATIO_MASK) {
					ged_dvfs_enable_async_ratio(i32Value & 0x1);
				} else if (i32Value & ASYNC_LOG_LEVEL_MASK) {
					ged_dvfs_set_async_log_level(i32Value & 0xF);
				} else if (i32Value & ASYNC_PERF_MODEL_MASK) {
					ged_dvfs_set_async_perf_model(i32Value & 0xF);
				} else if (i32Value & ENABLE_LB_ASYNC_RATIO_MASK) {
					ged_dvfs_enable_lb_async_ratio(i32Value & 0x1);
					if (i32Value & SUB_MASK)
						ged_dvfs_set_lb_async_perf_diff(i32Value & SUB_MASK);
				} else {
					if (i32Value & CLEAR_FORCE_OPP_MASK) {
						ged_dvfs_force_top_oppidx(0);
						ged_dvfs_force_stack_oppidx(0);
					} else {
						ged_dvfs_force_stack_oppidx(i32Value);
					}
				}
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(dvfs_async_ratio);
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static ssize_t ged_log_level_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int pos = 0;
	int length;

	length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"GED log level: %d\n", g_default_log_level);

	pos += length;

	return pos;
}

static ssize_t ged_log_level_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value >= 0)
					g_default_log_level = i32Value;
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(ged_log_level);
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static ssize_t ged_fallback_tuning_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int pos = 0;
	int length;

	length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"ged_fallback_tuning: %d\n", ged_dvfs_get_fallback_tuning());

	pos += length;

	return pos;
}

static ssize_t ged_fallback_tuning_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value >= 0)
					ged_dvfs_set_fallback_tuning(i32Value);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(ged_fallback_tuning);
//-----------------------------------------------------------------------------
unsigned int g_loading_slide_window_size = GED_DEFAULT_SLIDE_WINDOW_SIZE;

static ssize_t loading_window_size_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", g_loading_slide_window_size);
}

static ssize_t loading_window_size_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value > 0 && i32Value < 256)
					ged_dvfs_set_slide_window_size(i32Value);

				ged_eb_dvfs_task(EB_DBG_CMD, 0);
				ged_eb_dvfs_task(EB_REINIT, 0);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(loading_window_size);

#if !IS_ENABLED(CONFIG_MTK_GPU_LEGACY) /* MTK_GPU_EB_SUPPORT */
unsigned int g_enable_idx_notify;
static ssize_t enable_idx_notify_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u(%d)\n", g_enable_idx_notify,
						mtk_gpueb_sysram_read(SYSRAM_GPU_EB_USE_IDX_NOTIFY));
}
static ssize_t enable_idx_notify_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				g_enable_idx_notify = i32Value;
				if (i32Value > 0 && i32Value < 3)
					mtk_gpueb_sysram_write(SYSRAM_GPU_EB_USE_IDX_NOTIFY, i32Value);
				else
					mtk_gpueb_sysram_write(SYSRAM_GPU_EB_USE_IDX_NOTIFY, 0);
			}
		}
	}
	return count;

}
static KOBJ_ATTR_RW(enable_idx_notify);

static ssize_t eb_counter_select_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int eb_policy_mode;
	int pos = 0;
	bool ret = true;
	struct fdvfs_ipi_data *ipi_data;

	ipi_data = (struct fdvfs_ipi_data *)ged_alloc_atomic(
		sizeof(struct fdvfs_ipi_data));

	if (!ipi_data) {
		GED_LOGE("ged_alloc_atomic fail!\n");
		return pos;
	}

	memset(ipi_data, 0, sizeof(struct fdvfs_ipi_data));

	eb_policy_mode = is_fdvfs_enable();
	if (eb_policy_dts_flag > 0 && eb_policy_mode & POLICY_MODE_V2) {
		ipi_data->cmd = GPUFDVFS_IPI_SET_CONFIG;
		ipi_data->u.set_para.arg[0] = GPUFDVFS_IPI_GET_LOADING_SELECT;
		ret = mtk_get_fastdvfs_mode((void *)ipi_data);
		if (ret) {
			pos += scnprintf(buf + pos, PAGE_SIZE - pos, "%u\n", ipi_data->u.set_para.arg[1]);
		}
	} else {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos, "disbale\n");
	}

	if (ipi_data)
		ged_free(ipi_data, sizeof(struct fdvfs_ipi_data));

	return pos;

}
static ssize_t eb_counter_select_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;
	unsigned int eb_policy_mode;
	int pos = 0;
	bool ret = true;
	struct fdvfs_ipi_data *ipi_data;

	ipi_data = (struct fdvfs_ipi_data *)ged_alloc_atomic(
		sizeof(struct fdvfs_ipi_data));

	if (!ipi_data) {
		GED_LOGE("ged_alloc_atomic fail!\n");
		return pos;
	}

	memset(ipi_data, 0, sizeof(struct fdvfs_ipi_data));

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				eb_policy_mode = is_fdvfs_enable();
				if (eb_policy_dts_flag > 0 && eb_policy_mode & POLICY_MODE_V2) {
					ipi_data->cmd = GPUFDVFS_IPI_SET_CONFIG;
					ipi_data->u.set_para.arg[0] = GPUFDVFS_IPI_SET_LOADING_SELECT;
					ipi_data->u.set_para.arg[1] = i32Value;
					ret = mtk_get_fastdvfs_mode((void *)ipi_data);
					if (!ret)
						GED_LOGE("GPUFDVFS_IPI_SET_LOADING_SELECT fail");
				}
			}
		}
	}

	if (ipi_data)
		ged_free(ipi_data, sizeof(struct fdvfs_ipi_data));

	return count;

}
static KOBJ_ATTR_RW(eb_counter_select);
#endif

//-----------------------------------------------------------------------------
#if defined(MTK_GPU_SLC_POLICY)
static ssize_t gpu_slc_policy_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int pos = 0;
	struct gpu_slc_stat *slc_stat = NULL;

	slc_stat = get_gpu_slc_stat();

	if (slc_stat != NULL) {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"mode:			%d\n", slc_stat->mode);
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"(1)Common Hitrate:	%d\n", slc_stat->policy_0_hit_rate_r);
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"(2)RT Hitrate:		%d\n", slc_stat->policy_1_hit_rate_r);
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"(4)Common_LSC Hitrate:	%d\n", slc_stat->policy_2_hit_rate_r);
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"(8)Common_TEX Hitrate:	%d\n", slc_stat->policy_3_hit_rate_r);
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"Final Policy:		%d\n", slc_stat->policy);
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"Final HitRate:		%d\n", slc_stat->hit_rate_r);
		if(slc_stat->isoverflow == 1)
			pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"(bw overflow)\n");
	} else {
		pos = scnprintf(buf + pos, PAGE_SIZE - pos,
					"GPU SLC sysFS not supports\n");
	}

	return pos;
}

static ssize_t gpu_slc_policy_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	unsigned int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				ged_gpu_slc_dynamic_mode(i32Value);
		}
	}

	return count;
}
static KOBJ_ATTR_RW(gpu_slc_policy);
#endif /* MTK_GPU_SLC_POLICY */

//-----------------------------------------------------------------------------
#if defined(MTK_GPU_MEMSYS_UTIL)
static ssize_t gpu_memsys_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int pos = 0;
	struct gpu_memsys_stat *memsys_stat = NULL;

	memsys_stat = get_gpu_memsys_stat();

	if (memsys_stat != NULL) {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"features: 0x%x\n", memsys_stat->features);
	} else {
		pos = scnprintf(buf + pos, PAGE_SIZE - pos,
					"GPU MEMSYS sysFS not supports\n");
	}

	return pos;
}

static ssize_t gpu_memsys_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	unsigned int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				ged_gpu_memsys_feature_enable(i32Value);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(gpu_memsys);

#endif

//-----------------------------------------------------------------------------

static ssize_t ged_version_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int pos = 0;
	bool ret = true;
	unsigned int eb_policy_mode;
	struct fdvfs_ipi_data *ipi_data;

	eb_policy_mode = is_fdvfs_enable();
	if (eb_policy_dts_flag > 0 && eb_policy_mode & POLICY_MODE_V2) {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"k:0x%08x eb:0x%08x\n", GED_KDEBUG_VERSION,
					mtk_gpueb_sysram_read(SYSRAM_GPU_EB_VERSION));
		ipi_data = (struct fdvfs_ipi_data *)ged_alloc_atomic(
			sizeof(struct fdvfs_ipi_data));

		if (!ipi_data) {
			GED_LOGE("ged_alloc_atomic fail!\n");
			return pos;
		}

		ipi_data->cmd = GPUFDVFS_IPI_SET_CONFIG;
		ipi_data->u.set_para.arg[0] = GPUFDVFS_IPI_GET_DUMMY_SWITCH;
		ret = mtk_get_fastdvfs_mode((void *)ipi_data);
		if (ret) {
			pos += scnprintf(buf + pos, PAGE_SIZE - pos, "%u %u %u %u\n",
					ipi_data->u.set_para.arg[1],
					ipi_data->u.set_para.arg[2],
					ipi_data->u.set_para.arg[3],
					ipi_data->u.set_para.arg[4]);
		}

		if (ipi_data)
			ged_free(ipi_data, sizeof(struct fdvfs_ipi_data));
	} else {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
			"k:0x%08x\n", GED_KDEBUG_VERSION);
	}

	return pos;
}

static ssize_t ged_version_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int eb_policy_mode;
	int pos = 0;
	bool ret = true;
	struct fdvfs_ipi_data *ipi_data;

	ipi_data = (struct fdvfs_ipi_data *)ged_alloc_atomic(
		sizeof(struct fdvfs_ipi_data));

	if (!ipi_data) {
		GED_LOGE("ged_alloc_atomic fail!\n");
		return pos;
	}

	memset(ipi_data, 0, sizeof(struct fdvfs_ipi_data));

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		eb_policy_mode = is_fdvfs_enable();
		int config1 , config2, config3, config4 = 0;

		if (sscanf(buf, "%2d %10d %10d %10d", &config1, &config2, &config3, &config4) <= 4) {
			if (eb_policy_dts_flag > 0 && eb_policy_mode & POLICY_MODE_V2) {
				ipi_data->cmd = GPUFDVFS_IPI_SET_CONFIG;
				ipi_data->u.set_para.arg[0] = GPUFDVFS_IPI_SET_DUMMY_SWITCH;
				ipi_data->u.set_para.arg[1] = config1;
				ipi_data->u.set_para.arg[2] = config2;
				ipi_data->u.set_para.arg[3] = config3;
				ipi_data->u.set_para.arg[4] = config4;
				ret = mtk_get_fastdvfs_mode((void *)ipi_data);
				if (!ret)
					GED_LOGE("GPUFDVFS_IPI_SET_LOADING_SELECT fail");
			}
		}
	}

	if (ipi_data)
		ged_free(ipi_data, sizeof(struct fdvfs_ipi_data));

	return count;

}
static KOBJ_ATTR_RW(ged_version);

//-----------------------------------------------------------------------------

GED_ERROR ged_hal_init(void)
{
	GED_ERROR err = GED_OK;

	err = ged_sysfs_create_dir(NULL, "hal", &hal_kobj);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create hal dir!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj,
		&kobj_attr_total_gpu_freq_level_count);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create total_gpu_freq_level_count entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_custom_boost_gpu_freq);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create custom_boost_gpu_freq entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj,
		&kobj_attr_custom_upbound_gpu_freq);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create custom_upbound_gpu_freq entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_current_freqency);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create current_freqency entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_previous_freqency);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create previous_freqency entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_gpu_utilization);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create gpu_utilization entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_gpu_boost_level);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create gpu_boost_level entry!\n");
		goto ERROR;
	}

#ifdef MTK_GED_KPI
	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_ged_kpi);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create ged_kpi entry!\n");
		goto ERROR;
	}
#endif

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dvfs_margin_value);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create dvfs_margin_value entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj,
		&kobj_attr_loading_base_dvfs_step);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create loading_base_dvfs_step entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj,
		&kobj_attr_timer_base_dvfs_margin);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create timer_base_dvfs_margin entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dvfs_loading_mode);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create dvfs_loading_mode entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dvfs_workload_mode);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create dvfs_workload_mode entry!\n");
		goto ERROR;
	}

	ged_fb_notifier.notifier_call = ged_fb_notifier_callback;
	if (fb_register_client(&ged_fb_notifier))
		GED_LOGE("Register fb_notifier fail!\n");

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_eb_dvfs_policy);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create eb_dvfs_policy entry!\n");

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_fb_rsf_policy);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create fb_rsf_policy entry!\n");

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_ged_npu_hint);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create ged_npu_hint entry!\n");

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_eb_dvfs_kpi);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create eb_dvfs_kpi entry!\n");

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_v_table);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create v_table entry!\n");

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_force_loading_base);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create force_loading_base entry!\n");

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_early_force_fallback_policy);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create early_force_fallback_policy entry!\n");

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_gpu_fps);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create gpu_fps entry!\n");

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_default_fps_margin_support);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create default_fps_margin_support entry!\n");

#if IS_ENABLED(CONFIG_MTK_GPUFREQ_V2) /* GED_DCS_POLICY */
	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dcs_mode);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create dcs_mode entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dcs_major_min);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create dcs_mode entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dcs_stress);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create dcs_stress entry!\n");

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dcs_adjust_support);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create dcs_adjust_support entry!\n");

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dcs_adjust_fr_cnt);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create dcs_adjust_fr_cnt entry!\n");

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_gov_mask);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create gov_mask entry!\n");

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_lowpwr_mode);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create lowpwr_mode entry!\n");

#endif /* CONFIG_MTK_GPUFREQ_V2 */

#if IS_ENABLED(CONFIG_MTK_GPU_FW_IDLE)
	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_fw_idle);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create fw_idle entry!\n");
		goto ERROR;
	}
#endif /* MTK_GPU_FW_IDLE */

#if IS_ENABLED(CONFIG_MTK_GPU_POWER_ON_OFF_TEST)
	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_gpu_power_on_off_test);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create gpu_power_on_off_test entry!\n");
		goto ERROR;
	}
#endif /* MTK_GPU_POWER_ON_OFF_TEST */

#if IS_ENABLED(CONFIG_MTK_GPU_APO_SUPPORT)
	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_apo_thr_us);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create apo_thr_us entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_apo_wakeup_us);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create apo_wakeup_us entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_apo_lp_thr_us);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create apo_lp_thr_us entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_apo_api_boost_thr_us);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create apo_api_boost_thr_us entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_apo_force_hint);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create apo_force_hint entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_apo_autosuspend_delay_target_ref_count);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create apo_autosuspend_delay_target_ref_count entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_apo_autosuspend_delay_ms);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create apo_autosuspend_delay_ms entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_apo_status);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create apo_status entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_apo_legacy);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create apo_legacy entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_apo_api_sync_status);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create apo_api_sync_status entry!\n");
		goto ERROR;
	}
#endif /* CONFIG_MTK_GPU_APO_SUPPORT */

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_autosuspend_stress);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create autosuspend_stress entry!\n");

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_loading_window_size);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create loading_window_size entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_loading_stride_size);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create loading_stride_size entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_fallback_timing);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create fallback_timing entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_fallback_interval);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create fallback_interval entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_fallback_window_size);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create fallback_window_size entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_fallback_frequency_adjust);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create fallback_frequency_adjust entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dvfs_async_ratio);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create dvfs_async_ratio entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_ged_log_level);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create ged_log_level entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_ged_fallback_tuning);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create ged_fallback_tuning entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_frame_base_optimize);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create frame_base_optimize entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_pre_fence_chk);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create pre_fence_chk entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_target_fps_vsync);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create target_fps_vsync entry!\n");
		goto ERROR;
	}

#if !IS_ENABLED(CONFIG_MTK_GPU_LEGACY) /* MTK_GPU_EB_SUPPORT */
	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_enable_idx_notify);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create enable_idx_notify entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_eb_counter_select);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create eb_counter_select entry!\n");
		goto ERROR;
	}
#endif

#if defined(MTK_GPU_SLC_POLICY)
	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_gpu_slc_policy);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create gpu_slc_policy entry!\n");
		goto ERROR;
	}
#endif /* MTK_GPU_SLC_POLICY */

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_whitebox_power_support);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create whitebox_power_support entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_whitebox_power_force_state);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create whitebox_power_force_state entry!\n");
		goto ERROR;
	}

	/* MBrain */
	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_opp_logs);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("ged: failed to create opp_logs entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_gpu_sum_loading);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create gpu_sum_loading entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_gpu_power_state);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create gpu_power_state entry!\n");
		goto ERROR;
	}
	/* MBrain end */
#if defined(MTK_GPU_MEMSYS_UTIL)
	err = ged_sysfs_create_dir(hal_kobj, "memsys" , &memsys_kobj);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create memsys dir!\n");
		goto ERROR;
	}
	err = ged_sysfs_create_file(memsys_kobj, &kobj_attr_gpu_memsys);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create gpu_memsys entry!\n");
		goto ERROR;
	}
#endif /* MTK_GPU_MEMSYS_UTIL */

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_ged_version);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create ged_version entry!\n");
		goto ERROR;
	}

	return err;

ERROR:

	ged_hal_exit();

	return err;
}
//-----------------------------------------------------------------------------
void ged_hal_exit(void)
{
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_eb_dvfs_policy);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_eb_dvfs_kpi);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_v_table);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_fb_rsf_policy);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_eb_dvfs_kpi);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_v_table);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dvfs_loading_mode);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dvfs_workload_mode);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_timer_base_dvfs_margin);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_loading_base_dvfs_step);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dvfs_margin_value);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_ged_npu_hint);
#ifdef MTK_GED_KPI
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_ged_kpi);
#endif
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_loading_window_size);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_loading_stride_size);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_gpu_boost_level);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_gpu_utilization);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_previous_freqency);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_current_freqency);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_custom_upbound_gpu_freq);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_custom_boost_gpu_freq);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_total_gpu_freq_level_count);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_fallback_timing);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_fallback_interval);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_fallback_window_size);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_fallback_frequency_adjust);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dvfs_async_ratio);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_ged_log_level);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_force_loading_base);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_early_force_fallback_policy);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_ged_fallback_tuning);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_gpu_fps);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_default_fps_margin_support);
#if IS_ENABLED(CONFIG_MTK_GPUFREQ_V2) /* GED_DCS_POLICY */
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dcs_mode);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dcs_major_min);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dcs_stress);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dcs_adjust_support);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dcs_adjust_fr_cnt);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_gov_mask);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_lowpwr_mode);
#endif
#if IS_ENABLED(CONFIG_MTK_GPU_FW_IDLE)
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_fw_idle);
#endif /* MTK_GPU_FW_IDLE */
#if IS_ENABLED(CONFIG_MTK_GPU_APO_SUPPORT)
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_apo_thr_us);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_apo_wakeup_us);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_apo_lp_thr_us);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_apo_api_boost_thr_us);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_apo_force_hint);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_apo_autosuspend_delay_target_ref_count);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_apo_autosuspend_delay_ms);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_apo_status);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_apo_legacy);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_apo_api_sync_status);
#endif /* CONFIG_MTK_GPU_APO_SUPPORT */
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_autosuspend_stress);

#if !IS_ENABLED(CONFIG_MTK_GPU_LEGACY) /* MTK_GPU_EB_SUPPORT */
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_enable_idx_notify);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_eb_counter_select);
#endif

#if defined(MTK_GPU_SLC_POLICY)
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_gpu_slc_policy);
#endif /* MTK_GPU_SLC_POLICY */

	/* MBrain */
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_opp_logs);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_gpu_sum_loading);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_gpu_power_state);
	/* MBrain end */

#if defined(MTK_GPU_MEMSYS_UTIL)
	ged_sysfs_remove_file(memsys_kobj, &kobj_attr_gpu_memsys);
	ged_sysfs_remove_dir(&memsys_kobj);
#endif

	ged_sysfs_remove_file(hal_kobj, &kobj_attr_ged_version);

	ged_sysfs_remove_dir(&hal_kobj);
}
//-----------------------------------------------------------------------------
