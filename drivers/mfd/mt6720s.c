// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2025 Mediatek Inc.
// Copyright (c) 2025 Richtek Technology Corp.
// Author: ChiYuan Huang <cy_huang@richtek.com>
// Author: ChiaEn Wu <chiaen_wu@richtek.com>

#include <asm-generic/errno-base.h>
#include <dt-bindings/mfd/mt6720.h>
#include <linux/bits.h>
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
#include <linux/pm_wakeirq.h>
#include <linux/regmap.h>
#include <linux/sched.h>
#include <linux/spmi.h>
#include <linux/sysfs.h>
#include <linux/unaligned.h>

#define MT6720_REG_DEV_INFO		0x00
#define MT6720_REG_TM_PASS_CODE		0x07
#define MT6720_REG_IRQ_IND		0x0B
#define MT6720_REG_SPMI_RCS1		0x26
#define MT6720_REG_SPMI_RCS2		0x27
#define MT6720_REG_SPMI_TXDRV2		0x2B
#define MT6720_REG_CHG_IRQ0		0x50
#define MT6720_REG_CHG_MASK0		0x90

#define MT6720_MASK_IRQ_UFCS		BIT(0)
#define MT6720_MASK_RGS_RCS_INIT_DONE	BIT(0)
#define MT6720_VENID_MASK		GENMASK(7, 4)

#define MT6720_IRQEVT_RGCNT		14
#define MT6720_IRQNSPARSE_RGCNT		27
#define MT6720_VENDOR_ID		0x70
#define MT6720_WRRD_WAIT_US		8
#define MT6720_DEFAULT_SVID		0x0E

struct mt6720_info {
	struct device *dev;
	struct irq_domain *irq_domain;
	struct mutex irq_chip_lock;
	bool bypass_retrigger;
	int irq;
	u32 svid;
	u8 irqmask_buffer[MT6720_IRQEVT_RGCNT];
	u16 access_reg;
	size_t access_len;
	ktime_t access_time;
};

static void check_rg_access_limit(struct mt6720_info *info, u16 addr, size_t len, ktime_t now)
{
	ktime_t guarantee_time = ktime_add_us(info->access_time, MT6720_WRRD_WAIT_US);
	s64 min_wait_time;

	/* Check RG addr overlap */
	if (info->access_reg >= addr + len || addr >= info->access_reg + info->access_len)
		return;

	/* Check RG aceess time is after previous time + 9us */
	if (ktime_after(now, guarantee_time))
		return;

	min_wait_time = ktime_us_delta(guarantee_time, now);
	udelay(min_wait_time);
}

static void put_rg_access_limit(struct mt6720_info *info, u16 addr, size_t len, ktime_t now)
{
	info->access_reg = addr;
	info->access_len = len;
	info->access_time = now;
}

static int mt6720_regmap_read(void *context, const void *reg_buf,
			      size_t reg_size, void *val_buf, size_t val_size)
{
	struct mt6720_info *info = context;
	struct spmi_device *sdev = to_spmi_device(info->dev);
	u16 addr;

	/* The SPMI I/O limitation of MTK common platform is 2 bytes */
	WARN_ON_ONCE(reg_size != 2);

	addr = get_unaligned_be16(reg_buf);
	check_rg_access_limit(info, addr, val_size, ktime_get());

	return spmi_ext_register_readl(sdev, addr, val_buf, val_size);
}

static int mt6720_regmap_write(void *context, const void *val, size_t val_len)
{
	struct mt6720_info *info = context;
	struct spmi_device *sdev = to_spmi_device(info->dev);
	u16 addr;
	int ret;

	WARN_ON_ONCE(val_len < 2);

	addr = get_unaligned_be16(val);

	/*
	 * If using gpio-eint for IRQ triggering,
	 * DO NOT ACCESS the address of RCS retrigger!
	 */
	if (info->bypass_retrigger && addr <= MT6720_REG_SPMI_TXDRV2
	    && (addr + val_len - 2) > MT6720_REG_SPMI_TXDRV2)
		return 0;

	ret = spmi_ext_register_writel(sdev, addr, val + 2, val_len - 2);
	if (ret)
		return ret;

	put_rg_access_limit(info, addr, val_len - 2, ktime_get());
	return 0;
}

static int mt6720_regmap_gather_write(void *context, const void *reg,
				      size_t reg_len, const void *val,
				      size_t val_len)
{
	struct mt6720_info *info = context;
	struct spmi_device *sdev = to_spmi_device(info->dev);
	u16 addr;
	int ret;

	/* The SPMI I/O limitation of MTK common platform is 2 bytes */
	WARN_ON_ONCE(reg_len != 2);

	addr = get_unaligned_be16(reg);

	/*
	 * If using gpio-eint for IRQ triggering,
	 * DO NOT ACCESS the address of RCS retrigger!
	 */
	if (info->bypass_retrigger && addr <= MT6720_REG_SPMI_TXDRV2
	    && (addr + val_len) > MT6720_REG_SPMI_TXDRV2)
		return 0;

	ret = spmi_ext_register_writel(sdev, addr, val, val_len);
	if (ret)
		return ret;

	put_rg_access_limit(info, addr, val_len, ktime_get());
	return 0;
}

