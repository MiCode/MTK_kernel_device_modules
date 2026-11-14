/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#ifndef __MTK_SMAP_COMMON_H__
#define __MTK_SMAP_COMMON_H__

#define STR_SIZE 1024
#define smap_print(fmt, args...) \
	pr_info("[SMAP] %s: "fmt"\n", __func__, ##args)


enum SMAP_MODE {
	MODE_NORMAL,
	MODE_THRESHOLD_10GB,
	MODE_THRESHOLD_10GB_BYPASS_TEMP,
	MODE_NUM,
};

enum SMAP_DUMP_LOG_TYPE {
	NO_DUMP,
	DUMP_HEADER,
	DUMP_NO_HEADER,
	DUMP_KERNEL,
};

enum SMAP_SEND_LOG_TYPE {
	NO_SEND,
	SEND_MBRAIN,
};

struct smap_entry {
	unsigned int addr;
	unsigned int value;
	unsigned int mask;
	unsigned int others;
};

struct smap_mbrain {
	int chipid;
	unsigned int cnt;
	unsigned int type;
	unsigned int enable;
	unsigned int mode;
	unsigned int temp_mask;
	unsigned int dump_cnt;
	unsigned int mitigation_cnt;
	unsigned int total_mitigation_cnt;
	unsigned int dect_cnt;
	unsigned int temp_cnt;
	unsigned int mitigation_rate;
	unsigned int sys_time;
	unsigned int dect_result;
	unsigned int dyn_base;
	unsigned int cg_subsys_dyn;
	unsigned int cg_ratio;
	unsigned int dram0_smap_snapshot;
	unsigned int dram1_smap_snapshot;
	unsigned int dram2_smap_snapshot;
	unsigned int dram3_smap_snapshot;
	unsigned int chinf0_smap_snapshot;
	unsigned int chinf1_smap_snapshot;
	unsigned int venc0_smap_snapshot;
	unsigned int venc1_smap_snapshot;
	unsigned int venc2_smap_snapshot;
	unsigned int emi_snapshot;
	unsigned int emi_s_snapshot;
	unsigned int zram_snapshot;
	unsigned int apu_snapshot;
	unsigned long long real_time_start;
	unsigned long long real_time_end;
};

struct mtk_smap {
	struct device *dev;
	void __iomem *regs;
	unsigned int reg_value;
	unsigned int mode;
	int chipid;
	struct workqueue_struct *smap_wq;
	struct delayed_work defer_work;
	unsigned int delay_ms;
	bool def_disable;
	struct smap_mbrain debug_data;
};

typedef void (*smap_mbrain_callback)(struct smap_mbrain *debug_data);
int register_smap_mbrain_cb(smap_mbrain_callback smap_mbrain_cb);
int get_smap_mbrain_data(struct smap_mbrain *debug_data);


#endif /* __MTK_SMAP_COMMON_H__ */
