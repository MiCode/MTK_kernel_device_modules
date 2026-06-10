/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Claude Yen <claude.yen@mediatek.com>
 */

#ifndef __DRV_CLKCHK_MT6881_H
#define __DRV_CLKCHK_MT6881_H

// Bypass CLK/PWR check before suspend is ready
#define BYPASS_SUSPEND_CLK_PWR_CHK	1

enum chk_sys_id {
	cksys_reg = 0,
	infra_infracfg_ao_reg = 1,
	apmixed = 2,
	pericfg_ao_reg = 3,
	afe = 4,
	ufsao = 5,
	ufspdn = 6,
	imp_iic_top_wrap_s = 7,
	imp_iic_top_wrap_w = 8,
	mipi_csi_top_ctrl_0 = 9,
	mm = 10,
	img = 11,
	dip_top_dip1 = 12,
	dip_nr1_dip1 = 13,
	dip_nr2_dip1 = 14,
	wpe_eis_dip1 = 15,
	wpe_tnr_dip1 = 16,
	traw_dip1 = 17,
	traw_cap_dip1 = 18,
	img_v = 19,
	vde2 = 20,
	ven1 = 21,
	spm = 22,
	vlpcfg_reg_bus = 23,
	vlp_cksys_top = 24,
	cam_m = 25,
	cam_mr = 26,
	cam_ra = 27,
	camsys_rmsa = 28,
	cam_ya = 29,
	cam_rb = 30,
	camsys_rmsb = 31,
	cam_yb = 32,
	cam_v = 33,
	ssr_top = 34,
	mminfra_config = 35,
	mdp = 36,
	hwv = 37,
	chk_sys_num = 38,
};

enum chk_pd_id {
	MT6881_CHK_PD_CONN = 0,
	MT6881_CHK_PD_AUDIO = 1,
	MT6881_CHK_PD_MM_INFRA = 2,
	MT6881_CHK_PD_ISP_VCORE = 3,
	MT6881_CHK_PD_ISP_MAIN = 4,
	MT6881_CHK_PD_ISP_DIP1 = 5,
	MT6881_CHK_PD_VDE0 = 6,
	MT6881_CHK_PD_VEN0 = 7,
	MT6881_CHK_PD_CAM_VCORE = 8,
	MT6881_CHK_PD_CAM_MAIN = 9,
	MT6881_CHK_PD_CAM_SUBA = 10,
	MT6881_CHK_PD_CAM_SUBB = 11,
	MT6881_CHK_PD_DIS0 = 12,
	MT6881_CHK_PD_MM_PROC = 13,
	MT6881_CHK_PD_CSI_RX = 14,
	MT6881_CHK_PD_SSUSB = 15,
	MT6881_CHK_PD_APU = 16,
	MT6881_CHK_PD_NUM,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif

extern void print_subsys_reg_mt6881(enum chk_sys_id id);
extern void set_subsys_reg_dump_mt6881(enum chk_sys_id id[]);
extern void get_subsys_reg_dump_mt6881(void);
extern u32 get_mt6881_reg_value(u32 id, u32 ofs);
extern void release_mt6881_hwv_secure(void);
extern void dump_clk_event(void);
#endif	/* __DRV_CLKCHK_MT6881_H */
