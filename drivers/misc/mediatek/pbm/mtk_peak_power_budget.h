/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#ifndef __MTK_PEAK_POWER_BUDGETING_H__
#define __MTK_PEAK_POWER_BUDGETING_H__


enum ppb_kicker {
	KR_BUDGET,
	KR_FLASHLIGHT,
	KR_AUDIO,
	KR_CAMERA,
	KR_DISPLAY,
	KR_APU,
	KR_NUM
};

enum ppb_sram_offset {
	PPB_VSYS_PWR_NOERR,
	HPT_SF_ENABLE,
	HPT_CPU_B_SF_L1,
	HPT_CPU_B_SF_L2,
	HPT_CPU_M_SF_L1,
	HPT_CPU_M_SF_L2,
	HPT_CPU_L_SF_L1,
	HPT_CPU_L_SF_L2,
	HPT_GPU_SF_L1,
	HPT_GPU_SF_L2,
	HPT_DELAY_TIME,
	PPB_MODE = 32,
	PPB_CG_PWR,
	PPB_VSYS_PWR,
	PPB_VSYS_ACK,
	PPB_FLASH_PWR,
	PPB_AUDIO_PWR,
	PPB_CAMERA_PWR,
	PPB_APU_PWR,
	PPB_DISPLAY_PWR,
	PPB_DRAM_PWR,
	PPB_MD_PWR,
	PPB_WIFI_PWR,
	PPB_RESERVE4,
	PPB_RESERVE5,
	PPB_APU_PWR_ACK,
	PPB_BOOT_MODE,
	PPB_MD_SMEM_ADDR,
	PPB_WIFI_SMEM_ADDR,
	PPB_CG_PWR_THD,
	PPB_CG_PWR_CNT,
	PPB_OFFSET_NUM
};

struct ppb_ctrl {
	u8 ppb_stop;
	u8 ppb_drv_done;
	u8 manual_mode;
	u8 ppb_mode;
};

enum hpt_ctrl_reg {
	HPT_CTRL,
	HPT_CTRL_SET,
	HPT_CTRL_CLR
};

struct ppb {
	unsigned int loading_flash;
	unsigned int loading_audio;
	unsigned int loading_camera;
	unsigned int loading_display;
	unsigned int loading_apu;
	unsigned int loading_dram;
	unsigned int vsys_budget;
	unsigned int vsys_budget_noerr;
	unsigned int remain_budget;
	unsigned int cg_budget_thd;
	unsigned int cg_budget_cnt;
};

struct power_budget_t {
	unsigned int version;
	int hpt_max_lv;
	int hpt_cur_lv;
	int hpt_lv_t[10];
	int soc;
	int uisoc;
	int combo0_uisoc;
	int fix_combo0[50];
	unsigned int uisoc_cur_stage;
	unsigned int uisoc_max_stage;
	int uisoc_thd[6];
	int temp;
	unsigned int soc_err;
	unsigned int temp_cur_stage;
	unsigned int temp_max_stage;
	int temp_thd[6];
	unsigned int aging_cur_stage;
	unsigned int aging_max_stage;
	unsigned int circuit_rdc;
	unsigned int rdc[7];
	unsigned int rac[7];
	unsigned int aging_thd[10];
	unsigned int aging_multi[10];
	unsigned int aging_rdc;
	unsigned int uvlo;
	unsigned int ocp;
	unsigned int cur_rdc;
	unsigned int cur_rac;
	unsigned int imax;
	unsigned int ocv_noerr;
	unsigned int sys_power_noerr;
	unsigned int bat_power_noerr;
	unsigned int ocv;
	unsigned int sys_power;
	unsigned int bat_power;
	struct work_struct bat_work;
	struct power_supply *psy;
	struct device *dev;
	unsigned int hpt_exclude_lbat_cg_thl;
	int is_evb;
};

struct ocv_table_t {
	unsigned int mah;
	unsigned int dod;
	unsigned int voltage;
	unsigned int rdc;
};

struct fg_info_t {
	int temp;
	int qmax;
	int ocv_table_size;
	struct ocv_table_t ocv_table[100];
};

struct fg_cus_data {
	unsigned int fg_info_size;
	unsigned int bat_type;
	struct fg_info_t fg_info[11];
};

struct ppb_ipi_data {
	unsigned int cmd;
};

struct xpu_dbg_t {
	unsigned int cpub_len;
	unsigned int cpum_len;
	unsigned int gpu_len;
	unsigned int cpub_cnt;
	unsigned int cpum_cnt;
	unsigned int gpu_cnt;
	unsigned int cpub_th_t;
	unsigned int cpum_th_t;
	unsigned int gpu_th_t;
};

