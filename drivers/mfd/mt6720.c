// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2025 Mediatek Inc.
// Copyright (c) 2025 Richtek Technology Corp.
// Author: ChiYuan Huang <cy_huang@richtek.com>

#include <dt-bindings/mfd/mt6720.h>
#include <linux/bits.h>
#include <linux/cpumask.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/irqdomain.h>
#include <linux/irqnr.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/sched.h>
#include <linux/sysfs.h>

#define MT6720_REG_DEV_INFO	0x00
#define MT6720_REG_TM_PASS_CODE	0x07
#define MT6720_REG_IRQ_IND	0x0B
#define MT6720_REG_SPMI_TXDRV2	0x2B
#define MT6720_REG_CHG_IRQ0	0x50
#define MT6720_REG_CHG_MASK0	0x90

#define MT6720_MASK_IRQ_UFCS	BIT(0)
#define MT6720_VENID_MASK	GENMASK(7, 4)

#define MT6720_IRQEVT_RGCNT	14
#define MT6720_IRQNSPARSE_RGCNT	27
#define MT6720_VENDOR_ID	0x70
#define MT6720_MAX_I2C_ADDR	10
#define MT6720_MAX_ADDRLEN	2
#define MT6720_I2C_TM_BANKID	3

static const u16 mt6720_slaves[MT6720_MAX_I2C_ADDR] = {
	0x34, 0x52, 0x53, 0x3f, 0x4e, 0x4f, 0x58, 0x1a, 0x4a, 0x64
};

struct mt6720_info {
	struct device *dev;
	struct i2c_client *i2c[MT6720_MAX_I2C_ADDR];
	struct irq_domain *irq_domain;
	u8 irqmask_buffer[MT6720_IRQEVT_RGCNT];
	struct mutex irq_chip_lock;
	bool bypass_retrigger;
};

static int mt6720_regmap_read(void *context, const void *reg_buf,
			      size_t reg_size, void *val_buf, size_t val_size)
{
	struct mt6720_info *info = context;
	const u8 *u8_buf = reg_buf;
	u8 bank_idx = u8_buf[0], bank_addr = u8_buf[1];
	struct i2c_client *i2c = info->i2c[bank_idx];
	struct i2c_msg msg[] = {
		{
			.addr = i2c->addr,
			.flags = i2c->flags,
			.len = 1,
			.buf = &bank_addr,
		}, {
			.addr = i2c->addr,
			.flags = i2c->flags | I2C_M_RD,
			.len = val_size,
			.buf = val_buf,
		},
	};
	int ret;

	ret = i2c_transfer(i2c->adapter, msg, ARRAY_SIZE(msg));
	if (ret == ARRAY_SIZE(msg))
		return 0;
	return ret < 0 ? ret : -EIO;
}

static int mt6720_regmap_write(void *context, const void *data, size_t count)
{
	struct mt6720_info *info = context;
	const u8 *u8_buf = data;
	u8 bank_idx, bank_addr;
	int len = count - MT6720_MAX_ADDRLEN;
	u16 addr = *(u16 *)data;

	bank_idx = u8_buf[0];
	bank_addr = u8_buf[1];

	/*
	 * If using gpio-eint for IRQ triggering,
	 * DO NOT ACCESS the address of RCS retrigger!
	 */
	if ((addr == MT6720_REG_SPMI_TXDRV2) && info->bypass_retrigger)
		return 0;

	return i2c_smbus_write_i2c_block_data(info->i2c[bank_idx], bank_addr, len,
					      data + MT6720_MAX_ADDRLEN);
}

static const struct regmap_bus mt6720_regmap_bus = {
	.read = mt6720_regmap_read,
	.write = mt6720_regmap_write,
};

static const struct regmap_config mt6720_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = (MT6720_MAX_I2C_ADDR << 8) - 1,
};

