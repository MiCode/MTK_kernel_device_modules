// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2023 Huaqin Technology(Shanghai) Co., Ltd.
 */

#ifndef __LINUX_HUAQIN_FG_CLASS_H__
#define __LINUX_HUAQIN_FG_CLASS_H__

enum err_stat {
	FG_ERR_AUTH_FAIL = 0x01,
	FG_EER_I2C_FAIL,
	FG_ERR_ISC_ALARM = 0x0C,
	FG_ERR_CHG_WATT = 0x10,
	FG_BATT_AUTH_DONE = 0x20,
	FG_ERR_MASK = 0x1F,
};

struct fuel_gauge_dev;
struct fuel_gauge_ops {
	int (*get_soc_decimal)(struct fuel_gauge_dev *);
	int (*get_soc_decimal_rate)(struct fuel_gauge_dev *);
	int (*get_rsoc)(struct fuel_gauge_dev *);
	int (*set_rsoc_update0)(struct fuel_gauge_dev *fuel_gauge, bool value);
#if IS_ENABLED(CONFIG_HUAQIN_SOH2_SUPPORT)
	int (*get_soh)(struct fuel_gauge_dev *);
	int (*set_soh)(struct fuel_gauge_dev *fuel_gauge, int value);
#endif
	int (*get_c_car)(struct fuel_gauge_dev *);
	int (*get_v_car)(struct fuel_gauge_dev *);
	int (*set_fastcharge_mode)(struct fuel_gauge_dev *, bool);
	int (*get_fastcharge_mode)(struct fuel_gauge_dev *);
	int (*get_batt_id)(struct fuel_gauge_dev *);
	int (*get_batt_auth)(struct fuel_gauge_dev *);
	int (*get_batt_id_voltage)(struct fuel_gauge_dev *);
	int (*check_fg_status)(struct fuel_gauge_dev *);
	int (*report_fg_soc100)(struct fuel_gauge_dev *);
	int (*get_raw_soc)(struct fuel_gauge_dev *);
};

struct fuel_gauge_dev {
	struct device dev;
	char *name;
	void *private;
	struct fuel_gauge_ops *ops;

	bool changed;
	struct mutex changed_lock;
	struct work_struct changed_work;
};

struct fuel_gauge_dev *fuel_gauge_find_dev_by_name(const char *name);
struct fuel_gauge_dev *fuel_gauge_register(char *name, struct device *parent,
			struct fuel_gauge_ops *ops, void *private);

void *fuel_gauge_get_private(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_unregister(struct fuel_gauge_dev *fuel_gauge);

int fuel_gauge_get_soc_decimal(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_soc_decimal_rate(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_set_fastcharge_mode(struct fuel_gauge_dev *fuel_gauge, bool);
int fuel_gauge_get_fastcharge_mode(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_rsoc(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_set_rsoc_update0(struct fuel_gauge_dev *fuel_gauge, bool value);
int fuel_gauge_get_c_car(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_v_car(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_batt_id(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_batt_auth(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_batt_id_voltage(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_check_fg_status(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_report_fg_soc100(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_raw_soc(struct fuel_gauge_dev *fuel_gauge);
#endif
