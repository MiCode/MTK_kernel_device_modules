// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
/*
 * Copyright (C) 2022 GoldenRiver Inc.
 */

// Note: Enable the following macro to show debug messages if CONFIG_DYNAMIC_DEBUG is not set
// #define DEBUG

#define pr_fmt(fmt) "mtk-eint-dummy: " fmt

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/idr.h>
#include <linux/io.h>
#include <linux/gpio/driver.h>

#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/spinlock.h>
#include <linux/virtio.h>
#include <linux/vringh.h>
#include <uapi/linux/virtio_ids.h>
#include <linux/virtio_ring.h>
#include <linux/dma-mapping.h>
#include <linux/virtio_config.h>
#include <linux/completion.h>
#include <linux/printk.h>
#include <linux/kthread.h>

#include <linux/atomic.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/vmalloc.h>
#include <uapi/linux/sched/types.h>

#include "virtio-eint.h"
#include "mtk-eint.h"

#if defined(CONFIG_EINT_UNITTEST)
#include "eint-unittest.h"
#endif

#define GIRQ_NO_TIMEOUT	    0xffffffff
#define GIRQ_TIMEOUT_DEFAULT 1000
#define GIRQ_MAX_BUF_SIZE    (1024 * 64)
#define VIRTIO_EINT_DEV "virtio_eint"
#define MAX_GIRQ_MSG_SIZE 64
#define RQ_TIMEOUT_MS 1000
enum EINT_DEBUG_LEVEL {
	EINT_DEBUG_LEVEL_NONE = 0,
	EINT_DEBUG_LEVEL_ERROR,
	EINT_DEBUG_LEVEL_INFO,
	EINT_DEBUG_LEVEL_DEBUG,
	EINT_DEBUG_LEVEL_VERBOSE,
};

#define MTK_EINT_EDGE_SENSITIVE           0
#define MTK_EINT_LEVEL_SENSITIVE          1
#define MTK_EINT_DBNC_SET_DBNC_BITS	  4
#define MTK_EINT_DBNC_RST_BIT		  (0x1 << 1)
#define MTK_EINT_DBNC_SET_EN		  (0x1 << 0)

#define MTK_EINT_NO_OFFSET		  0

static struct mtk_eint *global_eintc;
static unsigned int *debounce_time;

void mtk_eint_virtio_irq_handler(int hwvirq, int eventId)
{
	int virq, j;
	struct irq_desc *desc;

	virq = irq_find_mapping(global_eintc->domain, hwvirq);
	if (get_debug_level())
		pr_info("virtio-eint handle hwirq:%d virq:%d eventId:%d\n", hwvirq, virq, eventId);
	if (get_debug_gpio() == hwvirq && get_debug_drop()) {
		pr_info("force drop irq handler hwirq:%d virq:%d\n", hwvirq, virq);
		return;
	}
	generic_handle_irq_safe(virq);
	desc = irq_to_desc(virq);
	if (get_debug_level())
		for_each_online_cpu(j)
			pr_info("cpu:%d irqs:%d\n", j, per_cpu(desc->kstat_irqs->cnt, j));
}
EXPORT_SYMBOL_GPL(mtk_eint_virtio_irq_handler);

int get_virq_by_hwirq(int hwvirq)
{
	int virq;

	virq = irq_find_mapping(global_eintc->domain, hwvirq);
	return virq;
}
EXPORT_SYMBOL_GPL(get_virq_by_hwirq);

int ResretValue;

int getResretValue(void)
{
	return ResretValue;
}

static const struct mtk_eint_regs mtk_generic_eint_regs = {
	.stat      = 0x000,
	.ack       = 0x040,
	.mask      = 0x080,
	.mask_set  = 0x0c0,
	.mask_clr  = 0x100,
	.sens      = 0x140,
	.sens_set  = 0x180,
	.sens_clr  = 0x1c0,
	.soft      = 0x200,
	.soft_set  = 0x240,
	.soft_clr  = 0x280,
	.pol       = 0x300,
	.pol_set   = 0x340,
	.pol_clr   = 0x380,
	.dom_en    = 0x400,
	.dbnc_ctrl = 0x500,
	.dbnc_set  = 0x600,
	.dbnc_clr  = 0x700,
	.raw_stat  = 0xa00,
};

