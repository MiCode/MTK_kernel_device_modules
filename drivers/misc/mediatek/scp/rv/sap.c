// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#define pr_fmt(fmt) "sap " fmt

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/io.h>
#include <linux/soc/mediatek/mtk-mbox.h>
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include <linux/vmalloc.h>
#include "scp_helper.h"
#include "sap.h"

enum {
	REG_WDT_IRQ,
	REG_GPR5,
	REG_STATUS,
	REG_PC,
	REG_LR,
	REG_SP,
	REG_PC_LATCH,
	REG_LR_LATCH,
	REG_SP_LATCH,
	MAX_DTS_DEFINE_REGS,
};

struct struct_reg {
	bool valid;
	const char *name;
	void __iomem * addr;
	uint32_t size;
};

struct sap_status_reg {
	uint32_t status;
	uint32_t pc;
	uint32_t lr;
	uint32_t sp;
	uint32_t pc_latch;
	uint32_t lr_latch;
	uint32_t sp_latch;
};

struct sap_device {
	bool enable;
	bool pll_clock_support;
	bool reg_parse_from_dts;

	uint8_t core_id;
	void __iomem *reg_cfg;
	struct mtk_mbox_device mbox_dev;
	struct sap_status_reg status_reg;
	struct struct_reg *reg_list;

	bool dump_ois_pin_status;
	int ois_avdd_pin;
	int ois_vdd_pin;
	int ois_scl_pin;
	int ois_sda_pin;
};

