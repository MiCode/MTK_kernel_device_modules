/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2025 Huaqin Technology(Shanghai) Co., Ltd.
 */

#ifndef __XM_CHG_DFS_H__
#define __XM_CHG_DFS_H__

#include <linux/atomic.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/bitmap.h>
#include <linux/bitops.h>

#include "mievent.h"
#include "hq_fg_class.h"
#include "charger_class.h"
#include "adapter_class.h"

/* Charge DFS configurations */
#define PARAMETER_LEN_MAX                       (128)

#define DFS_CHECK_5S_INTERVAL                   (5000)  // 5s
#define DFS_CHECK_10S_INTERVAL                  (10000) // 10s

/********************************************************/
/*               Charge DFS Events Define               */
/********************************************************/
/* Charge Speed Low */
#define DFX_ID_CHG_PD_AUTH_FAIL                 909001004
#define DFX_ID_CHG_CP_EN_FAIL                   909001005

/* Charge Abnormal Stop */
#define DFX_ID_CHG_NONE_STANDARD_CHG            909002001
#define DFX_ID_CHG_CORROSION_DISCHARGE          909002002
#define DFX_ID_CHG_LPD_DETECTED                 909002003
#define DFX_ID_CHG_CP_VBUS_OVP                  909002004
#define DFX_ID_CHG_CP_IBUS_OCP                  909002005
#define DFX_ID_CHG_CP_VBAT_OVP                  909002006
#define DFX_ID_CHG_CP_IBAT_OCP                  909002007
#define DFX_ID_CHG_CP_VAC_OVP                   909002008
#define DFX_ID_CHG_ANTI_BURN_TRIG               909002012

/* Cann't Report 100% SOC */
#define DFX_ID_CHG_CHG_BATT_CYCLE               909003001
#define DFX_ID_CHG_SOC_NOT_FULL                 909003002
#define DFX_ID_CHG_SMART_ENDURA_TRIG            909003004
#define DFX_ID_CHG_SMART_NAVI_TRIG              909003006

/* Cann't Charing */
#define DFX_ID_CHG_FG_I2C_ERR                   909005001
#define DFX_ID_CHG_CP_I2C_ERR                   909005002
#define DFX_ID_CHG_BATT_LINKER_ABSENT           909005003
#define DFX_ID_CHG_CP_TDIE_HOT                  909005004
#define DFX_ID_CHG_VBUS_UVLO                    909005006
#define DFX_ID_CHG_NOT_CHG_IN_LOW_TEMP          909005007
#define DFX_ID_CHG_NOT_CHG_IN_HIGH_TEMP         909005008
#define DFX_ID_CHG_DUAL_BATT_LINKER_ABSENT      909005009

/* SOC Not Accurately */
#define DFX_ID_CHG_VBAT_SOC_NOT_MATCH           909006001
#define DFX_ID_CHG_SMART_ENDURA_SOC_ERR         909006010
#define DFX_ID_CHG_SMART_NAVI_SOC_ERR           909006011

/* IC Connumicate Failed */
#define DFX_ID_CHG_BATT_AUTH_FAIL               909007001
#define DFX_ID_CHG_CHG_BATT_AUTH_FAIL           909007002
// #define DFX_ID_CHG_CURR_I2C_ERR                 909007003 /* TODO: Should Double Check */

/* Temperature Abnormal */
#define DFX_ID_CHG_TBAT_HOT                     909009001
#define DFX_ID_CHG_TBAT_COLD                    909009002
#define DFX_ID_CHG_ANTI_FAIL                    909009003

/* Wireless Charge Spedd Low */
#define DFX_ID_CHG_WLS_FAST_CHG_FAIL            909011001
#define DFX_ID_CHG_WLS_Q_LOW                    909011002

/* Wireless Charge Abnormal Stop */
#define DFX_ID_CHG_WLS_RX_OTP                   909012001
#define DFX_ID_CHG_WLS_RX_OVP                   909012002
#define DFX_ID_CHG_WLS_RX_OCP                   909012003
#define DFX_ID_CHG_WLS_TRX_FOD                  909012004
#define DFX_ID_CHG_WLS_TRX_OCP                  909012005
#define DFX_ID_CHG_WLS_TRX_UVLO                 909012006

/* Wireless Charge Invalid */
#define DFX_ID_CHG_WLS_TRX_I2C_ERR              909013001
#define DFX_ID_CHG_WLS_RX_I2C_ERR               909013004

/* Battery Security */
#define DFX_ID_CHG_DUAL_VBAT_DIFF               909014002

/* Event specific parameters */
#define DFX_BAT_HOT_TEMP       (550)
#define DFX_BAT_COLD_TEMP      (-100)

