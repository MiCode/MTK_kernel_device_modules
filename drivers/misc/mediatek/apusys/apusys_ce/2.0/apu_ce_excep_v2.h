/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef APU_CE_EXCEP_V2_H
#define APU_CE_EXCEP_V2_H

int is_apu_ce_excep_init_v2(void);
int apu_ce_excep_is_compatible_v2(struct platform_device *pdev);
int apu_ce_excep_init_v2(struct platform_device *pdev, struct mtk_apu *apu);
void apu_ce_excep_remove_v2(struct platform_device *pdev, struct mtk_apu *apu);
void apu_ce_mrdump_register_v2(struct mtk_apu *apu);
void apu_ce_procfs_init_v2(struct platform_device *pdev,
	struct proc_dir_entry *procfs_root);
void apu_ce_procfs_remove_v2(struct platform_device *pdev,
	struct proc_dir_entry *procfs_root);
uint32_t apu_ce_reg_dump_v2(struct device *dev);
uint32_t apu_ce_sram_dump_v2(struct device *dev);

#endif /* APU_CE_EXCEP_V2_H */