#define INIT_STRUCT_REG(n)	\
	{ .valid = false, .name = #n, .addr = NULL, .size = 0 }
static struct struct_reg sap_reg_list[MAX_DTS_DEFINE_REGS] = {
	INIT_STRUCT_REG(sap_wdt_irq),
	INIT_STRUCT_REG(sap_gpr5),
	INIT_STRUCT_REG(sap_status),
	INIT_STRUCT_REG(mon_pc),
	INIT_STRUCT_REG(mon_lr),
	INIT_STRUCT_REG(mon_sp),
	INIT_STRUCT_REG(mon_pc_latch),
	INIT_STRUCT_REG(mon_lr_latch),
	INIT_STRUCT_REG(mon_sp_latch),
};

static struct sap_device sap_dev;
struct mtk_ipi_device sap_ipidev;
EXPORT_SYMBOL(sap_ipidev);

static void sap_dump_last_regs_v2(struct struct_reg *reg_list)
{
	struct sap_status_reg *reg = &sap_dev.status_reg;

	if (READ_ONCE(reg_list[REG_STATUS].valid))
		reg->status = readl(reg_list[REG_STATUS].addr);

	if (READ_ONCE(reg_list[REG_PC].valid))
		reg->pc = readl(reg_list[REG_PC].addr);

	if (READ_ONCE(reg_list[REG_LR].valid))
		reg->lr = readl(reg_list[REG_LR].addr);

	if (READ_ONCE(reg_list[REG_SP].valid))
		reg->sp = readl(reg_list[REG_SP].addr);

	if (READ_ONCE(reg_list[REG_PC_LATCH].valid))
		reg->pc_latch  = readl(reg_list[REG_PC_LATCH].addr);

	if (READ_ONCE(reg_list[REG_LR_LATCH].valid))
		reg->lr_latch  = readl(reg_list[REG_LR_LATCH].addr);

	if (READ_ONCE(reg_list[REG_SP_LATCH].valid))
		reg->sp_latch  = readl(reg_list[REG_SP_LATCH].addr);
}

void sap_dump_last_regs(void)
{
	struct sap_status_reg *reg = &sap_dev.status_reg;
	void __iomem *cfg_base = sap_dev.reg_cfg;

	if (!READ_ONCE(sap_dev.enable))
		return;

	if (READ_ONCE(sap_dev.reg_parse_from_dts)) {
		sap_dump_last_regs_v2(sap_dev.reg_list);
		return;
	}

	if (!cfg_base) {
		pr_err("invalid cfg_base, skip read last regs\n");
		return;
	}

	reg->status = readl(cfg_base + CFG_STATUS_OFFSET);
	reg->pc = readl(cfg_base + CFG_MON_PC_OFFSET);
	reg->lr = readl(cfg_base + CFG_MON_LR_OFFSET);
	reg->sp = readl(cfg_base + CFG_MON_SP_OFFSET);
	reg->pc_latch = readl(cfg_base + CFG_MON_PC_LATCH_OFFSET);
	reg->lr_latch = readl(cfg_base + CFG_MON_LR_LATCH_OFFSET);
	reg->sp_latch = readl(cfg_base + CFG_MON_SP_LATCH_OFFSET);
}

static void sap_dump_gpio_range(int start, int end)
{
	gpio_dump_regs_range(start, end);
}

static void sap_crash_dump_ois_pin_status(void)
{
	struct sap_device *dev = &sap_dev;

	if (!dev->dump_ois_pin_status)
		return;

	if (dev->ois_avdd_pin >= 0)
		pr_notice("ois avdd pin %d value %d\n",
			dev->ois_avdd_pin, gpio_get_value(dev->ois_avdd_pin));
	if (dev->ois_vdd_pin >= 0)
		pr_notice("ois vdd pin %d value %d\n",
			dev->ois_vdd_pin, gpio_get_value(dev->ois_vdd_pin));
	if (dev->ois_scl_pin >= 0)
		sap_dump_gpio_range(dev->ois_scl_pin, dev->ois_scl_pin);
	if (dev->ois_sda_pin >= 0)
		sap_dump_gpio_range(dev->ois_sda_pin, dev->ois_sda_pin);
}

void sap_show_last_regs(void)
{
	struct sap_status_reg *reg = &sap_dev.status_reg;

	if (!READ_ONCE(sap_dev.enable))
		return;

	pr_notice("reg status = %08x\n", reg->status);
	pr_notice("reg pc = %08x\n", reg->pc);
	pr_notice("reg lr = %08x\n", reg->lr);
	pr_notice("reg sp = %08x\n", reg->sp);
	pr_notice("reg pc_latch = %08x\n", reg->pc_latch);
	pr_notice("reg lr_latch = %08x\n", reg->lr_latch);
	pr_notice("reg sp_latch = %08x\n", reg->sp_latch);
	sap_crash_dump_ois_pin_status();
}

uint32_t sap_print_last_regs(char *buf, uint32_t size)
{
	struct sap_status_reg *reg = &sap_dev.status_reg;
	uint32_t len = 0;

	if (!READ_ONCE(sap_dev.enable))
		return 0;

	len += scnprintf(buf + len, size - len,
		"sap_status = %08x\n", reg->status);

	len += scnprintf(buf + len, size - len,
		"sap_pc = %08x\n", reg->pc);

	len += scnprintf(buf + len, size - len,
		"sap_lr = %08x\n", reg->lr);

	len += scnprintf(buf + len, size - len,
		"sap_sp = %08x\n", reg->sp);

	len += scnprintf(buf + len, size - len,
		"sap_pc_latch = %08x\n", reg->pc_latch);

	len += scnprintf(buf + len, size - len,
		"sap_lr_latch = %08x\n", reg->lr_latch);

	len += scnprintf(buf + len, size - len,
		"sap_sp_latch = %08x\n", reg->sp_latch);

	return len;
}

uint32_t sap_dump_detail_buff(uint8_t *buff, uint32_t size)
{
	struct sap_device *dev = &sap_dev;
	struct sap_status_reg *reg = &sap_dev.status_reg;

	if (!READ_ONCE(dev->enable))
		return 0;

	return snprintf(buff, size, "sap pc=0x%08x, lr=0x%08x, sp=0x%08x\n",
		reg->pc, reg->lr, reg->sp);
}

uint32_t sap_get_secure_dump_size(void)
{
	struct device_node *node = NULL;
	const char *sap_status = NULL;
	uint32_t dump_size = 0;

	/*
	 * NOTE: callee before sap_device_probe,
	 * directly access device tree get node value.
	 */
	node = of_find_node_by_name(NULL, "sap");
	if (!node) {
		pr_err("Node mediatek,sap not found\n");
		return 0;
	}

	of_property_read_string(node, "status", &sap_status);
	if (strncmp(sap_status, "okay", sizeof("okay"))) {
		pr_err("sap not enabled, skip\n");
		return 0;
	}

	of_property_read_u32(node, "secure-dump-size", &dump_size);
	return dump_size;
}

uint8_t sap_get_core_id(void)
{
	return sap_dev.core_id;
}

bool sap_enabled(void)
{
	return READ_ONCE(sap_dev.enable);
}
EXPORT_SYMBOL_GPL(sap_enabled);

bool sap_delicated_clock_supported(void)
{
	return READ_ONCE(sap_dev.enable)
		&& READ_ONCE(sap_dev.pll_clock_support);
}
EXPORT_SYMBOL_GPL(sap_delicated_clock_supported);

static uint32_t sap_cfg_reg_read(uint32_t reg_offset)
{
	void __iomem *cfg_base = sap_dev.reg_cfg;

	if (!cfg_base) {
		pr_err("invalid cfg_base, skip read %u\n", reg_offset);
		return 0;
	}

	return readl(cfg_base + reg_offset);
}

bool is_sap_trigger_wdt(void)
{
	struct struct_reg *reg_list = sap_dev.reg_list;

	if (!READ_ONCE(sap_dev.enable))
		return false;

	if (READ_ONCE(reg_list[REG_WDT_IRQ].valid))
		return readl(reg_list[REG_WDT_IRQ].addr);

	return sap_cfg_reg_read(CFG_WDT_IRQ_OFFSET);
}

bool is_sap_ready_to_reboot(void)
{
	struct struct_reg *reg_list = sap_dev.reg_list;

	if (!READ_ONCE(sap_dev.enable))
		return true;

	if (READ_ONCE(reg_list[REG_GPR5].valid))
		return readl(reg_list[REG_GPR5].addr) & CORE_RDY_TO_REBOOT;

	return sap_cfg_reg_read(CFG_GPR5_OFFSET) & CORE_RDY_TO_REBOOT;
}

bool is_sap_halted(void)
{
	struct struct_reg *reg_list = sap_dev.reg_list;

	if (!READ_ONCE(sap_dev.enable))
		return true;

	if (READ_ONCE(reg_list[REG_STATUS].valid))
		return readl(reg_list[REG_STATUS].addr) & B_CORE_HALT;

	return sap_cfg_reg_read(CFG_STATUS_OFFSET) & B_CORE_HALT;
}

static void sap_setup_pin_table(struct mtk_mbox_device *mbox_dev, uint8_t mbox)
{
	int i = 0, last_ofs = 0, last_idx = 0, last_slot = 0, last_sz = 0;
	struct mtk_mbox_pin_send *send_tbl = NULL;
	struct mtk_mbox_pin_recv *recv_tbl = NULL;
	struct mtk_mbox_info *info_tbl = NULL;

	send_tbl = (struct mtk_mbox_pin_send *)mbox_dev->pin_send_table;
	recv_tbl = (struct mtk_mbox_pin_recv *)mbox_dev->pin_recv_table;
	info_tbl = (struct mtk_mbox_info *)mbox_dev->info_table;

	for (i = 0; i < mbox_dev->send_count; i++) {
		if (mbox == send_tbl[i].mbox) {
			send_tbl[i].offset = last_ofs + last_slot;
			send_tbl[i].pin_index = last_idx + last_sz;
			last_idx = send_tbl[i].pin_index;
			if (info_tbl[mbox].is64d == 1) {
				last_sz = DIV_ROUND_UP(
				send_tbl[i].msg_size, 2);
				last_ofs = last_sz * 2;
				last_slot = last_idx * 2;
			} else {
				last_sz = send_tbl[i].msg_size;
				last_ofs = last_sz;
				last_slot = last_idx;
			}
		} else if (mbox < send_tbl[i].mbox)
			break; /* no need to search the rest ipi */
	}

	for (i = 0; i < mbox_dev->recv_count; i++) {
		if (mbox == recv_tbl[i].mbox) {
			recv_tbl[i].offset = last_ofs + last_slot;
			recv_tbl[i].pin_index = last_idx + last_sz;
			last_idx = recv_tbl[i].pin_index;
			if (info_tbl[mbox].is64d == 1) {
				last_sz = DIV_ROUND_UP(
				recv_tbl[i].msg_size, 2);
				last_ofs = last_sz * 2;
				last_slot = last_idx * 2;
			} else {
				last_sz = recv_tbl[i].msg_size;
				last_ofs = last_sz;
				last_slot = last_idx;
			}
		} else if (mbox < recv_tbl[i].mbox)
			break; /* no need to search the rest ipi */
	}

	if (last_idx > 32 ||
		(last_ofs + last_slot) > (info_tbl[mbox].is64d + 1) * 32) {
		pr_notice("mbox%d ofs(%d)/slot(%d) exceed the maximum\n",
			mbox, last_idx, last_ofs + last_slot);
		WARN_ON(1);
	}
}

static bool sap_parse_ipi_table(struct mtk_mbox_device *mbox_dev,
	struct platform_device *pdev)
{
	enum table_item_num {
		send_item_num = 3,
		recv_item_num = 4
	};
	u32 i, ret, mbox, recv_opt, recv_cells_mode;
	u32 recv_cells_num, lock, buf_full_opt, cb_ctx_opt;
	struct device_node *node = pdev->dev.of_node;
	struct mtk_mbox_info *mbox_info = NULL;
	struct mtk_mbox_pin_send *mbox_pin_send = NULL;
	struct mtk_mbox_pin_recv *mbox_pin_recv = NULL;

	of_property_read_u32(node, "mbox-count", &mbox_dev->count);
	if (!mbox_dev->count) {
		pr_err("mbox count not found\n");
		return false;
	}

	ret = of_property_read_u32(node, "#recv_cells_mode", &recv_cells_mode);
	if (ret) {
		recv_cells_num = recv_item_num;
	} else {
		if (recv_cells_mode == 1)
			recv_cells_num = 7;
		else
			recv_cells_num = recv_item_num;
	}

	mbox_dev->send_count = of_property_count_u32_elems(node, "send-table")
				/ send_item_num;
	if (mbox_dev->send_count <= 0) {
		pr_err("ipi send table not found\n");
		return false;
	}

	mbox_dev->recv_count = of_property_count_u32_elems(node,
		"recv-table") / recv_cells_num;
	if (mbox_dev->recv_count <= 0) {
		pr_err("ipi recv table not found\n");
		return false;
	}
	/* alloc and init scp_mbox_info */
	mbox_dev->info_table = vzalloc(sizeof(struct mtk_mbox_info)
		* mbox_dev->count);
	if (!mbox_dev->info_table)
		return false;

	mbox_info = mbox_dev->info_table;
	for (i = 0; i < mbox_dev->count; ++i) {
		mbox_info[i].id = i;
		mbox_info[i].slot = 64;
		mbox_info[i].enable = 1;
		mbox_info[i].is64d = 1;
	}
	/* alloc and init send table */
	mbox_dev->pin_send_table = vzalloc(sizeof(struct mtk_mbox_pin_send)
		* mbox_dev->send_count);
	if (!mbox_dev->pin_send_table)
		return false;

	mbox_pin_send = mbox_dev->pin_send_table;
	for (i = 0; i < mbox_dev->send_count; ++i) {
		ret = of_property_read_u32_index(node, "send-table",
			i * send_item_num, &mbox_pin_send[i].chan_id);
		if (ret) {
			pr_err("get chan_id fail for send_tbl %u\n", i);
			return false;
		}
		ret = of_property_read_u32_index(node, "send-table",
			i * send_item_num + 1, &mbox);
		if (ret || mbox >= mbox_dev->count) {
			pr_err("get mbox id fail for send_tbl %u\n", i);
			return false;
		}
		/* because mbox and recv_opt is a bit-field */
		mbox_pin_send[i].mbox = mbox;
		ret = of_property_read_u32_index(node, "send-table",
			i * send_item_num + 2, &mbox_pin_send[i].msg_size);
		if (ret) {
			pr_err("get msg_size fail for send_tbl %u", i);
			return false;
		}
	}

	/* alloc and init recv table */
	mbox_dev->pin_recv_table = vzalloc(sizeof(struct mtk_mbox_pin_recv)
		* mbox_dev->recv_count);
	if (!mbox_dev->pin_recv_table)
		return false;

	mbox_pin_recv = mbox_dev->pin_recv_table;
	for (i = 0; i < mbox_dev->recv_count; ++i) {
		ret = of_property_read_u32_index(node, "recv-table",
			i * recv_cells_num, &mbox_pin_recv[i].chan_id);
		if (ret) {
			pr_err("get chan_id fail for recv_tbl %u\n", i);
			return false;
		}
		ret = of_property_read_u32_index(node, "recv-table",
			i * recv_cells_num + 1,	&mbox);
		if (ret || mbox >= mbox_dev->count) {
			pr_err("get mbox_id fail for recv_tbl %u\n", i);
			return false;
		}
		/* because mbox and recv_opt is a bit-field */
		mbox_pin_recv[i].mbox = mbox;
		ret = of_property_read_u32_index(node, "recv-table",
			i * recv_cells_num + 2,	&mbox_pin_recv[i].msg_size);
		if (ret) {
			pr_err("get msg_size fail for recv_tbl %u\n", i);
			return false;
		}
		ret = of_property_read_u32_index(node, "recv-table",
			i * recv_cells_num + 3,	&recv_opt);
		if (ret) {
			pr_err("get recv_opt fail for recv_tbl %u\n", i);
			return false;
		}
		/* because mbox and recv_opt is a bit-field */
		mbox_pin_recv[i].recv_opt = recv_opt;
		if (recv_cells_mode == 1) {
			ret = of_property_read_u32_index(node, "recv-table",
				i * recv_cells_num + 4,	&lock);
			if (ret) {
				pr_err("get lock fail for recv_tbl %u\n", i);
				return false;
			}
			/* because lock is a bit-field */
			mbox_pin_recv[i].lock = lock;
			ret = of_property_read_u32_index(node, "recv-table",
				i * recv_cells_num + 5,	&buf_full_opt);
			if (ret) {
				pr_err("get buf_full_opt fail for recv_tbl %u\n", i);
				return false;
			}
			/* because buf_full_opt is a bit-field */
			mbox_pin_recv[i].buf_full_opt = buf_full_opt;
			ret = of_property_read_u32_index(node, "recv-table",
				i * recv_cells_num + 6,	&cb_ctx_opt);
			if (ret) {
				pr_err("get cb_ctx_opt fail for recv_tbl %u\n", i);
				return false;
			}
			/* because cb_ctx_opt is a bit-field */
			mbox_pin_recv[i].cb_ctx_opt = cb_ctx_opt;
		}
	}

	for (i = 0; i < mbox_dev->count; ++i) {
		mbox_info[i].mbdev = mbox_dev;
		if (mtk_mbox_probe(pdev, mbox_info[i].mbdev, i) < 0)
			continue;

		ret = enable_irq_wake(mbox_info[i].irq_num);
		if (ret < 0) {
			pr_notice("[SCP]mbox%d enable irq fail\n", i);
			continue;
		}

		sap_setup_pin_table(mbox_dev, i);
	}

	return true;
}

static int sap_device_parse_regs(struct platform_device *pdev)
{
	struct resource *res = NULL;
	struct sap_device *dev = &sap_dev;
	struct struct_reg *reg = NULL;
	uint32_t i = 0;

	for (i = 0; i < MAX_DTS_DEFINE_REGS; i++) {
		reg = &dev->reg_list[i];
		res = platform_get_resource_byname(pdev,
			IORESOURCE_MEM, reg->name);
		if (!res)
			continue;

		reg->addr = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(reg->addr))
			return (int)PTR_ERR(reg->addr);

		reg->size = resource_size(res);
		reg->valid = true;
	}

	return 0;
}

