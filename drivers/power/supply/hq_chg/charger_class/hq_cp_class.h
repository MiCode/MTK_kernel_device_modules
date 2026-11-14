/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2023 Huaqin Technology(Shanghai) Co., Ltd.
 */

#ifndef __LINUX_HUAQIN_CP_CLASS_H__
#define __LINUX_HUAQIN_CP_CLASS_H__

#define CHARGERPUMP_ERROR_VBUS_HIGH        BIT(0)
#define CHARGERPUMP_ERROR_VBUS_LOW         BIT(1)
#define CHARGERPUMP_ERROR_VBUS_OVP         BIT(2)
#define CHARGERPUMP_ERROR_IBUS_OCP         BIT(3)
#define CHARGERPUMP_ERROR_VBAT_OVP         BIT(4)
#define CHARGERPUMP_ERROR_IBAT_OCP         BIT(5)

#define CP_DEV_ID_NU2115 (0x90)

enum cp_adc_channel {
	CP_ADC_VBUS      = 0,
	CP_ADC_VSYS      = 1,
	CP_ADC_VBAT      = 2,
	CP_ADC_VAC       = 3,
	CP_ADC_IBUS      = 4,
	CP_ADC_IBAT      = 5,
	CP_ADC_TSBUS     = 6,
	CP_ADC_TSBAT     = 7,
	CP_ADC_TDIE      = 8,
	CP_ADC_MAX       = 9,
};

struct chargerpump_dev;
struct chargerpump_ops {
	int (*set_chip_init)(struct chargerpump_dev *chargerpump);
	int (*set_enable)(struct chargerpump_dev *chargerpump, bool enable);
	int (*set_vbus_ovp)(struct chargerpump_dev *chargerpump, int mv);
	int (*set_ibus_ocp)(struct chargerpump_dev *chargerpump, int ma);
	int (*set_vbat_ovp)(struct chargerpump_dev *chargerpump, int mv);
	int (*set_ibat_ocp)(struct chargerpump_dev *chargerpump, int ma);
	int (*set_enable_adc)(struct chargerpump_dev *chargerpump, bool enable);

	int (*get_is_enable)(struct chargerpump_dev *chargerpump, bool *enable);
	int (*get_status)(struct chargerpump_dev *chargerpump, uint32_t *status);
	int (*get_adc_value)(struct chargerpump_dev *chargerpump, enum cp_adc_channel ch, int *value);

	int (*get_chip_id)(struct chargerpump_dev *chargerpump, int *value);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	int (*set_cp_workmode)(struct chargerpump_dev *chargerpump, int workmode);
	int (*get_cp_workmode)(struct chargerpump_dev *chargerpump, int *workmode);
#endif
	int (*dump_cp_register)(struct chargerpump_dev *chargerpump);
	int (*reset_cp_register)(struct chargerpump_dev *chargerpump);
};

struct chargerpump_dev {
	struct device dev;
	char *name;
	void *private;
	struct chargerpump_ops *ops;

	bool changed;
	struct mutex changed_lock;
	struct work_struct changed_work;
};

struct chargerpump_dev *chargerpump_find_dev_by_name(const char *name);
struct chargerpump_dev *chargerpump_register(char *name, struct device *parent,
							struct chargerpump_ops *ops, void *private);
void *chargerpump_get_private(struct chargerpump_dev *charger);
int chargerpump_unregister(struct chargerpump_dev *charger);

int chargerpump_set_chip_init(struct chargerpump_dev *charger_pump);
int chargerpump_set_enable(struct chargerpump_dev *charger_pump, bool enable);
int chargerpump_set_vbus_ovp(struct chargerpump_dev *charger_pump, int mv);
int chargerpump_set_ibus_ocp(struct chargerpump_dev *charger_pump, int ma);
int chargerpump_set_vbat_ovp(struct chargerpump_dev *charger_pump, int mv);
int chargerpump_set_ibat_ocp(struct chargerpump_dev *charger_pump, int ma);
int chargerpump_set_enable_adc(struct chargerpump_dev *charger_pump, bool enable);
int chargerpump_get_is_enable(struct chargerpump_dev *charger_pump, bool *enable);
int chargerpump_get_status(struct chargerpump_dev *charger_pump, uint32_t *status);
int chargerpump_get_adc_value(struct chargerpump_dev *charger_pump, enum cp_adc_channel ch, int *value);
int chargerpump_get_chip_id(struct chargerpump_dev *chargerpump_dev, int *id);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
int chargerpump_set_cp_workmode(struct chargerpump_dev *chargerpump, int workmode);
int chargerpump_get_cp_workmode(struct chargerpump_dev *chargerpump, int *workmode);
#endif
int chargerpump_dump_register(struct chargerpump_dev *chargerpump);
int chargerpump_reset_register(struct chargerpump_dev *chargerpump);

#endif /* __LINUX_HUAQIN_CP__CLASS_H__ */
