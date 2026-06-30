// SPDX-License-Identifier: GPL-2.0-only
/*
 * mt6379-i2c.c -- I2C access for Mediatek MT6379
 *
 * Copyright (c) 2024 MediaTek Inc.
 *
 * Author: ChiYuan Huang <cy_huang@richtek.com>
 */

#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/regmap.h>

#include "mt6379.h"

#define MT6379_REG_ADDR_SIZE	2

enum {
	MT6379_I2C_PMU = 0,
	MT6379_I2C_CHG,
	MT6379_I2C_FLED,
	MT6379_I2C_TM,
	MT6379_I2C_PD1,
	MT6379_I2C_PD2,
	MT6379_I2C_DPDM,
	MT6379_I2C_BAT1_BM,
	MT6379_I2C_BAT1_HK1,
	MT6379_I2C_BAT1_HK2,
	MT6379_I2C_BAT2_BM,
	MT6379_I2C_BAT2_HK1,
	MT6379_I2C_BAT2_HK2,
	MT6379_I2C_EUSB,
	MT6379_MAX_I2C,
};

struct mt6379_priv {
	struct device *dev;
	struct i2c_client *i2c_devs[MT6379_MAX_I2C];
};

static const unsigned int mt6379_i2c_addr_map[MT6379_MAX_I2C] = {
	[MT6379_I2C_CHG]	= 0x52,
	[MT6379_I2C_FLED]	= 0x53,
	[MT6379_I2C_TM]		= 0x3F,
	[MT6379_I2C_PD1]	= 0x4E,
	[MT6379_I2C_PD2]	= 0x4F,
	[MT6379_I2C_DPDM]	= 0x58,
	[MT6379_I2C_BAT1_BM]	= 0x1A,
	[MT6379_I2C_BAT1_HK1]	= 0x4A,
	[MT6379_I2C_BAT1_HK2]	= 0x64,
	[MT6379_I2C_BAT2_BM]	= 0x1B,
	[MT6379_I2C_BAT2_HK1]	= 0x4B,
	[MT6379_I2C_BAT2_HK2]	= 0x65,
	[MT6379_I2C_EUSB]	= 0x66,
};

static int mt6379_regmap_read(void *context, const void *reg_buf,
			      size_t reg_size, void *val_buf, size_t val_size)
{
	struct mt6379_data *data = context;
	struct mt6379_priv *priv = data->priv;
	const u8 *u8_buf = reg_buf;
	u8 bank_idx, bank_addr;
	int ret;

	bank_idx = u8_buf[0];

	if (bank_idx >= MT6379_MAX_I2C)
		return -EINVAL;

	bank_addr = u8_buf[1];
	ret = i2c_smbus_read_i2c_block_data(priv->i2c_devs[bank_idx], bank_addr,
					    val_size, val_buf);
	if (ret < 0)
		return ret;
	else if (ret != val_size)
		return -EIO;

	return 0;
}

static int mt6379_regmap_write(void *context, const void *val_buf, size_t count)
{
	const u8 *reg_buf = val_buf, *wrdata = val_buf + MT6379_REG_ADDR_SIZE;
	int len = count - MT6379_REG_ADDR_SIZE;
	struct mt6379_data *data = context;
	struct mt6379_priv *priv = data->priv;
	u8 bank_idx, bank_addr;

	bank_idx = reg_buf[0];

	if (bank_idx >= MT6379_MAX_I2C || len < 0)
		return -EINVAL;

	bank_addr = reg_buf[1];

	/* If using i2c interface, DO NOT ACCESS the address of RCS retrigger! */
	if ((((u32)bank_idx << 8) | (u32)bank_addr) == MT6379_REG_SPMI_TXDRV2)
		return 0;

	return i2c_smbus_write_i2c_block_data(priv->i2c_devs[bank_idx], bank_addr, len, wrdata);
}

static const struct regmap_bus mt6379_i2c_bus = {
	.read	= mt6379_regmap_read,
	.write	= mt6379_regmap_write,
};

static const struct regmap_config mt6379_i2c_config = {
	.reg_bits		= 16,
	.val_bits		= 8,
	.reg_format_endian	= REGMAP_ENDIAN_BIG,
	.max_register		= 0xdff,
};

static void mt6379_i2c_irq_bus_lock(struct irq_data *d)
{
	struct mt6379_data *data = irq_data_get_irq_chip_data(d);

	mutex_lock(&data->irq_lock);
}

