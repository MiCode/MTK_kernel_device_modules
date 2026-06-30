#ifndef __SC8541_CHARGER_H
#define __SC8541_CHARGER_H

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/alarmtimer.h>
#include <linux/kernel.h>
#include <linux/i2c.h>

#define CP_SYSFS_FIELD_RO(_name, _prop) \
{                       \
        .attr   = __ATTR(_name, 0444, cp_sysfs_show, cp_sysfs_store),\
        .prop   = _prop,                                  \
        .get    = _name##_get,                                          \
}

enum cp_property {
        CP_PROP_VBUS,
        CP_PROP_IBUS,
        CP_PROP_TDIE,
        CP_PROP_CHIP_OK,
};

#endif