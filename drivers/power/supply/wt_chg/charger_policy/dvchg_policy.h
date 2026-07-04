// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */
#ifndef __LINUX_SOUTHCHIP_CHARGER_PUMP_POLICY_H__
#define __LINUX_SOUTHCHIP_CHARGER_PUMP_POLICY_H__

#include "../charger_class/adapter_class.h"

#define ADAPTER_DEVICE_MAX   5

enum thermal_level {
	THERMAL_COLD,
	THERMAL_VERY_COOL,
	THERMAL_COOL,
	THERMAL_NORMAL,
	THERMAL_WARM,
	THERMAL_VERY_WARM,
	THERMAL_HOT,
	THERMAL_MAX,
};

enum state_machine {
	PM_STATE_CHECK_DEV = 0,
	PM_STATE_INIT,
	PM_STATE_MEASURE_RCABLE,
	PM_STATE_ENABLE_DVCHG,
	PM_STATE_DVCHG_CC_CV,
	PM_STATE_DVCHG_EXIT,
};

enum policy_state {
	POLICY_NO_START = 0,
	POLICY_NO_SUPPORT,
	POLICY_RUNNING,
	POLICY_STOP,
};

struct chg_info {
	int vbat;
	int ibat;
	int vbus;
	int ibus;
	uint8_t tdie;
	uint8_t tadapter;
	bool swchg_chging;
	bool dvchg_chging;
};

struct dvchg_policy_config {
	uint32_t cv;
	uint32_t cc;
	uint32_t exit_cc;
	uint32_t min_vbat;
	uint32_t step_volt;

	uint32_t max_request_volt;
	uint32_t min_request_volt;
	uint32_t max_request_curr;

	uint32_t max_vbat;
	uint32_t max_ibat;

	int tbat_level_def[THERMAL_MAX];
	int tbat_curlmt[THERMAL_MAX];
	int tbat_recovery;
};

struct dvchg_policy {
	struct device *dev;
	struct dvchg_policy_config cfg;
	wait_queue_head_t wait_queue;
	struct task_struct *thread;
	bool run_thread;

	enum state_machine sm;
	enum policy_state state;

	struct timer_list policy_timer;

	char *adapter_name[ADAPTER_DEVICE_MAX];
	struct adapter_dev *use_adapter;
	struct charger_dev *use_charger;
	struct dvchg_dev *use_dvchg;

	struct adapter_cap cap;
	uint8_t cap_nr;

	struct chg_info info;

	uint32_t next_time;
	bool recover;
	uint8_t recover_cnt;

	uint32_t request_volt;
	uint32_t request_curr;

	bool fast_request;

	struct mutex state_lock;
	struct mutex running_lock;
	struct mutex access_lock;

	enum thermal_level tdie_lv;
};

int dvchg_policy_start(struct dvchg_policy *policy);
int dvchg_policy_stop(struct dvchg_policy *policy);

#endif /* __LINUX_SOUTHCHIP_CHARGER_PUMP_POLICY_H__ */