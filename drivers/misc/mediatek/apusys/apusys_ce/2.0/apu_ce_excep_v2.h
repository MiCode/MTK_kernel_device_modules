/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef APU_CE_EXCEP_V2_H
#define APU_CE_EXCEP_V2_H

#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include "apu.h"

#define MT6993_ARE_ABNORMAL_IRQ_BIT                   (21)
#define APU_V2_CE_SRAMBASE			(0x19040000)
#define APU_V2_ARE_REG_BASE			(0x19050000)
#define APU_V2_ARE_SRAM_SIZE			(0x10000)
#define APU_V2_CE_REG_DUMP_MAGIC_NUM          (0xA5A5A5A5)
#define APU_V2_CE_0_IRQ_MASK                  (0x0000001F)
#define APU_V2_CE_1_IRQ_MASK                  (0x00001F00)
#define APU_V2_CE_2_IRQ_MASK                  (0x001F0000)
#define APU_V2_CE_3_IRQ_MASK                  (0x1F000000)
#define APU_V2_CE_TASK_JOB_SFT                      (0x10)
#define APU_V2_CE_TASK_JOB_MSK                      (0x1F)
#define APU_V2_CE_MISS_TYPE2_REQ_FLAG_0_MSK        (0x100)
#define APU_V2_CE_MISS_TYPE2_REQ_FLAG_1_MSK        (0x200)
#define APU_V2_CE_MISS_TYPE2_REQ_FLAG_2_MSK        (0x400)
#define APU_V2_CE_MISS_TYPE2_REQ_FLAG_3_MSK        (0x800)
#define APU_V2_CE_NON_ALIGNED_APB_FLAG_MSK     (0x6000000)
#define APU_V2_CE_NON_ALIGNED_APB_OUT_FLAG_MSK (0x2000000)
#define APU_V2_CE_NON_ALIGNED_APB_IN_FLAG_MSK  (0x4000000)
#define APU_V2_CE_APB_ERR_STATUS_CE0_MSK             (0x1)
#define APU_V2_CE_APB_ERR_STATUS_CE1_MSK             (0x2)
#define APU_V2_CE_APB_ERR_STATUS_CE2_MSK             (0x4)
#define APU_V2_CE_APB_ERR_STATUS_CE3_MSK             (0x8)

#define GET_SMC_OP_V2(op1, op2, op3) (op1 | op2 << (8 * 1) | op3 << (8 * 2))

