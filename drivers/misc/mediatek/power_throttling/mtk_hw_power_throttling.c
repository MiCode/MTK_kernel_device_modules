// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#include <linux/io.h>
#include <linux/kallsyms.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/proc_fs.h>
#include <linux/nvmem-consumer.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/types.h>
#include "mtk_hw_power_throttling.h"
#include "pmic_lbat_service.h"

#define LOG_DURATION msecs_to_jiffies(10000)
#define MAX_VALUE 0x7FFF
#define MAX_SF_VALUE 7
#define MAX_SOC_STAGES 10
#define MAX_TEMP_STAGES 10
#define STR_SIZE 256
#define NAME_LENGTH 32

static struct delayed_work hpt_dbg_work;
static struct notifier_block hpt_nb;
static hpt_mbrain_func cb_func_hpt;

struct hpt_t hpt;
struct xpu_dbg_t last_klog_xpu_dbg;

// Global/static variables for SF/freq tables and levels
static const char *pt_version;
static const char *efuse_field = "fab_info";
static unsigned int *cpu_cluster_sf;
static unsigned int *preuv_lv;
static unsigned int *cpu_freq_limit;
static unsigned int *gpu_freq_limit;
static unsigned int *apu_freq_limit;
static unsigned int *preuv_tb;
static unsigned int cpu_sf_cluster_num;
static unsigned int hpt_max_level;
static unsigned int hpt_enable_num;
static unsigned int freq_tb_num, preuv_tb_num;
static unsigned int cpu_freq_tb_idx;
static unsigned int gpu_freq_tb_idx;
static unsigned int apu_freq_tb_idx;
static unsigned int preuv_tb_idx;
static unsigned int stop_update_bat_info;
static DEFINE_MUTEX(apu_freq_lock);
static DEFINE_MUTEX(cpu_freq_lock);
static DEFINE_MUTEX(gpu_freq_lock);
static DEFINE_MUTEX(preuv_lock);

void __iomem *cpu_dbg_base;              //non-Flagship HBVC based HWPT debug log
void __iomem *cpu_limit_freq_tcm;        //Flagship platfom HWPT throttle limit freq TCM address
void __iomem *cpu_sf_tcm;
void __iomem *hpt_ctrl_base;             //Flagship platform HWPT control panel
void __iomem *hpt_ctrl;                  //non-Flagship platform HWPT control panel
void __iomem *hpt_sram_base;
void __iomem *hpt_status_base;           //Flagship platform HWPT trigger/release status
void __iomem *spbm_csram_base;           //Flagship platform HWPT debug log

struct xpu_hpt_priv {
	char freq_limit_name[NAME_LENGTH];
	u32 *freq_limit;
};

static struct xpu_hpt_priv cpu_hpt_info[CPU_CLUSTER] = {
	[BCPU] = {
		.freq_limit_name = "bcpu-limit-freq-tb",
	},
	[MCPU] = {
		.freq_limit_name = "mcpu-limit-freq-tb",
	},
	[LCPU] = {
		.freq_limit_name = "lcpu-limit-freq-tb",
	},
	[DSU] = {
		.freq_limit_name = "dsu-limit-freq-tb",
	}
};

static struct xpu_hpt_priv gpu_hpt_info = {
	.freq_limit_name = "gpu-limit-freq-tb",
};

static struct xpu_hpt_priv apu_hpt_info = {
	.freq_limit_name = "apu-limit-freq-tb",
};


static int __used hpt_read_sram(int offset)
{
	void __iomem *addr = hpt_sram_base + offset * 4;

	if (!hpt_sram_base) {
		pr_info("hpt_sram_base error %p\n", hpt_sram_base);
		return 0;
	}
	return readl(addr);
}

static void __used hpt_write_sram(unsigned int val, int offset)
{
	if (!hpt_sram_base) {
		pr_info("hpt_sram_base error %p\n", hpt_sram_base);
		return;
	}
	writel(val, (void __iomem *)(hpt_sram_base + offset * 4));
}

static int __used reg_read(void __iomem *reg_base, int offset)
{
	void __iomem *addr = reg_base + offset * 4;

	if (!reg_base) {
		pr_info("reg_base error %p, offset %d\n", reg_base, offset);
		return 0;
	}
	return readl(addr);
}

static void __used reg_write(void __iomem *reg_base, int offset, unsigned int val)
{
	if (!reg_base) {
		pr_info("reg_base error %p, offset %d\n", reg_base, offset);
		return;
	}
	writel(val, (void __iomem *)(reg_base + offset * 4));
}

static int __used get_xpu_debug_info(struct xpu_dbg_t *data)
{
	unsigned int val;

	if (!data)
		return -EINVAL;

	if (cpu_dbg_base) {
		/* Read debug info for each CPU cluster */
		for (int i = 0; i < cpu_sf_cluster_num; i++) {
			/* Register 0: Scaling factor */
			val = reg_read(cpu_dbg_base, i * 4);
			data->cpu_cluster_sf[i] = val & 0x7;

			/* Register 1: Throttle length and throttle counter for each CPU cluster */
			val = reg_read(cpu_dbg_base, i * 4 + 2);
			data->cpu_cluster_len[i] = (val >> 16) & 0x3FF;
			data->cpu_cluster_cnt[i] = val & 0xFFFF;

			/* Register 2: Throttle cumulative length for each cluster */
			val = reg_read(cpu_dbg_base, i * 4 + 3);
			data->cpu_cluster_th_t[i] = val;
		}
		data->apu_limit_freq = hpt_read_sram(LV1_HPT_APU_LIMIT_FREQ);
	}

	return 0;
}

