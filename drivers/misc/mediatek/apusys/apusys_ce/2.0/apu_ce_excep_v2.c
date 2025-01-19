// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>

#include "apu.h"
#include "apu_ce_excep.h"
#include "apu_ce_excep_v2.h"

static const struct of_device_id apu_ce_of_match[] = {
	{ .compatible = "mediatek,mt6993-apusys_rv"},
	{},
};

int apu_ce_excep_is_compatible_v2(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(apu_ce_of_match); i++)
		if (of_device_is_compatible(
				pdev->dev.of_node, apu_ce_of_match[i].compatible))
			return 1;

	return 0;
}

int is_apu_ce_excep_init_v2(void) {return 0;}
int apu_ce_excep_init_v2(struct platform_device *pdev, struct mtk_apu *apu) {return 0;}
void apu_ce_excep_remove_v2(struct platform_device *pdev, struct mtk_apu *apu) {}
void apu_ce_mrdump_register_v2(struct mtk_apu *apu) {}
void apu_ce_procfs_init_v2(struct platform_device *pdev,
	struct proc_dir_entry *procfs_root) {}
void apu_ce_procfs_remove_v2(struct platform_device *pdev,
	struct proc_dir_entry *procfs_root) {}
uint32_t apu_ce_reg_dump_v2(struct device *dev) {return 0;}
uint32_t apu_ce_sram_dump_v2(struct device *dev) {return 0;}