static int sap_device_probe(struct platform_device *pdev)
{
	struct sap_device *dev = &sap_dev;
	const char *sap_status = NULL;
	uint8_t core_id = 0;
	struct resource *res = NULL;
	int ret = 0;

	ret = of_property_read_string(pdev->dev.of_node, "status", &sap_status);
	if (ret < 0 || strncmp(sap_status, "okay", sizeof("okay"))) {
		pr_info("sap not enabled, no need and skip\n");
		return 0;
	}

	dev->pll_clock_support = of_property_read_bool(
		pdev->dev.of_node, "pll-clock-support");
	if (!dev->pll_clock_support)
		pr_info("don't support pll clock, using scp clock\n");
	else
		pr_info("pll clock support\n");

	ret = of_property_read_u8(pdev->dev.of_node, "core-id", &core_id);
	if (ret < 0) {
		pr_err("invalid core_id res, %d\n", ret);
		return ret;
	}

	WRITE_ONCE(dev->enable, true);
	dev->core_id = core_id;

	dev->reg_parse_from_dts = of_property_read_bool(
			pdev->dev.of_node, "reg-parse-from-dts");
	if (dev->reg_parse_from_dts) {
		ret = sap_device_parse_regs(pdev);
		if (ret < 0)
			goto err_res;
	} else {
		res = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "sap_cfg_reg");
		if (!res) {
			ret = -EINVAL;
			goto err_res;
		}

		dev->reg_cfg = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(dev->reg_cfg)) {
			ret = (int)PTR_ERR(dev->reg_cfg);
			goto err_res;
		}
	}

	dev->mbox_dev.name = "sap_mboxdev";
	if (!sap_parse_ipi_table(&dev->mbox_dev, pdev)) {
		ret = -EINVAL;
		goto err_ipi;
	}

	sap_ipidev.name = "sap_ipidev";
	sap_ipidev.id = IPI_DEV_SAP;
	sap_ipidev.mbdev = &dev->mbox_dev;
	sap_ipidev.pre_cb = (ipi_tx_cb_t)scp_awake_lock;
	sap_ipidev.post_cb = (ipi_tx_cb_t)scp_awake_unlock;
	sap_ipidev.prdata = 0;
	ret = mtk_ipi_device_register(&sap_ipidev, pdev,
		&dev->mbox_dev, SAP_IPI_COUNT);
	if (ret < 0) {
		pr_err("register ipi fail %d\n", ret);
		goto err_ipi;
	}

	dev->dump_ois_pin_status = of_property_read_bool(
			pdev->dev.of_node, "dump-ois-pin-status");
	if (dev->dump_ois_pin_status) {
		dev->ois_avdd_pin = of_get_named_gpio(
			pdev->dev.of_node, "ois-avdd-pin", 0);
		dev->ois_vdd_pin = of_get_named_gpio(
			pdev->dev.of_node, "ois-vdd-pin", 0);
		ret = of_property_read_u32(
			pdev->dev.of_node, "ois-scl-pin", &dev->ois_scl_pin);
		if (ret < 0)
			dev->ois_scl_pin = -EINVAL;
		ret = of_property_read_u32(
			pdev->dev.of_node, "ois-sda-pin", &dev->ois_sda_pin);
		if (ret < 0)
			dev->ois_sda_pin = -EINVAL;
	}

	return 0;

err_ipi:
err_res:
	WRITE_ONCE(dev->enable, false);
	return ret;
}

static void sap_device_remove(struct platform_device *pdev)
{
	struct sap_device *dev = &sap_dev;

	if (!READ_ONCE(dev->enable))
		return;

	WRITE_ONCE(dev->enable, false);
	WRITE_ONCE(dev->pll_clock_support, false);
}

static const struct of_device_id sap_of_ids[] = {
	{ .compatible = "mediatek,sap", },
	{}
};

static struct platform_driver mtk_sap_device = {
	.probe = sap_device_probe,
	.remove = sap_device_remove,
	.driver = {
		.name = "sap",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = sap_of_ids,
#endif
	},
};

void sap_init(void)
{
	struct sap_device *dev = &sap_dev;

	dev->reg_list = sap_reg_list;
	WRITE_ONCE(dev->enable, false);
	WRITE_ONCE(dev->pll_clock_support, false);
	if (platform_driver_register(&mtk_sap_device))
		pr_err("register fail\n");
}

void sap_exit(void)
{
	struct sap_device *dev = &sap_dev;

	if (!READ_ONCE(dev->enable))
		return;

	platform_driver_unregister(&mtk_sap_device);
}
