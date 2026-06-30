/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef APU_CE_EXCEP_V1_H
#define APU_CE_EXCEP_V1_H

#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include "apu.h"

#define MT6878_ARE_ABNORMAL_IRQ_BIT                   (21)
#define APU_V1_CE_SRAMBASE                    (0x190a0000)
#define APU_V1_CE_REG_DUMP_MAGIC_NUM          (0xA5A5A5A5)
#define APU_V1_CE_0_IRQ_MASK                         (0xF)
#define APU_V1_CE_1_IRQ_MASK                        (0xF0)
#define APU_V1_CE_2_IRQ_MASK                       (0xF00)
#define APU_V1_CE_3_IRQ_MASK                      (0xF000)
#define APU_V1_CE_TASK_JOB_SFT                      (0x10)
#define APU_V1_CE_TASK_JOB_MSK                      (0x1F)
#define APU_V1_CE_MISS_TYPE2_REQ_FLAG_0_MSK        (0x100)
#define APU_V1_CE_MISS_TYPE2_REQ_FLAG_1_MSK        (0x200)
#define APU_V1_CE_MISS_TYPE2_REQ_FLAG_2_MSK        (0x400)
#define APU_V1_CE_MISS_TYPE2_REQ_FLAG_3_MSK        (0x800)
#define APU_V1_CE_NON_ALIGNED_APB_FLAG_MSK     (0x6000000)
#define APU_V1_CE_NON_ALIGNED_APB_OUT_FLAG_MSK (0x2000000)
#define APU_V1_CE_NON_ALIGNED_APB_IN_FLAG_MSK  (0x4000000)
#define APU_V1_CE_APB_ERR_STATUS_CE0_MSK             (0x1)
#define APU_V1_CE_APB_ERR_STATUS_CE1_MSK             (0x2)
#define APU_V1_CE_APB_ERR_STATUS_CE2_MSK             (0x4)
#define APU_V1_CE_APB_ERR_STATUS_CE3_MSK             (0x8)

#define GET_SMC_OP_V1(op1, op2, op3) (op1 | op2 << (8 * 1) | op3 << (8 * 2))

enum {
	SMC_OP_APU_V1_CE_NULL = 0,
	SMC_OP_APU_V1_ACE_ABN_IRQ_FLAG_CE,
	SMC_OP_APU_V1_ACE_ABN_IRQ_FLAG_ACE_SW,
	SMC_OP_APU_V1_ACE_ABN_IRQ_FLAG_USER,
	SMC_OP_APU_V1_ACE_CE0_TASK_ING,
	SMC_OP_APU_V1_ACE_CE1_TASK_ING,
	SMC_OP_APU_V1_ACE_CE2_TASK_ING,
	SMC_OP_APU_V1_ACE_CE3_TASK_ING,
	SMC_OP_APU_V1_CE0_RUN_INSTR,
	SMC_OP_APU_V1_CE1_RUN_INSTR,
	SMC_OP_APU_V1_CE2_RUN_INSTR,
	SMC_OP_APU_V1_CE3_RUN_INSTR,
	SMC_OP_APU_V1_CE0_TIMEOUT_INSTR,
	SMC_OP_APU_V1_CE1_TIMEOUT_INSTR,
	SMC_OP_APU_V1_CE2_TIMEOUT_INSTR,
	SMC_OP_APU_V1_CE3_TIMEOUT_INSTR,
	SMC_OP_APU_V1_ACE_APB_MST_OUT_STATUS_ERR,
	SMC_OP_APU_V1_ACE_APB_MST_IN_STATUS_ERR,
	SMC_OP_APU_V1_CE0_RUN_PC,
	SMC_OP_APU_V1_CE1_RUN_PC,
	SMC_OP_APU_V1_CE2_RUN_PC,
	SMC_OP_APU_V1_CE3_RUN_PC,
	SMC_OP_APU_V1_ACE_CMD_Q_STATUS_0,
	SMC_OP_APU_V1_ACE_CMD_Q_STATUS_1,
	SMC_OP_APU_V1_ACE_CMD_Q_STATUS_2,
	SMC_OP_APU_V1_ACE_CMD_Q_STATUS_3,
	SMC_OP_APU_V1_ACE_CMD_Q_STATUS_4,
	SMC_OP_APU_V1_ACE_CMD_Q_STATUS_5,
	SMC_OP_APU_V1_ACE_CMD_Q_STATUS_6,
	SMC_OP_APU_V1_ACE_CMD_Q_STATUS_7,
	SMC_OP_APU_V1_CE0_STEP,
	SMC_OP_APU_V1_CE1_STEP,
	SMC_OP_APU_V1_CE2_STEP,
	SMC_OP_APU_V1_CE3_STEP,
	SMC_OP_APU_V1_CE_NUM
};

struct apu_ce_v1_ops {
	int (*check_apu_exp_irq)(struct mtk_apu *apu);
};

int is_apu_ce_excep_init_v1(void);
int apu_ce_excep_is_compatible_v1(struct platform_device *pdev);
int apu_ce_excep_init_v1(struct platform_device *pdev, struct mtk_apu *apu);
void apu_ce_excep_remove_v1(struct platform_device *pdev, struct mtk_apu *apu);
void apu_ce_mrdump_register_v1(struct mtk_apu *apu);
void apu_ce_procfs_init_v1(struct platform_device *pdev,
	struct proc_dir_entry *procfs_root);
void apu_ce_procfs_remove_v1(struct platform_device *pdev,
	struct proc_dir_entry *procfs_root);
uint32_t apu_ce_reg_dump_v1(struct device *dev);
uint32_t apu_ce_sram_dump_v1(struct device *dev);

#endif /* APU_CE_EXCEP_V1_H */
