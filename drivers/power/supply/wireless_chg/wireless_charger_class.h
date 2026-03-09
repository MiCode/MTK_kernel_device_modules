/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 XiaoMi Inc.
 */
#ifndef LINUX_WIRELESS_CHARGER_CLASS_H
#define LINUX_WIRELESS_CHARGER_CLASS_H
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/mutex.h>

enum wireless_project_vendor {
	WLS_CHIP_VENDOR_FUDA1652,
	WLS_CHIP_VENDOR_SC96281,
	WLS_CHIP_VENDOR_SC96231,
	WLS_CHIP_VENDOR_MAX,
};

struct wireless_charger_properties {
	const char *alias_name;
};
struct wireless_charger_device {
	struct wireless_charger_properties props;
	const struct wireless_charger_ops *ops;
	struct mutex ops_lock;
	struct device dev;
	struct srcu_notifier_head evt_nh;
	void	*driver_data;
	bool is_polling_mode;
};
struct wireless_charger_ops {
	int (*wls_enable_reverse_chg)(struct wireless_charger_device *dev, bool en);
	int (*wls_is_wireless_present)(struct wireless_charger_device *dev, bool *present);
	int (*wls_is_i2c_ok)(struct wireless_charger_device *dev, bool *i2c_ok);
	int (*wls_is_qc_enable)(struct wireless_charger_device *dev, bool *enable);
	int (*wls_is_firmware_update)(struct wireless_charger_device *dev, bool *update);
	int (*wls_set_vout)(struct wireless_charger_device *dev, int vout);
	int (*wls_get_vout)(struct wireless_charger_device *dev, int *vout);
	int (*wls_get_iout)(struct wireless_charger_device *dev, int *iout);
	int (*wls_get_vrect)(struct wireless_charger_device *dev, int *vrect);
	int (*wls_get_tx_adapter)(struct wireless_charger_device *dev, int *adapter);
	int (*wls_get_tx_uuid)(struct wireless_charger_device *dev, char *buf);
	int (*wls_get_reverse_chg_state)(struct wireless_charger_device *dev, int *state);
	int (*wls_is_enable)(struct wireless_charger_device *dev, bool *en);
	int (*wls_enable_chg)(struct wireless_charger_device *dev, bool en);
	int (*wls_is_car_adapter)(struct wireless_charger_device *dev, bool *enable);
	int (*wls_get_reverse_chg)(struct wireless_charger_device *dev, bool *enable);
	int (*wls_set_rx_sleep_mode)(struct wireless_charger_device *dev, int sleep_for_dam);
	int (*wls_set_quiet_sts)(struct wireless_charger_device *dev, int quiet_sts);
	int (*wls_set_parallel_charge)(struct wireless_charger_device *dev, bool parachg);
	int (*wls_is_vout_range_set_done)(struct wireless_charger_device *dev, bool *is_done);
	int (*wls_get_adapter_chg_mode)(struct wireless_charger_device *dev, int *cp_chg_mode);
	/*For LN8282*/
	int (*wls_ln_set_mode)(struct wireless_charger_device *dev, int value);
	int (*wls_ln_get_mode)(struct wireless_charger_device *dev, int *value);
	int (*wls_notify_cp_status)(struct wireless_charger_device *dev, int state);
	int (*wls_get_chip_version)(struct wireless_charger_device *dev, char *buf);
	int (*wls_firmware_update)(struct wireless_charger_device *dev, int cmd);
	int (*wls_check_fw_version)(struct wireless_charger_device *dev);
	/* for pen tx*/
	int (*wls_get_pen_soc)(struct wireless_charger_device *dev, int *soc);
};
static inline void *wireless_charger_dev_get_drvdata(
	const struct wireless_charger_device *wireless_charger_dev)
{
	return wireless_charger_dev->driver_data;
}
static inline void wireless_charger_dev_set_drvdata(
	struct wireless_charger_device *wireless_charger_dev, void *data)
{
	wireless_charger_dev->driver_data = data;
}
extern struct wireless_charger_device *wireless_charger_device_register(
	const char *name,
	struct device *parent, void *devdata, const struct wireless_charger_ops *ops,
	const struct wireless_charger_properties *props);
extern void wireless_charger_device_unregister(
	struct wireless_charger_device *wireless_charger_dev);