static void hpt_print_dbg_log(struct work_struct *work)
{
	unsigned long duration, time;
	struct xpu_dbg_t dbg_data;
	static ktime_t l_ktime;
	ktime_t ktime;
	unsigned int cpu_cluster_cnt, cpu_cluster_th_t;
	unsigned long long duration_unit = 3846; /* 38.46 ns */
	unsigned int  duration_us_factor = 100000;
	int offset, ret;
	char str[STR_SIZE];
	int i, cnt, dur;
	static int prev_oc_count;

	if (!hpt.hpt_drv_done)
		return;

	ktime = ktime_get();
	duration = ktime_us_delta(ktime, l_ktime);
	time = ktime_to_us(ktime);
	l_ktime = ktime;

	get_xpu_debug_info(&dbg_data);

	offset = 0;

	ret = snprintf(str + offset, STR_SIZE - offset,
		"hpt_dbg t[k:%lu,d:%lu(ms)] bat[soc:%d t:%d] ",
		time / 1000, duration / 1000, hpt.soc, hpt.temp);

	if (ret < 0)
		pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
	else
		offset = offset + ret;

	if (cpu_dbg_base) {
		for (int i = 0; i < cpu_sf_cluster_num; i++) {
			cpu_cluster_cnt = dbg_data.cpu_cluster_cnt[i] - last_klog_xpu_dbg.cpu_cluster_cnt[i];
			cpu_cluster_th_t = dbg_data.cpu_cluster_th_t[i] - last_klog_xpu_dbg.cpu_cluster_th_t[i];
			ret = snprintf(str + offset, STR_SIZE - offset,
				"cpu_cluster%u [cnt:%u len:%u sf:%u]",
				i, cpu_cluster_cnt, cpu_cluster_th_t, dbg_data.cpu_cluster_sf[i]);
			if (ret < 0)
				pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			else
				offset = offset + ret;
		}
		ret = snprintf(str + offset, STR_SIZE - offset, " APU [freq:%u]", dbg_data.apu_limit_freq);
		if (ret < 0)
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
		else
			offset = offset + ret;
	} else if (spbm_csram_base) {
		for (i = 0; i < hpt_max_level; i++){
			cnt = reg_read(spbm_csram_base + SPBM_PREUV_CNT, i * 3);
			dur = reg_read(spbm_csram_base + SPBM_PREUV_DUR, i * 3);
			dur = dur * (duration_unit) / (duration_us_factor);
			ret = snprintf(str + offset, STR_SIZE - offset,
				"hpt_level[%d]: cnt[%u], dur[%u] ", i, cnt, dur);
			if (ret < 0)
				pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			else
				offset = offset + ret;
		}
	}

	pr_info("%s\n", str);

	memcpy(&last_klog_xpu_dbg, &dbg_data, sizeof(struct xpu_dbg_t));
	schedule_delayed_work(&hpt_dbg_work, LOG_DURATION);

	if (cpu_dbg_base) {
		if ((cpu_cluster_cnt != prev_oc_count) && (cb_func_hpt != NULL))
			cb_func_hpt();
		prev_oc_count = cpu_cluster_cnt;
	} else if (spbm_csram_base) {
		cnt = reg_read(spbm_csram_base + SPBM_PREUV_CNT, 0);
		if ((cnt != prev_oc_count) && (cb_func_hpt != NULL))
			cb_func_hpt();
		prev_oc_count = cnt;
	}
}

static void update_limit_freq(void)
{
	unsigned int index;
	int i, j;
	struct xpu_hpt_priv *hpt_info_p;

	if (hpt.temp_max_stage == 0 || hpt.soc_max_stage == 0)
		return;

	if (cpu_sf_cluster_num != 0){
		for (i = 0; i < cpu_sf_cluster_num; i++) {
			index = i * ((hpt.temp_max_stage + 1) * (hpt.soc_max_stage + 1)) +
					(hpt.soc_max_stage + 1) * hpt.temp_cur_stage + hpt.soc_cur_stage;
			if (!IS_ERR(cpu_sf_tcm))
				reg_write(cpu_sf_tcm, i, cpu_cluster_sf[index]);
		}
	} else {
		for (i = 0; i < hpt_max_level; i++) {
			for (j = 0; j < CPU_CLUSTER; j++){
				hpt_info_p = &cpu_hpt_info[j];
				index = i * ((hpt.temp_max_stage + 1) * (hpt.soc_max_stage + 1)) +
					(hpt.soc_max_stage + 1) * hpt.temp_cur_stage + hpt.soc_cur_stage;
				mutex_lock(&cpu_freq_lock);
				if (hpt_info_p->freq_limit[index] != 0)
					reg_write(cpu_limit_freq_tcm, i * CPU_CLUSTER +
							j, hpt_info_p->freq_limit[index]);
				mutex_unlock(&cpu_freq_lock);
			}
		}
	}

	for (i = 0; i < hpt_max_level; i++) {
		index = i * ((hpt.temp_max_stage + 1) * (hpt.soc_max_stage + 1)) +
				(hpt.soc_max_stage + 1) * hpt.temp_cur_stage + hpt.soc_cur_stage;
		mutex_lock(&apu_freq_lock);
		if (apu_hpt_info.freq_limit[index] != 0)
			hpt_write_sram (apu_hpt_info.freq_limit[index], LV1_HPT_APU_LIMIT_FREQ + i);
		mutex_unlock(&apu_freq_lock);
		if (gpu_hpt_info.freq_limit[index] != 0)
			pr_info("update gpu limit freq"); //TODO
	}
}

static void update_preuv(void)
{
	unsigned int index;

	if (hpt.temp_max_stage == 0 || hpt.soc_max_stage == 0)
		return;
	for (int i = 0; i < hpt_max_level; i++) {
		index = i * ((hpt.temp_max_stage + 1) * (hpt.soc_max_stage + 1)) +
				(hpt.soc_max_stage + 1) * hpt.temp_cur_stage + hpt.soc_cur_stage;
		if (hpt_max_level == 1){
			mutex_lock(&preuv_lock);
			lbat_set_preuv_lvl(preuv_lv[index]);
			mutex_unlock(&preuv_lock);
		} else if (hpt_max_level == 2 && preuv_lv[index] != 0)
			pr_info("preuv level = %u", preuv_lv[index]);
	}

}

static void bat_handler(struct work_struct *work)
{
	struct power_supply *psy = hpt.psy;
	union power_supply_propval val;
	static int last_soc = MAX_VALUE, last_temp = MAX_VALUE;
	unsigned int temp_stage, soc_stage;
	int ret = 0, soc, temp;
	bool loop;

	if (!hpt.psy) {
		pr_info("[%s] psy not init\n", __func__);
		return;
	}
	if (strcmp(psy->desc->name, "battery") != 0)
		return;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	if (ret)
		return;
	soc = val.intval;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TEMP, &val);
	if (ret)
		return;
	temp = val.intval / 10;

	if (soc > 100 || soc < 0) {
		pr_info("%s:%d soc:%d return\n", __func__, __LINE__, soc);
		return;
	}
	if (soc == last_soc && temp == last_temp) {
		pr_info("%s:%d soc and temperature are all the same\n", __func__, __LINE__);
		return;
	}

	soc_stage = hpt.soc_cur_stage;
	temp_stage = hpt.temp_cur_stage;

	do {
		loop = false;
		if (soc < last_soc) {
			if (soc_stage < hpt.soc_max_stage) {
				if (soc < hpt.soc_thd[soc_stage]) {
					soc_stage++;
					loop = true;
				}
			}
		} else if (soc > last_soc) {
			if (soc_stage > 0) {
				if (soc >= hpt.soc_thd[soc_stage-1]) {
					soc_stage--;
					loop = true;
				}
			}
		}
		if (temp < last_temp) {
			if (temp_stage < hpt.temp_max_stage) {
				if (temp < hpt.temp_thd[temp_stage]) {
					temp_stage++;
					loop = true;
				}
			}
		} else if (temp > last_temp) {
			if (temp_stage > 0) {
				if (temp >= hpt.temp_thd[temp_stage-1]) {
					temp_stage--;
					loop = true;
				}
			}
		}
	} while (loop);

	if (stop_update_bat_info)
		return;

	if ((last_soc == MAX_VALUE) || (temp_stage <= hpt.temp_max_stage && temp_stage != hpt.temp_cur_stage) ||
		(soc_stage <= hpt.soc_max_stage && soc_stage != hpt.soc_cur_stage)) {
		hpt.soc_cur_stage = soc_stage;
		hpt.temp_cur_stage = temp_stage;
		update_limit_freq();
		update_preuv();
	}

	if (temp != last_temp || soc != last_soc) {
		if (cancel_delayed_work_sync(&hpt_dbg_work))
			hpt_print_dbg_log(NULL);
		last_temp = temp;
		last_soc = soc;
		hpt.soc = soc;
		hpt.temp = temp;
	}
}

