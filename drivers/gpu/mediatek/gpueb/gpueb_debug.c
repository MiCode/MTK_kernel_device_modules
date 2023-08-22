// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

/**
 * @file    gpueb_debug.c
 * @brief   Debug mechanism for GPU-DVFS
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

#include "gpueb_helper.h"
#include "gpueb_debug.h"
#include "gpueb_ipi.h"

/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */
enum gpueb_smc_op {
	GPUEB_SMC_OP_TRIGGER_WDT            = 0,
	GPUACP_SMC_OP_CPUPM_PWR             = 1,
	GPUEB_SMC_OP_NUMBER                 = 2,
};

static void __iomem *g_gpueb_base;
static void __iomem *g_gpueb_gpr_base;
static void __iomem *g_gpueb_reg_base;
static void __iomem *g_gpueb_mbox_ipi;
static void __iomem *g_gpueb_mbox_sw_int;

/**
 * ===============================================
 * Function Definition
 * ===============================================
 */
void gpueb_dump_status(char *log_buf, int *log_len, int log_size)
{
	gpueb_pr_logbuf(log_buf, log_len, log_size,
		"== [GPUEB STATUS] ==");

	if (g_gpueb_reg_base) {
		/* 0x13C60300 */
		gpueb_pr_logbuf(log_buf, log_len, log_size,
			"GPUEB_INTC_IRQ_EN (0x%x): 0x%08x",
			(0x13C60000 + 0x300), readl(g_gpueb_reg_base + 0x300));
		/* 0x13C60338 */
		gpueb_pr_logbuf(log_buf, log_len, log_size,
			"GPUEB_INTC_IRQ_STA (0x%x): 0x%08x",
			(0x13C60000 + 0x338), readl(g_gpueb_reg_base + 0x338));
		/* 0x13C6033C */
		gpueb_pr_logbuf(log_buf, log_len, log_size,
			"GPUEB_INTC_IRQ_RAW_STA (0x%x): 0x%08x",
			(0x13C60000 + 0x33C), readl(g_gpueb_reg_base + 0x33C));
		/* 0x13C60348 */
		gpueb_pr_logbuf(log_buf, log_len, log_size,
			"GPUEB_INTC_IRQ_GRP2_STA (0x%x): 0x%08x",
			(0x13C60000 + 0x348), readl(g_gpueb_reg_base + 0x348));

		/* 0x13C6060C */
		gpueb_pr_logbuf(log_buf, log_len, log_size,
			"GPUEB_CFGREG_AXI_CFG (0x%x): 0x%08x",
			(0x13C60000 + 0x60C), readl(g_gpueb_reg_base + 0x60C));
		/* 0x13C60610 */
		gpueb_pr_logbuf(log_buf, log_len, log_size,
			"GPUEB_CFGREG_AXI_STA (0x%x): 0x%08x",
			(0x13C60000 + 0x610), readl(g_gpueb_reg_base + 0x610));
		/* 0x13C60618 */
		gpueb_pr_logbuf(log_buf, log_len, log_size,
			"GPUEB_CFGREG_WDT_CON (0x%x): 0x%08x",
			(0x13C60000 + 0x618), readl(g_gpueb_reg_base + 0x618));
		/* 0x13C6061C */
		gpueb_pr_logbuf(log_buf, log_len, log_size,
			"GPUEB_CFGREG_WDT_KICK (0x%x): 0x%08x",
			(0x13C60000 + 0x61C), readl(g_gpueb_reg_base + 0x61C));
		/* 0x13C60634 */
		gpueb_pr_logbuf(log_buf, log_len, log_size,
			"GPUEB_CFGREG_MDSP_CFG (0x%x): 0x%08x",
			(0x13C60000 + 0x634), readl(g_gpueb_reg_base + 0x634));
		/* 0x13C6071C */
		gpueb_pr_logbuf(log_buf, log_len, log_size,
			"GPUEB_CFGREG_DBG_APB_PC (0x%x): 0x%08x",
			(0x13C60000 + 0x71C), readl(g_gpueb_reg_base + 0x71C));
		/* 0x13C60720 */
		gpueb_pr_logbuf(log_buf, log_len, log_size,
			"GPUEB_CFGREG_DBG_APB_LR (0x%x): 0x%08x",
			(0x13C60000 + 0x720), readl(g_gpueb_reg_base + 0x720));
		/* 0x13C60724 */
		gpueb_pr_logbuf(log_buf, log_len, log_size,
			"GPUEB_CFGREG_DBG_APB_SP (0x%x): 0x%08x",
			(0x13C60000 + 0x724), readl(g_gpueb_reg_base + 0x724));
	} else
		gpueb_pr_info("skip null g_gpueb_reg_base");

	if (g_gpueb_gpr_base) {
		gpueb_pr_logbuf(log_buf, log_len, log_size,
			"LOW_POWER_FOOTPRINT_GPR: 0x%08x",
			readl(g_gpueb_gpr_base + 0x14));
		gpueb_pr_logbuf(log_buf, log_len, log_size,
			"GPUFREQ_FOOTPRINT_GPR: 0x%08x",
			readl(g_gpueb_gpr_base + 0x44));
		gpueb_pr_logbuf(log_buf, log_len, log_size,
			"GPUEB_DRAM_RES_STA_GPR: 0x%08x",
			readl(g_gpueb_gpr_base + 0x40));
		gpueb_pr_logbuf(log_buf, log_len, log_size,
			"GPUEB_DIAGNOSIS_GPR: 0x%08x",
			readl(g_gpueb_gpr_base + 0x48));
	} else
		gpueb_pr_info("skip null g_gpueb_gpr_base");

	if (g_gpueb_mbox_ipi) {
		/* 0x13C62000 */
		gpueb_pr_logbuf(log_buf, log_len, log_size,
			"GPUEB_MBOX_IPI_GPUEB (0x%x): 0x%08x",
			(0x13C62000), readl(g_gpueb_mbox_ipi));
	} else
		gpueb_pr_info("skip null g_gpueb_mbox_ipi");

	if (g_gpueb_mbox_sw_int) {
		/* 0x13C62078 */
		gpueb_pr_logbuf(log_buf, log_len, log_size,
			"GPUEB_MBOX_SW_INT_STA (0x%x): 0x%08x",
			(0x13C62078), readl(g_gpueb_mbox_sw_int));
	} else
		gpueb_pr_info("skip null g_gpueb_mbox_sw_int");
}
EXPORT_SYMBOL(gpueb_dump_status);

