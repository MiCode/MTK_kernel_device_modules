// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */

#ifndef __LINUX_SOUTHCHIP_DVCHG_CLASS_H__
#define __LINUX_SOUTHCHIP_DVCHG_CLASS_H__

#include "../charger_policy/adc_channel_def.h"

#define DVCHG_ERROR_VBUS_HIGH        BIT(0)
#define DVCHG_ERROR_VBUS_LOW         BIT(1)
#define DVCHG_ERROR_VBUS_OVP         BIT(2)
#define DVCHG_ERROR_IBUS_OCP         BIT(3)
#define DVCHG_ERROR_VBAT_OVP         BIT(4)
#define DVCHG_ERROR_IBAT_OCP         BIT(5)

struct dvchg_dev;
struct dvchg_ops {
	int (*set_chip_init)(struct dvchg_dev *);
	int (*set_enable)(struct dvchg_dev *, bool);
	int (*set_vbus_ovp)(struct dvchg_dev *, int);
	int (*set_ibus_ocp)(struct dvchg_dev *, int);
	int (*set_vbat_ovp)(struct dvchg_dev *, int);
	int (*set_ibat_ocp)(struct dvchg_dev *, int);
	int (*set_enable_adc)(struct dvchg_dev *, bool);

	int (*get_is_enable)(struct dvchg_dev *, bool *);
	int (*get_status)(struct dvchg_dev *, uint32_t *);
	int (*get_adc_value)(struct dvchg_dev *, enum sc_adc_channel, int *);
};

struct dvchg_dev {
	struct device dev;
	char *name;
	void *private;
	struct dvchg_ops *ops;

	bool changed;
	struct mutex changed_lock;
	struct work_struct changed_work;
};

struct dvchg_dev *dvchg_find_dev_by_name(const char *name);
struct dvchg_dev *dvchg_register(char *name, struct device *parent,
							struct dvchg_ops *ops, void *private);
void *dvchg_get_private(struct dvchg_dev *charger);
int dvchg_unregister(struct dvchg_dev *charger);

int dvchg_set_chip_init(struct dvchg_dev *charger_pump);
int dvchg_set_enable(struct dvchg_dev *charger_pump, bool enable);
int dvchg_set_vbus_ovp(struct dvchg_dev *charger_pump, int mv);
int dvchg_set_ibus_ocp(struct dvchg_dev *charger_pump, int ma);
int dvchg_set_vbat_ovp(struct dvchg_dev *charger_pump, int mv);
int dvchg_set_ibat_ocp(struct dvchg_dev *charger_pump, int ma);
int dvchg_set_enable_adc(struct dvchg_dev *charger_pump, bool enable);
int dvchg_get_is_enable(struct dvchg_dev *charger_pump, bool *enable);
int dvchg_get_status(struct dvchg_dev *charger_pump, uint32_t *status);
int dvchg_get_adc_value(struct dvchg_dev *charger_pump, enum sc_adc_channel ch, int *value);

#endif /* __LINUX_SOUTHCHIP_DVCHG__CLASS_H__ */