#define TOTAL_PWR_INTERVALS 11
#define SF_NUM 5
struct ppb3_dbg_t {
	unsigned int lcpu_pwr[TOTAL_PWR_INTERVALS];
	unsigned int mcpu_pwr[TOTAL_PWR_INTERVALS];
	unsigned int bcpu_pwr[TOTAL_PWR_INTERVALS];
	unsigned int gpu_pwr[TOTAL_PWR_INTERVALS];
	unsigned int npu_pwr[TOTAL_PWR_INTERVALS];
	unsigned int bat_pwr;
	unsigned int pre_uv;
	unsigned int cgn_sf[SF_NUM];
	unsigned int oc_count;
	unsigned int oc_duration;
	unsigned int oc_duration_us;
};

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
struct spbm_scmi_state_t {
	unsigned int enabled;
	unsigned int debug;
	unsigned int fake_bcpu_tgt_pwr;
	unsigned int fake_mcpu_tgt_pwr;
	unsigned int fake_lcpu_tgt_pwr;
	unsigned int fake_gpu;
	unsigned int fake_gpu_tgt_pwr;
	unsigned int fake_gpu_avg_pwr;
	unsigned int fake_npu;
	unsigned int fake_npu_tgt_pwr;
	unsigned int fake_npu_avg_pwr;
	unsigned int sf_bcpu;
	unsigned int sf_mcpu;
	unsigned int sf_lcpu;
	unsigned int sf_gpu;
	unsigned int sf_npu;
	unsigned int send_pwr_to_xpu;
};

enum {
	SPBM_SCMI_SET,
	SPBM_SCMI_GET,
};

enum {
	SPBM_SCMI_SOC_BAT_PWR,
	SPBM_SCMI_SPBM_ENABLE,
	SPBM_SCMI_DEBUG,
	SPBM_SCMI_FAKE_CPU_PWR_LIMIT,
	SPBM_SCMI_FAKE_GPU_PWR_LIMIT,
	SPBM_SCMI_FAKE_NPU_PWR_LIMIT,
	SPBM_SCMI_SCALING_FACTOR_CPU,
	SPBM_SCMI_SCALING_FACTOR_GPU,
	SPBM_SCMI_SCALING_FACTOR_NPU,
	SPBM_SCMI_ENABLE_SEND_PWR,
	NR_SPBM_SCMI,
};
#endif

struct plt_ipi_data_s {
	unsigned int cmd;
	union {
		struct {
			unsigned int phys;
			unsigned int size;
		} ctrl;
		struct {
			unsigned int enable;
		} logger;
		struct {
			unsigned int BMCPU;
			unsigned int LDCPU;
		} hwpt;
	} u;
};

extern void kicker_ppb_request_power(enum ppb_kicker kicker, unsigned int power);
extern int ppb_set_wifi_pwr_addr(unsigned int val);
extern u32 get_mcupms_ipidev_number(void);
extern void *get_mcupm_ipidev(void);
#define GET_MCUPM_IPIDEV(t) \
(t < get_mcupms_ipidev_number() ? (&(((struct mtk_ipi_device *)get_mcupm_ipidev())[t])) : NULL)
#define PLT_HWPT_IPI_DATA 0x504C5404

/*save CPU power limiter times*/
#define SPBM_LCPU_PWR_LIMIT_TIMES_OFFSET         (0x00)
#define SPBM_MCPU_PWR_LIMIT_TIMES_OFFSET         (0x3C)
#define SPBM_BCPU_PWR_LIMIT_TIMES_OFFSET         (0x78)
#define SPBM_GPU_PWR_LIMIT_TIMES_OFFSET          (0xB4)
#define SPBM_NPU_PWR_LIMIT_TIMES_OFFSET          (0xF0)
/*scaling factor for CPU/GPU/NPU tuning*/
#define SPBM_KERNEL_B_SF_OFFSET     0x60
#define SPBM_KERNEL_M_SF_OFFSET     0x64
#define SPBM_KERNEL_L_SF_OFFSET     0x68
#define SPBM_KERNEL_G_SF_OFFSET     0x6C
#define SPBM_KERNEL_N_SF_OFFSET     0x70

/*for CPU to save CPU power limiter freq, TCM*/
#define SPBM_CPU_BCORE_TGT_PWR_TCM_OFFSET   (0x00)
#define SPBM_CPU_MCORE_TGT_PWR_TCM_OFFSET   (0x04)
#define SPBM_CPU_LCORE_TGT_PWR_TCM_OFFSET   (0x08)

