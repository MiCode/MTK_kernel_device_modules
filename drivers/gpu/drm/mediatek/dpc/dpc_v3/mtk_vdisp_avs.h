/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_VDISP_AVS_H__
#define __MTK_VDISP_AVS_H__

#include <soc/mediatek/mmdvfs_v3.h>

#define VDISP_EFUSE_DEFAULT_NUM (5)

/* MMUP share memory */
// common
#define VDISP_SHRMEM_BASE (g_vdisp_mmup_sram.vdisp_base_va)
#define VDISP_SHRMEM_READ_CHK(addr) \
	(g_vdisp_mmup_sram.is_valid ? readl(addr) : 0)
// bitwise
#define VDISP_SHRMEM_BITWISE        (VDISP_SHRMEM_BASE + 0x0)
#define VDISP_SHRMEM_BITWISE_VAL    VDISP_SHRMEM_READ_CHK(VDISP_SHRMEM_BITWISE)
#define VDISP_AVS_ENABLE_BIT        0
#define VDISP_AVS_AGING_ENABLE_BIT  1
#define VDISP_AVS_AGING_ACK_BIT     2
#define VDISP_AVS_IPI_ACK_BIT       3
// hist
#define VDISP_BUCK_HIST_REC_CNT   (8)
#define VDISP_BUCK_HIST_OBJ_CNT   (6)
#define VDISP_BUCK_HIST_BASE      (VDISP_SHRMEM_BASE + 0x4)
#define VDISP_BUCK_HIST_SEC_OFST  (0)
#define VDISP_BUCK_HIST_USEC_OFST (1)
#define VDISP_BUCK_HIST_LVL_OFST  (2)
#define VDISP_BUCK_HIST_VOLT_OFST (3)
#define VDISP_BUCK_HIST_TEMP_OFST (4)
#define VDISP_BUCK_HIST_STEP_OFST (5)
#define VDISP_BUCK_HIST_IDX \
	(VDISP_BUCK_HIST_BASE + 0x4*VDISP_BUCK_HIST_REC_CNT*VDISP_BUCK_HIST_OBJ_CNT)
// others
#define VDISP_CAL_BASE (VDISP_SHRMEM_BASE + 0xC8)
#define VDISP_CAL(lvl) VDISP_SHRMEM_READ_CHK(VDISP_CAL_BASE + 0x4*lvl)

void mtk_vdisp_avs_query_aging_val(struct device *dev);
int mtk_vdisp_avs_probe(struct platform_device *pdev);
int mtk_vdisp_up_dbg_opt(const char *opt);
void mtk_vdisp_set_clk(unsigned long rate);
int mtk_vdisp_up_analysis(void);

/* This enum is used to define the IPI function IDs for vdisp */
enum mtk_vdisp_avs_ipi_func_id {
	FUNC_IPI_AGING_UPDATE,
	FUNC_IPI_AVS_EN,
	FUNC_IPI_AVS_DBG_MODE,
	FUNC_IPI_AGING_ACK,
	FUNC_IPI_AVS_STEP,
	FUNC_IPI_UNIT_TEST,
	FUNC_IPI_RESET_EFUSE_VAR,
	FUNC_IPI_CHANGE_STAGE,
	FUNC_IPI_RESTORE_FREERUN,
	FUNC_IPI_SET_TEMP,
	FUNC_IPI_MGK_SUPPORT_AVS,
};

enum vdisp_ut_id {
	UT_RD_LVL,
	UT_RD_VOL,
	UT_WR_LVL,
	UT_PWR_ON,
	UT_PWR_OFF,
};

struct mtk_vdisp_efuse_lut {
	const char *name;
	const char *cell_name;
};
struct mtk_vdisp_efuse_data {
	const u32 num; // size of tbl
	const struct mtk_vdisp_efuse_lut *tbl;
};

struct mtk_vdisp_aging_data {
	/* Aging */
	// register
	u32 reg_ro_en0;
	u32 reg_ro_en1;
	u32 reg_pwr_ctrl;
	u32 ro_fresh;
	u32 ro_aging;
	u32 ro_sel_0;
	u32 win_cyc;
	u32 reg_test;
	u32 reg_ro_en2;
	u32 ro_sel_1;
	// value
	u32 ro_sel_0_fresh_val;
	u32 ro_sel_1_fresh_val;
	u32 ro_sel_0_aging_val;
	u32 ro_sel_1_aging_val;
	u32 reg_ro_en0_fresh_val;
	u32 reg_ro_en0_aging_val;
	u32 reg_ro_en2_fresh_val;
	u32 reg_ro_en2_aging_val;
};

