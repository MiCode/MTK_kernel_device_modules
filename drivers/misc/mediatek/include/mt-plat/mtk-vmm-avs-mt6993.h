/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#ifndef __hvs_REGS_H__
#define __hvs_REGS_H__

#include <linux/elf.h>
#include <linux/elfcore.h>
#include <linux/stdarg.h>
#include <mt-plat/aee.h>
#include <linux/types.h>
#include <linux/io.h>

#define PACKING __packed

// ----------------- efuse related -------------------
#define EFUSE_ISP_VMIN_REG			  readl(vmm_regs.vmm_efuse_va + 0x05C)
#define EFUSE_ISP_VB_VERSION			(readl(vmm_regs.vmm_efuse_va + 0x064) & 0x1f)
#define EFUSE_VDE_VMIN_REG			  readl(vmm_regs.vmm_efuse_va + 0x074)
#define EFUSE_CAM_DMIN_OP6570		   readl(vmm_regs.vmm_efuse_va + 0x12C)
#define EFUSE_CAM_DMIN_OP5760		   readl(vmm_regs.vmm_efuse_va + 0x130)
#define EFUSE_IMG_DMIN_OP6570		   readl(vmm_regs.vmm_efuse_va + 0x11C)
#define EFUSE_IMG_DMIN_OP5760		   readl(vmm_regs.vmm_efuse_va + 0x120)
#define EFUSE_IPE_DMIN_OP6570		   readl(vmm_regs.vmm_efuse_va + 0x124)
#define EFUSE_IPE_DMIN_OP5760		   readl(vmm_regs.vmm_efuse_va + 0x128)
#define EFUSE_ISP_CAM_SFT			readl(vmm_regs.vmm_efuse_va + 0x118)
#define EFUSE_ISP_IPS_REG			readl(vmm_regs.vmm_efuse_va + 0x114)

/* ISP IPS define */
#define EFUSE_CAM_IPS_VAL			((EFUSE_ISP_IPS_REG) & 0xFF)
#define EFUSE_IMG_IPS_VAL			(((EFUSE_ISP_IPS_REG) >> 8) & 0xFF)
#define IPS_LOWER_THRESHOLD				(106)
#define IPS_UPPER_THRESHOLD				(114)

/* ISP Efuse define */
#define ISP_575_VBIN_OFFSET 15
#define ISP_575_VBIN_VAL (((EFUSE_ISP_VMIN_REG) >> ISP_575_VBIN_OFFSET) & 0x1f)
#define ISP_600_VBIN_OFFSET 10
#define ISP_600_VBIN_VAL (((EFUSE_ISP_VMIN_REG) >> ISP_600_VBIN_OFFSET) & 0x1f)
#define ISP_650_VBIN_OFFSET 5
#define ISP_650_VBIN_VAL (((EFUSE_ISP_VMIN_REG) >> ISP_650_VBIN_OFFSET) & 0x1f)
#define ISP_700_VBIN_OFFSET 0
#define ISP_700_VBIN_VAL (((EFUSE_ISP_VMIN_REG) >> ISP_700_VBIN_OFFSET) & 0x1f)

/* ISP CAM Efuse sft define */
#define ISP_SFT_SIGNOFF_FLAG_OFFSET 25
#define ISP_SFT_SIGNOFF_FLAG ((EFUSE_ISP_CAM_SFT >> ISP_SFT_SIGNOFF_FLAG_OFFSET) & 0xF)
#define ISP_SFT_VERSION_OFFSET 20
#define EFUSE_ISP_SFT_VERSION ((EFUSE_ISP_CAM_SFT >> ISP_SFT_VERSION_OFFSET) & 0x1F)
#define ISP_CAM_575_SFT_OFFSET 15
#define ISP_CAM_575_SFT_MARGIN ((EFUSE_ISP_CAM_SFT >> ISP_CAM_575_SFT_OFFSET) & 0x1f)
#define ISP_CAM_600_SFT_OFFSET 10
#define ISP_CAM_600_SFT_MARGIN ((EFUSE_ISP_CAM_SFT >> ISP_CAM_600_SFT_OFFSET) & 0x1f)
#define ISP_CAM_650_SFT_OFFSET 5
#define ISP_CAM_650_SFT_MARGIN ((EFUSE_ISP_CAM_SFT >> ISP_CAM_650_SFT_OFFSET) & 0x1f)
#define ISP_CAM_700_SFT_OFFSET 0
#define ISP_CAM_700_SFT_MARGIN ((EFUSE_ISP_CAM_SFT >> ISP_CAM_700_SFT_OFFSET) & 0x1f)

/* VDE Efuse define */
#define VDE_575_VBIN_OFFSET 0
#define VDE_575_VBIN_VAL (((EFUSE_VDE_VMIN_REG) >> VDE_575_VBIN_OFFSET) & 0x1f)
#define VDE_600_VBIN_OFFSET 5
#define VDE_600_VBIN_VAL (((EFUSE_VDE_VMIN_REG) >> VDE_600_VBIN_OFFSET) & 0x1f)
#define VDE_650_VBIN_OFFSET 10
#define VDE_650_VBIN_VAL (((EFUSE_VDE_VMIN_REG) >> VDE_650_VBIN_OFFSET) & 0x1f)
#define VDE_700_VBIN_OFFSET 15
#define VDE_700_VBIN_VAL (((EFUSE_VDE_VMIN_REG) >> VDE_700_VBIN_OFFSET) & 0x1f)

/* others */
#define FORCE_ZERO					  (0)
#define VMM_ONE_STEP_MARGIN			 (5000)
#define SIGNED_OFF_575V				 (575000)
#define SIGNED_OFF_575V_NORM			(SIGNED_OFF_575V/VMM_ONE_STEP_MARGIN)
#define SIGNED_OFF_600V				 (600000)
#define SIGNED_OFF_600V_NORM			(SIGNED_OFF_600V/VMM_ONE_STEP_MARGIN)
#define SIGNED_OFF_650V				 (650000)
#define SIGNED_OFF_650V_NORM			(SIGNED_OFF_650V/VMM_ONE_STEP_MARGIN)
#define SIGNED_OFF_700V				 (700000)
#define SIGNED_OFF_700V_NORM		   (SIGNED_OFF_700V/VMM_ONE_STEP_MARGIN)
#define ATE_BASE_BIN					(4)
#define ATE_575V						(546250)  /* BIN4 */
#define ATE_600V						(570000)  /* BIN4 */
#define ATE_650V						(617500)  /* BIN4 */
#define ATE_700V						(665000)  /* BIN4 */
// ----------------- hvs Bit unsigned int Definitions -------------------