extern struct wireless_charger_device *get_wireless_charger_by_name(
	const char *name);
#define to_wireless_charger_device(obj) container_of(obj, struct wireless_charger_device, dev)
static inline void *wireless_charger_get_data(
	struct wireless_charger_device *wireless_charger_dev)
{
	return dev_get_drvdata(&wireless_charger_dev->dev);
}
extern int register_wireless_charger_device_notifier(
	struct wireless_charger_device *wireless_charger_dev,
			      struct notifier_block *nb);
extern int unregister_wireless_charger_device_notifier(
	struct wireless_charger_device *wireless_charger_dev,
				struct notifier_block *nb);
extern int wireless_charger_dev_notify(
	struct wireless_charger_device *wireless_charger_dev, int event);
/*For wireless charge*/
extern int wireless_charger_dev_enable_wls_reverse_chg(struct wireless_charger_device *chg_dev, bool en);
extern int wireless_charger_dev_is_wireless_present(struct wireless_charger_device *chg_dev, bool *present);
extern int wireless_charger_dev_is_i2c_ok(struct wireless_charger_device *chg_dev, bool *i2c_ok);
extern int wireless_charger_dev_is_qc_enable(struct wireless_charger_device *chg_dev, bool *enable);
extern int wireless_charger_dev_is_firmware_update(struct wireless_charger_device *chg_dev, bool *update);
extern int wireless_charger_dev_wls_set_vout(struct wireless_charger_device *chg_dev, int vout);
extern int wireless_charger_dev_wls_get_vout(struct wireless_charger_device *chg_dev, int *vout);
extern int wireless_charger_dev_wls_get_iout(struct wireless_charger_device *chg_dev, int *iout);
extern int wireless_charger_dev_wls_get_vrect(struct wireless_charger_device *chg_dev, int *vrect);
extern int wireless_charger_dev_wls_get_tx_adapter(struct wireless_charger_device *chg_dev, int *adapter);
extern int wireless_charger_dev_wls_get_tx_uuid(struct wireless_charger_device *chg_dev, char *buf);
extern int wireless_charger_dev_wls_get_reverse_chg_state(struct wireless_charger_device *chg_dev, int *state);
extern int wireless_charger_dev_is_enable(struct wireless_charger_device *chg_dev, bool *en);
extern int wireless_charger_dev_enable_wls_chg(struct wireless_charger_device *chg_dev, bool en);
extern int wireless_charger_dev_is_car_adapter(struct wireless_charger_device *chg_dev, bool *enable);
extern int wireless_charger_dev_get_reverse_chg(struct wireless_charger_device *chg_dev, bool *enable);
extern int wireless_charger_dev_wls_set_rx_sleep_mode(struct wireless_charger_device *chg_dev, int sleep_for_dam);
extern int wireless_charger_dev_wls_set_quiet_sts(struct wireless_charger_device *chg_dev, int quiet_sts);
extern int wireless_charger_dev_wls_set_parallel_charge(struct wireless_charger_device *chg_dev, bool parachg);
extern int wireless_charger_dev_is_vout_range_set_done(struct wireless_charger_device *chg_dev, bool *is_done);
extern int wireless_charger_dev_wls_get_adapter_chg_mode(struct wireless_charger_device *chg_dev, int *cp_chg_mode);
/* For LN8282 */
extern int wireless_charger_dev_wls_ln_set_mode(struct wireless_charger_device *chg_dev, int value);
extern int wireless_charger_dev_wls_ln_get_mode(struct wireless_charger_device *chg_dev, int *value);
extern int wireless_charger_dev_wls_notify_cp_status(struct wireless_charger_device *chg_dev, int status);
extern int wireless_charger_dev_wls_get_chip_version(struct wireless_charger_device *chg_dev, char *buf);
extern int wireless_charger_dev_wls_firmware_update(struct wireless_charger_device *chg_dev, int cmd);
extern int wireless_charger_dev_wls_check_fw_version(struct wireless_charger_device *chg_dev);
/* for pen tx */
extern int wireless_charger_dev_wls_get_pen_soc(struct wireless_charger_device *chg_dev, int *soc);
#endif /*LINUX_WIRELESS_CHARGER_CLASS_H*/
