// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kconfig.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include "mtk-smap-common.h"
#include "smap-v6993.h"

static struct mtk_smap *smap_data;
static smap_mbrain_callback mbrain_cb;

int register_smap_mbrain_cb(smap_mbrain_callback smap_mbrain_cb)
{
	if (!smap_mbrain_cb)
		return -ENODATA;

	mbrain_cb = smap_mbrain_cb;

	smap_print("register mbrain callback done\n");
	return 0;
}
EXPORT_SYMBOL(register_smap_mbrain_cb);

static inline u32 smap_read(u32 offset)
{
	int len;

	if (!smap_data) {
		smap_print("No smap_data\n");
		return 0;
	}

	len = readl(smap_data->regs + offset);
	return len;
}

static inline void smap_write(u32 offset, u32 val)
{
	if (!smap_data) {
		smap_print("No smap_data\n");
		return;
	}

	writel(val, smap_data->regs + offset);
}

static void smap_init(enum SMAP_MODE mode)
{
	smap_print("mode: %d\n", mode);

	if (!smap_data) {
		smap_print("No smap_data\n");
		return;
	}

	smap_write(SMAP_DETECTION_MODE, 0x3);
	smap_write(SMAP_IMAX_LEN, 0x67);
	smap_write(SMAP_IMAX_WEIGHT_LEN, 0x67);
	smap_write(SPM_DVS_LEVEL_FLAG_MASK_EN, 0xFFFCC00);

	if ((mode == MODE_TEST1) || (mode == MODE_TEST2) || (mode == MODE_TEST3)) {
		smap_write(SMAP_EMI_IMAX_THRESHOLD, 0x5028);
		smap_write(SMAP_DRAMG0_IMAX_THRESHOLD, 0xA0 << 10 | 0xA0);
		smap_write(SMAP_DRAMG1_IMAX_THRESHOLD, 0xA0 << 10 | 0xA0);
		smap_write(SMAP_CHINF_IMAX_THRESHOLD, 0x50 << 10 | 0x50);
		smap_write(SMAP_DRAMG0_IMAX_WEIGHT_THRESHOLD, 0xA0 << 10 | 0xA0);
		smap_write(SMAP_DRAMG1_IMAX_WEIGHT_THRESHOLD, 0xA0 << 10 | 0xA0);
		smap_write(SMAP_CHINF_IMAX_WEIGHT_THRESHOLD, 0x50 << 10 | 0x50);
		smap_write(SMAP_EMI_IMAX_WEIGHT_THRESHOLD, (0x28 << 18) | (0x28 << 9) | 0x28);
	} else {
		smap_write(SMAP_EMI_IMAX_THRESHOLD, 0xE472);
		smap_write(SMAP_DRAMG0_IMAX_THRESHOLD, 0x100 << 10 | 0x100);
		smap_write(SMAP_DRAMG1_IMAX_THRESHOLD, 0xA0 << 10 | 0xA0);
		smap_write(SMAP_CHINF_IMAX_THRESHOLD, 0x80 << 10 | 0x80);
		smap_write(SMAP_DRAMG0_IMAX_WEIGHT_THRESHOLD, 0x100 << 10 | 0x100);
		smap_write(SMAP_DRAMG1_IMAX_WEIGHT_THRESHOLD, 0x100 << 10 | 0x100);
		smap_write(SMAP_CHINF_IMAX_WEIGHT_THRESHOLD, 0x80 << 10 | 0x80);
		smap_write(SMAP_EMI_IMAX_WEIGHT_THRESHOLD, (0x28 << 18) | (0xA0 << 9) | 0xA0);
	}

	smap_write(SMAP_EMI_IMAX_WEIGHT_THRESHOLD_1, (0x50 << 18) | (0x50 << 9) | 0x28);
	smap_write(SMAP_EMI_IMAX_WEIGHT_THRESHOLD_2, (0xA0 << 18) | (0x78 << 9) | 0x78);
	smap_write(PMSR_RESERVED_RW_REG_1, (0x140 << 19) | (0xA0 << 9) | 0xA0);
	smap_write(PMSR_RESERVED_RW_REG_2, (0xA0 << 20) | (0x280 << 10) | 0x1E0);
	smap_write(SMAP_DRAM_IMAX_WEIGHT_THRESHOLD_1, (0xA0 << 20) | (0x280 << 10) | 0x1E0);
	smap_write(PMSR_RESERVED_RW_REG_3, (0x1E0 << 20) | (0x140 << 10) | 0xA0);
	smap_write(SMAP_DRAM_IMAX_WEIGHT_THRESHOLD_3, (0x140 << 20) | (0xA0 << 10) | 0x280);
	smap_write(SMAP_DRAM_IMAX_WEIGHT_THRESHOLD_CHI, (0x140 << 20) | (0xA0 << 10) | 0x280);
	smap_write(SMAP_CHI_IMAX_WEIGHT_THRESHOLD_1, (0xF0 << 11) | 0xA0);
	smap_write(SMAP_CHI_IMAX_WEIGHT_THRESHOLD_2, (0x50 << 11) | 0x140);
	smap_write(SMAP_CHI_IMAX_WEIGHT_THRESHOLD_3, (0xF0 << 11) | 0xA0);
	smap_write(SMAP_CHI_IMAX_WEIGHT_THRESHOLD_4, 0x140);
	smap_write(SMAP_DRAM_IMAX_MASK, 0x0);
	smap_write(SMAP_DRAM_IMAX_WEIGHT_MASK, 0x0);
	smap_write(SMAP_CHINF_IMAX_MASK, 0x0);
	smap_write(SMAP_CHINF_IMAX_WEIGHT_MASK, 0x0);
	smap_write(SMAP_ZRAM_IMAX_MASK, 0x0);
	smap_write(SMAP_ZRAM_IMAX_WEIGHT_MASK, 0x0);
	smap_write(SMAP_APU_IDLE2MAX_MASK, 0x3);
	smap_write(SMAP_APU_IMAX_MASK, 0x0);
	smap_write(SMAP_APU_IMAX_WEIGHT_MASK, 0x0);
	smap_write(SMAP_EMI_VDEC_VENC_IDLE2MAX_MASK, 0xfff);

	if (mode == MODE_NORMAL)
		smap_write(SMAP_EMI_VDEC_VENC_IMAX_MASK, 0xADA);
	else
		smap_write(SMAP_EMI_VDEC_VENC_IMAX_MASK, 0xADE);

	smap_write(SMAP_EMI_VDEC_IMAX_WEIGHT_MASK, 0xADE);
	smap_write(SMAP_IMAX_WEIGHT_GROUP_0, 0x1 | (0x2 << 8) | (0x3 << 16) | (0x4 << 24));
	smap_write(SMAP_IMAX_WEIGHT_GROUP_1, 0x5 | (0x2 << 8) | (0x2 << 16) | (0x2 << 24));
	smap_write(SMAP_IMAX_WEIGHT_GROUP_2, 0x1 | (0x2 << 8) | (0x3 << 16) | (0x4 << 24));
	smap_write(SMAP_IMAX_WEIGHT_GROUP_3, 0x5 | (0x1 << 8) | (0x1 << 16) | (0x1 << 24));
	smap_write(SMAP_IMAX_WEIGHT_GROUP_4, 0x2 | (0x3 << 8) | (0x4 << 16) | (0x5 << 24));
	smap_write(SMAP_IMAX_WEIGHT_GROUP_5, 0x1 | (0x2 << 8) | (0x3 << 16) | (0x4 << 24));
	smap_write(SMAP_IMAX_WEIGHT_GROUP_6, 0x5 | (0x1 << 8) | (0x2 << 16) | (0x3 << 24));
	smap_write(SMAP_IMAX_WEIGHT_GROUP_7, 0x4 | (0x5 << 8) | (0x1 << 16) | (0x2 << 24));
	smap_write(SMAP_IMAX_WEIGHT_GROUP_8, 0x3 | (0x4 << 8) | (0x5 << 16) | (0x1 << 24));
	smap_write(SMAP_IMAX_WEIGHT_GROUP_9, 0x2 | (0x3 << 8) | (0x4 << 16) | (0x5 << 24));
	smap_write(SMAP_IMAX_WEIGHT_GROUP_10, 0x1 | (0x2 << 8) | (0x3 << 16) | (0x4 << 24));
	smap_write(SMAP_IMAX_WEIGHT_GROUP_DVS, 0x5 | (0x3 << 8) | (0x4 << 16) | (0x5 << 24));
	smap_write(SMAP_IMAX_WEIGHT_GROUP_DVS_1, 0x6 | (0x7 << 8) | (0x3 << 16) | (0x4 << 24));
	smap_write(SMAP_IMAX_WEIGHT_GROUP_DVS_2, 0x5 | (0x6 << 8) | (0x7 << 16) | (0x3 << 24));
	smap_write(SMAP_IMAX_WEIGHT_GROUP_DVS_3, 0x4 | (0x5 << 8) | (0x6 << 16) | (0x7 << 24));
	smap_write(SMAP_IMAX_WEIGHT_GROUP_DVS_4, 0x3 | (0x4 << 8) | (0x5 << 16) | (0x6 << 24));
	smap_write(SMAP_IMAX_WEIGHT_GROUP_DVS_5, 0x7 | (0x4 << 8) | (0x4 << 16) | (0x4 << 24));
	smap_write(SMAP_IMAX_WEIGHT_GROUP_DVS_6, 0x3 | (0x4 << 8) | (0x5 << 16) | (0x6 << 24));
	smap_write(SMAP_IMAX_WEIGHT_GROUP_DVS_7, 0x7 | (0x2 << 8) | (0x2 << 16) | (0x3 << 24));
	smap_write(SMAP_IMAX_WEIGHT_GROUP_DVS_8, 0x4 | (0x5 << 8) | (0x6 << 16) | (0x7 << 24));
	smap_write(SMAP_IMAX_WEIGHT_GROUP_DVS_9, 0x3 | (0x4 << 8) | (0x5 << 16) | (0x6 << 24));
	smap_write(SMAP_IMAX_WEIGHT_GROUP_DVS_10, 0x7 | (0x3 << 8) | (0x4 << 16) | (0x5 << 24));
	smap_write(SMAP_IMAX_WEIGHT_GROUP_DVS_11, 0x6 | (0x7 << 8));
	smap_write(SMAP_CLK_GATING_MASK, 0xFADFE033);

	if (mode == MODE_TEST3) {
		smap_write(SMAP_CLK_GATING_CONFIG_3, 0x55555555);
		smap_write(SMAP_CLK_GATING_CONFIG_4, 0x5);
	} else {
		smap_write(SMAP_CLK_GATING_CONFIG_3, 0x33333333);
		smap_write(SMAP_CLK_GATING_CONFIG_4, 0x3);
	}

	smap_write(SMAP_CLK_GATING_CONFIG_6, 0x55555555);
	smap_write(SMAP_CLK_GATING_CONFIG_7, 0x5);
	smap_write(VENC0_SMAP_CLK_GATING_LENGTH, 0x88);
	smap_write(VENC1_SMAP_CLK_GATING_LENGTH, 0x88);
	smap_write(VENC2_SMAP_CLK_GATING_LENGTH, 0x88);
	smap_write(EMI_N_SMAP_CLK_GATING_LENGTH, 0x88);
	smap_write(EMI_S_SMAP_CLK_GATING_LENGTH, 0x88);
	smap_write(VENC0_SMAP_CLK_GATING_LENGTH_1, 0x88);
	smap_write(VENC1_SMAP_CLK_GATING_LENGTH_1, 0x88);
	smap_write(VENC2_SMAP_CLK_GATING_LENGTH_1, 0x88);
	smap_write(EMI_N_SMAP_CLK_GATING_LENGTH_1, 0x88);
	smap_write(EMI_S_SMAP_CLK_GATING_LENGTH_1, 0x88);
	smap_write(VENC0_SMAP_CLK_GATING_LENGTH_2, 0x88);
	smap_write(VENC1_SMAP_CLK_GATING_LENGTH_2, 0x88);
	smap_write(VENC2_SMAP_CLK_GATING_LENGTH_2, 0x88);
	smap_write(EMI_N_SMAP_CLK_GATING_LENGTH_2, 0x88);
	smap_write(EMI_S_SMAP_CLK_GATING_LENGTH_2, 0x88);
	smap_write(EMI_HRT_FLAG_MASK_EN, 0x1);
	smap_write(VENC0_HRT_FLAG_MASK_EN, 0x1);
	smap_write(VENC1_HRT_FLAG_MASK_EN, 0x1);
	smap_write(VENC2_HRT_FLAG_MASK_EN, 0x1);
	smap_write(SMAP_IMAX_WEIGHT_CONFIG, 0xFF | (0x1DD << 8));
	smap_write(SUBSYS_CG_SEL, 0x19F0);
	smap_write(SUBSYS_CG_RATIO_56, 0x384 | (0x44C << 12));
	smap_write(SUBSYS_CG_RATIO_78, 0x514 | (0x5DC << 12));
	smap_write(SUBSYS_CG_RATIO_5_1, 0x32A | (0x2D0 << 12));
	smap_write(SUBSYS_CG_RATIO_5_2, 0x276 | (0x21C << 12));
	smap_write(SUBSYS_CG_RATIO_5_3, 0x1C2 | (0x168 << 12));
	smap_write(SUBSYS_CG_RATIO_5_4, 0x10E | (0xB4 << 12));
	smap_write(SUBSYS_CG_RATIO_5_5, 0x5A);
	smap_write(SUBSYS_CG_RATIO_6_1, 0x3DE | (0x370 << 12));
	smap_write(SUBSYS_CG_RATIO_6_2, 0x302 | (0x294 << 12));
	smap_write(SUBSYS_CG_RATIO_6_3, 0x226 | (0x1B8 << 12));
	smap_write(SUBSYS_CG_RATIO_6_4, 0x14A | (0xDC << 12));
	smap_write(SUBSYS_CG_RATIO_6_5, 0x6E);
	smap_write(SUBSYS_CG_RATIO_7_1, 0x492 | (0x410 << 12));
	smap_write(SUBSYS_CG_RATIO_7_2, 0x38E | (0x30C << 12));
	smap_write(SUBSYS_CG_RATIO_7_3, 0x28A | (0x208 << 12));
	smap_write(SUBSYS_CG_RATIO_7_4, 0x186 | (0x104 << 12));
	smap_write(SUBSYS_CG_RATIO_7_5, 0x82);
	smap_write(SUBSYS_CG_RATIO_8_1, 0x546 | (0x4B0 << 12));
	smap_write(SUBSYS_CG_RATIO_8_2, 0x41A | (0x384 << 12));
	smap_write(SUBSYS_CG_RATIO_8_3, 0x2EE | (0x258 << 12));
	smap_write(SUBSYS_CG_RATIO_8_4, 0x1C2 | (0x12C << 12));
	smap_write(SUBSYS_CG_RATIO_8_5, 0x96);
	smap_write(MC99_IC, 0x0);
	smap_write(MC99_IC_MASK_EN, 0x1);

	if (mode == MODE_TEST2 || mode == MODE_TEST3)
		smap_write(HIGH_TEMP_MASK_EN, 0x1);
	else
		smap_write(HIGH_TEMP_MASK_EN, 0x0);

	smap_write(SMAP_IRQ_B_MASK, 0x2);
	smap_write(SMAP_ENABLE, 0x1);
}