void gpueb_trigger_wdt(const char *name)
{
	struct arm_smccc_res res;

	gpueb_pr_info("GPUEB WDT is triggered by %s", name);

	arm_smccc_smc(
		MTK_SIP_KERNEL_GPUEB_CONTROL,  /* a0 */
		GPUEB_SMC_OP_TRIGGER_WDT,      /* a1 */
		0, 0, 0, 0, 0, 0, &res);
}
EXPORT_SYMBOL(gpueb_trigger_wdt);

#if defined(CONFIG_PROC_FS)
/* PROCFS: show current gpueb status */
static int gpueb_status_proc_show(struct seq_file *m, void *v)
{
	seq_puts(m, "[GPUEB-DEBUG] Current Status of GPUEB\n");

	if (g_gpueb_reg_base) {
		/* 0x13C60300 */
		seq_printf(m, "@%s: GPUEB_INTC_IRQ_EN (0x%x): 0x%08x\n", __func__,
			(0x13C60000 + 0x300), readl(g_gpueb_reg_base + 0x300));
		/* 0x13C60338 */
		seq_printf(m, "@%s: GPUEB_INTC_IRQ_STA (0x%x): 0x%08x\n", __func__,
			(0x13C60000 + 0x338), readl(g_gpueb_reg_base + 0x338));
		/* 0x13C6033C */
		seq_printf(m, "@%s: GPUEB_INTC_IRQ_RAW_STA (0x%x): 0x%08x\n", __func__,
			(0x13C60000 + 0x33C), readl(g_gpueb_reg_base + 0x33C));
		/* 0x13C60348 */
		seq_printf(m, "@%s: GPUEB_INTC_IRQ_GRP2_STA (0x%x): 0x%08x\n", __func__,
			(0x13C60000 + 0x348), readl(g_gpueb_reg_base + 0x348));

		/* 0x13C6060C */
		seq_printf(m, "@%s: GPUEB_CFGREG_AXI_CFG (0x%x): 0x%08x\n", __func__,
			(0x13C60000 + 0x60C), readl(g_gpueb_reg_base + 0x60C));
		/* 0x13C60610 */
		seq_printf(m, "@%s: GPUEB_CFGREG_AXI_STA (0x%x): 0x%08x\n", __func__,
			(0x13C60000 + 0x610), readl(g_gpueb_reg_base + 0x610));
		/* 0x13C60618 */
		seq_printf(m, "@%s: GPUEB_CFGREG_WDT_CON (0x%x): 0x%08x\n", __func__,
			(0x13C60000 + 0x618), readl(g_gpueb_reg_base + 0x618));
		/* 0x13C6061C */
		seq_printf(m, "@%s: GPUEB_CFGREG_WDT_KICK (0x%x): 0x%08x\n", __func__,
			(0x13C60000 + 0x61C), readl(g_gpueb_reg_base + 0x61C));
		/* 0x13C60634 */
		seq_printf(m, "@%s: GPUEB_CFGREG_MDSP_CFG (0x%x): 0x%08x\n", __func__,
			(0x13C60000 + 0x634), readl(g_gpueb_reg_base + 0x634));
		/* 0x13C6071C */
		seq_printf(m, "@%s: GPUEB_CFGREG_DBG_APB_PC (0x%x): 0x%08x\n", __func__,
			(0x13C60000 + 0x71C), readl(g_gpueb_reg_base + 0x71C));
		/* 0x13C60720 */
		seq_printf(m, "@%s: GPUEB_CFGREG_DBG_APB_LR (0x%x): 0x%08x\n", __func__,
			(0x13C60000 + 0x720), readl(g_gpueb_reg_base + 0x720));
		/* 0x13C60724 */
		seq_printf(m, "@%s: GPUEB_CFGREG_DBG_APB_SP (0x%x): 0x%08x\n", __func__,
			(0x13C60000 + 0x724), readl(g_gpueb_reg_base + 0x724));
	} else
		seq_printf(m, "@%s: skip null g_gpueb_reg_base\n", __func__);

	if (g_gpueb_gpr_base) {
		seq_printf(m, "@%s: LOW_POWER_FOOTPRINT_GPR: 0x%08x\n", __func__,
			readl(g_gpueb_gpr_base + 0x14));
		seq_printf(m, "@%s: GPUFREQ_FOOTPRINT_GPR: 0x%08x\n", __func__,
			readl(g_gpueb_gpr_base + 0x44));
	} else
		seq_printf(m, "@%s: skip null g_gpueb_gpr_base\n", __func__);

	if (g_gpueb_mbox_ipi)
		/* 0x13C62000 */
		seq_printf(m, "@%s: GPUEB_MBOX_IPI_GPUEB (0x%x): 0x%08x\n", __func__,
			(0x13C62000), readl(g_gpueb_mbox_ipi));
	else
		seq_printf(m, "@%s: skip null g_gpueb_mbox_ipi\n", __func__);

	if (g_gpueb_mbox_sw_int)
		/* 0x13C62078 */
		seq_printf(m, "@%s: GPUEB_MBOX_SW_INT_STA (0x%x): 0x%08x\n", __func__,
			(0x13C62078), readl(g_gpueb_mbox_sw_int));
	else
		seq_printf(m, "@%s: skip null g_gpueb_mbox_sw_int\n", __func__);

	return 0;
}