static void __iomem *mtk_eint_get_offset(struct mtk_eint *eint,
					 unsigned int eint_num,
					 unsigned int offset,
					 unsigned int *instance,
					 unsigned int *index)
{
	void __iomem *reg;

	if (eint_num >= eint->total_pin_number ||
	    !eint->pins[eint_num].enabled) {
		WARN_ON(1);
		return NULL;
	}

	*instance = eint->pins[eint_num].instance;
	*index = eint->pins[eint_num].index;
	reg = eint->instances[*instance].base + offset + (*index / 32 * 4);

	return reg;
}

static int mtk_eint_flip_edge(struct mtk_eint *eint, int eint_num)
{
	return 0;
}

static void mtk_eint_mask(struct irq_data *d)
{
	submit_cmd(d, EINT_OPS_SUBMIT_MASK, -1, 0);
}

static void mtk_eint_unmask(struct irq_data *d)
{
	submit_cmd(d, EINT_OPS_SUBMIT_UNMASK, -1, 0);
}

static void mtk_eint_ack(struct irq_data *d)
{
}

static int mtk_eint_set_type(struct irq_data *d, unsigned int type)
{
	struct mtk_eint *eint = irq_data_get_irq_chip_data(d);
	u32 mask;
	unsigned int instance, index;
	void __iomem *reg;

	if (((type & IRQ_TYPE_EDGE_BOTH) && (type & IRQ_TYPE_LEVEL_MASK)) ||
	    ((type & IRQ_TYPE_LEVEL_MASK) == IRQ_TYPE_LEVEL_MASK)) {
		dev_info(eint->dev,
			"Can't configure IRQ%d (EINT%lu) for type 0x%X\n",
			d->irq, d->hwirq, type);
		return -EINVAL;
	}

	if ((type & IRQ_TYPE_EDGE_BOTH) == IRQ_TYPE_EDGE_BOTH)
		eint->pins[d->hwirq].dual_edge = 1;
	else
		eint->pins[d->hwirq].dual_edge = 0;

	if (type & (IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_EDGE_FALLING))
		reg = mtk_eint_get_offset(eint, d->hwirq,
					  eint->comp->regs->pol_clr,
					  &instance, &index);
	else
		reg = mtk_eint_get_offset(eint, d->hwirq,
					  eint->comp->regs->pol_set,
					  &instance, &index);

	mask = BIT(index & 0x1f);

	if (type & (IRQ_TYPE_EDGE_RISING | IRQ_TYPE_EDGE_FALLING))
		reg = mtk_eint_get_offset(eint, d->hwirq,
					  eint->comp->regs->sens_clr,
					  &instance, &index);
	else
		reg = mtk_eint_get_offset(eint, d->hwirq,
					  eint->comp->regs->sens_set,
					  &instance, &index);

	if (!reg) {
		dev_info(eint->dev, "%s invalid eint_num %lu\n",
			__func__, d->hwirq);
		return 0;
	}

	mask = BIT(index & 0x1f);

	if (eint->pins[d->hwirq].dual_edge)
		mtk_eint_flip_edge(eint, d->hwirq);

	submit_cmd(d, EINT_OPS_SUBMIT_NORMAL, type, debounce_time[d->hwirq]);

	return 0;
}

static int mtk_eint_irq_set_wake(struct irq_data *d, unsigned int on)
{
	return 0;
}

