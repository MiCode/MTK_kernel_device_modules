/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef __GPUFREQ_MT6781_H__
#define __GPUFREQ_MT6781_H__

/**************************************************
 * GPUFREQ Local Config
 **************************************************/
#define GPUFREQ_BRINGUP                 (0)
/*
 * 0 -> power on once then never off and disable DDK power on/off callback
 */
#define GPUFREQ_POWER_CTRL_ENABLE       (1)
/*
 * (DVFS_ENABLE, CUST_INIT)
 * (1, 1) -> DVFS enable and init to CUST_INIT_OPPIDX
 * (1, 0) -> DVFS enable
 * (0, 1) -> DVFS disable but init to CUST_INIT_OPPIDX (do DVFS only onces)
 * (0, 0) -> DVFS disable
 */
#define GPUFREQ_DVFS_ENABLE             (1)
#define GPUFREQ_CUST_INIT_ENABLE        (0)
#define GPUFREQ_CUST_INIT_OPPIDX        (16) //As K419 has (g_opp_table_segment_1[16].gpufreq_khz)
#define MT_GPUFREQ_DFD_ENABLE        (0)

#define PBM_RAEDY                       (0)
#define SHADER_CORE_NUM                 (2)
/**************************************************
 * Clock Setting
 **************************************************/
#define POSDIV_2_MAX_FREQ               (1900000)       /* KHz */
#define POSDIV_2_MIN_FREQ               (750000)        /* KHz */
#define POSDIV_4_MAX_FREQ               (950000)        /* KHz */
#define POSDIV_4_MIN_FREQ               (375000)        /* KHz */
#define POSDIV_8_MAX_FREQ               (475000)        /* KHz */
#define POSDIV_8_MIN_FREQ               (187500)        /* KHz */
#define POSDIV_16_MAX_FREQ              (237500)        /* KHz */
#define POSDIV_16_MIN_FREQ              (125000)        /* KHz */
#define POSDIV_SHIFT                    (24)            /* bit */
#define DDS_SHIFT                       (14)            /* bit */
#define TO_MHZ_HEAD                     (100)
#define TO_MHZ_TAIL                     (10)
#define ROUNDING_VALUE                  (5)
#define MFGPLL_FIN                      (26)            /* MHz */
#define MFGPLL_FH_PLL                   FH_PLL4//(6)
#define MFGPLL_CON0                     (g_apmixed_base + 0x250)
#define MFGPLL_CON1                     (g_apmixed_base + 0x254)
#define MFGPLL_PWR_CON0                 (g_apmixed_base + 0x25C)

/**************************************************
 * Frequency Hopping Setting
 **************************************************/
 //todo disable until CONFIG_COMMON_CLK_MTK_FREQ_HOPPING ready
#define GPUFREQ_FHCTL_ENABLE            (0)
#define MFG_PLL_NAME                    "mfgpll1"
//TODO:MFG_PLL_NAME should be FH_PLL4 same as K419 ?
//Currently not working as GPUFREQ_FHCTL_ENABLE is 0

/**************************************************
 * Power Domain Setting
 **************************************************/
#define GPUFREQ_CHECK_MTCMOS_PWR_STATUS (0)
#define MT_GPUFREQ_STATIC_PWR_READY2USE         1


/**************************************************
 * Shader Core Setting
 **************************************************/
#define MFG2_SHADER_STACK0         (T0C0)
#define MFG3_SHADER_STACK2         (T2C0)
#define MT_GPU_SHADER_PRESENT_2    (T0C0 | T2C0)

/**************************************************
 * Reference Power Setting MT6781
 **************************************************/
#define GPU_ACT_REF_POWER			(1307)		/* mW  */
#define GPU_ACT_REF_FREQ			(950000)	/* KHz */
#define GPU_ACT_REF_VOLT			(90000)		/* mV x 100 */
#define PTPOD_DISABLE_VOLT              (75000)

#define GPU_LEAKAGE_POWER               (71)

/**************************************************
 * PMIC Setting MT6781
 **************************************************/
#define VGPU_MAX_VOLT		(110000)         /* mV x 100 */
#define VGPU_MIN_VOLT       (55000)         /* mV x 100 */
#define VSRAM_MAX_VOLT      (120000)        /* mV x 100 */
#define VSRAM_MIN_VOLT      (60000)         /* mV x 100 */
#define PMIC_STEP           (625)           /* mV x 100 */

/*
 * (0)mv <= (VSRAM - VGPU) <= (250)mV
 */
#define MAX_BUCK_DIFF                   (25000)         /* mV x 100 */
#define MIN_BUCK_DIFF                   (10000)        /* mV x 100 */