/*
 * P1 - CHG, offset 0, size 4
 * P2 - BASE/ADC/CHRDET/SCP, offset 8, size 5
 * P3 - DIV2, offset 16, size 3
 * P4 - GM, offset 24, size 1
 * P5 - PD, offset 26, size 1
 */
static const struct {
	unsigned int offset;
	size_t size;
} irq_offset[] = {
	{ 0, 4 }, { 8, 5 }, { 16, 3 }, { 24, 1 }, { 26, 1 }
};

static void mt6720_irqevt_padding(u8 *unpads, size_t len)
{
	u8 pads[MT6720_IRQEVT_RGCNT] = {0}, *padptr;
	int i;

	if (len != MT6720_IRQNSPARSE_RGCNT)
		return;

	for (i = 0, padptr = pads; i < ARRAY_SIZE(irq_offset); i++) {
		memcpy(padptr, unpads + irq_offset[i].offset, irq_offset[i].size);
		padptr += irq_offset[i].size;
	}

	/* Copy back to the original buffer */
	memcpy(unpads, pads, MT6720_IRQEVT_RGCNT);
}

static void mt6720_irqevt_unpadding(u8 *pads, size_t len)
{
	u8 unpads[MT6720_IRQNSPARSE_RGCNT] = {0}, *padptr;
	int i;

	if (len != MT6720_IRQNSPARSE_RGCNT)
		return;

	for (i = 0, padptr = pads; i < ARRAY_SIZE(irq_offset); i++) {
		memcpy(unpads + irq_offset[i].offset, padptr, irq_offset[i].size);
		padptr += irq_offset[i].size;
	}

	/* Copy back to the original buffer */
	memcpy(pads, unpads, MT6720_IRQNSPARSE_RGCNT);
}

static irqreturn_t mt6720_irq_thread_handler(int irq, void *d)
{
	struct mt6720_info *info = d;
	struct regmap *regmap = dev_get_regmap(info->dev, NULL);
	u8 irqnsparse[MT6720_IRQNSPARSE_RGCNT], irqevts[MT6720_IRQEVT_RGCNT];
	unsigned int irq_ind;
	int i, j, ret;

	ret = regmap_raw_read(regmap, MT6720_REG_CHG_IRQ0, irqnsparse, MT6720_IRQNSPARSE_RGCNT);
	ret |= regmap_read(regmap, MT6720_REG_IRQ_IND, &irq_ind);
	if (ret)
		return IRQ_NONE;

	mt6720_irqevt_padding(irqnsparse, MT6720_IRQNSPARSE_RGCNT);

	mutex_lock(&info->irq_chip_lock);
	for (i = 0; i < MT6720_IRQEVT_RGCNT; i++)
		irqnsparse[i] &= ~info->irqmask_buffer[i];
	mutex_unlock(&info->irq_chip_lock);

	memcpy(irqevts, irqnsparse, MT6720_IRQEVT_RGCNT);
	mt6720_irqevt_unpadding(irqnsparse, MT6720_IRQNSPARSE_RGCNT);

	ret = regmap_raw_write(regmap, MT6720_REG_CHG_IRQ0, irqnsparse, MT6720_IRQNSPARSE_RGCNT);
	if (ret)
		return IRQ_NONE;

	if (irq_ind & MT6720_MASK_IRQ_UFCS)
		irqevts[MT6720_EVT_DPDM_UFCS / 8] |= BIT(MT6720_EVT_DPDM_UFCS % 8);

	for (i = 0; i < MT6720_IRQEVT_RGCNT; i++) {
		for (j = 0; irqevts[i] && j < 8; j++) {
			if (irqevts[i] & BIT(j))
				handle_nested_irq(irq_find_mapping(info->irq_domain, i * 8 + j));
		}
	}

	return IRQ_HANDLED;
}

static void mt6720_irq_enable(struct irq_data *d)
{
	struct mt6720_info *info = irq_data_get_irq_chip_data(d);

	if (d->hwirq == MT6720_EVT_DPDM_UFCS)
		return;

	info->irqmask_buffer[d->hwirq / 8] &= ~BIT(d->hwirq % 8);
}

