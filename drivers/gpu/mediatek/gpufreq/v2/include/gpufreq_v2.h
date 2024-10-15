/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_V2_H__
#define __GPUFREQ_V2_H__

#include <linux/bits.h>
#include <uapi/asm-generic/errno-base.h>

/**************************************************
 * Definition
 **************************************************/
#define GPUFREQ_DEBUG_ENABLE            (0)
#define GPUFREQ_TRACE_ENABLE            (0)
#define GPUFREQ_FORCE_WDT_ENABLE        (0)
#define GPUFERQ_TAG                     "[GPU/FREQ]"
#define GPUFREQ_TRACE_TAG               "[GPU/TRACE]"
#define GPUFREQ_MEM_TABLE_IDX           (1)
#define GPUFREQ_MAGIC_NUMBER            (0xBABADADA)
#define GPUFREQ_MAX_OPP_NUM             (70)
#define GPUFREQ_MAX_ADJ_NUM             (10)
#define GPUFREQ_MAX_REG_NUM             (70)
#define GPUFREQ_MAX_GPM3_NUM            (20)
#define GPUFREQ_MAX_BUSTRK_NUM          (10)
#define GPUFREQ_DUMP_INFRA_SIZE         (8192)
#define GPUFREQ_UNREFERENCED(param)     ((void)(param))
#define GPUFREQ_PROFILE_TYPE_STRING(type) \
	( \
		type == PROF_PWR_ON ? __stringify(PWR_ON) : \
		type == PROF_PWR_OFF ? __stringify(PWR_OFF) : \
		type == PROF_ACTIVE ? __stringify(ACTIVE) : \
		type == PROF_SLEEP ? __stringify(SLEEP) : \
		type == PROF_DVFS ? __stringify(DVFS) : "UNKNOWN" \
	)
#define GPUFREQ_BUS_TRACKER_TYPE_STRING(type) \
	( \
		type == BUS_VCORE_AR ? __stringify(VCORE_AR) : \
		type == BUS_VCORE_AW ? __stringify(VCORE_AW) : \
		type == BUS_VGPU_AR ? __stringify(VGPU_AR) : \
		type == BUS_VGPU_AW ? __stringify(VGPU_AW) : \
		type == BUS_GPUEB_AR ? __stringify(GPUEB_AR) : \
		type == BUS_GPUEB_AW ? __stringify(GPUEB_AW) : "UNKNOWN" \
	)

/**************************************************
 * GPUFREQ Log Setting
 **************************************************/
