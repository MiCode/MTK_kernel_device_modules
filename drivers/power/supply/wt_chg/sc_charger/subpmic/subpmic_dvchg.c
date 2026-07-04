// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2022 - 2023 SOUTHCHIP Semiconductor Technology(Shanghai) Co., Ltd.
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/sched/prio.h>
#include <uapi/linux/sched/types.h>
#include <linux/platform_device.h>

#include "subpmic.h"
#include "../../charger_class/dvchg_class.h"

#define SUBPMIC_DVCHG_VERSION           "1.0.0"

struct subpmic_dvchg_device {
    struct i2c_client *client;
    struct device *dev;
    struct regmap *rmap;

    struct dvchg_dev *dvchg;
};

static int subpmic_dvchg_bulk_read(struct subpmic_dvchg_device *sc, u8 reg,
                                u8 *val,size_t count)
{
    int ret = 0;
    ret = regmap_bulk_read(sc->rmap, reg, val, count);
    if (ret < 0) {
        dev_err(sc->dev, "i2c bulk read failed\n");
    }
    return ret;
}

static int subpmic_dvchg_bulk_write(struct subpmic_dvchg_device *sc, u8 reg,
                                u8 *val, size_t count)
{
    int ret = 0;
    ret = regmap_bulk_write(sc->rmap, reg, val, count);
    if (ret < 0) {
        dev_err(sc->dev, "i2c bulk write failed\n");
    }
    return ret;
}

static int subpmic_dvchg_write_byte(struct subpmic_dvchg_device *sc, u8 reg,
                                                    u8 val)
{
    u8 temp = val;
    return subpmic_dvchg_bulk_write(sc, reg, &temp, 1);
}

static int subpmic_dvchg_read_byte(struct subpmic_dvchg_device *sc, u8 reg,
                                                    u8 *val)
{
    return subpmic_dvchg_bulk_read(sc, reg, val, 1);
}

#ifdef CONFIG_SOUTHCHIP_DVCHG_CLASS_MODULE
static int subpmic_dvchg_set_enable(struct dvchg_dev *dvchg, bool enable)
{
    struct subpmic_dvchg_device *sc = dvchg_get_private(dvchg);
    int ret = 0;
    uint8_t val = 0;
    
    ret = subpmic_dvchg_read_byte(sc, SUBPMIC_REG_CHG_CTRL, &val);

    if (enable) {
        subpmic_dvchg_write_byte(sc, SUBPMIC_REG_CHG_CTRL, val | 0x40);
    } else {
        subpmic_dvchg_write_byte(sc, SUBPMIC_REG_CHG_CTRL, val & (~0x40));
    }

    ret = subpmic_dvchg_read_byte(sc, 0x64, &val);

    val = enable ? val | 0x01 : val & (~0x01);

    ret = subpmic_dvchg_write_byte(sc, 0x64, val);
    return 0;
}

static int subpmic_dvchg_get_is_enable(struct dvchg_dev *dvchg, bool *enable)
{
    struct subpmic_dvchg_device *sc = dvchg_get_private(dvchg);
    int ret = 0;
    uint8_t val = 0;
    ret = subpmic_dvchg_read_byte(sc, 0x66, &val);

    *enable = val & 0x01;
    return 0;
}

static int subpmic_dvchg_get_status(struct dvchg_dev *dvchg, uint32_t *status)
{
    struct subpmic_dvchg_device *sc = dvchg_get_private(dvchg);
    int ret = 0;
    uint8_t val = 0;
    ret = subpmic_dvchg_read_byte(sc, 0x66, &val);
    *status = 0;
    if (val & BIT(1))
        *status |= DVCHG_ERROR_VBUS_LOW;
    if (val & BIT(2))
        *status |= DVCHG_ERROR_VBUS_HIGH;
    return 0;
}

static struct dvchg_ops dvchg_ops = {
    .set_enable = subpmic_dvchg_set_enable,
    .get_status = subpmic_dvchg_get_status,
    .get_is_enable = subpmic_dvchg_get_is_enable,
};
#endif /* CONFIG_SOUTHCHIP_DVCHG_CLASS_MODULE */

static int subpmic_dvchg_probe(struct platform_device *pdev)
{
    struct subpmic_dvchg_device *sc;
    struct device *dev = &pdev->dev;
    int ret = 0;

    dev_info(dev, "%s (%s)\n", __func__, SUBPMIC_DVCHG_VERSION);

    sc = devm_kzalloc(dev, sizeof(*sc), GFP_KERNEL);
    if (!sc)
        return -ENOMEM;

    sc->rmap = dev_get_regmap(dev->parent, NULL);
    if (!sc->rmap) {
        dev_err(dev, "failed to get regmap\n");
        return -ENODEV;
    }
    sc->dev = dev;
    platform_set_drvdata(pdev, sc);

#ifdef CONFIG_SOUTHCHIP_DVCHG_CLASS_MODULE
    sc->dvchg = dvchg_register("sc_dvchg",
                             sc->dev, &dvchg_ops, sc);
    if (!sc->dvchg) {
        ret = PTR_ERR(sc->dvchg);
        goto err;
    }
#endif /* CONFIG_SOUTHCHIP_CLASS_MODULE */

    dev_info(dev, "%s success probe\n", __func__);
    return 0;

err:
    return ret;
}

static int subpmic_dvchg_remove(struct platform_device *pdev)
{
    return 0;
}

static const struct of_device_id sc6607_of_match[] = {
    {.compatible = "HUAQIN,subpmic_dvchg",},
    {},
};
MODULE_DEVICE_TABLE(of, sc6607_of_match);

static struct platform_driver subpmic_dvchg_driver = {
    .driver = {
        .name = "subpmic_dvchg",
        .of_match_table = of_match_ptr(sc6607_of_match),
    },
    .probe = subpmic_dvchg_probe,
    .remove = subpmic_dvchg_remove,
};

module_platform_driver(subpmic_dvchg_driver);

MODULE_AUTHOR("Boqiang Liu <air-liu@HUAQIN.com>");
MODULE_DESCRIPTION("subpmic dvchg core driver");
MODULE_LICENSE("GPL v2");