static const struct regmap_bus mt6720_regmap_bus = {
	.read = mt6720_regmap_read,
	.write = mt6720_regmap_write,
	.gather_write = mt6720_regmap_gather_write,
	.max_raw_read = 2,
	.max_raw_write = 2,
	.fast_io = true,
};

static const struct regmap_config mt6720_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.reg_format_endian = REGMAP_ENDIAN_BIG,
	.max_register = 0x9ff,
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

	if (!regmap) {
		dev_info(info->dev, "%s, BUG: No regmap!!\n", __func__);
		return IRQ_HANDLED;
	}

	ret = regmap_raw_read(regmap, MT6720_REG_CHG_IRQ0, irqnsparse, MT6720_IRQNSPARSE_RGCNT);
	ret |= regmap_read(regmap, MT6720_REG_IRQ_IND, &irq_ind);
	if (ret) {
		dev_info(info->dev, "%s, Failed to read IRQ flags\n", __func__);
		goto out;
	}

	mt6720_irqevt_padding(irqnsparse, MT6720_IRQNSPARSE_RGCNT);

	mutex_lock(&info->irq_chip_lock);
	for (i = 0; i < MT6720_IRQEVT_RGCNT; i++)
		irqnsparse[i] &= ~info->irqmask_buffer[i];
	mutex_unlock(&info->irq_chip_lock);

	memcpy(irqevts, irqnsparse, MT6720_IRQEVT_RGCNT);
	mt6720_irqevt_unpadding(irqnsparse, MT6720_IRQNSPARSE_RGCNT);

	ret = regmap_raw_write(regmap, MT6720_REG_CHG_IRQ0, irqnsparse, MT6720_IRQNSPARSE_RGCNT);
	if (ret) {
		dev_info(info->dev, "%s, Failed to clear IRQ flags\n", __func__);
		goto out;
	}

	if (irq_ind & MT6720_MASK_IRQ_UFCS)
		irqevts[MT6720_EVT_DPDM_UFCS / 8] |= BIT(MT6720_EVT_DPDM_UFCS % 8);

	for (i = 0; i < MT6720_IRQEVT_RGCNT; i++) {
		for (j = 0; irqevts[i] && j < 8; j++) {
			if (irqevts[i] & BIT(j))
				handle_nested_irq(irq_find_mapping(info->irq_domain, i * 8 + j));
		}
	}

out:
	ret = regmap_write(regmap, MT6720_REG_SPMI_TXDRV2, MT6720_MASK_RGS_RCS_INIT_DONE);
	if (ret)
		dev_info(info->dev, "%s, Failed to do rcs IRQ retrigger\n", __func__);

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
			dev_info(info->dev, "Failed to update irq %ld RG\n", d->hwirq);
	}

out:
	mutex_unlock(&info->irq_chip_lock);
}

static const struct irq_chip mt6720_irq_chip = {
	.name = "mt6720-spmi-irqs",
	.irq_bus_lock = mt6720_irq_bus_lock,
	.irq_bus_sync_unlock = mt6720_irq_bus_sync_unlock,
	.irq_enable = mt6720_irq_enable,
	.irq_disable = mt6720_irq_disable,
	.flags = IRQCHIP_SKIP_SET_WAKE,
};

static int mt6720_irq_domain_map(struct irq_domain *d, unsigned int virq, irq_hw_number_t hwirq)
{
	struct mt6720_info *info = d->host_data;

	irq_set_chip_data(virq, info);
	irq_set_chip(virq, &mt6720_irq_chip);
	irq_set_nested_thread(virq, true);
	irq_set_parent(virq, info->irq);
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
	u8 irqmask[MT6720_IRQNSPARSE_RGCNT] = { 0 };
	int ret;

	memset(irqmask, 0xff, MT6720_IRQEVT_RGCNT);
	mt6720_irqevt_unpadding(irqmask, MT6720_IRQNSPARSE_RGCNT);

	ret = regmap_raw_write(regmap, MT6720_REG_CHG_MASK0, irqmask, MT6720_IRQNSPARSE_RGCNT);
	ret |= regmap_raw_write(regmap, MT6720_REG_CHG_IRQ0, irqmask, MT6720_IRQNSPARSE_RGCNT);
	if (ret) {
		dev_info(dev, "Failed masknwrc evts\n");
		return ret;
	}

	mutex_init(&info->irq_chip_lock);

	info->irq_domain = irq_domain_create_linear(dev_fwnode(dev), MT6720_IRQEVT_RGCNT * 8,
						    &mt6720_irq_domain_ops, info);
	if (!info->irq_domain) {
		dev_info(dev, "Failed to add irq domain\n");
		return -EINVAL;
	}

	ret = devm_add_action_or_reset(dev, mt6720_irq_domain_release, info);
	if (ret) {
		dev_info(dev, "Failed to add irq domain release action\n");
		return ret;
	}

	ret = devm_request_threaded_irq(dev, info->irq, NULL, mt6720_irq_thread_handler,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT, dev_name(dev), info);
	if (ret) {
		dev_info(dev, "Failed to request MT6720 IRQ(irq:%d)\n", info->irq);
		return ret;
	}

	return 0;
}

