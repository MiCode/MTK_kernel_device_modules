/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef MT6993_PLAT_H_
#define MT6993_PLAT_H_

#define RV_SYS_COMM_CTRL_0 (0x0)
#define RV_SYS_COMM_CTRL_1 (0x4)
#define RV_SYS_COMM_CTRL_2 (0x8)
#define RV_CORE_COMM_CTRL_C0_0 (0xC)
#define RV_CORE_COMM_CTRL_C0_1 (0x10)
#define RV_CORE_COMM_CTRL_C1_0 (0x14)
#define RV_CORE_COMM_CTRL_C1_1 (0x18)
#define RV_CORE_L2_CTRL_0 (0x1C)
#define RV_CORE_MON_STSTUS_0 (0x20)
#define RV_CORE_MON_PC_T0_0 (0x24)
#define RV_CORE_MON_LR_T0_0 (0x28)
#define RV_CORE_MON_SP_T0_0 (0x2C)
#define RV_CORE_MON_PC_T1_0 (0x30)
#define RV_CORE_MON_LR_T1_0 (0x34)
#define RV_CORE_MON_SP_T1_0 (0x38)
#define RV_CORE_MON_PC_T2_0 (0x3C)
#define RV_CORE_MON_LR_T2_0 (0x40)
#define RV_CORE_MON_SP_T2_0 (0x44)
#define RV_CORE_MON_PC_T3_0 (0x48)
#define RV_CORE_MON_LR_T3_0 (0x4C)
#define RV_CORE_MON_SP_T3_0 (0x50)
#define RV_CORE_MON_TBUF_WPTR_0_P0 (0x54)
#define RV_CORE_MON_STSTUS_1 (0x58)
#define RV_CORE_MON_PC_T0_1 (0x5C)
#define RV_CORE_MON_LR_T0_1 (0x60)
#define RV_CORE_MON_SP_T0_1 (0x64)
#define RV_CORE_MON_PC_T1_1 (0x68)
#define RV_CORE_MON_LR_T1_1 (0x6C)
#define RV_CORE_MON_SP_T1_1 (0x70)
#define RV_CORE_MON_PC_T2_1 (0x74)
#define RV_CORE_MON_LR_T2_1 (0x78)
#define RV_CORE_MON_SP_T2_1 (0x7C)
#define RV_CORE_MON_PC_T3_1 (0x80)
#define RV_CORE_MON_LR_T3_1 (0x84)
#define RV_CORE_MON_SP_T3_1 (0x88)
#define RV_CORE_MON_TBUF_WPTR_1_P0 (0x8C)
#define RV_CORE_STATUS (0x90)

#define UP_CG_EN (0xC4)
#define UP_SYSCTRL (0xC8)

#define UP_SPARE_REG_0 (0x500)
#define UP_SPARE_REG_1 (0x504)
#define UP_SPARE_REG_2 (0x508)
#define UP_SPARE_REG_3 (0x50C)

/* rv_core_comm_ctrl_cx_0 register definition */
#define LOCAL_INT_REQ (1UL << 16)
#define BOOT_FROM_ITCM (1UL << 3)
#define SCRB_EN (1UL << 2)
#define CORE_FETCH_BLOCK (1UL << 1)
#define FORCE_CLK_EN (1UL << 0)

/* up_cg_en */
#define SYS_MON_GATED_BYPASS (1UL << 30)
#define CORE_1_MON_GATED_BYPASS (1UL << 29)
#define CORE_0_MON_GATED_BYPASS (1UL << 28)
#define RV55_CORE_CG_EN (1UL << 4)
#define G2B_CG_EN (1UL << 0)

/* up_sysctrl */
#define RV55_DBG_EN (1UL << 0)
#define SNAPSHOT_RESET (1UL << 4)

#define DEBUG_MEMORY_DUMP_SIZE (16)

#endif /* MT6993_PLAT_H_ */