static ssize_t dump_smap_staus(char *buf, enum SMAP_DUMP_LOG_TYPE log_type,
	enum SMAP_MBRAIN_LOG mlog)
{
	unsigned int len = 0, cnt;
	unsigned int sys_time_h, sys_time_l, type, result, dyn_base, cg_subsys_dyn, cg_ratio;
	unsigned long long sys_time;
	struct timespec64 tv = {0};

	if (!smap_data) {
		smap_print("No smap_data\n");
		return -ENODATA;
	}

	cnt = smap_read(PMSR_RESERVED_RW_REG_4);
	sys_time_h = smap_read(PMSR_RESERVED_RW_REG_5);
	sys_time_l = smap_read(PMSR_RESERVED_RW_REG_6);
	result = smap_read(PMSR_RESERVED_RW_REG_7);
	type = result >> 29;
	dyn_base = (result >> 4) & 0xFFF;
	cg_subsys_dyn = (result >> 16) & 0xFFF;
	cg_ratio = (result >> 28) & 0xF;

	if (log_type == DUMP_HEADER) {
		len += snprintf(buf + len, PAGE_SIZE - len, "CNT=%u\n", cnt);
		len += snprintf(buf + len, PAGE_SIZE - len, "SYSTIME_H=%u\n", sys_time_h);
		len += snprintf(buf + len, PAGE_SIZE - len, "SYSTIME_L=%u\n", sys_time_l);
		len += snprintf(buf + len, PAGE_SIZE - len, "RESULT=%x\n", result);
		len += snprintf(buf + len, PAGE_SIZE - len, "TYPE=%x\n", type);
		len += snprintf(buf + len, PAGE_SIZE - len, "DYN_BASE=%x\n", dyn_base);
		len += snprintf(buf + len, PAGE_SIZE - len, "CG_SUBSYS_DYN=%x\n", cg_subsys_dyn);
		len += snprintf(buf + len, PAGE_SIZE - len, "CG_RATIO=%x\n", cg_ratio);
	} else if (log_type == DUMP_NO_HEADER) {
		len += snprintf(buf + len, PAGE_SIZE - len, "%u, ", cnt);
		len += snprintf(buf + len, PAGE_SIZE - len, "%u, ", sys_time_h);
		len += snprintf(buf + len, PAGE_SIZE - len, "%u, ", sys_time_l);
		len += snprintf(buf + len, PAGE_SIZE - len, "%x, ", result);
		len += snprintf(buf + len, PAGE_SIZE - len, "%x, ", type);
		len += snprintf(buf + len, PAGE_SIZE - len, "%x, ", dyn_base);
		len += snprintf(buf + len, PAGE_SIZE - len, "%x, ", cg_subsys_dyn);
		len += snprintf(buf + len, PAGE_SIZE - len, "%x\n", cg_ratio);
	} else if (log_type == DUMP_KERNEL) {
		len += snprintf(buf + len, PAGE_SIZE - len, "CNT=%u, ", cnt);
		len += snprintf(buf + len, PAGE_SIZE - len, "SYSTIME_H=%u, ", sys_time_h);
		len += snprintf(buf + len, PAGE_SIZE - len, "SYSTIME_L=%u, ", sys_time_l);
		len += snprintf(buf + len, PAGE_SIZE - len, "RESULT=%x, ", result);
		len += snprintf(buf + len, PAGE_SIZE - len, "TYPE=%x, ", type);
		len += snprintf(buf + len, PAGE_SIZE - len, "DYN_BASE=%x, ", dyn_base);
		len += snprintf(buf + len, PAGE_SIZE - len, "CG_SUBSYS_DYN=%x, ", cg_subsys_dyn);
		len += snprintf(buf + len, PAGE_SIZE - len, "CG_RATIO=%x\n", cg_ratio);
	} else if (log_type == NO_DUMP)
		len = 0;

