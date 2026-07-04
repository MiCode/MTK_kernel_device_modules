/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __DRV_CLKCHK_MT6761_H
#define __DRV_CLKCHK_MT6761_H

enum chk_sys_id {
	topckgen,
	infracfg,
	pericfg,
	scpsys,
	apmixed,
	gce,
	audio,
	mipi_0a,
	mipi_0b,
	mipi_1a,
	mipi_1b,
	mipi_2a,
	mipi_2b,
	mfgsys,
	mmsys,
	camsys,
	vencsys,
	chk_sys_num,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif

extern void print_subsys_reg_mt6761(enum chk_sys_id id);
extern u32 get_mt6761_reg_value(u32 id, u32 ofs);
#endif	/* __DRV_CLKCHK_MT6761_H */