static int mtk_eint_irq_request_resources(struct irq_data *d)
{
	struct mtk_eint *eint = irq_data_get_irq_chip_data(d);
	struct gpio_chip *gpio_c;
	unsigned int gpio_n;
	int err;

	err = eint->gpio_xlate->get_gpio_n(eint->pctl, d->hwirq,
					   &gpio_n, &gpio_c);
	if (err < 0) {
		dev_info(eint->dev, "Can not find pin\n");
		goto err_out;
	}
	pr_debug("eintgpio gpio_n:%d\n", gpio_n);

	err = gpiochip_lock_as_irq(gpio_c, gpio_n);
	if (err < 0) {
		dev_info(eint->dev, "unable to lock HW IRQ %lu for IRQ\n",
			irqd_to_hwirq(d));
		goto err_out;
	}

	err = eint->gpio_xlate->set_gpio_as_eint(eint->pctl, d->hwirq);
	if (err < 0) {
		dev_info(eint->dev, "Can not eint mode\n");
		goto err_out;
	}

	return 0;
err_out:
	return err;
}

static void mtk_eint_irq_release_resources(struct irq_data *d)
{
	struct mtk_eint *eint = irq_data_get_irq_chip_data(d);
	struct gpio_chip *gpio_c;
	unsigned int gpio_n;

	eint->gpio_xlate->get_gpio_n(eint->pctl, d->hwirq, &gpio_n,
				     &gpio_c);

	gpiochip_unlock_as_irq(gpio_c, gpio_n);
}

static int mtk_eint_irq_set_affinity(struct irq_data *d,
									const struct cpumask *mask,
									bool force)
{
	irq_data_update_effective_affinity(d, mask);
	return 0;
}

static struct irq_chip mtk_eint_irq_chip = {
	.name = "mtk-eint",
	.irq_disable = mtk_eint_mask,
	.irq_mask = mtk_eint_mask,
	.irq_unmask = mtk_eint_unmask,
	.irq_ack = mtk_eint_ack,
	.irq_set_type = mtk_eint_set_type,
	.irq_set_wake = mtk_eint_irq_set_wake,
	.irq_request_resources = mtk_eint_irq_request_resources,
	.irq_release_resources = mtk_eint_irq_release_resources,
	.irq_set_affinity = mtk_eint_irq_set_affinity,
};

static unsigned int mtk_eint_can_en_debounce(struct mtk_eint *eint,
					     unsigned int eint_num)
{
	unsigned int sens;
	unsigned int instance, index;
	void __iomem *reg = mtk_eint_get_offset(eint, eint_num,
						eint->comp->regs->sens,
						&instance, &index);
	unsigned int bit = BIT(index & 0x1f);

	if (!reg) {
		dev_info(eint->dev, "%s invalid eint_num %d\n",
			__func__, eint_num);
		return 0;
	}

	if (readl(reg) & bit)
		sens = MTK_EINT_LEVEL_SENSITIVE;
	else
		sens = MTK_EINT_EDGE_SENSITIVE;

	if (eint->pins[eint_num].debounce &&
	    sens != MTK_EINT_EDGE_SENSITIVE)
		return 1;
	else
		return 0;
}

int mtk_eint_set_debounce(struct mtk_eint *eint, unsigned long eint_num,
			  unsigned int debounce)
{
	if (!mtk_eint_can_en_debounce(eint, eint_num))
		return -EINVAL;
	debounce_time[eint_num] = debounce;
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_eint_set_debounce);

unsigned int mtk_eint_get_debounce_en(struct mtk_eint *eint,
				      unsigned int eint_num)
{
	unsigned int instance, index, bit;
	void __iomem *reg;

	reg = mtk_eint_get_offset(eint, eint_num, MTK_EINT_NO_OFFSET,
				  &instance, &index);

	if (!reg) {
		dev_info(eint->dev, "%s invalid eint_num %d\n",
			__func__, eint_num);
		return 0;
	}

	reg = eint->instances[instance].base +
		(index / 4) * 4 + eint->comp->regs->dbnc_ctrl;

	bit = MTK_EINT_DBNC_SET_EN << ((index % 4) * 8);

	return (readl(reg) & bit) ? 1 : 0;
}