static void mt6379_i2c_irq_bus_sync_unlock(struct irq_data *d)
{
	struct mt6379_data *data = irq_data_get_irq_chip_data(d);
	struct device *dev = data->dev;
	unsigned int hwirq_reg;
	int ret;

	hwirq_reg = mt6379_find_irq_hwreg(d->hwirq);
	ret = regmap_write(data->regmap, hwirq_reg, data->mask_buf[d->hwirq / 8]);
	if (ret)
		dev_info(dev, "%s, Failed to config for hwirq %ld\n", __func__, d->hwirq);

	mutex_unlock(&data->irq_lock);
}

static void mt6379_i2c_irq_enable(struct irq_data *d)
{
	struct mt6379_data *data = irq_data_get_irq_chip_data(d);

	data->mask_buf[d->hwirq / 8] &= ~BIT(d->hwirq % 8);
}

static void mt6379_i2c_irq_disable(struct irq_data *d)
{
	struct mt6379_data *data = irq_data_get_irq_chip_data(d);

	data->mask_buf[d->hwirq / 8] |= BIT(d->hwirq % 8);
}

static const struct irq_chip mt6379_i2c_irq_chip = {
	.name = "mt6379-i2c-irqs",
	.irq_bus_lock = mt6379_i2c_irq_bus_lock,
	.irq_bus_sync_unlock = mt6379_i2c_irq_bus_sync_unlock,
	.irq_enable = mt6379_i2c_irq_enable,
	.irq_disable = mt6379_i2c_irq_disable,
	.flags = IRQCHIP_SKIP_SET_WAKE,
};

static int mt6379_probe(struct i2c_client *i2c)
{
	struct device *dev = &i2c->dev;
	struct i2c_client *i2c_new;
	struct mt6379_data *data;
	struct mt6379_priv *priv;
	struct irq_chip *irqc;
	int irqno = i2c->irq;
	char *irqc_name;
	int i;

	if (irqno <= 0) {
		dev_info(dev, "%s, Invalid irq number (%d)\n", __func__, irqno);
		return -EINVAL;
	}

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;
	priv->i2c_devs[MT6379_I2C_PMU] = i2c;

	for (i = MT6379_I2C_CHG; i < MT6379_MAX_I2C; i++) {
		i2c_new = devm_i2c_new_dummy_device(dev, i2c->adapter,
						    mt6379_i2c_addr_map[i]);
		if (IS_ERR(i2c_new)) {
			dev_info(dev, "%s, Failed to new i2c dev (%d)\n", __func__, i);
			return PTR_ERR(i2c_new);
		}

		priv->i2c_devs[i] = i2c_new;
	}

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	dev_set_drvdata(dev, data);
	data->interface_type = MT6379_IFT_I2C;
	data->dev = dev;
	data->irq = irqno;
	data->priv = priv;

	data->regmap = devm_regmap_init(dev, &mt6379_i2c_bus, data,
					&mt6379_i2c_config);
	if (IS_ERR(data->regmap)) {
		dev_info(dev, "%s, Failed to init regmap\n", __func__);
		return PTR_ERR(data->regmap);
	}

	irqc = &data->irq_chip;
	irqc_name = devm_kasprintf(dev, GFP_KERNEL, "mt6379i-irqs(%s)", dev_name(dev));

	memcpy(irqc, &mt6379_i2c_irq_chip, sizeof(data->irq_chip));
	if (irqc_name)
		irqc->name = irqc_name;
	else
		dev_info(dev, "%s, Failed to allocate irq chip name, using default name(%s)\n",
			 __func__, irqc->name);

	return mt6379_device_init(data);
}

static int mt6379_suspend(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);

	disable_irq(i2c->irq);
	return 0;
}

static int mt6379_resume(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);

	enable_irq(i2c->irq);
	return 0;
}

static SIMPLE_DEV_PM_OPS(mt6379_pm_ops, mt6379_suspend, mt6379_resume);

static const struct of_device_id mt6379_i2c_dt_match[] = {
	{ .compatible = "mediatek,mt6379" },
	{ /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, mt6379_i2c_dt_match);

static struct i2c_driver mt6379_driver = {
	.driver = {
		.name = "mt6379",
		.pm = &mt6379_pm_ops,
		.of_match_table = mt6379_i2c_dt_match,
	},
	.probe = mt6379_probe,
};
module_i2c_driver(mt6379_driver);

MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("Mediatek MT6379 I2C Driver");
MODULE_LICENSE("GPL");
