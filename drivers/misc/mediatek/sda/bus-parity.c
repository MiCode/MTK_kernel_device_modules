// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <asm/cputype.h>
#include <linux/arm-smccc.h>
#include <linux/atomic.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <mt-plat/aee.h>
#include "sda.h"
#include "dbg_error_flag.h"

#define MCU_BP_IRQ_TRIGGER_THRESHOLD	(2)
#define INFRA_BP_IRQ_TRIGGER_THRESHOLD	(2)

#define DBG_ERR_FLAG_STATUS0 0x88
#define DBG_ERR_FLAG_STATUS1 0x8c

#define BUS_TRACER_COMPATIBLE "mediatek,bus_tracer-v1"
#define DBG_ERR_FLAG_COMPATIBLE "mediatek, soc-dbg-error-flag"

/*
 * The base address of dbgao is placed in bus_tracer node
 * temporarily, and will be moved out latter.
 */

static struct device_node *err_flag_node;

union bus_parity_err {
	struct _mst {
		unsigned int parity_data;
		unsigned int rid;
		unsigned int rdata[4];
		bool is_err;
	} mst;
	struct _slv {
		unsigned int parity_data;
		unsigned int wid;
		unsigned int wdata[4];
		unsigned int arid;
		unsigned int araddr[2];
		unsigned int awid;
		unsigned int awaddr[2];
		bool is_err;
	} slv;
};

struct bus_parity_elem {
	const char *name;
	void __iomem *base;
	unsigned int type;
	unsigned int data_len;
	unsigned int rd0_wd0_offset;
	unsigned int fail_bit_shift;
	union bus_parity_err bpr;
};

struct bus_parity {
	struct bus_parity_elem *bpm;
	unsigned int nr_bpm;
	unsigned int nr_err;
	unsigned long long ts;
	struct work_struct wk;
	void __iomem *parity_sta;
	void __iomem *dbgao_base;
	unsigned int irq;
	char *dump;
};
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define BPR_LOG(fmt, ...) \
	do { \
		pr_notice(fmt, __VA_ARGS__); \
		aee_sram_printk(fmt, __VA_ARGS__); \
	} while (0)
#else
#define BPR_LOG(fmt, ...)
#endif

static struct bus_parity mcu_bp, infra_bp;
static DEFINE_SPINLOCK(mcu_bp_isr_lock);
static DEFINE_SPINLOCK(infra_bp_isr_lock);

static ssize_t bus_status_show(struct device_driver *driver, char *buf)
{
	int n;

	n = 0;

	if (mcu_bp.nr_err || infra_bp.nr_err) {
		n += snprintf(buf + n, PAGE_SIZE - n, "True\n");

		n += snprintf(buf + n, PAGE_SIZE - n,
			"MCU Bus Parity: %u times (1st timestamp: %llu ns)\n",
			mcu_bp.nr_err, mcu_bp.ts);

		n += snprintf(buf + n, PAGE_SIZE - n,
			"Infra Bus Parity: %u times (1st timestamp: %llu ns)\n",
			infra_bp.nr_err, infra_bp.ts);

		return n;
	} else
		return snprintf(buf, PAGE_SIZE, "False\n");
}

static DRIVER_ATTR_RO(bus_status);

static void mcu_bp_irq_work(struct work_struct *w)
{
	struct bus_parity_elem *bpm;
	union bus_parity_err *bpr;
	int i;

	for (i = 0; i < mcu_bp.nr_bpm; i++) {
		bpm = &mcu_bp.bpm[i];
		bpr = &mcu_bp.bpm[i].bpr;

		if (!bpm->type && (bpr->mst.is_err == true))
			bpr->mst.is_err = false;
		else if (bpm->type && (bpr->slv.is_err == true))
			bpr->slv.is_err = false;
		else
			continue;
	}
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	aee_kernel_exception("MCU Bus Parity", mcu_bp.dump);
#endif
	if (mcu_bp.nr_err < MCU_BP_IRQ_TRIGGER_THRESHOLD)
		enable_irq(mcu_bp.irq);
	else
		BPR_LOG("%s disable irq %d due to trigger over than %d times.\n",
			__func__, mcu_bp.irq, MCU_BP_IRQ_TRIGGER_THRESHOLD);
}