unsigned int mtk_eint_get_debounce_value(struct mtk_eint *eint,
					   unsigned int eint_num)
{
	unsigned int instance, index, mask, offset;
	void __iomem *reg;

	reg = mtk_eint_get_offset(eint, eint_num, MTK_EINT_NO_OFFSET,
				  &instance, &index);

	if (!reg) {
		dev_info(eint->dev, "%s invalid eint_num %d\n",
			__func__, eint_num);
		return 0;
	}

	reg = eint->instances[instance].base +
		(index / 4) * 4 + eint->comp->regs->dbnc_ctrl;

	offset = MTK_EINT_DBNC_SET_DBNC_BITS + ((index % 4) * 8);
	mask = 0xf << offset;

	return ((readl(reg) & mask) >> offset);
}

int mtk_eint_find_irq(struct mtk_eint *eint, unsigned long eint_n)
{
	int irq;

	irq = irq_find_mapping(eint->domain, eint_n);
	if (!irq)
		return -EINVAL;

	return irq;
}
EXPORT_SYMBOL_GPL(mtk_eint_find_irq);

/*
 * Dump the properties/states of the specific EINT pin.
 * @eint_num: the global EINT number.
 * @buf: the pointer of a string buffer.
 * @buf_size: the size of the buffer.
 *
 * If the return value < 0, it means that the @eint_num is invalid;
 * Otherwise, return 0;
 */

int dump_eint_pin_status(unsigned int eint_num, char *buf, unsigned int buf_size)
{
	return 0;
}
EXPORT_SYMBOL_GPL(dump_eint_pin_status);

int mtk_eint_do_suspend(struct mtk_eint *eint)
{
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_eint_do_suspend);

int mtk_eint_do_resume(struct mtk_eint *eint)
{
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_eint_do_resume);

static const struct mtk_eint_compatible default_compat = {
	.regs = &mtk_generic_eint_regs,
};

static const struct of_device_id eint_compatible_ids[] = {
	{ }
};