static int hpt_psy_event(struct notifier_block *nb, unsigned long event, void *v)
{
	struct power_supply *psy = v;

	if (!hpt.hpt_drv_done) {
		pr_info("[%s] hpt not init\n", __func__);
		return NOTIFY_DONE;
	}

	hpt.psy = psy;
	schedule_work(&hpt.bat_work);

	return NOTIFY_DONE;
}

static int __used read_hpt_dts(struct platform_device *pdev)
{
	int ret;
	int temp_count, soc_count, num, total_entries;
	struct device_node *np;
	struct xpu_hpt_priv *hpt_info_p;
	char buf[NAME_LENGTH];

	np = pdev->dev.of_node;

	if (!np) {
		dev_notice(&pdev->dev, "get HW power throttle dts fail\n");
		return -ENODATA;
	}

	/* Read enable bits */
	ret = of_property_read_u32(np, "hpt-enable-num", &hpt_enable_num);
	if (ret) {
		pr_info("hw_power_throttling:hpt-enable-num not found, using default 24\n");
		hpt_enable_num = 24;
	}

	ret = of_property_read_u32(np, "cpu-sf-cluster-num", &cpu_sf_cluster_num);
	if (ret) {
		pr_info("hw_power_throttling:cpu-sf-cluster-num not found\n");
		cpu_sf_cluster_num = 0;
	}

	ret = of_property_read_u32(np, "hpt-max-level", &hpt_max_level);
	if (ret) {
		pr_info("hw_power_throttling:cpu-cluster-num not found, using default 1\n");
		hpt_max_level = 1;
	}

	/* Read PT version */
	ret = of_property_read_string(np, "pt-version", &pt_version);
	if (ret)
		pr_info("hw_power_throttling: pt-version unavailable");


	/* Read temperature stage thresholds */
	temp_count = of_property_count_u32_elems(np, "temperature-stage-threshold");
	if (temp_count <= 0 || temp_count > MAX_TEMP_STAGES - 1) {
		pr_info("hw_power_throttling:Invalid temperature-stage-threshold count %d\n", temp_count);
		temp_count = 0;
	}
	hpt.temp_max_stage = temp_count;

	/* Allocate temperature threshold array */
	if (temp_count > 0) {
		hpt.temp_thd = devm_kcalloc(&pdev->dev, temp_count, sizeof(int), GFP_KERNEL);
		if (!hpt.temp_thd)
			return -ENOMEM;

		ret = of_property_read_u32_array(np, "temperature-stage-threshold",
						(u32 *)hpt.temp_thd, temp_count);
		if (ret) {
			pr_info("hw_power_throttling:Failed to read temperature-stage-threshold\n");
			return ret;
		}
	}

	/* Read SOC stage thresholds */
	soc_count = of_property_count_u32_elems(np, "soc-threshold");
	if (soc_count <= 0) {
		pr_info("hw_power_throttling:Invalid soc-threshold count %d\n", soc_count);
		soc_count = 0;
	}
	hpt.soc_max_stage = soc_count;

	/* Allocate SOC threshold array */
	if (soc_count > 0) {
		hpt.soc_thd = devm_kcalloc(&pdev->dev, soc_count, sizeof(int), GFP_KERNEL);
		if (!hpt.soc_thd)
			return -ENOMEM;

		ret = of_property_read_u32_array(np, "soc-threshold",
						(u32 *)hpt.soc_thd, soc_count);
		if (ret) {
			pr_info("hw_power_throttling:Failed to read soc-threshold\n");
			return ret;
		}
	}

	/* Calculate total entries needed for SF tables and pre-UV table */
	total_entries = (temp_count + 1) * (soc_count + 1);

	if (cpu_sf_cluster_num != 0) {
	/* Read CPU cluster SF levels setting if available */
		cpu_cluster_sf = devm_kcalloc(&pdev->dev, total_entries *
						cpu_sf_cluster_num, sizeof(unsigned int), GFP_KERNEL);
		if (!cpu_cluster_sf)
			return -ENOMEM;

		for (int i = 0; i < cpu_sf_cluster_num; i++){
			memset(buf, 0, sizeof(buf));
			snprintf (buf, sizeof(buf), "cpu-cluster%d-sf", i);
			num = of_property_count_u32_elems(np, buf);
			if (num == total_entries) {
				ret = of_property_read_u32_array(np, buf, &cpu_cluster_sf[i * total_entries], num);
				if (ret) {
					pr_info("hw_power_throttling:Failed to read %s, using default\n", buf);
					break;
				}
			} else if (num > 0)
				pr_info("hw_power_throttling:%s count mismatch: expected %d, got %d\n",
					buf, total_entries, num);
		}
	} else {
		for (int i = 0; i < CPU_CLUSTER; i++){
			hpt_info_p = &cpu_hpt_info[i];
			hpt_info_p->freq_limit = devm_kcalloc(&pdev->dev, hpt_max_level *
						total_entries, sizeof(unsigned int), GFP_KERNEL);
			if (!hpt_info_p->freq_limit)
				return -ENOMEM;
			for (int j = 0; j < hpt_max_level; j++) {
				memset(buf, 0, sizeof(buf));
				snprintf(buf, sizeof(buf), "lv%d-%s%d", j + 1, hpt_info_p->freq_limit_name, 0);
				num = of_property_count_u32_elems(np, buf);
				if (num == total_entries) {
					ret = of_property_read_u32_array(np, buf, &hpt_info_p->freq_limit[j *
									total_entries], num);
					if (ret) {
						pr_info("hw_power_throttling:Failed to read %s, using default\n", buf);
						break;
					}
				} else if (num > 0) {
					pr_info("hw_power_throttling:%s count mismatch: expected %d, got %d\n",
							buf, total_entries, num);
				}
			}
		}
	}

	/* Read GPU limit frequency if available */
	gpu_hpt_info.freq_limit = devm_kcalloc(&pdev->dev, hpt_max_level *
					total_entries, sizeof(unsigned int), GFP_KERNEL);
	if (!gpu_hpt_info.freq_limit)
		return -ENOMEM;

	for (int i = 0; i < hpt_max_level; i++) {
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "lv%d-%s%d", i + 1, gpu_hpt_info.freq_limit_name, 0);
		num = of_property_count_u32_elems(np, buf);
		if (num == total_entries) {
			ret = of_property_read_u32_array(np, buf, &gpu_hpt_info.freq_limit[i *
									total_entries], total_entries);
			if (ret) {
				pr_info("hw_power_throttling:Failed to read %s, using default\n", buf);
				break;
			}
		} else if (num > 0) {
			pr_info("hw_power_throttling:%s count mismatch: expected %d, got %d\n",
					buf, total_entries, num);
		}
	}

	/* Read APU limit frequency if available */
	apu_hpt_info.freq_limit = devm_kcalloc(&pdev->dev, hpt_max_level *
					total_entries, sizeof(unsigned int), GFP_KERNEL);
	if (!apu_hpt_info.freq_limit)
		return -ENOMEM;

	for (int i = 0; i < hpt_max_level; i++) {
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "lv%d-%s%d", i + 1, apu_hpt_info.freq_limit_name, 0);
		num = of_property_count_u32_elems(np, buf);
		if (num < 0) {
			snprintf(buf, sizeof(buf), "lv%d-%s%d", i + 1, "apu-limit-current-tb", 0);
			num = of_property_count_u32_elems(np, buf);
		}
		if (num == total_entries) {
			ret = of_property_read_u32_array(np, buf, &apu_hpt_info.freq_limit[i *
									total_entries], total_entries);
			if (ret) {
				pr_info("hw_power_throttling:Failed to read %s, using default\n", buf);
				break;
			}
		} else if (num > 0) {
			pr_info("hw_power_throttling:%s count mismatch: expected %d, got %d\n",
					buf, total_entries, num);
		}
	}

	/* Read pre-UV levels if available */
	preuv_lv = devm_kcalloc(&pdev->dev, total_entries * hpt_max_level, sizeof(unsigned int), GFP_KERNEL);
	if (!preuv_lv)
		return -ENOMEM;

	for (int i = 0; i < hpt_max_level; i++){
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "lv%d-soc-preuv-tb%d", i + 1, 0);
		num = of_property_count_u32_elems(np, buf);
		if (num == total_entries) {
			ret = of_property_read_u32_array(np, buf, &preuv_lv[i * total_entries], num);
			if (ret)
				pr_info("hw_power_throttling:Failed to read soc-preuv-level, using default\n");
		} else if (num > 0) {
			pr_info("hw_power_throttling:soc-preuv-level count mismatch: expected %d, got %d\n",
					total_entries, num);
		}
	}


	/* read XPU-freq-tb */
	ret = of_property_read_u32(np, "freq-tb-num", &freq_tb_num);
	if (ret) {
		pr_info("hw_power_throttling:freq-tb-num not found\n");
		freq_tb_num = 0;
	}

	if (freq_tb_num == 0)
		goto preuv_tb_init;

	cpu_freq_limit = devm_kcalloc(&pdev->dev, CPU_CLUSTER * total_entries * hpt_max_level * freq_tb_num,
					sizeof(unsigned int), GFP_KERNEL);
	if (!cpu_freq_limit) {
		freq_tb_num = 0;
		return -ENOMEM;
	}
	gpu_freq_limit = devm_kcalloc(&pdev->dev, total_entries * hpt_max_level * freq_tb_num,
					sizeof(unsigned int), GFP_KERNEL);
	if (!gpu_freq_limit){
		freq_tb_num = 0;
		return -ENOMEM;
	}
	apu_freq_limit = devm_kcalloc(&pdev->dev, total_entries * hpt_max_level * freq_tb_num,
					sizeof(unsigned int), GFP_KERNEL);
	if (!apu_freq_limit){
		freq_tb_num = 0;
		return -ENOMEM;
	}

	for (int i = 0; i < freq_tb_num; i++) {
		for (int j = 0; j < CPU_CLUSTER; j++) {
			hpt_info_p = &cpu_hpt_info[j];
			for (int k = 0; k < hpt_max_level; k++) {
				memset(buf, 0, sizeof(buf));
				snprintf(buf, sizeof(buf), "lv%d-%s%d", k + 1, hpt_info_p->freq_limit_name, i);
				num = of_property_count_u32_elems(np, buf);
				if (num == total_entries) {
					ret = of_property_read_u32_array(np, buf, &cpu_freq_limit[i * CPU_CLUSTER *
							hpt_max_level * total_entries + j * hpt_max_level *
							total_entries + k * total_entries], num);
					if (ret) {
						pr_info("hw_power_throttling:Failed to read %s, using default\n", buf);
						freq_tb_num = 0;
						goto preuv_tb_init;
					}
				} else {
					freq_tb_num = 0;
					goto preuv_tb_init;
				}
			}
		}
		for (int j = 0; j < hpt_max_level; j++) {
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "lv%d-%s%d", j + 1, gpu_hpt_info.freq_limit_name, i);
			num = of_property_count_u32_elems(np, buf);
			if (num == total_entries) {
				ret = of_property_read_u32_array(np, buf, &gpu_freq_limit[i * hpt_max_level *
								total_entries + j * total_entries], num);
				if (ret) {
					pr_info("hw_power_throttling:Failed to read %s, using default\n", buf);
					freq_tb_num = 0;
					goto preuv_tb_init;
				}
			} else {
				freq_tb_num = 0;
				goto preuv_tb_init;
			}
		}
		for (int k = 0; k < hpt_max_level; k++) {
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "lv%d-%s%d", k + 1, apu_hpt_info.freq_limit_name, i);
			num = of_property_count_u32_elems(np, buf);
			if (num == total_entries) {
				ret = of_property_read_u32_array(np, buf, &apu_freq_limit[i * hpt_max_level *
								total_entries + k * total_entries], num);
				if (ret) {
					pr_info("hw_power_throttling:Failed to read %s, using default\n", buf);
					freq_tb_num = 0;
					goto preuv_tb_init;
				}
			} else {
				freq_tb_num = 0;
				goto preuv_tb_init;
			}
		}
	}

