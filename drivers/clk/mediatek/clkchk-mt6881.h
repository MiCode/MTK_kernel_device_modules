/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Claude Yen <claude.yen@mediatek.com>
 */

#ifndef __DRV_CLKCHK_MT6881_H
#define __DRV_CLKCHK_MT6881_H

// Bypass CLK/PWR check before suspend is ready
#define BYPASS_SUSPEND_CLK_PWR_CHK	0

enum chk_sys_id {
	top = 0,
	cksys_reg = 1,
	infra_infracfg_ao_reg = 2,
	infra_infracfg_ao_reg_bus = 3,
	apmixed = 4,
	pericfg_ao_reg = 5,
	afe = 6,
	ufscfg_ao_bus = 7,
	ufsao = 8,
	ufspdn = 9,
	imp_iic_top_wrap_s = 10,
	imp_iic_top_wrap_w = 11,
	mipi_csi_top_ctrl_0 = 12,
	mm = 13,
	img = 14,
	dip_top_dip1 = 15,
	dip_nr1_dip1 = 16,
	dip_nr2_dip1 = 17,
	wpe_eis_dip1 = 18,
	wpe_tnr_dip1 = 19,
	traw_dip1 = 20,
	traw_cap_dip1 = 21,
	img_v = 22,
	vde2 = 23,
	ven1 = 24,
	spm = 25,
	vlpcfg_reg_bus = 26,
	vlp_cksys_top = 27,
	ssr_top_bus = 28,
	cam_m = 29,
	cam_mr = 30,
	cam_ra = 31,
	camsys_rmsa = 32,
	cam_ya = 33,
	cam_rb = 34,
	camsys_rmsb = 35,
	cam_yb = 36,
	cam_v = 37,
	mminfra_config = 38,
	mdp = 39,
	hwv = 40,
	hwv_ext = 41,
	hwv_wrt = 42,
	chk_sys_num = 43,
};

enum chk_pd_id {
	MT6881_CHK_PD_CONN = 0,
	MT6881_CHK_PD_UFS0 = 1,
	MT6881_CHK_PD_UFS0_PHY = 2,
	MT6881_CHK_PD_AUDIO = 3,
	MT6881_CHK_PD_MM_INFRA = 4,
	MT6881_CHK_PD_ISP_VCORE = 5,
	MT6881_CHK_PD_ISP_MAIN = 6,
	MT6881_CHK_PD_ISP_DIP1 = 7,
	MT6881_CHK_PD_VDE0 = 8,
	MT6881_CHK_PD_VEN0 = 9,
	MT6881_CHK_PD_CAM_VCORE = 10,
	MT6881_CHK_PD_CAM_MAIN = 11,
	MT6881_CHK_PD_CAM_SUBA = 12,
	MT6881_CHK_PD_CAM_SUBB = 13,
	MT6881_CHK_PD_DIS0 = 14,
	MT6881_CHK_PD_MM_PROC = 15,
	MT6881_CHK_PD_CSI_RX = 16,
	MT6881_CHK_PD_SSRSYS = 17,
	MT6881_CHK_PD_SSUSB = 18,
	MT6881_CHK_PD_APU = 19,
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