/* On opp table, low vgpu will use the same vsram.
 * And hgih vgpu will have the same diff with vsram.
 *
 * if (vgpu <= FIXED_VSRAM_VOLT_THSRESHOLD) {
 *     vsram = FIXED_VSRAM_VOLT;
 * } else {
 *     vsram = vgpu + FIXED_VSRAM_VOLT_DIFF;
 * }
 */
#define VSRAM_FIXED_THRESHOLD           (75000)
#define VSRAM_FIXED_VOLT                (85000)
#define VSRAM_FIXED_DIFF                (10000)
#define VOLT_NORMALIZATION(volt) \
	((volt % PMIC_STEP) ? (volt - (volt % PMIC_STEP) + PMIC_STEP) : volt)


/**************************************************
 * SRAMRC Setting
 **************************************************/
#define GPUFREQ_SAFE_VLOGIC             (60000)

/**************************************************
 * Power Throttling Setting
 **************************************************/
// MT6781_PORTING_TODO check below power settings @{
/**************************************************
 * Battery Over Current Protect
 **************************************************/
#define GPUFREQ_BATT_OC_ENABLE          (1)
#define GPUFREQ_BATT_OC_FREQ            (485000)
/**************************************************
 * Battery Percentage Protect
 **************************************************/
#define GPUFREQ_BATT_PERCENT_ENABLE     (0)
#define GPUFREQ_BATT_PERCENT_IDX        (1)
/**************************************************
 * Low Battery Volume Protect
 **************************************************/
#define GPUFREQ_LOW_BATT_ENABLE         (1)
#define GPUFREQ_LOW_BATT_FREQ           (485000)

/**************************************************
 * AGING Setting
 **************************************************/
#define GPUFREQ_AGING_MOST_AGRRESIVE    (0)
/**************************************************
 * Dump infra status
 **************************************************/
#define FM_HF_FMFG_CK 3
#define FM_AD_MFGPLL_CK 26

/**************************************************
 * Enumeration MT6781
 **************************************************/
enum gpufreq_segment {
	MT6781_SEGMENT = 1,
	MT6781M_SEGMENT,
	MT6781T_SEGMENT,		//Reserve Segment
};

enum gpufreq_clk_src {
	CLOCK_MAIN = 0,
	CLOCK_SUB,
};

/**************************************************
 * Structure MT6781
 **************************************************/
struct g_clk_info {
	struct clk *clk_mux;
	struct clk *clk_main_parent;
	struct clk *clk_sub_parent;
	struct clk *subsys_mfg_cg;
	struct clk *mtcmos_mfg_async;
	struct clk *mtcmos_mfg;
	struct clk *mtcmos_mfg_core0;
	struct clk *mtcmos_mfg_core1;
};
struct g_pmic_info {
	struct regulator *reg_vgpu;
	struct regulator *reg_vsram_gpu;
};
struct gpufreq_sb_info {
	int up;
	int down;
};
struct gpufreq_status {
	struct gpufreq_opp_info *signed_table;
	struct gpufreq_opp_info *working_table;
	struct gpufreq_sb_info *sb_table;
	int buck_count;
	int mtcmos_count;
	int cg_count;
	int power_count;
	unsigned int segment_id;
	int signed_opp_num;
	int segment_upbound;
	int segment_lowbound;
	int opp_num;
	int max_oppidx;
	int min_oppidx;
	int cur_oppidx;
	unsigned int cur_freq;
	unsigned int cur_volt;
	unsigned int cur_vsram;
};

/**************************************************
 * GPU Platform OPP Table Definition
 **************************************************/
#define SIGNED_OPP_GPU_NUM              ARRAY_SIZE(g_opp_table_segment_1)

