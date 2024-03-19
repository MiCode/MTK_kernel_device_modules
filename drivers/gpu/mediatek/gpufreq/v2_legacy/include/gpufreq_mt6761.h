/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#ifndef __GPUFREQ_MT6761_H__
#define __GPUFREQ_MT6761_H__
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
#define GPUFREQ_CUST_INIT_OPPIDX        (0)

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
#define MFGPLL_FH_PLL                   (3)
#define MFGPLL_CON0				        (g_apmixed_base + 0x218)
#define MFGPLL_CON1				        (g_apmixed_base + 0x21c)
/**************************************************
 * Frequency Hopping Setting
 **************************************************/
#define GPUFREQ_FHCTL_ENABLE            (1)
#define MFG_PLL_NAME                    "mfgpll"
/**************************************************
 * Power Domain Setting
 **************************************************/
#define GPUFREQ_CHECK_MTCMOS_PWR_STATUS (0)

/**************************************************
 * Reference Power Setting MT6761
 **************************************************/
#define GPU_ACT_REF_POWER			(1285)		/* mW  */
#define GPU_ACT_REF_FREQ			(900000)	/* KHz */
#define GPU_ACT_REF_VOLT			(90000)		/* mV x 100 */
#define GPU_DVFS_PTPOD_DISABLE_VOLT	(65000)		/* mV x 100 */
#define GPU_LEAKAGE_POWER               (71)

/**************************************************
 * PMIC Setting MT6761
 **************************************************/
#define VGPU_MAX_VOLT		(131250)         /* mV x 100 */
#define VGPU_MIN_VOLT       (51875)         /* mV x 100 */
#define VSRAM_MAX_VOLT      (87500)        /* mV x 100 */
#define VSRAM_MIN_VOLT      (87500)         /* mV x 100 */
#define DELAY_FACTOR		(625)
/*
 * (0)mv <= (VSRAM - VGPU) <= (250)mV
 */
#define MAX_BUCK_DIFF                   (25000)         /* mV x 100 */
#define MIN_BUCK_DIFF                   (10000)        /* mV x 100 */

/*
 * (Vgpu > THRESH): Vsram = Vgpu + DIFF
 * (Vgpu <= THRESH): Vsram = FIXED_VOLT
 */
#define VSRAM_FIXED_THRESHOLD           (75000)
#define VSRAM_FIXED_VOLT                (85000)
#define VSRAM_FIXED_DIFF                (10000)
#define VOLT_NORMALIZATION(volt) \
	((volt % 625) ? (volt - (volt % 625) + 625) : volt)

/*
 * PMIC hardware range:
 * vgpu      0.4 ~ 1.19300 V (MT6368)
 * vsram     0.5 ~ 1.29300 V (MT6363)
 */
#define PMIC_STEP                       (625)           /* mV x 100 */

/**************************************************
 * SRAMRC Setting
 **************************************************/
#define GPUFREQ_SAFE_VLOGIC             (60000)

/**************************************************
 * Power Throttling Setting
 **************************************************/
//over current and lot battery disabled so OC freq / Low batt freq dont matter
#define GPUFREQ_BATT_OC_ENABLE          (1)
#define GPUFREQ_BATT_PERCENT_ENABLE     (1)
#define GPUFREQ_LOW_BATT_ENABLE         (1)
#define GPUFREQ_BATT_OC_FREQ            (485000)
#define GPUFREQ_BATT_PERCENT_IDX        (0)
#define GPUFREQ_LOW_BATT_FREQ           (485000)
/**************************************************
 * Enumeration MT6761
 * Remove below segments for never used
 * MT6762M_SEGMENT, MT6762_SEGMENT, MT6761T_SEGMENT
 **************************************************/
enum gpufreq_segment {
	MT6761_SEGMENT = 1,
	MT6761D_SEGMENT,
};

enum gpufreq_clk_src {
	CLOCK_MAIN = 0,
	CLOCK_SUB,
};
/**************************************************
 * Structure MT6761
 **************************************************/
