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
	MODE_TEST1,
	MODE_TEST2,
	MODE_TEST3,
	MODE_NUM,
};

enum SMAP_DUMP_LOG_TYPE {
	DUMP_HEADER,
	DUMP_NO_HEADER,
	DUMP_KERNEL,
	NO_DUMP,
};

enum SMAP_MBRAIN_LOG {
	MBRAIN_LOG_ON,
	MBRAIN_LOG_OFF,
};

struct smap_entry {
	unsigned int addr;
	unsigned int value;
	unsigned int mask;
	unsigned int others;
};

struct smap_mbrain {
	unsigned int cnt;
	unsigned int type;
	unsigned long long sys_time;
	unsigned long long real_time_start;
	unsigned long long real_time_end;
	unsigned int dyn_base;
	unsigned int cg_subsys_dyn;
	unsigned int cg_ratio;
};

struct mtk_smap {
	struct device *dev;
	void __iomem *regs;
	unsigned int reg_value;
	unsigned int mode;
	struct workqueue_struct *smap_wq;
	struct delayed_work defer_work;
	unsigned int delay_ms;
	bool def_disable;
	struct smap_mbrain mbrain_data;
};

typedef void (*smap_mbrain_callback)(struct smap_mbrain *mbrain_data);
int register_smap_mbrain_cb(smap_mbrain_callback smap_mbrain_cb);
int get_smap_mbrain_data(struct smap_mbrain *mbrain_data);


#endif /* __MTK_SMAP_COMMON_H__ */