static void mt6720_irq_disable(struct irq_data *d)
{
	struct mt6720_info *info = irq_data_get_irq_chip_data(d);

	if (d->hwirq == MT6720_EVT_DPDM_UFCS)
		return;

	info->irqmask_buffer[d->hwirq / 8] |= BIT(d->hwirq % 8);
}

static void mt6720_irq_bus_lock(struct irq_data *d)
{
	struct mt6720_info *info = irq_data_get_irq_chip_data(d);

	mutex_lock(&info->irq_chip_lock);
}

static void mt6720_irq_bus_sync_unlock(struct irq_data *d)
{
	struct mt6720_info *info = irq_data_get_irq_chip_data(d);
	struct regmap *regmap = dev_get_regmap(info->dev, NULL);
	int i, ret, rg_offs, idx = d->hwirq / 8;
	bool found = false;

	if (d->hwirq == MT6720_EVT_DPDM_UFCS)
		goto out;

	/* Find RG offset start from REG_CHG_IRQ0 */
	for (i = 0, rg_offs = 0; i < ARRAY_SIZE(irq_offset); i++) {
		rg_offs += irq_offset[i].size;

		if (rg_offs > idx) {
			rg_offs -= irq_offset[i].size;
			rg_offs = irq_offset[i].offset + idx - rg_offs;
			found = true;
			break;
		}
	}

	if (found) {
		ret = regmap_write(regmap, MT6720_REG_CHG_MASK0 + rg_offs,
				   info->irqmask_buffer[idx]);
		if (ret)
			dev_err(info->dev, "Failed to update irq %ld RG\n", d->hwirq);
	}

out:
	mutex_unlock(&info->irq_chip_lock);
}

static const struct irq_chip mt6720_irq_chip = {
	.name = "mt6720-irqs",
	.irq_bus_lock = mt6720_irq_bus_lock,
	.irq_bus_sync_unlock = mt6720_irq_bus_sync_unlock,
	.irq_enable = mt6720_irq_enable,
	.irq_disable = mt6720_irq_disable,
	.flags = IRQCHIP_SKIP_SET_WAKE,
};

static int mt6720_irq_domain_map(struct irq_domain *d, unsigned int virq, irq_hw_number_t hwirq)
{
	struct mt6720_info *info = d->host_data;
	struct i2c_client *i2c = to_i2c_client(info->dev);

	irq_set_chip_data(virq, info);
	irq_set_chip(virq, &mt6720_irq_chip);
	irq_set_nested_thread(virq, true);
	irq_set_parent(virq, i2c->irq);
	irq_set_noprobe(virq);

	return 0;
}

static const struct irq_domain_ops mt6720_irq_domain_ops = {
	.xlate = irq_domain_xlate_onetwocell,
	.map = mt6720_irq_domain_map,
};

static void mt6720_irq_domain_release(void *d)
{
	struct mt6720_info *info = d;

	irq_domain_remove(info->irq_domain);
	mutex_destroy(&info->irq_chip_lock);
}