static void mt6720_check_of_irq(struct mt6720_info *info)
{
	struct device_node *parent;
	int ret;

	info->bypass_retrigger = false;

	ret = device_property_read_u32(info->dev, "reg", &info->svid);
	if (ret) {
		dev_info(info->dev, "%s, Failed to get MT6379 SPMI slave id, use default value\n",
			 __func__);
		info->svid = MT6720_DEFAULT_SVID;
	}

	dev_info(info->dev, "%s, MT6379 SPMI slave id: 0x%02X\n", __func__, info->svid);

	parent = of_irq_find_parent(info->dev->of_node);
	if (parent) {
		if (of_property_read_bool(parent, "gpio-controller"))
			info->bypass_retrigger = true;

		of_node_put(parent);
	}

	dev_notice(info->dev, "%s, bypass_retrigger: %d\n", __func__, info->bypass_retrigger);
}

static int mt6720_probe(struct spmi_device *sdev)
{
	struct device *dev = &sdev->dev;
	struct mt6720_info *info;
	struct regmap *regmap;
	unsigned int ven_id;
	int irqno, ret;

	irqno = of_irq_get(dev->of_node, 0);
	if (irqno <= 0) {
		dev_info(dev, "Invalid irq number (%d)\n", irqno);
		return -EINVAL;
	}

	device_init_wakeup(dev, true);

	ret = dev_pm_set_wake_irq(dev, irqno);
	if (ret)
		dev_warn(dev, "Failed to set up wakeup irq\n");

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = dev;
	info->irq = irqno;
	memset(info->irqmask_buffer, 0xff, MT6720_IRQEVT_RGCNT);

	mt6720_check_of_irq(info);
	dev_set_drvdata(dev, info);

	regmap = devm_regmap_init(dev, &mt6720_regmap_bus, info, &mt6720_regmap_config);
	if (IS_ERR(regmap)) {
		dev_info(dev, "Failed to init regmap\n");
		return PTR_ERR(regmap);
	}

	/* If using RCS, should set RCS config */
	if (!info->bypass_retrigger) {
		ret = regmap_write(regmap, MT6720_REG_SPMI_RCS2, info->svid);
		if (ret)
			dev_info(dev, "%s, Failed to set rcs_addr\n", __func__);

		ret = regmap_write(regmap, MT6720_REG_SPMI_RCS1, 0x91);
		if (ret)
			dev_info(dev, "%s, Failed to enable MT6720 RCS\n", __func__);
	}

	ret = regmap_read(regmap, MT6720_REG_DEV_INFO, &ven_id);
	if (ret) {
		dev_info(dev, "Failed to read device information\n");
		return ret;
	}

	if ((ven_id & MT6720_VENID_MASK) != MT6720_VENDOR_ID) {
		dev_info(dev, "Incorrect vendor id (0x%02x)\n", ven_id);
		return -ENODEV;
	}

	ret = mt6720_register_interrupt(info);
	if (ret) {
		dev_info(dev, "Fialed to register interrupt\n");
		return ret;
	}

	return devm_of_platform_populate(dev);
}

static int mt6720_spmi_suspend(struct device *dev)
{
	struct mt6720_info *info = dev_get_drvdata(dev);

	disable_irq(info->irq);
	return 0;
}

static int mt6720_spmi_resume(struct device *dev)
{
	struct mt6720_info *info = dev_get_drvdata(dev);

	enable_irq(info->irq);
	return 0;
}

static const struct dev_pm_ops mt6720_spmi_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mt6720_spmi_suspend, mt6720_spmi_resume)
};

static const struct of_device_id mt6720_dt_match_table[] = {
	{ .compatible = "mediatek,mt6720" },
	{}
};
MODULE_DEVICE_TABLE(of, mt6720_dt_match_table);

static struct spmi_driver mt6720_driver = {
	.driver = {
		.name = "mt6720",
		.of_match_table = mt6720_dt_match_table,
		.pm = &mt6720_spmi_pm_ops,
	},
	.probe = mt6720_probe,
};
module_spmi_driver(mt6720_driver);

MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_AUTHOR("ChiaEn Wu <chiaen_wu@richtek.com>");
MODULE_DESCRIPTION("MediaTek MT6720 SubPMIC SPMI Driver");
MODULE_LICENSE("GPL");