static void mcu_bp_dump(void)
{
	struct bus_parity_elem *bpm;
	union bus_parity_err *bpr;
	int n, i, j;

	if (!mcu_bp.dump)
		return;

	n = 0;

	n += snprintf(mcu_bp.dump + n, PAGE_SIZE - n, "mcu_bp err:\n");
	if (n > PAGE_SIZE)
		return;

	for (i = 0; i < mcu_bp.nr_bpm; i++) {
		bpm = &mcu_bp.bpm[i];
		bpr = &mcu_bp.bpm[i].bpr;

		if (!bpm->type && (bpr->mst.is_err == true)) {
			n += snprintf(mcu_bp.dump + n, PAGE_SIZE - n,
				"%s,pd:0x%x,rid:0x%x,rdata:0x%x",
				bpm->name, bpr->mst.parity_data,
				bpr->mst.rid, bpr->mst.rdata[0]);

			if (n > PAGE_SIZE)
				return;

			for (j = 1; j < bpm->data_len; j++) {
				n += snprintf(mcu_bp.dump + n, PAGE_SIZE - n,
					"/0x%x", bpr->mst.rdata[j]);

				if (n > PAGE_SIZE)
					return;
			}

			n += snprintf(mcu_bp.dump + n, PAGE_SIZE - n, "\n");

			if (n > PAGE_SIZE)
				return;
		} else if (bpm->type && (bpr->slv.is_err == true)) {
			n += snprintf(mcu_bp.dump + n, PAGE_SIZE - n,
				"%s,pd:0x%x,awid:0x%x,awaddr:0x%x/0x%x,",
				bpm->name, bpr->slv.parity_data, bpr->slv.awid,
				bpr->slv.awaddr[0], bpr->slv.awaddr[1]);

			if (n > PAGE_SIZE)
				return;

			n += snprintf(mcu_bp.dump + n, PAGE_SIZE - n,
				"arid:0x%x,araddr:0x%x/0x%x,",
				bpr->slv.arid, bpr->slv.araddr[0],
				bpr->slv.araddr[1]);

			if (n > PAGE_SIZE)
				return;

			n += snprintf(mcu_bp.dump + n, PAGE_SIZE - n,
				"wid:0x%x,wdata:0x%x",
				bpr->slv.wid, bpr->slv.wdata[0]);

			if (n > PAGE_SIZE)
				return;

			for (j = 1; j < bpm->data_len; j++) {
				n += snprintf(mcu_bp.dump + n, PAGE_SIZE - n,
					"/0x%x", bpr->slv.wdata[j]);

				if (n > PAGE_SIZE)
					return;
			}

			n += snprintf(mcu_bp.dump + n, PAGE_SIZE - n, "\n");

			if (n > PAGE_SIZE)
				return;
		} else
			continue;
	}
}

static void infra_bp_dump(void)
{
	struct bus_parity_elem *bpm;
	union bus_parity_err *bpr;
	int n, i;

	if (!infra_bp.dump)
		return;

	n = 0;

	n += snprintf(infra_bp.dump + n, PAGE_SIZE - n, "infra_bp err:\n");
	if (n > PAGE_SIZE)
		return;

	for (i = 0; i < infra_bp.nr_bpm; i++) {
		bpm = &infra_bp.bpm[i];
		bpr = &infra_bp.bpm[i].bpr;

		if (!bpm->type && (bpr->mst.is_err == true)) {
			n += snprintf(infra_bp.dump + n, PAGE_SIZE - n,
				"%s,pd:0x%x,rid:0x%x\n",
				bpm->name, bpr->mst.parity_data,
				bpr->mst.rid);

			if (n > PAGE_SIZE)
				return;
		} else if (bpm->type && (bpr->slv.is_err == true)) {
			n += snprintf(infra_bp.dump + n, PAGE_SIZE - n,
				"%s,pd:0x%x,awid:0x%x,awaddr:0x%x/0x%x,",
				bpm->name, bpr->slv.parity_data, bpr->slv.awid,
				bpr->slv.awaddr[0], bpr->slv.awaddr[1]);

			if (n > PAGE_SIZE)
				return;

			n += snprintf(infra_bp.dump + n, PAGE_SIZE - n,
				"arid:0x%x,araddr:0x%x/0x%x,wid:0x%x\n",
				bpr->slv.arid, bpr->slv.araddr[0],
				bpr->slv.araddr[1], bpr->slv.wid);

			if (n > PAGE_SIZE)
				return;
		} else
			continue;
	}
}

