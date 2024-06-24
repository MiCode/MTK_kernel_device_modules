/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: KY Liu <ky.liu@mediatek.com>
 */

#ifndef __DRV_CLKCHK_MT6899_H
#define __DRV_CLKCHK_MT6899_H

enum chk_sys_id {
	top = 0,
	infracfg_ao = 1,
	apmixed = 2,
	infra_ifrbus_ao_reg_bus = 3,
	emi_nemicfg_ao_mem_prot_reg_bus = 4,
	emi_semicfg_ao_mem_prot_reg_bus = 5,
	perao = 6,
	afe = 7,
	impc = 8,
	ufscfg_ao_bus = 9,
	ufsao = 10,
	ufspdn = 11,
	impen = 12,
	impe = 13,
	imps = 14,
	impes = 15,
	impw = 16,
	impn = 17,
	mfg_ao = 18,
	mfgsc_ao = 19,
	mm = 20,
	mm1 = 21,
	ovl = 22,
	img = 23,
	dip_top_dip1 = 24,
	dip_nr1_dip1 = 25,
	dip_nr2_dip1 = 26,
	wpe1_dip1 = 27,
	wpe2_dip1 = 28,
	wpe3_dip1 = 29,
	traw_dip1 = 30,
	traw_cap_dip1 = 31,
	img_v = 32,
	vde1 = 33,
	vde2 = 34,
	ven1 = 35,
	ven2 = 36,
	spm = 37,
	vlpcfg = 38,
	vlp_ck = 39,
	cam_m = 40,
	cam_mr = 41,
	camsys_ipe = 42,
	cam_ra = 43,
	camsys_rmsa = 44,
	cam_ya = 45,
	cam_rb = 46,
	camsys_rmsb = 47,
	cam_yb = 48,
	cam_rc = 49,
	camsys_rmsc = 50,
	cam_yc = 51,
	ccu = 52,
	camv = 53,
	mminfra_ao_config = 54,
	mdp = 55,
	mdp1 = 56,
	cci = 57,
	cpu_ll = 58,
	cpu_bl = 59,
	cpu_b = 60,
	ptp = 61,
	hwv = 62,
	mm_hwv = 63,
	hfrp = 64,
	hfrp_bus = 65,
	chk_sys_num = 66,
};

enum chk_pd_id {
	MT6899_CHK_PD_MD1 = 0,
	MT6899_CHK_PD_CONN = 1,
	MT6899_CHK_PD_PERI_USB0 = 2,
	MT6899_CHK_PD_PERI_AUDIO = 3,
	MT6899_CHK_PD_ADSP_AO = 4,
	MT6899_CHK_PD_ADSP_INFRA = 5,
	MT6899_CHK_PD_ADSP_TOP = 6,
	MT6899_CHK_PD_MM_INFRA = 7,
	MT6899_CHK_PD_ISP_VCORE = 8,
	MT6899_CHK_PD_ISP_MAIN = 9,
	MT6899_CHK_PD_ISP_TRAW = 10,
	MT6899_CHK_PD_ISP_DIP1 = 11,
	MT6899_CHK_PD_VDE0 = 12,
	MT6899_CHK_PD_VDE1 = 13,
	MT6899_CHK_PD_VEN0 = 14,
	MT6899_CHK_PD_VEN1 = 15,
	MT6899_CHK_PD_CAM_VCORE = 16,
	MT6899_CHK_PD_CAM_MAIN = 17,
	MT6899_CHK_PD_CAM_MRAW = 18,
	MT6899_CHK_PD_CAM_SUBA = 19,
	MT6899_CHK_PD_CAM_SUBB = 20,
	MT6899_CHK_PD_CAM_SUBC = 21,
	MT6899_CHK_PD_CAM_CCU = 22,
	MT6899_CHK_PD_CAM_CCU_AO = 23,
	MT6899_CHK_PD_DISP_VCORE = 24,
	MT6899_CHK_PD_DIS1 = 25,
	MT6899_CHK_PD_MML0 = 26,
	MT6899_CHK_PD_MML1 = 27,
	MT6899_CHK_PD_DIS0 = 28,
	MT6899_CHK_PD_OVL0 = 29,
	MT6899_CHK_PD_DP_TX = 30,
	MT6899_CHK_PD_CSI_RX = 31,
	MT6899_CHK_PD_NUM,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif

extern void print_subsys_reg_mt6899(enum chk_sys_id id);
extern void set_subsys_reg_dump_mt6899(enum chk_sys_id id[]);
extern void get_subsys_reg_dump_mt6899(void);
extern u32 get_mt6899_reg_value(u32 id, u32 ofs);
#endif	/* __DRV_CLKCHK_MT6899_H */
