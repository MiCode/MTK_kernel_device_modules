/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __MCUPM_DRIVER_H__
#define __MCUPM_DRIVER_H__

enum {
	MCUPM_MEM_ID = 0,
#if !IS_ENABLED(CONFIG_MTK_GMO_RAM_OPTIMIZE) && !IS_ENABLED(CONFIG_MTK_MET_MEM_ALLOC)
	MCUPM_MET_ID,
#endif
	MCUPM_EEMSN_MEM_ID,
	MCUPM_BRISKET_ID,
	NUMS_MCUPM_MEM_ID,
};

extern phys_addr_t mcupm_reserve_mem_get_phys(unsigned int id);
extern phys_addr_t mcupm_reserve_mem_get_virt(unsigned int id);
extern phys_addr_t mcupm_reserve_mem_get_size(unsigned int id);
#endif
