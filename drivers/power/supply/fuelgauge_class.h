/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef LINUX_POWER_FUELGAUGE_CLASS_H
#define LINUX_POWER_FUELGAUGE_CLASS_H

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/mutex.h>

struct fg_properties {
	const char *alias_name;
};

struct fg_device {
	struct fg_properties props;
	const struct fg_ops *ops;
	struct mutex ops_lock;
	struct device dev;
	void	*driver_data;
	struct srcu_notifier_head evt_nh;
};

struct fg_ops {
	int (*read_cyclecount)(struct fg_device *dev, int *cyclecount);
	int (*read_rsoc)(struct fg_device *dev, int *rsoc);
	int (*read_soh)(struct fg_device *dev, int *soh);
	int (*get_raw_soc)(struct fg_device *dev, int *raw_soc);
	int (*read_current)(struct fg_device *dev, int *ibat);
	int (*read_temperature)(struct fg_device *dev, int *temp);
	int (*read_volt)(struct fg_device *dev, int *vbat);
	int (*read_status)(struct fg_device *dev, bool *batt_fc);
	int (*update_battery_shutdown_vol)(struct fg_device *dev, int *shutdown_vbat);
	int (*update_monitor_delay)(struct fg_device *dev, int *monitor_delay);
	int (*update_eea_chg_support)(struct fg_device *dev, bool *eea_support);
	int (*get_charging_done_status)(struct fg_device *dev, bool *charging_done);
	int (*get_en_smooth_full_status)(struct fg_device *dev, bool *en_smooth_full);
	int (*get_rm)(struct fg_device *dev, int *rm);
	int (*read_avg_current)(struct fg_device *dev, int *avg_current);
	int (*read_i2c_error_count)(struct fg_device *dev, int *count);
	int (*read_fcc)(struct fg_device *dev, int *fcc);
	int (*read_design_capacity)(struct fg_device *dev, int *dc);
	int (*get_fg_chip_ok)(struct fg_device *dev, int *chip_ok);
	int (*set_fg_in_sleep)(struct fg_device *dev, int sleep);
	int (*fg_get_ui_soh)(struct fg_device *dev, char *buf);
	int (*fg_set_ui_soh)(struct fg_device *dev, char *buf);
	int (*fg_get_soh_sn)(struct fg_device *dev, char *buf);
};

static inline void *fg_dev_get_drvdata(
	const struct fg_device *fg_dev)
{
	return fg_dev->driver_data;
}

static inline void fg_dev_set_drvdata(
	struct fg_device *fg_dev, void *data)
{
	fg_dev->driver_data = data;
}

extern int register_fuelgauge_device_notifier(struct fg_device *fg_dev,
				struct notifier_block *nb);
extern int unregister_fuelgauge_device_notifier(struct fg_device *fg_dev,
				struct notifier_block *nb);

extern struct fg_device *fg_device_register(
	const char *name,
	struct device *parent, void *devdata, const struct fg_ops *ops,
	const struct fg_properties *props);
extern void fg_device_unregister(struct fg_device *fg_dev);
extern struct fg_device *get_fuelgauge_by_name(const char *name);
extern int fuelgauge_dev_read_cyclecount(struct fg_device *fg_dev, int *cyclecount);
extern int fuelgauge_dev_read_rsoc(struct fg_device *fg_dev, int *rsoc);
extern int fuelgauge_dev_read_soh(struct fg_device *fg_dev, int *soh);
extern int fuelgauge_dev_get_raw_soc(struct fg_device *fg_dev, int *raw_soc);
extern int fuelgauge_dev_read_current(struct fg_device *fg_dev, int *ibat);
extern int fuelgauge_dev_read_temperature(struct fg_device *fg_dev, int *temp);
extern int fuelgauge_dev_read_volt(struct fg_device *fg_dev, int *vbat);
extern int fuelgauge_dev_read_status(struct fg_device *fg_dev, bool *batt_fc);
extern int fuelgauge_dev_update_battery_shutdown_vol(struct fg_device *fg_dev, int *shutdown_vbat);
extern int fuelgauge_dev_update_monitor_delay(struct fg_device *fg_dev, int *monitor_delay);
extern int fuelgauge_dev_update_eea_chg_support(struct fg_device *fg_dev, bool *eea_support);
extern int fuelgauge_dev_get_charging_done_status(struct fg_device *fg_dev, bool *charging_done);
extern int fuelgauge_dev_get_en_smooth_full_status(struct fg_device *fg_dev, bool *en_smooth_full);
extern int fuelgauge_dev_get_rm(struct fg_device *fg_dev, int *rm);
extern int fuelgauge_dev_read_avg_current(struct fg_device *fg_dev, int *avg_current);
extern int fuelgauge_dev_read_i2c_error_count(struct fg_device *fg_dev, int *count);
extern int fuelgauge_dev_read_fcc(struct fg_device *fg_dev, int *fcc);
extern int fuelgauge_dev_read_design_capacity(struct fg_device *fg_dev, int *dc);
extern int fuelgauge_dev_get_fg_chip_ok(struct fg_device *fg_dev, int *chip_ok);
extern int fuelgauge_dev_set_fg_in_sleep(struct fg_device *fg_dev, int sleep);
extern int fuelgauge_dev_fg_get_ui_soh(struct fg_device *fg_dev, char *buf);
extern int fuelgauge_dev_fg_set_ui_soh(struct fg_device *fg_dev, char *buf);
extern int fuelgauge_dev_fg_get_soh_sn(struct fg_device *fg_dev, char *buf);

#define to_fg_device(obj) container_of(obj, struct fg_device, dev)

static inline void *fuelgauge_get_data(
	struct fg_device *fg_dev)
{
	return dev_get_drvdata(&fg_dev->dev);
}

extern int fg_dev_enable(struct fg_device *fg_dev, bool en);

#endif /*LINUX_POWER_fg_class_H*/