preuv_tb_init:
	/* read soc-preuv-level-tb */
	ret = of_property_read_u32(np, "preuv-tb-num", &preuv_tb_num);
	if (ret) {
		pr_info("hw_power_throttling:preuv-tb-num not found\n");
		preuv_tb_num = 0;
	}
	if (preuv_tb_num == 0)
		return 0;
	preuv_tb = devm_kcalloc(&pdev->dev, total_entries * hpt_max_level * preuv_tb_num,
				sizeof(unsigned int), GFP_KERNEL);
	if (!preuv_tb) {
		preuv_tb_num = 0;
		return -ENOMEM;
	}
	for (int i = 0; i < preuv_tb_num; i++) {
		for (int k = 0; k < hpt_max_level; k++) {
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "lv%d-soc-preuv-tb%d", k + 1, i);
			num = of_property_count_u32_elems(np, buf);
			if (num == total_entries) {
				ret = of_property_read_u32_array(np, buf, &preuv_tb[i * hpt_max_level *
								total_entries + k * total_entries], num);
				if (ret) {
					pr_info("hw_power_throttling:Failed to read %s, using default\n",
						buf);
					preuv_tb_num = 0;
					return ret;
				}
			} else {
				preuv_tb_num = 0;
				return -EINVAL;
			}
		}
	}
	return 0;
}