enum {
	SMC_OP_APU_V2_CE_NULL = 0,
	SMC_OP_APU_V2_ACE_ABN_IRQ_FLAG_CE,
	SMC_OP_APU_V2_ACE_ABN_IRQ_FLAG_ACE_SW,
	SMC_OP_APU_V2_ACE_ABN_IRQ_FLAG_USER,
	SMC_OP_APU_V2_ACE_CE0_TASK_ING,
	SMC_OP_APU_V2_ACE_CE1_TASK_ING,
	SMC_OP_APU_V2_ACE_CE2_TASK_ING,
	SMC_OP_APU_V2_ACE_CE3_TASK_ING,
	SMC_OP_APU_V2_CE0_RUN_INSTR,
	SMC_OP_APU_V2_CE1_RUN_INSTR,
	SMC_OP_APU_V2_CE2_RUN_INSTR,
	SMC_OP_APU_V2_CE3_RUN_INSTR,
	SMC_OP_APU_V2_CE0_TIMEOUT_INSTR,
	SMC_OP_APU_V2_CE1_TIMEOUT_INSTR,
	SMC_OP_APU_V2_CE2_TIMEOUT_INSTR,
	SMC_OP_APU_V2_CE3_TIMEOUT_INSTR,
	SMC_OP_APU_V2_ACE_AXI_MST_WR_STATUS_ERR,
	SMC_OP_APU_V2_ACE_AXI_MST_RD_STATUS_ERR,
	SMC_OP_APU_V2_CE0_RUN_PC,
	SMC_OP_APU_V2_CE1_RUN_PC,
	SMC_OP_APU_V2_CE2_RUN_PC,
	SMC_OP_APU_V2_CE3_RUN_PC,
	SMC_OP_APU_V2_ACE_CMD_Q_STATUS_0,
	SMC_OP_APU_V2_ACE_CMD_Q_STATUS_1,
	SMC_OP_APU_V2_ACE_CMD_Q_STATUS_2,
	SMC_OP_APU_V2_ACE_CMD_Q_STATUS_3,
	SMC_OP_APU_V2_ACE_CMD_Q_STATUS_4,
	SMC_OP_APU_V2_ACE_CMD_Q_STATUS_5,
	SMC_OP_APU_V2_ACE_CMD_Q_STATUS_6,
	SMC_OP_APU_V2_ACE_CMD_Q_STATUS_7,
	SMC_OP_APU_V2_CE0_STEP,
	SMC_OP_APU_V2_CE1_STEP,
	SMC_OP_APU_V2_CE2_STEP,
	SMC_OP_APU_V2_CE3_STEP,
	SMC_OP_APU_V2_CE0_FOOTPRINT_0,
	SMC_OP_APU_V2_CE0_FOOTPRINT_1,
	SMC_OP_APU_V2_CE0_FOOTPRINT_2,
	SMC_OP_APU_V2_CE0_FOOTPRINT_3,
	SMC_OP_APU_V2_CE1_FOOTPRINT_0,
	SMC_OP_APU_V2_CE1_FOOTPRINT_1,
	SMC_OP_APU_V2_CE1_FOOTPRINT_2,
	SMC_OP_APU_V2_CE1_FOOTPRINT_3,
	SMC_OP_APU_V2_CE2_FOOTPRINT_0,
	SMC_OP_APU_V2_CE2_FOOTPRINT_1,
	SMC_OP_APU_V2_CE2_FOOTPRINT_2,
	SMC_OP_APU_V2_CE2_FOOTPRINT_3,
	SMC_OP_APU_V2_CE3_FOOTPRINT_0,
	SMC_OP_APU_V2_CE3_FOOTPRINT_1,
	SMC_OP_APU_V2_CE3_FOOTPRINT_2,
	SMC_OP_APU_V2_CE3_FOOTPRINT_3,
	SMC_OP_APU_V2_ACE_APB_MST_IN_STATUS,
	SMC_OP_APU_V2_CE0_RUN_STATUS,
	SMC_OP_APU_V2_CE1_RUN_STATUS,
	SMC_OP_APU_V2_CE2_RUN_STATUS,
	SMC_OP_APU_V2_CE3_RUN_STATUS,
	SMC_OP_APU_V2_CE0_RUN_STATUS_2,
	SMC_OP_APU_V2_CE1_RUN_STATUS_2,
	SMC_OP_APU_V2_CE2_RUN_STATUS_2,
	SMC_OP_APU_V2_CE3_RUN_STATUS_2,
	SMC_OP_APU_V2_ACE_AXI_MST_WR_STATUS,
	SMC_OP_APU_V2_ACE_AXI_MST_RD_STATUS,
	SMC_OP_APU_V2_CE_NUM
};

struct apu_ce_v2_ops {
	int (*check_apu_exp_irq)(struct mtk_apu *apu);
};





int is_apu_ce_excep_init_v2(void);
int apu_ce_excep_is_compatible_v2(struct platform_device *pdev);
int apu_ce_bin_init_v2(struct platform_device *pdev, struct mtk_apu *apu);
int apu_load_ce_bin_v2(struct platform_device *pdev, struct mtk_apu *apu);
int apu_ce_excep_init_v2(struct platform_device *pdev, struct mtk_apu *apu);
int apu_ce_memmap_v2(struct platform_device *pdev, struct mtk_apu *apu);
void apu_ce_excep_remove_v2(struct platform_device *pdev, struct mtk_apu *apu);
void apu_ce_mrdump_register_v2(struct mtk_apu *apu);
void apu_ce_procfs_init_v2(struct platform_device *pdev,
	struct proc_dir_entry *procfs_root);
void apu_ce_procfs_remove_v2(struct platform_device *pdev,
	struct proc_dir_entry *procfs_root);
uint32_t apu_ce_reg_dump_v2(struct device *dev);
uint32_t apu_ce_sram_dump_v2(struct device *dev);

#endif /* APU_CE_EXCEP_V2_H */
