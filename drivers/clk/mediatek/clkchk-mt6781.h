/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __DRV_CLKCHK_MT6781_H
#define __DRV_CLKCHK_MT6781_H

enum chk_sys_id {
	topckgen,
	infracfg,
	scpsys,
	apmixedsys,
	audiosys,
	mfgsys,
	mmsys,
	imgsys,
	camsys,
	vencsys,
	vdecsys,
	ipu_vcore,
	ipu_conn,
	ipu0,
	ipu1,
	ipu2,
	chk_sys_num,
};

extern void print_subsys_reg_mt6781(enum chk_sys_id id);
extern u32 get_mt6781_reg_value(u32 id, u32 ofs);
#endif	/* __DRV_CLKCHK_MT6833_H */

