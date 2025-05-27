/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __MT6993_APUPWR_PROT_H__
#define __MT6993_APUPWR_PROT_H__
#include "apu_top.h"
#include "mt6993_apupwr.h"

// mbox offset define (for data exchange with remote)
#define SPARE_DBG_REG0          0x00 // mbox12_dummy0
#define SPARE_DBG_REG1          0x04 // mbox12_dummy1
#define SPARE_DBG_REG2          0x08 // mbox12_dummy2
#define SPARE_DBG_REG3          0x0C // mbox12_dummy3
#define SPARE_DBG_REG4          0x10 // mbox12_dummy4
#define SPARE_DBG_REG5          0x14 // mbox12_dummy5
#define SPARE_DBG_REG6          0x18 // mbox12_dummy6
#define SPARE_DBG_REG7          0x1C // mbox12_dummy7
#define SPARE_DBG_REG8          0x20 // mbox12_dummy8
#define SPARE_DBG_REG9          0x24 // mbox12_dummy9
#define SPARE_DBG_REG10         0x28 // mbox12_dummy10
#define SPARE_DBG_REG11         0x2C // mbox12_dummy11
#define SPARE_DBG_REG12         0x30 // mbox12_dummy12
#define SPARE_DBG_REG13         0x34 // mbox12_dummy13
#define SPARE_DBG_REG14         0x38 // mbox12_dummy14
#define SPARE_DBG_REG15         0x3C // mbox12_dummy15
#define SPARE_DBG_REG16         0x40 // mbox12_dummy16
#define SPARE_DBG_REG17         0x44 // mbox12_dummy17
#define SPARE_DBG_REG18         0x48 // mbox12_dummy18
#define SPARE_DBG_REG19         0x4C // mbox12_dummy19

/*
 * The following are used for data exchange through spare register(s)
 * direction : uP write -> APMCU read
 */
#define DRV_STAT_SYNC_REG               SPARE_DBG_REG7
#define MBRAIN_DATA_SYNC_0_REG          SPARE_DBG_REG8  // pll recording
#define MBRAIN_DATA_SYNC_1_REG          SPARE_DBG_REG9  // vapu recording
#define MBRAIN_DUMP_SIZE		32

/*
 * The following are used for data exchange through spare register(s)
 * direction : APMCU write -> uP read
 */
#define DRV_CFG_SYNC_REG                SPARE_DBG_REG14
#define FEATURE_OPTION_0_SYNC_REG       SPARE_DBG_REG15
#define FEATURE_OPTION_1_SYNC_REG       SPARE_DBG_REG16
#define D_ACX_LIMIT_OPP_REG             SPARE_DBG_REG18

/*
 * ARE empty entries usage
 */
#define APU_ARE_ETRY_DEPUTY_ADDR        SPARE_DBG_REG19

/*
 * Apu cooler usage
 */
#define APU_COOLING_UNLIMITED_STATE	(0)

enum {
	APUPWR_DBG_DEV_CTL = 0,
	APUPWR_DBG_DEV_SET_OPP,
	APUPWR_DBG_DVFS_DEBUG,
	APUPWR_DBG_DUMP_OPP_TBL,
	APUPWR_DBG_CURR_STATUS,
	APUPWR_DBG_PROFILING,
	APUPWR_DBG_CLK_SET_RATE,
	APUPWR_DBG_BUK_SET_VOLT,
	APUPWR_DBG_ARE,
	APUPWR_DBG_HW_VOTER,
	APUPWR_DBG_DUMP_OPP_TBL2,
	APUPWR_DBG_MISC,
};
enum apu_opp_limit_type {
	OPP_LIMIT_THERMAL = 0,	// limit by power API
	OPP_LIMIT_HAL,		// limit by i/o ctl
	OPP_LIMIT_DEBUG,	// limit by i/o ctl
};

struct drv_cfg_data {
	int8_t log_level;
	int8_t dvfs_debounce;   // debounce unit : ms
	int8_t disable_hw_meter;// 1: disable hw meter, bypass to read volt/freq
	int8_t pwr_drv_flag;    // func switch or toggle some features
};

struct apupw_rtfo_0 {
	int8_t pwrctl;
	int8_t dvfs;
	int8_t ndm;
	int8_t reserved;
};

struct apupw_rtfo_1 {
	int8_t reserved0;
	int8_t reserved1;
	int8_t reserved2;
	int8_t reserved3;
};

struct plat_cfg_data {
	int8_t aging_flag:4,
	       hw_id:4;
	int8_t vsram_vb_en;
};

struct device_opp_limit {
	int32_t vpu_max:6,
		vpu_min:6,
		dla_max:6,
		dla_min:6,
		lmt_type:8; // limit reason
};
struct cluster_dev_opp_info {
	uint32_t opp_lmt_reg;
	struct device_opp_limit dev_opp_lmt;
};
/*
 * due to this struct will be used to do data exchange through rpmsg
 * so the struct size can't over than 256 bytes
 * 4 bytes * 14 struct members = 56 bytes
 */
struct apu_pwr_curr_info {
	int buck_volt[BUCK_NUM];
	int buck_opp[BUCK_NUM];
	int pll_freq[PLL_NUM];
	int pll_opp[PLL_NUM];
};
/*
 * for satisfy size limitation of rpmsg data exchange is 256 bytes
 * we only put necessary information for opp table here
 * opp entries : 4 bytes * 6 struct members * 10 opp entries = 240 bytes
 * tbl_size : 4 bytes
 * total : 240 + 4 = 244 bytes
 */
struct tiny_dvfs_opp_entry {
	int vapu;       // = volt_bin - volt_age + volt_avs
	int vsram;
	int pll_freq[PLL_NUM];
};
struct tiny_dvfs_opp_tbl {
	int tbl_size;   // entry number
	struct tiny_dvfs_opp_entry opp[USER_MIN_OPP_VAL + 1];   // entry data
};
extern int opp_level_pll_freq[OPP_TABLE_SIZE];

void mt6993_aputop_opp_limit(int upper_opp, int low_opp,
		enum apu_opp_limit_type type);
#if IS_ENABLED(CONFIG_DEBUG_FS)
int mt6993_apu_top_dbg_open(struct inode *inode, struct file *file);
ssize_t mt6993_apu_top_dbg_write(
		struct file *flip, const char __user *buffer,
		size_t count, loff_t *f_pos);
#endif
int mt6993_init_remote_data_sync(void __iomem *reg_base);
int mt6993_drv_cfg_remote_sync(struct aputop_func_param *aputop);
int mt6993_apu_top_rpmsg_cb(int cmd, void *data, int len, void *priv, u32 src);
/* new function for freq upper and lower limit */
int mt6993_set_freq_limit(int upper_limit, int lower_limit,
		int *request_id, int calltype);
void mt6993_request_opp_table(void);
#endif // MT6993_APUPWR_PROT_H__