static irqreturn_t mcu_bp_isr(int irq, void *dev_id)
{
	int i, j;
	unsigned int status;
	struct bus_parity_elem *bpm;
	union bus_parity_err *bpr;
	struct arm_smccc_res res;

	disable_irq_nosync(irq);

	if (!mcu_bp.nr_err)
		mcu_bp.ts = local_clock();
	mcu_bp.nr_err++;

	status = readl(mcu_bp.parity_sta);
	for (i = 0; i < mcu_bp.nr_bpm; i++) {
		if (status & (0x1<<mcu_bp.bpm[i].fail_bit_shift)) {
			bpm = &mcu_bp.bpm[i];
			bpr = &mcu_bp.bpm[i].bpr;

			if (!bpm->type) {
				bpr->mst.is_err = true;
				bpr->mst.parity_data = readl(bpm->base+0x4);
				bpr->mst.rid = readl(bpm->base+0x8);
				for (j = 0; j < bpm->data_len; j++)
					bpr->mst.rdata[j] =
						readl(bpm->base+bpm->rd0_wd0_offset+(j<<2));
			} else {
				bpr->slv.is_err = true;
				bpr->slv.parity_data = readl(bpm->base+0x4);
				bpr->slv.awid = readl(bpm->base+0x8);
				bpr->slv.arid = readl(bpm->base+0xC);
				bpr->slv.awaddr[0] = readl(bpm->base+0x10);
				bpr->slv.awaddr[1] = readl(bpm->base+0x14);
				bpr->slv.araddr[0] = readl(bpm->base+0x18);
				bpr->slv.araddr[1] = readl(bpm->base+0x1C);
				bpr->slv.wid = readl(bpm->base+0x20);
				for (j = 0; j < bpm->data_len; j++)
					bpr->slv.wdata[j] =
						readl(bpm->base+bpm->rd0_wd0_offset+(j<<2));
			}
		} else
			continue;
	}

	schedule_work(&mcu_bp.wk);

	spin_lock(&mcu_bp_isr_lock);

	arm_smccc_smc(MTK_SIP_SDA_CONTROL, SDA_BUS_PARITY, BP_MCU_CLR, status,
			0, 0, 0, 0, &res);

	if (res.a0)
		pr_notice("%s: can't clear mcu bus pariity(0x%lx)\n",
				__func__, res.a0);

	spin_unlock(&mcu_bp_isr_lock);

	mcu_bp_dump();
	BPR_LOG("%s", mcu_bp.dump);

	return IRQ_HANDLED;
}