	if (mlog == MBRAIN_LOG_ON) {
		ktime_get_real_ts64(&tv);
		smap_data->mbrain_data.cnt = cnt;
		sys_time = sys_time_h;
		smap_data->mbrain_data.sys_time = (sys_time << 32) | sys_time_l;
		smap_data->mbrain_data.real_time_end = (tv.tv_sec*1000) + (tv.tv_nsec/1000000);
		smap_data->mbrain_data.real_time_start =
			smap_data->mbrain_data.real_time_end - smap_data->delay_ms;
		smap_data->mbrain_data.dyn_base = dyn_base;
		smap_data->mbrain_data.cg_subsys_dyn = cg_subsys_dyn;
		smap_data->mbrain_data.cg_ratio = cg_ratio;

		if (mbrain_cb)
			mbrain_cb(&smap_data->mbrain_data);
	}

	smap_print("Log type:%u, mbrain log:%u\n", log_type, mlog);
	return len;
}

static void smap_update_result(struct mtk_smap *smap)
{
	static unsigned int last_cnt;
	unsigned int len, cnt = 0;
	char buf[STR_SIZE];

	if (!smap_data) {
		smap_print("No smap_data\n");
		return;
	}

	cnt = smap_read(PMSR_RESERVED_RW_REG_4);

	if (cnt == last_cnt)
		return;

	len = dump_smap_staus(buf, DUMP_KERNEL, MBRAIN_LOG_ON);
	if (len)
		smap_print("%s\n", buf);
	last_cnt = cnt;
}