#define GPUFREQ_LOGE(fmt, args...) \
	pr_err(GPUFERQ_TAG"[ERROR]@%s: "fmt"\n", __func__, ##args)
#define GPUFREQ_LOGW(fmt, args...) \
	pr_debug(GPUFERQ_TAG"[WARN]@%s: "fmt"\n", __func__, ##args)
#define GPUFREQ_LOGI(fmt, args...) \
	pr_info(GPUFERQ_TAG"[INFO]@%s: "fmt"\n", __func__, ##args)
#define GPUFREQ_LOGB(buf, len, size, fmt, args...) \
	{ \
		pr_info(GPUFERQ_TAG"[INFO]@%s: "fmt"\n", __func__, ##args); \
		if (buf && len) \
			*len += snprintf(buf + *len, size - *len, fmt"\n", ##args); \
	}

#if GPUFREQ_DEBUG_ENABLE
	#define GPUFREQ_LOGD(fmt, args...) \
		pr_info(GPUFERQ_TAG"[DEBUG]@%s: "fmt"\n", __func__, ##args)
#else
	#define GPUFREQ_LOGD(fmt, args...) {}
#endif /* GPUFREQ_DEBUG_ENABLE */

#if GPUFREQ_TRACE_ENABLE
	#define GPUFREQ_TRACE_START(fmt, args...) \
		pr_info(GPUFREQ_TRACE_TAG" + %s("fmt")\n", __func__, ##args)
	#define GPUFREQ_TRACE_END(fmt, args...) \
		pr_info(GPUFREQ_TRACE_TAG" - %s("fmt")\n", __func__, ##args)
#else
	#define GPUFREQ_TRACE_START(fmt, args...) {}
	#define GPUFREQ_TRACE_END(fmt, args...) {}
#endif /* GPUFREQ_TRACE_ENABLE */

/**************************************************
 * Enumeration
 **************************************************/
enum gpufreq_return {
	GPUFREQ_HW_LIMIT = 1,
	GPUFREQ_SUCCESS  = 0,
	GPUFREQ_EINVAL   = -EINVAL,  /* -22 */
	GPUFREQ_ENOMEM   = -ENOMEM,  /* -12 */
	GPUFREQ_ENOENT   = -ENOENT,  /* -2  */
	GPUFREQ_ENODEV   = -ENODEV,  /* -19 */
	GPUFREQ_EACCES   = -EACCES,  /* -13 */
};

enum gpufreq_posdiv {
	POSDIV_POWER_1 = 0,
	POSDIV_POWER_2 = 1,
	POSDIV_POWER_4,
	POSDIV_POWER_8,
	POSDIV_POWER_16,
};

enum gpufreq_dvfs_state {
	DVFS_FREE          = 0,       /* 0000 0000 0000 */
	DVFS_DISABLE       = BIT(0),  /* 0000 0000 0001 */
	DVFS_POWEROFF      = BIT(1),  /* 0000 0000 0010 */
	DVFS_FIX_OPP       = BIT(2),  /* 0000 0000 0100 */
	DVFS_FIX_FREQ_VOLT = BIT(3),  /* 0000 0000 1000 */
	DVFS_AGING_KEEP    = BIT(4),  /* 0000 0001 0000 */
	DVFS_SLEEP         = BIT(5),  /* 0000 0010 0000 */
	DVFS_MSSV_TEST     = BIT(6),  /* 0000 0100 0000 */
	DVFS_VMETER_CALI   = BIT(7),  /* 0000 1000 0000 */
	DVFS_PRE_SLEEP     = BIT(8),  /* 0001 0000 0000 */
	DVFS_PTP3_DEBUG    = BIT(9),  /* 0010 0000 0000 */
	DVFS_SES_CALI      = BIT(10), /* 0100 0000 0000 */
};

enum gpufreq_target {
	TARGET_DEFAULT = 0,
	TARGET_GPU     = 1,
	TARGET_STACK,
	TARGET_INVALID,
};

enum gpufreq_power_state {
	GPU_PWR_OFF = 0,
	GPU_PWR_ON,
};

enum gpufreq_test_mode {
	TEST_NORMAL    = 0,
	TEST_ADVANCED  = 1,
	TEST_PRIVILEGE = 2,
};

enum gpufreq_config_target {
	CONFIG_TARGET_INVALID        = -1,
	CONFIG_TEST_MODE             = 0,
	CONFIG_STRESS_TEST           = 1,
	CONFIG_MARGIN                = 2,
	CONFIG_GPM1                  = 3,
	CONFIG_GPM3                  = 4,
	CONFIG_DFD                   = 5,
	CONFIG_IMAX_GPU              = 6,
	CONFIG_IMAX_STACK            = 7,
	CONFIG_IMAX_SRAM             = 8,
	CONFIG_PMAX_STACK            = 9,
	CONFIG_DYN_GPU               = 10,
	CONFIG_DYN_STACK             = 11,
	CONFIG_DYN_SRAM_GPU          = 12,
	CONFIG_DYN_SRAM_STACK        = 13,
	CONFIG_IPS                   = 14,
	CONFIG_FAKE_MTCMOS_CTRL      = 15,
	CONFIG_MCUETM_CLK            = 16,
	CONFIG_PTP3                  = 17,
	CONFIG_MFG2_BEFORE_OFF       = 18,
	CONFIG_DEVAPC_HANDLE         = 19,
	CONFIG_GPU_PROFILING         = 20,
	CONFIG_AP_IMPL_BOUNDARY      = 100, /* implement on AP */
	CONFIG_WB_TEST_ONCE          = 101,
	CONFIG_WB_MFG1_SLAVE_STRESS  = 102,
};

enum gpufreq_config_value {
	CONFIG_VAL_INVALID = -2,
	CONFIG_VAL_IGNORE  = -1,
	FEAT_DISABLE       = 0,
	FEAT_ENABLE        = 1,
	DFD_FORCE_DUMP     = 2,
	IPS_VMIN_GET       = 3,
	STRESS_RANDOM      = 4,
	STRESS_TRAVERSE    = 5,
	STRESS_MAX_MIN     = 6,
	STRESS_ASCENDING   = 7,
	STRESS_DESCENDING  = 8,
	STRESS_SLT2        = 9,
	PTP3_SAFE_MARGIN   = 10,
	DATA_UPDATE        = 11,
	DATA_LEVEL_0       = 12,
	DATA_LEVEL_1       = 13,
	DATA_LEVEL_2       = 14,
};

enum gpufreq_chip_type {
	GPUIC_NORMAL      = 0,
	GPUIC_OVERDRIVE   = 1,
};

enum gpuppm_reserved_idx {
	GPUPPM_DEFAULT_IDX = -1,
	GPUPPM_RESET_IDX   = -2,
	GPUPPM_KEEP_IDX    = -3,
};

enum gpuppm_limiter {
	LIMIT_SEGMENT      = 0,
	LIMIT_DEBUG        = 1,
	LIMIT_GPM3         = 2,
	LIMIT_PEAK_POWER   = 3,
	LIMIT_THERMAL_AP   = 4,
	LIMIT_THERMAL_EB   = 5,
	LIMIT_SRAMRC       = 6,
	LIMIT_BATT_OC      = 7,
	LIMIT_BATT_PERCENT = 8,
	LIMIT_LOW_BATT     = 9,
	LIMIT_PBM          = 10,
	LIMIT_APIBOOST     = 11,
	LIMIT_POWERHAL     = 12,
	LIMIT_FIXCMD       = 13,
	LIMIT_NUM          = 14,
};

enum gpuppm_limit_type {
	GPUPPM_CEILING = 0,
	GPUPPM_FLOOR   = 1,
	GPUPPM_INVALID,
};

enum gpufreq_opp_direct {
	SCALE_DOWN = 0,
	SCALE_UP,
	SCALE_STAY,
};

enum gpufreq_dvfs_mode {
	LEGACY_SW_DVFS    = 0, /* default */
	SW_DUAL_LOOP_DVFS = 1,
	HW_DUAL_LOOP_DVFS = 2,
};

enum gpufreq_delsel_mode {
	SW_DELSEL = 0, /* default */
	HW_DELSEL = 1,
};

enum gpufreq_gpm3_prot_mode {
	GPM3_DISABLE     = 0, /* default */
	GPM3_0_IMAX_PROT = 1,
	GPM3_5_IMAX_PROT = 2,
};

enum gpufreq_brcast_mode {
	BRCAST_DISABLE       = 0, /* default */
	BRCAST_SW_REFILLED   = 1,
	BRCAST_WITH_AUTO_DMA = 2,
	BRCAST_SW_ONLY_ACK   = 3, /* SW refill and only check ACK */
};

enum gpufreq_brisket_mode {
	BRISKET_UNSUPPORTED = -1,
	BRISKET_DISABLE     = 0, /* default */
	BRISKET_ENABLE      = 1,
};

enum gpufreq_dfd_mode {
	GPU_DFD_DISABLE = 0, /* default */
	GPU_DFD2_0      = 1,
	GPU_DFD3_6      = 2,
	GPU_DFD6_0      = 3,
};

enum gpufreq_profile_type {
	PROF_PWR_ON   = 0,
	PROF_PWR_OFF  = 1,
	PROF_ACTIVE   = 2,
	PROF_SLEEP    = 3,
	PROF_DVFS     = 4,
	PROF_TYPE_NUM = 5,
};

enum gpufreq_profile_index {
	PROF_IDX_START = 0,
	PROF_IDX_END   = 1,
	PROF_IDX_COUNT = 2,
	PROF_IDX_ONCE  = 3,
	PROF_IDX_TOTAL = 4,
	PROF_IDX_AVG   = 5,
	PROF_IDX_MAX   = 6,
	PROF_IDX_MIN   = 7,
	PROF_IDX_NUM   = 8,
};

enum gpufreq_profile_op {
	PROF_OP_START  = 0,
	PROF_OP_END    = 1,
	PROF_OP_RESULT = 2,
};

enum gpufreq_bus_tracker_type {
	BUS_VCORE_AR = 0,
	BUS_VCORE_AW = 1,
	BUS_VGPU_AR  = 2,
	BUS_VGPU_AW  = 3,
	BUS_GPUEB_AR = 4,
	BUS_GPUEB_AW = 5,
};

/**************************************************
 * Structure
 **************************************************/
struct gpufreq_opp_info {
	unsigned int freq;            /* KHz */
	unsigned int volt;            /* mV x 100 */
	unsigned int vsram;           /* mV x 100 */
	enum gpufreq_posdiv posdiv;
	unsigned int margin;          /* mV x 100 */
	unsigned int power;           /* mW */
};

struct gpufreq_adj_info {
	int oppidx;
	unsigned int freq;
	unsigned int volt;
	unsigned int vsram;
};

struct gpufreq_core_mask_info {
	unsigned int num;
	unsigned int mask;
};

struct gpuppm_limit_info {
	unsigned int limiter;
	char name[20];
	unsigned int priority;
	int ceiling;
	unsigned int c_enable;
	int floor;
	unsigned int f_enable;
};

struct gpufreq_asensor_info {
	unsigned int efuse1;
	unsigned int efuse2;
	unsigned int efuse3;
	unsigned int efuse4;
	unsigned int efuse1_addr;
	unsigned int efuse2_addr;
	unsigned int efuse3_addr;
	unsigned int efuse4_addr;
	unsigned int a_t0_efuse1;
	unsigned int a_t0_efuse2;
	unsigned int a_t0_efuse3;
	unsigned int a_t0_efuse4;
	unsigned int a_tn_sensor1;
	unsigned int a_tn_sensor2;
	unsigned int a_tn_sensor3;
	unsigned int a_tn_sensor4;
	int a_diff1;
	int a_diff2;
	int a_diff3;
	int a_diff4;
	int tj_max;
	unsigned int aging_table_idx;
	unsigned int aging_table_idx_agrresive;
	unsigned int leakage_power;
	unsigned int lvts5_0_y_temperature;
};

struct gpufreq_ips_info {
	unsigned int vmin_reg_val;
	unsigned int vmin_val;
	unsigned int autok_result;
	unsigned int autok_trim0;
	unsigned int autok_trim1;
	unsigned int autok_trim2;
};

struct gpufreq_gpm3_info {
	int temper;
	int ceiling;
	unsigned int i_stack;
	unsigned int i_sram;
	unsigned int p_stack;
};

struct gpufreq_reg_info {
	unsigned int addr;
	unsigned int val;
};

struct gpufreq_ptp3_shared_status {
	enum gpufreq_dvfs_mode dvfs_mode;
	enum gpufreq_gpm3_prot_mode gpm3_prot_mode;
	enum gpufreq_brcast_mode brcast_mode;
	enum gpufreq_delsel_mode delsel_mode;
	unsigned int ptp3_mode;
	unsigned int ptp3_debug_mode;
	unsigned int hbvc_freq_ctrl_support;
	unsigned int hbvc_volt_ctrl_support;
	unsigned int hbvc_preoc_support;
	unsigned int hbvc_preoc_mode;
	unsigned int hbvc_vgpu_upper_bound;
	unsigned int hbvc_vgpu_lower_bound;
	unsigned int hbvc_vstack_upper_bound;
	unsigned int hbvc_vstack_lower_bound;
	unsigned int brisket_fll_mode;
	enum gpufreq_brisket_mode brisket_atmc_mode;
	enum gpufreq_brisket_mode brisket_vmeter_mode;
	enum gpufreq_brisket_mode brisket_tmeter_mode;
	unsigned int brisket_cpmeter_mode;
	unsigned int brisket_ctt_mode;
	unsigned int brisket_safe_margin;
	unsigned int auto_dma_refill_top_brisket;
	unsigned int auto_dma_refill_top_gpm;
	unsigned int ses_top_support;
	unsigned int ses_top_mode;
	unsigned int ses_stack_support;
	unsigned int ses_stack_mode;
	unsigned int ses_scheduler_support;
	unsigned int ses_scheduler_mode;
};

struct gpu_ptp3_info {
	unsigned int infreq0;
	unsigned int outfreq0;
	unsigned int infreq1;
	unsigned int outfreq1;
	unsigned int hw_cc;
	unsigned int hw_fc;
	unsigned int sw_cc;
	unsigned int sw_fc;
};

struct gpufreq_bus_tracker_info {
	enum gpufreq_bus_tracker_type type;
	unsigned long long timestamp;
	unsigned int log;
	unsigned int id;
	unsigned int addr;
};

struct gpufreq_preoc_info {
	unsigned int count;
	unsigned int throttle_time; /* us */
	unsigned int scale_factor;
};

struct gpufreq_slt2_bmodel {
	unsigned int vgpu_h1;
	unsigned int vgpu_h;
	unsigned int vgpu_l;
	unsigned int vstack_h1;
	unsigned int vstack_h;
	unsigned int vstack_l;
};

/**************************************************
 * Shared Status
 **************************************************/
#define GPUFREQ_SHARED_STATUS_SIZE      (sizeof(struct gpufreq_shared_status))
struct gpufreq_shared_status {
	int magic;
	int cur_oppidx_gpu;
	int cur_oppidx_stack;
	int opp_num_gpu;
	int opp_num_stack;
	int signed_opp_num_gpu;
	int signed_opp_num_stack;
	int power_count;
	int buck_count;
	int mtcmos_count;
	int cg_count;
	int active_count;
	int temperature;
	int temper_comp_norm_gpu;
	int temper_comp_high_gpu;
	int temper_comp_norm_stack;
	int temper_comp_high_stack;
	unsigned int cur_fgpu;
	unsigned int cur_fstack;
	unsigned int cur_out_fgpu;
	unsigned int cur_out_fstack;
	unsigned int cur_fmeter_fgpu;
	unsigned int cur_fmeter_fstack;
	unsigned int cur_vgpu;
	unsigned int cur_vstack;
	unsigned int cur_vsram_gpu;
	unsigned int cur_vsram_stack;
	unsigned int cur_regulator_vgpu;
	unsigned int cur_regulator_vstack;
	unsigned int cur_regulator_vsram_gpu;
	unsigned int cur_regulator_vsram_stack;
	unsigned int cur_power_gpu;
	unsigned int cur_power_stack;
	unsigned int max_power_gpu;
	unsigned int max_power_stack;
	unsigned int min_power_gpu;
	unsigned int min_power_stack;
	unsigned int lkg_rt_info_gpu;
	unsigned int lkg_rt_info_stack;
	unsigned int lkg_rt_info_sram;
	unsigned int lkg_ht_info_gpu;
	unsigned int lkg_ht_info_stack;
	unsigned int lkg_ht_info_sram;
	unsigned int dac_low_vgpu;
	unsigned int dac_low_vstack;
	unsigned int cur_ceiling;
	unsigned int cur_floor;
	unsigned int cur_c_limiter;
	unsigned int cur_f_limiter;
	unsigned int cur_c_priority;
	unsigned int cur_f_priority;
	unsigned int power_control;
	unsigned int active_sleep_control;
	unsigned int dvfs_state;
	unsigned int shader_present;
	unsigned int asensor_enable;
	unsigned int aging_load;
	unsigned int aging_margin;
	unsigned int avs_enable;
	unsigned int avs_margin;
	unsigned int chip_type;
	unsigned int sb_version;
	unsigned int ptp_version;
	unsigned int dbg_version;
	unsigned int kdbg_version;
	unsigned int gpm1_mode;
	unsigned int gpm3_mode;
	unsigned int dfd_mode;
	unsigned int dual_buck;
	unsigned int segment_id;
	unsigned int power_time_h;
	unsigned int power_time_l;
	unsigned int mfg_pwr_status;
	unsigned int stress_test;
	unsigned int test_mode;
	unsigned int ips_mode;
	unsigned int temper_comp_mode;
	unsigned int ht_temper_comp_mode;
	unsigned int power_tracker_mode;
	unsigned long long profile_time[PROF_TYPE_NUM][PROF_IDX_NUM];
	struct gpufreq_reg_info reg_stack_sel;
	struct gpufreq_reg_info reg_top_delsel;
	struct gpufreq_reg_info reg_stack_delsel;
	struct gpufreq_asensor_info asensor_info;
	struct gpufreq_ips_info ips_info;
	struct gpufreq_opp_info working_table_gpu[GPUFREQ_MAX_OPP_NUM];
	struct gpufreq_opp_info working_table_stack[GPUFREQ_MAX_OPP_NUM];
	struct gpufreq_opp_info signed_table_gpu[GPUFREQ_MAX_OPP_NUM];
	struct gpufreq_opp_info signed_table_stack[GPUFREQ_MAX_OPP_NUM];
	struct gpuppm_limit_info limit_table[LIMIT_NUM];
	struct gpufreq_adj_info aging_table_gpu[GPUFREQ_MAX_ADJ_NUM];
	struct gpufreq_adj_info aging_table_stack[GPUFREQ_MAX_ADJ_NUM];
	struct gpufreq_adj_info avs_table_gpu[GPUFREQ_MAX_ADJ_NUM];
	struct gpufreq_adj_info avs_table_stack[GPUFREQ_MAX_ADJ_NUM];
	struct gpufreq_gpm3_info gpm3_table[GPUFREQ_MAX_GPM3_NUM];
	struct gpufreq_ptp3_shared_status ptp3_status;
	struct gpu_ptp3_info ptp3_info;
	struct gpufreq_preoc_info preoc_info;
	struct gpufreq_bus_tracker_info bus_slv_error[GPUFREQ_MAX_BUSTRK_NUM];
	struct gpufreq_bus_tracker_info bus_slv_timeout[GPUFREQ_MAX_BUSTRK_NUM];
	struct gpufreq_slt2_bmodel slt2_bmodel;
};

/**************************************************
 * Platform Implementation
 **************************************************/
struct gpufreq_platform_fp {
	/* Common */
	unsigned int (*power_ctrl_enable)(void);
	unsigned int (*active_sleep_ctrl_enable)(void);
	unsigned int (*get_power_state)(void);
	unsigned int (*get_dvfs_state)(void);
	unsigned int (*get_shader_present)(void);
	int (*power_control)(enum gpufreq_power_state power);
	int (*active_sleep_control)(enum gpufreq_power_state power);
	void (*dump_infra_status)(char *log_buf, int *log_len, int log_size);
	void (*dump_power_tracker_status)(void);
	unsigned int (*bus_tracker_vio_handler)(void);
	void (*set_mfgsys_config)(enum gpufreq_config_target target, enum gpufreq_config_value val);
	struct gpufreq_core_mask_info *(*get_core_mask_table)(void);
	unsigned int (*get_core_num)(void);
	void (*pdca_config)(enum gpufreq_power_state power);
	void (*update_debug_opp_info)(void);
	void (*set_shared_status)(struct gpufreq_shared_status *shared_status);
	int (*mssv_commit)(unsigned int target, unsigned int val);
	int (*fix_target_oppidx_dual)(int oppidx_gpu, int oppidx_stack);
	int (*fix_custom_freq_volt_dual)(unsigned int fgpu, unsigned int vgpu,
		unsigned int fstack, unsigned int vstack);
	void (*update_temperature)(void);
	/* GPU */
	unsigned int (*get_cur_fgpu)(void);
	unsigned int (*get_cur_vgpu)(void);
	unsigned int (*get_cur_vsram_gpu)(void);
	unsigned int (*get_cur_pgpu)(void);
	unsigned int (*get_max_pgpu)(void);
	unsigned int (*get_min_pgpu)(void);
	int (*get_cur_idx_gpu)(void);
	int (*get_opp_num_gpu)(void);
	int (*get_signed_opp_num_gpu)(void);
	unsigned int (*get_fgpu_by_idx)(int oppidx);
	unsigned int (*get_pgpu_by_idx)(int oppidx);
	int (*get_idx_by_fgpu)(unsigned int freq);
	unsigned int (*get_lkg_pgpu)(unsigned int volt, int temper);
	unsigned int (*get_dyn_pgpu)(unsigned int freq, unsigned int volt);
	int (*fix_target_oppidx_gpu)(int oppidx);
	int (*fix_custom_freq_volt_gpu)(unsigned int freq, unsigned int volt);
	/* STACK */
	unsigned int (*get_cur_fstack)(void);
	unsigned int (*get_cur_vstack)(void);
	unsigned int (*get_cur_vsram_stack)(void);
	unsigned int (*get_cur_pstack)(void);
	unsigned int (*get_max_pstack)(void);
	unsigned int (*get_min_pstack)(void);
	int (*get_cur_idx_stack)(void);
	int (*get_opp_num_stack)(void);
	int (*get_signed_opp_num_stack)(void);
	unsigned int (*get_fstack_by_idx)(int oppidx);
	unsigned int (*get_pstack_by_idx)(int oppidx);
	int (*get_idx_by_fstack)(unsigned int freq);
	unsigned int (*get_lkg_pstack)(unsigned int volt, int temper);
	unsigned int (*get_dyn_pstack)(unsigned int freq, unsigned int volt);
	int (*fix_target_oppidx_stack)(int oppidx);
	int (*fix_custom_freq_volt_stack)(unsigned int freq, unsigned int volt);
};

struct gpuppm_platform_fp {
	int (*limited_commit)(enum gpufreq_target target, int oppidx);
	int (*limited_dual_commit)(int gpu_oppidx, int stack_oppidx);
	int (*set_limit)(enum gpufreq_target target, enum gpuppm_limiter limiter,
		int ceiling_info, int floor_info, unsigned int instant_dvfs);
	int (*switch_limit)(enum gpufreq_target target, enum gpuppm_limiter limiter,
		int c_enable, int f_enable, unsigned int instant_dvfs);
	int (*get_ceiling)(void);
	int (*get_floor)(void);
	unsigned int (*get_c_limiter)(void);
	unsigned int (*get_f_limiter)(void);
	void (*set_shared_status)(struct gpufreq_shared_status *shared_status);
};

/**************************************************
 * GPU HAL Interface
 **************************************************/
extern int (*mtk_get_gpu_limit_index_fp)(enum gpufreq_target target,
	enum gpuppm_limit_type limit);
extern unsigned int (*mtk_get_gpu_limiter_fp)(enum gpufreq_target target,
	enum gpuppm_limit_type limit);
extern unsigned int (*mtk_get_gpu_cur_freq_fp)(enum gpufreq_target target);
extern int (*mtk_get_gpu_cur_oppidx_fp)(enum gpufreq_target target);

/**************************************************
 * GED Interface
 **************************************************/
extern unsigned long (*ged_get_last_commit_idx_fp)(void);
extern unsigned long (*ged_get_last_commit_top_idx_fp)(void);
extern unsigned long (*ged_get_last_commit_stack_idx_fp)(void);

/**************************************************
 * External Function
 **************************************************/
/* Common */
unsigned int gpufreq_bringup(void);
unsigned int gpufreq_power_ctrl_enable(void);
unsigned int gpufreq_active_sleep_ctrl_enable(void);
unsigned int gpufreq_get_power_state(void);
unsigned int gpufreq_get_dvfs_state(void);
unsigned int gpufreq_get_shader_present(void);
unsigned int gpufreq_get_segment_id(void);
void gpufreq_dump_infra_status(void);
void gpufreq_dump_infra_status_logbuffer(char *log_buf, int *log_len, int log_size);
unsigned int gpufreq_get_cur_freq(enum gpufreq_target target);
unsigned int gpufreq_get_cur_out_freq(enum gpufreq_target target);
unsigned int gpufreq_get_cur_volt(enum gpufreq_target target);
unsigned int gpufreq_get_cur_vsram(enum gpufreq_target target);
unsigned int gpufreq_get_cur_power(enum gpufreq_target target);
unsigned int gpufreq_get_max_power(enum gpufreq_target target);
unsigned int gpufreq_get_min_power(enum gpufreq_target target);
int gpufreq_get_cur_oppidx(enum gpufreq_target target);
int gpufreq_get_opp_num(enum gpufreq_target target);
unsigned int gpufreq_get_freq_by_idx(enum gpufreq_target target, int oppidx);
unsigned int gpufreq_get_power_by_idx(enum gpufreq_target target, int oppidx);
int gpufreq_get_oppidx_by_freq(enum gpufreq_target target, unsigned int freq);
unsigned int gpufreq_get_leakage_power(enum gpufreq_target target, unsigned int volt);
unsigned int gpufreq_get_dynamic_power(enum gpufreq_target target,
	unsigned int freq, unsigned int volt);
int gpufreq_get_cur_temperature(void);
int gpufreq_set_limit(enum gpufreq_target target,
	enum gpuppm_limiter limiter, int ceiling_info, int floor_info);
int gpufreq_get_cur_limit_idx(enum gpufreq_target target,enum gpuppm_limit_type limit);
unsigned int gpufreq_get_cur_limiter(enum gpufreq_target target, enum gpuppm_limit_type limit);
int gpufreq_power_control(enum gpufreq_power_state power);
int gpufreq_active_sleep_control(enum gpufreq_power_state power);
int gpufreq_commit(enum gpufreq_target target, int oppidx);
int gpufreq_dual_commit(int gpu_oppidx, int stack_oppidx);
struct gpufreq_core_mask_info *gpufreq_get_core_mask_table(void);
unsigned int gpufreq_get_core_num(void);
void gpufreq_pdca_config(enum gpufreq_power_state power);
void gpufreq_fake_mtcmos_control(enum gpufreq_power_state power);
void gpufreq_register_gpufreq_fp(struct gpufreq_platform_fp *platform_fp);
void gpufreq_register_gpuppm_fp(struct gpuppm_platform_fp *platform_fp);

/* Debug */
int gpufreq_update_debug_opp_info(void);
const struct gpufreq_opp_info *gpufreq_get_working_table(enum gpufreq_target target);
int gpufreq_switch_limit(enum gpufreq_target target,
	enum gpuppm_limiter limiter, int c_enable, int f_enable);
int gpufreq_fix_target_oppidx(enum gpufreq_target target, int oppidx);
int gpufreq_fix_dual_target_oppidx(int gpu_oppidx, int stack_oppidx);
int gpufreq_fix_custom_freq_volt(enum gpufreq_target target,
	unsigned int freq, unsigned int volt);
int gpufreq_fix_dual_custom_freq_volt(unsigned int fgpu, unsigned int vgpu,
	unsigned int fstack, unsigned int vstack);
int gpufreq_set_mfgsys_config(enum gpufreq_config_target target, enum gpufreq_config_value val);
int gpufreq_mssv_commit(unsigned int target, unsigned int val);

#endif /* __GPUFREQ_V2_H__ */