static void infra_bp_dump_flow(void)
{
	int i;
	unsigned int status;
	unsigned int check_count = 0;
	struct bus_parity_elem *bpm;
	union bus_parity_err *bpr;

	/* infra parity dump */
	if (!infra_bp.nr_err)
		infra_bp.ts = local_clock();
	infra_bp.nr_err++;

	for (i = 0; i < infra_bp.nr_bpm; i++) {
		status = readl(infra_bp.bpm[i].base);
		if (status & (0x1<<31)) {
			bpm = &infra_bp.bpm[i];
			bpr = &infra_bp.bpm[i].bpr;

			if (!bpm->type) { /* infra master */
				bpr->mst.is_err = true;
				bpr->mst.parity_data = (status << 1) >> 16;
				bpr->mst.rid = readl(bpm->base+0x4);
			} else if (bpm->type == 1) { /* infra slave */
				bpr->slv.is_err = true;
				bpr->slv.parity_data = (status << 1) >> 10;
				bpr->slv.awaddr[0] = readl(bpm->base+0x4);
				status = readl(bpm->base+0x8);
				bpr->slv.awaddr[1] = (status << 27) >> 27;
				bpr->slv.awid = (status << 14) >> 19;
				bpr->slv.wid = (status << 1) >> 19;
				bpr->slv.araddr[0] = readl(bpm->base+0xC);
				status = readl(bpm->base+0x10);
				bpr->slv.araddr[1] = (status << 27) >> 27;
				bpr->slv.arid = (status << 14) >> 19;
			} else if (bpm->type == 2) { /* emi slave */
				bpr->slv.is_err = true;
				bpr->slv.parity_data = (status << 1) >> 4;
				bpr->slv.araddr[0] = readl(bpm->base+0x4);
				bpr->slv.arid = readl(bpm->base+0x8);
				bpr->slv.araddr[1] = readl(bpm->base+0xC);
				bpr->slv.wid = readl(bpm->base+0x10);
				bpr->slv.awaddr[0] = readl(bpm->base+0x14);
				bpr->slv.awid = readl(bpm->base+0x18);
				bpr->slv.awaddr[1] = readl(bpm->base+0x1C);
			}
		} else {
			check_count++;
			continue;
		}
	}

	spin_lock(&infra_bp_isr_lock);

	for (i = 0; i < infra_bp.nr_bpm; i++) {
		status = readl(infra_bp.bpm[i].base);
		writel((status|(0x1<<2)), infra_bp.bpm[i].base);
		dsb(sy);
		writel(status, infra_bp.bpm[i].base);
		dsb(sy);
	}

	spin_unlock(&infra_bp_isr_lock);

	infra_bp_dump();
	BPR_LOG("%s", infra_bp.dump);
}

static int infra_bp_dump_event(struct notifier_block *this,
				unsigned long err_flag_status,
				void *ptr)
{
	unsigned long infra_bp_err_status;

	infra_bp_err_status = get_dbg_error_flag_mask(MCU2SUB_EMI_M1_PARITY) |
				get_dbg_error_flag_mask(MCU2SUB_EMI_M0_PARITY) |
				get_dbg_error_flag_mask(MCU2EMI_M1_PARITY) |
				get_dbg_error_flag_mask(MCU2EMI_M0_PARITY) |
				get_dbg_error_flag_mask(MCU2INFRA_REG_PARITY) |
				get_dbg_error_flag_mask(INFRA_L3_CACHE2MCU_PARITY) |
				get_dbg_error_flag_mask(EMI_PARITY_CEN);


	if (!(err_flag_status & infra_bp_err_status)) {
		BPR_LOG("err_flag_status %lx, infra_bp_err_status %lx\n",
			err_flag_status,
			infra_bp_err_status);
		return 0;
	}

	/* infra parity dump */
	infra_bp_dump_flow();

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	aee_kernel_exception("INFRA Bus Parity", infra_bp.dump);
#endif

	return 0;
}

static struct notifier_block dbg_error_flag_notifier = {
	.notifier_call = infra_bp_dump_event,
};