struct gpufreq_opp_info g_opp_table_segment_1[] = {
	GPUOP(950000, 90000, 100000, POSDIV_POWER_4, 625, 0), /* 0 sign off */
	GPUOP(940000, 89375, 99375,  POSDIV_POWER_4, 625, 0), /* 1 */
	GPUOP(930000, 88750, 98750,  POSDIV_POWER_4, 625, 0), /* 2 */
	GPUOP(920000, 88125, 98125,  POSDIV_POWER_4, 625, 0), /* 3 */
	GPUOP(910000, 87500, 97500,  POSDIV_POWER_4, 625, 0), /* 4 */
	GPUOP(900000, 86875, 96875,  POSDIV_POWER_4, 625, 0), /* 5 */
	GPUOP(890000, 86250, 96250,  POSDIV_POWER_4, 625, 0), /* 6 */
	GPUOP(880000, 85625, 95625,  POSDIV_POWER_4, 625, 0), /* 7 */
	GPUOP(870000, 85000, 95000,  POSDIV_POWER_4, 625, 0), /* 8 */
	GPUOP(860000, 84375, 94375,  POSDIV_POWER_4, 625, 0), /* 9 */
	GPUOP(850000, 83750, 93750,  POSDIV_POWER_4, 625, 0), /*10 */
	GPUOP(840000, 83125, 93125,  POSDIV_POWER_4, 625, 0), /*11 */
	GPUOP(830000, 82500, 92500,  POSDIV_POWER_4, 625, 0), /*12 */
	GPUOP(820000, 81875, 91875,  POSDIV_POWER_4, 625, 0), /*13 */
	GPUOP(810000, 81250, 91250,  POSDIV_POWER_4, 625, 0), /*14 */
	GPUOP(800000, 80625, 90625,  POSDIV_POWER_4, 625, 0), /*15 */
	GPUOP(790000, 80000, 90000,  POSDIV_POWER_4, 625, 0), /*16 sign off */
	GPUOP(775000, 79375, 89375,  POSDIV_POWER_4, 625, 0), /*17 */
	GPUOP(761000, 78750, 88750,  POSDIV_POWER_4, 625, 0), /*18 */
	GPUOP(746000, 78125, 88125,  POSDIV_POWER_4, 625, 0), /*19 */
	GPUOP(732000, 77500, 87500,  POSDIV_POWER_4, 625, 0), /*20 */
	GPUOP(718000, 76875, 86875,  POSDIV_POWER_4, 625, 0), /*21 */
	GPUOP(703000, 76250, 86250,  POSDIV_POWER_4, 625, 0), /*22 */
	GPUOP(689000, 75625, 85625,  POSDIV_POWER_4, 625, 0), /*23 */
	GPUOP(675000, 75000, 85000,  POSDIV_POWER_4, 625, 0), /*24 */
	GPUOP(660000, 74375, 85000,  POSDIV_POWER_4, 625, 0), /*25 */
	GPUOP(646000, 73750, 85000,  POSDIV_POWER_4, 625, 0), /*26 */
	GPUOP(631000, 73125, 85000,  POSDIV_POWER_4, 625, 0), /*27 */
	GPUOP(617000, 72500, 85000,  POSDIV_POWER_4, 625, 0), /*28 */
	GPUOP(603000, 71875, 85000,  POSDIV_POWER_4, 625, 0), /*29 */
	GPUOP(588000, 71250, 85000,  POSDIV_POWER_4, 625, 0), /*30 */
	GPUOP(574000, 70625, 85000,  POSDIV_POWER_4, 625, 0), /*31 */
	GPUOP(560000, 70000, 85000,  POSDIV_POWER_4, 625, 0), /*32 sign off */
	GPUOP(543000, 69375, 85000,  POSDIV_POWER_4, 625, 0), /*33 */
	GPUOP(527000, 68750, 85000,  POSDIV_POWER_4, 625, 0), /*34 */
	GPUOP(511000, 68125, 85000,  POSDIV_POWER_4, 625, 0), /*35 */
	GPUOP(495000, 67500, 85000,  POSDIV_POWER_4, 625, 0), /*36 */
	GPUOP(478000, 66875, 85000,  POSDIV_POWER_4, 625, 0), /*37 */
	GPUOP(462000, 66250, 85000,  POSDIV_POWER_4, 625, 0), /*38 */
	GPUOP(446000, 65625, 85000,  POSDIV_POWER_4, 625, 0), /*39 */
	GPUOP(430000, 65000, 85000,  POSDIV_POWER_4, 625, 0), /*40 */
	GPUOP(413000, 64375, 85000,  POSDIV_POWER_4, 625, 0), /*41 */
	GPUOP(397000, 63750, 85000,  POSDIV_POWER_4, 625, 0), /*42 */
	GPUOP(381000, 63125, 85000,  POSDIV_POWER_4, 625, 0), /*43 */
	GPUOP(365000, 62500, 85000,  POSDIV_POWER_8, 625, 0), /*44 */
	GPUOP(348000, 61875, 85000,  POSDIV_POWER_8, 625, 0), /*45 */
	GPUOP(332000, 61250, 85000,  POSDIV_POWER_8, 625, 0), /*46 */
	GPUOP(316000, 60625, 85000,  POSDIV_POWER_8, 625, 0), /*47 */
	GPUOP(300000, 60000, 85000,  POSDIV_POWER_8, 625, 0), /*48 sign off */
};
/**************************************************
 * PTPOD Adjustment
 **************************************************/
#define PTPOD_OPP_GPU_NUM              ARRAY_SIZE(g_ptpod_opp_idx_table)
unsigned int g_ptpod_opp_idx_table[] = {
	0, 16, 19, 22,
	25, 28, 30, 32,
	34, 36, 38, 40,
	42, 44, 46, 48
};

struct mt_gpufreq_power_table_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_volt;
	unsigned int gpufreq_power;
};

#endif /* __GPUFREQ_MT6781_H__ */