/* PROCFS: show current gpueb dram user status */
static int gpueb_dram_user_status_proc_show(struct seq_file *m, void *v)
{
	unsigned int dram_res = 0;
	int user_id = 0;

	if (g_gpueb_gpr_base) {
		dram_res = readl(g_gpueb_gpr_base + 0x40);
		seq_printf(m, "@%s: GPUEB_DRAM_RES_STA_GPR: 0x%08x\n",
			__func__, dram_res);

		for (user_id = 0; user_id < ARRAY_SIZE(gpueb_dram_user_name); user_id++) {
			seq_printf(m, "%s:%d ", gpueb_dram_user_name[user_id],
				(dram_res & (0x1 << user_id)) ? 1 : 0);
		}
		seq_puts(m, "\n");
	}
	return 0;
}

/* PROCFS: trigger GPUEB WDT */
static int force_trigger_wdt_proc_show(struct seq_file *m, void *v)
{
	seq_puts(m, "[GPUEB-DEBUG] Force trigger GPUEB WDT\n");
	return 0;
}

static ssize_t force_trigger_wdt_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = 0;
	char buf[64];
	unsigned int len = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = -1;
		goto done;
	}
	buf[len] = '\0';

	if (sysfs_streq(buf, WDT_EXCEPTION_EN))
		gpueb_trigger_wdt("GPUEB_DEBUG");

done:
	return (ret < 0) ? ret : count;
}

#if IPI_TEST
/* PROCFS: GPUEB ipi test */
static int gpueb_ipi_test_proc_show(struct seq_file *m, void *v)
{
	int i, ret;
	int send_msg;

	seq_puts(m, "[GPUEB-TEST] Test AP <-> EB IPI\n");

	for (i = 0; i < gpueb_get_send_pin_count(); i++) {
		send_msg = i * 10 + 1;
		ret = gpueb_ipi_send_compl_test(i, send_msg);
		if (ret < 0)
			seq_printf(m, "ipi #%d send_compl failed, ret=%d\n", i, ret);
		else
			seq_printf(m, "ipi #%d, send %d, recv %d\n", i, send_msg, ret);
	}
	return 0;
}