enum vmm_dbg_list {
	VMM_DBG_FORCE_BUCK_OFF = 0,
	VMM_DBG_FORCE_BUCK_ON = 1,
	VMM_DBG_FORCE_VOL_START,
	VMM_DBG_FORCE_VOL0 = VMM_DBG_FORCE_VOL_START,
	VMM_DBG_FORCE_VOL1,
	VMM_DBG_FORCE_VOL2,
	VMM_DBG_FORCE_VOL3,
	VMM_DBG_FORCE_VOL4,
	VMM_DBG_FORCE_VOL5,
	VMM_DBG_DISABLE_CVFS,
	VMM_DBG_DISABLE_AVS,
	VMM_DBG_DUMP_VMM,
};

#define VMM_CVFS_BASE				   (vmm_regs.vmm_cvfs_va)

union AVS_PHASE1_VMIN_1_VAL_t {
	struct {
		unsigned int AVS_PHASE1_OPP0 : 8;
		unsigned int AVS_PHASE1_OPP1 : 8;
		unsigned int AVS_PHASE1_OPP2 : 8;
		unsigned int AVS_PHASE1_OPP3 : 8;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_PHASE1_VMIN_1_REG  (VMM_CVFS_BASE + 0x0)
union AVS_PHASE1_VMIN_1_VAL_t   AVS_PHASE1_VMIN_1_VAL;

union AVS_PHASE1_VMIN_2_VAL_t {
	struct {
		unsigned int AVS_PHASE1_OPP4 : 8;
		unsigned int AVS_PHASE1_OPP5 : 8;
		unsigned int AVS_PHASE1_OPP6 : 8;
		unsigned int AVS_PHASE1_OPP7 : 8;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_PHASE1_VMIN_2_REG  (VMM_CVFS_BASE + 0x4)
union AVS_PHASE1_VMIN_2_VAL_t   AVS_PHASE1_VMIN_2_VAL;

union AVS_PHASE1_VMIN_3_VAL_t {
	struct {
		unsigned int AVS_PHASE1_OPP8 : 8;
		unsigned int AVS_PHASE1_OPP9 : 8;
		unsigned int AVS_PHASE1_OPP10 : 8;
		unsigned int AVS_PHASE1_OPP11 : 8;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_PHASE1_VMIN_3_REG  (VMM_CVFS_BASE + 0x8)
union AVS_PHASE1_VMIN_3_VAL_t   AVS_PHASE1_VMIN_3_VAL;

union AVS_PHASE1_VMIN_4_VAL_t {
	struct {
		unsigned int AVS_PHASE1_OPP12 : 8;
		unsigned int AVS_PHASE1_OPP13 : 8;
		unsigned int AVS_PHASE1_OPP14 : 8;
		unsigned int AVS_PHASE1_OPP15 : 8;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_PHASE1_VMIN_4_REG  (VMM_CVFS_BASE + 0xC)
union AVS_PHASE1_VMIN_4_VAL_t   AVS_PHASE1_VMIN_4_VAL;

union AVS_MARGIN_TEMP_OPP0_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP0_Z1 : 5;
		unsigned int rsv_5				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP0_Z2 : 5;
		unsigned int rsv_13				  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP0_Z3 : 5;
		unsigned int rsv_21				  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP0_Z4 : 5;
		unsigned int rsv_29				  : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP0_1_REG  (VMM_CVFS_BASE + 0x10)
union AVS_MARGIN_TEMP_OPP0_1_VAL_t   AVS_MARGIN_TEMP_OPP0_1_VAL;

union AVS_MARGIN_TEMP_OPP0_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP0_Z5 : 5;
		unsigned int rsv_5				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP0_Z6 : 5;
		unsigned int rsv_13				  : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP0_2_REG  (VMM_CVFS_BASE + 0x14)
union AVS_MARGIN_TEMP_OPP0_2_VAL_t   AVS_MARGIN_TEMP_OPP0_2_VAL;

union AVS_MARGIN_TEMP_OPP1_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP1_Z1 : 5;
		unsigned int rsv_5				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP1_Z2 : 5;
		unsigned int rsv_13				  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP1_Z3 : 5;
		unsigned int rsv_21				  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP1_Z4 : 5;
		unsigned int rsv_29				  : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP1_1_REG  (VMM_CVFS_BASE + 0x18)
union AVS_MARGIN_TEMP_OPP1_1_VAL_t   AVS_MARGIN_TEMP_OPP1_1_VAL;

union AVS_MARGIN_TEMP_OPP1_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP1_Z5 : 5;
		unsigned int rsv_5				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP1_Z6 : 5;
		unsigned int rsv_13				  : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP1_2_REG  (VMM_CVFS_BASE + 0x1C)
union AVS_MARGIN_TEMP_OPP1_2_VAL_t   AVS_MARGIN_TEMP_OPP1_2_VAL;

union AVS_MARGIN_TEMP_OPP2_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP2_Z1 : 5;
		unsigned int rsv_5				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP2_Z2 : 5;
		unsigned int rsv_13				  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP2_Z3 : 5;
		unsigned int rsv_21				  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP2_Z4 : 5;
		unsigned int rsv_29				  : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP2_1_REG  (VMM_CVFS_BASE + 0x20)
union AVS_MARGIN_TEMP_OPP2_1_VAL_t   AVS_MARGIN_TEMP_OPP2_1_VAL;

union AVS_MARGIN_TEMP_OPP2_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP2_Z5 : 5;
		unsigned int rsv_5				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP2_Z6 : 5;
		unsigned int rsv_13				  : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP2_2_REG  (VMM_CVFS_BASE + 0x24)
union AVS_MARGIN_TEMP_OPP2_2_VAL_t   AVS_MARGIN_TEMP_OPP2_2_VAL;

union AVS_MARGIN_TEMP_OPP3_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP3_Z1 : 5;
		unsigned int rsv_5				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP3_Z2 : 5;
		unsigned int rsv_13				  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP3_Z3 : 5;
		unsigned int rsv_21				  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP3_Z4 : 5;
		unsigned int rsv_29				  : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP3_1_REG  (VMM_CVFS_BASE + 0x28)
union AVS_MARGIN_TEMP_OPP3_1_VAL_t   AVS_MARGIN_TEMP_OPP3_1_VAL;

union AVS_MARGIN_TEMP_OPP3_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP3_Z5 : 5;
		unsigned int rsv_5				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP3_Z6 : 5;
		unsigned int rsv_13				  : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP3_2_REG  (VMM_CVFS_BASE + 0x2C)
union AVS_MARGIN_TEMP_OPP3_2_VAL_t   AVS_MARGIN_TEMP_OPP3_2_VAL;

union AVS_MARGIN_TEMP_OPP4_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP4_Z1 : 5;
		unsigned int rsv_5				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP4_Z2 : 5;
		unsigned int rsv_13				  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP4_Z3 : 5;
		unsigned int rsv_21				  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP4_Z4 : 5;
		unsigned int rsv_29				  : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP4_1_REG  (VMM_CVFS_BASE + 0x30)
union AVS_MARGIN_TEMP_OPP4_1_VAL_t   AVS_MARGIN_TEMP_OPP4_1_VAL;

union AVS_MARGIN_TEMP_OPP4_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP4_Z5 : 5;
		unsigned int rsv_5				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP4_Z6 : 5;
		unsigned int rsv_13				  : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP4_2_REG  (VMM_CVFS_BASE + 0x34)
union AVS_MARGIN_TEMP_OPP4_2_VAL_t   AVS_MARGIN_TEMP_OPP4_2_VAL;

union AVS_MARGIN_TEMP_OPP5_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP5_Z1 : 5;
		unsigned int rsv_5				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP5_Z2 : 5;
		unsigned int rsv_13				  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP5_Z3 : 5;
		unsigned int rsv_21				  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP5_Z4 : 5;
		unsigned int rsv_29				  : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP5_1_REG  (VMM_CVFS_BASE + 0x38)
union AVS_MARGIN_TEMP_OPP5_1_VAL_t   AVS_MARGIN_TEMP_OPP5_1_VAL;

union AVS_MARGIN_TEMP_OPP5_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP5_Z5 : 5;
		unsigned int rsv_5				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP5_Z6 : 5;
		unsigned int rsv_13				  : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP5_2_REG  (VMM_CVFS_BASE + 0x3C)
union AVS_MARGIN_TEMP_OPP5_2_VAL_t   AVS_MARGIN_TEMP_OPP5_2_VAL;

union AVS_MARGIN_TEMP_OPP6_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP6_Z1 : 5;
		unsigned int rsv_5				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP6_Z2 : 5;
		unsigned int rsv_13				  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP6_Z3 : 5;
		unsigned int rsv_21				  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP6_Z4 : 5;
		unsigned int rsv_29				  : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP6_1_REG  (VMM_CVFS_BASE + 0x40)
union AVS_MARGIN_TEMP_OPP6_1_VAL_t   AVS_MARGIN_TEMP_OPP6_1_VAL;

union AVS_MARGIN_TEMP_OPP6_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP6_Z5 : 5;
		unsigned int rsv_5				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP6_Z6 : 5;
		unsigned int rsv_13				  : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP6_2_REG  (VMM_CVFS_BASE + 0x44)
union AVS_MARGIN_TEMP_OPP6_2_VAL_t   AVS_MARGIN_TEMP_OPP6_2_VAL;

union AVS_MARGIN_TEMP_OPP7_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP7_Z1 : 5;
		unsigned int rsv_5				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP7_Z2 : 5;
		unsigned int rsv_13				  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP7_Z3 : 5;
		unsigned int rsv_21				  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP7_Z4 : 5;
		unsigned int rsv_29				  : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP7_1_REG  (VMM_CVFS_BASE + 0x48)
union AVS_MARGIN_TEMP_OPP7_1_VAL_t   AVS_MARGIN_TEMP_OPP7_1_VAL;

union AVS_MARGIN_TEMP_OPP7_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP7_Z5 : 5;
		unsigned int rsv_5				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP7_Z6 : 5;
		unsigned int rsv_13				  : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP7_2_REG  (VMM_CVFS_BASE + 0x4C)
union AVS_MARGIN_TEMP_OPP7_2_VAL_t   AVS_MARGIN_TEMP_OPP7_2_VAL;

union AVS_MARGIN_TEMP_OPP8_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP8_Z1 : 5;
		unsigned int rsv_5				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP8_Z2 : 5;
		unsigned int rsv_13				  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP8_Z3 : 5;
		unsigned int rsv_21				  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP8_Z4 : 5;
		unsigned int rsv_29				  : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP8_1_REG  (VMM_CVFS_BASE + 0x50)
union AVS_MARGIN_TEMP_OPP8_1_VAL_t   AVS_MARGIN_TEMP_OPP8_1_VAL;

union AVS_MARGIN_TEMP_OPP8_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP8_Z5 : 5;
		unsigned int rsv_5				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP8_Z6 : 5;
		unsigned int rsv_13				  : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP8_2_REG  (VMM_CVFS_BASE + 0x54)
union AVS_MARGIN_TEMP_OPP8_2_VAL_t   AVS_MARGIN_TEMP_OPP8_2_VAL;

union AVS_MARGIN_TEMP_OPP9_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP9_Z1 : 5;
		unsigned int rsv_5				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP9_Z2 : 5;
		unsigned int rsv_13				  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP9_Z3 : 5;
		unsigned int rsv_21				  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP9_Z4 : 5;
		unsigned int rsv_29				  : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP9_1_REG  (VMM_CVFS_BASE + 0x58)
union AVS_MARGIN_TEMP_OPP9_1_VAL_t   AVS_MARGIN_TEMP_OPP9_1_VAL;

union AVS_MARGIN_TEMP_OPP9_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP9_Z5 : 5;
		unsigned int rsv_5				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP9_Z6 : 5;
		unsigned int rsv_13				  : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP9_2_REG  (VMM_CVFS_BASE + 0x5C)
union AVS_MARGIN_TEMP_OPP9_2_VAL_t   AVS_MARGIN_TEMP_OPP9_2_VAL;

union AVS_MARGIN_TEMP_OPP10_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP10_Z1 : 5;
		unsigned int rsv_5					: 3;
		unsigned int AVS_MARGIN_TEMP_OPP10_Z2 : 5;
		unsigned int rsv_13				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP10_Z3 : 5;
		unsigned int rsv_21				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP10_Z4 : 5;
		unsigned int rsv_29				   : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP10_1_REG  (VMM_CVFS_BASE + 0x60)
union AVS_MARGIN_TEMP_OPP10_1_VAL_t   AVS_MARGIN_TEMP_OPP10_1_VAL;

union AVS_MARGIN_TEMP_OPP10_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP10_Z5 : 5;
		unsigned int rsv_5					: 3;
		unsigned int AVS_MARGIN_TEMP_OPP10_Z6 : 5;
		unsigned int rsv_13				   : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP10_2_REG  (VMM_CVFS_BASE + 0x64)
union AVS_MARGIN_TEMP_OPP10_2_VAL_t   AVS_MARGIN_TEMP_OPP10_2_VAL;

union AVS_MARGIN_TEMP_OPP11_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP11_Z1 : 5;
		unsigned int rsv_5					: 3;
		unsigned int AVS_MARGIN_TEMP_OPP11_Z2 : 5;
		unsigned int rsv_13				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP11_Z3 : 5;
		unsigned int rsv_21				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP11_Z4 : 5;
		unsigned int rsv_29				   : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP11_1_REG  (VMM_CVFS_BASE + 0x68)
union AVS_MARGIN_TEMP_OPP11_1_VAL_t   AVS_MARGIN_TEMP_OPP11_1_VAL;

union AVS_MARGIN_TEMP_OPP11_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP11_Z5 : 5;
		unsigned int rsv_5					: 3;
		unsigned int AVS_MARGIN_TEMP_OPP11_Z6 : 5;
		unsigned int rsv_13				   : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP11_2_REG  (VMM_CVFS_BASE + 0x6C)
union AVS_MARGIN_TEMP_OPP11_2_VAL_t   AVS_MARGIN_TEMP_OPP11_2_VAL;

union AVS_MARGIN_TEMP_OPP12_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP12_Z1 : 5;
		unsigned int rsv_5					: 3;
		unsigned int AVS_MARGIN_TEMP_OPP12_Z2 : 5;
		unsigned int rsv_13				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP12_Z3 : 5;
		unsigned int rsv_21				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP12_Z4 : 5;
		unsigned int rsv_29				   : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP12_1_REG  (VMM_CVFS_BASE + 0x70)
union AVS_MARGIN_TEMP_OPP12_1_VAL_t   AVS_MARGIN_TEMP_OPP12_1_VAL;

union AVS_MARGIN_TEMP_OPP12_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP12_Z5 : 5;
		unsigned int rsv_5					: 3;
		unsigned int AVS_MARGIN_TEMP_OPP12_Z6 : 5;
		unsigned int rsv_13				   : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP12_2_REG  (VMM_CVFS_BASE + 0x74)
union AVS_MARGIN_TEMP_OPP12_2_VAL_t   AVS_MARGIN_TEMP_OPP12_2_VAL;

union AVS_MARGIN_TEMP_OPP13_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP13_Z1 : 5;
		unsigned int rsv_5					: 3;
		unsigned int AVS_MARGIN_TEMP_OPP13_Z2 : 5;
		unsigned int rsv_13				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP13_Z3 : 5;
		unsigned int rsv_21				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP13_Z4 : 5;
		unsigned int rsv_29				   : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP13_1_REG  (VMM_CVFS_BASE + 0x78)
union AVS_MARGIN_TEMP_OPP13_1_VAL_t   AVS_MARGIN_TEMP_OPP13_1_VAL;

union AVS_MARGIN_TEMP_OPP13_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP13_Z5 : 5;
		unsigned int rsv_5					: 3;
		unsigned int AVS_MARGIN_TEMP_OPP13_Z6 : 5;
		unsigned int rsv_13				   : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP13_2_REG  (VMM_CVFS_BASE + 0x7C)
union AVS_MARGIN_TEMP_OPP13_2_VAL_t   AVS_MARGIN_TEMP_OPP13_2_VAL;

union AVS_MARGIN_TEMP_OPP14_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP14_Z1 : 5;
		unsigned int rsv_5					: 3;
		unsigned int AVS_MARGIN_TEMP_OPP14_Z2 : 5;
		unsigned int rsv_13				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP14_Z3 : 5;
		unsigned int rsv_21				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP14_Z4 : 5;
		unsigned int rsv_29				   : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP14_1_REG  (VMM_CVFS_BASE + 0x80)
union AVS_MARGIN_TEMP_OPP14_1_VAL_t   AVS_MARGIN_TEMP_OPP14_1_VAL;

union AVS_MARGIN_TEMP_OPP14_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP14_Z5 : 5;
		unsigned int rsv_5					: 3;
		unsigned int AVS_MARGIN_TEMP_OPP14_Z6 : 5;
		unsigned int rsv_13				   : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP14_2_REG  (VMM_CVFS_BASE + 0x84)
union AVS_MARGIN_TEMP_OPP14_2_VAL_t   AVS_MARGIN_TEMP_OPP14_2_VAL;

union AVS_MARGIN_TEMP_OPP15_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP15_Z1 : 5;
		unsigned int rsv_5					: 3;
		unsigned int AVS_MARGIN_TEMP_OPP15_Z2 : 5;
		unsigned int rsv_13				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP15_Z3 : 5;
		unsigned int rsv_21				   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP15_Z4 : 5;
		unsigned int rsv_29				   : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP15_1_REG  (VMM_CVFS_BASE + 0x88)
union AVS_MARGIN_TEMP_OPP15_1_VAL_t   AVS_MARGIN_TEMP_OPP15_1_VAL;

union AVS_MARGIN_TEMP_OPP15_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP15_Z5 : 5;
		unsigned int rsv_5					: 3;
		unsigned int AVS_MARGIN_TEMP_OPP15_Z6 : 5;
		unsigned int rsv_13				   : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP15_2_REG  (VMM_CVFS_BASE + 0x8C)
union AVS_MARGIN_TEMP_OPP15_2_VAL_t   AVS_MARGIN_TEMP_OPP15_2_VAL;

union AVS_MARGIN_AGING_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_AGING_OPP0 : 3;
		unsigned int rsv_3				 : 1;
		unsigned int AVS_MARGIN_AGING_OPP1 : 3;
		unsigned int rsv_7				 : 1;
		unsigned int AVS_MARGIN_AGING_OPP2 : 3;
		unsigned int rsv_11				: 1;
		unsigned int AVS_MARGIN_AGING_OPP3 : 3;
		unsigned int rsv_15				: 1;
		unsigned int AVS_MARGIN_AGING_OPP4 : 3;
		unsigned int rsv_19				: 1;
		unsigned int AVS_MARGIN_AGING_OPP5 : 3;
		unsigned int rsv_23				: 1;
		unsigned int AVS_MARGIN_AGING_OPP6 : 3;
		unsigned int rsv_27				: 1;
		unsigned int AVS_MARGIN_AGING_OPP7 : 3;
		unsigned int rsv_31				: 1;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_AGING_1_REG  (VMM_CVFS_BASE + 0x90)
union AVS_MARGIN_AGING_1_VAL_t   AVS_MARGIN_AGING_1_VAL;

union AVS_MARGIN_AGING_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_AGING_OPP8 : 3;
		unsigned int rsv_3				 : 1;
		unsigned int AVS_MARGIN_AGING_OPP9 : 3;
		unsigned int rsv_7				 : 1;
		unsigned int AVS_MARGIN_AGING_OPP10 : 3;
		unsigned int rsv_11				: 1;
		unsigned int AVS_MARGIN_AGING_OPP11 : 3;
		unsigned int rsv_15				: 1;
		unsigned int AVS_MARGIN_AGING_OPP12 : 3;
		unsigned int rsv_19				: 1;
		unsigned int AVS_MARGIN_AGING_OPP13 : 3;
		unsigned int rsv_23				: 1;
		unsigned int AVS_MARGIN_AGING_OPP14 : 3;
		unsigned int rsv_27				: 1;
		unsigned int AVS_MARGIN_AGING_OPP15 : 3;
		unsigned int rsv_31				: 1;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_AGING_2_REG  (VMM_CVFS_BASE + 0x94)
union AVS_MARGIN_AGING_2_VAL_t   AVS_MARGIN_AGING_2_VAL;

union AVS_PHASE1_VMIN_1_partial_VAL_t {
	struct {
		unsigned int AVS_PHASE1_OPP0_partial : 8;
		unsigned int AVS_PHASE1_OPP1_partial : 8;
		unsigned int AVS_PHASE1_OPP2_partial : 8;
		unsigned int AVS_PHASE1_OPP3_partial : 8;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_PHASE1_VMIN_1_partial_REG (VMM_CVFS_BASE + 0x100)
union AVS_PHASE1_VMIN_1_partial_VAL_t   AVS_PHASE1_VMIN_1_partial_VAL;

union AVS_PHASE1_VMIN_2_partial_VAL_t {
	struct {
		unsigned int AVS_PHASE1_OPP4_partial : 8;
		unsigned int AVS_PHASE1_OPP5_partial : 8;
		unsigned int AVS_PHASE1_OPP6_partial : 8;
		unsigned int AVS_PHASE1_OPP7_partial : 8;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_PHASE1_VMIN_2_partial_REG (VMM_CVFS_BASE + 0x104)
union AVS_PHASE1_VMIN_2_partial_VAL_t   AVS_PHASE1_VMIN_2_partial_VAL;

union AVS_PHASE1_VMIN_3_partial_VAL_t {
	struct {
		unsigned int AVS_PHASE1_OPP8_partial : 8;
		unsigned int AVS_PHASE1_OPP9_partial : 8;
		unsigned int AVS_PHASE1_OPP10_partial : 8;
		unsigned int AVS_PHASE1_OPP11_partial : 8;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_PHASE1_VMIN_3_partial_REG (VMM_CVFS_BASE + 0x108)
union AVS_PHASE1_VMIN_3_partial_VAL_t   AVS_PHASE1_VMIN_3_partial_VAL;

union AVS_PHASE1_VMIN_4_partial_VAL_t {
	struct {
		unsigned int AVS_PHASE1_OPP12_partial : 8;
		unsigned int AVS_PHASE1_OPP13_partial : 8;
		unsigned int AVS_PHASE1_OPP14_partial : 8;
		unsigned int AVS_PHASE1_OPP15_partial : 8;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_PHASE1_VMIN_4_partial_REG (VMM_CVFS_BASE + 0x10C)
union AVS_PHASE1_VMIN_4_partial_VAL_t   AVS_PHASE1_VMIN_4_partial_VAL;

union AVS_MARGIN_TEMP_OPP0_partial_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP0_Z1_partial : 5;
		unsigned int rsv_5						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP0_Z2_partial : 5;
		unsigned int rsv_13						  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP0_Z3_partial : 5;
		unsigned int rsv_21						  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP0_Z4_partial : 5;
		unsigned int rsv_29						  : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP0_partial_1_REG (VMM_CVFS_BASE + 0x110)
union AVS_MARGIN_TEMP_OPP0_partial_1_VAL_t   AVS_MARGIN_TEMP_OPP0_partial_1_VAL;

union AVS_MARGIN_TEMP_OPP0_partial_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP0_Z5_partial : 5;
		unsigned int rsv_5						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP0_Z6_partial : 5;
		unsigned int rsv_13						  : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP0_partial_2_REG (VMM_CVFS_BASE + 0x114)
union AVS_MARGIN_TEMP_OPP0_partial_2_VAL_t   AVS_MARGIN_TEMP_OPP0_partial_2_VAL;

union AVS_MARGIN_TEMP_OPP1_partial_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP1_Z1_partial : 5;
		unsigned int rsv_5						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP1_Z2_partial : 5;
		unsigned int rsv_13						  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP1_Z3_partial : 5;
		unsigned int rsv_21						  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP1_Z4_partial : 5;
		unsigned int rsv_29						  : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP1_partial_1_REG (VMM_CVFS_BASE + 0x118)
union AVS_MARGIN_TEMP_OPP1_partial_1_VAL_t   AVS_MARGIN_TEMP_OPP1_partial_1_VAL;

union AVS_MARGIN_TEMP_OPP1_partial_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP1_Z5_partial : 5;
		unsigned int rsv_5						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP1_Z6_partial : 5;
		unsigned int rsv_13						  : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP1_partial_2_REG (VMM_CVFS_BASE + 0x11C)
union AVS_MARGIN_TEMP_OPP1_partial_2_VAL_t   AVS_MARGIN_TEMP_OPP1_partial_2_VAL;

union AVS_MARGIN_TEMP_OPP2_partial_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP2_Z1_partial : 5;
		unsigned int rsv_5						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP2_Z2_partial : 5;
		unsigned int rsv_13						  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP2_Z3_partial : 5;
		unsigned int rsv_21						  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP2_Z4_partial : 5;
		unsigned int rsv_29						  : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP2_partial_1_REG (VMM_CVFS_BASE + 0x120)
union AVS_MARGIN_TEMP_OPP2_partial_1_VAL_t   AVS_MARGIN_TEMP_OPP2_partial_1_VAL;

union AVS_MARGIN_TEMP_OPP2_partial_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP2_Z5_partial : 5;
		unsigned int rsv_5						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP2_Z6_partial : 5;
		unsigned int rsv_13						  : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP2_partial_2_REG (VMM_CVFS_BASE + 0x124)
union AVS_MARGIN_TEMP_OPP2_partial_2_VAL_t   AVS_MARGIN_TEMP_OPP2_partial_2_VAL;

union AVS_MARGIN_TEMP_OPP3_partial_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP3_Z1_partial : 5;
		unsigned int rsv_5						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP3_Z2_partial : 5;
		unsigned int rsv_13						  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP3_Z3_partial : 5;
		unsigned int rsv_21						  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP3_Z4_partial : 5;
		unsigned int rsv_29						  : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP3_partial_1_REG (VMM_CVFS_BASE + 0x128)
union AVS_MARGIN_TEMP_OPP3_partial_1_VAL_t   AVS_MARGIN_TEMP_OPP3_partial_1_VAL;

union AVS_MARGIN_TEMP_OPP3_partial_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP3_Z5_partial : 5;
		unsigned int rsv_5						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP3_Z6_partial : 5;
		unsigned int rsv_13						  : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP3_partial_2_REG (VMM_CVFS_BASE + 0x12C)
union AVS_MARGIN_TEMP_OPP3_partial_2_VAL_t   AVS_MARGIN_TEMP_OPP3_partial_2_VAL;

union AVS_MARGIN_TEMP_OPP4_partial_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP4_Z1_partial : 5;
		unsigned int rsv_5						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP4_Z2_partial : 5;
		unsigned int rsv_13						  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP4_Z3_partial : 5;
		unsigned int rsv_21						  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP4_Z4_partial : 5;
		unsigned int rsv_29						  : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP4_partial_1_REG (VMM_CVFS_BASE + 0x130)
union AVS_MARGIN_TEMP_OPP4_partial_1_VAL_t   AVS_MARGIN_TEMP_OPP4_partial_1_VAL;

union AVS_MARGIN_TEMP_OPP4_partial_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP4_Z5_partial : 5;
		unsigned int rsv_5						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP4_Z6_partial : 5;
		unsigned int rsv_13						  : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP4_partial_2_REG (VMM_CVFS_BASE + 0x134)
union AVS_MARGIN_TEMP_OPP4_partial_2_VAL_t   AVS_MARGIN_TEMP_OPP4_partial_2_VAL;

union AVS_MARGIN_TEMP_OPP5_partial_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP5_Z1_partial : 5;
		unsigned int rsv_5						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP5_Z2_partial : 5;
		unsigned int rsv_13						  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP5_Z3_partial : 5;
		unsigned int rsv_21						  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP5_Z4_partial : 5;
		unsigned int rsv_29						  : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP5_partial_1_REG (VMM_CVFS_BASE + 0x138)
union AVS_MARGIN_TEMP_OPP5_partial_1_VAL_t   AVS_MARGIN_TEMP_OPP5_partial_1_VAL;

union AVS_MARGIN_TEMP_OPP5_partial_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP5_Z5_partial : 5;
		unsigned int rsv_5						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP5_Z6_partial : 5;
		unsigned int rsv_13						  : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP5_partial_2_REG (VMM_CVFS_BASE + 0x13C)
union AVS_MARGIN_TEMP_OPP5_partial_2_VAL_t   AVS_MARGIN_TEMP_OPP5_partial_2_VAL;

union AVS_MARGIN_TEMP_OPP6_partial_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP6_Z1_partial : 5;
		unsigned int rsv_5						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP6_Z2_partial : 5;
		unsigned int rsv_13						  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP6_Z3_partial : 5;
		unsigned int rsv_21						  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP6_Z4_partial : 5;
		unsigned int rsv_29						  : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP6_partial_1_REG (VMM_CVFS_BASE + 0x140)
union AVS_MARGIN_TEMP_OPP6_partial_1_VAL_t   AVS_MARGIN_TEMP_OPP6_partial_1_VAL;

union AVS_MARGIN_TEMP_OPP6_partial_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP6_Z5_partial : 5;
		unsigned int rsv_5						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP6_Z6_partial : 5;
		unsigned int rsv_13						  : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP6_partial_2_REG (VMM_CVFS_BASE + 0x144)
union AVS_MARGIN_TEMP_OPP6_partial_2_VAL_t   AVS_MARGIN_TEMP_OPP6_partial_2_VAL;

union AVS_MARGIN_TEMP_OPP7_partial_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP7_Z1_partial : 5;
		unsigned int rsv_5						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP7_Z2_partial : 5;
		unsigned int rsv_13						  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP7_Z3_partial : 5;
		unsigned int rsv_21						  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP7_Z4_partial : 5;
		unsigned int rsv_29						  : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP7_partial_1_REG (VMM_CVFS_BASE + 0x148)
union AVS_MARGIN_TEMP_OPP7_partial_1_VAL_t   AVS_MARGIN_TEMP_OPP7_partial_1_VAL;

union AVS_MARGIN_TEMP_OPP7_partial_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP7_Z5_partial : 5;
		unsigned int rsv_5						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP7_Z6_partial : 5;
		unsigned int rsv_13						  : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP7_partial_2_REG (VMM_CVFS_BASE + 0x14C)
union AVS_MARGIN_TEMP_OPP7_partial_2_VAL_t   AVS_MARGIN_TEMP_OPP7_partial_2_VAL;

union AVS_MARGIN_TEMP_OPP8_partial_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP8_Z1_partial : 5;
		unsigned int rsv_5						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP8_Z2_partial : 5;
		unsigned int rsv_13						  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP8_Z3_partial : 5;
		unsigned int rsv_21						  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP8_Z4_partial : 5;
		unsigned int rsv_29						  : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP8_partial_1_REG (VMM_CVFS_BASE + 0x150)
union AVS_MARGIN_TEMP_OPP8_partial_1_VAL_t   AVS_MARGIN_TEMP_OPP8_partial_1_VAL;

union AVS_MARGIN_TEMP_OPP8_partial_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP8_Z5_partial : 5;
		unsigned int rsv_5						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP8_Z6_partial : 5;
		unsigned int rsv_13						  : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP8_partial_2_REG (VMM_CVFS_BASE + 0x154)
union AVS_MARGIN_TEMP_OPP8_partial_2_VAL_t   AVS_MARGIN_TEMP_OPP8_partial_2_VAL;

union AVS_MARGIN_TEMP_OPP9_partial_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP9_Z1_partial : 5;
		unsigned int rsv_5						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP9_Z2_partial : 5;
		unsigned int rsv_13						  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP9_Z3_partial : 5;
		unsigned int rsv_21						  : 3;
		unsigned int AVS_MARGIN_TEMP_OPP9_Z4_partial : 5;
		unsigned int rsv_29						  : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP9_partial_1_REG (VMM_CVFS_BASE + 0x158)
union AVS_MARGIN_TEMP_OPP9_partial_1_VAL_t   AVS_MARGIN_TEMP_OPP9_partial_1_VAL;

union AVS_MARGIN_TEMP_OPP9_partial_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP9_Z5_partial : 5;
		unsigned int rsv_5						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP9_Z6_partial : 5;
		unsigned int rsv_13						  : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP9_partial_2_REG (VMM_CVFS_BASE + 0x15C)
union AVS_MARGIN_TEMP_OPP9_partial_2_VAL_t   AVS_MARGIN_TEMP_OPP9_partial_2_VAL;

union AVS_MARGIN_TEMP_OPP10_partial_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP10_Z1_partial : 5;
		unsigned int rsv_5							: 3;
		unsigned int AVS_MARGIN_TEMP_OPP10_Z2_partial : 5;
		unsigned int rsv_13						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP10_Z3_partial : 5;
		unsigned int rsv_21						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP10_Z4_partial : 5;
		unsigned int rsv_29						   : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP10_partial_1_REG (VMM_CVFS_BASE + 0x160)
union AVS_MARGIN_TEMP_OPP10_partial_1_VAL_t   AVS_MARGIN_TEMP_OPP10_partial_1_VAL;

union AVS_MARGIN_TEMP_OPP10_partial_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP10_Z5_partial : 5;
		unsigned int rsv_5							: 3;
		unsigned int AVS_MARGIN_TEMP_OPP10_Z6_partial : 5;
		unsigned int rsv_13						   : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP10_partial_2_REG (VMM_CVFS_BASE + 0x164)
union AVS_MARGIN_TEMP_OPP10_partial_2_VAL_t   AVS_MARGIN_TEMP_OPP10_partial_2_VAL;

union AVS_MARGIN_TEMP_OPP11_partial_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP11_Z1_partial : 5;
		unsigned int rsv_5							: 3;
		unsigned int AVS_MARGIN_TEMP_OPP11_Z2_partial : 5;
		unsigned int rsv_13						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP11_Z3_partial : 5;
		unsigned int rsv_21						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP11_Z4_partial : 5;
		unsigned int rsv_29						   : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP11_partial_1_REG (VMM_CVFS_BASE + 0x168)
union AVS_MARGIN_TEMP_OPP11_partial_1_VAL_t   AVS_MARGIN_TEMP_OPP11_partial_1_VAL;

union AVS_MARGIN_TEMP_OPP11_partial_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP11_Z5_partial : 5;
		unsigned int rsv_5							: 3;
		unsigned int AVS_MARGIN_TEMP_OPP11_Z6_partial : 5;
		unsigned int rsv_13						   : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP11_partial_2_REG (VMM_CVFS_BASE + 0x16C)
union AVS_MARGIN_TEMP_OPP11_partial_2_VAL_t   AVS_MARGIN_TEMP_OPP11_partial_2_VAL;

union AVS_MARGIN_TEMP_OPP12_partial_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP12_Z1_partial : 5;
		unsigned int rsv_5							: 3;
		unsigned int AVS_MARGIN_TEMP_OPP12_Z2_partial : 5;
		unsigned int rsv_13						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP12_Z3_partial : 5;
		unsigned int rsv_21						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP12_Z4_partial : 5;
		unsigned int rsv_29						   : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP12_partial_1_REG (VMM_CVFS_BASE + 0x170)
union AVS_MARGIN_TEMP_OPP12_partial_1_VAL_t   AVS_MARGIN_TEMP_OPP12_partial_1_VAL;

union AVS_MARGIN_TEMP_OPP12_partial_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP12_Z5_partial : 5;
		unsigned int rsv_5							: 3;
		unsigned int AVS_MARGIN_TEMP_OPP12_Z6_partial : 5;
		unsigned int rsv_13						   : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP12_partial_2_REG (VMM_CVFS_BASE + 0x174)
union AVS_MARGIN_TEMP_OPP12_partial_2_VAL_t   AVS_MARGIN_TEMP_OPP12_partial_2_VAL;

union AVS_MARGIN_TEMP_OPP13_partial_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP13_Z1_partial : 5;
		unsigned int rsv_5							: 3;
		unsigned int AVS_MARGIN_TEMP_OPP13_Z2_partial : 5;
		unsigned int rsv_13						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP13_Z3_partial : 5;
		unsigned int rsv_21						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP13_Z4_partial : 5;
		unsigned int rsv_29						   : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP13_partial_1_REG (VMM_CVFS_BASE + 0x178)
union AVS_MARGIN_TEMP_OPP13_partial_1_VAL_t   AVS_MARGIN_TEMP_OPP13_partial_1_VAL;

union AVS_MARGIN_TEMP_OPP13_partial_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP13_Z5_partial : 5;
		unsigned int rsv_5							: 3;
		unsigned int AVS_MARGIN_TEMP_OPP13_Z6_partial : 5;
		unsigned int rsv_13						   : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP13_partial_2_REG (VMM_CVFS_BASE + 0x17C)
union AVS_MARGIN_TEMP_OPP13_partial_2_VAL_t   AVS_MARGIN_TEMP_OPP13_partial_2_VAL;

union AVS_MARGIN_TEMP_OPP14_partial_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP14_Z1_partial : 5;
		unsigned int rsv_5							: 3;
		unsigned int AVS_MARGIN_TEMP_OPP14_Z2_partial : 5;
		unsigned int rsv_13						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP14_Z3_partial : 5;
		unsigned int rsv_21						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP14_Z4_partial : 5;
		unsigned int rsv_29						   : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP14_partial_1_REG (VMM_CVFS_BASE + 0x180)
union AVS_MARGIN_TEMP_OPP14_partial_1_VAL_t   AVS_MARGIN_TEMP_OPP14_partial_1_VAL;

union AVS_MARGIN_TEMP_OPP14_partial_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP14_Z5_partial : 5;
		unsigned int rsv_5							: 3;
		unsigned int AVS_MARGIN_TEMP_OPP14_Z6_partial : 5;
		unsigned int rsv_13						   : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP14_partial_2_REG (VMM_CVFS_BASE + 0x184)
union AVS_MARGIN_TEMP_OPP14_partial_2_VAL_t   AVS_MARGIN_TEMP_OPP14_partial_2_VAL;

union AVS_MARGIN_TEMP_OPP15_partial_1_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP15_Z1_partial : 5;
		unsigned int rsv_5							: 3;
		unsigned int AVS_MARGIN_TEMP_OPP15_Z2_partial : 5;
		unsigned int rsv_13						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP15_Z3_partial : 5;
		unsigned int rsv_21						   : 3;
		unsigned int AVS_MARGIN_TEMP_OPP15_Z4_partial : 5;
		unsigned int rsv_29						   : 3;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP15_partial_1_REG (VMM_CVFS_BASE + 0x188)
union AVS_MARGIN_TEMP_OPP15_partial_1_VAL_t   AVS_MARGIN_TEMP_OPP15_partial_1_VAL;

union AVS_MARGIN_TEMP_OPP15_partial_2_VAL_t {
	struct {
		unsigned int AVS_MARGIN_TEMP_OPP15_Z5_partial : 5;
		unsigned int rsv_5							: 3;
		unsigned int AVS_MARGIN_TEMP_OPP15_Z6_partial : 5;
		unsigned int rsv_13						   : 19;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_TEMP_OPP15_partial_2_REG (VMM_CVFS_BASE + 0x18C)
union AVS_MARGIN_TEMP_OPP15_partial_2_VAL_t   AVS_MARGIN_TEMP_OPP15_partial_2_VAL;

union AVS_MARGIN_AGING_1_partial_VAL_t {
	struct {
		unsigned int AVS_MARGIN_AGING_OPP0_partial : 3;
		unsigned int rsv_3						 : 1;
		unsigned int AVS_MARGIN_AGING_OPP1_partial : 3;
		unsigned int rsv_7						 : 1;
		unsigned int AVS_MARGIN_AGING_OPP2_partial : 3;
		unsigned int rsv_11						: 1;
		unsigned int AVS_MARGIN_AGING_OPP3_partial : 3;
		unsigned int rsv_15						: 1;
		unsigned int AVS_MARGIN_AGING_OPP4_partial : 3;
		unsigned int rsv_19						: 1;
		unsigned int AVS_MARGIN_AGING_OPP5_partial : 3;
		unsigned int rsv_23						: 1;
		unsigned int AVS_MARGIN_AGING_OPP6_partial : 3;
		unsigned int rsv_27						: 1;
		unsigned int AVS_MARGIN_AGING_OPP7_partial : 3;
		unsigned int rsv_31						: 1;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_AGING_1_partial_REG (VMM_CVFS_BASE + 0x190)
union AVS_MARGIN_AGING_1_partial_VAL_t   AVS_MARGIN_AGING_1_partial_VAL;

union AVS_MARGIN_AGING_2_partial_VAL_t {
	struct {
		unsigned int AVS_MARGIN_AGING_OPP8_partial : 3;
		unsigned int rsv_3						 : 1;
		unsigned int AVS_MARGIN_AGING_OPP9_partial : 3;
		unsigned int rsv_7						 : 1;
		unsigned int AVS_MARGIN_AGING_OPP10_partial : 3;
		unsigned int rsv_11						: 1;
		unsigned int AVS_MARGIN_AGING_OPP11_partial : 3;
		unsigned int rsv_15						: 1;
		unsigned int AVS_MARGIN_AGING_OPP12_partial : 3;
		unsigned int rsv_19						: 1;
		unsigned int AVS_MARGIN_AGING_OPP13_partial : 3;
		unsigned int rsv_23						: 1;
		unsigned int AVS_MARGIN_AGING_OPP14_partial : 3;
		unsigned int rsv_27						: 1;
		unsigned int AVS_MARGIN_AGING_OPP15_partial : 3;
		unsigned int rsv_31						: 1;
	} PACKING Bits;
	uint32_t Raw;
};
#define AVS_MARGIN_AGING_2_partial_REG (VMM_CVFS_BASE + 0x194)
union AVS_MARGIN_AGING_2_partial_VAL_t   AVS_MARGIN_AGING_2_partial_VAL;

#endif