int get_hpt_mbrain_data_v2(struct hpt_mbrain_data_v2 *data)
{
	struct xpu_dbg_t dbg_data;
	unsigned long long duration_unit = 3846; /* 38.46 ns */
	unsigned int  duration_us_factor = 100000;
	int dur, i;

	memset(&dbg_data, 0, sizeof(dbg_data));

	data->oc_support_level = hpt_max_level;

	if (cpu_dbg_base){
		get_xpu_debug_info(&dbg_data);
		data->oc_count[0] = dbg_data.cpu_cluster_cnt[0];
		data->oc_duration_us[0] = dbg_data.cpu_cluster_th_t[0];
	} else if (spbm_csram_base){
		for (i = 0; i < hpt_max_level; i++){
			data->oc_count[i] = reg_read(spbm_csram_base + SPBM_PREUV_CNT, i * 3);
			dur = reg_read(spbm_csram_base + SPBM_PREUV_DUR, i * 3);
			data->oc_duration_us[i] = dur * (duration_unit) / (duration_us_factor);
		}
	}

	return 0;
}
EXPORT_SYMBOL(get_hpt_mbrain_data_v2);

int register_hpt_mbrain_v2_cb(hpt_mbrain_func func_p)
{
	if (!func_p)
		return -EINVAL;

	cb_func_hpt = func_p;
	return 0;
}
EXPORT_SYMBOL(register_hpt_mbrain_v2_cb);

int unregister_hpt_mbrain_v2_cb(void)
{
	cb_func_hpt = NULL;
	return 0;
}
EXPORT_SYMBOL(unregister_hpt_mbrain_v2_cb);

static int mt_hpt_ctrl_proc_show(struct seq_file *m, void *v)
{
	unsigned int reg = 0;

	if (hpt_ctrl){
		reg = reg_read(hpt_ctrl, 0);
		seq_printf(m, "hpt_ctrl: 0x%x\n", reg);
	} else if (hpt_ctrl_base) {
		reg = reg_read(hpt_ctrl_base, HPT_CTRL);
		seq_printf(m, "hpt_ctrl: 0x%x\n", reg);
	}
	if (hpt_status_base){
		reg = reg_read(hpt_status_base, 0);
		seq_printf(m, "hpt_status: 0x%x\n", reg);
	}
	return 0;
}

static ssize_t mt_hpt_ctrl_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64], cmd[21];
	unsigned int len = 0, val = 0, current_val;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%20s %d", cmd, &val) != 2) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (strncmp(cmd, "ctrl", 4))
		return -EINVAL;

	if (val <= hpt_enable_num) {
		if (hpt_ctrl){
			current_val = reg_read(hpt_ctrl, 0);
			val = (current_val & ~hpt_enable_num) | (val & hpt_enable_num);
			reg_write(hpt_ctrl, 0, val);
		} else if (hpt_ctrl_base) {
			reg_write(hpt_ctrl_base, HPT_CTRL_SET, val);
			val = ~val & 0x7;
			reg_write(hpt_ctrl_base, HPT_CTRL_CLR, val);
		}
	} else
		pr_notice("hpt ctrl should be less than %d\n", hpt_enable_num);

	return count;
}

static int mt_hpt_sf_setting_proc_show(struct seq_file *m, void *v)
{
	unsigned int val;

	if (cpu_sf_cluster_num == 0 || cpu_limit_freq_tcm)
		return 0;

	for (int i = 0; i < cpu_sf_cluster_num; i++) {
		val = reg_read(cpu_dbg_base, i * 4);
		val = val & 0x7;
		seq_printf (m, "CPU cluster %u SF: %u\n", i, val);
	}

	val  = hpt_read_sram(LV1_HPT_APU_LIMIT_FREQ);
	seq_printf(m, "APU limit freq: %u\n", val);

	return 0;
}

static ssize_t mt_hpt_sf_setting_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64];
	char xpu_type[13];
	unsigned int sf, len = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);

	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%12s %u", xpu_type, &sf) != 2) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (cpu_sf_cluster_num == 0 || cpu_limit_freq_tcm)
		return 0;

	if (strcmp(xpu_type, "CPU_cluster0") == 0) {
		if (sf <= MAX_SF_VALUE && cpu_sf_cluster_num >= 1)
			reg_write(cpu_sf_tcm, 0, sf);
	} else if (strcmp(xpu_type, "CPU_cluster1") == 0) {
		if (sf <= MAX_SF_VALUE && cpu_sf_cluster_num >= 2)
			reg_write(cpu_sf_tcm, 1, sf);
	} else if (strcmp(xpu_type, "CPU_cluster2") == 0) {
		if (sf <= MAX_SF_VALUE && cpu_sf_cluster_num >= 3)
			reg_write(cpu_sf_tcm, 2, sf);
	} else if (strcmp(xpu_type, "APU") == 0) {
		hpt_write_sram (sf, LV1_HPT_APU_LIMIT_FREQ);
	} else {
		pr_notice("Invalid CPU type\n");
		return -EPERM;
	}

	return count;
}

static int mt_switch_cpu_freq_tb_proc_show(struct seq_file *m, void *v)
{
	if (freq_tb_num != 0)
		seq_printf(m, "CPU freq table: %u\n", cpu_freq_tb_idx);
	else
		seq_puts(m, "switch CPU freq table not support\n");
	return 0;
}