static ssize_t gpueb_ipi_test_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = 0;
	char buf[64];
	unsigned int len = 0;
	int ipi_id, send_msg;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = -1;
		goto done;
	}
	buf[len] = '\0';

	if (sscanf(buf, "%d %d", &ipi_id, &send_msg) == 2) {
		ret = gpueb_ipi_send_compl_test(ipi_id, send_msg);
		if (ret < 0)
			gpueb_pr_info("ipi send failed, ret=%d", ret);
		else
			gpueb_pr_info("ipi send %d, recv %d", send_msg, ret);
	}

done:
	return (ret < 0) ? ret : count;
}
#endif

/* PROCFS : initialization */
PROC_FOPS_RO(gpueb_status);
PROC_FOPS_RO(gpueb_dram_user_status);
PROC_FOPS_RW(force_trigger_wdt);
#if IPI_TEST
PROC_FOPS_RW(gpueb_ipi_test);
#endif

static int gpueb_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i = 0;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
	};

	const struct pentry default_entries[] = {
		PROC_ENTRY(gpueb_status),
		PROC_ENTRY(gpueb_dram_user_status),
		PROC_ENTRY(force_trigger_wdt),
#if IPI_TEST
		PROC_ENTRY(gpueb_ipi_test)
#endif
	};

	dir = proc_mkdir("gpueb", NULL);
	if (!dir) {
		gpueb_pr_info("fail to create /proc/gpueb (ENOMEM)");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(default_entries); i++) {
		if (!proc_create(default_entries[i].name, 0660,
			dir, default_entries[i].fops))
			gpueb_pr_info("fail to create /proc/gpueb/%s", default_entries[i].name);
	}

	return 0;
}
#endif /* CONFIG_PROC_FS */

void gpueb_debug_init(struct platform_device *pdev)
{
	int ret = 0;
	struct device *gpueb_dev = &pdev->dev;
	struct resource *res = NULL;

#if defined(CONFIG_PROC_FS)
	ret = gpueb_create_procfs();
	if (ret)
		gpueb_pr_info("fail to create procfs (%d)", ret);
#endif /* CONFIG_PROC_FS */

	if (unlikely(!gpueb_dev)) {
		gpueb_pr_info("fail to find gpueb device");
		return;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gpueb_base");
	if (unlikely(!res)) {
		gpueb_pr_info("fail to get resource GPUEB_BASE");
		return;
	}
	g_gpueb_base = devm_ioremap(gpueb_dev, res->start, resource_size(res));
	if (unlikely(!g_gpueb_base)) {
		gpueb_pr_info("fail to ioremap GPUEB_BASE: 0x%llx", res->start);
		return;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gpueb_gpr_base");
	if (unlikely(!res)) {
		gpueb_pr_info("fail to get resource GPUEB_GPR_BASE");
		return;
	}
	g_gpueb_gpr_base = devm_ioremap(gpueb_dev, res->start, resource_size(res));
	if (unlikely(!g_gpueb_gpr_base)) {
		gpueb_pr_info("fail to ioremap gpr base: 0x%llx", res->start);
		return;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gpueb_reg_base");
	if (unlikely(!res)) {
		gpueb_pr_info("fail to get resource GPUEB_REG_BASE");
		return;
	}
	g_gpueb_reg_base = devm_ioremap(gpueb_dev, res->start, resource_size(res));
	if (unlikely(!g_gpueb_reg_base)) {
		gpueb_pr_info("fail to ioremap reg base: 0x%llx", res->start);
		return;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mbox0_send");
	if (unlikely(!res)) {
		gpueb_pr_info("fail to get resource MBOX0_SEND");
		return;
	}
	g_gpueb_mbox_ipi = devm_ioremap(gpueb_dev, res->start, resource_size(res));
	if (unlikely(!g_gpueb_mbox_ipi)) {
		gpueb_pr_info("fail to ioremap MBOX0_SEND: 0x%llx", res->start);
		return;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mbox0_recv");
	if (unlikely(!res)) {
		gpueb_pr_info("fail to get resource MBOX0_RECV");
		return;
	}
	g_gpueb_mbox_sw_int = devm_ioremap(gpueb_dev, res->start, resource_size(res));
	if (unlikely(!g_gpueb_mbox_sw_int)) {
		gpueb_pr_info("fail to ioremap MBOX0_RECV: 0x%llx", res->start);
		return;
	}
}