static void smap_periodic_work_handler(struct work_struct *work)
{
	struct mtk_smap *smap =
		container_of(work, struct mtk_smap, defer_work.work);

	smap_update_result(smap);

	queue_delayed_work(smap->smap_wq, &smap->defer_work,
		msecs_to_jiffies(smap->delay_ms));
}

static ssize_t smap_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int len = 0;

	if (!smap_data) {
		smap_print("No smap_data\n");
		return -ENODATA;
	}

	len = dump_smap_staus(buf, DUMP_HEADER, MBRAIN_LOG_OFF);

	return len;
}
static DEVICE_ATTR_RO(smap_status);

static ssize_t smap_dbg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int len = 0;

	if (!smap_data) {
		smap_print("No smap_data\n");
		return -ENODATA;
	}

	len = dump_smap_staus(buf, DUMP_NO_HEADER, MBRAIN_LOG_OFF);

	return len;
}

static ssize_t smap_dbg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	char cmd[20], buffer[STR_SIZE];
	u32 val = 0;
	int ret;

	if (!smap_data) {
		smap_print("No smap_data\n");
		return -ENODATA;
	}

	ret = sscanf(buf, "%19s %d", cmd, &val);
	if (ret < 1) {
		ret = -EPERM;
		goto out;
	}

	if (!strcmp(cmd, "clear")) {
		smap_write(PMSR_RESERVED_RW_REG_4, 0);
		smap_write(PMSR_RESERVED_RW_REG_5, 0);
		smap_write(PMSR_RESERVED_RW_REG_6, 0);
		smap_write(PMSR_RESERVED_RW_REG_7, 0);
	} else if (!strcmp(cmd, "enable")) {
		if (val)
			smap_write(SMAP_ENABLE, 0x1);
		else
			smap_write(SMAP_ENABLE, 0x0);
	} else if (!strcmp(cmd, "mon_en")) {
		if (smap_data->smap_wq) {
			cancel_delayed_work_sync(&smap_data->defer_work);
			if (val) {
				queue_delayed_work(smap_data->smap_wq, &smap_data->defer_work,
					msecs_to_jiffies(smap_data->delay_ms));
			}
		}
	} else if (!strcmp(cmd, "mb")) {
		dump_smap_staus(buffer, DUMP_KERNEL, MBRAIN_LOG_ON);
		smap_print("%s\n", buffer);
	}