static int mt6720_register_interrupt(struct mt6720_info *info)
{
	struct device *dev = info->dev;
	struct regmap *regmap = dev_get_regmap(dev, NULL);
	struct i2c_client *i2c = to_i2c_client(dev);
	u8 irqmask[MT6720_IRQNSPARSE_RGCNT] = { 0 };
	struct task_struct *irq_thread;
	struct cpumask new_cpumask;
	struct irq_desc *desc;
	int ret;

	memset(irqmask, 0xff, MT6720_IRQEVT_RGCNT);
	mt6720_irqevt_unpadding(irqmask, MT6720_IRQNSPARSE_RGCNT);

	ret = regmap_raw_write(regmap, MT6720_REG_CHG_MASK0, irqmask, MT6720_IRQNSPARSE_RGCNT);
	ret |= regmap_raw_write(regmap, MT6720_REG_CHG_IRQ0, irqmask, MT6720_IRQNSPARSE_RGCNT);
	if (ret)
		return dev_err_probe(dev, ret, "Failed masknwrc evts\n");

	mutex_init(&info->irq_chip_lock);

	info->irq_domain = irq_domain_create_linear(dev_fwnode(dev), MT6720_IRQEVT_RGCNT * 8,
						    &mt6720_irq_domain_ops, info);
	if (!info->irq_domain)
		return dev_err_probe(dev, -EINVAL, "Failed to add irq domain\n");

	ret = devm_add_action_or_reset(dev, mt6720_irq_domain_release, info);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to add irq domain release action\n");

	ret = devm_request_threaded_irq(dev, i2c->irq, NULL, mt6720_irq_thread_handler,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT, dev_name(dev), info);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to request MT6720 IRQ(irq:%d)\n", i2c->irq);

	desc = irq_to_desc(i2c->irq);
	irq_thread = desc->action->thread;
	cpumask_copy(&new_cpumask, cpu_online_mask);
	cpumask_clear_cpu(0, &new_cpumask);
	if (cpumask_any(&new_cpumask) < nr_cpu_ids)
		set_cpus_allowed_ptr(irq_thread, &new_cpumask);

	return 0;
}

static void mt6720_check_of_irq(struct mt6720_info *info)
{
	struct device_node *parent;

	info->bypass_retrigger = false;

	parent = of_irq_find_parent(info->dev->of_node);
	if (parent) {
		if (of_property_read_bool(parent, "gpio-controller"))
			info->bypass_retrigger = true;

		of_node_put(parent);
	}

	dev_notice(info->dev, "%s, bypass_retrigger: %d\n", __func__, info->bypass_retrigger);
}

static int mt6720_probe(struct i2c_client *i2c)
{
	struct i2c_adapter *adap = i2c->adapter;
	struct device *dev = &i2c->dev;
	struct mt6720_info *info;
	struct regmap *regmap;
	unsigned int ven_id;
	int i, ret;

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = dev;
	memset(info->irqmask_buffer, 0xff, MT6720_IRQEVT_RGCNT);
	i2c_set_clientdata(i2c, info);

	info->i2c[0] = i2c;
	for (i = 1; i < MT6720_MAX_I2C_ADDR; i++) {
		info->i2c[i] = devm_i2c_new_dummy_device(dev, adap, mt6720_slaves[i]);
		if (IS_ERR(info->i2c[i])) {
			ret = PTR_ERR(info->i2c[i]);
			return dev_err_probe(dev, ret, "Failed to new [%d] i2c client\n", i);
		}
	}

	mt6720_check_of_irq(info);
	dev_set_drvdata(dev, info);

	regmap = devm_regmap_init(dev, &mt6720_regmap_bus, info, &mt6720_regmap_config);
	if (IS_ERR(regmap))
		return dev_err_probe(dev, PTR_ERR(regmap), "Failed to init regmap\n");

	ret = regmap_read(regmap, MT6720_REG_DEV_INFO, &ven_id);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to read device information\n");

	if ((ven_id & MT6720_VENID_MASK) != MT6720_VENDOR_ID)
		return dev_err_probe(dev, -ENODEV, "Incorrect vendor id (0x%02x)\n", ven_id);

	ret = mt6720_register_interrupt(info);
	if (ret)
		return dev_err_probe(dev, ret, "Fialed to register interrupt\n");

	return devm_of_platform_populate(dev);
}

static const struct of_device_id mt6720_dt_match_table[] = {
	{ .compatible = "mediatek,mt6720" },
	{}
};
MODULE_DEVICE_TABLE(of, mt6720_dt_match_table);

static struct i2c_driver mt6720_driver = {
	.driver = {
		.name = "mt6720",
		.of_match_table = mt6720_dt_match_table,
	},
	.probe = mt6720_probe,
};
module_i2c_driver(mt6720_driver);

MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("Mediatek MT6720 SubPMIC Driver");
MODULE_LICENSE("GPL");