static ssize_t mt_switch_cpu_freq_tb_proc_write(struct file *file, const char __user *buffer, size_t count,
			loff_t *data)
{
	char desc[64], cmd[21];
	unsigned int len = 0, table_idx = 0;
	struct xpu_hpt_priv *hpt_info_p;
	int i;
	unsigned int total_entries = (hpt.temp_max_stage + 1) * (hpt.soc_max_stage + 1);

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;
	desc[len] = '\0';

	if (freq_tb_num == 0)
		return -EINVAL;

	if (sscanf(desc, "%20s %u", cmd, &table_idx) != 2) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (strncmp(cmd, "cpu_freq_tb", 11) != 0)
		return -EINVAL;

	if (table_idx >= freq_tb_num) {
		pr_notice("Invalid table index %u\n", table_idx);
		return -EINVAL;
	}

	mutex_lock(&cpu_freq_lock);
	for (i = 0; i < CPU_CLUSTER; i++) {
		hpt_info_p = &cpu_hpt_info[i];
		memcpy(hpt_info_p->freq_limit,
			&cpu_freq_limit[table_idx * (CPU_CLUSTER * hpt_max_level * total_entries) +
			i * hpt_max_level * total_entries],
			sizeof(unsigned int) * hpt_max_level * total_entries);
	}
	cpu_freq_tb_idx = table_idx;
	mutex_unlock(&cpu_freq_lock);

	update_limit_freq();
	/*debug*/
	for (i = 0; i < CPU_CLUSTER; i++) {
		hpt_info_p = &cpu_hpt_info[i];
		for (int j = 0; j < hpt_max_level * total_entries; j++)
			pr_info("cpu cluster [%d][%d] = %d\n", i, j, hpt_info_p->freq_limit[j]);
	}

	return count;
}

static int mt_switch_gpu_freq_tb_proc_show(struct seq_file *m, void *v)
{
	if (freq_tb_num != 0)
		seq_printf(m, "GPU freq table: %u\n", gpu_freq_tb_idx);
	else
		seq_puts(m, "switch GPU freq table not support\n");
	return 0;
}

static ssize_t mt_switch_gpu_freq_tb_proc_write(struct file *file, const char __user *buffer, size_t count,
			loff_t *data)
{
	char desc[64], cmd[21];
	unsigned int len = 0, table_idx = 0;
	unsigned int total_entries = (hpt.temp_max_stage + 1) * (hpt.soc_max_stage + 1);

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;
	desc[len] = '\0';

	if (freq_tb_num == 0)
		return -EINVAL;

	if (sscanf(desc, "%20s %u", cmd, &table_idx) != 2) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (strncmp(cmd, "gpu_freq_tb", 11) != 0)
		return -EINVAL;

	if (table_idx >= freq_tb_num) {
		pr_notice("Invalid table index %u\n", table_idx);
		return -EINVAL;
	}

	mutex_lock(&gpu_freq_lock);
	memcpy(gpu_hpt_info.freq_limit,
		&gpu_freq_limit[table_idx * hpt_max_level * total_entries],
		sizeof(unsigned int) * hpt_max_level * total_entries);
	gpu_freq_tb_idx = table_idx;
	mutex_unlock(&gpu_freq_lock);



	update_limit_freq();
	/*debug*/
	for (int i = 0; i < hpt_max_level * total_entries; i++)
		pr_info("gpu_freq_limit[%d] = %d\n", i, gpu_hpt_info.freq_limit[i]);

	return count;
}

static int mt_switch_apu_freq_tb_proc_show(struct seq_file *m, void *v)
{
	if (freq_tb_num != 0)
		seq_printf(m, "APU freq table: %u\n", apu_freq_tb_idx);
	else
		seq_puts(m, "switch APU freq table not support\n");
	return 0;
}

static ssize_t mt_switch_apu_freq_tb_proc_write(struct file *file, const char __user *buffer, size_t count,
			loff_t *data)
{
	char desc[64], cmd[21];
	unsigned int len = 0, table_idx = 0;
	unsigned int total_entries = (hpt.temp_max_stage + 1) * (hpt.soc_max_stage + 1);

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;
	desc[len] = '\0';

	if (freq_tb_num == 0)
		return -EINVAL;

	if (sscanf(desc, "%20s %u", cmd, &table_idx) != 2) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (strncmp(cmd, "apu_freq_tb", 11) != 0)
		return -EINVAL;

	if (table_idx >= freq_tb_num) {
		pr_notice("Invalid table index %u\n", table_idx);
		return -EINVAL;
	}

	mutex_lock(&apu_freq_lock);
	memcpy(apu_hpt_info.freq_limit,
		&apu_freq_limit[table_idx * hpt_max_level * total_entries],
		sizeof(unsigned int) * hpt_max_level * total_entries);
	apu_freq_tb_idx = table_idx;
	mutex_unlock(&apu_freq_lock);

	update_limit_freq();
	/*debug*/
	for (int i = 0; i < hpt_max_level * total_entries; i++)
		pr_info("apu_freq_limit[%d] = %d\n", i, apu_hpt_info.freq_limit[i]);

	return count;
}

static int mt_switch_preuv_tb_proc_show(struct seq_file *m, void *v)
{
	if (preuv_tb_num != 0)
		seq_printf(m, "preuv table: %u\n", preuv_tb_idx);
	else
		seq_puts(m, "switch preuv table not support\n");
	return 0;
}

static ssize_t mt_switch_preuv_tb_proc_write(struct file *file, const char __user *buffer, size_t count,
			loff_t *data)
{
	char desc[64], cmd[21];
	unsigned int len = 0, table_idx = 0;
	unsigned int total_entries = (hpt.temp_max_stage + 1) * (hpt.soc_max_stage + 1);

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;
	desc[len] = '\0';

	if (preuv_tb_num == 0)
		return -EINVAL;

	if (sscanf(desc, "%20s %u", cmd, &table_idx) != 2) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (strncmp(cmd, "preuv_tb", 8) != 0)
		return -EINVAL;

	if (table_idx >= preuv_tb_num) {
		pr_notice("Invalid table index %u\n", table_idx);
		return -EINVAL;
	}

	mutex_lock(&preuv_lock);
	memcpy(preuv_lv,
		&preuv_tb[table_idx * hpt_max_level * total_entries],
		sizeof(unsigned int) * hpt_max_level * total_entries);
	preuv_tb_idx = table_idx;
	mutex_unlock(&preuv_lock);

	update_preuv();

	return count;
}

