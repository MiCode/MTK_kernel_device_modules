/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Yujen Chen <yujen.chen@mediatek.com>
 */
#define MAX_OC_LEVEL 3

enum hpt_ctrl_reg {
	HPT_CTRL,
	HPT_CTRL_SET,
	HPT_CTRL_CLR
};

enum cpu_pt_type {
	BCPU,
	MCPU,
	LCPU,
	DSU,
	CPU_CLUSTER
};

enum hpt_sram_offset {
	LV1_HPT_APU_LIMIT_FREQ,
	LV2_HPT_APU_LIMIT_FREQ

};

struct hpt_t {
	int soc;
	int soc_cur_stage;
	int soc_max_stage;
	int *soc_thd;
	int temp;
	int temp_cur_stage;
	int temp_max_stage;
	int *temp_thd;
	struct work_struct bat_work;
	struct power_supply *psy;
	unsigned int hpt_drv_done;
};

struct xpu_dbg_t {
	unsigned int cpu_cluster_cnt[MAX_OC_LEVEL];
	unsigned int cpu_cluster_len[MAX_OC_LEVEL];
	unsigned int cpu_cluster_sf[MAX_OC_LEVEL];
	unsigned int cpu_cluster_th_t[MAX_OC_LEVEL];
	unsigned int apu_limit_freq;
};

#define SPBM_PREUV_CNT            (0xF8)
#define SPBM_PREUV_DUR            (0xFC)

struct hpt_mbrain_data_v2 {
	unsigned int oc_support_level;
	unsigned int oc_count[MAX_OC_LEVEL];
	unsigned int oc_duration_us[MAX_OC_LEVEL];
};

typedef void (*hpt_mbrain_func)(void);

extern int get_hpt_mbrain_data_v2(struct hpt_mbrain_data_v2 *data);
extern int register_hpt_mbrain_v2_cb(hpt_mbrain_func func_p);
extern int unregister_hpt_mbrain_v2_cb(void);