out:
	if (ret < 0)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(smap_dbg);

static ssize_t smap_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int len = 0;

	if (!smap_data) {
		smap_print("No smap_data\n");
		return -ENODATA;
	}

	len += snprintf(buf + len, PAGE_SIZE, "smap mode:%d\n", smap_data->mode);
	return len;
}

static ssize_t smap_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	char desc[64];
	unsigned int len = 0, mode = 0;
	int ret;

	if (!smap_data) {
		smap_print("No smap_data\n");
		return -ENODATA;
	}

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buf, len))
		return 0;

	desc[len] = '\0';

	ret = kstrtouint(desc, 10, &mode);
	if (ret) {
		smap_print("parameter number not correct\n");
		return -EPERM;
	}

	if (mode < MODE_NUM) {
		smap_data->mode = mode;
		smap_init(mode);
	}

	return 0;
}
static DEVICE_ATTR_RW(smap_mode);


static struct attribute *smap_sysfs_attrs[] = {
	&dev_attr_smap_status.attr,
	&dev_attr_smap_dbg.attr,
	&dev_attr_smap_mode.attr,
	NULL,
};

static struct attribute_group smap_sysfs_attr_group = {
	.attrs = smap_sysfs_attrs,
};

int smap_register_sysfs(struct device *dev)
{
	int ret;

	ret = sysfs_create_group(&dev->kobj, &smap_sysfs_attr_group);
	if (ret)
		return ret;

	ret = sysfs_create_link(kernel_kobj, &dev->kobj, "smap");

	return ret;
}