struct g_clk_info {
	struct clk *clk_mux;/* main clock for mfg setting*/
	struct clk *clk_main_parent;/* sub clock for mfg trans mux setting*/
	struct clk *clk_sub_parent; /* sub clock for mfg trans parent setting*/
	struct clk *pd_mfg_async;
	struct clk *pd_mfg; /* dependent on pd_mfg_async */
	struct clk *pd_mfg_core0; /* dependent on pd_mfg */
};

struct g_pmic_info {
	//struct regulator *reg_vgpu;
	//struct regulator *reg_vsram_gpu;
	struct regulator *reg_vcore;
	struct regulator *mtk_pm_vgpu;
};

struct gpufreq_sb_info {
	int up;
	int down;
};

//gpufreq_adj_info defined in gpufreq_v2_legacy.h
#if 0
struct gpufreq_adj_info {
	int oppidx;
	unsigned int freq;
	unsigned int volt;
	unsigned int vsram;
	unsigned int vaging;
};
#endif

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

struct mt_gpufreq_power_table_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_volt;
	unsigned int gpufreq_power;
};

/**************************************************
 * GPU Platform OPP Table Definition
 **************************************************/
#define SIGNED_OPP_GPU_NUM              ARRAY_SIZE(g_default_gpu_segment1)
struct gpufreq_opp_info g_default_gpu[] = {
	GPUOP(660000, 80000, 87500, 0, 0, 0),
	GPUOP(500000, 70000, 87500, 0, 0, 0),
	GPUOP(390000, 65000, 87500, 0, 0, 0),
};

/* OPP table based on segment : followed from legacy
 * Freq , Volt , Vsram required --> Fetched from opp table
 * Posdiv --> Not fetched from table for use. Hardcoded values used in code
 * Vagaing --> Disabled on default load[No entry for "enable-aging" in load dts].
 * Required on aging load
 * power --> 0 in mt6855/mt6768 . Keep same
 */
// MT6761 segment_1
struct gpufreq_opp_info g_default_gpu_segment1[] = {
	GPUOP(660000, 80000, 87500, 0, 0, 0),
	GPUOP(500000, 70000, 87500, 0, 0, 0),
	GPUOP(390000, 65000, 87500, 0, 0, 0),
};

// MT6761D segment_2
struct gpufreq_opp_info g_default_gpu_segment2[] = {
	GPUOP(550000, 80000, 87500, 0, 0, 0),
	GPUOP(500000, 70000, 87500, 0, 0, 0),
	GPUOP(390000, 65000, 87500, 0, 0, 0),
};
/**************************************************
 * Segment Adjustment
 **************************************************/
//TODO:GKI
#if 0
#define SEGMENT_ADJ_NUM                 ARRAY_SIZE(g_segment_adj)
struct gpufreq_adj_info g_segment_adj[] = {
	ADJOP(25, 0, 65000, 0, 0), /* sign off */
	ADJOP(26, 0, 64375, 0, 0),
	ADJOP(27, 0, 64375, 0, 0),
	ADJOP(28, 0, 63750, 0, 0),
	ADJOP(29, 0, 63750, 0, 0),
	ADJOP(30, 0, 63125, 0, 0),
	ADJOP(31, 0, 63125, 0, 0),
	ADJOP(32, 0, 62500, 0, 0),
	ADJOP(33, 0, 62500, 0, 0),
	ADJOP(34, 0, 61875, 0, 0),
	ADJOP(35, 0, 61875, 0, 0),
	ADJOP(36, 0, 61250, 0, 0),
	ADJOP(37, 0, 61250, 0, 0),
	ADJOP(38, 0, 60625, 0, 0),
	ADJOP(39, 0, 60625, 0, 0),
	ADJOP(40, 0, 60000, 0, 0), /* sign off */
};

