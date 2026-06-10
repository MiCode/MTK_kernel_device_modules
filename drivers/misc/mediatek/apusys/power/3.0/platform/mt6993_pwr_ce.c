// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/slab.h>
#include "apu_top.h"
#include "aputop_log.h"
#include "aputop_rpmsg.h"
#include "mt6993_apupwr.h"
#include "mt6993_apupwr_prot.h"
#define LOCAL_DBG	(1)
#include "mt6993_apuce_decl.h"
#include "mt6993_apuce_tbl.h"
#include "mt6993_apupwr_ce.h"

#if 1
/* apuce mmap address */
static void *apu_are_sram;
static void *apu_ce_reg;

#endif

int mt6993_load_ce_bin(void)
{
	unsigned int i = 0;
	unsigned int reg_val = 0;
	unsigned int magic_num = 0;

	apu_are_sram = ioremap(APU_CE_SRAMBASE, APU_ARE_SRAM_SIZE);
	apu_ce_reg = ioremap(APU_ARE_REG_BASE, APU_ARE_SRAM_SIZE);

	/* Initial HW job CE_SEL */
	iowrite32(0x0F0F0F0F, apu_ce_reg + APU_CE_HW_JOB_BITMAP_0_OFS);
	iowrite32(0x0F0F0F0F, apu_ce_reg + APU_CE_HW_JOB_BITMAP_4_OFS);
	iowrite32(0x0F0F0F0F, apu_ce_reg + APU_CE_HW_JOB_BITMAP_8_OFS);
	iowrite32(0x0F0F0F0F, apu_ce_reg + APU_CE_HW_JOB_BITMAP_12_OFS);

	for (i = 0; i < apusys_ce_img_len / 4; i += 1) {
		reg_val |= (apusys_ce_img_tbl[4 * i + 3] << 24);
		reg_val |= (apusys_ce_img_tbl[4 * i + 2] << 16);
		reg_val |= (apusys_ce_img_tbl[4 * i + 1] << 8);
		reg_val |= (apusys_ce_img_tbl[4 * i + 0] << 0);

		if (i == 0) {
			magic_num = reg_val;

			if (magic_num != APU_CE_MAGIC)
				return -EINVAL;
		} else {

			iowrite32(reg_val, apu_are_sram + APU_CE_OFFSET + 4 * (i-1));
		}
		reg_val = 0;
	}

	/* ACE HW Job Config */
	for (i = 0; i < APU_CE_CONFIG_COUNT; i++) {
		iowrite32(apu_ce_configs[i].value, apu_are_sram + APU_CE_HW_CONFIG_OFS +
			apu_ce_configs[i].config_id * 4);
	}

	// Jayer SRAM Virtual Region
	// ARE_ACE BASE ADDR: 0x19050000, ofs: 0x5b4  bit 13:0 - pc_start
	// bit 19:26: size; bit 31: mmu enable
	reg_val = APU_CE_OFFSET + APU_CE_IMAGE_SIZE;
	reg_val |= ((APU_CE_STACK_SIZE) >> 2) << 16;
	iowrite32(reg_val, apu_ce_reg + APU_CE_VIRTUAL_REG_OFS);

	return 0;
}
