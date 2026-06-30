// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>

#include <mtk_cpupm_dbg.h>
#include <lpm_dbg_common_v2.h>
#include <lpm_module.h>
#include <lpm_dbg_fs_common.h>
#include <lpm_dbg_logger.h>
#if IS_ENABLED(CONFIG_MTK_SYS_RES_DBG_SUPPORT)
#include <lpm_sys_res.h>
#endif

static unsigned int ack_chk_irq_clr(void)
{
	return lpm_smc_spm_dbg(MT_SPM_DBG_SMC_ACK_CHK, MT_LPM_SMC_ACT_SET, 0, 0);
}

static irqreturn_t spm_dbg_ack_chk_handler(int irq, void *dev_id)
{
	disable_irq_nosync(irq);
	lpm_ocla_pause();

	return IRQ_WAKE_THREAD;
}

static irqreturn_t spm_dbg_ack_chk_thread(int irq, void *dev_id)
{
	pr_info("[name:spm&][SPM] spm ack chk dbg irq thread\n");
	lpm_ocla_sram_bk();
	lpm_ocla_continue();

	if (!ack_chk_irq_clr())
		enable_irq(irq);

	pr_info("[name:spm&][SPM] spm ack chk dbg irq thread done\n");

	return IRQ_HANDLED;
}

static irqreturn_t spm_dbg_irq_handler(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

#define MTK_LPM_SLEEP_DEBUG_COMPATIBLE_STRING "mediatek,sleep-debug"
#define MTK_LPM_ACK_CHK_IRQ_NAME "spm-dbg-ack-chk"

static int lpm_dbg_init_spm_irq(void)
{
	struct device_node *node;
	int irq_cnt, ret;
	unsigned int i, irq;
	const char *irq_name;

	node = of_find_compatible_node(NULL, NULL, MTK_LPM_SLEEP_DEBUG_COMPATIBLE_STRING);
	if (!node) {
		pr_info("[name:spm&][SPM] %s: node %s not found.\n", __func__,
			MTK_LPM_SLEEP_DEBUG_COMPATIBLE_STRING);
		goto FINISHED;
	}

	irq_cnt = of_property_count_u32_elems(node, "interrupts") / 4;

	for (i = 0; i < irq_cnt; i++) {
		irq = irq_of_parse_and_map(node, i);
		if (!irq) {
			pr_info("[name:spm&][SPM] failed to get spm irq\n");
			goto NODE_FINISHED;
		}

		ret = of_property_read_string_index(node, "interrupt-names", i, &irq_name);
		if (ret) {
			pr_info("[name:spm&][SPM] Failed to get irq name for irq %d\n", i);
			goto NODE_FINISHED;
		}

		ret = enable_irq_wake(irq);
		if (ret) {
			pr_info("[name:spm&][SPM] failed to enable spm irq wake, ret = %d\n", ret);
			goto NODE_FINISHED;
		}

		if (!strcmp(irq_name, MTK_LPM_ACK_CHK_IRQ_NAME))
			ret = request_threaded_irq(irq, spm_dbg_ack_chk_handler,
						   spm_dbg_ack_chk_thread, 0, irq_name, NULL);
		/* Add dbg irq here */
		else
			ret = request_irq(irq, spm_dbg_irq_handler, 0, irq_name, NULL);
		if (ret) {
			pr_info("[name:spm&][SPM] failed to install spm irq handler, ret = %d\n",
				ret);
			goto NODE_FINISHED;
		}
		pr_info("[name:spm&][SPM] %s: install %s %d\n", __func__, irq_name,  irq);
	}
NODE_FINISHED:
	of_node_put(node);
FINISHED:
	return 0;
}

static int __init dbg_early_initcall(void)
{
	return 0;
}

static int __init dbg_device_initcall(void)
{
	lpm_dbg_common_fs_init();
	lpm_dbg_fs_init();
	mtk_cpupm_dbg_init();
	return 0;
}

static int __init dbg_late_initcall(void)
{
	lpm_logger_init();
	lpm_dbg_init_spm_irq();
	return 0;
}

int __init lpm_dbg_init(void)
{
	int ret = 0;
	ret = dbg_early_initcall();
	if (ret)
		goto dbg_init_fail;

	ret = dbg_device_initcall();

	if (ret)
		goto dbg_init_fail;

	ret = dbg_late_initcall();

	if (ret)
		goto dbg_init_fail;

	ret = lpm_dbg_pm_init();

	if (ret)
		goto dbg_init_fail;
#if IS_ENABLED(CONFIG_MTK_SYS_RES_DBG_SUPPORT)
	lpm_sys_res_init();
#endif
	return 0;

dbg_init_fail:
	return -EAGAIN;
}

void __exit lpm_dbg_exit(void)
{
	lpm_dbg_pm_exit();
	lpm_dbg_common_fs_exit();
	lpm_dbg_fs_exit();
	mtk_cpupm_dbg_exit();
	lpm_logger_deinit();
#if IS_ENABLED(CONFIG_MTK_SYS_RES_DBG_SUPPORT)
	lpm_sys_res_exit();
#endif
}

module_init(lpm_dbg_init);
module_exit(lpm_dbg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("low power debug module");
MODULE_AUTHOR("MediaTek Inc.");
