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

#define PARAMETER_LEN_MAX               (128)

#define DFX_ID_CHG_PD_AUTH_FAIL         909001004
#define DFX_ID_CHG_CP_ENABLE_ERR        909001005
#define DFX_ID_CHG_NONE_STANDARD_CHG    909002001
#define DFX_ID_CHG_CORROSION_DISCHARGE  909002002
#define DFX_ID_CHG_LPD_DISCHARGE        909002003
#define DFX_ID_CHG_CP_VBUS_OVP          909002004
#define DFX_ID_CHG_CP_IBUS_OCP          909002005
#define DFX_ID_CHG_CP_VBAT_OVP          909002006
#define DFX_ID_CHG_CP_IBAT_OCP          909002007
#define DFX_ID_CHG_BATT_CYCLE_COUNT     909003001
#define DFX_ID_CHG_UISOC_NOT_FULL       909003002
#define DFX_ID_CHG_FG_IIC_ERR           909005001
#define DFX_ID_CHG_CP_I2C_ERR           909005002
#define DFX_ID_CHG_BATT_LINKER_ABSENT   909005003
#define DFX_ID_CHG_CP_TDIE_HOT          909005004
#define DFX_ID_CHG_CP_VBUS_LOW          909005006
#define DFX_ID_CHG_NOT_CHG_IN_LOW_TEMP  909005007
#define DFX_ID_CHG_NOT_CHG_IN_HIGH_TEMP 909005008
#define DFX_ID_CHG_BATT_AUTH_FAIL       909007001
#define DFX_ID_CHG_TBAT_HOT             909009001
#define DFX_ID_CHG_TBAT_COLD            909009002

enum xm_chg_dfx_type {
	CHG_DFX_DEFAULT_TYPE,
	CHG_DFX_PD_AUTH_FAIL,
	CHG_DFX_CP_ENABLE_ERR,
	CHG_DFX_NONE_STANDARD_CHG,
	CHG_DFX_CORROSION_DISCHARGE,
	CHG_DFX_CP_VBUS_OVP,
	CHG_DFX_CP_IBUS_OCP,
	CHG_DFX_CP_VBAT_OVP,
	CHG_DFX_CP_IBAT_OCP,
	CHG_DFX_CHG_BATT_CYCLE,
	CHG_DFX_SOC_NOT_FULL,
	CHG_DFX_FG_IIC_ERR,
	CHG_DFX_CP_I2C_ERR,
	CHG_DFX_BATT_LINKER_ABSENT,
	CHG_DFX_CP_TDIE_HOT,
    	CHG_DFX_CP_VBUS_LOW,
	CHG_DFX_NOT_CHG_IN_LOW_TEMP,
	CHG_DFX_NOT_CHG_IN_HIGH_TEMP,
	CHG_DFX_BATT_AUTH_FAIL,
        CHG_DFX_TBAT_HOT,
	CHG_DFX_TBAT_COLD,
	CHG_DFX_MAX_TYPE,
};

struct xm_chg_dfx_evt_cond {
	// int evt_code;
	// char *evt_name;
	// int evt_type;
	int dfx_type;
	bool is_first_report;
	bool boot_report;
	bool charging_report;
	int report_times;
	int max_report_times;
};

struct xm_chg_dfs {
	struct task_struct *report_thread;
	wait_queue_head_t report_wq;
	//struct xm_chg_dfx_evt_cond *evt_cond;
	atomic_t run_report;
	int dfx_type;
	unsigned long evt_report_bits[BITS_TO_LONGS(CHG_DFX_MAX_TYPE)];

	struct notifier_block psy_nb;
	struct notifier_block charger_nb;
	struct notifier_block fg_nb;
	struct notifier_block cm_nb;
	struct notifier_block cp_nb;

    	struct adapter_device *pd_adapter;
        struct charger_dev *charger;
	struct delayed_work evt_cond_check_work;
	struct mutex cond_check_lock;

        int check_interval_ms;
	//int board_version;
	int tdie;
        int cp_enable_err_flag;
	int ibat_ocp_flag;  
	int vbat_ovp_flag;  
	int ibus_ocp_flag;  
	int vbus_ovp_flag;  
        int cp_absent_flag;
        int fg_absent_flag;
        int batt_linker_absent_flag;
        int vbus;
	int tbat;
	int tbat_max;
	int tbat_min;
	int batt_auth;
  	int chip_chg_status;
	int cycle_cnt;
	int last_cycle_count;
	int vbat;
	int soc;
	int rsoc;
	int vbus_type;
	//int tboard;
    	int adapter_svid;
  	int pd_auth_fail;
  	int pd_verify_done;
	int adapter_plug_in;
};

#endif /* __XM_CHG_DFS_H__ */