static int bus_parity_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *np_chosen;
	struct tag_chipid *chipid;
	size_t size;
	int ret, i;

	dev_info(dev, "driver probed\n");

	mcu_bp.nr_err = 0;
	infra_bp.nr_err = 0;

	INIT_WORK(&mcu_bp.wk, mcu_bp_irq_work);

	ret = of_property_count_strings(np, "mcu-names");
	if (ret < 0) {
		dev_err(dev, "can't count mcu-names(%d)\n", ret);
		return ret;
	}
	mcu_bp.nr_bpm = ret;

	ret = of_property_count_strings(np, "infra-names");
	if (ret < 0) {
		dev_err(dev, "can't count infra-names(%d)\n", ret);
		return ret;
	}
	infra_bp.nr_bpm = ret;

	dev_info(dev, "%s=%d, %s=%d\n", "nr_mcu_bpm", mcu_bp.nr_bpm,
			"nr_infra_bpm", infra_bp.nr_bpm);

	size = sizeof(struct bus_parity_elem) * mcu_bp.nr_bpm;
	mcu_bp.bpm = devm_kzalloc(dev, size, GFP_KERNEL);
	if (!mcu_bp.bpm)
		return -ENOMEM;

	size = sizeof(struct bus_parity_elem) * infra_bp.nr_bpm;
	infra_bp.bpm = devm_kzalloc(dev, size, GFP_KERNEL);
	if (!infra_bp.bpm)
		return -ENOMEM;

	for (i = 0; i < mcu_bp.nr_bpm; i++) {
		ret = of_property_read_string_index(np, "mcu-names", i,
				&mcu_bp.bpm[i].name);
		if (ret) {
			dev_err(dev, "can't read mcu-names(%d)\n", ret);
			return ret;
		}
	}

	for (i = 0; i < infra_bp.nr_bpm; i++) {
		ret = of_property_read_string_index(np, "infra-names", i,
				&infra_bp.bpm[i].name);
		if (ret) {
			dev_err(dev, "can't read infra-names(%d)\n", ret);
			return ret;
		}
	}

	for (i = 0; i < mcu_bp.nr_bpm; i++) {
		mcu_bp.bpm[i].base = of_iomap(np, i);
		if (!mcu_bp.bpm[i].base) {
			dev_err(dev, "can't map mcu_bp(%d)\n", i);
			return -ENXIO;
		}
	}

	for (i = 0; i < infra_bp.nr_bpm; i++) {
		infra_bp.bpm[i].base = of_iomap(np, mcu_bp.nr_bpm + i);
		if (!infra_bp.bpm[i].base) {
			dev_err(dev, "can't map infra_bp(%d)\n", i);
			return -ENXIO;
		}
	}

	mcu_bp.parity_sta = of_iomap(np, mcu_bp.nr_bpm + infra_bp.nr_bpm);
	if (!mcu_bp.parity_sta) {
		dev_err(dev, "can't map mcu_bp status\n");
		return -ENXIO;
	}

	for (i = 0; i < mcu_bp.nr_bpm; i++) {
		ret = of_property_read_u32_index(np, "mcu-types", i,
				&mcu_bp.bpm[i].type);
		if (ret) {
			dev_err(dev, "can't read mcu-types(%d)\n", ret);
			return ret;
		}
	}

	for (i = 0; i < infra_bp.nr_bpm; i++) {
		ret = of_property_read_u32_index(np, "infra-types", i,
				&infra_bp.bpm[i].type);
		if (ret) {
			dev_err(dev, "can't read infra-types(%d)\n", ret);
			return ret;
		}
	}

	for (i = 0; i < mcu_bp.nr_bpm; i++) {
		ret = of_property_read_u32_index(np, "mcu-rd0wd0-offset", i,
				&mcu_bp.bpm[i].rd0_wd0_offset);
		if (ret) {
			dev_notice(dev, "can't read mcu-rd0-offset(%d)\n", ret);
			mcu_bp.bpm[i].rd0_wd0_offset = 0x10;
		}
	}

	for (i = 0; i < mcu_bp.nr_bpm; i++) {
		ret = of_property_read_u32_index(np, "mcu-fail-bit-shift", i,
		&mcu_bp.bpm[i].fail_bit_shift);
		if (ret) {
			dev_notice(dev, "can't read mcu-fail-bit-shift(%d)\n", ret);
			mcu_bp.bpm[i].fail_bit_shift = i;
		}
	}

	for (i = 0; i < mcu_bp.nr_bpm; i++) {
		ret = of_property_read_u32_index(np, "mcu-data-len", i,
				&mcu_bp.bpm[i].data_len);
		if (ret) {
			dev_notice(dev, "can't read mcu-data-len(%d)\n", ret);
			return ret;
		}
	}

	/* find the bus_tracer node from dts */
	err_flag_node = of_find_compatible_node(NULL, NULL, BUS_TRACER_COMPATIBLE);
	if (err_flag_node == NULL) {
		dev_info(dev, "can't find node '%s' from dts.\n", BUS_TRACER_COMPATIBLE);
		err_flag_node = of_find_compatible_node(NULL, NULL, DBG_ERR_FLAG_COMPATIBLE);
		if (err_flag_node == NULL) {
			dev_info(dev, "can't find node '%s' from dts.\n", DBG_ERR_FLAG_COMPATIBLE);
			return -EINVAL;
		}
		dev_info(dev, "find node '%s' from dts.\n", DBG_ERR_FLAG_COMPATIBLE);
		/* get the base address for error flag from bus_tracer node. */
		infra_bp.dbgao_base = of_iomap(err_flag_node, 0);
	} else {
		/* get the base address for error flag from bus_tracer node. */
		infra_bp.dbgao_base = of_iomap(err_flag_node, 1);
	}

	mcu_bp.dump = devm_kzalloc(dev, PAGE_SIZE, GFP_KERNEL);
	if (!mcu_bp.dump)
		return -ENOMEM;

	infra_bp.dump = devm_kzalloc(dev, PAGE_SIZE, GFP_KERNEL);
	if (!infra_bp.dump)
		return -ENOMEM;

	mcu_bp.irq = irq_of_parse_and_map(np, 0);
	if (!mcu_bp.irq) {
		dev_err(dev, "can't map mcu-bus-parity irq\n");
		return -EINVAL;
	}

	ret = devm_request_irq(dev, mcu_bp.irq, mcu_bp_isr, IRQF_ONESHOT |
			IRQF_TRIGGER_NONE, "mcu-bus-parity", NULL);
	if (ret) {
		dev_err(dev, "can't request mcu-bus-parity irq(%d)\n", ret);
		return ret;
	}

	/* register error flag notifier to dump infra parity status for error flag mcu irq */
	dbg_error_flag_register_notify(&dbg_error_flag_notifier);

	np_chosen = of_find_node_by_path("/chosen");
	if (!np_chosen)
		np_chosen = of_find_node_by_path("/chosen@0");

	if (np_chosen) {
		chipid = (struct tag_chipid *) of_get_property(np_chosen,
				"atag,chipid", NULL);
		if (!chipid)
			return 0;

		pr_info("get chipid 0x%x:0x%x:0x%x:0x%x.\n",
			chipid->hw_code, chipid->hw_subcode, chipid->hw_ver, chipid->sw_ver);

		/* XXX for SLV_L3GIC check */
		if (chipid->hw_code == 0x1229 && chipid->hw_subcode == 0x8a00
			&& chipid->hw_ver == 0xca00 && chipid->sw_ver == 0x0000) {
			mcu_bp.nr_bpm = mcu_bp.nr_bpm - 1;
		}
	}

	return 0;
}

static int bus_parity_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "driver removed\n");

	flush_work(&mcu_bp.wk);
	flush_work(&infra_bp.wk);

	return 0;
}

static const struct of_device_id bus_parity_of_ids[] = {
	{ .compatible = "mediatek,bus-parity", },
	{ .compatible = "mediatek,mt6885-bus-parity", },
	{}
};

static struct platform_driver bus_parity_drv = {
	.driver = {
		.name = "bus_parity",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = bus_parity_of_ids,
	},
	.probe = bus_parity_probe,
	.remove = bus_parity_remove,
};

static int __init bus_parity_init(void)
{
	int ret;

	ret = platform_driver_register(&bus_parity_drv);
	if (ret)
		return ret;

	ret = driver_create_file(&bus_parity_drv.driver,
				 &driver_attr_bus_status);
	if (ret)
		return ret;

	return 0;
}

static __exit void bus_parity_exit(void)
{
	driver_remove_file(&bus_parity_drv.driver,
			 &driver_attr_bus_status);

	platform_driver_unregister(&bus_parity_drv);
}

module_init(bus_parity_init);
module_exit(bus_parity_exit);

MODULE_DESCRIPTION("MediaTek Bus Parity Driver");
MODULE_LICENSE("GPL v2");
