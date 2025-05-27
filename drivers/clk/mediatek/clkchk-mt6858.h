/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: KY Liu <ky.liu@mediatek.com>
 */

#ifndef __DRV_CLKCHK_MT6858_H
#define __DRV_CLKCHK_MT6858_H

// Bypass CLK/PWR check before suspend is ready
#define BYPASS_SUSPEND_CLK_PWR_CHK	1

enum chk_sys_id {
	top = 0,
	infra_infracfg_ao_reg = 1,
	apmixed = 2,
	nemicfg_ao_mem_reg_bus = 3,
	ssr_top = 4,
	perao = 5,
	afe = 6,
	impc = 7,
	ufsao = 8,
	ufspdn = 9,
	impes = 10,
	imps = 11,
	mm = 12,
	imgsys1 = 13,
	img_sub0_bus = 14,
	imgsys2 = 15,
	vde2 = 16,
	ven1 = 17,
	cam_sub1_bus = 18,
	cam_sub0_bus = 19,
	ipe_sub0_bus = 20,
	spm = 21,
	vlpcfg_reg_bus = 22,
	vlp_top = 23,
	cam_m = 24,
	cam_ra = 25,
	cam_rb = 26,
	ipe = 27,
	mminfra_config = 28,
	mdp = 29,
	hwv = 30,
	chk_sys_num = 31,
};

enum chk_pd_id {
	MT6858_CHK_PD_CONN = 0,
	MT6858_CHK_PD_AUDIO = 1,
	MT6858_CHK_PD_MM_INFRA = 2,
	MT6858_CHK_PD_ISP_IMG1 = 3,
	MT6858_CHK_PD_ISP_IMG2 = 4,
	MT6858_CHK_PD_ISP_IPE = 5,
	MT6858_CHK_PD_VDE0 = 6,
	MT6858_CHK_PD_VEN0 = 7,
	MT6858_CHK_PD_CAM_MAIN = 8,
	MT6858_CHK_PD_CAM_SUBA = 9,
	MT6858_CHK_PD_CAM_SUBB = 10,
	MT6858_CHK_PD_DIS0 = 11,
	MT6858_CHK_PD_MM_PROC = 12,
	MT6858_CHK_PD_CSI_RX = 13,
	MT6858_CHK_PD_SSUSB = 14,
	MT6858_CHK_PD_APU = 15,
	MT6858_CHK_PD_NUM,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif

extern void print_subsys_reg_mt6858(enum chk_sys_id id);
extern void set_subsys_reg_dump_mt6858(enum chk_sys_id id[]);
extern void get_subsys_reg_dump_mt6858(void);
extern u32 get_mt6858_reg_value(u32 id, u32 ofs);
extern void release_mt6858_hwv_secure(void);
extern void dump_clk_event(void);
#endif	/* __DRV_CLKCHK_MT6858_H */