int mtk_eint_do_init(struct mtk_eint *eint)
{
	int i, matrix_number = 0;
	struct device_node *node;
	unsigned int ret, size, offset;
	unsigned int id, inst, idx, support_deb;

	const phandle *ph;

#if defined(MTK_EINT_DEBUG)
	struct mtk_eint_pin pin;
#endif
	if (!get_virtio_eint_ready()) {
		pr_info("virtio eint do init probe defer\n");
		return -EPROBE_DEFER;
	}
	pr_info("virtio eint do init +\n");
	ph = of_get_property(eint->dev->of_node, "mediatek,eint", NULL);
	if (!ph) {
		dev_info(eint->dev, "Cannot find EINT phandle in PIO node.\n");
		return -ENODEV;
	}

	node = of_find_node_by_phandle(be32_to_cpup(ph));
	if (!node) {
		dev_info(eint->dev, "Cannot find EINT node by phandle.\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(node, "mediatek,total-pin-number",
				   &eint->total_pin_number);
	if (ret) {
		dev_info(eint->dev,
		       "%s cannot read total-pin-number from device node.\n",
		       __func__);
		return -EINVAL;
	}

	dev_dbg(eint->dev, "%s virtio eint total %u pins.\n", __func__,
		eint->total_pin_number);

	ret = of_property_read_u32(node, "mediatek,instance-num",
				   &eint->instance_number);
	if (ret)
		eint->instance_number = 1; // only 1 instance in legacy chip

	size = eint->instance_number * sizeof(struct mtk_eint_instance);
	eint->instances = devm_kzalloc(eint->dev, size, GFP_KERNEL);
	if (!eint->instances)
		return -ENOMEM;

	size = eint->total_pin_number * sizeof(struct mtk_eint_pin);
	eint->pins = devm_kzalloc(eint->dev, size, GFP_KERNEL);
	if (!eint->pins)
		return -ENOMEM;

	for (i = 0; i < eint->instance_number; i++) {
		ret = of_property_read_string_index(node, "reg-name", i,
						    &(eint->instances[i].name));
		if (ret) {
			dev_info(eint->dev,
				 "%s cannot read the name of instance %d.\n",
				 __func__, i);
		}

		eint->instances[i].base = of_iomap(node, i);
		if (!eint->instances[i].base)
			return -ENOMEM;
	}

	matrix_number = of_property_count_u32_elems(node, "mediatek,pins") / 4;
	if (matrix_number < 0) {
		matrix_number = eint->total_pin_number;
		dev_info(eint->dev, "%s virtio eint in legacy mode, assign the matrix number to %u.\n",
			 __func__, matrix_number);
	} else
		dev_dbg(eint->dev, "%s virtio eint in new mode, assign the matrix number to %u.\n",
			 __func__, matrix_number);

	for (i = 0; i < matrix_number; i++) {
		offset = i * 4;

		ret = of_property_read_u32_index(node, "mediatek,pins",
					   offset, &id);
		ret |= of_property_read_u32_index(node, "mediatek,pins",
					   offset+1, &inst);
		ret |= of_property_read_u32_index(node, "mediatek,pins",
					   offset+2, &idx);
		ret |= of_property_read_u32_index(node, "mediatek,pins",
					   offset+3, &support_deb);

		/* Legacy chip which no need to give coordinate list */
		if (ret) {
			id = i;
			inst = 0;
			idx = i;
			support_deb = (i < 32) ? 1 : 0;
		}

		eint->pins[id].enabled = true;
		eint->pins[id].instance = inst;
		eint->pins[id].index = idx;
		eint->pins[id].debounce = support_deb;

		eint->instances[inst].pin_list[idx] = id;
		eint->instances[inst].number++;

#if defined(MTK_EINT_DEBUG)
		pin = eint->pins[id];
		dev_info(eint->dev,
			 "EINT%u in (%u-%u, %u), deb = %u. %u",
			 id,
			 pin.instance,
			 eint->instances[inst].number,
			 pin.index,
			 pin.debounce,
			 eint->instances[pin.instance].pin_list[pin.index]);
#endif
	}

	for (i = 0; i < eint->instance_number; i++) {
		size = (eint->instances[i].number / 32 + 1) * sizeof(unsigned int);
		eint->instances[i].wake_mask =
			devm_kzalloc(eint->dev, size, GFP_KERNEL);
		eint->instances[i].cur_mask =
			devm_kzalloc(eint->dev, size, GFP_KERNEL);

		if (!eint->instances[i].wake_mask ||
		    !eint->instances[i].cur_mask)
			return -ENOMEM;
	}

	eint->comp = &default_compat;

	eint->irq = irq_of_parse_and_map(node, 0);
	if (!eint->irq) {
		dev_info(eint->dev,
			"%s IRQ parse fail.\n", __func__);
		return -EINVAL;
	}

	eint->domain = irq_domain_add_linear(eint->dev->of_node,
					     eint->total_pin_number,
					     &irq_domain_simple_ops, NULL);
	if (!eint->domain)
		return -ENOMEM;

	for (i = 0; i < eint->total_pin_number; i++) {
		unsigned int virq = irq_create_mapping(eint->domain, i);

		if (virq == 0) {
			pr_info("%s: Failed to create IRQ mapping for pin %d\n",
				__func__, i);
			continue;
		}

		irq_set_chip_and_handler(virq, &mtk_eint_irq_chip,
					 handle_level_irq);
		irq_set_chip_data(virq, eint);
	}

	global_eintc = eint;
	debounce_time = kcalloc(eint->total_pin_number, sizeof(int), GFP_KERNEL);

	set_gpio_count(eint->total_pin_number);
	register_handler_cb(mtk_eint_virtio_irq_handler);
	register_findirq_cb(get_virq_by_hwirq);
	pr_info("virtio eint do init -\n");
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_eint_do_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Dummy eint device driver");
MODULE_AUTHOR("Gerd Hoffmann <kraxel@redhat.com>");

