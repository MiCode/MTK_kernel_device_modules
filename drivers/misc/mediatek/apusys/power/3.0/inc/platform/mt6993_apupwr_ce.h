/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MT6993_APUPWR_CE_H__
#define __MT6993_APUPWR_CE_H__

#include <linux/io.h>
#include <linux/clk.h>

#define APU_CE_SRAMBASE                   (0x19040000)
#define APU_ARE_REG_BASE                     (0x19050000)
#define APU_ARE_SRAM_SIZE                    (0x10000)
#define APU_CE_VIRTUAL_REG_OFS (0x5B4)
#define APU_CE_HW_CONFIG_OFS (0x50)
#define APU_CE_HW_JOB_BITMAP_0_OFS            (0x04d0)
#define APU_CE_HW_JOB_BITMAP_4_OFS            (0x04d4)
#define APU_CE_HW_JOB_BITMAP_8_OFS            (0x04d8)
#define APU_CE_HW_JOB_BITMAP_12_OFS            (0x04dc)

int mt6993_load_ce_bin(void);
#endif // __MT6993_APUPWR_CE_H__