#define SPBM_CPU_BCORE_AVG_PWR_TCM_OFFSET   (0x0C)
#define SPBM_CPU_MCORE_AVG_PWR_TCM_OFFSET   (0x10)
#define SPBM_CPU_LCORE_AVG_PWR_TCM_OFFSET   (0x14)

#define SPBM_CPU_BCORE_DB_I_TCM_OFFSET      (0x18)
#define SPBM_CPU_MCORE_DB_I_TCM_OFFSET      (0x1C)
#define SPBM_CPU_LCORE_DB_I_TCM_OFFSET      (0x20)

#define SPBM_CPU_BCORE_DB_C_TCM_OFFSET      (0x24)
#define SPBM_CPU_MCORE_DB_C_TCM_OFFSET      (0x28)
#define SPBM_CPU_LCORE_DB_C_TCM_OFFSET      (0x2C)

#define SPBM_CPU_BCORE_DB_TSO_TCM_OFFSET    (0x30)
#define SPBM_CPU_MCORE_DB_TSO_TCM_OFFSET    (0x34)
#define SPBM_CPU_LCORE_DB_TSO_TCM_OFFSET    (0x38)

/*sysram for GPU to save GPU power limiter freq, sysram*/
#define SPBM_GPU_FREQ_OFFSET                (0x00)
#define SPBM_GPU_AVG_CURRENT_OFFSET         (0x04)
#define SPBM_GPU_TARGET_PWR_OFFSET          (0x08)
#define SPBM_GPU_THROTTLED_OFFSET           (0x0C)

/*sysram for NPU to save NPU power limiter freq, sysram*/
#define SPBM_NPU_FREQ_OFFSET                (0x28)
#define SPBM_NPU_AVG_CURRENT_OFFSET         (0x2C)
#define SPBM_NPU_TARGET_PWR_OFFSET          (0x30)
#define SPBM_NPU_THROTTLED_OFFSET           (0x34)


/*SPMB current reporting log*/
#define SPBM_BCPU_V_OFFSET              (0x80)
#define SPBM_BCPU_I_OFFSET              (0x84)
#define SPBM_BCPU_W_OFFSET              (0x88)

#define SPBM_MCPU_V_OFFSET              (0x8C)
#define SPBM_MCPU_I_OFFSET              (0x90)
#define SPBM_MCPU_W_OFFSET              (0x94)

#define SPBM_LCPU_V_OFFSET              (0x98)
#define SPBM_LCPU_I_OFFSET              (0x9C)
#define SPBM_LCPU_W_OFFSET              (0xA0)

#define SPBM_GPU_V_OFFSET               (0xA4)
#define SPBM_GPU_I_OFFSET               (0xA8)
#define SPBM_GPU_W_OFFSET               (0xAC)

#define SPBM_NPU_V_OFFSET               (0xB0)
#define SPBM_NPU_I_OFFSET               (0xB4)
#define SPBM_NPU_W_OFFSET               (0xB8)


/*for SPMB to save power limit(send to xpu) log*/
#define SPBM_LCPU_PWR_LIMIT_OFFSET      (0xBC)
#define SPBM_MCPU_PWR_LIMIT_OFFSET      (0xC0)
#define SPBM_BCPU_PWR_LIMIT_OFFSET      (0xC4)
#define SPBM_GPU_PWR_LIMIT_OFFSET       (0xC8)
#define SPBM_NPU_PWR_LIMIT_OFFSET       (0xCC)

/*for SPMB to save request power log*/
#define SPBM_LCPU_REQUEST_PWR_OFFSET    (0xD0)
#define SPBM_MCPU_REQUEST_PWR_OFFSET    (0xD4)
#define SPBM_BCPU_REQUEST_PWR_OFFSET    (0xD8)
#define SPBM_GPU_REQUEST_PWR_OFFSET     (0xDC)
#define SPBM_NPU_REQUEST_PWR_OFFSET     (0xE0)

/*for SPMB to save Record the number of times UVLO is triggered*/
#define SPBM_BAT_PWR_OFFSET             (0xF0)
#define SPBM_UVLO_TRIGGER_TIMES_OFFSET  (0xF4)
#define SPBM_OC_COUNT_OFFSET            (0xF8)
#define SPBM_OC_DURATION_OFFSET         (0xFC)

#define APU_LIMIT_FREQ_OFFSET  (0xDC)
#endif /* __MTK_PEAK_POWER_BUDGETING_H__ */