static int mt_hpt_freq_setting_proc_show(struct seq_file *m, void *v)
{
	unsigned int val;
	int i, j;

	if (cpu_sf_cluster_num != 0 && !cpu_limit_freq_tcm)
		return 0;

	for (i = 0; i < hpt_max_level; i++){
		for (j = 0; j < CPU_CLUSTER; j++) {
			val = reg_read(cpu_limit_freq_tcm, (i * CPU_CLUSTER + j));
			seq_printf (m, "%s%d level %d freq: %u\n", cpu_hpt_info[j].freq_limit_name,
					cpu_freq_tb_idx, i + 1, val);
		}
		seq_printf(m, "%s%d level %d limit freq: %u\n", gpu_hpt_info.freq_limit_name, gpu_freq_tb_idx,
				i + 1, gpu_hpt_info.freq_limit[i * (hpt.temp_max_stage + 1) * (hpt.soc_max_stage + 1) +
				(hpt.soc_max_stage + 1) * hpt.temp_cur_stage + hpt.soc_cur_stage]);
		val = hpt_read_sram(LV1_HPT_APU_LIMIT_FREQ + i);
		seq_printf(m, "%s%d level %d limit freq: %u\n", apu_hpt_info.freq_limit_name, apu_freq_tb_idx,
				i + 1, val);
	}

	return 0;
}

static ssize_t mt_hpt_freq_setting_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64];
	char xpu_type[13];
	unsigned int hpt_level, freq, len = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);

	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%12s %u %u", xpu_type, &hpt_level, &freq) != 3) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (cpu_sf_cluster_num != 0 && !cpu_limit_freq_tcm)
		return 0;

	if (strcmp(xpu_type, "BCPU") == 0) {
		if (hpt_level > hpt_max_level)
			return -EPERM;
		reg_write(cpu_limit_freq_tcm, 0 + ((hpt_level - 1) * CPU_CLUSTER), freq);
	} else if (strcmp(xpu_type, "MCPU") == 0) {
		if (hpt_level > hpt_max_level)
			return -EPERM;
		reg_write(cpu_limit_freq_tcm, 1 + ((hpt_level - 1) * CPU_CLUSTER), freq);
	} else if (strcmp(xpu_type, "LCPU") == 0) {
		if (hpt_level > hpt_max_level)
			return -EPERM;
		reg_write(cpu_limit_freq_tcm, 2 + ((hpt_level - 1) * CPU_CLUSTER), freq);
	} else if (strcmp(xpu_type, "DSU") == 0) {
		if (hpt_level > hpt_max_level)
			return -EPERM;
		reg_write(cpu_limit_freq_tcm, 3 + ((hpt_level - 1) * CPU_CLUSTER), freq);
	} else if (strcmp(xpu_type, "GPU") == 0){
		if (hpt_level > hpt_max_level)
			return -EPERM;
	} else if (strcmp(xpu_type, "APU") == 0){
		if (hpt_level > hpt_max_level)
			return -EPERM;
		hpt_write_sram(freq, LV1_HPT_APU_LIMIT_FREQ + (hpt_level - 1));
	} else {
		pr_notice("Invalid CPU type\n");
		return -EPERM;
	}

	return count;
}

static int mt_hpt_preuv_setting_proc_show(struct seq_file *m, void *v)
{
	u8 lvl = 0;

	if (hpt_max_level == 1) {
		if (lbat_get_preuv_lvl(&lvl) == 0)
			seq_printf(m, "preuv_lv: %u\n", lvl);
		else
			seq_puts(m, "get preuv lvl fail\n");
	} else if (hpt_max_level == 2)
		seq_puts(m, "level 2 hpt not support yet\n");
	return 0;
}

static ssize_t mt_hpt_preuv_setting_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64], cmd[21];
	unsigned int len = 0, val = 0;
	int ret = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%20s %d", cmd, &val) != 2) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (strncmp(cmd, "hpt_volt", 8))
		return -EINVAL;

	if (val <= 5) {
		ret = lbat_set_preuv_lvl(val);
		pr_notice("hpt set volt lv ret %d\n", ret);
	} else
		pr_notice("hpt volt lv should be 0 ~ 5\n");

	return count;
}

static int mt_stop_update_bat_info_proc_show(struct seq_file *m, void *v)
{
	if (stop_update_bat_info)
		seq_printf(m, "stop = %d, temp and vol stage will not update\n", stop_update_bat_info);
	else
		seq_printf(m, "stop = %d, temp and vol stage will update\n", stop_update_bat_info);
	return 0;
}

static ssize_t mt_stop_update_bat_info_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64], cmd[21];
	unsigned int len = 0, val = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%20s %d", cmd, &val) != 2) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (strncmp(cmd, "stop", 4))
		return -EINVAL;

	stop_update_bat_info = val;

	return count;
}

static int mt_xpu_dbg_dump_proc_show(struct seq_file *m, void *v)
{
	struct xpu_dbg_t dbg_data;
	unsigned long long duration_unit = 3846; /* 38.46 ns */
	unsigned int  duration_us_factor = 100000;
	int i;
	unsigned int cnt, dur;

	if (cpu_dbg_base){
		get_xpu_debug_info(&dbg_data);
		for (int i = 0; i < cpu_sf_cluster_num; i++) {
			seq_printf(m,"cpu_cluster%u [cnt:%u len:%u sf:%u]",
			i, dbg_data.cpu_cluster_cnt[i], dbg_data.cpu_cluster_th_t[i], dbg_data.cpu_cluster_sf[i]);
		}
		seq_printf(m, " APU [sf:%u]\n", dbg_data.apu_limit_freq);
	} else if (spbm_csram_base){
		for (i = 0; i < hpt_max_level; i++){
			cnt = reg_read(spbm_csram_base + SPBM_PREUV_CNT, i * 3);
			dur = reg_read(spbm_csram_base + SPBM_PREUV_DUR, i * 3);
			dur = dur * (duration_unit) / (duration_us_factor);
			seq_printf(m, "hpt_level [%d]: cnt[%u], dur[%u] ", i, cnt, dur);
		}
		seq_puts(m, "\n");
	}

	return 0;
}

static int mt_pt_version_proc_show(struct seq_file *m, void *v)
{
	if (pt_version)
		seq_printf(m, "PT version: %s\n", pt_version);
	else
		seq_puts(m, "PT version: Not available\n");
	return 0;
}

#define PROC_FOPS_RW(name)						\
static int mt_ ## name ## _proc_open(struct inode *inode, struct file *file)\
{									\
	return single_open(file, mt_ ## name ## _proc_show, pde_data(inode));\
}									\
static const struct proc_ops mt_ ## name ## _proc_fops = {	\
	.proc_open		= mt_ ## name ## _proc_open,			\
	.proc_read		= seq_read,					\
	.proc_lseek		= seq_lseek,					\
	.proc_release		= single_release,				\
	.proc_write		= mt_ ## name ## _proc_write,			\
}