static int smap_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct mtk_smap *priv;
	struct device_node *np = pdev->dev.of_node;
	u32 smap_mode = 0;
	u32 interval = 0;

	priv = devm_kzalloc(dev, sizeof(struct mtk_smap), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	priv->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(priv->regs))
		return PTR_ERR(priv->regs);

	priv->dev = dev;
	of_property_read_u32(np, "smap-mode", &smap_mode);
	priv->mode = smap_mode;
	priv->def_disable= of_property_read_bool(np, "smap-disable");
	if (priv->def_disable)
		return -EINVAL;

	of_property_read_u32(np, "smap-mon-interval", &interval);
	if (interval < 500)
		priv->delay_ms = 1000 * 3;
	else
		priv->delay_ms = interval;

	priv->smap_wq = create_singlethread_workqueue("smap_wq");
	INIT_DELAYED_WORK(&priv->defer_work, smap_periodic_work_handler);
	if (of_property_read_bool(np, "smap-mon-enable")) {
		if (priv->smap_wq) {
			queue_delayed_work(priv->smap_wq, &priv->defer_work,
				msecs_to_jiffies(priv->delay_ms));
		}
	}

	smap_register_sysfs(dev);
	ret = devm_of_platform_populate(dev);
	if (ret)
		smap_print("devm_of_platform_populate Failed\n");

	platform_set_drvdata(pdev, priv);
	smap_data = priv;

	smap_init(priv->mode);
	smap_print("IC[val:mask]=%u,%u, TEMP[val:mask]=%u,%u\n",
		smap_read(MC99_IC), smap_read(MC99_IC_MASK_EN),
		smap_read(HIGH_TEMP), smap_read(HIGH_TEMP_MASK_EN));

	return 0;
}

static void smap_remove(struct platform_device *pdev)
{
	struct mtk_smap *smap = platform_get_drvdata(pdev);

	if (smap->smap_wq) {
		cancel_delayed_work_sync(&smap->defer_work);
		destroy_workqueue(smap->smap_wq);
	}
}

static const struct of_device_id smap_of_match[] = {
	{ .compatible = "mediatek,mt6993-smap", },
	{}
};

static struct platform_driver smap_pdrv = {
	.probe = smap_probe,
	.remove = smap_remove,
	.driver = {
		.name = "smap",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(smap_of_match),
	},
};

static int __init smap_module_init(void)
{
	return platform_driver_register(&smap_pdrv);
}
module_init(smap_module_init);

static void __exit smap_module_exit(void)
{
	platform_driver_unregister(&smap_pdrv);
}
module_exit(smap_module_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MTK SMAP driver");