/* MCL50 flavor load */
#define MCL50_ADJ_NUM                   ARRAY_SIZE(g_mcl50_adj)
struct gpufreq_adj_info g_mcl50_adj[] = {
	ADJOP(0,  0, 68750, VSRAM_LEVEL_0, 0), /*  0 sign off */
	ADJOP(1,  0, 68750, VSRAM_LEVEL_0, 0),
	ADJOP(2,  0, 68125, VSRAM_LEVEL_0, 0),
	ADJOP(3,  0, 67500, VSRAM_LEVEL_0, 0),
	ADJOP(4,  0, 66875, VSRAM_LEVEL_0, 0),
	ADJOP(5,  0, 66250, VSRAM_LEVEL_0, 0),
	ADJOP(6,  0, 65625, VSRAM_LEVEL_0, 0),
	ADJOP(7,  0, 65000, VSRAM_LEVEL_0, 0),
	ADJOP(8,  0, 64375, 0,             0), /*  8 sign off */
	ADJOP(9,  0, 64375, 0,             0),
	ADJOP(10, 0, 63750, 0,             0),
	ADJOP(11, 0, 63125, 0,             0),
	ADJOP(12, 0, 62500, 0,             0),
	ADJOP(13, 0, 62500, 0,             0),
	ADJOP(14, 0, 61875, 0,             0),
	ADJOP(15, 0, 61250, 0,             0),
	ADJOP(16, 0, 60625, 0,             0), /* 16 sign off */
	ADJOP(17, 0, 60625, 0,             0),
	ADJOP(18, 0, 60000, 0,             0),
	ADJOP(19, 0, 59375, 0,             0),
	ADJOP(20, 0, 58750, 0,             0),
	ADJOP(21, 0, 58750, 0,             0),
	ADJOP(22, 0, 58125, 0,             0),
	ADJOP(23, 0, 57500, 0,             0),
	ADJOP(24, 0, 56875, 0,             0),
	ADJOP(25, 0, 56875, 0,             0),
	ADJOP(26, 0, 56875, 0,             0),
	ADJOP(27, 0, 56875, 0,             0),
	ADJOP(28, 0, 56250, 0,             0),
	ADJOP(29, 0, 56250, 0,             0),
	ADJOP(30, 0, 56250, 0,             0),
	ADJOP(31, 0, 56250, 0,             0),
	ADJOP(32, 0, 55625, 0,             0), /* 32 sign off */
	ADJOP(33, 0, 55625, 0,             0),
	ADJOP(34, 0, 55625, 0,             0),
	ADJOP(35, 0, 55625, 0,             0),
	ADJOP(36, 0, 55000, 0,             0),
	ADJOP(37, 0, 55000, 0,             0),
	ADJOP(38, 0, 55000, 0,             0),
	ADJOP(39, 0, 55000, 0,             0),
	ADJOP(40, 0, 54375, 0,             0), /* 40 sign off */
};
/**************************************************
 * Aging Adjustment
 **************************************************/
/*
 * todo: Need to check correct table for applying aging
 */
unsigned int g_aging_table[][SIGNED_OPP_GPU_NUM] = {
	{ /* aging table 0 */
		1875, 1875, 1875, 1875, 1875, 1875, 1875, 1875, 1875, 1875, /* OPP 0~9   */
		1875, 1875, 1875, 1875, 1875, 1250, 1250, 1250, 1250, 1250, /* OPP 10~19 */
		1250, 1250, 1250, 1250, 1250, 1250, 1250, 1250, 1250, 625,  /* OPP 20~29 */
		625,  625,  625,  625,  625,  625,  625,  625,  625,  625,  /* OPP 30~39 */
		625,  625,  625,  625,  625,                                /* OPP 40~44 */
	},
	{ /* aging table 1 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 0~9   */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 10~19 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 20~29 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 30~39 */
		0, 0, 0, 0, 0,                /* OPP 40~44 */
	},
	{ /* aging table 2 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 0~9   */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 10~19 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 20~29 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 30~39 */
		0, 0, 0, 0, 0,                /* OPP 40~44 */
	},
	{ /* aging table 3 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 0~9   */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 10~19 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 20~29 */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* OPP 30~39 */
		0, 0, 0, 0, 0,                /* OPP 40~44 */
	},
};
//TODO:GKI
#endif
#endif /* __GPUFREQ_MT6761_H__ */