#define PROC_FOPS_RO(name)						\
static int mt_ ## name ## _proc_open(struct inode *inode, struct file *file)\
{									\
	return single_open(file, mt_ ## name ## _proc_show, pde_data(inode));\
}									\
static const struct proc_ops mt_ ## name ## _proc_fops = {	\
	.proc_open		= mt_ ## name ## _proc_open,		\
	.proc_read		= seq_read,				\
	.proc_lseek		= seq_lseek,				\
	.proc_release	= single_release,			\
}

#define PROC_ENTRY(name)	{__stringify(name), &mt_ ## name ## _proc_fops}

PROC_FOPS_RW(hpt_ctrl);
PROC_FOPS_RW(hpt_sf_setting);
PROC_FOPS_RW(hpt_preuv_setting);
PROC_FOPS_RO(xpu_dbg_dump);
PROC_FOPS_RW(hpt_freq_setting);
PROC_FOPS_RO(pt_version);
PROC_FOPS_RW(switch_cpu_freq_tb);
PROC_FOPS_RW(switch_gpu_freq_tb);
PROC_FOPS_RW(switch_apu_freq_tb);
PROC_FOPS_RW(switch_preuv_tb);
PROC_FOPS_RW(stop_update_bat_info);


static int mt_hpt_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(hpt_ctrl),
		PROC_ENTRY(hpt_sf_setting),
		PROC_ENTRY(hpt_preuv_setting),
		PROC_ENTRY(xpu_dbg_dump),
		PROC_ENTRY(hpt_freq_setting),
		PROC_ENTRY(pt_version),
		PROC_ENTRY(switch_cpu_freq_tb),
		PROC_ENTRY(switch_gpu_freq_tb),
		PROC_ENTRY(switch_apu_freq_tb),
		PROC_ENTRY(switch_preuv_tb),
		PROC_ENTRY(stop_update_bat_info),
	};

	dir = proc_mkdir("hpt", NULL);

	if (!dir) {
		pr_notice("fail to create /proc/hpt @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0660, dir, entries[i].fops))
			pr_notice("@%s: create /proc/hpt/%s failed\n", __func__,
				    entries[i].name);
	}

	return 0;
}

static int hw_power_throttling_probe(struct platform_device *pdev)
{
	int ret;
	size_t len;
	struct resource *res;
	void __iomem *addr;
	struct device_node *es_np;
	struct nvmem_cell *cell;
	u32 *nvmem_buf, value;

	cell = nvmem_cell_get(&pdev->dev, efuse_field);
	if (!IS_ERR(cell)) {
		nvmem_buf = (u32 *)nvmem_cell_read(cell, &len);
		nvmem_cell_put(cell);
		if (!IS_ERR(nvmem_buf)) {
			value = *nvmem_buf;
			pr_info("[%s]:fab_value = %u", __func__, value);
			if (value == 0) {
				es_np = of_find_compatible_node(NULL, NULL, "mediatek,es-hw-power-throttling");
				if (es_np != NULL)
					pdev->dev.of_node = es_np;
				else
					pr_info("[%s]:es_np is NULL", __func__);
			}
			kfree(nvmem_buf);
		} else
			pr_info ("[%s]:get fab_info failed", __func__);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "hpt_sram");

	addr = devm_ioremap_resource(&pdev->dev, res);

	if (IS_ERR(addr))
		return PTR_ERR(addr);

	hpt_sram_base = addr;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "hpt_ctrl_base");

	addr = devm_ioremap_resource(&pdev->dev, res);

	if (IS_ERR(addr))
		pr_info("%s:%d hpt_ctrl_base get addr error 0x%p\n", __func__, __LINE__, addr);
	else
		hpt_ctrl_base = addr;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "hpt_ctrl");

	addr = devm_ioremap_resource(&pdev->dev, res);

	if (IS_ERR(addr))
		pr_info("%s:%d hpt_ctrl get addr error 0x%p\n", __func__, __LINE__, addr);
	else
		hpt_ctrl = addr;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cpu_dbg");

	addr = devm_ioremap_resource(&pdev->dev, res);

	if (IS_ERR(addr))
		pr_info("%s:%d cpu_dbg get addr error 0x%p\n", __func__, __LINE__, addr);
	else
		cpu_dbg_base = addr;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "hpt_status_base");

	addr = devm_ioremap_resource(&pdev->dev, res);

	if (IS_ERR(addr))
		pr_info("%s:%d hpt_status_base get addr error 0x%p\n", __func__, __LINE__, addr);
	else
		hpt_status_base = addr;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "spbm_csram_base");

	addr = devm_ioremap_resource(&pdev->dev, res);

	if (IS_ERR(addr))
		pr_info("%s:%d spbm_csram_base get addr error 0x%p\n", __func__, __LINE__, addr);
	else
		spbm_csram_base = addr;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cpu_limit_freq_tcm");

	addr = devm_ioremap_resource(&pdev->dev, res);

	if (IS_ERR(addr))
		pr_info("%s:%d cpu_limit_freq_tcm get addr error 0x%p\n", __func__, __LINE__, addr);
	else
		cpu_limit_freq_tcm = addr;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cpu_sf_tcm_base");

	addr = devm_ioremap_resource(&pdev->dev, res);

	if (IS_ERR(addr))
		pr_info("%s:%d cpu_sf_tcm_base get addr error 0x%p\n", __func__, __LINE__, addr);
	else
		cpu_sf_tcm = addr;

	INIT_WORK(&hpt.bat_work, bat_handler);

	read_hpt_dts(pdev);

	hpt_nb.notifier_call = hpt_psy_event;

	ret = power_supply_reg_notifier(&hpt_nb);
	if (ret) {
		dev_notice(&pdev->dev, "power_supply_reg_notifier fail\n");
		return ret;
	}

	mt_hpt_create_procfs();

	INIT_DELAYED_WORK(&hpt_dbg_work, hpt_print_dbg_log);

	schedule_delayed_work(&hpt_dbg_work, LOG_DURATION);

	hpt.hpt_drv_done = 1;
	return 0;
}

static const struct of_device_id hw_power_throttling_of_match[] = {
	{ .compatible = "mediatek,hw-power-throttling", },
	{},
};

MODULE_DEVICE_TABLE(of, hw_power_throttling_of_match);

static struct platform_driver hw_power_throttling_driver = {
	.probe = hw_power_throttling_probe,
	.driver = {
		.name = "mtk_hw_power_throttling",
		.of_match_table = hw_power_throttling_of_match,
	},
};

module_platform_driver(hw_power_throttling_driver);
MODULE_AUTHOR("Yujen Chen");
MODULE_DESCRIPTION("MTK HW power throttling");
MODULE_LICENSE("GPL");