#define REPORT_PERIOD_300S     (300)
#define REPORT_PERIOD_600S     (600)

enum xm_chg_dfx_type {
	CHG_DFX_PD_AUTH_FAIL,
	CHG_DFX_CP_EN_FAIL,
	CHG_DFX_NONE_STANDARD_CHG,
	CHG_DFX_CORROSION_DISCHARGE,
	CHG_DFX_LPD_DETECTED,
	CHG_DFX_CP_VBUS_OVP,
	CHG_DFX_CP_IBUS_OCP,
	CHG_DFX_CP_VBAT_OVP,
	CHG_DFX_CP_IBAT_OCP,
	CHG_DFX_CP_VAC_OVP,
	CHG_DFX_ANTI_BURN_TRIG,
	CHG_DFX_CHG_BATT_CYCLE,
	CHG_DFX_SOC_NOT_FULL,
	CHG_DFX_SMART_ENDURA_TRIG,
	CHG_DFX_SMART_NAVI_TRIG,
	CHG_DFX_FG_I2C_ERR,
	CHG_DFX_CP_I2C_ERR,
	CHG_DFX_BATT_LINKER_ABSENT,
	CHG_DFX_CP_TDIE_HOT,
	CHG_DFX_VBUS_UVLO,
	CHG_DFX_NOT_CHG_IN_LOW_TEMP,
	CHG_DFX_NOT_CHG_IN_HIGH_TEMP,
	// CHG_DFX_DUAL_BATT_LINKER_ABSENT,
	CHG_DFX_VBAT_SOC_NOT_MATCH,
	CHG_DFX_SMART_ENDURA_SOC_ERR,
	CHG_DFX_SMART_NAVI_SOC_ERR,
	CHG_DFX_BATT_AUTH_FAIL,
	// CHG_DFX_CHG_BATT_AUTH_FAIL,
	CHG_DFX_TBAT_HOT,
	CHG_DFX_TBAT_COLD,
	CHG_DFX_ANTI_FAIL,
	// CHG_DFX_WLS_FACT_CHG_FAIL,
	// CHG_DFX_WLS_Q_LOW,
	// CHG_DFX_RX_OTP,
	// CHG_DFX_RX_OVP,
	// CHG_DFX_RX_OCP,
	// CHG_DFX_TRX_FOD,
	// CHG_DFX_TRX_OCP,
	// CHG_DFX_TRX_UVLO,
	// CHG_DFX_TRX_I2C_ERR,
	// CHG_DFX_RX_I2C_ERR,
	// CHG_DFX_DUAL_VBAT_DIFF,

	/* NOTE: add new type here */
	CHG_DFX_MAX_TYPE,
};

struct xm_chg_dfx_evt {
	int dfx_type;
	int dfx_id;
	uint8_t para_cnt;
	//unsigned long report_mode[BITS_TO_LONGS(8)];

	int report_cnt;
	int report_cnt_limit;

	time64_t report_time;
	int report_period_limit;
};

struct xm_chg_dfs {
	struct task_struct *report_thread;
	wait_queue_head_t report_wq;
	atomic_t run_report;
	unsigned long evt_report_bits[BITS_TO_LONGS(CHG_DFX_MAX_TYPE)];

	struct notifier_block psy_nb;
	struct notifier_block cp_master_nb;
	struct notifier_block fg_nb;
	struct notifier_block cm_nb;

	struct charger_device *cp_master;
	struct adapter_device *pd_adapter;
	struct fuel_gauge_dev *fuel_gauge;
	struct delayed_work evt_report_check_work;
	struct mutex report_check_lock;

	int check_interval;

	int tbat;
	int tbat_max;
	int tbat_min;
	int batt_auth_fail;
	int chg_status;
	int cycle_cnt;
	int last_cycle_count;
	int vbat;
	int vbat_max;
	int soc;
	int rsoc;
	int real_type;
	int tboard;
	int adapter_plug_in;
	int master_ok;
	// int slave_ok;
	int adapter_svid;
	int cp_ibat_ocp;
	int cp_ibus_ocp;
	int cp_vbus_ovp;
	int cp_vbat_ovp;
	int cp_vac_ovp;
	int pd_auth_fail;
	int batt_absent;
	int fg_i2c_err;
	int pd_verify_done;
	int lpd_flag;
	int tconn;
	int typec_burn;
	int anti_burn_fail;
	int master_tdie;
	// int slave_tdie;
	int aicl_threshold;
	int vbus;
	int cp_en_fail;
	int cp_tdie_flt;
	int smart_endura_trig;
	int smart_navi_trig;

};

#endif /* __XM_CHG_DFS_H__ */