struct mtk_vdisp_avs_data {
	const struct mtk_vdisp_aging_data *aging;
	const struct mtk_vdisp_efuse_data *efuse;
};

struct mtk_vdisp_up_data {
	const struct mtk_vdisp_avs_data *avs;
	const bool buck_hist_support;
};

static const struct mtk_vdisp_aging_data mt6989_vdisp_aging_driver_data = {
	.reg_ro_en0   = 0x00,
	.reg_ro_en1   = 0x04,
	.reg_pwr_ctrl = 0x08,
	.ro_fresh     = 0x10, //counter_grp0
	.ro_aging     = 0x2C, //counter_grp7
	.ro_sel_0     = 0x34, //aging_ro_sel
	.ro_sel_0_fresh_val = 0x1B6DB6DB,
	.ro_sel_0_aging_val = 0x00000000,
	.win_cyc      = 0x38,
	.reg_test     = 0x3C,
	.reg_ro_en2   = 0x40,
	.ro_sel_1     = 0x44, //aging_reg_dbg_rdata
	.ro_sel_1_fresh_val = 0x00000000,
	.ro_sel_1_aging_val = 0x00000001,
	.reg_ro_en0_fresh_val = 0x00000004,
	.reg_ro_en0_aging_val = 0x00000000,
	.reg_ro_en2_fresh_val = 0x00000000,
	.reg_ro_en2_aging_val = 0x00010000,
};

// compatible with mt6991, mt6993
static const struct mtk_vdisp_aging_data default_vdisp_aging_driver_data = {
	.reg_ro_en0   = 0x00,
	.reg_ro_en1   = 0x04,
	.reg_pwr_ctrl = 0x14,
	.ro_fresh     = 0x24, //counter_grp2
	.ro_aging     = 0x4C, //counter_grp12
	.ro_sel_0     = 0x54,
	.ro_sel_0_fresh_val = 0x000001C0,
	.ro_sel_0_aging_val = 0x00000000,
	.win_cyc      = 0x6C,
	.reg_test     = 0x70,
	.reg_ro_en2   = 0x08,
	.ro_sel_1     = 0x58,
	.ro_sel_1_fresh_val = 0x00000000,
	.ro_sel_1_aging_val = 0x00000200,
	.reg_ro_en0_fresh_val = 0x00100000,
	.reg_ro_en0_aging_val = 0x00000000,
	.reg_ro_en2_fresh_val = 0x00000000,
	.reg_ro_en2_aging_val = 0x00000800,
};

static const struct mtk_vdisp_efuse_lut
	mtk_vdisp_efuse_mt6991_tbl[VDISP_EFUSE_DEFAULT_NUM] = {
	{"vdisp_search_vb"  , NULL},
	{"vdisp_critical_vb", NULL},
	{"vdisp_ver_ctrl"   , NULL},
	{"vdisp_ips"        , NULL},
	{"vdisp_ro_efuse"   , NULL},
};
static const struct mtk_vdisp_efuse_data
	mtk_vdisp_efuse_data_mt6991 = {
	.num = VDISP_EFUSE_DEFAULT_NUM,
	.tbl = mtk_vdisp_efuse_mt6991_tbl,
};

static const struct mtk_vdisp_efuse_lut
	mtk_vdisp_efuse_default_tbl[VDISP_EFUSE_DEFAULT_NUM] = {
	{"vdisp_search_vb"  , "vdisp-data0"},
	{"vdisp_critical_vb", "vdisp-data1"},
	{"vdisp_ver_ctrl"   , "vdisp-data2"},
	{"vdisp_ro_efuse"   , "vdisp-data3"},
	{"vdisp_ips"        , "vdisp-data4"},
};
static const struct mtk_vdisp_efuse_data
	mtk_vdisp_efuse_default_data = {
	.num = VDISP_EFUSE_DEFAULT_NUM,
	.tbl = mtk_vdisp_efuse_default_tbl,
};

static const struct mtk_vdisp_avs_data mt6991_vdisp_avs_driver_data = {
	.aging = &default_vdisp_aging_driver_data,
	.efuse = &mtk_vdisp_efuse_data_mt6991,
};

static const struct mtk_vdisp_avs_data default_vdisp_avs_driver_data = {
	.aging = &default_vdisp_aging_driver_data,
	.efuse = &mtk_vdisp_efuse_default_data,
};

static const struct mtk_vdisp_up_data mt6991_vdisp_up_driver_data = {
	.avs = &mt6991_vdisp_avs_driver_data,
	.buck_hist_support = false,
};

static const struct mtk_vdisp_up_data default_vdisp_up_driver_data = {
	.avs = &default_vdisp_avs_driver_data,
	.buck_hist_support = true,
};
#endif
