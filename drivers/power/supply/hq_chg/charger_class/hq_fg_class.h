// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2023 Huaqin Technology(Shanghai) Co., Ltd.
 */

#ifndef __LINUX_HUAQIN_FG_CLASS_H__
#define __LINUX_HUAQIN_FG_CLASS_H__

struct fuel_gauge_dev;
struct fuel_gauge_ops {
	int (*get_soc_decimal)(struct fuel_gauge_dev *);
	int (*get_soc_decimal_rate)(struct fuel_gauge_dev *);
	int (*get_rsoc)(struct fuel_gauge_dev *);
	int (*set_rsoc_update0)(struct fuel_gauge_dev *fuel_gauge, bool value);
#if IS_ENABLED(CONFIG_HUAQIN_SOH2_SUPPORT)
	int (*get_soh)(struct fuel_gauge_dev *);
	int (*set_soh)(struct fuel_gauge_dev *fuel_gauge, int value);
	int (*set_fake_soh)(struct fuel_gauge_dev *fuel_gauge, int value);
#endif
	int (*get_c_car)(struct fuel_gauge_dev *);
	int (*get_v_car)(struct fuel_gauge_dev *);
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	int (*check_i2c_function)(struct fuel_gauge_dev *);
#endif
	int (*set_fastcharge_mode)(struct fuel_gauge_dev *, bool);
	int (*get_fastcharge_mode)(struct fuel_gauge_dev *);
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
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
int fuel_gauge_check_i2c_function(struct fuel_gauge_dev *fuel_gauge);
#endif
int fuel_gauge_set_fastcharge_mode(struct fuel_gauge_dev *fuel_gauge, bool);
int fuel_gauge_get_fastcharge_mode(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_rsoc(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_set_rsoc_update0(struct fuel_gauge_dev *fuel_gauge, bool value);
#if IS_ENABLED(CONFIG_HUAQIN_SOH2_SUPPORT)
int fuel_gauge_get_soh(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_set_soh(struct fuel_gauge_dev *fuel_gauge, int value);
int fuel_gauge_set_fake_soh(struct fuel_gauge_dev *fuel_gauge, int value);
#endif
int fuel_gauge_get_c_car(struct fuel_gauge_dev *fuel_gauge);
int fuel_gauge_get_v_car(struct fuel_gauge_dev *fuel_gauge);
#endif
