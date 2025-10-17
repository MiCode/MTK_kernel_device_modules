/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 XiaoMi Inc.
 */
#ifndef LINUX_WIRELESS_CHARGER_CLASS_WEAK_H
#define LINUX_WIRELESS_CHARGER_CLASS_WEAK_H
#ifndef CONFIG_XM_WLS_CHG_CLASS_INTF
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/slab.h>
#include "wireless_charger_class.h"

struct wireless_charger_device * __attribute__ ((weak))
        get_wireless_charger_by_name(const char *name)
{
    pr_err("%s: weak func!\n", __func__);
    return (struct wireless_charger_device *)NULL;
}

int __attribute__ ((weak))
        wireless_charger_dev_wls_check_fw_version(struct wireless_charger_device *chg_dev)
{
    pr_err("%s: weak func!\n", __func__);
    return -ENOTSUPP;
}

int __attribute__ ((weak))
        wireless_charger_dev_wls_get_chip_version(struct wireless_charger_device *chg_dev, char *buf)
{
    pr_err("%s: weak func!\n", __func__);
    return -ENOTSUPP;
}

int __attribute__ ((weak))
        wireless_charger_dev_wls_get_vout(struct wireless_charger_device *chg_dev, int *vout)
{
    pr_err("%s: weak func!\n", __func__);
    return -ENOTSUPP;
}

int __attribute__ ((weak))
        wireless_charger_dev_wls_get_iout(struct wireless_charger_device *chg_dev, int *iout)
{
    pr_err("%s: weak func!\n", __func__);
    return -ENOTSUPP;
}

int __attribute__ ((weak))
        wireless_charger_dev_wls_get_vrect(struct wireless_charger_device *chg_dev, int *vrect)
{
    pr_err("%s: weak func!\n", __func__);
    return -ENOTSUPP;
}

int __attribute__ ((weak))
        wireless_charger_dev_wls_get_tx_adapter(struct wireless_charger_device *chg_dev, int *adapter)
{
    pr_err("%s: weak func!\n", __func__);
    return -ENOTSUPP;
}

int __attribute__ ((weak))
        wireless_charger_dev_wls_set_vout(struct wireless_charger_device *chg_dev, int vout)
{
    pr_err("%s: weak func!\n", __func__);
    return -ENOTSUPP;
}

int __attribute__ ((weak))
        wireless_charger_dev_wls_set_rx_sleep_mode(struct wireless_charger_device *chg_dev, int sleep_for_dam)
{
    pr_err("%s: weak func!\n", __func__);
    return -ENOTSUPP;
}

int __attribute__ ((weak))
        wireless_charger_dev_is_car_adapter(struct wireless_charger_device *chg_dev, bool *enable)
{
    pr_err("%s: weak func!\n", __func__);
    return -ENOTSUPP;
}

int __attribute__ ((weak))
        wireless_charger_dev_is_wireless_present(struct wireless_charger_device *chg_dev, bool *present)
{
    pr_err("%s: weak func!\n", __func__);
    return -ENOTSUPP;
}

int __attribute__ ((weak))
        wireless_charger_dev_is_i2c_ok(struct wireless_charger_device *chg_dev, bool *i2c_ok)
{
    pr_err("%s: weak func!\n", __func__);
    return -ENOTSUPP;
}

int __attribute__ ((weak))
        wireless_charger_dev_is_qc_enable(struct wireless_charger_device *chg_dev, bool *enable)
{
    pr_err("%s: weak func!\n", __func__);
    return -ENOTSUPP;
}

int __attribute__ ((weak))
        wireless_charger_dev_is_firmware_update(struct wireless_charger_device *chg_dev, bool *update)
{
    pr_err("%s: weak func!\n", __func__);
    return -ENOTSUPP;
}

int __attribute__ ((weak))
        wireless_charger_dev_wls_firmware_update(struct wireless_charger_device *chg_dev, int cmd)
{
    pr_err("%s: weak func!\n", __func__);
    return -ENOTSUPP;
}

int __attribute__ ((weak))
        wireless_charger_dev_is_enable(struct wireless_charger_device *chg_dev, bool *en)
{
    pr_err("%s: weak func!\n", __func__);
    return -ENOTSUPP;
}

int __attribute__ ((weak))
        wireless_charger_dev_enable_wls_chg(struct wireless_charger_device *chg_dev, bool en)
{
    pr_err("%s: weak func!\n", __func__);
    return -ENOTSUPP;
}

int __attribute__ ((weak))
        wireless_charger_dev_get_reverse_chg(struct wireless_charger_device *chg_dev, bool *enable)
{
    pr_err("%s: weak func!\n", __func__);
    return -ENOTSUPP;
}

int __attribute__ ((weak))
        wireless_charger_dev_enable_wls_reverse_chg(struct wireless_charger_device *chg_dev, bool en)
{
    pr_err("%s: weak func!\n", __func__);
    return -ENOTSUPP;
}

int __attribute__ ((weak))
        wireless_charger_dev_wls_get_reverse_chg_state(struct wireless_charger_device *chg_dev, int *state)
{
    pr_err("%s: weak func!\n", __func__);
    return -ENOTSUPP;
}

int __attribute__ ((weak))
        wireless_charger_dev_wls_set_quiet_sts(struct wireless_charger_device *chg_dev, int quiet_sts)
{
    pr_err("%s: weak func!\n", __func__);
    return -ENOTSUPP;
}

#endif /*CONFIG_XM_WLS_CHG_CLASS_INTF*/
#endif /*LINUX_WIRELESS_CHARGER_CLASS_WEAK_H*/